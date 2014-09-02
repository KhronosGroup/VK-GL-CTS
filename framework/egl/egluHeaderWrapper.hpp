#ifndef _EGLUHEADERWRAPPER_HPP
#define _EGLUHEADERWRAPPER_HPP
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
 * \brief EGL header file wrapper
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

// egl.h includes windows.h on Windows
#if (DE_OS == DE_OS_WIN32)
#	if !defined(VC_EXTRALEAN)
#		define VC_EXTRALEAN 1
#	endif
#	if !defined(WIN32_LEAN_AND_MEAN)
#		define WIN32_LEAN_AND_MEAN 1
#	endif
#	if !defined(NOMINMAX)
#		define NOMINMAX 1
#	endif
#endif

#include <EGL/egl.h>

#if !defined(EGL_VERSION_1_5)
	typedef deIntptr EGLAttrib;
#endif

#endif // _EGLUHEADERWRAPPER_HPP
