/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 ARM Ltd.
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
 * \brief VK_EXT_shader_tile_image tests.
 *//*--------------------------------------------------------------------*/

// Draw overwrapped patches with incremental value. The last value should be the patch count.
// Decision is made with comparing between simulated value and result value.
// All multi sample tests run with per sample shading property except MsaaSampleMask test case.
// There are several variants.
//  - Color
//  - Depth
//  - Stencil
//  - Msaa
//  - Formats
//  - Draw Count
//  - Patch Count per Draw
//  - Coherent Mode
//  ...

#include "vktShaderTileImageTests.hpp"
#include "deDefs.hpp"
#include "deSharedPtr.hpp"
#include "deUniquePtr.hpp"
#include "draw/vktDrawBufferObjectUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuResource.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktRasterizationTests.hpp"
#include "vktTestCase.hpp"

using namespace vk;
using de::MovePtr;
using de::SharedPtr;

namespace vkt
{

namespace rasterization
{

namespace
{

constexpr deUint32 kImageSize			   = 4; // power of 2 for helper test
constexpr deUint32 kMultiDrawElementCount  = 3;
constexpr deUint32 kMultiPatchElementCount = 3;
constexpr deUint32 kMRTCount			   = 2;
constexpr uint32_t kDerivative0			   = 1; // derivative 0 + offset 1
constexpr uint32_t kDerivative1			   = 2; // derivative 1 + offset 1

enum class TestType
{
	Color,
	MultiRenderTarget,
	MultiRenderTargetDynamicIndex,
	MsaaSampleMask,
	HelperClassColor,
	HelperClassDepth,
	HelperClassStencil,
	Depth,
	Stencil
};

struct TestParam
{
	bool				  coherent;
	TestType			  testType;
	VkFormat			  colorFormat;
	VkFormat			  depthStencilFormat;
	VkSampleCountFlagBits m_sampleCount;
	bool				  multipleDrawCalls;
	bool				  multiplePatchesPerDraw;
	deUint32			  frameBufferSize;
};

bool isHelperClassTest(TestType testType)
{
	const bool helperClass = (testType == TestType::HelperClassColor) || (testType == TestType::HelperClassDepth) ||
							 (testType == TestType::HelperClassStencil);
	return helperClass;
}

deUint32 getSampleCount(VkSampleCountFlagBits sampleCount)
{
	deUint32 ret = 0;
	switch (sampleCount)
	{
	case VK_SAMPLE_COUNT_1_BIT:
		ret = 1;
		break;
	case VK_SAMPLE_COUNT_2_BIT:
		ret = 2;
		break;
	case VK_SAMPLE_COUNT_4_BIT:
		ret = 4;
		break;
	case VK_SAMPLE_COUNT_8_BIT:
		ret = 8;
		break;
	case VK_SAMPLE_COUNT_16_BIT:
		ret = 16;
		break;
	case VK_SAMPLE_COUNT_32_BIT:
		ret = 32;
		break;
	case VK_SAMPLE_COUNT_64_BIT:
		ret = 64;
		break;
	default:
		DE_ASSERT(false);
	};
	return ret;
}

deUint32 getSampleMask(TestType testType)
{
	return (testType == TestType::MsaaSampleMask) ? 0xaaaaaaaa : 0;
}

deUint32 getColorAttachmentCount(TestType testType)
{
	switch (testType)
	{
	case TestType::MultiRenderTargetDynamicIndex:
	case TestType::MultiRenderTarget:
	case TestType::HelperClassColor:
	case TestType::HelperClassDepth:
	case TestType::HelperClassStencil:
		return kMRTCount;
	default:
		return 1;
	}
	return 1;
}

deUint32 getVertexCountPerPatch(const TestParam* testParam)
{
	return (testParam->testType == TestType::MsaaSampleMask) ? 3 : 6;
}

deUint32 getPatchesPerDrawCount(bool multiplePatchesPerDraw)
{
	return multiplePatchesPerDraw ? kMultiPatchElementCount : 1;
}

deUint32 getDrawCallCount(const TestParam* testParam)
{
	if (isHelperClassTest(testParam->testType))
	{
		// helper class use two draw calls, but it is similar to single draw call
		DE_ASSERT(!testParam->multipleDrawCalls);
		return 2;
	}

	return testParam->multipleDrawCalls ? kMultiDrawElementCount : 1;
}

bool isNormalizedColorFormat(VkFormat format)
{
	const tcu::TextureFormat	   colorFormat(mapVkFormat(format));
	const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(colorFormat.type));
	const bool normalizedColorFormat = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
										channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT);
	return normalizedColorFormat;
}

void addOverhead(std::stringstream& shaderStream)
{
	shaderStream << "{\n"
				 << "	uint overheadLoop = uint(gl_FragCoord.x) * uint(${TOTAL_PATCH_COUNT} + 1);\n"
				 << "	zero = patchIndex / (${TOTAL_PATCH_COUNT} + 1);\n"
				 << "	for(uint index = 0u; index < overheadLoop; index++)\n"
				 << "	{\n"
				 << "		zero = uint(sin(float(zero)));\n"
				 << "	}\n"
				 << "}\n";
}

void transition2DImage(const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image,
					   vk::VkImageAspectFlags aspectMask, vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout,
					   vk::VkAccessFlags srcAccessMask, vk::VkAccessFlags dstAccessMask,
					   vk::VkPipelineStageFlags srcStageMask, vk::VkPipelineStageFlags dstStageMask)
{
	vk::VkImageMemoryBarrier barrier;
	barrier.sType							= vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext							= DE_NULL;
	barrier.srcAccessMask					= srcAccessMask;
	barrier.dstAccessMask					= dstAccessMask;
	barrier.oldLayout						= oldLayout;
	barrier.newLayout						= newLayout;
	barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	barrier.image							= image;
	barrier.subresourceRange.aspectMask		= aspectMask;
	barrier.subresourceRange.baseMipLevel	= 0;
	barrier.subresourceRange.levelCount		= 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount		= 1;

	vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (vk::VkDependencyFlags)0, 0,
						  (const vk::VkMemoryBarrier*)DE_NULL, 0, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1,
						  &barrier);
}

class ShaderTileImageTestCase : public TestCase
{
public:
	ShaderTileImageTestCase(tcu::TestContext& context, const std::string& name, const std::string& description,
							const TestParam& testParam);
	~ShaderTileImageTestCase() override = default;
	TestInstance* createInstance(Context& context) const override;

protected:
	void initPrograms(SourceCollections& programCollection) const override;
	void checkSupport(Context& context) const override;

	void addVS(SourceCollections& programCollection, const std::map<std::string, std::string>& params) const;
	void addFS(SourceCollections& programCollection, const std::map<std::string, std::string>& params) const;
	void addCS(SourceCollections& programCollection, const std::map<std::string, std::string>& params) const;

	void getColorTestTypeFS(std::stringstream& fragShader) const;
	void getHelperClassTestTypeFS(std::stringstream& fragShader) const;
	void getSampleMaskTypeFS(std::stringstream& fragShader) const;
	void getDepthTestTypeFS(std::stringstream& fragShader) const;
	void getStencilTestTypeFS(std::stringstream& fragShader) const;

protected:
	const TestParam m_testParam;
};

class ShaderTileImageTestInstance : public TestInstance
{
public:
	ShaderTileImageTestInstance(Context& context, const TestParam* testParam);
	~ShaderTileImageTestInstance() override = default;
	tcu::TestStatus iterate() override;

protected:
	void			 initialize();
	void			 generateCmdBuffer();
	void			 generateVertexBuffer();
	void			 generateAttachments();
	Move<VkPipeline> generateGraphicsPipeline(bool disableColor0Write, bool disableDepthWrite,
											  bool disableStencilWrite);
	void			 generateComputePipeline();
	void			 rendering();
	deUint32		 getResultValue(deUint32 fx, deUint32 fy, deUint32 fs, deUint32 renderTargetID) const;
	deUint32		 simulate(deUint32 fx, deUint32 fy, deUint32 fs, deUint32 renderTargetID) const;
	tcu::TestStatus	 checkResult() const;

protected:
	const TestParam* m_testParam;

	const DeviceInterface&	m_vk;
	SharedPtr<Draw::Buffer> m_vertexBuffer;

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	Move<vk::VkDescriptorPool>		m_descriptorPool;
	Move<vk::VkDescriptorSet>		m_descriptorSets[kMRTCount];
	Move<VkPipelineLayout>			m_graphicsPipelineLayout;
	Move<VkPipeline>				m_graphicsPipeline;
	Move<VkPipeline>				m_graphicsPipelineForHelperClass;
	Move<vk::VkDescriptorSetLayout> m_computeDescriptorSetLayout;
	Move<VkPipelineLayout>			m_computePipelineLayout;
	Move<VkPipeline>				m_computePipeline;
	Move<VkShaderModule>			m_vertexModule;
	Move<VkShaderModule>			m_fragmentModule;
	Move<VkImage>					m_imageColor[kMRTCount];
	MovePtr<Allocation>				m_imageColorAlloc[kMRTCount];
	deUint32*						m_imageColorBufferHostPtr;
	Move<VkImageView>				m_imageColorView[kMRTCount];
	SharedPtr<Draw::Buffer>			m_imageBuffer[kMRTCount];
	Move<VkImage>					m_imageDepthStencil;
	MovePtr<Allocation>				m_imageDepthStencilAlloc;
	Move<VkImageView>				m_imageDepthStencilView;
};

ShaderTileImageTestCase::ShaderTileImageTestCase(tcu::TestContext& context, const std::string& name,
												 const std::string& description, const TestParam& testParam)
	: TestCase(context, name, description), m_testParam(testParam)
{
}

void ShaderTileImageTestCase::addVS(SourceCollections&						  programCollection,
									const std::map<std::string, std::string>& params) const
{
	std::stringstream vertShader;
	vertShader << "#version 450 core\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout(location = 0) in highp vec2 v_position;\n"
			   << "layout(location = 0) flat out uint patchIndex;"
			   << "layout( push_constant ) uniform ConstBlock\n"
			   << "{\n"
			   << "	highp uint drawIndex;\n"
			   << "};\n"
			   << "void main ()\n"
			   << "{\n"
			   << "	uint localPatchIndex = uint(gl_VertexIndex) / ${VERTEX_COUNT_PER_PATCH} + 1;\n" // index from 1
			   << "	uint patchCountPerDraw = ${PATCH_COUNT_PER_DRAW};\n"
			   << "	uint globalPatchIndex = drawIndex * patchCountPerDraw + localPatchIndex;\n"
			   << "	patchIndex = globalPatchIndex;\n"
			   << "	gl_Position = vec4(v_position, ${INV_TOTAL_PATCH_COUNT} * globalPatchIndex, 1);\n"
			   << "}\n";

	tcu::StringTemplate vertShaderTpl(vertShader.str());
	programCollection.glslSources.add("vert") << glu::VertexSource(vertShaderTpl.specialize(params));
}

