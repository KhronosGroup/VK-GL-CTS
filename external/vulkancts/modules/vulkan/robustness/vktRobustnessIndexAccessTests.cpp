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
#include <algorithm>
#include <numeric>
#include <tuple>
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

enum OOTypes
{
	OO_NONE,
	OO_INDEX,
	OO_SIZE,
	OO_WHOLE_SIZE
};

struct TestParams
{
	TestMode	mode;
	OOTypes		ooType;
	deUint32	leadingCount;
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

	void				checkSupport			(Context&				context) const override;
	TestInstance*		createInstance			(Context&				context) const override;
	void				initPrograms			(SourceCollections&		programCollection) const override;

protected:
	void				createDeviceAndDriver	(Context&				context,
												 Move<VkDevice>&		device,
												 DeviceDriverPtr&		driver) const;
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

void DrawIndexedTestCase::checkSupport (Context& context) const
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

void DrawIndexedTestCase::createDeviceAndDriver (Context& context, Move<VkDevice>& device, DeviceDriverPtr& driver) const
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

	device = createRobustBufferAccessDevice(context, &features2);
	driver =
#ifndef CTS_USES_VULKANSC
		DeviceDriverPtr(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion()));
#else
		DeviceDriverPtr(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device, context.getTestContext().getCommandLine(),
			context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(), context.getUsedApiVersion()),
			vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC
}

TestInstance* DrawIndexedTestCase::createInstance(Context& context) const
{
	Move<VkDevice>	device;
	DeviceDriverPtr deviceDriver;
	createDeviceAndDriver(context, device, deviceDriver);
	return new DrawIndexedInstance(context, device, deviceDriver, m_testMode, m_robustnessVersion);
}

void DrawIndexedTestCase::initPrograms (SourceCollections& sourceCollections) const
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

class BindIndexBuffer2Instance : public vkt::TestInstance
{
public:
							BindIndexBuffer2Instance	(Context&			c,
														 Move<VkDevice>		device,
														 DeviceDriverPtr	driver,
														 const TestParams&	params);
	virtual					~BindIndexBuffer2Instance	(void) = default;

	virtual tcu::TestStatus	iterate						(void) override;

protected:
	const	Move<VkDevice>		m_device;
	const	DeviceDriverPtr		m_driver;
	const	TestParams			m_params;
			VkPhysicalDevice	m_physDevice;
			SimpleAllocator		m_allocator;

protected:
	inline	const DeviceInterface&	getDeviceInterface	() const { return *m_driver; }
	inline	VkDevice				getDevice			() const { return *m_device; }
	inline	VkPhysicalDevice		getPhysicalDevice	() const { return m_physDevice; }
	inline	Allocator&				getAllocator		()       { return m_allocator; }
			VkQueue					getQueue			() const;
};

BindIndexBuffer2Instance::BindIndexBuffer2Instance	(Context&			c,
													 Move<VkDevice>		device,
													 DeviceDriverPtr	driver,
													 const TestParams&	params)
	: vkt::TestInstance	(c)
	, m_device			(device)
	, m_driver			(driver)
	, m_params			(params)
	, m_physDevice		(chooseDevice(c.getInstanceInterface(), c.getInstance(), c.getTestContext().getCommandLine()))
	, m_allocator		(getDeviceInterface(), getDevice(), getPhysicalDeviceMemoryProperties(c.getInstanceInterface(), m_physDevice))
{
}

VkQueue BindIndexBuffer2Instance::getQueue () const
{
	VkQueue	queue = DE_NULL;
	getDeviceInterface().getDeviceQueue(getDevice(), m_context.getUniversalQueueFamilyIndex(), 0, &queue);
	return queue;
}

class BindIndexBuffer2TestCase : public DrawIndexedTestCase
{
public:
					BindIndexBuffer2TestCase	(tcu::TestContext&	testContext,
												 const std::string&	name,
												 const TestParams&	params);
					~BindIndexBuffer2TestCase	(void) = default;

