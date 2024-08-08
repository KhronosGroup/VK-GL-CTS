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
#include "vkDeviceFeatures.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <limits>
#include <numeric>
#include <vector>
#include <set>

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

tcu::TestStatus createInstanceTest(Context &context)
{
    tcu::TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector resultCollector(log);
    const char *appNames[]       = {"appName",   nullptr,      "", "app, name", "app(\"name\"", "app~!@#$%^&*()_+name",
                                    "app\nName", "app\r\nName"};
    const char *engineNames[]    = {"engineName",   nullptr,          "",
                                    "engine. name", "engine\"(name)", "eng~!@#$%^&*()_+name",
                                    "engine\nName", "engine\r\nName"};
    const int patchNumbers[]     = {0, 1, 2, 3, 4, 5, 13, 4094, 4095};
    const uint32_t appVersions[] = {0, 1, (uint32_t)-1};
    const uint32_t engineVersions[] = {0, 1, (uint32_t)-1};
    const uint32_t apiVersion       = context.getUsedApiVersion();
    vector<VkApplicationInfo> appInfos;

    // test over appName
    for (int appNameNdx = 0; appNameNdx < DE_LENGTH_OF_ARRAY(appNames); appNameNdx++)
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            appNames[appNameNdx],               // const char* pAppName;
            0u,                                 // uint32_t appVersion;
            "engineName",                       // const char* pEngineName;
            0u,                                 // uint32_t engineVersion;
            apiVersion,                         // uint32_t apiVersion;
        };

        appInfos.push_back(appInfo);
    }

    // test over engineName
    for (int engineNameNdx = 0; engineNameNdx < DE_LENGTH_OF_ARRAY(engineNames); engineNameNdx++)
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            "appName",                          // const char* pAppName;
            0u,                                 // uint32_t appVersion;
            engineNames[engineNameNdx],         // const char* pEngineName;
            0u,                                 // uint32_t engineVersion;
            apiVersion,                         // uint32_t apiVersion;
        };

        appInfos.push_back(appInfo);
    }

    // test over appVersion
    for (int appVersionNdx = 0; appVersionNdx < DE_LENGTH_OF_ARRAY(appVersions); appVersionNdx++)
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            "appName",                          // const char* pAppName;
            appVersions[appVersionNdx],         // uint32_t appVersion;
            "engineName",                       // const char* pEngineName;
            0u,                                 // uint32_t engineVersion;
            apiVersion,                         // uint32_t apiVersion;
        };

        appInfos.push_back(appInfo);
    }

    // test over engineVersion
    for (int engineVersionNdx = 0; engineVersionNdx < DE_LENGTH_OF_ARRAY(engineVersions); engineVersionNdx++)
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            "appName",                          // const char* pAppName;
            0u,                                 // uint32_t appVersion;
            "engineName",                       // const char* pEngineName;
            engineVersions[engineVersionNdx],   // uint32_t engineVersion;
            apiVersion,                         // uint32_t apiVersion;
        };

        appInfos.push_back(appInfo);
    }

    const uint32_t variantNum = unpackVersion(apiVersion).variantNum;
    const uint32_t majorNum   = unpackVersion(apiVersion).majorNum;
    const uint32_t minorNum   = unpackVersion(apiVersion).minorNum;

    // patch component of api version checking (should be ignored by implementation)
    for (int patchVersion = 0; patchVersion < DE_LENGTH_OF_ARRAY(patchNumbers); patchVersion++)
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,                                              // VkStructureType sType;
            nullptr,                                                                         // const void* pNext;
            "appName",                                                                       // const char* pAppName;
            0u,                                                                              // uint32_t appVersion;
            "engineName",                                                                    // const char* pEngineName;
            0u,                                                                              // uint32_t engineVersion;
            VK_MAKE_API_VERSION(variantNum, majorNum, minorNum, patchNumbers[patchVersion]), // uint32_t apiVersion;
        };

        appInfos.push_back(appInfo);
    }

    // test when apiVersion is 0
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            "appName",                          // const char* pAppName;
            0u,                                 // uint32_t appVersion;
            "engineName",                       // const char* pEngineName;
            0u,                                 // uint32_t engineVersion;
            0u,                                 // uint32_t apiVersion;
        };

        appInfos.push_back(appInfo);
    }

    // run the tests!
    for (size_t appInfoNdx = 0; appInfoNdx < appInfos.size(); ++appInfoNdx)
    {
        const VkApplicationInfo &appInfo              = appInfos[appInfoNdx];
        const VkInstanceCreateInfo instanceCreateInfo = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            (VkInstanceCreateFlags)0u,              // VkInstanceCreateFlags flags;
            &appInfo,                               // const VkApplicationInfo* pAppInfo;
            0u,                                     // uint32_t layerCount;
            nullptr,                                // const char*const* ppEnabledLayernames;
            0u,                                     // uint32_t extensionCount;
            nullptr,                                // const char*const* ppEnabledExtensionNames;
        };

        log << TestLog::Message << "Creating instance with appInfo: " << appInfo << TestLog::EndMessage;

        try
        {
            CustomInstance instance = createCustomInstanceFromInfo(context, &instanceCreateInfo);
            log << TestLog::Message << "Succeeded" << TestLog::EndMessage;
        }
        catch (const vk::Error &err)
        {
            resultCollector.fail("Failed, Error code: " + de::toString(err.getMessage()));
        }
    }

    return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithInvalidApiVersionTest(Context &context)
{
    tcu::TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector resultCollector(log);
    const PlatformInterface &platformInterface = context.getPlatformInterface();

    uint32_t instanceApiVersion = 0u;
    platformInterface.enumerateInstanceVersion(&instanceApiVersion);

    const ApiVersion apiVersion = unpackVersion(instanceApiVersion);

    const uint32_t invalidApiVariant   = (1 << 3) - 1;
    const uint32_t invalidMajorVersion = (1 << 7) - 1;
    const uint32_t invalidMinorVersion = (1 << 10) - 1;
    vector<ApiVersion> invalidApiVersions;

    invalidApiVersions.push_back(
        ApiVersion(invalidApiVariant, invalidMajorVersion, apiVersion.minorNum, apiVersion.patchNum));
    invalidApiVersions.push_back(
        ApiVersion(apiVersion.variantNum, invalidMajorVersion, apiVersion.minorNum, apiVersion.patchNum));
    invalidApiVersions.push_back(
        ApiVersion(apiVersion.variantNum, apiVersion.majorNum, invalidMinorVersion, apiVersion.patchNum));
#ifdef CTS_USES_VULKANSC
    invalidApiVersions.push_back(
        ApiVersion(invalidApiVariant, apiVersion.majorNum, apiVersion.minorNum, apiVersion.patchNum));
    invalidApiVersions.push_back(ApiVersion(0, apiVersion.majorNum, apiVersion.minorNum, apiVersion.patchNum));
#endif // CTS_USES_VULKANSC

    for (size_t apiVersionNdx = 0; apiVersionNdx < invalidApiVersions.size(); apiVersionNdx++)
    {
        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,      // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            "appName",                               // const char* pAppName;
            0u,                                      // uint32_t appVersion;
            "engineName",                            // const char* pEngineName;
            0u,                                      // uint32_t engineVersion;
            pack(invalidApiVersions[apiVersionNdx]), // uint32_t apiVersion;
        };
        const VkInstanceCreateInfo instanceCreateInfo = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            (VkInstanceCreateFlags)0u,              // VkInstanceCreateFlags flags;
            &appInfo,                               // const VkApplicationInfo* pAppInfo;
            0u,                                     // uint32_t layerCount;
            nullptr,                                // const char*const* ppEnabledLayernames;
            0u,                                     // uint32_t extensionCount;
            nullptr,                                // const char*const* ppEnabledExtensionNames;
        };

        log << TestLog::Message << "API version reported by enumerateInstanceVersion: " << apiVersion
            << ", api version used to create instance: " << invalidApiVersions[apiVersionNdx] << TestLog::EndMessage;

        {
            UncheckedInstance instance;
            const VkResult result = createUncheckedInstance(context, &instanceCreateInfo, nullptr, &instance);

#ifdef CTS_USES_VULKANSC
            if (invalidApiVersions[apiVersionNdx].variantNum == apiVersion.variantNum)
#else
            if (apiVersion.majorNum == 1 && apiVersion.minorNum == 0)
            {
                if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
                {
                    TCU_CHECK(!static_cast<bool>(instance));
                    log << TestLog::Message << "Pass, instance creation with invalid apiVersion is rejected"
                        << TestLog::EndMessage;
                }
                else
                    resultCollector.fail("Fail, instance creation with invalid apiVersion is not rejected");
            }
            else if (apiVersion.majorNum == 1 && apiVersion.minorNum >= 1)
#endif // CTS_USES_VULKANSC
            {
                if (result == VK_SUCCESS)
                {
                    TCU_CHECK(static_cast<bool>(instance));
                    log << TestLog::Message << "Pass, instance creation with nonstandard apiVersion succeeds for "
                        << ((apiVersion.variantNum == 0) ? "Vulkan 1.1" : "Vulkan SC when api variant is correct")
                        << TestLog::EndMessage;
                }
                else if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
                {
                    std::ostringstream message;
                    message << "Fail, instance creation must not return VK_ERROR_INCOMPATIBLE_DRIVER for "
                            << ((apiVersion.variantNum == 0) ? "Vulkan 1.1" : "Vulkan SC when api variant is correct");
                    resultCollector.fail(message.str().c_str());
                }
                else
                {
                    std::ostringstream message;
                    message << "Fail, createInstance failed with " << result;
                    resultCollector.fail(message.str().c_str());
                }
            }
#ifdef CTS_USES_VULKANSC
            else if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
            {
                TCU_CHECK(!static_cast<bool>(instance));
                log << TestLog::Message << "Pass, instance creation with invalid apiVersion is rejected"
                    << TestLog::EndMessage;
            }
            else
                resultCollector.fail("Fail, instance creation with invalid apiVersion is not rejected");
#endif // CTS_USES_VULKANSC
        }
    }

    return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithNullApplicationInfoTest(Context &context)
{
    tcu::TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector resultCollector(log);

    const VkInstanceCreateInfo instanceCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        (VkInstanceCreateFlags)0u,              // VkInstanceCreateFlags flags;
        nullptr,                                // const VkApplicationInfo* pAppInfo;
        0u,                                     // uint32_t layerCount;
        nullptr,                                // const char*const* ppEnabledLayernames;
        0u,                                     // uint32_t extensionCount;
        nullptr,                                // const char*const* ppEnabledExtensionNames;
    };

    log << TestLog::Message << "Creating instance with NULL pApplicationInfo" << TestLog::EndMessage;

    try
    {
        CustomInstance instance = createCustomInstanceFromInfo(context, &instanceCreateInfo);
        log << TestLog::Message << "Succeeded" << TestLog::EndMessage;
    }
    catch (const vk::Error &err)
    {
        resultCollector.fail("Failed, Error code: " + de::toString(err.getMessage()));
    }

    return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createInstanceWithUnsupportedExtensionsTest(Context &context)
{
    tcu::TestLog &log               = context.getTestContext().getLog();
    const char *enabledExtensions[] = {"VK_UNSUPPORTED_EXTENSION", "THIS_IS_NOT_AN_EXTENSION"};
    const uint32_t apiVersion       = context.getUsedApiVersion();
    const VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
        nullptr,                            // const void* pNext;
        "appName",                          // const char* pAppName;
        0u,                                 // uint32_t appVersion;
        "engineName",                       // const char* pEngineName;
        0u,                                 // uint32_t engineVersion;
        apiVersion,                         // uint32_t apiVersion;
    };

    const VkInstanceCreateInfo instanceCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        (VkInstanceCreateFlags)0u,              // VkInstanceCreateFlags flags;
        &appInfo,                               // const VkApplicationInfo* pAppInfo;
        0u,                                     // uint32_t layerCount;
        nullptr,                                // const char*const* ppEnabledLayernames;
        DE_LENGTH_OF_ARRAY(enabledExtensions),  // uint32_t extensionCount;
        enabledExtensions,                      // const char*const* ppEnabledExtensionNames;
    };

    log << TestLog::Message << "Enabled extensions are: " << TestLog::EndMessage;

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(enabledExtensions); ndx++)
        log << TestLog::Message << enabledExtensions[ndx] << TestLog::EndMessage;

    {
        UncheckedInstance instance;
        const VkResult result = createUncheckedInstance(context, &instanceCreateInfo, nullptr, &instance);

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

string getUTF8AbuseString(int index)
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
        return string("Illegal bytes in UTF-8: "
                      "\xc0 \xc1 \xf5 \xf6 \xf7 \xf8 \xf9 \xfa \xfb \xfc \xfd \xfe \xff"
                      "illegal surrogates: \xed\xad\xbf \xed\xbe\x80");

    case UTF8ABUSE_OVERLONGNUL:
        // Zero encoded as overlong, not exactly legal but often supported to differentiate from terminating zero
        return string("UTF-8 encoded nul \xC0\x80 (should not end name)");

    case UTF8ABUSE_OVERLONG:
        // Some overlong encodings
        return string("UTF-8 overlong \xF0\x82\x82\xAC \xfc\x83\xbf\xbf\xbf\xbf \xf8\x87\xbf\xbf\xbf "
                      "\xf0\x8f\xbf\xbf");

    case UTF8ABUSE_ZALGO:
        // Internet "zalgo" meme "bleeding text"
        return string("\x56\xcc\xb5\xcc\x85\xcc\x94\xcc\x88\xcd\x8a\xcc\x91\xcc\x88\xcd\x91\xcc\x83\xcd\x82"
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
        return string("\xe8\xaf\xbb\xe4\xb9\xa6\xe9\xa1\xbb\xe7\x94\xa8\xe6\x84\x8f\xef\xbc\x8c\xe4\xb8\x80"
                      "\xe5\xad\x97\xe5\x80\xbc\xe5\x8d\x83\xe9\x87\x91\x20");

    default:
        DE_ASSERT(index == UTF8ABUSE_EMPTY);
        // Also try an empty string.
        return string("");
    }
}

