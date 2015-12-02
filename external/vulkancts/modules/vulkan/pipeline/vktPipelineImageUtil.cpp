/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
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
#include "deRandom.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

/*! Gets the next multiple of M */
template<deUint32 M>
static int getNextMultiple(deUint32 value)
{
	if (value % M == 0)
	{
		return value;
	}
	return value + M - (value % M);
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
													const tcu::IVec2&			renderSize)
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
		DE_NULL,												// VkRenderPass						renderPass;
		0u,														// deUint32							subpass;
		DE_NULL,												// VkFramebuffer					framebuffer;
		false,													// VkBool32							occlusionQueryEnable;
		0u,														// VkQueryControlFlags				queryFlags;
		0u														// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	const void* const	imageBarrierPtr		= &imageBarrier;
	const void* const	bufferBarrierPtr	= &bufferBarrier;

	// Copy image to buffer

	const VkBufferImageCopy copyRegion =
	{
		0u,											// VkDeviceSize				bufferOffset;
		(deUint32)renderSize.x(),					// deUint32					bufferRowLength;
		(deUint32)renderSize.y(),					// deUint32					bufferImageHeight;
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u },	// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },								// VkOffset3D				imageOffset;
		{ renderSize.x(), renderSize.y(), 1 }		// VkExtent3D				imageExtent;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 1, &imageBarrierPtr);
	vk.cmdCopyImageToBuffer(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &bufferBarrierPtr);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
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
		0u,												// VkAccessFlags			dstAccessMask;
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
		DE_NULL,										// VkRenderPass						renderPass;
		0u,												// deUint32							subpass;
		DE_NULL,										// VkFramebuffer					framebuffer;
		false,											// VkBool32							occlusionQueryEnable;
		0u,												// VkQueryControlFlags				queryFlags;
		0u												// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	const void* preCopyBarriers[2] =
	{
		&preBufferBarrier,
		&preImageBarrier
	};

	const void* const						postCopyBarrier	= &postImageBarrier;
	const std::vector<VkBufferImageCopy>	copyRegions		= srcTexture.getBufferCopyRegions();

	// Write buffer data
	srcTexture.write(reinterpret_cast<deUint8*>(bufferAlloc->getHostPtr()));
	flushMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), bufferSize);

	// Copy buffer to image
	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 2, preCopyBarriers);
	vk.cmdCopyBufferToImage(*cmdBuffer, *buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegions.size(), copyRegions.data());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &postCopyBarrier);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
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
	deUint32 textureSize = 0;

	for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
	{
		for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
		{
			const tcu::ConstPixelBufferAccess level = getLevel(levelNdx, layerNdx);
			textureSize = getNextMultiple<4>(textureSize);
			textureSize += level.getWidth() * level.getHeight() * level.getDepth() * level.getFormat().getPixelSize();
		}
	}

	return textureSize;
}

deUint32 TestTexture::getCompressedSize (void) const
{
	if (!isCompressed())
		throw tcu::InternalError("Texture is not compressed");

	deUint32 textureSize = 0;

	for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
	{
		for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
		{
			textureSize = getNextMultiple<4>(textureSize);
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
	std::vector<VkBufferImageCopy>	regions;
	deUint32						layerDataOffset	= 0;

	if (isCompressed())
	{
		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				const tcu::CompressedTexture& level = getCompressedLevel(levelNdx, layerNdx);

				layerDataOffset = getNextMultiple<4>(layerDataOffset);

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
					{ 0u, 0u, 0u },							// VkOffset3D				imageOffset;
					{										// VkExtent3D				imageExtent;
						level.getWidth(),
						level.getHeight(),
						level.getDepth()
					}
				};

				regions.push_back(layerRegion);
				layerDataOffset += level.getDataSize();
			}
		}
	}
	else
	{
		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				const tcu::ConstPixelBufferAccess level = getLevel(levelNdx, layerNdx);

				layerDataOffset = getNextMultiple<4>(layerDataOffset);

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
						level.getWidth(),
						level.getHeight(),
						level.getDepth()
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
	deUint32 levelOffset = 0;

	if (isCompressed())
	{
		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				levelOffset = getNextMultiple<4>(levelOffset);

				const tcu::CompressedTexture&		compressedTex	= getCompressedLevel(levelNdx, layerNdx);

				deMemcpy(destPtr + levelOffset, compressedTex.getData(), compressedTex.getDataSize());
				levelOffset += compressedTex.getDataSize();
			}
		}
	}
	else
	{
		for (int levelNdx = 0; levelNdx < getNumLevels(); levelNdx++)
		{
			for (int layerNdx = 0; layerNdx < getArraySize(); layerNdx++)
			{
				levelOffset = getNextMultiple<4>(levelOffset);

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

void TestTexture::populateCompressedLevels (const tcu::CompressedTexFormat& format, const std::vector<tcu::PixelBufferAccess>& decompressedLevels)
{
	// Generate random compressed data and update decompressed data

	de::Random random(123);

	for (size_t levelNdx = 0; levelNdx < decompressedLevels.size(); levelNdx++)
	{
		const tcu::PixelBufferAccess	level				= decompressedLevels[levelNdx];
		tcu::CompressedTexture*			compressedLevel		= new tcu::CompressedTexture(format, level.getWidth(), level.getHeight(), level.getDepth());
		deUint8* const					compressedData		= (deUint8*)compressedLevel->getData();

		// Generate random compressed data
		for (int byteNdx = 0; byteNdx < compressedLevel->getDataSize(); byteNdx++)
			compressedData[byteNdx] = 0xFF & random.getUint32();

		m_compressedLevels.push_back(compressedLevel);

		// Store decompressed data
		compressedLevel->decompress(level);
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
	return m_texture.getLevel(level);
}

const tcu::ConstPixelBufferAccess TestTexture1D::getLevel (int level, int layer) const
{
	DE_ASSERT(layer == 0);
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
	return m_texture.getLevel(level);
}

const tcu::ConstPixelBufferAccess TestTexture2D::getLevel (int level, int layer) const
{
	DE_ASSERT(layer == 0);
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
	return m_texture.getLevel(level);
}

const tcu::ConstPixelBufferAccess TestTexture3D::getLevel (int level, int layer) const
{
	DE_ASSERT(layer == 0);
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
