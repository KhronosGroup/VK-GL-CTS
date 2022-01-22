/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2019 Advanced Micro Devices, Inc.
* Copyright (c) 2019 The Khronos Group Inc.
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
* \brief VK_EXT_tooling_info tests
*//*--------------------------------------------------------------------*/

#include "vktApiToolingInfoTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include <iostream>
#include <string>
#include <vector>

#include <string.h>

using namespace vk;

namespace vkt
{
namespace api
{
namespace
{

bool validateToolPurposeFlagBits(const VkToolPurposeFlagsEXT purposes)
{
	const VkToolPurposeFlagsEXT validPurposes =	VK_TOOL_PURPOSE_VALIDATION_BIT_EXT			|
												VK_TOOL_PURPOSE_PROFILING_BIT_EXT			|
												VK_TOOL_PURPOSE_TRACING_BIT_EXT				|
												VK_TOOL_PURPOSE_ADDITIONAL_FEATURES_BIT_EXT	|
												VK_TOOL_PURPOSE_MODIFYING_FEATURES_BIT_EXT	|
												VK_TOOL_PURPOSE_DEBUG_REPORTING_BIT_EXT		|
												VK_TOOL_PURPOSE_DEBUG_MARKERS_BIT_EXT;
	return (purposes | validPurposes) == validPurposes;
}

void checkSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_tooling_info");
}

tcu::TestStatus validateGetter(Context& context)
{
	tcu::TestLog& testLog = context.getTestContext().getLog();

	VkResult result		= VK_SUCCESS;
	deUint32 toolCount	= 0;

	result = context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCount, DE_NULL);

	if(result != VK_SUCCESS)
	{
		testLog << tcu::TestLog::Message << "getPhysicalDeviceToolPropertiesEXT wrong result code" << tcu::TestLog::EndMessage;
		return tcu::TestStatus::fail("Fail");
	}

