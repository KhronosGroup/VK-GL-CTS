/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
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
 * \brief Tests for unreferenced invalid descriptors
 *//*--------------------------------------------------------------------*/

#include "vktBindingUnusedInvalidDescriptorTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuImageCompare.hpp"

#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

using namespace vk;

namespace vkt
{
namespace BindingModel
{
namespace
{

typedef de::MovePtr<vk::Allocation> AllocationMp;

constexpr VkFormat kImageFormat   = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkFormat kInvalidFormat = VK_FORMAT_R32_UINT;
constexpr VkExtent3D kExtent      = {32u, 32u, 1u};

enum ResourceType
{
    UNIFORM_BUFFER = 0,
    STORAGE_BUFFER,
    SAMPLED_IMAGE,
    COMBINED_IMAGE_SAMPLER,
    STORAGE_IMAGE,
    _MAX_ENUM,
};

struct TestParams
{
    ResourceType type;
    bool addInvalidDescriptor;
};

static bool requireImage(ResourceType type)
{
    static const bool translateTable[_MAX_ENUM] = {
        false, // UNIFORM_BUFFER
        false, // STORAGE_BUFFER
        true,  // SAMPLED_IMAGE
        true,  // COMBINED_IMAGE_SAMPLER
        true,  // STORAGE_IMAGE
    };

    return translateTable[type];
}

static bool requireBuffer(ResourceType type)
{
    static const bool translateTable[_MAX_ENUM] = {
        true,  // UNIFORM_BUFFER
        true,  // STORAGE_BUFFER
        false, // SAMPLED_IMAGE
        false, // COMBINED_IMAGE_SAMPLER
        false, // STORAGE_IMAGE
    };

    return translateTable[type];
}

static bool requireSampler(ResourceType type)
{
    static const bool translateTable[_MAX_ENUM] = {
        false, // UNIFORM_BUFFER
        false, // STORAGE_BUFFER
        false, // SAMPLED_IMAGE
        true,  // COMBINED_IMAGE_SAMPLER
        false, // STORAGE_IMAGE
    };

    return translateTable[type];
}

static VkImageUsageFlags getVkImageUsage(ResourceType type)
{
    static const VkImageUsageFlags translateTable[_MAX_ENUM] = {
        0u,                         // UNIFORM_BUFFER
        0u,                         // STORAGE_BUFFER
        VK_IMAGE_USAGE_SAMPLED_BIT, // SAMPLED_IMAGE
        VK_IMAGE_USAGE_SAMPLED_BIT, // COMBINED_IMAGE_SAMPLER
        VK_IMAGE_USAGE_STORAGE_BIT, // STORAGE_IMAGE
    };

    return translateTable[type];
}

static VkBufferUsageFlags getVkBufferUsage(ResourceType type)
{
    static const VkBufferUsageFlags translateTable[_MAX_ENUM] = {
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, // UNIFORM_BUFFER
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // STORAGE_BUFFER
        0u,                                 // SAMPLED_IMAGE
        0u,                                 // COMBINED_IMAGE_SAMPLER
        0u,                                 // STORAGE_IMAGE
    };

    return translateTable[type];
}

static VkDescriptorType getVkDescriptorType(ResourceType type)
{
    static const VkDescriptorType translateTable[_MAX_ENUM] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         // UNIFORM_BUFFER
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         // STORAGE_BUFFER
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          // SAMPLED_IMAGE
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // COMBINED_IMAGE_SAMPLER
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          // STORAGE_IMAGE
    };

