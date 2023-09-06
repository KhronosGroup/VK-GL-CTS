/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google Inc.
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
 * \brief YCbCr Test Utilities
 *//*--------------------------------------------------------------------*/

#include "vktYCbCrUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "deMath.h"
#include "deFloat16.h"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"

#include "deSTLUtil.hpp"
#include "deUniquePtr.hpp"

#include <limits>

namespace vkt
{
namespace ycbcr
{

using namespace vk;

using de::MovePtr;
using tcu::FloatFormat;
using tcu::Interval;
using tcu::IVec2;
using tcu::IVec4;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec4;
using std::vector;
using std::string;

// MultiPlaneImageData

MultiPlaneImageData::MultiPlaneImageData (VkFormat format, const UVec2& size)
	: m_format		(format)
	, m_description	(getPlanarFormatDescription(format))
	, m_size		(size)
{
	for (deUint32 planeNdx = 0; planeNdx < m_description.numPlanes; ++planeNdx)
		m_planeData[planeNdx].resize(getPlaneSizeInBytes(m_description, size, planeNdx, 0, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY));
}

MultiPlaneImageData::MultiPlaneImageData (const MultiPlaneImageData& other)
	: m_format		(other.m_format)
	, m_description	(other.m_description)
	, m_size		(other.m_size)
{
	for (deUint32 planeNdx = 0; planeNdx < m_description.numPlanes; ++planeNdx)
		m_planeData[planeNdx] = other.m_planeData[planeNdx];
}

MultiPlaneImageData::~MultiPlaneImageData (void)
{
}

tcu::PixelBufferAccess MultiPlaneImageData::getChannelAccess (deUint32 channelNdx)
{
	void*		planePtrs[PlanarFormatDescription::MAX_PLANES];
	deUint32	planeRowPitches[PlanarFormatDescription::MAX_PLANES];

	for (deUint32 planeNdx = 0; planeNdx < m_description.numPlanes; ++planeNdx)
	{
		const deUint32	planeW		= m_size.x() / ( m_description.blockWidth * m_description.planes[planeNdx].widthDivisor);
		planeRowPitches[planeNdx]	= m_description.planes[planeNdx].elementSizeBytes * planeW;
		planePtrs[planeNdx]			= &m_planeData[planeNdx][0];
	}

	return vk::getChannelAccess(m_description,
								m_size,
								planeRowPitches,
								planePtrs,
								channelNdx);
}

tcu::ConstPixelBufferAccess MultiPlaneImageData::getChannelAccess (deUint32 channelNdx) const
{
	const void*	planePtrs[PlanarFormatDescription::MAX_PLANES];
	deUint32	planeRowPitches[PlanarFormatDescription::MAX_PLANES];

	for (deUint32 planeNdx = 0; planeNdx < m_description.numPlanes; ++planeNdx)
	{
		const deUint32	planeW		= m_size.x() / (m_description.blockWidth * m_description.planes[planeNdx].widthDivisor);
		planeRowPitches[planeNdx]	= m_description.planes[planeNdx].elementSizeBytes * planeW;
		planePtrs[planeNdx]			= &m_planeData[planeNdx][0];
	}

	return vk::getChannelAccess(m_description,
								m_size,
								planeRowPitches,
								planePtrs,
								channelNdx);
}

// Misc utilities

namespace
{

void allocateStagingBuffers (const DeviceInterface&			vkd,
							 VkDevice						device,
							 Allocator&						allocator,
							 const MultiPlaneImageData&		imageData,
							 vector<VkBufferSp>*			buffers,
							 vector<AllocationSp>*			allocations)
{
	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		const VkBufferCreateInfo	bufferInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			(VkBufferCreateFlags)0u,
			(VkDeviceSize)imageData.getPlaneSize(planeNdx),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0u,
			(const deUint32*)DE_NULL,
		};
		Move<VkBuffer>				buffer		(createBuffer(vkd, device, &bufferInfo));
		MovePtr<Allocation>			allocation	(allocator.allocate(getBufferMemoryRequirements(vkd, device, *buffer),
																	MemoryRequirement::HostVisible|MemoryRequirement::Any));

		VK_CHECK(vkd.bindBufferMemory(device, *buffer, allocation->getMemory(), allocation->getOffset()));

		buffers->push_back(VkBufferSp(new Unique<VkBuffer>(buffer)));
		allocations->push_back(AllocationSp(allocation.release()));
	}
}

void allocateAndWriteStagingBuffers (const DeviceInterface&		vkd,
									  VkDevice						device,
									  Allocator&					allocator,
									  const MultiPlaneImageData&	imageData,
									  vector<VkBufferSp>*			buffers,
									  vector<AllocationSp>*			allocations)
{
	allocateStagingBuffers(vkd, device, allocator, imageData, buffers, allocations);

	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		deMemcpy((*allocations)[planeNdx]->getHostPtr(), imageData.getPlanePtr(planeNdx), imageData.getPlaneSize(planeNdx));
		flushMappedMemoryRange(vkd, device, (*allocations)[planeNdx]->getMemory(), 0u, VK_WHOLE_SIZE);
	}
}

void readStagingBuffers (MultiPlaneImageData*			imageData,
						 const DeviceInterface&			vkd,
						 VkDevice						device,
						 const vector<AllocationSp>&	allocations)
{
	for (deUint32 planeNdx = 0; planeNdx < imageData->getDescription().numPlanes; ++planeNdx)
	{
		invalidateMappedMemoryRange(vkd, device, allocations[planeNdx]->getMemory(), 0u, VK_WHOLE_SIZE);
		deMemcpy(imageData->getPlanePtr(planeNdx), allocations[planeNdx]->getHostPtr(), imageData->getPlaneSize(planeNdx));
	}
}

} // anonymous

void checkImageSupport (Context& context, VkFormat format, VkImageCreateFlags createFlags, VkImageTiling tiling)
{
	const bool													disjoint	= (createFlags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0;
	const VkPhysicalDeviceSamplerYcbcrConversionFeatures		features	= context.getSamplerYcbcrConversionFeatures();

	if (features.samplerYcbcrConversion == VK_FALSE)
		TCU_THROW(NotSupportedError, "samplerYcbcrConversion is not supported");

	if (disjoint)
	{
		context.requireDeviceFunctionality("VK_KHR_bind_memory2");
		context.requireDeviceFunctionality("VK_KHR_get_memory_requirements2");
	}

	{
		const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(),
																							context.getPhysicalDevice(),
																							format);
		const VkFormatFeatureFlags	featureFlags		= tiling == VK_IMAGE_TILING_OPTIMAL
														? formatProperties.optimalTilingFeatures
														: formatProperties.linearTilingFeatures;

		if ((featureFlags & (VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT | VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT)) == 0)
			TCU_THROW(NotSupportedError, "YCbCr conversion is not supported for format");

		if (disjoint && ((featureFlags & VK_FORMAT_FEATURE_DISJOINT_BIT) == 0))
			TCU_THROW(NotSupportedError, "Disjoint planes are not supported for format");
	}
}

void fillRandomNoNaN(de::Random* randomGen, deUint8* const data, deUint32 size, const vk::VkFormat format)
{
	bool isFloat = false;
	deUint32 stride = 1;

	switch (format)
	{
	case vk::VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		isFloat = true;
		stride = 1;
		break;
	case vk::VK_FORMAT_R16_SFLOAT:
	case vk::VK_FORMAT_R16G16_SFLOAT:
	case vk::VK_FORMAT_R16G16B16_SFLOAT:
	case vk::VK_FORMAT_R16G16B16A16_SFLOAT:
		isFloat = true;
		stride = 2;
		break;
	case vk::VK_FORMAT_R32_SFLOAT:
	case vk::VK_FORMAT_R32G32_SFLOAT:
	case vk::VK_FORMAT_R32G32B32_SFLOAT:
	case vk::VK_FORMAT_R32G32B32A32_SFLOAT:
		isFloat = true;
		stride = 4;
		break;
	case vk::VK_FORMAT_R64_SFLOAT:
	case vk::VK_FORMAT_R64G64_SFLOAT:
	case vk::VK_FORMAT_R64G64B64_SFLOAT:
	case vk::VK_FORMAT_R64G64B64A64_SFLOAT:
		isFloat = true;
		stride = 8;
		break;
	default:
		stride = 1;
		break;
	}

	if (isFloat) {
		deUint32 ndx = 0;
		for (; ndx < size - stride + 1; ndx += stride)
		{
			if (stride == 1) {
				// Set first bit of each channel to 0 to avoid NaNs, only format is B10G11R11
				const deUint8 mask[] = { 0x7F, 0xDF, 0xFB, 0xFF };
				// Apply mask for both endians
				data[ndx] = (randomGen->getUint8() & mask[ndx % 4]) & mask[3 - ndx % 4];
			}
			else if (stride == 2)
			{
				deFloat16* ptr = reinterpret_cast<deFloat16*>(&data[ndx]);
				*ptr = deFloat32To16(randomGen->getFloat());
			}
			else if (stride == 4)
			{
				float* ptr = reinterpret_cast<float*>(&data[ndx]);
				*ptr = randomGen->getFloat();
			}
			else if (stride == 8)
			{
				double* ptr = reinterpret_cast<double*>(&data[ndx]);
				*ptr = randomGen->getDouble();
			}
		}
		while (ndx < size) {
			data[ndx] = 0;
		}
	}
	else
	{
		for (deUint32 ndx = 0; ndx < size; ++ndx)
		{
			data[ndx] = randomGen->getUint8();
		}
	}
}

