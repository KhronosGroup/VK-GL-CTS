#include "vktApiCommandBuffersTests.hpp"
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*--------------------------------------------------------------------*/

#include <sstream>
#include <time.h>
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"

#include "BufferComputeInstance.hpp"
#include "ComputeInstanceResultBuffer.hpp"

namespace vkt
{
namespace api
{
namespace
{


using namespace vk;

// Global variables
const deUint64								INFINITE_TIMEOUT		= ~(deUint64)0u;

// Testcases
tcu::TestStatus createBufferTest (Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBuffer>				cmdBuf					(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	return tcu::TestStatus::pass("create Command Buffer succeeded");
}

tcu::TestStatus executePrimaryBufferTest (Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			queueFamilyIndex,											//	deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
		};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				primCmdBuf				(createCommandBuffer(vk, vkDevice, &cmdBufParams));
	const VkCmdBufferBeginInfo				primCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			0,															// flags (for later: VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT)
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	// Fill create info struct for event
	const VkEventCreateInfo					eventCreateInfo			=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			DE_NULL,
			0u,
		};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = 0x0000FFFF;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	VK_CHECK(vk.endCommandBuffer(*primCmdBuf));

	const VkFenceCreateInfo					fenceCreateInfo			=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
		};

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// submit primary buffer
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result == VK_EVENT_SET)
		return tcu::TestStatus::pass("Execute Primary Command Buffer succeeded");

	return tcu::TestStatus::fail("Execute Primary Command Buffer FAILED");
}

tcu::TestStatus simultanousUsePrimary (Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			queueFamilyIndex,											//	deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
		};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				primCmdBuf				(createCommandBuffer(vk, vkDevice, &cmdBufParams));
	const VkCmdBufferBeginInfo				primCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			VK_CMD_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,					// flags
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	// Fill create info struct for event
	const VkEventCreateInfo					eventCreateInfo			=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			DE_NULL,
			0u,
		};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = 0x0000FFFF;

		// wait for event
		VK_CHECK(vk.cmdWaitEvents(*primCmdBuf, 1, &event.get(), stageMask, stageMask, 0, DE_NULL));

		// reset event
		vk.cmdResetEvent(*primCmdBuf, *event);
	}
	VK_CHECK(vk.endCommandBuffer(*primCmdBuf));

	const VkFenceCreateInfo					fenceCreateInfo			=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
		};

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence1					(createFence(vk, vkDevice, &fenceCreateInfo));
	const Unique<VkFence>					fence2					(createFence(vk, vkDevice, &fenceCreateInfo));

	// submit first buffer
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence1));

	// submit second buffer
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence2));

	// wait for both buffer to stop at event
	sleep(1);

	// set event
	VK_CHECK(vk.setEvent(vkDevice, *event));

	// wait for end of execution of first buffer
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence1.get(), 0u, INFINITE_TIMEOUT));

	// wait for end of execution of first buffer
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence2.get(), 0u, INFINITE_TIMEOUT));

	// TODO: this will be true if the command buffer was executed only once
	// TODO: add some test that will say if it was executed twice

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result == VK_EVENT_RESET)
		return tcu::TestStatus::pass("Execute Primary Command Buffer succeeded");
	else
		return tcu::TestStatus::fail("Execute Primary Command Buffer FAILED");
}

tcu::TestStatus executeSecondaryBufferTest (Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBuffer>				primCmdBuf				(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer
	const VkCmdBufferCreateInfo				secCmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_SECONDARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBuffer>				secCmdBuf				(createCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCmdBufferBeginInfo				primCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		0,															// flags (for later: VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT)
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
	};

	const VkCmdBufferBeginInfo				secCmdBufBeginInfo		=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		0u,															// flags (for later: VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT)
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
	};

	// Fill create info struct for event
	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
		DE_NULL,
		0u,
	};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
	{
		// record secondary command buffer
		VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
		{
			// allow execution of event during every stage of pipeline
			VkPipelineStageFlags stageMask = 0x0000FFFF;

			// record setting event
			vk.cmdSetEvent(*secCmdBuf, *event,stageMask);
		}

		// end recording of secondary buffers
		VK_CHECK(vk.endCommandBuffer(*secCmdBuf));

		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());
	}
	VK_CHECK(vk.endCommandBuffer(*primCmdBuf));

	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,															// flags
	};

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// submit primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result == VK_EVENT_SET)
		return tcu::TestStatus::pass("Execute Secondary Command Buffer succeeded");

	return tcu::TestStatus::fail("Execute Secondary Command Buffer FAILED");
}

