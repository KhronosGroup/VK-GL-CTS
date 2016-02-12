#ifndef _VKTDRAWIMAGEOBJECTUTIL_HPP
#define _VKTDRAWIMAGEOBJECTUTIL_HPP
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

#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"

#include "deSharedPtr.hpp"

#include "tcuTexture.hpp"

namespace vkt
{
namespace Draw
{

class MemoryOp
{
public:
	static void pack	(int					pixelSize,
						 int					width,
						 int					height,
						 int					depth,
						 vk::VkDeviceSize		rowPitchOrZero,
						 vk::VkDeviceSize		depthPitchOrZero,
						 const void *			srcBuffer,
						 void *					destBuffer);

	static void unpack	(int					pixelSize,
						 int					width,
						 int					height,
						 int					depth,
						 vk::VkDeviceSize		rowPitchOrZero,
						 vk::VkDeviceSize		depthPitchOrZero,
						 const void *			srcBuffer,
						 void *					destBuffer);
};

class Image
{
public:
	static de::SharedPtr<Image> create				(const vk::DeviceInterface& vk, vk::VkDevice device, const vk::VkImageCreateInfo& createInfo);

	static de::SharedPtr<Image> createAndAlloc		(const vk::DeviceInterface&				vk,
													 vk::VkDevice							device,
													 const vk::VkImageCreateInfo&			createInfo,
													 vk::Allocator&							allocator,
													 vk::MemoryRequirement					memoryRequirement = vk::MemoryRequirement::Any);

	tcu::ConstPixelBufferAccess readSurface			(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	tcu::ConstPixelBufferAccess readSurface1D		(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	tcu::ConstPixelBufferAccess readVolume			(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	tcu::ConstPixelBufferAccess readSurfaceLinear	(vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	void						read				(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 vk::VkImageType						type,
													 void *									data);

	void						readUsingBuffer		(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 void *									data);

	void						readLinear			(vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 void *									data);

	void						uploadVolume		(const tcu::ConstPixelBufferAccess&		access,
													 vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	void						uploadSurface		 (const tcu::ConstPixelBufferAccess&	access,
														vk::VkQueue							queue,
														vk::Allocator&						allocator,
														vk::VkImageLayout					layout,
														vk::VkOffset3D						offset,
														vk::VkImageAspectFlagBits			aspect,
														unsigned int						mipLevel = 0,
														unsigned int						arrayElement = 0);

	void						uploadSurface1D		(const tcu::ConstPixelBufferAccess&		access,
													 vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	void						uploadSurfaceLinear	(const tcu::ConstPixelBufferAccess&		access,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 vk::VkImageAspectFlagBits				aspect,
													 unsigned int							mipLevel = 0,
													 unsigned int							arrayElement = 0);

	void						upload				(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 vk::VkImageType						type,
													 const void *							data);

	void						uploadUsingBuffer	(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 const void *							data);

	void						uploadLinear		(vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 const void *							data);

	de::SharedPtr<Image>		copyToLinearImage	(vk::VkQueue							queue,
													 vk::Allocator&							allocator,
													 vk::VkImageLayout						layout,
													 vk::VkOffset3D							offset,
													 int									width,
													 int									height,
													 int									depth,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement,
													 vk::VkImageAspectFlagBits				aspect,
													 vk::VkImageType						type);

	const vk::VkFormat&			getFormat			(void) const											{ return m_format;		}
	vk::VkImage					object				(void) const											{ return *m_object;		}
	void						bindMemory			(de::MovePtr<vk::Allocation>			allocation);
	vk::Allocation				getBoundMemory		(void) const											{ return *m_allocation; }

private:
	vk::VkDeviceSize			getPixelOffset		(vk::VkOffset3D							offset,
													 vk::VkDeviceSize						rowPitch,
													 vk::VkDeviceSize						depthPitch,
													 unsigned int							mipLevel,
													 unsigned int							arrayElement);

								Image				(const vk::DeviceInterface&				vk,
													 vk::VkDevice							device,
													 vk::VkFormat							format,
													 const vk::VkExtent3D&					extend,
													 deUint32								levelCount,
													 deUint32								layerCount,
													 vk::Move<vk::VkImage>					object);

	Image											(const Image& other);	// Not allowed!
	Image&						operator=			(const Image& other);	// Not allowed!

	de::MovePtr<vk::Allocation>	m_allocation;
	vk::Unique<vk::VkImage>		m_object;

	vk::VkFormat				m_format;
	vk::VkExtent3D				m_extent;
	deUint32					m_levelCount;
	deUint32					m_layerCount;

	std::vector<deUint8>		m_pixelAccessData;

	const vk::DeviceInterface&	m_vk;
	vk::VkDevice				m_device;
};

void transition2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageAspectFlags aspectMask, vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout);

void initialTransitionColor2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

void initialTransitionDepth2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

void initialTransitionStencil2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

void initialTransitionDepthStencil2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

} // Draw
} // vkt

#endif // _VKTDRAWIMAGEOBJECTUTIL_HPP