// When noNan is true, fillRandom does not generate NaNs in float formats.
void fillRandom (de::Random* randomGen, MultiPlaneImageData* imageData, const vk::VkFormat format, const bool noNan)
{
	for (deUint32 planeNdx = 0; planeNdx < imageData->getDescription().numPlanes; ++planeNdx)
	{
		const size_t	planeSize	= imageData->getPlaneSize(planeNdx);
		deUint8* const	planePtr	= (deUint8*)imageData->getPlanePtr(planeNdx);

		if (noNan) {
			fillRandomNoNaN(randomGen, planePtr, (deUint32)planeSize, format);
		}
		else
		{
			for (size_t ndx = 0; ndx < planeSize; ++ndx)
			{
				planePtr[ndx] = randomGen->getUint8();
			}
		}
	}
}

void fillGradient (MultiPlaneImageData* imageData, const tcu::Vec4& minVal, const tcu::Vec4& maxVal)
{
	const PlanarFormatDescription&	formatInfo	= imageData->getDescription();

	// \todo [pyry] Optimize: no point in re-rendering source gradient for each channel.

	for (deUint32 channelNdx = 0; channelNdx < 4; channelNdx++)
	{
		if (formatInfo.hasChannelNdx(channelNdx))
		{
			const tcu::PixelBufferAccess		channelAccess	= imageData->getChannelAccess(channelNdx);
			tcu::TextureLevel					tmpTexture		(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT),  channelAccess.getWidth(), channelAccess.getHeight());
			const tcu::ConstPixelBufferAccess	tmpAccess		= tmpTexture.getAccess();

			tcu::fillWithComponentGradients(tmpTexture, minVal, maxVal);

			for (int y = 0; y < channelAccess.getHeight(); ++y)
			for (int x = 0; x < channelAccess.getWidth(); ++x)
			{
				channelAccess.setPixel(tcu::Vec4(tmpAccess.getPixel(x, y)[channelNdx]), x, y);
			}
		}
	}
}

void fillZero (MultiPlaneImageData* imageData)
{
	for (deUint32 planeNdx = 0; planeNdx < imageData->getDescription().numPlanes; ++planeNdx)
		deMemset(imageData->getPlanePtr(planeNdx), 0, imageData->getPlaneSize(planeNdx));
}

vector<AllocationSp> allocateAndBindImageMemory (const DeviceInterface&	vkd,
												 VkDevice				device,
												 Allocator&				allocator,
												 VkImage				image,
												 VkFormat				format,
												 VkImageCreateFlags		createFlags,
												 vk::MemoryRequirement	requirement)
{
	vector<AllocationSp> allocations;

	if ((createFlags & VK_IMAGE_CREATE_DISJOINT_BIT) != 0)
	{
		const deUint32	numPlanes	= getPlaneCount(format);

		bindImagePlanesMemory(vkd, device, image, numPlanes, allocations, allocator, requirement);
	}
	else
	{
		const VkMemoryRequirements	reqs	= getImageMemoryRequirements(vkd, device, image);

		allocations.push_back(AllocationSp(allocator.allocate(reqs, requirement).release()));

		VK_CHECK(vkd.bindImageMemory(device, image, allocations.back()->getMemory(), allocations.back()->getOffset()));
	}

	return allocations;
}

void uploadImage (const DeviceInterface&		vkd,
				  VkDevice						device,
				  deUint32						queueFamilyNdx,
				  Allocator&					allocator,
				  VkImage						image,
				  const MultiPlaneImageData&	imageData,
				  VkAccessFlags					nextAccess,
				  VkImageLayout					finalLayout,
				  deUint32						arrayLayer)
{
	const VkQueue					queue			= getDeviceQueue(vkd, device, queueFamilyNdx, 0u);
	const Unique<VkCommandPool>		cmdPool			(createCommandPool(vkd, device, (VkCommandPoolCreateFlags)0, queueFamilyNdx));
	const Unique<VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vector<VkBufferSp>				stagingBuffers;
	vector<AllocationSp>			stagingMemory;

	const PlanarFormatDescription&	formatDesc		= imageData.getDescription();

	allocateAndWriteStagingBuffers(vkd, device, allocator, imageData, &stagingBuffers, &stagingMemory);

	beginCommandBuffer(vkd, *cmdBuffer);

	for (deUint32 planeNdx = 0; planeNdx < imageData.getDescription().numPlanes; ++planeNdx)
	{
		const VkImageAspectFlagBits	aspect	= (formatDesc.numPlanes > 1)
											? getPlaneAspect(planeNdx)
											: VK_IMAGE_ASPECT_COLOR_BIT;
		const VkExtent3D imageExtent		= makeExtent3D(imageData.getSize().x(), imageData.getSize().y(), 1u);
		const VkExtent3D planeExtent		= getPlaneExtent(formatDesc, imageExtent, planeNdx, 0);
		const VkBufferImageCopy		copy	=
		{
			0u,		// bufferOffset
			0u,		// bufferRowLength
			0u,		// bufferImageHeight
			{ (VkImageAspectFlags)aspect, 0u, arrayLayer, 1u },
			makeOffset3D(0u, 0u, 0u),
			planeExtent
		};

		{
			const VkImageMemoryBarrier		preCopyBarrier	=
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					DE_NULL,
					(VkAccessFlags)0,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED,
					VK_QUEUE_FAMILY_IGNORED,
					image,
					{ (VkImageAspectFlags)aspect, 0u, 1u, arrayLayer, 1u }
				};

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   (VkPipelineStageFlags)VK_PIPELINE_STAGE_HOST_BIT,
								   (VkPipelineStageFlags)VK_PIPELINE_STAGE_TRANSFER_BIT,
								   (VkDependencyFlags)0u,
								   0u,
								   (const VkMemoryBarrier*)DE_NULL,
								   0u,
								   (const VkBufferMemoryBarrier*)DE_NULL,
								   1u,
								   &preCopyBarrier);
		}

		vkd.cmdCopyBufferToImage(*cmdBuffer, **stagingBuffers[planeNdx], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy);

		{
			const VkImageMemoryBarrier		postCopyBarrier	=
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					DE_NULL,
					VK_ACCESS_TRANSFER_WRITE_BIT,
					nextAccess,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					finalLayout,
					VK_QUEUE_FAMILY_IGNORED,
					VK_QUEUE_FAMILY_IGNORED,
					image,
					{ (VkImageAspectFlags)aspect, 0u, 1u, arrayLayer, 1u }
				};

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   (VkPipelineStageFlags)VK_PIPELINE_STAGE_TRANSFER_BIT,
								   (VkPipelineStageFlags)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								   (VkDependencyFlags)0u,
								   0u,
								   (const VkMemoryBarrier*)DE_NULL,
								   0u,
								   (const VkBufferMemoryBarrier*)DE_NULL,
								   1u,
								   &postCopyBarrier);
		}

	}

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
}

void fillImageMemory (const vk::DeviceInterface&							vkd,
					  vk::VkDevice											device,
					  deUint32												queueFamilyNdx,
					  vk::VkImage											image,
					  const std::vector<de::SharedPtr<vk::Allocation> >&	allocations,
					  const MultiPlaneImageData&							imageData,
					  vk::VkAccessFlags										nextAccess,
					  vk::VkImageLayout										finalLayout,
					  deUint32												arrayLayer)
{
	const VkQueue					queue			= getDeviceQueue(vkd, device, queueFamilyNdx, 0u);
	const Unique<VkCommandPool>		cmdPool			(createCommandPool(vkd, device, (VkCommandPoolCreateFlags)0, queueFamilyNdx));
	const Unique<VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const PlanarFormatDescription&	formatDesc		= imageData.getDescription();

	for (deUint32 planeNdx = 0; planeNdx < formatDesc.numPlanes; ++planeNdx)
	{
		const VkImageAspectFlagBits			aspect		= (formatDesc.numPlanes > 1)
														? getPlaneAspect(planeNdx)
														: VK_IMAGE_ASPECT_COLOR_BIT;
		const de::SharedPtr<Allocation>&	allocation	= allocations.size() > 1
														? allocations[planeNdx]
														: allocations[0];
		const size_t						planeSize	= imageData.getPlaneSize(planeNdx);
		const deUint32						planeH		= imageData.getSize().y() / formatDesc.planes[planeNdx].heightDivisor;
		const VkImageSubresource			subresource	=
		{
			static_cast<vk::VkImageAspectFlags>(aspect),
			0u,
			arrayLayer,
		};
		VkSubresourceLayout			layout;

		vkd.getImageSubresourceLayout(device, image, &subresource, &layout);

		for (deUint32 row = 0; row < planeH; ++row)
		{
			const size_t		rowSize		= planeSize / planeH;
			void* const			dstPtr		= ((deUint8*)allocation->getHostPtr()) + layout.offset + layout.rowPitch * row;
			const void* const	srcPtr		= ((const deUint8*)imageData.getPlanePtr(planeNdx)) + row * rowSize;

			deMemcpy(dstPtr, srcPtr, rowSize);
		}
		flushMappedMemoryRange(vkd, device, allocation->getMemory(), 0u, VK_WHOLE_SIZE);
	}

	beginCommandBuffer(vkd, *cmdBuffer);

	{
		const VkImageMemoryBarrier		postCopyBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			0u,
			nextAccess,
			VK_IMAGE_LAYOUT_PREINITIALIZED,
			finalLayout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, arrayLayer, 1u }
		};

		vkd.cmdPipelineBarrier(*cmdBuffer,
								(VkPipelineStageFlags)VK_PIPELINE_STAGE_HOST_BIT,
								(VkPipelineStageFlags)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								(VkDependencyFlags)0u,
								0u,
								(const VkMemoryBarrier*)DE_NULL,
								0u,
								(const VkBufferMemoryBarrier*)DE_NULL,
								1u,
								&postCopyBarrier);
	}

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
}

