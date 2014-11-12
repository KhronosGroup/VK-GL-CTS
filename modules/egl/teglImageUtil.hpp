#ifndef _TEGLIMAGEUTIL_HPP
#define _TEGLIMAGEUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program EGL Module
 * ---------------------------------------
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
 * \brief Common utilities for EGL images.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTexture.hpp"

#include "deUniquePtr.hpp"

#include "teglTestCase.hpp"

#include "egluUtil.hpp"
#include "egluUnique.hpp"
#include "egluHeaderWrapper.hpp"

#include "glwDefs.hpp"

namespace deqp
{
namespace egl
{
namespace Image
{

class ManagedSurface
{
public:
										ManagedSurface	(de::MovePtr<eglu::UniqueSurface> surface) : m_surface(surface) {}
	virtual								~ManagedSurface	(void) {}
	EGLSurface							get				(void) const { return **m_surface; }

private:
	de::UniquePtr<eglu::UniqueSurface>	m_surface;
};

de::MovePtr<ManagedSurface> createSurface (EglTestContext& eglTestCtx, EGLConfig config, int width, int height);

class ClientBuffer
{
public:
	virtual					~ClientBuffer	(void) {}
	EGLClientBuffer			get				(void) const { return reinterpret_cast<EGLClientBuffer>(static_cast<deUintptr>(getName())); }

protected:
	virtual glw::GLuint		getName			(void) const = 0;
};

class ImageSource
{
public:
	virtual								~ImageSource		(void) {}
	virtual EGLenum						getSource			(void) const = 0;
	virtual eglu::AttribMap				getCreateAttribs	(void) const = 0;
	virtual std::string					getRequiredExtension(void) const = 0;
	virtual de::MovePtr<ClientBuffer>	createBuffer		(const glw::Functions& gl, tcu::Texture2D* reference = DE_NULL) const = 0;
	EGLImageKHR							createImage			(const eglu::ImageFunctions& imgExt, EGLDisplay dpy, EGLContext ctx, EGLClientBuffer clientBuffer) const;
};

de::MovePtr<ImageSource> createTextureImageSource			(EGLenum source, glw::GLenum format, glw::GLenum type, bool useTexLevel0 = false);
de::MovePtr<ImageSource> createRenderbufferImageSource		(glw::GLenum storage);

} // Image
} // egl
} // deqp


#endif // _TEGLIMAGEUTIL_HPP
