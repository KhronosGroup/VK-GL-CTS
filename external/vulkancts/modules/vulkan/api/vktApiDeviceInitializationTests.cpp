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
#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkApiVersion.hpp"
#include "vkAllocationCallbackUtil.hpp"

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
	const int					patchNumbers[]			= { 0, 1, 2, 3, 4, 5, 13, 4094, 4095 };
	const deUint32				appVersions[]			= { 0, 1, (deUint32)-1 };
	const deUint32				engineVersions[]		= { 0, 1, (deUint32)-1 };
	const deUint32				apiVersion				= context.getUsedApiVersion();
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
			apiVersion,								// deUint32						apiVersion;
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
			apiVersion,								// deUint32						apiVersion;
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
			apiVersion,								// deUint32						apiVersion;
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
			apiVersion,								// deUint32						apiVersion;
		};

		appInfos.push_back(appInfo);
	}
	const deUint32	manjorNum	= unpackVersion(apiVersion).majorNum;
	const deUint32	minorNum	= unpackVersion(apiVersion).minorNum;

	// patch component of api version checking (should be ignored by implementation)
	for (int patchVersion = 0; patchVersion < DE_LENGTH_OF_ARRAY(patchNumbers); patchVersion++)
	{
		const VkApplicationInfo appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,									// VkStructureType				sType;
			DE_NULL,															// const void*					pNext;
			"appName",															// const char*					pAppName;
			0u,																	// deUint32						appVersion;
			"engineName",														// const char*					pEngineName;
			0u,																	// deUint32						engineVersion;
			VK_MAKE_VERSION(manjorNum, minorNum, patchNumbers[patchVersion]),	// deUint32						apiVersion;
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
			CustomInstance instance = createCustomInstanceFromInfo(context, &instanceCreateInfo);
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
	tcu::TestLog&				log						= context.getTestContext().getLog();
	tcu::ResultCollector		resultCollector			(log);
	const PlatformInterface&	platformInterface		= context.getPlatformInterface();

	deUint32					instanceApiVersion		= 0u;
	platformInterface.enumerateInstanceVersion(&instanceApiVersion);

	const ApiVersion			apiVersion				= unpackVersion(instanceApiVersion);

	const deUint32				invalidMajorVersion		= (1 << 10) - 1;
	const deUint32				invalidMinorVersion		= (1 << 10) - 1;
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
		const VkInstanceCreateInfo	instanceCreateInfo	=
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
			<< "API version reported by enumerateInstanceVersion: " << apiVersion
			<< ", api version used to create instance: " << invalidApiVersions[apiVersionNdx]
			<< TestLog::EndMessage;

		{
			UncheckedInstance	instance;
			const VkResult		result		= createUncheckedInstance(context, &instanceCreateInfo, DE_NULL, &instance);

			if (apiVersion.majorNum == 1 && apiVersion.minorNum == 0)
			{
				if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
				{
					TCU_CHECK(!static_cast<bool>(instance));
					log << TestLog::Message << "Pass, instance creation with invalid apiVersion is rejected" << TestLog::EndMessage;
				}
				else
					resultCollector.fail("Fail, instance creation with invalid apiVersion is not rejected");
			}
			else if (apiVersion.majorNum == 1 && apiVersion.minorNum >= 1)
			{
				if (result == VK_SUCCESS)
				{
					TCU_CHECK(static_cast<bool>(instance));
					log << TestLog::Message << "Pass, instance creation with nonstandard apiVersion succeeds for Vulkan 1.1" << TestLog::EndMessage;
				}
				else if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
				{
					resultCollector.fail("Fail, In Vulkan 1.1 instance creation must not return VK_ERROR_INCOMPATIBLE_DRIVER.");
				}
				else
				{
					std::ostringstream message;
					message << "Fail, createInstance failed with " << result;
					resultCollector.fail(message.str().c_str());
				}
			}
		}
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithNullApplicationInfoTest (Context& context)
{
	tcu::TestLog&				log						= context.getTestContext().getLog();
	tcu::ResultCollector		resultCollector			(log);

	const VkInstanceCreateInfo	instanceCreateInfo		=
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
		CustomInstance instance = createCustomInstanceFromInfo(context, &instanceCreateInfo);
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
	const char*							enabledExtensions[]		= {"VK_UNSUPPORTED_EXTENSION", "THIS_IS_NOT_AN_EXTENSION"};
	const deUint32						apiVersion				= context.getUsedApiVersion();
	const VkApplicationInfo				appInfo					=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,						// VkStructureType				sType;
		DE_NULL,												// const void*					pNext;
		"appName",												// const char*					pAppName;
		0u,														// deUint32						appVersion;
		"engineName",											// const char*					pEngineName;
		0u,														// deUint32						engineVersion;
		apiVersion,												// deUint32						apiVersion;
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
		UncheckedInstance	instance;
		const VkResult		result		= createUncheckedInstance(context, &instanceCreateInfo, DE_NULL, &instance);

		if (result == VK_ERROR_EXTENSION_NOT_PRESENT)
		{
			TCU_CHECK(!static_cast<bool>(instance));
			return tcu::TestStatus::pass("Pass, creating instance with unsupported extension was rejected.");
		}
		else
			return tcu::TestStatus::fail("Fail, creating instance with unsupported extensions succeeded.");
	}
}