tcu::TestStatus createInstanceWithExtensionNameAbuseTest(Context &context)
{
    const char *extensionList[1] = {0};
    const uint32_t apiVersion    = context.getUsedApiVersion();
    uint32_t failCount           = 0;

    for (int i = 0; i < UTF8ABUSE_MAX; i++)
    {
        string abuseString = getUTF8AbuseString(i);
        extensionList[0]   = abuseString.c_str();

        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            "appName",                          // const char* pAppName;
            0u,                                 // uint32_t appVersion;
            "engineName",                       // const char* pEngineName;
            0u,                                 // uint32_t engineVersion;
            apiVersion,                         // uint32_t apiVersion;
        };

        const VkInstanceCreateInfo instanceCreateInfo = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            (VkInstanceCreateFlags)0u,              // VkInstanceCreateFlags flags;
            &appInfo,                               // const VkApplicationInfo* pAppInfo;
            0u,                                     // uint32_t layerCount;
            nullptr,                                // const char*const* ppEnabledLayernames;
            1u,                                     // uint32_t extensionCount;
            extensionList,                          // const char*const* ppEnabledExtensionNames;
        };

        {
            UncheckedInstance instance;
            const VkResult result = createUncheckedInstance(context, &instanceCreateInfo, nullptr, &instance);

            if (result != VK_ERROR_EXTENSION_NOT_PRESENT)
                failCount++;

            TCU_CHECK(!static_cast<bool>(instance));
        }
    }

    if (failCount > 0)
        return tcu::TestStatus::fail("Fail, creating instances with unsupported extensions succeeded.");

    return tcu::TestStatus::pass("Pass, creating instances with unsupported extensions were rejected.");
}

tcu::TestStatus createInstanceWithLayerNameAbuseTest(Context &context)
{
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const char *layerList[1]                   = {0};
    const uint32_t apiVersion                  = context.getUsedApiVersion();
    uint32_t failCount                         = 0;

    for (int i = 0; i < UTF8ABUSE_MAX; i++)
    {
        string abuseString = getUTF8AbuseString(i);
        layerList[0]       = abuseString.c_str();

        const VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            "appName",                          // const char* pAppName;
            0u,                                 // uint32_t appVersion;
            "engineName",                       // const char* pEngineName;
            0u,                                 // uint32_t engineVersion;
            apiVersion,                         // uint32_t apiVersion;
        };

        const VkInstanceCreateInfo instanceCreateInfo = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            (VkInstanceCreateFlags)0u,              // VkInstanceCreateFlags flags;
            &appInfo,                               // const VkApplicationInfo* pAppInfo;
            1u,                                     // uint32_t layerCount;
            layerList,                              // const char*const* ppEnabledLayernames;
            0u,                                     // uint32_t extensionCount;
            nullptr,                                // const char*const* ppEnabledExtensionNames;
        };

        {
            VkInstance instance = (VkInstance)0;
            const VkResult result =
                platformInterface.createInstance(&instanceCreateInfo, nullptr /*pAllocator*/, &instance);
            const bool gotInstance = !!instance;

            if (instance)
            {
                const InstanceDriver instanceIface(platformInterface, instance);
                instanceIface.destroyInstance(instance, nullptr /*pAllocator*/);
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

#ifndef CTS_USES_VULKANSC
tcu::TestStatus enumerateDevicesAllocLeakTest(Context &context)
{
    // enumeratePhysicalDevices uses instance-provided allocator
    // and this test checks if all alocated memory is freed

    typedef AllocationCallbackRecorder::RecordIterator RecordIterator;

    DeterministicFailAllocator objAllocator(getSystemAllocator(), DeterministicFailAllocator::MODE_DO_NOT_COUNT, 0);
    AllocationCallbackRecorder recorder(objAllocator.getCallbacks(), 128);
    const auto instance = createCustomInstanceFromContext(context, recorder.getCallbacks(), true);
    const auto &vki     = instance.getDriver();
    vector<VkPhysicalDevice> devices(enumeratePhysicalDevices(vki, instance));
    RecordIterator recordToCheck(recorder.getRecordsEnd());

    try
    {
        devices = enumeratePhysicalDevices(vki, instance);
    }
    catch (const vk::OutOfMemoryError &e)
    {
        if (e.getError() != VK_ERROR_OUT_OF_HOST_MEMORY)
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Got out of memory error - leaks in enumeratePhysicalDevices not tested.");
    }

    // make sure that same number of allocations and frees was done
    int32_t allocationRecords(0);
    RecordIterator lastRecordToCheck(recorder.getRecordsEnd());
    while (recordToCheck != lastRecordToCheck)
    {
        const AllocationCallbackRecord &record = *recordToCheck;
        switch (record.type)
        {
        case AllocationCallbackRecord::TYPE_ALLOCATION:
            ++allocationRecords;
            break;
        case AllocationCallbackRecord::TYPE_FREE:
            if (record.data.free.mem != nullptr)
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
#endif // CTS_USES_VULKANSC

tcu::TestStatus createDeviceTest(Context &context)
{
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const CustomInstance instance(createCustomInstanceFromContext(context));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const uint32_t queueFamilyIndex = 0;
    const uint32_t queueCount       = 1;
    const uint32_t queueIndex       = 0;
    const float queuePriority       = 1.0f;

    const vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        (VkDeviceQueueCreateFlags)0u,
        queueFamilyIndex, //queueFamilyIndex;
        queueCount,       //queueCount;
        &queuePriority,   //pQueuePriorities;
    };

    void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
        pNext,                                //pNext;
        (VkDeviceCreateFlags)0u,
        1,                      //queueRecordCount;
        &deviceQueueCreateInfo, //pRequestedQueues;
        0,                      //layerCount;
        nullptr,                //ppEnabledLayerNames;
        0,                      //extensionCount;
        nullptr,                //ppEnabledExtensionNames;
        nullptr,                //pEnabledFeatures;
    };

    const Unique<VkDevice> device(createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                                                     platformInterface, instance, instanceDriver, physicalDevice,
                                                     &deviceCreateInfo));
    const DeviceDriver deviceDriver(platformInterface, instance, device.get(), context.getUsedApiVersion(),
                                    context.getTestContext().getCommandLine());
    const VkQueue queue = getDeviceQueue(deviceDriver, *device, queueFamilyIndex, queueIndex);

    VK_CHECK(deviceDriver.queueWaitIdle(queue));

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus createMultipleDevicesTest(Context &context)
{
    tcu::TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector resultCollector(log);
#ifndef CTS_USES_VULKANSC
    const int numDevices = 5;
#else
    const int numDevices = 2;
#endif // CTS_USES_VULKANSC

    const PlatformInterface &platformInterface = context.getPlatformInterface();

    vector<CustomInstance> instances;
    vector<VkDevice> devices(numDevices, VK_NULL_HANDLE);

    try
    {
        for (int deviceNdx = 0; deviceNdx < numDevices; deviceNdx++)
        {
            instances.emplace_back(createCustomInstanceFromContext(context));

            const InstanceDriver &instanceDriver(instances.back().getDriver());
            const VkPhysicalDevice physicalDevice =
                chooseDevice(instanceDriver, instances.back(), context.getTestContext().getCommandLine());
            const vector<VkQueueFamilyProperties> queueFamilyProperties =
                getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
            const uint32_t queueFamilyIndex                     = 0;
            const uint32_t queueCount                           = 1;
            const uint32_t queueIndex                           = 0;
            const float queuePriority                           = 1.0f;
            const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                nullptr,
                (VkDeviceQueueCreateFlags)0u, //flags;
                queueFamilyIndex,             //queueFamilyIndex;
                queueCount,                   //queueCount;
                &queuePriority,               //pQueuePriorities;
            };

            void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
            VkDeviceObjectReservationCreateInfo memReservationInfo =
                context.getTestContext().getCommandLine().isSubProcess() ?
                    context.getResourceInterface()->getStatMax() :
                    resetDeviceObjectReservationCreateInfo();
            memReservationInfo.pNext = pNext;
            pNext                    = &memReservationInfo;

            VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
            sc10Features.pNext                              = pNext;
            pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

            const VkDeviceCreateInfo deviceCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
                pNext,                                //pNext;
                (VkDeviceCreateFlags)0u,
                1,                      //queueRecordCount;
                &deviceQueueCreateInfo, //pRequestedQueues;
                0,                      //layerCount;
                nullptr,                //ppEnabledLayerNames;
                0,                      //extensionCount;
                nullptr,                //ppEnabledExtensionNames;
                nullptr,                //pEnabledFeatures;
            };

            const VkResult result =
                createUncheckedDevice(context.getTestContext().getCommandLine().isValidationEnabled(), instanceDriver,
                                      physicalDevice, &deviceCreateInfo, nullptr /*pAllocator*/, &devices[deviceNdx]);

            if (result != VK_SUCCESS)
            {
                resultCollector.fail("Failed to create Device No." + de::toString(deviceNdx) +
                                     ", Error Code: " + de::toString(result));
                break;
            }

            {
                const DeviceDriver deviceDriver(platformInterface, instances.back(), devices[deviceNdx],
                                                context.getUsedApiVersion(), context.getTestContext().getCommandLine());
                const VkQueue queue = getDeviceQueue(deviceDriver, devices[deviceNdx], queueFamilyIndex, queueIndex);

                VK_CHECK(deviceDriver.queueWaitIdle(queue));
            }
        }
    }
    catch (const vk::Error &error)
    {
        resultCollector.fail(de::toString(error.getError()));
    }
    catch (...)
    {
        for (int deviceNdx = (int)devices.size() - 1; deviceNdx >= 0; deviceNdx--)
        {
            if (devices[deviceNdx] != VK_NULL_HANDLE)
            {
                DeviceDriver deviceDriver(platformInterface, instances[deviceNdx], devices[deviceNdx],
                                          context.getUsedApiVersion(), context.getTestContext().getCommandLine());
                deviceDriver.destroyDevice(devices[deviceNdx], nullptr /*pAllocator*/);
            }
        }

        throw;
    }

    for (int deviceNdx = (int)devices.size() - 1; deviceNdx >= 0; deviceNdx--)
    {
        if (devices[deviceNdx] != VK_NULL_HANDLE)
        {
            DeviceDriver deviceDriver(platformInterface, instances[deviceNdx], devices[deviceNdx],
                                      context.getUsedApiVersion(), context.getTestContext().getCommandLine());
            deviceDriver.destroyDevice(devices[deviceNdx], nullptr /*pAllocator*/);
        }
    }

    return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

tcu::TestStatus createDeviceWithUnsupportedExtensionsTest(Context &context)
{
    tcu::TestLog &log                          = context.getTestContext().getLog();
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const CustomInstance instance(createCustomInstanceFromContext(context, nullptr, false));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const char *enabledExtensions[] = {"VK_UNSUPPORTED_EXTENSION", "THIS_IS_NOT_AN_EXTENSION", "VK_DONT_SUPPORT_ME"};
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const float queuePriority                           = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        (VkDeviceQueueCreateFlags)0u,
        0,              //queueFamilyIndex;
        1,              //queueCount;
        &queuePriority, //pQueuePriorities;
    };

    void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
        pNext,                                //pNext;
        (VkDeviceCreateFlags)0u,
        1,                                     //queueRecordCount;
        &deviceQueueCreateInfo,                //pRequestedQueues;
        0,                                     //layerCount;
        nullptr,                               //ppEnabledLayerNames;
        DE_LENGTH_OF_ARRAY(enabledExtensions), //extensionCount;
        enabledExtensions,                     //ppEnabledExtensionNames;
        nullptr,                               //pEnabledFeatures;
    };

    log << TestLog::Message << "Enabled extensions are: " << TestLog::EndMessage;

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(enabledExtensions); ndx++)
        log << TestLog::Message << enabledExtensions[ndx] << TestLog::EndMessage;

    {
        VkDevice device = VK_NULL_HANDLE;
        const VkResult result =
            createUncheckedDevice(context.getTestContext().getCommandLine().isValidationEnabled(), instanceDriver,
                                  physicalDevice, &deviceCreateInfo, nullptr /*pAllocator*/, &device);
        const bool gotDevice = !!device;

        if (device)
        {
            const DeviceDriver deviceIface(platformInterface, instance, device, context.getUsedApiVersion(),
                                           context.getTestContext().getCommandLine());
            deviceIface.destroyDevice(device, nullptr /*pAllocator*/);
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

uint32_t getGlobalMaxQueueCount(const vector<VkQueueFamilyProperties> &queueFamilyProperties)
{
    uint32_t maxQueueCount = 0;

    for (uint32_t queueFamilyNdx = 0; queueFamilyNdx < (uint32_t)queueFamilyProperties.size(); queueFamilyNdx++)
    {
        maxQueueCount = de::max(maxQueueCount, queueFamilyProperties[queueFamilyNdx].queueCount);
    }

    return maxQueueCount;
}

tcu::TestStatus createDeviceWithVariousQueueCountsTest(Context &context)
{
    tcu::TestLog &log                          = context.getTestContext().getLog();
    const int queueCountDiff                   = 1;
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const CustomInstance instance(createCustomInstanceFromContext(context));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
    const vector<float> queuePriorities(getGlobalMaxQueueCount(queueFamilyProperties), 1.0f);
    vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

    for (uint32_t queueFamilyNdx = 0; queueFamilyNdx < (uint32_t)queueFamilyProperties.size(); queueFamilyNdx++)
    {
        const uint32_t maxQueueCount = queueFamilyProperties[queueFamilyNdx].queueCount;

        for (uint32_t queueCount = 1; queueCount <= maxQueueCount; queueCount += queueCountDiff)
        {
            const VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                             nullptr,
                                                             (VkDeviceQueueCreateFlags)0u,
                                                             queueFamilyNdx,
                                                             queueCount,
                                                             queuePriorities.data()};

            deviceQueueCreateInfos.push_back(queueCreateInfo);
        }
    }

    for (size_t testNdx = 0; testNdx < deviceQueueCreateInfos.size(); testNdx++)
    {
        const VkDeviceQueueCreateInfo &queueCreateInfo = deviceQueueCreateInfos[testNdx];
        void *pNext                                    = nullptr;
#ifdef CTS_USES_VULKANSC
        VkDeviceObjectReservationCreateInfo memReservationInfo =
            context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() :
                                                                       resetDeviceObjectReservationCreateInfo();
        memReservationInfo.pNext = pNext;
        pNext                    = &memReservationInfo;

        VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
        sc10Features.pNext                              = pNext;
        pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
            pNext,                                //pNext;
            (VkDeviceCreateFlags)0u,
            1,                //queueRecordCount;
            &queueCreateInfo, //pRequestedQueues;
            0,                //layerCount;
            nullptr,          //ppEnabledLayerNames;
            0,                //extensionCount;
            nullptr,          //ppEnabledExtensionNames;
            nullptr,          //pEnabledFeatures;
        };

        const Unique<VkDevice> device(
            createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface,
                               instance, instanceDriver, physicalDevice, &deviceCreateInfo));
        const DeviceDriver deviceDriver(platformInterface, instance, device.get(), context.getUsedApiVersion(),
                                        context.getTestContext().getCommandLine());
        const uint32_t queueFamilyIndex = deviceCreateInfo.pQueueCreateInfos->queueFamilyIndex;
        const uint32_t queueCount       = deviceCreateInfo.pQueueCreateInfos->queueCount;

        for (uint32_t queueIndex = 0; queueIndex < queueCount; queueIndex++)
        {
            const VkQueue queue = getDeviceQueue(deviceDriver, *device, queueFamilyIndex, queueIndex);
            VkResult result;

            TCU_CHECK(!!queue);

            result = deviceDriver.queueWaitIdle(queue);
            if (result != VK_SUCCESS)
            {
                log << TestLog::Message << "vkQueueWaitIdle failed"
                    << ",  queueIndex = " << queueIndex << ", queueCreateInfo " << queueCreateInfo
                    << ", Error Code: " << result << TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }
    return tcu::TestStatus::pass("Pass");
}

void checkGlobalPrioritySupport(Context &context, bool useKhrGlobalPriority)
{
    const std::string extName = (useKhrGlobalPriority ? "VK_KHR_global_priority" : "VK_EXT_global_priority");
    context.requireDeviceFunctionality(extName);
}

tcu::TestStatus createDeviceWithGlobalPriorityTest(Context &context, bool useKhrGlobalPriority)
{
    tcu::TestLog &log                          = context.getTestContext().getLog();
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const auto &instanceDriver                 = context.getInstanceInterface();
    const auto instance                        = context.getInstance();
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const vector<float> queuePriorities(1, 1.0f);
    const VkQueueGlobalPriorityEXT globalPriorities[] = {
        VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT, VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT, VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT,
        VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT};

#ifndef CTS_USES_VULKANSC
    uint32_t queueFamilyPropertyCount = ~0u;

    instanceDriver.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, nullptr);
    TCU_CHECK(queueFamilyPropertyCount > 0);

    std::vector<VkQueueFamilyProperties2> queueFamilyProperties2(queueFamilyPropertyCount);
    std::vector<VkQueueFamilyGlobalPriorityPropertiesKHR> globalPriorityProperties(queueFamilyPropertyCount);

    if (useKhrGlobalPriority)
    {
        for (uint32_t ndx = 0; ndx < queueFamilyPropertyCount; ndx++)
        {
            globalPriorityProperties[ndx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR;
            globalPriorityProperties[ndx].pNext = nullptr;
            queueFamilyProperties2[ndx].sType   = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
            queueFamilyProperties2[ndx].pNext   = &globalPriorityProperties[ndx];
        }

        instanceDriver.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount,
                                                               queueFamilyProperties2.data());
        TCU_CHECK((size_t)queueFamilyPropertyCount == queueFamilyProperties2.size());
    }

    std::vector<const char *> enabledExtensions = {"VK_EXT_global_priority"};
    if (useKhrGlobalPriority)
        enabledExtensions = {"VK_KHR_global_priority"};

    VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT globalPriorityQueryFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT, //sType;
        nullptr,                                                              //pNext;
        VK_TRUE                                                               //globalPriorityQuery;
    };
