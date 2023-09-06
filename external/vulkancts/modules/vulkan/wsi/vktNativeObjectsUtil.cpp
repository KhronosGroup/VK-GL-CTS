/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief WSI Native Objects utility class.
 *//*--------------------------------------------------------------------*/
#include "vktNativeObjectsUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkWsiUtil.hpp"

#include "tcuPlatform.hpp"

#include "deDefs.hpp"

namespace vkt
{
namespace wsi
{

de::MovePtr<vk::wsi::Display> NativeObjects::createDisplay	(const vk::Platform&				platform,
															 const NativeObjects::Extensions&	supportedExtensions,
															 vk::wsi::Type						wsiType)
{
	try
	{
		return de::MovePtr<vk::wsi::Display>(platform.createWsiDisplay(wsiType));
	}
	catch (const tcu::NotSupportedError& e)
	{
		if (vk::isExtensionStructSupported(supportedExtensions, vk::RequiredExtension(vk::wsi::getExtensionName(wsiType))) &&
			platform.hasDisplay(wsiType))
		{
			// If VK_KHR_{platform}_surface was supported, vk::Platform implementation
			// must support creating native display & window for that WSI type.
			throw tcu::TestError(e.getMessage());
		}
		else
			throw;
	}
}

de::MovePtr<vk::wsi::Window> NativeObjects::createWindow (const vk::wsi::Display& display, const tcu::Maybe<tcu::UVec2>& initialSize)
{
	try
	{
		return de::MovePtr<vk::wsi::Window>(display.createWindow(initialSize));
	}
	catch (const tcu::NotSupportedError& e)
	{
		// See createDisplay - assuming that wsi::Display was supported platform port
		// should also support creating a window.
		throw tcu::TestError(e.getMessage());
	}
}

NativeObjects::NativeObjects (Context&						context,
							  const Extensions&				supportedExtensions,
							  vk::wsi::Type					wsiType,
							  size_t						windowCount,
							  const tcu::Maybe<tcu::UVec2>&	initialWindowSize)
	: display (createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
{
	DE_ASSERT(windowCount > 0u);
	for (size_t i = 0; i < windowCount; ++i)
		windows.emplace_back(createWindow(*display, initialWindowSize));
}

NativeObjects::NativeObjects (NativeObjects&& other)
	: display	(other.display.move())
	, windows	()
{
	windows.swap(other.windows);
}

vk::wsi::Display& NativeObjects::getDisplay	() const
{
	return *display;
}

vk::wsi::Window& NativeObjects::getWindow (size_t index) const
{
	DE_ASSERT(index < windows.size());
	return *windows[index];
}

} // wsi
} // vkt
