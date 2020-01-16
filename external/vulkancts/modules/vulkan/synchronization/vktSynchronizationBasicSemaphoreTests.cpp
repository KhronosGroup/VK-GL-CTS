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
 * \brief Synchronization semaphore basic tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationBasicSemaphoreTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"


#include "vkRef.hpp"

#include "tcuCommandLine.hpp"

namespace vkt
{
namespace synchronization
{
namespace
{

using namespace vk;

struct TestConfig
{
	bool			useTypeCreate;
	VkSemaphoreType	semaphoreType;
};

static const int basicChainLength	= 32768;

Move<VkSemaphore> createTestSemaphore(Context& context, const DeviceInterface& vk, const VkDevice device, const TestConfig& config)
{
	Move<VkSemaphore> semaphore;

	if (config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR && !context.getTimelineSemaphoreFeatures().timelineSemaphore)
		TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

	return Move<VkSemaphore>(config.useTypeCreate ? createSemaphoreType(vk, device, config.semaphoreType) : createSemaphore(vk, device));
}

#define FENCE_WAIT	~0ull

tcu::TestStatus basicOneQueueCase (Context& context, const TestConfig config)
{
	const DeviceInterface&					vk					= context.getDeviceInterface();
	const VkDevice							device				= context.getDevice();
	const VkQueue							queue				= context.getUniversalQueue();
	const deUint32							queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const Unique<VkSemaphore>				semaphore			(createTestSemaphore(context, vk, device, config));
	const Unique<VkCommandPool>				cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));
	const VkCommandBufferBeginInfo			info				=
																{
																	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
																	DE_NULL,										// const void*                              pNext;
																	VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,	// VkCommandBufferUsageFlags                flags;
																	DE_NULL,										// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
																};
	const VkPipelineStageFlags				stageBits[]			= { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
	const deUint64							timelineValue		= 1u;
	const VkTimelineSemaphoreSubmitInfo		timelineWaitInfo	=
																{
																	VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
																	DE_NULL,												// const void*		pNext;
																	1u,														// deUint32			waitSemaphoreValueCount
																	&timelineValue,											// const deUint64*	pWaitSemaphoreValues
																	0u,														// deUint32			signalSemaphoreValueCount
																	DE_NULL,												// const deUint64*	pSignalSemaphoreValues
																};
	const VkTimelineSemaphoreSubmitInfo		timelineSignalInfo	=
																{
																	VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
																	DE_NULL,												// const void*		pNext;
																	0u,														// deUint32			waitSemaphoreValueCount
																	DE_NULL,												// const deUint64*	pWaitSemaphoreValues
																	1u,														// deUint32			signalSemaphoreValueCount
																	&timelineValue,											// const deUint64*	pSignalSemaphoreValues
																};
	const VkSubmitInfo						submitInfo[2]		=
																{
																	{
																		VK_STRUCTURE_TYPE_SUBMIT_INFO,															// VkStructureType			sType;
																		config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR ? &timelineSignalInfo : DE_NULL,	// const void*				pNext;
																		0u,																						// deUint32					waitSemaphoreCount;
																		DE_NULL,																				// const VkSemaphore*		pWaitSemaphores;
																		(const VkPipelineStageFlags*)DE_NULL,
																		1u,																						// deUint32					commandBufferCount;
																		&cmdBuffer.get(),																		// const VkCommandBuffer*	pCommandBuffers;
																		1u,																						// deUint32					signalSemaphoreCount;
																		&semaphore.get(),																		// const VkSemaphore*		pSignalSemaphores;
																	},
																	{
																		VK_STRUCTURE_TYPE_SUBMIT_INFO,															// VkStructureType				sType;
																		config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR ? &timelineWaitInfo : DE_NULL,	// const void*					pNext;
																		1u,																						// deUint32						waitSemaphoreCount;
																		&semaphore.get(),																		// const VkSemaphore*			pWaitSemaphores;
																		stageBits,																				// const VkPipelineStageFlags*	pWaitDstStageMask;
																		1u,																						// deUint32						commandBufferCount;
																		&cmdBuffer.get(),																		// const VkCommandBuffer*		pCommandBuffers;
																		0u,																						// deUint32						signalSemaphoreCount;
																		DE_NULL,																				// const VkSemaphore*			pSignalSemaphores;
																	}
																};
	const Unique<VkFence>					fence				(createFence(vk, device));

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &info));
	endCommandBuffer(vk, *cmdBuffer);
	VK_CHECK(vk.queueSubmit(queue, 2u, submitInfo, *fence));

