/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Link Tests
 *//*--------------------------------------------------------------------*/

#include "vktShaderObjectRenderingTests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktShaderObjectCreateUtil.hpp"
#include "vkObjUtil.hpp"
#include "deRandom.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"
#include <cmath>

namespace vkt
{
namespace ShaderObject
{

namespace
{

enum ExtraAttachments {
	NONE = 0,
	BEFORE,
	BETWEEN,
	AFTER,
};

enum DummyRenderPass
{
	DUMMY_NONE = 0,
	DUMMY_DYNAMIC,
	DUMMY_STATIC,
};

struct TestParams {
	deUint32 colorAttachmentCount;
	deUint32 extraAttachmentCount;
	ExtraAttachments extraAttachments;
	deUint32 extraFragmentOutputCount;
	ExtraAttachments extraOutputs;
	bool useDepthAttachment;
	vk::VkFormat colorFormat;
	vk::VkFormat depthFormat;
	bool bindShadersBeforeBeginRendering;
	DummyRenderPass dummyRenderPass;
	bool writeGlFragDepth;
	bool randomColorFormats;
};

const vk::VkFormat colorFormats[] =
{
	vk::VK_FORMAT_R4G4_UNORM_PACK8,
	vk::VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	vk::VK_FORMAT_B4G4R4A4_UNORM_PACK16,
	vk::VK_FORMAT_R5G6B5_UNORM_PACK16,
	vk::VK_FORMAT_B5G6R5_UNORM_PACK16,
	vk::VK_FORMAT_R5G5B5A1_UNORM_PACK16,
	vk::VK_FORMAT_B5G5R5A1_UNORM_PACK16,
	vk::VK_FORMAT_A1R5G5B5_UNORM_PACK16,
	vk::VK_FORMAT_R8_UNORM,
	vk::VK_FORMAT_R8_SNORM,
	vk::VK_FORMAT_R8_USCALED,
	vk::VK_FORMAT_R8_SSCALED,
	vk::VK_FORMAT_R8_UINT,
	vk::VK_FORMAT_R8_SINT,
	vk::VK_FORMAT_R8_SRGB,
	vk::VK_FORMAT_R8G8_UNORM,
	vk::VK_FORMAT_R8G8_SNORM,
	vk::VK_FORMAT_R8G8_USCALED,
	vk::VK_FORMAT_R8G8_SSCALED,
	vk::VK_FORMAT_R8G8_UINT,
	vk::VK_FORMAT_R8G8_SINT,
	vk::VK_FORMAT_R8G8_SRGB,
	vk::VK_FORMAT_R8G8B8_UNORM,
	vk::VK_FORMAT_R8G8B8_SNORM,
	vk::VK_FORMAT_R8G8B8_USCALED,
	vk::VK_FORMAT_R8G8B8_SSCALED,
	vk::VK_FORMAT_R8G8B8_UINT,
	vk::VK_FORMAT_R8G8B8_SINT,
	vk::VK_FORMAT_R8G8B8_SRGB,
	vk::VK_FORMAT_B8G8R8_UNORM,
	vk::VK_FORMAT_B8G8R8_SNORM,
	vk::VK_FORMAT_B8G8R8_USCALED,
	vk::VK_FORMAT_B8G8R8_SSCALED,
	vk::VK_FORMAT_B8G8R8_UINT,
	vk::VK_FORMAT_B8G8R8_SINT,
	vk::VK_FORMAT_B8G8R8_SRGB,
	vk::VK_FORMAT_R8G8B8A8_UNORM,
	vk::VK_FORMAT_R8G8B8A8_SNORM,
	vk::VK_FORMAT_R8G8B8A8_USCALED,
	vk::VK_FORMAT_R8G8B8A8_SSCALED,
	vk::VK_FORMAT_R8G8B8A8_UINT,
	vk::VK_FORMAT_R8G8B8A8_SINT,
	vk::VK_FORMAT_R8G8B8A8_SRGB,
	vk::VK_FORMAT_B8G8R8A8_UNORM,
	vk::VK_FORMAT_B8G8R8A8_SNORM,
	vk::VK_FORMAT_B8G8R8A8_USCALED,
	vk::VK_FORMAT_B8G8R8A8_SSCALED,
	vk::VK_FORMAT_B8G8R8A8_UINT,
	vk::VK_FORMAT_B8G8R8A8_SINT,
	vk::VK_FORMAT_B8G8R8A8_SRGB,
	vk::VK_FORMAT_A8B8G8R8_UNORM_PACK32,
	vk::VK_FORMAT_A8B8G8R8_SNORM_PACK32,
	vk::VK_FORMAT_A8B8G8R8_USCALED_PACK32,
	vk::VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
	vk::VK_FORMAT_A8B8G8R8_UINT_PACK32,
	vk::VK_FORMAT_A8B8G8R8_SINT_PACK32,
	vk::VK_FORMAT_A8B8G8R8_SRGB_PACK32,
	vk::VK_FORMAT_A2R10G10B10_UNORM_PACK32,
	vk::VK_FORMAT_A2R10G10B10_SNORM_PACK32,
	vk::VK_FORMAT_A2R10G10B10_USCALED_PACK32,
	vk::VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
	vk::VK_FORMAT_A2R10G10B10_UINT_PACK32,
	vk::VK_FORMAT_A2R10G10B10_SINT_PACK32,
	vk::VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	vk::VK_FORMAT_A2B10G10R10_SNORM_PACK32,
	vk::VK_FORMAT_A2B10G10R10_USCALED_PACK32,
	vk::VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
	vk::VK_FORMAT_A2B10G10R10_UINT_PACK32,
	vk::VK_FORMAT_A2B10G10R10_SINT_PACK32,
	vk::VK_FORMAT_R16_UNORM,
	vk::VK_FORMAT_R16_SNORM,
	vk::VK_FORMAT_R16_USCALED,
	vk::VK_FORMAT_R16_SSCALED,
	vk::VK_FORMAT_R16_UINT,
	vk::VK_FORMAT_R16_SINT,
	vk::VK_FORMAT_R16_SFLOAT,
	vk::VK_FORMAT_R16G16_UNORM,
	vk::VK_FORMAT_R16G16_SNORM,
	vk::VK_FORMAT_R16G16_USCALED,
	vk::VK_FORMAT_R16G16_SSCALED,
	vk::VK_FORMAT_R16G16_UINT,
	vk::VK_FORMAT_R16G16_SINT,
	vk::VK_FORMAT_R16G16_SFLOAT,
	vk::VK_FORMAT_R16G16B16_UNORM,
	vk::VK_FORMAT_R16G16B16_SNORM,
	vk::VK_FORMAT_R16G16B16_USCALED,
	vk::VK_FORMAT_R16G16B16_SSCALED,
	vk::VK_FORMAT_R16G16B16_UINT,
	vk::VK_FORMAT_R16G16B16_SINT,
	vk::VK_FORMAT_R16G16B16_SFLOAT,
	vk::VK_FORMAT_R16G16B16A16_UNORM,
	vk::VK_FORMAT_R16G16B16A16_SNORM,
	vk::VK_FORMAT_R16G16B16A16_USCALED,
	vk::VK_FORMAT_R16G16B16A16_SSCALED,
	vk::VK_FORMAT_R16G16B16A16_UINT,
	vk::VK_FORMAT_R16G16B16A16_SINT,
	vk::VK_FORMAT_R16G16B16A16_SFLOAT,
	vk::VK_FORMAT_R32_UINT,
	vk::VK_FORMAT_R32_SINT,
	vk::VK_FORMAT_R32_SFLOAT,
	vk::VK_FORMAT_R32G32_UINT,
	vk::VK_FORMAT_R32G32_SINT,
	vk::VK_FORMAT_R32G32_SFLOAT,
	vk::VK_FORMAT_R32G32B32_UINT,
	vk::VK_FORMAT_R32G32B32_SINT,
	vk::VK_FORMAT_R32G32B32_SFLOAT,
	vk::VK_FORMAT_R32G32B32A32_UINT,
	vk::VK_FORMAT_R32G32B32A32_SINT,
	vk::VK_FORMAT_R32G32B32A32_SFLOAT,
};

const vk::VkFormat randomColorFormats[] =
{
	vk::VK_FORMAT_R8_UNORM,
	vk::VK_FORMAT_R8_SNORM,
	vk::VK_FORMAT_R8G8_UNORM,
	vk::VK_FORMAT_R8G8_SNORM,
	vk::VK_FORMAT_R8G8B8A8_UNORM,
	vk::VK_FORMAT_R8G8B8A8_SNORM,
	vk::VK_FORMAT_A8B8G8R8_UNORM_PACK32,
	vk::VK_FORMAT_A8B8G8R8_SNORM_PACK32,
	vk::VK_FORMAT_R16_UNORM,
	vk::VK_FORMAT_R16_SNORM,
	vk::VK_FORMAT_R16G16_UNORM,
	vk::VK_FORMAT_R16G16_SNORM,
	vk::VK_FORMAT_R16G16_SFLOAT,
	vk::VK_FORMAT_R16G16B16_UNORM,
	vk::VK_FORMAT_R16G16B16_SNORM,
	vk::VK_FORMAT_R16G16B16_SFLOAT,
	vk::VK_FORMAT_R16G16B16A16_UNORM,
	vk::VK_FORMAT_R16G16B16A16_SNORM,
	vk::VK_FORMAT_R16G16B16A16_SFLOAT,
	vk::VK_FORMAT_R32_SFLOAT,
	vk::VK_FORMAT_R32G32_SFLOAT,
	vk::VK_FORMAT_R32G32B32_SFLOAT,
	vk::VK_FORMAT_R32G32B32A32_SFLOAT,
};

de::MovePtr<tcu::TextureLevel> readDepthAttachment (const vk::DeviceInterface&	vk,
													vk::VkDevice				device,
													vk::VkQueue					queue,
													deUint32					queueFamilyIndex,
													vk::Allocator&				allocator,
													vk::VkImage					image,
													vk::VkFormat				format,
													const tcu::UVec2&			renderSize,
													vk::VkImageLayout			currentLayout)
{
	vk::Move<vk::VkBuffer>					buffer;
	de::MovePtr<vk::Allocation>				bufferAlloc;
	vk::Move<vk::VkCommandPool>				cmdPool;
	vk::Move<vk::VkCommandBuffer>			cmdBuffer;

	tcu::TextureFormat				retFormat		(tcu::TextureFormat::D, tcu::TextureFormat::CHANNELTYPE_LAST);
	tcu::TextureFormat				bufferFormat	(tcu::TextureFormat::D, tcu::TextureFormat::CHANNELTYPE_LAST);
	const vk::VkImageAspectFlags	barrierAspect	= vk::VK_IMAGE_ASPECT_DEPTH_BIT | (mapVkFormat(format).order == tcu::TextureFormat::DS ? vk::VK_IMAGE_ASPECT_STENCIL_BIT : (vk::VkImageAspectFlagBits)0);

	switch (format)
	{
	case vk::VK_FORMAT_D16_UNORM:
	case vk::VK_FORMAT_D16_UNORM_S8_UINT:
		bufferFormat.type = retFormat.type = tcu::TextureFormat::UNORM_INT16;
		break;
	case vk::VK_FORMAT_D24_UNORM_S8_UINT:
	case vk::VK_FORMAT_X8_D24_UNORM_PACK32:
		retFormat.type = tcu::TextureFormat::UNORM_INT24;
		// vkCmdCopyBufferToImage copies D24 data to 32-bit pixels.
		bufferFormat.type = tcu::TextureFormat::UNSIGNED_INT_24_8_REV;
		break;
	case vk::VK_FORMAT_D32_SFLOAT:
	case vk::VK_FORMAT_D32_SFLOAT_S8_UINT:
		bufferFormat.type = retFormat.type = tcu::TextureFormat::FLOAT;
		break;
	default:
		TCU_FAIL("unrecognized format");
	}

	const vk::VkDeviceSize				pixelDataSize	= renderSize.x() * renderSize.y() * bufferFormat.getPixelSize();
	de::MovePtr<tcu::TextureLevel>	resultLevel		(new tcu::TextureLevel(retFormat, renderSize.x(), renderSize.y()));

	// Create destination buffer
	{
		const vk::VkBufferCreateInfo bufferParams =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			pixelDataSize,								// VkDeviceSize			size;
			vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, device, &bufferParams);
		bufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), vk::MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Create command pool and buffer
	cmdPool		= createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	cmdBuffer	= allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);
	copyImageToBuffer(vk, *cmdBuffer, image, *buffer, tcu::IVec2(renderSize.x(), renderSize.y()), vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, currentLayout, 1u, barrierAspect, vk::VK_IMAGE_ASPECT_DEPTH_BIT);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Read buffer data
	invalidateAlloc(vk, device, *bufferAlloc);
	tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(bufferFormat, resultLevel->getSize(), bufferAlloc->getHostPtr()));

