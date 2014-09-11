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
 * \brief Reference Texture Implementation.
 *//*--------------------------------------------------------------------*/

#include "tcuTexture.hpp"
#include "deInt32.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deMemory.h"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuFloat.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"

#include <limits>

namespace tcu
{

// \note No denorm support, no sign.
typedef Float<deUint32, 5, 6, 15, 0>	Float11;
typedef Float<deUint32, 5, 5, 15, 0>	Float10;

namespace
{

// Optimized getters for common formats.
// \todo [2012-11-14 pyry] Use intrinsics if available.

inline Vec4		readRGBA8888Float	(const deUint8* ptr) { return Vec4(ptr[0]/255.0f, ptr[1]/255.0f, ptr[2]/255.0f, ptr[3]/255.0f); }
inline Vec4		readRGB888Float		(const deUint8* ptr) { return Vec4(ptr[0]/255.0f, ptr[1]/255.0f, ptr[2]/255.0f, 1.0f); }
inline IVec4	readRGBA8888Int		(const deUint8* ptr) { return IVec4(ptr[0], ptr[1], ptr[2], ptr[3]); }
inline IVec4	readRGB888Int		(const deUint8* ptr) { return IVec4(ptr[0], ptr[1], ptr[2], 0xff); }

// Optimized setters.

inline void writeRGBA8888Int (deUint8* ptr, const IVec4& val)
{
	ptr[0] = de::clamp(val[0], 0, 255);
	ptr[1] = de::clamp(val[1], 0, 255);
	ptr[2] = de::clamp(val[2], 0, 255);
	ptr[3] = de::clamp(val[3], 0, 255);
}

inline void writeRGB888Int (deUint8* ptr, const IVec4& val)
{
	ptr[0] = de::clamp(val[0], 0, 255);
	ptr[1] = de::clamp(val[1], 0, 255);
	ptr[2] = de::clamp(val[2], 0, 255);
}

inline void writeRGBA8888Float (deUint8* ptr, const Vec4& val)
{
	ptr[0] = floatToU8(val[0]);
	ptr[1] = floatToU8(val[1]);
	ptr[2] = floatToU8(val[2]);
	ptr[3] = floatToU8(val[3]);
}

inline void writeRGB888Float (deUint8* ptr, const Vec4& val)
{
	ptr[0] = floatToU8(val[0]);
	ptr[1] = floatToU8(val[1]);
	ptr[2] = floatToU8(val[2]);
}

enum Channel
{
	// \note CHANNEL_N must equal int N
	CHANNEL_0 = 0,
	CHANNEL_1,
	CHANNEL_2,
	CHANNEL_3,

