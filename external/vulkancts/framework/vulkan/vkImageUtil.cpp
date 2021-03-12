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

#include "vkImageUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "deMath.h"

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

	if (isYCbCrFormat(format))
		return false;

	const tcu::TextureFormat tcuFormat = mapVkFormat(format);
	return tcuFormat.order == tcu::TextureFormat::D || tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS;
}

bool isSrgbFormat (VkFormat format)
{
	switch (mapVkFormat(format).order)
	{
		case tcu::TextureFormat::sR:
		case tcu::TextureFormat::sRG:
		case tcu::TextureFormat::sRGB:
		case tcu::TextureFormat::sRGBA:
		case tcu::TextureFormat::sBGR:
		case tcu::TextureFormat::sBGRA:
			return true;

		default:
			return false;
	}
}

bool isUfloatFormat (VkFormat format)
{
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

	switch (format)
	{
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
			return true;

		default:
			return false;
	}
}

bool isSfloatFormat (VkFormat format)
{
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

	switch (format)
	{
		case VK_FORMAT_R16_SFLOAT:
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:
			return true;

		default:
			return false;
	}
}

bool isCompressedFormat (VkFormat format)
{
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

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

bool isYCbCrFormat (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_G8B8G8R8_422_UNORM:
		case VK_FORMAT_B8G8R8G8_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
		case VK_FORMAT_R10X6_UNORM_PACK16:
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case VK_FORMAT_R12X4_UNORM_PACK16:
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		case VK_FORMAT_G16B16G16R16_422_UNORM:
		case VK_FORMAT_B16G16R16G16_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
		case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
			return true;

		default:
			return false;
	}
}

bool isYCbCrExtensionFormat (VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
	case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
	case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
	case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
		return true;

	default:
		return false;
	}
}

bool isYCbCr420Format (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
			return true;

		default:
			return false;
	}
}

bool isYCbCr422Format (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_G8B8G8R8_422_UNORM:
		case VK_FORMAT_B8G8R8G8_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G16B16G16R16_422_UNORM:
		case VK_FORMAT_B16G16R16G16_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
			return true;

		default:
			return false;
	}
}

const PlanarFormatDescription& getYCbCrPlanarFormatDescription (VkFormat format)
{
	using tcu::TextureFormat;

	const deUint32	chanR			= PlanarFormatDescription::CHANNEL_R;
	const deUint32	chanG			= PlanarFormatDescription::CHANNEL_G;
	const deUint32	chanB			= PlanarFormatDescription::CHANNEL_B;
	const deUint32	chanA			= PlanarFormatDescription::CHANNEL_A;

	const deUint8	unorm			= (deUint8)tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;

	if (format >= VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT && format <= VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT)
	{
		static const PlanarFormatDescription s_formatInfo[] =
		{
			// VK_FORMAT_G8_B8R8_2PLANE_444_UNORM
			{
				2, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_R8_UNORM },
					{	2,		1,		1,		VK_FORMAT_R8G8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	1,		unorm,	8,		8,		2 },	// R
					{	0,		unorm,	0,		8,		1 },	// G
					{	1,		unorm,	0,		8,		2 },	// B
					{ 0, 0, 0, 0, 0 }
				}
			},
			// VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT
			{
				2, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
					{	4,		1,		1,		VK_FORMAT_R10X6G10X6_UNORM_2PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	1,		unorm,	22,		10,		4 },	// R
					{	0,		unorm,	6,		10,		2 },	// G
					{	1,		unorm,	6,		10,		4 },	// B
					{ 0, 0, 0, 0, 0 }
				}
			},
			// VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT
			{
				2, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
					{	4,		1,		1,		VK_FORMAT_R12X4G12X4_UNORM_2PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	1,		unorm,	20,		12,		4 },	// R
					{	0,		unorm,	4,		12,		2 },	// G
					{	1,		unorm,	4,		12,		4 },	// B
					{ 0, 0, 0, 0, 0 }
				}
			},
			// VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT
			{
				2, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R16_UNORM },
					{	4,		1,		1,		VK_FORMAT_R16G16_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	1,		unorm,	16,		16,		4 },	// R
					{	0,		unorm,	0,		16,		2 },	// G
					{	1,		unorm,	0,		16,		4 },	// B
					{ 0, 0, 0, 0, 0 }
				}
			},
		};

		const size_t	offset	= (size_t)VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT;

		DE_ASSERT(de::inBounds<size_t>((size_t)format, offset, offset+(size_t)DE_LENGTH_OF_ARRAY(s_formatInfo)));

		return s_formatInfo[(size_t)format-offset];
	}

	static const PlanarFormatDescription s_formatInfo[] =
	{
		// VK_FORMAT_G8B8G8R8_422_UNORM
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	4,		1,		1,		VK_FORMAT_G8B8G8R8_422_UNORM	},
				{	0,		0,		0,		VK_FORMAT_UNDEFINED	},
				{	0,		0,		0,		VK_FORMAT_UNDEFINED	},
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	24,		8,		4 },	// R
				{	0,		unorm,	0,		8,		2 },	// G
				{	0,		unorm,	8,		8,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_B8G8R8G8_422_UNORM
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	4,		1,		1,		VK_FORMAT_B8G8R8G8_422_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	16,		8,		4 },	// R
				{	0,		unorm,	8,		8,		2 },	// G
				{	0,		unorm,	0,		8,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
				{	1,		2,		2,		VK_FORMAT_R8_UNORM },
				{	1,		2,		2,		VK_FORMAT_R8_UNORM },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	0,		8,		1 },	// R
				{	0,		unorm,	0,		8,		1 },	// G
				{	1,		unorm,	0,		8,		1 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G8_B8R8_2PLANE_420_UNORM
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
				{	2,		2,		2,		VK_FORMAT_R8G8_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	8,		8,		2 },	// R
				{	0,		unorm,	0,		8,		1 },	// G
				{	1,		unorm,	0,		8,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
				{	1,		2,		1,		VK_FORMAT_R8_UNORM },
				{	1,		2,		1,		VK_FORMAT_R8_UNORM },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	0,		8,		1 },	// R
				{	0,		unorm,	0,		8,		1 },	// G
				{	1,		unorm,	0,		8,		1 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G8_B8R8_2PLANE_422_UNORM
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
				{	2,		2,		1,		VK_FORMAT_R8G8_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	8,		8,		2 },	// R
				{	0,		unorm,	0,		8,		1 },	// G
				{	1,		unorm,	0,		8,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
				{	1,		1,		1,		VK_FORMAT_R8_UNORM },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	0,		8,		1 },	// R
				{	0,		unorm,	0,		8,		1 },	// G
				{	1,		unorm,	0,		8,		1 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_R10X6_UNORM_PACK16
		{
			1, // planes
			chanR,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	6,		10,		2 },	// R
				{ 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0 },
			}
		},
		// VK_FORMAT_R10X6G10X6_UNORM_2PACK16
		{
			1, // planes
			chanR|chanG,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	4,		1,		1,		VK_FORMAT_R10X6G10X6_UNORM_2PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	6,		10,		4 },	// R
				{	0,		unorm,	22,		10,		4 },	// G
				{ 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0 },
			}
		},
		// VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16
		{
			1, // planes
			chanR|chanG|chanB|chanA,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	6,		10,		8 },	// R
				{	0,		unorm,	22,		10,		8 },	// G
				{	0,		unorm,	38,		10,		8 },	// B
				{	0,		unorm,	54,		10,		8 },	// A
			}
		},
		// VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	54,		10,		8 },	// R
				{	0,		unorm,	6,		10,		4 },	// G
				{	0,		unorm,	22,		10,		8 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	38,		10,		8 },	// R
				{	0,		unorm,	22,		10,		4 },	// G
				{	0,		unorm,	6,		10,		8 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	2,		2,		2,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	2,		2,		2,		VK_FORMAT_R10X6_UNORM_PACK16 },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	6,		10,		2 },	// R
				{	0,		unorm,	6,		10,		2 },	// G
				{	1,		unorm,	6,		10,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	4,		2,		2,		VK_FORMAT_R10X6G10X6_UNORM_2PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	22,		10,		4 },	// R
				{	0,		unorm,	6,		10,		2 },	// G
				{	1,		unorm,	6,		10,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	2,		2,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	2,		2,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	6,		10,		2 },	// R
				{	0,		unorm,	6,		10,		2 },	// G
				{	1,		unorm,	6,		10,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	4,		2,		1,		VK_FORMAT_R10X6G10X6_UNORM_2PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	22,		10,		4 },	// R
				{	0,		unorm,	6,		10,		2 },	// G
				{	1,		unorm,	6,		10,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
				{	2,		1,		1,		VK_FORMAT_R10X6_UNORM_PACK16 },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	6,		10,		2 },	// R
				{	0,		unorm,	6,		10,		2 },	// G
				{	1,		unorm,	6,		10,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_R12X4_UNORM_PACK16
		{
			1, // planes
			chanR,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	4,		12,		2 },	// R
				{ 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0 },
			}
		},
		// VK_FORMAT_R12X4G12X4_UNORM_2PACK16
		{
			1, // planes
			chanR|chanG,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	4,		1,		1,		VK_FORMAT_R12X4G12X4_UNORM_2PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	4,		12,		4 },	// R
				{	0,		unorm,	20,		12,		4 },	// G
				{ 0, 0, 0, 0, 0 },
				{ 0, 0, 0, 0, 0 },
			}
		},
		// VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16
		{
			1, // planes
			chanR|chanG|chanB|chanA,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	4,		12,		8 },	// R
				{	0,		unorm,	20,		12,		8 },	// G
				{	0,		unorm,	36,		12,		8 },	// B
				{	0,		unorm,	52,		12,		8 },	// A
			}
		},
		// VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	52,		12,		8 },	// R
				{	0,		unorm,	4,		12,		4 },	// G
				{	0,		unorm,	20,		12,		8 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	36,		12,		8 },	// R
				{	0,		unorm,	20,		12,		4 },	// G
				{	0,		unorm,	4,		12,		8 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	2,		2,		2,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	2,		2,		2,		VK_FORMAT_R12X4_UNORM_PACK16 },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	4,		12,		2 },	// R
				{	0,		unorm,	4,		12,		2 },	// G
				{	1,		unorm,	4,		12,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	4,		2,		2,		VK_FORMAT_R12X4G12X4_UNORM_2PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	20,		12,		4 },	// R
				{	0,		unorm,	4,		12,		2 },	// G
				{	1,		unorm,	4,		12,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	2,		2,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	2,		2,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	4,		12,		2 },	// R
				{	0,		unorm,	4,		12,		2 },	// G
				{	1,		unorm,	4,		12,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	4,		2,		1,		VK_FORMAT_R12X4G12X4_UNORM_2PACK16 },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	20,		12,		4 },	// R
				{	0,		unorm,	4,		12,		2 },	// G
				{	1,		unorm,	4,		12,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
				{	2,		1,		1,		VK_FORMAT_R12X4_UNORM_PACK16 },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	4,		12,		2 },	// R
				{	0,		unorm,	4,		12,		2 },	// G
				{	1,		unorm,	4,		12,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G16B16G16R16_422_UNORM
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_G16B16G16R16_422_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	48,		16,		8 },	// R
				{	0,		unorm,	0,		16,		4 },	// G
				{	0,		unorm,	16,		16,		8 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_B16G16R16G16_422_UNORM
		{
			1, // planes
			chanR|chanG|chanB,
			2,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	8,		1,		1,		VK_FORMAT_B16G16R16G16_422_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	0,		unorm,	32,		16,		8 },	// R
				{	0,		unorm,	16,		16,		4 },	// G
				{	0,		unorm,	0,		16,		8 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
				{	2,		2,		2,		VK_FORMAT_R16_UNORM },
				{	2,		2,		2,		VK_FORMAT_R16_UNORM },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	0,		16,		2 },	// R
				{	0,		unorm,	0,		16,		2 },	// G
				{	1,		unorm,	0,		16,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G16_B16R16_2PLANE_420_UNORM
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
				{	4,		2,		2,		VK_FORMAT_R16G16_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	16,		16,		4 },	// R
				{	0,		unorm,	0,		16,		2 },	// G
				{	1,		unorm,	0,		16,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
				{	2,		2,		1,		VK_FORMAT_R16_UNORM },
				{	2,		2,		1,		VK_FORMAT_R16_UNORM },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	0,		16,		2 },	// R
				{	0,		unorm,	0,		16,		2 },	// G
				{	1,		unorm,	0,		16,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G16_B16R16_2PLANE_422_UNORM
		{
			2, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
				{	4,		2,		1,		VK_FORMAT_R16G16_UNORM },
				{	0,		0,		0,		VK_FORMAT_UNDEFINED },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	1,		unorm,	16,		16,		4 },	// R
				{	0,		unorm,	0,		16,		2 },	// G
				{	1,		unorm,	0,		16,		4 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
		// VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM
		{
			3, // planes
			chanR|chanG|chanB,
			1,1,
			{
			//		Size	WDiv	HDiv	planeCompatibleFormat
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
				{	2,		1,		1,		VK_FORMAT_R16_UNORM },
			},
			{
			//		Plane	Type	Offs	Size	Stride
				{	2,		unorm,	0,		16,		2 },	// R
				{	0,		unorm,	0,		16,		2 },	// G
				{	1,		unorm,	0,		16,		2 },	// B
				{ 0, 0, 0, 0, 0 }
			}
		},
	};

	const size_t	offset	= (size_t)VK_FORMAT_G8B8G8R8_422_UNORM;

	DE_ASSERT(de::inBounds<size_t>((size_t)format, offset, offset+(size_t)DE_LENGTH_OF_ARRAY(s_formatInfo)));

	return s_formatInfo[(size_t)format-offset];
}

PlanarFormatDescription getCorePlanarFormatDescription (VkFormat format)
{
	const deUint8			snorm	= (deUint8)tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
	const deUint8			unorm	= (deUint8)tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
	const deUint8			sint	= (deUint8)tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER;
	const deUint8			uint	= (deUint8)tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
	const deUint8			sfloat	= (deUint8)tcu::TEXTURECHANNELCLASS_FLOATING_POINT;

	const deUint8			chanR	= (deUint8)PlanarFormatDescription::CHANNEL_R;
	const deUint8			chanG	= (deUint8)PlanarFormatDescription::CHANNEL_G;
	const deUint8			chanB	= (deUint8)PlanarFormatDescription::CHANNEL_B;
	const deUint8			chanA	= (deUint8)PlanarFormatDescription::CHANNEL_A;

	DE_ASSERT(de::inBounds<deUint32>(format, VK_FORMAT_UNDEFINED+1, VK_CORE_FORMAT_LAST));

#if (DE_ENDIANNESS != DE_LITTLE_ENDIAN)
#	error "Big-endian is not supported"
#endif

	switch (format)
	{
		case VK_FORMAT_R8_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_R8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		8,		1 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8_SNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_R8_SNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		snorm,	0,		8,		1 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}


		case VK_FORMAT_R8G8_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R8G8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		8,		2 },	// R
					{	0,		unorm,	8,		8,		2 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8G8_SNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R8G8_SNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		snorm,	0,		8,		2 },	// R
					{	0,		snorm,	8,		8,		2 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R16_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		16,		2 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16_SNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R16_SNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		snorm,	0,		16,		2 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R16G16_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		16,		4 },	// R
					{	0,		unorm,	16,		16,		4 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16_SNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R16G16_SNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		snorm,	0,		16,		4 },	// R
					{	0,		snorm,	16,		16,		4 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_B10G11R11_UFLOAT_PACK32 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		11,		4 },	// R
					{	0,		unorm,	11,		11,		4 },	// G
					{	0,		unorm,	22,		10,		4 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R4G4_UNORM_PACK8:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_R4G4_UNORM_PACK8 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	4,		4,		1 },	// R
					{	0,		unorm,	0,		4,		1 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R4G4B4A4_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	12,		4,		2 },	// R
					{	0,		unorm,	8,		4,		2 },	// G
					{	0,		unorm,	4,		4,		2 },	// B
					{	0,		unorm,	0,		4,		2 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_B4G4R4A4_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	4,		4,		2 },	// R
					{	0,		unorm,	8,		4,		2 },	// G
					{	0,		unorm,	12,		4,		2 },	// B
					{	0,		unorm,	0,		4,		2 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R5G6B5_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R5G6B5_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	11,		5,		2 },	// R
					{	0,		unorm,	5,		6,		2 },	// G
					{	0,		unorm,	0,		5,		2 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_B5G6R5_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_B5G6R5_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		5,		2 },	// R
					{	0,		unorm,	5,		6,		2 },	// G
					{	0,		unorm,	11,		5,		2 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R5G5B5A1_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	11,		5,		2 },	// R
					{	0,		unorm,	6,		5,		2 },	// G
					{	0,		unorm,	1,		5,		2 },	// B
					{	0,		unorm,	0,		1,		2 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_B5G5R5A1_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	1,		5,		2 },	// R
					{	0,		unorm,	6,		5,		2 },	// G
					{	0,		unorm,	11,		5,		2 },	// B
					{	0,		unorm,	0,		1,		2 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_A1R5G5B5_UNORM_PACK16 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	10,		5,		2 },	// R
					{	0,		unorm,	5,		5,		2 },	// G
					{	0,		unorm,	0,		5,		2 },	// B
					{	0,		unorm,	15,		1,		2 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8G8B8_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	3,		1,		1,		VK_FORMAT_R8G8B8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		8,		3 },	// R
					{	0,		unorm,	8,		8,		3 },	// G
					{	0,		unorm,	16,		8,		3 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_B8G8R8_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	3,		1,		1,		VK_FORMAT_B8G8R8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	16,		8,		3 },	// R
					{	0,		unorm,	8,		8,		3 },	// G
					{	0,		unorm,	0,		8,		3 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R8G8B8A8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		8,		4 },	// R
					{	0,		unorm,	8,		8,		4 },	// G
					{	0,		unorm,	16,		8,		4 },	// B
					{	0,		unorm,	24,		8,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_B8G8R8A8_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_B8G8R8A8_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	16,		8,		4 },	// R
					{	0,		unorm,	8,		8,		4 },	// G
					{	0,		unorm,	0,		8,		4 },	// B
					{	0,		unorm,	24,		8,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_A2R10G10B10_UNORM_PACK32 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	20,		10,		4 },	// R
					{	0,		unorm,	10,		10,		4 },	// G
					{	0,		unorm,	0,		10,		4 },	// B
					{	0,		unorm,	30,		2,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_A2B10G10R10_UNORM_PACK32 },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		10,		4 },	// R
					{	0,		unorm,	10,		10,		4 },	// G
					{	0,		unorm,	20,		10,		4 },	// B
					{	0,		unorm,	30,		2,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16B16_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	6,		1,		1,		VK_FORMAT_R16G16B16_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		16,		6 },	// R
					{	0,		unorm,	16,		16,		6 },	// G
					{	0,		unorm,	32,		16,		6 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16B16A16_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R16G16B16A16_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		16,		8 },	// R
					{	0,		unorm,	16,		16,		8 },	// G
					{	0,		unorm,	32,		16,		8 },	// B
					{	0,		unorm,	48,		16,		8 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_R8_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		8,		1 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R16_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		16,		2 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R32_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		32,		4 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}


		case VK_FORMAT_R64_SINT:
		{
			const PlanarFormatDescription	desc =
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R64_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
					//		Plane	Type	Offs	Size	Stride
						{	0,		sint,	0,		64,		8 },	// R
						{	0,		0,		0,		0,		0 },	// G
						{	0,		0,		0,		0,		0 },	// B
						{	0,		0,		0,		0,		0 }		// A
					}
			};
			return desc;
		}

		case VK_FORMAT_R8G8_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R8G8_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		8,		2 },	// R
					{	0,		sint,	8,		8,		2 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R16G16_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		16,		4 },	// R
					{	0,		sint,	16,		16,		4 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32G32_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R32G32_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		32,		8 },	// R
					{	0,		sint,	32,		32,		8 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8G8B8A8_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R8G8B8A8_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		8,		4 },	// R
					{	0,		sint,	8,		8,		4 },	// G
					{	0,		sint,	16,		8,		4 },	// B
					{	0,		sint,	24,		8,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16B16A16_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R16G16B16A16_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		16,		8 },	// R
					{	0,		sint,	16,		16,		8 },	// G
					{	0,		sint,	32,		16,		8 },	// B
					{	0,		sint,	48,		16,		8 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32G32B32A32_SINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	16,		1,		1,		VK_FORMAT_R32G32B32A32_SINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sint,	0,		32,		16 },	// R
					{	0,		sint,	32,		32,		16 },	// G
					{	0,		sint,	64,		32,		16 },	// B
					{	0,		sint,	96,		32,		16 }	// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_R8_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		8,		1 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R16_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		16,		2 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R32_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		32,		4 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R64_UINT:
		{
			const PlanarFormatDescription	desc =
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R64_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
					//		Plane	Type	Offs	Size	Stride
						{	0,		uint,	0,		64,		8 },	// R
						{	0,		0,		0,		0,		0 },	// G
						{	0,		0,		0,		0,		0 },	// B
						{	0,		0,		0,		0,		0 }		// A
					}
			};
			return desc;
		}

		case VK_FORMAT_R8G8_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_R8G8_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		8,		2 },	// R
					{	0,		uint,	8,		8,		2 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R16G16_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		16,		4 },	// R
					{	0,		uint,	16,		16,		4 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32G32_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R32G32_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		32,		8 },	// R
					{	0,		uint,	32,		32,		8 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8G8B8A8_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R8G8B8A8_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		8,		4 },	// R
					{	0,		uint,	8,		8,		4 },	// G
					{	0,		uint,	16,		8,		4 },	// B
					{	0,		uint,	24,		8,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16B16A16_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R16G16B16A16_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		16,		8 },	// R
					{	0,		uint,	16,		16,		8 },	// G
					{	0,		uint,	32,		16,		8 },	// B
					{	0,		uint,	48,		16,		8 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32G32B32A32_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	16,		1,		1,		VK_FORMAT_R32G32B32A32_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		32,		16 },	// R
					{	0,		uint,	32,		32,		16 },	// G
					{	0,		uint,	64,		32,		16 },	// B
					{	0,		uint,	96,		32,		16 }	// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R8G8B8A8_SNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R8G8B8A8_SNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		snorm,	0,		8,		4 },	// R
					{	0,		snorm,	8,		8,		4 },	// G
					{	0,		snorm,	16,		8,		4 },	// B
					{	0,		snorm,	24,		8,		4 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R16G16B16A16_SNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR | chanG | chanB | chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	8,		1,		1,		VK_FORMAT_R16G16B16A16_SNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		snorm,	0,		16,		8 },	// R
					{	0,		snorm,	16,		16,		8 },	// G
					{	0,		snorm,	32,		16,		8 },	// B
					{	0,		snorm,	48,		16,		8 }		// A
				}
			};
			return desc;
		}
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	4,		1,		1,		VK_FORMAT_R32_SFLOAT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sfloat,	0,		32,		4 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_D16_UNORM:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_D16_UNORM },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		16,		2 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_S8_UINT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	1,		1,		1,		VK_FORMAT_S8_UINT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED},
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		uint,	0,		8,		1 },	// R
					{	0,		0,		0,		0,		0 },	// G
					{	0,		0,		0,		0,		0 },	// B
					{	0,		0,		0,		0,		0 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_R32G32B32A32_SFLOAT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	16,		1,		1,		VK_FORMAT_R32G32B32A32_SFLOAT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		sfloat,	0,		32,		16 },	// R
					{	0,		sfloat,	32,		32,		16 },	// G
					{	0,		sfloat,	64,		32,		16 },	// B
					{	0,		sfloat,	96,		32,		16 },	// A
				}
			};
			return desc;
		}

		case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	8,		4,		2 },	// R
					{	0,		unorm,	4,		4,		2 },	// G
					{	0,		unorm,	0,		4,		2 },	// B
					{	0,		unorm,	12,		4,		2 }		// A
				}
			};
			return desc;
		}

		case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT:
		{
			const PlanarFormatDescription	desc	=
			{
				1, // planes
				chanR|chanG|chanB|chanA,
				1,1,
				{
				//		Size	WDiv	HDiv	planeCompatibleFormat
					{	2,		1,		1,		VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
					{	0,		0,		0,		VK_FORMAT_UNDEFINED },
				},
				{
				//		Plane	Type	Offs	Size	Stride
					{	0,		unorm,	0,		4,		2 },	// R
					{	0,		unorm,	4,		4,		2 },	// G
					{	0,		unorm,	8,		4,		2 },	// B
					{	0,		unorm,	12,		4,		2 }		// A
				}
			};
			return desc;
		}


		default:
			TCU_THROW(InternalError, "Not implemented");
	}
}

