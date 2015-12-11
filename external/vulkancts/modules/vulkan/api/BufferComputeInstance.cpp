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

#include "BufferComputeInstance.hpp"
#include "ComputeInstanceResultBuffer.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"

using namespace vk;
/*
VkDescriptorInfo createDescriptorInfo (VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
	const VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		0,							// sampler
		0,							// imageView
		(VkImageLayout) 0,			// imageLayout
		{buffer, offset, range}		// bufferInfo
	};
	return resultInfo;
};

VkDescriptorInfo createDescriptorInfo (VkBufferView bufferView) {
	const VkDescriptorInfo resultInfo =
	{
		bufferView,					// bufferView
		0,							// sampler
		0,							// imageView
		(VkImageLayout) 0,			// imageLayout
		{(VkBuffer) 0, 0, 0}		// bufferInfo
	};
	return resultInfo;
};

VkDescriptorInfo createDescriptorInfo (VkSampler sampler) {
	const VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		sampler,					// sampler
		0,							// imageView
		(VkImageLayout) 0,			// imageLayout
		{(VkBuffer) 0, 0, 0}		// bufferInfo
	};
	return resultInfo;
};

VkDescriptorInfo createDescriptorInfo (VkImageView imageView, VkImageLayout layout) {
	const VkDescriptorInfo resultInfo =
	{
		0,						// bufferView
		0,						// sampler
		imageView,				// imageView
		layout,					// imageLayout
		{(VkBuffer) 0, 0, 0}	// bufferInfo
	};
	return resultInfo;
};

VkDescriptorInfo createDescriptorInfo (VkSampler sampler, VkImageView imageView, VkImageLayout layout) {
	const VkDescriptorInfo resultInfo =
	{
		0,						// bufferView
		sampler,				// sampler
		imageView,				// imageView
		layout,					// imageLayout
		{(VkBuffer) 0, 0, 0}	// bufferInfo
	};
	return resultInfo;
};

Move <VkBuffer> createColorDataBuffer (deUint32 offset, deUint32 bufferSize, const tcu::Vec4 &value1,
									   const tcu::Vec4 &value2, de::MovePtr <Allocation> *outAllocation,
									   vkt::Context &context) {
	const DeviceInterface &m_vki = context.getDeviceInterface();
	const VkDevice m_device = context.getDevice();
	Allocator &m_allocator = context.getDefaultAllocator();

	DE_ASSERT(offset + sizeof(tcu::Vec4[2]) <= bufferSize);

	const VkBufferUsageFlags usageFlags = (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	const VkBufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(VkDeviceSize) bufferSize,		// size
		usageFlags,						// usage
		0u,								// flags
		VK_SHARING_MODE_EXCLUSIVE,		// sharingMode
		0u,								// queueFamilyCount
		DE_NULL,						// pQueueFamilyIndices
	};
	Move <VkBuffer> buffer(createBuffer(m_vki, m_device, &createInfo));
	de::MovePtr <Allocation> allocation(
		allocateAndBindObjectMemory(m_vki, m_device, m_allocator, *buffer, MemoryRequirement::HostVisible));
	void *mapPtr = allocation->getHostPtr();

	if (offset)
		deMemset(mapPtr, 0x5A, (size_t) offset);
	deMemcpy((deUint8 *) mapPtr + offset, value1.getPtr(), sizeof(tcu::Vec4));
	deMemcpy((deUint8 *) mapPtr + offset + sizeof(tcu::Vec4), value2.getPtr(), sizeof(tcu::Vec4));
	deMemset((deUint8 *) mapPtr + offset + 2 * sizeof(tcu::Vec4), 0x5A,
			 (size_t) bufferSize - (size_t) offset - 2 * sizeof(tcu::Vec4));

	flushMappedMemoryRange(m_vki, m_device, allocation->getMemory(), allocation->getOffset(), bufferSize);

	*outAllocation = allocation;
	return buffer;
}

Move <VkDescriptorSetLayout> createDescriptorSetLayout (vkt::Context &context) {

	const DeviceInterface &m_vki = context.getDeviceInterface();
	const VkDevice m_device = context.getDevice();

	DescriptorSetLayoutBuilder builder;

	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	return builder.build(m_vki, m_device);
}

Move <VkDescriptorPool> createDescriptorPool (vkt::Context &context) {
	const DeviceInterface &m_vki = context.getDeviceInterface();
	const VkDevice m_device = context.getDevice();

	return DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u)
		.build(m_vki, m_device, VK_DESCRIPTOR_POOL_USAGE_ONE_SHOT, 1);
}

Move <VkDescriptorSet> createDescriptorSet (VkDescriptorPool pool, VkDescriptorSetLayout layout,
											VkBuffer viewA, deUint32 offsetA, VkBuffer viewB,
											deUint32 offsetB, VkBuffer resBuf, vkt::Context &context) {
	const DeviceInterface &m_vki = context.getDeviceInterface();
	const VkDevice m_device = context.getDevice();

	const VkDescriptorInfo resultInfo = createDescriptorInfo(resBuf, 0u,
															 (VkDeviceSize) ComputeInstanceResultBuffer::DATA_SIZE);
	const VkDescriptorInfo bufferInfos[2] =
	{
		createDescriptorInfo(viewA, (VkDeviceSize) offsetA, (VkDeviceSize)sizeof(tcu::Vec4[2])),
		createDescriptorInfo(viewB, (VkDeviceSize)offsetB, (VkDeviceSize)sizeof(tcu::Vec4[2])),
	};

	Move <VkDescriptorSet> descriptorSet = allocDescriptorSet(m_vki, m_device, pool,
															  VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, layout);
	DescriptorSetUpdateBuilder builder;

	// result
	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
						VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo);

	// buffers
	builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
						VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfos[0]);

	builder.update(m_vki, m_device);
	return descriptorSet;
}
*/