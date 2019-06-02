/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
*
* Copyright (c) 2019 Google Inc.
* Copyright (c) 2019 Khronos Group
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
* \brief API Version Check test - prints out version info
*//*--------------------------------------------------------------------*/

#include <iostream>
#include <typeinfo>

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"

#include "vkApiVersion.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"

#include "vktApiVersionCheck.hpp"
#include "vktTestCase.hpp"

#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include "deString.h"
#include "deStringUtil.hpp"

#include <map>
#include <vector>

using namespace vk;
using namespace std;

namespace vkt
{

namespace api
{

namespace
{

#include "vkExtensionFunctions.inl"
#include "vkCoreFunctionalities.inl"

class APIVersionTestInstance : public TestInstance
{
public:
								APIVersionTestInstance	(Context&				ctx)
									: TestInstance	(ctx)
	{}
	virtual tcu::TestStatus		iterate					(void)
	{
		tcu::TestLog&			log				= m_context.getTestContext().getLog();
		const vk::ApiVersion	instanceVersion	= vk::unpackVersion(m_context.getAvailableInstanceVersion());
		const vk::ApiVersion	deviceVersion	= vk::unpackVersion(m_context.getDeviceVersion());
		const vk::ApiVersion	usedApiVersion	= vk::unpackVersion(m_context.getUsedApiVersion());

		log << tcu::TestLog::Message << "availableInstanceVersion: " << instanceVersion << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "deviceVersion: " << deviceVersion << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "usedApiVersion: " << usedApiVersion << tcu::TestLog::EndMessage;
		const ::std::string		result			= de::toString(usedApiVersion.majorNum) + ::std::string(".") + de::toString(usedApiVersion.minorNum) + ::std::string(".") + de::toString(usedApiVersion.patchNum);
		return tcu::TestStatus::pass(result);
	}
};

class APIVersionTestCase : public TestCase
{
public:
							APIVersionTestCase	(tcu::TestContext&		testCtx)
								: TestCase	(testCtx, "version", "Prints out API info.")
	{}