#else
    (void)useKhrGlobalPriority;
    std::vector<const char *> enabledExtensions = {"VK_EXT_global_priority"};
#endif // CTS_USES_VULKANSC

    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        enabledExtensions.emplace_back("VK_KHR_get_physical_device_properties2");
    }

    for (VkQueueGlobalPriorityEXT globalPriority : globalPriorities)
    {
        const VkDeviceQueueGlobalPriorityCreateInfoEXT queueGlobalPriority = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR, //sType;
            nullptr,                                                        //pNext;
            globalPriority                                                  //globalPriority;
        };

        const VkDeviceQueueCreateInfo queueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, //sType;
            &queueGlobalPriority,                       //pNext;
            (VkDeviceQueueCreateFlags)0u,               //flags;
            0,                                          //queueFamilyIndex;
            1,                                          //queueCount;
            queuePriorities.data()                      //pQueuePriorities;
        };

        void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
        VkDeviceObjectReservationCreateInfo memReservationInfo =
            context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() :
                                                                       resetDeviceObjectReservationCreateInfo();
        memReservationInfo.pNext = pNext;
        pNext                    = &memReservationInfo;

        VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
        sc10Features.pNext                              = pNext;
        pNext                                           = &sc10Features;
#else
        pNext = useKhrGlobalPriority ? &globalPriorityQueryFeatures : nullptr;
#endif // CTS_USES_VULKANSC

        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
            pNext,                                //pNext;
            (VkDeviceCreateFlags)0u,              //flags;
            1,                                    //queueRecordCount;
            &queueCreateInfo,                     //pRequestedQueues;
            0,                                    //layerCount;
            nullptr,                              //ppEnabledLayerNames;
            (uint32_t)enabledExtensions.size(),   //extensionCount;
            enabledExtensions.data(),             //ppEnabledExtensionNames;
            nullptr,                              //pEnabledFeatures;
        };

        const bool mayBeDenied = globalPriority > VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT;
#ifndef CTS_USES_VULKANSC
        const bool mustFail =
            useKhrGlobalPriority &&
            (globalPriority < globalPriorityProperties[0].priorities[0] ||
             globalPriority > globalPriorityProperties[0].priorities[globalPriorityProperties[0].priorityCount - 1]);
#endif // CTS_USES_VULKANSC

        try
        {
            const Unique<VkDevice> device(
                createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface,
                                   instance, instanceDriver, physicalDevice, &deviceCreateInfo));
            const DeviceDriver deviceDriver(platformInterface, instance, device.get(), context.getUsedApiVersion(),
                                            context.getTestContext().getCommandLine());
            const uint32_t queueFamilyIndex = deviceCreateInfo.pQueueCreateInfos->queueFamilyIndex;
            const VkQueue queue             = getDeviceQueue(deviceDriver, *device, queueFamilyIndex, 0);
            VkResult result;

            TCU_CHECK(!!queue);

            result = deviceDriver.queueWaitIdle(queue);
            if (result == VK_ERROR_NOT_PERMITTED_KHR && mayBeDenied)
            {
                continue;
            }

