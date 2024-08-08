#ifndef _VKOBJUTIL_HPP
#define _VKOBJUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 * \brief Utilities for creating commonly used Vulkan objects
 *//*--------------------------------------------------------------------*/

#include <vector>
#include <memory>
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

namespace vk
{

struct CommandPoolWithBuffer
{
    Move<VkCommandPool> cmdPool;
    Move<VkCommandBuffer> cmdBuffer;

    CommandPoolWithBuffer(const DeviceInterface &vkd, const VkDevice device, const uint32_t queueFamilyIndex);
};

Move<VkPipeline> makeComputePipeline(const DeviceInterface &vk, const VkDevice device,
                                     const VkPipelineLayout pipelineLayout, const VkPipelineCreateFlags pipelineFlags,
                                     const void *pipelinePNext, const VkShaderModule shaderModule,
                                     const VkPipelineShaderStageCreateFlags shaderFlags,
                                     const VkSpecializationInfo *specializationInfo = nullptr,
                                     const VkPipelineCache pipelineCache            = VK_NULL_HANDLE,
                                     const uint32_t subgroupSize                    = 0);

Move<VkPipeline> makeComputePipeline(const DeviceInterface &vk, VkDevice device, VkPipelineLayout pipelineLayout,
                                     VkShaderModule shaderModule);

Move<VkPipeline> makeGraphicsPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const VkShaderModule vertexShaderModule, const VkShaderModule tessellationControlShaderModule,
    const VkShaderModule tessellationEvalShaderModule, const VkShaderModule geometryShaderModule,
    const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass, const std::vector<VkViewport> &viewports,
    const std::vector<VkRect2D> &scissors, const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    const uint32_t subpass = 0u, const uint32_t patchControlPoints = 0u,
    const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo     = nullptr,
    const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo = nullptr,
    const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo     = nullptr,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo   = nullptr,
    const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo       = nullptr,
    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo = nullptr, const void *pNext = nullptr,
    const VkPipelineCreateFlags pipelineCreateFlags = 0u);

Move<VkPipeline> makeGraphicsPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const VkShaderModule vertexShaderModule, const VkShaderModule tessellationControlShaderModule,
    const VkShaderModule tessellationEvalShaderModule, const VkShaderModule geometryShaderModule,
    const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass, const uint32_t subpass = 0u,
    const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo     = nullptr,
    const VkPipelineInputAssemblyStateCreateInfo *inputAssemblyStateCreateInfo = nullptr,
    const VkPipelineTessellationStateCreateInfo *tessStateCreateInfo           = nullptr,
    const VkPipelineViewportStateCreateInfo *viewportStateCreateInfo           = nullptr,
    const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo = nullptr,
    const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo     = nullptr,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo   = nullptr,
    const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo       = nullptr,
    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo = nullptr, const void *pNext = nullptr,
    const VkPipelineCreateFlags pipelineCreateFlags = 0u);

#ifndef CTS_USES_VULKANSC
Move<VkPipeline> makeGraphicsPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipelineLayout pipelineLayout,
    const VkShaderModule taskShaderModule, const VkShaderModule meshShaderModule,
    const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass, const std::vector<VkViewport> &viewports,
    const std::vector<VkRect2D> &scissors, const uint32_t subpass = 0u,
    const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo = nullptr,
    const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo     = nullptr,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo   = nullptr,
    const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo       = nullptr,
    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo             = nullptr,
    const VkPipelineCreateFlags pipelineCreateFlags = 0u, const void *pNext = nullptr);
#endif // CTS_USES_VULKANSC

Move<VkPipeline> makeGraphicsPipeline(
    const DeviceInterface &vk, const VkDevice device, const VkPipeline basePipelineHandle,
    const VkPipelineLayout pipelineLayout, const VkPipelineCreateFlags pipelineCreateFlags,
    const std::vector<VkPipelineShaderStageCreateInfo> &shaderCreateInfos, const VkRenderPass renderPass,
    const std::vector<VkViewport> &viewports, const std::vector<VkRect2D> &scissors, const uint32_t subpass = 0u,
    const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo = nullptr,
    const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo     = nullptr,
    const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo   = nullptr,
    const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo       = nullptr,
    const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo = nullptr, const void *pNext = nullptr);

