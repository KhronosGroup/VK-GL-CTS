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
 * \brief Vulkan Multi View Render Util
 *//*--------------------------------------------------------------------*/

#include "vktMultiViewRenderUtil.hpp"
#include "vktMultiViewRenderPassUtil.hpp"

#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

namespace vkt
{
namespace MultiView
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using std::vector;

VkImageAspectFlags getAspectFlags(tcu::TextureFormat format)
{
    VkImageAspectFlags aspectFlag = 0;
    aspectFlag |= (tcu::hasDepthComponent(format.order) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
    aspectFlag |= (tcu::hasStencilComponent(format.order) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    if (!aspectFlag)
        aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

    return aspectFlag;
}

VkFormat getStencilBufferFormat(const vk::VkFormat depthStencilImageFormat)
{
    const tcu::TextureFormat tcuFormat = mapVkFormat(depthStencilImageFormat);
    const VkFormat result = (tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS) ?
                                VK_FORMAT_S8_UINT :
                                VK_FORMAT_UNDEFINED;

    DE_ASSERT(result != VK_FORMAT_UNDEFINED);

    return result;
}

VkFormat getDepthBufferFormat(const vk::VkFormat depthStencilImageFormat)
{
    VkFormat result = VK_FORMAT_UNDEFINED;

    switch (depthStencilImageFormat)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    {
        result = VK_FORMAT_D16_UNORM;

        break;
    }

    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    {
        result = VK_FORMAT_D32_SFLOAT;
        break;
    }

    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    {
        result = VK_FORMAT_D24_UNORM_S8_UINT;
        break;
    }

    default:
        result = VK_FORMAT_UNDEFINED;
    }

    DE_ASSERT(result != VK_FORMAT_UNDEFINED);

    return result;
}

VkImageCreateInfo makeImageCreateInfo(const VkImageType imageType, const VkExtent3D &extent, const VkFormat format,
                                      const VkImageUsageFlags usage, const VkSampleCountFlagBits samples)
{
    const VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0,               // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        format,                              // VkFormat format;
        {extent.width, extent.height, 1u},   // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        extent.depth,                        // uint32_t arrayLayers;
        samples,                             // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    return imageInfo;
}

Move<VkDescriptorSetLayout> makeDescriptorSetLayout(const DeviceInterface &vk, const VkDevice device)
{
    const VkDescriptorSetLayoutBinding binding = {
        0u,                                      //uint32_t binding;
        vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, //VkDescriptorType descriptorType;
        1u,                                      //uint32_t descriptorCount;
        vk::VK_SHADER_STAGE_FRAGMENT_BIT,        //VkShaderStageFlags stageFlags;
        nullptr                                  //const VkSampler* pImmutableSamplers;
    };

    const VkDescriptorSetLayoutCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, //VkStructureType sType;
        nullptr,                                                 //const void* pNext;
        0u,                                                      //VkDescriptorSetLayoutCreateFlags flags;
        1u,                                                      //uint32_t bindingCount;
        &binding                                                 //const VkDescriptorSetLayoutBinding* pBindings;
    };
    return vk::createDescriptorSetLayout(vk, device, &createInfo);
}

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> makeRenderPass(const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat,
                                  const vector<uint32_t> &viewMasks, const VkSampleCountFlagBits samples,
                                  const VkAttachmentLoadOp colorLoadOp, const VkFormat dsFormat,
                                  const bool useGeneralLayout)
{
    const bool dsAttachmentAvailable    = (dsFormat != vk::VK_FORMAT_UNDEFINED);
    const bool colorAttachmentAvailable = (colorFormat != vk::VK_FORMAT_UNDEFINED);
    const uint32_t colorAttachmentCount = (colorAttachmentAvailable ? 1u : 0u);
    const uint32_t dsAttachmentIndex    = colorAttachmentCount;
    const uint32_t subpassCount         = static_cast<uint32_t>(viewMasks.size());
    const VkImageLayout colorAttachmentLayout =
        useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkImageLayout dsAttachmentLayout =
        useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    const AttachmentDesc
        colorAttachmentDescription // VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                         //   ||  const void* pNext;
            (VkAttachmentDescriptionFlags)0, //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            colorFormat,                     //  VkFormat format; ||  VkFormat format;
            samples,                         //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            colorLoadOp,                     //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,    //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            colorAttachmentLayout,            //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            colorAttachmentLayout             //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );

    const AttachmentRef
        colorAttachmentReference //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,               //   ||  const void* pNext;
            0u,                    //  uint32_t attachment; ||  uint32_t attachment;
            colorAttachmentLayout, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                     // ||  VkImageAspectFlags aspectMask;
        );

