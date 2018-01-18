/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Device Initialization Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiDeviceInitializationTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkApiVersion.hpp"

#include "tcuTestLog.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <vector>

namespace vkt
{
namespace api
{

namespace
{

using namespace vk;
using namespace std;
using std::vector;
using tcu::TestLog;

tcu::TestStatus createInstanceTest (Context& context)
{
	tcu::TestLog&				log						= context.getTestContext().getLog();
	tcu::ResultCollector		resultCollector			(log);
	const char*					appNames[]				= { "appName", DE_NULL, "",  "app, name", "app(\"name\"", "app~!@#$%^&*()_+name", "app\nName", "app\r\nName" };
	const char*					engineNames[]			= { "engineName", DE_NULL, "",  "engine. name", "engine\"(name)", "eng~!@#$%^&*()_+name", "engine\nName", "engine\r\nName" };
	const int                   patchNumbers[]          = { 0, 1, 2, 3, 4, 5, 13, 4094, 4095 };
	const deUint32				appVersions[]			= { 0, 1, (deUint32)-1 };
	const deUint32				engineVersions[]		= { 0, 1, (deUint32)-1 };
	const PlatformInterface&	platformInterface		= context.getPlatformInterface();
	vector<VkApplicationInfo>	appInfos;

	// test over appName
	for (int appNameNdx = 0; appNameNdx < DE_LENGTH_OF_ARRAY(appNames); appNameNdx++)
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType				sType;
			DE_NULL,								// const void*					pNext;
			appNames[appNameNdx],					// const char*					pAppName;
			0u,										// deUint32						appVersion;
			"engineName",							// const char*					pEngineName;
			0u,										// deUint32						engineVersion;
			VK_API_VERSION,							// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}

	// test over engineName
	for (int engineNameNdx = 0; engineNameNdx < DE_LENGTH_OF_ARRAY(engineNames); engineNameNdx++)
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType				sType;
			DE_NULL,								// const void*					pNext;
			"appName",								// const char*					pAppName;
			0u,										// deUint32						appVersion;
			engineNames[engineNameNdx],				// const char*					pEngineName;
			0u,										// deUint32						engineVersion;
			VK_API_VERSION,							// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}

	// test over appVersion
	for (int appVersionNdx = 0; appVersionNdx < DE_LENGTH_OF_ARRAY(appVersions); appVersionNdx++)
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType				sType;
			DE_NULL,								// const void*					pNext;
			"appName",								// const char*					pAppName;
			appVersions[appVersionNdx],				// deUint32						appVersion;
			"engineName",							// const char*					pEngineName;
			0u,										// deUint32						engineVersion;
			VK_API_VERSION,							// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}

	// test over engineVersion
	for (int engineVersionNdx = 0; engineVersionNdx < DE_LENGTH_OF_ARRAY(engineVersions); engineVersionNdx++)
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType				sType;
			DE_NULL,								// const void*					pNext;
			"appName",								// const char*					pAppName;
			0u,										// deUint32						appVersion;
			"engineName",							// const char*					pEngineName;
			engineVersions[engineVersionNdx],		// deUint32						engineVersion;
			VK_API_VERSION,							// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}
	// patch component of api version checking (should be ignored by implementation)
	for (int patchVersion = 0; patchVersion < DE_LENGTH_OF_ARRAY(patchNumbers); patchVersion++)
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,					// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			"appName",											// const char*					pAppName;
			0u,													// deUint32						appVersion;
			"engineName",										// const char*					pEngineName;
			0u,													// deUint32						engineVersion;
			VK_MAKE_VERSION(1, 0, patchNumbers[patchVersion]),	// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}

	// test when apiVersion is 0
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType				sType;
			DE_NULL,								// const void*					pNext;
			"appName",								// const char*					pAppName;
			0u,										// deUint32						appVersion;
			"engineName",							// const char*					pEngineName;
			0u,										// deUint32						engineVersion;
			0u,										// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}

	// run the tests!
	for (size_t appInfoNdx = 0; appInfoNdx < appInfos.size(); ++appInfoNdx)
	{
		const VkApplicationInfo&		appInfo					= appInfos[appInfoNdx];
		const VkInstanceCreateInfo		instanceCreateInfo		=
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

		log << TestLog::Message << "Creating instance with appInfo: " << appInfo << TestLog::EndMessage;

		try
		{
			const Unique<VkInstance> instance(createInstance(platformInterface, &instanceCreateInfo));
			log << TestLog::Message << "Succeeded" << TestLog::EndMessage;
		}
		catch (const vk::Error& err)
		{
			resultCollector.fail("Failed, Error code: " + de::toString(err.getMessage()));
		}
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithInvalidApiVersionTest (Context& context)
{
	tcu::TestLog&				log					= context.getTestContext().getLog();
	tcu::ResultCollector		resultCollector		(log);
	const PlatformInterface&	platformInterface	= context.getPlatformInterface();
	const ApiVersion			apiVersion			= unpackVersion(VK_API_VERSION);
	const deUint32				invalidMajorVersion	= (1 << 10) - 1;
	const deUint32				invalidMinorVersion	= (1 << 10) - 1;
	vector<ApiVersion>			invalidApiVersions;

	invalidApiVersions.push_back(ApiVersion(invalidMajorVersion, apiVersion.minorNum, apiVersion.patchNum));
	invalidApiVersions.push_back(ApiVersion(apiVersion.majorNum, invalidMinorVersion, apiVersion.patchNum));

	for (size_t apiVersionNdx = 0; apiVersionNdx < invalidApiVersions.size(); apiVersionNdx++)
	{
		const VkApplicationInfo appInfo					=
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,			// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			"appName",									// const char*					pAppName;
			0u,											// deUint32						appVersion;
			"engineName",								// const char*					pEngineName;
			0u,											// deUint32						engineVersion;
			pack(invalidApiVersions[apiVersionNdx]),	// deUint32						apiVersion;
		};
		const VkInstanceCreateInfo instanceCreateInfo	=
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,									// const void*					pNext;
			(VkInstanceCreateFlags)0u,					// VkInstanceCreateFlags		flags;
			&appInfo,									// const VkApplicationInfo*		pAppInfo;
			0u,											// deUint32						layerCount;
			DE_NULL,									// const char*const*			ppEnabledLayernames;
			0u,											// deUint32						extensionCount;
			DE_NULL,									// const char*const*			ppEnabledExtensionNames;
		};


		log << TestLog::Message
			<<"VK_API_VERSION defined in vulkan.h: " << apiVersion
			<< ", api version used to create instance: " << invalidApiVersions[apiVersionNdx]
			<< TestLog::EndMessage;

		{
			VkInstance		instance	= (VkInstance)0;
			const VkResult	result		= platformInterface.createInstance(&instanceCreateInfo, DE_NULL/*pAllocator*/, &instance);
			const bool		gotInstance	= !!instance;

			if (instance)
			{
				const InstanceDriver	instanceIface	(platformInterface, instance);
				instanceIface.destroyInstance(instance, DE_NULL/*pAllocator*/);
			}

			if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
			{
				TCU_CHECK(!gotInstance);
				log << TestLog::Message << "Pass, instance creation with invalid apiVersion is rejected" << TestLog::EndMessage;
			}
			else
				resultCollector.fail("Fail, instance creation with invalid apiVersion is not rejected");
		}
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithNullApplicationInfoTest (Context& context)
{
	tcu::TestLog&				log						= context.getTestContext().getLog();
	tcu::ResultCollector		resultCollector			(log);
	const PlatformInterface&	platformInterface		= context.getPlatformInterface();

	const VkInstanceCreateInfo		instanceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,								// const void*					pNext;
		(VkInstanceCreateFlags)0u,				// VkInstanceCreateFlags		flags;
		DE_NULL,								// const VkApplicationInfo*		pAppInfo;
		0u,										// deUint32						layerCount;
		DE_NULL,								// const char*const*			ppEnabledLayernames;
		0u,										// deUint32						extensionCount;
		DE_NULL,								// const char*const*			ppEnabledExtensionNames;
	};

	log << TestLog::Message << "Creating instance with NULL pApplicationInfo" << TestLog::EndMessage;

	try
	{
		const Unique<VkInstance> instance(createInstance(platformInterface, &instanceCreateInfo));
		log << TestLog::Message << "Succeeded" << TestLog::EndMessage;
	}
	catch (const vk::Error& err)
	{
		resultCollector.fail("Failed, Error code: " + de::toString(err.getMessage()));
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithUnsupportedExtensionsTest (Context& context)
{
	tcu::TestLog&						log						= context.getTestContext().getLog();
	const PlatformInterface&			platformInterface		= context.getPlatformInterface();
	const char*							enabledExtensions[]		= {"VK_UNSUPPORTED_EXTENSION", "THIS_IS_NOT_AN_EXTENSION"};
	const VkApplicationInfo				appInfo					=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,						// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		"appName",												// const char*					pAppName;
		0u,														// deUint32						appVersion;
		"engineName",											// const char*					pEngineName;
		0u,														// deUint32						engineVersion;
		VK_API_VERSION,											// deUint32						apiVersion;
	};
	const VkInstanceCreateInfo			instanceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,					// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		(VkInstanceCreateFlags)0u,								// VkInstanceCreateFlags		flags;
		&appInfo,												// const VkApplicationInfo*		pAppInfo;
		0u,														// deUint32						layerCount;
		DE_NULL,												// const char*const*			ppEnabledLayernames;
		DE_LENGTH_OF_ARRAY(enabledExtensions),					// deUint32						extensionCount;
		enabledExtensions,										// const char*const*			ppEnabledExtensionNames;
	};

	log << TestLog::Message << "Enabled extensions are: " << TestLog::EndMessage;

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(enabledExtensions); ndx++)
		log << TestLog::Message << enabledExtensions[ndx] <<  TestLog::EndMessage;

	{
		VkInstance		instance	= (VkInstance)0;
		const VkResult	result		= platformInterface.createInstance(&instanceCreateInfo, DE_NULL/*pAllocator*/, &instance);
		const bool		gotInstance	= !!instance;

		if (instance)
		{
			const InstanceDriver	instanceIface	(platformInterface, instance);
			instanceIface.destroyInstance(instance, DE_NULL/*pAllocator*/);
		}

		if (result == VK_ERROR_EXTENSION_NOT_PRESENT)
		{
			TCU_CHECK(!gotInstance);
			return tcu::TestStatus::pass("Pass, creating instance with unsupported extension was rejected.");
		}
		else
			return tcu::TestStatus::fail("Fail, creating instance with unsupported extensions succeeded.");
	}
}

tcu::TestStatus createDeviceTest (Context& context)
{
	const PlatformInterface&		platformInterface		= context.getPlatformInterface();
	const Unique<VkInstance>		instance				(createDefaultInstance(platformInterface));
	const InstanceDriver			instanceDriver			(platformInterface, instance.get());
	const VkPhysicalDevice			physicalDevice			= chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
	const deUint32					queueFamilyIndex		= 0;
	const deUint32					queueCount				= 1;
	const deUint32					queueIndex				= 0;
	const float						queuePriority			= 1.0f;
	const VkDeviceQueueCreateInfo	deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		DE_NULL,
		(VkDeviceQueueCreateFlags)0u,
		queueFamilyIndex,						//queueFamilyIndex;
		queueCount,								//queueCount;
		&queuePriority,							//pQueuePriorities;
	};
	const VkDeviceCreateInfo		deviceCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//sType;
		DE_NULL,								//pNext;
		(VkDeviceCreateFlags)0u,
		1,										//queueRecordCount;
		&deviceQueueCreateInfo,					//pRequestedQueues;
		0,										//layerCount;
		DE_NULL,								//ppEnabledLayerNames;
		0,										//extensionCount;
		DE_NULL,								//ppEnabledExtensionNames;
		DE_NULL,								//pEnabledFeatures;
	};

