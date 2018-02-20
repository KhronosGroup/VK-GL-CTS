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

namespace vk
{

void submitCommandsAndWait (const DeviceInterface&	vk,
							const VkDevice			device,
							const VkQueue			queue,
							const VkCommandBuffer	commandBuffer,
							const bool				useDeviceGroups,
							const deUint32			deviceMask)
{
	const Unique<VkFence>	fence					(createFence(vk, device));

	VkDeviceGroupSubmitInfo	deviceGroupSubmitInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR,	//	VkStructureType		sType;
		DE_NULL,										//	const void*			pNext;
		0u,												//	uint32_t			waitSemaphoreCount;
		DE_NULL,										//	const uint32_t*		pWaitSemaphoreDeviceIndices;
		1u,												//	uint32_t			commandBufferCount;
		&deviceMask,									//	const uint32_t*		pCommandBufferDeviceMasks;
		0u,												//	uint32_t			signalSemaphoreCount;
		DE_NULL,										//	const uint32_t*		pSignalSemaphoreDeviceIndices;
	};

	const VkSubmitInfo		submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
		useDeviceGroups ? &deviceGroupSubmitInfo : DE_NULL,	// const void*					pNext;
		0u,													// deUint32						waitSemaphoreCount;
		DE_NULL,											// const VkSemaphore*			pWaitSemaphores;
		(const VkPipelineStageFlags*)DE_NULL,				// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,													// deUint32						commandBufferCount;
		&commandBuffer,										// const VkCommandBuffer*		pCommandBuffers;
		0u,													// deUint32						signalSemaphoreCount;
		DE_NULL,											// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

} // vk
