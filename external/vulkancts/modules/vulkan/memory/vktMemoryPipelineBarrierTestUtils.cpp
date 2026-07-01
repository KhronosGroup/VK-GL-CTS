/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Pipeline barrier tests - shared base utilities
 *//*--------------------------------------------------------------------*/

#include "vktMemoryPipelineBarrierTestUtils.hpp"

namespace vkt
{
namespace memory
{
namespace pipelinebarrier
{

bool supportsDeviceBufferWrites(Usage usage)
{
    if (usage & USAGE_TRANSFER_DST)
        return true;

    if (usage & USAGE_STORAGE_BUFFER)
        return true;

    if (usage & USAGE_STORAGE_TEXEL_BUFFER)
        return true;

    return false;
}

bool supportsDeviceImageWrites(Usage usage)
{
    if (usage & USAGE_TRANSFER_DST)
        return true;

    if (usage & USAGE_STORAGE_IMAGE)
        return true;

    if (usage & USAGE_COLOR_ATTACHMENT)
        return true;

    return false;
}
Access accessFlagToAccess(vk::VkAccessFlagBits flag)
{
    switch (flag)
    {
    case vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT:
        return ACCESS_INDIRECT_COMMAND_READ_BIT;
    case vk::VK_ACCESS_INDEX_READ_BIT:
        return ACCESS_INDEX_READ_BIT;
    case vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
        return ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case vk::VK_ACCESS_UNIFORM_READ_BIT:
        return ACCESS_UNIFORM_READ_BIT;
    case vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
        return ACCESS_INPUT_ATTACHMENT_READ_BIT;
    case vk::VK_ACCESS_SHADER_READ_BIT:
        return ACCESS_SHADER_READ_BIT;
    case vk::VK_ACCESS_SHADER_WRITE_BIT:
        return ACCESS_SHADER_WRITE_BIT;
    case vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
        return ACCESS_COLOR_ATTACHMENT_READ_BIT;
    case vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
        return ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
        return ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
        return ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case vk::VK_ACCESS_TRANSFER_READ_BIT:
        return ACCESS_TRANSFER_READ_BIT;
    case vk::VK_ACCESS_TRANSFER_WRITE_BIT:
        return ACCESS_TRANSFER_WRITE_BIT;
    case vk::VK_ACCESS_HOST_READ_BIT:
        return ACCESS_HOST_READ_BIT;
    case vk::VK_ACCESS_HOST_WRITE_BIT:
        return ACCESS_HOST_WRITE_BIT;
    case vk::VK_ACCESS_MEMORY_READ_BIT:
        return ACCESS_MEMORY_READ_BIT;
    case vk::VK_ACCESS_MEMORY_WRITE_BIT:
        return ACCESS_MEMORY_WRITE_BIT;

    default:
        DE_FATAL("Unknown access flags");
        return ACCESS_LAST;
    }
}
PipelineStage pipelineStageFlagToPipelineStage(vk::VkPipelineStageFlagBits flag)
{
    switch (flag)
    {
    case vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
        return PIPELINESTAGE_TOP_OF_PIPE_BIT;
    case vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
        return PIPELINESTAGE_BOTTOM_OF_PIPE_BIT;
    case vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
        return PIPELINESTAGE_DRAW_INDIRECT_BIT;
    case vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
        return PIPELINESTAGE_VERTEX_INPUT_BIT;
    case vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
        return PIPELINESTAGE_VERTEX_SHADER_BIT;
    case vk::VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT:
        return PIPELINESTAGE_TESSELLATION_CONTROL_SHADER_BIT;
    case vk::VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT:
        return PIPELINESTAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    case vk::VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
        return PIPELINESTAGE_GEOMETRY_SHADER_BIT;
    case vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
        return PIPELINESTAGE_FRAGMENT_SHADER_BIT;
    case vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
        return PIPELINESTAGE_EARLY_FRAGMENT_TESTS_BIT;
    case vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
        return PIPELINESTAGE_LATE_FRAGMENT_TESTS_BIT;
    case vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
        return PIPELINESTAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
        return PIPELINESTAGE_COMPUTE_SHADER_BIT;
    case vk::VK_PIPELINE_STAGE_TRANSFER_BIT:
        return PIPELINESTAGE_TRANSFER_BIT;
    case vk::VK_PIPELINE_STAGE_HOST_BIT:
        return PIPELINESTAGE_HOST_BIT;

    default:
        DE_FATAL("Unknown pipeline stage flags");
        return PIPELINESTAGE_LAST;
    }
}
string usageToName(Usage usage)
{
    const struct
    {
        Usage usage;
        const char *const name;
    } usageNames[] = {
        {USAGE_HOST_READ, "host_read"},
        {USAGE_HOST_WRITE, "host_write"},

        {USAGE_TRANSFER_SRC, "transfer_src"},
        {USAGE_TRANSFER_DST, "transfer_dst"},

        {USAGE_INDEX_BUFFER, "index_buffer"},
        {USAGE_VERTEX_BUFFER, "vertex_buffer"},
        {USAGE_UNIFORM_BUFFER, "uniform_buffer"},
        {USAGE_STORAGE_BUFFER, "storage_buffer"},
        {USAGE_UNIFORM_TEXEL_BUFFER, "uniform_texel_buffer"},
        {USAGE_STORAGE_TEXEL_BUFFER, "storage_texel_buffer"},
        {USAGE_INDIRECT_BUFFER, "indirect_buffer"},
        {USAGE_SAMPLED_IMAGE, "image_sampled"},
        {USAGE_STORAGE_IMAGE, "storage_image"},
        {USAGE_COLOR_ATTACHMENT, "color_attachment"},
        {USAGE_INPUT_ATTACHMENT, "input_attachment"},
        {USAGE_DEPTH_STENCIL_ATTACHMENT, "depth_stencil_attachment"},
    };

    std::ostringstream stream;
    bool first = true;

    for (size_t usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usageNames); usageNdx++)
    {
        if (usage & usageNames[usageNdx].usage)
        {
            if (!first)
                stream << "_";
            else
                first = false;

            stream << usageNames[usageNdx].name;
        }
    }

    return stream.str();
}

vk::VkBufferUsageFlags usageToBufferUsageFlags(Usage usage)
{
    vk::VkBufferUsageFlags flags = 0;

    if (usage & USAGE_TRANSFER_SRC)
        flags |= vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (usage & USAGE_TRANSFER_DST)
        flags |= vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (usage & USAGE_INDEX_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (usage & USAGE_VERTEX_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (usage & USAGE_INDIRECT_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    if (usage & USAGE_UNIFORM_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (usage & USAGE_STORAGE_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (usage & USAGE_UNIFORM_TEXEL_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

    if (usage & USAGE_STORAGE_TEXEL_BUFFER)
        flags |= vk::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

    return flags;
}

vk::VkImageUsageFlags usageToImageUsageFlags(Usage usage)
{
    vk::VkImageUsageFlags flags = 0;

    if (usage & USAGE_TRANSFER_SRC)
        flags |= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (usage & USAGE_TRANSFER_DST)
        flags |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (usage & USAGE_SAMPLED_IMAGE)
        flags |= vk::VK_IMAGE_USAGE_SAMPLED_BIT;

    if (usage & USAGE_STORAGE_IMAGE)
        flags |= vk::VK_IMAGE_USAGE_STORAGE_BIT;

    if (usage & USAGE_COLOR_ATTACHMENT)
        flags |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (usage & USAGE_INPUT_ATTACHMENT)
        flags |= vk::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    if (usage & USAGE_DEPTH_STENCIL_ATTACHMENT)
        flags |= vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    return flags;
}

vk::VkPipelineStageFlags usageToStageFlags(Usage usage, TestBackend backend)
{
    vk::VkPipelineStageFlags flags = 0;

    if (usage & (USAGE_HOST_READ | USAGE_HOST_WRITE))
        flags |= vk::VK_PIPELINE_STAGE_HOST_BIT;

    if (usage & (USAGE_TRANSFER_SRC | USAGE_TRANSFER_DST))
        flags |= vk::VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (usage & (USAGE_VERTEX_BUFFER | USAGE_INDEX_BUFFER))
        flags |= vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

    if (usage & USAGE_INDIRECT_BUFFER)
        flags |= vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

    if (usage & (USAGE_UNIFORM_BUFFER | USAGE_STORAGE_BUFFER | USAGE_UNIFORM_TEXEL_BUFFER | USAGE_STORAGE_TEXEL_BUFFER |
                 USAGE_SAMPLED_IMAGE | USAGE_STORAGE_IMAGE))
    {
        // Vertex/fragment shader stages require a graphics-capable queue; compute shader stage
        // requires a compute-capable queue. Neither is guaranteed on a transfer-only queue.
        if (backend == BACKEND_GRAPHICS)
            flags |= vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if (backend != BACKEND_TRANSFER)
            flags |= vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    if (usage & USAGE_INPUT_ATTACHMENT)
        flags |= vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (usage & USAGE_COLOR_ATTACHMENT)
        flags |= vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (usage & USAGE_DEPTH_STENCIL_ATTACHMENT)
    {
        flags |= vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

    return flags;
}

vk::VkAccessFlags usageToAccessFlags(Usage usage)
{
    vk::VkAccessFlags flags = 0;

    if (usage & USAGE_HOST_READ)
        flags |= vk::VK_ACCESS_HOST_READ_BIT;

    if (usage & USAGE_HOST_WRITE)
        flags |= vk::VK_ACCESS_HOST_WRITE_BIT;

    if (usage & USAGE_TRANSFER_SRC)
        flags |= vk::VK_ACCESS_TRANSFER_READ_BIT;

    if (usage & USAGE_TRANSFER_DST)
        flags |= vk::VK_ACCESS_TRANSFER_WRITE_BIT;

    if (usage & USAGE_INDEX_BUFFER)
        flags |= vk::VK_ACCESS_INDEX_READ_BIT;

    if (usage & USAGE_VERTEX_BUFFER)
        flags |= vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

    if (usage & USAGE_UNIFORM_BUFFER)
        flags |= vk::VK_ACCESS_UNIFORM_READ_BIT;

    if (usage & USAGE_SAMPLED_IMAGE)
        flags |= vk::VK_ACCESS_SHADER_READ_BIT;

    if (usage & (USAGE_STORAGE_BUFFER | USAGE_UNIFORM_TEXEL_BUFFER | USAGE_STORAGE_TEXEL_BUFFER | USAGE_STORAGE_IMAGE))
        flags |= vk::VK_ACCESS_SHADER_READ_BIT | vk::VK_ACCESS_SHADER_WRITE_BIT;

    if (usage & USAGE_INDIRECT_BUFFER)
        flags |= vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    if (usage & USAGE_COLOR_ATTACHMENT)
        flags |= vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (usage & USAGE_INPUT_ATTACHMENT)
        flags |= vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    if (usage & USAGE_DEPTH_STENCIL_ATTACHMENT)
        flags |= vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    return flags;
}
vk::Move<vk::VkCommandBuffer> createBeginCommandBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                       vk::VkCommandPool pool, vk::VkCommandBufferLevel level)
{
    const vk::VkCommandBufferInheritanceInfo inheritInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        VK_NULL_HANDLE,                                        // VkRenderPass renderPass;
        0,                                                     // uint32_t subpass;
        VK_NULL_HANDLE,                                        // VkFramebuffer framebuffer;
        VK_FALSE,                                              // VkBool32 occlusionQueryEnable;
        0u,                                                    // VkQueryControlFlags queryFlags;
        0u                                                     // VkQueryPipelineStatisticFlags pipelineStatistics;
    };
    const vk::VkCommandBufferBeginInfo beginInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        0u,                                              // VkCommandBufferUsageFlags flags;
        (level == vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY ?
             &inheritInfo :
             nullptr), // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };

    vk::Move<vk::VkCommandBuffer> commandBuffer(allocateCommandBuffer(vkd, device, pool, level));

    VK_CHECK(vkd.beginCommandBuffer(*commandBuffer, &beginInfo));

    return commandBuffer;
}

vk::Move<vk::VkBuffer> createBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceSize size,
                                    vk::VkBufferUsageFlags usage, vk::VkSharingMode sharingMode,
                                    const vector<uint32_t> &queueFamilies)
{
    const vk::VkBufferCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0,                                        // VkBufferCreateFlags flags;
        size,                                     // VkDeviceSize size;
        usage,                                    // VkBufferUsageFlags usage;
        sharingMode,                              // VkSharingMode sharingMode;
        (uint32_t)queueFamilies.size(),           // uint32_t queueFamilyIndexCount;
        &queueFamilies[0]                         // const uint32_t* pQueueFamilyIndices;
    };

    return vk::createBuffer(vkd, device, &createInfo);
}

vk::Move<vk::VkDeviceMemory> allocMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceSize size,
                                         uint32_t memoryTypeIndex)
{
    const vk::VkMemoryAllocateInfo alloc = {
        vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        size,                                       // VkDeviceSize allocationSize;
        memoryTypeIndex                             // uint32_t memoryTypeIndex;
    };

    return vk::allocateMemory(vkd, device, &alloc);
}

vk::Move<vk::VkDeviceMemory> bindBufferMemory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd,
                                              vk::VkPhysicalDevice physicalDevice, vk::VkDevice device,
                                              vk::VkBuffer buffer, vk::VkMemoryPropertyFlags properties)
{
    const vk::VkMemoryRequirements memoryRequirements = vk::getBufferMemoryRequirements(vkd, device, buffer);
    const vk::VkPhysicalDeviceMemoryProperties memoryProperties =
        vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    uint32_t memoryTypeIndex;

    for (memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
    {
        if ((memoryRequirements.memoryTypeBits & (0x1u << memoryTypeIndex)) &&
            (memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & properties) == properties)
        {
            try
            {
                const vk::VkMemoryAllocateInfo allocationInfo = {
                    vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    memoryRequirements.size,                    // VkDeviceSize allocationSize;
                    memoryTypeIndex                             // uint32_t memoryTypeIndex;
                };
                vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &allocationInfo));

                VK_CHECK(vkd.bindBufferMemory(device, buffer, *memory, 0));

                return memory;
            }
            catch (const vk::Error &error)
            {
                if (error.getError() == vk::VK_ERROR_OUT_OF_DEVICE_MEMORY ||
                    error.getError() == vk::VK_ERROR_OUT_OF_HOST_MEMORY)
                {
                    // Try next memory type/heap if out of memory
                }
                else
                {
                    // Throw all other errors forward
                    throw;
                }
            }
        }
    }

    TCU_FAIL("Failed to allocate memory for buffer");
}

vk::Move<vk::VkDeviceMemory> bindImageMemory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd,
                                             vk::VkPhysicalDevice physicalDevice, vk::VkDevice device,
                                             vk::VkImage image, vk::VkMemoryPropertyFlags properties)
{
    const vk::VkMemoryRequirements memoryRequirements = vk::getImageMemoryRequirements(vkd, device, image);
    const vk::VkPhysicalDeviceMemoryProperties memoryProperties =
        vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    uint32_t memoryTypeIndex;

    for (memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
    {
        if ((memoryRequirements.memoryTypeBits & (0x1u << memoryTypeIndex)) &&
            (memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & properties) == properties)
        {
            try
            {
                const vk::VkMemoryAllocateInfo allocationInfo = {
                    vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    memoryRequirements.size,                    // VkDeviceSize allocationSize;
                    memoryTypeIndex                             // uint32_t memoryTypeIndex;
                };
                vk::Move<vk::VkDeviceMemory> memory(vk::allocateMemory(vkd, device, &allocationInfo));

                VK_CHECK(vkd.bindImageMemory(device, image, *memory, 0));

                return memory;
            }
            catch (const vk::Error &error)
            {
                if (error.getError() == vk::VK_ERROR_OUT_OF_DEVICE_MEMORY ||
                    error.getError() == vk::VK_ERROR_OUT_OF_HOST_MEMORY)
                {
                    // Try next memory type/heap if out of memory
                }
                else
                {
                    // Throw all other errors forward
                    throw;
                }
            }
        }
    }

    TCU_FAIL("Failed to allocate memory for image");
}

void *mapMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory, vk::VkDeviceSize size)
{
    void *ptr;

    VK_CHECK(vkd.mapMemory(device, memory, 0, size, 0, &ptr));

    return ptr;
}
ReferenceMemory::ReferenceMemory(size_t size) : m_data(size, 0), m_defined(size / 64 + (size % 64 == 0 ? 0 : 1), 0ull)
{
}

void ReferenceMemory::set(size_t pos, uint8_t val)
{
    DE_ASSERT(pos < m_data.size());

    m_data[pos] = val;
    m_defined[pos / 64] |= 0x1ull << (pos % 64);
}

void ReferenceMemory::setData(size_t offset, size_t size, const void *data_)
{
    const uint8_t *data = (const uint8_t *)data_;

    DE_ASSERT(offset < m_data.size());
    DE_ASSERT(offset + size <= m_data.size());

    // \todo [2016-03-09 mika] Optimize
    for (size_t pos = 0; pos < size; pos++)
    {
        m_data[offset + pos] = data[pos];
        m_defined[(offset + pos) / 64] |= 0x1ull << ((offset + pos) % 64);
    }
}

void ReferenceMemory::setUndefined(size_t offset, size_t size)
{
    // \todo [2016-03-09 mika] Optimize
    for (size_t pos = 0; pos < size; pos++)
        m_defined[(offset + pos) / 64] |= 0x1ull << ((offset + pos) % 64);
}

uint8_t ReferenceMemory::get(uint64_t pos) const
{
    DE_ASSERT(pos < m_data.size());
    DE_ASSERT(isDefined(pos));
    return m_data[(size_t)pos];
}

bool ReferenceMemory::isDefined(uint64_t pos) const
{
    DE_ASSERT(pos < m_data.size());

    return (m_defined[(size_t)pos / 64] & (0x1ull << (pos % 64))) != 0;
}
vk::VkMemoryType getMemoryTypeInfo(const vk::InstanceInterface &vki, vk::VkPhysicalDevice device,
                                   uint32_t memoryTypeIndex)
{
    const vk::VkPhysicalDeviceMemoryProperties memoryProperties = vk::getPhysicalDeviceMemoryProperties(vki, device);

    DE_ASSERT(memoryTypeIndex < memoryProperties.memoryTypeCount);

    return memoryProperties.memoryTypes[memoryTypeIndex];
}

vk::VkDeviceSize findMaxBufferSize(const vk::DeviceInterface &vkd, vk::VkDevice device,

                                   vk::VkBufferUsageFlags usage, vk::VkSharingMode sharingMode,
                                   const vector<uint32_t> &queueFamilies,

                                   vk::VkDeviceSize memorySize, uint32_t memoryTypeIndex)
{
    vk::VkDeviceSize lastSuccess = 0;
    vk::VkDeviceSize currentSize = memorySize / 2;

    {
        const vk::Unique<vk::VkBuffer> buffer(createBuffer(vkd, device, memorySize, usage, sharingMode, queueFamilies));
        const vk::VkMemoryRequirements requirements(vk::getBufferMemoryRequirements(vkd, device, *buffer));

        if (requirements.size == memorySize && requirements.memoryTypeBits & (0x1u << memoryTypeIndex))
            return memorySize;
    }

    for (vk::VkDeviceSize stepSize = memorySize / 4; currentSize > 0; stepSize /= 2)
    {
        const vk::Unique<vk::VkBuffer> buffer(
            createBuffer(vkd, device, currentSize, usage, sharingMode, queueFamilies));
        const vk::VkMemoryRequirements requirements(vk::getBufferMemoryRequirements(vkd, device, *buffer));

        if (requirements.size <= memorySize && requirements.memoryTypeBits & (0x1u << memoryTypeIndex))
        {
            lastSuccess = currentSize;
            currentSize += stepSize;
        }
        else
            currentSize -= stepSize;

        if (stepSize == 0)
            break;
    }

    return lastSuccess;
}

// Round size down maximum W * H * 4, where W and H < 4096
vk::VkDeviceSize roundBufferSizeToWxHx4(vk::VkDeviceSize size)
{
    const vk::VkDeviceSize maxTextureSize = 4096;
    vk::VkDeviceSize maxTexelCount        = size / 4;
    vk::VkDeviceSize bestW                = de::max(maxTexelCount, maxTextureSize);
    vk::VkDeviceSize bestH                = maxTexelCount / bestW;

    // \todo [2016-03-09 mika] Could probably be faster?
    for (vk::VkDeviceSize w = 1; w * w < maxTexelCount && w < maxTextureSize && bestW * bestH * 4 < size; w++)
    {
        const vk::VkDeviceSize h = maxTexelCount / w;

        if (bestW * bestH < w * h)
        {
            bestW = w;
            bestH = h;
        }
    }

    return bestW * bestH * 4;
}

// Find RGBA8 image size that has exactly "size" of number of bytes.
// "size" must be W * H * 4 where W and H < 4096
IVec2 findImageSizeWxHx4(vk::VkDeviceSize size)
{
    const vk::VkDeviceSize maxTextureSize = 4096;
    vk::VkDeviceSize texelCount           = size / 4;

    DE_ASSERT((size % 4) == 0);

    // \todo [2016-03-09 mika] Could probably be faster?
    for (vk::VkDeviceSize w = 1; w < maxTextureSize && w < texelCount; w++)
    {
        const vk::VkDeviceSize h = texelCount / w;

        if ((texelCount % w) == 0 && h < maxTextureSize)
            return IVec2((int)w, (int)h);
    }

    DE_FATAL("Invalid size");
    return IVec2(-1, -1);
}

IVec2 findMaxRGBA8ImageSize(const vk::DeviceInterface &vkd, vk::VkDevice device,

                            vk::VkImageUsageFlags usage, vk::VkSharingMode sharingMode,
                            const vector<uint32_t> &queueFamilies,

                            vk::VkDeviceSize memorySize, uint32_t memoryTypeIndex)
{
    IVec2 lastSuccess(0);
    IVec2 currentSize;

    {
        const uint32_t texelCount = (uint32_t)(memorySize / 4);
        const uint32_t width      = (uint32_t)deFloatSqrt((float)texelCount);
        const uint32_t height     = texelCount / width;

        currentSize[0] = deMaxu32(width, height);
        currentSize[1] = deMinu32(width, height);
    }

    for (int32_t stepSize = currentSize[0] / 2; currentSize[0] > 0; stepSize /= 2)
    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0u,                                      // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)currentSize[0],
                (uint32_t)currentSize[1],
                1u,
            },                              // VkExtent3D extent;
            1u,                             // uint32_t mipLevels;
            1u,                             // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,      // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,    // VkImageTiling tiling;
            usage,                          // VkImageUsageFlags usage;
            sharingMode,                    // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };
        const vk::Unique<vk::VkImage> image(vk::createImage(vkd, device, &createInfo));
        const vk::VkMemoryRequirements requirements(vk::getImageMemoryRequirements(vkd, device, *image));