	virtual					~APIVersionTestCase	(void)
	{}
	virtual TestInstance*	createInstance		(Context&				ctx) const
	{
		return new APIVersionTestInstance(ctx);
	}

private:
};

class APIEntryPointsTestInstance : public TestInstance
{
public:
								APIEntryPointsTestInstance	(Context&				ctx)
									: TestInstance	(ctx)
	{

	}
	virtual tcu::TestStatus		iterate						(void)
	{
		tcu::TestLog&						log				= m_context.getTestContext().getLog();
		const deUint32						apiVersion		= m_context.getUsedApiVersion();
		const vk::Platform&					platform		= m_context.getTestContext().getPlatform().getVulkanPlatform();
		de::MovePtr<vk::Library>			vkLibrary		= de::MovePtr<vk::Library>(platform.createLibrary());
		const tcu::FunctionLibrary&			funcLibrary		= vkLibrary->getFunctionLibrary();

		deUint32							failsQuantity	= 0u;

		// Tests with default instance and device without extensions
		{
			instance			= createDefaultInstance(m_context.getPlatformInterface(), m_context.getUsedApiVersion());
			device				= createTestDevice(m_context.getPlatformInterface(), m_context.getInstanceInterface(), m_context.getPhysicalDevice());
			getInstanceProcAddr	= reinterpret_cast<GetInstanceProcAddrFunc>(funcLibrary.getFunction("vkGetInstanceProcAddr"));
			getDeviceProcAddr	= reinterpret_cast<GetDeviceProcAddrFunc>(getInstanceProcAddr(*instance, "vkGetDeviceProcAddr"));

			// Check entry points of core functions
			{
				ApisMap							functions			= ApisMap();
				initApisMap(functions);
				ApisMap::const_iterator			lastGoodVersion		= functions.begin();
				const ApisMap::const_iterator	versionsEnd			= functions.end();
				for (ApisMap::const_iterator it = lastGoodVersion; it != versionsEnd; ++it)
				{
					if (it->first <= m_context.getUsedApiVersion())
						lastGoodVersion = it;
				}

				log << tcu::TestLog::Message << "Regular check - tries to get core functions from proper vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char* const				regularResult		= regularCheck(log, failsQuantity, lastGoodVersion->second) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << regularResult << tcu::TestLog::EndMessage;

				log << tcu::TestLog::Message << "Cross check - tries to get core functions from improper vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char* const				mixupResult			= mixupAddressProcCheck(log, failsQuantity, lastGoodVersion->second) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << mixupResult << tcu::TestLog::EndMessage;
			}

			// Check function entry points of disabled extesions
			{
				FunctionInfosList				extFunctions		= FunctionInfosList();
				extFunctions.push_back(FunctionInfo("vkTrimCommandPoolKHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkCmdPushDescriptorSetKHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkCreateSamplerYcbcrConversionKHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkGetSwapchainStatusKHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkCreateSwapchainKHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkGetImageSparseMemoryRequirements2KHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkBindBufferMemory2KHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkImportFenceWin32HandleKHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkGetBufferMemoryRequirements2KHR", FUNCTIONORIGIN_DEVICE));
				extFunctions.push_back(FunctionInfo("vkGetImageMemoryRequirements2KHR", FUNCTIONORIGIN_DEVICE));

				log << tcu::TestLog::Message << "Disabled extensions check - tries to get functions of disabled extensions from proper vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char * const				result				= specialCasesCheck(log, failsQuantity, extFunctions) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
			}

			// Check special cases
			{
				FunctionInfosList				dummyFunctions		= FunctionInfosList();
				for (deUint32 i = 0; i <= FUNCTIONORIGIN_DEVICE; ++i)
				{
					const FunctionOrigin origin = static_cast<FunctionOrigin>(i);
					dummyFunctions.push_back(FunctionInfo("vkSomeName", origin));
					dummyFunctions.push_back(FunctionInfo("vkNonexistingKHR", origin));
					dummyFunctions.push_back(FunctionInfo("", origin));
				}

				log << tcu::TestLog::Message << "Special check - tries to get some dummy functions from various vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char * const				result				= specialCasesCheck(log, failsQuantity, dummyFunctions) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
			}
		}

		// Tests with instance and device with extensions
		{
			instance			= createInstanceWithExtensions(m_context.getPlatformInterface(), m_context.getUsedApiVersion(), getSupportedInstanceExtensions(apiVersion));
			device				= createTestDevice(m_context.getPlatformInterface(), m_context.getInstanceInterface(), m_context.getPhysicalDevice(), getSupportedDeviceExtensions(apiVersion));
			getInstanceProcAddr	= reinterpret_cast<GetInstanceProcAddrFunc>(funcLibrary.getFunction("vkGetInstanceProcAddr"));
			getDeviceProcAddr	= reinterpret_cast<GetDeviceProcAddrFunc>(getInstanceProcAddr(*instance, "vkGetDeviceProcAddr"));

			// Check function entry points of enabled extensions
			{
				vector<FunctionInfo>	extFunctions;

				// Add supported instance extension functions
				for (size_t instanceExtNdx = 0; instanceExtNdx < DE_LENGTH_OF_ARRAY(instanceExtensionNames); instanceExtNdx++)
				{
					vector<const char*> instanceExtFunctions;

					if (isSupportedInstanceExt(instanceExtensionNames[instanceExtNdx], apiVersion))
						getInstanceExtensionFunctions(apiVersion, instanceExtensionNames[instanceExtNdx], instanceExtFunctions);

					for (size_t instanceFuncNdx = 0; instanceFuncNdx < instanceExtFunctions.size(); instanceFuncNdx++)
						extFunctions.push_back(FunctionInfo(instanceExtFunctions[instanceFuncNdx], FUNCTIONORIGIN_INSTANCE));
				}

				// Add supported device extension functions
				for (size_t deviceExtNdx = 0; deviceExtNdx < DE_LENGTH_OF_ARRAY(deviceExtensionNames); deviceExtNdx++)
				{
					vector<const char*> deviceExtFunctions;

					if (isSupportedDeviceExt(deviceExtensionNames[deviceExtNdx], apiVersion))
						getDeviceExtensionFunctions(apiVersion, deviceExtensionNames[deviceExtNdx], deviceExtFunctions);

					for (size_t deviceFuncNdx = 0; deviceFuncNdx < deviceExtFunctions.size(); deviceFuncNdx++)
						extFunctions.push_back(FunctionInfo(deviceExtFunctions[deviceFuncNdx], FUNCTIONORIGIN_DEVICE));
				}

				log << tcu::TestLog::Message << "Enabled extensions check - tries to get functions of supported extensions from proper vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char * const		result = regularCheck(log, failsQuantity, extFunctions) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
			}
		}

		if (failsQuantity > 0u)
			return tcu::TestStatus::fail("Fail");
		else
			return tcu::TestStatus::pass("Pass");
	}

private:

	GetDeviceProcAddrFunc	getDeviceProcAddr;
	GetInstanceProcAddrFunc	getInstanceProcAddr;
	Move<VkInstance>		instance;
	Move<VkDevice>			device;

