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

Move<VkPipeline> createGraphicsPipeline(const DeviceInterface &vk, VkDevice device, VkPipelineCache pipelineCache,
                                        const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator = nullptr);
Move<VkPipeline> createComputePipeline(const DeviceInterface &vk, VkDevice device, VkPipelineCache pipelineCache,
                                       const VkComputePipelineCreateInfo *pCreateInfo,
                                       const VkAllocationCallbacks *pAllocator = nullptr);
#ifndef CTS_USES_VULKANSC
Move<VkPipeline> createRayTracingPipelineNV(const DeviceInterface &vk, VkDevice device, VkPipelineCache pipelineCache,
                                            const VkRayTracingPipelineCreateInfoNV *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator = nullptr);
Move<VkPipeline> createRayTracingPipelineKHR(const DeviceInterface &vk, VkDevice device,
                                             VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache,
                                             const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator = nullptr);
#endif // CTS_USES_VULKANSC

Move<VkCommandBuffer> allocateCommandBuffer(const DeviceInterface &vk, VkDevice device,
                                            const VkCommandBufferAllocateInfo *pAllocateInfo);
void allocateCommandBuffers(const DeviceInterface &vk, VkDevice device,
                            const VkCommandBufferAllocateInfo *pAllocateInfo, Move<VkCommandBuffer> *pCommandBuffers);
Move<VkDescriptorSet> allocateDescriptorSet(const DeviceInterface &vk, VkDevice device,
                                            const VkDescriptorSetAllocateInfo *pAllocateInfo);

Move<VkSemaphore> createSemaphore(const DeviceInterface &vk, VkDevice device,
                                  VkSemaphoreCreateFlags flags            = (VkSemaphoreCreateFlags)0,
                                  const VkAllocationCallbacks *pAllocator = nullptr);

Move<VkSemaphore> createSemaphoreType(const DeviceInterface &vk, VkDevice device, VkSemaphoreType type,
                                      VkSemaphoreCreateFlags flags            = (VkSemaphoreCreateFlags)0,
                                      const uint64_t initialValue             = 0,
                                      const VkAllocationCallbacks *pAllocator = nullptr);

Move<VkFence> createFence(const DeviceInterface &vk, VkDevice device, VkFenceCreateFlags flags = (VkFenceCreateFlags)0,
                          const VkAllocationCallbacks *pAllocator = nullptr);

Move<VkCommandPool> createCommandPool(const DeviceInterface &vk, VkDevice device, VkCommandPoolCreateFlags flags,
                                      uint32_t queueFamilyIndex, const VkAllocationCallbacks *pAllocator = nullptr);

Move<VkCommandBuffer> allocateCommandBuffer(const DeviceInterface &vk, VkDevice device, VkCommandPool commandPool,
                                            VkCommandBufferLevel level);

Move<VkEvent> createEvent(const DeviceInterface &vk, VkDevice device, VkEventCreateFlags flags = (VkEventCreateFlags)0u,
                          const VkAllocationCallbacks *pAllocateInfo = nullptr);

#ifdef CTS_USES_VULKANSC
Move<VkShaderModule> createShaderModule(const DeviceInterface &vk, VkDevice device,
                                        const VkShaderModuleCreateInfo *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator = nullptr);
#endif // CTS_USES_VULKANSC

} // namespace vk

#endif // _VKREFUTIL_HPP
