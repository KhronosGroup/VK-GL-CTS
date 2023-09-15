/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC.
 * Copyright (c) 2022 LunarG, Inc.
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
 * \brief Tests for VK_EXT_host_image_copy
 *//*--------------------------------------------------------------------*/

#include "vktImageHostImageCopyTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktImageTestsUtil.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"

#include "tcuTestLog.hpp"

namespace vkt
{
namespace image
{
namespace
{

using namespace vk;

vk::VkImageAspectFlags getAspectFlags (vk::VkFormat format)
{
	if (isCompressedFormat(format))
	{
		return vk::VK_IMAGE_ASPECT_COLOR_BIT;
	}

	const auto	sampledFormat	= mapVkFormat(format);
	if (sampledFormat.order == tcu::TextureFormat::S)
	{
		return vk::VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if (sampledFormat.order == tcu::TextureFormat::D || sampledFormat.order == tcu::TextureFormat::DS)
	{
		return vk::VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	return vk::VK_IMAGE_ASPECT_COLOR_BIT;
}

deUint32 getChannelSize (vk::VkFormat format)
{
	const auto tcuFormat = isCompressedFormat(format) ? tcu::getUncompressedFormat(mapVkCompressedFormat(format)) : mapVkFormat(format);
	if (tcuFormat.order != tcu::TextureFormat::D && tcuFormat.order != tcu::TextureFormat::S && tcuFormat.order != tcu::TextureFormat::DS)
	{
		return tcu::getChannelSize(tcuFormat.type);
	}
	switch (format)
	{
	case vk::VK_FORMAT_D24_UNORM_S8_UINT:
		return 4;
	case vk::VK_FORMAT_D32_SFLOAT:
		return 4;
	case vk::VK_FORMAT_D16_UNORM:
		return 2;
	case vk::VK_FORMAT_S8_UINT:
		return 1;
	default:
		break;
	}
	DE_ASSERT(0);
	return 0;
}

deUint32 getNumChannels (vk::VkFormat format)
{
	const auto tcuFormat = isCompressedFormat(format) ? tcu::getUncompressedFormat(mapVkCompressedFormat(format)) : mapVkFormat(format);
	if (tcuFormat.order != tcu::TextureFormat::D && tcuFormat.order != tcu::TextureFormat::S && tcuFormat.order != tcu::TextureFormat::DS)
	{
		return tcu::getNumUsedChannels(tcuFormat.order);
	}
	return 1;
}

void generateData(void* ptr, deUint32 size, vk::VkFormat format) {
	if (isDepthStencilFormat(format))
	{
		de::Random randomGen(deInt32Hash((deUint32)format) ^
							 deInt32Hash((deUint32)size));
		if (format == VK_FORMAT_D16_UNORM) {
			ycbcr::fillRandomNoNaN(&randomGen, (deUint8*)ptr, size, VK_FORMAT_R16_UNORM);
		} else {
			ycbcr::fillRandomNoNaN(&randomGen, (deUint8*)ptr, size, VK_FORMAT_R32_SFLOAT);
		}
	}
	else if (isCompressedFormat(format))
	{
		memset(ptr, 255, size);
	}
	else
	{
		de::Random randomGen(deInt32Hash((deUint32)format) ^
							 deInt32Hash((deUint32)size));
		ycbcr::fillRandomNoNaN(&randomGen, (deUint8*)ptr, size, format);
	}
}

void getHostImageCopyProperties(const vk::InstanceDriver& instanceDriver, VkPhysicalDevice physicalDevice, vk::VkPhysicalDeviceHostImageCopyPropertiesEXT* hostImageCopyProperties) {
	vk::VkPhysicalDeviceProperties				properties;
	deMemset(&properties, 0, sizeof(vk::VkPhysicalDeviceProperties));
	vk::VkPhysicalDeviceProperties2				properties2 =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,			// VkStructureType					sType
		hostImageCopyProperties,									// const void*						pNext
		properties													// VkPhysicalDeviceProperties		properties
	};
	instanceDriver.getPhysicalDeviceProperties2(physicalDevice, &properties2);
}

bool isBlockCompressedFormat (vk::VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
		case VK_FORMAT_BC2_UNORM_BLOCK:
		case VK_FORMAT_BC2_SRGB_BLOCK:
		case VK_FORMAT_BC3_UNORM_BLOCK:
		case VK_FORMAT_BC3_SRGB_BLOCK:
		case VK_FORMAT_BC4_UNORM_BLOCK:
		case VK_FORMAT_BC4_SNORM_BLOCK:
		case VK_FORMAT_BC5_UNORM_BLOCK:
		case VK_FORMAT_BC5_SNORM_BLOCK:
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:
		case VK_FORMAT_BC7_UNORM_BLOCK:
		case VK_FORMAT_BC7_SRGB_BLOCK:
			return true;
		default:
			break;
	}
	return false;
}

void checkSupportedFormatFeatures(const vk::InstanceDriver& vki, VkPhysicalDevice physicalDevice, vk::VkFormat format, vk::VkImageTiling tiling, deUint64 *outDrmModifier)
{
	vk::VkDrmFormatModifierPropertiesList2EXT drmList = vk::initVulkanStructure();
	vk::VkFormatProperties3 formatProperties3 = tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ?
												vk::initVulkanStructure(&drmList) :
												vk::initVulkanStructure();
	vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);
	std::vector<vk::VkDrmFormatModifierProperties2EXT> modifiers(drmList.drmFormatModifierCount);

