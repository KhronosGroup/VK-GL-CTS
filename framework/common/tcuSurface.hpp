#ifndef _TCUSURFACE_HPP
#define _TCUSURFACE_HPP
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
 * \brief RGBA8888 surface class.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuRGBA.hpp"
#include "tcuTexture.hpp"

#include <vector>

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief RGBA8888 surface
 *
 * Surface provides basic pixel storage functionality. Only single format
 * (RGBA8888) is supported.
 *
 * PixelBufferAccess (see tcuTexture.h) provides much more flexible API
 * for handling various pixel formats. This is mainly a convinience class.
 *//*--------------------------------------------------------------------*/
class Surface
{
public:
							Surface				(void);
							Surface				(int width, int height);
							~Surface			(void);

	void					setSize				(int width, int height);

	int						getWidth			(void) const { return m_width;  }
	int						getHeight			(void) const { return m_height; }

	void					setPixel			(int x, int y, RGBA col);
	RGBA					getPixel			(int x, int y) const;

	ConstPixelBufferAccess	getAccess			(void) const;
	PixelBufferAccess		getAccess			(void);

	ConstPixelBufferAccess	getSubAccess		(int x, int y, int width, int height) const;
	PixelBufferAccess		getSubAccess		(int x, int y, int width, int height);

private:
	// \note Copy constructor and assignment operators are public and auto-generated

	int						m_width;
	int 					m_height;
	std::vector<deUint32>	m_pixels;
};

inline void Surface::setPixel (int x, int y, RGBA col)
{
	DE_ASSERT(de::inBounds(x, 0, m_width) && de::inBounds(y, 0, m_height));

	const int		pixOffset	= y*m_width + x;
	deUint32*		pixAddr		= &m_pixels[pixOffset];

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
	*pixAddr = col.getPacked();
#else
	*((deUint8*)pixAddr + 0) = (deUint8)col.getRed();
	*((deUint8*)pixAddr + 1) = (deUint8)col.getGreen();
	*((deUint8*)pixAddr + 2) = (deUint8)col.getBlue();
	*((deUint8*)pixAddr + 3) = (deUint8)col.getAlpha();
#endif
}

inline RGBA Surface::getPixel (int x, int y) const
{
	DE_ASSERT(de::inBounds(x, 0, m_width) && de::inBounds(y, 0, m_height));

	const int		pixOffset	= y*m_width + x;
	const deUint32*	pixAddr		= &m_pixels[pixOffset];

	DE_STATIC_ASSERT(RGBA::RED_SHIFT == 0 && RGBA::GREEN_SHIFT == 8 && RGBA::BLUE_SHIFT == 16 && RGBA::ALPHA_SHIFT == 24);

#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
	return RGBA(*pixAddr);
#else
	const deUint8*	byteAddr	= (const deUint8*)pixAddr;
	return RGBA(byteAddr[0], byteAddr[1], byteAddr[2], byteAddr[3]);
#endif
}


/** Get pixel sub buffer access from surface. */
inline ConstPixelBufferAccess Surface::getSubAccess (int x, int y, int width, int height) const
{
	DE_ASSERT(x >= 0);
	DE_ASSERT(width >= 1);
	DE_ASSERT(x < m_width);

	DE_ASSERT(y >= 0);
	DE_ASSERT(height >= 1);
	DE_ASSERT(y < m_height);

	DE_ASSERT(x + width <= m_width);
	DE_ASSERT(y + height <= m_height);

	const deUint8* ptr = (m_pixels.empty() ? NULL : ((deUint8*)&m_pixels[0]) + 4 * (x + y * m_width));
	return ConstPixelBufferAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), width, height, 1, m_width*4, 0, ptr);
}

/** Get pixel sub buffer access from surface. */
inline PixelBufferAccess Surface::getSubAccess (int x, int y, int width, int height)
{
	DE_ASSERT(x >= 0);
	DE_ASSERT(width >= 1);
	DE_ASSERT(x < m_width);

	DE_ASSERT(y >= 0);
	DE_ASSERT(height >= 1);
	DE_ASSERT(y < m_height);

	DE_ASSERT(x + width <= m_width);
	DE_ASSERT(y + height <= m_height);

	deUint8* ptr = (m_pixels.empty() ? NULL : ((deUint8*)&m_pixels[0]) + 4 * (x + y * m_width));
	return PixelBufferAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), width, height, 1, m_width*4, 0, ptr);
}

/** Get pixel buffer access from surface. */
inline ConstPixelBufferAccess Surface::getAccess (void) const
{
	return ConstPixelBufferAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), m_width, m_height, 1, m_pixels.empty() ? DE_NULL : &m_pixels[0]);
}

/** Get pixel buffer access from surface. */
inline PixelBufferAccess Surface::getAccess (void)
{
	return PixelBufferAccess(TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8), m_width, m_height, 1, m_pixels.empty() ? DE_NULL : &m_pixels[0]);
}

} // tcu

#endif // _TCUSURFACE_HPP
