/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Tests for render passses with multisample attachments
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassMultisampleTests.hpp"
#include "vktRenderPassTestsUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuFloat.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

using namespace vk;

using tcu::BVec4;
using tcu::IVec2;
using tcu::IVec4;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec4;

using tcu::Maybe;
using tcu::just;

using tcu::ConstPixelBufferAccess;
using tcu::PixelBufferAccess;

using tcu::TestLog;

using std::pair;
using std::string;
using std::vector;

typedef de::SharedPtr<vk::Unique<VkImage> > VkImageSp;
typedef de::SharedPtr<vk::Unique<VkImageView> > VkImageViewSp;
typedef de::SharedPtr<vk::Unique<VkBuffer> > VkBufferSp;
typedef de::SharedPtr<vk::Unique<VkPipeline> > VkPipelineSp;

namespace vkt
{
namespace
{
using namespace renderpass;

enum
{
	MAX_COLOR_ATTACHMENT_COUNT = 4u
};

enum TestSeparateUsage
{
	TEST_DEPTH	 = (1 << 0),
	TEST_STENCIL = (1 << 1)
};

template<typename T>
de::SharedPtr<T> safeSharedPtr (T* ptr)
{
	try
	{
		return de::SharedPtr<T>(ptr);
	}
	catch (...)
	{
		delete ptr;
		throw;
	}
}

VkImageAspectFlags getImageAspectFlags (VkFormat vkFormat)
{
	const tcu::TextureFormat	format		(mapVkFormat(vkFormat));
	const bool					hasDepth	(tcu::hasDepthComponent(format.order));
	const bool					hasStencil	(tcu::hasStencilComponent(format.order));

	if (hasDepth || hasStencil)
	{
		return (hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : (VkImageAspectFlagBits)0u)
				| (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : (VkImageAspectFlagBits)0u);
	}
	else
		return VK_IMAGE_ASPECT_COLOR_BIT;
}

void bindBufferMemory (const DeviceInterface& vk, VkDevice device, VkBuffer buffer, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	VK_CHECK(vk.bindBufferMemory(device, buffer, mem, memOffset));
}

void bindImageMemory (const DeviceInterface& vk, VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	VK_CHECK(vk.bindImageMemory(device, image, mem, memOffset));
}

de::MovePtr<Allocation> createBufferMemory (const DeviceInterface&	vk,
											VkDevice				device,
											Allocator&				allocator,
											VkBuffer				buffer)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), MemoryRequirement::HostVisible));
	bindBufferMemory(vk, device, buffer, allocation->getMemory(), allocation->getOffset());
	return allocation;
}

de::MovePtr<Allocation> createImageMemory (const DeviceInterface&	vk,
										   VkDevice					device,
										   Allocator&				allocator,
										   VkImage					image)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getImageMemoryRequirements(vk, device, image), MemoryRequirement::Any));
	bindImageMemory(vk, device, image, allocation->getMemory(), allocation->getOffset());
	return allocation;
}

Move<VkImage> createImage (const DeviceInterface&	vk,
						   VkDevice					device,
						   VkImageCreateFlags		flags,
						   VkImageType				imageType,
						   VkFormat					format,
						   VkExtent3D				extent,
						   deUint32					mipLevels,
						   deUint32					arrayLayers,
						   VkSampleCountFlagBits	samples,
						   VkImageTiling			tiling,
						   VkImageUsageFlags		usage,
						   VkSharingMode			sharingMode,
						   deUint32					queueFamilyCount,
						   const deUint32*			pQueueFamilyIndices,
						   VkImageLayout			initialLayout,
						   TestSeparateUsage		separateStencilUsage)
{
	VkImageUsageFlags depthUsage	= (separateStencilUsage == TEST_DEPTH)	 ? usage : (VkImageUsageFlags)VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	VkImageUsageFlags stencilUsage	= (separateStencilUsage == TEST_STENCIL) ? usage : (VkImageUsageFlags)VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	const VkImageStencilUsageCreateInfo stencilUsageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO,
		DE_NULL,
		stencilUsage
	};

	const VkImageCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		separateStencilUsage ? &stencilUsageInfo : DE_NULL,
		flags,
		imageType,
		format,
		extent,
		mipLevels,
		arrayLayers,
		samples,
		tiling,
		separateStencilUsage ? depthUsage : usage,
		sharingMode,
		queueFamilyCount,
		pQueueFamilyIndices,
		initialLayout
	};

	return createImage(vk, device, &pCreateInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&	vk,
								   VkDevice					device,
								   VkImageViewCreateFlags	flags,
								   VkImage					image,
								   VkImageViewType			viewType,
								   VkFormat					format,
								   VkComponentMapping		components,
								   VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		flags,
		image,
		viewType,
		format,
		components,
		subresourceRange,
	};
	return createImageView(vk, device, &pCreateInfo);
}

