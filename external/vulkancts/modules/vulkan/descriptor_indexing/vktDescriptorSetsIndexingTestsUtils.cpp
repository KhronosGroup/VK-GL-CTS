/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2019 The Khronos Group Inc.
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
* \brief Vulkan Decriptor Indexing Tests
*//*--------------------------------------------------------------------*/

#include <algorithm>
#include <iostream>
#include <iterator>
#include <functional>
#include <sstream>
#include <utility>
#include <vector>

#include "vktDescriptorSetsIndexingTests.hpp"

#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deStringUtil.hpp"

namespace vkt
{
namespace DescriptorIndexing
{
using namespace vk;
namespace ut
{

ImageHandleAlloc::ImageHandleAlloc	(Move<VkImage>&					image_,
									 AllocMv&						alloc_,
									 const VkExtent3D&				extent_,
									 VkFormat						format_,
									 bool							usesMipMaps_)
	: image		(image_)
	, alloc		(alloc_)
	, extent	(extent_)
	, format	(format_)
	, levels	(usesMipMaps_ ? computeMipMapCount(extent_) : 1)
{
}

std::string buildShaderName			(VkShaderStageFlagBits			stage,
									 VkDescriptorType				descriptorType,
									 deBool							updateAfterBind,
									 bool							calculateInLoop,
									 bool							minNonUniform,
									 bool							performWritesInVertex)
{
	const char* stageName = DE_NULL;
	switch (stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:					stageName = "vert"; break;
	case VK_SHADER_STAGE_FRAGMENT_BIT:					stageName = "frag"; break;
	case VK_SHADER_STAGE_COMPUTE_BIT:					stageName = "comp"; break;
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		stageName = "tesc"; break;
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	stageName = "tese";	break;
	case VK_SHADER_STAGE_GEOMETRY_BIT:					stageName = "geom"; break;
	default:											stageName = "any";	break;
	}
	DE_ASSERT(stageName);

	std::map<std::string, std::string> m;
	m["STAGE"]	= stageName;
	m["DESC"]	= de::toString(deUint32(descriptorType));
	m["ABIND"]	= updateAfterBind		? "_afterBind"		: "";
	m["LOOP"]	= calculateInLoop		? "_inLoop"			: "";
	m["MINNU"]	= minNonUniform			? "_minNonUniform"	: "";
	m["SHWR"]	= performWritesInVertex	? "_shaderWrites"	: "";

	return tcu::StringTemplate("descriptorIndexing_${STAGE}${DESC}${ABIND}${LOOP}${MINNU}${SHWR}").specialize(m);
}

std::vector<deUint32> generatePrimes (deUint32						limit)
{
	deUint32 i, j, *data;
	std::vector<deUint32> v(limit);

	data = v.data();

	for (i = 0; i < limit; ++i)
		data[i] = i;

	for (i = 2; i < limit; ++i)
	{
		if (data[i])
		{
			for (j = i*2; j < limit; j += i)
				data[j] = 0;
		}
	}

	std::vector<deUint32>::iterator x = std::stable_partition(v.begin(), v.end(), [](deUint32 value) { return value >= 2; });

	return std::vector<deUint32>(v.begin(), x);
}

deUint32 computePrimeCount			(deUint32						limit)
{
	deUint32 i, j, k, *data;
	std::vector<deUint32> v(limit);

	data = v.data();

	for (i = 0; i < limit; ++i)
		data[i] = i;

	k = 0;
	for (i = 2; i < limit; ++i)
	{
		if (data[i])
		{
			++k;
			for (j = i*2; j < limit; j += i)
				data[j] = 0;
		}
	}
	return k;
}

deUint32 computeMipMapCount			(const VkExtent3D&				extent)
{
	return deUint32(floor(log2(std::max(extent.width, extent.height)))) + 1;
}

deUint32 computeImageSize			(const VkExtent3D&				extent,
									 VkFormat						format,
									 bool							withMipMaps,
									 deUint32						level)
{
	deUint32 mipSize = extent.width * extent.height * extent.depth * vk::mapVkFormat(format).getPixelSize();
	if (withMipMaps)
	{
		deUint32		mipIdx		= 0u;
		deUint32		width		= extent.width;
		deUint32		height		= extent.height;
		const deUint32	mipCount	= computeMipMapCount(extent) - 1;
		do
		{
			width /= 2;
			height /= 2;
			deUint32 tmpSize = width * height * extent.depth * vk::mapVkFormat(format).getPixelSize();

			if (level == mipIdx)
			{
				break;
			}
			else if (level == maxDeUint32)
			{
				mipSize += tmpSize;
			}
			else
			{
				mipSize = tmpSize;
			}

		} while (++mipIdx < mipCount);
	}
	return mipSize;
}

deUint32 computeImageSize			(const ImageHandleAllocSp&		image)
{
	return computeImageSize(image->extent, image->format);
}

void createImageAndBind				(ut::ImageHandleAllocSp&		output,
									 const vkt::Context&			ctx,
									 VkFormat						colorFormat,
									 const VkExtent3D&				extent,
									 VkImageLayout					initialLayout,
									 bool							withMipMaps,
									 VkImageType					imageType)
{
	const bool						isDepthStencilFormat = vk::isDepthStencilFormat(colorFormat);

	const VkImageUsageFlags			imageUsageFlagsDependent = isDepthStencilFormat
		? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
		: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const VkImageUsageFlags			imageUsageFlags = imageUsageFlagsDependent
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	const deUint32 mipLevels = withMipMaps ? computeMipMapCount(extent) : 1;
	const VkImageCreateInfo			createInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		(VkImageCreateFlags)0,					// flags
		imageType,								// imageType
		colorFormat,							// format
		extent,									// extent
		mipLevels,								// mipLevels
		(deUint32)1,							// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// samples
		VK_IMAGE_TILING_OPTIMAL,				// tiling
		imageUsageFlags,						// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		(deUint32)0,							// queueFamilyCount
		DE_NULL,								// pQueueFamilyIndices
		initialLayout							// initialLayout
	};

