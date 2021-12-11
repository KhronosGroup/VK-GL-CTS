/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2018 Advanced Micro Devices, Inc.
* Copyright (c) 2018 The Khronos Group Inc.
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
* \brief VK_KHR_driver_properties tests
*//*--------------------------------------------------------------------*/

#include "vktApiDriverPropertiesTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkKnownDriverIds.inl"

using namespace vk;

namespace vkt
{
namespace api
{
namespace
{

enum TestType
{
	TEST_TYPE_DRIVER_ID_MATCH			= 0,
	TEST_TYPE_NAME_IS_NOT_EMPTY,
	TEST_TYPE_NAME_ZERO_TERMINATED,
	TEST_TYPE_INFO_ZERO_TERMINATED,
	TEST_TYPE_VERSION,
};

static const VkConformanceVersionKHR knownConformanceVersions[] =
{
	makeConformanceVersion(1, 3, 1, 0),
	makeConformanceVersion(1, 3, 0, 0),
	makeConformanceVersion(1, 2, 8, 0),
	makeConformanceVersion(1, 2, 7, 2),
	makeConformanceVersion(1, 2, 7, 1),
	makeConformanceVersion(1, 2, 7, 0),
	makeConformanceVersion(1, 2, 6, 2),
	makeConformanceVersion(1, 2, 6, 1),
	makeConformanceVersion(1, 2, 6, 0),
	makeConformanceVersion(1, 2, 5, 2),
	makeConformanceVersion(1, 2, 5, 1),
	makeConformanceVersion(1, 2, 5, 0),
	makeConformanceVersion(1, 2, 4, 1),
	makeConformanceVersion(1, 2, 4, 0),
	makeConformanceVersion(1, 2, 3, 3),
	makeConformanceVersion(1, 2, 3, 2),
	makeConformanceVersion(1, 2, 3, 1),
	makeConformanceVersion(1, 2, 3, 0),
	makeConformanceVersion(1, 2, 2, 2),
	makeConformanceVersion(1, 2, 2, 1),
	makeConformanceVersion(1, 2, 2, 0),
	makeConformanceVersion(1, 2, 1, 2),
	makeConformanceVersion(1, 2, 1, 1),
	makeConformanceVersion(1, 2, 1, 0),
	makeConformanceVersion(1, 2, 0, 2),
	makeConformanceVersion(1, 2, 0, 1),
	makeConformanceVersion(1, 2, 0, 0),
	makeConformanceVersion(1, 1, 6, 3),
	makeConformanceVersion(1, 1, 6, 2),
	makeConformanceVersion(1, 1, 6, 1),
	makeConformanceVersion(1, 1, 6, 0),
	makeConformanceVersion(1, 1, 5, 2),
	makeConformanceVersion(1, 1, 5, 1),
	makeConformanceVersion(1, 1, 5, 0),
	makeConformanceVersion(1, 1, 4, 3),
	makeConformanceVersion(1, 1, 4, 2),
	makeConformanceVersion(1, 1, 4, 1),
	makeConformanceVersion(1, 1, 4, 0),
	makeConformanceVersion(1, 1, 3, 3),
	makeConformanceVersion(1, 1, 3, 2),
	makeConformanceVersion(1, 1, 3, 1),
	makeConformanceVersion(1, 1, 3, 0),
};

DE_INLINE bool isNullTerminated(const char* str, const deUint32 maxSize)
{
	return deStrnlen(str, maxSize) < maxSize;
}

DE_INLINE bool operator==(const VkConformanceVersion& a, const VkConformanceVersion& b)
{
	return ((a.major == b.major)		&&
			(a.minor == b.minor)		&&
			(a.subminor == b.subminor)	&&
			(a.patch == b.patch));
}

void checkSupport (Context& context, const TestType config)
{
	DE_UNREF(config);
	context.requireDeviceFunctionality("VK_KHR_driver_properties");
}

void testDriverMatch (const VkPhysicalDeviceDriverPropertiesKHR& deviceDriverProperties)
{
	for (deUint32 driverNdx = 0; driverNdx < DE_LENGTH_OF_ARRAY(driverIds); driverNdx++)
	{
		if (deviceDriverProperties.driverID == driverIds[driverNdx].id)
			return;
	}

	TCU_FAIL("Driver ID did not match any known driver");
}

void testNameIsNotEmpty (const VkPhysicalDeviceDriverPropertiesKHR& deviceDriverProperties)
{
	if (deviceDriverProperties.driverName[0] == 0)
		TCU_FAIL("Driver name is empty");
}

void testNameZeroTerminated (const VkPhysicalDeviceDriverPropertiesKHR& deviceDriverProperties)
{
	if (!isNullTerminated(deviceDriverProperties.driverName, VK_MAX_DRIVER_NAME_SIZE_KHR))
		TCU_FAIL("Driver name is not a null-terminated string");
}

void testInfoZeroTerminated (const VkPhysicalDeviceDriverPropertiesKHR& deviceDriverProperties)
{
	if (!isNullTerminated(deviceDriverProperties.driverInfo, VK_MAX_DRIVER_INFO_SIZE_KHR))
		TCU_FAIL("Driver info is not a null-terminated string");
}

void testVersion (const VkPhysicalDeviceDriverPropertiesKHR& deviceDriverProperties, deUint32 usedApiVersion)
{
	const deUint32 apiMajorVersion = VK_API_VERSION_MAJOR(usedApiVersion);
	const deUint32 apiMinorVersion = VK_API_VERSION_MINOR(usedApiVersion);

	if (deviceDriverProperties.conformanceVersion.major < apiMajorVersion ||
		(deviceDriverProperties.conformanceVersion.major == apiMajorVersion &&
		 deviceDriverProperties.conformanceVersion.minor < apiMinorVersion))
	{
		TCU_FAIL("Wrong driver conformance version (older than used API version)");
	}

	for (const VkConformanceVersionKHR* pConformanceVersion  = knownConformanceVersions;
										pConformanceVersion != DE_ARRAY_END(knownConformanceVersions);
									  ++pConformanceVersion)
	{
		if (deviceDriverProperties.conformanceVersion == *pConformanceVersion)
			return;
	}

	TCU_FAIL("Wrong driver conformance version (not known)");
}

tcu::TestStatus testQueryProperties (Context& context, const TestType testType)
{
	// Query the driver properties
	const VkPhysicalDevice				physDevice			= context.getPhysicalDevice();
	const int							memsetPattern		= 0xaa;
	VkPhysicalDeviceProperties2			deviceProperties2;
	VkPhysicalDeviceDriverProperties	deviceDriverProperties;

	deMemset(&deviceDriverProperties, memsetPattern, sizeof(deviceDriverProperties));
	deviceDriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
	deviceDriverProperties.pNext = DE_NULL;

	deMemset(&deviceProperties2, memsetPattern, sizeof(deviceProperties2));
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &deviceDriverProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(physDevice, &deviceProperties2);

	// Verify the returned values
	switch (testType)
	{
		case TEST_TYPE_DRIVER_ID_MATCH:			testDriverMatch			(deviceDriverProperties);								break;
		case TEST_TYPE_NAME_IS_NOT_EMPTY:		testNameIsNotEmpty		(deviceDriverProperties);								break;
		case TEST_TYPE_NAME_ZERO_TERMINATED:	testNameZeroTerminated	(deviceDriverProperties);								break;
		case TEST_TYPE_INFO_ZERO_TERMINATED:	testInfoZeroTerminated	(deviceDriverProperties);								break;
		case TEST_TYPE_VERSION:					testVersion				(deviceDriverProperties, context.getUsedApiVersion());	break;
		default:								TCU_THROW(InternalError, "Unknown test type specified");
	}

	return tcu::TestStatus::pass("Pass");
}

void createTestCases (tcu::TestCaseGroup* group)
{
	addFunctionCase(group, "driver_id_match",		"Check driverID is supported",					checkSupport,	testQueryProperties,	TEST_TYPE_DRIVER_ID_MATCH);
	addFunctionCase(group, "name_is_not_empty",		"Check name field is not empty",				checkSupport,	testQueryProperties,	TEST_TYPE_NAME_IS_NOT_EMPTY);
	addFunctionCase(group, "name_zero_terminated",	"Check name field is zero-terminated",			checkSupport,	testQueryProperties,	TEST_TYPE_NAME_ZERO_TERMINATED);
	addFunctionCase(group, "info_zero_terminated",	"Check info field is zero-terminated",			checkSupport,	testQueryProperties,	TEST_TYPE_INFO_ZERO_TERMINATED);
	addFunctionCase(group, "conformance_version",	"Check conformanceVersion reported by driver",	checkSupport,	testQueryProperties,	TEST_TYPE_VERSION);
}

} // anonymous

tcu::TestCaseGroup*	createDriverPropertiesTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "driver_properties", "VK_KHR_driver_properties tests", createTestCases);
}

} // api
} // vkt