#ifndef CTS_USES_VULKANSC
            if (result == VK_ERROR_INITIALIZATION_FAILED && mustFail)
            {
                continue;
            }

            if (mustFail)
            {
                log << TestLog::Message << "device creation must fail but not"
                    << ", globalPriority = " << globalPriority << ", queueCreateInfo " << queueCreateInfo
                    << TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
#endif // CTS_USES_VULKANSC

            if (result != VK_SUCCESS)
            {
                log << TestLog::Message << "vkQueueWaitIdle failed"
                    << ", globalPriority = " << globalPriority << ", queueCreateInfo " << queueCreateInfo
                    << ", Error Code: " << result << TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
        catch (const Error &error)
        {
            if ((error.getError() == VK_ERROR_NOT_PERMITTED_KHR && mayBeDenied)
#ifndef CTS_USES_VULKANSC
                || (error.getError() == VK_ERROR_INITIALIZATION_FAILED && mustFail)
#endif // CTS_USES_VULKANSC
            )
            {
                continue;
            }
            else
            {
                log << TestLog::Message << "exception thrown " << error.getMessage()
                    << ", globalPriority = " << globalPriority << ", queueCreateInfo " << queueCreateInfo
                    << ", Error Code: " << error.getError() << TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
void checkGlobalPriorityQuerySupport(Context &context, bool useKhrGlobalPriority)
{
    const std::string extName = (useKhrGlobalPriority ? "VK_KHR_global_priority" : "VK_EXT_global_priority_query");
    context.requireDeviceFunctionality(extName);
}

bool isValidGlobalPriority(VkQueueGlobalPriorityEXT priority)
{
    switch (priority)
    {
    case VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT:
    case VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT:
    case VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT:
    case VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT:
        return true;
    default:
        return false;
    }
}

void checkGlobalPriorityProperties(const VkQueueFamilyGlobalPriorityPropertiesEXT &properties)
{
    TCU_CHECK(properties.priorityCount > 0);
    TCU_CHECK(properties.priorityCount <= VK_MAX_GLOBAL_PRIORITY_SIZE_KHR);
    TCU_CHECK(isValidGlobalPriority(properties.priorities[0]));

    for (uint32_t ndx = 1; ndx < properties.priorityCount; ndx++)
    {
        TCU_CHECK(isValidGlobalPriority(properties.priorities[ndx]));
        TCU_CHECK(properties.priorities[ndx] == (properties.priorities[ndx - 1] << 1));
    }
}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
tcu::TestStatus createDeviceWithQueriedGlobalPriorityTest(Context &context, bool useKhrGlobalPriority)
{
    tcu::TestLog &log                          = context.getTestContext().getLog();
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const auto &instanceDriver                 = context.getInstanceInterface();
    const auto instance                        = context.getInstance();
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const VkQueueGlobalPriorityEXT globalPriorities[] = {
        VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT, VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT, VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT,
        VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT};
    const vector<float> queuePriorities(1, 1.0f);
    uint32_t queueFamilyPropertyCount = ~0u;

    instanceDriver.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, nullptr);
    TCU_CHECK(queueFamilyPropertyCount > 0);

    std::vector<VkQueueFamilyProperties2> queueFamilyProperties2(queueFamilyPropertyCount);
    std::vector<VkQueueFamilyGlobalPriorityPropertiesEXT> globalPriorityProperties(queueFamilyPropertyCount);

    for (uint32_t ndx = 0; ndx < queueFamilyPropertyCount; ndx++)
    {
        globalPriorityProperties[ndx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT;
        globalPriorityProperties[ndx].pNext = nullptr;
        queueFamilyProperties2[ndx].sType   = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        queueFamilyProperties2[ndx].pNext   = &globalPriorityProperties[ndx];
    }

    instanceDriver.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount,
                                                           queueFamilyProperties2.data());
    TCU_CHECK((size_t)queueFamilyPropertyCount == queueFamilyProperties2.size());

    std::vector<const char *> enabledExtensions = {"VK_EXT_global_priority", "VK_EXT_global_priority_query"};
    if (useKhrGlobalPriority)
        enabledExtensions = {"VK_KHR_global_priority"};

    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        enabledExtensions.emplace_back("VK_KHR_get_physical_device_properties2");
    }

    for (uint32_t ndx = 0; ndx < queueFamilyPropertyCount; ndx++)
    {
        checkGlobalPriorityProperties(globalPriorityProperties[ndx]);

        for (VkQueueGlobalPriorityEXT globalPriority : globalPriorities)
        {
            const VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT globalPriorityQueryFeatures = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT, //sType;
                nullptr,                                                              //pNext;
                VK_TRUE                                                               //globalPriorityQuery;
            };
            const VkDeviceQueueGlobalPriorityCreateInfoEXT queueGlobalPriorityCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT, //sType;
                nullptr,                                                        //pNext;
                globalPriority,                                                 //globalPriority;
            };
            const VkDeviceQueueCreateInfo queueCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, //sType;
                &queueGlobalPriorityCreateInfo,             //pNext;
                (VkDeviceQueueCreateFlags)0u,               //flags;
                ndx,                                        //queueFamilyIndex;
                1,                                          //queueCount;
                queuePriorities.data()                      //pQueuePriorities;
            };
            const VkDeviceCreateInfo deviceCreateInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
                &globalPriorityQueryFeatures,         //pNext;
                (VkDeviceCreateFlags)0u,              //flags;
                1,                                    //queueRecordCount;
                &queueCreateInfo,                     //pRequestedQueues;
                0,                                    //layerCount;
                nullptr,                              //ppEnabledLayerNames;
                (uint32_t)enabledExtensions.size(),   //extensionCount;
                enabledExtensions.data(),             //ppEnabledExtensionNames;
                nullptr,                              //pEnabledFeatures;
            };
            const bool mayBeDenied = globalPriority > VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT;
            const bool mustFail =
                globalPriority < globalPriorityProperties[ndx].priorities[0] ||
                globalPriority >
                    globalPriorityProperties[ndx].priorities[globalPriorityProperties[ndx].priorityCount - 1];

            try
            {
                const Unique<VkDevice> device(
                    createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                                       platformInterface, instance, instanceDriver, physicalDevice, &deviceCreateInfo));
                const DeviceDriver deviceDriver(platformInterface, instance, device.get(), context.getUsedApiVersion(),
                                                context.getTestContext().getCommandLine());
                const VkQueue queue = getDeviceQueue(deviceDriver, *device, ndx, 0);

                TCU_CHECK(!!queue);

                if (mustFail)
                {
                    log << TestLog::Message << "device creation must fail but not"
                        << ", globalPriority = " << globalPriority << ", queueCreateInfo " << queueCreateInfo
                        << TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
            catch (const Error &error)
            {
                if (mustFail || (error.getError() == VK_ERROR_NOT_PERMITTED_EXT && mayBeDenied))
                {
                    continue;
                }
                else
                {
                    log << TestLog::Message << "exception thrown " << error.getMessage()
                        << ", globalPriority = " << globalPriority << ", queueCreateInfo " << queueCreateInfo
                        << ", Error Code: " << error.getError() << TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus createDeviceFeatures2Test(Context &context)
{
    const PlatformInterface &vkp = context.getPlatformInterface();
    const CustomInstance instance(createCustomInstanceWithExtension(context, "VK_KHR_get_physical_device_properties2"));
    const InstanceDriver &vki(instance.getDriver());
    const VkPhysicalDevice physicalDevice = chooseDevice(vki, instance, context.getTestContext().getCommandLine());
    const uint32_t queueFamilyIndex       = 0;
    const uint32_t queueCount             = 1;
    const uint32_t queueIndex             = 0;
    const float queuePriority             = 1.0f;
    const vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);

    VkPhysicalDeviceFeatures2 enabledFeatures;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        (VkDeviceQueueCreateFlags)0u,
        queueFamilyIndex,
        queueCount,
        &queuePriority,
    };

    void *pNext = &enabledFeatures;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        pNext,
        (VkDeviceCreateFlags)0u,
        1,
        &deviceQueueCreateInfo,
        0u,
        nullptr,
        0,
        nullptr,
        nullptr,
    };

    // Populate enabledFeatures
    enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabledFeatures.pNext = nullptr;

    vki.getPhysicalDeviceFeatures2(physicalDevice, &enabledFeatures);

    {
        const Unique<VkDevice> device(
            createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki,
                               physicalDevice, &deviceCreateInfo));
        const DeviceDriver vkd(vkp, instance, device.get(), context.getUsedApiVersion(),
                               context.getTestContext().getCommandLine());
        const VkQueue queue = getDeviceQueue(vkd, *device, queueFamilyIndex, queueIndex);

        VK_CHECK(vkd.queueWaitIdle(queue));
    }

    return tcu::TestStatus::pass("Pass");
}

struct Feature
{
    const char *name;
    size_t offset;
};

#define FEATURE_ITEM(STRUCT, MEMBER)      \
    {                                     \
        #MEMBER, offsetof(STRUCT, MEMBER) \
    }
// This macro is used to avoid the "out of array bounds" compiler warnings/errors in the checkFeatures function.
#define SAFE_OFFSET(LIMITING_STRUCT, STRUCT, MEMBER) \
    std::min<size_t>(sizeof(LIMITING_STRUCT) - sizeof(VkBool32), offsetof(STRUCT, MEMBER))

template <typename StructType>
void checkFeatures(const PlatformInterface &vkp, const VkInstance &instance, const InstanceDriver &instanceDriver,
                   const VkPhysicalDevice physicalDevice, int numFeatures, const Feature features[],
                   const StructType *supportedFeatures, const uint32_t queueFamilyIndex, const uint32_t queueCount,
                   const float queuePriority, int &numErrors, tcu::ResultCollector &resultCollector,
                   const vector<const char *> *extensionNames,
                   const VkPhysicalDeviceFeatures &defaultPhysicalDeviceFeatures,
#ifdef CTS_USES_VULKANSC
                   VkDeviceObjectReservationCreateInfo memReservationStatMax,
#endif // CTS_USES_VULKANSC
                   bool isSubProcess, uint32_t usedApiVersion, const tcu::CommandLine &commandLine)
{
    struct StructureBase
    {
        VkStructureType sType;
        void *pNext;
    };

    for (int featureNdx = 0; featureNdx < numFeatures; featureNdx++)
    {
        // Test only features that are not supported.
        if (*(((VkBool32 *)((uint8_t *)(supportedFeatures) + features[featureNdx].offset))))
            continue;

        StructType structCopy;
        deMemset(&structCopy, 0, sizeof(StructType));

        auto *structBase              = reinterpret_cast<StructureBase *>(&structCopy);
        VkStructureType structureType = reinterpret_cast<const StructureBase *>(supportedFeatures)->sType;
        structBase->sType             = structureType;
        structBase->pNext             = nullptr;

        VkPhysicalDeviceFeatures physicalDeviceFeaturesCopy = defaultPhysicalDeviceFeatures;

        // Some features require that other feature(s) are also enabled.

        if (structureType == vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES)
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceVulkan11Features, StructType>::value));
            // If multiviewGeometryShader is enabled then multiview must also be enabled.
            // If multiviewTessellationShader is enabled then multiview must also be enabled.
            if (features[featureNdx].offset == offsetof(VkPhysicalDeviceVulkan11Features, multiviewGeometryShader) ||
                features[featureNdx].offset == offsetof(VkPhysicalDeviceVulkan11Features, multiviewTessellationShader))
            {
                auto *memberPtr =
                    reinterpret_cast<VkBool32 *>(reinterpret_cast<uint8_t *>(&structCopy) +
                                                 SAFE_OFFSET(StructType, VkPhysicalDeviceVulkan11Features, multiview));
                *memberPtr = VK_TRUE;
            }

            // If variablePointers is enabled then variablePointersStorageBuffer must also be enabled.
            if (features[featureNdx].offset == offsetof(VkPhysicalDeviceVulkan11Features, variablePointers))
            {
                auto *memberPtr = reinterpret_cast<VkBool32 *>(
                    reinterpret_cast<uint8_t *>(&structCopy) +
                    SAFE_OFFSET(StructType, VkPhysicalDeviceVulkan11Features, variablePointersStorageBuffer));
                *memberPtr = VK_TRUE;
            }
        }
#ifndef CTS_USES_VULKANSC
        // If rayTracingPipelineShaderGroupHandleCaptureReplayMixed is VK_TRUE, rayTracingPipelineShaderGroupHandleCaptureReplay must also be VK_TRUE.
        else if (structureType == vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR &&
                 features[featureNdx].offset == offsetof(VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
                                                         rayTracingPipelineShaderGroupHandleCaptureReplayMixed))
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, StructType>::value));
            auto *memberPtr =
                reinterpret_cast<VkBool32 *>(reinterpret_cast<uint8_t *>(&structCopy) +
                                             SAFE_OFFSET(StructType, VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
                                                         rayTracingPipelineShaderGroupHandleCaptureReplay));
            *memberPtr = VK_TRUE;
        }