    const AttachmentDesc
        dsAttachmentDescription //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                         //   ||  const void* pNext;
            (VkAttachmentDescriptionFlags)0, //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            dsFormat,                        //  VkFormat format; ||  VkFormat format;
            samples,                         //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,      //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE, //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_LOAD,   //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_STORE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            dsAttachmentLayout,           //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            dsAttachmentLayout            //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );

    const AttachmentRef
        dsAttachmentReference //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,            //   ||  const void* pNext;
            dsAttachmentIndex,  //  uint32_t attachment; ||  uint32_t attachment;
            dsAttachmentLayout, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                  // ||  VkImageAspectFlags aspectMask;
        );

    std::vector<AttachmentDesc> attachmentDescriptions;

    if (colorAttachmentAvailable)
        attachmentDescriptions.push_back(colorAttachmentDescription);
    if (dsAttachmentAvailable)
        attachmentDescriptions.push_back(dsAttachmentDescription);

    const auto *colorAttachmentReferencePtr = (colorAttachmentAvailable ? &colorAttachmentReference : nullptr);
    const auto *dsAttachmentReferencePtr    = (dsAttachmentAvailable ? &dsAttachmentReference : nullptr);

    DE_ASSERT((typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo1)) ||
              (typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo2)));

    vector<SubpassDesc> subpassDescriptions;
    for (uint32_t subpassNdx = 0; subpassNdx < subpassCount; ++subpassNdx)
    {
        const uint32_t viewMask =
            (typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo2)) ? viewMasks[subpassNdx] : 0u;
        const SubpassDesc
            subpassDescription //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
            (
                //  ||  VkStructureType sType;
                nullptr,                      //   ||  const void* pNext;
                (VkSubpassDescriptionFlags)0, //  VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
                VK_PIPELINE_BIND_POINT_GRAPHICS, //  VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
                viewMask,                        //   ||  uint32_t viewMask;
                0u,                              //  uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
                nullptr, //  const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
                colorAttachmentCount, //  uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
                colorAttachmentReferencePtr, //  const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
                nullptr, //  const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
                dsAttachmentReferencePtr, //  const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
                0u,     //  uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
                nullptr //  const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
            );

        subpassDescriptions.push_back(subpassDescription);
    }

    const VkRenderPassMultiviewCreateInfo renderPassMultiviewInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        subpassCount,                                        // uint32_t subpassCount;
        &viewMasks[0],                                       // const uint32_t* pViewMasks;
        0u,                                                  // uint32_t dependencyCount;
        nullptr,                                             // const int32_t* pViewOffsets;
        0u,                                                  // uint32_t correlationMaskCount;
        nullptr,                                             // const uint32_t* pCorrelationMasks;
    };
    const VkRenderPassMultiviewCreateInfo *renderPassMultiviewInfoPtr =
        (typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo1)) ? &renderPassMultiviewInfo : nullptr;

    const VkPipelineStageFlags srcStageMask = dsAttachmentAvailable ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT :
                                                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkPipelineStageFlags dstStageMask = dsAttachmentAvailable ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT :
                                                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkAccessFlags srcAccessMask =
        dsAttachmentAvailable ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    const VkAccessFlags dstAccessMask =
        dsAttachmentAvailable ?
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT :
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vector<SubpassDep> subpassDependencies;
    for (uint32_t subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
    {
        uint32_t viewMask = viewMasks[subpassNdx];

        // For every view written in this subpass, there should be a dependency
        // to the next subpass that writes to the same view.
        for (uint32_t dstSubpassNdx = subpassNdx + 1; dstSubpassNdx < subpassCount; ++dstSubpassNdx)
        {
            if (viewMask & viewMasks[dstSubpassNdx])
            {
                viewMask &= ~viewMasks[dstSubpassNdx];

                const SubpassDep
                    subpassDependency //  VkSubpassDependency                                            ||  VkSubpassDependency2KHR
                    (
                        //  || VkStructureType sType;
                        nullptr,       //   || const void* pNext;
                        subpassNdx,    //  uint32_t srcSubpass; || uint32_t srcSubpass;
                        dstSubpassNdx, //  uint32_t dstSubpass; || uint32_t dstSubpass;
                        srcStageMask,  //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
                        dstStageMask,  //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
                        srcAccessMask, //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
                        dstAccessMask, //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
                        VK_DEPENDENCY_VIEW_LOCAL_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
                        0                             //    || int32_t viewOffset;
                    );
                subpassDependencies.push_back(subpassDependency);
            }
        }

        // If there are views left that are not written by any future subpasses,
        // there should be a external dependency.
        if (viewMask)
        {
            const SubpassDep
                subpassDependency //  VkSubpassDependency                                            ||  VkSubpassDependency2KHR
                (
                    //  || VkStructureType sType;
                    nullptr,             //   || const void* pNext;
                    subpassNdx,          //  uint32_t srcSubpass; || uint32_t srcSubpass;
                    VK_SUBPASS_EXTERNAL, //  uint32_t dstSubpass; || uint32_t dstSubpass;
                    srcStageMask,        //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
                    dstStageMask,        //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
                    srcAccessMask,       //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
                    dstAccessMask,       //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
                    VK_DEPENDENCY_BY_REGION_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
                    0                            //    || int32_t viewOffset;
                );
            subpassDependencies.push_back(subpassDependency);
        }
    }

    const RenderPassCreateInfo
        renderPassInfo //  VkRenderPassCreateInfo                                        ||  VkRenderPassCreateInfo2KHR
        (
            //  VkStructureType sType; ||  VkStructureType sType;
            renderPassMultiviewInfoPtr,          //  const void* pNext; ||  const void* pNext;
            (VkRenderPassCreateFlags)0,          //  VkRenderPassCreateFlags flags; ||  VkRenderPassCreateFlags flags;
            de::sizeU32(attachmentDescriptions), //  uint32_t attachmentCount; ||  uint32_t attachmentCount;
            de::dataOrNull(
                attachmentDescriptions), //  const VkAttachmentDescription* pAttachments; ||  const VkAttachmentDescription2KHR* pAttachments;
            subpassCount, //  uint32_t subpassCount; ||  uint32_t subpassCount;
            &subpassDescriptions
                [0],      //  const VkSubpassDescription* pSubpasses; ||  const VkSubpassDescription2KHR* pSubpasses;
            subpassCount, //  uint32_t dependencyCount; ||  uint32_t dependencyCount;
            &subpassDependencies
                [0], //  const VkSubpassDependency* pDependencies; ||  const VkSubpassDependency2KHR* pDependencies;
            0u,      //   ||  uint32_t correlatedViewMaskCount;
            nullptr  //  ||  const uint32_t* pCorrelatedViewMasks;
        );

    return renderPassInfo.createRenderPass(vk, device);
}

