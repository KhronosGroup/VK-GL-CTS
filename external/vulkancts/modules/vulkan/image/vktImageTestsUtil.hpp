#ifndef _VKTIMAGETESTSUTIL_HPP
#define _VKTIMAGETESTSUTIL_HPP
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
 * \file
 * \brief Image Tests Utility Classes
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
namespace image
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
std::string				getFormatPrefix					(const tcu::TextureFormat& format);
std::string				getShaderImageType				(const tcu::TextureFormat& format, const ImageType imageType, const bool multisample = false);
std::string				getShaderImageFormatQualifier	(const tcu::TextureFormat& format);
std::string				getGlslSamplerType				(const tcu::TextureFormat& format, vk::VkImageViewType type);
const char*				getGlslInputFormatType			(const vk::VkFormat format);
const char*				getGlslFormatType				(const vk::VkFormat format);
const char*				getGlslAttachmentType			(const vk::VkFormat format);
const char*				getGlslInputAttachmentType		(const vk::VkFormat format);
bool					isPackedType					(const vk::VkFormat format);
bool					isComponentSwizzled				(const vk::VkFormat format);
int						getNumUsedChannels				(const vk::VkFormat format);
bool					isFormatImageLoadStoreCapable	(const vk::VkFormat format);

class Image
{
public:
									Image			(const vk::DeviceInterface&		vk,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkImageCreateInfo&	imageCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);
	virtual							~Image			(void)		 {}

	const vk::VkImage&				get				(void) const { return *m_image; }
	const vk::VkImage&				operator*		(void) const { return get(); }

	virtual vk::VkSemaphore			getSemaphore	(void) const { return DE_NULL; }

									Image			(const Image&) = delete;
	Image&							operator=		(const Image&) = delete;

protected:
	using AllocationsVec = std::vector<de::SharedPtr<vk::Allocation>>;

									Image			(void);

	AllocationsVec					m_allocations;
	vk::Move<vk::VkImage>			m_image;
};

#ifndef CTS_USES_VULKANSC
class SparseImage : public Image
{
public:
									SparseImage		(const vk::DeviceInterface&		vkd,
													 vk::VkDevice					device,
													 vk::VkPhysicalDevice			physicalDevice,
													 const vk::InstanceInterface&	vki,
													 const vk::VkImageCreateInfo&	createInfo,
													 const vk::VkQueue				sparseQueue,
													 vk::Allocator&					allocator,
													 const tcu::TextureFormat&		format);

	virtual vk::VkSemaphore			getSemaphore	(void) const { return m_semaphore.get(); }

									SparseImage		(const SparseImage&) = delete;
	SparseImage&					operator=		(const SparseImage&) = delete;

protected:
	vk::Move<vk::VkSemaphore>		m_semaphore;
};
#endif // CTS_USES_VULKANSC

tcu::UVec3				getShaderGridSize	(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Size used for addresing image in a shader
tcu::UVec3				getLayerSize		(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Size of a single layer
deUint32				getNumLayers		(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Number of array layers (for array and cube types)
deUint32				getNumPixels		(const ImageType imageType, const tcu::UVec3& imageSize);	//!< Number of texels in an image
deUint32				getDimensions		(const ImageType imageType);								//!< Coordinate dimension used for addressing (e.g. 3 (x,y,z) for 2d array)
deUint32				getLayerDimensions	(const ImageType imageType);								//!< Coordinate dimension used for addressing a single layer (e.g. 2 (x,y) for 2d array)

vk::Move<vk::VkPipeline>			makeGraphicsPipeline			(const vk::DeviceInterface&					vk,
																	 const vk::VkDevice							device,
																	 const vk::VkPipelineLayout					pipelineLayout,
																	 const vk::VkRenderPass						renderPass,
																	 const vk::VkShaderModule					vertexModule,
																	 const vk::VkShaderModule					fragmentModule,
																	 const vk::VkExtent2D						renderSize,
																	 const deUint32								colorAttachmentCount,
																	 const bool									dynamicSize = false);

vk::Move<vk::VkRenderPass>			makeRenderPass					(const vk::DeviceInterface&					vk,
																	 const vk::VkDevice							device,
																	 const vk::VkFormat							inputFormat,
																	 const vk::VkFormat							colorFormat);

vk::VkBufferImageCopy				makeBufferImageCopy				(const vk::VkExtent3D						extent,
																	 const deUint32								arraySize);

vk::VkImageViewUsageCreateInfo		makeImageViewUsageCreateInfo	(const vk::VkImageUsageFlags				imageUsageFlags);

vk::VkSamplerCreateInfo				makeSamplerCreateInfo			();

inline vk::VkDeviceSize getImageSizeBytes (const tcu::IVec3& imageSize, const vk::VkFormat format)
{
	return tcu::getPixelSize(vk::mapVkFormat(format)) * imageSize.x() * imageSize.y() * imageSize.z();
}

tcu::UVec3			getCompressedImageResolutionInBlocks	(const vk::VkFormat format, const tcu::UVec3& size);
tcu::UVec3			getCompressedImageResolutionBlockCeil	(const vk::VkFormat format, const tcu::UVec3& size);
vk::VkDeviceSize	getCompressedImageSizeInBytes			(const vk::VkFormat format, const tcu::UVec3& size);
vk::VkDeviceSize	getUncompressedImageSizeInBytes			(const vk::VkFormat format, const tcu::UVec3& size);

std::string	getFormatShortString (const vk::VkFormat format);

std::vector<tcu::Vec4> createFullscreenQuad (void);

vk::VkBufferImageCopy makeBufferImageCopy (const deUint32 imageWidth, const deUint32 imageHeight, const deUint32 mipLevel = 0u, const deUint32 layer = 0u);
vk::VkBufferImageCopy makeBufferImageCopy (const deUint32 imageWidth, const deUint32 imageHeight, const deUint32 mipLevel, const deUint32 layer, const deUint32 bufferRowLength, const deUint32 bufferImageHeight);

void beginRenderPass (const vk::DeviceInterface&	vk,
					  const vk::VkCommandBuffer		commandBuffer,
					  const vk::VkRenderPass		renderPass,
					  const vk::VkFramebuffer		framebuffer,
					  const vk::VkExtent2D&			renderSize);

} // image
} // vkt

#endif // _VKTIMAGETESTSUTIL_HPP
