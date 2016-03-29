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
#include "vkTypeUtil.hpp"
#include "tcuTextureUtil.hpp"

#include <deMath.h>

using namespace vk;

namespace vkt
{
namespace sparse
{

Buffer::Buffer (const DeviceInterface&		vk,
				const VkDevice				device,
				Allocator&					allocator,
				const VkBufferCreateInfo&	bufferCreateInfo,
				const MemoryRequirement		memoryRequirement)
	: m_buffer		(createBuffer(vk, device, &bufferCreateInfo))
	, m_allocation	(allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement))
{
	VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
}

Image::Image (const DeviceInterface&	vk,
			  const VkDevice			device,
			  Allocator&				allocator,
			  const VkImageCreateInfo&	imageCreateInfo,
			  const MemoryRequirement	memoryRequirement)
	: m_image		(createImage(vk, device, &imageCreateInfo))
	, m_allocation	(allocator.allocate(getImageMemoryRequirements(vk, device, *m_image), memoryRequirement))
{
	VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));
}

tcu::UVec3 getShaderGridSize(const ImageType imageType, const tcu::UVec3& imageSize, const deUint32 mipLevel)
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

tcu::UVec3 getLayerSize(const ImageType imageType, const tcu::UVec3& imageSize)
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

deUint32 getNumLayers(const ImageType imageType, const tcu::UVec3& imageSize)
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

deUint32 getNumPixels(const ImageType imageType, const tcu::UVec3& imageSize)
{
	const tcu::UVec3 gridSize = getShaderGridSize(imageType, imageSize);

	return gridSize.x() * gridSize.y() * gridSize.z();
}

deUint32 getDimensions(const ImageType imageType)
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

deUint32 getLayerDimensions(const ImageType imageType)
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

bool isImageSizeSupported(const ImageType imageType, const tcu::UVec3& imageSize, const vk::VkPhysicalDeviceLimits& limits)
{
	switch (imageType)
	{
		case IMAGE_TYPE_1D:
			return	imageSize.x() <= limits.maxImageDimension1D;
		case IMAGE_TYPE_1D_ARRAY:
			return	imageSize.x() <= limits.maxImageDimension1D &&
					imageSize.z() <= limits.maxImageArrayLayers;
		case IMAGE_TYPE_2D:
			return	imageSize.x() <= limits.maxImageDimension2D &&
					imageSize.y() <= limits.maxImageDimension2D;
		case IMAGE_TYPE_2D_ARRAY:
			return	imageSize.x() <= limits.maxImageDimension2D &&
					imageSize.y() <= limits.maxImageDimension2D &&
					imageSize.z() <= limits.maxImageArrayLayers;
		case IMAGE_TYPE_CUBE:
			return	imageSize.x() <= limits.maxImageDimensionCube &&
					imageSize.y() <= limits.maxImageDimensionCube;
		case IMAGE_TYPE_CUBE_ARRAY:
			return	imageSize.x() <= limits.maxImageDimensionCube &&
					imageSize.y() <= limits.maxImageDimensionCube &&
					imageSize.z() <= limits.maxImageArrayLayers;
		case IMAGE_TYPE_3D:
			return	imageSize.x() <= limits.maxImageDimension3D &&
					imageSize.y() <= limits.maxImageDimension3D &&
					imageSize.z() <= limits.maxImageDimension3D;
		case IMAGE_TYPE_BUFFER:
			return true;
		default:
			DE_FATAL("Unknown image type");
			return false;
	}
}

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize			bufferSize,
										 const VkBufferUsageFlags	usage)
{
	const VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkBufferCreateFlags	flags;
		bufferSize,								// VkDeviceSize			size;
		usage,									// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		DE_NULL,								// const deUint32*		pQueueFamilyIndices;
	};
	return bufferCreateInfo;
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

Move<VkCommandPool> makeCommandPool (const DeviceInterface& vk, const VkDevice device, const deUint32 queueFamilyIndex)
{
	const VkCommandPoolCreateInfo commandPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,									// deUint32					queueFamilyIndex;
	};
	return createCommandPool(vk, device, &commandPoolParams);
}

Move<VkCommandBuffer> makeCommandBuffer (const DeviceInterface& vk, const VkDevice device, const VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo bufferAllocateParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		commandPool,										// VkCommandPool			commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// VkCommandBufferLevel		level;
		1u,													// deUint32					bufferCount;
	};
	return allocateCommandBuffer(vk, device, &bufferAllocateParams);
}

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkDescriptorSetLayout	descriptorSetLayout)
{
	const VkPipelineLayoutCreateInfo pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							setLayoutCount;
		&descriptorSetLayout,								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};
	return createPipelineLayout(vk, device, &pipelineLayoutParams);
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&	vk,
									  const VkDevice			device,
									  const VkPipelineLayout	pipelineLayout,
									  const VkShaderModule		shaderModule)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
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

Move<VkBufferView> makeBufferView (const DeviceInterface&	vk,
								   const VkDevice			vkDevice,
								   const VkBuffer			buffer,
								   const VkFormat			format,
								   const VkDeviceSize		offset,
								   const VkDeviceSize		size)
{
	const VkBufferViewCreateInfo bufferViewParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkBufferViewCreateFlags	flags;
		buffer,										// VkBuffer					buffer;
		format,										// VkFormat					format;
		offset,										// VkDeviceSize				offset;
		size,										// VkDeviceSize				range;
	};
	return createBufferView(vk, vkDevice, &bufferViewParams);
}

