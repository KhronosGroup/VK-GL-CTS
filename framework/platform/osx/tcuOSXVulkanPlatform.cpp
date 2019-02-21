/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief OSX Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuOSXVulkanPlatform.hpp"
#include "tcuOSXPlatform.hpp"
#include "tcuOSXMetalView.hpp"
#include "vkWsiPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuFunctionLibrary.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"

#include <sys/utsname.h>

using de::MovePtr;
using de::UniquePtr;

namespace tcu
{
namespace osx
{

class VulkanWindow : public vk::wsi::MacOSWindowInterface
{
public:
	VulkanWindow (MovePtr<osx::MetalView> view)
	: vk::wsi::MacOSWindowInterface(view->getView())
	, m_view(view)
	{
	}

	void resize (const UVec2& newSize) {
		m_view->setSize(newSize.x(), newSize.y());
	}

private:
	UniquePtr<osx::MetalView> m_view;
};

class VulkanDisplay : public vk::wsi::Display
{
public:
	VulkanDisplay ()
	{
	}

	vk::wsi::Window* createWindow (const Maybe<UVec2>& initialSize) const
	{
		const deUint32 width = !initialSize ? 400 : initialSize->x();
		const deUint32 height = !initialSize ? 300 : initialSize->y();
		return new VulkanWindow(MovePtr<osx::MetalView>(new osx::MetalView(width, height)));
	}
};

class VulkanLibrary : public vk::Library
{
public:
	VulkanLibrary (void)
		: m_library	("libvulkan.dylib")
		, m_driver	(m_library)
	{
	}

	const vk::PlatformInterface&	getPlatformInterface	(void) const
	{
		return m_driver;
	}

	const tcu::FunctionLibrary&		getFunctionLibrary		(void) const
	{
		return m_library;
	}

private:
	const DynamicFunctionLibrary	m_library;
	const vk::PlatformDriver		m_driver;
};

VulkanPlatform::VulkanPlatform ()
{
}

vk::wsi::Display* VulkanPlatform::createWsiDisplay (vk::wsi::Type wsiType) const
{
	if (wsiType != vk::wsi::TYPE_MACOS)
		TCU_THROW(NotSupportedError, "WSI type not supported");

	return new VulkanDisplay();
}

bool VulkanPlatform::hasDisplay (vk::wsi::Type wsiType)  const
{
	if (wsiType != vk::wsi::TYPE_MACOS)
		return false;

	return true;
}
vk::Library* VulkanPlatform::createLibrary (void) const
{
	return new VulkanLibrary();
}

void VulkanPlatform::describePlatform (std::ostream& dst) const
{
	utsname		sysInfo;
	deMemset(&sysInfo, 0, sizeof(sysInfo));

	if (uname(&sysInfo) != 0)
		throw std::runtime_error("uname() failed");

	dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " " << sysInfo.version << "\n";
	dst << "CPU: " << sysInfo.machine << "\n";
}

void VulkanPlatform::getMemoryLimits (vk::PlatformMemoryLimits& limits) const
{
	limits.totalSystemMemory					= 256*1024*1024;
	limits.totalDeviceLocalMemory				= 128*1024*1024;
	limits.deviceMemoryAllocationGranularity	= 64*1024;
	limits.devicePageSize						= 4096;
	limits.devicePageTableEntrySize				= 8;
	limits.devicePageTableHierarchyLevels		= 3;
}

} // osx
} // tcu