Move<VkImage> createImage (const InstanceInterface&	vki,
						   VkPhysicalDevice			physicalDevice,
						   const DeviceInterface&	vkd,
						   VkDevice					device,
						   VkFormat					vkFormat,
						   VkSampleCountFlagBits	sampleCountBit,
						   VkImageUsageFlags		usage,
						   deUint32					width,
						   deUint32					height,
						   TestSeparateUsage		separateStencilUsage = (TestSeparateUsage)0u)
{
	try
	{
		const tcu::TextureFormat		format					(mapVkFormat(vkFormat));
		const VkImageType				imageType				(VK_IMAGE_TYPE_2D);
		const VkImageTiling				imageTiling				(VK_IMAGE_TILING_OPTIMAL);
		const VkFormatProperties		formatProperties		(getPhysicalDeviceFormatProperties(vki, physicalDevice, vkFormat));
		const VkImageFormatProperties	imageFormatProperties	(getPhysicalDeviceImageFormatProperties(vki, physicalDevice, vkFormat, imageType, imageTiling, usage, 0u));
		const VkImageUsageFlags			depthUsage				= (separateStencilUsage == TEST_DEPTH)	 ? usage : (VkImageUsageFlags)VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		const VkImageUsageFlags			stencilUsage			= (separateStencilUsage == TEST_STENCIL) ? usage : (VkImageUsageFlags)VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		const VkExtent3D				imageExtent				=
		{
			width,
			height,
			1u
		};

		if ((tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order))
			&& (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
			TCU_THROW(NotSupportedError, "Format can't be used as depth stencil attachment");

		if (!(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order))
			&& (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
			TCU_THROW(NotSupportedError, "Format can't be used as color attachment");

		if (imageFormatProperties.maxExtent.width < imageExtent.width
			|| imageFormatProperties.maxExtent.height < imageExtent.height
			|| ((imageFormatProperties.sampleCounts & sampleCountBit) == 0))
		{
			TCU_THROW(NotSupportedError, "Image type not supported");
		}

		if (separateStencilUsage)
		{
			const VkImageStencilUsageCreateInfo	stencilUsageInfo =
			{
				VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO,				//	VkStructureType			sType
				DE_NULL,														//	const void*				pNext
				stencilUsage													//	VkImageUsageFlags		stencilUsage
			};

			const VkPhysicalDeviceImageFormatInfo2 formatInfo2 =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,			//	VkStructureType			sType
				&stencilUsageInfo,												//	const void*				pNext
				vkFormat,														//	VkFormat				format
				imageType,														//	VkImageType				type
				imageTiling,													//	VkImageTiling			tiling
				depthUsage,														//	VkImageUsageFlags		usage
				(VkImageCreateFlags)0u											//	VkImageCreateFlags		flags
			};

			VkImageFormatProperties2				extProperties =
			{
				VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
				DE_NULL,
			{
				{
					0,	// width
					0,	// height
					0,	// depth
				},
				0u,		// maxMipLevels
				0u,		// maxArrayLayers
				0,		// sampleCounts
				0u,		// maxResourceSize
			},
			};

			if ((vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &formatInfo2, &extProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
				|| extProperties.imageFormatProperties.maxExtent.width < imageExtent.width
				|| extProperties.imageFormatProperties.maxExtent.height < imageExtent.height
				|| ((extProperties.imageFormatProperties.sampleCounts & sampleCountBit) == 0))
			{
				TCU_THROW(NotSupportedError, "Image format not supported");
			}

		}

		return createImage(vkd, device, 0u, imageType, vkFormat, imageExtent, 1u, 1u, sampleCountBit, imageTiling, usage, VK_SHARING_MODE_EXCLUSIVE, 0u, DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED, separateStencilUsage);
	}
	catch (const vk::Error& error)
	{
		if (error.getError() == VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "Image format not supported");

		throw;
	}
}

Move<VkImageView> createImageAttachmentView (const DeviceInterface&	vkd,
											 VkDevice				device,
											 VkImage				image,
											 VkFormat				format,
											 VkImageAspectFlags		aspect)
{
	const VkImageSubresourceRange	range =
	{
		aspect,
		0u,
		1u,
		0u,
		1u
	};

	return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

Move<VkImageView> createSrcPrimaryInputImageView (const DeviceInterface&	vkd,
												  VkDevice					device,
												  VkImage					image,
												  VkFormat					format,
												  VkImageAspectFlags		aspect,
												  TestSeparateUsage			testSeparateUsage)
{
	VkImageAspectFlags primaryDepthStencilAspect = (testSeparateUsage == TEST_STENCIL) ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;

	const VkImageSubresourceRange	range =
	{
		aspect == (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT)
			? primaryDepthStencilAspect
			: aspect,
		0u,
		1u,
		0u,
		1u
	};

	return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

Move<VkImageView> createSrcSecondaryInputImageView (const DeviceInterface&	vkd,
													VkDevice				device,
													VkImage					image,
													VkFormat				format,
													VkImageAspectFlags		aspect,
													TestSeparateUsage		separateStencilUsage)
{
	if ((aspect == (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT)) && !separateStencilUsage)
	{
		const VkImageSubresourceRange	range =
		{
			VK_IMAGE_ASPECT_STENCIL_BIT,
			0u,
			1u,
			0u,
			1u
		};

		return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
	}
	else
		return Move<VkImageView>();
}

VkDeviceSize getPixelSize (VkFormat vkFormat)
{
	const tcu::TextureFormat	format	(mapVkFormat(vkFormat));

	return format.getPixelSize();
}

Move<VkBuffer> createBuffer (const DeviceInterface&		vkd,
							 VkDevice					device,
							 VkFormat					format,
							 deUint32					width,
							 deUint32					height)
{
	const VkBufferUsageFlags	bufferUsage			(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize			pixelSize			(getPixelSize(format));
	const VkBufferCreateInfo	createInfo			=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		width * height * pixelSize,
		bufferUsage,

		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL
	};
	return createBuffer(vkd, device, &createInfo);
}

VkSampleCountFlagBits sampleCountBitFromomSampleCount (deUint32 count)
{
	switch (count)
	{
		case 1:  return VK_SAMPLE_COUNT_1_BIT;
		case 2:  return VK_SAMPLE_COUNT_2_BIT;
		case 4:  return VK_SAMPLE_COUNT_4_BIT;
		case 8:  return VK_SAMPLE_COUNT_8_BIT;
		case 16: return VK_SAMPLE_COUNT_16_BIT;
		case 32: return VK_SAMPLE_COUNT_32_BIT;
		case 64: return VK_SAMPLE_COUNT_64_BIT;

		default:
			DE_FATAL("Invalid sample count");
			return (VkSampleCountFlagBits)(0x1u << count);
	}
}

std::vector<VkImageSp> createMultisampleImages (const InstanceInterface&	vki,
												VkPhysicalDevice			physicalDevice,
												const DeviceInterface&		vkd,
												VkDevice					device,
												VkFormat					format,
												deUint32					sampleCount,
												deUint32					width,
												deUint32					height)
{
	std::vector<VkImageSp> images (sampleCount);

	for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
		images[imageNdx] = safeSharedPtr(new vk::Unique<VkImage>(createImage(vki, physicalDevice, vkd, device, format, sampleCountBitFromomSampleCount(sampleCount), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, width, height)));

	return images;
}

std::vector<VkImageSp> createSingleSampleImages (const InstanceInterface&	vki,
												 VkPhysicalDevice			physicalDevice,
												 const DeviceInterface&		vkd,
												 VkDevice					device,
												 VkFormat					format,
												 deUint32					sampleCount,
												 deUint32					width,
												 deUint32					height)
{
	std::vector<VkImageSp> images (sampleCount);

	for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
		images[imageNdx] = safeSharedPtr(new vk::Unique<VkImage>(createImage(vki, physicalDevice, vkd, device, format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, width, height)));

	return images;
}

std::vector<de::SharedPtr<Allocation> > createImageMemory (const DeviceInterface&		vkd,
														   VkDevice						device,
														   Allocator&					allocator,
														   const std::vector<VkImageSp>	images)
{
	std::vector<de::SharedPtr<Allocation> > memory (images.size());

	for (size_t memoryNdx = 0; memoryNdx < memory.size(); memoryNdx++)
		memory[memoryNdx] = safeSharedPtr(createImageMemory(vkd, device, allocator, **images[memoryNdx]).release());

	return memory;
}

std::vector<VkImageViewSp> createImageAttachmentViews (const DeviceInterface&			vkd,
													   VkDevice							device,
													   const std::vector<VkImageSp>&	images,
													   VkFormat							format,
													   VkImageAspectFlagBits			aspect)
{
	std::vector<VkImageViewSp> views (images.size());

	for (size_t imageNdx = 0; imageNdx < images.size(); imageNdx++)
		views[imageNdx] = safeSharedPtr(new vk::Unique<VkImageView>(createImageAttachmentView(vkd, device, **images[imageNdx], format, aspect)));

	return views;
}

std::vector<VkBufferSp> createBuffers (const DeviceInterface&	vkd,
									   VkDevice					device,
									   VkFormat					format,
									   deUint32					sampleCount,
									   deUint32					width,
									   deUint32					height)
{
	std::vector<VkBufferSp> buffers (sampleCount);

	for (size_t bufferNdx = 0; bufferNdx < buffers.size(); bufferNdx++)
		buffers[bufferNdx] = safeSharedPtr(new vk::Unique<VkBuffer>(createBuffer(vkd, device, format, width, height)));

	return buffers;
}

std::vector<de::SharedPtr<Allocation> > createBufferMemory (const DeviceInterface&			vkd,
															VkDevice						device,
															Allocator&						allocator,
															const std::vector<VkBufferSp>	buffers)
{
	std::vector<de::SharedPtr<Allocation> > memory (buffers.size());

	for (size_t memoryNdx = 0; memoryNdx < memory.size(); memoryNdx++)
		memory[memoryNdx] = safeSharedPtr(createBufferMemory(vkd, device, allocator, **buffers[memoryNdx]).release());

	return memory;
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vkd,
									 VkDevice				device,
									 VkFormat				srcFormat,
									 VkFormat				dstFormat,
									 deUint32				sampleCount,
									 RenderingType			renderingType,
									 TestSeparateUsage		separateStencilUsage)
{
	const VkSampleCountFlagBits		samples						(sampleCountBitFromomSampleCount(sampleCount));
	const deUint32					splitSubpassCount			(deDivRoundUp32(sampleCount, MAX_COLOR_ATTACHMENT_COUNT));
	const tcu::TextureFormat		format						(mapVkFormat(srcFormat));
	const bool						isDepthStencilFormat		(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order));
	const VkImageAspectFlags		inputAspect					(separateStencilUsage == TEST_DEPTH ? (VkImageAspectFlags)VK_IMAGE_ASPECT_DEPTH_BIT
																: separateStencilUsage == TEST_STENCIL ? (VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT
																									   : getImageAspectFlags(srcFormat));
	vector<SubpassDesc>				subpasses;
	vector<vector<AttachmentRef> >	dstAttachmentRefs			(splitSubpassCount);
	vector<vector<AttachmentRef> >	dstResolveAttachmentRefs	(splitSubpassCount);
	vector<AttachmentDesc>			attachments;
	vector<SubpassDep>				dependencies;
	const AttachmentRef				srcAttachmentRef				//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		0u,															//  deUint32						attachment;					||  deUint32							attachment;
		isDepthStencilFormat										//  VkImageLayout					layout;						||  VkImageLayout						layout;
			? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		0u															//																||  VkImageAspectFlags					aspectMask;
	);
	const AttachmentRef				srcAttachmentInputRef			//  VkAttachmentReference										||  VkAttachmentReference2KHR
	(
																	//																||  VkStructureType						sType;
		DE_NULL,													//																||  const void*							pNext;
		0u,															//  deUint32						attachment;					||  deUint32							attachment;
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,					//  VkImageLayout					layout;						||  VkImageLayout						layout;
		(renderingType == RENDERING_TYPE_RENDERPASS2)				//																||  VkImageAspectFlags					aspectMask;
			? inputAspect
			: 0u
	);

	{
		const AttachmentDesc srcAttachment							//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
		(
																	//																||  VkStructureType						sType;
			DE_NULL,												//																||  const void*							pNext;
			0u,														//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
			srcFormat,												//  VkFormat						format;						||  VkFormat							format;
			samples,												//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,								//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
			VK_IMAGE_LAYOUT_GENERAL									//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
		);

		attachments.push_back(srcAttachment);
	}

	for (deUint32 splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
	{
		for (deUint32 sampleNdx = 0; sampleNdx < de::min((deUint32)MAX_COLOR_ATTACHMENT_COUNT, sampleCount  - splitSubpassIndex * MAX_COLOR_ATTACHMENT_COUNT); sampleNdx++)
		{
			// Multisample color attachment
			{
				const AttachmentDesc dstAttachment					//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
				(
																	//																||  VkStructureType						sType;
					DE_NULL,										//																||  const void*							pNext;
					0u,												//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
					dstFormat,										//  VkFormat						format;						||  VkFormat							format;
					samples,										//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,				//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
					VK_ATTACHMENT_STORE_OP_DONT_CARE,				//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
					VK_IMAGE_LAYOUT_UNDEFINED,						//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL		//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
				);
				const AttachmentRef dstAttachmentRef				//  VkAttachmentReference										||  VkAttachmentReference2KHR
				(
																	//																||  VkStructureType						sType;
					DE_NULL,										//																||  const void*							pNext;
					(deUint32)attachments.size(),					//  deUint32						attachment;					||  deUint32							attachment;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//  VkImageLayout					layout;						||  VkImageLayout						layout;
					0u												//																||  VkImageAspectFlags					aspectMask;
				);

				attachments.push_back(dstAttachment);
				dstAttachmentRefs[splitSubpassIndex].push_back(dstAttachmentRef);
			}
			// Resolve attachment
			{
				const AttachmentDesc dstAttachment					//  VkAttachmentDescription										||  VkAttachmentDescription2KHR
				(
																	//																||  VkStructureType						sType;
					DE_NULL,										//																||  const void*							pNext;
					0u,												//  VkAttachmentDescriptionFlags	flags;						||  VkAttachmentDescriptionFlags		flags;
					dstFormat,										//  VkFormat						format;						||  VkFormat							format;
					VK_SAMPLE_COUNT_1_BIT,							//  VkSampleCountFlagBits			samples;					||  VkSampleCountFlagBits				samples;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//  VkAttachmentLoadOp				loadOp;						||  VkAttachmentLoadOp					loadOp;
					VK_ATTACHMENT_STORE_OP_STORE,					//  VkAttachmentStoreOp				storeOp;					||  VkAttachmentStoreOp					storeOp;
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,				//  VkAttachmentLoadOp				stencilLoadOp;				||  VkAttachmentLoadOp					stencilLoadOp;
					VK_ATTACHMENT_STORE_OP_STORE,					//  VkAttachmentStoreOp				stencilStoreOp;				||  VkAttachmentStoreOp					stencilStoreOp;
					VK_IMAGE_LAYOUT_UNDEFINED,						//  VkImageLayout					initialLayout;				||  VkImageLayout						initialLayout;
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL			//  VkImageLayout					finalLayout;				||  VkImageLayout						finalLayout;
				);
				const AttachmentRef dstAttachmentRef				//  VkAttachmentReference										||  VkAttachmentReference2KHR
				(
																	//																||  VkStructureType						sType;
					DE_NULL,										//																||  const void*							pNext;
					(deUint32)attachments.size(),					//  deUint32						attachment;					||  deUint32							attachment;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		//  VkImageLayout					layout;						||  VkImageLayout						layout;
					0u												//																||  VkImageAspectFlags					aspectMask;
				);

				attachments.push_back(dstAttachment);
				dstResolveAttachmentRefs[splitSubpassIndex].push_back(dstAttachmentRef);
			}
		}
	}

	{
		{
			const SubpassDesc	subpass								//  VkSubpassDescription										||  VkSubpassDescription2KHR
			(
																	//																||  VkStructureType						sType;
				DE_NULL,											//																||  const void*							pNext;
				(VkSubpassDescriptionFlags)0,						//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
				VK_PIPELINE_BIND_POINT_GRAPHICS,					//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
				0u,													//																||  deUint32							viewMask;
				0u,													//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
				DE_NULL,											//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2KHR*	pInputAttachments;
				isDepthStencilFormat ? 0u : 1u,						//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
				isDepthStencilFormat ? DE_NULL : &srcAttachmentRef,	//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2KHR*	pColorAttachments;
				DE_NULL,											//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2KHR*	pResolveAttachments;
				isDepthStencilFormat ? &srcAttachmentRef : DE_NULL,	//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2KHR*	pDepthStencilAttachment;
				0u,													//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
				DE_NULL												//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
			);

			subpasses.push_back(subpass);
		}

		for (deUint32 splitSubpassIndex = 0; splitSubpassIndex < splitSubpassCount; splitSubpassIndex++)
		{
			{
				const SubpassDesc	subpass									//  VkSubpassDescription										||  VkSubpassDescription2KHR
				(
																			//																||  VkStructureType						sType;
					DE_NULL,												//																||  const void*							pNext;
					(VkSubpassDescriptionFlags)0,							//  VkSubpassDescriptionFlags		flags;						||  VkSubpassDescriptionFlags			flags;
					VK_PIPELINE_BIND_POINT_GRAPHICS,						//  VkPipelineBindPoint				pipelineBindPoint;			||  VkPipelineBindPoint					pipelineBindPoint;
					0u,														//																||  deUint32							viewMask;
					1u,														//  deUint32						inputAttachmentCount;		||  deUint32							inputAttachmentCount;
					&srcAttachmentInputRef,									//  const VkAttachmentReference*	pInputAttachments;			||  const VkAttachmentReference2KHR*	pInputAttachments;
					(deUint32)dstAttachmentRefs[splitSubpassIndex].size(),	//  deUint32						colorAttachmentCount;		||  deUint32							colorAttachmentCount;
					&dstAttachmentRefs[splitSubpassIndex][0],				//  const VkAttachmentReference*	pColorAttachments;			||  const VkAttachmentReference2KHR*	pColorAttachments;
					&dstResolveAttachmentRefs[splitSubpassIndex][0],		//  const VkAttachmentReference*	pResolveAttachments;		||  const VkAttachmentReference2KHR*	pResolveAttachments;
					DE_NULL,												//  const VkAttachmentReference*	pDepthStencilAttachment;	||  const VkAttachmentReference2KHR*	pDepthStencilAttachment;
					0u,														//  deUint32						preserveAttachmentCount;	||  deUint32							preserveAttachmentCount;
					DE_NULL													//  const deUint32*					pPreserveAttachments;		||  const deUint32*						pPreserveAttachments;
				);
				subpasses.push_back(subpass);
			}
			{
				const SubpassDep	dependency																//  VkSubpassDependency							||  VkSubpassDependency2KHR
				(
																											//												||	VkStructureType			sType;
					DE_NULL,																				//												||	const void*				pNext;
					0u,																						//  deUint32				srcSubpass;			||	deUint32				srcSubpass;
					splitSubpassIndex + 1,																	//  deUint32				dstSubpass;			||	deUint32				dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,											//  VkPipelineStageFlags	srcStageMask;		||	VkPipelineStageFlags	srcStageMask;
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,													//  VkPipelineStageFlags	dstStageMask;		||	VkPipelineStageFlags	dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,	//  VkAccessFlags			srcAccessMask;		||	VkAccessFlags			srcAccessMask;
					VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,													//  VkAccessFlags			dstAccessMask;		||	VkAccessFlags			dstAccessMask;
					VK_DEPENDENCY_BY_REGION_BIT,															//  VkDependencyFlags		dependencyFlags;	||	VkDependencyFlags		dependencyFlags;
					0u																						//												||	deInt32					viewOffset;
				);

				dependencies.push_back(dependency);
			}
		}
		// the last subpass must synchronize with all prior subpasses
		for (deUint32 splitSubpassIndex = 0; splitSubpassIndex < (splitSubpassCount - 1); splitSubpassIndex++)
		{
				const SubpassDep	dependency																//  VkSubpassDependency							||  VkSubpassDependency2KHR
				(
																											//												||	VkStructureType			sType;
					DE_NULL,																				//												||	const void*				pNext;
					splitSubpassIndex + 1,																	//  deUint32				srcSubpass;			||	deUint32				srcSubpass;
					splitSubpassCount,																		//  deUint32				dstSubpass;			||	deUint32				dstSubpass;
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
					| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,											//  VkPipelineStageFlags	srcStageMask;		||	VkPipelineStageFlags	srcStageMask;
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,													//  VkPipelineStageFlags	dstStageMask;		||	VkPipelineStageFlags	dstStageMask;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,	//  VkAccessFlags			srcAccessMask;		||	VkAccessFlags			srcAccessMask;
					VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,													//  VkAccessFlags			dstAccessMask;		||	VkAccessFlags			dstAccessMask;
					VK_DEPENDENCY_BY_REGION_BIT,															//  VkDependencyFlags		dependencyFlags;	||	VkDependencyFlags		dependencyFlags;
					0u																						//												||	deInt32					viewOffset;
				);
				dependencies.push_back(dependency);
		}
		const RenderPassCreateInfo	renderPassCreator						//  VkRenderPassCreateInfo										||  VkRenderPassCreateInfo2KHR
		(
																			//  VkStructureType					sType;						||  VkStructureType						sType;
			DE_NULL,														//  const void*						pNext;						||  const void*							pNext;
			(VkRenderPassCreateFlags)0u,									//  VkRenderPassCreateFlags			flags;						||  VkRenderPassCreateFlags				flags;
			(deUint32)attachments.size(),									//  deUint32						attachmentCount;			||  deUint32							attachmentCount;
			&attachments[0],												//  const VkAttachmentDescription*	pAttachments;				||  const VkAttachmentDescription2KHR*	pAttachments;
			(deUint32)subpasses.size(),										//  deUint32						subpassCount;				||  deUint32							subpassCount;
			&subpasses[0],													//  const VkSubpassDescription*		pSubpasses;					||  const VkSubpassDescription2KHR*		pSubpasses;
			(deUint32)dependencies.size(),									//  deUint32						dependencyCount;			||  deUint32							dependencyCount;
			&dependencies[0],												//  const VkSubpassDependency*		pDependencies;				||  const VkSubpassDependency2KHR*		pDependencies;
			0u,																//																||  deUint32							correlatedViewMaskCount;
			DE_NULL															//																||  const deUint32*						pCorrelatedViewMasks;
		);

		return renderPassCreator.createRenderPass(vkd, device);
	}
}

Move<VkRenderPass> createRenderPass (const DeviceInterface&		vkd,
									 VkDevice					device,
									 VkFormat					srcFormat,
									 VkFormat					dstFormat,
									 deUint32					sampleCount,
									 const RenderingType		renderingType,
									 const TestSeparateUsage	separateStencilUsage)
{
	switch (renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType, separateStencilUsage);
		case RENDERING_TYPE_RENDERPASS2:
			return createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vkd, device, srcFormat, dstFormat, sampleCount, renderingType, separateStencilUsage);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface&				vkd,
									   VkDevice								device,
									   VkRenderPass							renderPass,
									   VkImageView							srcImageView,
									   const std::vector<VkImageViewSp>&	dstMultisampleImageViews,
									   const std::vector<VkImageViewSp>&	dstSinglesampleImageViews,
									   deUint32								width,
									   deUint32								height)
{
	std::vector<VkImageView> attachments;

	attachments.reserve(dstMultisampleImageViews.size() + dstSinglesampleImageViews.size() + 1u);

	attachments.push_back(srcImageView);

	DE_ASSERT(dstMultisampleImageViews.size() == dstSinglesampleImageViews.size());

	for (size_t ndx = 0; ndx < dstMultisampleImageViews.size(); ndx++)
	{
		attachments.push_back(**dstMultisampleImageViews[ndx]);
		attachments.push_back(**dstSinglesampleImageViews[ndx]);
	}

	const VkFramebufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		0u,

		renderPass,
		(deUint32)attachments.size(),
		&attachments[0],

		width,
		height,
		1u
	};

	return createFramebuffer(vkd, device, &createInfo);
}

Move<VkPipelineLayout> createRenderPipelineLayout (const DeviceInterface&	vkd,
												   VkDevice					device)
{
	const VkPushConstantRange			pushConstant			=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		4u
	};
	const VkPipelineLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,

		0u,
		DE_NULL,

		1u,
		&pushConstant
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> createRenderPipeline (const DeviceInterface&		vkd,
									   VkDevice						device,
									   VkFormat						srcFormat,
									   VkRenderPass					renderPass,
									   VkPipelineLayout				pipelineLayout,
									   const vk::BinaryCollection&	binaryCollection,
									   deUint32						width,
									   deUint32						height,
									   deUint32						sampleCount)
{
	const tcu::TextureFormat		format						(mapVkFormat(srcFormat));
	const bool						isDepthStencilFormat		(tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order));

	const Unique<VkShaderModule>	vertexShaderModule			(createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule>	fragmentShaderModule		(createShaderModule(vkd, device, binaryCollection.get("quad-frag"), 0u));
	// Disable blending
	const VkPipelineColorBlendAttachmentState attachmentBlendState =
	{
		VK_FALSE,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
	};
	const VkPipelineVertexInputStateCreateInfo vertexInputState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0u,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};
	const std::vector<VkViewport>	viewports	(1, makeViewport(tcu::UVec2(width, height)));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(tcu::UVec2(width, height)));

	const VkPipelineMultisampleStateCreateInfo multisampleState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineMultisampleStateCreateFlags)0u,

		sampleCountBitFromomSampleCount(sampleCount),
		VK_FALSE,
		0.0f,
		DE_NULL,
		VK_FALSE,
		VK_FALSE,
	};
	const VkPipelineDepthStencilStateCreateInfo depthStencilState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineDepthStencilStateCreateFlags)0u,

		VK_TRUE,
		VK_TRUE,
		VK_COMPARE_OP_ALWAYS,
		VK_FALSE,
		VK_TRUE,
		{
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_INCREMENT_AND_WRAP,
			VK_STENCIL_OP_KEEP,
			VK_COMPARE_OP_ALWAYS,
			~0u,
			~0u,
			0xFFu / (sampleCount + 1)
		},
		{
			VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_INCREMENT_AND_WRAP,
			VK_STENCIL_OP_KEEP,
			VK_COMPARE_OP_ALWAYS,
			~0u,
			~0u,
			0xFFu / (sampleCount + 1)
		},

		0.0f,
		1.0f
	};
	const VkPipelineColorBlendStateCreateInfo blendState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineColorBlendStateCreateFlags)0u,

		VK_FALSE,
		VK_LOGIC_OP_COPY,
		(isDepthStencilFormat ? 0u : 1u),
		(isDepthStencilFormat ? DE_NULL : &attachmentBlendState),
		{ 0.0f, 0.0f, 0.0f, 0.0f }
	};

	return makeGraphicsPipeline(vkd,									// const DeviceInterface&                        vk
								device,									// const VkDevice                                device
								pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
								*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,								// const VkShaderModule                          geometryShaderModule
								*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
								renderPass,								// const VkRenderPass                            renderPass
								viewports,								// const std::vector<VkViewport>&                viewports
								scissors,								// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
								0u,										// const deUint32                                subpass
								0u,										// const deUint32                                patchControlPoints
								&vertexInputState,						// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&multisampleState,						// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								&depthStencilState,						// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
								&blendState);							// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
}

