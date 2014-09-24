#ifndef _TCUTEXTUREUTIL_HPP
#define _TCUTEXTUREUTIL_HPP
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
 * \brief Texture utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"

namespace tcu
{

// PixelBufferAccess utilities.
PixelBufferAccess		getSubregion	(const PixelBufferAccess& access, int x, int y, int z, int width, int height, int depth);
ConstPixelBufferAccess	getSubregion	(const ConstPixelBufferAccess& access, int x, int y, int z, int width, int height, int depth);

PixelBufferAccess		getSubregion	(const PixelBufferAccess& access, int x, int y, int width, int height);
ConstPixelBufferAccess	getSubregion	(const ConstPixelBufferAccess& access, int x, int y, int width, int height);

PixelBufferAccess		flipYAccess		(const PixelBufferAccess& access);
ConstPixelBufferAccess	flipYAccess		(const ConstPixelBufferAccess& access);

// sRGB - linear conversion.
Vec4					sRGBToLinear	(const Vec4& cs);
Vec4					linearToSRGB	(const Vec4& cl);

/*--------------------------------------------------------------------*//*!
 * \brief Color channel storage type
 *//*--------------------------------------------------------------------*/
enum TextureChannelClass
{
	TEXTURECHANNELCLASS_SIGNED_FIXED_POINT = 0,
	TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT,
	TEXTURECHANNELCLASS_SIGNED_INTEGER,
	TEXTURECHANNELCLASS_UNSIGNED_INTEGER,
	TEXTURECHANNELCLASS_FLOATING_POINT,

	TEXTURECHANNELCLASS_LAST
};

TextureChannelClass getTextureChannelClass (TextureFormat::ChannelType channelType);

/*--------------------------------------------------------------------*//*!
 * \brief Standard parameters for texture format testing
 *//*--------------------------------------------------------------------*/
struct TextureFormatInfo
{
	Vec4	valueMin;
	Vec4	valueMax;
	Vec4	lookupScale;
	Vec4	lookupBias;

	TextureFormatInfo (const Vec4& valueMin_, const Vec4& valueMax_, const Vec4& lookupScale_, const Vec4& lookupBias_)
		: valueMin		(valueMin_)
		, valueMax		(valueMax_)
		, lookupScale	(lookupScale_)
		, lookupBias	(lookupBias_)
	{
	}
};

TextureFormatInfo	getTextureFormatInfo				(const TextureFormat& format);
IVec4				getTextureFormatBitDepth			(const TextureFormat& format);
IVec4				getTextureFormatMantissaBitDepth	(const TextureFormat& format);

// Texture fill.
void	clear							(const PixelBufferAccess& access, const Vec4& color);
void	clear							(const PixelBufferAccess& access, const IVec4& color);
void	clearDepth						(const PixelBufferAccess& access, float depth);
void	clearStencil					(const PixelBufferAccess& access, int stencil);
void	fillWithComponentGradients		(const PixelBufferAccess& access, const Vec4& minVal, const Vec4& maxVal);
void	fillWithGrid					(const PixelBufferAccess& access, int cellSize, const Vec4& colorA, const Vec4& colorB);
void	fillWithRepeatableGradient		(const PixelBufferAccess& access, const Vec4& colorA, const Vec4& colorB);
void	fillWithMetaballs				(const PixelBufferAccess& access, int numMetaballs, deUint32 seed);
void	fillWithRGBAQuads				(const PixelBufferAccess& access);

void	copy							(const PixelBufferAccess& dst, const ConstPixelBufferAccess& src);
void	scale							(const PixelBufferAccess& dst, const ConstPixelBufferAccess& src, Sampler::FilterMode filter);

void	estimatePixelValueRange			(const ConstPixelBufferAccess& access, Vec4& minVal, Vec4& maxVal);
void	computePixelScaleBias			(const ConstPixelBufferAccess& access, Vec4& scale, Vec4& bias);

int		getCubeArrayFaceIndex			(CubeFace face);

//! FP32->U8 with RTE rounding (extremely fast, always accurate).
inline deUint8 floatToU8 (float fv)
{
	union { float fv; deUint32 uv; deInt32 iv; } v;
	v.fv = fv;

	const deUint32	e	= (deUint32)(126-(v.iv>>23));
	deUint32		m	= v.uv;

	m &= 0x00ffffffu;
	m |= 0x00800000u;
	m  = (m << 8) - m;
	m  = 0x00800000u + (m >> e);

	if (e > 8)
		m = e;

	return (deUint8)(m>>24);
}

} // tcu

#endif // _TCUTEXTUREUTIL_HPP
