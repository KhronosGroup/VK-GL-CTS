/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected Memory
 *//*--------------------------------------------------------------------*/
#include "vktProtectedMemContext.hpp"

namespace vkt
{
namespace ProtectedMem
{

ProtectedContext::ProtectedContext	(Context&						ctx,
									 const std::vector<std::string>	instanceExtensions,
									 const std::vector<std::string>	deviceExtensions)
	: m_context				(ctx)
	, m_interface			(m_context.getPlatformInterface())
	, m_instance			(makeProtectedMemInstance(m_context, instanceExtensions))
	, m_vki					(m_instance.getDriver())
	, m_phyDevice			(vk::chooseDevice(m_vki, m_instance, m_context.getTestContext().getCommandLine()))
	, m_queueFamilyIndex	(chooseProtectedMemQueueFamilyIndex(m_vki, m_phyDevice))
	, m_device				(makeProtectedMemDevice(m_interface, m_instance, m_vki, m_phyDevice, m_queueFamilyIndex, ctx.getUsedApiVersion(), deviceExtensions, m_context.getTestContext().getCommandLine().isValidationEnabled()))
	, m_deviceDriver		(m_context.getPlatformInterface(), m_instance, *m_device)
	, m_allocator			(createAllocator())
	, m_queue				(getProtectedQueue(m_deviceDriver, *m_device, m_queueFamilyIndex, 0))
{
}

ProtectedContext::ProtectedContext	(Context&						ctx,
									 vk::wsi::Type					wsiType,
									 vk::wsi::Display&				display,
									 vk::wsi::Window&				window,
									 const std::vector<std::string>	instanceExtensions,
									 const std::vector<std::string>	deviceExtensions)
	: m_context				(ctx)
	, m_interface			(m_context.getPlatformInterface())
	, m_instance			(makeProtectedMemInstance(m_context, instanceExtensions))
	, m_vki					(m_instance.getDriver())
	, m_phyDevice			(vk::chooseDevice(m_vki, m_instance, m_context.getTestContext().getCommandLine()))
	, m_surface				(vk::wsi::createSurface(m_vki, m_instance, wsiType, display, window))
	, m_queueFamilyIndex	(chooseProtectedMemQueueFamilyIndex(m_vki, m_phyDevice, *m_surface))
	, m_device				(makeProtectedMemDevice(m_interface, m_instance, m_vki, m_phyDevice, m_queueFamilyIndex, ctx.getUsedApiVersion(), deviceExtensions, m_context.getTestContext().getCommandLine().isValidationEnabled()))
	, m_deviceDriver		(m_interface, m_instance, *m_device)
	, m_allocator(createAllocator())
	, m_queue				(getProtectedQueue(m_deviceDriver, *m_device, m_queueFamilyIndex, 0))
{
}

} // ProtectedMem
} // vkt
