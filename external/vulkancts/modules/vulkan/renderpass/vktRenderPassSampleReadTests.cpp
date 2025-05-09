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
 * \brief Tests reading of samples from a previous subpass.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassSampleReadTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuResultCollector.hpp"

#include "deUniquePtr.hpp"

using namespace vk;

using tcu::UVec4;
using tcu::Vec4;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::string;
using std::vector;

namespace vkt
{
namespace
{
using namespace renderpass;

de::MovePtr<Allocation> createBufferMemory(const DeviceInterface &vk, VkDevice device, Allocator &allocator,
                                           VkBuffer buffer)
{
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), MemoryRequirement::HostVisible));
    VK_CHECK(vk.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
    return allocation;
}

de::MovePtr<Allocation> createImageMemory(const DeviceInterface &vk, VkDevice device, Allocator &allocator,
                                          VkImage image)
{
    de::MovePtr<Allocation> allocation(
        allocator.allocate(getImageMemoryRequirements(vk, device, image), MemoryRequirement::Any));
    VK_CHECK(vk.bindImageMemory(device, image, allocation->getMemory(), allocation->getOffset()));
    return allocation;
}

Move<VkImage> createImage(const DeviceInterface &vk, VkDevice device, VkImageCreateFlags flags, VkImageType imageType,
                          VkFormat format, VkExtent3D extent, uint32_t mipLevels, uint32_t arrayLayers,
                          VkSampleCountFlagBits samples, VkImageTiling tiling, VkImageUsageFlags usage,
                          VkSharingMode sharingMode, uint32_t queueFamilyCount, const uint32_t *pQueueFamilyIndices,
                          VkImageLayout initialLayout)
{
    const VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                          nullptr,
                                          flags,
                                          imageType,
                                          format,
                                          extent,
                                          mipLevels,
                                          arrayLayers,
                                          samples,
                                          tiling,
                                          usage,
                                          sharingMode,
                                          queueFamilyCount,
                                          pQueueFamilyIndices,
                                          initialLayout};
    return createImage(vk, device, &createInfo);
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
                          VkImageUsageFlags usage, uint32_t width, uint32_t height)
{
    try
    {
        const VkImageType imageType(VK_IMAGE_TYPE_2D);
        const VkImageTiling imageTiling(VK_IMAGE_TILING_OPTIMAL);
        const VkImageFormatProperties imageFormatProperties(
            getPhysicalDeviceImageFormatProperties(vki, physicalDevice, vkFormat, imageType, imageTiling, usage, 0u));
        const VkExtent3D imageExtent = {width, height, 1u};

        if (imageFormatProperties.maxExtent.width < imageExtent.width ||
            imageFormatProperties.maxExtent.height < imageExtent.height ||
            ((imageFormatProperties.sampleCounts & sampleCountBit) == 0))
        {
            TCU_THROW(NotSupportedError, "Image type not supported");
        }

        return createImage(vkd, device, 0u, imageType, vkFormat, imageExtent, 1u, 1u, sampleCountBit, imageTiling,
                           usage, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr, VK_IMAGE_LAYOUT_UNDEFINED);
    }
    catch (const vk::Error &error)
    {
        if (error.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Image format not supported");

        throw;
    }
}

Move<VkImageView> createImageView(const DeviceInterface &vkd, VkDevice device, VkImage image, VkFormat format,
                                  VkImageAspectFlags aspect)
{
    const VkImageSubresourceRange range = {aspect, 0u, 1u, 0u, 1u};

    return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
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
#endif
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

VkSampleCountFlagBits sampleCountBitFromSampleCount(uint32_t count)
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

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat srcFormat, VkFormat dstFormat,
                                    uint32_t sampleCount, RenderingType renderingType)
{
    const VkSampleCountFlagBits samples(sampleCountBitFromSampleCount(sampleCount));
    const VkImageAspectFlagBits aspectFlag((renderingType == RENDERING_TYPE_RENDERPASS2) ?
                                               VK_IMAGE_ASPECT_COLOR_BIT :
                                               static_cast<VkImageAspectFlagBits>(0u));
    const AttachmentRef
        srcAttachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                  //   ||  const void* pNext;
            0u,                                       //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                                        // ||  VkImageAspectFlags aspectMask;
        );
    const AttachmentRef
        srcAttachmentInputRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                  //   ||  const void* pNext;
            0u,                                       //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            aspectFlag                                // ||  VkImageAspectFlags aspectMask;
        );
    const AttachmentRef
        dstAttachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                  //   ||  const void* pNext;
            1u,                                       //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                                        // ||  VkImageAspectFlags aspectMask;
        );
    const AttachmentRef
        dstResolveAttachmentRef //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                  //   ||  const void* pNext;
            2u,                                       //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                                        // ||  VkImageAspectFlags aspectMask;
        );
    const SubpassDep
        dependency //  VkSubpassDependency                                            ||  VkSubpassDependency2KHR
        (
            //  || VkStructureType sType;
            nullptr,                                       //   || const void* pNext;
            0u,                                            //  uint32_t srcSubpass; || uint32_t srcSubpass;
            1u,                                            //  uint32_t dstSubpass; || uint32_t dstSubpass;
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,  //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
            VK_DEPENDENCY_BY_REGION_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
            0u                           //    || int32_t viewOffset;
        );
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
            VK_IMAGE_LAYOUT_UNDEFINED,        //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );
    const AttachmentDesc
        dstMultisampleAttachment //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,   //   ||  const void* pNext;
            0u,        //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            dstFormat, //  VkFormat format; ||  VkFormat format;
            samples,   //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,        //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );
    const AttachmentDesc
        dstResolveAttachment //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,               //   ||  const void* pNext;
            0u,                    //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            dstFormat,             //  VkFormat format; ||  VkFormat format;
            VK_SAMPLE_COUNT_1_BIT, //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,    //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_STORE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,    //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );
    const AttachmentDesc attachments[] = {srcAttachment, dstMultisampleAttachment, dstResolveAttachment};
    const SubpassDesc
        subpass1 //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                      //   ||  const void* pNext;
            (VkSubpassDescriptionFlags)0, //  VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
            0u,                              //   ||  uint32_t viewMask;
            0u,                              //  uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
            nullptr, //  const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
            1u, //  uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
            &srcAttachmentRef, //  const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
            nullptr, //  const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
            nullptr, //  const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
            0u,     //  uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
            nullptr //  const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
        );
    const SubpassDesc
        subpass2 //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                      //   ||  const void* pNext;
            (VkSubpassDescriptionFlags)0, //  VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
            0u,                              //   ||  uint32_t viewMask;
            1u,                              //  uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
            &srcAttachmentInputRef, //  const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
            1u, //  uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
            &dstAttachmentRef, //  const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
            &dstResolveAttachmentRef, //  const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
            nullptr, //  const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
            0u,     //  uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
            nullptr //  const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
        );
    const SubpassDesc subpasses[] = {subpass1, subpass2};
    const RenderPassCreateInfo
        renderPassCreator //  VkRenderPassCreateInfo                                        ||  VkRenderPassCreateInfo2KHR
        (
            //  VkStructureType sType; ||  VkStructureType sType;
            nullptr,                     //  const void* pNext; ||  const void* pNext;
            (VkRenderPassCreateFlags)0u, //  VkRenderPassCreateFlags flags; ||  VkRenderPassCreateFlags flags;
            3u,                          //  uint32_t attachmentCount; ||  uint32_t attachmentCount;
            attachments, //  const VkAttachmentDescription* pAttachments; ||  const VkAttachmentDescription2KHR* pAttachments;
            2u,          //  uint32_t subpassCount; ||  uint32_t subpassCount;
            subpasses,   //  const VkSubpassDescription* pSubpasses; ||  const VkSubpassDescription2KHR* pSubpasses;
            1u,          //  uint32_t dependencyCount; ||  uint32_t dependencyCount;
            &dependency, //  const VkSubpassDependency* pDependencies; ||  const VkSubpassDependency2KHR* pDependencies;
            0u,          //   ||  uint32_t correlatedViewMaskCount;
            nullptr      //  ||  const uint32_t* pCorrelatedViewMasks;
        );

    return renderPassCreator.createRenderPass(vkd, device);
}

