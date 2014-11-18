#ifndef _ES3FASTCDECOMPRESSIONCASES_HPP
#define _ES3FASTCDECOMPRESSIONCASES_HPP
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
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "tes3TestCase.hpp"
#include "tcuCompressedTexture.hpp"
#include "deUniquePtr.hpp"

#include <vector>

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace ASTCDecompressionCaseInternal
{

class ASTCRenderer2D;

}

enum ASTCBlockTestType
{
	ASTCBLOCKTESTTYPE_VOID_EXTENT_LDR = 0,
	ASTCBLOCKTESTTYPE_VOID_EXTENT_HDR,
	ASTCBLOCKTESTTYPE_WEIGHT_GRID,
	ASTCBLOCKTESTTYPE_WEIGHT_ISE,
	ASTCBLOCKTESTTYPE_CEMS,
	ASTCBLOCKTESTTYPE_PARTITION_SEED,
	ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_LDR,
	ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_NO_15,
	ASTCBLOCKTESTTYPE_ENDPOINT_VALUE_HDR_15,
	ASTCBLOCKTESTTYPE_ENDPOINT_ISE,
	ASTCBLOCKTESTTYPE_CCS,
	ASTCBLOCKTESTTYPE_RANDOM,

	ASTCBLOCKTESTTYPE_LAST
};

// General ASTC block test class.
class ASTCBlockCase2D : public TestCase
{
public:
																	ASTCBlockCase2D			(Context&						context,
																							 const char*					name,
																							 const char*					description,
																							 ASTCBlockTestType				testType,
																							 tcu::CompressedTexFormat		format);
																	~ASTCBlockCase2D		(void);

	void															init					(void);
	void															deinit					(void);
	IterateResult													iterate					(void);

private:
	static void														generateDummyBlocks		(deUint8* dst, int num);

																	ASTCBlockCase2D			(const ASTCBlockCase2D& other);
	ASTCBlockCase2D&												operator=				(const ASTCBlockCase2D& other);

	const ASTCBlockTestType											m_testType;
	const tcu::CompressedTexFormat									m_format;
	std::vector<deUint8>											m_blockData;

	int																m_numBlocksTested;
	int																m_currentIteration;

	de::UniquePtr<ASTCDecompressionCaseInternal::ASTCRenderer2D>	m_renderer;
};

// For a format with block size (W, H), test with texture sizes {(k*W + a, k*H + b) | 0 <= a < W, 0 <= b < H } .
class ASTCBlockSizeRemainderCase2D : public TestCase
{
public:
																	ASTCBlockSizeRemainderCase2D	(Context&						context,
																									 const char*					name,
																									 const char*					description,
																									 tcu::CompressedTexFormat		format);
																	~ASTCBlockSizeRemainderCase2D	(void);

	void															init							(void);
	void															deinit							(void);
	IterateResult													iterate							(void);

private:
	enum
	{
		MAX_NUM_BLOCKS_X = 5,
		MAX_NUM_BLOCKS_Y = 5
	};

	static void														generateDefaultBlockData		(deUint8* dst, int numBlocks, int blockWidth, int blockHeight);

																	ASTCBlockSizeRemainderCase2D	(const ASTCBlockSizeRemainderCase2D& other);
	ASTCBlockSizeRemainderCase2D&									operator=						(const ASTCBlockSizeRemainderCase2D& other);

	const tcu::CompressedTexFormat									m_format;

	int																m_currentIteration;

	de::UniquePtr<ASTCDecompressionCaseInternal::ASTCRenderer2D>	m_renderer;
};

const char*		getBlockTestTypeName			(ASTCBlockTestType testType);
const char*		getBlockTestTypeDescription		(ASTCBlockTestType testType);
bool			isBlockTestTypeHDROnly			(ASTCBlockTestType testType);

} // Functional
} // gles3
} // deqp

#endif // _ES3FASTCDECOMPRESSIONCASES_HPP