	if (toolCount > 0)
	{
		deUint32 toolCountSecondCall = toolCount;

		std::vector<VkPhysicalDeviceToolPropertiesEXT>	deviceToolPropertiesEXTArray(toolCountSecondCall);

		for (size_t toolNdx = 0; toolNdx < deviceToolPropertiesEXTArray.size(); ++toolNdx)
		{
			deviceToolPropertiesEXTArray[toolNdx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT;
		}

		result = context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCountSecondCall, &deviceToolPropertiesEXTArray[0]);

		if (result != VK_SUCCESS)
		{
			testLog << tcu::TestLog::Message << "getPhysicalDeviceToolPropertiesEXT wrong result code" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		if (toolCountSecondCall != toolCount)
		{
			testLog << tcu::TestLog::Message << "Got different tools count on the second call" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		toolCountSecondCall++;

		deviceToolPropertiesEXTArray.resize(toolCountSecondCall);

		for (size_t toolNdx = 0; toolNdx < deviceToolPropertiesEXTArray.size(); ++toolNdx)
		{
			deviceToolPropertiesEXTArray[toolNdx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT;
		}

		result = context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCountSecondCall, &deviceToolPropertiesEXTArray[0]);

		if (result != VK_SUCCESS)
		{
			testLog << tcu::TestLog::Message << "getPhysicalDeviceToolPropertiesEXT wrong result code" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		if (toolCountSecondCall != toolCount)
		{
			testLog << tcu::TestLog::Message << "Bigger array causes an error" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		toolCountSecondCall = 0;

		result = context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCountSecondCall, &deviceToolPropertiesEXTArray[0]);

		if (result != VK_INCOMPLETE)
		{
			testLog << tcu::TestLog::Message << "getPhysicalDeviceToolPropertiesEXT wrong result code" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		if (toolCountSecondCall != 0)
		{
			testLog << tcu::TestLog::Message << "Zero array causes an error" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	if (toolCount > 1)
	{
		deUint32 toolCountSecondCall = toolCount / 2;

		std::vector<VkPhysicalDeviceToolPropertiesEXT>	deviceToolPropertiesEXTArray(toolCountSecondCall);

		for (size_t toolNdx = 0; toolNdx < deviceToolPropertiesEXTArray.size(); ++toolNdx)
		{
			deviceToolPropertiesEXTArray[toolNdx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT;
		}

		result = context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCountSecondCall, &deviceToolPropertiesEXTArray[0]);

		if (result != VK_INCOMPLETE)
		{
			testLog << tcu::TestLog::Message << "getPhysicalDeviceToolPropertiesEXT wrong result code" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}

		if (toolCountSecondCall != (toolCount / 2))
		{
			testLog << tcu::TestLog::Message << "Smaller array causes an error" << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("Fail");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus validateToolsProperties (Context& context)
{
	tcu::TestLog& testLog = context.getTestContext().getLog();

	bool	 result		= true;
	deUint32 toolCount	= 0;

	VK_CHECK(context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCount, DE_NULL));

	if (toolCount > 0)
	{
		std::vector<VkPhysicalDeviceToolPropertiesEXT>	deviceToolPropertiesEXTArray(toolCount);

		for (size_t toolNdx = 0; toolNdx < deviceToolPropertiesEXTArray.size(); ++toolNdx)
		{
			deviceToolPropertiesEXTArray[toolNdx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT;
		}

		VK_CHECK(context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(context.getPhysicalDevice(), &toolCount, &deviceToolPropertiesEXTArray[0]));

		for (deUint32 i = 0; i < toolCount; ++i)
		{
			size_t nameSize		= strnlen(deviceToolPropertiesEXTArray[i].name, VK_MAX_EXTENSION_NAME_SIZE);
			size_t versionSize	= strnlen(deviceToolPropertiesEXTArray[i].version, VK_MAX_EXTENSION_NAME_SIZE);
			size_t descSize		= strnlen(deviceToolPropertiesEXTArray[i].description, VK_MAX_DESCRIPTION_SIZE);
			size_t layerSize	= strnlen(deviceToolPropertiesEXTArray[i].layer, VK_MAX_EXTENSION_NAME_SIZE);

			result = result && (deviceToolPropertiesEXTArray[i].sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT);
			result = result && validateToolPurposeFlagBits(deviceToolPropertiesEXTArray[i].purposes);
			result = result && ((nameSize > 0)		&& (nameSize < VK_MAX_EXTENSION_NAME_SIZE));
			result = result && ((versionSize > 0)	&& (versionSize < VK_MAX_EXTENSION_NAME_SIZE));
			result = result && ((descSize > 0)		&& (descSize < VK_MAX_DESCRIPTION_SIZE));
			result = result && ((layerSize == 0)	|| (layerSize < VK_MAX_EXTENSION_NAME_SIZE));

			if (result == false)
			{
				testLog << tcu::TestLog::Message << "Tool validation failed" << tcu::TestLog::EndMessage;
				testLog << tcu::TestLog::Message << "Tool name: " << deviceToolPropertiesEXTArray[i].name << tcu::TestLog::EndMessage;
				testLog << tcu::TestLog::Message << "Version: " << deviceToolPropertiesEXTArray[i].version << tcu::TestLog::EndMessage;
				testLog << tcu::TestLog::Message << "Description: " << deviceToolPropertiesEXTArray[i].description << tcu::TestLog::EndMessage;
				testLog << tcu::TestLog::Message << "Purposes: " << getToolPurposeFlagsEXTStr(deviceToolPropertiesEXTArray[i].purposes) << tcu::TestLog::EndMessage;
				if (layerSize > 0)
				{
					testLog << tcu::TestLog::Message << "Corresponding Layer: " << deviceToolPropertiesEXTArray[i].layer << tcu::TestLog::EndMessage;
				}
				break;
			}
		}
	}

	if (result)
	{
		return tcu::TestStatus::pass("Pass");
	}
	else
	{
		return tcu::TestStatus::fail("Fail");
	}
}

void createTestCases (tcu::TestCaseGroup* group)
{
	addFunctionCase(group, "validate_getter", "Validate getPhysicalDeviceToolPropertiesEXT", checkSupport, validateGetter);
	addFunctionCase(group, "validate_tools_properties","Validate tools properties",	checkSupport, validateToolsProperties);
}

} // anonymous

tcu::TestCaseGroup*	createToolingInfoTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "tooling_info", "VK_EXT_tooling_info tests", createTestCases);
}

} // api
} // vkt
