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
#include "deMemory.h"

namespace vkt
{
namespace
{

using namespace vk;
using std::vector;
using std::string;
using tcu::TestLog;
using tcu::ScopedLogSection;

tcu::TestStatus enumeratePhysicalDevices (Context& context)
{
	TestLog&						log		= context.getTestContext().getLog();
	const vector<VkPhysicalDevice>	devices	= enumeratePhysicalDevices(context.getInstanceInterface(), context.getInstance());

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(devices.size()));

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
		log << TestLog::Message << ndx << ": " << devices[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

tcu::TestStatus enumerateInstanceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	const vector<VkLayerProperties>	properties	= enumerateInstanceLayerProperties(context.getPlatformInterface());

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating layers succeeded");
}

tcu::TestStatus enumerateInstanceExtensions (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateInstanceLayerProperties(context.getPlatformInterface());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateInstanceExtensionProperties(context.getPlatformInterface(), layer->layerName);

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating extensions succeeded");
}

tcu::TestStatus enumerateDeviceLayers (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	const vector<VkLayerProperties>	properties	= vk::enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	for (size_t ndx = 0; ndx < properties.size(); ndx++)
		log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating layers succeeded");
}

tcu::TestStatus enumerateDeviceExtensions (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	{
		const ScopedLogSection				section		(log, "Global", "Global Extensions");
		const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), DE_NULL);

		for (size_t ndx = 0; ndx < properties.size(); ndx++)
			log << TestLog::Message << ndx << ": " << properties[ndx] << TestLog::EndMessage;
	}

	{
		const vector<VkLayerProperties>	layers	= enumerateDeviceLayerProperties(context.getInstanceInterface(), context.getPhysicalDevice());

		for (vector<VkLayerProperties>::const_iterator layer = layers.begin(); layer != layers.end(); ++layer)
		{
			const ScopedLogSection				section		(log, layer->layerName, string("Layer: ") + layer->layerName);
			const vector<VkExtensionProperties>	properties	= enumerateDeviceExtensionProperties(context.getInstanceInterface(), context.getPhysicalDevice(), layer->layerName);

			for (size_t extNdx = 0; extNdx < properties.size(); extNdx++)
				log << TestLog::Message << extNdx << ": " << properties[extNdx] << TestLog::EndMessage;
		}
	}

	return tcu::TestStatus::pass("Enumerating extensions succeeded");
}

tcu::TestStatus deviceFeatures (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceFeatures		features;

	deMemset(&features, 0, sizeof(features));

	context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << features << TestLog::EndMessage;

	return tcu::TestStatus::pass("Query succeeded");
}

tcu::TestStatus deviceProperties (Context& context)
{
	TestLog&						log			= context.getTestContext().getLog();
	VkPhysicalDeviceProperties		props;

	deMemset(&props, 0, sizeof(props));

	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &props);

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage
		<< TestLog::Message << props << TestLog::EndMessage;

	return tcu::TestStatus::pass("Query succeeded");
}

tcu::TestStatus deviceQueueFamilyProperties (Context& context)
{
	TestLog&								log					= context.getTestContext().getLog();
	const vector<VkQueueFamilyProperties>	queueProperties		= getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage;

	for (size_t queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
		log << TestLog::Message << queueNdx << ": " << queueProperties[queueNdx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying queue properties succeeded");
}

tcu::TestStatus deviceMemoryProperties (Context& context)
{
	TestLog&	log		= context.getTestContext().getLog();

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage;

	log << TestLog::Message
		<< getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())
		<< TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying memory properties succeeded");
}

} // anonymous

tcu::TestCaseGroup* createInfoTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	infoTests	(new tcu::TestCaseGroup(testCtx, "info", "Platform Information Tests"));

	{
		de::MovePtr<tcu::TestCaseGroup> instanceInfoTests	(new tcu::TestCaseGroup(testCtx, "instance", "Instance Information Tests"));

		addFunctionCase(instanceInfoTests.get(), "physical_devices",		"Physical devices",			enumeratePhysicalDevices);
		addFunctionCase(instanceInfoTests.get(), "layers",					"Layers",					enumerateInstanceLayers);
		addFunctionCase(instanceInfoTests.get(), "extensions",				"Extensions",				enumerateInstanceExtensions);

		infoTests->addChild(instanceInfoTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> deviceInfoTests	(new tcu::TestCaseGroup(testCtx, "device", "Device Information Tests"));

		addFunctionCase(deviceInfoTests.get(), "features",					"Device Features",			deviceFeatures);
		addFunctionCase(deviceInfoTests.get(), "properties",				"Device Properties",		deviceProperties);
		addFunctionCase(deviceInfoTests.get(), "queue_family_properties",	"Queue family properties",	deviceQueueFamilyProperties);
		addFunctionCase(deviceInfoTests.get(), "memory_properties",			"Memory properties",		deviceMemoryProperties);
		addFunctionCase(deviceInfoTests.get(), "layers",					"Layers",					enumerateDeviceLayers);
		addFunctionCase(deviceInfoTests.get(), "extensions",				"Extensions",				enumerateDeviceExtensions);

		infoTests->addChild(deviceInfoTests.release());
	}

	return infoTests.release();
}

} // vkt
