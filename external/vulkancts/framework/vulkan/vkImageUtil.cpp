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

#include "vkImageUtil.hpp"
#include "tcuTextureUtil.hpp"

namespace vk
{

bool isFloatFormat (VkFormat format)
{
	return tcu::getTextureChannelClass(mapVkFormat(format).type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
}

bool isUnormFormat (VkFormat format)
{
	return tcu::getTextureChannelClass(mapVkFormat(format).type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
}

bool isSnormFormat (VkFormat format)
{
	return tcu::getTextureChannelClass(mapVkFormat(format).type) == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
}

bool isIntFormat (VkFormat format)
{
	return tcu::getTextureChannelClass(mapVkFormat(format).type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER;
}

bool isUintFormat (VkFormat format)
{
	return tcu::getTextureChannelClass(mapVkFormat(format).type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
}

bool isDepthStencilFormat (VkFormat format)
{
	if (isCompressedFormat(format))
		return false;

	const tcu::TextureFormat tcuFormat = mapVkFormat(format);
	return tcuFormat.order == tcu::TextureFormat::D || tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS;
}

bool isCompressedFormat (VkFormat format)
{
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 185);

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
		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
		case VK_FORMAT_EAC_R11_UNORM_BLOCK:
		case VK_FORMAT_EAC_R11_SNORM_BLOCK:
		case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
		case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
		case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
		case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
		case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
		case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
		case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
			return true;

		default:
			return false;
	}
}

VkFormat mapTextureFormat (const tcu::TextureFormat& format)
{
	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST < (1<<16));
	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELTYPE_LAST < (1<<16));

#define PACK_FMT(ORDER, TYPE) ((int(ORDER) << 16) | int(TYPE))
#define FMT_CASE(ORDER, TYPE) PACK_FMT(tcu::TextureFormat::ORDER, tcu::TextureFormat::TYPE)

	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 185);