PlanarFormatDescription getPlanarFormatDescription (VkFormat format)
{
	if (isYCbCrFormat(format))
		return getYCbCrPlanarFormatDescription(format);
	else
		return getCorePlanarFormatDescription(format);
}

int getPlaneCount (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_G8B8G8R8_422_UNORM:
		case VK_FORMAT_B8G8R8G8_422_UNORM:
		case VK_FORMAT_R10X6_UNORM_PACK16:
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case VK_FORMAT_R12X4_UNORM_PACK16:
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case VK_FORMAT_G16B16G16R16_422_UNORM:
		case VK_FORMAT_B16G16R16G16_422_UNORM:
			return 1;

		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
		case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
			return 2;

		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
			return 3;

		default:
			DE_FATAL("Not YCbCr format");
			return 0;
	}
}

deUint32 getMipmapCount(VkFormat format, const vk::PlanarFormatDescription& formatDescription, const VkImageFormatProperties& imageFormatProperties, const VkExtent3D& extent)
{
	if (isYCbCrFormat(format))
		return 1;
	tcu::UVec3 imageAlignment	= getImageSizeAlignment(formatDescription);
	deUint32 mipmapEdge			= std::max(std::max(extent.width, extent.height), extent.depth);
	if (imageAlignment.x() > 1)
		mipmapEdge = std::min(mipmapEdge, extent.width / imageAlignment.x());
	if (imageAlignment.y() > 1)
		mipmapEdge = std::min(mipmapEdge, extent.height / imageAlignment.y());
	if (imageAlignment.z() > 1)
		mipmapEdge = std::min(mipmapEdge, extent.depth / imageAlignment.z());
	return std::min(static_cast<deUint32>(deFloatLog2(static_cast<float>(mipmapEdge))) + 1u, imageFormatProperties.maxMipLevels);
}

deUint32 getPlaneSizeInBytes (const PlanarFormatDescription&	formatInfo,
							  const VkExtent3D&					baseExtents,
							  const deUint32					planeNdx,
							  const deUint32					mipmapLevel,
							  const deUint32					mipmapMemoryAlignment)
{
	VkExtent3D imageExtent	= getPlaneExtent(formatInfo, baseExtents, planeNdx, mipmapLevel);
	imageExtent.width		/= formatInfo.blockWidth;
	imageExtent.height		/= formatInfo.blockHeight;
	return deAlign32( formatInfo.planes[planeNdx].elementSizeBytes * imageExtent.width * imageExtent.height * imageExtent.depth, mipmapMemoryAlignment);
}

deUint32 getPlaneSizeInBytes (const PlanarFormatDescription&	formatInfo,
							  const tcu::UVec2&					baseExtents,
							  const deUint32					planeNdx,
							  const deUint32					mipmapLevel,
							  const deUint32					mipmapMemoryAlignment)
{
	tcu::UVec2 mipExtents = getPlaneExtent(formatInfo, baseExtents, planeNdx, mipmapLevel) / tcu::UVec2(formatInfo.blockWidth, formatInfo.blockHeight);
	return deAlign32( formatInfo.planes[planeNdx].elementSizeBytes * mipExtents.x() * mipExtents.y(), mipmapMemoryAlignment);
}