#endif // CTS_USES_VULKANSC
        else if (structureType == vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES)
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceMultiviewFeatures, StructType>::value));
            // If multiviewGeometryShader is enabled then multiview must also be enabled.
            // If multiviewTessellationShader is enabled then multiview must also be enabled.
            if (features[featureNdx].offset == offsetof(VkPhysicalDeviceMultiviewFeatures, multiviewGeometryShader) ||
                features[featureNdx].offset == offsetof(VkPhysicalDeviceMultiviewFeatures, multiviewTessellationShader))
            {
                auto *memberPtr =
                    reinterpret_cast<VkBool32 *>(reinterpret_cast<uint8_t *>(&structCopy) +
                                                 SAFE_OFFSET(StructType, VkPhysicalDeviceMultiviewFeatures, multiview));
                *memberPtr = VK_TRUE;
            }
        }
        else if (structureType == vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT)
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceRobustness2FeaturesEXT, StructType>::value));
            // If robustBufferAccess2 is enabled then robustBufferAccess must also be enabled.
            if (features[featureNdx].offset == offsetof(VkPhysicalDeviceRobustness2FeaturesEXT, robustBufferAccess2))
            {
                physicalDeviceFeaturesCopy.robustBufferAccess = true;
            }
        }
        else if (structureType == vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT)
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT, StructType>::value));
            // If sparseImageInt64Atomics is enabled, shaderImageInt64Atomics must be enabled.
            if (features[featureNdx].offset ==
                offsetof(VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT, sparseImageInt64Atomics))
            {
                auto *memberPtr = reinterpret_cast<VkBool32 *>(
                    reinterpret_cast<uint8_t *>(&structCopy) +
                    SAFE_OFFSET(StructType, VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT,
                                shaderImageInt64Atomics));
                *memberPtr = VK_TRUE;
            }
        }
        else if (structureType == vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT)
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, StructType>::value));
            // If sparseImageFloat32Atomics is enabled, shaderImageFloat32Atomics must be enabled.
            if (features[featureNdx].offset ==
                offsetof(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, sparseImageFloat32Atomics))
            {
                auto *memberPtr = reinterpret_cast<VkBool32 *>(
                    reinterpret_cast<uint8_t *>(&structCopy) +
                    SAFE_OFFSET(StructType, VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderImageFloat32Atomics));
                *memberPtr = VK_TRUE;
            }

            // If sparseImageFloat32AtomicAdd is enabled, shaderImageFloat32AtomicAdd must be enabled.
            if (features[featureNdx].offset ==
                offsetof(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, sparseImageFloat32AtomicAdd))
            {
                auto *memberPtr = reinterpret_cast<VkBool32 *>(
                    reinterpret_cast<uint8_t *>(&structCopy) +
                    SAFE_OFFSET(StructType, VkPhysicalDeviceShaderAtomicFloatFeaturesEXT, shaderImageFloat32AtomicAdd));
                *memberPtr = VK_TRUE;
            }
        }
#ifndef CTS_USES_VULKANSC
        else if (structureType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT)
        {
            DE_ASSERT((std::is_same<VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT, StructType>::value));
            // If sparseImageFloat32AtomicMinMax is enabled, shaderImageFloat32AtomicMinMax must be enabled.
            if (features[featureNdx].offset ==
                offsetof(VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT, sparseImageFloat32AtomicMinMax))
            {
                auto *memberPtr =
                    reinterpret_cast<VkBool32 *>(reinterpret_cast<uint8_t *>(&structCopy) +
                                                 SAFE_OFFSET(StructType, VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT,
                                                             shaderImageFloat32AtomicMinMax));
                *memberPtr = VK_TRUE;
            }
        }
#endif // CTS_USES_VULKANSC

        // Enable the feature we're testing.
        *reinterpret_cast<VkBool32 *>(reinterpret_cast<uint8_t *>(&structCopy) + features[featureNdx].offset) = VK_TRUE;

        const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
            nullptr,                                    // pNext
            (VkDeviceQueueCreateFlags)0u,               // flags
            queueFamilyIndex,                           // queueFamilyIndex
            queueCount,                                 // queueCount
            &queuePriority                              // pQueuePriorities
        };
        VkPhysicalDeviceFeatures2 deviceFeatures2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, // sType
            &structCopy,                                  // pNext
            physicalDeviceFeaturesCopy                    // features
        };

        void *pNext = &deviceFeatures2;
#ifdef CTS_USES_VULKANSC
        VkDeviceObjectReservationCreateInfo memReservationInfo =
            isSubProcess ? memReservationStatMax : resetDeviceObjectReservationCreateInfo();
        memReservationInfo.pNext = pNext;
        pNext                    = &memReservationInfo;

        VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
        sc10Features.pNext                              = pNext;
        pNext                                           = &sc10Features;
#else
        DE_UNREF(isSubProcess);
#endif // CTS_USES_VULKANSC

        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                                          // sType
            pNext,                                                                         // pNext
            (VkDeviceCreateFlags)0u,                                                       // flags
            1,                                                                             // queueCreateInfoCount
            &deviceQueueCreateInfo,                                                        // pQueueCreateInfos
            0u,                                                                            // enabledLayerCount
            nullptr,                                                                       // ppEnabledLayerNames
            static_cast<uint32_t>(extensionNames == nullptr ? 0 : extensionNames->size()), // enabledExtensionCount
            extensionNames == nullptr ? nullptr : extensionNames->data(),                  // ppEnabledExtensionNames
            nullptr                                                                        // pEnabledFeatures
        };

        VkDevice device = VK_NULL_HANDLE;
        const VkResult res =
            createUncheckedDevice(false, instanceDriver, physicalDevice, &deviceCreateInfo, nullptr, &device);

        if (res != VK_ERROR_FEATURE_NOT_PRESENT)
        {
            numErrors++;
            resultCollector.fail("Not returning VK_ERROR_FEATURE_NOT_PRESENT when creating device with feature " +
                                 de::toString(features[featureNdx].name) + ", which was reported as unsupported.");
        }
        if (device != VK_NULL_HANDLE)
        {
            DeviceDriver deviceDriver(vkp, instance, device, usedApiVersion, commandLine);
            deviceDriver.destroyDevice(device, nullptr);
        }
    }
}

tcu::TestStatus createDeviceWithUnsupportedFeaturesTest(Context &context)
{
    const PlatformInterface &vkp = context.getPlatformInterface();
    tcu::TestLog &log            = context.getTestContext().getLog();
    tcu::ResultCollector resultCollector(log);
    const CustomInstance instance(
        createCustomInstanceWithExtensions(context, context.getInstanceExtensions(), nullptr, true));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const uint32_t queueFamilyIndex = 0;
    const uint32_t queueCount       = 1;
    const float queuePriority       = 1.0f;
    const DeviceFeatures deviceFeaturesAll(context.getInstanceInterface(), context.getUsedApiVersion(), physicalDevice,
                                           context.getInstanceExtensions(), context.getDeviceExtensions(), true);
    const VkPhysicalDeviceFeatures2 deviceFeatures2 = deviceFeaturesAll.getCoreFeatures2();
    const VkPhysicalDeviceFeatures deviceFeatures   = deviceFeatures2.features;
    const vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    // Test features listed in VkPhysicalDeviceFeatures structure
    {
        static const Feature features[] = {
            // robustBufferAccess is removed, because it's always supported.
            FEATURE_ITEM(VkPhysicalDeviceFeatures, fullDrawIndexUint32),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, imageCubeArray),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, independentBlend),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, geometryShader),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, tessellationShader),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sampleRateShading),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, dualSrcBlend),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, logicOp),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, multiDrawIndirect),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, drawIndirectFirstInstance),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, depthClamp),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, depthBiasClamp),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, fillModeNonSolid),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, depthBounds),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, wideLines),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, largePoints),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, alphaToOne),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, multiViewport),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, samplerAnisotropy),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, textureCompressionETC2),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, textureCompressionASTC_LDR),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, textureCompressionBC),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, occlusionQueryPrecise),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, pipelineStatisticsQuery),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, vertexPipelineStoresAndAtomics),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, fragmentStoresAndAtomics),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderTessellationAndGeometryPointSize),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderImageGatherExtended),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderStorageImageExtendedFormats),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderStorageImageMultisample),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderStorageImageReadWithoutFormat),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderStorageImageWriteWithoutFormat),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderUniformBufferArrayDynamicIndexing),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderSampledImageArrayDynamicIndexing),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderStorageBufferArrayDynamicIndexing),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderStorageImageArrayDynamicIndexing),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderClipDistance),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderCullDistance),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderFloat64),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderInt64),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderInt16),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderResourceResidency),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, shaderResourceMinLod),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseBinding),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidencyBuffer),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidencyImage2D),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidencyImage3D),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidency2Samples),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidency4Samples),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidency8Samples),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidency16Samples),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, sparseResidencyAliased),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, variableMultisampleRate),
            FEATURE_ITEM(VkPhysicalDeviceFeatures, inheritedQueries)};

        for (const auto &feature : features)
        {
            // Test only features that are not supported.
            if (*(((VkBool32 *)((uint8_t *)(&deviceFeatures) + feature.offset))))
                continue;

            VkPhysicalDeviceFeatures enabledFeatures                        = deviceFeatures;
            *((VkBool32 *)((uint8_t *)(&enabledFeatures) + feature.offset)) = VK_TRUE;

            const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                                   nullptr,
                                                                   (VkDeviceQueueCreateFlags)0u,
                                                                   queueFamilyIndex,
                                                                   queueCount,
                                                                   &queuePriority};

            void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
            VkDeviceObjectReservationCreateInfo memReservationInfo =
                context.getTestContext().getCommandLine().isSubProcess() ?
                    context.getResourceInterface()->getStatMax() :
                    resetDeviceObjectReservationCreateInfo();
            memReservationInfo.pNext = pNext;
            pNext                    = &memReservationInfo;

            VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
            sc10Features.pNext                              = pNext;
            pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

            const VkDeviceCreateInfo deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                         pNext,
                                                         (VkDeviceCreateFlags)0u,
                                                         1,
                                                         &deviceQueueCreateInfo,
                                                         0u,
                                                         nullptr,
                                                         0,
                                                         nullptr,
                                                         &enabledFeatures};

            VkDevice device = VK_NULL_HANDLE;
            const VkResult res =
                createUncheckedDevice(false, instanceDriver, physicalDevice, &deviceCreateInfo, nullptr, &device);

            if (res != VK_ERROR_FEATURE_NOT_PRESENT)
            {
                resultCollector.fail("Not returning VK_ERROR_FEATURE_NOT_PRESENT when creating device with feature " +
                                     de::toString(feature.name) + ", which was reported as unsupported.");
            }

            if (device != nullptr)
            {
                DeviceDriver deviceDriver(vkp, instance, device, context.getUsedApiVersion(),
                                          context.getTestContext().getCommandLine());
                deviceDriver.destroyDevice(device, nullptr);
            }
        }
    }

    return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

#include "vkDeviceFeatureTest.inl"

tcu::TestStatus createDeviceQueue2Test(Context &context)
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
        TCU_THROW(NotSupportedError, "Vulkan 1.1 is not supported");

    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const CustomInstance instance(createCustomInstanceFromContext(context));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    const uint32_t queueCount       = 1;
    const uint32_t queueIndex       = 0;
    const float queuePriority       = 1.0f;

    VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeature = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, // VkStructureType sType;
        nullptr,                                                     // void* pNext;
        VK_FALSE                                                     // VkBool32 protectedMemory;
    };

    VkPhysicalDeviceFeatures2 features2;
    deMemset(&features2, 0, sizeof(features2));
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &protectedMemoryFeature;

    instanceDriver.getPhysicalDeviceFeatures2(physicalDevice, &features2);
    if (protectedMemoryFeature.protectedMemory == VK_FALSE)
        TCU_THROW(NotSupportedError, "Protected memory feature is not supported");

    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,       // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           // uint32_t queueFamilyIndex;
        queueCount,                                 // uint32_t queueCount;
        &queuePriority,                             // const float* pQueuePriorities;
    };

    void *pNext = &features2;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        (VkDeviceCreateFlags)0u,              // VkDeviceCreateFlags flags;
        1,                                    // uint32_t queueCreateInfoCount;
        &deviceQueueCreateInfo,               // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0,                                    // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        0,                                    // uint32_t enabledExtensionCount;
        nullptr,                              // const char* const* ppEnabledExtensionNames;
        nullptr,                              // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    const VkDeviceQueueInfo2 deviceQueueInfo2 = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2, // VkStructureType sType;
        nullptr,                               // const void* pNext;
        VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,  // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                      // uint32_t queueFamilyIndex;
        queueIndex,                            // uint32_t queueIndex;
    };

    {
        const Unique<VkDevice> device(
            createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface,
                               instance, instanceDriver, physicalDevice, &deviceCreateInfo));
        const DeviceDriver deviceDriver(platformInterface, instance, device.get(), context.getUsedApiVersion(),
                                        context.getTestContext().getCommandLine());
        const VkQueue queue2 = getDeviceQueue2(deviceDriver, *device, &deviceQueueInfo2);

        VK_CHECK(deviceDriver.queueWaitIdle(queue2));
    }

    return tcu::TestStatus::pass("Pass");
}

