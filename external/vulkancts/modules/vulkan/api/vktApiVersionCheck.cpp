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
#include "tcuCommandLine.hpp"

#include "vkApiVersion.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "vktApiVersionCheck.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

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
		tcu::TestLog&			log						= m_context.getTestContext().getLog();
		const vk::ApiVersion	maxVulkanVersion		= vk::unpackVersion(m_context.getMaximumFrameworkVulkanVersion());
		const vk::ApiVersion	instanceVersion			= vk::unpackVersion(m_context.getAvailableInstanceVersion());
		const ::std::string		instanceVersionString	= de::toString(instanceVersion.majorNum) + ::std::string(".") + de::toString(instanceVersion.minorNum) + ::std::string(".") + de::toString(instanceVersion.patchNum);
		const vk::ApiVersion	deviceVersion			= vk::unpackVersion(m_context.getDeviceVersion());
		const ::std::string		deviceVersionString		= de::toString(deviceVersion.majorNum) + ::std::string(".") + de::toString(deviceVersion.minorNum) + ::std::string(".") + de::toString(deviceVersion.patchNum);
		const vk::ApiVersion	usedApiVersion			= vk::unpackVersion(m_context.getUsedApiVersion());
		const ::std::string		usedApiVersionString	= de::toString(usedApiVersion.majorNum) + ::std::string(".") + de::toString(usedApiVersion.minorNum) + ::std::string(".") + de::toString(usedApiVersion.patchNum);

		log << tcu::TestLog::Message << "availableInstanceVersion: " << instanceVersion << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "deviceVersion: " << deviceVersion << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "usedApiVersion: " << usedApiVersion << tcu::TestLog::EndMessage;

		if (deviceVersion.majorNum > maxVulkanVersion.majorNum || deviceVersion.minorNum > maxVulkanVersion.minorNum)
			return tcu::TestStatus::fail(de::toString("This version of CTS does not support Vulkan device version ") + deviceVersionString);
		else
			return tcu::TestStatus::pass(usedApiVersionString);
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
	struct APIContext
	{
		VkInstance				instance;
		VkDevice				device;
		GetInstanceProcAddrFunc	getInstanceProcAddr;
		GetDeviceProcAddrFunc	getDeviceProcAddr;
	};

								APIEntryPointsTestInstance	(Context&				ctx)
									: TestInstance	(ctx)
	{
	}

	virtual tcu::TestStatus		iterate						(void)
	{
		tcu::TestLog&						log					= m_context.getTestContext().getLog();
		const deUint32						instanceApiVersion	= m_context.getAvailableInstanceVersion();
		const deUint32						deviceApiVersion	= m_context.getUsedApiVersion();
		const vk::Platform&					platform			= m_context.getTestContext().getPlatform().getVulkanPlatform();
#ifdef DE_PLATFORM_USE_LIBRARY_TYPE
		de::MovePtr<vk::Library>			vkLibrary			= de::MovePtr<vk::Library>(platform.createLibrary(vk::Platform::LibraryType::LIBRARY_TYPE_VULKAN, m_context.getTestContext().getCommandLine().getVkLibraryPath()));
#else
		de::MovePtr<vk::Library>			vkLibrary			= de::MovePtr<vk::Library>(platform.createLibrary(m_context.getTestContext().getCommandLine().getVkLibraryPath()));
#endif
		const tcu::FunctionLibrary&			funcLibrary			= vkLibrary->getFunctionLibrary();
		deUint32							failsQuantity		= 0u;

		// Tests with default instance and device without extensions
		{
			CustomInstance			instance			= createCustomInstanceFromContext(m_context, DE_NULL, false);
			Move<VkDevice>			device				= createTestDevice(m_context, instance, vector<string>(), false);
			GetInstanceProcAddrFunc	getInstanceProcAddr	= reinterpret_cast<GetInstanceProcAddrFunc>(funcLibrary.getFunction("vkGetInstanceProcAddr"));
			GetDeviceProcAddrFunc	getDeviceProcAddr	= reinterpret_cast<GetDeviceProcAddrFunc>(getInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
			APIContext				ctx					= { instance, *device, getInstanceProcAddr, getDeviceProcAddr };

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
				const char* const				regularResult		= regularCheck(ctx, log, failsQuantity, lastGoodVersion->second) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << regularResult << tcu::TestLog::EndMessage;

				log << tcu::TestLog::Message << "Cross check - tries to get core functions from improper vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char* const				mixupResult			= mixupAddressProcCheck(ctx, log, failsQuantity, lastGoodVersion->second) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << mixupResult << tcu::TestLog::EndMessage;
			}

			// Check function entry points of disabled extensions
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
				const char * const				result					= specialCasesCheck(ctx, log, failsQuantity, extFunctions) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
			}

			// Check special cases
			{
				FunctionInfosList				nonexistingFunctions	= FunctionInfosList();
				for (deUint32 i = 0; i <= FUNCTIONORIGIN_DEVICE; ++i)
				{
					const FunctionOrigin origin = static_cast<FunctionOrigin>(i);
					nonexistingFunctions.push_back(FunctionInfo("vkSomeName", origin));
					nonexistingFunctions.push_back(FunctionInfo("vkNonexistingKHR", origin));
					nonexistingFunctions.push_back(FunctionInfo("", origin));
				}

				log << tcu::TestLog::Message << "Special check - tries to get some nonexisting functions from various vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char * const				result				= specialCasesCheck(ctx, log, failsQuantity, nonexistingFunctions) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
			}
		}

		// Tests with instance and device with extensions
		{
			CustomInstance			instance			= createCustomInstanceWithExtensions(m_context, getSupportedInstanceExtensions(instanceApiVersion), DE_NULL, false);
			Move<VkDevice>			device				= createTestDevice(m_context, instance, getSupportedDeviceExtensions(deviceApiVersion), false);
			GetInstanceProcAddrFunc	getInstanceProcAddr	= reinterpret_cast<GetInstanceProcAddrFunc>(funcLibrary.getFunction("vkGetInstanceProcAddr"));
			GetDeviceProcAddrFunc	getDeviceProcAddr	= reinterpret_cast<GetDeviceProcAddrFunc>(getInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
			APIContext				ctx					= { instance, *device, getInstanceProcAddr, getDeviceProcAddr };

			// Check function entry points of enabled extensions
			{
				vector<FunctionInfo>	extFunctions;

				// Add supported instance extension functions
				for (size_t instanceExtNdx = 0; instanceExtNdx < DE_LENGTH_OF_ARRAY(instanceExtensionNames); instanceExtNdx++)
				{
					vector<const char*> instanceExtFunctions;
					vector<const char*> deviceExtFunctions;

					if (isSupportedInstanceExt(instanceExtensionNames[instanceExtNdx], instanceApiVersion))
					{
						getInstanceExtensionFunctions(instanceApiVersion, instanceExtensionNames[instanceExtNdx], instanceExtFunctions);
					}
					if (isSupportedInstanceExt(instanceExtensionNames[instanceExtNdx], deviceApiVersion))
					{
						getDeviceExtensionFunctions(deviceApiVersion, instanceExtensionNames[instanceExtNdx], deviceExtFunctions);
					}

					for (size_t instanceFuncNdx = 0; instanceFuncNdx < instanceExtFunctions.size(); instanceFuncNdx++)
						extFunctions.push_back(FunctionInfo(instanceExtFunctions[instanceFuncNdx], FUNCTIONORIGIN_INSTANCE));

					for (size_t deviceFuncNdx = 0; deviceFuncNdx < deviceExtFunctions.size(); deviceFuncNdx++)
						extFunctions.push_back(FunctionInfo(deviceExtFunctions[deviceFuncNdx], FUNCTIONORIGIN_DEVICE));
				}

				// Add supported device extension functions
				for (size_t deviceExtNdx = 0; deviceExtNdx < DE_LENGTH_OF_ARRAY(deviceExtensionNames); deviceExtNdx++)
				{
					vector<const char*> deviceExtFunctions;

					if (isSupportedDeviceExt(deviceExtensionNames[deviceExtNdx], deviceApiVersion))
						getDeviceExtensionFunctions(deviceApiVersion, deviceExtensionNames[deviceExtNdx], deviceExtFunctions);

					for (size_t deviceFuncNdx = 0; deviceFuncNdx < deviceExtFunctions.size(); deviceFuncNdx++)
						extFunctions.push_back(FunctionInfo(deviceExtFunctions[deviceFuncNdx], FUNCTIONORIGIN_DEVICE));
				}

				log << tcu::TestLog::Message << "Enabled extensions check - tries to get functions of supported extensions from proper vkGet*ProcAddr." << tcu::TestLog::EndMessage;
				const char * const		result = regularCheck(ctx, log, failsQuantity, extFunctions) ? "Passed" : "Failed";
				log << tcu::TestLog::Message << result << tcu::TestLog::EndMessage;
			}
		}

		if (failsQuantity > 0u)
			return tcu::TestStatus::fail("Fail");
		else
			return tcu::TestStatus::pass("Pass");
	}

