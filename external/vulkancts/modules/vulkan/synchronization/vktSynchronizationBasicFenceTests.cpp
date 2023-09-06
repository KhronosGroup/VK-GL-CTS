/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization fence basic tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationBasicFenceTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkRef.hpp"
#include "vkCmdUtil.hpp"

#include <vector>
#include <algorithm>
#include <iterator>

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;
using vkt::synchronization::VideoCodecOperationFlags;

static const deUint64	SHORT_FENCE_WAIT	= 1000ull;			// 1us
static const deUint64	LONG_FENCE_WAIT		= 1000000000ull;	// 1s

struct FenceConfig
{
	uint32_t					numFences;
	VideoCodecOperationFlags	videoCodecOperationFlags;
};

tcu::TestStatus basicOneFenceCase (Context& context, FenceConfig config)
{
	de::MovePtr<VideoDevice>		videoDevice			(config.videoCodecOperationFlags != 0 ? new VideoDevice(context, config.videoCodecOperationFlags) : DE_NULL);
	const DeviceInterface&			vk					= getSyncDeviceInterface(videoDevice, context);
	const VkDevice					device				= getSyncDevice(videoDevice, context);
	const VkQueue					queue				= getSyncQueue(videoDevice, context);
	const deUint32					queueFamilyIndex	= getSyncQueueFamilyIndex(videoDevice, context);
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	const VkFenceCreateInfo			fenceInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkFenceCreateFlags   flags;
	};

	const Unique<VkFence>			fence				(createFence(vk, device, &fenceInfo));

	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// VkStructureType                sType;
		DE_NULL,								// const void*                    pNext;
		0u,										// deUint32                       waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*             pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,	// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,										// deUint32                       commandBufferCount;
		&cmdBuffer.get(),						// const VkCommandBuffer*         pCommandBuffers;
		0u,										// deUint32                       signalSemaphoreCount;
		DE_NULL,								// const VkSemaphore*             pSignalSemaphores;
	};

	if (VK_NOT_READY != vk.getFenceStatus(device, *fence))
		return tcu::TestStatus::fail("Created fence should be in unsignaled state");

	if (VK_TIMEOUT != vk.waitForFences(device, 1u, &fence.get(), VK_TRUE, SHORT_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_TIMEOUT");

	if (VK_NOT_READY != vk.getFenceStatus(device, *fence))
		return tcu::TestStatus::fail("Created fence should be in unsignaled state");

	beginCommandBuffer(vk, *cmdBuffer);
	endCommandBuffer(vk, *cmdBuffer);

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));

	if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS");

	if (VK_SUCCESS != vk.getFenceStatus(device, *fence))
		return tcu::TestStatus::fail("Fence should be in signaled state");

	if (VK_SUCCESS != vk.resetFences(device, 1u, &fence.get()))
		return tcu::TestStatus::fail("Couldn't reset the fence");

	if (VK_NOT_READY != vk.getFenceStatus(device, *fence))
		return tcu::TestStatus::fail("Fence after reset should be in unsignaled state");

	return tcu::TestStatus::pass("Basic one fence tests passed");
}

void checkVideoSupport (Context& context, FenceConfig config)
{
	if (config.videoCodecOperationFlags != 0)
		VideoDevice::checkSupport(context, config.videoCodecOperationFlags);
}

void checkCommandBufferSimultaneousUseSupport (Context& context, FenceConfig config)
{
#ifdef CTS_USES_VULKANSC
	if (context.getDeviceVulkanSC10Properties().commandBufferSimultaneousUse == VK_FALSE)
		TCU_THROW(NotSupportedError, "commandBufferSimultaneousUse is not supported");
#endif

	checkVideoSupport(context, config);
}

