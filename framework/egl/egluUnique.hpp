#ifndef _EGLUUNIQUE_HPP
#define _EGLUUNIQUE_HPP
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

#include "egluDefs.hpp"
#include "egluHeaderWrapper.hpp"

namespace eglu
{

class UniqueSurface
{
public:
					UniqueSurface	(EGLDisplay display, EGLSurface surface);
					~UniqueSurface	(void);

	EGLSurface		operator*		(void) { return m_surface; }
	operator		bool			(void) const { return m_surface != EGL_NO_SURFACE; }

private:
	EGLDisplay		m_display;
	EGLSurface		m_surface;

	// Disabled
	UniqueSurface&	operator=		(const UniqueSurface&);
					UniqueSurface	(const UniqueSurface&);
};

class UniqueContext
{
public:
					UniqueContext	(EGLDisplay display, EGLContext context);
					~UniqueContext	(void);

	EGLContext		operator*		(void) { return m_context; }
	operator		bool			(void) const { return m_context != EGL_NO_CONTEXT; }

private:
	EGLDisplay		m_display;
	EGLContext		m_context;

	// Disabled
	UniqueContext	operator=		(const UniqueContext&);
					UniqueContext	(const UniqueContext&);
};

class ScopedCurrentContext
{
public:
	ScopedCurrentContext	(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context);
	~ScopedCurrentContext	(void);

private:
	EGLDisplay		m_display;
};

} // eglu

#endif // _EGLUUNIQUE_HPP
