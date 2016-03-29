#ifndef _VKREFUTIL_HPP
#define _VKREFUTIL_HPP
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
 * \brief Vulkan object reference holder utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"

namespace vk
{

#include "vkRefUtil.inl"

Move<VkPipeline>		createGraphicsPipeline	(const DeviceInterface&					vk,
												 VkDevice								device,
												 VkPipelineCache						pipelineCache,
												 const VkGraphicsPipelineCreateInfo*	pCreateInfo,
												 const VkAllocationCallbacks*			pAllocator = DE_NULL);
Move<VkPipeline>		createComputePipeline	(const DeviceInterface&					vk,
												 VkDevice								device,
												 VkPipelineCache						pipelineCache,
												 const VkComputePipelineCreateInfo*		pCreateInfo,
												 const VkAllocationCallbacks*			pAllocator = DE_NULL);
Move<VkCommandBuffer>	allocateCommandBuffer	(const DeviceInterface& vk, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo);
Move<VkDescriptorSet>	allocateDescriptorSet	(const DeviceInterface& vk, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo);

} // vk

#endif // _VKREFUTIL_HPP