	if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, FENCE_WAIT))
		return tcu::TestStatus::fail("Basic semaphore tests with one queue failed");

	return tcu::TestStatus::pass("Basic semaphore tests with one queue passed");
}

tcu::TestStatus basicChainCase (Context& context)
{
	VkResult					err			= VK_SUCCESS;
	const DeviceInterface&		vk			= context.getDeviceInterface();
	const VkDevice&				device		= context.getDevice();
	const VkQueue				queue		= context.getUniversalQueue();
	VkPipelineStageFlags		flags		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSemaphoreCreateInfo		sci			= { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, DE_NULL, 0 };
	VkFenceCreateInfo			fci			= { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, DE_NULL, 0 };
	std::vector<VkSemaphore>	semaphores;
	VkFence						fence;

	for (int i = 0; err == VK_SUCCESS && i < basicChainLength; i++)
	{
		VkSemaphore				semaphore;
		err = vk.createSemaphore(device, &sci, DE_NULL, &semaphore);
		if (err == VK_SUCCESS)
		{
			semaphores.push_back(semaphore);

			VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO,
				DE_NULL,
				semaphores.size() > 1 ? 1u : 0u,
				semaphores.size() > 1 ? &semaphores[semaphores.size() - 2] : DE_NULL,
				&flags,
				0,
				DE_NULL,
				1,
				&semaphores[semaphores.size() - 1] };
			err = vk.queueSubmit(queue, 1, &si, 0);
		}
	}

	VK_CHECK(vk.createFence(device, &fci, DE_NULL, &fence));

	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, DE_NULL, 1, &semaphores.back(), &flags, 0, DE_NULL, 0, DE_NULL };
	VK_CHECK(vk.queueSubmit(queue, 1, &si, fence));

	vk.waitForFences(device, 1, &fence, VK_TRUE, ~(0ull));

	vk.destroyFence(device, fence, DE_NULL);

	for (unsigned int i = 0; i < semaphores.size(); i++)
		vk.destroySemaphore(device, semaphores[i], DE_NULL);

	if (err == VK_SUCCESS)
		return tcu::TestStatus::pass("Basic semaphore chain test passed");

	return tcu::TestStatus::fail("Basic semaphore chain test failed");
}