	CHANNEL_ZERO,
	CHANNEL_ONE
};

// \todo [2011-09-21 pyry] Move to tcutil?
template <typename T>
inline T convertSatRte (float f)
{
	// \note Doesn't work for 64-bit types
	DE_STATIC_ASSERT(sizeof(T) < sizeof(deUint64));
	DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

	deInt64	minVal	= std::numeric_limits<T>::min();
	deInt64 maxVal	= std::numeric_limits<T>::max();
	float	q		= deFloatFrac(f);
	deInt64 intVal	= (deInt64)(f-q);

	// Rounding.
	if (q == 0.5f)
	{
		if (intVal % 2 != 0)
			intVal++;
	}
	else if (q > 0.5f)
		intVal++;
	// else Don't add anything

	// Saturate.
	intVal = de::max(minVal, de::min(maxVal, intVal));

	return (T)intVal;
}

const Channel* getChannelReadMap (TextureFormat::ChannelOrder order)
{
	static const Channel INV[]	= { CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_ONE };
	static const Channel R[]	= { CHANNEL_0,		CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_ONE };
	static const Channel A[]	= { CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_0	};
	static const Channel I[]	= { CHANNEL_0,		CHANNEL_0,		CHANNEL_0,		CHANNEL_0	};
	static const Channel L[]	= { CHANNEL_0,		CHANNEL_0,		CHANNEL_0,		CHANNEL_ONE	};
	static const Channel LA[]	= { CHANNEL_0,		CHANNEL_0,		CHANNEL_0,		CHANNEL_1	};
	static const Channel RG[]	= { CHANNEL_0,		CHANNEL_1,		CHANNEL_ZERO,	CHANNEL_ONE	};
	static const Channel RA[]	= { CHANNEL_0,		CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_1	};
	static const Channel RGB[]	= { CHANNEL_0,		CHANNEL_1,		CHANNEL_2,		CHANNEL_ONE	};
	static const Channel RGBA[]	= { CHANNEL_0,		CHANNEL_1,		CHANNEL_2,		CHANNEL_3	};
	static const Channel BGRA[]	= { CHANNEL_2,		CHANNEL_1,		CHANNEL_0,		CHANNEL_3	};
	static const Channel ARGB[]	= { CHANNEL_1,		CHANNEL_2,		CHANNEL_3,		CHANNEL_0	};
	static const Channel D[]	= { CHANNEL_0,		CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_ONE	};
	static const Channel S[]	= { CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_0	};
	static const Channel DS[]	= { CHANNEL_0,		CHANNEL_ZERO,	CHANNEL_ZERO,	CHANNEL_1	};

	switch (order)
	{
		case TextureFormat::R:			return R;
		case TextureFormat::A:			return A;
		case TextureFormat::I:			return I;
		case TextureFormat::L:			return L;
		case TextureFormat::LA:			return LA;
		case TextureFormat::RG:			return RG;
		case TextureFormat::RA:			return RA;
		case TextureFormat::RGB:		return RGB;
		case TextureFormat::RGBA:		return RGBA;
		case TextureFormat::ARGB:		return ARGB;
		case TextureFormat::BGRA:		return BGRA;
		case TextureFormat::sRGB:		return RGB;
		case TextureFormat::sRGBA:		return RGBA;
		case TextureFormat::D:			return D;
		case TextureFormat::S:			return S;
		case TextureFormat::DS:			return DS;
		default:
			DE_ASSERT(DE_FALSE);
			return INV;
	}
}

const int* getChannelWriteMap (TextureFormat::ChannelOrder order)
{
	static const int R[]	= { 0 };
	static const int A[]	= { 3 };
	static const int I[]	= { 0 };
	static const int L[]	= { 0 };
	static const int LA[]	= { 0, 3 };
	static const int RG[]	= { 0, 1 };
	static const int RA[]	= { 0, 3 };
	static const int RGB[]	= { 0, 1, 2 };
	static const int RGBA[]	= { 0, 1, 2, 3 };
	static const int BGRA[]	= { 2, 1, 0, 3 };
	static const int ARGB[]	= { 3, 0, 1, 2 };
	static const int D[]	= { 0 };
	static const int S[]	= { 3 };
	static const int DS[]	= { 0, 3 };

	switch (order)
	{
		case TextureFormat::R:			return R;
		case TextureFormat::A:			return A;
		case TextureFormat::I:			return I;
		case TextureFormat::L:			return L;
		case TextureFormat::LA:			return LA;
		case TextureFormat::RG:			return RG;
		case TextureFormat::RA:			return RA;
		case TextureFormat::RGB:		return RGB;
		case TextureFormat::RGBA:		return RGBA;
		case TextureFormat::ARGB:		return ARGB;
		case TextureFormat::BGRA:		return BGRA;
		case TextureFormat::sRGB:		return RGB;
		case TextureFormat::sRGBA:		return RGBA;
		case TextureFormat::D:			return D;
		case TextureFormat::S:			return S;
		case TextureFormat::DS:			return DS;
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

int getChannelSize (TextureFormat::ChannelType type)
{
	switch (type)
	{
		case TextureFormat::SNORM_INT8:			return 1;
		case TextureFormat::SNORM_INT16:		return 2;
		case TextureFormat::SNORM_INT32:		return 4;
		case TextureFormat::UNORM_INT8:			return 1;
		case TextureFormat::UNORM_INT16:		return 2;
		case TextureFormat::UNORM_INT32:		return 4;
		case TextureFormat::SIGNED_INT8:		return 1;
		case TextureFormat::SIGNED_INT16:		return 2;
		case TextureFormat::SIGNED_INT32:		return 4;
		case TextureFormat::UNSIGNED_INT8:		return 1;
		case TextureFormat::UNSIGNED_INT16:		return 2;
		case TextureFormat::UNSIGNED_INT32:		return 4;
		case TextureFormat::HALF_FLOAT:			return 2;
		case TextureFormat::FLOAT:				return 4;
		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

int getNumUsedChannels (TextureFormat::ChannelOrder order)
{
	switch (order)
	{
		case TextureFormat::R:			return 1;
		case TextureFormat::A:			return 1;
		case TextureFormat::I:			return 1;
		case TextureFormat::L:			return 1;
		case TextureFormat::LA:			return 2;
		case TextureFormat::RG:			return 2;
		case TextureFormat::RA:			return 2;
		case TextureFormat::RGB:		return 3;
		case TextureFormat::RGBA:		return 4;
		case TextureFormat::ARGB:		return 4;
		case TextureFormat::BGRA:		return 4;
		case TextureFormat::sRGB:		return 3;
		case TextureFormat::sRGBA:		return 4;
		case TextureFormat::D:			return 1;
		case TextureFormat::S:			return 1;
		case TextureFormat::DS:			return 2;
		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

inline float channelToFloat (const deUint8* value, TextureFormat::ChannelType type)
{
	switch (type)
	{
		case TextureFormat::SNORM_INT8:			return de::max(-1.0f, (float)*((const deInt8*)value) / 127.0f);
		case TextureFormat::SNORM_INT16:		return de::max(-1.0f, (float)*((const deInt16*)value) / 32767.0f);
		case TextureFormat::SNORM_INT32:		return de::max(-1.0f, (float)*((const deInt32*)value) / 2147483647.0f);
		case TextureFormat::UNORM_INT8:			return (float)*((const deUint8*)value) / 255.0f;
		case TextureFormat::UNORM_INT16:		return (float)*((const deUint16*)value) / 65535.0f;
		case TextureFormat::UNORM_INT32:		return (float)*((const deUint32*)value) / 4294967295.0f;
		case TextureFormat::SIGNED_INT8:		return (float)*((const deInt8*)value);
		case TextureFormat::SIGNED_INT16:		return (float)*((const deInt16*)value);
		case TextureFormat::SIGNED_INT32:		return (float)*((const deInt32*)value);
		case TextureFormat::UNSIGNED_INT8:		return (float)*((const deUint8*)value);
		case TextureFormat::UNSIGNED_INT16:		return (float)*((const deUint16*)value);
		case TextureFormat::UNSIGNED_INT32:		return (float)*((const deUint32*)value);
		case TextureFormat::HALF_FLOAT:			return deFloat16To32(*(const deFloat16*)value);
		case TextureFormat::FLOAT:				return *((const float*)value);
		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

inline int channelToInt (const deUint8* value, TextureFormat::ChannelType type)
{
	switch (type)
	{
		case TextureFormat::SNORM_INT8:			return (int)*((const deInt8*)value);
		case TextureFormat::SNORM_INT16:		return (int)*((const deInt16*)value);
		case TextureFormat::SNORM_INT32:		return (int)*((const deInt32*)value);
		case TextureFormat::UNORM_INT8:			return (int)*((const deUint8*)value);
		case TextureFormat::UNORM_INT16:		return (int)*((const deUint16*)value);
		case TextureFormat::UNORM_INT32:		return (int)*((const deUint32*)value);
		case TextureFormat::SIGNED_INT8:		return (int)*((const deInt8*)value);
		case TextureFormat::SIGNED_INT16:		return (int)*((const deInt16*)value);
		case TextureFormat::SIGNED_INT32:		return (int)*((const deInt32*)value);
		case TextureFormat::UNSIGNED_INT8:		return (int)*((const deUint8*)value);
		case TextureFormat::UNSIGNED_INT16:		return (int)*((const deUint16*)value);
		case TextureFormat::UNSIGNED_INT32:		return (int)*((const deUint32*)value);
		case TextureFormat::HALF_FLOAT:			return (int)deFloat16To32(*(const deFloat16*)value);
		case TextureFormat::FLOAT:				return (int)*((const float*)value);
		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

void floatToChannel (deUint8* dst, float src, TextureFormat::ChannelType type)
{
	switch (type)
	{
		case TextureFormat::SNORM_INT8:			*((deInt8*)dst)			= convertSatRte<deInt8>		(src * 127.0f);			break;
		case TextureFormat::SNORM_INT16:		*((deInt16*)dst)		= convertSatRte<deInt16>	(src * 32767.0f);		break;
		case TextureFormat::SNORM_INT32:		*((deInt32*)dst)		= convertSatRte<deInt32>	(src * 2147483647.0f);	break;
		case TextureFormat::UNORM_INT8:			*((deUint8*)dst)		= convertSatRte<deUint8>	(src * 255.0f);			break;
		case TextureFormat::UNORM_INT16:		*((deUint16*)dst)		= convertSatRte<deUint16>	(src * 65535.0f);		break;
		case TextureFormat::UNORM_INT32:		*((deUint32*)dst)		= convertSatRte<deUint32>	(src * 4294967295.0f);	break;
		case TextureFormat::SIGNED_INT8:		*((deInt8*)dst)			= convertSatRte<deInt8>		(src);					break;
		case TextureFormat::SIGNED_INT16:		*((deInt16*)dst)		= convertSatRte<deInt16>	(src);					break;
		case TextureFormat::SIGNED_INT32:		*((deInt32*)dst)		= convertSatRte<deInt32>	(src);					break;
		case TextureFormat::UNSIGNED_INT8:		*((deUint8*)dst)		= convertSatRte<deUint8>	(src);					break;
		case TextureFormat::UNSIGNED_INT16:		*((deUint16*)dst)		= convertSatRte<deUint16>	(src);					break;
		case TextureFormat::UNSIGNED_INT32:		*((deUint32*)dst)		= convertSatRte<deUint32>	(src);					break;
		case TextureFormat::HALF_FLOAT:			*((deFloat16*)dst)		= deFloat32To16				(src);					break;
		case TextureFormat::FLOAT:				*((float*)dst)			= src;												break;
		default:
			DE_ASSERT(DE_FALSE);
	}
}

template <typename T, typename S>
static inline T convertSat (S src)
{
	S min = (S)std::numeric_limits<T>::min();
	S max = (S)std::numeric_limits<T>::max();

	if (src < min)
		return (T)min;
	else if (src > max)
		return (T)max;
	else
		return (T)src;
}

void intToChannel (deUint8* dst, int src, TextureFormat::ChannelType type)
{
	switch (type)
	{
		case TextureFormat::SNORM_INT8:			*((deInt8*)dst)			= convertSat<deInt8>	(src);				break;
		case TextureFormat::SNORM_INT16:		*((deInt16*)dst)		= convertSat<deInt16>	(src);				break;
		case TextureFormat::UNORM_INT8:			*((deUint8*)dst)		= convertSat<deUint8>	(src);				break;
		case TextureFormat::UNORM_INT16:		*((deUint16*)dst)		= convertSat<deUint16>	(src);				break;
		case TextureFormat::SIGNED_INT8:		*((deInt8*)dst)			= convertSat<deInt8>	(src);				break;
		case TextureFormat::SIGNED_INT16:		*((deInt16*)dst)		= convertSat<deInt16>	(src);				break;
		case TextureFormat::SIGNED_INT32:		*((deInt32*)dst)		= convertSat<deInt32>	(src);				break;
		case TextureFormat::UNSIGNED_INT8:		*((deUint8*)dst)		= convertSat<deUint8>	((deUint32)src);	break;
		case TextureFormat::UNSIGNED_INT16:		*((deUint16*)dst)		= convertSat<deUint16>	((deUint32)src);	break;
		case TextureFormat::UNSIGNED_INT32:		*((deUint32*)dst)		= convertSat<deUint32>	((deUint32)src);	break;
		case TextureFormat::HALF_FLOAT:			*((deFloat16*)dst)		= deFloat32To16((float)src);				break;
		case TextureFormat::FLOAT:				*((float*)dst)			= (float)src;								break;
		default:
			DE_ASSERT(DE_FALSE);
	}
}

inline float channelToNormFloat (deUint32 src, int bits)
{
	deUint32 maxVal = (1u << bits) - 1;
	return (float)src / (float)maxVal;
}

inline deUint32 normFloatToChannel (float src, int bits)
{
	deUint32 maxVal = (1u << bits) - 1;
	deUint32 intVal = convertSatRte<deUint32>(src * (float)maxVal);
	return de::min(maxVal, intVal);
}

inline deUint32 uintToChannel (deUint32 src, int bits)
{
	deUint32 maxVal = (1u << bits) - 1;
	return de::min(maxVal, src);
}

deUint32 packRGB999E5 (const tcu::Vec4& color)
{
	const int	mBits	= 9;
	const int	eBits	= 5;
	const int	eBias	= 15;
	const int	eMax	= (1<<eBits)-1;
	const float	maxVal	= (float)(((1<<mBits) - 1) * (1<<(eMax-eBias))) / (float)(1<<mBits);

	float	rc		= deFloatClamp(color[0], 0.0f, maxVal);
	float	gc		= deFloatClamp(color[1], 0.0f, maxVal);
	float	bc		= deFloatClamp(color[2], 0.0f, maxVal);
	float	maxc	= de::max(rc, de::max(gc, bc));
	int		expp	= de::max(-eBias - 1, deFloorFloatToInt32(deFloatLog2(maxc))) + 1 + eBias;
	float	e		= deFloatPow(2.0f, (float)(expp-eBias-mBits));
	int		maxs	= deFloorFloatToInt32(maxc / e + 0.5f);

	deUint32	exps	= maxs == (1<<mBits) ? expp+1 : expp;
	deUint32	rs		= (deUint32)deClamp32(deFloorFloatToInt32(rc / e + 0.5f), 0, (1<<9)-1);
	deUint32	gs		= (deUint32)deClamp32(deFloorFloatToInt32(gc / e + 0.5f), 0, (1<<9)-1);
	deUint32	bs		= (deUint32)deClamp32(deFloorFloatToInt32(bc / e + 0.5f), 0, (1<<9)-1);

	DE_ASSERT((exps & ~((1<<5)-1)) == 0);
	DE_ASSERT((rs & ~((1<<9)-1)) == 0);
	DE_ASSERT((gs & ~((1<<9)-1)) == 0);
	DE_ASSERT((bs & ~((1<<9)-1)) == 0);

	return rs | (gs << 9) | (bs << 18) | (exps << 27);
}

tcu::Vec4 unpackRGB999E5 (deUint32 color)
{
	const int	mBits	= 9;
	const int	eBias	= 15;

	deUint32	exp		= color >> 27;
	deUint32	bs		= (color >> 18) & ((1<<9)-1);
	deUint32	gs		= (color >> 9) & ((1<<9)-1);
	deUint32	rs		= color & ((1<<9)-1);

	float		e		= deFloatPow(2.0f, (float)((int)exp - eBias - mBits));
	float		r		= (float)rs * e;
	float		g		= (float)gs * e;
	float		b		= (float)bs * e;

	return tcu::Vec4(r, g, b, 1.0f);
}

} // anonymous

/** Get pixel size in bytes. */
int TextureFormat::getPixelSize (void) const
{
	if (type == CHANNELTYPE_LAST && order == CHANNELORDER_LAST)
	{
		// Invalid/empty format.
		return 0;
	}
	else if (type == UNORM_SHORT_565	||
			 type == UNORM_SHORT_555	||
			 type == UNORM_SHORT_4444	||
			 type == UNORM_SHORT_5551)
	{
		DE_ASSERT(order == RGB || order == RGBA);
		return 2;
	}
	else if (type == UNORM_INT_101010			||
			 type == UNSIGNED_INT_999_E5_REV	||
			 type == UNSIGNED_INT_11F_11F_10F_REV)
	{
		DE_ASSERT(order == RGB);
		return 4;
	}
	else if (type == UNORM_INT_1010102_REV		||
			 type == UNSIGNED_INT_1010102_REV)
	{
		DE_ASSERT(order == RGBA);
		return 4;
	}
	else if (type == UNSIGNED_INT_24_8)
	{
		DE_ASSERT(order == D || order == DS);
		return 4;
	}
	else if (type == FLOAT_UNSIGNED_INT_24_8_REV)
	{
		DE_ASSERT(order == DS);
		return 8;
	}
	else
	{
		int numChannels	= 0;
		int channelSize	= 0;

		switch (order)
		{
			case R:			numChannels = 1;	break;
			case A:			numChannels = 1;	break;
			case I:			numChannels = 1;	break;
			case L:			numChannels = 1;	break;
			case LA:		numChannels = 2;	break;
			case RG:		numChannels = 2;	break;
			case RA:		numChannels = 2;	break;
			case RGB:		numChannels = 3;	break;
			case RGBA:		numChannels = 4;	break;
			case ARGB:		numChannels = 4;	break;
			case BGRA:		numChannels = 4;	break;
			case sRGB:		numChannels = 3;	break;
			case sRGBA:		numChannels = 4;	break;
			case D:			numChannels = 1;	break;
			case S:			numChannels = 1;	break;
			case DS:		numChannels = 2;	break;
			default:		DE_ASSERT(DE_FALSE);
		}

		switch (type)
		{
			case SNORM_INT8:		channelSize = 1;	break;
			case SNORM_INT16:		channelSize = 2;	break;
			case SNORM_INT32:		channelSize = 4;	break;
			case UNORM_INT8:		channelSize = 1;	break;
			case UNORM_INT16:		channelSize = 2;	break;
			case UNORM_INT32:		channelSize = 4;	break;
			case SIGNED_INT8:		channelSize = 1;	break;
			case SIGNED_INT16:		channelSize = 2;	break;
			case SIGNED_INT32:		channelSize = 4;	break;
			case UNSIGNED_INT8:		channelSize = 1;	break;
			case UNSIGNED_INT16:	channelSize = 2;	break;
			case UNSIGNED_INT32:	channelSize = 4;	break;
			case HALF_FLOAT:		channelSize = 2;	break;
			case FLOAT:				channelSize = 4;	break;
			default:				DE_ASSERT(DE_FALSE);
		}

		return numChannels*channelSize;
	}
}

ConstPixelBufferAccess::ConstPixelBufferAccess (void)
	: m_width		(0)
	, m_height		(0)
	, m_depth		(0)
	, m_rowPitch	(0)
	, m_slicePitch	(0)
	, m_data		(DE_NULL)
{
}

ConstPixelBufferAccess::ConstPixelBufferAccess (const TextureFormat& format, int width, int height, int depth, const void* data)
	: m_format		(format)
	, m_width		(width)
	, m_height		(height)
	, m_depth		(depth)
	, m_rowPitch	(width*format.getPixelSize())
	, m_slicePitch	(m_rowPitch*height)
	, m_data		((void*)data)
{
}

ConstPixelBufferAccess::ConstPixelBufferAccess (const TextureFormat& format, int width, int height, int depth, int rowPitch, int slicePitch, const void* data)
	: m_format		(format)
	, m_width		(width)
	, m_height		(height)
	, m_depth		(depth)
	, m_rowPitch	(rowPitch)
	, m_slicePitch	(slicePitch)
	, m_data		((void*)data)
{
}

ConstPixelBufferAccess::ConstPixelBufferAccess (const TextureLevel& level)
	: m_format		(level.getFormat())
	, m_width		(level.getWidth())
	, m_height		(level.getHeight())
	, m_depth		(level.getDepth())
	, m_rowPitch	(m_width*m_format.getPixelSize())
	, m_slicePitch	(m_rowPitch*m_height)
	, m_data		((void*)level.getPtr())
{
}

PixelBufferAccess::PixelBufferAccess (const TextureFormat& format, int width, int height, int depth, void* data)
	: ConstPixelBufferAccess(format, width, height, depth, data)
{
}

PixelBufferAccess::PixelBufferAccess (const TextureFormat& format, int width, int height, int depth, int rowPitch, int slicePitch, void* data)
	: ConstPixelBufferAccess(format, width, height, depth, rowPitch, slicePitch, data)
{
}

PixelBufferAccess::PixelBufferAccess (TextureLevel& level)
	: ConstPixelBufferAccess(level)
{
}

void PixelBufferAccess::setPixels (const void* buf, int bufSize) const
{
	DE_ASSERT(bufSize == getDataSize());
	deMemcpy(getDataPtr(), buf, bufSize);
}

Vec4 ConstPixelBufferAccess::getPixel (int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, m_width));
	DE_ASSERT(de::inBounds(y, 0, m_height));
	DE_ASSERT(de::inBounds(z, 0, m_depth));

	// Optimized fomats.
	if (m_format.type == TextureFormat::UNORM_INT8)
	{
		if (m_format.order == TextureFormat::RGBA)
			return readRGBA8888Float((const deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*4);
		else if (m_format.order == TextureFormat::RGB)
			return readRGB888Float((const deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*3);
	}

	int				pixelSize		= m_format.getPixelSize();
	const deUint8*	pixelPtr		= (const deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

#define UB16(OFFS, COUNT)		((*((const deUint16*)pixelPtr) >> (OFFS)) & ((1<<(COUNT))-1))
#define UB32(OFFS, COUNT)		((*((const deUint32*)pixelPtr) >> (OFFS)) & ((1<<(COUNT))-1))
#define NB16(OFFS, COUNT)		channelToNormFloat(UB16(OFFS, COUNT), (COUNT))
#define NB32(OFFS, COUNT)		channelToNormFloat(UB32(OFFS, COUNT), (COUNT))

	// Packed formats.
	switch (m_format.type)
	{
		case TextureFormat::UNORM_SHORT_565:			return Vec4(NB16(11,  5), NB16( 5,  6), NB16( 0,  5), 1.0f);
		case TextureFormat::UNORM_SHORT_555:			return Vec4(NB16(10,  5), NB16( 5,  5), NB16( 0,  5), 1.0f);
		case TextureFormat::UNORM_SHORT_4444:			return Vec4(NB16(12,  4), NB16( 8,  4), NB16( 4,  4), NB16( 0, 4));
		case TextureFormat::UNORM_SHORT_5551:			return Vec4(NB16(11,  5), NB16( 6,  5), NB16( 1,  5), NB16( 0, 1));
		case TextureFormat::UNORM_INT_101010:			return Vec4(NB32(22, 10), NB32(12, 10), NB32( 2, 10), 1.0f);
		case TextureFormat::UNORM_INT_1010102_REV:		return Vec4(NB32( 0, 10), NB32(10, 10), NB32(20, 10), NB32(30, 2));
		case TextureFormat::UNSIGNED_INT_1010102_REV:	return UVec4(UB32(0, 10), UB32(10, 10), UB32(20, 10), UB32(30, 2)).cast<float>();
		case TextureFormat::UNSIGNED_INT_999_E5_REV:	return unpackRGB999E5(*((const deUint32*)pixelPtr));

		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				// \note Stencil is always ignored.
				case TextureFormat::D:	return Vec4(NB32(8, 24), 0.0f, 0.0f, 1.0f);
				case TextureFormat::DS:	return Vec4(NB32(8, 24), 0.0f, 0.0f, 1.0f /* (float)UB32(0, 8) */);
				default:
					DE_ASSERT(false);
			}

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
		{
			DE_ASSERT(m_format.order == TextureFormat::DS);
			float	d	= *((const float*)pixelPtr);
			// \note Stencil is ignored.
//			deUint8	s	= *((const deUint32*)(pixelPtr+4)) & 0xff;
			return Vec4(d, 0.0f, 0.0f, 1.0f);
		}

		case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
			return Vec4(Float11(UB32(0, 11)).asFloat(), Float11(UB32(11, 11)).asFloat(), Float10(UB32(22, 10)).asFloat(), 1.0f);

		default:
			break;
	}

#undef NB16
#undef NB32
#undef UB16
#undef UB32

	// Generic path.
	Vec4			result;
	const Channel*	channelMap	= getChannelReadMap(m_format.order);
	int				channelSize	= getChannelSize(m_format.type);

	for (int c = 0; c < 4; c++)
	{
		Channel map = channelMap[c];
		if (map == CHANNEL_ZERO)
			result[c] = 0.0f;
		else if (map == CHANNEL_ONE)
			result[c] = 1.0f;
		else
			result[c] = channelToFloat(pixelPtr + channelSize*((int)map), m_format.type);
	}

	return result;
}

IVec4 ConstPixelBufferAccess::getPixelInt (int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, m_width));
	DE_ASSERT(de::inBounds(y, 0, m_height));
	DE_ASSERT(de::inBounds(z, 0, m_depth));

	int				pixelSize		= m_format.getPixelSize();
	const deUint8*	pixelPtr		= (const deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;
	IVec4			result;

	// Optimized fomats.
	if (m_format.type == TextureFormat::UNORM_INT8)
	{
		if (m_format.order == TextureFormat::RGBA)		return readRGBA8888Int(pixelPtr);
		else if (m_format.order == TextureFormat::RGB)	return readRGB888Int(pixelPtr);
	}

#define U16(OFFS, COUNT)		((*((const deUint16*)pixelPtr) >> (OFFS)) & ((1<<(COUNT))-1))
#define U32(OFFS, COUNT)		((*((const deUint32*)pixelPtr) >> (OFFS)) & ((1<<(COUNT))-1))

	switch (m_format.type)
	{
		case TextureFormat::UNORM_SHORT_565:			return UVec4(U16(11,  5), U16( 5,  6), U16( 0,  5), 1).cast<int>();
		case TextureFormat::UNORM_SHORT_555:			return UVec4(U16(10,  5), U16( 5,  5), U16( 0,  5), 1).cast<int>();
		case TextureFormat::UNORM_SHORT_4444:			return UVec4(U16(12,  4), U16( 8,  4), U16( 4,  4), U16( 0, 4)).cast<int>();
		case TextureFormat::UNORM_SHORT_5551:			return UVec4(U16(11,  5), U16( 6,  5), U16( 1,  5), U16( 0, 1)).cast<int>();
		case TextureFormat::UNORM_INT_101010:			return UVec4(U32(22, 10), U32(12, 10), U32( 2, 10), 1).cast<int>();
		case TextureFormat::UNORM_INT_1010102_REV:		return UVec4(U32( 0, 10), U32(10, 10), U32(20, 10), U32(30, 2)).cast<int>();
		case TextureFormat::UNSIGNED_INT_1010102_REV:	return UVec4(U32( 0, 10), U32(10, 10), U32(20, 10), U32(30, 2)).cast<int>();

		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::D:	return UVec4(U32(8, 24), 0, 0, 1).cast<int>();
				case TextureFormat::S:	return UVec4(0, 0, 0, U32(8, 24)).cast<int>();
				case TextureFormat::DS:	return UVec4(U32(8, 24), 0, 0, U32(0, 8)).cast<int>();
				default:
					DE_ASSERT(false);
			}

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
		{
			DE_ASSERT(m_format.order == TextureFormat::DS);
			float	d	= *((const float*)pixelPtr);
			deUint8	s	= *((const deUint32*)(pixelPtr+4)) & 0xffu;
			// \note Returns bit-representation of depth floating-point value.
			return UVec4(tcu::Float32(d).bits(), 0, 0, s).cast<int>();
		}

		default:
			break; // To generic path.
	}

#undef U16
#undef U32

	// Generic path.
	const Channel*	channelMap	= getChannelReadMap(m_format.order);
	int				channelSize	= getChannelSize(m_format.type);

	for (int c = 0; c < 4; c++)
	{
		Channel map = channelMap[c];
		if (map == CHANNEL_ZERO)
			result[c] = 0;
		else if (map == CHANNEL_ONE)
			result[c] = 1;
		else
			result[c] = channelToInt(pixelPtr + channelSize*((int)map), m_format.type);
	}

	return result;
}

template<>
Vec4 ConstPixelBufferAccess::getPixelT (int x, int y, int z) const
{
	return getPixel(x, y, z);
}

template<>
IVec4 ConstPixelBufferAccess::getPixelT (int x, int y, int z) const
{
	return getPixelInt(x, y, z);
}

template<>
UVec4 ConstPixelBufferAccess::getPixelT (int x, int y, int z) const
{
	return getPixelUint(x, y, z);
}

float ConstPixelBufferAccess::getPixDepth (int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, getWidth()));
	DE_ASSERT(de::inBounds(y, 0, getHeight()));
	DE_ASSERT(de::inBounds(z, 0, getDepth()));

	int			pixelSize	= m_format.getPixelSize();
	deUint8*	pixelPtr	= (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

#define UB32(OFFS, COUNT) ((*((const deUint32*)pixelPtr) >> (OFFS)) & ((1<<(COUNT))-1))
#define NB32(OFFS, COUNT) channelToNormFloat(UB32(OFFS, COUNT), (COUNT))

	DE_ASSERT(m_format.order == TextureFormat::DS || m_format.order == TextureFormat::D);

	switch (m_format.type)
	{
		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::D:
				case TextureFormat::DS: // \note Fall-through.
					return NB32(8, 24);
				default:
					DE_ASSERT(false);
					return 0.0f;
			}

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			DE_ASSERT(m_format.order == TextureFormat::DS);
			return *((const float*)pixelPtr);

		default:
			DE_ASSERT(m_format.order == TextureFormat::D || m_format.order == TextureFormat::DS);
			return channelToFloat(pixelPtr, m_format.type);
	}

#undef UB32
#undef NB32
}

int ConstPixelBufferAccess::getPixStencil (int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, getWidth()));
	DE_ASSERT(de::inBounds(y, 0, getHeight()));
	DE_ASSERT(de::inBounds(z, 0, getDepth()));

	int			pixelSize	= m_format.getPixelSize();
	deUint8*	pixelPtr	= (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

	switch (m_format.type)
	{
		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::S:		return (int)(*((const deUint32*)pixelPtr) >> 8);
				case TextureFormat::DS:		return (int)(*((const deUint32*)pixelPtr) & 0xff);

				default:
					DE_ASSERT(false);
					return 0;
			}

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			DE_ASSERT(m_format.order == TextureFormat::DS);
			return *((const deUint32*)(pixelPtr+4)) & 0xff;

		default:
		{
			if (m_format.order == TextureFormat::S)
				return channelToInt(pixelPtr, m_format.type);
			else
			{
				DE_ASSERT(m_format.order == TextureFormat::DS);
				const int stencilChannelIndex = 3;
				return channelToInt(pixelPtr + getChannelSize(m_format.type)*stencilChannelIndex, m_format.type);
			}
		}
	}
}

void PixelBufferAccess::setPixel (const Vec4& color, int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, getWidth()));
	DE_ASSERT(de::inBounds(y, 0, getHeight()));
	DE_ASSERT(de::inBounds(z, 0, getDepth()));

	// Optimized fomats.
	if (m_format.type == TextureFormat::UNORM_INT8)
	{
		if (m_format.order == TextureFormat::RGBA)
		{
			deUint8* const ptr = (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*4;
			writeRGBA8888Float(ptr, color);
			return;
		}
		else if (m_format.order == TextureFormat::RGB)
		{
			deUint8* const ptr = (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*3;
			writeRGB888Float(ptr, color);
			return;
		}
	}

	const int		pixelSize	= m_format.getPixelSize();
	deUint8* const	pixelPtr	= (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

#define PN(VAL, OFFS, BITS)		(normFloatToChannel((VAL), (BITS)) << (OFFS))
#define PU(VAL, OFFS, BITS)		(uintToChannel((VAL), (BITS)) << (OFFS))

	switch (m_format.type)
	{
		case TextureFormat::UNORM_SHORT_565:		*((deUint16*)pixelPtr) = (deUint16)(PN(color[0], 11, 5) | PN(color[1], 5, 6) | PN(color[2], 0, 5));							break;
		case TextureFormat::UNORM_SHORT_555:		*((deUint16*)pixelPtr) = (deUint16)(PN(color[0], 10, 5) | PN(color[1], 5, 5) | PN(color[2], 0, 5));							break;
		case TextureFormat::UNORM_SHORT_4444:		*((deUint16*)pixelPtr) = (deUint16)(PN(color[0], 12, 4) | PN(color[1], 8, 4) | PN(color[2], 4, 4) | PN(color[3], 0, 4));	break;
		case TextureFormat::UNORM_SHORT_5551:		*((deUint16*)pixelPtr) = (deUint16)(PN(color[0], 11, 5) | PN(color[1], 6, 5) | PN(color[2], 1, 5) | PN(color[3], 0, 1));	break;
		case TextureFormat::UNORM_INT_101010:		*((deUint32*)pixelPtr) = PN(color[0], 22, 10) | PN(color[1], 12, 10) | PN(color[2], 2, 10);									break;
		case TextureFormat::UNORM_INT_1010102_REV:	*((deUint32*)pixelPtr) = PN(color[0],  0, 10) | PN(color[1], 10, 10) | PN(color[2], 20, 10) | PN(color[3], 30, 2);			break;

		case TextureFormat::UNSIGNED_INT_1010102_REV:
		{
			UVec4 u = color.cast<deUint32>();
			*((deUint32*)pixelPtr) = PU(u[0], 0, 10) | PU(u[1], 10, 10) |PU(u[2], 20, 10) | PU(u[3], 30, 2);
			break;
		}

		case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
			*((deUint32*)pixelPtr) = Float11(color[0]).bits() | (Float11(color[1]).bits() << 11) | (Float10(color[2]).bits() << 22);
			break;

		case TextureFormat::UNSIGNED_INT_999_E5_REV:
			*((deUint32*)pixelPtr) = packRGB999E5(color);
			break;

		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::D:		*((deUint32*)pixelPtr) = PN(color[0], 8, 24);									break;
				case TextureFormat::S:		*((deUint32*)pixelPtr) = PN(color[3], 8, 24);									break;
				case TextureFormat::DS:		*((deUint32*)pixelPtr) = PN(color[0], 8, 24) | PU((deUint32)color[3], 0, 8);	break;
				default:
					DE_ASSERT(false);
			}
			break;

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			DE_ASSERT(m_format.order == TextureFormat::DS);
			*((float*)pixelPtr)			= color[0];
			*((deUint32*)(pixelPtr+4))	= PU((deUint32)color[3], 0, 8);
			break;

		case TextureFormat::FLOAT:
			if (m_format.order == TextureFormat::D)
			{
				*((float*)pixelPtr) = color[0];
				break;
			}
			// else fall-through to default case!

		default:
		{
			// Generic path.
			int			numChannels	= getNumUsedChannels(m_format.order);
			const int*	map			= getChannelWriteMap(m_format.order);
			int			channelSize	= getChannelSize(m_format.type);

			for (int c = 0; c < numChannels; c++)
				floatToChannel(pixelPtr + channelSize*c, color[map[c]], m_format.type);

			break;
		}
	}

#undef PN
#undef PU
}

void PixelBufferAccess::setPixel (const IVec4& color, int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, getWidth()));
	DE_ASSERT(de::inBounds(y, 0, getHeight()));
	DE_ASSERT(de::inBounds(z, 0, getDepth()));

	int			pixelSize	= m_format.getPixelSize();
	deUint8*	pixelPtr	= (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

	// Optimized fomats.
	if (m_format.type == TextureFormat::UNORM_INT8)
	{
		if (m_format.order == TextureFormat::RGBA)		{ writeRGBA8888Int(pixelPtr, color);	return; }
		else if (m_format.order == TextureFormat::RGB)	{ writeRGB888Int(pixelPtr, color);		return; }
	}

#define PU(VAL, OFFS, BITS)		(uintToChannel((deUint32)(VAL), (BITS)) << (OFFS))

	switch (m_format.type)
	{
		case TextureFormat::UNORM_SHORT_565:			*((deUint16*)pixelPtr) = (deUint16)(PU(color[0], 11, 5) | PU(color[1], 5, 6) | PU(color[2], 0, 5));							break;
		case TextureFormat::UNORM_SHORT_555:			*((deUint16*)pixelPtr) = (deUint16)(PU(color[0], 10, 5) | PU(color[1], 5, 5) | PU(color[2], 0, 5));							break;
		case TextureFormat::UNORM_SHORT_4444:			*((deUint16*)pixelPtr) = (deUint16)(PU(color[0], 12, 4) | PU(color[1], 8, 4) | PU(color[2], 4, 4) | PU(color[3], 0, 4));	break;
		case TextureFormat::UNORM_SHORT_5551:			*((deUint16*)pixelPtr) = (deUint16)(PU(color[0], 11, 5) | PU(color[1], 6, 5) | PU(color[2], 1, 5) | PU(color[3], 0, 1));	break;
		case TextureFormat::UNORM_INT_101010:			*((deUint32*)pixelPtr) = PU(color[0], 22, 10) | PU(color[1], 12, 10) | PU(color[2], 2, 10);									break;
		case TextureFormat::UNORM_INT_1010102_REV:		*((deUint32*)pixelPtr) = PU(color[0],  0, 10) | PU(color[1], 10, 10) | PU(color[2], 20, 10) | PU(color[3], 30, 2);			break;
		case TextureFormat::UNSIGNED_INT_1010102_REV:	*((deUint32*)pixelPtr) = PU(color[0],  0, 10) | PU(color[1], 10, 10) | PU(color[2], 20, 10) | PU(color[3], 30, 2);			break;

		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::D:		*((deUint32*)pixelPtr) = PU(color[0], 8, 24);										break;
				case TextureFormat::S:		*((deUint32*)pixelPtr) = PU(color[3], 8, 24);										break;
				case TextureFormat::DS:		*((deUint32*)pixelPtr) = PU(color[0], 8, 24) | PU((deUint32)color[3], 0, 8);		break;
				default:
					DE_ASSERT(false);
			}
			break;

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			DE_ASSERT(m_format.order == TextureFormat::DS);
			*((deUint32*)pixelPtr)		= color[0];
			*((deUint32*)(pixelPtr+4))	= PU((deUint32)color[3], 0, 8);
			break;

		default:
		{
			// Generic path.
			int			numChannels	= getNumUsedChannels(m_format.order);
			const int*	map			= getChannelWriteMap(m_format.order);
			int			channelSize	= getChannelSize(m_format.type);

			for (int c = 0; c < numChannels; c++)
				intToChannel(pixelPtr + channelSize*c, color[map[c]], m_format.type);

			break;
		}
	}

#undef PU
}

void PixelBufferAccess::setPixDepth (float depth, int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, getWidth()));
	DE_ASSERT(de::inBounds(y, 0, getHeight()));
	DE_ASSERT(de::inBounds(z, 0, getDepth()));