tcu::TestStatus simulatnousUseSecondary (Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			queueFamilyIndex,											//	deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
		};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				primCmdBuf				(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer params
	const VkCmdBufferCreateInfo				secCmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_SECONDARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				secCmdBuf				(createCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCmdBufferBeginInfo				primCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			0,															// flags (for later: VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT)
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	const VkCmdBufferBeginInfo				secCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			VK_CMD_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,					// flags
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	// Fill create info struct for event
	const VkEventCreateInfo					eventCreateInfo			=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			DE_NULL,
			0u,
		};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));

	// execute secondary buffer
	vk.cmdExecuteCommands(*primCmdBuf, 1, &secCmdBuf.get());

	VK_CHECK(vk.endCommandBuffer(*primCmdBuf));

	// record secondary command buffer
	VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));

	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = 0x0000FFFF;

		// wait for event
		VK_CHECK(vk.cmdWaitEvents(*secCmdBuf, 1, &event.get(), stageMask, stageMask, 0, DE_NULL));

		// reset event
		vk.cmdResetEvent(*primCmdBuf, *event);
	}

	// end recording of secondary buffers
	VK_CHECK(vk.endCommandBuffer(*secCmdBuf));



	const VkFenceCreateInfo					fenceCreateInfo			=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
		};

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// submit primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence));

	// wait for both buffers to stop at event
	sleep(1);

	// set event
	VK_CHECK(vk.setEvent(vkDevice, *event));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// TODO: this will be true if the command buffer was executed only once
	// TODO: add some test that will say if it was executed twice

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::pass("Simulatous Secondary Command Buffer Execution succeeded");
	else
		return tcu::TestStatus::fail("Simulatous Secondary Command Buffer Execution FAILED");
}

tcu::TestStatus submitTwicePrimaryTest (Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			queueFamilyIndex,											//	deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
		};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				primCmdBuf				(createCommandBuffer(vk, vkDevice, &cmdBufParams));
	const VkCmdBufferBeginInfo				primCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			0,															// flags
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	// Fill create info struct for event
	const VkEventCreateInfo					eventCreateInfo			=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			DE_NULL,
			0u,
		};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf, &primCmdBufBeginInfo));
	{
		// allow execution of event during every stage of pipeline
		VkPipelineStageFlags stageMask = 0x0000FFFF;

		// record setting event
		vk.cmdSetEvent(*primCmdBuf, *event,stageMask);
	}
	VK_CHECK(vk.endCommandBuffer(*primCmdBuf));

	const VkFenceCreateInfo					fenceCreateInfo			=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
		};

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// submit primary buffer
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Test FAILED");

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// submit primary buffer again
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf.get(), *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if buffer has been executed
	result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Test FAILED");
	else
		return tcu::TestStatus::pass("Submit Twice Test succeeded");


}


