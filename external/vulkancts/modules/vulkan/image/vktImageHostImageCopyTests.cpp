/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC.
 * Copyright (c) 2022 LunarG, Inc.
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
 * \brief Tests for VK_EXT_host_image_copy
 *//*--------------------------------------------------------------------*/

#include "vktImageHostImageCopyTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktImageTestsUtil.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"

#include <set>

namespace vkt
{
namespace image
{
namespace
{

using namespace vk;

vk::VkImageAspectFlags getAspectFlags(vk::VkFormat format)
{
    if (isCompressedFormat(format))
    {
        return vk::VK_IMAGE_ASPECT_COLOR_BIT;
    }

    const auto sampledFormat = mapVkFormat(format);
    if (sampledFormat.order == tcu::TextureFormat::S)
    {
        return vk::VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    if (sampledFormat.order == tcu::TextureFormat::D || sampledFormat.order == tcu::TextureFormat::DS)
    {
        return vk::VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    return vk::VK_IMAGE_ASPECT_COLOR_BIT;
}

uint32_t getChannelSize(vk::VkFormat format)
{
    const auto tcuFormat =
        isCompressedFormat(format) ? tcu::getUncompressedFormat(mapVkCompressedFormat(format)) : mapVkFormat(format);
    if (tcuFormat.order != tcu::TextureFormat::D && tcuFormat.order != tcu::TextureFormat::S &&
        tcuFormat.order != tcu::TextureFormat::DS)
    {
        return tcu::getChannelSize(tcuFormat.type);
    }
    switch (format)
    {
    case vk::VK_FORMAT_D24_UNORM_S8_UINT:
        return 4;
    case vk::VK_FORMAT_D32_SFLOAT:
        return 4;
    case vk::VK_FORMAT_D16_UNORM:
        return 2;
    case vk::VK_FORMAT_S8_UINT:
        return 1;
    default:
        break;
    }
    DE_ASSERT(0);
    return 0;
}

uint32_t getNumChannels(vk::VkFormat format)
{
    const auto tcuFormat =
        isCompressedFormat(format) ? tcu::getUncompressedFormat(mapVkCompressedFormat(format)) : mapVkFormat(format);
    if (tcuFormat.order != tcu::TextureFormat::D && tcuFormat.order != tcu::TextureFormat::S &&
        tcuFormat.order != tcu::TextureFormat::DS)
    {
        return tcu::getNumUsedChannels(tcuFormat.order);
    }
    return 1;
}

void generateData(void *ptr, uint32_t size, vk::VkFormat format)
{
    if (isDepthStencilFormat(format))
    {
        de::Random randomGen(deInt32Hash((uint32_t)format) ^ deInt32Hash((uint32_t)size));
        if (format == VK_FORMAT_D16_UNORM)
        {
            ycbcr::fillRandomNoNaN(&randomGen, (uint8_t *)ptr, size, VK_FORMAT_R16_UNORM);
        }
        else
        {
            ycbcr::fillRandomNoNaN(&randomGen, (uint8_t *)ptr, size, VK_FORMAT_R32_SFLOAT);
        }
    }
    else if (isCompressedFormat(format))
    {
        memset(ptr, 255, size);
    }
    else
    {
        de::Random randomGen(deInt32Hash((uint32_t)format) ^ deInt32Hash((uint32_t)size));
        ycbcr::fillRandomNoNaN(&randomGen, (uint8_t *)ptr, size, format);
    }
}

void getHostImageCopyProperties(const vk::InstanceDriver &instanceDriver, VkPhysicalDevice physicalDevice,
                                vk::VkPhysicalDeviceHostImageCopyPropertiesEXT *hostImageCopyProperties)
{
    vk::VkPhysicalDeviceProperties properties;
    deMemset(&properties, 0, sizeof(vk::VkPhysicalDeviceProperties));
    vk::VkPhysicalDeviceProperties2 properties2 = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, // VkStructureType                    sType
        hostImageCopyProperties,                            // const void*                        pNext
        properties                                          // VkPhysicalDeviceProperties        properties
    };
    instanceDriver.getPhysicalDeviceProperties2(physicalDevice, &properties2);
}

bool isBlockCompressedFormat(vk::VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return true;
    default:
        break;
    }
    return false;
}

void checkSupportedFormatFeatures(const vk::InstanceDriver &vki, VkPhysicalDevice physicalDevice, vk::VkFormat format,
                                  vk::VkImageTiling tiling, uint64_t *outDrmModifier)
{
    vk::VkDrmFormatModifierPropertiesList2EXT drmList = vk::initVulkanStructure();
    vk::VkFormatProperties3 formatProperties3         = tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ?
                                                            vk::initVulkanStructure(&drmList) :
                                                            vk::initVulkanStructure();
    vk::VkFormatProperties2 formatProperties2         = vk::initVulkanStructure(&formatProperties3);
    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);
    std::vector<vk::VkDrmFormatModifierProperties2EXT> modifiers(drmList.drmFormatModifierCount);

    if (tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
    {
        if (drmList.drmFormatModifierCount == 0)
            TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for drm format modifier.");
        drmList.pDrmFormatModifierProperties = modifiers.data();
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);

        bool modifierFound = false;
        for (uint32_t i = 0; i < drmList.drmFormatModifierCount; ++i)
        {
            if (drmList.pDrmFormatModifierProperties[i].drmFormatModifierTilingFeatures &
                vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT)
            {
                *outDrmModifier = drmList.pDrmFormatModifierProperties[i].drmFormatModifier;
                return;
            }
        }

        if (!modifierFound)
            TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for drm format modifier.");
    }
    else
    {
        if (tiling == vk::VK_IMAGE_TILING_LINEAR &&
            (formatProperties3.linearTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
            TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for optimal tiling.");
        if (tiling == vk::VK_IMAGE_TILING_OPTIMAL &&
            (formatProperties3.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
            TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for optimal tiling.");
    }
}

enum Command
{
    DRAW,
    DISPATCH,
};

enum HostCopyAction
{
    MEMORY_TO_IMAGE,
    IMAGE_TO_MEMORY,
    MEMCPY,
};

struct TestParameters
{
    HostCopyAction action;
    bool hostCopy;
    bool hostTransferLayout;
    bool dynamicRendering;
    Command command;
    vk::VkFormat imageSampledFormat;
    vk::VkImageLayout srcLayout;
    vk::VkImageLayout dstLayout;
    vk::VkImageLayout intermediateLayout;
    vk::VkImageTiling sampledTiling;
    vk::VkFormat imageOutputFormat;
    vk::VkExtent3D imageSize;
    bool sparse;
    uint32_t mipLevel;
    uint32_t regionsCount;
    uint32_t padding;
};

class HostImageCopyTestInstance : public vkt::TestInstance
{
public:
    HostImageCopyTestInstance(vkt::Context &context, const TestParameters &parameters)
        : vkt::TestInstance(context)
        , m_parameters(parameters)
    {
    }
    void transitionImageLayout(const Move<vk::VkCommandBuffer> *cmdBuffer, vk::VkImage image,
                               vk::VkImageUsageFlags usage, vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout,
                               vk::VkImageSubresourceRange subresourceRange);
    void copyMemoryToImage(const std::vector<uint8_t> testData, vk::VkImage image, uint32_t texelSize,
                           const vk::VkImageSubresourceLayers subresourceLayers, int32_t xOffset, int32_t yOffset,
                           uint32_t width, uint32_t height);

private:
    tcu::TestStatus iterate(void);

    const TestParameters m_parameters;
};

void HostImageCopyTestInstance::transitionImageLayout(const Move<vk::VkCommandBuffer> *cmdBuffer, vk::VkImage image,
                                                      vk::VkImageUsageFlags usage, vk::VkImageLayout oldLayout,
                                                      vk::VkImageLayout newLayout,
                                                      vk::VkImageSubresourceRange subresourceRange)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device = m_context.getDevice();
    const vk::VkQueue queue   = m_context.getUniversalQueue();

    if (m_parameters.hostTransferLayout && (usage & vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT))
    {
        vk::VkHostImageLayoutTransitionInfoEXT transition = {
            vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            image,                                                       // VkImage image;
            oldLayout,                                                   // VkImageLayout oldLayout;
            newLayout,                                                   // VkImageLayout newLayout;
            subresourceRange                                             // VkImageSubresourceRange subresourceRange;
        };
        vk.transitionImageLayoutEXT(device, 1, &transition);
    }
    else
    {
        vk::beginCommandBuffer(vk, **cmdBuffer, 0u);
        auto imageMemoryBarrier =
            makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, oldLayout, newLayout, image, subresourceRange);
        vk.cmdPipelineBarrier(**cmdBuffer, vk::VK_PIPELINE_STAGE_NONE, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1, &imageMemoryBarrier);
        vk::endCommandBuffer(vk, **cmdBuffer);
        vk::submitCommandsAndWait(vk, device, queue, **cmdBuffer);
    }
}

void HostImageCopyTestInstance::copyMemoryToImage(const std::vector<uint8_t> testData, vk::VkImage image,
                                                  uint32_t texelSize,
                                                  const vk::VkImageSubresourceLayers subresourceLayers, int32_t xOffset,
                                                  int32_t yOffset, uint32_t width, uint32_t height)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device = m_context.getDevice();
    std::vector<uint8_t> data(texelSize * width * height);
    const uint32_t imageWidth = m_parameters.imageSize.width;
    for (uint32_t i = 0; i < height; ++i)
    {
        memcpy(&data[i * width * texelSize], &testData[((yOffset + i) * imageWidth + xOffset) * texelSize],
               width * texelSize);
    }

    const uint32_t regionsCount = m_parameters.regionsCount > height ? m_parameters.regionsCount : 1u;
    std::vector<vk::VkMemoryToImageCopyEXT> regions;

    for (uint32_t i = 0; i < regionsCount; ++i)
    {
        vk::VkOffset3D offset = {xOffset, (int32_t)(yOffset + height / regionsCount * i), 0};
        vk::VkExtent3D extent = {width, height / regionsCount, 1};
        if (i == regionsCount - 1)
            extent.height = height - height / regionsCount * i;

        if (extent.height == 0)
            continue;

        uint32_t dataOffset = width * (height / regionsCount * i) * texelSize;

        const vk::VkMemoryToImageCopyEXT region = {
            vk::VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            &data[dataOffset],                              // const void* memoryHostPointer;
            0,                                              // uint32_t memoryRowLength;
            0,                                              // uint32_t memoryImageHeight;
            subresourceLayers,                              // VkImageSubresourceLayers imageSubresource;
            offset,                                         // VkOffset3D imageOffset;
            extent                                          // VkExtent3D imageExtent;
        };
        regions.push_back(region);
    }

    vk::VkCopyMemoryToImageInfoEXT copyMemoryToImageInfo = {
        vk::VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkMemoryImageCopyFlagsEXT flags;
        image,                                               // VkImage dstImage;
        m_parameters.dstLayout,                              // VkImageLayout dstImageLayout;
        (uint32_t)regions.size(),                            // uint32_t regionCount;
        regions.data(),                                      // const VkMemoryToImageCopyEXT* pRegions;
    };

    vk.copyMemoryToImageEXT(device, &copyMemoryToImageInfo);
}

tcu::TestStatus HostImageCopyTestInstance::iterate(void)
{
    const InstanceInterface &vki              = m_context.getInstanceInterface();
    const DeviceInterface &vk                 = m_context.getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::VkDevice device                 = m_context.getDevice();
    const auto &deviceExtensions              = m_context.getDeviceExtensions();
    const uint32_t queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const vk::VkQueue queue                   = m_context.getUniversalQueue();
    auto &alloc                               = m_context.getDefaultAllocator();
    tcu::TestLog &log                         = m_context.getTestContext().getLog();

    std::stringstream commandsLog;

    const Move<vk::VkCommandPool> cmdPool(
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const vk::VkExtent3D imageSize    = {m_parameters.imageSize.width * (uint32_t)dePow(2, m_parameters.mipLevel),
                                         m_parameters.imageSize.height * (uint32_t)dePow(2, m_parameters.mipLevel), 1};
    const vk::VkExtent3D mipImageSize = {m_parameters.imageSize.width, m_parameters.imageSize.height, 1};

    const vk::VkRect2D renderArea = vk::makeRect2D(0, 0, mipImageSize.width, mipImageSize.height);

    const auto sampledChannelSize = getChannelSize(m_parameters.imageSampledFormat);
    const auto sampledNumChannels = getNumChannels(m_parameters.imageSampledFormat);
    const auto sampledBufferCount = mipImageSize.width * mipImageSize.height * sampledNumChannels;
    const auto sampledBufferSize  = sampledBufferCount * sampledChannelSize;

    const auto outputFormat      = mapVkFormat(m_parameters.imageOutputFormat);
    const auto outputChannelSize = getChannelSize(m_parameters.imageOutputFormat);
    const auto outputNumChannels = getNumUsedChannels(m_parameters.imageOutputFormat);
    const auto outputBufferCount = mipImageSize.width * mipImageSize.height * outputNumChannels;
    const auto outputBufferSize  = outputBufferCount * outputChannelSize;

    vk::VkImage sampledImage     = VK_NULL_HANDLE;
    vk::VkImage sampledImageCopy = VK_NULL_HANDLE;
    de::MovePtr<ImageWithMemory> sampledImageWithMemory;
    de::MovePtr<SparseImage> sparseSampledImage;
    de::MovePtr<SparseImage> sparseSampledImageCopy;
    de::MovePtr<ImageWithMemory> sampledImageWithMemoryCopy;
    de::MovePtr<ImageWithMemory> outputImage;
    Move<vk::VkImageView> sampledImageView;
    Move<vk::VkImageView> sampledImageViewCopy;
    Move<vk::VkImageView> outputImageView;
    vk::VkImageUsageFlags sampledImageUsage;
    vk::VkImageUsageFlags outputImageUsage;

    const vk::VkImageAspectFlags sampledAspect      = getAspectFlags(m_parameters.imageSampledFormat);
    const vk::VkComponentMapping componentMapping   = makeComponentMappingRGBA();
    const vk::VkOffset3D imageOffset                = makeOffset3D(0, 0, 0);
    const vk::VkImageSubresource sampledSubresource = makeImageSubresource(sampledAspect, m_parameters.mipLevel, 0u);
    const vk::VkImageSubresourceRange sampledSubresourceRange =
        makeImageSubresourceRange(sampledAspect, m_parameters.mipLevel, 1u, 0u, 1u);
    const vk::VkImageSubresourceLayers sampledSubresourceLayers =
        makeImageSubresourceLayers(sampledAspect, m_parameters.mipLevel, 0u, 1u);
    const auto outputSubresourceRange =
        makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, m_parameters.mipLevel, 1u, 0u, 1u);
    const vk::VkImageSubresourceLayers outputSubresourceLayers =
        makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, m_parameters.mipLevel, 0u, 1u);

    std::vector<uint8_t> testData(sampledBufferSize);
    generateData(testData.data(), sampledBufferSize, m_parameters.imageSampledFormat);

    // Create sampled image
    {
        sampledImageUsage = vk::VK_IMAGE_USAGE_SAMPLED_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (m_parameters.action == MEMORY_TO_IMAGE || m_parameters.action == MEMCPY)
            sampledImageUsage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
        if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            sampledImageUsage |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            sampledImageUsage |= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            sampledImageUsage |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
            nullptr,                                 // const void*                pNext
            0u,                                      // VkImageCreateFlags        flags
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
            m_parameters.imageSampledFormat,         // VkFormat                    format
            imageSize,                               // VkExtent3D                extent
            m_parameters.mipLevel + 1,               // uint32_t                    mipLevels
            1u,                                      // uint32_t                    arrayLayers
            vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
            m_parameters.sampledTiling,              // VkImageTiling            tiling
            sampledImageUsage,                       // VkImageUsageFlags        usage
            vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
            0,                                       // uint32_t                    queueFamilyIndexCount
            nullptr,                                 // const uint32_t*            pQueueFamilyIndices
            vk::VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
        };