void ShaderTileImageTestCase::getColorTestTypeFS(std::stringstream& fragShader) const
{
	const deUint32 attachmentCount		   = getColorAttachmentCount(m_testParam.testType);
	const bool	   mrtDynamicIndexTestType = (m_testParam.testType == TestType::MultiRenderTargetDynamicIndex);
	const bool	   multiSampleTest		   = (m_testParam.m_sampleCount != VK_SAMPLE_COUNT_1_BIT);

	const tcu::TextureFormat	   colorFormat(mapVkFormat(m_testParam.colorFormat));
	const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(colorFormat.type));
	const bool					   normalizedColorFormat = isNormalizedColorFormat(m_testParam.colorFormat);
	const tcu::IVec4			   channelBitDepth		 = tcu::getTextureFormatBitDepth(colorFormat);

	fragShader << "#version 450 core\n"
			   << "#extension GL_EXT_shader_tile_image : require\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout( push_constant ) uniform ConstBlock\n"
			   << "{\n"
			   << "	highp uint drawIndex;\n"
			   << "};\n"
			   << "layout( location = 0 ) flat in uint patchIndex;\n";

	if (!m_testParam.coherent)
	{
		fragShader << "layout( non_coherent_color_attachment_readEXT ) in;\n";
	}

	if (mrtDynamicIndexTestType)
	{
		// layout( location = 0 ) tileImageEXT highp attachmentEXT colorIn[0]
		fragShader << "layout( location = 0 ) tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn[${ATTACHMENT_COUNT}];\n";
	}
	else
	{
		for (deUint32 i = 0; i < attachmentCount; i++)
		{
			// layout( location = 0 ) tileImageEXT highp attachmentEXT colorIn0
			fragShader << "layout( location = " << i << ") tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn" << i << ";\n";
		}
	}

	for (deUint32 i = 0; i < attachmentCount; i++)
	{
		// layout( location = 0 ) out highp vec4 out0
		fragShader << "layout( location = " << i << " ) out highp ${OUTPUT_VECTOR_NAME} out" << i << ";\n";
	}

	fragShader << "void main()\n"
			   << "{\n"
			   << "	uint zero = 0;\n"
			   << "	uvec2 previous[${ATTACHMENT_COUNT}];\n";

	float amplifier = 1.0f;
	if (normalizedColorFormat)
	{
		amplifier = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT) ?
						static_cast<float>(1 << (channelBitDepth.y() - 1)) : // signed
						static_cast<float>((1 << channelBitDepth.y()) - 1);	 // unsigned

		// color output precision is less than test case;
		DE_ASSERT(amplifier > static_cast<float>(kMultiPatchElementCount * kMultiDrawElementCount * attachmentCount +
												 getSampleCount(m_testParam.m_sampleCount)));
	}

	for (deUint32 i = 0; i < attachmentCount; i++)
	{
		// in0 or colorIn[0]
		const std::string inputImage =
			mrtDynamicIndexTestType ? "colorIn[" + std::to_string(i) + "]" : "colorIn" + std::to_string(i);

		// (in0) or (colorIn0, gl_SampleID)
		const std::string funcParams = multiSampleTest ? "(" + inputImage + ", gl_SampleID)" : "(" + inputImage + ")";

		if (normalizedColorFormat)
		{
			// previous[0] = round(colorAttachmentRead(in0) *  amplifier).xy;\n";
			fragShader << "	previous[" << i << "] = uvec2(round((colorAttachmentReadEXT" << funcParams << " * "
					   << amplifier << ").xy));\n";
		}
		else
		{
			// previous[0] *= uvec2(round(colorAttachmentRead(in0).xy));\n";
			fragShader << "	previous[" << i << "] = uvec2(round((colorAttachmentReadEXT" << funcParams << ").xy));\n";
		}
	}

	// add overhead after fetching data
	addOverhead(fragShader);

	// used only for normalized color format
	const float invAmplifier = 1.0f / static_cast<float>(amplifier);

	// write output
	for (deUint32 i = 0; i < attachmentCount; i++)
	{
		// if (previous[0].x == 0 && patchIndex == 1)", initial write
		//  out0.y = float(patchIndex + zero + gl_SampleID +  0);"
		// else if (previous[0].x == 0 && (previous[0].y + 1) == (patchIndex + gl_SampleID + 0))"
		//  out0.y = float(previous[0].y + 1);"
		// else
		//  out0.y = float(previous[0].y);"
		//  out0.x = 1;" // error
		fragShader << "	if (previous[" << i << "].x == 0 && patchIndex == 1)\n"
				   << "	{\n"
				   << "		out" << i << ".y = ${OUTPUT_BASIC_TYPE}(patchIndex + zero + gl_SampleID + " << i << ");\n"
				   << "	}\n"
				   << "	else if (previous[" << i << "].x == 0 && (previous[" << i
				   << "].y + 1) == (patchIndex + gl_SampleID + " << i << "))\n"
				   << "	{\n"
				   << "		out" << i << ".y = ${OUTPUT_BASIC_TYPE}(previous[" << i << "].y + 1 + zero);\n"
				   << "	}\n"
				   << "	else\n"
				   << "	{\n"
				   << "		out" << i << ".y = ${OUTPUT_BASIC_TYPE}(previous[" << i << "].y);\n" // for debug purpose
				   << "		out" << i << ".x = 1;\n"											 // error
				   << "	}\n";

		if (normalizedColorFormat)
		{
			// out0.y *= invAmplifier;
			fragShader << "		out" << i << ".y *= " << invAmplifier << ";\n";
		}
	}
	fragShader << "}\n";
}

void ShaderTileImageTestCase::getHelperClassTestTypeFS(std::stringstream& fragShader) const
{
	const bool depthHelperClassTest	  = (m_testParam.testType == TestType::HelperClassDepth);
	const bool stencilHelperClassTest = (m_testParam.testType == TestType::HelperClassStencil);

	DE_ASSERT(getPatchesPerDrawCount(!m_testParam.multiplePatchesPerDraw));
	DE_ASSERT(getDrawCallCount(&m_testParam) == 2);
	DE_ASSERT(getColorAttachmentCount(m_testParam.testType) == 2);
	DE_ASSERT((m_testParam.m_sampleCount == VK_SAMPLE_COUNT_1_BIT));
	DE_ASSERT(!isNormalizedColorFormat(m_testParam.colorFormat));

	fragShader << "#version 450 core\n"
			   << "#extension GL_EXT_shader_tile_image : require\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout( push_constant ) uniform ConstBlock\n"
			   << "{\n"
			   << "	highp uint drawIndex;\n"
			   << "};\n"
			   << "layout( location = 0 ) flat in uint patchIndex;\n";

	if (!m_testParam.coherent)
	{
		fragShader << "layout( non_coherent_color_attachment_readEXT ) in;\n";
		if (depthHelperClassTest)
		{
			fragShader << "layout( non_coherent_depth_attachment_readEXT ) in;\n";
		}

		if (stencilHelperClassTest)
		{
			fragShader << "layout( non_coherent_stencil_attachment_readEXT ) in;\n";
		}
	}

	fragShader << "layout(location = 0) tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn0;\n";
	fragShader << "layout(location = 1) tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn1;\n";

	fragShader << "layout(location = 0) out highp ${OUTPUT_VECTOR_NAME} out0;\n";
	fragShader << "layout(location = 1) out highp ${OUTPUT_VECTOR_NAME} out1;\n";

	fragShader << "void main()\n"
			   << "{\n"
			   << "	uint zero = 0;\n"
			   << "	uvec2 previous;\n";

	if (depthHelperClassTest)
	{
		fragShader << " uint scalingFactor = ${TOTAL_PATCH_COUNT};\n";
		fragShader << "	previous.x = uint(round(colorAttachmentReadEXT(colorIn0).x));\n";		// read error status
		fragShader << "	previous.y = uint(round(depthAttachmentReadEXT() * scalingFactor));\n"; // read depth value
	}
	else if (stencilHelperClassTest)
	{
		fragShader << "	previous.x = uint(round(colorAttachmentReadEXT(colorIn0).x));\n"; // read error status
		fragShader << "	previous.y = uint(stencilAttachmentReadEXT());\n";				  // read stencil value
	}
	else
	{
		fragShader << "	previous = uvec2(round((colorAttachmentReadEXT(colorIn0)).xy));\n";
	}

	{
		// draw only one triangle for helperClassTestType, dx or dy should be 0 inside of triangle.
		// And they should be patchIndex in the diagonal edge of triangle.
		fragShader << "	uint err = 0;\n"
				   << "	uint dx = 0;\n"
				   << "	uint dy = 0;\n"
				   << "	if (patchIndex != 1)"
				   << "	{\n"
				   << "		dx = uint(round(abs(dFdxFine(previous.y))));\n"
				   << "		dy = uint(round(abs(dFdyFine(previous.y))));\n"
				   << "		uint err = 0;\n"
				   << "		if ((dx != 0 && dx != patchIndex - 1) || (dy != 0 && dy != patchIndex - 1))\n"
				   << "		{\n"
				   << "			err = 1;\n" // first draw doesn't have error check.
				   << "		}\n"
				   << "	}\n";
	}

	// add overhead after fetching data
	addOverhead(fragShader);

	// first draw writes to attachment0
	// second draw reads from attachment0(depth) writes to attachment1
	{
		fragShader << "	if (patchIndex == 1 && err != 1)\n"
				   << "	{\n"
				   << "		out0.y = ${OUTPUT_BASIC_TYPE}(patchIndex);\n"
				   << "		out0.x = 0;\n" // error
				   << "	}\n"
				   << "	else if (previous.x == 0 && err != 1 && ((previous.y + 1) == patchIndex || previous.y == 0))\n"
				   << "	{\n"
				   << "		out1.y = ${OUTPUT_BASIC_TYPE}(max(dx, dy) + 1);\n" // last 1 is to differentiate clear value
				   << "	}\n"
				   << "	else\n"
				   << "	{\n"
				   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previous.y);\n" // for debug purpose
				   << "		out0.x = 1;\n"								  // error
				   << "		out1.y = ${OUTPUT_BASIC_TYPE}(previous.x);\n"
				   << "		out1.x = 1;\n" // error
				   << "	}\n";
	}
	fragShader << "}\n";
}

