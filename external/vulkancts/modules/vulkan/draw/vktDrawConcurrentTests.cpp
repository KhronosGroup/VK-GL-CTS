/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
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
 * \brief Concurrent draw tests
 * Tests that create queue for rendering as well as queue for
 * compute, and trigger work on both pipelines at the same time,
 * and finally verify that the results are as expected.
 *//*--------------------------------------------------------------------*/

#include "vktDrawConcurrentTests.hpp"

#include "vktCustomInstancesDevices.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"
#include "../compute/vktComputeTestsUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "deRandom.hpp"

using namespace vk;

namespace vkt
{
namespace Draw
{
namespace
{

class ConcurrentDraw : public DrawTestsBaseClass
{
public:
	typedef TestSpecBase	TestSpec;
							ConcurrentDraw			(Context &context, TestSpec testSpec);
	virtual tcu::TestStatus	iterate					(void);
};

ConcurrentDraw::ConcurrentDraw (Context &context, TestSpec testSpec)
	: DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], testSpec.groupParams, testSpec.topology)
{
	m_data.push_back(VertexElementData(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));
	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

	int refVertexIndex = 2;

	for (int i = 0; i < 1000; i++)
	{
		m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, -0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
		m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
		m_data.push_back(VertexElementData(tcu::Vec4(0.3f, -0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
		m_data.push_back(VertexElementData(tcu::Vec4(0.3f, -0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
		m_data.push_back(VertexElementData(tcu::Vec4(0.3f, 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
		m_data.push_back(VertexElementData(tcu::Vec4(-0.3f, 0.3f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), refVertexIndex++));
	}
	m_data.push_back(VertexElementData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec(), -1));

	initialize();
}

tcu::TestStatus ConcurrentDraw::iterate (void)
{
	enum
	{
		NO_MATCH_FOUND		= ~((deUint32)0),
		ERROR_NONE			= 0,
		ERROR_WAIT_COMPUTE	= 1,
		ERROR_WAIT_DRAW		= 2
	};

	struct Queue
	{
		VkQueue		queue;
		deUint32	queueFamilyIndex;
	};

	const deUint32							numValues		= 1024;
	const CustomInstance					instance		(createCustomInstanceFromContext(m_context));
	const InstanceDriver&					instanceDriver	(instance.getDriver());
	const VkPhysicalDevice					physicalDevice	= chooseDevice(instanceDriver, instance, m_context.getTestContext().getCommandLine());
//
//	const InstanceInterface&				instance		= m_context.getInstanceInterface();
//	const VkPhysicalDevice					physicalDevice	= m_context.getPhysicalDevice();
	const auto								validation		= m_context.getTestContext().getCommandLine().isValidationEnabled();
	tcu::TestLog&							log				= m_context.getTestContext().getLog();
	Move<VkDevice>							computeDevice;
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties;
	VkDeviceCreateInfo						deviceInfo;
	VkPhysicalDeviceFeatures				deviceFeatures;
	const float								queuePriority	= 1.0f;
	VkDeviceQueueCreateInfo					queueInfos;
	Queue									computeQueue	= { DE_NULL, (deUint32)NO_MATCH_FOUND };

	// Set up compute

	queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

	for (deUint32 queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
	{
		if (queueFamilyProperties[queueNdx].queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			if (computeQueue.queueFamilyIndex == NO_MATCH_FOUND)
				computeQueue.queueFamilyIndex = queueNdx;
		}
	}

	if (computeQueue.queueFamilyIndex == NO_MATCH_FOUND)
		TCU_THROW(NotSupportedError, "Compute queue couldn't be created");

	VkDeviceQueueCreateInfo queueInfo;
	deMemset(&queueInfo, 0, sizeof(queueInfo));

	queueInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.pNext				= DE_NULL;
	queueInfo.flags				= (VkDeviceQueueCreateFlags)0u;
	queueInfo.queueFamilyIndex	= computeQueue.queueFamilyIndex;
	queueInfo.queueCount		= 1;
	queueInfo.pQueuePriorities	= &queuePriority;

	queueInfos = queueInfo;

	deMemset(&deviceInfo, 0, sizeof(deviceInfo));
	instanceDriver.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	void* pNext												= DE_NULL;
#ifdef CTS_USES_VULKANSC
	VkDeviceObjectReservationCreateInfo memReservationInfo	= m_context.getTestContext().getCommandLine().isSubProcess() ? m_context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
	memReservationInfo.pNext								= pNext;
	pNext													= &memReservationInfo;

	VkPhysicalDeviceVulkanSC10Features sc10Features			= createDefaultSC10Features();
	sc10Features.pNext										= pNext;
	pNext													= &sc10Features;

	VkPipelineCacheCreateInfo			pcCI;
	std::vector<VkPipelinePoolSize>		poolSizes;
	if (m_context.getTestContext().getCommandLine().isSubProcess())
	{
		if (m_context.getResourceInterface()->getCacheDataSize() > 0)
		{
			pcCI =
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,			// VkStructureType				sType;
				DE_NULL,												// const void*					pNext;
				VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
					VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
				m_context.getResourceInterface()->getCacheDataSize(),	// deUintptr					initialDataSize;
				m_context.getResourceInterface()->getCacheData()		// const void*					pInitialData;
			};
			memReservationInfo.pipelineCacheCreateInfoCount		= 1;
			memReservationInfo.pPipelineCacheCreateInfos		= &pcCI;
		}

		poolSizes							= m_context.getResourceInterface()->getPipelinePoolSizes();
		if (!poolSizes.empty())
		{
			memReservationInfo.pipelinePoolSizeCount			= deUint32(poolSizes.size());
			memReservationInfo.pPipelinePoolSizes				= poolSizes.data();
		}
	}
#endif // CTS_USES_VULKANSC

	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= pNext;
	deviceInfo.enabledExtensionCount	= 0u;
	deviceInfo.ppEnabledExtensionNames	= DE_NULL;
	deviceInfo.enabledLayerCount		= 0u;
	deviceInfo.ppEnabledLayerNames		= DE_NULL;
	deviceInfo.pEnabledFeatures			= &deviceFeatures;
	deviceInfo.queueCreateInfoCount		= 1;
	deviceInfo.pQueueCreateInfos		= &queueInfos;

	computeDevice = createCustomDevice(validation, m_context.getPlatformInterface(), instance, instanceDriver, physicalDevice, &deviceInfo);

#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>	deviceDriver = de::MovePtr<vk::DeviceDriver>(new vk::DeviceDriver(m_context.getPlatformInterface(), instance, *computeDevice, m_context.getUsedApiVersion()));
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(m_context.getPlatformInterface(), instance, *computeDevice, m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(), m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(), m_context.getUsedApiVersion()), vk::DeinitDeviceDeleter(m_context.getResourceInterface().get(), *computeDevice));
#endif // CTS_USES_VULKANSC
	vk::DeviceInterface& vk = *deviceDriver;

	vk.getDeviceQueue(*computeDevice, computeQueue.queueFamilyIndex, 0, &computeQueue.queue);

	// Create an input/output buffer
	const VkPhysicalDeviceMemoryProperties memoryProperties = getPhysicalDeviceMemoryProperties(instanceDriver, physicalDevice);

	de::MovePtr<SimpleAllocator> allocator			= de::MovePtr<SimpleAllocator>(new SimpleAllocator(vk, *computeDevice, memoryProperties));
	const VkDeviceSize			 bufferSizeBytes	= sizeof(deUint32) * numValues;
	const vk::BufferWithMemory	 buffer(vk, *computeDevice, *allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Fill the buffer with data

	typedef std::vector<deUint32> data_vector_t;
	data_vector_t inputData(numValues);

	{
		de::Random			rnd(0x82ce7f);
		const Allocation&	bufferAllocation	= buffer.getAllocation();
		deUint32*			bufferPtr			= static_cast<deUint32*>(bufferAllocation.getHostPtr());

		for (deUint32 i = 0; i < numValues; ++i)
		{
			deUint32 val = rnd.getUint32();
			inputData[i] = val;
			*bufferPtr++ = val;
		}

		flushAlloc(vk, *computeDevice, bufferAllocation);
	}

	// Create descriptor set

	const Unique<VkDescriptorSetLayout>	descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, *computeDevice));

	const Unique<VkDescriptorPool>		descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, *computeDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet>		descriptorSet(makeDescriptorSet(vk, *computeDevice, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo		bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.update(vk, *computeDevice);

	// Perform the computation

	const Unique<VkShaderModule>		shaderModule(createShaderModule(vk, *computeDevice, m_context.getBinaryCollection().get("vulkan/draw/ConcurrentPayload.comp"), 0u));

	const Unique<VkPipelineLayout>		pipelineLayout(makePipelineLayout(vk, *computeDevice, *descriptorSetLayout));
	const Unique<VkPipeline>			pipeline(makeComputePipeline(vk, *computeDevice, *pipelineLayout, *shaderModule));
	const VkBufferMemoryBarrier			hostWriteBarrier	= makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer, 0ull, bufferSizeBytes);
	const VkBufferMemoryBarrier			shaderWriteBarrier	= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);
	const Unique<VkCommandPool>			cmdPool(makeCommandPool(vk, *computeDevice, computeQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer>		computeCommandBuffer(allocateCommandBuffer(vk, *computeDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Compute command buffer

	beginCommandBuffer(vk, *computeCommandBuffer);
	vk.cmdBindPipeline(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
	vk.cmdPipelineBarrier(*computeCommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*computeCommandBuffer, 1, 1, 1);
	vk.cmdPipelineBarrier(*computeCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *computeCommandBuffer);

	const VkSubmitInfo	submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// sType
		DE_NULL,								// pNext
		0u,										// waitSemaphoreCount
		DE_NULL,								// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,	// pWaitDstStageMask
		1u,										// commandBufferCount
		&computeCommandBuffer.get(),			// pCommandBuffers
		0u,										// signalSemaphoreCount
		DE_NULL									// pSignalSemaphores
	};

	// Set up draw

	const VkQueue		drawQueue			= m_context.getUniversalQueue();
	const VkDevice		drawDevice			= m_context.getDevice();
	const VkDeviceSize	vertexBufferOffset	= 0;
	const VkBuffer		vertexBuffer		= m_vertexBuffer->object();

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDraw(*m_secCmdBuffer, 6, 1, 2, 0);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDraw(*m_cmdBuffer, 6, 1, 2, 0);

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginLegacyRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdDraw(*m_cmdBuffer, 6, 1, 2, 0);

		endLegacyRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	const VkCommandBuffer	drawCommandBuffer	= m_cmdBuffer.get();
	const bool				useDeviceGroups		= false;
	const deUint32			deviceMask			= 1u;
	const Unique<VkFence>	drawFence(createFence(vk, drawDevice));

	VkDeviceGroupSubmitInfo	deviceGroupSubmitInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO,		//	VkStructureType		sType;
		DE_NULL,										//	const void*			pNext;
		0u,												//	deUint32			waitSemaphoreCount;
		DE_NULL,										//	const deUint32*		pWaitSemaphoreDeviceIndices;
		1u,												//	deUint32			commandBufferCount;
		&deviceMask,									//	const deUint32*		pCommandBufferDeviceMasks;
		0u,												//	deUint32			signalSemaphoreCount;
		DE_NULL,										//	const deUint32*		pSignalSemaphoreDeviceIndices;
	};

	const VkSubmitInfo		drawSubmitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
		useDeviceGroups ? &deviceGroupSubmitInfo : DE_NULL,	// const void*					pNext;
		0u,													// deUint32						waitSemaphoreCount;
		DE_NULL,											// const VkSemaphore*			pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,				// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,													// deUint32						commandBufferCount;
		&drawCommandBuffer,									// const VkCommandBuffer*		pCommandBuffers;
		0u,													// deUint32						signalSemaphoreCount;
		DE_NULL,											// const VkSemaphore*			pSignalSemaphores;
	};

	const Unique<VkFence>	computeFence(createFence(vk, *computeDevice));

	// Submit both compute and draw queues
	VK_CHECK(vk.queueSubmit(computeQueue.queue, 1u, &submitInfo, *computeFence));
	VK_CHECK(vk.queueSubmit(drawQueue, 1u, &drawSubmitInfo, *drawFence));

	int err = ERROR_NONE;

	if (VK_SUCCESS != vk.waitForFences(*computeDevice, 1u, &computeFence.get(), DE_TRUE, ~0ull))
		err = ERROR_WAIT_COMPUTE;

	if (VK_SUCCESS != vk.waitForFences(drawDevice, 1u, &drawFence.get(), DE_TRUE, ~0ull))
		err = ERROR_WAIT_DRAW;

	// Have to wait for all fences before calling fail, or some fence may be left hanging.


#ifdef CTS_USES_VULKANSC
	if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
	{
		if (err == ERROR_WAIT_COMPUTE)
		{
			return tcu::TestStatus::fail("Failed waiting for compute queue fence.");
		}

		if (err == ERROR_WAIT_DRAW)
		{
			return tcu::TestStatus::fail("Failed waiting for draw queue fence.");
		}

		// Validation - compute

		const Allocation&	bufferAllocation	= buffer.getAllocation();
		invalidateAlloc(vk, *computeDevice, bufferAllocation);
		const deUint32*		bufferPtr			= static_cast<deUint32*>(bufferAllocation.getHostPtr());

		for (deUint32 ndx = 0; ndx < numValues; ++ndx)
		{
			const deUint32 res = bufferPtr[ndx];
			const deUint32 inp = inputData[ndx];
			const deUint32 ref = ~inp;

			if (res != ref)
			{
				std::ostringstream msg;
				msg << "Comparison failed (compute) for InOut.values[" << ndx << "] ref:" << ref << " res:" << res << " inp:" << inp;
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}

	// Validation - draw

	tcu::Texture2D referenceFrame(mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));

	referenceFrame.allocLevel(0);

	const deInt32 frameWidth	= referenceFrame.getWidth();
	const deInt32 frameHeight	= referenceFrame.getHeight();

	tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	ReferenceImageCoordinates refCoords;

	for (int y = 0; y < frameHeight; y++)
	{
		const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

		for (int x = 0; x < frameWidth; x++)
		{
			const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

			if ((yCoord >= refCoords.bottom	&&
				 yCoord <= refCoords.top	&&
				 xCoord >= refCoords.left	&&
				 xCoord <= refCoords.right))
				referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
		}
	}

	const VkOffset3D					zeroOffset		= { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess	renderedFrame	= m_colorTargetImage->readSurface(
		drawQueue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
		referenceFrame.getLevel(0), renderedFrame, 0.05f,
		tcu::COMPARE_LOG_RESULT))
	{
		res = QP_TEST_RESULT_FAIL;
	}
	return tcu::TestStatus(res, qpGetTestResultName(res));
}

void checkSupport(Context& context, ConcurrentDraw::TestSpec testSpec)
{
	if (testSpec.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

}	// anonymous

ConcurrentDrawTests::ConcurrentDrawTests (tcu::TestContext &testCtx, const SharedGroupParams groupParams)
	: TestCaseGroup			(testCtx, "concurrent", "concurrent drawing")
	, m_groupParams			(groupParams)
{
	/* Left blank on purpose */
}

void ConcurrentDrawTests::init (void)
{
	ConcurrentDraw::TestSpec testSpec
	{
		{
			{ glu::SHADERTYPE_VERTEX,	"vulkan/draw/VertexFetch.vert" },
			{ glu::SHADERTYPE_FRAGMENT,	"vulkan/draw/VertexFetch.frag" },
			{ glu::SHADERTYPE_COMPUTE,	"vulkan/draw/ConcurrentPayload.comp" }
		},
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		m_groupParams
	};

	addChild(new InstanceFactory<ConcurrentDraw, FunctionSupport1<ConcurrentDraw::TestSpec>>(m_testCtx, "compute_and_triangle_list", "Draws triangle list while running a compute shader", testSpec, FunctionSupport1<ConcurrentDraw::TestSpec>::Args(checkSupport, testSpec)));
}

}	// DrawTests
}	// vkt
