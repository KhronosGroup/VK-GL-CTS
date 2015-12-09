/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Vulkan object reference holder utilities.
 *//*--------------------------------------------------------------------*/

#include "vkRefUtil.hpp"

namespace vk
{

#include "vkRefUtilImpl.inl"

Move<VkPipeline> createGraphicsPipeline (const DeviceInterface&					vk,
										 VkDevice								device,
										 VkPipelineCache						pipelineCache,
										 const VkGraphicsPipelineCreateInfo*	pCreateInfo,
										 const VkAllocationCallbacks*			pAllocator)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createGraphicsPipelines(device, pipelineCache, 1u, pCreateInfo, pAllocator, &object));
	return Move<VkPipeline>(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, pAllocator));
}

Move<VkPipeline> createComputePipeline (const DeviceInterface&				vk,
										VkDevice							device,
										VkPipelineCache						pipelineCache,
										const VkComputePipelineCreateInfo*	pCreateInfo,
										const VkAllocationCallbacks*		pAllocator)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createComputePipelines(device, pipelineCache, 1u, pCreateInfo, pAllocator, &object));
	return Move<VkPipeline>(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, pAllocator));
}

Move<VkCommandBuffer> allocateCommandBuffer (const DeviceInterface& vk, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo)
{
	VkCommandBuffer object = 0;
	DE_ASSERT(pAllocateInfo->bufferCount == 1u);
	VK_CHECK(vk.allocateCommandBuffers(device, pAllocateInfo, &object));
	return Move<VkCommandBuffer>(check<VkCommandBuffer>(object), Deleter<VkCommandBuffer>(vk, device, pAllocateInfo->commandPool));
}

Move<VkDescriptorSet> allocateDescriptorSet (const DeviceInterface& vk, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo)
{
	VkDescriptorSet object = 0;
	DE_ASSERT(pAllocateInfo->setLayoutCount == 1u);
	VK_CHECK(vk.allocateDescriptorSets(device, pAllocateInfo, &object));
	return Move<VkDescriptorSet>(check<VkDescriptorSet>(object), Deleter<VkDescriptorSet>(vk, device, pAllocateInfo->descriptorPool));
}

} // vk
