/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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

#include "vktPipelineImageUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuAstcUtil.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

/*! Gets the next multiple of a given divisor */
static deUint32 getNextMultiple (deUint32 divisor, deUint32 value)
{
	if (value % divisor == 0)
	{
		return value;
	}
	return value + divisor - (value % divisor);
}

/*! Gets the next value that is multiple of all given divisors */
static deUint32 getNextMultiple (const std::vector<deUint32>& divisors, deUint32 value)
{
	deUint32	nextMultiple		= value;
	bool		nextMultipleFound	= false;

	while (true)
	{
		nextMultipleFound = true;

		for (size_t divNdx = 0; divNdx < divisors.size(); divNdx++)
			nextMultipleFound = nextMultipleFound && (nextMultiple % divisors[divNdx] == 0);

		if (nextMultipleFound)
			break;

		DE_ASSERT(nextMultiple < ~((deUint32)0u));
		nextMultiple = getNextMultiple(divisors[0], nextMultiple + 1);
	}

	return nextMultiple;
}

bool isSupportedSamplableFormat (const InstanceInterface& instanceInterface, VkPhysicalDevice device, VkFormat format)
{
	if (isCompressedFormat(format))
	{
		VkPhysicalDeviceFeatures		physicalFeatures;
		const tcu::CompressedTexFormat	compressedFormat	= mapVkCompressedFormat(format);

		instanceInterface.getPhysicalDeviceFeatures(device, &physicalFeatures);

		if (tcu::isAstcFormat(compressedFormat))
		{
			if (!physicalFeatures.textureCompressionASTC_LDR)
				return false;
		}
		else if (tcu::isEtcFormat(compressedFormat))
		{
			if (!physicalFeatures.textureCompressionETC2)
				return false;
		}
		else
		{
			DE_FATAL("Unsupported compressed format");
		}
	}

	VkFormatProperties	formatProps;
	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0u;
}

// \todo [2016-01-21 pyry] Update this to just rely on vkDefs.hpp once
//						   CTS has been updated to 1.0.2.
enum
{
	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT = 0x00001000,
};

bool isLinearFilteringSupported (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkFormat format, VkImageTiling tiling)
{
	const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(vki, physicalDevice, format);
	const VkFormatFeatureFlags	formatFeatures		= tiling == VK_IMAGE_TILING_LINEAR
													? formatProperties.linearTilingFeatures
													: formatProperties.optimalTilingFeatures;

	switch (format)
	{
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return (formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;

		default:
			// \todo [2016-01-21 pyry] Check for all formats once drivers have been updated to 1.0.2
			//						   and we have tests to verify format properties.
			return true;
	}
}

VkBorderColor getFormatBorderColor (BorderColor color, VkFormat format)
{
	if (!isCompressedFormat(format) && (isIntFormat(format) || isUintFormat(format)))
	{
		switch (color)
		{
			case BORDER_COLOR_OPAQUE_BLACK:			return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			case BORDER_COLOR_OPAQUE_WHITE:			return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
			case BORDER_COLOR_TRANSPARENT_BLACK:	return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
			default:
				break;
		}
	}
	else
	{
		switch (color)
		{
			case BORDER_COLOR_OPAQUE_BLACK:			return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			case BORDER_COLOR_OPAQUE_WHITE:			return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			case BORDER_COLOR_TRANSPARENT_BLACK:	return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			default:
				break;
		}
	}

	DE_ASSERT(false);
	return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
}

de::MovePtr<tcu::TextureLevel> readColorAttachment (const vk::DeviceInterface&	vk,
													vk::VkDevice				device,
													vk::VkQueue					queue,
													deUint32					queueFamilyIndex,
													vk::Allocator&				allocator,
													vk::VkImage					image,
													vk::VkFormat				format,
													const tcu::UVec2&			renderSize)
{
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	Move<VkCommandPool>				cmdPool;
	Move<VkCommandBuffer>			cmdBuffer;
	Move<VkFence>					fence;
	const tcu::TextureFormat		tcuFormat		= mapVkFormat(format);
	const VkDeviceSize				pixelDataSize	= renderSize.x() * renderSize.y() * tcuFormat.getPixelSize();
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(tcuFormat, renderSize.x(), renderSize.y()));

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			pixelDataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,										// const void*			pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCmdPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32				queueFamilyIndex;
		};

		cmdPool = createCommandPool(vk, device, &cmdPoolParams);

		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u												// deUint32					bufferCount;
		};

		cmdBuffer = allocateCommandBuffer(vk, device, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u											// VkFenceCreateFlags	flags;
		};

		fence = createFence(vk, device, &fenceParams);
	}

	// Barriers for copying image to buffer

	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			1u							// deUint32				arraySize;
		}
	};

	const VkBufferMemoryBarrier bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*buffer,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		pixelDataSize								// VkDeviceSize		size;
	};

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,			// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	// Copy image to buffer

	const VkBufferImageCopy copyRegion =
	{
		0u,												// VkDeviceSize				bufferOffset;
		(deUint32)renderSize.x(),						// deUint32					bufferRowLength;
		(deUint32)renderSize.y(),						// deUint32					bufferImageHeight;
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u },		// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },									// VkOffset3D				imageOffset;
		{ renderSize.x(), renderSize.y(), 1u }			// VkExtent3D				imageExtent;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);
	vk.cmdCopyImageToBuffer(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1, &fence.get(), 0, ~(0ull) /* infinity */));

	// Read buffer data
	invalidateMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), bufferAlloc->getHostPtr()));

	return resultLevel;
}

