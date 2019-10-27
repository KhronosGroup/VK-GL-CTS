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
 * \file  vktSparseResourcesTestsUtil.cpp
 * \brief Sparse Resources Tests Utility Classes
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesTestsUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"

#include <deMath.h>

using namespace vk;

namespace vkt
{
namespace sparse
{

std::vector<TestFormat> getTestFormats (const ImageType& imageType)
{
	std::vector<TestFormat> results =
	{
		{ VK_FORMAT_R32_SINT },				{ VK_FORMAT_R16_SINT },				{ VK_FORMAT_R8_SINT },
		{ VK_FORMAT_R32_UINT },				{ VK_FORMAT_R16_UINT },				{ VK_FORMAT_R8_UINT },
											{ VK_FORMAT_R16_UNORM },			{ VK_FORMAT_R8_UNORM },
											{ VK_FORMAT_R16_SNORM },			{ VK_FORMAT_R8_SNORM },
		{ VK_FORMAT_R32G32_SINT },			{ VK_FORMAT_R16G16_SINT },			{ VK_FORMAT_R8G8_SINT },
		{ VK_FORMAT_R32G32_UINT },			{ VK_FORMAT_R16G16_UINT },			{ VK_FORMAT_R8G8_UINT },
											{ VK_FORMAT_R16G16_UNORM },			{ VK_FORMAT_R8G8_UNORM },
											{ VK_FORMAT_R16G16_SNORM },			{ VK_FORMAT_R8G8_SNORM },
		{ VK_FORMAT_R32G32B32A32_SINT },	{ VK_FORMAT_R16G16B16A16_SINT },	{ VK_FORMAT_R8G8B8A8_SINT },
		{ VK_FORMAT_R32G32B32A32_UINT },	{ VK_FORMAT_R16G16B16A16_UINT },	{ VK_FORMAT_R8G8B8A8_UINT },
											{ VK_FORMAT_R16G16B16A16_UNORM },	{ VK_FORMAT_R8G8B8A8_UNORM },
											{ VK_FORMAT_R16G16B16A16_SNORM },	{ VK_FORMAT_R8G8B8A8_SNORM }
	};

	if (imageType == IMAGE_TYPE_2D || imageType == IMAGE_TYPE_2D_ARRAY)
	{
		std::vector<TestFormat> ycbcrFormats =
		{
			{ VK_FORMAT_G8B8G8R8_422_UNORM },
			{ VK_FORMAT_B8G8R8G8_422_UNORM },
			{ VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM },
			{ VK_FORMAT_G8_B8R8_2PLANE_420_UNORM },
			{ VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM },
			{ VK_FORMAT_G8_B8R8_2PLANE_422_UNORM },
			{ VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM },
			{ VK_FORMAT_R10X6_UNORM_PACK16 },
			{ VK_FORMAT_R10X6G10X6_UNORM_2PACK16 },
			{ VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 },
			{ VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 },
			{ VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 },
			{ VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 },
			{ VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 },
			{ VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 },
			{ VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 },
			{ VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 },
			{ VK_FORMAT_R12X4_UNORM_PACK16 },
			{ VK_FORMAT_R12X4G12X4_UNORM_2PACK16 },
			{ VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 },
			{ VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 },
			{ VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 },
			{ VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 },
			{ VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 },
			{ VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 },
			{ VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 },
			{ VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 },
			{ VK_FORMAT_G16B16G16R16_422_UNORM },
			{ VK_FORMAT_B16G16R16G16_422_UNORM },
			{ VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM },
			{ VK_FORMAT_G16_B16R16_2PLANE_420_UNORM },
			{ VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM },
			{ VK_FORMAT_G16_B16R16_2PLANE_422_UNORM },
			{ VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM }
		};
		std::copy(begin(ycbcrFormats), end(ycbcrFormats), std::back_inserter(results));
	}

	return results;
}

tcu::UVec3 getShaderGridSize (const ImageType imageType, const tcu::UVec3& imageSize, const deUint32 mipLevel)
{
	const deUint32 mipLevelX = std::max(imageSize.x() >> mipLevel, 1u);
	const deUint32 mipLevelY = std::max(imageSize.y() >> mipLevel, 1u);
	const deUint32 mipLevelZ = std::max(imageSize.z() >> mipLevel, 1u);

	switch (imageType)
	{
		case IMAGE_TYPE_1D:
			return tcu::UVec3(mipLevelX, 1u, 1u);

		case IMAGE_TYPE_BUFFER:
			return tcu::UVec3(imageSize.x(), 1u, 1u);

		case IMAGE_TYPE_1D_ARRAY:
			return tcu::UVec3(mipLevelX, imageSize.z(), 1u);

		case IMAGE_TYPE_2D:
			return tcu::UVec3(mipLevelX, mipLevelY, 1u);

		case IMAGE_TYPE_2D_ARRAY:
			return tcu::UVec3(mipLevelX, mipLevelY, imageSize.z());

		case IMAGE_TYPE_3D:
			return tcu::UVec3(mipLevelX, mipLevelY, mipLevelZ);

		case IMAGE_TYPE_CUBE:
			return tcu::UVec3(mipLevelX, mipLevelY, 6u);

		case IMAGE_TYPE_CUBE_ARRAY:
			return tcu::UVec3(mipLevelX, mipLevelY, 6u * imageSize.z());

		default:
			DE_FATAL("Unknown image type");
			return tcu::UVec3(1u, 1u, 1u);
	}
}

tcu::UVec3 getLayerSize (const ImageType imageType, const tcu::UVec3& imageSize)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_BUFFER:
			return tcu::UVec3(imageSize.x(), 1u, 1u);

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return tcu::UVec3(imageSize.x(), imageSize.y(), 1u);

