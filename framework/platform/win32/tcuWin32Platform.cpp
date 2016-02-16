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

// \todo [2016-01-22 pyry] GetVersionEx() used by getOSInfo() is deprecated.
//						   Find a way to get version info without using deprecated APIs.
#pragma warning(disable : 4996)

#include "tcuWin32Platform.hpp"
#include "tcuWin32Window.hpp"
#include "tcuWGLContextFactory.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuFormatUtil.hpp"

#include "vkWsiPlatform.hpp"
#include "tcuVector.hpp"

#include "deUniquePtr.hpp"
#include "deMemory.h"

#if defined(DEQP_SUPPORT_EGL)
#	include "tcuWin32EGLNativeDisplayFactory.hpp"
#	include "egluGLContextFactory.hpp"
#endif

namespace tcu
{

// \todo [2016-02-23 pyry] Move vulkan platform implementation out

using de::MovePtr;
using de::UniquePtr;

DE_STATIC_ASSERT(sizeof(vk::pt::Win32InstanceHandle)	== sizeof(HINSTANCE));
DE_STATIC_ASSERT(sizeof(vk::pt::Win32WindowHandle)		== sizeof(HWND));

class VulkanWindow : public vk::wsi::Win32WindowInterface
{
public:
	VulkanWindow (MovePtr<Win32Window> window)
		: vk::wsi::Win32WindowInterface	(vk::pt::Win32WindowHandle(window->getHandle()))
		, m_window						(window)
	{
	}

	void resize (const UVec2& newSize)
	{
		m_window->setSize((int)newSize.x(), (int)newSize.y());
	}

private:
	UniquePtr<Win32Window>	m_window;
};

class VulkanDisplay : public vk::wsi::Win32DisplayInterface
{
public:
	VulkanDisplay (HINSTANCE instance)
		: vk::wsi::Win32DisplayInterface	(vk::pt::Win32InstanceHandle(instance))
	{
	}

	vk::wsi::Window* createWindow (const Maybe<UVec2>& initialSize) const
	{
		const HINSTANCE	instance	= (HINSTANCE)m_native.internal;
		const deUint32	width		= !initialSize ? 400 : initialSize->x();
		const deUint32	height		= !initialSize ? 300 : initialSize->y();

		return new VulkanWindow(MovePtr<Win32Window>(new Win32Window(instance, (int)width, (int)height)));
	}
};

class VulkanLibrary : public vk::Library
{
public:
	VulkanLibrary (void)
		: m_library	("vulkan-1.dll")
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

Win32VulkanPlatform::Win32VulkanPlatform (HINSTANCE instance)
	: m_instance(instance)
{
}

Win32VulkanPlatform::~Win32VulkanPlatform (void)
{
}

vk::Library* Win32VulkanPlatform::createLibrary (void) const
{
	return new VulkanLibrary();
}

const char* getProductTypeName (WORD productType)
{
	switch (productType)
	{
		case VER_NT_DOMAIN_CONTROLLER:	return "Windows Server (domain controller)";
		case VER_NT_SERVER:				return "Windows Server";
		case VER_NT_WORKSTATION:		return "Windows NT";
		default:						return DE_NULL;
	}
}

static void getOSInfo (std::ostream& dst)
{
	OSVERSIONINFOEX	osInfo;

	deMemset(&osInfo, 0, sizeof(osInfo));
	osInfo.dwOSVersionInfoSize = (DWORD)sizeof(osInfo);

	GetVersionEx((OSVERSIONINFO*)&osInfo);

	{
		const char* const	productName	= getProductTypeName(osInfo.wProductType);

		if (productName)
			dst << productName;
		else
			dst << "unknown product " << tcu::toHex(osInfo.wProductType);
	}

	dst << " " << osInfo.dwMajorVersion << "." << osInfo.dwMinorVersion
		<< ", service pack " << osInfo.wServicePackMajor << "." << osInfo.wServicePackMinor
		<< ", build " << osInfo.dwBuildNumber;
}

const char* getProcessorArchitectureName (WORD arch)
{
	switch (arch)
	{
		case PROCESSOR_ARCHITECTURE_AMD64:		return "AMD64";
		case PROCESSOR_ARCHITECTURE_ARM:		return "ARM";
		case PROCESSOR_ARCHITECTURE_IA64:		return "IA64";
		case PROCESSOR_ARCHITECTURE_INTEL:		return "INTEL";
		case PROCESSOR_ARCHITECTURE_UNKNOWN:	return "UNKNOWN";
		default:								return DE_NULL;
	}
}

static void getProcessorInfo (std::ostream& dst)
{
	SYSTEM_INFO	sysInfo;

	deMemset(&sysInfo, 0, sizeof(sysInfo));
	GetSystemInfo(&sysInfo);

	dst << "arch ";
	{
		const char* const	archName	= getProcessorArchitectureName(sysInfo.wProcessorArchitecture);

		if (archName)
			dst << archName;
		else
			dst << tcu::toHex(sysInfo.wProcessorArchitecture);
	}

	dst << ", level " << tcu::toHex(sysInfo.wProcessorLevel) << ", revision " << tcu::toHex(sysInfo.wProcessorRevision);
}

void Win32VulkanPlatform::describePlatform (std::ostream& dst) const
{
	dst << "OS: ";
	getOSInfo(dst);
	dst << "\n";

	dst << "CPU: ";
	getProcessorInfo(dst);
	dst << "\n";
}

vk::wsi::Display* Win32VulkanPlatform::createWsiDisplay (vk::wsi::Type wsiType) const
{
	if (wsiType != vk::wsi::TYPE_WIN32)
		TCU_THROW(NotSupportedError, "WSI type not supported");

	return new VulkanDisplay(m_instance);
}

Win32Platform::Win32Platform (void)
	: m_instance		(GetModuleHandle(NULL))
	, m_vulkanPlatform	(m_instance)
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