void uploadTestTexture (const DeviceInterface&			vk,
						VkDevice						device,
						VkQueue							queue,
						deUint32						queueFamilyIndex,
						Allocator&						allocator,
						const TestTexture&				srcTexture,
						VkImage							destImage)
{
	deUint32						bufferSize;
	Move<VkBuffer>					buffer;
	de::MovePtr<Allocation>			bufferAlloc;
	Move<VkCommandPool>				cmdPool;
	Move<VkCommandBuffer>			cmdBuffer;
	Move<VkFence>					fence;
	std::vector<deUint32>			levelDataSizes;

	// Calculate buffer size
	bufferSize =  (srcTexture.isCompressed())? srcTexture.getCompressedSize(): srcTexture.getSize();

	// Create source buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			bufferSize,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCommandPoolCreateFlags	flags;
			queueFamilyIndex,								// deUint32					queueFamilyIndex;
		};

		cmdPool = createCommandPool(vk, device, &cmdPoolParams);

		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,												// deUint32					bufferCount;
		};

		cmdBuffer = allocateCommandBuffer(vk, device, &cmdBufferAllocateInfo);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u											// VkFenceCreateFlags	flags;
		};

		fence = createFence(vk, device, &fenceParams);
	}

	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier preBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,					// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*buffer,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		bufferSize									// VkDeviceSize		size;
	};

	const VkImageMemoryBarrier preImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			(deUint32)srcTexture.getNumLevels(),	// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			(deUint32)srcTexture.getArraySize(),	// deUint32			arraySize;
		}
	};

	const VkImageMemoryBarrier postImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,		// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspect	aspect;
			0u,										// deUint32			baseMipLevel;
			(deUint32)srcTexture.getNumLevels(),	// deUint32			mipLevels;
			0u,										// deUint32			baseArraySlice;
			(deUint32)srcTexture.getArraySize(),	// deUint32			arraySize;
		}
	};

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	const std::vector<VkBufferImageCopy>	copyRegions		= srcTexture.getBufferCopyRegions();

	// Write buffer data
	srcTexture.write(reinterpret_cast<deUint8*>(bufferAlloc->getHostPtr()));
	flushMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), bufferSize);

	// Copy buffer to image
	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 1, &preImageBarrier);
	vk.cmdCopyBufferToImage(*cmdBuffer, *buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegions.size(), copyRegions.data());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1, &fence.get(), true, ~(0ull) /* infinity */));
}


