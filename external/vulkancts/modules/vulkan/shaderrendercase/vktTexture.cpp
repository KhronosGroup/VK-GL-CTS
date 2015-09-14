/*------------------------------------------------------------------------
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Texture utilities
 *//*--------------------------------------------------------------------*/

#include "vktTexture.hpp"

#include "deFilePath.hpp"
#include "tcuImageIO.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

namespace vkt
{

static vk::VkFormat mapVkFormat(tcu::CompressedTexFormat format)
{
	switch (format)
	{
		// TODO!!
		default:	return vk::VK_FORMAT_UNDEFINED;
	}
}

static tcu::TextureFormat::ChannelType mapVkChannelType (deUint32 dataType, bool normalized)
{
	// TODO!!
	switch (dataType)
	{
		default:	return normalized ? tcu::TextureFormat::UNORM_INT8	: tcu::TextureFormat::UNSIGNED_INT8;
	}
}

static tcu::TextureFormat mapVkTransferFormat (deUint32 format, deUint32 dataType)
{
	// TODO!!
	switch (format)
	{
		case vk::VK_FORMAT_R8G8B8_UNORM:	return tcu::TextureFormat(tcu::TextureFormat::RGB,		mapVkChannelType(dataType, true));
		case vk::VK_FORMAT_R8G8B8A8_UNORM:
		default:							return tcu::TextureFormat(tcu::TextureFormat::RGBA,		mapVkChannelType(dataType, true));
	}
}

static tcu::TextureFormat mapVkInternalFormat (vk::VkFormat format)
{
	switch (format)
	{
		// TODO!!
		default:	return tcu::TextureFormat(tcu::TextureFormat::RGBA,		tcu::TextureFormat::FLOAT);
	}
}


Texture2D::Texture2D (int numLevels, const tcu::CompressedTexture* levels, const tcu::TexDecompressionParams& decompressionParams)
	: m_isCompressed	(true)
	, m_format			(mapVkFormat(levels[0].getFormat()))
	, m_refTexture		(getUncompressedFormat(levels[0].getFormat()), levels[0].getWidth(), levels[0].getHeight())
{
	try
	{
		loadCompressed(numLevels, levels, decompressionParams);
	}
	catch (const std::exception&)
	{
		throw;
	}
}

Texture2D::Texture2D (vk::VkFormat format, deUint32 dataType, int width, int height)
	: m_isCompressed	(false)
	, m_format			(format)
	, m_refTexture		(mapVkTransferFormat(format, dataType), width, height)
{
}


Texture2D::Texture2D (vk::VkFormat format, int width, int height)
	: m_isCompressed	(false)
	, m_format			(format)
	, m_refTexture		(mapVkInternalFormat(format), width, height)
{
}

Texture2D::~Texture2D (void)
{
}

void Texture2D::upload (const Context& context)
{
	const vk::VkDevice			vkDevice			= context.getDevice();
	const vk::DeviceInterface&	vk					= context.getDeviceInterface();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	vk::SimpleAllocator			memAlloc			(vk, vkDevice, vk::getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	const vk::VkImageCreateInfo	imageCreateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,													// const void*				pnext;
		vk::VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
		m_format,													// VkFormat					format;
		{ m_refTexture.getWidth(), m_refTexture.getHeight(), 1 },	// VkExtend3D				extent;
		1u,															// deUint32					mipLevels;
		1u,															// deUint32					arraySize;
		1u,															// deUint32					samples;
		vk::VK_IMAGE_TILING_LINEAR,									// VkImageTiling			tiling;
		vk::VK_IMAGE_USAGE_SAMPLED_BIT,								// VkImageUsageFlags		usage;
		vk::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,					// VkImageCreateFlags		flags;
		vk::VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		1,															// deuint32					queueFamilyCount;
		&queueFamilyIndex											// const deUint32*			pQueueFamilyIndices;
	};

	m_vkTexture = vk::createImage(vk, vkDevice, &imageCreateInfo);

	// Allocate and bind color image memory
	m_allocation = memAlloc.allocate(vk::getImageMemoryRequirements(vk, vkDevice, *m_vkTexture), vk::MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *m_vkTexture, m_allocation->getMemory(), 0));

	const vk::VkImageSubresource subres	=
	{
		vk::VK_IMAGE_ASPECT_COLOR,
		0u,
		0u
	};

	vk::VkSubresourceLayout layout;
	VK_CHECK(vk.getImageSubresourceLayout(vkDevice, *m_vkTexture, &subres, &layout));

	void *imagePtr;
	VK_CHECK(vk.mapMemory(vkDevice, m_allocation->getMemory(), m_allocation->getOffset(), layout.size, 0u, &imagePtr));

	tcu::ConstPixelBufferAccess access = m_refTexture.getLevel(0);

	deMemcpy(imagePtr, access.getDataPtr(), layout.size);

	const vk::VkMappedMemoryRange range =
	{
		vk::VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,		// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		m_allocation->getMemory(),						// VkDeviceMemory	mem;
		0,												// VkDeviceSize		offset;
		layout.size,									// VkDeviceSize		size;
	};

	VK_CHECK(vk.flushMappedMemoryRanges(vkDevice, 1u, &range));
	VK_CHECK(vk.unmapMemory(vkDevice, m_allocation->getMemory()));
}

