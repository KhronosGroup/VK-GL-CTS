/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief Utilities for commonly used command tasks
 *//*--------------------------------------------------------------------*/

#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vk
{

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

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const deUint32			clearValueCount,
					  const VkClearValue*		clearValues,
					  const VkSubpassContents	contents,
					  const void*				pNext)
{
	const VkRenderPassBeginInfo	renderPassBeginInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
		pNext,										// const void*             pNext;
		renderPass,									// VkRenderPass            renderPass;
		framebuffer,								// VkFramebuffer           framebuffer;
		renderArea,									// VkRect2D                renderArea;
		clearValueCount,							// deUint32                clearValueCount;
		clearValues,								// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const VkClearValue&		clearValue,
					  const VkSubpassContents	contents)
{
	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, 1u, &clearValue, contents);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const VkSubpassContents	contents)
{
	const VkClearValue clearValue = makeClearValueColor(clearColor);

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, clearValue, contents);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const void*				pNext,
					  const VkSubpassContents	contents)
{
	const VkClearValue clearValue = makeClearValueColor(clearColor);

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, 1u, &clearValue, contents, pNext);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const float				clearDepth,
					  const deUint32			clearStencil,
					  const void*				pNext,
					  const VkSubpassContents	contents)
{
	const VkClearValue			clearValues[]		=
	{
		makeClearValueColor(clearColor),						// attachment 0
		makeClearValueDepthStencil(clearDepth, clearStencil),	// attachment 1
	};

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, DE_LENGTH_OF_ARRAY(clearValues), clearValues, contents, pNext);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::UVec4&			clearColor,
					  const VkSubpassContents	contents)
{
	const VkClearValue clearValue = makeClearValueColorU32(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());

	beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, clearValue, contents);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const VkSubpassContents	contents)
{
	const VkRenderPassBeginInfo renderPassBeginInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
		DE_NULL,									// const void*             pNext;
		renderPass,									// VkRenderPass            renderPass;
		framebuffer,								// VkFramebuffer           framebuffer;
		renderArea,									// VkRect2D                renderArea;
		0u,											// deUint32                clearValueCount;
		DE_NULL,									// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const float				clearDepth,
					  const deUint32			clearStencil,
					  const VkSubpassContents	contents)
{
	const VkClearValue			clearValues[]		=
	{
		makeClearValueColor(clearColor),						// attachment 0
		makeClearValueDepthStencil(clearDepth, clearStencil),	// attachment 1
	};

	const VkRenderPassBeginInfo	renderPassBeginInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
		DE_NULL,									// const void*             pNext;
		renderPass,									// VkRenderPass            renderPass;
		framebuffer,								// VkFramebuffer           framebuffer;
		renderArea,									// VkRect2D                renderArea;
		DE_LENGTH_OF_ARRAY(clearValues),			// deUint32                clearValueCount;
		clearValues,								// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
}

void endRenderPass (const DeviceInterface&	vk,
					const VkCommandBuffer	commandBuffer)
{
	vk.cmdEndRenderPass(commandBuffer);
}

void submitCommandsAndWait (const DeviceInterface&		vk,
							const VkDevice				device,
							const VkQueue				queue,
							const VkCommandBuffer		commandBuffer,
							const bool					useDeviceGroups,
							const deUint32				deviceMask,
							const deUint32				waitSemaphoreCount,
							const VkSemaphore*			waitSemaphores,
							const VkPipelineStageFlags*	waitStages)
{
	// For simplicity. A more complete approach can be found in vkt::sparse::submitCommandsAndWait().
	DE_ASSERT(!(useDeviceGroups && waitSemaphoreCount > 0u));

	if (waitSemaphoreCount > 0u)
	{
		DE_ASSERT(waitSemaphores != nullptr);
		DE_ASSERT(waitStages != nullptr);
	}

	const Unique<VkFence>	fence					(createFence(vk, device));

	VkDeviceGroupSubmitInfo	deviceGroupSubmitInfo	=
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

	const VkSubmitInfo		submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
		useDeviceGroups ? &deviceGroupSubmitInfo : DE_NULL,	// const void*					pNext;
		waitSemaphoreCount,									// deUint32						waitSemaphoreCount;
		waitSemaphores,										// const VkSemaphore*			pWaitSemaphores;
		waitStages,											// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,													// deUint32						commandBufferCount;
		&commandBuffer,										// const VkCommandBuffer*		pCommandBuffers;
		0u,													// deUint32						signalSemaphoreCount;
		nullptr,											// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

} // vk