	const Unique<VkDevice>			device					(createDevice(instanceDriver, physicalDevice, &deviceCreateInfo));
	const DeviceDriver				deviceDriver			(instanceDriver, device.get());
	const VkQueue					queue					= getDeviceQueue(deviceDriver, *device,  queueFamilyIndex, queueIndex);

	VK_CHECK(deviceDriver.queueWaitIdle(queue));

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus createMultipleDevicesTest (Context& context)
{
	tcu::TestLog&										log						= context.getTestContext().getLog();
	tcu::ResultCollector								resultCollector			(log);
	const int											numDevices				= 5;
	const PlatformInterface&							platformInterface		= context.getPlatformInterface();
	const Unique<VkInstance>							instance				(createDefaultInstance(platformInterface));
	const InstanceDriver								instanceDriver			(platformInterface, instance.get());
	const VkPhysicalDevice								physicalDevice			= chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
	const deUint32										queueFamilyIndex		= 0;
	const deUint32										queueCount				= 1;
	const deUint32										queueIndex				= 0;
	const float											queuePriority			= 1.0f;
	const VkDeviceQueueCreateInfo						deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		DE_NULL,
		(VkDeviceQueueCreateFlags)0u,					//flags;
		queueFamilyIndex,								//queueFamilyIndex;
		queueCount,										//queueCount;
		&queuePriority,									//pQueuePriorities;
	};
	const VkDeviceCreateInfo							deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,			//sType;
		DE_NULL,										//pNext;
		(VkDeviceCreateFlags)0u,
		1,												//queueRecordCount;
		&deviceQueueCreateInfo,							//pRequestedQueues;
		0,												//layerCount;
		DE_NULL,										//ppEnabledLayerNames;
		0,												//extensionCount;
		DE_NULL,										//ppEnabledExtensionNames;
		DE_NULL,										//pEnabledFeatures;
	};
	vector<VkDevice>									devices(numDevices, (VkDevice)DE_NULL);