Texture2D* Texture2D::create (const Context& context, const tcu::Archive& archive, int numLevels, const char* const* levelFileNames)
{
	DE_ASSERT(numLevels > 0);

	std::string ext = de::FilePath(levelFileNames[0]).getFileExtension();

	if (ext == "png")
	{
		// Uncompressed texture.
		tcu::TextureLevel level;

		// Load level 0.
		tcu::ImageIO::loadPNG(level, archive, levelFileNames[0]);

		TCU_CHECK_INTERNAL(level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8) ||
							level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8));

		bool isRGBA = (level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8));
		DE_UNREF(isRGBA);
		// \todo [2015-09-07 elecro] use correct format based on the isRGBA value
		Texture2D* texture = new Texture2D(/*isRGBA ? vk::VK_FORMAT_R8G8B8A8_UNORM :*/ vk::VK_FORMAT_R8G8B8A8_UNORM, /*GL_UNSIGNED_BYTE*/0, level.getWidth(), level.getHeight());

		try
		{
			// Fill level 0.
			texture->getRefTexture().allocLevel(0);

			tcu::copy(texture->getRefTexture().getLevel(0), level.getAccess());

			// Fill remaining levels.
			for (int levelNdx = 1; levelNdx < numLevels; levelNdx++)
			{
				tcu::ImageIO::loadPNG(level, archive, levelFileNames[levelNdx]);

				texture->getRefTexture().allocLevel(levelNdx);
				tcu::copy(texture->getRefTexture().getLevel(levelNdx), level.getAccess());
			}

			texture->upload(context);
		}
		catch (const std::exception&)
		{
			delete texture;
			throw;
		}

		return texture;
	}
	else
		TCU_FAIL("Unsupported file format"); // TODO: maybe support pkm?
}


void Texture2D::loadCompressed (int numLevels, const tcu::CompressedTexture* levels, const tcu::TexDecompressionParams& decompressionParams)
{
	DE_UNREF(numLevels);
	DE_UNREF(levels);
	DE_UNREF(decompressionParams);

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		const tcu::CompressedTexture& level = levels[levelNdx];

		// Decompress to reference texture.
		m_refTexture.allocLevel(levelNdx);
		tcu::PixelBufferAccess refLevelAccess = m_refTexture.getLevel(levelNdx);
		TCU_CHECK(level.getWidth()  == refLevelAccess.getWidth() &&
				  level.getHeight() == refLevelAccess.getHeight());
		level.decompress(refLevelAccess, decompressionParams);

		// \todo [2015-09-07 elecro] add 'upload' logic for compressed image
		TCU_THROW(InternalError, "Compressed image upload not supported yet.");
	}
}


vk::VkTexFilter mapTexFilter (const tcu::Sampler::FilterMode& filterMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:					return vk::VK_TEX_FILTER_LINEAR;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return vk::VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return vk::VK_TEX_FILTER_LINEAR;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_TEX_FILTER_NEAREST;
}

vk::VkTexMipmapMode mapTexMipmapMode (const tcu::Sampler::FilterMode& filterMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return vk::VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::LINEAR:					return vk::VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return vk::VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return vk::VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return vk::VK_TEX_MIPMAP_MODE_LINEAR;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_TEX_MIPMAP_MODE_BASE;
}

vk::VkTexAddress mapWrapMode (const tcu::Sampler::WrapMode& wrapMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(wrapMode)
	{
		case tcu::Sampler::CLAMP_TO_EDGE:		return vk::VK_TEX_ADDRESS_CLAMP;
		case tcu::Sampler::CLAMP_TO_BORDER:		return vk::VK_TEX_ADDRESS_CLAMP_BORDER;
		case tcu::Sampler::REPEAT_GL:			return vk::VK_TEX_ADDRESS_WRAP;
		case tcu::Sampler::REPEAT_CL:			return vk::VK_TEX_ADDRESS_WRAP;
		case tcu::Sampler::MIRRORED_REPEAT_GL:	return vk::VK_TEX_ADDRESS_MIRROR;
		case tcu::Sampler::MIRRORED_REPEAT_CL:	return vk::VK_TEX_ADDRESS_MIRROR;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_TEX_ADDRESS_WRAP;
}

vk::VkCompareOp mapCompareMode (const tcu::Sampler::CompareMode& mode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(mode)
	{
		case tcu::Sampler::COMPAREMODE_NONE:				return vk::VK_COMPARE_OP_NEVER;
		case tcu::Sampler::COMPAREMODE_LESS:				return vk::VK_COMPARE_OP_LESS;
		case tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL:		return vk::VK_COMPARE_OP_LESS_EQUAL;
		case tcu::Sampler::COMPAREMODE_GREATER:				return vk::VK_COMPARE_OP_GREATER;
		case tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL:	return vk::VK_COMPARE_OP_GREATER_EQUAL;
		case tcu::Sampler::COMPAREMODE_EQUAL:				return vk::VK_COMPARE_OP_EQUAL;
		case tcu::Sampler::COMPAREMODE_NOT_EQUAL:			return vk::VK_COMPARE_OP_NOT_EQUAL;
		case tcu::Sampler::COMPAREMODE_ALWAYS:				return vk::VK_COMPARE_OP_ALWAYS;
		case tcu::Sampler::COMPAREMODE_NEVER:				return vk::VK_COMPARE_OP_NEVER;
		default:
			DE_ASSERT(false);
	}

	return vk::VK_COMPARE_OP_NEVER;
}

} // vkt
