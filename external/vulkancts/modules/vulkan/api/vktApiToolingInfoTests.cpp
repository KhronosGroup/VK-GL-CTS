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

#include "vktCustomInstancesDevices.hpp"
#include "vkDeviceUtil.hpp"
#include "vktApiToolingInfoTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
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

bool validateToolPurposeFlagBits (const VkToolPurposeFlagsEXT purposes)
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

CustomInstance createCustomInstance (Context& context, bool allowLayers)
{
	std::vector<const char*>		enabledLayers;
	std::vector<std::string>		enabledLayersStr;
	const std::vector<std::string>	enabledExtensions;

	const deUint32					apiVersion	= context.getUsedApiVersion();
	const vk::PlatformInterface&	vkp			= context.getPlatformInterface();

	if (allowLayers)
	{
		enabledLayers		= getValidationLayers(context.getPlatformInterface());
		enabledLayersStr	= std::vector<std::string>(begin(enabledLayers), end(enabledLayers));
	}

	Move<VkInstance> instance = vk::createDefaultInstance(vkp, apiVersion, enabledLayersStr, enabledExtensions, DE_NULL);
	return CustomInstance(context, instance, !enabledLayers.empty(), context.getTestContext().getCommandLine().printValidationErrors());
}

bool checkToolsProperties (Context& context, const std::vector<VkPhysicalDeviceToolPropertiesEXT>& deviceToolPropertiesEXTArray)
{
	tcu::TestLog&	testLog = context.getTestContext().getLog();
	bool			result  = true;

	for (size_t i = 0; i < deviceToolPropertiesEXTArray.size(); ++i)
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
	return result;
}

tcu::TestStatus validateGetter (Context& context)
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

		if (checkToolsProperties(context, deviceToolPropertiesEXTArray) == false)
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus validateInstanceLayers (Context& context)
{
	const std::vector<const char*>	layers			= getValidationLayers(context.getPlatformInterface());
	bool							qualityWarning	= false;

	{
		deUint32			toolCount		= 0;
		CustomInstance		instance		(createCustomInstance(context, true));
		VkPhysicalDevice	physicalDevice	= chooseDevice(instance.getDriver(), instance, context.getTestContext().getCommandLine());

		VK_CHECK(instance.getDriver().getPhysicalDeviceToolPropertiesEXT(physicalDevice, &toolCount, DE_NULL));

		if (toolCount < layers.size())
			qualityWarning = true;

		if (toolCount > 0)
		{
			std::vector<VkPhysicalDeviceToolPropertiesEXT>	deviceToolPropertiesEXTArray(toolCount);

			for (size_t toolNdx = 0; toolNdx < deviceToolPropertiesEXTArray.size(); ++toolNdx)
			{
				deviceToolPropertiesEXTArray[toolNdx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT;
			}

			VK_CHECK(context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(physicalDevice, &toolCount, &deviceToolPropertiesEXTArray[0]));

			if (checkToolsProperties(context, deviceToolPropertiesEXTArray) == false)
				return tcu::TestStatus::fail("Fail");

			for (size_t layerNdx = 0; layerNdx < layers.size(); ++layerNdx)
			{
				deUint32 count = 0u;

				for (deUint32 toolNdx = 0; toolNdx < toolCount; ++toolNdx)
				{
					if (strcmp(layers[layerNdx], deviceToolPropertiesEXTArray[toolNdx].layer) == 0)
						count++;
				}

				if (count != 1)
				{
					qualityWarning = true;
					break;
				}
			}
		}
	}

	{
		deUint32			toolCount		= 0;
		CustomInstance		instance		(createCustomInstance(context, false));
		VkPhysicalDevice	physicalDevice	= chooseDevice(instance.getDriver(), instance, context.getTestContext().getCommandLine());

		VK_CHECK(instance.getDriver().getPhysicalDeviceToolPropertiesEXT(physicalDevice, &toolCount, DE_NULL));

		if (toolCount > 0)
		{
			std::vector<VkPhysicalDeviceToolPropertiesEXT>	deviceToolPropertiesEXTArray(toolCount);

			for (size_t toolNdx = 0; toolNdx < deviceToolPropertiesEXTArray.size(); ++toolNdx)
			{
				deviceToolPropertiesEXTArray[toolNdx].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT;
			}

			VK_CHECK(context.getInstanceInterface().getPhysicalDeviceToolPropertiesEXT(physicalDevice, &toolCount, &deviceToolPropertiesEXTArray[0]));

			if (checkToolsProperties(context, deviceToolPropertiesEXTArray) == false)
				return tcu::TestStatus::fail("Fail");

			for (size_t layerNdx = 0; layerNdx < layers.size(); ++layerNdx)
			{
				for (deUint32 toolNdx = 0; toolNdx < toolCount; ++toolNdx)
				{
					if (strcmp(layers[layerNdx], deviceToolPropertiesEXTArray[toolNdx].layer) == 0)
					{
						qualityWarning	= true;
						layerNdx		= layers.size();
						break;
					}
				}
			}
		}
	}

	if (qualityWarning)
	{
		return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Layers have been reported wrong");
	}
	else
	{
		return tcu::TestStatus::pass("Pass");
	}
}

void createTestCases (tcu::TestCaseGroup* group)
{
	addFunctionCase(group, "validate_getter", "Validate getPhysicalDeviceToolPropertiesEXT", checkSupport, validateGetter);
	addFunctionCase(group, "validate_tools_properties","Validate tools properties",	checkSupport, validateToolsProperties);
	addFunctionCase(group, "validate_instance_layers", "Validate instance layers", checkSupport, validateInstanceLayers);
}

} // anonymous

tcu::TestCaseGroup*	createToolingInfoTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "tooling_info", "VK_EXT_tooling_info tests", createTestCases);
}

} // api
} // vkt