// Instantiate function for legacy renderpass structures
template Move<VkRenderPass> makeRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                           SubpassDependency1, RenderPassCreateInfo1>(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
    const VkSampleCountFlagBits samples, const VkAttachmentLoadOp colorLoadOp, const VkFormat dsFormat,
    const bool useGeneralLayout);

// Instantiate function for renderpass2 structures
template Move<VkRenderPass> makeRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                           SubpassDependency2, RenderPassCreateInfo2>(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
    const VkSampleCountFlagBits samples, const VkAttachmentLoadOp colorLoadOp, const VkFormat dsFormat,
    const bool useGeneralLayout);

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> makeRenderPassWithDepth(const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat,
                                           const vector<uint32_t> &viewMasks, const VkFormat dsFormat,
                                           const bool useGeneralLayout)
{
    return makeRenderPass<AttachmentDesc, AttachmentRef, SubpassDesc, SubpassDep, RenderPassCreateInfo>(
        vk, device, colorFormat, viewMasks, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, dsFormat,
        useGeneralLayout);
}

// Instantiate function for legacy renderpass structures
template Move<VkRenderPass> makeRenderPassWithDepth<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                                    SubpassDependency1, RenderPassCreateInfo1>(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
    const VkFormat dsFormat, const bool useGeneralLayout);

