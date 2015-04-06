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
 * \brief Null GL platform.
 *//*--------------------------------------------------------------------*/

#include "tcuNullPlatform.hpp"
#include "tcuNullRenderContext.hpp"
#include "egluNativeDisplay.hpp"
#include "eglwLibrary.hpp"

namespace tcu
{
namespace null
{

class NullGLContextFactory : public glu::ContextFactory
{
public:
	NullGLContextFactory (void)
		: glu::ContextFactory("null", "Null Render Context")
	{
	}

	glu::RenderContext* createContext (const glu::RenderConfig& config, const tcu::CommandLine&) const
	{
		return new RenderContext(config);
	}
};

class NullEGLDisplay : public eglu::NativeDisplay
{
public:
	NullEGLDisplay (void)
		: eglu::NativeDisplay(CAPABILITY_GET_DISPLAY_LEGACY)
	{
		// \note All functions in library are null
	}

	const eglw::Library& getLibrary (void) const
	{
		return m_library;
	}

private:
	eglw::FuncPtrLibrary	m_library;
};

class NullEGLDisplayFactory : public eglu::NativeDisplayFactory
{
public:
	NullEGLDisplayFactory (void)
		: eglu::NativeDisplayFactory("null", "Null EGL Display", eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY)
	{
	}

	eglu::NativeDisplay* createDisplay (const eglw::EGLAttrib*) const
	{
		return new NullEGLDisplay();
	}
};

Platform::Platform (void)
{
	m_contextFactoryRegistry.registerFactory(new NullGLContextFactory());
	m_nativeDisplayFactoryRegistry.registerFactory(new NullEGLDisplayFactory());
}

Platform::~Platform (void)
{
}

} // null
} // tcu

tcu::Platform* createPlatform (void)
{
	return new tcu::null::Platform();
}
