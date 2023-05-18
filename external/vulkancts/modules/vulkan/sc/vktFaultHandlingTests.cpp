/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief  Vulkan SC fault handling tests
*//*--------------------------------------------------------------------*/

#include "vktFaultHandlingTests.hpp"

#include <set>
#include <vector>
#include <string>

#include "vktTestCaseUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace sc
{

using namespace vk;

namespace
{

enum FHFaultValue
{
	FHF_UNUSED = 0,
	FHF_NULL,
	FHF_ARRAY
};

struct TestParams
{
	VkFaultQueryBehavior	queryBehaviour;
	FHFaultValue			faultValue;
};

tcu::TestStatus testGetFaultData (Context& context, TestParams testParams)
{
	const DeviceInterface&		vk							= context.getDeviceInterface();
	const VkDevice				device						= context.getDevice();

	deUint32					maxQueryFaultCount			= context.getDeviceVulkanSC10Properties().maxQueryFaultCount;

	VkBool32					unrecordedFaults			= VK_TRUE;
	deUint32					faultCount					= maxQueryFaultCount;
	std::vector<VkFaultData>	faults;
	for (deUint32 i = 0; i < maxQueryFaultCount; ++i)
	{
		faults.push_back
		(
			{
				VK_STRUCTURE_TYPE_FAULT_DATA,	// VkStructureType	sType;
				DE_NULL,						// void*			pNext;
				VK_FAULT_LEVEL_UNASSIGNED,		// VkFaultLevel		faultLevel;
				VK_FAULT_TYPE_UNASSIGNED,		// VkFaultType		faultType;
			}
		);
	}
	bool						isOK						= true;
	bool						faultsModified				= false;
	VkResult					result;

	switch (testParams.faultValue)
	{
	case FHF_NULL:
		result = vk.getFaultData(device, testParams.queryBehaviour, &unrecordedFaults, &faultCount, DE_NULL);

		if (result != VK_SUCCESS)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Result is not VK_SUCCESS" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		if (unrecordedFaults != VK_FALSE)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "unrecordedFaults is not VK_FALSE" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		if (faultCount != 0u)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "faultCount is not 0" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		break;
	case FHF_ARRAY:
		result = vk.getFaultData(device, testParams.queryBehaviour, &unrecordedFaults, &faultCount, faults.data());

		if (result != VK_SUCCESS)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Result is not VK_SUCCESS" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		if (unrecordedFaults != VK_FALSE)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "unrecordedFaults is not VK_FALSE" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		if (faultCount != 0u)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "faultCount is not 0" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		for (deUint32 i = 0; i < maxQueryFaultCount; ++i)
			if (faults[i].faultLevel != VK_FAULT_LEVEL_UNASSIGNED || faults[i].faultType != VK_FAULT_TYPE_UNASSIGNED)
				faultsModified = true;
		if (faultsModified)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "pFaults have been modified" << tcu::TestLog::EndMessage;
			isOK = false;
		}
		break;
	default:
		TCU_THROW(InternalError, "Unrecognized fault type");
	}

	return isOK ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail");
}

VKAPI_ATTR void	VKAPI_CALL testFaultCallback (VkBool32				incompleteFaultData,
											  deUint32				faultCount,
											  const VkFaultData*	pFaultData)
{
	DE_UNREF(incompleteFaultData);
	DE_UNREF(faultCount);
	DE_UNREF(pFaultData);
}

struct FaultCallbackInfoTestParams
{
	bool allocateFaultData;
};

tcu::TestStatus testCreateDeviceWithFaultCallbackInfo (Context& context, FaultCallbackInfoTestParams testParams)
{
	const CustomInstance				instance				(createCustomInstanceFromContext(context));
	const InstanceDriver&				instanceDriver			(instance.getDriver());
	const VkPhysicalDevice				physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());

	void*								pNext					= DE_NULL;

	VkDeviceObjectReservationCreateInfo	memReservationInfo		= context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
	memReservationInfo.pNext									= pNext;
	pNext														= &memReservationInfo;

	VkPhysicalDeviceVulkanSC10Features	sc10Features			= createDefaultSC10Features();
	sc10Features.pNext											= pNext;
	pNext														= &sc10Features;

	// create VkFaultCallbackInfo
	deUint32							maxQueryFaultCount		= context.getDeviceVulkanSC10Properties().maxQueryFaultCount;
	std::vector<VkFaultData>			faults;

	if (testParams.allocateFaultData)
	{
		for (deUint32 i = 0; i < maxQueryFaultCount; ++i)
		{
			faults.push_back
			(
				{
					VK_STRUCTURE_TYPE_FAULT_DATA,	// VkStructureType	sType;
					DE_NULL,						// void*			pNext;
					VK_FAULT_LEVEL_UNASSIGNED,		// VkFaultLevel		faultLevel;
					VK_FAULT_TYPE_UNASSIGNED,		// VkFaultType		faultType;
				}
			);
		}
	}

	VkFaultCallbackInfo					faultCallBackInfo		=
	{
		VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO,					//	VkStructureType				sType;
		DE_NULL,												//	void*						pNext;
		deUint32(faults.size()),								//	uint32_t					faultCount;
		testParams.allocateFaultData ? faults.data() : nullptr,	//	VkFaultData*				pFaults;
		testFaultCallback										//	PFN_vkFaultCallbackFunction	pfnFaultCallback;
	};
	faultCallBackInfo.pNext										= pNext;
	pNext														= &faultCallBackInfo;

	// create VkDeviceCreateInfo

	const float							queuePriority			= 1.0f;

	const VkDeviceQueueCreateInfo		deviceQueueCI			=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,				// sType
		DE_NULL,												// pNext
		(VkDeviceQueueCreateFlags)0u,							// flags
		0,														//queueFamilyIndex;
		1,														//queueCount;
		&queuePriority,											//pQueuePriorities;
	};

	VkDeviceCreateInfo					deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,					// sType;
		pNext,													// pNext;
		(VkDeviceCreateFlags)0u,								// flags
		1,														// queueRecordCount;
		&deviceQueueCI,											// pRequestedQueues;
		0,														// layerCount;
		DE_NULL,												// ppEnabledLayerNames;
		0,														// extensionCount;
		DE_NULL,												// ppEnabledExtensionNames;
		DE_NULL,												// pEnabledFeatures;
	};

	Move<VkDevice> resultingDevice = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), instance, instanceDriver, physicalDevice, &deviceCreateInfo);

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createFaultHandlingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "fault_handling", "Tests verifying Vulkan SC fault handling"));

	// add tests for vkGetFaultData function
	{
		de::MovePtr<tcu::TestCaseGroup>	getFaultDataGroup(new tcu::TestCaseGroup(testCtx, "get_fault_data", "Testing vkGetFaultData results"));

		const struct
		{
			VkFaultQueryBehavior						queryBehaviour;
			const char*									name;
		} behaviours[] =
		{
			{ VK_FAULT_QUERY_BEHAVIOR_GET_AND_CLEAR_ALL_FAULTS,	"get_and_clear_all_faults"	},
		};

		const struct
		{
			FHFaultValue								faultValue;
			const char*									name;
		} faults[] =
		{
			{ FHF_NULL,									"null"	},
			{ FHF_ARRAY,								"array"	},
		};

		for (int behaviourIdx = 0; behaviourIdx < DE_LENGTH_OF_ARRAY(behaviours); ++behaviourIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> behaviourGroup(new tcu::TestCaseGroup(testCtx, behaviours[behaviourIdx].name, ""));

			for (int faultIdx = 0; faultIdx < DE_LENGTH_OF_ARRAY(faults); ++faultIdx)
			{
				TestParams testParams{ behaviours[behaviourIdx].queryBehaviour, faults[faultIdx].faultValue };

				addFunctionCase(behaviourGroup.get(), faults[faultIdx].name, "", testGetFaultData, testParams);
			}
			getFaultDataGroup->addChild(behaviourGroup.release());
		}
		group->addChild(getFaultDataGroup.release());
	}

	// add tests for VkFaultCallbackInfo
	{
		de::MovePtr<tcu::TestCaseGroup>	faultCallbackInfoGroup(new tcu::TestCaseGroup(testCtx, "fault_callback_info", "Testing vkGetFaultData results"));

		{
			FaultCallbackInfoTestParams testParams { true };
			addFunctionCase(faultCallbackInfoGroup.get(), "create_device_with_callback_with_fault_data", "", testCreateDeviceWithFaultCallbackInfo, testParams);
		}
		{
			FaultCallbackInfoTestParams testParams { false };
			addFunctionCase(faultCallbackInfoGroup.get(), "create_device_with_callback_without_fault_data", "", testCreateDeviceWithFaultCallbackInfo, testParams);
		}
		group->addChild(faultCallbackInfoGroup.release());
	}

	return group.release();
}

}	// sc

}	// vkt