tcu::TestStatus submitTwiceSecondaryTest (Context& context)
{

	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			queueFamilyIndex,											//	deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
		};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				primCmdBuf1				(createCommandBuffer(vk, vkDevice, &cmdBufParams));
	const Unique<VkCmdBuffer>				primCmdBuf2				(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	// Secondary Command buffer
	const VkCmdBufferCreateInfo				secCmdBufParams			=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
			DE_NULL,													//	const void*				pNext;
			*cmdPool,													//	VkCmdPool				pool;
			VK_CMD_BUFFER_LEVEL_SECONDARY,								//	VkCmdBufferLevel		level;
			0u,															//	VkCmdBufferCreateFlags	flags;
		};
	const Unique<VkCmdBuffer>				secCmdBuf				(createCommandBuffer(vk, vkDevice, &secCmdBufParams));

	const VkCmdBufferBeginInfo				primCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			0,															// flags (for later: VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT)
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	const VkCmdBufferBeginInfo				secCmdBufBeginInfo		=
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
			DE_NULL,
			0u,															// flags (for later: VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT)
			(VkRenderPass)0u,											// renderPass
			0u,															// subpass
			(VkFramebuffer)0u,											// framebuffer
		};

	// Fill create info struct for event
	const VkEventCreateInfo					eventCreateInfo			=
		{
			VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
			DE_NULL,
			0u,
		};

	// create event that will be used to check if secondary command buffer has been executed
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// reset event
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record first primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf1, &primCmdBufBeginInfo));
	{
		// record secondary command buffer
		VK_CHECK(vk.beginCommandBuffer(*secCmdBuf, &secCmdBufBeginInfo));
		{
			// allow execution of event during every stage of pipeline
			VkPipelineStageFlags stageMask = 0x0000FFFF;

			// record setting event
			vk.cmdSetEvent(*secCmdBuf, *event,stageMask);
		}

		// end recording of secondary buffers
		VK_CHECK(vk.endCommandBuffer(*secCmdBuf));

		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf1, 1, &secCmdBuf.get());
	}
	VK_CHECK(vk.endCommandBuffer(*primCmdBuf1));

	const VkFenceCreateInfo					fenceCreateInfo			=
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
		};

	// create fence to wait for execution of queue
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// submit primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf1.get(), *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if secondary buffer has been executed
	VkResult result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");

	// reset first primary buffer
	vk.resetCommandBuffer( *primCmdBuf1, 0u);

	// reset event to allow receiving it again
	VK_CHECK(vk.resetEvent(vkDevice, *event));

	// record first primary command buffer
	VK_CHECK(vk.beginCommandBuffer(*primCmdBuf2, &primCmdBufBeginInfo));
	{
		// execute secondary buffer
		vk.cmdExecuteCommands(*primCmdBuf2, 1, &secCmdBuf.get());
	}
	// end recording
	VK_CHECK(vk.endCommandBuffer(*primCmdBuf2));

	// submit second primary buffer, the secondary should be executed too
	VK_CHECK(vk.queueSubmit(queue,1, &primCmdBuf2.get(), *fence));

	// wait for end of execution of queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// check if secondary buffer has been executed
	result = vk.getEventStatus(vkDevice,*event);
	if (result != VK_EVENT_SET)
		return tcu::TestStatus::fail("Submit Twice Secondary Command Buffer FAILED");
	else
		return tcu::TestStatus::pass("Submit Twice Secondary Command Buffer succeeded");

}

