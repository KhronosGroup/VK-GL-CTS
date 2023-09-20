/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation.
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
 * \file  vktImageSubresourceLayoutTests.cpp
 * \brief Tests for vkGetImageSubresourceLayout
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include "tcuTestLog.hpp"
#include "vktTestCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuCommandLine.hpp"

#include "deRandom.hpp"

#include <vector>
#include <sstream>
#include <limits>
#include <string>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

// Helper class to calculate buffer sizes and offsets for image mipmap levels.
class BufferLevels
{
public:
	struct Level
	{
		VkDeviceSize	offset;		// In bytes.
		VkDeviceSize	size;		// In bytes.
		VkExtent3D		dimensions;	// .depth will be the number of layers for 2D images and the depth for 3D images.
	};

					BufferLevels	(VkImageType type, VkFormat format, VkExtent3D levelZero, deUint32 maxLevels, VkImageAspectFlags aspects = 0u);
	VkDeviceSize	totalSize		() const;
	VkDeviceSize	pixelSize		() const;
	deUint32		numLevels		() const;
	const Level&	getLevel		(deUint32 level) const;

private:
	VkDeviceSize		m_pixelSize; // In bytes.
	std::vector<Level>	m_levels;
};

BufferLevels::BufferLevels (VkImageType type, VkFormat format, VkExtent3D levelZero, deUint32 maxLevels, VkImageAspectFlags aspects)
{
	DE_ASSERT(type == VK_IMAGE_TYPE_2D || type == VK_IMAGE_TYPE_3D);
	DE_ASSERT(maxLevels >= 1u);

	const auto		tcuFormat		= vk::mapVkFormat(format);
	const auto		maxLevelsSz		= static_cast<size_t>(maxLevels);

	VkDeviceSize	currentOffset	= 0ull;
	VkExtent3D		nextExtent		= levelZero;

	if (!aspects || (aspects & VK_IMAGE_ASPECT_COLOR_BIT))
	{
		m_pixelSize = static_cast<VkDeviceSize>(tcu::getPixelSize(tcuFormat));
	}
	else if (aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
	{
		const auto copyFormat = getDepthCopyFormat(format);
		m_pixelSize = static_cast<VkDeviceSize>(tcu::getPixelSize(copyFormat));
	}
	else if (aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
	{
		const auto copyFormat = getStencilCopyFormat(format);
		m_pixelSize = static_cast<VkDeviceSize>(tcu::getPixelSize(copyFormat));
	}
	else
		DE_ASSERT(false);

	while (m_levels.size() < maxLevelsSz)
	{
		Level level;

		level.offset		= currentOffset;
		level.size			= m_pixelSize * nextExtent.width * nextExtent.height * nextExtent.depth;
		level.dimensions	= nextExtent;

		m_levels.push_back(level);

		// This was the last available level.
		if (nextExtent.width == 1u && nextExtent.height == 1u && (type == VK_IMAGE_TYPE_2D || nextExtent.depth == 1u))
			break;

		nextExtent.width = std::max(1u, (nextExtent.width / 2u));
		nextExtent.height = std::max(1u, (nextExtent.height / 2u));

		// 2D arrays all have the same array size.
		if (type == VK_IMAGE_TYPE_3D)
			nextExtent.depth = std::max(1u, (nextExtent.depth / 2u));

		currentOffset += level.size;
	}
}

VkDeviceSize BufferLevels::totalSize () const
{
	VkDeviceSize total = 0ull;
	std::for_each(begin(m_levels), end(m_levels), [&total] (const Level& l) { total += l.size; });
	return total;
}

VkDeviceSize BufferLevels::pixelSize () const
{
	return m_pixelSize;
}

deUint32 BufferLevels::numLevels () const
{
	return static_cast<deUint32>(m_levels.size());
}

const BufferLevels::Level& BufferLevels::getLevel (deUint32 level) const
{
	return m_levels.at(level);
}

// Default image dimensions. For 2D images, .depth indicates the number of layers.
VkExtent3D getDefaultDimensions (VkImageType type, bool array)
{
	DE_ASSERT(type == VK_IMAGE_TYPE_2D || type == VK_IMAGE_TYPE_3D);
	DE_ASSERT(!array || type == VK_IMAGE_TYPE_2D);

	constexpr VkExtent3D kDefault3D			= { 32u, 48u, 56u };
	constexpr VkExtent3D kDefault2DArray	= kDefault3D;
	constexpr VkExtent3D kDefault2D			= { 240u, 320u, 1u };

	if (type == VK_IMAGE_TYPE_3D)
		return kDefault3D;
	if (array)
		return kDefault2DArray;
	return kDefault2D;
}

struct TestParams
{
	VkImageType	imageType;
	VkFormat	imageFormat;
	VkExtent3D	dimensions;		// .depth will be the number of layers for 2D images and the depth for 3D images.
	deUint32	mipLevels;
	bool		imageOffset;	// Add an offset when a region of memory is bound to an image.
};

class ImageSubresourceLayoutCase : public vkt::TestCase
{
public:
							ImageSubresourceLayoutCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~ImageSubresourceLayoutCase		(void) {}

	virtual void			initPrograms					(vk::SourceCollections&) const {}
	virtual TestInstance*	createInstance					(Context& context) const;
	virtual void			checkSupport					(Context& context) const;

	static constexpr VkFormatFeatureFlags	kRequiredFeatures	= (VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
	static constexpr VkImageUsageFlags		kImageUsageFlags	= (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	static constexpr VkImageTiling			kImageTiling		= VK_IMAGE_TILING_LINEAR;
protected:
	TestParams m_params;
};

class ImageSubresourceLayoutInstance : public vkt::TestInstance
{
public:
								ImageSubresourceLayoutInstance	(Context& context, const TestParams& params);
	virtual						~ImageSubresourceLayoutInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);
	tcu::TestStatus				iterateAspect					(VkImageAspectFlagBits aspect);
private:
	TestParams m_params;
};

ImageSubresourceLayoutCase::ImageSubresourceLayoutCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{
}

TestInstance* ImageSubresourceLayoutCase::createInstance (Context& context) const
{
	return new ImageSubresourceLayoutInstance (context, m_params);
}

void ImageSubresourceLayoutCase::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

#ifndef CTS_USES_VULKANSC
	if (m_params.imageFormat == VK_FORMAT_A8_UNORM_KHR || m_params.imageFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

	const auto formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, m_params.imageFormat);
	if ((formatProperties.linearTilingFeatures & kRequiredFeatures) != kRequiredFeatures)
		TCU_THROW(NotSupportedError, "Required format features not supported");

	VkImageFormatProperties imgFormatProperties;
	const auto result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_params.imageFormat, m_params.imageType, kImageTiling, kImageUsageFlags, 0u, &imgFormatProperties);
	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Linear tiling not supported for format");
	VK_CHECK(result);

	{
		BufferLevels levels (m_params.imageType, m_params.imageFormat, m_params.dimensions, m_params.mipLevels);
		if (imgFormatProperties.maxMipLevels < levels.numLevels())
			TCU_THROW(NotSupportedError, "Required number of mip levels not supported for format");
	}

	if (m_params.imageType == VK_IMAGE_TYPE_2D && imgFormatProperties.maxArrayLayers < m_params.dimensions.depth)
		TCU_THROW(NotSupportedError, "Required number of layers not supported for format");
}

ImageSubresourceLayoutInstance::ImageSubresourceLayoutInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{
}

// Fills length bytes starting at location with pseudorandom data.
void fillWithRandomData (de::Random& rnd, void* location, VkDeviceSize length)
{
	auto		bytePtr	= reinterpret_cast<unsigned char*>(location);
	const auto	endPtr	= bytePtr + length;

	while (bytePtr != endPtr)
	{
		const auto remaining = (endPtr - bytePtr);

		if (remaining >= 8)			{ const auto data = rnd.getUint64();	deMemcpy(bytePtr, &data, sizeof(data)); bytePtr += sizeof(data); }
		else if (remaining >= 4)	{ const auto data = rnd.getUint32();	deMemcpy(bytePtr, &data, sizeof(data)); bytePtr += sizeof(data); }
		else if (remaining >= 2)	{ const auto data = rnd.getUint16();	deMemcpy(bytePtr, &data, sizeof(data)); bytePtr += sizeof(data); }
		else						{ const auto data = rnd.getUint8();		deMemcpy(bytePtr, &data, sizeof(data)); bytePtr += sizeof(data); }
	}
}

// Fills data in blocks of 32 bits, discarding the higher 8 bits of each block.
void fillWithRandomData24In32 (de::Random& rnd, void* location, VkDeviceSize length)
{
	static const auto blockSize = sizeof(deUint32);
	DE_ASSERT(length % blockSize == 0);

	auto		dataPtr		= reinterpret_cast<unsigned char*>(location);
	const auto	numBlocks	= length / blockSize;

	for (VkDeviceSize i = 0; i < numBlocks; ++i)
	{
		auto data = rnd.getUint32();
		data &= 0xFFFFFFu; // Remove the higher 8 bits.
		deMemcpy(dataPtr, &data, blockSize);
		dataPtr += blockSize;
	}
}

// Helpers to make fillWithRandomFloatingPoint a template. Returns normal numbers in the range [0, 1).
template <class T>
T getNormalFPValue (de::Random& rnd);

template<>
float getNormalFPValue<float> (de::Random& rnd)
{
	float value;
	do {
		value = rnd.getFloat();
	} while (tcu::Float32(value).isDenorm());
	return value;
}

template<>
double getNormalFPValue<double> (de::Random& rnd)
{
	double value;
	do {
		value = rnd.getDouble();
	} while (tcu::Float64(value).isDenorm());
	return value;
}

template<>
tcu::Float16 getNormalFPValue<tcu::Float16> (de::Random& rnd)
{
	tcu::Float16 value;
	do {
		value = tcu::Float16(rnd.getFloat());
	} while (value.isDenorm());
	return value;
}

template <class T>
void fillWithRandomFloatingPoint (de::Random& rnd, void* location, VkDeviceSize length)
{
	static const auto typeSize = sizeof(T);

	DE_ASSERT(length % typeSize == 0);

	const auto	numElements	= length / typeSize;
	auto		elemPtr		= reinterpret_cast<unsigned char*>(location);
	T			elem;

	for (VkDeviceSize i = 0; i < numElements; ++i)
	{
		elem = getNormalFPValue<T>(rnd);
		deMemcpy(elemPtr, &elem, typeSize);
		elemPtr += typeSize;
	}
}

tcu::TestStatus ImageSubresourceLayoutInstance::iterate (void)
{
	// Test every aspect supported by the image format.
	const auto tcuFormat	= mapVkFormat(m_params.imageFormat);
	const auto aspectFlags	= getImageAspectFlags(tcuFormat);

	static const VkImageAspectFlagBits aspectBits[] =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_ASPECT_STENCIL_BIT,
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(aspectBits); ++i)
	{
		const auto bit = aspectBits[i];
		if (aspectFlags & bit)
		{
			const auto aspectResult = iterateAspect(bit);
			if (aspectResult.getCode() != QP_TEST_RESULT_PASS)
				return aspectResult; // Early return for failures.
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ImageSubresourceLayoutInstance::iterateAspect (VkImageAspectFlagBits imageAspect)
{
	// * Create linear image with several mipmaps
	// * Fill its levels with unique appropriate data (avoiding invalid sfloat values, for example).
	// * Ask for the subresource layout parameters.
	// * Verify they make sense.
	// * Check accessing data with the given parameters gives back the original data.

	const auto&	vkd					= m_context.getDeviceInterface();
	const auto	device				= m_context.getDevice();
	auto&		alloc				= m_context.getDefaultAllocator();
	const auto	queue				= m_context.getUniversalQueue();
	const auto	queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	auto&		log					= m_context.getTestContext().getLog();

	log << tcu::TestLog::Message << "Testing aspect " << imageAspect << tcu::TestLog::EndMessage;

	// Get an idea of the buffer size and parameters to prepare image data.
	const BufferLevels	bufferLevels	(m_params.imageType, m_params.imageFormat, m_params.dimensions, m_params.mipLevels, imageAspect);
	const auto			pixelSize		= bufferLevels.pixelSize();
	const auto			pixelSizeSz		= static_cast<size_t>(pixelSize);
	const auto			numLevels		= bufferLevels.numLevels();

	// Create source buffer.
	const auto			bufferSize	= bufferLevels.totalSize();
	const auto			bufferInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	BufferWithMemory	buffer		(vkd, device, alloc, bufferInfo, MemoryRequirement::HostVisible);
	auto&				bufferAlloc	= buffer.getAllocation();
	auto*				bufferPtr	= reinterpret_cast<unsigned char*>(bufferAlloc.getHostPtr());

	// Fill buffer with random appropriate data.
	const deUint32	randomSeed	= 1594055758u + static_cast<deUint32>(m_params.imageFormat) + static_cast<deUint32>(imageAspect);
	de::Random		rnd			(randomSeed);
	const auto		tcuFormat	= mapVkFormat(m_params.imageFormat);
	// For some formats, the copy block is 32 bits wide but the 8 MSB need to be ignored, so we zero them out.
	const bool		use24LSB	= ((m_params.imageFormat == VK_FORMAT_X8_D24_UNORM_PACK32 || m_params.imageFormat == VK_FORMAT_D24_UNORM_S8_UINT) && imageAspect == VK_IMAGE_ASPECT_DEPTH_BIT);

	if (tcuFormat.type == tcu::TextureFormat::FLOAT || (m_params.imageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT && imageAspect == VK_IMAGE_ASPECT_DEPTH_BIT))
		fillWithRandomFloatingPoint<float>(rnd, bufferPtr, bufferSize);
	else if (tcuFormat.type == tcu::TextureFormat::FLOAT64)
		fillWithRandomFloatingPoint<double>(rnd, bufferPtr, bufferSize);
	else if (tcuFormat.type == tcu::TextureFormat::HALF_FLOAT)
		fillWithRandomFloatingPoint<tcu::Float16>(rnd, bufferPtr, bufferSize);
	else if (use24LSB)
		fillWithRandomData24In32(rnd, bufferPtr, bufferSize);
	else
		fillWithRandomData(rnd, bufferPtr, bufferSize);

	flushAlloc(vkd, device, bufferAlloc);

	// Reinterpret the depth dimension parameter as the number of layers if needed.
	const auto	numLayers	= ((m_params.imageType == VK_IMAGE_TYPE_3D) ? 1u : m_params.dimensions.depth);
	VkExtent3D	imageExtent	= m_params.dimensions;
	if (m_params.imageType == VK_IMAGE_TYPE_2D)
		imageExtent.depth = 1u;

	// Create image.
	const VkImageCreateInfo imageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			//	VkStructureType			sType;
		nullptr,										//	const void*				pNext;
		0u,												//	VkImageCreateFlags		flags;
		m_params.imageType,								//	VkImageType				imageType;
		m_params.imageFormat,							//	VkFormat				format;
		imageExtent,									//	VkExtent3D				extent;
		numLevels,										//	deUint32				mipLevels;
		numLayers,										//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							//	VkSampleCountFlagBits	samples;
		ImageSubresourceLayoutCase::kImageTiling,		//	VkImageTiling			tiling;
		ImageSubresourceLayoutCase::kImageUsageFlags,	//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						//	VkSharingMode			sharingMode;
		0u,												//	deUint32				queueFamilyIndexCount;
		nullptr,										//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						//	VkImageLayout			initialLayout;
	};

	Move<VkImage>				image = createImage(vkd, device, &imageInfo);
	VkMemoryRequirements		req = getImageMemoryRequirements(vkd, device, *image);
	if (m_params.imageOffset)
		req.size += req.alignment;

	Allocator&									allocator				= m_context.getDefaultAllocator();
	de::MovePtr<Allocation>		imageAlloc	= allocator.allocate(req, MemoryRequirement::HostVisible);

	VK_CHECK(vkd.bindImageMemory(device, *image, imageAlloc->getMemory(), m_params.imageOffset ? req.alignment : 0u));
	auto*			imagePtr	= reinterpret_cast<unsigned char*>(imageAlloc->getHostPtr());

	// Copy regions.
	std::vector<VkBufferImageCopy> copyRegions;
	copyRegions.reserve(numLevels);

	for (deUint32 levelNdx = 0u; levelNdx < numLevels; ++levelNdx)
	{
		const auto&	level		= bufferLevels.getLevel(levelNdx);
		auto		levelExtent	= level.dimensions;

		if (m_params.imageType == VK_IMAGE_TYPE_2D)
			levelExtent.depth = 1u;	// For 2D images, .depth indicates the number of layers.

		VkBufferImageCopy region;
		region.bufferOffset						= level.offset;
		region.bufferRowLength					= 0u;	// Tightly packed data.
		region.bufferImageHeight				= 0u;	// Ditto.
		region.imageSubresource.aspectMask		= imageAspect;
		region.imageSubresource.baseArrayLayer	= 0u;
		region.imageSubresource.layerCount		= numLayers;
		region.imageSubresource.mipLevel		= levelNdx;
		region.imageOffset						= { 0, 0, 0 };
		region.imageExtent						= levelExtent;

		copyRegions.push_back(region);
	}

	// Image layout transitions.
	const auto imageSubresourceRange	= makeImageSubresourceRange(imageAspect, 0u, numLevels, 0u, numLayers);
	const auto initialLayoutBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.get(), imageSubresourceRange);
	const auto finalLayoutBarrier		= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, image.get(), imageSubresourceRange);

	// Command buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueFamilyIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	// Transition layout, copy, transition layout.
	beginCommandBuffer(vkd, cmdBuffer);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &initialLayoutBarrier);
	vkd.cmdCopyBufferToImage(cmdBuffer, buffer.get(), image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<deUint32>(copyRegions.size()), copyRegions.data());
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &finalLayoutBarrier);
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

