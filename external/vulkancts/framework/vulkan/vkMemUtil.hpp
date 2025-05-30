#ifndef _VKMEMUTIL_HPP
#define _VKMEMUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Memory management utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuMaybe.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include <vector>

namespace vk
{

/*--------------------------------------------------------------------*//*!
 * \brief Memory allocation interface
 *
 * Allocation represents block of device memory and is allocated by
 * Allocator implementation. Test code should use Allocator for allocating
 * memory, unless there is a reason not to (for example testing vkAllocMemory).
 *
 * Allocation doesn't necessarily correspond to a whole VkDeviceMemory, but
 * instead it may represent sub-allocation. Thus whenever VkDeviceMemory
 * (getMemory()) managed by Allocation is passed to Vulkan API calls,
 * offset given by getOffset() must be used.
 *
 * If host-visible memory was requested, host pointer to the memory can
 * be queried with getHostPtr(). No offset is needed when accessing host
 * pointer, i.e. the pointer is already adjusted in case of sub-allocation.
 *
 * Memory mappings are managed solely by Allocation, i.e. unmapping or
 * re-mapping VkDeviceMemory owned by Allocation is not allowed.
 *//*--------------------------------------------------------------------*/
class Allocation
{
public:
    virtual ~Allocation(void);

    //! Get VkDeviceMemory backing this allocation
    VkDeviceMemory getMemory(void) const
    {
        return m_memory;
    }

    //! Get offset in VkDeviceMemory for this allocation
    VkDeviceSize getOffset(void) const
    {
        return m_offset;
    }

    //! Get host pointer for this allocation. Only available for host-visible allocations
    void *getHostPtr(void) const
    {
        DE_ASSERT(m_hostPtr);
        return m_hostPtr;
    }

protected:
    Allocation(VkDeviceMemory memory, VkDeviceSize offset, void *hostPtr);

private:
    const VkDeviceMemory m_memory;
    const VkDeviceSize m_offset;
    void *const m_hostPtr;
};

void flushAlloc(const DeviceInterface &vkd, VkDevice device, const Allocation &alloc);
void invalidateAlloc(const DeviceInterface &vkd, VkDevice device, const Allocation &alloc);

//! Memory allocation requirements
class MemoryRequirement
{
public:
    static const MemoryRequirement Any;
    static const MemoryRequirement HostVisible;
    static const MemoryRequirement Coherent;
    static const MemoryRequirement LazilyAllocated;
    static const MemoryRequirement Protected;
    static const MemoryRequirement Local;
    static const MemoryRequirement Cached;
    static const MemoryRequirement NonLocal;
    static const MemoryRequirement DeviceAddress;
    static const MemoryRequirement DeviceAddressCaptureReplay;
#ifndef CTS_USES_VULKANSC
    static const MemoryRequirement ZeroInitialize;
#endif // CTS_USES_VULKANSC

    inline MemoryRequirement operator|(MemoryRequirement requirement) const
    {
        return MemoryRequirement(m_flags | requirement.m_flags);
    }

    inline MemoryRequirement operator&(MemoryRequirement requirement) const
    {
        return MemoryRequirement(m_flags & requirement.m_flags);
    }

    bool matchesHeap(VkMemoryPropertyFlags heapFlags) const;

    inline operator bool(void) const
    {
        return m_flags != 0u;
    }

private:
    explicit MemoryRequirement(uint32_t flags);

    const uint32_t m_flags;

    enum Flags
    {
        FLAG_HOST_VISIBLE                  = 1u << 0u,
        FLAG_COHERENT                      = 1u << 1u,
        FLAG_LAZY_ALLOCATION               = 1u << 2u,
        FLAG_PROTECTED                     = 1u << 3u,
        FLAG_LOCAL                         = 1u << 4u,
        FLAG_CACHED                        = 1u << 5u,
        FLAG_NON_LOCAL                     = 1u << 6u,
        FLAG_DEVICE_ADDRESS                = 1u << 7u,
        FLAG_DEVICE_ADDRESS_CAPTURE_REPLAY = 1u << 8u,
        FLAG_ZERO_INITIALIZE               = 1u << 9u,
    };
};

enum class HostIntent
{
    NONE = 0,
    R    = 1, // Reading data from the host.
    W    = 2, // Writing data from the host.
    RW   = 3, // Reading and writing from the host.
};

//! Memory allocator interface
class Allocator
{
public:
    Allocator(void)
    {
    }
    virtual ~Allocator(void)
    {
    }

