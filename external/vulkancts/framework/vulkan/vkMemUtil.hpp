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

	virtual de::MovePtr<Allocation>	allocate	(const VkMemoryAllocInfo& allocInfo, VkDeviceSize alignment) = 0;
	virtual de::MovePtr<Allocation>	allocate	(const VkMemoryRequirements& memRequirements, VkMemoryPropertyFlags allocProps) = 0;
};

//! Allocator that backs every allocation with its own VkDeviceMemory
class SimpleAllocator : public Allocator
{
public:
											SimpleAllocator	(const DeviceInterface& vk, VkDevice device, const VkPhysicalDeviceMemoryProperties& deviceMemProps);

	de::MovePtr<Allocation>					allocate		(const VkMemoryAllocInfo& allocInfo, VkDeviceSize alignment);
	de::MovePtr<Allocation>					allocate		(const VkMemoryRequirements& memRequirements, VkMemoryPropertyFlags allocProps);

private:
	const DeviceInterface&					m_vk;
	const VkDevice							m_device;
	const VkPhysicalDeviceMemoryProperties	m_memProps;
};

} // vk

#endif // _VKMEMUTIL_HPP
