/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief  Vulkan SC VkCommandPoolMemoryReservationCreateInfo tests
*//*--------------------------------------------------------------------*/

#include "vktCommandPoolMemoryReservationTests.hpp"

#include <set>
#include <vector>
#include <string>

#include "vkRefUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace sc
{

using namespace vk;

namespace
{

typedef de::SharedPtr<vk::Unique<vk::VkEvent> >	VkEventSp;

enum CommandPoolReservedSize
{
	CPS_UNUSED = 0,
	CPS_SMALL,
	CPS_BIG
};

struct TestParams
{
	CommandPoolReservedSize	commandPoolReservedSize;
	deUint32				commandBufferCount;
	deUint32				iterations;
	bool					multipleRecording;
};

void beginCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags)
{
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                  sType;
		DE_NULL,										// const void*                      pNext;
		flags,											// VkCommandBufferUsageFlags        flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &commandBufBeginParams));
}

void endCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	VK_CHECK(vk.endCommandBuffer(commandBuffer));
}


// verify that VkCommandPoolMemoryReservationCreateInfo::commandPoolReservedSize == VkCommandPoolMemoryConsumption::commandPoolReservedSize
tcu::TestStatus verifyCommandPoolReservedSize (Context& context, TestParams testParams)
{
	const VkDevice							device					= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	if ( testParams.commandBufferCount >  context.getDeviceVulkanSC10Properties().maxCommandPoolCommandBuffers )
		TCU_THROW(NotSupportedError, "commandBufferCount is greater than maxCommandPoolCommandBuffers");

	VkDeviceSize							commandPoolReservedSize	= 0u;
	switch (testParams.commandPoolReservedSize)
	{
		case CPS_SMALL:
			commandPoolReservedSize = de::max(VkDeviceSize(64u * context.getTestContext().getCommandLine().getCommandDefaultSize()), VkDeviceSize(context.getTestContext().getCommandLine().getCommandPoolMinSize()) );
			break;
		case CPS_BIG:
			commandPoolReservedSize = de::max(VkDeviceSize(8192u * context.getTestContext().getCommandLine().getCommandDefaultSize()), VkDeviceSize(context.getTestContext().getCommandLine().getCommandPoolMinSize()));
			break;
		default:
			TCU_THROW(InternalError, "Unsupported commandPoolReservedSize value");
	}
	commandPoolReservedSize = de::max(commandPoolReservedSize, VkDeviceSize(testParams.commandBufferCount * context.getTestContext().getCommandLine().getCommandBufferMinSize()));

	// Create command pool with declared size
	// By connecting our own VkCommandPoolMemoryReservationCreateInfo we avoid getting unknown data from DeviceDriverSC::createCommandPoolHandlerNorm()
	VkCommandPoolMemoryReservationCreateInfo cpMemReservationCI		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_RESERVATION_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,														// const void*			pNext
		commandPoolReservedSize,										// VkDeviceSize			commandPoolReservedSize
		testParams.commandBufferCount									// uint32_t				commandPoolMaxCommandBuffers
	};

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		(const void*)&cpMemReservationCI,							//	const void*					pNext;
		0u,															//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, &cmdPoolParams));

	// check if size collected by vkGetCommandPoolMemoryConsumption matches size from VkCommandPoolMemoryReservationCreateInfo

	VkCommandPoolMemoryConsumption			memConsumption			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_CONSUMPTION,	// VkStructureType	sType
		DE_NULL,											// void*			pNext
		0,													// VkDeviceSize		commandPoolAllocated
		0,													// VkDeviceSize		commandPoolReservedSize
		0,													// VkDeviceSize		commandBufferAllocated
	};

	vk.getCommandPoolMemoryConsumption(device, *cmdPool, DE_NULL, &memConsumption);

	if (commandPoolReservedSize != memConsumption.commandPoolReservedSize)
		return tcu::TestStatus::fail("Failed");
	return tcu::TestStatus::pass("Pass");
}

