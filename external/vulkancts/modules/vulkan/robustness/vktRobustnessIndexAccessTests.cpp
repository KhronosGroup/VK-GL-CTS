/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Robust Index Buffer Access Tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessIndexAccessTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vktRobustnessUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMath.h"
#include "tcuVectorUtil.hpp"
#include "deUniquePtr.hpp"
#include <vector>

namespace vkt
{
namespace robustness
{

using namespace vk;

#ifndef CTS_USES_VULKANSC
typedef de::MovePtr<vk::DeviceDriver> DeviceDriverPtr;
#else
typedef de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> DeviceDriverPtr;
#endif // CTS_USES_VULKANSC

enum TestMode
{
	TM_DRAW_INDEXED					= 0,
	TM_DRAW_INDEXED_INDIRECT,
	TM_DRAW_INDEXED_INDIRECT_COUNT,
	TM_DRAW_MULTI_INDEXED,
};

class DrawIndexedInstance : public vkt::TestInstance
{
public:
								DrawIndexedInstance		(Context&										context,
														 Move<VkDevice>									device,
														 DeviceDriverPtr								deviceDriver,
														 TestMode										mode,
														 deUint32										robustnessVersion);

	virtual						~DrawIndexedInstance	(void) = default;

	virtual tcu::TestStatus		iterate					(void);

protected:

