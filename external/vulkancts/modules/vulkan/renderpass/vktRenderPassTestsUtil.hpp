#ifndef _VKTRENDERPASSTESTSUTIL_HPP
#define _VKTRENDERPASSTESTSUTIL_HPP
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
 * \brief RenderPass test utils
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vkRef.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktRenderPassGroupParams.hpp"

#include <vector>

namespace vkt
{
namespace renderpass
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

// For internal to RP/RP2 conversions

class AttachmentReference
{
public:
    AttachmentReference(uint32_t attachment, VkImageLayout layout,
                        VkImageAspectFlags aspectMask = static_cast<VkImageAspectFlags>(0u));

    uint32_t getAttachment(void) const;
    VkImageLayout getImageLayout(void) const;
    VkImageAspectFlags getAspectMask(void) const;
    void setImageLayout(VkImageLayout layout);

private:
    uint32_t m_attachment;
    VkImageLayout m_layout;
    VkImageAspectFlags m_aspectMask;
};

class Subpass
{
public:
    Subpass(VkPipelineBindPoint pipelineBindPoint, VkSubpassDescriptionFlags flags,
            const std::vector<AttachmentReference> &inputAttachments,
            const std::vector<AttachmentReference> &colorAttachments,
            const std::vector<AttachmentReference> &resolveAttachments, AttachmentReference depthStencilAttachment,
            const std::vector<uint32_t> &preserveAttachments, bool omitBlendState = false);

    VkPipelineBindPoint getPipelineBindPoint(void) const;
    VkSubpassDescriptionFlags getFlags(void) const;
    const std::vector<AttachmentReference> &getInputAttachments(void) const;
    const std::vector<AttachmentReference> &getColorAttachments(void) const;
    const std::vector<AttachmentReference> &getResolveAttachments(void) const;
    const AttachmentReference &getDepthStencilAttachment(void) const;
    const std::vector<uint32_t> &getPreserveAttachments(void) const;
    bool getOmitBlendState(void) const;

private:
    VkPipelineBindPoint m_pipelineBindPoint;
    VkSubpassDescriptionFlags m_flags;

    std::vector<AttachmentReference> m_inputAttachments;
    std::vector<AttachmentReference> m_colorAttachments;
    std::vector<AttachmentReference> m_resolveAttachments;
    AttachmentReference m_depthStencilAttachment;

    std::vector<uint32_t> m_preserveAttachments;
    bool m_omitBlendState;
};

class SubpassDependency
{
public:
    SubpassDependency(uint32_t srcPass, uint32_t dstPass,

                      VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,

                      VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,

                      VkDependencyFlags flags);

    uint32_t getSrcPass(void) const;
    uint32_t getDstPass(void) const;

    VkPipelineStageFlags getSrcStageMask(void) const;
    VkPipelineStageFlags getDstStageMask(void) const;

    VkAccessFlags getSrcAccessMask(void) const;
    VkAccessFlags getDstAccessMask(void) const;

    VkDependencyFlags getFlags(void) const;

private:
    uint32_t m_srcPass;
    uint32_t m_dstPass;

    VkPipelineStageFlags m_srcStageMask;
    VkPipelineStageFlags m_dstStageMask;

    VkAccessFlags m_srcAccessMask;
    VkAccessFlags m_dstAccessMask;
    VkDependencyFlags m_flags;
};

class Attachment
{
public:
    Attachment(VkFormat format, VkSampleCountFlagBits samples,

               VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp,

               VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp,

               VkImageLayout initialLayout, VkImageLayout finalLayout);

    VkFormat getFormat(void) const;
    VkSampleCountFlagBits getSamples(void) const;

    VkAttachmentLoadOp getLoadOp(void) const;
    VkAttachmentStoreOp getStoreOp(void) const;

    VkAttachmentLoadOp getStencilLoadOp(void) const;
    VkAttachmentStoreOp getStencilStoreOp(void) const;

    VkImageLayout getInitialLayout(void) const;
    VkImageLayout getFinalLayout(void) const;

private:
    VkFormat m_format;
    VkSampleCountFlagBits m_samples;

    VkAttachmentLoadOp m_loadOp;
    VkAttachmentStoreOp m_storeOp;

    VkAttachmentLoadOp m_stencilLoadOp;
    VkAttachmentStoreOp m_stencilStoreOp;

    VkImageLayout m_initialLayout;
    VkImageLayout m_finalLayout;
};

class RenderPass
{
public:
    RenderPass(const std::vector<Attachment> &attachments, const std::vector<Subpass> &subpasses,
               const std::vector<SubpassDependency> &dependencies,
               const std::vector<VkInputAttachmentAspectReference> inputAspects =
                   std::vector<VkInputAttachmentAspectReference>());

    const std::vector<Attachment> &getAttachments(void) const;
    const std::vector<Subpass> &getSubpasses(void) const;
    const std::vector<SubpassDependency> &getDependencies(void) const;
    const std::vector<VkInputAttachmentAspectReference> &getInputAspects(void) const;

private:
    std::vector<Attachment> m_attachments;
    std::vector<Subpass> m_subpasses;
    std::vector<SubpassDependency> m_dependencies;
    std::vector<VkInputAttachmentAspectReference> m_inputAspects;
};

Move<VkRenderPass> createRenderPass(const DeviceInterface &vk, VkDevice device, const RenderPass &renderPassInfo,
                                    RenderingType renderingType,
                                    SynchronizationType synchronizationType = SYNCHRONIZATION_TYPE_LEGACY);

} // namespace renderpass

} // namespace vkt

#endif // _VKTRENDERPASSTESTSUTIL_HPP
