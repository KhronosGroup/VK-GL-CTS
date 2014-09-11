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
 * \brief Win32 platform port.
 *//*--------------------------------------------------------------------*/

#include "tcuWin32Platform.hpp"
#include "tcuWGLContextFactory.hpp"

#if defined(DEQP_SUPPORT_EGL)
#	include "tcuWin32EGLNativeDisplayFactory.hpp"
#	include "egluGLContextFactory.hpp"
#endif

namespace tcu
{

Win32Platform::Win32Platform (void)
	: m_instance(GetModuleHandle(NULL))
{
	// Set process priority to lower.
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

	{
		WGLContextFactory* factory = DE_NULL;

		try
		{
			factory = new WGLContextFactory(m_instance);
		}
		catch (const std::exception& e)
		{
			print("Warning: WGL not supported: %s\n", e.what());
		}

		if (factory)
		{
			try
			{
				m_contextFactoryRegistry.registerFactory(factory);
			}
			catch (...)
			{
				delete factory;
				throw;
			}
		}
	}

#if defined(DEQP_SUPPORT_EGL)
	m_nativeDisplayFactoryRegistry.registerFactory(new Win32EGLNativeDisplayFactory(m_instance));
	m_contextFactoryRegistry.registerFactory(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
#endif
}

Win32Platform::~Win32Platform (void)
{
}

bool Win32Platform::processEvents (void)
{
	MSG msg;
	while (PeekMessage(&msg, (HWND)-1, 0, 0, PM_REMOVE))
	{
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
			return false;
	}
	return true;
}

} // tcu

// Create platform
tcu::Platform* createPlatform (void)
{
	return new tcu::Win32Platform();
}