    return translateTable[type];
}

VkImageCreateInfo makeComputeRenderTargetCI()
{
    return {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        kImageFormat,                        // VkFormat format;
        kExtent,                             // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
}

static VkImageCreateInfo makeImageCI(ResourceType type, bool invalid)
{

    VkImageUsageFlags usage           = VK_IMAGE_USAGE_TRANSFER_DST_BIT | getVkImageUsage(type);
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkFormat format                   = kImageFormat;

    if (invalid)
    {
        if (type == STORAGE_IMAGE) // For storage image we use invalid format
        {
            format = kInvalidFormat;
        }
        else // For sampled we use 2x MSAA
        {
            sampleCount = VK_SAMPLE_COUNT_2_BIT;
        }
    }

    VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        kExtent,                             // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        sampleCount,                         // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    return imageCreateInfo;
}

static VkImageViewCreateInfo makeImageViewCI(VkImage image, VkFormat format,
                                             const VkImageSubresourceRange &subresourceRange)
{
    return {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkImageViewCreateFlags flags;
        image,                                    // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        format,                                   // VkFormat format;
        {},                                       // VkComponentMapping components;
        subresourceRange                          // VkImageSubresourceRange subresourceRange;
    };
}

static const char *toString(ResourceType type)
{
    static const char *translateTable[_MAX_ENUM] = {
        "uniform_buffer",         // UNIFORM_BUFFER
        "storage_buffer",         // STORAGE_BUFFER
        "sampled_image",          // SAMPLED_IMAGE
        "combined_image_sampler", // COMBINED_IMAGE_SAMPLER
        "storage_image",          // STORAGE_IMAGE
    };

    return translateTable[type];
}

std::string getResourceDeclaration(ResourceType type)
{
    std::ostringstream oss;
    oss << "layout(set = 0, binding = 1";

    switch (type)
    {
    case UNIFORM_BUFFER:
    {
        oss << ") uniform UniformBuffer";
        oss << "{\n";
        oss << "\tvec4 data;\n";
        oss << "} u_buffer[2];\n";
        break;
    }
    case STORAGE_BUFFER:
    {
        oss << ") buffer StorageBuffer";
        oss << "{\n";
        oss << "\tvec4 data;\n";
        oss << "} u_buffer[2];\n";

        break;
    }
    case SAMPLED_IMAGE:
    {
        oss << ") uniform texture2D u_textures[2];\n";
        break;
    }
    case COMBINED_IMAGE_SAMPLER:
    {
        oss << ") uniform sampler2D u_textures[2];\n";
        break;
    }
    case STORAGE_IMAGE:
    {
        oss << ", rgba32f) uniform image2D u_textures[2];\n";
        break;
    }
    default:
    {
        DE_ASSERT(false);
        DE_FATAL("Invalid resource type");
        break;
    }
    }

    return oss.str();
}

std::string getResourceAccess(ResourceType type, uint32_t ndx)
{
    std::ostringstream oss;
    oss << "\tvec4 color" << ndx << " = ";

    switch (type)
    {
    case UNIFORM_BUFFER:
    {
        oss << "u_buffer[ndx + " << ndx << "].data;\n";
        break;
    }
    case STORAGE_BUFFER:
    {
        oss << "u_buffer[ndx + " << ndx << "].data;\n";
        break;
    }
    case SAMPLED_IMAGE:
    {
        oss << "texture(sampler2D(u_textures[ndx + " << ndx
            << "], u_sampler), ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y));\n";
        break;
    }
    case COMBINED_IMAGE_SAMPLER:
    {
        oss << "texture(u_textures[ndx + " << ndx << "], ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y));\n";
        break;
    }
    case STORAGE_IMAGE:
    {
        oss << "imageLoad(u_textures[ndx + " << ndx << "], ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y));\n";
        break;
    }
    default:
    {
        DE_ASSERT(false);
        DE_FATAL("Invalid resource type");
        break;
    }
    }

    return oss.str();
}

class Resource
{
public:
    Resource(ResourceType type, const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
             bool invalid = false)
        : m_type(type)
        , m_allocation()
        , m_image()
        , m_imageView()
        , m_sampler()
        , m_imageInfo()
        , m_buffer()
        , m_bufferInfo()
        , m_invalid(invalid)
    {
        // Create resource and resource view of tested type
        if (requireImage(m_type))
        {
            const VkImageCreateInfo imageCreateInfo = makeImageCI(m_type, m_invalid);
            VK_CHECK(vk.createImage(device, &imageCreateInfo, nullptr, &m_image));
            const VkMemoryRequirements requirements = getImageMemoryRequirements(vk, device, m_image);
            m_allocation                            = allocator.allocate(requirements, MemoryRequirement::Any);

            VK_CHECK(vk.bindImageMemory(device, m_image, m_allocation->getMemory(), m_allocation->getOffset()));

            VkFormat format = kImageFormat;
            if (m_invalid && m_type == STORAGE_IMAGE) // For storage image we use invalid format
            {
                format = kInvalidFormat;
            }

            VkImageViewCreateInfo imgViewCI =
                makeImageViewCI(m_image, format, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
            VK_CHECK(vk.createImageView(device, &imgViewCI, nullptr, &m_imageView));

            if (requireSampler(m_type))
            {
                const VkSamplerCreateInfo samplerCreateInfo = {
                    VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType        sType
                    nullptr,                                 // const void*            pNext
                    0u,                                      // VkSamplerCreateFlags   flags
                    VK_FILTER_NEAREST,                       // VkFilter               magFilter
                    VK_FILTER_NEAREST,                       // VkFilter               minFilter
                    VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode    mipmapMode
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeU
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeV
                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeW
                    0.0f,                                    // float                  mipLodBias
                    false,                                   // VkBool32               anisotropyEnable
                    1.0f,                                    // float                  maxAnisotropy
                    false,                                   // VkBool32               compareEnable
                    VK_COMPARE_OP_ALWAYS,                    // VkCompareOp            compareOp
                    0.0f,                                    // float                  minLod
                    0.0f,                                    // float                  maxLod
                    VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor          borderColor
                    false                                    // VkBool32               unnormalizedCoordinates
                };

                VK_CHECK(vk.createSampler(device, &samplerCreateInfo, nullptr, &m_sampler));
            }
            else
            {
                m_sampler = VK_NULL_HANDLE;
            }

            m_imageInfo.imageView   = m_imageView;
            m_imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            m_imageInfo.sampler     = m_sampler;
        }
        else // reqireBuffer
        {
            VkBufferUsageFlags usageFlags = getVkBufferUsage(m_type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            const VkBufferCreateInfo bufferCreateInfo = {
                VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
                nullptr,                              // const void*            pNext
                0,                                    // VkBufferCreateFlags    flags
                4 * sizeof(float),                    // VkDeviceSize           size
                usageFlags,                           // VkBufferUsageFlags     usage
                VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode          sharingMode
                0,                                    // uint32_t               queueFamilyIndexCount
                nullptr,                              // const uint32_t*        pQueueFamilyIndices
            };

            VK_CHECK(vk.createBuffer(device, &bufferCreateInfo, nullptr, &m_buffer));
            const VkMemoryRequirements requirements = getBufferMemoryRequirements(vk, device, m_buffer);
            m_allocation                            = allocator.allocate(requirements, MemoryRequirement::HostVisible);

            VK_CHECK(vk.bindBufferMemory(device, m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));

            m_bufferInfo.buffer = m_buffer;
            m_bufferInfo.offset = 0;
            m_bufferInfo.range  = VK_WHOLE_SIZE;
        }
    }

    Resource(Resource &&other) noexcept
        : m_type(other.m_type)
        , m_allocation(std::move(other.m_allocation))
        , m_image(std::move(other.m_image))
        , m_imageView(std::move(other.m_imageView))
        , m_sampler(std::move(other.m_sampler))
        , m_imageInfo(other.m_imageInfo)
        , m_buffer(std::move(other.m_buffer))
        , m_bufferInfo(other.m_bufferInfo)
        , m_invalid(other.m_invalid)
    {
        other.m_imageInfo  = VkDescriptorImageInfo{};
        other.m_bufferInfo = VkDescriptorBufferInfo{};
    }

    VkDescriptorType getDescriptorType() const
    {
        return getVkDescriptorType(m_type);
    }

    VkImage getImage()
    {
        DE_ASSERT(requireImage(m_type));
        return m_image;
    }

    VkImageView getImageView()
    {
        DE_ASSERT(requireImage(m_type));
        return m_imageView;
    }

    const VkDescriptorImageInfo &getImageInfo()
    {
        DE_ASSERT(requireImage(m_type));
        return m_imageInfo;
    }

    const VkDescriptorBufferInfo &getBufferInfo()
    {
        DE_ASSERT(requireBuffer(m_type));
        return m_bufferInfo;
    }

    void update(const DeviceInterface &vk, const VkDevice device, const QueueData &queueData)
    {
        DE_ASSERT(queueData.handle != VK_NULL_HANDLE);

        if (requireImage(m_type))
        {
            // Create command pool
            Move<VkCommandPool> cmdPool =
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueData.familyIndex);

            // Create command buffer
            Move<VkCommandBuffer> cmdBuffer =
                allocateCommandBuffer(vk, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            beginCommandBuffer(vk, cmdBuffer.get());

            const VkImageSubresourceRange colorSubresourceRange =
                makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
            VkClearColorValue clearColorValue = {{0.5f, 0.5f, 0.5f, 0.5f}};

            // preClear barrier
            VkImageMemoryBarrier preClearImgBarrier = {

                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
                nullptr,                                // const void*             pNext
                VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
                VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
                m_image,                                // VkImage                 image
                colorSubresourceRange                   // VkImageSubresourceRange subresourceRange
            };
            vk.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0u, 0u, nullptr, 0u, nullptr, 1u, &preClearImgBarrier);
            vk.cmdClearColorImage(cmdBuffer.get(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1u,
                                  &colorSubresourceRange);

            // postClear barrier
            VkImageMemoryBarrier postClearImgBarrier = {

                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
                nullptr,                                // const void*             pNext
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           srcAccessMask
                VK_ACCESS_SHADER_READ_BIT,              // VkAccessFlags           dstAccessMask
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           oldLayout
                VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout           newLayout
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
                m_image,                                // VkImage                 image
                colorSubresourceRange                   // VkImageSubresourceRange subresourceRange
            };
            vk.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  0u, 0u, nullptr, 0u, nullptr, 1u, &postClearImgBarrier);

            endCommandBuffer(vk, cmdBuffer.get());
            submitCommandsAndWait(vk, device, queueData.handle, cmdBuffer.get());
        }
        else // reqireBuffer
        {
            tcu::Vec4 color(0.5f, 0.5f, 0.5f, 0.5f);

            deMemcpy(m_allocation->getHostPtr(), &color, sizeof(color));

            invalidateAlloc(vk, device, *m_allocation.get());
        }
    }

    void DestroyInternals(const DeviceInterface &vk, const VkDevice device)
    {
        if (requireImage(m_type))
        {
            if (m_sampler != VK_NULL_HANDLE)
                vk.destroySampler(device, m_sampler, nullptr);
            if (m_imageView != VK_NULL_HANDLE)
                vk.destroyImageView(device, m_imageView, nullptr);
            if (m_image != VK_NULL_HANDLE)
                vk.destroyImage(device, m_image, nullptr);
        }
        else // reqireBuffer
        {
            if (m_buffer != VK_NULL_HANDLE)
                vk.destroyBuffer(device, m_buffer, nullptr);
        }
    }

    Resource &operator=(Resource &&other) noexcept
    {
        if (this != &other)
        {
            m_type       = other.m_type;
            m_allocation = std::move(other.m_allocation);
            m_image      = std::move(other.m_image);
            m_imageView  = std::move(other.m_imageView);
            m_sampler    = std::move(other.m_sampler);
            m_imageInfo  = other.m_imageInfo;
            m_buffer     = std::move(other.m_buffer);
            m_bufferInfo = other.m_bufferInfo;
            m_invalid    = other.m_invalid;

            other.m_imageInfo  = VkDescriptorImageInfo{};
            other.m_bufferInfo = VkDescriptorBufferInfo{};
        }
        return *this;
    }

    Resource(const Resource &)            = delete;
    Resource &operator=(const Resource &) = delete;

private:
    ResourceType m_type;
    AllocationMp m_allocation;
    VkImage m_image;
    VkImageView m_imageView;
    VkSampler m_sampler;
    VkDescriptorImageInfo m_imageInfo;
    VkBuffer m_buffer;
    VkDescriptorBufferInfo m_bufferInfo;
    bool m_invalid;
};

class UnusedInvalidDescriptorWriteTestCase : public vkt::TestCase
{
public:
    UnusedInvalidDescriptorWriteTestCase(tcu::TestContext &context, const std::string &name, const TestParams &params)
        : vkt::TestCase(context, name)
        , m_params(params)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override;

    void checkSupport(Context &context) const override;

private:
    const TestParams m_params;
};

class UnusedInvalidDescriptorWriteTestInstance : public vkt::MultiQueueRunnerTestInstance
{
public:
    UnusedInvalidDescriptorWriteTestInstance(Context &context, const TestParams &params)
        : vkt::MultiQueueRunnerTestInstance(context, COMPUTE_QUEUE)
        , m_params(params)
    {
    }

    tcu::TestStatus queuePass(const QueueData &queueData) override;

private:
    const TestParams m_params;
};

void UnusedInvalidDescriptorWriteTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 450\n"
         << "layout(push_constant) uniform PushConstants \n"
         << "{\n"
         << "    uint index;\n"
         << "} pc;\n"
         << "layout(set = 0, binding = 0, rgba32f) writeonly uniform image2D o_color;\n"
         << getResourceDeclaration(m_params.type) << "layout(set = 0, binding = 2) uniform sampler u_sampler;"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    uint ndx = pc.index;\n"
         << getResourceAccess(m_params.type, 0u) << getResourceAccess(m_params.type, 1u)
         << "    vec4 color = color0 + color1;\n"
         << "    imageStore(o_color, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y), color);\n"
         << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *UnusedInvalidDescriptorWriteTestCase::createInstance(Context &context) const
{
    return new UnusedInvalidDescriptorWriteTestInstance(context, m_params);
}

void UnusedInvalidDescriptorWriteTestCase::checkSupport(Context &context) const
{
    DE_UNREF(context);
}

tcu::TestStatus UnusedInvalidDescriptorWriteTestInstance::queuePass(const QueueData &queueData)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // Create compute render target
    const VkImageCreateInfo computeRtCi = makeComputeRenderTargetCI();
    const ImageWithMemory computeRt{vk, device, allocator, computeRtCi, MemoryRequirement::Any};

    // Create compute render target view
    const VkImageSubresourceRange colorSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const Move<VkImageView> computeRtView =
        makeImageView(vk, device, *computeRt, VK_IMAGE_VIEW_TYPE_2D, kImageFormat, colorSubresourceRange);
    const VkDescriptorImageInfo computeRtWriteInfo = {
        VK_NULL_HANDLE,          // VkSampler sampler
        computeRtView.get(),     // VkImageView imageView
        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout imageLayout
    };
    // Buffer to copy rendering result to.
    const auto tcuFormat                     = mapVkFormat(kImageFormat);
    const vk::VkDeviceSize resultsBufferSize = static_cast<vk::VkDeviceSize>(
        static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * kExtent.width * kExtent.height * kExtent.depth);
    const auto resultsBufferInfo = makeBufferCreateInfo(resultsBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const vk::BufferWithMemory resultsBuffer{vk, device, allocator, resultsBufferInfo,
                                             vk::MemoryRequirement::HostVisible};

    // Create universal sampler
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType        sType
        nullptr,                                 // const void*            pNext
        0u,                                      // VkSamplerCreateFlags   flags
        VK_FILTER_NEAREST,                       // VkFilter               magFilter
        VK_FILTER_NEAREST,                       // VkFilter               minFilter
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode    mipmapMode
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeU
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeV
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeW
        0.0f,                                    // float                  mipLodBias
        false,                                   // VkBool32               anisotropyEnable
        1.0f,                                    // float                  maxAnisotropy
        false,                                   // VkBool32               compareEnable
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp            compareOp
        0.0f,                                    // float                  minLod
        0.0f,                                    // float                  maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor          borderColor
        false                                    // VkBool32               unnormalizedCoordinates
    };
    Move<VkSampler> universalSampler = createSampler(vk, device, &samplerCreateInfo);

    const VkDescriptorImageInfo samplerInfo = {
        universalSampler.get(),    // VkSampler sampler
        VK_NULL_HANDLE,            // VkImageView imageView
        VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout imageLayout
    };

    // Create tested resources
    std::vector<Resource> testedResources;
    for (uint32_t ndx = 0; ndx < 2; ++ndx)
    {
        testedResources.emplace_back(m_params.type, vk, device, allocator);
    }
    // Create invalid resource if needed - if shader try to access this resource, it will cause crash
    if (m_params.addInvalidDescriptor && requireImage(m_params.type))
        testedResources.emplace_back(m_params.type, vk, device, allocator, true);

    // Create descriptor set
    Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
            .addType(getVkDescriptorType(m_params.type), 3u)
            .addType(VK_DESCRIPTOR_TYPE_SAMPLER, 1u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addArrayBinding(getVkDescriptorType(m_params.type), 3u, VK_SHADER_STAGE_COMPUTE_BIT,
                             VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device);

    const VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType
        nullptr,                                        // const void*                     pNext
        descriptorPool.get(),                           // VkDescriptorPool                descriptorPool
        1u,                                             // uint32_t                        descriptorSetCount
        &descriptorSetLayout.get(),                     // const VkDescriptorSetLayout*    pSetLayouts
    };
    Move<VkDescriptorSet> descriptorSet = allocateDescriptorSet(vk, device, &allocInfo);

    DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
    descriptorSetUpdateBuilder
        .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &computeRtWriteInfo)
        .writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2), VK_DESCRIPTOR_TYPE_SAMPLER,
                     &samplerInfo);
    if (requireBuffer(m_params.type))
    {
        descriptorSetUpdateBuilder.writeArray(
            descriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 0),
            testedResources[0].getDescriptorType(), 1u, &testedResources[0].getBufferInfo());
        descriptorSetUpdateBuilder.writeArray(
            descriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 1),
            testedResources[1].getDescriptorType(), 1u, &testedResources[1].getBufferInfo());
    }
    else if (requireImage(m_params.type))
    {
        descriptorSetUpdateBuilder.writeArray(
            descriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 0),
            testedResources[0].getDescriptorType(), 1u, &testedResources[0].getImageInfo());
        descriptorSetUpdateBuilder.writeArray(
            descriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 1),
            testedResources[1].getDescriptorType(), 1u, &testedResources[1].getImageInfo());

        if (m_params.addInvalidDescriptor)
        {
            // setting invalid resource that will not be accessed by pipeline
            descriptorSetUpdateBuilder.writeArray(
                descriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 2),
                testedResources[2].getDescriptorType(), 1u, &testedResources[2].getImageInfo());
        }
    }
    descriptorSetUpdateBuilder.update(vk, device);

    // Create pipeline
    Move<VkShaderModule> computeModule =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u);

    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0u,                          // uint32_t offset;
        4u,                          // uint32_t size;
    };
    Move<VkPipelineLayout> pipelineLayout =
        makePipelineLayout(vk, device, descriptorSetLayout.get(), &pushConstantRange);

    Move<VkPipeline> computePipeline = makeComputePipeline(vk, device, pipelineLayout.get(), computeModule.get());

    // Create command pool
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueData.familyIndex);

    // Create command buffer
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Update resources
    for (auto &resource : testedResources)
    {
        resource.update(vk, device, queueData);
    }

    // Record command buffer
    beginCommandBuffer(vk, cmdBuffer.get());

    // Clear compute render target
    VkClearColorValue clearRtColorValue = {{1.0, 0.0f, 0.0f, 1.0f}};

    // preClear barrier
    VkImageMemoryBarrier preClearRtBarrier = {

        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
        nullptr,                                // const void*             pNext
        VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        computeRt.get(),                        // VkImage                 image
        colorSubresourceRange                   // VkImageSubresourceRange subresourceRange
    };
    vk.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &preClearRtBarrier);
    vk.cmdClearColorImage(cmdBuffer.get(), computeRt.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearRtColorValue,
                          1u, &colorSubresourceRange);

    // preWrite barrier
    VkImageMemoryBarrier preWriteRtBarrier = {

        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
        nullptr,                                // const void*             pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           srcAccessMask
        VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags           dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           oldLayout
        VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout           newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        computeRt.get(),                        // VkImage                 image
        colorSubresourceRange                   // VkImageSubresourceRange subresourceRange
    };
    vk.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &preWriteRtBarrier);

    // Bind pipeline and descriptor set
    vk.cmdBindPipeline(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.get());
    vk.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1,
                             &descriptorSet.get(), 0, nullptr);

    // Push constants
    uint32_t index = 0u;
    vk.cmdPushConstants(cmdBuffer.get(), pipelineLayout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(index), &index);

    // Dispath
    vk.cmdDispatch(cmdBuffer.get(), kExtent.width, kExtent.height, 1);

    // copy result to results buffer
    const tcu::IVec2 copySize{static_cast<int>(kExtent.width), static_cast<int>(kExtent.height)};
    copyImageToBuffer(vk, cmdBuffer.get(), computeRt.get(), resultsBuffer.get(), copySize,
                      vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_GENERAL);

    endCommandBuffer(vk, cmdBuffer.get());

    // Submit work
    submitCommandsAndWait(vk, device, queueData.handle, cmdBuffer.get());

    // Destroying resources
    for (auto &resource : testedResources)
    {
        resource.DestroyInternals(vk, device);
    }

    // Check result
    const auto &resultsBufferAlloc = resultsBuffer.getAllocation();
    vk::invalidateAlloc(vk, device, resultsBufferAlloc);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultsBufferAlloc.getHostPtr()) + resultsBufferAlloc.getOffset();
    const tcu::ConstPixelBufferAccess resultPixels{tcuFormat, static_cast<int>(kExtent.width),
                                                   static_cast<int>(kExtent.height), 1, resultsBufferPtr};

    // Generate expected result and compare Pixel access
    std::vector<tcu::Vec4> expectedPixelsData(kExtent.width * kExtent.height, tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    const tcu::ConstPixelBufferAccess expectedPixels{tcuFormat, static_cast<int>(kExtent.width),
                                                     static_cast<int>(kExtent.height), 1, expectedPixelsData.data()};

    // Compare result and log
    const tcu::Vec4 threshold(0.0f);
    std::string compTitle = "Queue family " + std::to_string(queueData.familyIndex) + " result comparison";
    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", compTitle.c_str(), expectedPixels,
                                    resultPixels, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Failed");

    return tcu::TestStatus::pass("Pass");
}