	if (tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
	{
		if (drmList.drmFormatModifierCount == 0)
			TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for drm format modifier.");
		drmList.pDrmFormatModifierProperties = modifiers.data();
		vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);

		bool modifierFound = false;
		for (deUint32 i = 0; i < drmList.drmFormatModifierCount; ++i)
		{
			if (drmList.pDrmFormatModifierProperties[i].drmFormatModifierTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT)
			{
				*outDrmModifier = drmList.pDrmFormatModifierProperties[i].drmFormatModifier;
				return;
			}
		}

		if (!modifierFound)
			TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for drm format modifier.");
	}
	else
	{
		if (tiling == vk::VK_IMAGE_TILING_LINEAR &&
			(formatProperties3.linearTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
			TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for optimal tiling.");
		if (tiling == vk::VK_IMAGE_TILING_OPTIMAL &&
			(formatProperties3.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
			TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for optimal tiling.");
	}
}

enum Command {
	DRAW,
	DISPATCH,
};

struct TestParameters
{
	bool				hostCopyMemoryToImage;
	bool				hostCopyImageToMemory;
	bool				hostTransferLayout;
	bool				outputImageHostTransition;
	bool				memcpyFlag;
	bool				dynamicRendering;
	Command				command;
	vk::VkFormat		imageSampledFormat;
	vk::VkImageLayout	srcLayout;
	vk::VkImageLayout	dstLayout;
	vk::VkImageLayout	intermediateLayout;
	vk::VkImageTiling	sampledTiling;
	vk::VkFormat		imageOutputFormat;
	vk::VkExtent3D		imageSize;
	bool				sparse;
	deUint32			mipLevel;
	deUint32			regionsCount;
	deUint32			padding;
};

class HostImageCopyTestInstance : public vkt::TestInstance
{
public:
	HostImageCopyTestInstance							(vkt::Context& context, const TestParameters& parameters)
														: vkt::TestInstance	(context)
														, m_parameters		(parameters)
														{
														}
	void						transitionImageLayout	(const Move<vk::VkCommandBuffer>* cmdBuffer, vk::VkImage image, vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout, vk::VkImageSubresourceRange subresourceRange);
	void						copyMemoryToImage		(const std::vector<deUint8> testData, vk::VkImage image, deUint32 texelSize, const vk::VkImageSubresourceLayers subresourceLayers, deInt32 xOffset, deInt32 yOffset, deUint32 width, deUint32 height);

private:
	tcu::TestStatus				iterate					(void);

	const TestParameters	m_parameters;
};

void HostImageCopyTestInstance::transitionImageLayout (const Move<vk::VkCommandBuffer>* cmdBuffer, vk::VkImage image, vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout, vk::VkImageSubresourceRange subresourceRange)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const vk::VkDevice				device				= m_context.getDevice();
	const vk::VkQueue				queue				= m_context.getUniversalQueue();

	if (m_parameters.hostTransferLayout)
	{
		vk::VkHostImageLayoutTransitionInfoEXT transition =
		{
			vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,		// VkStructureType			sType;
			DE_NULL,															// const void*				pNext;
			image,																// VkImage					image;
			oldLayout,															// VkImageLayout			oldLayout;
			newLayout,															// VkImageLayout			newLayout;
			subresourceRange													// VkImageSubresourceRange subresourceRange;
		};
		vk.transitionImageLayoutEXT(device, 1, &transition);
	}
	else
	{
		vk::beginCommandBuffer(vk, **cmdBuffer, 0u);
		auto imageMemoryBarrier = makeImageMemoryBarrier(0u, vk::VK_ACCESS_TRANSFER_WRITE_BIT, oldLayout, newLayout, image, subresourceRange);
		vk.cmdPipelineBarrier(**cmdBuffer, vk::VK_PIPELINE_STAGE_NONE, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1, &imageMemoryBarrier);
		vk::endCommandBuffer(vk, **cmdBuffer);
		vk::submitCommandsAndWait(vk, device, queue, **cmdBuffer);
	}
}

void HostImageCopyTestInstance::copyMemoryToImage (const std::vector<deUint8> testData, vk::VkImage image, deUint32 texelSize, const vk::VkImageSubresourceLayers subresourceLayers, deInt32 xOffset, deInt32 yOffset, deUint32 width, deUint32 height)
{
	const DeviceInterface&			vk			= m_context.getDeviceInterface();
	const vk::VkDevice				device		= m_context.getDevice();
	std::vector<deUint8>			data		(texelSize * width * height);
	const deUint32					imageWidth	= m_parameters.imageSize.width;
	for (deUint32 i = 0; i < height; ++i) {
		memcpy(&data[i * width * texelSize], &testData[((yOffset + i) * imageWidth + xOffset) * texelSize], width * texelSize);
	}

	const deUint32 regionsCount = m_parameters.regionsCount > height ? m_parameters.regionsCount : 1u;
	std::vector<vk::VkMemoryToImageCopyEXT> regions;

	for (deUint32 i = 0; i < regionsCount; ++i)
	{
		vk::VkOffset3D	offset = { xOffset, (deInt32)(yOffset + height / regionsCount * i), 0 };
		vk::VkExtent3D	extent = { width, height / regionsCount, 1 };
		if (i == regionsCount - 1)
			extent.height = height - height / regionsCount * i;

		if (extent.height == 0)
			continue;

		deUint32 dataOffset = width * (height / regionsCount * i) * texelSize;

		const vk::VkMemoryToImageCopyEXT region = {
			vk::VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			&data[dataOffset],									// const void*					memoryHostPointer;
			0,													// uint32_t						memoryRowLength;
			0,													// uint32_t						memoryImageHeight;
			subresourceLayers,									// VkImageSubresourceLayers		imageSubresource;
			offset,												// VkOffset3D					imageOffset;
			extent												// VkExtent3D					imageExtent;
		};
		regions.push_back(region);
	}

	vk::VkCopyMemoryToImageInfoEXT copyMemoryToImageInfo =
	{
		vk::VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		0u,														// VkMemoryImageCopyFlagsEXT		flags;
		image,													// VkImage							dstImage;
		m_parameters.dstLayout,									// VkImageLayout					dstImageLayout;
		(deUint32)regions.size(),								// uint32_t							regionCount;
		regions.data(),											// const VkMemoryToImageCopyEXT*	pRegions;
	};

	vk.copyMemoryToImageEXT(device, &copyMemoryToImageInfo);
}

tcu::TestStatus HostImageCopyTestInstance::iterate (void)
{
	const InstanceInterface&			vki					= m_context.getInstanceInterface();
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const vk::VkPhysicalDevice			physicalDevice		= m_context.getPhysicalDevice();
	const vk::VkDevice					device				= m_context.getDevice();
	const auto&							deviceExtensions	= m_context.getDeviceExtensions();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue					queue				= m_context.getUniversalQueue();
	auto&								alloc				= m_context.getDefaultAllocator();
	tcu::TestLog&						log					= m_context.getTestContext().getLog();

	std::stringstream					commandsLog;

	const Move<vk::VkCommandPool>		cmdPool				(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Move<vk::VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::VkExtent3D				imageSize			= { m_parameters.imageSize.width * (deUint32)dePow(2, m_parameters.mipLevel), m_parameters.imageSize.height * (deUint32)dePow(2, m_parameters.mipLevel), 1 };
	const vk::VkExtent3D				mipImageSize		= { m_parameters.imageSize.width, m_parameters.imageSize.height, 1 };

	const vk::VkRect2D					renderArea			= vk::makeRect2D(0, 0, mipImageSize.width, mipImageSize.height);

	const auto							sampledChannelSize	= getChannelSize(m_parameters.imageSampledFormat);
	const auto							sampledNumChannels	= getNumChannels(m_parameters.imageSampledFormat);
	const auto							sampledBufferCount	= mipImageSize.width * mipImageSize.height * sampledNumChannels;
	const auto							sampledBufferSize	= sampledBufferCount * sampledChannelSize;

	const auto							outputFormat		= mapVkFormat(m_parameters.imageOutputFormat);
	const auto							outputChannelSize	= getChannelSize(m_parameters.imageOutputFormat);
	const auto							outputNumChannels	= getNumUsedChannels(m_parameters.imageOutputFormat);
	const auto							outputBufferCount	= mipImageSize.width * mipImageSize.height * outputNumChannels;
	const auto							outputBufferSize	= outputBufferCount * outputChannelSize;

	vk::VkImage							sampledImage;
	de::MovePtr<ImageWithMemory>		sampledImageWithMemory;
	de::MovePtr<SparseImage>			sparseSampledImage;
	de::MovePtr<ImageWithMemory>		sampledImageWithMemoryCopy;
	de::MovePtr<ImageWithMemory>		outputImage;
	Move<vk::VkImageView>				sampledImageView;
	Move<vk::VkImageView>				sampledImageViewCopy;
	Move<vk::VkImageView>				outputImageView;

	const vk::VkImageAspectFlags		sampledAspect				= getAspectFlags(m_parameters.imageSampledFormat);
	const vk::VkComponentMapping		componentMapping			= makeComponentMappingRGBA();
	const vk::VkOffset3D				imageOffset					= makeOffset3D(0, 0, 0);
	const vk::VkImageSubresource		sampledSubresource			= makeImageSubresource(sampledAspect, m_parameters.mipLevel, 0u);
	const vk::VkImageSubresourceRange	sampledSubresourceRange		= makeImageSubresourceRange(sampledAspect, m_parameters.mipLevel, 1u, 0u, 1u);
	const vk::VkImageSubresourceLayers	sampledSubresourceLayers	= makeImageSubresourceLayers(sampledAspect, m_parameters.mipLevel, 0u, 1u);
	const auto							outputSubresourceRange		= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, m_parameters.mipLevel, 1u, 0u, 1u);
	const vk::VkImageSubresourceLayers	outputSubresourceLayers		= makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, m_parameters.mipLevel, 0u, 1u);

	std::vector<deUint8>				testData					(sampledBufferSize);
	generateData(testData.data(), sampledBufferSize, m_parameters.imageSampledFormat);

	// Create sampled image
	{
		vk::VkImageUsageFlags	usage = vk::VK_IMAGE_USAGE_SAMPLED_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (m_parameters.hostCopyMemoryToImage || m_parameters.hostCopyImageToMemory || m_parameters.memcpyFlag || m_parameters.hostTransferLayout)
			usage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
		if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			usage |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			usage |= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			usage |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		vk::VkImageCreateInfo	createInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageCreateFlags		flags
			vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			m_parameters.imageSampledFormat,			// VkFormat					format
			imageSize,									// VkExtent3D				extent
			m_parameters.mipLevel + 1,					// uint32_t					mipLevels
			1u,											// uint32_t					arrayLayers
			vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			m_parameters.sampledTiling,					// VkImageTiling			tiling
			usage,										// VkImageUsageFlags		usage
			vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			0,											// uint32_t					queueFamilyIndexCount
			DE_NULL,									// const uint32_t*			pQueueFamilyIndices
			vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		if (m_parameters.sparse)
		{
			createInfo.flags |= (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
			sparseSampledImage = de::MovePtr<SparseImage>(new SparseImage(vk, device, physicalDevice, vki, createInfo, m_context.getSparseQueue(), alloc, mapVkFormat(createInfo.format)));
			sampledImage = **sparseSampledImage;
		}
		else
		{
			sampledImageWithMemory = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
			if (m_parameters.memcpyFlag)
				sampledImageWithMemoryCopy = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
			sampledImage = **sampledImageWithMemory;
		}

		vk::VkImageViewCreateInfo		imageViewCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			(vk::VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
			sampledImage,									// VkImage					image;
			vk::VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
			m_parameters.imageSampledFormat,				// VkFormat					format;
			componentMapping,								// VkComponentMapping		components;
			sampledSubresourceRange							// VkImageSubresourceRange	subresourceRange;
		};
		sampledImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
		if (m_parameters.memcpyFlag)
		{
			imageViewCreateInfo.image = **sampledImageWithMemoryCopy;
			sampledImageViewCopy = createImageView(vk, device, &imageViewCreateInfo, NULL);;
		}
	}

	// Create output image
	{
		vk::VkImageUsageFlags	usage = vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (m_parameters.outputImageHostTransition || m_parameters.hostCopyImageToMemory || m_parameters.hostTransferLayout)
			usage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
		if (m_parameters.command == DISPATCH)
			usage |= vk::VK_IMAGE_USAGE_STORAGE_BIT;

		const vk::VkImageCreateInfo	createInfo		=
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageCreateFlags		flags
			vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
			m_parameters.imageOutputFormat,				// VkFormat					format
			imageSize,									// VkExtent3D				extent
			m_parameters.mipLevel + 1,					// uint32_t					mipLevels
			1u,											// uint32_t					arrayLayers
			vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
			vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
			usage,										// VkImageUsageFlags		usage
			vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
			0,											// uint32_t					queueFamilyIndexCount
			DE_NULL,									// const uint32_t*			pQueueFamilyIndices
			vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
		};

		outputImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));

		vk::VkImageViewCreateInfo		imageViewCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			(VkImageViewCreateFlags)0u,						// VkImageViewCreateFlags	flags;
			**outputImage,									// VkImage					image;
			vk::VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
			m_parameters.imageOutputFormat,					// VkFormat					format;
			componentMapping,								// VkComponentMapping		components;
			outputSubresourceRange							// VkImageSubresourceRange	subresourceRange;
		};
		outputImageView = createImageView(vk, device, &imageViewCreateInfo, NULL);
	}

	const vk::VkAttachmentDescription	colorAttachmentDescription	=
	{
			(vk::VkAttachmentDescriptionFlags)0u,		// VkAttachmentDescriptionFlags    flags
			m_parameters.imageOutputFormat,				// VkFormat                        format
			vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits           samples
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// VkAttachmentLoadOp              loadOp
			vk::VK_ATTACHMENT_STORE_OP_STORE,			// VkAttachmentStoreOp             storeOp
			vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// VkAttachmentLoadOp              stencilLoadOp
			vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,		// VkAttachmentStoreOp             stencilStoreOp
			vk::VK_IMAGE_LAYOUT_GENERAL,				// VkImageLayout                   initialLayout
			vk::VK_IMAGE_LAYOUT_GENERAL					// VkImageLayout                   finalLayout
	};

	const vk::VkAttachmentReference				colorAttachmentRef =
	{
		0u,								// deUint32         attachment
		vk::VK_IMAGE_LAYOUT_GENERAL		// VkImageLayout    layout
	};

	const vk::VkSubpassDescription				subpassDescription =
	{
		(vk::VkSubpassDescriptionFlags)0u,						// VkSubpassDescriptionFlags       flags
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint             pipelineBindPoint
		0u,														// deUint32                        inputAttachmentCount
		DE_NULL,												// const VkAttachmentReference*    pInputAttachments
		1u,														// deUint32                        colorAttachmentCount
		&colorAttachmentRef,									// const VkAttachmentReference*    pColorAttachments
		DE_NULL,												// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,												// const VkAttachmentReference*    pDepthStencilAttachment
		0u,														// deUint32                        preserveAttachmentCount
		DE_NULL													// const deUint32*                 pPreserveAttachments
	};

	Move<vk::VkRenderPass>				renderPass;
	Move<vk::VkFramebuffer>				framebuffer;
	if (!m_parameters.dynamicRendering && m_parameters.command == DRAW)
	{
		const vk::VkRenderPassCreateInfo renderPassInfo =
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
			nullptr,										// const void*						pNext;
			0u,												// VkRenderPassCreateFlags			flags;
			1u,												// deUint32							attachmentCount;
			&colorAttachmentDescription,					// const VkAttachmentDescription*	pAttachments;
			1u,												// deUint32							subpassCount;
			&subpassDescription,							// const VkSubpassDescription*		pSubpasses;
			0u,												// deUint32							dependencyCount;
			nullptr,										// const VkSubpassDependency*		pDependencies;
		};
		renderPass	= createRenderPass(vk, device, &renderPassInfo);
		framebuffer = makeFramebuffer(vk, device, *renderPass, *outputImageView, renderArea.extent.width, renderArea.extent.height);
	}

	const std::vector<vk::VkViewport>	viewports			{ makeViewport(renderArea.extent) };
	const std::vector<vk::VkRect2D>		scissors			{ makeRect2D(renderArea.extent) };

	vk::ShaderWrapper			vert				= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
	vk::ShaderWrapper			frag				= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

