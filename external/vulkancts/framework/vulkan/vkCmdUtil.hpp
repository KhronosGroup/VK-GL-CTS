#ifndef _VKCMDUTIL_HPP
#define _VKCMDUTIL_HPP
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

#include "vkDefs.hpp"
#include "tcuVector.hpp"

namespace vk
{

void beginCommandBuffer		(const DeviceInterface&		vk,
							 const VkCommandBuffer		commandBuffer,
							 VkCommandBufferUsageFlags	flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

void endCommandBuffer		(const DeviceInterface&	vk,
							 const VkCommandBuffer	commandBuffer);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const deUint32			clearValueCount,
					  const VkClearValue*		clearValues,
					  const VkSubpassContents	contents			= VK_SUBPASS_CONTENTS_INLINE,
					  const void*				pNext				= DE_NULL);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const VkClearValue&		clearValue,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const void*				pNext,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const float				clearDepth,
					  const deUint32			clearStencil,
					  const void*				pNext,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::UVec4&			clearColor,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const float				clearDepth,
					  const deUint32			clearStencil,
					  const VkSubpassContents	contents = VK_SUBPASS_CONTENTS_INLINE);

void endRenderPass (const DeviceInterface&	vk,
					const VkCommandBuffer	commandBuffer);

void submitCommandsAndWait	(const DeviceInterface&	vk,
							 const VkDevice			device,
							 const VkQueue			queue,
							 const VkCommandBuffer	commandBuffer,
							 const bool				useDeviceGroups = false,
							 const deUint32			deviceMask = 1u);

} // vk

#endif // _VKCMDUTIL_HPP