Move<VkRenderPass> createRenderPass(const DeviceInterface &vkd, VkDevice device, VkFormat srcFormat, VkFormat dstFormat,
                                    uint32_t sampleCount, RenderingType renderingType)
{
    switch (renderingType)
    {
    case RENDERING_TYPE_RENDERPASS_LEGACY:
        return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1,
                                RenderPassCreateInfo1>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType);
    case RENDERING_TYPE_RENDERPASS2:
        return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2,
                                RenderPassCreateInfo2>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType);
    case RENDERING_TYPE_DYNAMIC_RENDERING:
        return Move<VkRenderPass>();
    default:
        TCU_THROW(InternalError, "Impossible");
    }
}

Move<VkFramebuffer> createFramebuffer(const DeviceInterface &vkd, VkDevice device, VkRenderPass renderPass,
                                      VkImageView srcImageView, VkImageView dstMultisampleImageView,
                                      VkImageView dstSinglesampleImageView, uint32_t width, uint32_t height)
{
    // when RenderPass was not created then we are testing dynamic rendering
    // and we can't create framebuffer without valid RenderPass object
    if (!renderPass)
        return Move<VkFramebuffer>();

    VkImageView attachments[] = {srcImageView, dstMultisampleImageView, dstSinglesampleImageView};

    const VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0u,

                                                renderPass,
                                                3u,
                                                attachments,

                                                width,
                                                height,
                                                1u};

    return createFramebuffer(vkd, device, &createInfo);
}