void ShaderTileImageTestCase::getSampleMaskTypeFS(std::stringstream& fragShader) const
{
	const deUint32 sampleCount = getSampleCount(m_testParam.m_sampleCount);

	const tcu::TextureFormat	   colorFormat(mapVkFormat(m_testParam.colorFormat));
	const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(colorFormat.type));
	const bool					   normalizedColorFormat = isNormalizedColorFormat(m_testParam.colorFormat);
	const tcu::IVec4			   channelBitDepth		 = tcu::getTextureFormatBitDepth(colorFormat);

	deUint32 amplifier = 1;
	if (normalizedColorFormat)
	{
		amplifier = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT) ?
						(1 << (channelBitDepth.y() - 1)) : // signed
						((1 << channelBitDepth.y()) - 1);  // unsigned
	}

	// Samples which is not covered should be 0
	fragShader << "#version 450 core\n"
			   << "#extension GL_EXT_shader_tile_image : require\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout( push_constant ) uniform ConstBlock\n"
			   << "{\n"
			   << "	highp uint drawIndex;\n"
			   << "};\n";
	if (!m_testParam.coherent)
	{
		fragShader << "layout( non_coherent_color_attachment_readEXT ) in;\n";
	}
	fragShader << "layout( location = 0 ) flat in uint patchIndex;\n"
			   << "layout( location = 0 ) tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn0;\n"
			   << "layout( location = 0 ) out highp ${OUTPUT_VECTOR_NAME} out0;\n"
			   << "\n"
			   << "void main()\n"
			   << "{\n"
			   << "	uint zero = 0;\n"
			   << "	uint previous = 0;\n"
			   << "	bool error = false;\n"
			   << "	for (int i = 0; i < " << sampleCount << "; ++i)\n"
			   << "	{\n"
			   << "		if (((gl_SampleMaskIn[0] >> i) & 0x1) == 0x1)\n"
			   << "		{\n"
			   << "			uvec2 previousSample = uvec2(round(colorAttachmentReadEXT"
			   << "(colorIn0, i) * " << amplifier << ")).xy;\n"
			   << "			if (previousSample.x != 0)\n"
			   << "			{\n"
			   << "				error = true;\n"
			   << "				break;"
			   << "			}\n"
			   << "			if (previous == 0)\n"
			   << "			{\n"
			   << "				previous = previousSample.y;\n" // write non zero value to the covered sample
			   << "			}\n"
			   << "\n"
			   << "			if ((patchIndex != 1 && previousSample.y == 0) || previous != previousSample.y)\n"
			   << "			{\n"
			   << "				error = true;\n"
			   << "				break;\n"
			   << "			}\n"
			   << "		}\n"
			   << "	}\n"
			   << "\n";

	// add overhead after fetching data
	addOverhead(fragShader);

	// write output
	fragShader << "if (!error && (previous + 1 == patchIndex))\n"
			   << "	{\n"
			   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previous + 1 + zero);\n"
			   << "	}\n"
			   << "	else\n"
			   << "	{\n"
			   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previous);\n"
			   << "		out0.x = 1;\n" // error
			   << "	}\n";

	const float invAmplifier = 1.0f / static_cast<float>(amplifier);
	if (normalizedColorFormat)
	{
		fragShader << "		out0.y *= " << invAmplifier << ";\n";
	}

	fragShader << "}\n";
}

void ShaderTileImageTestCase::getDepthTestTypeFS(std::stringstream& fragShader) const
{
	const bool		  multiSampleTest = (m_testParam.m_sampleCount != VK_SAMPLE_COUNT_1_BIT);
	const std::string depthFuncParams = multiSampleTest ? "(gl_SampleID)" : "()";
	const std::string colorFuncParams = multiSampleTest ? "(colorIn0, gl_SampleID)" : "(colorIn0)";
	const deUint32	  sampleCount	  = getSampleCount(m_testParam.m_sampleCount);

	fragShader << "#version 450 core\n"
			   << "#extension GL_EXT_shader_tile_image : require\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout( push_constant ) uniform ConstBlock\n"
			   << "{\n"
			   << "	highp uint drawIndex;\n"
			   << "};\n";
	if (!m_testParam.coherent)
	{
		fragShader << "layout( non_coherent_depth_attachment_readEXT ) in;\n";
		fragShader << "layout( non_coherent_color_attachment_readEXT ) in;\n";
	}
	fragShader << "layout( location = 0 ) flat in uint patchIndex;\n"
			   << "layout( location = 0 ) tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn0;\n"
			   << "layout( location = 0 ) out highp ${OUTPUT_VECTOR_NAME} out0;\n"
			   << "\n"
			   << "void main()\n"
			   << "{\n"
			   << "	uint zero = 0;\n"
			   << " uint scalingFactor = ${TOTAL_PATCH_COUNT};\n";
	if (multiSampleTest)
	{
		// scaling with (patch count + sample count) for multisample case
		fragShader << " scalingFactor += " << sampleCount << ";\n";
	}
	fragShader << "	uint previousDepth = uint(round(depthAttachmentReadEXT" << depthFuncParams
			   << " * scalingFactor));\n"
			   << "	${OUTPUT_VECTOR_NAME} previous = ${OUTPUT_VECTOR_NAME}(round(colorAttachmentReadEXT"
			   << colorFuncParams << "));\n";

	// add overhead after fetching data
	addOverhead(fragShader);

	// write output
	fragShader << "	if (previous.x == 0 && patchIndex == 1)\n"
			   << "	{\n"
			   << "		out0.y = (1u + zero + gl_SampleID);\n"
			   << "	}\n"
			   << "	else if (previous.x == 0 && (previous.y + 1) == (patchIndex + gl_SampleID) && (previousDepth + 1) "
				  "== (patchIndex + gl_SampleID))\n"
			   << "	{\n"
			   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previousDepth + 1 + zero);\n"
			   << "	}\n"
			   << "	else\n"
			   << "	{\n"
			   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previousDepth);\n" // debug purpose
			   << "		out0.x = 1;\n"									 // error
			   << "	}\n";

	if (multiSampleTest)
	{
		// Depth value is written without adding SampleID.
		// Forcely write all fragment depth
		fragShader << " gl_FragDepth = float(out0.y) / scalingFactor;\n";
	}

	fragShader << "}\n";
}

void ShaderTileImageTestCase::getStencilTestTypeFS(std::stringstream& fragShader) const
{
	const bool		  multiSampleTest	= (m_testParam.m_sampleCount != VK_SAMPLE_COUNT_1_BIT);
	const std::string stencilFuncParams = multiSampleTest ? "(gl_SampleID)" : "()";
	const std::string colorFuncParams	= multiSampleTest ? "(colorIn0, gl_SampleID)" : "(colorIn0)";

	fragShader << "#version 450 core\n"
			   << "#extension GL_EXT_shader_tile_image : require\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout( push_constant ) uniform ConstBlock\n"
			   << "{\n"
			   << "	highp uint drawIndex;\n"
			   << "};\n";
	if (!m_testParam.coherent)
	{
		fragShader << "layout( non_coherent_stencil_attachment_readEXT ) in;\n";
		fragShader << "layout( non_coherent_color_attachment_readEXT ) in;\n";
	}
	fragShader << "layout( location = 0 ) flat in uint patchIndex;\n"
			   << "layout( location = 0 ) tileImageEXT highp ${TILE_IMAGE_TYPE} colorIn0;\n"
			   << "layout( location = 0 ) out highp ${OUTPUT_VECTOR_NAME} out0;\n"
			   << "\n"
			   << "void main()\n"
			   << "{\n"
			   << "	uint zero = 0;\n"
			   << "	uint previousStencil = uint(round(stencilAttachmentReadEXT" << stencilFuncParams << " ));\n"
			   << "	${OUTPUT_VECTOR_NAME} previous = ${OUTPUT_VECTOR_NAME}(round(colorAttachmentReadEXT"
			   << colorFuncParams << "));\n";

	// add overhead after fetching data
	addOverhead(fragShader);

	// write output
	fragShader << "	if (previous.x == 0 && (previous.y + 1) == patchIndex && (previousStencil + 1) == patchIndex)\n"
			   << "	{\n"
			   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previousStencil + 1 + zero);\n"
			   << "	}\n"
			   << "	else\n"
			   << "	{\n"
			   << "		out0.y = ${OUTPUT_BASIC_TYPE}(previousStencil);\n" // debug purpose
			   << "		out0.x = 1;\n"									   // error
			   << "	}\n"
			   << "}\n";
}

void rasterization::ShaderTileImageTestCase::addFS(SourceCollections&						 programCollection,
												   const std::map<std::string, std::string>& params) const
{
	std::stringstream fragShader;

	switch (m_testParam.testType)
	{
	case TestType::Color:
	case TestType::MultiRenderTarget:
	case TestType::MultiRenderTargetDynamicIndex:
		getColorTestTypeFS(fragShader);
		break;
	case TestType::HelperClassColor:
	case TestType::HelperClassDepth:
	case TestType::HelperClassStencil:
		getHelperClassTestTypeFS(fragShader);
		break;
	case TestType::MsaaSampleMask:
		getSampleMaskTypeFS(fragShader);
		break;
	case TestType::Depth:
		getDepthTestTypeFS(fragShader);
		break;
	case TestType::Stencil:
		getStencilTestTypeFS(fragShader);
		break;
	default:
		DE_ASSERT(true);
	}

	tcu::StringTemplate fragShaderTpl(fragShader.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShaderTpl.specialize(params));
}

// Copy Image to Buffer using Compute Shader for handling multi sample cases
void ShaderTileImageTestCase::addCS(SourceCollections&						  programCollection,
									const std::map<std::string, std::string>& params) const
{
	std::stringstream compShader;

	const deUint32	  sampleCount = getSampleCount(m_testParam.m_sampleCount);
	const std::string fsampler	  = sampleCount > 1 ? "texture2DMS" : "texture2D";
	const std::string usampler	  = sampleCount > 1 ? "utexture2DMS" : "utexture2D";
	const std::string isampler	  = sampleCount > 1 ? "itexture2DMS" : "itexture2D";

	const tcu::TextureFormat	   colorFormat(mapVkFormat(m_testParam.colorFormat));
	const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(colorFormat.type));
	const tcu::IVec4			   channelBitDepth		 = tcu::getTextureFormatBitDepth(colorFormat);
	const bool					   normalizedColorFormat = isNormalizedColorFormat(m_testParam.colorFormat);

	std::string sampler;
	switch (channelClass)
	{
	case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		sampler = usampler;
		break;
	case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		sampler = isampler;
		break;
	default:
		sampler = fsampler;
	}

	deUint32 amplifier = 1;

	if (normalizedColorFormat)
	{
		amplifier = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT) ?
						(1 << (channelBitDepth.y() - 1)) : // signed
						((1 << channelBitDepth.y()) - 1);  // unsigned
	}

	// Compute shader copies color to linear layout in buffer memory
	compShader << "#version 450 core\n"
			   << "#extension GL_EXT_samplerless_texture_functions : enable\n"
			   << "precision highp float;\n"
			   << "precision highp int;\n"
			   << "layout(set = 0, binding = 0) uniform " << sampler << " colorTex;\n"
			   << "layout(set = 0, binding = 1, std430) buffer Block0 { uvec2 values[]; } colorbuf;\n"
			   << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
			   << "void main()\n"
			   << "{\n"
			   << "	for (uint i = 0u; i < " << sampleCount << "u; ++i) {\n"
			   << "		uint idx = ((gl_GlobalInvocationID.y * " << m_testParam.frameBufferSize
			   << "u) + gl_GlobalInvocationID.x) * " << sampleCount << "u + i;\n";

	if (normalizedColorFormat)
	{
		compShader << "		colorbuf.values[idx].y = uint(round(texelFetch(colorTex, ivec2(gl_GlobalInvocationID.xy), "
					  "int(i)).y * "
				   << amplifier << "));\n";
		compShader << "		colorbuf.values[idx].x = uint(round(texelFetch(colorTex, ivec2(gl_GlobalInvocationID.xy), "
					  "int(i)).x));\n";
	}
	else
	{
		compShader << "		colorbuf.values[idx] = uvec2(round(vec2(texelFetch(colorTex, "
					  "ivec2(gl_GlobalInvocationID.xy), int(i)).xy)));\n";
	}

	compShader << "	}\n"
			   << "}\n";

	tcu::StringTemplate computeShaderTpl(compShader.str());
	programCollection.glslSources.add("comp") << glu::ComputeSource(computeShaderTpl.specialize(params));
}