        if (m_parameters.sparse)
        {
            createInfo.flags |= (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            sparseSampledImage = de::MovePtr<SparseImage>(new SparseImage(vk, device, physicalDevice, vki, createInfo,
                                                                          m_context.getSparseQueue(), alloc,
                                                                          mapVkFormat(createInfo.format)));
            sampledImage       = **sparseSampledImage;
            if (m_parameters.action == MEMCPY)
            {
                sparseSampledImageCopy = de::MovePtr<SparseImage>(
                    new SparseImage(vk, device, physicalDevice, vki, createInfo, m_context.getSparseQueue(), alloc,
                                    mapVkFormat(createInfo.format)));
                sampledImageCopy = **sparseSampledImageCopy;
            }
        }
        else
        {
            sampledImageWithMemory = de::MovePtr<ImageWithMemory>(
                new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
            sampledImage = **sampledImageWithMemory;
            if (m_parameters.action == MEMCPY)
            {
                sampledImageWithMemoryCopy = de::MovePtr<ImageWithMemory>(
                    new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
                sampledImageCopy = **sampledImageWithMemoryCopy;
            }
        }

        vk::VkImageViewCreateInfo imageViewCreateInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
            sampledImage,                                 // VkImage image;
            vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            m_parameters.imageSampledFormat,              // VkFormat format;
            componentMapping,                             // VkComponentMapping components;
            sampledSubresourceRange                       // VkImageSubresourceRange subresourceRange;
        };
        sampledImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
        if (m_parameters.action == MEMCPY)
        {
            imageViewCreateInfo.image = sampledImageCopy;
            sampledImageViewCopy      = createImageView(vk, device, &imageViewCreateInfo, NULL);
            ;
        }
    }

    // Create output image
    {
        outputImageUsage = vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (m_parameters.action == IMAGE_TO_MEMORY)
            outputImageUsage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
        if (m_parameters.command == DISPATCH)
            outputImageUsage |= vk::VK_IMAGE_USAGE_STORAGE_BIT;

        const vk::VkImageCreateInfo createInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
            nullptr,                                 // const void*                pNext
            0u,                                      // VkImageCreateFlags        flags
            vk::VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
            m_parameters.imageOutputFormat,          // VkFormat                    format
            imageSize,                               // VkExtent3D                extent
            m_parameters.mipLevel + 1,               // uint32_t                    mipLevels
            1u,                                      // uint32_t                    arrayLayers
            vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
            vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
            outputImageUsage,                        // VkImageUsageFlags        usage
            vk::VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
            0,                                       // uint32_t                    queueFamilyIndexCount
            nullptr,                                 // const uint32_t*            pQueueFamilyIndices
            vk::VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
        };

        outputImage = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));

        vk::VkImageViewCreateInfo imageViewCreateInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            (VkImageViewCreateFlags)0u,                   // VkImageViewCreateFlags flags;
            **outputImage,                                // VkImage image;
            vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            m_parameters.imageOutputFormat,               // VkFormat format;
            componentMapping,                             // VkComponentMapping components;
            outputSubresourceRange                        // VkImageSubresourceRange subresourceRange;
        };
        outputImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
    }

    const vk::VkAttachmentDescription colorAttachmentDescription = {
        (vk::VkAttachmentDescriptionFlags)0u, // VkAttachmentDescriptionFlags    flags
        m_parameters.imageOutputFormat,       // VkFormat                        format
        vk::VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits           samples
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              loadOp
        vk::VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp             storeOp
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              stencilLoadOp
        vk::VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             stencilStoreOp
        vk::VK_IMAGE_LAYOUT_GENERAL,          // VkImageLayout                   initialLayout
        vk::VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout                   finalLayout
    };

    const vk::VkAttachmentReference colorAttachmentRef = {
        0u,                         // uint32_t         attachment
        vk::VK_IMAGE_LAYOUT_GENERAL // VkImageLayout    layout
    };

    const vk::VkSubpassDescription subpassDescription = {
        (vk::VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags       flags
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
        0u,                                  // uint32_t                        inputAttachmentCount
        nullptr,                             // const VkAttachmentReference*    pInputAttachments
        1u,                                  // uint32_t                        colorAttachmentCount
        &colorAttachmentRef,                 // const VkAttachmentReference*    pColorAttachments
        nullptr,                             // const VkAttachmentReference*    pResolveAttachments
        nullptr,                             // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                  // uint32_t                        preserveAttachmentCount
        nullptr                              // const uint32_t*                 pPreserveAttachments
    };

    Move<vk::VkRenderPass> renderPass;
    Move<vk::VkFramebuffer> framebuffer;
    if (!m_parameters.dynamicRendering && m_parameters.command == DRAW)
    {
        const vk::VkRenderPassCreateInfo renderPassInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkRenderPassCreateFlags flags;
            1u,                                            // uint32_t attachmentCount;
            &colorAttachmentDescription,                   // const VkAttachmentDescription* pAttachments;
            1u,                                            // uint32_t subpassCount;
            &subpassDescription,                           // const VkSubpassDescription* pSubpasses;
            0u,                                            // uint32_t dependencyCount;
            nullptr,                                       // const VkSubpassDependency* pDependencies;
        };
        renderPass  = createRenderPass(vk, device, &renderPassInfo);
        framebuffer = makeFramebuffer(vk, device, *renderPass, *outputImageView, renderArea.extent.width,
                                      renderArea.extent.height);
    }

    const std::vector<vk::VkViewport> viewports{makeViewport(renderArea.extent)};
    const std::vector<vk::VkRect2D> scissors{makeRect2D(renderArea.extent)};

    vk::ShaderWrapper vert = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
    vk::ShaderWrapper frag = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

    DescriptorSetLayoutBuilder descriptorBuilder;
    descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                       vk::VK_SHADER_STAGE_FRAGMENT_BIT | vk::VK_SHADER_STAGE_COMPUTE_BIT);
    if (m_parameters.command == DISPATCH)
        descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);

    const auto descriptorSetLayout(descriptorBuilder.build(vk, device));
    const vk::PipelineLayoutWrapper pipelineLayout(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device,
                                                   *descriptorSetLayout);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    if (m_parameters.command == DISPATCH)
        poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    const Move<vk::VkDescriptorPool> descriptorPool =
        poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const Move<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::VkSamplerCreateInfo samplerParams = {
        vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkSamplerCreateFlags flags;
        vk::VK_FILTER_NEAREST,                       // VkFilter magFilter;
        vk::VK_FILTER_NEAREST,                       // VkFilter minFilter;
        vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // VkSamplerAddressMode addressModeU;
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // VkSamplerAddressMode addressModeV;
        vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, // VkSamplerAddressMode addressModeW;
        0.0f,                                        // float mipLodBias;
        VK_FALSE,                                    // VkBool32 anisotropyEnable;
        0.0f,                                        // float maxAnisotropy;
        VK_FALSE,                                    // VkBool32 compareEnable;
        vk::VK_COMPARE_OP_NEVER,                     // VkCompareOp compareOp;
        0.0f,                                        // float minLod;
        VK_LOD_CLAMP_NONE,                           // float maxLod;
        vk::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,      // VkBorderColor borderColor;
        VK_FALSE                                     // VkBool32 unnormalizedCoordinates;
    };
    const vk::Move<vk::VkSampler> sampler = createSampler(vk, device, &samplerParams);
    vk::VkDescriptorImageInfo descriptorSrcImageInfo(
        makeDescriptorImageInfo(*sampler, *sampledImageView, vk::VK_IMAGE_LAYOUT_GENERAL));
    const vk::VkDescriptorImageInfo descriptorDstImageInfo(
        makeDescriptorImageInfo(*sampler, *outputImageView, vk::VK_IMAGE_LAYOUT_GENERAL));

    const vk::VkPipelineVertexInputStateCreateInfo vertexInput = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                       // const void* pNext;
        0u,                                                            // VkPipelineVertexInputStateCreateFlags flags;
        0u,                                                            // uint32_t vertexBindingDescriptionCount;
        nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        0u,      // uint32_t vertexAttributeDescriptionCount;
        nullptr, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    GraphicsPipelineWrapper pipeline(vki, vk, physicalDevice, device, deviceExtensions,
                                     vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
    Move<vk::VkPipeline> computePipeline;

    if (m_parameters.command == DRAW)
    {
        vk::VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo;
        if (m_parameters.dynamicRendering)
        {
            pipelineRenderingCreateInfo = {
                vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType    sType
                nullptr,                                              // const void*        pNext
                0u,                                                   // uint32_t            viewMask
                1u,                                                   // uint32_t            colorAttachmentCount
                &m_parameters.imageOutputFormat,                      // const VkFormat*    pColorAttachmentFormats
                vk::VK_FORMAT_UNDEFINED,                              // VkFormat            depthAttachmentFormat
                vk::VK_FORMAT_UNDEFINED                               // VkFormat            stencilAttachmentFormat
            };
        }

        vk::PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
        renderingCreateInfoWrapper.ptr = m_parameters.dynamicRendering ? &pipelineRenderingCreateInfo : nullptr;

        pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setupVertexInputState(&vertexInput)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vert, nullptr, {},
                                              {}, {}, nullptr, nullptr, renderingCreateInfoWrapper)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, frag)
            .setupFragmentOutputState(*renderPass)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();
    }
    else
    {
        const Unique<vk::VkShaderModule> cs(
            vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
        const vk::VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // sType
            nullptr,                                                 // pNext
            (VkPipelineShaderStageCreateFlags)0u,                    // flags
            vk::VK_SHADER_STAGE_COMPUTE_BIT,                         // stage
            *cs,                                                     // module
            "main",                                                  // pName
            nullptr,                                                 // pSpecializationInfo
        };
        const vk::VkComputePipelineCreateInfo pipelineCreateInfo = {
            vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // sType
            nullptr,                                            // pNext
            (VkPipelineCreateFlags)0u,                          // flags
            pipelineShaderStageParams,                          // stage
            *pipelineLayout,                                    // layout
            VK_NULL_HANDLE,                                     // basePipelineHandle
            0,                                                  // basePipelineIndex
        };
        computePipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    de::MovePtr<BufferWithMemory> colorOutputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(outputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    // Load sampled image
    if (m_parameters.action == MEMORY_TO_IMAGE)
    {
        transitionImageLayout(&cmdBuffer, sampledImage, sampledImageUsage, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                              m_parameters.dstLayout, sampledSubresourceRange);
        commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage << " to layout "
                    << getImageLayoutStr(m_parameters.dstLayout).toString() << "\n";

        copyMemoryToImage(testData, sampledImage, sampledChannelSize * sampledNumChannels, sampledSubresourceLayers, 0,
                          0, mipImageSize.width, mipImageSize.height);
        commandsLog << "vkCopyMemoryToImageEXT() with image " << sampledImage << ", xOffset (0), yOffset (0), width ("
                    << mipImageSize.width << "), height (" << mipImageSize.height << ")\n";

        de::Random randomGen(deInt32Hash((uint32_t)m_parameters.imageSampledFormat) ^
                             deInt32Hash((uint32_t)mipImageSize.width) ^ deInt32Hash((uint32_t)mipImageSize.height) ^
                             deInt32Hash((uint32_t)mipImageSize.depth));
        for (int i = 0; i < 20; ++i)
        {
            int32_t xOffset = randomGen.getInt32() % (mipImageSize.width / 2);
            int32_t yOffset = randomGen.getInt32() % (mipImageSize.height / 2);
            uint32_t width  = deMaxu32(randomGen.getUint32() % (mipImageSize.width / 2), 1u);
            uint32_t height = deMaxu32(randomGen.getUint32() % (mipImageSize.height / 2), 1u);

            if (isCompressedFormat(m_parameters.imageSampledFormat))
            {
                uint32_t blockWidth  = getBlockWidth(m_parameters.imageSampledFormat);
                uint32_t blockHeight = getBlockHeight(m_parameters.imageSampledFormat);
                xOffset              = (xOffset / blockWidth) * blockWidth;
                yOffset              = (yOffset / blockHeight) * blockHeight;
                width                = deMaxu32((width / blockWidth) * blockWidth, blockWidth);
                height               = deMaxu32((height / blockHeight) * blockHeight, blockHeight);
            }

            copyMemoryToImage(testData, sampledImage, sampledChannelSize * sampledNumChannels, sampledSubresourceLayers,
                              xOffset, yOffset, width, height);
            commandsLog << "vkCopyMemoryToImageEXT() with image " << sampledImage << ", xOffset (" << xOffset
                        << "), yOffset (" << yOffset << "), width (" << width << "), height (" << height << ")\n";
        }

        if (m_parameters.dstLayout != vk::VK_IMAGE_LAYOUT_GENERAL)
        {
            transitionImageLayout(&cmdBuffer, sampledImage, sampledImageUsage, m_parameters.dstLayout,
                                  vk::VK_IMAGE_LAYOUT_GENERAL, sampledSubresourceRange);
            commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage
                        << " to layout VK_IMAGE_LAYOUT_GENERAL\n";
        }
    }
    else
    {
        de::MovePtr<BufferWithMemory> sampledBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, alloc,
                                 makeBufferCreateInfo(sampledBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                             vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                 MemoryRequirement::HostVisible));

        auto &bufferAlloc = sampledBuffer->getAllocation();
        memcpy(bufferAlloc.getHostPtr(), testData.data(), sampledBufferSize);
        flushAlloc(vk, device, bufferAlloc);

        transitionImageLayout(&cmdBuffer, sampledImage, sampledImageUsage, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                              m_parameters.dstLayout, sampledSubresourceRange);
        commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage << " to layout"
                    << getImageLayoutStr(m_parameters.dstLayout).toString() << "\n";

        vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
        const vk::VkBufferImageCopy copyRegion = {
            0,
            0,
            0,
            sampledSubresourceLayers,
            imageOffset,
            {
                mipImageSize.width,
                mipImageSize.height,
                mipImageSize.depth,
            },
        };
        vk.cmdCopyBufferToImage(*cmdBuffer, sampledBuffer->get(), sampledImage, m_parameters.dstLayout, 1u,
                                &copyRegion);
        commandsLog << "vkCmdCopyBufferToImage() with image " << sampledImage << ", xOffset ("
                    << copyRegion.imageOffset.x << "), yOffset (" << copyRegion.imageOffset.y << "), width ("
                    << mipImageSize.width << "), height (" << mipImageSize.height << ")\n";

        {
            auto imageMemoryBarrier = makeImageMemoryBarrier(
                vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, m_parameters.dstLayout,
                m_parameters.intermediateLayout, sampledImage, sampledSubresourceRange);
            vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0u, 0u, nullptr, 0u, nullptr, 1, &imageMemoryBarrier);
        }

        vk::endCommandBuffer(vk, *cmdBuffer);
        uint32_t semaphoreCount         = 0;
        vk::VkSemaphore semaphore       = VK_NULL_HANDLE;
        VkPipelineStageFlags waitStages = 0;
        if (m_parameters.sparse)
        {
            semaphoreCount = 1;
            semaphore      = sparseSampledImage->getSemaphore();
            waitStages     = vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer, false, 1u, semaphoreCount, &semaphore, &waitStages);

        if (m_parameters.intermediateLayout != vk::VK_IMAGE_LAYOUT_GENERAL)
        {
            transitionImageLayout(&cmdBuffer, sampledImage, sampledImageUsage, m_parameters.intermediateLayout,
                                  vk::VK_IMAGE_LAYOUT_GENERAL, sampledSubresourceRange);
            commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage
                        << " to layout VK_IMAGE_LAYOUT_GENERAL\n";
        }
    }

    if (m_parameters.action == MEMCPY)
    {
        vk::VkImageSubresource2EXT subresource2 = {
            vk::VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT, // VkStructureType sType;
            nullptr,                                       // void* pNext;
            sampledSubresource                             // VkImageSubresource imageSubresource;
        };

        vk::VkSubresourceHostMemcpySizeEXT subresourceHostMemcpySize = vk::initVulkanStructure();
        vk::VkSubresourceLayout2EXT subresourceLayout = vk::initVulkanStructure(&subresourceHostMemcpySize);
        vk.getImageSubresourceLayout2KHR(device, sampledImage, &subresource2, &subresourceLayout);

        std::vector<uint8_t> data((size_t)subresourceHostMemcpySize.size);

        const vk::VkImageToMemoryCopyEXT region = {
            vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            data.data(),                                    // void* memoryHostPointer;
            0u,                                             // uint32_t memoryRowLength;
            0u,                                             // uint32_t memoryImageHeight;
            sampledSubresourceLayers,                       // VkImageSubresourceLayers imageSubresource;
            imageOffset,                                    // VkOffset3D imageOffset;
            mipImageSize,                                   // VkExtent3D imageExtent;
        };

        const vk::VkCopyImageToMemoryInfoEXT copyImageToMemoryInfo = {
            vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            vk::VK_HOST_IMAGE_COPY_MEMCPY_EXT,                   // VkMemoryImageCopyFlagsEXT flags;
            sampledImage,                                        // VkImage srcImage;
            vk::VK_IMAGE_LAYOUT_GENERAL,                         // VkImageLayout srcImageLayout;
            1,                                                   // uint32_t regionCount;
            &region,                                             // const VkImageToMemoryCopyEXT* pRegions;
        };
        vk.copyImageToMemoryEXT(device, &copyImageToMemoryInfo);
        commandsLog << "vkCopyImageToMemoryEXT() with image " << sampledImage << ", xOffset (" << region.imageOffset.x
                    << "), yOffset (" << region.imageOffset.y << "), width (" << mipImageSize.width << "), height ("
                    << mipImageSize.height << ")\n";

        transitionImageLayout(&cmdBuffer, sampledImageCopy, sampledImageUsage, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                              m_parameters.dstLayout, sampledSubresourceRange);

        const vk::VkMemoryToImageCopyEXT toImageRegion = {
            vk::VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            data.data(),                                    // const void* memoryHostPointer;
            0,                                              // uint32_t memoryRowLength;
            0,                                              // uint32_t memoryImageHeight;
            sampledSubresourceLayers,                       // VkImageSubresourceLayers imageSubresource;
            imageOffset,                                    // VkOffset3D imageOffset;
            mipImageSize                                    // VkExtent3D imageExtent;
        };

        vk::VkCopyMemoryToImageInfoEXT copyMemoryToImageInfo = {
            vk::VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            vk::VK_HOST_IMAGE_COPY_MEMCPY_EXT,                   // VkMemoryImageCopyFlagsEXT flags;
            sampledImageCopy,                                    // VkImage dstImage;
            m_parameters.dstLayout,                              // VkImageLayout dstImageLayout;
            1u,                                                  // uint32_t regionCount;
            &toImageRegion,                                      // const VkMemoryToImageCopyEXT* pRegions;
        };
        vk.copyMemoryToImageEXT(device, &copyMemoryToImageInfo);
        commandsLog << "vkCopyMemoryToImageEXT() with image " << sampledImageCopy << ", xOffset ("
                    << toImageRegion.imageOffset.x << "), yOffset (" << toImageRegion.imageOffset.y << "), width ("
                    << toImageRegion.imageExtent.width << "), height (" << toImageRegion.imageExtent.height << ")\n";
        descriptorSrcImageInfo.imageView = *sampledImageViewCopy;

        transitionImageLayout(&cmdBuffer, sampledImageCopy, sampledImageUsage, m_parameters.dstLayout,
                              vk::VK_IMAGE_LAYOUT_GENERAL, sampledSubresourceRange);
    }

    // Transition output image
    transitionImageLayout(&cmdBuffer, **outputImage, outputImageUsage, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                          vk::VK_IMAGE_LAYOUT_GENERAL, outputSubresourceRange);
    commandsLog << "vkTransitionImageLayoutEXT() image " << **outputImage << " to layout VK_IMAGE_LAYOUT_GENERAL\n";
    vk::beginCommandBuffer(vk, *cmdBuffer, 0u);

    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorSrcImageInfo);
    if (m_parameters.command == DISPATCH)
        updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                                  vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorDstImageInfo);
    updateBuilder.update(vk, device);

    if (m_parameters.command == DRAW)
    {
        if (m_parameters.dynamicRendering)
            beginRendering(vk, *cmdBuffer, *outputImageView, renderArea, vk::VkClearValue(),
                           vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        else
            beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea);

        vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
        vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1,
                                 &*descriptorSet, 0, nullptr);
        vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
        commandsLog << "vkCmdDraw()\n";

        if (m_parameters.dynamicRendering)
            endRendering(vk, *cmdBuffer);
        else
            endRenderPass(vk, *cmdBuffer);

        const auto postImageBarrier = makeImageMemoryBarrier(
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL,
            m_parameters.srcLayout, **outputImage, outputSubresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                              &postImageBarrier);
    }
    else
    {
        const auto imageMemoryBarrier =
            makeImageMemoryBarrier(0u, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, **outputImage, outputSubresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0u, nullptr, 0u, nullptr, 1u,
                              &imageMemoryBarrier);
        vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &*descriptorSet,
                                 0, nullptr);
        vk.cmdDispatch(*cmdBuffer, renderArea.extent.width, renderArea.extent.height, 1);
        commandsLog << "vkCmdDispatch()\n";

        vk::VkImageMemoryBarrier postImageBarrier = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
            nullptr,                                    // const void*                pNext
            vk::VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags            srcAccessMask
            vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags            dstAccessMask
            vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout            oldLayout
            m_parameters.srcLayout,                     // VkImageLayout            newLayout
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                    srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t                    dstQueueFamilyIndex
            **outputImage,                              // VkImage                    image
            outputSubresourceRange                      // VkImageSubresourceRange    subresourceRange
        };

        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);
    }

    const vk::VkBufferImageCopy copyRegion = {
        0u,                                                    // VkDeviceSize bufferOffset;
        0u,                                                    // uint32_t bufferRowLength;
        0u,                                                    // uint32_t bufferImageHeight;
        outputSubresourceLayers,                               // VkImageSubresourceLayers imageSubresource;
        imageOffset,                                           // VkOffset3D imageOffset;
        {renderArea.extent.width, renderArea.extent.height, 1} // VkExtent3D imageExtent;
    };
    vk.cmdCopyImageToBuffer(*cmdBuffer, **outputImage, m_parameters.srcLayout, **colorOutputBuffer, 1u, &copyRegion);
    commandsLog << "vkCmdCopyImageToBuffer() with image " << **outputImage << ", xOffset (" << imageOffset.x
                << "), yOffset (" << imageOffset.y << "), width (" << renderArea.extent.width << "), height ("
                << renderArea.extent.height << "\n";
    vk::endCommandBuffer(vk, *cmdBuffer);

    vk::submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Verify image
    tcu::ConstPixelBufferAccess resultBuffer =
        tcu::ConstPixelBufferAccess(outputFormat, renderArea.extent.width, renderArea.extent.height, 1,
                                    (const void *)colorOutputBuffer->getAllocation().getHostPtr());

    if (m_parameters.action == IMAGE_TO_MEMORY)
    {
        const uint32_t paddedBufferSize = (mipImageSize.width + m_parameters.padding) *
                                          (mipImageSize.height + m_parameters.padding) * outputNumChannels *
                                          outputChannelSize;
        const uint32_t memoryRowLength   = (mipImageSize.width + m_parameters.padding);
        const uint32_t memoryImageHeight = (mipImageSize.height + m_parameters.padding);
        std::vector<uint8_t> paddedData(paddedBufferSize);
        std::vector<uint8_t> data(outputBufferSize);

        std::vector<vk::VkImageToMemoryCopyEXT> regions(m_parameters.regionsCount);

        for (uint32_t i = 0; i < m_parameters.regionsCount; ++i)
        {
            vk::VkOffset3D offset = {0, (int32_t)(mipImageSize.height / m_parameters.regionsCount * i), 0};
            vk::VkExtent3D extent = {mipImageSize.width, mipImageSize.height / m_parameters.regionsCount, 1};
            if (i == m_parameters.regionsCount - 1)
                extent.height = mipImageSize.height - mipImageSize.height / m_parameters.regionsCount * i;

            uint32_t dataOffset =
                (mipImageSize.width + m_parameters.padding) * offset.y * outputNumChannels * outputChannelSize;

            const vk::VkImageToMemoryCopyEXT region = {
                vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT, // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                &paddedData[dataOffset],                        // void* memoryHostPointer;
                memoryRowLength,                                // uint32_t memoryRowLength;
                memoryImageHeight,                              // uint32_t memoryImageHeight;
                outputSubresourceLayers,                        // VkImageSubresourceLayers imageSubresource;
                offset,                                         // VkOffset3D imageOffset;
                extent,                                         // VkExtent3D imageExtent;
            };

            regions[i] = region;
        }

        const vk::VkCopyImageToMemoryInfoEXT copyImageToMemoryInfo = {
            vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0u,                                                  // VkMemoryImageCopyFlagsEXT flags;
            **outputImage,                                       // VkImage srcImage;
            m_parameters.srcLayout,                              // VkImageLayout srcImageLayout;
            (uint32_t)regions.size(),                            // uint32_t regionCount;
            regions.data(),                                      // const VkImageToMemoryCopyEXT* pRegions;
        };
        vk.copyImageToMemoryEXT(device, &copyImageToMemoryInfo);
        commandsLog << "vkCopyImageToMemoryEXT() with image " << **outputImage << "\n";

        for (uint32_t j = 0; j < mipImageSize.height; ++j)
        {
            for (uint32_t i = 0; i < mipImageSize.width; ++i)
            {
                for (uint32_t k = 0; k < outputNumChannels * outputChannelSize; ++k)
                {
                    uint32_t dstIndex = j * mipImageSize.width * (outputNumChannels * outputChannelSize) +
                                        i * (outputNumChannels * outputChannelSize) + k;
                    uint32_t srcIndex =
                        j * (mipImageSize.width + m_parameters.padding) * (outputNumChannels * outputChannelSize) +
                        i * (outputNumChannels * outputChannelSize) + k;
                    data[dstIndex] = paddedData[srcIndex];
                }
            }
        }

        bool match = true;
        if (m_parameters.imageOutputFormat == VK_FORMAT_R10X6_UNORM_PACK16)
        {
            for (uint32_t i = 0; i < outputBufferSize / 2; ++i)
            {
                uint16_t ref    = ((uint16_t *)data.data())[i];
                uint16_t result = ((uint16_t *)resultBuffer.getDataPtr())[i];
                if ((ref & 0xffc0) != (result & 0xffc0))
                    match = false;
            }
        }
        else
        {
            match = memcmp(data.data(), resultBuffer.getDataPtr(), outputBufferSize) == 0;
        }
        if (!match)
        {
            log << tcu::TestLog::Message << commandsLog.str() << tcu::TestLog::EndMessage;
            for (uint32_t i = 0; i < outputBufferSize; ++i)
            {
                if (data[i] != ((uint8_t *)resultBuffer.getDataPtr())[i])
                {
                    log << tcu::TestLog::Message << "At byte " << i << " data from vkCopyImageToMemoryEXT() is "
                        << data[i] << ", but data from vkCmdCopyImageToBuffer() (after padding) is "
                        << ((uint8_t *)resultBuffer.getDataPtr())[i] << tcu::TestLog::EndMessage;
                    break;
                }
            }
            return tcu::TestStatus::fail("copyImageToMemoryEXT failed");
        }
    }

    if (m_parameters.imageOutputFormat == m_parameters.imageSampledFormat)
    {
        std::vector<uint8_t> resultData(sampledBufferSize);
        const Allocation &outputAlloc = colorOutputBuffer->getAllocation();
        deMemcpy(resultData.data(), outputAlloc.getHostPtr(), sampledBufferSize);

        bool match = true;
        if (m_parameters.imageOutputFormat == VK_FORMAT_R10X6_UNORM_PACK16)
        {
            for (uint32_t i = 0; i < sampledBufferSize / 2; ++i)
            {
                uint16_t ref    = ((uint16_t *)testData.data())[i];
                uint16_t result = ((uint16_t *)resultData.data())[i];
                if ((ref & 0xffc0) != (result & 0xffc0))
                {
                    match = false;
                    break;
                }
            }
        }
        else
        {
            for (uint32_t i = 0; i < sampledBufferSize; ++i)
            {
                if (resultData[i] != testData[i])
                {
                    match = false;
                    break;
                }
            }
        }
        if (!match)
        {
            if (!isCompressedFormat(m_parameters.imageSampledFormat))
            {
                const tcu::ConstPixelBufferAccess bufferData(
                    mapVkFormat(m_parameters.imageSampledFormat), m_parameters.imageSize.width,
                    m_parameters.imageSize.height, m_parameters.imageSize.depth, outputAlloc.getHostPtr());

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Section("host_copy_result", "host_copy_result")
                    << tcu::LogImage("image", "", bufferData) << tcu::TestLog::EndSection;
            }
            return tcu::TestStatus::fail("Image verification failed");
        }
    }
    return tcu::TestStatus::pass("Pass");
}