	return resultLevel;
}

class ShaderObjectRenderingInstance : public vkt::TestInstance
{
public:
							ShaderObjectRenderingInstance	(Context& context, const TestParams& params)
															: vkt::TestInstance	(context)
															, m_params			(params)
															{}
	virtual					~ShaderObjectRenderingInstance	(void) {}

	tcu::TestStatus			iterate							(void) override;
private:
	void					beginRendering					(vk::VkCommandBuffer cmdBuffer);
	void					createDummyImage				(void);
	void					createDummyRenderPass			(void);
	void					setColorFormats					(const vk::InstanceDriver& vki);
	void					generateExpectedImage			(const tcu::PixelBufferAccess& outputImage, const deUint32 width, const deUint32 height, deUint32 attachmentIndex);

	TestParams								m_params;

	const vk::VkRect2D						m_renderArea = vk::makeRect2D(0, 0, 32, 32);
	std::vector<vk::VkFormat>				m_colorFormats;
	std::vector<vk::Move<vk::VkImageView>>	m_colorImageViews;
	vk::Move<vk::VkImageView>				m_depthImageView;

	de::MovePtr<vk::ImageWithMemory>		m_dummyImage;
	vk::Move<vk::VkImageView>				m_dummyImageView;
	vk::Move<vk::VkRenderPass>				m_dummyRenderPass;
	vk::Move<vk::VkFramebuffer>				m_dummyFramebuffer;
};

