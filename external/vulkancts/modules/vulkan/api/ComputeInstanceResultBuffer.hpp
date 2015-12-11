#ifndef _COMPUTEINSTANCERESULTBUFFER_HPP
#define _COMPUTEINSTANCERESULTBUFFER_HPP

/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 *//*--------------------------------------------------------------------*/

#include "vkRef.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTestLog.hpp"
#include "deUniquePtr.hpp"

class ComputeInstanceResultBuffer {
public:
	enum {
		DATA_SIZE = sizeof(tcu::Vec4[4])
	};

											ComputeInstanceResultBuffer(const vk::DeviceInterface &vki,
													vk::VkDevice device,
													vk::Allocator &allocator);

	void 									readResultContentsTo(tcu::Vec4 (*results)[4]) const;

	inline 									vk::VkBuffer getBuffer(void) const { return *m_buffer; }

	inline const void 						*getResultReadBarrier(void) const { return &m_bufferBarrier; }

private:
	static vk::Move <vk::VkBuffer> 			createResultBuffer(const vk::DeviceInterface &vki,
													  vk::VkDevice device,
													  vk::Allocator &allocator,
													  de::MovePtr <vk::Allocation> *outAllocation);

	static vk::VkBufferMemoryBarrier 		createResultBufferBarrier(vk::VkBuffer buffer);

	const vk::DeviceInterface &				m_vki;
	const vk::VkDevice 						m_device;

	de::MovePtr <vk::Allocation> 			m_bufferMem;
	const vk::Unique <vk::VkBuffer> 		m_buffer;
	const vk::VkBufferMemoryBarrier 		m_bufferBarrier;
};

de::MovePtr <vk::Allocation> 				allocateAndBindObjectMemory(const vk::DeviceInterface &vki, vk::VkDevice device,
														 vk::Allocator &allocator, vk::VkBuffer buffer,
														 vk::MemoryRequirement requirement);

#endif //_COMPUTEINSTANCERESULTBUFFER_HPP