map<uint32_t, VkQueueFamilyProperties> findQueueFamiliesWithCaps(const InstanceInterface &vkInstance,
                                                                 VkPhysicalDevice physicalDevice,
                                                                 VkQueueFlags requiredCaps)
{
    const vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);
    map<uint32_t, VkQueueFamilyProperties> queueFamilies;

    for (uint32_t queueNdx = 0; queueNdx < static_cast<uint32_t>(queueProps.size()); queueNdx++)
    {
        const VkQueueFamilyProperties &queueFamilyProperties = queueProps[queueNdx];

        if ((queueFamilyProperties.queueFlags & requiredCaps) == requiredCaps)
            queueFamilies[queueNdx] = queueFamilyProperties;
    }

    if (queueFamilies.empty())
        TCU_THROW(NotSupportedError, "No matching queue found");

    return queueFamilies;
}

void checkProtectedMemorySupport(Context &context)
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
        TCU_THROW(NotSupportedError, "Vulkan 1.1 is not supported");

    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeature = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, // VkStructureType sType;
        nullptr,                                                     // void* pNext;
        VK_FALSE                                                     // VkBool32 protectedMemory;
    };

    VkPhysicalDeviceFeatures2 features2;
    deMemset(&features2, 0, sizeof(features2));
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &protectedMemoryFeature;

    instanceDriver.getPhysicalDeviceFeatures2(physicalDevice, &features2);
    if (protectedMemoryFeature.protectedMemory == VK_FALSE)
        TCU_THROW(NotSupportedError, "Protected memory feature is not supported");
}

Move<VkDevice> createProtectedDeviceWithQueueConfig(Context &context,
                                                    const std::vector<VkDeviceQueueCreateInfo> &queueCreateInfos,
                                                    bool dumpExtraInfo = false)
{
    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const VkInstance instance                  = context.getInstance();
    const InstanceInterface &instanceDriver    = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice      = context.getPhysicalDevice();

    if (dumpExtraInfo)
    {
        tcu::TestLog &log = context.getTestContext().getLog();

        log << tcu::TestLog::Message
            << "Creating VkDevice with the following Queue configuration:" << tcu::TestLog::EndMessage;

        for (size_t idx = 0; idx < queueCreateInfos.size(); idx++)
        {
            const VkDeviceQueueCreateInfo &queueCreateInfo = queueCreateInfos[idx];

            log << tcu::TestLog::Message << "QueueCreateInfo " << idx << ": "
                << "flags: " << queueCreateInfo.flags << " "
                << "family: " << queueCreateInfo.queueFamilyIndex << " "
                << "count: " << queueCreateInfo.queueCount << tcu::TestLog::EndMessage;
        }
    }

    // Protected memory features availability should be already checked at this point.
    VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeature = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, // VkStructureType sType;
        nullptr,                                                     // void* pNext;
        VK_TRUE                                                      // VkBool32 protectedMemory;
    };

    VkPhysicalDeviceFeatures2 features2;
    deMemset(&features2, 0, sizeof(features2));
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &protectedMemoryFeature;

#ifdef CTS_USES_VULKANSC
    void *pNext = nullptr;

    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
#ifdef CTS_USES_VULKANSC
        &sc10Features, // const void* pNext;
#else
        &features2, // const void* pNext;
#endif                                     // CTS_USES_VULKANSC
        (VkDeviceCreateFlags)0u,           // VkDeviceCreateFlags flags;
        (uint32_t)queueCreateInfos.size(), // uint32_t queueCreateInfoCount;
        queueCreateInfos.data(),           // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0,                                 // uint32_t enabledLayerCount;
        nullptr,                           // const char* const* ppEnabledLayerNames;
        0,                                 // uint32_t enabledExtensionCount;
        nullptr,                           // const char* const* ppEnabledExtensionNames;
        nullptr,                           // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), platformInterface,
                              instance, instanceDriver, physicalDevice, &deviceCreateInfo);
}

VkQueue getDeviceQueue2WithOptions(const DeviceDriver &deviceDriver, const VkDevice device,
                                   VkDeviceQueueCreateFlags flags, uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    VkDeviceQueueInfo2 queueInfo2 = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2, // VkStructureType sType;
        nullptr,                               // const void* pNext;
        flags,                                 // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                      // uint32_t queueFamilyIndex;
        queueIndex,                            // uint32_t queueIndex;
    };

    return getDeviceQueue2(deviceDriver, device, &queueInfo2);
}

struct QueueCreationInfo
{
    uint32_t familyIndex;
    VkDeviceQueueCreateFlags flags;
    uint32_t count;
};

bool runQueueCreationTestCombination(Context &context, tcu::ResultCollector &results,
                                     const std::vector<QueueCreationInfo> &testCombination, bool dumpExtraInfo = false)
{
    uint32_t sumQueueCount = 0u;
    for (const QueueCreationInfo &info : testCombination)
    {
        sumQueueCount += info.count;
    }

    // Have an array of queue priorities which can be used when creating the queues (it is always greater or equal to the number of queues for a given VkDeviceQueueCreateInfo).
    const vector<float> queuePriorities(sumQueueCount, 1.0f);
    vector<VkDeviceQueueCreateInfo> queueCreateInfo;

    for (const QueueCreationInfo &info : testCombination)
    {
        const VkDeviceQueueCreateInfo queueInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            info.flags,                                 // VkDeviceQueueCreateFlags flags;
            info.familyIndex,                           // uint32_t queueFamilyIndex;
            info.count,                                 // uint32_t queueCount;
            queuePriorities.data(),                     // const float* pQueuePriorities;
        };
        queueCreateInfo.push_back(queueInfo);
    }

    const PlatformInterface &platformInterface = context.getPlatformInterface();
    const VkInstance instance                  = context.getInstance();

    const Unique<VkDevice> device(createProtectedDeviceWithQueueConfig(context, queueCreateInfo, dumpExtraInfo));
    const DeviceDriver deviceDriver(platformInterface, instance, *device, context.getUsedApiVersion(),
                                    context.getTestContext().getCommandLine());

    for (const QueueCreationInfo &info : testCombination)
    {
        // Query Queues (based on the test configuration)
        for (uint32_t queueIdx = 0; queueIdx < info.count; queueIdx++)
        {
            const string message = "(queueFamilyIndex: " + de::toString(info.familyIndex) +
                                   ", flags: " + de::toString(info.flags) + ", queue Index: " + de::toString(queueIdx) +
                                   ")";
            const VkQueue queue =
                getDeviceQueue2WithOptions(deviceDriver, *device, info.flags, info.familyIndex, queueIdx);

            if (queue != nullptr)
            {
                VK_CHECK(deviceDriver.queueWaitIdle(queue));
                results.addResult(QP_TEST_RESULT_PASS, "Found Queue. " + message);
            }
            else
                results.fail("Unable to access the Queue. " + message);
        }
    }

    return results.getResult() == QP_TEST_RESULT_PASS;
}