void ShaderObjectRenderingInstance::createDummyImage (void)
{
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	auto&								alloc						= m_context.getDefaultAllocator();
	const auto							colorSubresourceRange		= makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	vk::VkFormat format = m_params.colorFormat == vk::VK_FORMAT_R8G8B8A8_UNORM ? vk::VK_FORMAT_R32G32B32A32_SFLOAT : vk::VK_FORMAT_R8G8B8A8_UNORM;

	const vk::VkImageCreateInfo	createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		format,										// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	m_dummyImage = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
	m_dummyImageView = vk::makeImageView(vk, device, **m_dummyImage, vk::VK_IMAGE_VIEW_TYPE_2D, format, colorSubresourceRange);
}

void ShaderObjectRenderingInstance::createDummyRenderPass (void)
{
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	vk::VkFormat format = m_params.colorFormat == vk::VK_FORMAT_R8G8B8A8_UNORM ? vk::VK_FORMAT_R32G32B32A32_SFLOAT : vk::VK_FORMAT_R8G8B8A8_UNORM;
	m_dummyRenderPass = vk::makeRenderPass(vk, device, format);
	m_dummyFramebuffer = vk::makeFramebuffer(vk, device, *m_dummyRenderPass, 1u, &*m_dummyImageView, m_renderArea.extent.width, m_renderArea.extent.height);
}

vk::VkClearValue getClearValue(const tcu::TextureFormat tcuFormat)
{
	const tcu::TextureChannelClass	channelClass = tcu::getTextureChannelClass(tcuFormat.type);

	if (channelClass != tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER && channelClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		return vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });

	const tcu::IVec4	bits = tcu::min(tcu::getTextureFormatBitDepth(tcuFormat), tcu::IVec4(32));
	const int			signBit = (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ? 1 : 0);

	if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		return vk::makeClearValueColorU32(0u, 0u, 0u, deUint32((deUint64(1) << (bits[3] - signBit)) - 1));

	return vk::makeClearValueColorI32(0u, 0u, 0u, deInt32((deUint64(1) << (bits[3] - signBit)) - 1));
}