class HostImageCopyTestCase : public vkt::TestCase
{
public:
    HostImageCopyTestCase(tcu::TestContext &context, const char *name, const TestParameters &parameters)
        : TestCase(context, name)
        , m_parameters(parameters)
    {
    }

private:
    void checkSupport(vkt::Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new HostImageCopyTestInstance(context, m_parameters);
    }

    const TestParameters m_parameters;
};

void HostImageCopyTestCase::checkSupport(vkt::Context &context) const
{
    vk::VkInstance instance(context.getInstance());
    vk::InstanceDriver instanceDriver(context.getPlatformInterface(), instance);
    const vk::InstanceInterface &vki    = context.getInstanceInterface();
    vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_EXT_host_image_copy");

    if (m_parameters.dynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (m_parameters.sparse)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

    vk::VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT, // VkStructureType                    sType
        nullptr,                                                            // const void*                        pNext
        VK_FALSE,                                                           // VkBool32 hostImageCopy;
    };

    vk::VkPhysicalDeviceFeatures features;
    deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));
    vk::VkPhysicalDeviceFeatures2 features2 = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // VkStructureType                    sType
        &hostImageCopyFeatures,                           // const void*                        pNext
        features                                          // VkPhysicalDeviceFeatures            features
    };

    instanceDriver.getPhysicalDeviceFeatures2(physicalDevice, &features2);

    vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT, // VkStructureType sType;
        nullptr,                                                              // void* pNext;
        0u,                                                                   // uint32_t copySrcLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopySrcLayouts;
        0u,                                                                   // uint32_t copyDstLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopyDstLayouts;
        {},   // uint8_t optimalTilingLayoutUUID[VK_UUID_SIZE];
        false // VkBool32 identicalMemoryTypeRequirements;
    };
    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
    std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
    std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
    hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
    hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
    bool layoutSupported             = false;
    bool intermediateLayoutSupported = false;
    for (uint32_t i = 0; i < hostImageCopyProperties.copySrcLayoutCount; ++i)
    {
        if (hostImageCopyProperties.pCopySrcLayouts[i] == m_parameters.srcLayout)
            layoutSupported = true;
        if (hostImageCopyProperties.pCopySrcLayouts[i] == m_parameters.intermediateLayout)
            intermediateLayoutSupported = true;
    }
    if (layoutSupported == false || intermediateLayoutSupported == false)
        TCU_THROW(NotSupportedError, "Layout not supported for src host copy");
    layoutSupported = false;
    for (uint32_t i = 0; i < hostImageCopyProperties.copyDstLayoutCount; ++i)
    {
        if (hostImageCopyProperties.pCopyDstLayouts[i] == m_parameters.dstLayout)
        {
            layoutSupported = true;
            break;
        }
    }
    if (layoutSupported == false)
        TCU_THROW(NotSupportedError, "Layout not supported for dst host copy");

    vk::VkImageUsageFlags usage = vk::VK_IMAGE_USAGE_SAMPLED_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (m_parameters.action == MEMORY_TO_IMAGE || m_parameters.action == MEMCPY)
        usage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
    if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        usage |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        usage |= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        usage |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    vk::VkImageCreateFlags flags = 0u;
    if (m_parameters.sparse)
        flags |= vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    vk::VkImageFormatProperties imageFormatProperties;
    if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.imageSampledFormat,
                                                   vk::VK_IMAGE_TYPE_2D, m_parameters.sampledTiling, usage, flags,
                                                   &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Image format not supported.");

    vk::VkImageUsageFlags outputUsage = vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (m_parameters.action == IMAGE_TO_MEMORY)
        outputUsage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
    if (m_parameters.command == DISPATCH)
        outputUsage |= vk::VK_IMAGE_USAGE_STORAGE_BIT;
    vk::VkImageFormatProperties outputImageFormatProperties;
    if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.imageOutputFormat, vk::VK_IMAGE_TYPE_2D,
                                                   vk::VK_IMAGE_TILING_OPTIMAL, outputUsage, flags,
                                                   &outputImageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Image format not supported.");

    vk::VkFormatProperties3 formatProperties3 = vk::initVulkanStructure();
    vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
    vki.getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), m_parameters.imageSampledFormat,
                                           &formatProperties2);
    if (m_parameters.sampledTiling == vk::VK_IMAGE_TILING_LINEAR &&
        (formatProperties3.linearTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
        TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for linear tiling.");
    if (m_parameters.sampledTiling == vk::VK_IMAGE_TILING_OPTIMAL &&
        (formatProperties3.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
        TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for optimal tiling.");

    if (hostImageCopyFeatures.hostImageCopy != VK_TRUE)
        TCU_THROW(NotSupportedError, "hostImageCopy not supported");
    if (imageFormatProperties.maxMipLevels <= m_parameters.mipLevel)
        TCU_THROW(NotSupportedError, "Required image mip levels not supported.");

    if (m_parameters.command == DISPATCH)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT);
}

void HostImageCopyTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    {
        std::ostringstream vert;
        vert << "#version 450\n"
             << "layout (location=0) out vec2 texCoord;\n"
             << "void main()\n"
             << "{\n"
             << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);"
             << "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
             << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    }
    {
        std::string output;
        if (isDepthStencilFormat(m_parameters.imageSampledFormat))
            output = "    out_color = vec4(texture(combinedSampler, texCoord).r, 0, 0, 0);\n";
        else
            output = "    out_color = texture(combinedSampler, texCoord);\n";

        std::ostringstream frag;
        frag << "#version 450\n"
             << "layout (location=0) out vec4 out_color;\n"
             << "layout (location=0) in vec2 texCoord;\n"
             << "layout (set=0, binding=0) uniform sampler2D combinedSampler;\n"
             << "void main()\n"
             << "{\n"
             << output << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
    {
        std::string image;
        std::string output;
        if (m_parameters.imageOutputFormat == vk::VK_FORMAT_R8G8B8A8_UINT)
        {
            image  = "uimage2D";
            output = "uvec4(texture(combinedSampler, vec2(pixelCoord) / (textureSize(combinedSampler, 0) - "
                     "vec2(0.001f))) * 255)";
        }
        else
        {
            image  = "image2D";
            output = "texture(combinedSampler, vec2(pixelCoord) / (textureSize(combinedSampler, 0) - vec2(0.001f)))";
        }

        std::ostringstream comp;
        comp << "#version 450\n"
             << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout (set=0, binding=0) uniform sampler2D combinedSampler;\n"
             << "layout (set=0, binding=1) uniform writeonly " << image << " outImage;\n"
             << "void main()\n"
             << "{\n"
             << "    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);\n"
             << "    imageStore(outImage, pixelCoord, " << output << ");\n"
             << "}\n";

        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
}

class PreinitializedTestInstance : public vkt::TestInstance
{
public:
    PreinitializedTestInstance(vkt::Context &context, const vk::VkFormat format, vk::VkImageLayout srcLayout,
                               vk::VkImageLayout dstLayout, vk::VkExtent3D size, uint32_t arrayLayers,
                               bool imageToImageCopy, bool memcpy, vk::VkImageTiling tiling, uint32_t offset)
        : vkt::TestInstance(context)
        , m_format(format)
        , m_srcLayout(srcLayout)
        , m_dstLayout(dstLayout)
        , m_size(size)
        , m_arrayLayers(arrayLayers)
        , m_imageToImageCopy(imageToImageCopy)
        , m_memcpy(memcpy)
        , m_tiling(tiling)
        , m_offset(offset)
    {
    }

private:
    tcu::TestStatus iterate(void);

    const vk::VkFormat m_format;
    const vk::VkImageLayout m_srcLayout;
    const vk::VkImageLayout m_dstLayout;
    const vk::VkExtent3D m_size;
    const uint32_t m_arrayLayers;
    const bool m_imageToImageCopy;
    const bool m_memcpy;
    const vk::VkImageTiling m_tiling;
    const uint32_t m_offset;
};

tcu::TestStatus PreinitializedTestInstance::iterate(void)
{
    vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), m_context.getInstance());
    vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const vk::VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    const vk::VkQueue queue             = m_context.getUniversalQueue();
    auto &alloc                         = m_context.getDefaultAllocator();
    tcu::TestLog &log                   = m_context.getTestContext().getLog();

    const auto subresourceRange  = makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_arrayLayers);
    const auto subresourceLayers = makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_arrayLayers);
    const vk::VkOffset3D offset  = {0u, 0u, 0u};

    const auto channelSize = getChannelSize(m_format);
    const auto numChannels = getNumChannels(m_format);
    const auto bufferCount = m_size.width * m_size.height * m_size.depth * m_arrayLayers * numChannels;
    const auto bufferSize  = bufferCount * channelSize;

    const vk::VkCommandPoolCreateInfo cmdPoolInfo = {
        vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,      // sType
        nullptr,                                             // pNext
        vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // flags
        queueFamilyIndex,                                    // queuefamilyindex
    };
    const Move<vk::VkCommandPool> cmdPool(createCommandPool(vk, device, &cmdPoolInfo));
    const Move<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    const vk::SimpleAllocator::OptionalOffsetParams offsetParams(
        {m_context.getDeviceProperties().limits.nonCoherentAtomSize, m_offset});
    de::MovePtr<Allocator> allocatorWithOffset = de::MovePtr<Allocator>(new SimpleAllocator(
        vk, device, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()),
        offsetParams));

    const vk::VkImageType imageType = m_size.depth > 1 ? vk::VK_IMAGE_TYPE_3D : vk::VK_IMAGE_TYPE_2D;

    uint64_t modifier = 0;
    checkSupportedFormatFeatures(instanceDriver, physicalDevice, m_format, m_tiling, &modifier);

    vk::VkImageDrmFormatModifierListCreateInfoEXT drmCreateInfo = vk::initVulkanStructure();
    drmCreateInfo.drmFormatModifierCount                        = 1;
    drmCreateInfo.pDrmFormatModifiers                           = &modifier;

    vk::VkImageCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
        m_tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ? &drmCreateInfo : nullptr,
        // const void*                pNext
        0u,                        // VkImageCreateFlags        flags
        imageType,                 // VkImageType                imageType
        m_format,                  // VkFormat                    format
        m_size,                    // VkExtent3D                extent
        1u,                        // uint32_t                    mipLevels
        m_arrayLayers,             // uint32_t                    arrayLayers
        vk::VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits    samples
        m_tiling,                  // VkImageTiling            tiling
        vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        // VkImageUsageFlags        usage
        vk::VK_SHARING_MODE_EXCLUSIVE,     // VkSharingMode            sharingMode
        0,                                 // uint32_t                    queueFamilyIndexCount
        nullptr,                           // const uint32_t*            pQueueFamilyIndices
        vk::VK_IMAGE_LAYOUT_PREINITIALIZED // VkImageLayout            initialLayout
    };

    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        createInfo.usage |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        createInfo.usage |= vk::VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        createInfo.usage |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    de::MovePtr<ImageWithMemory> image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, *allocatorWithOffset, createInfo, vk::MemoryRequirement::HostVisible));
    de::MovePtr<ImageWithMemory> copyImage = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, *allocatorWithOffset, createInfo, vk::MemoryRequirement::Any));
    const vk::VkImage endImage                 = m_imageToImageCopy ? **copyImage : **image;
    de::MovePtr<BufferWithMemory> outputBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                             MemoryRequirement::HostVisible));

    vk::Allocation &allocation = image->getAllocation();
    void *ptr                  = allocation.getHostPtr();
    generateData(ptr, bufferSize, m_format);

    vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT, // VkStructureType sType;
        nullptr,                                                              // void* pNext;
        0u,                                                                   // uint32_t copySrcLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopySrcLayouts;
        0u,                                                                   // uint32_t copyDstLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopyDstLayouts;
        {},    // uint8_t optimalTilingLayoutUUID[VK_UUID_SIZE];
        false, // VkBool32 identicalMemoryTypeRequirements;
    };
    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
    if (hostImageCopyProperties.identicalMemoryTypeRequirements)
    {
        createInfo.flags &= ~(vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT);
        de::MovePtr<ImageWithMemory> imageWithoutHostCopy = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::HostVisible));
        vk::VkMemoryRequirements hostImageMemoryRequirements;
        vk::VkMemoryRequirements memoryRequirements;
        vk.getImageMemoryRequirements(device, **image, &hostImageMemoryRequirements);
        vk.getImageMemoryRequirements(device, **imageWithoutHostCopy, &memoryRequirements);

        if (hostImageMemoryRequirements.memoryTypeBits != memoryRequirements.memoryTypeBits)
            TCU_THROW(NotSupportedError, "Layout not supported for src host copy");
    }

    // map device memory and initialize
    {
        const vk::VkHostImageLayoutTransitionInfoEXT transition = {
            vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            image->get(),                                                // VkImage image;
            vk::VK_IMAGE_LAYOUT_PREINITIALIZED,                          // VkImageLayout oldLayout;
            m_srcLayout,                                                 // VkImageLayout newLayout;
            subresourceRange                                             // VkImageSubresourceRange subresourceRange;
        };
        vk.transitionImageLayoutEXT(device, 1, &transition);
    }

    if (m_imageToImageCopy)
    {
        vk::VkHostImageLayoutTransitionInfoEXT transition = {
            vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            copyImage->get(),                                            // VkImage image;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                               // VkImageLayout oldLayout;
            m_dstLayout,                                                 // VkImageLayout newLayout;
            subresourceRange                                             // VkImageSubresourceRange subresourceRange;
        };
        vk.transitionImageLayoutEXT(device, 1, &transition);

        const vk::VkImageCopy2KHR region = {
            vk::VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            subresourceLayers,                      // VkImageSubresourceLayers srcSubresource;
            offset,                                 // VkOffset3D srcOffset;
            subresourceLayers,                      // VkImageSubresourceLayers dstSubresource;
            offset,                                 // VkOffset3D dstOffset;
            m_size                                  // VkExtent3D extent;
        };

        const vk::VkHostImageCopyFlagsEXT hostImageCopyFlags =
            m_memcpy ? (vk::VkHostImageCopyFlagsEXT)vk::VK_HOST_IMAGE_COPY_MEMCPY_EXT : (vk::VkHostImageCopyFlagsEXT)0u;

        const vk::VkCopyImageToImageInfoEXT copyImageToImageInfo = {
            vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO_EXT, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            hostImageCopyFlags,                                 // VkHostImageCopyFlagsEXT flags;
            **image,                                            // VkImage srcImage;
            m_srcLayout,                                        // VkImageLayout srcImageLayout;
            **copyImage,                                        // VkImage dstImage;
            m_dstLayout,                                        // VkImageLayout dstImageLayout;
            1u,                                                 // uint32_t regionCount;
            &region,                                            // const VkImageCopy2* pRegions;
        };

        vk.copyImageToImageEXT(device, &copyImageToImageInfo);

        transition.oldLayout = m_dstLayout;
        transition.newLayout = m_srcLayout;
        vk.transitionImageLayoutEXT(device, 1, &transition);
    }

    uint8_t *data = new uint8_t[bufferSize];

    const vk::VkImageToMemoryCopyEXT region = {
        vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        data,                                           // void* memoryHostPointer;
        0u,                                             // uint32_t memoryRowLength;
        0u,                                             // uint32_t memoryImageHeight;
        subresourceLayers,                              // VkImageSubresourceLayers imageSubresource;
        offset,                                         // VkOffset3D imageOffset;
        m_size,                                         // VkExtent3D imageExtent;
    };

    const vk::VkCopyImageToMemoryInfoEXT copyImageToMemoryInfo = {
        vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkMemoryImageCopyFlagsEXT flags;
        endImage,                                            // VkImage srcImage;
        m_srcLayout,                                         // VkImageLayout srcImageLayout;
        1,                                                   // uint32_t regionCount;
        &region,                                             // const VkImageToMemoryCopyEXT* pRegions;
    };
    vk.copyImageToMemoryEXT(device, &copyImageToMemoryInfo);

    vk::beginCommandBuffer(vk, *cmdBuffer);
    {
        auto imageMemoryBarrier =
            makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, m_srcLayout,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **image, subresourceRange);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_NONE, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1, &imageMemoryBarrier);

        const vk::VkBufferImageCopy copyRegion = {
            0u,                // VkDeviceSize bufferOffset;
            0u,                // uint32_t bufferRowLength;
            0u,                // uint32_t bufferImageHeight;
            subresourceLayers, // VkImageSubresourceLayers imageSubresource;
            offset,            // VkOffset3D imageOffset;
            m_size             // VkExtent3D imageExtent;
        };
        vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **outputBuffer, 1u,
                                &copyRegion);
    }
    vk::endCommandBuffer(vk, *cmdBuffer);

    vk::submitCommandsAndWait(vk, device, queue, cmdBuffer.get());
    auto outputPtr = outputBuffer->getAllocation().getHostPtr();
    bool match     = memcmp(data, outputPtr, bufferSize) == 0;

    if (!match)
    {
        for (uint32_t i = 0; i < bufferSize; ++i)
        {
            if (data[i] != ((uint8_t *)outputPtr)[i])
            {
                log << tcu::TestLog::Message << "At byte " << i << " data from vkCopyImageToMemoryEXT() is " << data[i]
                    << ", but data from vkCmdCopyImageToBuffer() is " << ((uint8_t *)outputPtr)[i]
                    << tcu::TestLog::EndMessage;
                break;
            }
        }
    }

    delete[] data;

    if (!match)
    {
        return tcu::TestStatus::fail("Copies values do not match");
    }

    return tcu::TestStatus::pass("Pass");
}