class InvalidDescriptorCopyTestCase : public vkt::TestCase
{
public:
    InvalidDescriptorCopyTestCase(tcu::TestContext &context, const std::string &name, const TestParams &params)
        : vkt::TestCase(context, name)
        , m_params(params)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override;

    void checkSupport(Context &context) const override;

private:
    const TestParams m_params;
};

class InvalidDescriptorCopyTestInstance : public vkt::MultiQueueRunnerTestInstance
{
public:
    InvalidDescriptorCopyTestInstance(Context &context, const TestParams &params)
        : vkt::MultiQueueRunnerTestInstance(context, COMPUTE_QUEUE)
        , m_params(params)
    {
    }

    tcu::TestStatus queuePass(const QueueData &queueData) override;

private:
    const TestParams m_params;
};

void InvalidDescriptorCopyTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 450\n"
         << "layout(push_constant) uniform PushConstants \n"
         << "{\n"
         << "    uint index;\n"
         << "} pc;\n"
         << "layout(set = 0, binding = 0, rgba32f) writeonly uniform image2D o_color;\n"
         << getResourceDeclaration(m_params.type) << "layout(set = 0, binding = 2) uniform sampler u_sampler;"
         << "\n"
         << "void main()\n"
         << "{\n"
         << "    uint ndx = pc.index;\n"
         << getResourceAccess(m_params.type, 0u) << getResourceAccess(m_params.type, 1u)
         << "    vec4 color = color0 + color1;\n"
         << "    imageStore(o_color, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y), color);\n"
         << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *InvalidDescriptorCopyTestCase::createInstance(Context &context) const
{
    return new InvalidDescriptorCopyTestInstance(context, m_params);
}

