#ifndef _VKTENSORWITHMEMORY_HPP
#define _VKTENSORWITHMEMORY_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 * \brief Tensor backed with memory
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC
class TensorWithMemory
{
public:
    TensorWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                     const vk::VkTensorCreateInfoARM &tensorCreateInfo, const vk::MemoryRequirement memoryRequirement)
        : m_tensor(createTensorARM(vk, device, &tensorCreateInfo))
        , m_allocation(bindTensor(vk, device, allocator, *m_tensor, memoryRequirement, &m_allocationSize))
        , m_externalMemoryHandleTypes(0)
    {
        const VkExternalMemoryTensorCreateInfoARM *const externalInfo =
            findStructure<VkExternalMemoryTensorCreateInfoARM>(tensorCreateInfo.pNext);
        if (externalInfo != nullptr)
        {
            m_externalMemoryHandleTypes = externalInfo->handleTypes;
        }
    }

    const vk::VkTensorARM &get(void) const
    {
        return *m_tensor;
    }
    const vk::VkTensorARM &operator*(void) const
    {
        return get();
    }
    vk::Allocation &getAllocation(void) const
    {
        return *m_allocation;
    }
    VkDeviceSize getAllocationSize(void) const
    {
        return m_allocationSize;
    }
    vk::VkExternalMemoryHandleTypeFlags getExternalMemoryHandleTypes(void) const
    {
        return m_externalMemoryHandleTypes;
    }

private:
    const vk::Unique<vk::VkTensorARM> m_tensor;
    const de::UniquePtr<vk::Allocation> m_allocation;
    vk::VkDeviceSize m_allocationSize;
    vk::VkExternalMemoryHandleTypeFlags m_externalMemoryHandleTypes;

    // "deleted"
    TensorWithMemory(const TensorWithMemory &);
    TensorWithMemory operator=(const TensorWithMemory &);
};

#endif // CTS_USES_VULKANSC

} // namespace vk

#endif // _VKTENSORWITHMEMORY_HPP
