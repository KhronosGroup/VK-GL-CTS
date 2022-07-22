#ifndef _VKSAFETYCRITICALUTIL_HPP
#define _VKSAFETYCRITICALUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Vulkan SC utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include <map>
#include <vector>
#include <functional>

#ifdef CTS_USES_VULKANSC

namespace vk
{

VkDeviceObjectReservationCreateInfo resetDeviceObjectReservationCreateInfo	();
VkPipelineOfflineCreateInfo			resetPipelineOfflineCreateInfo			();
VkPhysicalDeviceVulkanSC10Features	createDefaultSC10Features				();
void								applyPipelineIdentifier					(VkPipelineOfflineCreateInfo&					pipelineIdentifier,
																			 const std::string&								value);

VkGraphicsPipelineCreateInfo		prepareSimpleGraphicsPipelineCI			(VkPipelineVertexInputStateCreateInfo&			vertexInputStateCreateInfo,
																			 std::vector<VkPipelineShaderStageCreateInfo>&	shaderStageCreateInfos,
																			 VkPipelineInputAssemblyStateCreateInfo&		inputAssemblyStateCreateInfo,
																			 VkPipelineViewportStateCreateInfo&				viewPortStateCreateInfo,
																			 VkPipelineRasterizationStateCreateInfo&		rasterizationStateCreateInfo,
																			 VkPipelineMultisampleStateCreateInfo&			multisampleStateCreateInfo,
																			 VkPipelineColorBlendAttachmentState&			colorBlendAttachmentState,
																			 VkPipelineColorBlendStateCreateInfo&			colorBlendStateCreateInfo,
																			 VkPipelineDynamicStateCreateInfo&				dynamicStateCreateInfo,
																			 std::vector<VkDynamicState>&					dynamicStates,
																			 VkPipelineLayout								pipelineLayout,
																			 VkRenderPass									renderPass);
VkComputePipelineCreateInfo			prepareSimpleComputePipelineCI			(const VkPipelineShaderStageCreateInfo&			shaderStageCreateInfo,
																			 VkPipelineLayout								pipelineLayout);
VkRenderPassCreateInfo				prepareSimpleRenderPassCI				(VkFormat										format,
																			 VkAttachmentDescription&						attachmentDescription,
																			 VkAttachmentReference&							attachmentReference,
																			 VkSubpassDescription&							subpassDescription);
VkFormat							getRenderTargetFormat					(const InstanceInterface&						vk,
																			 const VkPhysicalDevice&						device);

std::size_t							calculateGraphicsPipelineHash			(const VkGraphicsPipelineCreateInfo&		gpCI,
																			 const std::map<deUint64,std::size_t>&		objectHashes);
std::size_t							calculateComputePipelineHash			(const VkComputePipelineCreateInfo&			cpCI,
																			 const std::map<deUint64,std::size_t>&		objectHashes);
std::size_t							calculateSamplerYcbcrConversionHash		(const VkSamplerYcbcrConversionCreateInfo&	scCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);
std::size_t							calculateSamplerHash					(const VkSamplerCreateInfo&					sCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);
std::size_t							calculateDescriptorSetLayoutHash		(const VkDescriptorSetLayoutCreateInfo&		sCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);
std::size_t							calculatePipelineLayoutHash				(const VkPipelineLayoutCreateInfo&			pCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);
std::size_t							calculateShaderModuleHash				(const VkShaderModuleCreateInfo&			sCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);
std::size_t							calculateRenderPassHash					(const VkRenderPassCreateInfo&				pCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);
std::size_t							calculateRenderPass2Hash				(const VkRenderPassCreateInfo2&				pCI,
																			 const std::map<deUint64, std::size_t>&		objectHashes);

template <typename T, typename... Rest>
inline void hash_combine(std::size_t &seed, T const &v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t &seed, T const &v, Rest &&... rest)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	hash_combine(seed, rest...);
}

} // vk

#endif // CTS_USES_VULKANSC

#endif // _VKSAFETYCRITICALUTIL_HPP
