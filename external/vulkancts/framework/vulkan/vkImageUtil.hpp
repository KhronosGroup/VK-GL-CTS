#ifndef _VKIMAGEUTIL_HPP
#define _VKIMAGEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2015 Google Inc.
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
 * \brief Utilities for images.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"
#include "deSharedPtr.hpp"

namespace vk
{

bool						isFloatFormat				(VkFormat format);
bool						isUfloatFormat				(VkFormat format);
bool						isSfloatFormat				(VkFormat format);
bool						isUnormFormat				(VkFormat format);
bool						isSnormFormat				(VkFormat format);
bool						isIntFormat					(VkFormat format);
bool						isUintFormat				(VkFormat format);
bool						isDepthStencilFormat		(VkFormat format);
bool						isCompressedFormat			(VkFormat format);
bool						isSrgbFormat				(VkFormat format);

bool						isSupportedByFramework		(VkFormat format);
void						checkImageSupport			(const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const VkImageCreateInfo& imageCreateInfo);

tcu::TextureFormat			mapVkFormat					(VkFormat format);
tcu::CompressedTexFormat	mapVkCompressedFormat		(VkFormat format);
tcu::TextureFormat			getDepthCopyFormat			(VkFormat combinedFormat);
tcu::TextureFormat			getStencilCopyFormat		(VkFormat combinedFormat);

tcu::Sampler				mapVkSampler				(const VkSamplerCreateInfo& samplerCreateInfo);
tcu::Sampler::CompareMode	mapVkSamplerCompareOp		(VkCompareOp compareOp);
tcu::Sampler::WrapMode		mapVkSamplerAddressMode		(VkSamplerAddressMode addressMode);
tcu::Sampler::ReductionMode mapVkSamplerReductionMode	(VkSamplerReductionMode reductionMode);
tcu::Sampler::FilterMode	mapVkMinTexFilter			(VkFilter filter, VkSamplerMipmapMode mipMode);
tcu::Sampler::FilterMode	mapVkMagTexFilter			(VkFilter filter);

VkFilter					mapFilterMode				(tcu::Sampler::FilterMode filterMode);
VkSamplerMipmapMode			mapMipmapMode				(tcu::Sampler::FilterMode filterMode);
VkSamplerAddressMode		mapWrapMode					(tcu::Sampler::WrapMode wrapMode);
VkCompareOp					mapCompareMode				(tcu::Sampler::CompareMode mode);
VkFormat					mapTextureFormat			(const tcu::TextureFormat& format);
VkFormat					mapCompressedTextureFormat	(const tcu::CompressedTexFormat format);
VkSamplerCreateInfo			mapSampler					(const tcu::Sampler& sampler, const tcu::TextureFormat& format, float minLod = 0.0f, float maxLod = 1000.0f, bool unnormal = false);
rr::GenericVec4				mapVkColor					(const VkClearColorValue& color);
VkClearColorValue			mapVkColor					(const rr::GenericVec4& color);

void						imageUtilSelfTest			(void);

float						getRepresentableDiffUnorm	(const VkFormat format, const deUint32 componentNdx);
float						getRepresentableDiffSnorm	(const VkFormat format, const deUint32 componentNdx);
deUint32					getFormatComponentWidth		(const VkFormat format, const deUint32 componentNdx);
deUint32					getBlockSizeInBytes			(const VkFormat compressedFormat);
deUint32					getBlockWidth				(const VkFormat compressedFormat);
deUint32					getBlockHeight				(const VkFormat compressedFormat);

const deUint32 BUFFER_IMAGE_COPY_OFFSET_GRANULARITY = 4u;

// \todo [2017-05-18 pyry] Consider moving this to tcu
struct PlanarFormatDescription
{
	enum
	{
		MAX_CHANNELS	= 4,
		MAX_PLANES		= 3
	};

