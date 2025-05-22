/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Tests for render passses with multisample attachments
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassMultisampleTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuFloat.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

using namespace vk;

using tcu::BVec4;
using tcu::IVec2;
using tcu::IVec4;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec4;

using tcu::just;
using tcu::Maybe;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::pair;
using std::string;
using std::vector;

typedef de::SharedPtr<vk::Unique<VkImage>> VkImageSp;
typedef de::SharedPtr<vk::Unique<VkImageView>> VkImageViewSp;
typedef de::SharedPtr<vk::Unique<VkBuffer>> VkBufferSp;
typedef de::SharedPtr<vk::Unique<VkPipeline>> VkPipelineSp;

namespace vkt
{

namespace renderpass
{

namespace
{

enum
{
    MAX_COLOR_ATTACHMENT_COUNT = 4u
};

enum TestSeparateUsage
{
    TEST_DEPTH   = (1 << 0),
    TEST_STENCIL = (1 << 1)
};

template <typename T>
de::SharedPtr<T> safeSharedPtr(T *ptr)
{
    try
    {
        return de::SharedPtr<T>(ptr);
    }
    catch (...)
    {
        delete ptr;
        throw;
    }
}

VkImageAspectFlags getImageAspectFlags(VkFormat vkFormat)
{
    const tcu::TextureFormat format(mapVkFormat(vkFormat));
    const bool hasDepth(tcu::hasDepthComponent(format.order));
    const bool hasStencil(tcu::hasStencilComponent(format.order));

    if (hasDepth || hasStencil)
    {
        return (hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : (VkImageAspectFlagBits)0u) |
               (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : (VkImageAspectFlagBits)0u);
    }
    else
        return VK_IMAGE_ASPECT_COLOR_BIT;
}

void bindBufferMemory(const DeviceInterface &vk, VkDevice device, VkBuffer buffer, VkDeviceMemory mem,
                      VkDeviceSize memOffset)
{
    VK_CHECK(vk.bindBufferMemory(device, buffer, mem, memOffset));
}

void bindImageMemory(const DeviceInterface &vk, VkDevice device, VkImage image, VkDeviceMemory mem,
                     VkDeviceSize memOffset)
{
    VK_CHECK(vk.bindImageMemory(device, image, mem, memOffset));
}

de::MovePtr<Allocation> createBufferMemory(const DeviceInterface &vk, VkDevice device, Allocator &allocator,
                                           VkBuffer buffer)
{
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), MemoryRequirement::HostVisible));
    bindBufferMemory(vk, device, buffer, allocation->getMemory(), allocation->getOffset());
    return allocation;
}

de::MovePtr<Allocation> createImageMemory(const DeviceInterface &vk, VkDevice device, Allocator &allocator,
                                          VkImage image)
{
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getImageMemoryRequirements(vk, device, image), MemoryRequirement::Any));
    bindImageMemory(vk, device, image, allocation->getMemory(), allocation->getOffset());
    return allocation;
}

Move<VkImage> createImage(const DeviceInterface &vk, VkDevice device, VkImageCreateFlags flags, VkImageType imageType,
                          VkFormat format, VkExtent3D extent, uint32_t mipLevels, uint32_t arrayLayers,
                          VkSampleCountFlagBits samples, VkImageTiling tiling, VkImageUsageFlags usage,
                          VkSharingMode sharingMode, uint32_t queueFamilyCount, const uint32_t *pQueueFamilyIndices,
                          VkImageLayout initialLayout, TestSeparateUsage separateStencilUsage)
{
    VkImageUsageFlags depthUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    VkImageUsageFlags stencilUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    VkImageUsageFlags imageUsage(usage);

    if (separateStencilUsage)
    {
        if (separateStencilUsage == TEST_DEPTH)
            depthUsage = usage;
        else // (separateStencilUsage == TEST_STENCIL)
            stencilUsage = usage;

        imageUsage = depthUsage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }

    const VkImageStencilUsageCreateInfo stencilUsageInfo{VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO, nullptr,
                                                         stencilUsage};

    const VkImageCreateInfo pCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                        separateStencilUsage ? &stencilUsageInfo : nullptr,
                                        flags,
                                        imageType,
                                        format,
                                        extent,
                                        mipLevels,
                                        arrayLayers,
                                        samples,
                                        tiling,
                                        imageUsage,
                                        sharingMode,
                                        queueFamilyCount,
                                        pQueueFamilyIndices,
                                        initialLayout};

    return createImage(vk, device, &pCreateInfo);
}

Move<VkImageView> createImageView(const DeviceInterface &vk, VkDevice device, VkImageViewCreateFlags flags,
                                  VkImage image, VkImageViewType viewType, VkFormat format,
                                  VkComponentMapping components, VkImageSubresourceRange subresourceRange)
{
    const VkImageViewCreateInfo pCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, flags, image, viewType, format, components, subresourceRange,
    };
    return createImageView(vk, device, &pCreateInfo);
}

Move<VkImage> createImage(const InstanceInterface &vki, VkPhysicalDevice physicalDevice, const DeviceInterface &vkd,
                          VkDevice device, VkFormat vkFormat, VkSampleCountFlagBits sampleCountBit,
                          VkImageUsageFlags usage, uint32_t width, uint32_t height,
                          TestSeparateUsage separateStencilUsage = (TestSeparateUsage)0u)
{
    try
    {
        const tcu::TextureFormat format(mapVkFormat(vkFormat));
        const VkImageType imageType(VK_IMAGE_TYPE_2D);
        const VkImageTiling imageTiling(VK_IMAGE_TILING_OPTIMAL);
        const VkFormatProperties formatProperties(getPhysicalDeviceFormatProperties(vki, physicalDevice, vkFormat));
        const VkImageFormatProperties imageFormatProperties(
            getPhysicalDeviceImageFormatProperties(vki, physicalDevice, vkFormat, imageType, imageTiling, usage, 0u));
        const VkImageUsageFlags depthUsage   = (separateStencilUsage == TEST_DEPTH) ?
                                                   usage :
                                                   (VkImageUsageFlags)VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        const VkImageUsageFlags stencilUsage = (separateStencilUsage == TEST_STENCIL) ?
                                                   usage :
                                                   (VkImageUsageFlags)VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        const VkExtent3D imageExtent         = {width, height, 1u};

        if ((tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order)) &&
            (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
            TCU_THROW(NotSupportedError, "Format can't be used as depth stencil attachment");

        if (!(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order)) &&
            (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
            TCU_THROW(NotSupportedError, "Format can't be used as color attachment");

        if (imageFormatProperties.maxExtent.width < imageExtent.width ||
            imageFormatProperties.maxExtent.height < imageExtent.height ||
            ((imageFormatProperties.sampleCounts & sampleCountBit) == 0))
        {
            TCU_THROW(NotSupportedError, "Image type not supported");
        }

        if (separateStencilUsage)
        {
            const VkImageStencilUsageCreateInfo stencilUsageInfo = {
                VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO, //    VkStructureType            sType
                nullptr,                                           //    const void*                pNext
                stencilUsage                                       //    VkImageUsageFlags        stencilUsage
            };

            const VkPhysicalDeviceImageFormatInfo2 formatInfo2 = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2, //    VkStructureType            sType
                &stencilUsageInfo,                                     //    const void*                pNext
                vkFormat,                                              //    VkFormat                format
                imageType,                                             //    VkImageType                type
                imageTiling,                                           //    VkImageTiling            tiling
                depthUsage,                                            //    VkImageUsageFlags        usage
                (VkImageCreateFlags)0u                                 //    VkImageCreateFlags        flags
            };

            VkImageFormatProperties2 extProperties = {
                VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
                nullptr,
                {
                    {
                        0, // width
                        0, // height
                        0, // depth
                    },
                    0u, // maxMipLevels
                    0u, // maxArrayLayers
                    0,  // sampleCounts
                    0u, // maxResourceSize
                },
            };

            if ((vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &formatInfo2, &extProperties) ==
                 VK_ERROR_FORMAT_NOT_SUPPORTED) ||
                extProperties.imageFormatProperties.maxExtent.width < imageExtent.width ||
                extProperties.imageFormatProperties.maxExtent.height < imageExtent.height ||
                ((extProperties.imageFormatProperties.sampleCounts & sampleCountBit) == 0))
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
        }

        return createImage(vkd, device, 0u, imageType, vkFormat, imageExtent, 1u, 1u, sampleCountBit, imageTiling,
                           usage, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr, VK_IMAGE_LAYOUT_UNDEFINED,
                           separateStencilUsage);
    }
    catch (const vk::Error &error)
    {
        if (error.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Image format not supported");

        throw;
    }
}

Move<VkImageView> createImageAttachmentView(const DeviceInterface &vkd, VkDevice device, VkImage image, VkFormat format,
                                            VkImageAspectFlags aspect)
{
    const VkImageSubresourceRange range = {aspect, 0u, 1u, 0u, 1u};

    return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

Move<VkImageView> createSrcPrimaryInputImageView(const DeviceInterface &vkd, VkDevice device, VkImage image,
                                                 VkFormat format, VkImageAspectFlags aspect,
                                                 TestSeparateUsage testSeparateUsage)
{
    VkImageAspectFlags primaryDepthStencilAspect =
        (testSeparateUsage == TEST_STENCIL) ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;

    const VkImageSubresourceRange range = {
        aspect == (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT) ? primaryDepthStencilAspect : aspect, 0u,
        1u, 0u, 1u};

    return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

Move<VkImageView> createSrcSecondaryInputImageView(const DeviceInterface &vkd, VkDevice device, VkImage image,
                                                   VkFormat format, VkImageAspectFlags aspect,
                                                   TestSeparateUsage separateStencilUsage)
{
    if ((aspect == (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT)) && !separateStencilUsage)
    {
        const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u};

        return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(),
                               range);
    }
    else
        return Move<VkImageView>();
}

VkDeviceSize getPixelSize(VkFormat vkFormat)
{
    const tcu::TextureFormat format(mapVkFormat(vkFormat));

    return format.getPixelSize();
}

Move<VkBuffer> createBuffer(const DeviceInterface &vkd, VkDevice device, VkFormat format, uint32_t width,
                            uint32_t height)
{
    const VkBufferUsageFlags bufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const VkDeviceSize pixelSize(getPixelSize(format));
    const VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                           nullptr,
                                           0u,

                                           width * height * pixelSize,
                                           bufferUsage,

                                           VK_SHARING_MODE_EXCLUSIVE,
                                           0u,
                                           nullptr};
    return createBuffer(vkd, device, &createInfo);
}

VkSampleCountFlagBits sampleCountBitFromomSampleCount(uint32_t count)
{
    switch (count)
    {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    case 32:
        return VK_SAMPLE_COUNT_32_BIT;
    case 64:
        return VK_SAMPLE_COUNT_64_BIT;

    default:
        DE_FATAL("Invalid sample count");
        return (VkSampleCountFlagBits)(0x1u << count);
    }
}

std::vector<VkImageSp> createMultisampleImages(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                               const DeviceInterface &vkd, VkDevice device, VkFormat format,
                                               uint32_t sampleCount, uint32_t width, uint32_t height)
{
    std::vector<VkImageSp> images(sampleCount);

    for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
        images[imageNdx] = safeSharedPtr(new vk::Unique<VkImage>(
            createImage(vki, physicalDevice, vkd, device, format, sampleCountBitFromomSampleCount(sampleCount),
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height)));

    return images;
}

std::vector<VkImageSp> createSingleSampleImages(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                const DeviceInterface &vkd, VkDevice device, VkFormat format,
                                                uint32_t sampleCount, uint32_t width, uint32_t height)
{
    std::vector<VkImageSp> images(sampleCount);

    for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
        images[imageNdx] = safeSharedPtr(new vk::Unique<VkImage>(
            createImage(vki, physicalDevice, vkd, device, format, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, width, height)));

    return images;
}

