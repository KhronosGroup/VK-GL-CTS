/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Utilities
 * -----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Instance and device initialization utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDeviceUtil.hpp"

#include "tcuCommandLine.hpp"

#include "qpInfo.h"

#include <vector>

namespace vk
{

using std::vector;

Move<VkInstanceT> createDefaultInstance (const PlatformInterface& vkPlatform)
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
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,								//	const void*					pNext;
		&appInfo,								//	const VkApplicationInfo*	pAppInfo;
		DE_NULL,								//	const VkAllocCallbacks*		pAllocCb;
		0u,										//	deUint32					extensionCount;
		DE_NULL									//	const char*const*			ppEnabledExtensionNames;
	};

	return createInstance(vkPlatform, &instanceInfo);
}

VkPhysicalDevice chooseDevice (const PlatformInterface& vkPlatform, VkInstance instance, const tcu::CommandLine& cmdLine)
{
	vector<VkPhysicalDevice>	devices;
	deUint32					numDevices	= 0;

	VK_CHECK(vkPlatform.enumeratePhysicalDevices(instance, &numDevices, DE_NULL));

	if (numDevices > 0)
	{
		devices.resize(numDevices);
		VK_CHECK(vkPlatform.enumeratePhysicalDevices(instance, &numDevices, &devices[0]));

		if (numDevices != devices.size())
			TCU_FAIL("Number of devices changed between queries");

		if (!de::inBounds(cmdLine.getVKDeviceId(), 1, (int)devices.size()+1))
			TCU_THROW(InternalError, "Invalid --deqp-vk-device-id");

		return devices[(size_t)(cmdLine.getVKDeviceId()-1)];
	}
	else
		TCU_THROW(NotSupportedError, "No Vulkan devices available");
}

} // vk
