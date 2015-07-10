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
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "deInt32.h"

#include <sstream>

namespace vk
{

using de::MovePtr;

namespace
{

deUint32 selectMemoryTypeWithProperties (const VkPhysicalDeviceMemoryProperties& deviceMemProps, deUint32 allowedMemTypeBits, VkMemoryPropertyFlags allocProps)
{
	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < deviceMemProps.memoryTypeCount; memoryTypeNdx++)
	{
		if ((allowedMemTypeBits & (1u << memoryTypeNdx)) != 0 &&
			(deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags & allocProps) == allocProps)
			return memoryTypeNdx;
	}

	TCU_THROW(NotSupportedError, "No compatible memory type found");
}

} // anonymous

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
									SimpleAllocation	(Move<VkDeviceMemory> mem);
	virtual							~SimpleAllocation	(void);

private:
	const Unique<VkDeviceMemory>	m_memHolder;
};

SimpleAllocation::SimpleAllocation (Move<VkDeviceMemory> mem)
	: Allocation	(*mem, (VkDeviceSize)0)
	, m_memHolder	(mem)
{
}

SimpleAllocation::~SimpleAllocation (void)
{
}

SimpleAllocator::SimpleAllocator (const DeviceInterface& vk, VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemProps)
	: m_vk		(vk)
	, m_device	(device)
	, m_memProps(deviceMemProps)
{
}

MovePtr<Allocation> SimpleAllocator::allocate (const VkMemoryAllocInfo& allocInfo, VkDeviceSize alignment)
{
	DE_UNREF(alignment);
	return MovePtr<Allocation>(new SimpleAllocation(allocMemory(m_vk, m_device, &allocInfo)));
}

MovePtr<Allocation> SimpleAllocator::allocate (const VkMemoryRequirements& memReqs, VkMemoryPropertyFlags allocProps)
{
	const deUint32			memoryTypeNdx	= selectMemoryTypeWithProperties(m_memProps, memReqs.memoryTypeBits, allocProps);
	const VkMemoryAllocInfo	allocInfo	=
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO,	//	VkStructureType			sType;
		DE_NULL,								//	const void*				pNext;
		memReqs.size,							//	VkDeviceSize			allocationSize;
		memoryTypeNdx,							//	deUint32				memoryTypeIndex;
	};

	return allocate(allocInfo, memReqs.alignment);
}

} // vk