	Allocator&						allocator	= ctx.getDefaultAllocator();
	VkDevice						device		= ctx.getDevice();
	const DeviceInterface&			dinterface	= ctx.getDeviceInterface();

	Move<VkImage>					image		= vk::createImage(dinterface, device, &createInfo);

	const VkMemoryRequirements		memReqs		= vk::getImageMemoryRequirements(dinterface, device, *image);
	de::MovePtr<Allocation>			allocation	= allocator.allocate(memReqs, MemoryRequirement::Any);

	VK_CHECK(dinterface.bindImageMemory(device, *image, allocation->getMemory(), allocation->getOffset()));

	output = ImageHandleAllocSp(new ImageHandleAlloc(image, allocation, extent, colorFormat, withMipMaps));
}

void recordCopyBufferToImage		(VkCommandBuffer				cmd,
									 const DeviceInterface&			interface,
									 VkPipelineStageFlagBits		srcStageMask,
									 VkPipelineStageFlagBits		dstStageMask,
									 const VkDescriptorBufferInfo&	bufferInfo,
									 VkImage						image,
									 const VkExtent3D&				imageExtent,
									 VkFormat						imageFormat,
									 VkImageLayout					oldImageLayout,
									 VkImageLayout					newImageLayout,
									 deUint32						mipLevelCount)
{
	const VkImageAspectFlags imageAspect = vk::isDepthStencilFormat(imageFormat)
		? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
		: VK_IMAGE_ASPECT_COLOR_BIT;

	std::vector<VkBufferImageCopy>	copyRegions;
	{
		deUint32		width			= imageExtent.width;
		deUint32		height			= imageExtent.height;
		VkDeviceSize	bufferOffset	= bufferInfo.offset;

		for (deUint32 mipIdx = 0; mipIdx < mipLevelCount; ++mipIdx)
		{
			VkDeviceSize	imageSize = computeImageSize(imageExtent, imageFormat, true, mipIdx);

			const VkBufferImageCopy		copyRegion =
			{
				bufferOffset,							// bufferOffset
				width,									// bufferRowLength
				height,									// bufferImageHeight
				{
					imageAspect,						// aspect
					mipIdx,								// mipLevel
					0u,									// baseArrayLayer
					1u,									// layerCount
				},										// VkImageSubresourceLayers imageSubresource
				{ 0,0,0 },								// VkOffset3D				imageOffset
				{ width, height, 1 }					// VkExtent3D				imageExtent
			};

			copyRegions.push_back(copyRegion);

			bufferOffset	+= imageSize;
			width			/= 2;
			height			/= 2;
		}
	}

	const VkImageSubresourceRange		subresourceRange =
	{
		imageAspect,									// aspectMask
		0u,												// baseMipLevel
		mipLevelCount,									// levelCount
		0u,												// baseArrayLayer
		1u,												// layerCount
	};

	const VkImageMemoryBarrier	barrierBefore =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType;
		DE_NULL,										// pNext;
		0,												// srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// dstAccessMask;
		oldImageLayout,									// oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// dstQueueFamilyIndex;
		image,											// image
		subresourceRange								// subresourceRange
	};

