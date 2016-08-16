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
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		srcAccessMask,							// VkAccessFlags			outputMask;
		dstAccessMask,							// VkAccessFlags			inputMask;
		oldLayout,								// VkImageLayout			oldLayout;
		newLayout,								// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					destQueueFamilyIndex;
		image,									// VkImage					image;
		subresourceRange,						// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

VkImageMemoryBarrier makeImageMemoryBarrier (const vk::VkAccessFlags			srcAccessMask,
											 const vk::VkAccessFlags			dstAccessMask,
											 const vk::VkImageLayout			oldLayout,
											 const vk::VkImageLayout			newLayout,
											 const deUint32						srcQueueFamilyIndex,
											 const deUint32						destQueueFamilyIndex,
											 const vk::VkImage					image,
											 const vk::VkImageSubresourceRange	subresourceRange)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		srcAccessMask,							// VkAccessFlags			outputMask;
		dstAccessMask,							// VkAccessFlags			inputMask;
		oldLayout,								// VkImageLayout			oldLayout;
		newLayout,								// VkImageLayout			newLayout;
		srcQueueFamilyIndex,					// deUint32					srcQueueFamilyIndex;
		destQueueFamilyIndex,					// deUint32					destQueueFamilyIndex;
		image,									// VkImage					image;
		subresourceRange,						// VkImageSubresourceRange	subresourceRange;
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
			DE_ASSERT(0);
			return "";
	}
}

VkExtent3D mipLevelExtents (const VkExtent3D& baseExtents, const deUint32 mipLevel)
{
	VkExtent3D result;

	result.width	= std::max(baseExtents.width  >> mipLevel, 1u);
	result.height	= std::max(baseExtents.height >> mipLevel, 1u);
	result.depth	= std::max(baseExtents.depth  >> mipLevel, 1u);

	return result;
}

deUint32 getImageMaxMipLevels (const VkImageFormatProperties& imageFormatProperties, const VkExtent3D& extent)
{
	const deUint32 widestEdge = std::max(std::max(extent.width, extent.height), extent.depth);

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

VkSparseImageMemoryBind	makeSparseImageMemoryBind  (const DeviceInterface&		vk,
													const VkDevice				device,
													const VkDeviceSize			allocationSize,
													const deUint32				memoryType,
													const VkImageSubresource&	subresource,
													const VkOffset3D&			offset,
													const VkExtent3D&			extent)
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

VkSparseMemoryBind makeSparseMemoryBind	(const vk::DeviceInterface&	vk,
										 const vk::VkDevice			device,
										 const vk::VkDeviceSize		allocationSize,
										 const deUint32				memoryType,
										 const vk::VkDeviceSize		resourceOffset)
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
	memoryBind.flags			= 0u;

	return memoryBind;
}