Move<VkDescriptorSetLayout> createSubpassDescriptorSetLayout(const DeviceInterface &vkd, VkDevice device)
{
    const VkDescriptorSetLayoutBinding bindings[] = {
        {0u, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1u, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    const VkDescriptorSetLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr,
                                                        0u,

                                                        1u, bindings};

    return createDescriptorSetLayout(vkd, device, &createInfo);
}

Move<VkDescriptorPool> createSubpassDescriptorPool(const DeviceInterface &vkd, VkDevice device)
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

Move<VkDescriptorSet> createSubpassDescriptorSet(const DeviceInterface &vkd, VkDevice device, VkRenderPass renderPass,
                                                 VkDescriptorPool pool, VkDescriptorSetLayout layout,
                                                 VkImageView imageView, VkImageLayout imageReadLayout)
{
    DE_UNREF(renderPass);

    const VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, pool, 1u,
                                                   &layout};
    Move<VkDescriptorSet> set(allocateDescriptorSet(vkd, device, &allocateInfo));
    const VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, imageView, imageReadLayout};
    const VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     nullptr,

                                     *set,
                                     0u,
                                     0u,
                                     1u,
                                     VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                                     &imageInfo,
                                     nullptr,
                                     nullptr};

    vkd.updateDescriptorSets(device, 1u, &write, 0u, nullptr);

    return set;
}

#ifndef CTS_USES_VULKANSC
void beginSecondaryCmdBuffer(const DeviceInterface &vk, VkCommandBuffer secCmdBuffer)
{
    VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkFormat colorAttachmentFormats[]    = {VK_FORMAT_R32_UINT, VK_FORMAT_R8_UNORM};
    const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                         // const void* pNext;
        0u,                                                              // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        2u,                                                              // uint32_t colorAttachmentCount;
        colorAttachmentFormats,                                          // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        VK_SAMPLE_COUNT_1_BIT,                                           // VkSampleCountFlagBits rasterizationSamples;
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

enum TestMode
{
    TESTMODE_ADD = 0,
    TESTMODE_SELECT,

    TESTMODE_LAST
};

struct TestConfig
{
    TestConfig(uint32_t sampleCount_, TestMode testMode_, uint32_t selectedSample_,
               const SharedGroupParams groupParams_)
        : sampleCount(sampleCount_)
        , testMode(testMode_)
        , selectedSample(selectedSample_)
        , groupParams(groupParams_)
    {
    }

    uint32_t sampleCount;
    TestMode testMode;
    uint32_t selectedSample;
    const SharedGroupParams groupParams;
};

class SampleReadTestInstance : public TestInstance
{
public:
    SampleReadTestInstance(Context &context, TestConfig config);
    ~SampleReadTestInstance(void);

    tcu::TestStatus iterate(void);

protected:
    template <typename RenderpassSubpass>
    tcu::TestStatus iterateInternal(void);
    tcu::TestStatus iterateInternalDynamicRendering(void);

    void createRenderPipeline(void);
    void createSubpassPipeline(void);

#ifndef CTS_USES_VULKANSC
    void preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
#endif // CTS_USES_VULKANSC
    void drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawSecondSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void postRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);

    void verifyResult(void);

private:
    const SharedGroupParams m_groupParams;

    const uint32_t m_sampleCount;
    const uint32_t m_width;
    const uint32_t m_height;

    const Unique<VkImage> m_srcImage;
    const de::UniquePtr<Allocation> m_srcImageMemory;
    const Unique<VkImageView> m_srcImageView;
    const Unique<VkImageView> m_srcInputImageView;
    const VkImageLayout m_srcInputImageReadLayout;

    const Unique<VkImage> m_dstMultisampleImage;
    const de::UniquePtr<Allocation> m_dstMultisampleImageMemory;
    const Unique<VkImageView> m_dstMultisampleImageView;

    const Unique<VkImage> m_dstSinglesampleImage;
    const de::UniquePtr<Allocation> m_dstSinglesampleImageMemory;
    const Unique<VkImageView> m_dstSinglesampleImageView;

    const Unique<VkBuffer> m_dstBuffer;
    const de::UniquePtr<Allocation> m_dstBufferMemory;

    const Unique<VkRenderPass> m_renderPass;
    const Unique<VkFramebuffer> m_framebuffer;

    PipelineLayoutWrapper m_renderPipelineLayout;
    GraphicsPipelineWrapper m_renderPipeline;

    const Unique<VkDescriptorSetLayout> m_subpassDescriptorSetLayout;
    PipelineLayoutWrapper m_subpassPipelineLayout;
    GraphicsPipelineWrapper m_subpassPipeline;
    const Unique<VkDescriptorPool> m_subpassDescriptorPool;
    const Unique<VkDescriptorSet> m_subpassDescriptorSet;

    const Unique<VkCommandPool> m_commandPool;
    tcu::ResultCollector m_resultCollector;
};

