#ifndef _VKTMULTIVIEWRENDERUTIL_HPP
#define _VKTMULTIVIEWRENDERUTIL_HPP
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

#include "tcuDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuTexture.hpp"

namespace vkt
{
namespace MultiView
{

vk::VkImageAspectFlags				getAspectFlags					(tcu::TextureFormat format);
vk::VkFormat						getStencilBufferFormat			(const vk::VkFormat depthStencilImageFormat);
vk::VkFormat						getDepthBufferFormat			(const vk::VkFormat depthStencilImageFormat);
vk::VkImageCreateInfo				makeImageCreateInfo				(const vk::VkImageType imageType, const vk::VkExtent3D& extent, const vk::VkFormat format, const vk::VkImageUsageFlags usage, const vk::VkSampleCountFlagBits samples);
vk::Move<vk::VkDescriptorSetLayout>	makeDescriptorSetLayout			(const vk::DeviceInterface& vk, const vk::VkDevice device);

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
vk::Move<vk::VkRenderPass>			makeRenderPass					(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkFormat colorFormat, const std::vector<deUint32>& viewMasks, const vk::VkSampleCountFlagBits samples, const vk::VkAttachmentLoadOp colorLoadOp, const vk::VkFormat dsFormat);

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
vk::Move<vk::VkRenderPass>			makeRenderPassWithAttachments	(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkFormat colorFormat, const std::vector<deUint32>& viewMasks, bool useAspects);

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
vk::Move<vk::VkRenderPass>			makeRenderPassWithDepth			(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkFormat colorFormat, const std::vector<deUint32>& viewMasks, const vk::VkFormat dsFormat);

void								beginSecondaryCommandBuffer		(const vk::DeviceInterface& vk, const vk::VkCommandBuffer commandBuffer, const vk::VkRenderPass renderPass, const deUint32 subpass, const vk::VkFramebuffer framebuffer);
void								imageBarrier					(const vk::DeviceInterface& vk, const vk::VkCommandBuffer cmdBuffer, const vk::VkImage image, const vk::VkImageSubresourceRange subresourceRange, const vk::VkImageLayout oldLayout, const vk::VkImageLayout newLayout, const vk::VkAccessFlags srcAccessMask, const vk::VkAccessFlags dstAccessMask, const vk::VkPipelineStageFlags srcStageMask = vk::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, const vk::VkPipelineStageFlags dstStageMask = vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

} // MultiView
} // vkt

#endif // _VKTMULTIVIEWRENDERUTIL_HPP