		case IMAGE_TYPE_3D:
			return tcu::UVec3(imageSize.x(), imageSize.y(), imageSize.z());

		default:
			DE_FATAL("Unknown image type");
			return tcu::UVec3(1u, 1u, 1u);
	}
}

deUint32 getNumLayers (const ImageType imageType, const tcu::UVec3& imageSize)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_3D:
		case IMAGE_TYPE_BUFFER:
			return 1u;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D_ARRAY:
			return imageSize.z();

		case IMAGE_TYPE_CUBE:
			return 6u;

		case IMAGE_TYPE_CUBE_ARRAY:
			return imageSize.z() * 6u;

		default:
			DE_FATAL("Unknown image type");
			return 0u;
	}
}

deUint32 getNumPixels (const ImageType imageType, const tcu::UVec3& imageSize)
{
	const tcu::UVec3 gridSize = getShaderGridSize(imageType, imageSize);

	return gridSize.x() * gridSize.y() * gridSize.z();
}

deUint32 getDimensions (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return 1u;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return 2u;

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
		case IMAGE_TYPE_3D:
			return 3u;

		default:
			DE_FATAL("Unknown image type");
			return 0u;
	}
}

deUint32 getLayerDimensions (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
		case IMAGE_TYPE_1D_ARRAY:
			return 1u;

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return 2u;

		case IMAGE_TYPE_3D:
			return 3u;

		default:
			DE_FATAL("Unknown image type");
			return 0u;
	}
}

bool isImageSizeSupported (const InstanceInterface& instance, const VkPhysicalDevice physicalDevice, const ImageType imageType, const tcu::UVec3& imageSize)
{
	const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

	switch (imageType)
	{
		case IMAGE_TYPE_1D:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimension1D;
		case IMAGE_TYPE_1D_ARRAY:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimension1D &&
					imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
		case IMAGE_TYPE_2D:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimension2D &&
					imageSize.y() <= deviceProperties.limits.maxImageDimension2D;
		case IMAGE_TYPE_2D_ARRAY:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimension2D &&
					imageSize.y() <= deviceProperties.limits.maxImageDimension2D &&
					imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
		case IMAGE_TYPE_CUBE:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimensionCube &&
					imageSize.y() <= deviceProperties.limits.maxImageDimensionCube;
		case IMAGE_TYPE_CUBE_ARRAY:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimensionCube &&
					imageSize.y() <= deviceProperties.limits.maxImageDimensionCube &&
					imageSize.z() <= deviceProperties.limits.maxImageArrayLayers;
		case IMAGE_TYPE_3D:
			return	imageSize.x() <= deviceProperties.limits.maxImageDimension3D &&
					imageSize.y() <= deviceProperties.limits.maxImageDimension3D &&
					imageSize.z() <= deviceProperties.limits.maxImageDimension3D;
		case IMAGE_TYPE_BUFFER:
			return true;
		default:
			DE_FATAL("Unknown image type");
			return false;
	}
}