Move<VkRenderPass> makeRenderPass(
    const DeviceInterface &vk, const VkDevice device, const VkFormat colorFormat = VK_FORMAT_UNDEFINED,
    const VkFormat depthStencilFormat                      = VK_FORMAT_UNDEFINED,
    const VkAttachmentLoadOp loadOperation                 = VK_ATTACHMENT_LOAD_OP_CLEAR,
    const VkImageLayout finalLayoutColor                   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    const VkImageLayout finalLayoutDepthStencil            = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    const VkImageLayout subpassLayoutColor                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    const VkImageLayout subpassLayoutDepthStencil          = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    const VkAllocationCallbacks *const allocationCallbacks = nullptr, const void *pNext = nullptr);

Move<VkImageView> makeImageView(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                const VkImageViewType imageViewType, const VkFormat format,
                                const VkImageSubresourceRange subresourceRange,
                                const vk::VkImageViewUsageCreateInfo *imageUsageCreateInfo = nullptr);

Move<VkBufferView> makeBufferView(const DeviceInterface &vk, const VkDevice vkDevice, const VkBuffer buffer,
                                  const VkFormat format, const VkDeviceSize offset, const VkDeviceSize size);

Move<VkDescriptorSet> makeDescriptorSet(const DeviceInterface &vk, const VkDevice device,
                                        const VkDescriptorPool descriptorPool, const VkDescriptorSetLayout setLayout,
                                        const void *pNext = nullptr);

VkBufferCreateInfo makeBufferCreateInfo(const VkDeviceSize size, const VkBufferUsageFlags usage);

VkBufferCreateInfo makeBufferCreateInfo(const VkDeviceSize size, const VkBufferUsageFlags usage,
                                        const std::vector<uint32_t> &queueFamilyIndices,
                                        const VkBufferCreateFlags createFlags = 0, const void *pNext = nullptr);

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE,
                                          const VkPushConstantRange *pushConstantRange    = nullptr);

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts);

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const std::vector<vk::Move<VkDescriptorSetLayout>> &descriptorSetLayouts);

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const uint32_t setLayoutCount,
                                          const VkDescriptorSetLayout *descriptorSetLayout,
                                          const VkPipelineLayoutCreateFlags flags = 0u);

Move<VkPipelineLayout> makePipelineLayout(const DeviceInterface &vk, const VkDevice device,
                                          const uint32_t setLayoutCount,
                                          const VkDescriptorSetLayout *descriptorSetLayout,
                                          const uint32_t pushConstantRangeCount,
                                          const VkPushConstantRange *pPushConstantRanges,
                                          const VkPipelineLayoutCreateFlags flags = 0u);

Move<VkFramebuffer> makeFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkRenderPass renderPass,
                                    const VkImageView colorAttachment, const uint32_t width, const uint32_t height,
                                    const uint32_t layers = 1u);

Move<VkFramebuffer> makeFramebuffer(const DeviceInterface &vk, const VkDevice device, const VkRenderPass renderPass,
                                    const uint32_t attachmentCount, const VkImageView *attachmentsArray,
                                    const uint32_t width, const uint32_t height, const uint32_t layers = 1u);

Move<VkCommandPool> makeCommandPool(const DeviceInterface &vk, const VkDevice device, const uint32_t queueFamilyIndex);

inline Move<VkBuffer> makeBuffer(const DeviceInterface &vk, const VkDevice device, const VkDeviceSize bufferSize,
                                 const VkBufferUsageFlags usage)
{
    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, usage);
    return createBuffer(vk, device, &bufferCreateInfo);
}

inline Move<VkBuffer> makeBuffer(const vk::DeviceInterface &vk, const vk::VkDevice device,
                                 const vk::VkBufferCreateInfo &createInfo)
{
    return createBuffer(vk, device, &createInfo);
}

inline Move<VkImage> makeImage(const DeviceInterface &vk, const VkDevice device, const VkImageCreateInfo &createInfo)
{
    return createImage(vk, device, &createInfo);
}

VkBufferImageCopy makeBufferImageCopy(const VkExtent3D extent, const VkImageSubresourceLayers subresourceLayers);

} // namespace vk

#endif // _VKOBJUTIL_HPP
