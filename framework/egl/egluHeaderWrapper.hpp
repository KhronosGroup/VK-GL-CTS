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
#include <EGL/eglext.h>

#if !defined(EGL_VERSION_1_5)
	typedef deIntptr EGLAttrib;
#endif

#if !defined(EGL_KHR_create_context)
	#define EGL_KHR_create_context 1
	#define EGL_CONTEXT_MAJOR_VERSION_KHR						0x3098
	#define EGL_CONTEXT_MINOR_VERSION_KHR						0x30FB
	#define EGL_CONTEXT_FLAGS_KHR								0x30FC
	#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR					0x30FD
	#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR	0x31BD
	#define EGL_NO_RESET_NOTIFICATION_KHR						0x31BE
	#define EGL_LOSE_CONTEXT_ON_RESET_KHR						0x31BF
	#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR					0x00000001
	#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR		0x00000002
	#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR			0x00000004
	#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR				0x00000001
	#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR	0x00000002
	#define EGL_OPENGL_ES3_BIT_KHR								0x00000040
#endif // EGL_KHR_create_context

#endif // _EGLUHEADERWRAPPER_HPP