	const VkBufferMemoryBarrier	bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// sType;
		DE_NULL,										// pNext;
		pipelineAccessFromStage(srcStageMask, false),	// srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,					// dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// dstQueueFamilyIndex;
		bufferInfo.buffer,								// buffer;
		bufferInfo.offset,								// offset;
		bufferInfo.range								// size;
	};

	const VkImageMemoryBarrier	barrierAfter =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType;
		DE_NULL,										// pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// srcAccessMask;
		pipelineAccessFromStage(dstStageMask, true)
		| pipelineAccessFromStage(dstStageMask, false),	// dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// oldLayout;
		newImageLayout,									// newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// dstQueueFamilyIndex;
		image,											// image
		subresourceRange								// subresourceRange
	};

	interface.cmdPipelineBarrier(cmd,
		srcStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT,	// srcStageMask, dstStageMask
		(VkDependencyFlags)0,							// dependencyFlags
		0u, DE_NULL,									// memoryBarrierCount, pMemoryBarriers
		1u, &bufferBarrier,								// bufferBarrierCount, pBufferBarriers
		1u, &barrierBefore);							// imageBarrierCount, pImageBarriers

	interface.cmdCopyBufferToImage(cmd, bufferInfo.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(copyRegions.size()), copyRegions.data());

	interface.cmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask,	// srcStageMask, dstStageMask
		(VkDependencyFlags)0,							// dependencyFlags
		0u, DE_NULL,									// memoryBarrierCount, pMemoryBarriers
		0u, DE_NULL,									// bufferBarrierCount, pBufferBarriers
		1u, &barrierAfter);								// imageBarrierCount, pImageBarriers
}