Move<VkImageView> makeImageView (const DeviceInterface&			vk,
								 const VkDevice					vkDevice,
								 const VkImage					image,
								 const VkImageViewType			imageViewType,
								 const VkFormat					format,
								 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo imageViewParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkImageViewCreateFlags	flags;
		image,											// VkImage					image;
		imageViewType,									// VkImageViewType			viewType;
		format,											// VkFormat					format;
		makeComponentMappingRGBA(),						// VkComponentMapping		components;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return createImageView(vk, vkDevice, &imageViewParams);
}

Move<VkDescriptorSet> makeDescriptorSet (const DeviceInterface&			vk,
										 const VkDevice					device,
										 const VkDescriptorPool			descriptorPool,
										 const VkDescriptorSetLayout	setLayout)
{
	const VkDescriptorSetAllocateInfo allocateParams =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		descriptorPool,										// VkDescriptorPool				descriptorPool;
		1u,													// deUint32						setLayoutCount;
		&setLayout,											// const VkDescriptorSetLayout*	pSetLayouts;
	};
	return allocateDescriptorSet(vk, device, &allocateParams);
}

Move<VkSemaphore> makeSemaphore (const DeviceInterface& vk, const VkDevice device)
{
	const VkSemaphoreCreateInfo semaphoreCreateInfo =
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		DE_NULL,
		0u
	};

	return createSemaphore(vk, device, &semaphoreCreateInfo);
}

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes)
{
	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		srcAccessMask,								// VkAccessFlags	srcAccessMask;
		dstAccessMask,								// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			destQueueFamilyIndex;
		buffer,										// VkBuffer			buffer;
		offset,										// VkDeviceSize		offset;
		bufferSizeBytes,							// VkDeviceSize		size;
	};
	return barrier;
}

VkImageMemoryBarrier makeImageMemoryBarrier	(const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		srcAccessMask,									// VkAccessFlags			outputMask;
		dstAccessMask,									// VkAccessFlags			inputMask;
		oldLayout,										// VkImageLayout			oldLayout;
		newLayout,										// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

vk::VkMemoryBarrier makeMemoryBarrier (const vk::VkAccessFlags	srcAccessMask,
									   const vk::VkAccessFlags	dstAccessMask)
{
	const VkMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		srcAccessMask,						// VkAccessFlags			outputMask;
		dstAccessMask,						// VkAccessFlags			inputMask;
	};
	return barrier;
}

void beginCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		0u,												// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &commandBufBeginParams));
}

void endCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	VK_CHECK(vk.endCommandBuffer(commandBuffer));
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
							const VkSemaphore*			pSignalSemaphores)
{
	const VkFenceCreateInfo	fenceParams =
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkFenceCreateFlags	flags;
	};
	const Unique<VkFence> fence(createFence(vk, device, &fenceParams));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType				sType;
		DE_NULL,							// const void*					pNext;
		waitSemaphoreCount,					// deUint32						waitSemaphoreCount;
		pWaitSemaphores,					// const VkSemaphore*			pWaitSemaphores;
		pWaitDstStageMask,					// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,									// deUint32						commandBufferCount;
		&commandBuffer,						// const VkCommandBuffer*		pCommandBuffers;
		signalSemaphoreCount,				// deUint32						signalSemaphoreCount;
		pSignalSemaphores,					// const VkSemaphore*			pSignalSemaphores;
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
			DE_ASSERT(false);
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
			DE_ASSERT(false);
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
			DE_ASSERT(false);
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
			DE_ASSERT(false);
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
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			return "vec4";
		default:
			DE_ASSERT(false);
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
			DE_ASSERT(false);
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
			DE_ASSERT(false);
			typePart = DE_NULL;
	}

	return std::string() + orderPart + typePart;
}

VkExtent3D mipLevelExtents (const VkExtent3D& baseExtents, const deUint32 mipLevel)
{
	VkExtent3D result;

	result.width	= std::max(baseExtents.width  >> mipLevel, 1u);
	result.height	= std::max(baseExtents.height >> mipLevel, 1u);
	result.depth	= std::max(baseExtents.depth  >> mipLevel, 1u);

	return result;
}

deUint32 getImageMaxMipLevels (const VkImageFormatProperties& imageFormatProperties, const VkImageCreateInfo &imageInfo)
{
	const deUint32 widestEdge = std::max(std::max(imageInfo.extent.width, imageInfo.extent.height), imageInfo.extent.depth);

	return std::min(static_cast<deUint32>(deFloatLog2(static_cast<float>(widestEdge))) + 1u, imageFormatProperties.maxMipLevels);
}

deUint32 getImageMipLevelSizeInBytes (const VkExtent3D& baseExtents, const deUint32 layersCount, const tcu::TextureFormat& format, const deUint32 mipmapLevel)
{
	const VkExtent3D extents = mipLevelExtents(baseExtents, mipmapLevel);

	return extents.width * extents.height * extents.depth * layersCount * tcu::getPixelSize(format);
}

deUint32 getImageSizeInBytes (const VkExtent3D& baseExtents, const deUint32 layersCount, const tcu::TextureFormat& format, const deUint32 mipmapLevelsCount)
{
	deUint32 imageSizeInBytes = 0;
	for (deUint32 mipmapLevel = 0; mipmapLevel < mipmapLevelsCount; ++mipmapLevel)
	{
		imageSizeInBytes += getImageMipLevelSizeInBytes(baseExtents, layersCount, format, mipmapLevel);
	}

	return imageSizeInBytes;
}

} // sparse
} // vkt
