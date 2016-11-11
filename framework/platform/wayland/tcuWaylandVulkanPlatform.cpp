/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Wayland Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuWaylandVulkanPlatform.hpp"
#include "tcuWaylandPlatform.hpp"
#include "vkWsiPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuWayland.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"

#include <sys/utsname.h>

using de::MovePtr;
using de::UniquePtr;

namespace tcu
{
namespace wayland
{

class VulkanWindowWayland : public vk::wsi::WaylandWindowInterface
{
public:
	VulkanWindowWayland (MovePtr<wayland::Window> window)
		: vk::wsi::WaylandWindowInterface	(vk::pt::WaylandSurfacePtr(window->getSurface()))
		, m_window							(window)
	{
	}

	void resize (const UVec2& newSize)
	{
		m_window->setDimensions((int)newSize.x(), (int)newSize.y());
	}

private:
	UniquePtr<wayland::Window>	m_window;
};

class VulkanDisplayWayland : public vk::wsi::WaylandDisplayInterface
{
public:
	VulkanDisplayWayland (MovePtr<wayland::Display> display)
		: vk::wsi::WaylandDisplayInterface	(vk::pt::WaylandDisplayPtr(display->getDisplay()))
		, m_display		(display)
	{
	}

	vk::wsi::Window* createWindow (const Maybe<UVec2>& initialSize) const
	{
		const deUint32	height		= !initialSize ? (deUint32)DEFAULT_WINDOW_HEIGHT : initialSize->y();
		const deUint32	width		= !initialSize ? (deUint32)DEFAULT_WINDOW_WIDTH : initialSize->x();
		return new VulkanWindowWayland(MovePtr<wayland::Window>(new wayland::Window(*m_display, (int)width, (int)height)));
	}

private:
	MovePtr<wayland::Display> m_display;
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
	const DynamicFunctionLibrary	m_library;
	const vk::PlatformDriver		m_driver;
};

WaylandVulkanPlatform::WaylandVulkanPlatform (EventState& eventState)
	: m_eventState(eventState)
{
}

vk::wsi::Display* WaylandVulkanPlatform::createWsiDisplay (vk::wsi::Type wsiType) const
{
	switch(wsiType)
	{
	case vk::wsi::TYPE_WAYLAND:
		return new VulkanDisplayWayland(MovePtr<Display>(new Display(m_eventState, DE_NULL)));
		break;
	default:
		TCU_THROW(NotSupportedError, "WSI type not supported");

	};
}

vk::Library* WaylandVulkanPlatform::createLibrary (void) const
{
	return new VulkanLibrary();
}

void WaylandVulkanPlatform::describePlatform (std::ostream& dst) const
{
	utsname		sysInfo;
	deMemset(&sysInfo, 0, sizeof(sysInfo));

	if (uname(&sysInfo) != 0)
		throw std::runtime_error("uname() failed");

	dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " " << sysInfo.version << "\n";
	dst << "CPU: " << sysInfo.machine << "\n";
}

void WaylandVulkanPlatform::getMemoryLimits (vk::PlatformMemoryLimits& limits) const
{
	limits.totalSystemMemory					= 256*1024*1024;
	limits.totalDeviceLocalMemory				= 128*1024*1024;
	limits.deviceMemoryAllocationGranularity	= 64*1024;
	limits.devicePageSize						= 4096;
	limits.devicePageTableEntrySize				= 8;
	limits.devicePageTableHierarchyLevels		= 3;
}

} // wayland
} // tcu

