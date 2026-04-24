#ifndef _VKDATAGRAPHSESSIONWITHMEMORY_HPP
#define _VKDATAGRAPHSESSIONWITHMEMORY_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2024-2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Data graph session backed with memory
 */
/*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC
class DataGraphSessionWithMemory
{
public:
    DataGraphSessionWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                               const vk::VkDataGraphPipelineSessionCreateInfoARM &sessionCreateInfo,
                               const vk::MemoryRequirement memoryRequirement = vk::MemoryRequirement::HostVisible,
                               bool testRequiresTransient                    = false)
        : m_session(createDataGraphPipelineSessionARM(vk, device, &sessionCreateInfo, nullptr))
    {
        m_allocations = bindDataGraphSession(vk, device, allocator, *m_session,
                                             getDataGraphPipelineSessionBindPointRequirements(vk, device, *m_session),
                                             memoryRequirement, &m_allocatedSize, testRequiresTransient);
    }

    const vk::VkDataGraphPipelineSessionARM &get(void) const
    {
        return *m_session;
    }
    const vk::VkDataGraphPipelineSessionARM &operator*(void) const
    {
        return get();
    }

    vk::VkDeviceSize getTotalAllocatedSize() const
    {
        return m_allocatedSize;
    }

private:
    vk::VkDeviceSize m_allocatedSize;
    const vk::Unique<vk::VkDataGraphPipelineSessionARM> m_session;
    std::vector<de::MovePtr<Allocation>> m_allocations;

    // "deleted"
    DataGraphSessionWithMemory(const DataGraphSessionWithMemory &);
    DataGraphSessionWithMemory operator=(const DataGraphSessionWithMemory &);
};

#endif // CTS_USES_VULKANSC

} // namespace vk

#endif // _VKDATAGRAPHSESSIONWITHMEMORY_HPP
