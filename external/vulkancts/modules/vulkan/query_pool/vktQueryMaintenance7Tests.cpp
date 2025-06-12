/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 Khronos Group
 * Copyright (c) 2024 LunarG, Inc.
 * Copyright (c) 2024 Google LLC
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
* \brief Verify correct 32 bit wrapping behavior for queries when
*        maintenance7 is enabled.
*//*--------------------------------------------------------------------*/

#include "vkQueryUtil.hpp"
#include "vktQueryMaintenance7Tests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"

#ifndef CTS_USES_VULKANSC

namespace vkt::QueryPool
{

constexpr uint32_t MIN_TIMESTAMP_VALID_BITS = 36;
constexpr uint32_t MAX_TIMESTAMP_VALID_BITS = 64;

class Maintenance7QueryInstance : public vkt::TestInstance
{
    const bool m_maint7Enabled = false;
    uint64_t m_timestampMask   = 0;
    vk::Move<vk::VkQueryPool> m_queryPool;
    vk::Move<vk::VkCommandPool> m_cmdPool;
    vk::Move<vk::VkCommandBuffer> m_cmdBuffer;

public:
    Maintenance7QueryInstance(vkt::Context &context, bool maint7Enabled)
        : vkt::TestInstance(context)
        , m_maint7Enabled(maint7Enabled)
    {
        recordComands();
    }

    // Checks support for timestamps and returns the timestamp mask.
    uint64_t checkTimestampsSupported(const vk::InstanceInterface &vki, const vk::VkPhysicalDevice physDevice,
                                      const uint32_t queueFamilyIndex)
    {
        const std::vector<vk::VkQueueFamilyProperties> queueProperties =
            vk::getPhysicalDeviceQueueFamilyProperties(vki, physDevice);
        DE_ASSERT(queueFamilyIndex < queueProperties.size());
        const uint32_t &validBits = queueProperties[queueFamilyIndex].timestampValidBits;

        if (validBits == 0)
            throw tcu::NotSupportedError("Queue does not support timestamps");

        checkValidBits(validBits, queueFamilyIndex);
        return timestampMaskFromValidBits(validBits);
    }

    // Checks the number of valid bits for the given queue meets the spec requirements.
    void checkValidBits(uint32_t validBits, uint32_t queueFamilyIndex)
    {
        if (validBits < MIN_TIMESTAMP_VALID_BITS || validBits > MAX_TIMESTAMP_VALID_BITS)
        {
            std::ostringstream msg;
            msg << "Invalid value for timestampValidBits (" << validBits << ") in queue index " << queueFamilyIndex;
            TCU_FAIL(msg.str());
        }
    }

    // Returns the timestamp mask given the number of valid timestamp bits.
    uint64_t timestampMaskFromValidBits(uint32_t validBits)
    {
        return ((validBits == MAX_TIMESTAMP_VALID_BITS) ? std::numeric_limits<uint64_t>::max() :
                                                          ((1ULL << validBits) - 1));
    }

    void recordComands()
    {
        const vk::DeviceInterface &vk   = m_context.getDeviceInterface();
        const vk::VkDevice vkDevice     = m_context.getDevice();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

        // Check support for timestamp queries
        m_timestampMask =
            checkTimestampsSupported(m_context.getInstanceInterface(), m_context.getPhysicalDevice(), queueFamilyIndex);

        const vk::VkQueryPoolCreateInfo queryPoolParams = {
            vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType               sType;
            nullptr,                                      // const void*                   pNext;
            0u,                                           // VkQueryPoolCreateFlags        flags;
            vk::VK_QUERY_TYPE_TIMESTAMP,                  // VkQueryType                   queryType;
            1u,                                           // uint32_t                      entryCount;
            0u,                                           // VkQueryPipelineStatisticFlags pipelineStatistics;
        };

        m_queryPool = createQueryPool(vk, vkDevice, &queryPoolParams);
        m_cmdPool   = createCommandPool(vk, vkDevice, vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        // Prepare command buffer.
        vk::beginCommandBuffer(vk, *m_cmdBuffer);
        vk.cmdResetQueryPool(*m_cmdBuffer, *m_queryPool, 0u, 1u);
        vk.cmdWriteTimestamp(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, *m_queryPool, 0u);
        vk::endCommandBuffer(vk, *m_cmdBuffer);
    }

private:
    tcu::TestStatus iterate(void) override;
};

tcu::TestStatus Maintenance7QueryInstance::iterate(void)
{

    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice vkDevice   = m_context.getDevice();
    const vk::VkQueue queue       = m_context.getDeviceQueueInfo(0).queue;

    uint32_t tsGet32Bits;
    uint64_t tsGet64Bits;
    constexpr uint32_t maxDeUint32Value = std::numeric_limits<uint32_t>::max();

    vk::submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

    // Get results with vkGetQueryPoolResults().
    VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(tsGet32Bits), &tsGet32Bits,
                                    sizeof(tsGet32Bits), vk::VK_QUERY_RESULT_WAIT_BIT));
    VK_CHECK(vk.getQueryPoolResults(vkDevice, *m_queryPool, 0u, 1u, sizeof(tsGet64Bits), &tsGet64Bits,
                                    sizeof(tsGet64Bits), (vk::VK_QUERY_RESULT_64_BIT | vk::VK_QUERY_RESULT_WAIT_BIT)));