	void			checkSupport				(Context&			context) const override;
	TestInstance*	createInstance				(Context&			context) const override;
	void			initPrograms				(SourceCollections& programs) const override;

protected:
	const OOTypes	m_ooType;
	const deUint32	m_leadingCount;
};

BindIndexBuffer2TestCase::BindIndexBuffer2TestCase	(tcu::TestContext&	testContext,
													 const std::string&	name,
													 const TestParams&	params)
	: DrawIndexedTestCase	(testContext, name, params.mode, 2)
	, m_ooType			(params.ooType)
	, m_leadingCount	(params.leadingCount)
{
}

#ifdef CTS_USES_VULKANSC
#define DEPENDENT_MAINTENANCE_5_EXTENSION_NAME "VK_KHR_maintenance5"
#else
#define DEPENDENT_MAINTENANCE_5_EXTENSION_NAME VK_KHR_MAINTENANCE_5_EXTENSION_NAME
#endif

void BindIndexBuffer2TestCase::checkSupport (Context& context) const
{
	DrawIndexedTestCase::checkSupport(context);
	context.requireDeviceFunctionality(DEPENDENT_MAINTENANCE_5_EXTENSION_NAME);
}

void BindIndexBuffer2TestCase::initPrograms (SourceCollections& programs) const
{
	const std::string vertexSource(
		"#version 450\n"
		"layout(location = 0) in vec4 inPosition;\n"
		"void main(void) {\n"
		"   gl_Position = inPosition;\n"
		"   gl_PointSize = 1.0;\n"
		"}\n");
	programs.glslSources.add("vert") << glu::VertexSource(vertexSource);

	const std::string fragmentSource(
		"#version 450\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main (void) {\n"
		"   fragColor = vec4(1.0);\n"
		"}\n");
	programs.glslSources.add("frag") << glu::FragmentSource(fragmentSource);
}

TestInstance* BindIndexBuffer2TestCase::createInstance (Context& context) const
{
	TestParams			params;
	Move<VkDevice>		device;
	DeviceDriverPtr		deviceDriver;
	createDeviceAndDriver(context, device, deviceDriver);

	params.mode					= m_testMode;
	params.ooType				= m_ooType;
	params.leadingCount			= m_leadingCount;

	return new BindIndexBuffer2Instance(context, device, deviceDriver, params);
}

