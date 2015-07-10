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
	TestLog&						log		= context.getTestContext().getLog();
	const vector<VkPhysicalDevice>	devices	= vk::enumeratePhysicalDevices(context.getInstanceInterface(), context.getInstance());

	log << TestLog::Integer("NumDevices", "Number of devices", "", QP_KEY_TAG_NONE, deInt64(devices.size()));

	for (size_t ndx = 0; ndx < devices.size(); ndx++)
		log << TestLog::Message << ndx << ": " << devices[ndx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Enumerating devices succeeded");
}

tcu::TestStatus queueProperties (Context& context)
{
	TestLog&										log					= context.getTestContext().getLog();
	const vector<VkPhysicalDeviceQueueProperties>	queueProperties		= getPhysicalDeviceQueueProperties(context.getInstanceInterface(), context.getPhysicalDevice());

	log << TestLog::Message << "device = " << context.getPhysicalDevice() << TestLog::EndMessage;

	for (size_t queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
		log << TestLog::Message << queueNdx << ": " << queueProperties[queueNdx] << TestLog::EndMessage;

	return tcu::TestStatus::pass("Querying queue properties succeeded");
}

tcu::TestStatus memoryProperties (Context& context)
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

	addFunctionCase(infoTests.get(), "physical_devices",	"Physical devices",		enumeratePhysicalDevices);
	addFunctionCase(infoTests.get(), "queue_properties",	"Queue properties",		queueProperties);
	addFunctionCase(infoTests.get(), "memory_properties",	"Memory properties",	memoryProperties);

	return infoTests.release();
}

} // vkt
