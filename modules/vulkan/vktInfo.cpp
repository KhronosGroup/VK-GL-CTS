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
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

namespace vkt
{
namespace
{

using namespace vk;
using std::vector;
using std::string;
using tcu::TestLog;

tcu::TestStatus enumeratePhysicalDevices (Context& context)
{
	const PlatformInterface&		vkPlatform	= context.getPlatformInterface();
	TestLog&						log			= context.getTestContext().getLog();
	const VkInstance				instance	= context.getInstance();
	const vector<VkPhysicalDevice>	devices		= vk::enumeratePhysicalDevices(vkPlatform, instance);

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(devices.size()));

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
		log << TestLog::Message << ndx << ": " << tcu::toHex(devices[ndx]) << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

template<VkPhysicalDeviceInfoType InfoType>
tcu::TestStatus singleProperty (Context& context)
{
	const PlatformInterface&		vkPlatform	= context.getPlatformInterface();
	TestLog&						log			= context.getTestContext().getLog();
	const VkInstance				instance	= context.getInstance();
	const vector<VkPhysicalDevice>	devices		= vk::enumeratePhysicalDevices(vkPlatform, instance);

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
	{
		const VkPhysicalDevice		physicalDevice	= devices[ndx];
		const tcu::ScopedLogSection	section			(log, string("Device") + de::toString(ndx), string("Device ") + de::toString(ndx) + " (" + de::toString(tcu::toHex(physicalDevice)) + ")");
		const vk::DeviceDriver		vkDevice		(vkPlatform, physicalDevice);

		log << TestLog::Message << getPhysicalDeviceInfo<InfoType>(vkDevice, physicalDevice) << TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("Querying properties succeeded");
}

template<VkPhysicalDeviceInfoType InfoType>
tcu::TestStatus multiProperty (Context& context, const char* propName)
{
	typedef typename vk::querydetails::PhysicalDeviceInfoTraits<InfoType>::Type PropertyType;

	const PlatformInterface&		vkPlatform	= context.getPlatformInterface();
	TestLog&						log			= context.getTestContext().getLog();
	const VkInstance				instance	= context.getInstance();
	const vector<VkPhysicalDevice>	devices		= vk::enumeratePhysicalDevices(vkPlatform, instance);

	for (size_t deviceNdx = 0; deviceNdx < devices.size(); deviceNdx++)
	{
		const VkPhysicalDevice			physicalDevice	= devices[deviceNdx];
		const tcu::ScopedLogSection		deviceSection	(log, string("Device") + de::toString(deviceNdx), string("Device ") + de::toString(deviceNdx) + " (" + de::toString(tcu::toHex(physicalDevice)) + ")");
		const vk::DeviceDriver			vkDevice		(vkPlatform, physicalDevice);
		const vector<PropertyType>		properties		= getPhysicalDeviceInfo<InfoType>(vkDevice, physicalDevice);

		log << TestLog::Integer(string("Num") + propName + "Props", string("Number of ") + propName + " properties", "", QP_KEY_TAG_NONE, (deInt64)properties.size());

		for (size_t entryNdx = 0; entryNdx < properties.size(); entryNdx++)
			log << TestLog::Message << properties[entryNdx] << TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("Querying properties succeeded");
}

} // anonymous

tcu::TestCaseGroup* createInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	addFunctionCase(infoTests.get(), "physical_devices",	"Physical devices",		enumeratePhysicalDevices);
	addFunctionCase(infoTests.get(), "device_properties",	"Device properties",	singleProperty<VK_PHYSICAL_DEVICE_INFO_TYPE_PROPERTIES>);
	addFunctionCase(infoTests.get(), "performance",			"Performance",			singleProperty<VK_PHYSICAL_DEVICE_INFO_TYPE_PERFORMANCE>);
	addFunctionCase(infoTests.get(), "queue_properties",	"Queue properties",		multiProperty<VK_PHYSICAL_DEVICE_INFO_TYPE_QUEUE_PROPERTIES>,	"Queue");
	addFunctionCase(infoTests.get(), "memory_properties",	"Memory properties",	multiProperty<VK_PHYSICAL_DEVICE_INFO_TYPE_MEMORY_PROPERTIES>,	"Memory");

	return infoTests.release();
}

} // vkt