	try
	{
		for (int deviceNdx = 0; deviceNdx < numDevices; deviceNdx++)
		{
			const VkResult result = instanceDriver.createDevice(physicalDevice, &deviceCreateInfo, DE_NULL/*pAllocator*/, &devices[deviceNdx]);

			if (result != VK_SUCCESS)
			{
				resultCollector.fail("Failed to create Device No." + de::toString(deviceNdx) + ", Error Code: " + de::toString(result));
				break;
			}

			{
				const DeviceDriver	deviceDriver	(instanceDriver, devices[deviceNdx]);
				const VkQueue		queue			= getDeviceQueue(deviceDriver, devices[deviceNdx], queueFamilyIndex, queueIndex);

				VK_CHECK(deviceDriver.queueWaitIdle(queue));
			}
		}
	}
	catch (const vk::Error& error)
	{
		resultCollector.fail(de::toString(error.getError()));
	}
	catch (...)
	{
		for (int deviceNdx = (int)devices.size()-1; deviceNdx >= 0; deviceNdx--)
		{
			if (devices[deviceNdx] != (VkDevice)DE_NULL)
			{
				DeviceDriver deviceDriver(instanceDriver, devices[deviceNdx]);
				deviceDriver.destroyDevice(devices[deviceNdx], DE_NULL/*pAllocator*/);
			}
		}

		throw;
	}

