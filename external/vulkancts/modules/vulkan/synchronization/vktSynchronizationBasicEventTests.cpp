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
 * \brief Synchronization event basic tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationBasicEventTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkRef.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace synchronization
{
namespace
{

using namespace vk;
#define SHORT_FENCE_WAIT	1000ull
#define LONG_FENCE_WAIT		~0ull

struct TestConfig
{
	SynchronizationType		type;
	VkEventCreateFlags		flags;
};

tcu::TestStatus hostResetSetEventCase (Context& context, TestConfig config)
{
	const DeviceInterface&		vk			= context.getDeviceInterface();
	const VkDevice				device		= context.getDevice();
	const VkEventCreateInfo		eventInfo	=
											{
												VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
												DE_NULL,
												0
											};
	VkEvent						event;
	Move<VkEvent>				ptrEvent;

	DE_UNREF(config);

	if (VK_SUCCESS != vk.createEvent(device, &eventInfo, DE_NULL, &event))
		return tcu::TestStatus::fail("Couldn't create event");

	ptrEvent = Move<VkEvent>(check<VkEvent>(event), Deleter<VkEvent>(vk, device, DE_NULL));

	if (VK_EVENT_RESET != vk.getEventStatus(device, event))
		return tcu::TestStatus::fail("Created event should be in unsignaled state");

	if (VK_SUCCESS != vk.setEvent(device, event))
		return tcu::TestStatus::fail("Couldn't set event");

	if (VK_EVENT_SET != vk.getEventStatus(device, event))
		return tcu::TestStatus::fail("Event should be in signaled state after set");

	if (VK_SUCCESS != vk.resetEvent(device, event))
		return tcu::TestStatus::fail("Couldn't reset event");

	if (VK_EVENT_RESET != vk.getEventStatus(device, event))
		return tcu::TestStatus::fail("Event should be in unsignaled state after reset");

	return tcu::TestStatus::pass("Tests set and reset event on host pass");
}

tcu::TestStatus deviceResetSetEventCase (Context& context, TestConfig config)
{
	const DeviceInterface&				vk						= context.getDeviceInterface();
	const VkDevice						device					= context.getDevice();
	const VkQueue						queue					= context.getUniversalQueue();
	const deUint32						queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const Unique<VkCommandPool>			cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(makeCommandBuffer(vk, device, *cmdPool));
	const Unique<VkEvent>				event					(createEvent(vk, device));
	const VkCommandBufferSubmitInfoKHR	commandBufferSubmitInfo = makeCommonCommandBufferSubmitInfo(cmdBuffer.get());
	const VkMemoryBarrier2KHR			memoryBarrier2			=
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,				// VkStructureType					sType
		DE_NULL,											// const void*						pNext
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
		VK_ACCESS_2_NONE_KHR,								// VkAccessFlags2KHR				srcAccessMask
		VK_PIPELINE_STAGE_2_HOST_BIT_KHR,					// VkPipelineStageFlags2KHR			dstStageMask
		VK_ACCESS_2_HOST_READ_BIT_KHR						// VkAccessFlags2KHR				dstAccessMask
	};
	VkDependencyInfoKHR					dependencyInfo			= makeCommonDependencyInfo(&memoryBarrier2, DE_NULL, DE_NULL, DE_TRUE);

	{
		SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(config.type, vk, DE_FALSE);

		beginCommandBuffer(vk, *cmdBuffer);
		synchronizationWrapper->cmdSetEvent(*cmdBuffer, *event, &dependencyInfo);
		endCommandBuffer(vk, *cmdBuffer);

		synchronizationWrapper->addSubmitInfo(
			0u,										// deUint32								waitSemaphoreInfoCount
			DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			1u,										// deUint32								commandBufferInfoCount
			&commandBufferSubmitInfo,				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			0u,										// deUint32								signalSemaphoreInfoCount
			DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
		);

		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));
	}

	VK_CHECK(vk.queueWaitIdle(queue));

	if (VK_EVENT_SET != vk.getEventStatus(device, *event))
		return tcu::TestStatus::fail("Event should be in signaled state after set");

	{
		SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(config.type, vk, DE_FALSE);

		beginCommandBuffer(vk, *cmdBuffer);
		synchronizationWrapper->cmdResetEvent(*cmdBuffer, *event, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
		endCommandBuffer(vk, *cmdBuffer);

		synchronizationWrapper->addSubmitInfo(
			0u,										// deUint32								waitSemaphoreInfoCount
			DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
			1u,										// deUint32								commandBufferInfoCount
			&commandBufferSubmitInfo,				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
			0u,										// deUint32								signalSemaphoreInfoCount
			DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
		);

		VK_CHECK(synchronizationWrapper->queueSubmit(queue, DE_NULL));
	}

	VK_CHECK(vk.queueWaitIdle(queue));

	if (VK_EVENT_RESET != vk.getEventStatus(device, *event))
		return tcu::TestStatus::fail("Event should be in unsignaled state after set");

	return tcu::TestStatus::pass("Device set and reset event tests pass");
}