VkBufferImageCopy makeBufferImageCopy (const VkExtent3D		extent,
									   const deUint32		layerCount,
									   const deUint32		mipmapLevel,
									   const VkDeviceSize	bufferOffset)
{
	const VkBufferImageCopy copyParams =
	{
		bufferOffset,																		//	VkDeviceSize				bufferOffset;
		0u,																					//	deUint32					bufferRowLength;
		0u,																					//	deUint32					bufferImageHeight;
		makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mipmapLevel, 0u, layerCount),	//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, 0),																//	VkOffset3D					imageOffset;
		extent,																				//	VkExtent3D					imageExtent;
	};
	return copyParams;
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&		vk,
									  const VkDevice				device,
									  const VkPipelineLayout		pipelineLayout,
									  const VkShaderModule			shaderModule,
									  const VkSpecializationInfo*	specializationInfo)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		specializationInfo,										// const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo);
}

void submitCommands (const DeviceInterface&			vk,
					 const VkQueue					queue,
					 const VkCommandBuffer			commandBuffer,
					 const deUint32					waitSemaphoreCount,
					 const VkSemaphore*				pWaitSemaphores,
					 const VkPipelineStageFlags*	pWaitDstStageMask,
					 const deUint32					signalSemaphoreCount,
					 const VkSemaphore*				pSignalSemaphores)
{
	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		DE_NULL,						// const void*					pNext;
		waitSemaphoreCount,				// deUint32						waitSemaphoreCount;
		pWaitSemaphores,				// const VkSemaphore*			pWaitSemaphores;
		pWaitDstStageMask,				// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// deUint32						commandBufferCount;
		&commandBuffer,					// const VkCommandBuffer*		pCommandBuffers;
		signalSemaphoreCount,			// deUint32						signalSemaphoreCount;
		pSignalSemaphores,				// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, DE_NULL));
}

void submitCommandsAndWait (const DeviceInterface&		vk,
							const VkDevice				device,
							const VkQueue				queue,
							const VkCommandBuffer		commandBuffer,
							const deUint32				waitSemaphoreCount,
							const VkSemaphore*			pWaitSemaphores,
							const VkPipelineStageFlags*	pWaitDstStageMask,
							const deUint32				signalSemaphoreCount,
							const VkSemaphore*			pSignalSemaphores,
							const bool					useDeviceGroups,
							const deUint32				physicalDeviceID)
{
	const VkFenceCreateInfo	fenceParams				=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,				// VkStructureType		sType;
		DE_NULL,											// const void*			pNext;
		0u,													// VkFenceCreateFlags	flags;
	};
	const Unique<VkFence>	fence(createFence		(vk, device, &fenceParams));

	const deUint32			deviceMask				= 1 << physicalDeviceID;
	std::vector<deUint32>	deviceIndices			(waitSemaphoreCount, physicalDeviceID);
	VkDeviceGroupSubmitInfo deviceGroupSubmitInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR,		//VkStructureType		sType
		DE_NULL,											// const void*			pNext
		waitSemaphoreCount,									// uint32_t				waitSemaphoreCount
		deviceIndices.size() ? &deviceIndices[0] : DE_NULL,	// const uint32_t*		pWaitSemaphoreDeviceIndices
		1u,													// uint32_t				commandBufferCount
		&deviceMask,										// const uint32_t*		pCommandBufferDeviceMasks
		0u,													// uint32_t				signalSemaphoreCount
		DE_NULL,											// const uint32_t*		pSignalSemaphoreDeviceIndices
	};
	const VkSubmitInfo		submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType;
		useDeviceGroups ? &deviceGroupSubmitInfo : DE_NULL,	// const void*					pNext;
		waitSemaphoreCount,									// deUint32						waitSemaphoreCount;
		pWaitSemaphores,									// const VkSemaphore*			pWaitSemaphores;
		pWaitDstStageMask,									// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,													// deUint32						commandBufferCount;
		&commandBuffer,										// const VkCommandBuffer*		pCommandBuffers;
		signalSemaphoreCount,								// deUint32						signalSemaphoreCount;
		pSignalSemaphores,									// const VkSemaphore*			pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

VkImageType	mapImageType (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_BUFFER:
			return VK_IMAGE_TYPE_1D;

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return VK_IMAGE_TYPE_2D;

		case IMAGE_TYPE_3D:
			return VK_IMAGE_TYPE_3D;

		default:
			DE_FATAL("Unexpected image type");
			return VK_IMAGE_TYPE_LAST;
	}
}