tcu::TestStatus executeOrderTest (Context& context)
{
	const DeviceInterface&					m_vki					= context.getDeviceInterface();
	const VkDevice							m_device				= context.getDevice();
	const VkQueue							m_queue					= context.getUniversalQueue();
	const deUint32							m_queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	Allocator&								m_allocator				= context.getDefaultAllocator();
	const ComputeInstanceResultBuffer		m_result				(m_vki, m_device, m_allocator);

	enum
	{
		ADDRESSABLE_SIZE = 256, // allocate a lot more than required
	};

	const tcu::Vec4							colorA1					= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4							colorA2					= tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);

	const deUint32							dataOffsetA				= (0u);
	const deUint32							dataOffsetB				= (0u);
	const deUint32							viewOffsetA				= (0u);
	const deUint32							viewOffsetB				= (0u);
	const deUint32							bufferSizeA				= dataOffsetA + ADDRESSABLE_SIZE;
	const deUint32							bufferSizeB				= dataOffsetB + ADDRESSABLE_SIZE;

	de::MovePtr<Allocation>					bufferMemA;
	const Unique<VkBuffer>					bufferA					(createColorDataBuffer(dataOffsetA, bufferSizeA, colorA1, colorA2, &bufferMemA, context));

	de::MovePtr<Allocation>					bufferMemB;
	const Unique<VkBuffer>					bufferB					((Move<VkBuffer>()));

	const Unique<VkDescriptorSetLayout>		descriptorSetLayout		(createDescriptorSetLayout(context));
	const Unique<VkDescriptorPool>			descriptorPool			(createDescriptorPool(context));

	const Unique<VkDescriptorSet>			descriptorSet			(createDescriptorSet(*descriptorPool, *descriptorSetLayout, *bufferA, viewOffsetA, *bufferB, viewOffsetB, m_result.getBuffer(), context));
	const VkDescriptorSet					descriptorSets[]		= { *descriptorSet };
	const int								numDescriptorSets		= DE_LENGTH_OF_ARRAY(descriptorSets);

	const VkPipelineLayoutCreateInfo layoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		numDescriptorSets,											// descriptorSetCount
		&descriptorSetLayout.get(),									// pSetLayouts
		0u,															// pushConstantRangeCount
		DE_NULL,													// pPushConstantRanges
	};
	Unique<VkPipelineLayout>				m_pipelineLayout		(createPipelineLayout(m_vki, m_device, &layoutCreateInfo));

	const Unique<VkShaderModule>			computeModuleGood		(createShaderModule(m_vki, m_device, context.getBinaryCollection().get("compute_good"), (VkShaderModuleCreateFlags)0u));
	const Unique<VkShaderModule>			computeModuleBad 		(createShaderModule(m_vki, m_device, context.getBinaryCollection().get("compute_bad"),  (VkShaderModuleCreateFlags)0u));

	const VkShaderCreateInfo				shaderCreateInfoGood	=
	{
		VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,
		DE_NULL,
		*computeModuleGood,											// module
		"main",														// pName
		0u,															// flags
		VK_SHADER_STAGE_COMPUTE
	};
	const VkShaderCreateInfo				shaderCreateInfoBad	=
	{
		VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,
		DE_NULL,
		*computeModuleBad,											// module
		"main",														// pName
		0u,															// flags
		VK_SHADER_STAGE_COMPUTE
	};

	const Unique<VkShader>					computeShaderGood		(createShader(m_vki, m_device, &shaderCreateInfoGood));
	const Unique<VkShader>					computeShaderBad		(createShader(m_vki, m_device, &shaderCreateInfoBad));

	const VkPipelineShaderStageCreateInfo	csGood					=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		VK_SHADER_STAGE_COMPUTE,									// stage
		*computeShaderGood,											// shader
		DE_NULL,													// pSpecializationInfo
	};

	const VkPipelineShaderStageCreateInfo	csBad					=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		VK_SHADER_STAGE_COMPUTE,									// stage
		*computeShaderBad,											// shader
		DE_NULL,													// pSpecializationInfo
	};

	const VkComputePipelineCreateInfo		createInfoGood			=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		csGood,														// cs
		0u,															// flags
		*m_pipelineLayout,											// descriptorSetLayout.get()
		(VkPipeline)0,												// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const VkComputePipelineCreateInfo		createInfoBad			=
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		csBad,														// cs
		0u,															// flags
		*m_pipelineLayout,											// descriptorSetLayout.get()
		(VkPipeline)0,												// basePipelineHandle
		0u,															// basePipelineIndex
	};

	const Unique<VkPipeline> 				m_pipelineGood			(createComputePipeline(m_vki, m_device, (VkPipelineCache)0u, &createInfoGood));
	const Unique<VkPipeline> 				m_pipelineBad			(createComputePipeline(m_vki, m_device, (VkPipelineCache)0u, &createInfoBad));

	const VkMemoryInputFlags				inputBit				= (VK_MEMORY_INPUT_UNIFORM_READ_BIT) ;
	const VkBufferMemoryBarrier				bufferBarrierA			=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		VK_MEMORY_OUTPUT_HOST_WRITE_BIT,							// outputMask
		inputBit,													// inputMask
		VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
		*bufferA,													// buffer
		(VkDeviceSize)0u,											// offset
		(VkDeviceSize)bufferSizeA,									// size
	};

	const VkBufferMemoryBarrier				bufferBarrierB			=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		VK_MEMORY_OUTPUT_HOST_WRITE_BIT,							// outputMask
		inputBit,													// inputMask
		VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
		*bufferB,													// buffer
		(VkDeviceSize)0u,											// offset
		(VkDeviceSize)bufferSizeB,									// size
	};

	const deUint32							numSrcBuffers			= 1u;

	const deUint32* const					dynamicOffsets			= (DE_NULL);
	const deUint32							numDynamicOffsets		= (0);
	const void* const						preBarriers[]			= { &bufferBarrierA, &bufferBarrierB };
	const int								numPreBarriers			= numSrcBuffers;
	const void* const						postBarriers[]			= { m_result.getResultReadBarrier() };
	const int								numPostBarriers			= DE_LENGTH_OF_ARRAY(postBarriers);
	const tcu::Vec4							refQuadrantValue14		=  (colorA2);
	const tcu::Vec4							refQuadrantValue23		=  (colorA1);
	const tcu::Vec4							references[4]			=
	{
		refQuadrantValue14,
		refQuadrantValue23,
		refQuadrantValue23,
		refQuadrantValue14,
	};
	tcu::Vec4								results[4];

	// submit and wait begin

	const tcu::UVec3 m_numWorkGroups = tcu::UVec3(4, 1, 1);

	const VkCmdPoolCreateInfo				cmdPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,
		DE_NULL,
		m_queueFamilyIndex,											// m_queueFamilyIndex
		VK_CMD_POOL_CREATE_TRANSIENT_BIT,							// flags
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(m_vki, m_device, &cmdPoolCreateInfo));

	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,			// flags
	};

	const VkCmdBufferCreateInfo				cmdBufCreateInfo		=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*cmdPool,													// cmdPool
		VK_CMD_BUFFER_LEVEL_PRIMARY,								// level
		0u,															// flags
	};

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(VkRenderPass)0u,											// renderPass
		0u,															// subpass
		(VkFramebuffer)0u,											// framebuffer
	};

	const Unique<VkFence>					cmdCompleteFence		(createFence(m_vki, m_device, &fenceCreateInfo));
	const Unique<VkCmdBuffer>				cmd						(createCommandBuffer(m_vki, m_device, &cmdBufCreateInfo));
	VK_CHECK(m_vki.beginCommandBuffer(*cmd, &cmdBufBeginInfo));

	m_vki.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineBad);
	m_vki.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineGood);
	m_vki.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, numDescriptorSets, descriptorSets, numDynamicOffsets, dynamicOffsets);

	if (numPreBarriers)
		m_vki.cmdPipelineBarrier(*cmd, 0u, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_FALSE, numPreBarriers, preBarriers);

	m_vki.cmdDispatch(*cmd, m_numWorkGroups.x(), m_numWorkGroups.y(), m_numWorkGroups.z());
	m_vki.cmdPipelineBarrier(*cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, numPostBarriers, postBarriers);
	VK_CHECK(m_vki.endCommandBuffer(*cmd));

	// run
	VK_CHECK(m_vki.queueSubmit(m_queue, 1, &cmd.get(), *cmdCompleteFence));
	VK_CHECK(m_vki.waitForFences(m_device, 1, &cmdCompleteFence.get(), 0u, INFINITE_TIMEOUT)); // \note: timeout is failure

	// submit and wait end
	m_result.readResultContentsTo(&results);

	// verify
	if (results[0] == references[0] &&
		results[1] == references[1] &&
		results[2] == references[2] &&
		results[3] == references[3])
	{
		return tcu::TestStatus::pass("Pass");
	}
	else if (results[0] == tcu::Vec4(-1.0f) &&
			 results[1] == tcu::Vec4(-1.0f) &&
			 results[2] == tcu::Vec4(-1.0f) &&
			 results[3] == tcu::Vec4(-1.0f))
	{
		context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Result buffer was not written to."
		<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Result buffer was not written to");
	}
	else
	{
		context.getTestContext().getLog()
		<< tcu::TestLog::Message
		<< "Error expected ["
		<< references[0] << ", "
		<< references[1] << ", "
		<< references[2] << ", "
		<< references[3] << "], got ["
		<< results[0] << ", "
		<< results[1] << ", "
		<< results[2] << ", "
		<< results[3] << "]"
		<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Invalid result values");
	}
};