tcu::TestStatus BindIndexBuffer2Instance::iterate (void)
{
	const DeviceInterface&			vk				= this->getDeviceInterface();
	const VkDevice					device			= this->getDevice();
	Allocator&						allocator		= this->getAllocator();
	const VkQueue					queue			= this->getQueue();
	const deUint32					queueFamilyIdx	= m_context.getUniversalQueueFamilyIndex();
	tcu::TestLog&					log				= m_context.getTestContext().getLog();

	const VkFormat					colorFormat		{ VK_FORMAT_R32G32B32A32_SFLOAT };
	const tcu::UVec2				renderSize		{ 64, 64 };
	const std::vector<VkViewport>	viewports		{ makeViewport(renderSize) };
	const std::vector<VkRect2D>		scissors		{ makeRect2D(renderSize) };

	// build vertices data
	std::vector<tcu::Vec4>	vertices;

	// first triangle in 2nd quarter, it should not be drawn
	vertices.emplace_back(-1.0f, 0.1f, 0.0f, 1.0f);
	vertices.emplace_back(-1.0f, 1.0f, 0.0f, 1.0f);
	vertices.emplace_back(-0.1f, 0.1f, 0.0f, 1.0f);

	// second triangle in 2nd quarter, it should not be drawn
	vertices.emplace_back(-0.1f, 0.1f, 0.0f, 1.0f);
	vertices.emplace_back(-1.0f, 1.0f, 0.0f, 1.0f);
	vertices.emplace_back(-0.1f, 1.0f, 0.0f, 1.0f);

	// first triangle in 3rd quarter, it must be drawn
	vertices.emplace_back(0.0f, -1.0f, 0.0f, 1.0f);
	vertices.emplace_back(-1.0f, -1.0f, 0.0f, 1.0f);
	vertices.emplace_back(-1.0f, 0.0f, 0.0f, 1.0f);

	// second triangle in 3rd quarter if robustness works as expected,
	// otherwise will be drawn in 1st quarter as well
	vertices.emplace_back(0.0f, -1.0f, 0.0f, 1.0f);
	vertices.emplace_back(-1.0f, 0.0f, 0.0f, 1.0f);
	vertices.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);

	// create vertex buffer
	const VkBufferCreateInfo vertexBufferInfo = makeBufferCreateInfo(vertices.size() * sizeof(tcu::Vec4), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory vertexBuffer(vk, device, allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
	deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), vertices.size() * sizeof(tcu::Vec4));

	// build index data
	const deUint32	leadingCount = m_params.leadingCount;
	std::vector<deUint32>	indices	(leadingCount * 6 + 6);
	for (deUint32 j = 0; j < leadingCount; ++j)
	for (deUint32 k = 0; k < 6; ++k)
	{
		indices[j * 6 + k] = k;
	}
	std::iota(std::next(indices.begin(), (leadingCount * 6)), indices.end(), 6u);

	const deUint32		firstIndex		= 0;
	const deUint32		indexCount		= 6;
	const VkDeviceSize	bindingOffset	= leadingCount * 6 * sizeof(deUint32);
	VkDeviceSize		bindingSize		= 6 * sizeof(deUint32);
	VkDeviceSize		allocSize		= indices.size() * sizeof(deUint32);
	switch (m_params.ooType)
	{
	case OOTypes::OO_NONE:
		// default values already set
		break;
	case OOTypes::OO_INDEX:
		indices.back()	= 33; // out of range index
		break;
	case OOTypes::OO_SIZE:
		bindingSize		= 5 * sizeof(deUint32);
		break;
	case OOTypes::OO_WHOLE_SIZE:
		bindingSize		= VK_WHOLE_SIZE;
		allocSize		= (indices.size() - 1) * sizeof(deUint32);
		break;
	}

	// create index buffer
	const VkBufferCreateInfo indexBufferInfo = makeBufferCreateInfo(allocSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory indexBuffer(vk, device, allocator, indexBufferInfo, MemoryRequirement::HostVisible);
	deMemcpy(indexBuffer.getAllocation().getHostPtr(), indices.data(), size_t(allocSize));

	// create indirect buffer
	const vk::VkDrawIndexedIndirectCommand drawIndirectCommand
	{
		indexCount,		// indexCount
		1u,				// instanceCount
		firstIndex,		// firstIndex
		0u,				// vertexOffset
		0u,				// firstInstance
	};
	const VkBufferCreateInfo indirectBufferInfo = makeBufferCreateInfo(sizeof(drawIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory indirectBuffer(vk, *m_device, allocator, indirectBufferInfo, MemoryRequirement::HostVisible);
	if ((m_params.mode == TM_DRAW_INDEXED_INDIRECT) || (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT))
	{
		deMemcpy(indirectBuffer.getAllocation().getHostPtr(), &drawIndirectCommand, sizeof(drawIndirectCommand));
	}

	// create indirect count buffer
	const VkBufferCreateInfo indirectCountBufferInfo = makeBufferCreateInfo(sizeof(deUint32), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory indirectCountBuffer(vk, *m_device, allocator, indirectCountBufferInfo, MemoryRequirement::HostVisible);
	if (m_params.mode == TM_DRAW_INDEXED_INDIRECT_COUNT)
	{
		*static_cast<deUint32*>(indirectCountBuffer.getAllocation().getHostPtr()) = 1u;
	}

	// create output buffer that will be used to read rendered image
	const VkDeviceSize				outputBufferSize	= renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkBufferCreateInfo		outputBufferInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory				outputBuffer		(vk, device, allocator, outputBufferInfo, MemoryRequirement::HostVisible);

	// create color buffer
	const VkExtent3D				imageExtent			= makeExtent3D(renderSize.x(), renderSize.y(), 1u);
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
	const tcu::Vec4					clearColor			(0.0f, 0.0f, 0.0f, 1.0f);
	const VkImageSubresourceRange	colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	ImageWithMemory					colorImage			(vk, *m_device, allocator, imageCreateInfo, MemoryRequirement::Any);
	Move<VkImageView>				colorImageView		= makeImageView(vk, *m_device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// create shader modules, renderpass, framebuffer and pipeline
	Move<VkShaderModule>			vertShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>			fragShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);
	Move<VkRenderPass>				renderPass			= makeRenderPass(vk, device, colorFormat);
	Move<VkPipelineLayout>			pipelineLayout		= makePipelineLayout(vk, device);
	Move<VkFramebuffer>				framebuffer			= makeFramebuffer(vk, device, *renderPass, *colorImageView, renderSize.x(), renderSize.y());
	Move<VkPipeline>				graphicsPipeline	= makeGraphicsPipeline(vk, device, *pipelineLayout,
																				*vertShaderModule, DE_NULL, DE_NULL, DE_NULL, *fragShaderModule,
																				*renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	Move<VkCommandPool>				cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIdx);
	vk::Move<vk::VkCommandBuffer>	cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);

	// transition colorbuffer layout
	VkImageMemoryBarrier imageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT,
																VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																colorImage.get(), colorSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	const VkRect2D renderArea = makeRect2D(0, 0, renderSize.x(), renderSize.y());
	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &static_cast<const VkDeviceSize&>(0));

#ifndef CTS_USES_VULKANSC
	vk.cmdBindIndexBuffer2KHR(*cmdBuffer, indexBuffer.get(), bindingOffset, bindingSize, VK_INDEX_TYPE_UINT32);
#else
	DE_UNREF(bindingOffset);
	DE_UNREF(bindingSize);
#endif

	// we will draw all points at index 0
	switch (m_params.mode)
	{
	case TM_DRAW_INDEXED:
		vk.cmdDrawIndexed(*cmdBuffer, indexCount, 1u, firstIndex, 0, 0);
		break;

	case TM_DRAW_INDEXED_INDIRECT:
		vk.cmdDrawIndexedIndirect(*cmdBuffer, indirectBuffer.get(), 0, 1, deUint32(sizeof(drawIndirectCommand)));
		break;

	case TM_DRAW_INDEXED_INDIRECT_COUNT:
		vk.cmdDrawIndexedIndirectCount(*cmdBuffer, indirectBuffer.get(), 0, indirectCountBuffer.get(), 0, 1, deUint32(sizeof(drawIndirectCommand)));
		break;

	case TM_DRAW_MULTI_INDEXED:
#ifndef CTS_USES_VULKANSC
		{
			const VkMultiDrawIndexedInfoEXT indexInfo [/* { firstIndex, indexCount, vertexOffset } */]
			{
				{ firstIndex+3,	3, 0 },
				{ firstIndex,	3, 0 },
			};
			vk.cmdDrawMultiIndexedEXT(*cmdBuffer, DE_LENGTH_OF_ARRAY(indexInfo), indexInfo, 1, 0, sizeof(VkMultiDrawIndexedInfoEXT), DE_NULL);
		}
#endif
		break;
	}

	endRenderPass(vk, *cmdBuffer);

	// wait till data is transfered to image
	imageBarrier = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
											VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
											colorImage.get(), colorSRR);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

	// read back color image
	const VkImageSubresourceLayers	colorSL		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy			copyRegion	= makeBufferImageCopy(imageExtent, colorSL);
	vk.cmdCopyImageToBuffer(*cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputBuffer.get(), 1u, &copyRegion);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// get output buffer
	invalidateAlloc(vk, device, outputBuffer.getAllocation());
	const tcu::TextureFormat resultFormat = mapVkFormat(colorFormat);
	tcu::ConstPixelBufferAccess resultAccess(resultFormat, renderSize.x(), renderSize.y(), 1u, outputBuffer.getAllocation().getHostPtr());


	// neither one triangle should be drawn in the second quarter, they are omitted by the offset or the firstIndex parameters
	const tcu::Vec4	p11	=	resultAccess.getPixel((1 * renderSize.x()) / 8, (5 * renderSize.y()) / 8);
	const tcu::Vec4	p12	=	resultAccess.getPixel((3 * renderSize.x()) / 8, (7 * renderSize.y()) / 8);
	const bool		c1	=	p11.x() == clearColor.x() && p11.y() == clearColor.y() && p11.z() == clearColor.z()
						&&  p12.x() == clearColor.x() && p12.y() == clearColor.y() && p12.z() == clearColor.z();

	// small triangle in the third quarter must be drawn always
	const tcu::Vec4 p2	=	resultAccess.getPixel((1 * renderSize.x()) / 8, (1 * renderSize.y()) / 8);
	const bool		c2	=	p2.x() != clearColor.x() && p2.y() != clearColor.y() && p2.z() != clearColor.z();

	// if robustness works, then the origin of coordinate system will be read in shader instead of a value that an index points (1,1)
	const tcu::Vec4	p3	=	resultAccess.getPixel((3 * renderSize.x()) / 4, (3 * renderSize.y()) / 4);
	const bool		c3	=	p3.x() == clearColor.x() && p3.y() == clearColor.y() && p3.z() == clearColor.z();

	bool verdict = false;
	switch (m_params.ooType)
	{
		case OOTypes::OO_NONE:
			verdict = c1 && c2 && !c3;
			break;
		default:
			verdict = c1 && c2 && c3;
			break;
	}

	log << tcu::TestLog::ImageSet("Result", "")
		<< tcu::TestLog::Image(std::to_string(m_params.mode), "", resultAccess)
		<< tcu::TestLog::EndImageSet;
	return (*(verdict ? &tcu::TestStatus::pass : &tcu::TestStatus::fail))(std::string());
}