tcu::TestStatus basicChainTimelineCase (Context& context)
{
	VkResult						err			= VK_SUCCESS;
	const DeviceInterface&			vk			= context.getDeviceInterface();
	const VkDevice&					device		= context.getDevice();
	const VkQueue					queue		= context.getUniversalQueue();
	VkPipelineStageFlags			flags		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkSemaphoreTypeCreateInfo		scti		= { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR, DE_NULL, VK_SEMAPHORE_TYPE_TIMELINE_KHR, 0 };
	VkSemaphoreCreateInfo			sci			= { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &scti, 0 };
	VkFenceCreateInfo				fci			= { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, DE_NULL, 0 };
	VkSemaphore						semaphore;
	VkFence							fence;

	if (!context.getTimelineSemaphoreFeatures().timelineSemaphore)
		TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

	VK_CHECK(vk.createSemaphore(device, &sci, DE_NULL, &semaphore));

	for (int i = 0; err == VK_SUCCESS && i < basicChainLength; i++)
	{
		deUint64						waitValue = i;
		deUint64						signalValue = i + 1;
		VkTimelineSemaphoreSubmitInfo	tsi =
		{
			VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
			DE_NULL,												// const void*		pNext;
			i == 0 ? 0u : 1u,										// deUint32			waitSemaphoreValueCount
			&waitValue,												// const deUint64*	pWaitSemaphoreValues
			1u,														// deUint32			signalSemaphoreValueCount
			&signalValue,											// const deUint64*	pSignalSemaphoreValues
		};
		VkSubmitInfo					si =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
			&tsi,							// const void*					pNext;
			i == 0 ? 0u : 1u,				// deUint32						waitSemaphoreCount;
			&semaphore,						// const VkSemaphore*			pWaitSemaphores;
			&flags,							// const VkPipelineStageFlags*	pWaitDstStageMask;
			0,								// deUint32						commandBufferCount;
			DE_NULL,						// const VkCommandBuffer*		pCommandBuffers;
			1,								// deUint32						signalSemaphoreCount;
			&semaphore,						// const VkSemaphore*			pSignalSemaphores;
		};
		err = vk.queueSubmit(queue, 1, &si, 0);
	}

	VK_CHECK(vk.createFence(device, &fci, DE_NULL, &fence));

	deUint64						waitValue = basicChainLength;
	VkTimelineSemaphoreSubmitInfo	tsi =
	{
		VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,		// VkStructureType	sType;
		DE_NULL,												// const void*		pNext;
		1u,														// deUint32			waitSemaphoreValueCount
		&waitValue,												// const deUint64*	pWaitSemaphoreValues
		0u,														// deUint32			signalSemaphoreValueCount
		DE_NULL,												// const deUint64*	pSignalSemaphoreValues
	};
	VkSubmitInfo					si =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		&tsi,							// const void*					pNext;
		1,								// deUint32						waitSemaphoreCount;
		&semaphore,						// const VkSemaphore*			pWaitSemaphores;
		&flags,							// const VkPipelineStageFlags*	pWaitDstStageMask;
		0,								// deUint32						commandBufferCount;
		DE_NULL,						// const VkCommandBuffer*		pCommandBuffers;
		0,								// deUint32						signalSemaphoreCount;
		DE_NULL,						// const VkSemaphore*			pSignalSemaphores;
	};
	VK_CHECK(vk.queueSubmit(queue, 1, &si, fence));

	vk.waitForFences(device, 1, &fence, VK_TRUE, ~(0ull));

	vk.destroyFence(device, fence, DE_NULL);
	vk.destroySemaphore(device, semaphore, DE_NULL);

	if (err == VK_SUCCESS)
		return tcu::TestStatus::pass("Basic semaphore chain test passed");

	return tcu::TestStatus::fail("Basic semaphore chain test failed");
}

