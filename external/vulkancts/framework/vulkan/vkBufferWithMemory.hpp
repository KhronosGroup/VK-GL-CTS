#ifndef _VKBUFFERWITHMEMORY_HPP
#define _VKBUFFERWITHMEMORY_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Google Inc.
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
 * \brief Buffer backed with memory
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

namespace vk
{
class BufferWithMemory
{
public:
										BufferWithMemory	(const vk::DeviceInterface&		vk,
															 const vk::VkDevice				device,
															 vk::Allocator&					allocator,
															 const vk::VkBufferCreateInfo&	bufferCreateInfo,
															 const vk::MemoryRequirement	memoryRequirement,
															 const bool						bindOnCreation = true)

											: m_vk			(vk)
											, m_device		(device)
											, m_buffer		(createBuffer(vk, device, &bufferCreateInfo))
											, m_allocation	(allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement))
											, m_memoryBound	(false)
										{
											if (bindOnCreation)
												bindMemory();
										}

	const vk::VkBuffer&					get				(void) const { return *m_buffer; }
	const vk::VkBuffer&					operator*		(void) const { return get(); }
	vk::Allocation&						getAllocation	(void) const { return *m_allocation; }
	void								bindMemory		(void)
	{
		if (!m_memoryBound)
		{
			VK_CHECK(m_vk.bindBufferMemory(m_device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
			m_memoryBound = true;
		}
	}

private:
	const vk::DeviceInterface&			m_vk;
	const vk::VkDevice					m_device;
	const vk::Unique<vk::VkBuffer>		m_buffer;
	const de::UniquePtr<vk::Allocation>	m_allocation;
	bool								m_memoryBound;

	// "deleted"
										BufferWithMemory	(const BufferWithMemory&);
	BufferWithMemory					operator=			(const BufferWithMemory&);
};
} // vk

#endif // _VKBUFFERWITHMEMORY_HPP
