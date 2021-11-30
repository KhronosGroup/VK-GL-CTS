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
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

namespace vk
{
Move<VkPipeline> makeComputePipeline (const DeviceInterface&					vk,
									  const VkDevice							device,
									  const VkPipelineLayout					pipelineLayout,
									  const VkPipelineCreateFlags				pipelineFlags,
									  const VkShaderModule						shaderModule,
									  const VkPipelineShaderStageCreateFlags	shaderFlags,
									  const VkSpecializationInfo*				specializationInfo);

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&							vk,
									   const VkDevice									device,
									   const VkPipelineLayout							pipelineLayout,
									   const VkShaderModule								vertexShaderModule,
									   const VkShaderModule								tessellationControlShaderModule,
									   const VkShaderModule								tessellationEvalShaderModule,
									   const VkShaderModule								geometryShaderModule,
									   const VkShaderModule								fragmentShaderModule,
									   const VkRenderPass								renderPass,
									   const std::vector<VkViewport>&					viewports,
									   const std::vector<VkRect2D>&						scissors,
									   const VkPrimitiveTopology						topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
									   const deUint32									subpass = 0u,
									   const deUint32									patchControlPoints = 0u,
									   const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo = DE_NULL,
									   const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo = DE_NULL,
									   const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo = DE_NULL,
									   const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo = DE_NULL,
									   const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo = DE_NULL,
									   const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo = DE_NULL,
									   const void*										pNext = DE_NULL);

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&							vk,
									   const VkDevice									device,
									   const VkPipelineLayout							pipelineLayout,
									   const VkShaderModule								vertexShaderModule,
									   const VkShaderModule								tessellationControlShaderModule,
									   const VkShaderModule								tessellationEvalShaderModule,
									   const VkShaderModule								geometryShaderModule,
									   const VkShaderModule								fragmentShaderModule,
									   const VkRenderPass								renderPass,
									   const deUint32									subpass = 0u,
									   const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo = DE_NULL,
									   const VkPipelineInputAssemblyStateCreateInfo*	inputAssemblyStateCreateInfo = DE_NULL,
									   const VkPipelineTessellationStateCreateInfo*		tessStateCreateInfo = DE_NULL,
									   const VkPipelineViewportStateCreateInfo*			viewportStateCreateInfo = DE_NULL,
									   const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo = DE_NULL,
									   const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo = DE_NULL,
									   const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo = DE_NULL,
									   const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo = DE_NULL,
									   const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo = DE_NULL,
									   const void*										pNext = DE_NULL);

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&							vk,
									   const VkDevice									device,
									   const VkPipelineLayout							pipelineLayout,
									   const VkShaderModule								taskShaderModule,
									   const VkShaderModule								meshShaderModule,
									   const VkShaderModule								fragmentShaderModule,
									   const VkRenderPass								renderPass,
									   const std::vector<VkViewport>&					viewports,
									   const std::vector<VkRect2D>&						scissors,
									   const deUint32									subpass = 0u,
									   const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo = nullptr,
									   const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo = nullptr,
									   const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo = nullptr,
									   const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo = nullptr,
									   const VkPipelineDynamicStateCreateInfo*			dynamicStateCreateInfo = nullptr);

Move<VkRenderPass> makeRenderPass (const DeviceInterface&				vk,
								   const VkDevice						device,
								   const VkFormat						colorFormat					= VK_FORMAT_UNDEFINED,
								   const VkFormat						depthStencilFormat			= VK_FORMAT_UNDEFINED,
								   const VkAttachmentLoadOp				loadOperation				= VK_ATTACHMENT_LOAD_OP_CLEAR,
								   const VkImageLayout					finalLayoutColor			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
								   const VkImageLayout					finalLayoutDepthStencil		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								   const VkImageLayout					subpassLayoutColor			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
								   const VkImageLayout					subpassLayoutDepthStencil	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								   const VkAllocationCallbacks* const	allocationCallbacks			= DE_NULL);

Move<VkImageView> makeImageView (const DeviceInterface&					vk,
								 const VkDevice							vkDevice,
								 const VkImage							image,
								 const VkImageViewType					imageViewType,
								 const VkFormat							format,
								 const VkImageSubresourceRange			subresourceRange,
								 const vk::VkImageViewUsageCreateInfo*	imageUsageCreateInfo = DE_NULL);

Move<VkBufferView> makeBufferView (const DeviceInterface&	vk,
								   const VkDevice			vkDevice,
								   const VkBuffer			buffer,
								   const VkFormat			format,
								   const VkDeviceSize		offset,
								   const VkDeviceSize		size);

Move<VkDescriptorSet> makeDescriptorSet (const DeviceInterface&			vk,
										 const VkDevice					device,
										 const VkDescriptorPool			descriptorPool,
										 const VkDescriptorSetLayout	setLayout,
										 const void*					pNext = DE_NULL);

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize			size,
										 const VkBufferUsageFlags	usage);

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize				size,
										 const VkBufferUsageFlags		usage,
										 const std::vector<deUint32>&	queueFamilyIndices);

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkDescriptorSetLayout	descriptorSetLayout = DE_NULL);

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&								vk,
										   const VkDevice										device,
										   const std::vector<vk::Move<VkDescriptorSetLayout>>	&descriptorSetLayouts);

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const deUint32				setLayoutCount,
										   const VkDescriptorSetLayout*	descriptorSetLayout);

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const deUint32				setLayoutCount,
										   const VkDescriptorSetLayout*	descriptorSetLayout,
										   const deUint32               pushConstantRangeCount,
										   const VkPushConstantRange*   pPushConstantRanges);

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&	vk,
									 const VkDevice			device,
									 const VkRenderPass		renderPass,
									 const VkImageView		colorAttachment,
									 const deUint32			width,
									 const deUint32			height,
									 const deUint32			layers = 1u);

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&	vk,
									 const VkDevice			device,
									 const VkRenderPass		renderPass,
									 const deUint32			attachmentCount,
									 const VkImageView*		attachmentsArray,
									 const deUint32			width,
									 const deUint32			height,
									 const deUint32			layers = 1u);

Move<VkCommandPool> makeCommandPool (const DeviceInterface&	vk,
									 const VkDevice			device,
									 const deUint32			queueFamilyIndex);

inline Move<VkBuffer> makeBuffer (const DeviceInterface&	vk,
								  const VkDevice			device,
								  const VkDeviceSize		bufferSize,
								  const VkBufferUsageFlags	usage)
{
	const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, usage);
	return createBuffer(vk, device, &bufferCreateInfo);
}

inline Move<VkBuffer> makeBuffer (const vk::DeviceInterface&	vk,
								  const vk::VkDevice			device,
								  const vk::VkBufferCreateInfo&	createInfo)
{
	return createBuffer(vk, device, &createInfo);
}

inline Move<VkImage> makeImage (const DeviceInterface&		vk,
								const VkDevice				device,
								const VkImageCreateInfo&	createInfo)
{
	return createImage(vk, device, &createInfo);
}

VkBufferImageCopy makeBufferImageCopy (const VkExtent3D					extent,
									   const VkImageSubresourceLayers	subresourceLayers);

} // vk

#endif // _VKOBJUTIL_HPP
