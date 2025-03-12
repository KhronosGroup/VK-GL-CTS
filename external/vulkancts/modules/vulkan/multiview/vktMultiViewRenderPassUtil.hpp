#ifndef _VKTMULTIVIEWRENDERPASSUTIL_HPP
#define _VKTMULTIVIEWRENDERPASSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief RenderPass utils
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vkRef.hpp"
#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace MultiView
{
using namespace vk;

class AttachmentDescription1 : public vk::VkAttachmentDescription
{
public:
    AttachmentDescription1(const void *pNext, VkAttachmentDescriptionFlags flags, VkFormat format,
                           VkSampleCountFlagBits samples, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                           VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp,
                           VkImageLayout initialLayout, VkImageLayout finalLayout);
};

class AttachmentDescription2 : public vk::VkAttachmentDescription2
{
public:
    AttachmentDescription2(const void *pNext, VkAttachmentDescriptionFlags flags, VkFormat format,
                           VkSampleCountFlagBits samples, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,
                           VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp,
                           VkImageLayout initialLayout, VkImageLayout finalLayout);
};

class AttachmentReference1 : public vk::VkAttachmentReference
{
public:
    AttachmentReference1(const void *pNext, uint32_t attachment, VkImageLayout layout, VkImageAspectFlags aspectMask);
};

class AttachmentReference2 : public vk::VkAttachmentReference2
{
public:
    AttachmentReference2(const void *pNext, uint32_t attachment, VkImageLayout layout, VkImageAspectFlags aspectMask);
};

class SubpassDescription1 : public vk::VkSubpassDescription
{
public:
    SubpassDescription1(const void *pNext, VkSubpassDescriptionFlags flags, VkPipelineBindPoint pipelineBindPoint,
                        uint32_t viewMask, uint32_t inputAttachmentCount,
                        const VkAttachmentReference *pInputAttachments, uint32_t colorAttachmentCount,
                        const VkAttachmentReference *pColorAttachments,
                        const VkAttachmentReference *pResolveAttachments,
                        const VkAttachmentReference *pDepthStencilAttachment, uint32_t preserveAttachmentCount,
                        const uint32_t *pPreserveAttachments);
};

class SubpassDescription2 : public vk::VkSubpassDescription2
{
public:
    SubpassDescription2(const void *pNext, VkSubpassDescriptionFlags flags, VkPipelineBindPoint pipelineBindPoint,
                        uint32_t viewMask, uint32_t inputAttachmentCount,
                        const VkAttachmentReference2 *pInputAttachments, uint32_t colorAttachmentCount,
                        const VkAttachmentReference2 *pColorAttachments,
                        const VkAttachmentReference2 *pResolveAttachments,
                        const VkAttachmentReference2 *pDepthStencilAttachment, uint32_t preserveAttachmentCount,
                        const uint32_t *pPreserveAttachments);
};

class SubpassDependency1 : public vk::VkSubpassDependency
{
public:
    SubpassDependency1(const void *pNext, uint32_t srcSubpass, uint32_t dstSubpass, VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                       VkDependencyFlags dependencyFlags, int32_t viewOffset);
};

class SubpassDependency2 : public vk::VkSubpassDependency2
{
public:
    SubpassDependency2(const void *pNext, uint32_t srcSubpass, uint32_t dstSubpass, VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                       VkDependencyFlags dependencyFlags, int32_t viewOffset);
};

class RenderPassCreateInfo1 : public VkRenderPassCreateInfo
{
public:
    RenderPassCreateInfo1(const void *pNext, VkRenderPassCreateFlags flags, uint32_t attachmentCount,
                          const VkAttachmentDescription *pAttachments, uint32_t subpassCount,
                          const VkSubpassDescription *pSubpasses, uint32_t dependencyCount,
                          const VkSubpassDependency *pDependencies, uint32_t correlatedViewMaskCount,
                          const uint32_t *pCorrelatedViewMasks);

    Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice device) const;
};

class RenderPassCreateInfo2 : public VkRenderPassCreateInfo2
{
public:
    RenderPassCreateInfo2(const void *pNext, VkRenderPassCreateFlags flags, uint32_t attachmentCount,
                          const VkAttachmentDescription2 *pAttachments, uint32_t subpassCount,
                          const VkSubpassDescription2 *pSubpasses, uint32_t dependencyCount,
                          const VkSubpassDependency2 *pDependencies, uint32_t correlatedViewMaskCount,
                          const uint32_t *pCorrelatedViewMasks);

    Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice device) const;
};

class SubpassBeginInfo1
{
public:
    SubpassBeginInfo1(const void *pNext, VkSubpassContents contents);

    VkSubpassContents contents;
};

class SubpassBeginInfo2 : public VkSubpassBeginInfo
{
public:
    SubpassBeginInfo2(const void *pNext, VkSubpassContents contents);
};

class SubpassEndInfo1
{
public:
    SubpassEndInfo1(const void *pNext);
};

class SubpassEndInfo2 : public VkSubpassEndInfo
{
public:
    SubpassEndInfo2(const void *pNext);
};

class RenderpassSubpass1
{
public:
    typedef SubpassBeginInfo1 SubpassBeginInfo;
    typedef SubpassEndInfo1 SubpassEndInfo;

    static void cmdBeginRenderPass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                   const VkRenderPassBeginInfo *pRenderPassBegin,
                                   const SubpassBeginInfo *pSubpassBeginInfo);

    static void cmdNextSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                               const SubpassBeginInfo *pSubpassBeginInfo, const SubpassEndInfo *pSubpassEndInfo);

    static void cmdEndRenderPass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                 const SubpassEndInfo *pSubpassEndInfo);
};

class RenderpassSubpass2
{
public:
    typedef SubpassBeginInfo2 SubpassBeginInfo;
    typedef SubpassEndInfo2 SubpassEndInfo;

    static void cmdBeginRenderPass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                   const VkRenderPassBeginInfo *pRenderPassBegin,
                                   const SubpassBeginInfo *pSubpassBeginInfo);

    static void cmdNextSubpass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                               const SubpassBeginInfo *pSubpassBeginInfo, const SubpassEndInfo *pSubpassEndInfo);

    static void cmdEndRenderPass(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                 const SubpassEndInfo *pSubpassEndInfo);
};

} // namespace MultiView

} // namespace vkt

#endif // _VKTMULTIVIEWRENDERPASSUTIL_HPP