enum
{
	UTF8ABUSE_LONGNAME = 0,
	UTF8ABUSE_BADNAMES,
	UTF8ABUSE_OVERLONGNUL,
	UTF8ABUSE_OVERLONG,
	UTF8ABUSE_ZALGO,
	UTF8ABUSE_CHINESE,
	UTF8ABUSE_EMPTY,
	UTF8ABUSE_MAX
};

string getUTF8AbuseString (int index)
{
	switch (index)
	{
	case UTF8ABUSE_LONGNAME:
		// Generate a long name.
		{
			std::string longname;
			longname.resize(65535, 'k');
			return longname;
		}

	case UTF8ABUSE_BADNAMES:
		// Various illegal code points in utf-8
		return string(
			"Illegal bytes in UTF-8: "
			"\xc0 \xc1 \xf5 \xf6 \xf7 \xf8 \xf9 \xfa \xfb \xfc \xfd \xfe \xff"
			"illegal surrogates: \xed\xad\xbf \xed\xbe\x80");

	case UTF8ABUSE_OVERLONGNUL:
		// Zero encoded as overlong, not exactly legal but often supported to differentiate from terminating zero
		return string("UTF-8 encoded nul \xC0\x80 (should not end name)");

	case UTF8ABUSE_OVERLONG:
		// Some overlong encodings
		return string(
			"UTF-8 overlong \xF0\x82\x82\xAC \xfc\x83\xbf\xbf\xbf\xbf \xf8\x87\xbf\xbf\xbf "
			"\xf0\x8f\xbf\xbf");

	case UTF8ABUSE_ZALGO:
		// Internet "zalgo" meme "bleeding text"
		return string(
			"\x56\xcc\xb5\xcc\x85\xcc\x94\xcc\x88\xcd\x8a\xcc\x91\xcc\x88\xcd\x91\xcc\x83\xcd\x82"
			"\xcc\x83\xcd\x90\xcc\x8a\xcc\x92\xcc\x92\xcd\x8b\xcc\x94\xcd\x9d\xcc\x98\xcc\xab\xcc"
			"\xae\xcc\xa9\xcc\xad\xcc\x97\xcc\xb0\x75\xcc\xb6\xcc\xbe\xcc\x80\xcc\x82\xcc\x84\xcd"
			"\x84\xcc\x90\xcd\x86\xcc\x9a\xcd\x84\xcc\x9b\xcd\x86\xcd\x92\xcc\x9a\xcd\x99\xcd\x99"
			"\xcc\xbb\xcc\x98\xcd\x8e\xcd\x88\xcd\x9a\xcc\xa6\xcc\x9c\xcc\xab\xcc\x99\xcd\x94\xcd"
			"\x99\xcd\x95\xcc\xa5\xcc\xab\xcd\x89\x6c\xcc\xb8\xcc\x8e\xcc\x8b\xcc\x8b\xcc\x9a\xcc"
			"\x8e\xcd\x9d\xcc\x80\xcc\xa1\xcc\xad\xcd\x9c\xcc\xba\xcc\x96\xcc\xb3\xcc\xa2\xcd\x8e"
			"\xcc\xa2\xcd\x96\x6b\xcc\xb8\xcc\x84\xcd\x81\xcc\xbf\xcc\x8d\xcc\x89\xcc\x85\xcc\x92"
			"\xcc\x84\xcc\x90\xcd\x81\xcc\x93\xcd\x90\xcd\x92\xcd\x9d\xcc\x84\xcd\x98\xcd\x9d\xcd"
			"\xa0\xcd\x91\xcc\x94\xcc\xb9\xcd\x93\xcc\xa5\xcd\x87\xcc\xad\xcc\xa7\xcd\x96\xcd\x99"
			"\xcc\x9d\xcc\xbc\xcd\x96\xcd\x93\xcc\x9d\xcc\x99\xcc\xa8\xcc\xb1\xcd\x85\xcc\xba\xcc"
			"\xa7\x61\xcc\xb8\xcc\x8e\xcc\x81\xcd\x90\xcd\x84\xcd\x8c\xcc\x8c\xcc\x85\xcd\x86\xcc"
			"\x84\xcd\x84\xcc\x90\xcc\x84\xcc\x8d\xcd\x99\xcd\x8d\xcc\xb0\xcc\xa3\xcc\xa6\xcd\x89"
			"\xcd\x8d\xcd\x87\xcc\x98\xcd\x8d\xcc\xa4\xcd\x9a\xcd\x8e\xcc\xab\xcc\xb9\xcc\xac\xcc"
			"\xa2\xcd\x87\xcc\xa0\xcc\xb3\xcd\x89\xcc\xb9\xcc\xa7\xcc\xa6\xcd\x89\xcd\x95\x6e\xcc"
			"\xb8\xcd\x8a\xcc\x8a\xcd\x82\xcc\x9b\xcd\x81\xcd\x90\xcc\x85\xcc\x9b\xcd\x80\xcd\x91"
			"\xcd\x9b\xcc\x81\xcd\x81\xcc\x9a\xcc\xb3\xcd\x9c\xcc\x9e\xcc\x9d\xcd\x99\xcc\xa2\xcd"
			"\x93\xcd\x96\xcc\x97\xff");

	case UTF8ABUSE_CHINESE:
		// Some Chinese glyphs.
		// "English equivalent: The devil is in the details", https://en.wikiquote.org/wiki/Chinese_proverbs
		return string(
			"\xe8\xaf\xbb\xe4\xb9\xa6\xe9\xa1\xbb\xe7\x94\xa8\xe6\x84\x8f\xef\xbc\x8c\xe4\xb8\x80"
			"\xe5\xad\x97\xe5\x80\xbc\xe5\x8d\x83\xe9\x87\x91\x20");

	default:
		DE_ASSERT(index == UTF8ABUSE_EMPTY);
		// Also try an empty string.
		return string("");
	}
}

