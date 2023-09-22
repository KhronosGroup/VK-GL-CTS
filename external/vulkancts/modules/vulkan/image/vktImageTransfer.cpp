/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
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
 * \file  vktImageTransfer.cpp
 * \brief Tests for image transfers
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include "tcuTestLog.hpp"
#include "vktTestCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuCommandLine.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkRefUtil.hpp"
#include "deRandom.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"

#include <vector>
#include <string>

using namespace vk;

namespace vkt
{
using namespace ycbcr;
namespace image
{
namespace
{

class TransferQueueCase : public vkt::TestCase
{
public:
	struct TestParams
	{
		VkImageType	imageType;
		VkFormat	imageFormat;
		VkExtent3D	dimensions;		// .depth will be the number of layers for 2D images and the depth for 3D images.
	};

							TransferQueueCase				(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~TransferQueueCase				(void) {}

	virtual void			initPrograms					(vk::SourceCollections&) const {}
	virtual TestInstance*	createInstance					(Context& context) const;
	virtual void			checkSupport					(Context& context) const;
private:
	TestParams m_params;
};

class TransferQueueInstance : public vkt::TestInstance
{
public:
								TransferQueueInstance			(Context& context, const TransferQueueCase::TestParams& params);
	virtual						~TransferQueueInstance			(void) {}

	virtual tcu::TestStatus		iterate							(void);
private:
	TransferQueueCase::TestParams m_params;
	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
};

TransferQueueCase::TransferQueueCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{
}

TestInstance* TransferQueueCase::createInstance (Context& context) const
{
	return new TransferQueueInstance (context, m_params);
}

void TransferQueueCase::checkSupport (Context& context) const
{
	const auto& vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

#ifndef CTS_USES_VULKANSC
	if (m_params.imageFormat == VK_FORMAT_A8_UNORM_KHR || m_params.imageFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

	VkImageFormatProperties	formatProperties;
	const auto result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_params.imageFormat, m_params.imageType, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u, &formatProperties);
	if (result != VK_SUCCESS)
	{
		if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
			TCU_THROW(NotSupportedError, "format " + de::toString(m_params.imageFormat) + " does not support the required features");
		else
			TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
	}
}

TransferQueueInstance::TransferQueueInstance (Context& context, const TransferQueueCase::TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{
	const auto& vk					= context.getDeviceInterface();
	const auto& device				= context.getDevice();
	const deUint32 queueFamilyIndex = context.getUniversalQueueFamilyIndex();

	// Create command pool
	m_cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

	// Create command buffer
	m_cmdBuffer = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

tcu::TestStatus TransferQueueInstance::iterate (void)
{
	// Test every aspect supported by the image format.
	const auto tcuFormat			= mapVkFormat(m_params.imageFormat);

	Allocator& allocator			= m_context.getDefaultAllocator();
	const auto& vk					= m_context.getDeviceInterface();
	const auto device				= m_context.getDevice();
	const auto queue				= getDeviceQueue(m_context.getDeviceInterface(), device, m_context.getUniversalQueueFamilyIndex(), 0u);

	const auto width				= m_params.dimensions.width;
	const auto height				= m_params.dimensions.height;
	const auto layers				= m_params.imageType == vk::VK_IMAGE_TYPE_3D ? 1u : m_params.dimensions.depth;
	const auto depth				= m_params.imageType == vk::VK_IMAGE_TYPE_3D ? m_params.dimensions.depth : 1u;
	const uint32_t pixelDataSize	= tcuFormat.getPixelSize() * width * height * layers * depth;

	const vk::VkBufferCreateInfo bufferCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		0u,																						// flags
		static_cast<VkDeviceSize>(pixelDataSize),												// size
		vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// usage
		vk::VK_SHARING_MODE_EXCLUSIVE,															// sharingMode
		0u,																						// queueFamilyCount
		DE_NULL,																				// pQueueFamilyIndices
	};

	BufferWithMemory srcBuffer(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
	BufferWithMemory dstBuffer(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

	vk::VkExtent3D extent = { m_params.dimensions.width, m_params.dimensions.height, depth };
	const vk::VkImageCreateInfo	imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0,										// VkImageCreateFlags	flags;
		m_params.imageType,						// VkImageType			imageType;
		m_params.imageFormat,					// VkFormat				format;
		extent,									// VkExtent3D			extent;
		1u,										// deUint32				mipLevels;
		layers,									// deUint32				arraySize;
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling;
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		(const deUint32*)DE_NULL,				// const deUint32*		pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout;
	};
	Image image(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any);

	// Generate data for source buffer and copy it into buffer
	std::vector<deUint8>			generatedData	(pixelDataSize);
	de::Random						randomGen		(deInt32Hash((deUint32)m_params.imageFormat) ^
													 deInt32Hash((deUint32)m_params.imageType) ^
													 deInt32Hash((deUint32)m_params.dimensions.width) ^
													 deInt32Hash((deUint32)m_params.dimensions.height) ^
													 deInt32Hash((deUint32)m_params.dimensions.depth));
	{
		fillRandomNoNaN(&randomGen, generatedData.data(), (deUint32)generatedData.size(), m_params.imageFormat);
		const Allocation& alloc = srcBuffer.getAllocation();
		deMemcpy(alloc.getHostPtr(), generatedData.data(), generatedData.size());
		flushAlloc(vk, device, alloc);
	}

	beginCommandBuffer(vk, *m_cmdBuffer);
	const VkImageSubresourceRange			subresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layers);
	const VkImageMemoryBarrier				imageBarrier		= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *image, subresourceRange);
	vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1u, &imageBarrier);
	// Copy buffer to image
	{
		const bool		isCompressed	= isCompressedFormat(m_params.imageFormat);
		const deUint32	blockHeight		= (isCompressed) ? getBlockHeight(m_params.imageFormat) : 1u;
		deUint32		imageHeight		= ((height + blockHeight - 1) / blockHeight) * blockHeight;

		const vk::VkBufferImageCopy	copyRegion =
		{
			0u,												// VkDeviceSize				bufferOffset;
			0,												// deUint32					bufferRowLength;
			imageHeight,									// deUint32					bufferImageHeight;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,					// VkImageAspectFlags		aspect;
				0,											// deUint32					mipLevel;
				0u,											// deUint32					baseArrayLayer;
				layers,										// deUint32					layerCount;
			},												// VkImageSubresourceLayers	imageSubresource;
			{ 0, 0, 0 },									// VkOffset3D				imageOffset;
			extent											// VkExtent3D				imageExtent;
		};

