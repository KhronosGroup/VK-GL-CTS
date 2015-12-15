#ifndef _VKIMAGEUTIL_HPP
#define _VKIMAGEUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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

#include "vkDefs.hpp"
#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"

namespace vk
{

bool						isFloatFormat				(VkFormat format);
bool						isUnormFormat				(VkFormat format);
bool						isSnormFormat				(VkFormat format);
bool						isIntFormat					(VkFormat format);
bool						isUintFormat				(VkFormat format);
bool						isDepthStencilFormat		(VkFormat format);
bool						isCompressedFormat			(VkFormat format);

tcu::TextureFormat			mapVkFormat					(VkFormat format);
tcu::CompressedTexFormat	mapVkCompressedFormat		(VkFormat format);
tcu::TextureFormat			getDepthCopyFormat			(VkFormat combinedFormat);
tcu::TextureFormat			getStencilCopyFormat		(VkFormat combinedFormat);

tcu::Sampler				mapVkSampler				(const VkSamplerCreateInfo& samplerCreateInfo);
tcu::Sampler::CompareMode	mapVkSamplerCompareOp		(VkCompareOp compareOp);
tcu::Sampler::WrapMode		mapVkSamplerAddressMode		(VkSamplerAddressMode addressMode);
tcu::Sampler::FilterMode	mapVkMinTexFilter			(VkFilter filter, VkSamplerMipmapMode mipMode);
tcu::Sampler::FilterMode	mapVkMagTexFilter			(VkFilter filter);
int							mapVkComponentSwizzle		(const VkComponentSwizzle& channelSwizzle);
tcu::UVec4					mapVkComponentMapping		(const vk::VkComponentMapping& mapping);

VkComponentMapping			getFormatComponentMapping	(VkFormat format);
VkFilter					mapFilterMode				(tcu::Sampler::FilterMode filterMode);
VkSamplerMipmapMode			mapMipmapMode				(tcu::Sampler::FilterMode filterMode);
VkSamplerAddressMode		mapWrapMode					(tcu::Sampler::WrapMode wrapMode);
VkCompareOp					mapCompareMode				(tcu::Sampler::CompareMode mode);
VkFormat					mapTextureFormat			(const tcu::TextureFormat& format);

void						imageUtilSelfTest			(void);

} // vk

#endif // _VKIMAGEUTIL_HPP
