#ifndef _VKMEMUTIL_HPP
#define _VKMEMUTIL_HPP
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

#include "vkDefs.hpp"
#include "deUniquePtr.hpp"

namespace vk
{

//! Memory allocation interface
class Allocation
{
public:
	virtual					~Allocation	(void);

	VkDeviceMemory			getMemory	(void) const { return m_memory;	}
	VkDeviceSize			getOffset	(void) const { return m_offset;	}

protected:
							Allocation	(VkDeviceMemory memory, VkDeviceSize offset);

private:
	const VkDeviceMemory	m_memory;
	const VkDeviceSize		m_offset;
};

//! Memory allocator interface
class Allocator
{
public:
									Allocator	(void) {}
	virtual							~Allocator	(void) {}

	virtual de::MovePtr<Allocation>	allocate	(const VkMemoryAllocInfo* allocInfo, VkDeviceSize alignment) = 0;
};

//! Allocator that backs every allocation with its own VkDeviceMemory
class SimpleAllocator : public Allocator
{
public:
								SimpleAllocator	(const DeviceInterface& vk, VkDevice device);

	de::MovePtr<Allocation>		allocate		(const VkMemoryAllocInfo* allocInfo, VkDeviceSize alignment);

private:
	const DeviceInterface&		m_vk;
	VkDevice					m_device;
};

// Utilities

de::MovePtr<Allocation>		allocate	(Allocator&						allocator,
										 VkDeviceSize					allocationSize,
										 VkMemoryPropertyFlags			memProps,
										 VkDeviceSize					alignment,
										 VkMemoryPriority				memPriority = VK_MEMORY_PRIORITY_UNUSED);

de::MovePtr<Allocation>		allocate	(Allocator&						allocator,
										 const VkMemoryRequirements&	requirements,
										 VkMemoryPropertyFlags			memProps = 0u,
										 VkMemoryPriority				priority = VK_MEMORY_PRIORITY_UNUSED);

} // vk

#endif // _VKMEMUTIL_HPP