void ShaderObjectRenderingInstance::beginRendering (vk::VkCommandBuffer cmdBuffer)
{
	const vk::DeviceInterface&			vk					= m_context.getDeviceInterface();
	const vk::VkClearValue				floatClearValue		= vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
	const vk::VkClearValue				clearDepthValue		= vk::makeClearValueDepthStencil(1.0f, 0u);

	vk::VkRenderingAttachmentInfo colorAttachment
	{
		vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		VK_NULL_HANDLE,											// VkImageView							imageView;
		vk::VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout						imageLayout;
		vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
		DE_NULL,												// VkImageView							resolveImageView;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout						resolveImageLayout;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp					loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
		floatClearValue											// VkClearValue							clearValue;
	};

	deUint32 outputCount = m_params.colorAttachmentCount + m_params.extraFragmentOutputCount + m_params.extraAttachmentCount;
	std::vector<vk::VkRenderingAttachmentInfo> colorAttachments(outputCount);
	deUint32 i = 0;
	if (m_params.extraOutputs == BEFORE || (m_params.extraOutputs == BETWEEN && m_params.colorAttachmentCount + m_params.extraAttachmentCount == 0))
	{
		colorAttachment.imageView = VK_NULL_HANDLE;
		for (deUint32 j = 0; j < m_params.extraFragmentOutputCount; ++j)
			colorAttachments[i++] = colorAttachment;
	}
	for (deUint32 j = 0; j < m_params.colorAttachmentCount + m_params.extraAttachmentCount; ++j)
	{
		if (m_params.extraOutputs == BETWEEN && i == (m_params.colorAttachmentCount + m_params.extraAttachmentCount) / 2 + 1)
		{
			colorAttachment.imageView = VK_NULL_HANDLE;
			for (deUint32 k = 0; k < m_params.extraFragmentOutputCount; ++k)
				colorAttachments[i++] = colorAttachment;
		}
		colorAttachment.imageView = *m_colorImageViews[j];
		colorAttachment.clearValue = getClearValue(vk::mapVkFormat(m_colorFormats[j]));

		colorAttachments[i++] = colorAttachment;
	}
	if (m_params.extraOutputs == AFTER || (m_params.extraOutputs == BETWEEN && m_params.colorAttachmentCount + m_params.extraAttachmentCount == 1))
	{
		colorAttachment.imageView = VK_NULL_HANDLE;
		for (deUint32 j = 0; j < m_params.extraFragmentOutputCount; ++j)
			colorAttachments[i++] = colorAttachment;
	}

	vk::VkRenderingAttachmentInfo depthAttachment
	{
		vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		*m_depthImageView,										// VkImageView							imageView;
		vk::VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout						imageLayout;
		vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
		DE_NULL,												// VkImageView							resolveImageView;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout						resolveImageLayout;
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp					loadOp;
		vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
		clearDepthValue											// VkClearValue							clearValue;
	};

	vk::VkRenderingInfoKHR renderingInfo
	{
		vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		DE_NULL,
		(vk::VkRenderingFlags)0u,									// VkRenderingFlagsKHR					flags;
		m_renderArea,												// VkRect2D								renderArea;
		1u,															// deUint32								layerCount;
		0x0,														// deUint32								viewMask;
		(deUint32)colorAttachments.size(),							// deUint32								colorAttachmentCount;
		colorAttachments.data(),									// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		m_params.useDepthAttachment ? &depthAttachment : DE_NULL,	// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,													// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	vk.cmdBeginRendering(cmdBuffer, &renderingInfo);
}

void ShaderObjectRenderingInstance::setColorFormats (const vk::InstanceDriver& vki)
{
	const auto							physicalDevice = m_context.getPhysicalDevice();

	m_colorFormats.resize(m_params.colorAttachmentCount + m_params.extraAttachmentCount);
	if (m_params.randomColorFormats)
	{
		if (m_colorFormats.size() > 0)
		{
			m_colorFormats[0] = m_params.colorFormat;
		}
		de::Random random(102030);
		for (deUint32 i = 1; i < (deUint32)m_colorFormats.size(); ++i)
		{
			if (i <= m_params.extraAttachmentCount && m_params.extraAttachments == BEFORE)
				m_colorFormats[i] = m_params.colorFormat;
			else
			{
				while (true)
				{
					// Find random color format, and make sure it is supported
					vk::VkFormat format = randomColorFormats[random.getUint32() % DE_LENGTH_OF_ARRAY(randomColorFormats)];
					vk::VkImageFormatProperties	colorImageFormatProperties;
					const auto colorResult = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT), 0, &colorImageFormatProperties);
					if (colorResult == vk::VK_SUCCESS)
					{
						m_colorFormats[i] = format;
						break;
					}
				}
			}
		}
	}
	else
	{
		for (auto& colorFormat : m_colorFormats)
			colorFormat = m_params.colorFormat;
	}
}

void ShaderObjectRenderingInstance::generateExpectedImage (const tcu::PixelBufferAccess& outputImage, const deUint32 width, const deUint32 height, deUint32 attachmentIndex)
{
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(outputImage.getFormat().type);
	const vk::VkClearValue			clearValue		= getClearValue(outputImage.getFormat());

	const deUint32					xOffset			= 8;
	const deUint32					yOffset			= 8;

	if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		tcu::clear(outputImage, tcu::UVec4(clearValue.color.uint32));
	else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		tcu::clear(outputImage, tcu::IVec4(clearValue.color.int32));
	else
		tcu::clear(outputImage, tcu::Vec4(clearValue.color.float32));

	if ((m_params.extraAttachments == BEFORE && attachmentIndex < m_params.extraAttachmentCount) || (m_params.extraAttachments == BETWEEN && attachmentIndex > m_params.colorAttachmentCount / 2u && attachmentIndex <= m_params.colorAttachmentCount / 2u + m_params.extraAttachmentCount) || (m_params.extraAttachments == AFTER && attachmentIndex >= m_params.colorAttachmentCount))
		return;

	tcu::Vec4 setColor		= tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	tcu::IVec4 setColorInt	= tcu::IVec4(0, 0, 0, 0);
	tcu::UVec4 setColorUint = tcu::UVec4(0u, 0u, 0u, 0u);

	for (deInt32 i = 0; i < tcu::getNumUsedChannels(outputImage.getFormat().order); ++i)
	{
		setColor[i] = 1.0f;
		setColorInt[i] = 255;
		setColorUint[i] = 255u;
	}

	for (deUint32 j = 0; j < height; ++j)
	{
		for (deUint32 i = 0; i < width; ++i)
		{
			if (i >= xOffset && i < width - xOffset && j >= yOffset && j < height - yOffset)
			{
				if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
					outputImage.setPixel(setColorUint, i, j, 0);
				else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
					outputImage.setPixel(setColorInt, i, j, 0);
				else
					outputImage.setPixel(setColor, i, j, 0);
			}
		}
	}
}

