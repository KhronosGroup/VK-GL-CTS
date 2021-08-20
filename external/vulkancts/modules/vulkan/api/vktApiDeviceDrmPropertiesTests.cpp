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
#if DEQP_SUPPORT_DRM
#if !defined(__FreeBSD__)
// major() and minor() are defined in sys/types.h on FreeBSD, and in
// sys/sysmacros.h on Linux and Solaris.
#include <sys/sysmacros.h>
#endif // !defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xf86drm.h>
#endif // DEQP_SUPPORT_DRM

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

#if DEQP_SUPPORT_DRM
class LibDrm : protected de::DynamicLibrary
{
	static const char* libDrmFiles[];

	typedef int (*PFNDRMGETDEVICES2PROC)(deUint32, drmDevicePtr[], int);
	typedef int (*PFNDRMGETDEVICESPROC)(drmDevicePtr[], int);
	typedef void (*PFNDRMFREEDEVICESPROC)(drmDevicePtr[], int);
	PFNDRMGETDEVICES2PROC pGetDevices2;
	PFNDRMGETDEVICESPROC pGetDevices;
	PFNDRMFREEDEVICESPROC pFreeDevices;

	int intGetDevices(drmDevicePtr devices[], int maxDevices) const
    {
		if (pGetDevices2)
			return pGetDevices2(0, devices, maxDevices);
		else
			return pGetDevices(devices, maxDevices);
	}

public:
	LibDrm() : DynamicLibrary(libDrmFiles) {
		pGetDevices2 = (PFNDRMGETDEVICES2PROC)getFunction("drmGetDevices2");
		pGetDevices = (PFNDRMGETDEVICESPROC)getFunction("drmGetDevices");
		pFreeDevices = (PFNDRMFREEDEVICESPROC)getFunction("drmFreeDevices");

		if (!pGetDevices2 && !pGetDevices)
			TCU_FAIL("Could not load a valid drmGetDevices() variant from libdrm");

		if (!pFreeDevices)
			TCU_FAIL("Could not load drmFreeDevices() from libdrm");
	}

	drmDevicePtr *getDevices(int *pNumDevices) const
	{
		*pNumDevices = intGetDevices(DE_NULL, 0);

		if (*pNumDevices < 0)
			TCU_THROW(NotSupportedError, "Failed to query number of DRM devices in system");

		if (*pNumDevices == 0)
			return DE_NULL;

		drmDevicePtr *devs = new drmDevicePtr[*pNumDevices];

		*pNumDevices = intGetDevices(devs, *pNumDevices);

		if (*pNumDevices < 0)
        {
			delete[] devs;
			TCU_FAIL("Failed to query list of DRM devices in system");
		}

		return devs;
	}

	void freeDevices(drmDevicePtr *devices, int count) const
    {
		pFreeDevices(devices, count);
		delete[] devices;
	}

	~LibDrm() { }
};

const char* LibDrm::libDrmFiles[] =
{
	"libdrm.so.2",
	"libdrm.so",
	DE_NULL
};
#endif // DEQP_SUPPORT_DRM

void testFilesExist (const VkPhysicalDeviceDrmPropertiesEXT& deviceDrmProperties)
{
	bool primaryFound = !deviceDrmProperties.hasPrimary;
	bool renderFound = !deviceDrmProperties.hasRender;

#if DEQP_SUPPORT_DRM
	static const LibDrm libDrm;

	int numDrmDevices;
	drmDevicePtr* drmDevices = libDrm.getDevices(&numDrmDevices);

	for (int i = 0; i < numDrmDevices; i++)
    {
		for (int j = 0; j < DRM_NODE_MAX; j++)
        {
			if (!(drmDevices[i]->available_nodes & (1 << j)))
				continue;

			struct stat statBuf;
			deMemset(&statBuf, 0, sizeof(statBuf));
			int res = stat(drmDevices[i]->nodes[j], &statBuf);

			if (res || !(statBuf.st_mode & S_IFCHR))
                continue;

			if (deviceDrmProperties.primaryMajor == major(statBuf.st_rdev) &&
				deviceDrmProperties.primaryMinor == minor(statBuf.st_rdev))
            {
				primaryFound = true;
				continue;
			}

			if (deviceDrmProperties.renderMajor == major(statBuf.st_rdev) &&
				deviceDrmProperties.renderMinor == minor(statBuf.st_rdev))
            {
				renderFound = true;
				continue;
			}
		}
	}

	libDrm.freeDevices(drmDevices, numDrmDevices);
#endif // DEQP_SUPPORT_DRM

	if (!primaryFound && !renderFound) {
		TCU_THROW(NotSupportedError, "Nether DRM primary nor render device files were found");
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
