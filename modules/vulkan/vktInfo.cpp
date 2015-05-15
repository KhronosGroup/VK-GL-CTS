/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
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
 * \brief Platform information tests
 *//*--------------------------------------------------------------------*/

#include "vktInfo.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include "qpInfo.h"

namespace vkt
{

using namespace vk;
using std::vector;
using std::string;
using tcu::TestLog;

tcu::TestStatus enumeratePhysicalDevices (Context& context)
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

	const PlatformInterface&	vkPlatform	= context.getPlatformInterface();
	TestLog&					log			= context.getTestContext().getLog();
	const Unique<VkInstanceT>	instance	(createInstance(vkPlatform, &instanceInfo));
	vector<VkPhysicalDevice>	devices;
	deUint32					numDevices	= 0;

	VK_CHECK(vkPlatform.enumeratePhysicalDevices(*instance, &numDevices, DE_NULL));

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(numDevices));

	if (numDevices > 0)
	{
		devices.resize(numDevices);
		VK_CHECK(vkPlatform.enumeratePhysicalDevices(*instance, &numDevices, &devices[0]));

		for (deUint32 ndx = 0; ndx < numDevices; ndx++)
			log << TestLog::Message << ndx << ": " << tcu::toHex(devices[ndx]) << TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

tcu::TestStatus deviceProperties (Context& context)
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

	const PlatformInterface&	vkPlatform	= context.getPlatformInterface();
	TestLog&					log			= context.getTestContext().getLog();
	const Unique<VkInstanceT>	instance	(createInstance(vkPlatform, &instanceInfo));
	vector<VkPhysicalDevice>	devices;
	deUint32					numDevices	= 0;

	VK_CHECK(vkPlatform.enumeratePhysicalDevices(*instance, &numDevices, DE_NULL));

	if (numDevices > 0)
	{
		devices.resize(numDevices);
		VK_CHECK(vkPlatform.enumeratePhysicalDevices(*instance, &numDevices, &devices[0]));

		for (deUint32 ndx = 0; ndx < numDevices; ndx++)
		{
			const VkPhysicalDevice		physicalDevice		= devices[ndx];
			const tcu::ScopedLogSection	section				(log, string("Device") + de::toString(ndx), string("Device ") + de::toString(ndx) + " (" + de::toString(tcu::toHex(physicalDevice)) + ")");
			const vk::DeviceDriver		vkDevice			(vkPlatform, physicalDevice);
			VkPhysicalDeviceProperties	properties;
			deUintptr					propertiesSize		= sizeof(properties);

			VK_CHECK(vkDevice.getPhysicalDeviceInfo(physicalDevice, VK_PHYSICAL_DEVICE_INFO_TYPE_PROPERTIES, &propertiesSize, &properties));

			log << TestLog::Message << "apiVersion = " << unpackVersion(properties.apiVersion) << "\n"
									<< "driverVersion = " << tcu::toHex(properties.driverVersion) << "\n"
									<< "vendorId = " << tcu::toHex(properties.vendorId) << "\n"
									<< "deviceId = " << tcu::toHex(properties.deviceId) << "\n"
									<< "deviceType = " << properties.deviceType << "\n"
									<< "deviceName = " << properties.deviceName << "\n"
									<< "maxInlineMemoryUpdateSize = " << properties.maxInlineMemoryUpdateSize << "\n"
									<< "maxBoundDescriptorSets = " << properties.maxBoundDescriptorSets << "\n"
									<< "maxThreadGroupSize = " << properties.maxThreadGroupSize << "\n"
									<< "timestampFrequency = " << properties.timestampFrequency << "\n"
									<< "multiColorAttachmentClears = " << properties.multiColorAttachmentClears << "\n"
									<< "maxDescriptorSets = " << properties.maxDescriptorSets << "\n"
									<< "maxViewports = " << properties.maxViewports << "\n"
									<< "maxColorAttachments = " << properties.maxColorAttachments
				<< TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

tcu::TestCaseGroup* createInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	addFunctionCase(infoTests.get(), "physical_devices",	"Physical devices",		enumeratePhysicalDevices);
	addFunctionCase(infoTests.get(), "device_properties",	"Device properties",	deviceProperties);

	return infoTests.release();
}

} // vkt
