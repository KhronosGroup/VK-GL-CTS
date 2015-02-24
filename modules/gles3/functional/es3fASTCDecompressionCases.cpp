/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief ASTC decompression tests
 *
 * \todo Parts of the block-generation code are same as in decompression
 *		 code in tcuCompressedTexture.cpp ; could put them to some shared
 *		 ASTC utility file.
 *
 * \todo Tests for void extents with nontrivial extent coordinates.
 *
 * \todo Better checking of the error color. Currently legitimate error
 *		 pixels are just ignored in image comparison; however, spec says
 *		 that error color is either magenta or all-NaNs. Can NaNs cause
 *		 troubles, or can we assume that NaNs are well-supported in shader
 *		 if the implementation chooses NaNs as error color?
 *//*--------------------------------------------------------------------*/

#include "es3fASTCDecompressionCases.hpp"
#include "gluTexture.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "glsTextureTestUtil.hpp"
#include "tcuCompressedTexture.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuSurface.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deFloat16.h"
#include "deString.h"
#include "deMemory.h"

#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include <vector>
#include <string>
#include <algorithm>

using tcu::TestLog;
using tcu::CompressedTexture;
using tcu::CompressedTexFormat;
using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec4;
using tcu::Sampler;
using tcu::Surface;
using std::vector;
using std::string;

namespace deqp
{

using gls::TextureTestUtil::TextureRenderer;
using gls::TextureTestUtil::RandomViewport;
using gls::TextureTestUtil::ReferenceParams;

namespace gles3
{
namespace Functional
{

namespace ASTCDecompressionCaseInternal
{

static const int ASTC_BLOCK_SIZE_BYTES = 128/8;

static inline int divRoundUp (int a, int b)
{
	return a/b + ((a%b) ? 1 : 0);
}

namespace ASTCBlockGeneratorInternal
{

static inline deUint32 reverseBits (deUint32 src, int numBits)
{
	DE_ASSERT(de::inRange(numBits, 0, 32));
	deUint32 result = 0;
	for (int i = 0; i < numBits; i++)
		result |= ((src >> i) & 1) << (numBits-1-i);
	return result;
}

static inline deUint32 getBit (deUint32 src, int ndx)
{
	DE_ASSERT(de::inBounds(ndx, 0, 32));
	return (src >> ndx) & 1;
}

static inline deUint32 getBits (deUint32 src, int low, int high)
{
	const int numBits = (high-low) + 1;
	if (numBits == 0)
		return 0;
	DE_ASSERT(de::inRange(numBits, 1, 32));
	return (src >> low) & ((1u<<numBits)-1);
}

#if defined(DE_DEBUG)
static inline bool isFloat16InfOrNan (deFloat16 v)
{
	return getBits(v, 10, 14) == 31;
}
#endif

template <typename T, typename Y>
struct isSameType			{ enum { V = 0 }; };
template <typename T>
struct isSameType<T, T>		{ enum { V = 1 }; };

// Helper class for setting bits in a 128-bit block.
class AssignBlock128
{
private:
	typedef deUint64 Word;

	enum
	{
		WORD_BYTES	= sizeof(Word),
		WORD_BITS	= 8*WORD_BYTES,
		NUM_WORDS	= 128 / WORD_BITS
	};

	DE_STATIC_ASSERT(128 % WORD_BITS == 0);

public:
	AssignBlock128 (void)
	{
		for (int wordNdx = 0; wordNdx < NUM_WORDS; wordNdx++)
			m_words[wordNdx] = 0;
	}

	void setBit (int ndx, deUint32 val)
	{
		DE_ASSERT(de::inBounds(ndx, 0, 128));
		DE_ASSERT((val & 1) == val);
		const int wordNdx	= ndx / WORD_BITS;
		const int bitNdx	= ndx % WORD_BITS;
		m_words[wordNdx] = (m_words[wordNdx] & ~((Word)1 << bitNdx)) | ((Word)val << bitNdx);
	}

	void setBits (int low, int high, deUint32 bits)
	{
		DE_ASSERT(de::inBounds(low, 0, 128));
		DE_ASSERT(de::inBounds(high, 0, 128));
		DE_ASSERT(de::inRange(high-low+1, 0, 32));
		DE_ASSERT((bits & (((Word)1 << (high-low+1)) - 1)) == bits);

		if (high-low+1 == 0)
			return;

		const int word0Ndx		= low / WORD_BITS;
		const int word1Ndx		= high / WORD_BITS;
		const int lowNdxInW0	= low % WORD_BITS;

		if (word0Ndx == word1Ndx)
			m_words[word0Ndx] = (m_words[word0Ndx] & ~((((Word)1 << (high-low+1)) - 1) << lowNdxInW0)) | ((Word)bits << lowNdxInW0);
		else
		{
			DE_ASSERT(word1Ndx == word0Ndx + 1);

			const int	highNdxInW1			= high % WORD_BITS;
			const int	numBitsToSetInW0	= WORD_BITS - lowNdxInW0;
			const Word	bitsLowMask			= ((Word)1 << numBitsToSetInW0) - 1;

			m_words[word0Ndx] = (m_words[word0Ndx] & (((Word)1 << lowNdxInW0) - 1))			| (((Word)bits & bitsLowMask) << lowNdxInW0);
			m_words[word1Ndx] = (m_words[word1Ndx] & ~(((Word)1 << (highNdxInW1+1)) - 1))	| (((Word)bits & ~bitsLowMask) >> numBitsToSetInW0);
		}
	}

	void assignToMemory (deUint8* dst) const
	{
		for (int wordNdx = 0; wordNdx < NUM_WORDS; wordNdx++)
		{
			for (int byteNdx = 0; byteNdx < WORD_BYTES; byteNdx++)
				dst[wordNdx*WORD_BYTES + byteNdx] = (deUint8)((m_words[wordNdx] >> (8*byteNdx)) & 0xff);
		}
	}

	void pushBytesToVector (vector<deUint8>& dst) const
	{
		const int assignStartIndex = (int)dst.size();
		dst.resize(dst.size() + ASTC_BLOCK_SIZE_BYTES);
		assignToMemory(&dst[assignStartIndex]);
	}

private:
	Word m_words[NUM_WORDS];
};

// A helper for sequential access into a AssignBlock128.
class BitAssignAccessStream
{
public:
	BitAssignAccessStream (AssignBlock128& dst, int startNdxInSrc, int length, bool forward)
		: m_dst				(dst)
		, m_startNdxInSrc	(startNdxInSrc)
		, m_length			(length)
		, m_forward			(forward)
		, m_ndx				(0)
	{
	}

	// Set the next num bits. Bits at positions greater than or equal to m_length are not touched.
	void setNext (int num, deUint32 bits)
	{
		DE_ASSERT((bits & (((deUint64)1 << num) - 1)) == bits);

		if (num == 0 || m_ndx >= m_length)
			return;

		const int		end				= m_ndx + num;
		const int		numBitsToDst	= de::max(0, de::min(m_length, end) - m_ndx);
		const int		low				= m_ndx;
		const int		high			= m_ndx + numBitsToDst - 1;
		const deUint32	actualBits		= getBits(bits, 0, numBitsToDst-1);

		m_ndx += num;

		return m_forward ? m_dst.setBits(m_startNdxInSrc + low,  m_startNdxInSrc + high, actualBits)
						 : m_dst.setBits(m_startNdxInSrc - high, m_startNdxInSrc - low, reverseBits(actualBits, numBitsToDst));
	}

private:
	AssignBlock128&		m_dst;
	const int			m_startNdxInSrc;
	const int			m_length;
	const bool			m_forward;

	int					m_ndx;
};

struct VoidExtentParams
{
	DE_STATIC_ASSERT((isSameType<deFloat16, deUint16>::V));
	bool		isHDR;
	deUint16	r;
	deUint16	g;
	deUint16	b;
	deUint16	a;
	// \note Currently extent coordinates are all set to all-ones.

	VoidExtentParams (bool isHDR_, deUint16 r_, deUint16 g_, deUint16 b_, deUint16 a_) : isHDR(isHDR_), r(r_), g(g_), b(b_), a(a_) {}
};

static AssignBlock128 generateVoidExtentBlock (const VoidExtentParams& params)
{
	AssignBlock128 block;

	block.setBits(0, 8, 0x1fc); // \note Marks void-extent block.
	block.setBit(9, params.isHDR);
	block.setBits(10, 11, 3); // \note Spec shows that these bits are both set, although they serve no purpose.

	// Extent coordinates - currently all-ones.
	block.setBits(12, 24, 0x1fff);
	block.setBits(25, 37, 0x1fff);
	block.setBits(38, 50, 0x1fff);
	block.setBits(51, 63, 0x1fff);

	DE_ASSERT(!params.isHDR || (!isFloat16InfOrNan(params.r) &&
								!isFloat16InfOrNan(params.g) &&
								!isFloat16InfOrNan(params.b) &&
								!isFloat16InfOrNan(params.a)));

	block.setBits(64,  79,  params.r);
	block.setBits(80,  95,  params.g);
	block.setBits(96,  111, params.b);
	block.setBits(112, 127, params.a);

	return block;
}

enum ISEMode
{
	ISEMODE_TRIT = 0,
	ISEMODE_QUINT,
	ISEMODE_PLAIN_BIT,

	ISEMODE_LAST
};

struct ISEParams
{
	ISEMode		mode;
	int			numBits;

	ISEParams (ISEMode mode_, int numBits_) : mode(mode_), numBits(numBits_) {}
};

// An input array of ISE inputs for an entire ASTC block. Can be given as either single values in the
// range [0, maximumValueOfISERange] or as explicit block value specifications. The latter is needed
// so we can test all possible values of T and Q in a block, since multiple T or Q values may map
// to the same set of decoded values.
struct ISEInput
{
	struct Block
	{
		deUint32 tOrQValue; //!< The 8-bit T or 7-bit Q in a trit or quint ISE block.
		deUint32 bitValues[5];
	};

	bool isGivenInBlockForm;
	union
	{
		//!< \note 64 comes from the maximum number of weight values in an ASTC block.
		deUint32	plain[64];
		Block		block[64];
	} value;

	ISEInput (void)
		: isGivenInBlockForm (false)
	{
	}
};

static inline int computeNumRequiredBits (const ISEParams& iseParams, int numValues)
{
	switch (iseParams.mode)
	{
		case ISEMODE_TRIT:			return divRoundUp(numValues*8, 5) + numValues*iseParams.numBits;
		case ISEMODE_QUINT:			return divRoundUp(numValues*7, 3) + numValues*iseParams.numBits;
		case ISEMODE_PLAIN_BIT:		return numValues*iseParams.numBits;
		default:
			DE_ASSERT(false);
			return -1;
	}
}

static inline deUint32 computeISERangeMax (const ISEParams& iseParams)
{
	switch (iseParams.mode)
	{
		case ISEMODE_TRIT:			return (1u << iseParams.numBits) * 3 - 1;
		case ISEMODE_QUINT:			return (1u << iseParams.numBits) * 5 - 1;
		case ISEMODE_PLAIN_BIT:		return (1u << iseParams.numBits)     - 1;
		default:
			DE_ASSERT(false);
			return -1;
	}
}

struct NormalBlockParams
{
	int					weightGridWidth;
	int					weightGridHeight;
	ISEParams			weightISEParams;
	bool				isDualPlane;
	deUint32			ccs; //! \note Irrelevant if !isDualPlane.
	int					numPartitions;
	deUint32			colorEndpointModes[4];
	// \note Below members are irrelevant if numPartitions == 1.
	bool				isMultiPartSingleCemMode; //! \note If true, the single CEM is at colorEndpointModes[0].
	deUint32			partitionSeed;