    virtual de::MovePtr<Allocation> allocate(const VkMemoryAllocateInfo &allocInfo, VkDeviceSize alignment) = 0;
    virtual de::MovePtr<Allocation> allocate(const VkMemoryRequirements &memRequirements, MemoryRequirement requirement,
                                             uint64_t memoryOpaqueCaptureAddr = 0u)                         = 0;
    virtual de::MovePtr<Allocation> allocate(const VkMemoryRequirements &memReqs, HostIntent intent,
                                             VkMemoryAllocateFlags allocFlags = 0u)                         = 0;
};

//! Allocator that backs every allocation with its own VkDeviceMemory
class SimpleAllocator : public Allocator
{
public:
    struct OffsetParams
    {
        const vk::VkDeviceSize nonCoherentAtomSize;
        const vk::VkDeviceSize offset;
    };
    typedef tcu::Maybe<OffsetParams> OptionalOffsetParams;

    SimpleAllocator(const DeviceInterface &vk, VkDevice device, const VkPhysicalDeviceMemoryProperties &deviceMemProps,
                    const OptionalOffsetParams &offsetParams = tcu::Nothing);

    de::MovePtr<Allocation> allocate(const VkMemoryAllocateInfo &allocInfo, VkDeviceSize alignment) override;
    de::MovePtr<Allocation> allocate(const VkMemoryRequirements &memRequirements, MemoryRequirement requirement,
                                     uint64_t memoryOpaqueCaptureAddr = 0u) override;
    de::MovePtr<Allocation> allocate(const VkMemoryRequirements &memReqs, HostIntent intent,
                                     VkMemoryAllocateFlags allocFlags = 0u) override;

private:
    de::MovePtr<Allocation> allocate(const VkMemoryRequirements &memRequirements, MemoryRequirement requirement,
                                     const tcu::Maybe<HostIntent> &hostIntent, uint64_t memoryOpaqueCaptureAddr = 0u);

    const DeviceInterface &m_vk;
    const VkDevice m_device;
    const VkPhysicalDeviceMemoryProperties m_memProps;
    const tcu::Maybe<OffsetParams> m_offsetParams;
};

de::MovePtr<Allocation> allocateExtended(const InstanceInterface &vki, const DeviceInterface &vkd,
                                         const VkPhysicalDevice &physDevice, const VkDevice device,
                                         const VkMemoryRequirements &memReqs, const MemoryRequirement requirement,
                                         const void *pNext);
de::MovePtr<Allocation> allocateDedicated(const InstanceInterface &vki, const DeviceInterface &vkd,
                                          const VkPhysicalDevice &physDevice, const VkDevice device,
                                          const VkBuffer buffer, MemoryRequirement requirement);
de::MovePtr<Allocation> allocateDedicated(const InstanceInterface &vki, const DeviceInterface &vkd,
                                          const VkPhysicalDevice &physDevice, const VkDevice device,
                                          const VkImage image, MemoryRequirement requirement);

void *mapMemory(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size,
                VkMemoryMapFlags flags);
void flushMappedMemoryRange(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                            VkDeviceSize size);
void invalidateMappedMemoryRange(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory memory,
                                 VkDeviceSize offset, VkDeviceSize size);

uint32_t selectMatchingMemoryType(const VkPhysicalDeviceMemoryProperties &deviceMemProps, uint32_t allowedMemTypeBits,
                                  MemoryRequirement requirement);
uint32_t selectBestMemoryType(const VkPhysicalDeviceMemoryProperties &deviceMemProps, uint32_t allowedMemTypeBits,
                              MemoryRequirement requirement, const tcu::Maybe<HostIntent> &hostIntent);
uint32_t getCompatibleMemoryTypes(const VkPhysicalDeviceMemoryProperties &deviceMemProps,
                                  MemoryRequirement requirement);
#ifdef CTS_USES_VULKANSC
uint32_t getSEUSafeMemoryTypes(const VkPhysicalDeviceMemoryProperties &deviceMemProps);
#endif // CTS_USES_VULKANSC

void bindImagePlanesMemory(const vk::DeviceInterface &vkd, const vk::VkDevice device, const vk::VkImage image,
                           const uint32_t numPlanes, std::vector<de::SharedPtr<Allocation>> &allocations,
                           vk::Allocator &allocator, const vk::MemoryRequirement requirement);

de::MovePtr<Allocation> bindImage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                  const VkImage image, const MemoryRequirement requirement);

de::MovePtr<Allocation> bindImage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                  const VkImage image, const HostIntent hostIntent,
                                  const VkMemoryAllocateFlags memAllocFlags = 0u);

de::MovePtr<Allocation> bindBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                   const VkBuffer buffer, const MemoryRequirement requirement);

de::MovePtr<Allocation> bindBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                   const VkBuffer buffer, const HostIntent hostIntent,
                                   const VkMemoryAllocateFlags memAllocFlags = 0u);

void zeroBuffer(const DeviceInterface &vk, const VkDevice device, const Allocation &alloc, const VkDeviceSize size);

} // namespace vk

#endif // _VKMEMUTIL_HPP
