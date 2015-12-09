#ifndef _VKTPIPELINEIMAGEUTIL_HPP
#define _VKTPIPELINEIMAGEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Utilities for images.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vkDefs.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"

namespace vkt
{
namespace pipeline
{

class TestTexture;

enum BorderColor
{
	BORDER_COLOR_OPAQUE_BLACK,
	BORDER_COLOR_OPAQUE_WHITE,
	BORDER_COLOR_TRANSPARENT_BLACK,

	BORDER_COLOR_COUNT
};

bool							isSupportedSamplableFormat	(const vk::InstanceInterface& instanceInterface,
															 vk::VkPhysicalDevice device,
															 vk::VkFormat format);

vk::VkBorderColor				getFormatBorderColor		(BorderColor color, vk::VkFormat format);

/*--------------------------------------------------------------------*//*!
 * Gets a tcu::TextureLevel initialized with data from a VK color
 * attachment.
 *
 * The VkImage must be non-multisampled and able to be used as a source
 * operand for transfer operations.
 *//*--------------------------------------------------------------------*/
de::MovePtr<tcu::TextureLevel>	readColorAttachment			 (const vk::DeviceInterface&	vk,
															  vk::VkDevice					device,
															  vk::VkQueue					queue,
															  deUint32						queueFamilyIndex,
															  vk::Allocator&				allocator,
															  vk::VkImage					image,
															  vk::VkFormat					format,
															  const tcu::IVec2&				renderSize);

/*--------------------------------------------------------------------*//*!
 * Uploads data from a test texture to a destination VK image.
 *
 * The VkImage must be non-multisampled and able to be used as a
 * destination operand for transfer operations.
 *//*--------------------------------------------------------------------*/
void							uploadTestTexture			(const vk::DeviceInterface&		vk,
															 vk::VkDevice					device,
															 vk::VkQueue					queue,
															 deUint32						queueFamilyIndex,
															 vk::Allocator&					allocator,
															 const TestTexture&				testTexture,
															 vk::VkImage					destImage);

class TestTexture
{
public:
												TestTexture					(const tcu::TextureFormat& format, int width, int height, int depth);
												TestTexture					(const tcu::CompressedTexFormat& format, int width, int height, int depth);
	virtual										~TestTexture				(void);

	virtual int									getNumLevels				(void) const = 0;
	virtual deUint32							getSize						(void) const;
	virtual int									getArraySize				(void) const { return 1; }

	virtual bool								isCompressed				(void) const { return !m_compressedLevels.empty(); }
	virtual deUint32							getCompressedSize			(void) const;

	virtual tcu::PixelBufferAccess				getLevel					(int level, int layer) = 0;
	virtual const tcu::ConstPixelBufferAccess	getLevel					(int level, int layer) const = 0;

	virtual tcu::CompressedTexture&				getCompressedLevel			(int level, int layer);
	virtual const tcu::CompressedTexture&		getCompressedLevel			(int level, int layer) const;

	virtual std::vector<vk::VkBufferImageCopy>	getBufferCopyRegions		(void) const;
	virtual void								write						(deUint8* destPtr) const;

protected:
	void										populateLevels				(const std::vector<tcu::PixelBufferAccess>& levels);
	void										populateCompressedLevels	(const tcu::CompressedTexFormat& format, const std::vector<tcu::PixelBufferAccess>& decompressedLevels);

	static void									fillWithGradient			(const tcu::PixelBufferAccess& levelAccess);

protected:
	std::vector<tcu::CompressedTexture*>		m_compressedLevels;
};

class TestTexture1D : public TestTexture
{
private:
	tcu::Texture1D								m_texture;

public:
												TestTexture1D	(const tcu::TextureFormat& format, int width);
												TestTexture1D	(const tcu::CompressedTexFormat& format, int width);
	virtual										~TestTexture1D	(void);