	NormalBlockParams (void)
		: weightGridWidth			(-1)
		, weightGridHeight			(-1)
		, weightISEParams			(ISEMODE_LAST, -1)
		, isDualPlane				(true)
		, ccs						((deUint32)-1)
		, numPartitions				(-1)
		, isMultiPartSingleCemMode	(false)
		, partitionSeed				((deUint32)-1)
	{
		colorEndpointModes[0] = 0;
		colorEndpointModes[1] = 0;
		colorEndpointModes[2] = 0;
		colorEndpointModes[3] = 0;
	}
};

struct NormalBlockISEInputs
{
	ISEInput weight;
	ISEInput endpoint;

	NormalBlockISEInputs (void)
		: weight	()
		, endpoint	()
	{
	}
};

static inline int computeNumWeights (const NormalBlockParams& params)
{
	return params.weightGridWidth * params.weightGridHeight * (params.isDualPlane ? 2 : 1);
}

static inline int computeNumBitsForColorEndpoints (const NormalBlockParams& params)
{
	const int numWeightBits			= computeNumRequiredBits(params.weightISEParams, computeNumWeights(params));
	const int numConfigDataBits		= (params.numPartitions == 1 ? 17 : params.isMultiPartSingleCemMode ? 29 : 25 + 3*params.numPartitions) +
									  (params.isDualPlane ? 2 : 0);

	return 128 - numWeightBits - numConfigDataBits;
}

static inline int computeNumColorEndpointValues (deUint32 endpointMode)
{
	DE_ASSERT(endpointMode < 16);
	return (endpointMode/4 + 1) * 2;
}

static inline int computeNumColorEndpointValues (const deUint32* endpointModes, int numPartitions, bool isMultiPartSingleCemMode)
{
	if (isMultiPartSingleCemMode)
		return numPartitions * computeNumColorEndpointValues(endpointModes[0]);
	else
	{
		int result = 0;
		for (int i = 0; i < numPartitions; i++)
			result += computeNumColorEndpointValues(endpointModes[i]);
		return result;
	}
}

static inline bool isValidBlockParams (const NormalBlockParams& params, int blockWidth, int blockHeight)
{
	const int numWeights				= computeNumWeights(params);
	const int numWeightBits				= computeNumRequiredBits(params.weightISEParams, numWeights);
	const int numColorEndpointValues	= computeNumColorEndpointValues(&params.colorEndpointModes[0], params.numPartitions, params.isMultiPartSingleCemMode);
	const int numBitsForColorEndpoints	= computeNumBitsForColorEndpoints(params);

	return numWeights <= 64										&&
		   de::inRange(numWeightBits, 24, 96)					&&
		   params.weightGridWidth <= blockWidth					&&
		   params.weightGridHeight <= blockHeight				&&
		   !(params.numPartitions == 4 && params.isDualPlane)	&&
		   numColorEndpointValues <= 18							&&
		   numBitsForColorEndpoints >= divRoundUp(13*numColorEndpointValues, 5);
}

// Write bits 0 to 10 of an ASTC block.
static void writeBlockMode (AssignBlock128& dst, const NormalBlockParams& blockParams)
{
	const deUint32	d = blockParams.isDualPlane != 0;
	// r and h initialized in switch below.
	deUint32		r;
	deUint32		h;
	// a, b and blockModeLayoutNdx initialized in block mode layout index detecting loop below.
	deUint32		a = (deUint32)-1;
	deUint32		b = (deUint32)-1;
	int				blockModeLayoutNdx;

	// Find the values of r and h (ISE range).
	switch (computeISERangeMax(blockParams.weightISEParams))
	{
		case 1:		r = 2; h = 0;	break;
		case 2:		r = 3; h = 0;	break;
		case 3:		r = 4; h = 0;	break;
		case 4:		r = 5; h = 0;	break;
		case 5:		r = 6; h = 0;	break;
		case 7:		r = 7; h = 0;	break;

		case 9:		r = 2; h = 1;	break;
		case 11:	r = 3; h = 1;	break;
		case 15:	r = 4; h = 1;	break;
		case 19:	r = 5; h = 1;	break;
		case 23:	r = 6; h = 1;	break;
		case 31:	r = 7; h = 1;	break;

		default:
			DE_ASSERT(false);
			r = (deUint32)-1;
			h = (deUint32)-1;
	}

	// Find block mode layout index, i.e. appropriate row in the "2d block mode layout" table in ASTC spec.

	{
		enum BlockModeLayoutABVariable { Z=0, A=1, B=2 };

		static const struct BlockModeLayout
		{
			int							aNumBits;
			int							bNumBits;
			BlockModeLayoutABVariable	gridWidthVariableTerm;
			int							gridWidthConstantTerm;
			BlockModeLayoutABVariable	gridHeightVariableTerm;
			int							gridHeightConstantTerm;
		} blockModeLayouts[] =
		{
			{ 2, 2,   B,  4,   A,  2},
			{ 2, 2,   B,  8,   A,  2},
			{ 2, 2,   A,  2,   B,  8},
			{ 2, 1,   A,  2,   B,  6},
			{ 2, 1,   B,  2,   A,  2},
			{ 2, 0,   Z, 12,   A,  2},
			{ 2, 0,   A,  2,   Z, 12},
			{ 0, 0,   Z,  6,   Z, 10},
			{ 0, 0,   Z, 10,   Z,  6},
			{ 2, 2,   A,  6,   B,  6}
		};

		for (blockModeLayoutNdx = 0; blockModeLayoutNdx < DE_LENGTH_OF_ARRAY(blockModeLayouts); blockModeLayoutNdx++)
		{
			const BlockModeLayout&	layout					= blockModeLayouts[blockModeLayoutNdx];
			const int				aMax					= (1 << layout.aNumBits) - 1;
			const int				bMax					= (1 << layout.bNumBits) - 1;
			const int				variableOffsetsMax[3]	= { 0, aMax, bMax };
			const int				widthMin				= layout.gridWidthConstantTerm;
			const int				heightMin				= layout.gridHeightConstantTerm;
			const int				widthMax				= widthMin  + variableOffsetsMax[layout.gridWidthVariableTerm];
			const int				heightMax				= heightMin + variableOffsetsMax[layout.gridHeightVariableTerm];

			DE_ASSERT(layout.gridWidthVariableTerm != layout.gridHeightVariableTerm || layout.gridWidthVariableTerm == Z);

			if (de::inRange(blockParams.weightGridWidth, widthMin, widthMax) &&
				de::inRange(blockParams.weightGridHeight, heightMin, heightMax))
			{
				deUint32	dummy			= 0;
				deUint32&	widthVariable	= layout.gridWidthVariableTerm == A  ? a : layout.gridWidthVariableTerm == B  ? b : dummy;
				deUint32&	heightVariable	= layout.gridHeightVariableTerm == A ? a : layout.gridHeightVariableTerm == B ? b : dummy;

				widthVariable	= blockParams.weightGridWidth  - layout.gridWidthConstantTerm;
				heightVariable	= blockParams.weightGridHeight - layout.gridHeightConstantTerm;

				break;
			}
		}
	}

	// Set block mode bits.

	const deUint32 a0 = getBit(a, 0);
	const deUint32 a1 = getBit(a, 1);
	const deUint32 b0 = getBit(b, 0);
	const deUint32 b1 = getBit(b, 1);
	const deUint32 r0 = getBit(r, 0);
	const deUint32 r1 = getBit(r, 1);
	const deUint32 r2 = getBit(r, 2);

#define SB(NDX, VAL) dst.setBit((NDX), (VAL))
#define ASSIGN_BITS(B10, B9, B8, B7, B6, B5, B4, B3, B2, B1, B0) do { SB(10,(B10)); SB(9,(B9)); SB(8,(B8)); SB(7,(B7)); SB(6,(B6)); SB(5,(B5)); SB(4,(B4)); SB(3,(B3)); SB(2,(B2)); SB(1,(B1)); SB(0,(B0)); } while (false)

	switch (blockModeLayoutNdx)
	{
		case 0: ASSIGN_BITS(d,  h,  b1, b0, a1, a0, r0, 0,  0,  r2, r1);									break;
		case 1: ASSIGN_BITS(d,  h,  b1, b0, a1, a0, r0, 0,  1,  r2, r1);									break;
		case 2: ASSIGN_BITS(d,  h,  b1, b0, a1, a0, r0, 1,  0,  r2, r1);									break;
		case 3: ASSIGN_BITS(d,  h,   0,  b, a1, a0, r0, 1,  1,  r2, r1);									break;
		case 4: ASSIGN_BITS(d,  h,   1,  b, a1, a0, r0, 1,  1,  r2, r1);									break;
		case 5: ASSIGN_BITS(d,  h,   0,  0, a1, a0, r0, r2, r1,  0,  0);									break;
		case 6: ASSIGN_BITS(d,  h,   0,  1, a1, a0, r0, r2, r1,  0,  0);									break;
		case 7: ASSIGN_BITS(d,  h,   1,  1,  0,  0, r0, r2, r1,  0,  0);									break;
		case 8: ASSIGN_BITS(d,  h,   1,  1,  0,  1, r0, r2, r1,  0,  0);									break;
		case 9: ASSIGN_BITS(b1, b0,  1,  0, a1, a0, r0, r2, r1,  0,  0); DE_ASSERT(d == 0 && h == 0);		break;
		default:
			DE_ASSERT(false);
	}

#undef ASSIGN_BITS
#undef SB
}

// Write color endpoint mode data of an ASTC block.
static void writeColorEndpointModes (AssignBlock128& dst, const deUint32* colorEndpointModes, bool isMultiPartSingleCemMode, int numPartitions, int extraCemBitsStart)
{
	if (numPartitions == 1)
		dst.setBits(13, 16, colorEndpointModes[0]);
	else
	{
		if (isMultiPartSingleCemMode)
		{
			dst.setBits(23, 24, 0);
			dst.setBits(25, 28, colorEndpointModes[0]);
		}
		else
		{
			DE_ASSERT(numPartitions > 0);
			const deUint32 minCem				= *std::min_element(&colorEndpointModes[0], &colorEndpointModes[numPartitions]);
			const deUint32 maxCem				= *std::max_element(&colorEndpointModes[0], &colorEndpointModes[numPartitions]);
			const deUint32 minCemClass			= minCem/4;
			const deUint32 maxCemClass			= maxCem/4;
			DE_ASSERT(maxCemClass - minCemClass <= 1);
			DE_UNREF(minCemClass); // \note For non-debug builds.
			const deUint32 highLevelSelector	= de::max(1u, maxCemClass);

			dst.setBits(23, 24, highLevelSelector);

			for (int partNdx = 0; partNdx < numPartitions; partNdx++)
			{
				const deUint32 c			= colorEndpointModes[partNdx] / 4 == highLevelSelector ? 1 : 0;
				const deUint32 m			= colorEndpointModes[partNdx] % 4;
				const deUint32 lowMBit0Ndx	= numPartitions + 2*partNdx;
				const deUint32 lowMBit1Ndx	= numPartitions + 2*partNdx + 1;
				dst.setBit(25 + partNdx, c);
				dst.setBit(lowMBit0Ndx < 4 ? 25+lowMBit0Ndx : extraCemBitsStart+lowMBit0Ndx-4, getBit(m, 0));
				dst.setBit(lowMBit1Ndx < 4 ? 25+lowMBit1Ndx : extraCemBitsStart+lowMBit1Ndx-4, getBit(m, 1));
			}
		}
	}
}

static ISEParams computeMaximumRangeISEParams (int numAvailableBits, int numValuesInSequence)
{
	int curBitsForTritMode		= 6;
	int curBitsForQuintMode		= 5;
	int curBitsForPlainBitMode	= 8;

	while (true)
	{
		DE_ASSERT(curBitsForTritMode > 0 || curBitsForQuintMode > 0 || curBitsForPlainBitMode > 0);

		const int tritRange			= curBitsForTritMode > 0		? (3 << curBitsForTritMode) - 1			: -1;
		const int quintRange		= curBitsForQuintMode > 0		? (5 << curBitsForQuintMode) - 1		: -1;
		const int plainBitRange		= curBitsForPlainBitMode > 0	? (1 << curBitsForPlainBitMode) - 1		: -1;
		const int maxRange			= de::max(de::max(tritRange, quintRange), plainBitRange);

		if (maxRange == tritRange)
		{
			const ISEParams params(ISEMODE_TRIT, curBitsForTritMode);
			if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
				return ISEParams(ISEMODE_TRIT, curBitsForTritMode);
			curBitsForTritMode--;
		}
		else if (maxRange == quintRange)
		{
			const ISEParams params(ISEMODE_QUINT, curBitsForQuintMode);
			if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
				return ISEParams(ISEMODE_QUINT, curBitsForQuintMode);
			curBitsForQuintMode--;
		}
		else
		{
			const ISEParams params(ISEMODE_PLAIN_BIT, curBitsForPlainBitMode);
			DE_ASSERT(maxRange == plainBitRange);
			if (computeNumRequiredBits(params, numValuesInSequence) <= numAvailableBits)
				return ISEParams(ISEMODE_PLAIN_BIT, curBitsForPlainBitMode);
			curBitsForPlainBitMode--;
		}
	}
}

static void encodeISETritBlock (BitAssignAccessStream& dst, int numBits, bool fromExplicitInputBlock, const ISEInput::Block& blockInput, const deUint32* nonBlockInput, int numValues)
{
	// tritBlockTValue[t0][t1][t2][t3][t4] is a value of T (not necessarily the only one) that will yield the given trits when decoded.
	static const deUint32 tritBlockTValue[3][3][3][3][3] =
	{
		{
			{{{0, 128, 96}, {32, 160, 224}, {64, 192, 28}}, {{16, 144, 112}, {48, 176, 240}, {80, 208, 156}}, {{3, 131, 99}, {35, 163, 227}, {67, 195, 31}}},
			{{{4, 132, 100}, {36, 164, 228}, {68, 196, 60}}, {{20, 148, 116}, {52, 180, 244}, {84, 212, 188}}, {{19, 147, 115}, {51, 179, 243}, {83, 211, 159}}},
			{{{8, 136, 104}, {40, 168, 232}, {72, 200, 92}}, {{24, 152, 120}, {56, 184, 248}, {88, 216, 220}}, {{12, 140, 108}, {44, 172, 236}, {76, 204, 124}}}
		},
		{
			{{{1, 129, 97}, {33, 161, 225}, {65, 193, 29}}, {{17, 145, 113}, {49, 177, 241}, {81, 209, 157}}, {{7, 135, 103}, {39, 167, 231}, {71, 199, 63}}},
			{{{5, 133, 101}, {37, 165, 229}, {69, 197, 61}}, {{21, 149, 117}, {53, 181, 245}, {85, 213, 189}}, {{23, 151, 119}, {55, 183, 247}, {87, 215, 191}}},
			{{{9, 137, 105}, {41, 169, 233}, {73, 201, 93}}, {{25, 153, 121}, {57, 185, 249}, {89, 217, 221}}, {{13, 141, 109}, {45, 173, 237}, {77, 205, 125}}}
		},
		{
			{{{2, 130, 98}, {34, 162, 226}, {66, 194, 30}}, {{18, 146, 114}, {50, 178, 242}, {82, 210, 158}}, {{11, 139, 107}, {43, 171, 235}, {75, 203, 95}}},
			{{{6, 134, 102}, {38, 166, 230}, {70, 198, 62}}, {{22, 150, 118}, {54, 182, 246}, {86, 214, 190}}, {{27, 155, 123}, {59, 187, 251}, {91, 219, 223}}},
			{{{10, 138, 106}, {42, 170, 234}, {74, 202, 94}}, {{26, 154, 122}, {58, 186, 250}, {90, 218, 222}}, {{14, 142, 110}, {46, 174, 238}, {78, 206, 126}}}
		}
	};

	DE_ASSERT(de::inRange(numValues, 1, 5));

	deUint32 tritParts[5];
	deUint32 bitParts[5];

	for (int i = 0; i < 5; i++)
	{
		if (i < numValues)
		{
			if (fromExplicitInputBlock)
			{
				bitParts[i]		= blockInput.bitValues[i];
				tritParts[i]	= -1; // \note Won't be used, but silences warning.
			}
			else
			{
				bitParts[i]		= getBits(nonBlockInput[i], 0, numBits-1);
				tritParts[i]	= nonBlockInput[i] >> numBits;
			}
		}
		else
		{
			bitParts[i]		= 0;
			tritParts[i]	= 0;
		}
	}

	const deUint32 T = fromExplicitInputBlock ? blockInput.tOrQValue : tritBlockTValue[tritParts[0]]
																					  [tritParts[1]]
																					  [tritParts[2]]
																					  [tritParts[3]]
																					  [tritParts[4]];

	dst.setNext(numBits,	bitParts[0]);
	dst.setNext(2,			getBits(T, 0, 1));
	dst.setNext(numBits,	bitParts[1]);
	dst.setNext(2,			getBits(T, 2, 3));
	dst.setNext(numBits,	bitParts[2]);
	dst.setNext(1,			getBit(T, 4));
	dst.setNext(numBits,	bitParts[3]);
	dst.setNext(2,			getBits(T, 5, 6));
	dst.setNext(numBits,	bitParts[4]);
	dst.setNext(1,			getBit(T, 7));
}

static void encodeISEQuintBlock (BitAssignAccessStream& dst, int numBits, bool fromExplicitInputBlock, const ISEInput::Block& blockInput, const deUint32* nonBlockInput, int numValues)
{
	// quintBlockQValue[q0][q1][q2] is a value of Q (not necessarily the only one) that will yield the given quints when decoded.
	static const deUint32 quintBlockQValue[5][5][5] =
	{
		{{0, 32, 64, 96, 102}, {8, 40, 72, 104, 110}, {16, 48, 80, 112, 118}, {24, 56, 88, 120, 126}, {5, 37, 69, 101, 39}},
		{{1, 33, 65, 97, 103}, {9, 41, 73, 105, 111}, {17, 49, 81, 113, 119}, {25, 57, 89, 121, 127}, {13, 45, 77, 109, 47}},
		{{2, 34, 66, 98, 70}, {10, 42, 74, 106, 78}, {18, 50, 82, 114, 86}, {26, 58, 90, 122, 94}, {21, 53, 85, 117, 55}},
		{{3, 35, 67, 99, 71}, {11, 43, 75, 107, 79}, {19, 51, 83, 115, 87}, {27, 59, 91, 123, 95}, {29, 61, 93, 125, 63}},
		{{4, 36, 68, 100, 38}, {12, 44, 76, 108, 46}, {20, 52, 84, 116, 54}, {28, 60, 92, 124, 62}, {6, 14, 22, 30, 7}}
	};

	DE_ASSERT(de::inRange(numValues, 1, 3));

	deUint32 quintParts[3];
	deUint32 bitParts[3];

	for (int i = 0; i < 3; i++)
	{
		if (i < numValues)
		{
			if (fromExplicitInputBlock)
			{
				bitParts[i]		= blockInput.bitValues[i];
				quintParts[i]	= -1; // \note Won't be used, but silences warning.
			}
			else
			{
				bitParts[i]		= getBits(nonBlockInput[i], 0, numBits-1);
				quintParts[i]	= nonBlockInput[i] >> numBits;
			}
		}
		else
		{
			bitParts[i]		= 0;
			quintParts[i]	= 0;
		}
	}

	const deUint32 Q = fromExplicitInputBlock ? blockInput.tOrQValue : quintBlockQValue[quintParts[0]]
																					   [quintParts[1]]
																					   [quintParts[2]];

	dst.setNext(numBits,	bitParts[0]);
	dst.setNext(3,			getBits(Q, 0, 2));
	dst.setNext(numBits,	bitParts[1]);
	dst.setNext(2,			getBits(Q, 3, 4));
	dst.setNext(numBits,	bitParts[2]);
	dst.setNext(2,			getBits(Q, 5, 6));
}

static void encodeISEBitBlock (BitAssignAccessStream& dst, int numBits, deUint32 value)
{
	DE_ASSERT(de::inRange(value, 0u, (1u<<numBits)-1));
	dst.setNext(numBits, value);
}

static void encodeISE (BitAssignAccessStream& dst, const ISEParams& params, const ISEInput& input, int numValues)
{
	if (params.mode == ISEMODE_TRIT)
	{
		const int numBlocks = divRoundUp(numValues, 5);
		for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
		{
			const int numValuesInBlock = blockNdx == numBlocks-1 ? numValues - 5*(numBlocks-1) : 5;
			encodeISETritBlock(dst, params.numBits, input.isGivenInBlockForm,
							   input.isGivenInBlockForm ? input.value.block[blockNdx]	: ISEInput::Block(),
							   input.isGivenInBlockForm ? DE_NULL						: &input.value.plain[5*blockNdx],
							   numValuesInBlock);
		}
	}
	else if (params.mode == ISEMODE_QUINT)
	{
		const int numBlocks = divRoundUp(numValues, 3);
		for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
		{
			const int numValuesInBlock = blockNdx == numBlocks-1 ? numValues - 3*(numBlocks-1) : 3;
			encodeISEQuintBlock(dst, params.numBits, input.isGivenInBlockForm,
								input.isGivenInBlockForm ? input.value.block[blockNdx]	: ISEInput::Block(),
								input.isGivenInBlockForm ? DE_NULL						: &input.value.plain[3*blockNdx],
								numValuesInBlock);
		}
	}
	else
	{
		DE_ASSERT(params.mode == ISEMODE_PLAIN_BIT);
		for (int i = 0; i < numValues; i++)
			encodeISEBitBlock(dst, params.numBits, input.isGivenInBlockForm ? input.value.block[i].bitValues[0] : input.value.plain[i]);
	}
}

static void writeWeightData (AssignBlock128& dst, const ISEParams& iseParams, const ISEInput& input, int numWeights)
{
	const int				numWeightBits	= computeNumRequiredBits(iseParams, numWeights);
	BitAssignAccessStream	access			(dst, 127, numWeightBits, false);
	encodeISE(access, iseParams, input, numWeights);
}

static void writeColorEndpointData (AssignBlock128& dst, const ISEParams& iseParams, const ISEInput& input, int numEndpoints, int numBitsForColorEndpoints, int colorEndpointDataStartNdx)
{
	BitAssignAccessStream access(dst, colorEndpointDataStartNdx, numBitsForColorEndpoints, true);
	encodeISE(access, iseParams, input, numEndpoints);
}

static AssignBlock128 generateNormalBlock (const NormalBlockParams& blockParams, int blockWidth, int blockHeight, const NormalBlockISEInputs& iseInputs)
{
	DE_ASSERT(isValidBlockParams(blockParams, blockWidth, blockHeight));
	DE_UNREF(blockWidth);	// \note For non-debug builds.
	DE_UNREF(blockHeight);	// \note For non-debug builds.

	AssignBlock128	block;
	const int		numWeights		= computeNumWeights(blockParams);
	const int		numWeightBits	= computeNumRequiredBits(blockParams.weightISEParams, numWeights);

	writeBlockMode(block, blockParams);

	block.setBits(11, 12, blockParams.numPartitions - 1);
	if (blockParams.numPartitions > 1)
		block.setBits(13, 22, blockParams.partitionSeed);

	{
		const int extraCemBitsStart = 127 - numWeightBits - (blockParams.numPartitions == 1 || blockParams.isMultiPartSingleCemMode		? -1
															: blockParams.numPartitions == 4											? 7
															: blockParams.numPartitions == 3											? 4
															: blockParams.numPartitions == 2											? 1
															: 0);

		writeColorEndpointModes(block, &blockParams.colorEndpointModes[0], blockParams.isMultiPartSingleCemMode, blockParams.numPartitions, extraCemBitsStart);

		if (blockParams.isDualPlane)
			block.setBits(extraCemBitsStart-2, extraCemBitsStart-1, blockParams.ccs);
	}

	writeWeightData(block, blockParams.weightISEParams, iseInputs.weight, numWeights);

	{
		const int			numColorEndpointValues		= computeNumColorEndpointValues(&blockParams.colorEndpointModes[0], blockParams.numPartitions, blockParams.isMultiPartSingleCemMode);
		const int			numBitsForColorEndpoints	= computeNumBitsForColorEndpoints(blockParams);
		const int			colorEndpointDataStartNdx	= blockParams.numPartitions == 1 ? 17 : 29;
		const ISEParams&	colorEndpointISEParams		= computeMaximumRangeISEParams(numBitsForColorEndpoints, numColorEndpointValues);

		writeColorEndpointData(block, colorEndpointISEParams, iseInputs.endpoint, numColorEndpointValues, numBitsForColorEndpoints, colorEndpointDataStartNdx);
	}

	return block;
}

// Generate default ISE inputs for weight and endpoint data - gradient-ish values.
static NormalBlockISEInputs generateDefaultISEInputs (const NormalBlockParams& blockParams)
{
	NormalBlockISEInputs result;

	{
		result.weight.isGivenInBlockForm = false;

		const int numWeights		= computeNumWeights(blockParams);
		const int weightRangeMax	= computeISERangeMax(blockParams.weightISEParams);

		if (blockParams.isDualPlane)
		{
			for (int i = 0; i < numWeights; i += 2)
				result.weight.value.plain[i] = (i*weightRangeMax + (numWeights-1)/2) / (numWeights-1);

			for (int i = 1; i < numWeights; i += 2)
				result.weight.value.plain[i] = weightRangeMax - (i*weightRangeMax + (numWeights-1)/2) / (numWeights-1);
		}
		else
		{
			for (int i = 0; i < numWeights; i++)
				result.weight.value.plain[i] = (i*weightRangeMax + (numWeights-1)/2) / (numWeights-1);
		}
	}

	{
		result.endpoint.isGivenInBlockForm = false;

		const int			numColorEndpointValues		= computeNumColorEndpointValues(&blockParams.colorEndpointModes[0], blockParams.numPartitions, blockParams.isMultiPartSingleCemMode);
		const int			numBitsForColorEndpoints	= computeNumBitsForColorEndpoints(blockParams);
		const ISEParams&	colorEndpointISEParams		= computeMaximumRangeISEParams(numBitsForColorEndpoints, numColorEndpointValues);
		const int			colorEndpointRangeMax		= computeISERangeMax(colorEndpointISEParams);

		for (int i = 0; i < numColorEndpointValues; i++)
			result.endpoint.value.plain[i] = (i*colorEndpointRangeMax + (numColorEndpointValues-1)/2) / (numColorEndpointValues-1);
	}

	return result;
}

} // ASTCBlockGeneratorInternal

static Vec4 getBlockTestTypeColorScale (ASTCBlockTestType testType)
{
	switch (testType)
	{
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR:				return Vec4(0.5f/65504.0f);
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15:	return Vec4(1.0f/65504.0f, 1.0f/65504.0f, 1.0f/65504.0f, 1.0f);
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15:		return Vec4(1.0f/65504.0f);
		default:											return Vec4(1.0f);
	}
}

static Vec4 getBlockTestTypeColorBias (ASTCBlockTestType testType)
{
	switch (testType)
	{
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR:		return Vec4(0.5f);
		default:									return Vec4(0.0f);
	}
}

// Generate block data for a given ASTCBlockTestType and format.
static void generateBlockCaseTestData (vector<deUint8>& dst, CompressedTexFormat format, ASTCBlockTestType testType)
{
	using namespace ASTCBlockGeneratorInternal;

	static const ISEParams weightISEParamsCandidates[] =
	{
		ISEParams(ISEMODE_PLAIN_BIT,	1),
		ISEParams(ISEMODE_TRIT,			0),
		ISEParams(ISEMODE_PLAIN_BIT,	2),
		ISEParams(ISEMODE_QUINT,		0),
		ISEParams(ISEMODE_TRIT,			1),
		ISEParams(ISEMODE_PLAIN_BIT,	3),
		ISEParams(ISEMODE_QUINT,		1),
		ISEParams(ISEMODE_TRIT,			2),
		ISEParams(ISEMODE_PLAIN_BIT,	4),
		ISEParams(ISEMODE_QUINT,		2),
		ISEParams(ISEMODE_TRIT,			3),
		ISEParams(ISEMODE_PLAIN_BIT,	5)
	};

	DE_ASSERT(tcu::isAstcFormat(format));
	DE_ASSERT(!(tcu::isAstcSRGBFormat(format) && isBlockTestTypeHDROnly(testType)));

	const IVec3 blockSize = getBlockPixelSize(format);
	DE_ASSERT(blockSize.z() == 1);

	switch (testType)
	{
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_LDR:
		// Generate a gradient-like set of LDR void-extent blocks.
		{
			const int			numBlocks	= 1<<13;
			const deUint32		numValues	= 1<<16;
			dst.reserve(numBlocks*ASTC_BLOCK_SIZE_BYTES);

			for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
			{
				const deUint32 baseValue	= blockNdx*(numValues-1) / (numBlocks-1);
				const deUint16 r			= (deUint16)((baseValue + numValues*0/4) % numValues);
				const deUint16 g			= (deUint16)((baseValue + numValues*1/4) % numValues);
				const deUint16 b			= (deUint16)((baseValue + numValues*2/4) % numValues);
				const deUint16 a			= (deUint16)((baseValue + numValues*3/4) % numValues);
				AssignBlock128 block;

				generateVoidExtentBlock(VoidExtentParams(false, r, g, b, a)).pushBytesToVector(dst);
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR:
		// Generate a gradient-like set of HDR void-extent blocks, with values ranging from the largest finite negative to largest finite positive of fp16.
		{
			const float		minValue	= -65504.0f;
			const float		maxValue	= +65504.0f;
			const int		numBlocks	= 1<<13;
			dst.reserve(numBlocks*ASTC_BLOCK_SIZE_BYTES);

			for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
			{
				const int			rNdx	= (blockNdx + numBlocks*0/4) % numBlocks;
				const int			gNdx	= (blockNdx + numBlocks*1/4) % numBlocks;
				const int			bNdx	= (blockNdx + numBlocks*2/4) % numBlocks;
				const int			aNdx	= (blockNdx + numBlocks*3/4) % numBlocks;
				const deFloat16		r		= deFloat32To16(minValue + (float)rNdx * (maxValue - minValue) / (float)(numBlocks-1));
				const deFloat16		g		= deFloat32To16(minValue + (float)gNdx * (maxValue - minValue) / (float)(numBlocks-1));
				const deFloat16		b		= deFloat32To16(minValue + (float)bNdx * (maxValue - minValue) / (float)(numBlocks-1));
				const deFloat16		a		= deFloat32To16(minValue + (float)aNdx * (maxValue - minValue) / (float)(numBlocks-1));

				generateVoidExtentBlock(VoidExtentParams(true, r, g, b, a)).pushBytesToVector(dst);
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_WEIGHT_GRID:
		// Generate different combinations of plane count, weight ISE params, and grid size.
		{
			for (int isDualPlane = 0;		isDualPlane <= 1;												isDualPlane++)
			for (int iseParamsNdx = 0;		iseParamsNdx < DE_LENGTH_OF_ARRAY(weightISEParamsCandidates);	iseParamsNdx++)
			for (int weightGridWidth = 2;	weightGridWidth <= 12;											weightGridWidth++)
			for (int weightGridHeight = 2;	weightGridHeight <= 12;											weightGridHeight++)
			{
				NormalBlockParams		blockParams;
				NormalBlockISEInputs	iseInputs;

				blockParams.weightGridWidth			= weightGridWidth;
				blockParams.weightGridHeight		= weightGridHeight;
				blockParams.isDualPlane				= isDualPlane != 0;
				blockParams.weightISEParams			= weightISEParamsCandidates[iseParamsNdx];
				blockParams.ccs						= 0;
				blockParams.numPartitions			= 1;
				blockParams.colorEndpointModes[0]	= 0;

				if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y()))
					generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), generateDefaultISEInputs(blockParams)).pushBytesToVector(dst);
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_WEIGHT_ISE:
		// For each weight ISE param set, generate blocks that cover:
		// - each single value of the ISE's range, at each position inside an ISE block
		// - for trit and quint ISEs, each single T or Q value of an ISE block
		{
			for (int iseParamsNdx = 0;	iseParamsNdx < DE_LENGTH_OF_ARRAY(weightISEParamsCandidates);	iseParamsNdx++)
			{
				const ISEParams&	iseParams = weightISEParamsCandidates[iseParamsNdx];
				NormalBlockParams	blockParams;

				blockParams.weightGridWidth			= 4;
				blockParams.weightGridHeight		= 4;
				blockParams.weightISEParams			= iseParams;
				blockParams.numPartitions			= 1;
				blockParams.isDualPlane				= blockParams.weightGridWidth * blockParams.weightGridHeight < 24 ? true : false;
				blockParams.ccs						= 0;
				blockParams.colorEndpointModes[0]	= 0;

				while (!isValidBlockParams(blockParams, blockSize.x(), blockSize.y()))
				{
					blockParams.weightGridWidth--;
					blockParams.weightGridHeight--;
				}

				const int numValuesInISEBlock	= iseParams.mode == ISEMODE_TRIT ? 5 : iseParams.mode == ISEMODE_QUINT ? 3 : 1;
				const int numWeights			= computeNumWeights(blockParams);

				{
					const int				numWeightValues		= (int)computeISERangeMax(iseParams) + 1;
					const int				numBlocks			= divRoundUp(numWeightValues, numWeights);
					NormalBlockISEInputs	iseInputs			= generateDefaultISEInputs(blockParams);
					iseInputs.weight.isGivenInBlockForm = false;

					for (int offset = 0;	offset < numValuesInISEBlock;	offset++)
					for (int blockNdx = 0;	blockNdx < numBlocks;			blockNdx++)
					{
						for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
							iseInputs.weight.value.plain[weightNdx] = (blockNdx*numWeights + weightNdx + offset) % numWeightValues;

						generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), iseInputs).pushBytesToVector(dst);
					}
				}

				if (iseParams.mode == ISEMODE_TRIT || iseParams.mode == ISEMODE_QUINT)
				{
					NormalBlockISEInputs iseInputs = generateDefaultISEInputs(blockParams);
					iseInputs.weight.isGivenInBlockForm = true;

					const int numTQValues			= 1 << (iseParams.mode == ISEMODE_TRIT ? 8 : 7);
					const int numISEBlocksPerBlock	= divRoundUp(numWeights, numValuesInISEBlock);
					const int numBlocks				= divRoundUp(numTQValues, numISEBlocksPerBlock);

					for (int offset = 0;	offset < numValuesInISEBlock;	offset++)
					for (int blockNdx = 0;	blockNdx < numBlocks;			blockNdx++)
					{
						for (int iseBlockNdx = 0; iseBlockNdx < numISEBlocksPerBlock; iseBlockNdx++)
						{
							for (int i = 0; i < numValuesInISEBlock; i++)
								iseInputs.weight.value.block[iseBlockNdx].bitValues[i] = 0;
							iseInputs.weight.value.block[iseBlockNdx].tOrQValue = (blockNdx*numISEBlocksPerBlock + iseBlockNdx + offset) % numTQValues;
						}

						generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), iseInputs).pushBytesToVector(dst);
					}
				}
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_CEMS:
		// For each plane count & partition count combination, generate all color endpoint mode combinations.
		{
			for (int isDualPlane = 0;		isDualPlane <= 1;								isDualPlane++)
			for (int numPartitions = 1;		numPartitions <= (isDualPlane != 0 ? 3 : 4);	numPartitions++)
			{
				// Multi-partition, single-CEM mode.
				if (numPartitions > 1)
				{
					for (deUint32 singleCem = 0; singleCem < 16; singleCem++)
					{
						NormalBlockParams blockParams;
						blockParams.weightGridWidth				= 4;
						blockParams.weightGridHeight			= 4;
						blockParams.isDualPlane					= isDualPlane != 0;
						blockParams.ccs							= 0;
						blockParams.numPartitions				= numPartitions;
						blockParams.isMultiPartSingleCemMode	= true;
						blockParams.colorEndpointModes[0]		= singleCem;
						blockParams.partitionSeed				= 634;

						for (int iseParamsNdx = 0; iseParamsNdx < DE_LENGTH_OF_ARRAY(weightISEParamsCandidates); iseParamsNdx++)
						{
							blockParams.weightISEParams = weightISEParamsCandidates[iseParamsNdx];
							if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y()))
							{
								generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), generateDefaultISEInputs(blockParams)).pushBytesToVector(dst);
								break;
							}
						}
					}
				}

