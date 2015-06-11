/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
	virtual 				~SimpleAllocation	(void);

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