	for (int deviceNdx = (int)devices.size()-1; deviceNdx >= 0; deviceNdx--)
	{
		if (devices[deviceNdx] != (VkDevice)DE_NULL)
		{
			DeviceDriver deviceDriver(instanceDriver, devices[deviceNdx]);
			deviceDriver.destroyDevice(devices[deviceNdx], DE_NULL/*pAllocator*/);
		}
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createDeviceWithUnsupportedExtensionsTest (Context& context)
{
	tcu::TestLog&					log						= context.getTestContext().getLog();
	const PlatformInterface&		platformInterface		= context.getPlatformInterface();
	const Unique<VkInstance>		instance				(createDefaultInstance(platformInterface));
	const InstanceDriver			instanceDriver			(platformInterface, instance.get());
	const char*						enabledExtensions[]		= {"VK_UNSUPPORTED_EXTENSION", "THIS_IS_NOT_AN_EXTENSION", "VK_DONT_SUPPORT_ME"};
	const VkPhysicalDevice			physicalDevice			= chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
	const float						queuePriority			= 1.0f;
	const VkDeviceQueueCreateInfo	deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		DE_NULL,
		(VkDeviceQueueCreateFlags)0u,
		0,										//queueFamilyIndex;
		1,										//queueCount;
		&queuePriority,							//pQueuePriorities;
	};
	const VkDeviceCreateInfo		deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//sType;
		DE_NULL,								//pNext;
		(VkDeviceCreateFlags)0u,
		1,										//queueRecordCount;
		&deviceQueueCreateInfo,					//pRequestedQueues;
		0,										//layerCount;
		DE_NULL,								//ppEnabledLayerNames;
		DE_LENGTH_OF_ARRAY(enabledExtensions),	//extensionCount;
		enabledExtensions,						//ppEnabledExtensionNames;
		DE_NULL,								//pEnabledFeatures;
	};

	log << TestLog::Message << "Enabled extensions are: " << TestLog::EndMessage;

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(enabledExtensions); ndx++)
		log << TestLog::Message << enabledExtensions[ndx] <<  TestLog::EndMessage;

	{
		VkDevice		device		= (VkDevice)0;
		const VkResult	result		= instanceDriver.createDevice(physicalDevice, &deviceCreateInfo, DE_NULL/*pAllocator*/, &device);
		const bool		gotDevice	= !!device;

		if (device)
		{
			const DeviceDriver	deviceIface	(instanceDriver, device);
			deviceIface.destroyDevice(device, DE_NULL/*pAllocator*/);
		}

		if (result == VK_ERROR_EXTENSION_NOT_PRESENT)
		{
			TCU_CHECK(!gotDevice);
			return tcu::TestStatus::pass("Pass, create device with unsupported extension is rejected.");
		}
		else
			return tcu::TestStatus::fail("Fail, create device with unsupported extension but succeed.");
	}
}

deUint32 getGlobalMaxQueueCount(const vector<VkQueueFamilyProperties>& queueFamilyProperties)
{
	deUint32 maxQueueCount = 0;

	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < (deUint32)queueFamilyProperties.size(); queueFamilyNdx++)
	{
		maxQueueCount = de::max(maxQueueCount, queueFamilyProperties[queueFamilyNdx].queueCount);
	}

	return maxQueueCount;
}

tcu::TestStatus createDeviceWithVariousQueueCountsTest (Context& context)
{
	tcu::TestLog&							log						= context.getTestContext().getLog();
	const int								queueCountDiff			= 1;
	const PlatformInterface&				platformInterface		= context.getPlatformInterface();
	const Unique<VkInstance>				instance				(createDefaultInstance(platformInterface));
	const InstanceDriver					instanceDriver			(platformInterface, instance.get());
	const VkPhysicalDevice					physicalDevice			= chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
	const vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
	const vector<float>						queuePriorities			(getGlobalMaxQueueCount(queueFamilyProperties), 1.0f);
	vector<VkDeviceQueueCreateInfo>			deviceQueueCreateInfos;

	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < (deUint32)queueFamilyProperties.size(); queueFamilyNdx++)
	{
		const deUint32 maxQueueCount = queueFamilyProperties[queueFamilyNdx].queueCount;

		for (deUint32 queueCount = 1; queueCount <= maxQueueCount; queueCount += queueCountDiff)
		{
			const VkDeviceQueueCreateInfo queueCreateInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				DE_NULL,
				(VkDeviceQueueCreateFlags)0u,
				queueFamilyNdx,
				queueCount,
				queuePriorities.data()
			};

			deviceQueueCreateInfos.push_back(queueCreateInfo);
		}
	}

	for (size_t testNdx = 0; testNdx < deviceQueueCreateInfos.size(); testNdx++)
	{
		const VkDeviceQueueCreateInfo&	queueCreateInfo		= deviceQueueCreateInfos[testNdx];
		const VkDeviceCreateInfo		deviceCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//sType;
			DE_NULL,								//pNext;
			(VkDeviceCreateFlags)0u,
			1,										//queueRecordCount;
			&queueCreateInfo,						//pRequestedQueues;
			0,										//layerCount;
			DE_NULL,								//ppEnabledLayerNames;
			0,										//extensionCount;
			DE_NULL,								//ppEnabledExtensionNames;
			DE_NULL,								//pEnabledFeatures;
		};
		const Unique<VkDevice>			device				(createDevice(instanceDriver, physicalDevice, &deviceCreateInfo));
		const DeviceDriver				deviceDriver		(instanceDriver, device.get());
		const deUint32					queueFamilyIndex	= deviceCreateInfo.pQueueCreateInfos->queueFamilyIndex;
		const deUint32					queueCount			= deviceCreateInfo.pQueueCreateInfos->queueCount;

		for (deUint32 queueIndex = 0; queueIndex < queueCount; queueIndex++)
		{
			const VkQueue		queue	= getDeviceQueue(deviceDriver, *device, queueFamilyIndex, queueIndex);
			VkResult			result;

			TCU_CHECK(!!queue);

			result = deviceDriver.queueWaitIdle(queue);
			if (result != VK_SUCCESS)
			{
				log << TestLog::Message
					<< "vkQueueWaitIdle failed"
					<< ",  queueIndex = " << queueIndex
					<< ", queueCreateInfo " << queueCreateInfo
					<< ", Error Code: " << result
					<< TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
		}
	}
	return tcu::TestStatus::pass("Pass");
}