VkImageViewType	mapImageViewType (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:			return VK_IMAGE_VIEW_TYPE_1D;
		case IMAGE_TYPE_1D_ARRAY:	return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		case IMAGE_TYPE_2D:			return VK_IMAGE_VIEW_TYPE_2D;
		case IMAGE_TYPE_2D_ARRAY:	return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case IMAGE_TYPE_3D:			return VK_IMAGE_VIEW_TYPE_3D;
		case IMAGE_TYPE_CUBE:		return VK_IMAGE_VIEW_TYPE_CUBE;
		case IMAGE_TYPE_CUBE_ARRAY:	return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

		default:
			DE_FATAL("Unexpected image type");
			return VK_IMAGE_VIEW_TYPE_LAST;
	}
}

std::string getImageTypeName (const ImageType imageType)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:			return "1d";
		case IMAGE_TYPE_1D_ARRAY:	return "1d_array";
		case IMAGE_TYPE_2D:			return "2d";
		case IMAGE_TYPE_2D_ARRAY:	return "2d_array";
		case IMAGE_TYPE_3D:			return "3d";
		case IMAGE_TYPE_CUBE:		return "cube";
		case IMAGE_TYPE_CUBE_ARRAY:	return "cube_array";
		case IMAGE_TYPE_BUFFER:		return "buffer";

		default:
			DE_FATAL("Unexpected image type");
			return "";
	}
}

std::string getShaderImageType (const tcu::TextureFormat& format, const ImageType imageType)
{
	std::string formatPart = tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" :
							 tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" : "";

	std::string imageTypePart;
	switch (imageType)
	{
		case IMAGE_TYPE_1D:			imageTypePart = "1D";			break;
		case IMAGE_TYPE_1D_ARRAY:	imageTypePart = "1DArray";		break;
		case IMAGE_TYPE_2D:			imageTypePart = "2D";			break;
		case IMAGE_TYPE_2D_ARRAY:	imageTypePart = "2DArray";		break;
		case IMAGE_TYPE_3D:			imageTypePart = "3D";			break;
		case IMAGE_TYPE_CUBE:		imageTypePart = "Cube";			break;
		case IMAGE_TYPE_CUBE_ARRAY:	imageTypePart = "CubeArray";	break;
		case IMAGE_TYPE_BUFFER:		imageTypePart = "Buffer";		break;

		default:
			DE_FATAL("Unexpected image type");
	}

	return formatPart + "image" + imageTypePart;
}

std::string getShaderImageType (const vk::PlanarFormatDescription& description, const ImageType imageType)
{
	std::string	formatPart;
	std::string	imageTypePart;

	// all PlanarFormatDescription types have at least one channel ( 0 ) and all channel types are the same :
	switch (description.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			formatPart = "i";
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			formatPart = "u";
			break;
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			break;

		default:
			DE_FATAL("Unexpected channel type");
	}

	switch (imageType)
	{
		case IMAGE_TYPE_1D:			imageTypePart = "1D";			break;
		case IMAGE_TYPE_1D_ARRAY:	imageTypePart = "1DArray";		break;
		case IMAGE_TYPE_2D:			imageTypePart = "2D";			break;
		case IMAGE_TYPE_2D_ARRAY:	imageTypePart = "2DArray";		break;
		case IMAGE_TYPE_3D:			imageTypePart = "3D";			break;
		case IMAGE_TYPE_CUBE:		imageTypePart = "Cube";			break;
		case IMAGE_TYPE_CUBE_ARRAY:	imageTypePart = "CubeArray";	break;
		case IMAGE_TYPE_BUFFER:		imageTypePart = "Buffer";		break;

		default:
			DE_FATAL("Unexpected image type");
	}

	return formatPart + "image" + imageTypePart;
}

std::string getShaderImageDataType(const tcu::TextureFormat& format)
{
	switch (tcu::getTextureChannelClass(format.type))
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "uvec4";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "ivec4";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "vec4";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getShaderImageDataType (const vk::PlanarFormatDescription& description)
{
	switch (description.channels[0].type)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return "uvec4";
		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return "ivec4";
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "vec4";
		default:
			DE_FATAL("Unexpected channel type");
			return "";
	}
}