VkExtent3D getPlaneExtent(const PlanarFormatDescription&	formatInfo,
						  const VkExtent3D&					baseExtents,
						  const deUint32					planeNdx,
						  const deUint32					mipmapLevel)
{
	deUint32	widthDivisor	= formatInfo.planes[planeNdx].widthDivisor;
	deUint32	heightDivisor	= formatInfo.planes[planeNdx].heightDivisor;
	deUint32	depthDivisor	= 1u;
	VkExtent3D	mip0Extents		{ baseExtents.width / widthDivisor, baseExtents.height / heightDivisor, baseExtents.depth / depthDivisor };

	return mipLevelExtents(mip0Extents, mipmapLevel);
}

tcu::UVec2 getPlaneExtent(const PlanarFormatDescription&	formatInfo,
						  const tcu::UVec2&					baseExtents,
						  const deUint32					planeNdx,
						  const deUint32					mipmapLevel)
{
	deUint32 widthDivisor			= formatInfo.planes[planeNdx].widthDivisor;
	deUint32 heightDivisor			= formatInfo.planes[planeNdx].heightDivisor;
	tcu::UVec2 mip0Extents			{ baseExtents.x() / widthDivisor, baseExtents.y() / heightDivisor };

	return tcu::UVec2
	{
		std::max(mip0Extents.x() >> mipmapLevel, 1u),
		std::max(mip0Extents.y() >> mipmapLevel, 1u)
	};
}

tcu::UVec3 getImageSizeAlignment(VkFormat format)
{
	return getImageSizeAlignment(getPlanarFormatDescription(format));
}

tcu::UVec3 getImageSizeAlignment(const PlanarFormatDescription&	formatInfo)
{
	tcu::UVec3 imgAlignment{ formatInfo.blockWidth, formatInfo.blockHeight, 1 };
	for (deUint32 planeNdx = 0; planeNdx < formatInfo.numPlanes; ++planeNdx)
	{
		imgAlignment.x() = std::max(imgAlignment.x(), static_cast<deUint32>(formatInfo.planes[planeNdx].widthDivisor));
		imgAlignment.y() = std::max(imgAlignment.y(), static_cast<deUint32>(formatInfo.planes[planeNdx].heightDivisor));
	}
	return imgAlignment;
}

tcu::UVec2 getBlockExtent(VkFormat format)
{
	return getBlockExtent(getPlanarFormatDescription(format));
}

tcu::UVec2 getBlockExtent(const PlanarFormatDescription& formatInfo)
{
	return tcu::UVec2{ formatInfo.blockWidth, formatInfo.blockHeight };
}

VkFormat getPlaneCompatibleFormat(VkFormat format, deUint32 planeNdx)
{
	return getPlaneCompatibleFormat(getPlanarFormatDescription(format), planeNdx);
}

VkFormat getPlaneCompatibleFormat(const PlanarFormatDescription& formatInfo, deUint32 planeNdx)
{
	DE_ASSERT(planeNdx < formatInfo.numPlanes);
	return formatInfo.planes[planeNdx].planeCompatibleFormat;
}

VkImageAspectFlagBits getPlaneAspect (deUint32 planeNdx)
{
	DE_ASSERT(de::inBounds(planeNdx, 0u, 3u));
	return (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_PLANE_0_BIT << planeNdx);
}

deUint32 getAspectPlaneNdx (VkImageAspectFlagBits flags)
{
	switch (flags)
	{
		case VK_IMAGE_ASPECT_PLANE_0_BIT:	return 0;
		case VK_IMAGE_ASPECT_PLANE_1_BIT:	return 1;
		case VK_IMAGE_ASPECT_PLANE_2_BIT:	return 2;
		default:
			DE_FATAL("Invalid plane aspect");
			return 0;
	}
}

bool isChromaSubsampled (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_G8B8G8R8_422_UNORM:
		case VK_FORMAT_B8G8R8G8_422_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
		case VK_FORMAT_G16B16G16R16_422_UNORM:
		case VK_FORMAT_B16G16R16G16_422_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
			return true;

		default:
			return false;
	}
}

bool isSupportedByFramework (VkFormat format)
{
	if (format == VK_FORMAT_UNDEFINED || format > VK_CORE_FORMAT_LAST)
		return false;

	switch (format)
	{
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_UINT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			// \todo [2016-12-01 pyry] Support 64-bit channel types
			return false;

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
			return false;

		default:
			return true;
	}
}

void checkImageSupport (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const VkImageCreateInfo& imageCreateInfo)
{
	VkImageFormatProperties imageFormatProperties;

	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, imageCreateInfo.format, imageCreateInfo.imageType,
												   imageCreateInfo.tiling, imageCreateInfo.usage, imageCreateInfo.flags,
												   &imageFormatProperties))
	{
		TCU_THROW(NotSupportedError, "Image format not supported.");
	}
	if (((VkSampleCountFlagBits)imageFormatProperties.sampleCounts & imageCreateInfo.samples) == 0)
	{
		TCU_THROW(NotSupportedError, "Sample count not supported.");
	}
	if (imageFormatProperties.maxArrayLayers < imageCreateInfo.arrayLayers)
	{
		TCU_THROW(NotSupportedError, "Layer count not supported.");
	}
}

VkFormat mapTextureFormat (const tcu::TextureFormat& format)
{
	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST < (1<<16));
	DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELTYPE_LAST < (1<<16));

#define PACK_FMT(ORDER, TYPE) ((int(ORDER) << 16) | int(TYPE))
#define FMT_CASE(ORDER, TYPE) PACK_FMT(tcu::TextureFormat::ORDER, tcu::TextureFormat::TYPE)

	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

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
		case FMT_CASE(R, UNSIGNED_INT64):					return VK_FORMAT_R64_UINT;
		case FMT_CASE(R, SIGNED_INT64):						return VK_FORMAT_R64_SINT;
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

		case FMT_CASE(R,	UNORM_SHORT_10):				return VK_FORMAT_R10X6_UNORM_PACK16;
		case FMT_CASE(RG,	UNORM_SHORT_10):				return VK_FORMAT_R10X6G10X6_UNORM_2PACK16;
		case FMT_CASE(RGBA,	UNORM_SHORT_10):				return VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16;

		case FMT_CASE(R,	UNORM_SHORT_12):				return VK_FORMAT_R12X4_UNORM_PACK16;
		case FMT_CASE(RG,	UNORM_SHORT_12):				return VK_FORMAT_R12X4G12X4_UNORM_2PACK16;
		case FMT_CASE(RGBA,	UNORM_SHORT_12):				return VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16;

		case FMT_CASE(R,	USCALED_INT8):					return VK_FORMAT_R8_USCALED;
		case FMT_CASE(RG,	USCALED_INT8):					return VK_FORMAT_R8G8_USCALED;
		case FMT_CASE(RGB,	USCALED_INT8):					return VK_FORMAT_R8G8B8_USCALED;
		case FMT_CASE(RGBA,	USCALED_INT8):					return VK_FORMAT_R8G8B8A8_USCALED;

		case FMT_CASE(R,	USCALED_INT16):					return VK_FORMAT_R16_USCALED;
		case FMT_CASE(RG,	USCALED_INT16):					return VK_FORMAT_R16G16_USCALED;
		case FMT_CASE(RGB,	USCALED_INT16):					return VK_FORMAT_R16G16B16_USCALED;
		case FMT_CASE(RGBA,	USCALED_INT16):					return VK_FORMAT_R16G16B16A16_USCALED;

		case FMT_CASE(R,	SSCALED_INT8):					return VK_FORMAT_R8_SSCALED;
		case FMT_CASE(RG,	SSCALED_INT8):					return VK_FORMAT_R8G8_SSCALED;
		case FMT_CASE(RGB,	SSCALED_INT8):					return VK_FORMAT_R8G8B8_SSCALED;
		case FMT_CASE(RGBA,	SSCALED_INT8):					return VK_FORMAT_R8G8B8A8_SSCALED;

		case FMT_CASE(R,	SSCALED_INT16):					return VK_FORMAT_R16_SSCALED;
		case FMT_CASE(RG,	SSCALED_INT16):					return VK_FORMAT_R16G16_SSCALED;
		case FMT_CASE(RGB,	SSCALED_INT16):					return VK_FORMAT_R16G16B16_SSCALED;
		case FMT_CASE(RGBA,	SSCALED_INT16):					return VK_FORMAT_R16G16B16A16_SSCALED;

		case FMT_CASE(RGBA, USCALED_INT_1010102_REV):		return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
		case FMT_CASE(RGBA, SSCALED_INT_1010102_REV):		return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;

		case FMT_CASE(ARGB, UNORM_SHORT_4444):				return VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT;
		case FMT_CASE(ABGR, UNORM_SHORT_4444):				return VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT;

		default:
			TCU_THROW(InternalError, "Unknown texture format");
	}

#undef PACK_FMT
#undef FMT_CASE
}

VkFormat mapCompressedTextureFormat (const tcu::CompressedTexFormat format)
{
	// update this mapping if CompressedTexFormat changes
	DE_STATIC_ASSERT(tcu::COMPRESSEDTEXFORMAT_LAST == 55);

	switch (format)
	{
		case tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8:						return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8:						return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:	return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:	return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:					return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:			return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;

		case tcu::COMPRESSEDTEXFORMAT_EAC_R11:							return VK_FORMAT_EAC_R11_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:					return VK_FORMAT_EAC_R11_SNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_EAC_RG11:							return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:					return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;

		case tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA:					return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA:					return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA:					return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA:					return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA:					return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA:					return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA:					return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA:					return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA:					return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA:					return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA:					return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA:					return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA:					return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA:					return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:			return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;

		case tcu::COMPRESSEDTEXFORMAT_BC1_RGB_UNORM_BLOCK:				return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK:				return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC1_RGBA_UNORM_BLOCK:				return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK:				return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC2_UNORM_BLOCK:					return VK_FORMAT_BC2_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK:					return VK_FORMAT_BC2_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC3_UNORM_BLOCK:					return VK_FORMAT_BC3_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK:					return VK_FORMAT_BC3_SRGB_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK:					return VK_FORMAT_BC4_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK:					return VK_FORMAT_BC4_SNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK:					return VK_FORMAT_BC5_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK:					return VK_FORMAT_BC5_SNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK:				return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK:				return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK:					return VK_FORMAT_BC7_UNORM_BLOCK;
		case tcu::COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:					return VK_FORMAT_BC7_SRGB_BLOCK;

		default:
			TCU_THROW(InternalError, "Unknown texture format");
			return VK_FORMAT_UNDEFINED;
	}
}