	int			pixelSize	= m_format.getPixelSize();
	deUint8*	pixelPtr	= (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

#define PN(VAL, OFFS, BITS) (normFloatToChannel((VAL), (BITS)) << (OFFS))

	switch (m_format.type)
	{
		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::D:		*((deUint32*)pixelPtr) = PN(depth, 8, 24);											break;
				case TextureFormat::DS:		*((deUint32*)pixelPtr) = (*((deUint32*)pixelPtr) & 0x000000ff) | PN(depth, 8, 24);	break;
				default:
					DE_ASSERT(false);
			}
			break;

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			DE_ASSERT(m_format.order == TextureFormat::DS);
			*((float*)pixelPtr) = depth;
			break;

		default:
			DE_ASSERT(m_format.order == TextureFormat::D || m_format.order == TextureFormat::DS);
			floatToChannel(pixelPtr, depth, m_format.type);
			break;
	}

#undef PN
}

void PixelBufferAccess::setPixStencil (int stencil, int x, int y, int z) const
{
	DE_ASSERT(de::inBounds(x, 0, getWidth()));
	DE_ASSERT(de::inBounds(y, 0, getHeight()));
	DE_ASSERT(de::inBounds(z, 0, getDepth()));

	int			pixelSize	= m_format.getPixelSize();
	deUint8*	pixelPtr	= (deUint8*)getDataPtr() + z*m_slicePitch + y*m_rowPitch + x*pixelSize;

#define PU(VAL, OFFS, BITS) (uintToChannel((deUint32)(VAL), (BITS)) << (OFFS))

	switch (m_format.type)
	{
		case TextureFormat::UNSIGNED_INT_24_8:
			switch (m_format.order)
			{
				case TextureFormat::S:		*((deUint32*)pixelPtr) = PU(stencil, 8, 24);										break;
				case TextureFormat::DS:		*((deUint32*)pixelPtr) = (*((deUint32*)pixelPtr) & 0xffffff00) | PU(stencil, 0, 8);	break;
				default:
					DE_ASSERT(false);
			}
			break;

		case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
			DE_ASSERT(m_format.order == TextureFormat::DS);
			*((deUint32*)(pixelPtr+4))	= PU((deUint32)stencil, 0, 8);
			break;

		default:
			if (m_format.order == TextureFormat::S)
				intToChannel(pixelPtr, stencil, m_format.type);
			else
			{
				DE_ASSERT(m_format.order == TextureFormat::DS);
				const int stencilChannelIndex = 3;
				intToChannel(pixelPtr + getChannelSize(m_format.type)*stencilChannelIndex, stencil, m_format.type);
			}

			break;
	}

#undef PU
}

static inline int imod (int a, int b)
{
	int m = a % b;
	return m < 0 ? m + b : m;
}

static inline int mirror (int a)
{
	return a >= 0.0f ? a : -(1 + a);
}

// Nearest-even rounding in case of tie (fractional part 0.5), otherwise ordinary rounding.
static inline float rint (float a)
{
	DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

	float		fracVal		= deFloatFrac(a);

	if (fracVal != 0.5f)
		return deFloatRound(a); // Ordinary case.

	float	floorVal	= a - fracVal;
	bool	roundUp		= (deInt64)floorVal % 2 != 0;

	return floorVal + (roundUp ? 1.0f : 0.0f);
}

static inline int wrap (Sampler::WrapMode mode, int c, int size)
{
	switch (mode)
	{
		case tcu::Sampler::CLAMP_TO_BORDER:
			return deClamp32(c, -1, size);

		case tcu::Sampler::CLAMP_TO_EDGE:
			return deClamp32(c, 0, size-1);

		case tcu::Sampler::REPEAT_GL:
			return imod(c, size);

		case tcu::Sampler::REPEAT_CL:
			return imod(c, size);

		case tcu::Sampler::MIRRORED_REPEAT_GL:
			return (size - 1) - mirror(imod(c, 2*size) - size);

		case tcu::Sampler::MIRRORED_REPEAT_CL:
			return deClamp32(c, 0, size-1); // \note Actual mirroring done already in unnormalization function.

		default:
			DE_ASSERT(DE_FALSE);
			return 0;
	}
}

// Special unnormalization for REPEAT_CL and MIRRORED_REPEAT_CL wrap modes; otherwise ordinary unnormalization.
static inline float unnormalize (Sampler::WrapMode mode, float c, int size)
{
	switch (mode)
	{
		case tcu::Sampler::CLAMP_TO_EDGE:
		case tcu::Sampler::CLAMP_TO_BORDER:
		case tcu::Sampler::REPEAT_GL:
		case tcu::Sampler::MIRRORED_REPEAT_GL: // Fall-through (ordinary case).
			return (float)size*c;

		case tcu::Sampler::REPEAT_CL:
			return (float)size * (c - deFloatFloor(c));

		case tcu::Sampler::MIRRORED_REPEAT_CL:
			return (float)size * deFloatAbs(c - 2.0f * rint(0.5f * c));

		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

static inline bool isSRGB (TextureFormat format)
{
	return format.order == TextureFormat::sRGB || format.order == TextureFormat::sRGBA;
}

static bool isFixedPointDepthTextureFormat (const tcu::TextureFormat& format)
{
	const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(format.type);

	if (format.order == TextureFormat::D)
	{
		// depth internal formats cannot be non-normalized integers
		return channelClass != tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
	}
	else if (format.order == TextureFormat::DS)
	{
		// combined formats have no single channel class, detect format manually
		switch (format.type)
		{
			case tcu::TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:	return false;
			case tcu::TextureFormat::UNSIGNED_INT_24_8:				return true;

			default:
			{
				// unknown format
				DE_ASSERT(false);
				return true;
			}
		}
	}

	return false;
}

// Texel lookup with color conversion.
static inline Vec4 lookup (const ConstPixelBufferAccess& access, int i, int j, int k)
{
	Vec4 p = access.getPixel(i, j, k);
	return isSRGB(access.getFormat()) ? sRGBToLinear(p) : p;
}

static inline float execCompare (const tcu::Vec4& color, Sampler::CompareMode compare, int chanNdx, float ref_, bool isFixedPoint)
{
	const bool	clampValues	= isFixedPoint;	// if comparing against a floating point texture, ref (and value) is not clamped
	const float	cmp			= (clampValues) ? (de::clamp(color[chanNdx], 0.0f, 1.0f)) : (color[chanNdx]);
	const float	ref			= (clampValues) ? (de::clamp(ref_, 0.0f, 1.0f)) : (ref_);
	bool		res			= false;

	switch (compare)
	{
		case Sampler::COMPAREMODE_LESS:				res = ref < cmp;	break;
		case Sampler::COMPAREMODE_LESS_OR_EQUAL:	res = ref <= cmp;	break;
		case Sampler::COMPAREMODE_GREATER:			res = ref > cmp;	break;
		case Sampler::COMPAREMODE_GREATER_OR_EQUAL:	res = ref >= cmp;	break;
		case Sampler::COMPAREMODE_EQUAL:			res = ref == cmp;	break;
		case Sampler::COMPAREMODE_NOT_EQUAL:		res = ref != cmp;	break;
		case Sampler::COMPAREMODE_ALWAYS:			res = true;			break;
		case Sampler::COMPAREMODE_NEVER:			res = false;		break;
		default:
			DE_ASSERT(false);
	}

	return res ? 1.0f : 0.0f;
}

static Vec4 sampleNearest1D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, int level)
{
	int width	= access.getWidth();
	int x		= deFloorFloatToInt32(u);

	// Check for CLAMP_TO_BORDER.
	if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width)))
		return sampler.borderColor;

	int i = wrap(sampler.wrapS, x, width);

	return lookup(access, i, level, 0);
}