        if (requirements.size <= memorySize && requirements.memoryTypeBits & (0x1u << memoryTypeIndex))
        {
            lastSuccess = currentSize;
            currentSize[0] += stepSize;
            currentSize[1] += stepSize;
        }
        else
        {
            currentSize[0] -= stepSize;
            currentSize[1] -= stepSize;
        }

        if (stepSize == 0)
            break;
    }

    return lastSuccess;
}

Memory::Memory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd, vk::VkPhysicalDevice physicalDevice,
               vk::VkDevice device, vk::VkDeviceSize size, uint32_t memoryTypeIndex, vk::VkDeviceSize maxBufferSize,
               int32_t maxImageWidth, int32_t maxImageHeight)
    : m_size(size)
    , m_memoryTypeIndex(memoryTypeIndex)
    , m_memoryType(getMemoryTypeInfo(vki, physicalDevice, memoryTypeIndex))
    , m_memory(allocMemory(vkd, device, size, memoryTypeIndex))
    , m_maxBufferSize(maxBufferSize)
    , m_maxImageWidth(maxImageWidth)
    , m_maxImageHeight(maxImageHeight)
{
}

class Map : public Command
{
public:
    Map(void)
    {
    }
    ~Map(void)
    {
    }
    const char *getName(void) const
    {
        return "Map";
    }

    void logExecute(TestLog &log, size_t commandIndex) const
    {
        log << TestLog::Message << commandIndex << ":" << getName() << " Map memory" << TestLog::EndMessage;
    }

    void prepare(PrepareContext &context)
    {
        m_memory = context.getMemory().getMemory();
        m_size   = context.getMemory().getSize();
    }

    void execute(ExecuteContext &context)
    {
        const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
        const vk::VkDevice device      = context.getContext().getDevice();

        context.setMapping(mapMemory(vkd, device, m_memory, m_size));
    }

private:
    vk::VkDeviceMemory m_memory;
    vk::VkDeviceSize m_size;
};

class UnMap : public Command
{
public:
    UnMap(void)
    {
    }
    ~UnMap(void)
    {
    }
    const char *getName(void) const
    {
        return "UnMap";
    }

    void logExecute(TestLog &log, size_t commandIndex) const
    {
        log << TestLog::Message << commandIndex << ": Unmap memory" << TestLog::EndMessage;
    }

    void prepare(PrepareContext &context)
    {
        m_memory = context.getMemory().getMemory();
    }

    void execute(ExecuteContext &context)
    {
        const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
        const vk::VkDevice device      = context.getContext().getDevice();

        vkd.unmapMemory(device, m_memory);
        context.setMapping(nullptr);
    }

private:
    vk::VkDeviceMemory m_memory;
};

class Invalidate : public Command
{
public:
    Invalidate(void)
    {
    }
    ~Invalidate(void)
    {
    }
    const char *getName(void) const
    {
        return "Invalidate";
    }

    void logExecute(TestLog &log, size_t commandIndex) const
    {
        log << TestLog::Message << commandIndex << ": Invalidate mapped memory" << TestLog::EndMessage;
    }

    void prepare(PrepareContext &context)
    {
        m_memory = context.getMemory().getMemory();
        m_size   = context.getMemory().getSize();
    }

    void execute(ExecuteContext &context)
    {
        const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
        const vk::VkDevice device      = context.getContext().getDevice();

        vk::invalidateMappedMemoryRange(vkd, device, m_memory, 0, VK_WHOLE_SIZE);
    }

private:
    vk::VkDeviceMemory m_memory;
    vk::VkDeviceSize m_size;
};

class Flush : public Command
{
public:
    Flush(void)
    {
    }
    ~Flush(void)
    {
    }
    const char *getName(void) const
    {
        return "Flush";
    }

    void logExecute(TestLog &log, size_t commandIndex) const
    {
        log << TestLog::Message << commandIndex << ": Flush mapped memory" << TestLog::EndMessage;
    }

    void prepare(PrepareContext &context)
    {
        m_memory = context.getMemory().getMemory();
        m_size   = context.getMemory().getSize();
    }

    void execute(ExecuteContext &context)
    {
        const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
        const vk::VkDevice device      = context.getContext().getDevice();

        vk::flushMappedMemoryRange(vkd, device, m_memory, 0, VK_WHOLE_SIZE);
    }

private:
    vk::VkDeviceMemory m_memory;
    vk::VkDeviceSize m_size;
};

// Host memory reads and writes
class HostMemoryAccess : public Command
{
public:
    HostMemoryAccess(bool read, bool write, uint32_t seed);
    ~HostMemoryAccess(void)
    {
    }
    const char *getName(void) const
    {
        return "HostMemoryAccess";
    }

    void logExecute(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void execute(ExecuteContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const bool m_read;
    const bool m_write;
    const uint32_t m_seed;

    size_t m_size;
    vector<uint8_t> m_readData;
};

HostMemoryAccess::HostMemoryAccess(bool read, bool write, uint32_t seed)
    : m_read(read)
    , m_write(write)
    , m_seed(seed)
    , m_size(0)
{
}

void HostMemoryAccess::logExecute(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ": Host memory access:" << (m_read ? " read" : "")
        << (m_write ? " write" : "") << ", seed: " << m_seed << TestLog::EndMessage;
}

void HostMemoryAccess::prepare(PrepareContext &context)
{
    m_size = (size_t)context.getMemory().getSize();

    if (m_read)
        m_readData.resize(m_size, 0);
}

void HostMemoryAccess::execute(ExecuteContext &context)
{
    if (m_read && m_write)
    {
        de::Random rng(m_seed);
        uint8_t *const ptr = (uint8_t *)context.getMapping();
        if (m_size >= ONE_MEGABYTE)
        {
            deMemcpy(&m_readData[0], ptr, m_size);
            for (size_t pos = 0; pos < m_size; ++pos)
            {
                ptr[pos] = m_readData[pos] ^ rng.getUint8();
            }
        }
        else
        {
            for (size_t pos = 0; pos < m_size; ++pos)
            {
                const uint8_t mask  = rng.getUint8();
                const uint8_t value = ptr[pos];

                m_readData[pos] = value;
                ptr[pos]        = value ^ mask;
            }
        }
    }
    else if (m_read)
    {
        const uint8_t *const ptr = (uint8_t *)context.getMapping();
        if (m_size >= ONE_MEGABYTE)
        {
            deMemcpy(&m_readData[0], ptr, m_size);
        }
        else
        {
            for (size_t pos = 0; pos < m_size; ++pos)
            {
                m_readData[pos] = ptr[pos];
            }
        }
    }
    else if (m_write)
    {
        de::Random rng(m_seed);
        uint8_t *const ptr = (uint8_t *)context.getMapping();
        for (size_t pos = 0; pos < m_size; ++pos)
        {
            ptr[pos] = rng.getUint8();
        }
    }
    else
        DE_FATAL("Host memory access without read or write.");
}

void HostMemoryAccess::verify(VerifyContext &context, size_t commandIndex)
{
    tcu::ResultCollector &resultCollector = context.getResultCollector();
    ReferenceMemory &reference            = context.getReference();
    de::Random rng(m_seed);

    if (m_read && m_write)
    {
        for (size_t pos = 0; pos < m_size; pos++)
        {
            const uint8_t mask  = rng.getUint8();
            const uint8_t value = m_readData[pos];

            if (reference.isDefined(pos))
            {
                if (value != reference.get(pos))
                {
                    resultCollector.fail(
                        de::toString(commandIndex) + ":" + getName() +
                        " Result differs from reference, Expected: " + de::toString(tcu::toHex<8>(reference.get(pos))) +
                        ", Got: " + de::toString(tcu::toHex<8>(value)) + ", At offset: " + de::toString(pos));
                    break;
                }

                reference.set(pos, reference.get(pos) ^ mask);
            }
        }
    }
    else if (m_read)
    {
        for (size_t pos = 0; pos < m_size; pos++)
        {
            const uint8_t value = m_readData[pos];

            if (reference.isDefined(pos))
            {
                if (value != reference.get(pos))
                {
                    resultCollector.fail(
                        de::toString(commandIndex) + ":" + getName() +
                        " Result differs from reference, Expected: " + de::toString(tcu::toHex<8>(reference.get(pos))) +
                        ", Got: " + de::toString(tcu::toHex<8>(value)) + ", At offset: " + de::toString(pos));
                    break;
                }
            }
        }
    }
    else if (m_write)
    {
        for (size_t pos = 0; pos < m_size; pos++)
        {
            const uint8_t value = rng.getUint8();

            reference.set(pos, value);
        }
    }
    else
        DE_FATAL("Host memory access without read or write.");
}

class CreateBuffer : public Command
{
public:
    CreateBuffer(vk::VkBufferUsageFlags usage, vk::VkSharingMode sharing);
    ~CreateBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "CreateBuffer";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);

private:
    const vk::VkBufferUsageFlags m_usage;
    const vk::VkSharingMode m_sharing;
};

CreateBuffer::CreateBuffer(vk::VkBufferUsageFlags usage, vk::VkSharingMode sharing) : m_usage(usage), m_sharing(sharing)
{
}

void CreateBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create buffer, Sharing mode: " << m_sharing
        << ", Usage: " << vk::getBufferUsageFlagsStr(m_usage) << TestLog::EndMessage;
}

void CreateBuffer::prepare(PrepareContext &context)
{
    const vk::DeviceInterface &vkd        = context.getContext().getDeviceInterface();
    const vk::VkDevice device             = context.getContext().getDevice();
    const vk::VkDeviceSize bufferSize     = context.getMemory().getMaxBufferSize();
    const vector<uint32_t> &queueFamilies = context.getContext().getQueueFamilies();

    context.setBuffer(createBuffer(vkd, device, bufferSize, m_usage, m_sharing, queueFamilies), bufferSize);
}

class DestroyBuffer : public Command
{
public:
    DestroyBuffer(void);
    ~DestroyBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "DestroyBuffer";
    }

    void logExecute(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void execute(ExecuteContext &context);

private:
    vk::Move<vk::VkBuffer> m_buffer;
};

DestroyBuffer::DestroyBuffer(void)
{
}

void DestroyBuffer::prepare(PrepareContext &context)
{
    m_buffer = vk::Move<vk::VkBuffer>(vk::check(context.getBuffer()),
                                      vk::Deleter<vk::VkBuffer>(context.getContext().getDeviceInterface(),
                                                                context.getContext().getDevice(), nullptr));
    context.releaseBuffer();
}

void DestroyBuffer::logExecute(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Destroy buffer" << TestLog::EndMessage;
}

void DestroyBuffer::execute(ExecuteContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();

    vkd.destroyBuffer(device, m_buffer.disown(), nullptr);
}

class BindBufferMemory : public Command
{
public:
    BindBufferMemory(void)
    {
    }
    ~BindBufferMemory(void)
    {
    }
    const char *getName(void) const
    {
        return "BindBufferMemory";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
};

void BindBufferMemory::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Bind memory to buffer" << TestLog::EndMessage;
}

void BindBufferMemory::prepare(PrepareContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();

    VK_CHECK(vkd.bindBufferMemory(device, context.getBuffer(), context.getMemory().getMemory(), 0));
}

class CreateImage : public Command
{
public:
    CreateImage(vk::VkImageUsageFlags usage, vk::VkSharingMode sharing);
    ~CreateImage(void)
    {
    }
    const char *getName(void) const
    {
        return "CreateImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const vk::VkImageUsageFlags m_usage;
    const vk::VkSharingMode m_sharing;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
};

CreateImage::CreateImage(vk::VkImageUsageFlags usage, vk::VkSharingMode sharing)
    : m_usage(usage)
    , m_sharing(sharing)
    , m_imageWidth(0)
    , m_imageHeight(0)
{
}

void CreateImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Create image, sharing: " << m_sharing
        << ", usage: " << vk::getImageUsageFlagsStr(m_usage) << TestLog::EndMessage;
}

void CreateImage::prepare(PrepareContext &context)
{
    const vk::DeviceInterface &vkd        = context.getContext().getDeviceInterface();
    const vk::VkDevice device             = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies = context.getContext().getQueueFamilies();

    m_imageWidth  = context.getMemory().getMaxImageWidth();
    m_imageHeight = context.getMemory().getMaxImageHeight();

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0u,                                      // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_imageWidth,
                (uint32_t)m_imageHeight,
                1u,
            },                              // VkExtent3D extent;
            1u,                             // uint32_t mipLevels;
            1u,                             // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,      // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,    // VkImageTiling tiling;
            m_usage,                        // VkImageUsageFlags usage;
            m_sharing,                      // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };
        vk::Move<vk::VkImage> image(createImage(vkd, device, &createInfo));
        const vk::VkMemoryRequirements requirements = vk::getImageMemoryRequirements(vkd, device, *image);

        context.setImage(image, vk::VK_IMAGE_LAYOUT_UNDEFINED, requirements.size, m_imageWidth, m_imageHeight);
    }
}

void CreateImage::verify(VerifyContext &context, size_t)
{
    context.getReferenceImage() =
        TextureLevel(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), m_imageWidth, m_imageHeight);
}

class DestroyImage : public Command
{
public:
    DestroyImage(void);
    ~DestroyImage(void)
    {
    }
    const char *getName(void) const
    {
        return "DestroyImage";
    }

    void logExecute(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void execute(ExecuteContext &context);

private:
    vk::Move<vk::VkImage> m_image;
};

DestroyImage::DestroyImage(void)
{
}

void DestroyImage::prepare(PrepareContext &context)
{
    m_image = vk::Move<vk::VkImage>(
        vk::check(context.getImage()),
        vk::Deleter<vk::VkImage>(context.getContext().getDeviceInterface(), context.getContext().getDevice(), nullptr));
    context.releaseImage();
}

void DestroyImage::logExecute(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Destroy image" << TestLog::EndMessage;
}

void DestroyImage::execute(ExecuteContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();

    vkd.destroyImage(device, m_image.disown(), nullptr);
}

class BindImageMemory : public Command
{
public:
    BindImageMemory(void)
    {
    }
    ~BindImageMemory(void)
    {
    }
    const char *getName(void) const
    {
        return "BindImageMemory";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
};

void BindImageMemory::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Bind memory to image" << TestLog::EndMessage;
}

void BindImageMemory::prepare(PrepareContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();

    VK_CHECK(vkd.bindImageMemory(device, context.getImage(), context.getMemory().getMemory(), 0));
}

class QueueWaitIdle : public Command
{
public:
    QueueWaitIdle(void)
    {
    }
    ~QueueWaitIdle(void)
    {
    }
    const char *getName(void) const
    {
        return "QueuetWaitIdle";
    }

    void logExecute(TestLog &log, size_t commandIndex) const;
    void execute(ExecuteContext &context);
};

void QueueWaitIdle::logExecute(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Queue wait idle" << TestLog::EndMessage;
}

void QueueWaitIdle::execute(ExecuteContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkQueue queue        = context.getContext().getQueue();

    VK_CHECK(vkd.queueWaitIdle(queue));
}

class DeviceWaitIdle : public Command
{
public:
    DeviceWaitIdle(void)
    {
    }
    ~DeviceWaitIdle(void)
    {
    }
    const char *getName(void) const
    {
        return "DeviceWaitIdle";
    }

    void logExecute(TestLog &log, size_t commandIndex) const;
    void execute(ExecuteContext &context);
};

void DeviceWaitIdle::logExecute(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Device wait idle" << TestLog::EndMessage;
}

void DeviceWaitIdle::execute(ExecuteContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkDevice device      = context.getContext().getDevice();

    VK_CHECK(vkd.deviceWaitIdle(device));
}

class SubmitCommandBuffer : public Command
{
public:
    SubmitCommandBuffer(const vector<CmdCommand *> &commands);
    ~SubmitCommandBuffer(void);

    const char *getName(void) const
    {
        return "SubmitCommandBuffer";
    }
    void logExecute(TestLog &log, size_t commandIndex) const;
    bool logExecuteFailureTrace(TestLog &log, size_t commandIndex, const string &failureMessage) const;
    void logPrepare(TestLog &log, size_t commandIndex) const;

    // Allocate command buffer and submit commands to command buffer
    void prepare(PrepareContext &context);
    void execute(ExecuteContext &context);

    // Verify that results are correct.
    void verify(VerifyContext &context, size_t commandIndex);

private:
    vector<CmdCommand *> m_commands;
    vk::Move<vk::VkCommandBuffer> m_commandBuffer;
    // Index of the sub-command that first introduced a verification failure, or -1
    // if none. Recorded during verify() and consumed by logExecuteFailureTrace().
    int m_firstFailingCmdNdx;
};

SubmitCommandBuffer::SubmitCommandBuffer(const vector<CmdCommand *> &commands)
    : m_commands(commands)
    , m_firstFailingCmdNdx(-1)
{
}

SubmitCommandBuffer::~SubmitCommandBuffer(void)
{
    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        delete m_commands[cmdNdx];
}

void SubmitCommandBuffer::prepare(PrepareContext &context)
{
    const vk::DeviceInterface &vkd      = context.getContext().getDeviceInterface();
    const vk::VkDevice device           = context.getContext().getDevice();
    const vk::VkCommandPool commandPool = context.getContext().getCommandPool();

    m_commandBuffer = createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        CmdCommand &command = *m_commands[cmdNdx];

        command.prepare(context);
    }

    {
        SubmitContext submitContext(context, *m_commandBuffer);

        for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        {
            CmdCommand &command = *m_commands[cmdNdx];

            command.submit(submitContext);
        }

        endCommandBuffer(vkd, *m_commandBuffer);
    }
}

void SubmitCommandBuffer::execute(ExecuteContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer cmd  = *m_commandBuffer;
    const vk::VkQueue queue        = context.getContext().getQueue();
    const vk::VkSubmitInfo submit  = {
        vk::VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                           // const void* pNext;
        0,                                 // uint32_t waitSemaphoreCount;
        nullptr,                           // const VkSemaphore* pWaitSemaphores;
        nullptr,                           // const VkPipelineStageFlags* pWaitDstStageMask;
        1,                                 // uint32_t commandBufferCount;
        &cmd,                              // const VkCommandBuffer* pCommandBuffers;
        0,                                 // uint32_t signalSemaphoreCount;
        nullptr                            // const VkSemaphore* pSignalSemaphores;
    };

    vkd.queueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
}

void SubmitCommandBuffer::verify(VerifyContext &context, size_t commandIndex)
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(context.getLog(), sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        const bool failedBefore = context.getResultCollector().getResult() != QP_TEST_RESULT_PASS;

        m_commands[cmdNdx]->verify(context, cmdNdx);

        // Remember the first sub-command that flipped the result so the failure
        // trace can mark the exact position amongst the barriers.
        if (m_firstFailingCmdNdx == -1 && !failedBefore &&
            context.getResultCollector().getResult() != QP_TEST_RESULT_PASS)
            m_firstFailingCmdNdx = (int)cmdNdx;
    }
}

void SubmitCommandBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        m_commands[cmdNdx]->logPrepare(log, cmdNdx);
}

void SubmitCommandBuffer::logExecute(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        m_commands[cmdNdx]->logSubmit(log, cmdNdx);
}

bool SubmitCommandBuffer::logExecuteFailureTrace(TestLog &log, size_t commandIndex, const string &failureMessage) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);
    bool marked = false;

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        const bool childMarked = m_commands[cmdNdx]->logSubmitFailureTrace(log, cmdNdx, failureMessage);

        marked = marked || childMarked;

        // Mark the failing sub-command in submission order. If the child already
        // marked a deeper failure (a secondary command buffer), don't mark again.
        if (!childMarked && (int)cmdNdx == m_firstFailingCmdNdx)
        {
            log << TestLog::Message << "Failure origin: " << failureMessage << TestLog::EndMessage;
            marked = true;
        }
    }

    return marked;
}

class PipelineBarrier : public CmdCommand
{
public:
    enum Type
    {
        TYPE_GLOBAL = 0,
        TYPE_BUFFER,
        TYPE_IMAGE,
        TYPE_LAST
    };
    PipelineBarrier(const vk::VkPipelineStageFlags srcStages, const vk::VkAccessFlags srcAccesses,
                    const vk::VkPipelineStageFlags dstStages, const vk::VkAccessFlags dstAccesses, Type type,
                    const tcu::Maybe<vk::VkImageLayout> imageLayout);
    ~PipelineBarrier(void)
    {
    }
    const char *getName(void) const
    {
        return "PipelineBarrier";
    }

    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);

private:
    const vk::VkPipelineStageFlags m_srcStages;
    const vk::VkAccessFlags m_srcAccesses;
    const vk::VkPipelineStageFlags m_dstStages;
    const vk::VkAccessFlags m_dstAccesses;
    const Type m_type;
    const tcu::Maybe<vk::VkImageLayout> m_imageLayout;
};

PipelineBarrier::PipelineBarrier(const vk::VkPipelineStageFlags srcStages, const vk::VkAccessFlags srcAccesses,
                                 const vk::VkPipelineStageFlags dstStages, const vk::VkAccessFlags dstAccesses,
                                 Type type, const tcu::Maybe<vk::VkImageLayout> imageLayout)
    : m_srcStages(srcStages)
    , m_srcAccesses(srcAccesses)
    , m_dstStages(dstStages)
    , m_dstAccesses(dstAccesses)
    , m_type(type)
    , m_imageLayout(imageLayout)
{
}

