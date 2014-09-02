#ifndef _TCUCOMPRESSEDTEXTURE_HPP
#define _TCUCOMPRESSEDTEXTURE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief Compressed Texture Utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"

#include <vector>

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Compressed texture
 *
 * This class implements container for common compressed texture formats.
 * Reference decoding to uncompressed formats is supported.
 *//*--------------------------------------------------------------------*/
class CompressedTexture
{
public:
	enum Format
	{
		ETC1_RGB8 = 0,
		EAC_R11,
		EAC_SIGNED_R11,
		EAC_RG11,
		EAC_SIGNED_RG11,
		ETC2_RGB8,
		ETC2_SRGB8,
		ETC2_RGB8_PUNCHTHROUGH_ALPHA1,
		ETC2_SRGB8_PUNCHTHROUGH_ALPHA1,
		ETC2_EAC_RGBA8,
		ETC2_EAC_SRGB8_ALPHA8,

		ASTC_4x4_RGBA,
		ASTC_5x4_RGBA,
		ASTC_5x5_RGBA,
		ASTC_6x5_RGBA,
		ASTC_6x6_RGBA,
		ASTC_8x5_RGBA,
		ASTC_8x6_RGBA,
		ASTC_8x8_RGBA,
		ASTC_10x5_RGBA,
		ASTC_10x6_RGBA,
		ASTC_10x8_RGBA,
		ASTC_10x10_RGBA,
		ASTC_12x10_RGBA,
		ASTC_12x12_RGBA,
		ASTC_4x4_SRGB8_ALPHA8,
		ASTC_5x4_SRGB8_ALPHA8,
		ASTC_5x5_SRGB8_ALPHA8,
		ASTC_6x5_SRGB8_ALPHA8,
		ASTC_6x6_SRGB8_ALPHA8,
		ASTC_8x5_SRGB8_ALPHA8,
		ASTC_8x6_SRGB8_ALPHA8,
		ASTC_8x8_SRGB8_ALPHA8,
		ASTC_10x5_SRGB8_ALPHA8,
		ASTC_10x6_SRGB8_ALPHA8,
		ASTC_10x8_SRGB8_ALPHA8,
		ASTC_10x10_SRGB8_ALPHA8,
		ASTC_12x10_SRGB8_ALPHA8,
		ASTC_12x12_SRGB8_ALPHA8,

		FORMAT_LAST
	};

	struct DecompressionParams
	{
		bool isASTCModeLDR; //!< \note Ignored if not ASTC format.

		DecompressionParams (bool isASTCModeLDR_) : isASTCModeLDR(isASTCModeLDR_) {}
	};


							CompressedTexture			(Format format, int width, int height, int depth = 1);
							CompressedTexture			(void);
							~CompressedTexture			(void);

	void					setStorage					(Format format, int width, int height, int depth = 1);

	int						getWidth					(void) const	{ return m_width;				}
	int						getHeight					(void) const	{ return m_height;				}
	Format					getFormat					(void) const	{ return m_format;				}
	int						getDataSize					(void) const	{ return (int)m_data.size();	}
	const void*				getData						(void) const	{ return &m_data[0];			}
	void*					getData						(void)			{ return &m_data[0];			}

	TextureFormat			getUncompressedFormat		(void) const;
	void					decompress					(const PixelBufferAccess& dst, const DecompressionParams& params = DecompressionParams(false)) const;

private:
	Format					m_format;
	int						m_width;
	int						m_height;
	int						m_depth;
	std::vector<deUint8>	m_data;
};

bool						isEtcFormat					(CompressedTexture::Format fmt);
bool						isASTCFormat				(CompressedTexture::Format fmt);
bool						isASTCSRGBFormat			(CompressedTexture::Format fmt);

IVec3						getASTCBlockSize			(CompressedTexture::Format fmt);
CompressedTexture::Format	getASTCFormatByBlockSize	(int width, int height, int depth, bool isSRGB);

} // tcu

#endif // _TCUCOMPRESSEDTEXTURE_HPP