tcu::TestStatus basicSignaledCase (Context& context, FenceConfig config)
{
	de::MovePtr<VideoDevice>	videoDevice	(config.videoCodecOperationFlags != 0 ? new VideoDevice(context, config.videoCodecOperationFlags) : DE_NULL);
	const DeviceInterface&		vkd			= getSyncDeviceInterface(videoDevice, context);
	const VkDevice				device		= getSyncDevice(videoDevice, context);

	std::vector<Move<VkFence>> fences;
	fences.reserve(config.numFences);

	const VkFenceCreateInfo fenceCreateInfo =
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	//	VkStructureType		sType;
		nullptr,								//	const void*			pNext;
		VK_FENCE_CREATE_SIGNALED_BIT,			//	VkFenceCreateFlags	flags;
	};

	for (uint32_t i = 0u; i < config.numFences; ++i)
	{
		fences.push_back(createFence(vkd, device, &fenceCreateInfo));
		if (vkd.getFenceStatus(device, fences.back().get()) != VK_SUCCESS)
			TCU_FAIL("Fence was not created signaled");
	}

	std::vector<VkFence> rawFences;
	std::transform(begin(fences), end(fences), std::back_inserter(rawFences), [](const Move<VkFence>& f) { return f.get(); });

	const auto waitResult = vkd.waitForFences(device, static_cast<uint32_t>(rawFences.size()), de::dataOrNull(rawFences), VK_TRUE, LONG_FENCE_WAIT);
	if (waitResult != VK_SUCCESS)
		TCU_FAIL("vkWaitForFences failed with exit status " + std::to_string(waitResult));

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus basicMultiFenceCase (Context& context, FenceConfig config)
{
	enum
	{
		FIRST_FENCE = 0,
		SECOND_FENCE
	};

	de::MovePtr<VideoDevice>		videoDevice			(config.videoCodecOperationFlags != 0 ? new VideoDevice(context, config.videoCodecOperationFlags) : DE_NULL);
	const DeviceInterface&			vk					= getSyncDeviceInterface(videoDevice, context);
	const VkDevice					device				= getSyncDevice(videoDevice, context);
	const VkQueue					queue				= getSyncQueue(videoDevice, context);
	const deUint32					queueFamilyIndex	= getSyncQueueFamilyIndex(videoDevice, context);
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	const VkFenceCreateInfo			fenceInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType      sType;
		DE_NULL,								// const void*          pNext;
		0u,										// VkFenceCreateFlags   flags;
	};

	const Move<VkFence>				ptrFence[2]			=
	{
		createFence(vk, device, &fenceInfo),
		createFence(vk, device, &fenceInfo)
	};

	const VkFence					fence[2]			=
	{
		*ptrFence[FIRST_FENCE],
		*ptrFence[SECOND_FENCE]
	};

	const VkCommandBufferBeginInfo	info				=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,	// VkCommandBufferUsageFlags                flags;
		DE_NULL,										// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// VkStructureType                sType;
		DE_NULL,								// const void*                    pNext;
		0u,										// deUint32                       waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*             pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,	// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,										// deUint32                       commandBufferCount;
		&cmdBuffer.get(),						// const VkCommandBuffer*         pCommandBuffers;
		0u,										// deUint32                       signalSemaphoreCount;
		DE_NULL,								// const VkSemaphore*             pSignalSemaphores;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &info));
	endCommandBuffer(vk, *cmdBuffer);

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence[FIRST_FENCE]));

	if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence[FIRST_FENCE], DE_FALSE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS");

	if (VK_SUCCESS != vk.resetFences(device, 1u, &fence[FIRST_FENCE]))
		return tcu::TestStatus::fail("Couldn't reset the fence");

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence[FIRST_FENCE]));

	if (VK_TIMEOUT != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_TRUE, SHORT_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_TIMEOUT");

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence[SECOND_FENCE]));

	if (VK_SUCCESS != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS");

	return tcu::TestStatus::pass("Basic multi fence tests passed");
}

