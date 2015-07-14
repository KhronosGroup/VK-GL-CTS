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
 * \brief Fuzzy image comparison.
 *//*--------------------------------------------------------------------*/

#include "tcuFuzzyImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "deMath.h"
#include "deRandom.hpp"

#include <vector>

namespace tcu
{

using std::vector;

template<int Channel>
static inline deUint8 getChannel (deUint32 color)
{
	return (deUint8)((color >> (Channel*8)) & 0xff);
}

static inline deUint8 getChannel (deUint32 color, int channel)
{
	return (deUint8)((color >> (channel*8)) & 0xff);
}

static inline deUint32 setChannel (deUint32 color, int channel, deUint8 val)
{
	return (color & ~(0xffu << (8*channel))) | (val << (8*channel));
}

static inline Vec4 toFloatVec (deUint32 color)
{
	return Vec4((float)getChannel<0>(color), (float)getChannel<1>(color), (float)getChannel<2>(color), (float)getChannel<3>(color));
}

static inline deUint8 roundToUint8Sat (float v)
{
	return (deUint8)de::clamp((int)(v + 0.5f), 0, 255);
}

static inline deUint32 toColor (Vec4 v)
{
	return roundToUint8Sat(v[0]) | (roundToUint8Sat(v[1]) << 8) | (roundToUint8Sat(v[2]) << 16) | (roundToUint8Sat(v[3]) << 24);
}

template<int NumChannels>
static inline deUint32 readUnorm8 (const tcu::ConstPixelBufferAccess& src, int x, int y)
{
	const deUint8*	ptr	= (const deUint8*)src.getDataPtr() + src.getRowPitch()*y + x*NumChannels;
	deUint32		v	= 0;

	for (int c = 0; c < NumChannels; c++)
		v |= ptr[c] << (c*8);

	if (NumChannels < 4)
		v |= 0xffu << 24;

	return v;
}

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
template<>
inline deUint32 readUnorm8<4> (const tcu::ConstPixelBufferAccess& src, int x, int y)
{
	return *(const deUint32*)((const deUint8*)src.getDataPtr() + src.getRowPitch()*y + x*4);
}
#endif

template<int NumChannels>
static inline void writeUnorm8 (const tcu::PixelBufferAccess& dst, int x, int y, deUint32 val)
{
	deUint8* ptr = (deUint8*)dst.getDataPtr() + dst.getRowPitch()*y + x*NumChannels;

	for (int c = 0; c < NumChannels; c++)
		ptr[c] = getChannel(val, c);
}

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
template<>
inline void writeUnorm8<4> (const tcu::PixelBufferAccess& dst, int x, int y, deUint32 val)
{
	*(deUint32*)((deUint8*)dst.getDataPtr() + dst.getRowPitch()*y + x*4) = val;
}
#endif

static inline float compareColors (deUint32 pa, deUint32 pb, int minErrThreshold)
{
	int r = de::max<int>(de::abs((int)getChannel<0>(pa) - (int)getChannel<0>(pb)) - minErrThreshold, 0);
	int g = de::max<int>(de::abs((int)getChannel<1>(pa) - (int)getChannel<1>(pb)) - minErrThreshold, 0);
	int b = de::max<int>(de::abs((int)getChannel<2>(pa) - (int)getChannel<2>(pb)) - minErrThreshold, 0);
	int a = de::max<int>(de::abs((int)getChannel<3>(pa) - (int)getChannel<3>(pb)) - minErrThreshold, 0);

	float scale	= 1.0f/(255-minErrThreshold);
	float sqSum	= (float)(r*r + g*g + b*b + a*a) * (scale*scale);

	return deFloatSqrt(sqSum);
}

template<int NumChannels>
inline deUint32 bilinearSample (const ConstPixelBufferAccess& src, float u, float v)
{
	int w = src.getWidth();
	int h = src.getHeight();

	int x0 = deFloorFloatToInt32(u-0.5f);
	int x1 = x0+1;
	int y0 = deFloorFloatToInt32(v-0.5f);
	int y1 = y0+1;

	int i0 = de::clamp(x0, 0, w-1);
	int i1 = de::clamp(x1, 0, w-1);
	int j0 = de::clamp(y0, 0, h-1);
	int j1 = de::clamp(y1, 0, h-1);

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);