tcu::TestStatus createDeviceQueue2WithTwoQueuesSmokeTest(Context &context)
{
    const bool dumpExtraInfo = true;

    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();
    tcu::TestLog &log                       = context.getTestContext().getLog();

    vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    // Find the first protected-capabale queue with a queueCount >= 2 and use it for testing (smoke test)
    constexpr uint32_t MAX_DEUINT32   = std::numeric_limits<uint32_t>::max();
    uint32_t queueFamilyIndex         = MAX_DEUINT32;
    const VkQueueFlags requiredCaps   = VK_QUEUE_PROTECTED_BIT;
    const uint32_t requiredQueueCount = 2;

    for (uint32_t queueNdx = 0; queueNdx < queueFamilyProperties.size(); queueNdx++)
    {
        if ((queueFamilyProperties[queueNdx].queueFlags & requiredCaps) == requiredCaps &&
            queueFamilyProperties[queueNdx].queueCount >= requiredQueueCount)
        {
            queueFamilyIndex = queueNdx;
            break;
        }
    }

    if (queueFamilyIndex == MAX_DEUINT32)
        TCU_THROW(NotSupportedError,
                  "Unable to find a queue family that is protected-capable and supports more than one queue.");

    if (dumpExtraInfo)
        log << tcu::TestLog::Message << "Selected VkQueueFamilyProperties index: " << queueFamilyIndex
            << tcu::TestLog::EndMessage;

    // Use the previously selected queue family index to create 1 protected-capable and 1 unprotected queue.
    const QueueCreationInfo protectedQueueConfig   = {queueFamilyIndex, VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT, 1};
    const QueueCreationInfo unprotectedQueueConfig = {queueFamilyIndex, (VkDeviceQueueCreateFlags)0u, 1};

    tcu::ResultCollector results(log);
    const std::vector<QueueCreationInfo> testCombination = {protectedQueueConfig, unprotectedQueueConfig};
    bool success = runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);

    if (success)
        return tcu::TestStatus::pass("All Queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus createDeviceQueue2WithAllProtectedQueues(Context &context)
{
    const bool dumpExtraInfo = true;

    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    // Select only protected-capable queue families
    map<uint32_t, VkQueueFamilyProperties> queueFamilyProperties =
        findQueueFamiliesWithCaps(instanceDriver, physicalDevice, VK_QUEUE_PROTECTED_BIT);

    bool success = true;
    tcu::ResultCollector results(context.getTestContext().getLog());

    // For each protected-capable queue family, create a device with the max number of queues available and all queues created as protected-capable.
    for (const pair<uint32_t, VkQueueFamilyProperties> queueFamilyProperty : queueFamilyProperties)
    {
        const uint32_t queueFamilyIndex = queueFamilyProperty.first;
        const uint32_t queueCount       = queueFamilyProperty.second.queueCount;

        const QueueCreationInfo protectedQueueConfig    = {queueFamilyIndex, VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,
                                                           queueCount};
        const vector<QueueCreationInfo> testCombination = {protectedQueueConfig};

        // Run current confugurations.
        success = success && runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);
    }

    if (success)
        return tcu::TestStatus::pass("All queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus createDeviceQueue2WithAllUnprotectedQueues(Context &context)
{
    const bool dumpExtraInfo = true;

    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    // Select all queue families with or without protected bit
    map<uint32_t, VkQueueFamilyProperties> queueFamilyProperties =
        findQueueFamiliesWithCaps(instanceDriver, physicalDevice, 0);

    bool success = true;
    tcu::ResultCollector results(context.getTestContext().getLog());

    // For each Queue Family create the max number of unprotected Queues.
    for (const pair<uint32_t, VkQueueFamilyProperties> queueFamilyProperty : queueFamilyProperties)
    {
        const uint32_t queueFamilyIndex = queueFamilyProperty.first;
        const uint32_t queueCount       = queueFamilyProperty.second.queueCount;

        const QueueCreationInfo unprotectedQueueConfig  = {queueFamilyIndex, (VkDeviceQueueCreateFlags)0u, queueCount};
        const vector<QueueCreationInfo> testCombination = {unprotectedQueueConfig};

        // Run current confugurations.
        success = success && runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);
    }

    if (success)
        return tcu::TestStatus::pass("All Queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

typedef vector<QueueCreationInfo> DeviceQueueConfig;
typedef map<uint32_t, vector<DeviceQueueConfig>> QueueFamilyConfigurations;

QueueFamilyConfigurations buildQueueConfigurations(const map<uint32_t, VkQueueFamilyProperties> &queueFamilyProperties)
{
    QueueFamilyConfigurations queuesPerFamily;

    // Build up the queue creation combinations (N protected and M unprotected queues where N+M == queueFamily.queueCount)
    // on each protected-capable queue family
    for (const pair<uint32_t, VkQueueFamilyProperties> queueFamily : queueFamilyProperties)
    {
        const uint32_t queueFamilyIndex                   = queueFamily.first;
        const VkQueueFamilyProperties queueFamilyProperty = queueFamily.second;
        const uint32_t allowedQueueCount                  = queueFamilyProperty.queueCount;

        for (uint32_t splitCount = 0; splitCount <= allowedQueueCount; splitCount++)
        {
            const uint32_t protectedQueuesCount   = allowedQueueCount - splitCount;
            const uint32_t unprotectedQueuesCount = splitCount;

            vector<QueueCreationInfo> testCombination = {};

            if (protectedQueuesCount)
                testCombination.push_back(
                    {queueFamilyIndex, VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT, protectedQueuesCount});

            if (unprotectedQueuesCount)
                testCombination.push_back({queueFamilyIndex, (VkDeviceQueueCreateFlags)0u, unprotectedQueuesCount});

            queuesPerFamily[queueFamilyIndex].push_back(testCombination);
        }
    }

    return queuesPerFamily;
}

tcu::TestStatus createDeviceQueue2WithNProtectedAndMUnprotectedQueues(Context &context)
{
    const bool dumpExtraInfo = true;

    tcu::TestLog &log                       = context.getTestContext().getLog();
    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    // Select only protected-capable queue families
    map<uint32_t, VkQueueFamilyProperties> queueFamilyProperties =
        findQueueFamiliesWithCaps(instanceDriver, physicalDevice, VK_QUEUE_PROTECTED_BIT);

    // Build all protected-unprotected splits per queue family.
    QueueFamilyConfigurations queuesPerFamily = buildQueueConfigurations(queueFamilyProperties);
    vector<DeviceQueueConfig> queueCreateCombinations;

    // Transform configurations to a simple vector
    for (const auto &item : queuesPerFamily)
    {
        const vector<DeviceQueueConfig> &queueConfigs = item.second;

        std::copy(queueConfigs.begin(), queueConfigs.end(), std::back_inserter(queueCreateCombinations));
    }

    if (dumpExtraInfo)
    {
        for (const vector<QueueCreationInfo> &testCombination : queueCreateCombinations)
        {
            ostringstream queuesInfo;
            for (const QueueCreationInfo &queueInfo : testCombination)
            {
                queuesInfo << "(Queue family: " << queueInfo.familyIndex << ", flags: " << queueInfo.flags
                           << ", count: " << queueInfo.count << ")";
            }

            log << tcu::TestLog::Message << "Test Combination: " << queuesInfo.str() << tcu::TestLog::EndMessage;
        }
    }

    bool success = true;
    tcu::ResultCollector results(log);

    // Based on the protected-unprotected queue combinations, run each test case.
    for (const vector<QueueCreationInfo> &testCombination : queueCreateCombinations)
    {
        success = success && runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);
    }

    // Run the test cases also in reverse order (so the unprotected queue creation info is at the start of the VkDeviceQueueCreateInfo vector).
    for (vector<QueueCreationInfo> &testCombination : queueCreateCombinations)
    {
        std::reverse(testCombination.begin(), testCombination.end());

        success = success && runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);
    }

    if (success)
        return tcu::TestStatus::pass("All Queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus createDeviceQueue2WithMultipleQueueCombinations(Context &context)
{
    const bool dumpExtraInfo = true;

    tcu::TestLog &log                       = context.getTestContext().getLog();
    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    // Select only protected-capable queue families.
    map<uint32_t, VkQueueFamilyProperties> queueFamilyProperties =
        findQueueFamiliesWithCaps(instanceDriver, physicalDevice, VK_QUEUE_PROTECTED_BIT);

    // Build all protected-unprotected splits per queue family.
    QueueFamilyConfigurations queuesPerFamily = buildQueueConfigurations(queueFamilyProperties);

    // Build up all combinations of queue families from the previous mapping.
    vector<DeviceQueueConfig> queueCreateCombinations;
    {
        vector<uint32_t> itemIndices(queuesPerFamily.size(), 0u);

        // Calculate the max number of combinations.
        auto multiplyConfigCounts = [](uint32_t &count, const typename QueueFamilyConfigurations::value_type &item)
        { return count * (uint32_t)item.second.size(); };
        const uint32_t itemCount = accumulate(queuesPerFamily.begin(), queuesPerFamily.end(), 1u, multiplyConfigCounts);

        for (uint32_t count = 0u; count < itemCount; count++)
        {
            DeviceQueueConfig testCombination;

            // Select queue configurations from each family
            for (uint32_t ndx = 0u; ndx < static_cast<uint32_t>(itemIndices.size()); ndx++)
            {
                const auto &familyConfigurations           = queuesPerFamily[ndx];
                const DeviceQueueConfig &targetQueueConfig = familyConfigurations[itemIndices[ndx]];

                std::copy(targetQueueConfig.begin(), targetQueueConfig.end(), std::back_inserter(testCombination));
            }

            queueCreateCombinations.push_back(testCombination);

            // Increment the indices.
            for (uint32_t ndx = 0u; ndx < static_cast<uint32_t>(itemIndices.size()); ndx++)
            {
                itemIndices[ndx]++;
                if (itemIndices[ndx] < queuesPerFamily[ndx].size())
                {
                    break;
                }

                // "overflow" happened in the given index, restart from zero and increment the next item index (restart loop).
                itemIndices[ndx] = 0;
            }
        }
    }

    if (dumpExtraInfo)
    {
        for (const vector<QueueCreationInfo> &testCombination : queueCreateCombinations)
        {
            ostringstream queuesInfo;
            for (const QueueCreationInfo &queueInfo : testCombination)
            {
                queuesInfo << "(Queue family: " << queueInfo.familyIndex << ", flags: " << queueInfo.flags
                           << ", count: " << queueInfo.count << ")";
            }

            log << tcu::TestLog::Message << "Test Combination: " << queuesInfo.str() << tcu::TestLog::EndMessage;
        }
    }

    bool success = true;
    tcu::ResultCollector results(log);

    // Based on the protected-unprotected queue combinations above run each test case.
    for (const DeviceQueueConfig &testCombination : queueCreateCombinations)
    {
        success = success && runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);
    }

    // Run the test cases also in reverse queue order (so the unprotected queue creation info is at the start of the VkDeviceQueueCreateInfo vector).
    for (DeviceQueueConfig &testCombination : queueCreateCombinations)
    {
        std::reverse(testCombination.begin(), testCombination.end());

        success = success && runQueueCreationTestCombination(context, results, testCombination, dumpExtraInfo);
    }

    if (success)
        return tcu::TestStatus::pass("All Queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus createDeviceQueue2WithAllFamilies(Context &context)
{
    const bool dumpExtraInfo = true;

    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    // Get all queue families
    map<uint32_t, VkQueueFamilyProperties> queueFamilyProperties =
        findQueueFamiliesWithCaps(instanceDriver, physicalDevice, (VkDeviceQueueCreateFlags)0u);

    // Create test configuration where for each queue family the maximum number of queues are created.
    vector<QueueCreationInfo> queueConfigurations;
    for (const pair<uint32_t, VkQueueFamilyProperties> queueFamilyProperty : queueFamilyProperties)
    {
        const uint32_t queueFamilyIndex = queueFamilyProperty.first;
        const uint32_t queueCount       = queueFamilyProperty.second.queueCount;

        const QueueCreationInfo queueConfig = {queueFamilyIndex, (VkDeviceQueueCreateFlags)0u, queueCount};

        queueConfigurations.push_back(queueConfig);
    }

    tcu::ResultCollector results(context.getTestContext().getLog());

    // Execute test to see if it possible to have all queue families created at the same time.
    bool success = runQueueCreationTestCombination(context, results, queueConfigurations, dumpExtraInfo);

    if (success)
        return tcu::TestStatus::pass("All Queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus createDeviceQueue2WithAllFamiliesProtected(Context &context)
{
    const bool dumpExtraInfo = true;

    const InstanceInterface &instanceDriver = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();

    // Get all queue families
    map<uint32_t, VkQueueFamilyProperties> queueFamilyProperties =
        findQueueFamiliesWithCaps(instanceDriver, physicalDevice, (VkDeviceQueueCreateFlags)0u);

    // Create test configuration where for each queue family the maximum number of queues are created.
    // If a queue supports protected memory then create a protected-capable queue.
    vector<QueueCreationInfo> queueConfigurations;
    for (const pair<uint32_t, VkQueueFamilyProperties> queueFamilyProperty : queueFamilyProperties)
    {
        const uint32_t queueFamilyIndex = queueFamilyProperty.first;
        const uint32_t queueCount       = queueFamilyProperty.second.queueCount;

        VkDeviceQueueCreateFlags useFlags = (VkDeviceQueueCreateFlags)0u;
        if ((queueFamilyProperty.second.queueFlags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT) ==
            VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT)
            useFlags |= VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;

        const QueueCreationInfo queueConfig = {queueFamilyIndex, useFlags, queueCount};

        queueConfigurations.push_back(queueConfig);
    }

    tcu::ResultCollector results(context.getTestContext().getLog());

    // Execute test to see if it possible to have all queue families created at the same time.
    bool success = runQueueCreationTestCombination(context, results, queueConfigurations, dumpExtraInfo);

    if (success)
        return tcu::TestStatus::pass("All Queues were queried correctly.");

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

#ifndef CTS_USES_VULKANSC
// Allocation tracking utilities
struct AllocTrack
{
    bool active;
    bool wasAllocated;
    void *alignedStartAddress;
    char *actualStartAddress;
    size_t requestedSizeBytes;
    size_t actualSizeBytes;
    VkSystemAllocationScope allocScope;
    uint64_t userData;

    AllocTrack()
        : active(false)
        , wasAllocated(false)
        , alignedStartAddress(nullptr)
        , actualStartAddress(nullptr)
        , requestedSizeBytes(0)
        , actualSizeBytes(0)
        , allocScope(VK_SYSTEM_ALLOCATION_SCOPE_COMMAND)
        , userData(0)
    {
    }
};

// Global vector to track allocations. This will be resized before each test and emptied after
// However, we have to globally define it so the allocation callback functions work properly
std::vector<AllocTrack> g_allocatedVector;
bool g_intentionalFailEnabled  = false;
uint32_t g_intenionalFailIndex = 0;
uint32_t g_intenionalFailCount = 0;
size_t g_allocationsCount      = 0;

void freeAllocTracker(void)
{
    g_allocatedVector.clear();
    g_allocationsCount = 0;
}

void initAllocTracker(size_t size, uint32_t intentionalFailIndex = (uint32_t)~0)
{
    if (g_allocatedVector.size() > 0)
        freeAllocTracker();

    g_allocatedVector.resize(size);

    if (intentionalFailIndex != (uint32_t)~0)
    {
        g_intentionalFailEnabled = true;
        g_intenionalFailIndex    = intentionalFailIndex;
        g_intenionalFailCount    = 0;
    }
    else
    {
        g_intentionalFailEnabled = false;
        g_intenionalFailIndex    = 0;
        g_intenionalFailCount    = 0;
    }

    g_allocationsCount = 0;
}

bool isAllocTrackerEmpty()
{
    bool success      = true;
    bool wasAllocated = false;

    for (uint32_t vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
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

VKAPI_ATTR void *VKAPI_CALL allocCallbackFunc(void *pUserData, size_t size, size_t alignment,
                                              VkSystemAllocationScope allocationScope)
{
    if (g_intentionalFailEnabled)
        if (++g_intenionalFailCount >= g_intenionalFailIndex)
            return nullptr;

    for (uint32_t vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
    {
        if (!g_allocatedVector[vectorIdx].active)
        {
            g_allocatedVector[vectorIdx].requestedSizeBytes  = size;
            g_allocatedVector[vectorIdx].actualSizeBytes     = size + (alignment - 1);
            g_allocatedVector[vectorIdx].alignedStartAddress = nullptr;
            g_allocatedVector[vectorIdx].actualStartAddress  = new char[g_allocatedVector[vectorIdx].actualSizeBytes];

            if (g_allocatedVector[vectorIdx].actualStartAddress != nullptr)
            {
                uint64_t addr = (uint64_t)g_allocatedVector[vectorIdx].actualStartAddress;
                addr += (alignment - 1);
                addr &= ~(alignment - 1);
                g_allocatedVector[vectorIdx].alignedStartAddress = (void *)addr;
                g_allocatedVector[vectorIdx].allocScope          = allocationScope;
                g_allocatedVector[vectorIdx].userData            = (uint64_t)pUserData;
                g_allocatedVector[vectorIdx].active              = true;
                g_allocatedVector[vectorIdx].wasAllocated        = true;
            }

            g_allocationsCount++;
            return g_allocatedVector[vectorIdx].alignedStartAddress;
        }
    }
    return nullptr;
}

VKAPI_ATTR void VKAPI_CALL freeCallbackFunc(void *pUserData, void *pMemory)
{
    DE_UNREF(pUserData);

    for (uint32_t vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
    {
        if (g_allocatedVector[vectorIdx].active && g_allocatedVector[vectorIdx].alignedStartAddress == pMemory)
        {
            delete[] g_allocatedVector[vectorIdx].actualStartAddress;
            g_allocatedVector[vectorIdx].active = false;
            break;
        }
    }
}

VKAPI_ATTR void *VKAPI_CALL reallocCallbackFunc(void *pUserData, void *pOriginal, size_t size, size_t alignment,
                                                VkSystemAllocationScope allocationScope)
{
    if (pOriginal != nullptr)
    {
        for (uint32_t vectorIdx = 0; vectorIdx < g_allocatedVector.size(); vectorIdx++)
        {
            if (g_allocatedVector[vectorIdx].active && g_allocatedVector[vectorIdx].alignedStartAddress == pOriginal)
            {
                if (size == 0)
                {
                    freeCallbackFunc(pUserData, pOriginal);
                    return nullptr;
                }
                else if (size < g_allocatedVector[vectorIdx].requestedSizeBytes)
                    return pOriginal;
                else
                {
                    void *pNew = allocCallbackFunc(pUserData, size, alignment, allocationScope);

                    if (pNew != nullptr)
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
        return nullptr;
    }
    else
        return allocCallbackFunc(pUserData, size, alignment, allocationScope);
}

tcu::TestStatus createInstanceDeviceIntentionalAllocFail(Context &context)
{
    const PlatformInterface &vkp        = context.getPlatformInterface();
    const uint32_t chosenDevice         = context.getTestContext().getCommandLine().getVKDeviceId() - 1;
    VkInstance instance                 = VK_NULL_HANDLE;
    VkDevice device                     = VK_NULL_HANDLE;
    uint32_t physicalDeviceCount        = 0;
    uint32_t queueFamilyCount           = 0;
    uint32_t queueFamilyIndex           = 0;
    const float queuePriority           = 0.0f;
    VkInstanceCreateFlags instanceFlags = 0u;
    uint32_t instanceExtCount           = 0u;
    const char **instanceExtensions     = nullptr;

    const VkAllocationCallbacks allocationCallbacks = {
        nullptr,             // userData
        allocCallbackFunc,   // pfnAllocation
        reallocCallbackFunc, // pfnReallocation
        freeCallbackFunc,    // pfnFree
        nullptr,             // pfnInternalAllocation
        nullptr              // pfnInternalFree
    };
    const VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO, // sType
        nullptr,                            // pNext
        "appName",                          // pApplicationName
        0u,                                 // applicationVersion
        "engineName",                       // pEngineName
        0u,                                 // engineVersion
        VK_API_VERSION_1_0                  // apiVersion
    };

#ifndef CTS_USES_VULKANSC
    std::vector<vk::VkExtensionProperties> availableExtensions =
        vk::enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr);
    const char *portabilityExtension[] = {"VK_KHR_portability_enumeration"};
    if (vk::isExtensionStructSupported(availableExtensions, vk::RequiredExtension("VK_KHR_portability_enumeration")))
    {
        instanceExtCount   = 1u;
        instanceExtensions = portabilityExtension;
        instanceFlags |= vk::VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif // CTS_USES_VULKANSC

    const VkInstanceCreateInfo instanceCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // sType
        nullptr,                                // pNext
        instanceFlags,                          // flags
        &appInfo,                               // pApplicationInfo
        0u,                                     // enabledLayerCount
        nullptr,                                // ppEnabledLayerNames
        instanceExtCount,                       // enabledExtensionCount
        instanceExtensions                      // ppEnabledExtensionNames
    };

    uint32_t failIndex       = 0;
    VkResult result          = VK_SUCCESS;
    size_t max_allowed_alloc = 0;

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

            if (failIndex >= static_cast<uint32_t>(max_allowed_alloc))
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

        const InstanceDriver instanceDriver(vkp, instance);
        const InstanceInterface &vki(instanceDriver);

        result = vki.enumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

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

        vki.getPhysicalDeviceQueueFamilyProperties(physicalDevices[chosenDevice], &queueFamilyCount, nullptr);

        if (queueFamilyCount == 0u)
            return tcu::TestStatus::fail("getPhysicalDeviceQueueFamilyProperties returned zero queue families");

        vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

        vki.getPhysicalDeviceQueueFamilyProperties(physicalDevices[chosenDevice], &queueFamilyCount,
                                                   queueFamilies.data());

        if (queueFamilyCount == 0u)
            return tcu::TestStatus::fail("getPhysicalDeviceQueueFamilyProperties returned zero queue families");

        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                queueFamilyIndex = i;
                break;
            }
        }

        const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
            nullptr,                                    // pNext
            (VkDeviceQueueCreateFlags)0u,               // flags
            queueFamilyIndex,                           // queueFamilyIndex
            1u,                                         // queueCount
            &queuePriority                              // pQueuePriorities
        };

        void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
        VkDeviceObjectReservationCreateInfo memReservationInfo =
            context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() :
                                                                       resetDeviceObjectReservationCreateInfo();
        memReservationInfo.pNext = pNext;
        pNext                    = &memReservationInfo;

        VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
        sc10Features.pNext                              = pNext;
        pNext                                           = &sc10Features;
#endif // CTS_USES_VULKANSC

        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // sType
            pNext,                                // pNext
            (VkDeviceCreateFlags)0u,              // flags
            1u,                                   // queueCreateInfoCount
            &deviceQueueCreateInfo,               // pQueueCreateInfos
            0u,                                   // enabledLayerCount
            nullptr,                              // ppEnabledLayerNames
            0u,                                   // enabledExtensionCount
            nullptr,                              // ppEnabledExtensionNames
            nullptr                               // pEnabledFeatures
        };

        result = createUncheckedDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vki,
                                       physicalDevices[chosenDevice], &deviceCreateInfo, &allocationCallbacks, &device);

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

        DeviceDriver(vkp, instance, device, context.getUsedApiVersion(), context.getTestContext().getCommandLine())
            .destroyDevice(device, &allocationCallbacks);
        vki.destroyInstance(instance, &allocationCallbacks);
        if (max_allowed_alloc == 0)
        {
            max_allowed_alloc = g_allocationsCount + 100;
            result            = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        freeAllocTracker();
    } while (result == VK_ERROR_OUT_OF_HOST_MEMORY);

    return tcu::TestStatus::pass("Pass");
}

#endif // CTS_USES_VULKANSC

} // namespace

static inline void addFunctionCaseInNewSubgroup(tcu::TestContext &testCtx, tcu::TestCaseGroup *group,
                                                const std::string &subgroupName, FunctionInstance0::Function testFunc)
{
    de::MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, subgroupName.c_str()));
    addFunctionCase(subgroup.get(), "basic", testFunc);
    group->addChild(subgroup.release());
}

static inline void addFunctionCaseInNewSubgroup(tcu::TestContext &testCtx, tcu::TestCaseGroup *group,
                                                const std::string &subgroupName,
                                                FunctionSupport0::Function checkSupport,
                                                FunctionInstance0::Function testFunc)
{
    de::MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, subgroupName.c_str()));
    addFunctionCase(subgroup.get(), "basic", checkSupport, testFunc);
    group->addChild(subgroup.release());
}

template <typename Arg0>
static void addFunctionCaseInNewSubgroup(tcu::TestContext &testCtx, tcu::TestCaseGroup *group,
                                         const std::string &subgroupName,
                                         typename FunctionSupport1<Arg0>::Function checkSupport,
                                         typename FunctionInstance1<Arg0>::Function testFunc, Arg0 arg0)
{
    de::MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, subgroupName.c_str()));
    subgroup->addChild(createFunctionCase<Arg0>(testCtx, "basic", checkSupport, testFunc, arg0));
    group->addChild(subgroup.release());
}