void PipelineBarrier::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " "
        << (m_type == TYPE_GLOBAL ? "Global pipeline barrier" :
            m_type == TYPE_BUFFER ? "Buffer pipeline barrier" :
                                    "Image pipeline barrier")
        << ", srcStages: " << vk::getPipelineStageFlagsStr(m_srcStages)
        << ", srcAccesses: " << vk::getAccessFlagsStr(m_srcAccesses)
        << ", dstStages: " << vk::getPipelineStageFlagsStr(m_dstStages)
        << ", dstAccesses: " << vk::getAccessFlagsStr(m_dstAccesses) << TestLog::EndMessage;
}

void PipelineBarrier::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer cmd  = context.getCommandBuffer();

    switch (m_type)
    {
    case TYPE_GLOBAL:
    {
        const vk::VkMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            m_srcAccesses,                        // VkAccessFlags srcAccessMask;
            m_dstAccesses                         // VkAccessFlags dstAccessMask;
        };

        vkd.cmdPipelineBarrier(cmd, m_srcStages, m_dstStages, (vk::VkDependencyFlags)0, 1, &barrier, 0, nullptr, 0,
                               nullptr);
        break;
    }

    case TYPE_BUFFER:
    {
        const vk::VkBufferMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            m_srcAccesses,                               // VkAccessFlags srcAccessMask;
            m_dstAccesses,                               // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            context.getBuffer(),                         // VkBuffer buffer;
            0,                                           // VkDeviceSize offset;
            VK_WHOLE_SIZE                                // VkDeviceSize size;
        };

        vkd.cmdPipelineBarrier(cmd, m_srcStages, m_dstStages, (vk::VkDependencyFlags)0, 0, nullptr, 1, &barrier, 0,
                               nullptr);
        break;
    }

    case TYPE_IMAGE:
    {
        const vk::VkImageMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            m_srcAccesses,                              // VkAccessFlags srcAccessMask;
            m_dstAccesses,                              // VkAccessFlags dstAccessMask;
            *m_imageLayout,                             // VkImageLayout oldLayout;
            *m_imageLayout,                             // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            context.getImage(),                         // VkImage image;
            {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1} // VkImageSubresourceRange subresourceRange;
        };

        vkd.cmdPipelineBarrier(cmd, m_srcStages, m_dstStages, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &barrier);
        break;
    }

    default:
        DE_FATAL("Unknown pipeline barrier type");
    }
}

class ImageTransition : public CmdCommand
{
public:
    ImageTransition(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,

                    vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses,

                    vk::VkImageLayout srcLayout, vk::VkImageLayout dstLayout);

    ~ImageTransition(void)
    {
    }
    const char *getName(void) const
    {
        return "ImageTransition";
    }

    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t);

private:
    const vk::VkPipelineStageFlags m_srcStages;
    const vk::VkAccessFlags m_srcAccesses;
    const vk::VkPipelineStageFlags m_dstStages;
    const vk::VkAccessFlags m_dstAccesses;
    const vk::VkImageLayout m_srcLayout;
    const vk::VkImageLayout m_dstLayout;

    vk::VkDeviceSize m_imageMemorySize;
};

ImageTransition::ImageTransition(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,

                                 vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses,

                                 vk::VkImageLayout srcLayout, vk::VkImageLayout dstLayout)
    : m_srcStages(srcStages)
    , m_srcAccesses(srcAccesses)
    , m_dstStages(dstStages)
    , m_dstAccesses(dstAccesses)
    , m_srcLayout(srcLayout)
    , m_dstLayout(dstLayout)
    , m_imageMemorySize(0)
{
}

void ImageTransition::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Image transition pipeline barrier"
        << ", srcStages: " << vk::getPipelineStageFlagsStr(m_srcStages)
        << ", srcAccesses: " << vk::getAccessFlagsStr(m_srcAccesses)
        << ", dstStages: " << vk::getPipelineStageFlagsStr(m_dstStages)
        << ", dstAccesses: " << vk::getAccessFlagsStr(m_dstAccesses) << ", srcLayout: " << m_srcLayout
        << ", dstLayout: " << m_dstLayout << TestLog::EndMessage;
}

void ImageTransition::prepare(PrepareContext &context)
{
    DE_ASSERT(context.getImageLayout() == vk::VK_IMAGE_LAYOUT_UNDEFINED ||
              m_srcLayout == vk::VK_IMAGE_LAYOUT_UNDEFINED || context.getImageLayout() == m_srcLayout);

    context.setImageLayout(m_dstLayout);
    m_imageMemorySize = context.getImageMemorySize();
}

void ImageTransition::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd         = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer cmd          = context.getCommandBuffer();
    const vk::VkImageMemoryBarrier barrier = {
        vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,     // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        m_srcAccesses,                                  // VkAccessFlags srcAccessMask;
        m_dstAccesses,                                  // VkAccessFlags dstAccessMask;
        m_srcLayout,                                    // VkImageLayout oldLayout;
        m_dstLayout,                                    // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                        // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                        // uint32_t dstQueueFamilyIndex;
        context.getImage(),                             // VkImage image;
        {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
    };

    vkd.cmdPipelineBarrier(cmd, m_srcStages, m_dstStages, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                           &barrier);
}

void ImageTransition::verify(VerifyContext &context, size_t)
{
    context.getReference().setUndefined(0, (size_t)m_imageMemorySize);
}

class FillBuffer : public CmdCommand
{
public:
    FillBuffer(uint32_t value) : m_value(value), m_bufferSize(0)
    {
    }
    ~FillBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "FillBuffer";
    }

    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_value;
    vk::VkDeviceSize m_bufferSize;
};

void FillBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Fill value: " << m_value << TestLog::EndMessage;
}

void FillBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd  = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer cmd   = context.getCommandBuffer();
    const vk::VkBuffer buffer       = context.getBuffer();
    const vk::VkDeviceSize sizeMask = ~(0x3ull); // \note Round down to multiple of 4

    m_bufferSize = sizeMask & context.getBufferSize();
    vkd.cmdFillBuffer(cmd, buffer, 0, m_bufferSize, m_value);
}

void FillBuffer::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference = context.getReference();

    for (size_t ndx = 0; ndx < m_bufferSize; ndx++)
    {
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
        reference.set(ndx, (uint8_t)(0xffu & (m_value >> (8 * (ndx % 4)))));
#else
        reference.set(ndx, (uint8_t)(0xffu & (m_value >> (8 * (3 - (ndx % 4))))));
#endif
    }
}

class UpdateBuffer : public CmdCommand
{
public:
    UpdateBuffer(uint32_t seed) : m_seed(seed), m_bufferSize(0)
    {
    }
    ~UpdateBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "UpdateBuffer";
    }

    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_seed;
    vk::VkDeviceSize m_bufferSize;
};

void UpdateBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Update buffer, seed: " << m_seed
        << TestLog::EndMessage;
}

void UpdateBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer cmd  = context.getCommandBuffer();
    const vk::VkBuffer buffer      = context.getBuffer();
    const size_t blockSize         = 65536;
    std::vector<uint8_t> data(blockSize, 0);
    de::Random rng(m_seed);

    m_bufferSize = context.getBufferSize();

    for (size_t updated = 0; updated < m_bufferSize; updated += blockSize)
    {
        for (size_t ndx = 0; ndx < data.size(); ndx++)
            data[ndx] = rng.getUint8();

        if (m_bufferSize - updated > blockSize)
            vkd.cmdUpdateBuffer(cmd, buffer, updated, blockSize, (const uint32_t *)(&data[0]));
        else
            vkd.cmdUpdateBuffer(cmd, buffer, updated, m_bufferSize - updated, (const uint32_t *)(&data[0]));
    }
}

void UpdateBuffer::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference = context.getReference();
    const size_t blockSize     = 65536;
    vector<uint8_t> data(blockSize, 0);
    de::Random rng(m_seed);

    for (size_t updated = 0; updated < m_bufferSize; updated += blockSize)
    {
        for (size_t ndx = 0; ndx < data.size(); ndx++)
            data[ndx] = rng.getUint8();

        if (m_bufferSize - updated > blockSize)
            reference.setData(updated, blockSize, &data[0]);
        else
            reference.setData(updated, (size_t)(m_bufferSize - updated), &data[0]);
    }
}

string padLeft(const string &value, size_t width)
{
    return value.size() >= width ? value : string(width - value.size(), ' ') + value;
}

// Formats a byte-level hex table around a verification mismatch at pos, e.g.:
//     offset:      0    1    2    3    4    5
//     expected:  [53]  b0   88   31   ec   77
//     got:       [b7]  7d   44   fa   b7   cc
// so the wrong bytes can be matched against the write that produced them (e.g. a
// repeating 32-bit fill pattern vs. random update data landing where it shouldn't).
string formatByteWindow(const ReferenceMemory &reference, const uint8_t *data, size_t pos, size_t windowStart,
                        size_t windowEnd)
{
    size_t colWidth = de::toString(windowEnd - 1).size() + 1;
    if (colWidth < 5)
        colWidth = 5;

    string offsetRow;
    string expectedRow;
    string gotRow;

    for (size_t ndx = windowStart; ndx < windowEnd; ndx++)
    {
        const bool mark = (ndx == pos);
        const string expectedByte =
            reference.isDefined(ndx) ? de::toString(tcu::toHex<2>(reference.get(ndx))).substr(2) : "??";
        const string gotByte = de::toString(tcu::toHex<2>(data[ndx])).substr(2);

        offsetRow += padLeft(de::toString(ndx), colWidth);
        expectedRow += padLeft(mark ? "[" + expectedByte + "]" : expectedByte, colWidth);
        gotRow += padLeft(mark ? "[" + gotByte + "]" : gotByte, colWidth);
    }

    return "    offset:   " + offsetRow + "\n" + "    expected: " + expectedRow + "\n" + "    got:      " + gotRow;
}

class BufferCopyToBuffer : public CmdCommand
{
public:
    BufferCopyToBuffer(void)
    {
    }
    ~BufferCopyToBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "BufferCopyToBuffer";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    vk::VkDeviceSize m_bufferSize;
    vk::Move<vk::VkBuffer> m_dstBuffer;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void BufferCopyToBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate destination buffer for buffer to buffer copy." << TestLog::EndMessage;
}

void BufferCopyToBuffer::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_bufferSize = context.getBufferSize();

    m_dstBuffer = createBuffer(vkd, device, m_bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies);
    m_memory =
        bindBufferMemory(vki, vkd, physicalDevice, device, *m_dstBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

void BufferCopyToBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy buffer to another buffer"
        << TestLog::EndMessage;
}

void BufferCopyToBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkBufferCopy range            = {
        0,           // VkDeviceSize srcOffset;
        0,           // VkDeviceSize dstOffset;
        m_bufferSize // VkDeviceSize size;
    };

    vkd.cmdCopyBuffer(commandBuffer, context.getBuffer(), *m_dstBuffer, 1, &range);
}

void BufferCopyToBuffer::verify(VerifyContext &context, size_t commandIndex)
{
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    ReferenceMemory &reference(context.getReference());
    const vk::DeviceInterface &vkd      = context.getContext().getDeviceInterface();
    const vk::VkDevice device           = context.getContext().getDevice();
    const vk::VkQueue queue             = context.getContext().getQueue();
    const vk::VkCommandPool commandPool = context.getContext().getCommandPool();
    const vk::Unique<vk::VkCommandBuffer> commandBuffer(
        createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const vk::VkBufferMemoryBarrier barrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        *m_dstBuffer,                                // VkBuffer buffer;
        0,                                           // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                           (vk::VkDependencyFlags)0, 0, nullptr, 1, &barrier, 0, nullptr);

    endCommandBuffer(vkd, *commandBuffer);
    submitCommandsAndWait(vkd, device, queue, *commandBuffer);

    {
        void *const ptr = mapMemory(vkd, device, *m_memory, m_bufferSize);
        bool isOk       = true;

        vk::invalidateMappedMemoryRange(vkd, device, *m_memory, 0, VK_WHOLE_SIZE);

        {
            const uint8_t *const data = (const uint8_t *)ptr;

            for (size_t pos = 0; pos < (size_t)m_bufferSize; pos++)
            {
                if (reference.isDefined(pos))
                {
                    if (data[pos] != reference.get(pos))
                    {
                        const size_t windowStart = pos >= 4 ? pos - 4 : 0;
                        const size_t windowEnd   = pos + 12 < (size_t)m_bufferSize ? pos + 12 : (size_t)m_bufferSize;

                        resultCollector.fail(de::toString(commandIndex) + ":" + getName() +
                                             " buffer content differs from reference at offset " + de::toString(pos) +
                                             " (expected " + de::toString(tcu::toHex<8>(reference.get(pos))) +
                                             ", got " + de::toString(tcu::toHex<8>(data[pos])) + ")\n" +
                                             formatByteWindow(reference, data, pos, windowStart, windowEnd));
                        break;
                    }
                }
            }
        }

        vkd.unmapMemory(device, *m_memory);

        if (!isOk)
            context.getLog() << TestLog::Message << commandIndex << ": Buffer copy to buffer verification failed"
                             << TestLog::EndMessage;
    }
}

class BufferCopyFromBuffer : public CmdCommand
{
public:
    BufferCopyFromBuffer(uint32_t seed) : m_seed(seed), m_bufferSize(0)
    {
    }
    ~BufferCopyFromBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "BufferCopyFromBuffer";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_seed;
    vk::VkDeviceSize m_bufferSize;
    vk::Move<vk::VkBuffer> m_srcBuffer;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void BufferCopyFromBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate source buffer for buffer to buffer copy. Seed: " << m_seed << TestLog::EndMessage;
}

void BufferCopyFromBuffer::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_bufferSize = context.getBufferSize();
    m_srcBuffer  = createBuffer(vkd, device, m_bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies);
    m_memory =
        bindBufferMemory(vki, vkd, physicalDevice, device, *m_srcBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    {
        void *const ptr = mapMemory(vkd, device, *m_memory, m_bufferSize);
        de::Random rng(m_seed);

        {
            uint8_t *const data = (uint8_t *)ptr;

            for (size_t ndx = 0; ndx < (size_t)m_bufferSize; ndx++)
                data[ndx] = rng.getUint8();
        }

        vk::flushMappedMemoryRange(vkd, device, *m_memory, 0, VK_WHOLE_SIZE);
        vkd.unmapMemory(device, *m_memory);
    }
}

void BufferCopyFromBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy buffer data from another buffer"
        << TestLog::EndMessage;
}

void BufferCopyFromBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkBufferCopy range            = {
        0,           // VkDeviceSize srcOffset;
        0,           // VkDeviceSize dstOffset;
        m_bufferSize // VkDeviceSize size;
    };

    vkd.cmdCopyBuffer(commandBuffer, *m_srcBuffer, context.getBuffer(), 1, &range);
}

void BufferCopyFromBuffer::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference(context.getReference());
    de::Random rng(m_seed);

    for (size_t ndx = 0; ndx < (size_t)m_bufferSize; ndx++)
        reference.set(ndx, rng.getUint8());
}

class BufferCopyToImage : public CmdCommand
{
public:
    BufferCopyToImage(void)
    {
    }
    ~BufferCopyToImage(void)
    {
    }
    const char *getName(void) const
    {
        return "BufferCopyToImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::Move<vk::VkImage> m_dstImage;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void BufferCopyToImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate destination image for buffer to image copy." << TestLog::EndMessage;
}

void BufferCopyToImage::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();
    const IVec2 imageSize                     = findImageSizeWxHx4(context.getBufferSize());

    m_imageWidth  = imageSize[0];
    m_imageHeight = imageSize[1];

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0,                                       // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_imageWidth,
                (uint32_t)m_imageHeight,
                1u,
            },                                                                         // VkExtent3D extent;
            1,                                                                         // uint32_t mipLevels;
            1,                                                                         // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_dstImage = vk::createImage(vkd, device, &createInfo);
    }

    m_memory = bindImageMemory(vki, vkd, physicalDevice, device, *m_dstImage, 0);

    {
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_dstImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &barrier);

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);
    }
}

void BufferCopyToImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy buffer to image" << TestLog::EndMessage;
}

void BufferCopyToImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkBufferImageCopy region      = {
        0, // VkDeviceSize bufferOffset;
        0, // uint32_t bufferRowLength;
        0, // uint32_t bufferImageHeight;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
            0,                                                // uint32_t mipLevel;
            0,                                                // uint32_t baseArrayLayer;
            1                                                 // uint32_t layerCount;
        },                                                    // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                                            // VkOffset3D imageOffset;
        {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
    };

    vkd.cmdCopyBufferToImage(commandBuffer, context.getBuffer(), *m_dstImage, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             1, &region);
}

void BufferCopyToImage::verify(VerifyContext &context, size_t commandIndex)
{
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    ReferenceMemory &reference(context.getReference());
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vk::Unique<vk::VkCommandBuffer> commandBuffer(
        createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const vector<uint32_t> &queueFamilies = context.getContext().getQueueFamilies();
    const vk::Unique<vk::VkBuffer> dstBuffer(createBuffer(vkd, device, 4 * m_imageWidth * m_imageHeight,
                                                          vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                          vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
    const vk::Unique<vk::VkDeviceMemory> memory(
        bindBufferMemory(vki, vkd, physicalDevice, device, *dstBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    {
        const vk::VkImageMemoryBarrier imageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_dstImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkBufferMemoryBarrier bufferBarrier = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            *dstBuffer,                                  // VkBuffer buffer;
            0,                                           // VkDeviceSize offset;
            VK_WHOLE_SIZE                                // VkDeviceSize size;
        };

        const vk::VkBufferImageCopy region = {
            0, // VkDeviceSize bufferOffset;
            0, // uint32_t bufferRowLength;
            0, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
                0,                                                // uint32_t mipLevel;
                0,                                                // uint32_t baseArrayLayer;
                1                                                 // uint32_t layerCount;
            },                                                    // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                            // VkOffset3D imageOffset;
            {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
        };

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        vkd.cmdCopyImageToBuffer(*commandBuffer, *m_dstImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstBuffer, 1,
                                 &region);
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }

    endCommandBuffer(vkd, *commandBuffer);
    submitCommandsAndWait(vkd, device, queue, *commandBuffer);

    {
        void *const ptr = mapMemory(vkd, device, *memory, 4 * m_imageWidth * m_imageHeight);

        invalidateMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);

        {
            const uint8_t *const data = (const uint8_t *)ptr;

            for (size_t pos = 0; pos < (size_t)(4 * m_imageWidth * m_imageHeight); pos++)
            {
                if (reference.isDefined(pos))
                {
                    if (data[pos] != reference.get(pos))
                    {
                        resultCollector.fail(de::toString(commandIndex) + ":" + getName() +
                                             " Result differs from reference, Expected: " +
                                             de::toString(tcu::toHex<8>(reference.get(pos))) +
                                             ", Got: " + de::toString(tcu::toHex<8>(data[pos])) +
                                             ", At offset: " + de::toString(pos));
                        break;
                    }
                }
            }
        }

        vkd.unmapMemory(device, *memory);
    }
}

class BufferCopyFromImage : public CmdCommand
{
public:
    BufferCopyFromImage(uint32_t seed) : m_seed(seed)
    {
    }
    ~BufferCopyFromImage(void)
    {
    }
    const char *getName(void) const
    {
        return "BufferCopyFromImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_seed;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::Move<vk::VkImage> m_srcImage;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void BufferCopyFromImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Allocate source image for image to buffer copy."
        << TestLog::EndMessage;
}

void BufferCopyFromImage::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();
    const IVec2 imageSize                     = findImageSizeWxHx4(context.getBufferSize());

    m_imageWidth  = imageSize[0];
    m_imageHeight = imageSize[1];

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0,                                       // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_imageWidth,
                (uint32_t)m_imageHeight,
                1u,
            },                                                                         // VkExtent3D extent;
            1,                                                                         // uint32_t mipLevels;
            1,                                                                         // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_srcImage = vk::createImage(vkd, device, &createInfo);
    }

    m_memory = bindImageMemory(vki, vkd, physicalDevice, device, *m_srcImage, 0);

    {
        const vk::Unique<vk::VkBuffer> srcBuffer(createBuffer(vkd, device, 4 * m_imageWidth * m_imageHeight,
                                                              vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
        const vk::Unique<vk::VkDeviceMemory> memory(
            bindBufferMemory(vki, vkd, physicalDevice, device, *srcBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageMemoryBarrier preImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_srcImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkImageMemoryBarrier postImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            0,                                          // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_srcImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkBufferImageCopy region = {
            0, // VkDeviceSize bufferOffset;
            0, // uint32_t bufferRowLength;
            0, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
                0,                                                // uint32_t mipLevel;
                0,                                                // uint32_t baseArrayLayer;
                1                                                 // uint32_t layerCount;
            },                                                    // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                            // VkOffset3D imageOffset;
            {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
        };

        {
            void *const ptr = mapMemory(vkd, device, *memory, 4 * m_imageWidth * m_imageHeight);
            de::Random rng(m_seed);

            {
                uint8_t *const data = (uint8_t *)ptr;

                for (size_t ndx = 0; ndx < (size_t)(4 * m_imageWidth * m_imageHeight); ndx++)
                    data[ndx] = rng.getUint8();
            }

            vk::flushMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);
            vkd.unmapMemory(device, *memory);
        }

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &preImageBarrier);
        vkd.cmdCopyBufferToImage(*commandBuffer, *srcBuffer, *m_srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                 &region);
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);
    }
}

void BufferCopyFromImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy buffer data from image"
        << TestLog::EndMessage;
}

void BufferCopyFromImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkBufferImageCopy region      = {
        0, // VkDeviceSize bufferOffset;
        0, // uint32_t bufferRowLength;
        0, // uint32_t bufferImageHeight;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
            0,                                                // uint32_t mipLevel;
            0,                                                // uint32_t baseArrayLayer;
            1                                                 // uint32_t layerCount;
        },                                                    // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                                            // VkOffset3D imageOffset;
        {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
    };

    vkd.cmdCopyImageToBuffer(commandBuffer, *m_srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, context.getBuffer(),
                             1, &region);
}

void BufferCopyFromImage::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference(context.getReference());
    de::Random rng(m_seed);

    for (size_t ndx = 0; ndx < (size_t)(4 * m_imageWidth * m_imageHeight); ndx++)
        reference.set(ndx, rng.getUint8());
}

class ImageCopyToBuffer : public CmdCommand
{
public:
    ImageCopyToBuffer(vk::VkImageLayout imageLayout) : m_imageLayout(imageLayout)
    {
    }
    ~ImageCopyToBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "BufferCopyToImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    vk::VkImageLayout m_imageLayout;
    vk::VkDeviceSize m_bufferSize;
    vk::Move<vk::VkBuffer> m_dstBuffer;
    vk::Move<vk::VkDeviceMemory> m_memory;
    vk::VkDeviceSize m_imageMemorySize;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
};

void ImageCopyToBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate destination buffer for image to buffer copy." << TestLog::EndMessage;
}

void ImageCopyToBuffer::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_imageWidth      = context.getImageWidth();
    m_imageHeight     = context.getImageHeight();
    m_bufferSize      = 4 * m_imageWidth * m_imageHeight;
    m_imageMemorySize = context.getImageMemorySize();
    m_dstBuffer       = createBuffer(vkd, device, m_bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies);
    m_memory =
        bindBufferMemory(vki, vkd, physicalDevice, device, *m_dstBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

void ImageCopyToBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy image to buffer" << TestLog::EndMessage;
}

void ImageCopyToBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkBufferImageCopy region      = {
        0, // VkDeviceSize bufferOffset;
        0, // uint32_t bufferRowLength;
        0, // uint32_t bufferImageHeight;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
            0,                                                // uint32_t mipLevel;
            0,                                                // uint32_t baseArrayLayer;
            1                                                 // uint32_t layerCount;
        },                                                    // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                                            // VkOffset3D imageOffset;
        {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
    };

    vkd.cmdCopyImageToBuffer(commandBuffer, context.getImage(), m_imageLayout, *m_dstBuffer, 1, &region);
}

void ImageCopyToBuffer::verify(VerifyContext &context, size_t commandIndex)
{
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    ReferenceMemory &reference(context.getReference());
    const vk::DeviceInterface &vkd      = context.getContext().getDeviceInterface();
    const vk::VkDevice device           = context.getContext().getDevice();
    const vk::VkQueue queue             = context.getContext().getQueue();
    const vk::VkCommandPool commandPool = context.getContext().getCommandPool();
    const vk::Unique<vk::VkCommandBuffer> commandBuffer(
        createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const vk::VkBufferMemoryBarrier barrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        *m_dstBuffer,                                // VkBuffer buffer;
        0,                                           // VkDeviceSize offset;
        VK_WHOLE_SIZE                                // VkDeviceSize size;
    };

    vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                           (vk::VkDependencyFlags)0, 0, nullptr, 1, &barrier, 0, nullptr);

    endCommandBuffer(vkd, *commandBuffer);
    submitCommandsAndWait(vkd, device, queue, *commandBuffer);

    reference.setUndefined(0, (size_t)m_imageMemorySize);
    {
        void *const ptr = mapMemory(vkd, device, *m_memory, m_bufferSize);
        const ConstPixelBufferAccess referenceImage(context.getReferenceImage().getAccess());
        const ConstPixelBufferAccess resultImage(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8),
                                                 m_imageWidth, m_imageHeight, 1, ptr);

        vk::invalidateMappedMemoryRange(vkd, device, *m_memory, 0, VK_WHOLE_SIZE);

        if (!tcu::intThresholdCompare(context.getLog(), (de::toString(commandIndex) + ":" + getName()).c_str(),
                                      (de::toString(commandIndex) + ":" + getName()).c_str(), referenceImage,
                                      resultImage, UVec4(0), tcu::COMPARE_LOG_ON_ERROR))
            resultCollector.fail(de::toString(commandIndex) + ":" + getName() + " Image comparison failed");

        vkd.unmapMemory(device, *m_memory);
    }
}

class ImageCopyFromBuffer : public CmdCommand
{
public:
    ImageCopyFromBuffer(uint32_t seed, vk::VkImageLayout imageLayout) : m_seed(seed), m_imageLayout(imageLayout)
    {
    }
    ~ImageCopyFromBuffer(void)
    {
    }
    const char *getName(void) const
    {
        return "ImageCopyFromBuffer";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_seed;
    const vk::VkImageLayout m_imageLayout;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::VkDeviceSize m_imageMemorySize;
    vk::VkDeviceSize m_bufferSize;
    vk::Move<vk::VkBuffer> m_srcBuffer;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void ImageCopyFromBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate source buffer for buffer to image copy. Seed: " << m_seed << TestLog::EndMessage;
}

void ImageCopyFromBuffer::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_imageWidth      = context.getImageHeight();
    m_imageHeight     = context.getImageWidth();
    m_imageMemorySize = context.getImageMemorySize();
    m_bufferSize      = m_imageWidth * m_imageHeight * 4;
    m_srcBuffer       = createBuffer(vkd, device, m_bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies);
    m_memory =
        bindBufferMemory(vki, vkd, physicalDevice, device, *m_srcBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    {
        void *const ptr = mapMemory(vkd, device, *m_memory, m_bufferSize);
        de::Random rng(m_seed);

        {
            uint8_t *const data = (uint8_t *)ptr;

            for (size_t ndx = 0; ndx < (size_t)m_bufferSize; ndx++)
                data[ndx] = rng.getUint8();
        }

        vk::flushMappedMemoryRange(vkd, device, *m_memory, 0, VK_WHOLE_SIZE);
        vkd.unmapMemory(device, *m_memory);
    }
}

void ImageCopyFromBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy image data from buffer"
        << TestLog::EndMessage;
}

void ImageCopyFromBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkBufferImageCopy region      = {
        0, // VkDeviceSize bufferOffset;
        0, // uint32_t bufferRowLength;
        0, // uint32_t bufferImageHeight;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
            0,                                                // uint32_t mipLevel;
            0,                                                // uint32_t baseArrayLayer;
            1                                                 // uint32_t layerCount;
        },                                                    // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                                            // VkOffset3D imageOffset;
        {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
    };

    vkd.cmdCopyBufferToImage(commandBuffer, *m_srcBuffer, context.getImage(), m_imageLayout, 1, &region);
}

void ImageCopyFromBuffer::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference(context.getReference());
    de::Random rng(m_seed);

    reference.setUndefined(0, (size_t)m_imageMemorySize);

    {
        const PixelBufferAccess &refAccess(context.getReferenceImage().getAccess());

        for (int32_t y = 0; y < m_imageHeight; y++)
            for (int32_t x = 0; x < m_imageWidth; x++)
            {
                const uint8_t r8 = rng.getUint8();
                const uint8_t g8 = rng.getUint8();
                const uint8_t b8 = rng.getUint8();
                const uint8_t a8 = rng.getUint8();

                refAccess.setPixel(UVec4(r8, g8, b8, a8), x, y);
            }
    }
}

class ImageCopyFromImage : public CmdCommand
{
public:
    ImageCopyFromImage(uint32_t seed, vk::VkImageLayout imageLayout) : m_seed(seed), m_imageLayout(imageLayout)
    {
    }
    ~ImageCopyFromImage(void)
    {
    }
    const char *getName(void) const
    {
        return "ImageCopyFromImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_seed;
    const vk::VkImageLayout m_imageLayout;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::VkDeviceSize m_imageMemorySize;
    vk::Move<vk::VkImage> m_srcImage;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void ImageCopyFromImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Allocate source image for image to image copy."
        << TestLog::EndMessage;
}

void ImageCopyFromImage::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_imageWidth      = context.getImageWidth();
    m_imageHeight     = context.getImageHeight();
    m_imageMemorySize = context.getImageMemorySize();

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0,                                       // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_imageWidth,
                (uint32_t)m_imageHeight,
                1u,
            },                                                                         // VkExtent3D extent;
            1,                                                                         // uint32_t mipLevels;
            1,                                                                         // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_srcImage = vk::createImage(vkd, device, &createInfo);
    }

    m_memory = bindImageMemory(vki, vkd, physicalDevice, device, *m_srcImage, 0);

    {
        const vk::Unique<vk::VkBuffer> srcBuffer(createBuffer(vkd, device, 4 * m_imageWidth * m_imageHeight,
                                                              vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
        const vk::Unique<vk::VkDeviceMemory> memory(
            bindBufferMemory(vki, vkd, physicalDevice, device, *srcBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageMemoryBarrier preImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_srcImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkImageMemoryBarrier postImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            0,                                          // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_srcImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkBufferImageCopy region = {
            0, // VkDeviceSize bufferOffset;
            0, // uint32_t bufferRowLength;
            0, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
                0,                                                // uint32_t mipLevel;
                0,                                                // uint32_t baseArrayLayer;
                1                                                 // uint32_t layerCount;
            },                                                    // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                            // VkOffset3D imageOffset;
            {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
        };

        {
            void *const ptr = mapMemory(vkd, device, *memory, 4 * m_imageWidth * m_imageHeight);
            de::Random rng(m_seed);

            {
                uint8_t *const data = (uint8_t *)ptr;

                for (size_t ndx = 0; ndx < (size_t)(4 * m_imageWidth * m_imageHeight); ndx++)
                    data[ndx] = rng.getUint8();
            }

            vk::flushMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);
            vkd.unmapMemory(device, *memory);
        }

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &preImageBarrier);
        vkd.cmdCopyBufferToImage(*commandBuffer, *srcBuffer, *m_srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                 &region);
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);
    }
}

void ImageCopyFromImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy image data from another image"
        << TestLog::EndMessage;
}

void ImageCopyFromImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkImageCopy region            = {
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0,                             // uint32_t mipLevel;
            0,                             // uint32_t baseArrayLayer;
            1                              // uint32_t layerCount;
        },                                 // VkImageSubresourceLayers srcSubresource;
        {0, 0, 0},                         // VkOffset3D srcOffset;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
            0,                                                // uint32_t mipLevel;
            0,                                                // uint32_t baseArrayLayer;
            1                                                 // uint32_t layerCount;
        },                                                    // VkImageSubresourceLayers dstSubresource;
        {0, 0, 0},                                            // VkOffset3D dstOffset;
        {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D extent;
    };

    vkd.cmdCopyImage(commandBuffer, *m_srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, context.getImage(),
                     m_imageLayout, 1, &region);
}

void ImageCopyFromImage::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference(context.getReference());
    de::Random rng(m_seed);

    reference.setUndefined(0, (size_t)m_imageMemorySize);

    {
        const PixelBufferAccess &refAccess(context.getReferenceImage().getAccess());

        for (int32_t y = 0; y < m_imageHeight; y++)
            for (int32_t x = 0; x < m_imageWidth; x++)
            {
                const uint8_t r8 = rng.getUint8();
                const uint8_t g8 = rng.getUint8();
                const uint8_t b8 = rng.getUint8();
                const uint8_t a8 = rng.getUint8();

                refAccess.setPixel(UVec4(r8, g8, b8, a8), x, y);
            }
    }
}

class ImageCopyToImage : public CmdCommand
{
public:
    ImageCopyToImage(vk::VkImageLayout imageLayout) : m_imageLayout(imageLayout)
    {
    }
    ~ImageCopyToImage(void)
    {
    }
    const char *getName(void) const
    {
        return "ImageCopyToImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const vk::VkImageLayout m_imageLayout;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::VkDeviceSize m_imageMemorySize;
    vk::Move<vk::VkImage> m_dstImage;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void ImageCopyToImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate destination image for image to image copy." << TestLog::EndMessage;
}

void ImageCopyToImage::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_imageWidth      = context.getImageWidth();
    m_imageHeight     = context.getImageHeight();
    m_imageMemorySize = context.getImageMemorySize();

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0,                                       // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_imageWidth,
                (uint32_t)m_imageHeight,
                1u,
            },                                                                         // VkExtent3D extent;
            1,                                                                         // uint32_t mipLevels;
            1,                                                                         // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_dstImage = vk::createImage(vkd, device, &createInfo);
    }

    m_memory = bindImageMemory(vki, vkd, physicalDevice, device, *m_dstImage, 0);

    {
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_dstImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &barrier);

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);
    }
}

void ImageCopyToImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Copy image to another image"
        << TestLog::EndMessage;
}

void ImageCopyToImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkImageCopy region            = {
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0,                             // uint32_t mipLevel;
            0,                             // uint32_t baseArrayLayer;
            1                              // uint32_t layerCount;
        },                                 // VkImageSubresourceLayers srcSubresource;
        {0, 0, 0},                         // VkOffset3D srcOffset;
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
            0,                                                // uint32_t mipLevel;
            0,                                                // uint32_t baseArrayLayer;
            1                                                 // uint32_t layerCount;
        },                                                    // VkImageSubresourceLayers dstSubresource;
        {0, 0, 0},                                            // VkOffset3D dstOffset;
        {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D extent;
    };

    vkd.cmdCopyImage(commandBuffer, context.getImage(), m_imageLayout, *m_dstImage,
                     vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void ImageCopyToImage::verify(VerifyContext &context, size_t commandIndex)
{
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vk::Unique<vk::VkCommandBuffer> commandBuffer(
        createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const vector<uint32_t> &queueFamilies = context.getContext().getQueueFamilies();
    const vk::Unique<vk::VkBuffer> dstBuffer(createBuffer(vkd, device, 4 * m_imageWidth * m_imageHeight,
                                                          vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                          vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
    const vk::Unique<vk::VkDeviceMemory> memory(
        bindBufferMemory(vki, vkd, physicalDevice, device, *dstBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    {
        const vk::VkImageMemoryBarrier imageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_dstImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkBufferMemoryBarrier bufferBarrier = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            *dstBuffer,                                  // VkBuffer buffer;
            0,                                           // VkDeviceSize offset;
            VK_WHOLE_SIZE                                // VkDeviceSize size;
        };
        const vk::VkBufferImageCopy region = {
            0, // VkDeviceSize bufferOffset;
            0, // uint32_t bufferRowLength;
            0, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT,                    // VkImageAspectFlags aspectMask;
                0,                                                // uint32_t mipLevel;
                0,                                                // uint32_t baseArrayLayer;
                1                                                 // uint32_t layerCount;
            },                                                    // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                            // VkOffset3D imageOffset;
            {(uint32_t)m_imageWidth, (uint32_t)m_imageHeight, 1u} // VkExtent3D imageExtent;
        };

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        vkd.cmdCopyImageToBuffer(*commandBuffer, *m_dstImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstBuffer, 1,
                                 &region);
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }

    endCommandBuffer(vkd, *commandBuffer);
    submitCommandsAndWait(vkd, device, queue, *commandBuffer);

    {
        void *const ptr = mapMemory(vkd, device, *memory, 4 * m_imageWidth * m_imageHeight);

        vk::invalidateMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);

        {
            const uint8_t *const data = (const uint8_t *)ptr;
            const ConstPixelBufferAccess resAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8),
                                                   m_imageWidth, m_imageHeight, 1, data);
            const ConstPixelBufferAccess &refAccess(context.getReferenceImage().getAccess());

            if (!tcu::intThresholdCompare(context.getLog(), (de::toString(commandIndex) + ":" + getName()).c_str(),
                                          (de::toString(commandIndex) + ":" + getName()).c_str(), refAccess, resAccess,
                                          UVec4(0), tcu::COMPARE_LOG_ON_ERROR))
                resultCollector.fail(de::toString(commandIndex) + ":" + getName() + " Image comparison failed");
        }

        vkd.unmapMemory(device, *memory);
    }
}

enum BlitScale
{
    BLIT_SCALE_20,
    BLIT_SCALE_10,
};

class ImageBlitFromImage : public CmdCommand
{
public:
    ImageBlitFromImage(uint32_t seed, BlitScale scale, vk::VkImageLayout imageLayout)
        : m_seed(seed)
        , m_scale(scale)
        , m_imageLayout(imageLayout)
    {
    }
    ~ImageBlitFromImage(void)
    {
    }
    const char *getName(void) const
    {
        return "ImageBlitFromImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const uint32_t m_seed;
    const BlitScale m_scale;
    const vk::VkImageLayout m_imageLayout;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::VkDeviceSize m_imageMemorySize;
    int32_t m_srcImageWidth;
    int32_t m_srcImageHeight;
    vk::Move<vk::VkImage> m_srcImage;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void ImageBlitFromImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Allocate source image for image to image blit."
        << TestLog::EndMessage;
}

void ImageBlitFromImage::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_imageWidth      = context.getImageWidth();
    m_imageHeight     = context.getImageHeight();
    m_imageMemorySize = context.getImageMemorySize();

    if (m_scale == BLIT_SCALE_10)
    {
        m_srcImageWidth  = m_imageWidth;
        m_srcImageHeight = m_imageHeight;
    }
    else if (m_scale == BLIT_SCALE_20)
    {
        m_srcImageWidth  = m_imageWidth == 1 ? 1 : m_imageWidth / 2;
        m_srcImageHeight = m_imageHeight == 1 ? 1 : m_imageHeight / 2;
    }
    else
        DE_FATAL("Unsupported scale");

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0,                                       // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_srcImageWidth,
                (uint32_t)m_srcImageHeight,
                1u,
            },                                                                         // VkExtent3D extent;
            1,                                                                         // uint32_t mipLevels;
            1,                                                                         // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_srcImage = vk::createImage(vkd, device, &createInfo);
    }

    m_memory = bindImageMemory(vki, vkd, physicalDevice, device, *m_srcImage, 0);

    {
        const vk::Unique<vk::VkBuffer> srcBuffer(createBuffer(vkd, device, 4 * m_srcImageWidth * m_srcImageHeight,
                                                              vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
        const vk::Unique<vk::VkDeviceMemory> memory(
            bindBufferMemory(vki, vkd, physicalDevice, device, *srcBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageMemoryBarrier preImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_srcImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkImageMemoryBarrier postImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            0,                                          // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_srcImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkBufferImageCopy region = {
            0, // VkDeviceSize bufferOffset;
            0, // uint32_t bufferRowLength;
            0, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT,                          // VkImageAspectFlags aspectMask;
                0,                                                      // uint32_t mipLevel;
                0,                                                      // uint32_t baseArrayLayer;
                1                                                       // uint32_t layerCount;
            },                                                          // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                                  // VkOffset3D imageOffset;
            {(uint32_t)m_srcImageWidth, (uint32_t)m_srcImageHeight, 1u} // VkExtent3D imageExtent;
        };

        {
            void *const ptr = mapMemory(vkd, device, *memory, 4 * m_srcImageWidth * m_srcImageHeight);
            de::Random rng(m_seed);

            {
                uint8_t *const data = (uint8_t *)ptr;

                for (size_t ndx = 0; ndx < (size_t)(4 * m_srcImageWidth * m_srcImageHeight); ndx++)
                    data[ndx] = rng.getUint8();
            }

            vk::flushMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);
            vkd.unmapMemory(device, *memory);
        }

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &preImageBarrier);
        vkd.cmdCopyBufferToImage(*commandBuffer, *srcBuffer, *m_srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                 &region);
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);
    }
}

void ImageBlitFromImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Blit from another image"
        << (m_scale == BLIT_SCALE_20 ? " scale 2x" : "") << TestLog::EndMessage;
}

void ImageBlitFromImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkImageBlit region            = {
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0,                             // uint32_t mipLevel;
            0,                             // uint32_t baseArrayLayer;
            1                              // uint32_t layerCount;
        },                                 // VkImageSubresourceLayers srcSubresource;
        {
            {0, 0, 0},
            {m_srcImageWidth, m_srcImageHeight, 1},
        }, // VkOffset3D srcOffsets[2];
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,             // VkImageAspectFlags aspectMask;
            0,                                         // uint32_t mipLevel;
            0,                                         // uint32_t baseArrayLayer;
            1                                          // uint32_t layerCount;
        },                                             // VkImageSubresourceLayers dstSubresource;
        {{0, 0, 0}, {m_imageWidth, m_imageHeight, 1u}} // VkOffset3D dstOffsets[2];
    };
    vkd.cmdBlitImage(commandBuffer, *m_srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, context.getImage(),
                     m_imageLayout, 1, &region, vk::VK_FILTER_NEAREST);
}

void ImageBlitFromImage::verify(VerifyContext &context, size_t)
{
    ReferenceMemory &reference(context.getReference());
    de::Random rng(m_seed);

    reference.setUndefined(0, (size_t)m_imageMemorySize);

    {
        const PixelBufferAccess &refAccess(context.getReferenceImage().getAccess());

        if (m_scale == BLIT_SCALE_10)
        {
            for (int32_t y = 0; y < m_imageHeight; y++)
                for (int32_t x = 0; x < m_imageWidth; x++)
                {
                    const uint8_t r8 = rng.getUint8();
                    const uint8_t g8 = rng.getUint8();
                    const uint8_t b8 = rng.getUint8();
                    const uint8_t a8 = rng.getUint8();

                    refAccess.setPixel(UVec4(r8, g8, b8, a8), x, y);
                }
        }
        else if (m_scale == BLIT_SCALE_20)
        {
            tcu::TextureLevel source(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), m_srcImageWidth,
                                     m_srcImageHeight);
            const float xscale = ((float)m_srcImageWidth) / (float)m_imageWidth;
            const float yscale = ((float)m_srcImageHeight) / (float)m_imageHeight;

            for (int32_t y = 0; y < m_srcImageHeight; y++)
                for (int32_t x = 0; x < m_srcImageWidth; x++)
                {
                    const uint8_t r8 = rng.getUint8();
                    const uint8_t g8 = rng.getUint8();
                    const uint8_t b8 = rng.getUint8();
                    const uint8_t a8 = rng.getUint8();

                    source.getAccess().setPixel(UVec4(r8, g8, b8, a8), x, y);
                }

            for (int32_t y = 0; y < m_imageHeight; y++)
                for (int32_t x = 0; x < m_imageWidth; x++)
                    refAccess.setPixel(source.getAccess().getPixelUint(int((float(x) + 0.5f) * xscale),
                                                                       int((float(y) + 0.5f) * yscale)),
                                       x, y);
        }
        else
            DE_FATAL("Unsupported scale");
    }
}

