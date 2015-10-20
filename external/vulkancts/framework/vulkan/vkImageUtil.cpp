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
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 174);

	switch (format)
	{
		case VK_FORMAT_BC1_RGB_UNORM:
		case VK_FORMAT_BC1_RGB_SRGB:
		case VK_FORMAT_BC1_RGBA_UNORM:
		case VK_FORMAT_BC1_RGBA_SRGB:
		case VK_FORMAT_BC2_UNORM:
		case VK_FORMAT_BC2_SRGB:
		case VK_FORMAT_BC3_UNORM:
		case VK_FORMAT_BC3_SRGB:
		case VK_FORMAT_BC4_UNORM:
		case VK_FORMAT_BC4_SNORM:
		case VK_FORMAT_BC5_UNORM:
		case VK_FORMAT_BC5_SNORM:
		case VK_FORMAT_BC6H_UFLOAT:
		case VK_FORMAT_BC6H_SFLOAT:
		case VK_FORMAT_BC7_UNORM:
		case VK_FORMAT_BC7_SRGB:
		case VK_FORMAT_ETC2_R8G8B8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:
		case VK_FORMAT_EAC_R11_UNORM:
		case VK_FORMAT_EAC_R11_SNORM:
		case VK_FORMAT_EAC_R11G11_UNORM:
		case VK_FORMAT_EAC_R11G11_SNORM:
		case VK_FORMAT_ASTC_4x4_UNORM:
		case VK_FORMAT_ASTC_4x4_SRGB:
		case VK_FORMAT_ASTC_5x4_UNORM:
		case VK_FORMAT_ASTC_5x4_SRGB:
		case VK_FORMAT_ASTC_5x5_UNORM:
		case VK_FORMAT_ASTC_5x5_SRGB:
		case VK_FORMAT_ASTC_6x5_UNORM:
		case VK_FORMAT_ASTC_6x5_SRGB:
		case VK_FORMAT_ASTC_6x6_UNORM:
		case VK_FORMAT_ASTC_6x6_SRGB:
		case VK_FORMAT_ASTC_8x5_UNORM:
		case VK_FORMAT_ASTC_8x5_SRGB:
		case VK_FORMAT_ASTC_8x6_UNORM:
		case VK_FORMAT_ASTC_8x6_SRGB:
		case VK_FORMAT_ASTC_8x8_UNORM:
		case VK_FORMAT_ASTC_8x8_SRGB:
		case VK_FORMAT_ASTC_10x5_UNORM:
		case VK_FORMAT_ASTC_10x5_SRGB:
		case VK_FORMAT_ASTC_10x6_UNORM:
		case VK_FORMAT_ASTC_10x6_SRGB:
		case VK_FORMAT_ASTC_10x8_UNORM:
		case VK_FORMAT_ASTC_10x8_SRGB:
		case VK_FORMAT_ASTC_10x10_UNORM:
		case VK_FORMAT_ASTC_10x10_SRGB:
		case VK_FORMAT_ASTC_12x10_UNORM:
		case VK_FORMAT_ASTC_12x10_SRGB:
		case VK_FORMAT_ASTC_12x12_UNORM:
		case VK_FORMAT_ASTC_12x12_SRGB:
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
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 174);

	switch (PACK_FMT(format.order, format.type))
	{
		case FMT_CASE(RG, UNORM_BYTE_44):					return VK_FORMAT_R4G4_UNORM;
		case FMT_CASE(RGB, UNORM_SHORT_565):				return VK_FORMAT_R5G6B5_UNORM;
		case FMT_CASE(RGBA, UNORM_SHORT_4444):				return VK_FORMAT_R4G4B4A4_UNORM;
		case FMT_CASE(RGBA, UNORM_SHORT_5551):				return VK_FORMAT_R5G5B5A1_UNORM;

		case FMT_CASE(RG, UNSIGNED_BYTE_44):				return VK_FORMAT_R4G4_USCALED;
		case FMT_CASE(RGBA, UNSIGNED_SHORT_4444):			return VK_FORMAT_R4G4B4A4_USCALED;
		case FMT_CASE(RGB, UNSIGNED_SHORT_565):				return VK_FORMAT_R5G6B5_USCALED;
		case FMT_CASE(RGBA, UNSIGNED_SHORT_5551):			return VK_FORMAT_R5G5B5A1_USCALED;

		case FMT_CASE(BGR, UNORM_SHORT_565):				return VK_FORMAT_B5G6R5_UNORM;
		case FMT_CASE(BGRA, UNORM_SHORT_4444):				return VK_FORMAT_B4G4R4A4_UNORM;
		case FMT_CASE(BGRA, UNORM_SHORT_5551):				return VK_FORMAT_B5G5R5A1_UNORM;

		case FMT_CASE(BGR, UNSIGNED_SHORT_565):				return VK_FORMAT_B5G6R5_USCALED;

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

		case FMT_CASE(RGBA, SNORM_INT_1010102_REV):			return VK_FORMAT_R10G10B10A2_SNORM;
		case FMT_CASE(RGBA, UNORM_INT_1010102_REV):			return VK_FORMAT_R10G10B10A2_UNORM;
		case FMT_CASE(RGBA, UNSIGNED_INT_1010102_REV):		return VK_FORMAT_R10G10B10A2_UINT;
		case FMT_CASE(RGBA, SIGNED_INT_1010102_REV):		return VK_FORMAT_R10G10B10A2_SINT;

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

		case FMT_CASE(RGB, UNSIGNED_INT_11F_11F_10F_REV):	return VK_FORMAT_R11G11B10_UFLOAT;
		case FMT_CASE(RGB, UNSIGNED_INT_999_E5_REV):		return VK_FORMAT_R9G9B9E5_UFLOAT;

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

		case FMT_CASE(BGRA, UNORM_INT_1010102_REV):			return VK_FORMAT_B10G10R10A2_UNORM;
		case FMT_CASE(BGRA, SNORM_INT_1010102_REV):			return VK_FORMAT_B10G10R10A2_SNORM;
		case FMT_CASE(BGRA, UNSIGNED_INT_1010102_REV):		return VK_FORMAT_B10G10R10A2_UINT;
		case FMT_CASE(BGRA, SIGNED_INT_1010102_REV):		return VK_FORMAT_B10G10R10A2_SINT;

		case FMT_CASE(D, UNORM_INT16):						return VK_FORMAT_D16_UNORM;
		case FMT_CASE(D, UNSIGNED_INT_24_8_REV):			return VK_FORMAT_D24_UNORM_X8;
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
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 174);

	switch (format)
	{
		case VK_FORMAT_R4G4_UNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_BYTE_44);
		case VK_FORMAT_R5G6B5_UNORM:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_SHORT_565);
		case VK_FORMAT_R4G4B4A4_UNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_4444);
		case VK_FORMAT_R5G5B5A1_UNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_5551);

		case VK_FORMAT_R4G4_USCALED:		return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_BYTE_44);
		case VK_FORMAT_R4G4B4A4_USCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_SHORT_4444);
		case VK_FORMAT_R5G6B5_USCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_SHORT_565);
		case VK_FORMAT_R5G5B5A1_USCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_SHORT_5551);

		case VK_FORMAT_B5G6R5_UNORM:		return TextureFormat(TextureFormat::BGR,	TextureFormat::UNORM_SHORT_565);
		case VK_FORMAT_B4G4R4A4_UNORM:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_SHORT_4444);
		case VK_FORMAT_B5G5R5A1_UNORM:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_SHORT_5551);

		case VK_FORMAT_B5G6R5_USCALED:		return TextureFormat(TextureFormat::BGR,	TextureFormat::UNSIGNED_SHORT_565);

		case VK_FORMAT_R8_UNORM:			return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8_SNORM:			return TextureFormat(TextureFormat::R,		TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8_USCALED:			return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8_SSCALED:			return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8_UINT:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8_SINT:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8_SRGB:				return TextureFormat(TextureFormat::sR,		TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8_UNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8_SNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8_USCALED:		return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8_SSCALED:		return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8_UINT:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8_SINT:			return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8_SRGB:			return TextureFormat(TextureFormat::sRG,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8B8_UNORM:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8B8_SNORM:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8B8_USCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8_SSCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8_UINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8_SINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8_SRGB:			return TextureFormat(TextureFormat::sRGB,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8B8A8_UNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8B8A8_SNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8B8A8_USCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SSCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_UINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SRGB:		return TextureFormat(TextureFormat::sRGBA,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R10G10B10A2_UNORM:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT_1010102_REV);
		case VK_FORMAT_R10G10B10A2_SNORM:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT_1010102_REV);
		case VK_FORMAT_R10G10B10A2_UINT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_R10G10B10A2_SINT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT_1010102_REV);
		case VK_FORMAT_R10G10B10A2_USCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_R10G10B10A2_SSCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT_1010102_REV);

		case VK_FORMAT_R16_UNORM:			return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16_SNORM:			return TextureFormat(TextureFormat::R,		TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16_USCALED:			return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16_SSCALED:			return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16_UINT:			return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16_SINT:			return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16_SFLOAT:			return TextureFormat(TextureFormat::R,		TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16_UNORM:		return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16_SNORM:		return TextureFormat(TextureFormat::RG,		TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16_USCALED:		return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16_SSCALED:		return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16_UINT:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16_SINT:			return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16_SFLOAT:		return TextureFormat(TextureFormat::RG,		TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16B16_UNORM:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16B16_SNORM:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16B16_USCALED:	return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16_SSCALED:	return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16_UINT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16_SINT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16_SFLOAT:	return TextureFormat(TextureFormat::RGB,	TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16B16A16_UNORM:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16B16A16_SNORM:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16B16A16_USCALED:return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_SSCALED:return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_UINT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_SINT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16A16_SFLOAT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R32_UINT:			return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32_SINT:			return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32_SFLOAT:			return TextureFormat(TextureFormat::R,		TextureFormat::FLOAT);

		case VK_FORMAT_R32G32_UINT:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32G32_SINT:			return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32G32_SFLOAT:		return TextureFormat(TextureFormat::RG,		TextureFormat::FLOAT);

		case VK_FORMAT_R32G32B32_UINT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32G32B32_SINT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32G32B32_SFLOAT:	return TextureFormat(TextureFormat::RGB,	TextureFormat::FLOAT);

		case VK_FORMAT_R32G32B32A32_UINT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT32);
		case VK_FORMAT_R32G32B32A32_SINT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT32);
		case VK_FORMAT_R32G32B32A32_SFLOAT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::FLOAT);

		case VK_FORMAT_R64_SFLOAT:			return TextureFormat(TextureFormat::R,		TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64_SFLOAT:		return TextureFormat(TextureFormat::RG,		TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64B64_SFLOAT:	return TextureFormat(TextureFormat::RGB,	TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64B64A64_SFLOAT:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::FLOAT64);

		case VK_FORMAT_R11G11B10_UFLOAT:	return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT_11F_11F_10F_REV);
		case VK_FORMAT_R9G9B9E5_UFLOAT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT_999_E5_REV);

		case VK_FORMAT_B8G8R8_UNORM:		return TextureFormat(TextureFormat::BGR,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_B8G8R8_SNORM:		return TextureFormat(TextureFormat::BGR,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_B8G8R8_USCALED:		return TextureFormat(TextureFormat::BGR,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8_SSCALED:		return TextureFormat(TextureFormat::BGR,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8_UINT:			return TextureFormat(TextureFormat::BGR,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8_SINT:			return TextureFormat(TextureFormat::BGR,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8_SRGB:			return TextureFormat(TextureFormat::sBGR,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_B8G8R8A8_UNORM:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_B8G8R8A8_SNORM:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_B8G8R8A8_USCALED:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_SSCALED:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_UINT:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_SINT:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8A8_SRGB:		return TextureFormat(TextureFormat::sBGRA,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_D16_UNORM:			return TextureFormat(TextureFormat::D,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_D24_UNORM_X8:		return TextureFormat(TextureFormat::D,		TextureFormat::UNSIGNED_INT_24_8_REV);
		case VK_FORMAT_D32_SFLOAT:			return TextureFormat(TextureFormat::D,		TextureFormat::FLOAT);

		case VK_FORMAT_S8_UINT:				return TextureFormat(TextureFormat::S,		TextureFormat::UNSIGNED_INT8);

		// \note There is no standard interleaved memory layout for DS formats; buffer-image copies
		//		 will always operate on either D or S aspect only. See Khronos bug 12998
		case VK_FORMAT_D16_UNORM_S8_UINT:	return TextureFormat(TextureFormat::DS,		TextureFormat::UNSIGNED_INT_16_8_8);
		case VK_FORMAT_D24_UNORM_S8_UINT:	return TextureFormat(TextureFormat::DS,		TextureFormat::UNSIGNED_INT_24_8_REV);
		case VK_FORMAT_D32_SFLOAT_S8_UINT:	return TextureFormat(TextureFormat::DS,		TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV);

		case VK_FORMAT_B10G10R10A2_UNORM:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_INT_1010102_REV);
		case VK_FORMAT_B10G10R10A2_SNORM:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SNORM_INT_1010102_REV);
		case VK_FORMAT_B10G10R10A2_USCALED:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_B10G10R10A2_SSCALED:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT_1010102_REV);
		case VK_FORMAT_B10G10R10A2_UINT:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_B10G10R10A2_SINT:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT_1010102_REV);

		default:
			TCU_THROW(InternalError, "Unknown image format");
	}
}

tcu::CompressedTexFormat mapVkCompressedFormat (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_ETC2_R8G8B8_UNORM:		return tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8;
		case VK_FORMAT_ETC2_R8G8B8_SRGB:		return tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8;
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM:		return tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1;
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB:		return tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1;
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM:		return tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8;
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB:		return tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8;
		case VK_FORMAT_EAC_R11_UNORM:			return tcu::COMPRESSEDTEXFORMAT_EAC_R11;
		case VK_FORMAT_EAC_R11_SNORM:			return tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11;
		case VK_FORMAT_EAC_R11G11_UNORM:		return tcu::COMPRESSEDTEXFORMAT_EAC_RG11;
		case VK_FORMAT_EAC_R11G11_SNORM:		return tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11;
		case VK_FORMAT_ASTC_4x4_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA;
		case VK_FORMAT_ASTC_4x4_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_5x4_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA;
		case VK_FORMAT_ASTC_5x4_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_5x5_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA;
		case VK_FORMAT_ASTC_5x5_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_6x5_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA;
		case VK_FORMAT_ASTC_6x5_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_6x6_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA;
		case VK_FORMAT_ASTC_6x6_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x5_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA;
		case VK_FORMAT_ASTC_8x5_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x6_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA;
		case VK_FORMAT_ASTC_8x6_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x8_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA;
		case VK_FORMAT_ASTC_8x8_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x5_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA;
		case VK_FORMAT_ASTC_10x5_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x6_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA;
		case VK_FORMAT_ASTC_10x6_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x8_UNORM:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA;
		case VK_FORMAT_ASTC_10x8_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_10x10_UNORM:		return tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA;
		case VK_FORMAT_ASTC_10x10_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_12x10_UNORM:		return tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA;
		case VK_FORMAT_ASTC_12x10_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_12x12_UNORM:		return tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA;
		case VK_FORMAT_ASTC_12x12_SRGB:			return tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8;
		default:
			break;
	}

	return tcu::COMPRESSEDTEXFORMAT_LAST;
}

VkChannelMapping getFormatChannelMapping (VkFormat format)
{
	using tcu::TextureFormat;

	static const VkChannelMapping	R		= {	VK_CHANNEL_SWIZZLE_R,		VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_ONE	};
	static const VkChannelMapping	RG		= {	VK_CHANNEL_SWIZZLE_R,		VK_CHANNEL_SWIZZLE_G,		VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_ONE	};
	static const VkChannelMapping	RGB		= {	VK_CHANNEL_SWIZZLE_R,		VK_CHANNEL_SWIZZLE_G,		VK_CHANNEL_SWIZZLE_B,		VK_CHANNEL_SWIZZLE_ONE	};
	static const VkChannelMapping	RGBA	= {	VK_CHANNEL_SWIZZLE_R,		VK_CHANNEL_SWIZZLE_G,		VK_CHANNEL_SWIZZLE_B,		VK_CHANNEL_SWIZZLE_A	};
	static const VkChannelMapping	S		= { VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_A	};
	static const VkChannelMapping	DS		= {	VK_CHANNEL_SWIZZLE_R,		VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_ZERO,	VK_CHANNEL_SWIZZLE_A	};
	static const VkChannelMapping	BGRA	= {	VK_CHANNEL_SWIZZLE_B,		VK_CHANNEL_SWIZZLE_G,		VK_CHANNEL_SWIZZLE_R,		VK_CHANNEL_SWIZZLE_A	};

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
		case TextureFormat::sR:		return R;
		case TextureFormat::sRG:	return RG;
		case TextureFormat::sRGB:	return RGB;
		case TextureFormat::sRGBA:	return RGBA;
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
	DE_STATIC_ASSERT(VK_FORMAT_LAST == 174);

	switch (format)
	{
		case VK_FORMAT_R4G4_USCALED:
		case VK_FORMAT_R4G4B4A4_USCALED:
		case VK_FORMAT_R5G6B5_USCALED:
		case VK_FORMAT_R5G5B5A1_USCALED:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R10G10B10A2_USCALED:
		case VK_FORMAT_R10G10B10A2_SSCALED:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_B5G6R5_USCALED:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B10G10R10A2_USCALED:
		case VK_FORMAT_B10G10R10A2_SSCALED:
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
		return (format != VK_FORMAT_UNDEFINED);
}

void imageUtilSelfTest (void)
{
	for (int formatNdx = 0; formatNdx < VK_FORMAT_LAST; formatNdx++)
	{
		const VkFormat	format	= (VkFormat)formatNdx;

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

VkTexFilter mapFilterMode (tcu::Sampler::FilterMode filterMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:					return VK_TEX_FILTER_LINEAR;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return VK_TEX_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return VK_TEX_FILTER_LINEAR;
		default:
			DE_ASSERT(false);
	}

	return VK_TEX_FILTER_NEAREST;
}

VkTexMipmapMode mapMipmapMode (tcu::Sampler::FilterMode filterMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(filterMode)
	{
		case tcu::Sampler::NEAREST:					return VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::LINEAR:					return VK_TEX_MIPMAP_MODE_BASE;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return VK_TEX_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return VK_TEX_MIPMAP_MODE_LINEAR;
		default:
			DE_ASSERT(false);
	}

	return VK_TEX_MIPMAP_MODE_BASE;
}

VkTexAddressMode mapWrapMode (tcu::Sampler::WrapMode wrapMode)
{
	// \todo [2015-09-07 elecro] dobule check the mappings
	switch(wrapMode)
	{
		case tcu::Sampler::CLAMP_TO_EDGE:		return VK_TEX_ADDRESS_MODE_CLAMP;
		case tcu::Sampler::CLAMP_TO_BORDER:		return VK_TEX_ADDRESS_MODE_CLAMP_BORDER;
		case tcu::Sampler::REPEAT_GL:			return VK_TEX_ADDRESS_MODE_WRAP;
		case tcu::Sampler::REPEAT_CL:			return VK_TEX_ADDRESS_MODE_WRAP;
		case tcu::Sampler::MIRRORED_REPEAT_GL:	return VK_TEX_ADDRESS_MODE_MIRROR;
		case tcu::Sampler::MIRRORED_REPEAT_CL:	return VK_TEX_ADDRESS_MODE_MIRROR;
		default:
			DE_ASSERT(false);
	}

	return VK_TEX_ADDRESS_MODE_WRAP;
}

vk::VkCompareOp mapCompareMode (tcu::Sampler::CompareMode mode)
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

} // vk
