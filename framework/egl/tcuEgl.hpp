#ifndef _TCUEGL_HPP
#define _TCUEGL_HPP
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

#include "egluDefs.hpp"
#include "tcuPixelFormat.hpp"
#include "egluHeaderWrapper.hpp"

#include <vector>
#include <string>

#define TCU_CHECK_EGL()				EGLU_CHECK()
#define TCU_CHECK_EGL_MSG(MSG)		EGLU_CHECK_MSG(MSG)
#define TCU_CHECK_EGL_CALL(CALL)	EGLU_CHECK_CALL(CALL)

namespace eglu
{
class ConfigInfo;
}

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief EGL utilities
 *//*--------------------------------------------------------------------*/
namespace egl
{

class Surface;

class Display
{
public:
							Display				(EGLDisplay display, EGLint majorVersion, EGLint minorVersion);
							Display				(EGLNativeDisplayType nativeDisplay);
	virtual					~Display			(void);

	void					getConfigs			(std::vector<EGLConfig>& configs) const;
	void					chooseConfig		(const EGLint* attributeList, std::vector<EGLConfig>& configs) const;

	EGLint					getConfigAttrib		(EGLConfig config, EGLint attribute) const;
	void					describeConfig		(EGLConfig config, tcu::PixelFormat& pixelFormat) const;
	void					describeConfig		(EGLConfig config, eglu::ConfigInfo& info) const;

	EGLDisplay				getEGLDisplay		(void) const { return m_display; }
	EGLint					getEGLMajorVersion	(void) const { return m_version[0]; }
	EGLint					getEGLMinorVersion	(void) const { return m_version[1]; }

	eglu::Version			getVersion			(void) const { return eglu::Version(m_version[0], m_version[1]); }

	void					getString			(EGLint name, std::string& dst) const;
	void					getExtensions		(std::vector<std::string>& dst) const;

	std::string				getString			(EGLint name) const { std::string str; getString(name, str); return str; }

protected:
							Display				(const Display&); // not allowed
	Display&				operator=			(const Display&); // not allowed

	EGLDisplay				m_display;
	EGLint					m_version[2];
};

class Surface
{
public:
	virtual					~Surface			(void) {}

	EGLSurface				getEGLSurface		(void) const { return m_surface; }
	Display&				getDisplay			(void) const { return m_display; }

	EGLint					getAttribute		(EGLint attribute) const;
	void					setAttribute		(EGLint attribute, EGLint value);

	int						getWidth			(void) const;
	int						getHeight			(void) const;
	void					getSize				(int& width, int& height) const;

protected:
							Surface				(Display& display) : m_display(display), m_surface(EGL_NO_SURFACE) {}

							Surface				(const Surface&); // not allowed
	Surface&				operator=			(const Surface&); // not allowed

	Display&				m_display;
	EGLSurface				m_surface;
};

class WindowSurface : public Surface
{
public:
							WindowSurface		(Display& display, EGLSurface windowSurface);
							WindowSurface		(Display& display, EGLConfig config, EGLNativeWindowType nativeWindow, const EGLint* attribList);
	virtual					~WindowSurface		(void);

	void					swapBuffers			(void);
};

class PixmapSurface : public Surface
{
public:
							PixmapSurface		(Display& display, EGLSurface surface);
							PixmapSurface		(Display& display, EGLConfig config, EGLNativePixmapType nativePixmap, const EGLint* attribList);
	virtual					~PixmapSurface		(void);
};

class PbufferSurface : public Surface
{
public:
							PbufferSurface		(Display& display, EGLConfig config, const EGLint* attribList);
	virtual					~PbufferSurface		(void);
};

class Context
{
public:
							Context				(const Display& display, EGLConfig config, const EGLint* attribList, EGLenum api);
							~Context			(void);

	EGLenum					getAPI				(void) const { return m_api;		}
	EGLContext				getEGLContext		(void) const { return m_context;	}
	EGLConfig				getConfig			(void) const { return m_config;		}

	void					makeCurrent			(const Surface& draw, const Surface& read);

protected:
							Context				(const Context&); // not allowed
	Context&				operator=			(const Context&); // not allowed

	const Display&			m_display;
	EGLConfig				m_config;
	EGLenum					m_api;
	EGLContext				m_context;
};

} // egl
} // tcu

#endif // _TCUEGL_HPP