tcu::TestStatus singleSubmissionCase (Context& context, TestConfig config)
{
	enum {SET=0, WAIT, COUNT};
	const DeviceInterface&			vk							= context.getDeviceInterface();
	const VkDevice					device						= context.getDevice();
	const VkQueue					queue						= context.getUniversalQueue();
	const deUint32					queueFamilyIndex			= context.getUniversalQueueFamilyIndex();
	const Unique<VkFence>			fence						(createFence(vk, device));
	const Unique<VkCommandPool>		cmdPool						(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Move<VkCommandBuffer>		ptrCmdBuffer[COUNT]			= { makeCommandBuffer(vk, device, *cmdPool), makeCommandBuffer(vk, device, *cmdPool) };
	VkCommandBuffer					cmdBuffers[COUNT]			= {*ptrCmdBuffer[SET], *ptrCmdBuffer[WAIT]};
	const Unique<VkEvent>			event						(createEvent(vk, device, config.flags));
	VkCommandBufferSubmitInfoKHR	commandBufferSubmitInfo[]	{
																	makeCommonCommandBufferSubmitInfo(cmdBuffers[SET]),
																	makeCommonCommandBufferSubmitInfo(cmdBuffers[WAIT])
																};
	VkDependencyInfoKHR				dependencyInfo				= makeCommonDependencyInfo(DE_NULL, DE_NULL, DE_NULL, DE_TRUE);
	SynchronizationWrapperPtr		synchronizationWrapper		= getSynchronizationWrapper(config.type, vk, DE_FALSE);

	synchronizationWrapper->addSubmitInfo(
		0u,										// deUint32								waitSemaphoreInfoCount
		DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
		2u,										// deUint32								commandBufferInfoCount
		commandBufferSubmitInfo,				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
		0u,										// deUint32								signalSemaphoreInfoCount
		DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
	);

	beginCommandBuffer(vk, cmdBuffers[SET]);
	synchronizationWrapper->cmdSetEvent(cmdBuffers[SET], *event, &dependencyInfo);
	endCommandBuffer(vk, cmdBuffers[SET]);

	beginCommandBuffer(vk, cmdBuffers[WAIT]);
	synchronizationWrapper->cmdWaitEvents(cmdBuffers[WAIT], 1u, &event.get(), &dependencyInfo);
	endCommandBuffer(vk, cmdBuffers[WAIT]);

	VK_CHECK(synchronizationWrapper->queueSubmit(queue, *fence));

	if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("Queue should end execution");

	return tcu::TestStatus::pass("Wait and set even on device single submission tests pass");
}

tcu::TestStatus multiSubmissionCase(Context& context, TestConfig config)
{
	enum { SET = 0, WAIT, COUNT };
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	const VkQueue					queue				= context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const Move<VkFence>				ptrFence[COUNT]		= { createFence(vk, device), createFence(vk, device) };
	VkFence							fence[COUNT]		= { *ptrFence[SET], *ptrFence[WAIT] };
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Move<VkCommandBuffer>		ptrCmdBuffer[COUNT] = { makeCommandBuffer(vk, device, *cmdPool), makeCommandBuffer(vk, device, *cmdPool) };
	VkCommandBuffer					cmdBuffers[COUNT]	= { *ptrCmdBuffer[SET], *ptrCmdBuffer[WAIT] };
	const Unique<VkEvent>			event				(createEvent(vk, device, config.flags));
	VkCommandBufferSubmitInfoKHR	commandBufferSubmitInfo[] =
	{
		makeCommonCommandBufferSubmitInfo(cmdBuffers[SET]),
		makeCommonCommandBufferSubmitInfo(cmdBuffers[WAIT])
	};
	SynchronizationWrapperPtr		synchronizationWrapper[] =
	{
		getSynchronizationWrapper(config.type, vk, DE_FALSE),
		getSynchronizationWrapper(config.type, vk, DE_FALSE)
	};
	VkDependencyInfoKHR				dependencyInfos[] =
	{
		makeCommonDependencyInfo(DE_NULL, DE_NULL, DE_NULL, DE_TRUE),
		makeCommonDependencyInfo(DE_NULL, DE_NULL, DE_NULL, DE_TRUE)
	};

	synchronizationWrapper[SET]->addSubmitInfo(
		0u,										// deUint32								waitSemaphoreInfoCount
		DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
		1u,										// deUint32								commandBufferInfoCount
		&commandBufferSubmitInfo[SET],			// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
		0u,										// deUint32								signalSemaphoreInfoCount
		DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
	);

	synchronizationWrapper[WAIT]->addSubmitInfo(
		0u,										// deUint32								waitSemaphoreInfoCount
		DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
		1u,										// deUint32								commandBufferInfoCount
		&commandBufferSubmitInfo[WAIT],			// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
		0u,										// deUint32								signalSemaphoreInfoCount
		DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
	);

	beginCommandBuffer(vk, cmdBuffers[SET]);
	synchronizationWrapper[SET]->cmdSetEvent(cmdBuffers[SET], *event, &dependencyInfos[SET]);
	endCommandBuffer(vk, cmdBuffers[SET]);

	beginCommandBuffer(vk, cmdBuffers[WAIT]);
	synchronizationWrapper[WAIT]->cmdWaitEvents(cmdBuffers[WAIT], 1u, &event.get(), &dependencyInfos[WAIT]);
	endCommandBuffer(vk, cmdBuffers[WAIT]);

	VK_CHECK(synchronizationWrapper[SET]->queueSubmit(queue, fence[SET]));
	VK_CHECK(synchronizationWrapper[WAIT]->queueSubmit(queue, fence[WAIT]));

	if (VK_SUCCESS != vk.waitForFences(device, 2u, fence, DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("Queue should end execution");

	return tcu::TestStatus::pass("Wait and set even on device multi submission tests pass");
}

tcu::TestStatus secondaryCommandBufferCase (Context& context, TestConfig config)
{
	enum {SET=0, WAIT, COUNT};
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkDevice							device					= context.getDevice();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const Unique<VkFence>					fence					(createFence(vk, device));
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Move<VkCommandBuffer>				primaryCmdBuffer		(makeCommandBuffer(vk, device, *cmdPool));
	const VkCommandBufferAllocateInfo		cmdBufferInfo			=
																	{
																		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType		sType;
																		DE_NULL,											// const void*			pNext;
																		*cmdPool,											// VkCommandPool		commandPool;
																		VK_COMMAND_BUFFER_LEVEL_SECONDARY,					// VkCommandBufferLevel	level;
																		1u,													// deUint32				commandBufferCount;
																	};
	const Move<VkCommandBuffer>				prtCmdBuffers[COUNT]	= {allocateCommandBuffer (vk, device, &cmdBufferInfo), allocateCommandBuffer (vk, device, &cmdBufferInfo)};
	VkCommandBuffer							secondaryCmdBuffers[]	= {*prtCmdBuffers[SET], *prtCmdBuffers[WAIT]};
	const Unique<VkEvent>					event					(createEvent(vk, device, config.flags));

	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
																	{
																		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	//VkStructureType					sType;
																		DE_NULL,											//const void*						pNext;
																		DE_NULL,											//VkRenderPass					renderPass;
																		0u,													//deUint32						subpass;
																		DE_NULL,											//VkFramebuffer					framebuffer;
																		VK_FALSE,											//VkBool32						occlusionQueryEnable;
																		(VkQueryControlFlags)0u,							//VkQueryControlFlags				queryFlags;
																		(VkQueryPipelineStatisticFlags)0u,					//VkQueryPipelineStatisticFlags	pipelineStatistics;
																	};
	const VkCommandBufferBeginInfo			cmdBufferBeginInfo		=
																	{
																		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
																		DE_NULL,										// const void*                              pNext;
																		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags                flags;
																		&secCmdBufInheritInfo,							// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
																	};
	VkCommandBufferSubmitInfoKHR			commandBufferSubmitInfo	= makeCommonCommandBufferSubmitInfo(*primaryCmdBuffer);
	VkDependencyInfoKHR						dependencyInfos[]		=
																	{
																		makeCommonDependencyInfo(DE_NULL, DE_NULL, DE_NULL, DE_TRUE),
																		makeCommonDependencyInfo(DE_NULL, DE_NULL, DE_NULL, DE_TRUE)
																	};
	SynchronizationWrapperPtr				synchronizationWrapper	= getSynchronizationWrapper(config.type, vk, DE_FALSE);

	synchronizationWrapper->addSubmitInfo(
		0u,										// deUint32								waitSemaphoreInfoCount
		DE_NULL,								// const VkSemaphoreSubmitInfoKHR*		pWaitSemaphoreInfos
		1u,										// deUint32								commandBufferInfoCount
		&commandBufferSubmitInfo,				// const VkCommandBufferSubmitInfoKHR*	pCommandBufferInfos
		0u,										// deUint32								signalSemaphoreInfoCount
		DE_NULL									// const VkSemaphoreSubmitInfoKHR*		pSignalSemaphoreInfos
	);

	VK_CHECK(vk.beginCommandBuffer(secondaryCmdBuffers[SET], &cmdBufferBeginInfo));
	synchronizationWrapper->cmdSetEvent(secondaryCmdBuffers[SET], *event, &dependencyInfos[SET]);
	endCommandBuffer(vk, secondaryCmdBuffers[SET]);

	VK_CHECK(vk.beginCommandBuffer(secondaryCmdBuffers[WAIT], &cmdBufferBeginInfo));
	synchronizationWrapper->cmdWaitEvents(secondaryCmdBuffers[WAIT], 1u, &event.get(), &dependencyInfos[WAIT]);
	endCommandBuffer(vk, secondaryCmdBuffers[WAIT]);

	beginCommandBuffer(vk, *primaryCmdBuffer);
	vk.cmdExecuteCommands(*primaryCmdBuffer, 2u, secondaryCmdBuffers);
	endCommandBuffer(vk, *primaryCmdBuffer);

	VK_CHECK(synchronizationWrapper->queueSubmit(queue, *fence));

	if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, LONG_FENCE_WAIT))
		return tcu::TestStatus::fail("Queue should end execution");

	return tcu::TestStatus::pass("Wait and set even on device using secondary command buffers tests pass");
}

void checkSupport(Context& context, TestConfig config)
{
	if (config.type == SynchronizationType::SYNCHRONIZATION2)
		context.requireDeviceFunctionality("VK_KHR_synchronization2");

	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getPortabilitySubsetFeatures().events)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
}

} // anonymous

