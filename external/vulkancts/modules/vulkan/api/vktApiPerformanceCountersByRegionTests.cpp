/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (C) 2025 Arm Limited or its affiliates.
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
 * \brief VK_ARM_performance_counters_by_region tests.
 *//*--------------------------------------------------------------------*/

#include "vktApiPerformanceCountersByRegionTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuCommandLine.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deMath.h"

#include <limits>

#ifndef CTS_USES_VULKANSC

using namespace vk;

using tcu::TestLog;

using std::string;
using std::vector;

namespace vkt
{
namespace
{
class PerformanceCountersByRegionRenderPassBasicTestInstance : public TestInstance
{
public:
    PerformanceCountersByRegionRenderPassBasicTestInstance(Context &context);
    ~PerformanceCountersByRegionRenderPassBasicTestInstance(void);

    tcu::TestStatus iterate(void);

private:
    void checkCounterEnumeration(std::vector<VkPerformanceCounterARM> &perfCounters, uint32_t count,
                                 uint32_t dummyValue);
    void checkCounterDescEnumeration(std::vector<VkPerformanceCounterDescriptionARM> &perfCounterDescs, uint32_t count,
                                     uint32_t dummyValue);

    tcu::ResultCollector m_resultCollector;
};

PerformanceCountersByRegionRenderPassBasicTestInstance::PerformanceCountersByRegionRenderPassBasicTestInstance(
    Context &context)
    : TestInstance(context)
{
}

PerformanceCountersByRegionRenderPassBasicTestInstance::~PerformanceCountersByRegionRenderPassBasicTestInstance()
{
}

void PerformanceCountersByRegionRenderPassBasicTestInstance::checkCounterEnumeration(
    std::vector<VkPerformanceCounterARM> &perfCounters, uint32_t count, uint32_t dummyValue)
{
    DE_ASSERT(count + 1 <= perfCounters.size());

    for (uint32_t idx = 0; idx < count; ++idx)
    {
        if (perfCounters[idx].counterID == dummyValue)
        {
            m_resultCollector.fail("Too few counters were written.");
        }
    }

    if (perfCounters[count].counterID != dummyValue)
    {
        m_resultCollector.fail("Counters beyond the requested limit were overwritten.");
    }
}

void PerformanceCountersByRegionRenderPassBasicTestInstance::checkCounterDescEnumeration(
    std::vector<VkPerformanceCounterDescriptionARM> &perfCounterDescs, uint32_t count, uint32_t dummyValue)
{
    DE_ASSERT(count + 1 <= perfCounterDescs.size());

    for (uint32_t idx = 0; idx < count; ++idx)
    {
        if (perfCounterDescs[idx].flags == dummyValue)
        {
            m_resultCollector.fail("Too few counter descriptions were written.");
            break;
        }
    }

    if (perfCounterDescs[count].flags != dummyValue)
    {
        m_resultCollector.fail("Counter descriptions beyond the requested limit were overwritten.");
    }
}

void resetCounters(std::vector<VkPerformanceCounterARM> &perfCounters,
                   std::vector<VkPerformanceCounterDescriptionARM> &perfCounterDescs, uint32_t dummyValue)
{
    for (uint32_t idx = 0; idx < perfCounters.size(); idx++)
    {
        perfCounters[idx].sType     = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR;
        perfCounters[idx].counterID = dummyValue;
    }

    for (uint32_t idx = 0; idx < perfCounterDescs.size(); idx++)
    {
        perfCounterDescs[idx].sType   = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR;
        perfCounterDescs[idx].flags   = dummyValue;
        perfCounterDescs[idx].name[0] = 0u;
    }
}

uint32_t findDummyValue(Context &context, std::vector<VkPerformanceCounterARM> &perfCounters,
                        std::vector<VkPerformanceCounterDescriptionARM> &perfCounterDescs, uint32_t count)
{
    VK_CHECK(context.getInstanceInterface().enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
        context.getPhysicalDevice(), context.getUniversalQueueFamilyIndex(), &count, perfCounters.data(),
        perfCounterDescs.data()));

    uint32_t dummyValue;
    for (dummyValue = UINT32_MAX; dummyValue > 0; dummyValue--)
    {
        bool found = false;
        for (uint32_t i = 0; i < perfCounters.size(); ++i)
        {
            if (perfCounters[i].counterID == dummyValue)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            for (uint32_t i = 0; i < perfCounterDescs.size(); ++i)
            {
                if (perfCounterDescs[i].flags == dummyValue)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            break;
        }
    }

    DE_ASSERT(dummyValue != 0u);

    return dummyValue;
}

tcu::TestStatus PerformanceCountersByRegionRenderPassBasicTestInstance::iterate(void)
{
    uint32_t queueFamilyIndex = 0;

    uint32_t perfCounterCount;

    const InstanceInterface &vki           = m_context.getInstanceInterface();
    const VkPhysicalDevice &physicalDevice = m_context.getPhysicalDevice();

    // Get the count of counters supported.
    vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(physicalDevice, queueFamilyIndex,
                                                                         &perfCounterCount, nullptr, nullptr);

    if (perfCounterCount == 0)
    {
        m_resultCollector.fail("No counters found.");
        return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
    }

    std::vector<VkPerformanceCounterARM> perfCounters(perfCounterCount + 1);
    std::vector<VkPerformanceCounterDescriptionARM> perfCounterDescs(perfCounterCount + 1);

    // A value not used for counterId or flags, to use when detecting which counter structs were written.
    const uint32_t dummyValue = findDummyValue(m_context, perfCounters, perfCounterDescs, perfCounterCount);

    if (perfCounterCount > 1)
    {
        // Tests with space for fewer than the total number of counters.
        {
            uint32_t count = 1;
            resetCounters(perfCounters, perfCounterDescs, dummyValue);
            vk::VkResult result = vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
                physicalDevice, queueFamilyIndex, &count, perfCounters.data(), nullptr);
            if (count > 1)
            {
                m_resultCollector.fail("Unexpected count when requesting few counters.");
            }
            else
            {
                checkCounterEnumeration(perfCounters, count, dummyValue);
            }

            if (result != vk::VK_INCOMPLETE)
            {
                m_resultCollector.fail("Expected VK_INCOMPLETE.");
            }
        }

        {
            uint32_t count = 1;
            resetCounters(perfCounters, perfCounterDescs, dummyValue);
            vk::VkResult result = vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
                physicalDevice, queueFamilyIndex, &count, nullptr, perfCounterDescs.data());
            if (count > 1)
            {
                m_resultCollector.fail("Unexpected count when requesting few counters.");
            }
            else
            {
                checkCounterDescEnumeration(perfCounterDescs, count, dummyValue);
            }

            if (result != vk::VK_INCOMPLETE)
            {
                m_resultCollector.fail("Expected VK_INCOMPLETE.");
            }
        }