class ImageBlitToImage : public CmdCommand
{
public:
    ImageBlitToImage(BlitScale scale, vk::VkImageLayout imageLayout) : m_scale(scale), m_imageLayout(imageLayout)
    {
    }
    ~ImageBlitToImage(void)
    {
    }
    const char *getName(void) const
    {
        return "ImageBlitToImage";
    }

    void logPrepare(TestLog &log, size_t commandIndex) const;
    void prepare(PrepareContext &context);
    void logSubmit(TestLog &log, size_t commandIndex) const;
    void submit(SubmitContext &context);
    void verify(VerifyContext &context, size_t commandIndex);

private:
    const BlitScale m_scale;
    const vk::VkImageLayout m_imageLayout;
    int32_t m_imageWidth;
    int32_t m_imageHeight;
    vk::VkDeviceSize m_imageMemorySize;
    int32_t m_dstImageWidth;
    int32_t m_dstImageHeight;
    vk::Move<vk::VkImage> m_dstImage;
    vk::Move<vk::VkDeviceMemory> m_memory;
};

void ImageBlitToImage::logPrepare(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName()
        << " Allocate destination image for image to image blit." << TestLog::EndMessage;
}

void ImageBlitToImage::prepare(PrepareContext &context)
{
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vector<uint32_t> &queueFamilies     = context.getContext().getQueueFamilies();

    m_imageWidth      = context.getImageWidth();
    m_imageHeight     = context.getImageHeight();
    m_imageMemorySize = context.getImageMemorySize();

    if (m_scale == BLIT_SCALE_10)
    {
        m_dstImageWidth  = context.getImageWidth();
        m_dstImageHeight = context.getImageHeight();
    }
    else if (m_scale == BLIT_SCALE_20)
    {
        m_dstImageWidth  = context.getImageWidth() * 2;
        m_dstImageHeight = context.getImageHeight() * 2;
    }
    else
        DE_FATAL("Unsupportd blit scale");

    {
        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            0,                                       // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
            {
                (uint32_t)m_dstImageWidth,
                (uint32_t)m_dstImageHeight,
                1u,
            },                                                                         // VkExtent3D extent;
            1,                                                                         // uint32_t mipLevels;
            1,                                                                         // uint32_t arrayLayers;
            vk::VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            (uint32_t)queueFamilies.size(), // uint32_t queueFamilyIndexCount;
            &queueFamilies[0],              // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED   // VkImageLayout initialLayout;
        };

        m_dstImage = vk::createImage(vkd, device, &createInfo);
    }

    m_memory = bindImageMemory(vki, vkd, physicalDevice, device, *m_dstImage, 0);

    {
        const vk::Unique<vk::VkCommandBuffer> commandBuffer(
            createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const vk::VkImageMemoryBarrier barrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0,                                          // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_dstImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                               &barrier);

        endCommandBuffer(vkd, *commandBuffer);
        submitCommandsAndWait(vkd, device, queue, *commandBuffer);
    }
}

void ImageBlitToImage::logSubmit(TestLog &log, size_t commandIndex) const
{
    log << TestLog::Message << commandIndex << ":" << getName() << " Blit image to another image"
        << (m_scale == BLIT_SCALE_20 ? " scale 2x" : "") << TestLog::EndMessage;
}

void ImageBlitToImage::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();
    const vk::VkImageBlit region            = {
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0,                             // uint32_t mipLevel;
            0,                             // uint32_t baseArrayLayer;
            1                              // uint32_t layerCount;
        },                                 // VkImageSubresourceLayers srcSubresource;
        {
            {0, 0, 0},
            {m_imageWidth, m_imageHeight, 1},
        }, // VkOffset3D srcOffsets[2];
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                   // VkImageAspectFlags aspectMask;
            0,                                               // uint32_t mipLevel;
            0,                                               // uint32_t baseArrayLayer;
            1                                                // uint32_t layerCount;
        },                                                   // VkImageSubresourceLayers dstSubresource;
        {{0, 0, 0}, {m_dstImageWidth, m_dstImageHeight, 1u}} // VkOffset3D dstOffsets[2];
    };
    vkd.cmdBlitImage(commandBuffer, context.getImage(), m_imageLayout, *m_dstImage,
                     vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, vk::VK_FILTER_NEAREST);
}

void ImageBlitToImage::verify(VerifyContext &context, size_t commandIndex)
{
    tcu::ResultCollector &resultCollector(context.getResultCollector());
    const vk::InstanceInterface &vki          = context.getContext().getInstanceInterface();
    const vk::DeviceInterface &vkd            = context.getContext().getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getContext().getPhysicalDevice();
    const vk::VkDevice device                 = context.getContext().getDevice();
    const vk::VkQueue queue                   = context.getContext().getQueue();
    const vk::VkCommandPool commandPool       = context.getContext().getCommandPool();
    const vk::Unique<vk::VkCommandBuffer> commandBuffer(
        createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const vector<uint32_t> &queueFamilies = context.getContext().getQueueFamilies();
    const vk::Unique<vk::VkBuffer> dstBuffer(createBuffer(vkd, device, 4 * m_dstImageWidth * m_dstImageHeight,
                                                          vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                          vk::VK_SHARING_MODE_EXCLUSIVE, queueFamilies));
    const vk::Unique<vk::VkDeviceMemory> memory(
        bindBufferMemory(vki, vkd, physicalDevice, device, *dstBuffer, vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    {
        const vk::VkImageMemoryBarrier imageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            *m_dstImage,                                // VkImage image;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0,                             // uint32_t baseMipLevel;
                1,                             // uint32_t levelCount;
                0,                             // uint32_t baseArrayLayer;
                1                              // uint32_t layerCount;
            }                                  // VkImageSubresourceRange subresourceRange;
        };
        const vk::VkBufferMemoryBarrier bufferBarrier = {
            vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
            *dstBuffer,                                  // VkBuffer buffer;
            0,                                           // VkDeviceSize offset;
            VK_WHOLE_SIZE                                // VkDeviceSize size;
        };
        const vk::VkBufferImageCopy region = {
            0, // VkDeviceSize bufferOffset;
            0, // uint32_t bufferRowLength;
            0, // uint32_t bufferImageHeight;
            {
                vk::VK_IMAGE_ASPECT_COLOR_BIT,                         // VkImageAspectFlags aspectMask;
                0,                                                     // uint32_t mipLevel;
                0,                                                     // uint32_t baseArrayLayer;
                1                                                      // uint32_t layerCount;
            },                                                         // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},                                                 // VkOffset3D imageOffset;
            {(uint32_t)m_dstImageWidth, (uint32_t)m_dstImageHeight, 1} // VkExtent3D imageExtent;
        };

        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
        vkd.cmdCopyImageToBuffer(*commandBuffer, *m_dstImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstBuffer, 1,
                                 &region);
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT,
                               (vk::VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    }

    endCommandBuffer(vkd, *commandBuffer);
    submitCommandsAndWait(vkd, device, queue, *commandBuffer);

    {
        void *const ptr = mapMemory(vkd, device, *memory, 4 * m_dstImageWidth * m_dstImageHeight);

        vk::invalidateMappedMemoryRange(vkd, device, *memory, 0, VK_WHOLE_SIZE);

        if (m_scale == BLIT_SCALE_10)
        {
            const uint8_t *const data = (const uint8_t *)ptr;
            const ConstPixelBufferAccess resAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8),
                                                   m_dstImageWidth, m_dstImageHeight, 1, data);
            const ConstPixelBufferAccess &refAccess(context.getReferenceImage().getAccess());

            if (!tcu::intThresholdCompare(context.getLog(), (de::toString(commandIndex) + ":" + getName()).c_str(),
                                          (de::toString(commandIndex) + ":" + getName()).c_str(), refAccess, resAccess,
                                          UVec4(0), tcu::COMPARE_LOG_ON_ERROR))
                resultCollector.fail(de::toString(commandIndex) + ":" + getName() + " Image comparison failed");
        }
        else if (m_scale == BLIT_SCALE_20)
        {
            const uint8_t *const data = (const uint8_t *)ptr;
            const ConstPixelBufferAccess resAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8),
                                                   m_dstImageWidth, m_dstImageHeight, 1, data);
            tcu::TextureLevel reference(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), m_dstImageWidth,
                                        m_dstImageHeight, 1);

            {
                const ConstPixelBufferAccess &refAccess(context.getReferenceImage().getAccess());

                for (int32_t y = 0; y < m_dstImageHeight; y++)
                    for (int32_t x = 0; x < m_dstImageWidth; x++)
                    {
                        reference.getAccess().setPixel(refAccess.getPixel(x / 2, y / 2), x, y);
                    }
            }

            if (!tcu::intThresholdCompare(context.getLog(), (de::toString(commandIndex) + ":" + getName()).c_str(),
                                          (de::toString(commandIndex) + ":" + getName()).c_str(), reference.getAccess(),
                                          resAccess, UVec4(0), tcu::COMPARE_LOG_ON_ERROR))
                resultCollector.fail(de::toString(commandIndex) + ":" + getName() + " Image comparison failed");
        }
        else
            DE_FATAL("Unknown scale");

        vkd.unmapMemory(device, *memory);
    }
}

class ExecuteSecondaryCommandBuffer : public CmdCommand
{
public:
    ExecuteSecondaryCommandBuffer(const vector<CmdCommand *> &commands);
    ~ExecuteSecondaryCommandBuffer(void);
    const char *getName(void) const
    {
        return "ExecuteSecondaryCommandBuffer";
    }

    void logPrepare(TestLog &, size_t) const;
    void logSubmit(TestLog &, size_t) const;
    bool logSubmitFailureTrace(TestLog &, size_t, const string &) const;

    void prepare(PrepareContext &);
    void submit(SubmitContext &);

    void verify(VerifyContext &, size_t);

private:
    vk::Move<vk::VkCommandBuffer> m_commandBuffer;
    vk::Move<vk::VkDeviceMemory> m_colorTargetMemory;
    de::MovePtr<vk::Allocation> m_colorTargetMemory2;
    vk::Move<vk::VkImage> m_colorTarget;
    vk::Move<vk::VkImageView> m_colorTargetView;
    vk::Move<vk::VkFramebuffer> m_framebuffer;
    vector<CmdCommand *> m_commands;
    // Index of the sub-command that first introduced a verification failure, or -1
    // if none. Recorded during verify() and consumed by logSubmitFailureTrace().
    int m_firstFailingCmdNdx;
};

ExecuteSecondaryCommandBuffer::ExecuteSecondaryCommandBuffer(const vector<CmdCommand *> &commands)
    : m_commands(commands)
    , m_firstFailingCmdNdx(-1)
{
}

ExecuteSecondaryCommandBuffer::~ExecuteSecondaryCommandBuffer(void)
{
    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        delete m_commands[cmdNdx];
}

void ExecuteSecondaryCommandBuffer::logPrepare(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        CmdCommand &command = *m_commands[cmdNdx];
        command.logPrepare(log, cmdNdx);
    }
}

void ExecuteSecondaryCommandBuffer::logSubmit(TestLog &log, size_t commandIndex) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        CmdCommand &command = *m_commands[cmdNdx];
        command.logSubmit(log, cmdNdx);
    }
}

bool ExecuteSecondaryCommandBuffer::logSubmitFailureTrace(TestLog &log, size_t commandIndex,
                                                          const string &failureMessage) const
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(log, sectionName, sectionName);
    bool marked = false;

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        const bool childMarked = m_commands[cmdNdx]->logSubmitFailureTrace(log, cmdNdx, failureMessage);

        marked = marked || childMarked;

        if (!childMarked && (int)cmdNdx == m_firstFailingCmdNdx)
        {
            log << TestLog::Message << "Failure origin: " << failureMessage << TestLog::EndMessage;
            marked = true;
        }
    }

    return marked;
}

void ExecuteSecondaryCommandBuffer::prepare(PrepareContext &context)
{
    const vk::DeviceInterface &vkd      = context.getContext().getDeviceInterface();
    const vk::VkDevice device           = context.getContext().getDevice();
    const vk::VkCommandPool commandPool = context.getContext().getCommandPool();

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        CmdCommand &command = *m_commands[cmdNdx];

        command.prepare(context);
    }

    m_commandBuffer = createBeginCommandBuffer(vkd, device, commandPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    {
        SubmitContext submitContext(context, *m_commandBuffer);

        for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        {
            CmdCommand &command = *m_commands[cmdNdx];

            command.submit(submitContext);
        }

        endCommandBuffer(vkd, *m_commandBuffer);
    }
}

void ExecuteSecondaryCommandBuffer::submit(SubmitContext &context)
{
    const vk::DeviceInterface &vkd          = context.getContext().getDeviceInterface();
    const vk::VkCommandBuffer commandBuffer = context.getCommandBuffer();

    {
        vkd.cmdExecuteCommands(commandBuffer, 1, &m_commandBuffer.get());
    }
}

void ExecuteSecondaryCommandBuffer::verify(VerifyContext &context, size_t commandIndex)
{
    const string sectionName(de::toString(commandIndex) + ":" + getName());
    const tcu::ScopedLogSection section(context.getLog(), sectionName, sectionName);

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        const bool failedBefore = context.getResultCollector().getResult() != QP_TEST_RESULT_PASS;

        m_commands[cmdNdx]->verify(context, cmdNdx);

        if (m_firstFailingCmdNdx == -1 && !failedBefore &&
            context.getResultCollector().getResult() != QP_TEST_RESULT_PASS)
            m_firstFailingCmdNdx = (int)cmdNdx;
    }
}
vk::VkAccessFlags getWriteAccessFlags(void)
{
    return vk::VK_ACCESS_SHADER_WRITE_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
           vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | vk::VK_ACCESS_TRANSFER_WRITE_BIT |
           vk::VK_ACCESS_HOST_WRITE_BIT | vk::VK_ACCESS_MEMORY_WRITE_BIT;
}

bool isWriteAccess(vk::VkAccessFlagBits access)
{
    return (getWriteAccessFlags() & access) != 0;
}
CacheState::CacheState(vk::VkPipelineStageFlags allowedStages, vk::VkAccessFlags allowedAccesses)
    : m_allowedStages(allowedStages)
    , m_allowedAccesses(allowedAccesses)
{
    for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= m_allowedStages; dstStage_ <<= 1)
    {
        if ((dstStage_ & m_allowedStages) == 0)
            continue;

        const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

        // All operations are initially visible
        m_invisibleOperations[dstStage] = 0;

        // There are no incomplete read operations initially
        m_incompleteOperations[dstStage] = 0;

        // There are no incomplete layout transitions
        m_unavailableLayoutTransition[dstStage] = false;

        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= m_allowedStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & m_allowedStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            // There are no write operations that are not yet available
            // initially.
            for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
            {
                if ((dstAccess_ & m_allowedAccesses) == 0)
                    continue;

                const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                m_unavailableWriteOperations[dstStage][srcStage][dstAccess] = 0;
            }
        }
    }
}

bool CacheState::isValid(vk::VkPipelineStageFlagBits stage, vk::VkAccessFlagBits access) const
{
    DE_ASSERT((access & (~m_allowedAccesses)) == 0);
    DE_ASSERT((stage & (~m_allowedStages)) == 0);

    const PipelineStage dstStage = pipelineStageFlagToPipelineStage(stage);

    // Previous operations are not visible to access on stage
    if (m_unavailableLayoutTransition[dstStage] || (m_invisibleOperations[dstStage] & access) != 0)
        return false;

    if (isWriteAccess(access))
    {
        // Memory operations from other stages have not completed before
        // dstStage
        if (m_incompleteOperations[dstStage] != 0)
            return false;
    }

    return true;
}

void CacheState::perform(vk::VkPipelineStageFlagBits stage, vk::VkAccessFlagBits access)
{
    DE_ASSERT((access & (~m_allowedAccesses)) == 0);
    DE_ASSERT((stage & (~m_allowedStages)) == 0);

    const PipelineStage srcStage = pipelineStageFlagToPipelineStage(stage);

    for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= m_allowedStages; dstStage_ <<= 1)
    {
        if ((dstStage_ & m_allowedStages) == 0)
            continue;

        const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

        // Mark stage as incomplete for all stages
        m_incompleteOperations[dstStage] |= stage;

        if (isWriteAccess(access))
        {
            // Mark all accesses from all stages invisible
            m_invisibleOperations[dstStage] |= m_allowedAccesses;

            // Mark write access from srcStage unavailable to all stages for all accesses
            for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
            {
                if ((dstAccess_ & m_allowedAccesses) == 0)
                    continue;

                const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                m_unavailableWriteOperations[dstStage][srcStage][dstAccess] |= access;
            }
        }
    }
}

void CacheState::submitCommandBuffer(void)
{
    // Flush all host writes and reads
    barrier(m_allowedStages & vk::VK_PIPELINE_STAGE_HOST_BIT,
            m_allowedAccesses & (vk::VK_ACCESS_HOST_READ_BIT | vk::VK_ACCESS_HOST_WRITE_BIT), m_allowedStages,
            m_allowedAccesses);
}

void CacheState::waitForIdle(void)
{
    // Make all writes available
    barrier(m_allowedStages, m_allowedAccesses & getWriteAccessFlags(), m_allowedStages, 0);

    // Make all writes visible on device side
    barrier(m_allowedStages, 0, m_allowedStages & (~vk::VK_PIPELINE_STAGE_HOST_BIT), m_allowedAccesses);
}

void CacheState::getFullBarrier(vk::VkPipelineStageFlags &srcStages, vk::VkAccessFlags &srcAccesses,
                                vk::VkPipelineStageFlags &dstStages, vk::VkAccessFlags &dstAccesses) const
{
    srcStages   = 0;
    srcAccesses = 0;
    dstStages   = 0;
    dstAccesses = 0;

    for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= m_allowedStages; dstStage_ <<= 1)
    {
        if ((dstStage_ & m_allowedStages) == 0)
            continue;

        const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

        // Make sure all previous operation are complete in all stages
        if (m_incompleteOperations[dstStage])
        {
            dstStages |= dstStage_;
            srcStages |= m_incompleteOperations[dstStage];
        }

        // Make sure all read operations are visible in dstStage
        if (m_invisibleOperations[dstStage])
        {
            dstStages |= dstStage_;
            dstAccesses |= m_invisibleOperations[dstStage];
        }

        // Make sure all write operations from all stages are available
        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= m_allowedStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & m_allowedStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
            {
                if ((dstAccess_ & m_allowedAccesses) == 0)
                    continue;

                const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                if (m_unavailableWriteOperations[dstStage][srcStage][dstAccess])
                {
                    dstStages |= dstStage_;
                    srcStages |= dstStage_;
                    srcAccesses |= m_unavailableWriteOperations[dstStage][srcStage][dstAccess];
                }
            }

            if (m_unavailableLayoutTransition[dstStage] && !m_unavailableLayoutTransition[srcStage])
            {
                // Add dependency between srcStage and dstStage if layout transition has not completed in dstStage,
                // but has completed in srcStage.
                dstStages |= dstStage_;
                srcStages |= dstStage_;
            }
        }
    }

    DE_ASSERT((srcStages & (~m_allowedStages)) == 0);
    DE_ASSERT((srcAccesses & (~m_allowedAccesses)) == 0);
    DE_ASSERT((dstStages & (~m_allowedStages)) == 0);
    DE_ASSERT((dstAccesses & (~m_allowedAccesses)) == 0);
}

void CacheState::checkImageLayoutBarrier(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,
                                         vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses)
{
    DE_ASSERT((srcStages & (~m_allowedStages)) == 0);
    DE_ASSERT((srcAccesses & (~m_allowedAccesses)) == 0);
    DE_ASSERT((dstStages & (~m_allowedStages)) == 0);
    DE_ASSERT((dstAccesses & (~m_allowedAccesses)) == 0);

    DE_UNREF(srcStages);
    DE_UNREF(srcAccesses);

    DE_UNREF(dstStages);
    DE_UNREF(dstAccesses);

#if defined(DE_DEBUG)
    // Check that all stages have completed before srcStages or are in srcStages.
    {
        vk::VkPipelineStageFlags completedStages = srcStages;

        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= srcStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & srcStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            completedStages |= (~m_incompleteOperations[srcStage]);
        }

        DE_ASSERT((completedStages & m_allowedStages) == m_allowedStages);
    }

    // Check that any write is available at least in one stage. Since all stages are complete even single flush is enough.
    if ((getWriteAccessFlags() & m_allowedAccesses) != 0 && (srcAccesses & getWriteAccessFlags()) == 0)
    {
        bool anyWriteAvailable = false;

        for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= m_allowedStages; dstStage_ <<= 1)
        {
            if ((dstStage_ & m_allowedStages) == 0)
                continue;

            const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

            for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= m_allowedStages; srcStage_ <<= 1)
            {
                if ((srcStage_ & m_allowedStages) == 0)
                    continue;

                const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

                for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
                {
                    if ((dstAccess_ & m_allowedAccesses) == 0)
                        continue;

                    const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                    if (m_unavailableWriteOperations[dstStage][srcStage][dstAccess] !=
                        (getWriteAccessFlags() & m_allowedAccesses))
                    {
                        anyWriteAvailable = true;
                        break;
                    }
                }
            }
        }

        DE_ASSERT(anyWriteAvailable);
    }
#endif
}

