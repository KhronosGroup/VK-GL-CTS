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
VkSamplerCreateInfo			mapSampler					(const tcu::Sampler& sampler, const tcu::TextureFormat& format);

void						imageUtilSelfTest			(void);

} // vk

#endif // _VKIMAGEUTIL_HPP