std::vector<de::SharedPtr<Allocation>> createImageMemory(const DeviceInterface &vkd, VkDevice device,
                                                         Allocator &allocator, const std::vector<VkImageSp> images)
{
    std::vector<de::SharedPtr<Allocation>> memory(images.size());

    for (size_t memoryNdx = 0; memoryNdx < memory.size(); memoryNdx++)
        memory[memoryNdx] = safeSharedPtr(createImageMemory(vkd, device, allocator, **images[memoryNdx]).release());

    return memory;
}

std::vector<VkImageViewSp> createImageAttachmentViews(const DeviceInterface &vkd, VkDevice device,
                                                      const std::vector<VkImageSp> &images, VkFormat format,
                                                      VkImageAspectFlagBits aspect)
{
    std::vector<VkImageViewSp> views(images.size());

    for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
        views[imageNdx] = safeSharedPtr(
            new vk::Unique<VkImageView>(createImageAttachmentView(vkd, device, **images[imageNdx], format, aspect)));

    return views;
}

std::vector<VkBufferSp> createBuffers(const DeviceInterface &vkd, VkDevice device, VkFormat format,
                                      uint32_t sampleCount, uint32_t width, uint32_t height)
{
    std::vector<VkBufferSp> buffers(sampleCount);

    for (size_t bufferNdx = 0; bufferNdx < buffers.size(); bufferNdx++)
        buffers[bufferNdx] = safeSharedPtr(new vk::Unique<VkBuffer>(createBuffer(vkd, device, format, width, height)));

    return buffers;
}

std::vector<de::SharedPtr<Allocation>> createBufferMemory(const DeviceInterface &vkd, VkDevice device,
                                                          Allocator &allocator, const std::vector<VkBufferSp> buffers)
{
    std::vector<de::SharedPtr<Allocation>> memory(buffers.size());

    for (size_t memoryNdx = 0; memoryNdx < memory.size(); memoryNdx++)
        memory[memoryNdx] = safeSharedPtr(createBufferMemory(vkd, device, allocator, **buffers[memoryNdx]).release());

    return memory;
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat srcFormat, VkFormat dstFormat,
                                    uint32_t sampleCount, RenderingType renderingType,
                                    TestSeparateUsage separateStencilUsage)
{
    const VkSampleCountFlagBits samples(sampleCountBitFromomSampleCount(sampleCount));
    const uint32_t splitSubpassCount(deDivRoundUp32(sampleCount, MAX_COLOR_ATTACHMENT_COUNT));
    const tcu::TextureFormat format(mapVkFormat(srcFormat));
    const bool isDepthStencilFormat(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order));
    const VkImageAspectFlags inputAspect(
        separateStencilUsage == TEST_DEPTH   ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT :
        separateStencilUsage == TEST_STENCIL ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT :
                                               getImageAspectFlags(srcFormat));
    vector<SubpassDesc> subpasses;
    vector<vector<AttachmentRef>> dstAttachmentRefs(splitSubpassCount);
    vector<vector<AttachmentRef>> dstResolveAttachmentRefs(splitSubpassCount);
    vector<AttachmentDesc> attachments;
    vector<SubpassDep> dependencies;
    const AttachmentRef
        srcAttachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,             //   ||  const void* pNext;
            0u,                  //  uint32_t attachment; ||  uint32_t attachment;
            isDepthStencilFormat //  VkImageLayout layout; ||  VkImageLayout layout;
                ?
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0u // ||  VkImageAspectFlags aspectMask;
        );
    const AttachmentRef
        srcAttachmentInputRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                      //   ||  const void* pNext;
            0u,                                           //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,     //  VkImageLayout layout; ||  VkImageLayout layout;
            (renderingType == RENDERING_TYPE_RENDERPASS2) // ||  VkImageAspectFlags aspectMask;
                ?
                inputAspect :
                0u);

    {
        const AttachmentDesc
            srcAttachment //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
            (
                //  ||  VkStructureType sType;
                nullptr,   //   ||  const void* pNext;
                0u,        //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
                srcFormat, //  VkFormat format; ||  VkFormat format;
                samples,   //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
                VK_IMAGE_LAYOUT_UNDEFINED, //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
                VK_IMAGE_LAYOUT_GENERAL    //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
            );

        attachments.push_back(srcAttachment);
    }

    for (uint32_t splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
    {
        for (uint32_t sampleNdx = 0; sampleNdx < de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT,
                                                         sampleCount - splitSubpassIndex * MAX_COLOR_ATTACHMENT_COUNT);
             sampleNdx++)
        {
            // Multisample color attachment
            {
                const AttachmentDesc
                    dstAttachment //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
                    (
                        //  ||  VkStructureType sType;
                        nullptr,   //   ||  const void* pNext;
                        0u,        //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
                        dstFormat, //  VkFormat format; ||  VkFormat format;
                        samples,   //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
                        VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
                        VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
                        VK_IMAGE_LAYOUT_UNDEFINED, //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
                    );
                const AttachmentRef
                    dstAttachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
                    (
                        //  ||  VkStructureType sType;
                        nullptr,                                  //   ||  const void* pNext;
                        (uint32_t)attachments.size(),             //  uint32_t attachment; ||  uint32_t attachment;
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
                        0u                                        // ||  VkImageAspectFlags aspectMask;
                    );

                attachments.push_back(dstAttachment);
                dstAttachmentRefs[splitSubpassIndex].push_back(dstAttachmentRef);
            }
            // Resolve attachment
            {
                const AttachmentDesc
                    dstAttachment //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
                    (
                        //  ||  VkStructureType sType;
                        nullptr,   //   ||  const void* pNext;
                        0u,        //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
                        dstFormat, //  VkFormat format; ||  VkFormat format;
                        VK_SAMPLE_COUNT_1_BIT, //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
                        VK_ATTACHMENT_STORE_OP_STORE, //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
                        VK_ATTACHMENT_STORE_OP_STORE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
                        VK_IMAGE_LAYOUT_UNDEFINED, //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
                    );
                const AttachmentRef
                    dstAttachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
                    (
                        //  ||  VkStructureType sType;
                        nullptr,                                  //   ||  const void* pNext;
                        (uint32_t)attachments.size(),             //  uint32_t attachment; ||  uint32_t attachment;
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
                        0u                                        // ||  VkImageAspectFlags aspectMask;
                    );

                attachments.push_back(dstAttachment);
                dstResolveAttachmentRefs[splitSubpassIndex].push_back(dstAttachmentRef);
            }
        }
    }

    {
        {
            const SubpassDesc
                subpass //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
                (
                    //  ||  VkStructureType sType;
                    nullptr,                      //   ||  const void* pNext;
                    (VkSubpassDescriptionFlags)0, //  VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
                    VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
                    0u,                              //   ||  uint32_t viewMask;
                    0u, //  uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
                    nullptr, //  const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
                    isDepthStencilFormat ? 0u :
                                           1u, //  uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
                    isDepthStencilFormat ?
                        nullptr :
                        &srcAttachmentRef, //  const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
                    nullptr, //  const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
                    isDepthStencilFormat ?
                        &srcAttachmentRef :
                        nullptr, //  const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
                    0u,     //  uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
                    nullptr //  const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
                );

            subpasses.push_back(subpass);
        }

        for (uint32_t splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
        {
            {
                const SubpassDesc
                    subpass //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
                    (
                        //  ||  VkStructureType sType;
                        nullptr,                      //   ||  const void* pNext;
                        (VkSubpassDescriptionFlags)0, //  VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
                        VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
                        0u,                              //   ||  uint32_t viewMask;
                        1u, //  uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
                        &srcAttachmentInputRef, //  const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
                        (uint32_t)dstAttachmentRefs[splitSubpassIndex]
                            .size(), //  uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
                        &dstAttachmentRefs
                            [splitSubpassIndex]
                            [0], //  const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
                        &dstResolveAttachmentRefs
                            [splitSubpassIndex]
                            [0], //  const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
                        nullptr, //  const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
                        0u,     //  uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
                        nullptr //  const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
                    );
                subpasses.push_back(subpass);
            }
            {
                const SubpassDep
                    dependency //  VkSubpassDependency                            ||  VkSubpassDependency2KHR
                    (
                        // || VkStructureType sType;
                        nullptr,               // || const void* pNext;
                        0u,                    //  uint32_t srcSubpass; || uint32_t srcSubpass;
                        splitSubpassIndex + 1, //  uint32_t dstSubpass; || uint32_t dstSubpass;
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
                        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
                        VK_DEPENDENCY_BY_REGION_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
                        0u                           // || int32_t viewOffset;
                    );

                dependencies.push_back(dependency);
            }
        }
        // the last subpass must synchronize with all prior subpasses
        for (uint32_t splitSubpassIndex = 0; splitSubpassIndex < (splitSubpassCount - 1); splitSubpassIndex++)
        {
            const SubpassDep dependency //  VkSubpassDependency                            ||  VkSubpassDependency2KHR
                (
                    // || VkStructureType sType;
                    nullptr,               // || const void* pNext;
                    splitSubpassIndex + 1, //  uint32_t srcSubpass; || uint32_t srcSubpass;
                    splitSubpassCount,     //  uint32_t dstSubpass; || uint32_t dstSubpass;
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
                    VK_DEPENDENCY_BY_REGION_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
                    0u                           // || int32_t viewOffset;
                );
            dependencies.push_back(dependency);
        }
        const RenderPassCreateInfo
            renderPassCreator //  VkRenderPassCreateInfo                                        ||  VkRenderPassCreateInfo2KHR
            (
                //  VkStructureType sType; ||  VkStructureType sType;
                nullptr,                      //  const void* pNext; ||  const void* pNext;
                (VkRenderPassCreateFlags)0u,  //  VkRenderPassCreateFlags flags; ||  VkRenderPassCreateFlags flags;
                (uint32_t)attachments.size(), //  uint32_t attachmentCount; ||  uint32_t attachmentCount;
                &attachments
                    [0], //  const VkAttachmentDescription* pAttachments; ||  const VkAttachmentDescription2KHR* pAttachments;
                (uint32_t)subpasses.size(), //  uint32_t subpassCount; ||  uint32_t subpassCount;
                &subpasses
                    [0], //  const VkSubpassDescription* pSubpasses; ||  const VkSubpassDescription2KHR* pSubpasses;
                (uint32_t)dependencies.size(), //  uint32_t dependencyCount; ||  uint32_t dependencyCount;
                &dependencies
                    [0], //  const VkSubpassDependency* pDependencies; ||  const VkSubpassDependency2KHR* pDependencies;
                0u,      //   ||  uint32_t correlatedViewMaskCount;
                nullptr  //  ||  const uint32_t* pCorrelatedViewMasks;
            );

        return renderPassCreator.createRenderPass(vkd, device);
    }
}

Move<VkRenderPass> createRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat srcFormat, VkFormat dstFormat,
                                    uint32_t sampleCount, const RenderingType renderingType,
                                    const TestSeparateUsage separateStencilUsage)
{
    switch (renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1,
                                RenderPassCreateInfo1>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType,
                                                       separateStencilUsage);
    case RENDERING_TYPE_RENDERPASS2:
        return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2,
                                RenderPassCreateInfo2>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType,
                                                       separateStencilUsage);
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return Move<VkRenderPass>();
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

