#ifndef _VKTQUERYPOOLIMAGEOBJECTUTIL_HPP
#define _VKTQUERYPOOLIMAGEOBJECTUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Image Object Util
 *//*--------------------------------------------------------------------*/

#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"

#include "deSharedPtr.hpp"

#include "tcuTexture.hpp"

namespace vkt
{
namespace QueryPool
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

void transition2DImage (const vk::DeviceInterface&	vk,
						vk::VkCommandBuffer			cmdBuffer,
						vk::VkImage					image,
						vk::VkImageAspectFlags		aspectMask,
						vk::VkImageLayout			oldLayout,
						vk::VkImageLayout			newLayout,
						vk::VkAccessFlags			srcAccessMask = 0,
						vk::VkAccessFlags			dstAccessMask = 0);

void initialTransitionColor2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

void initialTransitionDepth2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

void initialTransitionStencil2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

void initialTransitionDepthStencil2DImage (const vk::DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image, vk::VkImageLayout layout);

} //QueryPool
} //vkt

#endif // _VKTQUERYPOOLIMAGEOBJECTUTIL_HPP
