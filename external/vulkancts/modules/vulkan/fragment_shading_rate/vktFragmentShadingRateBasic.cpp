/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2020 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests for VK_KHR_fragment_shading_rate
 * The test renders 9*9 triangles, where each triangle has one of the valid
 * fragment sizes ({1,2,4},{1,2,4}) (clamped to implementation limits) for
 * each of the pipeline shading rate and the primitive shading rate. The
 * fragment shader does an atomic add to a memory location to get a unique
 * identifier for the fragment, and outputs the primitive ID, atomic counter,
 * fragment size, and some other info the the color output. Then a compute
 * shader copies this to buffer memory, and the host verifies several
 * properties of the output. For example, if a sample has a particular
 * primitive ID and atomic value, then all other samples in the tile with
 * the same primitive ID should have the same atomic value.
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShadingRateBasic.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace FragmentShadingRate
{
namespace
{
using namespace vk;
using namespace std;

#define NUM_TRIANGLES (9*9)

enum class AttachmentUsage
{
	NO_ATTACHMENT = 0,
	NO_ATTACHMENT_PTR,
	WITH_ATTACHMENT,
};

struct CaseDef
{
	deInt32 seed;
	VkExtent2D framebufferDim;
	VkSampleCountFlagBits samples;
	VkFragmentShadingRateCombinerOpKHR combinerOp[2];
	AttachmentUsage attachmentUsage;
	bool shaderWritesRate;
	bool geometryShader;
	bool useDynamicState;
	bool useDynamicRendering;
	bool useApiSampleMask;
	bool useSampleMaskIn;
	bool conservativeEnable;
	VkConservativeRasterizationModeEXT conservativeMode;
	bool useDepthStencil; // == fragDepth || fragStencil
	bool fragDepth;
	bool fragStencil;
	bool multiViewport;
	bool colorLayered;
	bool srLayered; // colorLayered must also be true
	deUint32 numColorLayers;
	bool multiView;
	bool interlock;
	bool sampleLocations;
	bool sampleShadingEnable;
	bool sampleShadingInput;
	bool sampleMaskTest;

	bool useAttachment () const
	{
		return (attachmentUsage == AttachmentUsage::WITH_ATTACHMENT);
	}
};

class FSRTestInstance : public TestInstance
{
public:
						FSRTestInstance		(Context& context, const CaseDef& data);
						~FSRTestInstance	(void);
	tcu::TestStatus		iterate				(void);

private:
	// Test parameters
	CaseDef				m_data;

	// Cache simulated combiner operations, to avoid recomputing per-sample
	deInt32				m_simulateValueCount;
	vector<deInt32>		m_simulateCache;
	// Cache mapping of primitive ID to pipeline/primitive shading rate
	vector<deInt32>		m_primIDToPrimitiveShadingRate;
	vector<deInt32>		m_primIDToPipelineShadingRate;
	deUint32			m_supportedFragmentShadingRateCount;
	vector<VkPhysicalDeviceFragmentShadingRateKHR>	m_supportedFragmentShadingRates;
	VkPhysicalDeviceFragmentShadingRatePropertiesKHR	m_shadingRateProperties;

	deInt32				PrimIDToPrimitiveShadingRate	(deInt32 primID);
	deInt32				PrimIDToPipelineShadingRate		(deInt32 primID);
	VkExtent2D			SanitizeExtent		(VkExtent2D ext) const;
	deInt32				SanitizeRate		(deInt32 rate) const;
	deInt32				ShadingRateExtentToClampedMask	(VkExtent2D ext, bool allowSwap) const;
	deInt32				ShadingRateExtentToEnum	(VkExtent2D ext) const;
	VkExtent2D			ShadingRateEnumToExtent	(deInt32 rate) const;
	deInt32				Simulate			(deInt32 rate0, deInt32 rate1, deInt32 rate2);
	VkExtent2D			Combine				(VkExtent2D ext0, VkExtent2D ext1, VkFragmentShadingRateCombinerOpKHR comb) const;
	bool				Force1x1			() const;
};

FSRTestInstance::FSRTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
	, m_simulateValueCount	(((4 * 4) | 4) + 1)
	, m_simulateCache		(m_simulateValueCount*m_simulateValueCount*m_simulateValueCount, ~0)
	, m_primIDToPrimitiveShadingRate(NUM_TRIANGLES, ~0)
	, m_primIDToPipelineShadingRate(NUM_TRIANGLES, ~0)
{
	m_supportedFragmentShadingRateCount = 0;
	m_context.getInstanceInterface().getPhysicalDeviceFragmentShadingRatesKHR(m_context.getPhysicalDevice(), &m_supportedFragmentShadingRateCount, DE_NULL);

	if (m_supportedFragmentShadingRateCount < 3)
		TCU_THROW(TestError, "*pFragmentShadingRateCount too small");

	m_supportedFragmentShadingRates.resize(m_supportedFragmentShadingRateCount);
	for (deUint32 i = 0; i < m_supportedFragmentShadingRateCount; ++i)
	{
		m_supportedFragmentShadingRates[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
		m_supportedFragmentShadingRates[i].pNext = nullptr;
	}
	m_context.getInstanceInterface().getPhysicalDeviceFragmentShadingRatesKHR(m_context.getPhysicalDevice(), &m_supportedFragmentShadingRateCount, &m_supportedFragmentShadingRates[0]);

	m_shadingRateProperties = m_context.getFragmentShadingRateProperties();
}

FSRTestInstance::~FSRTestInstance (void)
{
}

class FSRTestCase : public TestCase
{
	public:
								FSRTestCase		(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
								~FSRTestCase	(void);
	virtual	void				initPrograms	(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance	(Context& context) const;
	virtual void				checkSupport	(Context& context) const;

private:
	CaseDef						m_data;
};

FSRTestCase::FSRTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

FSRTestCase::~FSRTestCase	(void)
{
}

bool FSRTestInstance::Force1x1() const
{
	if (m_data.useApiSampleMask && !m_context.getFragmentShadingRateProperties().fragmentShadingRateWithSampleMask)
		return true;

	if (m_data.useSampleMaskIn && !m_context.getFragmentShadingRateProperties().fragmentShadingRateWithShaderSampleMask)
		return true;

	if (m_data.conservativeEnable && !m_context.getFragmentShadingRateProperties().fragmentShadingRateWithConservativeRasterization)
		return true;

	if (m_data.useDepthStencil && !m_context.getFragmentShadingRateProperties().fragmentShadingRateWithShaderDepthStencilWrites)
		return true;

	if (m_data.interlock && !m_context.getFragmentShadingRateProperties().fragmentShadingRateWithFragmentShaderInterlock)
		return true;

	if (m_data.sampleLocations && !m_context.getFragmentShadingRateProperties().fragmentShadingRateWithCustomSampleLocations)
		return true;

	if (m_data.sampleShadingEnable || m_data.sampleShadingInput)
		return true;

	return false;
}

static VkImageUsageFlags cbUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
								   VK_IMAGE_USAGE_SAMPLED_BIT |
								   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
								   VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

void FSRTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

	if (m_data.useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (!context.getFragmentShadingRateFeatures().pipelineFragmentShadingRate)
		TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate not supported");

	if (m_data.shaderWritesRate &&
		!context.getFragmentShadingRateFeatures().primitiveFragmentShadingRate)
		TCU_THROW(NotSupportedError, "primitiveFragmentShadingRate not supported");

	if (!context.getFragmentShadingRateFeatures().primitiveFragmentShadingRate &&
		m_data.combinerOp[0] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR)
		TCU_THROW(NotSupportedError, "primitiveFragmentShadingRate not supported");

	if (!context.getFragmentShadingRateFeatures().attachmentFragmentShadingRate &&
		m_data.combinerOp[1] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR)
		TCU_THROW(NotSupportedError, "attachmentFragmentShadingRate not supported");

	VkImageFormatProperties imageProperties;
	VkResult result = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), VK_FORMAT_R32G32B32A32_UINT, VK_IMAGE_TYPE_2D,
																							VK_IMAGE_TILING_OPTIMAL, cbUsage, 0, &imageProperties);

	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "VK_FORMAT_R32G32B32A32_UINT not supported");

	if (!(imageProperties.sampleCounts & m_data.samples))
		TCU_THROW(NotSupportedError, "color buffer sample count not supported");

	if (m_data.numColorLayers > imageProperties.maxArrayLayers)
		TCU_THROW(NotSupportedError, "color buffer layers not supported");

	if (m_data.useAttachment() && !context.getFragmentShadingRateFeatures().attachmentFragmentShadingRate)
		TCU_THROW(NotSupportedError, "attachmentFragmentShadingRate not supported");

	if (!context.getFragmentShadingRateProperties().fragmentShadingRateNonTrivialCombinerOps &&
		((m_data.combinerOp[0] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR && m_data.combinerOp[0] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR) ||
		 (m_data.combinerOp[1] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR && m_data.combinerOp[1] != VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR)))
		TCU_THROW(NotSupportedError, "fragmentShadingRateNonTrivialCombinerOps not supported");

	if (m_data.conservativeEnable)
	{
		context.requireDeviceFunctionality("VK_EXT_conservative_rasterization");
		if (m_data.conservativeMode == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT &&
			!context.getConservativeRasterizationPropertiesEXT().primitiveUnderestimation)
			TCU_THROW(NotSupportedError, "primitiveUnderestimation not supported");
	}

	if (m_data.fragStencil)
		context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

	if (m_data.multiViewport &&
		!context.getFragmentShadingRateProperties().primitiveFragmentShadingRateWithMultipleViewports)
		TCU_THROW(NotSupportedError, "primitiveFragmentShadingRateWithMultipleViewports not supported");

	if (m_data.srLayered &&
		!context.getFragmentShadingRateProperties().layeredShadingRateAttachments)
		TCU_THROW(NotSupportedError, "layeredShadingRateAttachments not supported");

	if ((m_data.multiViewport || m_data.colorLayered) &&
		!m_data.geometryShader)
		context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");

	if (m_data.multiView && m_data.geometryShader &&
		!context.getMultiviewFeatures().multiviewGeometryShader)
		TCU_THROW(NotSupportedError, "multiviewGeometryShader not supported");

	if (m_data.interlock &&
		!context.getFragmentShaderInterlockFeaturesEXT().fragmentShaderPixelInterlock)
		TCU_THROW(NotSupportedError, "fragmentShaderPixelInterlock not supported");

	if (m_data.sampleLocations)
	{
		context.requireDeviceFunctionality("VK_EXT_sample_locations");
		if (!(m_data.samples & context.getSampleLocationsPropertiesEXT().sampleLocationSampleCounts))
			TCU_THROW(NotSupportedError, "samples not supported in sampleLocationSampleCounts");
	}

	if (m_data.sampleMaskTest && !context.getFragmentShadingRateProperties().fragmentShadingRateWithSampleMask)
		TCU_THROW(NotSupportedError, "fragmentShadingRateWithSampleMask not supported");
}

// Error codes writted by the fragment shader
enum
{
	ERROR_NONE = 0,
	ERROR_FRAGCOORD_CENTER = 1,
	ERROR_VTG_READBACK = 2,
	ERROR_FRAGCOORD_DERIV = 3,
	ERROR_FRAGCOORD_IMPLICIT_DERIV = 4,
};

void FSRTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::stringstream vss;

	vss <<
		"#version 450 core\n"
		"#extension GL_EXT_fragment_shading_rate : enable\n"
		"#extension GL_ARB_shader_viewport_layer_array : enable\n"
		"layout(push_constant) uniform PC {\n"
		"	int shadingRate;\n"
		"} pc;\n"
		"layout(location = 0) in vec2 pos;\n"
		"layout(location = 0) out int instanceIndex;\n"
		"layout(location = 1) out int readbackok;\n"
		"layout(location = 2) out float zero;\n"
		"out gl_PerVertex\n"
		"{\n"
		"   vec4 gl_Position;\n"
		"};\n"
		"void main()\n"
		"{\n"
		"  gl_Position = vec4(pos, 0, 1);\n"
		"  instanceIndex = gl_InstanceIndex;\n"
		"  readbackok = 1;\n"
		"  zero = 0;\n";

	if (m_data.shaderWritesRate)
	{
		vss << "  gl_PrimitiveShadingRateEXT = pc.shadingRate;\n";

		// Verify that we can read from the output variable
		vss << "  if (gl_PrimitiveShadingRateEXT != pc.shadingRate) readbackok = 0;\n";

		if (!m_data.geometryShader)
		{
			if (m_data.multiViewport)
				vss << "  gl_ViewportIndex = instanceIndex & 1;\n";
			if (m_data.colorLayered)
				vss << "  gl_Layer = (instanceIndex & 2) >> 1;\n";
		}
	}

	vss << "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

	if (m_data.geometryShader)
	{
		std::string writeShadingRate = "";
		if (m_data.shaderWritesRate)
		{
			writeShadingRate =
				"  gl_PrimitiveShadingRateEXT = pc.shadingRate;\n"
				"  if (gl_PrimitiveShadingRateEXT != pc.shadingRate) readbackok = 0;\n";

			if (m_data.multiViewport)
				writeShadingRate += "  gl_ViewportIndex = inInstanceIndex[0] & 1;\n";

			if (m_data.colorLayered)
				writeShadingRate += "  gl_Layer = (inInstanceIndex[0] & 2) >> 1;\n";
		}

		std::stringstream gss;
		gss <<
			"#version 450 core\n"
			"#extension GL_EXT_fragment_shading_rate : enable\n"
			"\n"
			"layout(push_constant) uniform PC {\n"
			"	int shadingRate;\n"
			"} pc;\n"
			"\n"
			"in gl_PerVertex\n"
			"{\n"
			"   vec4 gl_Position;\n"
			"} gl_in[3];\n"
			"\n"
			"layout(location = 0) in int inInstanceIndex[];\n"
			"layout(location = 0) out int outInstanceIndex;\n"
			"layout(location = 1) out int readbackok;\n"
			"layout(location = 2) out float zero;\n"
			"layout(triangles) in;\n"
			"layout(triangle_strip, max_vertices=3) out;\n"
			"\n"
			"out gl_PerVertex {\n"
			"   vec4 gl_Position;\n"
			"};\n"
			"\n"
			"void main(void)\n"
			"{\n"
			"   gl_Position = gl_in[0].gl_Position;\n"
			"   outInstanceIndex = inInstanceIndex[0];\n"
			"   readbackok  = 1;\n"
			"   zero = 0;\n"
			<< writeShadingRate <<
			"   EmitVertex();"
			"\n"
			"   gl_Position = gl_in[1].gl_Position;\n"
			"   outInstanceIndex = inInstanceIndex[1];\n"
			"   readbackok = 1;\n"
			"   zero = 0;\n"
			<< writeShadingRate <<
			"   EmitVertex();"
			"\n"
			"   gl_Position = gl_in[2].gl_Position;\n"
			"   outInstanceIndex = inInstanceIndex[2];\n"
			"   readbackok = 1;\n"
			"   zero = 0;\n"
			<< writeShadingRate <<
			"   EmitVertex();"
			"}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(gss.str());
	}

	std::stringstream fss;

	fss <<
		"#version 450 core\n"
		"#extension GL_EXT_fragment_shading_rate : enable\n"
		"#extension GL_ARB_shader_stencil_export : enable\n"
		"#extension GL_ARB_fragment_shader_interlock : enable\n"
		"layout(location = 0) out uvec4 col0;\n"
		"layout(set = 0, binding = 0) buffer Block { uint counter; } buf;\n"
		"layout(set = 0, binding = 3) uniform usampler2D tex;\n"
		"layout(location = 0) flat in int instanceIndex;\n"
		"layout(location = 1) flat in int readbackok;\n"
		"layout(location = 2) " << (m_data.sampleShadingInput ? "sample " : "") << "in float zero;\n";

	if (m_data.interlock)
		fss << "layout(pixel_interlock_ordered) in;\n";

	fss <<
		"void main()\n"
		"{\n";

	if (m_data.interlock)
		fss << "  beginInvocationInterlockARB();\n";

	fss <<
		// X component gets shading rate enum
		"  col0.x = gl_ShadingRateEXT;\n"
		"  col0.y = 0;\n"
		// Z component gets packed primitiveID | atomic value
		"  col0.z = (instanceIndex << 24) | ((atomicAdd(buf.counter, 1) + 1) & 0x00FFFFFFu);\n"
		"  ivec2 fragCoordXY = ivec2(gl_FragCoord.xy);\n"
		"  ivec2 fragSize = ivec2(1<<((gl_ShadingRateEXT/4)&3), 1<<(gl_ShadingRateEXT&3));\n"
		// W component gets error code
		"  col0.w = uint(zero)" << (m_data.sampleShadingInput ? " * gl_SampleID" : "") << ";\n"
		"  if (((fragCoordXY - fragSize / 2) % fragSize) != ivec2(0,0))\n"
		"    col0.w = " << ERROR_FRAGCOORD_CENTER << ";\n";

	if (m_data.shaderWritesRate)
	{
		fss <<
			"  if (readbackok != 1)\n"
			"    col0.w = " << ERROR_VTG_READBACK << ";\n";
	}

	// When sample shading, gl_FragCoord is more likely to give bad derivatives,
	// e.g. due to a partially covered quad having some pixels center sample and
	// some sample at a sample location.
	if (!m_data.sampleShadingEnable && !m_data.sampleShadingInput)
	{
		fss << "  if (dFdx(gl_FragCoord.xy) != ivec2(fragSize.x, 0) || dFdy(gl_FragCoord.xy) != ivec2(0, fragSize.y))\n"
			   "    col0.w = (fragSize.y << 26) | (fragSize.x << 20) | (int(dFdx(gl_FragCoord.xy)) << 14) | (int(dFdx(gl_FragCoord.xy)) << 8) | " << ERROR_FRAGCOORD_DERIV << ";\n";

		fss << "  uint implicitDerivX = texture(tex, vec2(gl_FragCoord.x / textureSize(tex, 0).x, 0)).x;\n"
			   "  uint implicitDerivY = texture(tex, vec2(0, gl_FragCoord.y / textureSize(tex, 0).y)).x;\n"
			   "  if (implicitDerivX != fragSize.x || implicitDerivY != fragSize.y)\n"
			   "    col0.w = (fragSize.y << 26) | (fragSize.x << 20) | (implicitDerivY << 14) | (implicitDerivX << 8) | " << ERROR_FRAGCOORD_IMPLICIT_DERIV << ";\n";
	}
	// Y component gets sample mask value
	if (m_data.useSampleMaskIn)
		fss << "  col0.y = gl_SampleMaskIn[0];\n";

	if (m_data.fragDepth)
		fss << "  gl_FragDepth = float(instanceIndex) / float(" << NUM_TRIANGLES << ");\n";

	if (m_data.fragStencil)
		fss << "  gl_FragStencilRefARB = instanceIndex;\n";

	if (m_data.interlock)
		fss << "  endInvocationInterlockARB();\n";

	fss <<
		"}\n";

	programCollection.glslSources.add("frag") << glu::FragmentSource(fss.str());

	std::stringstream css;

	std::string fsampType = m_data.samples > 1 ?  "texture2DMSArray" :  "texture2DArray";
	std::string usampType = m_data.samples > 1 ? "utexture2DMSArray" : "utexture2DArray";

	// Compute shader copies color/depth/stencil to linear layout in buffer memory
	css <<
		"#version 450 core\n"
		"#extension GL_EXT_samplerless_texture_functions : enable\n"
		"layout(set = 0, binding = 1) uniform " << usampType << " colorTex;\n"
		"layout(set = 0, binding = 2, std430) buffer Block0 { uvec4 b[]; } colorbuf;\n"
		"layout(set = 0, binding = 4, std430) buffer Block1 { float b[]; } depthbuf;\n"
		"layout(set = 0, binding = 5, std430) buffer Block2 { uint b[]; } stencilbuf;\n"
		"layout(set = 0, binding = 6) uniform " << fsampType << " depthTex;\n"
		"layout(set = 0, binding = 7) uniform " << usampType << " stencilTex;\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"void main()\n"
		"{\n"
		"   for (int i = 0; i < " << m_data.samples << "; ++i) {\n"
		"      uint idx = ((gl_GlobalInvocationID.z * " << m_data.framebufferDim.height << " + gl_GlobalInvocationID.y) * " << m_data.framebufferDim.width << " + gl_GlobalInvocationID.x) * " << m_data.samples << " + i;\n"
		"      colorbuf.b[idx] = texelFetch(colorTex, ivec3(gl_GlobalInvocationID.xyz), i);\n";

	if (m_data.fragDepth)
		css << "      depthbuf.b[idx] = texelFetch(depthTex, ivec3(gl_GlobalInvocationID.xyz), i).x;\n";

	if (m_data.fragStencil)
		css << "      stencilbuf.b[idx] = texelFetch(stencilTex, ivec3(gl_GlobalInvocationID.xyz), i).x;\n";

	css <<
		"   }\n"
		"}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(css.str());
}

TestInstance* FSRTestCase::createInstance (Context& context) const
{
	return new FSRTestInstance(context, m_data);
}

deInt32 FSRTestInstance::ShadingRateExtentToEnum(VkExtent2D ext) const
{
	ext.width = deCtz32(ext.width);
	ext.height = deCtz32(ext.height);

	return (ext.width << 2) | ext.height;
}

VkExtent2D FSRTestInstance::ShadingRateEnumToExtent(deInt32 rate) const
{
	VkExtent2D ret;
	ret.width = 1 << ((rate/4) & 3);
	ret.height = 1 << (rate & 3);

	return ret;
}

VkExtent2D FSRTestInstance::Combine(VkExtent2D ext0, VkExtent2D ext1, VkFragmentShadingRateCombinerOpKHR comb) const
{
	VkExtent2D ret;
	switch (comb)
	{
	default:
		DE_ASSERT(0);
		// fallthrough
	case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR:
		return ext0;
	case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR:
		return ext1;
	case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR:
		ret = { de::min(ext0.width, ext1.width), de::min(ext0.height, ext1.height) };
		return ret;
	case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR:
		ret = { de::max(ext0.width, ext1.width), de::max(ext0.height, ext1.height) };
		return ret;
	case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR:
		ret = { ext0.width * ext1.width, ext0.height * ext1.height };
		if (!m_shadingRateProperties.fragmentShadingRateStrictMultiplyCombiner)
		{
			if (ext0.width == 1 && ext1.width == 1)
				ret.width = 2;
			if (ext0.height == 1 && ext1.height == 1)
				ret.height = 2;
		}
		return ret;
	}
}

deInt32 FSRTestInstance::Simulate(deInt32 rate0, deInt32 rate1, deInt32 rate2)
{
	deInt32 &cachedRate = m_simulateCache[(rate2*m_simulateValueCount + rate1)*m_simulateValueCount + rate0];
	if (cachedRate != ~0)
		return cachedRate;

	VkExtent2D extent0 = ShadingRateEnumToExtent(rate0);
	VkExtent2D extent1 = ShadingRateEnumToExtent(rate1);
	VkExtent2D extent2 = ShadingRateEnumToExtent(rate2);

	deInt32 finalMask = 0;
	// Simulate once for implementations that don't allow swapping rate xy,
	// and once for those that do. Any of those results is allowed.
	for (deUint32 allowSwap = 0; allowSwap <= 1; ++allowSwap)
	{
		// Combine rate 0 and 1, get a mask of possible clamped rates
		VkExtent2D intermed = Combine(extent0, extent1, m_data.combinerOp[0]);
		deInt32 intermedMask = ShadingRateExtentToClampedMask(intermed, allowSwap == 1);

		// For each clamped rate, combine that with rate 2 and accumulate the possible clamped rates
		for (int i = 0; i < 16; ++i)
		{
			if (intermedMask & (1<<i))
			{
				VkExtent2D final = Combine(ShadingRateEnumToExtent(i), extent2, m_data.combinerOp[1]);
				finalMask |= ShadingRateExtentToClampedMask(final, allowSwap == 1);
			}
		}
		{
			// unclamped intermediate value is also permitted
			VkExtent2D final = Combine(intermed, extent2, m_data.combinerOp[1]);
			finalMask |= ShadingRateExtentToClampedMask(final, allowSwap == 1);
		}
	}

	if (Force1x1())
		finalMask = 0x1;

	cachedRate = finalMask;
	return finalMask;
}

// If a rate is not valid (<=4x4), clamp it to something valid.
// This is only used for "inputs" to the system, not to mimic
// how the implementation internally clamps intermediate values.
VkExtent2D FSRTestInstance::SanitizeExtent(VkExtent2D ext) const
{
	DE_ASSERT(ext.width > 0 && ext.height > 0);

	ext.width = de::min(ext.width, 4u);
	ext.height = de::min(ext.height, 4u);

	return ext;
}

// Map an extent to a mask of all modes smaller than or equal to it in either dimension
deInt32 FSRTestInstance::ShadingRateExtentToClampedMask(VkExtent2D ext, bool allowSwap) const
{
	deUint32 desiredSize = ext.width * ext.height;

	deInt32 mask = 0;

	while (desiredSize > 0)
	{
		// First, find modes that maximize the area
		for (deUint32 i = 0; i < m_supportedFragmentShadingRateCount; ++i)
		{
			const VkPhysicalDeviceFragmentShadingRateKHR &supportedRate = m_supportedFragmentShadingRates[i];
			if ((supportedRate.sampleCounts & m_data.samples) &&
				supportedRate.fragmentSize.width * supportedRate.fragmentSize.height == desiredSize &&
				((supportedRate.fragmentSize.width  <= ext.width && supportedRate.fragmentSize.height <= ext.height) ||
				 (supportedRate.fragmentSize.height <= ext.width && supportedRate.fragmentSize.width  <= ext.height && allowSwap)))
			{
				mask |= 1 << ShadingRateExtentToEnum(supportedRate.fragmentSize);
			}
		}
		if (mask)
		{
			// Amongst the modes that maximize the area, pick the ones that
			// minimize the aspect ratio. Prefer ratio of 1, then 2, then 4.
			// 1x1 = 0, 2x2 = 5, 4x4 = 10
			static const deUint32 aspectMaskRatio1 = 0x421;
			// 2x1 = 4, 1x2 = 1, 4x2 = 9, 2x4 = 6
			static const deUint32 aspectMaskRatio2 = 0x252;
			// 4x1 = 8, 1x4 = 2,
			static const deUint32 aspectMaskRatio4 = 0x104;

			if (mask & aspectMaskRatio1)
			{
				mask &= aspectMaskRatio1;
				break;
			}
			if (mask & aspectMaskRatio2)
			{
				mask &= aspectMaskRatio2;
				break;
			}
			if (mask & aspectMaskRatio4)
			{
				mask &= aspectMaskRatio4;
				break;
			}
			DE_ASSERT(0);
		}
		desiredSize /= 2;
	}

	return mask;
}


deInt32 FSRTestInstance::SanitizeRate(deInt32 rate) const
{
	VkExtent2D extent = ShadingRateEnumToExtent(rate);

	extent = SanitizeExtent(extent);

	return ShadingRateExtentToEnum(extent);
}

// Map primID % 9 to primitive shading rate
deInt32 FSRTestInstance::PrimIDToPrimitiveShadingRate(deInt32 primID)
{
	deInt32 &cachedRate = m_primIDToPrimitiveShadingRate[primID];
	if (cachedRate != ~0)
		return cachedRate;

	VkExtent2D extent;
	extent.width = 1 << (primID % 3);
	extent.height = 1 << ((primID/3) % 3);

	cachedRate = ShadingRateExtentToEnum(extent);
	return cachedRate;
}

// Map primID / 9 to pipeline shading rate
deInt32 FSRTestInstance::PrimIDToPipelineShadingRate(deInt32 primID)
{
	deInt32 &cachedRate = m_primIDToPipelineShadingRate[primID];
	if (cachedRate != ~0)
		return cachedRate;

	primID /= 9;
	VkExtent2D extent;
	extent.width = 1 << (primID % 3);
	extent.height = 1 << ((primID/3) % 3);

	cachedRate = ShadingRateExtentToEnum(extent);
	return cachedRate;
}

static de::MovePtr<BufferWithMemory> CreateCachedBuffer(const vk::DeviceInterface&		vk,
														const vk::VkDevice				device,
														vk::Allocator&					allocator,
														const vk::VkBufferCreateInfo&	bufferCreateInfo)
{
	try
	{
		return de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::Cached));
	}
	catch (const tcu::NotSupportedError&)
	{
		return de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
	}
}

