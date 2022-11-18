/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Object creation utilities
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMakeUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{
using namespace vk;
using de::MovePtr;

Move<VkCommandBuffer> makeCommandBuffer (const DeviceInterface& vk, const VkDevice device, const VkCommandPool commandPool)
{
	return allocateCommandBuffer(vk, device, commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

MovePtr<Allocation> bindImageDedicated (const InstanceInterface& vki, const DeviceInterface& vkd, const VkPhysicalDevice physDevice, const VkDevice device, const VkImage image, const MemoryRequirement requirement)
{
	MovePtr<Allocation> alloc(allocateDedicated(vki, vkd, physDevice, device, image, requirement));
	VK_CHECK(vkd.bindImageMemory(device, image, alloc->getMemory(), alloc->getOffset()));
	return alloc;
}

MovePtr<Allocation> bindBufferDedicated (const InstanceInterface& vki, const DeviceInterface& vkd, const VkPhysicalDevice physDevice, const VkDevice device, const VkBuffer buffer, const MemoryRequirement requirement)
{
	MovePtr<Allocation> alloc(allocateDedicated(vki, vkd, physDevice, device, buffer, requirement));
	VK_CHECK(vkd.bindBufferMemory(device, buffer, alloc->getMemory(), alloc->getOffset()));
	return alloc;
}

} // pipeline
} // vkt
