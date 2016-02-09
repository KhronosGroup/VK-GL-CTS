#ifndef _VKTDRAWBUFFEROBJECTUTIL_HPP
#define _VKTDRAWBUFFEROBJECTUTIL_HPP
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

#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"

#include "deSharedPtr.hpp"

namespace vkt
{
namespace Draw
{

class Buffer
{
public:

	static de::SharedPtr<Buffer> create			(const vk::DeviceInterface& vk, vk::VkDevice device, const vk::VkBufferCreateInfo &createInfo);

	static de::SharedPtr<Buffer> createAndAlloc (const vk::DeviceInterface&		vk,
												 vk::VkDevice					device,
												 const vk::VkBufferCreateInfo&	createInfo,
												 vk::Allocator&					allocator,
												 vk::MemoryRequirement			allocationMemoryProperties = vk::MemoryRequirement::Any);

								Buffer			(const vk::DeviceInterface &vk, vk::VkDevice device, vk::Move<vk::VkBuffer> object);

	void						bindMemory		(de::MovePtr<vk::Allocation> allocation);

	vk::VkBuffer				object			(void) const								{ return *m_object;		}
	vk::Allocation				getBoundMemory	(void) const								{ return *m_allocation;	}

private:

	Buffer										(const Buffer& other);	// Not allowed!
	Buffer&						operator=		(const Buffer& other);	// Not allowed!

	de::MovePtr<vk::Allocation>		m_allocation;
	vk::Unique<vk::VkBuffer>		m_object;

	const vk::DeviceInterface&		m_vk;
	vk::VkDevice					m_device;
};

} // Draw
} // vkt

#endif // _VKTDRAWBUFFEROBJECTUTIL_HPP