tcu::TextureFormat mapVkFormat (VkFormat format)
{
	using tcu::TextureFormat;

	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

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
		case VK_FORMAT_R8_USCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::USCALED_INT8);
		case VK_FORMAT_R8_SSCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::SSCALED_INT8);
		case VK_FORMAT_R8_UINT:					return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8_SINT:					return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8_SRGB:					return TextureFormat(TextureFormat::sR,		TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8_UNORM:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8_SNORM:				return TextureFormat(TextureFormat::RG,		TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8_USCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::USCALED_INT8);
		case VK_FORMAT_R8G8_SSCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::SSCALED_INT8);
		case VK_FORMAT_R8G8_UINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8_SINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8_SRGB:				return TextureFormat(TextureFormat::sRG,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8B8_UNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8B8_SNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8B8_USCALED:			return TextureFormat(TextureFormat::RGB,	TextureFormat::USCALED_INT8);
		case VK_FORMAT_R8G8B8_SSCALED:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SSCALED_INT8);
		case VK_FORMAT_R8G8B8_UINT:				return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8_SINT:				return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8_SRGB:				return TextureFormat(TextureFormat::sRGB,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R8G8B8A8_UNORM:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_R8G8B8A8_SNORM:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_R8G8B8A8_USCALED:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::USCALED_INT8);
		case VK_FORMAT_R8G8B8A8_SSCALED:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SSCALED_INT8);
		case VK_FORMAT_R8G8B8A8_UINT:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SINT:			return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_R8G8B8A8_SRGB:			return TextureFormat(TextureFormat::sRGBA,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_R16_UNORM:				return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16_SNORM:				return TextureFormat(TextureFormat::R,		TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16_USCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::USCALED_INT16);
		case VK_FORMAT_R16_SSCALED:				return TextureFormat(TextureFormat::R,		TextureFormat::SSCALED_INT16);
		case VK_FORMAT_R16_UINT:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16_SINT:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16_SFLOAT:				return TextureFormat(TextureFormat::R,		TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16_UNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16_SNORM:			return TextureFormat(TextureFormat::RG,		TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16_USCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::USCALED_INT16);
		case VK_FORMAT_R16G16_SSCALED:			return TextureFormat(TextureFormat::RG,		TextureFormat::SSCALED_INT16);
		case VK_FORMAT_R16G16_UINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16_SINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16_SFLOAT:			return TextureFormat(TextureFormat::RG,		TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16B16_UNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16B16_SNORM:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16B16_USCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::USCALED_INT16);
		case VK_FORMAT_R16G16B16_SSCALED:		return TextureFormat(TextureFormat::RGB,	TextureFormat::SSCALED_INT16);
		case VK_FORMAT_R16G16B16_UINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT16);
		case VK_FORMAT_R16G16B16_SINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT16);
		case VK_FORMAT_R16G16B16_SFLOAT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::HALF_FLOAT);

		case VK_FORMAT_R16G16B16A16_UNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT16);
		case VK_FORMAT_R16G16B16A16_SNORM:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT16);
		case VK_FORMAT_R16G16B16A16_USCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::USCALED_INT16);
		case VK_FORMAT_R16G16B16A16_SSCALED:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SSCALED_INT16);
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

		case VK_FORMAT_R64_UINT:				return TextureFormat(TextureFormat::R,		TextureFormat::UNSIGNED_INT64);
		case VK_FORMAT_R64G64_UINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::UNSIGNED_INT64);
		case VK_FORMAT_R64G64B64_UINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT64);
		case VK_FORMAT_R64G64B64A64_UINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT64);
		case VK_FORMAT_R64_SINT:				return TextureFormat(TextureFormat::R,		TextureFormat::SIGNED_INT64);
		case VK_FORMAT_R64G64_SINT:				return TextureFormat(TextureFormat::RG,		TextureFormat::SIGNED_INT64);
		case VK_FORMAT_R64G64B64_SINT:			return TextureFormat(TextureFormat::RGB,	TextureFormat::SIGNED_INT64);
		case VK_FORMAT_R64G64B64A64_SINT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT64);
		case VK_FORMAT_R64_SFLOAT:				return TextureFormat(TextureFormat::R,		TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64_SFLOAT:			return TextureFormat(TextureFormat::RG,		TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64B64_SFLOAT:		return TextureFormat(TextureFormat::RGB,	TextureFormat::FLOAT64);
		case VK_FORMAT_R64G64B64A64_SFLOAT:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::FLOAT64);

		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:	return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT_11F_11F_10F_REV);
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:	return TextureFormat(TextureFormat::RGB,	TextureFormat::UNSIGNED_INT_999_E5_REV);

		case VK_FORMAT_B8G8R8_UNORM:			return TextureFormat(TextureFormat::BGR,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_B8G8R8_SNORM:			return TextureFormat(TextureFormat::BGR,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_B8G8R8_USCALED:			return TextureFormat(TextureFormat::BGR,	TextureFormat::USCALED_INT8);
		case VK_FORMAT_B8G8R8_SSCALED:			return TextureFormat(TextureFormat::BGR,	TextureFormat::SSCALED_INT8);
		case VK_FORMAT_B8G8R8_UINT:				return TextureFormat(TextureFormat::BGR,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_B8G8R8_SINT:				return TextureFormat(TextureFormat::BGR,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_B8G8R8_SRGB:				return TextureFormat(TextureFormat::sBGR,	TextureFormat::UNORM_INT8);

		case VK_FORMAT_B8G8R8A8_UNORM:			return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_INT8);
		case VK_FORMAT_B8G8R8A8_SNORM:			return TextureFormat(TextureFormat::BGRA,	TextureFormat::SNORM_INT8);
		case VK_FORMAT_B8G8R8A8_USCALED:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::USCALED_INT8);
		case VK_FORMAT_B8G8R8A8_SSCALED:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::SSCALED_INT8);
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
		case VK_FORMAT_A8B8G8R8_USCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::USCALED_INT8);
		case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SSCALED_INT8);
		case VK_FORMAT_A8B8G8R8_UINT_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT8);
		case VK_FORMAT_A8B8G8R8_SINT_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT8);
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:	return TextureFormat(TextureFormat::sRGBA,	TextureFormat::UNORM_INT8);
#else
#	error "Big-endian not supported"
#endif

		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNORM_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_SNORM_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SNORM_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::USCALED_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:	return TextureFormat(TextureFormat::BGRA,	TextureFormat::SSCALED_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_UINT_PACK32:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_A2R10G10B10_SINT_PACK32:		return TextureFormat(TextureFormat::BGRA,	TextureFormat::SIGNED_INT_1010102_REV);

		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_SNORM_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SNORM_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_USCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::USCALED_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::SSCALED_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNSIGNED_INT_1010102_REV);
		case VK_FORMAT_A2B10G10R10_SINT_PACK32:		return TextureFormat(TextureFormat::RGBA,	TextureFormat::SIGNED_INT_1010102_REV);

		// YCbCr formats that can be mapped
		case VK_FORMAT_R10X6_UNORM_PACK16:					return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_SHORT_10);
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_SHORT_10);
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_10);

		case VK_FORMAT_R12X4_UNORM_PACK16:					return TextureFormat(TextureFormat::R,		TextureFormat::UNORM_SHORT_12);
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:			return TextureFormat(TextureFormat::RG,		TextureFormat::UNORM_SHORT_12);
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:	return TextureFormat(TextureFormat::RGBA,	TextureFormat::UNORM_SHORT_12);

		case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT:				return TextureFormat(TextureFormat::ARGB,	TextureFormat::UNORM_SHORT_4444);
		case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT:				return TextureFormat(TextureFormat::ABGR,	TextureFormat::UNORM_SHORT_4444);

		default:
			TCU_THROW(InternalError, "Unknown image format");
	}
}

tcu::CompressedTexFormat mapVkCompressedFormat (VkFormat format)
{
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

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
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA;
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA;
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA;
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8;
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA;
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8;
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

		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_BC1_RGB_UNORM_BLOCK;
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK;
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:		return tcu::COMPRESSEDTEXFORMAT_BC1_RGBA_UNORM_BLOCK;
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK;
		case VK_FORMAT_BC2_UNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC2_UNORM_BLOCK;
		case VK_FORMAT_BC2_SRGB_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK;
		case VK_FORMAT_BC3_UNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC3_UNORM_BLOCK;
		case VK_FORMAT_BC3_SRGB_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK;
		case VK_FORMAT_BC4_UNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK;
		case VK_FORMAT_BC4_SNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK;
		case VK_FORMAT_BC5_UNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK;
		case VK_FORMAT_BC5_SNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK;
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK;
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:			return tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK;
		case VK_FORMAT_BC7_UNORM_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK;
		case VK_FORMAT_BC7_SRGB_BLOCK:				return tcu::COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK;

		default:
			TCU_THROW(InternalError, "Unknown image format");
			return tcu::COMPRESSEDTEXFORMAT_LAST;
	}
}

