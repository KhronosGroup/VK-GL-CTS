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
 * \brief X11 Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuX11Platform.hpp"

#include "deUniquePtr.hpp"
#include "gluPlatform.hpp"
#include "vkPlatform.hpp"
#include "tcuX11.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deMemory.h"

#if defined (DEQP_SUPPORT_GLX)
#	include "tcuX11GlxPlatform.hpp"
#endif
#if defined (DEQP_SUPPORT_EGL)
#	include "tcuX11EglPlatform.hpp"
#endif

#include <sys/utsname.h>

namespace tcu
{
namespace x11
{

class X11GLPlatform : public glu::Platform
{
public:
	void		registerFactory	(de::MovePtr<glu::ContextFactory> factory)
	{
		m_contextFactoryRegistry.registerFactory(factory.release());
	}
};

class VulkanLibrary : public vk::Library
{
public:
	VulkanLibrary (void)
		: m_library	("libvulkan.so.1")
		, m_driver	(m_library)
	{
	}

	const vk::PlatformInterface& getPlatformInterface (void) const
	{
		return m_driver;
	}

private:
	const tcu::DynamicFunctionLibrary	m_library;
	const vk::PlatformDriver			m_driver;
};

class X11VulkanPlatform : public vk::Platform
{
public:
	vk::Library* createLibrary (void) const
	{
		return new VulkanLibrary();
	}

	void describePlatform (std::ostream& dst) const
	{
		utsname		sysInfo;

		deMemset(&sysInfo, 0, sizeof(sysInfo));

		if (uname(&sysInfo) != 0)
			throw std::runtime_error("uname() failed");

		dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " " << sysInfo.version << "\n";
		dst << "CPU: " << sysInfo.machine << "\n";
	}

	void getMemoryLimits (vk::PlatformMemoryLimits& limits) const
	{
		limits.totalSystemMemory					= 256*1024*1024;
		limits.totalDeviceLocalMemory				= 128*1024*1024;
		limits.deviceMemoryAllocationGranularity	= 64*1024;
		limits.devicePageSize						= 4096;
		limits.devicePageTableEntrySize				= 8;
		limits.devicePageTableHierarchyLevels		= 3;
	}
};

class X11Platform : public tcu::Platform
{
public:
							X11Platform			(void);
	bool					processEvents		(void) { return !m_eventState.getQuitFlag(); }
	const glu::Platform&	getGLPlatform		(void) const { return m_glPlatform; }

#if defined (DEQP_SUPPORT_EGL)
	const eglu::Platform&	getEGLPlatform		(void) const { return m_eglPlatform; }
#endif // DEQP_SUPPORT_EGL

	const vk::Platform&		getVulkanPlatform	(void) const { return m_vkPlatform; }

private:
	EventState				m_eventState;
#if defined (DEQP_SUPPORT_EGL)
	x11::egl::Platform		m_eglPlatform;
#endif // DEQP_SPPORT_EGL
	X11GLPlatform			m_glPlatform;
	X11VulkanPlatform		m_vkPlatform;
};

X11Platform::X11Platform (void)
#if defined (DEQP_SUPPORT_EGL)
	: m_eglPlatform	(m_eventState)
#endif // DEQP_SUPPORT_EGL
{
#if defined (DEQP_SUPPORT_GLX)
	m_glPlatform.registerFactory(glx::createContextFactory(m_eventState));
#endif // DEQP_SUPPORT_GLX
#if defined (DEQP_SUPPORT_EGL)
	m_glPlatform.registerFactory(m_eglPlatform.createContextFactory());
#endif // DEQP_SUPPORT_EGL
}

} // x11
} // tcu

tcu::Platform* createPlatform (void)
{
	// From man:XinitThreads(3):
	//
	//     The XInitThreads function initializes Xlib support for concurrent
	//     threads.  This function must be the first Xlib function
	//     a multi-threaded program calls, and it must complete before any other
	//     Xlib call is made.
	DE_CHECK_RUNTIME_ERR(XInitThreads() != 0);

	return new tcu::x11::X11Platform();
}