// Utilities for test textures

template<typename TcuTextureType>
void allocateLevels (TcuTextureType& texture)
{
	for (int levelNdx = 0; levelNdx < texture.getNumLevels(); levelNdx++)
		texture.allocLevel(levelNdx);
}

template<typename TcuTextureType>
std::vector<tcu::PixelBufferAccess> getLevelsVector (const TcuTextureType& texture)
{
	std::vector<tcu::PixelBufferAccess> levels(texture.getNumLevels());

	for (int levelNdx = 0; levelNdx < texture.getNumLevels(); levelNdx++)
		levels[levelNdx] = *reinterpret_cast<const tcu::PixelBufferAccess*>(&texture.getLevel(levelNdx));

	return levels;
}


// TestTexture

TestTexture::TestTexture (const tcu::TextureFormat& format, int width, int height, int depth)
{
	DE_ASSERT(width >= 1);
	DE_ASSERT(height >= 1);
	DE_ASSERT(depth >= 1);

	DE_UNREF(format);
	DE_UNREF(width);
	DE_UNREF(height);
	DE_UNREF(depth);
}

TestTexture::TestTexture (const tcu::CompressedTexFormat& format, int width, int height, int depth)
{
	DE_ASSERT(width >= 1);
	DE_ASSERT(height >= 1);
	DE_ASSERT(depth >= 1);

	DE_UNREF(format);
	DE_UNREF(width);
	DE_UNREF(height);
	DE_UNREF(depth);
}

TestTexture::~TestTexture (void)
{
	for (size_t levelNdx = 0; levelNdx < m_compressedLevels.size(); levelNdx++)
		delete m_compressedLevels[levelNdx];
}

deUint32 TestTexture::getSize (void) const
{
	std::vector<deUint32>	offsetMultiples;
	deUint32				textureSize = 0;

	offsetMultiples.push_back(4);
	offsetMultiples.push_back(getLevel(0, 0).getFormat().getPixelSize());

	for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
	{
		for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
		{
			const tcu::ConstPixelBufferAccess level = getLevel(levelNdx, layerNdx);
			textureSize = getNextMultiple(offsetMultiples, textureSize);
			textureSize += level.getWidth() * level.getHeight() * level.getDepth() * level.getFormat().getPixelSize();
		}
	}

	return textureSize;
}

deUint32 TestTexture::getCompressedSize (void) const
{
	if (!isCompressed())
		throw tcu::InternalError("Texture is not compressed");

	std::vector<deUint32>	offsetMultiples;
	deUint32				textureSize			= 0;

	offsetMultiples.push_back(4);
	offsetMultiples.push_back(tcu::getBlockSize(getCompressedLevel(0, 0).getFormat()));

	for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
	{
		for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
		{
			textureSize = getNextMultiple(offsetMultiples, textureSize);
			textureSize += getCompressedLevel(levelNdx, layerNdx).getDataSize();
		}
	}

	return textureSize;
}

tcu::CompressedTexture& TestTexture::getCompressedLevel (int level, int layer)
{
	DE_ASSERT(level >= 0 && level < getNumLevels());
	DE_ASSERT(layer >= 0 && layer < getArraySize());

	return *m_compressedLevels[level * getArraySize() + layer];
}

const tcu::CompressedTexture& TestTexture::getCompressedLevel (int level, int layer) const
{
	DE_ASSERT(level >= 0 && level < getNumLevels());
	DE_ASSERT(layer >= 0 && layer < getArraySize());

	return *m_compressedLevels[level * getArraySize() + layer];
}