static Vec4 sampleNearest1D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, const IVec2& offset)
{
	int width	= access.getWidth();

	int x = deFloorFloatToInt32(u)+offset.x();

	// Check for CLAMP_TO_BORDER.
	if (sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width))
		return sampler.borderColor;

	int i = wrap(sampler.wrapS, x, width);

	return lookup(access, i, offset.y(), 0);
}

static Vec4 sampleNearest2D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, int depth)
{
	int width	= access.getWidth();
	int height	= access.getHeight();

	int x = deFloorFloatToInt32(u);
	int y = deFloorFloatToInt32(v);

	// Check for CLAMP_TO_BORDER.
	if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width)) ||
		(sampler.wrapT == Sampler::CLAMP_TO_BORDER && !deInBounds32(y, 0, height)))
		return sampler.borderColor;

	int i = wrap(sampler.wrapS, x, width);
	int j = wrap(sampler.wrapT, y, height);

	return lookup(access, i, j, depth);
}

static Vec4 sampleNearest2D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, const IVec3& offset)
{
	int width	= access.getWidth();
	int height	= access.getHeight();

	int x = deFloorFloatToInt32(u)+offset.x();
	int y = deFloorFloatToInt32(v)+offset.y();

	// Check for CLAMP_TO_BORDER.
	if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width)) ||
		(sampler.wrapT == Sampler::CLAMP_TO_BORDER && !deInBounds32(y, 0, height)))
		return sampler.borderColor;

	int i = wrap(sampler.wrapS, x, width);
	int j = wrap(sampler.wrapT, y, height);

	return lookup(access, i, j, offset.z());
}

static Vec4 sampleNearest3D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, float w)
{
	int width	= access.getWidth();
	int height	= access.getHeight();
	int depth	= access.getDepth();

	int x = deFloorFloatToInt32(u);
	int y = deFloorFloatToInt32(v);
	int z = deFloorFloatToInt32(w);

	// Check for CLAMP_TO_BORDER.
	if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width))	||
		(sampler.wrapT == Sampler::CLAMP_TO_BORDER && !deInBounds32(y, 0, height))	||
		(sampler.wrapR == Sampler::CLAMP_TO_BORDER && !deInBounds32(z, 0, depth)))
		return sampler.borderColor;

	int i = wrap(sampler.wrapS, x, width);
	int j = wrap(sampler.wrapT, y, height);
	int k = wrap(sampler.wrapR, z, depth);

	return lookup(access, i, j, k);
}

static Vec4 sampleNearest3D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, float w, const IVec3& offset)
{
	int width	= access.getWidth();
	int height	= access.getHeight();
	int depth	= access.getDepth();

	int x = deFloorFloatToInt32(u)+offset.x();
	int y = deFloorFloatToInt32(v)+offset.y();
	int z = deFloorFloatToInt32(w)+offset.z();

	// Check for CLAMP_TO_BORDER.
	if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width))	||
		(sampler.wrapT == Sampler::CLAMP_TO_BORDER && !deInBounds32(y, 0, height))	||
		(sampler.wrapR == Sampler::CLAMP_TO_BORDER && !deInBounds32(z, 0, depth)))
		return sampler.borderColor;

	int i = wrap(sampler.wrapS, x, width);
	int j = wrap(sampler.wrapT, y, height);
	int k = wrap(sampler.wrapR, z, depth);

	return lookup(access, i, j, k);
}

static Vec4 sampleLinear1D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, int level)
{
	int w = access.getWidth();

	int x0 = deFloorFloatToInt32(u-0.5f);
	int x1 = x0+1;

	int i0 = wrap(sampler.wrapS, x0, w);
	int i1 = wrap(sampler.wrapS, x1, w);

	float a = deFloatFrac(u-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p0 = i0UseBorder ? sampler.borderColor : lookup(access, i0, level, 0);
	Vec4 p1 = i1UseBorder ? sampler.borderColor : lookup(access, i1, level, 0);

	// Interpolate.
	return p0 * (1.0f-a) + a * p1;
}

static Vec4 sampleLinear1D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, const IVec2& offset)
{
	int w = access.getWidth();

	int x0 = deFloorFloatToInt32(u-0.5f)+offset.x();
	int x1 = x0+1;

	int i0 = wrap(sampler.wrapS, x0, w);
	int i1 = wrap(sampler.wrapS, x1, w);

	float a = deFloatFrac(u-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p0 = i0UseBorder ? sampler.borderColor : lookup(access, i0, offset.y(), 0);
	Vec4 p1 = i1UseBorder ? sampler.borderColor : lookup(access, i1, offset.y(), 0);

	// Interpolate.
	return p0 * (1.0f - a) + p1 * a;
}

static Vec4 sampleLinear2D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, int depth)
{
	int w = access.getWidth();
	int h = access.getHeight();

	int x0 = deFloorFloatToInt32(u-0.5f);
	int x1 = x0+1;
	int y0 = deFloorFloatToInt32(v-0.5f);
	int y1 = y0+1;

	int i0 = wrap(sampler.wrapS, x0, w);
	int i1 = wrap(sampler.wrapS, x1, w);
	int j0 = wrap(sampler.wrapT, y0, h);
	int j1 = wrap(sampler.wrapT, y1, h);

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);
	bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, h);
	bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, h);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p00 = (i0UseBorder || j0UseBorder) ? sampler.borderColor : lookup(access, i0, j0, depth);
	Vec4 p10 = (i1UseBorder || j0UseBorder) ? sampler.borderColor : lookup(access, i1, j0, depth);
	Vec4 p01 = (i0UseBorder || j1UseBorder) ? sampler.borderColor : lookup(access, i0, j1, depth);
	Vec4 p11 = (i1UseBorder || j1UseBorder) ? sampler.borderColor : lookup(access, i1, j1, depth);

	// Interpolate.
	return (p00*(1.0f-a)*(1.0f-b)) +
		   (p10*(     a)*(1.0f-b)) +
		   (p01*(1.0f-a)*(     b)) +
		   (p11*(     a)*(     b));
}

static Vec4 sampleLinear2D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, const IVec3& offset)
{
	int w = access.getWidth();
	int h = access.getHeight();

	int x0 = deFloorFloatToInt32(u-0.5f)+offset.x();
	int x1 = x0+1;
	int y0 = deFloorFloatToInt32(v-0.5f)+offset.y();
	int y1 = y0+1;

	int i0 = wrap(sampler.wrapS, x0, w);
	int i1 = wrap(sampler.wrapS, x1, w);
	int j0 = wrap(sampler.wrapT, y0, h);
	int j1 = wrap(sampler.wrapT, y1, h);

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);
	bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, h);
	bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, h);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p00 = (i0UseBorder || j0UseBorder) ? sampler.borderColor : lookup(access, i0, j0, offset.z());
	Vec4 p10 = (i1UseBorder || j0UseBorder) ? sampler.borderColor : lookup(access, i1, j0, offset.z());
	Vec4 p01 = (i0UseBorder || j1UseBorder) ? sampler.borderColor : lookup(access, i0, j1, offset.z());
	Vec4 p11 = (i1UseBorder || j1UseBorder) ? sampler.borderColor : lookup(access, i1, j1, offset.z());

	// Interpolate.
	return (p00*(1.0f-a)*(1.0f-b)) +
		   (p10*(     a)*(1.0f-b)) +
		   (p01*(1.0f-a)*(     b)) +
		   (p11*(     a)*(     b));
}

static float sampleLinear1DCompare (const ConstPixelBufferAccess& access, const Sampler& sampler, float ref, float u, const IVec2& offset, bool isFixedPointDepthFormat)
{
	int w = access.getWidth();

	int x0 = deFloorFloatToInt32(u-0.5f)+offset.x();
	int x1 = x0+1;

	int i0 = wrap(sampler.wrapS, x0, w);
	int i1 = wrap(sampler.wrapS, x1, w);

	float a = deFloatFrac(u-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p0Clr = i0UseBorder  ? sampler.borderColor : lookup(access, i0, offset.y(), 0);
	Vec4 p1Clr = i1UseBorder  ? sampler.borderColor : lookup(access, i1, offset.y(), 0);

	// Execute comparisons.
	float p0 = execCompare(p0Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
	float p1 = execCompare(p1Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);

	// Interpolate.
	return (p0 * (1.0f - a)) + (p1 * a);
}

static float sampleLinear2DCompare (const ConstPixelBufferAccess& access, const Sampler& sampler, float ref, float u, float v, const IVec3& offset, bool isFixedPointDepthFormat)
{
	int w = access.getWidth();
	int h = access.getHeight();

	int x0 = deFloorFloatToInt32(u-0.5f)+offset.x();
	int x1 = x0+1;
	int y0 = deFloorFloatToInt32(v-0.5f)+offset.y();
	int y1 = y0+1;

	int i0 = wrap(sampler.wrapS, x0, w);
	int i1 = wrap(sampler.wrapS, x1, w);
	int j0 = wrap(sampler.wrapT, y0, h);
	int j1 = wrap(sampler.wrapT, y1, h);

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);
	bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, h);
	bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, h);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p00Clr = (i0UseBorder || j0UseBorder) ? sampler.borderColor : lookup(access, i0, j0, offset.z());
	Vec4 p10Clr = (i1UseBorder || j0UseBorder) ? sampler.borderColor : lookup(access, i1, j0, offset.z());
	Vec4 p01Clr = (i0UseBorder || j1UseBorder) ? sampler.borderColor : lookup(access, i0, j1, offset.z());
	Vec4 p11Clr = (i1UseBorder || j1UseBorder) ? sampler.borderColor : lookup(access, i1, j1, offset.z());

	// Execute comparisons.
	float p00 = execCompare(p00Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
	float p10 = execCompare(p10Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
	float p01 = execCompare(p01Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
	float p11 = execCompare(p11Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);

	// Interpolate.
	return (p00*(1.0f-a)*(1.0f-b)) +
		   (p10*(     a)*(1.0f-b)) +
		   (p01*(1.0f-a)*(     b)) +
		   (p11*(     a)*(     b));
}

static Vec4 sampleLinear3D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, float w)
{
	int width	= access.getWidth();
	int height	= access.getHeight();
	int depth	= access.getDepth();

	int x0 = deFloorFloatToInt32(u-0.5f);
	int x1 = x0+1;
	int y0 = deFloorFloatToInt32(v-0.5f);
	int y1 = y0+1;
	int z0 = deFloorFloatToInt32(w-0.5f);
	int z1 = z0+1;

	int i0 = wrap(sampler.wrapS, x0, width);
	int i1 = wrap(sampler.wrapS, x1, width);
	int j0 = wrap(sampler.wrapT, y0, height);
	int j1 = wrap(sampler.wrapT, y1, height);
	int k0 = wrap(sampler.wrapR, z0, depth);
	int k1 = wrap(sampler.wrapR, z1, depth);

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);
	float c = deFloatFrac(w-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, width);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, width);
	bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, height);
	bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, height);
	bool k0UseBorder = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k0, 0, depth);
	bool k1UseBorder = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k1, 0, depth);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p000 = (i0UseBorder || j0UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i0, j0, k0);
	Vec4 p100 = (i1UseBorder || j0UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i1, j0, k0);
	Vec4 p010 = (i0UseBorder || j1UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i0, j1, k0);
	Vec4 p110 = (i1UseBorder || j1UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i1, j1, k0);
	Vec4 p001 = (i0UseBorder || j0UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i0, j0, k1);
	Vec4 p101 = (i1UseBorder || j0UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i1, j0, k1);
	Vec4 p011 = (i0UseBorder || j1UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i0, j1, k1);
	Vec4 p111 = (i1UseBorder || j1UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i1, j1, k1);

	// Interpolate.
	return (p000*(1.0f-a)*(1.0f-b)*(1.0f-c)) +
		   (p100*(     a)*(1.0f-b)*(1.0f-c)) +
		   (p010*(1.0f-a)*(     b)*(1.0f-c)) +
		   (p110*(     a)*(     b)*(1.0f-c)) +
		   (p001*(1.0f-a)*(1.0f-b)*(     c)) +
		   (p101*(     a)*(1.0f-b)*(     c)) +
		   (p011*(1.0f-a)*(     b)*(     c)) +
		   (p111*(     a)*(     b)*(     c));
}

