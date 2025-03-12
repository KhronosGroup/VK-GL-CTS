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
#include "vkRef.hpp"
#include "tcuVector.hpp"

namespace vk
{

void beginCommandBuffer(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                        VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

// Begins a secondary command buffer.
// Note if renderPass is not nullptr, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT will be added to the flags.
void beginSecondaryCommandBuffer(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                 const VkRenderPass renderPass         = VK_NULL_HANDLE,
                                 const VkFramebuffer framebuffer       = VK_NULL_HANDLE,
                                 const VkCommandBufferUsageFlags flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                 const void *inhPNext                  = nullptr);

void endCommandBuffer(const DeviceInterface &vk, const VkCommandBuffer commandBuffer);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const uint32_t clearValueCount,
                     const VkClearValue *clearValues, const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE,
                     const void *pNext = nullptr);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const VkClearValue &clearValue,
                     const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea,
                     const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const void *pNext, const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const float clearDepth, const uint32_t clearStencil, const void *pNext,
                     const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::UVec4 &clearColor,
                     const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const float clearDepth, const uint32_t clearStencil,
                     const VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);

void endRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer);

#ifndef CTS_USES_VULKANSC
void beginRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkImageView colorImageView,
                    const VkRect2D &renderArea, const VkClearValue &clearValue,
                    const VkImageLayout imageLayout        = VK_IMAGE_LAYOUT_GENERAL,
                    const VkAttachmentLoadOp loadOperation = VK_ATTACHMENT_LOAD_OP_LOAD,
                    VkRenderingFlagsKHR renderingFlags = 0, const uint32_t layerCount = 1u,
                    const uint32_t viewMask = 0u);

void beginRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkImageView colorImageView,
                    const VkImageView depthStencilImageView, const bool useStencilAttachment,
                    const VkRect2D &renderArea, const VkClearValue &clearColorValue,
                    const VkClearValue &clearDepthValue, const VkImageLayout colorImageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    const VkImageLayout depthImageLayout   = VK_IMAGE_LAYOUT_GENERAL,
                    const VkAttachmentLoadOp loadOperation = VK_ATTACHMENT_LOAD_OP_LOAD,
                    VkRenderingFlagsKHR renderingFlags = 0, const uint32_t layerCount = 1u,
                    const uint32_t viewMask = 0u);

void endRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer);
#endif // CTS_USES_VULKANSC

void submitCommandsAndWait(const DeviceInterface &vk, const VkDevice device, const VkQueue queue,
                           const VkCommandBuffer commandBuffer, const bool useDeviceGroups = false,
                           const uint32_t deviceMask = 1u, const uint32_t waitSemaphoreCount = 0u,
                           const VkSemaphore *waitSemaphores      = nullptr,
                           const VkPipelineStageFlags *waitStages = nullptr, const uint32_t signalSemaphoreCount = 0u,
                           const VkSemaphore *pSignalSemaphores = nullptr);

vk::Move<VkFence> submitCommands(const DeviceInterface &vk, const VkDevice device, const VkQueue queue,
                                 const VkCommandBuffer commandBuffer, const bool useDeviceGroups = false,
                                 const uint32_t deviceMask = 1u, const uint32_t waitSemaphoreCount = 0u,
                                 const VkSemaphore *waitSemaphores      = nullptr,
                                 const VkPipelineStageFlags *waitStages = nullptr,
                                 const uint32_t signalSemaphoreCount    = 0u,
                                 const VkSemaphore *pSignalSemaphores   = nullptr);

void waitForFence(const DeviceInterface &vk, const VkDevice device, const VkFence fence,
                  uint64_t timeoutNanos = (~0ull));

} // namespace vk

#endif // _VKCMDUTIL_HPP