				// Separate-CEM mode.
				for (deUint32 cem0 = 0; cem0 < 16; cem0++)
				for (deUint32 cem1 = 0; cem1 < (numPartitions >= 2 ? 16u : 1u); cem1++)
				for (deUint32 cem2 = 0; cem2 < (numPartitions >= 3 ? 16u : 1u); cem2++)
				for (deUint32 cem3 = 0; cem3 < (numPartitions >= 4 ? 16u : 1u); cem3++)
				{
					NormalBlockParams blockParams;
					blockParams.weightGridWidth				= 4;
					blockParams.weightGridHeight			= 4;
					blockParams.isDualPlane					= isDualPlane != 0;
					blockParams.ccs							= 0;
					blockParams.numPartitions				= numPartitions;
					blockParams.isMultiPartSingleCemMode	= false;
					blockParams.colorEndpointModes[0]		= cem0;
					blockParams.colorEndpointModes[1]		= cem1;
					blockParams.colorEndpointModes[2]		= cem2;
					blockParams.colorEndpointModes[3]		= cem3;
					blockParams.partitionSeed				= 634;

					{
						const deUint32 minCem		= *std::min_element(&blockParams.colorEndpointModes[0], &blockParams.colorEndpointModes[numPartitions]);
						const deUint32 maxCem		= *std::max_element(&blockParams.colorEndpointModes[0], &blockParams.colorEndpointModes[numPartitions]);
						const deUint32 minCemClass	= minCem/4;
						const deUint32 maxCemClass	= maxCem/4;

						if (maxCemClass - minCemClass > 1)
							continue;
					}

					for (int iseParamsNdx = 0; iseParamsNdx < DE_LENGTH_OF_ARRAY(weightISEParamsCandidates); iseParamsNdx++)
					{
						blockParams.weightISEParams = weightISEParamsCandidates[iseParamsNdx];
						if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y()))
						{
							generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), generateDefaultISEInputs(blockParams)).pushBytesToVector(dst);
							break;
						}
					}
				}
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_PARTITION_SEED:
		// Test all partition seeds ("partition pattern indices").
		{
			for (int		numPartitions = 2;	numPartitions <= 4;		numPartitions++)
			for (deUint32	partitionSeed = 0;	partitionSeed < 1<<10;	partitionSeed++)
			{
				NormalBlockParams blockParams;
				blockParams.weightGridWidth				= 4;
				blockParams.weightGridHeight			= 4;
				blockParams.weightISEParams				= ISEParams(ISEMODE_PLAIN_BIT, 2);
				blockParams.isDualPlane					= false;
				blockParams.numPartitions				= numPartitions;
				blockParams.isMultiPartSingleCemMode	= true;
				blockParams.colorEndpointModes[0]		= 0;
				blockParams.partitionSeed				= partitionSeed;

				generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), generateDefaultISEInputs(blockParams)).pushBytesToVector(dst);
			}

			break;
		}

		// \note Fall-through.
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_LDR:
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15:
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15:
		// For each endpoint mode, for each pair of components in the endpoint value, test 10x10 combinations of values for that pair.
		// \note Separate modes for HDR and mode 15 due to different color scales and biases.
		{
			for (deUint32 cem = 0; cem < 16; cem++)
			{
				const bool isHDRCem = cem == 2		||
									  cem == 3		||
									  cem == 7		||
									  cem == 11		||
									  cem == 14		||
									  cem == 15;

				if ((testType == ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_LDR			&& isHDRCem)					||
					(testType == ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15		&& (!isHDRCem || cem == 15))	||
					(testType == ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15		&& cem != 15))
					continue;

				NormalBlockParams blockParams;
				blockParams.weightGridWidth			= 3;
				blockParams.weightGridHeight		= 4;
				blockParams.weightISEParams			= ISEParams(ISEMODE_PLAIN_BIT, 2);
				blockParams.isDualPlane				= false;
				blockParams.numPartitions			= 1;
				blockParams.colorEndpointModes[0]	= cem;

				{
					const int			numBitsForEndpoints		= computeNumBitsForColorEndpoints(blockParams);
					const int			numEndpointParts		= computeNumColorEndpointValues(cem);
					const ISEParams		endpointISE				= computeMaximumRangeISEParams(numBitsForEndpoints, numEndpointParts);
					const int			endpointISERangeMax		= computeISERangeMax(endpointISE);

					for (int endpointPartNdx0 = 0;						endpointPartNdx0 < numEndpointParts; endpointPartNdx0++)
					for (int endpointPartNdx1 = endpointPartNdx0+1;		endpointPartNdx1 < numEndpointParts; endpointPartNdx1++)
					{
						NormalBlockISEInputs	iseInputs			= generateDefaultISEInputs(blockParams);
						const int				numEndpointValues	= de::min(10, endpointISERangeMax+1);

						for (int endpointValueNdx0 = 0; endpointValueNdx0 < numEndpointValues; endpointValueNdx0++)
						for (int endpointValueNdx1 = 0; endpointValueNdx1 < numEndpointValues; endpointValueNdx1++)
						{
							const int endpointValue0 = endpointValueNdx0 * endpointISERangeMax / (numEndpointValues-1);
							const int endpointValue1 = endpointValueNdx1 * endpointISERangeMax / (numEndpointValues-1);

							iseInputs.endpoint.value.plain[endpointPartNdx0] = endpointValue0;
							iseInputs.endpoint.value.plain[endpointPartNdx1] = endpointValue1;

							generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), iseInputs).pushBytesToVector(dst);
						}
					}
				}
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_ENDPOINT_ISE:
		// Similar to ASTCBLOCKTESTTYPE_WEIGHT_ISE, see above.
		{
			static const deUint32 endpointRangeMaximums[] = { 5, 9, 11, 19, 23, 39, 47, 79, 95, 159, 191 };

			for (int endpointRangeNdx = 0; endpointRangeNdx < DE_LENGTH_OF_ARRAY(endpointRangeMaximums); endpointRangeNdx++)
			{
				bool validCaseGenerated = false;

				for (int numPartitions = 1;			!validCaseGenerated && numPartitions <= 4;													numPartitions++)
				for (int isDual = 0;				!validCaseGenerated && isDual <= 1;															isDual++)
				for (int weightISEParamsNdx = 0;	!validCaseGenerated && weightISEParamsNdx < DE_LENGTH_OF_ARRAY(weightISEParamsCandidates);	weightISEParamsNdx++)
				for (int weightGridWidth = 2;		!validCaseGenerated && weightGridWidth <= 12;												weightGridWidth++)
				for (int weightGridHeight = 2;		!validCaseGenerated && weightGridHeight <= 12;												weightGridHeight++)
				{
					NormalBlockParams blockParams;
					blockParams.weightGridWidth				= weightGridWidth;
					blockParams.weightGridHeight			= weightGridHeight;
					blockParams.weightISEParams				= weightISEParamsCandidates[weightISEParamsNdx];
					blockParams.isDualPlane					= isDual != 0;
					blockParams.ccs							= 0;
					blockParams.numPartitions				= numPartitions;
					blockParams.isMultiPartSingleCemMode	= true;
					blockParams.colorEndpointModes[0]		= 12;
					blockParams.partitionSeed				= 634;

					if (isValidBlockParams(blockParams, blockSize.x(), blockSize.y()))
					{
						const ISEParams endpointISEParams = computeMaximumRangeISEParams(computeNumBitsForColorEndpoints(blockParams),
																						 computeNumColorEndpointValues(&blockParams.colorEndpointModes[0], numPartitions, true));

						if (computeISERangeMax(endpointISEParams) == endpointRangeMaximums[endpointRangeNdx])
						{
							validCaseGenerated = true;

							const int numColorEndpoints		= computeNumColorEndpointValues(&blockParams.colorEndpointModes[0], numPartitions, blockParams.isMultiPartSingleCemMode);
							const int numValuesInISEBlock	= endpointISEParams.mode == ISEMODE_TRIT ? 5 : endpointISEParams.mode == ISEMODE_QUINT ? 3 : 1;

							{
								const int				numColorEndpointValues	= (int)computeISERangeMax(endpointISEParams) + 1;
								const int				numBlocks				= divRoundUp(numColorEndpointValues, numColorEndpoints);
								NormalBlockISEInputs	iseInputs				= generateDefaultISEInputs(blockParams);
								iseInputs.endpoint.isGivenInBlockForm = false;

								for (int offset = 0;	offset < numValuesInISEBlock;	offset++)
								for (int blockNdx = 0;	blockNdx < numBlocks;			blockNdx++)
								{
									for (int endpointNdx = 0; endpointNdx < numColorEndpoints; endpointNdx++)
										iseInputs.endpoint.value.plain[endpointNdx] = (blockNdx*numColorEndpoints + endpointNdx + offset) % numColorEndpointValues;

									generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), iseInputs).pushBytesToVector(dst);
								}
							}

							if (endpointISEParams.mode == ISEMODE_TRIT || endpointISEParams.mode == ISEMODE_QUINT)
							{
								NormalBlockISEInputs iseInputs = generateDefaultISEInputs(blockParams);
								iseInputs.endpoint.isGivenInBlockForm = true;

								const int numTQValues			= 1 << (endpointISEParams.mode == ISEMODE_TRIT ? 8 : 7);
								const int numISEBlocksPerBlock	= divRoundUp(numColorEndpoints, numValuesInISEBlock);
								const int numBlocks				= divRoundUp(numTQValues, numISEBlocksPerBlock);

								for (int offset = 0;	offset < numValuesInISEBlock;	offset++)
								for (int blockNdx = 0;	blockNdx < numBlocks;			blockNdx++)
								{
									for (int iseBlockNdx = 0; iseBlockNdx < numISEBlocksPerBlock; iseBlockNdx++)
									{
										for (int i = 0; i < numValuesInISEBlock; i++)
											iseInputs.endpoint.value.block[iseBlockNdx].bitValues[i] = 0;
										iseInputs.endpoint.value.block[iseBlockNdx].tOrQValue = (blockNdx*numISEBlocksPerBlock + iseBlockNdx + offset) % numTQValues;
									}

									generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), iseInputs).pushBytesToVector(dst);
								}
							}
						}
					}
				}

				DE_ASSERT(validCaseGenerated);
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_CCS:
		// For all partition counts, test all values of the CCS (color component selector).
		{
			for (int		numPartitions = 1;		numPartitions <= 3;		numPartitions++)
			for (deUint32	ccs = 0;				ccs < 4;				ccs++)
			{
				NormalBlockParams blockParams;
				blockParams.weightGridWidth				= 3;
				blockParams.weightGridHeight			= 3;
				blockParams.weightISEParams				= ISEParams(ISEMODE_PLAIN_BIT, 2);
				blockParams.isDualPlane					= true;
				blockParams.ccs							= ccs;
				blockParams.numPartitions				= numPartitions;
				blockParams.isMultiPartSingleCemMode	= true;
				blockParams.colorEndpointModes[0]		= 8;
				blockParams.partitionSeed				= 634;

				generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), generateDefaultISEInputs(blockParams)).pushBytesToVector(dst);
			}

			break;
		}

		case ASTCBLOCKTESTTYPE_RANDOM:
		// Generate a number of random (but valid) blocks.
		{
			const int		numBlocks			= 16384;
			de::Random		rnd					(1);
			int				numBlocksGenerated	= 0;

			dst.reserve(numBlocks*ASTC_BLOCK_SIZE_BYTES);

			for (numBlocksGenerated = 0; numBlocksGenerated < numBlocks; numBlocksGenerated++)
			{
				if (rnd.getFloat() < 0.1f)
				{
					// Void extent block.
					const bool		isVoidExtentHDR		= rnd.getBool();
					const deUint16	r					= isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : rnd.getInt(0, 0xffff);
					const deUint16	g					= isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : rnd.getInt(0, 0xffff);
					const deUint16	b					= isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : rnd.getInt(0, 0xffff);
					const deUint16	a					= isVoidExtentHDR ? deFloat32To16(rnd.getFloat(0.0f, 1.0f)) : rnd.getInt(0, 0xffff);
					generateVoidExtentBlock(VoidExtentParams(isVoidExtentHDR, r, g, b, a)).pushBytesToVector(dst);
				}
				else
				{
					// Not void extent block.

					// Generate block params.

					NormalBlockParams blockParams;

					do
					{
						blockParams.weightGridWidth				= rnd.getInt(2, blockSize.x());
						blockParams.weightGridHeight			= rnd.getInt(2, blockSize.y());
						blockParams.weightISEParams				= weightISEParamsCandidates[rnd.getInt(0, DE_LENGTH_OF_ARRAY(weightISEParamsCandidates)-1)];
						blockParams.numPartitions				= rnd.getInt(1, 4);
						blockParams.isMultiPartSingleCemMode	= rnd.getFloat() < 0.25f;
						blockParams.isDualPlane					= blockParams.numPartitions != 4 && rnd.getBool();
						blockParams.ccs							= rnd.getInt(0, 3);
						blockParams.partitionSeed				= rnd.getInt(0, 1023);

						blockParams.colorEndpointModes[0] = rnd.getInt(0, 15);

						{
							const int cemDiff = blockParams.isMultiPartSingleCemMode		? 0
												: blockParams.colorEndpointModes[0] == 0	? 1
												: blockParams.colorEndpointModes[0] == 15	? -1
												: rnd.getBool()								? 1 : -1;

							for (int i = 1; i < blockParams.numPartitions; i++)
								blockParams.colorEndpointModes[i] = blockParams.colorEndpointModes[0] + (cemDiff == -1 ? rnd.getInt(-1, 0) : cemDiff == 1 ? rnd.getInt(0, 1) : 0);
						}
					} while (!isValidBlockParams(blockParams, blockSize.x(), blockSize.y()));

					// Generate ISE inputs for both weight and endpoint data.

					NormalBlockISEInputs iseInputs;

					for (int weightOrEndpoints = 0; weightOrEndpoints <= 1; weightOrEndpoints++)
					{
						const bool			setWeights	= weightOrEndpoints == 0;
						const int			numValues	= setWeights ? computeNumWeights(blockParams) :
														  computeNumColorEndpointValues(&blockParams.colorEndpointModes[0], blockParams.numPartitions, blockParams.isMultiPartSingleCemMode);
						const ISEParams		iseParams	= setWeights ? blockParams.weightISEParams : computeMaximumRangeISEParams(computeNumBitsForColorEndpoints(blockParams), numValues);
						ISEInput&			iseInput	= setWeights ? iseInputs.weight : iseInputs.endpoint;

						iseInput.isGivenInBlockForm = rnd.getBool();

						if (iseInput.isGivenInBlockForm)
						{
							const int numValuesPerISEBlock	= iseParams.mode == ISEMODE_TRIT	? 5
															: iseParams.mode == ISEMODE_QUINT	? 3
															:									  1;
							const int iseBitMax				= (1 << iseParams.numBits) - 1;
							const int numISEBlocks			= divRoundUp(numValues, numValuesPerISEBlock);

							for (int iseBlockNdx = 0; iseBlockNdx < numISEBlocks; iseBlockNdx++)
							{
								iseInput.value.block[iseBlockNdx].tOrQValue = rnd.getInt(0, 255);
								for (int i = 0; i < numValuesPerISEBlock; i++)
									iseInput.value.block[iseBlockNdx].bitValues[i] = rnd.getInt(0, iseBitMax);
							}
						}
						else
						{
							const int rangeMax = computeISERangeMax(iseParams);

							for (int valueNdx = 0; valueNdx < numValues; valueNdx++)
								iseInput.value.plain[valueNdx] = rnd.getInt(0, rangeMax);
						}
					}

					generateNormalBlock(blockParams, blockSize.x(), blockSize.y(), iseInputs).pushBytesToVector(dst);
				}
			}

			break;
		}

		default:
			DE_ASSERT(false);
	}
}

