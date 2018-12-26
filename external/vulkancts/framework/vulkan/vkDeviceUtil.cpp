/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkApiVersion.hpp"

#include "tcuCommandLine.hpp"

#include "qpInfo.h"

namespace vk
{

using std::vector;
using std::string;

Move<VkInstance> createDefaultInstance (const PlatformInterface&		vkPlatform,
										deUint32						apiVersion,
										const vector<string>&			enabledLayers,
										const vector<string>&			enabledExtensions,
										const VkAllocationCallbacks*	pAllocator)
{
	vector<const char*>		layerNamePtrs		(enabledLayers.size());
	vector<const char*>		extensionNamePtrs	(enabledExtensions.size());

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

	return createInstance(vkPlatform, &instanceInfo, pAllocator);
}

Move<VkInstance> createDefaultInstance (const PlatformInterface& vkPlatform, deUint32 apiVersion)
{
	return createDefaultInstance(vkPlatform, apiVersion, vector<string>(), vector<string>(), DE_NULL);
}

Move<VkInstance> createInstanceWithExtensions (const PlatformInterface&			vkp,
											   const deUint32					version,
											   const std::vector<std::string>	requiredExtensions)
{
	std::vector<std::string>					extensionPtrs;
	const std::vector<VkExtensionProperties>	availableExtensions	= enumerateInstanceExtensionProperties(vkp, DE_NULL);
	for (size_t extensionID = 0; extensionID < requiredExtensions.size(); extensionID++)
	{
		if (!isInstanceExtensionSupported(version, availableExtensions, RequiredExtension(requiredExtensions[extensionID])))
			TCU_THROW(NotSupportedError, (requiredExtensions[extensionID] + " is not supported").c_str());

		if (!isCoreInstanceExtension(version, requiredExtensions[extensionID]))
			extensionPtrs.push_back(requiredExtensions[extensionID]);
	}

	return createDefaultInstance(vkp, version, std::vector<std::string>() /* layers */, extensionPtrs, DE_NULL);
}

Move<VkInstance> createInstanceWithExtension (const PlatformInterface&	vkp,
											  const deUint32			version,
											  const std::string			requiredExtension)
{
	return createInstanceWithExtensions(vkp, version, std::vector<std::string>(1, requiredExtension));
}

deUint32 chooseDeviceIndex (const InstanceInterface& vkInstance, const VkInstance instance, const tcu::CommandLine& cmdLine)
{
	const vector<VkPhysicalDevice>			devices					= enumeratePhysicalDevices(vkInstance, instance);

	if (devices.empty())
		TCU_THROW(NotSupportedError, "No Vulkan devices available");

	const deUint32							deviceIdFromCmdLine		= cmdLine.getVKDeviceId();
	if (!de::inBounds(deviceIdFromCmdLine, 0u, static_cast<deUint32>(devices.size() + 1)))
		TCU_THROW(InternalError, "Invalid --deqp-vk-device-id");

	if (deviceIdFromCmdLine > 0)
		return deviceIdFromCmdLine - 1u;

	deUint32								maxReportedApiVersion	= 0u;
	deUint32								ndxOfMaximumVersion		= 0u;

	for (deUint32 deviceNdx = 0u; deviceNdx < devices.size(); ++deviceNdx)
	{
		const VkPhysicalDeviceProperties	props					= getPhysicalDeviceProperties(vkInstance, devices[deviceNdx]);

		if (props.apiVersion > maxReportedApiVersion)
		{
			maxReportedApiVersion = props.apiVersion;
			ndxOfMaximumVersion = deviceNdx;
		}
	}

	return ndxOfMaximumVersion;
}

VkPhysicalDevice chooseDevice (const InstanceInterface& vkInstance, const VkInstance instance, const tcu::CommandLine& cmdLine)
{
	const vector<VkPhysicalDevice>	devices		= enumeratePhysicalDevices(vkInstance, instance);

	if (devices.empty())
		TCU_THROW(NotSupportedError, "No Vulkan devices available");

	const size_t					deviceId	= chooseDeviceIndex(vkInstance, instance, cmdLine);
	return devices[deviceId];
}

} // vk