SampleReadTestInstance::SampleReadTestInstance(Context &context, TestConfig config)
    : TestInstance(context)
    , m_groupParams(config.groupParams)
    , m_sampleCount(config.sampleCount)
    , m_width(32u)
    , m_height(32u)
    , m_srcImage(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(),
                             context.getDevice(), VK_FORMAT_R32_UINT, sampleCountBitFromSampleCount(m_sampleCount),
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, m_width,
                             m_height))
    , m_srcImageMemory(createImageMemory(context.getDeviceInterface(), context.getDevice(),
                                         context.getDefaultAllocator(), *m_srcImage))
    , m_srcImageView(createImageView(context.getDeviceInterface(), context.getDevice(), *m_srcImage, VK_FORMAT_R32_UINT,
                                     VK_IMAGE_ASPECT_COLOR_BIT))
    , m_srcInputImageView(createImageView(context.getDeviceInterface(), context.getDevice(), *m_srcImage,
                                          VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT))
    , m_srcInputImageReadLayout(chooseSrcInputImageLayout(config.groupParams))
    , m_dstMultisampleImage(createImage(context.getInstanceInterface(), context.getPhysicalDevice(),
                                        context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R8_UNORM,
                                        sampleCountBitFromSampleCount(m_sampleCount),
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, m_width, m_height))
    , m_dstMultisampleImageMemory(createImageMemory(context.getDeviceInterface(), context.getDevice(),
                                                    context.getDefaultAllocator(), *m_dstMultisampleImage))
    , m_dstMultisampleImageView(createImageView(context.getDeviceInterface(), context.getDevice(),
                                                *m_dstMultisampleImage, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT))
    , m_dstSinglesampleImage(
          createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(),
                      context.getDevice(), VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, m_width, m_height))
    , m_dstSinglesampleImageMemory(createImageMemory(context.getDeviceInterface(), context.getDevice(),
                                                     context.getDefaultAllocator(), *m_dstSinglesampleImage))
    , m_dstSinglesampleImageView(createImageView(context.getDeviceInterface(), context.getDevice(),
                                                 *m_dstSinglesampleImage, VK_FORMAT_R8_UNORM,
                                                 VK_IMAGE_ASPECT_COLOR_BIT))
    , m_dstBuffer(
          createBuffer(context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R8_UNORM, m_width, m_height))
    , m_dstBufferMemory(createBufferMemory(context.getDeviceInterface(), context.getDevice(),
                                           context.getDefaultAllocator(), *m_dstBuffer))
    , m_renderPass(createRenderPass(context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R32_UINT,
                                    VK_FORMAT_R8_UNORM, m_sampleCount, m_groupParams->renderingType))
    , m_framebuffer(createFramebuffer(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_srcImageView,
                                      *m_dstMultisampleImageView, *m_dstSinglesampleImageView, m_width, m_height))
    , m_renderPipelineLayout(m_groupParams->pipelineConstructionType, context.getDeviceInterface(), context.getDevice())
    , m_renderPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                       context.getDevice(), context.getDeviceExtensions(), m_groupParams->pipelineConstructionType)
    , m_subpassDescriptorSetLayout(createSubpassDescriptorSetLayout(context.getDeviceInterface(), context.getDevice()))
    , m_subpassPipelineLayout(m_groupParams->pipelineConstructionType, context.getDeviceInterface(),
                              context.getDevice(), 1u, &*m_subpassDescriptorSetLayout)
    , m_subpassPipeline(context.getInstanceInterface(), context.getDeviceInterface(), context.getPhysicalDevice(),
                        context.getDevice(), context.getDeviceExtensions(), m_groupParams->pipelineConstructionType)
    , m_subpassDescriptorPool(createSubpassDescriptorPool(context.getDeviceInterface(), context.getDevice()))
    , m_subpassDescriptorSet(createSubpassDescriptorSet(
          context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_subpassDescriptorPool,
          *m_subpassDescriptorSetLayout, *m_srcInputImageView, m_srcInputImageReadLayout))
    , m_commandPool(createCommandPool(context.getDeviceInterface(), context.getDevice(),
                                      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
    createRenderPipeline();
    createSubpassPipeline();
}

SampleReadTestInstance::~SampleReadTestInstance(void)
{
}

tcu::TestStatus SampleReadTestInstance::iterate(void)
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

template <typename RenderpassSubpass>
tcu::TestStatus SampleReadTestInstance::iterateInternal(void)
{
    const DeviceInterface &vkd(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(nullptr, VK_SUBPASS_CONTENTS_INLINE);
    const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(nullptr);

    beginCommandBuffer(vkd, *commandBuffer);

    {
        const VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                 nullptr,

                                                 *m_renderPass,
                                                 *m_framebuffer,

                                                 {{0u, 0u}, {m_width, m_height}},

                                                 0u,
                                                 nullptr};
        RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);
    }

    drawFirstSubpass(vkd, *commandBuffer);

    RenderpassSubpass::cmdNextSubpass(vkd, *commandBuffer, &subpassBeginInfo, &subpassEndInfo);

    drawSecondSubpass(vkd, *commandBuffer);

    RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

    postRenderCommands(vkd, *commandBuffer);

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);

    verifyResult();

    return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