// Get a string describing the data of an ASTC block. Currently contains just hex and bin dumps of the block.
static string astcBlockDataStr (const deUint8* data)
{
	string result;
	result += "  Hexadecimal (big endian: upper left hex digit is block bits 127 to 124):";

	{
		static const char* const hexDigits = "0123456789ABCDEF";

		for (int i = ASTC_BLOCK_SIZE_BYTES-1; i >= 0; i--)
		{
			if ((i+1) % 2 == 0)
				result += "\n    ";
			else
				result += "  ";

			result += hexDigits[(data[i] & 0xf0) >> 4];
			result += " ";
			result += hexDigits[(data[i] & 0x0f) >> 0];
		}
	}

	result += "\n\n  Binary (big endian: upper left bit is block bit 127):";

	for (int i = ASTC_BLOCK_SIZE_BYTES-1; i >= 0; i--)
	{
		if ((i+1) % 2 == 0)
			result += "\n    ";
		else
			result += "  ";

		for (int j = 8-1; j >= 0; j--)
		{
			if (j == 3)
				result += " ";

			result += (data[i] >> j) & 1 ? "1" : "0";
		}
	}

	result += "\n";

	return result;
}

// Compare reference and result block images, reporting also the position of the first non-matching block.
static bool compareBlockImages (const Surface&		reference,
								const Surface&		result,
								const tcu::RGBA&	thresholdRGBA,
								const IVec2&		blockSize,
								int					numNonDummyBlocks,
								IVec2&				firstFailedBlockCoordDst,
								Surface&			errorMaskDst,
								IVec4&				maxDiffDst)
{
	TCU_CHECK_INTERNAL(reference.getWidth() == result.getWidth() && reference.getHeight() == result.getHeight());

	const int		width		= result.getWidth();
	const int		height		= result.getHeight();
	const IVec4		threshold	= thresholdRGBA.toIVec();
	const int		numXBlocks	= width / blockSize.x();

	DE_ASSERT(width % blockSize.x() == 0 && height % blockSize.y() == 0);

	errorMaskDst.setSize(width, height);

	firstFailedBlockCoordDst	= IVec2(-1, -1);
	maxDiffDst					= IVec4(0);

	for (int y = 0; y < height; y++)
	for (int x = 0; x < width; x++)
	{
		const IVec2 blockCoord = IVec2(x, y) / blockSize;

		if (blockCoord.y()*numXBlocks + blockCoord.x() < numNonDummyBlocks)
		{
			const IVec4 refPix = reference.getPixel(x, y).toIVec();

			if (refPix == IVec4(255, 0, 255, 255))
			{
				// ASTC error color - allow anything in result.
				errorMaskDst.setPixel(x, y, tcu::RGBA(255, 0, 255, 255));
				continue;
			}

			const IVec4		resPix		= result.getPixel(x, y).toIVec();
			const IVec4		diff		= tcu::abs(refPix - resPix);
			const bool		isOk		= tcu::boolAll(tcu::lessThanEqual(diff, threshold));

			maxDiffDst = tcu::max(maxDiffDst, diff);

			errorMaskDst.setPixel(x, y, isOk ? tcu::RGBA::green : tcu::RGBA::red);

			if (!isOk && firstFailedBlockCoordDst.x() == -1)
				firstFailedBlockCoordDst = blockCoord;
		}
	}

	return boolAll(lessThanEqual(maxDiffDst, threshold));
}