void downloadImage (const DeviceInterface&	vkd,
					VkDevice				device,
					deUint32				queueFamilyNdx,
					Allocator&				allocator,
					VkImage					image,
					MultiPlaneImageData*	imageData,
					VkAccessFlags			prevAccess,
					VkImageLayout			initialLayout)
{
	const VkQueue					queue			= getDeviceQueue(vkd, device, queueFamilyNdx, 0u);
	const Unique<VkCommandPool>		cmdPool			(createCommandPool(vkd, device, (VkCommandPoolCreateFlags)0, queueFamilyNdx));
	const Unique<VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vector<VkBufferSp>				stagingBuffers;
	vector<AllocationSp>			stagingMemory;

	const PlanarFormatDescription&	formatDesc		= imageData->getDescription();

	allocateStagingBuffers(vkd, device, allocator, *imageData, &stagingBuffers, &stagingMemory);

	beginCommandBuffer(vkd, *cmdBuffer);

	for (deUint32 planeNdx = 0; planeNdx < imageData->getDescription().numPlanes; ++planeNdx)
	{
		const VkImageAspectFlagBits	aspect	= (formatDesc.numPlanes > 1)
											? getPlaneAspect(planeNdx)
											: VK_IMAGE_ASPECT_COLOR_BIT;
		{
			const VkImageMemoryBarrier		preCopyBarrier	=
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				DE_NULL,
				prevAccess,
				VK_ACCESS_TRANSFER_READ_BIT,
				initialLayout,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				image,
				{
					static_cast<vk::VkImageAspectFlags>(aspect),
					0u,
					1u,
					0u,
					1u
				}
			};

			vkd.cmdPipelineBarrier(*cmdBuffer,
									(VkPipelineStageFlags)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
									(VkPipelineStageFlags)VK_PIPELINE_STAGE_TRANSFER_BIT,
									(VkDependencyFlags)0u,
									0u,
									(const VkMemoryBarrier*)DE_NULL,
									0u,
									(const VkBufferMemoryBarrier*)DE_NULL,
									1u,
									&preCopyBarrier);
		}
		{
			const VkExtent3D imageExtent		= makeExtent3D(imageData->getSize().x(), imageData->getSize().y(), 1u);
			const VkExtent3D planeExtent		= getPlaneExtent(formatDesc, imageExtent, planeNdx, 0);
			const VkBufferImageCopy		copy	=
			{
				0u,		// bufferOffset
				0u,		// bufferRowLength
				0u,		// bufferImageHeight
				{ (VkImageAspectFlags)aspect, 0u, 0u, 1u },
				makeOffset3D(0u, 0u, 0u),
				planeExtent
			};

			vkd.cmdCopyImageToBuffer(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **stagingBuffers[planeNdx], 1u, &copy);
		}
		{
			const VkBufferMemoryBarrier		postCopyBarrier	=
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				DE_NULL,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_HOST_READ_BIT,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				**stagingBuffers[planeNdx],
				0u,
				VK_WHOLE_SIZE
			};

			vkd.cmdPipelineBarrier(*cmdBuffer,
									(VkPipelineStageFlags)VK_PIPELINE_STAGE_TRANSFER_BIT,
									(VkPipelineStageFlags)VK_PIPELINE_STAGE_HOST_BIT,
									(VkDependencyFlags)0u,
									0u,
									(const VkMemoryBarrier*)DE_NULL,
									1u,
									&postCopyBarrier,
									0u,
									(const VkImageMemoryBarrier*)DE_NULL);
		}
	}

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	readStagingBuffers(imageData, vkd, device, stagingMemory);
}

void readImageMemory (const vk::DeviceInterface&							vkd,
					  vk::VkDevice											device,
					  deUint32												queueFamilyNdx,
					  vk::VkImage											image,
					  const std::vector<de::SharedPtr<vk::Allocation> >&	allocations,
					  MultiPlaneImageData*									imageData,
					  vk::VkAccessFlags										prevAccess,
					  vk::VkImageLayout										initialLayout)
{
	const VkQueue					queue			= getDeviceQueue(vkd, device, queueFamilyNdx, 0u);
	const Unique<VkCommandPool>		cmdPool			(createCommandPool(vkd, device, (VkCommandPoolCreateFlags)0, queueFamilyNdx));
	const Unique<VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const PlanarFormatDescription&	formatDesc		= imageData->getDescription();

	beginCommandBuffer(vkd, *cmdBuffer);

	{
		const VkImageMemoryBarrier		preCopyBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			DE_NULL,
			prevAccess,
			vk::VK_ACCESS_HOST_READ_BIT,
			initialLayout,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }
		};

		vkd.cmdPipelineBarrier(*cmdBuffer,
								(VkPipelineStageFlags)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								(VkPipelineStageFlags)VK_PIPELINE_STAGE_HOST_BIT,
								(VkDependencyFlags)0u,
								0u,
								(const VkMemoryBarrier*)DE_NULL,
								0u,
								(const VkBufferMemoryBarrier*)DE_NULL,
								1u,
								&preCopyBarrier);
	}

	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

	for (deUint32 planeNdx = 0; planeNdx < formatDesc.numPlanes; ++planeNdx)
	{
		const VkImageAspectFlagBits			aspect		= (formatDesc.numPlanes > 1)
														? getPlaneAspect(planeNdx)
														: VK_IMAGE_ASPECT_COLOR_BIT;
		const de::SharedPtr<Allocation>&	allocation	= allocations.size() > 1
														? allocations[planeNdx]
														: allocations[0];
		const size_t						planeSize	= imageData->getPlaneSize(planeNdx);
		const deUint32						planeH		= imageData->getSize().y() / formatDesc.planes[planeNdx].heightDivisor;
		const VkImageSubresource			subresource	=
		{
			static_cast<vk::VkImageAspectFlags>(aspect),
			0u,
			0u,
		};
		VkSubresourceLayout			layout;

		vkd.getImageSubresourceLayout(device, image, &subresource, &layout);

		invalidateMappedMemoryRange(vkd, device, allocation->getMemory(), 0u, VK_WHOLE_SIZE);

		for (deUint32 row = 0; row < planeH; ++row)
		{
			const size_t		rowSize	= planeSize / planeH;
			const void* const	srcPtr	= ((const deUint8*)allocation->getHostPtr()) + layout.offset + layout.rowPitch * row;
			void* const			dstPtr	= ((deUint8*)imageData->getPlanePtr(planeNdx)) + row * rowSize;

			deMemcpy(dstPtr, srcPtr, rowSize);
		}
	}
}

// ChannelAccess utilities
namespace
{

//! Extend < 32b signed integer to 32b
inline deInt32 signExtend (deUint32 src, int bits)
{
	const deUint32 signBit = 1u << (bits-1);

	src |= ~((src & signBit) - 1);

	return (deInt32)src;
}

deUint32 divRoundUp (deUint32 a, deUint32 b)
{
	if (a % b == 0)
		return a / b;
	else
		return (a / b) + 1;
}

// \todo Taken from tcuTexture.cpp
// \todo [2011-09-21 pyry] Move to tcutil?
template <typename T>
inline T convertSatRte (float f)
{
	// \note Doesn't work for 64-bit types
	DE_STATIC_ASSERT(sizeof(T) < sizeof(deUint64));
	DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

	deInt64	minVal	= std::numeric_limits<T>::min();
	deInt64 maxVal	= std::numeric_limits<T>::max();
	float	q		= deFloatFrac(f);
	deInt64 intVal	= (deInt64)(f-q);

	// Rounding.
	if (q == 0.5f)
	{
		if (intVal % 2 != 0)
			intVal++;
	}
	else if (q > 0.5f)
		intVal++;
	// else Don't add anything

	// Saturate.
	intVal = de::max(minVal, de::min(maxVal, intVal));

	return (T)intVal;
}

} // anonymous

ChannelAccess::ChannelAccess (tcu::TextureChannelClass	channelClass,
							  deUint8					channelSize,
							  const tcu::IVec3&			size,
							  const tcu::IVec3&			bitPitch,
							  void*						data,
							  deUint32					bitOffset)
	: m_channelClass	(channelClass)
	, m_channelSize		(channelSize)
	, m_size			(size)
	, m_bitPitch		(bitPitch)
	, m_data			((deUint8*)data + (bitOffset / 8))
	, m_bitOffset		(bitOffset % 8)
{
}