static Vec4 sampleLinear3D (const ConstPixelBufferAccess& access, const Sampler& sampler, float u, float v, float w, const IVec3& offset)
{
	int width	= access.getWidth();
	int height	= access.getHeight();
	int depth	= access.getDepth();

	int x0 = deFloorFloatToInt32(u-0.5f)+offset.x();
	int x1 = x0+1;
	int y0 = deFloorFloatToInt32(v-0.5f)+offset.y();
	int y1 = y0+1;
	int z0 = deFloorFloatToInt32(w-0.5f)+offset.z();
	int z1 = z0+1;

	int i0 = wrap(sampler.wrapS, x0, width);
	int i1 = wrap(sampler.wrapS, x1, width);
	int j0 = wrap(sampler.wrapT, y0, height);
	int j1 = wrap(sampler.wrapT, y1, height);
	int k0 = wrap(sampler.wrapR, z0, depth);
	int k1 = wrap(sampler.wrapR, z1, depth);

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);
	float c = deFloatFrac(w-0.5f);

	bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, width);
	bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, width);
	bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, height);
	bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, height);
	bool k0UseBorder = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k0, 0, depth);
	bool k1UseBorder = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k1, 0, depth);

	// Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
	Vec4 p000 = (i0UseBorder || j0UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i0, j0, k0);
	Vec4 p100 = (i1UseBorder || j0UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i1, j0, k0);
	Vec4 p010 = (i0UseBorder || j1UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i0, j1, k0);
	Vec4 p110 = (i1UseBorder || j1UseBorder || k0UseBorder) ? sampler.borderColor : lookup(access, i1, j1, k0);
	Vec4 p001 = (i0UseBorder || j0UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i0, j0, k1);
	Vec4 p101 = (i1UseBorder || j0UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i1, j0, k1);
	Vec4 p011 = (i0UseBorder || j1UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i0, j1, k1);
	Vec4 p111 = (i1UseBorder || j1UseBorder || k1UseBorder) ? sampler.borderColor : lookup(access, i1, j1, k1);

	// Interpolate.
	return (p000*(1.0f-a)*(1.0f-b)*(1.0f-c)) +
		   (p100*(     a)*(1.0f-b)*(1.0f-c)) +
		   (p010*(1.0f-a)*(     b)*(1.0f-c)) +
		   (p110*(     a)*(     b)*(1.0f-c)) +
		   (p001*(1.0f-a)*(1.0f-b)*(     c)) +
		   (p101*(     a)*(1.0f-b)*(     c)) +
		   (p011*(1.0f-a)*(     b)*(     c)) +
		   (p111*(     a)*(     b)*(     c));
}

Vec4 ConstPixelBufferAccess::sample1D (const Sampler& sampler, Sampler::FilterMode filter, float s, int level) const
{
	DE_ASSERT(de::inBounds(level, 0, m_height));

	// Non-normalized coordinates.
	float u = s;

	if (sampler.normalizedCoords)
		u = unnormalize(sampler.wrapS, s, m_width);

	switch (filter)
	{
		case Sampler::NEAREST:	return sampleNearest1D	(*this, sampler, u, level);
		case Sampler::LINEAR:	return sampleLinear1D	(*this, sampler, u, level);
		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 ConstPixelBufferAccess::sample2D (const Sampler& sampler, Sampler::FilterMode filter, float s, float t, int depth) const
{
	DE_ASSERT(de::inBounds(depth, 0, m_depth));

	// Non-normalized coordinates.
	float u = s;
	float v = t;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, m_width);
		v = unnormalize(sampler.wrapT, t, m_height);
	}

	switch (filter)
	{
		case Sampler::NEAREST:	return sampleNearest2D	(*this, sampler, u, v, depth);
		case Sampler::LINEAR:	return sampleLinear2D	(*this, sampler, u, v, depth);
		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 ConstPixelBufferAccess::sample1DOffset (const Sampler& sampler, Sampler::FilterMode filter, float s, const IVec2& offset) const
{
	DE_ASSERT(de::inBounds(offset.y(), 0, m_width));

	// Non-normalized coordinates.
	float u = s;

	if (sampler.normalizedCoords)
		u = unnormalize(sampler.wrapS, s, m_width);

	switch (filter)
	{
		case Sampler::NEAREST:	return sampleNearest1D	(*this, sampler, u, offset);
		case Sampler::LINEAR:	return sampleLinear1D	(*this, sampler, u, offset);
		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 ConstPixelBufferAccess::sample2DOffset (const Sampler& sampler, Sampler::FilterMode filter, float s, float t, const IVec3& offset) const
{
	DE_ASSERT(de::inBounds(offset.z(), 0, m_depth));

	// Non-normalized coordinates.
	float u = s;
	float v = t;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, m_width);
		v = unnormalize(sampler.wrapT, t, m_height);
	}

	switch (filter)
	{
		case Sampler::NEAREST:	return sampleNearest2D	(*this, sampler, u, v, offset);
		case Sampler::LINEAR:	return sampleLinear2D	(*this, sampler, u, v, offset);
		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

float ConstPixelBufferAccess::sample1DCompare (const Sampler& sampler, Sampler::FilterMode filter, float ref, float s, const IVec2& offset) const
{
	DE_ASSERT(de::inBounds(offset.y(), 0, m_height));

	// Format information for comparison function
	const bool isFixedPointDepth = isFixedPointDepthTextureFormat(m_format);

	// Non-normalized coordinates.
	float u = s;

	if (sampler.normalizedCoords)
		u = unnormalize(sampler.wrapS, s, m_width);

	switch (filter)
	{
		case Sampler::NEAREST:	return execCompare(sampleNearest1D(*this, sampler, u, offset), sampler.compare, sampler.compareChannel, ref, isFixedPointDepth);
		case Sampler::LINEAR:	return sampleLinear1DCompare(*this, sampler, ref, u, offset, isFixedPointDepth);
		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

float ConstPixelBufferAccess::sample2DCompare (const Sampler& sampler, Sampler::FilterMode filter, float ref, float s, float t, const IVec3& offset) const
{
	DE_ASSERT(de::inBounds(offset.z(), 0, m_depth));

	// Format information for comparison function
	const bool isFixedPointDepth = isFixedPointDepthTextureFormat(m_format);

	// Non-normalized coordinates.
	float u = s;
	float v = t;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, m_width);
		v = unnormalize(sampler.wrapT, t, m_height);
	}

	switch (filter)
	{
		case Sampler::NEAREST:	return execCompare(sampleNearest2D(*this, sampler, u, v, offset), sampler.compare, sampler.compareChannel, ref, isFixedPointDepth);
		case Sampler::LINEAR:	return sampleLinear2DCompare(*this, sampler, ref, u, v, offset, isFixedPointDepth);
		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

Vec4 ConstPixelBufferAccess::sample3D (const Sampler& sampler, Sampler::FilterMode filter, float s, float t, float r) const
{
	// Non-normalized coordinates.
	float u = s;
	float v = t;
	float w = r;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, m_width);
		v = unnormalize(sampler.wrapT, t, m_height);
		w = unnormalize(sampler.wrapR, r, m_depth);
	}

	switch (filter)
	{
		case Sampler::NEAREST:	return sampleNearest3D	(*this, sampler, u, v, w);
		case Sampler::LINEAR:	return sampleLinear3D	(*this, sampler, u, v, w);
		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 ConstPixelBufferAccess::sample3DOffset (const Sampler& sampler, Sampler::FilterMode filter, float s, float t, float r, const IVec3& offset) const
{
	// Non-normalized coordinates.
	float u = s;
	float v = t;
	float w = r;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, m_width);
		v = unnormalize(sampler.wrapT, t, m_height);
		w = unnormalize(sampler.wrapR, r, m_depth);
	}

	switch (filter)
	{
		case Sampler::NEAREST:	return sampleNearest3D	(*this, sampler, u, v, w, offset);
		case Sampler::LINEAR:	return sampleLinear3D	(*this, sampler, u, v, w, offset);
		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

TextureLevel::TextureLevel (void)
	: m_format	()
	, m_width	(0)
	, m_height	(0)
	, m_depth	(0)
{
}

TextureLevel::TextureLevel (const TextureFormat& format)
	: m_format	(format)
	, m_width	(0)
	, m_height	(0)
	, m_depth	(0)
{
}

TextureLevel::TextureLevel (const TextureFormat& format, int width, int height, int depth)
	: m_format	(format)
	, m_width	(0)
	, m_height	(0)
	, m_depth	(0)
{
	setSize(width, height, depth);
}

TextureLevel::~TextureLevel (void)
{
}

void TextureLevel::setStorage (const TextureFormat& format, int width, int height, int depth)
{
	m_format = format;
	setSize(width, height, depth);
}

void TextureLevel::setSize (int width, int height, int depth)
{
	int pixelSize = m_format.getPixelSize();

	m_width		= width;
	m_height	= height;
	m_depth		= depth;

	m_data.setStorage(m_width*m_height*m_depth*pixelSize);
}

Vec4 sampleLevelArray1D (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float s, int depth, float lod)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample1D(sampler, filterMode, s, depth);
		case Sampler::LINEAR:	return levels[0].sample1D(sampler, filterMode, s, depth);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample1D(sampler, levelFilter, s, depth);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			tcu::Vec4			t0			= levels[level0].sample1D(sampler, levelFilter, s, depth);
			tcu::Vec4			t1			= levels[level1].sample1D(sampler, levelFilter, s, depth);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 sampleLevelArray1DOffset (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float s, float lod, const IVec2& offset)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample1DOffset(sampler, filterMode, s, offset);
		case Sampler::LINEAR:	return levels[0].sample1DOffset(sampler, filterMode, s, offset);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample1DOffset(sampler, levelFilter, s, offset);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			tcu::Vec4			t0			= levels[level0].sample1DOffset(sampler, levelFilter, s, offset);
			tcu::Vec4			t1			= levels[level1].sample1DOffset(sampler, levelFilter, s, offset);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 sampleLevelArray2D (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float s, float t, int depth, float lod)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample2D(sampler, filterMode, s, t, depth);
		case Sampler::LINEAR:	return levels[0].sample2D(sampler, filterMode, s, t, depth);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample2D(sampler, levelFilter, s, t, depth);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			tcu::Vec4			t0			= levels[level0].sample2D(sampler, levelFilter, s, t, depth);
			tcu::Vec4			t1			= levels[level1].sample2D(sampler, levelFilter, s, t, depth);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 sampleLevelArray2DOffset (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float s, float t, float lod, const IVec3& offset)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample2DOffset(sampler, filterMode, s, t, offset);
		case Sampler::LINEAR:	return levels[0].sample2DOffset(sampler, filterMode, s, t, offset);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample2DOffset(sampler, levelFilter, s, t, offset);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			tcu::Vec4			t0			= levels[level0].sample2DOffset(sampler, levelFilter, s, t, offset);
			tcu::Vec4			t1			= levels[level1].sample2DOffset(sampler, levelFilter, s, t, offset);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 sampleLevelArray3D (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float s, float t, float r, float lod)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample3D(sampler, filterMode, s, t, r);
		case Sampler::LINEAR:	return levels[0].sample3D(sampler, filterMode, s, t, r);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample3D(sampler, levelFilter, s, t, r);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			tcu::Vec4			t0			= levels[level0].sample3D(sampler, levelFilter, s, t, r);
			tcu::Vec4			t1			= levels[level1].sample3D(sampler, levelFilter, s, t, r);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

Vec4 sampleLevelArray3DOffset (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float s, float t, float r, float lod, const IVec3& offset)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample3DOffset(sampler, filterMode, s, t, r, offset);
		case Sampler::LINEAR:	return levels[0].sample3DOffset(sampler, filterMode, s, t, r, offset);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample3DOffset(sampler, levelFilter, s, t, r, offset);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			tcu::Vec4			t0			= levels[level0].sample3DOffset(sampler, levelFilter, s, t, r, offset);
			tcu::Vec4			t1			= levels[level1].sample3DOffset(sampler, levelFilter, s, t, r, offset);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

float sampleLevelArray1DCompare (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float ref, float s, float lod, const IVec2& offset)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample1DCompare(sampler, filterMode, ref, s, offset);
		case Sampler::LINEAR:	return levels[0].sample1DCompare(sampler, filterMode, ref, s, offset);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample1DCompare(sampler, levelFilter, ref, s, offset);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			float				t0			= levels[level0].sample1DCompare(sampler, levelFilter, ref, s, offset);
			float				t1			= levels[level1].sample1DCompare(sampler, levelFilter, ref, s, offset);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

float sampleLevelArray2DCompare (const ConstPixelBufferAccess* levels, int numLevels, const Sampler& sampler, float ref, float s, float t, float lod, const IVec3& offset)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:	return levels[0].sample2DCompare(sampler, filterMode, ref, s, t, offset);
		case Sampler::LINEAR:	return levels[0].sample2DCompare(sampler, filterMode, ref, s, t, offset);

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			return levels[level].sample2DCompare(sampler, levelFilter, ref, s, t, offset);
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int					maxLevel	= (int)numLevels-1;
			int					level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int					level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode	levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float				f			= deFloatFrac(lod);
			float				t0			= levels[level0].sample2DCompare(sampler, levelFilter, ref, s, t, offset);
			float				t1			= levels[level1].sample2DCompare(sampler, levelFilter, ref, s, t, offset);

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

Vec4 gatherArray2DOffsets (const ConstPixelBufferAccess& src, const Sampler& sampler, float s, float t, int depth, int componentNdx, const IVec2 (&offsets)[4])
{
	DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);
	DE_ASSERT(de::inBounds(componentNdx, 0, 4));

	const int		w	= src.getWidth();
	const int		h	= src.getHeight();
	const float		u	= unnormalize(sampler.wrapS, s, w);
	const float		v	= unnormalize(sampler.wrapT, t, h);
	const int		x0	= deFloorFloatToInt32(u-0.5f);
	const int		y0	= deFloorFloatToInt32(v-0.5f);

	IVec2 samplePositions[4];
	for (int i = 0; i < DE_LENGTH_OF_ARRAY(samplePositions); i++)
		samplePositions[i] = IVec2(wrap(sampler.wrapS, x0 + offsets[i].x(), w),
								   wrap(sampler.wrapT, y0 + offsets[i].y(), h));

	Vec4 result;
	for (int i = 0; i < 4; i++)
	{
		const Vec4 pixel = lookup(src, samplePositions[i].x(), samplePositions[i].y(), depth);
		result[i] = pixel[componentNdx];
	}

	return result;
}

Vec4 gatherArray2DOffsetsCompare (const ConstPixelBufferAccess& src, const Sampler& sampler, float ref, float s, float t, int depth, const IVec2 (&offsets)[4])
{
	DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);
	DE_ASSERT(src.getFormat().order == TextureFormat::D || src.getFormat().order == TextureFormat::DS);
	DE_ASSERT(sampler.compareChannel == 0);

	Sampler noCompareSampler = sampler;
	noCompareSampler.compare = Sampler::COMPAREMODE_NONE;

	const Vec4	gathered			= gatherArray2DOffsets(src, noCompareSampler, s, t, depth, 0 /* component 0: depth */, offsets);
	const bool	isFixedPoint		= isFixedPointDepthTextureFormat(src.getFormat());
	Vec4 result;
	for (int i = 0; i < 4; i++)
		result[i] = execCompare(gathered, sampler.compare, i, ref, isFixedPoint);

	return result;
}

static Vec4 sampleCubeSeamlessNearest (const ConstPixelBufferAccess& faceAccess, const Sampler& sampler, float s, float t, int depth)
{
	Sampler clampingSampler = sampler;
	clampingSampler.wrapS = Sampler::CLAMP_TO_EDGE;
	clampingSampler.wrapT = Sampler::CLAMP_TO_EDGE;
	return faceAccess.sample2D(clampingSampler, Sampler::NEAREST, s, t, depth);
}

CubeFace selectCubeFace (const Vec3& coords)
{
	const float	x	= coords.x();
	const float	y	= coords.y();
	const float	z	= coords.z();
	const float	ax	= deFloatAbs(x);
	const float	ay	= deFloatAbs(y);
	const float	az	= deFloatAbs(z);

	if (ay < ax && az < ax)
		return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
	else if (ax < ay && az < ay)
		return y >= 0.0f ? CUBEFACE_POSITIVE_Y : CUBEFACE_NEGATIVE_Y;
	else if (ax < az && ay < az)
		return z >= 0.0f ? CUBEFACE_POSITIVE_Z : CUBEFACE_NEGATIVE_Z;
	else
	{
		// Some of the components are equal. Use tie-breaking rule.
		if (ax == ay)
		{
			if (ax < az)
				return z >= 0.0f ? CUBEFACE_POSITIVE_Z : CUBEFACE_NEGATIVE_Z;
			else
				return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
		}
		else if (ax == az)
		{
			if (az < ay)
				return y >= 0.0f ? CUBEFACE_POSITIVE_Y : CUBEFACE_NEGATIVE_Y;
			else
				return z >= 0.0f ? CUBEFACE_POSITIVE_Z : CUBEFACE_NEGATIVE_Z;
		}
		else if (ay == az)
		{
			if (ay < ax)
				return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
			else
				return y >= 0.0f ? CUBEFACE_POSITIVE_Y : CUBEFACE_NEGATIVE_Y;
		}
		else
			return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
	}
}

Vec2 projectToFace (CubeFace face, const Vec3& coord)
{
	const float	rx		= coord.x();
	const float	ry		= coord.y();
	const float	rz		= coord.z();
	float		sc		= 0.0f;
	float		tc		= 0.0f;
	float		ma		= 0.0f;
	float		s;
	float		t;

	switch (face)
	{
		case CUBEFACE_NEGATIVE_X: sc = +rz; tc = -ry; ma = -rx; break;
		case CUBEFACE_POSITIVE_X: sc = -rz; tc = -ry; ma = +rx; break;
		case CUBEFACE_NEGATIVE_Y: sc = +rx; tc = -rz; ma = -ry; break;
		case CUBEFACE_POSITIVE_Y: sc = +rx; tc = +rz; ma = +ry; break;
		case CUBEFACE_NEGATIVE_Z: sc = -rx; tc = -ry; ma = -rz; break;
		case CUBEFACE_POSITIVE_Z: sc = +rx; tc = -ry; ma = +rz; break;
		default:
			DE_ASSERT(DE_FALSE);
	}

	// Compute s, t
	s = ((sc / ma) + 1.0f) / 2.0f;
	t = ((tc / ma) + 1.0f) / 2.0f;

	return Vec2(s, t);
}

CubeFaceFloatCoords getCubeFaceCoords (const Vec3& coords)
{
	const CubeFace face = selectCubeFace(coords);
	return CubeFaceFloatCoords(face, projectToFace(face, coords));
}

// Checks if origCoords.coords is in bounds defined by size; if not, return a CubeFaceIntCoords with face set to the appropriate neighboring face and coords transformed accordingly.
// \note If both x and y in origCoords.coords are out of bounds, this returns with face CUBEFACE_LAST, signifying that there is no unique neighboring face.
CubeFaceIntCoords remapCubeEdgeCoords (const CubeFaceIntCoords& origCoords, int size)
{
	bool uInBounds = de::inBounds(origCoords.s, 0, size);
	bool vInBounds = de::inBounds(origCoords.t, 0, size);

	if (uInBounds && vInBounds)
		return origCoords;

	if (!uInBounds && !vInBounds)
		return CubeFaceIntCoords(CUBEFACE_LAST, -1, -1);

	IVec2 coords(wrap(Sampler::CLAMP_TO_BORDER, origCoords.s, size),
				 wrap(Sampler::CLAMP_TO_BORDER, origCoords.t, size));
	IVec3 canonizedCoords;

	// Map the uv coordinates to canonized 3d coordinates.

	switch (origCoords.face)
	{
		case CUBEFACE_NEGATIVE_X: canonizedCoords = IVec3(0,					size-1-coords.y(),	coords.x());			break;
		case CUBEFACE_POSITIVE_X: canonizedCoords = IVec3(size-1,				size-1-coords.y(),	size-1-coords.x());		break;
		case CUBEFACE_NEGATIVE_Y: canonizedCoords = IVec3(coords.x(),			0,					size-1-coords.y());		break;
		case CUBEFACE_POSITIVE_Y: canonizedCoords = IVec3(coords.x(),			size-1,				coords.y());			break;
		case CUBEFACE_NEGATIVE_Z: canonizedCoords = IVec3(size-1-coords.x(),	size-1-coords.y(),	0);						break;
		case CUBEFACE_POSITIVE_Z: canonizedCoords = IVec3(coords.x(),			size-1-coords.y(),	size-1);				break;
		default: DE_ASSERT(false);
	}

	// Find an appropriate face to re-map the coordinates to.

	if (canonizedCoords.x() == -1)
		return CubeFaceIntCoords(CUBEFACE_NEGATIVE_X, IVec2(canonizedCoords.z(), size-1-canonizedCoords.y()));

	if (canonizedCoords.x() == size)
		return CubeFaceIntCoords(CUBEFACE_POSITIVE_X, IVec2(size-1-canonizedCoords.z(), size-1-canonizedCoords.y()));

	if (canonizedCoords.y() == -1)
		return CubeFaceIntCoords(CUBEFACE_NEGATIVE_Y, IVec2(canonizedCoords.x(), size-1-canonizedCoords.z()));

	if (canonizedCoords.y() == size)
		return CubeFaceIntCoords(CUBEFACE_POSITIVE_Y, IVec2(canonizedCoords.x(), canonizedCoords.z()));

	if (canonizedCoords.z() == -1)
		return CubeFaceIntCoords(CUBEFACE_NEGATIVE_Z, IVec2(size-1-canonizedCoords.x(), size-1-canonizedCoords.y()));

	if (canonizedCoords.z() == size)
		return CubeFaceIntCoords(CUBEFACE_POSITIVE_Z, IVec2(canonizedCoords.x(), size-1-canonizedCoords.y()));

	DE_ASSERT(false);
	return CubeFaceIntCoords(CUBEFACE_LAST, IVec2(-1));
}

static void getCubeLinearSamples (const ConstPixelBufferAccess (&faceAccesses)[CUBEFACE_LAST], CubeFace baseFace, float u, float v, int depth, Vec4 (&dst)[4])
{
	DE_ASSERT(faceAccesses[0].getWidth() == faceAccesses[0].getHeight());
	int		size					= faceAccesses[0].getWidth();
	int		x0						= deFloorFloatToInt32(u-0.5f);
	int		x1						= x0+1;
	int		y0						= deFloorFloatToInt32(v-0.5f);
	int		y1						= y0+1;
	IVec2	baseSampleCoords[4]		=
	{
		IVec2(x0, y0),
		IVec2(x1, y0),
		IVec2(x0, y1),
		IVec2(x1, y1)
	};
	Vec4	sampleColors[4];
	bool	hasBothCoordsOutOfBounds[4]; //!< Whether correctCubeFace() returns CUBEFACE_LAST, i.e. both u and v are out of bounds.

	// Find correct faces and coordinates for out-of-bounds sample coordinates.

	for (int i = 0; i < 4; i++)
	{
		CubeFaceIntCoords coords = remapCubeEdgeCoords(CubeFaceIntCoords(baseFace, baseSampleCoords[i]), size);
		hasBothCoordsOutOfBounds[i] = coords.face == CUBEFACE_LAST;
		if (!hasBothCoordsOutOfBounds[i])
			sampleColors[i] = lookup(faceAccesses[coords.face], coords.s, coords.t, depth);
	}

	// If a sample was out of bounds in both u and v, we get its color from the average of the three other samples.
	// \note This averaging behavior is not required by the GLES3 spec (though it is recommended). GLES3 spec only
	//		 requires that if the three other samples all have the same color, then the doubly-out-of-bounds sample
	//		 must have this color as well.

	{
		int bothOutOfBoundsNdx = -1;
		for (int i = 0; i < 4; i++)
		{
			if (hasBothCoordsOutOfBounds[i])
			{
				DE_ASSERT(bothOutOfBoundsNdx < 0); // Only one sample can be out of bounds in both u and v.
				bothOutOfBoundsNdx = i;
			}
		}
		if (bothOutOfBoundsNdx != -1)
		{
			sampleColors[bothOutOfBoundsNdx] = Vec4(0.0f);
			for (int i = 0; i < 4; i++)
				if (i != bothOutOfBoundsNdx)
					sampleColors[bothOutOfBoundsNdx] += sampleColors[i];

			sampleColors[bothOutOfBoundsNdx] = sampleColors[bothOutOfBoundsNdx] * (1.0f/3.0f);
		}
	}

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(sampleColors); i++)
		dst[i] = sampleColors[i];
}

// \todo [2014-02-19 pyry] Optimize faceAccesses
static Vec4 sampleCubeSeamlessLinear (const ConstPixelBufferAccess (&faceAccesses)[CUBEFACE_LAST], CubeFace baseFace, const Sampler& sampler, float s, float t, int depth)
{
	DE_ASSERT(faceAccesses[0].getWidth() == faceAccesses[0].getHeight());

	int		size	= faceAccesses[0].getWidth();
	// Non-normalized coordinates.
	float	u		= s;
	float	v		= t;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, size);
		v = unnormalize(sampler.wrapT, t, size);
	}

	// Get sample colors.

	Vec4 sampleColors[4];
	getCubeLinearSamples(faceAccesses, baseFace, u, v, depth, sampleColors);

	// Interpolate.

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);

	return (sampleColors[0]*(1.0f-a)*(1.0f-b)) +
		   (sampleColors[1]*(     a)*(1.0f-b)) +
		   (sampleColors[2]*(1.0f-a)*(     b)) +
		   (sampleColors[3]*(     a)*(     b));
}