	DescriptorSetLayoutBuilder			descriptorBuilder;
	descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vk::VK_SHADER_STAGE_FRAGMENT_BIT | vk::VK_SHADER_STAGE_COMPUTE_BIT);
	if (m_parameters.command == DISPATCH)
		descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk::VK_SHADER_STAGE_COMPUTE_BIT);

	const auto							descriptorSetLayout	(descriptorBuilder.build(vk, device));
	const vk::PipelineLayoutWrapper		pipelineLayout		(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, *descriptorSetLayout);

	DescriptorPoolBuilder				poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	if (m_parameters.command == DISPATCH)
		poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	const Move<vk::VkDescriptorPool>	descriptorPool		= poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
	const Move<vk::VkDescriptorSet>		descriptorSet		(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	vk::VkSamplerCreateInfo				samplerParams		=
	{
		vk::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,										// const void*			pNext;
		0u,												// VkSamplerCreateFlags	flags;
		vk::VK_FILTER_NEAREST,							// VkFilter				magFilter;
		vk::VK_FILTER_NEAREST,							// VkFilter				minFilter;
		vk::VK_SAMPLER_MIPMAP_MODE_NEAREST,				// VkSamplerMipmapMode	mipmapMode;
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeU;
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeV;
		vk::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,	// VkSamplerAddressMode	addressModeW;
		0.0f,											// float				mipLodBias;
		VK_FALSE,										// VkBool32				anisotropyEnable;
		0.0f,											// float				maxAnisotropy;
		VK_FALSE,										// VkBool32				compareEnable;
		vk::VK_COMPARE_OP_NEVER,						// VkCompareOp			compareOp;
		0.0f,											// float				minLod;
		VK_LOD_CLAMP_NONE,								// float				maxLod;
		vk::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,			// VkBorderColor		borderColor;
		VK_FALSE										// VkBool32				unnormalizedCoordinates;
	};
	const vk::Move<vk::VkSampler>		sampler					= createSampler(vk, device, &samplerParams);
	vk::VkDescriptorImageInfo			descriptorSrcImageInfo	(makeDescriptorImageInfo(*sampler, *sampledImageView, vk::VK_IMAGE_LAYOUT_GENERAL));
	const vk::VkDescriptorImageInfo		descriptorDstImageInfo	(makeDescriptorImageInfo(*sampler, *outputImageView, vk::VK_IMAGE_LAYOUT_GENERAL));

	const vk::VkPipelineVertexInputStateCreateInfo vertexInput =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		0u,																//	deUint32									vertexBindingDescriptionCount;
		DE_NULL,														//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		0u,																//	deUint32									vertexAttributeDescriptionCount;
		DE_NULL,														//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	GraphicsPipelineWrapper	pipeline(vki, vk, physicalDevice, device, deviceExtensions, vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
	Move<vk::VkPipeline>	computePipeline;

	if (m_parameters.command == DRAW)
	{
		vk::VkPipelineRenderingCreateInfo		pipelineRenderingCreateInfo;
		if (m_parameters.dynamicRendering)
		{
			pipelineRenderingCreateInfo = {
				vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,	// VkStructureType	sType
				DE_NULL,												// const void*		pNext
				0u,														// uint32_t			viewMask
				1u,														// uint32_t			colorAttachmentCount
				&m_parameters.imageOutputFormat,						// const VkFormat*	pColorAttachmentFormats
				vk::VK_FORMAT_UNDEFINED,								// VkFormat			depthAttachmentFormat
				vk::VK_FORMAT_UNDEFINED									// VkFormat			stencilAttachmentFormat
			};
		}

		vk::PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
		renderingCreateInfoWrapper.ptr = m_parameters.dynamicRendering ? &pipelineRenderingCreateInfo : DE_NULL;

		pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
			.setDefaultRasterizationState()
			.setDefaultMultisampleState()
			.setDefaultDepthStencilState()
			.setDefaultColorBlendState()
			.setupVertexInputState(&vertexInput)
			.setupPreRasterizationShaderState(viewports,
				scissors,
				pipelineLayout,
				*renderPass,
				0u,
				vert,
				DE_NULL,
				{},
				{},
				{},
				DE_NULL,
				DE_NULL,
				renderingCreateInfoWrapper)
			.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, frag)
			.setupFragmentOutputState(*renderPass)
			.setMonolithicPipelineLayout(pipelineLayout)
			.buildPipeline();
	}
	else
	{
		const Unique<vk::VkShaderModule> cs(vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
		const vk::VkPipelineShaderStageCreateInfo	pipelineShaderStageParams =
		{
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// sType
			DE_NULL,													// pNext
			(VkPipelineShaderStageCreateFlags)0u,						// flags
			vk::VK_SHADER_STAGE_COMPUTE_BIT,							// stage
			*cs,														// module
			"main",														// pName
			DE_NULL,													// pSpecializationInfo
		};
		const vk::VkComputePipelineCreateInfo		pipelineCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// sType
			DE_NULL,													// pNext
			(VkPipelineCreateFlags)0u,									// flags
			pipelineShaderStageParams,									// stage
			*pipelineLayout,											// layout
			DE_NULL,													// basePipelineHandle
			0,															// basePipelineIndex
		};
		computePipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo);
	}

	de::MovePtr<BufferWithMemory>	colorOutputBuffer		= de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, alloc, makeBufferCreateInfo(outputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

	// Load sampled image
	if (m_parameters.hostCopyMemoryToImage)
	{
		transitionImageLayout(&cmdBuffer, sampledImage, vk::VK_IMAGE_LAYOUT_UNDEFINED, m_parameters.dstLayout, sampledSubresourceRange);
		commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage << " to layout " << getImageLayoutStr(m_parameters.dstLayout).toString() << "\n";

		copyMemoryToImage(testData, sampledImage, sampledChannelSize * sampledNumChannels, sampledSubresourceLayers, 0, 0, mipImageSize.width, mipImageSize.height);
		commandsLog << "vkCopyMemoryToImageEXT() with image " << sampledImage << ", xOffset (0), yOffset (0), width (" << mipImageSize.width << "), height (" << mipImageSize.height << ")\n";

		de::Random randomGen(deInt32Hash((deUint32)m_parameters.imageSampledFormat) ^
							 deInt32Hash((deUint32)mipImageSize.width) ^
							 deInt32Hash((deUint32)mipImageSize.height) ^
							 deInt32Hash((deUint32)mipImageSize.depth));
		for (int i = 0; i < 20; ++i)
		{
			deInt32 xOffset = randomGen.getInt32() % (mipImageSize.width / 2);
			deInt32 yOffset = randomGen.getInt32() % (mipImageSize.height / 2);
			deUint32 width = deMaxu32(randomGen.getUint32() % (mipImageSize.width / 2), 1u);
			deUint32 height = deMaxu32(randomGen.getUint32() % (mipImageSize.height / 2), 1u);

			if (isCompressedFormat(m_parameters.imageSampledFormat)) {
				deUint32 blockWidth		= getBlockWidth(m_parameters.imageSampledFormat);
				deUint32 blockHeight	= getBlockHeight(m_parameters.imageSampledFormat);
				xOffset = (xOffset / blockWidth) * blockWidth;
				yOffset = (yOffset / blockHeight) * blockHeight;
				width	= deMaxu32((width / blockWidth) * blockWidth, blockWidth);
				height	= deMaxu32((height / blockHeight) * blockHeight, blockHeight);
			}

			copyMemoryToImage(testData, sampledImage, sampledChannelSize * sampledNumChannels, sampledSubresourceLayers, xOffset, yOffset, width, height);
			commandsLog << "vkCopyMemoryToImageEXT() with image " << sampledImage << ", xOffset (" << xOffset << "), yOffset (" << yOffset << "), width (" << width << "), height (" << height << ")\n";
		}

		if (m_parameters.dstLayout != vk::VK_IMAGE_LAYOUT_GENERAL)
		{
			transitionImageLayout(&cmdBuffer, sampledImage, m_parameters.dstLayout, vk::VK_IMAGE_LAYOUT_GENERAL, sampledSubresourceRange);
			commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage << " to layout VK_IMAGE_LAYOUT_GENERAL\n";
		}
	}
	else
	{
		de::MovePtr<BufferWithMemory> sampledBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(sampledBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::HostVisible));

		auto& bufferAlloc = sampledBuffer->getAllocation();
		memcpy(bufferAlloc.getHostPtr(), testData.data(), sampledBufferSize);
		flushAlloc(vk, device, bufferAlloc);

		transitionImageLayout(&cmdBuffer, sampledImage, vk::VK_IMAGE_LAYOUT_UNDEFINED, m_parameters.dstLayout, sampledSubresourceRange);
		commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage << " to layout" << getImageLayoutStr(m_parameters.dstLayout).toString() << "\n";

		vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
		const vk::VkBufferImageCopy copyRegion =
		{
			0,
			0,
			0,
			sampledSubresourceLayers,
			imageOffset,
			{
				mipImageSize.width,
				mipImageSize.height,
				mipImageSize.depth,
			},
		};
		vk.cmdCopyBufferToImage(*cmdBuffer, sampledBuffer->get(), sampledImage, m_parameters.dstLayout, 1u, &copyRegion);
		commandsLog << "vkCmdCopyBufferToImage() with image " << sampledImage << ", xOffset (" << copyRegion.imageOffset.x << "), yOffset (" << copyRegion.imageOffset.y << "), width (" << mipImageSize.width << "), height (" << mipImageSize.height << ")\n";

		{
			auto		imageMemoryBarrier	= makeImageMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, m_parameters.dstLayout, m_parameters.intermediateLayout, sampledImage, sampledSubresourceRange);
			vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1, &imageMemoryBarrier);
		}

		vk::endCommandBuffer(vk, *cmdBuffer);
		deUint32				semaphoreCount	= 0;
		vk::VkSemaphore			semaphore		= DE_NULL;
		VkPipelineStageFlags	waitStages		= 0;
		if (m_parameters.sparse)
		{
			semaphoreCount = 1;
			semaphore = sparseSampledImage->getSemaphore();
			waitStages = vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
		vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer, false, 1u, semaphoreCount, &semaphore, &waitStages);

		if (m_parameters.intermediateLayout != vk::VK_IMAGE_LAYOUT_GENERAL)
		{
			transitionImageLayout(&cmdBuffer, sampledImage, m_parameters.intermediateLayout, vk::VK_IMAGE_LAYOUT_GENERAL, sampledSubresourceRange);
			commandsLog << "vkTransitionImageLayoutEXT() image " << sampledImage << " to layout VK_IMAGE_LAYOUT_GENERAL\n";
		}
	}

	if (m_parameters.memcpyFlag)
	{
		vk::VkImageSubresource2EXT			subresource2 =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT,	// VkStructureType		sType;
			DE_NULL,										// void*				pNext;
			sampledSubresource								// VkImageSubresource	imageSubresource;
		};

		vk::VkSubresourceHostMemcpySizeEXT	subresourceHostMemcpySize	= vk::initVulkanStructure();
		vk::VkSubresourceLayout2EXT			subresourceLayout			= vk::initVulkanStructure(&subresourceHostMemcpySize);
		vk.getImageSubresourceLayout2KHR(device, sampledImage, &subresource2, &subresourceLayout);

		std::vector<deUint8> data((size_t)subresourceHostMemcpySize.size);

		const vk::VkImageToMemoryCopyEXT region =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			data.data(),									// void*						memoryHostPointer;
			0u,												// uint32_t						memoryRowLength;
			0u,												// uint32_t						memoryImageHeight;
			sampledSubresourceLayers,						// VkImageSubresourceLayers		imageSubresource;
			imageOffset,									// VkOffset3D					imageOffset;
			mipImageSize,									// VkExtent3D					imageExtent;
		};

		const vk::VkCopyImageToMemoryInfoEXT copyImageToMemoryInfo =
		{
			vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			vk::VK_HOST_IMAGE_COPY_MEMCPY_EXT,						// VkMemoryImageCopyFlagsEXT		flags;
			sampledImage,											// VkImage							srcImage;
			vk::VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout					srcImageLayout;
			1,														// uint32_t							regionCount;
			&region,												// const VkImageToMemoryCopyEXT*	pRegions;
		};
		vk.copyImageToMemoryEXT(device, &copyImageToMemoryInfo);
		commandsLog << "vkCopyImageToMemoryEXT() with image " << sampledImage << ", xOffset (" << region.imageOffset.x << "), yOffset (" << region.imageOffset.y << "), width (" << mipImageSize.width << "), height (" << mipImageSize.height << ")\n";

		transitionImageLayout(&cmdBuffer, **sampledImageWithMemoryCopy, vk::VK_IMAGE_LAYOUT_UNDEFINED, m_parameters.dstLayout, sampledSubresourceRange);

		const vk::VkMemoryToImageCopyEXT toImageRegion = {
			vk::VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			data.data(),										// const void*					memoryHostPointer;
			0,													// uint32_t						memoryRowLength;
			0,													// uint32_t						memoryImageHeight;
			sampledSubresourceLayers,							// VkImageSubresourceLayers		imageSubresource;
			imageOffset,										// VkOffset3D					imageOffset;
			mipImageSize										// VkExtent3D					imageExtent;
		};

		vk::VkCopyMemoryToImageInfoEXT copyMemoryToImageInfo =
		{
			vk::VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			vk::VK_HOST_IMAGE_COPY_MEMCPY_EXT,						// VkMemoryImageCopyFlagsEXT		flags;
			**sampledImageWithMemoryCopy,							// VkImage							dstImage;
			m_parameters.dstLayout,									// VkImageLayout					dstImageLayout;
			1u,														// uint32_t							regionCount;
			&toImageRegion,											// const VkMemoryToImageCopyEXT*	pRegions;
		};
		vk.copyMemoryToImageEXT(device, &copyMemoryToImageInfo);
		commandsLog << "vkCopyMemoryToImageEXT() with image " << **sampledImageWithMemoryCopy << ", xOffset (" << toImageRegion.imageOffset.x << "), yOffset (" << toImageRegion.imageOffset.y << "), width (" << toImageRegion.imageExtent.width << "), height (" << toImageRegion.imageExtent.height << ")\n";
		descriptorSrcImageInfo.imageView = *sampledImageViewCopy;

		transitionImageLayout(&cmdBuffer, **sampledImageWithMemoryCopy, m_parameters.dstLayout, vk::VK_IMAGE_LAYOUT_GENERAL, sampledSubresourceRange);
	}

	// Transition output image
	transitionImageLayout(&cmdBuffer, **outputImage, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, outputSubresourceRange);
	commandsLog << "vkTransitionImageLayoutEXT() image " << **outputImage << " to layout VK_IMAGE_LAYOUT_GENERAL\n";
	vk::beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk::DescriptorSetUpdateBuilder		updateBuilder;
	updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorSrcImageInfo);
	if (m_parameters.command == DISPATCH)
		updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorDstImageInfo);
	updateBuilder.update(vk, device);

	if (m_parameters.command == DRAW)
	{
		if (m_parameters.dynamicRendering)
			beginRendering(vk, *cmdBuffer, *outputImageView, renderArea, vk::VkClearValue(), vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE);
		else
			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea);

		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &*descriptorSet, 0, DE_NULL);
		vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
		commandsLog << "vkCmdDraw()\n";

		if (m_parameters.dynamicRendering)
			endRendering(vk, *cmdBuffer);
		else
			endRenderPass(vk, *cmdBuffer);

		const auto	postImageBarrier = makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, m_parameters.srcLayout, **outputImage, outputSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 0, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	}
	else
	{
		const auto	imageMemoryBarrier	= makeImageMemoryBarrier(0u, vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **outputImage, outputSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageMemoryBarrier);
		vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &*descriptorSet, 0, DE_NULL);
		vk.cmdDispatch(*cmdBuffer, renderArea.extent.width, renderArea.extent.height, 1);
		commandsLog << "vkCmdDispatch()\n";

		vk::VkImageMemoryBarrier postImageBarrier =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			vk::VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			srcAccessMask
			vk::VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask
			vk::VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout
			m_parameters.srcLayout,							// VkImageLayout			newLayout
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t					srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t					dstQueueFamilyIndex
			**outputImage,									// VkImage					image
			outputSubresourceRange							// VkImageSubresourceRange	subresourceRange
		};

		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const vk::VkMemoryBarrier*)DE_NULL, 0, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
	}

	const vk::VkBufferImageCopy	copyRegion =
	{
		0u,																		// VkDeviceSize				bufferOffset;
		0u,																		// deUint32					bufferRowLength;
		0u,																		// deUint32					bufferImageHeight;
		outputSubresourceLayers,												// VkImageSubresourceLayers	imageSubresource;
		imageOffset,															// VkOffset3D				imageOffset;
		{renderArea.extent.width, renderArea.extent.height, 1}					// VkExtent3D				imageExtent;
	};
	vk.cmdCopyImageToBuffer(*cmdBuffer, **outputImage, m_parameters.srcLayout, **colorOutputBuffer, 1u, &copyRegion);
	commandsLog << "vkCmdCopyImageToBuffer() with image " << **outputImage << ", xOffset (" << imageOffset.x << "), yOffset (" << imageOffset.y << "), width (" << renderArea.extent.width << "), height (" << renderArea.extent.height << "\n";
	vk::endCommandBuffer(vk, *cmdBuffer);

	vk::submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Verify image
	tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(outputFormat, renderArea.extent.width, renderArea.extent.height, 1, (const void*)colorOutputBuffer->getAllocation().getHostPtr());

	if (m_parameters.hostCopyImageToMemory)
	{
		const deUint32			paddedBufferSize	= (mipImageSize.width + m_parameters.padding) * (mipImageSize.height + m_parameters.padding) * outputNumChannels * outputChannelSize;
		const deUint32			memoryRowLength		= (mipImageSize.width + m_parameters.padding);
		const deUint32			memoryImageHeight	= (mipImageSize.height + m_parameters.padding);
		std::vector<deUint8>	paddedData			(paddedBufferSize);
		std::vector<deUint8>	data				(outputBufferSize);

		std::vector<vk::VkImageToMemoryCopyEXT> regions(m_parameters.regionsCount);

		for (deUint32 i = 0; i < m_parameters.regionsCount; ++i)
		{
			vk::VkOffset3D	offset	= { 0, (deInt32)(mipImageSize.height / m_parameters.regionsCount * i), 0 };
			vk::VkExtent3D	extent	= { mipImageSize.width, mipImageSize.height / m_parameters.regionsCount, 1 };
			if (i == m_parameters.regionsCount - 1)
				extent.height = mipImageSize.height - mipImageSize.height / m_parameters.regionsCount * i;

			deUint32 dataOffset = (mipImageSize.width + m_parameters.padding) * offset.y * outputNumChannels * outputChannelSize;

			const vk::VkImageToMemoryCopyEXT region =
			{
				vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT,	// VkStructureType				sType;
				DE_NULL,										// const void*					pNext;
				&paddedData[dataOffset],								// void*						memoryHostPointer;
				memoryRowLength,								// uint32_t						memoryRowLength;
				memoryImageHeight,								// uint32_t						memoryImageHeight;
				outputSubresourceLayers,						// VkImageSubresourceLayers		imageSubresource;
				offset,											// VkOffset3D					imageOffset;
				extent,											// VkExtent3D					imageExtent;
			};

			regions[i] = region;
		}

		const vk::VkCopyImageToMemoryInfoEXT copyImageToMemoryInfo =
		{
			vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			0u,														// VkMemoryImageCopyFlagsEXT		flags;
			**outputImage,											// VkImage							srcImage;
			m_parameters.srcLayout,									// VkImageLayout					srcImageLayout;
			(deUint32)regions.size(),								// uint32_t							regionCount;
			regions.data(),											// const VkImageToMemoryCopyEXT*	pRegions;
		};
		vk.copyImageToMemoryEXT(device, &copyImageToMemoryInfo);
		commandsLog << "vkCopyImageToMemoryEXT() with image " << **outputImage << "\n";

		for (deUint32 j = 0; j < mipImageSize.height; ++j)
		{
			for (deUint32 i = 0; i < mipImageSize.width; ++i)
			{
				for (deUint32 k = 0; k < outputNumChannels * outputChannelSize; ++k)
				{
					deUint32 dstIndex = j * mipImageSize.width * (outputNumChannels * outputChannelSize) + i * (outputNumChannels * outputChannelSize) + k;
					deUint32 srcIndex = j * (mipImageSize.width + m_parameters.padding) * (outputNumChannels * outputChannelSize) + i * (outputNumChannels * outputChannelSize) + k;
					data[dstIndex] = paddedData[srcIndex];
				}
			}
		}

		bool match = memcmp(data.data(), resultBuffer.getDataPtr(), outputBufferSize) == 0;
		if (!match)
		{
			log << tcu::TestLog::Message << commandsLog.str() << tcu::TestLog::EndMessage;
			for (deUint32 i = 0; i < outputBufferSize; ++i) {
				if (data[i] != ((deUint8*)resultBuffer.getDataPtr())[i])
				{
					log << tcu::TestLog::Message << "At byte " << i << " data from vkCopyImageToMemoryEXT() is " << data[i] << ", but data from vkCmdCopyImageToBuffer() (after padding) is " << ((deUint8*)resultBuffer.getDataPtr())[i] << tcu::TestLog::EndMessage;
					break;
				}
			}
			return tcu::TestStatus::fail("copyImageToMemoryEXT failed");
		}
	}

	if (m_parameters.imageOutputFormat == m_parameters.imageSampledFormat) {
		std::vector<deUint8> resultData(sampledBufferSize);
		const Allocation& outputAlloc = colorOutputBuffer->getAllocation();
		deMemcpy(resultData.data(), outputAlloc.getHostPtr(), sampledBufferSize);

		for (uint32_t i = 0; i < sampledBufferSize; ++i) {
			if (resultData[i] != testData[i]) {

				if (!isCompressedFormat(m_parameters.imageSampledFormat))
				{
					const tcu::ConstPixelBufferAccess bufferData(mapVkFormat(m_parameters.imageSampledFormat),
						m_parameters.imageSize.width,
						m_parameters.imageSize.height,
						m_parameters.imageSize.depth,
						outputAlloc.getHostPtr());

					m_context.getTestContext().getLog()
						<< tcu::TestLog::Section("host_copy_result", "host_copy_result")
						<< tcu::LogImage("image", "", bufferData)
						<< tcu::TestLog::EndSection;
				}

				return tcu::TestStatus::fail("Image verification failed");
			}
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class HostImageCopyTestCase : public vkt::TestCase
{
public:
								HostImageCopyTestCase	(tcu::TestContext& context, const char* name, const char* description, const TestParameters& parameters)
								: TestCase		(context, name, description)
								, m_parameters	(parameters)
								{
								}
private:
	void						checkSupport			(vkt::Context& context) const;
	void						initPrograms			(vk::SourceCollections& programCollection) const;
	vkt::TestInstance*			createInstance			(vkt::Context& context) const { return new HostImageCopyTestInstance(context, m_parameters); }

	const TestParameters	m_parameters;
};

void HostImageCopyTestCase::checkSupport (vkt::Context& context) const
{
	vk::VkInstance										instance		(context.getInstance());
	vk::InstanceDriver									instanceDriver	(context.getPlatformInterface(), instance);
	const vk::InstanceInterface&						vki				= context.getInstanceInterface();
	vk::VkPhysicalDevice								physicalDevice	= context.getPhysicalDevice();

	context.requireDeviceFunctionality("VK_EXT_host_image_copy");

	if (m_parameters.dynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (m_parameters.sparse)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

	vk::VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT,				// VkStructureType					sType
		DE_NULL,																		// const void*						pNext
		VK_FALSE,																		// VkBool32							hostImageCopy;
	};

	vk::VkPhysicalDeviceFeatures				features;
	deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));
	vk::VkPhysicalDeviceFeatures2				features2 =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,			// VkStructureType					sType
		&hostImageCopyFeatures,										// const void*						pNext
		features													// VkPhysicalDeviceFeatures			features
	};

	instanceDriver.getPhysicalDeviceFeatures2(physicalDevice, &features2);

	vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT,	// VkStructureType	sType;
		DE_NULL,																// void*			pNext;
		0u,																		// uint32_t			copySrcLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopySrcLayouts;
		0u,																		// uint32_t			copyDstLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopyDstLayouts;
		{},																		// uint8_t			optimalTilingLayoutUUID[VK_UUID_SIZE];
		DE_FALSE																// VkBool32			identicalMemoryTypeRequirements;
	};
	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
	std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
	std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
	hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
	hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
	bool layoutSupported = false;
	bool intermediateLayoutSupported = false;
	for (deUint32 i = 0; i < hostImageCopyProperties.copySrcLayoutCount; ++i)
	{
		if (hostImageCopyProperties.pCopySrcLayouts[i] == m_parameters.srcLayout)
			layoutSupported = true;
		if (hostImageCopyProperties.pCopySrcLayouts[i] == m_parameters.intermediateLayout)
			intermediateLayoutSupported = true;
	}
	if (layoutSupported == false || intermediateLayoutSupported == false)
		TCU_THROW(NotSupportedError, "Layout not supported for src host copy");
	layoutSupported = false;
	for (deUint32 i = 0; i < hostImageCopyProperties.copyDstLayoutCount; ++i)
	{
		if (hostImageCopyProperties.pCopyDstLayouts[i] == m_parameters.dstLayout)
		{
			layoutSupported = true;
			break;
		}
	}
	if (layoutSupported == false)
		TCU_THROW(NotSupportedError, "Layout not supported for dst host copy");

	vk::VkImageUsageFlags	usage = vk::VK_IMAGE_USAGE_SAMPLED_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (m_parameters.hostCopyMemoryToImage || m_parameters.hostCopyImageToMemory || m_parameters.memcpyFlag || m_parameters.hostTransferLayout)
		usage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
	if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		usage |= vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		usage |= vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	else if (m_parameters.intermediateLayout == vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		usage |= vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	vk::VkImageCreateFlags	flags = 0u;
	if (m_parameters.sparse)
		flags |= vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	vk::VkImageFormatProperties imageFormatProperties;
	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.imageSampledFormat, vk::VK_IMAGE_TYPE_2D,
		m_parameters.sampledTiling, usage, flags,
		&imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Image format not supported.");

	vk::VkImageUsageFlags outputUsage = vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (m_parameters.outputImageHostTransition || m_parameters.hostCopyImageToMemory)
		outputUsage |= vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT;
	if (m_parameters.command == DISPATCH)
		outputUsage |= vk::VK_IMAGE_USAGE_STORAGE_BIT;
	vk::VkImageFormatProperties outputImageFormatProperties;
	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_parameters.imageOutputFormat, vk::VK_IMAGE_TYPE_2D,
		vk::VK_IMAGE_TILING_OPTIMAL, outputUsage, flags,
		&outputImageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Image format not supported.");

	vk::VkFormatProperties3 formatProperties3 = vk::initVulkanStructure();
	vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
	vki.getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), m_parameters.imageSampledFormat, &formatProperties2);
	if (m_parameters.sampledTiling == vk::VK_IMAGE_TILING_LINEAR && (formatProperties3.linearTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
		TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for linear tiling.");
	if (m_parameters.sampledTiling == vk::VK_IMAGE_TILING_OPTIMAL && (formatProperties3.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
		TCU_THROW(NotSupportedError, "Format feature host image transfer not supported for optimal tiling.");

	if (hostImageCopyFeatures.hostImageCopy != VK_TRUE)
		TCU_THROW(NotSupportedError, "hostImageCopy not supported");
	if (imageFormatProperties.maxMipLevels <= m_parameters.mipLevel)
		TCU_THROW(NotSupportedError, "Required image mip levels not supported.");

	if (m_parameters.command == DISPATCH)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT);
}

void HostImageCopyTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	{
		std::ostringstream vert;
		vert
			<< "#version 450\n"
			<< "layout (location=0) out vec2 texCoord;\n"
			<< "void main()\n"
			<< "{\n"
			<< "	texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);"
			<< "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
			<< "}\n"
			;

		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	}
	{
		std::string output;
		if (isDepthStencilFormat(m_parameters.imageSampledFormat))
			output = "    out_color = vec4(texture(combinedSampler, texCoord).r, 0, 0, 0);\n";
		else
			output = "    out_color = texture(combinedSampler, texCoord);\n";

		std::ostringstream frag;
		frag
			<< "#version 450\n"
			<< "layout (location=0) out vec4 out_color;\n"
			<< "layout (location=0) in vec2 texCoord;\n"
			<< "layout (set=0, binding=0) uniform sampler2D combinedSampler;\n"
			<< "void main()\n"
			<< "{\n"
			<< output
			<< "}\n"
			;

		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
	{
		std::string image;
		std::string output;
		if (m_parameters.imageOutputFormat == vk::VK_FORMAT_R8G8B8A8_UINT)
		{
			image	= "uimage2D";
			output	= "uvec4(texture(combinedSampler, vec2(pixelCoord) / (textureSize(combinedSampler, 0) - vec2(0.001f))) * 255)";
		}
		else
		{
			image = "image2D";
			output = "texture(combinedSampler, vec2(pixelCoord) / (textureSize(combinedSampler, 0) - vec2(0.001f)))";
		}

		std::ostringstream comp;
		comp
			<< "#version 450\n"
			<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
			<< "layout (set=0, binding=0) uniform sampler2D combinedSampler;\n"
			<< "layout (set=0, binding=1) uniform writeonly " << image << " outImage;\n"
			<< "void main()\n"
			<< "{\n"
			<< "	ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);\n"
			<< "	imageStore(outImage, pixelCoord, " << output << ");\n"
			<< "}\n"
			;

		programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
	}
}

class PreinitializedTestInstance : public vkt::TestInstance
{
public:
	PreinitializedTestInstance	(vkt::Context& context, const vk::VkFormat format, vk::VkImageLayout srcLayout, vk::VkImageLayout dstLayout, vk::VkExtent3D size, deUint32 arrayLayers, bool imageToImageCopy, bool memcpy, vk::VkImageTiling tiling, deUint32 offset)
								: vkt::TestInstance		(context)
								, m_format				(format)
								, m_srcLayout			(srcLayout)
								, m_dstLayout			(dstLayout)
								, m_size				(size)
								, m_arrayLayers			(arrayLayers)
								, m_imageToImageCopy	(imageToImageCopy)
								, m_memcpy				(memcpy)
								, m_tiling				(tiling)
								, m_offset				(offset)
								{
								}

private:
	tcu::TestStatus				iterate			(void);

	const vk::VkFormat			m_format;
	const vk::VkImageLayout		m_srcLayout;
	const vk::VkImageLayout		m_dstLayout;
	const vk::VkExtent3D		m_size;
	const deUint32				m_arrayLayers;
	const bool					m_imageToImageCopy;
	const bool					m_memcpy;
	const vk::VkImageTiling		m_tiling;
	const deUint32				m_offset;
};

tcu::TestStatus PreinitializedTestInstance::iterate (void)
{
	vk::InstanceDriver				instanceDriver		(m_context.getPlatformInterface(), m_context.getInstance());
	vk::VkPhysicalDevice			physicalDevice		= m_context.getPhysicalDevice();
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const vk::VkDevice				device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue				queue				= m_context.getUniversalQueue();
	auto&							alloc				= m_context.getDefaultAllocator();
	tcu::TestLog&					log					= m_context.getTestContext().getLog();

	const auto						subresourceRange	= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_arrayLayers);
	const auto						subresourceLayers	= makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_arrayLayers);
	const vk::VkOffset3D			offset				= { 0u, 0u, 0u };

	const auto						channelSize			= getChannelSize(m_format);
	const auto						numChannels			= getNumChannels(m_format);
	const auto						bufferCount			= m_size.width * m_size.height * m_size.depth * m_arrayLayers * numChannels;
	const auto						bufferSize			= bufferCount * channelSize;

	const vk::VkCommandPoolCreateInfo	cmdPoolInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// sType
		DE_NULL,												// pNext
		vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// flags
		queueFamilyIndex,										// queuefamilyindex
	};
	const Move	<vk::VkCommandPool>					cmdPool				(createCommandPool(vk, device, &cmdPoolInfo));
	const Move	<vk::VkCommandBuffer>				cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const vk::SimpleAllocator::OptionalOffsetParams	offsetParams		({ m_context.getDeviceProperties().limits.nonCoherentAtomSize, m_offset });
	de::MovePtr<Allocator>							allocatorWithOffset = de::MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()), offsetParams));

	const vk::VkImageType							imageType			= m_size.depth > 1 ? vk::VK_IMAGE_TYPE_3D : vk::VK_IMAGE_TYPE_2D;

	deUint64										modifier = 0;
	checkSupportedFormatFeatures(instanceDriver, physicalDevice, m_format, m_tiling, &modifier);

	vk::VkImageDrmFormatModifierListCreateInfoEXT	drmCreateInfo	= vk::initVulkanStructure();
	drmCreateInfo.drmFormatModifierCount					= 1;
	drmCreateInfo.pDrmFormatModifiers						= &modifier;

	vk::VkImageCreateInfo				createInfo			=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		m_tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ? &drmCreateInfo : DE_NULL,
													// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		imageType,									// VkImageType				imageType
		m_format,									// VkFormat					format
		m_size,										// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		m_arrayLayers,								// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		m_tiling,									// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
													// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_PREINITIALIZED			// VkImageLayout			initialLayout
	};

	de::MovePtr<ImageWithMemory>	image				= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, *allocatorWithOffset, createInfo, vk::MemoryRequirement::HostVisible));
	de::MovePtr<ImageWithMemory>	copyImage			= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, *allocatorWithOffset, createInfo, vk::MemoryRequirement::Any));
	const vk::VkImage				endImage			= m_imageToImageCopy ? **copyImage : **image;
	de::MovePtr<BufferWithMemory>	outputBuffer		= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, alloc, makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

	vk::Allocation&					allocation			= image->getAllocation();
	void*							ptr					= allocation.getHostPtr();
	generateData(ptr, bufferSize, m_format);

	vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT,	// VkStructureType	sType;
		DE_NULL,																// void*			pNext;
		0u,																		// uint32_t			copySrcLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopySrcLayouts;
		0u,																		// uint32_t			copyDstLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopyDstLayouts;
		{},																		// uint8_t			optimalTilingLayoutUUID[VK_UUID_SIZE];
		DE_FALSE,																// VkBool32			identicalMemoryTypeRequirements;
	};
	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
	if (hostImageCopyProperties.identicalMemoryTypeRequirements)
	{
		createInfo.flags &= ~(vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT);
		de::MovePtr<ImageWithMemory>	imageWithoutHostCopy = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::HostVisible));
		vk::VkMemoryRequirements hostImageMemoryRequirements;
		vk::VkMemoryRequirements memoryRequirements;
		vk.getImageMemoryRequirements(device, **image, &hostImageMemoryRequirements);
		vk.getImageMemoryRequirements(device, **imageWithoutHostCopy, &memoryRequirements);

		if (hostImageMemoryRequirements.memoryTypeBits != memoryRequirements.memoryTypeBits)
			TCU_THROW(NotSupportedError, "Layout not supported for src host copy");
	}

	// map device memory and initialize
	{
		const vk::VkHostImageLayoutTransitionInfoEXT transition =
		{
			vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,		// VkStructureType			sType;
			DE_NULL,															// const void*				pNext;
			image->get(),														// VkImage					image;
			vk::VK_IMAGE_LAYOUT_PREINITIALIZED,									// VkImageLayout			oldLayout;
			m_srcLayout,														// VkImageLayout			newLayout;
			subresourceRange													// VkImageSubresourceRange	subresourceRange;
		};
		vk.transitionImageLayoutEXT(device, 1, &transition);
	}

	if (m_imageToImageCopy)
	{
		vk::VkHostImageLayoutTransitionInfoEXT transition =
		{
			vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,		// VkStructureType			sType;
			DE_NULL,															// const void*				pNext;
			copyImage->get(),													// VkImage					image;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout			oldLayout;
			m_dstLayout,														// VkImageLayout			newLayout;
			subresourceRange													// VkImageSubresourceRange	subresourceRange;
		};
		vk.transitionImageLayoutEXT(device, 1, &transition);

		const vk::VkImageCopy2KHR	region =
		{
			vk::VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR,		// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			subresourceLayers,							// VkImageSubresourceLayers		srcSubresource;
			offset,										// VkOffset3D					srcOffset;
			subresourceLayers,							// VkImageSubresourceLayers		dstSubresource;
			offset,										// VkOffset3D					dstOffset;
			m_size										// VkExtent3D					extent;
		};

		const vk::VkHostImageCopyFlagsEXT	hostImageCopyFlags		= m_memcpy ? (vk::VkHostImageCopyFlagsEXT)vk::VK_HOST_IMAGE_COPY_MEMCPY_EXT : (vk::VkHostImageCopyFlagsEXT)0u;

		const vk::VkCopyImageToImageInfoEXT copyImageToImageInfo	=
		{
			vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO_EXT,			// VkStructureType			sType;
			DE_NULL,													// const void*				pNext;
			hostImageCopyFlags,											// VkHostImageCopyFlagsEXT	flags;
			**image,													// VkImage					srcImage;
			m_srcLayout,												// VkImageLayout			srcImageLayout;
			**copyImage,												// VkImage					dstImage;
			m_dstLayout,												// VkImageLayout			dstImageLayout;
			1u,															// uint32_t					regionCount;
			&region,													// const VkImageCopy2*		pRegions;
		};

		vk.copyImageToImageEXT(device, &copyImageToImageInfo);

		transition.oldLayout = m_dstLayout;
		transition.newLayout = m_srcLayout;
		vk.transitionImageLayoutEXT(device, 1, &transition);
	}

	deUint8* data = new deUint8[bufferSize];

	const vk::VkImageToMemoryCopyEXT region =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		data,											// void*						memoryHostPointer;
		0u,												// uint32_t						memoryRowLength;
		0u,												// uint32_t						memoryImageHeight;
		subresourceLayers,								// VkImageSubresourceLayers		imageSubresource;
		offset,											// VkOffset3D					imageOffset;
		m_size,											// VkExtent3D					imageExtent;
	};

	const vk::VkCopyImageToMemoryInfoEXT copyImageToMemoryInfo =
	{
		vk::VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT,	// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		0u,														// VkMemoryImageCopyFlagsEXT		flags;
		endImage,												// VkImage							srcImage;
		m_srcLayout,											// VkImageLayout					srcImageLayout;
		1,														// uint32_t							regionCount;
		&region,												// const VkImageToMemoryCopyEXT*	pRegions;
	};
	vk.copyImageToMemoryEXT(device, &copyImageToMemoryInfo);

	vk::beginCommandBuffer(vk, *cmdBuffer);
	{
		const vk::VkHostImageLayoutTransitionInfoEXT transition =
		{
			vk::VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,		// VkStructureType			sType;
			DE_NULL,															// const void*				pNext;
			**image,															// VkImage					image;
			m_srcLayout,														// VkImageLayout			oldLayout;
			vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,							// VkImageLayout			newLayout;
			subresourceRange													// VkImageSubresourceRange subresourceRange;
		};
		vk.transitionImageLayoutEXT(device, 1, &transition);

		const vk::VkBufferImageCopy	copyRegion	=
		{
			0u,												// VkDeviceSize				bufferOffset;
			0u,												// deUint32					bufferRowLength;
			0u,												// deUint32					bufferImageHeight;
			subresourceLayers,								// VkImageSubresourceLayers	imageSubresource;
			offset,											// VkOffset3D				imageOffset;
			m_size											// VkExtent3D				imageExtent;
		};
		vk.cmdCopyImageToBuffer(*cmdBuffer, **image, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **outputBuffer, 1u, &copyRegion);
	}
	vk::endCommandBuffer(vk, *cmdBuffer);

	vk::submitCommandsAndWait(vk, device, queue, cmdBuffer.get());
	auto outputPtr = outputBuffer->getAllocation().getHostPtr();
	bool match = memcmp(data, outputPtr, bufferSize) == 0;

	if (!match)
	{
		for (deUint32 i = 0; i < bufferSize; ++i) {
			if (data[i] != ((deUint8*)outputPtr)[i])
			{
				log << tcu::TestLog::Message << "At byte " << i << " data from vkCopyImageToMemoryEXT() is " << data[i] << ", but data from vkCmdCopyImageToBuffer() is " << ((deUint8*)outputPtr)[i] << tcu::TestLog::EndMessage;
				break;
			}
		}
	}

	delete[] data;

	if (!match)
	{
		return tcu::TestStatus::fail("Copies values do not match");
	}

	return tcu::TestStatus::pass("Pass");
}