Move<VkFramebuffer> createFramebuffer(const DeviceInterface &vkd, VkDevice device, VkRenderPass renderPass,
                                      VkImageView srcImageView,
                                      const std::vector<VkImageViewSp> &dstMultisampleImageViews,
                                      const std::vector<VkImageViewSp> &dstSinglesampleImageViews, uint32_t width,
                                      uint32_t height)
{
    // when RenderPass was not created then we are testing dynamic rendering
    // and we can't create framebuffer without valid RenderPass object
    if (!renderPass)
        return Move<VkFramebuffer>();

    std::vector<VkImageView> attachments;

    attachments.reserve(dstMultisampleImageViews.size() + dstSinglesampleImageViews.size() + 1u);

    attachments.push_back(srcImageView);

    DE_ASSERT(dstMultisampleImageViews.size() == dstSinglesampleImageViews.size());

    for (size_t ndx = 0; ndx < dstMultisampleImageViews.size(); ndx++)
    {
        attachments.push_back(**dstMultisampleImageViews[ndx]);
        attachments.push_back(**dstSinglesampleImageViews[ndx]);
    }

    const VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0u,

                                                renderPass,
                                                (uint32_t)attachments.size(),
                                                &attachments[0],

                                                width,
                                                height,
                                                1u};

    return createFramebuffer(vkd, device, &createInfo);
}

Move<VkDescriptorSetLayout> createSplitDescriptorSetLayout(const DeviceInterface &vkd, VkDevice device,
                                                           VkFormat vkFormat)
{
    const tcu::TextureFormat format(mapVkFormat(vkFormat));
    const bool hasDepth(tcu::hasDepthComponent(format.order));
    const bool hasStencil(tcu::hasStencilComponent(format.order));
    const VkDescriptorSetLayoutBinding bindings[] = {
        {0u, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1u, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    const VkDescriptorSetLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
                                                        0u,

                                                        hasDepth && hasStencil ? 2u : 1u, bindings};

    return createDescriptorSetLayout(vkd, device, &createInfo);
}

#ifndef CTS_USES_VULKANSC
VkRenderingAttachmentLocationInfoKHR getRenderingAttachmentLocationInfo(std::vector<uint32_t> &colorAttachmentLocations,
                                                                        bool isDepthStencilFormat, uint32_t sampleCount,
                                                                        uint32_t subpassIndex)
{
    const uint32_t colorAttachmentCount(
        de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT, sampleCount - subpassIndex * MAX_COLOR_ATTACHMENT_COUNT));
    const uint32_t firstAttachment(subpassIndex * colorAttachmentCount + !isDepthStencilFormat);

    DE_ASSERT(firstAttachment + colorAttachmentCount <= colorAttachmentLocations.size());

    std::fill(colorAttachmentLocations.begin(), colorAttachmentLocations.end(), VK_ATTACHMENT_UNUSED);
    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        colorAttachmentLocations[firstAttachment + i] = i;

    return {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR, nullptr,
        (uint32_t)colorAttachmentLocations.size(), // uint32_t                colorAttachmentCount
        colorAttachmentLocations.data(),           // const uint32_t*        pColorAttachmentLocations
    };
}

VkRenderingInputAttachmentIndexInfoKHR getRenderingInputAttachmentIndexInfo(
    std::vector<uint32_t> &colorAttachmentInputIndices, uint32_t &depthAttachmentInputIndex,
    uint32_t &stencilAttachmentInputIndex, bool isDepthFormat, bool isStencilFormat, void *pNext = nullptr)
{
    depthAttachmentInputIndex   = 0u;
    stencilAttachmentInputIndex = 0u;

    std::fill(colorAttachmentInputIndices.begin(), colorAttachmentInputIndices.end(), VK_ATTACHMENT_UNUSED);
    if (!isDepthFormat && !isStencilFormat)
        colorAttachmentInputIndices[0] = 0;

    return {
        VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
        pNext,
        (uint32_t)colorAttachmentInputIndices.size(),           // uint32_t                colorAttachmentCount
        colorAttachmentInputIndices.data(),                     // const uint32_t*        pColorAttachmentInputIndices
        (isDepthFormat ? &depthAttachmentInputIndex : nullptr), // uint32_t*            pDepthInputAttachmentIndex
        (isStencilFormat ? &stencilAttachmentInputIndex : nullptr), // uint32_t*            pStencilInputAttachmentIndex
    };
}
#endif

Move<VkDescriptorPool> createSplitDescriptorPool(const DeviceInterface &vkd, VkDevice device)
{
    const VkDescriptorPoolSize size             = {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2u};
    const VkDescriptorPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                   nullptr,
                                                   VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                                   2u,
                                                   1u,
                                                   &size};

    return createDescriptorPool(vkd, device, &createInfo);
}

Move<VkDescriptorSet> createSplitDescriptorSet(const DeviceInterface &vkd, VkDevice device, VkDescriptorPool pool,
                                               VkDescriptorSetLayout layout, VkRenderPass renderPass,
                                               VkImageView primaryImageView, VkImageView secondaryImageView,
                                               VkImageLayout imageReadLayout)
{
    DE_UNREF(renderPass);

    const VkDescriptorSetAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,

                                                      pool, 1u, &layout};
    Move<VkDescriptorSet> set(allocateDescriptorSet(vkd, device, &allocateInfo));

    {
        const VkDescriptorImageInfo imageInfos[] = {{VK_NULL_HANDLE, primaryImageView, imageReadLayout},
                                                    {VK_NULL_HANDLE, secondaryImageView, imageReadLayout}};
        const VkWriteDescriptorSet writes[]      = {
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,

                  *set, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[0], nullptr, nullptr},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,

                  *set, 1u, 0u, 1u, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[1], nullptr, nullptr}};
        const uint32_t count = secondaryImageView != VK_NULL_HANDLE ? 2u : 1u;

        vkd.updateDescriptorSets(device, count, writes, 0u, nullptr);
    }
    return set;
}

struct TestConfig
{
    TestConfig(VkFormat format_, uint32_t sampleCount_, SharedGroupParams groupParams_,
               TestSeparateUsage separateStencilUsage_ = (TestSeparateUsage)0u)
        : format(format_)
        , sampleCount(sampleCount_)
        , groupParams(groupParams_)
        , separateStencilUsage(separateStencilUsage_)
    {
    }

    VkFormat format;
    uint32_t sampleCount;
    SharedGroupParams groupParams;
    TestSeparateUsage separateStencilUsage;
};

VkImageUsageFlags getSrcImageUsage(VkFormat vkFormat)
{
    const tcu::TextureFormat format(mapVkFormat(vkFormat));
    const bool hasDepth(tcu::hasDepthComponent(format.order));
    const bool hasStencil(tcu::hasStencilComponent(format.order));

    if (hasDepth || hasStencil)
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    else
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
}

VkFormat getDstFormat(VkFormat vkFormat, TestSeparateUsage separateStencilUsage)
{
    const tcu::TextureFormat format(mapVkFormat(vkFormat));
    const bool hasDepth(tcu::hasDepthComponent(format.order));
    const bool hasStencil(tcu::hasStencilComponent(format.order));

    if (hasDepth && hasStencil && !separateStencilUsage)
        return VK_FORMAT_R32G32_SFLOAT;
    else if (hasDepth || hasStencil)
        return VK_FORMAT_R32_SFLOAT;
    else
        return vkFormat;
}

VkImageLayout chooseSrcInputImageLayout(const SharedGroupParams groupParams)
{
#ifndef CTS_USES_VULKANSC
    if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        // use general layout for local reads for some tests
        if (groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            return VK_IMAGE_LAYOUT_GENERAL;
        return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
    }
#else
    DE_UNREF(groupParams);
#endif // CTS_USES_VULKANSC

    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

#ifndef CTS_USES_VULKANSC
void beginSecondaryCmdBuffer(const DeviceInterface &vk, VkCommandBuffer secCmdBuffer, VkFormat srcFormat,
                             VkFormat dstFormat, uint32_t colorAttachmentCount, uint32_t sampleCount)
{
    VkCommandBufferUsageFlags usageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    const tcu::TextureFormat format(mapVkFormat(srcFormat));
    const bool isDepthFormat(tcu::hasDepthComponent(format.order));
    const bool isStencilFormat(tcu::hasStencilComponent(format.order));
    std::vector<VkFormat> colorAttachmentFormats(colorAttachmentCount, dstFormat);

    const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                         // const void* pNext;
        0u,                                                              // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        colorAttachmentCount,                                            // uint32_t colorAttachmentCount;
        colorAttachmentFormats.data(),                                   // const VkFormat* pColorAttachmentFormats;
        isDepthFormat ? srcFormat : VK_FORMAT_UNDEFINED,                 // VkFormat depthAttachmentFormat;
        isStencilFormat ? srcFormat : VK_FORMAT_UNDEFINED,               // VkFormat stencilAttachmentFormat;
        sampleCountBitFromomSampleCount(sampleCount),                    // VkSampleCountFlagBits rasterizationSamples;
    };
    const VkCommandBufferInheritanceInfo bufferInheritanceInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        &inheritanceRenderingInfo,                         // const void* pNext;
        VK_NULL_HANDLE,                                    // VkRenderPass renderPass;
        0u,                                                // uint32_t subpass;
        VK_NULL_HANDLE,                                    // VkFramebuffer framebuffer;
        VK_FALSE,                                          // VkBool32 occlusionQueryEnable;
        (VkQueryControlFlags)0u,                           // VkQueryControlFlags queryFlags;
        (VkQueryPipelineStatisticFlags)0u                  // VkQueryPipelineStatisticFlags pipelineStatistics;
    };
    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo                       // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };
    VK_CHECK(vk.beginCommandBuffer(secCmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

class MultisampleRenderPassTestInstance : public TestInstance
{
public:
    MultisampleRenderPassTestInstance(Context &context, TestConfig config);
    ~MultisampleRenderPassTestInstance(void) = default;

    tcu::TestStatus iterate(void);

protected:
    void createRenderPipeline(void);
    void createSplitPipelines(void);

    template <typename RenderpassSubpass>
    tcu::TestStatus iterateInternal(void);
    tcu::TestStatus iterateInternalDynamicRendering(void);
#ifndef CTS_USES_VULKANSC
    void preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, VkImageAspectFlags aspectMask);
    void inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, VkImageAspectFlags aspectMask);
#endif // CTS_USES_VULKANSC
    void drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawNextSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, uint32_t splitPipelineNdx);
    void postRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);

    tcu::TestStatus verifyResult(void);

private:
    const SharedGroupParams m_groupParams;
    const TestSeparateUsage m_separateStencilUsage;

    const VkFormat m_srcFormat;
    const VkFormat m_dstFormat;
    const uint32_t m_sampleCount;
    const uint32_t m_width;
    const uint32_t m_height;
    const VkPushConstantRange m_pushConstantRange;

    const VkImageAspectFlags m_srcImageAspect;
    const VkImageUsageFlags m_srcImageUsage;
    const Unique<VkImage> m_srcImage;
    const de::UniquePtr<Allocation> m_srcImageMemory;
    const Unique<VkImageView> m_srcImageView;
    const Unique<VkImageView> m_srcPrimaryInputImageView;
    const Unique<VkImageView> m_srcSecondaryInputImageView;
    const VkImageLayout m_srcInputImageReadLayout;

    const std::vector<VkImageSp> m_dstMultisampleImages;
    const std::vector<de::SharedPtr<Allocation>> m_dstMultisampleImageMemory;
    const std::vector<VkImageViewSp> m_dstMultisampleImageViews;

    const std::vector<VkImageSp> m_dstSinglesampleImages;
    const std::vector<de::SharedPtr<Allocation>> m_dstSinglesampleImageMemory;
    const std::vector<VkImageViewSp> m_dstSinglesampleImageViews;

    const std::vector<VkBufferSp> m_dstBuffers;
    const std::vector<de::SharedPtr<Allocation>> m_dstBufferMemory;

    const Unique<VkRenderPass> m_renderPass;
    const Unique<VkFramebuffer> m_framebuffer;

    const PipelineLayoutWrapper m_renderPipelineLayout;
    GraphicsPipelineWrapper m_renderPipeline;

    const Unique<VkDescriptorSetLayout> m_splitDescriptorSetLayout;
    const PipelineLayoutWrapper m_splitPipelineLayout;
    std::vector<GraphicsPipelineWrapper> m_splitPipelines;
    const Unique<VkDescriptorPool> m_splitDescriptorPool;
    const Unique<VkDescriptorSet> m_splitDescriptorSet;

    const Unique<VkCommandPool> m_commandPool;
    tcu::ResultCollector m_resultCollector;
};

