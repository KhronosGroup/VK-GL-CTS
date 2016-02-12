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
#include "vkRefUtil.hpp"
#include "deInt32.h"

#include <sstream>

namespace vk
{

using de::UniquePtr;
using de::MovePtr;

namespace
{

class HostPtr
{
public:
								HostPtr		(const DeviceInterface& vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags);
								~HostPtr	(void);

	void*						get			(void) const { return m_ptr; }

private:
	const DeviceInterface&		m_vkd;
	const VkDevice				m_device;
	const VkDeviceMemory		m_memory;
	void* const					m_ptr;
};

void* mapMemory (const DeviceInterface& vkd, VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	void* hostPtr = DE_NULL;
	VK_CHECK(vkd.mapMemory(device, mem, offset, size, flags, &hostPtr));
	TCU_CHECK(hostPtr);
	return hostPtr;
}

HostPtr::HostPtr (const DeviceInterface& vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
	: m_vkd		(vkd)
	, m_device	(device)
	, m_memory	(memory)
	, m_ptr		(mapMemory(vkd, device, memory, offset, size, flags))
{
}

HostPtr::~HostPtr (void)
{
	m_vkd.unmapMemory(m_device, m_memory);
}

deUint32 selectMatchingMemoryType (const VkPhysicalDeviceMemoryProperties& deviceMemProps, deUint32 allowedMemTypeBits, MemoryRequirement requirement)
{
	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < deviceMemProps.memoryTypeCount; memoryTypeNdx++)
	{
		if ((allowedMemTypeBits & (1u << memoryTypeNdx)) != 0 &&
			requirement.matchesHeap(deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags))
			return memoryTypeNdx;
	}

	TCU_THROW(NotSupportedError, "No compatible memory type found");
}

bool isHostVisibleMemory (const VkPhysicalDeviceMemoryProperties& deviceMemProps, deUint32 memoryTypeNdx)
{
	DE_ASSERT(memoryTypeNdx < deviceMemProps.memoryTypeCount);
	return (deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
}

} // anonymous

// Allocation

Allocation::Allocation (VkDeviceMemory memory, VkDeviceSize offset, void* hostPtr)
	: m_memory	(memory)
	, m_offset	(offset)
	, m_hostPtr	(hostPtr)
{
}

Allocation::~Allocation (void)
{
}

// MemoryRequirement

const MemoryRequirement MemoryRequirement::Any				= MemoryRequirement(0x0u);
const MemoryRequirement MemoryRequirement::HostVisible		= MemoryRequirement(MemoryRequirement::FLAG_HOST_VISIBLE);
const MemoryRequirement MemoryRequirement::Coherent			= MemoryRequirement(MemoryRequirement::FLAG_COHERENT);
const MemoryRequirement MemoryRequirement::LazilyAllocated	= MemoryRequirement(MemoryRequirement::FLAG_LAZY_ALLOCATION);

bool MemoryRequirement::matchesHeap (VkMemoryPropertyFlags heapFlags) const
{
	// sanity check
	if ((m_flags & FLAG_COHERENT) && !(m_flags & FLAG_HOST_VISIBLE))
		DE_FATAL("Coherent memory must be host-visible");
	if ((m_flags & FLAG_HOST_VISIBLE) && (m_flags & FLAG_LAZY_ALLOCATION))
		DE_FATAL("Lazily allocated memory cannot be mappable");

	// host-visible
	if ((m_flags & FLAG_HOST_VISIBLE) && !(heapFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
		return false;

	// coherent
	if ((m_flags & FLAG_COHERENT) && !(heapFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return false;

	// lazy
	if ((m_flags & FLAG_LAZY_ALLOCATION) && !(heapFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT))
		return false;

	return true;
}

MemoryRequirement::MemoryRequirement (deUint32 flags)
	: m_flags(flags)
{
}

// SimpleAllocator

class SimpleAllocation : public Allocation
{
public:
									SimpleAllocation	(Move<VkDeviceMemory> mem, MovePtr<HostPtr> hostPtr);
	virtual							~SimpleAllocation	(void);

private:
	const Unique<VkDeviceMemory>	m_memHolder;
	const UniquePtr<HostPtr>		m_hostPtr;
};

SimpleAllocation::SimpleAllocation (Move<VkDeviceMemory> mem, MovePtr<HostPtr> hostPtr)
	: Allocation	(*mem, (VkDeviceSize)0, hostPtr ? hostPtr->get() : DE_NULL)
	, m_memHolder	(mem)
	, m_hostPtr		(hostPtr)
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

MovePtr<Allocation> SimpleAllocator::allocate (const VkMemoryAllocateInfo& allocInfo, VkDeviceSize alignment)
{
	DE_UNREF(alignment);

	Move<VkDeviceMemory>	mem		= allocateMemory(m_vk, m_device, &allocInfo);
	MovePtr<HostPtr>		hostPtr;

	if (isHostVisibleMemory(m_memProps, allocInfo.memoryTypeIndex))
		hostPtr = MovePtr<HostPtr>(new HostPtr(m_vk, m_device, *mem, 0u, allocInfo.allocationSize, 0u));

	return MovePtr<Allocation>(new SimpleAllocation(mem, hostPtr));
}

MovePtr<Allocation> SimpleAllocator::allocate (const VkMemoryRequirements& memReqs, MemoryRequirement requirement)
{
	const deUint32				memoryTypeNdx	= selectMatchingMemoryType(m_memProps, memReqs.memoryTypeBits, requirement);
	const VkMemoryAllocateInfo	allocInfo		=
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType			sType;
		DE_NULL,								//	const void*				pNext;
		memReqs.size,							//	VkDeviceSize			allocationSize;
		memoryTypeNdx,							//	deUint32				memoryTypeIndex;
	};

	Move<VkDeviceMemory>		mem				= allocateMemory(m_vk, m_device, &allocInfo);
	MovePtr<HostPtr>			hostPtr;

	if (requirement & MemoryRequirement::HostVisible)
	{
		DE_ASSERT(isHostVisibleMemory(m_memProps, allocInfo.memoryTypeIndex));
		hostPtr = MovePtr<HostPtr>(new HostPtr(m_vk, m_device, *mem, 0u, allocInfo.allocationSize, 0u));
	}

	return MovePtr<Allocation>(new SimpleAllocation(mem, hostPtr));
}

void flushMappedMemoryRange (const DeviceInterface& vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
{
	const VkMappedMemoryRange	range	=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		DE_NULL,
		memory,
		offset,
		size
	};

	VK_CHECK(vkd.flushMappedMemoryRanges(device, 1u, &range));
}

void invalidateMappedMemoryRange (const DeviceInterface& vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size)
{
	const VkMappedMemoryRange	range	=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		DE_NULL,
		memory,
		offset,
		size
	};

	VK_CHECK(vkd.invalidateMappedMemoryRanges(device, 1u, &range));
}

} // vk
