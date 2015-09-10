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

namespace vkt
{
namespace pipeline
{

using namespace vk;

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
	Move<VkCmdPool>					cmdPool;
	Move<VkCmdBuffer>				cmdBuffer;
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
			pixelDataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT,   // VkBufferUsageFlags	usage;
			0u,											// VkBufferCreateFlags	flags;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	{
		const VkCmdPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			queueFamilyIndex,							// deUint32				queueFamilyIndex;
			VK_CMD_POOL_CREATE_TRANSIENT_BIT			// VkCmdPoolCreateFlags	flags;
		};

		cmdPool = createCommandPool(vk, device, &cmdPoolParams);

		const VkCmdBufferCreateInfo cmdBufferParams =
		{
			VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			*cmdPool,									// VkCmdPool				cmdPool;
			VK_CMD_BUFFER_LEVEL_PRIMARY,				// VkCmdBufferLevel			level;
			0u,											// VkCmdBufferCreateFlags	flags;
		};

		cmdBuffer = createCommandBuffer(vk, device, &cmdBufferParams);
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
		VK_MEMORY_OUTPUT_COLOR_ATTACHMENT_BIT,		// VkMemoryOutputFlags		outputMask;
		VK_MEMORY_INPUT_TRANSFER_BIT,				// VkMemoryInputFlags		inputMask;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
		image,										// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR,	// VkImageAspect	aspect;
			0u,						// deUint32			baseMipLevel;
			1u,						// deUint32			mipLevels;
			0u,						// deUint32			baseArraySlice;
			1u						// deUint32			arraySize;
		}
	};

	const VkBufferMemoryBarrier bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		VK_MEMORY_OUTPUT_TRANSFER_BIT,				// VkMemoryOutputFlags	outputMask;
		VK_MEMORY_INPUT_HOST_READ_BIT,				// VkMemoryInputFlags	inputMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32				srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32				destQueueFamilyIndex;
		*buffer,									// VkBuffer				buffer;
		0u,											// VkDeviceSize			offset;
		pixelDataSize								// VkDeviceSize			size;
	};

	const VkCmdBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,												// VkStructureType			sType;
		DE_NULL,																				// const void*				pNext;
		VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// VkCmdBufferOptimizeFlags	flags;
		DE_NULL,																				// VkRenderPass				renderPass;
		DE_NULL																					// VkFramebuffer			framebuffer;
	};

	const void* const	imageBarrierPtr		= &imageBarrier;
	const void* const	bufferBarrierPtr	= &bufferBarrier;

	// Copy image to buffer

	const VkBufferImageCopy copyRegion =
	{
		0u,											// VkDeviceSize			bufferOffset;
		(deUint32)renderSize.x(),					// deUint32				bufferRowLength;
		(deUint32)renderSize.y(),					// deUint32				bufferImageHeight;
		{ VK_IMAGE_ASPECT_COLOR, 0u, 0u },			// VkImageSubresource	imageSubresource;
		{ 0, 0, 0 },								// VkOffset3D			imageOffset;
		{ renderSize.x(), renderSize.y(), 1 }		// VkExtent3D			imageExtent;
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_FALSE, 1, &imageBarrierPtr);
	vk.cmdCopyImageToBuffer(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, *buffer, 1, &copyRegion);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_FALSE, 1, &bufferBarrierPtr);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	VK_CHECK(vk.queueSubmit(queue, 1, &cmdBuffer.get(), *fence));
	VK_CHECK(vk.waitForFences(device, 1, &fence.get(), 0, ~(0ull) /* infinity */));


	// Map and read buffer data

	const VkMappedMemoryRange memoryRange =
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,		// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		bufferAlloc->getMemory(),					// VkDeviceMemory	mem;
		bufferAlloc->getOffset(),					// VkDeviceSize		offset;
		pixelDataSize								// VkDeviceSize		size;
	};

	void* bufferPtr;
	VK_CHECK(vk.mapMemory(device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize, 0, &bufferPtr));
	VK_CHECK(vk.invalidateMappedMemoryRanges(device, 1, &memoryRange));
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(), bufferPtr));
	VK_CHECK(vk.unmapMemory(device, bufferAlloc->getMemory()));

	return resultLevel;
}

} // pipeline
} // vkt