// Instantiate function for renderpass2 structures
template Move<VkRenderPass> makeRenderPassWithDepth<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                                    SubpassDependency2, RenderPassCreateInfo2>(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
    const VkFormat dsFormat, const bool useGeneralLayout);

template <typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep,
          typename RenderPassCreateInfo>
Move<VkRenderPass> makeRenderPassWithAttachments(const DeviceInterface &vk, const VkDevice device,
                                                 const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
                                                 bool useAspects)
{
    const uint32_t subpassCount = static_cast<uint32_t>(viewMasks.size());

    const AttachmentDesc
        colorAttachmentDescription //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                         //   ||  const void* pNext;
            (VkAttachmentDescriptionFlags)0, //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            colorFormat,                     //  VkFormat format; ||  VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,           //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,     //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,    //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );

    const AttachmentDesc
        inputAttachmentDescription //  VkAttachmentDescription                                        ||  VkAttachmentDescription2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                         //   ||  const void* pNext;
            (VkAttachmentDescriptionFlags)0, //  VkAttachmentDescriptionFlags flags; ||  VkAttachmentDescriptionFlags flags;
            colorFormat,                     //  VkFormat format; ||  VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,           //  VkSampleCountFlagBits samples; ||  VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,      //  VkAttachmentLoadOp loadOp; ||  VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,    //  VkAttachmentStoreOp storeOp; ||  VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, //  VkAttachmentLoadOp stencilLoadOp; ||  VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, //  VkAttachmentStoreOp stencilStoreOp; ||  VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,          //  VkImageLayout initialLayout; ||  VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           //  VkImageLayout finalLayout; ||  VkImageLayout finalLayout;
        );

    vector<AttachmentDesc> attachments;
    attachments.push_back(colorAttachmentDescription);
    attachments.push_back(inputAttachmentDescription);

    const AttachmentRef
        colorAttachmentReference //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                                  //   ||  const void* pNext;
            0u,                                       //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                                        // ||  VkImageAspectFlags aspectMask;
        );

    const AttachmentRef
        inputAttachmentReference //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                 //   ||  const void* pNext;
            1u,                      //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_GENERAL, //  VkImageLayout layout; ||  VkImageLayout layout;
            useAspects ? VK_IMAGE_ASPECT_COLOR_BIT :
                         static_cast<VkImageAspectFlagBits>(0u) // ||  VkImageAspectFlags aspectMask;
        );

    const AttachmentRef
        depthAttachmentReference //  VkAttachmentReference                                        ||  VkAttachmentReference2KHR
        (
            //  ||  VkStructureType sType;
            nullptr,                   //   ||  const void* pNext;
            VK_ATTACHMENT_UNUSED,      //  uint32_t attachment; ||  uint32_t attachment;
            VK_IMAGE_LAYOUT_UNDEFINED, //  VkImageLayout layout; ||  VkImageLayout layout;
            0u                         // ||  VkImageAspectFlags aspectMask;
        );

    DE_ASSERT((typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo1)) ||
              (typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo2)));

    vector<SubpassDesc> subpassDescriptions;
    for (uint32_t subpassNdx = 0; subpassNdx < subpassCount; ++subpassNdx)
    {
        const uint32_t viewMask =
            (typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo2)) ? viewMasks[subpassNdx] : 0u;
        const SubpassDesc
            subpassDescription //  VkSubpassDescription                                        ||  VkSubpassDescription2KHR
            (
                //  ||  VkStructureType sType;
                nullptr,                      //   ||  const void* pNext;
                (VkSubpassDescriptionFlags)0, // VkSubpassDescriptionFlags flags; ||  VkSubpassDescriptionFlags flags;
                VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint; ||  VkPipelineBindPoint pipelineBindPoint;
                viewMask,                        //   ||  uint32_t viewMask;
                1u,                              // uint32_t inputAttachmentCount; ||  uint32_t inputAttachmentCount;
                &inputAttachmentReference, // const VkAttachmentReference* pInputAttachments; ||  const VkAttachmentReference2KHR* pInputAttachments;
                1u, // uint32_t colorAttachmentCount; ||  uint32_t colorAttachmentCount;
                &colorAttachmentReference, // const VkAttachmentReference* pColorAttachments; ||  const VkAttachmentReference2KHR* pColorAttachments;
                nullptr, // const VkAttachmentReference* pResolveAttachments; ||  const VkAttachmentReference2KHR* pResolveAttachments;
                &depthAttachmentReference, // const VkAttachmentReference* pDepthStencilAttachment; ||  const VkAttachmentReference2KHR* pDepthStencilAttachment;
                0u,     // uint32_t preserveAttachmentCount; ||  uint32_t preserveAttachmentCount;
                nullptr // const uint32_t* pPreserveAttachments; ||  const uint32_t* pPreserveAttachments;
            );
        subpassDescriptions.push_back(subpassDescription);
    }

    const VkRenderPassMultiviewCreateInfo renderPassMultiviewInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, //VkStructureType sType;
        nullptr,                                             //const void* pNext;
        subpassCount,                                        //uint32_t subpassCount;
        &viewMasks[0],                                       //const uint32_t* pViewMasks;
        0u,                                                  //uint32_t dependencyCount;
        nullptr,                                             //const int32_t* pViewOffsets;
        0u,                                                  //uint32_t correlationMaskCount;
        nullptr,                                             //const uint32_t* pCorrelationMasks;
    };
    const VkRenderPassMultiviewCreateInfo *renderPassMultiviewInfoPtr =
        typeid(RenderPassCreateInfo) == typeid(RenderPassCreateInfo1) ? &renderPassMultiviewInfo : nullptr;

    vector<SubpassDep> subpassDependencies;
    for (uint32_t subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
    {
        uint32_t viewMask = viewMasks[subpassNdx];

        // For every view written in this subpass, there should be a dependency
        // to the next subpass that writes to the same view.
        for (uint32_t dstSubpassNdx = subpassNdx + 1; dstSubpassNdx < subpassCount; ++dstSubpassNdx)
        {
            if (viewMask & viewMasks[dstSubpassNdx])
            {
                viewMask &= ~viewMasks[dstSubpassNdx];

                const SubpassDep
                    subpassDependency //  VkSubpassDependency                                            ||  VkSubpassDependency2KHR
                    (
                        //  || VkStructureType sType;
                        nullptr,                                       //   || const void* pNext;
                        subpassNdx,                                    //  uint32_t srcSubpass; || uint32_t srcSubpass;
                        dstSubpassNdx,                                 //  uint32_t dstSubpass; || uint32_t dstSubpass;
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
                        VK_DEPENDENCY_VIEW_LOCAL_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
                        0                             //    || int32_t viewOffset;
                    );
                subpassDependencies.push_back(subpassDependency);
            }
        }

        // If there are views left that are not written by any future subpasses,
        // there should be a external dependency.
        if (viewMask)
        {
            const SubpassDep
                subpassDependency //  VkSubpassDependency                                            ||  VkSubpassDependency2KHR
                (
                    //  || VkStructureType sType;
                    nullptr,                                       //   || const void* pNext;
                    subpassNdx,                                    //  uint32_t srcSubpass; || uint32_t srcSubpass;
                    VK_SUBPASS_EXTERNAL,                           //  uint32_t dstSubpass; || uint32_t dstSubpass;
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //  VkPipelineStageFlags srcStageMask; || VkPipelineStageFlags srcStageMask;
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //  VkPipelineStageFlags dstStageMask; || VkPipelineStageFlags dstStageMask;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //  VkAccessFlags srcAccessMask; || VkAccessFlags srcAccessMask;
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //  VkAccessFlags dstAccessMask; || VkAccessFlags dstAccessMask;
                    VK_DEPENDENCY_BY_REGION_BIT, //  VkDependencyFlags dependencyFlags; || VkDependencyFlags dependencyFlags;
                    0                            //    || int32_t viewOffset;
                );
            subpassDependencies.push_back(subpassDependency);
        }
    }

    const RenderPassCreateInfo
        renderPassInfo //  VkRenderPassCreateInfo                                        ||  VkRenderPassCreateInfo2KHR
        (
            //  VkStructureType sType; ||  VkStructureType sType;
            renderPassMultiviewInfoPtr, //  const void* pNext; ||  const void* pNext;
            (VkRenderPassCreateFlags)0, //  VkRenderPassCreateFlags flags; ||  VkRenderPassCreateFlags flags;
            2u,                         //  uint32_t attachmentCount; ||  uint32_t attachmentCount;
            &attachments
                [0], //  const VkAttachmentDescription* pAttachments; ||  const VkAttachmentDescription2KHR* pAttachments;
            subpassCount, //  uint32_t subpassCount; ||  uint32_t subpassCount;
            &subpassDescriptions
                [0],      //  const VkSubpassDescription* pSubpasses; ||  const VkSubpassDescription2KHR* pSubpasses;
            subpassCount, //  uint32_t dependencyCount; ||  uint32_t dependencyCount;
            &subpassDependencies
                [0], //  const VkSubpassDependency* pDependencies; ||  const VkSubpassDependency2KHR* pDependencies;
            0u,      //   ||  uint32_t correlatedViewMaskCount;
            nullptr  //  ||  const uint32_t* pCorrelatedViewMasks;
        );

    return renderPassInfo.createRenderPass(vk, device);
}

