#ifndef _VKBARRIERUTIL_HPP
#define _VKBARRIERUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Utility for generating barriers
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

namespace vk
{

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes,
											   const deUint32		srcQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED,
											   const deUint32		dstQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED);

VkBufferMemoryBarrier2KHR makeBufferMemoryBarrier2(const VkPipelineStageFlags2KHR	srcStageMask,
												   const VkAccessFlags2KHR			srcAccessMask,
												   const VkPipelineStageFlags2KHR	dstStageMask,
												   const VkAccessFlags2KHR			dstAccessMask,
												   const VkBuffer					buffer,
												   const VkDeviceSize				offset,
												   const VkDeviceSize				size,
												   const deUint32					srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
												   const deUint32					dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

VkImageMemoryBarrier makeImageMemoryBarrier (const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange,
											 const deUint32					srcQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED,
											 const deUint32					dstQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED);

VkImageMemoryBarrier2KHR makeImageMemoryBarrier2 (const VkPipelineStageFlags2KHR	srcStageMask,
												  const VkAccessFlags2KHR			srcAccessMask,
												  const VkPipelineStageFlags2KHR	dstStageMask,
												  const VkAccessFlags2KHR			dstAccessMask,
												  const VkImageLayout				oldLayout,
												  const VkImageLayout				newLayout,
												  const VkImage						image,
												  const VkImageSubresourceRange		subresourceRange,
												  const deUint32					srcQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED,
												  const deUint32					dstQueueFamilyIndex		= VK_QUEUE_FAMILY_IGNORED);

VkMemoryBarrier makeMemoryBarrier (const VkAccessFlags	srcAccessMask,
								   const VkAccessFlags	dstAccessMask);

VkMemoryBarrier2KHR makeMemoryBarrier2(const VkPipelineStageFlags2KHR	srcStageMask,
									   const VkAccessFlags2KHR			srcAccessMask,
									   const VkPipelineStageFlags2KHR	dstStageMask,
									   const VkAccessFlags2KHR			dstAccessMask);

void cmdPipelineMemoryBarrier		(const DeviceInterface&			vk,
									 const VkCommandBuffer			commandBuffer,
									 const VkPipelineStageFlags		srcStageMask,
									 const VkPipelineStageFlags		dstStageMask,
									 const VkMemoryBarrier*			pMemoryBarriers,
									 const size_t					memoryBarrierCount = 1u,
									 const VkDependencyFlags		dependencyFlags = 0);

void cmdPipelineBufferMemoryBarrier	(const DeviceInterface&			vk,
									 const VkCommandBuffer			commandBuffer,
									 const VkPipelineStageFlags		srcStageMask,
									 const VkPipelineStageFlags		dstStageMask,
									 const VkBufferMemoryBarrier*	pBufferMemoryBarriers,
									 const size_t					bufferMemoryBarrierCount = 1u,
									 const VkDependencyFlags		dependencyFlags = 0);

void cmdPipelineImageMemoryBarrier	(const DeviceInterface&			vk,
									 const VkCommandBuffer			commandBuffer,
									 const VkPipelineStageFlags		srcStageMask,
									 const VkPipelineStageFlags		dstStageMask,
									 const VkImageMemoryBarrier*	pImageMemoryBarriers,
									 const size_t					imageMemoryBarrierCount = 1u,
									 const VkDependencyFlags		dependencyFlags = 0);

} // vk

#endif // _VKBARRIERUTIL_HPP