deUint32 ChannelAccess::getChannelUint (const tcu::IVec3& pos) const
{
	DE_ASSERT(pos[0] < m_size[0]);
	DE_ASSERT(pos[1] < m_size[1]);
	DE_ASSERT(pos[2] < m_size[2]);

	const deInt32			bitOffset	(m_bitOffset + tcu::dot(m_bitPitch, pos));
	const deUint8* const	firstByte	= ((const deUint8*)m_data) + (bitOffset / 8);
	const deUint32			byteCount	= divRoundUp((bitOffset + m_channelSize) - 8u * (bitOffset / 8u), 8u);
	const deUint32			mask		(m_channelSize == 32u ? ~0x0u : (0x1u << m_channelSize) - 1u);
	const deUint32			offset		= bitOffset % 8;
	deUint32				bits		= 0u;

	deMemcpy(&bits, firstByte, byteCount);

	return (bits >> offset) & mask;
}

void ChannelAccess::setChannel (const tcu::IVec3& pos, deUint32 x)
{
	DE_ASSERT(pos[0] < m_size[0]);
	DE_ASSERT(pos[1] < m_size[1]);
	DE_ASSERT(pos[2] < m_size[2]);

	const deInt32	bitOffset	(m_bitOffset + tcu::dot(m_bitPitch, pos));
	deUint8* const	firstByte	= ((deUint8*)m_data) + (bitOffset / 8);
	const deUint32	byteCount	= divRoundUp((bitOffset + m_channelSize) - 8u * (bitOffset / 8u), 8u);
	const deUint32	mask		(m_channelSize == 32u ? ~0x0u : (0x1u << m_channelSize) - 1u);
	const deUint32	offset		= bitOffset % 8;

	const deUint32	bits		= (x & mask) << offset;
	deUint32		oldBits		= 0;

	deMemcpy(&oldBits, firstByte, byteCount);

	{
		const deUint32	newBits	= bits | (oldBits & (~(mask << offset)));

		deMemcpy(firstByte, &newBits,  byteCount);
	}
}

float ChannelAccess::getChannel (const tcu::IVec3& pos) const
{
	const deUint32	bits	(getChannelUint(pos));

	switch (m_channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
			return (float)bits / (float)(m_channelSize == 32 ? ~0x0u : ((0x1u << m_channelSize) - 1u));

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return (float)bits;

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
			return de::max(-1.0f, (float)signExtend(bits, m_channelSize) / (float)((0x1u << (m_channelSize - 1u)) - 1u));

		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return (float)signExtend(bits, m_channelSize);

		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			if (m_channelSize == 32)
				return tcu::Float32(bits).asFloat();
			else
			{
				DE_FATAL("Float type not supported");
				return -1.0f;
			}

		default:
			DE_FATAL("Unknown texture channel class");
			return -1.0f;
	}
}

tcu::Interval ChannelAccess::getChannel (const tcu::FloatFormat&	conversionFormat,
										 const tcu::IVec3&			pos) const
{
	const deUint32	bits	(getChannelUint(pos));

	switch (m_channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
			return conversionFormat.roundOut(conversionFormat.roundOut((double)bits, false)
											/ conversionFormat.roundOut((double)(m_channelSize == 32 ? ~0x0u : ((0x1u << m_channelSize) - 1u)), false), false);

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return conversionFormat.roundOut((double)bits, false);

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		{
			const tcu::Interval result (conversionFormat.roundOut(conversionFormat.roundOut((double)signExtend(bits, m_channelSize), false)
																/ conversionFormat.roundOut((double)((0x1u << (m_channelSize - 1u)) - 1u), false), false));

			return tcu::Interval(de::max(-1.0, result.lo()), de::max(-1.0, result.hi()));
		}

		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return conversionFormat.roundOut((double)signExtend(bits, m_channelSize), false);

		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
			if (m_channelSize == 32)
				return conversionFormat.roundOut(tcu::Float32(bits).asFloat(), false);
			else
			{
				DE_FATAL("Float type not supported");
				return tcu::Interval();
			}

		default:
			DE_FATAL("Unknown texture channel class");
			return tcu::Interval();
	}
}

void ChannelAccess::setChannel (const tcu::IVec3& pos, float x)
{
	DE_ASSERT(pos[0] < m_size[0]);
	DE_ASSERT(pos[1] < m_size[1]);
	DE_ASSERT(pos[2] < m_size[2]);

	const deUint32	mask	(m_channelSize == 32u ? ~0x0u : (0x1u << m_channelSize) - 1u);

	switch (m_channelClass)
	{
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		{
			const deUint32	maxValue	(mask);
			const deUint32	value		(de::min(maxValue, (deUint32)convertSatRte<deUint32>(x * (float)maxValue)));
			setChannel(pos, value);
			break;
		}

		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		{
			const deInt32	range	((0x1u << (m_channelSize - 1u)) - 1u);
			const deUint32	value	((deUint32)de::clamp<deInt32>(convertSatRte<deInt32>(x * (float)range), -range, range));
			setChannel(pos, value);
			break;
		}

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		{
			const deUint32	maxValue	(mask);
			const deUint32	value		(de::min(maxValue, (deUint32)x));
			setChannel(pos, value);
			break;
		}

		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
		{
			const deInt32	minValue	(-(deInt32)(1u << (m_channelSize - 1u)));
			const deInt32	maxValue	((deInt32)((1u << (m_channelSize - 1u)) - 1u));
			const deUint32	value		((deUint32)de::clamp((deInt32)x, minValue, maxValue));
			setChannel(pos, value);
			break;
		}

		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
		{
			if (m_channelSize == 32)
			{
				const deUint32	value		= tcu::Float32(x).bits();
				setChannel(pos, value);
			}
			else
				DE_FATAL("Float type not supported");
			break;
		}

		default:
			DE_FATAL("Unknown texture channel class");
	}
}

ChannelAccess getChannelAccess (MultiPlaneImageData&				data,
								const vk::PlanarFormatDescription&	formatInfo,
								const UVec2&						size,
								int									channelNdx)
{
	DE_ASSERT(formatInfo.hasChannelNdx(channelNdx));

	const deUint32	planeNdx			= formatInfo.channels[channelNdx].planeNdx;
	const deUint32	valueOffsetBits		= formatInfo.channels[channelNdx].offsetBits;
	const deUint32	pixelStrideBytes	= formatInfo.channels[channelNdx].strideBytes;
	const deUint32	pixelStrideBits		= pixelStrideBytes * 8;
	const deUint8	sizeBits			= formatInfo.channels[channelNdx].sizeBits;

	DE_ASSERT(size.x() % (formatInfo.blockWidth * formatInfo.planes[planeNdx].widthDivisor) == 0);
	DE_ASSERT(size.y() % (formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor) == 0);

	deUint32		accessWidth			= size.x() / ( formatInfo.blockWidth * formatInfo.planes[planeNdx].widthDivisor );
	const deUint32	accessHeight		= size.y() / ( formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor );
	const deUint32	elementSizeBytes	= formatInfo.planes[planeNdx].elementSizeBytes;
	const deUint32	rowPitch			= formatInfo.planes[planeNdx].elementSizeBytes * accessWidth;
	const deUint32	rowPitchBits		= rowPitch * 8;

	if (pixelStrideBytes != elementSizeBytes)
	{
		DE_ASSERT(elementSizeBytes % pixelStrideBytes == 0);
		accessWidth *= elementSizeBytes/pixelStrideBytes;
	}

	return ChannelAccess((tcu::TextureChannelClass)formatInfo.channels[channelNdx].type, sizeBits, tcu::IVec3(accessWidth, accessHeight, 1u), tcu::IVec3((int)pixelStrideBits, (int)rowPitchBits, 0), data.getPlanePtr(planeNdx), (deUint32)valueOffsetBits);
}

bool isXChromaSubsampled (vk::VkFormat format)
{
	switch (format)
	{
		case vk::VK_FORMAT_G8B8G8R8_422_UNORM:
		case vk::VK_FORMAT_B8G8R8G8_422_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G16B16G16R16_422_UNORM:
		case vk::VK_FORMAT_B16G16R16G16_422_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
			return true;

		default:
			return false;
	}
}

bool isYChromaSubsampled (vk::VkFormat format)
{
	switch (format)
	{
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
			return true;

		default:
			return false;
	}
}

bool areLsb6BitsDontCare(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
	if ((srcFormat == vk::VK_FORMAT_R10X6_UNORM_PACK16)	                        ||
		(dstFormat == vk::VK_FORMAT_R10X6_UNORM_PACK16)                         ||
		(srcFormat == vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16)                   ||
		(dstFormat == vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16)                   ||
		(srcFormat == vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)         ||
		(dstFormat == vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)         ||
		(srcFormat == vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16)     ||
		(dstFormat == vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16)     ||
		(srcFormat == vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16)     ||
		(dstFormat == vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16)     ||
		(srcFormat == vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16)  ||
		(dstFormat == vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16)  ||
		(srcFormat == vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16) ||
		(dstFormat == vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16) ||
		(srcFormat == vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16) ||
		(dstFormat == vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16) ||
		(srcFormat == vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16)  ||
		(dstFormat == vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16)  ||
		(srcFormat == vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16) ||
		(dstFormat == vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16))
	{
		return true;
	}

	return false;
}