Move<VkDescriptorSetLayout> createSplitDescriptorSetLayout (const DeviceInterface&	vkd,
															VkDevice				device,
															VkFormat				vkFormat)
{
	const tcu::TextureFormat				format		(mapVkFormat(vkFormat));
	const bool								hasDepth	(tcu::hasDepthComponent(format.order));
	const bool								hasStencil	(tcu::hasStencilComponent(format.order));
	const VkDescriptorSetLayoutBinding		bindings[]	=
	{
		{
			0u,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			1u,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			DE_NULL
		},
		{
			1u,
			VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
			1u,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			DE_NULL
		}
	};
	const VkDescriptorSetLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		0u,

		hasDepth && hasStencil ? 2u : 1u,
		bindings
	};

	return createDescriptorSetLayout(vkd, device, &createInfo);
}

Move<VkPipelineLayout> createSplitPipelineLayout (const DeviceInterface&	vkd,
												  VkDevice					device,
												  VkDescriptorSetLayout		descriptorSetLayout)
{
	const VkPushConstantRange			pushConstant			=
	{
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		4u
	};
	const VkPipelineLayoutCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,

		1u,
		&descriptorSetLayout,

		1u,
		&pushConstant
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> createSplitPipeline (const DeviceInterface&		vkd,
									  VkDevice						device,
									  VkRenderPass					renderPass,
									  deUint32						subpassIndex,
									  VkPipelineLayout				pipelineLayout,
									  const vk::BinaryCollection&	binaryCollection,
									  deUint32						width,
									  deUint32						height,
									  deUint32						sampleCount)
{
	const Unique<VkShaderModule>	vertexShaderModule			(createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule>	fragmentShaderModule		(createShaderModule(vkd, device, binaryCollection.get("quad-split-frag"), 0u));
	// Disable blending
	const VkPipelineColorBlendAttachmentState attachmentBlendState =
	{
		VK_FALSE,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
	};
	const std::vector<VkPipelineColorBlendAttachmentState> attachmentBlendStates (de::min((deUint32)MAX_COLOR_ATTACHMENT_COUNT, sampleCount), attachmentBlendState);
	const VkPipelineVertexInputStateCreateInfo vertexInputState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineVertexInputStateCreateFlags)0u,

		0u,
		DE_NULL,

		0u,
		DE_NULL
	};
	const std::vector<VkViewport>	viewports	(1, makeViewport(tcu::UVec2(width, height)));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(tcu::UVec2(width, height)));

	const VkPipelineMultisampleStateCreateInfo multisampleState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineMultisampleStateCreateFlags)0u,

		sampleCountBitFromomSampleCount(sampleCount),
		VK_FALSE,
		0.0f,
		DE_NULL,
		VK_FALSE,
		VK_FALSE,
	};
	const VkPipelineColorBlendStateCreateInfo blendState =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		DE_NULL,
		(VkPipelineColorBlendStateCreateFlags)0u,

		VK_FALSE,
		VK_LOGIC_OP_COPY,

		(deUint32)attachmentBlendStates.size(),
		&attachmentBlendStates[0],

		{ 0.0f, 0.0f, 0.0f, 0.0f }
	};

	return makeGraphicsPipeline(vkd,									// const DeviceInterface&                        vk
								device,									// const VkDevice                                device
								pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
								*vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,								// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,								// const VkShaderModule                          geometryShaderModule
								*fragmentShaderModule,					// const VkShaderModule                          fragmentShaderModule
								renderPass,								// const VkRenderPass                            renderPass
								viewports,								// const std::vector<VkViewport>&                viewports
								scissors,								// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
								subpassIndex,							// const deUint32                                subpass
								0u,										// const deUint32                                patchControlPoints
								&vertexInputState,						// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&multisampleState,						// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
								&blendState);							// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
}

