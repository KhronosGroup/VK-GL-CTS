/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 NVIDIA CORPORATION, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief  Vulkan SC VK_EXT_application_parameters Tests
*//*--------------------------------------------------------------------*/

#include "vktApplicationParametersTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace sc
{

using namespace vk;

enum ApplicationParametersCreateType
{
	INSTANCE = 0,
	DEVICE
};

enum ApplicationParametersTestType
{
	INVALID_VENDOR_ID = 0,
	INVALID_DEVICE_ID,
	INVALID_PARAM_KEY,
	INVALID_PARAM_VALUE,
	VALID
};

struct TestParams
{
	ApplicationParametersCreateType	createType;
	ApplicationParametersTestType	testType;
};

struct TestData
{
	TestParams	testParams;
	uint32_t	vendorId;
	uint32_t	deviceId;
	uint32_t	paramKey;
	uint64_t	paramValue;
	VkResult	expectedResult;
};

void readIDsFromDevice (Context& context, uint32_t& vendorId, uint32_t& deviceId)
{
	const InstanceInterface&	instanceInterface	= context.getInstanceInterface();
	VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	VkPhysicalDeviceProperties	properties;

	instanceInterface.getPhysicalDeviceProperties(physicalDevice, &properties);

	vendorId = properties.vendorID;
	deviceId = properties.deviceID;
}

TestData getDefaultTestData (Context& context, TestParams testParams)
{
	TestData testData{};
	testData.testParams = testParams;

	readIDsFromDevice(context, testData.vendorId, testData.deviceId);

	switch(testParams.testType)
	{
		case INVALID_VENDOR_ID:
			testData.vendorId = 0x01234567;
			testData.expectedResult = VK_ERROR_INCOMPATIBLE_DRIVER;
			break;

		case INVALID_DEVICE_ID:
			testData.deviceId = 0x01234567;
			testData.expectedResult = VK_ERROR_INCOMPATIBLE_DRIVER;
			break;

		case INVALID_PARAM_KEY:
			testData.paramKey = 0x7fffffff;
			testData.expectedResult = VK_ERROR_INITIALIZATION_FAILED;
			break;

		case INVALID_PARAM_VALUE:
		case VALID:
			// There is no default test case for the invalid param value and valid tests.
			// Vendors should provide their own test data for these tests in getTestDataList.
			break;
	}

	if (testParams.createType == DEVICE && testParams.testType != VALID)
		testData.expectedResult = VK_ERROR_INITIALIZATION_FAILED;

	return testData;
}

std::vector<TestData> getTestDataList(Context& context, TestParams testParams)
{
	std::vector<TestData>		testDataList;
	uint32_t					vendorId;
	uint32_t					deviceId;

	readIDsFromDevice(context, vendorId, deviceId);

//#define VENDOR_PARAMS_ADDED 1
#if defined(VENDOR_PARAMS_ADDED)
	uint32_t validVendorID = vendorId;
	uint32_t validDeviceID = deviceId;
	uint32_t validInstanceParamKey = 0;		// TODO: provide valid instance parameter key
	uint64_t invalidInstanceParamValue = 0;	// TODO: provide invalid parameter value for <validInstanceParamKey>
	uint64_t validInstanceParamValue = 0;	// TODO: provide valid parameter value for <validInstanceParamKey>
	uint32_t validDeviceParamKey = 0;		// TODO: provide valid device parameter key
	uint64_t invalidDeviceParamValue = 0;	// TODO: provide invalid parameter value for <validDeviceParamKey>
	uint64_t validDeviceParamValue = 0;		// TODO: provide valid parameter value for <validDeviceParamKey>
#endif

	const std::vector<TestData> vendorTestDataList =
	{
		//	The invalid param value and valid tests need to use vendor-specific application
		//	parameter keys and values. In order to have full test coverage, vendors should
		//	provide their own test data for the invalid param value and valid tests here.
		//
#if defined(VENDOR_PARAMS_ADDED)
		{
			{ INSTANCE, INVALID_PARAM_VALUE },
			validVendorID,
			validDeviceID,
			validInstanceParamKey,
			invalidInstanceParamValue,
			VK_ERROR_INITIALIZATION_FAILED
		},
		{
			{ INSTANCE, VALID },
			validVendorID,
			validDeviceID,
			validInstanceParamKey,
			validInstanceParamValue,
			VK_SUCCESS
		},
		{
			{ DEVICE, INVALID_PARAM_VALUE },
			validVendorID,
			validDeviceID,
			validDeviceParamKey,
			invalidDeviceParamValue,
			VK_ERROR_INITIALIZATION_FAILED
		},
		{
			{ DEVICE, VALID },
			validVendorID,
			validDeviceID,
			validDeviceParamKey,
			validDeviceParamValue,
			VK_SUCCESS
		}
#endif // defined(VENDOR_PARAMS_ADDED)
	};

	if (testParams.testType != INVALID_PARAM_VALUE && testParams.testType != VALID)
		testDataList.push_back(getDefaultTestData(context, testParams));

	for (TestData vendorTestData : vendorTestDataList)
	{
		if (vendorTestData.testParams.createType == testParams.createType &&
			vendorTestData.testParams.testType == testParams.testType &&
			vendorTestData.vendorId == vendorId &&
			(vendorTestData.deviceId == 0 || vendorTestData.deviceId == deviceId))
		{
			testDataList.push_back(vendorTestData);
		}
	}

	return testDataList;
}

void checkSupport (Context& context, TestParams testParams)
{
	const std::vector<VkExtensionProperties> supportedExtensions = enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL);

	if (!isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_application_parameters")))
		TCU_THROW(NotSupportedError, "VK_EXT_application_parameters is not supported");

	const std::vector<TestData> testDataList = getTestDataList(context, testParams);

	if (testDataList.empty())
		TCU_THROW(TestError, "No test data available - please update vendorTestDataList");
}

tcu::TestStatus createDeviceTest (Context& context, TestParams testParams)
{
	tcu::TestLog&								log						= context.getTestContext().getLog();
	const PlatformInterface&					platformInterface		= context.getPlatformInterface();
	const CustomInstance						instance				(createCustomInstanceFromContext(context));
	const InstanceDriver&						instanceDriver			(instance.getDriver());
	const VkPhysicalDevice						physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
	const std::vector<TestData>					testDataList			= getTestDataList(context, testParams);
	const float									queuePriority			= 1.0f;
	VkDeviceObjectReservationCreateInfo			devObjectResCreateInfo	= resetDeviceObjectReservationCreateInfo();
	bool										testPassed				= true;
	const VkPhysicalDeviceVulkanSC10Features	sc10Features			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES,	// sType;
		&devObjectResCreateInfo,									// pNext;
		VK_FALSE													// shaderAtomicInstructions;
	};

	for (TestData testData : testDataList)
	{
		const VkApplicationParametersEXT appParams =
		{
			VK_STRUCTURE_TYPE_APPLICATION_PARAMETERS_EXT,
			&sc10Features,
			testData.vendorId,
			testData.deviceId,
			testData.paramKey,
			testData.paramValue
		};

		const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,										// sType
			DE_NULL,																		// pNext
			(VkDeviceQueueCreateFlags)0u,													// flags
			0,																				// queueFamilyIndex;
			1,																				// queueCount;
			&queuePriority,																	// pQueuePriorities;
		};

		VkDeviceCreateInfo deviceCreateInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,											// sType;
			&appParams,																		// pNext;
			(VkDeviceCreateFlags)0u,														// flags
			1,																				// queueRecordCount;
			&deviceQueueCreateInfo,															// pRequestedQueues;
			0,																				// layerCount;
			DE_NULL,																		// ppEnabledLayerNames;
			0,																				// extensionCount;
			DE_NULL,																		// ppEnabledExtensionNames;
			DE_NULL,																		// pEnabledFeatures;
		};

		log << tcu::TestLog::Message << "Creating device with application parameters: " << appParams << tcu::TestLog::EndMessage;

		VkDevice		device		= (VkDevice)0;
		const VkResult	result		= instanceDriver.createDevice(physicalDevice, &deviceCreateInfo, DE_NULL, &device);

		if (device)
		{
			const DeviceDriver deviceIface(platformInterface, instance, device, context.getUsedApiVersion());
			deviceIface.destroyDevice(device, DE_NULL/*pAllocator*/);
		}

		log << tcu::TestLog::Message << "Device creation returned with " + de::toString(getResultName(result)) +
			" (expecting " + de::toString(getResultName(testData.expectedResult)) + ")" << tcu::TestLog::EndMessage;

		if (result != testData.expectedResult)
			testPassed = false;
	}

	if (testPassed)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus createInstanceTest (Context& context, TestParams testParams)
{
	tcu::TestLog&				log					= context.getTestContext().getLog();
	const PlatformInterface&	platformInterface	= context.getPlatformInterface();
	const std::vector<TestData>	testDataList		= getTestDataList(context, testParams);
	bool						testPassed			= true;

	for (TestData testData : testDataList)
	{
		const VkApplicationParametersEXT appParams
		{
			VK_STRUCTURE_TYPE_APPLICATION_PARAMETERS_EXT,
			DE_NULL,
			testData.vendorId,
			testData.deviceId,
			testData.paramKey,
			testData.paramValue
		};

		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType				sType;
			&appParams,								// const void*					pNext;
			"appName",								// const char*					pAppName;
			0u,										// deUint32						appVersion;
			"engineName",							// const char*					pEngineName;
			0u,										// deUint32						engineVersion;
			context.getUsedApiVersion(),			// deUint32						apiVersion;
		};

		const VkInstanceCreateInfo instanceCreateInfo =
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,								// const void*					pNext;
			(VkInstanceCreateFlags)0u,				// VkInstanceCreateFlags		flags;
			&appInfo,								// const VkApplicationInfo*		pAppInfo;
			0u,										// deUint32						layerCount;
			DE_NULL,								// const char*const*			ppEnabledLayernames;
			0u,										// deUint32						extensionCount;
			DE_NULL,								// const char*const*			ppEnabledExtensionNames;
		};

		log << tcu::TestLog::Message << "Creating instance with application parameters: " << appParams << tcu::TestLog::EndMessage;

		VkInstance				  instance			= (VkInstance)0;
		const VkResult			  result			= platformInterface.createInstance(&instanceCreateInfo, DE_NULL/*pAllocator*/, &instance);

		if (instance)
		{
			const InstanceDriver instanceIface(platformInterface, instance);
			instanceIface.destroyInstance(instance, DE_NULL/*pAllocator*/);
		}

		log << tcu::TestLog::Message << "Instance creation returned with " + de::toString(getResultName(result)) +
			" (expecting " + de::toString(getResultName(testData.expectedResult)) + ")" << tcu::TestLog::EndMessage;

		if (result != testData.expectedResult)
			testPassed = false;
	}

	if (testPassed)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