void ShaderTileImageTestCase::initPrograms(SourceCollections& programCollection) const
{
	std::map<std::string, std::string> params;

	const deUint32				   drawCount		 = getDrawCallCount(&m_testParam);
	const deUint32				   patchCountPerDraw = getPatchesPerDrawCount(m_testParam.multiplePatchesPerDraw);
	const deUint32				   attachmentCount	 = getColorAttachmentCount(m_testParam.testType);
	const tcu::TextureFormat	   colorFormat(mapVkFormat(m_testParam.colorFormat));
	const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(colorFormat.type));

	params["VERTEX_COUNT_PER_PATCH"] = std::to_string(getVertexCountPerPatch(&m_testParam));
	params["PATCH_COUNT_PER_DRAW"]	 = std::to_string(patchCountPerDraw);
	params["INV_TOTAL_PATCH_COUNT"]	 = std::to_string(1.0f / static_cast<float>(drawCount * patchCountPerDraw));
	params["TOTAL_PATCH_COUNT"]		 = std::to_string(drawCount * patchCountPerDraw);
	params["ATTACHMENT_COUNT"]		 = std::to_string(attachmentCount);

	std::string strVecName;
	std::string strBasicType;
	std::string strTileImageType;

	switch (channelClass)
	{
	case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		strVecName		 = "uvec";
		strTileImageType = "uattachmentEXT";
		strBasicType	 = "uint";
		break;
	case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		strVecName		 = "ivec";
		strTileImageType = "iattachmentEXT";
		strBasicType	 = "int";
		break;
	default:
		strVecName		 = "vec";
		strTileImageType = "attachmentEXT";
		strBasicType	 = "float";
	}
	params["OUTPUT_VECTOR_NAME"] = strVecName + std::to_string(tcu::getNumUsedChannels(colorFormat.order));
	params["OUTPUT_BASIC_TYPE"]	 = strBasicType;
	params["TILE_IMAGE_TYPE"]	 = strTileImageType;

	addVS(programCollection, params);
	addFS(programCollection, params);
	addCS(programCollection, params);
}

TestInstance* ShaderTileImageTestCase::createInstance(Context& context) const
{
	return new ShaderTileImageTestInstance(context, &m_testParam);
}