std::string getShaderImageFormatQualifier (const tcu::TextureFormat& format)
{
	const char* orderPart;
	const char* typePart;

	switch (format.order)
	{
		case tcu::TextureFormat::R:		orderPart = "r";	break;
		case tcu::TextureFormat::RG:	orderPart = "rg";	break;
		case tcu::TextureFormat::RGB:	orderPart = "rgb";	break;
		case tcu::TextureFormat::RGBA:	orderPart = "rgba";	break;

		default:
			DE_FATAL("Unexpected channel order");
			orderPart = DE_NULL;
	}

	switch (format.type)
	{
		case tcu::TextureFormat::FLOAT:				typePart = "32f";		break;
		case tcu::TextureFormat::HALF_FLOAT:		typePart = "16f";		break;

		case tcu::TextureFormat::UNSIGNED_INT32:	typePart = "32ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT16:	typePart = "16ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT8:		typePart = "8ui";		break;

		case tcu::TextureFormat::SIGNED_INT32:		typePart = "32i";		break;
		case tcu::TextureFormat::SIGNED_INT16:		typePart = "16i";		break;
		case tcu::TextureFormat::SIGNED_INT8:		typePart = "8i";		break;

		case tcu::TextureFormat::UNORM_INT16:		typePart = "16";		break;
		case tcu::TextureFormat::UNORM_INT8:		typePart = "8";			break;

		case tcu::TextureFormat::SNORM_INT16:		typePart = "16_snorm";	break;
		case tcu::TextureFormat::SNORM_INT8:		typePart = "8_snorm";	break;

		default:
			DE_FATAL("Unexpected channel type");
			typePart = DE_NULL;
	}

	return std::string() + orderPart + typePart;
}

std::string getShaderImageFormatQualifier (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_R8_SINT:										return "r8i";
		case VK_FORMAT_R16_SINT:									return "r16i";
		case VK_FORMAT_R32_SINT:									return "r32i";
		case VK_FORMAT_R8_UINT:										return "r8ui";
		case VK_FORMAT_R16_UINT:									return "r16ui";
		case VK_FORMAT_R32_UINT:									return "r32ui";
		case VK_FORMAT_R8_SNORM:									return "r8_snorm";
		case VK_FORMAT_R16_SNORM:									return "r16_snorm";
		case VK_FORMAT_R8_UNORM:									return "r8";
		case VK_FORMAT_R16_UNORM:									return "r16";

		case VK_FORMAT_R8G8_SINT:									return "rg8i";
		case VK_FORMAT_R16G16_SINT:									return "rg16i";
		case VK_FORMAT_R32G32_SINT:									return "rg32i";
		case VK_FORMAT_R8G8_UINT:									return "rg8ui";
		case VK_FORMAT_R16G16_UINT:									return "rg16ui";
		case VK_FORMAT_R32G32_UINT:									return "rg32ui";
		case VK_FORMAT_R8G8_SNORM:									return "rg8_snorm";
		case VK_FORMAT_R16G16_SNORM:								return "rg16_snorm";
		case VK_FORMAT_R8G8_UNORM:									return "rg8";
		case VK_FORMAT_R16G16_UNORM:								return "rg16";

		case VK_FORMAT_R8G8B8A8_SINT:								return "rgba8i";
		case VK_FORMAT_R16G16B16A16_SINT:							return "rgba16i";
		case VK_FORMAT_R32G32B32A32_SINT:							return "rgba32i";
		case VK_FORMAT_R8G8B8A8_UINT:								return "rgba8ui";
		case VK_FORMAT_R16G16B16A16_UINT:							return "rgba16ui";
		case VK_FORMAT_R32G32B32A32_UINT:							return "rgba32ui";
		case VK_FORMAT_R8G8B8A8_SNORM:								return "rgba8_snorm";
		case VK_FORMAT_R16G16B16A16_SNORM:							return "rgba16_snorm";
		case VK_FORMAT_R8G8B8A8_UNORM:								return "rgba8";
		case VK_FORMAT_R16G16B16A16_UNORM:							return "rgba16";

		case VK_FORMAT_G8B8G8R8_422_UNORM:							return "rgba8";
		case VK_FORMAT_B8G8R8G8_422_UNORM:							return "rgba8";
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:					return "rgba8";
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:					return "rgba8";
		case VK_FORMAT_R10X6_UNORM_PACK16:							return "r16";
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:					return "rg16";
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:			return "rgba16";
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_R12X4_UNORM_PACK16:							return "r16";
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:					return "rg16";
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:			return "rgba16";
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:		return "rgba16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:	return "rgba16";
		case VK_FORMAT_G16B16G16R16_422_UNORM:						return "rgba16";
		case VK_FORMAT_B16G16R16G16_422_UNORM:						return "rgba16";
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:				return "rgba16";
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:					return "rgba16";
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:				return "rgba16";
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:					return "rgba16";
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:				return "rgba16";

		default:
			DE_FATAL("Unexpected texture format");
			return "error";
	}
}