Move<VkInstance> createInstanceWithExtension (const PlatformInterface& vkp, const char* extensionName)
{
	const vector<VkExtensionProperties>	instanceExts	= enumerateInstanceExtensionProperties(vkp, DE_NULL);
	vector<string>						enabledExts;

	if (!isExtensionSupported(instanceExts, RequiredExtension(extensionName)))
		TCU_THROW(NotSupportedError, (string(extensionName) + " is not supported").c_str());

	enabledExts.push_back(extensionName);

	return createDefaultInstance(vkp, vector<string>() /* layers */, enabledExts);
}

tcu::TestStatus createDeviceFeatures2Test (Context& context)
{
	const PlatformInterface&		vkp						= context.getPlatformInterface();
	const Unique<VkInstance>		instance				(createInstanceWithExtension(vkp, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver			vki						(vkp, instance.get());
	const VkPhysicalDevice			physicalDevice			= chooseDevice(vki, instance.get(), context.getTestContext().getCommandLine());
	const deUint32					queueFamilyIndex		= 0;
	const deUint32					queueCount				= 1;
	const deUint32					queueIndex				= 0;
	const float						queuePriority			= 1.0f;

	VkPhysicalDeviceFeatures2KHR	enabledFeatures;
	const VkDeviceQueueCreateInfo	deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		DE_NULL,
		(VkDeviceQueueCreateFlags)0u,
		queueFamilyIndex,
		queueCount,
		&queuePriority,
	};
	const VkDeviceCreateInfo		deviceCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		&enabledFeatures,
		(VkDeviceCreateFlags)0u,
		1,
		&deviceQueueCreateInfo,
		0,
		DE_NULL,
		0,
		DE_NULL,
		DE_NULL,
	};

	// Populate enabledFeatures
	enabledFeatures.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
	enabledFeatures.pNext		= DE_NULL;

	vki.getPhysicalDeviceFeatures2KHR(physicalDevice, &enabledFeatures);

	{
		const Unique<VkDevice>	device		(createDevice(vki, physicalDevice, &deviceCreateInfo));
		const DeviceDriver		vkd			(vki, device.get());
		const VkQueue			queue		= getDeviceQueue(vkd, *device, queueFamilyIndex, queueIndex);

		VK_CHECK(vkd.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
}

struct Feature
{
	const char*	name;
	size_t		offset;
};

#define FEATURE_ITEM(MEMBER) {#MEMBER, DE_OFFSET_OF(VkPhysicalDeviceFeatures, MEMBER)}

tcu::TestStatus createDeviceWithUnsupportedFeaturesTest (Context& context)
{
	tcu::TestLog&				log						= context.getTestContext().getLog();
	tcu::ResultCollector		resultCollector			(log);
	const PlatformInterface&	platformInterface		= context.getPlatformInterface();
	const Unique<VkInstance>	instance				(createDefaultInstance(platformInterface));
	const InstanceDriver		instanceDriver			(platformInterface, instance.get());
	const VkPhysicalDevice		physicalDevice			= chooseDevice(instanceDriver, instance.get(), context.getTestContext().getCommandLine());
	const deUint32				queueFamilyIndex		= 0;
	const deUint32				queueCount				= 1;
	const float					queuePriority			= 1.0f;
	VkPhysicalDeviceFeatures	physicalDeviceFeatures;

	instanceDriver.getPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

	static const Feature features[] =
	{
		FEATURE_ITEM(robustBufferAccess),
		FEATURE_ITEM(fullDrawIndexUint32),
		FEATURE_ITEM(imageCubeArray),
		FEATURE_ITEM(independentBlend),
		FEATURE_ITEM(geometryShader),
		FEATURE_ITEM(tessellationShader),
		FEATURE_ITEM(sampleRateShading),
		FEATURE_ITEM(dualSrcBlend),
		FEATURE_ITEM(logicOp),
		FEATURE_ITEM(multiDrawIndirect),
		FEATURE_ITEM(drawIndirectFirstInstance),
		FEATURE_ITEM(depthClamp),
		FEATURE_ITEM(depthBiasClamp),
		FEATURE_ITEM(fillModeNonSolid),
		FEATURE_ITEM(depthBounds),
		FEATURE_ITEM(wideLines),
		FEATURE_ITEM(largePoints),
		FEATURE_ITEM(alphaToOne),
		FEATURE_ITEM(multiViewport),
		FEATURE_ITEM(samplerAnisotropy),
		FEATURE_ITEM(textureCompressionETC2),
		FEATURE_ITEM(textureCompressionASTC_LDR),
		FEATURE_ITEM(textureCompressionBC),
		FEATURE_ITEM(occlusionQueryPrecise),
		FEATURE_ITEM(pipelineStatisticsQuery),
		FEATURE_ITEM(vertexPipelineStoresAndAtomics),
		FEATURE_ITEM(fragmentStoresAndAtomics),
		FEATURE_ITEM(shaderTessellationAndGeometryPointSize),
		FEATURE_ITEM(shaderImageGatherExtended),
		FEATURE_ITEM(shaderStorageImageExtendedFormats),
		FEATURE_ITEM(shaderStorageImageMultisample),
		FEATURE_ITEM(shaderStorageImageReadWithoutFormat),
		FEATURE_ITEM(shaderStorageImageWriteWithoutFormat),
		FEATURE_ITEM(shaderUniformBufferArrayDynamicIndexing),
		FEATURE_ITEM(shaderSampledImageArrayDynamicIndexing),
		FEATURE_ITEM(shaderStorageBufferArrayDynamicIndexing),
		FEATURE_ITEM(shaderStorageImageArrayDynamicIndexing),
		FEATURE_ITEM(shaderClipDistance),
		FEATURE_ITEM(shaderCullDistance),
		FEATURE_ITEM(shaderFloat64),
		FEATURE_ITEM(shaderInt64),
		FEATURE_ITEM(shaderInt16),
		FEATURE_ITEM(shaderResourceResidency),
		FEATURE_ITEM(shaderResourceMinLod),
		FEATURE_ITEM(sparseBinding),
		FEATURE_ITEM(sparseResidencyBuffer),
		FEATURE_ITEM(sparseResidencyImage2D),
		FEATURE_ITEM(sparseResidencyImage3D),
		FEATURE_ITEM(sparseResidency2Samples),
		FEATURE_ITEM(sparseResidency4Samples),
		FEATURE_ITEM(sparseResidency8Samples),
		FEATURE_ITEM(sparseResidency16Samples),
		FEATURE_ITEM(sparseResidencyAliased),
		FEATURE_ITEM(variableMultisampleRate),
		FEATURE_ITEM(inheritedQueries)
	};

	const int	numFeatures		= DE_LENGTH_OF_ARRAY(features);
	int			numErrors		= 0;

	for (int featureNdx = 0; featureNdx < numFeatures; featureNdx++)
	{
		// Test only features that are not supported.
		if (*(((VkBool32*)((deUint8*)(&physicalDeviceFeatures) + features[featureNdx].offset))))
			continue;

		VkPhysicalDeviceFeatures enabledFeatures;

		for (int i = 0; i < numFeatures; i++)
			*((VkBool32*)((deUint8*)(&enabledFeatures) + features[i].offset)) = (i == featureNdx ? VK_TRUE : VK_FALSE);

		const VkDeviceQueueCreateInfo	deviceQueueCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
			(VkDeviceQueueCreateFlags)0u,
			queueFamilyIndex,
			queueCount,
			&queuePriority
		};
		const VkDeviceCreateInfo		deviceCreateInfo		=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			DE_NULL,
			(VkDeviceCreateFlags)0u,
			1,
			&deviceQueueCreateInfo,
			0,
			DE_NULL,
			0,
			DE_NULL,
			&enabledFeatures
		};

		VkDevice		device;
		const VkResult	res	= instanceDriver.createDevice(physicalDevice, &deviceCreateInfo, DE_NULL, &device);

		if (res != VK_ERROR_FEATURE_NOT_PRESENT)
		{
			numErrors++;
			resultCollector.fail("Not returning VK_ERROR_FEATURE_NOT_PRESENT when creating device with feature "
								 + de::toString(features[featureNdx].name) + ", which was reported as unsupported.");
		}
	}

	if (numErrors > 1)
		return tcu::TestStatus(resultCollector.getResult(), "Enabling " + de::toString(numErrors) + " unsupported features didn't return VK_ERROR_FEATURE_NOT_PRESENT.");
	else
		return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

// Allocation tracking utilities
struct	AllocTrack
{
	bool						active;
	bool						wasAllocated;
	void*						alignedStartAddress;
	char*						actualStartAddress;
	size_t						requestedSizeBytes;
	size_t						actualSizeBytes;
	VkSystemAllocationScope		allocScope;
	deUint64					userData;

	AllocTrack()
		: active				(false)
		, wasAllocated			(false)
		, alignedStartAddress	(DE_NULL)
		, actualStartAddress	(DE_NULL)
		, requestedSizeBytes	(0)
		, actualSizeBytes		(0)
		, allocScope			(VK_SYSTEM_ALLOCATION_SCOPE_COMMAND)
		, userData(0)			{}
};

// Global vector to track allocations. This will be resized before each test and emptied after
// However, we have to globally define it so the allocation callback functions work properly
std::vector<AllocTrack>	g_allocatedVector;
bool					g_intentionalFailEnabled	= false;
deUint32				g_intenionalFailIndex		= 0;
deUint32				g_intenionalFailCount		= 0;

void freeAllocTracker (void)
{
	g_allocatedVector.clear();
}

void initAllocTracker (size_t size, deUint32 intentionalFailIndex = (deUint32)~0)
{
	if (g_allocatedVector.size() > 0)
		freeAllocTracker();

	g_allocatedVector.resize(size);

	if (intentionalFailIndex != (deUint32)~0)
	{
		g_intentionalFailEnabled	= true;
		g_intenionalFailIndex		= intentionalFailIndex;
		g_intenionalFailCount		= 0;
	}
	else
	{
		g_intentionalFailEnabled	= false;
		g_intenionalFailIndex		= 0;
		g_intenionalFailCount		= 0;
	}
}

bool isAllocTrackerEmpty ()
{
	bool success		= true;
	bool wasAllocated	= false;

	for (deUint32 vectorIdx	= 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
	{
		if (g_allocatedVector[vectorIdx].active)
			success = false;
		else if (!wasAllocated && g_allocatedVector[vectorIdx].wasAllocated)
			wasAllocated = true;
	}

	if (!g_intentionalFailEnabled && !wasAllocated)
		success = false;

	return success;
}

VKAPI_ATTR void *VKAPI_CALL allocCallbackFunc (void *pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	if (g_intentionalFailEnabled)
		if (++g_intenionalFailCount >= g_intenionalFailIndex)
			return DE_NULL;

	for (deUint32 vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
	{
		if (!g_allocatedVector[vectorIdx].active)
		{
			g_allocatedVector[vectorIdx].requestedSizeBytes		= size;
			g_allocatedVector[vectorIdx].actualSizeBytes		= size + (alignment - 1);
			g_allocatedVector[vectorIdx].alignedStartAddress	= DE_NULL;
			g_allocatedVector[vectorIdx].actualStartAddress		= new char[g_allocatedVector[vectorIdx].actualSizeBytes];

			if (g_allocatedVector[vectorIdx].actualStartAddress != DE_NULL)
			{
				deUint64 addr	=	(deUint64)g_allocatedVector[vectorIdx].actualStartAddress;
				addr			+=	(alignment - 1);
				addr			&=	~(alignment - 1);
				g_allocatedVector[vectorIdx].alignedStartAddress	= (void *)addr;
				g_allocatedVector[vectorIdx].allocScope				= allocationScope;
				g_allocatedVector[vectorIdx].userData				= (deUint64)pUserData;
				g_allocatedVector[vectorIdx].active					= true;
				g_allocatedVector[vectorIdx].wasAllocated			= true;
			}

			return g_allocatedVector[vectorIdx].alignedStartAddress;
		}
	}
	return DE_NULL;
}

VKAPI_ATTR void VKAPI_CALL freeCallbackFunc (void *pUserData, void *pMemory)
{
	DE_UNREF(pUserData);

	for (deUint32 vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
	{
		if (g_allocatedVector[vectorIdx].active && g_allocatedVector[vectorIdx].alignedStartAddress == pMemory)
		{
			delete[] g_allocatedVector[vectorIdx].actualStartAddress;
			g_allocatedVector[vectorIdx].active = false;
			break;
		}
	}
}

VKAPI_ATTR void *VKAPI_CALL reallocCallbackFunc (void *pUserData, void *pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	if (pOriginal != DE_NULL)
	{
		for (deUint32 vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
		{
			if (g_allocatedVector[vectorIdx].active && g_allocatedVector[vectorIdx].alignedStartAddress == pOriginal)
			{
				if (size == 0)
				{
					freeCallbackFunc(pUserData, pOriginal);
					return DE_NULL;
				}
				else if (size < g_allocatedVector[vectorIdx].requestedSizeBytes)
					return pOriginal;
				else
				{
					void *pNew = allocCallbackFunc(pUserData, size, alignment, allocationScope);

					if (pNew != DE_NULL)
					{
						size_t copySize = size;

						if (g_allocatedVector[vectorIdx].requestedSizeBytes < size)
							copySize = g_allocatedVector[vectorIdx].requestedSizeBytes;

						memcpy(pNew, pOriginal, copySize);
						freeCallbackFunc(pUserData, pOriginal);
					}
					return pNew;
				}
			}
		}
		return DE_NULL;
	}
	else
		return allocCallbackFunc(pUserData, size, alignment, allocationScope);
}

tcu::TestStatus createInstanceDeviceIntentionalAllocFail (Context& context)
{
	const PlatformInterface&	vkp					= context.getPlatformInterface();
	const deUint32				chosenDevice		= context.getTestContext().getCommandLine().getVKDeviceId() - 1;
	VkInstance					instance			= DE_NULL;
	VkDevice					device				= DE_NULL;
	deUint32					physicalDeviceCount	= 0;
	deUint32					queueFamilyCount	= 0;
	deUint32					queueFamilyIndex	= 0;
	const float					queuePriority		= 0.0f;
	const VkAllocationCallbacks	allocationCallbacks	=
	{
		DE_NULL,								// userData
		allocCallbackFunc,						// pfnAllocation
		reallocCallbackFunc,					// pfnReallocation
		freeCallbackFunc,						// pfnFree
		DE_NULL,								// pfnInternalAllocation
		DE_NULL									// pfnInternalFree
	};
	const VkApplicationInfo		appInfo				=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,		// sType
		DE_NULL,								// pNext
		"appName",								// pApplicationName
		0u,										// applicationVersion
		"engineName",							// pEngineName
		0u,										// engineVersion
		VK_API_VERSION							// apiVersion
	};
	const VkInstanceCreateInfo	instanceCreateInfo	=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		(VkInstanceCreateFlags)0u,				// flags
		&appInfo,								// pApplicationInfo
		0u,										// enabledLayerCount
		DE_NULL,								// ppEnabledLayerNames
		0u,										// enabledExtensionCount
		DE_NULL									// ppEnabledExtensionNames
	};
	deUint32					failIndex			= 0;
	VkResult					result				= VK_ERROR_OUT_OF_HOST_MEMORY;

	while (result == VK_ERROR_OUT_OF_HOST_MEMORY)
	{
		initAllocTracker(9999, failIndex++);

		if (failIndex >= 9999u)
			return tcu::TestStatus::fail("Out of retries, could not create instance and device");

		result = vkp.createInstance(&instanceCreateInfo, &allocationCallbacks, &instance);

		if (result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			if (!isAllocTrackerEmpty())
				return tcu::TestStatus::fail("Allocations still remain, failed on index " + de::toString(failIndex));

			freeAllocTracker();
			continue;
		}
		else if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("createInstance returned " + de::toString(result));

		const InstanceDriver		instanceDriver	(vkp, instance);
		const InstanceInterface&	vki				(instanceDriver);

		result = vki.enumeratePhysicalDevices(instance, &physicalDeviceCount, DE_NULL);

		if (result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			vki.destroyInstance(instance, &allocationCallbacks);

			if (!isAllocTrackerEmpty())
				return tcu::TestStatus::fail("Allocations still remain, failed on index " + de::toString(failIndex));

			freeAllocTracker();
			continue;
		}
		else if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("enumeratePhysicalDevices returned " + de::toString(result));

		vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

		result = vki.enumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

		if (result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			vki.destroyInstance(instance, &allocationCallbacks);

			if (!isAllocTrackerEmpty())
				return tcu::TestStatus::fail("Allocations still remain, failed on index " + de::toString(failIndex));

			freeAllocTracker();
			continue;
		}
		else if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("enumeratePhysicalDevices returned " + de::toString(result));

		vki.getPhysicalDeviceQueueFamilyProperties(physicalDevices[chosenDevice], &queueFamilyCount, DE_NULL);

		if (queueFamilyCount == 0u)
			return tcu::TestStatus::fail("getPhysicalDeviceQueueFamilyProperties returned zero queue families");

		vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

		vki.getPhysicalDeviceQueueFamilyProperties(physicalDevices[chosenDevice], &queueFamilyCount, queueFamilies.data());

		if (queueFamilyCount == 0u)
			return tcu::TestStatus::fail("getPhysicalDeviceQueueFamilyProperties returned zero queue families");

		for (deUint32 i = 0; i < queueFamilyCount; i++)
		{
			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queueFamilyIndex = i;
				break;
			}
		}

		const VkDeviceQueueCreateInfo	deviceQueueCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// sType
			DE_NULL,									// pNext
			(VkDeviceQueueCreateFlags)0u,				// flags
			queueFamilyIndex,							// queueFamilyIndex
			1u,											// queueCount
			&queuePriority								// pQueuePriorities
		};
		const VkDeviceCreateInfo		deviceCreateInfo		=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		// sType
			DE_NULL,									// pNext
			(VkDeviceCreateFlags)0u,					// flags
			1u,											// queueCreateInfoCount
			&deviceQueueCreateInfo,						// pQueueCreateInfos
			0u,											// enabledLayerCount
			DE_NULL,									// ppEnabledLayerNames
			0u,											// enabledExtensionCount
			DE_NULL,									// ppEnabledExtensionNames
			DE_NULL										// pEnabledFeatures
		};

		result = vki.createDevice(physicalDevices[chosenDevice], &deviceCreateInfo, &allocationCallbacks, &device);

		if (result == VK_ERROR_OUT_OF_HOST_MEMORY)
		{
			vki.destroyInstance(instance, &allocationCallbacks);

			if (!isAllocTrackerEmpty())
				return tcu::TestStatus::fail("Allocations still remain, failed on index " + de::toString(failIndex));

			freeAllocTracker();
			continue;
		}
		else if (result != VK_SUCCESS)
			return tcu::TestStatus::fail("VkCreateDevice returned " + de::toString(result));

		DeviceDriver(vki, device).destroyDevice(device, &allocationCallbacks);
		vki.destroyInstance(instance, &allocationCallbacks);
		freeAllocTracker();
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createDeviceInitializationTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	deviceInitializationTests (new tcu::TestCaseGroup(testCtx, "device_init", "Device Initialization Tests"));

	addFunctionCase(deviceInitializationTests.get(), "create_instance_name_version",					"", createInstanceTest);
	addFunctionCase(deviceInitializationTests.get(), "create_instance_invalid_api_version",				"", createInstanceWithInvalidApiVersionTest);
	addFunctionCase(deviceInitializationTests.get(), "create_instance_null_appinfo",					"", createInstanceWithNullApplicationInfoTest);
	addFunctionCase(deviceInitializationTests.get(), "create_instance_unsupported_extensions",			"", createInstanceWithUnsupportedExtensionsTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device",									"", createDeviceTest);
	addFunctionCase(deviceInitializationTests.get(), "create_multiple_devices",							"", createMultipleDevicesTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_unsupported_extensions",			"", createDeviceWithUnsupportedExtensionsTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_various_queue_counts",				"", createDeviceWithVariousQueueCountsTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_features2",							"", createDeviceFeatures2Test);
	addFunctionCase(deviceInitializationTests.get(), "create_device_unsupported_features",				"", createDeviceWithUnsupportedFeaturesTest);
	addFunctionCase(deviceInitializationTests.get(), "create_instance_device_intentional_alloc_fail",	"", createInstanceDeviceIntentionalAllocFail);

	return deviceInitializationTests.release();
}

} // api
} // vkt