static bool isScaledFormat (VkFormat format)
{
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

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

tcu::TextureFormat getChannelAccessFormat (tcu::TextureChannelClass	type,
										   deUint32					offsetBits,
										   deUint32					sizeBits)
{
	using tcu::TextureFormat;

	if (offsetBits == 0)
	{
		static const TextureFormat::ChannelType	s_size8[tcu::TEXTURECHANNELCLASS_LAST] =
		{
			TextureFormat::SNORM_INT8,			// snorm
			TextureFormat::UNORM_INT8,			// unorm
			TextureFormat::SIGNED_INT8,			// sint
			TextureFormat::UNSIGNED_INT8,		// uint
			TextureFormat::CHANNELTYPE_LAST,	// float
		};
		static const TextureFormat::ChannelType	s_size16[tcu::TEXTURECHANNELCLASS_LAST] =
		{
			TextureFormat::SNORM_INT16,			// snorm
			TextureFormat::UNORM_INT16,			// unorm
			TextureFormat::SIGNED_INT16,		// sint
			TextureFormat::UNSIGNED_INT16,		// uint
			TextureFormat::HALF_FLOAT,			// float
		};
		static const TextureFormat::ChannelType	s_size32[tcu::TEXTURECHANNELCLASS_LAST] =
		{
			TextureFormat::SNORM_INT32,			// snorm
			TextureFormat::UNORM_INT32,			// unorm
			TextureFormat::SIGNED_INT32,		// sint
			TextureFormat::UNSIGNED_INT32,		// uint
			TextureFormat::FLOAT,				// float
		};

		static const TextureFormat::ChannelType	s_size64[tcu::TEXTURECHANNELCLASS_LAST] =
		{
			TextureFormat::CHANNELTYPE_LAST,	// snorm
			TextureFormat::CHANNELTYPE_LAST,	// unorm
			TextureFormat::SIGNED_INT64,		// sint
			TextureFormat::UNSIGNED_INT64,		// uint
			TextureFormat::FLOAT64,				// float
		};

		TextureFormat::ChannelType	chnType		= TextureFormat::CHANNELTYPE_LAST;

		if (sizeBits == 8)
			chnType = s_size8[type];
		else if (sizeBits == 16)
			chnType = s_size16[type];
		else if (sizeBits == 32)
			chnType = s_size32[type];
		else if (sizeBits == 64)
			chnType = s_size64[type];

		if (chnType != TextureFormat::CHANNELTYPE_LAST)
			return TextureFormat(TextureFormat::R, chnType);
	}
	else
	{
		if (type		== tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT	&&
			offsetBits	== 6												&&
			sizeBits	== 10)
			return TextureFormat(TextureFormat::R, TextureFormat::UNORM_SHORT_10);
		else if (type		== tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT	&&
				 offsetBits	== 4												&&
				 sizeBits	== 12)
			return TextureFormat(TextureFormat::R, TextureFormat::UNORM_SHORT_12);
	}

	TCU_THROW(InternalError, "Channel access format is not supported");
}

tcu::PixelBufferAccess getChannelAccess (const PlanarFormatDescription&	formatInfo,
										 const tcu::UVec2&				size,
										 const deUint32*				planeRowPitches,
										 void* const*					planePtrs,
										 deUint32						channelNdx)
{
	DE_ASSERT(formatInfo.hasChannelNdx(channelNdx));

	const deUint32	planeNdx			= formatInfo.channels[channelNdx].planeNdx;
	const deUint32	planeOffsetBytes	= formatInfo.channels[channelNdx].offsetBits / 8;
	const deUint32	valueOffsetBits		= formatInfo.channels[channelNdx].offsetBits % 8;
	const deUint32	pixelStrideBytes	= formatInfo.channels[channelNdx].strideBytes;

	DE_ASSERT(size.x() % (formatInfo.blockWidth * formatInfo.planes[planeNdx].widthDivisor) == 0);
	DE_ASSERT(size.y() % (formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor) == 0);

	const deUint32	accessHeight		= size.y() / ( formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor );
	const deUint32	elementSizeBytes	= formatInfo.planes[planeNdx].elementSizeBytes;
	const deUint32	rowPitch			= planeRowPitches[planeNdx];

	DE_ASSERT(elementSizeBytes % pixelStrideBytes == 0);

	tcu::IVec3		texDivider(
		std::max(formatInfo.blockWidth * formatInfo.planes[planeNdx].widthDivisor * pixelStrideBytes / elementSizeBytes, 1u),
		std::max(formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor * pixelStrideBytes / elementSizeBytes, 1u),
		1);

	return tcu::PixelBufferAccess(getChannelAccessFormat((tcu::TextureChannelClass)formatInfo.channels[channelNdx].type,
														 valueOffsetBits,
														 formatInfo.channels[channelNdx].sizeBits),
														 tcu::IVec3((int)size.x(), (int)size.y(), 1),
														 tcu::IVec3((int)pixelStrideBytes, (int)rowPitch, (int)(accessHeight*rowPitch)),
														 texDivider,
														 (deUint8*)planePtrs[planeNdx] + planeOffsetBytes);
}

tcu::ConstPixelBufferAccess getChannelAccess (const PlanarFormatDescription&	formatInfo,
											  const tcu::UVec2&					size,
											  const deUint32*					planeRowPitches,
											  const void* const*				planePtrs,
											  deUint32							channelNdx)
{
	return getChannelAccess(formatInfo, size, planeRowPitches, const_cast<void* const*>(planePtrs), channelNdx);
}

tcu::PixelBufferAccess getChannelAccess (const PlanarFormatDescription&	formatInfo,
										 const tcu::UVec3&				size,
										 const deUint32*				planeRowPitches,
										 void* const*					planePtrs,
										 deUint32						channelNdx)
{
	DE_ASSERT(formatInfo.hasChannelNdx(channelNdx));

	const deUint32	planeNdx			= formatInfo.channels[channelNdx].planeNdx;
	const deUint32	planeOffsetBytes	= formatInfo.channels[channelNdx].offsetBits / 8;
	const deUint32	valueOffsetBits		= formatInfo.channels[channelNdx].offsetBits % 8;
	const deUint32	pixelStrideBytes	= formatInfo.channels[channelNdx].strideBytes;

	DE_ASSERT(size.x() % (formatInfo.blockWidth * formatInfo.planes[planeNdx].widthDivisor) == 0);
	DE_ASSERT(size.y() % (formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor) == 0);

	const deUint32	accessHeight		= size.y() / ( formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor );
	const deUint32	elementSizeBytes	= formatInfo.planes[planeNdx].elementSizeBytes;
	const deUint32	rowPitch			= planeRowPitches[planeNdx];

	DE_ASSERT(elementSizeBytes % pixelStrideBytes == 0);

	tcu::IVec3		texDivider(
		std::max(formatInfo.blockWidth * formatInfo.planes[planeNdx].widthDivisor * pixelStrideBytes / elementSizeBytes, 1u),
		std::max(formatInfo.blockHeight * formatInfo.planes[planeNdx].heightDivisor * pixelStrideBytes / elementSizeBytes, 1u),
		1);

	return tcu::PixelBufferAccess(getChannelAccessFormat((tcu::TextureChannelClass)formatInfo.channels[channelNdx].type,
														 valueOffsetBits,
														 formatInfo.channels[channelNdx].sizeBits),
														 tcu::IVec3((int)size.x(), (int)size.y(), (int)size.z()),
														 tcu::IVec3((int)pixelStrideBytes, (int)rowPitch, (int)(accessHeight*rowPitch)),
														 texDivider,
														 (deUint8*)planePtrs[planeNdx] + planeOffsetBytes);
}

tcu::ConstPixelBufferAccess getChannelAccess (const PlanarFormatDescription&	formatInfo,
											  const tcu::UVec3&					size,
											  const deUint32*					planeRowPitches,
											  const void* const*				planePtrs,
											  deUint32							channelNdx)
{
	return getChannelAccess(formatInfo, size, planeRowPitches, const_cast<void* const*>(planePtrs), channelNdx);
}

void imageUtilSelfTest (void)
{
	for (int formatNdx = 0; formatNdx < VK_CORE_FORMAT_LAST; formatNdx++)
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

	for (int formatNdx = VK_FORMAT_G8B8G8R8_422_UNORM; formatNdx <= VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM; formatNdx++)
	{
		const VkFormat					format	= (VkFormat)formatNdx;
		const PlanarFormatDescription&	info	= getPlanarFormatDescription(format);

		DE_TEST_ASSERT(isYCbCrFormat(format));
		DE_TEST_ASSERT(de::inRange<deUint8>(info.numPlanes, 1u, 3u));
		DE_TEST_ASSERT(info.numPlanes == getPlaneCount(format));
	}

	for (int formatNdx = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT; formatNdx <= VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT; formatNdx++)
	{
		const VkFormat					format	= (VkFormat)formatNdx;
		const PlanarFormatDescription&	info	= getPlanarFormatDescription(format);

		DE_TEST_ASSERT(isYCbCrFormat(format));
		DE_TEST_ASSERT(de::inRange<deUint8>(info.numPlanes, 1u, 3u));
		DE_TEST_ASSERT(info.numPlanes == getPlaneCount(format));
	}
}

struct CompressedFormatParameters
{
	VkFormat	format;
	deUint32	blockBytes;
	deUint32	blockWidth;
	deUint32	blockHeight;
};

CompressedFormatParameters	compressedFormatParameters[VK_FORMAT_ASTC_12x12_SRGB_BLOCK - VK_FORMAT_BC1_RGB_UNORM_BLOCK + 1] =
{
	{ VK_FORMAT_BC1_RGB_UNORM_BLOCK,		8,	4,	4 },
	{ VK_FORMAT_BC1_RGB_SRGB_BLOCK,			8,	4,	4 },
	{ VK_FORMAT_BC1_RGBA_UNORM_BLOCK,		8,	4,	4 },
	{ VK_FORMAT_BC1_RGBA_SRGB_BLOCK,		8,	4,	4 },
	{ VK_FORMAT_BC2_UNORM_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC2_SRGB_BLOCK,				16,	4,	4 },
	{ VK_FORMAT_BC3_UNORM_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC3_SRGB_BLOCK,				16,	4,	4 },
	{ VK_FORMAT_BC4_UNORM_BLOCK,			8,	4,	4 },
	{ VK_FORMAT_BC4_SNORM_BLOCK,			8,	4,	4 },
	{ VK_FORMAT_BC5_UNORM_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC5_SNORM_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC6H_UFLOAT_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC6H_SFLOAT_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC7_UNORM_BLOCK,			16,	4,	4 },
	{ VK_FORMAT_BC7_SRGB_BLOCK,				16,	4,	4 },
	{ VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,	8,	4,	4 },
	{ VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,		8,	4,	4 },
	{ VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,	8,	4,	4 },
	{ VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,	8,	4,	4 },
	{ VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,	16,	4,	4 },
	{ VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,	16,	4,	4 },
	{ VK_FORMAT_EAC_R11_UNORM_BLOCK,		8,	4,	4 },
	{ VK_FORMAT_EAC_R11_SNORM_BLOCK,		8,	4,	4 },
	{ VK_FORMAT_EAC_R11G11_UNORM_BLOCK,		16,	4,	4 },
	{ VK_FORMAT_EAC_R11G11_SNORM_BLOCK,		16,	4,	4 },
	{ VK_FORMAT_ASTC_4x4_UNORM_BLOCK,		16,	4,	4 },
	{ VK_FORMAT_ASTC_4x4_SRGB_BLOCK,		16,	4,	4 },
	{ VK_FORMAT_ASTC_5x4_UNORM_BLOCK,		16,	5,	4 },
	{ VK_FORMAT_ASTC_5x4_SRGB_BLOCK,		16,	5,	4 },
	{ VK_FORMAT_ASTC_5x5_UNORM_BLOCK,		16,	5,	5 },
	{ VK_FORMAT_ASTC_5x5_SRGB_BLOCK,		16,	5,	5 },
	{ VK_FORMAT_ASTC_6x5_UNORM_BLOCK,		16,	6,	5 },
	{ VK_FORMAT_ASTC_6x5_SRGB_BLOCK,		16,	6,	5 },
	{ VK_FORMAT_ASTC_6x6_UNORM_BLOCK,		16,	6,	6 },
	{ VK_FORMAT_ASTC_6x6_SRGB_BLOCK,		16,	6,	6 },
	{ VK_FORMAT_ASTC_8x5_UNORM_BLOCK,		16,	8,	5 },
	{ VK_FORMAT_ASTC_8x5_SRGB_BLOCK,		16,	8,	5 },
	{ VK_FORMAT_ASTC_8x6_UNORM_BLOCK,		16,	8,	6 },
	{ VK_FORMAT_ASTC_8x6_SRGB_BLOCK,		16,	8,	6 },
	{ VK_FORMAT_ASTC_8x8_UNORM_BLOCK,		16,	8,	8 },
	{ VK_FORMAT_ASTC_8x8_SRGB_BLOCK,		16,	8,	8 },
	{ VK_FORMAT_ASTC_10x5_UNORM_BLOCK,		16,	10,	5 },
	{ VK_FORMAT_ASTC_10x5_SRGB_BLOCK,		16,	10,	5 },
	{ VK_FORMAT_ASTC_10x6_UNORM_BLOCK,		16,	10,	6 },
	{ VK_FORMAT_ASTC_10x6_SRGB_BLOCK,		16,	10,	6 },
	{ VK_FORMAT_ASTC_10x8_UNORM_BLOCK,		16,	10,	8 },
	{ VK_FORMAT_ASTC_10x8_SRGB_BLOCK,		16,	10,	8 },
	{ VK_FORMAT_ASTC_10x10_UNORM_BLOCK,		16,	10,	10 },
	{ VK_FORMAT_ASTC_10x10_SRGB_BLOCK,		16,	10,	10 },
	{ VK_FORMAT_ASTC_12x10_UNORM_BLOCK,		16,	12,	10 },
	{ VK_FORMAT_ASTC_12x10_SRGB_BLOCK,		16,	12,	10 },
	{ VK_FORMAT_ASTC_12x12_UNORM_BLOCK,		16,	12,	12 },
	{ VK_FORMAT_ASTC_12x12_SRGB_BLOCK,		16,	12,	12 }
};

deUint32 getFormatComponentWidth (const VkFormat format, const deUint32 componentNdx)
{
	const tcu::TextureFormat	tcuFormat		(mapVkFormat(format));
	const deUint32				componentCount	(tcu::getNumUsedChannels(tcuFormat.order));

	if (componentNdx >= componentCount)
		DE_FATAL("Component index out of range");
	else
	{
		switch (tcuFormat.type)
		{
			case tcu::TextureFormat::UNORM_INT8:
			case tcu::TextureFormat::SNORM_INT8:
			case tcu::TextureFormat::UNSIGNED_INT8:
			case tcu::TextureFormat::SIGNED_INT8:
				return 8;

			case tcu::TextureFormat::UNORM_SHORT_12:
				return 12;

			case tcu::TextureFormat::UNORM_INT16:
			case tcu::TextureFormat::SNORM_INT16:
			case tcu::TextureFormat::UNSIGNED_INT16:
			case tcu::TextureFormat::SIGNED_INT16:
				return 16;

			case tcu::TextureFormat::UNORM_INT24:
			case tcu::TextureFormat::UNSIGNED_INT24:
				return 24;

			case tcu::TextureFormat::UNORM_INT32:
			case tcu::TextureFormat::SNORM_INT32:
			case tcu::TextureFormat::UNSIGNED_INT32:
			case tcu::TextureFormat::SIGNED_INT32:
			case tcu::TextureFormat::FLOAT:
				return 32;

			case tcu::TextureFormat::FLOAT64:
			case tcu::TextureFormat::UNSIGNED_INT64:
			case tcu::TextureFormat::SIGNED_INT64:
				return 64;

			// Packed formats
			case tcu::TextureFormat::UNORM_SHORT_4444:
			case tcu::TextureFormat::UNSIGNED_SHORT_4444:
				return 4;

			case tcu::TextureFormat::UNORM_SHORT_565:
			case tcu::TextureFormat::UNSIGNED_SHORT_565:
				return (componentNdx == 1 ? 6 : 5);

			case tcu::TextureFormat::UNSIGNED_INT_24_8:
			case tcu::TextureFormat::UNSIGNED_INT_24_8_REV:
			case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
				return (componentNdx == 0 ? 24 : 8);

			case tcu::TextureFormat::UNORM_SHORT_1555:
				return (componentNdx == 0 ? 1 : 5);

			case tcu::TextureFormat::UNORM_INT_1010102_REV:
			case tcu::TextureFormat::SNORM_INT_1010102_REV:
			case tcu::TextureFormat::UNSIGNED_INT_1010102_REV:
			case tcu::TextureFormat::SIGNED_INT_1010102_REV:
				return (componentNdx == 3 ? 2 : 10);

			default:
				DE_FATAL("Format unimplemented");
		}
	}

	return 0;
}

float getRepresentableDiffUnorm (const VkFormat format, const deUint32 componentNdx)
{
	const deUint32 size (getFormatComponentWidth(format, componentNdx));

	return 1.0f / float((1 << (size)) - 1);
}

float getRepresentableDiffSnorm (const VkFormat format, const deUint32 componentNdx)
{
	const deUint32 size (getFormatComponentWidth(format, componentNdx));

	return 1.0f / float((1 << (size - 1)) - 1);
}

deUint32 getBlockSizeInBytes (const VkFormat compressedFormat)
{
	deUint32 formatNdx = static_cast<deUint32>(compressedFormat - VK_FORMAT_BC1_RGB_UNORM_BLOCK);

	DE_ASSERT(deInRange32(formatNdx, 0, DE_LENGTH_OF_ARRAY(compressedFormatParameters)));
	DE_ASSERT(compressedFormatParameters[formatNdx].format == compressedFormat);

	return compressedFormatParameters[formatNdx].blockBytes;
}

deUint32 getBlockWidth (const VkFormat compressedFormat)
{
	deUint32 formatNdx = static_cast<deUint32>(compressedFormat - VK_FORMAT_BC1_RGB_UNORM_BLOCK);

	DE_ASSERT(deInRange32(formatNdx, 0, DE_LENGTH_OF_ARRAY(compressedFormatParameters)));
	DE_ASSERT(compressedFormatParameters[formatNdx].format == compressedFormat);

	return compressedFormatParameters[formatNdx].blockWidth;
}

deUint32 getBlockHeight (const VkFormat compressedFormat)
{
	deUint32 formatNdx = static_cast<deUint32>(compressedFormat - VK_FORMAT_BC1_RGB_UNORM_BLOCK);

	DE_ASSERT(deInRange32(formatNdx, 0, DE_LENGTH_OF_ARRAY(compressedFormatParameters)));
	DE_ASSERT(compressedFormatParameters[formatNdx].format == compressedFormat);

	return compressedFormatParameters[formatNdx].blockHeight;
}

VkFilter mapFilterMode (tcu::Sampler::FilterMode filterMode)
{
	DE_STATIC_ASSERT(tcu::Sampler::FILTERMODE_LAST == 9);

	switch (filterMode)
	{
		case tcu::Sampler::NEAREST:					return VK_FILTER_NEAREST;
		case tcu::Sampler::LINEAR:					return VK_FILTER_LINEAR;
		case tcu::Sampler::CUBIC:					return VK_FILTER_CUBIC_EXT;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return VK_FILTER_NEAREST;
		case tcu::Sampler::NEAREST_MIPMAP_LINEAR:	return VK_FILTER_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return VK_FILTER_LINEAR;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return VK_FILTER_LINEAR;
		case tcu::Sampler::CUBIC_MIPMAP_NEAREST:	return VK_FILTER_CUBIC_EXT;
		case tcu::Sampler::CUBIC_MIPMAP_LINEAR:		return VK_FILTER_CUBIC_EXT;
		default:
			DE_FATAL("Illegal filter mode");
			return (VkFilter)0;
	}
}

VkSamplerMipmapMode mapMipmapMode (tcu::Sampler::FilterMode filterMode)
{
	DE_STATIC_ASSERT(tcu::Sampler::FILTERMODE_LAST == 9);

	// \note VkSamplerCreateInfo doesn't have a flag for disabling mipmapping. Instead
	//		 minLod = 0 and maxLod = 0.25 should be used to match OpenGL NEAREST and LINEAR
	//		 filtering mode behavior.

	switch (filterMode)
	{
		case tcu::Sampler::NEAREST:					return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR:					return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::CUBIC:					return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::NEAREST_MIPMAP_NEAREST:	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::NEAREST_MIPMAP_LINEAR:	return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case tcu::Sampler::LINEAR_MIPMAP_NEAREST:	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::LINEAR_MIPMAP_LINEAR:	return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case tcu::Sampler::CUBIC_MIPMAP_NEAREST:	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case tcu::Sampler::CUBIC_MIPMAP_LINEAR:		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		default:
			DE_FATAL("Illegal filter mode");
			return (VkSamplerMipmapMode)0;
	}
}

VkSamplerAddressMode mapWrapMode (tcu::Sampler::WrapMode wrapMode)
{
	switch (wrapMode)
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
	switch (mode)
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

static VkBorderColor mapBorderColor (tcu::TextureChannelClass channelClass, const rr::GenericVec4& color)
{
	if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
	{
		const tcu::UVec4	uColor	= color.get<deUint32>();

		if (uColor		== tcu::UVec4(0, 0, 0, 0)) return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		else if (uColor	== tcu::UVec4(0, 0, 0, 1)) return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		else if (uColor == tcu::UVec4(1, 1, 1, 1)) return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		else									   return VK_BORDER_COLOR_INT_CUSTOM_EXT;
	}
	else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
	{
		const tcu::IVec4	sColor	= color.get<deInt32>();

		if (sColor		== tcu::IVec4(0, 0, 0, 0)) return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		else if (sColor	== tcu::IVec4(0, 0, 0, 1)) return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		else if (sColor == tcu::IVec4(1, 1, 1, 1)) return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		else									   return	VK_BORDER_COLOR_INT_CUSTOM_EXT;
	}
	else
	{
		const tcu::Vec4		fColor	= color.get<float>();

		if (fColor		== tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)) return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		else if (fColor == tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		else if (fColor == tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)) return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		else												  return VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
	}

	DE_FATAL("Unsupported border color");
	return VK_BORDER_COLOR_MAX_ENUM;
}

VkSamplerCreateInfo mapSampler (const tcu::Sampler& sampler, const tcu::TextureFormat& format, float minLod, float maxLod, bool unnormal)
{
	const bool			compareEnabled	= (sampler.compare != tcu::Sampler::COMPAREMODE_NONE);
	const VkCompareOp	compareOp		= (compareEnabled) ? (mapCompareMode(sampler.compare)) : (VK_COMPARE_OP_ALWAYS);
	const VkBorderColor	borderColor		= mapBorderColor(getTextureChannelClass(format.type), sampler.borderColor);
	const bool			isMipmapEnabled = (sampler.minFilter != tcu::Sampler::NEAREST && sampler.minFilter != tcu::Sampler::LINEAR && sampler.minFilter != tcu::Sampler::CUBIC);

	const VkSamplerCreateInfo	createInfo		=
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		DE_NULL,
		(VkSamplerCreateFlags)0,
		mapFilterMode(sampler.magFilter),							// magFilter
		mapFilterMode(sampler.minFilter),							// minFilter
		mapMipmapMode(sampler.minFilter),							// mipMode
		mapWrapMode(sampler.wrapS),									// addressU
		mapWrapMode(sampler.wrapT),									// addressV
		mapWrapMode(sampler.wrapR),									// addressW
		0.0f,														// mipLodBias
		VK_FALSE,													// anisotropyEnable
		1.0f,														// maxAnisotropy
		(VkBool32)(compareEnabled ? VK_TRUE : VK_FALSE),			// compareEnable
		compareOp,													// compareOp
		(isMipmapEnabled ? minLod : 0.0f),							// minLod
		(isMipmapEnabled ? maxLod : (unnormal ? 0.0f : 0.25f)),		// maxLod
		borderColor,												// borderColor
		(VkBool32)(sampler.normalizedCoords ? VK_FALSE : VK_TRUE),	// unnormalizedCoords
	};

	return createInfo;
}