bool areLsb4BitsDontCare(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
	if ((srcFormat == vk::VK_FORMAT_R12X4_UNORM_PACK16)                         ||
		(dstFormat == vk::VK_FORMAT_R12X4_UNORM_PACK16)                         ||
		(srcFormat == vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16)                   ||
		(dstFormat == vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16)                   ||
		(srcFormat == vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16)         ||
		(dstFormat == vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16)         ||
		(srcFormat == vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16)     ||
		(dstFormat == vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16)     ||
		(srcFormat == vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16)     ||
		(dstFormat == vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16)     ||
		(srcFormat == vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16) ||
		(dstFormat == vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16) ||
		(srcFormat == vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16)  ||
		(dstFormat == vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16)  ||
		(srcFormat == vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16) ||
		(dstFormat == vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16) ||
		(srcFormat == vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16)  ||
		(dstFormat == vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16)  ||
		(srcFormat == vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16) ||
		(dstFormat == vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16))
	{
		return true;
	}

	return false;
}

// \note Used for range expansion
tcu::UVec4 getYCbCrBitDepth (vk::VkFormat format)
{
	switch (format)
	{
		case vk::VK_FORMAT_G8B8G8R8_422_UNORM:
		case vk::VK_FORMAT_B8G8R8G8_422_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
			return tcu::UVec4(8, 8, 8, 0);

		case vk::VK_FORMAT_R10X6_UNORM_PACK16:
			return tcu::UVec4(10, 0, 0, 0);

		case vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
			return tcu::UVec4(10, 10, 0, 0);

		case vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
			return tcu::UVec4(10, 10, 10, 10);

		case vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
			return tcu::UVec4(10, 10, 10, 0);

		case vk::VK_FORMAT_R12X4_UNORM_PACK16:
			return tcu::UVec4(12, 0, 0, 0);

		case vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
			return tcu::UVec4(12, 12, 0, 0);

		case vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
			return tcu::UVec4(12, 12, 12, 12);

		case vk::VK_FORMAT_G16B16G16R16_422_UNORM:
		case vk::VK_FORMAT_B16G16R16G16_422_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
			return tcu::UVec4(16, 16, 16, 0);

		default:
			return tcu::getTextureFormatBitDepth(vk::mapVkFormat(format)).cast<deUint32>();
	}
}

std::vector<tcu::FloatFormat> getPrecision (VkFormat format)
{
	std::vector<FloatFormat>	floatFormats;
	UVec4						channelDepth	= getYCbCrBitDepth (format);

	for (deUint32 channelIdx = 0; channelIdx < 4; channelIdx++)
		floatFormats.push_back(tcu::FloatFormat(0, 0, channelDepth[channelIdx], false, tcu::YES));

	return floatFormats;
}

deUint32 getYCbCrFormatChannelCount (vk::VkFormat format)
{
	switch (format)
	{
		case vk::VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		case vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		case vk::VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		case vk::VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		case vk::VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		case vk::VK_FORMAT_B5G5R5A1_UNORM_PACK16:
		case vk::VK_FORMAT_B8G8R8A8_UNORM:
		case vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		case vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case vk::VK_FORMAT_R16G16B16A16_UNORM:
		case vk::VK_FORMAT_R4G4B4A4_UNORM_PACK16:
		case vk::VK_FORMAT_R5G5B5A1_UNORM_PACK16:
		case vk::VK_FORMAT_R8G8B8A8_UNORM:
			return 4;

		case vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case vk::VK_FORMAT_B16G16R16G16_422_UNORM:
		case vk::VK_FORMAT_B5G6R5_UNORM_PACK16:
		case vk::VK_FORMAT_B8G8R8G8_422_UNORM:
		case vk::VK_FORMAT_B8G8R8_UNORM:
		case vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		case vk::VK_FORMAT_G16B16G16R16_422_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
		case vk::VK_FORMAT_G8B8G8R8_422_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
		case vk::VK_FORMAT_R16G16B16_UNORM:
		case vk::VK_FORMAT_R5G6B5_UNORM_PACK16:
		case vk::VK_FORMAT_R8G8B8_UNORM:
			return 3;

		case vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
		case vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
			return 2;

		case vk::VK_FORMAT_R10X6_UNORM_PACK16:
		case vk::VK_FORMAT_R12X4_UNORM_PACK16:
			return 1;

		default:
			DE_FATAL("Unknown number of channels");
			return -1;
	}
}

// YCbCr color conversion utilities
namespace
{

tcu::Interval rangeExpandChroma (vk::VkSamplerYcbcrRange		range,
								 const tcu::FloatFormat&		conversionFormat,
								 const deUint32					bits,
								 const tcu::Interval&			sample)
{
	const deUint32	values	(0x1u << bits);

	switch (range)
	{
		case vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL:
			return conversionFormat.roundOut(sample - conversionFormat.roundOut(tcu::Interval((double)(0x1u << (bits - 1u)) / (double)((0x1u << bits) - 1u)), false), false);

		case vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW:
		{
			const tcu::Interval	a			(conversionFormat.roundOut(sample * tcu::Interval((double)(values - 1u)), false));
			const tcu::Interval	dividend	(conversionFormat.roundOut(a - tcu::Interval((double)(128u * (0x1u << (bits - 8u)))), false));
			const tcu::Interval	divisor		((double)(224u * (0x1u << (bits - 8u))));
			const tcu::Interval	result		(conversionFormat.roundOut(dividend / divisor, false));

			return result;
		}

		default:
			DE_FATAL("Unknown YCbCrRange");
			return tcu::Interval();
	}
}

tcu::Interval rangeExpandLuma (vk::VkSamplerYcbcrRange		range,
							   const tcu::FloatFormat&		conversionFormat,
							   const deUint32				bits,
							   const tcu::Interval&			sample)
{
	const deUint32	values	(0x1u << bits);

	switch (range)
	{
		case vk::VK_SAMPLER_YCBCR_RANGE_ITU_FULL:
			return conversionFormat.roundOut(sample, false);

		case vk::VK_SAMPLER_YCBCR_RANGE_ITU_NARROW:
		{
			const tcu::Interval	a			(conversionFormat.roundOut(sample * tcu::Interval((double)(values - 1u)), false));
			const tcu::Interval	dividend	(conversionFormat.roundOut(a - tcu::Interval((double)(16u * (0x1u << (bits - 8u)))), false));
			const tcu::Interval	divisor		((double)(219u * (0x1u << (bits - 8u))));
			const tcu::Interval	result		(conversionFormat.roundOut(dividend / divisor, false));

			return result;
		}

		default:
			DE_FATAL("Unknown YCbCrRange");
			return tcu::Interval();
	}
}

tcu::Interval clampMaybe (const tcu::Interval&	x,
						  double				min,
						  double				max)
{
	tcu::Interval result = x;

	DE_ASSERT(min <= max);

	if (x.lo() < min)
		result = result | tcu::Interval(min);

	if (x.hi() > max)
		result = result | tcu::Interval(max);

	return result;
}

void convertColor (vk::VkSamplerYcbcrModelConversion	colorModel,
				   vk::VkSamplerYcbcrRange				range,
				   const vector<tcu::FloatFormat>&		conversionFormat,
				   const tcu::UVec4&					bitDepth,
				   const tcu::Interval					input[4],
				   tcu::Interval						output[4])
{
	switch (colorModel)
	{
		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY:
		{
			for (size_t ndx = 0; ndx < 4; ndx++)
				output[ndx] = input[ndx];
			break;
		}

		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY:
		{
			output[0] = clampMaybe(rangeExpandChroma(range, conversionFormat[0], bitDepth[0], input[0]), -0.5, 0.5);
			output[1] = clampMaybe(rangeExpandLuma(range, conversionFormat[1], bitDepth[1], input[1]), 0.0, 1.0);
			output[2] = clampMaybe(rangeExpandChroma(range, conversionFormat[2], bitDepth[2], input[2]), -0.5, 0.5);
			output[3] = input[3];
			break;
		}

		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601:
		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709:
		case vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020:
		{
			const tcu::Interval	y			(rangeExpandLuma(range, conversionFormat[1], bitDepth[1], input[1]));
			const tcu::Interval	cr			(rangeExpandChroma(range, conversionFormat[0], bitDepth[0], input[0]));
			const tcu::Interval	cb			(rangeExpandChroma(range, conversionFormat[2], bitDepth[2], input[2]));

			const tcu::Interval	yClamped	(clampMaybe(y,   0.0, 1.0));
			const tcu::Interval	crClamped	(clampMaybe(cr, -0.5, 0.5));
			const tcu::Interval	cbClamped	(clampMaybe(cb, -0.5, 0.5));

			if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601)
			{
				output[0] = conversionFormat[0].roundOut(yClamped + conversionFormat[0].roundOut(1.402 * crClamped, false), false);
				output[1] = conversionFormat[1].roundOut(conversionFormat[1].roundOut(yClamped - conversionFormat[1].roundOut((0.202008 / 0.587) * cbClamped, false), false) - conversionFormat[1].roundOut((0.419198 / 0.587) * crClamped, false), false);
				output[2] = conversionFormat[2].roundOut(yClamped + conversionFormat[2].roundOut(1.772 * cbClamped, false), false);
			}
			else if (colorModel == vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709)
			{
				output[0] = conversionFormat[0].roundOut(yClamped + conversionFormat[0].roundOut(1.5748 * crClamped, false), false);
				output[1] = conversionFormat[1].roundOut(conversionFormat[1].roundOut(yClamped - conversionFormat[1].roundOut((0.13397432 / 0.7152) * cbClamped, false), false) - conversionFormat[1].roundOut((0.33480248 / 0.7152) * crClamped, false), false);
				output[2] = conversionFormat[2].roundOut(yClamped + conversionFormat[2].roundOut(1.8556 * cbClamped, false), false);
			}
			else
			{
				output[0] = conversionFormat[0].roundOut(yClamped + conversionFormat[0].roundOut(1.4746 * crClamped, false), false);
				output[1] = conversionFormat[1].roundOut(conversionFormat[1].roundOut(yClamped - conversionFormat[1].roundOut(conversionFormat[1].roundOut(0.11156702 / 0.6780, false) * cbClamped, false), false) - conversionFormat[1].roundOut(conversionFormat[1].roundOut(0.38737742 / 0.6780, false) * crClamped, false), false);
				output[2] = conversionFormat[2].roundOut(yClamped + conversionFormat[2].roundOut(1.8814 * cbClamped, false), false);
			}
			output[3] = input[3];
			break;
		}

		default:
			DE_FATAL("Unknown YCbCrModel");
	}