tcu::TestStatus createInstanceWithExtensionNameAbuseTest (Context& context)
{
	const char*					extensionList[1]	= { 0 };
	const deUint32				apiVersion			= context.getUsedApiVersion();
	deUint32					failCount			= 0;

	for (int i = 0; i < UTF8ABUSE_MAX; i++)
	{
		string abuseString	= getUTF8AbuseString(i);
		extensionList[0]	= abuseString.c_str();

		const VkApplicationInfo		appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,				// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			"appName",										// const char*				pAppName;
			0u,												// deUint32					appVersion;
			"engineName",									// const char*				pEngineName;
			0u,												// deUint32					engineVersion;
			apiVersion,										// deUint32					apiVersion;
		};

		const VkInstanceCreateInfo	instanceCreateInfo =
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			(VkInstanceCreateFlags)0u,						// VkInstanceCreateFlags	flags;
			&appInfo,										// const VkApplicationInfo*	pAppInfo;
			0u,												// deUint32					layerCount;
			DE_NULL,										// const char*const*		ppEnabledLayernames;
			1u,												// deUint32					extensionCount;
			extensionList,									// const char*const*		ppEnabledExtensionNames;
		};

		{
			UncheckedInstance	instance;
			const VkResult		result		= createUncheckedInstance(context, &instanceCreateInfo, DE_NULL, &instance);

			if (result != VK_ERROR_EXTENSION_NOT_PRESENT)
				failCount++;

			TCU_CHECK(!static_cast<bool>(instance));
		}
	}

	if (failCount > 0)
		return tcu::TestStatus::fail("Fail, creating instances with unsupported extensions succeeded.");

	return tcu::TestStatus::pass("Pass, creating instances with unsupported extensions were rejected.");
}

