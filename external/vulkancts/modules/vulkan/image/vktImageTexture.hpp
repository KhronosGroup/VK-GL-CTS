#ifndef _VKTIMAGETEXTURE_HPP
#define _VKTIMAGETEXTURE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Texture utility class
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktImageTestsUtil.hpp"

namespace vkt
{
namespace image
{

//! Texture buffer/image abstraction. Helps managing size and number of layers.
class Texture
{
public:
						Texture			(const ImageType type, const tcu::IVec3& layerSize, const int layers);

	ImageType			type			(void) const { return m_type; }			//!< Texture type
	tcu::IVec3			layerSize		(void) const { return m_layerSize; }	//!< Size of a single layer
	int					numLayers		(void) const { return m_numLayers; }	//!< Number of array layers (for array and cube types)

	tcu::IVec3			size			(void) const;	//!< Size including number of layers in additional dimension (e.g. z in 2d texture)
	int					dimension		(void) const;	//!< Coordinate dimension used for addressing (e.g. 3 (x,y,z) for 2d array)
	int					layerDimension	(void) const;	//!< Coordinate dimension used for addressing a single layer (e.g. 2 (x,y) for 2d array)

private:
	const tcu::IVec3	m_layerSize;
	const ImageType		m_type;
	const int			m_numLayers;
};

inline bool isCube (const Texture& texture)
{
	return texture.type() == IMAGE_TYPE_CUBE || texture.type() == IMAGE_TYPE_CUBE_ARRAY;
}

} // image
} // vkt

#endif // _VKTIMAGETEXTURE_HPP
