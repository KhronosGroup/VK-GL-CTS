#ifndef _EGLUUTIL_HPP
#define _EGLUUTIL_HPP
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

#include "tcuDefs.hpp"
#include "egluHeaderWrapper.hpp"
#include "egluNativeWindow.hpp"

#include <vector>
#include <map>
#include <string>

namespace tcu
{
class CommandLine;
}

namespace eglu
{

class NativeDisplay;
class NativePixmap;

typedef std::map<EGLint, EGLint> AttribMap;

std::vector<std::string>	getPlatformExtensions		(void);
std::vector<std::string>	getClientExtensions			(EGLDisplay display);
std::vector<EGLConfig>		getConfigs					(EGLDisplay display);
std::vector<EGLConfig>		chooseConfig				(EGLDisplay display, const AttribMap& attribs);
EGLConfig					chooseSingleConfig			(EGLDisplay display, const AttribMap& attribs);
EGLint						getConfigAttribInt			(EGLDisplay display, EGLConfig config, EGLint attrib);
EGLint						querySurfaceInt				(EGLDisplay display, EGLSurface surface, EGLint attrib);
tcu::IVec2					getSurfaceSize				(EGLDisplay display, EGLSurface surface);
tcu::IVec2					getSurfaceResolution		(EGLDisplay display, EGLSurface surface);
EGLDisplay					getDisplay					(NativeDisplay& nativeDisplay);
EGLSurface					createWindowSurface			(NativeDisplay& nativeDisplay, NativeWindow& window, EGLDisplay display, EGLConfig config, const EGLAttrib* attribList);
EGLSurface					createPixmapSurface			(NativeDisplay& nativeDisplay, NativePixmap& pixmap, EGLDisplay display, EGLConfig config, const EGLAttrib* attribList);

WindowParams::Visibility	parseWindowVisibility		(const tcu::CommandLine& commandLine);

std::vector<EGLint>			toLegacyAttribList			(const EGLAttrib* attribs);

} // eglu

#endif // _EGLUUTIL_HPP