tcu::TestStatus createInstanceWithLayerNameAbuseTest (Context& context)
{
	const PlatformInterface&	platformInterface	= context.getPlatformInterface();
	const char*					layerList[1]		= { 0 };
	const deUint32				apiVersion			= context.getUsedApiVersion();
	deUint32					failCount			= 0;

	for (int i = 0; i < UTF8ABUSE_MAX; i++)
	{
		string abuseString	= getUTF8AbuseString(i);
		layerList[0]		= abuseString.c_str();

		const VkApplicationInfo		appInfo =
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,		// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			"appName",								// const char*				pAppName;
			0u,										// deUint32					appVersion;
			"engineName",							// const char*				pEngineName;
			0u,										// deUint32					engineVersion;
			apiVersion,								// deUint32					apiVersion;
		};

		const VkInstanceCreateInfo	instanceCreateInfo =
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			(VkInstanceCreateFlags)0u,				// VkInstanceCreateFlags	flags;
			&appInfo,								// const VkApplicationInfo*	pAppInfo;
			1u,										// deUint32					layerCount;
			layerList,								// const char*const*		ppEnabledLayernames;
			0u,										// deUint32					extensionCount;
			DE_NULL,								// const char*const*		ppEnabledExtensionNames;
		};

		{
			VkInstance		instance	= (VkInstance)0;
			const VkResult	result		= platformInterface.createInstance(&instanceCreateInfo, DE_NULL/*pAllocator*/, &instance);
			const bool		gotInstance	= !!instance;

			if (instance)
			{
				const InstanceDriver instanceIface(platformInterface, instance);
				instanceIface.destroyInstance(instance, DE_NULL/*pAllocator*/);
			}

			if (result != VK_ERROR_LAYER_NOT_PRESENT)
				failCount++;

			TCU_CHECK(!gotInstance);
		}
	}

	if (failCount > 0)
		return tcu::TestStatus::fail("Fail, creating instances with unsupported layers succeeded.");

	return tcu::TestStatus::pass("Pass, creating instances with unsupported layers were rejected.");
}

tcu::TestStatus enumerateDevicesAllocLeakTest(Context& context)
{
	// enumeratePhysicalDevices uses instance-provided allocator
	// and this test checks if all alocated memory is freed

	typedef AllocationCallbackRecorder::RecordIterator RecordIterator;

	const PlatformInterface&	vkp				(context.getPlatformInterface());
	const deUint32				apiVersion		(context.getUsedApiVersion());
	DeterministicFailAllocator	objAllocator	(getSystemAllocator(), DeterministicFailAllocator::MODE_DO_NOT_COUNT, 0);
	AllocationCallbackRecorder	recorder		(objAllocator.getCallbacks(), 128);
	Move<VkInstance>			instance		(vk::createDefaultInstance(vkp, apiVersion, {}, {}, recorder.getCallbacks()));
	InstanceDriver				vki				(vkp, *instance);
	vector<VkPhysicalDevice>	devices			(enumeratePhysicalDevices(vki, *instance));
	RecordIterator				recordToCheck	(recorder.getRecordsEnd());

	try
	{
		devices = enumeratePhysicalDevices(vki, *instance);
	}
	catch (const vk::OutOfMemoryError& e)
	{
		if (e.getError() != VK_ERROR_OUT_OF_HOST_MEMORY)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Got out of memory error - leaks in enumeratePhysicalDevices not tested.");
	}

	// make sure that same number of allocations and frees was done
	deInt32			allocationRecords	(0);
	RecordIterator	lastRecordToCheck	(recorder.getRecordsEnd());
	while (recordToCheck != lastRecordToCheck)
	{
		const AllocationCallbackRecord& record = *recordToCheck;
		switch (record.type)
		{
		case AllocationCallbackRecord::TYPE_ALLOCATION:
			++allocationRecords;
			break;
		case AllocationCallbackRecord::TYPE_FREE:
			if (record.data.free.mem != DE_NULL)
				--allocationRecords;
			break;
		default:
			break;
		}
		++recordToCheck;
	}

	if (allocationRecords)
		return tcu::TestStatus::fail("enumeratePhysicalDevices leaked memory");
	return tcu::TestStatus::pass("Ok");
}

