/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Image Object Util
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolImageObjectUtil.hpp"

#include "tcuSurface.hpp"
#include "tcuVectorUtil.hpp"

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktQueryPoolCreateInfoUtil.hpp"
#include "vktQueryPoolBufferObjectUtil.hpp"

#include "tcuTextureUtil.hpp"

namespace vkt
{
namespace QueryPool
{

void MemoryOp::pack (int				pixelSize,
					 int				width,
					 int				height,
					 int				depth,
					 vk::VkDeviceSize	rowPitch,
					 vk::VkDeviceSize	depthPitch,
					 const void *		srcBuffer,
					 void *				destBuffer)
{
	if (rowPitch == 0) 
		rowPitch = width * pixelSize;

	if (depthPitch == 0) 
		depthPitch = rowPitch * height;

	const vk::VkDeviceSize size = depthPitch * depth;

	const char *srcRow = reinterpret_cast<const char *>(srcBuffer);
	const char *srcStart;
	srcStart = srcRow;
	char *dstRow = reinterpret_cast<char *>(destBuffer);
	char *dstStart;
	dstStart = dstRow;

	if (rowPitch == static_cast<vk::VkDeviceSize>(width * pixelSize) &&
		depthPitch == static_cast<vk::VkDeviceSize>(rowPitch * height)) 
	{
		// fast path
		deMemcpy(dstRow, srcRow, static_cast<size_t>(size));
	}
	else
	{
		// slower, per row path
		for (int d = 0; d < depth; d++)
		{
			vk::VkDeviceSize offsetDepthDst = d * depthPitch;
			vk::VkDeviceSize offsetDepthSrc = d * (pixelSize * width * height);
			srcRow = srcStart + offsetDepthSrc;
			dstRow = dstStart + offsetDepthDst;
			for (int r = 0; r < height; ++r) 
			{
				deMemcpy(dstRow, srcRow, static_cast<size_t>(rowPitch));
				srcRow += pixelSize * width;
				dstRow += rowPitch;
			}
		}
	}
}

void MemoryOp::unpack (int					pixelSize,
					   int					width,
					   int					height,
					   int					depth,
					   vk::VkDeviceSize		rowPitch,
					   vk::VkDeviceSize		depthPitch,
					   const void *			srcBuffer,
					   void *				destBuffer)
{
	if (rowPitch == 0)
		rowPitch = width * pixelSize;

	if (depthPitch == 0) 
		depthPitch = rowPitch * height;

	const vk::VkDeviceSize size = depthPitch * depth;

	const char *srcRow = reinterpret_cast<const char *>(srcBuffer);
	const char *srcStart;
	srcStart = srcRow;
	char *dstRow = reinterpret_cast<char *>(destBuffer);
	char *dstStart;
	dstStart = dstRow;

	if (rowPitch == static_cast<vk::VkDeviceSize>(width * pixelSize) &&
		depthPitch == static_cast<vk::VkDeviceSize>(rowPitch * height)) 
	{
		// fast path
		deMemcpy(dstRow, srcRow, static_cast<size_t>(size));
	}
	else {
		// slower, per row path
		for (size_t d = 0; d < (size_t)depth; d++)
		{
			vk::VkDeviceSize offsetDepthDst = d * (pixelSize * width * height);
			vk::VkDeviceSize offsetDepthSrc = d * depthPitch;
			srcRow = srcStart + offsetDepthSrc;
			dstRow = dstStart + offsetDepthDst;
			for (int r = 0; r < height; ++r) 
			{
				deMemcpy(dstRow, srcRow, static_cast<size_t>(pixelSize * width));
				srcRow += rowPitch;
				dstRow += pixelSize * width;
			}
		}
	}
}

Image::Image (const vk::DeviceInterface &vk,
			  vk::VkDevice				device,
			  vk::VkFormat				format,
			  const vk::VkExtent3D 		&extend,
			  deUint32					mipLevels,
			  deUint32					arraySize,
			  vk::Move<vk::VkImage>		object)
	: m_allocation		(DE_NULL)
	, m_object			(object)
	, m_format			(format)
	, m_extent			(extend)
	, m_mipLevels		(mipLevels)
	, m_arraySize		(arraySize)
	, m_vk(vk)
	, m_device(device)
{
}

tcu::ConstPixelBufferAccess Image::readSurface (vk::VkQueue			queue,
												vk::Allocator		&allocator,
												vk::VkImageLayout	layout,
												vk::VkOffset3D		offset,
												int					width,
												int					height,
												vk::VkImageAspect	aspect,
												unsigned int		mipLevel,
												unsigned int		arrayElement)
{
	m_pixelAccessData.resize(width * height * vk::mapVkFormat(m_format).getPixelSize());
	memset(m_pixelAccessData.data(), 0, m_pixelAccessData.size());
	if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
	{
		read(queue, allocator, layout, offset, width, height, 1, mipLevel, arrayElement, aspect, vk::VK_IMAGE_TYPE_2D,
		m_pixelAccessData.data());
	}
	if (aspect == vk::VK_IMAGE_ASPECT_DEPTH || aspect == vk::VK_IMAGE_ASPECT_STENCIL)
	{
		readUsingBuffer(queue, allocator, layout, offset, width, height, 1, mipLevel, arrayElement, aspect, m_pixelAccessData.data());
	}
	return tcu::ConstPixelBufferAccess(vk::mapVkFormat(m_format), width, height, 1, m_pixelAccessData.data());
}

tcu::ConstPixelBufferAccess Image::readVolume (vk::VkQueue			queue,
											   vk::Allocator		&allocator,
											   vk::VkImageLayout	layout,
											   vk::VkOffset3D		offset,
											   int					width,
											   int					height,
											   int					depth,
											   vk::VkImageAspect	aspect,
											   unsigned int			mipLevel,
											   unsigned int			arrayElement)
{
	m_pixelAccessData.resize(width * height * depth * vk::mapVkFormat(m_format).getPixelSize());
	memset(m_pixelAccessData.data(), 0, m_pixelAccessData.size());
	if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
	{
		read(queue, allocator, layout, offset, width, height, depth, mipLevel, arrayElement, aspect, vk::VK_IMAGE_TYPE_3D,
		m_pixelAccessData.data());
	}
	if (aspect == vk::VK_IMAGE_ASPECT_DEPTH || aspect == vk::VK_IMAGE_ASPECT_STENCIL)
	{
		readUsingBuffer(queue, allocator, layout, offset, width, height, depth, mipLevel, arrayElement, aspect, m_pixelAccessData.data());
	}
	return tcu::ConstPixelBufferAccess(vk::mapVkFormat(m_format), width, height, depth, m_pixelAccessData.data());
}

tcu::ConstPixelBufferAccess Image::readSurface1D(vk::VkQueue		queue,
												 vk::Allocator		&allocator,
												 vk::VkImageLayout	layout,
												 vk::VkOffset3D		offset,
												 int				width,
												 vk::VkImageAspect	aspect,
												 unsigned int		mipLevel,
												 unsigned int		arrayElement)
{
	m_pixelAccessData.resize(width * vk::mapVkFormat(m_format).getPixelSize());
	memset(m_pixelAccessData.data(), 0, m_pixelAccessData.size());
	if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
	{
		read(queue, allocator, layout, offset, width, 1, 1, mipLevel, arrayElement, aspect, vk::VK_IMAGE_TYPE_1D,
		m_pixelAccessData.data());
	}
	if (aspect == vk::VK_IMAGE_ASPECT_DEPTH || aspect == vk::VK_IMAGE_ASPECT_STENCIL)
	{
		readUsingBuffer(queue, allocator, layout, offset, width, 1, 1, mipLevel, arrayElement, aspect,
		m_pixelAccessData.data());
	}
	return tcu::ConstPixelBufferAccess(vk::mapVkFormat(m_format), width, 1, 1, m_pixelAccessData.data());
}

void Image::read (vk::VkQueue			queue,
				  vk::Allocator			&allocator,
				  vk::VkImageLayout		layout,
				  vk::VkOffset3D		offset,
				  int					width,
				  int					height,
				  int					depth,
				  unsigned int			mipLevel,
				  unsigned int			arrayElement,
				  vk::VkImageAspect		aspect,
				  vk::VkImageType		type,
				  void *				data)
{
	if (layout != vk::VK_IMAGE_LAYOUT_GENERAL && layout != vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL) 
		TCU_FAIL("Image::uploadFromSurface usage error: this function is not going to change Image layout!");

	de::SharedPtr<Image> stagingResource = copyToLinearImage(queue, allocator, layout, offset, width,
															 height, depth, mipLevel, arrayElement, aspect, type);
	const vk::VkOffset3D zeroOffset = {0, 0, 0};
	stagingResource->readLinear(zeroOffset, width, height, depth, 0, 0, aspect, data);
}

void Image::readUsingBuffer (vk::VkQueue		queue,
							 vk::Allocator		&allocator,
							 vk::VkImageLayout	layout,
							 vk::VkOffset3D		offset,
							 int				width,
							 int				height,
							 int				depth,
							 unsigned int		mipLevel,
							 unsigned int		arrayElement,
							 vk::VkImageAspect	aspect,
							 void *				data)
{
	if (layout != vk::VK_IMAGE_LAYOUT_GENERAL && layout != vk::VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL) 
		TCU_FAIL("Image::uploadFromSurface usage error: this function is not going to change Image layout!");

	de::SharedPtr<Buffer> stagingResource;

	bool isCombinedType = isCombinedDepthStencilType(vk::mapVkFormat(m_format).type);
	vk::VkDeviceSize bufferSize = 0;

	if (!isCombinedType)
		bufferSize = vk::mapVkFormat(m_format).getPixelSize() * width * height * depth;

	if (isCombinedType)
	{
		int pixelSize = 0;
		switch (m_format)
		{
			case vk::VK_FORMAT_D16_UNORM_S8_UINT:
				pixelSize = (aspect == vk::VK_IMAGE_ASPECT_DEPTH) ? 2 : 1;
				break;
			case  vk::VK_FORMAT_D32_SFLOAT_S8_UINT:
				pixelSize = (aspect == vk::VK_IMAGE_ASPECT_DEPTH) ? 4 : 1;
				break;
			case vk::VK_FORMAT_D24_UNORM_X8:
			case vk::VK_FORMAT_D24_UNORM_S8_UINT:
				pixelSize = (aspect == vk::VK_IMAGE_ASPECT_DEPTH) ? 3 : 1;
				break;
		}
		bufferSize = pixelSize*width*height*depth;
	}

	BufferCreateInfo stagingBufferResourceCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT);
	stagingResource = Buffer::CreateAndAlloc(m_vk, m_device, stagingBufferResourceCreateInfo, allocator, vk::MemoryRequirement::HostVisible);