std::string getImageFormatID (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_R8_SINT:				return "r8i";
		case VK_FORMAT_R16_SINT:			return "r16i";
		case VK_FORMAT_R32_SINT:			return "r32i";
		case VK_FORMAT_R8_UINT:				return "r8ui";
		case VK_FORMAT_R16_UINT:			return "r16ui";
		case VK_FORMAT_R32_UINT:			return "r32ui";
		case VK_FORMAT_R8_SNORM:			return "r8_snorm";
		case VK_FORMAT_R16_SNORM:			return "r16_snorm";
		case VK_FORMAT_R8_UNORM:			return "r8";
		case VK_FORMAT_R16_UNORM:			return "r16";

		case VK_FORMAT_R8G8_SINT:			return "rg8i";
		case VK_FORMAT_R16G16_SINT:			return "rg16i";
		case VK_FORMAT_R32G32_SINT:			return "rg32i";
		case VK_FORMAT_R8G8_UINT:			return "rg8ui";
		case VK_FORMAT_R16G16_UINT:			return "rg16ui";
		case VK_FORMAT_R32G32_UINT:			return "rg32ui";
		case VK_FORMAT_R8G8_SNORM:			return "rg8_snorm";
		case VK_FORMAT_R16G16_SNORM:		return "rg16_snorm";
		case VK_FORMAT_R8G8_UNORM:			return "rg8";
		case VK_FORMAT_R16G16_UNORM:		return "rg16";

		case VK_FORMAT_R8G8B8A8_SINT:		return "rgba8i";
		case VK_FORMAT_R16G16B16A16_SINT:	return "rgba16i";
		case VK_FORMAT_R32G32B32A32_SINT:	return "rgba32i";
		case VK_FORMAT_R8G8B8A8_UINT:		return "rgba8ui";
		case VK_FORMAT_R16G16B16A16_UINT:	return "rgba16ui";
		case VK_FORMAT_R32G32B32A32_UINT:	return "rgba32ui";
		case VK_FORMAT_R8G8B8A8_SNORM:		return "rgba8_snorm";
		case VK_FORMAT_R16G16B16A16_SNORM:	return "rgba16_snorm";
		case VK_FORMAT_R8G8B8A8_UNORM:		return "rgba8";
		case VK_FORMAT_R16G16B16A16_UNORM:	return "rgba16";

		case VK_FORMAT_G8B8G8R8_422_UNORM:
		case VK_FORMAT_B8G8R8G8_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
		case VK_FORMAT_R10X6_UNORM_PACK16:
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case VK_FORMAT_R12X4_UNORM_PACK16:
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		case VK_FORMAT_G16B16G16R16_422_UNORM:
		case VK_FORMAT_B16G16R16G16_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
			return de::toLower(std::string(getFormatName(format)).substr(10));

		default:
			DE_FATAL("Unexpected texture format");
			return "error";
	}
}

std::string getShaderImageCoordinates	(const ImageType	imageType,
										 const std::string&	x,
										 const std::string&	xy,
										 const std::string&	xyz)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return x;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return xy;

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_3D:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return xyz;

		default:
			DE_FATAL("Unexpected image type");
			return "";
	}
}

deUint32 getImageMipLevelSizeInBytes(const VkExtent3D& baseExtents, const deUint32 layersCount, const tcu::TextureFormat& format, const deUint32 mipmapLevel, const deUint32 mipmapMemoryAlignment)
{
	const VkExtent3D extents = mipLevelExtents(baseExtents, mipmapLevel);

	return deAlign32(extents.width * extents.height * extents.depth * layersCount * tcu::getPixelSize(format), mipmapMemoryAlignment);
}

deUint32 getImageSizeInBytes(const VkExtent3D& baseExtents, const deUint32 layersCount, const tcu::TextureFormat& format, const deUint32 mipmapLevelsCount, const deUint32 mipmapMemoryAlignment)
{
	deUint32 imageSizeInBytes = 0;
	for (deUint32 mipmapLevel = 0; mipmapLevel < mipmapLevelsCount; ++mipmapLevel)
		imageSizeInBytes += getImageMipLevelSizeInBytes(baseExtents, layersCount, format, mipmapLevel, mipmapMemoryAlignment);

	return imageSizeInBytes;
}

deUint32 getImageMipLevelSizeInBytes (const VkExtent3D& baseExtents, const deUint32 layersCount, const vk::PlanarFormatDescription& formatDescription, const deUint32 planeNdx, const deUint32 mipmapLevel, const deUint32 mipmapMemoryAlignment)
{
	return layersCount * getPlaneSizeInBytes(formatDescription, baseExtents, planeNdx, mipmapLevel, mipmapMemoryAlignment);
}

