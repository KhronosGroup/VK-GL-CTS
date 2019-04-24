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
 *//*
 * \file  vktSparseResourcesShaderIntrinsicsBase.cpp
 * \brief Sparse Resources Shader Intrinsics Base Classes
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesShaderIntrinsicsBase.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{

std::string getOpTypeImageComponent (const tcu::TextureFormat& format)
{
	switch (tcu::getTextureChannelClass(format.type))
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "OpTypeInt 32 0";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "OpTypeInt 32 1";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "OpTypeFloat 32";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getOpTypeImageComponent (const vk::PlanarFormatDescription& description)
{
	switch (description.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "OpTypeInt 32 0";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "OpTypeInt 32 1";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "OpTypeFloat 32";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getImageComponentTypeName (const tcu::TextureFormat& format)
{
	switch (tcu::getTextureChannelClass(format.type))
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "%type_uint";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "%type_int";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "%type_float";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getImageComponentTypeName (const vk::PlanarFormatDescription& description)
{
	switch (description.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "%type_uint";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "%type_int";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "%type_float";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getImageComponentVec4TypeName (const tcu::TextureFormat& format)
{
	switch (tcu::getTextureChannelClass(format.type))
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "%type_uvec4";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "%type_ivec4";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "%type_vec4";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getImageComponentVec4TypeName (const vk::PlanarFormatDescription& description)
{
	switch (description.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "%type_uvec4";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "%type_ivec4";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "%type_vec4";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getOpTypeImageSparse (const ImageType			imageType,
								  const tcu::TextureFormat&	format,
								  const std::string&		componentType,
								  const bool				requiresSampler)
{
	std::ostringstream	src;

	src << "OpTypeImage " << componentType << " ";

	switch (imageType)
	{
		case IMAGE_TYPE_1D :
			src << "1D 0 0 0 ";
		break;
		case IMAGE_TYPE_1D_ARRAY :
			src << "1D 0 1 0 ";
		break;
		case IMAGE_TYPE_2D :
			src << "2D 0 0 0 ";
		break;
		case IMAGE_TYPE_2D_ARRAY :
			src << "2D 0 1 0 ";
		break;
		case IMAGE_TYPE_3D :
			src << "3D 0 0 0 ";
		break;
		case IMAGE_TYPE_CUBE :
			src << "Cube 0 0 0 ";
		break;
		case IMAGE_TYPE_CUBE_ARRAY :
			src << "Cube 0 1 0 ";
		break;
		default :
			DE_FATAL("Unexpected image type");
		break;
	};

	if (requiresSampler)
		src << "1 ";
	else
		src << "2 ";

	switch (format.order)
	{
		case tcu::TextureFormat::R:
			src << "R";
		break;
		case tcu::TextureFormat::RG:
			src << "Rg";
			break;
		case tcu::TextureFormat::RGB:
			src << "Rgb";
			break;
		case tcu::TextureFormat::RGBA:
			src << "Rgba";
		break;
		default:
			DE_FATAL("Unexpected channel order");
		break;
	}

	switch (format.type)
	{
		case tcu::TextureFormat::SIGNED_INT8:
			src << "8i";
		break;
		case tcu::TextureFormat::SIGNED_INT16:
			src << "16i";
		break;
		case tcu::TextureFormat::SIGNED_INT32:
			src << "32i";
		break;
		case tcu::TextureFormat::UNSIGNED_INT8:
			src << "8ui";
		break;
		case tcu::TextureFormat::UNSIGNED_INT16:
			src << "16ui";
		break;
		case tcu::TextureFormat::UNSIGNED_INT32:
			src << "32ui";
		break;
		case tcu::TextureFormat::SNORM_INT8:
			src << "8Snorm";
		break;
		case tcu::TextureFormat::SNORM_INT16:
			src << "16Snorm";
		break;
		case tcu::TextureFormat::SNORM_INT32:
			src << "32Snorm";
		break;
		case tcu::TextureFormat::UNORM_INT8:
			src << "8";
		break;
		case tcu::TextureFormat::UNORM_INT16:
			src << "16";
		break;
		case tcu::TextureFormat::UNORM_INT32:
			src << "32";
		break;
		default:
			DE_FATAL("Unexpected channel type");
		break;
	};

	return src.str();
}

std::string getOpTypeImageSparse (const ImageType		imageType,
								  const VkFormat		format,
								  const std::string&	componentType,
								  const bool			requiresSampler)
{
	std::ostringstream	src;

	src << "OpTypeImage " << componentType << " ";

	switch (imageType)
	{
		case IMAGE_TYPE_1D :
			src << "1D 0 0 0 ";
		break;
		case IMAGE_TYPE_1D_ARRAY :
			src << "1D 0 1 0 ";
		break;
		case IMAGE_TYPE_2D :
			src << "2D 0 0 0 ";
		break;
		case IMAGE_TYPE_2D_ARRAY :
			src << "2D 0 1 0 ";
		break;
		case IMAGE_TYPE_3D :
			src << "3D 0 0 0 ";
		break;
		case IMAGE_TYPE_CUBE :
			src << "Cube 0 0 0 ";
		break;
		case IMAGE_TYPE_CUBE_ARRAY :
			src << "Cube 0 1 0 ";
		break;
		default :
			DE_FATAL("Unexpected image type");
		break;
	};

	if (requiresSampler)
		src << "1 ";
	else
		src << "2 ";

	switch (format)
	{
		case VK_FORMAT_R8_SINT:										src <<	"R8i";			break;
		case VK_FORMAT_R16_SINT:									src <<	"R16i";			break;
		case VK_FORMAT_R32_SINT:									src <<	"R32i";			break;
		case VK_FORMAT_R8_UINT:										src <<	"R8ui";			break;
		case VK_FORMAT_R16_UINT:									src <<	"R16ui";		break;
		case VK_FORMAT_R32_UINT:									src <<	"R32ui";		break;
		case VK_FORMAT_R8_SNORM:									src <<	"R8Snorm";		break;
		case VK_FORMAT_R16_SNORM:									src <<	"R16Snorm";		break;
		case VK_FORMAT_R8_UNORM:									src <<	"R8";			break;
		case VK_FORMAT_R16_UNORM:									src <<	"R16";			break;

		case VK_FORMAT_R8G8_SINT:									src <<	"Rg8i";			break;
		case VK_FORMAT_R16G16_SINT:									src <<	"Rg16i";		break;
		case VK_FORMAT_R32G32_SINT:									src <<	"Rg32i";		break;
		case VK_FORMAT_R8G8_UINT:									src <<	"Rg8ui";		break;
		case VK_FORMAT_R16G16_UINT:									src <<	"Rg16ui";		break;
		case VK_FORMAT_R32G32_UINT:									src <<	"Rg32ui";		break;
		case VK_FORMAT_R8G8_SNORM:									src <<	"Rg8Snorm";		break;
		case VK_FORMAT_R16G16_SNORM:								src <<	"Rg16Snorm";	break;
		case VK_FORMAT_R8G8_UNORM:									src <<	"Rg8";			break;
		case VK_FORMAT_R16G16_UNORM:								src <<	"Rg16";			break;

		case VK_FORMAT_R8G8B8A8_SINT:								src <<	"Rgba8i";		break;
		case VK_FORMAT_R16G16B16A16_SINT:							src <<	"Rgba16i";		break;
		case VK_FORMAT_R32G32B32A32_SINT:							src <<	"Rgba32i";		break;
		case VK_FORMAT_R8G8B8A8_UINT:								src <<	"Rgba8ui";		break;
		case VK_FORMAT_R16G16B16A16_UINT:							src <<	"Rgba16ui";		break;
		case VK_FORMAT_R32G32B32A32_UINT:							src <<	"Rgba32ui";		break;
		case VK_FORMAT_R8G8B8A8_SNORM:								src <<	"Rgba8Snorm";	break;
		case VK_FORMAT_R16G16B16A16_SNORM:							src <<	"Rgba16Snorm";	break;
		case VK_FORMAT_R8G8B8A8_UNORM:								src <<	"Rgba8";		break;
		case VK_FORMAT_R16G16B16A16_UNORM:							src <<	"Rgba16";		break;

		case VK_FORMAT_G8B8G8R8_422_UNORM:							src <<	"Rgba8";		break;
		case VK_FORMAT_B8G8R8G8_422_UNORM:							src <<	"Rgba8";		break;
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:					src <<	"Rgba8";		break;
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:					src <<	"Rgba8";		break;
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:					src <<	"Rgba8";		break;
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:					src <<	"Rgba8";		break;
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:					src <<	"Rgba8";		break;
		case VK_FORMAT_R10X6_UNORM_PACK16:							src <<	"R16";			break;
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:					src <<	"Rg16";			break;
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:			src <<	"Rgba16";		break;
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:		src <<	"Rgba16";		break;
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:		src <<	"Rgba16";		break;
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_R12X4_UNORM_PACK16:							src <<	"R16";			break;
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:					src <<	"Rg16";			break;
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:			src <<	"Rgba16";		break;
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:		src <<	"Rgba16";		break;
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:		src <<	"Rgba16";		break;
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:	src <<	"Rgba16";		break;
		case VK_FORMAT_G16B16G16R16_422_UNORM:						src <<	"Rgba16";		break;
		case VK_FORMAT_B16G16R16G16_422_UNORM:						src <<	"Rgba16";		break;
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:				src <<	"Rgba16";		break;
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:					src <<	"Rgba16";		break;
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:				src <<	"Rgba16";		break;
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:					src <<	"Rgba16";		break;
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:				src <<	"Rgba16";		break;

		default:
			DE_FATAL("Unexpected texture format");
			break;
	}
	return src.str();
}


std::string getOpTypeImageResidency (const ImageType imageType)
{
	std::ostringstream	src;

	src << "OpTypeImage %type_uint ";

	switch (imageType)
	{
		case IMAGE_TYPE_1D :
			src << "1D 0 0 0 2 R32ui";
		break;
		case IMAGE_TYPE_1D_ARRAY :
			src << "1D 0 1 0 2 R32ui";
		break;
		case IMAGE_TYPE_2D :
			src << "2D 0 0 0 2 R32ui";
		break;
		case IMAGE_TYPE_2D_ARRAY :
			src << "2D 0 1 0 2 R32ui";
		break;
		case IMAGE_TYPE_3D :
			src << "3D 0 0 0 2 R32ui";
		break;
		case IMAGE_TYPE_CUBE :
			src << "Cube 0 0 0 2 R32ui";
		break;
		case IMAGE_TYPE_CUBE_ARRAY :
			src << "Cube 0 1 0 2 R32ui";
		break;
		default :
			DE_FATAL("Unexpected image type");
		break;
	};

	return src.str();
}

tcu::TestStatus SparseShaderIntrinsicsInstanceBase::iterate (void)
{
	const InstanceInterface&			instance				= m_context.getInstanceInterface();
	const VkPhysicalDevice				physicalDevice			= m_context.getPhysicalDevice();
	VkImageCreateInfo					imageSparseInfo;
	VkImageCreateInfo					imageTexelsInfo;
	VkImageCreateInfo					imageResidencyInfo;
	std::vector <deUint32>				residencyReferenceData;
	std::vector<DeviceMemorySp>			deviceMemUniquePtrVec;
	const PlanarFormatDescription		formatDescription		= getPlanarFormatDescription(m_format);

	imageSparseInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageSparseInfo.pNext					= DE_NULL;
	imageSparseInfo.flags					= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
	imageSparseInfo.imageType				= mapImageType(m_imageType);
	imageSparseInfo.format					= m_format;
	imageSparseInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageSize));
	imageSparseInfo.arrayLayers				= getNumLayers(m_imageType, m_imageSize);
	imageSparseInfo.samples					= VK_SAMPLE_COUNT_1_BIT;
	imageSparseInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	imageSparseInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imageSparseInfo.usage					= VK_IMAGE_USAGE_TRANSFER_DST_BIT | imageSparseUsageFlags();
	imageSparseInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imageSparseInfo.queueFamilyIndexCount	= 0u;
	imageSparseInfo.pQueueFamilyIndices		= DE_NULL;

	if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
	{
		imageSparseInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	// Check if device supports sparse operations for image format
	if (!checkSparseSupportForImageFormat(instance, physicalDevice, imageSparseInfo))
		TCU_THROW(NotSupportedError, "The image format does not support sparse operations");

	{
		// Assign maximum allowed mipmap levels to image
		VkImageFormatProperties imageFormatProperties;
		if (instance.getPhysicalDeviceImageFormatProperties(physicalDevice,
			imageSparseInfo.format,
			imageSparseInfo.imageType,
			imageSparseInfo.tiling,
			imageSparseInfo.usage,
			imageSparseInfo.flags,
			&imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		{
			TCU_THROW(NotSupportedError, "Image format does not support sparse operations");
		}

		imageSparseInfo.mipLevels = getMipmapCount(m_format, formatDescription, imageFormatProperties, imageSparseInfo.extent);
	}

	{
		// Create logical device supporting both sparse and compute/graphics queues
		QueueRequirementsVec queueRequirements;
		queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
		queueRequirements.push_back(QueueRequirements(getQueueFlags(), 1u));

		createDeviceSupportingQueues(queueRequirements);
	}

	// Create queues supporting sparse binding operations and compute/graphics operations
	const DeviceInterface&			deviceInterface		= getDeviceInterface();
	const Queue&					sparseQueue			= getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
	const Queue&					extractQueue		= getQueue(getQueueFlags(), 0);

	// Create sparse image
	const Unique<VkImage> imageSparse(createImage(deviceInterface, getDevice(), &imageSparseInfo));

	// Create sparse image memory bind semaphore
	const Unique<VkSemaphore> memoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

	std::vector<VkSparseImageMemoryRequirements> sparseMemoryRequirements;

	deUint32	imageSparseSizeInBytes	= 0;
	deUint32	imageSizeInPixels		= 0;

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
		{
			imageSparseSizeInBytes	+= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
			imageSizeInPixels		+= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx) / formatDescription.planes[planeNdx].elementSizeBytes;
		}
	}

	residencyReferenceData.assign(imageSizeInPixels, MEMORY_BLOCK_NOT_BOUND_VALUE);

	{
		// Get sparse image general memory requirements
		const VkMemoryRequirements				imageMemoryRequirements	= getImageMemoryRequirements(deviceInterface, getDevice(), *imageSparse);

		// Check if required image memory size does not exceed device limits
		if (imageMemoryRequirements.size > getPhysicalDeviceProperties(instance, physicalDevice).limits.sparseAddressSpaceSize)
			TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

		DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

		const deUint32							memoryType				= findMatchingMemoryType(instance, physicalDevice, imageMemoryRequirements, MemoryRequirement::Any);

		if (memoryType == NO_MATCH_FOUND)
			return tcu::TestStatus::fail("No matching memory type found");

		// Get sparse image sparse memory requirements
		sparseMemoryRequirements = getImageSparseMemoryRequirements(deviceInterface, getDevice(), *imageSparse);

		DE_ASSERT(sparseMemoryRequirements.size() != 0);

		const deUint32							metadataAspectIndex		= getSparseAspectRequirementsIndex(sparseMemoryRequirements, VK_IMAGE_ASPECT_METADATA_BIT);
		deUint32								pixelOffset				= 0u;
		std::vector<VkSparseImageMemoryBind>	imageResidencyMemoryBinds;
		std::vector<VkSparseMemoryBind>			imageMipTailBinds;

		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags		aspect				= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
			const deUint32					aspectIndex			= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

			if (aspectIndex == NO_MATCH_FOUND)
				TCU_THROW(NotSupportedError, "Not supported image aspect");

			VkSparseImageMemoryRequirements	aspectRequirements	= sparseMemoryRequirements[aspectIndex];

			DE_ASSERT((aspectRequirements.imageMipTailSize % imageMemoryRequirements.alignment) == 0);

			VkExtent3D						imageGranularity	= aspectRequirements.formatProperties.imageGranularity;

			// Bind memory for each mipmap level
			for (deUint32 mipmapNdx = 0; mipmapNdx < aspectRequirements.imageMipTailFirstLod; ++mipmapNdx)
			{
				const deUint32 mipLevelSizeInPixels = getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx) / formatDescription.planes[planeNdx].elementSizeBytes;

				if (mipmapNdx % MEMORY_BLOCK_TYPE_COUNT == MEMORY_BLOCK_NOT_BOUND)
				{
					pixelOffset += mipLevelSizeInPixels;
					continue;
				}

				for (deUint32 pixelNdx = 0u; pixelNdx < mipLevelSizeInPixels; ++pixelNdx)
				{
					residencyReferenceData[pixelOffset + pixelNdx] = MEMORY_BLOCK_BOUND_VALUE;
				}

				pixelOffset += mipLevelSizeInPixels;

				for (deUint32 layerNdx = 0; layerNdx < imageSparseInfo.arrayLayers; ++layerNdx)
				{
					const VkExtent3D			mipExtent		= getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, mipmapNdx);
					const tcu::UVec3			sparseBlocks	= alignedDivide(mipExtent, imageGranularity);
					const deUint32				numSparseBlocks	= sparseBlocks.x() * sparseBlocks.y() * sparseBlocks.z();
					const VkImageSubresource	subresource		= { aspect, mipmapNdx, layerNdx };

					const VkSparseImageMemoryBind imageMemoryBind = makeSparseImageMemoryBind(deviceInterface, getDevice(),
						imageMemoryRequirements.alignment * numSparseBlocks, memoryType, subresource, makeOffset3D(0u, 0u, 0u), mipExtent);

					deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

					imageResidencyMemoryBinds.push_back(imageMemoryBind);
				}
			}

			if (aspectRequirements.imageMipTailFirstLod < imageSparseInfo.mipLevels)
			{
				if (aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT)
				{
					const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
						aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset);

					deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

					imageMipTailBinds.push_back(imageMipTailMemoryBind);
				}
				else
				{
					for (deUint32 layerNdx = 0; layerNdx < imageSparseInfo.arrayLayers; ++layerNdx)
					{
						const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
							aspectRequirements.imageMipTailSize, memoryType, aspectRequirements.imageMipTailOffset + layerNdx * aspectRequirements.imageMipTailStride);

						deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

						imageMipTailBinds.push_back(imageMipTailMemoryBind);
					}
				}

				for (deUint32 pixelNdx = pixelOffset; pixelNdx < residencyReferenceData.size(); ++pixelNdx)
				{
					residencyReferenceData[pixelNdx] = MEMORY_BLOCK_BOUND_VALUE;
				}
			}
		}

		// Metadata
		if (metadataAspectIndex != NO_MATCH_FOUND)
		{
			const VkSparseImageMemoryRequirements metadataAspectRequirements = sparseMemoryRequirements[metadataAspectIndex];

			const deUint32 metadataBindCount = (metadataAspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT ? 1u : imageSparseInfo.arrayLayers);
			for (deUint32 bindNdx = 0u; bindNdx < metadataBindCount; ++bindNdx)
			{
				const VkSparseMemoryBind imageMipTailMemoryBind = makeSparseMemoryBind(deviceInterface, getDevice(),
					metadataAspectRequirements.imageMipTailSize, memoryType,
					metadataAspectRequirements.imageMipTailOffset + bindNdx * metadataAspectRequirements.imageMipTailStride,
					VK_SPARSE_MEMORY_BIND_METADATA_BIT);

				deviceMemUniquePtrVec.push_back(makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(imageMipTailMemoryBind.memory), Deleter<VkDeviceMemory>(deviceInterface, getDevice(), DE_NULL))));

				imageMipTailBinds.push_back(imageMipTailMemoryBind);
			}
		}

		VkBindSparseInfo bindSparseInfo =
		{
			VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,	//VkStructureType							sType;
			DE_NULL,							//const void*								pNext;
			0u,									//deUint32									waitSemaphoreCount;
			DE_NULL,							//const VkSemaphore*						pWaitSemaphores;
			0u,									//deUint32									bufferBindCount;
			DE_NULL,							//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
			0u,									//deUint32									imageOpaqueBindCount;
			DE_NULL,							//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
			0u,									//deUint32									imageBindCount;
			DE_NULL,							//const VkSparseImageMemoryBindInfo*		pImageBinds;
			1u,									//deUint32									signalSemaphoreCount;
			&memoryBindSemaphore.get()			//const VkSemaphore*						pSignalSemaphores;
		};

		VkSparseImageMemoryBindInfo			imageResidencyBindInfo;
		VkSparseImageOpaqueMemoryBindInfo	imageMipTailBindInfo;

		if (imageResidencyMemoryBinds.size() > 0)
		{
			imageResidencyBindInfo.image		= *imageSparse;
			imageResidencyBindInfo.bindCount	= static_cast<deUint32>(imageResidencyMemoryBinds.size());
			imageResidencyBindInfo.pBinds		= imageResidencyMemoryBinds.data();

			bindSparseInfo.imageBindCount		= 1u;
			bindSparseInfo.pImageBinds			= &imageResidencyBindInfo;
		}

		if (imageMipTailBinds.size() > 0)
		{
			imageMipTailBindInfo.image			= *imageSparse;
			imageMipTailBindInfo.bindCount		= static_cast<deUint32>(imageMipTailBinds.size());
			imageMipTailBindInfo.pBinds			= imageMipTailBinds.data();

			bindSparseInfo.imageOpaqueBindCount = 1u;
			bindSparseInfo.pImageOpaqueBinds	= &imageMipTailBindInfo;
		}

		// Submit sparse bind commands for execution
		VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, DE_NULL));
	}

	// Create image to store texels copied from sparse image
	imageTexelsInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageTexelsInfo.pNext					= DE_NULL;
	imageTexelsInfo.flags					= 0u;
	imageTexelsInfo.imageType				= imageSparseInfo.imageType;
	imageTexelsInfo.format					= imageSparseInfo.format;
	imageTexelsInfo.extent					= imageSparseInfo.extent;
	imageTexelsInfo.arrayLayers				= imageSparseInfo.arrayLayers;
	imageTexelsInfo.mipLevels				= imageSparseInfo.mipLevels;
	imageTexelsInfo.samples					= imageSparseInfo.samples;
	imageTexelsInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	imageTexelsInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imageTexelsInfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | imageOutputUsageFlags();
	imageTexelsInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imageTexelsInfo.queueFamilyIndexCount	= 0u;
	imageTexelsInfo.pQueueFamilyIndices		= DE_NULL;

	if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
	{
		imageTexelsInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	const Unique<VkImage>			imageTexels			(createImage(deviceInterface, getDevice(), &imageTexelsInfo));
	const de::UniquePtr<Allocation>	imageTexelsAlloc	(bindImage(deviceInterface, getDevice(), getAllocator(), *imageTexels, MemoryRequirement::Any));

	// Create image to store residency info copied from sparse image
	imageResidencyInfo			= imageTexelsInfo;
	imageResidencyInfo.format	= mapTextureFormat(m_residencyFormat);

	const Unique<VkImage>			imageResidency		(createImage(deviceInterface, getDevice(), &imageResidencyInfo));
	const de::UniquePtr<Allocation>	imageResidencyAlloc	(bindImage(deviceInterface, getDevice(), getAllocator(), *imageResidency, MemoryRequirement::Any));

	std::vector <VkBufferImageCopy> bufferImageSparseCopy(formatDescription.numPlanes * imageSparseInfo.mipLevels);

	{
		deUint32 bufferOffset = 0u;
		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			{
				bufferImageSparseCopy[planeNdx*imageSparseInfo.mipLevels + mipmapNdx] =
				{
					bufferOffset,																		//	VkDeviceSize				bufferOffset;
					0u,																					//	deUint32					bufferRowLength;
					0u,																					//	deUint32					bufferImageHeight;
					makeImageSubresourceLayers(aspect, mipmapNdx, 0u, imageSparseInfo.arrayLayers),		//	VkImageSubresourceLayers	imageSubresource;
					makeOffset3D(0, 0, 0),																//	VkOffset3D					imageOffset;
					vk::getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, mipmapNdx)	//	VkExtent3D					imageExtent;
				};
				bufferOffset += getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
			}
		}
	}

	// Create command buffer for compute and transfer operations
	const Unique<VkCommandPool>		commandPool(makeCommandPool(deviceInterface, getDevice(), extractQueue.queueFamilyIndex));
	const Unique<VkCommandBuffer>	commandBuffer(allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	// Create input buffer
	const VkBufferCreateInfo		inputBufferCreateInfo	= makeBufferCreateInfo(imageSparseSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	const Unique<VkBuffer>			inputBuffer				(createBuffer(deviceInterface, getDevice(), &inputBufferCreateInfo));
	const de::UniquePtr<Allocation>	inputBufferAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *inputBuffer, MemoryRequirement::HostVisible));

	// Fill input buffer with reference data
	std::vector<deUint8> referenceData(imageSparseSizeInBytes);

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		for (deUint32 mipmapNdx = 0u; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
		{
			const deUint32 mipLevelSizeinBytes	= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx);
			const deUint32 bufferOffset			= static_cast<deUint32>(bufferImageSparseCopy[mipmapNdx].bufferOffset);

			for (deUint32 byteNdx = 0u; byteNdx < mipLevelSizeinBytes; ++byteNdx)
			{
				referenceData[bufferOffset + byteNdx] = (deUint8)( (mipmapNdx + byteNdx) % 127u );
			}
		}
	}

	deMemcpy(inputBufferAlloc->getHostPtr(), referenceData.data(), imageSparseSizeInBytes);
	flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

	{
		// Prepare input buffer for data transfer operation
		const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier
		(
			VK_ACCESS_HOST_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			*inputBuffer,
			0u,
			imageSparseSizeInBytes
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 1u, &inputBufferBarrier, 0u, DE_NULL);
	}

	{
		// Prepare sparse image for data transfer operation
		std::vector<VkImageMemoryBarrier> imageSparseTransferDstBarriers;
		for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			imageSparseTransferDstBarriers.emplace_back(makeImageMemoryBarrier
			(
				0u,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				*imageSparse,
				makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers),
				sparseQueue.queueFamilyIndex != extractQueue.queueFamilyIndex ? sparseQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED,
				sparseQueue.queueFamilyIndex != extractQueue.queueFamilyIndex ? extractQueue.queueFamilyIndex : VK_QUEUE_FAMILY_IGNORED
			));
		}
		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, static_cast<deUint32>(imageSparseTransferDstBarriers.size()), imageSparseTransferDstBarriers.data());
	}

	// Copy reference data from input buffer to sparse image
	deviceInterface.cmdCopyBufferToImage(*commandBuffer, *inputBuffer, *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(bufferImageSparseCopy.size()), bufferImageSparseCopy.data());

	recordCommands(*commandBuffer, imageSparseInfo, *imageSparse, *imageTexels, *imageResidency);

	const VkBufferCreateInfo		bufferTexelsCreateInfo	= makeBufferCreateInfo(imageSparseSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const Unique<VkBuffer>			bufferTexels			(createBuffer(deviceInterface, getDevice(), &bufferTexelsCreateInfo));
	const de::UniquePtr<Allocation>	bufferTexelsAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *bufferTexels, MemoryRequirement::HostVisible));

	// Copy data from texels image to buffer
	deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageTexels, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *bufferTexels, static_cast<deUint32>(bufferImageSparseCopy.size()), bufferImageSparseCopy.data());

	const deUint32				imageResidencySizeInBytes = getImageSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, m_residencyFormat, imageSparseInfo.mipLevels, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);

	const VkBufferCreateInfo		bufferResidencyCreateInfo	= makeBufferCreateInfo(imageResidencySizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const Unique<VkBuffer>			bufferResidency				(createBuffer(deviceInterface, getDevice(), &bufferResidencyCreateInfo));
	const de::UniquePtr<Allocation>	bufferResidencyAlloc		(bindBuffer(deviceInterface, getDevice(), getAllocator(), *bufferResidency, MemoryRequirement::HostVisible));

	// Copy data from residency image to buffer
	std::vector <VkBufferImageCopy> bufferImageResidencyCopy(formatDescription.numPlanes * imageSparseInfo.mipLevels);

	{
		deUint32 bufferOffset = 0u;
		for (deUint32 planeNdx = 0u; planeNdx < formatDescription.numPlanes; ++planeNdx)
		{
			const VkImageAspectFlags aspect = (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

			for (deUint32 mipmapNdx = 0u; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
			{
				bufferImageResidencyCopy[planeNdx * imageSparseInfo.mipLevels + mipmapNdx] =
				{
					bufferOffset,																		//	VkDeviceSize				bufferOffset;
					0u,																					//	deUint32					bufferRowLength;
					0u,																					//	deUint32					bufferImageHeight;
					makeImageSubresourceLayers(aspect, mipmapNdx, 0u, imageSparseInfo.arrayLayers),		//	VkImageSubresourceLayers	imageSubresource;
					makeOffset3D(0, 0, 0),																//	VkOffset3D					imageOffset;
					vk::getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx, mipmapNdx)	//	VkExtent3D					imageExtent;
				};
				bufferOffset += getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, m_residencyFormat, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
			}
		}
	}

	deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageResidency, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *bufferResidency, static_cast<deUint32>(bufferImageResidencyCopy.size()), bufferImageResidencyCopy.data());

	{
		VkBufferMemoryBarrier bufferOutputHostReadBarriers[2];

		bufferOutputHostReadBarriers[0] = makeBufferMemoryBarrier
		(
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT,
			*bufferTexels,
			0u,
			imageSparseSizeInBytes
		);

		bufferOutputHostReadBarriers[1] = makeBufferMemoryBarrier
		(
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT,
			*bufferResidency,
			0u,
			imageResidencySizeInBytes
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 2u, bufferOutputHostReadBarriers, 0u, DE_NULL);
	}

	// End recording commands
	endCommandBuffer(deviceInterface, *commandBuffer);

	const VkPipelineStageFlags stageBits[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

	// Submit commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, getDevice(), extractQueue.queueHandle, *commandBuffer, 1u, &memoryBindSemaphore.get(), stageBits);

	// Wait for sparse queue to become idle
	deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

	// Retrieve data from residency buffer to host memory
	invalidateAlloc(deviceInterface, getDevice(), *bufferResidencyAlloc);

	const deUint32* bufferResidencyData = static_cast<const deUint32*>(bufferResidencyAlloc->getHostPtr());

	deUint32 pixelOffsetNotAligned = 0u;
	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
		{
			const deUint32 mipLevelSizeInBytes	= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, m_residencyFormat, mipmapNdx);
			const deUint32 pixelOffsetAligned	= static_cast<deUint32>(bufferImageResidencyCopy[planeNdx * imageSparseInfo.mipLevels + mipmapNdx].bufferOffset) / tcu::getPixelSize(m_residencyFormat);

			if (deMemCmp(&bufferResidencyData[pixelOffsetAligned], &residencyReferenceData[pixelOffsetNotAligned], mipLevelSizeInBytes) != 0)
				return tcu::TestStatus::fail("Failed");

			pixelOffsetNotAligned += mipLevelSizeInBytes / tcu::getPixelSize(m_residencyFormat);
		}
}
	// Retrieve data from texels buffer to host memory
	invalidateAlloc(deviceInterface, getDevice(), *bufferTexelsAlloc);

	const deUint8* bufferTexelsData = static_cast<const deUint8*>(bufferTexelsAlloc->getHostPtr());

	for (deUint32 planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
	{
		const VkImageAspectFlags	aspect		= (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
		const deUint32				aspectIndex	= getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

		if (aspectIndex == NO_MATCH_FOUND)
			TCU_THROW(NotSupportedError, "Not supported image aspect");

		VkSparseImageMemoryRequirements	aspectRequirements	= sparseMemoryRequirements[aspectIndex];

		for (deUint32 mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
		{
			const deUint32 mipLevelSizeInBytes	= getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription,planeNdx, mipmapNdx);
			const deUint32 bufferOffset			= static_cast<deUint32>(bufferImageSparseCopy[planeNdx * imageSparseInfo.mipLevels + mipmapNdx].bufferOffset);

			if (mipmapNdx < aspectRequirements.imageMipTailFirstLod)
			{
				if (mipmapNdx % MEMORY_BLOCK_TYPE_COUNT == MEMORY_BLOCK_BOUND)
				{
					if (deMemCmp(&bufferTexelsData[bufferOffset], &referenceData[bufferOffset], mipLevelSizeInBytes) != 0)
						return tcu::TestStatus::fail("Failed");
				}
				else if (getPhysicalDeviceProperties(instance, physicalDevice).sparseProperties.residencyNonResidentStrict)
				{
					std::vector<deUint8> zeroData;
					zeroData.assign(mipLevelSizeInBytes, 0u);

					if (deMemCmp(&bufferTexelsData[bufferOffset], zeroData.data(), mipLevelSizeInBytes) != 0)
						return tcu::TestStatus::fail("Failed");
				}
			}
			else
			{
				if (deMemCmp(&bufferTexelsData[bufferOffset], &referenceData[bufferOffset], mipLevelSizeInBytes) != 0)
					return tcu::TestStatus::fail("Failed");
			}
		}
	}

	return tcu::TestStatus::pass("Passed");
}

} // sparse
} // vkt
