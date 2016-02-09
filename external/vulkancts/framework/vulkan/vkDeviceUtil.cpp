/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Instance and device initialization utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include "tcuCommandLine.hpp"

#include "qpInfo.h"

#include <vector>

namespace vk
{

using std::vector;

Move<VkInstance> createDefaultInstance (const PlatformInterface& vkPlatform)
{
	const struct VkApplicationInfo		appInfo			=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		DE_NULL,
		"deqp",									// pAppName
		qpGetReleaseId(),						// appVersion
		"deqp",									// pEngineName
		qpGetReleaseId(),						// engineVersion
		VK_API_VERSION							// apiVersion
	};
	const struct VkInstanceCreateInfo	instanceInfo	=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		DE_NULL,
		(VkInstanceCreateFlags)0,
		&appInfo,
		0u,										// enabledLayerNameCount
		DE_NULL,								// ppEnabledLayerNames
		0u,										// enabledExtensionNameCount;
		DE_NULL									// ppEnabledExtensionNames
	};

	return createInstance(vkPlatform, &instanceInfo);
}

VkPhysicalDevice chooseDevice (const InstanceInterface& vkInstance, VkInstance instance, const tcu::CommandLine& cmdLine)
{
	const vector<VkPhysicalDevice>	devices	= enumeratePhysicalDevices(vkInstance, instance);

	if (devices.empty())
		TCU_THROW(NotSupportedError, "No Vulkan devices available");

	if (!de::inBounds(cmdLine.getVKDeviceId(), 1, (int)devices.size()+1))
		TCU_THROW(InternalError, "Invalid --deqp-vk-device-id");

	return devices[(size_t)(cmdLine.getVKDeviceId()-1)];
}

} // vk
