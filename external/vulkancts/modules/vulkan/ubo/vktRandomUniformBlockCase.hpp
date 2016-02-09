#ifndef _VKTRANDOMUNIFORMBLOCKCASE_HPP
#define _VKTRANDOMUNIFORMBLOCKCASE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
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
 * \brief Random uniform block layout case.
 *//*--------------------------------------------------------------------*/

#include "vktUniformBlockCase.hpp"

namespace de
{
class Random;
} // de

namespace vkt
{
namespace ubo
{

enum FeatureBits
{
	FEATURE_VECTORS				= (1<<0),
	FEATURE_MATRICES			= (1<<1),
	FEATURE_ARRAYS				= (1<<2),
	FEATURE_STRUCTS				= (1<<3),
	FEATURE_NESTED_STRUCTS		= (1<<4),
	FEATURE_INSTANCE_ARRAYS		= (1<<5),
	FEATURE_VERTEX_BLOCKS		= (1<<6),
	FEATURE_FRAGMENT_BLOCKS		= (1<<7),
	FEATURE_SHARED_BLOCKS		= (1<<8),
	FEATURE_UNUSED_UNIFORMS		= (1<<9),
	FEATURE_UNUSED_MEMBERS		= (1<<10),
	FEATURE_PACKED_LAYOUT		= (1<<12),
	FEATURE_SHARED_LAYOUT		= (1<<13),
	FEATURE_STD140_LAYOUT		= (1<<14),
	FEATURE_MATRIX_LAYOUT		= (1<<15),	//!< Matrix layout flags.
	FEATURE_ARRAYS_OF_ARRAYS	= (1<<16)
};

class RandomUniformBlockCase : public UniformBlockCase
{
public:
							RandomUniformBlockCase		(tcu::TestContext&		testCtx,
														 const std::string&		name,
														 const std::string&		description,
														 BufferMode				bufferMode,
														 deUint32				features,
														 deUint32				seed);

private:
	void					generateBlock				(de::Random& rnd, deUint32 layoutFlags);
	void					generateUniform				(de::Random& rnd, UniformBlock& block);
	VarType					generateType				(de::Random& rnd, int typeDepth, bool arrayOk);

	const deUint32			m_features;
	const int				m_maxVertexBlocks;
	const int				m_maxFragmentBlocks;
	const int				m_maxSharedBlocks;
	const int				m_maxInstances;
	const int				m_maxArrayLength;
	const int				m_maxStructDepth;
	const int				m_maxBlockMembers;
	const int				m_maxStructMembers;
	const deUint32			m_seed;

	int						m_blockNdx;
	int						m_uniformNdx;
	int						m_structNdx;
};

} // ubo
} // vkt

#endif // _VKTRANDOMUNIFORMBLOCKCASE_HPP