class PreinitializedTestCase : public vkt::TestCase
{
public:
    PreinitializedTestCase(tcu::TestContext &context, const char *name, const vk::VkFormat format,
                           vk::VkImageLayout srcLayout, vk::VkImageLayout dstLayout, vk::VkExtent3D size,
                           uint32_t arrayLayers, bool imageToImageCopy, bool memcpy, vk::VkImageTiling tiling,
                           uint32_t offset)
        : TestCase(context, name)
        , m_format(format)
        , m_srcLayout(srcLayout)
        , m_dstLayout(dstLayout)
        , m_size(size)
        , m_arrayLayers(arrayLayers)
        , m_imageToImageCopy(imageToImageCopy)
        , m_memcpy(memcpy)
        , m_tiling(tiling)
        , m_offset(offset)
    {
    }

private:
    void checkSupport(vkt::Context &context) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new PreinitializedTestInstance(context, m_format, m_srcLayout, m_dstLayout, m_size, m_arrayLayers,
                                              m_imageToImageCopy, m_memcpy, m_tiling, m_offset);
    }

    const vk::VkFormat m_format;
    const vk::VkImageLayout m_srcLayout;
    const vk::VkImageLayout m_dstLayout;
    const vk::VkExtent3D m_size;
    const uint32_t m_arrayLayers;
    const bool m_imageToImageCopy;
    const bool m_memcpy;
    const vk::VkImageTiling m_tiling;
    const uint32_t m_offset;
};

void PreinitializedTestCase::checkSupport(vkt::Context &context) const
{
    vk::VkInstance instance(context.getInstance());
    vk::InstanceDriver instanceDriver(context.getPlatformInterface(), instance);
    const InstanceInterface &vki        = context.getInstanceInterface();
    vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_EXT_host_image_copy");

    if (m_tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
        context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");

    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR || m_dstLayout == vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        context.requireDeviceFunctionality("VK_KHR_swapchain");

    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
        m_dstLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
        m_srcLayout == vk::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR ||
        m_dstLayout == vk::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance2");

    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL ||
        m_dstLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL ||
        m_srcLayout == vk::VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL ||
        m_dstLayout == vk::VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL ||
        m_srcLayout == vk::VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL ||
        m_dstLayout == vk::VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL)
        context.requireDeviceFunctionality("VK_KHR_separate_depth_stencil_layouts");

    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL ||
        m_srcLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
        context.requireDeviceFunctionality("VK_KHR_synchronization2");

    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT ||
        m_dstLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT)
        context.requireDeviceFunctionality("VK_EXT_attachment_feedback_loop_layout");

    vk::VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT, // VkStructureType                    sType
        nullptr,                                                            // const void*                        pNext
        VK_FALSE,                                                           // VkBool32 hostImageCopy;
    };

    vk::VkPhysicalDeviceFeatures features;
    deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));
    vk::VkPhysicalDeviceFeatures2 features2 = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // VkStructureType                    sType
        &hostImageCopyFeatures,                           // const void*                        pNext
        features                                          // VkPhysicalDeviceFeatures            features
    };

    instanceDriver.getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

    vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT, // VkStructureType sType;
        nullptr,                                                              // void* pNext;
        0u,                                                                   // uint32_t copySrcLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopySrcLayouts;
        0u,                                                                   // uint32_t copyDstLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopyDstLayouts;
        {},   // uint8_t optimalTilingLayoutUUID[VK_UUID_SIZE];
        false // VkBool32 identicalMemoryTypeRequirements;
    };

    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
    std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
    std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
    hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
    hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);

    bool layoutSupported = false;
    for (uint32_t i = 0; i < hostImageCopyProperties.copySrcLayoutCount; ++i)
    {
        if (hostImageCopyProperties.pCopySrcLayouts[i] == m_srcLayout)
            layoutSupported = true;
    }
    if (layoutSupported == false)
        TCU_THROW(NotSupportedError, "Layout not supported for src host copy");
    layoutSupported = false;
    for (uint32_t i = 0; i < hostImageCopyProperties.copyDstLayoutCount; ++i)
    {
        if (hostImageCopyProperties.pCopyDstLayouts[i] == m_dstLayout)
            layoutSupported = true;
    }
    if (layoutSupported == false)
        TCU_THROW(NotSupportedError, "Layout not supported for dst host copy");

    if (hostImageCopyFeatures.hostImageCopy != VK_TRUE)
        TCU_THROW(NotSupportedError, "hostImageCopy not supported");

    uint64_t modifier = 0;
    checkSupportedFormatFeatures(instanceDriver, physicalDevice, m_format, m_tiling, &modifier);

    vk::VkImageType const imageType                    = m_size.depth > 1 ? vk::VK_IMAGE_TYPE_3D : vk::VK_IMAGE_TYPE_2D;
    vk::VkImageFormatProperties2 imageFormatProperties = {
        vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2, // VkStructureType sType;
        nullptr,                                         // void* pNext;
        {},                                              // VkImageFormatProperties imageFormatProperties;
    };
    vk::VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT, // VkStructureType sType;
        nullptr,                                                                  // const void* pNext;
        modifier,                                                                 // uint64_t drmFormatModifier;
        VK_SHARING_MODE_EXCLUSIVE,                                                // VkSharingMode sharingMode;
        0u,                                                                       // uint32_t queueFamilyIndexCount;
        nullptr // const uint32_t* pQueueFamilyIndices;
    };

    vk::VkImageUsageFlags usage = vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        usage |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        usage |= vk::VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_srcLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        usage |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    vk::VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,                         // VkStructureType sType;
        m_tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ? &modifierInfo : nullptr, // const void* pNext;
        m_format,                                                                          // VkFormat format;
        imageType,                                                                         // VkImageType type;
        m_tiling,                                                                          // VkImageTiling tiling;
        usage,                                                                             // VkImageUsageFlags usage;
        (vk::VkImageCreateFlags)0u                                                         // VkImageCreateFlags flags;
    };
    if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties) ==
        vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Image format not supported.");
    if (imageFormatProperties.imageFormatProperties.maxArrayLayers < m_arrayLayers)
        TCU_THROW(NotSupportedError, "Required image array layers not supported.");
}