tcu::TestCaseGroup* createCmdBindIndexBuffer2Tests (tcu::TestContext& testCtx)
{
	const std::pair<const char*, TestMode> modes[]
	{
		{ "draw_indexed",					TestMode::TM_DRAW_INDEXED },
		{ "draw_indexed_indirect",			TestMode::TM_DRAW_INDEXED_INDIRECT },
		{ "draw_indexed_indirect_count",	TestMode::TM_DRAW_INDEXED_INDIRECT_COUNT },
		{ "draw_multi_indexed",				TestMode::TM_DRAW_MULTI_INDEXED },
	};

	const std::pair<std::string, OOTypes> OutOfTypes[]
	{
		{ "oo_none",		OOTypes::OO_NONE		},
		{ "oo_index",		OOTypes::OO_INDEX		},
		{ "oo_size",		OOTypes::OO_SIZE		},
		{ "oo_whole_size",	OOTypes::OO_WHOLE_SIZE	},
	};

	const deUint32 offsets[] = { 0, 100 };

	de::MovePtr<tcu::TestCaseGroup> gRoot(new tcu::TestCaseGroup(testCtx, "bind_index_buffer2", "Test access outside of the buffer with using the vkCmdBindIndexBuffer2 function from VK_KHR_maintenance5 extension."));
	for (deUint32 offset : offsets)
	{
		de::MovePtr<tcu::TestCaseGroup> gOffset(new tcu::TestCaseGroup(testCtx, ("offset_" + std::to_string(offset)).c_str(), ""));
		for (const auto& mode : modes)
		{
			de::MovePtr<tcu::TestCaseGroup> gMode(new tcu::TestCaseGroup(testCtx, mode.first, ""));
			for (const auto& ooType : OutOfTypes)
			{
				TestParams p;
				p.mode = mode.second;
				p.ooType = ooType.second;
				p.leadingCount = offset;
				gMode->addChild(new BindIndexBuffer2TestCase(testCtx, ooType.first, p));
			}
			gOffset->addChild(gMode.release());
		}
		gRoot->addChild(gOffset.release());
	}

	return gRoot.release();
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
