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

std::vector<Vertex4Tex4> createFullscreenQuad (void)
{
	using tcu::Vec4;

	const Vertex4Tex4 lowerLeftVertex =
	{
		Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
		Vec4(0.0f, 0.0f, 0.0f, 0.0f)
	};
	const Vertex4Tex4 upperLeftVertex =
	{
		Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
		Vec4(0.0f, 1.0f, 0.0f, 0.0f)
	};
	const Vertex4Tex4 lowerRightVertex =
	{
		Vec4(1.0f, -1.0f, 0.0f, 1.0f),
		Vec4(1.0f, 0.0f, 0.0f, 0.0f)
	};
	const Vertex4Tex4 upperRightVertex =
	{
		Vec4(1.0f, 1.0f, 0.0f, 1.0f),
		Vec4(1.0f, 1.0f, 0.0f, 0.0f)
	};

	const Vertex4Tex4 vertices[6] =
	{
		lowerLeftVertex,
		lowerRightVertex,
		upperLeftVertex,

		upperLeftVertex,
		lowerRightVertex,
		upperRightVertex
	};

	return std::vector<Vertex4Tex4>(vertices, vertices + DE_LENGTH_OF_ARRAY(vertices));
}

std::vector<Vertex4Tex4> createQuadMosaic (int rows, int columns)
{
	using tcu::Vec4;

	DE_ASSERT(rows >= 1);
	DE_ASSERT(columns >= 1);

	std::vector<Vertex4Tex4>	vertices;
	const float					rowSize		= 2.0f / (float)rows;
	const float					columnSize	= 2.0f / (float)columns;
	int							arrayIndex	= 0;

	for (int rowNdx = 0; rowNdx < rows; rowNdx++)
	{
		for (int columnNdx = 0; columnNdx < columns; columnNdx++)
		{
			const Vertex4Tex4 lowerLeftVertex =
			{
				Vec4(-1.0f + (float)columnNdx * columnSize, -1.0f + (float)rowNdx * rowSize, 0.0f, 1.0f),
				Vec4(0.0f, 0.0f, (float)arrayIndex, 0.0f)
			};
			const Vertex4Tex4 upperLeftVertex =
			{
				Vec4(lowerLeftVertex.position.x(), lowerLeftVertex.position.y() + rowSize, 0.0f, 1.0f),
				Vec4(0.0f, 1.0f, (float)arrayIndex, 0.0f)
			};
			const Vertex4Tex4 lowerRightVertex =
			{
				Vec4(lowerLeftVertex.position.x() + columnSize, lowerLeftVertex.position.y(), 0.0f, 1.0f),
				Vec4(1.0f, 0.0f, (float)arrayIndex, 0.0f)
			};
			const Vertex4Tex4 upperRightVertex =
			{
				Vec4(lowerLeftVertex.position.x() + columnSize, lowerLeftVertex.position.y() + rowSize, 0.0f, 1.0f),
				Vec4(1.0f, 1.0f, (float)arrayIndex, 0.0f)
			};

			vertices.push_back(lowerLeftVertex);
			vertices.push_back(lowerRightVertex);
			vertices.push_back(upperLeftVertex);
			vertices.push_back(upperLeftVertex);
			vertices.push_back(lowerRightVertex);
			vertices.push_back(upperRightVertex);

			arrayIndex++;
		}
	}

	return vertices;
}

std::vector<Vertex4Tex4> createQuadMosaicCube (void)
{
	using tcu::Vec3;

	static const Vec3 texCoordsCube[8] =
	{
		Vec3(-1.0f, -1.0f, -1.0f),	// 0: -X, -Y, -Z
		Vec3(1.0f, -1.0f, -1.0f),	// 1:  X, -Y, -Z
		Vec3(1.0f, -1.0f, 1.0f),	// 2:  X, -Y,  Z
		Vec3(-1.0f, -1.0f, 1.0f),	// 3: -X, -Y,  Z

		Vec3(-1.0f, 1.0f, -1.0f),	// 4: -X,  Y, -Z
		Vec3(1.0f, 1.0f, -1.0f),	// 5:  X,  Y, -Z
		Vec3(1.0f, 1.0f, 1.0f),		// 6:  X,  Y,  Z
		Vec3(-1.0f, 1.0f, 1.0f),	// 7: -X,  Y,  Z
	};

	static const int texCoordCubeIndices[6][6] =
	{
		{ 6, 5, 2, 2, 5, 1 },		// +X face
		{ 3, 0, 7, 7, 0, 4 },		// -X face
		{ 4, 5, 7, 7, 5, 6 },		// +Y face
		{ 3, 2, 0, 0, 2, 1 },		// -Y face
		{ 2, 3, 6, 6, 3, 7 },		// +Z face
		{ 0, 1, 4, 4, 1, 5 }		// -Z face
	};

	// Create 6 quads and set appropriate texture coordinates for cube mapping

	std::vector<Vertex4Tex4>			vertices	= createQuadMosaic(2, 3);
	std::vector<Vertex4Tex4>::iterator	vertexItr	= vertices.begin();

	for (int quadNdx = 0; quadNdx < 6; quadNdx++)
	{
		for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
		{
			vertexItr->texCoord.xyz() = texCoordsCube[texCoordCubeIndices[quadNdx][vertexNdx]];
			vertexItr++;
		}
	}

	return vertices;
}

std::vector<Vertex4Tex4> createQuadMosaicCubeArray (int faceArrayIndices[6])
{
	std::vector<Vertex4Tex4>			vertices	= createQuadMosaicCube();
	std::vector<Vertex4Tex4>::iterator	vertexItr	= vertices.begin();

	for (int quadNdx = 0; quadNdx < 6; quadNdx++)
	{
		for (int vertexNdx = 0; vertexNdx < 6; vertexNdx++)
		{
			vertexItr->texCoord.w() = (float)faceArrayIndices[quadNdx];
			vertexItr++;
		}
	}

	return vertices;
}

std::vector<Vertex4Tex4> createTestQuadMosaic (vk::VkImageViewType viewType)
{
	std::vector<Vertex4Tex4> vertices;

	switch (viewType)
	{
		case vk::VK_IMAGE_VIEW_TYPE_1D:
		case vk::VK_IMAGE_VIEW_TYPE_2D:
			vertices = createFullscreenQuad();
			break;

		case vk::VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			vertices = createQuadMosaic(2, 3);

			// Set up array indices
			for (size_t quadNdx = 0; quadNdx < 6; quadNdx++)
				for (size_t vertexNdx = 0; vertexNdx < 6; vertexNdx++)
					vertices[quadNdx * 6 + vertexNdx].texCoord.y() = (float)quadNdx;

			break;

		case vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			vertices = createQuadMosaic(2, 3);
			break;

		case vk::VK_IMAGE_VIEW_TYPE_3D:
			vertices = createQuadMosaic(2, 3);

			// Use z between 0.0 and 1.0.
			for (size_t vertexNdx = 0; vertexNdx < vertices.size(); vertexNdx++)
				vertices[vertexNdx].texCoord.z() /= 5.0f;

			break;

		case vk::VK_IMAGE_VIEW_TYPE_CUBE:
			vertices = createQuadMosaicCube();
			break;

		case vk::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			{
				int faceArrayIndices[6] = { 0, 1, 2, 3, 4, 5 };
				vertices = createQuadMosaicCubeArray(faceArrayIndices);
			}
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	return vertices;
}

} // pipeline
} // vkt