	Move<VkDevice>								m_device;
	DeviceDriverPtr								m_deviceDriver;
	TestMode									m_mode;
	deUint32									m_robustnessVersion;
};

DrawIndexedInstance::DrawIndexedInstance(Context&								context,
										 Move<VkDevice>							device,
										 DeviceDriverPtr						deviceDriver,
										 TestMode								mode,
										 deUint32								robustnessVersion)
	: vkt::TestInstance			(context)
	, m_device					(device)
	, m_deviceDriver			(deviceDriver)
	, m_mode					(mode)
	, m_robustnessVersion		(robustnessVersion)
{
}

tcu::TestStatus DrawIndexedInstance::iterate(void)
{
	const DeviceInterface&	vk					= *m_deviceDriver;
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto&				vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice	physicalDevice		= chooseDevice(vki, m_context.getInstance(), m_context.getTestContext().getCommandLine());
	SimpleAllocator			memAlloc			(vk, *m_device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

	// this is testsed - first index in index buffer is outside of bounds
	const deUint32					oobFirstIndex = std::numeric_limits<deUint32>::max() - 100;

	const VkFormat					colorFormat	{ VK_FORMAT_R8G8B8A8_UNORM };
	const tcu::UVec2				renderSize	{ 16 };
	const std::vector<VkViewport>	viewports	{ makeViewport(renderSize) };
	const std::vector<VkRect2D>		scissors	{ makeRect2D(renderSize) };

	// create vertex buffer
	const std::vector<float> vertices
	{
		 0.0f, -0.8f,    0.0f, 1.0f,
		 0.0f,  0.8f,    0.0f, 1.0f,
		 0.8f, -0.8f,    0.0f, 1.0f,
		 0.8f,  0.8f,    0.0f, 1.0f,
		-0.8f, -0.8f,    0.0f, 1.0f,
		-0.8f,  0.8f,    0.0f, 1.0f,
	};
	const VkBufferCreateInfo vertexBufferInfo = makeBufferCreateInfo(vertices.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory vertexBuffer(vk, *m_device, memAlloc, vertexBufferInfo, MemoryRequirement::HostVisible);
	deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), vertices.size() * sizeof(float));
	flushAlloc(vk, *m_device, vertexBuffer.getAllocation());

	// create index buffer for 6 points
	// 4--0--2
	// |  |  |
	// 5--1--3
	const std::vector<deUint32> index = { 0, 1, 2, 3, 4, 5 };
	const VkBufferCreateInfo indexBufferInfo = makeBufferCreateInfo(index.size() * sizeof(deUint32), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory indexBuffer(vk, *m_device, memAlloc, indexBufferInfo, MemoryRequirement::HostVisible);
	deMemcpy(indexBuffer.getAllocation().getHostPtr(), index.data(), index.size() * sizeof(deUint32));
	flushAlloc(vk, *m_device, indexBuffer.getAllocation());

	// create indirect buffer
	const vk::VkDrawIndexedIndirectCommand drawIndirectCommand
	{
		(deUint32)index.size(),	// indexCount
		1u,						// instanceCount
		oobFirstIndex,			// firstIndex
		0u,						// vertexOffset
		0u,						// firstInstance
	};
	const VkBufferCreateInfo indirectBufferInfo = makeBufferCreateInfo(sizeof(drawIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory indirectBuffer(vk, *m_device, memAlloc, indirectBufferInfo, MemoryRequirement::HostVisible);
	if ((m_mode == TM_DRAW_INDEXED_INDIRECT) || (m_mode == TM_DRAW_INDEXED_INDIRECT_COUNT))
	{
		deMemcpy(indirectBuffer.getAllocation().getHostPtr(), &drawIndirectCommand, sizeof(drawIndirectCommand));
		flushAlloc(vk, *m_device, indirectBuffer.getAllocation());
	}

	// create indirect count buffer
	const VkBufferCreateInfo indirectCountBufferInfo = makeBufferCreateInfo(sizeof(deUint32), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory indirectCountBuffer(vk, *m_device, memAlloc, indirectCountBufferInfo, MemoryRequirement::HostVisible);
	if (m_mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
	{
		*(reinterpret_cast<deUint32*>(indirectCountBuffer.getAllocation().getHostPtr())) = 1;
		flushAlloc(vk, *m_device, indirectCountBuffer.getAllocation());
	}

	// create output buffer that will be used to read rendered image
	const VkDeviceSize outputBufferSize = renderSize.x()* renderSize.y()* tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkBufferCreateInfo outputBufferInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory outputBuffer(vk, *m_device, memAlloc, outputBufferInfo, MemoryRequirement::HostVisible);

	// create color buffer
	VkExtent3D imageExtent = makeExtent3D(renderSize.x(), renderSize.y(), 1u);
	const VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									//	VkStructureType			sType;
		DE_NULL,																//	const void*				pNext;
		0u,																		//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,														//	VkImageType				imageType;
		colorFormat,															//	VkFormat				format;
		imageExtent,															//	VkExtent3D				extent;
		1u,																		//	deUint32				mipLevels;
		1u,																		//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,													//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,												//	VkImageTiling			tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,												//	VkSharingMode			sharingMode;
		0u,																		//	deUint32				queueFamilyIndexCount;
		DE_NULL,																//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,												//	VkImageLayout			initialLayout;
	};
	const VkImageSubresourceRange colorSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	ImageWithMemory colorImage(vk, *m_device, memAlloc, imageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView> colorImageView = makeImageView(vk, *m_device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// create shader modules, renderpass, framebuffer and pipeline
	Move<VkShaderModule>	vertShaderModule	= createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShaderModule	= createShaderModule(vk, *m_device, m_context.getBinaryCollection().get("frag"), 0);
	Move<VkRenderPass>		renderPass			= makeRenderPass(vk, *m_device, colorFormat);
	Move<VkPipelineLayout>	pipelineLayout		= makePipelineLayout(vk, *m_device, DE_NULL);
	Move<VkFramebuffer>		framebuffer			= makeFramebuffer(vk, *m_device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());
	Move<VkPipeline>		graphicsPipeline	= makeGraphicsPipeline(vk, *m_device, *pipelineLayout,
																	   *vertShaderModule, DE_NULL, DE_NULL, DE_NULL, *fragShaderModule,
																	   *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

	Move<VkCommandPool>				cmdPool		= createCommandPool(vk, *m_device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	vk::Move<vk::VkCommandBuffer>	cmdBuffer	= allocateCommandBuffer(vk, *m_device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);

	// transition colorbuffer layout
	VkImageMemoryBarrier imageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
															   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
															   colorImage.get(), colorSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	const VkRect2D renderArea = makeRect2D(0, 0, renderSize.x(), renderSize.y());
	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	const VkDeviceSize vBuffOffset = 0;
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vBuffOffset);
	vk.cmdBindIndexBuffer(*cmdBuffer, indexBuffer.get(), 0, VK_INDEX_TYPE_UINT32);

	// we will draw all points at index 0
	if (m_mode == TM_DRAW_INDEXED)
		vk.cmdDrawIndexed(*cmdBuffer, (deUint32)index.size(), 1, oobFirstIndex, 0, 0);
	else if (m_mode == TM_DRAW_INDEXED_INDIRECT)
		vk.cmdDrawIndexedIndirect(*cmdBuffer, indirectBuffer.get(), 0, 1, 0);
	else if (m_mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
		vk.cmdDrawIndexedIndirectCount(*cmdBuffer, indirectBuffer.get(), 0, indirectCountBuffer.get(), 0, 1, sizeof(VkDrawIndexedIndirectCommand));
	else if (m_mode == TM_DRAW_MULTI_INDEXED)
	{
#ifndef CTS_USES_VULKANSC
		VkMultiDrawIndexedInfoEXT indexInfo[]
		{
			{ oobFirstIndex, 3, 0 },
			{ oobFirstIndex - 3, 3, 0 },
		};
		vk.cmdDrawMultiIndexedEXT(*cmdBuffer, 2, indexInfo, 1, 0, sizeof(VkMultiDrawIndexedInfoEXT), DE_NULL);
#endif // CTS_USES_VULKANSC
	}

	endRenderPass(vk, *cmdBuffer);

	// wait till data is transfered to image
	imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
										  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
										  colorImage.get(), colorSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	// read back color image
	const VkImageSubresourceLayers colorSL = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy copyRegion = makeBufferImageCopy(imageExtent, colorSL);
	vk.cmdCopyImageToBuffer(*cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer.get(), 1u, &copyRegion);

	auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, outputBuffer.get(), 0u, VK_WHOLE_SIZE);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, 0u, 1u, &bufferBarrier, 0u, 0u);

	endCommandBuffer(vk, *cmdBuffer);

	VkQueue queue;
	vk.getDeviceQueue(*m_device, queueFamilyIndex, 0, &queue);
	submitCommandsAndWait(vk, *m_device, queue, *cmdBuffer);

	// for robustBufferAccess (the original feature) OOB access will return undefined value;
	// we can only expect that above drawing will be executed without errors (we can't expect any specific result)
	if (m_robustnessVersion < 2u)
		return tcu::TestStatus::pass("Pass");

	// get output buffer
	invalidateAlloc(vk, *m_device, outputBuffer.getAllocation());
	const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
	tcu::ConstPixelBufferAccess outputAccess(resultFormat, renderSize.x(), renderSize.y(), 1u, outputBuffer.getAllocation().getHostPtr());

	// for VK_EXT_robustness2 OOB access should return 0 and we can verify
	// that single fragment is drawn in the middle-top part of the image
	tcu::UVec4 expectedValue(51, 255, 127, 255);
	bool fragmentFound = false;

	for (deUint32 x = 0u; x < renderSize.x(); ++x)
	for (deUint32 y = 0u; y < renderSize.y(); ++y)
	{
		tcu::UVec4 pixel = outputAccess.getPixelUint(x, y, 0);

		if (tcu::boolAll(tcu::lessThan(tcu::absDiff(pixel, expectedValue), tcu::UVec4(2))))
		{
			if (fragmentFound)
			{
				m_context.getTestContext().getLog()
					<< tcu::TestLog::Message << "Expected single fragment with: " << expectedValue
					<< " color, got more, second at " << tcu::UVec2(x, y) << tcu::TestLog::EndMessage
					<< tcu::TestLog::Image("Result", "Result", outputAccess);
				return tcu::TestStatus::fail("Fail");
			}
			else if ((y < 3) && (x > 5) && (x < 10))
				fragmentFound = true;
			else
			{
				m_context.getTestContext().getLog()
					<< tcu::TestLog::Message << "Expected fragment in the middle-top of the image, got at: "
					<< tcu::UVec2(x, y) << tcu::TestLog::EndMessage
					<< tcu::TestLog::Image("Result", "Result", outputAccess);
				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	if (fragmentFound)
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

class DrawIndexedTestCase : public vkt::TestCase
{
public:

						DrawIndexedTestCase		(tcu::TestContext&		testContext,
												 const std::string&		name,
												 TestMode				mode,
												 deUint32				robustnessVersion);

	virtual				~DrawIndexedTestCase	(void) = default;

	void				checkSupport			(Context& context) const override;
	TestInstance*		createInstance			(Context& context) const override;
	void				initPrograms			(SourceCollections& programCollection) const override;

protected:
	const TestMode		m_testMode;
	const deUint32		m_robustnessVersion;
};

DrawIndexedTestCase::DrawIndexedTestCase(tcu::TestContext&		testContext,
										 const std::string&		name,
										 TestMode				mode,
										 deUint32				robustnessVersion)

	: vkt::TestCase			(testContext, name, "")
	, m_testMode			(mode)
	, m_robustnessVersion	(robustnessVersion)
{}

void DrawIndexedTestCase::checkSupport(Context& context) const
{
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getDeviceFeatures().robustBufferAccess)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: robustBufferAccess not supported by this implementation");

	if (m_testMode == TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT)
		context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
	if (m_testMode == TestMode::TM_DRAW_MULTI_INDEXED)
		context.requireDeviceFunctionality("VK_EXT_multi_draw");
	if (m_robustnessVersion == 2)
	{
		context.requireDeviceFunctionality("VK_EXT_robustness2");

		const auto& vki				= context.getInstanceInterface();
		const auto	physicalDevice	= context.getPhysicalDevice();

		VkPhysicalDeviceRobustness2FeaturesEXT	robustness2Features	= initVulkanStructure();
		VkPhysicalDeviceFeatures2				features2			= initVulkanStructure(&robustness2Features);

		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

		if (!robustness2Features.robustBufferAccess2)
			TCU_THROW(NotSupportedError, "robustBufferAccess2 not supported");
	}
}

TestInstance* DrawIndexedTestCase::createInstance(Context& context) const
{
	VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();
	features2.features.robustBufferAccess = DE_TRUE;

	void** nextPtr = &features2.pNext;

#ifndef CTS_USES_VULKANSC
	VkPhysicalDeviceMultiDrawFeaturesEXT multiDrawFeatures = initVulkanStructure();
	if (m_testMode == TestMode::TM_DRAW_MULTI_INDEXED)
	{
		multiDrawFeatures.multiDraw = DE_TRUE;
		addToChainVulkanStructure(&nextPtr, multiDrawFeatures);
	}
#endif // CTS_USES_VULKANSC

	VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
	if (m_robustnessVersion > 1u)
	{
		robustness2Features.robustBufferAccess2 = DE_TRUE;
		addToChainVulkanStructure(&nextPtr, robustness2Features);
	}

	deUint32 apiVersion = context.getUsedApiVersion();
	VkPhysicalDeviceVulkan12Features vulkan12Features = initVulkanStructure();
	if ((m_testMode == TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT) && (apiVersion > VK_MAKE_API_VERSION(0, 1, 1, 0)))
	{
		vulkan12Features.drawIndirectCount = DE_TRUE;
		addToChainVulkanStructure(&nextPtr, vulkan12Features);
	}

	Move<VkDevice>	device = createRobustBufferAccessDevice(context, &features2);
	DeviceDriverPtr deviceDriver =
#ifndef CTS_USES_VULKANSC
		DeviceDriverPtr(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device));
#else
		DeviceDriverPtr(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device, context.getTestContext().getCommandLine(),
										   context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties()),
						vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC

	return new DrawIndexedInstance(context, device, deviceDriver, m_testMode, m_robustnessVersion);
}

void DrawIndexedTestCase::initPrograms(SourceCollections& sourceCollections) const
{
	std::string vertexSource(
		"#version 450\n"
		"layout(location = 0) in vec4 inPosition;\n"
		"void main(void)\n"
		"{\n"
		"\tgl_Position = inPosition;\n"
		"\tgl_PointSize = 1.0;\n"
		"}\n");
	sourceCollections.glslSources.add("vert") << glu::VertexSource(vertexSource);

	std::string fragmentSource(
		"#version 450\n"
		"precision highp float;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"\tfragColor = vec4(0.2, 1.0, 0.5, 1.0);\n"
		"}\n");

	sourceCollections.glslSources.add("frag") << glu::FragmentSource(fragmentSource);
}

tcu::TestCaseGroup* createIndexAccessTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> indexAccessTests(new tcu::TestCaseGroup(testCtx, "index_access", "Test access outside of the buffer for indices"));

	struct TestConfig
	{
		std::string		name;
		TestMode		mode;
	};

	const std::vector<TestConfig> testConfigs
	{
		{ "draw_indexed",					TestMode::TM_DRAW_INDEXED },
		{ "draw_indexed_indirect",			TestMode::TM_DRAW_INDEXED_INDIRECT },
		{ "draw_indexed_indirect_count",	TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT },
		{ "draw_multi_indexed",				TestMode::TM_DRAW_MULTI_INDEXED },
	};

	const deUint32 robustnessVersion = 2;
	for (const auto& c : testConfigs)
	{
		std::string name = c.name + "_" + std::to_string(robustnessVersion);
		indexAccessTests->addChild(new DrawIndexedTestCase(testCtx, name, c.mode, robustnessVersion));
	}

	return indexAccessTests.release();
}

} // robustness
} // vkt