void ShaderTileImageTestCase::checkSupport(Context& context) const
{
	if (!context.requireDeviceFunctionality("VK_KHR_dynamic_rendering"))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_dynamic_rendering not supported");
	}

	if (!context.requireDeviceFunctionality("VK_EXT_shader_tile_image"))
	{
		TCU_THROW(NotSupportedError, "VK_EXT_shader_tile_image not supported");
	}
	/* sampleRateShading must be enabled to call fragment shader for all the samples in multisampling */
	VkPhysicalDeviceShaderTileImageFeaturesEXT shaderTileImageFeature = {};
	shaderTileImageFeature.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT;

	VkPhysicalDeviceFeatures  features	= {};
	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType						= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext						= &shaderTileImageFeature;

	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);
	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (!shaderTileImageFeature.shaderTileImageColorReadAccess)
	{
		TCU_THROW(NotSupportedError, "color read access of VK_EXT_shader_tile_image is not supported");
	}
	switch (m_testParam.testType)
	{
	case TestType::Depth:
	case TestType::HelperClassDepth:
		if (!shaderTileImageFeature.shaderTileImageDepthReadAccess)
		{
			TCU_THROW(NotSupportedError, "depth read access of VK_EXT_shader_tile_image is not supported");
		}
		break;
	case TestType::Stencil:
	case TestType::HelperClassStencil:
		if (!shaderTileImageFeature.shaderTileImageStencilReadAccess)
		{
			TCU_THROW(NotSupportedError, "stencil read access of VK_EXT_shader_tile_image is not supported");
		}
		break;
	case TestType::Color:
	case TestType::MultiRenderTarget:
	case TestType::MultiRenderTargetDynamicIndex:
	case TestType::MsaaSampleMask:
	case TestType::HelperClassColor:
		break;
	default:
		DE_ASSERT(0);
	}

	VkPhysicalDeviceVulkan12Properties vulkan12Properties = {};
	vulkan12Properties.sType							  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

	VkPhysicalDeviceShaderTileImagePropertiesEXT shaderTileImageProperties = {};
	shaderTileImageProperties.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_PROPERTIES_EXT;
	shaderTileImageProperties.pNext = &vulkan12Properties;

	VkPhysicalDeviceProperties2 properties = {};
	properties.sType					   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext					   = &shaderTileImageProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

	// shaderTileImageReadSampleFromPixelRateInvocation is a boolean that will be VK_TRUE if reading from samples from a
	// pixel rate fragment invocation is supported when VkPipelineMultisampleStateCreateInfo::rasterizationSamples > 1.
	// shaderTileImageReadFromHelperInvocation is a boolean that will be VK_TRUE if reads of tile image data from helper
	// fragment invocations result in valid values.
	if (!shaderTileImageProperties.shaderTileImageReadSampleFromPixelRateInvocation)
	{
		if (m_testParam.testType == TestType::MsaaSampleMask)
		{
			TCU_THROW(NotSupportedError, "multi-samples pixel access of VK_EXT_shader_tile_image is not supported");
		}
	}

	if (!shaderTileImageProperties.shaderTileImageReadFromHelperInvocation)
	{
		if (isHelperClassTest(m_testParam.testType))
		{
			TCU_THROW(NotSupportedError, "helper class fragments access of VK_EXT_shader_tile_image is not supported");
		}
	}

	const tcu::TextureFormat	   colorFormat(mapVkFormat(m_testParam.colorFormat));
	const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(colorFormat.type));
	if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ||
		channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
	{
		if ((vulkan12Properties.framebufferIntegerColorSampleCounts & m_testParam.m_sampleCount) == 0 ||
			(properties.properties.limits.sampledImageIntegerSampleCounts & m_testParam.m_sampleCount) == 0)
		{
			TCU_THROW(NotSupportedError, "Sample count not supported");
		}
	}
	else
	{
		if ((properties.properties.limits.framebufferColorSampleCounts & m_testParam.m_sampleCount) == 0 ||
			(properties.properties.limits.sampledImageColorSampleCounts & m_testParam.m_sampleCount) == 0)
		{
			TCU_THROW(NotSupportedError, "Sample count not supported");
		}
	}

	if (m_testParam.m_sampleCount != VK_SAMPLE_COUNT_1_BIT && m_testParam.testType != TestType::MsaaSampleMask &&
		!features.sampleRateShading)
	{
		TCU_THROW(NotSupportedError, "sampleRateShading feature not supported");
	}

	const deUint32 attachmentCount = getColorAttachmentCount(m_testParam.testType);

	if (properties.properties.limits.maxFragmentOutputAttachments < attachmentCount ||
		properties.properties.limits.maxPerStageDescriptorInputAttachments < attachmentCount)
	{
		TCU_THROW(NotSupportedError, "attachment number not supported");
	}

	const InstanceInterface& vki			= context.getInstanceInterface();
	VkPhysicalDevice		 physicalDevice = context.getPhysicalDevice();
	const VkFormatProperties colorFormatProperties(
		getPhysicalDeviceFormatProperties(vki, physicalDevice, m_testParam.colorFormat));
	const VkFormatProperties dsFormatProperties(
		getPhysicalDeviceFormatProperties(vki, physicalDevice, m_testParam.depthStencilFormat));

	if ((colorFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
	{
		TCU_THROW(NotSupportedError, "Format can't be used as color attachment");
	}

	if ((dsFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
	{
		TCU_THROW(NotSupportedError, "Format can't be used as depth stencil attachment");
	}
}

ShaderTileImageTestInstance::ShaderTileImageTestInstance(Context& context, const TestParam* testParam)
	: TestInstance(context), m_testParam(testParam), m_vk(m_context.getDeviceInterface())
{
	initialize();
}

void ShaderTileImageTestInstance::initialize()
{
	generateCmdBuffer();
	generateAttachments();
	generateVertexBuffer();
	m_graphicsPipeline				 = generateGraphicsPipeline(false, false, false);
	m_graphicsPipelineForHelperClass = generateGraphicsPipeline(true, true, true);
	generateComputePipeline();
}

void ShaderTileImageTestInstance::generateComputePipeline()
{
	const deUint32 attachmentSize = getColorAttachmentCount(m_testParam->testType);
	const VkDevice device		  = m_context.getDevice();

	const Unique<VkShaderModule> cs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get("comp"), 0));

	VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

	const VkDescriptorSetLayoutBinding bindings[] = {
		{
			0,								  // binding
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, // descriptorType
			1,								  // descriptorCount
			VK_SHADER_STAGE_COMPUTE_BIT,	  // stageFlags
			DE_NULL,						  // pImmutableSamplers
		},
		{
			1,								   // binding
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
			1,								   // descriptorCount
			VK_SHADER_STAGE_COMPUTE_BIT,	   // stageFlags
			DE_NULL,						   // pImmutableSamplers
		},
	};

	// Create a layout and allocate a descriptor set for it.
	const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
		DE_NULL,												 // pNext
		layoutCreateFlags,										 // flags
		sizeof(bindings) / sizeof(bindings[0]),					 // bindingCount
		&bindings[0]											 // pBindings
	};

	m_computeDescriptorSetLayout = vk::createDescriptorSetLayout(m_vk, device, &setLayoutCreateInfo);

	const VkPipelineShaderStageCreateInfo csShaderCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		(VkPipelineShaderStageCreateFlags)0,
		VK_SHADER_STAGE_COMPUTE_BIT, // stage
		*cs,						 // shader
		"main",
		DE_NULL, // pSpecializationInfo
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
		DE_NULL,									   // pNext
		(VkPipelineLayoutCreateFlags)0,
		1,									 // setLayoutCount
		&m_computeDescriptorSetLayout.get(), // pSetLayouts
		0,									 // pushConstantRangeCount
		DE_NULL,							 // pPushConstantRanges
	};

	m_computePipelineLayout = createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo, NULL);

	const VkComputePipelineCreateInfo pipelineCreateInfo = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,						  // flags
		csShaderCreateInfo,		  // cs
		*m_computePipelineLayout, // layout
		(vk::VkPipeline)0,		  // basePipelineHandle
		0u,						  // basePipelineIndex
	};

	m_computePipeline = createComputePipeline(m_vk, device, DE_NULL, &pipelineCreateInfo, NULL);

	VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	vk::DescriptorPoolBuilder poolBuilder;
	for (deUint32 i = 0; i < (deInt32)(sizeof(bindings) / sizeof(bindings[0])); ++i)
	{
		poolBuilder.addType(bindings[i].descriptorType, bindings[i].descriptorCount * attachmentSize);
	}
	m_descriptorPool = poolBuilder.build(m_vk, device, poolCreateFlags, attachmentSize);

	for (deUint32 i = 0; i < attachmentSize; ++i)
	{
		m_descriptorSets[i] = makeDescriptorSet(m_vk, device, *m_descriptorPool, *m_computeDescriptorSetLayout);
		VkDescriptorImageInfo  imageInfo;
		VkDescriptorBufferInfo bufferInfo;

		VkWriteDescriptorSet w = {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType
			DE_NULL,								// pNext
			*m_descriptorSets[i],					// dstSet
			(deUint32)0,							// dstBinding
			0,										// dstArrayElement
			1u,										// descriptorCount
			bindings[0].descriptorType,				// descriptorType
			&imageInfo,								// pImageInfo
			&bufferInfo,							// pBufferInfo
			DE_NULL,								// pTexelBufferView
		};

		imageInfo = makeDescriptorImageInfo(DE_NULL, *m_imageColorView[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		w.dstBinding	 = 0;
		w.descriptorType = bindings[0].descriptorType;
		m_vk.updateDescriptorSets(device, 1, &w, 0, NULL);

		bufferInfo		 = makeDescriptorBufferInfo(m_imageBuffer[i]->object(), 0, VK_WHOLE_SIZE);
		w.dstBinding	 = 1;
		w.descriptorType = bindings[1].descriptorType;
		m_vk.updateDescriptorSets(device, 1, &w, 0, NULL);
	}
}

Move<VkPipeline> ShaderTileImageTestInstance::generateGraphicsPipeline(bool disableColor0Write, bool disableDepthWrite,
																	   bool disableStencilWrite)
{
	const VkDevice device = m_context.getDevice();

	VkPushConstantRange pushConstant;
	pushConstant.offset		= 0;
	pushConstant.size		= sizeof(deUint32);
	pushConstant.stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	m_graphicsPipelineLayout = makePipelineLayout(m_vk, device, 0, nullptr, 1, &pushConstant);
	m_vertexModule			 = createShaderModule(m_vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	m_fragmentModule		 = createShaderModule(m_vk, device, m_context.getBinaryCollection().get("frag"), 0u);

	const VkVertexInputBindingDescription vertexInputBindingDescription = {
		0,							 // deUint32 binding;
		sizeof(tcu::Vec2),			 // deUint32 strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputStepRate stepRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
		0u,						 // deUint32 location;
		0u,						 // deUint32 binding;
		VK_FORMAT_R32G32_SFLOAT, // VkFormat format;
		0u,						 // deUint32 offsetInBytes;
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
		DE_NULL,												   // const void* pNext;
		0,														   // VkPipelineVertexInputStateCreateFlags	flags;
		1u,														   // deUint32 bindingCount;
		&vertexInputBindingDescription,	  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
		1u,								  // deUint32 attributeCount;
		&vertexInputAttributeDescription, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
		DE_NULL,													 // const void* pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,					 // VkPipelineInputAssemblyStateCreateFlags flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,						 // VkPrimitiveTopology topology;
		VK_FALSE,													 // VkBool32 primitiveRestartEnable;
	};

	const VkViewport viewport{
		0, 0, static_cast<float>(m_testParam->frameBufferSize), static_cast<float>(m_testParam->frameBufferSize), 0, 1
	};
	const VkRect2D scissor{ { 0, 0 }, { m_testParam->frameBufferSize, m_testParam->frameBufferSize } };

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
		DE_NULL,											   // const void* pNext;
		(VkPipelineViewportStateCreateFlags)0,				   // VkPipelineViewportStateCreateFlags flags;
		1u,													   // uint32_t viewportCount;
		&viewport,											   // const VkViewport* pViewports;
		1u,													   // uint32_t scissorCount;
		&scissor,											   // const VkRect2D* pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
		DE_NULL,													// const void* pNext;
		0u,															// VkPipelineRasterizationStateCreateFlags flags;
		VK_FALSE,													// VkBool32 depthClampEnable;
		VK_FALSE,													// VkBool32 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace frontFace;
		VK_FALSE,													// VkBool32 depthBiasEnable;
		0.0f,														// float depthBiasConstantFactor;
		0.0f,														// float depthBiasClamp;
		0.0f,														// float depthBiasSlopeFactor;
		1.0f,														// float lineWidth;
	};

	const VkSampleMask	sampleMask	= getSampleMask(m_testParam->testType);
	const VkSampleMask* pSampleMask = (m_testParam->testType == TestType::MsaaSampleMask) ? &sampleMask : DE_NULL;
	const bool			sampleShadingEnable = (m_testParam->testType != TestType::MsaaSampleMask);

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
		DE_NULL,												  // const void* pNext;
		0u,														  // VkPipelineMultisampleStateCreateFlags flags;
		m_testParam->m_sampleCount,								  // VkSampleCountFlagBits rasterizationSamples;
		sampleShadingEnable,									  // VkBool32 sampleShadingEnable;
		1.0f,													  // float minSampleShading;
		pSampleMask,											  // const VkSampleMask* pSampleMask;
		VK_FALSE,												  // VkBool32 alphaToCoverageEnable;
		VK_FALSE												  // VkBool32 alphaToOneEnable;
	};

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentState(
		getColorAttachmentCount(m_testParam->testType),
		{
			false,												  // VkBool32 blendEnable;
			VK_BLEND_FACTOR_ONE,								  // VkBlend srcBlendColor;
			VK_BLEND_FACTOR_ONE,								  // VkBlend destBlendColor;
			VK_BLEND_OP_ADD,									  // VkBlendOp blendOpColor;
			VK_BLEND_FACTOR_ONE,								  // VkBlend srcBlendAlpha;
			VK_BLEND_FACTOR_ONE,								  // VkBlend destBlendAlpha;
			VK_BLEND_OP_ADD,									  // VkBlendOp blendOpAlpha;
			(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT) // VkChannelFlags channelWriteMask;
		});

	if (disableColor0Write)
	{
		colorBlendAttachmentState[0].colorWriteMask = 0;
	}

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
		DE_NULL,												  // const void* pNext;
		/* always needed */
		0,											// VkPipelineColorBlendStateCreateFlags flags;
		false,										// VkBool32 logicOpEnable;
		VK_LOGIC_OP_COPY,							// VkLogicOp logicOp;
		(deUint32)colorBlendAttachmentState.size(), // deUint32 attachmentCount;
		colorBlendAttachmentState.data(),			// const VkPipelineColorBlendAttachmentState* pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },					// float blendConst[4];
	};

	VkStencilOpState stencilOpState = {
		VK_STENCIL_OP_ZERO,				  // VkStencilOp failOp;
		VK_STENCIL_OP_INCREMENT_AND_WRAP, // VkStencilOp passOp;
		VK_STENCIL_OP_INCREMENT_AND_WRAP, // VkStencilOp depthFailOp;
		VK_COMPARE_OP_ALWAYS,			  // VkCompareOp compareOp;
		0xff,							  // uint32_t compareMask;
		0xff,							  // uint32_t writeMask;
		0,								  // uint32_t reference;
	};

	if (disableStencilWrite)
	{
		stencilOpState.failOp	   = VK_STENCIL_OP_KEEP;
		stencilOpState.passOp	   = VK_STENCIL_OP_KEEP;
		stencilOpState.depthFailOp = VK_STENCIL_OP_KEEP;
	}

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		// VkStructureType sType;
		DE_NULL, // const void* pNext;
		0,
		// VkPipelineDepthStencilStateCreateFlags flags;
		VK_TRUE,			  // VkBool32 depthTestEnable;
		VK_TRUE,			  // VkBool32 depthWriteEnable;
		VK_COMPARE_OP_ALWAYS, // VkCompareOp depthCompareOp;
		VK_FALSE,			  // VkBool32 depthBoundsTestEnable;
		VK_TRUE,			  // VkBool32 stencilTestEnable;
		stencilOpState,		  // VkStencilOpState front;
		stencilOpState,		  // VkStencilOpState back;
		0.0f,				  // float minDepthBounds;
		1.0f,				  // float maxDepthBounds;
	};

	if (disableDepthWrite)
	{
		pipelineDepthStencilStateInfo.depthWriteEnable = VK_FALSE;
	}

	std::vector<VkFormat>				   colorsAttachmentFormats(getColorAttachmentCount(m_testParam->testType),
																   m_testParam->colorFormat);
	const tcu::TextureFormat depthStencilTexFormat = mapVkFormat(m_testParam->depthStencilFormat);
	VkFormat depthFormat = tcu::hasDepthComponent(depthStencilTexFormat.order) ? m_testParam->depthStencilFormat : VK_FORMAT_UNDEFINED;
	VkFormat stencilFormat = tcu::hasStencilComponent(depthStencilTexFormat.order) ? m_testParam->depthStencilFormat : VK_FORMAT_UNDEFINED;
	const VkPipelineRenderingCreateInfoKHR renderingCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,	// VkStructureType sType;
		DE_NULL,												// const void* pNext;
		0u,														// deUint32 viewMask;
		static_cast<deUint32>(colorsAttachmentFormats.size()),	// deUint32 colorAttachmentCount;
		colorsAttachmentFormats.data(),							// const VkFormat* pColorAttachmentFormats;
		depthFormat,											// VkFormat depthAttachmentFormat;
		stencilFormat,											// VkFormat stencilAttachmentFormat;
	};

	const VkPipelineShaderStageCreateInfo pShaderStages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
			DE_NULL,											 // const void*	 pNext;
			(VkPipelineShaderStageCreateFlags)0,				 // VkPipelineShaderStageCreateFlags flags;
			VK_SHADER_STAGE_VERTEX_BIT,							 // VkShaderStageFlagBits stage;
			*m_vertexModule,									 // VkShaderModule module;
			"main",												 // const char* pName;
			DE_NULL,											 // const VkSpecializationInfo* pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
			DE_NULL,											 // const void* pNext;
			(VkPipelineShaderStageCreateFlags)0,				 // VkPipelineShaderStageCreateFlags flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,						 // VkShaderStageFlagBits stage;
			*m_fragmentModule,									 // VkShaderModule module;
			"main",												 // const char* pName;
			DE_NULL,											 // const VkSpecializationInfo* pSpecializationInfo;
		},
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
		&renderingCreateInfo,							 // const void* pNext;
		(VkPipelineCreateFlags)0,						 // VkPipelineCreateFlags flags;
		2u,												 // deUint32 stageCount;
		pShaderStages,									 // const VkPipelineShaderStageCreateInfo* pStages;
		&vertexInputStateParams,		 // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
		&pipelineInputAssemblyStateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
		DE_NULL,						 // const VkPipelineTessellationStateCreateInfo* pTessellationState;
		&pipelineViewportStateInfo,		 // const VkPipelineViewportStateCreateInfo* pViewportState;
		&pipelineRasterizationStateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
		&pipelineMultisampleStateInfo,	 // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
		&pipelineDepthStencilStateInfo,	 // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
		&pipelineColorBlendStateInfo,	 // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
		DE_NULL,						 // const VkPipelineDynamicStateCreateInfo* pDynamicState;
		*m_graphicsPipelineLayout,		 // VkPipelineLayout layout;
		DE_NULL,						 // VkRenderPass renderPass;
		0u,								 // deUint32 subpass;
		DE_NULL,						 // VkPipeline basePipelineHandle;
		0,								 // deInt32 basePipelineIndex;
	};

	return createGraphicsPipeline(m_vk, device, DE_NULL, &graphicsPipelineInfo);
}