rr::GenericVec4 mapVkColor (const VkClearColorValue& color)
{
	rr::GenericVec4 value;

	static_assert(sizeof(rr::GenericVec4) == sizeof(VkClearColorValue), "GenericVec4 and VkClearColorValue size mismatch");
	deMemcpy(&value, &color, sizeof(rr::GenericVec4));
	return value;
}

VkClearColorValue mapVkColor(const rr::GenericVec4& color)
{
	VkClearColorValue value;

	static_assert(sizeof(rr::GenericVec4) == sizeof(VkClearColorValue), "GenericVec4 and VkClearColorValue size mismatch");
	deMemcpy(&value, &color, sizeof(VkClearColorValue));
	return value;
}

tcu::Sampler mapVkSampler (const VkSamplerCreateInfo& samplerCreateInfo)
{
	// \note minLod & maxLod are not supported by tcu::Sampler. LOD must be clamped
	//       before passing it to tcu::Texture*::sample*()

	tcu::Sampler::ReductionMode reductionMode = tcu::Sampler::WEIGHTED_AVERAGE;
	rr::GenericVec4 borderColorValue;

	void const *pNext = samplerCreateInfo.pNext;
	while (pNext != DE_NULL)
	{
		const VkStructureType nextType = *reinterpret_cast<const VkStructureType*>(pNext);
		switch (nextType)
		{
			case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
			{
				const VkSamplerReductionModeCreateInfo reductionModeCreateInfo = *reinterpret_cast<const VkSamplerReductionModeCreateInfo*>(pNext);
				reductionMode = mapVkSamplerReductionMode(reductionModeCreateInfo.reductionMode);
				pNext = reinterpret_cast<const VkSamplerReductionModeCreateInfo*>(pNext)->pNext;
				break;
			}
			case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
				pNext = reinterpret_cast<const VkSamplerYcbcrConversionInfo*>(pNext)->pNext;
				break;
			case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
			{
				const VkSamplerCustomBorderColorCreateInfoEXT customBorderColorCreateInfo = *reinterpret_cast<const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext);
				borderColorValue = mapVkColor(customBorderColorCreateInfo.customBorderColor);
				pNext = reinterpret_cast<const VkSamplerCustomBorderColorCreateInfoEXT*>(pNext)->pNext;
				break;
			}
			default:
				TCU_FAIL("Unrecognized sType in chained sampler create info");
		}
	}



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
						 true,
						 tcu::Sampler::MODE_DEPTH,
						 reductionMode);

	if (samplerCreateInfo.anisotropyEnable)
		TCU_THROW(InternalError, "Anisotropic filtering is not supported by tcu::Sampler");

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
		case VK_BORDER_COLOR_FLOAT_CUSTOM_EXT:
		case VK_BORDER_COLOR_INT_CUSTOM_EXT:
			sampler.borderColor = borderColorValue;
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

tcu::Sampler::ReductionMode mapVkSamplerReductionMode (VkSamplerReductionMode reductionMode)
{
	switch (reductionMode)
	{
		case VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE:	return tcu::Sampler::WEIGHTED_AVERAGE;
		case VK_SAMPLER_REDUCTION_MODE_MIN:					return tcu::Sampler::MIN;
		case VK_SAMPLER_REDUCTION_MODE_MAX:					return tcu::Sampler::MAX;
		default:
			break;
	}

	DE_ASSERT(false);
	return tcu::Sampler::REDUCTIONMODE_LAST;
}

tcu::Sampler::FilterMode mapVkMinTexFilter (VkFilter filter, VkSamplerMipmapMode mipMode)
{
	switch (filter)
	{
		case VK_FILTER_LINEAR:
			switch (mipMode)
			{
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:		return tcu::Sampler::LINEAR_MIPMAP_LINEAR;
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:	return tcu::Sampler::LINEAR_MIPMAP_NEAREST;
				default:
					break;
			}
			break;

		case VK_FILTER_NEAREST:
			switch (mipMode)
			{
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:		return tcu::Sampler::NEAREST_MIPMAP_LINEAR;
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:	return tcu::Sampler::NEAREST_MIPMAP_NEAREST;
				default:
					break;
			}
			break;
		case VK_FILTER_CUBIC_EXT:
			switch (mipMode)
			{
			case VK_SAMPLER_MIPMAP_MODE_LINEAR:		return tcu::Sampler::CUBIC_MIPMAP_LINEAR;
			case VK_SAMPLER_MIPMAP_MODE_NEAREST:	return tcu::Sampler::CUBIC_MIPMAP_NEAREST;
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
		case VK_FILTER_CUBIC_EXT:	return tcu::Sampler::CUBIC;
		default:
			break;
	}

	DE_ASSERT(false);
	return tcu::Sampler::FILTERMODE_LAST;
}

//! Get a format that matches the layout in buffer memory used for a
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

//! Get a format that matches the layout in buffer memory used for a
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

VkImageAspectFlags getImageAspectFlags (const tcu::TextureFormat textureFormat)
{
	VkImageAspectFlags imageAspectFlags = 0;

	if (tcu::hasDepthComponent(textureFormat.order))
		imageAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;

	if (tcu::hasStencilComponent(textureFormat.order))
		imageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	if (imageAspectFlags == 0)
		imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

	return imageAspectFlags;
}

VkExtent3D mipLevelExtents (const VkExtent3D& baseExtents, const deUint32 mipLevel)
{
	VkExtent3D result;

	result.width	= std::max(baseExtents.width >> mipLevel, 1u);
	result.height	= std::max(baseExtents.height >> mipLevel, 1u);
	result.depth	= std::max(baseExtents.depth >> mipLevel, 1u);

	return result;
}

tcu::UVec3 alignedDivide (const VkExtent3D& extent, const VkExtent3D& divisor)
{
	tcu::UVec3 result;

	result.x() = extent.width  / divisor.width  + ((extent.width  % divisor.width != 0)  ? 1u : 0u);
	result.y() = extent.height / divisor.height + ((extent.height % divisor.height != 0) ? 1u : 0u);
	result.z() = extent.depth  / divisor.depth  + ((extent.depth  % divisor.depth != 0)  ? 1u : 0u);

	return result;
}

void copyBufferToImage (const DeviceInterface&					vk,
						const VkCommandBuffer&					cmdBuffer,
						const VkBuffer&							buffer,
						VkDeviceSize							bufferSize,
						const std::vector<VkBufferImageCopy>&	copyRegions,
						VkImageAspectFlags						imageAspectFlags,
						deUint32								mipLevels,
						deUint32								arrayLayers,
						VkImage									destImage,
						VkImageLayout							destImageLayout,
						VkPipelineStageFlags					destImageDstStageFlags)
{
	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier preBufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,					// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		buffer,										// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		bufferSize									// VkDeviceSize		size;
	};

	const VkImageMemoryBarrier preImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		0u,												// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			imageAspectFlags,							// VkImageAspectFlags		aspect;
			0u,											// deUint32					baseMipLevel;
			mipLevels,									// deUint32					mipLevels;
			0u,											// deUint32					baseArraySlice;
			arrayLayers									// deUint32					arraySize;
		}
	};

	const VkImageMemoryBarrier postImageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		destImageLayout,								// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex;
		destImage,										// VkImage					image;
		{												// VkImageSubresourceRange	subresourceRange;
			imageAspectFlags,							// VkImageAspectFlags		aspect;
			0u,											// deUint32					baseMipLevel;
			mipLevels,									// deUint32					mipLevels;
			0u,											// deUint32					baseArraySlice;
			arrayLayers									// deUint32					arraySize;
		}
	};

	// Copy buffer to image
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 1, &preImageBarrier);
	vk.cmdCopyBufferToImage(cmdBuffer, buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegions.size(), copyRegions.data());
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destImageDstStageFlags, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
}

void copyBufferToImage (const DeviceInterface&					vk,
						VkDevice								device,
						VkQueue									queue,
						deUint32								queueFamilyIndex,
						const VkBuffer&							buffer,
						VkDeviceSize							bufferSize,
						const std::vector<VkBufferImageCopy>&	copyRegions,
						const VkSemaphore*						waitSemaphore,
						VkImageAspectFlags						imageAspectFlags,
						deUint32								mipLevels,
						deUint32								arrayLayers,
						VkImage									destImage,
						VkImageLayout							destImageLayout,
						VkPipelineStageFlags					destImageDstStageFlags)
{
	Move<VkCommandPool>		cmdPool		= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	cmdBuffer	= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkFence>			fence		= createFence(vk, device);

	const VkCommandBufferBeginInfo cmdBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags		flags;
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
	copyBufferToImage(vk, *cmdBuffer, buffer, bufferSize, copyRegions, imageAspectFlags, mipLevels, arrayLayers, destImage, destImageLayout, destImageDstStageFlags);
	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		DE_NULL,						// const void*					pNext;
		waitSemaphore ? 1u : 0u,		// deUint32						waitSemaphoreCount;
		waitSemaphore,					// const VkSemaphore*			pWaitSemaphores;
		&pipelineStageFlags,			// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// deUint32						commandBufferCount;
		&cmdBuffer.get(),				// const VkCommandBuffer*		pCommandBuffers;
		0u,								// deUint32						signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*			pSignalSemaphores;
	};

	try
	{
		VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
		VK_CHECK(vk.waitForFences(device, 1, &fence.get(), true, ~(0ull) /* infinity */));
	}
	catch (...)
	{
		VK_CHECK(vk.deviceWaitIdle(device));
		throw;
	}
}

void copyImageToBuffer (const DeviceInterface&	vk,
						VkCommandBuffer			cmdBuffer,
						VkImage					image,
						VkBuffer				buffer,
						tcu::IVec2				size,
						VkAccessFlags			srcAccessMask,
						VkImageLayout			oldLayout,
						deUint32				numLayers,
						VkImageAspectFlags		barrierAspect,
						VkImageAspectFlags		copyAspect)
{
	const VkImageMemoryBarrier	imageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,							// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		srcAccessMask,													// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,									// VkAccessFlags			dstAccessMask;
		oldLayout,														// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,							// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,										// deUint32					destQueueFamilyIndex;
		image,															// VkImage					image;
		makeImageSubresourceRange(barrierAspect, 0u, 1u, 0, numLayers)	// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
						  0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

	const VkImageSubresourceLayers	subresource	=
	{
		copyAspect,									// VkImageAspectFlags	aspectMask;
		0u,											// deUint32				mipLevel;
		0u,											// deUint32				baseArrayLayer;
		numLayers									// deUint32				layerCount;
	};

	const VkBufferImageCopy			region		=
	{
		0ull,										// VkDeviceSize					bufferOffset;
		0u,											// deUint32						bufferRowLength;
		0u,											// deUint32						bufferImageHeight;
		subresource,								// VkImageSubresourceLayers		imageSubresource;
		makeOffset3D(0, 0, 0),						// VkOffset3D					imageOffset;
		makeExtent3D(size.x(), size.y(), 1u)		// VkExtent3D					imageExtent;
	};

	vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1u, &region);

	const VkBufferMemoryBarrier	bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		buffer,										// VkBuffer			buffer;
		0ull,										// VkDeviceSize		offset;
		VK_WHOLE_SIZE								// VkDeviceSize		size;
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
						  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
}

void clearColorImage (const DeviceInterface&	vk,
					  const VkDevice			device,
					  const VkQueue				queue,
					  deUint32					queueFamilyIndex,
					  VkImage					image,
					  tcu::Vec4					clearColor,
					  VkImageLayout				oldLayout,
					  VkImageLayout				newLayout,
					  VkPipelineStageFlags		dstStageFlags)
{
	Move<VkCommandPool>				cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkClearColorValue			clearColorValue		= makeClearValueColor(clearColor).color;

	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,							// deUint32				baseMipLevel
		1u,							// deUint32				levelCount
		0u,							// deUint32				baseArrayLayer
		1u,							// deUint32				layerCount
	};

	const VkImageMemoryBarrier		preImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	const VkImageMemoryBarrier		postImageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_HOST_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &preImageBarrier);
	vk.cmdClearColorImage(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  dstStageFlags,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &postImageBarrier);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

