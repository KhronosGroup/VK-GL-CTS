#ifndef _VKTGLOBALPRIORITYQUEUEUTILS_HPP
#define _VKTGLOBALPRIORITYQUEUEUTILS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \file  vktGlobalPriorityQueueUtils.hpp
 * \brief Global Priority Queue Utils
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "deUniquePtr.hpp"
#include "../image/vktImageTestsUtil.hpp"

#include <set>
#include <vector>

namespace vkt
{
// forward declaration
class Context;
namespace synchronization
{

constexpr uint32_t INVALID_UINT32 = (~(static_cast<uint32_t>(0u)));

uint32_t findQueueFamilyIndex(const vk::InstanceInterface &vki, vk::VkPhysicalDevice dev,
                              vk::VkQueueGlobalPriority priority, vk::VkQueueFlags includeFlags,
                              vk::VkQueueFlags excludeFlags, uint32_t excludeIndex = INVALID_UINT32);

struct SpecialDevice
{
    SpecialDevice(Context &ctx, vk::VkQueueFlagBits transitionFrom, vk::VkQueueFlagBits transitionTo,
                  vk::VkQueueGlobalPriority priorityFrom, vk::VkQueueGlobalPriority priorityTo, bool enableProtected,
                  bool enableSparseBinding);
    SpecialDevice(const SpecialDevice &)            = delete;
    SpecialDevice &operator=(const SpecialDevice &) = delete;
    static vk::VkQueueFlags getColissionFlags(vk::VkQueueFlags flags);
    virtual ~SpecialDevice();

public:
    const uint32_t &queueFamilyIndexFrom;
    const uint32_t &queueFamilyIndexTo;
    const vk::VkDevice &handle;
    const vk::VkQueue &queueFrom;
    const vk::VkQueue &queueTo;
    const vk::VkResult &createResult;
    const char *const &createExpression;
    const char *const &createFileName;
    const int32_t &createFileLine;
    vk::Allocator &getAllocator() const
    {
        return *m_allocator;
    }

protected:
    Context &m_context;
    vk::VkQueueFlagBits m_transitionFrom;
    vk::VkQueueFlagBits m_transitionTo;
    uint32_t m_queueFamilyIndexFrom;
    uint32_t m_queueFamilyIndexTo;
    vk::VkDevice m_deviceHandle;
    vk::VkQueue m_queueFrom;
    vk::VkQueue m_queueTo;
    de::MovePtr<vk::Allocator> m_allocator;
    vk::VkResult m_createResult;
    const char *m_createExpression;
    const char *m_createFileName;
    int32_t m_createFileLine;
};

class BufferWithMemory
{
public:
    BufferWithMemory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd, const vk::VkPhysicalDevice phys,
                     const vk::VkDevice device, vk::Allocator &allocator,
                     const vk::VkBufferCreateInfo &bufferCreateInfo, const vk::MemoryRequirement memoryRequirement,
                     const vk::VkQueue sparseQueue = VK_NULL_HANDLE);

    const vk::VkBuffer &get(void) const
    {
        return *m_buffer;
    }
    const vk::VkBuffer *getPtr(void) const
    {
        return &(*m_buffer);
    }
    const vk::VkBuffer &operator*(void) const
    {
        return get();
    }
    void *getHostPtr(void) const;
    vk::VkDeviceSize getSize() const
    {
        return m_size;
    }
    void invalidateAlloc(const vk::DeviceInterface &vk, const vk::VkDevice device) const;
    void flushAlloc(const vk::DeviceInterface &vk, const vk::VkDevice device) const;

protected:
    void assertIAmSparse() const;

    const bool m_amISparse;
    const vk::Unique<vk::VkBuffer> m_buffer;
    const vk::VkMemoryRequirements m_requirements;
    std::vector<de::SharedPtr<vk::Allocation>> m_allocations;
    const vk::VkDeviceSize m_size;

    BufferWithMemory(const BufferWithMemory &);
    BufferWithMemory operator=(const BufferWithMemory &);
};

class ImageWithMemory : public image::Image
{
public:
    ImageWithMemory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd, const vk::VkPhysicalDevice phys,
                    const vk::VkDevice device, vk::Allocator &allocator, const vk::VkImageCreateInfo &imageCreateInfo,
                    const vk::VkQueue sparseQueue                 = VK_NULL_HANDLE,
                    const vk::MemoryRequirement memoryRequirement = vk::MemoryRequirement::Any);

    const vk::VkImage &get(void) const
    {
        return m_image->get();
    }
    const vk::VkImage &operator*(void) const
    {
        return m_image->get();
    }

protected:
    de::MovePtr<image::Image> m_image;

private:
    ImageWithMemory &operator=(const ImageWithMemory &);
};

} // namespace synchronization
} // namespace vkt

#endif // _VKTGLOBALPRIORITYQUEUEUTILS_HPP