void recordCopyImageToBuffer		(VkCommandBuffer				cmd,
									 const DeviceInterface&			interface,
									 VkPipelineStageFlagBits		srcStageMask,
									 VkPipelineStageFlagBits		dstStageMask,
									 VkImage						image,
									 const VkExtent3D&				imageExtent,
									 VkFormat						imageFormat,
									 VkImageLayout					oldImageLayout,
									 VkImageLayout					newImageLayout,
									 const VkDescriptorBufferInfo&	bufferInfo)
{
	const VkImageAspectFlags imageAspect = vk::isDepthStencilFormat(imageFormat)
		? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
		: VK_IMAGE_ASPECT_COLOR_BIT;

	const VkBufferImageCopy		copyRegion =
	{
		bufferInfo.offset,								// bufferOffset
		imageExtent.width,								// bufferRowLength
		imageExtent.height,								// bufferImageHeight
		{
			imageAspect,								// aspect
			0u,											// mipLevel
			0u,											// baseArrayLayer
			1u,											// layerCount
		},												// VkImageSubresourceLayers
		{ 0, 0, 0 },									// imageOffset
		imageExtent										// imageExtent
	};

	VkImageSubresourceRange		subresourceRange =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,						// aspectMask
		0u,												// baseMipLevel
		1u,												// levelCount
		0u,												// baseArrayLayer
		1u,												// layerCount
	};

	const VkImageMemoryBarrier	barrierBefore =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType;
		DE_NULL,										// pNext;
		pipelineAccessFromStage(srcStageMask, false),	// srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,					// dstAccessMask;
		oldImageLayout,									// oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// dstQueueFamilyIndex;
		image,											// image;
		subresourceRange,								// subresourceRange;
	};

	const VkImageMemoryBarrier	barrierAfter =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// sType;
		DE_NULL,										// pNext;
		VK_ACCESS_TRANSFER_READ_BIT,					// srcAccessMask;
		pipelineAccessFromStage(dstStageMask, true)
		| pipelineAccessFromStage(dstStageMask, false),	// dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// oldLayout;
		newImageLayout,									// newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// dstQueueFamilyIndex;
		image,											// image
		subresourceRange								// subresourceRange
	};

	interface.cmdPipelineBarrier(cmd,					// commandBuffer
		srcStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT,	// srcStageMask, dstStageMask
		(VkDependencyFlags)0,							// dependencyFlags
		0u, DE_NULL,									// memoryBarrierCount, pMemoryBarriers
		0u, DE_NULL,									// bufferBarrierCount, pBufferBarriers
		1u, &barrierBefore);							// imageBarrierCount, pImageBarriers

	interface.cmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bufferInfo.buffer, 1u, &copyRegion);

	interface.cmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT, dstStageMask,
		(VkDependencyFlags)0,
		0u, DE_NULL,
		0u, DE_NULL,
		0u, &barrierAfter);
}