	deUint32 p00	= readUnorm8<NumChannels>(src, i0, j0);
	deUint32 p10	= readUnorm8<NumChannels>(src, i1, j0);
	deUint32 p01	= readUnorm8<NumChannels>(src, i0, j1);
	deUint32 p11	= readUnorm8<NumChannels>(src, i1, j1);
	deUint32 dst	= 0;

	// Interpolate.
	for (int c = 0; c < NumChannels; c++)
	{
		float f = (getChannel(p00, c)*(1.0f-a)*(1.0f-b)) +
				  (getChannel(p10, c)*(     a)*(1.0f-b)) +
				  (getChannel(p01, c)*(1.0f-a)*(     b)) +
				  (getChannel(p11, c)*(     a)*(     b));
		dst = setChannel(dst, c, roundToUint8Sat(f));
	}

	return dst;
}

template<int DstChannels, int SrcChannels>
static void separableConvolve (const PixelBufferAccess& dst, const ConstPixelBufferAccess& src, int shiftX, int shiftY, const std::vector<float>& kernelX, const std::vector<float>& kernelY)
{
	DE_ASSERT(dst.getWidth() == src.getWidth() && dst.getHeight() == src.getHeight());

	TextureLevel		tmp			(dst.getFormat(), dst.getHeight(), dst.getWidth());
	PixelBufferAccess	tmpAccess	= tmp.getAccess();

	int kw = (int)kernelX.size();
	int kh = (int)kernelY.size();

	// Horizontal pass
	// \note Temporary surface is written in column-wise order
	for (int j = 0; j < src.getHeight(); j++)
	{
		for (int i = 0; i < src.getWidth(); i++)
		{
			Vec4 sum(0);

			for (int kx = 0; kx < kw; kx++)
			{
				float		f = kernelX[kw-kx-1];
				deUint32	p = readUnorm8<SrcChannels>(src, de::clamp(i+kx-shiftX, 0, src.getWidth()-1), j);

				sum += toFloatVec(p)*f;
			}

			writeUnorm8<DstChannels>(tmpAccess, j, i, toColor(sum));
		}
	}

	// Vertical pass
	for (int j = 0; j < src.getHeight(); j++)
	{
		for (int i = 0; i < src.getWidth(); i++)
		{
			Vec4 sum(0.0f);

			for (int ky = 0; ky < kh; ky++)
			{
				float		f = kernelY[kh-ky-1];
				deUint32	p = readUnorm8<DstChannels>(tmpAccess, de::clamp(j+ky-shiftY, 0, tmp.getWidth()-1), i);

				sum += toFloatVec(p)*f;
			}

			writeUnorm8<DstChannels>(dst, i, j, toColor(sum));
		}
	}
}

template<int NumChannels>
static float compareToNeighbor (const FuzzyCompareParams& params, de::Random& rnd, deUint32 pixel, const ConstPixelBufferAccess& surface, int x, int y)
{
	float minErr = +100.f;

	// (x, y) + (0, 0)
	minErr = deFloatMin(minErr, compareColors(pixel, readUnorm8<NumChannels>(surface, x, y), params.minErrThreshold));
	if (minErr == 0.0f)
		return minErr;

	// Area around (x, y)
	static const int s_coords[][2] =
	{
		{-1, -1},
		{ 0, -1},
		{+1, -1},
		{-1,  0},
		{+1,  0},
		{-1, +1},
		{ 0, +1},
		{+1, +1}
	};

	for (int d = 0; d < (int)DE_LENGTH_OF_ARRAY(s_coords); d++)
	{
		int dx = x + s_coords[d][0];
		int dy = y + s_coords[d][1];

		if (!deInBounds32(dx, 0, surface.getWidth()) || !deInBounds32(dy, 0, surface.getHeight()))
			continue;

		minErr = deFloatMin(minErr, compareColors(pixel, readUnorm8<NumChannels>(surface, dx, dy), params.minErrThreshold));
		if (minErr == 0.0f)
			return minErr;
	}

	// Random bilinear-interpolated samples around (x, y)
	for (int s = 0; s < 32; s++)
	{
		float dx = (float)x + rnd.getFloat()*2.0f - 0.5f;
		float dy = (float)y + rnd.getFloat()*2.0f - 0.5f;

		deUint32 sample = bilinearSample<NumChannels>(surface, dx, dy);

		minErr = deFloatMin(minErr, compareColors(pixel, sample, params.minErrThreshold));
		if (minErr == 0.0f)
			return minErr;
	}

	return minErr;
}