	deUint32 findQueueFamilyIndex(const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
	{
		deUint32								numQueues = 0;
		vkInstance.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, DE_NULL);
		if (numQueues > 0)
		{
			vector<VkQueueFamilyProperties>		properties(numQueues);
			vkInstance.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, &properties[0]);
			if (numQueues != static_cast<deUint32>(properties.size()))
				TCU_FAIL("Returned queue family count changes between queries.");
			for (deUint32 queueNdx = 0u; queueNdx < numQueues; queueNdx++)
				if ((properties[queueNdx].queueFlags & requiredCaps) == requiredCaps)
					return queueNdx;
		}
		TCU_FAIL("Returned queue family count was 0.");
		return 0u;
	}

	vector<string> filterMultiAuthorExtensions (vector<VkExtensionProperties> extProperties)
	{
		vector<string>	multiAuthorExtensions;
		const char*		extensionGroups[] =
		{
			"VK_KHR_",
			"VK_EXT_"
		};

		for (size_t extNdx = 0; extNdx < extProperties.size(); extNdx++)
		{
			for (int extGroupNdx = 0; extGroupNdx < DE_LENGTH_OF_ARRAY(extensionGroups); extGroupNdx++)
			{
				if (deStringBeginsWith(extProperties[extNdx].extensionName, extensionGroups[extGroupNdx]))
					multiAuthorExtensions.push_back(extProperties[extNdx].extensionName);
			}
		}

		return multiAuthorExtensions;
	}

	vector<string> getSupportedInstanceExtensions (const deUint32 apiVersion)
	{
		vector<VkExtensionProperties>	enumeratedExtensions (enumerateInstanceExtensionProperties(m_context.getPlatformInterface(), DE_NULL));
		vector<VkExtensionProperties>	supportedExtensions;

		for (size_t extNdx = 0; extNdx < enumeratedExtensions.size(); extNdx++)
		{
			if (!isCoreInstanceExtension(apiVersion, enumeratedExtensions[extNdx].extensionName))
				supportedExtensions.push_back(enumeratedExtensions[extNdx]);
		}

		return filterMultiAuthorExtensions(supportedExtensions);
	}

	vector<string> getSupportedDeviceExtensions (const deUint32 apiVersion)
	{
		vector<VkExtensionProperties>	enumeratedExtensions (enumerateDeviceExtensionProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), DE_NULL));
		vector<VkExtensionProperties>	supportedExtensions;

		for (size_t extNdx = 0; extNdx < enumeratedExtensions.size(); extNdx++)
		{
			if (!isCoreDeviceExtension(apiVersion, enumeratedExtensions[extNdx].extensionName))
				supportedExtensions.push_back(enumeratedExtensions[extNdx]);
		}

		return filterMultiAuthorExtensions(supportedExtensions);
	}

	Move<VkDevice> createTestDevice (const PlatformInterface& vkp, const InstanceInterface& vki, VkPhysicalDevice physicalDevice, vector<string> extensions = vector<string>())
	{
		vector<const char*>			extensionPtrs;
		const float					queuePriority	= 1.0f;
		const deUint32				queueIndex		= findQueueFamilyIndex(vki, physicalDevice, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

		for (size_t i = 0; i < extensions.size(); i++)
			extensionPtrs.push_back(extensions[i].c_str());

		VkDeviceQueueCreateInfo		queueInfo		= {
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
			static_cast<VkDeviceQueueCreateFlags>(0u),
			queueIndex,
			1u,
			&queuePriority
		};
		VkDeviceCreateInfo			deviceInfo		= {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			DE_NULL,
			static_cast<VkDeviceCreateFlags>(0u),
			1u,
			&queueInfo,
			0u,
			DE_NULL,
			(deUint32)extensions.size(),
			extensions.size() ? &extensionPtrs[0] : DE_NULL,
			DE_NULL,
		};

		return vk::createDevice(vkp, *instance, vki, physicalDevice, &deviceInfo);
	}

	void reportFail (tcu::TestLog& log, const char* const functionName, const char* const firstParamName, const char* const secondParamName, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		log << tcu::TestLog::Message
			<< "[" << failsQuantity << "] " << functionName << '(' << firstParamName << ", \"" << secondParamName << "\") "
			<< "returned " << (shouldBeNonNull ? "nullptr" : "non-null") << ". Should return " << (shouldBeNonNull ? "valid function address." : "nullptr.")
			<< tcu::TestLog::EndMessage;
		++failsQuantity;
	}

	void checkPlatformFunction (tcu::TestLog& log, const char* const name, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		if ((getInstanceProcAddr(DE_NULL, name) == DE_NULL) == shouldBeNonNull)
			reportFail(log, "vkGetInstanceProcAddr", "DE_NULL", name, shouldBeNonNull, failsQuantity);
	}

	void checkInstanceFunction (tcu::TestLog& log, const char* const name, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		if ((getInstanceProcAddr(*instance, name) == DE_NULL) == shouldBeNonNull)
			reportFail(log, "vkGetInstanceProcAddr", "instance", name, shouldBeNonNull, failsQuantity);
	}

	void checkDeviceFunction (tcu::TestLog& log, const char* const name, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		if ((getDeviceProcAddr(*device, name) == DE_NULL) == shouldBeNonNull)
			reportFail(log, "vkGetDeviceProcAddr", "device", name, shouldBeNonNull, failsQuantity);
	}

	deBool isSupportedInstanceExt (const string extName, const deUint32 apiVersion)
	{
		const vector<string> supportedInstanceExtensions (getSupportedInstanceExtensions(apiVersion));

		return de::contains(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), extName);
	}