	switch (PACK_FMT(format.order, format.type))
	{
		case FMT_CASE(RG, UNORM_BYTE_44):					return VK_FORMAT_R4G4_UNORM_PACK8;
		case FMT_CASE(RGB, UNORM_SHORT_565):				return VK_FORMAT_R5G6B5_UNORM_PACK16;
		case FMT_CASE(RGBA, UNORM_SHORT_4444):				return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
		case FMT_CASE(RGBA, UNORM_SHORT_5551):				return VK_FORMAT_R5G5B5A1_UNORM_PACK16;

		case FMT_CASE(BGR, UNORM_SHORT_565):				return VK_FORMAT_B5G6R5_UNORM_PACK16;
		case FMT_CASE(BGRA, UNORM_SHORT_4444):				return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case FMT_CASE(BGRA, UNORM_SHORT_5551):				return VK_FORMAT_B5G5R5A1_UNORM_PACK16;

		case FMT_CASE(ARGB, UNORM_SHORT_1555):				return VK_FORMAT_A1R5G5B5_UNORM_PACK16;

		case FMT_CASE(R, UNORM_INT8):						return VK_FORMAT_R8_UNORM;
		case FMT_CASE(R, SNORM_INT8):						return VK_FORMAT_R8_SNORM;
		case FMT_CASE(R, UNSIGNED_INT8):					return VK_FORMAT_R8_UINT;
		case FMT_CASE(R, SIGNED_INT8):						return VK_FORMAT_R8_SINT;
		case FMT_CASE(sR, UNORM_INT8):						return VK_FORMAT_R8_SRGB;

		case FMT_CASE(RG, UNORM_INT8):						return VK_FORMAT_R8G8_UNORM;
		case FMT_CASE(RG, SNORM_INT8):						return VK_FORMAT_R8G8_SNORM;
		case FMT_CASE(RG, UNSIGNED_INT8):					return VK_FORMAT_R8G8_UINT;
		case FMT_CASE(RG, SIGNED_INT8):						return VK_FORMAT_R8G8_SINT;
		case FMT_CASE(sRG, UNORM_INT8):						return VK_FORMAT_R8G8_SRGB;

		case FMT_CASE(RGB, UNORM_INT8):						return VK_FORMAT_R8G8B8_UNORM;
		case FMT_CASE(RGB, SNORM_INT8):						return VK_FORMAT_R8G8B8_SNORM;
		case FMT_CASE(RGB, UNSIGNED_INT8):					return VK_FORMAT_R8G8B8_UINT;
		case FMT_CASE(RGB, SIGNED_INT8):					return VK_FORMAT_R8G8B8_SINT;
		case FMT_CASE(sRGB, UNORM_INT8):					return VK_FORMAT_R8G8B8_SRGB;

		case FMT_CASE(RGBA, UNORM_INT8):					return VK_FORMAT_R8G8B8A8_UNORM;
		case FMT_CASE(RGBA, SNORM_INT8):					return VK_FORMAT_R8G8B8A8_SNORM;
		case FMT_CASE(RGBA, UNSIGNED_INT8):					return VK_FORMAT_R8G8B8A8_UINT;
		case FMT_CASE(RGBA, SIGNED_INT8):					return VK_FORMAT_R8G8B8A8_SINT;
		case FMT_CASE(sRGBA, UNORM_INT8):					return VK_FORMAT_R8G8B8A8_SRGB;

		case FMT_CASE(RGBA, UNORM_INT_1010102_REV):			return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case FMT_CASE(RGBA, SNORM_INT_1010102_REV):			return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
		case FMT_CASE(RGBA, UNSIGNED_INT_1010102_REV):		return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		case FMT_CASE(RGBA, SIGNED_INT_1010102_REV):		return VK_FORMAT_A2B10G10R10_SINT_PACK32;

		case FMT_CASE(R, UNORM_INT16):						return VK_FORMAT_R16_UNORM;
		case FMT_CASE(R, SNORM_INT16):						return VK_FORMAT_R16_SNORM;
		case FMT_CASE(R, UNSIGNED_INT16):					return VK_FORMAT_R16_UINT;
		case FMT_CASE(R, SIGNED_INT16):						return VK_FORMAT_R16_SINT;
		case FMT_CASE(R, HALF_FLOAT):						return VK_FORMAT_R16_SFLOAT;

		case FMT_CASE(RG, UNORM_INT16):						return VK_FORMAT_R16G16_UNORM;
		case FMT_CASE(RG, SNORM_INT16):						return VK_FORMAT_R16G16_SNORM;
		case FMT_CASE(RG, UNSIGNED_INT16):					return VK_FORMAT_R16G16_UINT;
		case FMT_CASE(RG, SIGNED_INT16):					return VK_FORMAT_R16G16_SINT;
		case FMT_CASE(RG, HALF_FLOAT):						return VK_FORMAT_R16G16_SFLOAT;

		case FMT_CASE(RGB, UNORM_INT16):					return VK_FORMAT_R16G16B16_UNORM;
		case FMT_CASE(RGB, SNORM_INT16):					return VK_FORMAT_R16G16B16_SNORM;
		case FMT_CASE(RGB, UNSIGNED_INT16):					return VK_FORMAT_R16G16B16_UINT;
		case FMT_CASE(RGB, SIGNED_INT16):					return VK_FORMAT_R16G16B16_SINT;
		case FMT_CASE(RGB, HALF_FLOAT):						return VK_FORMAT_R16G16B16_SFLOAT;

		case FMT_CASE(RGBA, UNORM_INT16):					return VK_FORMAT_R16G16B16A16_UNORM;
		case FMT_CASE(RGBA, SNORM_INT16):					return VK_FORMAT_R16G16B16A16_SNORM;
		case FMT_CASE(RGBA, UNSIGNED_INT16):				return VK_FORMAT_R16G16B16A16_UINT;
		case FMT_CASE(RGBA, SIGNED_INT16):					return VK_FORMAT_R16G16B16A16_SINT;
		case FMT_CASE(RGBA, HALF_FLOAT):					return VK_FORMAT_R16G16B16A16_SFLOAT;

		case FMT_CASE(R, UNSIGNED_INT32):					return VK_FORMAT_R32_UINT;
		case FMT_CASE(R, SIGNED_INT32):						return VK_FORMAT_R32_SINT;
		case FMT_CASE(R, FLOAT):							return VK_FORMAT_R32_SFLOAT;

		case FMT_CASE(RG, UNSIGNED_INT32):					return VK_FORMAT_R32G32_UINT;
		case FMT_CASE(RG, SIGNED_INT32):					return VK_FORMAT_R32G32_SINT;
		case FMT_CASE(RG, FLOAT):							return VK_FORMAT_R32G32_SFLOAT;

		case FMT_CASE(RGB, UNSIGNED_INT32):					return VK_FORMAT_R32G32B32_UINT;
		case FMT_CASE(RGB, SIGNED_INT32):					return VK_FORMAT_R32G32B32_SINT;
		case FMT_CASE(RGB, FLOAT):							return VK_FORMAT_R32G32B32_SFLOAT;

		case FMT_CASE(RGBA, UNSIGNED_INT32):				return VK_FORMAT_R32G32B32A32_UINT;
		case FMT_CASE(RGBA, SIGNED_INT32):					return VK_FORMAT_R32G32B32A32_SINT;
		case FMT_CASE(RGBA, FLOAT):							return VK_FORMAT_R32G32B32A32_SFLOAT;

		case FMT_CASE(R, FLOAT64):							return VK_FORMAT_R64_SFLOAT;
		case FMT_CASE(RG, FLOAT64):							return VK_FORMAT_R64G64_SFLOAT;
		case FMT_CASE(RGB, FLOAT64):						return VK_FORMAT_R64G64B64_SFLOAT;
		case FMT_CASE(RGBA, FLOAT64):						return VK_FORMAT_R64G64B64A64_SFLOAT;

		case FMT_CASE(RGB, UNSIGNED_INT_11F_11F_10F_REV):	return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case FMT_CASE(RGB, UNSIGNED_INT_999_E5_REV):		return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

		case FMT_CASE(BGR, UNORM_INT8):						return VK_FORMAT_B8G8R8_UNORM;
		case FMT_CASE(BGR, SNORM_INT8):						return VK_FORMAT_B8G8R8_SNORM;
		case FMT_CASE(BGR, UNSIGNED_INT8):					return VK_FORMAT_B8G8R8_UINT;
		case FMT_CASE(BGR, SIGNED_INT8):					return VK_FORMAT_B8G8R8_SINT;
		case FMT_CASE(sBGR, UNORM_INT8):					return VK_FORMAT_B8G8R8_SRGB;

		case FMT_CASE(BGRA, UNORM_INT8):					return VK_FORMAT_B8G8R8A8_UNORM;
		case FMT_CASE(BGRA, SNORM_INT8):					return VK_FORMAT_B8G8R8A8_SNORM;
		case FMT_CASE(BGRA, UNSIGNED_INT8):					return VK_FORMAT_B8G8R8A8_UINT;
		case FMT_CASE(BGRA, SIGNED_INT8):					return VK_FORMAT_B8G8R8A8_SINT;
		case FMT_CASE(sBGRA, UNORM_INT8):					return VK_FORMAT_B8G8R8A8_SRGB;

		case FMT_CASE(BGRA, UNORM_INT_1010102_REV):			return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
		case FMT_CASE(BGRA, SNORM_INT_1010102_REV):			return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
		case FMT_CASE(BGRA, UNSIGNED_INT_1010102_REV):		return VK_FORMAT_A2R10G10B10_UINT_PACK32;
		case FMT_CASE(BGRA, SIGNED_INT_1010102_REV):		return VK_FORMAT_A2R10G10B10_SINT_PACK32;

		case FMT_CASE(D, UNORM_INT16):						return VK_FORMAT_D16_UNORM;
		case FMT_CASE(D, UNSIGNED_INT_24_8_REV):			return VK_FORMAT_X8_D24_UNORM_PACK32;
		case FMT_CASE(D, FLOAT):							return VK_FORMAT_D32_SFLOAT;

		case FMT_CASE(S, UNSIGNED_INT8):					return VK_FORMAT_S8_UINT;

		case FMT_CASE(DS, UNSIGNED_INT_16_8_8):				return VK_FORMAT_D16_UNORM_S8_UINT;
		case FMT_CASE(DS, UNSIGNED_INT_24_8_REV):			return VK_FORMAT_D24_UNORM_S8_UINT;
		case FMT_CASE(DS, FLOAT_UNSIGNED_INT_24_8_REV):		return VK_FORMAT_D32_SFLOAT_S8_UINT;

		default:
			TCU_THROW(InternalError, "Unknown texture format");
	}

#undef PACK_FMT
#undef FMT_CASE
}