#ifdef CTS_USES_VULKANSC
	if (!m_context.getTestContext().getCommandLine().isSubProcess())
		return tcu::TestStatus::pass("Pass");
#endif

	// Sync image memory for host access.
	invalidateAlloc(vkd, device, *imageAlloc);

	VkSubresourceLayout levelSubresourceLayout;
	VkSubresourceLayout subresourceLayout;
	for (deUint32 levelNdx = 0u; levelNdx < numLevels; ++levelNdx)
	{
		// Get base level subresource.
		const auto levelSubresource = makeImageSubresource(imageAspect, levelNdx, 0u);
		vkd.getImageSubresourceLayout(device, image.get(), &levelSubresource, &levelSubresourceLayout);

		const auto& level = bufferLevels.getLevel(levelNdx);
		for (deUint32 layerNdx = 0; layerNdx < numLayers; ++layerNdx)
		{
			const auto imageSubresource = makeImageSubresource(imageAspect, levelNdx, layerNdx);
			vkd.getImageSubresourceLayout(device, image.get(), &imageSubresource, &subresourceLayout);

			// Verify returned values.
			const auto subresourceWidth		= level.dimensions.width;
			const auto subresourceHeight	= level.dimensions.height;
			const auto subresourceDepth		= ((m_params.imageType == VK_IMAGE_TYPE_2D) ? 1u : level.dimensions.depth);
			const auto numPixels			= subresourceWidth * subresourceHeight * subresourceDepth;

			if (numLayers > 1u && levelSubresourceLayout.arrayPitch != subresourceLayout.arrayPitch)
			{
				// Inconsistent arrayPitch.
				std::ostringstream msg;
				msg << "Image level " << levelNdx
					<< " layer " << layerNdx
					<< " reports array pitch of " << subresourceLayout.arrayPitch << " bytes in size"
					<< " with base layer reporting array pitch of " << levelSubresourceLayout.arrayPitch << " bytes in size";
				return tcu::TestStatus::fail(msg.str());
			}

			if ((subresourceLayout.offset - levelSubresourceLayout.offset) != (layerNdx * subresourceLayout.arrayPitch))
			{
				// Inconsistent offset.
				std::ostringstream msg;
				msg << "Image level " << levelNdx
					<< " layer " << layerNdx
					<< " has offset inconsistent with array pitch: base offset " << levelSubresourceLayout.offset
					<< ", layer offset " << subresourceLayout.offset
					<< ", array pitch " << subresourceLayout.arrayPitch;
				return tcu::TestStatus::fail(msg.str());
			}

			if (subresourceLayout.size < pixelSize * numPixels)
			{
				// Subresource size too small.
				std::ostringstream msg;
				msg << "Image level " << levelNdx
					<< " layer " << layerNdx
					<< " reports " << subresourceLayout.size << " bytes in size"
					<< " with pixel size " << pixelSize
					<< " and dimensions " << subresourceWidth << "x" << subresourceHeight << "x" << subresourceDepth;
				return tcu::TestStatus::fail(msg.str());
			}

			// Note: if subresourceHeight is <= 1u, rowPitch can be zero.
			if (subresourceHeight > 1u && subresourceLayout.rowPitch < pixelSize * subresourceWidth)
			{
				// Row pitch too small.
				std::ostringstream msg;
				msg << "Image level " << levelNdx
					<< " layer " << layerNdx
					<< " reports row pitch of " << subresourceLayout.rowPitch
					<< " bytes with " << pixelSize
					<< " bytes in pixel size and width " << subresourceWidth;
				return tcu::TestStatus::fail(msg.str());
			}

			if (numLayers > 1u && subresourceLayout.arrayPitch < pixelSize * numPixels)
			{
				// Array pitch too small.
				std::ostringstream msg;
				msg << "Image level " << levelNdx
					<< " layer " << layerNdx
					<< " reports array pitch of " << subresourceLayout.arrayPitch
					<< " bytes with " << pixelSize
					<< " bytes in pixel size and layer dimensions " << subresourceWidth << "x" << subresourceHeight;
				return tcu::TestStatus::fail(msg.str());
			}

			// If subresourceDepth is <= 1u, depthPitch can be zero.
			if (subresourceDepth > 1u && m_params.imageType == VK_IMAGE_TYPE_3D && subresourceLayout.depthPitch < pixelSize * subresourceWidth * subresourceHeight)
			{
				// Depth pitch too small.
				std::ostringstream msg;
				msg << "Image level " << levelNdx
					<< " layer " << layerNdx
					<< " reports depth pitch of " << subresourceLayout.depthPitch << " bytes"
					<< " with pixel size " << pixelSize
					<< " and dimensions " << subresourceWidth << "x" << subresourceHeight << "x" << subresourceDepth;
				return tcu::TestStatus::fail(msg.str());
			}

			// Verify image data.
			const auto	layerBufferOffset	= level.offset + layerNdx * numPixels * pixelSize;
			const auto	layerImageOffset	= subresourceLayout.offset;
			const auto	layerBufferPtr		= bufferPtr + layerBufferOffset;
			const auto	layerImagePtr		= imagePtr + layerImageOffset + (m_params.imageOffset ? req.alignment : 0u);
			bool		pixelMatch;

			// We could do this row by row to be faster, but in the use24LSB case we need to manipulate pixels independently.
			for (deUint32 x = 0u; x < subresourceWidth; ++x)
			for (deUint32 y = 0u; y < subresourceHeight; ++y)
			for (deUint32 z = 0u; z < subresourceDepth; ++z)
			{
				const auto bufferPixelOffset	= (z * subresourceWidth * subresourceHeight + y * subresourceWidth + x) * pixelSize;
				const auto imagePixelOffset		= z * subresourceLayout.depthPitch + y * subresourceLayout.rowPitch + x * pixelSize;
				const auto bufferPixelPtr		= layerBufferPtr + bufferPixelOffset;
				const auto imagePixelPtr		= layerImagePtr + imagePixelOffset;

				if (use24LSB)
				{
					DE_ASSERT(pixelSize == sizeof(deUint32));
					deUint32 pixelValue;
					deMemcpy(&pixelValue, imagePixelPtr, pixelSizeSz);
					pixelValue &= 0xFFFFFFu; // Discard the 8 MSB.
					pixelMatch = (deMemCmp(bufferPixelPtr, &pixelValue, pixelSizeSz) == 0);
				}
				else
					pixelMatch = (deMemCmp(bufferPixelPtr, imagePixelPtr, pixelSizeSz) == 0);

				if (!pixelMatch)
				{
					std::ostringstream msg;
					msg << "Found difference from image pixel to buffer pixel at coordinates"
						<< " level=" << levelNdx
						<< " layer=" << layerNdx
						<< " x=" << x
						<< " y=" << y
						<< " z=" << z
						;
					return tcu::TestStatus::fail(msg.str());
				}
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class ImageSubresourceLayoutInvarianceInstance: public vkt::TestInstance
{
public:
								ImageSubresourceLayoutInvarianceInstance	(Context& context, const TestParams& params);
	virtual						~ImageSubresourceLayoutInvarianceInstance	(void) = default;

	virtual tcu::TestStatus		iterate										(void);

private:
	TestParams m_params;
};

ImageSubresourceLayoutInvarianceInstance::ImageSubresourceLayoutInvarianceInstance(Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{
}

tcu::TestStatus ImageSubresourceLayoutInvarianceInstance::iterate(void)
{
#ifndef CTS_USES_VULKANSC
	const VkDevice			device	= m_context.getDevice();
	const DeviceInterface&	vk		= m_context.getDeviceInterface();

	// Reinterpret the depth dimension parameter as the number of layers if needed
	const auto	numLayers = ((m_params.imageType == VK_IMAGE_TYPE_3D) ? 1u : m_params.dimensions.depth);
	VkExtent3D	imageExtent = m_params.dimensions;
	if (m_params.imageType == VK_IMAGE_TYPE_2D)
		imageExtent.depth = 1u;

	// Create image
	const VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			//	VkStructureType			sType;
		nullptr,										//	const void*				pNext;
		0u,												//	VkImageCreateFlags		flags;
		m_params.imageType,								//	VkImageType				imageType;
		m_params.imageFormat,							//	VkFormat				format;
		imageExtent,									//	VkExtent3D				extent;
		m_params.mipLevels,								//	deUint32				mipLevels;
		numLayers,										//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							//	VkSampleCountFlagBits	samples;
		ImageSubresourceLayoutCase::kImageTiling,		//	VkImageTiling			tiling;
		ImageSubresourceLayoutCase::kImageUsageFlags,	//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						//	VkSharingMode			sharingMode;
		0u,												//	deUint32				queueFamilyIndexCount;
		nullptr,										//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						//	VkImageLayout			initialLayout;
	};

	Move<VkImage> image				= createImage(vk, device, &imageCreateInfo);
	const auto tcuFormat			= mapVkFormat(m_params.imageFormat);
	const auto supportedAspectFlags	= getImageAspectFlags(tcuFormat);

	const std::vector<VkImageAspectFlagBits> testedAspectBits
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_ASPECT_STENCIL_BIT,
	};
	// test every aspect supported by the image format
	for (const auto aspectBit : testedAspectBits)
	{
		if ((supportedAspectFlags & aspectBit) == 0)
			continue;

		// get base level subresource using image handle
		VkSubresourceLayout	subresourceLayout1	= {};
		VkImageSubresource	imageSubresource1	= makeImageSubresource(aspectBit, 0u, 0u);
		vk.getImageSubresourceLayout(device, *image, &imageSubresource1, &subresourceLayout1);

		// get level subresource without using image handle
		VkImageSubresource2KHR				imageSubresource2		= initVulkanStructure();
		imageSubresource2.imageSubresource	= imageSubresource1;
		VkSubresourceLayout2KHR				subresourceLayout2      = initVulkanStructure();
		VkDeviceImageSubresourceInfoKHR		imageSubresourceInfo	= initVulkanStructure();
		imageSubresourceInfo.pCreateInfo	= &imageCreateInfo;
		imageSubresourceInfo.pSubresource	= &imageSubresource2;
		vk.getDeviceImageSubresourceLayoutKHR(device, &imageSubresourceInfo, &subresourceLayout2);

		const auto size = sizeof(VkSubresourceLayout);
		if (deMemCmp(&subresourceLayout1, &(subresourceLayout2.subresourceLayout), size))
			return tcu::TestStatus::fail("Fail (vkGetDeviceImageSubresourceLayoutKHR)");

		if (m_context.isDeviceFunctionalitySupported("VK_EXT_image_compression_control"))
		{
			VkSubresourceLayout2EXT subresourceLayout3 = initVulkanStructure();
			vk.getImageSubresourceLayout2KHR(device, *image, &imageSubresource2, &subresourceLayout3);

			if (deMemCmp(&subresourceLayout1, &(subresourceLayout3.subresourceLayout), size))
				return tcu::TestStatus::fail("Fail (vkGetImageSubresourceLayout2KHR)");
		}
	}
#else
	DE_UNREF(m_params);
#endif // CTS_USES_VULKANSC
	return tcu::TestStatus::pass("Pass");
}

class ImageSubresourceLayoutInvarianceCase: public ImageSubresourceLayoutCase
{
public:
							ImageSubresourceLayoutInvarianceCase	(tcu::TestContext& testCtx, const std::string& name, const TestParams& params);
	virtual					~ImageSubresourceLayoutInvarianceCase	(void) = default;

	virtual TestInstance*	createInstance							(Context& context) const;
	virtual void			checkSupport							(Context& context) const;
};

ImageSubresourceLayoutInvarianceCase::ImageSubresourceLayoutInvarianceCase(tcu::TestContext& testCtx, const std::string& name, const TestParams& params)
	: ImageSubresourceLayoutCase(testCtx, name, "", params)
{
}

TestInstance* ImageSubresourceLayoutInvarianceCase::createInstance(Context& context) const
{
	return new ImageSubresourceLayoutInvarianceInstance(context, m_params);
}

void ImageSubresourceLayoutInvarianceCase::checkSupport(Context& context) const
{
	ImageSubresourceLayoutCase::checkSupport(context);
	context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

} // anonymous namespace

tcu::TestCaseGroup* createImageSubresourceLayoutTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> layoutTestGroup (new tcu::TestCaseGroup(testCtx, "subresource_layout", "Tests for vkGetImageSubresourceLayout"));

	struct
	{
		VkImageType	type;
		bool		array;
		const char*	name;
		const char*	desc;
	} imageClass[] =
	{
		{ VK_IMAGE_TYPE_2D,	false,	"2d",		"2D images"							},
		{ VK_IMAGE_TYPE_2D,	true,	"2d_array",	"2D images with multiple layers"	},
		{ VK_IMAGE_TYPE_3D,	false,	"3d",		"3D images"							},
	};

	struct
	{
		deUint32	maxLevels;
		const char*	name;
		const char*	desc;
	} mipLevels[] =
	{
		{ 1u,									"1_level",		"Single mip level"		},
		{ 2u,									"2_levels",		"Two mip levels"		},
		{ 4u,									"4_levels",		"Four mip levels"		},
		{ std::numeric_limits<deUint32>::max(),	"all_levels",	"All possible levels"	},
	};

	VkFormat testFormats[] =
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
#ifndef CTS_USES_VULKANSC
		VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_UINT,
		VK_FORMAT_R64G64B64A64_SINT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
		// Leaving out depth/stencil formats due to this part of the spec:
		//
		// "Depth/stencil formats are considered opaque and need not be stored in the exact number of bits per texel or component
		// ordering indicated by the format enum. However, implementations must not substitute a different depth or stencil
		// precision than that described in the format (e.g. D16 must not be implemented as D24 or D32)."
		//
		// Which means the size of the texel is not known for depth/stencil formats and we cannot iterate over them to check their
		// values.
#if 0
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
#endif
	};

	static const auto prefixLen = std::string("VK_FORMAT_").size();
	for (int classIdx = 0; classIdx < DE_LENGTH_OF_ARRAY(imageClass); ++classIdx)
	{
		const auto& imgClass = imageClass[classIdx];
		de::MovePtr<tcu::TestCaseGroup> classGroup (new tcu::TestCaseGroup(testCtx, imgClass.name, imgClass.desc));

		for (int mipIdx = 0; mipIdx < DE_LENGTH_OF_ARRAY(mipLevels); ++mipIdx)
		{
			const auto &mipLevel = mipLevels[mipIdx];
			de::MovePtr<tcu::TestCaseGroup> mipGroup (new tcu::TestCaseGroup(testCtx, mipLevel.name, mipLevel.desc));

			for (int formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(testFormats); ++formatIdx)
			{
				const auto			format		= testFormats[formatIdx];
				const auto			fmtName		= std::string(getFormatName(format));
				const auto			name		= de::toLower(fmtName.substr(prefixLen)); // Remove VK_FORMAT_ prefix.
				const auto			desc		= "Using format " + fmtName;

				TestParams params;
				params.imageFormat	= format;
				params.imageType	= imgClass.type;
				params.mipLevels	= mipLevel.maxLevels;
				params.dimensions	= getDefaultDimensions(imgClass.type, imgClass.array);
				params.imageOffset	= false;

				mipGroup->addChild(new ImageSubresourceLayoutCase(testCtx, name, desc, params));

				params.imageOffset	= true;

				mipGroup->addChild(new ImageSubresourceLayoutCase(testCtx, name+"_offset", desc, params));
			}

			classGroup->addChild(mipGroup.release());
		}

		layoutTestGroup->addChild(classGroup.release());
	}

#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup> invarianceGroup(new tcu::TestCaseGroup(testCtx, "invariance", "Tests for vkGetDeviceImageSubresourceLayoutKHR"));

	TestParams params;
	params.imageOffset = false;
	params.mipLevels = 1;

	for (int formatIdx = 0 ; formatIdx < DE_LENGTH_OF_ARRAY(testFormats); ++formatIdx)
	for (int classIdx = 0 ; classIdx < DE_LENGTH_OF_ARRAY(imageClass); ++classIdx)
	{
		const auto& imgClass = imageClass[classIdx];

		params.imageFormat	= testFormats[formatIdx];
		params.imageType	= imgClass.type;
		params.dimensions	= getDefaultDimensions(imgClass.type, imgClass.array);
		params.dimensions.width += formatIdx;

		std::string name = getFormatName(params.imageFormat);
		name = de::toLower(name.substr(prefixLen)) + "_" + imgClass.name;

		invarianceGroup->addChild(new ImageSubresourceLayoutInvarianceCase(testCtx, name, params));
	}
	layoutTestGroup->addChild(invarianceGroup.release());
#endif // CTS_USES_VULKANSC

	return layoutTestGroup.release();
}

} // namespace image
} // namespace vkt