enum ASTCSupportLevel
{
	// \note Ordered from smallest subset to full, for convenient comparison.
	ASTCSUPPORTLEVEL_NONE = 0,
	ASTCSUPPORTLEVEL_LDR,
	ASTCSUPPORTLEVEL_HDR,
	ASTCSUPPORTLEVEL_FULL
};

static inline ASTCSupportLevel getASTCSupportLevel (const glu::ContextInfo& contextInfo)
{
	const vector<string>& extensions = contextInfo.getExtensions();

	ASTCSupportLevel maxLevel = ASTCSUPPORTLEVEL_NONE;

	for (int extNdx = 0; extNdx < (int)extensions.size(); extNdx++)
	{
		const string& ext = extensions[extNdx];

		maxLevel = de::max(maxLevel, ext == "GL_KHR_texture_compression_astc_ldr"	? ASTCSUPPORTLEVEL_LDR
								   : ext == "GL_KHR_texture_compression_astc_hdr"	? ASTCSUPPORTLEVEL_HDR
								   : ext == "GL_OES_texture_compression_astc"		? ASTCSUPPORTLEVEL_FULL
								   : ASTCSUPPORTLEVEL_NONE);
	}

	return maxLevel;
}

// Class handling the common rendering stuff of ASTC cases.
class ASTCRenderer2D
{
public:
								ASTCRenderer2D		(Context&				context,
													 CompressedTexFormat	format,
													 deUint32				randomSeed);