	if (colorModel != vk::VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY)
	{
		for (int ndx = 0; ndx < 3; ndx++)
			output[ndx] = clampMaybe(output[ndx], 0.0, 1.0);
	}
}

int mirror (int coord)
{
	return coord >= 0 ? coord : -(1 + coord);
}

int imod (int a, int b)
{
	int m = a % b;
	return m < 0 ? m + b : m;
}

tcu::Interval frac (const tcu::Interval& x)
{
	if (x.hi() - x.lo() >= 1.0)
		return tcu::Interval(0.0, 1.0);
	else
	{
		const tcu::Interval ret (deFrac(x.lo()), deFrac(x.hi()));

		return ret;
	}
}

tcu::Interval calculateUV (const tcu::FloatFormat&	coordFormat,
						   const tcu::Interval&		st,
						   const int				size)
{
	return coordFormat.roundOut(coordFormat.roundOut(st, false) * tcu::Interval((double)size), false);
}

tcu::IVec2 calculateNearestIJRange (const tcu::FloatFormat&	coordFormat,
								    const tcu::Interval&	uv)
{
	const tcu::Interval	ij	(coordFormat.roundOut(coordFormat.roundOut(uv, false) - tcu::Interval(0.5), false));

	return tcu::IVec2(deRoundToInt32(ij.lo() - coordFormat.ulp(ij.lo(), 1)), deRoundToInt32(ij.hi() + coordFormat.ulp(ij.hi(), 1)));
}

// Calculate range of pixel coordinates that can be used as lower coordinate for linear sampling
tcu::IVec2 calculateLinearIJRange (const tcu::FloatFormat&	coordFormat,
								   const tcu::Interval&		uv)
{
	const tcu::Interval	ij	(coordFormat.roundOut(uv - tcu::Interval(0.5), false));

	return tcu::IVec2(deFloorToInt32(ij.lo()), deFloorToInt32(ij.hi()));
}

tcu::IVec2 calculateIJRange (vk::VkFilter				filter,
							 const tcu::FloatFormat&	coordFormat,
							 const tcu::Interval&		uv)
{
	DE_ASSERT(filter == vk::VK_FILTER_NEAREST || filter == vk::VK_FILTER_LINEAR);
	return (filter == vk::VK_FILTER_LINEAR)	? calculateLinearIJRange(coordFormat, uv)
											: calculateNearestIJRange(coordFormat, uv);
}

tcu::Interval calculateAB (const deUint32		subTexelPrecisionBits,
						   const tcu::Interval&	uv,
						   int					ij)
{
	const deUint32		subdivisions	= 0x1u << subTexelPrecisionBits;
	const tcu::Interval	ab				(frac((uv - 0.5) & tcu::Interval((double)ij, (double)(ij + 1))));
	const tcu::Interval	gridAB			(ab * tcu::Interval(subdivisions));
	const tcu::Interval	rounded			(de::max(deFloor(gridAB.lo()) / subdivisions, 0.0) , de::min(deCeil(gridAB.hi()) / subdivisions, 1.0));

	return rounded;
}

tcu::Interval lookupWrapped (const ChannelAccess&		access,
							 const tcu::FloatFormat&	conversionFormat,
							 vk::VkSamplerAddressMode	addressModeU,
							 vk::VkSamplerAddressMode	addressModeV,
							 const tcu::IVec2&			coord)
{
	return access.getChannel(conversionFormat,
							 tcu::IVec3(wrap(addressModeU, coord.x(), access.getSize().x()), wrap(addressModeV, coord.y(), access.getSize().y()), 0));
}

tcu::Interval linearInterpolate (const tcu::FloatFormat&	filteringFormat,
								 const tcu::Interval&		a,
								 const tcu::Interval&		b,
								 const tcu::Interval&		p00,
								 const tcu::Interval&		p10,
								 const tcu::Interval&		p01,
								 const tcu::Interval&		p11)
{
	const tcu::Interval	p[4] =
	{
		p00,
		p10,
		p01,
		p11
	};
	tcu::Interval		result	(0.0);

	for (size_t ndx = 0; ndx < 4; ndx++)
	{
		const tcu::Interval	weightA	(filteringFormat.roundOut((ndx % 2) == 0 ? (1.0 - a) : a, false));
		const tcu::Interval	weightB	(filteringFormat.roundOut((ndx / 2) == 0 ? (1.0 - b) : b, false));
		const tcu::Interval	weight	(filteringFormat.roundOut(weightA * weightB, false));

		result = filteringFormat.roundOut(result + filteringFormat.roundOut(p[ndx] * weight, false), false);
	}

	return result;
}

tcu::Interval calculateImplicitChromaUV (const tcu::FloatFormat&	coordFormat,
										 vk::VkChromaLocation		offset,
										 const tcu::Interval&		uv)
{
	if (offset == vk::VK_CHROMA_LOCATION_COSITED_EVEN)
		return coordFormat.roundOut(0.5 * coordFormat.roundOut(uv + 0.5, false), false);
	else
		return coordFormat.roundOut(0.5 * uv, false);
}

tcu::Interval linearSample (const ChannelAccess&		access,
						    const tcu::FloatFormat&		conversionFormat,
						    const tcu::FloatFormat&		filteringFormat,
						    vk::VkSamplerAddressMode	addressModeU,
						    vk::VkSamplerAddressMode	addressModeV,
						    const tcu::IVec2&			coord,
						    const tcu::Interval&		a,
						    const tcu::Interval&		b)
{
	return linearInterpolate(filteringFormat, a, b,
									lookupWrapped(access, conversionFormat, addressModeU, addressModeV, coord + tcu::IVec2(0, 0)),
									lookupWrapped(access, conversionFormat, addressModeU, addressModeV, coord + tcu::IVec2(1, 0)),
									lookupWrapped(access, conversionFormat, addressModeU, addressModeV, coord + tcu::IVec2(0, 1)),
									lookupWrapped(access, conversionFormat, addressModeU, addressModeV, coord + tcu::IVec2(1, 1)));
}

tcu::Interval reconstructLinearXChromaSample (const tcu::FloatFormat&	filteringFormat,
											  const tcu::FloatFormat&	conversionFormat,
											  vk::VkChromaLocation		offset,
											  vk::VkSamplerAddressMode	addressModeU,
											  vk::VkSamplerAddressMode	addressModeV,
											  const ChannelAccess&		access,
											  int						i,
											  int						j)
{
	const int subI	= offset == vk::VK_CHROMA_LOCATION_COSITED_EVEN
					? divFloor(i, 2)
					: (i % 2 == 0 ? divFloor(i, 2) - 1 : divFloor(i, 2));
	const double a	= offset == vk::VK_CHROMA_LOCATION_COSITED_EVEN
					? (i % 2 == 0 ? 0.0 : 0.5)
					: (i % 2 == 0 ? 0.25 : 0.75);

	const tcu::Interval A (filteringFormat.roundOut(       a  * lookupWrapped(access, conversionFormat, addressModeU, addressModeV, tcu::IVec2(subI, j)), false));
	const tcu::Interval B (filteringFormat.roundOut((1.0 - a) * lookupWrapped(access, conversionFormat, addressModeU, addressModeV, tcu::IVec2(subI + 1, j)), false));
	return filteringFormat.roundOut(A + B, false);
}

tcu::Interval reconstructLinearXYChromaSample (const tcu::FloatFormat&	filteringFormat,
										  const tcu::FloatFormat&		conversionFormat,
										  vk::VkChromaLocation			xOffset,
										  vk::VkChromaLocation			yOffset,
										  vk::VkSamplerAddressMode		addressModeU,
										  vk::VkSamplerAddressMode		addressModeV,
										  const ChannelAccess&			access,
										  int							i,
										  int							j)
{
	const int		subI	= xOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN
							? divFloor(i, 2)
							: (i % 2 == 0 ? divFloor(i, 2) - 1 : divFloor(i, 2));
	const int		subJ	= yOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN
							? divFloor(j, 2)
							: (j % 2 == 0 ? divFloor(j, 2) - 1 : divFloor(j, 2));

	const double	a		= xOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN
							? (i % 2 == 0 ? 0.0 : 0.5)
							: (i % 2 == 0 ? 0.25 : 0.75);
	const double	b		= yOffset == vk::VK_CHROMA_LOCATION_COSITED_EVEN
							? (j % 2 == 0 ? 0.0 : 0.5)
							: (j % 2 == 0 ? 0.25 : 0.75);

	return linearSample(access, conversionFormat, filteringFormat, addressModeU, addressModeV, tcu::IVec2(subI, subJ), a, b);
}

