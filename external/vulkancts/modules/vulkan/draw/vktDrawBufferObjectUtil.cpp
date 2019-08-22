/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Buffer Object Util
 *//*--------------------------------------------------------------------*/

#include "vktDrawBufferObjectUtil.hpp"

#include "vkQueryUtil.hpp"

namespace vkt
{
namespace Draw
{

Buffer::Buffer (const vk::DeviceInterface& vk, vk::VkDevice device, vk::Move<vk::VkBuffer> object_)
	: m_allocation  (DE_NULL)
	, m_object		(object_)
	, m_vk			(vk)
	, m_device		(device)
{
}

void Buffer::bindMemory (de::MovePtr<vk::Allocation> allocation)
{
	DE_ASSERT(allocation);
	VK_CHECK(m_vk.bindBufferMemory(m_device, *m_object, allocation->getMemory(), allocation->getOffset()));

	DE_ASSERT(!m_allocation);
	m_allocation = allocation;
}

de::SharedPtr<Buffer> Buffer::createAndAlloc (const vk::DeviceInterface& vk,
											  vk::VkDevice device,
											  const vk::VkBufferCreateInfo &createInfo,
											  vk::Allocator &allocator,
											  vk::MemoryRequirement memoryRequirement)
{
	de::SharedPtr<Buffer> ret = create(vk, device, createInfo);

	vk::VkMemoryRequirements bufferRequirements = vk::getBufferMemoryRequirements(vk, device, ret->object());
	ret->bindMemory(allocator.allocate(bufferRequirements, memoryRequirement));
	return ret;
}

de::SharedPtr<Buffer> Buffer::create (const vk::DeviceInterface& vk,
									  vk::VkDevice device,
									  const vk::VkBufferCreateInfo& createInfo)
{
	return de::SharedPtr<Buffer>(new Buffer(vk, device, vk::createBuffer(vk, device, &createInfo)));
}

void bufferBarrier (const vk::DeviceInterface&	vk,
					vk::VkCommandBuffer			cmdBuffer,
					vk::VkBuffer				buffer,
					vk::VkAccessFlags			srcAccessMask,
					vk::VkAccessFlags			dstAccessMask,
					vk::VkPipelineStageFlags	srcStageMask,
					vk::VkPipelineStageFlags	dstStageMask)
{
	vk::VkBufferMemoryBarrier barrier;
	barrier.sType				= vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext				= DE_NULL;
	barrier.srcAccessMask		= srcAccessMask;
	barrier.dstAccessMask		= dstAccessMask;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer				= buffer;
	barrier.offset				= 0;
	barrier.size				= VK_WHOLE_SIZE;

	vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (vk::VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL,
						  1, &barrier, 0, (const vk::VkImageMemoryBarrier*)DE_NULL);
}

} // Draw
} // vkt