std::vector<VkBufferImageCopy> TestTexture::getBufferCopyRegions (void) const
{
	std::vector<deUint32>			offsetMultiples;
	std::vector<VkBufferImageCopy>	regions;
	deUint32						layerDataOffset	= 0;

	offsetMultiples.push_back(4);

	if (isCompressed())
	{
		offsetMultiples.push_back(tcu::getBlockSize(getCompressedLevel(0, 0).getFormat()));

		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				const tcu::CompressedTexture& level = getCompressedLevel(levelNdx, layerNdx);
				tcu::IVec3 blockPixelSize			= getBlockPixelSize(level.getFormat());
				layerDataOffset						= getNextMultiple(offsetMultiples, layerDataOffset);

				const VkBufferImageCopy layerRegion =
				{
					layerDataOffset,													// VkDeviceSize				bufferOffset;
					(deUint32)getNextMultiple(blockPixelSize.x(), level.getWidth()),	// deUint32					bufferRowLength;
					(deUint32)getNextMultiple(blockPixelSize.y(), level.getHeight()),	// deUint32					bufferImageHeight;
					{																	// VkImageSubresourceLayers	imageSubresource;
						VK_IMAGE_ASPECT_COLOR_BIT,
						(deUint32)levelNdx,
						(deUint32)layerNdx,
						1u
					},
					{ 0u, 0u, 0u },							// VkOffset3D				imageOffset;
					{										// VkExtent3D				imageExtent;
						(deUint32)level.getWidth(),
						(deUint32)level.getHeight(),
						(deUint32)level.getDepth()
					}
				};

				regions.push_back(layerRegion);
				layerDataOffset += level.getDataSize();
			}
		}
	}
	else
	{
		offsetMultiples.push_back(getLevel(0, 0).getFormat().getPixelSize());

		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				const tcu::ConstPixelBufferAccess level = getLevel(levelNdx, layerNdx);

				layerDataOffset = getNextMultiple(offsetMultiples, layerDataOffset);

				const VkBufferImageCopy layerRegion =
				{
					layerDataOffset,						// VkDeviceSize				bufferOffset;
					(deUint32)level.getWidth(),				// deUint32					bufferRowLength;
					(deUint32)level.getHeight(),			// deUint32					bufferImageHeight;
					{										// VkImageSubresourceLayers	imageSubresource;
						VK_IMAGE_ASPECT_COLOR_BIT,
						(deUint32)levelNdx,
						(deUint32)layerNdx,
						1u
					},
					{ 0u, 0u, 0u },							// VkOffset3D			imageOffset;
					{										// VkExtent3D			imageExtent;
						(deUint32)level.getWidth(),
						(deUint32)level.getHeight(),
						(deUint32)level.getDepth()
					}
				};

				regions.push_back(layerRegion);
				layerDataOffset += level.getWidth() * level.getHeight() * level.getDepth() * level.getFormat().getPixelSize();
			}
		}
	}

	return regions;
}

void TestTexture::write (deUint8* destPtr) const
{
	std::vector<deUint32>	offsetMultiples;
	deUint32				levelOffset		= 0;

	offsetMultiples.push_back(4);

	if (isCompressed())
	{
		offsetMultiples.push_back(tcu::getBlockSize(getCompressedLevel(0, 0).getFormat()));

		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				levelOffset = getNextMultiple(offsetMultiples, levelOffset);

				const tcu::CompressedTexture&		compressedTex	= getCompressedLevel(levelNdx, layerNdx);

				deMemcpy(destPtr + levelOffset, compressedTex.getData(), compressedTex.getDataSize());
				levelOffset += compressedTex.getDataSize();
			}
		}
	}
	else
	{
		offsetMultiples.push_back(getLevel(0, 0).getFormat().getPixelSize());

		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				levelOffset = getNextMultiple(offsetMultiples, levelOffset);

				const tcu::ConstPixelBufferAccess	srcAccess		= getLevel(levelNdx, layerNdx);
				const tcu::PixelBufferAccess		destAccess		(srcAccess.getFormat(), srcAccess.getSize(), srcAccess.getPitch(), destPtr + levelOffset);

				tcu::copy(destAccess, srcAccess);
				levelOffset += srcAccess.getWidth() * srcAccess.getHeight() * srcAccess.getDepth() * srcAccess.getFormat().getPixelSize();
			}
		}
	}
}