VkAccessFlags pipelineAccessFromStage (VkPipelineStageFlagBits stage, bool readORwrite)
{
	VkAccessFlags access[2];
	VkAccessFlags& readAccess = access[1];
	VkAccessFlags& writeAccess = access[0];
	readAccess = writeAccess = static_cast<VkAccessFlagBits>(0);

	switch (stage)
	{
	case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
	case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
		readAccess = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		break;

	case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
		readAccess = static_cast<VkAccessFlagBits>(VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
		break;

	case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
	case VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT:
	case VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT:
	case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
	case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
		readAccess = VK_ACCESS_SHADER_READ_BIT;
		writeAccess = VK_ACCESS_SHADER_WRITE_BIT;
		break;

	case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
		readAccess = static_cast<VkAccessFlagBits>(VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
		writeAccess = VK_ACCESS_SHADER_READ_BIT;
		break;

	case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
		readAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		writeAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
	case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
		readAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		writeAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_PIPELINE_STAGE_TRANSFER_BIT:
		readAccess = VK_ACCESS_TRANSFER_READ_BIT;
		writeAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_PIPELINE_STAGE_HOST_BIT:
		readAccess = VK_ACCESS_HOST_READ_BIT;
		writeAccess = VK_ACCESS_HOST_WRITE_BIT;
		break;

	default:
		if (stage == 0)
		{
			readAccess = VK_ACCESS_MEMORY_READ_BIT;
			writeAccess = VK_ACCESS_MEMORY_WRITE_BIT;
			break;
		}

		DE_ASSERT(DE_FALSE);
	}
	return access[readORwrite ? 1 : 0];
}

void createFrameBuffer				(FrameBufferSp&					outputFB,
									 const vkt::Context&			context,
									 const VkExtent3D&				extent,
									 VkFormat						colorFormat,
									 VkRenderPass					renderpass,
									 deUint32						additionalAttachmentCount,
									 const VkImageView				additionalAttachments[])
{
	outputFB						= FrameBufferSp(new ut::FrameBuffer);
	VkDevice						device = context.getDevice();
	const DeviceInterface&			interface = context.getDeviceInterface();
	createImageAndBind(outputFB->image, context, colorFormat, extent, VK_IMAGE_LAYOUT_UNDEFINED);

	// create and attachment0
	{
		const VkImageViewCreateInfo viewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			(VkImageViewCreateFlags)0,					// flags
			*outputFB->image->image,						// image
			VK_IMAGE_VIEW_TYPE_2D,						// viewType
			colorFormat,								// format
			vk::makeComponentMappingRGBA(),				// components
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// aspectMask
				(deUint32)0,				// baseMipLevel
				(deUint32)1,				// mipLevels
				(deUint32)0,				// baseArrayLayer
				(deUint32)1u,				// arraySize
			},
		};

		outputFB->attachment0 = vk::createImageView(interface, device, &viewCreateInfo);

		std::vector<VkImageView>& attachments(outputFB->attachments);
		attachments.push_back(*outputFB->attachment0);
		if (additionalAttachments && additionalAttachmentCount)
		{
			attachments.insert(attachments.end(), additionalAttachments, additionalAttachments + additionalAttachmentCount);
		}
	}

	// create a frame buffer
	{
		std::vector<VkImageView>& attachments(outputFB->attachments);

		const VkFramebufferCreateInfo	framebufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			(VkFramebufferCreateFlags)0,				// flags
			renderpass,									// renderPass
			static_cast<deUint32>(attachments.size()),	// attachmentCount
			attachments.data(),							// pAttachments
			extent.width,								// width
			extent.height,								// height
			(deUint32)1									// layers
		};

		outputFB->buffer = vk::createFramebuffer(interface, device, &framebufferCreateInfo);
	}
}

VkDeviceSize createBufferAndBind	(ut::BufferHandleAllocSp&	output,
									 const vkt::Context&		ctx,
									 VkBufferUsageFlags			usage,
									 VkDeviceSize				desiredSize)
{
	const size_t				nonCoherentAtomSize	(static_cast<size_t>(ctx.getDeviceProperties().limits.nonCoherentAtomSize));
	const VkDeviceSize			roundedSize			(deAlignSize(static_cast<size_t>(desiredSize), nonCoherentAtomSize));
	Allocator&					allocator			(ctx.getDefaultAllocator());
	VkDevice					device				(ctx.getDevice());
	const DeviceInterface&		interface			(ctx.getDeviceInterface());

	const VkBufferCreateInfo	createInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// sType
		DE_NULL,									// pNext
		(VkBufferCreateFlags)0,						// flags
		roundedSize,								// size
		usage,										// usage
		VK_SHARING_MODE_EXCLUSIVE,					// sharingMode
		0u,											// queueFamilyIndexCount
		DE_NULL,									// pQueueFamilyIndices
	};

	Move<VkBuffer>				buffer				= vk::createBuffer(interface, device, &createInfo);

	const VkMemoryRequirements	memRequirements		= vk::getBufferMemoryRequirements(interface, device, *buffer);
	de::MovePtr<Allocation>		allocation			= allocator.allocate(memRequirements, MemoryRequirement::HostVisible);

	VK_CHECK(interface.bindBufferMemory(device, *buffer, allocation->getMemory(), allocation->getOffset()));

	output = BufferHandleAllocSp(new BufferHandleAlloc(buffer, allocation));

	return roundedSize;
}

