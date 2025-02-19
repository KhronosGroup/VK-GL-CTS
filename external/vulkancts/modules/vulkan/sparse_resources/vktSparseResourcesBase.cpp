/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesBase.cpp
 * \brief Sparse Resources Base Instance
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBase.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

struct QueueFamilyQueuesCount
{
    QueueFamilyQueuesCount() : queueCount(0u)
    {
    }

    uint32_t queueCount;
};

uint32_t findMatchingQueueFamilyIndex(const std::vector<VkQueueFamilyProperties> &queueFamilyProperties,
                                      const VkQueueFlags queueFlags, const uint32_t startIndex)
{
    for (uint32_t queueNdx = startIndex; queueNdx < queueFamilyProperties.size(); ++queueNdx)
    {
        if ((queueFamilyProperties[queueNdx].queueFlags & queueFlags) == queueFlags)
            return queueNdx;
    }

    return NO_MATCH_FOUND;
}

uint32_t findSpecificQueueFamilyIndex(const std::vector<VkQueueFamilyProperties> &queueFamilyProperties,
                                      const VkQueueFlags requestedFlags, const uint32_t startIndex)
{
    for (uint32_t queueNdx = startIndex; queueNdx < queueFamilyProperties.size(); ++queueNdx)
    {
        const VkQueueFlags queueFlags         = queueFamilyProperties[queueNdx].queueFlags;
        const VkQueueFlags coreFlags          = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
        const VkQueueFlags coreQueueFlags     = queueFlags & coreFlags;
        const VkQueueFlags coreRequestedFlags = requestedFlags & coreFlags;

        bool isUniversal   = (coreQueueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        bool needUniversal = (coreRequestedFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

        if (isUniversal && needUniversal)
            return queueNdx;

        if ((coreQueueFlags == coreRequestedFlags) && ((queueFlags & requestedFlags) == requestedFlags))
            return queueNdx;
    }

    return NO_MATCH_FOUND;
}

} // namespace

void SparseResourcesBaseInstance::createDeviceSupportingQueues(const QueueRequirementsVec &queueRequirements,
                                                               bool requireShaderImageAtomicInt64Features /* = false */,
                                                               bool requireMaintenance5 /* = false */,
                                                               bool requireTransformFeedback /* = false */,
                                                               bool requireCopyMemoryIndirect /* = false */,
                                                               bool requireBufferDeviceAddress /* = false */)
{
    typedef std::map<VkQueueFlags, std::vector<Queue>> QueuesMap;
    typedef std::map<uint32_t, QueueFamilyQueuesCount> SelectedQueuesMap;
    typedef std::map<uint32_t, std::vector<float>> QueuePrioritiesMap;

    std::vector<VkPhysicalDeviceGroupProperties> devGroupProperties;
    std::vector<const char *> deviceExtensions;
    VkDeviceGroupDeviceCreateInfo deviceGroupInfo = {
        VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO, //stype
        nullptr,                                           //pNext
        0,                                                 //physicalDeviceCount
        nullptr                                            //physicalDevices
    };
    m_physicalDevices.push_back(m_context.getPhysicalDevice());

    // If requested, create an intance with device groups
    if (m_useDeviceGroups)
    {
        const std::vector<std::string> requiredExtensions{"VK_KHR_device_group_creation",
                                                          "VK_KHR_get_physical_device_properties2"};
        m_deviceGroupInstance = createCustomInstanceWithExtensions(m_context, requiredExtensions);
        devGroupProperties    = enumeratePhysicalDeviceGroups(m_context.getInstanceInterface(), m_deviceGroupInstance);
        m_numPhysicalDevices  = devGroupProperties[m_deviceGroupIdx].physicalDeviceCount;

        m_physicalDevices.clear();
        for (size_t physDeviceID = 0; physDeviceID < m_numPhysicalDevices; physDeviceID++)
        {
            m_physicalDevices.push_back(devGroupProperties[m_deviceGroupIdx].physicalDevices[physDeviceID]);
        }
        if (m_numPhysicalDevices < 2)
            TCU_THROW(NotSupportedError, "Sparse binding device group tests not supported with 1 physical device");

        deviceGroupInfo.physicalDeviceCount = devGroupProperties[m_deviceGroupIdx].physicalDeviceCount;
        deviceGroupInfo.pPhysicalDevices    = devGroupProperties[m_deviceGroupIdx].physicalDevices;

        if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
            deviceExtensions.push_back("VK_KHR_device_group");
    }
    else
    {
        m_context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    }

    const VkInstance &instance(m_useDeviceGroups ? m_deviceGroupInstance : m_context.getInstance());
    InstanceDriver instanceDriver(m_context.getPlatformInterface(), instance);
    const VkPhysicalDevice physicalDevice = getPhysicalDevice();
    uint32_t queueFamilyPropertiesCount   = 0u;
    instanceDriver.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, nullptr);

    if (queueFamilyPropertiesCount == 0u)
        TCU_THROW(ResourceError, "Device reports an empty set of queue family properties");

    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    queueFamilyProperties.resize(queueFamilyPropertiesCount);

    instanceDriver.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount,
                                                          &queueFamilyProperties[0]);

    if (queueFamilyPropertiesCount == 0u)
        TCU_THROW(ResourceError, "Device reports an empty set of queue family properties");

    SelectedQueuesMap selectedQueueFamilies;
    QueuePrioritiesMap queuePriorities;

    for (uint32_t queueReqNdx = 0; queueReqNdx < queueRequirements.size(); ++queueReqNdx)
    {
        const QueueRequirements &queueRequirement = queueRequirements[queueReqNdx];

        uint32_t queueFamilyIndex = 0u;
        uint32_t queuesFoundCount = 0u;

        do
        {
            queueFamilyIndex =
                m_forceSpecificQueue ?
                    findSpecificQueueFamilyIndex(queueFamilyProperties, queueRequirement.queueFlags, queueFamilyIndex) :
                    findMatchingQueueFamilyIndex(queueFamilyProperties, queueRequirement.queueFlags, queueFamilyIndex);

            if (queueFamilyIndex == NO_MATCH_FOUND)
                TCU_THROW(NotSupportedError, "No match found for queue requirements");

            const uint32_t queuesPerFamilyCount = deMin32(queueFamilyProperties[queueFamilyIndex].queueCount,
                                                          queueRequirement.queueCount - queuesFoundCount);

            selectedQueueFamilies[queueFamilyIndex].queueCount =
                deMax32(queuesPerFamilyCount, selectedQueueFamilies[queueFamilyIndex].queueCount);

            for (uint32_t queueNdx = 0; queueNdx < queuesPerFamilyCount; ++queueNdx)
            {
                Queue queue            = {nullptr, 0, 0};
                queue.queueFamilyIndex = queueFamilyIndex;
                queue.queueIndex       = queueNdx;

                m_queues[queueRequirement.queueFlags].push_back(queue);
            }

            queuesFoundCount += queuesPerFamilyCount;

            ++queueFamilyIndex;
        } while (queuesFoundCount < queueRequirement.queueCount);
    }

    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    for (SelectedQueuesMap::iterator queueFamilyIter = selectedQueueFamilies.begin();
         queueFamilyIter != selectedQueueFamilies.end(); ++queueFamilyIter)
    {
        for (uint32_t queueNdx = 0; queueNdx < queueFamilyIter->second.queueCount; ++queueNdx)
            queuePriorities[queueFamilyIter->first].push_back(1.0f);

        const VkDeviceQueueCreateInfo queueInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,  // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            (VkDeviceQueueCreateFlags)0u,                // VkDeviceQueueCreateFlags flags;
            queueFamilyIter->first,                      // uint32_t queueFamilyIndex;
            queueFamilyIter->second.queueCount,          // uint32_t queueCount;
            &queuePriorities[queueFamilyIter->first][0], // const float* pQueuePriorities;
        };

        queueInfos.push_back(queueInfo);
    }

    auto shaderImageAtomicInt64Features  = m_context.getShaderImageAtomicInt64FeaturesEXT();
    auto maintenance5Features            = m_context.getMaintenance5Features();
    auto transformFeedbackFeatures       = m_context.getTransformFeedbackFeaturesEXT();
    shaderImageAtomicInt64Features.pNext = nullptr;
    maintenance5Features.pNext           = nullptr;
    transformFeedbackFeatures.pNext      = nullptr;

    const VkPhysicalDeviceFeatures deviceFeatures = getPhysicalDeviceFeatures(instanceDriver, physicalDevice);
    vk::VkPhysicalDeviceFeatures2 deviceFeatures2 = getPhysicalDeviceFeatures2(instanceDriver, physicalDevice);

    const bool useFeatures2 =
        (requireShaderImageAtomicInt64Features || requireMaintenance5 || requireTransformFeedback);

    void *pNext = nullptr;

    if (useFeatures2)
    {
        pNext = &deviceFeatures2;

        if (m_useDeviceGroups)
        {
            deviceGroupInfo.pNext = deviceFeatures2.pNext;
            deviceFeatures2.pNext = &deviceGroupInfo;
        }

        if (requireShaderImageAtomicInt64Features)
        {
            shaderImageAtomicInt64Features.pNext = deviceFeatures2.pNext;
            deviceFeatures2.pNext                = &shaderImageAtomicInt64Features;

            deviceExtensions.push_back("VK_EXT_shader_image_atomic_int64");
        }

        if (requireMaintenance5)
        {
            maintenance5Features.pNext = deviceFeatures2.pNext;
            deviceFeatures2.pNext      = &maintenance5Features;

            deviceExtensions.push_back("VK_KHR_maintenance5");
        }

        if (requireTransformFeedback)
        {
            transformFeedbackFeatures.pNext = deviceFeatures2.pNext;
            deviceFeatures2.pNext           = &transformFeedbackFeatures;

            deviceExtensions.push_back("VK_EXT_transform_feedback");
        }

        if (requireCopyMemoryIndirect)
        {
            deviceExtensions.push_back("VK_KHR_copy_memory_indirect");
        }

        if (requireBufferDeviceAddress)
        {
            deviceExtensions.push_back("VK_KHR_buffer_device_address");
        }
    }
    else if (m_useDeviceGroups)
    {
        pNext = &deviceGroupInfo;
    }

    // Add these extensions regardless of whether we're using features2
    if (requireCopyMemoryIndirect)
    {
        deviceExtensions.push_back("VK_KHR_copy_memory_indirect");
    }

    if (requireBufferDeviceAddress)
    {
        deviceExtensions.push_back("VK_KHR_buffer_device_address");
    }

    const VkDeviceCreateInfo deviceInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                     // VkStructureType sType;
        pNext,                                                    // const void* pNext;
        (VkDeviceCreateFlags)0,                                   // VkDeviceCreateFlags flags;
        static_cast<uint32_t>(queueInfos.size()),                 // uint32_t queueCreateInfoCount;
        &queueInfos[0],                                           // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                                       // uint32_t enabledLayerCount;
        nullptr,                                                  // const char* const* ppEnabledLayerNames;
        uint32_t(deviceExtensions.size()),                        // uint32_t enabledExtensionCount;
        deviceExtensions.size() ? &deviceExtensions[0] : nullptr, // const char* const* ppEnabledExtensionNames;
        useFeatures2 ? nullptr : &deviceFeatures,                 // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    m_logicalDevice =
        createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(),
                           m_context.getPlatformInterface(), instance, instanceDriver, physicalDevice, &deviceInfo);
    m_deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), instance,
                                                                *m_logicalDevice, m_context.getUsedApiVersion(),
                                                                m_context.getTestContext().getCommandLine()));
    m_allocator    = de::MovePtr<Allocator>(new SimpleAllocator(
        *m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instanceDriver, physicalDevice)));

    for (QueuesMap::iterator queuesIter = m_queues.begin(); queuesIter != m_queues.end(); ++queuesIter)
    {
        for (uint32_t queueNdx = 0u; queueNdx < queuesIter->second.size(); ++queueNdx)
        {
            Queue &queue = queuesIter->second[queueNdx];

            queue.queueHandle =
                getDeviceQueue(*m_deviceDriver, *m_logicalDevice, queue.queueFamilyIndex, queue.queueIndex);
        }
    }
}

const Queue &SparseResourcesBaseInstance::getQueue(const VkQueueFlags queueFlags, const uint32_t queueIndex) const
{
    return m_queues.find(queueFlags)->second[queueIndex];
}

} // namespace sparse
} // namespace vkt