MultisampleRenderPassTestInstance::MultisampleRenderPassTestInstance(Context &context, TestConfig config)
    : TestInstance(context)
    , m_groupParams(config.groupParams)
    , m_separateStencilUsage(config.separateStencilUsage)
    , m_srcFormat(config.format)
    , m_dstFormat(getDstFormat(config.format, config.separateStencilUsage))
    , m_sampleCount(config.sampleCount)
    , m_width(32u)
    , m_height(32u)
    , m_pushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 4u}

    , m_srcImageAspect(getImageAspectFlags(m_srcFormat))
    , m_srcImageUsage(getSrcImageUsage(m_srcFormat))
    , m_srcImage(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(),
                             context.getDevice(), m_srcFormat, sampleCountBitFromomSampleCount(m_sampleCount),
                             m_srcImageUsage, m_width, m_height, m_separateStencilUsage))
    , m_srcImageMemory(createImageMemory(context.getDeviceInterface(), context.getDevice(),
                                         context.getDefaultAllocator(), *m_srcImage))
    , m_srcImageView(createImageAttachmentView(context.getDeviceInterface(), context.getDevice(), *m_srcImage,
                                               m_srcFormat, m_srcImageAspect))
    , m_srcPrimaryInputImageView(createSrcPrimaryInputImageView(context.getDeviceInterface(), context.getDevice(),
                                                                *m_srcImage, m_srcFormat, m_srcImageAspect,
                                                                m_separateStencilUsage))
    , m_srcSecondaryInputImageView(createSrcSecondaryInputImageView(context.getDeviceInterface(), context.getDevice(),
                                                                    *m_srcImage, m_srcFormat, m_srcImageAspect,
                                                                    m_separateStencilUsage))
    , m_srcInputImageReadLayout(chooseSrcInputImageLayout(config.groupParams))

    , m_dstMultisampleImages(createMultisampleImages(context.getInstanceInterface(), context.getPhysicalDevice(),
                                                     context.getDeviceInterface(), context.getDevice(), m_dstFormat,
                                                     m_sampleCount, m_width, m_height))
    , m_dstMultisampleImageMemory(createImageMemory(context.getDeviceInterface(), context.getDevice(),
                                                    context.getDefaultAllocator(), m_dstMultisampleImages))
    , m_dstMultisampleImageViews(createImageAttachmentViews(context.getDeviceInterface(), context.getDevice(),
                                                            m_dstMultisampleImages, m_dstFormat,
                                                            VK_IMAGE_ASPECT_COLOR_BIT))

    , m_dstSinglesampleImages(createSingleSampleImages(context.getInstanceInterface(), context.getPhysicalDevice(),
                                                       context.getDeviceInterface(), context.getDevice(), m_dstFormat,
                                                       m_sampleCount, m_width, m_height))
    , m_dstSinglesampleImageMemory(createImageMemory(context.getDeviceInterface(), context.getDevice(),
                                                     context.getDefaultAllocator(), m_dstSinglesampleImages))
    , m_dstSinglesampleImageViews(createImageAttachmentViews(context.getDeviceInterface(), context.getDevice(),
                                                             m_dstSinglesampleImages, m_dstFormat,
                                                             VK_IMAGE_ASPECT_COLOR_BIT))

    , m_dstBuffers(createBuffers(context.getDeviceInterface(), context.getDevice(), m_dstFormat, m_sampleCount, m_width,
                                 m_height))
    , m_dstBufferMemory(createBufferMemory(context.getDeviceInterface(), context.getDevice(),
                                           context.getDefaultAllocator(), m_dstBuffers))

    , m_renderPass(createRenderPass(context.getDeviceInterface(), context.getDevice(), m_srcFormat, m_dstFormat,
                                    m_sampleCount, m_groupParams->renderingType, m_separateStencilUsage))
    , m_framebuffer(createFramebuffer(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_srcImageView,
                                      m_dstMultisampleImageViews, m_dstSinglesampleImageViews, m_width, m_height))

    , m_renderPipelineLayout(m_groupParams->pipelineConstructionType, context.getDeviceInterface(), context.getDevice(),
                             VK_NULL_HANDLE, &m_pushConstantRange)
    , m_renderPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                       context.getDevice(), context.getDeviceExtensions(), m_groupParams->pipelineConstructionType)

    , m_splitDescriptorSetLayout(
          createSplitDescriptorSetLayout(context.getDeviceInterface(), context.getDevice(), m_srcFormat))
    , m_splitPipelineLayout(m_groupParams->pipelineConstructionType, context.getDeviceInterface(), context.getDevice(),
                            *m_splitDescriptorSetLayout, &m_pushConstantRange)
    , m_splitDescriptorPool(createSplitDescriptorPool(context.getDeviceInterface(), context.getDevice()))
    , m_splitDescriptorSet(createSplitDescriptorSet(
          context.getDeviceInterface(), context.getDevice(), *m_splitDescriptorPool, *m_splitDescriptorSetLayout,
          *m_renderPass, *m_srcPrimaryInputImageView, *m_srcSecondaryInputImageView, m_srcInputImageReadLayout))
    , m_commandPool(createCommandPool(context.getDeviceInterface(), context.getDevice(),
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
    createRenderPipeline();
    createSplitPipelines();
}

tcu::TestStatus MultisampleRenderPassTestInstance::iterate(void)
{
    switch (m_groupParams->renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return iterateInternal<RenderpassSubpass1>();
    case RENDERING_TYPE_RENDERPASS2:
        return iterateInternal<RenderpassSubpass2>();
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return iterateInternalDynamicRendering();
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

void MultisampleRenderPassTestInstance::createRenderPipeline(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    VkDevice device(m_context.getDevice());
    const tcu::TextureFormat format(mapVkFormat(m_srcFormat));
    const bool isDepthFormat(tcu::hasDepthComponent(format.order));
    const bool isStencilFormat(tcu::hasStencilComponent(format.order));
    const bool isDepthStencilFormat(isDepthFormat || isStencilFormat);
    const BinaryCollection &binaryCollection(m_context.getBinaryCollection());
    ShaderWrapper vertexShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u);
    ShaderWrapper fragmentShaderModule(vkd, device, binaryCollection.get("quad-frag"), 0u);
    uint32_t colorAttachmentCount(!isDepthStencilFormat);
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    const std::vector<VkViewport> viewports{makeViewport(m_width, m_height)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_width, m_height)};
    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;

    if (*m_renderPass == VK_NULL_HANDLE)
    {
        const uint32_t splitSubpassCount(deDivRoundUp32(m_sampleCount, MAX_COLOR_ATTACHMENT_COUNT));
        for (uint32_t splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
            colorAttachmentCount += de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT,
                                            m_sampleCount - splitSubpassIndex * MAX_COLOR_ATTACHMENT_COUNT);
    }

    // Disable blending
    const std::vector<VkPipelineColorBlendAttachmentState> attachmentBlendStates(
        colorAttachmentCount,
        {VK_FALSE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE,
         VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});
    const VkPipelineMultisampleStateCreateInfo multisampleState{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineMultisampleStateCreateFlags)0u,

        sampleCountBitFromomSampleCount(m_sampleCount),
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencilState{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineDepthStencilStateCreateFlags)0u,

        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        VK_TRUE,
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, ~0u, ~0u,
         0xFFu / (m_sampleCount + 1)},
        {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, ~0u, ~0u,
         0xFFu / (m_sampleCount + 1)},

        0.0f,
        1.0f};

    const VkPipelineColorBlendStateCreateInfo blendState{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineColorBlendStateCreateFlags)0u,
        VK_FALSE,
        VK_LOGIC_OP_COPY,
        colorAttachmentCount,
        ((isDepthStencilFormat && !!*m_renderPass) ? nullptr : attachmentBlendStates.data()),
        {0.0f, 0.0f, 0.0f, 0.0f}};

#ifndef CTS_USES_VULKANSC
    std::vector<VkFormat> colorAttachmentFormats(colorAttachmentCount, m_dstFormat);
    if (!isDepthStencilFormat)
        colorAttachmentFormats[0] = m_srcFormat;

    VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                      nullptr,
                                                      0u,
                                                      (uint32_t)colorAttachmentFormats.size(),
                                                      colorAttachmentFormats.data(),
                                                      (isDepthFormat ? m_srcFormat : VK_FORMAT_UNDEFINED),
                                                      (isStencilFormat ? m_srcFormat : VK_FORMAT_UNDEFINED)};

    if (*m_renderPass == VK_NULL_HANDLE)
        renderingCreateInfoWrapper.ptr = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

    m_renderPipeline.setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, m_renderPipelineLayout, *m_renderPass, 0u,
                                          vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                          nullptr, nullptr, renderingCreateInfoWrapper)
        .setupFragmentShaderState(m_renderPipelineLayout, *m_renderPass, 0u, fragmentShaderModule, &depthStencilState,
                                  &multisampleState)
        .setupFragmentOutputState(*m_renderPass, 0u, &blendState, &multisampleState)
        .setMonolithicPipelineLayout(m_renderPipelineLayout)
        .buildPipeline();
}

void MultisampleRenderPassTestInstance::createSplitPipelines(void)
{
    const InstanceInterface &vki(m_context.getInstanceInterface());
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkPhysicalDevice physicalDevice(m_context.getPhysicalDevice());
    const VkDevice device(m_context.getDevice());
    const std::vector<string> &deviceExtensions(m_context.getDeviceExtensions());

    const tcu::TextureFormat format(mapVkFormat(m_srcFormat));
    const bool isDepthFormat(tcu::hasDepthComponent(format.order));
    const bool isStencilFormat(tcu::hasStencilComponent(format.order));
    const bool isDepthStencilFormat(isDepthFormat || isStencilFormat);
    const uint32_t splitSubpassCount(deDivRoundUp32(m_sampleCount, MAX_COLOR_ATTACHMENT_COUNT));
    uint32_t colorAttachmentCount(de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT, m_sampleCount));
    const BinaryCollection &binaryCollection(m_context.getBinaryCollection());
    ShaderWrapper vertexShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u);
    ShaderWrapper fragmentShaderModule(vkd, device, binaryCollection.get("quad-split-frag"), 0u);

    DE_UNREF(isDepthStencilFormat);

    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
    RenderingAttachmentLocationInfoWrapper renderingAttachmentLocationInfoWrapper;
    RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfoWrapper;
    const std::vector<VkViewport> viewports{makeViewport(m_width, m_height)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_width, m_height)};

    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    VkPipelineMultisampleStateCreateInfo multisampleState       = initVulkanStructure();
    multisampleState.rasterizationSamples                       = sampleCountBitFromomSampleCount(m_sampleCount);

    // Disable blending
    const VkPipelineColorBlendAttachmentState attachmentBlendState{
        VK_FALSE,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const uint32_t maximalNumberOfAttachments = 1 + splitSubpassCount * MAX_COLOR_ATTACHMENT_COUNT;
    const std::vector<VkPipelineColorBlendAttachmentState> attachmentBlendStates(maximalNumberOfAttachments,
                                                                                 attachmentBlendState);

    VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   nullptr,
                                                   (VkPipelineColorBlendStateCreateFlags)0u,
                                                   VK_FALSE,
                                                   VK_LOGIC_OP_COPY,
                                                   colorAttachmentCount,
                                                   attachmentBlendStates.data(),
                                                   {0.0f, 0.0f, 0.0f, 0.0f}};

