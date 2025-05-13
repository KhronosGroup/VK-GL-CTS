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

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"

#include "deDefs.h"
#include "deInt32.h"

#include <vector>
#include <algorithm>
#include <memory>

namespace vk
{

using de::MovePtr;
using de::UniquePtr;
using std::vector;

typedef de::SharedPtr<Allocation> AllocationSp;

namespace
{

class HostPtr
{
public:
    HostPtr(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size,
            VkMemoryMapFlags flags);
    ~HostPtr(void);

    void *get(void) const
    {
        return m_ptr;
    }

private:
    const DeviceInterface &m_vkd;
    const VkDevice m_device;
    const VkDeviceMemory m_memory;
    void *const m_ptr;
};

HostPtr::HostPtr(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                 VkDeviceSize size, VkMemoryMapFlags flags)
    : m_vkd(vkd)
    , m_device(device)
    , m_memory(memory)
    , m_ptr(mapMemory(vkd, device, memory, offset, size, flags))
{
}

HostPtr::~HostPtr(void)
{
    m_vkd.unmapMemory(m_device, m_memory);
}

bool isHostVisibleMemory(const VkPhysicalDeviceMemoryProperties &deviceMemProps, uint32_t memoryTypeNdx)
{
    DE_ASSERT(memoryTypeNdx < deviceMemProps.memoryTypeCount);
    return (deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
}

} // namespace

// Allocation

Allocation::Allocation(VkDeviceMemory memory, VkDeviceSize offset, void *hostPtr)
    : m_memory(memory)
    , m_offset(offset)
    , m_hostPtr(hostPtr)
{
}

Allocation::~Allocation(void)
{
}

void flushAlloc(const DeviceInterface &vkd, VkDevice device, const Allocation &alloc)
{
    flushMappedMemoryRange(vkd, device, alloc.getMemory(), alloc.getOffset(), VK_WHOLE_SIZE);
}

void invalidateAlloc(const DeviceInterface &vkd, VkDevice device, const Allocation &alloc)
{
    invalidateMappedMemoryRange(vkd, device, alloc.getMemory(), alloc.getOffset(), VK_WHOLE_SIZE);
}

// MemoryRequirement

const MemoryRequirement MemoryRequirement::Any             = MemoryRequirement(0x0u);
const MemoryRequirement MemoryRequirement::HostVisible     = MemoryRequirement(MemoryRequirement::FLAG_HOST_VISIBLE);
const MemoryRequirement MemoryRequirement::Coherent        = MemoryRequirement(MemoryRequirement::FLAG_COHERENT);
const MemoryRequirement MemoryRequirement::LazilyAllocated = MemoryRequirement(MemoryRequirement::FLAG_LAZY_ALLOCATION);
const MemoryRequirement MemoryRequirement::Protected       = MemoryRequirement(MemoryRequirement::FLAG_PROTECTED);
const MemoryRequirement MemoryRequirement::Local           = MemoryRequirement(MemoryRequirement::FLAG_LOCAL);
const MemoryRequirement MemoryRequirement::Cached          = MemoryRequirement(MemoryRequirement::FLAG_CACHED);
const MemoryRequirement MemoryRequirement::NonLocal        = MemoryRequirement(MemoryRequirement::FLAG_NON_LOCAL);
const MemoryRequirement MemoryRequirement::DeviceAddress   = MemoryRequirement(MemoryRequirement::FLAG_DEVICE_ADDRESS);
const MemoryRequirement MemoryRequirement::DeviceAddressCaptureReplay =
    MemoryRequirement(MemoryRequirement::FLAG_DEVICE_ADDRESS_CAPTURE_REPLAY);

bool MemoryRequirement::matchesHeap(VkMemoryPropertyFlags heapFlags) const
{
    // Quick check
    if ((m_flags & FLAG_COHERENT) && !(m_flags & FLAG_HOST_VISIBLE))
        DE_FATAL("Coherent memory must be host-visible");
    if ((m_flags & FLAG_HOST_VISIBLE) && (m_flags & FLAG_LAZY_ALLOCATION))
        DE_FATAL("Lazily allocated memory cannot be mappable");
    if ((m_flags & FLAG_PROTECTED) && (m_flags & FLAG_HOST_VISIBLE))
        DE_FATAL("Protected memory cannot be mappable");

    // host-visible
    if ((m_flags & FLAG_HOST_VISIBLE) && !(heapFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
        return false;

    // coherent
    if ((m_flags & FLAG_COHERENT) && !(heapFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        return false;

    // lazy
    if ((m_flags & FLAG_LAZY_ALLOCATION) && !(heapFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT))
        return false;

    // protected
    if ((m_flags & FLAG_PROTECTED) && !(heapFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT))
        return false;

    // local
    if ((m_flags & FLAG_LOCAL) && !(heapFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        return false;

    // cached
    if ((m_flags & FLAG_CACHED) && !(heapFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
        return false;

    // non-local
    if ((m_flags & FLAG_NON_LOCAL) && (heapFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        return false;

    return true;
}

MemoryRequirement::MemoryRequirement(uint32_t flags) : m_flags(flags)
{
}

// SimpleAllocator

class SimpleAllocation : public Allocation
{
public:
    SimpleAllocation(Move<VkDeviceMemory> mem, MovePtr<HostPtr> hostPtr, size_t offset);
    virtual ~SimpleAllocation(void);

private:
    const Unique<VkDeviceMemory> m_memHolder;
    const UniquePtr<HostPtr> m_hostPtr;
};

SimpleAllocation::SimpleAllocation(Move<VkDeviceMemory> mem, MovePtr<HostPtr> hostPtr, size_t offset)
    : Allocation(*mem, offset, hostPtr ? hostPtr->get() : nullptr)
    , m_memHolder(mem)
    , m_hostPtr(hostPtr)
{
}

SimpleAllocation::~SimpleAllocation(void)
{
}

SimpleAllocator::SimpleAllocator(const DeviceInterface &vk, VkDevice device,
                                 const VkPhysicalDeviceMemoryProperties &deviceMemProps,
                                 const OptionalOffsetParams &offsetParams)
    : m_vk(vk)
    , m_device(device)
    , m_memProps(deviceMemProps)
    , m_offsetParams(offsetParams)
{
    if (m_offsetParams)
    {
        const auto zero = VkDeviceSize{0};
        DE_UNREF(zero); // For release builds.
        // If an offset is provided, a non-coherent atom size must be provided too.
        DE_ASSERT(m_offsetParams->offset == zero || m_offsetParams->nonCoherentAtomSize != zero);
    }
}

MovePtr<Allocation> SimpleAllocator::allocate(const VkMemoryAllocateInfo &allocInfo, VkDeviceSize alignment)
{
    // Align the offset to the requirements.
    // Aligning to the non coherent atom size prevents flush and memory invalidation valid usage errors.
    const auto requiredAlignment =
        (m_offsetParams ? de::lcm(m_offsetParams->nonCoherentAtomSize, alignment) : alignment);
    const auto offset = (m_offsetParams ? de::roundUp(m_offsetParams->offset, requiredAlignment) : 0);

    VkMemoryAllocateInfo info = allocInfo;
    info.allocationSize += offset;

    Move<VkDeviceMemory> mem = allocateMemory(m_vk, m_device, &info);
    MovePtr<HostPtr> hostPtr;

    if (isHostVisibleMemory(m_memProps, info.memoryTypeIndex))
        hostPtr = MovePtr<HostPtr>(new HostPtr(m_vk, m_device, *mem, offset, info.allocationSize, 0u));

    return MovePtr<Allocation>(new SimpleAllocation(mem, hostPtr, static_cast<size_t>(offset)));
}

MovePtr<Allocation> SimpleAllocator::allocate(const VkMemoryRequirements &memReqs, MemoryRequirement requirement,
                                              const tcu::Maybe<HostIntent> &hostIntent,
                                              uint64_t memoryOpaqueCaptureAddr)
{
#ifdef CTS_USES_VULKANSC
    const auto memoryTypeNdx = selectMatchingMemoryType(m_memProps, memReqs.memoryTypeBits, requirement);
    DE_UNREF(hostIntent);
#else
    const auto memoryTypeNdx = selectBestMemoryType(m_memProps, memReqs.memoryTypeBits, requirement, hostIntent);
#endif

    // Align the offset to the requirements.
    // Aligning to the non coherent atom size prevents flush and memory invalidation valid usage errors.
    const auto requiredAlignment =
        (m_offsetParams ? de::lcm(m_offsetParams->nonCoherentAtomSize, memReqs.alignment) : memReqs.alignment);
    const auto offset = (m_offsetParams ? de::roundUp(m_offsetParams->offset, requiredAlignment) : 0);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        memReqs.size + offset,                  // VkDeviceSize allocationSize;
        memoryTypeNdx,                          // uint32_t memoryTypeIndex;
    };

    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, //    VkStructureType            sType
        nullptr,                                      //    const void*                pNext
        0,                                            //    VkMemoryAllocateFlags    flags
        0,                                            //    uint32_t                deviceMask
    };

    VkMemoryOpaqueCaptureAddressAllocateInfoKHR captureInfo = {
        VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO, // VkStructureType sType
        nullptr,                                                       // const void*     pNext
        memoryOpaqueCaptureAddr,                                       // uint64_t        opaqueCaptureAddress
    };

    if (requirement & MemoryRequirement::DeviceAddress)
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    if (requirement & MemoryRequirement::DeviceAddressCaptureReplay)
    {
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;

        if (memoryOpaqueCaptureAddr)
            allocFlagsInfo.pNext = &captureInfo;
    }

    if (allocFlagsInfo.flags)
        allocInfo.pNext = &allocFlagsInfo;

    Move<VkDeviceMemory> mem = allocateMemory(m_vk, m_device, &allocInfo);
    MovePtr<HostPtr> hostPtr;

    if (requirement & MemoryRequirement::HostVisible)
    {
        DE_ASSERT(isHostVisibleMemory(m_memProps, allocInfo.memoryTypeIndex));
        hostPtr = MovePtr<HostPtr>(new HostPtr(m_vk, m_device, *mem, offset, memReqs.size, 0u));
    }

    return MovePtr<Allocation>(new SimpleAllocation(mem, hostPtr, static_cast<size_t>(offset)));
}

MovePtr<Allocation> SimpleAllocator::allocate(const VkMemoryRequirements &memReqs, MemoryRequirement requirement,
                                              uint64_t memoryOpaqueCaptureAddr)
{
    return SimpleAllocator::allocate(memReqs, requirement, tcu::Nothing, memoryOpaqueCaptureAddr);
}

MovePtr<Allocation> SimpleAllocator::allocate(const VkMemoryRequirements &memReqs, HostIntent intent,
                                              VkMemoryAllocateFlags allocFlags)
{
    const bool devAddrCR = (allocFlags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);
    const bool devAddr   = (allocFlags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

    const auto baseReq = ((intent == HostIntent::NONE) ? MemoryRequirement::Any : MemoryRequirement::HostVisible);
    const auto crReq   = (devAddrCR ? MemoryRequirement::DeviceAddressCaptureReplay : MemoryRequirement::Any);
    const auto daReq   = (devAddr ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any);

    const auto requirement = (baseReq | crReq | daReq);
    return SimpleAllocator::allocate(memReqs, requirement, tcu::just(intent));
}

MovePtr<Allocation> allocateExtended(const InstanceInterface &vki, const DeviceInterface &vkd,
                                     const VkPhysicalDevice &physDevice, const VkDevice device,
                                     const VkMemoryRequirements &memReqs, const MemoryRequirement requirement,
                                     const void *pNext)
{
    const VkPhysicalDeviceMemoryProperties memoryProperties = getPhysicalDeviceMemoryProperties(vki, physDevice);
    const uint32_t memoryTypeNdx = selectMatchingMemoryType(memoryProperties, memReqs.memoryTypeBits, requirement);
    const VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, //    VkStructureType    sType
        pNext,                                  //    const void*        pNext
        memReqs.size,                           //    VkDeviceSize    allocationSize
        memoryTypeNdx,                          //    uint32_t        memoryTypeIndex
    };
    Move<VkDeviceMemory> mem = allocateMemory(vkd, device, &allocInfo);
    MovePtr<HostPtr> hostPtr;

    if (requirement & MemoryRequirement::HostVisible)
    {
        DE_ASSERT(isHostVisibleMemory(memoryProperties, allocInfo.memoryTypeIndex));
        hostPtr = MovePtr<HostPtr>(new HostPtr(vkd, device, *mem, 0u, allocInfo.allocationSize, 0u));
    }

    return MovePtr<Allocation>(new SimpleAllocation(mem, hostPtr, 0u));
}

de::MovePtr<Allocation> allocateDedicated(const InstanceInterface &vki, const DeviceInterface &vkd,
                                          const VkPhysicalDevice &physDevice, const VkDevice device,
                                          const VkBuffer buffer, MemoryRequirement requirement)
{
    const VkMemoryRequirements memoryRequirements               = getBufferMemoryRequirements(vkd, device, buffer);
    const VkMemoryDedicatedAllocateInfo dedicatedAllocationInfo = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, // VkStructureType        sType
        nullptr,                                          // const void*            pNext
        VK_NULL_HANDLE,                                   // VkImage                image
        buffer                                            // VkBuffer                buffer
    };

    return allocateExtended(vki, vkd, physDevice, device, memoryRequirements, requirement, &dedicatedAllocationInfo);
}

de::MovePtr<Allocation> allocateDedicated(const InstanceInterface &vki, const DeviceInterface &vkd,
                                          const VkPhysicalDevice &physDevice, const VkDevice device,
                                          const VkImage image, MemoryRequirement requirement)
{
    const VkMemoryRequirements memoryRequirements               = getImageMemoryRequirements(vkd, device, image);
    const VkMemoryDedicatedAllocateInfo dedicatedAllocationInfo = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, // VkStructureType        sType
        nullptr,                                          // const void*            pNext
        image,                                            // VkImage                image
        VK_NULL_HANDLE                                    // VkBuffer                buffer
    };

    return allocateExtended(vki, vkd, physDevice, device, memoryRequirements, requirement, &dedicatedAllocationInfo);
}

void *mapMemory(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory mem, VkDeviceSize offset, VkDeviceSize size,
                VkMemoryMapFlags flags)
{
    void *hostPtr = nullptr;
    VK_CHECK(vkd.mapMemory(device, mem, offset, size, flags, &hostPtr));
    TCU_CHECK(hostPtr);
    return hostPtr;
}

void flushMappedMemoryRange(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                            VkDeviceSize size)
{
    const VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory, offset, size};

    VK_CHECK(vkd.flushMappedMemoryRanges(device, 1u, &range));
}

void invalidateMappedMemoryRange(const DeviceInterface &vkd, VkDevice device, VkDeviceMemory memory,
                                 VkDeviceSize offset, VkDeviceSize size)
{
    const VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory, offset, size};

    VK_CHECK(vkd.invalidateMappedMemoryRanges(device, 1u, &range));
}

uint32_t selectMatchingMemoryType(const VkPhysicalDeviceMemoryProperties &deviceMemProps, uint32_t allowedMemTypeBits,
                                  MemoryRequirement requirement)
{
    const uint32_t compatibleTypes = getCompatibleMemoryTypes(deviceMemProps, requirement);
    uint32_t candidates            = allowedMemTypeBits & compatibleTypes;
#ifdef CTS_USES_VULKANSC
    // in case of Vulkan SC: prefer memory types from SEU-safe heaps ( SEU = single event upsets )
    const uint32_t seuSafeTypes = getSEUSafeMemoryTypes(deviceMemProps);
    uint32_t seuSafeCandidates  = candidates & seuSafeTypes;
    if (seuSafeCandidates != 0u)
        candidates = seuSafeCandidates;
#endif // CTS_USES_VULKANSC

    if (candidates == 0u)
        TCU_THROW(NotSupportedError, "No compatible memory type found");

    return (uint32_t)deCtz32(candidates);
}

namespace
{

struct MemoryTypeInfo
{
    uint32_t memoryTypeIndex;
    VkMemoryPropertyFlags memoryFlags;

    MemoryTypeInfo(uint32_t memTypeIndex, VkMemoryPropertyFlags memFlags)
        : memoryTypeIndex(memTypeIndex)
        , memoryFlags(memFlags)
    {
    }

    bool hasProperty(VkMemoryPropertyFlagBits property) const
    {
        return ((memoryFlags & property) != 0u);
    }
};

class MemoryTypeSorter
{
public:
    virtual bool operator()(const MemoryTypeInfo &, const MemoryTypeInfo &) const = 0;
    virtual ~MemoryTypeSorter()
    {
    }
};

class UnknownIntentSorter : public MemoryTypeSorter
{
public:
    UnknownIntentSorter()
    {
    }
    bool operator()(const MemoryTypeInfo &a, const MemoryTypeInfo &b) const override
    {
        // The strategy below has been reported to decrease overall CTS performance, so we use a simple classic
        // alternative instead where we just sort memory types by index to choose the first one that matches.
#if 0
        // Non-host-visible types come first.
        const bool aVisible = a.hasProperty(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        const bool bVisible = b.hasProperty(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        if (aVisible != bVisible)
            return (aVisible < bVisible);

        if (aVisible) // bVisible == true
        {
            // Prioritize cached host-visible memory.
            const bool aCached = a.hasProperty(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            const bool bCached = b.hasProperty(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

            if (aCached != bCached)
                return (aCached > bCached);

            // Otherwise, the one that is *not* coherent, because it's supposed to be faster.
            const bool aCoherent = a.hasProperty(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            const bool bCoherent = b.hasProperty(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (aCoherent != bCoherent)
                return (aCoherent < bCoherent);
        }
#endif

        // Fall back to memory index.
        return (a.memoryTypeIndex < b.memoryTypeIndex);
    }
};

class HostReadSorter : public MemoryTypeSorter
{
public:
    HostReadSorter()
    {
    }
    bool operator()(const MemoryTypeInfo &a, const MemoryTypeInfo &b) const override
    {
        // Prioritize host-cached memory so as not to hammer the possible PCIe bus.
        const bool aCached = a.hasProperty(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        const bool bCached = b.hasProperty(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

        if (aCached != bCached)
            return (aCached > bCached);

        // Otherwise, prioritize those types that are not device-local.
        if (!aCached)
        {
            const bool aLocal = a.hasProperty(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            const bool bLocal = b.hasProperty(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (aLocal != bLocal)
                return (aLocal < bLocal);
        }

        // Fall back to memory index.
        return (a.memoryTypeIndex < b.memoryTypeIndex);
    }
};

class HostWriteSorter : public MemoryTypeSorter
{
public:
    HostWriteSorter()
    {
    }
    bool operator()(const MemoryTypeInfo &a, const MemoryTypeInfo &b) const override
    {
        // Prioritize device-local memory types.
        const bool aLocal = a.hasProperty(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        const bool bLocal = b.hasProperty(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (aLocal != bLocal)
            return (aLocal > bLocal);

        if (!aLocal)
        {
            // From those, prioritize host-cached memory.
            const bool aCached = a.hasProperty(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            const bool bCached = b.hasProperty(VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

            if (aCached != bCached)
                return (aCached > bCached);
        }

        // Fall back to memory index.
        return (a.memoryTypeIndex < b.memoryTypeIndex);
    }
};

class NoIntentSorter : public MemoryTypeSorter
{
public:
    NoIntentSorter()
    {
    }
    bool operator()(const MemoryTypeInfo &a, const MemoryTypeInfo &b) const override
    {
        // Prioritize memory that is not host-visible.
        const bool aVisible = a.hasProperty(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        const bool bVisible = b.hasProperty(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        if (aVisible != bVisible)
            return (aVisible < bVisible);

        // From those, the ones that are device local.
        const bool aLocal = a.hasProperty(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        const bool bLocal = b.hasProperty(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (aLocal != bLocal)
            return (aLocal > bLocal);

        // Fall back to memory index.
        return (a.memoryTypeIndex < b.memoryTypeIndex);
    }
};

using MemoryTypeSorterPtr = std::unique_ptr<MemoryTypeSorter>;

MemoryTypeSorterPtr buildMemoryTypeSorter(const tcu::Maybe<HostIntent> &hostIntent)
{
    if (!hostIntent)
        return MemoryTypeSorterPtr(new UnknownIntentSorter());

    switch (hostIntent.get())
    {
    case vk::HostIntent::NONE:
        return MemoryTypeSorterPtr(new NoIntentSorter());
    case vk::HostIntent::R:
        return MemoryTypeSorterPtr(new HostReadSorter());
    case vk::HostIntent::W:
        return MemoryTypeSorterPtr(new HostWriteSorter());
    case vk::HostIntent::RW:
        return MemoryTypeSorterPtr(new HostReadSorter());
    default:
        break;
    }

    return MemoryTypeSorterPtr();
}

// Separate class that can be copied and used by std::sort. Does not own the memory type sorter.
class MemoryTypeComp
{
public:
    MemoryTypeComp(const MemoryTypeSorter *p) : m_sorter(p)
    {
    }
    bool operator()(const MemoryTypeInfo &a, const MemoryTypeInfo &b) const
    {
        return (*m_sorter)(a, b);
    }

protected:
    const MemoryTypeSorter *m_sorter;
};

} // namespace

uint32_t selectBestMemoryType(const VkPhysicalDeviceMemoryProperties &deviceMemProps, uint32_t allowedMemTypeBits,
                              MemoryRequirement requirement, const tcu::Maybe<HostIntent> &hostIntent)
{
    if (hostIntent && hostIntent.get() != HostIntent::NONE)
        DE_ASSERT(!!(requirement & MemoryRequirement::HostVisible));

    std::vector<MemoryTypeInfo> memoryTypes;
    memoryTypes.reserve(deviceMemProps.memoryTypeCount);

    for (uint32_t memoryTypeIndex = 0u; memoryTypeIndex < deviceMemProps.memoryTypeCount; ++memoryTypeIndex)
    {
        const auto memTypeBit    = (1u << memoryTypeIndex);
        const auto &memTypeFlags = deviceMemProps.memoryTypes[memoryTypeIndex].propertyFlags;

        if ((allowedMemTypeBits & memTypeBit) && requirement.matchesHeap(memTypeFlags))
            memoryTypes.emplace_back(memoryTypeIndex, memTypeFlags);
    }

    if (memoryTypes.empty())
        TCU_THROW(NotSupportedError, "No compatible memory type found");

    auto sorter = buildMemoryTypeSorter(hostIntent);
    std::sort(begin(memoryTypes), end(memoryTypes), MemoryTypeComp(sorter.get()));
    return memoryTypes.front().memoryTypeIndex;
}

uint32_t getCompatibleMemoryTypes(const VkPhysicalDeviceMemoryProperties &deviceMemProps, MemoryRequirement requirement)
{
    uint32_t compatibleTypes = 0u;

    for (uint32_t memoryTypeNdx = 0; memoryTypeNdx < deviceMemProps.memoryTypeCount; memoryTypeNdx++)
    {
        if (requirement.matchesHeap(deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags))
            compatibleTypes |= (1u << memoryTypeNdx);
    }

    return compatibleTypes;
}

#ifdef CTS_USES_VULKANSC

uint32_t getSEUSafeMemoryTypes(const VkPhysicalDeviceMemoryProperties &deviceMemProps)
{
    uint32_t seuSafeTypes = 0u;

    for (uint32_t memoryTypeNdx = 0; memoryTypeNdx < deviceMemProps.memoryTypeCount; memoryTypeNdx++)
    {
        if ((deviceMemProps.memoryHeaps[deviceMemProps.memoryTypes[memoryTypeNdx].heapIndex].flags &
             VK_MEMORY_HEAP_SEU_SAFE_BIT) != 0u)
            seuSafeTypes |= (1u << memoryTypeNdx);
    }
    return seuSafeTypes;
}

#endif // CTS_USES_VULKANSC

void bindImagePlanesMemory(const DeviceInterface &vkd, const VkDevice device, const VkImage image,
                           const uint32_t numPlanes, vector<AllocationSp> &allocations, vk::Allocator &allocator,
                           const vk::MemoryRequirement requirement)
{
    vector<VkBindImageMemoryInfo> coreInfos;
    vector<VkBindImagePlaneMemoryInfo> planeInfos;
    coreInfos.reserve(numPlanes);
    planeInfos.reserve(numPlanes);

    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        const VkImageAspectFlagBits planeAspect = getPlaneAspect(planeNdx);
        const VkMemoryRequirements reqs         = getImagePlaneMemoryRequirements(vkd, device, image, planeAspect);

        allocations.push_back(AllocationSp(allocator.allocate(reqs, requirement).release()));

        VkBindImagePlaneMemoryInfo planeInfo = {VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO, nullptr, planeAspect};
        planeInfos.push_back(planeInfo);

        VkBindImageMemoryInfo coreInfo = {
            VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, &planeInfos.back(), image, allocations.back()->getMemory(),
            allocations.back()->getOffset(),
        };
        coreInfos.push_back(coreInfo);
    }

    VK_CHECK(vkd.bindImageMemory2(device, numPlanes, coreInfos.data()));
}

MovePtr<Allocation> bindImage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                              const VkImage image, const MemoryRequirement requirement)
{
    MovePtr<Allocation> alloc = allocator.allocate(getImageMemoryRequirements(vk, device, image), requirement);
    VK_CHECK(vk.bindImageMemory(device, image, alloc->getMemory(), alloc->getOffset()));
    return alloc;
}

de::MovePtr<Allocation> bindImage(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                  const VkImage image, const HostIntent hostIntent,
                                  const VkMemoryAllocateFlags memAllocFlags)
{
    MovePtr<Allocation> alloc =
        allocator.allocate(getImageMemoryRequirements(vk, device, image), hostIntent, memAllocFlags);
    VK_CHECK(vk.bindImageMemory(device, image, alloc->getMemory(), alloc->getOffset()));
    return alloc;
}

MovePtr<Allocation> bindBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                               const VkBuffer buffer, const MemoryRequirement requirement)
{
    MovePtr<Allocation> alloc(allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), requirement));
    VK_CHECK(vk.bindBufferMemory(device, buffer, alloc->getMemory(), alloc->getOffset()));
    return alloc;
}

de::MovePtr<Allocation> bindBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                   const VkBuffer buffer, const HostIntent hostIntent,
                                   const VkMemoryAllocateFlags memAllocFlags)
{
    MovePtr<Allocation> alloc(
        allocator.allocate(getBufferMemoryRequirements(vk, device, buffer), hostIntent, memAllocFlags));
    VK_CHECK(vk.bindBufferMemory(device, buffer, alloc->getMemory(), alloc->getOffset()));
    return alloc;
}

void zeroBuffer(const DeviceInterface &vk, const VkDevice device, const Allocation &alloc, const VkDeviceSize size)
{
    deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(size));
    flushAlloc(vk, device, alloc);
}

} // namespace vk