// verify that VkCommandPoolMemoryReservationCreateInfo::commandPoolAllocated == sum of VkCommandPoolMemoryConsumption::commandBufferAllocated
tcu::TestStatus verifyCommandPoolAllocEqualsCommandBufferAlloc (Context& context, TestParams testParams)
{
	const VkDevice							device					= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	// fill command buffers
	deUint32								eventCount				= 0u;
	switch (testParams.commandPoolReservedSize)
	{
		case CPS_SMALL:
			eventCount				= 1u;
			break;
		case CPS_BIG:
			eventCount				= 32u;
			break;
		default:
			TCU_THROW(InternalError, "Unsupported commandPoolReservedSize value");
	}
	VkDeviceSize							commandPoolReservedSize	= de::max(VkDeviceSize(eventCount * context.getTestContext().getCommandLine().getCommandDefaultSize()), VkDeviceSize(context.getTestContext().getCommandLine().getCommandPoolMinSize()));
	commandPoolReservedSize											= de::max(commandPoolReservedSize, VkDeviceSize(testParams.commandBufferCount * context.getTestContext().getCommandLine().getCommandBufferMinSize()));

	// Create command pool with declared size
	// By connecting our own VkCommandPoolMemoryReservationCreateInfo we avoid getting unknown data from DeviceDriverSC::createCommandPoolHandlerNorm()
	VkCommandPoolMemoryReservationCreateInfo cpMemReservationCI		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_RESERVATION_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,														// const void*			pNext
		commandPoolReservedSize,										// VkDeviceSize			commandPoolReservedSize
		testParams.commandBufferCount									// uint32_t				commandPoolMaxCommandBuffers
	};

	const VkCommandPoolCreateInfo			cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					//	VkStructureType				sType;
		(const void*)&cpMemReservationCI,							//	const void*					pNext;
		0u,															//	VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,											//	deUint32					queueFamilyIndex;
	};
	const Unique<VkCommandPool>				cmdPool					(createCommandPool(vk, device, &cmdPoolParams));

	// Allocate command buffers
	std::vector<Move<VkCommandBuffer>>			commandBuffers			(testParams.commandBufferCount);
	const VkCommandBufferAllocateInfo		cmdBufferAllocateInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// VkStructureType             sType;
		DE_NULL,													// const void*                 pNext;
		*cmdPool,													// VkCommandPool               commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// VkCommandBufferLevel        level;
		testParams.commandBufferCount								// deUint32                    commandBufferCount;
	};
    allocateCommandBuffers(vk, device, &cmdBufferAllocateInfo, commandBuffers.data());

	std::vector<VkEventSp>					events;
	for (deUint32 ndx = 0; ndx < eventCount; ++ndx)
		events.push_back(VkEventSp(new vk::Unique<VkEvent>(createEvent(vk, device))));

	bool isOK = true;
	for (deUint32 iter = 0; iter < 2 * testParams.iterations; ++iter)
	{
		// Build command buffers on even iteration
		if (0 == iter % 2)
		{
			if (testParams.multipleRecording)
			{
				for (deUint32 i = 0; i < testParams.commandBufferCount; ++i)
					beginCommandBuffer(vk, commandBuffers[i].get(), 0u);

				for (deUint32 i = 0; i < testParams.commandBufferCount; ++i)
					for (deUint32 j = 0; j < eventCount; ++j)
						vk.cmdSetEvent(commandBuffers[i].get(), events[j]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

				for (deUint32 i = 0; i < testParams.commandBufferCount; ++i)
					endCommandBuffer(vk, commandBuffers[i].get());
			}
			else
			{
				for (deUint32 i = 0; i < testParams.commandBufferCount; ++i)
				{
					beginCommandBuffer(vk, commandBuffers[i].get(), 0u);

					for (deUint32 j = 0; j < eventCount; ++j)
						vk.cmdSetEvent(commandBuffers[i].get(), events[j]->get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

					endCommandBuffer(vk, commandBuffers[i].get());
				}
			}
		}
		else // Reset command buffers on odd iteration
		{
			// leave loop when implementation is not able to perform vkResetCommandPool()
			if (context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
				break;
			vk.resetCommandPool(device, *cmdPool, VkCommandPoolResetFlags(0u));
		}

		// check if size collected by sum of command buffer allocs is equal to command pool alloc
		VkDeviceSize							cbAllocSum				= 0u;
		VkDeviceSize							commandPoolAlloc		= 0u;
		for (deUint32 i = 0; i < testParams.commandBufferCount; ++i)
		{
			VkCommandPoolMemoryConsumption			memConsumption =
			{
				VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_CONSUMPTION,	// VkStructureType	sType
				DE_NULL,											// void*			pNext
				0,													// VkDeviceSize		commandPoolAllocated
				0,													// VkDeviceSize		commandPoolReservedSize
				0,													// VkDeviceSize		commandBufferAllocated
			};
			vk.getCommandPoolMemoryConsumption(device, *cmdPool, commandBuffers[i].get(), &memConsumption);
			cbAllocSum			+=	memConsumption.commandBufferAllocated;
			commandPoolAlloc	=	memConsumption.commandPoolAllocated;
		}
		if (cbAllocSum != commandPoolAlloc)
			isOK = false;
		// if we just performed a vkResetCommandPool() then allocated commandPool memory should be equal to 0
		if ( (1 == iter % 2 ) && commandPoolAlloc != 0u )
			isOK = false;
	}
	return isOK ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Failed");
}

void checkSupport (Context& context, TestParams testParams)
{
	if (testParams.iterations > 1 && context.getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
		TCU_THROW(NotSupportedError, "commandPoolResetCommandBuffer is not supported");
	if (testParams.multipleRecording && context.getDeviceVulkanSC10Properties().commandPoolMultipleCommandBuffersRecording == VK_FALSE)
		TCU_THROW(NotSupportedError, "commandPoolMultipleCommandBuffersRecording is not supported");
	if (testParams.commandBufferCount > context.getDeviceVulkanSC10Properties().maxCommandPoolCommandBuffers)
		TCU_THROW(NotSupportedError, "commandBufferCount is greater than maxCommandPoolCommandBuffers");

}

} // anonymous

tcu::TestCaseGroup*	createCommandPoolMemoryReservationTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "command_pool_memory_reservation", "Tests verifying memory reservation for command pools in Vulkan SC"));

	// add vkGetCommandPoolMemoryConsumption tests
	const struct
	{
		deUint32					commandPoolMaxCommandBuffers;
		const char*					name;
	} maxCommandBuffers[] =
	{
		{ 1,						"cb_single"				},
		{ 4,						"cb_few"				},
		{ 21,						"cb_many"				},
		{ 256,						"cb_min_limit"			},
		{ 1024,						"cb_above_min_limit"	},
	};

	const struct
	{
		CommandPoolReservedSize		commandPoolReservedSize;
		const char*					name;
	} reservedSizes[] =
	{
		{ CPS_SMALL,				"size_small"	},
		{ CPS_BIG,					"size_big"		},
	};

	const struct
	{
		bool						multipleRecording;
		const char*					name;
	} recording[] =
	{
		{ false,					"single_recording"		},
		{ true,						"multiple_recording"	},
	};

	const struct
	{
		deUint32					count;
		const char*					name;
	} iterations[] =
	{
		{ 1u,						"1"				},
		{ 2u,						"2"				},
		{ 4u,						"8"				},
		{ 8u,						"16"			},
	};

	{
		de::MovePtr<tcu::TestCaseGroup>	memConGroup(new tcu::TestCaseGroup(testCtx, "memory_consumption", "Testing vkGetCommandPoolMemoryConsumption"));

		for (int cbIdx = 0; cbIdx < DE_LENGTH_OF_ARRAY(maxCommandBuffers); ++cbIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> cbGroup(new tcu::TestCaseGroup(testCtx, maxCommandBuffers[cbIdx].name, ""));

			for (int sizeIdx = 0; sizeIdx < DE_LENGTH_OF_ARRAY(reservedSizes); ++sizeIdx)
			{
				de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, reservedSizes[sizeIdx].name, ""));

				for (int simIdx = 0; simIdx < DE_LENGTH_OF_ARRAY(recording); ++simIdx)
				{
					de::MovePtr<tcu::TestCaseGroup> simGroup(new tcu::TestCaseGroup(testCtx, recording[simIdx].name, ""));

					if(!recording[simIdx].multipleRecording)
					{
						TestParams	testParams =
						{
							reservedSizes[sizeIdx].commandPoolReservedSize,
							maxCommandBuffers[cbIdx].commandPoolMaxCommandBuffers,
							1u,
							false
						};
						addFunctionCase(simGroup.get(), "reserved_size", "", verifyCommandPoolReservedSize, testParams);
					}

					for (int iterIdx = 0; iterIdx < DE_LENGTH_OF_ARRAY(iterations); ++iterIdx)
					{
						TestParams	testParams =
						{
							reservedSizes[sizeIdx].commandPoolReservedSize,
							maxCommandBuffers[cbIdx].commandPoolMaxCommandBuffers,
							iterations[iterIdx].count,
							recording[simIdx].multipleRecording
						};
						std::ostringstream testName;
						testName << "allocated_size_" << iterations[iterIdx].name;
						addFunctionCase(simGroup.get(), testName.str(), "", checkSupport, verifyCommandPoolAllocEqualsCommandBufferAlloc, testParams);
					}

					sizeGroup->addChild(simGroup.release());
				}
				cbGroup->addChild(sizeGroup.release());
			}
			memConGroup->addChild(cbGroup.release());
		}
		group->addChild(memConGroup.release());
	}

	return group.release();
}

}	// sc

}	// vkt
