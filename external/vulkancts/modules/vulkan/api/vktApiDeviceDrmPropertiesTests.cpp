/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2021 NVIDIA, Inc.
* Copyright (c) 2021 The Khronos Group Inc.
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
* \brief VK_EXT_device_drm_properties tests
*//*--------------------------------------------------------------------*/

#include "vktApiDeviceDrmPropertiesTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "deFilePath.hpp"
#include "deDirectoryIterator.hpp"
#include "deDynamicLibrary.hpp"
#include "tcuLibDrm.hpp"

using namespace vk;

namespace vkt
{
namespace api
{
namespace
{

enum TestType
{
	TEST_FILES_EXIST			= 0,
};

void checkSupport (Context& context, const TestType config)
{
	DE_UNREF(config);
	context.requireDeviceFunctionality("VK_EXT_physical_device_drm");
}

void testFilesExist (const VkPhysicalDeviceDrmPropertiesEXT& deviceDrmProperties)
{
	bool primaryFound = !deviceDrmProperties.hasPrimary;
	bool renderFound = !deviceDrmProperties.hasRender;

#if DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)
	static const tcu::LibDrm libDrm;

	int numDrmDevices;
	drmDevicePtr* drmDevices = libDrm.getDevices(&numDrmDevices);

	if (libDrm.findDeviceNode(drmDevices, numDrmDevices,
							  deviceDrmProperties.primaryMajor,
							  deviceDrmProperties.primaryMinor))
		primaryFound = true;
	if (libDrm.findDeviceNode(drmDevices, numDrmDevices,
							  deviceDrmProperties.renderMajor,
							  deviceDrmProperties.renderMinor))
		renderFound = true;

	libDrm.freeDevices(drmDevices, numDrmDevices);
#endif // DEQP_SUPPORT_DRM && !defined (CTS_USES_VULKANSC)

	if (!primaryFound && !renderFound) {
		TCU_THROW(NotSupportedError, "Neither DRM primary nor render device files were found");
	}
}

static tcu::TestStatus testDeviceDrmProperties (Context& context, const TestType testType)
{

	const VkPhysicalDevice				physDevice			= context.getPhysicalDevice();
	VkPhysicalDeviceProperties2			deviceProperties2;
	const int							memsetPattern		= 0xaa;
	VkPhysicalDeviceDrmPropertiesEXT	deviceDrmProperties;

	deMemset(&deviceDrmProperties, 0, sizeof(deviceDrmProperties));
	deviceDrmProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
	deviceDrmProperties.pNext = DE_NULL;

	deMemset(&deviceProperties2, memsetPattern, sizeof(deviceProperties2));
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &deviceDrmProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(physDevice, &deviceProperties2);

	switch (testType)
	{
		case TEST_FILES_EXIST:			testFilesExist		(deviceDrmProperties);					break;
		default:						TCU_THROW(InternalError, "Unknown test type specified");
	}

	return tcu::TestStatus::pass("Pass");
}

static void createTestCases (tcu::TestCaseGroup* group)
{
	addFunctionCase(group, "drm_files_exist",		"Verify device files for major/minor nodes exist",		checkSupport,	testDeviceDrmProperties,	TEST_FILES_EXIST);
}

} // anonymous

tcu::TestCaseGroup*	createDeviceDrmPropertiesTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "device_drm_properties", "VK_EXT_device_drm_properties tests", createTestCases);
}

} // api
} // vkt