								~ASTCRenderer2D		(void);

	void						initialize			(int minRenderWidth, int minRenderHeight, const Vec4& colorScale, const Vec4& colorBias);
	void						clear				(void);

	void						render				(Surface&					referenceDst,
													 Surface&					resultDst,
													 const glu::Texture2D&		texture,
													 const tcu::TextureFormat&	uncompressedFormat);

	CompressedTexFormat			getFormat			(void) const { return m_format; }
	IVec2						getBlockSize		(void) const { return m_blockSize; }
	ASTCSupportLevel			getASTCSupport		(void) const { DE_ASSERT(m_initialized); return m_astcSupport;	}

private:
	Context&					m_context;
	TextureRenderer				m_renderer;

	const CompressedTexFormat	m_format;
	const IVec2					m_blockSize;
	ASTCSupportLevel			m_astcSupport;
	Vec4						m_colorScale;
	Vec4						m_colorBias;

	de::Random					m_rnd;

	bool						m_initialized;
};

} // ASTCDecompressionCaseInternal

using namespace ASTCDecompressionCaseInternal;

ASTCRenderer2D::ASTCRenderer2D (Context&			context,
								CompressedTexFormat	format,
								deUint32			randomSeed)
	: m_context			(context)
	, m_renderer		(context.getRenderContext(), context.getTestContext().getLog(), glu::GLSL_VERSION_300_ES, glu::PRECISION_HIGHP)
	, m_format			(format)
	, m_blockSize		(tcu::getBlockPixelSize(format).xy())
	, m_astcSupport		(ASTCSUPPORTLEVEL_NONE)
	, m_colorScale		(-1.0f)
	, m_colorBias		(-1.0f)
	, m_rnd				(randomSeed)
	, m_initialized		(false)
{
	DE_ASSERT(tcu::getBlockPixelSize(format).z() == 1);
}