tcu::TextureFormat mapVkFormat (VkFormat format)
{
	using tcu::TextureFormat;

	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 185);

	switch (format)
	{
		case VK_FORMAT_R4G4_UNORM_PACK8:		return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_BYTE_44);
		case VK_FORMAT_R5G6B5_UNORM_PACK16:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_SHORT_565);
		case VK_FORMAT_R4G4B4A4_UNORM_PACK16:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_4444);
		case VK_FORMAT_R5G5B5A1_UNORM_PACK16:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_5551);

		case VK_FORMAT_B5G6R5_UNORM_PACK16:		return TextureFormat(TextureFormat::BGR,	TextureFormat::UNORM_SHORT_565);
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_SHORT_4444);
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_SHORT_5551);

		case VK_FORMAT_A1R5G5B5_UNORM_PACK16:	return TextureFormat(TextureFormat::ARGB,	TextureFormat::UNORM_SHORT_1555);

		case VK_FORMAT_R8_UNORM:				return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8_SNORM:				return TextureFormat(TextureFormat::R,		TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8_USCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8_SSCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8_UINT:					return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8_SINT:					return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8_SRGB:					return TextureFormat(TextureFormat::sR,		TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8_UNORM:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8_SNORM:				return TextureFormat(TextureFormat::RG,		TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8_USCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8_SSCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8_UINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8_SINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8_SRGB:				return TextureFormat(TextureFormat::sRG,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8B8_UNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8B8_SNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8B8_USCALED:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8_SSCALED:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8_UINT:				return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8_SINT:				return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8_SRGB:				return TextureFormat(TextureFormat::sRGB,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8B8A8_UNORM:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8B8A8_SNORM:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8B8A8_USCALED:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SSCALED:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_UINT:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SINT:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SRGB:			return TextureFormat(TextureFormat::sRGBA,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R16_UNORM:				return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16_SNORM:				return TextureFormat(TextureFormat::R,		TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16_USCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16_SSCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16_UINT:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16_SINT:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16_SFLOAT:				return TextureFormat(TextureFormat::R,		TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16_UNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16_SNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16_USCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16_SSCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16_UINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16_SINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16_SFLOAT:			return TextureFormat(TextureFormat::RG,		TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16B16_UNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16B16_SNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16B16_USCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16_SSCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16_UINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16_SINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16_SFLOAT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16B16A16_UNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16B16A16_SNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16B16A16_USCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_SSCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_UINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_SINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_SFLOAT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R32_UINT:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32_SINT:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32_SFLOAT:				return TextureFormat(TextureFormat::R,		TextureFormat::FLOAT);

		case VK_FORMAT_R32G32_UINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32G32_SINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32G32_SFLOAT:			return TextureFormat(TextureFormat::RG,		TextureFormat::FLOAT);

		case VK_FORMAT_R32G32B32_UINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32G32B32_SINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32G32B32_SFLOAT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::FLOAT);

		case VK_FORMAT_R32G32B32A32_UINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32G32B32A32_SINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32G32B32A32_SFLOAT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::FLOAT);

		case VK_FORMAT_R64_SFLOAT:				return TextureFormat(TextureFormat::R,		TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64_SFLOAT:			return TextureFormat(TextureFormat::RG,		TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64B64_SFLOAT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64B64A64_SFLOAT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::FLOAT64);

		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:	return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT_11F_11F_10F_REV);
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:	return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT_999_E5_REV);

		case VK_FORMAT_B8G8R8_UNORM:			return TextureFormat(TextureFormat::BGR,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_B8G8R8_SNORM:			return TextureFormat(TextureFormat::BGR,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_B8G8R8_USCALED:			return TextureFormat(TextureFormat::BGR,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8_SSCALED:			return TextureFormat(TextureFormat::BGR,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8_UINT:				return TextureFormat(TextureFormat::BGR,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8_SINT:				return TextureFormat(TextureFormat::BGR,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8_SRGB:				return TextureFormat(TextureFormat::sBGR,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_B8G8R8A8_UNORM:			return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_B8G8R8A8_SNORM:			return TextureFormat(TextureFormat::BGRA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_B8G8R8A8_USCALED:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_SSCALED:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_UINT:			return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_SINT:			return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_SRGB:			return TextureFormat(TextureFormat::sBGRA,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_D16_UNORM:				return TextureFormat(TextureFormat::D,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_X8_D24_UNORM_PACK32:		return TextureFormat(TextureFormat::D,		TextureFormat::UNSIGNED_INT_24_8_REV);
		case VK_FORMAT_D32_SFLOAT:				return TextureFormat(TextureFormat::D,		TextureFormat::FLOAT);

		case VK_FORMAT_S8_UINT:					return TextureFormat(TextureFormat::S,		TextureFormat::UNSIGNED_INT8);

		// \note There is no standard interleaved memory layout for DS formats; buffer-image copies
		//		 will always operate on either D or S aspect only. See Khronos bug 12998
		case VK_FORMAT_D16_UNORM_S8_UINT:		return TextureFormat(TextureFormat::DS,		TextureFormat::UNSIGNED_INT_16_8_8);
		case VK_FORMAT_D24_UNORM_S8_UINT:		return TextureFormat(TextureFormat::DS,		TextureFormat::UNSIGNED_INT_24_8_REV);
		case VK_FORMAT_D32_SFLOAT_S8_UINT:		return TextureFormat(TextureFormat::DS,		TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV);

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_A8B8G8R8_SNORM_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_A8B8G8R8_USCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_A8B8G8R8_UINT_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_A8B8G8R8_SINT_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:	return TextureFormat(TextureFormat::sRGBA,	TextureFormat::UNORM_INT8);
#else
#	error "Big-endian not supported"
#endif

		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_SNORM_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SNORM_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_UINT_PACK32:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_SINT_PACK32:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT_1010102_REV);

		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_SNORM_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_USCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_SINT_PACK32:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT_1010102_REV);


		default:
			TCU_THROW(InternalError, "Unknown image format");
	}
}

tcu::CompressedTexFormat mapVkCompressedFormat (VkFormat format)
{
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 185);

	switch (format)
	{
		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8;
		case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8;
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:	return tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1;
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:	return tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1;
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:	return tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8;
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:	return tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8;
		case VK_FORMAT_EAC_R11_UNORM_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_EAC_R11;
		case VK_FORMAT_EAC_R11_SNORM_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11;
		case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_EAC_RG11;
		case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11;
		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA;
		case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA;
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA;
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA;
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA;
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA;
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA;
		case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA;
		case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA;
		case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA;
		case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA;
		case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA;
		case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA;
		case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA;
		case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8;
		default:
			break;
	}

	return tcu::COMPRESSEDTEXFORMAT_LAST;
}

VkComponentMapping getFormatComponentMapping (VkFormat format)
{
	using tcu::TextureFormat;

	static const VkComponentMapping	R		= {	VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ONE	};
	static const VkComponentMapping	RG		= {	VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ONE	};
	static const VkComponentMapping	RGB		= {	VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_B,		VK_COMPONENT_SWIZZLE_ONE	};
	static const VkComponentMapping	RGBA	= {	VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_B,		VK_COMPONENT_SWIZZLE_A		};
	static const VkComponentMapping	S		= { VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_A		};
	static const VkComponentMapping	DS		= {	VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_ZERO,	VK_COMPONENT_SWIZZLE_A		};
	static const VkComponentMapping	BGRA	= {	VK_COMPONENT_SWIZZLE_B,		VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_A		};
	static const VkComponentMapping	BGR		= {	VK_COMPONENT_SWIZZLE_B,		VK_COMPONENT_SWIZZLE_G,		VK_COMPONENT_SWIZZLE_R,		VK_COMPONENT_SWIZZLE_ONE	};

	if (format == VK_FORMAT_UNDEFINED)
		return RGBA;

	const tcu::TextureFormat tcuFormat = (isCompressedFormat(format)) ? tcu::getUncompressedFormat(mapVkCompressedFormat(format))
																	  : mapVkFormat(format);

	switch (tcuFormat.order)
	{
		case TextureFormat::R:		return R;
		case TextureFormat::RG:		return RG;
		case TextureFormat::RGB:	return RGB;
		case TextureFormat::RGBA:	return RGBA;
		case TextureFormat::BGRA:	return BGRA;
		case TextureFormat::BGR:	return BGR;
		case TextureFormat::sR:		return R;
		case TextureFormat::sRG:	return RG;
		case TextureFormat::sRGB:	return RGB;
		case TextureFormat::sRGBA:	return RGBA;
		case TextureFormat::sBGR:	return BGR;
		case TextureFormat::sBGRA:	return BGRA;
		case TextureFormat::D:		return R;
		case TextureFormat::S:		return S;
		case TextureFormat::DS:		return DS;
		default:
			break;
	}

	DE_ASSERT(false);
	return RGBA;
}

static bool isScaledFormat (VkFormat format)
{
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 185);

	switch (format)
	{
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
		case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
			return true;

		default:
			return false;
	}
}

static bool fullTextureFormatRoundTripSupported (VkFormat format)
{
	if (isScaledFormat(format))
	{
		// *SCALED formats get mapped to correspoding (u)int formats since
		// accessing them through (float) getPixel/setPixel has same behavior
		// as in shader access in Vulkan.
		// Unfortunately full round-trip between tcu::TextureFormat and VkFormat
		// for most SCALED formats is not supported though.

		const tcu::TextureFormat	tcuFormat	= mapVkFormat(format);

		switch (tcuFormat.type)
		{
			case tcu::TextureFormat::UNSIGNED_INT8:
			case tcu::TextureFormat::UNSIGNED_INT16:
			case tcu::TextureFormat::UNSIGNED_INT32:
			case tcu::TextureFormat::SIGNED_INT8:
			case tcu::TextureFormat::SIGNED_INT16:
			case tcu::TextureFormat::SIGNED_INT32:
			case tcu::TextureFormat::UNSIGNED_INT_1010102_REV:
			case tcu::TextureFormat::SIGNED_INT_1010102_REV:
				return false;

			default:
				return true;
		}
	}
	else
	{
		switch (format)
		{
			case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
			case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
			case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
			case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
			case VK_FORMAT_A8B8G8R8_UINT_PACK32:
			case VK_FORMAT_A8B8G8R8_SINT_PACK32:
			case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
				return false; // These map to regular byte array formats

			default:
				break;
		}

		return (format != VK_FORMAT_UNDEFINED);
	}
}

void imageUtilSelfTest (void)
{
	for (int formatNdx = 0; formatNdx < VK_FORMAT_LAST; formatNdx++)
	{
		const VkFormat	format	= (VkFormat)formatNdx;

		if (format == VK_FORMAT_R64_UINT			||
			format == VK_FORMAT_R64_SINT			||
			format == VK_FORMAT_R64G64_UINT			||
			format == VK_FORMAT_R64G64_SINT			||
			format == VK_FORMAT_R64G64B64_UINT		||
			format == VK_FORMAT_R64G64B64_SINT		||
			format == VK_FORMAT_R64G64B64A64_UINT	||
			format == VK_FORMAT_R64G64B64A64_SINT)
			continue; // \todo [2015-12-05 pyry] Add framework support for (u)int64 channel type

		if (format != VK_FORMAT_UNDEFINED && !isCompressedFormat(format))
		{
			const tcu::TextureFormat	tcuFormat		= mapVkFormat(format);
			const VkFormat				remappedFormat	= mapTextureFormat(tcuFormat);

			DE_TEST_ASSERT(isValid(tcuFormat));

			if (fullTextureFormatRoundTripSupported(format))
				DE_TEST_ASSERT(format == remappedFormat);
		}
	}
}

VkFilter mapFilterMode (tcu::Sampler::FilterMode filterMode)
{
	DE_STATIC_ASSERT(tcu::Sampler::FILTERMODE_LAST == 6);

	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return VK_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:					return VK_FILTER_LINEAR;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return VK_FILTER_NEAREST;
		case tcu::Sampler::NEAREST_MIPMAP_LINEAR:	return VK_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return VK_FILTER_LINEAR;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return VK_FILTER_LINEAR;
		default:
			DE_FATAL("Illegal filter mode");
			return (VkFilter)0;

	}
}

VkSamplerMipmapMode mapMipmapMode (tcu::Sampler::FilterMode filterMode)
{
	DE_STATIC_ASSERT(tcu::Sampler::FILTERMODE_LAST == 6);

	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return VK_SAMPLER_MIPMAP_MODE_BASE;
		case tcu::Sampler::LINEAR:					return VK_SAMPLER_MIPMAP_MODE_BASE;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::NEAREST_MIPMAP_LINEAR:	return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		default:
			DE_FATAL("Illegal filter mode");
			return (VkSamplerMipmapMode)0;
	}
}

VkSamplerAddressMode mapWrapMode (tcu::Sampler::WrapMode wrapMode)
{
	switch(wrapMode)
	{
		case tcu::Sampler::CLAMP_TO_EDGE:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case tcu::Sampler::CLAMP_TO_BORDER:		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case tcu::Sampler::REPEAT_GL:			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case tcu::Sampler::MIRRORED_REPEAT_GL:	return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case tcu::Sampler::MIRRORED_ONCE:		return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		default:
			DE_FATAL("Wrap mode can't be mapped to Vulkan");
			return (vk::VkSamplerAddressMode)0;
	}
}

vk::VkCompareOp mapCompareMode (tcu::Sampler::CompareMode mode)
{
	switch(mode)
	{
		case tcu::Sampler::COMPAREMODE_NONE:				return vk::VK_COMPARE_OP_NEVER;
		case tcu::Sampler::COMPAREMODE_LESS:				return vk::VK_COMPARE_OP_LESS;
		case tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL:		return vk::VK_COMPARE_OP_LESS_OR_EQUAL;
		case tcu::Sampler::COMPAREMODE_GREATER:				return vk::VK_COMPARE_OP_GREATER;
		case tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL:	return vk::VK_COMPARE_OP_GREATER_OR_EQUAL;
		case tcu::Sampler::COMPAREMODE_EQUAL:				return vk::VK_COMPARE_OP_EQUAL;
		case tcu::Sampler::COMPAREMODE_NOT_EQUAL:			return vk::VK_COMPARE_OP_NOT_EQUAL;
		case tcu::Sampler::COMPAREMODE_ALWAYS:				return vk::VK_COMPARE_OP_ALWAYS;
		case tcu::Sampler::COMPAREMODE_NEVER:				return vk::VK_COMPARE_OP_NEVER;
		default:
			DE_FATAL("Illegal compare mode");
			return (vk::VkCompareOp)0;
	}
}

tcu::Sampler mapVkSampler (const VkSamplerCreateInfo& samplerCreateInfo)
{
	tcu::Sampler sampler(mapVkSamplerAddressMode(samplerCreateInfo.addressModeU),
						 mapVkSamplerAddressMode(samplerCreateInfo.addressModeV),
						 mapVkSamplerAddressMode(samplerCreateInfo.addressModeW),
						 mapVkMinTexFilter(samplerCreateInfo.minFilter, samplerCreateInfo.mipmapMode),
						 mapVkMagTexFilter(samplerCreateInfo.magFilter),
						 0.0f,
						 !samplerCreateInfo.unnormalizedCoordinates,
						 samplerCreateInfo.compareEnable ? mapVkSamplerCompareOp(samplerCreateInfo.compareOp)
														 : tcu::Sampler::COMPAREMODE_NONE,
						 0,
						 tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
						 true);

	switch (samplerCreateInfo.borderColor)
	{
	case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
		sampler.borderColor = tcu::UVec4(0,0,0,1);
		break;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
		sampler.borderColor = tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		break;
	case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
		sampler.borderColor = tcu::UVec4(1, 1, 1, 1);
		break;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
		sampler.borderColor = tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		break;
	case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
		sampler.borderColor = tcu::UVec4(0,0,0,0);
		break;
	case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
		sampler.borderColor = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		break;

	default:
		DE_ASSERT(false);
		break;
	}

	return sampler;
}

tcu::Sampler::CompareMode mapVkSamplerCompareOp (VkCompareOp compareOp)
{
	switch (compareOp)
	{
		case VK_COMPARE_OP_NEVER:				return tcu::Sampler::COMPAREMODE_NEVER;
		case VK_COMPARE_OP_LESS:				return tcu::Sampler::COMPAREMODE_LESS;
		case VK_COMPARE_OP_EQUAL:				return tcu::Sampler::COMPAREMODE_EQUAL;
		case VK_COMPARE_OP_LESS_OR_EQUAL:		return tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL;
		case VK_COMPARE_OP_GREATER:				return tcu::Sampler::COMPAREMODE_GREATER;
		case VK_COMPARE_OP_NOT_EQUAL:			return tcu::Sampler::COMPAREMODE_NOT_EQUAL;
		case VK_COMPARE_OP_GREATER_OR_EQUAL:	return tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL;
		case VK_COMPARE_OP_ALWAYS:				return tcu::Sampler::COMPAREMODE_ALWAYS;
		default:
			break;
	}

	DE_ASSERT(false);
	return tcu::Sampler::COMPAREMODE_LAST;
}

tcu::Sampler::WrapMode mapVkSamplerAddressMode (VkSamplerAddressMode addressMode)
{
	switch (addressMode)
	{
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:			return tcu::Sampler::CLAMP_TO_EDGE;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:		return tcu::Sampler::CLAMP_TO_BORDER;
		case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:		return tcu::Sampler::MIRRORED_REPEAT_GL;
		case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:	return tcu::Sampler::MIRRORED_ONCE;
		case VK_SAMPLER_ADDRESS_MODE_REPEAT:				return tcu::Sampler::REPEAT_GL;
		default:
			break;
	}

	DE_ASSERT(false);
	return tcu::Sampler::WRAPMODE_LAST;
}

tcu::Sampler::FilterMode mapVkMinTexFilter (VkFilter filter, VkSamplerMipmapMode mipMode)
{
	switch (filter)
	{
		case VK_FILTER_LINEAR:
			switch (mipMode)
			{
				case VK_SAMPLER_MIPMAP_MODE_BASE:		return tcu::Sampler::LINEAR_MIPMAP_NEAREST;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:		return tcu::Sampler::LINEAR_MIPMAP_LINEAR;
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:	return tcu::Sampler::LINEAR_MIPMAP_NEAREST;
				default:
					break;
			}
			break;

		case VK_FILTER_NEAREST:
			switch (mipMode)
			{
				case VK_SAMPLER_MIPMAP_MODE_BASE:		return tcu::Sampler::NEAREST_MIPMAP_NEAREST;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:		return tcu::Sampler::NEAREST_MIPMAP_LINEAR;
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:	return tcu::Sampler::NEAREST_MIPMAP_NEAREST;
				default:
					break;
			}
			break;

		default:
			break;
	}

	DE_ASSERT(false);
	return tcu::Sampler::FILTERMODE_LAST;
}

tcu::Sampler::FilterMode mapVkMagTexFilter (VkFilter filter)
{
	switch (filter)
	{
		case VK_FILTER_LINEAR:		return tcu::Sampler::LINEAR;
		case VK_FILTER_NEAREST:		return tcu::Sampler::NEAREST;
		default:
			break;
	}

	DE_ASSERT(false);
	return tcu::Sampler::FILTERMODE_LAST;
}


int mapVkComponentSwizzle (const vk::VkComponentSwizzle& channelSwizzle)
{
	switch (channelSwizzle)
	{
		case vk::VK_COMPONENT_SWIZZLE_ZERO:	return 0;
		case vk::VK_COMPONENT_SWIZZLE_ONE:	return 1;
		case vk::VK_COMPONENT_SWIZZLE_R:	return 2;
		case vk::VK_COMPONENT_SWIZZLE_G:	return 3;
		case vk::VK_COMPONENT_SWIZZLE_B:	return 4;
		case vk::VK_COMPONENT_SWIZZLE_A:	return 5;
		default:
			break;
	}

	DE_ASSERT(false);
	return 0;
}

tcu::UVec4 mapVkComponentMapping (const vk::VkComponentMapping& mapping)
{
	tcu::UVec4 swizzle;

	swizzle.x() = mapVkComponentSwizzle(mapping.r);
	swizzle.y() = mapVkComponentSwizzle(mapping.g);
	swizzle.z() = mapVkComponentSwizzle(mapping.b);
	swizzle.w() = mapVkComponentSwizzle(mapping.a);

	return swizzle;
}

//! Get a format the matches the layout in buffer memory used for a
//! buffer<->image copy on a depth/stencil format.
tcu::TextureFormat getDepthCopyFormat (VkFormat combinedFormat)
{
	switch (combinedFormat)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
			return mapVkFormat(combinedFormat);

		case VK_FORMAT_D16_UNORM_S8_UINT:
			return mapVkFormat(VK_FORMAT_D16_UNORM);
		case VK_FORMAT_D24_UNORM_S8_UINT:
			return mapVkFormat(VK_FORMAT_X8_D24_UNORM_PACK32);
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return mapVkFormat(VK_FORMAT_D32_SFLOAT);

		case VK_FORMAT_S8_UINT:
		default:
			DE_FATAL("Unexpected depth/stencil format");
			return tcu::TextureFormat();
	}
}

//! Get a format the matches the layout in buffer memory used for a
//! buffer<->image copy on a depth/stencil format.
tcu::TextureFormat getStencilCopyFormat (VkFormat combinedFormat)
{
	switch (combinedFormat)
	{
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_S8_UINT:
			return mapVkFormat(VK_FORMAT_S8_UINT);

		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
		default:
			DE_FATAL("Unexpected depth/stencil format");
			return tcu::TextureFormat();
	}
}

} // vk