void TestTexture::populateLevels (const std::vector<tcu::PixelBufferAccess>& levels)
{
	for (size_t levelNdx = 0; levelNdx < levels.size(); levelNdx++)
		TestTexture::fillWithGradient(levels[levelNdx]);
}

void TestTexture::populateCompressedLevels (tcu::CompressedTexFormat format, const std::vector<tcu::PixelBufferAccess>& decompressedLevels)
{
	// Generate random compressed data and update decompressed data

	de::Random random(123);

	for (size_t levelNdx = 0; levelNdx < decompressedLevels.size(); levelNdx++)
	{
		const tcu::PixelBufferAccess	level				= decompressedLevels[levelNdx];
		tcu::CompressedTexture*			compressedLevel		= new tcu::CompressedTexture(format, level.getWidth(), level.getHeight(), level.getDepth());
		deUint8* const					compressedData		= (deUint8*)compressedLevel->getData();

		if (tcu::isAstcFormat(format))
		{
			// \todo [2016-01-20 pyry] Comparison doesn't currently handle invalid blocks correctly so we use only valid blocks
			tcu::astc::generateRandomValidBlocks(compressedData, compressedLevel->getDataSize()/tcu::astc::BLOCK_SIZE_BYTES,
												 format, tcu::TexDecompressionParams::ASTCMODE_LDR, random.getUint32());
		}
		else
		{
			// Generate random compressed data
			for (int byteNdx = 0; byteNdx < compressedLevel->getDataSize(); byteNdx++)
				compressedData[byteNdx] = 0xFF & random.getUint32();
		}

		m_compressedLevels.push_back(compressedLevel);

		// Store decompressed data
		compressedLevel->decompress(level, tcu::TexDecompressionParams(tcu::TexDecompressionParams::ASTCMODE_LDR));
	}
}

void TestTexture::fillWithGradient (const tcu::PixelBufferAccess& levelAccess)
{
	const tcu::TextureFormatInfo formatInfo = tcu::getTextureFormatInfo(levelAccess.getFormat());
	tcu::fillWithComponentGradients(levelAccess, formatInfo.valueMin, formatInfo.valueMax);
}

// TestTexture1D

TestTexture1D::TestTexture1D (const tcu::TextureFormat& format, int width)
	: TestTexture	(format, width, 1, 1)
	, m_texture		(format, width)
{
	allocateLevels(m_texture);
	TestTexture::populateLevels(getLevelsVector(m_texture));
}

TestTexture1D::TestTexture1D (const tcu::CompressedTexFormat& format, int width)
	: TestTexture	(format, width, 1, 1)
	, m_texture		(tcu::getUncompressedFormat(format), width)
{
	allocateLevels(m_texture);
	TestTexture::populateCompressedLevels(format, getLevelsVector(m_texture));
}

TestTexture1D::~TestTexture1D (void)
{
}

int TestTexture1D::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTexture1D::getLevel (int level, int layer)
{
	DE_ASSERT(layer == 0);
	DE_UNREF(layer);
	return m_texture.getLevel(level);
}

const tcu::ConstPixelBufferAccess TestTexture1D::getLevel (int level, int layer) const
{
	DE_ASSERT(layer == 0);
	DE_UNREF(layer);
	return m_texture.getLevel(level);
}

const tcu::Texture1D& TestTexture1D::getTexture (void) const
{
	return m_texture;
}


// TestTexture1DArray

TestTexture1DArray::TestTexture1DArray (const tcu::TextureFormat& format, int width, int arraySize)
	: TestTexture	(format, width, 1, arraySize)
	, m_texture		(format, width, arraySize)
{
	allocateLevels(m_texture);
	TestTexture::populateLevels(getLevelsVector(m_texture));
}

