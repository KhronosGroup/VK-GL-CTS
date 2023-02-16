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

#ifndef CTS_USES_VULKANSC

Move<VkPipeline> createRayTracingPipelineNV (const DeviceInterface&						vk,
											 VkDevice									device,
											 VkPipelineCache							pipelineCache,
											 const VkRayTracingPipelineCreateInfoNV*	pCreateInfo,
											 const VkAllocationCallbacks*				pAllocator)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createRayTracingPipelinesNV(device, pipelineCache, 1u, pCreateInfo, pAllocator, &object));
	return Move<VkPipeline>(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, pAllocator));
}

Move<VkPipeline> createRayTracingPipelineKHR (const DeviceInterface&					vk,
											  VkDevice									device,
											  VkDeferredOperationKHR					deferredOperation,
											  VkPipelineCache							pipelineCache,
											  const VkRayTracingPipelineCreateInfoKHR*	pCreateInfo,
											  const VkAllocationCallbacks*				pAllocator)
{
	VkPipeline object = 0;
	VK_CHECK(vk.createRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, 1u, pCreateInfo, pAllocator, &object));
	return Move<VkPipeline>(check<VkPipeline>(object), Deleter<VkPipeline>(vk, device, pAllocator));
}

#endif // CTS_USES_VULKANSC

Move<VkCommandBuffer> allocateCommandBuffer (const DeviceInterface& vk, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo)
{
	VkCommandBuffer object = 0;
	DE_ASSERT(pAllocateInfo->commandBufferCount == 1u);
	VK_CHECK(vk.allocateCommandBuffers(device, pAllocateInfo, &object));
	return Move<VkCommandBuffer>(check<VkCommandBuffer>(object), Deleter<VkCommandBuffer>(vk, device, pAllocateInfo->commandPool));
}

void allocateCommandBuffers (const DeviceInterface& vk, VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, Move<VkCommandBuffer> *pCommandBuffers)
{
	VkCommandBufferAllocateInfo allocateInfoCopy = *pAllocateInfo;
	allocateInfoCopy.commandBufferCount = 1;
	for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
		VkCommandBuffer object = 0;
		VK_CHECK(vk.allocateCommandBuffers(device, &allocateInfoCopy, &object));
		pCommandBuffers[i] = Move<VkCommandBuffer>(check<VkCommandBuffer>(object), Deleter<VkCommandBuffer>(vk, device, pAllocateInfo->commandPool));
	}
}

Move<VkDescriptorSet> allocateDescriptorSet (const DeviceInterface& vk, VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo)
{
	VkDescriptorSet object = 0;
	DE_ASSERT(pAllocateInfo->descriptorSetCount == 1u);
	VK_CHECK(vk.allocateDescriptorSets(device, pAllocateInfo, &object));
	return Move<VkDescriptorSet>(check<VkDescriptorSet>(object), Deleter<VkDescriptorSet>(vk, device, pAllocateInfo->descriptorPool));
}

Move<VkSemaphore> createSemaphore (const DeviceInterface&		vk,
								   VkDevice						device,
								   VkSemaphoreCreateFlags		flags,
								   const VkAllocationCallbacks*	pAllocator)
{
	const VkSemaphoreCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		DE_NULL,

		flags
	};

	return createSemaphore(vk, device, &createInfo, pAllocator);
}

Move<VkSemaphore> createSemaphoreType (const DeviceInterface&		vk,
									   VkDevice						device,
									   VkSemaphoreType				type,
									   VkSemaphoreCreateFlags		flags,
									   const deUint64				initialValue,
									   const VkAllocationCallbacks*	pAllocator)
{
	const VkSemaphoreTypeCreateInfo	createTypeInfo =
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		DE_NULL,

		type,
		initialValue,
	};
	const VkSemaphoreCreateInfo		createInfo =
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		&createTypeInfo,

		flags
	};

	return createSemaphore(vk, device, &createInfo, pAllocator);
}

Move<VkFence> createFence (const DeviceInterface&		vk,
						   VkDevice						device,
						   VkFenceCreateFlags			flags,
						   const VkAllocationCallbacks*	pAllocator)
{
	const VkFenceCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,

		flags
	};

	return createFence(vk, device, &createInfo, pAllocator);
}

Move<VkCommandPool> createCommandPool (const DeviceInterface&		vk,
									   VkDevice						device,
									   VkCommandPoolCreateFlags		flags,
									   deUint32						queueFamilyIndex,
									   const VkAllocationCallbacks*	pAllocator)
{
	const VkCommandPoolCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		DE_NULL,

		flags,
		queueFamilyIndex
	};

	return createCommandPool(vk, device, &createInfo, pAllocator);
}

Move<VkCommandBuffer> allocateCommandBuffer (const DeviceInterface&	vk,
											 VkDevice				device,
											 VkCommandPool			commandPool,
											 VkCommandBufferLevel	level)
{
	const VkCommandBufferAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		DE_NULL,

		commandPool,
		level,
		1
	};

	return allocateCommandBuffer(vk, device, &allocInfo);
}

Move<VkEvent> createEvent (const DeviceInterface&		vk,
						   VkDevice						device,
						   VkEventCreateFlags			flags,
						   const VkAllocationCallbacks*	pAllocateInfo)
{
	const VkEventCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,
		DE_NULL,

		flags
	};

	return createEvent(vk, device, &createInfo, pAllocateInfo);
}

#ifdef CTS_USES_VULKANSC

// add missing function in Vulkan SC, so that we are able to hack into shader module creation

Move<VkShaderModule> createShaderModule(const DeviceInterface& vk, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkShaderModule object = 0;
	VK_CHECK(vk.createShaderModule(device, pCreateInfo, pAllocator, &object));
	return Move<VkShaderModule>(check<VkShaderModule>(object), Deleter<VkShaderModule>(vk, device, pAllocator));
}

// stubs for functions removed in Vulkan SC

namespace refdetails
{

template<>
void Deleter<VkDeviceMemory>::operator() (VkDeviceMemory obj) const
{
	DE_UNREF(obj);
}

template<>
void Deleter<VkShaderModule>::operator() (VkShaderModule obj) const
{
	DE_UNREF(obj);
}

template<>
void Deleter<VkQueryPool>::operator() (VkQueryPool obj) const
{
	DE_UNREF(obj);
}

template<>
void Deleter<VkDescriptorPool>::operator() (VkDescriptorPool obj) const
{
	// vkDestroyDescriptorPool is unsupported in VulkanSC. Instead, reset the descriptor pool
    // so that any sets allocated from it will be implicitly freed (similar to if it were being
    // destroyed). Lots of tests rely on sets being implicitly freed.
	m_deviceIface->resetDescriptorPool(m_device, obj, 0);
}

template<>
void Deleter<VkCommandPool>::operator() (VkCommandPool obj) const
{
	DE_UNREF(obj);
}

template<>
void Deleter<VkSwapchainKHR>::operator() (VkSwapchainKHR obj) const
{
	DE_UNREF(obj);
}

template<>
void Deleter<VkSemaphoreSciSyncPoolNV>::operator() (VkSemaphoreSciSyncPoolNV obj) const
{
	DE_UNREF(obj);
}


} // refdetails

#endif // CTS_USES_VULKANSC

} // vk