tcu::TestStatus emptySubmitCase (Context& context, FenceConfig config)
{
	de::MovePtr<VideoDevice>		videoDevice			(config.videoCodecOperationFlags != 0 ? new VideoDevice(context, config.videoCodecOperationFlags) : DE_NULL);
	const DeviceInterface&			vk					= getSyncDeviceInterface(videoDevice, context);
	const VkDevice					device				= getSyncDevice(videoDevice, context);
	const VkQueue					queue				= getSyncQueue(videoDevice, context);

	const VkFenceCreateInfo			fenceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType       sType;
		DE_NULL,								// const void*           pNext;
		(VkFenceCreateFlags)0,					// VkFenceCreateFlags    flags;
	};

	const Unique<VkFence>			fence				(createFence(vk, device, &fenceCreateInfo));

	VK_CHECK(vk.queueSubmit(queue, 0u, DE_NULL, *fence));

	if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS");

	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus basicMultiFenceWaitAllFalseCase (Context& context, FenceConfig config)
{
	enum
	{
		FIRST_FENCE = 0,
		SECOND_FENCE
	};

	de::MovePtr<VideoDevice>		videoDevice			(config.videoCodecOperationFlags != 0 ? new VideoDevice(context, config.videoCodecOperationFlags) : DE_NULL);
	const DeviceInterface&			vk					= getSyncDeviceInterface(videoDevice, context);
	const VkDevice					device				= getSyncDevice(videoDevice, context);
	const VkQueue					queue				= getSyncQueue(videoDevice, context);
	const deUint32					queueFamilyIndex	= getSyncQueueFamilyIndex(videoDevice, context);
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	const VkFenceCreateInfo			fenceInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType     sType;
		DE_NULL,								// const void*         pNext;
		0u,										// VkFenceCreateFlags  flags;
	};

	const Move<VkFence>				ptrFence[2]			=
	{
		createFence(vk, device, &fenceInfo),
		createFence(vk, device, &fenceInfo)
	};

	const VkFence					fence[2]			=
	{
		*ptrFence[FIRST_FENCE],
		*ptrFence[SECOND_FENCE]
	};

	const VkCommandBufferBeginInfo	info				=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,	// VkCommandBufferUsageFlags                flags;
		DE_NULL,										// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	const VkSubmitInfo				submitInfo			=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// VkStructureType                sType;
		DE_NULL,								// const void*                    pNext;
		0u,										// deUint32                       waitSemaphoreCount;
		DE_NULL,								// const VkSemaphore*             pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,	// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,										// deUint32                       commandBufferCount;
		&cmdBuffer.get(),						// const VkCommandBuffer*         pCommandBuffers;
		0u,										// deUint32                       signalSemaphoreCount;
		DE_NULL,								// const VkSemaphore*             pSignalSemaphores;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &info));
	endCommandBuffer(vk, *cmdBuffer);


	if (VK_TIMEOUT != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_FALSE, SHORT_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_TIMEOUT for case: Wait for any fence (No fence has been signaled)");

	if (VK_TIMEOUT != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_TRUE, SHORT_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_TIMEOUT for case: Wait for all fences (No fence has been signaled)");

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence[SECOND_FENCE]));

	if (VK_SUCCESS != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_FALSE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS for case: Wait for any fence (Only second fence signaled)");

	if (VK_TIMEOUT != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_TRUE, SHORT_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_TIMEOUT for case: Wait for all fences (Only second fence signaled)");

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, fence[FIRST_FENCE]));

	if (VK_SUCCESS != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_FALSE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS for case: Wait for any fence (All fences signaled)");

	if (VK_SUCCESS != vk.waitForFences(device, 2u, &fence[FIRST_FENCE], DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("vkWaitForFences should return VK_SUCCESS for case: Wait for all fences (All fences signaled)");

	return tcu::TestStatus::pass("Basic multi fence test without waitAll passed");
}

} // anonymous

tcu::TestCaseGroup* createBasicFenceTests (tcu::TestContext& testCtx, VideoCodecOperationFlags videoCodecOperationFlags)
{
	de::MovePtr<tcu::TestCaseGroup> basicFenceTests(new tcu::TestCaseGroup(testCtx, "fence", "Basic fence tests"));

	addFunctionCase(basicFenceTests.get(),	"one",					"Basic one fence tests",							checkVideoSupport,							basicOneFenceCase, FenceConfig { 0u, videoCodecOperationFlags });
	addFunctionCase(basicFenceTests.get(),	"multi",				"Basic multi fence tests",							checkCommandBufferSimultaneousUseSupport,	basicMultiFenceCase, FenceConfig { 0u, videoCodecOperationFlags });
	addFunctionCase(basicFenceTests.get(),	"empty_submit",			"Signal a fence after an empty queue submission",	checkVideoSupport,							emptySubmitCase, FenceConfig { 0u, videoCodecOperationFlags });
	addFunctionCase(basicFenceTests.get(),	"multi_waitall_false",	"Basic multi fence test without waitAll",			checkCommandBufferSimultaneousUseSupport,	basicMultiFenceWaitAllFalseCase, FenceConfig { 0u, videoCodecOperationFlags });
	addFunctionCase(basicFenceTests.get(),	"one_signaled",			"Create a single signaled fence and wait on it",	checkVideoSupport,							basicSignaledCase, FenceConfig { 1u, videoCodecOperationFlags });
	addFunctionCase(basicFenceTests.get(),	"multiple_signaled",	"Create multiple signaled fences and wait on them",	checkCommandBufferSimultaneousUseSupport,	basicSignaledCase, FenceConfig { 10u, videoCodecOperationFlags });

	return basicFenceTests.release();
}

} // synchronization
} // vkt
