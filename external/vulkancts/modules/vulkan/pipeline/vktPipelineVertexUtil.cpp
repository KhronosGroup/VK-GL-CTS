/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
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
 * \brief Utilities for vertex buffers.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineVertexUtil.hpp"
#include "tcuVectorUtil.hpp"

namespace vkt
{
namespace pipeline
{

std::vector<Vertex4RGBA> createOverlappingQuads (void)
{
	using tcu::Vec2;
	using tcu::Vec4;

	std::vector<Vertex4RGBA> vertices;

	const Vec2 translations[4] =
	{
		Vec2(-0.25f, -0.25f),
		Vec2(-1.0f, -0.25f),
		Vec2(-1.0f, -1.0f),
		Vec2(-0.25f, -1.0f)
	};

	const Vec4 quadColors[4] =
	{
		Vec4(1.0f, 0.0f, 0.0f, 1.0),
		Vec4(0.0f, 1.0f, 0.0f, 1.0),
		Vec4(0.0f, 0.0f, 1.0f, 1.0),
		Vec4(1.0f, 0.0f, 1.0f, 1.0)
	};

	const float quadSize = 1.25f;

	for (int quadNdx = 0; quadNdx < 4; quadNdx++)
	{
		const Vec2&	translation	= translations[quadNdx];
		const Vec4&	color		= quadColors[quadNdx];

		const Vertex4RGBA lowerLeftVertex =
		{
			Vec4(translation.x(), translation.y(), 0.0f, 1.0f),
			color
		};
		const Vertex4RGBA upperLeftVertex =
		{
			Vec4(translation.x(), translation.y() + quadSize, 0.0f, 1.0f),
			color
		};
		const Vertex4RGBA lowerRightVertex =
		{
			Vec4(translation.x() + quadSize, translation.y(), 0.0f, 1.0f),
			color
		};
		const Vertex4RGBA upperRightVertex =
		{
			Vec4(translation.x() + quadSize, translation.y() + quadSize, 0.0f, 1.0f),
			color
		};

		// Triangle 1, CCW
		vertices.push_back(lowerLeftVertex);
		vertices.push_back(lowerRightVertex);
		vertices.push_back(upperLeftVertex);

		// Triangle 2, CW
		vertices.push_back(lowerRightVertex);
		vertices.push_back(upperLeftVertex);
		vertices.push_back(upperRightVertex);
	}

	return vertices;
}

} // pipeline
} // vkt