const ChannelAccess& swizzle (vk::VkComponentSwizzle	swizzle,
							  const ChannelAccess&		identityPlane,
							  const ChannelAccess&		rPlane,
							  const ChannelAccess&		gPlane,
							  const ChannelAccess&		bPlane,
							  const ChannelAccess&		aPlane)
{
	switch (swizzle)
	{
		case vk::VK_COMPONENT_SWIZZLE_IDENTITY:	return identityPlane;
		case vk::VK_COMPONENT_SWIZZLE_R:		return rPlane;
		case vk::VK_COMPONENT_SWIZZLE_G:		return gPlane;
		case vk::VK_COMPONENT_SWIZZLE_B:		return bPlane;
		case vk::VK_COMPONENT_SWIZZLE_A:		return aPlane;

		default:
			DE_FATAL("Unsupported swizzle");
			return identityPlane;
	}
}

} // anonymous

int wrap (vk::VkSamplerAddressMode	addressMode,
		  int						coord,
		  int						size)
{
	switch (addressMode)
	{
		case vk::VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			return (size - 1) - mirror(imod(coord, 2 * size) - size);

		case vk::VK_SAMPLER_ADDRESS_MODE_REPEAT:
			return imod(coord, size);

		case vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			return de::clamp(coord, 0, size - 1);

		case vk::VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
			return de::clamp(mirror(coord), 0, size - 1);

		default:
			DE_FATAL("Unknown wrap mode");
			return ~0;
	}
}

int divFloor (int a, int b)
{
	if (a % b == 0)
		return a / b;
	else if (a > 0)
		return a / b;
	else
		return (a / b) - 1;
}