tcu::TestCaseGroup*	createApplicationParametersTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "application_parameters", "Tests VK_EXT_application_parameters"));

	const struct
	{
		ApplicationParametersCreateType	createType;
		const char*						name;
	} groups[2] =
	{
		{ INSTANCE,							"create_instance"	},
		{ DEVICE,							"create_device"		}
	};

	const struct
	{
		ApplicationParametersTestType		testType;
		const char*							name;
	} tests[5] =
	{
		{ INVALID_VENDOR_ID,				"invalid_vendor_id"			},
		{ INVALID_DEVICE_ID,				"invalid_device_id"			},
		{ INVALID_PARAM_KEY,				"invalid_parameter_key"		},
		{ INVALID_PARAM_VALUE,				"invalid_parameter_value"	},
		{ VALID,							"valid"						}
	};

	for (int groupIdx = 0; groupIdx < DE_LENGTH_OF_ARRAY(groups); ++groupIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> createGroup(new tcu::TestCaseGroup(testCtx, groups[groupIdx].name, ""));

		for (int testIdx = 0; testIdx < DE_LENGTH_OF_ARRAY(tests); ++testIdx)
		{
			TestParams testParams = { groups[groupIdx].createType, tests[testIdx].testType };

			if (testParams.createType == INSTANCE)
				addFunctionCase(createGroup.get(), tests[testIdx].name, "", checkSupport, createInstanceTest, testParams);
			else
				addFunctionCase(createGroup.get(), tests[testIdx].name, "", checkSupport, createDeviceTest, testParams);
		}

		group->addChild(createGroup.release());
	}

	return group.release();
}

} // sc

} // vkt
