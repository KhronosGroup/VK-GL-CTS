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
#include "vkApiVersion.hpp"

#include "tcuCommandLine.hpp"

#include "qpInfo.h"

namespace vk
{

using std::vector;
using std::string;

Move<VkInstance> createDefaultInstance (const PlatformInterface&	vkPlatform,
										const vector<string>&		enabledLayers,
										const vector<string>&		enabledExtensions)
{
	vector<const char*>		layerNamePtrs		(enabledLayers.size());
	vector<const char*>		extensionNamePtrs	(enabledExtensions.size());
	const deUint32			apiVersion			= pack(ApiVersion(1, 0, 0));

	const struct VkApplicationInfo		appInfo			=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		DE_NULL,
		"deqp",									// pAppName
		qpGetReleaseId(),						// appVersion
		"deqp",									// pEngineName
		qpGetReleaseId(),						// engineVersion
		apiVersion								// apiVersion
	};
	const struct VkInstanceCreateInfo	instanceInfo	=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		DE_NULL,
		(VkInstanceCreateFlags)0,
		&appInfo,
		(deUint32)layerNamePtrs.size(),
		layerNamePtrs.empty() ? DE_NULL : &layerNamePtrs[0],
		(deUint32)extensionNamePtrs.size(),
		extensionNamePtrs.empty() ? DE_NULL : &extensionNamePtrs[0],
	};

	for (size_t ndx = 0; ndx < enabledLayers.size(); ++ndx)
		layerNamePtrs[ndx] = enabledLayers[ndx].c_str();

	for (size_t ndx = 0; ndx < enabledExtensions.size(); ++ndx)
		extensionNamePtrs[ndx] = enabledExtensions[ndx].c_str();

	return createInstance(vkPlatform, &instanceInfo);
}

Move<VkInstance> createDefaultInstance (const PlatformInterface& vkPlatform)
{
	return createDefaultInstance(vkPlatform, vector<string>(), vector<string>());
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