std::vector<tcu::Vec4> createVertices (deUint32 width, deUint32 height, float& xSize, float& ySize)
{
	std::vector<tcu::Vec4> result;

	const float		xStep = 2.0f / static_cast<float>(width);
	const float		yStep = 2.0f / static_cast<float>(height);
	const float		xStart = -1.0f + xStep / 2.0f;
	const float		yStart = -1.0f + yStep / 2.0f;

	xSize = xStep;
	ySize = yStep;

	float x = xStart;
	float y = yStart;

	result.reserve(width * height);

	for (deUint32 row = 0u; row < height; ++row)
	{
		for (deUint32 col = 0u; col < width; ++col)
		{
			result.push_back(tcu::Vec4(x, y, 1.0f, 1.0f));
			x += xStep;
		}

		y += yStep;
		x = xStart;
	}

	return result;
}

bool isDynamicDescriptor (VkDescriptorType descriptorType)
{
	return descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
}

DeviceProperties::DeviceProperties (const DeviceProperties& src)
{
	m_descriptorIndexingFeatures = src.m_descriptorIndexingFeatures;
	m_features2 = src.m_features2;

	m_descriptorIndexingProperties = src.m_descriptorIndexingProperties;
	m_properties2 = src.m_properties2;
}

DeviceProperties::DeviceProperties (const vkt::Context& testContext)
{
	VkPhysicalDevice device = testContext.getPhysicalDevice();
	const InstanceInterface& interface = testContext.getInstanceInterface();

	deMemset(&m_descriptorIndexingFeatures, 0, sizeof(m_descriptorIndexingFeatures));
	m_descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	m_descriptorIndexingFeatures.pNext = DE_NULL;

	deMemset(&m_features2, 0, sizeof(m_features2));
	m_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	m_features2.pNext = &m_descriptorIndexingFeatures;

	interface.getPhysicalDeviceFeatures2(device, &m_features2);

	deMemset(&m_descriptorIndexingProperties, 0, sizeof(m_descriptorIndexingProperties));
	m_descriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
	m_descriptorIndexingProperties.pNext = DE_NULL;

	deMemset(&m_properties2, 0, sizeof(m_properties2));
	m_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	m_properties2.pNext = &m_descriptorIndexingProperties;

	interface.getPhysicalDeviceProperties2(device, &m_properties2);
}

