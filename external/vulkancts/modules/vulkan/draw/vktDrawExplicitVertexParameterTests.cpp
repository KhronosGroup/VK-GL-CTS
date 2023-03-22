/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief VK_AMD_shader_explicit_vertex_parameter tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawExplicitVertexParameterTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deDefs.h"
#include "deRandom.hpp"
#include "deString.h"
#include "deMath.h"

#include "tcuTestCase.hpp"
#include "tcuRGBA.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"

#include "rrRenderer.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;
using namespace std;

enum Interpolation
{
	SMOOTH = 0,
	NOPERSPECTIVE = 1,
};

enum AuxiliaryQualifier
{
	AUX_NONE = 0,
	AUX_CENTROID = 1,
	AUX_SAMPLE = 2,
};


enum
{
	WIDTH = 16,
	HEIGHT = 16
};

struct PositionValueVertex {
	PositionValueVertex(tcu::Vec4 pos, float val)
	: position(pos)
	, value(val)
	{}
public:
	tcu::Vec4	position;
	float		value;
};

struct DrawParams
{
	Interpolation				interpolation;
	vk::VkSampleCountFlagBits	samples;
	AuxiliaryQualifier			auxiliaryStorage;
	const SharedGroupParams		groupParams;
};

const char* interpolationToString (Interpolation interpolation)
{
	switch (interpolation)
	{
		case SMOOTH:
			return "smooth";
		case NOPERSPECTIVE:
			return "noperspective";
		default:
			DE_FATAL("Invalid interpolation enum");
	}

	return "";
}

std::string barycentricVariableString (Interpolation interpolation, AuxiliaryQualifier aux)
{
	std::ostringstream name;
	name << "gl_BaryCoord";
	switch (interpolation)
	{
		case SMOOTH:
			name << "Smooth";
			break;
		case NOPERSPECTIVE:
			name << "NoPersp";
			break;
		default:
			DE_FATAL("Invalid interpolation enum");
	}

	switch (aux)
	{
		case AUX_CENTROID:
			name << "Centroid";
			break;
		case AUX_SAMPLE:
			name << "Sample";
			break;
		case AUX_NONE:
			name << "";
			break;
		default:
			DE_FATAL("Invalid auxiliary storage qualifier enum");
	}
	name << "AMD";
	return name.str();
}

const char* auxiliaryQualifierToString (AuxiliaryQualifier aux)
{
	switch (aux)
	{
		case AUX_CENTROID:
			return "centroid";
		case AUX_SAMPLE:
			return "sample";
		case AUX_NONE:
			return "";
		default:
			DE_FATAL("Invalid auxiliary storage qualifier enum");
	}

	return "";
}

std::string getTestName (DrawParams params)
{
	std::ostringstream	name;

	name << interpolationToString(params.interpolation) << "_";

	if (params.auxiliaryStorage != AUX_NONE)
		name << auxiliaryQualifierToString(params.auxiliaryStorage) << "_";

	name << "samples_" << de::toString(params.samples);

	return name.str();
}

class DrawTestInstance : public TestInstance
{
public:
						DrawTestInstance		(Context& context, const DrawParams& data);
						~DrawTestInstance		(void);
	tcu::TestStatus		iterate					(void);

protected:
	void				preRenderCommands		(VkCommandBuffer cmdBuffer, VkImage colorTargetImage) const;
	void				beginRenderPass			(VkCommandBuffer cmdBuffer, VkRect2D renderArea,
												 const VkClearValue* pClearValues, deUint32 clearValueCount) const;
	void				drawCommands			(VkCommandBuffer cmdBuffer, VkBuffer vertexBuffer) const;

#ifndef CTS_USES_VULKANSC
	void				beginSecondaryCmdBuffer	(VkCommandBuffer cmdBuffer, VkFormat colorFormat,
												 VkRenderingFlagsKHR renderingFlags = 0u) const;
	void				beginDynamicRender		(VkCommandBuffer cmdBuffer, VkRect2D renderArea,
												 const VkClearValue* pClearValues, VkRenderingFlagsKHR renderingFlags = 0u) const;
#endif // CTS_USES_VULKANSC

private:
	DrawParams						m_data;
	Move<VkRenderPass>				m_renderPass;
	Move<VkImageView>				m_colorTargetView;
	Move<VkImageView>				m_multisampleTargetView;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkPipeline>				m_pipeline;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
};