class PropertiesTestInstance : public vkt::TestInstance
{
public:
    PropertiesTestInstance(vkt::Context &context) : vkt::TestInstance(context)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus PropertiesTestInstance::iterate(void)
{
    vk::VkInstance instance(m_context.getInstance());
    vk::InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

    vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT, // VkStructureType sType;
        nullptr,                                                              // void* pNext;
        0u,                                                                   // uint32_t copySrcLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopySrcLayouts;
        0u,                                                                   // uint32_t copyDstLayoutCount;
        nullptr,                                                              // VkImageLayout* pCopyDstLayouts;
        {},   // uint8_t optimalTilingLayoutUUID[VK_UUID_SIZE];
        false // VkBool32 identicalMemoryTypeRequirements;
    };
    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
    std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
    std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
    hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
    hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
    getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);

    if (hostImageCopyProperties.copySrcLayoutCount == 0)
        return tcu::TestStatus::fail("copySrcLayoutCount is 0");
    if (hostImageCopyProperties.copyDstLayoutCount == 0)
        return tcu::TestStatus::fail("copyDstLayoutCount is 0");

    bool layoutSupported = false;
    for (uint32_t i = 0; i < hostImageCopyProperties.copySrcLayoutCount; ++i)
    {
        if (hostImageCopyProperties.pCopySrcLayouts[i] == vk::VK_IMAGE_LAYOUT_GENERAL)
            layoutSupported = true;
    }
    if (layoutSupported == false)
        return tcu::TestStatus::fail("VK_IMAGE_LAYOUT_GENERAL not supported for src host copy");
    layoutSupported = false;
    for (uint32_t i = 0; i < hostImageCopyProperties.copyDstLayoutCount; ++i)
    {
        if (hostImageCopyProperties.pCopyDstLayouts[i] == vk::VK_IMAGE_LAYOUT_GENERAL)
            layoutSupported = true;
    }
    if (layoutSupported == false)
        return tcu::TestStatus::fail("VK_IMAGE_LAYOUT_GENERAL not supported for dst host copy");

    bool UUIDZero = true;
    for (uint32_t i = 0; i < VK_UUID_SIZE; ++i)
    {
        if (hostImageCopyProperties.optimalTilingLayoutUUID[i] != 0)
        {
            UUIDZero = false;
            break;
        }
    }
    if (UUIDZero)
        return tcu::TestStatus::fail("All bytes of optimalTilingLayoutUUID are 0");

    return tcu::TestStatus::pass("Pass");
}

class PropertiesTestCase : public vkt::TestCase
{
public:
    PropertiesTestCase(tcu::TestContext &context, const char *name) : TestCase(context, name)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new PropertiesTestInstance(context);
    }
    void checkSupport(vkt::Context &context) const;
};

void PropertiesTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_host_image_copy");
}

class QueryTestInstance : public vkt::TestInstance
{
public:
    QueryTestInstance(vkt::Context &context, const vk::VkFormat format, const vk::VkImageTiling tiling)
        : vkt::TestInstance(context)
        , m_format(format)
        , m_tiling(tiling)
    {
    }

private:
    tcu::TestStatus iterate(void);

    const vk::VkFormat m_format;
    const vk::VkImageTiling m_tiling;
};