#ifndef CTS_USES_VULKANSC
    uint32_t depthAttachmentInputIndex(0);
    uint32_t stencilAttachmentInputIndex(0);
    std::vector<VkFormat> colorAttachmentFormats(maximalNumberOfAttachments, m_dstFormat);
    std::vector<uint32_t> colorAttachmentLocations;
    std::vector<uint32_t> colorAttachmentInputIndices;
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocation;
    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo;

    if (!isDepthStencilFormat)
        colorAttachmentFormats[0] = m_srcFormat;

    VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      &renderingInputAttachmentIndexInfo,
                                                      0u,
                                                      0u,
                                                      colorAttachmentFormats.data(),
                                                      (isDepthFormat ? m_srcFormat : VK_FORMAT_UNDEFINED),
                                                      (isStencilFormat ? m_srcFormat : VK_FORMAT_UNDEFINED)};
#endif // CTS_USES_VULKANSC

    m_splitPipelines.reserve(splitSubpassCount);
    for (uint32_t ndx = 0; ndx < splitSubpassCount; ndx++)
    {
#ifndef CTS_USES_VULKANSC
        if (*m_renderPass == VK_NULL_HANDLE)
        {
            colorAttachmentCount =
                !isDepthStencilFormat + splitSubpassCount * de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT,
                                                                    m_sampleCount - ndx * MAX_COLOR_ATTACHMENT_COUNT);
            blendState.attachmentCount               = colorAttachmentCount;
            renderingCreateInfo.colorAttachmentCount = colorAttachmentCount;

            colorAttachmentLocations.resize(colorAttachmentCount);
            colorAttachmentInputIndices.resize(colorAttachmentCount);

            renderingAttachmentLocation =
                getRenderingAttachmentLocationInfo(colorAttachmentLocations, isDepthStencilFormat, m_sampleCount, ndx);
            renderingInputAttachmentIndexInfo =
                getRenderingInputAttachmentIndexInfo(colorAttachmentInputIndices, depthAttachmentInputIndex,
                                                     stencilAttachmentInputIndex, isDepthFormat, isStencilFormat);
            renderingCreateInfoWrapper.ptr               = &renderingCreateInfo;
            renderingAttachmentLocationInfoWrapper.ptr   = &renderingAttachmentLocation;
            renderingInputAttachmentIndexInfoWrapper.ptr = &renderingInputAttachmentIndexInfo;
        }
#endif // CTS_USES_VULKANSC

        m_splitPipelines.emplace_back(vki, vkd, physicalDevice, device, deviceExtensions,
                                      m_groupParams->pipelineConstructionType);
        m_splitPipelines[ndx]
            .setDefaultDepthStencilState()
            .setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputState)
            .setupPreRasterizationShaderState(viewports, scissors, m_splitPipelineLayout, *m_renderPass, ndx + 1u,
                                              vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                              nullptr, nullptr, renderingCreateInfoWrapper)
            .setupFragmentShaderState(m_splitPipelineLayout, *m_renderPass, ndx + 1u, fragmentShaderModule, 0u,
                                      &multisampleState, 0, VK_NULL_HANDLE, {},
                                      renderingInputAttachmentIndexInfoWrapper)
            .setupFragmentOutputState(*m_renderPass, ndx + 1u, &blendState, &multisampleState, VK_NULL_HANDLE, {},
                                      renderingAttachmentLocationInfoWrapper)
            .setMonolithicPipelineLayout(m_splitPipelineLayout)
            .buildPipeline();
    }
}

template <typename RenderpassSubpass>
tcu::TestStatus MultisampleRenderPassTestInstance::iterateInternal(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);

    beginCommandBuffer(vkd, *commandBuffer);

    const VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                             nullptr,

                                             *m_renderPass,
                                             *m_framebuffer,

                                             {{0u, 0u}, {m_width, m_height}},

                                             0u,
                                             nullptr};

    RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);

    // Stencil needs to be cleared if it exists.
    if (tcu::hasStencilComponent(mapVkFormat(m_srcFormat).order))
    {
        const VkClearAttachment clearAttachment{
            VK_IMAGE_ASPECT_STENCIL_BIT,     // VkImageAspectFlags aspectMask;
            0,                               // uint32_t colorAttachment;
            makeClearValueDepthStencil(0, 0) // VkClearValue clearValue;
        };

        const VkClearRect clearRect{
            {{0u, 0u}, {m_width, m_height}},
            0, // uint32_t baseArrayLayer;
            1  // uint32_t layerCount;
        };

        vkd.cmdClearAttachments(*commandBuffer, 1, &clearAttachment, 1, &clearRect);
    }

    drawFirstSubpass(vkd, *commandBuffer);

    for (uint32_t splitPipelineNdx = 0; splitPipelineNdx < m_splitPipelines.size(); splitPipelineNdx++)
    {
        RenderpassSubpass::cmdNextSubpass(vkd, *commandBuffer, &subpassBeginInfo, &subpassEndInfo);
        drawNextSubpass(vkd, *commandBuffer, splitPipelineNdx);
    }

    RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

    postRenderCommands(vkd, *commandBuffer);

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);

    return verifyResult();
}

tcu::TestStatus MultisampleRenderPassTestInstance::iterateInternalDynamicRendering()
{
#ifndef CTS_USES_VULKANSC

    const DeviceInterface &vk(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkCommandBuffer> secCmdBuffer;

    const uint32_t splitSubpassCount(deDivRoundUp32(m_sampleCount, MAX_COLOR_ATTACHMENT_COUNT));
    const VkClearValue clearValue(makeClearValueColor(tcu::Vec4(0.0f)));
    const tcu::TextureFormat format(mapVkFormat(m_srcFormat));
    const bool isDepthFormat(tcu::hasDepthComponent(format.order));
    const bool isStencilFormat(tcu::hasStencilComponent(format.order));
    const bool isDepthStencilFormat(isDepthFormat || isStencilFormat);
    VkResolveModeFlagBits resolveMode(VK_RESOLVE_MODE_AVERAGE_BIT);
    VkImageAspectFlags aspectMask(VK_IMAGE_ASPECT_NONE);

    if (isDepthFormat)
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (isStencilFormat)
        aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    if (aspectMask == VK_IMAGE_ASPECT_NONE)
    {
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if (isIntFormat(m_srcFormat) || isUintFormat(m_srcFormat))
            resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    }

    uint32_t colorAttachmentIndex = !isDepthStencilFormat;
    uint32_t colorAttachmentCount = colorAttachmentIndex;
    for (uint32_t splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
        colorAttachmentCount += de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT,
                                        m_sampleCount - splitSubpassIndex * MAX_COLOR_ATTACHMENT_COUNT);

    uint32_t depthAttachmentInputIndex(0);
    uint32_t stencilAttachmentInputIndex(0);
    std::vector<uint32_t> colorAttachmentInputIndices(colorAttachmentCount, VK_ATTACHMENT_UNUSED);
    std::vector<uint32_t> colorAttachmentLocations(colorAttachmentCount, VK_ATTACHMENT_UNUSED);

    VkRenderingAttachmentInfo depthAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        *m_srcImageView,                          // VkImageView imageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                     // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                           // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        clearValue                                // VkClearValue clearValue;
    };
    std::vector<VkRenderingAttachmentInfo> colorAttachments(colorAttachmentCount, depthAttachment);

    // If depth/stencil attachments are used then they will be used as input attachments
    depthAttachment.imageLayout = m_srcInputImageReadLayout;

    // If stencil attachment is used then we need to clear it
    VkRenderingAttachmentInfo stencilAttachment = depthAttachment;
    stencilAttachment.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;

    // If source image has color aspect then we will use first color attachment as input for second subpass
    if (colorAttachmentIndex)
        colorAttachments[0].imageLayout = m_srcInputImageReadLayout;

    for (uint32_t i = 0; i < m_dstMultisampleImageViews.size(); ++i)
    {
        colorAttachments[colorAttachmentIndex].imageView        = **m_dstMultisampleImageViews[i];
        colorAttachments[colorAttachmentIndex].resolveImageView = **m_dstSinglesampleImageViews[i];
        colorAttachments[colorAttachmentIndex].resolveMode      = resolveMode;
        ++colorAttachmentIndex;
    }
    DE_ASSERT(colorAttachmentIndex == colorAttachmentCount);

    VkRenderingInfo renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        0,                                              // VkRenderingFlagsKHR flags;
        makeRect2D(m_width, m_height),                  // VkRect2D renderArea;
        1u,                                             // uint32_t layerCount;
        0u,                                             // uint32_t viewMask;
        (uint32_t)colorAttachments.size(),              // uint32_t colorAttachmentCount;
        colorAttachments.data(),                        // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        isDepthFormat ? &depthAttachment : nullptr,     // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        isStencilFormat ? &stencilAttachment : nullptr, // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    auto renderingInputAttachmentIndexInfo =
        getRenderingInputAttachmentIndexInfo(colorAttachmentInputIndices, depthAttachmentInputIndex,
                                             stencilAttachmentInputIndex, isDepthFormat, isStencilFormat);
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocation;

    if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        secCmdBuffer = allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        // record secondary command buffer
        beginSecondaryCmdBuffer(vk, *secCmdBuffer, m_srcFormat, m_dstFormat, colorAttachmentCount, m_sampleCount);
        vk.cmdBeginRendering(*secCmdBuffer, &renderingInfo);

        drawFirstSubpass(vk, *secCmdBuffer);
        inbetweenRenderCommands(vk, *secCmdBuffer, aspectMask);

        for (uint32_t splitPipelineNdx = 0; splitPipelineNdx < m_splitPipelines.size(); splitPipelineNdx++)
        {
            renderingAttachmentLocation = getRenderingAttachmentLocationInfo(
                colorAttachmentLocations, isDepthStencilFormat, m_sampleCount, splitPipelineNdx);
            vk.cmdSetRenderingAttachmentLocationsKHR(*secCmdBuffer, &renderingAttachmentLocation);
            vk.cmdSetRenderingInputAttachmentIndicesKHR(*secCmdBuffer, &renderingInputAttachmentIndexInfo);

            drawNextSubpass(vk, *secCmdBuffer, splitPipelineNdx);
        }

        vk.cmdEndRendering(*secCmdBuffer);
        endCommandBuffer(vk, *secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(vk, *cmdBuffer);
        preRenderCommands(vk, *cmdBuffer, aspectMask);
        vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);
        postRenderCommands(vk, *cmdBuffer);
        endCommandBuffer(vk, *cmdBuffer);
    }
    else
    {
        beginCommandBuffer(vk, *cmdBuffer);

        preRenderCommands(vk, *cmdBuffer, aspectMask);

        vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);

        drawFirstSubpass(vk, *cmdBuffer);

        inbetweenRenderCommands(vk, *cmdBuffer, aspectMask);

        for (uint32_t splitPipelineNdx = 0; splitPipelineNdx < m_splitPipelines.size(); splitPipelineNdx++)
        {
            renderingAttachmentLocation = getRenderingAttachmentLocationInfo(
                colorAttachmentLocations, isDepthStencilFormat, m_sampleCount, splitPipelineNdx);
            vk.cmdSetRenderingAttachmentLocationsKHR(*cmdBuffer, &renderingAttachmentLocation);
            vk.cmdSetRenderingInputAttachmentIndicesKHR(*cmdBuffer, &renderingInputAttachmentIndexInfo);

            drawNextSubpass(vk, *cmdBuffer, splitPipelineNdx);
        }

        vk.cmdEndRendering(*cmdBuffer);

        postRenderCommands(vk, *cmdBuffer);

        endCommandBuffer(vk, *cmdBuffer);
    }

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

