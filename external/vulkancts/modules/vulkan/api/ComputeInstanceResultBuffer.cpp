
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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



#include "ComputeInstanceResultBuffer.hpp"


using namespace vk;
using tcu::TestLog;
using de::UniquePtr;


ComputeInstanceResultBuffer::ComputeInstanceResultBuffer (const vk::DeviceInterface&	vki,
                                                          vk::VkDevice					device,
                                                          vk::Allocator&				allocator)
        : m_vki				(vki)
        , m_device			(device)
        , m_bufferMem		(DE_NULL)
        , m_buffer			(createResultBuffer(m_vki, m_device, allocator, &m_bufferMem))
        , m_bufferBarrier	(createResultBufferBarrier(*m_buffer))
{
}

void ComputeInstanceResultBuffer::readResultContentsTo (tcu::Vec4 (*results)[4]) const
{
    invalidateMappedMemoryRange(m_vki, m_device, m_bufferMem->getMemory(), m_bufferMem->getOffset(), sizeof(*results));
    deMemcpy(*results, m_bufferMem->getHostPtr(), sizeof(*results));
}

de::MovePtr<vk::Allocation> allocateAndBindObjectMemory (const vk::DeviceInterface& vki, vk::VkDevice device, vk::Allocator& allocator, vk::VkBuffer buffer, vk::MemoryRequirement requirement)
{
    const vk::VkMemoryRequirements	requirements	= vk::getBufferMemoryRequirements(vki, device, buffer);
    de::MovePtr<vk::Allocation>		allocation		= allocator.allocate(requirements, requirement);

    VK_CHECK(vki.bindBufferMemory(device, buffer, allocation->getMemory(), allocation->getOffset()));
    return allocation;
}


vk::Move<vk::VkBuffer> ComputeInstanceResultBuffer::createResultBuffer (const vk::DeviceInterface&		vki,
                                                                        vk::VkDevice					device,
                                                                        vk::Allocator&					allocator,
                                                                        de::MovePtr<vk::Allocation>*	outAllocation)
{
    const vk::VkBufferCreateInfo	createInfo	=
            {
                    vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    DE_NULL,
                    (vk::VkDeviceSize)DATA_SIZE,				// size
                    vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// usage
                    0u,											// flags
                    vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
                    0u,											// queueFamilyCount
                    DE_NULL,									// pQueueFamilyIndices
            };
    vk::Move<vk::VkBuffer>			buffer		(vk::createBuffer(vki, device, &createInfo));
    de::MovePtr<vk::Allocation>		allocation	(allocateAndBindObjectMemory(vki, device, allocator, *buffer, vk::MemoryRequirement::HostVisible));
    const float						clearValue	= -1.0f;
    void*							mapPtr		= allocation->getHostPtr();

    for (size_t offset = 0; offset < DATA_SIZE; offset += sizeof(float))
        deMemcpy(((deUint8*)mapPtr) + offset, &clearValue, sizeof(float));

    flushMappedMemoryRange(vki, device, allocation->getMemory(), allocation->getOffset(), (vk::VkDeviceSize)DATA_SIZE);

    *outAllocation = allocation;
    return buffer;
}

vk::VkBufferMemoryBarrier ComputeInstanceResultBuffer::createResultBufferBarrier (vk::VkBuffer buffer)
{
    const vk::VkBufferMemoryBarrier bufferBarrier =
            {
                    vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    DE_NULL,
                    vk::VK_MEMORY_OUTPUT_SHADER_WRITE_BIT,		// outputMask
                    vk::VK_MEMORY_INPUT_HOST_READ_BIT,			// inputMask
                    vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
                    vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
                    buffer,										// buffer
                    (vk::VkDeviceSize)0u,						// offset
                    DATA_SIZE,									// size
            };
    return bufferBarrier;
}
