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
 * \brief EGL utilities
 *//*--------------------------------------------------------------------*/

#include "egluUtil.hpp"
#include "egluDefs.hpp"
#include "egluNativeDisplay.hpp"
#include "tcuCommandLine.hpp"
#include "deSTLUtil.hpp"

#include <algorithm>
#include <sstream>

using std::string;
using std::vector;

#if !defined(EGL_EXT_platform_base)
#	define EGL_EXT_platform_base 1
	typedef EGLDisplay (EGLAPIENTRYP PFNEGLGETPLATFORMDISPLAYEXTPROC) (EGLenum platform, void *native_display, const EGLint *attrib_list);
	typedef EGLSurface (EGLAPIENTRYP PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list);
	typedef EGLSurface (EGLAPIENTRYP PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC) (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list);
#endif // EGL_EXT_platform_base

namespace eglu
{

vector<string> getPlatformExtensions (void)
{
	const char* const	extensionStr	= eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	const EGLint		result			= eglGetError();

	if (result == EGL_SUCCESS)
	{
		std::istringstream	stream			(extensionStr);
		string				currentExtension;
		vector<string>		extensions;

		while (std::getline(stream, currentExtension, ' '))
			extensions.push_back(currentExtension);

		return extensions;
	}
	else if (result != EGL_BAD_DISPLAY)
		throw Error(result, "eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS)", DE_NULL, __FILE__, __LINE__);
	else
		return vector<string>();
}

vector<string> getClientExtensions (EGLDisplay display)
{
	const char* const	extensionStr	= eglQueryString(display, EGL_EXTENSIONS);
	const EGLint		result			= eglGetError();

	if (result == EGL_SUCCESS)
	{
		std::istringstream	stream			(extensionStr);
		string				currentExtension;
		vector<string>		extensions;

		while (std::getline(stream, currentExtension, ' '))
			extensions.push_back(currentExtension);

		return extensions;
	}
	else
		throw Error(result, "eglQueryString(display, EGL_EXTENSIONS)", DE_NULL, __FILE__, __LINE__);
}

vector<EGLConfig> getConfigs (EGLDisplay display)
{
	vector<EGLConfig>	configs;
	EGLint				configCount	= 0;
	EGLU_CHECK_CALL(eglGetConfigs(display, DE_NULL, 0, &configCount));

	if (configCount > 0)
	{
		configs.resize(configCount);
		EGLU_CHECK_CALL(eglGetConfigs(display, &(configs[0]), (EGLint)configs.size(), &configCount));
	}

	return configs;
}

vector<EGLConfig> chooseConfig (EGLDisplay display, const AttribMap& attribs)
{
	vector<EGLint> attribList;

	for (AttribMap::const_iterator it = attribs.begin(); it != attribs.end(); ++it)
	{
		attribList.push_back(it->first);
		attribList.push_back(it->second);
	}

	attribList.push_back(EGL_NONE);

	{
		EGLint numConfigs = 0;
		EGLU_CHECK_CALL(eglChooseConfig(display, &attribList.front(), DE_NULL, 0, &numConfigs));

		{
			vector<EGLConfig> configs(numConfigs);

			if (numConfigs > 0)
				EGLU_CHECK_CALL(eglChooseConfig(display, &attribList.front(), &configs.front(), numConfigs, &numConfigs));

			return configs;
		}
	}
}

EGLConfig chooseSingleConfig (EGLDisplay display, const AttribMap& attribs)
{
	vector<EGLConfig> configs (chooseConfig(display, attribs));
	if (configs.empty())
		TCU_THROW(NotSupportedError, "No suitable EGL configuration found");

	return configs.front();
}

EGLint getConfigAttribInt (EGLDisplay display, EGLConfig config, EGLint attrib)
{
	EGLint value = 0;
	EGLU_CHECK_CALL(eglGetConfigAttrib(display, config, attrib, &value));
	return value;
}

EGLint querySurfaceInt (EGLDisplay display, EGLSurface surface, EGLint attrib)
{
	EGLint value = 0;
	EGLU_CHECK_CALL(eglQuerySurface(display, surface, attrib, &value));
	return value;
}

tcu::IVec2 getSurfaceSize (EGLDisplay display, EGLSurface surface)
{
	const EGLint width	= querySurfaceInt(display, surface, EGL_WIDTH);
	const EGLint height	= querySurfaceInt(display, surface, EGL_HEIGHT);
	return tcu::IVec2(width, height);
}

tcu::IVec2 getSurfaceResolution (EGLDisplay display, EGLSurface surface)
{
	const EGLint hRes	= querySurfaceInt(display, surface, EGL_HORIZONTAL_RESOLUTION);
	const EGLint vRes	= querySurfaceInt(display, surface, EGL_VERTICAL_RESOLUTION);

	if (hRes == EGL_UNKNOWN || vRes == EGL_UNKNOWN)
		TCU_THROW(NotSupportedError, "Surface doesn't support pixel density queries");
	return tcu::IVec2(hRes, vRes);
}

//! Get EGLdisplay using eglGetDisplay() or eglGetPlatformDisplayEXT()
EGLDisplay getDisplay (NativeDisplay& nativeDisplay)
{
	const bool	supportsLegacyGetDisplay		= (nativeDisplay.getCapabilities() & NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY) != 0;
	const bool	supportsPlatformGetDisplay		= (nativeDisplay.getCapabilities() & NativeDisplay::CAPABILITY_GET_DISPLAY_PLATFORM) != 0;
	bool		usePlatformExt					= false;
	EGLDisplay	display							= EGL_NO_DISPLAY;

	TCU_CHECK_INTERNAL(supportsLegacyGetDisplay || supportsPlatformGetDisplay);

	if (supportsPlatformGetDisplay)
	{
		const vector<string> platformExts = getPlatformExtensions();
		usePlatformExt = de::contains(platformExts.begin(), platformExts.end(), string("EGL_EXT_platform_base")) &&
						 de::contains(platformExts.begin(), platformExts.end(), string(nativeDisplay.getPlatformExtensionName()));
	}

	if (usePlatformExt)
	{
		const PFNEGLGETPLATFORMDISPLAYEXTPROC	getPlatformDisplay	= (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

		EGLU_CHECK_MSG("eglGetProcAddress()");
		TCU_CHECK(getPlatformDisplay);

		display = getPlatformDisplay(nativeDisplay.getPlatformType(), nativeDisplay.getPlatformNative(), DE_NULL);
		EGLU_CHECK_MSG("eglGetPlatformDisplayEXT()");
		TCU_CHECK(display != EGL_NO_DISPLAY);
	}
	else if (supportsLegacyGetDisplay)
	{
		display = eglGetDisplay(nativeDisplay.getLegacyNative());
		EGLU_CHECK_MSG("eglGetDisplay()");
		TCU_CHECK(display != EGL_NO_DISPLAY);
	}
	else
		throw tcu::InternalError("No supported way to get EGL display", DE_NULL, __FILE__, __LINE__);

	DE_ASSERT(display != EGL_NO_DISPLAY);
	return display;
}

//! Create EGL window surface using eglCreateWindowSurface() or eglCreatePlatformWindowSurfaceEXT()
EGLSurface createWindowSurface (NativeDisplay& nativeDisplay, NativeWindow& window, EGLDisplay display, EGLConfig config, const EGLAttrib* attribList)
{
	const bool	supportsLegacyCreate			= (window.getCapabilities() & NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY) != 0;
	const bool	supportsPlatformCreate			= (window.getCapabilities() & NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM) != 0;
	bool		usePlatformExt					= false;
	EGLSurface	surface							= EGL_NO_SURFACE;

	TCU_CHECK_INTERNAL(supportsLegacyCreate || supportsPlatformCreate);

	if (supportsPlatformCreate)
	{
		const vector<string> platformExts = getPlatformExtensions();
		usePlatformExt = de::contains(platformExts.begin(), platformExts.end(), string("EGL_EXT_platform_base")) &&
						 de::contains(platformExts.begin(), platformExts.end(), string(nativeDisplay.getPlatformExtensionName()));
	}

	// \todo [2014-03-13 pyry] EGL 1.5 core support
	if (usePlatformExt)
	{
		const PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC	createPlatformWindowSurface	= (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
		const vector<EGLint>							legacyAttribs				= toLegacyAttribList(attribList);

		EGLU_CHECK_MSG("eglGetProcAddress()");
		TCU_CHECK(createPlatformWindowSurface);

		surface = createPlatformWindowSurface(display, config, window.getPlatformNative(), &legacyAttribs[0]);
		EGLU_CHECK_MSG("eglCreatePlatformWindowSurfaceEXT()");
		TCU_CHECK(surface != EGL_NO_SURFACE);
	}
	else if (supportsLegacyCreate)
	{
		const vector<EGLint> legacyAttribs = toLegacyAttribList(attribList);
		surface = eglCreateWindowSurface(display, config, window.getLegacyNative(), &legacyAttribs[0]);
		EGLU_CHECK_MSG("eglCreateWindowSurface()");
		TCU_CHECK(surface != EGL_NO_SURFACE);
	}
	else
		throw tcu::InternalError("No supported way to create EGL window surface", DE_NULL, __FILE__, __LINE__);

	DE_ASSERT(surface != EGL_NO_SURFACE);
	return surface;
}

//! Create EGL pixmap surface using eglCreatePixmapSurface() or eglCreatePlatformPixmapSurfaceEXT()
EGLSurface createPixmapSurface (NativeDisplay& nativeDisplay, NativePixmap& pixmap, EGLDisplay display, EGLConfig config, const EGLAttrib* attribList)
{
	const bool	supportsLegacyCreate			= (pixmap.getCapabilities() & NativePixmap::CAPABILITY_CREATE_SURFACE_LEGACY) != 0;
	const bool	supportsPlatformCreate			= (pixmap.getCapabilities() & NativePixmap::CAPABILITY_CREATE_SURFACE_PLATFORM) != 0;
	bool		usePlatformExt					= false;
	EGLSurface	surface							= EGL_NO_SURFACE;

	TCU_CHECK_INTERNAL(supportsLegacyCreate || supportsPlatformCreate);

	if (supportsPlatformCreate)
	{
		const vector<string> platformExts = getPlatformExtensions();
		usePlatformExt = de::contains(platformExts.begin(), platformExts.end(), string("EGL_EXT_platform_base")) &&
						 de::contains(platformExts.begin(), platformExts.end(), string(nativeDisplay.getPlatformExtensionName()));
	}

	if (usePlatformExt)
	{
		const PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC	createPlatformPixmapSurface	= (PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC)eglGetProcAddress("eglCreatePlatformPixmapSurfaceEXT");
		const vector<EGLint>							legacyAttribs				= toLegacyAttribList(attribList);

		EGLU_CHECK_MSG("eglGetProcAddress()");
		TCU_CHECK(createPlatformPixmapSurface);

		surface = createPlatformPixmapSurface(display, config, pixmap.getPlatformNative(), &legacyAttribs[0]);
		EGLU_CHECK_MSG("eglCreatePlatformPixmapSurfaceEXT()");
		TCU_CHECK(surface != EGL_NO_SURFACE);
	}
	else if (supportsLegacyCreate)
	{
		const vector<EGLint> legacyAttribs = toLegacyAttribList(attribList);
		surface = eglCreatePixmapSurface(display, config, pixmap.getLegacyNative(), &legacyAttribs[0]);
		EGLU_CHECK_MSG("eglCreatePixmapSurface()");
		TCU_CHECK(surface != EGL_NO_SURFACE);
	}
	else
		throw tcu::InternalError("No supported way to create EGL pixmap surface", DE_NULL, __FILE__, __LINE__);

	DE_ASSERT(surface != EGL_NO_SURFACE);
	return surface;
}

static WindowParams::Visibility getWindowVisibility (tcu::WindowVisibility visibility)
{
	switch (visibility)
	{
		case tcu::WINDOWVISIBILITY_WINDOWED:	return WindowParams::VISIBILITY_VISIBLE;
		case tcu::WINDOWVISIBILITY_FULLSCREEN:	return WindowParams::VISIBILITY_FULLSCREEN;
		case tcu::WINDOWVISIBILITY_HIDDEN:		return WindowParams::VISIBILITY_HIDDEN;

		default:
			DE_ASSERT(false);
			return WindowParams::VISIBILITY_DONT_CARE;
	}
}

WindowParams::Visibility parseWindowVisibility (const tcu::CommandLine& commandLine)
{
	return getWindowVisibility(commandLine.getVisibility());
}

vector<EGLint> toLegacyAttribList (const EGLAttrib* attribs)
{
	const deUint64	attribMask		= 0xffffffffull;	//!< Max bits that can be used
	vector<EGLint>	legacyAttribs;

	if (attribs)
	{
		for (const EGLAttrib* attrib = attribs; *attrib != EGL_NONE; attrib += 2)
		{
			if ((attrib[0] & ~attribMask) || (attrib[1] & ~attribMask))
				throw tcu::InternalError("Failed to translate EGLAttrib to EGLint", DE_NULL, __FILE__, __LINE__);

			legacyAttribs.push_back((EGLint)attrib[0]);
			legacyAttribs.push_back((EGLint)attrib[1]);
		}
	}

	legacyAttribs.push_back(EGL_NONE);

	return legacyAttribs;
}

} // eglu