tcu::TestCaseGroup* createBasicEventTests (tcu::TestContext& testCtx)
{
	TestConfig config
	{
		SynchronizationType::LEGACY,
		0U
	};

	de::MovePtr<tcu::TestCaseGroup> basicTests (new tcu::TestCaseGroup(testCtx, "event", "Basic event tests"));

	addFunctionCase(basicTests.get(), "host_set_reset", "Basic event tests set and reset on host", checkSupport, hostResetSetEventCase, config);
	addFunctionCase(basicTests.get(), "device_set_reset", "Basic event tests set and reset on device", checkSupport, deviceResetSetEventCase, config);
	addFunctionCase(basicTests.get(), "single_submit_multi_command_buffer", "Wait and set event single submission on device", checkSupport, singleSubmissionCase, config);
	addFunctionCase(basicTests.get(), "multi_submit_multi_command_buffer", "Wait and set event mutli submission on device", checkSupport, multiSubmissionCase, config);
	addFunctionCase(basicTests.get(), "multi_secondary_command_buffer", "Event used on secondary command buffer ", checkSupport, secondaryCommandBufferCase, config);

	return basicTests.release();
}

tcu::TestCaseGroup* createSynchronization2BasicEventTests (tcu::TestContext& testCtx)
{
	TestConfig config
	{
		SynchronizationType::SYNCHRONIZATION2,
		0U
	};

	de::MovePtr<tcu::TestCaseGroup> basicTests (new tcu::TestCaseGroup(testCtx, "event", "Basic event tests"));

	addFunctionCase(basicTests.get(), "device_set_reset", "Basic event tests set and reset on device", checkSupport, deviceResetSetEventCase, config);
	addFunctionCase(basicTests.get(), "single_submit_multi_command_buffer", "Wait and set event single submission on device", checkSupport, singleSubmissionCase, config);
	addFunctionCase(basicTests.get(), "multi_submit_multi_command_buffer", "Wait and set event mutli submission on device", checkSupport, multiSubmissionCase, config);
	addFunctionCase(basicTests.get(), "multi_secondary_command_buffer", "Event used on secondary command buffer ", checkSupport, secondaryCommandBufferCase, config);

	config.flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR;
	addFunctionCase(basicTests.get(), "single_submit_multi_command_buffer_device_only", "Wait and set GPU-only event single submission", checkSupport, singleSubmissionCase, config);
	addFunctionCase(basicTests.get(), "multi_submit_multi_command_buffer_device_only", "Wait and set GPU-only event mutli submission", checkSupport, multiSubmissionCase, config);
	addFunctionCase(basicTests.get(), "multi_secondary_command_buffer_device_only", "GPU-only event used on secondary command buffer ", checkSupport, secondaryCommandBufferCase, config);

	return basicTests.release();

}

} // synchronization
} // vkt