	{
		#pragma message("Get queue family index")
		CmdPoolCreateInfo copyCmdPoolCreateInfo(0);
		vk::Unique<vk::VkCmdPool> copyCmdPool(vk::createCommandPool(m_vk, m_device, &copyCmdPoolCreateInfo));

		CmdBufferCreateInfo copyCmdBufCreateInfo(*copyCmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
		vk::Unique<vk::VkCmdBuffer> copyCmdBuffer(vk::createCommandBuffer(m_vk, m_device, &copyCmdBufCreateInfo));

		CmdBufferBeginInfo beginInfo;
		VK_CHECK(m_vk.beginCommandBuffer(*copyCmdBuffer, &beginInfo));

		if (layout == vk::VK_IMAGE_LAYOUT_UNDEFINED)
		{
			layout = vk::VK_IMAGE_LAYOUT_GENERAL;

			vk::VkImageMemoryBarrier barrier;
			barrier.sType = vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.pNext = DE_NULL;
			barrier.outputMask = 0;
			barrier.inputMask = 0;
			barrier.oldLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = vk::VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
			barrier.destQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
			barrier.image = object();

			vk::VkImageAspectFlags aspectMask = 0;
			if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
			}
			if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
			}

			barrier.subresourceRange.aspectMask = aspectMask;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.mipLevels = m_mipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.arraySize = m_arraySize;

			void* barriers[] = { &barrier };

			m_vk.cmdPipelineBarrier(*copyCmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
				false, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		vk::VkImageAspectFlags aspectMask = 0;
		if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		vk::VkBufferImageCopy region = 
		{
			0, 0, 0,
			{ aspect, mipLevel, arrayElement, 1 },
			offset,
			{ width, height, depth }
		};

		m_vk.cmdCopyImageToBuffer(*copyCmdBuffer, object(), layout, stagingResource->object(), 1, &region);
		VK_CHECK(m_vk.endCommandBuffer(*copyCmdBuffer));

		VK_CHECK(m_vk.queueSubmit(queue, 1, &copyCmdBuffer.get(), DE_NULL));

		// TODO: make this less intrusive
		VK_CHECK(m_vk.queueWaitIdle(queue));
	}

	char* destPtr = reinterpret_cast<char*>(stagingResource->getBoundMemory().getHostPtr());
	deMemcpy(data, destPtr, bufferSize);
}

tcu::ConstPixelBufferAccess Image::readSurfaceLinear (vk::VkOffset3D		offset,
													  int					width,
													  int					height,
													  int					depth,
													  vk::VkImageAspect		aspect,
													  unsigned int			mipLevel,
													  unsigned int			arrayElement)
{
	m_pixelAccessData.resize(width * height * vk::mapVkFormat(m_format).getPixelSize());
	readLinear(offset, width, height, depth, mipLevel, arrayElement, aspect, m_pixelAccessData.data());
	return tcu::ConstPixelBufferAccess(vk::mapVkFormat(m_format), width, height, 1, m_pixelAccessData.data());
}

void Image::readLinear (vk::VkOffset3D		offset,
						int					width,
						int					height,
						int					depth,
						unsigned int		mipLevel,
						unsigned int		arrayElement,
						vk::VkImageAspect	aspect,
						void *				data)
{
	vk::VkImageSubresource imageSubResource = { aspect, mipLevel, arrayElement };

	vk::VkSubresourceLayout imageLayout = {};

	VK_CHECK(m_vk.getImageSubresourceLayout(m_device, object(), &imageSubResource, &imageLayout));

	const char* srcPtr = reinterpret_cast<const char*>(getBoundMemory().getHostPtr());
	srcPtr += imageLayout.offset + getPixelOffset(offset, imageLayout.rowPitch, imageLayout.depthPitch, mipLevel, arrayElement);

	MemoryOp::unpack(vk::mapVkFormat(m_format).getPixelSize(), width, height, depth,
		imageLayout.rowPitch, imageLayout.depthPitch, srcPtr, data);
}

de::SharedPtr<Image> Image::copyToLinearImage (vk::VkQueue			queue,
											   vk::Allocator		&allocator,
											   vk::VkImageLayout	layout,
											   vk::VkOffset3D		offset,
											   int					width,
											   int					height,
											   int					depth,
											   unsigned int			mipLevel,
											   unsigned int			arrayElement,
											   vk::VkImageAspect	aspect,
											   vk::VkImageType		type)
{
	de::SharedPtr<Image> stagingResource;
	{
		vk::VkExtent3D stagingExtent = {width, height, depth};
		ImageCreateInfo stagingResourceCreateInfo(
			type, m_format, stagingExtent, 1, 1, 1,
			vk::VK_IMAGE_TILING_LINEAR, vk::VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT);

		stagingResource = Image::CreateAndAlloc(m_vk, m_device, stagingResourceCreateInfo, allocator,
			vk::MemoryRequirement::HostVisible);

		#pragma message("Get queue family index")
		CmdPoolCreateInfo copyCmdPoolCreateInfo(0);
		vk::Unique<vk::VkCmdPool> copyCmdPool(vk::createCommandPool(m_vk, m_device, &copyCmdPoolCreateInfo));

		CmdBufferCreateInfo copyCmdBufCreateInfo(*copyCmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
		vk::Unique<vk::VkCmdBuffer> copyCmdBuffer(vk::createCommandBuffer(m_vk, m_device, &copyCmdBufCreateInfo));

		CmdBufferBeginInfo beginInfo;
		VK_CHECK(m_vk.beginCommandBuffer(*copyCmdBuffer, &beginInfo));


		vk::VkImageAspectFlags aspectMask = 0;
		if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		transition2DImage(m_vk, *copyCmdBuffer, stagingResource->object(), aspectMask, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL);

		const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
		vk::VkImageCopy region = { {aspect, mipLevel, arrayElement, 1}, offset, {aspect, 0, 0, 1}, zeroOffset, {width, height, depth} };

		m_vk.cmdCopyImage(*copyCmdBuffer, object(), layout, stagingResource->object(), vk::VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		VK_CHECK(m_vk.endCommandBuffer(*copyCmdBuffer));

		VK_CHECK(m_vk.queueSubmit(queue, 1, &copyCmdBuffer.get(), DE_NULL));

		// TODO: make this less intrusive
		VK_CHECK(m_vk.queueWaitIdle(queue));
	}
	return stagingResource;
}

void Image::uploadVolume(const tcu::ConstPixelBufferAccess	&access,
						 vk::VkQueue						queue,
						 vk::Allocator 						&allocator,
						 vk::VkImageLayout					layout,
						 vk::VkOffset3D						offset,
						 vk::VkImageAspect					aspect,
						 unsigned int						mipLevel,
						 unsigned int						arrayElement)
{
	if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
	{
		upload(queue, allocator, layout, offset, access.getWidth(),
		access.getHeight(), access.getDepth(), mipLevel, arrayElement, aspect, vk::VK_IMAGE_TYPE_3D,
		access.getDataPtr());
	}
	if (aspect == vk::VK_IMAGE_ASPECT_DEPTH || aspect == vk::VK_IMAGE_ASPECT_STENCIL)
	{
		uploadUsingBuffer(queue, allocator, layout, offset, access.getWidth(),
		access.getHeight(), access.getDepth(), mipLevel, arrayElement, aspect, access.getDataPtr());
	}
}

void Image::uploadSurface (const tcu::ConstPixelBufferAccess	&access,
						   vk::VkQueue							queue,
						   vk::Allocator 						&allocator,
						   vk::VkImageLayout					layout,
						   vk::VkOffset3D						offset,
						   vk::VkImageAspect					aspect,
						   unsigned int							mipLevel,
						   unsigned int							arrayElement)
{
	if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
	{
		upload(queue, allocator, layout, offset, access.getWidth(),
			access.getHeight(), access.getDepth(), mipLevel, arrayElement, aspect, vk::VK_IMAGE_TYPE_2D,
			access.getDataPtr());
	}
	if (aspect == vk::VK_IMAGE_ASPECT_DEPTH || aspect == vk::VK_IMAGE_ASPECT_STENCIL)
	{
		uploadUsingBuffer(queue, allocator, layout, offset, access.getWidth(),
			access.getHeight(), access.getDepth(), mipLevel, arrayElement, aspect, access.getDataPtr());
	}
}

void Image::uploadSurface1D (const tcu::ConstPixelBufferAccess 	&access,
							 vk::VkQueue						queue,
							 vk::Allocator 						&allocator,
							 vk::VkImageLayout					layout,
							 vk::VkOffset3D						offset,
							 vk::VkImageAspect					aspect,
							 unsigned int						mipLevel,
							 unsigned int						arrayElement)
{
	if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
	{
		upload(queue, allocator, layout, offset, access.getWidth(),
			access.getHeight(), access.getDepth(), mipLevel, arrayElement, aspect, vk::VK_IMAGE_TYPE_1D,
			access.getDataPtr());
	}
	if (aspect == vk::VK_IMAGE_ASPECT_DEPTH || aspect == vk::VK_IMAGE_ASPECT_STENCIL)
	{
		uploadUsingBuffer(queue, allocator, layout, offset, access.getWidth(),
			access.getHeight(), access.getDepth(), mipLevel, arrayElement, aspect, access.getDataPtr());
	}
}

void Image::uploadSurfaceLinear (const tcu::ConstPixelBufferAccess 	&access,
								 vk::VkOffset3D						offset,
								 int								width,
								 int								height,
								 int								depth,
								 vk::VkImageAspect					aspect,
								 unsigned int						mipLevel,
								 unsigned int						arrayElement)
{
	uploadLinear(offset, width, height, depth, mipLevel, arrayElement, aspect, access.getDataPtr());
}

void Image::upload (vk::VkQueue			queue,
					vk::Allocator 		&allocator,
					vk::VkImageLayout	layout,
					vk::VkOffset3D		offset,
					int					width,
					int					height,
					int					depth,
					unsigned int		mipLevel,
					unsigned int		arrayElement,
					vk::VkImageAspect	aspect,
					vk::VkImageType		type,
					const void *		data)
{

	if (layout != vk::VK_IMAGE_LAYOUT_UNDEFINED
		&& layout != vk::VK_IMAGE_LAYOUT_GENERAL
		&& layout != vk::VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL) 
	{
		TCU_FAIL("Image::uploadFromRaw usage error: this function is not going to change Image layout!");
	}

	de::SharedPtr<Image> stagingResource;
	vk::VkExtent3D extent = {width, height, depth};
	ImageCreateInfo stagingResourceCreateInfo(
		type, m_format, extent, 1, 1, 1,
		vk::VK_IMAGE_TILING_LINEAR, vk::VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT);

	stagingResource = Image::CreateAndAlloc(m_vk, m_device, stagingResourceCreateInfo, allocator,
								vk::MemoryRequirement::HostVisible);
	
	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	stagingResource->uploadLinear(zeroOffset, width, height, depth, 0, 0, aspect, data);

	{
		#pragma message("Get queue family index")
		CmdPoolCreateInfo copyCmdPoolCreateInfo(0);
		vk::Unique<vk::VkCmdPool> copyCmdPool(vk::createCommandPool(m_vk, m_device, &copyCmdPoolCreateInfo));

		CmdBufferCreateInfo copyCmdBufCreateInfo(*copyCmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
		vk::Unique<vk::VkCmdBuffer> copyCmdBuffer(vk::createCommandBuffer(m_vk, m_device, &copyCmdBufCreateInfo));

		CmdBufferBeginInfo beginInfo;
		VK_CHECK(m_vk.beginCommandBuffer(*copyCmdBuffer, &beginInfo));
		
		if (layout == vk::VK_IMAGE_LAYOUT_UNDEFINED)
		{
			layout = vk::VK_IMAGE_LAYOUT_GENERAL;

			vk::VkImageMemoryBarrier barrier;
			barrier.sType = vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.pNext = DE_NULL;
			barrier.outputMask = 0;
			barrier.inputMask = 0;
			barrier.oldLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = vk::VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
			barrier.destQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
			barrier.image = object();

			vk::VkImageAspectFlags aspectMask = 0;
			if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
			}
			if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
			}				
				
			barrier.subresourceRange.aspectMask = aspectMask;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.mipLevels = m_mipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.arraySize = m_arraySize;

			void* barriers[] = { &barrier };

			m_vk.cmdPipelineBarrier(*copyCmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		vk::VkImageAspectFlags aspectMask = 0;
		if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		transition2DImage(m_vk, *copyCmdBuffer, stagingResource->object(), aspectMask, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL);

		vk::VkImageCopy region = {{aspect, 0, 0, 1},
									zeroOffset,
									{aspect, mipLevel, arrayElement, 1},
									offset,
									{width, height, depth}};

		m_vk.cmdCopyImage(*copyCmdBuffer, stagingResource->object(),
								vk::VK_IMAGE_LAYOUT_GENERAL, object(), layout, 1, &region);
		VK_CHECK(m_vk.endCommandBuffer(*copyCmdBuffer));

		VK_CHECK(m_vk.queueSubmit(queue, 1, &copyCmdBuffer.get(), DE_NULL));

		// TODO: make this less intrusive
		VK_CHECK(m_vk.queueWaitIdle(queue));
	}
}

void Image::uploadUsingBuffer (vk::VkQueue			queue,
							   vk::Allocator 		&allocator,
							   vk::VkImageLayout	layout,
							   vk::VkOffset3D		offset,
							   int					width,
							   int					height,
							   int					depth,
							   unsigned int			mipLevel,
							   unsigned int			arrayElement,
							   vk::VkImageAspect	aspect,
							   const void *			data)
{
	if (layout != vk::VK_IMAGE_LAYOUT_UNDEFINED
		&& layout != vk::VK_IMAGE_LAYOUT_GENERAL
		&& layout != vk::VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL) 
	{
		TCU_FAIL("Image::uploadFromRaw usage error: this function is not going to change Image layout!");
	}

	de::SharedPtr<Buffer> stagingResource;
	bool isCombinedType = isCombinedDepthStencilType(vk::mapVkFormat(m_format).type);
	vk::VkDeviceSize bufferSize = 0;
	if (!isCombinedType)
		bufferSize = vk::mapVkFormat(m_format).getPixelSize() *width*height*depth;
	if (isCombinedType)
	{
		int pixelSize = 0;
		switch (m_format)
		{
			case vk::VK_FORMAT_D16_UNORM_S8_UINT:
				pixelSize = (aspect == vk::VK_IMAGE_ASPECT_DEPTH) ? 2 : 1;
				break;
			case  vk::VK_FORMAT_D32_SFLOAT_S8_UINT:
				pixelSize = (aspect == vk::VK_IMAGE_ASPECT_DEPTH) ? 4 : 1;
				break;
			case vk::VK_FORMAT_D24_UNORM_X8:
			case vk::VK_FORMAT_D24_UNORM_S8_UINT:
				pixelSize = (aspect == vk::VK_IMAGE_ASPECT_DEPTH) ? 3 : 1;
			break;
		}
		bufferSize = pixelSize*width*height*depth;
	}
	BufferCreateInfo stagingBufferResourceCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DESTINATION_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SOURCE_BIT);
	stagingResource = Buffer::CreateAndAlloc(m_vk, m_device, stagingBufferResourceCreateInfo, allocator, vk::MemoryRequirement::HostVisible);
	char* destPtr = reinterpret_cast<char*>(stagingResource->getBoundMemory().getHostPtr());
	deMemcpy(destPtr, data, bufferSize);
	vk::flushMappedMemoryRange(m_vk, m_device, stagingResource->getBoundMemory().getMemory(), stagingResource->getBoundMemory().getOffset(), bufferSize);
	{
		#pragma message("Get queue family index")
		CmdPoolCreateInfo copyCmdPoolCreateInfo(0);
		vk::Unique<vk::VkCmdPool> copyCmdPool(vk::createCommandPool(m_vk, m_device, &copyCmdPoolCreateInfo));

		CmdBufferCreateInfo copyCmdBufCreateInfo(*copyCmdPool, vk::VK_CMD_BUFFER_LEVEL_PRIMARY, 0);
		vk::Unique<vk::VkCmdBuffer> copyCmdBuffer(vk::createCommandBuffer(m_vk, m_device, &copyCmdBufCreateInfo));

		CmdBufferBeginInfo beginInfo;
		VK_CHECK(m_vk.beginCommandBuffer(*copyCmdBuffer, &beginInfo));

		if (layout == vk::VK_IMAGE_LAYOUT_UNDEFINED)
		{
			layout = vk::VK_IMAGE_LAYOUT_GENERAL;

			vk::VkImageMemoryBarrier barrier;
			barrier.sType = vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.pNext = DE_NULL;
			barrier.outputMask = 0;
			barrier.inputMask = 0;
			barrier.oldLayout = vk::VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = vk::VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
			barrier.destQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
			barrier.image = object();

			vk::VkImageAspectFlags aspectMask = 0;
			if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
			}
			if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
			{
				aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
			}

			barrier.subresourceRange.aspectMask = aspectMask;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.mipLevels = m_mipLevels;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.arraySize = m_arraySize;

			void* barriers[] = { &barrier };

			m_vk.cmdPipelineBarrier(*copyCmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		vk::VkImageAspectFlags aspectMask = 0;
		if (aspect == vk::VK_IMAGE_ASPECT_COLOR)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_DEPTH)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (aspect == vk::VK_IMAGE_ASPECT_STENCIL)
		{
			aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		vk::VkBufferImageCopy region = {
			0, 0, 0,
			{ aspect, mipLevel, arrayElement, 1 },
			offset,
			{ width, height, depth }
		};

		m_vk.cmdCopyBufferToImage(*copyCmdBuffer, stagingResource->object(),
			object(), layout, 1, &region);
		VK_CHECK(m_vk.endCommandBuffer(*copyCmdBuffer));

		VK_CHECK(m_vk.queueSubmit(queue, 1, &copyCmdBuffer.get(), DE_NULL));

		// TODO: make this less intrusive
		VK_CHECK(m_vk.queueWaitIdle(queue));
	}
}


void Image::uploadLinear (vk::VkOffset3D	offset,
						  int				width,
						  int				height,
						  int				depth,
						  unsigned int		mipLevel,
						  unsigned int		arrayElement,
						  vk::VkImageAspect	aspect,
						  const void *		data)
{
	vk::VkSubresourceLayout imageLayout;

	vk::VkImageSubresource imageSubResource = {aspect, mipLevel, arrayElement};

	VK_CHECK(m_vk.getImageSubresourceLayout(m_device, object(), &imageSubResource,
													&imageLayout));

	char* destPtr = reinterpret_cast<char*>(getBoundMemory().getHostPtr());

	destPtr += imageLayout.offset + getPixelOffset(offset, imageLayout.rowPitch, imageLayout.depthPitch, mipLevel, arrayElement);

	MemoryOp::pack(vk::mapVkFormat(m_format).getPixelSize(), width, height, depth, 
		imageLayout.rowPitch, imageLayout.depthPitch, data, destPtr);
}

vk::VkDeviceSize Image::getPixelOffset (vk::VkOffset3D		offset,
										vk::VkDeviceSize	rowPitch,
										vk::VkDeviceSize	depthPitch,
										unsigned int		mipLevel,
										unsigned int		arrayElement)
{
	if (mipLevel >= m_mipLevels) 
		TCU_FAIL("mip level too large");

	if (arrayElement >= m_arraySize)
		TCU_FAIL("array element too large");

	vk::VkDeviceSize mipLevelSizes[32];
	vk::VkDeviceSize mipLevelRectSizes[32];
	tcu::IVec3 mipExtend
	= tcu::IVec3(m_extent.width, m_extent.height, m_extent.depth);

	vk::VkDeviceSize arrayElemSize = 0;
	for (unsigned int i = 0; i < m_mipLevels && (mipExtend[0] > 1 || mipExtend[1] > 1 || mipExtend[2] > 1); ++i)
	{
		// Rect size is just a 3D image size;
		mipLevelSizes[i] = mipExtend[2] * depthPitch;

		arrayElemSize += mipLevelSizes[0];

		mipExtend = tcu::max(mipExtend / 2, tcu::IVec3(1));
	}

	vk::VkDeviceSize pixelOffset = arrayElement * arrayElemSize;
	for (size_t i = 0; i < mipLevel; ++i) {
		pixelOffset += mipLevelSizes[i];
	}
	pixelOffset += offset.z * mipLevelRectSizes[mipLevel];
	pixelOffset += offset.y * rowPitch;
	pixelOffset += offset.x;

	return pixelOffset;
}

void Image::bindMemory (de::MovePtr<vk::Allocation> allocation)
{
	if (allocation)
	{
		VK_CHECK(m_vk.bindImageMemory(m_device, *m_object, allocation->getMemory(), allocation->getOffset()));
	}
	else
	{
		VK_CHECK(m_vk.bindImageMemory(m_device, *m_object, DE_NULL, 0));
	}
	m_allocation = allocation;
}

de::SharedPtr<Image> Image::CreateAndAlloc(const vk::DeviceInterface	&vk,
										   vk::VkDevice					device,
										   const vk::VkImageCreateInfo 	&createInfo,
										   vk::Allocator 				&allocator,
										   vk::MemoryRequirement		memoryRequirement)
{
	de::SharedPtr<Image> ret = Create(vk, device, createInfo);

	vk::VkMemoryRequirements imageRequirements = vk::getImageMemoryRequirements(vk, device, ret->object());
	ret->bindMemory(allocator.allocate(imageRequirements, memoryRequirement));
	return ret;
}

de::SharedPtr<Image> Image::Create(const vk::DeviceInterface	&vk,
								   vk::VkDevice					device,
								   const vk::VkImageCreateInfo	&createInfo)
{
	return de::SharedPtr<Image>(new Image(vk, device, createInfo.format, createInfo.extent,
								createInfo.mipLevels, createInfo.arraySize,
								vk::createImage(vk, device, &createInfo)));
}

void transition2DImage (const vk::DeviceInterface	&vk, 
						vk::VkCmdBuffer				cmdBuffer, 
						vk::VkImage					image, 
						vk::VkImageAspectFlags		aspectMask, 
						vk::VkImageLayout			oldLayout,
						vk::VkImageLayout			newLayout)
{
	vk::VkImageMemoryBarrier barrier; 
	barrier.sType = vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = DE_NULL;
	barrier.outputMask = 0;
	barrier.inputMask = 0;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
	barrier.destQueueFamilyIndex = vk::VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.mipLevels = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.arraySize = 1;

	void* barriers[] = { &barrier };

	vk.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_GRAPHICS, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, DE_LENGTH_OF_ARRAY(barriers), barriers);
}


void initialTransitionColor2DImage (const vk::DeviceInterface &vk, vk::VkCmdBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout)
{
	transition2DImage(vk, cmdBuffer, image, vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, layout);
}

void initialTransitionDepth2DImage (const vk::DeviceInterface &vk, vk::VkCmdBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout)
{
	transition2DImage(vk, cmdBuffer, image, vk::VK_IMAGE_ASPECT_DEPTH_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, layout);
}

void initialTransitionStencil2DImage (const vk::DeviceInterface &vk, vk::VkCmdBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout)
{
	transition2DImage(vk, cmdBuffer, image, vk::VK_IMAGE_ASPECT_STENCIL_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, layout);
}

void initialTransitionDepthStencil2DImage (const vk::DeviceInterface &vk, vk::VkCmdBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout)
{
	transition2DImage(vk, cmdBuffer, image, vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, layout);
}

} //DynamicState
} //vkt