std::vector<VkBufferImageCopy> generateChessboardCopyRegions (deUint32				tileSize,
															  deUint32				imageWidth,
															  deUint32				imageHeight,
															  deUint32				tileIdx,
															  VkImageAspectFlags	aspectMask)
{
	std::vector<VkBufferImageCopy>	copyRegions;

	for (deUint32 x = 0; x < (deUint32)deFloatCeil((float)imageWidth / (float)tileSize); x++)
		for (deUint32 y = 0; y < (deUint32)deFloatCeil((float)imageHeight / (float)tileSize); y++)
		{
			if ((x + tileIdx) % 2 == y % 2) continue;

			const deUint32					tileWidth			= de::min(tileSize, imageWidth - tileSize * x);
			const deUint32					tileHeight			= de::min(tileSize, imageHeight - tileSize * y);

			const VkOffset3D				offset				=
			{
				(deInt32)x * (deInt32)tileWidth,	// deInt32	x
				(deInt32)y * (deInt32)tileHeight,	// deInt32	y
				0									// deInt32	z
			};

			const VkExtent3D				extent				=
			{
				tileWidth,	// deUint32	width
				tileHeight,	// deUint32	height
				1u			// deUint32	depth
			};

			const VkImageSubresourceLayers	subresourceLayers	=
			{
				aspectMask,	// VkImageAspectFlags	aspectMask
				0u,			// deUint32				mipLevel
				0u,			// deUint32				baseArrayLayer
				1u,			// deUint32				layerCount
			};

			const VkBufferImageCopy			copy				=
			{
				(VkDeviceSize)0,	// VkDeviceSize				bufferOffset
				0u,					// deUint32					bufferRowLength
				0u,					// deUint32					bufferImageHeight
				subresourceLayers,	// VkImageSubresourceLayers	imageSubresource
				offset,				// VkOffset3D				imageOffset
				extent				// VkExtent3D				imageExtent
			};

			copyRegions.push_back(copy);
		}

	return copyRegions;
}

void initColorImageChessboardPattern (const DeviceInterface&	vk,
									  const VkDevice			device,
									  const VkQueue				queue,
									  deUint32					queueFamilyIndex,
									  Allocator&				allocator,
									  VkImage					image,
									  VkFormat					format,
									  tcu::Vec4					colorValue0,
									  tcu::Vec4					colorValue1,
									  deUint32					imageWidth,
									  deUint32					imageHeight,
									  deUint32					tileSize,
									  VkImageLayout				oldLayout,
									  VkImageLayout				newLayout,
									  VkPipelineStageFlags		dstStageFlags)
{
	Move<VkCommandPool>				cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const tcu::TextureFormat		tcuFormat			= mapVkFormat(format);
	const tcu::Vec4					colorValues[]		= { colorValue0, colorValue1 };
	const deUint32					bufferSize			= tileSize * tileSize * tcuFormat.getPixelSize();

	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask
		0u,							// deUint32				baseMipLevel
		1u,							// deUint32				levelCount
		0u,							// deUint32				baseArrayLayer
		1u							// deUint32				layerCount
	};

	const VkImageMemoryBarrier		preImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	const VkImageMemoryBarrier		postImageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	// Create staging buffers for both color values
	Move<VkBuffer>					buffers[2];
	de::MovePtr<Allocation>			bufferAllocs[2];

	const VkBufferCreateInfo		bufferParams		=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,								// const void*			pNext
		0u,										// VkBufferCreateFlags	flags
		(VkDeviceSize)bufferSize,				// VkDeviceSize			size
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0u,										// deUint32				queueFamilyIndexCount
		DE_NULL									// const deUint32*		pQueueFamilyIndices
	};

	for (deUint32 bufferIdx = 0; bufferIdx < 2; bufferIdx++)
	{
		buffers[bufferIdx]		= createBuffer(vk, device, &bufferParams);
		bufferAllocs[bufferIdx]	= allocator.allocate(getBufferMemoryRequirements(vk, device, *buffers[bufferIdx]), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *buffers[bufferIdx], bufferAllocs[bufferIdx]->getMemory(), bufferAllocs[bufferIdx]->getOffset()));

		deUint32*				dstPtr	= (deUint32*)bufferAllocs[bufferIdx]->getHostPtr();
		tcu::PixelBufferAccess	access	(tcuFormat, tileSize, tileSize, 1, dstPtr);

		for (deUint32 x = 0; x < tileSize; x++)
			for (deUint32 y = 0; y < tileSize; y++)
				access.setPixel(colorValues[bufferIdx], x, y, 0);

		flushAlloc(vk, device, *bufferAllocs[bufferIdx]);
	}

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_HOST_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &preImageBarrier);

	for (deUint32 bufferIdx = 0; bufferIdx < 2; bufferIdx++)
	{
		std::vector<VkBufferImageCopy> copyRegions = generateChessboardCopyRegions(tileSize, imageWidth, imageHeight, bufferIdx, VK_IMAGE_ASPECT_COLOR_BIT);

		vk.cmdCopyBufferToImage(*cmdBuffer, *buffers[bufferIdx], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegions.size(), copyRegions.data());
	}

	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  dstStageFlags,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &postImageBarrier);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

void copyDepthStencilImageToBuffers (const DeviceInterface&	vk,
									 VkCommandBuffer		cmdBuffer,
									 VkImage				image,
									 VkBuffer				depthBuffer,
									 VkBuffer				stencilBuffer,
									 tcu::IVec2				size,
									 VkAccessFlags			srcAccessMask,
									 VkImageLayout			oldLayout,
									 deUint32				numLayers)
{
	const VkImageAspectFlags		aspect				= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkImageMemoryBarrier		imageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		srcAccessMask,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,							// VkAccessFlags			dstAccessMask;
		oldLayout,												// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,					// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,								// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,								// deUint32					destQueueFamilyIndex;
		image,													// VkImage					image;
		makeImageSubresourceRange(aspect, 0u, 1u, 0, numLayers)	// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
						  0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

	const VkImageSubresourceLayers	subresourceDepth	=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT,					// VkImageAspectFlags	aspectMask;
		0u,											// deUint32				mipLevel;
		0u,											// deUint32				baseArrayLayer;
		numLayers									// deUint32				layerCount;
	};

	const VkBufferImageCopy			regionDepth			=
	{
		0ull,										// VkDeviceSize					bufferOffset;
		0u,											// deUint32						bufferRowLength;
		0u,											// deUint32						bufferImageHeight;
		subresourceDepth,							// VkImageSubresourceLayers		imageSubresource;
		makeOffset3D(0, 0, 0),						// VkOffset3D					imageOffset;
		makeExtent3D(size.x(), size.y(), 1u)		// VkExtent3D					imageExtent;
	};

	const VkImageSubresourceLayers	subresourceStencil	=
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,				// VkImageAspectFlags	aspectMask;
		0u,											// deUint32				mipLevel;
		0u,											// deUint32				baseArrayLayer;
		numLayers									// deUint32				layerCount;
	};

	const VkBufferImageCopy			regionStencil		=
	{
		0ull,										// VkDeviceSize					bufferOffset;
		0u,											// deUint32						bufferRowLength;
		0u,											// deUint32						bufferImageHeight;
		subresourceStencil,							// VkImageSubresourceLayers		imageSubresource;
		makeOffset3D(0, 0, 0),						// VkOffset3D					imageOffset;
		makeExtent3D(size.x(), size.y(), 1u)		// VkExtent3D					imageExtent;
	};

	vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthBuffer, 1u, &regionDepth);
	vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stencilBuffer, 1u, &regionStencil);

	const VkBufferMemoryBarrier	bufferBarriers[]		=
	{
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
			DE_NULL,									// const void*		pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
			depthBuffer,								// VkBuffer			buffer;
			0ull,										// VkDeviceSize		offset;
			VK_WHOLE_SIZE								// VkDeviceSize		size;
		},
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
			DE_NULL,									// const void*		pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
			stencilBuffer,								// VkBuffer			buffer;
			0ull,										// VkDeviceSize		offset;
			VK_WHOLE_SIZE								// VkDeviceSize		size;
		}
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
						  0u, DE_NULL, 2u, bufferBarriers, 0u, DE_NULL);
}

void clearDepthStencilImage (const DeviceInterface&	vk,
							 const VkDevice			device,
							 const VkQueue			queue,
							 deUint32				queueFamilyIndex,
							 VkImage				image,
							 float					depthValue,
							 deUint32				stencilValue,
							 VkImageLayout			oldLayout,
							 VkImageLayout			newLayout,
							 VkPipelineStageFlags	dstStageFlags)
{
	Move<VkCommandPool>				cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkClearDepthStencilValue	clearValue			= makeClearValueDepthStencil(depthValue, stencilValue).depthStencil;

	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask
		0u,															// deUint32				baseMipLevel
		1u,															// deUint32				levelCount
		0u,															// deUint32				baseArrayLayer
		1u															// deUint32				layerCount
	};

	const VkImageMemoryBarrier		preImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	const VkImageMemoryBarrier		postImageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_HOST_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &preImageBarrier);
	vk.cmdClearDepthStencilImage(*cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &subresourceRange);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  dstStageFlags,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &postImageBarrier);
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

void initDepthStencilImageChessboardPattern (const DeviceInterface&	vk,
											 const VkDevice			device,
											 const VkQueue			queue,
											 deUint32				queueFamilyIndex,
											 Allocator&				allocator,
											 VkImage				image,
											 VkFormat				format,
											 float					depthValue0,
											 float					depthValue1,
											 deUint32				stencilValue0,
											 deUint32				stencilValue1,
											 deUint32				imageWidth,
											 deUint32				imageHeight,
											 deUint32				tileSize,
											 VkImageLayout			oldLayout,
											 VkImageLayout			newLayout,
											 VkPipelineStageFlags	dstStageFlags)
{
	Move<VkCommandPool>				cmdPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const deUint32					depthBufferSize		= tileSize * tileSize * 4;
	const deUint32					stencilBufferSize	= tileSize * tileSize;
	const float						depthValues[]		= { depthValue0, depthValue1 };
	const deUint32					stencilValues[]		= { stencilValue0, stencilValue1 };
	const tcu::TextureFormat		tcuFormat			= mapVkFormat(format);

	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,	// VkImageAspectFlags	aspectMask
		0u,															// deUint32				baseMipLevel
		1u,															// deUint32				levelCount
		0u,															// deUint32				baseArrayLayer
		1u															// deUint32				layerCount
	};

	const VkImageMemoryBarrier		preImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	const VkImageMemoryBarrier		postImageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		queueFamilyIndex,							// deUint32					srcQueueFamilyIndex;
		queueFamilyIndex,							// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	// Create staging buffers for depth and stencil values
	Move<VkBuffer>					depthBuffers[2];
	de::MovePtr<Allocation>			depthBufferAllocs[2];
	Move<VkBuffer>					stencilBuffers[2];
	de::MovePtr<Allocation>			stencilBufferAllocs[2];

	const VkBufferCreateInfo		depthBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,								// const void*			pNext
		0u,										// VkBufferCreateFlags	flags
		(VkDeviceSize)depthBufferSize,			// VkDeviceSize			size
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0u,										// deUint32				queueFamilyIndexCount
		DE_NULL									// const deUint32*		pQueueFamilyIndices
	};

	const VkBufferCreateInfo		stencilBufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,								// const void*			pNext
		0u,										// VkBufferCreateFlags	flags
		(VkDeviceSize)stencilBufferSize,		// VkDeviceSize			size
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,		// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0u,										// deUint32				queueFamilyIndexCount
		DE_NULL									// const deUint32*		pQueueFamilyIndices
	};

	for (deUint32 bufferIdx = 0; bufferIdx < 2; bufferIdx++)
	{
		depthBuffers[bufferIdx]			= createBuffer(vk, device, &depthBufferParams);
		depthBufferAllocs[bufferIdx]	= allocator.allocate(getBufferMemoryRequirements(vk, device, *depthBuffers[bufferIdx]), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *depthBuffers[bufferIdx], depthBufferAllocs[bufferIdx]->getMemory(), depthBufferAllocs[bufferIdx]->getOffset()));
		stencilBuffers[bufferIdx]		= createBuffer(vk, device, &stencilBufferParams);
		stencilBufferAllocs[bufferIdx]	= allocator.allocate(getBufferMemoryRequirements(vk, device, *stencilBuffers[bufferIdx]), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(device, *stencilBuffers[bufferIdx], stencilBufferAllocs[bufferIdx]->getMemory(), stencilBufferAllocs[bufferIdx]->getOffset()));

		deUint32*	depthPtr	= (deUint32*)depthBufferAllocs[bufferIdx]->getHostPtr();
		deUint32*	stencilPtr	= (deUint32*)stencilBufferAllocs[bufferIdx]->getHostPtr();

		if (format == VK_FORMAT_D24_UNORM_S8_UINT)
		{
			tcu::PixelBufferAccess access(tcuFormat, tileSize, tileSize, 1, depthPtr);

			for (deUint32 x = 0; x < tileSize; x++)
				for (deUint32 y = 0; y < tileSize; y++)
					access.setPixDepth(depthValues[bufferIdx], x, y, 0);
		}
		else
		{
			DE_ASSERT(format == VK_FORMAT_D32_SFLOAT_S8_UINT);

			for (deUint32 i = 0; i < tileSize * tileSize; i++)
				((float*)depthPtr)[i] = depthValues[bufferIdx];
		}

		deMemset(stencilPtr, stencilValues[bufferIdx], stencilBufferSize);
		flushAlloc(vk, device, *depthBufferAllocs[bufferIdx]);
		flushAlloc(vk, device, *stencilBufferAllocs[bufferIdx]);
	}

	beginCommandBuffer(vk, *cmdBuffer);
	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_HOST_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &preImageBarrier);

	for (deUint32 bufferIdx = 0; bufferIdx < 2; bufferIdx++)
	{
		std::vector<VkBufferImageCopy>	copyRegionsDepth	= generateChessboardCopyRegions(tileSize, imageWidth, imageHeight, bufferIdx, VK_IMAGE_ASPECT_DEPTH_BIT);
		std::vector<VkBufferImageCopy>	copyRegionsStencil	= generateChessboardCopyRegions(tileSize, imageWidth, imageHeight, bufferIdx, VK_IMAGE_ASPECT_STENCIL_BIT);

		vk.cmdCopyBufferToImage(*cmdBuffer, *depthBuffers[bufferIdx], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegionsDepth.size(), copyRegionsDepth.data());
		vk.cmdCopyBufferToImage(*cmdBuffer, *stencilBuffers[bufferIdx], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (deUint32)copyRegionsStencil.size(), copyRegionsStencil.data());
	}

	vk.cmdPipelineBarrier(*cmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  dstStageFlags,
						  (VkDependencyFlags)0,
						  0, (const VkMemoryBarrier*)DE_NULL,
						  0, (const VkBufferMemoryBarrier*)DE_NULL,
						  1, &postImageBarrier);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);
}

