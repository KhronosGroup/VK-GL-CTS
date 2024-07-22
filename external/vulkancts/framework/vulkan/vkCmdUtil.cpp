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

void beginCommandBuffer(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags)
{
    const VkCommandBufferBeginInfo commandBufBeginParams = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType                  sType;
        DE_NULL,                                     // const void*                      pNext;
        flags,                                       // VkCommandBufferUsageFlags        flags;
        (const VkCommandBufferInheritanceInfo *)DE_NULL,
    };
    VK_CHECK(vk.beginCommandBuffer(commandBuffer, &commandBufBeginParams));
}

void beginSecondaryCommandBuffer(const DeviceInterface &vkd, const VkCommandBuffer cmdBuffer,
                                 const VkRenderPass renderPass, const VkFramebuffer framebuffer,
                                 const VkCommandBufferUsageFlags flags, const void *inhPNext)
{
    const VkCommandBufferInheritanceInfo inheritanceInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        inhPNext,                                          // const void* pNext;
        renderPass,                                        // VkRenderPass renderPass;
        0u,                                                // uint32_t subpass;
        framebuffer,                                       // VkFramebuffer framebuffer;
        VK_FALSE,                                          // VkBool32 occlusionQueryEnable;
        0u,                                                // VkQueryControlFlags queryFlags;
        0u,                                                // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    const VkCommandBufferUsageFlags extraFlags =
        ((renderPass == VK_NULL_HANDLE) ? static_cast<VkCommandBufferUsageFlagBits>(0) :
                                          VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
    const VkCommandBufferUsageFlags usageFlags = (flags | extraFlags);
    const VkCommandBufferBeginInfo beginInfo   = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &inheritanceInfo,                            // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };

    vkd.beginCommandBuffer(cmdBuffer, &beginInfo);
}

void endCommandBuffer(const DeviceInterface &vk, const VkCommandBuffer commandBuffer)
{
    VK_CHECK(vk.endCommandBuffer(commandBuffer));
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const uint32_t clearValueCount,
                     const VkClearValue *clearValues, const VkSubpassContents contents, const void *pNext)
{
    const VkRenderPassBeginInfo renderPassBeginInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType         sType;
        pNext,                                    // const void*             pNext;
        renderPass,                               // VkRenderPass            renderPass;
        framebuffer,                              // VkFramebuffer           framebuffer;
        renderArea,                               // VkRect2D                renderArea;
        clearValueCount,                          // uint32_t                clearValueCount;
        clearValues,                              // const VkClearValue*     pClearValues;
    };

    vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const VkClearValue &clearValue,
                     const VkSubpassContents contents)
{
    beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, 1u, &clearValue, contents);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const VkSubpassContents contents)
{
    const VkClearValue clearValue = makeClearValueColor(clearColor);

    beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, clearValue, contents);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const void *pNext, const VkSubpassContents contents)
{
    const VkClearValue clearValue = makeClearValueColor(clearColor);

    beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, 1u, &clearValue, contents, pNext);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const float clearDepth, const uint32_t clearStencil, const void *pNext,
                     const VkSubpassContents contents)
{
    const VkClearValue clearValues[] = {
        makeClearValueColor(clearColor),                      // attachment 0
        makeClearValueDepthStencil(clearDepth, clearStencil), // attachment 1
    };

    beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, DE_LENGTH_OF_ARRAY(clearValues),
                    clearValues, contents, pNext);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::UVec4 &clearColor,
                     const VkSubpassContents contents)
{
    const VkClearValue clearValue =
        makeClearValueColorU32(clearColor.x(), clearColor.y(), clearColor.z(), clearColor.w());

    beginRenderPass(vk, commandBuffer, renderPass, framebuffer, renderArea, clearValue, contents);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const VkSubpassContents contents)
{
    const VkRenderPassBeginInfo renderPassBeginInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType         sType;
        DE_NULL,                                  // const void*             pNext;
        renderPass,                               // VkRenderPass            renderPass;
        framebuffer,                              // VkFramebuffer           framebuffer;
        renderArea,                               // VkRect2D                renderArea;
        0u,                                       // uint32_t                clearValueCount;
        DE_NULL,                                  // const VkClearValue*     pClearValues;
    };

    vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
}

void beginRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkRenderPass renderPass,
                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, const tcu::Vec4 &clearColor,
                     const float clearDepth, const uint32_t clearStencil, const VkSubpassContents contents)
{
    const VkClearValue clearValues[] = {
        makeClearValueColor(clearColor),                      // attachment 0
        makeClearValueDepthStencil(clearDepth, clearStencil), // attachment 1
    };

    const VkRenderPassBeginInfo renderPassBeginInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType         sType;
        DE_NULL,                                  // const void*             pNext;
        renderPass,                               // VkRenderPass            renderPass;
        framebuffer,                              // VkFramebuffer           framebuffer;
        renderArea,                               // VkRect2D                renderArea;
        DE_LENGTH_OF_ARRAY(clearValues),          // uint32_t                clearValueCount;
        clearValues,                              // const VkClearValue*     pClearValues;
    };

    vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, contents);
}

void endRenderPass(const DeviceInterface &vk, const VkCommandBuffer commandBuffer)
{
    vk.cmdEndRenderPass(commandBuffer);
}

#ifndef CTS_USES_VULKANSC

void beginRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkImageView colorImageView,
                    const VkRect2D &renderArea, const VkClearValue &clearValue, const VkImageLayout imageLayout,
                    const VkAttachmentLoadOp loadOperation, VkRenderingFlagsKHR renderingFlags,
                    const uint32_t layerCount, const uint32_t viewMask)
{
    VkRenderingAttachmentInfoKHR colorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                         // const void* pNext;
        colorImageView,                                  // VkImageView imageView;
        imageLayout,                                     // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
        loadOperation,                                   // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        clearValue                                       // VkClearValue clearValue;
    };

    VkRenderingInfoKHR renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        DE_NULL,
        renderingFlags,   // VkRenderingFlagsKHR flags;
        renderArea,       // VkRect2D renderArea;
        layerCount,       // uint32_t layerCount;
        viewMask,         // uint32_t viewMask;
        1u,               // uint32_t colorAttachmentCount;
        &colorAttachment, // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        DE_NULL,          // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        DE_NULL,          // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    vk.cmdBeginRendering(commandBuffer, &renderingInfo);
}

void beginRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer, const VkImageView colorImageView,
                    const VkImageView depthStencilImageView, const bool useStencilAttachment,
                    const VkRect2D &renderArea, const VkClearValue &clearColorValue,
                    const VkClearValue &clearDepthValue, const VkImageLayout colorImageLayout,
                    const VkImageLayout depthImageLayout, const VkAttachmentLoadOp loadOperation,
                    VkRenderingFlagsKHR renderingFlags, const uint32_t layerCount, const uint32_t viewMask)
{
    VkRenderingAttachmentInfoKHR colorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                         // const void* pNext;
        colorImageView,                                  // VkImageView imageView;
        colorImageLayout,                                // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
        loadOperation,                                   // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        clearColorValue                                  // VkClearValue clearValue;
    };

    VkRenderingAttachmentInfoKHR depthStencilAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                         // const void* pNext;
        depthStencilImageView,                           // VkImageView imageView;
        depthImageLayout,                                // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
        loadOperation,                                   // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        clearDepthValue                                  // VkClearValue clearValue;
    };

    VkRenderingInfoKHR renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        DE_NULL,
        renderingFlags,          // VkRenderingFlagsKHR flags;
        renderArea,              // VkRect2D renderArea;
        layerCount,              // uint32_t layerCount;
        viewMask,                // uint32_t viewMask;
        1u,                      // uint32_t colorAttachmentCount;
        &colorAttachment,        // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        &depthStencilAttachment, // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        useStencilAttachment ? &depthStencilAttachment :
                               DE_NULL, // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    vk.cmdBeginRendering(commandBuffer, &renderingInfo);
}

void endRendering(const DeviceInterface &vk, const VkCommandBuffer commandBuffer)
{
    vk.cmdEndRendering(commandBuffer);
}

#endif // CTS_USES_VULKANSC

void submitCommandsAndWait(const DeviceInterface &vk, const VkDevice device, const VkQueue queue,
                           const VkCommandBuffer commandBuffer, const bool useDeviceGroups, const uint32_t deviceMask,
                           const uint32_t waitSemaphoreCount, const VkSemaphore *waitSemaphores,
                           const VkPipelineStageFlags *waitStages, const uint32_t signalSemaphoreCount,
                           const VkSemaphore *pSignalSemaphores)
{
    const auto fence = submitCommands(vk, device, queue, commandBuffer, useDeviceGroups, deviceMask, waitSemaphoreCount,
                                      waitSemaphores, waitStages, signalSemaphoreCount, pSignalSemaphores);
    waitForFence(vk, device, *fence);
}

void waitForFence(const DeviceInterface &vk, const VkDevice device, const VkFence fence, uint64_t timeoutNanos)
{
    VK_CHECK(vk.waitForFences(device, 1u, &fence, VK_TRUE, timeoutNanos));
}

vk::Move<VkFence> submitCommands(const DeviceInterface &vk, const VkDevice device, const VkQueue queue,
                                 const VkCommandBuffer commandBuffer, const bool useDeviceGroups,
                                 const uint32_t deviceMask, const uint32_t waitSemaphoreCount,
                                 const VkSemaphore *waitSemaphores, const VkPipelineStageFlags *waitStages,
                                 const uint32_t signalSemaphoreCount, const VkSemaphore *pSignalSemaphores)
{
    // For simplicity. A more complete approach can be found in vkt::sparse::submitCommandsAndWait().
    DE_ASSERT(!(useDeviceGroups && waitSemaphoreCount > 0u));

    if (waitSemaphoreCount > 0u)
    {
        DE_ASSERT(waitSemaphores != nullptr);
        DE_ASSERT(waitStages != nullptr);
    }

    const VkDeviceGroupSubmitInfo deviceGroupSubmitInfo = {
        VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0u,                                         // uint32_t waitSemaphoreCount;
        nullptr,                                    // const uint32_t* pWaitSemaphoreDeviceIndices;
        1u,                                         // uint32_t commandBufferCount;
        &deviceMask,                                // const uint32_t* pCommandBufferDeviceMasks;
        0u,                                         // uint32_t signalSemaphoreCount;
        nullptr,                                    // const uint32_t* pSignalSemaphoreDeviceIndices;
    };

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,                      // VkStructureType sType;
        useDeviceGroups ? &deviceGroupSubmitInfo : nullptr, // const void* pNext;
        waitSemaphoreCount,                                 // uint32_t waitSemaphoreCount;
        waitSemaphores,                                     // const VkSemaphore* pWaitSemaphores;
        waitStages,                                         // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                                                 // uint32_t commandBufferCount;
        &commandBuffer,                                     // const VkCommandBuffer* pCommandBuffers;
        signalSemaphoreCount,                               // uint32_t signalSemaphoreCount;
        pSignalSemaphores,                                  // const VkSemaphore* pSignalSemaphores;
    };

    Move<VkFence> fence(createFence(vk, device));
    VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));

    return fence;
}

} // namespace vk