class PreinitializedTestCase : public vkt::TestCase
{
public:
	PreinitializedTestCase						(tcu::TestContext& context, const char* name, const char* description, const vk::VkFormat format, vk::VkImageLayout srcLayout, vk::VkImageLayout dstLayout, vk::VkExtent3D size, deUint32 arrayLayers, bool imageToImageCopy, bool memcpy, vk::VkImageTiling tiling, deUint32 offset)
												: TestCase				(context, name, description)
												, m_format				(format)
												, m_srcLayout			(srcLayout)
												, m_dstLayout			(dstLayout)
												, m_size				(size)
												, m_arrayLayers			(arrayLayers)
												, m_imageToImageCopy	(imageToImageCopy)
												, m_memcpy				(memcpy)
												, m_tiling				(tiling)
												, m_offset				(offset)
												{
												}
private:
	void						checkSupport	(vkt::Context& context) const;
	vkt::TestInstance*			createInstance	(vkt::Context& context) const { return new PreinitializedTestInstance(context, m_format, m_srcLayout, m_dstLayout, m_size, m_arrayLayers, m_imageToImageCopy, m_memcpy, m_tiling, m_offset); }

	const vk::VkFormat			m_format;
	const vk::VkImageLayout		m_srcLayout;
	const vk::VkImageLayout		m_dstLayout;
	const vk::VkExtent3D		m_size;
	const deUint32				m_arrayLayers;
	const bool					m_imageToImageCopy;
	const bool					m_memcpy;
	const vk::VkImageTiling		m_tiling;
	const deUint32				m_offset;
};

