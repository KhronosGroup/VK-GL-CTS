
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



#include "BufferComputeInstance.hpp"

using namespace vk;
using tcu::TestLog;
using de::UniquePtr;


vk::VkDescriptorInfo createDescriptorInfo (vk::VkBuffer buffer, vk::VkDeviceSize offset, vk::VkDeviceSize range)
{
    const vk::VkDescriptorInfo resultInfo =
            {
                    0,							// bufferView
                    0,							// sampler
                    0,							// imageView
                    (vk::VkImageLayout)0,		// imageLayout
                    { buffer, offset, range }	// bufferInfo
            };
    return resultInfo;
};

vk::VkDescriptorInfo createDescriptorInfo (vk::VkBufferView bufferView)
{
    const vk::VkDescriptorInfo resultInfo =
            {
                    bufferView,					// bufferView
                    0,							// sampler
                    0,							// imageView
                    (vk::VkImageLayout)0,		// imageLayout
                    { (vk::VkBuffer)0, 0, 0 }	// bufferInfo
            };
    return resultInfo;
};

vk::VkDescriptorInfo createDescriptorInfo (vk::VkSampler sampler)
{
    const vk::VkDescriptorInfo resultInfo =
            {
                    0,							// bufferView
                    sampler,					// sampler
                    0,							// imageView
                    (vk::VkImageLayout)0,		// imageLayout
                    { (vk::VkBuffer)0, 0, 0 }	// bufferInfo
            };
    return resultInfo;
};

vk::VkDescriptorInfo createDescriptorInfo (vk::VkImageView imageView, vk::VkImageLayout layout)
{
    const vk::VkDescriptorInfo resultInfo =
            {
                    0,							// bufferView
                    0,							// sampler
                    imageView,					// imageView
                    layout,						// imageLayout
                    { (vk::VkBuffer)0, 0, 0 }	// bufferInfo
            };
    return resultInfo;
};

vk::VkDescriptorInfo createDescriptorInfo (vk::VkSampler sampler, vk::VkImageView imageView, vk::VkImageLayout layout)
{
    const vk::VkDescriptorInfo resultInfo =
            {
                    0,							// bufferView
                    sampler,					// sampler
                    imageView,					// imageView
                    layout,						// imageLayout
                    { (vk::VkBuffer)0, 0, 0 }	// bufferInfo
            };
    return resultInfo;
};

vk::Move<vk::VkBuffer> createColorDataBuffer (deUint32 offset, deUint32 bufferSize, const tcu::Vec4& value1, const tcu::Vec4& value2, de::MovePtr<vk::Allocation>* outAllocation, vkt::Context& context)
{
    const vk::DeviceInterface&				m_vki = context.getDeviceInterface();
    const vk::VkDevice						m_device = context.getDevice();
    vk::Allocator&							m_allocator = context.getDefaultAllocator();

    DE_ASSERT(offset + sizeof(tcu::Vec4[2]) <= bufferSize);

    const vk::VkBufferUsageFlags	usageFlags			= (vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) ;
    const vk::VkBufferCreateInfo	createInfo =
            {
                    vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    DE_NULL,
                    (vk::VkDeviceSize)bufferSize,	// size
                    usageFlags,						// usage
                    0u,								// flags
                    vk::VK_SHARING_MODE_EXCLUSIVE,	// sharingMode
                    0u,								// queueFamilyCount
                    DE_NULL,						// pQueueFamilyIndices
            };
    vk::Move<vk::VkBuffer>			buffer				(vk::createBuffer(m_vki, m_device, &createInfo));
    de::MovePtr<vk::Allocation>		allocation			(allocateAndBindObjectMemory(m_vki, m_device, m_allocator, *buffer, vk::MemoryRequirement::HostVisible));
    void*							mapPtr				= allocation->getHostPtr();

    if (offset)
        deMemset(mapPtr, 0x5A, (size_t)offset);
    deMemcpy((deUint8*)mapPtr + offset, value1.getPtr(), sizeof(tcu::Vec4));
    deMemcpy((deUint8*)mapPtr + offset + sizeof(tcu::Vec4), value2.getPtr(), sizeof(tcu::Vec4));
    deMemset((deUint8*)mapPtr + offset + 2 * sizeof(tcu::Vec4), 0x5A, (size_t)bufferSize - (size_t)offset - 2 * sizeof(tcu::Vec4));

    flushMappedMemoryRange(m_vki, m_device, allocation->getMemory(), allocation->getOffset(), bufferSize);

    *outAllocation = allocation;
    return buffer;
}

vk::Move<vk::VkDescriptorSetLayout> createDescriptorSetLayout (vkt::Context& context)
{

    const vk::DeviceInterface&				m_vki = context.getDeviceInterface();
    const vk::VkDevice						m_device = context.getDevice();

    vk::DescriptorSetLayoutBuilder builder;

    builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

    builder.addSingleBinding( vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);


    return builder.build(m_vki, m_device);
}

vk::Move<vk::VkDescriptorPool> createDescriptorPool (vkt::Context& context)
{
    const vk::DeviceInterface&				m_vki = context.getDeviceInterface();
    const vk::VkDevice						m_device = context.getDevice();

    return vk::DescriptorPoolBuilder()
            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType( vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
            .build(m_vki, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 1);
}

vk::Move<vk::VkDescriptorSet> createDescriptorSet (vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout, vk::VkBuffer viewA, deUint32 offsetA, vk::VkBuffer viewB, deUint32 offsetB, vk::VkBuffer resBuf, vkt::Context& context)
{
    const vk::DeviceInterface&				m_vki = context.getDeviceInterface();
    const vk::VkDevice						m_device = context.getDevice();

    const vk::VkDescriptorInfo		resultInfo		= createDescriptorInfo(resBuf, 0u, (vk::VkDeviceSize)ComputeInstanceResultBuffer::DATA_SIZE);
    const vk::VkDescriptorInfo		bufferInfos[2]	=
            {
                createDescriptorInfo(viewA, (vk::VkDeviceSize)offsetA, (vk::VkDeviceSize)sizeof(tcu::Vec4[2])),
                createDescriptorInfo(viewB, (vk::VkDeviceSize)offsetB, (vk::VkDeviceSize)sizeof(tcu::Vec4[2])),
            };

    vk::Move<vk::VkDescriptorSet>	descriptorSet	= allocDescriptorSet(m_vki, m_device, pool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
    vk::DescriptorSetUpdateBuilder	builder;

    // result
    builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

    // buffers
    builder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u),  vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfos[0]);

    builder.update(m_vki, m_device);
    return descriptorSet;
}
