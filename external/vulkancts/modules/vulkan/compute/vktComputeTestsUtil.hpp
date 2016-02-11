#ifndef _VKTCOMPUTETESTSUTIL_HPP
#define _VKTCOMPUTETESTSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Compute tests utility classes
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"

namespace vkt
{
namespace compute
{

class Buffer
{
public:
									Buffer			(const vk::DeviceInterface&		vk,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkBufferCreateInfo&	bufferCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);

	const vk::VkBuffer&				get				(void) const { return *m_buffer; }
	const vk::VkBuffer&				operator*		(void) const { return get(); }
	vk::Allocation&					getAllocation	(void) const { return *m_allocation; }

private:
	de::MovePtr<vk::Allocation>		m_allocation;
	vk::Move<vk::VkBuffer>			m_buffer;

									Buffer			(const Buffer&);  // "deleted"
	Buffer&							operator=		(const Buffer&);
};

class Image
{
public:
									Image			(const vk::DeviceInterface&		vk,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkImageCreateInfo&	imageCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);

	const vk::VkImage&				get				(void) const { return *m_image; }
	const vk::VkImage&				operator*		(void) const { return get(); }
	vk::Allocation&					getAllocation	(void) const { return *m_allocation; }

private:
	de::MovePtr<vk::Allocation>		m_allocation;
	vk::Move<vk::VkImage>			m_image;

									Image			(const Image&);  // "deleted"
	Image&							operator=		(const Image&);
};

vk::Move<vk::VkCommandPool>			makeCommandPool				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const deUint32						queueFamilyIndex);

vk::Move<vk::VkCommandBuffer>		makeCommandBuffer			(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkCommandPool			commandPool);

vk::Move<vk::VkPipelineLayout>	makePipelineLayout				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device);

vk::Move<vk::VkPipelineLayout>	makePipelineLayout				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkDescriptorSetLayout	descriptorSetLayout);

vk::Move<vk::VkPipeline>		makeComputePipeline				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkPipelineLayout			pipelineLayout,
																 const vk::VkShaderModule			shaderModule);

vk::Move<vk::VkBufferView>		makeBufferView					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkBuffer					buffer,
																 const vk::VkFormat					format,
																 const vk::VkDeviceSize				offset,
																 const vk::VkDeviceSize				size);

vk::Move<vk::VkImageView>		makeImageView					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkImage					image,
																 const vk::VkImageViewType			imageViewType,
																 const vk::VkFormat					format,
																 const vk::VkImageSubresourceRange	subresourceRange);

vk::Move<vk::VkDescriptorSet>	makeDescriptorSet				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkDescriptorPool			descriptorPool,
																 const vk::VkDescriptorSetLayout	setLayout);

vk::VkBufferCreateInfo			makeBufferCreateInfo			(const vk::VkDeviceSize				bufferSize,
																 const vk::VkBufferUsageFlags		usage);

vk::VkBufferImageCopy			makeBufferImageCopy				(const vk::VkExtent3D				extent,
																 const deUint32						arraySize);

vk::VkBufferMemoryBarrier		makeBufferMemoryBarrier			(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask,
																 const vk::VkBuffer					buffer,
																 const vk::VkDeviceSize				offset,
																 const vk::VkDeviceSize				bufferSizeBytes);

vk::VkImageMemoryBarrier		makeImageMemoryBarrier			(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask,
																 const vk::VkImageLayout			oldLayout,
																 const vk::VkImageLayout			newLayout,
																 const vk::VkImage					image,
																 const vk::VkImageSubresourceRange	subresourceRange);

void							beginCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			cmdBuffer);

void							endCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			cmdBuffer);

void							submitCommandsAndWait			(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkQueue					queue,
																 const vk::VkCommandBuffer			cmdBuffer);

inline vk::VkExtent3D makeExtent3D (const tcu::IVec3& vec)
{
	return vk::makeExtent3D(vec.x(), vec.y(), vec.z());
}

inline vk::VkDeviceSize getImageSizeBytes (const tcu::IVec3& imageSize, const vk::VkFormat format)
{
	return tcu::getPixelSize(vk::mapVkFormat(format)) * imageSize.x() * imageSize.y() * imageSize.z();
}

} // compute
} // vkt

#endif // _VKTCOMPUTETESTSUTIL_HPP