static Vec4 sampleLevelArrayCubeSeamless (const ConstPixelBufferAccess* const (&faces)[CUBEFACE_LAST], int numLevels, CubeFace face, const Sampler& sampler, float s, float t, int depth, float lod)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:
			return sampleCubeSeamlessNearest(faces[face][0], sampler, s, t, depth);

		case Sampler::LINEAR:
		{
			ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
			for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				faceAccesses[i] = faces[i][0];

			return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, depth);
		}

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			if (levelFilter == Sampler::NEAREST)
				return sampleCubeSeamlessNearest(faces[face][level], sampler, s, t, depth);
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
					faceAccesses[i] = faces[i][level];

				return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, depth);
			}
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int						level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float					f			= deFloatFrac(lod);
			Vec4					t0;
			Vec4					t1;

			if (levelFilter == Sampler::NEAREST)
			{
				t0 = sampleCubeSeamlessNearest(faces[face][level0], sampler, s, t, depth);
				t1 = sampleCubeSeamlessNearest(faces[face][level1], sampler, s, t, depth);
			}
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
				ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				{
					faceAccesses0[i] = faces[i][level0];
					faceAccesses1[i] = faces[i][level1];
				}

				t0 = sampleCubeSeamlessLinear(faceAccesses0, face, sampler, s, t, depth);
				t1 = sampleCubeSeamlessLinear(faceAccesses1, face, sampler, s, t, depth);
			}

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

static float sampleCubeSeamlessNearestCompare (const ConstPixelBufferAccess& faceAccess, const Sampler& sampler, float ref, float s, float t, int depth = 0)
{
	Sampler clampingSampler = sampler;
	clampingSampler.wrapS = Sampler::CLAMP_TO_EDGE;
	clampingSampler.wrapT = Sampler::CLAMP_TO_EDGE;
	return faceAccess.sample2DCompare(clampingSampler, Sampler::NEAREST, ref, s, t, IVec3(0, 0, depth));
}

static float sampleCubeSeamlessLinearCompare (const ConstPixelBufferAccess (&faceAccesses)[CUBEFACE_LAST], CubeFace baseFace, const Sampler& sampler, float ref, float s, float t)
{
	DE_ASSERT(faceAccesses[0].getWidth() == faceAccesses[0].getHeight());

	int		size	= faceAccesses[0].getWidth();
	// Non-normalized coordinates.
	float	u		= s;
	float	v		= t;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, s, size);
		v = unnormalize(sampler.wrapT, t, size);
	}

	int			x0						= deFloorFloatToInt32(u-0.5f);
	int			x1						= x0+1;
	int			y0						= deFloorFloatToInt32(v-0.5f);
	int			y1						= y0+1;
	IVec2		baseSampleCoords[4]		=
	{
		IVec2(x0, y0),
		IVec2(x1, y0),
		IVec2(x0, y1),
		IVec2(x1, y1)
	};
	float		sampleRes[4];
	bool		hasBothCoordsOutOfBounds[4]; //!< Whether correctCubeFace() returns CUBEFACE_LAST, i.e. both u and v are out of bounds.

	// Find correct faces and coordinates for out-of-bounds sample coordinates.

	for (int i = 0; i < 4; i++)
	{
		CubeFaceIntCoords coords = remapCubeEdgeCoords(CubeFaceIntCoords(baseFace, baseSampleCoords[i]), size);
		hasBothCoordsOutOfBounds[i] = coords.face == CUBEFACE_LAST;

		if (!hasBothCoordsOutOfBounds[i])
		{
			const bool isFixedPointDepth = isFixedPointDepthTextureFormat(faceAccesses[coords.face].getFormat());

			sampleRes[i] = execCompare(faceAccesses[coords.face].getPixel(coords.s, coords.t), sampler.compare, sampler.compareChannel, ref, isFixedPointDepth);
		}
	}

	// If a sample was out of bounds in both u and v, we get its color from the average of the three other samples.
	// \note This averaging behavior is not required by the GLES3 spec (though it is recommended). GLES3 spec only
	//		 requires that if the three other samples all have the same color, then the doubly-out-of-bounds sample
	//		 must have this color as well.

	{
		int bothOutOfBoundsNdx = -1;
		for (int i = 0; i < 4; i++)
		{
			if (hasBothCoordsOutOfBounds[i])
			{
				DE_ASSERT(bothOutOfBoundsNdx < 0); // Only one sample can be out of bounds in both u and v.
				bothOutOfBoundsNdx = i;
			}
		}
		if (bothOutOfBoundsNdx != -1)
		{
			sampleRes[bothOutOfBoundsNdx] = 0.0f;
			for (int i = 0; i < 4; i++)
				if (i != bothOutOfBoundsNdx)
					sampleRes[bothOutOfBoundsNdx] += sampleRes[i];

			sampleRes[bothOutOfBoundsNdx] = sampleRes[bothOutOfBoundsNdx] * (1.0f/3.0f);
		}
	}

	// Interpolate.

	float a = deFloatFrac(u-0.5f);
	float b = deFloatFrac(v-0.5f);

	return (sampleRes[0]*(1.0f-a)*(1.0f-b)) +
		   (sampleRes[1]*(     a)*(1.0f-b)) +
		   (sampleRes[2]*(1.0f-a)*(     b)) +
		   (sampleRes[3]*(     a)*(     b));
}

static float sampleLevelArrayCubeSeamlessCompare (const ConstPixelBufferAccess* const (&faces)[CUBEFACE_LAST], int numLevels, CubeFace face, const Sampler& sampler, float ref, float s, float t, float lod)
{
	bool					magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode		filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:
			return sampleCubeSeamlessNearestCompare(faces[face][0], sampler, ref, s, t);

		case Sampler::LINEAR:
		{
			ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
			for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				faceAccesses[i] = faces[i][0];

			return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
		}

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			if (levelFilter == Sampler::NEAREST)
				return sampleCubeSeamlessNearestCompare(faces[face][level], sampler, ref, s, t);
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
					faceAccesses[i] = faces[i][level];

				return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
			}
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int						level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float					f			= deFloatFrac(lod);
			float					t0;
			float					t1;

			if (levelFilter == Sampler::NEAREST)
			{
				t0 = sampleCubeSeamlessNearestCompare(faces[face][level0], sampler, ref, s, t);
				t1 = sampleCubeSeamlessNearestCompare(faces[face][level1], sampler, ref, s, t);
			}
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
				ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				{
					faceAccesses0[i] = faces[i][level0];
					faceAccesses1[i] = faces[i][level1];
				}

				t0 = sampleCubeSeamlessLinearCompare(faceAccesses0, face, sampler, ref, s, t);
				t1 = sampleCubeSeamlessLinearCompare(faceAccesses1, face, sampler, ref, s, t);
			}

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