tcu::TestStatus basicMultiQueueCase (Context& context, TestConfig config)
{
	enum {NO_MATCH_FOUND = ~((deUint32)0)};
	enum QueuesIndexes {FIRST = 0, SECOND, COUNT};

	struct Queues
	{
		VkQueue		queue;
		deUint32	queueFamilyIndex;
	};


	const DeviceInterface&					vk							= context.getDeviceInterface();
	const InstanceInterface&				instance					= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice				= context.getPhysicalDevice();
	vk::Move<vk::VkDevice>					logicalDevice;
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties;
	VkDeviceCreateInfo						deviceInfo;
	VkPhysicalDeviceFeatures				deviceFeatures;
	const float								queuePriorities[COUNT]		= {1.0f, 1.0f};
	VkDeviceQueueCreateInfo					queueInfos[COUNT];
	Queues									queues[COUNT]				=
																		{
																			{DE_NULL, (deUint32)NO_MATCH_FOUND},
																			{DE_NULL, (deUint32)NO_MATCH_FOUND}
																		};
	const VkCommandBufferBeginInfo			info						=
																		{
																			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
																			DE_NULL,										// const void*                              pNext;
																			VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,	// VkCommandBufferUsageFlags                flags;
																			DE_NULL,										// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
																		};
	Move<VkSemaphore>						semaphore;
	Move<VkCommandPool>						cmdPool[COUNT];
	Move<VkCommandBuffer>					cmdBuffer[COUNT];
	const VkPipelineStageFlags				stageBits[]					= { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
	VkSubmitInfo							submitInfo[COUNT];
	VkTimelineSemaphoreSubmitInfo			timelineSubmitInfo[COUNT];
	deUint64								timelineValues[COUNT];
	Move<VkFence>							fence[COUNT];

	queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);

	for (deUint32 queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
	{
		if (NO_MATCH_FOUND == queues[FIRST].queueFamilyIndex)
			queues[FIRST].queueFamilyIndex = queueNdx;

		if (queues[FIRST].queueFamilyIndex != queueNdx || queueFamilyProperties[queueNdx].queueCount > 1u)
		{
			queues[SECOND].queueFamilyIndex = queueNdx;
			break;
		}
	}

	if (queues[FIRST].queueFamilyIndex == NO_MATCH_FOUND || queues[SECOND].queueFamilyIndex == NO_MATCH_FOUND)
		TCU_THROW(NotSupportedError, "Queues couldn't be created");

	for (int queueNdx = 0; queueNdx < COUNT; ++queueNdx)
	{
		VkDeviceQueueCreateInfo queueInfo;
		deMemset(&queueInfo, 0, sizeof(queueInfo));

		queueInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.pNext				= DE_NULL;
		queueInfo.flags				= (VkDeviceQueueCreateFlags)0u;
		queueInfo.queueFamilyIndex	= queues[queueNdx].queueFamilyIndex;
		queueInfo.queueCount		= (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex) ? 2 : 1;
		queueInfo.pQueuePriorities	= queuePriorities;

		queueInfos[queueNdx]		= queueInfo;

		if (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex)
			break;
	}

	deMemset(&deviceInfo, 0, sizeof(deviceInfo));
	instance.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= DE_NULL;
	deviceInfo.enabledExtensionCount	= 0u;
	deviceInfo.ppEnabledExtensionNames	= DE_NULL;
	deviceInfo.enabledLayerCount		= 0u;
	deviceInfo.ppEnabledLayerNames		= DE_NULL;
	deviceInfo.pEnabledFeatures			= &deviceFeatures;
	deviceInfo.queueCreateInfoCount		= (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex) ? 1 : COUNT;
	deviceInfo.pQueueCreateInfos		= queueInfos;

	logicalDevice = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), context.getInstance(), instance, physicalDevice, &deviceInfo);

	for (deUint32 queueReqNdx = 0; queueReqNdx < COUNT; ++queueReqNdx)
	{
		if (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex)
			vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, queueReqNdx, &queues[queueReqNdx].queue);
		else
			vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, 0u, &queues[queueReqNdx].queue);
	}

	semaphore			= (createTestSemaphore(context, vk, *logicalDevice, config));
	cmdPool[FIRST]		= (createCommandPool(vk, *logicalDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queues[FIRST].queueFamilyIndex));
	cmdPool[SECOND]		= (createCommandPool(vk, *logicalDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queues[SECOND].queueFamilyIndex));
	cmdBuffer[FIRST]	= (makeCommandBuffer(vk, *logicalDevice, *cmdPool[FIRST]));
	cmdBuffer[SECOND]	= (makeCommandBuffer(vk, *logicalDevice, *cmdPool[SECOND]));

	timelineValues[FIRST]									= 1ull;

	timelineSubmitInfo[FIRST].sType							= VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	timelineSubmitInfo[FIRST].pNext							= DE_NULL;
	timelineSubmitInfo[FIRST].waitSemaphoreValueCount		= 0;
	timelineSubmitInfo[FIRST].pWaitSemaphoreValues			= DE_NULL;
	timelineSubmitInfo[FIRST].signalSemaphoreValueCount		= 1;
	timelineSubmitInfo[FIRST].pSignalSemaphoreValues		= &timelineValues[FIRST];

	submitInfo[FIRST].sType									= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo[FIRST].pNext									= config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR ? &timelineSubmitInfo[FIRST] : DE_NULL;
	submitInfo[FIRST].waitSemaphoreCount					= 0u;
	submitInfo[FIRST].pWaitSemaphores						= DE_NULL;
	submitInfo[FIRST].pWaitDstStageMask						= (const VkPipelineStageFlags*)DE_NULL;
	submitInfo[FIRST].commandBufferCount					= 1u;
	submitInfo[FIRST].pCommandBuffers						= &cmdBuffer[FIRST].get();
	submitInfo[FIRST].signalSemaphoreCount					= 1u;
	submitInfo[FIRST].pSignalSemaphores						= &semaphore.get();

	timelineValues[SECOND]									= 2ull;

	timelineSubmitInfo[SECOND].sType						= VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	timelineSubmitInfo[SECOND].pNext						= DE_NULL;
	timelineSubmitInfo[SECOND].waitSemaphoreValueCount		= 1;
	timelineSubmitInfo[SECOND].pWaitSemaphoreValues			= &timelineValues[FIRST];
	timelineSubmitInfo[SECOND].signalSemaphoreValueCount	= 1;
	timelineSubmitInfo[SECOND].pSignalSemaphoreValues		= &timelineValues[SECOND];

	submitInfo[SECOND].sType								= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo[SECOND].pNext								= config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR ? &timelineSubmitInfo[SECOND] : DE_NULL;
	submitInfo[SECOND].waitSemaphoreCount					= 1u;
	submitInfo[SECOND].pWaitSemaphores						= &semaphore.get();
	submitInfo[SECOND].pWaitDstStageMask					= stageBits;
	submitInfo[SECOND].commandBufferCount					= 1u;
	submitInfo[SECOND].pCommandBuffers						= &cmdBuffer[SECOND].get();
	submitInfo[SECOND].signalSemaphoreCount					= 0u;
	submitInfo[SECOND].pSignalSemaphores					= DE_NULL;

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer[FIRST], &info));
	endCommandBuffer(vk, *cmdBuffer[FIRST]);
	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer[SECOND], &info));
	endCommandBuffer(vk, *cmdBuffer[SECOND]);

	fence[FIRST]  = (createFence(vk, *logicalDevice));
	fence[SECOND] = (createFence(vk, *logicalDevice));

	VK_CHECK(vk.queueSubmit(queues[FIRST].queue, 1u, &submitInfo[FIRST], *fence[FIRST]));
	VK_CHECK(vk.queueSubmit(queues[SECOND].queue, 1u, &submitInfo[SECOND], *fence[SECOND]));

	if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[FIRST].get(), DE_TRUE, FENCE_WAIT))
		return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

	if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[SECOND].get(), DE_TRUE, FENCE_WAIT))
		return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

	{
		VkSubmitInfo swapInfo				= submitInfo[SECOND];
		submitInfo[SECOND]					= submitInfo[FIRST];
		submitInfo[FIRST]					= swapInfo;
		submitInfo[SECOND].pCommandBuffers	= &cmdBuffer[SECOND].get();
		submitInfo[FIRST].pCommandBuffers	= &cmdBuffer[FIRST].get();
	}

	VK_CHECK(vk.resetFences(*logicalDevice, 1u, &fence[FIRST].get()));
	VK_CHECK(vk.resetFences(*logicalDevice, 1u, &fence[SECOND].get()));

	if (config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR)
	{
		timelineValues[FIRST]	= 3ull;
		timelineValues[SECOND]	= 4ull;
	}

	VK_CHECK(vk.queueSubmit(queues[SECOND].queue, 1u, &submitInfo[SECOND], *fence[SECOND]));
	VK_CHECK(vk.queueSubmit(queues[FIRST].queue, 1u, &submitInfo[FIRST], *fence[FIRST]));

	if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[FIRST].get(), DE_TRUE, FENCE_WAIT))
		return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

	if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[SECOND].get(), DE_TRUE, FENCE_WAIT))
		return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

	return tcu::TestStatus::pass("Basic semaphore tests with multi queue passed");
}

} // anonymous

