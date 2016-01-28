/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Mobica Ltd.
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
 * \brief Texture utility class
 *//*--------------------------------------------------------------------*/

#include "vktImageTexture.hpp"

namespace vkt
{
namespace image
{

Texture::Texture (const ImageType type_, const tcu::IVec3& layerSize_, const int layers)
	: m_layerSize	(layerSize_)
	, m_type		(type_)
	, m_numLayers	(layers)
{
	DE_ASSERT(m_numLayers >= 1);
	DE_ASSERT(m_layerSize.x() >= 1 && m_layerSize.y() >= 1 && m_layerSize.z() >= 1);

	switch (type_)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			DE_ASSERT(m_numLayers == 1);
			DE_ASSERT(m_layerSize.y() == 1 && m_layerSize.z() == 1);
			break;

		case IMAGE_TYPE_1D_ARRAY:
			DE_ASSERT(m_layerSize.y() == 1 && m_layerSize.z() == 1);
			break;

		case IMAGE_TYPE_2D:
			DE_ASSERT(m_numLayers == 1);
			DE_ASSERT(m_layerSize.z() == 1);
			break;

		case IMAGE_TYPE_2D_ARRAY:
			DE_ASSERT(m_layerSize.z() == 1);
			break;

		case IMAGE_TYPE_CUBE:
			DE_ASSERT(m_numLayers == 6);
			DE_ASSERT(m_layerSize.z() == 1);
			break;

		case IMAGE_TYPE_CUBE_ARRAY:
			DE_ASSERT(m_numLayers >= 6 && m_numLayers % 6 == 0);
			DE_ASSERT(m_layerSize.z() == 1);
			break;

		case IMAGE_TYPE_3D:
			DE_ASSERT(m_numLayers == 1);
			break;

		default:
			DE_FATAL("Internal error");
			break;
	}
}

tcu::IVec3 Texture::size (void) const
{
	switch (m_type)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_3D:
			return m_layerSize;

		case IMAGE_TYPE_1D_ARRAY:
			return tcu::IVec3(m_layerSize.x(), m_numLayers, 1);

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return tcu::IVec3(m_layerSize.x(), m_layerSize.y(), m_numLayers);

		default:
			DE_FATAL("Internal error");
			return tcu::IVec3();
	}
}

int Texture::dimension (void) const
{
	switch (m_type)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
			return 1;

		case IMAGE_TYPE_1D_ARRAY:
		case IMAGE_TYPE_2D:
			return 2;

		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
		case IMAGE_TYPE_3D:
			return 3;

		default:
			DE_FATAL("Internal error");
			return 0;
	}
}

int Texture::layerDimension (void) const
{
	switch (m_type)
	{
		case IMAGE_TYPE_1D:
		case IMAGE_TYPE_BUFFER:
		case IMAGE_TYPE_1D_ARRAY:
			return 1;

		case IMAGE_TYPE_2D:
		case IMAGE_TYPE_2D_ARRAY:
		case IMAGE_TYPE_CUBE:
		case IMAGE_TYPE_CUBE_ARRAY:
			return 2;

		case IMAGE_TYPE_3D:
			return 3;

		default:
			DE_FATAL("Internal error");
			return 0;
	}
}

} // image
} // vkt