// Instantiate function for legacy renderpass structures
template Move<VkRenderPass> makeRenderPassWithAttachments<
    AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
    bool useAspects);

// Instantiate function for renderpass2 structures
template Move<VkRenderPass> makeRenderPassWithAttachments<
    AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat, const vector<uint32_t> &viewMasks,
    bool useAspects);

void beginSecondaryCommandBuffer(const DeviceInterface &vk, const VkCommandBuffer commandBuffer,
                                 const VkRenderPass renderPass, const uint32_t subpass, const VkFramebuffer framebuffer)
{
    const VkCommandBufferInheritanceInfo secCmdBufInheritInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, //VkStructureType sType;
        nullptr,                                           //const void* pNext;
        renderPass,                                        //VkRenderPass renderPass;
        subpass,                                           //uint32_t subpass;
        framebuffer,                                       //VkFramebuffer framebuffer;
        VK_FALSE,                                          //VkBool32 occlusionQueryEnable;
        (VkQueryControlFlags)0u,                           //VkQueryControlFlags queryFlags;
        (VkQueryPipelineStatisticFlags)0u,                 //VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    const VkCommandBufferBeginInfo info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,      // VkStructureType sType;
        nullptr,                                          // const void* pNext;
        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, // VkCommandBufferUsageFlags flags;
        &secCmdBufInheritInfo,                            // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
    };
    VK_CHECK(vk.beginCommandBuffer(commandBuffer, &info));
}