static inline float toGrayscale (const Vec4& c)
{
	return 0.2126f*c[0] + 0.7152f*c[1] + 0.0722f*c[2];
}

static bool isFormatSupported (const TextureFormat& format)
{
	return format.type == TextureFormat::UNORM_INT8 && (format.order == TextureFormat::RGB || format.order == TextureFormat::RGBA);
}

float fuzzyCompare (const FuzzyCompareParams& params, const ConstPixelBufferAccess& ref, const ConstPixelBufferAccess& cmp, const PixelBufferAccess& errorMask)
{
	DE_ASSERT(ref.getWidth() == cmp.getWidth() && ref.getHeight() == cmp.getHeight());
	DE_ASSERT(errorMask.getWidth() == ref.getWidth() && errorMask.getHeight() == ref.getHeight());

	if (!isFormatSupported(ref.getFormat()) || !isFormatSupported(cmp.getFormat()))
		throw InternalError("Unsupported format in fuzzy comparison", DE_NULL, __FILE__, __LINE__);

	int			width	= ref.getWidth();
	int			height	= ref.getHeight();
	de::Random	rnd		(667);

	// Filtered
	TextureLevel refFiltered(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), width, height);
	TextureLevel cmpFiltered(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), width, height);

	// Kernel = {0.15, 0.7, 0.15}
	vector<float> kernel(3);
	kernel[0] = kernel[2] = 0.1f; kernel[1]= 0.8f;
	int shift = (int)(kernel.size() - 1) / 2;

	switch (ref.getFormat().order)
	{
		case TextureFormat::RGBA:	separableConvolve<4, 4>(refFiltered, ref, shift, shift, kernel, kernel);	break;
		case TextureFormat::RGB:	separableConvolve<4, 3>(refFiltered, ref, shift, shift, kernel, kernel);	break;
		default:
			DE_ASSERT(DE_FALSE);
	}

	switch (cmp.getFormat().order)
	{
		case TextureFormat::RGBA:	separableConvolve<4, 4>(cmpFiltered, cmp, shift, shift, kernel, kernel);	break;
		case TextureFormat::RGB:	separableConvolve<4, 3>(cmpFiltered, cmp, shift, shift, kernel, kernel);	break;
		default:
			DE_ASSERT(DE_FALSE);
	}

	int		numSamples	= 0;
	float	errSum		= 0.0f;

	// Clear error mask to green.
	clear(errorMask, Vec4(0.0f, 1.0f, 0.0f, 1.0f));

	ConstPixelBufferAccess refAccess = refFiltered.getAccess();
	ConstPixelBufferAccess cmpAccess = cmpFiltered.getAccess();

	for (int y = 1; y < height-1; y++)
	{
		for (int x = 1; x < width-1; x += params.maxSampleSkip > 0 ? (int)rnd.getInt(0, params.maxSampleSkip) : 1)
		{
			const float	err0	= compareToNeighbor<4>(params, rnd, readUnorm8<4>(refAccess, x, y), cmpAccess, x, y);
			const float	err1	= compareToNeighbor<4>(params, rnd, readUnorm8<4>(cmpAccess, x, y), refAccess, x, y);
			float		err		= deFloatMin(err0, err1);

			err = deFloatPow(err, params.errExp);

			errSum		+= err;
			numSamples	+= 1;

			// Build error image.
			float	red		= err * 500.0f;
			float	luma	= toGrayscale(cmp.getPixel(x, y));
			float	rF		= 0.7f + 0.3f*luma;
			errorMask.setPixel(Vec4(red*rF, (1.0f-red)*rF, 0.0f, 1.0f), x, y);
		}
	}

	// Scale error sum based on number of samples taken
	errSum *= (float)((width-2) * (height-2)) / (float)numSamples;

	return errSum;
}

} // tcu
