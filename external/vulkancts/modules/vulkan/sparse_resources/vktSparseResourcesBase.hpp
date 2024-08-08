#ifndef _VKTSPARSERESOURCESBASE_HPP
#define _VKTSPARSERESOURCESBASE_HPP
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
 * \file  vktSparseResourcesBase.hpp
 * \brief Sparse Resources Base Instance
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkRef.hpp"
#include "vkPlatform.hpp"
#include "deUniquePtr.hpp"
#include "tcuCommandLine.hpp"

#include <map>
#include <vector>

namespace vkt
{
namespace sparse
{

struct Queue
{
    vk::VkQueue queueHandle;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
};

struct QueueRequirements
{
    QueueRequirements(const vk::VkQueueFlags qFlags, const uint32_t qCount) : queueFlags(qFlags), queueCount(qCount)
    {
    }

    vk::VkQueueFlags queueFlags;
    uint32_t queueCount;
};

class SparseResourcesBaseInstance : public TestInstance
{
public:
    SparseResourcesBaseInstance(Context &context, bool useDeviceGroups = false)
        : TestInstance(context)
        , m_numPhysicalDevices(1)
        , m_useDeviceGroups(useDeviceGroups)
    {
        const tcu::CommandLine &cmdLine = context.getTestContext().getCommandLine();
        m_deviceGroupIdx                = cmdLine.getVKDeviceGroupId() - 1;
    }
    bool usingDeviceGroups()
    {
        return m_useDeviceGroups;
    }

protected:
    typedef std::vector<QueueRequirements> QueueRequirementsVec;

    uint32_t m_numPhysicalDevices;

    void createDeviceSupportingQueues(const QueueRequirementsVec &queueRequirements,
                                      bool requireShaderImageAtomicInt64Features = false,
                                      bool requireMaintenance5                   = false);
    const Queue &getQueue(const vk::VkQueueFlags queueFlags, const uint32_t queueIndex) const;
    const vk::DeviceInterface &getDeviceInterface(void) const
    {
        return *m_deviceDriver;
    }
    vk::VkDevice getDevice(void) const
    {
        return *m_logicalDevice;
    }
    vk::Allocator &getAllocator(void)
    {
        return *m_allocator;
    }
    vk::VkPhysicalDevice getPhysicalDevice(uint32_t i = 0)
    {
        return m_physicalDevices[i];
    }

private:
    bool m_useDeviceGroups;
    uint32_t m_deviceGroupIdx;
    CustomInstance m_deviceGroupInstance;
    std::vector<vk::VkPhysicalDevice> m_physicalDevices;
    std::map<vk::VkQueueFlags, std::vector<Queue>> m_queues;
    de::MovePtr<vk::DeviceDriver> m_deviceDriver;
    vk::Move<vk::VkDevice> m_logicalDevice;
    de::MovePtr<vk::Allocator> m_allocator;
};

} // namespace sparse
} // namespace vkt

#endif // _VKTSPARSERESOURCESBASE_HPP