#endif // CTS_USES_VULKANSC

    return verifyResult();
}

#ifndef CTS_USES_VULKANSC
void MultisampleRenderPassTestInstance::preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                                          VkImageAspectFlags aspectMask)
{
    const tcu::TextureFormat format(mapVkFormat(m_srcFormat));
    const VkImageSubresourceRange srcSubresourceRange(makeImageSubresourceRange(aspectMask, 0, 1, 0, 1));
    const VkImageSubresourceRange dstSubresourceRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));

    // Memory barrier to set singlesamepled image layout to COLOR_ATTACHMENT_OPTIMAL
    VkPipelineStageFlags dstStageMaskForSourceImage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkImageMemoryBarrier srcImageBarrier(makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                                VK_IMAGE_LAYOUT_UNDEFINED, m_srcInputImageReadLayout,
                                                                *m_srcImage, srcSubresourceRange));

    if (tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order))
    {
        dstStageMaskForSourceImage    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        srcImageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    // Memory barriers to set singlesamepled and multisample images layout to COLOR_ATTACHMENT_OPTIMAL
    std::vector<VkImageMemoryBarrier> dstImageBarriers(
        m_dstSinglesampleImages.size() + m_dstMultisampleImages.size(),
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_NULL_HANDLE, dstSubresourceRange));
    for (size_t dstNdx = 0; dstNdx < m_dstSinglesampleImages.size(); dstNdx++)
        dstImageBarriers[dstNdx].image = **m_dstSinglesampleImages[dstNdx];
    for (size_t dstNdx = m_dstSinglesampleImages.size(); dstNdx < dstImageBarriers.size(); dstNdx++)
        dstImageBarriers[dstNdx].image = **m_dstMultisampleImages[dstNdx - m_dstSinglesampleImages.size()];

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStageMaskForSourceImage, 0u, 0u, nullptr, 0u,
                          nullptr, 1u, &srcImageBarrier);
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          0u, 0u, nullptr, 0u, nullptr, (uint32_t)dstImageBarriers.size(), dstImageBarriers.data());
}

void MultisampleRenderPassTestInstance::inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                                                VkImageAspectFlags aspectMask)
{
    const VkImageSubresourceRange srcSubresourceRange(makeImageSubresourceRange(aspectMask, 0, 1, 0, 1));
    VkAccessFlags dstAccessMask(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    VkPipelineStageFlags dstStageMaskForSourceImage(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    if (aspectMask != VK_IMAGE_ASPECT_COLOR_BIT)
    {
        dstAccessMask              = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstStageMaskForSourceImage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

    VkImageMemoryBarrier imageBarrier(makeImageMemoryBarrier(dstAccessMask, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                                             m_srcInputImageReadLayout, m_srcInputImageReadLayout,
                                                             *m_srcImage, srcSubresourceRange));
    vk.cmdPipelineBarrier(cmdBuffer, dstStageMaskForSourceImage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr, 1u, &imageBarrier);
}
#endif // CTS_USES_VULKANSC

void MultisampleRenderPassTestInstance::drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderPipeline.getPipeline());
    for (uint32_t sampleNdx = 0; sampleNdx < m_sampleCount; sampleNdx++)
    {
        vk.cmdPushConstants(cmdBuffer, *m_renderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(sampleNdx),
                            &sampleNdx);
        vk.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
    }
}

void MultisampleRenderPassTestInstance::drawNextSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                                        uint32_t splitPipelineNdx)
{
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_splitPipelines[splitPipelineNdx].getPipeline());
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_splitPipelineLayout, 0u, 1u,
                             &*m_splitDescriptorSet, 0u, nullptr);
    vk.cmdPushConstants(cmdBuffer, *m_splitPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(splitPipelineNdx),
                        &splitPipelineNdx);
    vk.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
}

