/*
 * Copyright (c) 2022 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tcuOhosPlatform.hpp"

#include "deUniquePtr.hpp"
#include "ohos_context_i.h"

#include "egluGLContextFactory.hpp"
#include "display/tcuOhosEglDisplayFactory.hpp"
#include "context/tcuOhosEglContextFactory.hpp"

using de::MovePtr;
using de::UniquePtr;

using namespace tcu;
using namespace OHOS_ROSEN;

OhosPlatform::OhosPlatform (void)
{
    printf("OhosPlatform construct!\n");
    m_nativeDisplayFactoryRegistry.registerFactory(new OHOS_ROSEN::egl::OhosDisplayFactory());
    m_contextFactoryRegistry.registerFactory(new OHOS_ROSEN::egl::OhosContextFactory());
}

tcu::Platform* createOhosPlatform (void)
{
    return new tcu::OHOS_ROSEN::OhosPlatform();
}

#include "tcuFunctionLibrary.hpp"
class VulkanLibrary : public vk::Library
{
public:
	VulkanLibrary (void)
		: m_library	("libvulkan.so")
		, m_driver	(m_library)
	{
	}

	const vk::PlatformInterface& getPlatformInterface		(void) const
	{
		return m_driver;
	}

	const tcu::FunctionLibrary&		getFunctionLibrary		(void) const
	{
		return m_library;
	}

private:
	const tcu::DynamicFunctionLibrary	m_library;
	const vk::PlatformDriver			m_driver;
};

class VulkanWindowOhos : public vk::wsi::OhosWindowInterface
{
public:
	VulkanWindowOhos (uint64_t windowId)
        : vk::wsi::OhosWindowInterface	(vk::pt::OhosNativeWindowPtr(OHOS::OhosContextI::GetInstance().GetNativeWindow(windowId)))
		, windowId_(windowId)
	{
	}

	void setVisible(bool visible)
	{
		DE_UNREF(visible);
	}

	void resize(const UVec2& newSize)
	{
		DE_UNREF(newSize);
	}

	~VulkanWindowOhos (void)
	{
		OHOS::OhosContextI::GetInstance().DestoryWindow(windowId_);
	}

private:
	uint64_t windowId_;
};
class VulkanDisplayOhos : public vk::wsi::Display
{
public:
	VulkanDisplayOhos ()
	{
	}

	vk::wsi::Window* createWindow (const Maybe<UVec2>& initialSize) const
	{
		const deUint32		height		= !initialSize ? (deUint32)300 : initialSize->y();
		const deUint32		width		= !initialSize ? (deUint32)400 : initialSize->x();
		printf("%d,%d\n",width,height);
		uint64_t i = OHOS::OhosContextI::GetInstance().CreateWindow(0,0,width,height);
		return new VulkanWindowOhos(i);//cxunmz
	}

private:
};

vk::wsi::Display* OhosPlatform::createWsiDisplay(vk::wsi::Type wsiType) const
{
    if(wsiType == vk::wsi::TYPE_OHOS){
        printf("ok\n");
    }
    return NULL;
}

vk::Library* OhosPlatform::createLibrary(void) const
{
    return new VulkanLibrary();
}

bool OhosPlatform::hasDisplay(vk::wsi::Type wsiType) const
{
    if (wsiType == vk::wsi::TYPE_OHOS)
		return true;

	return false;
}

#include <sys/utsname.h>
#include "deMemory.h"
void OhosPlatform::describePlatform(std::ostream& dst) const
{
	utsname		sysInfo;
	deMemset(&sysInfo, 0, sizeof(sysInfo));

	if (uname(&sysInfo) != 0)
		throw std::runtime_error("uname() failed");

	dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " " << sysInfo.version << "\n";
	dst << "CPU: " << sysInfo.machine << "\n";
}

void OhosPlatform::getMemoryLimits(vk::PlatformMemoryLimits& limits) const
{
	limits.totalSystemMemory					= 256*1024*1024;
	limits.totalDeviceLocalMemory				= 0;//128*1024*1024;
	limits.deviceMemoryAllocationGranularity	= 64*1024;
	limits.devicePageSize						= 4096;
	limits.devicePageTableEntrySize				= 8;
	limits.devicePageTableHierarchyLevels		= 3;
}