// Cube map array sampling

static inline ConstPixelBufferAccess getCubeArrayFaceAccess (const ConstPixelBufferAccess* const levels, int levelNdx, int slice, CubeFace face)
{
	const ConstPixelBufferAccess&	level	= levels[levelNdx];
	const int						depth	= (slice * 6) + getCubeArrayFaceIndex(face);

	return getSubregion(level, 0, 0, depth, level.getWidth(), level.getHeight(), 1);
}

static Vec4 sampleCubeArraySeamless (const ConstPixelBufferAccess* const levels, int numLevels, int slice, CubeFace face, const Sampler& sampler, float s, float t, float lod)
{
	const int					faceDepth	= (slice * 6) + getCubeArrayFaceIndex(face);
	const bool					magnified	= lod <= sampler.lodThreshold;
	const Sampler::FilterMode	filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:
			return sampleCubeSeamlessNearest(levels[0], sampler, s, t, faceDepth);

		case Sampler::LINEAR:
		{
			ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
			for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				faceAccesses[i] = getCubeArrayFaceAccess(levels, 0, slice, (CubeFace)i);

			return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, 0);
		}

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			if (levelFilter == Sampler::NEAREST)
				return sampleCubeSeamlessNearest(levels[level], sampler, s, t, faceDepth);
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
					faceAccesses[i] = getCubeArrayFaceAccess(levels, level, slice, (CubeFace)i);

				return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, 0);
			}
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int						level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float					f			= deFloatFrac(lod);
			Vec4					t0;
			Vec4					t1;

			if (levelFilter == Sampler::NEAREST)
			{
				t0 = sampleCubeSeamlessNearest(levels[level0], sampler, s, t, faceDepth);
				t1 = sampleCubeSeamlessNearest(levels[level1], sampler, s, t, faceDepth);
			}
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
				ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				{
					faceAccesses0[i] = getCubeArrayFaceAccess(levels, level0, slice, (CubeFace)i);
					faceAccesses1[i] = getCubeArrayFaceAccess(levels, level1, slice, (CubeFace)i);
				}

				t0 = sampleCubeSeamlessLinear(faceAccesses0, face, sampler, s, t, 0);
				t1 = sampleCubeSeamlessLinear(faceAccesses1, face, sampler, s, t, 0);
			}

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return Vec4(0.0f);
	}
}

static float sampleCubeArraySeamlessCompare (const ConstPixelBufferAccess* const levels, int numLevels, int slice, CubeFace face, const Sampler& sampler, float ref, float s, float t, float lod)
{
	const int			faceDepth	= (slice * 6) + getCubeArrayFaceIndex(face);
	const bool			magnified	= lod <= sampler.lodThreshold;
	Sampler::FilterMode	filterMode	= magnified ? sampler.magFilter : sampler.minFilter;

	switch (filterMode)
	{
		case Sampler::NEAREST:
			return sampleCubeSeamlessNearestCompare(levels[0], sampler, ref, s, t, faceDepth);

		case Sampler::LINEAR:
		{
			ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
			for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				faceAccesses[i] = getCubeArrayFaceAccess(levels, 0, slice, (CubeFace)i);

			return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
		}

		case Sampler::NEAREST_MIPMAP_NEAREST:
		case Sampler::LINEAR_MIPMAP_NEAREST:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level		= deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

			if (levelFilter == Sampler::NEAREST)
				return sampleCubeSeamlessNearestCompare(levels[level], sampler, ref, s, t, faceDepth);
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
					faceAccesses[i] = getCubeArrayFaceAccess(levels, level, slice, (CubeFace)i);

				return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
			}
		}

		case Sampler::NEAREST_MIPMAP_LINEAR:
		case Sampler::LINEAR_MIPMAP_LINEAR:
		{
			int						maxLevel	= (int)numLevels-1;
			int						level0		= deClamp32((int)deFloatFloor(lod), 0, maxLevel);
			int						level1		= de::min(maxLevel, level0 + 1);
			Sampler::FilterMode		levelFilter	= (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
			float					f			= deFloatFrac(lod);
			float					t0;
			float					t1;

			if (levelFilter == Sampler::NEAREST)
			{
				t0 = sampleCubeSeamlessNearestCompare(levels[level0], sampler, ref, s, t, faceDepth);
				t1 = sampleCubeSeamlessNearestCompare(levels[level1], sampler, ref, s, t, faceDepth);
			}
			else
			{
				DE_ASSERT(levelFilter == Sampler::LINEAR);

				ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
				ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
				for (int i = 0; i < (int)CUBEFACE_LAST; i++)
				{
					faceAccesses0[i] = getCubeArrayFaceAccess(levels, level0, slice, (CubeFace)i);
					faceAccesses1[i] = getCubeArrayFaceAccess(levels, level1, slice, (CubeFace)i);
				}

				t0 = sampleCubeSeamlessLinearCompare(faceAccesses0, face, sampler, ref, s, t);
				t1 = sampleCubeSeamlessLinearCompare(faceAccesses1, face, sampler, ref, s, t);
			}

			return t0*(1.0f - f) + t1*f;
		}

		default:
			DE_ASSERT(DE_FALSE);
			return 0.0f;
	}
}

inline int computeMipPyramidLevels (int size)
{
	return deLog2Floor32(size)+1;
}

inline int computeMipPyramidLevels (int width, int height)
{
	return deLog2Floor32(de::max(width, height))+1;
}

inline int computeMipPyramidLevels (int width, int height, int depth)
{
	return deLog2Floor32(de::max(width, de::max(height, depth)))+1;
}

inline int getMipPyramidLevelSize (int baseLevelSize, int levelNdx)
{
	return de::max(baseLevelSize >> levelNdx, 1);
}

// TextureLevelPyramid

TextureLevelPyramid::TextureLevelPyramid (const TextureFormat& format, int numLevels)
	: m_format	(format)
	, m_data	(numLevels)
	, m_access	(numLevels)
{
}

TextureLevelPyramid::TextureLevelPyramid (const TextureLevelPyramid& other)
	: m_format	(other.m_format)
	, m_data	(other.getNumLevels())
	, m_access	(other.getNumLevels())
{
	for (int levelNdx = 0; levelNdx < other.getNumLevels(); levelNdx++)
	{
		if (!other.isLevelEmpty(levelNdx))
		{
			const tcu::ConstPixelBufferAccess& srcLevel = other.getLevel(levelNdx);

			m_data[levelNdx] = other.m_data[levelNdx];
			m_access[levelNdx] = PixelBufferAccess(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(), srcLevel.getDepth(), m_data[levelNdx].getPtr());
		}
	}
}

TextureLevelPyramid& TextureLevelPyramid::operator= (const TextureLevelPyramid& other)
{
	if (this == &other)
		return *this;

	m_format = other.m_format;
	m_data.resize(other.getNumLevels());
	m_access.resize(other.getNumLevels());

	for (int levelNdx = 0; levelNdx < other.getNumLevels(); levelNdx++)
	{
		if (!other.isLevelEmpty(levelNdx))
		{
			const tcu::ConstPixelBufferAccess& srcLevel = other.getLevel(levelNdx);

			m_data[levelNdx] = other.m_data[levelNdx];
			m_access[levelNdx] = PixelBufferAccess(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(), srcLevel.getDepth(), m_data[levelNdx].getPtr());
		}
		else if (!isLevelEmpty(levelNdx))
			clearLevel(levelNdx);
	}

	return *this;
}

TextureLevelPyramid::~TextureLevelPyramid (void)
{
}

void TextureLevelPyramid::allocLevel (int levelNdx, int width, int height, int depth)
{
	const int	size	= m_format.getPixelSize()*width*height*depth;

	DE_ASSERT(isLevelEmpty(levelNdx));

	m_data[levelNdx].setStorage(size);
	m_access[levelNdx] = PixelBufferAccess(m_format, width, height, depth, m_data[levelNdx].getPtr());
}

void TextureLevelPyramid::clearLevel (int levelNdx)
{
	DE_ASSERT(!isLevelEmpty(levelNdx));

	m_data[levelNdx].clear();
	m_access[levelNdx] = PixelBufferAccess();
}

// Texture1D

Texture1D::Texture1D (const TextureFormat& format, int width)
	: TextureLevelPyramid	(format, computeMipPyramidLevels(width))
	, m_width				(width)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture1D::Texture1D (const Texture1D& other)
	: TextureLevelPyramid	(other)
	, m_width				(other.m_width)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture1D& Texture1D::operator= (const Texture1D& other)
{
	if (this == &other)
		return *this;

	TextureLevelPyramid::operator=(other);

	m_width		= other.m_width;
	m_view		= Texture1DView(getNumLevels(), getLevels());

	return *this;
}

Texture1D::~Texture1D (void)
{
}

void Texture1D::allocLevel (int levelNdx)
{
	DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

	const int width = getMipPyramidLevelSize(m_width, levelNdx);

	TextureLevelPyramid::allocLevel(levelNdx, width, 1, 1);
}

// Texture2D

Texture2D::Texture2D (const TextureFormat& format, int width, int height)
	: TextureLevelPyramid	(format, computeMipPyramidLevels(width, height))
	, m_width				(width)
	, m_height				(height)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture2D::Texture2D (const Texture2D& other)
	: TextureLevelPyramid	(other)
	, m_width				(other.m_width)
	, m_height				(other.m_height)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture2D& Texture2D::operator= (const Texture2D& other)
{
	if (this == &other)
		return *this;

	TextureLevelPyramid::operator=(other);

	m_width		= other.m_width;
	m_height	= other.m_height;
	m_view		= Texture2DView(getNumLevels(), getLevels());

	return *this;
}

Texture2D::~Texture2D (void)
{
}

void Texture2D::allocLevel (int levelNdx)
{
	DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

	const int	width	= getMipPyramidLevelSize(m_width, levelNdx);
	const int	height	= getMipPyramidLevelSize(m_height, levelNdx);

	TextureLevelPyramid::allocLevel(levelNdx, width, height, 1);
}

// TextureCubeView

TextureCubeView::TextureCubeView (void)
	: m_numLevels(0)
{
	for (int ndx = 0; ndx < CUBEFACE_LAST; ndx++)
		m_levels[ndx] = DE_NULL;
}

TextureCubeView::TextureCubeView (int numLevels, const ConstPixelBufferAccess* const (&levels) [CUBEFACE_LAST])
	: m_numLevels(numLevels)
{
	for (int ndx = 0; ndx < CUBEFACE_LAST; ndx++)
		m_levels[ndx] = levels[ndx];
}

tcu::Vec4 TextureCubeView::sample (const Sampler& sampler, float s, float t, float r, float lod) const
{
	DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);

	// Computes (face, s, t).
	const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
	if (sampler.seamlessCubeMap)
		return sampleLevelArrayCubeSeamless(m_levels, m_numLevels, coords.face, sampler, coords.s, coords.t, 0 /* depth */, lod);
	else
		return sampleLevelArray2D(m_levels[coords.face], m_numLevels, sampler, coords.s, coords.t, 0 /* depth */, lod);
}

float TextureCubeView::sampleCompare (const Sampler& sampler, float ref, float s, float t, float r, float lod) const
{
	DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);

	// Computes (face, s, t).
	const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
	if (sampler.seamlessCubeMap)
		return sampleLevelArrayCubeSeamlessCompare(m_levels, m_numLevels, coords.face, sampler, ref, coords.s, coords.t, lod);
	else
		return sampleLevelArray2DCompare(m_levels[coords.face], m_numLevels, sampler, ref, coords.s, coords.t, lod, IVec3(0, 0, 0));
}

Vec4 TextureCubeView::gather (const Sampler& sampler, float s, float t, float r, int componentNdx) const
{
	DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);

	ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
	for (int i = 0; i < (int)CUBEFACE_LAST; i++)
		faceAccesses[i] = m_levels[i][0];

	const CubeFaceFloatCoords	coords	= getCubeFaceCoords(Vec3(s, t, r));
	const int					size	= faceAccesses[0].getWidth();
	// Non-normalized coordinates.
	float						u		= coords.s;
	float						v		= coords.t;

	if (sampler.normalizedCoords)
	{
		u = unnormalize(sampler.wrapS, coords.s, size);
		v = unnormalize(sampler.wrapT, coords.t, size);
	}

	Vec4 sampleColors[4];
	getCubeLinearSamples(faceAccesses, coords.face, u, v, 0, sampleColors);

	const int	sampleIndices[4] = { 2, 3, 1, 0 }; // \note Gather returns the samples in a non-obvious order.
	Vec4		result;
	for (int i = 0; i < 4; i++)
		result[i] = sampleColors[sampleIndices[i]][componentNdx];

	return result;
}

Vec4 TextureCubeView::gatherCompare (const Sampler& sampler, float ref, float s, float t, float r) const
{
	DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);
	DE_ASSERT(m_levels[0][0].getFormat().order == TextureFormat::D || m_levels[0][0].getFormat().order == TextureFormat::DS);
	DE_ASSERT(sampler.compareChannel == 0);

	Sampler noCompareSampler = sampler;
	noCompareSampler.compare = Sampler::COMPAREMODE_NONE;

	const Vec4 gathered			= gather(noCompareSampler, s, t, r, 0 /* component 0: depth */);
	const bool isFixedPoint		= isFixedPointDepthTextureFormat(m_levels[0][0].getFormat());
	Vec4 result;
	for (int i = 0; i < 4; i++)
		result[i] = execCompare(gathered, sampler.compare, i, ref, isFixedPoint);

	return result;
}

// TextureCube

TextureCube::TextureCube (const TextureFormat& format, int size)
	: m_format	(format)
	, m_size	(size)
{
	const int						numLevels		= computeMipPyramidLevels(m_size);
	const ConstPixelBufferAccess*	levels[CUBEFACE_LAST];

	for (int face = 0; face < CUBEFACE_LAST; face++)
	{
		m_data[face].resize(numLevels);
		m_access[face].resize(numLevels);
		levels[face] = &m_access[face][0];
	}

	m_view = TextureCubeView(numLevels, levels);
}

TextureCube::TextureCube (const TextureCube& other)
	: m_format	(other.m_format)
	, m_size	(other.m_size)
{
	const int						numLevels		= computeMipPyramidLevels(m_size);
	const ConstPixelBufferAccess*	levels[CUBEFACE_LAST];

	for (int face = 0; face < CUBEFACE_LAST; face++)
	{
		m_data[face].resize(numLevels);
		m_access[face].resize(numLevels);
		levels[face] = &m_access[face][0];
	}

	m_view = TextureCubeView(numLevels, levels);

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		for (int face = 0; face < CUBEFACE_LAST; face++)
		{
			if (!other.isLevelEmpty((CubeFace)face, levelNdx))
			{
				allocLevel((CubeFace)face, levelNdx);
				copy(getLevelFace(levelNdx, (CubeFace)face),
					 other.getLevelFace(levelNdx, (CubeFace)face));
			}
		}
	}
}

TextureCube& TextureCube::operator= (const TextureCube& other)
{
	if (this == &other)
		return *this;

	const int						numLevels		= computeMipPyramidLevels(other.m_size);
	const ConstPixelBufferAccess*	levels[CUBEFACE_LAST];

	for (int face = 0; face < CUBEFACE_LAST; face++)
	{
		m_data[face].resize(numLevels);
		m_access[face].resize(numLevels);
		levels[face] = &m_access[face][0];
	}

	m_format	= other.m_format;
	m_size		= other.m_size;
	m_view		= TextureCubeView(numLevels, levels);

	for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
	{
		for (int face = 0; face < CUBEFACE_LAST; face++)
		{
			if (!isLevelEmpty((CubeFace)face, levelNdx))
				clearLevel((CubeFace)face, levelNdx);

			if (!other.isLevelEmpty((CubeFace)face, levelNdx))
			{
				allocLevel((CubeFace)face, levelNdx);
				copy(getLevelFace(levelNdx, (CubeFace)face),
					 other.getLevelFace(levelNdx, (CubeFace)face));
			}
		}
	}

	return *this;
}