DrawTestInstance::DrawTestInstance (Context& context, const DrawParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

DrawTestInstance::~DrawTestInstance (void)
{
}

class DrawTestCase : public TestCase
{
	public:
								DrawTestCase		(tcu::TestContext& context, const char* name, const char* desc, const DrawParams data);
								~DrawTestCase		(void);
	virtual	void				initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance		(Context& context) const;
	virtual void				checkSupport		(Context& context) const;

private:
	DrawParams					m_data;
};

DrawTestCase::DrawTestCase (tcu::TestContext& context, const char* name, const char* desc, const DrawParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

DrawTestCase::~DrawTestCase	(void)
{
}

void DrawTestCase::checkSupport(Context &context) const
{
	context.requireDeviceFunctionality("VK_AMD_shader_explicit_vertex_parameter");

	if ((context.getDeviceProperties().limits.framebufferColorSampleCounts & m_data.samples) == 0)
		TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");

	if (m_data.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

void DrawTestCase::initPrograms (SourceCollections& programCollection) const
{
	const deUint32				numValues	= WIDTH * HEIGHT * m_data.samples;

	const tcu::StringTemplate	vertShader	(string(
		"#version 450\n"
		"#extension GL_AMD_shader_explicit_vertex_parameter : require\n"
		"\n"
		"layout(location = 0) in vec4 in_position;\n"
		"layout(location = 1) in float in_data;\n"
		"layout(location = 0) __explicitInterpAMD out float out_data_explicit;\n"
		"layout(location = 1) ${auxqualifier} ${qualifier}        out float out_data_${qualifier};\n"
		"\n"
		"out gl_PerVertex {\n"
		"    vec4  gl_Position;\n"
		"    float gl_PointSize;\n"
		"};\n"
		"\n"
		"void main() {\n"
		"    gl_PointSize              = 1.0;\n"
		"    gl_Position               = in_position;\n"
		"    out_data_explicit         = in_data;\n"
		"    out_data_${qualifier}     = in_data;\n"
		"}\n"));

	const tcu::StringTemplate	fragShader	(string(
		"#version 450\n"
		"#extension GL_AMD_shader_explicit_vertex_parameter : require\n"
		"\n"
		"layout(location = 0) __explicitInterpAMD in float in_data_explicit;\n"
		"layout(location = 1) ${auxqualifier} ${qualifier}        in float in_data_${qualifier};\n"
		"layout(location = 0) out vec4 out_color;\n"
		"layout (binding = 0, std140) writeonly buffer Output {\n"
		"    vec4 values [${numValues}];\n"
		"} sb_out;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    uint index = (uint(gl_FragCoord.y) * ${width} * ${samples}) + uint(gl_FragCoord.x) * ${samples} + gl_SampleID;\n"
		"    // Barycentric coodinates (I, J, K)\n"
		"    vec3 bary_coord = vec3(${barycoord}.x, ${barycoord}.y, 1.0f - ${barycoord}.x - ${barycoord}.y);\n"
		"\n"
		"    // Vertex 0 -> (I = 0, J = 0, K = 1)\n"
		"    float data0 = interpolateAtVertexAMD(in_data_explicit, 0);\n"
		"    // Vertex 1 -> (I = 1, J = 0, K = 0)\n"
		"    float data1 = interpolateAtVertexAMD(in_data_explicit, 1);\n"
		"    // Vertex 1 -> (I = 0, J = 1, K = 0)\n"
		"    float data2 = interpolateAtVertexAMD(in_data_explicit, 2);\n"
		"    // Match data component with barycentric coordinate\n"
		"    vec3  data  = vec3(data1, data2, data0);\n"
		"\n"
		"    float res      = (bary_coord.x * data.x) + (bary_coord.y * data.y) + (bary_coord.z * data.z);\n"
		"    float expected = in_data_${qualifier};\n"
		"\n"
		"    sb_out.values[ index ] = vec4(expected, res, 0u, 0u);\n"
		"\n"
		"    const float threshold = 0.0005f;\n"
		"    if (abs(res - expected) < threshold)\n"
		"        out_color = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"
		"    else\n"
		"        out_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
		"}\n"));

	map<string, string> attributes;
	attributes["width"]			= de::toString(WIDTH);
	attributes["numValues"]		= de::toString(numValues * m_data.samples);
	attributes["qualifier"]		= interpolationToString(m_data.interpolation);
	attributes["auxqualifier"]	= auxiliaryQualifierToString(m_data.auxiliaryStorage);
	attributes["barycoord"]		= barycentricVariableString(m_data.interpolation, m_data.auxiliaryStorage);
	attributes["samples"]		= de::toString(m_data.samples);

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader.specialize(attributes));
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader.specialize(attributes));
}

TestInstance* DrawTestCase::createInstance (Context& context) const
{
	return new DrawTestInstance(context, m_data);
}

tcu::TestStatus DrawTestInstance::iterate (void)
{
	de::SharedPtr<Image>			colorTargetImage;
	de::SharedPtr<Image>			multisampleTargetImage;
	tcu::TestLog					&log					= m_context.getTestContext().getLog();

	// Run two iterations with shaders that have different interpolation decorations. Images should still match.
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	const CmdPoolCreateInfo			cmdPoolCreateInfo		(m_context.getUniversalQueueFamilyIndex());
	Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, &cmdPoolCreateInfo);
	Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkCommandBuffer>			secCmdBuffer;
	const Unique<VkShaderModule>	vs						(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>	fs						(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));
	de::SharedPtr<Buffer>			vertexBuffer;
	de::SharedPtr<Buffer>			ssboBuffer;

	vk::VkFormat					imageFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32					numValues				= WIDTH * HEIGHT * m_data.samples;
	const deBool					useMultisampling		= m_data.samples != VK_SAMPLE_COUNT_1_BIT;

	// Create color buffer images.
	{
		const VkExtent3D			targetImageExtent		= { WIDTH, HEIGHT, 1 };
		const ImageCreateInfo		targetImageCreateInfo	(VK_IMAGE_TYPE_2D, imageFormat, targetImageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT,
															 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		colorTargetImage									= Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

		if (useMultisampling)
		{
			const ImageCreateInfo		multisampleTargetImageCreateInfo	(VK_IMAGE_TYPE_2D, imageFormat, targetImageExtent, 1, 1, m_data.samples,
																			 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
			multisampleTargetImage											= Image::createAndAlloc(vk, device, multisampleTargetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());
		}
	}

	const ImageViewCreateInfo colorTargetViewInfo(colorTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, imageFormat);
	m_colorTargetView = createImageView(vk, device, &colorTargetViewInfo);

	if (useMultisampling)
	{
		const ImageViewCreateInfo multisamplingTargetViewInfo(multisampleTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, imageFormat);
		m_multisampleTargetView = createImageView(vk, device, &multisamplingTargetViewInfo);
	}

	// Create render pass
	if (!m_data.groupParams->useDynamicRendering)
	{
		RenderPassCreateInfo			renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(imageFormat,
																 VK_SAMPLE_COUNT_1_BIT,
																 VK_ATTACHMENT_LOAD_OP_CLEAR,
																 VK_ATTACHMENT_STORE_OP_STORE,
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 VK_ATTACHMENT_STORE_OP_STORE,
																 VK_IMAGE_LAYOUT_UNDEFINED,
																 VK_IMAGE_LAYOUT_GENERAL));

		const VkAttachmentReference		colorAttachmentRef			= { 0u, VK_IMAGE_LAYOUT_GENERAL };
		const VkAttachmentReference		multisampleAttachmentRef	= { 1u, VK_IMAGE_LAYOUT_GENERAL };

		if (useMultisampling)
		{
			renderPassCreateInfo.addAttachment(AttachmentDescription(imageFormat,
																	 m_data.samples,
																	 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,
																	 vk::VK_ATTACHMENT_STORE_OP_STORE,
																	 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																	 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,
																	 vk::VK_IMAGE_LAYOUT_UNDEFINED,
																	 vk::VK_IMAGE_LAYOUT_GENERAL));
		}

		renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
														   0,
														   0,
														   DE_NULL,
														   1u,
														   useMultisampling ? &multisampleAttachmentRef : &colorAttachmentRef,
														   useMultisampling ? &colorAttachmentRef : DE_NULL,
														   AttachmentReference(),
														   0,
														   DE_NULL));

		m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

		// Create framebuffer
		vector<VkImageView> colorAttachments { *m_colorTargetView };
		if (useMultisampling)
			colorAttachments.push_back(*m_multisampleTargetView);

		const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, colorAttachments, WIDTH, HEIGHT, 1);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	// Create vertex buffer.
	{
		const PositionValueVertex	vertices[]	=
		{
			PositionValueVertex(
				tcu::Vec4(-1.0f, 1.0f, 0.5f, 1.0f),		// Coord
				float(1.0f)),							// Value

			PositionValueVertex(
				tcu::Vec4(-1.0f, -1.0f, 0.25f, 0.75f),	// Coord
				float(0.0f)),							// Value
			PositionValueVertex(
				tcu::Vec4( 1.0f,  1.0f, 0.0f, 2.0f),	// Coord
				float(0.5f)),							// Value
			PositionValueVertex(
				tcu::Vec4( 1.0f, -1.0f, 1.0f, 0.5f),	// Coord
				float(1.0f)),							// Value
		};

		const VkDeviceSize			dataSize	= DE_LENGTH_OF_ARRAY(vertices) * sizeof(PositionValueVertex);
		vertexBuffer							= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
		deUint8*					ptr			= reinterpret_cast<deUint8*>(vertexBuffer->getBoundMemory().getHostPtr());

		deMemcpy(ptr, vertices, static_cast<size_t>(dataSize));
		flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(), vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}

	// Create SSBO buffer
	{
		const VkDeviceSize		dataSize	= sizeof(tcu::Vec4) * numValues;
		ssboBuffer							= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
		deUint8*				ptr			= reinterpret_cast<deUint8*>(ssboBuffer->getBoundMemory().getHostPtr());

		deMemset(ptr, 0, static_cast<size_t>(dataSize));
		flushMappedMemoryRange(vk, device, ssboBuffer->getBoundMemory().getMemory(), ssboBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}

	// Create Descriptor Set layout
	{
		m_descriptorSetLayout = DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vk, device);
	}

	// Create Descriptor Set
	{
		m_descriptorPool = DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

		const VkDescriptorBufferInfo	bufferInfo =
		{
			ssboBuffer->object(),		// VkBuffer		buffer;
			0u,							// VkDeviceSize	offset;
			VK_WHOLE_SIZE				// VkDeviceSize	range;
		};

		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo)
			.update(vk, device);
	}

	// Create pipeline
	{
		const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

		VkViewport	viewport	= makeViewport(WIDTH, HEIGHT);
		VkRect2D	scissor		= makeRect2D(WIDTH, HEIGHT);

		const VkVertexInputBindingDescription vertexInputBindingDescription = { 0, (deUint32)(sizeof(tcu::Vec4) + sizeof(float)), VK_VERTEX_INPUT_RATE_VERTEX };

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{ 0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
			{ 1u, 0u, vk::VK_FORMAT_R32_SFLOAT, (deUint32)(sizeof(float)* 4) }
		};

		PipelineCreateInfo::VertexInputState vertexInputState	= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription, 2, vertexInputAttributeDescriptions);

		m_pipelineLayout = makePipelineLayout	(vk, device, *m_descriptorSetLayout);

		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, vector<VkViewport>(1, viewport), vector<VkRect2D>(1, scissor)));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState(m_data.samples));