	virtual int getNumLevels (void) const;
	virtual tcu::PixelBufferAccess				getLevel		(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel		(int level, int layer) const;
	virtual const tcu::Texture1D&				getTexture		(void) const;
};

class TestTexture1DArray : public TestTexture
{
private:
	tcu::Texture1DArray							m_texture;

public:
												TestTexture1DArray	(const tcu::TextureFormat& format, int width, int arraySize);
												TestTexture1DArray	(const tcu::CompressedTexFormat& format, int width, int arraySize);
	virtual										~TestTexture1DArray	(void);

	virtual int									getNumLevels		(void) const;
	virtual tcu::PixelBufferAccess				getLevel			(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel			(int level, int layer) const;
	virtual const tcu::Texture1DArray&			getTexture			(void) const;
	virtual int									getArraySize		(void) const;
};

class TestTexture2D : public TestTexture
{
private:
	tcu::Texture2D								m_texture;

public:
												TestTexture2D		(const tcu::TextureFormat& format, int width, int height);
												TestTexture2D		(const tcu::CompressedTexFormat& format, int width, int height);
	virtual										~TestTexture2D		(void);

	virtual int									getNumLevels		(void) const;
	virtual tcu::PixelBufferAccess				getLevel			(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel			(int level, int layer) const;
	virtual const tcu::Texture2D&				getTexture			(void) const;
};

class TestTexture2DArray : public TestTexture
{
private:
	tcu::Texture2DArray	m_texture;

public:
												TestTexture2DArray	(const tcu::TextureFormat& format, int width, int height, int arraySize);
												TestTexture2DArray	(const tcu::CompressedTexFormat& format, int width, int height, int arraySize);
	virtual										~TestTexture2DArray	(void);

	virtual int									getNumLevels		(void) const;
	virtual tcu::PixelBufferAccess				getLevel			(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel			(int level, int layer) const;
	virtual const tcu::Texture2DArray&			getTexture			(void) const;
	virtual int									getArraySize		(void) const;
};

class TestTexture3D : public TestTexture
{
private:
	tcu::Texture3D	m_texture;

public:
												TestTexture3D		(const tcu::TextureFormat& format, int width, int height, int depth);
												TestTexture3D		(const tcu::CompressedTexFormat& format, int width, int height, int depth);
	virtual										~TestTexture3D		(void);

	virtual int									getNumLevels		(void) const;
	virtual tcu::PixelBufferAccess				getLevel			(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel			(int level, int layer) const;
	virtual const tcu::Texture3D&				getTexture			(void) const;
};

class TestTextureCube : public TestTexture
{
private:
	tcu::TextureCube							m_texture;

public:
												TestTextureCube			(const tcu::TextureFormat& format, int size);
												TestTextureCube			(const tcu::CompressedTexFormat& format, int size);
	virtual										~TestTextureCube		(void);

	virtual int									getNumLevels			(void) const;
	virtual tcu::PixelBufferAccess				getLevel				(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel				(int level, int layer) const;
	virtual int									getArraySize			(void) const;
	virtual const tcu::TextureCube&				getTexture				(void) const;
};

class TestTextureCubeArray: public TestTexture
{
private:
	tcu::TextureCubeArray						m_texture;

public:
												TestTextureCubeArray	(const tcu::TextureFormat& format, int size, int arraySize);
												TestTextureCubeArray	(const tcu::CompressedTexFormat& format, int size, int arraySize);
	virtual										~TestTextureCubeArray	(void);

	virtual int									getNumLevels			(void) const;
	virtual tcu::PixelBufferAccess				getLevel				(int level, int layer);
	virtual const tcu::ConstPixelBufferAccess	getLevel				(int level, int layer) const;
	virtual int									getArraySize			(void) const;
	virtual const tcu::TextureCubeArray&		getTexture				(void) const;
};

} // pipeline
} // vkt

#endif // _VKTPIPELINEIMAGEUTIL_HPP