void imageBarrier(const DeviceInterface &vk, const VkCommandBuffer cmdBuffer, const VkImage image,
                  const VkImageSubresourceRange subresourceRange, const VkImageLayout oldLayout,
                  const VkImageLayout newLayout, const VkAccessFlags srcAccessMask, const VkAccessFlags dstAccessMask,
                  const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask)
{
    const VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        srcAccessMask,                          // VkAccessFlags srcAccessMask;
        dstAccessMask,                          // VkAccessFlags dstAccessMask;
        oldLayout,                              // VkImageLayout oldLayout;
        newLayout,                              // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        image,                                  // VkImage image;
        subresourceRange,                       // VkImageSubresourceRange subresourceRange;
    };

    vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 0u, nullptr, 0u, nullptr, 1u,
                          &barrier);
}

void memoryBarrier(const vk::DeviceInterface &vk, const vk::VkCommandBuffer cmdBuffer,
                   const vk::VkAccessFlags srcAccessMask, const vk::VkAccessFlags dstAccessMask,
                   const vk::VkPipelineStageFlags srcStageMask, const vk::VkPipelineStageFlags dstStageMask)
{
    VkMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                          // const void* pNext;
        srcAccessMask,                    // VkAccessFlags srcAccessMask;
        dstAccessMask,                    // VkAccessFlags dstAccessMask;
    };
    vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 1u, &barrier, 0u, nullptr, 0u,
                          nullptr);
}

} // namespace MultiView
} // namespace vkt