tcu::TestStatus SampleReadTestInstance::iterateInternalDynamicRendering()
{
#ifndef CTS_USES_VULKANSC

    const DeviceInterface &vk(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    Move<VkCommandBuffer> secCmdBuffer;

    const VkClearValue clearValue(makeClearValueColor(tcu::Vec4(0.0f)));

    uint32_t colorAttachmentLocations[]{VK_ATTACHMENT_UNUSED, 0};
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo = initVulkanStructure();
    renderingAttachmentLocationInfo.colorAttachmentCount                 = 2u;
    renderingAttachmentLocationInfo.pColorAttachmentLocations            = colorAttachmentLocations;

    uint32_t colorAttachmentInputIndices[]{0, VK_ATTACHMENT_UNUSED};
    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo = initVulkanStructure();
    renderingInputAttachmentIndexInfo.colorAttachmentCount                   = 2u;
    renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices           = colorAttachmentInputIndices;

    std::vector<VkRenderingAttachmentInfo> colorAttachments(
        2u,
        {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType            sType
            nullptr,                                     // const void*                pNext
            *m_srcImageView,                             // VkImageView                imageView
            m_srcInputImageReadLayout,                   // VkImageLayout            imageLayout
            VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits    resolveMode
            VK_NULL_HANDLE,                              // VkImageView                resolveImageView
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout            resolveImageLayout
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,             // VkAttachmentLoadOp        loadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,            // VkAttachmentStoreOp        storeOp
            clearValue                                   // VkClearValue                clearValue
        });

    colorAttachments[1].imageView        = *m_dstMultisampleImageView;
    colorAttachments[1].resolveMode      = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachments[1].resolveImageView = *m_dstSinglesampleImageView;

    VkRenderingInfo renderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        nullptr,
        0,                             // VkRenderingFlagsKHR flags;
        makeRect2D(m_width, m_height), // VkRect2D renderArea;
        1u,                            // uint32_t layerCount;
        0u,                            // uint32_t viewMask;
        2u,                            // uint32_t colorAttachmentCount;
        colorAttachments.data(),       // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                       // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                       // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
    {
        secCmdBuffer = allocateCommandBuffer(vk, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        // record secondary command buffer
        beginSecondaryCmdBuffer(vk, *secCmdBuffer);
        vk.cmdBeginRendering(*secCmdBuffer, &renderingInfo);

        drawFirstSubpass(vk, *secCmdBuffer);
        inbetweenRenderCommands(vk, *secCmdBuffer);
        vk.cmdSetRenderingAttachmentLocationsKHR(*secCmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(*secCmdBuffer, &renderingInputAttachmentIndexInfo);
        drawSecondSubpass(vk, *secCmdBuffer);

        vk.cmdEndRendering(*secCmdBuffer);
        endCommandBuffer(vk, *secCmdBuffer);

        // record primary command buffer
        beginCommandBuffer(vk, *cmdBuffer);
        preRenderCommands(vk, *cmdBuffer);
        vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);
        postRenderCommands(vk, *cmdBuffer);
        endCommandBuffer(vk, *cmdBuffer);
    }
    else
    {
        beginCommandBuffer(vk, *cmdBuffer);

        preRenderCommands(vk, *cmdBuffer);

        vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
        drawFirstSubpass(vk, *cmdBuffer);
        inbetweenRenderCommands(vk, *cmdBuffer);
        vk.cmdSetRenderingAttachmentLocationsKHR(*cmdBuffer, &renderingAttachmentLocationInfo);
        vk.cmdSetRenderingInputAttachmentIndicesKHR(*cmdBuffer, &renderingInputAttachmentIndexInfo);
        drawSecondSubpass(vk, *cmdBuffer);
        vk.cmdEndRendering(*cmdBuffer);

        postRenderCommands(vk, *cmdBuffer);

        endCommandBuffer(vk, *cmdBuffer);
    }

    submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

    verifyResult();

#endif // CTS_USES_VULKANSC

    return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

void SampleReadTestInstance::createRenderPipeline()
{
    const DeviceInterface &vk(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    vk::BinaryCollection &binaryCollection(m_context.getBinaryCollection());
    const std::vector<VkViewport> viewports{makeViewport(tcu::UVec2(m_width, m_height))};
    const std::vector<VkRect2D> scissors{makeRect2D(tcu::UVec2(m_width, m_height))};
    ShaderWrapper vertexShaderModule(vk, device, binaryCollection.get("quad-vert"), 0);
    ShaderWrapper fragmentShaderModule(vk, device, binaryCollection.get("quad-frag"), 0);

    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    const VkPipelineMultisampleStateCreateInfo multisampleState{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineMultisampleStateCreateFlags)0u,

        sampleCountBitFromSampleCount(m_sampleCount),
        VK_TRUE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentState.colorWriteMask = 0xF;

    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(
        uint32_t(*m_renderPass == VK_NULL_HANDLE) + 1u, colorBlendAttachmentState);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
    colorBlendStateCreateInfo.attachmentCount                     = uint32_t(colorBlendAttachmentStates.size());
    colorBlendStateCreateInfo.pAttachments                        = colorBlendAttachmentStates.data();

#ifndef CTS_USES_VULKANSC
    VkFormat colorAttachmentFormats[] = {VK_FORMAT_R32_UINT, VK_FORMAT_R8_UNORM};
    VkPipelineRenderingCreateInfo renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      nullptr,
                                                      0u,
                                                      2u,
                                                      colorAttachmentFormats,
                                                      VK_FORMAT_UNDEFINED,
                                                      VK_FORMAT_UNDEFINED};

    if (*m_renderPass == VK_NULL_HANDLE)
        renderingCreateInfoWrapper.ptr = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

    m_renderPipeline.setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, m_renderPipelineLayout, *m_renderPass, 0u,
                                          vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                          nullptr, nullptr, renderingCreateInfoWrapper)
        .setupFragmentShaderState(m_renderPipelineLayout, *m_renderPass, 0u, fragmentShaderModule, 0u,
                                  &multisampleState)
        .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateCreateInfo, &multisampleState)
        .setMonolithicPipelineLayout(m_renderPipelineLayout)
        .buildPipeline();
}

void SampleReadTestInstance::createSubpassPipeline()
{
    const DeviceInterface &vk(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());
    vk::BinaryCollection &binaryCollection(m_context.getBinaryCollection());
    const std::vector<VkViewport> viewports{makeViewport(tcu::UVec2(m_width, m_height))};
    const std::vector<VkRect2D> scissors{makeRect2D(tcu::UVec2(m_width, m_height))};
    ShaderWrapper vertexShaderModule(vk, device, binaryCollection.get("quad-vert"), 0u);
    ShaderWrapper fragmentShaderModule(vk, device, binaryCollection.get("quad-subpass-frag"), 0u);

    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
    RenderingAttachmentLocationInfoWrapper renderingAttachmentLocationInfoWrapper;
    RenderingInputAttachmentIndexInfoWrapper renderingInputAttachmentIndexInfoWrapper;
    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();
    const VkPipelineMultisampleStateCreateInfo multisampleState{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        (VkPipelineMultisampleStateCreateFlags)0u,

        sampleCountBitFromSampleCount(m_sampleCount),
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentState.colorWriteMask = 0xF;

    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(
        uint32_t(*m_renderPass == VK_NULL_HANDLE) + 1u, colorBlendAttachmentState);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
    colorBlendStateCreateInfo.attachmentCount                     = uint32_t(colorBlendAttachmentStates.size());
    colorBlendStateCreateInfo.pAttachments                        = colorBlendAttachmentStates.data();

#ifndef CTS_USES_VULKANSC
    uint32_t colorAttachmentLocations[]                              = {VK_ATTACHMENT_UNUSED, 0};
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocation = initVulkanStructure();
    renderingAttachmentLocation.colorAttachmentCount                 = 2u;
    renderingAttachmentLocation.pColorAttachmentLocations            = colorAttachmentLocations;

    uint32_t colorAttachmentInputIndices[]{0, VK_ATTACHMENT_UNUSED};
    VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo = initVulkanStructure();
    renderingInputAttachmentIndexInfo.colorAttachmentCount                   = 2u;
    renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices           = colorAttachmentInputIndices;

    VkFormat colorAttachmentFormats[]                 = {VK_FORMAT_R32_UINT, VK_FORMAT_R8_UNORM};
    VkPipelineRenderingCreateInfo renderingCreateInfo = initVulkanStructure();
    renderingCreateInfo.colorAttachmentCount          = 2u;
    renderingCreateInfo.pColorAttachmentFormats       = colorAttachmentFormats;

    if (*m_renderPass == VK_NULL_HANDLE)
    {
        renderingCreateInfoWrapper.ptr           = &renderingCreateInfo;
        renderingAttachmentLocationInfoWrapper   = &renderingAttachmentLocation;
        renderingInputAttachmentIndexInfoWrapper = &renderingInputAttachmentIndexInfo;
    }
#endif // CTS_USES_VULKANSC

    m_subpassPipeline.setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setupVertexInputState(&vertexInputState)
        .setupPreRasterizationShaderState(viewports, scissors, m_subpassPipelineLayout, *m_renderPass, 1u,
                                          vertexShaderModule, 0u, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                          nullptr, nullptr, renderingCreateInfoWrapper)
        .setupFragmentShaderState(m_subpassPipelineLayout, *m_renderPass, 1u, fragmentShaderModule, 0u,
                                  &multisampleState, 0, VK_NULL_HANDLE, {}, renderingInputAttachmentIndexInfoWrapper)
        .setupFragmentOutputState(*m_renderPass, 1u, &colorBlendStateCreateInfo, &multisampleState, VK_NULL_HANDLE, {},
                                  renderingAttachmentLocationInfoWrapper)
        .setMonolithicPipelineLayout(m_subpassPipelineLayout)
        .buildPipeline();
}

#ifndef CTS_USES_VULKANSC
void SampleReadTestInstance::preRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkImageSubresourceRange subresourceRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
    VkImageMemoryBarrier imageBarriers[]{
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               m_srcInputImageReadLayout, *m_srcImage, subresourceRange),
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *m_dstMultisampleImage, subresourceRange),
        makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *m_dstSinglesampleImage, subresourceRange),
    };

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          0u, 0u, nullptr, 0u, nullptr, 3u, imageBarriers);
}

