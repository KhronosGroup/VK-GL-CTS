#ifndef _VKMEMUTIL_HPP
#define _VKMEMUTIL_HPP
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

#include "vkDefs.hpp"
#include "deUniquePtr.hpp"

namespace vk
{

/*--------------------------------------------------------------------*//*!
 * \brief Memory allocation interface
 *
 * Allocation represents block of device memory and is allocated by
 * Allocator implementation. Test code should use Allocator for allocating
 * memory, unless there is a reason not to (for example testing vkAllocMemory).
 *
 * Allocation doesn't necessarily correspond to a whole VkDeviceMemory, but
 * instead it may represent sub-allocation. Thus whenever VkDeviceMemory
 * (getMemory()) managed by Allocation is passed to Vulkan API calls,
 * offset given by getOffset() must be used.
 *
 * If host-visible memory was requested, host pointer to the memory can
 * be queried with getHostPtr(). No offset is needed when accessing host
 * pointer, i.e. the pointer is already adjusted in case of sub-allocation.
 *
 * Memory mappings are managed solely by Allocation, i.e. unmapping or
 * re-mapping VkDeviceMemory owned by Allocation is not allowed.
 *//*--------------------------------------------------------------------*/
class Allocation
{
public:
	virtual					~Allocation	(void);

	//! Get VkDeviceMemory backing this allocation
	VkDeviceMemory			getMemory	(void) const { return m_memory;							}

	//! Get offset in VkDeviceMemory for this allocation
	VkDeviceSize			getOffset	(void) const { return m_offset;							}

	//! Get host pointer for this allocation. Only available for host-visible allocations
	void*					getHostPtr	(void) const { DE_ASSERT(m_hostPtr); return m_hostPtr;	}

protected:
							Allocation	(VkDeviceMemory memory, VkDeviceSize offset, void* hostPtr);

private:
	const VkDeviceMemory	m_memory;
	const VkDeviceSize		m_offset;
	void* const				m_hostPtr;
};

//! Memory allocation requirements
class MemoryRequirement
{
public:
	static const MemoryRequirement	Any;
	static const MemoryRequirement	HostVisible;
	static const MemoryRequirement	Coherent;
	static const MemoryRequirement	LazilyAllocated;

	inline MemoryRequirement		operator|			(MemoryRequirement requirement) const
	{
		return MemoryRequirement(m_flags | requirement.m_flags);
	}

	inline MemoryRequirement		operator&			(MemoryRequirement requirement) const
	{
		return MemoryRequirement(m_flags & requirement.m_flags);
	}

	bool							matchesHeap			(VkMemoryPropertyFlags heapFlags) const;

	inline operator					bool				(void) const { return m_flags != 0u; }

private:
	explicit						MemoryRequirement	(deUint32 flags);

	const deUint32					m_flags;

	enum Flags
	{
		FLAG_HOST_VISIBLE		= 1u << 0u,
		FLAG_COHERENT			= 1u << 1u,
		FLAG_LAZY_ALLOCATION	= 1u << 2u,
	};
};

//! Memory allocator interface
class Allocator
{
public:
									Allocator	(void) {}
	virtual							~Allocator	(void) {}

	virtual de::MovePtr<Allocation>	allocate	(const VkMemoryAllocateInfo& allocInfo, VkDeviceSize alignment) = 0;
	virtual de::MovePtr<Allocation>	allocate	(const VkMemoryRequirements& memRequirements, MemoryRequirement requirement) = 0;
};

//! Allocator that backs every allocation with its own VkDeviceMemory
class SimpleAllocator : public Allocator
{
public:
											SimpleAllocator	(const DeviceInterface& vk, VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemProps);

	de::MovePtr<Allocation>					allocate		(const VkMemoryAllocateInfo& allocInfo, VkDeviceSize alignment);
	de::MovePtr<Allocation>					allocate		(const VkMemoryRequirements& memRequirements, MemoryRequirement requirement);

private:
	const DeviceInterface&					m_vk;
	const VkDevice							m_device;
	const VkPhysicalDeviceMemoryProperties	m_memProps;
};

void	flushMappedMemoryRange		(const DeviceInterface& vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);
void	invalidateMappedMemoryRange	(const DeviceInterface& vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size);

} // vk

#endif // _VKMEMUTIL_HPP
