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

using namespace vk;

namespace vkt
{
namespace api
{
namespace
{

static const deUint32 knownDriverIds[] =
{
	// Specified in the Vulkan registry (vk.xml)
	1,	// author = "Advanced Micro Devices, Inc."   comment = "AMD proprietary driver"
	2,	// author = "Advanced Micro Devices, Inc."   comment = "AMD open-source driver"
	3,	// author = "Mesa open source project"       comment = "Mesa RADV driver"
	4,	// author = "NVIDIA Corporation"             comment = "NVIDIA proprietary driver"
	5,	// author = "Intel Corporation"              comment = "Intel proprietary Windows driver"
	6,	// author = "Intel Corporation"              comment = "Intel open-source Mesa driver"
	7,	// author = "Imagination Technologies"       comment = "Imagination proprietary driver"
	8,	// author = "Qualcomm Technologies, Inc."    comment = "Qualcomm proprietary driver"
	9,	// author = "Arm Limited"                    comment = "Arm proprietary driver"
};

static const VkConformanceVersionKHR knownConformanceVersions[] =
{
	makeConformanceVersionKHR(1, 1, 4, 1),
	makeConformanceVersionKHR(1, 1, 4, 0),
	makeConformanceVersionKHR(1, 1, 3, 2),
	makeConformanceVersionKHR(1, 1, 3, 1),
	makeConformanceVersionKHR(1, 1, 3, 0),
	makeConformanceVersionKHR(1, 1, 2, 3),
	makeConformanceVersionKHR(1, 1, 2, 2),
	makeConformanceVersionKHR(1, 1, 2, 1),
	makeConformanceVersionKHR(1, 1, 2, 0),
	makeConformanceVersionKHR(1, 1, 1, 3),
	makeConformanceVersionKHR(1, 1, 1, 2),
	makeConformanceVersionKHR(1, 1, 1, 1),
	makeConformanceVersionKHR(1, 1, 1, 0),
	makeConformanceVersionKHR(1, 1, 0, 3),
};

DE_INLINE bool isNullTerminated(const char* str, const deUint32 maxSize)
{
	return deStrnlen(str, maxSize) < maxSize;
}

DE_INLINE bool operator==(const VkConformanceVersionKHR& a, const VkConformanceVersionKHR& b)
{
	return ((a.major == b.major)		&&
			(a.minor == b.minor)		&&
			(a.subminor == b.subminor)	&&
			(a.patch == b.patch));
}

tcu::TestStatus testQueryProperties (Context& context)
{
	// Check extension support

	if (!isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_driver_properties"))
		TCU_THROW(NotSupportedError, "Unsupported extension: VK_KHR_driver_properties");

	// Query the driver properties

	const VkPhysicalDevice				physDevice			= context.getPhysicalDevice();
	const int							memsetPattern		= 0xaa;
	VkPhysicalDeviceProperties2			deviceProperties2;
	VkPhysicalDeviceDriverPropertiesKHR	deviceDriverProperties;

	deMemset(&deviceDriverProperties, memsetPattern, sizeof(deviceDriverProperties));
	deviceDriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
	deviceDriverProperties.pNext = DE_NULL;

	deMemset(&deviceProperties2, memsetPattern, sizeof(deviceProperties2));
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &deviceDriverProperties;

	context.getInstanceInterface().getPhysicalDeviceProperties2(physDevice, &deviceProperties2);

	// Verify the returned values

	bool match = false;

	for (const deUint32* pDriverId = knownDriverIds; (pDriverId != DE_ARRAY_END(knownDriverIds)) && !match; ++pDriverId)
	{
		if (deviceDriverProperties.driverID == *pDriverId)
		{
			match = true;

			if (!isNullTerminated(deviceDriverProperties.driverName, VK_MAX_DRIVER_NAME_SIZE_KHR))
				TCU_FAIL("Driver name is not a null-terminated string");

			if (deviceDriverProperties.driverName[0] == 0)
				TCU_FAIL("Driver name is empty");

			if (!isNullTerminated(deviceDriverProperties.driverInfo, VK_MAX_DRIVER_INFO_SIZE_KHR))
				TCU_FAIL("Driver info is not a null-terminated string");

			bool conformanceVersionMatch = false;

			for (const VkConformanceVersionKHR* pConformanceVersion  = knownConformanceVersions;
												pConformanceVersion != DE_ARRAY_END(knownConformanceVersions);
											  ++pConformanceVersion)
			{
				if (deviceDriverProperties.conformanceVersion == *pConformanceVersion)
				{
					conformanceVersionMatch = true;
					break;
				}
			}

			if (!conformanceVersionMatch)
				TCU_FAIL("Wrong driver conformance version");
		}
	}

	if (!match)
		TCU_FAIL("Driver ID did not match any known driver");

	return tcu::TestStatus::pass("Pass");
}

void createTestCases (tcu::TestCaseGroup* group)
{
	addFunctionCase(group, "properties", "Query VkPhysicalDeviceDriverPropertiesKHR and check its values", testQueryProperties);
}

} // anonymous

tcu::TestCaseGroup*	createDriverPropertiesTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "driver_properties", "VK_KHR_driver_properties tests", createTestCases);
}

} // api
} // vkt
