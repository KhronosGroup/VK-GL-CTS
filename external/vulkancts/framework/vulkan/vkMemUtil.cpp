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
 * \brief Memory management utilities.
 *//*--------------------------------------------------------------------*/

#include "vkMemUtil.hpp"
#include "vkStrUtil.hpp"

#include <sstream>

namespace vk
{

using de::MovePtr;

// Allocation

Allocation::Allocation (VkDeviceMemory memory, VkDeviceSize offset)
	: m_memory	(memory)
	, m_offset	(offset)
{
}

Allocation::~Allocation (void)
{
}

// SimpleAllocator

class SimpleAllocation : public Allocation
{
public:
							SimpleAllocation	(const DeviceInterface& vk, VkDevice device, VkDeviceMemory memory);
	virtual					~SimpleAllocation	(void);

private:
	const DeviceInterface&	m_vk;
	const VkDevice			m_device;
};

SimpleAllocation::SimpleAllocation (const DeviceInterface& vk, VkDevice device, VkDeviceMemory memory)
	: Allocation(memory, (VkDeviceSize)0)
	, m_vk		(vk)
	, m_device	(device)
{
}

SimpleAllocation::~SimpleAllocation (void)
{
	m_vk.freeMemory(m_device, getMemory());
}

SimpleAllocator::SimpleAllocator (const DeviceInterface& vk, VkDevice device)
	: m_vk		(vk)
	, m_device	(device)
{
}

MovePtr<Allocation> SimpleAllocator::allocate (const VkMemoryAllocInfo* allocInfo, VkDeviceSize alignment)
{
	VkDeviceMemory	mem	= DE_NULL;

	VK_CHECK(m_vk.allocMemory(m_device, allocInfo, &mem));
	TCU_CHECK(mem);

	DE_UNREF(alignment);

	try
	{
		return MovePtr<Allocation>(new SimpleAllocation(m_vk, m_device, mem));
	}
	catch (...)
	{
		m_vk.freeMemory(m_device, mem);
		throw;
	}
}

// Utils

MovePtr<Allocation> allocate (Allocator& allocator, VkDeviceSize allocationSize, VkMemoryPropertyFlags memProps, VkDeviceSize alignment, VkMemoryPriority memPriority)
{
	const VkMemoryAllocInfo	allocInfo	=
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO,	//	VkStructureType			sType;
		DE_NULL,								//	const void*				pNext;
		allocationSize,							//	VkDeviceSize			allocationSize;
		memProps,								//	VkMemoryPropertyFlags	memProps;
		memPriority,							//	VkMemoryPriority		memPriority;
	};

	return allocator.allocate(&allocInfo, alignment);
}

MovePtr<Allocation> allocate (Allocator& allocator, const VkMemoryRequirements& requirements, VkMemoryPropertyFlags memProps, VkMemoryPriority priority)
{
	if ((requirements.memPropsAllowed & memProps) != memProps)
	{
		std::ostringstream	msg;
		msg << getMemoryPropertyFlagsStr(memProps & ~requirements.memPropsAllowed) << " not supported by object type";
		TCU_THROW(NotSupportedError, msg.str().c_str());
	}

	return allocate(allocator, requirements.size, memProps | requirements.memPropsRequired, requirements.alignment, priority);
}

} // vk