void CacheState::imageLayoutBarrier(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,
                                    vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses)
{
    checkImageLayoutBarrier(srcStages, srcAccesses, dstStages, dstAccesses);

    for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= m_allowedStages; dstStage_ <<= 1)
    {
        if ((dstStage_ & m_allowedStages) == 0)
            continue;

        const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

        // All stages are incomplete after the barrier except each dstStage in it self.
        m_incompleteOperations[dstStage] = m_allowedStages & (~dstStage_);

        // All memory operations are invisible unless they are listed in dstAccess
        m_invisibleOperations[dstStage] = m_allowedAccesses & (~dstAccesses);

        // Layout transition is unavailable in stage unless it was listed in dstStages
        m_unavailableLayoutTransition[dstStage] = (dstStage_ & dstStages) == 0;

        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= m_allowedStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & m_allowedStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            // All write operations are available after layout transition
            for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
            {
                if ((dstAccess_ & m_allowedAccesses) == 0)
                    continue;

                const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                m_unavailableWriteOperations[dstStage][srcStage][dstAccess] = 0;
            }
        }
    }
}

void CacheState::barrier(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,
                         vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses)
{
    DE_ASSERT((srcStages & (~m_allowedStages)) == 0);
    DE_ASSERT((srcAccesses & (~m_allowedAccesses)) == 0);
    DE_ASSERT((dstStages & (~m_allowedStages)) == 0);
    DE_ASSERT((dstAccesses & (~m_allowedAccesses)) == 0);

    // Transitivity
    {
        vk::VkPipelineStageFlags oldIncompleteOperations[PIPELINESTAGE_LAST];
        vk::VkAccessFlags oldUnavailableWriteOperations[PIPELINESTAGE_LAST][PIPELINESTAGE_LAST][ACCESS_LAST];
        bool oldUnavailableLayoutTransition[PIPELINESTAGE_LAST];

        deMemcpy(oldIncompleteOperations, m_incompleteOperations, sizeof(oldIncompleteOperations));
        deMemcpy(oldUnavailableWriteOperations, m_unavailableWriteOperations, sizeof(oldUnavailableWriteOperations));
        deMemcpy(oldUnavailableLayoutTransition, m_unavailableLayoutTransition, sizeof(oldUnavailableLayoutTransition));

        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= srcStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & srcStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= dstStages; dstStage_ <<= 1)
            {
                if ((dstStage_ & dstStages) == 0)
                    continue;

                const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

                // Stages that have completed before srcStage have also completed before dstStage
                m_incompleteOperations[dstStage] &= oldIncompleteOperations[srcStage];

                // Image layout transition in srcStage are now available in dstStage
                m_unavailableLayoutTransition[dstStage] &= oldUnavailableLayoutTransition[srcStage];

                for (vk::VkPipelineStageFlags sharedStage_ = 1; sharedStage_ <= m_allowedStages; sharedStage_ <<= 1)
                {
                    if ((sharedStage_ & m_allowedStages) == 0)
                        continue;

                    const PipelineStage sharedStage =
                        pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)sharedStage_);

                    // Writes that are available in srcStage are also available in dstStage
                    for (vk::VkAccessFlags sharedAccess_ = 1; sharedAccess_ <= m_allowedAccesses; sharedAccess_ <<= 1)
                    {
                        if ((sharedAccess_ & m_allowedAccesses) == 0)
                            continue;

                        const Access sharedAccess = accessFlagToAccess((vk::VkAccessFlagBits)sharedAccess_);

                        m_unavailableWriteOperations[dstStage][sharedStage][sharedAccess] &=
                            oldUnavailableWriteOperations[srcStage][sharedStage][sharedAccess];
                    }
                }
            }
        }
    }

    // Barrier
    for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= dstStages; dstStage_ <<= 1)
    {
        const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);
        bool allWritesAvailable      = true;

        if ((dstStage_ & dstStages) == 0)
            continue;

        // Operations in srcStages have completed before any stage in dstStages
        m_incompleteOperations[dstStage] &= ~srcStages;

        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= m_allowedStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & m_allowedStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            // Make srcAccesses from srcStage available in dstStage for dstAccess
            for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
            {
                if ((dstAccess_ & m_allowedAccesses) == 0)
                    continue;

                const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                if (((srcStage_ & srcStages) != 0) && ((dstAccess_ & dstAccesses) != 0))
                    m_unavailableWriteOperations[dstStage][srcStage][dstAccess] &= ~srcAccesses;

                if (m_unavailableWriteOperations[dstStage][srcStage][dstAccess] != 0)
                    allWritesAvailable = false;
            }
        }

        // If all writes are available in dstStage make dstAccesses also visible
        if (allWritesAvailable)
            m_invisibleOperations[dstStage] &= ~dstAccesses;
    }
}

bool CacheState::isClean(void) const
{
    for (vk::VkPipelineStageFlags dstStage_ = 1; dstStage_ <= m_allowedStages; dstStage_ <<= 1)
    {
        if ((dstStage_ & m_allowedStages) == 0)
            continue;

        const PipelineStage dstStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)dstStage_);

        // Some operations are not visible to some stages
        if (m_invisibleOperations[dstStage] != 0)
            return false;

        // There are operation that have not completed yet
        if (m_incompleteOperations[dstStage] != 0)
            return false;

        // Layout transition has not completed yet
        if (m_unavailableLayoutTransition[dstStage])
            return false;

        for (vk::VkPipelineStageFlags srcStage_ = 1; srcStage_ <= m_allowedStages; srcStage_ <<= 1)
        {
            if ((srcStage_ & m_allowedStages) == 0)
                continue;

            const PipelineStage srcStage = pipelineStageFlagToPipelineStage((vk::VkPipelineStageFlagBits)srcStage_);

            for (vk::VkAccessFlags dstAccess_ = 1; dstAccess_ <= m_allowedAccesses; dstAccess_ <<= 1)
            {
                if ((dstAccess_ & m_allowedAccesses) == 0)
                    continue;

                const Access dstAccess = accessFlagToAccess((vk::VkAccessFlagBits)dstAccess_);

                // Some write operations are not available yet
                if (m_unavailableWriteOperations[dstStage][srcStage][dstAccess] != 0)
                    return false;
            }
        }
    }

    return true;
}

bool layoutSupportedByUsage(Usage usage, vk::VkImageLayout layout)
{
    switch (layout)
    {
    case vk::VK_IMAGE_LAYOUT_GENERAL:
        return true;

    case vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return (usage & USAGE_COLOR_ATTACHMENT) != 0;

    case vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return (usage & USAGE_DEPTH_STENCIL_ATTACHMENT) != 0;

    case vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return (usage & USAGE_DEPTH_STENCIL_ATTACHMENT) != 0;

    case vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        // \todo [2016-03-09 mika] Should include input attachment
        return (usage & USAGE_SAMPLED_IMAGE) != 0;

    case vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return (usage & USAGE_TRANSFER_SRC) != 0;

    case vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return (usage & USAGE_TRANSFER_DST) != 0;

    case vk::VK_IMAGE_LAYOUT_PREINITIALIZED:
        return true;

    default:
        DE_FATAL("Unknown layout");
        return false;
    }
}
size_t getNumberOfSupportedLayouts(Usage usage)
{
    const vk::VkImageLayout layouts[] = {
        vk::VK_IMAGE_LAYOUT_GENERAL,
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    };
    size_t supportedLayoutCount = 0;

    for (size_t layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layouts); layoutNdx++)
    {
        const vk::VkImageLayout layout = layouts[layoutNdx];

        if (layoutSupportedByUsage(usage, layout))
            supportedLayoutCount++;
    }

    return supportedLayoutCount;
}

