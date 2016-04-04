#ifndef _VKTSPARSERESOURCESTESTSUTIL_HPP
#define _VKTSPARSERESOURCESTESTSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesTestsUtil.hpp
 * \brief Sparse Resources Tests Utility Classes
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace sparse
{

enum ImageType
{
	IMAGE_TYPE_1D = 0,
	IMAGE_TYPE_1D_ARRAY,
	IMAGE_TYPE_2D,
	IMAGE_TYPE_2D_ARRAY,
	IMAGE_TYPE_3D,
	IMAGE_TYPE_CUBE,
	IMAGE_TYPE_CUBE_ARRAY,
	IMAGE_TYPE_BUFFER,

	IMAGE_TYPE_LAST
};

vk::VkImageType			mapImageType					(const ImageType imageType);
vk::VkImageViewType		mapImageViewType				(const ImageType imageType);
std::string				getImageTypeName				(const ImageType imageType);
std::string				getShaderImageType				(const tcu::TextureFormat& format, const ImageType imageType);
std::string				getShaderImageDataType			(const tcu::TextureFormat& format);
std::string				getShaderImageFormatQualifier	(const tcu::TextureFormat& format);

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
	vk::Unique<vk::VkBuffer>		m_buffer;
	de::UniquePtr<vk::Allocation>	m_allocation;

									Buffer			(const Buffer&);
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
	vk::Unique<vk::VkImage>			m_image;
	de::UniquePtr<vk::Allocation>	m_allocation;

									Image			(const Image&);
	Image&							operator=		(const Image&);
};

tcu::UVec3	getShaderGridSize		(const ImageType   imageType, 
									 const tcu::UVec3& imageSize, 
									 const deUint32	   mipLevel = 0);								//!< Size used for addresing image in a shader
tcu::UVec3	getLayerSize			(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Size of a single layer
deUint32	getNumLayers			(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Number of array layers (for array and cube types)
deUint32	getNumPixels			(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Number of texels in an image
deUint32	getDimensions			(const ImageType imageType);								//!< Coordinate dimension used for addressing (e.g. 3 (x,y,z) for 2d array)
deUint32	getLayerDimensions		(const ImageType imageType);								//!< Coordinate dimension used for addressing a single layer (e.g. 2 (x,y) for 2d array)
bool		isImageSizeSupported	(const ImageType					imageType,
									 const tcu::UVec3&					imageSize,
									 const vk::VkPhysicalDeviceLimits&	limits);				//!< Check is the requested image size is not above device limits

vk::Move<vk::VkCommandPool>		makeCommandPool					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const deUint32						queueFamilyIndex);

vk::Move<vk::VkCommandBuffer>	makeCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkCommandPool			commandPool);

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

vk::Move<vk::VkSemaphore>		makeSemaphore					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device);

vk::VkBufferCreateInfo			makeBufferCreateInfo			(const vk::VkDeviceSize				bufferSize,
																 const vk::VkBufferUsageFlags		usage);

vk::VkBufferImageCopy			makeBufferImageCopy				(const vk::VkExtent3D				extent,
																 const deUint32						layersCount,
																 const deUint32						mipmapLevel = 0u,
																 const vk::VkDeviceSize				bufferOffset = 0ull);

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

vk::VkMemoryBarrier				makeMemoryBarrier				(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask);

void							beginCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			cmdBuffer);

void							endCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			cmdBuffer);

void							submitCommands					(const vk::DeviceInterface&			vk,
																 const vk::VkQueue					queue,
																 const vk::VkCommandBuffer			cmdBuffer,
																 const deUint32						waitSemaphoreCount		= 0,
																 const vk::VkSemaphore*				pWaitSemaphores			= DE_NULL,
																 const vk::VkPipelineStageFlags*	pWaitDstStageMask		= DE_NULL,
																 const deUint32						signalSemaphoreCount	= 0,
																 const vk::VkSemaphore*				pSignalSemaphores		= DE_NULL);

void							submitCommandsAndWait			(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkQueue					queue,
																 const vk::VkCommandBuffer			cmdBuffer,
																 const deUint32						waitSemaphoreCount		= 0,
																 const vk::VkSemaphore*				pWaitSemaphores			= DE_NULL,
																 const vk::VkPipelineStageFlags*	pWaitDstStageMask		= DE_NULL,
																 const deUint32						signalSemaphoreCount	= 0,
																 const vk::VkSemaphore*				pSignalSemaphores		= DE_NULL);

vk::VkExtent3D					mipLevelExtents					(const vk::VkExtent3D&				baseExtents, 
																 const deUint32						mipLevel);

tcu::UVec3						mipLevelExtents					(const tcu::UVec3&					baseExtents,
																 const deUint32						mipLevel);

deUint32						getImageMaxMipLevels			(const vk::VkImageFormatProperties& imageFormatProperties,
																 const vk::VkImageCreateInfo&		imageInfo);

deUint32						getImageMipLevelSizeInBytes		(const vk::VkExtent3D&				baseExtents,
																 const deUint32						layersCount,
																 const tcu::TextureFormat&			format, 
																 const deUint32						mipmapLevel);

deUint32						getImageSizeInBytes				(const vk::VkExtent3D&				baseExtents,
																 const deUint32						layersCount,
																 const tcu::TextureFormat&			format, 
																 const deUint32						mipmapLevelsCount = 1u);

template<typename T>
inline de::SharedPtr<vk::Unique<T> > makeVkSharedPtr			(vk::Move<T> vkMove)
{
	return de::SharedPtr<vk::Unique<T> >(new vk::Unique<T>(vkMove));
}

} // sparse
} // vkt

#endif // _VKTSPARSERESOURCESTESTSUTIL_HPP
