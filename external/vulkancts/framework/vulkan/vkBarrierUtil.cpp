/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2023-2025 ARM Ltd.
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

#include "vkBarrierUtil.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC
VkTensorMemoryBarrierARM makeTensorMemoryBarrier(const VkPipelineStageFlags2 srcStageMask,
                                                 const VkAccessFlags2 srcAccessMask,
                                                 const VkPipelineStageFlags2 dstStageMask,
                                                 const VkAccessFlags2 dstAccessMask, const uint32_t srcQueueFamilyIndex,
                                                 const uint32_t dstQueueFamilyIndex, const VkTensorARM tensor)
{
    return {
        VK_STRUCTURE_TYPE_TENSOR_MEMORY_BARRIER_ARM, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        srcStageMask,                                // VkPipelineStageFlags2 srcStageMask;
        srcAccessMask,                               // VkAccessFlags2 srcAccessMask;
        dstStageMask,                                // VkPipelineStageFlags2 dstStageMask;
        dstAccessMask,                               // VkAccessFlags2 dstAccessMask;
        srcQueueFamilyIndex,                         // uint32_t srcQueueFamilyIndex;
        dstQueueFamilyIndex,                         // uint32_t destQueueFamilyIndex;
        tensor,                                      // VkTensorARM tensor;
    };
}
#endif // CTS_USES_VULKANSC

VkBufferMemoryBarrier makeBufferMemoryBarrier(const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask,
                                              const VkBuffer buffer, const VkDeviceSize offset,
                                              const VkDeviceSize bufferSizeBytes, const uint32_t srcQueueFamilyIndex,
                                              const uint32_t dstQueueFamilyIndex)
{
    return {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        srcAccessMask,                           // VkAccessFlags srcAccessMask;
        dstAccessMask,                           // VkAccessFlags dstAccessMask;
        srcQueueFamilyIndex,                     // uint32_t srcQueueFamilyIndex;
        dstQueueFamilyIndex,                     // uint32_t destQueueFamilyIndex;
        buffer,                                  // VkBuffer buffer;
        offset,                                  // VkDeviceSize offset;
        bufferSizeBytes,                         // VkDeviceSize size;
    };
}

VkBufferMemoryBarrier2KHR makeBufferMemoryBarrier2(const VkPipelineStageFlags2KHR srcStageMask,
                                                   const VkAccessFlags2KHR srcAccessMask,
                                                   const VkPipelineStageFlags2KHR dstStageMask,
                                                   const VkAccessFlags2KHR dstAccessMask, const VkBuffer buffer,
                                                   const VkDeviceSize offset, const VkDeviceSize size,
                                                   const uint32_t srcQueueFamilyIndex,
                                                   const uint32_t dstQueueFamilyIndex)
{
    return {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        srcStageMask,                                  // VkPipelineStageFlags2KHR srcStageMask;
        srcAccessMask,                                 // VkAccessFlags2KHR srcAccessMask;
        dstStageMask,                                  // VkPipelineStageFlags2KHR dstStageMask;
        dstAccessMask,                                 // VkAccessFlags2KHR dstAccessMask;
        srcQueueFamilyIndex,                           // uint32_t srcQueueFamilyIndex;
        dstQueueFamilyIndex,                           // uint32_t dstQueueFamilyIndex;
        buffer,                                        // VkBuffer buffer;
        offset,                                        // VkDeviceSize offset;
        size                                           // VkDeviceSize size;
    };
}

VkImageMemoryBarrier makeImageMemoryBarrier(const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask,
                                            const VkImageLayout oldLayout, const VkImageLayout newLayout,
                                            const VkImage image, const VkImageSubresourceRange subresourceRange,
                                            const uint32_t srcQueueFamilyIndex, const uint32_t dstQueueFamilyIndex)
{
    return {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        srcAccessMask,                          // VkAccessFlags outputMask;
        dstAccessMask,                          // VkAccessFlags inputMask;
        oldLayout,                              // VkImageLayout oldLayout;
        newLayout,                              // VkImageLayout newLayout;
        srcQueueFamilyIndex,                    // uint32_t srcQueueFamilyIndex;
        dstQueueFamilyIndex,                    // uint32_t destQueueFamilyIndex;
        image,                                  // VkImage image;
        subresourceRange,                       // VkImageSubresourceRange subresourceRange;
    };
}

