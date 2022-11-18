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

#include "deSTLUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkApiVersion.hpp"
#include "vkDebugReportUtil.hpp"

#include "tcuCommandLine.hpp"

#include "qpInfo.h"

#ifdef CTS_USES_VULKANSC
#include "vkAppParamsUtil.hpp"
#endif // CTS_USES_VULKANSC

namespace vk
{

using std::vector;
using std::string;

Move<VkInstance> createDefaultInstance (const PlatformInterface&		vkPlatform,
										deUint32						apiVersion,
										const vector<string>&			enabledLayers,
										const vector<string>&			enabledExtensions,
										const tcu::CommandLine&			cmdLine,
#ifndef CTS_USES_VULKANSC
										DebugReportRecorder*			recorder,
#endif // CTS_USES_VULKANSC
										const VkAllocationCallbacks*	pAllocator)
{
	bool			validationEnabled	= (!enabledLayers.empty());
	vector<string>	actualExtensions	= enabledExtensions;

    // Enumerate once, pass it in to the various functions that require the list of available extensions
	vector<vk::VkExtensionProperties> availableExtensions = enumerateInstanceExtensionProperties(vkPlatform, DE_NULL);

#ifndef CTS_USES_VULKANSC
	if (validationEnabled)
	{
		// Make sure the debug report extension is enabled when validation is enabled.
		if (!isExtensionStructSupported(availableExtensions, RequiredExtension("VK_EXT_debug_report")))
			TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

		if (!de::contains(begin(actualExtensions), end(actualExtensions), "VK_EXT_debug_report"))
			actualExtensions.push_back("VK_EXT_debug_report");

		DE_ASSERT(recorder);
	}

        // Make sure portability enumeration is enabled whenever it is available
        bool portability_enumeration_available = isExtensionStructSupported(availableExtensions, RequiredExtension("VK_KHR_portability_enumeration"));
        if (portability_enumeration_available)
        {
                actualExtensions.push_back("VK_KHR_portability_enumeration");
        }

#endif // CTS_USES_VULKANSC

	vector<const char*>		layerNamePtrs		(enabledLayers.size());
	vector<const char*>		extensionNamePtrs	(actualExtensions.size());

	for (size_t ndx = 0; ndx < enabledLayers.size(); ++ndx)
		layerNamePtrs[ndx] = enabledLayers[ndx].c_str();

	for (size_t ndx = 0; ndx < actualExtensions.size(); ++ndx)
		extensionNamePtrs[ndx] = actualExtensions[ndx].c_str();

#ifdef CTS_USES_VULKANSC
	vector<VkApplicationParametersEXT> appParams;
	const bool hasAppParams = readApplicationParameters(appParams, cmdLine, true);
#else
	DE_UNREF(cmdLine);
#endif // CTS_USES_VULKANSC

	const struct VkApplicationInfo			appInfo			=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
#ifdef CTS_USES_VULKANSC
		hasAppParams ? &appParams[0] : DE_NULL,
#else
		DE_NULL,
#endif // CTS_USES_VULKANSC
		"deqp",									// pAppName
		qpGetReleaseId(),						// appVersion
		"deqp",									// pEngineName
		qpGetReleaseId(),						// engineVersion
		apiVersion								// apiVersion
	};


#ifndef CTS_USES_VULKANSC
	const VkDebugReportCallbackCreateInfoEXT callbackInfo = (validationEnabled ? recorder->makeCreateInfo() : initVulkanStructure());
#endif // CTS_USES_VULKANSC

	const struct VkInstanceCreateInfo	instanceInfo	=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#ifndef CTS_USES_VULKANSC
		(validationEnabled ? &callbackInfo : nullptr),
		(VkInstanceCreateFlags)(portability_enumeration_available ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0),
#else
		nullptr,
		(VkInstanceCreateFlags)0,
#endif // CTS_USES_VULKANSC
		&appInfo,
		(deUint32)layerNamePtrs.size(),
		(validationEnabled ? layerNamePtrs.data() : nullptr),
		(deUint32)extensionNamePtrs.size(),
		(extensionNamePtrs.empty() ? nullptr : extensionNamePtrs.data()),
	};

	return createInstance(vkPlatform, &instanceInfo, pAllocator);
}

Move<VkInstance> createDefaultInstance (const PlatformInterface& vkPlatform, deUint32 apiVersion, const tcu::CommandLine& cmdLine)
{
#ifndef CTS_USES_VULKANSC
	return createDefaultInstance(vkPlatform, apiVersion, vector<string>(), vector<string>(), cmdLine, nullptr, nullptr);
#else
	return createDefaultInstance(vkPlatform, apiVersion, vector<string>(), vector<string>(), cmdLine, nullptr);
#endif
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