        {
            uint32_t count = 1;
            resetCounters(perfCounters, perfCounterDescs, dummyValue);
            vk::VkResult result = vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
                physicalDevice, queueFamilyIndex, &count, perfCounters.data(), perfCounterDescs.data());
            if (count > 1)
            {
                m_resultCollector.fail("Unexpected count when requesting few counters.");
            }
            else
            {
                checkCounterEnumeration(perfCounters, count, dummyValue);
                checkCounterDescEnumeration(perfCounterDescs, count, dummyValue);
            }

            if (result != vk::VK_INCOMPLETE)
            {
                m_resultCollector.fail("Expected VK_INCOMPLETE.");
            }
        }
    }

    // Tests with space for the total number of counters.
    {
        uint32_t count = perfCounterCount;
        resetCounters(perfCounters, perfCounterDescs, dummyValue);
        VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
            physicalDevice, queueFamilyIndex, &count, perfCounters.data(), nullptr));

        if (count != perfCounterCount)
        {
            m_resultCollector.fail("Unexpected number of performance counters returned.");
        }
        else
        {
            checkCounterEnumeration(perfCounters, count, dummyValue);
        }
    }

    {
        uint32_t count = perfCounterCount;
        resetCounters(perfCounters, perfCounterDescs, dummyValue);
        VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
            physicalDevice, queueFamilyIndex, &count, nullptr, perfCounterDescs.data()));

        if (count != perfCounterCount)
        {
            m_resultCollector.fail("Unexpected number of performance counters returned.");
        }
        else
        {
            checkCounterDescEnumeration(perfCounterDescs, perfCounterCount, dummyValue);
        }
    }

    {
        uint32_t count = perfCounterCount;
        resetCounters(perfCounters, perfCounterDescs, dummyValue);
        VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
            physicalDevice, queueFamilyIndex, &count, perfCounters.data(), perfCounterDescs.data()));

        if (count != perfCounterCount)
        {
            m_resultCollector.fail("Unexpected number of performance counters returned.");
        }
        else
        {
            checkCounterEnumeration(perfCounters, perfCounterCount, dummyValue);
            checkCounterDescEnumeration(perfCounterDescs, perfCounterCount, dummyValue);
        }
    }

    // Test with space for more counters than needed.
    {
        uint32_t count = perfCounterCount + 1;
        resetCounters(perfCounters, perfCounterDescs, dummyValue);
        VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceCountersByRegionARM(
            physicalDevice, queueFamilyIndex, &count, perfCounters.data(), perfCounterDescs.data()));

        if (count != perfCounterCount)
        {
            m_resultCollector.fail("Unexpected number of performance counters returned.");
        }
        else
        {
            checkCounterEnumeration(perfCounters, perfCounterCount, dummyValue);
            checkCounterDescEnumeration(perfCounterDescs, perfCounterCount, dummyValue);
        }
    }

    return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

class APIPerformanceCountersByRegionRenderPassBasicTestCase : public TestCase
{
public:
    // Check if vkGetDeviceProcAddr returns NULL for functions beyond app version.
    APIPerformanceCountersByRegionRenderPassBasicTestCase(tcu::TestContext &testCtx)
        : TestCase(testCtx, "enumerate_counters")
    {
    }

    virtual void checkSupport(Context &context) const
    {
        const InstanceInterface &vki        = context.getInstanceInterface();
        vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

        context.requireDeviceFunctionality("VK_ARM_performance_counters_by_region");
        context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

        VkPhysicalDeviceFeatures2 features2                                                = initVulkanStructure();
        VkPhysicalDevicePerformanceCountersByRegionFeaturesARM performanceCountersByRegion = initVulkanStructure();
        const auto addFeatures = makeStructChainAdder(&features2);

        addFeatures(&performanceCountersByRegion);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

        if (!performanceCountersByRegion.performanceCountersByRegion)
            TCU_THROW(NotSupportedError, "VkPhysicalDevicePerformanceCountersByRegionFeaturesARM is not supported");
    }

    virtual TestInstance *createInstance(Context &ctx) const
    {
        return new PerformanceCountersByRegionRenderPassBasicTestInstance(ctx);
    }
};

} // namespace

tcu::TestCaseGroup *createRenderPassPerformanceCountersByRegionApiTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> versionTests(new tcu::TestCaseGroup(testCtx, "performance_counters_by_region"));

    versionTests->addChild(new APIPerformanceCountersByRegionRenderPassBasicTestCase(testCtx));

    return versionTests.release();
}

} // namespace vkt

#endif