void calculateBounds (const ChannelAccess&				rPlane,
					  const ChannelAccess&				gPlane,
					  const ChannelAccess&				bPlane,
					  const ChannelAccess&				aPlane,
					  const UVec4&						bitDepth,
					  const vector<Vec2>&				sts,
					  const vector<FloatFormat>&		filteringFormat,
					  const vector<FloatFormat>&		conversionFormat,
					  const deUint32					subTexelPrecisionBits,
					  vk::VkFilter						filter,
					  vk::VkSamplerYcbcrModelConversion	colorModel,
					  vk::VkSamplerYcbcrRange			range,
					  vk::VkFilter						chromaFilter,
					  vk::VkChromaLocation				xChromaOffset,
					  vk::VkChromaLocation				yChromaOffset,
					  const vk::VkComponentMapping&		componentMapping,
					  bool								explicitReconstruction,
					  vk::VkSamplerAddressMode			addressModeU,
					  vk::VkSamplerAddressMode			addressModeV,
					  std::vector<Vec4>&				minBounds,
					  std::vector<Vec4>&				maxBounds,
					  std::vector<Vec4>&				uvBounds,
					  std::vector<IVec4>&				ijBounds)
{
	const FloatFormat		highp			(-126, 127, 23, true,
											 tcu::MAYBE,	// subnormals
											 tcu::YES,		// infinities
											 tcu::MAYBE);	// NaN
	const FloatFormat		coordFormat		(-32, 32, 16, true);
	const ChannelAccess&	rAccess			(swizzle(componentMapping.r, rPlane, rPlane, gPlane, bPlane, aPlane));
	const ChannelAccess&	gAccess			(swizzle(componentMapping.g, gPlane, rPlane, gPlane, bPlane, aPlane));
	const ChannelAccess&	bAccess			(swizzle(componentMapping.b, bPlane, rPlane, gPlane, bPlane, aPlane));
	const ChannelAccess&	aAccess			(swizzle(componentMapping.a, aPlane, rPlane, gPlane, bPlane, aPlane));

	const bool				subsampledX		= gAccess.getSize().x() > rAccess.getSize().x();
	const bool				subsampledY		= gAccess.getSize().y() > rAccess.getSize().y();

	minBounds.resize(sts.size(), Vec4(TCU_INFINITY));
	maxBounds.resize(sts.size(), Vec4(-TCU_INFINITY));

	uvBounds.resize(sts.size(), Vec4(TCU_INFINITY, -TCU_INFINITY, TCU_INFINITY, -TCU_INFINITY));
	ijBounds.resize(sts.size(), IVec4(0x7FFFFFFF, -1 -0x7FFFFFFF, 0x7FFFFFFF, -1 -0x7FFFFFFF));

	// Chroma plane sizes must match
	DE_ASSERT(rAccess.getSize() == bAccess.getSize());

	// Luma plane sizes must match
	DE_ASSERT(gAccess.getSize() == aAccess.getSize());

	// Luma plane size must match chroma plane or be twice as big
	DE_ASSERT(rAccess.getSize().x() == gAccess.getSize().x() || 2 * rAccess.getSize().x() == gAccess.getSize().x());
	DE_ASSERT(rAccess.getSize().y() == gAccess.getSize().y() || 2 * rAccess.getSize().y() == gAccess.getSize().y());

	DE_ASSERT(filter == vk::VK_FILTER_NEAREST || filter == vk::VK_FILTER_LINEAR);
	DE_ASSERT(chromaFilter == vk::VK_FILTER_NEAREST || chromaFilter == vk::VK_FILTER_LINEAR);
	DE_ASSERT(subsampledX || !subsampledY);


	for (size_t ndx = 0; ndx < sts.size(); ndx++)
	{
		const Vec2	st		(sts[ndx]);
		Interval	bounds[4];

		const Interval	u	(calculateUV(coordFormat, st[0], gAccess.getSize().x()));
		const Interval	v	(calculateUV(coordFormat, st[1], gAccess.getSize().y()));

		uvBounds[ndx][0] = (float)u.lo();
		uvBounds[ndx][1] = (float)u.hi();

		uvBounds[ndx][2] = (float)v.lo();
		uvBounds[ndx][3] = (float)v.hi();

		const IVec2	iRange	(calculateIJRange(filter, coordFormat, u));
		const IVec2	jRange	(calculateIJRange(filter, coordFormat, v));

		ijBounds[ndx][0] = iRange[0];
		ijBounds[ndx][1] = iRange[1];

		ijBounds[ndx][2] = jRange[0];
		ijBounds[ndx][3] = jRange[1];

		for (int j = jRange.x(); j <= jRange.y(); j++)
		for (int i = iRange.x(); i <= iRange.y(); i++)
		{
			if (filter == vk::VK_FILTER_NEAREST)
			{
				const Interval	gValue	(lookupWrapped(gAccess, conversionFormat[1], addressModeU, addressModeV, IVec2(i, j)));
				const Interval	aValue	(lookupWrapped(aAccess, conversionFormat[3], addressModeU, addressModeV, IVec2(i, j)));

				if (explicitReconstruction || !(subsampledX || subsampledY))
				{
					Interval rValue, bValue;
					if (chromaFilter == vk::VK_FILTER_NEAREST || !subsampledX)
					{
						// Reconstruct using nearest if needed, otherwise, just take what's already there.
						const int subI = subsampledX ? i / 2 : i;
						const int subJ = subsampledY ? j / 2 : j;
						rValue = lookupWrapped(rAccess, conversionFormat[0], addressModeU, addressModeV, IVec2(subI, subJ));
						bValue = lookupWrapped(bAccess, conversionFormat[2], addressModeU, addressModeV, IVec2(subI, subJ));
					}
					else // vk::VK_FILTER_LINEAR
					{
						if (subsampledY)
						{
							rValue = reconstructLinearXYChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, yChromaOffset, addressModeU, addressModeV, rAccess, i, j);
							bValue = reconstructLinearXYChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, yChromaOffset, addressModeU, addressModeV, bAccess, i, j);
						}
						else
						{
							rValue = reconstructLinearXChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, addressModeU, addressModeV, rAccess, i, j);
							bValue = reconstructLinearXChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, addressModeU, addressModeV, bAccess, i, j);
						}
					}

					const Interval srcColor[] =
					{
						rValue,
						gValue,
						bValue,
						aValue
					};
					Interval dstColor[4];

					convertColor(colorModel, range, conversionFormat, bitDepth, srcColor, dstColor);

					for (size_t compNdx = 0; compNdx < 4; compNdx++)
						bounds[compNdx] |= highp.roundOut(dstColor[compNdx], false);
				}
				else
				{
					const Interval	chromaU	(subsampledX ? calculateImplicitChromaUV(coordFormat, xChromaOffset, u) : u);
					const Interval	chromaV	(subsampledY ? calculateImplicitChromaUV(coordFormat, yChromaOffset, v) : v);

					// Reconstructed chroma samples with implicit filtering
					const IVec2	chromaIRange	(subsampledX ? calculateIJRange(chromaFilter, coordFormat, chromaU) : IVec2(i, i));
					const IVec2	chromaJRange	(subsampledY ? calculateIJRange(chromaFilter, coordFormat, chromaV) : IVec2(j, j));

					for (int chromaJ = chromaJRange.x(); chromaJ <= chromaJRange.y(); chromaJ++)
					for (int chromaI = chromaIRange.x(); chromaI <= chromaIRange.y(); chromaI++)
					{
						Interval rValue, bValue;

						if (chromaFilter == vk::VK_FILTER_NEAREST)
						{
							rValue = lookupWrapped(rAccess, conversionFormat[0], addressModeU, addressModeV, IVec2(chromaI, chromaJ));
							bValue = lookupWrapped(bAccess, conversionFormat[2], addressModeU, addressModeV, IVec2(chromaI, chromaJ));
						}
						else // vk::VK_FILTER_LINEAR
						{
							const Interval	chromaA	(calculateAB(subTexelPrecisionBits, chromaU, chromaI));
							const Interval	chromaB	(calculateAB(subTexelPrecisionBits, chromaV, chromaJ));

							rValue = linearSample(rAccess, conversionFormat[0], filteringFormat[0], addressModeU, addressModeV, IVec2(chromaI, chromaJ), chromaA, chromaB);
							bValue = linearSample(bAccess, conversionFormat[2], filteringFormat[2], addressModeU, addressModeV, IVec2(chromaI, chromaJ), chromaA, chromaB);
						}

						const Interval	srcColor[]	=
						{
							rValue,
							gValue,
							bValue,
							aValue
						};

						Interval dstColor[4];
						convertColor(colorModel, range, conversionFormat, bitDepth, srcColor, dstColor);

						for (size_t compNdx = 0; compNdx < 4; compNdx++)
							bounds[compNdx] |= highp.roundOut(dstColor[compNdx], false);
					}
				}
			}
			else // filter == vk::VK_FILTER_LINEAR
			{
				const Interval	lumaA		(calculateAB(subTexelPrecisionBits, u, i));
				const Interval	lumaB		(calculateAB(subTexelPrecisionBits, v, j));

				const Interval	gValue		(linearSample(gAccess, conversionFormat[1], filteringFormat[1], addressModeU, addressModeV, IVec2(i, j), lumaA, lumaB));
				const Interval	aValue		(linearSample(aAccess, conversionFormat[3], filteringFormat[3], addressModeU, addressModeV, IVec2(i, j), lumaA, lumaB));

				if (explicitReconstruction || !(subsampledX || subsampledY))
				{
					Interval rValue, bValue;
					if (chromaFilter == vk::VK_FILTER_NEAREST || !subsampledX)
					{
						rValue = linearInterpolate(filteringFormat[0], lumaA, lumaB,
													lookupWrapped(rAccess, conversionFormat[0], addressModeU, addressModeV, IVec2(i       / (subsampledX ? 2 : 1), j       / (subsampledY ? 2 : 1))),
													lookupWrapped(rAccess, conversionFormat[0], addressModeU, addressModeV, IVec2((i + 1) / (subsampledX ? 2 : 1), j       / (subsampledY ? 2 : 1))),
													lookupWrapped(rAccess, conversionFormat[0], addressModeU, addressModeV, IVec2(i       / (subsampledX ? 2 : 1), (j + 1) / (subsampledY ? 2 : 1))),
													lookupWrapped(rAccess, conversionFormat[0], addressModeU, addressModeV, IVec2((i + 1) / (subsampledX ? 2 : 1), (j + 1) / (subsampledY ? 2 : 1))));
						bValue = linearInterpolate(filteringFormat[2], lumaA, lumaB,
													lookupWrapped(bAccess, conversionFormat[2], addressModeU, addressModeV, IVec2(i       / (subsampledX ? 2 : 1), j       / (subsampledY ? 2 : 1))),
													lookupWrapped(bAccess, conversionFormat[2], addressModeU, addressModeV, IVec2((i + 1) / (subsampledX ? 2 : 1), j       / (subsampledY ? 2 : 1))),
													lookupWrapped(bAccess, conversionFormat[2], addressModeU, addressModeV, IVec2(i       / (subsampledX ? 2 : 1), (j + 1) / (subsampledY ? 2 : 1))),
													lookupWrapped(bAccess, conversionFormat[2], addressModeU, addressModeV, IVec2((i + 1) / (subsampledX ? 2 : 1), (j + 1) / (subsampledY ? 2 : 1))));
					}
					else // vk::VK_FILTER_LINEAR
					{
						if (subsampledY)
						{
							// Linear, Reconstructed xx chroma samples with explicit linear filtering
							rValue = linearInterpolate(filteringFormat[0], lumaA, lumaB,
														reconstructLinearXYChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, yChromaOffset, addressModeU, addressModeV, rAccess, i, j),
														reconstructLinearXYChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, yChromaOffset, addressModeU, addressModeV, rAccess, i + 1, j),
														reconstructLinearXYChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, yChromaOffset, addressModeU, addressModeV, rAccess, i , j + 1),
														reconstructLinearXYChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, yChromaOffset, addressModeU, addressModeV, rAccess, i + 1, j + 1));
							bValue = linearInterpolate(filteringFormat[2], lumaA, lumaB,
														reconstructLinearXYChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, yChromaOffset, addressModeU, addressModeV, bAccess, i, j),
														reconstructLinearXYChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, yChromaOffset, addressModeU, addressModeV, bAccess, i + 1, j),
														reconstructLinearXYChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, yChromaOffset, addressModeU, addressModeV, bAccess, i , j + 1),
														reconstructLinearXYChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, yChromaOffset, addressModeU, addressModeV, bAccess, i + 1, j + 1));
						}
						else
						{
							// Linear, Reconstructed x chroma samples with explicit linear filtering
							rValue = linearInterpolate(filteringFormat[0], lumaA, lumaB,
														reconstructLinearXChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, addressModeU, addressModeV, rAccess, i, j),
														reconstructLinearXChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, addressModeU, addressModeV, rAccess, i + 1, j),
														reconstructLinearXChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, addressModeU, addressModeV, rAccess, i , j + 1),
														reconstructLinearXChromaSample(filteringFormat[0], conversionFormat[0], xChromaOffset, addressModeU, addressModeV, rAccess, i + 1, j + 1));
							bValue = linearInterpolate(filteringFormat[2], lumaA, lumaB,
														reconstructLinearXChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, addressModeU, addressModeV, bAccess, i, j),
														reconstructLinearXChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, addressModeU, addressModeV, bAccess, i + 1, j),
														reconstructLinearXChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, addressModeU, addressModeV, bAccess, i , j + 1),
														reconstructLinearXChromaSample(filteringFormat[2], conversionFormat[2], xChromaOffset, addressModeU, addressModeV, bAccess, i + 1, j + 1));
						}
					}

					const Interval	srcColor[]	=
					{
						rValue,
						gValue,
						bValue,
						aValue
					};
					Interval dstColor[4];

					convertColor(colorModel, range, conversionFormat, bitDepth, srcColor, dstColor);

					for (size_t compNdx = 0; compNdx < 4; compNdx++)
						bounds[compNdx] |= highp.roundOut(dstColor[compNdx], false);
				}
				else
				{
					const Interval	chromaU	(subsampledX ? calculateImplicitChromaUV(coordFormat, xChromaOffset, u) : u);
					const Interval	chromaV	(subsampledY ? calculateImplicitChromaUV(coordFormat, yChromaOffset, v) : v);

					// TODO: It looks incorrect to ignore the chroma filter here. Is it?
					const IVec2	chromaIRange	(calculateNearestIJRange(coordFormat, chromaU));
					const IVec2	chromaJRange	(calculateNearestIJRange(coordFormat, chromaV));

					for (int chromaJ = chromaJRange.x(); chromaJ <= chromaJRange.y(); chromaJ++)
					for (int chromaI = chromaIRange.x(); chromaI <= chromaIRange.y(); chromaI++)
					{
						Interval rValue, bValue;

						if (chromaFilter == vk::VK_FILTER_NEAREST)
						{
							rValue = lookupWrapped(rAccess, conversionFormat[1], addressModeU, addressModeV, IVec2(chromaI, chromaJ));
							bValue = lookupWrapped(bAccess, conversionFormat[3], addressModeU, addressModeV, IVec2(chromaI, chromaJ));
						}
						else // vk::VK_FILTER_LINEAR
						{
							const Interval	chromaA	(calculateAB(subTexelPrecisionBits, chromaU, chromaI));
							const Interval	chromaB	(calculateAB(subTexelPrecisionBits, chromaV, chromaJ));

							rValue = linearSample(rAccess, conversionFormat[0], filteringFormat[0], addressModeU, addressModeV, IVec2(chromaI, chromaJ), chromaA, chromaB);
							bValue = linearSample(bAccess, conversionFormat[2], filteringFormat[2], addressModeU, addressModeV, IVec2(chromaI, chromaJ), chromaA, chromaB);
						}

						const Interval	srcColor[]	=
						{
							rValue,
							gValue,
							bValue,
							aValue
						};
						Interval dstColor[4];
						convertColor(colorModel, range, conversionFormat, bitDepth, srcColor, dstColor);

						for (size_t compNdx = 0; compNdx < 4; compNdx++)
							bounds[compNdx] |= highp.roundOut(dstColor[compNdx], false);
					}
				}
			}
		}

		minBounds[ndx] = Vec4((float)bounds[0].lo(), (float)bounds[1].lo(), (float)bounds[2].lo(), (float)bounds[3].lo());
		maxBounds[ndx] = Vec4((float)bounds[0].hi(), (float)bounds[1].hi(), (float)bounds[2].hi(), (float)bounds[3].hi());
	}
}

} // ycbcr

} // vkt
