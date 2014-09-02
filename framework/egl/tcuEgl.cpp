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
 * \brief Legacy EGL utilities
 *//*--------------------------------------------------------------------*/

#include "tcuEgl.hpp"
#include "egluStrUtil.hpp"
#include "egluConfigInfo.hpp"
#include "deString.h"

#include <sstream>

using std::vector;
using std::string;

namespace tcu
{
namespace egl
{

Display::Display (EGLDisplay display, EGLint majorVersion, EGLint minorVersion)
	: m_display(display)
{
	m_version[0] = majorVersion;
	m_version[1] = minorVersion;
}

Display::Display (EGLNativeDisplayType nativeDisplay)
	: m_display			(EGL_NO_DISPLAY)
{
	m_display = eglGetDisplay(nativeDisplay);
	TCU_CHECK_EGL();
	TCU_CHECK(m_display != EGL_NO_DISPLAY);

	TCU_CHECK_EGL_CALL(eglInitialize(m_display, &m_version[0], &m_version[1]));
}

Display::~Display ()
{
	if (m_display)
		eglTerminate(m_display);
}

void Display::getConfigs (std::vector<EGLConfig>& configs) const
{
	EGLint numConfigs = 0;
	TCU_CHECK_EGL_CALL(eglGetConfigs(m_display, DE_NULL, 0, &numConfigs));
	configs.resize(numConfigs);
	if (numConfigs > 0)
		TCU_CHECK_EGL_CALL(eglGetConfigs(m_display, &configs[0], (EGLint)configs.size(), &numConfigs));
}

void Display::chooseConfig (const EGLint* attribList, std::vector<EGLConfig>& configs) const
{
	EGLint numConfigs = 0;
	TCU_CHECK_EGL_CALL(eglChooseConfig(m_display, attribList, DE_NULL, 0, &numConfigs));
	configs.resize(numConfigs);
	if (numConfigs > 0)
		TCU_CHECK_EGL_CALL(eglChooseConfig(m_display, attribList, &configs[0], (EGLint)configs.size(), &numConfigs));
}

EGLint Display::getConfigAttrib (EGLConfig config, EGLint attribute) const
{
	EGLint value = 0;
	TCU_CHECK_EGL_CALL(eglGetConfigAttrib(m_display, config, attribute, &value));
	return value;
}

void Display::describeConfig (EGLConfig config, tcu::PixelFormat& pf) const
{
	eglGetConfigAttrib(m_display, config, EGL_RED_SIZE,		&pf.redBits);
	eglGetConfigAttrib(m_display, config, EGL_GREEN_SIZE,	&pf.greenBits);
	eglGetConfigAttrib(m_display, config, EGL_BLUE_SIZE,	&pf.blueBits);
	eglGetConfigAttrib(m_display, config, EGL_ALPHA_SIZE,	&pf.alphaBits);
	TCU_CHECK_EGL();
}

void Display::describeConfig (EGLConfig config, eglu::ConfigInfo& info) const
{
	eglu::queryConfigInfo(m_display, config, &info);
}

static void split (vector<string>& dst, const string& src)
{
	size_t start = 0;
	size_t end	 = string::npos;

	while ((end = src.find(' ', start)) != string::npos)
	{
		dst.push_back(src.substr(start, end-start));
		start = end+1;
	}

	if (start < end)
		dst.push_back(src.substr(start, end-start));
}

void Display::getExtensions (vector<string>& dst) const
{
	const char* extStr = eglQueryString(m_display, EGL_EXTENSIONS);
	TCU_CHECK_EGL_MSG("eglQueryString(EGL_EXTENSIONS");
	TCU_CHECK(extStr);
	split(dst, extStr);
}

void Display::getString (EGLint name, std::string& dst) const
{
	const char* retStr = eglQueryString(m_display, name);
	TCU_CHECK_EGL_MSG("eglQueryString()");
	TCU_CHECK(retStr);
	dst = retStr;
}

EGLint Surface::getAttribute (EGLint attribute) const
{
	EGLint value;
	TCU_CHECK_EGL_CALL(eglQuerySurface(m_display.getEGLDisplay(), m_surface, attribute, &value));
	return value;
}

void Surface::setAttribute (EGLint attribute, EGLint value)
{
	TCU_CHECK_EGL_CALL(eglSurfaceAttrib(m_display.getEGLDisplay(), m_surface, attribute, value));
}

int Surface::getWidth (void) const
{
	return getAttribute(EGL_WIDTH);
}

int Surface::getHeight (void) const
{
	return getAttribute(EGL_HEIGHT);
}

void Surface::getSize (int& x, int& y) const
{
	x = getWidth();
	y = getHeight();
}

WindowSurface::WindowSurface (Display& display, EGLSurface windowSurface)
	: Surface	(display)
{
	m_surface = windowSurface;
}

WindowSurface::WindowSurface (Display& display, EGLConfig config, EGLNativeWindowType nativeWindow, const EGLint* attribList)
	: Surface			(display)
{
	m_surface = eglCreateWindowSurface(display.getEGLDisplay(), config, nativeWindow, attribList);
	TCU_CHECK_EGL();
	TCU_CHECK(m_surface != EGL_NO_SURFACE);
}

WindowSurface::~WindowSurface (void)
{
	eglDestroySurface(m_display.getEGLDisplay(), m_surface);
	m_surface = EGL_NO_SURFACE;
}

void WindowSurface::swapBuffers (void)
{
	TCU_CHECK_EGL_CALL(eglSwapBuffers(m_display.getEGLDisplay(), m_surface));
}

PixmapSurface::PixmapSurface (Display& display, EGLSurface surface)
	: Surface	(display)
{
	m_surface = surface;
}

PixmapSurface::PixmapSurface (Display& display, EGLConfig config, EGLNativePixmapType nativePixmap, const EGLint* attribList)
	: Surface			(display)
{
	m_surface = eglCreatePixmapSurface(m_display.getEGLDisplay(), config, nativePixmap, attribList);
	TCU_CHECK_EGL();
	TCU_CHECK(m_surface != EGL_NO_SURFACE);
}

PixmapSurface::~PixmapSurface (void)
{
	eglDestroySurface(m_display.getEGLDisplay(), m_surface);
	m_surface = EGL_NO_SURFACE;
}

#if 0 // \todo [mika] Fix borken
void PixmapSurface::copyBuffers (void)
{
	TCU_CHECK_EGL_CALL(eglCopyBuffers(m_display.getEGLDisplay(), m_surface, m_nativePixmap));
}
#endif

PbufferSurface::PbufferSurface (Display& display, EGLConfig config, const EGLint* attribList)
	: Surface(display)
{
	m_surface = eglCreatePbufferSurface(m_display.getEGLDisplay(), config, attribList);
	TCU_CHECK_EGL();
	TCU_CHECK(m_surface != EGL_NO_SURFACE);
}

PbufferSurface::~PbufferSurface (void)
{
	eglDestroySurface(m_display.getEGLDisplay(), m_surface);
	m_surface = EGL_NO_SURFACE;
}

Context::Context (const Display& display, EGLConfig config, const EGLint* attribList, EGLenum api)
	: m_display(display)
	, m_config(config)
	, m_api(api)
	, m_context(EGL_NO_CONTEXT)
{
	TCU_CHECK_EGL_CALL(eglBindAPI(m_api));
	m_context = eglCreateContext(m_display.getEGLDisplay(), config, EGL_NO_CONTEXT, attribList);
	TCU_CHECK_EGL();
	TCU_CHECK(m_context);
}

Context::~Context (void)
{
	if (m_context)
	{
		/* If this is current surface, remove binding. */
		EGLContext curContext = EGL_NO_CONTEXT;
		eglBindAPI(m_api);
		curContext = eglGetCurrentContext();
		if (curContext == m_context)
			eglMakeCurrent(m_display.getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		eglDestroyContext(m_display.getEGLDisplay(), m_context);
	}
}

void Context::makeCurrent (const Surface& draw, const Surface& read)
{
	TCU_CHECK_EGL_CALL(eglMakeCurrent(m_display.getEGLDisplay(), draw.getEGLSurface(), read.getEGLSurface(), m_context));
}

} // egl
} // tcu