ASTCRenderer2D::~ASTCRenderer2D (void)
{
	clear();
}

void ASTCRenderer2D::initialize (int minRenderWidth, int minRenderHeight, const Vec4& colorScale, const Vec4& colorBias)
{
	DE_ASSERT(!m_initialized);

	const tcu::RenderTarget&	renderTarget	= m_context.getRenderTarget();
	TestLog&					log				= m_context.getTestContext().getLog();

	m_astcSupport	= getASTCSupportLevel(m_context.getContextInfo());
	m_colorScale	= colorScale;
	m_colorBias		= colorBias;

	switch (m_astcSupport)
	{
		case ASTCSUPPORTLEVEL_NONE:		log << TestLog::Message << "No ASTC support detected" << TestLog::EndMessage;		throw tcu::NotSupportedError("ASTC not supported");
		case ASTCSUPPORTLEVEL_LDR:		log << TestLog::Message << "LDR ASTC support detected" << TestLog::EndMessage;		break;
		case ASTCSUPPORTLEVEL_HDR:		log << TestLog::Message << "HDR ASTC support detected" << TestLog::EndMessage;		break;
		case ASTCSUPPORTLEVEL_FULL:		log << TestLog::Message << "Full ASTC support detected" << TestLog::EndMessage;		break;
		default:
			DE_ASSERT(false);
	}

	if (renderTarget.getWidth() < minRenderWidth || renderTarget.getHeight() < minRenderHeight)
		throw tcu::NotSupportedError("Render target must be at least " + de::toString(minRenderWidth) + "x" + de::toString(minRenderHeight));

	log << TestLog::Message << "Using color scale and bias: result = raw * " << colorScale << " + " << colorBias << TestLog::EndMessage;

	m_initialized = true;
}

void ASTCRenderer2D::clear (void)
{
	m_renderer.clear();
}

void ASTCRenderer2D::render (Surface& referenceDst, Surface& resultDst, const glu::Texture2D& texture, const tcu::TextureFormat& uncompressedFormat)
{
	DE_ASSERT(m_initialized);

	const glw::Functions&			gl						= m_context.getRenderContext().getFunctions();
	const glu::RenderContext&		renderCtx				= m_context.getRenderContext();
	const int						textureWidth			= texture.getRefTexture().getWidth();
	const int						textureHeight			= texture.getRefTexture().getHeight();
	const RandomViewport			viewport				(renderCtx.getRenderTarget(), textureWidth, textureHeight, m_rnd.getUint32());
	ReferenceParams					renderParams			(gls::TextureTestUtil::TEXTURETYPE_2D);
	vector<float>					texCoord;
	gls::TextureTestUtil::computeQuadTexCoord2D(texCoord, Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f));

	renderParams.samplerType	= gls::TextureTestUtil::getSamplerType(uncompressedFormat);
	renderParams.sampler		= Sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::NEAREST, Sampler::NEAREST);
	renderParams.colorScale		= m_colorScale;
	renderParams.colorBias		= m_colorBias;

	// Setup base viewport.
	gl.viewport(viewport.x, viewport.y, viewport.width, viewport.height);

	// Bind to unit 0.
	gl.activeTexture(GL_TEXTURE0);
	gl.bindTexture(GL_TEXTURE_2D, texture.getGLTexture());

	// Setup nearest neighbor filtering and clamp-to-edge.
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,		GL_CLAMP_TO_EDGE);
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,		GL_CLAMP_TO_EDGE);
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,	GL_NEAREST);
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,	GL_NEAREST);

	GLU_EXPECT_NO_ERROR(gl.getError(), "Set texturing state");

	// Issue GL draws.
	m_renderer.renderQuad(0, &texCoord[0], renderParams);
	gl.flush();

	// Compute reference.
	sampleTexture(gls::TextureTestUtil::SurfaceAccess(referenceDst, renderCtx.getRenderTarget().getPixelFormat()), texture.getRefTexture(), &texCoord[0], renderParams);

	// Read GL-rendered image.
	glu::readPixels(renderCtx, viewport.x, viewport.y, resultDst.getAccess());
}

ASTCBlockCase2D::ASTCBlockCase2D (Context&					context,
								  const char*				name,
								  const char*				description,
								  ASTCBlockTestType			testType,
								  CompressedTexFormat		format)
	: TestCase				(context, name, description)
	, m_testType			(testType)
	, m_format				(format)
	, m_numBlocksTested		(0)
	, m_currentIteration	(0)
	, m_renderer			(new ASTCRenderer2D(context, format, deStringHash(getName())))
{
	DE_ASSERT(!(tcu::isAstcSRGBFormat(m_format) && isBlockTestTypeHDROnly(m_testType))); // \note There is no HDR sRGB mode, so these would be redundant.
}

ASTCBlockCase2D::~ASTCBlockCase2D (void)
{
	ASTCBlockCase2D::deinit();
}

void ASTCBlockCase2D::init (void)
{
	m_renderer->initialize(64, 64, getBlockTestTypeColorScale(m_testType), getBlockTestTypeColorBias(m_testType));

	generateBlockCaseTestData(m_blockData, m_format, m_testType);
	DE_ASSERT(!m_blockData.empty());
	DE_ASSERT(m_blockData.size() % ASTC_BLOCK_SIZE_BYTES == 0);

	m_testCtx.getLog() << TestLog::Message << "Total " << m_blockData.size() / ASTC_BLOCK_SIZE_BYTES << " blocks to test" << TestLog::EndMessage
					   << TestLog::Message << "Note: Legitimate ASTC error pixels will be ignored when comparing to reference" << TestLog::EndMessage;
}

void ASTCBlockCase2D::deinit (void)
{
	m_renderer->clear();
	m_blockData.clear();
}

ASTCBlockCase2D::IterateResult ASTCBlockCase2D::iterate (void)
{
	TestLog&						log						= m_testCtx.getLog();

	if (m_renderer->getASTCSupport() == ASTCSUPPORTLEVEL_LDR && isBlockTestTypeHDROnly(m_testType))
	{
		log << TestLog::Message << "Passing the case immediately, since only LDR support was detected and test only contains HDR blocks" << TestLog::EndMessage;
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}

	const IVec2						blockSize				= m_renderer->getBlockSize();
	const int						totalNumBlocks			= (int)m_blockData.size() / ASTC_BLOCK_SIZE_BYTES;
	const int						numXBlocksPerImage		= de::min(m_context.getRenderTarget().getWidth(),  512) / blockSize.x();
	const int						numYBlocksPerImage		= de::min(m_context.getRenderTarget().getHeight(), 512) / blockSize.y();
	const int						numBlocksPerImage		= numXBlocksPerImage * numYBlocksPerImage;
	const int						imageWidth				= numXBlocksPerImage * blockSize.x();
	const int						imageHeight				= numYBlocksPerImage * blockSize.y();
	const int						numBlocksRemaining		= totalNumBlocks - m_numBlocksTested;
	const int						curNumNonDummyBlocks	= de::min(numBlocksPerImage, numBlocksRemaining);
	const int						curNumDummyBlocks		= numBlocksPerImage - curNumNonDummyBlocks;
	const glu::RenderContext&		renderCtx				= m_context.getRenderContext();
	const tcu::RGBA					threshold				= renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + (tcu::isAstcSRGBFormat(m_format) ? tcu::RGBA(2,2,2,2) : tcu::RGBA(1,1,1,1));
	tcu::CompressedTexture			compressed				(m_format, imageWidth, imageHeight);

	if (m_currentIteration == 0)
	{
		log << TestLog::Message << "Using texture of size "
								<< imageWidth << "x" << imageHeight
								<< ", with " << numXBlocksPerImage << " block columns and " << numYBlocksPerImage << " block rows "
								<< ", with block size " << blockSize.x() << "x" << blockSize.y()
			<< TestLog::EndMessage;
	}

	DE_ASSERT(compressed.getDataSize() == numBlocksPerImage*ASTC_BLOCK_SIZE_BYTES);
	deMemcpy(compressed.getData(), &m_blockData[m_numBlocksTested*ASTC_BLOCK_SIZE_BYTES], curNumNonDummyBlocks*ASTC_BLOCK_SIZE_BYTES);
	if (curNumDummyBlocks > 1)
		generateDummyBlocks((deUint8*)compressed.getData() + curNumNonDummyBlocks*ASTC_BLOCK_SIZE_BYTES, curNumDummyBlocks);

	// Create texture and render.

	glu::Texture2D	texture			(renderCtx, m_context.getContextInfo(), 1, &compressed, tcu::TexDecompressionParams((m_renderer->getASTCSupport() == ASTCSUPPORTLEVEL_LDR ? tcu::TexDecompressionParams::ASTCMODE_LDR : tcu::TexDecompressionParams::ASTCMODE_HDR)));
	Surface			renderedFrame	(imageWidth, imageHeight);
	Surface			referenceFrame	(imageWidth, imageHeight);

	m_renderer->render(referenceFrame, renderedFrame, texture, getUncompressedFormat(compressed.getFormat()));

	// Compare and log.
	// \note Since a case can draw quite many images, only log the first iteration and failures.

	{
		Surface		errorMask;
		IVec2		firstFailedBlockCoord;
		IVec4		maxDiff;
		const bool	compareOk = compareBlockImages(referenceFrame, renderedFrame, threshold, blockSize, curNumNonDummyBlocks, firstFailedBlockCoord, errorMask, maxDiff);

		if (m_currentIteration == 0 || !compareOk)
		{
			const char* const		imageSetName	= "ComparisonResult";
			const char* const		imageSetDesc	= "Comparison Result";

			{
				tcu::ScopedLogSection section(log, "Iteration " + de::toString(m_currentIteration),
													"Blocks " + de::toString(m_numBlocksTested) + " to " + de::toString(m_numBlocksTested + curNumNonDummyBlocks - 1));

				if (curNumDummyBlocks > 0)
					log << TestLog::Message << "Note: Only the first " << curNumNonDummyBlocks << " blocks in the image are relevant; rest " << curNumDummyBlocks << " are dummies and not checked" << TestLog::EndMessage;

				if (!compareOk)
				{
					log << TestLog::Message << "Image comparison failed: max difference = " << maxDiff << ", threshold = " << threshold << TestLog::EndMessage
						<< TestLog::ImageSet(imageSetName, imageSetDesc)
						<< TestLog::Image("Result",		"Result",		renderedFrame)
						<< TestLog::Image("Reference",	"Reference",	referenceFrame)
						<< TestLog::Image("ErrorMask",	"Error mask",	errorMask)
						<< TestLog::EndImageSet;

					const int blockNdx = m_numBlocksTested + firstFailedBlockCoord.y()*numXBlocksPerImage + firstFailedBlockCoord.x();
					DE_ASSERT(blockNdx < totalNumBlocks);

					log << TestLog::Message << "First failed block at column " << firstFailedBlockCoord.x() << " and row " << firstFailedBlockCoord.y() << TestLog::EndMessage
						<< TestLog::Message << "Data of first failed block:\n" << astcBlockDataStr(&m_blockData[blockNdx*ASTC_BLOCK_SIZE_BYTES]) << TestLog::EndMessage;

					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
					return STOP;
				}
				else
				{
					log << TestLog::ImageSet(imageSetName, imageSetDesc)
						<< TestLog::Image("Result", "Result", renderedFrame)
						<< TestLog::EndImageSet;
				}
			}

			if (m_numBlocksTested + curNumNonDummyBlocks < totalNumBlocks)
				log << TestLog::Message << "Note: not logging further images unless reference comparison fails" << TestLog::EndMessage;
		}
	}

	m_currentIteration++;
	m_numBlocksTested += curNumNonDummyBlocks;

	if (m_numBlocksTested >= totalNumBlocks)
	{
		DE_ASSERT(m_numBlocksTested == totalNumBlocks);
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}

	return CONTINUE;
}