deUint32 getImageSizeInBytes (const VkExtent3D& baseExtents, const deUint32 layersCount, const vk::PlanarFormatDescription& formatDescription, const deUint32 planeNdx, const deUint32 mipmapLevelsCount, const deUint32 mipmapMemoryAlignment)
{
	deUint32 imageSizeInBytes = 0;

	for (deUint32 mipmapLevel = 0; mipmapLevel < mipmapLevelsCount; ++mipmapLevel)
		imageSizeInBytes += getImageMipLevelSizeInBytes(baseExtents, layersCount, formatDescription, planeNdx, mipmapLevel, mipmapMemoryAlignment);

	return imageSizeInBytes;
}

VkSparseImageMemoryBind	makeSparseImageMemoryBind  (const DeviceInterface&			vk,
													const VkDevice					device,
													const VkDeviceSize				allocationSize,
													const deUint32					memoryType,
													const VkImageSubresource&		subresource,
													const VkOffset3D&				offset,
													const VkExtent3D&				extent)
{
	const VkMemoryAllocateInfo	allocInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType			sType;
		DE_NULL,								//	const void*				pNext;
		allocationSize,							//	VkDeviceSize			allocationSize;
		memoryType,								//	deUint32				memoryTypeIndex;
	};

	VkDeviceMemory deviceMemory = 0;
	VK_CHECK(vk.allocateMemory(device, &allocInfo, DE_NULL, &deviceMemory));

	VkSparseImageMemoryBind imageMemoryBind;

	imageMemoryBind.subresource		= subresource;
	imageMemoryBind.memory			= deviceMemory;
	imageMemoryBind.memoryOffset	= 0u;
	imageMemoryBind.flags			= 0u;
	imageMemoryBind.offset			= offset;
	imageMemoryBind.extent			= extent;

	return imageMemoryBind;
}

VkSparseMemoryBind makeSparseMemoryBind	(const DeviceInterface&			vk,
										 const VkDevice					device,
										 const VkDeviceSize				allocationSize,
										 const deUint32					memoryType,
										 const VkDeviceSize				resourceOffset,
										 const VkSparseMemoryBindFlags	flags)
{
	const VkMemoryAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	//	VkStructureType	sType;
		DE_NULL,								//	const void*		pNext;
		allocationSize,							//	VkDeviceSize	allocationSize;
		memoryType,								//	deUint32		memoryTypeIndex;
	};

	VkDeviceMemory deviceMemory = 0;
	VK_CHECK(vk.allocateMemory(device, &allocInfo, DE_NULL, &deviceMemory));

	VkSparseMemoryBind memoryBind;

	memoryBind.resourceOffset	= resourceOffset;
	memoryBind.size				= allocationSize;
	memoryBind.memory			= deviceMemory;
	memoryBind.memoryOffset		= 0u;
	memoryBind.flags			= flags;

	return memoryBind;
}

void requireFeatures (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const FeatureFlags flags)
{
	const VkPhysicalDeviceFeatures features = getPhysicalDeviceFeatures(vki, physDevice);

	if (((flags & FEATURE_TESSELLATION_SHADER) != 0) && !features.tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader not supported");

	if (((flags & FEATURE_GEOMETRY_SHADER) != 0) && !features.geometryShader)
		throw tcu::NotSupportedError("Geometry shader not supported");

	if (((flags & FEATURE_SHADER_FLOAT_64) != 0) && !features.shaderFloat64)
		throw tcu::NotSupportedError("Double-precision floats not supported");

	if (((flags & FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS) != 0) && !features.vertexPipelineStoresAndAtomics)
		throw tcu::NotSupportedError("SSBO and image writes not supported in vertex pipeline");

	if (((flags & FEATURE_FRAGMENT_STORES_AND_ATOMICS) != 0) && !features.fragmentStoresAndAtomics)
		throw tcu::NotSupportedError("SSBO and image writes not supported in fragment shader");

	if (((flags & FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE) != 0) && !features.shaderTessellationAndGeometryPointSize)
		throw tcu::NotSupportedError("Tessellation and geometry shaders don't support PointSize built-in");
}

deUint32 findMatchingMemoryType (const InstanceInterface&		instance,
								 const VkPhysicalDevice			physicalDevice,
								 const VkMemoryRequirements&	objectMemoryRequirements,
								 const MemoryRequirement&		memoryRequirement)
{
	const VkPhysicalDeviceMemoryProperties deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < deviceMemoryProperties.memoryTypeCount; ++memoryTypeNdx)
	{
		if ((objectMemoryRequirements.memoryTypeBits & (1u << memoryTypeNdx)) != 0 &&
			memoryRequirement.matchesHeap(deviceMemoryProperties.memoryTypes[memoryTypeNdx].propertyFlags))
		{
			return memoryTypeNdx;
		}
	}

	return NO_MATCH_FOUND;
}