	enum ChannelFlags
	{
		CHANNEL_R	= (1u<<0),	// Has "R" (0) channel
		CHANNEL_G	= (1u<<1),	// Has "G" (1) channel
		CHANNEL_B	= (1u<<2),	// Has "B" (2) channel
		CHANNEL_A	= (1u<<3),	// Has "A" (3) channel
	};

	struct Plane
	{
		deUint8		elementSizeBytes;
		deUint8		widthDivisor;
		deUint8		heightDivisor;
		VkFormat	planeCompatibleFormat;
	};

	struct Channel
	{
		deUint8		planeNdx;
		deUint8		type;				// tcu::TextureChannelClass value
		deUint8		offsetBits;			// Offset in element in bits
		deUint8		sizeBits;			// Value size in bits
		deUint8		strideBytes;		// Pixel stride (in bytes), usually plane elementSize
	};

	deUint8		numPlanes;
	deUint8		presentChannels;
	deUint8		blockWidth;
	deUint8		blockHeight;
	Plane		planes[MAX_PLANES];
	Channel		channels[MAX_CHANNELS];

	inline bool hasChannelNdx (deUint32 ndx) const
	{
		DE_ASSERT(de::inBounds(ndx, 0u, 4u));
		return (presentChannels & (1u<<ndx)) != 0;
	}
};

bool							isYCbCrFormat					(VkFormat						format);
PlanarFormatDescription			getPlanarFormatDescription		(VkFormat						format);
int								getPlaneCount					(VkFormat						format);
deUint32						getMipmapCount					(VkFormat						format,
																 const vk::PlanarFormatDescription&	formatDescription,
																 const vk::VkImageFormatProperties& imageFormatProperties,
																 const vk::VkExtent3D&				extent);

deUint32						getPlaneSizeInBytes				(const PlanarFormatDescription&	formatInfo,
																 const VkExtent3D&				baseExtents,
																 const deUint32					planeNdx,
																 const deUint32					mipmapLevel,
																 const deUint32					mipmapMemoryAlignment);
deUint32						getPlaneSizeInBytes				(const PlanarFormatDescription&	formatInfo,
																 const tcu::UVec2&				baseExtents,
																 const deUint32					planeNdx,
																 const deUint32					mipmapLevel,
																 const deUint32					mipmapMemoryAlignment);
VkExtent3D						getPlaneExtent					(const PlanarFormatDescription&	formatInfo,
																 const VkExtent3D&				baseExtents,
																 const deUint32					planeNdx,
																 const deUint32					mipmapLevel);
tcu::UVec2						getPlaneExtent					(const PlanarFormatDescription&	formatInfo,
																 const tcu::UVec2&				baseExtents,
																 const deUint32					planeNdx,
																 const deUint32					mipmapLevel);
tcu::UVec3						getImageSizeAlignment			(VkFormat						format);
tcu::UVec3						getImageSizeAlignment			(const PlanarFormatDescription&	formatInfo);
tcu::UVec2						getBlockExtent					(VkFormat						format);
tcu::UVec2						getBlockExtent					(const PlanarFormatDescription&	formatInfo);
VkFormat						getPlaneCompatibleFormat		(VkFormat						format,
																 deUint32						planeNdx);
VkFormat						getPlaneCompatibleFormat		(const PlanarFormatDescription&	formatInfo,
																 deUint32						planeNdx);

VkImageAspectFlagBits			getPlaneAspect					(deUint32						planeNdx);
deUint32						getAspectPlaneNdx				(VkImageAspectFlagBits			planeAspect);
bool							isChromaSubsampled				(VkFormat						format);
bool							isYCbCr422Format				(VkFormat						format);
bool							isYCbCr420Format				(VkFormat						format);

tcu::PixelBufferAccess			getChannelAccess				(const PlanarFormatDescription&	formatInfo,
																 const tcu::UVec2&				size,
																 const deUint32*				planeRowPitches,
																 void* const*					planePtrs,
																 deUint32						channelNdx);
tcu::ConstPixelBufferAccess		getChannelAccess				(const PlanarFormatDescription&	formatInfo,
																 const tcu::UVec2&				size,
																 const deUint32*				planeRowPitches,
																 const void* const*				planePtrs,
																 deUint32						channelNdx);
tcu::PixelBufferAccess			getChannelAccess				(const PlanarFormatDescription&	formatInfo,
																 const tcu::UVec3&				size,
																 const deUint32*				planeRowPitches,
																 void* const*					planePtrs,
																 deUint32						channelNdx);
tcu::ConstPixelBufferAccess		getChannelAccess				(const PlanarFormatDescription&	formatInfo,
																 const tcu::UVec3&				size,
																 const deUint32*				planeRowPitches,
																 const void* const*				planePtrs,
																 deUint32						channelNdx);
VkImageAspectFlags				getImageAspectFlags				(const tcu::TextureFormat		textureFormat);
VkExtent3D						mipLevelExtents					(const VkExtent3D&				baseExtents,
																 const deUint32					mipLevel);
tcu::UVec3						alignedDivide					(const VkExtent3D&				extent,
																 const VkExtent3D&				divisor);

/*--------------------------------------------------------------------*//*!
 * Copies buffer data into an image. The buffer is expected to be
 * in a state after host write.
*//*--------------------------------------------------------------------*/
void	copyBufferToImage						(const DeviceInterface&							vk,
												 vk::VkDevice									device,
												 vk::VkQueue									queue,
												 deUint32										queueFamilyIndex,
												 const vk::VkBuffer&							buffer,
												 vk::VkDeviceSize								bufferSize,
												 const std::vector<vk::VkBufferImageCopy>&		copyRegions,
												 const vk::VkSemaphore*							waitSemaphore,
												 vk::VkImageAspectFlags							imageAspectFlags,
												 deUint32										mipLevels,
												 deUint32										arrayLayers,
												 vk::VkImage									destImage,
												 VkImageLayout									destImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
												 VkPipelineStageFlags							destImageDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

void	copyBufferToImage						(const DeviceInterface&							vk,
												 const VkCommandBuffer&							cmdBuffer,
												 const VkBuffer&								buffer,
												 vk::VkDeviceSize								bufferSize,
												 const std::vector<VkBufferImageCopy>&			copyRegions,
												 VkImageAspectFlags								imageAspectFlags,
												 deUint32										mipLevels,
												 deUint32										arrayLayers,
												 VkImage										destImage,
												 VkImageLayout									destImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
												 VkPipelineStageFlags							destImageDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

/*--------------------------------------------------------------------*//*!
 * Copies image data into a buffer. The buffer is expected to be
 * read by the host.
*//*--------------------------------------------------------------------*/
void	copyImageToBuffer						(const DeviceInterface&							vk,
												 vk::VkCommandBuffer							cmdBuffer,
												 vk::VkImage									image,
												 vk::VkBuffer									buffer,
												 tcu::IVec2										size,
												 vk::VkAccessFlags								srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
												 vk::VkImageLayout								oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
												 deUint32										numLayers = 1u,
												 VkImageAspectFlags								barrierAspect = VK_IMAGE_ASPECT_COLOR_BIT,
												 VkImageAspectFlags								copyAspect = VK_IMAGE_ASPECT_COLOR_BIT);

/*--------------------------------------------------------------------*//*!
 * Clear a color image
*//*--------------------------------------------------------------------*/
void	clearColorImage							(const DeviceInterface&							vk,
												 const vk::VkDevice								device,
												 const vk::VkQueue								queue,
												 deUint32										queueFamilyIndex,
												 vk::VkImage									image,
												 tcu::Vec4										clearColor,
												 vk::VkImageLayout								oldLayout,
												 vk::VkImageLayout								newLayout,
												 vk::VkPipelineStageFlags						dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Initialize color image with a chessboard pattern
*//*--------------------------------------------------------------------*/
void	initColorImageChessboardPattern			(const DeviceInterface&							vk,
												 const vk::VkDevice								device,
												 const vk::VkQueue								queue,
												 deUint32										queueFamilyIndex,
												 Allocator&										allocator,
												 vk::VkImage									image,
												 vk::VkFormat									format,
												 tcu::Vec4										colorValue0,
												 tcu::Vec4										colorValue1,
												 deUint32										imageWidth,
												 deUint32										imageHeight,
												 deUint32										tileSize,
												 vk::VkImageLayout								oldLayout,
												 vk::VkImageLayout								newLayout,
												 vk::VkPipelineStageFlags						dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Copies depth/stencil image data into two separate buffers.
 * The buffers are expected to be read by the host.
*//*--------------------------------------------------------------------*/
void	copyDepthStencilImageToBuffers			(const DeviceInterface&							vk,
												 vk::VkCommandBuffer							cmdBuffer,
												 vk::VkImage									image,
												 vk::VkBuffer									depthBuffer,
												 vk::VkBuffer									stencilBuffer,
												 tcu::IVec2										size,
												 vk::VkAccessFlags								srcAccessMask,
												 vk::VkImageLayout								oldLayout,
												 deUint32										numLayers = 1u);

/*--------------------------------------------------------------------*//*!
 * Clear a depth/stencil image
*//*--------------------------------------------------------------------*/
void	clearDepthStencilImage					(const DeviceInterface&							vk,
												 const vk::VkDevice								device,
												 const vk::VkQueue								queue,
												 deUint32										queueFamilyIndex,
												 vk::VkImage									image,
												 float											depthValue,
												 deUint32										stencilValue,
												 vk::VkImageLayout								oldLayout,
												 vk::VkImageLayout								newLayout,
												 vk::VkPipelineStageFlags						dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Initialize depth and stencil channels with a chessboard pattern
*//*--------------------------------------------------------------------*/
void	initDepthStencilImageChessboardPattern	(const DeviceInterface&							vk,
												 const vk::VkDevice								device,
												 const vk::VkQueue								queue,
												 deUint32										queueFamilyIndex,
												 Allocator&										allocator,
												 vk::VkImage									image,
												 vk::VkFormat									format,
												 float											depthValue0,
												 float											depthValue1,
												 deUint32										stencilValue0,
												 deUint32										stencilValue1,
												 deUint32										imageWidth,
												 deUint32										imageHeight,
												 deUint32										tileSize,
												 vk::VkImageLayout								oldLayout,
												 vk::VkImageLayout								newLayout,
												 vk::VkPipelineStageFlags						dstStageFlags);

/*--------------------------------------------------------------------*//*!
 * Checks if the physical device supports creation of the specified
 * image format.
 *//*--------------------------------------------------------------------*/
bool	checkSparseImageFormatSupport			(const VkPhysicalDevice							physicalDevice,
												 const InstanceInterface&						instance,
												 const VkFormat									format,
												 const VkImageType								imageType,
												 const VkSampleCountFlagBits					sampleCount,
												 const VkImageUsageFlags						usageFlags,
												 const VkImageTiling							imageTiling);

bool	checkSparseImageFormatSupport			(const vk::VkPhysicalDevice						physicalDevice,
												 const vk::InstanceInterface&					instance,
												 const vk::VkImageCreateInfo&					imageCreateInfo);

/*--------------------------------------------------------------------*//*!
 * Allocates memory for a sparse image and handles the memory binding.
 *//*--------------------------------------------------------------------*/
void	allocateAndBindSparseImage				(const vk::DeviceInterface&						vk,
												 vk::VkDevice									device,
												 const vk::VkPhysicalDevice						physicalDevice,
												 const vk::InstanceInterface&					instance,
												 const vk::VkImageCreateInfo&					imageCreateInfo,
												 const vk::VkSemaphore&							signalSemaphore,
												 vk::VkQueue									queue,
												 vk::Allocator&									allocator,
												 std::vector<de::SharedPtr<vk::Allocation> >&	allocations,
												 tcu::TextureFormat								format,
												 vk::VkImage									destImage);

} // vk

#endif // _VKIMAGEUTIL_HPP