tcu::TestStatus explicitResetCmdBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBuffer>				cmdBuf		  		(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
		DE_NULL,
		0u,
		DE_NULL,
	};

	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkEventCreateFlags		flags;
	};
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// Put the command buffer in recording state.
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginInfo));
	{
		vk.cmdSetEvent(*cmdBuf, *event, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
	}
	VK_CHECK(vk.endCommandBuffer(*cmdBuf));

	// We'll use a fence to wait for the execution of the queue
	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkFenceCreateFlags		flags
	};
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// Submitting the command buffer that sets the event to the queue
	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuf.get(), *fence));

	// Waiting for the queue to finish executing
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// Check if the buffer was executed
	if (vk.getEventStatus(vkDevice, *event) != VK_EVENT_SET)
		return tcu::TestStatus::fail("Failed to set the event.");

	// Reset the event
	VK_CHECK(vk.resetEvent(vkDevice, *event));
	if(vk.getEventStatus(vkDevice, *event) != VK_EVENT_RESET)
		return tcu::TestStatus::fail("Failed to reset the event.");

	// Reset the command buffer.
	VK_CHECK(vk.resetCommandBuffer(*cmdBuf, 0));
	// Reset the fence so that we can reuse it
	VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));

	// Submit the command buffer after resetting. It should have no commands
	// recorded, so the event should remain unsignaled.
	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuf.get(), *fence));
	// Waiting for the queue to finish executing
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// Check if the event remained unset.
	if(vk.getEventStatus(vkDevice,*event) == VK_EVENT_RESET)
		return tcu::TestStatus::pass("Buffer was reset correctly.");
	else
		return tcu::TestStatus::fail("Buffer was not reset correctly.");
}