void PreinitializedTestCase::checkSupport (vkt::Context& context) const
{
	vk::VkInstance							instance		(context.getInstance());
	vk::InstanceDriver						instanceDriver	(context.getPlatformInterface(), instance);
	const InstanceInterface&				vki				= context.getInstanceInterface();
	vk::VkPhysicalDevice					physicalDevice	= context.getPhysicalDevice();

	context.requireDeviceFunctionality("VK_EXT_host_image_copy");

	if (m_tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
		context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");

	if (m_srcLayout == vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR || m_dstLayout == vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		context.requireDeviceFunctionality("VK_KHR_swapchain");

	if (m_srcLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
		m_srcLayout == vk::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR || m_dstLayout == vk::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR)
		context.requireDeviceFunctionality("VK_KHR_maintenance2");

	if (m_srcLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL ||
		m_srcLayout == vk::VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL ||
		m_srcLayout == vk::VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL)
		context.requireDeviceFunctionality("VK_KHR_separate_depth_stencil_layouts");

	if (m_srcLayout == vk::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL ||
		m_srcLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL || m_dstLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
		context.requireDeviceFunctionality("VK_KHR_synchronization2");

	if (m_srcLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT || m_dstLayout == vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT)
		context.requireDeviceFunctionality("VK_EXT_attachment_feedback_loop_layout");

	vk::VkPhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT,				// VkStructureType					sType
		DE_NULL,																		// const void*						pNext
		VK_FALSE,																		// VkBool32							hostImageCopy;
	};

	vk::VkPhysicalDeviceFeatures				features;
	deMemset(&features, 0, sizeof(vk::VkPhysicalDeviceFeatures));
	vk::VkPhysicalDeviceFeatures2				features2 =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,			// VkStructureType					sType
		&hostImageCopyFeatures,										// const void*						pNext
		features													// VkPhysicalDeviceFeatures			features
	};

	instanceDriver.getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT,	// VkStructureType	sType;
		DE_NULL,																// void*			pNext;
		0u,																		// uint32_t			copySrcLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopySrcLayouts;
		0u,																		// uint32_t			copyDstLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopyDstLayouts;
		{},																		// uint8_t			optimalTilingLayoutUUID[VK_UUID_SIZE];
		DE_FALSE																// VkBool32			identicalMemoryTypeRequirements;
	};

	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
	std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
	std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
	hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
	hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);

	bool layoutSupported = false;
	for (deUint32 i = 0; i < hostImageCopyProperties.copySrcLayoutCount; ++i)
	{
		if (hostImageCopyProperties.pCopySrcLayouts[i] == m_srcLayout)
			layoutSupported = true;
	}
	if (layoutSupported == false)
		TCU_THROW(NotSupportedError, "Layout not supported for src host copy");
	layoutSupported = false;
	for (deUint32 i = 0; i < hostImageCopyProperties.copyDstLayoutCount; ++i)
	{
		if (hostImageCopyProperties.pCopyDstLayouts[i] == m_dstLayout)
			layoutSupported = true;
	}
	if (layoutSupported == false)
		TCU_THROW(NotSupportedError, "Layout not supported for dst host copy");

	if (hostImageCopyFeatures.hostImageCopy != VK_TRUE)
		TCU_THROW(NotSupportedError, "hostImageCopy not supported");

	deUint64 modifier = 0;
	checkSupportedFormatFeatures(instanceDriver, physicalDevice, m_format, m_tiling, &modifier);

	vk::VkImageType const imageType = m_size.depth > 1 ? vk::VK_IMAGE_TYPE_3D : vk::VK_IMAGE_TYPE_2D;
	vk::VkImageFormatProperties2 imageFormatProperties =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,	// VkStructureType			sType;
		DE_NULL,											// void* pNext;
		{},													// VkImageFormatProperties	imageFormatProperties;
	};
	vk::VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo = {
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,			// VkStructureType	sType;
		DE_NULL,																			// const void*		pNext;
		modifier,																			// uint64_t			drmFormatModifier;
		VK_SHARING_MODE_EXCLUSIVE,															// VkSharingMode	sharingMode;
		0u,																					// uint32_t			queueFamilyIndexCount;
		DE_NULL																				// const uint32_t*	pQueueFamilyIndices;
	};
	vk::VkPhysicalDeviceImageFormatInfo2 imageFormatInfo =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,							// VkStructureType		sType;
		m_tiling == vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT ? &modifierInfo : DE_NULL,	// const void*			pNext;
		m_format,																			// VkFormat				format;
		imageType,																			// VkImageType			type;
		m_tiling,																			// VkImageTiling		tiling;
		vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags	usage;
		(vk::VkImageCreateFlags)0u															// VkImageCreateFlags	flags;
	};
	if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Image format not supported.");
	if (imageFormatProperties.imageFormatProperties.maxArrayLayers < m_arrayLayers)
		TCU_THROW(NotSupportedError, "Required image array layers not supported.");
}