	deBool isSupportedDeviceExt (const string extName, const deUint32 apiVersion)
	{
		const vector<string> supportedDeviceExtensions (getSupportedDeviceExtensions(apiVersion));

		return de::contains(supportedDeviceExtensions.begin(), supportedDeviceExtensions.end(), extName);
	}

	deBool mixupAddressProcCheck (tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
	{
		const deUint32 startingQuantity = failsQuantity;
		for (deUint32 ndx = 0u; ndx < testsArr.size(); ++ndx)
		{
			if (deStringEqual(testsArr[ndx].first, "vkGetInstanceProcAddr") || deStringEqual(testsArr[ndx].first, "vkEnumerateInstanceVersion"))
				continue;

			const char*	   functionName = testsArr[ndx].first;
			const deUint32 functionType = testsArr[ndx].second;
			if (functionType == FUNCTIONORIGIN_INSTANCE)
			{
				checkPlatformFunction(log, functionName, DE_FALSE, failsQuantity);
				checkDeviceFunction(log, functionName, DE_FALSE, failsQuantity);
			}
			else if (functionType == FUNCTIONORIGIN_DEVICE)
				checkPlatformFunction(log, functionName, DE_FALSE, failsQuantity);
		}
		return startingQuantity == failsQuantity;
	}

	deBool specialCasesCheck (tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
	{
		const deUint32 startingQuantity = failsQuantity;
		for (deUint32 ndx = 0u; ndx < testsArr.size(); ++ndx)
		{
			const deUint32 functionType = testsArr[ndx].second;
			if (functionType == FUNCTIONORIGIN_PLATFORM)
				checkPlatformFunction(log, testsArr[ndx].first, DE_FALSE, failsQuantity);
			else if (functionType == FUNCTIONORIGIN_INSTANCE)
				checkInstanceFunction(log, testsArr[ndx].first, DE_FALSE, failsQuantity);
			else if (functionType == FUNCTIONORIGIN_DEVICE)
				checkDeviceFunction(log, testsArr[ndx].first, DE_FALSE, failsQuantity);
		}
		return startingQuantity == failsQuantity;
	}

	deBool regularCheck (tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
	{
		const deUint32 startingQuantity = failsQuantity;
		for (deUint32 ndx = 0u; ndx < testsArr.size(); ++ndx)
		{
			if (deStringEqual(testsArr[ndx].first, "vkGetInstanceProcAddr") || deStringEqual(testsArr[ndx].first, "vkEnumerateInstanceVersion"))
				continue;

			const deUint32 functionType	= testsArr[ndx].second;
			if (functionType == FUNCTIONORIGIN_PLATFORM)
				checkPlatformFunction(log, testsArr[ndx].first, DE_TRUE, failsQuantity);
			else if (functionType == FUNCTIONORIGIN_INSTANCE)
				checkInstanceFunction(log, testsArr[ndx].first, DE_TRUE, failsQuantity);
			else if (functionType == FUNCTIONORIGIN_DEVICE)
				checkDeviceFunction(log, testsArr[ndx].first, DE_TRUE, failsQuantity);
		}
		return startingQuantity == failsQuantity;
	}
};

class APIEntryPointsTestCase : public TestCase
{
public:
							APIEntryPointsTestCase			(tcu::TestContext&		testCtx)
								: TestCase	(testCtx, "entry_points", "Prints out API info.")
	{}

	virtual					~APIEntryPointsTestCase			(void)
	{}
	virtual TestInstance*	createInstance					(Context&				ctx) const
	{
		return new APIEntryPointsTestInstance(ctx);
	}

private:
};

} // anonymous

tcu::TestCaseGroup*			createVersionSanityCheckTests	(tcu::TestContext & testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	versionTests	(new tcu::TestCaseGroup(testCtx, "version_check", "API Version Tests"));
	versionTests->addChild(new APIVersionTestCase(testCtx));
	versionTests->addChild(new APIEntryPointsTestCase(testCtx));
	return versionTests.release();
}

} // api

} // vkt
