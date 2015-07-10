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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
		VK_STRUCTURE_TYPE_APPLICATION_INFO,		//	VkStructureType	sType;
		DE_NULL,								//	const void*		pNext;
		"deqp",									//	const char*		pAppName;
		qpGetReleaseId(),						//	deUint32		appVersion;
		"deqp",									//	const char*		pEngineName;
		qpGetReleaseId(),						//	deUint32		engineVersion;
		VK_API_VERSION							//	deUint32		apiVersion;
	};
	const struct VkInstanceCreateInfo	instanceInfo	=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	//	VkStructureType				sType;
		DE_NULL,								//	const void*					pNext;
		&appInfo,								//	const VkApplicationInfo*	pAppInfo;
		DE_NULL,								//	const VkAllocCallbacks*		pAllocCb;
		0u,										//	deUint32					layerCount;
		DE_NULL,								//	const char*const*			ppEnabledLayerNames;
		0u,										//	deUint32					extensionCount;
		DE_NULL									//	const char*const*			ppEnabledExtensionNames;
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