tcu::TestCaseGroup *createDeviceInitializationTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> deviceInitializationTests(new tcu::TestCaseGroup(testCtx, "device_init"));

    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_instance_name_version",
                                 createInstanceTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_instance_invalid_api_version",
                                 createInstanceWithInvalidApiVersionTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_instance_null_appinfo",
                                 createInstanceWithNullApplicationInfoTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_instance_unsupported_extensions",
                                 createInstanceWithUnsupportedExtensionsTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_instance_extension_name_abuse",
                                 createInstanceWithExtensionNameAbuseTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_instance_layer_name_abuse",
                                 createInstanceWithLayerNameAbuseTest);
#ifndef CTS_USES_VULKANSC
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "enumerate_devices_alloc_leak",
                                 enumerateDevicesAllocLeakTest);
#endif // CTS_USES_VULKANSC
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device", createDeviceTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_multiple_devices",
                                 createMultipleDevicesTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_unsupported_extensions",
                                 createDeviceWithUnsupportedExtensionsTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_various_queue_counts",
                                 createDeviceWithVariousQueueCountsTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_global_priority",
                                 checkGlobalPrioritySupport, createDeviceWithGlobalPriorityTest, false);
#ifndef CTS_USES_VULKANSC
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_global_priority_khr",
                                 checkGlobalPrioritySupport, createDeviceWithGlobalPriorityTest, true);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_global_priority_query",
                                 checkGlobalPriorityQuerySupport, createDeviceWithQueriedGlobalPriorityTest, false);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_global_priority_query_khr",
                                 checkGlobalPriorityQuerySupport, createDeviceWithQueriedGlobalPriorityTest, true);
#endif // CTS_USES_VULKANSC
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_features2",
                                 createDeviceFeatures2Test);
    {
        de::MovePtr<tcu::TestCaseGroup> subgroup(new tcu::TestCaseGroup(testCtx, "create_device_unsupported_features"));
        addFunctionCase(subgroup.get(), "core", createDeviceWithUnsupportedFeaturesTest);
        addSeparateUnsupportedFeatureTests(subgroup.get());
        deviceInitializationTests->addChild(subgroup.release());
    }
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2",
                                 createDeviceQueue2Test);
#ifndef CTS_USES_VULKANSC
    // Removed because in main process this test does not really create any instance nor device and functions creating it always return VK_SUCCESS
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(),
                                 "create_instance_device_intentional_alloc_fail",
                                 createInstanceDeviceIntentionalAllocFail);
#endif // CTS_USES_VULKANSC

    // Tests using a single Queue Family when creating a device.
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2_two_queues",
                                 checkProtectedMemorySupport, createDeviceQueue2WithTwoQueuesSmokeTest);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2_all_protected",
                                 checkProtectedMemorySupport, createDeviceQueue2WithAllProtectedQueues);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2_all_unprotected",
                                 checkProtectedMemorySupport, createDeviceQueue2WithAllUnprotectedQueues);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2_split",
                                 checkProtectedMemorySupport, createDeviceQueue2WithNProtectedAndMUnprotectedQueues);

    // Tests using multiple Queue Families when creating a device.
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2_all_families",
                                 checkProtectedMemorySupport, createDeviceQueue2WithAllFamilies);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(),
                                 "create_device_queue2_all_families_protected", checkProtectedMemorySupport,
                                 createDeviceQueue2WithAllFamiliesProtected);
    addFunctionCaseInNewSubgroup(testCtx, deviceInitializationTests.get(), "create_device_queue2_all_combinations",
                                 checkProtectedMemorySupport, createDeviceQueue2WithMultipleQueueCombinations);

    return deviceInitializationTests.release();
}

} // namespace api
} // namespace vkt
