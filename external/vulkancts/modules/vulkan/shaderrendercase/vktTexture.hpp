#ifndef _VKTTEXTURE_HPP
#define _VKTTEXTURE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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

#include "tcuCompressedTexture.hpp"
#include "tcuTexture.hpp"
#include "tcuResource.hpp"

#include "deUniquePtr.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"

#include "vktTestCase.hpp"

namespace vkt
{

class Texture2D
{
public:
									Texture2D				(int numLevels,
															const tcu::CompressedTexture* levels,
															const tcu::TexDecompressionParams& decompressionParams = tcu::TexDecompressionParams());
									Texture2D				(vk::VkFormat format,
															deUint32 dataType,
															int width,
															int height);
									Texture2D				(vk::VkFormat format,
															int width,
															int height);
									~Texture2D				(void);

	tcu::Texture2D&					getRefTexture			(void) 			{ return m_refTexture; }
	const tcu::Texture2D&			getRefTexture			(void) const	{ return m_refTexture; }
	const vk::VkImage*				getVkTexture			(void) const	{ return &m_vkTexture.get(); }
	vk::VkFormat					getVkFormat				(void) const	{ return (vk::VkFormat)m_format; }


	static Texture2D* 				create 					(const Context& context,
															const tcu::Archive& archive,
															int numLevels,
															const char* const* levelFileNames);
	static Texture2D*				create					(const Context& context,
															const tcu::Archive& archive,
															const char* filename)
															{ return Texture2D::create(context, archive, 1, &filename); }

private:
									Texture2D				(const Texture2D& other);	// Not allowed!
	Texture2D&						operator=				(const Texture2D& other);	// Not allowed!


	void							upload					(const Context& context);

	void							loadCompressed			(int numLevels,
															const tcu::CompressedTexture* levels,
															const tcu::TexDecompressionParams& decompressionParams);

	bool							m_isCompressed;
	vk::VkFormat					m_format;
	tcu::Texture2D					m_refTexture;
	vk::Move<vk::VkImage>			m_vkTexture;

	de::MovePtr<vk::Allocation>		m_allocation;
};

vk::VkTexFilter getVkTexFilter (const tcu::Sampler::FilterMode& filterMode);
vk::VkTexMipmapMode getVkTexMipmapMode (const tcu::Sampler::FilterMode& filterMode);
vk::VkTexAddress getVkWrapMode (const tcu::Sampler::WrapMode& wrapMode);
vk::VkCompareOp getVkCompareMode (const tcu::Sampler::CompareMode& mode);

} // vkt

#endif // _VKTSHADERRENDERCASE_HPP