vector<VkPipelineSp> createSplitPipelines (const DeviceInterface&		vkd,
										 VkDevice						device,
										 VkRenderPass					renderPass,
										 VkPipelineLayout				pipelineLayout,
										 const vk::BinaryCollection&	binaryCollection,
										 deUint32						width,
										 deUint32						height,
										 deUint32						sampleCount)
{
	std::vector<VkPipelineSp> pipelines (deDivRoundUp32(sampleCount, MAX_COLOR_ATTACHMENT_COUNT), (VkPipelineSp)0u);

	for (size_t ndx = 0; ndx < pipelines.size(); ndx++)
		pipelines[ndx] = safeSharedPtr(new Unique<VkPipeline>(createSplitPipeline(vkd, device, renderPass, (deUint32)(ndx + 1), pipelineLayout, binaryCollection, width, height, sampleCount)));

	return pipelines;
}

Move<VkDescriptorPool> createSplitDescriptorPool (const DeviceInterface&	vkd,
												  VkDevice					device)
{
	const VkDescriptorPoolSize			size		=
	{
		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2u
	};
	const VkDescriptorPoolCreateInfo	createInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,


		2u,
		1u,
		&size
	};

	return createDescriptorPool(vkd, device, &createInfo);
}

Move<VkDescriptorSet> createSplitDescriptorSet (const DeviceInterface&	vkd,
												VkDevice				device,
												VkDescriptorPool		pool,
												VkDescriptorSetLayout	layout,
												VkImageView				primaryImageView,
												VkImageView				secondaryImageView)
{
	const VkDescriptorSetAllocateInfo	allocateInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,

		pool,
		1u,
		&layout
	};
	Move<VkDescriptorSet> set (allocateDescriptorSet(vkd, device, &allocateInfo));

	{
		const VkDescriptorImageInfo	imageInfos[]	=
		{
			{
				(VkSampler)0u,
				primaryImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			},
			{
				(VkSampler)0u,
				secondaryImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			}
		};
		const VkWriteDescriptorSet	writes[]	=
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				DE_NULL,

				*set,
				0u,
				0u,
				1u,
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				&imageInfos[0],
				DE_NULL,
				DE_NULL
			},
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				DE_NULL,

				*set,
				1u,
				0u,
				1u,
				VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				&imageInfos[1],
				DE_NULL,
				DE_NULL
			}
		};
		const deUint32	count	= secondaryImageView != (VkImageView)0
								? 2u
								: 1u;

		vkd.updateDescriptorSets(device, count, writes, 0u, DE_NULL);
	}
	return set;
}