tcu::TestStatus QueryTestInstance::iterate(void)
{
    const InstanceInterface &vki              = m_context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    tcu::TestLog &log                         = m_context.getTestContext().getLog();

    const vk::VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        m_format,                                                  // VkFormat format;
        vk::VK_IMAGE_TYPE_2D,                                      // VkImageType type;
        m_tiling,                                                  // VkImageTiling tiling;
        vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,                  // VkImageUsageFlags usage;
        (VkImageCreateFlags)0u                                     // VkImageCreateFlags flags;
    };

    vk::VkHostImageCopyDevicePerformanceQueryEXT hostImageCopyDevicePerformanceQuery = vk::initVulkanStructure();
    vk::VkImageFormatProperties2 imageFormatProperties = vk::initVulkanStructure(&hostImageCopyDevicePerformanceQuery);
    vk::VkResult res =
        vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties);

    if (hostImageCopyDevicePerformanceQuery.identicalMemoryLayout == VK_FALSE)
    {
        if (hostImageCopyDevicePerformanceQuery.optimalDeviceAccess != VK_FALSE)
        {
            log << tcu::TestLog::Message
                << "VkHostImageCopyDevicePerformanceQueryEXT::identicalMemoryLayout is VK_FALSE, but "
                   "VkHostImageCopyDevicePerformanceQueryEXT::optimalDeviceAccess is VK_TRUE"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }
    else
    {
        if (hostImageCopyDevicePerformanceQuery.optimalDeviceAccess != VK_TRUE)
        {
            log << tcu::TestLog::Message
                << "VkHostImageCopyDevicePerformanceQueryEXT::identicalMemoryLayout is VK_TRUE, but "
                   "VkHostImageCopyDevicePerformanceQueryEXT::optimalDeviceAccess is VK_FALSE"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    if (isBlockCompressedFormat(m_format) && res == vk::VK_SUCCESS)
    {
        if (hostImageCopyDevicePerformanceQuery.optimalDeviceAccess != VK_TRUE)
        {
            log << tcu::TestLog::Message
                << "Format is a block compressed format and vkGetPhysicalDeviceImageFormatProperties2 returned "
                   "VK_SUCCESS, but VkHostImageCopyDevicePerformanceQueryEXT::optimalDeviceAccess is VK_FALSE"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    if (!vk::isDepthStencilFormat(m_format))
    {
        vk::VkFormatProperties3 formatProperties3 = vk::initVulkanStructure();
        vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
        vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_format, &formatProperties2);

        if (m_tiling == VK_IMAGE_TILING_OPTIMAL)
        {
            if ((formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
            {
                log << tcu::TestLog::Message
                    << "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT is supported in optimalTilingFeatures for format "
                    << vk::getFormatStr(m_format).toString()
                    << ", but VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT is not" << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT not supported");
            }
        }
        else if (m_tiling == VK_IMAGE_TILING_LINEAR)
        {
            if ((formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
            {
                log << tcu::TestLog::Message
                    << "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT is supported in linearTilingFeatures for format "
                    << vk::getFormatStr(m_format).toString()
                    << ", but VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT is not" << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT not supported");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class QueryTestCase : public vkt::TestCase
{
public:
    QueryTestCase(tcu::TestContext &context, const char *name, const vk::VkFormat format,
                  const vk::VkImageTiling tiling)
        : TestCase(context, name)
        , m_format(format)
        , m_tiling(tiling)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new QueryTestInstance(context, m_format, m_tiling);
    }
    void checkSupport(vkt::Context &context) const;

    const vk::VkFormat m_format;
    const vk::VkImageTiling m_tiling;
};

void QueryTestCase::checkSupport(vkt::Context &context) const
{
    const InstanceInterface &vki = context.getInstanceInterface();

    context.requireDeviceFunctionality("VK_EXT_host_image_copy");

    vk::VkFormatProperties3 formatProperties3 = vk::initVulkanStructure();
    vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
    vki.getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), m_format, &formatProperties2);
    if (m_tiling == VK_IMAGE_TILING_OPTIMAL &&
        (formatProperties3.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT) == 0)
        TCU_THROW(NotSupportedError, "Format feature sampled image bit not supported for optimal tiling.");
    if (m_tiling == VK_IMAGE_TILING_LINEAR &&
        (formatProperties3.linearTilingFeatures & vk::VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT) == 0)
        TCU_THROW(NotSupportedError, "Format feature sampled image bit not supported for linear tiling.");
}

class IdenticalMemoryLayoutTestInstance : public vkt::TestInstance
{
public:
    IdenticalMemoryLayoutTestInstance(vkt::Context &context, const vk::VkFormat format, const vk::VkImageTiling tiling)
        : vkt::TestInstance(context)
        , m_format(format)
        , m_tiling(tiling)
    {
    }

private:
    tcu::TestStatus iterate(void);

    const vk::VkFormat m_format;
    const vk::VkImageTiling m_tiling;
};

void generateImageData(const DeviceInterface &vkd, VkDevice device, vk::Allocator &alloc, uint32_t qfIndex,
                       VkQueue queue, VkImage image, VkFormat format, const tcu::IVec3 &extent)
{
    const auto tcuFormat = mapVkFormat(format);
    const auto vkExtent  = makeExtent3D(extent);
    tcu::TextureLevel level(tcuFormat, extent.x(), extent.y());
    auto access        = level.getAccess();
    const auto chClass = tcu::getTextureChannelClass(tcuFormat.type);
    const tcu::Vec4 minValue(0.0f);
    tcu::Vec4 maxValue(1.0f);

    //
    // Generate image data on host memory.
    //

    if (tcuFormat.order == tcu::TextureFormat::S)
    {
        // Stencil-only is stored in the first component. Stencil is always 8 bits.
        maxValue.x() = 1 << 8;
    }
    else if (tcuFormat.order == tcu::TextureFormat::DS)
    {
        // In a combined format, fillWithComponentGradients expects stencil in the fourth component.
        maxValue.w() = 1 << 8;
    }
    else if (chClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER || chClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
    {
        // The tcu::Vectors we use as pixels are 32-bit, so clamp to that.
        const tcu::IVec4 bits = tcu::min(tcu::getTextureFormatBitDepth(tcuFormat), tcu::IVec4(32));
        const int signBit     = (chClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ? 1 : 0);

        for (int i = 0; i < 4; ++i)
        {
            if (bits[i] != 0)
                maxValue[i] = static_cast<float>((uint64_t(1) << (bits[i] - signBit)) - 1);
        }
    }

    tcu::fillWithComponentGradients2(access, minValue, maxValue);

    //
    // Upload generated data to the image.
    //

    if (isDepthStencilFormat(format))
    {
        // Iteration index: 0 is depth, 1 is stencil
        for (int i = 0; i < 2; ++i)
        {
            const auto hasComponent =
                ((i == 0) ? tcu::hasDepthComponent(tcuFormat.order) : tcu::hasStencilComponent(tcuFormat.order));
            const auto origMode    = ((i == 0) ? tcu::Sampler::MODE_DEPTH : tcu::Sampler::MODE_STENCIL);
            const auto layerAspect = ((i == 0) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT);

            if (hasComponent)
            {
                const auto xferFormat = ((i == 0) ? getDepthCopyFormat(format) : getStencilCopyFormat(format));
                auto origAccess       = tcu::getEffectiveDepthStencilAccess(access, origMode);
                tcu::TextureLevel copyLevel(xferFormat, extent.x(), extent.y());
                auto copyAccess       = copyLevel.getAccess();
                const auto pixelSize  = tcu::getPixelSize(xferFormat);
                const auto bufferSize = pixelSize * extent.x() * extent.y();

                // Get a copy of the aspect.
                tcu::copy(copyAccess, origAccess);

                // Upload that copy to a buffer and then the image.
                const auto bufferInfo =
                    makeBufferCreateInfo(static_cast<VkDeviceSize>(bufferSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                BufferWithMemory srcBuffer(vkd, device, alloc, bufferInfo, MemoryRequirement::HostVisible);
                auto &srcBufferAlloc = srcBuffer.getAllocation();
                void *srcBufferData  = srcBufferAlloc.getHostPtr();

                deMemcpy(srcBufferData, copyAccess.getDataPtr(), static_cast<size_t>(bufferSize));
                flushAlloc(vkd, device, srcBufferAlloc);

                const auto cmdPool   = makeCommandPool(vkd, device, qfIndex);
                const auto cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

                beginCommandBuffer(vkd, *cmdBuffer);

                const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT, &barrier);

                const auto copySRL    = makeImageSubresourceLayers(layerAspect, 0u, 0u, 1u);
                const auto copyRegion = makeBufferImageCopy(vkExtent, copySRL);
                vkd.cmdCopyBufferToImage(*cmdBuffer, srcBuffer.get(), image, VK_IMAGE_LAYOUT_GENERAL, 1u, &copyRegion);

                endCommandBuffer(vkd, *cmdBuffer);
                submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
            }
        }
    }
    else
    {
        // Simplest case.
        const auto pixelSize  = tcu::getPixelSize(tcuFormat);
        const auto bufferSize = pixelSize * extent.x() * extent.y();

        // Upload pixels to host-visible buffer.
        const auto bufferInfo =
            makeBufferCreateInfo(static_cast<VkDeviceSize>(bufferSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        BufferWithMemory srcBuffer(vkd, device, alloc, bufferInfo, MemoryRequirement::HostVisible);
        auto &srcBufferAlloc = srcBuffer.getAllocation();
        void *srcBufferData  = srcBufferAlloc.getHostPtr();

        deMemcpy(srcBufferData, access.getDataPtr(), static_cast<size_t>(bufferSize));
        flushAlloc(vkd, device, srcBufferAlloc);

        const auto cmdPool   = makeCommandPool(vkd, device, qfIndex);
        const auto cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vkd, *cmdBuffer);
        const auto copySRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
        const auto copyRegion = makeBufferImageCopy(vkExtent, copySRL);
        vkd.cmdCopyBufferToImage(*cmdBuffer, srcBuffer.get(), image, VK_IMAGE_LAYOUT_GENERAL, 1u, &copyRegion);

        endCommandBuffer(vkd, *cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
    }
}

// This is slightly special.
void generateCompressedImageData(const DeviceInterface &vkd, VkDevice device, vk::Allocator &alloc, uint32_t qfIndex,
                                 VkQueue queue, VkImage image, VkFormat format, const tcu::IVec3 &extent)
{
    const auto vkExtent  = makeExtent3D(extent);
    const auto tcuFormat = mapVkCompressedFormat(format);
    tcu::CompressedTexture texture(tcuFormat, extent.x(), extent.y());
    const auto dataSize = texture.getDataSize();
    auto dataPtr        = reinterpret_cast<uint8_t *>(texture.getData());

    // This is supposed to be safe for the compressed formats we're using (no ASTC, no ETC, no SFLOAT formats).
    de::Random rnd(static_cast<uint32_t>(format));
    for (int i = 0; i < dataSize; ++i)
        dataPtr[i] = rnd.getUint8();

    // Upload pixels to host-visible buffer.
    const auto bufferInfo = makeBufferCreateInfo(static_cast<VkDeviceSize>(dataSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    BufferWithMemory srcBuffer(vkd, device, alloc, bufferInfo, MemoryRequirement::HostVisible);
    auto &srcBufferAlloc = srcBuffer.getAllocation();
    void *srcBufferData  = srcBufferAlloc.getHostPtr();

    deMemcpy(srcBufferData, texture.getData(), static_cast<size_t>(dataSize));
    flushAlloc(vkd, device, srcBufferAlloc);

    // Transfer buffer to compressed image.
    const auto cmdPool   = makeCommandPool(vkd, device, qfIndex);
    const auto cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);
    const auto copySRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto copyRegion = makeBufferImageCopy(vkExtent, copySRL);
    vkd.cmdCopyBufferToImage(*cmdBuffer, srcBuffer.get(), image, VK_IMAGE_LAYOUT_GENERAL, 1u, &copyRegion);

    endCommandBuffer(vkd, *cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
}

tcu::TestStatus IdenticalMemoryLayoutTestInstance::iterate(void)
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const VkDevice device                 = m_context.getDevice();
    const auto memoryProperties           = getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue                   = m_context.getUniversalQueue();
    auto &alloc                           = m_context.getDefaultAllocator();
    const tcu::IVec3 extent(32, 32, 1);
    const auto vkExtent           = makeExtent3D(extent);
    const auto baseUsageFlags     = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto extendedUsageFlags = (baseUsageFlags | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT);

    // Create two images, one with the host transfer usage bit and another one without it, to check identicalMemoryLayout.
    VkImageCreateInfo imageCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
        nullptr,                                 // const void*                pNext
        0u,                                      // VkImageCreateFlags        flags
        VK_IMAGE_TYPE_2D,                        // VkImageType                imageType
        m_format,                                // VkFormat                    format
        vkExtent,                                // VkExtent3D                extent
        1u,                                      // uint32_t                    mipLevels
        1u,                                      // uint32_t                    arrayLayers
        VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits    samples
        m_tiling,                                // VkImageTiling            tiling
        baseUsageFlags,                          // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,               // VkSharingMode            sharingMode
        0u,                                      // uint32_t                    queueFamilyIndexCount
        nullptr,                                 // const uint32_t*            pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout            initialLayout
    };
    const auto baseImage     = createImage(vk, device, &imageCreateInfo);
    imageCreateInfo.usage    = extendedUsageFlags;
    const auto hostXferImage = createImage(vk, device, &imageCreateInfo);

    // Check memory requirements for both (size must match).
    const auto baseMemReqs  = getImageMemoryRequirements(vk, device, *baseImage);
    const auto hostXferReqs = getImageMemoryRequirements(vk, device, *hostXferImage);

    if (baseMemReqs.size != hostXferReqs.size)
        TCU_FAIL("Different memory sizes for normal and host-transfer image");

    const auto imageMemSize   = baseMemReqs.size;
    const auto imageMemSizeSz = static_cast<size_t>(imageMemSize);

    // Create two buffers that will share memory with the previous images.
    const auto bufferCreateInfo =
        makeBufferCreateInfo(imageMemSize, (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    const auto baseBuffer     = createBuffer(vk, device, &bufferCreateInfo);
    const auto hostXferBuffer = createBuffer(vk, device, &bufferCreateInfo);

    // Find common memory types for images and buffers and allocate memory.
    const auto baseBufferMemReqs     = getBufferMemoryRequirements(vk, device, *baseBuffer);
    const auto baseCommonMemoryTypes = (baseMemReqs.memoryTypeBits & baseBufferMemReqs.memoryTypeBits);

    // Very unlikely.
    if (baseCommonMemoryTypes == 0u)
        TCU_THROW(NotSupportedError, "Base buffer and image do not have any memory types in common");

    const auto baseSelectedMemType =
        selectMatchingMemoryType(memoryProperties, baseCommonMemoryTypes, MemoryRequirement::Any);

    const auto hostXferBufferMemReqs     = getBufferMemoryRequirements(vk, device, *hostXferBuffer);
    const auto hostXferCommonMemoryTypes = (hostXferReqs.memoryTypeBits & hostXferBufferMemReqs.memoryTypeBits);

    // Very unlikely.
    if (hostXferCommonMemoryTypes == 0u)
        TCU_THROW(NotSupportedError, "Host transfer buffer and image do not have any memory types in common");

    const auto hostXferSelectedMemType =
        selectMatchingMemoryType(memoryProperties, hostXferCommonMemoryTypes, MemoryRequirement::Any);

    VkMemoryAllocateInfo memoryAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        imageMemSize,                           // VkDeviceSize allocationSize;
        baseSelectedMemType,                    // uint32_t memoryTypeIndex;
    };
    const auto baseMemory              = allocateMemory(vk, device, &memoryAllocateInfo, nullptr);
    memoryAllocateInfo.memoryTypeIndex = hostXferSelectedMemType;
    const auto hostXferMemory          = allocateMemory(vk, device, &memoryAllocateInfo, nullptr);

    // Map allocations to images and buffers.
    vk.bindImageMemory(device, *baseImage, *baseMemory, 0u);
    vk.bindBufferMemory(device, *baseBuffer, *baseMemory, 0u);

    vk.bindImageMemory(device, *hostXferImage, *hostXferMemory, 0u);
    vk.bindBufferMemory(device, *hostXferBuffer, *hostXferMemory, 0u);

    // Clear both image memories to zero (via the memory-sharing buffers above) before filling images with data.
    {
        const auto cmdPool   = makeCommandPool(vk, device, queueFamilyIndex);
        const auto cmdbuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        // Transition both images to the general layout for subsequent operations.
        const auto isDepthStencil = isDepthStencilFormat(m_format);
        const auto tcuFormat      = (isCompressedFormat(m_format) ? tcu::TextureFormat() : mapVkFormat(m_format));
        const auto hasDepth       = isDepthStencil && tcu::hasDepthComponent(tcuFormat.order);
        const auto hasStencil     = isDepthStencil && tcu::hasStencilComponent(tcuFormat.order);
        const auto aspectMask =
            (isDepthStencil ?
                 ((hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)) :
                 VK_IMAGE_ASPECT_COLOR_BIT);
        const auto imageSRR = makeImageSubresourceRange(aspectMask, 0u, 1u, 0u, 1u);

        beginCommandBuffer(vk, *cmdbuffer);
        vk.cmdFillBuffer(*cmdbuffer, *baseBuffer, 0ull, VK_WHOLE_SIZE, 0u);
        vk.cmdFillBuffer(*cmdbuffer, *hostXferBuffer, 0ull, VK_WHOLE_SIZE, 0u);
        const std::vector<VkImageMemoryBarrier> transitionBarriers{
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *baseImage, imageSRR),
            makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *hostXferImage, imageSRR),
        };
        vk.cmdPipelineBarrier(*cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, de::sizeU32(transitionBarriers),
                              de::dataOrNull(transitionBarriers));
        endCommandBuffer(vk, *cmdbuffer);
        submitCommandsAndWait(vk, device, queue, *cmdbuffer);
    }

    // Generate data for both images.
    if (isCompressedFormat(m_format))
    {
        generateCompressedImageData(vk, device, alloc, queueFamilyIndex, queue, *baseImage, m_format, extent);
        generateCompressedImageData(vk, device, alloc, queueFamilyIndex, queue, *hostXferImage, m_format, extent);
    }
    else
    {
        generateImageData(vk, device, alloc, queueFamilyIndex, queue, *baseImage, m_format, extent);
        generateImageData(vk, device, alloc, queueFamilyIndex, queue, *hostXferImage, m_format, extent);
    }

    // Create a couple of host-visible buffers for verification.
    const auto verifBufferCreateInfo = makeBufferCreateInfo(imageMemSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory baseVerifBuffer(vk, device, alloc, verifBufferCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory hostXferVerifBuffer(vk, device, alloc, verifBufferCreateInfo, MemoryRequirement::HostVisible);

    // Copy data from shared-memory buffers to verification buffers.
    const auto cmdPool         = makeCommandPool(vk, device, queueFamilyIndex);
    const auto cmdBuffer       = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto copyRegion      = makeBufferCopy(0ull, 0ull, imageMemSize);
    const auto preCopyBarrier  = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    const auto postCopyBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    beginCommandBuffer(vk, *cmdBuffer);
    cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             &preCopyBarrier);
    vk.cmdCopyBuffer(*cmdBuffer, *baseBuffer, *baseVerifBuffer, 1u, &copyRegion);
    vk.cmdCopyBuffer(*cmdBuffer, *hostXferBuffer, *hostXferVerifBuffer, 1u, &copyRegion);
    cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &postCopyBarrier);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, baseVerifBuffer.getAllocation());
    invalidateAlloc(vk, device, hostXferVerifBuffer.getAllocation());

    const auto baseVerifData     = reinterpret_cast<const uint8_t *>(baseVerifBuffer.getAllocation().getHostPtr());
    const auto hostXferVerifData = reinterpret_cast<const uint8_t *>(hostXferVerifBuffer.getAllocation().getHostPtr());

    for (size_t i = 0; i < imageMemSizeSz; ++i)
    {
        if (baseVerifData[i] != hostXferVerifData[i])
        {
            std::ostringstream msg;
            msg << "Base image and host copy image data differs at byte " << i << ": 0x" << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(baseVerifData[i]) << " vs 0x" << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<int>(hostXferVerifData[i]);
            TCU_FAIL(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class IdenticalMemoryLayoutTestCase : public vkt::TestCase
{
public:
    IdenticalMemoryLayoutTestCase(tcu::TestContext &context, const char *name, const vk::VkFormat format,
                                  const vk::VkImageTiling tiling)
        : TestCase(context, name)
        , m_format(format)
        , m_tiling(tiling)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new IdenticalMemoryLayoutTestInstance(context, m_format, m_tiling);
    }
    void checkSupport(vkt::Context &context) const;

    const vk::VkFormat m_format;
    const vk::VkImageTiling m_tiling;
};

void IdenticalMemoryLayoutTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_host_image_copy");

    const InstanceInterface &vki          = context.getInstanceInterface();
    VkFormatProperties3 formatProperties3 = initVulkanStructure();
    VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
    const auto requiredFeatures =
        (VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT | VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT);
    const auto imageUsage = (VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    vki.getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), m_format, &formatProperties2);

    if (m_tiling == VK_IMAGE_TILING_OPTIMAL &&
        (formatProperties3.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
        TCU_THROW(NotSupportedError, "Required format feature not supported for optimal tiling.");

    if (m_tiling == VK_IMAGE_TILING_LINEAR &&
        (formatProperties3.linearTilingFeatures & requiredFeatures) != requiredFeatures)
        TCU_THROW(NotSupportedError, "Required format feature not supported for linear tiling.");

    VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        m_format,                                              // VkFormat format;
        VK_IMAGE_TYPE_2D,                                      // VkImageType type;
        m_tiling,                                              // VkImageTiling tiling;
        imageUsage,                                            // VkImageUsageFlags usage;
        0u,                                                    // VkImageCreateFlags flags;
    };

    VkHostImageCopyDevicePerformanceQueryEXT hostImageCopyDevicePerformanceQuery = initVulkanStructure();
    VkImageFormatProperties2 imageFormatProperties = initVulkanStructure(&hostImageCopyDevicePerformanceQuery);

    VkResult res = vki.getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &imageFormatInfo,
                                                               &imageFormatProperties);

    if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format not supported");

    if (!hostImageCopyDevicePerformanceQuery.identicalMemoryLayout)
        TCU_THROW(NotSupportedError, "identicalMemoryLayout not supported for this format");
}

struct DepthStencilHICParams
{
    VkFormat format;
    tcu::IVec2 extent;
};

class DepthStencilHostImageCopyInstance : public vkt::TestInstance
{
public:
    DepthStencilHostImageCopyInstance(Context &context, const DepthStencilHICParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~DepthStencilHostImageCopyInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const DepthStencilHICParams m_params;
};

class DepthStencilHostImageCopyTest : public vkt::TestCase
{
public:
    static float getGeometryDepth(void)
    {
        return 0.5f;
    }
    static tcu::Vec4 getGeometryColor(void)
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
    static tcu::Vec4 getClearColor(void)
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Tests require host image transfer, general transfer and depth/stencil usage.
    static VkImageUsageFlags getDepthStencilUsage(void)
    {
        return (VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    static VkFormatFeatureFlags2 getDepthStencilFormatFeatures(void)
    {
        return (VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT | VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT |
                VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    DepthStencilHostImageCopyTest(tcu::TestContext &testCtx, const std::string &name,
                                  const DepthStencilHICParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DepthStencilHostImageCopyTest(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const
    {
        return new DepthStencilHostImageCopyInstance(context, m_params);
    }
    void checkSupport(Context &context) const;

protected:
    const DepthStencilHICParams m_params;
};

void DepthStencilHostImageCopyTest::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto geometryDepth = getGeometryDepth();
    const auto geometryColor = getGeometryColor();

    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[3] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2( 3.0, -1.0),\n"
         << "    vec2(-1.0, 3.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 3], " << geometryDepth << ", 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4" << geometryColor << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void DepthStencilHostImageCopyTest::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_host_image_copy");

    const auto &vki                       = context.getInstanceInterface();
    const auto physicalDevice             = context.getPhysicalDevice();
    VkFormatProperties3 formatProperties3 = initVulkanStructure();
    VkFormatProperties2 formatProperties2 = initVulkanStructure(&formatProperties3);
    const auto requiredFeatures           = getDepthStencilFormatFeatures();
    const auto imageUsage                 = getDepthStencilUsage();

    // Check format support.
    vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_params.format, &formatProperties2);

    if ((formatProperties3.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
        TCU_THROW(NotSupportedError, "Required format features not supported for this format");

    // Check image usage support.
    const VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        m_params.format,                                       // VkFormat format;
        VK_IMAGE_TYPE_2D,                                      // VkImageType type;
        VK_IMAGE_TILING_OPTIMAL,                               // VkImageTiling tiling;
        imageUsage,                                            // VkImageUsageFlags usage;
        0u,                                                    // VkImageCreateFlags flags;
    };

    VkImageFormatProperties2 imageFormatProperties = initVulkanStructure();
    const auto res =
        vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties);

    if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Image usage not supported for this format");
}

void hostLayoutTransition(const DeviceInterface &vkd, const VkDevice device, const VkImage image,
                          const VkImageLayout oldLayout, const VkImageLayout newLayout,
                          const VkImageSubresourceRange &imageSRR)
{
    const VkHostImageLayoutTransitionInfoEXT toTransfer{
        VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT, // VkStructureType sType;
        nullptr,                                                 // const void* pNext;
        image,                                                   // VkImage image;
        oldLayout,                                               // VkImageLayout oldLayout;
        newLayout,                                               // VkImageLayout newLayout;
        imageSRR,                                                // VkImageSubresourceRange subresourceRange;
    };
    vkd.transitionImageLayoutEXT(device, 1u, &toTransfer);
}

void copyDSMemoryToImage(const DeviceInterface &vkd, const VkDevice device, const VkImage image,
                         const VkImageLayout layout, const VkImageAspectFlagBits aspect, const void *pixelData,
                         const VkExtent3D &extent)
{
    const VkMemoryToImageCopyEXT copyRegion = {
        VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,     // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        pixelData,                                      // const void* pHostPointer;
        0u,                                             // uint32_t memoryRowLength;
        0u,                                             // uint32_t memoryImageHeight;
        makeImageSubresourceLayers(aspect, 0u, 0u, 1u), // VkImageSubresourceLayers imageSubresource;
        makeOffset3D(0, 0, 0),                          // VkOffset3D imageOffset;
        extent,                                         // VkExtent3D imageExtent;
    };

    const VkCopyMemoryToImageInfoEXT copyInfo = {
        VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        0u,                                              // VkHostImageCopyFlagsEXT flags;
        image,                                           // VkImage dstImage;
        layout,                                          // VkImageLayout dstImageLayout;
        1u,                                              // uint32_t regionCount;
        &copyRegion,                                     // const VkMemoryToImageCopyEXT* pRegions;
    };

    vkd.copyMemoryToImageEXT(device, &copyInfo);
}

void copyDSImageToMemory(const DeviceInterface &vkd, const VkDevice device, const VkImage image,
                         const VkImageLayout layout, const VkImageAspectFlagBits aspect, void *pixelData,
                         const VkExtent3D &extent)
{
    const VkImageToMemoryCopyEXT copyRegion = {
        VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT,     // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        pixelData,                                      // void* pHostPointer;
        0u,                                             // uint32_t memoryRowLength;
        0u,                                             // uint32_t memoryImageHeight;
        makeImageSubresourceLayers(aspect, 0u, 0u, 1u), // VkImageSubresourceLayers imageSubresource;
        makeOffset3D(0, 0, 0),                          // VkOffset3D imageOffset;
        extent,                                         // VkExtent3D imageExtent;
    };

    const VkCopyImageToMemoryInfoEXT copyInfo = {
        VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        0u,                                              // VkHostImageCopyFlagsEXT flags;
        image,                                           // VkImage srcImage;
        layout,                                          // VkImageLayout srcImageLayout;
        1u,                                              // uint32_t regionCount;
        &copyRegion,                                     // const VkImageToMemoryCopyEXT* pRegions;
    };

    vkd.copyImageToMemoryEXT(device, &copyInfo);
}

tcu::TestStatus DepthStencilHostImageCopyInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR    = makeDefaultImageSubresourceRange();
    const auto colorSRL    = makeDefaultImageSubresourceLayers();
    const auto fbExtent    = tcu::IVec3(m_params.extent.x(), m_params.extent.y(), 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto tcuFormat   = mapVkFormat(m_params.format);
    const bool hasDepth    = tcu::hasDepthComponent(tcuFormat.order);
    const bool hasStencil  = tcu::hasStencilComponent(tcuFormat.order);
    const auto dsUsage     = DepthStencilHostImageCopyTest::getDepthStencilUsage();
    const auto dsAspects =
        ((hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));
    const auto dsSRR     = makeImageSubresourceRange(dsAspects, 0u, 1u, 0u, 1u);
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    const auto geometryDepth = DepthStencilHostImageCopyTest::getGeometryDepth();
    const auto geometryColor = DepthStencilHostImageCopyTest::getGeometryColor();
    const auto clearColor    = DepthStencilHostImageCopyTest::getClearColor();

    const auto depthFormat   = (hasDepth ? getDepthCopyFormat(m_params.format) : tcu::TextureFormat());
    const auto stencilFormat = (hasStencil ? getStencilCopyFormat(m_params.format) : tcu::TextureFormat());

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType);

    // Depth/stencil image.
    const VkImageCreateInfo dsImageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        m_params.format,                     // VkFormat format;
        vkExtent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        dsUsage,                             // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory dsImage(ctx.vkd, ctx.device, ctx.allocator, dsImageCreateInfo, MemoryRequirement::Any);
    const auto dsImageView =
        makeImageView(ctx.vkd, ctx.device, *dsImage, VK_IMAGE_VIEW_TYPE_2D, dsImageCreateInfo.format, dsSRR);

    tcu::TextureLevel origDepthLevel;
    tcu::TextureLevel origStencilLevel;
    tcu::PixelBufferAccess origDepthAccess;
    tcu::PixelBufferAccess origStencilAccess;

    const uint64_t randomSeed =
        ((static_cast<uint64_t>(vkExtent.width) << 48) | (static_cast<uint64_t>(vkExtent.height) << 32) |
         static_cast<uint64_t>(m_params.format));
    de::Random rnd(deUint64Hash(randomSeed));

    if (hasDepth)
    {
        // We will fill the depth buffer randomly with values that should not create precision problems.
        // Geometry depth should be 0.5 (see the vertex shader).
        origDepthLevel.setStorage(depthFormat, fbExtent.x(), fbExtent.y());
        origDepthAccess = origDepthLevel.getAccess();

        for (int y = 0; y < fbExtent.y(); ++y)
            for (int x = 0; x < fbExtent.x(); ++x)
            {
                const bool pass = rnd.getBool();
                // Generates values in the [0, 0.25] or [0.75, 1.0] ranges depending on "pass".
                const auto depthOffset = (pass ? 0.0f : 0.75f);
                origDepthAccess.setPixDepth(rnd.getFloat() * 0.25f + depthOffset, x, y);
            }
    }

    if (hasStencil)
    {
        // We will fill the stencil buffer randomly as well. In this case there are no precision issues, but we will reserve value
        // zero to be special-cased later. The stencil reference value will be 128 (see below).
        origStencilLevel.setStorage(stencilFormat, fbExtent.x(), fbExtent.y());
        origStencilAccess = origStencilLevel.getAccess();

        for (int y = 0; y < fbExtent.y(); ++y)
            for (int x = 0; x < fbExtent.x(); ++x)
                origStencilAccess.setPixStencil(rnd.getInt(1, 255), x, y);
    }

    // Fill the depth/stencil buffer from the host and prepare it to be used.
    hostLayoutTransition(ctx.vkd, ctx.device, *dsImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, dsSRR);

    if (hasDepth)
    {
        copyDSMemoryToImage(ctx.vkd, ctx.device, *dsImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT,
                            origDepthAccess.getDataPtr(), vkExtent);
    }

    if (hasStencil)
    {
        copyDSMemoryToImage(ctx.vkd, ctx.device, *dsImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_STENCIL_BIT,
                            origStencilAccess.getDataPtr(), vkExtent);
    }

    // Render pass.
    const std::vector<VkAttachmentDescription> attachmentDescriptions{
        // Color attachment description.
        makeAttachmentDescription(0u, colorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),

        // Depth/stencil attachment description.
        makeAttachmentDescription(0u, m_params.format, VK_SAMPLE_COUNT_1_BIT,
                                  (hasDepth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
                                  (hasDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE),
                                  (hasStencil ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
                                  (hasStencil ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE),
                                  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL),
    };

    const auto colorAttRef = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto dsAttRef    = makeAttachmentReference(1u, VK_IMAGE_LAYOUT_GENERAL);
    const auto subpass =
        makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &colorAttRef, nullptr, &dsAttRef, 0u, nullptr);

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescriptions),       // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescriptions),    // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpass,                                  // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr,                                   // const VkSubpassDependency* pDependencies;
    };
    const auto renderPass = createRenderPass(ctx.vkd, ctx.device, &renderPassCreateInfo);

    // Viewports and scissors.
    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    // Framebuffer.
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), *dsImageView};
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews),
                                             de::dataOrNull(fbViews), vkExtent.width, vkExtent.height);

    // Shader modules.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // Empty vertex input state.
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    // Depth/stencil state.
    const auto stencilRef = 128;
    const auto stencilOp  = makeStencilOpState(VK_STENCIL_OP_ZERO, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_ZERO,
                                               VK_COMPARE_OP_GREATER, 0xFFu, 0xFFu, static_cast<uint32_t>(stencilRef));

    const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        makeVkBool(hasDepth),                                       // VkBool32 depthTestEnable;
        makeVkBool(hasDepth),                                       // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_GREATER,                                      // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        makeVkBool(hasStencil),                                     // VkBool32 stencilTestEnable;
        stencilOp,                                                  // VkStencilOpState front;
        stencilOp,                                                  // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        0.0f,                                                       // float maxDepthBounds;
    };

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const auto pipeline       = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                                     VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule, *renderPass, viewports,
                                                     scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputState,
                                                     nullptr, nullptr, &depthStencilState);

    // Run pipeline.
    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Rendering.
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);

    // Depth/stencil buffer sync to the host layout change and host image copy operations that will follow.
    const auto dsBarrier = makeMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                             (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT));

    // Color buffer sync to the transfer operation that follows.
    const auto colorBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR);

    ctx.vkd.cmdPipelineBarrier(cmdBuffer,
                               (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),
                               (VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT), 0u, 1u, &dsBarrier, 0u,
                               nullptr, 1u, &colorBarrier);

    // Copy color buffer to its verification buffer.
    const auto colorCopyRegion = makeBufferImageCopy(vkExtent, colorSRL);
    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 colorBuffer.getBuffer(), 1u, &colorCopyRegion);

    // Sync color copy with host reads.
    const auto transferToHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &transferToHostBarrier);

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Invalidate color verification buffer allocation and create an access to it.
    auto &colorBufferAlloc     = colorBuffer.getBufferAllocation();
    const auto colorBufferData = colorBufferAlloc.getHostPtr();
    const auto colorAccess     = tcu::ConstPixelBufferAccess(mapVkFormat(colorFormat), fbExtent, colorBufferData);
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    // Transfer depth buffer (via host image copy) to a couple of texture levels for verification after use.
    tcu::TextureLevel finalDepthLevel;
    tcu::TextureLevel finalStencilLevel;
    tcu::PixelBufferAccess finalDepthAccess;
    tcu::PixelBufferAccess finalStencilAccess;

    if (hasDepth)
    {
        finalDepthLevel.setStorage(depthFormat, fbExtent.x(), fbExtent.y());
        finalDepthAccess = finalDepthLevel.getAccess();

        copyDSImageToMemory(ctx.vkd, ctx.device, *dsImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT,
                            finalDepthAccess.getDataPtr(), vkExtent);
    }

    if (hasStencil)
    {
        finalStencilLevel.setStorage(stencilFormat, fbExtent.x(), fbExtent.y());
        finalStencilAccess = finalStencilLevel.getAccess();

        copyDSImageToMemory(ctx.vkd, ctx.device, *dsImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_STENCIL_BIT,
                            finalStencilAccess.getDataPtr(), vkExtent);
    }

    // Verify color buffer and depth/stencil values.
    tcu::TextureLevel colorErrorMask(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                     fbExtent.x(), fbExtent.y());
    tcu::TextureLevel depthErrorMask(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                     fbExtent.x(), fbExtent.y());
    tcu::TextureLevel stencilErrorMask(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                                       fbExtent.x(), fbExtent.y());

    auto colorErrorAccess   = colorErrorMask.getAccess();
    auto depthErrorAccess   = depthErrorMask.getAccess();
    auto stencilErrorAccess = stencilErrorMask.getAccess();

    bool allColorOK   = true;
    bool allDepthOK   = true;
    bool allStencilOK = true;
    const auto green  = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    const auto red    = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);

    // Calculate depth threshold to be used when the expected depth value is the geometry depth.
    float depthTestPassThreshold = -1000.0f;
    const auto depthBits         = (hasDepth ? tcu::getTextureFormatBitDepth(tcuFormat).x() : 0);
    switch (depthBits)
    {
    case 0:
        // No depth aspect.
        break;
    case 16:
    case 24:
        depthTestPassThreshold = 1.0f / static_cast<float>((1u << depthBits) - 1u);
        break;
    case 32:
        // 0 is an acceptable threshold here because the geometry depth value is exactly representable as a float.
        depthTestPassThreshold = 0.0f;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float origDepth      = (hasDepth ? origDepthAccess.getPixDepth(x, y) : -1000.0f);
            const int origStencil      = (hasStencil ? origStencilAccess.getPixStencil(x, y) : -1);
            const bool depthTestPass   = (hasDepth ? (geometryDepth > origDepth) : true);  // VK_COMPARE_OP_GREATER
            const bool stencilTestPass = (hasStencil ? (stencilRef > origStencil) : true); // VK_COMPARE_OP_GREATER
            const bool bothTestsPass   = (depthTestPass && stencilTestPass);

            const auto expectedColor  = (bothTestsPass ? geometryColor : clearColor);
            const float expectedDepth = (bothTestsPass ? geometryDepth : origDepth);
            const int expectedStencil = (bothTestsPass ? origStencil /*VK_STENCIL_OP_KEEP*/ : 0 /*VK_STENCIL_OP_ZERO*/);

            const auto resultColor   = colorAccess.getPixel(x, y);
            const auto resultDepth   = (hasDepth ? finalDepthAccess.getPixDepth(x, y) : -1000.0f);
            const auto resultStencil = (hasStencil ? finalStencilAccess.getPixStencil(x, y) : -1);

            const bool colorOK = (resultColor == expectedColor);
            const bool depthOK =
                (hasDepth ? (depthTestPass ?
                                 (std::abs(resultDepth - expectedDepth) <= depthTestPassThreshold) // Geometry depth.
                                 :
                                 (expectedDepth == resultDepth)) // Unmodified depth, expect the exact same value back.
                            :
                            true);
            const bool stencilOK = (hasStencil ? (expectedStencil == resultStencil) : true);

            colorErrorAccess.setPixel((colorOK ? green : red), x, y);
            depthErrorAccess.setPixel((depthOK ? green : red), x, y);
            stencilErrorAccess.setPixel((stencilOK ? green : red), x, y);

            allColorOK   = (allColorOK && colorOK);
            allDepthOK   = (allDepthOK && depthOK);
            allStencilOK = (allStencilOK && stencilOK);
        }

    auto &log = m_context.getTestContext().getLog();

    if (!allColorOK)
    {
        log << tcu::TestLog::ImageSet("ColorComparison", "") << tcu::TestLog::Image("ColorResult", "", colorAccess)
            << tcu::TestLog::Image("ColorErrorMask", "", colorErrorAccess) << tcu::TestLog::EndImageSet;
    }

    if (!allDepthOK)
    {
        log << tcu::TestLog::ImageSet("DepthComparison", "")
            << tcu::TestLog::Image("DepthErrorMask", "", depthErrorAccess) << tcu::TestLog::EndImageSet;
    }

    if (!allStencilOK)
    {
        log << tcu::TestLog::ImageSet("StencilComparison", "")
            << tcu::TestLog::Image("StencilErrorMask", "", stencilErrorAccess) << tcu::TestLog::EndImageSet;
    }

    if (!(allColorOK && allDepthOK && allStencilOK))
        return tcu::TestStatus::fail("Unexpected values in color, depth or stencil buffers -- check log for details");
    return tcu::TestStatus::pass("Pass");
}

void testGenerator(tcu::TestCaseGroup *group)
{
    constexpr struct CopyTest
    {
        bool hostTransferLayout;
        bool hostCopy;
        const char *name;
    } copyTests[] = {
        // Transition using vkTransitionImageLayoutEXT and copy on host
        {true, true, "host_transition_host_copy"},
        // Transition using vkTransitionImageLayoutEXT and copy on gpu
        {true, false, "host_transition"},
        // Transition using a pipeline barrier and copy on host
        {false, true, "barrier_transition_host_copy"},
    };

    constexpr struct CopyActionTest
    {
        HostCopyAction action;
        const char *name;
    } copyActionTests[] = {
        // If copy on host, copy from memory to image
        {MEMORY_TO_IMAGE, "memory_to_image"},
        // If copy on host, copy from image to memory
        {IMAGE_TO_MEMORY, "image_to_memory"},
        // If copy on host, copy with VK_HOST_IMAGE_COPY_MEMCPY_EXT flag
        {MEMCPY, "memcpy"},
    };

    const struct Tiling
    {
        vk::VkImageTiling tiling;
        const char *name;
    } tilingTests[] = {
        {vk::VK_IMAGE_TILING_LINEAR, "linear"},
        {vk::VK_IMAGE_TILING_OPTIMAL, "optimal"},
    };

    const struct ImageFormatsAndCommand
    {
        Command command;
        vk::VkFormat sampled;
        vk::VkFormat output;
    } formatsAndCommands[] = {
        {DRAW, vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_R8G8B8A8_UNORM},
        {DRAW, vk::VK_FORMAT_R8G8_UNORM, vk::VK_FORMAT_R8G8_UNORM},
        {DRAW, vk::VK_FORMAT_R32G32B32A32_SFLOAT, vk::VK_FORMAT_R32G32B32A32_SFLOAT},
        {DRAW, vk::VK_FORMAT_R8_UNORM, vk::VK_FORMAT_R8_UNORM},
        {DRAW, vk::VK_FORMAT_R32G32_SFLOAT, vk::VK_FORMAT_R32G32_SFLOAT},
        {DRAW, vk::VK_FORMAT_R16_UNORM, vk::VK_FORMAT_R16_UNORM},
        {DRAW, vk::VK_FORMAT_D16_UNORM, vk::VK_FORMAT_R16_UNORM},
        {DRAW, vk::VK_FORMAT_D32_SFLOAT, vk::VK_FORMAT_R32_SFLOAT},
        {DRAW, vk::VK_FORMAT_BC7_UNORM_BLOCK, vk::VK_FORMAT_R8G8B8A8_UNORM},
        {DRAW, vk::VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, vk::VK_FORMAT_R8G8B8A8_UNORM},
        {DRAW, vk::VK_FORMAT_ASTC_4x4_UNORM_BLOCK, vk::VK_FORMAT_R8G8B8A8_UNORM},
        {DISPATCH, vk::VK_FORMAT_R10X6_UNORM_PACK16, vk::VK_FORMAT_R10X6_UNORM_PACK16},
        {DISPATCH, vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_R8G8B8A8_UNORM},
        {DISPATCH, vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_R8G8B8A8_UINT},
    };

    const std::set<vk::VkFormat> restrictedCombinationsFmt{
        vk::VK_FORMAT_R8G8_UNORM,
        vk::VK_FORMAT_R8_UNORM,
        vk::VK_FORMAT_R32G32_SFLOAT,
    };

    const struct ImageSizes
    {
        vk::VkExtent3D size;
        const char *name;
    } imageSizes[] = {
        {makeExtent3D(16u, 16u, 1u), "16x16"},
        {makeExtent3D(32u, 28u, 1u), "32x28"},
        {makeExtent3D(53u, 61u, 1u), "53x61"},
    };

    constexpr struct ImageLayoutTest
    {
        vk::VkImageLayout srcLayout;
        vk::VkImageLayout dstLayout;
        const char *name;
    } imageLayoutTests[] = {
        {vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, "general_general"},
        {vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         "transfer_src_transfer_dst"},
    };

    constexpr struct IntermediateImageLayoutTest
    {
        vk::VkImageLayout layout;
        const char *name;
    } intermediateImageLayoutTests[] = {
        {vk::VK_IMAGE_LAYOUT_GENERAL, "general"},
        {vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "color_attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, "depth_stencil_attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, "depth_stencil_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "shader_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "transfer_src_optimal"},
        {vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "transfer_dst_optimal"},
    };

    constexpr struct MipLevelRegionCountPaddingTest
    {
        uint32_t mipLevel;
        uint32_t regionsCount;
        uint32_t padding;
        const char *name;
        const char *desc;
    } mipLevelRegionCountPaddingTests[] = {
        {0u, 1u, 0u, "0_1_0", ""}, {1u, 1u, 0u, "1_1_0", ""},     {4u, 1u, 0u, "4_1_0", ""},
        {0u, 4u, 4u, "0_4_4", ""}, {0u, 16u, 64u, "0_16_64", ""},
    };

    tcu::TestContext &testCtx = group->getTestContext();

    for (const auto &formatAndCommand : formatsAndCommands)
    {
        std::string formatName = formatAndCommand.command == DRAW ? "draw" : "dispatch";
        formatName +=
            "_" + getFormatShortString(formatAndCommand.output) + "_" + getFormatShortString(formatAndCommand.sampled);
        tcu::TestCaseGroup *const formatGroup = new tcu::TestCaseGroup(testCtx, formatName.c_str());

        bool colorFormat = isCompressedFormat(formatAndCommand.sampled) ||
                           !(tcu::hasDepthComponent(mapVkFormat(formatAndCommand.sampled).order) ||
                             tcu::hasDepthComponent(mapVkFormat(formatAndCommand.sampled).order));

        bool dynamicRenderingBase = true;
        bool sparseImageBase      = true;

        for (const auto &copy : copyTests)
        {
            // Anitalias the config stride!
            dynamicRenderingBase  = !dynamicRenderingBase;
            bool dynamicRendering = dynamicRenderingBase;

            tcu::TestCaseGroup *const copyTestGroup = new tcu::TestCaseGroup(testCtx, copy.name);
            for (const auto &action : copyActionTests)
            {
                // This is identical to action == MEMORY_TO_IMAGE with no host copy, so can be skipped.
                if (!copy.hostCopy && action.action == MEMCPY)
                    continue;

                tcu::TestCaseGroup *const actionGroup = new tcu::TestCaseGroup(testCtx, action.name);
                for (const auto &layouts : imageLayoutTests)
                {
                    tcu::TestCaseGroup *const layoutsGroup = new tcu::TestCaseGroup(testCtx, layouts.name);
                    for (const auto &intermediateLayout : intermediateImageLayoutTests)
                    {
                        if (colorFormat &&
                            (intermediateLayout.layout == vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
                             intermediateLayout.layout == vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL))
                            continue;
                        else if (!colorFormat &&
                                 intermediateLayout.layout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                            continue;

                        tcu::TestCaseGroup *const intermediateLayoutGroup =
                            new tcu::TestCaseGroup(testCtx, intermediateLayout.name);
                        for (const auto &tiling : tilingTests)
                        {
                            tcu::TestCaseGroup *const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name);
                            for (const auto &mipLevelRegionCountPaddingTest : mipLevelRegionCountPaddingTests)
                            {
                                // We are alternating the sparseImage flag here, make sure that count is not even, otherwise this has to be moved to a different loop
                                static_assert(DE_LENGTH_OF_ARRAY(mipLevelRegionCountPaddingTests) % 2 != 0,
                                              "Variation count is not odd");
                                sparseImageBase  = !sparseImageBase;
                                bool sparseImage = sparseImageBase;

                                tcu::TestCaseGroup *const mipLevelRegionCountPaddingGroup =
                                    new tcu::TestCaseGroup(testCtx, mipLevelRegionCountPaddingTest.name);
                                for (const auto &size : imageSizes)
                                {
                                    // Alternate every test
                                    dynamicRendering = !dynamicRendering;
                                    sparseImage      = !sparseImage;

                                    if (sparseImage && isCompressedFormat(formatAndCommand.sampled))
                                        continue;

                                    // These formats were added later, with restricted combinations considered interesting.
                                    if (restrictedCombinationsFmt.find(formatAndCommand.sampled) !=
                                        restrictedCombinationsFmt.end())
                                    {
                                        // Layouts are not that important.
                                        if (layouts.srcLayout == VK_IMAGE_LAYOUT_GENERAL)
                                            continue;
                                        if (intermediateLayout.layout != VK_IMAGE_LAYOUT_GENERAL)
                                            continue;

                                        // Linear tiling covered by R16.
                                        if (tiling.tiling != VK_IMAGE_TILING_OPTIMAL)
                                            continue;

                                        // Mip levels covered by other formats.
                                        if (mipLevelRegionCountPaddingTest.mipLevel != 0u ||
                                            mipLevelRegionCountPaddingTest.regionsCount != 1u ||
                                            mipLevelRegionCountPaddingTest.padding != 0u)
                                        {
                                            continue;
                                        }
                                    }

                                    const TestParameters parameters = {
                                        action.action,             // HostCopyAction    action
                                        copy.hostCopy,             // bool                hostCopy
                                        copy.hostTransferLayout,   // bool                hostTransferLayout
                                        dynamicRendering,          // bool                dynamicRendering
                                        formatAndCommand.command,  // Command            command
                                        formatAndCommand.sampled,  // VkFormat            imageSampledFormat
                                        layouts.srcLayout,         // VkImageLayout    srcLayout
                                        layouts.dstLayout,         // VkImageLayout    dstLayout
                                        intermediateLayout.layout, // VkImageLayout    intermediateLayout
                                        tiling.tiling,             // VkImageTiling sampledTiling;
                                        formatAndCommand.output,   // VkFormat            imageOutputFormat
                                        size.size,                 // VkExtent3D        imageSize
                                        sparseImage,               // bool                sparse
                                        mipLevelRegionCountPaddingTest.mipLevel,     // uint32_t            mipLevel
                                        mipLevelRegionCountPaddingTest.regionsCount, // uint32_t            regionsCount
                                        mipLevelRegionCountPaddingTest.padding       // uint32_t            padding
                                    };

                                    mipLevelRegionCountPaddingGroup->addChild(
                                        new HostImageCopyTestCase(testCtx, size.name, parameters));
                                }
                                tilingGroup->addChild(mipLevelRegionCountPaddingGroup);
                            }
                            intermediateLayoutGroup->addChild(tilingGroup);
                        }
                        layoutsGroup->addChild(intermediateLayoutGroup);
                    }
                    actionGroup->addChild(layoutsGroup);
                }
                copyTestGroup->addChild(actionGroup);
            }
            formatGroup->addChild(copyTestGroup);
        }
        group->addChild(formatGroup);
    }

    {
        using FormatPair = std::pair<VkFormat, VkFormat>; // .first = sampled, .second = output

        const std::vector<FormatPair> formatCases{
            std::make_pair(VK_FORMAT_R8_UNORM, VK_FORMAT_R8_UNORM),
            std::make_pair(VK_FORMAT_R16_UNORM, VK_FORMAT_R16_UNORM),
            std::make_pair(VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM),
            std::make_pair(VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_UNORM),
            std::make_pair(VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32_SFLOAT),
            std::make_pair(VK_FORMAT_D32_SFLOAT, VK_FORMAT_R32_SFLOAT),
            std::make_pair(VK_FORMAT_R10X6_UNORM_PACK16, VK_FORMAT_R10X6_UNORM_PACK16),
        };

        const std::vector<VkExtent3D> extentCases{
            makeExtent3D(128u, 128u, 1u),
            makeExtent3D(512u, 512u, 1u),
            makeExtent3D(4096u, 4096u, 1u),
        };

        struct CopyAction
        {
            HostCopyAction action;
            std::string name;
        } copyActions[] = {
            {MEMORY_TO_IMAGE, "memory_to_image"},
            {IMAGE_TO_MEMORY, "image_to_memory"},
        };

        de::MovePtr<tcu::TestCaseGroup> largeImages(new tcu::TestCaseGroup(testCtx, "large_images"));
        for (const auto &format : formatCases)
        {
            const auto &sampledFormat = format.first;
            const auto &outputFormat  = format.second;

            for (const auto &extent : extentCases)
            {
                for (const auto &action : copyActions)
                {
                    const TestParameters parameters = {
                        action.action,           // HostCopyAction    action
                        true,                    // bool                hostCopy
                        true,                    // bool                hostTransferLayout
                        false,                   // bool                dynamicRendering
                        DRAW,                    // Command            command
                        sampledFormat,           // VkFormat            imageSampledFormat
                        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout    srcLayout
                        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout    dstLayout
                        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout    intermediateLayout
                        VK_IMAGE_TILING_OPTIMAL, // VkImageTiling sampledTiling;
                        outputFormat,            // VkFormat            imageOutputFormat
                        extent,                  // VkExtent3D        imageSize
                        false,                   // bool                sparse
                        0u,                      // uint32_t            mipLevel
                        1u,                      // uint32_t            regionsCount
                        0u,                      // uint32_t            padding
                    };

                    const std::string testName = action.name + "_" + getFormatShortString(sampledFormat) + "_" +
                                                 std::to_string(extent.height) + "_" + std::to_string(extent.height);
                    largeImages->addChild(new HostImageCopyTestCase(testCtx, testName.c_str(), parameters));
                }
            }
        }

        group->addChild(largeImages.release());
    }

    const struct PreinitializedFormats
    {
        vk::VkFormat format;
    } preinitializedFormats[] = {
        {vk::VK_FORMAT_R8G8B8A8_UNORM}, {vk::VK_FORMAT_R32G32B32A32_SFLOAT}, {vk::VK_FORMAT_R16_UNORM},
        {vk::VK_FORMAT_R16G16_UINT},    {vk::VK_FORMAT_B8G8R8A8_SINT},       {vk::VK_FORMAT_R16_SFLOAT},
    };

    const struct PreinitializedTiling
    {
        vk::VkImageTiling tiling;
        const char *name;
    } preinitializedTilingTests[] = {
        {vk::VK_IMAGE_TILING_LINEAR, "linear"},
        {vk::VK_IMAGE_TILING_OPTIMAL, "optimal"},
        {vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, "drm_format_modifier"},
    };

    constexpr struct PreinitializedImageLayoutTest
    {
        vk::VkImageLayout layout;
        const char *name;
    } preinitializedImageLayoutTests[] = {
        {vk::VK_IMAGE_LAYOUT_GENERAL, "general"},
        {vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, "color_attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, "depth_stencil_attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, "depth_stencil_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "shader_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "transfer_src_optimal"},
        {vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "transfer_dst_optimal"},
        {vk::VK_IMAGE_LAYOUT_PREINITIALIZED, "preinitialized"},
        {vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, "present_src"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL, "depth_read_only_stencil_attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL, "depth_attachment_stencil_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, "depth_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL, "stencil_attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL, "stencil_read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, "read_only_optimal"},
        {vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, "attachment_optimal"},
        {vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT, "attachment_feedback_loop_optimal"},
    };

    constexpr struct ImageToImageTest
    {
        bool imageToImageCopy;
        bool memcpy;
        const char *name;
    } imageToImageCopyTests[] = {
        {true, false, "image_to_image_copy"},
        {true, true, "image_to_image_memcpy"},
        {false, false, "preinitialized"},
    };

    constexpr struct ImageSizeTest
    {
        vk::VkExtent3D size;
        uint32_t layerCount;
        const char *name;
    } imageSizeTests[] = {
        {{32, 32, 1}, 1, "32x32x1_1"},
        {{32, 32, 1}, 2, "32x32x1_2"},
        {{51, 63, 1}, 1, "51x63x1_1"},
        {{24, 24, 4}, 1, "24x24x4_4"},
    };

    constexpr struct OffsetTest
    {
        uint32_t offset;
        const char *name;
    } offsetTests[] = {
        // No offset
        {0u, "0"},
        // Offset 64
        {64u, "64"},
    };

    for (const auto &tiling : preinitializedTilingTests)
    {
        tcu::TestCaseGroup *const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name);
        for (const auto &imageToImage : imageToImageCopyTests)
        {
            tcu::TestCaseGroup *const imageToImageCopyGroup = new tcu::TestCaseGroup(testCtx, imageToImage.name);
            for (const auto &srcLayout : preinitializedImageLayoutTests)
            {
                tcu::TestCaseGroup *const srcLayoutGroup = new tcu::TestCaseGroup(testCtx, srcLayout.name);
                for (const auto &dstLayout : preinitializedImageLayoutTests)
                {
                    tcu::TestCaseGroup *const dstLayoutGroup = new tcu::TestCaseGroup(testCtx, dstLayout.name);
                    for (const auto &size : imageSizeTests)
                    {
                        tcu::TestCaseGroup *const sizeGroup = new tcu::TestCaseGroup(testCtx, size.name);
                        for (const auto &offset : offsetTests)
                        {
                            tcu::TestCaseGroup *const offsetGroup = new tcu::TestCaseGroup(testCtx, offset.name);
                            for (const auto &format : preinitializedFormats)
                            {
                                const auto formatName = getFormatShortString(format.format);
                                offsetGroup->addChild(new PreinitializedTestCase(
                                    testCtx, formatName.c_str(), format.format, srcLayout.layout, dstLayout.layout,
                                    size.size, size.layerCount, imageToImage.imageToImageCopy, imageToImage.memcpy,
                                    tiling.tiling, offset.offset));
                            }
                            sizeGroup->addChild(offsetGroup);
                        }
                        dstLayoutGroup->addChild(sizeGroup);
                    }
                    srcLayoutGroup->addChild(dstLayoutGroup);
                }
                imageToImageCopyGroup->addChild(srcLayoutGroup);
            }
            tilingGroup->addChild(imageToImageCopyGroup);
        }
        group->addChild(tilingGroup);
    }

    tcu::TestCaseGroup *const propertiesGroup = new tcu::TestCaseGroup(testCtx, "properties");
    propertiesGroup->addChild(new PropertiesTestCase(testCtx, "properties"));

    const struct QueryFormats
    {
        vk::VkFormat format;
    } queryFormats[] = {
        {vk::VK_FORMAT_R8G8B8A8_UNORM},    {vk::VK_FORMAT_R32G32B32A32_SFLOAT}, {vk::VK_FORMAT_R16_UNORM},
        {vk::VK_FORMAT_R16G16_UINT},       {vk::VK_FORMAT_B8G8R8A8_SINT},       {vk::VK_FORMAT_R16_SFLOAT},
        {vk::VK_FORMAT_D24_UNORM_S8_UINT}, {vk::VK_FORMAT_D32_SFLOAT},          {vk::VK_FORMAT_BC7_UNORM_BLOCK},
        {vk::VK_FORMAT_BC5_SNORM_BLOCK},
    };

    group->addChild(propertiesGroup);

    tcu::TestCaseGroup *const queryGroup = new tcu::TestCaseGroup(testCtx, "query");

    for (const auto &tiling : tilingTests)
    {
        tcu::TestCaseGroup *const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name);
        for (const auto &format : queryFormats)
        {
            const auto formatName = getFormatShortString(format.format);
            tilingGroup->addChild(new QueryTestCase(testCtx, formatName.c_str(), format.format, tiling.tiling));
        }
        queryGroup->addChild(tilingGroup);
    }

    group->addChild(queryGroup);

    tcu::TestCaseGroup *const identicalMemoryLayoutGroup = new tcu::TestCaseGroup(testCtx, "identical_memory_layout");

    for (const auto &tiling : tilingTests)
    {
        tcu::TestCaseGroup *const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name);
        for (const auto &format : queryFormats)
        {
            const auto formatName = getFormatShortString(format.format);
            tilingGroup->addChild(
                new IdenticalMemoryLayoutTestCase(testCtx, formatName.c_str(), format.format, tiling.tiling));
        }
        identicalMemoryLayoutGroup->addChild(tilingGroup);
    }

    {
        const std::vector<VkFormat> dsFormats{
            VK_FORMAT_D16_UNORM,         VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT,         VK_FORMAT_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,   VK_FORMAT_D32_SFLOAT_S8_UINT,
        };

        de::MovePtr<tcu::TestCaseGroup> depthStencilGroup(new tcu::TestCaseGroup(testCtx, "depth_stencil"));
        for (const auto &format : dsFormats)
        {
            const DepthStencilHICParams params{
                format,
                tcu::IVec2(256, 256),
            };
            const auto testName = getFormatShortString(format);
            depthStencilGroup->addChild(new DepthStencilHostImageCopyTest(testCtx, testName, params));
        }

        group->addChild(depthStencilGroup.release());
    }

    group->addChild(identicalMemoryLayoutGroup);
}

} // namespace

tcu::TestCaseGroup *createImageHostImageCopyTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(createTestGroup(testCtx, "host_image_copy", testGenerator));
    return testGroup.release();
}

} // namespace image
} // namespace vkt