tcu::TestStatus FSRTestInstance::iterate (void)
{
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkDevice			device					= m_context.getDevice();
	tcu::TestLog&			log						= m_context.getTestContext().getLog();
	Allocator&				allocator				= m_context.getDefaultAllocator();
	VkFlags					allShaderStages			= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
	VkFlags					allPipelineStages		= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
													  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
													  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
													  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
													  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
													  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
													  VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV;
	const VkFormat			cbFormat				= VK_FORMAT_R32G32B32A32_UINT;
	const VkFormat			dsFormat				= VK_FORMAT_D32_SFLOAT_S8_UINT;

	if (m_data.geometryShader)
	{
		allShaderStages	|= VK_SHADER_STAGE_GEOMETRY_BIT;
		allPipelineStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
	}

	deRandom rnd;
	deRandom_init(&rnd, m_data.seed);

	qpTestResult res = QP_TEST_RESULT_PASS;
	deUint32 numUnexpected1x1Samples = 0;
	deUint32 numTotalSamples = 0;

	enum AttachmentModes
	{
		ATTACHMENT_MODE_DEFAULT = 0,
		ATTACHMENT_MODE_LAYOUT_OPTIMAL,
		ATTACHMENT_MODE_IMAGELESS,
		ATTACHMENT_MODE_2DARRAY,
		ATTACHMENT_MODE_TILING_LINEAR,

		ATTACHMENT_MODE_COUNT,
	};

	deUint32 numSRLayers = m_data.srLayered ? 2u : 1u;

	VkExtent2D minFragmentShadingRateAttachmentTexelSize = {1, 1};
	VkExtent2D maxFragmentShadingRateAttachmentTexelSize = {1, 1};
	deUint32 maxFragmentShadingRateAttachmentTexelSizeAspectRatio = 1;
	if (m_context.getFragmentShadingRateFeatures().attachmentFragmentShadingRate)
	{
		minFragmentShadingRateAttachmentTexelSize = m_context.getFragmentShadingRateProperties().minFragmentShadingRateAttachmentTexelSize;
		maxFragmentShadingRateAttachmentTexelSize = m_context.getFragmentShadingRateProperties().maxFragmentShadingRateAttachmentTexelSize;
		maxFragmentShadingRateAttachmentTexelSizeAspectRatio = m_context.getFragmentShadingRateProperties().maxFragmentShadingRateAttachmentTexelSizeAspectRatio;
	}

	VkDeviceSize atomicBufferSize = sizeof(deUint32);

	de::MovePtr<BufferWithMemory> atomicBuffer;
	atomicBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(atomicBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible | MemoryRequirement::Coherent));

	deUint32 *abuf = (deUint32 *)atomicBuffer->getAllocation().getHostPtr();

	// NUM_TRIANGLES triangles, 3 vertices, 2 components of float position
	VkDeviceSize vertexBufferSize = NUM_TRIANGLES * 3 * 2 * sizeof(float);

	de::MovePtr<BufferWithMemory> vertexBuffer;
	vertexBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible | MemoryRequirement::Coherent));

	float *vbuf = (float *)vertexBuffer->getAllocation().getHostPtr();
	for (deInt32 i = 0; i < (deInt32)(vertexBufferSize / sizeof(float)); ++i)
	{
		vbuf[i] = deRandom_getFloat(&rnd)*2.0f - 1.0f;
	}
	flushAlloc(vk, device, vertexBuffer->getAllocation());

	VkDeviceSize colorOutputBufferSize = m_data.framebufferDim.width * m_data.framebufferDim.height * m_data.samples * 4 * sizeof(deUint32) * m_data.numColorLayers;
	de::MovePtr<BufferWithMemory> colorOutputBuffer = CreateCachedBuffer(vk, device, allocator, makeBufferCreateInfo(colorOutputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

	VkDeviceSize depthOutputBufferSize = 0, stencilOutputBufferSize = 0;
	de::MovePtr<BufferWithMemory> depthOutputBuffer, stencilOutputBuffer;
	if (m_data.useDepthStencil)
	{
		depthOutputBufferSize = m_data.framebufferDim.width * m_data.framebufferDim.height * m_data.samples * sizeof(float) * m_data.numColorLayers;
		depthOutputBuffer = CreateCachedBuffer(vk, device, allocator, makeBufferCreateInfo(depthOutputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

		stencilOutputBufferSize = m_data.framebufferDim.width * m_data.framebufferDim.height * m_data.samples * sizeof(deUint32) * m_data.numColorLayers;
		stencilOutputBuffer = CreateCachedBuffer(vk, device, allocator, makeBufferCreateInfo(stencilOutputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	}

	deUint32 minSRTexelWidth = minFragmentShadingRateAttachmentTexelSize.width;
	deUint32 minSRTexelHeight = minFragmentShadingRateAttachmentTexelSize.height;
	deUint32 maxSRWidth = (m_data.framebufferDim.width + minSRTexelWidth - 1) / minSRTexelWidth;
	deUint32 maxSRHeight = (m_data.framebufferDim.height + minSRTexelHeight - 1) / minSRTexelHeight;

	// max size over all formats
	VkDeviceSize srFillBufferSize = numSRLayers * maxSRWidth * maxSRHeight * 32/*4 component 64-bit*/;
	de::MovePtr<BufferWithMemory> srFillBuffer;
	deUint8 *fillPtr = DE_NULL;
	if (m_data.useAttachment())
	{
		srFillBuffer = CreateCachedBuffer(vk, device, allocator, makeBufferCreateInfo(srFillBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
		fillPtr = (deUint8 *)srFillBuffer->getAllocation().getHostPtr();
	}

	de::MovePtr<ImageWithMemory> cbImage;
	Move<VkImageView> cbImageView;
	{
		const VkImageCreateInfo			imageCreateInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
			cbFormat,								// VkFormat					format;
			{
				m_data.framebufferDim.width,		// deUint32	width;
				m_data.framebufferDim.height,		// deUint32	height;
				1u									// deUint32	depth;
			},										// VkExtent3D				extent;
			1u,										// deUint32					mipLevels;
			m_data.numColorLayers,					// deUint32					arrayLayers;
			m_data.samples,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
			cbUsage,								// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
			0u,										// deUint32					queueFamilyIndexCount;
			DE_NULL,								// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
		};
		cbImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
			vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));

		VkImageViewCreateInfo		imageViewCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
			**cbImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D_ARRAY,				// VkImageViewType			viewType;
			cbFormat,									// VkFormat					format;
			{
				VK_COMPONENT_SWIZZLE_R,					// VkComponentSwizzle	r;
				VK_COMPONENT_SWIZZLE_G,					// VkComponentSwizzle	g;
				VK_COMPONENT_SWIZZLE_B,					// VkComponentSwizzle	b;
				VK_COMPONENT_SWIZZLE_A					// VkComponentSwizzle	a;
			},											// VkComponentMapping		 components;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
				0u,										// deUint32				baseMipLevel;
				1u,										// deUint32				levelCount;
				0u,										// deUint32				baseArrayLayer;
				m_data.numColorLayers					// deUint32				layerCount;
			}											// VkImageSubresourceRange	subresourceRange;
		};
		cbImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
	}

	de::MovePtr<ImageWithMemory> dsImage;
	Move<VkImageView> dsImageView, dImageView, sImageView;
	VkImageUsageFlags dsUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
								VK_IMAGE_USAGE_SAMPLED_BIT |
								VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
								VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (m_data.useDepthStencil)
	{
		const VkImageCreateInfo			imageCreateInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
			dsFormat,								// VkFormat					format;
			{
				m_data.framebufferDim.width,		// deUint32	width;
				m_data.framebufferDim.height,		// deUint32	height;
				1u									// deUint32	depth;
			},										// VkExtent3D				extent;
			1u,										// deUint32					mipLevels;
			m_data.numColorLayers,					// deUint32					arrayLayers;
			m_data.samples,							// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
			dsUsage,								// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
			0u,										// deUint32					queueFamilyIndexCount;
			DE_NULL,								// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
		};
		dsImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
			vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));

		VkImageViewCreateInfo		imageViewCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
			**dsImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D_ARRAY,				// VkImageViewType			viewType;
			dsFormat,									// VkFormat					format;
			{
				VK_COMPONENT_SWIZZLE_R,					// VkComponentSwizzle	r;
				VK_COMPONENT_SWIZZLE_G,					// VkComponentSwizzle	g;
				VK_COMPONENT_SWIZZLE_B,					// VkComponentSwizzle	b;
				VK_COMPONENT_SWIZZLE_A					// VkComponentSwizzle	a;
			},											// VkComponentMapping		 components;
			{
				VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask;
				0u,										// deUint32				baseMipLevel;
				1u,										// deUint32				levelCount;
				0u,										// deUint32				baseArrayLayer;
				m_data.numColorLayers					// deUint32				layerCount;
			}											// VkImageSubresourceRange	subresourceRange;
		};
		dsImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		dImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		sImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
	}

	// Image used to test implicit derivative calculations.
	// Filled with a value of 1<<lod.
	de::MovePtr<ImageWithMemory> derivImage;
	Move<VkImageView> derivImageView;
	VkImageUsageFlags derivUsage = VK_IMAGE_USAGE_SAMPLED_BIT |
								   VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	deUint32 derivNumLevels;
	{
		deUint32 maxDim = de::max(m_context.getFragmentShadingRateProperties().maxFragmentSize.width, m_context.getFragmentShadingRateProperties().maxFragmentSize.height);
		derivNumLevels = 1 + deCtz32(maxDim);
		const VkImageCreateInfo			imageCreateInfo			=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
			VK_FORMAT_R32_UINT,						// VkFormat					format;
			{
				m_context.getFragmentShadingRateProperties().maxFragmentSize.width,		// deUint32	width;
				m_context.getFragmentShadingRateProperties().maxFragmentSize.height,	// deUint32	height;
				1u									// deUint32	depth;
			},										// VkExtent3D				extent;
			derivNumLevels,							// deUint32					mipLevels;
			1u,										// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
			derivUsage,								// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
			0u,										// deUint32					queueFamilyIndexCount;
			DE_NULL,								// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
		};
		derivImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
			vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));

		VkImageViewCreateInfo		imageViewCreateInfo		=
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
			**derivImage,								// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
			VK_FORMAT_R32_UINT,							// VkFormat					format;
			{
				VK_COMPONENT_SWIZZLE_R,					// VkComponentSwizzle	r;
				VK_COMPONENT_SWIZZLE_G,					// VkComponentSwizzle	g;
				VK_COMPONENT_SWIZZLE_B,					// VkComponentSwizzle	b;
				VK_COMPONENT_SWIZZLE_A					// VkComponentSwizzle	a;
			},											// VkComponentMapping		 components;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
				0u,										// deUint32				baseMipLevel;
				derivNumLevels,							// deUint32				levelCount;
				0u,										// deUint32				baseArrayLayer;
				1u										// deUint32				layerCount;
			}											// VkImageSubresourceRange	subresourceRange;
		};
		derivImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
	}

	// sampler used with derivImage
	const struct VkSamplerCreateInfo		samplerInfo	=
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// sType
		DE_NULL,									// pNext
		0u,											// flags
		VK_FILTER_NEAREST,							// magFilter
		VK_FILTER_NEAREST,							// minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,				// mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,		// addressModeW
		0.0f,										// mipLodBias
		VK_FALSE,									// anisotropyEnable
		1.0f,										// maxAnisotropy
		DE_FALSE,									// compareEnable
		VK_COMPARE_OP_ALWAYS,						// compareOp
		0.0f,										// minLod
		(float)derivNumLevels,						// maxLod
		VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,		// borderColor
		VK_FALSE,									// unnormalizedCoords
	};

	Move<VkSampler>			sampler	= createSampler(vk, device, &samplerInfo);

	Move<vk::VkDescriptorSetLayout>	descriptorSetLayout;
	VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

	const VkDescriptorSetLayoutBinding bindings[] =
	{
		{
			0u,										// binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			1u,										// binding
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			2u,										// binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			3u,										// binding
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			4u,										// binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			5u,										// binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			6u,										// binding
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
		{
			7u,										// binding
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,		// descriptorType
			1u,										// descriptorCount
			allShaderStages,						// stageFlags
			DE_NULL,								// pImmutableSamplers
		},
	};

	// Create a layout and allocate a descriptor set for it.
	const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// sType
		DE_NULL,													// pNext
		layoutCreateFlags,											// flags
		sizeof(bindings)/sizeof(bindings[0]),						// bindingCount
		&bindings[0]												// pBindings
	};

	descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

	const VkPushConstantRange				pushConstantRange				=
	{
		allShaderStages,											// VkShaderStageFlags					stageFlags;
		0u,															// deUint32								offset;
		sizeof(deInt32)												// deUint32								size;
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		1,															// setLayoutCount
		&descriptorSetLayout.get(),									// pSetLayouts
		1u,															// pushConstantRangeCount
		&pushConstantRange,											// pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	const Unique<VkShaderModule>	cs						(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));

	const VkPipelineShaderStageCreateInfo	csShaderCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT,								// stage
		*cs,														// shader
		"main",
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
		csShaderCreateInfo,											// cs
		*pipelineLayout,											// layout
		(vk::VkPipeline)0,											// basePipelineHandle
		0u,															// basePipelineIndex
	};
	Move<VkPipeline> computePipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo, NULL);

	for (deUint32 modeIdx = 0; modeIdx < ATTACHMENT_MODE_COUNT; ++modeIdx)
	{
		// If we're not using an attachment, don't test all the different attachment modes
		if (modeIdx != ATTACHMENT_MODE_DEFAULT && !m_data.useAttachment())
			continue;

		// Consider all uint formats possible
		static const VkFormat srFillFormats[] =
		{
			VK_FORMAT_R8_UINT,
			VK_FORMAT_R8G8_UINT,
			VK_FORMAT_R8G8B8_UINT,
			VK_FORMAT_R8G8B8A8_UINT,
			VK_FORMAT_R16_UINT,
			VK_FORMAT_R16G16_UINT,
			VK_FORMAT_R16G16B16_UINT,
			VK_FORMAT_R16G16B16A16_UINT,
			VK_FORMAT_R32_UINT,
			VK_FORMAT_R32G32_UINT,
			VK_FORMAT_R32G32B32_UINT,
			VK_FORMAT_R32G32B32A32_UINT,
			VK_FORMAT_R64_UINT,
			VK_FORMAT_R64G64_UINT,
			VK_FORMAT_R64G64B64_UINT,
			VK_FORMAT_R64G64B64A64_UINT,
		};
		// Only test all formats in the default mode
		deUint32 numFillFormats = modeIdx == ATTACHMENT_MODE_DEFAULT ? (deUint32)(sizeof(srFillFormats)/sizeof(srFillFormats[0])) : 1u;

		// Iterate over all supported tile sizes and formats
		for (deUint32 srTexelWidth  = minFragmentShadingRateAttachmentTexelSize.width;
					  srTexelWidth <= maxFragmentShadingRateAttachmentTexelSize.width;
					  srTexelWidth *= 2)
		for (deUint32 srTexelHeight  = minFragmentShadingRateAttachmentTexelSize.height;
					  srTexelHeight <= maxFragmentShadingRateAttachmentTexelSize.height;
					  srTexelHeight *= 2)
		for (deUint32 formatIdx = 0; formatIdx < numFillFormats; ++formatIdx)
		{

			deUint32 aspectRatio = (srTexelHeight > srTexelWidth) ? (srTexelHeight / srTexelWidth) : (srTexelWidth / srTexelHeight);
			if (aspectRatio > maxFragmentShadingRateAttachmentTexelSizeAspectRatio)
				continue;

			// Go through the loop only once when not using an attachment
			if (!m_data.useAttachment() &&
				(srTexelWidth != minFragmentShadingRateAttachmentTexelSize.width ||
				 srTexelHeight != minFragmentShadingRateAttachmentTexelSize.height ||
				 formatIdx != 0))
				 continue;

			bool imagelessFB = modeIdx == ATTACHMENT_MODE_IMAGELESS;

			deUint32 srWidth = (m_data.framebufferDim.width + srTexelWidth - 1) / srTexelWidth;
			deUint32 srHeight = (m_data.framebufferDim.height + srTexelHeight - 1) / srTexelHeight;

			VkFormat srFormat = srFillFormats[formatIdx];
			deUint32 srFillBpp = tcu::getPixelSize(mapVkFormat(srFormat));

			VkImageLayout srLayout = modeIdx == ATTACHMENT_MODE_LAYOUT_OPTIMAL ? VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR : VK_IMAGE_LAYOUT_GENERAL;
			VkImageViewType srViewType = modeIdx == ATTACHMENT_MODE_2DARRAY ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			VkImageTiling srTiling = (modeIdx == ATTACHMENT_MODE_TILING_LINEAR) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

			VkFormatProperties srFormatProperties;
			m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), srFormat, &srFormatProperties);
			VkFormatFeatureFlags srFormatFeatures = srTiling == VK_IMAGE_TILING_LINEAR ? srFormatProperties.linearTilingFeatures : srFormatProperties.optimalTilingFeatures;

			if (m_context.getFragmentShadingRateFeatures().attachmentFragmentShadingRate &&
				!(srFormatFeatures & VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR))
			{
				if (srFormat == VK_FORMAT_R8_UINT && srTiling == VK_IMAGE_TILING_OPTIMAL)
				{
					log << tcu::TestLog::Message << "VK_FORMAT_R8_UINT/VK_IMAGE_TILING_OPTIMAL don't support VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR" << tcu::TestLog::EndMessage;
					res = QP_TEST_RESULT_FAIL;
				}
				continue;
			}

			Move<vk::VkDescriptorPool>		descriptorPool;
			Move<vk::VkDescriptorSet>		descriptorSet;
			VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

			vk::DescriptorPoolBuilder poolBuilder;
			for (deInt32 i = 0; i < (deInt32)(sizeof(bindings)/sizeof(bindings[0])); ++i)
				poolBuilder.addType(bindings[i].descriptorType, bindings[i].descriptorCount);

			descriptorPool = poolBuilder.build(vk, device, poolCreateFlags, 1u);
			descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

			de::MovePtr<ImageWithMemory> srImage;
			Move<VkImageView> srImageView;
			VkImageUsageFlags srUsage = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR |
										VK_IMAGE_USAGE_TRANSFER_DST_BIT |
										VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			if (m_data.useAttachment())
			{
				const VkImageCreateInfo			imageCreateInfo			=
				{
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
					DE_NULL,								// const void*				pNext;
					(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
					VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
					srFormat,								// VkFormat					format;
					{
						srWidth,							// deUint32	width;
						srHeight,							// deUint32	height;
						1u									// deUint32	depth;
					},										// VkExtent3D				extent;
					1u,										// deUint32					mipLevels;
					numSRLayers,							// deUint32					arrayLayers;
					VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
					srTiling,								// VkImageTiling			tiling;
					srUsage,								// VkImageUsageFlags		usage;
					VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
					0u,										// deUint32					queueFamilyIndexCount;
					DE_NULL,								// const deUint32*			pQueueFamilyIndices;
					VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
				};
				srImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
					vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));

				VkImageViewCreateInfo		imageViewCreateInfo		=
				{
					VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
					DE_NULL,									// const void*				pNext;
					(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
					**srImage,									// VkImage					image;
					srViewType,									// VkImageViewType			viewType;
					srFormat,									// VkFormat					format;
					{
						VK_COMPONENT_SWIZZLE_R,					// VkComponentSwizzle	r;
						VK_COMPONENT_SWIZZLE_G,					// VkComponentSwizzle	g;
						VK_COMPONENT_SWIZZLE_B,					// VkComponentSwizzle	b;
						VK_COMPONENT_SWIZZLE_A					// VkComponentSwizzle	a;
					},											// VkComponentMapping		 components;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
						0u,										// deUint32				baseMipLevel;
						1u,										// deUint32				levelCount;
						0u,										// deUint32				baseArrayLayer;
						srViewType == VK_IMAGE_VIEW_TYPE_2D ?
						1 : numSRLayers,						// deUint32				layerCount;
					}											// VkImageSubresourceRange	subresourceRange;
				};
				srImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
			}

			VkDescriptorImageInfo imageInfo;
			VkDescriptorBufferInfo bufferInfo;

			VkWriteDescriptorSet w =
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,							// sType
				DE_NULL,														// pNext
				*descriptorSet,													// dstSet
				(deUint32)0,													// dstBinding
				0,																// dstArrayElement
				1u,																// descriptorCount
				bindings[0].descriptorType,										// descriptorType
				&imageInfo,														// pImageInfo
				&bufferInfo,													// pBufferInfo
				DE_NULL,														// pTexelBufferView
			};

			abuf[0] = 0;
			flushAlloc(vk, device, atomicBuffer->getAllocation());

			bufferInfo = makeDescriptorBufferInfo(**atomicBuffer, 0, atomicBufferSize);
			w.dstBinding = 0;
			w.descriptorType = bindings[0].descriptorType;
			vk.updateDescriptorSets(device, 1, &w, 0, NULL);

			imageInfo = makeDescriptorImageInfo(DE_NULL, *cbImageView, VK_IMAGE_LAYOUT_GENERAL);
			w.dstBinding = 1;
			w.descriptorType = bindings[1].descriptorType;
			vk.updateDescriptorSets(device, 1, &w, 0, NULL);

			bufferInfo = makeDescriptorBufferInfo(**colorOutputBuffer, 0, colorOutputBufferSize);
			w.dstBinding = 2;
			w.descriptorType = bindings[2].descriptorType;
			vk.updateDescriptorSets(device, 1, &w, 0, NULL);

			imageInfo = makeDescriptorImageInfo(*sampler, *derivImageView, VK_IMAGE_LAYOUT_GENERAL);
			w.dstBinding = 3;
			w.descriptorType = bindings[3].descriptorType;
			vk.updateDescriptorSets(device, 1, &w, 0, NULL);

			if (m_data.useDepthStencil)
			{
				bufferInfo = makeDescriptorBufferInfo(**depthOutputBuffer, 0, depthOutputBufferSize);
				w.dstBinding = 4;
				w.descriptorType = bindings[4].descriptorType;
				vk.updateDescriptorSets(device, 1, &w, 0, NULL);

				bufferInfo = makeDescriptorBufferInfo(**stencilOutputBuffer, 0, stencilOutputBufferSize);
				w.dstBinding = 5;
				w.descriptorType = bindings[5].descriptorType;
				vk.updateDescriptorSets(device, 1, &w, 0, NULL);

				imageInfo = makeDescriptorImageInfo(DE_NULL, *dImageView, VK_IMAGE_LAYOUT_GENERAL);
				w.dstBinding = 6;
				w.descriptorType = bindings[6].descriptorType;
				vk.updateDescriptorSets(device, 1, &w, 0, NULL);

				imageInfo = makeDescriptorImageInfo(DE_NULL, *sImageView, VK_IMAGE_LAYOUT_GENERAL);
				w.dstBinding = 7;
				w.descriptorType = bindings[7].descriptorType;
				vk.updateDescriptorSets(device, 1, &w, 0, NULL);
			}

			Move<VkRenderPass> renderPass;
			Move<VkFramebuffer> framebuffer;

			std::vector<VkImageView> attachments;
			attachments.push_back(*cbImageView);
			deUint32 dsAttachmentIdx = 0, srAttachmentIdx = 0;
			if (m_data.useAttachment())
			{
				srAttachmentIdx = (deUint32)attachments.size();
				attachments.push_back(*srImageView);
			}
			if (m_data.useDepthStencil)
			{
				dsAttachmentIdx = (deUint32)attachments.size();
				attachments.push_back(*dsImageView);
			}

			if (!m_data.useDynamicRendering)
			{
				const vk::VkAttachmentReference2 colorAttachmentReference
				{
					VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,					// sType
					DE_NULL,													// pNext
					0,															// attachment
					vk::VK_IMAGE_LAYOUT_GENERAL,								// layout
					0,															// aspectMask
				};

				const vk::VkAttachmentReference2 fragmentShadingRateAttachment =
				{
					VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,					// sType
					DE_NULL,													// pNext
					srAttachmentIdx,											// attachment
					srLayout,													// layout
					0,															// aspectMask
				};

				const vk::VkAttachmentReference2 depthAttachmentReference =
				{
					VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,					// sType
					DE_NULL,													// pNext
					dsAttachmentIdx,											// attachment
					vk::VK_IMAGE_LAYOUT_GENERAL,								// layout
					0,															// aspectMask
				};

				const bool										noAttachmentPtr				= (m_data.attachmentUsage == AttachmentUsage::NO_ATTACHMENT_PTR);
				const VkFragmentShadingRateAttachmentInfoKHR	shadingRateAttachmentInfo	=
				{
					VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,							// VkStructureType				  sType;
					DE_NULL,																				// const void*					  pNext;
					(noAttachmentPtr ? nullptr : &fragmentShadingRateAttachment),							// const VkAttachmentReference2*	pFragmentShadingRateAttachment;
					{ srTexelWidth, srTexelHeight },														// VkExtent2D					   shadingRateAttachmentTexelSize;
				};

				const bool						useAttachmentInfo	= (m_data.attachmentUsage != AttachmentUsage::NO_ATTACHMENT);
				const VkSubpassDescription2		subpassDesc			=
				{
					VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,						// sType
					(useAttachmentInfo ? &shadingRateAttachmentInfo : nullptr),		// pNext;
					(vk::VkSubpassDescriptionFlags)0,								// flags
					vk::VK_PIPELINE_BIND_POINT_GRAPHICS,							// pipelineBindPoint
					m_data.multiView ? 0x3 : 0u,									// viewMask
					0u,																// inputCount
					DE_NULL,														// pInputAttachments
					1,																// colorCount
					&colorAttachmentReference,										// pColorAttachments
					DE_NULL,														// pResolveAttachments
					m_data.useDepthStencil ? &depthAttachmentReference : DE_NULL,	// depthStencilAttachment
					0u,																// preserveCount
					DE_NULL,														// pPreserveAttachments
				};

				std::vector<VkAttachmentDescription2> attachmentDescriptions
				{
					{
						VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,	// VkStructureType sType;
						DE_NULL,									// const void* pNext;
						(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
						cbFormat,					// VkFormat							format;
						m_data.samples,								// VkSampleCountFlagBits			samples;
						VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp;
						VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
						VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
						VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
						VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					initialLayout;
						VK_IMAGE_LAYOUT_GENERAL						// VkImageLayout					finalLayout;
					}
				};
				if (m_data.useAttachment())
					attachmentDescriptions.push_back(
					{
						VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,	// VkStructureType sType;
						DE_NULL,									// const void* pNext;
						(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
						srFormat,									// VkFormat							format;
						VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples;
						VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp;
						VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
						VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
						VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
						srLayout,									// VkImageLayout					initialLayout;
						srLayout									// VkImageLayout					finalLayout;
					}
					);

				if (m_data.useDepthStencil)
					attachmentDescriptions.push_back(
					{
						VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,	// VkStructureType sType;
						DE_NULL,									// const void* pNext;
						(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
						dsFormat,					// VkFormat							format;
						m_data.samples,								// VkSampleCountFlagBits			samples;
						VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp;
						VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
						VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				stencilLoadOp;
						VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				stencilStoreOp;
						VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					initialLayout;
						VK_IMAGE_LAYOUT_GENERAL						// VkImageLayout					finalLayout;
					}
					);

				const VkRenderPassCreateInfo2	renderPassParams	=
				{
					VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,			// sType
					DE_NULL,												// pNext
					(vk::VkRenderPassCreateFlags)0,
					(deUint32)attachmentDescriptions.size(),				// attachmentCount
					&attachmentDescriptions[0],								// pAttachments
					1u,														// subpassCount
					&subpassDesc,											// pSubpasses
					0u,														// dependencyCount
					DE_NULL,												// pDependencies
					0u,														// correlatedViewMaskCount
					DE_NULL,												// pCorrelatedViewMasks
				};

				renderPass = createRenderPass2(vk, device, &renderPassParams);

				std::vector<VkFramebufferAttachmentImageInfo> framebufferAttachmentImageInfo;
				framebufferAttachmentImageInfo.push_back(
					{
						VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
						DE_NULL,													//  const void*			pNext;
						(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
						cbUsage,													//  VkImageUsageFlags	usage;
						m_data.framebufferDim.width,								//  deUint32			width;
						m_data.framebufferDim.height,								//  deUint32			height;
						m_data.numColorLayers,										//  deUint32			layerCount;
						1u,															//  deUint32			viewFormatCount;
						&cbFormat													//  const VkFormat*		pViewFormats;
					}
				);
				if (m_data.useAttachment())
					framebufferAttachmentImageInfo.push_back(
					{
						VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
						DE_NULL,													//  const void*			pNext;
						(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
						srUsage,													//  VkImageUsageFlags	usage;
						srWidth,													//  deUint32			width;
						srHeight,													//  deUint32			height;
						numSRLayers,												//  deUint32			layerCount;
						1u,															//  deUint32			viewFormatCount;
						&srFormat													//  const VkFormat*		pViewFormats;
					}
					);

				if (m_data.useDepthStencil)
					framebufferAttachmentImageInfo.push_back(
					{
						VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO,		//  VkStructureType		sType;
						DE_NULL,													//  const void*			pNext;
						(VkImageCreateFlags)0u,										//  VkImageCreateFlags	flags;
						dsUsage,													//  VkImageUsageFlags	usage;
						m_data.framebufferDim.width,								//  deUint32			width;
						m_data.framebufferDim.height,								//  deUint32			height;
						m_data.numColorLayers,										//  deUint32			layerCount;
						1u,															//  deUint32			viewFormatCount;
						&dsFormat													//  const VkFormat*		pViewFormats;
					}
					);

				const VkFramebufferAttachmentsCreateInfo				framebufferAttachmentsCreateInfo	=
				{
					VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO,		//  VkStructureType								sType;
					DE_NULL,													//  const void*									pNext;
					(deUint32)framebufferAttachmentImageInfo.size(),			//  deUint32									attachmentImageInfoCount;
					&framebufferAttachmentImageInfo[0]							//  const VkFramebufferAttachmentImageInfo*		pAttachmentImageInfos;
				};

				const vk::VkFramebufferCreateInfo	framebufferParams	=
				{
					vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
					imagelessFB ? &framebufferAttachmentsCreateInfo : DE_NULL,				// pNext
					(vk::VkFramebufferCreateFlags)(imagelessFB ? VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT : 0),
					*renderPass,									// renderPass
					(deUint32)attachments.size(),					// attachmentCount
					imagelessFB ? DE_NULL : &attachments[0],		// pAttachments
					m_data.framebufferDim.width,					// width
					m_data.framebufferDim.height,					// height
					m_data.multiView ? 1 : m_data.numColorLayers,	// layers
				};

				framebuffer = createFramebuffer(vk, device, &framebufferParams);
			}

			const VkVertexInputBindingDescription		vertexBinding =
			{
				0u,							// deUint32				binding;
				sizeof(float) * 2,			// deUint32				stride;
				VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputRate	inputRate;
			};
			const VkVertexInputAttributeDescription		vertexInputAttributeDescription =
			{
				0u,							// deUint32	location;
				0u,							// deUint32	binding;
				VK_FORMAT_R32G32_SFLOAT,	// VkFormat	format;
				0u							// deUint32	offset;
			};

			const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
				DE_NULL,													// const void*								pNext;
				(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
				1u,															// deUint32									vertexBindingDescriptionCount;
				&vertexBinding,												// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
				1u,															// deUint32									vertexAttributeDescriptionCount;
				&vertexInputAttributeDescription							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
			};

			const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
				DE_NULL,														// const void*								pNext;
				(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
				VK_FALSE														// VkBool32									primitiveRestartEnable;
			};

			const VkPipelineRasterizationConservativeStateCreateInfoEXT consRastState =
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT,	// VkStructureType										   sType;
				DE_NULL,																		// const void*											   pNext;
				(VkPipelineRasterizationConservativeStateCreateFlagsEXT)0,						// VkPipelineRasterizationConservativeStateCreateFlagsEXT	flags;
				m_data.conservativeMode,														// VkConservativeRasterizationModeEXT						conservativeRasterizationMode;
				0.0f,																			// float													 extraPrimitiveOverestimationSize;
			};

			const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
			{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
				m_data.conservativeEnable ? &consRastState : DE_NULL,			// const void*								pNext;
				(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
				VK_FALSE,														// VkBool32									depthClampEnable;
				VK_FALSE,														// VkBool32									rasterizerDiscardEnable;
				VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
				VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
				VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
				VK_FALSE,														// VkBool32									depthBiasEnable;
				0.0f,															// float									depthBiasConstantFactor;
				0.0f,															// float									depthBiasClamp;
				0.0f,															// float									depthBiasSlopeFactor;
				1.0f															// float									lineWidth;
			};

			// Kill some bits from each AA mode
			const VkSampleMask	sampleMask	= m_data.sampleMaskTest ? 0x9 : 0x7D56;
			const VkSampleMask*	pSampleMask = m_data.useApiSampleMask ? &sampleMask : DE_NULL;

			// All samples at pixel center. We'll validate that pixels are fully covered or uncovered.
			std::vector<VkSampleLocationEXT> sampleLocations(m_data.samples, { 0.5f, 0.5f });
			const VkSampleLocationsInfoEXT sampleLocationsInfo =
			{
				VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				(VkSampleCountFlagBits)m_data.samples,			// VkSampleCountFlagBits		sampleLocationsPerPixel;
				{ 1, 1 },										// VkExtent2D					sampleLocationGridSize;
				(deUint32)m_data.samples,						// uint32_t						sampleLocationsCount;
				&sampleLocations[0],							// const VkSampleLocationEXT*	pSampleLocations;
			};

			const VkPipelineSampleLocationsStateCreateInfoEXT pipelineSampleLocationsCreateInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,	// VkStructureType			sType;
				DE_NULL,															// const void*				pNext;
				VK_TRUE,															// VkBool32					sampleLocationsEnable;
				sampleLocationsInfo,												// VkSampleLocationsInfoEXT	sampleLocationsInfo;
			};

			const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
				m_data.sampleLocations ? &pipelineSampleLocationsCreateInfo : DE_NULL,	// const void*					pNext
				0u,															// VkPipelineMultisampleStateCreateFlags	flags
				(VkSampleCountFlagBits)m_data.samples,						// VkSampleCountFlagBits					rasterizationSamples
				(VkBool32)m_data.sampleShadingEnable,						// VkBool32									sampleShadingEnable
				1.0f,														// float									minSampleShading
				pSampleMask,												// const VkSampleMask*						pSampleMask
				VK_FALSE,													// VkBool32									alphaToCoverageEnable
				VK_FALSE													// VkBool32									alphaToOneEnable
			};

			std::vector<VkViewport> viewports;
			std::vector<VkRect2D> scissors;
			if (m_data.multiViewport)
			{
				// Split the viewport into left and right halves
				int x0 = 0, x1 = m_data.framebufferDim.width/2, x2 = m_data.framebufferDim.width;

				viewports.push_back(makeViewport((float)x0, 0, (float)(x1-x0), (float)m_data.framebufferDim.height, 0.0f, 1.0f));
				scissors.push_back(makeRect2D(x0, 0, x1-x0, m_data.framebufferDim.height));

				viewports.push_back(makeViewport((float)x1, 0, (float)(x2-x1), (float)m_data.framebufferDim.height, 0.0f, 1.0f));
				scissors.push_back(makeRect2D(x1, 0, x2-x1, m_data.framebufferDim.height));
			}
			else
			{
				viewports.push_back(makeViewport(m_data.framebufferDim.width, m_data.framebufferDim.height));
				scissors.push_back(makeRect2D(m_data.framebufferDim.width, m_data.framebufferDim.height));
			}

			const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
			{
				VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		// VkStructureType							sType
				DE_NULL,													// const void*								pNext
				(VkPipelineViewportStateCreateFlags)0,						// VkPipelineViewportStateCreateFlags		flags
				(deUint32)viewports.size(),									// deUint32									viewportCount
				&viewports[0],												// const VkViewport*						pViewports
				(deUint32)scissors.size(),									// deUint32									scissorCount
				&scissors[0]												// const VkRect2D*							pScissors
			};

			Move<VkShaderModule> fragShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);
			Move<VkShaderModule> vertShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			Move<VkShaderModule> geomShader;
			if (m_data.geometryShader)
				geomShader = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0);

			deUint32 numStages = m_data.geometryShader ? 3 : 2u;

			const VkPipelineShaderStageCreateInfo	shaderCreateInfo[3] =
			{
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					DE_NULL,
					(VkPipelineShaderStageCreateFlags)0,
					VK_SHADER_STAGE_VERTEX_BIT,									// stage
					*vertShader,												// shader
					"main",
					DE_NULL,													// pSpecializationInfo
				},
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					DE_NULL,
					(VkPipelineShaderStageCreateFlags)0,
					VK_SHADER_STAGE_FRAGMENT_BIT,								// stage
					*fragShader,												// shader
					"main",
					DE_NULL,													// pSpecializationInfo
				},
				{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					DE_NULL,
					(VkPipelineShaderStageCreateFlags)0,
					VK_SHADER_STAGE_GEOMETRY_BIT,								// stage
					*geomShader,												// shader
					"main",
					DE_NULL,													// pSpecializationInfo
				}
			};

			const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
			{
				VK_FALSE,				// VkBool32					 blendEnable;
				VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			srcColorBlendFactor;
				VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			dstColorBlendFactor;
				VK_BLEND_OP_ADD,		// VkBlendOp				colorBlendOp;
				VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			srcAlphaBlendFactor;
				VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			dstAlphaBlendFactor;
				VK_BLEND_OP_ADD,		// VkBlendOp				alphaBlendOp;
				0xf						// VkColorComponentFlags	colorWriteMask;
			};

			const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
			{
				VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
				DE_NULL,													// const void*									pNext;
				0u,															// VkPipelineColorBlendStateCreateFlags			flags;
				VK_FALSE,													// VkBool32										logicOpEnable;
				VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
				1u,															// deUint32										attachmentCount;
				&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
				{ 1.0f, 1.0f, 1.0f, 1.0f }									// float										blendConstants[4];
			};

			const deUint32 fragSizeWH = m_data.sampleMaskTest ? 2 : 0;

			VkPipelineRenderingCreateInfoKHR renderingCreateInfo
			{
				VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
				DE_NULL,
				m_data.multiView ? 0x3 : 0u,
				1u,
				&cbFormat,
				m_data.useDepthStencil ? dsFormat : VK_FORMAT_UNDEFINED,
				m_data.useDepthStencil ? dsFormat : VK_FORMAT_UNDEFINED
			};

			VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRateStateCreateInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,	// VkStructureType						sType;
				m_data.useDynamicRendering ? &renderingCreateInfo : DE_NULL,			// const void*							pNext;
				{ fragSizeWH, fragSizeWH },												// VkExtent2D							fragmentSize;
				{ m_data.combinerOp[0], m_data.combinerOp[1] },							// VkFragmentShadingRateCombinerOpKHR	combinerOps[2];
			};

			VkDynamicState dynamicState = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
			const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				(VkPipelineDynamicStateCreateFlags)0,						// VkPipelineDynamicStateCreateFlags	flags;
				m_data.useDynamicState ? 1u : 0u,							// uint32_t								dynamicStateCount;
				&dynamicState,												// const VkDynamicState*				pDynamicStates;
			};

			// Enable depth/stencil writes, always passing
			VkPipelineDepthStencilStateCreateInfo		depthStencilStateParams				=
			{
				VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
				DE_NULL,													// const void*								pNext;
				0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
				VK_TRUE,													// VkBool32									depthTestEnable;
				VK_TRUE,													// VkBool32									depthWriteEnable;
				VK_COMPARE_OP_ALWAYS,										// VkCompareOp								depthCompareOp;
				VK_FALSE,													// VkBool32									depthBoundsTestEnable;
				VK_TRUE,													// VkBool32									stencilTestEnable;
				// VkStencilOpState	front;
				{
					VK_STENCIL_OP_REPLACE,	// VkStencilOp	failOp;
					VK_STENCIL_OP_REPLACE,	// VkStencilOp	passOp;
					VK_STENCIL_OP_REPLACE,	// VkStencilOp	depthFailOp;
					VK_COMPARE_OP_ALWAYS,	// VkCompareOp	compareOp;
					0u,						// deUint32		compareMask;
					0xFFu,					// deUint32		writeMask;
					0xFFu,					// deUint32		reference;
				},
				// VkStencilOpState	back;
				{
					VK_STENCIL_OP_REPLACE,	// VkStencilOp	failOp;
					VK_STENCIL_OP_REPLACE,	// VkStencilOp	passOp;
					VK_STENCIL_OP_REPLACE,	// VkStencilOp	depthFailOp;
					VK_COMPARE_OP_ALWAYS,	// VkCompareOp	compareOp;
					0u,						// deUint32		compareMask;
					0xFFu,					// deUint32		writeMask;
					0xFFu,					// deUint32		reference;
				},
				0.0f,						// float			minDepthBounds;
				0.0f,						// float			maxDepthBounds;
			};

			VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo	=
			{
				VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
				&shadingRateStateCreateInfo,						// const void*										pNext;
				(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
				numStages,											// deUint32											stageCount;
				&shaderCreateInfo[0],								// const VkPipelineShaderStageCreateInfo*			pStages;
				&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
				&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
				DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
				&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
				&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
				&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
				&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
				&colorBlendStateCreateInfo,							// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
				&dynamicStateCreateInfo,							// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
				pipelineLayout.get(),								// VkPipelineLayout									layout;
				renderPass.get(),									// VkRenderPass										renderPass;
				0u,													// deUint32											subpass;
				DE_NULL,											// VkPipeline										basePipelineHandle;
				0													// int												basePipelineIndex;
			};

			if (m_data.useDynamicRendering)
				graphicsPipelineCreateInfo.flags |= VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

			VkImageMemoryBarrier imageBarrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
				DE_NULL,											// const void*			pNext
				0u,													// VkAccessFlags		srcAccessMask
				VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
				VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout		newLayout
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
				**cbImage,											// VkImage				image
				{
					VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
					0u,										// uint32_t				baseMipLevel
					VK_REMAINING_MIP_LEVELS,				// uint32_t				mipLevels,
					0u,										// uint32_t				baseArray
					VK_REMAINING_ARRAY_LAYERS,				// uint32_t				arraySize
				}
			};

			const VkQueue					queue					= m_context.getUniversalQueue();
			Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex());
			Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
			VkClearValue					clearColor				= makeClearValueColorU32(0, 0, 0, 0);
			VkClearValue					clearDepthStencil		= makeClearValueDepthStencil(0.0, 0);

			beginCommandBuffer(vk, *cmdBuffer, 0u);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
									(VkDependencyFlags)0,
									0, (const VkMemoryBarrier*)DE_NULL,
									0, (const VkBufferMemoryBarrier*)DE_NULL,
									1, &imageBarrier);

			imageBarrier.image = **derivImage;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
									(VkDependencyFlags)0,
									0, (const VkMemoryBarrier*)DE_NULL,
									0, (const VkBufferMemoryBarrier*)DE_NULL,
									1, &imageBarrier);

			// Clear level to 1<<level
			for (deUint32 i = 0; i < derivNumLevels; ++i)
			{
				VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, i, 1u, 0u, 1u);
				VkClearValue clearLevelColor = makeClearValueColorU32(1<<i,0,0,0);
				vk.cmdClearColorImage(*cmdBuffer, **derivImage, VK_IMAGE_LAYOUT_GENERAL, &clearLevelColor.color, 1, &range);
			}

			// Clear color buffer to transparent black
			{
				VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, VK_REMAINING_ARRAY_LAYERS);
				vk.cmdClearColorImage(*cmdBuffer, **cbImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);
			}

			// Clear depth and stencil
			if (m_data.useDepthStencil)
			{
				VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, VK_REMAINING_ARRAY_LAYERS);
				VkImageMemoryBarrier dsBarrier = imageBarrier;
				dsBarrier.image = **dsImage;
				dsBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				dsBarrier.subresourceRange = range;
				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
										0u, // dependencyFlags
										0u, nullptr,
										0u, nullptr,
										1u, &dsBarrier);
				vk.cmdClearDepthStencilImage(*cmdBuffer, **dsImage, VK_IMAGE_LAYOUT_GENERAL, &clearDepthStencil.depthStencil, 1, &range);
			}

			// Initialize shading rate image with varying values
			if (m_data.useAttachment())
			{
				imageBarrier.image = **srImage;
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
										(VkDependencyFlags)0,
										0, (const VkMemoryBarrier*)DE_NULL,
										0, (const VkBufferMemoryBarrier*)DE_NULL,
										1, &imageBarrier);

				deMemset(fillPtr, 0, (size_t)srFillBufferSize);
				for (deUint32 layer = 0; layer < numSRLayers; ++layer)
				{
					for (deUint32 x = 0; x < srWidth; ++x)
					{
						for (deUint32 y = 0; y < srHeight; ++y)
						{
							deUint32 idx = (layer*srHeight + y)*srWidth + x;
							deUint8 val = (deUint8)SanitizeRate(idx & 0xF);
							// actual shading rate is always in the LSBs of the first byte of a texel
							fillPtr[srFillBpp*idx] = val;
						}
					}
				}
				flushAlloc(vk, device, srFillBuffer->getAllocation());

				const VkBufferImageCopy				copyRegion							=
				{
					0u,																	// VkDeviceSize			bufferOffset;
					0u,																	// deUint32				bufferRowLength;
					0u,																	// deUint32				bufferImageHeight;
					{
						VK_IMAGE_ASPECT_COLOR_BIT,										// VkImageAspectFlags	aspect;
						0u,																// deUint32				mipLevel;
						0u,																// deUint32				baseArrayLayer;
						numSRLayers,													// deUint32				layerCount;
					},																	// VkImageSubresourceLayers imageSubresource;
					{ 0, 0, 0 },														// VkOffset3D			imageOffset;
					{ srWidth, srHeight, 1 },											// VkExtent3D			imageExtent;
				};

				vk.cmdCopyBufferToImage(*cmdBuffer, **srFillBuffer, **srImage, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);

				imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageBarrier.newLayout = srLayout;

				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
										(VkDependencyFlags)0,
										0, (const VkMemoryBarrier*)DE_NULL,
										0, (const VkBufferMemoryBarrier*)DE_NULL,
										1, &imageBarrier);
			}

			VkMemoryBarrier					memBarrier =
			{
				VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
				DE_NULL,							// pNext
				0u,									// srcAccessMask
				0u,									// dstAccessMask
			};

			memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, allPipelineStages,
				0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

			vector<Move<VkPipeline>> pipelines;

			// If using dynamic state, create a single graphics pipeline and bind it
			if (m_data.useDynamicState)
			{
				pipelines.push_back(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo));
				vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines[0]);
			}

			VkRect2D renderArea = makeRect2D(m_data.framebufferDim.width, m_data.framebufferDim.height);
			if (m_data.useDynamicRendering)
			{
				VkRenderingFragmentShadingRateAttachmentInfoKHR shadingRateAttachmentInfo
				{
					VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,	// VkStructureType		sType;
					DE_NULL,																// const void*			pNext;
					*srImageView,															// VkImageView			imageView;
					srLayout,																// VkImageLayout		imageLayout;
					{ srTexelWidth, srTexelHeight }											// VkExtent2D			shadingRateAttachmentTexelSize;
				};

				VkRenderingAttachmentInfoKHR colorAttachment
				{
					vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,					// VkStructureType						sType;
					DE_NULL,																// const void*							pNext;
					*cbImageView,															// VkImageView							imageView;
					VK_IMAGE_LAYOUT_GENERAL,												// VkImageLayout						imageLayout;
					VK_RESOLVE_MODE_NONE,													// VkResolveModeFlagBits				resolveMode;
					DE_NULL,																// VkImageView							resolveImageView;
					VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout						resolveImageLayout;
					VK_ATTACHMENT_LOAD_OP_LOAD,												// VkAttachmentLoadOp					loadOp;
					VK_ATTACHMENT_STORE_OP_STORE,											// VkAttachmentStoreOp					storeOp;
					clearColor																// VkClearValue							clearValue;
				};

				std::vector<VkRenderingAttachmentInfoKHR> depthStencilAttachments(2,
				{
					VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,						// VkStructureType						sType;
					DE_NULL,																// const void*							pNext;
					*dsImageView,															// VkImageView							imageView;
					VK_IMAGE_LAYOUT_GENERAL,												// VkImageLayout						imageLayout;
					VK_RESOLVE_MODE_NONE,													// VkResolveModeFlagBits				resolveMode;
					DE_NULL,																// VkImageView							resolveImageView;
					VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout						resolveImageLayout;
					VK_ATTACHMENT_LOAD_OP_LOAD,												// VkAttachmentLoadOp					loadOp;
					VK_ATTACHMENT_STORE_OP_STORE,											// VkAttachmentStoreOp					storeOp;
					clearDepthStencil														// VkClearValue							clearValue;
				});

				vk::VkRenderingInfoKHR renderingInfo
				{
					vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
					m_data.useAttachment() ? &shadingRateAttachmentInfo : DE_NULL,
					0,																		// VkRenderingFlagsKHR					flags;
					renderArea,																// VkRect2D								renderArea;
					m_data.multiView ? 1 : m_data.numColorLayers,							// deUint32								layerCount;
					m_data.multiView ? 0x3 : 0u,											// deUint32								viewMask;
					1u,																		// deUint32								colorAttachmentCount;
					&colorAttachment,														// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
					m_data.useDepthStencil ? &depthStencilAttachments[0] : DE_NULL,			// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
					m_data.useDepthStencil ? &depthStencilAttachments[1] : DE_NULL,			// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
				};

				vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
			}
			else
			{
				const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo
				{
					VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,		//  VkStructureType		sType;
					DE_NULL,													//  const void*			pNext;
					(deUint32)attachments.size(),								//  deUint32			attachmentCount;
					&attachments[0]												//  const VkImageView*	pAttachments;
				};

				beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea,
								0, DE_NULL, VK_SUBPASS_CONTENTS_INLINE, imagelessFB ? &renderPassAttachmentBeginInfo : DE_NULL);
			}

			for (deInt32 i = 0; i < NUM_TRIANGLES; ++i)
			{
				// Bind vertex attributes pointing to the next triangle
				VkDeviceSize vertexBufferOffset = i*3*2*sizeof(float);
				VkBuffer vb = **vertexBuffer;
				vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vb, &vertexBufferOffset);

				// Put primitive shading rate in a push constant
				deInt32 shadingRatePC = PrimIDToPrimitiveShadingRate(i);
				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages, 0, sizeof(shadingRatePC), &shadingRatePC);

				if (m_data.useDynamicState)
				{
					VkExtent2D fragmentSize = ShadingRateEnumToExtent(PrimIDToPipelineShadingRate(i));
					vk.cmdSetFragmentShadingRateKHR(*cmdBuffer, &fragmentSize, m_data.combinerOp);
				}
				else
				{
					// Create a new pipeline with the desired pipeline shading rate
					shadingRateStateCreateInfo.fragmentSize = ShadingRateEnumToExtent(PrimIDToPipelineShadingRate(i));
					pipelines.push_back(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo));
					vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines.back());
				}

				// Draw one triangle, with "primitive ID" in gl_InstanceIndex
				vk.cmdDraw(*cmdBuffer, 3u, 1, 0u, i);
			}

			if (m_data.useDynamicRendering)
				endRendering(vk, *cmdBuffer);
			else
				endRenderPass(vk, *cmdBuffer);

			memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			vk.cmdPipelineBarrier(*cmdBuffer, allPipelineStages, allPipelineStages,
				0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

			// Copy color/depth/stencil buffers to buffer memory
			vk.cmdDispatch(*cmdBuffer, m_data.framebufferDim.width, m_data.framebufferDim.height, m_data.numColorLayers);

			memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			memBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
				0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

			endCommandBuffer(vk, *cmdBuffer);

			submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

			deUint32 *colorptr = (deUint32 *)colorOutputBuffer->getAllocation().getHostPtr();
			invalidateAlloc(vk, device, colorOutputBuffer->getAllocation());

			invalidateAlloc(vk, device, atomicBuffer->getAllocation());

			float *depthptr = DE_NULL;
			deUint32 *stencilptr = DE_NULL;

			if (m_data.useDepthStencil)
			{
				depthptr = (float *)depthOutputBuffer->getAllocation().getHostPtr();
				invalidateAlloc(vk, device, depthOutputBuffer->getAllocation());

				stencilptr = (deUint32 *)stencilOutputBuffer->getAllocation().getHostPtr();
				invalidateAlloc(vk, device, stencilOutputBuffer->getAllocation());
			}

			// Loop over all samples and validate the output
			for (deUint32 layer = 0; layer < m_data.numColorLayers && res == QP_TEST_RESULT_PASS; ++layer)
			{
				for (deUint32 y = 0; y < m_data.framebufferDim.height && res == QP_TEST_RESULT_PASS; ++y)
				{
					for (deUint32 x = 0; x < m_data.framebufferDim.width && res == QP_TEST_RESULT_PASS; ++x)
					{
						for (deInt32 s = 0; s < m_data.samples && res == QP_TEST_RESULT_PASS; ++s)
						{
							deUint32 *sample = &colorptr[4*(((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + s)];

							// If testing the rasterizer sample mask, if this sample is not set in the
							// mask then it shouldn't have written anything.
							if (m_data.useApiSampleMask && !(sampleMask & (1 << s)) && sample[2] != 0)
							{
								log << tcu::TestLog::Message << std::hex << "sample written despite pSampleMask (" << x << "," << y << ",sample " << s << ")" << tcu::TestLog::EndMessage;
								res = QP_TEST_RESULT_FAIL;
								continue;
							}

							// The same isn't covered by any primitives, skip it
							if (sample[2] == 0)
								continue;

							// skip samples that have the same value as sample zero - it would be redundant to check them.
							if (s > 0)
							{
								deUint32 *sample0 = &colorptr[4*(((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + 0)];
								bool same = deMemCmp(sample, sample0, 16) == 0;

								if (m_data.fragDepth)
								{
									float *dsample = &depthptr[((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + s];
									float *dsample0 = &depthptr[((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + 0];
									same = same && (*dsample == *dsample0);
								}

								if (m_data.fragStencil)
								{
									deUint32 *ssample = &stencilptr[((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + s];
									deUint32 *ssample0 = &stencilptr[((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + 0];
									same = same && (*ssample == *ssample0);
								}

								if (same)
									continue;
							}

							// Fragment shader writes error codes to .w component.
							// All nonzero values are unconditionally failures
							if (sample[3] != 0)
							{
								if (sample[3] == ERROR_FRAGCOORD_CENTER)
									log << tcu::TestLog::Message << std::hex << "fragcoord test failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")" << tcu::TestLog::EndMessage;
								else if (sample[3] == ERROR_VTG_READBACK)
									log << tcu::TestLog::Message << std::hex << "vs/gs output readback test failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")" << tcu::TestLog::EndMessage;
								else if ((sample[3] & 0xFF) == ERROR_FRAGCOORD_DERIV)
									log << tcu::TestLog::Message << std::hex << "fragcoord derivative test failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")="
																				"(0x" << ((sample[3] >>  8) & 0x3F) << ",0x" << ((sample[3] >> 14) & 0x3F) << "), expected="
																				"(0x" << ((sample[3] >> 20) & 0x3F) << ",0x" << ((sample[3] >> 26) & 0x3F) << ")" << tcu::TestLog::EndMessage;
								else if ((sample[3] & 0xFF) == ERROR_FRAGCOORD_IMPLICIT_DERIV)
									log << tcu::TestLog::Message << std::hex << "implicit derivative test failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")="
																				"(0x" << ((sample[3] >>  8) & 0x3F) << ",0x" << ((sample[3] >> 14) & 0x3F) << "), expected="
																				"(0x" << ((sample[3] >> 20) & 0x3F) << ",0x" << ((sample[3] >> 26) & 0x3F) << ")" << tcu::TestLog::EndMessage;
								else
									log << tcu::TestLog::Message << std::hex << "w coord unknown test failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")" << tcu::TestLog::EndMessage;
								res = QP_TEST_RESULT_FAIL;
								continue;
							}

							// x component of sample
							deUint32 rate = sample[0];
							// fragment size
							deUint32 pixelsX = 1 << ((rate/4)&3);
							deUint32 pixelsY = 1 << (rate&3);

							// Fragment region
							deUint32 fragMinX = x & ~(pixelsX-1);
							deUint32 fragMinY = y & ~(pixelsY-1);
							deUint32 fragMaxX = fragMinX + pixelsX;
							deUint32 fragMaxY = fragMinY + pixelsY;

							// Clamp to FB dimension for odd sizes
							if (fragMaxX > m_data.framebufferDim.width)
								fragMaxX = m_data.framebufferDim.width;
							if (fragMaxY > m_data.framebufferDim.height)
								fragMaxY = m_data.framebufferDim.height;

							// z component of sample
							deUint32 primID = sample[2] >> 24;
							deUint32 atomVal = sample[2] & 0xFFFFFF;

							// Compute pipeline and primitive rate from primitive ID, and attachment
							// rate from the x/y coordinate
							deInt32 pipelineRate = PrimIDToPipelineShadingRate(primID);
							deInt32 primitiveRate = m_data.shaderWritesRate ? PrimIDToPrimitiveShadingRate(primID) : 0;

							deInt32 attachmentLayer = (m_data.srLayered && modeIdx == ATTACHMENT_MODE_2DARRAY) ? layer : 0;
							deInt32 attachmentRate = m_data.useAttachment() ? fillPtr[srFillBpp*((attachmentLayer * srHeight + (y / srTexelHeight)) * srWidth + (x / srTexelWidth))] : 0;

							// Get mask of allowed shading rates
							deInt32 expectedMasks = Simulate(pipelineRate, primitiveRate, attachmentRate);

							if (!(expectedMasks & (1 << rate)))
							{
								log << tcu::TestLog::Message << std::hex << "unexpected shading rate. failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ") "
																			"result rate 0x" << rate << " mask of expected rates 0x" << expectedMasks <<
																			" pipelineRate=0x" << pipelineRate << " primitiveRate=0x" << primitiveRate << " attachmentRate =0x" << attachmentRate << tcu::TestLog::EndMessage;
								res = QP_TEST_RESULT_FAIL;
								continue;
							}
							// Check that not all fragments are downgraded to 1x1
							if (rate == 0 && expectedMasks != 1)
								numUnexpected1x1Samples++;
							numTotalSamples++;

							// Check that gl_FragDepth = primID / NUM_TRIANGLES
							if (m_data.fragDepth)
							{
								float *dsample = &depthptr[((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + s];
								float expected = (float)primID / NUM_TRIANGLES;
								if (fabs(*dsample - expected) > 0.01)
								{
									log << tcu::TestLog::Message << std::hex << "depth write failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")=" << *dsample << " expected " << expected << tcu::TestLog::EndMessage;
									res = QP_TEST_RESULT_FAIL;
									continue;
								}
							}

							// Check that stencil value = primID
							if (m_data.fragStencil)
							{
								deUint32 *ssample = &stencilptr[((layer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + s];
								if (*ssample != primID)
								{
									log << tcu::TestLog::Message << std::hex << "stencil write failed pixel (0x" << x << ",0x" << y << ",sample 0x" << s << ")=" << *ssample << " expected " << primID << tcu::TestLog::EndMessage;
									res = QP_TEST_RESULT_FAIL;
									continue;
								}
							}

							// Check that primitives are in the right viewport/scissor
							if (m_data.multiViewport)
							{
								VkRect2D *scissor = &scissors[primID & 1];
								if ((int)x < scissor->offset.x || (int)x >= (int)(scissor->offset.x + scissor->extent.width) ||
									(int)y < scissor->offset.y || (int)y >= (int)(scissor->offset.y + scissor->extent.height))
								{
									log << tcu::TestLog::Message << std::hex << "primitive found outside of expected viewport (0x" << x << ",0x" << y << ",sample 0x" << s << ") primID=" << primID << tcu::TestLog::EndMessage;
									res = QP_TEST_RESULT_FAIL;
									continue;
								}
							}

							// Check that primitives are in the right layer
							if (m_data.colorLayered)
							{
								if (layer != ((primID & 2)>>1))
								{
									log << tcu::TestLog::Message << std::hex << "primitive found in wrong layer (0x" << x << ",0x" << y << ",sample 0x" << s << ") primID=" << primID << " layer=" << layer << tcu::TestLog::EndMessage;
									res = QP_TEST_RESULT_FAIL;
									continue;
								}
							}

							// Check that multiview broadcasts the same primitive to both layers
							if (m_data.multiView)
							{
								deUint32 otherLayer = layer^1;
								deUint32 *othersample = &colorptr[4*(((otherLayer * m_data.framebufferDim.height + y) * m_data.framebufferDim.width + x)*m_data.samples + s)];
								deUint32 otherPrimID = othersample[2] >> 24;
								if (primID != otherPrimID)
								{
									log << tcu::TestLog::Message << std::hex << "multiview primitive mismatch (0x" << x << ",0x" << y << ",sample 0x" << s << ") primID=" << primID << "  otherPrimID=" << otherPrimID << tcu::TestLog::EndMessage;
									res = QP_TEST_RESULT_FAIL;
									continue;
								}
							}

							// Loop over all samples in the same fragment
							for (deUint32 fx = fragMinX; fx < fragMaxX; ++fx)
							{
								for (deUint32 fy = fragMinY; fy < fragMaxY; ++fy)
								{
									for (deInt32 fs = 0; fs < m_data.samples; ++fs)
									{
										deUint32 *fsample = &colorptr[4*(((layer * m_data.framebufferDim.height + fy) * m_data.framebufferDim.width + fx)*m_data.samples + fs)];
										deUint32 frate = fsample[0];
										deUint32 fprimID = fsample[2] >> 24;
										deUint32 fatomVal = fsample[2] & 0xFFFFFF;

										// If we write out the sample mask value, check that the samples in the
										// mask must not be uncovered, and that samples not in the mask must not
										// be covered by this primitive
										if (m_data.useSampleMaskIn)
										{
											int p = pixelsX * pixelsY - ((fx - fragMinX) + pixelsX * (fy - fragMinY)) - 1;
											int sampleIdx = fs + m_data.samples * p;

											if ((sample[1] & (1 << sampleIdx)) && fsample[2] == 0)
											{
												log << tcu::TestLog::Message << std::hex << "sample set in sampleMask but not written (0x" << fx << ",0x" << fy << ",sample 0x" << fs << ")" << tcu::TestLog::EndMessage;
												res = QP_TEST_RESULT_FAIL;
												continue;
											}
											if (!(sample[1] & (1 << sampleIdx)) && fsample[2] != 0 && fprimID == primID)
											{
												log << tcu::TestLog::Message << std::hex << "sample not set in sampleMask but written with same primID (0x" << fx << ",0x" << fy << ",sample 0x" << fs << ")" << tcu::TestLog::EndMessage;
												res = QP_TEST_RESULT_FAIL;
												continue;
											}
										}

										// If conservative raster is enabled, or custom sample locations all at the center, check that
										// samples in the same pixel must be covered.
										if (m_data.conservativeEnable ||
											(m_data.sampleLocations && m_context.getFragmentShadingRateProperties().fragmentShadingRateWithCustomSampleLocations))
										{
											// If it's in the same pixel, expect it to be fully covered.
											if (fx == x && fy == y && fsample[2] == 0)
											{
												log << tcu::TestLog::Message << std::hex << "pixel not fully covered (0x" << fx << ",0x" << fy << ",sample 0x" << fs << ")" << tcu::TestLog::EndMessage;
												res = QP_TEST_RESULT_FAIL;
												continue;
											}
										}

										if (fsample[2] == 0)
											continue;

										// If the primitive matches this sample, then it must have the same rate and
										// atomic value
										if (fprimID == primID)
										{
											if (rate != frate || (atomVal != fatomVal && !(m_data.sampleShadingEnable || m_data.sampleShadingInput)))
											{
												log << tcu::TestLog::Message << std::hex << "failed pixel (0x" << x << ",0x" << y << ",sample " << s << ")=0x" << ((primID<<24)|atomVal) <<
																							" compared to (0x" << fx << ",0x" << fy << ",sample " << fs << ")=0x" << ((fprimID<<24)|fatomVal) <<
																							" pipelineRate=0x" << pipelineRate << " primitiveRate=0x" << primitiveRate << " attachmentRate =0x" << attachmentRate <<
																							tcu::TestLog::EndMessage;
												res = QP_TEST_RESULT_FAIL;
											}
										}
									}
								}
							}
						}
					}
				}
			}
			if (res == QP_TEST_RESULT_FAIL)
				break;
		}
	}
	// All samples were coerced to 1x1, unexpected
	if (res == QP_TEST_RESULT_PASS &&
		numTotalSamples != 0 &&
		numUnexpected1x1Samples == numTotalSamples &&
		numTotalSamples > 16)
	{
		log << tcu::TestLog::Message << std::hex << "Quality warning - all fragments used 1x1" << tcu::TestLog::EndMessage;
		res = QP_TEST_RESULT_QUALITY_WARNING;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

}	// anonymous

void createBasicTests (tcu::TestContext& testCtx, tcu::TestCaseGroup* parentGroup, bool useDynamicRendering)
{
	typedef struct
	{
		deUint32				count;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	typedef struct
	{
		VkExtent2D				count;
		const char*				name;
		const char*				description;
	} TestGroupCase2D;

	typedef struct
	{
		AttachmentUsage			usage;
		const char*				name;
		const char*				description;
	} TestGroupUsageCase;

	TestGroupCase groupCases[] =
	{
		{ 0,	"basic",				"basic tests"					},
		{ 1,	"apisamplemask",		"use pSampleMask"				},
		{ 2,	"samplemaskin",			"use gl_SampleMaskIn"			},
		{ 3,	"conservativeunder",	"conservative underestimation"	},
		{ 4,	"conservativeover",		"conservative overestimation"	},
		{ 5,	"fragdepth",			"depth shader output"			},
		{ 6,	"fragstencil",			"stencil shader output"			},
		{ 7,	"multiviewport",		"multiple viewports and gl_ViewportIndex"	},
		{ 8,	"colorlayered",			"multiple layer color, single layer shading rate"	},
		{ 9,	"srlayered",			"multiple layer color, multiple layers shading rate"	},
		{ 10,	"multiview",			"multiview"	},
		{ 11,	"multiviewsrlayered",	"multiview and multilayer shading rate"	},
		{ 12,	"interlock",			"fragment shader interlock"	},
		{ 13,	"samplelocations",		"custom sample locations"	},
		{ 14,	"sampleshadingenable",	"enable sample shading in createinfo"	},
		{ 15,	"sampleshadinginput",	"enable sample shading by using gl_SampleID"	},
	};

	TestGroupCase dynCases[] =
	{
		{ 1,	"dynamic",	"uses dynamic shading rate state"	},
		{ 0,	"static",	"uses static shading rate state"	},
	};

	TestGroupUsageCase attCases[] =
	{
		{ AttachmentUsage::NO_ATTACHMENT,		"noattachment",		"no shading rate attachment"			},
		{ AttachmentUsage::WITH_ATTACHMENT,		"attachment",		"has shading rate attachment"			},
		{ AttachmentUsage::NO_ATTACHMENT_PTR,	"noattachmentptr",	"no shading rate attachment pointer"	},
	};

	TestGroupCase shdCases[] =
	{
		{ 0,	"noshaderrate",	"shader doesn't write rate"	},
		{ 1,	"shaderrate",	"shader writes rate"	},
	};

	TestGroupCase combCases[] =
	{
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,	"keep",		"keep"	},
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR,	"replace",	"replace"	},
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR,		"min",		"min"	},
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR,		"max",		"max"	},
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR,		"mul",		"mul"	},
	};

	TestGroupCase2D extentCases[] =
	{
		{ {1,   1},		"1x1",		"1x1"		},
		{ {4,   4},		"4x4",		"4x4"		},
		{ {33,  35},	"33x35",	"33x35"		},
		{ {151, 431},	"151x431",	"151x431"	},
		{ {256, 256},	"256x256",	"256x256"	},
	};

	TestGroupCase sampCases[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT,	"samples1",		"1 raster sample"	},
		{ VK_SAMPLE_COUNT_2_BIT,	"samples2",		"2 raster samples"	},
		{ VK_SAMPLE_COUNT_4_BIT,	"samples4",		"4 raster samples"	},
		{ VK_SAMPLE_COUNT_8_BIT,	"samples8",		"8 raster samples"	},
		{ VK_SAMPLE_COUNT_16_BIT,	"samples16",	"16 raster samples"	},
	};

	TestGroupCase geomCases[] =
	{
		{ 0,	"vs",	"vertex shader only"	},
		{ 1,	"gs",	"vertex and geometry shader"	},
	};

	deInt32 seed = 0;

	for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groupCases); groupNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, groupCases[groupNdx].name, groupCases[groupNdx].description));
		for (int dynNdx = 0; dynNdx < DE_LENGTH_OF_ARRAY(dynCases); dynNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> dynGroup(new tcu::TestCaseGroup(testCtx, dynCases[dynNdx].name, dynCases[dynNdx].description));
			for (int attNdx = 0; attNdx < DE_LENGTH_OF_ARRAY(attCases); attNdx++)
			{
				if (useDynamicRendering && attCases[attNdx].usage == AttachmentUsage::NO_ATTACHMENT_PTR)
					continue;

				de::MovePtr<tcu::TestCaseGroup> attGroup(new tcu::TestCaseGroup(testCtx, attCases[attNdx].name, attCases[attNdx].description));
				for (int shdNdx = 0; shdNdx < DE_LENGTH_OF_ARRAY(shdCases); shdNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> shdGroup(new tcu::TestCaseGroup(testCtx, shdCases[shdNdx].name, shdCases[shdNdx].description));
					for (int cmb0Ndx = 0; cmb0Ndx < DE_LENGTH_OF_ARRAY(combCases); cmb0Ndx++)
					{
						de::MovePtr<tcu::TestCaseGroup> cmb0Group(new tcu::TestCaseGroup(testCtx, combCases[cmb0Ndx].name, combCases[cmb0Ndx].description));
						for (int cmb1Ndx = 0; cmb1Ndx < DE_LENGTH_OF_ARRAY(combCases); cmb1Ndx++)
						{
							de::MovePtr<tcu::TestCaseGroup> cmb1Group(new tcu::TestCaseGroup(testCtx, combCases[cmb1Ndx].name, combCases[cmb1Ndx].description));
							for (int extNdx = 0; extNdx < DE_LENGTH_OF_ARRAY(extentCases); extNdx++)
							{
								de::MovePtr<tcu::TestCaseGroup> extGroup(new tcu::TestCaseGroup(testCtx, extentCases[extNdx].name, extentCases[extNdx].description));
								for (int sampNdx = 0; sampNdx < DE_LENGTH_OF_ARRAY(sampCases); sampNdx++)
								{
									de::MovePtr<tcu::TestCaseGroup> sampGroup(new tcu::TestCaseGroup(testCtx, sampCases[sampNdx].name, sampCases[sampNdx].description));
									for (int geomNdx = 0; geomNdx < DE_LENGTH_OF_ARRAY(geomCases); geomNdx++)
									{
										bool useApiSampleMask = groupNdx == 1;
										bool useSampleMaskIn = groupNdx == 2;
										bool consRast = groupNdx == 3 || groupNdx == 4;
										bool fragDepth = groupNdx == 5;
										bool fragStencil = groupNdx == 6;
										bool multiViewport = groupNdx == 7;
										bool colorLayered = groupNdx == 8 || groupNdx == 9;
										bool srLayered = groupNdx == 9 || groupNdx == 11;
										bool multiView = groupNdx == 10 || groupNdx == 11;
										bool interlock = groupNdx == 12;
										bool sampleLocations = groupNdx == 13;
										bool sampleShadingEnable = groupNdx == 14;
										bool sampleShadingInput = groupNdx == 15;
										VkConservativeRasterizationModeEXT conservativeMode = (groupNdx == 3) ? VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT : VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
										deUint32 numColorLayers = (colorLayered || multiView) ? 2u : 1u;

										// Don't bother with geometry shader if we're not testing shader writes
										if (geomCases[geomNdx].count && !shdCases[shdNdx].count)
											continue;

										// reduce number of tests
										if ((groupNdx != 0) &&
											(!dynCases[dynNdx].count ||
											 !(combCases[cmb0Ndx].count == VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR || combCases[cmb0Ndx].count == VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR) ||
											 !(combCases[cmb1Ndx].count == VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR || combCases[cmb1Ndx].count == VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR)))
											continue;

										// Don't bother with geometry shader if we're testing conservative raster, sample mask, depth/stencil
										if (geomCases[geomNdx].count && (useApiSampleMask || useSampleMaskIn || consRast || fragDepth || fragStencil))
											continue;

										// Don't bother with geometry shader if we're testing non-dynamic state
										if (geomCases[geomNdx].count && !dynCases[dynNdx].count)
											continue;

										// Only test multiViewport/layered with shaderWritesRate
										if ((multiViewport || colorLayered) && !shdCases[shdNdx].count)
											continue;

										// Can't test layered shading rate attachment without an attachment
										if (srLayered && attCases[attNdx].usage != AttachmentUsage::WITH_ATTACHMENT)
											continue;

										CaseDef c =
										{
											seed++,													// deInt32 seed;
											extentCases[extNdx].count,								// VkExtent2D framebufferDim;
											(VkSampleCountFlagBits)sampCases[sampNdx].count,		// VkSampleCountFlagBits samples;
											{
												(VkFragmentShadingRateCombinerOpKHR)combCases[cmb0Ndx].count,
												(VkFragmentShadingRateCombinerOpKHR)combCases[cmb1Ndx].count
											},														// VkFragmentShadingRateCombinerOpKHR combinerOp[2];
											attCases[attNdx].usage,									// AttachmentUsage attachmentUsage;
											(bool)shdCases[shdNdx].count,							// bool shaderWritesRate;
											(bool)geomCases[geomNdx].count,							// bool geometryShader;
											(bool)dynCases[dynNdx].count,							// bool useDynamicState;
											useDynamicRendering,									// bool useDynamicRendering;
											useApiSampleMask,										// bool useApiSampleMask;
											useSampleMaskIn,										// bool useSampleMaskIn;
											consRast,												// bool conservativeEnable;
											conservativeMode,										// VkConservativeRasterizationModeEXT conservativeMode;
											fragDepth || fragStencil,								// bool useDepthStencil;
											fragDepth,												// bool fragDepth;
											fragStencil,											// bool fragStencil;
											multiViewport,											// bool multiViewport;
											colorLayered,											// bool colorLayered;
											srLayered,												// bool srLayered;
											numColorLayers,											// deUint32 numColorLayers;
											multiView,												// bool multiView;
											interlock,												// bool interlock;
											sampleLocations,										// bool sampleLocations;
											sampleShadingEnable,									// bool sampleShadingEnable;
											sampleShadingInput,										// bool sampleShadingInput;
											false,													// bool sampleMaskTest;
										};

										sampGroup->addChild(new FSRTestCase(testCtx, geomCases[geomNdx].name, geomCases[geomNdx].description, c));
									}
									extGroup->addChild(sampGroup.release());
								}
								cmb1Group->addChild(extGroup.release());
							}
							cmb0Group->addChild(cmb1Group.release());
						}
						shdGroup->addChild(cmb0Group.release());
					}
					attGroup->addChild(shdGroup.release());
				}
				dynGroup->addChild(attGroup.release());
			}
			group->addChild(dynGroup.release());
		}
		parentGroup->addChild(group.release());
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "misc_tests", "Single tests that don't need to be part of above test matrix"));
	group->addChild(new FSRTestCase(testCtx, "sample_mask_test", "", {
		123,													// deInt32 seed;
		{32,  33},												// VkExtent2D framebufferDim;
		VK_SAMPLE_COUNT_4_BIT,									// VkSampleCountFlagBits samples;
		{
			VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
			VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR
		},														// VkFragmentShadingRateCombinerOpKHR combinerOp[2];
		AttachmentUsage::NO_ATTACHMENT,							// AttachmentUsage attachmentUsage;
		true,													// bool shaderWritesRate;
		false,													// bool geometryShader;
		false,													// bool useDynamicState;
		false,													// bool useDynamicRendering;
		true,													// bool useApiSampleMask;
		false,													// bool useSampleMaskIn;
		false,													// bool conservativeEnable;
		VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT,	// VkConservativeRasterizationModeEXT conservativeMode;
		false,													// bool useDepthStencil;
		false,													// bool fragDepth;
		false,													// bool fragStencil;
		false,													// bool multiViewport;
		false,													// bool colorLayered;
		false,													// bool srLayered;
		1u,														// deUint32 numColorLayers;
		false,													// bool multiView;
		false,													// bool interlock;
		false,													// bool sampleLocations;
		false,													// bool sampleShadingEnable;
		false,													// bool sampleShadingInput;
		true,													// bool sampleMaskTest;
	}));

	parentGroup->addChild(group.release());
}

}	// FragmentShadingRage
}	// vkt