deUint32 getHeapIndexForMemoryType (const InstanceInterface&	instance,
									const VkPhysicalDevice		physicalDevice,
									const deUint32				memoryType)
{
	const VkPhysicalDeviceMemoryProperties deviceMemoryProperties = getPhysicalDeviceMemoryProperties(instance, physicalDevice);
	DE_ASSERT(memoryType < deviceMemoryProperties.memoryTypeCount);
	return deviceMemoryProperties.memoryTypes[memoryType].heapIndex;
}

bool checkSparseSupportForImageType (const InstanceInterface&	instance,
									 const VkPhysicalDevice		physicalDevice,
									 const ImageType			imageType)
{
	const VkPhysicalDeviceFeatures deviceFeatures = getPhysicalDeviceFeatures(instance, physicalDevice);

	if (!deviceFeatures.sparseBinding)
		return false;

	switch (mapImageType(imageType))
	{
		case VK_IMAGE_TYPE_2D:
			return deviceFeatures.sparseResidencyImage2D == VK_TRUE;
		case VK_IMAGE_TYPE_3D:
			return deviceFeatures.sparseResidencyImage3D == VK_TRUE;
		default:
			DE_FATAL("Unexpected image type");
			return false;
	};
}

bool checkSparseSupportForImageFormat (const InstanceInterface&	instance,
									   const VkPhysicalDevice	physicalDevice,
									   const VkImageCreateInfo&	imageInfo)
{
	const std::vector<VkSparseImageFormatProperties> sparseImageFormatPropVec = getPhysicalDeviceSparseImageFormatProperties(
		instance, physicalDevice, imageInfo.format, imageInfo.imageType, imageInfo.samples, imageInfo.usage, imageInfo.tiling);

	return sparseImageFormatPropVec.size() > 0u;
}

bool checkImageFormatFeatureSupport (const InstanceInterface&	instance,
									 const VkPhysicalDevice		physicalDevice,
									 const VkFormat				format,
									 const VkFormatFeatureFlags	featureFlags)
{
	const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(instance, physicalDevice, format);

	return (formatProperties.optimalTilingFeatures & featureFlags) == featureFlags;
}

deUint32 getSparseAspectRequirementsIndex (const std::vector<VkSparseImageMemoryRequirements>&	requirements,
										   const VkImageAspectFlags								aspectFlags)
{
	for (deUint32 memoryReqNdx = 0; memoryReqNdx < requirements.size(); ++memoryReqNdx)
	{
		if (requirements[memoryReqNdx].formatProperties.aspectMask & aspectFlags)
			return memoryReqNdx;
	}

	return NO_MATCH_FOUND;
}

vk::VkFormat getPlaneCompatibleFormatForWriting(const vk::PlanarFormatDescription& formatInfo, deUint32 planeNdx)
{
	DE_ASSERT(planeNdx < formatInfo.numPlanes);
	vk::VkFormat result = formatInfo.planes[planeNdx].planeCompatibleFormat;

	// redirect result for some of the YCbCr image formats
	static const std::pair<vk::VkFormat, vk::VkFormat> ycbcrFormats[] =
	{
		{ VK_FORMAT_G8B8G8R8_422_UNORM_KHR,						VK_FORMAT_R8G8B8A8_UNORM		},
		{ VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_G16B16G16R16_422_UNORM_KHR,					VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_B8G8R8G8_422_UNORM_KHR,						VK_FORMAT_R8G8B8A8_UNORM		},
		{ VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR,	VK_FORMAT_R16G16B16A16_UNORM	},
		{ VK_FORMAT_B16G16R16G16_422_UNORM_KHR,					VK_FORMAT_R16G16B16A16_UNORM	}
	};
	auto it = std::find_if(std::begin(ycbcrFormats), std::end(ycbcrFormats), [result](const std::pair<vk::VkFormat, vk::VkFormat>& p) { return p.first == result; });
	if (it != std::end(ycbcrFormats))
		result = it->second;
	return result;
}

} // sparse
} // vkt