void SampleReadTestInstance::inbetweenRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkImageSubresourceRange subresourceRange(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
    VkImageMemoryBarrier imageBarrier(
        makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                               m_srcInputImageReadLayout, m_srcInputImageReadLayout, *m_srcImage, subresourceRange));

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr,
                          1u, &imageBarrier);
}
#endif // CTS_USES_VULKANSC

void SampleReadTestInstance::drawFirstSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderPipeline.getPipeline());
    vk.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
}

void SampleReadTestInstance::drawSecondSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_subpassPipeline.getPipeline());
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_subpassPipelineLayout, 0u, 1u,
                             &*m_subpassDescriptorSet, 0u, nullptr);
    vk.cmdDraw(cmdBuffer, 6u, 1u, 0u, 0u);
}

void SampleReadTestInstance::postRenderCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    auto srcStageMask = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (m_groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        srcStageMask = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    copyImageToBuffer(vk, cmdBuffer, *m_dstSinglesampleImage, *m_dstBuffer, tcu::IVec2(m_width, m_height),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, srcStageMask);
}

void SampleReadTestInstance::verifyResult()
{
    const DeviceInterface &vk(m_context.getDeviceInterface());
    const VkDevice device(m_context.getDevice());

    invalidateAlloc(vk, device, *m_dstBufferMemory);

    const tcu::TextureFormat format(mapVkFormat(VK_FORMAT_R8_UNORM));
    const void *const ptr(m_dstBufferMemory->getHostPtr());
    const tcu::ConstPixelBufferAccess access(format, m_width, m_height, 1, ptr);
    tcu::TextureLevel reference(format, m_width, m_height);

    // Check we got the 1.0f we expected
    for (uint32_t y = 0; y < m_height; y++)
        for (uint32_t x = 0; x < m_width; x++)
            reference.getAccess().setPixel(tcu::Vec4(1.0f, 0, 0, 1.0f), x, y);

    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "", "", reference.getAccess(), access,
                                    Vec4(0.0f), tcu::COMPARE_LOG_ON_ERROR))
        m_resultCollector.fail("Compare failed.");
}