struct TestConfig
{
				TestConfig		(VkFormat			format_,
								 deUint32			sampleCount_,
								 RenderingType		renderingType_,
								 TestSeparateUsage	separateStencilUsage_ = (TestSeparateUsage)0u)
		: format			(format_)
		, sampleCount		(sampleCount_)
		, renderingType		(renderingType_)
		, separateStencilUsage(separateStencilUsage_)
	{
	}

	VkFormat			format;
	deUint32			sampleCount;
	RenderingType		renderingType;
	TestSeparateUsage	separateStencilUsage;
};

VkImageUsageFlags getSrcImageUsage (VkFormat vkFormat)
{
	const tcu::TextureFormat	format		(mapVkFormat(vkFormat));
	const bool					hasDepth	(tcu::hasDepthComponent(format.order));
	const bool					hasStencil	(tcu::hasStencilComponent(format.order));

	if (hasDepth || hasStencil)
		return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	else
		return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
}

VkFormat getDstFormat (VkFormat vkFormat, TestSeparateUsage separateStencilUsage)
{
	const tcu::TextureFormat	format		(mapVkFormat(vkFormat));
	const bool					hasDepth	(tcu::hasDepthComponent(format.order));
	const bool					hasStencil	(tcu::hasStencilComponent(format.order));

	if (hasDepth && hasStencil && !separateStencilUsage)
		return VK_FORMAT_R32G32_SFLOAT;
	else if (hasDepth || hasStencil)
		return VK_FORMAT_R32_SFLOAT;
	else
		return vkFormat;
}

bool isExtensionSupported(Context& context, RenderingType renderingType, TestSeparateUsage separateStencilUsage, VkFormat format)
{
	if (renderingType == RENDERING_TYPE_RENDERPASS2)
		context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	if (separateStencilUsage)
	{
		context.requireDeviceFunctionality	("VK_EXT_separate_stencil_usage");
		context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	}

#ifndef CTS_USES_VULKANSC
	if (format == VK_FORMAT_A8_UNORM_KHR)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
#else
	DE_UNREF(format);
#endif // CTS_USES_VULKANSC

	return true;
}


class MultisampleRenderPassTestInstance : public TestInstance
{
public:
					MultisampleRenderPassTestInstance	(Context& context, TestConfig config);
					~MultisampleRenderPassTestInstance	(void);

	tcu::TestStatus	iterate								(void);

	template<typename RenderpassSubpass>
	tcu::TestStatus	iterateInternal						(void);

private:
	const bool										m_extensionSupported;
	const RenderingType								m_renderingType;
	const TestSeparateUsage							m_separateStencilUsage;

	const VkFormat									m_srcFormat;
	const VkFormat									m_dstFormat;
	const deUint32									m_sampleCount;
	const deUint32									m_width;
	const deUint32									m_height;

	const VkImageAspectFlags						m_srcImageAspect;
	const VkImageUsageFlags							m_srcImageUsage;
	const Unique<VkImage>							m_srcImage;
	const de::UniquePtr<Allocation>					m_srcImageMemory;
	const Unique<VkImageView>						m_srcImageView;
	const Unique<VkImageView>						m_srcPrimaryInputImageView;
	const Unique<VkImageView>						m_srcSecondaryInputImageView;

	const std::vector<VkImageSp>					m_dstMultisampleImages;
	const std::vector<de::SharedPtr<Allocation> >	m_dstMultisampleImageMemory;
	const std::vector<VkImageViewSp>				m_dstMultisampleImageViews;

	const std::vector<VkImageSp>					m_dstSinglesampleImages;
	const std::vector<de::SharedPtr<Allocation> >	m_dstSinglesampleImageMemory;
	const std::vector<VkImageViewSp>				m_dstSinglesampleImageViews;

	const std::vector<VkBufferSp>					m_dstBuffers;
	const std::vector<de::SharedPtr<Allocation> >	m_dstBufferMemory;

	const Unique<VkRenderPass>						m_renderPass;
	const Unique<VkFramebuffer>						m_framebuffer;

	const Unique<VkPipelineLayout>					m_renderPipelineLayout;
	const Unique<VkPipeline>						m_renderPipeline;

	const Unique<VkDescriptorSetLayout>				m_splitDescriptorSetLayout;
	const Unique<VkPipelineLayout>					m_splitPipelineLayout;
	const std::vector<VkPipelineSp>					m_splitPipelines;
	const Unique<VkDescriptorPool>					m_splitDescriptorPool;
	const Unique<VkDescriptorSet>					m_splitDescriptorSet;

	const Unique<VkCommandPool>						m_commandPool;
	tcu::ResultCollector							m_resultCollector;
};

MultisampleRenderPassTestInstance::MultisampleRenderPassTestInstance (Context& context, TestConfig config)
	: TestInstance					(context)
	, m_extensionSupported			(isExtensionSupported(context, config.renderingType, config.separateStencilUsage, config.format))
	, m_renderingType				(config.renderingType)
	, m_separateStencilUsage		(config.separateStencilUsage)
	, m_srcFormat					(config.format)
	, m_dstFormat					(getDstFormat(config.format, config.separateStencilUsage))
	, m_sampleCount					(config.sampleCount)
	, m_width						(32u)
	, m_height						(32u)

	, m_srcImageAspect				(getImageAspectFlags(m_srcFormat))
	, m_srcImageUsage				(getSrcImageUsage(m_srcFormat))
	, m_srcImage					(createImage(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(), context.getDevice(), m_srcFormat, sampleCountBitFromomSampleCount(m_sampleCount), m_srcImageUsage, m_width, m_height, m_separateStencilUsage))
	, m_srcImageMemory				(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_srcImage))
	, m_srcImageView				(createImageAttachmentView(context.getDeviceInterface(), context.getDevice(), *m_srcImage, m_srcFormat, m_srcImageAspect))
	, m_srcPrimaryInputImageView	(createSrcPrimaryInputImageView(context.getDeviceInterface(), context.getDevice(), *m_srcImage, m_srcFormat, m_srcImageAspect, m_separateStencilUsage))
	, m_srcSecondaryInputImageView	(createSrcSecondaryInputImageView(context.getDeviceInterface(), context.getDevice(), *m_srcImage, m_srcFormat, m_srcImageAspect, m_separateStencilUsage))

	, m_dstMultisampleImages		(createMultisampleImages(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(), context.getDevice(), m_dstFormat, m_sampleCount, m_width, m_height))
	, m_dstMultisampleImageMemory	(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_dstMultisampleImages))
	, m_dstMultisampleImageViews	(createImageAttachmentViews(context.getDeviceInterface(), context.getDevice(), m_dstMultisampleImages, m_dstFormat, VK_IMAGE_ASPECT_COLOR_BIT))

	, m_dstSinglesampleImages		(createSingleSampleImages(context.getInstanceInterface(), context.getPhysicalDevice(), context.getDeviceInterface(), context.getDevice(), m_dstFormat, m_sampleCount, m_width, m_height))
	, m_dstSinglesampleImageMemory	(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_dstSinglesampleImages))
	, m_dstSinglesampleImageViews	(createImageAttachmentViews(context.getDeviceInterface(), context.getDevice(), m_dstSinglesampleImages, m_dstFormat, VK_IMAGE_ASPECT_COLOR_BIT))

	, m_dstBuffers					(createBuffers(context.getDeviceInterface(), context.getDevice(), m_dstFormat, m_sampleCount, m_width, m_height))
	, m_dstBufferMemory				(createBufferMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), m_dstBuffers))

	, m_renderPass					(createRenderPass(context.getDeviceInterface(), context.getDevice(), m_srcFormat, m_dstFormat, m_sampleCount, config.renderingType, m_separateStencilUsage))
	, m_framebuffer					(createFramebuffer(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_srcImageView, m_dstMultisampleImageViews, m_dstSinglesampleImageViews, m_width, m_height))

	, m_renderPipelineLayout		(createRenderPipelineLayout(context.getDeviceInterface(), context.getDevice()))
	, m_renderPipeline				(createRenderPipeline(context.getDeviceInterface(), context.getDevice(), m_srcFormat, *m_renderPass, *m_renderPipelineLayout, context.getBinaryCollection(), m_width, m_height, m_sampleCount))

	, m_splitDescriptorSetLayout	(createSplitDescriptorSetLayout(context.getDeviceInterface(), context.getDevice(), m_srcFormat))
	, m_splitPipelineLayout			(createSplitPipelineLayout(context.getDeviceInterface(), context.getDevice(), *m_splitDescriptorSetLayout))
	, m_splitPipelines				(createSplitPipelines(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_splitPipelineLayout, context.getBinaryCollection(), m_width, m_height, m_sampleCount))
	, m_splitDescriptorPool			(createSplitDescriptorPool(context.getDeviceInterface(), context.getDevice()))
	, m_splitDescriptorSet			(createSplitDescriptorSet(context.getDeviceInterface(), context.getDevice(), *m_splitDescriptorPool, *m_splitDescriptorSetLayout, *m_srcPrimaryInputImageView, *m_srcSecondaryInputImageView))
	, m_commandPool					(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex()))
{
}

MultisampleRenderPassTestInstance::~MultisampleRenderPassTestInstance (void)
{
}