void InvalidDescriptorCopyTestCase::checkSupport(Context &context) const
{
    DE_UNREF(context);
}

tcu::TestStatus InvalidDescriptorCopyTestInstance::queuePass(const QueueData &queueData)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // Create compute render target
    const VkImageCreateInfo computeRtCi = makeComputeRenderTargetCI();
    const ImageWithMemory computeRt{vk, device, allocator, computeRtCi, MemoryRequirement::Any};

    // Create compute render target view
    const VkImageSubresourceRange colorSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const Move<VkImageView> computeRtView =
        makeImageView(vk, device, *computeRt, VK_IMAGE_VIEW_TYPE_2D, kImageFormat, colorSubresourceRange);
    const VkDescriptorImageInfo computeRtWriteInfo = {
        VK_NULL_HANDLE,          // VkSampler sampler
        computeRtView.get(),     // VkImageView imageView
        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout imageLayout
    };
    // Buffer to copy rendering result to.
    const auto tcuFormat                     = mapVkFormat(kImageFormat);
    const vk::VkDeviceSize resultsBufferSize = static_cast<vk::VkDeviceSize>(
        static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * kExtent.width * kExtent.height * kExtent.depth);
    const auto resultsBufferInfo = makeBufferCreateInfo(resultsBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const vk::BufferWithMemory resultsBuffer{vk, device, allocator, resultsBufferInfo,
                                             vk::MemoryRequirement::HostVisible};

    // Create universal sampler
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType        sType
        nullptr,                                 // const void*            pNext
        0u,                                      // VkSamplerCreateFlags   flags
        VK_FILTER_NEAREST,                       // VkFilter               magFilter
        VK_FILTER_NEAREST,                       // VkFilter               minFilter
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode    mipmapMode
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeU
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeV
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode   addressModeW
        0.0f,                                    // float                  mipLodBias
        false,                                   // VkBool32               anisotropyEnable
        1.0f,                                    // float                  maxAnisotropy
        false,                                   // VkBool32               compareEnable
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp            compareOp
        0.0f,                                    // float                  minLod
        0.0f,                                    // float                  maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor          borderColor
        false                                    // VkBool32               unnormalizedCoordinates
    };
    Move<VkSampler> universalSampler = createSampler(vk, device, &samplerCreateInfo);

    const VkDescriptorImageInfo samplerInfo = {
        universalSampler.get(),    // VkSampler sampler
        VK_NULL_HANDLE,            // VkImageView imageView
        VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout imageLayout
    };

    // Create and update tested resources
    std::vector<Resource> testedResources;
    for (uint32_t ndx = 0; ndx < 3; ++ndx)
    {
        testedResources.emplace_back(m_params.type, vk, device, allocator);
        testedResources[ndx].update(vk, device, queueData);
    }

    // Create descriptor set layout
    Move<VkDescriptorSetLayout> descriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addArrayBinding(getVkDescriptorType(m_params.type), 3u, VK_SHADER_STAGE_COMPUTE_BIT,
                             VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device);

    // Create src descriptor set
    Move<VkDescriptorPool> srcDescriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
            .addType(getVkDescriptorType(m_params.type), 3u)
            .addType(VK_DESCRIPTOR_TYPE_SAMPLER, 1u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    const VkDescriptorSetAllocateInfo srcAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType
        nullptr,                                        // const void*                     pNext
        srcDescriptorPool.get(),                        // VkDescriptorPool                descriptorPool
        1u,                                             // uint32_t                        descriptorSetCount
        &descriptorSetLayout.get(),                     // const VkDescriptorSetLayout*    pSetLayouts
    };
    Move<VkDescriptorSet> srcDescriptorSet = allocateDescriptorSet(vk, device, &srcAllocInfo);

    DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
    descriptorSetUpdateBuilder
        .writeSingle(srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &computeRtWriteInfo)
        .writeSingle(srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2),
                     VK_DESCRIPTOR_TYPE_SAMPLER, &samplerInfo);
    if (requireBuffer(m_params.type))
    {
        descriptorSetUpdateBuilder.writeArray(
            srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 0),
            testedResources[0].getDescriptorType(), 1u, &testedResources[0].getBufferInfo());
        descriptorSetUpdateBuilder.writeArray(
            srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 1),
            testedResources[1].getDescriptorType(), 1u, &testedResources[1].getBufferInfo());
        descriptorSetUpdateBuilder.writeArray(
            srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 2),
            testedResources[2].getDescriptorType(), 1u, &testedResources[2].getBufferInfo());
    }
    else if (requireImage(m_params.type))
    {
        descriptorSetUpdateBuilder.writeArray(
            srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 0),
            testedResources[0].getDescriptorType(), 1u, &testedResources[0].getImageInfo());
        descriptorSetUpdateBuilder.writeArray(
            srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 1),
            testedResources[1].getDescriptorType(), 1u, &testedResources[1].getImageInfo());
        descriptorSetUpdateBuilder.writeArray(
            srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 2),
            testedResources[2].getDescriptorType(), 1u, &testedResources[2].getImageInfo());
    }
    descriptorSetUpdateBuilder.update(vk, device);

    // Create dst descriptor set
    Move<VkDescriptorPool> dstDescriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
            .addType(getVkDescriptorType(m_params.type), 3u)
            .addType(VK_DESCRIPTOR_TYPE_SAMPLER, 1u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    const VkDescriptorSetAllocateInfo dstAllocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType
        nullptr,                                        // const void*                     pNext
        dstDescriptorPool.get(),                        // VkDescriptorPool                descriptorPool
        1u,                                             // uint32_t                        descriptorSetCount
        &descriptorSetLayout.get(),                     // const VkDescriptorSetLayout*    pSetLayouts
    };
    Move<VkDescriptorSet> dstDescriptorSet = allocateDescriptorSet(vk, device, &dstAllocInfo);

    // Destroy one resource and perform copy
    testedResources[2].DestroyInternals(vk, device);

    DescriptorSetUpdateBuilder()
        .copySingle(srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0), dstDescriptorSet.get(),
                    DescriptorSetUpdateBuilder::Location::binding(0))
        .copySingle(srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2), dstDescriptorSet.get(),
                    DescriptorSetUpdateBuilder::Location::binding(2))
        .copyArray(srcDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 0),
                   dstDescriptorSet.get(), DescriptorSetUpdateBuilder::Location::bindingArrayElement(1, 0), 3u)
        .update(vk, device);

    // Create pipeline
    Move<VkShaderModule> computeModule =
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u);

    VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags stageFlags;
        0u,                          // uint32_t offset;
        4u,                          // uint32_t size;
    };
    Move<VkPipelineLayout> pipelineLayout =
        makePipelineLayout(vk, device, descriptorSetLayout.get(), &pushConstantRange);

    Move<VkPipeline> computePipeline = makeComputePipeline(vk, device, pipelineLayout.get(), computeModule.get());

    // Create command pool
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueData.familyIndex);

    // Create command buffer
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Record command buffer
    beginCommandBuffer(vk, cmdBuffer.get());

    // Clear compute render target
    VkClearColorValue clearRtColorValue = {{1.0, 0.0f, 0.0f, 1.0f}};

    // preClear barrier
    VkImageMemoryBarrier preClearRtBarrier = {

        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
        nullptr,                                // const void*             pNext
        VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        computeRt.get(),                        // VkImage                 image
        colorSubresourceRange                   // VkImageSubresourceRange subresourceRange
    };
    vk.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &preClearRtBarrier);
    vk.cmdClearColorImage(cmdBuffer.get(), computeRt.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearRtColorValue,
                          1u, &colorSubresourceRange);

    // preWrite barrier
    VkImageMemoryBarrier preWriteRtBarrier = {

        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
        nullptr,                                // const void*             pNext
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           srcAccessMask
        VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags           dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           oldLayout
        VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout           newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        computeRt.get(),                        // VkImage                 image
        colorSubresourceRange                   // VkImageSubresourceRange subresourceRange
    };
    vk.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, 1u, &preWriteRtBarrier);

    // Bind pipeline and descriptor set
    vk.cmdBindPipeline(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.get());
    vk.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1,
                             &dstDescriptorSet.get(), 0, nullptr);

    // Push constants
    uint32_t index = 0u;
    vk.cmdPushConstants(cmdBuffer.get(), pipelineLayout.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(index), &index);

    // Dispath
    vk.cmdDispatch(cmdBuffer.get(), kExtent.width, kExtent.height, 1);

    // copy result to results buffer
    const tcu::IVec2 copySize{static_cast<int>(kExtent.width), static_cast<int>(kExtent.height)};
    copyImageToBuffer(vk, cmdBuffer.get(), computeRt.get(), resultsBuffer.get(), copySize,
                      vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_GENERAL);

    endCommandBuffer(vk, cmdBuffer.get());

    // Submit work
    submitCommandsAndWait(vk, device, queueData.handle, cmdBuffer.get());

    // Destroying resources
    for (uint32_t ndx = 0; ndx < 2; ++ndx)
    {
        testedResources[ndx].DestroyInternals(vk, device);
    }

    // Check result
    const auto &resultsBufferAlloc = resultsBuffer.getAllocation();
    vk::invalidateAlloc(vk, device, resultsBufferAlloc);

    const auto resultsBufferPtr =
        reinterpret_cast<const char *>(resultsBufferAlloc.getHostPtr()) + resultsBufferAlloc.getOffset();
    const tcu::ConstPixelBufferAccess resultPixels{tcuFormat, static_cast<int>(kExtent.width),
                                                   static_cast<int>(kExtent.height), 1, resultsBufferPtr};

    // Generate expected result and compare Pixel access
    std::vector<tcu::Vec4> expectedPixelsData(kExtent.width * kExtent.height, tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    const tcu::ConstPixelBufferAccess expectedPixels{tcuFormat, static_cast<int>(kExtent.width),
                                                     static_cast<int>(kExtent.height), 1, expectedPixelsData.data()};

    // Compare result and log
    const tcu::Vec4 threshold(0.0f);
    std::string compTitle = "Queue family " + std::to_string(queueData.familyIndex) + " result comparison";
    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", compTitle.c_str(), expectedPixels,
                                    resultPixels, threshold, tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Failed");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createUnusedInvalidDescriptorTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "unused_invalid_descriptor"));

    // Descriptor writes
    {
        de::MovePtr<tcu::TestCaseGroup> write(new tcu::TestCaseGroup(testCtx, "write"));

        // Unused binding
        {
            de::MovePtr<tcu::TestCaseGroup> unused(new tcu::TestCaseGroup(testCtx, "unused"));

            ResourceType types[] = {
                UNIFORM_BUFFER, STORAGE_BUFFER, SAMPLED_IMAGE, COMBINED_IMAGE_SAMPLER, STORAGE_IMAGE,
            };

            for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(types); ++ndx)
            {
                TestParams params;
                params.type                 = types[ndx];
                params.addInvalidDescriptor = false;
                unused->addChild(new UnusedInvalidDescriptorWriteTestCase(testCtx, toString(types[ndx]), params));
            }

            write->addChild(unused.release());
        }

        // Invalid binding
        {
            de::MovePtr<tcu::TestCaseGroup> invalid(new tcu::TestCaseGroup(testCtx, "invalid"));

            ResourceType types[] = {
                SAMPLED_IMAGE,
                COMBINED_IMAGE_SAMPLER,
                STORAGE_IMAGE,
            };

            for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(types); ++ndx)
            {
                TestParams params;
                params.type                 = types[ndx];
                params.addInvalidDescriptor = true;
                invalid->addChild(new UnusedInvalidDescriptorWriteTestCase(testCtx, toString(types[ndx]), params));
            }

            write->addChild(invalid.release());
        }

        group->addChild(write.release());
    }

    // Descriptor copy
    {
        de::MovePtr<tcu::TestCaseGroup> copy(new tcu::TestCaseGroup(testCtx, "copy"));

        ResourceType types[] = {
            UNIFORM_BUFFER, STORAGE_BUFFER, SAMPLED_IMAGE, COMBINED_IMAGE_SAMPLER, STORAGE_IMAGE,
        };

        for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(types); ++ndx)
        {
            TestParams params;
            params.type                 = types[ndx];
            params.addInvalidDescriptor = false;
            copy->addChild(new InvalidDescriptorCopyTestCase(testCtx, toString(types[ndx]), params));
        }

        group->addChild(copy.release());
    }

    return group.release();
}

} // namespace BindingModel
} // namespace vkt