void MultisampleRenderPassTestInstance::postRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (m_groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
        oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    for (size_t dstNdx = 0; dstNdx < m_dstSinglesampleImages.size(); dstNdx++)
        copyImageToBuffer(vk, cmdBuffer, **m_dstSinglesampleImages[dstNdx], **m_dstBuffers[dstNdx],
                          tcu::IVec2(m_width, m_height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, oldLayout);
}

tcu::TestStatus MultisampleRenderPassTestInstance::verifyResult(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const tcu::TextureFormat format(mapVkFormat(m_dstFormat));
    const tcu::TextureFormat srcFormat(mapVkFormat(m_srcFormat));
    const bool verifyDepth(m_separateStencilUsage ? (m_separateStencilUsage == TEST_DEPTH) :
                                                    tcu::hasDepthComponent(srcFormat.order));
    const bool verifyStencil(m_separateStencilUsage ? (m_separateStencilUsage == TEST_STENCIL) :
                                                      tcu::hasStencilComponent(srcFormat.order));

    for (uint32_t sampleNdx = 0; sampleNdx < m_sampleCount; sampleNdx++)
    {
        Allocation *dstBufMem = m_dstBufferMemory[sampleNdx].get();
        invalidateAlloc(vkd, device, *dstBufMem);

        const std::string name("Sample" + de::toString(sampleNdx));
        const void *const ptr(dstBufMem->getHostPtr());
        const tcu::ConstPixelBufferAccess access(format, m_width, m_height, 1, ptr);
        tcu::TextureLevel reference(format, m_width, m_height);

        if (verifyDepth || verifyStencil)
        {
            if (verifyDepth)
            {
                for (uint32_t y = 0; y < m_height; y++)
                    for (uint32_t x = 0; x < m_width; x++)
                    {
                        const uint32_t x1 = x ^ sampleNdx;
                        const uint32_t y1 = y ^ sampleNdx;
                        const float range = 1.0f;
                        float depth       = 0.0f;
                        uint32_t divider  = 2;

                        // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                        for (size_t bitNdx = 0; bitNdx < 10; bitNdx++)
                        {
                            depth += (range / (float)divider) *
                                     (((bitNdx % 2 == 0 ? x1 : y1) & (0x1u << (bitNdx / 2u))) == 0u ? 0.0f : 1.0f);
                            divider *= 2;
                        }

                        reference.getAccess().setPixel(Vec4(depth, 0.0f, 0.0f, 0.0f), x, y);
                    }
            }
            if (verifyStencil)
            {
                for (uint32_t y = 0; y < m_height; y++)
                    for (uint32_t x = 0; x < m_width; x++)
                    {
                        const uint32_t stencil = sampleNdx + 1u;

                        if (verifyDepth)
                        {
                            const Vec4 src(reference.getAccess().getPixel(x, y));

                            reference.getAccess().setPixel(Vec4(src.x(), (float)stencil, 0.0f, 0.0f), x, y);
                        }
                        else
                            reference.getAccess().setPixel(Vec4((float)stencil, 0.0f, 0.0f, 0.0f), x, y);
                    }
            }
            {
                const Vec4 threshold(verifyDepth ? (1.0f / 1024.0f) : 0.0f, 0.0f, 0.0f, 0.0f);

                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), "",
                                                reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
                    m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));
            }
        }
        else
        {
            const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));

            switch (channelClass)
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            {
                const UVec4 bits(tcu::getTextureFormatBitDepth(format).cast<uint32_t>());
                const UVec4 minValue(0);
                const UVec4 range(UVec4(1u) << tcu::min(bits, UVec4(31)));
                const int componentCount(tcu::getNumUsedChannels(format.order));
                const uint32_t bitSize(bits[0] + bits[1] + bits[2] + bits[3]);

                for (uint32_t y = 0; y < m_height; y++)
                    for (uint32_t x = 0; x < m_width; x++)
                    {
                        const uint32_t x1 = x ^ sampleNdx;
                        const uint32_t y1 = y ^ sampleNdx;
                        UVec4 color(minValue);
                        uint32_t dstBitsUsed[4] = {0u, 0u, 0u, 0u};
                        uint32_t nextSrcBit     = 0;
                        uint32_t divider        = 2;

                        // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                        while (nextSrcBit < de::min(bitSize, 10u))
                        {
                            for (int compNdx = 0; compNdx < componentCount; compNdx++)
                            {
                                if (dstBitsUsed[compNdx] > bits[compNdx])
                                    continue;

                                color[compNdx] +=
                                    (range[compNdx] / divider) *
                                    (((nextSrcBit % 2 == 0 ? x1 : y1) & (0x1u << (nextSrcBit / 2u))) == 0u ? 0u : 1u);

                                nextSrcBit++;
                                dstBitsUsed[compNdx]++;
                            }

                            divider *= 2;
                        }

                        reference.getAccess().setPixel(color, x, y);
                    }

                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), "",
                                              reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
                    m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));

                break;
            }

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            {
                const UVec4 bits(tcu::getTextureFormatBitDepth(format).cast<uint32_t>());
                const IVec4 minValue(0);
                const IVec4 range((UVec4(1u) << tcu::min(bits, UVec4(30))).cast<int32_t>());
                const int componentCount(tcu::getNumUsedChannels(format.order));
                const uint32_t bitSize(bits[0] + bits[1] + bits[2] + bits[3]);

                for (uint32_t y = 0; y < m_height; y++)
                    for (uint32_t x = 0; x < m_width; x++)
                    {
                        const uint32_t x1 = x ^ sampleNdx;
                        const uint32_t y1 = y ^ sampleNdx;
                        IVec4 color(minValue);
                        uint32_t dstBitsUsed[4] = {0u, 0u, 0u, 0u};
                        uint32_t nextSrcBit     = 0;
                        uint32_t divider        = 2;

                        // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                        while (nextSrcBit < de::min(bitSize, 10u))
                        {
                            for (int compNdx = 0; compNdx < componentCount; compNdx++)
                            {
                                if (dstBitsUsed[compNdx] > bits[compNdx])
                                    continue;

                                color[compNdx] +=
                                    (range[compNdx] / divider) *
                                    (((nextSrcBit % 2 == 0 ? x1 : y1) & (0x1u << (nextSrcBit / 2u))) == 0u ? 0u : 1u);

                                nextSrcBit++;
                                dstBitsUsed[compNdx]++;
                            }

                            divider *= 2;
                        }

                        reference.getAccess().setPixel(color, x, y);
                    }

                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), "",
                                              reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
                    m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));

                break;
            }

            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            {
                const tcu::TextureFormatInfo info(tcu::getTextureFormatInfo(format));
                const UVec4 bits(tcu::getTextureFormatBitDepth(format).cast<uint32_t>());
                const Vec4 minLimit(-65536.0);
                const Vec4 maxLimit(65536.0);
                const Vec4 minValue(tcu::max(info.valueMin, minLimit));
                const Vec4 range(tcu::min(info.valueMax, maxLimit) - minValue);
                const bool isAlphaOnly = isAlphaOnlyFormat(m_dstFormat);
                const int componentCount(isAlphaOnly ? 4 : tcu::getNumUsedChannels(format.order));
                const uint32_t bitSize(bits[0] + bits[1] + bits[2] + bits[3]);

                for (uint32_t y = 0; y < m_height; y++)
                    for (uint32_t x = 0; x < m_width; x++)
                    {
                        const uint32_t x1 = x ^ sampleNdx;
                        const uint32_t y1 = y ^ sampleNdx;
                        Vec4 color(minValue);
                        uint32_t dstBitsUsed[4] = {0u, 0u, 0u, 0u};
                        uint32_t nextSrcBit     = 0;
                        uint32_t divider        = 2;

                        // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                        while (nextSrcBit < de::min(bitSize, 10u))
                        {
                            for (int compNdx = 0; compNdx < componentCount; compNdx++)
                            {
                                if (dstBitsUsed[compNdx] > bits[compNdx])
                                    continue;

                                color[compNdx] +=
                                    (range[compNdx] / (float)divider) *
                                    (((nextSrcBit % 2 == 0 ? x1 : y1) & (0x1u << (nextSrcBit / 2u))) == 0u ? 0.f : 1.f);

                                nextSrcBit++;
                                dstBitsUsed[compNdx]++;
                            }

                            divider *= 2;
                        }

                        if (tcu::isSRGB(format))
                            reference.getAccess().setPixel(tcu::linearToSRGB(color), x, y);
                        else
                            reference.getAccess().setPixel(color, x, y);
                    }

                if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
                {
                    // Convert target format ulps to float ulps and allow 64ulp differences
                    const UVec4 threshold(
                        64u *
                        (UVec4(1u) << (UVec4(23) - tcu::getTextureFormatMantissaBitDepth(format).cast<uint32_t>())));

                    if (!tcu::floatUlpThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), "",
                                                       reference.getAccess(), access, threshold,
                                                       tcu::COMPARE_LOG_ON_ERROR))
                        m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));
                }
                else
                {
                    // Allow error of 4 times the minimum presentable difference
                    const Vec4 threshold(
                        4.0f * 1.0f /
                        ((UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(format).cast<uint32_t>()) - 1u)
                            .cast<float>());

                    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), "",
                                                    reference.getAccess(), access, threshold,
                                                    tcu::COMPARE_LOG_ON_ERROR))
                        m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));
                }

                break;
            }

            default:
                DE_FATAL("Unknown channel class");
            }
        }
    }

    return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        const tcu::TextureFormat format(mapVkFormat(config.format));
        const tcu::TextureChannelClass channelClass(tcu::getTextureChannelClass(format.type));
        const bool testDepth(config.separateStencilUsage ? (config.separateStencilUsage == TEST_DEPTH) :
                                                           tcu::hasDepthComponent(format.order));
        const bool testStencil(config.separateStencilUsage ? (config.separateStencilUsage == TEST_STENCIL) :
                                                             tcu::hasStencilComponent(format.order));
        // Note: the vertex coordinates being set here ensure that only one of the triangles in the quad
        // is actually on-screen. By only having one triangle visible we can be certain that all fragments
        // are always completely covered which means that doing multisampled subpassLoads from inputAttachments
        // will always work as expected. If we have two triangles and the seam is on screen then the coverage
        // can affect the subpassLoad results.
        // The spec says "The fragment can only access the covered samples in its input SampleMask"
        dst.glslSources.add("quad-vert") << glu::VertexSource(
            "#version 450\n"
            "out gl_PerVertex {\n"
            "\tvec4 gl_Position;\n"
            "};\n"
            "highp float;\n"
            "void main (void) {\n"
            "\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -3.0 : 1.0,\n"
            "\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 3.0, 0.0, 1.0);\n"
            "}\n");

        if (testDepth)
        {
            const Vec4 minValue(0.0f);
            const Vec4 range(1.0f);
            std::ostringstream fragmentShader;

            fragmentShader << "#version 450\n"
                              "layout(push_constant) uniform PushConstant {\n"
                              "\thighp uint sampleIndex;\n"
                              "} pushConstants;\n"
                              "void main (void)\n"
                              "{\n"
                              "\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
                              "\tgl_SampleMask[0] = int((~0x0u) << sampleIndex);\n"
                              "\thighp float depth;\n"
                              "\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
                              "\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

            fragmentShader << "\tdepth = " << minValue[0] << ";\n";

            {
                uint32_t divider = 2;

                // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                for (size_t bitNdx = 0; bitNdx < 10; bitNdx++)
                {
                    fragmentShader << "\tdepth += " << (range[0] / (float)divider) << " * float(bitfieldExtract("
                                   << (bitNdx % 2 == 0 ? "x" : "y") << ", " << (bitNdx / 2) << ", 1));\n";

                    divider *= 2;
                }
            }

            fragmentShader << "\tgl_FragDepth = depth;\n"
                              "}\n";

            dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
        }
        else if (testStencil)
        {
            dst.glslSources.add("quad-frag")
                << glu::FragmentSource("#version 450\n"
                                       "layout(push_constant) uniform PushConstant {\n"
                                       "\thighp uint sampleIndex;\n"
                                       "} pushConstants;\n"
                                       "void main (void)\n"
                                       "{\n"
                                       "\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
                                       "\tgl_SampleMask[0] = int((~0x0u) << sampleIndex);\n"
                                       "}\n");
        }
        else
        {
            switch (channelClass)
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            {
                const UVec4 bits(tcu::getTextureFormatBitDepth(format).cast<uint32_t>());
                const UVec4 minValue(0);
                const UVec4 range(UVec4(1u) << tcu::min(bits, UVec4(31)));
                std::ostringstream fragmentShader;

                fragmentShader << "#version 450\n"
                                  "layout(location = 0) out highp uvec4 o_color;\n"
                                  "layout(push_constant) uniform PushConstant {\n"
                                  "\thighp uint sampleIndex;\n"
                                  "} pushConstants;\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
                                  "\tgl_SampleMask[0] = int(0x1u << sampleIndex);\n"
                                  "\thighp uint color[4];\n"
                                  "\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
                                  "\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

                for (int ndx = 0; ndx < 4; ndx++)
                    fragmentShader << "\tcolor[" << ndx << "] = " << minValue[ndx] << ";\n";

                {
                    const int componentCount = tcu::getNumUsedChannels(format.order);
                    const uint32_t bitSize(bits[0] + bits[1] + bits[2] + bits[3]);
                    uint32_t dstBitsUsed[4] = {0u, 0u, 0u, 0u};
                    uint32_t nextSrcBit     = 0;
                    uint32_t divider        = 2;

                    // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                    while (nextSrcBit < de::min(bitSize, 10u))
                    {
                        for (int compNdx = 0; compNdx < componentCount; compNdx++)
                        {
                            if (dstBitsUsed[compNdx] > bits[compNdx])
                                continue;

                            fragmentShader << "\tcolor[" << compNdx << "] += " << (range[compNdx] / divider)
                                           << " * bitfieldExtract(" << (nextSrcBit % 2 == 0 ? "x" : "y") << ", "
                                           << (nextSrcBit / 2) << ", 1);\n";

                            nextSrcBit++;
                            dstBitsUsed[compNdx]++;
                        }

                        divider *= 2;
                    }
                }

                fragmentShader << "\to_color = uvec4(color[0], color[1], color[2], color[3]);\n"
                                  "}\n";

                dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
                break;
            }

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            {
                const UVec4 bits(tcu::getTextureFormatBitDepth(format).cast<uint32_t>());
                const IVec4 minValue(0);
                const IVec4 range((UVec4(1u) << tcu::min(bits, UVec4(30))).cast<int32_t>());
                const IVec4 maxV((UVec4(1u) << (bits - UVec4(1u))).cast<int32_t>());
                const IVec4 clampMax(maxV - 1);
                const IVec4 clampMin(-maxV);
                std::ostringstream fragmentShader;

                fragmentShader << "#version 450\n"
                                  "layout(location = 0) out highp ivec4 o_color;\n"
                                  "layout(push_constant) uniform PushConstant {\n"
                                  "\thighp uint sampleIndex;\n"
                                  "} pushConstants;\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
                                  "\tgl_SampleMask[0] = int(0x1u << sampleIndex);\n"
                                  "\thighp int color[4];\n"
                                  "\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
                                  "\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

                for (int ndx = 0; ndx < 4; ndx++)
                    fragmentShader << "\tcolor[" << ndx << "] = " << minValue[ndx] << ";\n";

                {
                    const int componentCount = tcu::getNumUsedChannels(format.order);
                    const uint32_t bitSize(bits[0] + bits[1] + bits[2] + bits[3]);
                    uint32_t dstBitsUsed[4] = {0u, 0u, 0u, 0u};
                    uint32_t nextSrcBit     = 0;
                    uint32_t divider        = 2;

                    // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                    while (nextSrcBit < de::min(bitSize, 10u))
                    {
                        for (int compNdx = 0; compNdx < componentCount; compNdx++)
                        {
                            if (dstBitsUsed[compNdx] > bits[compNdx])
                                continue;

                            fragmentShader << "\tcolor[" << compNdx << "] += " << (range[compNdx] / divider)
                                           << " * int(bitfieldExtract(" << (nextSrcBit % 2 == 0 ? "x" : "y") << ", "
                                           << (nextSrcBit / 2) << ", 1));\n";

                            nextSrcBit++;
                            dstBitsUsed[compNdx]++;
                        }

                        divider *= 2;
                    }
                }

                // The spec doesn't define whether signed-integers are clamped on output,
                // so we'll clamp them explicitly to have well-defined outputs.
                fragmentShader << "\to_color = clamp(ivec4(color[0], color[1], color[2], color[3]), "
                               << "ivec4" << clampMin << ", ivec4" << clampMax << ");\n"
                               << "}\n";

                dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
                break;
            }

            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
            {
                const tcu::TextureFormatInfo info(tcu::getTextureFormatInfo(format));
                const UVec4 bits(tcu::getTextureFormatMantissaBitDepth(format).cast<uint32_t>());
                const Vec4 minLimit(-65536.0);
                const Vec4 maxLimit(65536.0);
                const Vec4 minValue(tcu::max(info.valueMin, minLimit));
                const Vec4 range(tcu::min(info.valueMax, maxLimit) - minValue);
                std::ostringstream fragmentShader;

                fragmentShader << "#version 450\n"
                                  "layout(location = 0) out highp vec4 o_color;\n"
                                  "layout(push_constant) uniform PushConstant {\n"
                                  "\thighp uint sampleIndex;\n"
                                  "} pushConstants;\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
                                  "\tgl_SampleMask[0] = int(0x1u << sampleIndex);\n"
                                  "\thighp float color[4];\n"
                                  "\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
                                  "\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

                for (int ndx = 0; ndx < 4; ndx++)
                    fragmentShader << "\tcolor[" << ndx << "] = " << minValue[ndx] << ";\n";

                {
                    const bool isAlphaOnly   = isAlphaOnlyFormat(config.format);
                    const int componentCount = (isAlphaOnly ? 4 : tcu::getNumUsedChannels(format.order));
                    const uint32_t bitSize(bits[0] + bits[1] + bits[2] + bits[3]);
                    uint32_t dstBitsUsed[4] = {0u, 0u, 0u, 0u};
                    uint32_t nextSrcBit     = 0;
                    uint32_t divider        = 2;

                    // \note Limited to ten bits since the target is 32x32, so there are 10 input bits
                    while (nextSrcBit < de::min(bitSize, 10u))
                    {
                        for (int compNdx = 0; compNdx < componentCount; compNdx++)
                        {
                            if (dstBitsUsed[compNdx] > bits[compNdx])
                                continue;

                            fragmentShader << "\tcolor[" << compNdx << "] += " << (range[compNdx] / (float)divider)
                                           << " * float(bitfieldExtract(" << (nextSrcBit % 2 == 0 ? "x" : "y") << ", "
                                           << (nextSrcBit / 2) << ", 1));\n";

                            nextSrcBit++;
                            dstBitsUsed[compNdx]++;
                        }

                        divider *= 2;
                    }
                }

                fragmentShader << "\to_color = vec4(color[0], color[1], color[2], color[3]);\n"
                                  "}\n";

                dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
                break;
            }

            default:
                DE_FATAL("Unknown channel class");
            }
        }

        if (tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order))
        {
            std::ostringstream splitShader;

            splitShader << "#version 450\n";

            if (testDepth && testStencil)
            {
                splitShader << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInputMS "
                               "i_depth;\n"
                            << "layout(input_attachment_index = 0, set = 0, binding = 1) uniform highp usubpassInputMS "
                               "i_stencil;\n";
            }
            else if (testDepth)
                splitShader << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInputMS "
                               "i_depth;\n";
            else if (testStencil)
                splitShader << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp usubpassInputMS "
                               "i_stencil;\n";

            splitShader << "layout(push_constant) uniform PushConstant {\n"
                           "\thighp uint splitSubpassIndex;\n"
                           "} pushConstants;\n";

            for (uint32_t attachmentNdx = 0;
                 attachmentNdx < de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
            {
                if (testDepth && testStencil)
                    splitShader << "layout(location = " << attachmentNdx << ") out highp vec2 o_color" << attachmentNdx
                                << ";\n";
                else
                    splitShader << "layout(location = " << attachmentNdx << ") out highp float o_color" << attachmentNdx
                                << ";\n";
            }

            splitShader << "void main (void)\n"
                           "{\n";

            for (uint32_t attachmentNdx = 0;
                 attachmentNdx < de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
            {
                if (testDepth)
                    splitShader << "\thighp float depth" << attachmentNdx << " = subpassLoad(i_depth, int("
                                << MAX_COLOR_ATTACHMENT_COUNT << " * pushConstants.splitSubpassIndex + "
                                << attachmentNdx << "u)).x;\n";

                if (testStencil)
                    splitShader << "\thighp uint stencil" << attachmentNdx << " = subpassLoad(i_stencil, int("
                                << MAX_COLOR_ATTACHMENT_COUNT << " * pushConstants.splitSubpassIndex + "
                                << attachmentNdx << "u)).x;\n";

                if (testDepth && testStencil)
                    splitShader << "\to_color" << attachmentNdx << " = vec2(depth" << attachmentNdx << ", float(stencil"
                                << attachmentNdx << "));\n";
                else if (testDepth)
                    splitShader << "\to_color" << attachmentNdx << " = float(depth" << attachmentNdx << ");\n";
                else if (testStencil)
                    splitShader << "\to_color" << attachmentNdx << " = float(stencil" << attachmentNdx << ");\n";
            }

            splitShader << "}\n";

            dst.glslSources.add("quad-split-frag") << glu::FragmentSource(splitShader.str());
        }
        else
        {
            std::string subpassType;
            std::string outputType;

            switch (channelClass)
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
                subpassType = "usubpassInputMS";
                outputType  = "uvec4";
                break;

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
                subpassType = "isubpassInputMS";
                outputType  = "ivec4";
                break;

            case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
                subpassType = "subpassInputMS";
                outputType  = "vec4";
                break;

            default:
                DE_FATAL("Unknown channel class");
            }

            std::ostringstream splitShader;
            splitShader << "#version 450\n"
                           "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp "
                        << subpassType
                        << " i_color;\n"
                           "layout(push_constant) uniform PushConstant {\n"
                           "\thighp uint splitSubpassIndex;\n"
                           "} pushConstants;\n";

            for (uint32_t attachmentNdx = 0;
                 attachmentNdx < de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
                splitShader << "layout(location = " << attachmentNdx << ") out highp " << outputType << " o_color"
                            << attachmentNdx << ";\n";

            splitShader << "void main (void)\n"
                           "{\n";

            for (uint32_t attachmentNdx = 0;
                 attachmentNdx < de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
                splitShader << "\to_color" << attachmentNdx << " = subpassLoad(i_color, int("
                            << MAX_COLOR_ATTACHMENT_COUNT << " * pushConstants.splitSubpassIndex + " << attachmentNdx
                            << "u));\n";

            splitShader << "}\n";

            dst.glslSources.add("quad-split-frag") << glu::FragmentSource(splitShader.str());
        }
    }
};

void checkSupport(Context &context, TestConfig config)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    checkPipelineConstructionRequirements(vki, physicalDevice, config.groupParams->pipelineConstructionType);
    if (config.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    if (config.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        const vk::VkPhysicalDeviceProperties properties = vk::getPhysicalDeviceProperties(vki, physicalDevice);
        const uint32_t splitSubpassCount(deDivRoundUp32(config.sampleCount, MAX_COLOR_ATTACHMENT_COUNT));
        const tcu::TextureFormat format(mapVkFormat(config.format));
        const bool isDepthFormat(tcu::hasDepthComponent(format.order));
        const bool isStencilFormat(tcu::hasStencilComponent(format.order));

        uint32_t requiredColorAttachmentCount = !(isDepthFormat || isStencilFormat);
        for (uint32_t splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
            requiredColorAttachmentCount +=
                de::min((uint32_t)MAX_COLOR_ATTACHMENT_COUNT,
                        config.sampleCount - splitSubpassIndex * MAX_COLOR_ATTACHMENT_COUNT);

        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
        if (requiredColorAttachmentCount > properties.limits.maxColorAttachments)
            TCU_THROW(NotSupportedError, "Required number of color attachments not supported.");
    }

    if (config.separateStencilUsage)
    {
        context.requireDeviceFunctionality("VK_EXT_separate_stencil_usage");
        context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    }

#ifndef CTS_USES_VULKANSC
    if (config.format == VK_FORMAT_A8_UNORM_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC
}

std::string formatToName(VkFormat format)
{
    const std::string formatStr = de::toString(format);
    const std::string prefix    = "VK_FORMAT_";

    DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

    return de::toLower(formatStr.substr(prefix.length()));
}

void initTests(tcu::TestCaseGroup *group, const SharedGroupParams groupParams)
{
    static const VkFormat formats[] = {VK_FORMAT_R5G6B5_UNORM_PACK16,
                                       VK_FORMAT_R8_UNORM,
                                       VK_FORMAT_R8_SNORM,
                                       VK_FORMAT_R8_UINT,
                                       VK_FORMAT_R8_SINT,
                                       VK_FORMAT_R8G8_UNORM,
                                       VK_FORMAT_R8G8_SNORM,
                                       VK_FORMAT_R8G8_UINT,
                                       VK_FORMAT_R8G8_SINT,
#ifndef CTS_USES_VULKANSC
                                       VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
                                       VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_FORMAT_R8G8B8A8_SNORM,
                                       VK_FORMAT_R8G8B8A8_UINT,
                                       VK_FORMAT_R8G8B8A8_SINT,
                                       VK_FORMAT_R8G8B8A8_SRGB,
                                       VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                                       VK_FORMAT_A8B8G8R8_SNORM_PACK32,
                                       VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                       VK_FORMAT_A8B8G8R8_SINT_PACK32,
                                       VK_FORMAT_A8B8G8R8_SRGB_PACK32,
                                       VK_FORMAT_B8G8R8A8_UNORM,
                                       VK_FORMAT_B8G8R8A8_SRGB,
                                       VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                       VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                       VK_FORMAT_A2B10G10R10_UINT_PACK32,
                                       VK_FORMAT_R16_UNORM,
                                       VK_FORMAT_R16_SNORM,
                                       VK_FORMAT_R16_UINT,
                                       VK_FORMAT_R16_SINT,
                                       VK_FORMAT_R16_SFLOAT,
                                       VK_FORMAT_R16G16_UNORM,
                                       VK_FORMAT_R16G16_SNORM,
                                       VK_FORMAT_R16G16_UINT,
                                       VK_FORMAT_R16G16_SINT,
                                       VK_FORMAT_R16G16_SFLOAT,
                                       VK_FORMAT_R16G16B16A16_UNORM,
                                       VK_FORMAT_R16G16B16A16_SNORM,
                                       VK_FORMAT_R16G16B16A16_UINT,
                                       VK_FORMAT_R16G16B16A16_SINT,
                                       VK_FORMAT_R16G16B16A16_SFLOAT,
                                       VK_FORMAT_R32_UINT,
                                       VK_FORMAT_R32_SINT,
                                       VK_FORMAT_R32_SFLOAT,
                                       VK_FORMAT_R32G32_UINT,
                                       VK_FORMAT_R32G32_SINT,
                                       VK_FORMAT_R32G32_SFLOAT,
                                       VK_FORMAT_R32G32B32A32_UINT,
                                       VK_FORMAT_R32G32B32A32_SINT,
                                       VK_FORMAT_R32G32B32A32_SFLOAT,
                                       VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,

                                       VK_FORMAT_D16_UNORM,
                                       VK_FORMAT_X8_D24_UNORM_PACK32,
                                       VK_FORMAT_D32_SFLOAT,
                                       VK_FORMAT_S8_UINT,
                                       VK_FORMAT_D16_UNORM_S8_UINT,
                                       VK_FORMAT_D24_UNORM_S8_UINT,
                                       VK_FORMAT_D32_SFLOAT_S8_UINT};
    const uint32_t sampleCounts[] = {2u, 4u, 8u, 16u, 32u};
    tcu::TestContext &testCtx(group->getTestContext());
    de::MovePtr<tcu::TestCaseGroup> extGroup(new tcu::TestCaseGroup(testCtx, "separate_stencil_usage"));

    for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
    {
        const VkFormat format(formats[formatNdx]);
        const std::string formatName(formatToName(format));
        de::MovePtr<tcu::TestCaseGroup> formatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str()));
        de::MovePtr<tcu::TestCaseGroup> extFormatGroup(new tcu::TestCaseGroup(testCtx, formatName.c_str()));

        for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
        {
            // limit number of repeated tests for non monolithic pipelines
            if ((groupParams->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) &&
                (sampleCountNdx > 2))
                continue;

            const uint32_t sampleCount(sampleCounts[sampleCountNdx]);
            const TestConfig testConfig(format, sampleCount, groupParams);
            const std::string testName("samples_" + de::toString(sampleCount));

            formatGroup->addChild(new InstanceFactory1WithSupport<MultisampleRenderPassTestInstance, TestConfig,
                                                                  FunctionSupport1<TestConfig>, Programs>(
                testCtx, testName.c_str(), testConfig,
                typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));

            // create tests for VK_EXT_separate_stencil_usage
            if (tcu::hasDepthComponent(mapVkFormat(format).order) &&
                tcu::hasStencilComponent(mapVkFormat(format).order))
            {
                de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, testName.c_str()));
                {
                    const TestConfig separateUsageDepthTestConfig(format, sampleCount, groupParams, TEST_DEPTH);
                    sampleGroup->addChild(new InstanceFactory1WithSupport<MultisampleRenderPassTestInstance, TestConfig,
                                                                          FunctionSupport1<TestConfig>, Programs>(
                        testCtx, "test_depth", separateUsageDepthTestConfig,
                        typename FunctionSupport1<TestConfig>::Args(checkSupport, separateUsageDepthTestConfig)));

                    const TestConfig separateUsageStencilTestConfig(format, sampleCount, groupParams, TEST_STENCIL);
                    sampleGroup->addChild(new InstanceFactory1WithSupport<MultisampleRenderPassTestInstance, TestConfig,
                                                                          FunctionSupport1<TestConfig>, Programs>(
                        testCtx, "test_stencil", separateUsageStencilTestConfig,
                        typename FunctionSupport1<TestConfig>::Args(checkSupport, separateUsageStencilTestConfig)));
                }

                extFormatGroup->addChild(sampleGroup.release());
            }
        }

        group->addChild(formatGroup.release());
        extGroup->addChild(extFormatGroup.release());
    }

    group->addChild(extGroup.release());
}

} // namespace

tcu::TestCaseGroup *createRenderPassMultisampleTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "multisample", initTests, groupParams);
}

} // namespace renderpass

} // namespace vkt