VkImageMemoryBarrier2KHR makeImageMemoryBarrier2(const VkPipelineStageFlags2KHR srcStageMask,
                                                 const VkAccessFlags2KHR srcAccessMask,
                                                 const VkPipelineStageFlags2KHR dstStageMask,
                                                 const VkAccessFlags2KHR dstAccessMask, const VkImageLayout oldLayout,
                                                 const VkImageLayout newLayout, const VkImage image,
                                                 const VkImageSubresourceRange subresourceRange,
                                                 const uint32_t srcQueueFamilyIndex, const uint32_t dstQueueFamilyIndex)
{
    return {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        srcStageMask,                                 // VkPipelineStageFlags2KHR srcStageMask;
        srcAccessMask,                                // VkAccessFlags2KHR srcAccessMask;
        dstStageMask,                                 // VkPipelineStageFlags2KHR dstStageMask;
        dstAccessMask,                                // VkAccessFlags2KHR dstAccessMask;
        oldLayout,                                    // VkImageLayout oldLayout;
        newLayout,                                    // VkImageLayout newLayout;
        srcQueueFamilyIndex,                          // uint32_t srcQueueFamilyIndex;
        dstQueueFamilyIndex,                          // uint32_t destQueueFamilyIndex;
        image,                                        // VkImage image;
        subresourceRange,                             // VkImageSubresourceRange subresourceRange;
    };
}

VkMemoryBarrier makeMemoryBarrier(const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask)
{
    return {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                          // const void* pNext;
        srcAccessMask,                    // VkAccessFlags srcAccessMask;
        dstAccessMask,                    // VkAccessFlags dstAccessMask;
    };
}

VkMemoryBarrier2KHR makeMemoryBarrier2(const VkPipelineStageFlags2KHR srcStageMask,
                                       const VkAccessFlags2KHR srcAccessMask,
                                       const VkPipelineStageFlags2KHR dstStageMask,
                                       const VkAccessFlags2KHR dstAccessMask)
{
    return {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        srcStageMask,                           // VkPipelineStageFlags2KHR srcStageMask;
        srcAccessMask,                          // VkAccessFlags2KHR srcAccessMask;
        dstStageMask,                           // VkPipelineStageFlags2KHR dstStageMask;
        dstAccessMask                           // VkAccessFlags2KHR dstAccessMask;
    };
}

void cmdPipelineMemoryBarrier(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                              const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                              const VkMemoryBarrier *pMemoryBarriers, const size_t memoryBarrierCount,
                              const VkDependencyFlags dependencyFlags)
{
    const uint32_t memoryBarrierCount32 = static_cast<uint32_t>(memoryBarrierCount);

    DE_ASSERT(memoryBarrierCount == memoryBarrierCount32);

    vk.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount32,
                          pMemoryBarriers, 0u, nullptr, 0u, nullptr);
}

void cmdPipelineBufferMemoryBarrier(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                    const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                                    const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                    const size_t bufferMemoryBarrierCount, const VkDependencyFlags dependencyFlags)
{
    const uint32_t bufferMemoryBarrierCount32 = static_cast<uint32_t>(bufferMemoryBarrierCount);

    DE_ASSERT(bufferMemoryBarrierCount == bufferMemoryBarrierCount32);

    vk.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0u, nullptr,
                          bufferMemoryBarrierCount32, pBufferMemoryBarriers, 0u, nullptr);
}

void cmdPipelineImageMemoryBarrier(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                   const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask,
                                   const VkImageMemoryBarrier *pImageMemoryBarriers,
                                   const size_t imageMemoryBarrierCount, const VkDependencyFlags dependencyFlags)
{
    const uint32_t imageMemoryBarrierCount32 = static_cast<uint32_t>(imageMemoryBarrierCount);

    DE_ASSERT(imageMemoryBarrierCount == imageMemoryBarrierCount32);

    vk.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, 0u, nullptr, 0u, nullptr,
                          imageMemoryBarrierCount32, pImageMemoryBarriers);
}
} // namespace vk