void beginRenderPass (const DeviceInterface&			vk,
					  const VkCommandBuffer				commandBuffer,
					  const VkRenderPass				renderPass,
					  const VkFramebuffer				framebuffer,
					  const VkRect2D&					renderArea,
					  const std::vector<VkClearValue>&	clearValues)
{

	const VkRenderPassBeginInfo renderPassBeginInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType         sType;
		DE_NULL,										// const void*             pNext;
		renderPass,										// VkRenderPass            renderPass;
		framebuffer,									// VkFramebuffer           framebuffer;
		renderArea,										// VkRect2D                renderArea;
		static_cast<deUint32>(clearValues.size()),		// deUint32                clearValueCount;
		&clearValues[0],								// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void beginRenderPassWithRasterizationDisabled (const DeviceInterface&	vk,
											   const VkCommandBuffer	commandBuffer,
											   const VkRenderPass		renderPass,
											   const VkFramebuffer		framebuffer)
{
	const VkRect2D renderArea = {{ 0, 0 }, { 0, 0 }};

	const VkRenderPassBeginInfo renderPassBeginInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType         sType;
		DE_NULL,										// const void*             pNext;
		renderPass,										// VkRenderPass            renderPass;
		framebuffer,									// VkFramebuffer           framebuffer;
		renderArea,										// VkRect2D                renderArea;
		0u,												// uint32_t                clearValueCount;
		DE_NULL,										// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void endRenderPass (const DeviceInterface&	vk,
					const VkCommandBuffer	commandBuffer)
{
	vk.cmdEndRenderPass(commandBuffer);
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&	vk,
								   const VkDevice			device,
								   const VkFormat			colorFormat)
{
	const VkAttachmentDescription colorAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
		colorFormat,										// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout;
	};

	const VkAttachmentReference colorAttachmentReference =
	{
		0u,													// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
	};

	const VkAttachmentReference depthAttachmentReference =
	{
		VK_ATTACHMENT_UNUSED,								// deUint32			attachment;
		VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout	layout;
	};

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		0u,													// deUint32							inputAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
		1u,													// deUint32							colorAttachmentCount;
		&colorAttachmentReference,							// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
		&depthAttachmentReference,							// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
		1u,													// deUint32							attachmentCount;
		&colorAttachmentDescription,						// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkRenderPass> makeRenderPassWithoutAttachments (const DeviceInterface&	vk,
													 const VkDevice			device)
{
	const VkAttachmentReference unusedAttachment =
	{
		VK_ATTACHMENT_UNUSED,								// deUint32			attachment;
		VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout	layout;
	};

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		0u,													// deUint32							inputAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
		0u,													// deUint32							colorAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
		&unusedAttachment,									// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
		0u,													// deUint32							attachmentCount;
		DE_NULL,											// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&		vk,
									 const VkDevice				device,
									 const VkRenderPass			renderPass,
									 const VkImageView			colorAttachment,
									 const deUint32				width,
									 const deUint32				height,
									 const deUint32				layers)
{
	const VkFramebufferCreateInfo framebufferInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,										// const void*                                 pNext;
		(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags                    flags;
		renderPass,										// VkRenderPass                                renderPass;
		1u,												// uint32_t                                    attachmentCount;
		&colorAttachment,								// const VkImageView*                          pAttachments;
		width,											// uint32_t                                    width;
		height,											// uint32_t                                    height;
		layers,											// uint32_t                                    layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkFramebuffer> makeFramebufferWithoutAttachments (const DeviceInterface&		vk,
													   const VkDevice				device,
													   const VkRenderPass			renderPass)
{
	const VkFramebufferCreateInfo framebufferInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,										// const void*                                 pNext;
		(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags                    flags;
		renderPass,										// VkRenderPass                                renderPass;
		0u,												// uint32_t                                    attachmentCount;
		DE_NULL,										// const VkImageView*                          pAttachments;
		0u,												// uint32_t                                    width;
		0u,												// uint32_t                                    height;
		0u,												// uint32_t                                    layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::setShader (const DeviceInterface&			vk,
															 const VkDevice					device,
															 const VkShaderStageFlagBits	stage,
															 const ProgramBinary&			binary,
															 const VkSpecializationInfo*	specInfo)
{
	VkShaderModule module;
	switch (stage)
	{
		case (VK_SHADER_STAGE_VERTEX_BIT):
			DE_ASSERT(m_vertexShaderModule.get() == DE_NULL);
			m_vertexShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_vertexShaderModule;
			break;

		case (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT):
			DE_ASSERT(m_tessControlShaderModule.get() == DE_NULL);
			m_tessControlShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_tessControlShaderModule;
			break;

		case (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT):
			DE_ASSERT(m_tessEvaluationShaderModule.get() == DE_NULL);
			m_tessEvaluationShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_tessEvaluationShaderModule;
			break;

		case (VK_SHADER_STAGE_GEOMETRY_BIT):
			DE_ASSERT(m_geometryShaderModule.get() == DE_NULL);
			m_geometryShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_geometryShaderModule;
			break;

		case (VK_SHADER_STAGE_FRAGMENT_BIT):
			DE_ASSERT(m_fragmentShaderModule.get() == DE_NULL);
			m_fragmentShaderModule = createShaderModule(vk, device, binary, (VkShaderModuleCreateFlags)0);
			module = *m_fragmentShaderModule;
			break;

		default:
			DE_FATAL("Invalid shader stage");
			return *this;
	}

	const VkPipelineShaderStageCreateInfo pipelineShaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
		stage,													// VkShaderStageFlagBits				stage;
		module,													// VkShaderModule						module;
		"main",													// const char*							pName;
		specInfo,												// const VkSpecializationInfo*			pSpecializationInfo;
	};

	m_shaderStageFlags |= stage;
	m_shaderStages.push_back(pipelineShaderStageInfo);

	return *this;
}

template<typename T>
inline const T* dataPointer (const std::vector<T>& vec)
{
	return (vec.size() != 0 ? &vec[0] : DE_NULL);
}

Move<VkPipeline> GraphicsPipelineBuilder::build (const DeviceInterface&	vk,
												 const VkDevice			device,
												 const VkPipelineLayout	pipelineLayout,
												 const VkRenderPass		renderPass)
{
	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags       flags;
		static_cast<deUint32>(m_vertexInputBindings.size()),			// uint32_t                                    vertexBindingDescriptionCount;
		dataPointer(m_vertexInputBindings),								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		static_cast<deUint32>(m_vertexInputAttributes.size()),			// uint32_t                                    vertexAttributeDescriptionCount;
		dataPointer(m_vertexInputAttributes),							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const bool isTessellationEnabled = (m_shaderStageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0;

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,					// VkStructureType                             sType;
		DE_NULL,																		// const void*                                 pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,										// VkPipelineInputAssemblyStateCreateFlags     flags;
		isTessellationEnabled ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : m_primitiveTopology,	// VkPrimitiveTopology                         topology;
		VK_FALSE,																		// VkBool32                                    primitiveRestartEnable;
	};

	const VkPipelineTessellationStateCreateInfo pipelineTessellationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineTessellationStateCreateFlags)0,						// VkPipelineTessellationStateCreateFlags      flags;
		m_patchControlPoints,											// uint32_t                                    patchControlPoints;
	};

	const VkViewport viewport = makeViewport
	(
		0.0f, 0.0f,
		static_cast<float>(m_renderSize.x()), static_cast<float>(m_renderSize.y()),
		0.0f, 1.0f
	);

	const VkRect2D scissor =
	{
		makeOffset2D(0, 0),
		makeExtent2D(m_renderSize.x(), m_renderSize.y()),
	};

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,												// const void*                                 pNext;
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags;
		1u,														// uint32_t                                    viewportCount;
		&viewport,												// const VkViewport*                           pViewports;
		1u,														// uint32_t                                    scissorCount;
		&scissor,												// const VkRect2D*                             pScissors;
	};

	const bool isRasterizationDisabled = ((m_shaderStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT) == 0);

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		isRasterizationDisabled,										// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		m_cullModeFlags,												// VkCullModeFlags							cullMode;
		m_frontFace,													// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE,													// VkBool32									alphaToOneEnable;
	};

	const VkStencilOpState stencilOpState = makeStencilOpState
	(
		VK_STENCIL_OP_KEEP,		// stencil fail
		VK_STENCIL_OP_KEEP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_NEVER,	// compare op
		0u,						// compare mask
		0u,						// write mask
		0u						// reference
	);

	const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthTestEnable;
		VK_FALSE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
		stencilOpState,												// VkStencilOpState							front;
		stencilOpState,												// VkStencilOpState							back;
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentsStates;

	for (deUint32 attachmentNdx = 0; attachmentNdx < m_attachmentsCount; ++attachmentNdx)
	{
		const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
		{
			m_blendEnable,						// VkBool32					blendEnable;
			VK_BLEND_FACTOR_SRC_ALPHA,			// VkBlendFactor			srcColorBlendFactor;
			VK_BLEND_FACTOR_ONE,				// VkBlendFactor			dstColorBlendFactor;
			VK_BLEND_OP_ADD,					// VkBlendOp				colorBlendOp;
			VK_BLEND_FACTOR_SRC_ALPHA,			// VkBlendFactor			srcAlphaBlendFactor;
			VK_BLEND_FACTOR_ONE,				// VkBlendFactor			dstAlphaBlendFactor;
			VK_BLEND_OP_ADD,					// VkBlendOp				alphaBlendOp;
			colorComponentsAll,					// VkColorComponentFlags	colorWriteMask;
		};

		colorBlendAttachmentsStates.push_back(colorBlendAttachmentState);
	}

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		static_cast<deUint32>(colorBlendAttachmentsStates.size()),	// deUint32										attachmentCount;
		dataPointer(colorBlendAttachmentsStates),					// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
	};

	const bool hasDynamicState = static_cast<deUint32>(m_dynamicStates.size()) > 0u;

	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkPipelineDynamicStateCreateFlags)0,					// VkPipelineDynamicStateCreateFlags	flags;
		static_cast<deUint32>(m_dynamicStates.size()),			// deUint32								dynamicStateCount;
		dataPointer(m_dynamicStates),							// const VkDynamicState*				pDynamicStates;
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,						// VkStructureType									sType;
		DE_NULL,																// const void*										pNext;
		(VkPipelineCreateFlags)0,												// VkPipelineCreateFlags							flags;
		static_cast<deUint32>(m_shaderStages.size()),							// deUint32											stageCount;
		dataPointer(m_shaderStages),											// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,													// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,										// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		(isTessellationEnabled ? &pipelineTessellationStateInfo : DE_NULL),		// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		(isRasterizationDisabled ? DE_NULL : &pipelineViewportStateInfo),		// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,										// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		(isRasterizationDisabled ? DE_NULL : &pipelineMultisampleStateInfo),	// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		(isRasterizationDisabled ? DE_NULL : &pipelineDepthStencilStateInfo),	// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		(isRasterizationDisabled ? DE_NULL : &pipelineColorBlendStateInfo),		// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		(hasDynamicState ? &dynamicStateCreateInfo : DE_NULL),					// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,															// VkPipelineLayout									layout;
		renderPass,																// VkRenderPass										renderPass;
		0u,																		// deUint32											subpass;
		DE_NULL,																// VkPipeline										basePipelineHandle;
		0u,																		// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
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

} // sparse
} // vkt
