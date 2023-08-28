/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2022 The Khronos Group Inc.
* Copyright (c) 2022 Valve Corporation.
* Copyright (c) 2023 LunarG, Inc.
* Copyright (c) 2023 Nintendo
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
*//*
 * \file
 * \brief Tests involving dynamic patch control points
*//*--------------------------------------------------------------------*/

#include "vktPipelineDynamicControlPoints.hpp"
#include "vktTestCase.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "vktPipelineImageUtil.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMaybe.hpp"
#include "tcuImageCompare.hpp"
#include "tcuDefs.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <vector>
#include <sstream>
#include <algorithm>
#include <utility>
#include <iterator>
#include <string>
#include <limits>
#include <memory>
#include <functional>
#include <cstddef>
#include <set>

namespace vkt
{
namespace pipeline
{

struct TestConfig {
	vk::PipelineConstructionType constructionType;
	bool changeOutput;
	bool firstClockwise;
	bool secondClockwise;
	vk::VkCullModeFlags cullMode;
	tcu::Vec4 expectedFirst;
	tcu::Vec4 expectedSecond;
};

class DynamicControlPointsTestCase : public vkt::TestCase
{
public:
	DynamicControlPointsTestCase(tcu::TestContext& context, const std::string& name, const std::string& description, TestConfig config);
	void            initPrograms            (vk::SourceCollections& programCollection) const override;
	TestInstance*   createInstance          (Context& context) const override;
	void            checkSupport            (Context& context) const override;

private:
	TestConfig m_config;
};


class DynamicControlPointsTestInstance : public vkt::TestInstance
{
public:
	DynamicControlPointsTestInstance(Context& context, TestConfig config);
	virtual tcu::TestStatus iterate (void);
private:
	TestConfig m_config;
};


DynamicControlPointsTestCase::DynamicControlPointsTestCase(tcu::TestContext& context, const std::string& name, const std::string& description,
	TestConfig config) : vkt::TestCase (context, name, description), m_config(config)
{
}

void DynamicControlPointsTestCase::checkSupport(Context& context) const
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_config.constructionType);
	const auto& eds2Features  = context.getExtendedDynamicState2FeaturesEXT();
	if (!eds2Features.extendedDynamicState2PatchControlPoints) {
		TCU_THROW(NotSupportedError, "Dynamic patch control points aren't supported");
	}
}

void DynamicControlPointsTestCase::initPrograms(vk::SourceCollections& collection) const
{
	const std::string firstWinding = m_config.firstClockwise ? "cw" : "ccw";
	const std::string secondWinding = m_config.secondClockwise ? "cw" : "ccw";

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "vec2 positions[6] = vec2[](\n"
		<< "		vec2(-1.0, -1.0),"
		<< "		vec2(-1.0, 1.0),"
		<< "		vec2(1.0, -1.0),"
		<< "		vec2(1.0, -1.0),"
		<< "		vec2(-1.0, 1.0),"
		<< "		vec2(1.0, 1.0)"
		<< ");\n"
		<< "void main() {\n"
		<< "		gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
		<< "}";
		collection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(location = 0) out vec4 outColor;\n"
		<< "layout(location = 0) in vec3 fragColor;"
		<< "void main() {\n"
		<< "	outColor = vec4(fragColor, 1.0);\n"
		<< "}";
		collection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(vertices = 3) out;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    gl_TessLevelInner[0] = 2.0;\n"
		<< "    gl_TessLevelOuter[0] = 2.0;\n"
		<< "    gl_TessLevelOuter[1] = 2.0;\n"
		<< "    gl_TessLevelOuter[2] = 2.0;\n"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "}\n";
		collection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(triangles, " << firstWinding << ") in;\n"
		<< "layout(location = 0) out vec3 fragColor;"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
		<< "    vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
		<< "    vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
		<< "    gl_Position = p0 + p1 + p2;\n"
		<< "    fragColor = vec3(1.0, 0.0, 1.0); "
		<< "}\n";
		collection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(vertices = " << (m_config.changeOutput ? 4 : 3) <<  ") out;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    gl_TessLevelInner[0] = 2;\n"
		<< "    gl_TessLevelOuter[0] = 2.0;\n"
		<< "    gl_TessLevelOuter[1] = 2.0;\n"
		<< "    gl_TessLevelOuter[2] = 2.0;\n"
		<< "if (gl_InvocationID < 3) {"
		<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		<< "} else {"
		<< "    gl_out[gl_InvocationID].gl_Position = vec4(1.0, 0.0, 1.0, 1.0);"
		<< "}"
		<< "}\n";
		collection.glslSources.add("tesc2") << glu::TessellationControlSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
		<< "layout(triangles, " << secondWinding << ") in;\n"
		<< "layout(location = 0) out vec3 fragColor;"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
		<< "    vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
		<< "    vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
		<< "    gl_Position = p0 + p1 + p2;\n";
		if (m_config.changeOutput) {
			src << "    fragColor = vec3(gl_in[3].gl_Position.xyz);";
		} else {
			src << "    fragColor = vec3(1.0, 0.0, 1.0);";
		}
		src << "}\n";
		collection.glslSources.add("tese2") << glu::TessellationEvaluationSource(src.str());
	}
}