void allocateAndBindSparseImage (const DeviceInterface&						vk,
								 VkDevice									device,
								 const VkPhysicalDevice						physicalDevice,
								 const InstanceInterface&					instance,
								 const VkImageCreateInfo&					imageCreateInfo,
								 const VkSemaphore&							signalSemaphore,
								 VkQueue									queue,
								 Allocator&									allocator,
								 std::vector<de::SharedPtr<Allocation> >&	allocations,
								 tcu::TextureFormat							format,
								 VkImage									destImage)
{
	const VkImageAspectFlags				imageAspectFlags		= getImageAspectFlags(format);
	const VkPhysicalDeviceProperties		deviceProperties		= getPhysicalDeviceProperties(instance, physicalDevice);
	const VkPhysicalDeviceMemoryProperties	deviceMemoryProperties	= getPhysicalDeviceMemoryProperties(instance, physicalDevice);
	deUint32								sparseMemoryReqCount	= 0;

	// Check if the image format supports sparse operations
	if (!checkSparseImageFormatSupport(physicalDevice, instance, imageCreateInfo))
		TCU_THROW(NotSupportedError, "The image format does not support sparse operations.");

	vk.getImageSparseMemoryRequirements(device, destImage, &sparseMemoryReqCount, DE_NULL);

	DE_ASSERT(sparseMemoryReqCount != 0);

	std::vector<VkSparseImageMemoryRequirements> sparseImageMemoryRequirements;
	sparseImageMemoryRequirements.resize(sparseMemoryReqCount);

	vk.getImageSparseMemoryRequirements(device, destImage, &sparseMemoryReqCount, &sparseImageMemoryRequirements[0]);

	const deUint32 noMatchFound = ~((deUint32)0);

	deUint32 aspectIndex = noMatchFound;
	for (deUint32 memoryReqNdx = 0; memoryReqNdx < sparseMemoryReqCount; ++memoryReqNdx)
	{
		if (sparseImageMemoryRequirements[memoryReqNdx].formatProperties.aspectMask == imageAspectFlags)
		{
			aspectIndex = memoryReqNdx;
			break;
		}
	}

	deUint32 metadataAspectIndex = noMatchFound;
	for (deUint32 memoryReqNdx = 0; memoryReqNdx < sparseMemoryReqCount; ++memoryReqNdx)
	{
		if (sparseImageMemoryRequirements[memoryReqNdx].formatProperties.aspectMask & VK_IMAGE_ASPECT_METADATA_BIT)
		{
			metadataAspectIndex = memoryReqNdx;
			break;
		}
	}

	if (aspectIndex == noMatchFound)
		TCU_THROW(NotSupportedError, "Required image aspect not supported.");

	const VkMemoryRequirements	memoryRequirements	= getImageMemoryRequirements(vk, device, destImage);

	deUint32 memoryType = noMatchFound;
	for (deUint32 memoryTypeNdx = 0; memoryTypeNdx < deviceMemoryProperties.memoryTypeCount; ++memoryTypeNdx)
	{
		if ((memoryRequirements.memoryTypeBits & (1u << memoryTypeNdx)) != 0 &&
			MemoryRequirement::Any.matchesHeap(deviceMemoryProperties.memoryTypes[memoryTypeNdx].propertyFlags))
		{
			memoryType = memoryTypeNdx;
			break;
		}
	}

	if (memoryType == noMatchFound)
		TCU_THROW(NotSupportedError, "No matching memory type found.");

	if (memoryRequirements.size > deviceProperties.limits.sparseAddressSpaceSize)
		TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits.");

	const VkSparseImageMemoryRequirements		aspectRequirements	= sparseImageMemoryRequirements[aspectIndex];
	VkExtent3D									blockSize			= aspectRequirements.formatProperties.imageGranularity;

	std::vector<VkSparseImageMemoryBind>		imageResidencyMemoryBinds;
	std::vector<VkSparseMemoryBind>				imageMipTailMemoryBinds;

	for (deUint32 layerNdx = 0; layerNdx < imageCreateInfo.arrayLayers; ++layerNdx)
	{
		for (deUint32 mipLevelNdx = 0; mipLevelNdx < aspectRequirements.imageMipTailFirstLod; ++mipLevelNdx)
		{
			const VkExtent3D	mipExtent		= mipLevelExtents(imageCreateInfo.extent, mipLevelNdx);
			const tcu::UVec3	numSparseBinds	= alignedDivide(mipExtent, blockSize);
			const tcu::UVec3	lastBlockExtent	= tcu::UVec3(mipExtent.width  % blockSize.width  ? mipExtent.width  % blockSize.width  : blockSize.width,
															 mipExtent.height % blockSize.height ? mipExtent.height % blockSize.height : blockSize.height,
															 mipExtent.depth  % blockSize.depth  ? mipExtent.depth  % blockSize.depth  : blockSize.depth );

			for (deUint32 z = 0; z < numSparseBinds.z(); ++z)
			for (deUint32 y = 0; y < numSparseBinds.y(); ++y)
			for (deUint32 x = 0; x < numSparseBinds.x(); ++x)
			{
				const VkMemoryRequirements allocRequirements =
				{
					// 28.7.5 alignment shows the block size in bytes
					memoryRequirements.alignment,		// VkDeviceSize	size;
					memoryRequirements.alignment,		// VkDeviceSize	alignment;
					memoryRequirements.memoryTypeBits,	// uint32_t		memoryTypeBits;
				};

				de::SharedPtr<Allocation> allocation(allocator.allocate(allocRequirements, MemoryRequirement::Any).release());
				allocations.push_back(allocation);

				VkOffset3D offset;
				offset.x = x*blockSize.width;
				offset.y = y*blockSize.height;
				offset.z = z*blockSize.depth;

				VkExtent3D extent;
				extent.width	= (x == numSparseBinds.x() - 1) ? lastBlockExtent.x() : blockSize.width;
				extent.height	= (y == numSparseBinds.y() - 1) ? lastBlockExtent.y() : blockSize.height;
				extent.depth	= (z == numSparseBinds.z() - 1) ? lastBlockExtent.z() : blockSize.depth;

				const VkSparseImageMemoryBind imageMemoryBind =
				{
					{
						imageAspectFlags,	// VkImageAspectFlags	aspectMask;
						mipLevelNdx,		// uint32_t				mipLevel;
						layerNdx,			// uint32_t				arrayLayer;
					},							// VkImageSubresource		subresource;
					offset,						// VkOffset3D				offset;
					extent,						// VkExtent3D				extent;
					allocation->getMemory(),	// VkDeviceMemory			memory;
					allocation->getOffset(),	// VkDeviceSize				memoryOffset;
					0u,							// VkSparseMemoryBindFlags	flags;
				};

				imageResidencyMemoryBinds.push_back(imageMemoryBind);
			}
		}

		// Handle MIP tail. There are two cases to consider here:
		//
		// 1) VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT is requested by the driver: each layer needs a separate tail.
		// 2) otherwise:                                                            only one tail is needed.
		if (aspectRequirements.imageMipTailSize > 0)
		{
			if (layerNdx == 0 || (aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == 0)
			{
				const VkMemoryRequirements allocRequirements =
				{
					aspectRequirements.imageMipTailSize,	// VkDeviceSize	size;
					memoryRequirements.alignment,			// VkDeviceSize	alignment;
					memoryRequirements.memoryTypeBits,		// uint32_t		memoryTypeBits;
				};

				const de::SharedPtr<Allocation> allocation(allocator.allocate(allocRequirements, MemoryRequirement::Any).release());

				const VkSparseMemoryBind imageMipTailMemoryBind =
				{
					aspectRequirements.imageMipTailOffset + layerNdx * aspectRequirements.imageMipTailStride,	// VkDeviceSize					resourceOffset;
					aspectRequirements.imageMipTailSize,														// VkDeviceSize					size;
					allocation->getMemory(),																	// VkDeviceMemory				memory;
					allocation->getOffset(),																	// VkDeviceSize					memoryOffset;
					0u,																							// VkSparseMemoryBindFlags		flags;
				};

				allocations.push_back(allocation);

				imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
			}
		}

		// Handle Metadata. Similarly to MIP tail in aspectRequirements, there are two cases to consider here:
		//
		// 1) VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT is requested by the driver: each layer needs a separate tail.
		// 2) otherwise:
		if (metadataAspectIndex != noMatchFound)
		{
			const VkSparseImageMemoryRequirements	metadataAspectRequirements = sparseImageMemoryRequirements[metadataAspectIndex];

			if (layerNdx == 0 || (metadataAspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == 0)
			{
				const VkMemoryRequirements metadataAllocRequirements =
				{
					metadataAspectRequirements.imageMipTailSize,	// VkDeviceSize	size;
					memoryRequirements.alignment,					// VkDeviceSize	alignment;
					memoryRequirements.memoryTypeBits,				// uint32_t		memoryTypeBits;
				};
				const de::SharedPtr<Allocation>	metadataAllocation(allocator.allocate(metadataAllocRequirements, MemoryRequirement::Any).release());

				const VkSparseMemoryBind metadataMipTailMemoryBind =
				{
					metadataAspectRequirements.imageMipTailOffset +
					layerNdx * metadataAspectRequirements.imageMipTailStride,			// VkDeviceSize					resourceOffset;
					metadataAspectRequirements.imageMipTailSize,						// VkDeviceSize					size;
					metadataAllocation->getMemory(),									// VkDeviceMemory				memory;
					metadataAllocation->getOffset(),									// VkDeviceSize					memoryOffset;
					VK_SPARSE_MEMORY_BIND_METADATA_BIT									// VkSparseMemoryBindFlags		flags;
				};

				allocations.push_back(metadataAllocation);

				imageMipTailMemoryBinds.push_back(metadataMipTailMemoryBind);
			}
		}
	}

	VkBindSparseInfo bindSparseInfo =
	{
		VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,			//VkStructureType							sType;
		DE_NULL,									//const void*								pNext;
		0u,											//deUint32									waitSemaphoreCount;
		DE_NULL,									//const VkSemaphore*						pWaitSemaphores;
		0u,											//deUint32									bufferBindCount;
		DE_NULL,									//const VkSparseBufferMemoryBindInfo*		pBufferBinds;
		0u,											//deUint32									imageOpaqueBindCount;
		DE_NULL,									//const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
		0u,											//deUint32									imageBindCount;
		DE_NULL,									//const VkSparseImageMemoryBindInfo*		pImageBinds;
		1u,											//deUint32									signalSemaphoreCount;
		&signalSemaphore							//const VkSemaphore*						pSignalSemaphores;
	};

	VkSparseImageMemoryBindInfo			imageResidencyBindInfo;
	VkSparseImageOpaqueMemoryBindInfo	imageMipTailBindInfo;

	if (imageResidencyMemoryBinds.size() > 0)
	{
		imageResidencyBindInfo.image		= destImage;
		imageResidencyBindInfo.bindCount	= static_cast<deUint32>(imageResidencyMemoryBinds.size());
		imageResidencyBindInfo.pBinds		= &imageResidencyMemoryBinds[0];

		bindSparseInfo.imageBindCount		= 1u;
		bindSparseInfo.pImageBinds			= &imageResidencyBindInfo;
	}

	if (imageMipTailMemoryBinds.size() > 0)
	{
		imageMipTailBindInfo.image			= destImage;
		imageMipTailBindInfo.bindCount		= static_cast<deUint32>(imageMipTailMemoryBinds.size());
		imageMipTailBindInfo.pBinds			= &imageMipTailMemoryBinds[0];

		bindSparseInfo.imageOpaqueBindCount	= 1u;
		bindSparseInfo.pImageOpaqueBinds	= &imageMipTailBindInfo;
	}

	VK_CHECK(vk.queueBindSparse(queue, 1u, &bindSparseInfo, DE_NULL));
}

bool checkSparseImageFormatSupport (const VkPhysicalDevice		physicalDevice,
									const InstanceInterface&	instance,
									const VkFormat				format,
									const VkImageType			imageType,
									const VkSampleCountFlagBits	sampleCount,
									const VkImageUsageFlags		usageFlags,
									const VkImageTiling			imageTiling)
{
	const auto propVec = getPhysicalDeviceSparseImageFormatProperties(instance, physicalDevice, format, imageType, sampleCount, usageFlags, imageTiling);
	return (propVec.size() != 0);
}

bool checkSparseImageFormatSupport (const VkPhysicalDevice		physicalDevice,
									const InstanceInterface&	instance,
									const VkImageCreateInfo&	imageCreateInfo)
{
	return checkSparseImageFormatSupport(physicalDevice, instance, imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.samples, imageCreateInfo.usage, imageCreateInfo.tiling);
}

} // vk