void ShaderTileImageTestInstance::generateAttachments()
{
	const VkDevice device	 = m_context.getDevice();
	Allocator&	   allocator = m_context.getDefaultAllocator();

	auto makeImageCreateInfo = [](const VkFormat format, deUint32 imageSize, VkSampleCountFlagBits sampleCount,
								  VkImageUsageFlags usage) -> VkImageCreateInfo
	{
		const VkImageCreateInfo imageParams = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,   // VkStructureType sType;
			DE_NULL,							   // const void* pNext;
			(VkImageCreateFlags)0,				   // VkImageCreateFlags flags;
			VK_IMAGE_TYPE_2D,					   // VkImageType imageType;
			format,								   // VkFormat format;
			makeExtent3D(imageSize, imageSize, 1), // VkExtent3D extent;
			1u,									   // deUint32 mipLevels;
			1u,									   // deUint32 arrayLayers;
			sampleCount,						   // VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,			   // VkImageTiling tiling;
			usage,								   // VkImageUsageFlags usage;
			VK_SHARING_MODE_EXCLUSIVE,			   // VkSharingMode sharingMode;
			0u,									   // deUint32 queueFamilyIndexCount;
			DE_NULL,							   // const deUint32* pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,			   // VkImageLayout initialLayout;
		};
		return imageParams;
	};

	// Color Attachment
	{
		constexpr deUint32		imageBufferPixelSize = sizeof(deUint32) * 2; // always uvec2 type
		const VkImageUsageFlags imageUsage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		const VkDeviceSize imageBufferSize = m_testParam->frameBufferSize * m_testParam->frameBufferSize *
											 imageBufferPixelSize * getSampleCount(m_testParam->m_sampleCount);
		const VkImageSubresourceRange imageSubresource =
			makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const VkImageCreateInfo	 imageInfo = makeImageCreateInfo(m_testParam->colorFormat, m_testParam->frameBufferSize,
																 m_testParam->m_sampleCount, imageUsage);
		const VkBufferCreateInfo bufferInfo = makeBufferCreateInfo(
			imageBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		const deUint32 attachmentCount = getColorAttachmentCount(m_testParam->testType);
		for (deUint32 i = 0; i < attachmentCount; ++i)
		{
			m_imageColor[i]		 = makeImage(m_vk, device, imageInfo);
			m_imageColorAlloc[i] = bindImage(m_vk, device, allocator, *m_imageColor[i], MemoryRequirement::Any);
			m_imageBuffer[i] =
				Draw::Buffer::createAndAlloc(m_vk, device, bufferInfo, allocator, MemoryRequirement::HostVisible);
			m_imageColorView[i] = makeImageView(m_vk, device, *m_imageColor[i], VK_IMAGE_VIEW_TYPE_2D,
												m_testParam->colorFormat, imageSubresource);
		}

		m_imageColorBufferHostPtr = static_cast<deUint32*>(m_imageBuffer[0]->getHostPtr());
	}

	// depth/stencil attachment.
	{
		const tcu::TextureFormat depthStencilFormat = mapVkFormat(m_testParam->depthStencilFormat);
		const VkImageUsageFlags	 imageUsage =
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		VkImageAspectFlags aspect = 0;
		if (tcu::hasDepthComponent(depthStencilFormat.order))
		{
			aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (tcu::hasStencilComponent(depthStencilFormat.order))
		{
			aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		const VkImageCreateInfo imageInfo = makeImageCreateInfo(
			m_testParam->depthStencilFormat, m_testParam->frameBufferSize, m_testParam->m_sampleCount, imageUsage);

		const VkImageSubresourceRange imageSubresource = makeImageSubresourceRange(aspect, 0u, 1u, 0u, 1u);

		m_imageDepthStencil		 = makeImage(m_vk, device, imageInfo);
		m_imageDepthStencilAlloc = bindImage(m_vk, device, allocator, *m_imageDepthStencil, MemoryRequirement::Any);
		m_imageDepthStencilView	 = makeImageView(m_vk, device, *m_imageDepthStencil, VK_IMAGE_VIEW_TYPE_2D,
												 m_testParam->depthStencilFormat, imageSubresource);
	}
}

void ShaderTileImageTestInstance::generateVertexBuffer()
{
	const deUint32		   drawCount		 = getDrawCallCount(m_testParam);
	const deUint32		   patchCountPerDraw = getPatchesPerDrawCount(m_testParam->multiplePatchesPerDraw);
	const deUint32		   queueFamilyIndex	 = m_context.getUniversalQueueFamilyIndex();
	const VkDevice		   device			 = m_context.getDevice();
	Allocator&			   allocator		 = m_context.getDefaultAllocator();
	std::vector<tcu::Vec2> vbo;
	for (deUint32 patchIndex = 0; patchIndex < (patchCountPerDraw * drawCount); patchIndex++)
	{
		// _____
		// |  /
		// | /
		// |/
		vbo.emplace_back(tcu::Vec2(-1, -1));
		vbo.emplace_back(tcu::Vec2(1, 1));
		vbo.emplace_back(tcu::Vec2(-1, 1));

		if (getVertexCountPerPatch(m_testParam) == 6)
		{
			if (isHelperClassTest(m_testParam->testType) && patchIndex == 0)
			{
				// helper class cases render the first patch like follow.
				// _____
				// |  /
				// | /
				// |/
				// So, 3 of second triangle is dummy.
				vbo.emplace_back(tcu::Vec2(-1, -1));
				vbo.emplace_back(tcu::Vec2(-1, -1));
				vbo.emplace_back(tcu::Vec2(-1, -1));
			}
			else
			{
				// Other 6 vertices cases render like follow
				// _____
				// |  /|
				// | / |
				// |/__|
				vbo.emplace_back(tcu::Vec2(-1, -1));
				vbo.emplace_back(tcu::Vec2(1, -1));
				vbo.emplace_back(tcu::Vec2(1, 1));
			}
		}
	}

	const size_t dataSize = vbo.size() * sizeof(tcu::Vec2);
	{
		const VkBufferCreateInfo bufferInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
			DE_NULL,							  // const void* pNext;
			0u,									  // VkBufferCreateFlags flags;
			dataSize,							  // VkDeviceSize size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,	  // VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,			  // VkSharingMode sharingMode;
			1u,									  // deUint32 queueFamilyCount;
			&queueFamilyIndex					  // const deUint32* pQueueFamilyIndices;
		};
		m_vertexBuffer =
			Draw::Buffer::createAndAlloc(m_vk, device, bufferInfo, allocator, MemoryRequirement::HostVisible);
	}

	/* Load vertices into vertex buffer */
	deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), vbo.data(), dataSize);
	flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());
}
void ShaderTileImageTestInstance::generateCmdBuffer()
{
	const VkDevice device = m_context.getDevice();

	m_cmdPool	= createCommandPool(m_vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
									m_context.getUniversalQueueFamilyIndex());
	m_cmdBuffer = allocateCommandBuffer(m_vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

tcu::TestStatus ShaderTileImageTestInstance::iterate()
{
	rendering();
	return checkResult();
}

deUint32 ShaderTileImageTestInstance::getResultValue(deUint32 fx, deUint32 fy, deUint32 fs,
													 deUint32 renderTargetID) const
{
	const deUint32* resultData =
		static_cast<const deUint32*>(m_imageBuffer[renderTargetID]->getBoundMemory().getHostPtr());
	const deUint32 sampleCount = getSampleCount(m_testParam->m_sampleCount);
	const deUint32 index	   = (((fy * m_testParam->frameBufferSize) + fx) * sampleCount + fs) * 2; // 2 is for xy
	if (resultData[index] != 0)																		  // error
	{
		return 0xFFFFFFFF;
	}

	return resultData[index + 1]; // y value
}

deUint32 ShaderTileImageTestInstance::simulate(deUint32 fx, deUint32 fy, deUint32 fs, deUint32 renderTargetID) const
{
	const deUint32 totalLayerCount =
		getDrawCallCount(m_testParam) * getPatchesPerDrawCount(m_testParam->multiplePatchesPerDraw);

	if (m_testParam->testType == TestType::MsaaSampleMask)
	{
		deUint32 expectedValue = 0;

		if (((getSampleMask(m_testParam->testType) >> fs) & 0x1) == 0x1)
		{
			expectedValue = totalLayerCount + renderTargetID;
		}
		return expectedValue;
	}
	if (m_testParam->testType == TestType::Stencil)
	{
		// stencil test doesn't add fragment sample ID to the output;
		const deUint32 expectedValue = totalLayerCount + renderTargetID;
		return expectedValue;
	}
	if (isHelperClassTest(m_testParam->testType))
	{
		// ________      ________      ________
		// 1|1|1|0|      0|0|*|1|      1|1|#|2|
		// 1|1|0|0|      0|0|1|*|      1|1|2|#|
		// 1|0|0|0|  =>  *|1|0|0|  =>  #|2|1|1|
		// 0|0|0|0|      1|*|0|0|      2|#|1|1|
		// ________      ________      ________
		// raster       max(dx,dy)    result(+1)
		// *(#): max(dx, dy) could be 0(1) or 1(2).
		if ((fx) == (fy))
		{
			return kDerivative1; // derivative is 1 because of coverage. (+1) for differentiate clear value
		}
		else
		{
			return kDerivative0; // 0, fill all or fill none for quad. (+1) for differentiate clear value
		}
	}
	else
	{
		const deUint32 expectedValue = totalLayerCount + renderTargetID + fs;
		return expectedValue;
	}
}

tcu::TestStatus ShaderTileImageTestInstance::checkResult() const
{
	const VkDevice device = m_context.getDevice();

	qpTestResult   res				   = QP_TEST_RESULT_PASS;
	const deUint32 sampleCount		   = getSampleCount(m_testParam->m_sampleCount);
	const deUint32 attachmentCount	   = getColorAttachmentCount(m_testParam->testType);
	const deUint32 vertexCountPerPatch = getVertexCountPerPatch(m_testParam);
	// Loop over all samples in the same fragment

	for (deUint32 rt = 0; (res == QP_TEST_RESULT_PASS) && rt < attachmentCount; rt++)
	{
		// Result of Helper Class test valid only for the rt 1
		invalidateAlloc(m_vk, device, m_imageBuffer[rt]->getBoundMemory());

		if (rt != 1 && isHelperClassTest(m_testParam->testType))
		{
			continue;
		}

		for (deUint32 fy = 0; (res == QP_TEST_RESULT_PASS) && fy < m_testParam->frameBufferSize; ++fy)
		{
			for (deUint32 fx = 0; (res == QP_TEST_RESULT_PASS) && fx < m_testParam->frameBufferSize; ++fx)
			{
				for (deUint32 fs = 0; (res == QP_TEST_RESULT_PASS) && fs < sampleCount; ++fs)
				{
					const deUint32 expectedValue = simulate(fx, fy, fs, rt);
					const deUint32 resultValue	 = getResultValue(fx, fy, fs, rt);

					if (isHelperClassTest(m_testParam->testType))
					{
						// ________      ________      ________
						// 1|1|1|0|      0|0|*|1|      1|1|#|2|
						// 1|1|0|0|      0|0|1|*|      1|1|2|#|
						// 1|0|0|0|  =>  *|1|0|0|  =>  #|2|1|1|
						// 0|0|0|0|      1|*|0|0|      2|#|1|1|
						// ________      ________      ________
						// raster       max(dx,dy)    result(+1)
						// *(#): max(dx, dy) could be 0(1) or 1(2).
						if (expectedValue != resultValue)
						{
							if (std::abs(static_cast<deInt32>(fx - fy)) != 1 || resultValue != kDerivative1)
							{
								res = QP_TEST_RESULT_FAIL;
								break;
							}
						}
					}
					else if (vertexCountPerPatch == 6) // Fill full quad to the framebuffer
					{
						if (expectedValue != resultValue)
						{
							res = QP_TEST_RESULT_FAIL;
							break;
						}
					}
					else // Fill a triangle to the framebuffer, check half of framebuffer
					{
						if (fy > fx) // inside of triangle
						{
							if (expectedValue != resultValue) // not expected value
							{
								res = QP_TEST_RESULT_FAIL;
								break;
							}
						}
						else // outside of filling triangle or triangle edge
						{
							if (resultValue != 0 && resultValue != expectedValue) // can be not filling
							{
								res = QP_TEST_RESULT_FAIL;
								break;
							}
						}
					}
				}
			}
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void ShaderTileImageTestInstance::rendering()
{
	const VkDevice device = m_context.getDevice();
	const VkQueue  queue  = m_context.getUniversalQueue();

	beginCommandBuffer(m_vk, *m_cmdBuffer);

	// begin render pass
	const VkClearValue clearValue	= {}; // { 0, 0, 0, 0 }
	const VkClearValue dsClearValue = {}; // .depth = 0.0f, .stencil = 0
	const VkRect2D	   renderArea	= { { 0, 0 }, { m_testParam->frameBufferSize, m_testParam->frameBufferSize } };

	const deUint32 colorAttachmentCount = getColorAttachmentCount(m_testParam->testType);

	std::vector<VkRenderingAttachmentInfoKHR> colorAttachments;
	for (deUint32 colorIndex = 0; colorIndex < colorAttachmentCount; colorIndex++)
	{
		const VkRenderingAttachmentInfoKHR renderingAtachInfo = {
			VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
			DE_NULL,										 // const void* pNext;
			*m_imageColorView[colorIndex],					 // VkImageView imageView;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		 // VkImageLayout imageLayout;
			VK_RESOLVE_MODE_NONE,							 // VkResolveModeFlagBits resolveMode;
			DE_NULL,										 // VkImageView resolveImageView;
			VK_IMAGE_LAYOUT_UNDEFINED,						 // VkImageLayout resolveImageLayout;
			VK_ATTACHMENT_LOAD_OP_CLEAR,					 // VkAttachmentLoadOp loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,					 // VkAttachmentStoreOp storeOp;
			clearValue,										 // VkClearValue clearValue;
		};

		colorAttachments.push_back(renderingAtachInfo);
	}

	const tcu::TextureFormat depthStencilFormat = mapVkFormat(m_testParam->depthStencilFormat);
	const bool				 hasDepth			= tcu::hasDepthComponent(depthStencilFormat.order);
	const bool				 hasStencil			= tcu::hasStencilComponent(depthStencilFormat.order);
	VkImageLayout			 depthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageAspectFlags		 depthStencilAspect = 0;
	if (hasDepth && hasStencil)
	{
		depthStencilLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthStencilAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else if (hasDepth)
	{
		depthStencilLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthStencilAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else if (hasStencil)
	{
		depthStencilLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
		depthStencilAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	const VkRenderingAttachmentInfoKHR depthStencilAttachment = {
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType			sType;
		DE_NULL,										 // const void*				pNext;
		*m_imageDepthStencilView,						 // VkImageView				imageView;
		depthStencilLayout,								 // VkImageLayout			imageLayout;
		VK_RESOLVE_MODE_NONE,							 // VkResolveModeFlagBits	resolveMode;
		DE_NULL,										 // VkImageView				resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,						 // VkImageLayout			resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,					 // VkAttachmentLoadOp		loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,					 // VkAttachmentStoreOp		storeOp;
		dsClearValue,									 // VkClearValue				clearValue;
	};

	const VkRenderingInfoKHR renderingInfo = {
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,			// VkStructureType sType;
		DE_NULL,										// const void* pNext;
		0,												// VkRenderingFlagsKHR flags;
		renderArea,										// VkRect2D renderArea;
		1u,												// deUint32 layerCount;
		0u,												// deUint32 viewMask;
		static_cast<deUint32>(colorAttachments.size()), // deUint32 colorAttachmentCount;
		colorAttachments.data(),						// const VkRenderingAttachmentInfoKHR* pColorAttachments;
		hasDepth ? &depthStencilAttachment : DE_NULL,	// const VkRenderingAttachmentInfoKHR* pDepthAttachment;
		hasStencil ? &depthStencilAttachment : DE_NULL	// const VkRenderingAttachmentInfoKHR* pStencilAttachment;
	};

	for (deUint32 colorIndex = 0; colorIndex < colorAttachmentCount; colorIndex++)
	{
		transition2DImage(m_vk, *m_cmdBuffer, *m_imageColor[colorIndex], VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	transition2DImage(m_vk, *m_cmdBuffer, *m_imageDepthStencil, depthStencilAspect, VK_IMAGE_LAYOUT_UNDEFINED,
					  depthStencilLayout, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	m_vk.cmdBeginRendering(*m_cmdBuffer, &renderingInfo);

	// vertex input setup
	const VkBuffer vertexBuffer = m_vertexBuffer->object();

	for (deUint32 drawIndex = 0; drawIndex < getDrawCallCount(m_testParam); drawIndex++)
	{
		// pipeline setup
		if (drawIndex == 1 && isHelperClassTest(m_testParam->testType))
		{
			m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineForHelperClass);
		}
		else
		{
			m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
		}

		const deUint32 vertexCountPerPatch = getVertexCountPerPatch(m_testParam);
		const deUint32 vertexCount = vertexCountPerPatch * getPatchesPerDrawCount(m_testParam->multiplePatchesPerDraw);
		m_vk.cmdPushConstants(*m_cmdBuffer, *m_graphicsPipelineLayout,
							  (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT), 0, sizeof(deUint32),
							  &drawIndex);

		const VkDeviceSize vertexBufferOffset = (vertexCount * drawIndex) * sizeof(tcu::Vec2);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

		if (!m_testParam->coherent)
		{
			VkMemoryBarrier2KHR memoryBarrierForColor = {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,				// sType
				DE_NULL,											// pNext
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,// srcStageMask
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,			// srcAccessMask
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,// dstStageMask
				VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR			// dstAccessMask
			};

			VkMemoryBarrier2KHR memoryBarrierForDepthStencil = {
				VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,				// sType
				DE_NULL,											// pNext
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
					VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,	// srcStageMask
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR, // srcAccessMask
				VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
					VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT ,	// dstStageMask
				VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR	// dstAccessMask
			};

			VkMemoryBarrier2KHR* memoryBarrier =
				(m_testParam->testType == TestType::Depth) || (m_testParam->testType == TestType::Stencil) ?
					&memoryBarrierForDepthStencil :
					&memoryBarrierForColor;

			VkDependencyInfoKHR dependencyInfo{
				VK_STRUCTURE_TYPE_DEPENDENCY_INFO, // sType
				DE_NULL,						   // pNext
				VK_DEPENDENCY_BY_REGION_BIT,	   //dependency flags
				1,								   //memory barrier count
				memoryBarrier,					   //memory barrier
				0,								   // bufferMemoryBarrierCount
				DE_NULL,						   // pBufferMemoryBarriers
				0,								   // imageMemoryBarrierCount
				DE_NULL,						   // pImageMemoryBarriers
			};
			m_vk.cmdPipelineBarrier2(*m_cmdBuffer, &dependencyInfo);
		}

		m_vk.cmdDraw(*m_cmdBuffer, vertexCount, 1, 0, 0u);
	}
	m_vk.cmdEndRendering(*m_cmdBuffer);

	for (deUint32 colorIndex = 0; colorIndex < colorAttachmentCount; colorIndex++)
	{

		transition2DImage(m_vk, *m_cmdBuffer, *m_imageColor[colorIndex], VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
						  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	VkMemoryBarrier memBarrier = {
		VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
		DE_NULL,						  // pNext
		0u,								  // srcAccessMask
		0u,								  // dstAccessMask
	};
	memBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	m_vk.cmdPipelineBarrier(*m_cmdBuffer,
							VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
							VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);

	// Copy color images to buffer memory
	for (deUint32 attachmentIndex = 0; attachmentIndex < colorAttachmentCount; attachmentIndex++)
	{
		m_vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipelineLayout, 0u, 1,
								   &*m_descriptorSets[attachmentIndex], 0u, DE_NULL);

		m_vk.cmdDispatch(*m_cmdBuffer, m_testParam->frameBufferSize, m_testParam->frameBufferSize, 1);
	}
	memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	m_vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
							&memBarrier, 0, DE_NULL, 0, DE_NULL);

	VK_CHECK(m_vk.endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
}

std::string formatToName(VkFormat format)
{
	const std::string formatStr = de::toString(format);
	const std::string prefix	= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

void createShaderTileImageTestVariations(tcu::TestContext& testCtx, tcu::TestCaseGroup* gr)
{
	struct TestTypeParam
	{
		TestType	value;
		const char* name;
	};

	struct BoolParam
	{
		bool		value;
		const char* name;
	};

	struct VkSampleCountFlagParam
	{
		VkSampleCountFlagBits value;
		const char*			  name;
	};

	const std::vector<BoolParam> coherentParams = { { true, "coherent" }, { false, "non_coherent" } };

	const std::vector<TestTypeParam> testTypeParams = {
		{ TestType::Color, "color" },
		{ TestType::MultiRenderTarget, "mrt" },
		{ TestType::MultiRenderTargetDynamicIndex, "mrt_dynamic_index" },
		{ TestType::MsaaSampleMask, "msaa_sample_mask" },
		{ TestType::HelperClassColor, "helper_class_color" },
		{ TestType::HelperClassDepth, "helper_class_depth" },
		{ TestType::HelperClassStencil, "helper_class_stencil" },
		{ TestType::Depth, "depth" },
		{ TestType::Stencil, "stencil" },
	};

	const std::vector<VkSampleCountFlagParam> sampleCountParams = {
		{ VK_SAMPLE_COUNT_1_BIT, "samples_1" },	  { VK_SAMPLE_COUNT_2_BIT, "samples_2" },
		{ VK_SAMPLE_COUNT_4_BIT, "samples_4" },	  { VK_SAMPLE_COUNT_8_BIT, "samples_8" },
		{ VK_SAMPLE_COUNT_16_BIT, "samples_16" }, { VK_SAMPLE_COUNT_32_BIT, "samples_32" },
	};

	const std::vector<BoolParam> multiDrawsParams = { { false, "single_draw" }, { true, "multi_draws" } };

	const std::vector<BoolParam> multiPatchParams = { { false, "single_patch" }, { true, "multi_patches" } };

	const std::vector<VkFormat> formats = { VK_FORMAT_R5G6B5_UNORM_PACK16,
											VK_FORMAT_R8G8_UNORM,
											VK_FORMAT_R8G8_SNORM,
											VK_FORMAT_R8G8_UINT,
											VK_FORMAT_R8G8_SINT,
											VK_FORMAT_R8G8B8A8_UNORM,
											VK_FORMAT_R8G8B8A8_SNORM,
											VK_FORMAT_R8G8B8A8_UINT,
											VK_FORMAT_R8G8B8A8_SINT,
											VK_FORMAT_R8G8B8A8_SRGB,
											VK_FORMAT_A8B8G8R8_UNORM_PACK32,
											VK_FORMAT_A8B8G8R8_SNORM_PACK32,
											VK_FORMAT_A8B8G8R8_UINT_PACK32,
											VK_FORMAT_A8B8G8R8_SINT_PACK32,
											VK_FORMAT_A8B8G8R8_SRGB_PACK32,
											VK_FORMAT_B8G8R8A8_UNORM,
											VK_FORMAT_B8G8R8A8_SRGB,
											VK_FORMAT_A2R10G10B10_UNORM_PACK32,
											VK_FORMAT_A2B10G10R10_UNORM_PACK32,
											VK_FORMAT_A2B10G10R10_UINT_PACK32,
											VK_FORMAT_R16G16_UNORM,
											VK_FORMAT_R16G16_SNORM,
											VK_FORMAT_R16G16_UINT,
											VK_FORMAT_R16G16_SINT,
											VK_FORMAT_R16G16_SFLOAT,
											VK_FORMAT_R16G16B16A16_UNORM,
											VK_FORMAT_R16G16B16A16_SNORM,
											VK_FORMAT_R16G16B16A16_UINT,
											VK_FORMAT_R16G16B16A16_SINT,
											VK_FORMAT_R16G16B16A16_SFLOAT,
											VK_FORMAT_R32G32_UINT,
											VK_FORMAT_R32G32_SINT,
											VK_FORMAT_R32G32_SFLOAT,
											VK_FORMAT_R32G32B32A32_UINT,
											VK_FORMAT_R32G32B32A32_SINT,
											VK_FORMAT_R32G32B32A32_SFLOAT,
											VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,

											VK_FORMAT_D16_UNORM,
											VK_FORMAT_X8_D24_UNORM_PACK32,
											VK_FORMAT_D32_SFLOAT,
											VK_FORMAT_S8_UINT,
											VK_FORMAT_D16_UNORM_S8_UINT,
											VK_FORMAT_D24_UNORM_S8_UINT,
											VK_FORMAT_D32_SFLOAT_S8_UINT };

	tcu::TestCaseGroup*				 subGroup = nullptr;
	std::vector<tcu::TestCaseGroup*> testGroupStack;
	testGroupStack.push_back(gr);

	for (const BoolParam& coherentParam : coherentParams)
	{
		subGroup = (new tcu::TestCaseGroup(testCtx, coherentParam.name, coherentParam.name));
		testGroupStack.back()->addChild(subGroup);
		testGroupStack.push_back(subGroup);
		for (const TestTypeParam& testTypeParam : testTypeParams)
		{
			subGroup = new tcu::TestCaseGroup(testCtx, testTypeParam.name, testTypeParam.name);
			testGroupStack.back()->addChild(subGroup);
			testGroupStack.push_back(subGroup);

			for (const VkSampleCountFlagParam& sampleCountParam : sampleCountParams)
			{
				if (testTypeParam.value == TestType::MsaaSampleMask && sampleCountParam.value == VK_SAMPLE_COUNT_1_BIT)
				{
					// SampleMask test requires MSAA
					continue;
				}
				if (isHelperClassTest(testTypeParam.value) && sampleCountParam.value != VK_SAMPLE_COUNT_1_BIT)
				{
					// HelperClass test designed for non msaa case
					continue;
				}
				subGroup = new tcu::TestCaseGroup(testCtx, sampleCountParam.name, sampleCountParam.name);
				testGroupStack.back()->addChild(subGroup);
				testGroupStack.push_back(subGroup);

				for (const BoolParam& multiDrawsParam : multiDrawsParams)
				{
					if (isHelperClassTest(testTypeParam.value) && multiDrawsParam.value)
					{
						// helper class 2 draws but works like single draw call
						continue;
					}

					subGroup = new tcu::TestCaseGroup(testCtx, multiDrawsParam.name, multiDrawsParam.name);
					testGroupStack.back()->addChild(subGroup);
					testGroupStack.push_back(subGroup);

					for (const BoolParam& multiPatchParam : multiPatchParams)
					{
						if (!coherentParam.value && multiPatchParam.value) // cannot guarantee
						{
							continue;
						}
						if (isHelperClassTest(testTypeParam.value) && multiPatchParam.value)
						{
							// helper class works on single patch cases
							continue;
						}

						subGroup = new tcu::TestCaseGroup(testCtx, multiPatchParam.name, multiPatchParam.name);
						testGroupStack.back()->addChild(subGroup);
						testGroupStack.push_back(subGroup);

						for (VkFormat format : formats)
						{
							tcu::TestCaseGroup* curGroup   = testGroupStack.back();
							const bool			hasDepth   = tcu::hasDepthComponent(mapVkFormat(format).order);
							const bool			hasStencil = tcu::hasStencilComponent(mapVkFormat(format).order);
							std::string			name	   = formatToName(format);

							TestParam testParam				 = {};
							testParam.coherent				 = coherentParam.value;
							testParam.testType				 = testTypeParam.value;
							testParam.colorFormat			 = VK_FORMAT_R32G32B32A32_UINT;
							testParam.depthStencilFormat	 = VK_FORMAT_D32_SFLOAT_S8_UINT;
							testParam.m_sampleCount			 = sampleCountParam.value;
							testParam.multipleDrawCalls		 = multiDrawsParam.value;
							testParam.multiplePatchesPerDraw = multiPatchParam.value;
							testParam.frameBufferSize		 = kImageSize;
							if (testTypeParam.value == TestType::Depth ||
								testTypeParam.value == TestType::HelperClassDepth)
							{
								if (hasDepth)
								{
									testParam.depthStencilFormat = format;
									curGroup->addChild(new ShaderTileImageTestCase(testCtx, name, name, testParam));
								}
							}
							else if (testTypeParam.value == TestType::Stencil ||
									 testTypeParam.value == TestType::HelperClassStencil)
							{
								if (hasStencil)
								{
									testParam.depthStencilFormat = format;
									curGroup->addChild(new ShaderTileImageTestCase(testCtx, name, name, testParam));
								}
							}
							else
							{
								if (!hasStencil && !hasDepth)
								{
									if (isHelperClassTest(testTypeParam.value) && isNormalizedColorFormat(format))
									{
										// reduce helper class test cases and complexities
										continue;
									}

									const deUint32 maxResultValue =
										(getDrawCallCount(&testParam) *
											 getPatchesPerDrawCount(testParam.multiplePatchesPerDraw) *
											 getColorAttachmentCount(testParam.testType) +
										 getSampleCount(testParam.m_sampleCount));
									const tcu::IVec4 channelBitDepth =
										tcu::getTextureFormatBitDepth(mapVkFormat(format));

									// color output precision is less than test case.
									// ban the overflow problem.
									if (static_cast<deUint32>(1 << (channelBitDepth.y() - 1)) > maxResultValue)
									{
										testParam.colorFormat = format;
										curGroup->addChild(new ShaderTileImageTestCase(testCtx, name, name, testParam));
									}
								}
							}
						} // formats
						testGroupStack.pop_back();
					} // multiPatchParams
					testGroupStack.pop_back();
				} // multiDrawsParams
				testGroupStack.pop_back();
			} // sampleCountParams
			testGroupStack.pop_back();
		} // testTypeParams
		testGroupStack.pop_back();
	} // coherentParams
}
} // namespace
// anonymous namespace

tcu::TestCaseGroup* createShaderTileImageTests(tcu::TestContext& testCtx)
{
	/* Add the color tests */
	tcu::TestCaseGroup* gr = new tcu::TestCaseGroup(testCtx, "shader_tile_image", "Shader Tile Image tests");
	createShaderTileImageTestVariations(testCtx, gr);

	return gr;
}

} // namespace rasterization
} // namespace vkt