    // Check results are consistent.
    tcu::TestLog &log = m_context.getTestContext().getLog();
    if (m_maint7Enabled)
    {
        // If maintenance7 is supported, 32 bit queries _must_ be equivalent to the lower 32 bits of the 64 bit query
        if ((tsGet64Bits & maxDeUint32Value) == tsGet32Bits)
        {
            return tcu::TestStatus::pass("Pass");
        }
        else
        {
            log << tcu::TestLog::Message
                << "Maintenance 7 is enabled, but the 32 bit query value does equal the lower 32 bits of the 64 bit "
                   "query."
                << tcu::TestLog::EndMessage;
        }
    }
    else
    {
        if (((tsGet64Bits & maxDeUint32Value) == tsGet32Bits) ||
            ((tsGet64Bits > maxDeUint32Value) && (maxDeUint32Value == tsGet32Bits)))
        {
            return tcu::TestStatus::pass("Pass");
        }
        else
        {
            log << tcu::TestLog::Message
                << "Maintenance 7 is disabled, but the 32 bit query value does equal the lower 32 bits of the 64 bit "
                   "query nor is it saturated."
                << tcu::TestLog::EndMessage;
        }
    }

    return tcu::TestStatus::fail("Fail");
}

class Maintenance7QueryFeatureTestCase : public TestCase
{
public:
    Maintenance7QueryFeatureTestCase(tcu::TestContext &testCtx, const char *name, const bool maint7Enabled)
        : TestCase(testCtx, name)
        , m_maint7Enabled(maint7Enabled)
    {
    }

    virtual void checkSupport(Context &ctx) const override
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance7");

        if (m_maint7Enabled && !ctx.getMaintenance7Features().maintenance7)
        {
            TCU_THROW(NotSupportedError, "Requires maintenance 7 feature which is not supported");
        }
    }

    std::string getRequiredCapabilitiesId() const override
    {
        std::string name = typeid(Maintenance7QueryFeatureTestCase).name();
        return m_maint7Enabled ? (name + "_maint7") : name;
    }

    void initDeviceCapabilities(DevCaps &caps) override
    {
        if (m_maint7Enabled)
        {
            caps.addExtension("VK_KHR_maintenance7");
            caps.addFeature<vk::VkPhysicalDeviceMaintenance7FeaturesKHR>();
        }
    }

    virtual TestInstance *createInstance(Context &context) const override
    {
        return new Maintenance7QueryInstance(context, m_maint7Enabled);
    }

private:
    const bool m_maint7Enabled;
};

tcu::TestCaseGroup *createQueryMaintenance7Tests(tcu::TestContext &testCtx)
{

    de::MovePtr<tcu::TestCaseGroup> maint7Tests(
        new tcu::TestCaseGroup(testCtx, "maintenance7", "Maintenance7 Query Feature Tests"));

    maint7Tests->addChild(new Maintenance7QueryFeatureTestCase(testCtx, "query_32b_wrap_required", true));
    maint7Tests->addChild(new Maintenance7QueryFeatureTestCase(testCtx, "query_32b_wrap_notrequired", false));

    return maint7Tests.release();
}

} // namespace vkt::QueryPool

#endif // CTS_USES_VULKANSC