tcu::TestCaseGroup* createBasicBinarySemaphoreTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "binary_semaphore", "Basic semaphore tests"));

	for (deUint32 typedCreate = 0; typedCreate < 2; typedCreate++)
	{
		const TestConfig config =
		{
			typedCreate != 0,
			VK_SEMAPHORE_TYPE_BINARY_KHR,
		};
		const std::string createName = config.useTypeCreate ? "_typed" : "";

			addFunctionCase(basicTests.get(), "one_queue" + createName,   "Basic binary semaphore tests with one queue",   basicOneQueueCase, config);
			addFunctionCase(basicTests.get(), "multi_queue" + createName, "Basic binary semaphore tests with multi queue", basicMultiQueueCase, config);
	}

	addFunctionCase(basicTests.get(), "chain", "Binary semaphore chain test", basicChainCase);

	return basicTests.release();
}

tcu::TestCaseGroup* createBasicTimelineSemaphoreTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "timeline_semaphore", "Basic timeline semaphore tests"));
	const TestConfig				config =
	{
		true,
		VK_SEMAPHORE_TYPE_TIMELINE_KHR,
	};

	addFunctionCase(basicTests.get(), "one_queue",   "Basic timeline semaphore tests with one queue",   basicOneQueueCase, config);
	addFunctionCase(basicTests.get(), "multi_queue", "Basic timeline semaphore tests with multi queue", basicMultiQueueCase, config);
	addFunctionCase(basicTests.get(), "chain", "Timeline semaphore chain test", basicChainTimelineCase);

	return basicTests.release();
}

} // synchronization
} // vkt
