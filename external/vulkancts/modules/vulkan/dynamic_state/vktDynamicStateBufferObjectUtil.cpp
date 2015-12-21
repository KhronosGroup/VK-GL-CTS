/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
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
 * \brief Buffer Object Util
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateBufferObjectUtil.hpp"

#include "vkQueryUtil.hpp"

namespace vkt
{
namespace DynamicState
{

Buffer::Buffer (const vk::DeviceInterface& vk, vk::VkDevice device, vk::Move<vk::VkBuffer> object)
	: m_allocation  (DE_NULL)
	, m_object		(object)
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

} // DynamicState
} // vkt