struct Programs
{
    void init(vk::SourceCollections &dst, TestConfig config) const
    {
        std::ostringstream fragmentShader;
        std::ostringstream subpassShader;

        dst.glslSources.add("quad-vert") << glu::VertexSource(
            "#version 450\n"
            "out gl_PerVertex {\n"
            "\tvec4 gl_Position;\n"
            "};\n"
            "highp float;\n"
            "void main (void)\n"
            "{\n"
            "    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
            "                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
            "}\n");

        fragmentShader << "#version 450\n"
                          "layout(location = 0) out highp uvec4 o_color;\n"
                          "void main (void)\n"
                          "{\n"
                          "    o_color = uvec4(1u << gl_SampleID, 0, 0, 0);\n"
                          "}\n";

        dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());

        subpassShader
            << "#version 450\n"
               "precision mediump int;\n"
               "precision highp float;\n"
               "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp usubpassInputMS i_color;\n"
               "layout(location = 0) out highp vec4 o_color;\n"
               "void main (void)\n"
               "{\n"
               "    o_color = vec4(0.0);\n";

        if (config.testMode == TESTMODE_ADD)
        {
            subpassShader
                << "    uint load = 0;\n"
                << "    uint expect = 0;\n"
                << "    for (int i = 0; i < " << config.sampleCount << "; i++)\n"
                << "    {\n"
                << "        expect += (1 << i);\n"
                << "        if ((gl_SampleMaskIn[0] & (1 << i)) != 0)\n" // Only covered samples are valid to load
                << "            load += subpassLoad(i_color, i).r;\n"
                << "        else\n"
                << "            load += (1 << i);\n" // Non-covered samples must emulate a good result
                << "    }\n"
                << "    o_color.r = (load == expect) ? 1.0 : 0.0;\n";
        }
        else
        {
            subpassShader << "    float result = 1.0;\n" // Non-covered samples must emulate a good result
                          //      Only covered samples are valid to load
                          << "    if ((gl_SampleMaskIn[0] & (1 << " << de::toString(config.selectedSample)
                          << ")) != 0)\n"
                          << "    {\n"
                          << "        uint load = subpassLoad(i_color, " << de::toString(config.selectedSample)
                          << ").r;\n"
                          << "        result = (load == (1 << " << de::toString(config.selectedSample)
                          << ")) ? 1.0 : 0.0;\n"
                          << "    }\n"
                          << "    o_color.r = result;\n";
        }