vk::VkImageLayout getRandomNextLayout(de::Random &rng, Usage usage, vk::VkImageLayout previousLayout)
{
    const vk::VkImageLayout layouts[] = {
        vk::VK_IMAGE_LAYOUT_GENERAL,
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    };
    const size_t supportedLayoutCount = getNumberOfSupportedLayouts(usage);

    DE_ASSERT(supportedLayoutCount > 0);

    size_t nextLayoutNdx =
        ((size_t)rng.getUint32()) %
        (previousLayout == vk::VK_IMAGE_LAYOUT_UNDEFINED ? supportedLayoutCount : supportedLayoutCount - 1);

    for (size_t layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layouts); layoutNdx++)
    {
        const vk::VkImageLayout layout = layouts[layoutNdx];

        if (layoutSupportedByUsage(usage, layout) && layout != previousLayout)
        {
            if (nextLayoutNdx == 0)
                return layout;
            else
                nextLayoutNdx--;
        }
    }

    DE_FATAL("Unreachable");
    return vk::VK_IMAGE_LAYOUT_UNDEFINED;
}
void getAvailableOps(const State &state, bool supportsBuffers, bool supportsImages, Usage usage, TestBackend backend,
                     vector<Op> &ops)
{
    if (state.stage == STAGE_HOST)
    {
        if (usage & (USAGE_HOST_READ | USAGE_HOST_WRITE))
        {
            // Host memory operations
            if (state.mapped)
            {
                ops.push_back(OP_UNMAP);

                // Avoid flush and finish if they are not needed
                if (!state.hostFlushed)
                    ops.push_back(OP_MAP_FLUSH);

                if (!state.hostInvalidated && state.queueIdle &&
                    ((usage & USAGE_HOST_READ) == 0 ||
                     state.cache.isValid(vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_ACCESS_HOST_READ_BIT)) &&
                    ((usage & USAGE_HOST_WRITE) == 0 ||
                     state.cache.isValid(vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_ACCESS_HOST_WRITE_BIT)))
                {
                    ops.push_back(OP_MAP_INVALIDATE);
                }

                if (usage & USAGE_HOST_READ && usage & USAGE_HOST_WRITE && state.memoryDefined &&
                    state.hostInvalidated && state.queueIdle &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_ACCESS_HOST_WRITE_BIT) &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_ACCESS_HOST_READ_BIT))
                {
                    ops.push_back(OP_MAP_MODIFY);
                }

                if (usage & USAGE_HOST_READ && state.memoryDefined && state.hostInvalidated && state.queueIdle &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_ACCESS_HOST_READ_BIT))
                {
                    ops.push_back(OP_MAP_READ);
                }

                if (usage & USAGE_HOST_WRITE && state.hostInvalidated && state.queueIdle &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_ACCESS_HOST_WRITE_BIT))
                {
                    ops.push_back(OP_MAP_WRITE);
                }
            }
            else
                ops.push_back(OP_MAP);
        }

        if (state.hasBoundBufferMemory && state.queueIdle)
        {
            // \note Destroy only buffers after they have been bound
            ops.push_back(OP_BUFFER_DESTROY);
        }
        else
        {
            if (state.hasBuffer)
            {
                if (!state.hasBoundBufferMemory)
                    ops.push_back(OP_BUFFER_BINDMEMORY);
            }
            else if (!state.hasImage && supportsBuffers) // Avoid creating buffer if there is already image
                ops.push_back(OP_BUFFER_CREATE);
        }

        if (state.hasBoundImageMemory && state.queueIdle)
        {
            // \note Destroy only image after they have been bound
            ops.push_back(OP_IMAGE_DESTROY);
        }
        else
        {
            if (state.hasImage)
            {
                if (!state.hasBoundImageMemory)
                    ops.push_back(OP_IMAGE_BINDMEMORY);
            }
            else if (!state.hasBuffer && supportsImages) // Avoid creating image if there is already buffer
                ops.push_back(OP_IMAGE_CREATE);
        }

        // Host writes must be flushed before GPU commands and there must be
        // buffer or image for GPU commands
        if (state.hostFlushed &&
            (state.memoryDefined || supportsDeviceBufferWrites(usage) || state.imageDefined ||
             supportsDeviceImageWrites(usage)) &&
            (state.hasBoundBufferMemory ||
             state.hasBoundImageMemory) // Avoid command buffers if there is no object to use
            && (usageToStageFlags(usage, backend) & (~vk::VK_PIPELINE_STAGE_HOST_BIT)) !=
                   0) // Don't start command buffer if there are no ways to use memory from gpu
        {
            ops.push_back(OP_COMMAND_BUFFER_BEGIN);
        }

        if (!state.deviceIdle)
            ops.push_back(OP_DEVICE_WAIT_FOR_IDLE);

        if (!state.queueIdle)
            ops.push_back(OP_QUEUE_WAIT_FOR_IDLE);
    }
    else if (state.stage == STAGE_COMMAND_BUFFER)
    {
        if (!state.cache.isClean())
        {
            ops.push_back(OP_PIPELINE_BARRIER_GLOBAL);

            if (state.hasImage && (state.imageLayout != vk::VK_IMAGE_LAYOUT_UNDEFINED))
                ops.push_back(OP_PIPELINE_BARRIER_IMAGE);

            if (state.hasBuffer)
                ops.push_back(OP_PIPELINE_BARRIER_BUFFER);
        }

        if (state.hasBoundBufferMemory)
        {
            if (usage & USAGE_TRANSFER_DST &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT))
            {
                ops.push_back(OP_BUFFER_FILL);
                ops.push_back(OP_BUFFER_UPDATE);
                ops.push_back(OP_BUFFER_COPY_FROM_BUFFER);
                ops.push_back(OP_BUFFER_COPY_FROM_IMAGE);
            }

            if (usage & USAGE_TRANSFER_SRC && state.memoryDefined &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT))
            {
                ops.push_back(OP_BUFFER_COPY_TO_BUFFER);
                ops.push_back(OP_BUFFER_COPY_TO_IMAGE);
            }
        }

        if (state.hasBoundImageMemory &&
            (state.imageLayout == vk::VK_IMAGE_LAYOUT_UNDEFINED || getNumberOfSupportedLayouts(usage) > 1))
        {
            ops.push_back(OP_IMAGE_TRANSITION_LAYOUT);

            {
                if (usage & USAGE_TRANSFER_DST &&
                    (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
                     state.imageLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT))
                {
                    ops.push_back(OP_IMAGE_COPY_FROM_BUFFER);
                    ops.push_back(OP_IMAGE_COPY_FROM_IMAGE);

                    // Blits require VK_QUEUE_GRAPHICS_BIT
                    if (backend == BACKEND_GRAPHICS)
                        ops.push_back(OP_IMAGE_BLIT_FROM_IMAGE);
                }

                if (usage & USAGE_TRANSFER_SRC &&
                    (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
                     state.imageLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) &&
                    state.imageDefined &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT))
                {
                    ops.push_back(OP_IMAGE_COPY_TO_BUFFER);
                    ops.push_back(OP_IMAGE_COPY_TO_IMAGE);

                    // Blits require VK_QUEUE_GRAPHICS_BIT
                    if (backend == BACKEND_GRAPHICS)
                        ops.push_back(OP_IMAGE_BLIT_TO_IMAGE);
                }
            }
        }

        // \todo [2016-03-09 mika] Add other usages?
        if (backend == BACKEND_GRAPHICS &&
            ((state.memoryDefined && state.hasBoundBufferMemory &&
              (((usage & USAGE_VERTEX_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) ||
               ((usage & USAGE_INDEX_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, vk::VK_ACCESS_INDEX_READ_BIT)) ||
               ((usage & USAGE_UNIFORM_BUFFER) &&
                (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT) ||
                 state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT))) ||
               ((usage & USAGE_UNIFORM_TEXEL_BUFFER) &&
                (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT) ||
                 state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))) ||
               ((usage & USAGE_STORAGE_BUFFER) &&
                (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT) ||
                 state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))) ||
               ((usage & USAGE_STORAGE_TEXEL_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT)))) ||
             (state.imageDefined && state.hasBoundImageMemory &&
              (((usage & USAGE_STORAGE_IMAGE) && state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL &&
                (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT) ||
                 state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))) ||
               ((usage & USAGE_SAMPLED_IMAGE) &&
                (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
                 state.imageLayout == vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) &&
                (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT) ||
                 state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT)))))))
        {
            ops.push_back(OP_RENDERPASS_BEGIN);
        }

        if (backend == BACKEND_COMPUTE &&
            ((state.memoryDefined && state.hasBoundBufferMemory &&
              (((usage & USAGE_UNIFORM_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT)) ||
               ((usage & USAGE_UNIFORM_TEXEL_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT)) ||
               ((usage & USAGE_STORAGE_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT)) ||
               ((usage & USAGE_STORAGE_TEXEL_BUFFER) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT)))) ||
             (state.imageDefined && state.hasBoundImageMemory &&
              (((usage & USAGE_STORAGE_IMAGE) && state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT)) ||
               ((usage & USAGE_SAMPLED_IMAGE) &&
                (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
                 state.imageLayout == vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))))))
        {
            ops.push_back(OP_COMPUTEPASS_BEGIN);
        }

        ops.push_back(OP_SECONDARY_COMMAND_BUFFER_BEGIN);

        // \note This depends on previous operations and has to be always the
        // last command buffer operation check
        if (ops.empty() || !state.commandBufferIsEmpty)
            ops.push_back(OP_COMMAND_BUFFER_END);
    }
    else if (state.stage == STAGE_SECONDARY_COMMAND_BUFFER)
    {
        if (!state.cache.isClean())
        {
            ops.push_back(OP_PIPELINE_BARRIER_GLOBAL);

            if (state.hasImage && (state.imageLayout != vk::VK_IMAGE_LAYOUT_UNDEFINED))
                ops.push_back(OP_PIPELINE_BARRIER_IMAGE);

            if (state.hasBuffer)
                ops.push_back(OP_PIPELINE_BARRIER_BUFFER);
        }

        if (state.hasBoundBufferMemory)
        {
            if (usage & USAGE_TRANSFER_DST &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT))
            {
                ops.push_back(OP_BUFFER_FILL);
                ops.push_back(OP_BUFFER_UPDATE);
                ops.push_back(OP_BUFFER_COPY_FROM_BUFFER);
                ops.push_back(OP_BUFFER_COPY_FROM_IMAGE);
            }

            if (usage & USAGE_TRANSFER_SRC && state.memoryDefined &&
                state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT))
            {
                ops.push_back(OP_BUFFER_COPY_TO_BUFFER);
                ops.push_back(OP_BUFFER_COPY_TO_IMAGE);
            }
        }

        if (state.hasBoundImageMemory &&
            (state.imageLayout == vk::VK_IMAGE_LAYOUT_UNDEFINED || getNumberOfSupportedLayouts(usage) > 1))
        {
            ops.push_back(OP_IMAGE_TRANSITION_LAYOUT);

            {
                if (usage & USAGE_TRANSFER_DST &&
                    (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
                     state.imageLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT))
                {
                    ops.push_back(OP_IMAGE_COPY_FROM_BUFFER);
                    ops.push_back(OP_IMAGE_COPY_FROM_IMAGE);

                    // Blits require VK_QUEUE_GRAPHICS_BIT
                    if (backend == BACKEND_GRAPHICS)
                        ops.push_back(OP_IMAGE_BLIT_FROM_IMAGE);
                }

                if (usage & USAGE_TRANSFER_SRC &&
                    (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
                     state.imageLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) &&
                    state.imageDefined &&
                    state.cache.isValid(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT))
                {
                    ops.push_back(OP_IMAGE_COPY_TO_BUFFER);
                    ops.push_back(OP_IMAGE_COPY_TO_IMAGE);

                    // Blits require VK_QUEUE_GRAPHICS_BIT
                    if (backend == BACKEND_GRAPHICS)
                        ops.push_back(OP_IMAGE_BLIT_TO_IMAGE);
                }
            }
        }

        // \note This depends on previous operations and has to be always the
        // last command buffer operation check
        if (ops.empty() || !state.commandBufferIsEmpty)
            ops.push_back(OP_SECONDARY_COMMAND_BUFFER_END);
    }
    else if (state.stage == STAGE_RENDER_PASS)
    {
        if ((usage & USAGE_VERTEX_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
        {
            ops.push_back(OP_RENDER_VERTEX_BUFFER);
        }

        if ((usage & USAGE_INDEX_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, vk::VK_ACCESS_INDEX_READ_BIT))
        {
            ops.push_back(OP_RENDER_INDEX_BUFFER);
        }

        if ((usage & USAGE_UNIFORM_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory)
        {
            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT))
                ops.push_back(OP_RENDER_VERTEX_UNIFORM_BUFFER);

            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT))
                ops.push_back(OP_RENDER_FRAGMENT_UNIFORM_BUFFER);
        }

        if ((usage & USAGE_UNIFORM_TEXEL_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory)
        {
            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_VERTEX_UNIFORM_TEXEL_BUFFER);

            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_FRAGMENT_UNIFORM_TEXEL_BUFFER);
        }

        if ((usage & USAGE_STORAGE_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory)
        {
            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_VERTEX_STORAGE_BUFFER);

            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_FRAGMENT_STORAGE_BUFFER);
        }

        if ((usage & USAGE_STORAGE_TEXEL_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory)
        {
            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_VERTEX_STORAGE_TEXEL_BUFFER);

            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_FRAGMENT_STORAGE_TEXEL_BUFFER);
        }

        if ((usage & USAGE_STORAGE_IMAGE) != 0 && state.imageDefined && state.hasBoundImageMemory &&
            (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL))
        {
            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_VERTEX_STORAGE_IMAGE);

            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_FRAGMENT_STORAGE_IMAGE);
        }

        if ((usage & USAGE_SAMPLED_IMAGE) != 0 && state.imageDefined && state.hasBoundImageMemory &&
            (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
             state.imageLayout == vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        {
            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_VERTEX_SAMPLED_IMAGE);

            if (state.cache.isValid(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
                ops.push_back(OP_RENDER_FRAGMENT_SAMPLED_IMAGE);
        }

        if (!state.renderPassIsEmpty)
            ops.push_back(OP_RENDERPASS_END);
    }
    else if (state.stage == STAGE_COMPUTE_PASS)
    {
        if ((usage & USAGE_UNIFORM_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT))
        {
            ops.push_back(OP_COMPUTE_UNIFORM_BUFFER);
        }

        if ((usage & USAGE_UNIFORM_TEXEL_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
        {
            ops.push_back(OP_COMPUTE_UNIFORM_TEXEL_BUFFER);
        }

        if ((usage & USAGE_STORAGE_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
        {
            ops.push_back(OP_COMPUTE_STORAGE_BUFFER);
        }

        if ((usage & USAGE_STORAGE_TEXEL_BUFFER) != 0 && state.memoryDefined && state.hasBoundBufferMemory &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
        {
            ops.push_back(OP_COMPUTE_STORAGE_TEXEL_BUFFER);
        }

        if ((usage & USAGE_STORAGE_IMAGE) != 0 && state.imageDefined && state.hasBoundImageMemory &&
            (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL) &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
        {
            ops.push_back(OP_COMPUTE_STORAGE_IMAGE);
        }

        if ((usage & USAGE_SAMPLED_IMAGE) != 0 && state.imageDefined && state.hasBoundImageMemory &&
            (state.imageLayout == vk::VK_IMAGE_LAYOUT_GENERAL ||
             state.imageLayout == vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) &&
            state.cache.isValid(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT))
        {
            ops.push_back(OP_COMPUTE_SAMPLED_IMAGE);
        }

        if (!state.renderPassIsEmpty)
            ops.push_back(OP_COMPUTEPASS_END);
    }
    else
        DE_FATAL("Unknown stage");
}
void removeIllegalAccessFlags(vk::VkAccessFlags &accessflags, vk::VkPipelineStageFlags stageflags)
{
    if (!(stageflags & vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT))
        accessflags &= ~vk::VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT))
        accessflags &= ~vk::VK_ACCESS_INDEX_READ_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT))
        accessflags &= ~vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

    if (!(stageflags &
          (vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | vk::VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
           vk::VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | vk::VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
           vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)))
        accessflags &= ~vk::VK_ACCESS_UNIFORM_READ_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT))
        accessflags &= ~vk::VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

    if (!(stageflags &
          (vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | vk::VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
           vk::VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | vk::VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
           vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)))
        accessflags &= ~vk::VK_ACCESS_SHADER_READ_BIT;

    if (!(stageflags &
          (vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | vk::VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
           vk::VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | vk::VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
           vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)))
        accessflags &= ~vk::VK_ACCESS_SHADER_WRITE_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT))
        accessflags &= ~vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT))
        accessflags &= ~vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (!(stageflags &
          (vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)))
        accessflags &= ~vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    if (!(stageflags &
          (vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)))
        accessflags &= ~vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_TRANSFER_BIT))
        accessflags &= ~vk::VK_ACCESS_TRANSFER_READ_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_TRANSFER_BIT))
        accessflags &= ~vk::VK_ACCESS_TRANSFER_WRITE_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_HOST_BIT))
        accessflags &= ~vk::VK_ACCESS_HOST_READ_BIT;

    if (!(stageflags & vk::VK_PIPELINE_STAGE_HOST_BIT))
        accessflags &= ~vk::VK_ACCESS_HOST_WRITE_BIT;
}
void applyOp(State &state, const Memory &memory, Op op, Usage usage)
{
    switch (op)
    {
    case OP_MAP:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.mapped);
        state.mapped = true;
        break;

    case OP_UNMAP:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.mapped);
        state.mapped = false;
        break;

    case OP_MAP_FLUSH:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.hostFlushed);
        state.hostFlushed = true;
        break;

    case OP_MAP_INVALIDATE:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.hostInvalidated);
        state.hostInvalidated = true;
        break;

    case OP_MAP_READ:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.hostInvalidated);
        state.rng.getUint32();
        break;

    case OP_MAP_WRITE:
        DE_ASSERT(state.stage == STAGE_HOST);
        if ((memory.getMemoryType().propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            state.hostFlushed = false;

        state.memoryDefined = true;
        state.imageDefined  = false;
        state.imageLayout   = vk::VK_IMAGE_LAYOUT_UNDEFINED;
        state.rng.getUint32();
        break;

    case OP_MAP_MODIFY:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.hostInvalidated);

        if ((memory.getMemoryType().propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            state.hostFlushed = false;

        state.rng.getUint32();
        break;

    case OP_BUFFER_CREATE:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.hasBuffer);

        state.hasBuffer = true;
        break;

    case OP_BUFFER_DESTROY:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.hasBuffer);
        DE_ASSERT(state.hasBoundBufferMemory);

        state.hasBuffer            = false;
        state.hasBoundBufferMemory = false;
        break;

    case OP_BUFFER_BINDMEMORY:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.hasBuffer);
        DE_ASSERT(!state.hasBoundBufferMemory);

        state.hasBoundBufferMemory = true;
        break;

    case OP_IMAGE_CREATE:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.hasImage);
        DE_ASSERT(!state.hasBuffer);

        state.hasImage = true;
        break;

    case OP_IMAGE_DESTROY:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.hasImage);
        DE_ASSERT(state.hasBoundImageMemory);

        state.hasImage            = false;
        state.hasBoundImageMemory = false;
        state.imageLayout         = vk::VK_IMAGE_LAYOUT_UNDEFINED;
        state.imageDefined        = false;
        break;

    case OP_IMAGE_BINDMEMORY:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(state.hasImage);
        DE_ASSERT(!state.hasBoundImageMemory);

        state.hasBoundImageMemory = true;
        break;

    case OP_IMAGE_TRANSITION_LAYOUT:
    {
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);
        DE_ASSERT(state.hasImage);
        DE_ASSERT(state.hasBoundImageMemory);

        // \todo [2016-03-09 mika] Support linear tiling and predefined data
        const vk::VkImageLayout srcLayout =
            state.rng.getFloat() < 0.9f ? state.imageLayout : vk::VK_IMAGE_LAYOUT_UNDEFINED;
        const vk::VkImageLayout dstLayout = getRandomNextLayout(state.rng, usage, srcLayout);

        vk::VkPipelineStageFlags dirtySrcStages;
        vk::VkAccessFlags dirtySrcAccesses;
        vk::VkPipelineStageFlags dirtyDstStages;
        vk::VkAccessFlags dirtyDstAccesses;

        vk::VkPipelineStageFlags srcStages;
        vk::VkAccessFlags srcAccesses;
        vk::VkPipelineStageFlags dstStages;
        vk::VkAccessFlags dstAccesses;

        state.cache.getFullBarrier(dirtySrcStages, dirtySrcAccesses, dirtyDstStages, dirtyDstAccesses);

        // Try masking some random bits
        srcStages   = dirtySrcStages;
        srcAccesses = dirtySrcAccesses;

        dstStages   = state.cache.getAllowedStages() & state.rng.getUint32();
        dstAccesses = state.cache.getAllowedAcceses() & state.rng.getUint32();

        // If there are no bits in dst stage mask use all stages
        dstStages = dstStages ? dstStages : state.cache.getAllowedStages();

        if (!srcStages)
            srcStages = dstStages;

        removeIllegalAccessFlags(dstAccesses, dstStages);
        removeIllegalAccessFlags(srcAccesses, srcStages);

        if (srcLayout == vk::VK_IMAGE_LAYOUT_UNDEFINED)
            state.imageDefined = false;

        state.commandBufferIsEmpty = false;
        state.imageLayout          = dstLayout;
        state.memoryDefined        = false;
        state.cache.imageLayoutBarrier(srcStages, srcAccesses, dstStages, dstAccesses);
        break;
    }

    case OP_QUEUE_WAIT_FOR_IDLE:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.queueIdle);

        state.queueIdle = true;

        state.cache.waitForIdle();
        break;

    case OP_DEVICE_WAIT_FOR_IDLE:
        DE_ASSERT(state.stage == STAGE_HOST);
        DE_ASSERT(!state.deviceIdle);

        state.queueIdle  = true;
        state.deviceIdle = true;

        state.cache.waitForIdle();
        break;

    case OP_COMMAND_BUFFER_BEGIN:
        DE_ASSERT(state.stage == STAGE_HOST);
        state.stage                = STAGE_COMMAND_BUFFER;
        state.commandBufferIsEmpty = true;
        // Makes host writes visible to command buffer
        state.cache.submitCommandBuffer();
        break;

    case OP_COMMAND_BUFFER_END:
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);
        state.stage      = STAGE_HOST;
        state.queueIdle  = false;
        state.deviceIdle = false;
        break;

    case OP_SECONDARY_COMMAND_BUFFER_BEGIN:
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);
        state.stage                       = STAGE_SECONDARY_COMMAND_BUFFER;
        state.primaryCommandBufferIsEmpty = state.commandBufferIsEmpty;
        state.commandBufferIsEmpty        = true;
        break;

    case OP_SECONDARY_COMMAND_BUFFER_END:
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);
        state.stage                = STAGE_COMMAND_BUFFER;
        state.commandBufferIsEmpty = state.primaryCommandBufferIsEmpty;
        break;

    case OP_BUFFER_COPY_FROM_BUFFER:
    case OP_BUFFER_COPY_FROM_IMAGE:
    case OP_BUFFER_UPDATE:
    case OP_BUFFER_FILL:
        state.rng.getUint32();
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);

        if ((memory.getMemoryType().propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            state.hostInvalidated = false;

        state.commandBufferIsEmpty = false;
        state.memoryDefined        = true;
        state.imageDefined         = false;
        state.imageLayout          = vk::VK_IMAGE_LAYOUT_UNDEFINED;
        state.cache.perform(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT);
        break;

    case OP_BUFFER_COPY_TO_BUFFER:
    case OP_BUFFER_COPY_TO_IMAGE:
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);

        state.commandBufferIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT);
        break;

    case OP_IMAGE_BLIT_FROM_IMAGE:
        state.rng.getBool();
        // Fall through
    case OP_IMAGE_COPY_FROM_BUFFER:
    case OP_IMAGE_COPY_FROM_IMAGE:
        state.rng.getUint32();
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);

        if ((memory.getMemoryType().propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            state.hostInvalidated = false;

        state.commandBufferIsEmpty = false;
        state.memoryDefined        = false;
        state.imageDefined         = true;
        state.cache.perform(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT);
        break;

    case OP_IMAGE_BLIT_TO_IMAGE:
        state.rng.getBool();
        // Fall through
    case OP_IMAGE_COPY_TO_BUFFER:
    case OP_IMAGE_COPY_TO_IMAGE:
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);

        state.commandBufferIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT);
        break;

    case OP_PIPELINE_BARRIER_GLOBAL:
    case OP_PIPELINE_BARRIER_BUFFER:
    case OP_PIPELINE_BARRIER_IMAGE:
    {
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);

        vk::VkPipelineStageFlags dirtySrcStages;
        vk::VkAccessFlags dirtySrcAccesses;
        vk::VkPipelineStageFlags dirtyDstStages;
        vk::VkAccessFlags dirtyDstAccesses;

        vk::VkPipelineStageFlags srcStages;
        vk::VkAccessFlags srcAccesses;
        vk::VkPipelineStageFlags dstStages;
        vk::VkAccessFlags dstAccesses;

        state.cache.getFullBarrier(dirtySrcStages, dirtySrcAccesses, dirtyDstStages, dirtyDstAccesses);

        // Try masking some random bits
        srcStages   = dirtySrcStages & state.rng.getUint32();
        srcAccesses = dirtySrcAccesses & state.rng.getUint32();

        dstStages   = dirtyDstStages & state.rng.getUint32();
        dstAccesses = dirtyDstAccesses & state.rng.getUint32();

        // If there are no bits in stage mask use the original dirty stages
        srcStages = srcStages ? srcStages : dirtySrcStages;
        dstStages = dstStages ? dstStages : dirtyDstStages;

        if (!srcStages)
            srcStages = dstStages;

        removeIllegalAccessFlags(dstAccesses, dstStages);
        removeIllegalAccessFlags(srcAccesses, srcStages);

        state.commandBufferIsEmpty = false;
        state.cache.barrier(srcStages, srcAccesses, dstStages, dstAccesses);
        break;
    }

    case OP_RENDERPASS_BEGIN:
    {
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER);

        state.renderPassIsEmpty = true;
        state.stage             = STAGE_RENDER_PASS;
        break;
    }

    case OP_RENDERPASS_END:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = true;
        state.stage             = STAGE_COMMAND_BUFFER;
        break;
    }

    case OP_RENDER_VERTEX_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, vk::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
        break;
    }

    case OP_RENDER_INDEX_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, vk::VK_ACCESS_INDEX_READ_BIT);
        break;
    }

    case OP_RENDER_VERTEX_UNIFORM_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT);
        break;
    }

    case OP_RENDER_FRAGMENT_UNIFORM_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT);
        break;
    }

    case OP_RENDER_VERTEX_STORAGE_BUFFER:
    case OP_RENDER_VERTEX_STORAGE_TEXEL_BUFFER:
    case OP_RENDER_VERTEX_UNIFORM_TEXEL_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT);
        break;
    }

    case OP_RENDER_FRAGMENT_STORAGE_BUFFER:
    case OP_RENDER_FRAGMENT_STORAGE_TEXEL_BUFFER:
    case OP_RENDER_FRAGMENT_UNIFORM_TEXEL_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT);
        break;
    }

    case OP_RENDER_FRAGMENT_STORAGE_IMAGE:
    case OP_RENDER_FRAGMENT_SAMPLED_IMAGE:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT);
        break;
    }

    case OP_RENDER_VERTEX_STORAGE_IMAGE:
    case OP_RENDER_VERTEX_SAMPLED_IMAGE:
    {
        DE_ASSERT(state.stage == STAGE_RENDER_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT);
        break;
    }

    case OP_COMPUTEPASS_BEGIN:
    {
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER);

        state.renderPassIsEmpty = true;
        state.stage             = STAGE_COMPUTE_PASS;
        break;
    }

    case OP_COMPUTEPASS_END:
    {
        DE_ASSERT(state.stage == STAGE_COMPUTE_PASS);

        state.renderPassIsEmpty = true;
        state.stage             = STAGE_COMMAND_BUFFER;
        break;
    }

    case OP_COMPUTE_UNIFORM_BUFFER:
    {
        DE_ASSERT(state.stage == STAGE_COMPUTE_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_UNIFORM_READ_BIT);
        break;
    }

    case OP_COMPUTE_UNIFORM_TEXEL_BUFFER:
    case OP_COMPUTE_STORAGE_BUFFER:
    case OP_COMPUTE_STORAGE_TEXEL_BUFFER:
    case OP_COMPUTE_STORAGE_IMAGE:
    case OP_COMPUTE_SAMPLED_IMAGE:
    {
        DE_ASSERT(state.stage == STAGE_COMPUTE_PASS);

        state.renderPassIsEmpty = false;
        state.cache.perform(vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_ACCESS_SHADER_READ_BIT);
        break;
    }

    default:
        DE_FATAL("Unknown op");
    }
}
de::MovePtr<Command> createHostCommand(Op op, de::Random &rng, Usage usage, vk::VkSharingMode sharing)
{
    switch (op)
    {
    case OP_MAP:
        return de::MovePtr<Command>(new Map());
    case OP_UNMAP:
        return de::MovePtr<Command>(new UnMap());

    case OP_MAP_FLUSH:
        return de::MovePtr<Command>(new Flush());
    case OP_MAP_INVALIDATE:
        return de::MovePtr<Command>(new Invalidate());

    case OP_MAP_READ:
        return de::MovePtr<Command>(new HostMemoryAccess(true, false, rng.getUint32()));
    case OP_MAP_WRITE:
        return de::MovePtr<Command>(new HostMemoryAccess(false, true, rng.getUint32()));
    case OP_MAP_MODIFY:
        return de::MovePtr<Command>(new HostMemoryAccess(true, true, rng.getUint32()));

    case OP_BUFFER_CREATE:
        return de::MovePtr<Command>(new CreateBuffer(usageToBufferUsageFlags(usage), sharing));
    case OP_BUFFER_DESTROY:
        return de::MovePtr<Command>(new DestroyBuffer());
    case OP_BUFFER_BINDMEMORY:
        return de::MovePtr<Command>(new BindBufferMemory());

    case OP_IMAGE_CREATE:
        return de::MovePtr<Command>(new CreateImage(usageToImageUsageFlags(usage), sharing));
    case OP_IMAGE_DESTROY:
        return de::MovePtr<Command>(new DestroyImage());
    case OP_IMAGE_BINDMEMORY:
        return de::MovePtr<Command>(new BindImageMemory());

    case OP_QUEUE_WAIT_FOR_IDLE:
        return de::MovePtr<Command>(new QueueWaitIdle());
    case OP_DEVICE_WAIT_FOR_IDLE:
        return de::MovePtr<Command>(new DeviceWaitIdle());

    default:
        DE_FATAL("Unknown op");
        return de::MovePtr<Command>(nullptr);
    }
}

de::MovePtr<CmdCommand> createCmdCommand(de::Random &rng, const State &state, Op op, Usage usage)
{
    switch (op)
    {
    case OP_BUFFER_FILL:
        return de::MovePtr<CmdCommand>(new FillBuffer(rng.getUint32()));
    case OP_BUFFER_UPDATE:
        return de::MovePtr<CmdCommand>(new UpdateBuffer(rng.getUint32()));
    case OP_BUFFER_COPY_TO_BUFFER:
        return de::MovePtr<CmdCommand>(new BufferCopyToBuffer());
    case OP_BUFFER_COPY_FROM_BUFFER:
        return de::MovePtr<CmdCommand>(new BufferCopyFromBuffer(rng.getUint32()));

    case OP_BUFFER_COPY_TO_IMAGE:
        return de::MovePtr<CmdCommand>(new BufferCopyToImage());
    case OP_BUFFER_COPY_FROM_IMAGE:
        return de::MovePtr<CmdCommand>(new BufferCopyFromImage(rng.getUint32()));

    case OP_IMAGE_TRANSITION_LAYOUT:
    {
        DE_ASSERT(state.stage == STAGE_COMMAND_BUFFER || state.stage == STAGE_SECONDARY_COMMAND_BUFFER);
        DE_ASSERT(state.hasImage);
        DE_ASSERT(state.hasBoundImageMemory);

        const vk::VkImageLayout srcLayout = rng.getFloat() < 0.9f ? state.imageLayout : vk::VK_IMAGE_LAYOUT_UNDEFINED;
        const vk::VkImageLayout dstLayout = getRandomNextLayout(rng, usage, srcLayout);

        vk::VkPipelineStageFlags dirtySrcStages;
        vk::VkAccessFlags dirtySrcAccesses;
        vk::VkPipelineStageFlags dirtyDstStages;
        vk::VkAccessFlags dirtyDstAccesses;

        vk::VkPipelineStageFlags srcStages;
        vk::VkAccessFlags srcAccesses;
        vk::VkPipelineStageFlags dstStages;
        vk::VkAccessFlags dstAccesses;

        state.cache.getFullBarrier(dirtySrcStages, dirtySrcAccesses, dirtyDstStages, dirtyDstAccesses);

        // Try masking some random bits
        srcStages   = dirtySrcStages;
        srcAccesses = dirtySrcAccesses;

        dstStages   = state.cache.getAllowedStages() & rng.getUint32();
        dstAccesses = state.cache.getAllowedAcceses() & rng.getUint32();

        // If there are no bits in dst stage mask use all stages
        dstStages = dstStages ? dstStages : state.cache.getAllowedStages();

        if (!srcStages)
            srcStages = dstStages;

        removeIllegalAccessFlags(dstAccesses, dstStages);
        removeIllegalAccessFlags(srcAccesses, srcStages);

        return de::MovePtr<CmdCommand>(
            new ImageTransition(srcStages, srcAccesses, dstStages, dstAccesses, srcLayout, dstLayout));
    }

    case OP_IMAGE_COPY_TO_BUFFER:
        return de::MovePtr<CmdCommand>(new ImageCopyToBuffer(state.imageLayout));
    case OP_IMAGE_COPY_FROM_BUFFER:
        return de::MovePtr<CmdCommand>(new ImageCopyFromBuffer(rng.getUint32(), state.imageLayout));
    case OP_IMAGE_COPY_TO_IMAGE:
        return de::MovePtr<CmdCommand>(new ImageCopyToImage(state.imageLayout));
    case OP_IMAGE_COPY_FROM_IMAGE:
        return de::MovePtr<CmdCommand>(new ImageCopyFromImage(rng.getUint32(), state.imageLayout));
    case OP_IMAGE_BLIT_TO_IMAGE:
    {
        const BlitScale scale = rng.getBool() ? BLIT_SCALE_20 : BLIT_SCALE_10;
        return de::MovePtr<CmdCommand>(new ImageBlitToImage(scale, state.imageLayout));
    }

    case OP_IMAGE_BLIT_FROM_IMAGE:
    {
        const BlitScale scale = rng.getBool() ? BLIT_SCALE_20 : BLIT_SCALE_10;
        return de::MovePtr<CmdCommand>(new ImageBlitFromImage(rng.getUint32(), scale, state.imageLayout));
    }

    case OP_PIPELINE_BARRIER_GLOBAL:
    case OP_PIPELINE_BARRIER_BUFFER:
    case OP_PIPELINE_BARRIER_IMAGE:
    {
        vk::VkPipelineStageFlags dirtySrcStages;
        vk::VkAccessFlags dirtySrcAccesses;
        vk::VkPipelineStageFlags dirtyDstStages;
        vk::VkAccessFlags dirtyDstAccesses;

        vk::VkPipelineStageFlags srcStages;
        vk::VkAccessFlags srcAccesses;
        vk::VkPipelineStageFlags dstStages;
        vk::VkAccessFlags dstAccesses;

        state.cache.getFullBarrier(dirtySrcStages, dirtySrcAccesses, dirtyDstStages, dirtyDstAccesses);

        // Try masking some random bits
        srcStages   = dirtySrcStages & rng.getUint32();
        srcAccesses = dirtySrcAccesses & rng.getUint32();

        dstStages   = dirtyDstStages & rng.getUint32();
        dstAccesses = dirtyDstAccesses & rng.getUint32();

        // If there are no bits in stage mask use the original dirty stages
        srcStages = srcStages ? srcStages : dirtySrcStages;
        dstStages = dstStages ? dstStages : dirtyDstStages;

        if (!srcStages)
            srcStages = dstStages;

        removeIllegalAccessFlags(dstAccesses, dstStages);
        removeIllegalAccessFlags(srcAccesses, srcStages);

        PipelineBarrier::Type type;
        switch (op)
        {
        case OP_PIPELINE_BARRIER_IMAGE:
            type = PipelineBarrier::TYPE_IMAGE;
            break;
        case OP_PIPELINE_BARRIER_BUFFER:
            type = PipelineBarrier::TYPE_BUFFER;
            break;
        case OP_PIPELINE_BARRIER_GLOBAL:
            type = PipelineBarrier::TYPE_GLOBAL;
            break;
        default:
            type = PipelineBarrier::TYPE_LAST;
            DE_FATAL("Unknown op");
        }

        if (type == PipelineBarrier::TYPE_IMAGE)
            return de::MovePtr<CmdCommand>(new PipelineBarrier(srcStages, srcAccesses, dstStages, dstAccesses, type,
                                                               tcu::just(state.imageLayout)));
        else
            return de::MovePtr<CmdCommand>(
                new PipelineBarrier(srcStages, srcAccesses, dstStages, dstAccesses, type, tcu::Nothing));
    }

    default:
        DE_FATAL("Unknown op");
        return de::MovePtr<CmdCommand>(nullptr);
    }
}
de::MovePtr<CmdCommand> createSecondaryCmdCommands(const Memory &memory, de::Random &nextOpRng, State &state,
                                                   Usage usage, TestBackend backend, size_t &opNdx, size_t opCount)
{
    vector<CmdCommand *> commands;

    try
    {
        for (; opNdx < opCount; opNdx++)
        {
            vector<Op> ops;

            getAvailableOps(state, memory.getSupportBuffers(), memory.getSupportImages(), usage, backend, ops);

            DE_ASSERT(!ops.empty());

            {
                const Op op = nextOpRng.choose<Op>(ops.begin(), ops.end());

                if (op == OP_SECONDARY_COMMAND_BUFFER_END)
                {
                    break;
                }
                else
                {
                    de::Random rng(state.rng);

                    commands.push_back(createCmdCommand(rng, state, op, usage).release());
                    applyOp(state, memory, op, usage);

                    DE_ASSERT(state.rng == rng);
                }
            }
        }

        applyOp(state, memory, OP_SECONDARY_COMMAND_BUFFER_END, usage);
        return de::MovePtr<CmdCommand>(new ExecuteSecondaryCommandBuffer(commands));
    }
    catch (...)
    {
        for (size_t commandNdx = 0; commandNdx < commands.size(); commandNdx++)
            delete commands[commandNdx];

        throw;
    }
}