#ifndef CTS_USES_VULKANSC
		VkPipelineRenderingCreateInfoKHR renderingCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			DE_NULL,
			0u,
			1u,
			&imageFormat,
			VK_FORMAT_UNDEFINED,
			VK_FORMAT_UNDEFINED
		};

		if (m_data.groupParams->useDynamicRendering)
			pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

		m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
	}

	// Queue draw and read results.
	{
		const VkQueue				queue				= m_context.getUniversalQueue();
		const ImageSubresourceRange subresourceRange	(VK_IMAGE_ASPECT_COLOR_BIT);
		const tcu::Vec4				clearColor			= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const VkRect2D				renderArea			= makeRect2D(WIDTH, HEIGHT);
		const VkBuffer				buffer				= vertexBuffer->object();

		vector<VkClearValue>		clearColors;
		clearColors.push_back(makeClearValueColor(clearColor));
		if (useMultisampling)
			clearColors.push_back(makeClearValueColor(clearColor));

#ifndef CTS_USES_VULKANSC
		if (m_data.groupParams->useSecondaryCmdBuffer)
		{
			secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

			// record secondary command buffer
			if (m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			{
				beginSecondaryCmdBuffer(*secCmdBuffer, imageFormat, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
				beginDynamicRender(*secCmdBuffer, renderArea, clearColors.data());
			}
			else
				beginSecondaryCmdBuffer(*secCmdBuffer, imageFormat);

			drawCommands(*secCmdBuffer, buffer);

			if (m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				endRendering(vk, *secCmdBuffer);

			endCommandBuffer(vk, *secCmdBuffer);

			// record primary command buffer
			beginCommandBuffer(vk, *cmdBuffer, 0u);

			if (!m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				beginDynamicRender(*cmdBuffer, renderArea, clearColors.data(), VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

			vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

			if (!m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
				endRendering(vk, *cmdBuffer);

			endCommandBuffer(vk, *cmdBuffer);
		}
		else if (m_data.groupParams->useDynamicRendering)
		{
			beginCommandBuffer(vk, *cmdBuffer);
			beginDynamicRender(*cmdBuffer, renderArea, clearColors.data());
			drawCommands(*cmdBuffer, buffer);
			endRendering(vk, *cmdBuffer);
			endCommandBuffer(vk, *cmdBuffer);
		}
#endif // CTS_USES_VULKANSC

		if (!m_data.groupParams->useDynamicRendering)
		{
			beginCommandBuffer(vk, *cmdBuffer);
			beginRenderPass(*cmdBuffer, renderArea, clearColors.data(), (deUint32)clearColors.size());
			drawCommands(*cmdBuffer, buffer);
			endRenderPass(vk, *cmdBuffer);
			endCommandBuffer(vk, *cmdBuffer);
		}

		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());
	}

	qpTestResult res = QP_TEST_RESULT_PASS;

	{
		const Allocation& resultAlloc = ssboBuffer->getBoundMemory();
		invalidateAlloc(vk, device, resultAlloc);

		const tcu::Vec4*	ptr		= reinterpret_cast<tcu::Vec4*>(resultAlloc.getHostPtr());
		for (deUint32 valueNdx = 0u; valueNdx < numValues; valueNdx++)
		{
			if (deFloatAbs(ptr[valueNdx].x() - ptr[valueNdx].y()) > 0.0005f)
			{
				log << tcu::TestLog::Message << "Expected value " << valueNdx << " is " << ptr[valueNdx].x() << ", got " << ptr[valueNdx].y()
					<< tcu::TestLog::EndMessage;
				res = QP_TEST_RESULT_FAIL;
			}
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void DrawTestInstance::beginRenderPass(VkCommandBuffer cmdBuffer, VkRect2D renderArea,
									   const VkClearValue* pClearValues, deUint32 clearValueCount) const
{
	const DeviceInterface& vk = m_context.getDeviceInterface();

	const VkRenderPassBeginInfo renderPassBeginInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		*m_renderPass,											// VkRenderPass							renderPass;
		*m_framebuffer,											// VkFramebuffer						framebuffer;
		renderArea,												// VkRect2D								renderArea;
		clearValueCount,										// deUint32								clearValueCount;
		pClearValues,											// const VkClearValue*					pClearValues;
	};

	vk.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void DrawTestInstance::drawCommands(VkCommandBuffer cmdBuffer, VkBuffer vertexBuffer) const
{
	const DeviceInterface&	vk = m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset = 0;

	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &*m_descriptorSet, 0u, DE_NULL);
	vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
}

#ifndef CTS_USES_VULKANSC
void DrawTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorFormat, VkRenderingFlagsKHR renderingFlags) const
{
	VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&colorFormat,															// const VkFormat*					pColorAttachmentFormats;
		VK_FORMAT_UNDEFINED,													// VkFormat							depthAttachmentFormat;
		VK_FORMAT_UNDEFINED,													// VkFormat							stencilAttachmentFormat;
		m_data.samples,															// VkSampleCountFlagBits			rasterizationSamples;
	};
	const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

	VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_data.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const VkCommandBufferBeginInfo commandBufBeginParams
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	const DeviceInterface& vk = m_context.getDeviceInterface();
	VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}

void DrawTestInstance::beginDynamicRender(VkCommandBuffer cmdBuffer, VkRect2D renderArea, const VkClearValue* pClearValues, VkRenderingFlagsKHR renderingFlags) const
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const deBool			useMultisampling	= m_data.samples != VK_SAMPLE_COUNT_1_BIT;

	VkRenderingAttachmentInfoKHR colorAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,								// VkStructureType						sType;
		DE_NULL,																		// const void*							pNext;
		useMultisampling ? *m_multisampleTargetView : *m_colorTargetView,				// VkImageView							imageView;
		VK_IMAGE_LAYOUT_GENERAL,														// VkImageLayout						imageLayout;
		useMultisampling ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,			// VkResolveModeFlagBits				resolveMode;
		useMultisampling ? *m_colorTargetView : DE_NULL,								// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_GENERAL,														// VkImageLayout						resolveImageLayout;
		useMultisampling ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,	// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,													// VkAttachmentStoreOp					storeOp;
		pClearValues[0]																	// VkClearValue							clearValue;
	};

	VkRenderingInfoKHR renderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		DE_NULL,
		renderingFlags,											// VkRenderingFlagsKHR					flags;
		renderArea,												// VkRect2D								renderArea;
		1u,														// deUint32								layerCount;
		0u,														// deUint32								viewMask;
		1u,														// deUint32								colorAttachmentCount;
		&colorAttachment,										// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	vk.cmdBeginRendering(cmdBuffer, &renderingInfo);
}
#endif // CTS_USES_VULKANSC

void createTests (tcu::TestCaseGroup* testGroup, const SharedGroupParams groupParams)
{
	tcu::TestContext&	testCtx		= testGroup->getTestContext();

	const VkSampleCountFlagBits samples[] =
	{
		VK_SAMPLE_COUNT_1_BIT,
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT,
	};

	const Interpolation interTypes[] =
	{
		SMOOTH,
		NOPERSPECTIVE
	};

	const AuxiliaryQualifier auxQualifiers[] =
	{
		AUX_NONE,
		AUX_SAMPLE,
		AUX_CENTROID,
	};

	for (deUint32 sampleNdx	= 0; sampleNdx < DE_LENGTH_OF_ARRAY(samples); sampleNdx++)
	{
		// reduce number of tests for dynamic rendering cases where secondary command buffer is used
		if (groupParams->useSecondaryCmdBuffer && (sampleNdx > VK_SAMPLE_COUNT_2_BIT))
			continue;

		for (deUint32 auxNdx	= 0;	auxNdx		< DE_LENGTH_OF_ARRAY(auxQualifiers);	auxNdx++)
		for (deUint32 interNdx	= 0;	interNdx	< DE_LENGTH_OF_ARRAY(interTypes);		interNdx++)
		{
			if (samples[sampleNdx] == VK_SAMPLE_COUNT_1_BIT && auxQualifiers[auxNdx] != AUX_NONE)
				continue;

			const DrawParams params
			{
				interTypes[interNdx],
				samples[sampleNdx],
				auxQualifiers[auxNdx],
				groupParams
			};
			testGroup->addChild(new DrawTestCase(testCtx, getTestName(params).c_str(), "", params));
		}
	}
}

}	// anonymous

tcu::TestCaseGroup*	createExplicitVertexParameterTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "explicit_vertex_parameter", "Tests for VK_AMD_shader_explicit_vertex_parameter.", createTests, groupParams);
}

}	// Draw
}	// vkt