TextureCube::~TextureCube (void)
{
}

void TextureCube::allocLevel (tcu::CubeFace face, int levelNdx)
{
	const int	size		= getMipPyramidLevelSize(m_size, levelNdx);
	const int	dataSize	= m_format.getPixelSize()*size*size;
	DE_ASSERT(isLevelEmpty(face, levelNdx));

	m_data[face][levelNdx].setStorage(dataSize);
	m_access[face][levelNdx] = PixelBufferAccess(m_format, size, size, 1, m_data[face][levelNdx].getPtr());
}

void TextureCube::clearLevel (tcu::CubeFace face, int levelNdx)
{
	DE_ASSERT(!isLevelEmpty(face, levelNdx));
	m_data[face][levelNdx].clear();
	m_access[face][levelNdx] = PixelBufferAccess();
}

// Texture1DArrayView

Texture1DArrayView::Texture1DArrayView (int numLevels, const ConstPixelBufferAccess* levels)
	: m_numLevels	(numLevels)
	, m_levels		(levels)
{
}

inline int Texture1DArrayView::selectLayer (float r) const
{
	DE_ASSERT(m_numLevels > 0 && m_levels);
	return de::clamp(deFloorFloatToInt32(r + 0.5f), 0, m_levels[0].getHeight()-1);
}

Vec4 Texture1DArrayView::sample (const Sampler& sampler, float s, float t, float lod) const
{
	return sampleLevelArray1D(m_levels, m_numLevels, sampler, s, selectLayer(t), lod);
}

Vec4 Texture1DArrayView::sampleOffset (const Sampler& sampler, float s, float t, float lod, deInt32 offset) const
{
	return sampleLevelArray1DOffset(m_levels, m_numLevels, sampler, s, lod, IVec2(offset, selectLayer(t)));
}

float Texture1DArrayView::sampleCompare (const Sampler& sampler, float ref, float s, float t, float lod) const
{
	return sampleLevelArray1DCompare(m_levels, m_numLevels, sampler, ref, s, lod, IVec2(0, selectLayer(t)));
}

float Texture1DArrayView::sampleCompareOffset (const Sampler& sampler, float ref, float s, float t, float lod, deInt32 offset) const
{
	return sampleLevelArray1DCompare(m_levels, m_numLevels, sampler, ref, s, lod, IVec2(offset, selectLayer(t)));
}

// Texture2DArrayView

Texture2DArrayView::Texture2DArrayView (int numLevels, const ConstPixelBufferAccess* levels)
	: m_numLevels	(numLevels)
	, m_levels		(levels)
{
}

inline int Texture2DArrayView::selectLayer (float r) const
{
	DE_ASSERT(m_numLevels > 0 && m_levels);
	return de::clamp(deFloorFloatToInt32(r + 0.5f), 0, m_levels[0].getDepth()-1);
}

Vec4 Texture2DArrayView::sample (const Sampler& sampler, float s, float t, float r, float lod) const
{
	return sampleLevelArray2D(m_levels, m_numLevels, sampler, s, t, selectLayer(r), lod);
}

float Texture2DArrayView::sampleCompare (const Sampler& sampler, float ref, float s, float t, float r, float lod) const
{
	return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, s, t, lod, IVec3(0, 0, selectLayer(r)));
}

Vec4 Texture2DArrayView::sampleOffset (const Sampler& sampler, float s, float t, float r, float lod, const IVec2& offset) const
{
	return sampleLevelArray2DOffset(m_levels, m_numLevels, sampler, s, t, lod, IVec3(offset.x(), offset.y(), selectLayer(r)));
}

float Texture2DArrayView::sampleCompareOffset (const Sampler& sampler, float ref, float s, float t, float r, float lod, const IVec2& offset) const
{
	return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, s, t, lod, IVec3(offset.x(), offset.y(), selectLayer(r)));
}

Vec4 Texture2DArrayView::gatherOffsets (const Sampler& sampler, float s, float t, float r, int componentNdx, const IVec2 (&offsets)[4]) const
{
	return gatherArray2DOffsets(m_levels[0], sampler, s, t, selectLayer(r), componentNdx, offsets);
}

Vec4 Texture2DArrayView::gatherOffsetsCompare (const Sampler& sampler, float ref, float s, float t, float r, const IVec2 (&offsets)[4]) const
{
	return gatherArray2DOffsetsCompare(m_levels[0], sampler, ref, s, t, selectLayer(r), offsets);
}

// Texture1DArray

Texture1DArray::Texture1DArray (const TextureFormat& format, int width, int numLayers)
	: TextureLevelPyramid	(format, computeMipPyramidLevels(width))
	, m_width				(width)
	, m_numLayers			(numLayers)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture1DArray::Texture1DArray (const Texture1DArray& other)
	: TextureLevelPyramid	(other)
	, m_width				(other.m_width)
	, m_numLayers			(other.m_numLayers)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture1DArray& Texture1DArray::operator= (const Texture1DArray& other)
{
	if (this == &other)
		return *this;

	TextureLevelPyramid::operator=(other);

	m_width		= other.m_width;
	m_numLayers	= other.m_numLayers;
	m_view		= Texture1DArrayView(getNumLevels(), getLevels());

	return *this;
}

Texture1DArray::~Texture1DArray (void)
{
}

void Texture1DArray::allocLevel (int levelNdx)
{
	DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

	const int width = getMipPyramidLevelSize(m_width, levelNdx);

	TextureLevelPyramid::allocLevel(levelNdx, width, m_numLayers, 1);
}

// Texture2DArray

Texture2DArray::Texture2DArray (const TextureFormat& format, int width, int height, int numLayers)
	: TextureLevelPyramid	(format, computeMipPyramidLevels(width, height))
	, m_width				(width)
	, m_height				(height)
	, m_numLayers			(numLayers)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture2DArray::Texture2DArray (const Texture2DArray& other)
	: TextureLevelPyramid	(other)
	, m_width				(other.m_width)
	, m_height				(other.m_height)
	, m_numLayers			(other.m_numLayers)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture2DArray& Texture2DArray::operator= (const Texture2DArray& other)
{
	if (this == &other)
		return *this;

	TextureLevelPyramid::operator=(other);

	m_width		= other.m_width;
	m_height	= other.m_height;
	m_numLayers	= other.m_numLayers;
	m_view		= Texture2DArrayView(getNumLevels(), getLevels());

	return *this;
}

Texture2DArray::~Texture2DArray (void)
{
}

void Texture2DArray::allocLevel (int levelNdx)
{
	DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

	const int	width	= getMipPyramidLevelSize(m_width,	levelNdx);
	const int	height	= getMipPyramidLevelSize(m_height,	levelNdx);

	TextureLevelPyramid::allocLevel(levelNdx, width, height, m_numLayers);
}

// Texture3DView

Texture3DView::Texture3DView (int numLevels, const ConstPixelBufferAccess* levels)
	: m_numLevels	(numLevels)
	, m_levels		(levels)
{
}

// Texture3D

Texture3D::Texture3D (const TextureFormat& format, int width, int height, int depth)
	: TextureLevelPyramid	(format, computeMipPyramidLevels(width, height, depth))
	, m_width				(width)
	, m_height				(height)
	, m_depth				(depth)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture3D::Texture3D (const Texture3D& other)
	: TextureLevelPyramid	(other)
	, m_width				(other.m_width)
	, m_height				(other.m_height)
	, m_depth				(other.m_depth)
	, m_view				(getNumLevels(), getLevels())
{
}

Texture3D& Texture3D::operator= (const Texture3D& other)
{
	if (this == &other)
		return *this;

	TextureLevelPyramid::operator=(other);

	m_width		= other.m_width;
	m_height	= other.m_height;
	m_depth		= other.m_depth;
	m_view		= Texture3DView(getNumLevels(), getLevels());

	return *this;
}

Texture3D::~Texture3D (void)
{
}

void Texture3D::allocLevel (int levelNdx)
{
	DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

	const int width		= getMipPyramidLevelSize(m_width,	levelNdx);
	const int height	= getMipPyramidLevelSize(m_height,	levelNdx);
	const int depth		= getMipPyramidLevelSize(m_depth,	levelNdx);

	TextureLevelPyramid::allocLevel(levelNdx, width, height, depth);
}

// TextureCubeArrayView

TextureCubeArrayView::TextureCubeArrayView (int numLevels, const ConstPixelBufferAccess* levels)
	: m_numLevels	(numLevels)
	, m_levels		(levels)
{
}

inline int TextureCubeArrayView::selectLayer (float q) const
{
	DE_ASSERT(m_numLevels > 0 && m_levels);
	DE_ASSERT((m_levels[0].getDepth() % 6) == 0);

	return de::clamp(deFloorFloatToInt32(q + 0.5f), 0, (m_levels[0].getDepth() / 6)-1);
}

tcu::Vec4 TextureCubeArrayView::sample (const Sampler& sampler, float s, float t, float r, float q, float lod) const
{
	const CubeFaceFloatCoords	coords		= getCubeFaceCoords(Vec3(s, t, r));
	const int					layer		= selectLayer(q);
	const int					faceDepth	= (layer * 6) + getCubeArrayFaceIndex(coords.face);

	DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);

	if (sampler.seamlessCubeMap)
		return sampleCubeArraySeamless(m_levels, m_numLevels, layer, coords.face, sampler, coords.s, coords.t, lod);
	else
		return sampleLevelArray2D(m_levels, m_numLevels, sampler, coords.s, coords.t, faceDepth, lod);
}

float TextureCubeArrayView::sampleCompare (const Sampler& sampler, float ref, float s, float t, float r, float q, float lod) const
{
	const CubeFaceFloatCoords	coords		= getCubeFaceCoords(Vec3(s, t, r));
	const int					layer		= selectLayer(q);
	const int					faceDepth	= (layer * 6) + getCubeArrayFaceIndex(coords.face);

	DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);

	if (sampler.seamlessCubeMap)
		return sampleCubeArraySeamlessCompare(m_levels, m_numLevels, layer, coords.face, sampler, ref, coords.s, coords.t, lod);
	else
		return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, coords.s, coords.t, lod, IVec3(0, 0, faceDepth));
}

// TextureCubeArray

TextureCubeArray::TextureCubeArray (const TextureFormat& format, int size, int depth)
	: TextureLevelPyramid	(format, computeMipPyramidLevels(size))
	, m_size				(size)
	, m_depth				(depth)
	, m_view				(getNumLevels(), getLevels())
{
	DE_ASSERT(m_depth % 6 == 0);
}

TextureCubeArray::TextureCubeArray (const TextureCubeArray& other)
	: TextureLevelPyramid	(other)
	, m_size				(other.m_size)
	, m_depth				(other.m_depth)
	, m_view				(getNumLevels(), getLevels())
{
	DE_ASSERT(m_depth % 6 == 0);
}

TextureCubeArray& TextureCubeArray::operator= (const TextureCubeArray& other)
{
	if (this == &other)
		return *this;

	TextureLevelPyramid::operator=(other);

	m_size	= other.m_size;
	m_depth	= other.m_depth;
	m_view	= TextureCubeArrayView(getNumLevels(), getLevels());

	DE_ASSERT(m_depth % 6 == 0);

	return *this;
}

TextureCubeArray::~TextureCubeArray (void)
{
}

void TextureCubeArray::allocLevel (int levelNdx)
{
	DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

	const int size = getMipPyramidLevelSize(m_size, levelNdx);

	TextureLevelPyramid::allocLevel(levelNdx, size, size, m_depth);
}

std::ostream& operator<< (std::ostream& str, TextureFormat::ChannelOrder order)
{
	switch (order)
	{
		case TextureFormat::R:					return str << "R";
		case TextureFormat::A:					return str << "A";
		case TextureFormat::I:					return str << "I";
		case TextureFormat::L:					return str << "L";
		case TextureFormat::LA:					return str << "LA";
		case TextureFormat::RG:					return str << "RG";
		case TextureFormat::RA:					return str << "RA";
		case TextureFormat::RGB:				return str << "RGB";
		case TextureFormat::RGBA:				return str << "RGBA";
		case TextureFormat::ARGB:				return str << "ARGB";
		case TextureFormat::BGRA:				return str << "BGRA";
		case TextureFormat::CHANNELORDER_LAST:	return str << "CHANNELORDER_LAST";
		default:								return str << "UNKNOWN(" << (int)order << ")";
	}
}

std::ostream& operator<< (std::ostream& str, TextureFormat::ChannelType type)
{
	switch (type)
	{
		case TextureFormat::SNORM_INT8:			return str << "SNORM_INT8";
		case TextureFormat::SNORM_INT16:		return str << "SNORM_INT16";
		case TextureFormat::UNORM_INT8:			return str << "UNORM_INT8";
		case TextureFormat::UNORM_INT16:		return str << "UNORM_INT16";
		case TextureFormat::UNORM_SHORT_565:	return str << "UNORM_SHORT_565";
		case TextureFormat::UNORM_SHORT_555:	return str << "UNORM_SHORT_555";
		case TextureFormat::UNORM_SHORT_4444:	return str << "UNORM_SHORT_4444";
		case TextureFormat::UNORM_SHORT_5551:	return str << "UNORM_SHORT_5551";
		case TextureFormat::UNORM_INT_101010:	return str << "UNORM_INT_101010";
		case TextureFormat::SIGNED_INT8:		return str << "SIGNED_INT8";
		case TextureFormat::SIGNED_INT16:		return str << "SIGNED_INT16";
		case TextureFormat::SIGNED_INT32:		return str << "SIGNED_INT32";
		case TextureFormat::UNSIGNED_INT8:		return str << "UNSIGNED_INT8";
		case TextureFormat::UNSIGNED_INT16:		return str << "UNSIGNED_INT16";
		case TextureFormat::UNSIGNED_INT32:		return str << "UNSIGNED_INT32";
		case TextureFormat::HALF_FLOAT:			return str << "HALF_FLOAT";
		case TextureFormat::FLOAT:				return str << "FLOAT";
		case TextureFormat::CHANNELTYPE_LAST:	return str << "CHANNELTYPE_LAST";
		default:								return str << "UNKNOWN(" << (int)type << ")";
	}
}

std::ostream& operator<< (std::ostream& str, CubeFace face)
{
	switch (face)
	{
		case CUBEFACE_NEGATIVE_X:	return str << "CUBEFACE_NEGATIVE_X";
		case CUBEFACE_POSITIVE_X:	return str << "CUBEFACE_POSITIVE_X";
		case CUBEFACE_NEGATIVE_Y:	return str << "CUBEFACE_NEGATIVE_Y";
		case CUBEFACE_POSITIVE_Y:	return str << "CUBEFACE_POSITIVE_Y";
		case CUBEFACE_NEGATIVE_Z:	return str << "CUBEFACE_NEGATIVE_Z";
		case CUBEFACE_POSITIVE_Z:	return str << "CUBEFACE_POSITIVE_Z";
		case CUBEFACE_LAST:			return str << "CUBEFACE_LAST";
		default:					return str << "UNKNOWN(" << (int)face << ")";
	}
}

std::ostream& operator<< (std::ostream& str, TextureFormat format)
{
	return str << format.order << ", " << format.type << "";
}

std::ostream& operator<< (std::ostream& str, const ConstPixelBufferAccess& access)
{
	return str << "format = (" << access.getFormat() << "), size = "
			   << access.getWidth() << " x " << access.getHeight() << " x " << access.getDepth()
			   << ", pitch = " << access.getRowPitch() << " / " << access.getSlicePitch();
}

} // tcu
