/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Collabora, Ltd.
 * Copyright (c) 2017 Khronos Group
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
* \brief API Maintenance11 Check test - checks structs and function from VK_KHR_maintenance11
*//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"

#include "vkQueryUtil.hpp"

#include "vktApiMaintenance11Tests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>
#include <limits>
#include <utility>
#include <algorithm>
#include <map>
#include <set>

#ifndef CTS_USES_VULKANSC

using namespace vk;

using std::vector;

namespace vkt
{

namespace api
{

namespace
{

bool isPowerOfTwo32(uint32_t x)
{
    return x > 0 && (x & (x - 1)) == 0;
}

class Maintenance11QueuePropsTestInstance : public TestInstance
{
public:
    Maintenance11QueuePropsTestInstance(Context &ctx) : TestInstance(ctx)
    {
    }

    virtual tcu::TestStatus iterate(void)
    {
        const InstanceInterface &vki(m_context.getInstanceInterface());
        const VkPhysicalDevice physicalDevice(m_context.getPhysicalDevice());

        uint32_t numQueues = 0;
        vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &numQueues, nullptr);

        vector<VkQueueFamilyOptimalImageTransferGranularityPropertiesKHR> transferGranularityProps(numQueues);
        vector<VkQueueFamilyProperties2> queueFamilyProps(numQueues);
        for (uint32_t i = 0; i < numQueues; i++)
        {
            transferGranularityProps[i]                                 = initVulkanStructure();
            transferGranularityProps[i].optimalImageTransferGranularity = makeExtent3D(1337, 1337, 1337);
            queueFamilyProps[i] = initVulkanStructure(&transferGranularityProps[i]);
        }
        vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &numQueues, queueFamilyProps.data());

        for (size_t i = 0; i < queueFamilyProps.size(); i++)
        {
            const auto &props                           = queueFamilyProps[i].queueFamilyProperties;
            const auto &optimalImageTransferGranularity = transferGranularityProps[i].optimalImageTransferGranularity;

            if ((props.queueFlags & VK_QUEUE_TRANSFER_BIT) == 0)
                continue;

            if (props.minImageTransferGranularity.width != 1 || props.minImageTransferGranularity.height != 1 ||
                props.minImageTransferGranularity.depth != 1)
            {
                return tcu::TestStatus::fail("queue family " + std::to_string(i) + ": minImageTransferGranularity is " +
                                             std::to_string(i) + " but must be 1x1x1");
            }

            bool optimalImageTransferGranularityIsValid =
                (optimalImageTransferGranularity.width == 0 && optimalImageTransferGranularity.height == 0 &&
                 optimalImageTransferGranularity.depth == 0) ||
                (isPowerOfTwo32(optimalImageTransferGranularity.width) &&
                 isPowerOfTwo32(optimalImageTransferGranularity.height) &&
                 isPowerOfTwo32(optimalImageTransferGranularity.depth));
            if (!optimalImageTransferGranularityIsValid)
            {
                return tcu::TestStatus::fail("queue family " + std::to_string(i) +
                                             ": optimalImageTransferGranularity is " +
                                             std::to_string(optimalImageTransferGranularity.width) + "x" +
                                             std::to_string(optimalImageTransferGranularity.height) + "x" +
                                             std::to_string(optimalImageTransferGranularity.depth) +
                                             " but must be either all zeros, or the sides must be powers of two");
            }
        }

        return tcu::TestStatus::pass("");
    }
};

class Maintenance11QueuePropsTestCase : public TestCase
{
public:
    Maintenance11QueuePropsTestCase(tcu::TestContext &testCtx) : TestCase(testCtx, "queue_properties")
    {
    }

    virtual ~Maintenance11QueuePropsTestCase(void)
    {
    }
    virtual void checkSupport(Context &ctx) const
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance11");
    }
    virtual TestInstance *createInstance(Context &ctx) const
    {
        return new Maintenance11QueuePropsTestInstance(ctx);
    }

private:
};

} // namespace

tcu::TestCaseGroup *createMaintenance11Tests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> main11Tests(
        new tcu::TestCaseGroup(testCtx, "maintenance11", "Maintenance11 Tests"));
    main11Tests->addChild(new Maintenance11QueuePropsTestCase(testCtx));

    return main11Tests.release();
}

} // namespace api
} // namespace vkt

#endif // CTS_USES_VULKANSC