tcu::TestStatus createDeviceTest (Context& context)
{
	const PlatformInterface&		platformInterface		= context.getPlatformInterface();
	const CustomInstance			instance				(createCustomInstanceFromContext(context));
	const InstanceDriver&			instanceDriver			(instance.getDriver());
	const VkPhysicalDevice			physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
	const deUint32					queueFamilyIndex		= 0;
	const deUint32					queueCount				= 1;
	const deUint32					queueIndex				= 0;
	const float						queuePriority			= 1.0f;

	const vector<VkQueueFamilyProperties> queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

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

	const Unique<VkDevice>			device					(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
	const DeviceDriver				deviceDriver			(platformInterface, instance, device.get());
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
	const CustomInstance								instance				(createCustomInstanceFromContext(context));
	const InstanceDriver&								instanceDriver			(instance.getDriver());
	const VkPhysicalDevice								physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
	const vector<VkQueueFamilyProperties>				queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
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
			const VkResult result = createUncheckedDevice(context.getTestContext().getCommandLine().isValidationEnabled(), instanceDriver, physicalDevice, &deviceCreateInfo, DE_NULL/*pAllocator*/, &devices[deviceNdx]);

			if (result != VK_SUCCESS)
			{
				resultCollector.fail("Failed to create Device No." + de::toString(deviceNdx) + ", Error Code: " + de::toString(result));
				break;
			}

			{
				const DeviceDriver	deviceDriver	(platformInterface, instance, devices[deviceNdx]);
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
				DeviceDriver deviceDriver(platformInterface, instance, devices[deviceNdx]);
				deviceDriver.destroyDevice(devices[deviceNdx], DE_NULL/*pAllocator*/);
			}
		}

		throw;
	}

	for (int deviceNdx = (int)devices.size()-1; deviceNdx >= 0; deviceNdx--)
	{
		if (devices[deviceNdx] != (VkDevice)DE_NULL)
		{
			DeviceDriver deviceDriver(platformInterface, instance, devices[deviceNdx]);
			deviceDriver.destroyDevice(devices[deviceNdx], DE_NULL/*pAllocator*/);
		}
	}

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createDeviceWithUnsupportedExtensionsTest (Context& context)
{
	tcu::TestLog&					log						= context.getTestContext().getLog();
	const PlatformInterface&		platformInterface		= context.getPlatformInterface();
	const CustomInstance			instance				(createCustomInstanceFromContext(context, DE_NULL, false));
	const InstanceDriver&			instanceDriver			(instance.getDriver());
	const char*						enabledExtensions[]		= {"VK_UNSUPPORTED_EXTENSION", "THIS_IS_NOT_AN_EXTENSION", "VK_DONT_SUPPORT_ME"};
	const VkPhysicalDevice			physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
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
		const VkResult	result		= createUncheckedDevice(context.getTestContext().getCommandLine().isValidationEnabled(), instanceDriver, physicalDevice, &deviceCreateInfo, DE_NULL/*pAllocator*/, &device);
		const bool		gotDevice	= !!device;

		if (device)
		{
			const DeviceDriver	deviceIface	(platformInterface, instance, device);
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
	const CustomInstance					instance				(createCustomInstanceFromContext(context));
	const InstanceDriver&					instanceDriver			(instance.getDriver());
	const VkPhysicalDevice					physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
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

		const Unique<VkDevice>			device				(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
		const DeviceDriver				deviceDriver		(platformInterface, instance, device.get());
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

void checkGlobalPrioritySupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_global_priority");
}

tcu::TestStatus createDeviceWithGlobalPriorityTest (Context& context)
{
	tcu::TestLog&							log						= context.getTestContext().getLog();
	const PlatformInterface&				platformInterface		= context.getPlatformInterface();
	const CustomInstance					instance				(createCustomInstanceFromContext(context));
	const InstanceDriver&					instanceDriver			(instance.getDriver());
	const VkPhysicalDevice					physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
	const vector<float>						queuePriorities			(1, 1.0f);
	const VkQueueGlobalPriorityEXT			globalPriorities[]		= { VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT, VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT, VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT, VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT };

	for (VkQueueGlobalPriorityEXT globalPriority : globalPriorities)
	{
		const VkDeviceQueueGlobalPriorityCreateInfoEXT	queueGlobalPriority		=
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,	//sType;
			DE_NULL,														//pNext;
			globalPriority													//globalPriority;
		};

		const VkDeviceQueueCreateInfo	queueCreateInfo		=
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//sType;
			&queueGlobalPriority,						//pNext;
			(VkDeviceQueueCreateFlags)0u,				//flags;
			0,											//queueFamilyIndex;
			1,											//queueCount;
			queuePriorities.data()						//pQueuePriorities;
		};

		const VkDeviceCreateInfo		deviceCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//sType;
			DE_NULL,								//pNext;
			(VkDeviceCreateFlags)0u,				//flags;
			1,										//queueRecordCount;
			&queueCreateInfo,						//pRequestedQueues;
			0,										//layerCount;
			DE_NULL,								//ppEnabledLayerNames;
			0,										//extensionCount;
			DE_NULL,								//ppEnabledExtensionNames;
			DE_NULL,								//pEnabledFeatures;
		};

		const bool		mayBeDenied				= globalPriority > VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT;

		try
		{
			const Unique<VkDevice>		device				(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
			const DeviceDriver			deviceDriver		(platformInterface, instance, device.get());
			const deUint32				queueFamilyIndex	= deviceCreateInfo.pQueueCreateInfos->queueFamilyIndex;
			const VkQueue				queue				= getDeviceQueue(deviceDriver, *device, queueFamilyIndex, 0);
			VkResult					result;

			TCU_CHECK(!!queue);

			result = deviceDriver.queueWaitIdle(queue);
			if (result == VK_ERROR_NOT_PERMITTED_EXT && mayBeDenied)
			{
				continue;
			}

			if (result != VK_SUCCESS)
			{
				log << TestLog::Message
					<< "vkQueueWaitIdle failed"
					<< ", globalPriority = " << globalPriority
					<< ", queueCreateInfo " << queueCreateInfo
					<< ", Error Code: " << result
					<< TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
		}
		catch (const Error& error)
		{
			if (error.getError() == VK_ERROR_NOT_PERMITTED_EXT && mayBeDenied)
			{
				continue;
			}
			else
			{
				log << TestLog::Message
					<< "exception thrown " << error.getMessage()
					<< ", globalPriority = " << globalPriority
					<< ", queueCreateInfo " << queueCreateInfo
					<< ", Error Code: " << error.getError()
					<< TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus createDeviceFeatures2Test (Context& context)
{
	const PlatformInterface&				vkp						= context.getPlatformInterface();
	const CustomInstance					instance				(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
	const InstanceDriver&					vki						(instance.getDriver());
	const VkPhysicalDevice					physicalDevice			= chooseDevice(vki, instance, context.getTestContext().getCommandLine());
	const deUint32							queueFamilyIndex		= 0;
	const deUint32							queueCount				= 1;
	const deUint32							queueIndex				= 0;
	const float								queuePriority			= 1.0f;
	const vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);

	VkPhysicalDeviceFeatures2		enabledFeatures;
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
		0u,
		DE_NULL,
		0,
		DE_NULL,
		DE_NULL,
	};

	// Populate enabledFeatures
	enabledFeatures.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledFeatures.pNext		= DE_NULL;

	vki.getPhysicalDeviceFeatures2(physicalDevice, &enabledFeatures);

	{
		const Unique<VkDevice>	device		(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &deviceCreateInfo));
		const DeviceDriver		vkd			(vkp, instance, device.get());
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
	const CustomInstance		instance				(createCustomInstanceFromContext(context, DE_NULL, false));
	const InstanceDriver&		instanceDriver			(instance.getDriver());
	const VkPhysicalDevice		physicalDevice			= chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
	const deUint32				queueFamilyIndex		= 0;
	const deUint32				queueCount				= 1;
	const float					queuePriority			= 1.0f;
	VkPhysicalDeviceFeatures	physicalDeviceFeatures;

	const vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

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
			0u,
			DE_NULL,
			0,
			DE_NULL,
			&enabledFeatures
		};

		VkDevice		device;
		const VkResult	res	= createUncheckedDevice(false, instanceDriver, physicalDevice, &deviceCreateInfo, DE_NULL, &device);

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

tcu::TestStatus createDeviceQueue2Test (Context& context)
{
	if (!context.contextSupports(vk::ApiVersion(1, 1, 0)))
		TCU_THROW(NotSupportedError, "Vulkan 1.1 is not supported");

	const PlatformInterface&				platformInterface		= context.getPlatformInterface();
	const VkInstance						instance				= context.getInstance();
	const InstanceInterface&				instanceDriver			= context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= context.getPhysicalDevice();
	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const deUint32							queueCount				= 1;
	const deUint32							queueIndex				= 0;
	const float								queuePriority			= 1.0f;

	VkPhysicalDeviceProtectedMemoryFeatures	protectedMemoryFeature	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,	// VkStructureType					sType;
		DE_NULL,														// void*							pNext;
		VK_FALSE														// VkBool32							protectedMemory;
	};

	VkPhysicalDeviceFeatures2				features2;
	deMemset(&features2, 0, sizeof(features2));
	features2.sType													= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext													= &protectedMemoryFeature;

	instanceDriver.getPhysicalDeviceFeatures2(physicalDevice, &features2);
	if (protectedMemoryFeature.protectedMemory == VK_FALSE)
		TCU_THROW(NotSupportedError, "Protected memory feature is not supported");

	const VkDeviceQueueCreateInfo			deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,						// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,							// VkDeviceQueueCreateFlags			flags;
		queueFamilyIndex,												// deUint32							queueFamilyIndex;
		queueCount,														// deUint32							queueCount;
		&queuePriority,													// const float*						pQueuePriorities;
	};
	const VkDeviceCreateInfo				deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							// VkStructureType					sType;
		&features2,														// const void*						pNext;
		(VkDeviceCreateFlags)0u,										// VkDeviceCreateFlags				flags;
		1,																// deUint32							queueCreateInfoCount;
		&deviceQueueCreateInfo,											// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0,																// deUint32							enabledLayerCount;
		DE_NULL,														// const char* const*				ppEnabledLayerNames;
		0,																// deUint32							enabledExtensionCount;
		DE_NULL,														// const char* const*				ppEnabledExtensionNames;
		DE_NULL,														// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	const VkDeviceQueueInfo2				deviceQueueInfo2		=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,							// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,							// VkDeviceQueueCreateFlags			flags;
		queueFamilyIndex,												// deUint32							queueFamilyIndex;
		queueIndex,														// deUint32							queueIndex;
	};

	{
		const Unique<VkDevice>				device					(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
		const DeviceDriver					deviceDriver			(platformInterface, instance, device.get());
		const VkQueue						queue2					= getDeviceQueue2(deviceDriver, *device, &deviceQueueInfo2);

		VK_CHECK(deviceDriver.queueWaitIdle(queue2));
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus createDeviceQueue2UnmatchedFlagsTest (Context& context)
{
	if (!context.contextSupports(vk::ApiVersion(1, 1, 0)))
		TCU_THROW(NotSupportedError, "Vulkan 1.1 is not supported");

	const PlatformInterface&		platformInterface		= context.getPlatformInterface();
	const VkInstance				instance				= context.getInstance();
	const InstanceInterface&		instanceDriver			= context.getInstanceInterface();
	const VkPhysicalDevice			physicalDevice			= context.getPhysicalDevice();

	// Check if VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT flag can be used.
	{
		VkPhysicalDeviceProtectedMemoryFeatures		protectedFeatures;
		protectedFeatures.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
		protectedFeatures.pNext		= DE_NULL;

		VkPhysicalDeviceFeatures2					deviceFeatures;
		deviceFeatures.sType		= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures.pNext		= &protectedFeatures;

		instanceDriver.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
		if (!protectedFeatures.protectedMemory)
		{
			TCU_THROW(NotSupportedError, "protectedMemory feature is not supported, no queue creation flags available");
		}
	}

	const deUint32							queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const deUint32							queueCount				= 1;
	const deUint32							queueIndex				= 0;
	const float								queuePriority			= 1.0f;
	const VkDeviceQueueCreateInfo			deviceQueueCreateInfo	=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkDeviceQueueCreateFlags)0u,				// VkDeviceQueueCreateFlags			flags;
		queueFamilyIndex,							// deUint32							queueFamilyIndex;
		queueCount,									// deUint32							queueCount;
		&queuePriority,								// const float*						pQueuePriorities;
	};
	VkPhysicalDeviceProtectedMemoryFeatures	protectedFeatures		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,	// VkStructureType				sType;
		DE_NULL,														// void*						pNext;
		VK_TRUE															// VkBool32						protectedMemory;
	};

	VkPhysicalDeviceFeatures				emptyDeviceFeatures;
	deMemset(&emptyDeviceFeatures, 0, sizeof(emptyDeviceFeatures));

	const VkPhysicalDeviceFeatures2			deviceFeatures			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,					// VkStructureType				sType;
		&protectedFeatures,												// void*						pNext;
		emptyDeviceFeatures												// VkPhysicalDeviceFeatures		features;
	};

	const VkDeviceCreateInfo				deviceCreateInfo		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		// VkStructureType					sType;
		&deviceFeatures,							// const void*						pNext;
		(VkDeviceCreateFlags)0u,					// VkDeviceCreateFlags				flags;
		1,											// deUint32							queueCreateInfoCount;
		&deviceQueueCreateInfo,						// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0,											// deUint32							enabledLayerCount;
		DE_NULL,									// const char* const*				ppEnabledLayerNames;
		0,											// deUint32							enabledExtensionCount;
		DE_NULL,									// const char* const*				ppEnabledExtensionNames;
		DE_NULL,									// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	const VkDeviceQueueInfo2				deviceQueueInfo2		=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,		// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,		// VkDeviceQueueCreateFlags			flags;
		queueFamilyIndex,							// deUint32							queueFamilyIndex;
		queueIndex,									// deUint32							queueIndex;
	};

	{
		const Unique<VkDevice>		device					(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
		const DeviceDriver			deviceDriver			(platformInterface, instance, device.get());
		const VkQueue				queue2					= getDeviceQueue2(deviceDriver, *device, &deviceQueueInfo2);

		if (queue2 != DE_NULL)
			return tcu::TestStatus::fail("Fail, getDeviceQueue2 should return VK_NULL_HANDLE when flags in VkDeviceQueueCreateInfo and VkDeviceQueueInfo2 are different.");

		const VkQueue				queue					= getDeviceQueue(deviceDriver, *device,  queueFamilyIndex, queueIndex);

		VK_CHECK(deviceDriver.queueWaitIdle(queue));
	}

	return tcu::TestStatus::pass("Pass");
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
size_t					g_allocationsCount			= 0;

void freeAllocTracker (void)
{
	g_allocatedVector.clear();
	g_allocationsCount = 0;
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

	g_allocationsCount = 0;
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

			g_allocationsCount++;
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
		VK_API_VERSION_1_0						// apiVersion
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
	VkResult					result				= VK_SUCCESS;
	size_t						max_allowed_alloc	= 0;

	do
	{
		if (max_allowed_alloc == 0)
		{
			if (result != VK_SUCCESS)
				return tcu::TestStatus::fail("Could not create instance and device");

			initAllocTracker(99999);
		}
		else
		{
			initAllocTracker(max_allowed_alloc, failIndex++);

			if (failIndex >= static_cast<deUint32>(max_allowed_alloc))
				return tcu::TestStatus::fail("Out of retries, could not create instance and device");
		}

		// if the number of allocations the driver makes is large, we may end up
		// taking more than the watchdog timeout. touch here to avoid spurious
		// failures.
		if (failIndex % 128 == 0)
			context.getTestContext().touchWatchdog();

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

		result = createUncheckedDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vki, physicalDevices[chosenDevice], &deviceCreateInfo, &allocationCallbacks, &device);

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

		DeviceDriver(vkp, instance, device).destroyDevice(device, &allocationCallbacks);
		vki.destroyInstance(instance, &allocationCallbacks);
		if (max_allowed_alloc == 0)
		{
			max_allowed_alloc	= g_allocationsCount + 100;
			result				= VK_ERROR_OUT_OF_HOST_MEMORY;
		}
		freeAllocTracker();
	}
	while (result == VK_ERROR_OUT_OF_HOST_MEMORY);

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
	addFunctionCase(deviceInitializationTests.get(), "create_instance_extension_name_abuse",			"", createInstanceWithExtensionNameAbuseTest);
	addFunctionCase(deviceInitializationTests.get(), "create_instance_layer_name_abuse",				"", createInstanceWithLayerNameAbuseTest);
	addFunctionCase(deviceInitializationTests.get(), "enumerate_devices_alloc_leak",					"", enumerateDevicesAllocLeakTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device",									"", createDeviceTest);
	addFunctionCase(deviceInitializationTests.get(), "create_multiple_devices",							"", createMultipleDevicesTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_unsupported_extensions",			"", createDeviceWithUnsupportedExtensionsTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_various_queue_counts",				"", createDeviceWithVariousQueueCountsTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_global_priority",					"", checkGlobalPrioritySupport, createDeviceWithGlobalPriorityTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_features2",							"", createDeviceFeatures2Test);
	addFunctionCase(deviceInitializationTests.get(), "create_device_unsupported_features",				"", createDeviceWithUnsupportedFeaturesTest);
	addFunctionCase(deviceInitializationTests.get(), "create_device_queue2",							"", createDeviceQueue2Test);
	addFunctionCase(deviceInitializationTests.get(), "create_device_queue2_unmatched_flags",			"", createDeviceQueue2UnmatchedFlagsTest);
	addFunctionCase(deviceInitializationTests.get(), "create_instance_device_intentional_alloc_fail",	"", createInstanceDeviceIntentionalAllocFail);

	return deviceInitializationTests.release();
}

} // api
} // vkt