deUint32 DeviceProperties::computeMaxPerStageDescriptorCount	(VkDescriptorType	descriptorType,
																 bool				enableUpdateAfterBind,
																 bool				reserveUniformTexelBuffer) const
{
	const VkPhysicalDeviceDescriptorIndexingProperties&		descriptorProps = descriptorIndexingProperties();
	const VkPhysicalDeviceProperties&						deviceProps = physicalDeviceProperties();

	deUint32		result					= 0;
	deUint32		samplers				= 0;
	deUint32		uniformBuffers			= 0;
	deUint32		uniformBuffersDynamic	= 0;
	deUint32		storageBuffers			= 0;
	deUint32		storageBuffersDynamic	= 0;
	deUint32		sampledImages			= 0;
	deUint32		storageImages			= 0;
	deUint32		inputAttachments		= 0;
#ifndef CTS_USES_VULKANSC
	deUint32		inlineUniforms			= 0;
#endif

	// in_loop tests use an additional single texel buffer, which is calculated against the limits below
	const deUint32	reservedCount			= (reserveUniformTexelBuffer ? 1u : 0u);

	const deUint32	resources				= deviceProps.limits.maxPerStageResources - reservedCount;

	if (enableUpdateAfterBind)
	{
		samplers				= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindSamplers,			descriptorProps.maxDescriptorSetUpdateAfterBindSamplers);				// 1048576
		uniformBuffers			= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindUniformBuffers,		descriptorProps.maxDescriptorSetUpdateAfterBindUniformBuffers);			// 15
		uniformBuffersDynamic	= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindUniformBuffers,		descriptorProps.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic);	// 8
		storageBuffers			= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindStorageBuffers,		descriptorProps.maxDescriptorSetUpdateAfterBindStorageBuffers);			// 1048576
		storageBuffersDynamic	= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindStorageBuffers,		descriptorProps.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic);	// 8
		sampledImages			= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindSampledImages,		descriptorProps.maxDescriptorSetUpdateAfterBindSampledImages);			// 1048576
		storageImages			= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindStorageImages,		descriptorProps.maxDescriptorSetUpdateAfterBindStorageImages);			// 1048576
		inputAttachments		= deMinu32(	descriptorProps.maxPerStageDescriptorUpdateAfterBindInputAttachments,	descriptorProps.maxDescriptorSetUpdateAfterBindInputAttachments);		// 1048576
	}
	else
	{
		samplers				= deMinu32(	deviceProps.limits.maxPerStageDescriptorSamplers,						deviceProps.limits.maxDescriptorSetSamplers);							// 1048576
		uniformBuffers			= deMinu32(	deviceProps.limits.maxPerStageDescriptorUniformBuffers,					deviceProps.limits.maxDescriptorSetUniformBuffers);						// 15
		uniformBuffersDynamic	= deMinu32(	deviceProps.limits.maxPerStageDescriptorUniformBuffers,					deviceProps.limits.maxDescriptorSetUniformBuffersDynamic);				// 8
		storageBuffers			= deMinu32(	deviceProps.limits.maxPerStageDescriptorStorageBuffers,					deviceProps.limits.maxDescriptorSetStorageBuffers);						// 1048576
		storageBuffersDynamic	= deMinu32(	deviceProps.limits.maxPerStageDescriptorStorageBuffers,					deviceProps.limits.maxDescriptorSetStorageBuffersDynamic);				// 8
		sampledImages			= deMinu32(	deviceProps.limits.maxPerStageDescriptorSampledImages - reservedCount,	deviceProps.limits.maxDescriptorSetSampledImages - reservedCount);		// 1048576.
		storageImages			= deMinu32(	deviceProps.limits.maxPerStageDescriptorStorageImages,					deviceProps.limits.maxDescriptorSetStorageImages);						// 1048576
		inputAttachments		= deMinu32(	deviceProps.limits.maxPerStageDescriptorInputAttachments - 1,			deviceProps.limits.maxDescriptorSetInputAttachments - 1);				// 1048576. -1 because tests use a prime number + 1 to reference subpass input attachment in shader
	}

	// adding arbitrary upper bound limits to restrain the size of the test ( we are testing big arrays, not the maximum size arrays )
	samplers					= deMinu32(	samplers,				4096);
	uniformBuffers				= deMinu32(	uniformBuffers,			16);
	uniformBuffersDynamic		= deMinu32( uniformBuffersDynamic,	16);
	storageBuffers				= deMinu32(	storageBuffers,			8192);
	storageBuffersDynamic		= deMinu32(	storageBuffersDynamic,	8192);
	sampledImages				= deMinu32(	sampledImages,			8192);
	storageImages				= deMinu32(	storageImages,			8192);
	inputAttachments			= deMinu32(	inputAttachments,		16);

	switch (descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:					result = samplers;													break;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:		result = deMinu32(resources, deMinu32(samplers, sampledImages));	break;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:				result = deMinu32(resources, sampledImages);						break;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:				result = deMinu32(resources, storageImages);						break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:		result = deMinu32(resources, sampledImages);						break;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:		result = deMinu32(resources, storageImages);						break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:				result = deMinu32(resources, uniformBuffers);						break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:				result = deMinu32(resources, storageBuffers);						break;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:		result = deMinu32(resources, uniformBuffersDynamic);				break;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:		result = deMinu32(resources, storageBuffersDynamic);				break;
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:			result = deMinu32(resources, inputAttachments);						break;
#ifndef CTS_USES_VULKANSC
	case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:	result = deMinu32(resources, inlineUniforms);						break;
#endif
	default: DE_ASSERT(0);
	}

	DE_ASSERT(result);

	return result;
}

} // - namespace ut
} // - namespace DescriptorIndexing
} // - namespace vkt
