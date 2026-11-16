#ifndef _VKBUFFERWITHMEMORY_HPP
#define _VKBUFFERWITHMEMORY_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Google Inc.
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
 * \brief Buffer backed with memory
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"

#include "deSTLUtil.hpp"

#include <memory>
#include <vector>
#include <cstring>
#include <string>

namespace vk
{
class BufferWithMemory
{
public:
    BufferWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                     const vk::VkBufferCreateInfo &bufferCreateInfo, const vk::MemoryRequirement memoryRequirement,
                     const bool bindOnCreation = true, uint64_t memoryOpaqueCaptureAddr = 0u)

        : m_vk(vk)
        , m_device(device)
        , m_buffer(createBuffer(vk, device, &bufferCreateInfo))
        , m_createSize(bufferCreateInfo.size)
        , m_allocation(allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement,
                                          memoryOpaqueCaptureAddr))
        , m_memoryBound(false)
    {
        if (bindOnCreation)
            bindMemory();
    }

    BufferWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                     const vk::VkBufferCreateInfo &bufferCreateInfo, vk::HostIntent hostIntent,
                     const bool bindOnCreation = true, VkMemoryAllocateFlags memAllocFlags = 0u)

        : m_vk(vk)
        , m_device(device)
        , m_buffer(createBuffer(vk, device, &bufferCreateInfo))
        , m_createSize(bufferCreateInfo.size)
        , m_allocation(
              allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), hostIntent, memAllocFlags))
        , m_memoryBound(false)
    {
        if (bindOnCreation)
            bindMemory();
    }

    const vk::VkBuffer &get(void) const
    {
        return *m_buffer;
    }
    const vk::VkBuffer &operator*(void) const
    {
        return get();
    }
    vk::Allocation &getAllocation(void) const
    {
        return *m_allocation;
    }
    void bindMemory(void)
    {
        if (!m_memoryBound)
        {
            VK_CHECK(m_vk.bindBufferMemory(m_device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
            m_memoryBound = true;
        }
    }
    vk::VkDeviceSize getBufferSize(void) const
    {
        return m_createSize;
    }

    // Copies dataSize bytes from data to the buffer memory at bufferOffset, flushing the allocation if requested.
    void writeData(const void *data, size_t dataSize, VkDeviceSize bufferOffset = 0, bool flush = true)
    {
        checkHostAccess("writeData", dataSize, bufferOffset);

        const auto dst = static_cast<uint8_t *>(m_allocation->getHostPtr()) + static_cast<size_t>(bufferOffset);
        std::memcpy(dst, data, dataSize);

        if (flush)
            flushAlloc(m_vk, m_device, *m_allocation);
    }

    // Copies dataSize bytes from the buffer memory at bufferOffset to data, invalidating the allocation first if
    // requested.
    void readData(void *data, size_t dataSize, VkDeviceSize bufferOffset = 0, bool invalidate = true) const
    {
        checkHostAccess("readData", dataSize, bufferOffset);

        if (invalidate)
            invalidateAlloc(m_vk, m_device, *m_allocation);

        const auto src = static_cast<const uint8_t *>(m_allocation->getHostPtr()) + static_cast<size_t>(bufferOffset);
        std::memcpy(data, src, dataSize);
    }

private:
    const vk::DeviceInterface &m_vk;
    const vk::VkDevice m_device;
    const vk::Unique<vk::VkBuffer> m_buffer;
    const vk::VkDeviceSize m_createSize;
    const de::UniquePtr<vk::Allocation> m_allocation;
    bool m_memoryBound;

    // Throws if the buffer memory cannot be accessed from the host in the given range. Written so the range check
    // cannot overflow.
    void checkHostAccess(const char *funcName, size_t dataSize, VkDeviceSize bufferOffset) const
    {
        const std::string prefix = std::string("BufferWithMemory::") + funcName + "(): ";

        if (!m_memoryBound)
            TCU_THROW(InternalError, prefix + "memory not bound");

        if (!m_allocation->isHostVisible())
            TCU_THROW(InternalError, prefix + "memory not host-visible");

        const auto vkSize = static_cast<VkDeviceSize>(dataSize);
        if (bufferOffset > m_createSize || vkSize > m_createSize - bufferOffset)
            TCU_THROW(InternalError, prefix + "offset and size too large for buffer");
    }

    // "deleted"
    BufferWithMemory(const BufferWithMemory &);
    BufferWithMemory operator=(const BufferWithMemory &);
};

// Convenience function to fill a buffer with the contents of a vector.
template <class T>
std::unique_ptr<BufferWithMemory> makeBufferWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device,
                                                       vk::Allocator &allocator, const std::vector<T> &bufferData,
                                                       VkBufferUsageFlags usage)
{
    const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(bufferData));
    const auto bufferInfo = makeBufferCreateInfo(bufferSize, usage);
    std::unique_ptr<BufferWithMemory> bufferPtr(
        new BufferWithMemory(vk, device, allocator, bufferInfo, MemoryRequirement::HostVisible));
    {
        auto &bufferAlloc = bufferPtr->getAllocation();
        memcpy(bufferAlloc.getHostPtr(), de::dataOrNull(bufferData), de::dataSize(bufferData));
        flushAlloc(vk, device, bufferAlloc);
    }

    return bufferPtr;
}

} // namespace vk

#endif // _VKBUFFERWITHMEMORY_HPP
