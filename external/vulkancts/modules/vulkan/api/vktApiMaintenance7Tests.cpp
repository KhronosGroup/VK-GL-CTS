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
*//*--------------------------------------------------------------------*/

#include "vkQueryUtil.hpp"

#include "tcuTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#ifndef CTS_USES_VULKANSC

namespace vkt
{
namespace api
{

class Maintenance7LayeredApiVulkanPropertiesTestInstance : public vkt::TestInstance
{
public:
    Maintenance7LayeredApiVulkanPropertiesTestInstance(vkt::Context &context) : vkt::TestInstance(context)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus Maintenance7LayeredApiVulkanPropertiesTestInstance::iterate(void)
{
    const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    tcu::TestLog &log                         = m_context.getTestContext().getLog();

    vk::VkPhysicalDeviceLayeredApiPropertiesListKHR layeredApiPropertiesList = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_LIST_KHR, // VkStructureType sType;
        DE_NULL,                                                               // void* pNext;
        0u,                                                                    // uint32_t layeredApiCount;
        DE_NULL, // VkPhysicalDeviceLayeredApiPropertiesKHR* pLayeredApis;
    };
    vk::VkPhysicalDeviceProperties2 properties2 = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, // VkStructureType sType;
        &layeredApiPropertiesList,                          // void* pNext;
        {},                                                 // VkPhysicalDeviceProperties properties;
    };
    vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);
    if (layeredApiPropertiesList.layeredApiCount > 0)
    {
        std::vector<vk::VkPhysicalDeviceLayeredApiVulkanPropertiesKHR> layeredApiVulkanProperties(
            layeredApiPropertiesList.layeredApiCount);
        std::vector<vk::VkPhysicalDeviceLayeredApiPropertiesKHR> layeredApiProperties(
            layeredApiPropertiesList.layeredApiCount);
        for (uint32_t i = 0; i < layeredApiPropertiesList.layeredApiCount; ++i)
        {
            layeredApiVulkanProperties[i] = {};
            layeredApiVulkanProperties[i].sType =
                vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_VULKAN_PROPERTIES_KHR;
            layeredApiVulkanProperties[i].properties.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            memset(&layeredApiVulkanProperties[i].properties.properties.limits, 255,
                   sizeof(vk::VkPhysicalDeviceLimits));
            memset(&layeredApiVulkanProperties[i].properties.properties.sparseProperties, 255,
                   sizeof(vk::VkPhysicalDeviceSparseProperties));

            layeredApiProperties[i]       = {};
            layeredApiProperties[i].sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_KHR;
            layeredApiProperties[i].pNext = &layeredApiVulkanProperties[i];
        }
        layeredApiPropertiesList.pLayeredApis = layeredApiProperties.data();
        vki.getPhysicalDeviceProperties2(physicalDevice, &properties2);

        for (uint32_t i = 0; i < layeredApiPropertiesList.layeredApiCount; ++i)
        {

            if (layeredApiProperties[i].layeredAPI == vk::VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR)
            {
                if (layeredApiProperties[i].deviceID != layeredApiVulkanProperties[i].properties.properties.deviceID)
                {
                    log << tcu::TestLog::Message
                        << "deviceID of VkPhysicalDeviceLayeredApiPropertiesKHR and "
                           "VkPhysicalDeviceLayeredApiVulkanPropertiesKHR::properties::properties at index "
                        << i << " do not match" << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
                if (layeredApiProperties[i].vendorID != layeredApiVulkanProperties[i].properties.properties.vendorID)
                {
                    log << tcu::TestLog::Message
                        << "vendorID of VkPhysicalDeviceLayeredApiPropertiesKHR and "
                           "VkPhysicalDeviceLayeredApiVulkanPropertiesKHR::properties::properties at index "
                        << i << " do not match" << tcu::TestLog::EndMessage;
                    return tcu::TestStatus::fail("Fail");
                }
            }

            uint8_t *limits = reinterpret_cast<uint8_t *>(&layeredApiVulkanProperties[i].properties.properties.limits);
            for (uint32_t j = 0; j < sizeof(vk::VkPhysicalDeviceLimits) / sizeof(uint8_t); ++j)
            {
                if (layeredApiProperties[i].layeredAPI == vk::VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR)
                {
                    if (limits[j] != 0)
                    {
                        log << tcu::TestLog::Message << "VkPhysicalDeviceLayeredApiPropertiesKHR[" << i
                            << "].layerAPI is VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR, but "
                               "VkPhysicalDeviceLayeredApiVulkanPropertiesKHR::properties::limits in pNext was not "
                               "zero-filled"
                            << tcu::TestLog::EndMessage;

                        return tcu::TestStatus::fail("Fail");
                    }
                }
                else
                {
                    if (limits[j] != 255)
                    {
                        log << tcu::TestLog::Message << "VkPhysicalDeviceLayeredApiPropertiesKHR[" << i
                            << "].layerAPI is VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR, but "
                               "VkPhysicalDeviceLayeredApiVulkanPropertiesKHR::properties::limits in pNext was not "
                               "ignored"
                            << tcu::TestLog::EndMessage;

                        return tcu::TestStatus::fail("Fail");
                    }
                }
            }
            uint8_t *sparseProperties =
                reinterpret_cast<uint8_t *>(&layeredApiVulkanProperties[i].properties.properties.sparseProperties);
            for (uint32_t j = 0; j < sizeof(vk::VkPhysicalDeviceSparseProperties) / sizeof(uint8_t); ++j)
            {
                if (layeredApiProperties[i].layeredAPI == vk::VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR)
                {
                    if (sparseProperties[j] != 0)
                    {
                        log << tcu::TestLog::Message << "VkPhysicalDeviceLayeredApiPropertiesKHR[" << i
                            << "].layerAPI is VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR, but "
                               "VkPhysicalDeviceLayeredApiVulkanPropertiesKHR::properties::sparseProperties in pNext "
                               "was not zero-filled"
                            << tcu::TestLog::EndMessage;

                        return tcu::TestStatus::fail("Fail");
                    }
                }
                else
                {
                    if (sparseProperties[j] != 255)
                    {
                        log << tcu::TestLog::Message << "VkPhysicalDeviceLayeredApiPropertiesKHR[" << i
                            << "].layerAPI is VK_PHYSICAL_DEVICE_LAYERED_API_VULKAN_KHR, but "
                               "VkPhysicalDeviceLayeredApiVulkanPropertiesKHR::properties::sparseProperties in pNext "
                               "was not ignored"
                            << tcu::TestLog::EndMessage;

                        return tcu::TestStatus::fail("Fail");
                    }
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class Maintenance7LayeredApiVulkanPropertiesTestCase : public TestCase
{
public:
    Maintenance7LayeredApiVulkanPropertiesTestCase(tcu::TestContext &testCtx, const char *name)
        : TestCase(testCtx, name)
    {
    }

    virtual void checkSupport(Context &ctx) const
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance7");
    }
    virtual TestInstance *createInstance(Context &context) const
    {
        return new Maintenance7LayeredApiVulkanPropertiesTestInstance(context);
    }

private:
};

class Maintenance7TotalDynamicBuffersPropertiesTestInstance : public vkt::TestInstance
{
public:
    Maintenance7TotalDynamicBuffersPropertiesTestInstance(vkt::Context &context) : vkt::TestInstance(context)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus Maintenance7TotalDynamicBuffersPropertiesTestInstance::iterate(void)
{
    vk::VkPhysicalDeviceMaintenance7PropertiesKHR maint7Prop = vk::initVulkanStructure();
    vk::VkPhysicalDeviceProperties2 prop2                    = vk::initVulkanStructure(&maint7Prop);
    tcu::TestLog &log                                        = m_context.getTestContext().getLog();

    m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &prop2);

    // check dynamic buffers limits
    auto &deviceLimits = m_context.getDeviceProperties().limits;
    if (maint7Prop.maxDescriptorSetTotalUniformBuffersDynamic < deviceLimits.maxDescriptorSetUniformBuffersDynamic)
    {
        log << tcu::TestLog::Message
            << "maxDescriptorSetTotalUniformBuffersDynamic: " << maint7Prop.maxDescriptorSetTotalUniformBuffersDynamic
            << "is less than maxDescriptorSetUniformBuffersDynamic: "
            << deviceLimits.maxDescriptorSetUniformBuffersDynamic << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    if (maint7Prop.maxDescriptorSetTotalStorageBuffersDynamic < deviceLimits.maxDescriptorSetStorageBuffersDynamic)
    {
        log << tcu::TestLog::Message
            << "maxDescriptorSetTotalStorageBuffersDynamic: " << maint7Prop.maxDescriptorSetTotalStorageBuffersDynamic
            << "is less than maxDescriptorSetStorageBuffersDynamic: "
            << deviceLimits.maxDescriptorSetStorageBuffersDynamic << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    if (maint7Prop.maxDescriptorSetTotalBuffersDynamic <
        deviceLimits.maxDescriptorSetUniformBuffersDynamic + deviceLimits.maxDescriptorSetStorageBuffersDynamic)
    {
        log << tcu::TestLog::Message
            << "maxDescriptorSetTotalBuffersDynamic: " << maint7Prop.maxDescriptorSetTotalBuffersDynamic
            << "is less than some of maxDescriptorSetUniformBuffersDynamic: "
            << deviceLimits.maxDescriptorSetUniformBuffersDynamic
            << " and maxDescriptorSetStorageBuffersDynamic: " << deviceLimits.maxDescriptorSetStorageBuffersDynamic
            << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    // check update after bind dynamic buffers limits
    auto &deviceProp12 = m_context.getDeviceVulkan12Properties();
    if (maint7Prop.maxDescriptorSetUpdateAfterBindTotalUniformBuffersDynamic <
        deviceProp12.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic)
    {
        log << tcu::TestLog::Message << "maxDescriptorSetUpdateAfterBindTotalUniformBuffersDynamic: "
            << maint7Prop.maxDescriptorSetUpdateAfterBindTotalUniformBuffersDynamic
            << "is less than maxDescriptorSetUpdateAfterBindUniformBuffersDynamic: "
            << deviceProp12.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    if (maint7Prop.maxDescriptorSetUpdateAfterBindTotalStorageBuffersDynamic <
        deviceProp12.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic)
    {
        log << tcu::TestLog::Message << "maxDescriptorSetUpdateAfterBindTotalStorageBuffersDynamic: "
            << maint7Prop.maxDescriptorSetUpdateAfterBindTotalStorageBuffersDynamic
            << "is less than maxDescriptorSetUpdateAfterBindStorageBuffersDynamic: "
            << deviceProp12.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    if (maint7Prop.maxDescriptorSetUpdateAfterBindTotalBuffersDynamic <
        deviceProp12.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic +
            deviceProp12.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic)
    {
        log << tcu::TestLog::Message << "maxDescriptorSetUpdateAfterBindTotalBuffersDynamic: "
            << maint7Prop.maxDescriptorSetUpdateAfterBindTotalBuffersDynamic
            << "is less than some of maxDescriptorSetUpdateAfterBindUniformBuffersDynamic: "
            << deviceProp12.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic
            << " and maxDescriptorSetUpdateAfterBindStorageBuffersDynamic: "
            << deviceProp12.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class Maintenance7TotalDynamicBuffersPropertiesTestCase : public TestCase
{
public:
    Maintenance7TotalDynamicBuffersPropertiesTestCase(tcu::TestContext &testCtx, const char *name)
        : TestCase(testCtx, name)
    {
    }

    virtual void checkSupport(Context &ctx) const
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance7");
    }
    virtual TestInstance *createInstance(Context &context) const
    {
        return new Maintenance7TotalDynamicBuffersPropertiesTestInstance(context);
    }
};

tcu::TestCaseGroup *createMaintenance7Tests(tcu::TestContext &testCtx)
{

    de::MovePtr<tcu::TestCaseGroup> main7Tests(new tcu::TestCaseGroup(testCtx, "maintenance7", "Maintenance7 Tests"));

    main7Tests->addChild(new Maintenance7LayeredApiVulkanPropertiesTestCase(testCtx, "layered_api_vulkan_properties"));
    main7Tests->addChild(
        new Maintenance7TotalDynamicBuffersPropertiesTestCase(testCtx, "total_dynamic_buffers_properties"));

    return main7Tests.release();
}

} // namespace api
} // namespace vkt

#endif // CTS_USES_VULKANSC