tcu::TestStatus implicitResetCmdBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT					//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	const Unique<VkCmdBuffer>				cmdBuf		  		(createCommandBuffer(vk, vkDevice, &cmdBufParams));

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
		DE_NULL,
		0u,
		DE_NULL,
	};

	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkEventCreateFlags		flags;
	};
	const Unique<VkEvent>					event					(createEvent(vk, vkDevice, &eventCreateInfo));

	// Put the command buffer in recording state.
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginInfo));
	{
		// Set the event
		vk.cmdSetEvent(*cmdBuf, *event, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
	}
	VK_CHECK(vk.endCommandBuffer(*cmdBuf));

	// We'll use a fence to wait for the execution of the queue
	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkFenceCreateFlags		flags
	};
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// Submitting the command buffer that sets the event to the queue
	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuf.get(), *fence));

	// Waiting for the queue to finish executing
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// Check if the buffer was executed
	if (vk.getEventStatus(vkDevice, *event) != VK_EVENT_SET)
		return tcu::TestStatus::fail("Failed to set the event.");

	// Reset the event
	vk.resetEvent(vkDevice, *event);
	if(vk.getEventStatus(vkDevice, *event) != VK_EVENT_RESET)
		return tcu::TestStatus::fail("Failed to reset the event.");

	// Reset the command buffer by putting it in recording state again. This
	// should empty the command buffer.
	VK_CHECK(vk.beginCommandBuffer(*cmdBuf, &cmdBufBeginInfo));
	VK_CHECK(vk.endCommandBuffer(*cmdBuf));
	// Reset the fence so that we can reuse it
	VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));

	// Submit the command buffer after resetting. It should have no commands
	// recorded, so the event should remain unsignaled.
	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuf.get(), *fence));
	// Waiting for the queue to finish executing
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), 0u, INFINITE_TIMEOUT));

	// Check if the event remained unset.
	if(vk.getEventStatus(vkDevice, *event) == VK_EVENT_RESET)
		return tcu::TestStatus::pass("Buffer was reset correctly.");
	else
		return tcu::TestStatus::fail("Buffer was not reset correctly.");
}

tcu::TestStatus bulkResetCmdBufferTest(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const short								BUFFER_COUNT			= 2;

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		0u,															//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	VkCmdBuffer cmdBuffers[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createCommandBuffer(vkDevice, &cmdBufParams, &cmdBuffers[i]));

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
		DE_NULL,
		0u,
		DE_NULL,
	};

	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkEventCreateFlags		flags;
	};
	VkEvent events[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createEvent(vkDevice, &eventCreateInfo, &events[i]));

	// Record the command buffers
	for (short i = 0; i < BUFFER_COUNT; ++i)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[i], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[i], events[i], VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
		}
		VK_CHECK(vk.endCommandBuffer(cmdBuffers[i]));
	}

	// We'll use a fence to wait for the execution of the queue
	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkFenceCreateFlags		flags
	};
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// Submit the alpha command buffer to the queue
	VK_CHECK(vk.queueSubmit(queue, BUFFER_COUNT, cmdBuffers, *fence));
	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));
	// Reset the fence so that we can use it again
	VK_CHECK(vk.resetFences(vkDevice, 1, &fence.get()));

	// Check if the buffers were executed
	for (short i = 0; i < BUFFER_COUNT; ++i)
		if (vk.getEventStatus(vkDevice, events[i]) != VK_EVENT_SET)
			return tcu::TestStatus::fail("Failed to set the event.");

	// Reset the events
	for (short i = 0; i < BUFFER_COUNT; ++i)
	{
		VK_CHECK(vk.resetEvent(vkDevice, events[i]));
		// Check if the event was reset correctly
		if (vk.getEventStatus(vkDevice, events[0]) != VK_EVENT_RESET)
			return tcu::TestStatus::fail("Failed to reset the event.");
	}

	// Reset the command buffers by resetting the command pool
	VK_CHECK(vk.resetCommandPool(vkDevice, *cmdPool, 0u));

	// Submit the command buffers to the queue
	VK_CHECK(vk.queueSubmit(queue, BUFFER_COUNT, cmdBuffers, *fence));
	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

	// Check if the event remained unset.
	for (short i = 0; i < BUFFER_COUNT; ++i)
		if (vk.getEventStatus(vkDevice, events[i]) == VK_EVENT_SET)
			return tcu::TestStatus::fail("Buffers were not reset correctly.");

	return tcu::TestStatus::pass("All buffers were reset correctly.");
}