		const VkImageMemoryBarrier		postImageBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags			dstAccessMask;
			VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
			*image,											// VkImage					image;
			{												// VkImageSubresourceRange	subresourceRange;
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspect;
				0u,							// deUint32				baseMipLevel;
				1,							// deUint32				mipLevels;
				0u,							// deUint32				baseArraySlice;
				layers,						// deUint32				arraySize;
			}
		};

		vk.cmdCopyBufferToImage(*m_cmdBuffer, *srcBuffer, *image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);

		vk.cmdCopyImageToBuffer(*m_cmdBuffer, *image, VK_IMAGE_LAYOUT_GENERAL, *dstBuffer, 1, &copyRegion);
	}
	endCommandBuffer(vk, *m_cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *m_cmdBuffer);

	{
		std::vector<deUint8> resultData(pixelDataSize);
		const Allocation& alloc = dstBuffer.getAllocation();
		invalidateAlloc(vk, device, alloc);
		deMemcpy(resultData.data(), alloc.getHostPtr(), resultData.size());

		for (uint32_t i = 0; i < pixelDataSize; ++i) {
			if (resultData[i] != generatedData[i]) {
				return tcu::TestStatus::fail("Transfer queue test");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

VkImageAspectFlags getAspectFlags (tcu::TextureFormat format)
{
	VkImageAspectFlags	aspectFlag = 0;
	aspectFlag |= (tcu::hasDepthComponent(format.order) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
	aspectFlag |= (tcu::hasStencilComponent(format.order) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	if (!aspectFlag)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

	return aspectFlag;
}

tcu::TestCaseGroup* createTransferQueueImageTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> layoutTestGroup (new tcu::TestCaseGroup(testCtx, "queue_transfer", "Tests for transfering from buffer to image and back"));

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
		VkExtent3D	extent;
		const char*	name;
		const char*	desc;
	} extents[] =
	{
		{ {4u,	3u,		1u},							"4x3x1",			"4x3x1 extent"				},
		{ {16u, 15u,	1u},							"16x15x1",			"16x15x1 extent"			},
		{ {64u, 31u,	1u},							"64x31x1",			"64x31x1 extent"			},
		{ {4u,	3u,		2u},							"4x3x2",			"4x3x2extent"				},
		{ {16u, 15u,	16u},							"16x15x16",			"16x15x16 extent"			},
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
	};

	for (int classIdx = 0; classIdx < DE_LENGTH_OF_ARRAY(imageClass); ++classIdx)
	{
		const auto& imgClass = imageClass[classIdx];
		de::MovePtr<tcu::TestCaseGroup> classGroup (new tcu::TestCaseGroup(testCtx, imgClass.name, imgClass.desc));

		for (int extentIdx = 0; extentIdx < DE_LENGTH_OF_ARRAY(extents); ++extentIdx)
		{
			const auto &extent = extents[extentIdx];
			de::MovePtr<tcu::TestCaseGroup> mipGroup (new tcu::TestCaseGroup(testCtx, extent.name, extent.desc));

			for (int formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(testFormats); ++formatIdx)
			{
				static const auto	prefixLen	= std::string("VK_FORMAT_").size();
				const auto			format		= testFormats[formatIdx];
				const auto			fmtName		= std::string(getFormatName(format));
				const auto			name		= de::toLower(fmtName.substr(prefixLen)); // Remove VK_FORMAT_ prefix.
				const auto			desc		= "Using format " + fmtName;

				TransferQueueCase::TestParams params;
				params.imageFormat	= format;
				params.imageType	= imgClass.type;
				params.dimensions	= {extent.extent.width, extent.extent.height, extent.extent.depth};

				mipGroup->addChild(new TransferQueueCase(testCtx, name, desc, params));
			}

			classGroup->addChild(mipGroup.release());
		}

		layoutTestGroup->addChild(classGroup.release());
	}

	return layoutTestGroup.release();
}

} // namespace image
} // namespace vkt