de::MovePtr<Command> createCmdCommands(const Memory &memory, de::Random &nextOpRng, State &state,
                                       const TestConfig &testConfig, size_t &opNdx, size_t opCount)
{
    vector<CmdCommand *> commands;

    try
    {
        // Insert a mostly-full barrier to order this work wrt previous command buffer.
        commands.push_back(new PipelineBarrier(state.cache.getAllowedStages(), state.cache.getAllowedAcceses(),
                                               state.cache.getAllowedStages(), state.cache.getAllowedAcceses(),
                                               PipelineBarrier::TYPE_GLOBAL, tcu::Nothing));

        for (; opNdx < opCount; opNdx++)
        {
            vector<Op> ops;

            getAvailableOps(state, memory.getSupportBuffers(), memory.getSupportImages(), testConfig.usage,
                            testConfig.backend, ops);

            DE_ASSERT(!ops.empty());

            {
                const Op op = nextOpRng.choose<Op>(ops.begin(), ops.end());

                if (op == OP_COMMAND_BUFFER_END)
                {
                    break;
                }
                else
                {
                    // \note Command needs to known the state before the operation
                    if (op == OP_RENDERPASS_BEGIN)
                    {
                        applyOp(state, memory, op, testConfig.usage);
                        commands.push_back(
                            createRenderPassCommands(memory, nextOpRng, state, testConfig, opNdx, opCount).release());
                    }
                    else if (op == OP_COMPUTEPASS_BEGIN)
                    {
                        applyOp(state, memory, op, testConfig.usage);
                        commands.push_back(
                            createComputeCommands(memory, nextOpRng, state, testConfig, opNdx, opCount).release());
                    }
                    else if (op == OP_SECONDARY_COMMAND_BUFFER_BEGIN)
                    {
                        applyOp(state, memory, op, testConfig.usage);
                        commands.push_back(createSecondaryCmdCommands(memory, nextOpRng, state, testConfig.usage,
                                                                      testConfig.backend, opNdx, opCount)
                                               .release());
                    }
                    else
                    {
                        de::Random rng(state.rng);

                        commands.push_back(createCmdCommand(rng, state, op, testConfig.usage).release());
                        applyOp(state, memory, op, testConfig.usage);

                        DE_ASSERT(state.rng == rng);
                    }
                }
            }
        }

        applyOp(state, memory, OP_COMMAND_BUFFER_END, testConfig.usage);
        return de::MovePtr<Command>(new SubmitCommandBuffer(commands));
    }
    catch (...)
    {
        for (size_t commandNdx = 0; commandNdx < commands.size(); commandNdx++)
            delete commands[commandNdx];

        throw;
    }
}

void createCommands(vector<Command *> &commands, uint32_t seed, const Memory &memory, const TestConfig &testConfig,
                    size_t opCount)
{
    State state(testConfig.usage, testConfig.backend, seed);
    // Used to select next operation only
    de::Random nextOpRng(seed ^ 12930809);

    commands.reserve(opCount);

    for (size_t opNdx = 0; opNdx < opCount; opNdx++)
    {
        vector<Op> ops;

        getAvailableOps(state, memory.getSupportBuffers(), memory.getSupportImages(), testConfig.usage,
                        testConfig.backend, ops);

        DE_ASSERT(!ops.empty());

        {
            const Op op = nextOpRng.choose<Op>(ops.begin(), ops.end());

            if (op == OP_COMMAND_BUFFER_BEGIN)
            {
                applyOp(state, memory, op, testConfig.usage);
                commands.push_back(createCmdCommands(memory, nextOpRng, state, testConfig, opNdx, opCount).release());
            }
            else
            {
                de::Random rng(state.rng);

                commands.push_back(createHostCommand(op, rng, testConfig.usage, testConfig.sharing).release());
                applyOp(state, memory, op, testConfig.usage);

                // Make sure that random generator is in sync
                DE_ASSERT(state.rng == rng);
            }
        }
    }

    // Clean up resources
    if (state.hasBuffer && state.hasImage)
    {
        if (!state.queueIdle)
            commands.push_back(new QueueWaitIdle());

        if (state.hasBuffer)
            commands.push_back(new DestroyBuffer());

        if (state.hasImage)
            commands.push_back(new DestroyImage());
    }
}
void MemoryTestInstance::resetResources(void)
{
    const vk::DeviceInterface &vkd = m_context.getDeviceInterface();
    const vk::VkDevice device      = m_context.getDevice();

    VK_CHECK(vkd.deviceWaitIdle(device));

    for (size_t commandNdx = 0; commandNdx < m_commands.size(); commandNdx++)
    {
        delete m_commands[commandNdx];
        m_commands[commandNdx] = nullptr;
    }

    m_commands.clear();
    m_prepareContext.clear();
    m_memory.clear();
}

bool MemoryTestInstance::nextIteration(void)
{
    m_iteration++;

    if (m_iteration < m_iterationCount)
    {
        resetResources();
        m_stage = &MemoryTestInstance::createCommandsAndAllocateMemory;
        return true;
    }
    else
        return nextMemoryType();
}

bool MemoryTestInstance::nextMemoryType(void)
{
    resetResources();

    DE_ASSERT(m_commands.empty());

    m_memoryTypeNdx++;

    if (m_memoryTypeNdx < m_memoryProperties.memoryTypeCount)
    {
        m_iteration = 0;
        m_stage     = &MemoryTestInstance::createCommandsAndAllocateMemory;

        return true;
    }
    else
    {
        m_stage = nullptr;
        return false;
    }
}

const char *backendToName(TestBackend backend)
{
    switch (backend)
    {
    case BACKEND_GRAPHICS:
        return "graphics";
    case BACKEND_COMPUTE:
        return "compute";
    case BACKEND_TRANSFER:
        return "transfer";
    default:
        DE_FATAL("Unknown backend");
        return "";
    }
}

uint32_t getBackendQueueFamilyIndex(const ::vkt::Context &context, TestBackend backend)
{
    switch (backend)
    {
    case BACKEND_COMPUTE:
        return (uint32_t)context.getComputeQueueFamilyIndex();
    case BACKEND_TRANSFER:
        return (uint32_t)context.getTransferQueueFamilyIndex();
    default:
        return context.getUniversalQueueFamilyIndex();
    }
}

vk::VkQueue getBackendQueue(const ::vkt::Context &context, TestBackend backend)
{
    switch (backend)
    {
    case BACKEND_COMPUTE:
        return context.getComputeQueue();
    case BACKEND_TRANSFER:
        return context.getTransferQueue();
    default:
        return context.getUniversalQueue();
    }
}

MemoryTestInstance::MemoryTestInstance(::vkt::Context &context, const TestConfig &config)
    : TestInstance(context)
    , m_config(config)
    , m_iterationCount(5)
    , m_opCount(50)
    , m_memoryProperties(
          vk::getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
    , m_memoryTypeNdx(0)
    , m_iteration(0)
    , m_stage(&MemoryTestInstance::createCommandsAndAllocateMemory)
    , m_resultCollector(context.getTestContext().getLog())

    , m_memory(nullptr)
{
    TestLog &log = context.getTestContext().getLog();
    {
        const tcu::ScopedLogSection section(log, "TestCaseInfo", "Test Case Info");

        log << TestLog::Message << "Buffer size: " << config.size << TestLog::EndMessage;
        log << TestLog::Message << "Sharing: " << config.sharing << TestLog::EndMessage;
        log << TestLog::Message << "Access: " << config.usage << TestLog::EndMessage;
        log << TestLog::Message << "Backend: " << backendToName(config.backend) << TestLog::EndMessage;
    }

    {
        const tcu::ScopedLogSection section(log, "MemoryProperties", "Memory Properties");

        for (uint32_t heapNdx = 0; heapNdx < m_memoryProperties.memoryHeapCount; heapNdx++)
        {
            const tcu::ScopedLogSection heapSection(log, "Heap" + de::toString(heapNdx),
                                                    "Heap " + de::toString(heapNdx));

            log << TestLog::Message << "Size: " << m_memoryProperties.memoryHeaps[heapNdx].size << TestLog::EndMessage;
            log << TestLog::Message << "Flags: " << m_memoryProperties.memoryHeaps[heapNdx].flags
                << TestLog::EndMessage;
        }

        for (uint32_t memoryTypeNdx = 0; memoryTypeNdx < m_memoryProperties.memoryTypeCount; memoryTypeNdx++)
        {
            const tcu::ScopedLogSection memoryTypeSection(log, "MemoryType" + de::toString(memoryTypeNdx),
                                                          "Memory type " + de::toString(memoryTypeNdx));

            log << TestLog::Message << "Properties: " << m_memoryProperties.memoryTypes[memoryTypeNdx].propertyFlags
                << TestLog::EndMessage;
            log << TestLog::Message << "Heap: " << m_memoryProperties.memoryTypes[memoryTypeNdx].heapIndex
                << TestLog::EndMessage;
        }
    }

    {
        const vk::InstanceInterface &vki          = context.getInstanceInterface();
        const vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
        const vk::DeviceInterface &vkd            = context.getDeviceInterface();
        const vk::VkDevice device                 = context.getDevice();
        const vk::VkQueue queue                   = getBackendQueue(context, config.backend);
        const uint32_t queueFamilyIndex           = getBackendQueueFamilyIndex(context, config.backend);
        vector<pair<uint32_t, vk::VkQueue>> queues;

        queues.push_back(std::make_pair(queueFamilyIndex, queue));

        m_renderContext = MovePtr<Context>(new Context(vki, vkd, physicalDevice, device, queue, queueFamilyIndex,
                                                       queues, context.getBinaryCollection()));
    }
}

MemoryTestInstance::~MemoryTestInstance(void)
{
    resetResources();
}

bool MemoryTestInstance::createCommandsAndAllocateMemory(void)
{
    const vk::VkDevice device                 = m_context.getDevice();
    TestLog &log                              = m_context.getTestContext().getLog();
    const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::DeviceInterface &vkd            = m_context.getDeviceInterface();
    const vk::VkPhysicalDeviceMemoryProperties memoryProperties =
        vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    const tcu::ScopedLogSection section(
        log, "MemoryType" + de::toString(m_memoryTypeNdx) + "CreateCommands" + de::toString(m_iteration),
        "Memory type " + de::toString(m_memoryTypeNdx) + " create commands iteration " + de::toString(m_iteration));
    const vector<uint32_t> &queues = m_renderContext->getQueueFamilies();

    DE_ASSERT(m_commands.empty());

    if (m_config.usage & (USAGE_HOST_READ | USAGE_HOST_WRITE) &&
        !(memoryProperties.memoryTypes[m_memoryTypeNdx].propertyFlags & vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
    {
        log << TestLog::Message << "Memory type not supported" << TestLog::EndMessage;

        return nextMemoryType();
    }
    else if (memoryProperties.memoryTypes[m_memoryTypeNdx].propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT)
    {
        log << TestLog::Message << "Memory type not supported (protected)" << TestLog::EndMessage;

        return nextMemoryType();
    }
#ifndef CTS_USES_VULKANSC
    // VUID-vkAllocateMemory-deviceCoherentMemory-02790
    else if ((memoryProperties.memoryTypes[m_memoryTypeNdx].propertyFlags &
              vk::VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) &&
             !m_context.getCoherentMemoryFeaturesAMD().deviceCoherentMemory)
    {
        log << TestLog::Message << "Memory type not supported (AMD device coherent feature not enabled)"
            << TestLog::EndMessage;

        return nextMemoryType();
    }
#endif // CTS_USES_VULKANSC
    else
    {
        try
        {
            const vk::VkBufferUsageFlags bufferUsage = usageToBufferUsageFlags(m_config.usage);
            const vk::VkImageUsageFlags imageUsage   = usageToImageUsageFlags(m_config.usage);
            const vk::VkDeviceSize maxBufferSize =
                bufferUsage != 0 ? roundBufferSizeToWxHx4(findMaxBufferSize(vkd, device, bufferUsage, m_config.sharing,
                                                                            queues, m_config.size, m_memoryTypeNdx)) :
                                   0;
            const IVec2 maxImageSize = imageUsage != 0 ?
                                           findMaxRGBA8ImageSize(vkd, device, imageUsage, m_config.sharing, queues,
                                                                 m_config.size, m_memoryTypeNdx) :
                                           IVec2(0, 0);

            log << TestLog::Message << "Max buffer size: " << maxBufferSize << TestLog::EndMessage;
            log << TestLog::Message << "Max RGBA8 image size: " << maxImageSize << TestLog::EndMessage;

            // Skip tests if there are no supported operations
            if (maxBufferSize == 0 && maxImageSize[0] == 0 &&
                (m_config.usage & (USAGE_HOST_READ | USAGE_HOST_WRITE)) == 0)
            {
                log << TestLog::Message << "Skipping memory type. None of the usages are supported."
                    << TestLog::EndMessage;

                return nextMemoryType();
            }
            else
            {
                const uint32_t seed =
                    2830980989u ^
                    deUint32Hash((uint32_t)(m_iteration)*m_memoryProperties.memoryTypeCount + m_memoryTypeNdx);

                m_memory = MovePtr<Memory>(new Memory(vki, vkd, physicalDevice, device, m_config.size, m_memoryTypeNdx,
                                                      maxBufferSize, maxImageSize[0], maxImageSize[1]));

                log << TestLog::Message << "Create commands" << TestLog::EndMessage;
                createCommands(m_commands, seed, *m_memory, m_config, m_opCount);

                m_stage = &MemoryTestInstance::prepare;
                return true;
            }
        }
        catch (const tcu::TestError &e)
        {
            m_resultCollector.fail("Failed, got exception: " + string(e.getMessage()));
            return nextMemoryType();
        }
    }
}

void MemoryTestInstance::logCommandTrace(TestLog &log, size_t prepareCount, size_t executeCount) const
{
    const tcu::ScopedLogSection section(log, "FailedCommandTrace", "Command trace");

    for (size_t cmdNdx = 0; cmdNdx < prepareCount; cmdNdx++)
        m_commands[cmdNdx]->logPrepare(log, cmdNdx);

    for (size_t cmdNdx = 0; cmdNdx < executeCount; cmdNdx++)
        m_commands[cmdNdx]->logExecute(log, cmdNdx);
}

void MemoryTestInstance::logFailureTrace(TestLog &log, size_t failingCmdNdx, const string &failureMessage) const
{
    const tcu::ScopedLogSection section(
        log, "FailedCommandTrace",
        "Full command and barrier trace in Vulkan submission order. The sub-command that first "
        "produced a verification mismatch is annotated as \"Failure origin\".");

    // Preparation phase (resource allocation, seeds) for all commands.
    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
        m_commands[cmdNdx]->logPrepare(log, cmdNdx);

    // Submission phase for all commands, with barriers, marking the exact failure.
    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        const bool marked = m_commands[cmdNdx]->logExecuteFailureTrace(log, cmdNdx, failureMessage);

        // Fallback for a failing top-level command that isn't a container able to
        // pinpoint the sub-command (e.g. a host access command).
        if (!marked && cmdNdx == failingCmdNdx)
            log << TestLog::Message << "Failure origin: " << failureMessage << TestLog::EndMessage;
    }
}

bool MemoryTestInstance::prepare(void)
{
    TestLog &log = m_context.getTestContext().getLog();
    const tcu::ScopedLogSection section(
        log, "MemoryType" + de::toString(m_memoryTypeNdx) + "Prepare" + de::toString(m_iteration),
        "Memory type " + de::toString(m_memoryTypeNdx) + " prepare iteration " + de::toString(m_iteration));

    m_prepareContext = MovePtr<PrepareContext>(new PrepareContext(*m_renderContext, *m_memory));

    DE_ASSERT(!m_commands.empty());

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        Command &command = *m_commands[cmdNdx];

        try
        {
            command.prepare(*m_prepareContext);
        }
        catch (const tcu::TestError &e)
        {
            logCommandTrace(log, cmdNdx + 1, 0);
            m_resultCollector.fail(de::toString(cmdNdx) + ":" + command.getName() +
                                   " failed to prepare, got exception: " + string(e.getMessage()));
            return nextMemoryType();
        }
    }

    m_stage = &MemoryTestInstance::execute;
    return true;
}

bool MemoryTestInstance::execute(void)
{
    TestLog &log = m_context.getTestContext().getLog();
    const tcu::ScopedLogSection section(
        log, "MemoryType" + de::toString(m_memoryTypeNdx) + "Execute" + de::toString(m_iteration),
        "Memory type " + de::toString(m_memoryTypeNdx) + " execute iteration " + de::toString(m_iteration));
    ExecuteContext executeContext(*m_renderContext);
    const vk::VkDevice device      = m_context.getDevice();
    const vk::DeviceInterface &vkd = m_context.getDeviceInterface();

    DE_ASSERT(!m_commands.empty());

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        Command &command = *m_commands[cmdNdx];

        try
        {
            command.execute(executeContext);
        }
        catch (const tcu::TestError &e)
        {
            logCommandTrace(log, m_commands.size(), cmdNdx + 1);
            m_resultCollector.fail(de::toString(cmdNdx) + ":" + command.getName() +
                                   " failed to execute, got exception: " + string(e.getMessage()));
            return nextIteration();
        }
    }

    VK_CHECK(vkd.deviceWaitIdle(device));

    m_stage = &MemoryTestInstance::verify;
    return true;
}

bool MemoryTestInstance::verify(void)
{
    DE_ASSERT(!m_commands.empty());

    TestLog &log = m_context.getTestContext().getLog();
    const tcu::ScopedLogSection section(
        log, "MemoryType" + de::toString(m_memoryTypeNdx) + "Verify" + de::toString(m_iteration),
        "Memory type " + de::toString(m_memoryTypeNdx) + " verify iteration " + de::toString(m_iteration));
    VerifyContext verifyContext(log, m_resultCollector, *m_renderContext, m_config.size);

    log << TestLog::Message << "Begin verify" << TestLog::EndMessage;

    for (size_t cmdNdx = 0; cmdNdx < m_commands.size(); cmdNdx++)
    {
        Command &command        = *m_commands[cmdNdx];
        const bool failedBefore = m_resultCollector.getResult() != QP_TEST_RESULT_PASS;

        try
        {
            command.verify(verifyContext, cmdNdx);
        }
        catch (const tcu::TestError &e)
        {
            logCommandTrace(log, m_commands.size(), m_commands.size());
            m_resultCollector.fail(de::toString(cmdNdx) + ":" + command.getName() +
                                   " failed to verify, got exception: " + string(e.getMessage()));
            return nextIteration();
        }

        // command.verify() above may record failures via the result collector without throwing
        // (e.g. a data mismatch inside a nested SubmitCommandBuffer, whose own sub-command
        // indices restart at 0 and are otherwise impossible to place in the overall sequence).
        if (!failedBefore && m_resultCollector.getResult() != QP_TEST_RESULT_PASS)
        {
            log << TestLog::Message << "First failure introduced by top-level command " << cmdNdx << ":"
                << command.getName() << TestLog::EndMessage;
            logFailureTrace(log, cmdNdx, m_resultCollector.getMessage());
        }
    }

    return nextIteration();
}

tcu::TestStatus MemoryTestInstance::iterate(void)
{
    if ((this->*m_stage)())
        return tcu::TestStatus::incomplete();
    else
        return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

} // namespace pipelinebarrier
} // namespace memory
} // namespace vkt