class PropertiesTestInstance : public vkt::TestInstance
{
public:
	PropertiesTestInstance (vkt::Context& context)
		: vkt::TestInstance (context)
	{
	}

private:
	tcu::TestStatus				iterate			(void);
};

tcu::TestStatus PropertiesTestInstance::iterate (void) {
	vk::VkInstance										instance		(m_context.getInstance());
	vk::InstanceDriver									instanceDriver	(m_context.getPlatformInterface(), instance);
	vk::VkPhysicalDevice								physicalDevice	= m_context.getPhysicalDevice();

	vk::VkPhysicalDeviceHostImageCopyPropertiesEXT hostImageCopyProperties =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT,	// VkStructureType	sType;
		DE_NULL,																// void*			pNext;
		0u,																		// uint32_t			copySrcLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopySrcLayouts;
		0u,																		// uint32_t			copyDstLayoutCount;
		DE_NULL,																// VkImageLayout*	pCopyDstLayouts;
		{},																		// uint8_t			optimalTilingLayoutUUID[VK_UUID_SIZE];
		DE_FALSE																// VkBool32			identicalMemoryTypeRequirements;
	};
	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);
	std::vector<vk::VkImageLayout> srcLayouts(hostImageCopyProperties.copySrcLayoutCount);
	std::vector<vk::VkImageLayout> dstLayouts(hostImageCopyProperties.copyDstLayoutCount);
	hostImageCopyProperties.pCopySrcLayouts = srcLayouts.data();
	hostImageCopyProperties.pCopyDstLayouts = dstLayouts.data();
	getHostImageCopyProperties(instanceDriver, physicalDevice, &hostImageCopyProperties);

	if (hostImageCopyProperties.copySrcLayoutCount == 0)
		return tcu::TestStatus::fail("copySrcLayoutCount is 0");
	if (hostImageCopyProperties.copyDstLayoutCount == 0)
		return tcu::TestStatus::fail("copyDstLayoutCount is 0");

	bool layoutSupported = false;
	for (deUint32 i = 0; i < hostImageCopyProperties.copySrcLayoutCount; ++i)
	{
		if (hostImageCopyProperties.pCopySrcLayouts[i] == vk::VK_IMAGE_LAYOUT_GENERAL)
			layoutSupported = true;
	}
	if (layoutSupported == false)
		return tcu::TestStatus::fail("VK_IMAGE_LAYOUT_GENERAL not supported for src host copy");
	layoutSupported = false;
	for (deUint32 i = 0; i < hostImageCopyProperties.copyDstLayoutCount; ++i)
	{
		if (hostImageCopyProperties.pCopyDstLayouts[i] == vk::VK_IMAGE_LAYOUT_GENERAL)
			layoutSupported = true;
	}
	if (layoutSupported == false)
		return tcu::TestStatus::fail("VK_IMAGE_LAYOUT_GENERAL not supported for dst host copy");

	return tcu::TestStatus::pass("Pass");
}

