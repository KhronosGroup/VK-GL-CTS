/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief Implementation of utility classes for various kinds of
 * allocation memory for buffers and images
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferAndImageAllocationUtil.hpp"

#include "deInt32.h"
#include "tcuVectorUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"

#include <sstream>

namespace vkt
{

namespace api
{

void BufferSuballocation::createTestBuffer(const DeviceInterface &vk, VkDevice vkDevice, VkDeviceSize size,
                                           VkBufferUsageFlags usage, Context &context, Allocator &allocator,
                                           Move<VkBuffer> &buffer, const MemoryRequirement &requirement,
                                           de::MovePtr<Allocation> &memory) const
{
    DE_UNREF(context);

    const VkBufferCreateInfo bufferParams{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        size,                                 // VkDeviceSize size;
        usage,                                // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyCount;
        nullptr,                              // const uint32_t* pQueueFamilyIndices;
    };

    buffer = vk::createBuffer(vk, vkDevice, &bufferParams, nullptr);
    memory = allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), requirement);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, memory->getMemory(), 0));
}

void BufferDedicatedAllocation::createTestBuffer(const DeviceInterface &vk, VkDevice vkDevice, VkDeviceSize size,
                                                 VkBufferUsageFlags usage, Context &context, Allocator &allocator,
                                                 Move<VkBuffer> &buffer, const MemoryRequirement &requirement,
                                                 de::MovePtr<Allocation> &memory) const
{
    DE_UNREF(allocator);
    if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
        TCU_THROW(NotSupportedError, "Not supported");

    const InstanceInterface &vkInstance     = context.getInstanceInterface();
    const VkPhysicalDevice vkPhysicalDevice = context.getPhysicalDevice();

    const VkBufferCreateInfo bufferParams{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        0u,                                   // VkBufferCreateFlags flags;
        size,                                 // VkDeviceSize size;
        usage,                                // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyCount;
        nullptr,                              // const uint32_t* pQueueFamilyIndices;
    };

    buffer = vk::createBuffer(vk, vkDevice, &bufferParams, nullptr);
    memory = allocateDedicated(vkInstance, vk, vkPhysicalDevice, vkDevice, buffer.get(), requirement);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, memory->getMemory(), memory->getOffset()));
}

void ImageSuballocation::createTestImage(tcu::IVec2 size, VkFormat format, Context &context, Allocator &allocator,
                                         Move<VkImage> &image, const MemoryRequirement &requirement,
                                         de::MovePtr<Allocation> &memory, VkImageTiling tiling,
                                         VkImageUsageFlags usage) const
{
    const VkDevice vkDevice   = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    const VkImageCreateInfo colorImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,          // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        0u,                                           // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                             // VkImageType imageType;
        format,                                       // VkFormat format;
        {(uint32_t)size.x(), (uint32_t)size.y(), 1u}, // VkExtent3D extent;
        1u,                                           // uint32_t mipLevels;
        1u,                                           // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,                        // uint32_t samples;
        tiling,                                       // VkImageTiling tiling;
        (vk::VkImageUsageFlags)((tiling == VK_IMAGE_TILING_LINEAR) ?
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT :
                                    usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                            // VkSharingMode sharingMode;
        0u,                                                                   // uint32_t queueFamilyCount;
        nullptr,                                                              // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                            // VkImageLayout initialLayout;
    };

    image  = createImage(vk, vkDevice, &colorImageParams);
    memory = allocator.allocate(getImageMemoryRequirements(vk, vkDevice, *image), requirement);
    VK_CHECK(vk.bindImageMemory(vkDevice, *image, memory->getMemory(), memory->getOffset()));
}

void ImageDedicatedAllocation::createTestImage(tcu::IVec2 size, VkFormat format, Context &context, Allocator &allocator,
                                               Move<VkImage> &image, const MemoryRequirement &requirement,
                                               de::MovePtr<Allocation> &memory, VkImageTiling tiling,
                                               VkImageUsageFlags usage) const
{
    DE_UNREF(allocator);
    if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
        TCU_THROW(NotSupportedError, "Not supported");

    const InstanceInterface &vkInstance     = context.getInstanceInterface();
    const VkDevice vkDevice                 = context.getDevice();
    const VkPhysicalDevice vkPhysicalDevice = context.getPhysicalDevice();
    const DeviceInterface &vk               = context.getDeviceInterface();

    const VkImageCreateInfo colorImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,          // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        0u,                                           // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                             // VkImageType imageType;
        format,                                       // VkFormat format;
        {(uint32_t)size.x(), (uint32_t)size.y(), 1u}, // VkExtent3D extent;
        1u,                                           // uint32_t mipLevels;
        1u,                                           // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,                        // uint32_t samples;
        tiling,                                       // VkImageTiling tiling;
        (vk::VkImageUsageFlags)((tiling == VK_IMAGE_TILING_LINEAR) ?
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT :
                                    usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                            // VkSharingMode sharingMode;
        0u,                                                                   // uint32_t queueFamilyCount;
        nullptr,                                                              // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                            // VkImageLayout initialLayout;
    };

    image  = createImage(vk, vkDevice, &colorImageParams);
    memory = allocateDedicated(vkInstance, vk, vkPhysicalDevice, vkDevice, image.get(), requirement);
    VK_CHECK(vk.bindImageMemory(vkDevice, *image, memory->getMemory(), memory->getOffset()));
}

} // namespace api
} // namespace vkt
