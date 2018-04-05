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
bool						isUnormFormat				(VkFormat format);
bool						isSnormFormat				(VkFormat format);
bool						isIntFormat					(VkFormat format);
bool						isUintFormat				(VkFormat format);
bool						isDepthStencilFormat		(VkFormat format);
bool						isCompressedFormat			(VkFormat format);
bool						isSrgbFormat				(VkFormat format);

bool						isSupportedByFramework		(VkFormat format);

tcu::TextureFormat			mapVkFormat					(VkFormat format);
tcu::CompressedTexFormat	mapVkCompressedFormat		(VkFormat format);
tcu::TextureFormat			getDepthCopyFormat			(VkFormat combinedFormat);
tcu::TextureFormat			getStencilCopyFormat		(VkFormat combinedFormat);

tcu::Sampler				mapVkSampler				(const VkSamplerCreateInfo& samplerCreateInfo);
tcu::Sampler::CompareMode	mapVkSamplerCompareOp		(VkCompareOp compareOp);
tcu::Sampler::WrapMode		mapVkSamplerAddressMode		(VkSamplerAddressMode addressMode);
tcu::Sampler::ReductionMode mapVkSamplerReductionMode	(VkSamplerReductionModeEXT reductionMode);
tcu::Sampler::FilterMode	mapVkMinTexFilter			(VkFilter filter, VkSamplerMipmapMode mipMode);
tcu::Sampler::FilterMode	mapVkMagTexFilter			(VkFilter filter);

VkFilter					mapFilterMode				(tcu::Sampler::FilterMode filterMode);
VkSamplerMipmapMode			mapMipmapMode				(tcu::Sampler::FilterMode filterMode);
VkSamplerAddressMode		mapWrapMode					(tcu::Sampler::WrapMode wrapMode);
VkCompareOp					mapCompareMode				(tcu::Sampler::CompareMode mode);
VkFormat					mapTextureFormat			(const tcu::TextureFormat& format);
VkFormat					mapCompressedTextureFormat	(const tcu::CompressedTexFormat format);
VkSamplerCreateInfo			mapSampler					(const tcu::Sampler& sampler, const tcu::TextureFormat& format, float minLod = 0.0f, float maxLod = 1000.0f);

void						imageUtilSelfTest			(void);

deUint32					getBlockSizeInBytes			(const VkFormat compressedFormat);
deUint32					getBlockWidth				(const VkFormat compressedFormat);
deUint32					getBlockHeight				(const VkFormat compressedFormat);

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
	Plane		planes[MAX_PLANES];
	Channel		channels[MAX_CHANNELS];

	inline bool hasChannelNdx (deUint32 ndx) const
	{
		DE_ASSERT(de::inBounds(ndx, 0u, 4u));
		return (presentChannels & (1u<<ndx)) != 0;
	}
};

bool							isYCbCrFormat					(VkFormat format);
PlanarFormatDescription			getPlanarFormatDescription		(VkFormat format);
const PlanarFormatDescription&	getYCbCrPlanarFormatDescription	(VkFormat format);
int								getPlaneCount					(VkFormat format);
VkImageAspectFlagBits			getPlaneAspect					(deUint32 planeNdx);
deUint32						getAspectPlaneNdx				(VkImageAspectFlagBits planeAspect);
bool							isChromaSubsampled				(VkFormat format);

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
VkImageAspectFlags				getImageAspectFlags				(const tcu::TextureFormat		textureFormat);
VkExtent3D						mipLevelExtents					(const VkExtent3D&				baseExtents,
																 const deUint32					mipLevel);
tcu::UVec3						alignedDivide					(const VkExtent3D&				extent,
																 const VkExtent3D&				divisor);

/*--------------------------------------------------------------------*//*!
 * Copies buffer data into an image.
*//*--------------------------------------------------------------------*/
void							copyBufferToImage				(const DeviceInterface&						vk,
																 vk::VkDevice								device,
																 vk::VkQueue								queue,
																 deUint32									queueFamilyIndex,
																 const vk::VkBuffer&						buffer,
																 deUint32									bufferSize,
																 const std::vector<vk::VkBufferImageCopy>&	copyRegions,
																 const vk::VkSemaphore*						waitSemaphore,
																 vk::VkImageAspectFlags						imageAspectFlags,
																 deUint32									mipLevels,
																 deUint32									arrayLayers,
																 vk::VkImage								destImage,
																 VkImageLayout								destImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

/*--------------------------------------------------------------------*//*!
 * Checks if the physical device supports creation of the specified
 * image format.
 *//*--------------------------------------------------------------------*/
bool							checkSparseImageFormatSupport	(const vk::VkPhysicalDevice		physicalDevice,
																 const vk::InstanceInterface&	instance,
																 const vk::VkImageCreateInfo&	imageCreateInfo);

/*--------------------------------------------------------------------*//*!
 * Allocates memory for a sparse image and handles the memory binding.
 *//*--------------------------------------------------------------------*/
void							allocateAndBindSparseImage		(const vk::DeviceInterface&						vk,
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
