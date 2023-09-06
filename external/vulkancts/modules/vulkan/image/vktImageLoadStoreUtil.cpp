/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Image load/store utilities
 *//*--------------------------------------------------------------------*/

#include "deMath.h"
#include "tcuTextureUtil.hpp"
#include "vktImageLoadStoreUtil.hpp"
#include "vkQueryUtil.hpp"

using namespace vk;

namespace vkt
{
namespace image
{

float computeStoreColorScale (const vk::VkFormat format, const tcu::IVec3 imageSize)
{
	const int maxImageDimension = de::max(imageSize.x(), de::max(imageSize.y(), imageSize.z()));
	const float div = static_cast<float>(maxImageDimension - 1);

	if (isUnormFormat(format))
		return 1.0f / div;
	else if (isSnormFormat(format))
		return 2.0f / div;
	else
		return 1.0f;
}

ImageType getImageTypeForSingleLayer (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_1D_ARRAY:
			return IMAGE_TYPE_1D;

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			// A single layer for cube is a 2d face
			return IMAGE_TYPE_2D;

		case IMAGE_TYPE_3D:
			return IMAGE_TYPE_3D;

		case IMAGE_TYPE_BUFFER:
			return IMAGE_TYPE_BUFFER;

		default:
			DE_FATAL("Internal test error");
			return IMAGE_TYPE_LAST;
	}
}

VkImageCreateInfo makeImageCreateInfo (const Texture& texture, const VkFormat format, const VkImageUsageFlags usage, const VkImageCreateFlags flags, const VkImageTiling tiling)
{
	const VkSampleCountFlagBits samples = static_cast<VkSampleCountFlagBits>(texture.numSamples());	// integer and bit mask are aligned, so we can cast like this

	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,														// VkStructureType			sType;
		DE_NULL,																					// const void*				pNext;
		(isCube(texture) ? (VkImageCreateFlags)VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) | flags,	// VkImageCreateFlags		flags;
		mapImageType(texture.type()),																// VkImageType				imageType;
		format,																						// VkFormat					format;
		makeExtent3D(texture.layerSize()),															// VkExtent3D				extent;
		1u,																							// deUint32					mipLevels;
		(deUint32)texture.numLayers(),																// deUint32					arrayLayers;
		samples,																					// VkSampleCountFlagBits	samples;
		tiling,																						// VkImageTiling			tiling;
		usage,																						// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,																	// VkSharingMode			sharingMode;
		0u,																							// deUint32					queueFamilyIndexCount;
		DE_NULL,																					// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,																	// VkImageLayout			initialLayout;
	};
	return imageParams;
}


//! Minimum chunk size is determined by the offset alignment requirements.
VkDeviceSize getOptimalUniformBufferChunkSize (const InstanceInterface& vki, const VkPhysicalDevice physDevice, VkDeviceSize minimumRequiredChunkSizeBytes)
{
	const VkPhysicalDeviceProperties properties = getPhysicalDeviceProperties(vki, physDevice);
	const VkDeviceSize alignment = properties.limits.minUniformBufferOffsetAlignment;

	if (minimumRequiredChunkSizeBytes > alignment)
		return alignment + (minimumRequiredChunkSizeBytes / alignment) * alignment;
	else
		return alignment;
}

bool isRepresentableIntegerValue (tcu::Vector<deInt64, 4> value, tcu::TextureFormat format)
{
	const tcu::IVec4	formatBitDepths	= tcu::getTextureFormatBitDepth(format);
	const deUint32		numChannels		= getNumUsedChannels(mapTextureFormat(format));

	switch (tcu::getTextureChannelClass(format.type))
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		{
			for (deUint32 compNdx = 0; compNdx < numChannels; compNdx++)
			{
				if (deFloorToInt32(log2((double)value[compNdx]) + 1) > formatBitDepths[compNdx])
					return false;
			}

			break;
		}

		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		{
			for (deUint32 compNdx = 0; compNdx < numChannels; compNdx++)
			{
				if ((deFloorToInt32(log2((double)deAbs64(value[compNdx])) + 1) + 1) > formatBitDepths[compNdx])
					return false;
			}

			break;
		}

		default:
			DE_ASSERT(isIntegerFormat(mapTextureFormat(format)));
	}

	return true;
}

} // image
} // vkt