tcu::TestStatus submitCountNonZero(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const short								BUFFER_COUNT			= 5;

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		0u,															//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	VkCmdBuffer cmdBuffers[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createCommandBuffer(vkDevice, &cmdBufParams, &cmdBuffers[i]));

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
		DE_NULL,
		0u,
		DE_NULL,
	};

	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkEventCreateFlags		flags;
	};
	VkEvent events[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createEvent(vkDevice, &eventCreateInfo, &events[i]));

	// Record the command buffers
	for (short i = 0; i < BUFFER_COUNT; ++i)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[i], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[i], events[i], VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
		}
		VK_CHECK(vk.endCommandBuffer(cmdBuffers[i]));
	}

	// We'll use a fence to wait for the execution of the queue
	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkFenceCreateFlags		flags
	};
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// Submit the alpha command buffer to the queue
	VK_CHECK(vk.queueSubmit(queue, BUFFER_COUNT, cmdBuffers, *fence));
	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

	// Check if the buffers were executed
	for (short i = 0; i < BUFFER_COUNT; ++i)
		if (vk.getEventStatus(vkDevice, events[i]) != VK_EVENT_SET)
			return tcu::TestStatus::fail("Failed to set the event.");

	return tcu::TestStatus::pass("All buffers were submitted and executed correctly.");
}

tcu::TestStatus submitCountEqualZero(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const short								BUFFER_COUNT			= 5;

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		0u,															//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	VkCmdBuffer cmdBuffers[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createCommandBuffer(vkDevice, &cmdBufParams, &cmdBuffers[i]));

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
		DE_NULL,
		0u,
		DE_NULL,
	};

	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkEventCreateFlags		flags;
	};
	VkEvent events[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createEvent(vkDevice, &eventCreateInfo, &events[i]));

	// Record the command buffers
	for (short i = 0; i < BUFFER_COUNT; ++i)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[i], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[i], events[i], VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
		}
		VK_CHECK(vk.endCommandBuffer(cmdBuffers[i]));
	}

	// We'll use a fence to wait for the execution of the queue
	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkFenceCreateFlags		flags
	};
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// Submit the command buffer to the queue
	VK_CHECK(vk.queueSubmit(queue, 0, cmdBuffers, *fence));
	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

	// Check if the buffers were executed
	for (short i = 0; i < BUFFER_COUNT; ++i)
		if (vk.getEventStatus(vkDevice, events[i]) == VK_EVENT_SET)
			return tcu::TestStatus::fail("An even was signaled.");

	return tcu::TestStatus::pass("All buffers were ignored.");
}

tcu::TestStatus submitNullFence(Context& context)
{
	const VkDevice							vkDevice				= context.getDevice();
	const DeviceInterface&					vk						= context.getDeviceInterface();
	const VkQueue							queue					= context.getUniversalQueue();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const short								BUFFER_COUNT			= 2;

	const VkCmdPoolCreateInfo				cmdPoolParams			=
	{
		VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		queueFamilyIndex,											//	deUint32				queueFamilyIndex;
		0u,															//	VkCmdPoolCreateFlags	flags;
	};
	const Unique<VkCmdPool>					cmdPool					(createCommandPool(vk, vkDevice, &cmdPoolParams));

	// Command buffer
	const VkCmdBufferCreateInfo				cmdBufParams			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,					//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		*cmdPool,													//	VkCmdPool				pool;
		VK_CMD_BUFFER_LEVEL_PRIMARY,								//	VkCmdBufferLevel		level;
		0u,															//	VkCmdBufferCreateFlags	flags;
	};
	VkCmdBuffer cmdBuffers[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createCommandBuffer(vkDevice, &cmdBufParams, &cmdBuffers[i]));

	const VkCmdBufferBeginInfo				cmdBufBeginInfo			=
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,
		DE_NULL,
		0u,
		DE_NULL,
	};

	const VkEventCreateInfo					eventCreateInfo			=
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkEventCreateFlags		flags;
	};
	VkEvent events[BUFFER_COUNT];
	for (short i = 0; i < BUFFER_COUNT; ++i)
		VK_CHECK(vk.createEvent(vkDevice, &eventCreateInfo, &events[i]));

	// Record the command buffers
	for (short i = 0; i < BUFFER_COUNT; ++i)
	{
		VK_CHECK(vk.beginCommandBuffer(cmdBuffers[i], &cmdBufBeginInfo));
		{
			vk.cmdSetEvent(cmdBuffers[i], events[i], VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
		}
		VK_CHECK(vk.endCommandBuffer(cmdBuffers[i]));
	}

	// We'll use a fence to wait for the execution of the queue
	const VkFenceCreateInfo					fenceCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,						//	VkStructureType			sType;
		DE_NULL,													//	const void*				pNext;
		0u,															//	VkFenceCreateFlags		flags
	};
	const Unique<VkFence>					fence					(createFence(vk, vkDevice, &fenceCreateInfo));

	// Perform two submissions - one with no fence, the other one with a valid
	// fence Hoping submitting the other buffer will give the first one time to
	// execute
	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuffers[0], DE_NULL));
	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuffers[1], *fence));

	// Wait for the queue
	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, INFINITE_TIMEOUT));

	if (vk.getEventStatus(vkDevice, events[0]) != VK_EVENT_SET)
		return tcu::TestStatus::fail("The first event was not signaled -> the buffer was not executed.");

	return tcu::TestStatus::pass("The first event was signaled -> the buffer with null fence submitted and executed correctly");
}

} // anonymous

