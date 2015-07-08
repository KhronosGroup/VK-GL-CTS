/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
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