TestTexture1DArray::TestTexture1DArray (const tcu::CompressedTexFormat& format, int width, int arraySize)
	: TestTexture	(format, width, 1, arraySize)
	, m_texture		(tcu::getUncompressedFormat(format), width, arraySize)
{
	allocateLevels(m_texture);

	std::vector<tcu::PixelBufferAccess> layers;
	for (int levelNdx = 0; levelNdx < m_texture.getNumLevels(); levelNdx++)
		for (int layerNdx = 0; layerNdx < m_texture.getNumLayers(); layerNdx++)
			layers.push_back(getLevel(levelNdx, layerNdx));

	TestTexture::populateCompressedLevels(format, layers);
}

TestTexture1DArray::~TestTexture1DArray (void)
{
}

int TestTexture1DArray::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTexture1DArray::getLevel (int level, int layer)
{
	const tcu::PixelBufferAccess	levelLayers	= m_texture.getLevel(level);
	const deUint32					layerSize	= levelLayers.getWidth() * levelLayers.getFormat().getPixelSize();
	const deUint32					layerOffset	= layerSize * layer;

	return tcu::PixelBufferAccess(levelLayers.getFormat(), levelLayers.getWidth(), 1, 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
}

const tcu::ConstPixelBufferAccess TestTexture1DArray::getLevel (int level, int layer) const
{
	const tcu::ConstPixelBufferAccess	levelLayers	= m_texture.getLevel(level);
	const deUint32						layerSize	= levelLayers.getWidth() * levelLayers.getFormat().getPixelSize();
	const deUint32						layerOffset	= layerSize * layer;

	return tcu::ConstPixelBufferAccess(levelLayers.getFormat(), levelLayers.getWidth(), 1, 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
}

const tcu::Texture1DArray& TestTexture1DArray::getTexture (void) const
{
	return m_texture;
}

int TestTexture1DArray::getArraySize (void) const
{
	return m_texture.getNumLayers();
}


// TestTexture2D

TestTexture2D::TestTexture2D (const tcu::TextureFormat& format, int width, int height)
	: TestTexture	(format, width, height, 1)
	, m_texture		(format, width, height)
{
	allocateLevels(m_texture);
	TestTexture::populateLevels(getLevelsVector(m_texture));
}

TestTexture2D::TestTexture2D (const tcu::CompressedTexFormat& format, int width, int height)
	: TestTexture	(format, width, height, 1)
	, m_texture		(tcu::getUncompressedFormat(format), width, height)
{
	allocateLevels(m_texture);
	TestTexture::populateCompressedLevels(format, getLevelsVector(m_texture));
}

TestTexture2D::~TestTexture2D (void)
{
}

int TestTexture2D::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTexture2D::getLevel (int level, int layer)
{
	DE_ASSERT(layer == 0);
	DE_UNREF(layer);
	return m_texture.getLevel(level);
}

const tcu::ConstPixelBufferAccess TestTexture2D::getLevel (int level, int layer) const
{
	DE_ASSERT(layer == 0);
	DE_UNREF(layer);
	return m_texture.getLevel(level);
}

const tcu::Texture2D& TestTexture2D::getTexture (void) const
{
	return m_texture;
}


// TestTexture2DArray

TestTexture2DArray::TestTexture2DArray (const tcu::TextureFormat& format, int width, int height, int arraySize)
	: TestTexture	(format, width, height, arraySize)
	, m_texture		(format, width, height, arraySize)
{
	allocateLevels(m_texture);
	TestTexture::populateLevels(getLevelsVector(m_texture));
}

TestTexture2DArray::TestTexture2DArray (const tcu::CompressedTexFormat& format, int width, int height, int arraySize)
	: TestTexture	(format, width, height, arraySize)
	, m_texture		(tcu::getUncompressedFormat(format), width, height, arraySize)
{
	allocateLevels(m_texture);

	std::vector<tcu::PixelBufferAccess> layers;
	for (int levelNdx = 0; levelNdx < m_texture.getNumLevels(); levelNdx++)
		for (int layerNdx = 0; layerNdx < m_texture.getNumLayers(); layerNdx++)
			layers.push_back(getLevel(levelNdx, layerNdx));

	TestTexture::populateCompressedLevels(format, layers);
}

TestTexture2DArray::~TestTexture2DArray (void)
{
}

int TestTexture2DArray::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTexture2DArray::getLevel (int level, int layer)
{
	const tcu::PixelBufferAccess	levelLayers	= m_texture.getLevel(level);
	const deUint32					layerSize	= levelLayers.getWidth() * levelLayers.getHeight() * levelLayers.getFormat().getPixelSize();
	const deUint32					layerOffset	= layerSize * layer;

	return tcu::PixelBufferAccess(levelLayers.getFormat(), levelLayers.getWidth(), levelLayers.getHeight(), 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
}

const tcu::ConstPixelBufferAccess TestTexture2DArray::getLevel (int level, int layer) const
{
	const tcu::ConstPixelBufferAccess	levelLayers	= m_texture.getLevel(level);
	const deUint32						layerSize	= levelLayers.getWidth() * levelLayers.getHeight() * levelLayers.getFormat().getPixelSize();
	const deUint32						layerOffset	= layerSize * layer;

	return tcu::ConstPixelBufferAccess(levelLayers.getFormat(), levelLayers.getWidth(), levelLayers.getHeight(), 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
}

const tcu::Texture2DArray& TestTexture2DArray::getTexture (void) const
{
	return m_texture;
}

int TestTexture2DArray::getArraySize (void) const
{
	return m_texture.getNumLayers();
}


// TestTexture3D

TestTexture3D::TestTexture3D (const tcu::TextureFormat& format, int width, int height, int depth)
	: TestTexture	(format, width, height, depth)
	, m_texture		(format, width, height, depth)
{
	allocateLevels(m_texture);
	TestTexture::populateLevels(getLevelsVector(m_texture));
}

TestTexture3D::TestTexture3D (const tcu::CompressedTexFormat& format, int width, int height, int depth)
	: TestTexture	(format, width, height, depth)
	, m_texture		(tcu::getUncompressedFormat(format), width, height, depth)
{
	allocateLevels(m_texture);
	TestTexture::populateCompressedLevels(format, getLevelsVector(m_texture));
}

TestTexture3D::~TestTexture3D (void)
{
}

int TestTexture3D::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTexture3D::getLevel (int level, int layer)
{
	DE_ASSERT(layer == 0);
	DE_UNREF(layer);
	return m_texture.getLevel(level);
}

const tcu::ConstPixelBufferAccess TestTexture3D::getLevel (int level, int layer) const
{
	DE_ASSERT(layer == 0);
	DE_UNREF(layer);
	return m_texture.getLevel(level);
}

const tcu::Texture3D& TestTexture3D::getTexture (void) const
{
	return m_texture;
}


// TestTextureCube

const static tcu::CubeFace tcuFaceMapping[tcu::CUBEFACE_LAST] =
{
	tcu::CUBEFACE_POSITIVE_X,
	tcu::CUBEFACE_NEGATIVE_X,
	tcu::CUBEFACE_POSITIVE_Y,
	tcu::CUBEFACE_NEGATIVE_Y,
	tcu::CUBEFACE_POSITIVE_Z,
	tcu::CUBEFACE_NEGATIVE_Z
};

TestTextureCube::TestTextureCube (const tcu::TextureFormat& format, int size)
	: TestTexture	(format, size, size, 1)
	, m_texture		(format, size)
{
	for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
	{
		for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
		{
			m_texture.allocLevel(tcuFaceMapping[faceNdx], levelNdx);
			TestTexture::fillWithGradient(m_texture.getLevelFace(levelNdx, tcuFaceMapping[faceNdx]));
		}
	}
}

TestTextureCube::TestTextureCube (const tcu::CompressedTexFormat& format, int size)
	: TestTexture	(format, size, size, 1)
	, m_texture		(tcu::getUncompressedFormat(format), size)
{
	std::vector<tcu::PixelBufferAccess> levels(m_texture.getNumLevels() * tcu::CUBEFACE_LAST);

	for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
	{
		for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; faceNdx++)
		{
			m_texture.allocLevel(tcuFaceMapping[faceNdx], levelNdx);
			levels[levelNdx * tcu::CUBEFACE_LAST + faceNdx] = m_texture.getLevelFace(levelNdx, tcuFaceMapping[faceNdx]);
		}
	}

	TestTexture::populateCompressedLevels(format, levels);
}

TestTextureCube::~TestTextureCube (void)
{
}

int TestTextureCube::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTextureCube::getLevel (int level, int face)
{
	return m_texture.getLevelFace(level, (tcu::CubeFace)face);
}

const tcu::ConstPixelBufferAccess TestTextureCube::getLevel (int level, int face) const
{
	return m_texture.getLevelFace(level, (tcu::CubeFace)face);
}

int TestTextureCube::getArraySize (void) const
{
	return (int)tcu::CUBEFACE_LAST;
}

const tcu::TextureCube& TestTextureCube::getTexture (void) const
{
	return m_texture;
}

// TestTextureCubeArray

TestTextureCubeArray::TestTextureCubeArray (const tcu::TextureFormat& format, int size, int arraySize)
	: TestTexture	(format, size, size, arraySize)
	, m_texture		(format, size, arraySize)
{
	allocateLevels(m_texture);
	TestTexture::populateLevels(getLevelsVector(m_texture));
}

TestTextureCubeArray::TestTextureCubeArray (const tcu::CompressedTexFormat& format, int size, int arraySize)
	: TestTexture	(format, size, size, arraySize)
	, m_texture		(tcu::getUncompressedFormat(format), size, arraySize)
{
	DE_ASSERT(arraySize % 6 == 0);

	allocateLevels(m_texture);

	std::vector<tcu::PixelBufferAccess> layers;
	for (int levelNdx = 0; levelNdx < m_texture.getNumLevels(); levelNdx++)
		for (int layerNdx = 0; layerNdx < m_texture.getDepth(); layerNdx++)
			layers.push_back(getLevel(levelNdx, layerNdx));

	TestTexture::populateCompressedLevels(format, layers);
}

TestTextureCubeArray::~TestTextureCubeArray (void)
{
}

int TestTextureCubeArray::getNumLevels (void) const
{
	return m_texture.getNumLevels();
}

tcu::PixelBufferAccess TestTextureCubeArray::getLevel (int level, int layer)
{
	const tcu::PixelBufferAccess	levelLayers	= m_texture.getLevel(level);
	const deUint32					layerSize	= levelLayers.getWidth() * levelLayers.getHeight() * levelLayers.getFormat().getPixelSize();
	const deUint32					layerOffset	= layerSize * layer;

	return tcu::PixelBufferAccess(levelLayers.getFormat(), levelLayers.getWidth(), levelLayers.getHeight(), 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
}

const tcu::ConstPixelBufferAccess TestTextureCubeArray::getLevel (int level, int layer) const
{
	const tcu::ConstPixelBufferAccess	levelLayers	= m_texture.getLevel(level);
	const deUint32						layerSize	= levelLayers.getWidth() * levelLayers.getHeight() * levelLayers.getFormat().getPixelSize();
	const deUint32						layerOffset	= layerSize * layer;

	return tcu::ConstPixelBufferAccess(levelLayers.getFormat(), levelLayers.getWidth(), levelLayers.getHeight(), 1, (deUint8*)levelLayers.getDataPtr() + layerOffset);
}

int TestTextureCubeArray::getArraySize (void) const
{
	return m_texture.getDepth();
}

const tcu::TextureCubeArray& TestTextureCubeArray::getTexture (void) const
{
	return m_texture;
}

} // pipeline
} // vkt