tcu::TestStatus MultisampleRenderPassTestInstance::iterate (void)
{
	switch (m_renderingType)
	{
		case RENDERING_TYPE_RENDERPASS_LEGACY:
			return iterateInternal<RenderpassSubpass1>();
		case RENDERING_TYPE_RENDERPASS2:
			return iterateInternal<RenderpassSubpass2>();
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
tcu::TestStatus MultisampleRenderPassTestInstance::iterateInternal (void)
{
	const DeviceInterface&								vkd					(m_context.getDeviceInterface());
	const VkDevice										device				(m_context.getDevice());
	const Unique<VkCommandBuffer>						commandBuffer		(allocateCommandBuffer(vkd, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	beginCommandBuffer(vkd, *commandBuffer);

	{
		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			DE_NULL,

			*m_renderPass,
			*m_framebuffer,

			{
				{ 0u, 0u },
				{ m_width, m_height }
			},

			0u,
			DE_NULL
		};
		RenderpassSubpass::cmdBeginRenderPass(vkd, *commandBuffer, &beginInfo, &subpassBeginInfo);

		// Stencil needs to be cleared if it exists.
		if (tcu::hasStencilComponent(mapVkFormat(m_srcFormat).order))
		{
			const VkClearAttachment clearAttachment =
			{
				VK_IMAGE_ASPECT_STENCIL_BIT,						// VkImageAspectFlags	aspectMask;
				0,													// deUint32				colorAttachment;
				makeClearValueDepthStencil(0, 0)					// VkClearValue			clearValue;
			};

			const VkClearRect clearRect =
			{
				{
					{ 0u, 0u },
					{ m_width, m_height }
				},
				0,													// deUint32	baseArrayLayer;
				1													// deUint32	layerCount;
			};

			vkd.cmdClearAttachments(*commandBuffer, 1, &clearAttachment, 1, &clearRect);
		}
	}

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);

	for (deUint32 sampleNdx = 0; sampleNdx < m_sampleCount; sampleNdx++)
	{
		vkd.cmdPushConstants(*commandBuffer, *m_renderPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(sampleNdx), &sampleNdx);
		vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	}

	for (deUint32 splitPipelineNdx = 0; splitPipelineNdx < m_splitPipelines.size(); splitPipelineNdx++)
	{
		RenderpassSubpass::cmdNextSubpass(vkd, *commandBuffer, &subpassBeginInfo, &subpassEndInfo);

		vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **m_splitPipelines[splitPipelineNdx]);
		vkd.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_splitPipelineLayout, 0u, 1u,  &*m_splitDescriptorSet, 0u, DE_NULL);
		vkd.cmdPushConstants(*commandBuffer, *m_splitPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(splitPipelineNdx), &splitPipelineNdx);
		vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	}

	RenderpassSubpass::cmdEndRenderPass(vkd, *commandBuffer, &subpassEndInfo);

	for (size_t dstNdx = 0; dstNdx < m_dstSinglesampleImages.size(); dstNdx++)
		copyImageToBuffer(vkd, *commandBuffer, **m_dstSinglesampleImages[dstNdx], **m_dstBuffers[dstNdx], tcu::IVec2(m_width, m_height), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	endCommandBuffer(vkd, *commandBuffer);

	submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *commandBuffer);

	{
		const tcu::TextureFormat		format			(mapVkFormat(m_dstFormat));
		const tcu::TextureFormat		srcFormat		(mapVkFormat(m_srcFormat));
		const bool						verifyDepth		(m_separateStencilUsage ? (m_separateStencilUsage == TEST_DEPTH)   : tcu::hasDepthComponent(srcFormat.order));
		const bool						verifyStencil	(m_separateStencilUsage ? (m_separateStencilUsage == TEST_STENCIL) : tcu::hasStencilComponent(srcFormat.order));

		for (deUint32 sampleNdx = 0; sampleNdx < m_sampleCount; sampleNdx++)
		{
			Allocation *dstBufMem = m_dstBufferMemory[sampleNdx].get();
			invalidateAlloc(vkd, device, *dstBufMem);

			const std::string					name		("Sample" + de::toString(sampleNdx));
			const void* const					ptr			(dstBufMem->getHostPtr());
			const tcu::ConstPixelBufferAccess	access		(format, m_width, m_height, 1, ptr);
			tcu::TextureLevel					reference	(format, m_width, m_height);

			if (verifyDepth || verifyStencil)
			{
				if (verifyDepth)
				{
					for (deUint32 y = 0; y < m_height; y++)
					for (deUint32 x = 0; x < m_width; x++)
					{
						const deUint32	x1				= x ^ sampleNdx;
						const deUint32	y1				= y ^ sampleNdx;
						const float		range			= 1.0f;
						float			depth			= 0.0f;
						deUint32		divider			= 2;

						// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
						for (size_t bitNdx = 0; bitNdx < 10; bitNdx++)
						{
							depth += (range / (float)divider)
									* (((bitNdx % 2 == 0 ? x1 : y1) & (0x1u << (bitNdx / 2u))) == 0u ? 0u : 1u);
							divider *= 2;
						}

						reference.getAccess().setPixel(Vec4(depth, 0.0f, 0.0f, 0.0f), x, y);
					}
				}
				if (verifyStencil)
				{
					for (deUint32 y = 0; y < m_height; y++)
					for (deUint32 x = 0; x < m_width; x++)
					{
						const deUint32	stencil	= sampleNdx + 1u;

						if (verifyDepth)
						{
							const Vec4 src (reference.getAccess().getPixel(x, y));

							reference.getAccess().setPixel(Vec4(src.x(), (float)stencil, 0.0f, 0.0f), x, y);
						}
						else
							reference.getAccess().setPixel(Vec4((float)stencil, 0.0f, 0.0f, 0.0f), x, y);
					}
				}
				{
					const Vec4 threshold (verifyDepth ? (1.0f / 1024.0f) : 0.0f, 0.0f, 0.0f, 0.0f);

					if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), name.c_str(), reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
						m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));
				}
			}
			else
			{
				const tcu::TextureChannelClass	channelClass	(tcu::getTextureChannelClass(format.type));

				switch (channelClass)
				{
					case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
					{
						const UVec4		bits			(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
						const UVec4		minValue		(0);
						const UVec4		range			(UVec4(1u) << tcu::min(bits, UVec4(31)));
						const int		componentCount	(tcu::getNumUsedChannels(format.order));
						const deUint32	bitSize			(bits[0] + bits[1] + bits[2] + bits[3]);

						for (deUint32 y = 0; y < m_height; y++)
						for (deUint32 x = 0; x < m_width; x++)
						{
							const deUint32	x1				= x ^ sampleNdx;
							const deUint32	y1				= y ^ sampleNdx;
							UVec4			color			(minValue);
							deUint32		dstBitsUsed[4]	= { 0u, 0u, 0u, 0u };
							deUint32		nextSrcBit		= 0;
							deUint32		divider			= 2;

							// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
							while (nextSrcBit < de::min(bitSize, 10u))
							{
								for (int compNdx = 0; compNdx < componentCount; compNdx++)
								{
									if (dstBitsUsed[compNdx] > bits[compNdx])
										continue;

									color[compNdx] += (range[compNdx] / divider)
													* (((nextSrcBit % 2 == 0 ? x1 : y1) & (0x1u << (nextSrcBit / 2u))) == 0u ? 0u : 1u);

									nextSrcBit++;
									dstBitsUsed[compNdx]++;
								}

								divider *= 2;
							}

							reference.getAccess().setPixel(color, x, y);
						}

						if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), name.c_str(), reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
							m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));

						break;
					}

					case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
					{
						const UVec4		bits			(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
						const IVec4		minValue		(0);
						const IVec4		range			((UVec4(1u) << tcu::min(bits, UVec4(30))).cast<deInt32>());
						const int		componentCount	(tcu::getNumUsedChannels(format.order));
						const deUint32	bitSize			(bits[0] + bits[1] + bits[2] + bits[3]);

						for (deUint32 y = 0; y < m_height; y++)
						for (deUint32 x = 0; x < m_width; x++)
						{
							const deUint32	x1				= x ^ sampleNdx;
							const deUint32	y1				= y ^ sampleNdx;
							IVec4			color			(minValue);
							deUint32		dstBitsUsed[4]	= { 0u, 0u, 0u, 0u };
							deUint32		nextSrcBit		= 0;
							deUint32		divider			= 2;

							// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
							while (nextSrcBit < de::min(bitSize, 10u))
							{
								for (int compNdx = 0; compNdx < componentCount; compNdx++)
								{
									if (dstBitsUsed[compNdx] > bits[compNdx])
										continue;

									color[compNdx] += (range[compNdx] / divider)
													* (((nextSrcBit % 2 == 0 ? x1 : y1) & (0x1u << (nextSrcBit / 2u))) == 0u ? 0u : 1u);

									nextSrcBit++;
									dstBitsUsed[compNdx]++;
								}

								divider *= 2;
							}

							reference.getAccess().setPixel(color, x, y);
						}

						if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), name.c_str(), reference.getAccess(), access, UVec4(0u), tcu::COMPARE_LOG_ON_ERROR))
							m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));

						break;
					}

					case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
					case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
					case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
					{
						const tcu::TextureFormatInfo	info			(tcu::getTextureFormatInfo(format));
						const UVec4						bits			(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
						const Vec4						minLimit		(-65536.0);
						const Vec4						maxLimit		(65536.0);
						const Vec4						minValue		(tcu::max(info.valueMin, minLimit));
						const Vec4						range			(tcu::min(info.valueMax, maxLimit) - minValue);
						const bool						isAlphaOnly		= isAlphaOnlyFormat(m_dstFormat);
						const int						componentCount	(isAlphaOnly ? 4 : tcu::getNumUsedChannels(format.order));
						const deUint32					bitSize			(bits[0] + bits[1] + bits[2] + bits[3]);

						for (deUint32 y = 0; y < m_height; y++)
						for (deUint32 x = 0; x < m_width; x++)
						{
							const deUint32	x1				= x ^ sampleNdx;
							const deUint32	y1				= y ^ sampleNdx;
							Vec4			color			(minValue);
							deUint32		dstBitsUsed[4]	= { 0u, 0u, 0u, 0u };
							deUint32		nextSrcBit		= 0;
							deUint32		divider			= 2;

							// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
							while (nextSrcBit < de::min(bitSize, 10u))
							{
								for (int compNdx = 0; compNdx < componentCount; compNdx++)
								{
									if (dstBitsUsed[compNdx] > bits[compNdx])
										continue;

									color[compNdx] += (range[compNdx] / (float)divider)
													* (((nextSrcBit % 2 == 0 ? x1 : y1) & (0x1u << (nextSrcBit / 2u))) == 0u ? 0u : 1u);

									nextSrcBit++;
									dstBitsUsed[compNdx]++;
								}

								divider *= 2;
							}

							if (tcu::isSRGB(format))
								reference.getAccess().setPixel(tcu::linearToSRGB(color), x, y);
							else
								reference.getAccess().setPixel(color, x, y);
						}

						if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
						{
							// Convert target format ulps to float ulps and allow 64ulp differences
							const UVec4 threshold (64u * (UVec4(1u) << (UVec4(23) - tcu::getTextureFormatMantissaBitDepth(format).cast<deUint32>())));

							if (!tcu::floatUlpThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), name.c_str(), reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
								m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));
						}
						else
						{
							// Allow error of 4 times the minimum presentable difference
							const Vec4 threshold (4.0f * 1.0f / ((UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(format).cast<deUint32>()) - 1u).cast<float>());

							if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), name.c_str(), name.c_str(), reference.getAccess(), access, threshold, tcu::COMPARE_LOG_ON_ERROR))
								m_resultCollector.fail("Compare failed for sample " + de::toString(sampleNdx));
						}

						break;
					}

					default:
						DE_FATAL("Unknown channel class");
				}
			}
		}
	}

	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