tcu::TestStatus ShaderObjectRenderingInstance::iterate (void)
{
	const vk::VkInstance				instance					= m_context.getInstance();
	const vk::InstanceDriver			instanceDriver				(m_context.getPlatformInterface(), instance);
	const vk::DeviceInterface&			vk							= m_context.getDeviceInterface();
	const vk::VkDevice					device						= m_context.getDevice();
	const vk::VkQueue					queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	auto&								alloc						= m_context.getDefaultAllocator();
	tcu::TestLog&						log							= m_context.getTestContext().getLog();
	const auto							deviceExtensions			= vk::removeUnsupportedShaderObjectExtensions(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_context.getDeviceExtensions());
	const bool							tessellationSupported		= m_context.getDeviceFeatures().tessellationShader;
	const bool							geometrySupported			= m_context.getDeviceFeatures().geometryShader;
	const bool							taskSupported				= m_context.getMeshShaderFeatures().taskShader;
	const bool							meshSupported				= m_context.getMeshShaderFeatures().meshShader;

	const auto							colorSubresourceRange		= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	auto								depthSubresourceRange		= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
	if (m_params.useDepthAttachment && tcu::hasStencilComponent(mapVkFormat(m_params.depthFormat).order))
		depthSubresourceRange.aspectMask |= vk::VK_IMAGE_ASPECT_STENCIL_BIT;
	const auto							colorSubresourceLayers		= vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	vk::VkExtent3D						extent						= { m_renderArea.extent.width, m_renderArea.extent.height, 1};

	setColorFormats(instanceDriver);

	vk::VkImageCreateInfo	createInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		vk::VK_FORMAT_UNDEFINED,					// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	const vk::VkImageCreateInfo	depthCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0u,											// VkImageCreateFlags		flags
		vk::VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		m_params.depthFormat,						// VkFormat					format
		{ 32, 32, 1 },								// VkExtent3D				extent
		1u,											// uint32_t					mipLevels
		1u,											// uint32_t					arrayLayers
		vk::VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		vk::VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling
		vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,											// uint32_t					queueFamilyIndexCount
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices
		vk::VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout
	};

	deUint32 colorAttachmentCount = m_params.colorAttachmentCount + m_params.extraAttachmentCount;
	std::vector<de::MovePtr<vk::ImageWithMemory>>	colorImages	(colorAttachmentCount);
	m_colorImageViews.resize(colorAttachmentCount);
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		createInfo.format = m_colorFormats[i];
		colorImages[i] = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));
		m_colorImageViews[i] = vk::makeImageView(vk, device, **colorImages[i], vk::VK_IMAGE_VIEW_TYPE_2D, createInfo.format, colorSubresourceRange);
	}

	de::MovePtr<vk::ImageWithMemory>	depthImage;
	if (m_params.useDepthAttachment)
	{
		depthImage = de::MovePtr<vk::ImageWithMemory>(new vk::ImageWithMemory(vk, device, alloc, depthCreateInfo, vk::MemoryRequirement::Any));
		m_depthImageView = vk::makeImageView(vk, device, **depthImage, vk::VK_IMAGE_VIEW_TYPE_2D, m_params.depthFormat, depthSubresourceRange);
	}

	std::vector<de::MovePtr<vk::BufferWithMemory>>	colorOutputBuffers;
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		const vk::VkDeviceSize				colorOutputBufferSize = m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(vk::mapVkFormat(m_colorFormats[i]));
		colorOutputBuffers.push_back(de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
			vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible)));
	}

	const auto&							binaries				= m_context.getBinaryCollection();
	const auto							vertShader				= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_VERTEX_BIT, binaries.get("vertDepth"), tessellationSupported, geometrySupported));
	const auto							fragShader				= vk::createShader(vk, device, vk::makeShaderCreateInfo(vk::VK_SHADER_STAGE_FRAGMENT_BIT, binaries.get("fragMulti"), tessellationSupported, geometrySupported));

	const vk::Move<vk::VkCommandPool>	cmdPool					(vk::createCommandPool(vk, device, 0u, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>	cmdBuffer				(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	vk::beginCommandBuffer(vk, *cmdBuffer);

	if (m_params.dummyRenderPass == DUMMY_DYNAMIC)
	{
		createDummyImage();
		const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		vk::beginRendering(vk, *cmdBuffer, *m_dummyImageView, m_renderArea, clearValue);
	}
	else if (m_params.dummyRenderPass == DUMMY_STATIC)
	{
		createDummyImage();
		createDummyRenderPass();
		const vk::VkClearValue				clearValue = vk::makeClearValueColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		vk::beginRenderPass(vk, *cmdBuffer, *m_dummyRenderPass, *m_dummyFramebuffer, m_renderArea, clearValue);
	}

	if (m_params.bindShadersBeforeBeginRendering)
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, taskSupported, meshSupported);

	if (m_params.dummyRenderPass == DUMMY_DYNAMIC)
	{
		vk::endRendering(vk, *cmdBuffer);
	}
	else if (m_params.dummyRenderPass == DUMMY_STATIC)
	{
		vk::endRenderPass(vk, *cmdBuffer);
	}

	for (const auto& colorImage : colorImages)
	{
		vk::VkImageMemoryBarrier preImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **colorImage, colorSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preImageBarrier);
	}

	if (m_params.useDepthAttachment)
	{
		vk::VkImageMemoryBarrier preDepthImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_NONE, vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, **depthImage, depthSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &preDepthImageBarrier);
	}

	beginRendering(*cmdBuffer);
	vk::setDefaultShaderObjectDynamicStates(vk, *cmdBuffer, deviceExtensions, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	vk::VkBool32 colorBlendEnable = VK_FALSE;
	vk::VkColorBlendEquationEXT		colorBlendEquation = {
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	srcColorBlendFactor;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	dstColorBlendFactor;
		vk::VK_BLEND_OP_ADD,			// VkBlendOp		colorBlendOp;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	srcAlphaBlendFactor;
		vk::VK_BLEND_FACTOR_ONE,		// VkBlendFactor	dstAlphaBlendFactor;
		vk::VK_BLEND_OP_ADD,			// VkBlendOp		alphaBlendOp;
	};
	vk::VkColorComponentFlags		colorWriteMask = vk::VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT |
		vk::VK_COLOR_COMPONENT_B_BIT | vk::VK_COLOR_COMPONENT_A_BIT;
	deUint32 count = colorAttachmentCount + m_params.extraFragmentOutputCount;
	if (count == 0) ++count;
	std::vector<vk::VkBool32>					colorBlendEnables	(count, colorBlendEnable);
	std::vector<vk::VkColorBlendEquationEXT>	colorBlendEquations	(count, colorBlendEquation);
	std::vector<vk::VkColorComponentFlags>		colorWriteMasks		(count, colorWriteMask);
	vk.cmdSetColorBlendEnableEXT(*cmdBuffer, 0u, count, colorBlendEnables.data());
	vk.cmdSetColorBlendEquationEXT(*cmdBuffer, 0u, count, colorBlendEquations.data());
	vk.cmdSetColorWriteMaskEXT(*cmdBuffer, 0u, count, colorWriteMasks.data());
	std::vector<vk::VkBool32> colorWriteEnables	(count, VK_TRUE);
	vk.cmdSetColorWriteEnableEXT(*cmdBuffer, count, colorWriteEnables.data());
	vk.cmdSetDepthWriteEnable(*cmdBuffer, VK_TRUE);
	vk.cmdSetDepthTestEnable(*cmdBuffer, VK_TRUE);
	vk.cmdSetDepthCompareOp(*cmdBuffer, vk::VK_COMPARE_OP_LESS);
	vk::bindNullTaskMeshShaders(vk, *cmdBuffer, m_context.getMeshShaderFeaturesEXT());
	if (!m_params.bindShadersBeforeBeginRendering)
		vk::bindGraphicsShaders(vk, *cmdBuffer, *vertShader, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragShader, taskSupported, meshSupported);
	vk.cmdDraw(*cmdBuffer, 4, 1, 0, 0);
	vk::endRendering(vk, *cmdBuffer);

	for (const auto& colorImage : colorImages)
	{
		vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **colorImage, colorSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);
	}

	if (m_params.useDepthAttachment)
	{
		vk::VkImageMemoryBarrier postDepthImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL, **depthImage, depthSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postDepthImageBarrier);
	}

	const vk::VkBufferImageCopy	colorCopyRegion = vk::makeBufferImageCopy(extent, colorSubresourceLayers);
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
		vk.cmdCopyImageToBuffer(*cmdBuffer, **colorImages[i], vk::VK_IMAGE_LAYOUT_GENERAL, **colorOutputBuffers[i], 1u, &colorCopyRegion);

	vk::endCommandBuffer(vk, *cmdBuffer);

	vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	std::vector<tcu::ConstPixelBufferAccess> colorResultBuffers;
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
		colorResultBuffers.push_back(tcu::ConstPixelBufferAccess(vk::mapVkFormat(m_colorFormats[i]), m_renderArea.extent.width, m_renderArea.extent.height, 1, (const void*)colorOutputBuffers[i]->getAllocation().getHostPtr()));

	const deUint32			width	= m_renderArea.extent.width;
	const deUint32			height	= m_renderArea.extent.height;
	const deUint32			xOffset = 8;
	const deUint32			yOffset = 8;

	for (deUint32 k = 0; k < (deUint32)colorImages.size(); ++k)
	{
		tcu::TextureLevel					textureLevel	(mapVkFormat(m_colorFormats[k]), width, height);
		const tcu::PixelBufferAccess		expectedImage	= textureLevel.getAccess();
		generateExpectedImage(expectedImage, width, height, k);

		if (vk::isFloatFormat(m_colorFormats[k]))
		{
			if (!tcu::floatThresholdCompare(log, "Image Comparison", "", expectedImage, colorResultBuffers[k], tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("Fail");
		}
		else
		{
			if (!tcu::intThresholdCompare(log, "Image Comparison", "", expectedImage, colorResultBuffers[k], tcu::UVec4(2), tcu::COMPARE_LOG_RESULT))
				return tcu::TestStatus::fail("Fail");
		}
	}

	if (m_params.useDepthAttachment)
	{
		const auto depthBuffer = readDepthAttachment(vk, device, queue, queueFamilyIndex, alloc, **depthImage, m_params.depthFormat, tcu::UVec2(m_renderArea.extent.width, m_renderArea.extent.height), vk::VK_IMAGE_LAYOUT_GENERAL);
		const auto depthAccess = depthBuffer->getAccess();

		const float depthEpsilon = 0.02f;
		for (deUint32 j = 0; j < height; ++j)
		{
			for (deUint32 i = 0; i < width; ++i)
			{
				const float depth = depthAccess.getPixDepth(i, j);
				if (i >= xOffset && i < width - xOffset && j >= yOffset && j < height - yOffset)
				{
					if (deFloatAbs(depth - 0.5f) > depthEpsilon)
					{
						log << tcu::TestLog::Message << "Depth at (" << i << ", " << j << ") is expected to be 0.5, but was (" << depth << ")" << tcu::TestLog::EndMessage;
						return tcu::TestStatus::fail("Fail");
					}
				}
				else
				{
					if (deFloatAbs(depth - 1.0f) > depthEpsilon)
					{
						log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be 0.0, but was (" << depth << ")" << tcu::TestLog::EndMessage;
						return tcu::TestStatus::fail("Fail");
					}
				}
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class ShaderObjectRenderingCase : public vkt::TestCase
{
public:
							ShaderObjectRenderingCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
														: vkt::TestCase		(testCtx, name, description)
														, m_params			(params)
														{}
	virtual					~ShaderObjectRenderingCase	(void) {}

	void					checkSupport			(vkt::Context& context) const override;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*			createInstance			(Context& context) const override { return new ShaderObjectRenderingInstance(context, m_params); }
private:
	TestParams m_params;
};

void ShaderObjectRenderingCase::checkSupport (Context& context) const
{
	const auto&		vki				= context.getInstanceInterface();
	const auto		physicalDevice	= context.getPhysicalDevice();
	const vk::VkPhysicalDeviceProperties properties = vk::getPhysicalDeviceProperties(vki, physicalDevice);

	context.requireDeviceFunctionality("VK_EXT_shader_object");

	if (m_params.colorAttachmentCount + m_params.extraAttachmentCount + m_params.extraFragmentOutputCount > properties.limits.maxColorAttachments)
		TCU_THROW(NotSupportedError, "Tests uses more color attachments than VkPhysicalDeviceLimits::maxColorAttachments");

	vk::VkImageFormatProperties	colorImageFormatProperties;
	const auto colorResult = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_params.colorFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, (vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT), 0, &colorImageFormatProperties);
	if (colorResult != vk::VK_SUCCESS)
		TCU_THROW(NotSupportedError, "Format unsupported for tiling");
	vk::VkImageFormatProperties	depthImageFormatProperties;
	if (m_params.useDepthAttachment)
	{
		const auto depthResult = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_params.depthFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, (vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT), 0, &depthImageFormatProperties);
		if (depthResult != vk::VK_SUCCESS)
			TCU_THROW(NotSupportedError, "Format unsupported for tiling");
	}
}

void ShaderObjectRenderingCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::stringstream vertDepth;
	std::stringstream fragMulti;

	vertDepth
		<< "#version 450\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
		<< "    gl_Position = vec4(pos - 0.5f, 0.5f, 1.0f);\n"
		<< "}\n";

	fragMulti
		<< "#version 450\n";
	deUint32 outputCount = m_params.colorAttachmentCount + m_params.extraFragmentOutputCount;
	for (deUint32 i = 0; i < outputCount; ++i)
	{
		deUint32 j = i;
		if (m_params.extraAttachments == BEFORE || (m_params.extraAttachments == BETWEEN && i > outputCount / 2))
			j += m_params.extraAttachmentCount;
		bool firstWrittenAttachment = (m_params.extraOutputs == BEFORE) ? (i == m_params.extraFragmentOutputCount) : (i == 0);
		if (vk::isUintFormat(m_params.colorFormat) && (firstWrittenAttachment || !m_params.randomColorFormats))
			fragMulti << "layout (location = " << j << ") out uvec4 outColor" << j << ";\n";
		else if (vk::isIntFormat(m_params.colorFormat) && (firstWrittenAttachment || !m_params.randomColorFormats))
			fragMulti << "layout (location = " << j << ") out ivec4 outColor" << j << ";\n";
		else
			fragMulti << "layout (location = " << j << ") out vec4 outColor" << j << ";\n";
	}
	fragMulti
		<< "void main() {\n";
	for (deUint32 i = 0; i < outputCount; ++i)
	{
		deUint32 j = i;
		if (m_params.extraAttachments == BEFORE || (m_params.extraAttachments == BETWEEN && i > outputCount / 2))
			j += m_params.extraAttachmentCount;
		bool firstWrittenAttachment = (m_params.extraOutputs == BEFORE) ? (i == m_params.extraFragmentOutputCount) : (i == 0);
		if (vk::isUintFormat(m_params.colorFormat) && (firstWrittenAttachment || !m_params.randomColorFormats))
			fragMulti << "    outColor" << j << " = uvec4(255);\n";
		else if (vk::isIntFormat(m_params.colorFormat) && (firstWrittenAttachment || !m_params.randomColorFormats))
			fragMulti << "    outColor" << j << " = ivec4(255);\n";
		else
			fragMulti << "    outColor" << j << " = vec4(1.0f);\n";
	}
	if (m_params.writeGlFragDepth)
		fragMulti
			<< "    gl_FragDepth = 0.5f;\n";
	fragMulti
		<< "}\n";

	programCollection.glslSources.add("vertDepth") << glu::VertexSource(vertDepth.str());
	programCollection.glslSources.add("fragMulti") << glu::FragmentSource(fragMulti.str());
}

}

std::string getFormatCaseName(vk::VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

tcu::TestCaseGroup* createShaderObjectRenderingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> renderingGroup(new tcu::TestCaseGroup(testCtx, "rendering", ""));

	const struct
	{
		deUint32 colorAttachmentCount;
		const char* name;
	} colorAttachmentCountTests[] =
	{
		{ 0u,	"color_attachment_count_0",	},
		{ 1u,	"color_attachment_count_1",	},
		{ 4u,	"color_attachment_count_4",	},
		{ 8u,	"color_attachment_count_8",	},
	};

	const struct
	{
		deUint32 extraAttachmentCount;
		ExtraAttachments extraAttachment;
		const char* name;
	} extraAttachmentTests[] =
	{
		{ 0u,	NONE,		"none",							},
		{ 1u,	BEFORE,		"extra_attachment_before_1",	},
		{ 1u,	BETWEEN,	"extra_attachment_between_1",	},
		{ 1u,	AFTER,		"extra_attachment_after_1",		},
		{ 2u,	BEFORE,		"extra_attachment_before_2",	},
		{ 2u,	BETWEEN,	"extra_attachment_between_2",	},
		{ 2u,	AFTER,		"extra_attachment_after_2",		},
	};

	const struct
	{
		deUint32 extraFragmentOutputCount;
		ExtraAttachments extraAttachment;
		const char* name;
	} extraOutputTests[] =
	{
		{ 0u,	NONE,		"none",						},
		{ 1u,	BEFORE,		"extra_output_before_1",	},
		{ 1u,	BETWEEN,	"extra_output_between_1",	},
		{ 1u,	AFTER,		"extra_output_after_1",		},
		{ 2u,	BEFORE,		"extra_output_before_2",	},
		{ 2u,	BETWEEN,	"extra_output_between_2",	},
		{ 2u,	AFTER,		"extra_output_after_2",		},
	};

	const vk::VkFormat		depthStencilFormats[] =
	{
		vk::VK_FORMAT_D16_UNORM,
		vk::VK_FORMAT_X8_D24_UNORM_PACK32,
		vk::VK_FORMAT_D32_SFLOAT,
		vk::VK_FORMAT_D16_UNORM_S8_UINT,
		vk::VK_FORMAT_D24_UNORM_S8_UINT,
		vk::VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const struct
	{
		DummyRenderPass	dummyRenderPass;
		const char* name;
	} dummyRenderPassTests[] =
	{
		{ DUMMY_NONE,		"none",		},
		{ DUMMY_DYNAMIC,	"dynamic",	},
		{ DUMMY_STATIC,		"static",	},
	};

	for (const auto& colorAttachmentCountTest : colorAttachmentCountTests)
	{
		de::MovePtr<tcu::TestCaseGroup> colorAttachmentGroup(new tcu::TestCaseGroup(testCtx, colorAttachmentCountTest.name, ""));
		for (const auto& extraAttachment : extraAttachmentTests)
		{
			de::MovePtr<tcu::TestCaseGroup> extraAttachmentGroup(new tcu::TestCaseGroup(testCtx, extraAttachment.name, ""));
			for (const auto& extraOutput : extraOutputTests)
			{
				if (extraAttachment.extraAttachment != NONE && extraOutput.extraFragmentOutputCount != NONE)
					continue;

				de::MovePtr<tcu::TestCaseGroup> extraOutputGroup(new tcu::TestCaseGroup(testCtx, extraOutput.name, ""));

				for (const auto& dummyRenderPass : dummyRenderPassTests)
				{
					de::MovePtr<tcu::TestCaseGroup> dummyRenderPassGroup(new tcu::TestCaseGroup(testCtx, dummyRenderPass.name, ""));
					for (deUint32 m = 0; m < 2; ++m)
					{
						bool useRandomColorFormats = m == 0;
						if (useRandomColorFormats && colorAttachmentCountTest.colorAttachmentCount < 2)
							continue;
						std::string randomColorFormatsName = useRandomColorFormats ? "random_color_formats" : "same_color_formats";
						de::MovePtr<tcu::TestCaseGroup> randomColorFormatsGroup(new tcu::TestCaseGroup(testCtx, randomColorFormatsName.c_str(), ""));
						for (deUint32 k = 0; k < 2; ++k)
						{
							bool bindShadersBeforeBeginRendering = k == 0;
							std::string bindName = bindShadersBeforeBeginRendering ? "before" : "after";
							de::MovePtr<tcu::TestCaseGroup> bindGroup(new tcu::TestCaseGroup(testCtx, bindName.c_str(), ""));
							for (deUint32 l = 0; l < 2; ++l)
							{
								bool writeGlFragDepth = l == 0;
								std::string writeGlFragName = writeGlFragDepth ? "gl_frag_write" : "none";
								de::MovePtr<tcu::TestCaseGroup> fragWriteGroup(new tcu::TestCaseGroup(testCtx, writeGlFragName.c_str(), ""));
								for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(colorFormats); ++i)
								{
									if (extraAttachment.extraAttachmentCount > colorAttachmentCountTest.colorAttachmentCount)
										continue;

									if (!bindShadersBeforeBeginRendering && dummyRenderPass.dummyRenderPass != DUMMY_NONE)
										continue;

									const auto colorFormat = colorFormats[i];

									TestParams params;
									params.colorAttachmentCount = colorAttachmentCountTest.colorAttachmentCount;
									params.extraAttachmentCount = extraAttachment.extraAttachmentCount;
									params.extraAttachments = extraAttachment.extraAttachment;
									params.extraFragmentOutputCount = extraOutput.extraFragmentOutputCount;
									params.extraOutputs = extraOutput.extraAttachment;
									params.useDepthAttachment = false;
									params.colorFormat = colorFormat;
									params.depthFormat = vk::VK_FORMAT_UNDEFINED;
									params.bindShadersBeforeBeginRendering = bindShadersBeforeBeginRendering;
									params.dummyRenderPass = dummyRenderPass.dummyRenderPass;
									params.writeGlFragDepth = writeGlFragDepth;
									params.randomColorFormats = useRandomColorFormats;

									std::string name = getFormatCaseName(colorFormat);
									fragWriteGroup->addChild(new ShaderObjectRenderingCase(testCtx, name, "", params));

									if (writeGlFragDepth)
										continue;

									for (deUint32 j = 0; j < DE_LENGTH_OF_ARRAY(depthStencilFormats); ++j)
									{
										const auto depthFormat = depthStencilFormats[j];
										params.useDepthAttachment = true;
										params.depthFormat = depthFormat;

										std::string depthTestName = name + "_" + getFormatCaseName(depthFormat);
										fragWriteGroup->addChild(new ShaderObjectRenderingCase(testCtx, depthTestName, "", params));
									}
								}
								bindGroup->addChild(fragWriteGroup.release());
							}
							randomColorFormatsGroup->addChild(bindGroup.release());
						}
						dummyRenderPassGroup->addChild(randomColorFormatsGroup.release());
					}
					extraOutputGroup->addChild(dummyRenderPassGroup.release());
				}
				extraAttachmentGroup->addChild(extraOutputGroup.release());
			}
			colorAttachmentGroup->addChild(extraAttachmentGroup.release());
		}
		renderingGroup->addChild(colorAttachmentGroup.release());
	}

	return renderingGroup.release();
}

} // ShaderObject
} // vkt