        subpassShader << "}\n";

        dst.glslSources.add("quad-subpass-frag") << glu::FragmentSource(subpassShader.str());
    }
};

void checkSupport(vkt::Context &context, TestConfig config)
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          config.groupParams->pipelineConstructionType);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);

    if (config.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
    else if (config.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
}

void initTests(tcu::TestCaseGroup *group, const SharedGroupParams groupParams)
{
    const uint32_t sampleCounts[] = {2u, 4u, 8u, 16u, 32u};
    tcu::TestContext &testCtx(group->getTestContext());

    for (uint32_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
    {
        // limit number of repeated tests for non monolithic pipelines
        if ((groupParams->pipelineConstructionType != PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) && (sampleCountNdx > 1))
            continue;

        const uint32_t sampleCount(sampleCounts[sampleCountNdx]);
        {
            const TestConfig testConfig(sampleCount, TESTMODE_ADD, 0, groupParams);
            const std::string testName("numsamples_" + de::toString(sampleCount) + "_add");

            group->addChild(new InstanceFactory1WithSupport<SampleReadTestInstance, TestConfig,
                                                            FunctionSupport1<TestConfig>, Programs>(
                testCtx, testName.c_str(), testConfig,
                typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));
        }

        for (uint32_t sample = 0; sample < sampleCount; sample++)
        {
            const TestConfig testConfig(sampleCount, TESTMODE_SELECT, sample, groupParams);
            const std::string testName("numsamples_" + de::toString(sampleCount) + "_selected_sample_" +
                                       de::toString(sample));

            group->addChild(new InstanceFactory1WithSupport<SampleReadTestInstance, TestConfig,
                                                            FunctionSupport1<TestConfig>, Programs>(
                testCtx, testName.c_str(), testConfig,
                typename FunctionSupport1<TestConfig>::Args(checkSupport, testConfig)));
        }
    }
}

} // namespace

tcu::TestCaseGroup *createRenderPassSampleReadTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "sampleread", initTests, groupParams);
}

} // namespace vkt
