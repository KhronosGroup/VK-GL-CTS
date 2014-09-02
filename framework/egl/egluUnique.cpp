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
 * \brief EGL unique resources
 *//*--------------------------------------------------------------------*/

#include "egluUnique.hpp"

#include "tcuEgl.hpp"

namespace eglu
{

UniqueSurface::UniqueSurface (EGLDisplay display, EGLSurface surface)
	: m_display	(display)
	, m_surface	(surface)
{
}

UniqueSurface::~UniqueSurface (void)
{
	if (m_surface != EGL_NO_SURFACE)
		TCU_CHECK_EGL_CALL(eglDestroySurface(m_display, m_surface));
}

UniqueContext::UniqueContext (EGLDisplay display, EGLContext context)
	: m_display	(display)
	, m_context	(context)
{
}

UniqueContext::~UniqueContext (void)
{
	if (m_context != EGL_NO_CONTEXT)
		TCU_CHECK_EGL_CALL(eglDestroyContext(m_display, m_context));
}

ScopedCurrentContext::ScopedCurrentContext (EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
	: m_display (display)
{
	EGLU_CHECK_CALL(eglMakeCurrent(display, draw, read, context));
}

ScopedCurrentContext::~ScopedCurrentContext (void)
{
	EGLU_CHECK_CALL(eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

} // eglu