class PropertiesTestCase : public vkt::TestCase
{
public:
	PropertiesTestCase (tcu::TestContext& context, const char* name, const char* description)
		: TestCase (context, name, description)
	{
	}
private:
	vkt::TestInstance*			createInstance	(vkt::Context& context) const { return new PropertiesTestInstance(context); }
	void						checkSupport	(vkt::Context& context) const;
};

void PropertiesTestCase::checkSupport (vkt::Context& context) const {
	context.requireDeviceFunctionality("VK_EXT_host_image_copy");
}

class QueryTestInstance : public vkt::TestInstance
{
public:
	QueryTestInstance (vkt::Context& context, const vk::VkFormat format, const vk::VkImageTiling tiling)
		: vkt::TestInstance (context)
		, m_format			(format)
		, m_tiling			(tiling)
	{
	}

private:
	tcu::TestStatus				iterate			(void);

	const vk::VkFormat			m_format;
	const vk::VkImageTiling		m_tiling;
};

tcu::TestStatus QueryTestInstance::iterate (void)
{
	const InstanceInterface&			vki					= m_context.getInstanceInterface();
	const vk::VkPhysicalDevice			physicalDevice		= m_context.getPhysicalDevice();
	tcu::TestLog&						log					= m_context.getTestContext().getLog();

	const vk::VkPhysicalDeviceImageFormatInfo2			imageFormatInfo =
	{
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	// VkStructureType		sType;
		DE_NULL,													// const void*			pNext;
		m_format,													// VkFormat				format;
		vk::VK_IMAGE_TYPE_2D,										// VkImageType			type;
		m_tiling,													// VkImageTiling		tiling;
		vk::VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,					// VkImageUsageFlags	usage;
		(VkImageCreateFlags)0u										// VkImageCreateFlags	flags;
	};

	vk::VkHostImageCopyDevicePerformanceQueryEXT		hostImageCopyDevicePerformanceQuery = vk::initVulkanStructure();
	vk::VkImageFormatProperties2						imageFormatProperties				= vk::initVulkanStructure(&hostImageCopyDevicePerformanceQuery);
	vk::VkResult res = vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageFormatProperties);

	if (hostImageCopyDevicePerformanceQuery.identicalMemoryLayout == VK_FALSE) {
		if (hostImageCopyDevicePerformanceQuery.optimalDeviceAccess != VK_FALSE)
		{
			log << tcu::TestLog::Message << "VkHostImageCopyDevicePerformanceQueryEXT::identicalMemoryLayout is VK_FALSE, but VkHostImageCopyDevicePerformanceQueryEXT::optimalDeviceAccess is VK_TRUE" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}
	else
	{
		if (hostImageCopyDevicePerformanceQuery.optimalDeviceAccess != VK_TRUE)
		{
			log << tcu::TestLog::Message << "VkHostImageCopyDevicePerformanceQueryEXT::identicalMemoryLayout is VK_TRUE, but VkHostImageCopyDevicePerformanceQueryEXT::optimalDeviceAccess is VK_FALSE" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	if (isBlockCompressedFormat(m_format) && res == vk::VK_SUCCESS)
	{
		if (hostImageCopyDevicePerformanceQuery.optimalDeviceAccess != VK_TRUE)
		{
			log << tcu::TestLog::Message << "Format is a block compressed format and vkGetPhysicalDeviceImageFormatProperties2 returned VK_SUCCESS, but VkHostImageCopyDevicePerformanceQueryEXT::optimalDeviceAccess is VK_FALSE" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	if (!vk::isDepthStencilFormat(m_format))
	{
		vk::VkFormatProperties3	formatProperties3 = vk::initVulkanStructure();
		vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
		vki.getPhysicalDeviceFormatProperties2(physicalDevice, m_format, &formatProperties2);

		if (m_tiling == VK_IMAGE_TILING_OPTIMAL)
		{
			if ((formatProperties3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
			{
				log << tcu::TestLog::Message << "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT is supported in optimalTilingFeatures for format " << vk::getFormatStr(m_format).toString() << ", but VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT is not" << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT not supported");
			}
		}
		else if (m_tiling == VK_IMAGE_TILING_LINEAR)
		{
			if ((formatProperties3.linearTilingFeatures & VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT) == 0)
			{
				log << tcu::TestLog::Message << "VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT is supported in linearTilingFeatures for format " << vk::getFormatStr(m_format).toString() << ", but VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT is not" << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT not supported");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class QueryTestCase : public vkt::TestCase
{
public:
	QueryTestCase(tcu::TestContext& context, const char* name, const char* description, const vk::VkFormat format, const vk::VkImageTiling tiling)
		: TestCase (context, name, description)
		, m_format (format)
		, m_tiling (tiling)
	{
	}
private:
	vkt::TestInstance*			createInstance	(vkt::Context& context) const { return new QueryTestInstance(context, m_format, m_tiling); }
	void						checkSupport	(vkt::Context& context) const;

	const vk::VkFormat			m_format;
	const vk::VkImageTiling		m_tiling;
};

void QueryTestCase::checkSupport (vkt::Context& context) const {
	const InstanceInterface&				vki				= context.getInstanceInterface();

	context.requireDeviceFunctionality("VK_EXT_host_image_copy");

	vk::VkFormatProperties3 formatProperties3 = vk::initVulkanStructure();
	vk::VkFormatProperties2 formatProperties2 = vk::initVulkanStructure(&formatProperties3);
	vki.getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), m_format, &formatProperties2);
	if (m_tiling == VK_IMAGE_TILING_OPTIMAL && (formatProperties3.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Format feature sampled image bit not supported for optimal tiling.");
	if (m_tiling == VK_IMAGE_TILING_LINEAR && (formatProperties3.linearTilingFeatures & vk::VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT) == 0)
		TCU_THROW(NotSupportedError, "Format feature sampled image bit not supported for linear tiling.");
}

void testGenerator(tcu::TestCaseGroup* group)
{
	constexpr struct CopyTest
	{
		bool				hostTransferLayout;
		bool				copyMemoryToImage;
		const char* name;
		const char* desc;
	} copyTests[] =
	{
		{ true,		true,	"host_transfer_copy_general",	"Host copy and transfer"		},
		{ true,		false,	"host_transfer",				"Host transfer"					},
		{ false,	true,	"host_copy",					"Host copy"						},
	};

	constexpr struct CopyImageToMemory
	{
		bool			hostCopyImageToMemory;
		const char* name;
		const char* desc;
	} copyImageToMemoryTests[] = {
		{ true,		"host_image_to_memory_copy",		"Copy from image to memory on host"	},
		{ false,	"image_to_memory_copy",				"Copy from image to memory on gpu"	},
	};

	constexpr struct TransitionTest
	{
		bool		host;
		const char* name;
		const char* desc;
	} transitionTests[] =
	{
		{ true,		"host_transition",		"Transition using vkTransitionImageLayoutEXT"	},
		{ false,	"barrier_transition",	"Transition using a pipeline barrier"			},
	};

	const struct FlagsTest
	{
		bool		memcpy;
		const char* name;
		const char* desc;
	} flagsTests[] =
	{
		{ false,	"none",		"Copy with no flags"							},
		{ true,		"memcpy",	"Copy with VK_HOST_IMAGE_COPY_MEMCPY_EXT flag"	},
	};

	const struct Tiling {
		vk::VkImageTiling	tiling;
		const char*			name;
		const char*			desc;
	} tilingTests[] = {
		{ vk::VK_IMAGE_TILING_LINEAR,	"linear",			"Linear tiling"		},
		{ vk::VK_IMAGE_TILING_OPTIMAL,	"optimal",			"Optimal tiling"	},
	};

	const struct ImageFormatsAndCommand {
		Command			command;
		vk::VkFormat	sampled;
		vk::VkFormat	output;
	} formatsAndCommands[] = {
		{ DRAW,		vk::VK_FORMAT_R8G8B8A8_UNORM,				vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ DRAW,		vk::VK_FORMAT_R32G32B32A32_SFLOAT,			vk::VK_FORMAT_R32G32B32A32_SFLOAT	},
		{ DRAW,		vk::VK_FORMAT_R16_UNORM,					vk::VK_FORMAT_R16_UNORM				},
		{ DRAW,		vk::VK_FORMAT_D16_UNORM,					vk::VK_FORMAT_R16_UNORM				},
		{ DRAW,		vk::VK_FORMAT_D32_SFLOAT,					vk::VK_FORMAT_R32_SFLOAT			},
		{ DRAW,		vk::VK_FORMAT_BC7_UNORM_BLOCK,				vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ DRAW,		vk::VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,	vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ DRAW,		vk::VK_FORMAT_ASTC_4x4_UNORM_BLOCK,			vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ DISPATCH,	vk::VK_FORMAT_R10X6_UNORM_PACK16,			vk::VK_FORMAT_R10X6_UNORM_PACK16	},
		{ DISPATCH,	vk::VK_FORMAT_R8G8B8A8_UNORM,				vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ DISPATCH,	vk::VK_FORMAT_R8G8B8A8_UNORM,				vk::VK_FORMAT_R8G8B8A8_UINT			},
	};

	const struct ImageSizes
	{
		vk::VkExtent3D	size;
		const char* name;
		const char* desc;
	} imageSizes[] =
	{
		{ makeExtent3D(16u, 16u, 1u),			"16x16",		"Size of image"	},
		{ makeExtent3D(32u, 28u, 1u),			"32x28",		"Size of image"	},
		{ makeExtent3D(53u, 61u, 1u),			"53x61",		"Size of image"	},
	};

	constexpr struct ImageLayoutTest
	{
		vk::VkImageLayout srcLayout;
		vk::VkImageLayout dstLayout;
		const char* name;
		const char* desc;
	} imageLayoutTests[] =
	{
		{ vk::VK_IMAGE_LAYOUT_GENERAL,				vk::VK_IMAGE_LAYOUT_GENERAL,					"general_general",				"Src and dst copy layouts"	},
		{ vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		"transfer_src_transfer_dst",	"Src and dst copy layouts"	},
	};

	constexpr struct IntermediateImageLayoutTest
	{
		vk::VkImageLayout layout;
		const char* name;
		const char* desc;
	} intermediateImageLayoutTests[] =
	{
		{ vk::VK_IMAGE_LAYOUT_GENERAL,							"general",							"intermediate layout"	},
		{ vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			"color_attachment_optimal",			"intermediate layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	"depth_stencil_attachment_optimal",	"intermediate layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,	"depth_stencil_read_only_optimal",	"intermediate layout"	},
		{ vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,			"shader_read_only_optimal",			"intermediate layout"	},
		{ vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,				"transfer_src_optimal",				"intermediate layout"	},
		{ vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				"transfer_dst_optimal",				"intermediate layout"	},
	};

	constexpr struct MipLevelRegionCountPaddingTest
	{
		deUint32	mipLevel;
		deUint32	regionsCount;
		deUint32	padding;
		const char* name;
		const char* desc;
	} mipLevelRegionCountPaddingTests[] =
	{
		{	0u,		1,		0u,		"0_1_0",	""	},
		{	1u,		1,		0u,		"1_1_0",	""	},
		{	4u,		1,		0u,		"4_1_0",	""	},
		{	0u,		4,		4u,		"0_4_4",	""	},
		{	0u,		16,		64u,	"0_16_64",	""	},
	};

	tcu::TestContext& testCtx = group->getTestContext();

	for (const auto& formatAndCommand : formatsAndCommands)
	{
		std::string formatName = formatAndCommand.command == DRAW ? "draw" : "dispatch";
		formatName += "_" + getFormatShortString(formatAndCommand.output) + "_" + getFormatShortString(formatAndCommand.sampled);
		tcu::TestCaseGroup* const formatGroup = new tcu::TestCaseGroup(testCtx, formatName.c_str(), "image formats");

		bool colorFormat = isCompressedFormat(formatAndCommand.sampled) ||
			!(tcu::hasDepthComponent(mapVkFormat(formatAndCommand.sampled).order) || tcu::hasDepthComponent(mapVkFormat(formatAndCommand.sampled).order));

		bool dynamicRenderingBase = true;
		bool sparseImageBase = true;

		for (const auto& copy : copyTests)
		{
			// Anitalias the config stride!
			dynamicRenderingBase = !dynamicRenderingBase;
			bool dynamicRendering = dynamicRenderingBase;

			tcu::TestCaseGroup* const copyTestGroup = new tcu::TestCaseGroup(testCtx, copy.name, copy.desc);
			for (const auto& imageToMemory : copyImageToMemoryTests)
			{
				tcu::TestCaseGroup* const imageToMemoryGroup = new tcu::TestCaseGroup(testCtx, imageToMemory.name, imageToMemory.desc);
				for (const auto& transition : transitionTests)
				{
					tcu::TestCaseGroup* const transitionGroup = new tcu::TestCaseGroup(testCtx, transition.name, transition.desc);
					for (const auto& flags : flagsTests)
					{
						tcu::TestCaseGroup* const flagsGroup = new tcu::TestCaseGroup(testCtx, flags.name, flags.desc);
						for (const auto& layouts : imageLayoutTests)
						{
							tcu::TestCaseGroup* const layoutsGroup = new tcu::TestCaseGroup(testCtx, layouts.name, layouts.desc);
							for (const auto& intermediateLayout : intermediateImageLayoutTests)
							{
								if (colorFormat && (intermediateLayout.layout == vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || intermediateLayout.layout == vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL))
									continue;
								else if (!colorFormat && intermediateLayout.layout == vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
									continue;

								tcu::TestCaseGroup* const intermediateLayoutGroup = new tcu::TestCaseGroup(testCtx, intermediateLayout.name, intermediateLayout.desc);
								for (const auto& tiling : tilingTests)
								{
									tcu::TestCaseGroup* const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name, tiling.desc);
									for (const auto& mipLevelRegionCountPaddingTest : mipLevelRegionCountPaddingTests)
									{
										// We are alternating the sparseImage flag here, make sure that count is not even, otherwise this has to be moved to a different loop
										static_assert(DE_LENGTH_OF_ARRAY(mipLevelRegionCountPaddingTests) % 2 != 0, "Variation count is not odd");
										sparseImageBase = !sparseImageBase;
										bool sparseImage = sparseImageBase;

										tcu::TestCaseGroup* const mipLevelRegionCountPaddingGroup = new tcu::TestCaseGroup(testCtx, mipLevelRegionCountPaddingTest.name, mipLevelRegionCountPaddingTest.desc);
										for (const auto& size : imageSizes)
										{
											// Alternate every test
											dynamicRendering = !dynamicRendering;
											sparseImage = !sparseImage;

											if (sparseImage && isCompressedFormat(formatAndCommand.sampled))
												continue;

											const TestParameters parameters =
											{
												copy.copyMemoryToImage,							// bool				copyMemoryToImage
												imageToMemory.hostCopyImageToMemory,			// bool				hostCopyImageToMemory
												copy.hostTransferLayout,						// bool				hostTransferLayout
												transition.host,								// bool				outputImageHostTransition
												flags.memcpy,									// bool				memcpyFlag
												dynamicRendering,								// bool				dynamicRendering
												formatAndCommand.command,						// Command			command
												formatAndCommand.sampled,						// VkFormat			imageSampledFormat
												layouts.srcLayout,								// VkImageLayout	srcLayout
												layouts.dstLayout,								// VkImageLayout	dstLayout
												intermediateLayout.layout,						// VkImageLayout	intermediateLayout
												tiling.tiling,									// VkImageTiling	sampledTiling;
												formatAndCommand.output,						// VkFormat			imageOutputFormat
												size.size,										// VkExtent3D		imageSize
												sparseImage,										// bool				sparse
												mipLevelRegionCountPaddingTest.mipLevel,		// deUint32			mipLevel
												mipLevelRegionCountPaddingTest.regionsCount,	// deUint32			regionsCount
												mipLevelRegionCountPaddingTest.padding			// deUint32			padding
											};

											mipLevelRegionCountPaddingGroup->addChild(new HostImageCopyTestCase(testCtx, size.name, size.desc, parameters));
										}
										tilingGroup->addChild(mipLevelRegionCountPaddingGroup);
									}
									intermediateLayoutGroup->addChild(tilingGroup);
								}
								layoutsGroup->addChild(intermediateLayoutGroup);
							}
							flagsGroup->addChild(layoutsGroup);
						}
						transitionGroup->addChild(flagsGroup);
					}
					imageToMemoryGroup->addChild(transitionGroup);
				}
				copyTestGroup->addChild(imageToMemoryGroup);
			}
			formatGroup->addChild(copyTestGroup);
		}
		group->addChild(formatGroup);
	}

	const struct PreinitializedFormats {
		vk::VkFormat format;
	} preinitializedFormats[] = {
		{ vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ vk::VK_FORMAT_R32G32B32A32_SFLOAT },
		{ vk::VK_FORMAT_R16_UNORM			},
		{ vk::VK_FORMAT_R16G16_UINT			},
		{ vk::VK_FORMAT_B8G8R8A8_SINT		},
		{ vk::VK_FORMAT_R16_SFLOAT			},
	};

	const struct PreinitializedTiling {
		vk::VkImageTiling	tiling;
		const char*			name;
		const char*			desc;
	} preinitializedTilingTests[] = {
		{ vk::VK_IMAGE_TILING_LINEAR,					"linear",				"Linear tiling"			},
		{ vk::VK_IMAGE_TILING_OPTIMAL,					"optimal",				"Optimal tiling"		},
		{ vk::VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,	"drm_format_modifier",	"DRM format modifier"	},
	};

	constexpr struct PreinitializedImageLayoutTest
	{
		vk::VkImageLayout layout;
		const char* name;
		const char* desc;
	} preinitializedImageLayoutTests[] =
	{
		{ vk::VK_IMAGE_LAYOUT_GENERAL,										"general",											"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						"color_attachment_optimal",							"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				"depth_stencil_attachment_optimal",					"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,				"depth_stencil_read_only_optimal",					"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,						"shader_read_only_optimal",							"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,							"transfer_src_optimal",								"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,							"transfer_dst_optimal",								"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_PREINITIALIZED,								"preinitialized",									"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,								"present_src",										"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,	"depth_read_only_stencil_attachment_optimal",		"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,	"depth_attachment_stencil_read_only_optimal",		"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,						"depth_read_only_optimal",							"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,					"stencil_attachment_optimal",						"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,					"stencil_read_only_optimal",						"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,							"read_only_optimal",								"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,							"attachment_optimal",								"copy layout"	},
		{ vk::VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT,			"attachment_feedback_loop_optimal",					"copy layout"	},
	};

	constexpr struct ImageToImageTest
	{
		bool		imageToImageCopy;
		bool		memcpy;
		const char* name;
		const char* desc;
	} imageToImageCopyTests[] =
	{
		{ true,		false,	"image_to_image_copy",		"Image to image copy"	},
		{ true,		true,	"image_to_image_memcpy",	"Image to image copy with memcpy flag"	},
		{ false,	false,	"preinitialized",			"Preinitialized image"	},
	};

	constexpr struct ImageSizeTest
	{
		vk::VkExtent3D	size;
		deUint32		layerCount;
		const char* name;
		const char* desc;
	} imageSizeTests[] =
	{
		{ {32, 32, 1},		1,	"32x32x1_1",	"Image size"	},
		{ {32, 32, 1},		2,	"32x32x1_2",	"Image size"	},
		{ {51, 63, 1},		1,	"51x63x1_1",	"Image size"	},
		{ {24, 24, 4},		1,	"24x24x4_4",	"Image size"	},
	};

	constexpr struct OffsetTest
	{
		deUint32	offset;
		const char* name;
		const char* desc;
	} offsetTests[] =
	{
		{ 0u,	"0",	"No offset"	},
		{ 64u,	"64",	"Offset 64"	},
	};

	for (const auto& tiling : preinitializedTilingTests)
	{
		tcu::TestCaseGroup* const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name, tiling.desc);
		for (const auto& imageToImage : imageToImageCopyTests)
		{
			tcu::TestCaseGroup* const imageToImageCopyGroup = new tcu::TestCaseGroup(testCtx, imageToImage.name, imageToImage.desc);
			for (const auto& srcLayout : preinitializedImageLayoutTests)
			{
				tcu::TestCaseGroup* const srcLayoutGroup = new tcu::TestCaseGroup(testCtx, srcLayout.name, srcLayout.desc);
				for (const auto& dstLayout : preinitializedImageLayoutTests)
				{
					tcu::TestCaseGroup* const dstLayoutGroup = new tcu::TestCaseGroup(testCtx, dstLayout.name, dstLayout.desc);
					for (const auto& size : imageSizeTests)
					{
						tcu::TestCaseGroup* const sizeGroup = new tcu::TestCaseGroup(testCtx, size.name, size.desc);
						for (const auto& offset : offsetTests) {
							tcu::TestCaseGroup* const offsetGroup = new tcu::TestCaseGroup(testCtx, offset.name, offset.desc);
							for (const auto& format : preinitializedFormats)
							{
								const auto formatName = getFormatShortString(format.format);
								offsetGroup->addChild(new PreinitializedTestCase(testCtx, formatName.c_str(), "", format.format, srcLayout.layout, dstLayout.layout, size.size, size.layerCount, imageToImage.imageToImageCopy, imageToImage.memcpy, tiling.tiling, offset.offset));
							}
							sizeGroup->addChild(offsetGroup);
						}
						dstLayoutGroup->addChild(sizeGroup);
					}
					srcLayoutGroup->addChild(dstLayoutGroup);
				}
				imageToImageCopyGroup->addChild(srcLayoutGroup);
			}
			tilingGroup->addChild(imageToImageCopyGroup);
		}
		group->addChild(tilingGroup);
	}

	tcu::TestCaseGroup* const propertiesGroup = new tcu::TestCaseGroup(testCtx, "properties", "");
	propertiesGroup->addChild(new PropertiesTestCase(testCtx, "properties", ""));

	const struct QueryFormats {
		vk::VkFormat format;
	} queryFormats[] = {
		{ vk::VK_FORMAT_R8G8B8A8_UNORM		},
		{ vk::VK_FORMAT_R32G32B32A32_SFLOAT },
		{ vk::VK_FORMAT_R16_UNORM			},
		{ vk::VK_FORMAT_R16G16_UINT			},
		{ vk::VK_FORMAT_B8G8R8A8_SINT		},
		{ vk::VK_FORMAT_R16_SFLOAT			},
		{ vk::VK_FORMAT_D24_UNORM_S8_UINT	},
		{ vk::VK_FORMAT_BC7_UNORM_BLOCK		},
		{ vk::VK_FORMAT_BC5_SNORM_BLOCK		},
	};

	group->addChild(propertiesGroup);

	tcu::TestCaseGroup* const queryGroup = new tcu::TestCaseGroup(testCtx, "query", "");

	for (const auto& tiling : tilingTests)
	{
		tcu::TestCaseGroup* const tilingGroup = new tcu::TestCaseGroup(testCtx, tiling.name, tiling.desc);
		for (const auto& format : queryFormats)
		{
			const auto formatName = getFormatShortString(format.format);
			tilingGroup->addChild(new QueryTestCase(testCtx, formatName.c_str(), "", format.format, tiling.tiling));
		}
		queryGroup->addChild(tilingGroup);
	}

	group->addChild(queryGroup);
}

} // anonymous

tcu::TestCaseGroup* createImageHostImageCopyTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (createTestGroup(testCtx, "host_image_copy", "Tests for VK_EXT_host_image_copy", testGenerator));
	return testGroup.release();
}

} // image
} // vkt