// Generate a number of trivial dummy blocks to fill unneeded space in a texture.
void ASTCBlockCase2D::generateDummyBlocks (deUint8* dst, int num)
{
	using namespace ASTCBlockGeneratorInternal;

	AssignBlock128 block = generateVoidExtentBlock(VoidExtentParams(false, 0, 0, 0, 0));
	for (int i = 0; i < num; i++)
		block.assignToMemory(&dst[i * ASTC_BLOCK_SIZE_BYTES]);
}

ASTCBlockSizeRemainderCase2D::ASTCBlockSizeRemainderCase2D (Context&			context,
															const char*			name,
															const char*			description,
															CompressedTexFormat	format)
	: TestCase				(context, name, description)
	, m_format				(format)
	, m_currentIteration	(0)
	, m_renderer			(new ASTCRenderer2D(context, format, deStringHash(getName())))
{
}

ASTCBlockSizeRemainderCase2D::~ASTCBlockSizeRemainderCase2D (void)
{
	ASTCBlockSizeRemainderCase2D::deinit();
}

void ASTCBlockSizeRemainderCase2D::init (void)
{
	const IVec2 blockSize = m_renderer->getBlockSize();
	m_renderer->initialize(MAX_NUM_BLOCKS_X*blockSize.x(), MAX_NUM_BLOCKS_Y*blockSize.y(), Vec4(1.0f), Vec4(0.0f));
}

void ASTCBlockSizeRemainderCase2D::deinit (void)
{
	m_renderer->clear();
}

ASTCBlockSizeRemainderCase2D::IterateResult ASTCBlockSizeRemainderCase2D::iterate (void)
{
	TestLog&						log						= m_testCtx.getLog();
	const IVec2						blockSize				= m_renderer->getBlockSize();
	const int						curRemainderX			= m_currentIteration % blockSize.x();
	const int						curRemainderY			= m_currentIteration / blockSize.x();
	const int						imageWidth				= (MAX_NUM_BLOCKS_X-1)*blockSize.x() + curRemainderX;
	const int						imageHeight				= (MAX_NUM_BLOCKS_Y-1)*blockSize.y() + curRemainderY;
	const int						numBlocksX				= divRoundUp(imageWidth, blockSize.x());
	const int						numBlocksY				= divRoundUp(imageHeight, blockSize.y());
	const int						totalNumBlocks			= numBlocksX * numBlocksY;
	const glu::RenderContext&		renderCtx				= m_context.getRenderContext();
	const tcu::RGBA					threshold				= renderCtx.getRenderTarget().getPixelFormat().getColorThreshold() + (tcu::isAstcSRGBFormat(m_format) ? tcu::RGBA(2,2,2,2) : tcu::RGBA(1,1,1,1));
	tcu::CompressedTexture			compressed				(m_format, imageWidth, imageHeight);

	DE_ASSERT(compressed.getDataSize() == totalNumBlocks*ASTC_BLOCK_SIZE_BYTES);
	generateDefaultBlockData((deUint8*)compressed.getData(), totalNumBlocks, blockSize.x(), blockSize.y());

	// Create texture and render.

	Surface			renderedFrame	(imageWidth, imageHeight);
	Surface			referenceFrame	(imageWidth, imageHeight);
	glu::Texture2D	texture			(renderCtx, m_context.getContextInfo(), 1, &compressed, tcu::TexDecompressionParams(m_renderer->getASTCSupport() == ASTCSUPPORTLEVEL_LDR ? tcu::TexDecompressionParams::ASTCMODE_LDR : tcu::TexDecompressionParams::ASTCMODE_HDR));

	m_renderer->render(referenceFrame, renderedFrame, texture, getUncompressedFormat(compressed.getFormat()));

	{
		// Compare and log.

		tcu::ScopedLogSection section(log, "Iteration " + de::toString(m_currentIteration),
										   "Remainder " + de::toString(curRemainderX) + "x" + de::toString(curRemainderY));

		log << TestLog::Message << "Using texture of size "
								<< imageWidth << "x" << imageHeight
								<< " and block size "
								<< blockSize.x() << "x" << blockSize.y()
								<< "; the x and y remainders are "
								<< curRemainderX << " and " << curRemainderY << " respectively"
			<< TestLog::EndMessage;

		const bool compareOk = tcu::pixelThresholdCompare(m_testCtx.getLog(), "ComparisonResult", "Comparison Result", referenceFrame, renderedFrame, threshold,
														  m_currentIteration == 0 ? tcu::COMPARE_LOG_RESULT : tcu::COMPARE_LOG_ON_ERROR);

		if (!compareOk)
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Image comparison failed");
			return STOP;
		}
	}

	if (m_currentIteration == 0 && m_currentIteration+1 < blockSize.x()*blockSize.y())
		log << TestLog::Message << "Note: not logging further images unless reference comparison fails" << TestLog::EndMessage;

	m_currentIteration++;

	if (m_currentIteration >= blockSize.x()*blockSize.y())
	{
		DE_ASSERT(m_currentIteration == blockSize.x()*blockSize.y());
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
	return CONTINUE;
}

void ASTCBlockSizeRemainderCase2D::generateDefaultBlockData (deUint8* dst, int numBlocks, int blockWidth, int blockHeight)
{
	using namespace ASTCBlockGeneratorInternal;

	NormalBlockParams blockParams;

	blockParams.weightGridWidth			= 3;
	blockParams.weightGridHeight		= 3;
	blockParams.weightISEParams			= ISEParams(ISEMODE_PLAIN_BIT, 5);
	blockParams.isDualPlane				= false;
	blockParams.numPartitions			= 1;
	blockParams.colorEndpointModes[0]	= 8;

	NormalBlockISEInputs iseInputs = generateDefaultISEInputs(blockParams);
	iseInputs.weight.isGivenInBlockForm = false;

	const int numWeights		= computeNumWeights(blockParams);
	const int weightRangeMax	= computeISERangeMax(blockParams.weightISEParams);

	for (int blockNdx = 0; blockNdx < numBlocks; blockNdx++)
	{
		for (int weightNdx = 0; weightNdx < numWeights; weightNdx++)
			iseInputs.weight.value.plain[weightNdx] = (blockNdx*numWeights + weightNdx) * weightRangeMax / (numBlocks*numWeights-1);

		generateNormalBlock(blockParams, blockWidth, blockHeight, iseInputs).assignToMemory(dst + blockNdx*ASTC_BLOCK_SIZE_BYTES);
	}
}

const char* getBlockTestTypeName (ASTCBlockTestType testType)
{
	switch (testType)
	{
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_LDR:				return "void_extent_ldr";
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR:				return "void_extent_hdr";
		case ASTCBLOCKTESTTYPE_WEIGHT_GRID:					return "weight_grid";
		case ASTCBLOCKTESTTYPE_WEIGHT_ISE:					return "weight_ise";
		case ASTCBLOCKTESTTYPE_CEMS:						return "color_endpoint_modes";
		case ASTCBLOCKTESTTYPE_PARTITION_SEED:				return "partition_pattern_index";
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_LDR:			return "endpoint_value_ldr";
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15:	return "endpoint_value_hdr_cem_not_15";
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15:		return "endpoint_value_hdr_cem_15";
		case ASTCBLOCKTESTTYPE_ENDPOINT_ISE:				return "endpoint_ise";
		case ASTCBLOCKTESTTYPE_CCS:							return "color_component_selector";
		case ASTCBLOCKTESTTYPE_RANDOM:						return "random";
		default:
			DE_ASSERT(false);
			return DE_NULL;
	}
}

const char* getBlockTestTypeDescription (ASTCBlockTestType testType)
{
	switch (testType)
	{
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_LDR:				return "Test void extent block, LDR mode";
		case ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR:				return "Test void extent block, HDR mode";
		case ASTCBLOCKTESTTYPE_WEIGHT_GRID:					return "Test combinations of plane count, weight integer sequence encoding parameters, and weight grid size";
		case ASTCBLOCKTESTTYPE_WEIGHT_ISE:					return "Test different integer sequence encoding block values for weight grid";
		case ASTCBLOCKTESTTYPE_CEMS:						return "Test different color endpoint mode combinations, combined with different plane and partition counts";
		case ASTCBLOCKTESTTYPE_PARTITION_SEED:				return "Test different partition pattern indices";
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_LDR:			return "Test various combinations of each pair of color endpoint values, for each LDR color endpoint mode";
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15:	return "Test various combinations of each pair of color endpoint values, for each HDR color endpoint mode other than mode 15";
		case ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15:		return "Test various combinations of each pair of color endpoint values, HDR color endpoint mode 15";
		case ASTCBLOCKTESTTYPE_ENDPOINT_ISE:				return "Test different integer sequence encoding block values for color endpoints";
		case ASTCBLOCKTESTTYPE_CCS:							return "Test color component selector, for different partition counts";
		case ASTCBLOCKTESTTYPE_RANDOM:						return "Random block test";
		default:
			DE_ASSERT(false);
			return DE_NULL;
	}
}

bool isBlockTestTypeHDROnly (ASTCBlockTestType testType)
{
	return testType == ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR			||
		   testType == ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15	||
		   testType == ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15;
}

} // Functional
} // gles3
} // deqp