private:

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

	Move<VkDevice> createTestDevice (const Context& context, VkInstance instance, vector<string> extensions = vector<string>(), bool allowLayers = true)
	{
		auto&						cmdLine			= context.getTestContext().getCommandLine();
		const PlatformInterface&	vkp				= context.getPlatformInterface();
		const InstanceInterface&	vki				= context.getInstanceInterface();
		VkPhysicalDevice			physicalDevice	= chooseDevice(context.getInstanceInterface(), instance, cmdLine);
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

		void* pNext									= DE_NULL;
#ifdef CTS_USES_VULKANSC
		VkDeviceObjectReservationCreateInfo memReservationInfo	= context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
		memReservationInfo.pNext								= pNext;
		pNext													= &memReservationInfo;

		VkPhysicalDeviceVulkanSC10Features sc10Features			= createDefaultSC10Features();
		sc10Features.pNext										= pNext;
		pNext													= &sc10Features;

		VkPipelineCacheCreateInfo			pcCI;
		std::vector<VkPipelinePoolSize>		poolSizes;
		if (context.getTestContext().getCommandLine().isSubProcess())
		{
			if (context.getResourceInterface()->getCacheDataSize() > 0)
			{
				pcCI =
				{
					VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,		// VkStructureType				sType;
					DE_NULL,											// const void*					pNext;
					VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
						VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
					context.getResourceInterface()->getCacheDataSize(),	// deUintptr					initialDataSize;
					context.getResourceInterface()->getCacheData()		// const void*					pInitialData;
				};
				memReservationInfo.pipelineCacheCreateInfoCount		= 1;
				memReservationInfo.pPipelineCacheCreateInfos		= &pcCI;
			}

			poolSizes							= context.getResourceInterface()->getPipelinePoolSizes();
			if (!poolSizes.empty())
			{
				memReservationInfo.pipelinePoolSizeCount		= deUint32(poolSizes.size());
				memReservationInfo.pPipelinePoolSizes			= poolSizes.data();
			}
		}
#endif // CTS_USES_VULKANSC

		const VkDeviceCreateInfo	deviceInfo		= {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			pNext,
			static_cast<VkDeviceCreateFlags>(0u),
			1u,
			&queueInfo,
			0u,
			DE_NULL,
			(deUint32)extensions.size(),
			extensions.size() ? &extensionPtrs[0] : DE_NULL,
			DE_NULL,
		};

		const bool					validationEnabled = (cmdLine.isValidationEnabled() && allowLayers);
		return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceInfo);
	}

	void reportFail (tcu::TestLog& log, const char* const functionName, const char* const firstParamName, const char* const secondParamName, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		log << tcu::TestLog::Message
			<< "[" << failsQuantity << "] " << functionName << '(' << firstParamName << ", \"" << secondParamName << "\") "
			<< "returned " << (shouldBeNonNull ? "nullptr" : "non-null") << ". Should return " << (shouldBeNonNull ? "valid function address." : "nullptr.")
			<< tcu::TestLog::EndMessage;
		++failsQuantity;
	}

	void checkPlatformFunction (const APIContext& ctx, tcu::TestLog& log, const char* const name, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		if ((ctx.getInstanceProcAddr(DE_NULL, name) == DE_NULL) == shouldBeNonNull)
			reportFail(log, "vkGetInstanceProcAddr", "DE_NULL", name, shouldBeNonNull, failsQuantity);
	}

	void checkInstanceFunction (const APIContext& ctx, tcu::TestLog& log, const char* const name, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		if ((ctx.getInstanceProcAddr(ctx.instance, name) == DE_NULL) == shouldBeNonNull)
			reportFail(log, "vkGetInstanceProcAddr", "instance", name, shouldBeNonNull, failsQuantity);
	}

	void checkDeviceFunction (const APIContext& ctx, tcu::TestLog& log, const char* const name, deBool shouldBeNonNull, deUint32& failsQuantity)
	{
		if ((ctx.getDeviceProcAddr(ctx.device, name) == DE_NULL) == shouldBeNonNull)
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

	deBool mixupAddressProcCheck (const APIContext& ctx, tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
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
				checkPlatformFunction(ctx, log, functionName, DE_FALSE, failsQuantity);
				checkDeviceFunction(ctx, log, functionName, DE_FALSE, failsQuantity);
			}
			else if (functionType == FUNCTIONORIGIN_DEVICE)
				checkPlatformFunction(ctx, log, functionName, DE_FALSE, failsQuantity);
		}
		return startingQuantity == failsQuantity;
	}

	deBool specialCasesCheck (const APIContext& ctx, tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
	{
		const deUint32 startingQuantity = failsQuantity;
		for (deUint32 ndx = 0u; ndx < testsArr.size(); ++ndx)
		{
			const deUint32 functionType = testsArr[ndx].second;
			if (functionType == FUNCTIONORIGIN_PLATFORM)
				checkPlatformFunction(ctx, log, testsArr[ndx].first, DE_FALSE, failsQuantity);
			else if (functionType == FUNCTIONORIGIN_INSTANCE)
				checkInstanceFunction(ctx, log, testsArr[ndx].first, DE_FALSE, failsQuantity);
			else if (functionType == FUNCTIONORIGIN_DEVICE)
				checkDeviceFunction(ctx, log, testsArr[ndx].first, DE_FALSE, failsQuantity);
		}
		return startingQuantity == failsQuantity;
	}

	deBool regularCheck (const APIContext& ctx, tcu::TestLog& log, deUint32& failsQuantity, const vector<pair<const char*, FunctionOrigin> >& testsArr)
	{
		const deUint32 startingQuantity = failsQuantity;

		for (deUint32 ndx = 0u; ndx < testsArr.size(); ++ndx)
		{
			const auto&	funcName	= testsArr[ndx].first;
			const auto&	funcType	= testsArr[ndx].second;
			const auto	apiVersion	= m_context.getUsedApiVersion();

			if (deStringEqual(funcName, "vkGetInstanceProcAddr") && apiVersion < VK_API_VERSION_1_2)
				continue;

			// VK_KHR_draw_indirect_count was promoted to core in Vulkan 1.2, but these entrypoints are not mandatory unless the
			// device supports the extension. In that case, the drawIndirectCount feature bit will also be true. Any of the two
			// checks is valid. We use the extension name for convenience here.
			if ((deStringEqual(funcName, "vkCmdDrawIndirectCount") || deStringEqual(funcName, "vkCmdDrawIndexedIndirectCount"))
				&& !isSupportedDeviceExt("VK_KHR_draw_indirect_count", apiVersion))
				continue;

			if (funcType == FUNCTIONORIGIN_PLATFORM)
			{
				checkPlatformFunction(ctx, log, funcName, DE_TRUE, failsQuantity);
			}
			else if (funcType == FUNCTIONORIGIN_INSTANCE)
			{
				checkInstanceFunction(ctx, log, funcName, DE_TRUE, failsQuantity);
				checkDeviceFunction(ctx, log, funcName, DE_FALSE, failsQuantity);
			}
			else if (funcType == FUNCTIONORIGIN_DEVICE)
			{
				checkInstanceFunction(ctx, log, funcName, DE_TRUE, failsQuantity);
				checkDeviceFunction(ctx, log, funcName, DE_TRUE, failsQuantity);
			}
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

class APIUnavailableEntryPointsTestInstance : public TestInstance
{
public:
	APIUnavailableEntryPointsTestInstance(Context& ctx): TestInstance(ctx)
	{}

	virtual tcu::TestStatus iterate(void)
	{
		const vk::PlatformInterface&	vkp					= m_context.getPlatformInterface();
		tcu::TestLog&					log					= m_context.getTestContext().getLog();
		const auto						supportedApiVersion	= m_context.getUsedApiVersion();
		bool							testPassed			= true;

		ApisMap functionsPerVersion;
		initApisMap(functionsPerVersion);

		// create custom instance for each api version
		for (const auto& testedApiVersion : functionsPerVersion)
		{
			// we cant test api versions that are higher then api version support by this device
			if (testedApiVersion.first > supportedApiVersion)
				break;

			// there is no api version above the last api version
			if (testedApiVersion.first == functionsPerVersion.rbegin()->first)
				break;

			VkApplicationInfo appInfo	= initVulkanStructure();
			appInfo.pApplicationName	= "a";
			appInfo.pEngineName			= "b";
			appInfo.apiVersion			= testedApiVersion.first;
			VkInstanceCreateInfo instanceCreateInfo	= initVulkanStructure();
			instanceCreateInfo.pApplicationInfo		= &appInfo;

#ifndef CTS_USES_VULKANSC
			char const * requiredExtensionForVk10 = "VK_KHR_get_physical_device_properties2";
			if (appInfo.apiVersion == VK_API_VERSION_1_0)
			{
				instanceCreateInfo.enabledExtensionCount = 1U;
				instanceCreateInfo.ppEnabledExtensionNames = &requiredExtensionForVk10;
			}
#endif // CTS_USES_VULKANSC

			// create instance for currentluy tested vulkan version
			Move<VkInstance>					customInstance			(vk::createInstance(vkp, &instanceCreateInfo, DE_NULL));
			std::unique_ptr<vk::InstanceDriver>	instanceDriver			(new InstanceDriver(vkp, *customInstance));
			const VkPhysicalDevice				physicalDevice			= chooseDevice(*instanceDriver, *customInstance, m_context.getTestContext().getCommandLine());
			const auto							queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(*instanceDriver, physicalDevice);

			const float queuePriority = 1.0f;
			VkDeviceQueueCreateInfo deviceQueueCreateInfo	= initVulkanStructure();
			deviceQueueCreateInfo.queueCount				= 1;
			deviceQueueCreateInfo.pQueuePriorities			= &queuePriority;

			VkDeviceCreateInfo deviceCreateInfo				= initVulkanStructure();
			deviceCreateInfo.queueCreateInfoCount			= 1u;
			deviceCreateInfo.pQueueCreateInfos				= &deviceQueueCreateInfo;

#ifndef CTS_USES_VULKANSC
			char const * extensionName						= "VK_KHR_maintenance5";
			deviceCreateInfo.enabledExtensionCount			= 1u;
			deviceCreateInfo.ppEnabledExtensionNames		= &extensionName;

			vk::VkPhysicalDeviceMaintenance5FeaturesKHR maint5 = initVulkanStructure();
			vk::VkPhysicalDeviceFeatures2 features2			= initVulkanStructure(&maint5);
			instanceDriver->getPhysicalDeviceFeatures2(physicalDevice, &features2);
			deviceCreateInfo.pNext							= &features2;
#endif // CTS_USES_VULKANSC

			// create custom device
			const Unique<VkDevice>	device			(createCustomDevice(false, vkp, *customInstance, *instanceDriver, physicalDevice, &deviceCreateInfo));
			const DeviceDriver		deviceDriver	(vkp, *customInstance, *device, supportedApiVersion);

			log << tcu::TestLog::Message << "Checking apiVersion("
				<< VK_API_VERSION_MAJOR(testedApiVersion.first) << ", "
				<< VK_API_VERSION_MINOR(testedApiVersion.first) << ")" << tcu::TestLog::EndMessage;

			// iterate over api versions that are above tested api version
			auto& previousVersionFunctions = functionsPerVersion[VK_API_VERSION_1_0];
			for (const auto& versionFunctions : functionsPerVersion)
			{
				// skip api versions that are not above tested api version
				if (versionFunctions.first <= testedApiVersion.first)
				{
					previousVersionFunctions = versionFunctions.second;
					continue;
				}

				// iterate over all functions
				for (const auto& function : versionFunctions.second)
				{
					// we are interested only in device functions
					if (function.second != FUNCTIONORIGIN_DEVICE)
						continue;

					// skip functions that are present in previous version;
					// functionsPerVersion contains all functions that are
					// available in vulkan version, not only ones that were added
					const auto&	funcName	= function.first;
					const auto	isMatch		= [&funcName](const FunctionInfo& fi) { return !strcmp(funcName, fi.first); };
					auto matchIt = std::find_if(begin(previousVersionFunctions), end(previousVersionFunctions), isMatch);
					if (matchIt != previousVersionFunctions.end())
						continue;

					// check if returned function pointer is NULL
					if (deviceDriver.getDeviceProcAddr(*device, funcName) != DE_NULL)
					{
						log << tcu::TestLog::Message << "getDeviceProcAddr(" << funcName
							<< ") returned non-null pointer, expected NULL" << tcu::TestLog::EndMessage;
						testPassed = false;
					}
				}

				previousVersionFunctions = versionFunctions.second;
			}
		}

		if (testPassed)
			return tcu::TestStatus::pass("Pass");
		return tcu::TestStatus::fail("Fail");
	}
};

class APIUnavailableEntryPointsTestCase : public TestCase
{
public:
	APIUnavailableEntryPointsTestCase(tcu::TestContext& testCtx)
		: TestCase(testCtx, "unavailable_entry_points", "Check if vkGetDeviceProcAddr returns NULL for functions beyond app version.")
	{}

	virtual void			checkSupport(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
	}

	virtual TestInstance*	createInstance(Context& ctx) const
	{
		return new APIUnavailableEntryPointsTestInstance(ctx);
	}
};

} // anonymous

tcu::TestCaseGroup*			createVersionSanityCheckTests	(tcu::TestContext & testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	versionTests	(new tcu::TestCaseGroup(testCtx, "version_check", "API Version Tests"));
	versionTests->addChild(new APIVersionTestCase(testCtx));
	versionTests->addChild(new APIEntryPointsTestCase(testCtx));

#ifndef CTS_USES_VULKANSC
	versionTests->addChild(new APIUnavailableEntryPointsTestCase(testCtx));
#endif

	return versionTests.release();
}

} // api

} // vkt