TestInstance* DynamicControlPointsTestCase::createInstance(Context& context) const
{
	return new DynamicControlPointsTestInstance(context, m_config);
}

DynamicControlPointsTestInstance::DynamicControlPointsTestInstance(Context& context, TestConfig config) :
	vkt::TestInstance (context), m_config (config) { }

//make a buffer to read an image back after rendering
std::unique_ptr<vk::BufferWithMemory> makeBufferForImage(const vk::DeviceInterface& vkd, const vk::VkDevice device, vk::Allocator& allocator, tcu::TextureFormat tcuFormat, vk::VkExtent3D imageExtent)
{
	const auto	outBufferSize		= static_cast<vk::VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * imageExtent.width * imageExtent.height);
	const auto	outBufferUsage	= vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto	outBufferInfo		= makeBufferCreateInfo(outBufferSize, outBufferUsage);
	auto outBuffer = std::unique_ptr<vk::BufferWithMemory>(new vk::BufferWithMemory(vkd, device, allocator, outBufferInfo, vk::MemoryRequirement::HostVisible));

	return outBuffer;
}

tcu::TestStatus DynamicControlPointsTestInstance::iterate(void)
{
	const auto& vki				= m_context.getInstanceInterface();
	const auto& vkd				= m_context.getDeviceInterface();
	const auto physicalDevice	= m_context.getPhysicalDevice();
	const auto  device			= m_context.getDevice();
	auto& alloc					= m_context.getDefaultAllocator();
	auto imageFormat			= vk::VK_FORMAT_R8G8B8A8_UNORM;
	auto imageExtent			= vk::makeExtent3D(4, 4, 1u);
	auto mappedFormat			= mapVkFormat(imageFormat);

	const tcu::IVec3 imageDim	(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), static_cast<int>(imageExtent.depth));
	const tcu::IVec2 imageSize	(imageDim.x(), imageDim.y());

	de::MovePtr<vk::ImageWithMemory>  colorAttachment;

	vk::GraphicsPipelineWrapper pipeline1(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_config.constructionType);
	vk::GraphicsPipelineWrapper pipeline2(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_config.constructionType);
	const auto  qIndex      = m_context.getUniversalQueueFamilyIndex();

	const auto  imageUsage      = static_cast<vk::VkImageUsageFlags>(vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const vk::VkImageCreateInfo imageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType				sType;
		nullptr,									//	const void*					pNext;
		0u,											//	VkImageCreateFlags			flags;
		vk::VK_IMAGE_TYPE_2D,						//	VkImageType					imageType;
		imageFormat,								//	VkFormat					format;
		imageExtent,								//	VkExtent3D					extent;
		1u,											//	deUint32					mipLevels;
		1u,											//	deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits		samples;
		vk::VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling				tiling;
		imageUsage,									//	VkImageUsageFlags			usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode				sharingMode;
		0,											//	deUint32					queueFamilyIndexCount;
		nullptr,									//	const deUint32*				pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout				initialLayout;
	};

	const auto subresourceRange			= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	colorAttachment						= de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vkd, device, alloc, imageCreateInfo, vk::MemoryRequirement::Any));
	auto colorAttachmentView			= vk::makeImageView(vkd, device, colorAttachment->get(), vk::VK_IMAGE_VIEW_TYPE_2D, imageFormat, subresourceRange);

	vk::RenderPassWrapper renderPass	(m_config.constructionType, vkd, device, imageFormat);
	renderPass.createFramebuffer(vkd, device, **colorAttachment, colorAttachmentView.get(), imageExtent.width, imageExtent.height);

	//buffer to read the output image
	auto outBuffer = makeBufferForImage(vkd, device, alloc, mappedFormat, imageExtent);
	auto&		outBufferAlloc	= outBuffer->getAllocation();
	void*		outBufferData		= outBufferAlloc.getHostPtr();

	const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = vk::initVulkanStructure();

	const std::vector<vk::VkViewport>	viewport_left	{ vk::makeViewport(0.0f, 0.0f, (float)imageExtent.width / 2, (float)imageExtent.height, 0.0f, 1.0f) };
	const std::vector<vk::VkViewport>	viewport_right  { vk::makeViewport((float)imageExtent.width / 2, 0.0f, (float)imageExtent.width / 2, (float)imageExtent.height, 0.0f, 1.0f) };
	const std::vector<vk::VkRect2D>		scissors_left	{ vk::makeRect2D(0.0f, 0.0f, imageExtent.width / 2, imageExtent.height) };
	const std::vector<vk::VkRect2D>		scissors_right	{ vk::makeRect2D(imageExtent.width / 2, 0.0, imageExtent.width / 2, imageExtent.height) };
	const vk::PipelineLayoutWrapper		graphicsPipelineLayout (m_config.constructionType, vkd, device);

	auto vtxshader  = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"));
	auto frgshader  = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"));
	auto tscshader1 = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tesc"));
	auto tscshader2 = vk::ShaderWrapper(vkd, device,
		m_config.changeOutput ? m_context.getBinaryCollection().get("tesc2") : m_context.getBinaryCollection().get("tesc"));
	auto tseshader1 = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tese"));
	auto tseshader2 = vk::ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tese2"));

	vk::VkPipelineRasterizationStateCreateInfo raster = {
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,															// const void*									pNext
		0u,																	// VkPipelineRasterizationStateCreateFlags		flags
		VK_FALSE,															// VkBool32										depthClampEnable
		VK_FALSE,															// VkBool32										rasterizerDiscardEnable
		vk::VK_POLYGON_MODE_FILL,											// VkPolygonMode								polygonMode
		m_config.cullMode,													// VkCullModeFlags								cullMode
		vk::VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace									frontFace
		VK_FALSE,															// VkBool32										depthBiasEnable
		0.0f,																// float										depthBiasConstantFactor
		0.0f,																// float										depthBiasClamp
		0.0f,																// float										depthBiasSlopeFactor
		1.0f																// float										lineWidth
	};

	vk::VkDynamicState dynamicStates = vk::VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT;
		vk::VkPipelineDynamicStateCreateInfo dynamicInfo = {
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&dynamicStates
	};

	pipeline1.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
		.setDynamicState(&dynamicInfo)
		.setDefaultRasterizationState()
		.setDefaultMultisampleState()
		.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setupVertexInputState(&vertexInputState)
		.setupPreRasterizationShaderState(
			viewport_left,
			scissors_left,
			graphicsPipelineLayout,
			*renderPass,
			0u,
			vtxshader, &raster,
			tscshader1, tseshader1)
		.setupFragmentShaderState(graphicsPipelineLayout, *renderPass, 0u,
			frgshader, 0)
		.setupFragmentOutputState(*renderPass, 0u)
		.setMonolithicPipelineLayout(graphicsPipelineLayout).buildPipeline();

	pipeline2.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
		.setDynamicState(&dynamicInfo)
		.setDefaultRasterizationState()
		.setDefaultMultisampleState()
		.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setupVertexInputState(&vertexInputState)
		.setupPreRasterizationShaderState(
			viewport_right,
			scissors_right,
			graphicsPipelineLayout,
			*renderPass,
			0u,
			vtxshader, &raster,
			tscshader2, tseshader2)
		.setupFragmentShaderState(graphicsPipelineLayout, *renderPass, 0u,
			frgshader, 0)
		.setupFragmentOutputState(*renderPass, 0u)
		.setMonolithicPipelineLayout(graphicsPipelineLayout).buildPipeline();

	auto commandPool = createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, qIndex);
	auto commandBuffer = vk::allocateCommandBuffer(vkd, device, commandPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const tcu::Vec4 clearColor(1.0f, 1.0f, 1.0f, 1.0f);

	const vk::VkRect2D renderArea =
	{
		{ 0u, 0u },
		{ imageExtent.width, imageExtent.height }
	};

	//render 2 triangles with each pipeline, covering the entire screen
	//depending on the test settings one of them might be culled
	vk::beginCommandBuffer(vkd, commandBuffer.get());
	renderPass.begin(vkd, *commandBuffer, renderArea, clearColor);
	vkd.cmdSetPatchControlPointsEXT(commandBuffer.get(), 3);
	pipeline1.bind(commandBuffer.get());
	vkd.cmdDraw(commandBuffer.get(), 6, 1, 0, 0);
	pipeline2.bind(commandBuffer.get());
	vkd.cmdDraw(commandBuffer.get(), 6, 1, 0, 0);
	renderPass.end(vkd, commandBuffer.get());
	vk::copyImageToBuffer(vkd, commandBuffer.get(), colorAttachment.get()->get(), (*outBuffer).get(), imageSize);
	vk::endCommandBuffer(vkd, commandBuffer.get());
	vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), commandBuffer.get());

	invalidateAlloc(vkd, device, outBufferAlloc);
	tcu::ConstPixelBufferAccess outPixels(mappedFormat, imageDim, outBufferData);

	auto expectedFirst = m_config.expectedFirst;
	auto expectedSecond = m_config.expectedSecond;

	tcu::TextureLevel referenceLevel(mappedFormat, imageExtent.height, imageExtent.height);
	tcu::PixelBufferAccess reference = referenceLevel.getAccess();
	tcu::clear(getSubregion(reference, 0, 0, imageExtent.width / 2, imageExtent.height), expectedFirst);
	tcu::clear(getSubregion(reference, imageExtent.width / 2, 0, imageExtent.width / 2, imageExtent.height), expectedSecond);

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", reference, outPixels, tcu::Vec4(0.0), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Color output does not match reference, image added to log");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestCaseGroup* createDynamicControlPointTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "dynamic_control_points", "Tests checking dynamic bind points and switching pipelines"));

	group->addChild(new DynamicControlPointsTestCase(testCtx, "change_output", "test switching pipelines with dynamic control points while changing the number of tcs invocations",
		TestConfig {
			pipelineConstructionType,
			true,
			false,
			false,
			vk::VK_CULL_MODE_NONE,
			tcu::Vec4(1.0, 0.0, 1.0, 1.0),
			tcu::Vec4(1.0, 0.0, 1.0, 1.0),
	}));

	group->addChild(new DynamicControlPointsTestCase(testCtx, "change_winding", "test switching pipelines with dynamic control points while switching winding",
		TestConfig {
			pipelineConstructionType,
			false,
			true,
			false,
			vk::VK_CULL_MODE_FRONT_BIT,
			tcu::Vec4(1.0, 1.0, 1.0, 1.0),
			tcu::Vec4(1.0, 0.0, 1.0, 1.0)
	}));

	group->addChild(new DynamicControlPointsTestCase(testCtx, "change_output_winding", "test switching pipelines with dynamic control points while switching winding and number of tcs invocations",
		TestConfig {
			pipelineConstructionType,
			true,
			true,
			false,
			vk::VK_CULL_MODE_FRONT_BIT,
			tcu::Vec4(1.0, 1.0, 1.0, 1.0),
			tcu::Vec4(1.0, 0.0, 1.0, 1.0)
	}));
	return group.release();
}

} // pipeline
} // vkt