// Shaders
void genComputeSource (SourceCollections& programCollection)
{
	const char* const						versionDecl				= glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES);
	std::ostringstream						buf_good;

	buf_good << versionDecl << "\n"
	<< ""
	<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	<< "layout(set = 0, binding = 1, std140) uniform BufferName\n"
	<< "{\n"
	<< "	highp vec4 colorA;\n"
	<< "	highp vec4 colorB;\n"
	<< "} b_instance;\n"
	<< "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
	<< "{\n"
	<< "	highp vec4 read_colors[4];\n"
	<< "} b_out;\n"
	<< "void main(void)\n"
	<< "{\n"
	<< "	highp int quadrant_id = int(gl_WorkGroupID.x);\n"
	<< "	highp vec4 result_color;\n"
	<< "	if (quadrant_id == 1 || quadrant_id == 2)\n"
	<< "		result_color = b_instance.colorA;\n"
	<< "	else\n"
	<< "		result_color = b_instance.colorB;\n"
	<< "	b_out.read_colors[gl_WorkGroupID.x] = result_color;\n"
	<< "}\n";

	programCollection.glslSources.add("compute_good") << glu::ComputeSource(buf_good.str());

	std::ostringstream	buf_bad;

	buf_bad	<< versionDecl << "\n"
	<< ""
	<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	<< "layout(set = 0, binding = 1, std140) uniform BufferName\n"
	<< "{\n"
	<< "	highp vec4 colorA;\n"
	<< "	highp vec4 colorB;\n"
	<< "} b_instance;\n"
	<< "layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
	<< "{\n"
	<< "	highp vec4 read_colors[4];\n"
	<< "} b_out;\n"
	<< "void main(void)\n"
	<< "{\n"
	<< "	highp int quadrant_id = int(gl_WorkGroupID.x);\n"
	<< "	highp vec4 result_color;\n"
	<< "	if (quadrant_id == 1 || quadrant_id == 2)\n"
	<< "		result_color = b_instance.colorA;\n"
	<< "	else\n"
	<< "		result_color = b_instance.colorB;\n"
	<< "	b_out.read_colors[gl_WorkGroupID.x] =  vec4(0.0, 0.0, 0.0, 0.0);\n"
	<< "}\n";

	programCollection.glslSources.add("compute_bad") << glu::ComputeSource(buf_bad.str());
}

tcu::TestCaseGroup* createCommandBuffersTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	commandBuffersTests	(new tcu::TestCaseGroup(testCtx, "command_buffers", "Command Buffers Tests"));

	addFunctionCase				(commandBuffersTests.get(), "create_buffers",				"",	createBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "execute_primary_buffers",		"",	executePrimaryBufferTest);
//	addFunctionCase				(commandBuffersTests.get(), "execute_secondary_buffers",	"",	executeSecondaryBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "submit_twice_primary",			"",	submitTwicePrimaryTest);
	addFunctionCase				(commandBuffersTests.get(), "submit_twice_secondary",		"",	submitTwiceSecondaryTest);
	addFunctionCase				(commandBuffersTests.get(), "simultanous_use_primary",		"",	simultanousUsePrimary);
	addFunctionCase				(commandBuffersTests.get(), "simultanous_use_secondary",	"",	simultanousUseSecondary);
	addFunctionCaseWithPrograms (commandBuffersTests.get(), "order_of_execution", 			"", genComputeSource, executeOrderTest);
	addFunctionCase				(commandBuffersTests.get(), "explicit_reset",				"", explicitResetCmdBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "implicit_reset",				"", implicitResetCmdBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "bulk_reset",					"", bulkResetCmdBufferTest);
	addFunctionCase				(commandBuffersTests.get(), "submit_count_non_zero",		"", submitCountNonZero);
	addFunctionCase				(commandBuffersTests.get(), "submit_count_equal_zero",		"", submitCountEqualZero);
	addFunctionCase				(commandBuffersTests.get(), "submit_null_fence",			"", submitNullFence);

	return commandBuffersTests.release();
}

} // api
} // vkt