struct Programs
{
	void init (vk::SourceCollections& dst, TestConfig config) const
	{
		const tcu::TextureFormat		format			(mapVkFormat(config.format));
		const tcu::TextureChannelClass	channelClass	(tcu::getTextureChannelClass(format.type));
		const bool						testDepth		(config.separateStencilUsage ? (config.separateStencilUsage == TEST_DEPTH) : tcu::hasDepthComponent(format.order));
		const bool						testStencil		(config.separateStencilUsage ? (config.separateStencilUsage == TEST_STENCIL) : tcu::hasStencilComponent(format.order));

		dst.glslSources.add("quad-vert") << glu::VertexSource(
			"#version 450\n"
			"out gl_PerVertex {\n"
			"\tvec4 gl_Position;\n"
			"};\n"
			"highp float;\n"
			"void main (void) {\n"
			"\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			"\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			"}\n");

		if (testDepth)
		{
			const Vec4			minValue		(0.0f);
			const Vec4			range			(1.0f);
			std::ostringstream	fragmentShader;

			fragmentShader <<
				"#version 450\n"
				"layout(push_constant) uniform PushConstant {\n"
				"\thighp uint sampleIndex;\n"
				"} pushConstants;\n"
				"void main (void)\n"
				"{\n"
				"\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
				"\tgl_SampleMask[0] = int((~0x0u) << sampleIndex);\n"
				"\thighp float depth;\n"
				"\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
				"\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

			fragmentShader << "\tdepth = "  << minValue[0] << ";\n";

			{
				deUint32 divider = 2;

				// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
				for (size_t bitNdx = 0; bitNdx < 10; bitNdx++)
				{
					fragmentShader <<
							"\tdepth += " << (range[0] / (float)divider)
							<< " * float(bitfieldExtract(" << (bitNdx % 2 == 0 ? "x" : "y") << ", " << (bitNdx / 2) << ", 1));\n";

					divider *= 2;
				}
			}

			fragmentShader <<
				"\tgl_FragDepth = depth;\n"
				"}\n";

			dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
		}
		else if (testStencil)
		{
			dst.glslSources.add("quad-frag") << glu::FragmentSource(
				"#version 450\n"
				"layout(push_constant) uniform PushConstant {\n"
				"\thighp uint sampleIndex;\n"
				"} pushConstants;\n"
				"void main (void)\n"
				"{\n"
				"\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
				"\tgl_SampleMask[0] = int((~0x0u) << sampleIndex);\n"
				"}\n");
		}
		else
		{
			switch (channelClass)
			{
				case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
				{
					const UVec4	bits		(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
					const UVec4 minValue	(0);
					const UVec4 range		(UVec4(1u) << tcu::min(bits, UVec4(31)));
					std::ostringstream		fragmentShader;

					fragmentShader <<
						"#version 450\n"
						"layout(location = 0) out highp uvec4 o_color;\n"
						"layout(push_constant) uniform PushConstant {\n"
						"\thighp uint sampleIndex;\n"
						"} pushConstants;\n"
						"void main (void)\n"
						"{\n"
						"\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
						"\tgl_SampleMask[0] = int(0x1u << sampleIndex);\n"
						"\thighp uint color[4];\n"
						"\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
						"\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

					for (int ndx = 0; ndx < 4; ndx++)
						fragmentShader << "\tcolor[" << ndx << "] = "  << minValue[ndx] << ";\n";

					{
						const int		componentCount	= tcu::getNumUsedChannels(format.order);
						const deUint32	bitSize			(bits[0] + bits[1] + bits[2] + bits[3]);
						deUint32		dstBitsUsed[4]	= { 0u, 0u, 0u, 0u };
						deUint32		nextSrcBit		= 0;
						deUint32		divider			= 2;

						// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
						while (nextSrcBit < de::min(bitSize, 10u))
						{
							for (int compNdx = 0; compNdx < componentCount; compNdx++)
							{
								if (dstBitsUsed[compNdx] > bits[compNdx])
									continue;

								fragmentShader <<
										"\tcolor[" << compNdx << "] += " << (range[compNdx] / divider)
										<< " * bitfieldExtract(" << (nextSrcBit % 2 == 0 ? "x" : "y") << ", " << (nextSrcBit / 2) << ", 1);\n";

								nextSrcBit++;
								dstBitsUsed[compNdx]++;
							}

							divider *= 2;
						}
					}

					fragmentShader <<
						"\to_color = uvec4(color[0], color[1], color[2], color[3]);\n"
						"}\n";

					dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
					break;
				}

				case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
				{
					const UVec4	bits		(tcu::getTextureFormatBitDepth(format).cast<deUint32>());
					const IVec4 minValue	(0);
					const IVec4 range		((UVec4(1u) << tcu::min(bits, UVec4(30))).cast<deInt32>());
					const IVec4 maxV		((UVec4(1u) << (bits - UVec4(1u))).cast<deInt32>());
					const IVec4 clampMax	(maxV - 1);
					const IVec4 clampMin	(-maxV);
					std::ostringstream		fragmentShader;

					fragmentShader <<
						"#version 450\n"
						"layout(location = 0) out highp ivec4 o_color;\n"
						"layout(push_constant) uniform PushConstant {\n"
						"\thighp uint sampleIndex;\n"
						"} pushConstants;\n"
						"void main (void)\n"
						"{\n"
						"\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
						"\tgl_SampleMask[0] = int(0x1u << sampleIndex);\n"
						"\thighp int color[4];\n"
						"\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
						"\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

					for (int ndx = 0; ndx < 4; ndx++)
						fragmentShader << "\tcolor[" << ndx << "] = "  << minValue[ndx] << ";\n";

					{
						const int		componentCount	= tcu::getNumUsedChannels(format.order);
						const deUint32	bitSize			(bits[0] + bits[1] + bits[2] + bits[3]);
						deUint32		dstBitsUsed[4]	= { 0u, 0u, 0u, 0u };
						deUint32		nextSrcBit		= 0;
						deUint32		divider			= 2;

						// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
						while (nextSrcBit < de::min(bitSize, 10u))
						{
							for (int compNdx = 0; compNdx < componentCount; compNdx++)
							{
								if (dstBitsUsed[compNdx] > bits[compNdx])
									continue;

								fragmentShader <<
										"\tcolor[" << compNdx << "] += " << (range[compNdx] / divider)
										<< " * int(bitfieldExtract(" << (nextSrcBit % 2 == 0 ? "x" : "y") << ", " << (nextSrcBit / 2) << ", 1));\n";

								nextSrcBit++;
								dstBitsUsed[compNdx]++;
							}

							divider *= 2;
						}
					}

					// The spec doesn't define whether signed-integers are clamped on output,
					// so we'll clamp them explicitly to have well-defined outputs.
					fragmentShader <<
						"\to_color = clamp(ivec4(color[0], color[1], color[2], color[3]), " <<
						"ivec4" << clampMin << ", ivec4" << clampMax << ");\n" <<
						"}\n";

					dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
					break;
				}

				case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
				case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
				case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
				{
					const tcu::TextureFormatInfo	info			(tcu::getTextureFormatInfo(format));
					const UVec4						bits			(tcu::getTextureFormatMantissaBitDepth(format).cast<deUint32>());
					const Vec4						minLimit		(-65536.0);
					const Vec4						maxLimit		(65536.0);
					const Vec4						minValue		(tcu::max(info.valueMin, minLimit));
					const Vec4						range			(tcu::min(info.valueMax, maxLimit) - minValue);
					std::ostringstream				fragmentShader;

					fragmentShader <<
						"#version 450\n"
						"layout(location = 0) out highp vec4 o_color;\n"
						"layout(push_constant) uniform PushConstant {\n"
						"\thighp uint sampleIndex;\n"
						"} pushConstants;\n"
						"void main (void)\n"
						"{\n"
						"\thighp uint sampleIndex = pushConstants.sampleIndex;\n"
						"\tgl_SampleMask[0] = int(0x1u << sampleIndex);\n"
						"\thighp float color[4];\n"
						"\thighp uint x = sampleIndex ^ uint(gl_FragCoord.x);\n"
						"\thighp uint y = sampleIndex ^ uint(gl_FragCoord.y);\n";

					for (int ndx = 0; ndx < 4; ndx++)
						fragmentShader << "\tcolor[" << ndx << "] = "  << minValue[ndx] << ";\n";

					{
						const bool		isAlphaOnly		= isAlphaOnlyFormat(config.format);
						const int		componentCount	= (isAlphaOnly ? 4 : tcu::getNumUsedChannels(format.order));
						const deUint32	bitSize			(bits[0] + bits[1] + bits[2] + bits[3]);
						deUint32		dstBitsUsed[4]	= { 0u, 0u, 0u, 0u };
						deUint32		nextSrcBit		= 0;
						deUint32		divider			= 2;

						// \note Limited to ten bits since the target is 32x32, so there are 10 input bits
						while (nextSrcBit < de::min(bitSize, 10u))
						{
							for (int compNdx = 0; compNdx < componentCount; compNdx++)
							{
								if (dstBitsUsed[compNdx] > bits[compNdx])
									continue;

								fragmentShader <<
										"\tcolor[" << compNdx << "] += " << (range[compNdx] / (float)divider)
										<< " * float(bitfieldExtract(" << (nextSrcBit % 2 == 0 ? "x" : "y") << ", " << (nextSrcBit / 2) << ", 1));\n";

								nextSrcBit++;
								dstBitsUsed[compNdx]++;
							}

							divider *= 2;
						}
					}

					fragmentShader <<
						"\to_color = vec4(color[0], color[1], color[2], color[3]);\n"
						"}\n";

					dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
					break;
				}

				default:
					DE_FATAL("Unknown channel class");
			}
		}

		if (tcu::hasDepthComponent(format.order) || tcu::hasStencilComponent(format.order))
		{
			std::ostringstream splitShader;

			splitShader <<
				"#version 450\n";

			if (testDepth && testStencil)
			{
				splitShader << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInputMS i_depth;\n"
							<< "layout(input_attachment_index = 0, set = 0, binding = 1) uniform highp usubpassInputMS i_stencil;\n";
			}
			else if (testDepth)
				splitShader << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInputMS i_depth;\n";
			else if (testStencil)
				splitShader << "layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp usubpassInputMS i_stencil;\n";

			splitShader <<
				"layout(push_constant) uniform PushConstant {\n"
				"\thighp uint splitSubpassIndex;\n"
				"} pushConstants;\n";

			for (deUint32 attachmentNdx = 0; attachmentNdx < de::min((deUint32)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
			{
				if (testDepth && testStencil)
					splitShader << "layout(location = " << attachmentNdx << ") out highp vec2 o_color" << attachmentNdx << ";\n";
				else
					splitShader << "layout(location = " << attachmentNdx << ") out highp float o_color" << attachmentNdx << ";\n";
			}

			splitShader <<
				"void main (void)\n"
				"{\n";

			for (deUint32 attachmentNdx = 0; attachmentNdx < de::min((deUint32)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
			{
				if (testDepth)
					splitShader << "\thighp float depth" << attachmentNdx << " = subpassLoad(i_depth, int(" << MAX_COLOR_ATTACHMENT_COUNT << " * pushConstants.splitSubpassIndex + " << attachmentNdx << "u)).x;\n";

				if (testStencil)
					splitShader << "\thighp uint stencil" << attachmentNdx << " = subpassLoad(i_stencil, int(" << MAX_COLOR_ATTACHMENT_COUNT << " * pushConstants.splitSubpassIndex + " << attachmentNdx << "u)).x;\n";

				if (testDepth && testStencil)
					splitShader << "\to_color" << attachmentNdx << " = vec2(depth" << attachmentNdx << ", float(stencil" << attachmentNdx << "));\n";
				else if (testDepth)
					splitShader << "\to_color" << attachmentNdx << " = float(depth" << attachmentNdx << ");\n";
				else if (testStencil)
					splitShader << "\to_color" << attachmentNdx << " = float(stencil" << attachmentNdx << ");\n";
			}

			splitShader <<
				"}\n";

			dst.glslSources.add("quad-split-frag") << glu::FragmentSource(splitShader.str());
		}
		else
		{
			std::string subpassType;
			std::string outputType;

			switch (channelClass)
			{
				case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
					subpassType	= "usubpassInputMS";
					outputType	= "uvec4";
					break;

				case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
					subpassType	= "isubpassInputMS";
					outputType	= "ivec4";
					break;

				case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
				case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
				case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
					subpassType	= "subpassInputMS";
					outputType	= "vec4";
					break;

				default:
					DE_FATAL("Unknown channel class");
			}

			std::ostringstream splitShader;
			splitShader <<
				"#version 450\n"
				"layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp " << subpassType << " i_color;\n"
				"layout(push_constant) uniform PushConstant {\n"
				"\thighp uint splitSubpassIndex;\n"
				"} pushConstants;\n";

			for (deUint32 attachmentNdx = 0; attachmentNdx < de::min((deUint32)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
				splitShader << "layout(location = " << attachmentNdx << ") out highp " << outputType << " o_color" << attachmentNdx << ";\n";

			splitShader <<
				"void main (void)\n"
				"{\n";

			for (deUint32 attachmentNdx = 0; attachmentNdx < de::min((deUint32)MAX_COLOR_ATTACHMENT_COUNT, config.sampleCount); attachmentNdx++)
				splitShader << "\to_color" << attachmentNdx << " = subpassLoad(i_color, int(" << MAX_COLOR_ATTACHMENT_COUNT << " * pushConstants.splitSubpassIndex + " << attachmentNdx << "u));\n";

			splitShader <<
				"}\n";

			dst.glslSources.add("quad-split-frag") << glu::FragmentSource(splitShader.str());
		}
	}
};

std::string formatToName (VkFormat format)
{
	const std::string	formatStr	= de::toString(format);
	const std::string	prefix		= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

void initTests (tcu::TestCaseGroup* group, RenderingType renderingType)
{
	static const VkFormat	formats[]	=
	{
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,

		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};
	const deUint32			sampleCounts[] =
	{
		2u, 4u, 8u, 16u, 32u
	};
	tcu::TestContext&				testCtx		(group->getTestContext());
	de::MovePtr<tcu::TestCaseGroup>	extGroup	(new tcu::TestCaseGroup(testCtx, "separate_stencil_usage", "test VK_EXT_separate_stencil_usage"));

	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
	{
		const VkFormat					format			(formats[formatNdx]);
		const std::string				formatName		(formatToName(format));
		de::MovePtr<tcu::TestCaseGroup>	formatGroup		(new tcu::TestCaseGroup(testCtx, formatName.c_str(), formatName.c_str()));
		de::MovePtr<tcu::TestCaseGroup>	extFormatGroup	(new tcu::TestCaseGroup(testCtx, formatName.c_str(), formatName.c_str()));

		for (size_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountNdx++)
		{
			const deUint32		sampleCount	(sampleCounts[sampleCountNdx]);
			const TestConfig	testConfig	(format, sampleCount, renderingType);
			const std::string	testName	("samples_" + de::toString(sampleCount));

			formatGroup->addChild(new InstanceFactory1<MultisampleRenderPassTestInstance, TestConfig, Programs>(testCtx, testName.c_str(), testName.c_str(), testConfig));

			// create tests for VK_EXT_separate_stencil_usage
			if (tcu::hasDepthComponent(mapVkFormat(format).order) && tcu::hasStencilComponent(mapVkFormat(format).order))
			{
				de::MovePtr<tcu::TestCaseGroup>	sampleGroup	(new tcu::TestCaseGroup(testCtx, testName.c_str(), testName.c_str()));
				{
					const TestConfig	separateUsageDepthTestConfig	(format, sampleCount, renderingType, TEST_DEPTH);
					sampleGroup->addChild(new InstanceFactory1<MultisampleRenderPassTestInstance, TestConfig, Programs>(testCtx, "test_depth", "depth with input attachment bit", separateUsageDepthTestConfig));

					const TestConfig	separateUsageStencilTestConfig	(format, sampleCount, renderingType, TEST_STENCIL);
					sampleGroup->addChild(new InstanceFactory1<MultisampleRenderPassTestInstance, TestConfig, Programs>(testCtx, "test_stencil", "stencil with input attachment bit", separateUsageStencilTestConfig));
				}

				extFormatGroup->addChild(sampleGroup.release());
			}
		}

		group->addChild(formatGroup.release());
		extGroup->addChild(extFormatGroup.release());
	}

	group->addChild(extGroup.release());
}

} // anonymous

tcu::TestCaseGroup* createRenderPassMultisampleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "multisample", "Multisample render pass tests", initTests, RENDERING_TYPE_RENDERPASS_LEGACY);
}

tcu::TestCaseGroup* createRenderPass2MultisampleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "multisample", "Multisample render pass tests", initTests, RENDERING_TYPE_RENDERPASS2);
}

} // vkt
