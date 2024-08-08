/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Simple memory mapping tests.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryMappingTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "tcuMaybe.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuCommandLine.hpp"

#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkImageUtil.hpp"

#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSTLUtil.hpp"
#include "deMath.h"

#include <string>
#include <vector>
#include <algorithm>

using tcu::Maybe;
using tcu::TestLog;

using de::SharedPtr;

using std::pair;
using std::string;
using std::vector;

using namespace vk;

namespace vkt
{
namespace memory
{
namespace
{
template <typename T>
T divRoundUp(const T &a, const T &b)
{
    return (a / b) + (a % b == 0 ? 0 : 1);
}

template <typename T>
T roundDownToMultiple(const T &a, const T &b)
{
    return b * (a / b);
}

template <typename T>
T roundUpToMultiple(const T &a, const T &b)
{
    return b * (a / b + (a % b != 0 ? 1 : 0));
}

enum AllocationKind
{
    ALLOCATION_KIND_SUBALLOCATED     = 0,
    ALLOCATION_KIND_DEDICATED_BUFFER = 1,
    ALLOCATION_KIND_DEDICATED_IMAGE  = 2,
    ALLOCATION_KIND_LAST
};

void mapMemoryWrapper(const DeviceInterface &vkd, VkDevice device, vk::VkDeviceMemory memory,
                      VkDeviceSize mappingOffset, VkDeviceSize mappingSize, void **ptr, bool useMap2)
{
    if (!useMap2)
    {
        VK_CHECK(vkd.mapMemory(device, memory, mappingOffset, mappingSize, 0u, ptr));
    }
    else
    {
        const VkMemoryMapInfoKHR info = {
            VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR, // VkStructureType    sType
            nullptr,                               // const void        *pNext
            0u,                                    // VkMemoryMapFlags flags
            memory,                                // VkDeviceMemory    memory
            mappingOffset,                         // VkDeviceSize        offset
            mappingSize,                           // VkDeviceSize        size
        };
        VK_CHECK(vkd.mapMemory2KHR(device, &info, ptr));
    }
}

void unmapMemoryWrapper(const DeviceInterface &vkd, VkDevice device, vk::VkDeviceMemory memory, bool useMap2)
{
    if (!useMap2)
    {
        vkd.unmapMemory(device, memory);
    }
    else
    {
        const VkMemoryUnmapInfoKHR unmap{
            VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR, // VkStructureType            sType
            nullptr,                                 // const void*                pNext
            0u,                                      // VkMemoryUnmapFlagsEXT    flags
            memory,                                  // VkDeviceMemory            memory
        };
        VK_CHECK(vkd.unmapMemory2KHR(device, &unmap));
    }
}

// \note Bit vector that guarantees that each value takes only one bit.
// std::vector<bool> is often optimized to only take one bit for each bool, but
// that is implementation detail and in this case we really need to known how much
// memory is used.
class BitVector
{
public:
    enum
    {
        BLOCK_BIT_SIZE = 8 * sizeof(uint32_t)
    };

    BitVector(size_t size, bool value = false)
        : m_data(divRoundUp<size_t>(size, (size_t)BLOCK_BIT_SIZE), value ? ~0x0u : 0x0u)
    {
    }

    bool get(size_t ndx) const
    {
        return (m_data[ndx / BLOCK_BIT_SIZE] & (0x1u << (uint32_t)(ndx % BLOCK_BIT_SIZE))) != 0;
    }

    void set(size_t ndx, bool value)
    {
        if (value)
            m_data[ndx / BLOCK_BIT_SIZE] |= 0x1u << (uint32_t)(ndx % BLOCK_BIT_SIZE);
        else
            m_data[ndx / BLOCK_BIT_SIZE] &= ~(0x1u << (uint32_t)(ndx % BLOCK_BIT_SIZE));
    }

    void setRange(size_t offset, size_t count, bool value)
    {
        size_t ndx = offset;

        for (; (ndx < offset + count) && ((ndx % BLOCK_BIT_SIZE) != 0); ndx++)
        {
            DE_ASSERT(ndx >= offset);
            DE_ASSERT(ndx < offset + count);
            set(ndx, value);
        }

        {
            const size_t endOfFullBlockNdx = roundDownToMultiple<size_t>(offset + count, BLOCK_BIT_SIZE);

            if (ndx < endOfFullBlockNdx)
            {
                deMemset(&m_data[ndx / BLOCK_BIT_SIZE], (value ? 0xFF : 0x0), (endOfFullBlockNdx - ndx) / 8);
                ndx = endOfFullBlockNdx;
            }
        }

        for (; ndx < offset + count; ndx++)
        {
            DE_ASSERT(ndx >= offset);
            DE_ASSERT(ndx < offset + count);
            set(ndx, value);
        }
    }

    void vectorAnd(const BitVector &other, size_t offset, size_t count)
    {
        size_t ndx = offset;

        for (; ndx < offset + count && (ndx % BLOCK_BIT_SIZE) != 0; ndx++)
        {
            DE_ASSERT(ndx >= offset);
            DE_ASSERT(ndx < offset + count);
            set(ndx, other.get(ndx) && get(ndx));
        }

        for (; ndx < roundDownToMultiple<size_t>(offset + count, BLOCK_BIT_SIZE); ndx += BLOCK_BIT_SIZE)
        {
            DE_ASSERT(ndx >= offset);
            DE_ASSERT(ndx < offset + count);
            DE_ASSERT(ndx % BLOCK_BIT_SIZE == 0);
            DE_ASSERT(ndx + BLOCK_BIT_SIZE <= offset + count);
            m_data[ndx / BLOCK_BIT_SIZE] &= other.m_data[ndx / BLOCK_BIT_SIZE];
        }

        for (; ndx < offset + count; ndx++)
        {
            DE_ASSERT(ndx >= offset);
            DE_ASSERT(ndx < offset + count);
            set(ndx, other.get(ndx) && get(ndx));
        }
    }

private:
    vector<uint32_t> m_data;
};

class ReferenceMemory
{
public:
    ReferenceMemory(size_t size, size_t atomSize)
        : m_atomSize(atomSize)
        , m_bytes(size, 0xDEu)
        , m_defined(size, false)
        , m_flushed(size / atomSize, false)
    {
        DE_ASSERT(size % m_atomSize == 0);
    }

    void write(size_t pos, uint8_t value)
    {
        m_bytes[pos] = value;
        m_defined.set(pos, true);
        m_flushed.set(pos / m_atomSize, false);
    }

    bool read(size_t pos, uint8_t value)
    {
        const bool isOk = !m_defined.get(pos) || m_bytes[pos] == value;

        m_bytes[pos] = value;
        m_defined.set(pos, true);

        return isOk;
    }

    bool modifyXor(size_t pos, uint8_t value, uint8_t mask)
    {
        const bool isOk = !m_defined.get(pos) || m_bytes[pos] == value;

        m_bytes[pos] = value ^ mask;
        m_defined.set(pos, true);
        m_flushed.set(pos / m_atomSize, false);

        return isOk;
    }

    void flush(size_t offset, size_t size)
    {
        DE_ASSERT((offset % m_atomSize) == 0);
        DE_ASSERT((size % m_atomSize) == 0);

        m_flushed.setRange(offset / m_atomSize, size / m_atomSize, true);
    }

    void invalidate(size_t offset, size_t size)
    {
        DE_ASSERT((offset % m_atomSize) == 0);
        DE_ASSERT((size % m_atomSize) == 0);

        if (m_atomSize == 1)
        {
            m_defined.vectorAnd(m_flushed, offset, size);
        }
        else
        {
            for (size_t ndx = 0; ndx < size / m_atomSize; ndx++)
            {
                if (!m_flushed.get((offset / m_atomSize) + ndx))
                    m_defined.setRange(offset + ndx * m_atomSize, m_atomSize, false);
            }
        }
    }

private:
    const size_t m_atomSize;
    vector<uint8_t> m_bytes;
    BitVector m_defined;
    BitVector m_flushed;
};

struct MemoryType
{
    MemoryType(uint32_t index_, const VkMemoryType &type_) : index(index_), type(type_)
    {
    }

    MemoryType(void) : index(~0u)
    {
    }

    uint32_t index;
    VkMemoryType type;
};

size_t computeDeviceMemorySystemMemFootprint(const DeviceInterface &vk, VkDevice device)
{
    AllocationCallbackRecorder callbackRecorder(getSystemAllocator());

    {
        // 1 B allocation from memory type 0
        const VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            1u,
            0u,
        };
        const Unique<VkDeviceMemory> memory(allocateMemory(vk, device, &allocInfo, callbackRecorder.getCallbacks()));
        AllocationCallbackValidationResults validateRes;

        validateAllocationCallbacks(callbackRecorder, &validateRes);

        TCU_CHECK(validateRes.violations.empty());

        return getLiveSystemAllocationTotal(validateRes) +
               sizeof(void *) * validateRes.liveAllocations.size(); // allocation overhead
    }
}

Move<VkImage> makeImage(const DeviceInterface &vk, VkDevice device, VkDeviceSize size, uint32_t queueFamilyIndex)
{
    const VkFormat formats[] = {
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R32G32B32A32_UINT,
    };

    VkFormat format         = VK_FORMAT_UNDEFINED;
    uint32_t powerOfTwoSize = 0;

    for (const VkFormat f : formats)
    {
        const int pixelSize             = vk::mapVkFormat(f).getPixelSize();
        const VkDeviceSize sizeInPixels = (size + 3u) / pixelSize;
        const uint32_t sqrtSize = static_cast<uint32_t>(deFloatCeil(deFloatSqrt(static_cast<float>(sizeInPixels))));

        format         = f;
        powerOfTwoSize = deSmallestGreaterOrEquallPowerOfTwoU32(sqrtSize);

        // maxImageDimension2D
        if (powerOfTwoSize < 4096)
            break;
    }

    const VkImageCreateInfo colorImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
        nullptr,                                                           // const void* pNext;
        0u,                                                                // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                  // VkImageType imageType;
        format,                                                            // VkFormat format;
        {powerOfTwoSize, powerOfTwoSize, 1u},                              // VkExtent3D extent;
        1u,                                                                // uint32_t mipLevels;
        1u,                                                                // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
        VK_IMAGE_TILING_LINEAR,                                            // VkImageTiling tiling;
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
        1u,                                                                // uint32_t queueFamilyCount;
        &queueFamilyIndex,                                                 // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
    };

    return createImage(vk, device, &colorImageParams);
}

Move<VkBuffer> makeBuffer(const DeviceInterface &vk, VkDevice device, VkDeviceSize size, uint32_t queueFamilyIndex)
{
    const VkBufferCreateInfo bufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                // VkStructureType sType;
        nullptr,                                                             // const void* pNext;
        0u,                                                                  // VkBufferCreateFlags flags;
        size,                                                                // VkDeviceSize size;
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                           // VkSharingMode sharingMode;
        1u,                                                                  // uint32_t queueFamilyCount;
        &queueFamilyIndex,                                                   // const uint32_t* pQueueFamilyIndices;
    };
    return vk::createBuffer(vk, device, &bufferParams, nullptr);
}

VkMemoryRequirements getImageMemoryRequirements(const DeviceInterface &vk, VkDevice device, Move<VkImage> &image)
{
    VkImageMemoryRequirementsInfo2 info = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, // VkStructureType            sType
        nullptr,                                            // const void*                pNext
        *image                                              // VkImage                    image
    };
    VkMemoryDedicatedRequirements dedicatedRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, // VkStructureType            sType
        nullptr,                                         // const void*                pNext
        VK_FALSE,                                        // VkBool32                    prefersDedicatedAllocation
        VK_FALSE                                         // VkBool32                    requiresDedicatedAllocation
    };
    VkMemoryRequirements2 req2 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, // VkStructureType            sType
        &dedicatedRequirements,                  // void*                    pNext
        {0, 0, 0}                                // VkMemoryRequirements        memoryRequirements
    };

    vk.getImageMemoryRequirements2(device, &info, &req2);

    return req2.memoryRequirements;
}

VkMemoryRequirements getBufferMemoryRequirements(const DeviceInterface &vk, VkDevice device, Move<VkBuffer> &buffer)
{
    VkBufferMemoryRequirementsInfo2 info = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, // VkStructureType            sType
        nullptr,                                             // const void*                pNext
        *buffer                                              // VkImage                    image
    };
    VkMemoryDedicatedRequirements dedicatedRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, // VkStructureType            sType
        nullptr,                                         // const void*                pNext
        VK_FALSE,                                        // VkBool32                    prefersDedicatedAllocation
        VK_FALSE                                         // VkBool32                    requiresDedicatedAllocation
    };
    VkMemoryRequirements2 req2 = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, // VkStructureType        sType
        &dedicatedRequirements,                  // void*                pNext
        {0, 0, 0}                                // VkMemoryRequirements    memoryRequirements
    };

    vk.getBufferMemoryRequirements2(device, &info, &req2);

    return req2.memoryRequirements;
}

Move<VkDeviceMemory> allocMemory(const DeviceInterface &vk, VkDevice device, VkDeviceSize pAllocInfo_allocationSize,
                                 uint32_t pAllocInfo_memoryTypeIndex)
{
    const VkMemoryAllocateInfo pAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        pAllocInfo_allocationSize,
        pAllocInfo_memoryTypeIndex,
    };
    return allocateMemory(vk, device, &pAllocInfo);
}

VkDeviceSize findLargeAllocationSize(const DeviceInterface &vk, VkDevice device, VkDeviceSize max,
                                     uint32_t memoryTypeIndex)
{
    // max must be power of two
    DE_ASSERT((max & (max - 1)) == 0);

    for (VkDeviceSize size = max; size > 0; size >>= 1)
    {
        const VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            size,
            memoryTypeIndex,
        };

        VkDeviceMemory memory;
        VkResult result = vk.allocateMemory(device, &allocInfo, NULL, &memory);

        if (result == VK_SUCCESS)
        {
            vk.freeMemory(device, memory, NULL);
            return size;
        }
    }

    return 0;
}

Move<VkDeviceMemory> allocMemory(const DeviceInterface &vk, VkDevice device, VkDeviceSize pAllocInfo_allocationSize,
                                 uint32_t pAllocInfo_memoryTypeIndex, Move<VkImage> &image, Move<VkBuffer> &buffer,
                                 const VkAllocationCallbacks *allocator = nullptr)
{
    DE_ASSERT((!image) || (!buffer));

    const VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, // VkStructureType        sType
        nullptr,                                          // const void*            pNext
        *image,                                           // VkImage                image
        *buffer                                           // VkBuffer                buffer
    };

    const VkMemoryAllocateInfo pAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        !image && !buffer ? nullptr : &dedicatedAllocateInfo,
        pAllocInfo_allocationSize,
        pAllocInfo_memoryTypeIndex,
    };
    return allocateMemory(vk, device, &pAllocInfo, allocator);
}

struct MemoryRange
{
    MemoryRange(VkDeviceSize offset_ = ~(VkDeviceSize)0, VkDeviceSize size_ = ~(VkDeviceSize)0)
        : offset(offset_)
        , size(size_)
    {
    }

    VkDeviceSize offset;
    VkDeviceSize size;
};

struct TestConfig
{
    TestConfig(void) : allocationSize(~(VkDeviceSize)0), allocationKind(ALLOCATION_KIND_SUBALLOCATED)
    {
    }

    VkDeviceSize allocationSize;
    uint32_t seed;

    MemoryRange mapping;
    vector<MemoryRange> flushMappings;
    vector<MemoryRange> invalidateMappings;
    bool remap;
    bool implicitUnmap;
    AllocationKind allocationKind;
    bool memoryMap2;
};

bool compareAndLogBuffer(TestLog &log, size_t size, size_t referenceSize, const uint8_t *result,
                         const uint8_t *reference)
{
    size_t stride      = size / referenceSize;
    size_t failedBytes = 0;
    size_t firstFailed = (size_t)-1;

    DE_ASSERT(referenceSize <= size);

    for (size_t ndx = 0; ndx < referenceSize; ndx += stride)
    {
        if (result[ndx * stride] != reference[ndx])
        {
            failedBytes++;

            if (firstFailed == (size_t)-1)
                firstFailed = ndx;
        }
    }

    if (failedBytes > 0)
    {
        log << TestLog::Message << "Comparison failed. Failed bytes " << failedBytes << ". First failed at offset "
            << firstFailed << "." << TestLog::EndMessage;

        std::ostringstream expectedValues;
        std::ostringstream resultValues;

        for (size_t ndx = firstFailed; ndx < firstFailed + 10 && ndx < referenceSize; ndx++)
        {
            if (ndx != firstFailed)
            {
                expectedValues << ", ";
                resultValues << ", ";
            }

            expectedValues << reference[ndx];
            resultValues << result[ndx * stride];
        }

        if (firstFailed + 10 < size)
        {
            expectedValues << "...";
            resultValues << "...";
        }

        log << TestLog::Message << "Expected values at offset: " << firstFailed << ", " << expectedValues.str()
            << TestLog::EndMessage;
        log << TestLog::Message << "Result values at offset: " << firstFailed << ", " << resultValues.str()
            << TestLog::EndMessage;

        return false;
    }
    else
        return true;
}

static Move<VkDevice> createProtectedMemoryDevice(const Context &context, const VkPhysicalDeviceFeatures2 &features2)
{
    auto &cmdLine                = context.getTestContext().getCommandLine();
    const InstanceInterface &vki = context.getInstanceInterface();
    const float queuePriority    = 1.0f;
    uint32_t queueFamilyIndex    = context.getUniversalQueueFamilyIndex();

    // Enable VK_KHR_map_memory2 if supported, as required by some tests.
    std::vector<const char *> enabledExtensions;
    if (context.isDeviceFunctionalitySupported("VK_KHR_map_memory2"))
        enabledExtensions.push_back("VK_KHR_map_memory2");

    VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        vk::VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT,   // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    const VkDeviceCreateInfo deviceInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        &features2,                           // const void* pNext;
        (VkDeviceCreateFlags)0,               // VkDeviceCreateFlags flags;
        1u,                                   // uint32_t queueCreateInfoCount;
        &queueInfo,                           // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        de::sizeU32(enabledExtensions),       // uint32_t enabledExtensionCount;
        de::dataOrNull(enabledExtensions),    // const char* const* ppEnabledExtensionNames;
        nullptr,                              // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return createCustomDevice(cmdLine.isValidationEnabled(), context.getPlatformInterface(), context.getInstance(), vki,
                              context.getPhysicalDevice(), &deviceInfo);
}

tcu::TestStatus testMemoryMapping(Context &context, const TestConfig config)
{
    TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector result(log);
    bool atLeastOneTestPerformed                            = false;
    const VkPhysicalDevice physicalDevice                   = context.getPhysicalDevice();
    const InstanceInterface &vki                            = context.getInstanceInterface();
    const DeviceInterface &vkd                              = context.getDeviceInterface();
    const VkPhysicalDeviceMemoryProperties memoryProperties = getPhysicalDeviceMemoryProperties(vki, physicalDevice);
    const VkDeviceSize nonCoherentAtomSize                  = context.getDeviceProperties().limits.nonCoherentAtomSize;
    const uint32_t queueFamilyIndex                         = context.getUniversalQueueFamilyIndex();

    //Create protected memory device if protected memory is supported
    //otherwise use the default device
    Move<VkDevice> protectMemoryDevice;
    VkDevice device;
    {
        VkPhysicalDeviceProtectedMemoryFeatures protectedFeatures;
        protectedFeatures.sType           = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES;
        protectedFeatures.pNext           = nullptr;
        protectedFeatures.protectedMemory = VK_FALSE;

        VkPhysicalDeviceFeatures2 deviceFeatures2;
        deviceFeatures2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &protectedFeatures;

        vki.getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &deviceFeatures2);
        if (protectedFeatures.protectedMemory && config.implicitUnmap)
        {
            protectMemoryDevice = createProtectedMemoryDevice(context, deviceFeatures2);
            device              = *protectMemoryDevice;
        }
        else
        {
            device = context.getDevice();
        }
    }

    {
        const tcu::ScopedLogSection section(log, "TestCaseInfo", "TestCaseInfo");

        log << TestLog::Message << "Seed: " << config.seed << TestLog::EndMessage;
        log << TestLog::Message << "Allocation size: " << config.allocationSize << TestLog::EndMessage;
        log << TestLog::Message << "Mapping, offset: " << config.mapping.offset << ", size: " << config.mapping.size
            << TestLog::EndMessage;

        if (!config.flushMappings.empty())
        {
            log << TestLog::Message << "Invalidating following ranges:" << TestLog::EndMessage;

            for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
                log << TestLog::Message << "\tOffset: " << config.flushMappings[ndx].offset
                    << ", Size: " << config.flushMappings[ndx].size << TestLog::EndMessage;
        }

        if (config.remap)
            log << TestLog::Message << "Remapping memory between flush and invalidation." << TestLog::EndMessage;

        if (!config.invalidateMappings.empty())
        {
            log << TestLog::Message << "Flushing following ranges:" << TestLog::EndMessage;

            for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
                log << TestLog::Message << "\tOffset: " << config.invalidateMappings[ndx].offset
                    << ", Size: " << config.invalidateMappings[ndx].size << TestLog::EndMessage;
        }
    }

    for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; memoryTypeIndex++)
    {
        try
        {
            const tcu::ScopedLogSection section(log, "MemoryType" + de::toString(memoryTypeIndex),
                                                "MemoryType" + de::toString(memoryTypeIndex));
            const vk::VkMemoryType &memoryType = memoryProperties.memoryTypes[memoryTypeIndex];
            const VkMemoryHeap &memoryHeap     = memoryProperties.memoryHeaps[memoryType.heapIndex];
            const VkDeviceSize atomSize        = nonCoherentAtomSize;
            const VkDeviceSize stride          = config.implicitUnmap ? 1024 : 1;
            const uint32_t iterations          = config.implicitUnmap ? 128 : 1;

            VkDeviceSize allocationSize = (config.allocationSize % atomSize == 0) ?
                                              config.allocationSize :
                                              config.allocationSize + (atomSize - (config.allocationSize % atomSize));
            size_t referenceSize        = 0;
            vector<uint8_t> reference;

            if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) != 0 &&
                !context.getCoherentMemoryFeaturesAMD().deviceCoherentMemory)
                continue;

            if (config.implicitUnmap)
            {
                VkDeviceSize max = 0x10000000; // 256MiB

                while (memoryHeap.size <= 4 * max)
                    max >>= 1;

                allocationSize = findLargeAllocationSize(vkd, device, max, memoryTypeIndex);
            }

            vk::VkMemoryRequirements req = {(VkDeviceSize)allocationSize, (VkDeviceSize)0, ~(uint32_t)0u};
            Move<VkImage> image;
            Move<VkBuffer> buffer;

            if (config.allocationKind == ALLOCATION_KIND_DEDICATED_IMAGE)
            {
                image = makeImage(vkd, device, allocationSize, queueFamilyIndex);
                req   = getImageMemoryRequirements(vkd, device, image);
            }
            else if (config.allocationKind == ALLOCATION_KIND_DEDICATED_BUFFER)
            {
                buffer = makeBuffer(vkd, device, allocationSize, queueFamilyIndex);
                req    = getBufferMemoryRequirements(vkd, device, buffer);
            }
            allocationSize             = req.size;
            VkDeviceSize mappingSize   = (config.mapping.size % atomSize == 0) ?
                                             config.mapping.size :
                                             config.mapping.size + (atomSize - (config.mapping.size % atomSize));
            VkDeviceSize mappingOffset = (config.mapping.offset % atomSize == 0) ?
                                             config.mapping.offset :
                                             config.mapping.offset - (config.mapping.offset % atomSize);
            if (config.mapping.size == config.allocationSize && config.mapping.offset == 0u)
            {
                mappingSize = allocationSize;
            }

            referenceSize = static_cast<size_t>(mappingSize / stride);
            reference.resize(static_cast<size_t>(mappingOffset) + referenceSize);

            log << TestLog::Message << "MemoryType: " << memoryType << TestLog::EndMessage;
            log << TestLog::Message << "MemoryHeap: " << memoryHeap << TestLog::EndMessage;
            log << TestLog::Message << "AtomSize: " << atomSize << TestLog::EndMessage;
            log << TestLog::Message << "AllocationSize: " << allocationSize << TestLog::EndMessage;
            log << TestLog::Message << "Mapping, offset: " << mappingOffset << ", size: " << mappingSize
                << TestLog::EndMessage;

            if ((req.memoryTypeBits & (1u << memoryTypeIndex)) == 0)
            {
                static const char *const allocationKindName[] = {"suballocation", "dedicated allocation of buffers",
                                                                 "dedicated allocation of images"};
                log << TestLog::Message << "Memory type does not support "
                    << allocationKindName[static_cast<uint32_t>(config.allocationKind)] << '.' << TestLog::EndMessage;
                continue;
            }

            if (!config.flushMappings.empty())
            {
                log << TestLog::Message << "Invalidating following ranges:" << TestLog::EndMessage;

                for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
                {
                    const VkDeviceSize offset =
                        (config.flushMappings[ndx].offset % atomSize == 0) ?
                            config.flushMappings[ndx].offset :
                            config.flushMappings[ndx].offset - (config.flushMappings[ndx].offset % atomSize);
                    const VkDeviceSize size =
                        (config.flushMappings[ndx].size % atomSize == 0) ?
                            config.flushMappings[ndx].size :
                            config.flushMappings[ndx].size + (atomSize - (config.flushMappings[ndx].size % atomSize));
                    log << TestLog::Message << "\tOffset: " << offset << ", Size: " << size << TestLog::EndMessage;
                }
            }

            if (!config.invalidateMappings.empty())
            {
                log << TestLog::Message << "Flushing following ranges:" << TestLog::EndMessage;

                for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
                {
                    const VkDeviceSize offset =
                        (config.invalidateMappings[ndx].offset % atomSize == 0) ?
                            config.invalidateMappings[ndx].offset :
                            config.invalidateMappings[ndx].offset - (config.invalidateMappings[ndx].offset % atomSize);
                    const VkDeviceSize size = (config.invalidateMappings[ndx].size % atomSize == 0) ?
                                                  config.invalidateMappings[ndx].size :
                                                  config.invalidateMappings[ndx].size +
                                                      (atomSize - (config.invalidateMappings[ndx].size % atomSize));
                    log << TestLog::Message << "\tOffset: " << offset << ", Size: " << size << TestLog::EndMessage;
                }
            }

            if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
            {
                log << TestLog::Message << "Memory type doesn't support mapping." << TestLog::EndMessage;
            }
            else if (memoryHeap.size <= 4 * allocationSize)
            {
                log << TestLog::Message << "Memory type's heap is too small." << TestLog::EndMessage;
            }
            else
                for (uint32_t iteration = 0; iteration < iterations; iteration++)
                {
                    atLeastOneTestPerformed = true;
                    AllocationCallbackRecorder recorder(getSystemAllocator());
                    const VkAllocationCallbacks *allocator = config.implicitUnmap ? recorder.getCallbacks() : nullptr;
                    Move<VkDeviceMemory> memory(
                        allocMemory(vkd, device, allocationSize, memoryTypeIndex, image, buffer, allocator));
                    de::Random rng(config.seed);
                    uint8_t *mapping = nullptr;

                    {
                        void *ptr;
                        mapMemoryWrapper(vkd, device, *memory, mappingOffset, mappingSize, &ptr, config.memoryMap2);
                        TCU_CHECK(ptr);

                        mapping = (uint8_t *)ptr;
                    }

                    for (VkDeviceSize ndx = 0; ndx < referenceSize; ndx += stride)
                    {
                        const uint8_t val = rng.getUint8();

                        mapping[ndx * stride]                    = val;
                        reference[(size_t)(mappingOffset + ndx)] = val;
                    }

                    if (!config.flushMappings.empty())
                    {
                        vector<VkMappedMemoryRange> ranges;

                        for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
                        {
                            const VkMappedMemoryRange range = {
                                VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                nullptr,

                                *memory,
                                (config.flushMappings[ndx].offset % atomSize == 0) ?
                                    config.flushMappings[ndx].offset :
                                    config.flushMappings[ndx].offset - (config.flushMappings[ndx].offset % atomSize),
                                (config.flushMappings[ndx].size % atomSize == 0) ?
                                    config.flushMappings[ndx].size :
                                    config.flushMappings[ndx].size +
                                        (atomSize - (config.flushMappings[ndx].size % atomSize)),
                            };

                            ranges.push_back(range);
                        }

                        VK_CHECK(vkd.flushMappedMemoryRanges(device, (uint32_t)ranges.size(), &ranges[0]));
                    }

                    if (config.remap)
                    {
                        unmapMemoryWrapper(vkd, device, *memory, config.memoryMap2);
                        void *ptr;
                        mapMemoryWrapper(vkd, device, *memory, mappingOffset, mappingSize, &ptr, config.memoryMap2);

                        TCU_CHECK(ptr);

                        mapping = (uint8_t *)ptr;
                    }

                    if (!config.invalidateMappings.empty())
                    {
                        vector<VkMappedMemoryRange> ranges;

                        for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
                        {
                            const VkMappedMemoryRange range = {
                                VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                nullptr,

                                *memory,
                                (config.invalidateMappings[ndx].offset % atomSize == 0) ?
                                    config.invalidateMappings[ndx].offset :
                                    config.invalidateMappings[ndx].offset -
                                        (config.invalidateMappings[ndx].offset % atomSize),
                                (config.invalidateMappings[ndx].size % atomSize == 0) ?
                                    config.invalidateMappings[ndx].size :
                                    config.invalidateMappings[ndx].size +
                                        (atomSize - (config.invalidateMappings[ndx].size % atomSize)),
                            };

                            ranges.push_back(range);
                        }

                        VK_CHECK(
                            vkd.invalidateMappedMemoryRanges(device, static_cast<uint32_t>(ranges.size()), &ranges[0]));
                    }

                    if (!compareAndLogBuffer(log, static_cast<size_t>(mappingSize), referenceSize, mapping,
                                             &reference[static_cast<size_t>(mappingOffset)]))
                        result.fail("Unexpected values read from mapped memory.");

                    if (config.implicitUnmap)
                    {
                        AllocationCallbackValidationResults results;

                        vkd.freeMemory(device, memory.disown(), allocator);
                        validateAllocationCallbacks(recorder, &results);

                        if (!results.liveAllocations.empty())
                            result.fail("Live allocations remain after freeing mapped memory");
                    }
                    else
                    {
                        unmapMemoryWrapper(vkd, device, *memory, config.memoryMap2);
                    }

                    context.getTestContext().touchWatchdog();
                }
        }
        catch (const tcu::TestError &error)
        {
            result.fail(error.getMessage());
        }
    }

    if (!atLeastOneTestPerformed)
        result.addResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable memory kind found to perform test.");

    return tcu::TestStatus(result.getResult(), result.getMessage());
}

class MemoryMapping
{
public:
    MemoryMapping(const MemoryRange &range, void *ptr, ReferenceMemory &reference);

    void randomRead(de::Random &rng);
    void randomWrite(de::Random &rng);
    void randomModify(de::Random &rng);

    const MemoryRange &getRange(void) const
    {
        return m_range;
    }

private:
    MemoryRange m_range;
    void *m_ptr;
    ReferenceMemory &m_reference;
};

MemoryMapping::MemoryMapping(const MemoryRange &range, void *ptr, ReferenceMemory &reference)
    : m_range(range)
    , m_ptr(ptr)
    , m_reference(reference)
{
    DE_ASSERT(range.size > 0);
}

void MemoryMapping::randomRead(de::Random &rng)
{
    const size_t count = (size_t)rng.getInt(0, 100);

    for (size_t ndx = 0; ndx < count; ndx++)
    {
        const size_t pos  = (size_t)(rng.getUint64() % (uint64_t)m_range.size);
        const uint8_t val = ((uint8_t *)m_ptr)[pos];

        TCU_CHECK(m_reference.read((size_t)(m_range.offset + pos), val));
    }
}

void MemoryMapping::randomWrite(de::Random &rng)
{
    const size_t count = (size_t)rng.getInt(0, 100);

    for (size_t ndx = 0; ndx < count; ndx++)
    {
        const size_t pos  = (size_t)(rng.getUint64() % (uint64_t)m_range.size);
        const uint8_t val = rng.getUint8();

        ((uint8_t *)m_ptr)[pos] = val;
        m_reference.write((size_t)(m_range.offset + pos), val);
    }
}

void MemoryMapping::randomModify(de::Random &rng)
{
    const size_t count = (size_t)rng.getInt(0, 100);

    for (size_t ndx = 0; ndx < count; ndx++)
    {
        const size_t pos   = (size_t)(rng.getUint64() % (uint64_t)m_range.size);
        const uint8_t val  = ((uint8_t *)m_ptr)[pos];
        const uint8_t mask = rng.getUint8();

        ((uint8_t *)m_ptr)[pos] = val ^ mask;
        TCU_CHECK(m_reference.modifyXor((size_t)(m_range.offset + pos), val, mask));
    }
}

VkDeviceSize randomSize(de::Random &rng, VkDeviceSize atomSize, VkDeviceSize maxSize)
{
    const VkDeviceSize maxSizeInAtoms = maxSize / atomSize;

    DE_ASSERT(maxSizeInAtoms > 0);

    return maxSizeInAtoms > 1 ? atomSize * (1 + (VkDeviceSize)(rng.getUint64() % (uint64_t)maxSizeInAtoms)) : atomSize;
}

VkDeviceSize randomOffset(de::Random &rng, VkDeviceSize atomSize, VkDeviceSize maxOffset)
{
    const VkDeviceSize maxOffsetInAtoms = maxOffset / atomSize;

    return maxOffsetInAtoms > 0 ? atomSize * (VkDeviceSize)(rng.getUint64() % (uint64_t)(maxOffsetInAtoms + 1)) : 0;
}

void randomRanges(de::Random &rng, vector<VkMappedMemoryRange> &ranges, size_t count, VkDeviceMemory memory,
                  VkDeviceSize minOffset, VkDeviceSize maxSize, VkDeviceSize atomSize)
{
    ranges.resize(count);

    for (size_t rangeNdx = 0; rangeNdx < count; rangeNdx++)
    {
        const VkDeviceSize size   = randomSize(rng, atomSize, maxSize);
        const VkDeviceSize offset = minOffset + randomOffset(rng, atomSize, maxSize - size);

        const VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr,

                                           memory, offset, size};
        ranges[rangeNdx]                = range;
    }
}

class MemoryObject
{
public:
    MemoryObject(const DeviceInterface &vkd, VkDevice device, VkDeviceSize size, uint32_t memoryTypeIndex,
                 VkDeviceSize atomSize, VkDeviceSize memoryUsage, VkDeviceSize referenceMemoryUsage);

    ~MemoryObject(void);

    MemoryMapping *mapRandom(const DeviceInterface &vkd, VkDevice device, de::Random &rng, bool map2);
    void unmap(bool map2);

    void randomFlush(const DeviceInterface &vkd, VkDevice device, de::Random &rng);
    void randomInvalidate(const DeviceInterface &vkd, VkDevice device, de::Random &rng);

    VkDeviceSize getSize(void) const
    {
        return m_size;
    }
    MemoryMapping *getMapping(void)
    {
        return m_mapping;
    }

    VkDeviceSize getMemoryUsage(void) const
    {
        return m_memoryUsage;
    }
    VkDeviceSize getReferenceMemoryUsage(void) const
    {
        return m_referenceMemoryUsage;
    }

private:
    const DeviceInterface &m_vkd;
    const VkDevice m_device;

    const uint32_t m_memoryTypeIndex;
    const VkDeviceSize m_size;
    const VkDeviceSize m_atomSize;
    const VkDeviceSize m_memoryUsage;
    const VkDeviceSize m_referenceMemoryUsage;

    Move<VkDeviceMemory> m_memory;

    MemoryMapping *m_mapping;
    ReferenceMemory m_referenceMemory;
};

MemoryObject::MemoryObject(const DeviceInterface &vkd, VkDevice device, VkDeviceSize size, uint32_t memoryTypeIndex,
                           VkDeviceSize atomSize, VkDeviceSize memoryUsage, VkDeviceSize referenceMemoryUsage)
    : m_vkd(vkd)
    , m_device(device)
    , m_memoryTypeIndex(memoryTypeIndex)
    , m_size(size)
    , m_atomSize(atomSize)
    , m_memoryUsage(memoryUsage)
    , m_referenceMemoryUsage(referenceMemoryUsage)
    , m_mapping(nullptr)
    , m_referenceMemory((size_t)size, (size_t)m_atomSize)
{
    m_memory = allocMemory(m_vkd, m_device, m_size, m_memoryTypeIndex);
}

MemoryObject::~MemoryObject(void)
{
    delete m_mapping;
}

MemoryMapping *MemoryObject::mapRandom(const DeviceInterface &vkd, VkDevice device, de::Random &rng, bool map2)
{
    const VkDeviceSize size   = randomSize(rng, m_atomSize, m_size);
    const VkDeviceSize offset = randomOffset(rng, m_atomSize, m_size - size);
    void *ptr;

    DE_ASSERT(!m_mapping);

    mapMemoryWrapper(vkd, device, *m_memory, offset, size, &ptr, map2);
    TCU_CHECK(ptr);
    m_mapping = new MemoryMapping(MemoryRange(offset, size), ptr, m_referenceMemory);

    return m_mapping;
}

void MemoryObject::unmap(bool map2)
{
    unmapMemoryWrapper(m_vkd, m_device, *m_memory, map2);

    delete m_mapping;
    m_mapping = nullptr;
}

void MemoryObject::randomFlush(const DeviceInterface &vkd, VkDevice device, de::Random &rng)
{
    const size_t rangeCount = (size_t)rng.getInt(1, 10);
    vector<VkMappedMemoryRange> ranges(rangeCount);

    randomRanges(rng, ranges, rangeCount, *m_memory, m_mapping->getRange().offset, m_mapping->getRange().size,
                 m_atomSize);

    for (size_t rangeNdx = 0; rangeNdx < ranges.size(); rangeNdx++)
        m_referenceMemory.flush((size_t)ranges[rangeNdx].offset, (size_t)ranges[rangeNdx].size);

    VK_CHECK(vkd.flushMappedMemoryRanges(device, (uint32_t)ranges.size(), ranges.empty() ? nullptr : &ranges[0]));
}

void MemoryObject::randomInvalidate(const DeviceInterface &vkd, VkDevice device, de::Random &rng)
{
    const size_t rangeCount = (size_t)rng.getInt(1, 10);
    vector<VkMappedMemoryRange> ranges(rangeCount);

    randomRanges(rng, ranges, rangeCount, *m_memory, m_mapping->getRange().offset, m_mapping->getRange().size,
                 m_atomSize);

    for (size_t rangeNdx = 0; rangeNdx < ranges.size(); rangeNdx++)
        m_referenceMemory.invalidate((size_t)ranges[rangeNdx].offset, (size_t)ranges[rangeNdx].size);

    VK_CHECK(vkd.invalidateMappedMemoryRanges(device, (uint32_t)ranges.size(), ranges.empty() ? nullptr : &ranges[0]));
}

enum
{
    MAX_MEMORY_USAGE_DIV = 2, // Use only 1/2 of each memory heap.
    MAX_MEMORY_ALLOC_DIV = 2, // Do not alloc more than 1/2 of available space.
};

template <typename T>
void removeFirstEqual(vector<T> &vec, const T &val)
{
    for (size_t ndx = 0; ndx < vec.size(); ndx++)
    {
        if (vec[ndx] == val)
        {
            vec[ndx] = vec.back();
            vec.pop_back();
            return;
        }
    }
}

enum MemoryClass
{
    MEMORY_CLASS_SYSTEM = 0,
    MEMORY_CLASS_DEVICE,

    MEMORY_CLASS_LAST
};

// \todo [2016-04-20 pyry] Consider estimating memory fragmentation
class TotalMemoryTracker
{
public:
    TotalMemoryTracker(void)
    {
        std::fill(DE_ARRAY_BEGIN(m_usage), DE_ARRAY_END(m_usage), 0);
    }

    void allocate(MemoryClass memClass, VkDeviceSize size)
    {
        m_usage[memClass] += size;
    }

    void free(MemoryClass memClass, VkDeviceSize size)
    {
        DE_ASSERT(size <= m_usage[memClass]);
        m_usage[memClass] -= size;
    }

    VkDeviceSize getUsage(MemoryClass memClass) const
    {
        return m_usage[memClass];
    }

    VkDeviceSize getTotalUsage(void) const
    {
        VkDeviceSize total = 0;
        for (int ndx = 0; ndx < MEMORY_CLASS_LAST; ++ndx)
            total += getUsage((MemoryClass)ndx);
        return total;
    }

private:
    VkDeviceSize m_usage[MEMORY_CLASS_LAST];
};

VkDeviceSize getHostPageSize(void)
{
    return 4096;
}

class MemoryHeap
{
public:
    MemoryHeap(const VkMemoryHeap &heap, const vector<MemoryType> &memoryTypes,
               const tcu::PlatformMemoryLimits &memoryLimits, const VkDeviceSize nonCoherentAtomSize,
               TotalMemoryTracker &totalMemTracker)
        : m_heap(heap)
        , m_memoryTypes(memoryTypes)
        , m_limits(memoryLimits)
        , m_nonCoherentAtomSize(nonCoherentAtomSize)
        , m_minAtomSize(nonCoherentAtomSize)
        , m_totalMemTracker(totalMemTracker)
        , m_usage(0)
    {
    }

    ~MemoryHeap(void)
    {
        for (vector<MemoryObject *>::iterator iter = m_objects.begin(); iter != m_objects.end(); ++iter)
            delete *iter;
    }

    bool full(void) const;
    bool empty(void) const
    {
        return m_usage == 0 && !full();
    }

    MemoryObject *allocateRandom(const DeviceInterface &vkd, VkDevice device, de::Random &rng);

    MemoryObject *getRandomObject(de::Random &rng) const
    {
        return rng.choose<MemoryObject *>(m_objects.begin(), m_objects.end());
    }

    void free(MemoryObject *object)
    {
        removeFirstEqual(m_objects, object);
        m_usage -= object->getMemoryUsage();
        m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, object->getReferenceMemoryUsage());
        m_totalMemTracker.free(getMemoryClass(), object->getMemoryUsage());
        delete object;
    }

private:
    MemoryClass getMemoryClass(void) const
    {
        if ((m_heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
            return MEMORY_CLASS_DEVICE;
        else
            return MEMORY_CLASS_SYSTEM;
    }

    const VkMemoryHeap m_heap;
    const vector<MemoryType> m_memoryTypes;
    const tcu::PlatformMemoryLimits &m_limits;
    const VkDeviceSize m_nonCoherentAtomSize;
    const VkDeviceSize m_minAtomSize;
    TotalMemoryTracker &m_totalMemTracker;

    VkDeviceSize m_usage;
    vector<MemoryObject *> m_objects;
};

// Heap is full if there is not enough memory to allocate minimal memory object.
bool MemoryHeap::full(void) const
{
    DE_ASSERT(m_usage <= m_heap.size / MAX_MEMORY_USAGE_DIV);

    const VkDeviceSize availableInHeap = m_heap.size / MAX_MEMORY_USAGE_DIV - m_usage;
    const bool isUMA                   = m_limits.totalDeviceLocalMemory == 0;
    const MemoryClass memClass         = getMemoryClass();
    const VkDeviceSize minAllocationSize =
        de::max(m_minAtomSize, memClass == MEMORY_CLASS_DEVICE ? m_limits.devicePageSize : getHostPageSize());
    // Memory required for reference. One byte and one bit for each byte and one bit per each m_atomSize.
    const VkDeviceSize minReferenceSize = minAllocationSize + divRoundUp<VkDeviceSize>(minAllocationSize, 8) +
                                          divRoundUp<VkDeviceSize>(minAllocationSize, m_minAtomSize * 8);

    if (isUMA)
    {
        const VkDeviceSize totalUsage  = m_totalMemTracker.getTotalUsage();
        const VkDeviceSize totalSysMem = (VkDeviceSize)m_limits.totalSystemMemory;

        DE_ASSERT(totalUsage <= totalSysMem);

        return (minAllocationSize + minReferenceSize) > (totalSysMem - totalUsage) ||
               minAllocationSize > availableInHeap;
    }
    else
    {
        const VkDeviceSize totalUsage  = m_totalMemTracker.getTotalUsage();
        const VkDeviceSize totalSysMem = (VkDeviceSize)m_limits.totalSystemMemory;

        const VkDeviceSize totalMemClass =
            memClass == MEMORY_CLASS_SYSTEM ? m_limits.totalSystemMemory : m_limits.totalDeviceLocalMemory;
        const VkDeviceSize usedMemClass = m_totalMemTracker.getUsage(memClass);

        DE_ASSERT(usedMemClass <= totalMemClass);

        return minAllocationSize > availableInHeap || minAllocationSize > (totalMemClass - usedMemClass) ||
               minReferenceSize > (totalSysMem - totalUsage);
    }
}

MemoryObject *MemoryHeap::allocateRandom(const DeviceInterface &vkd, VkDevice device, de::Random &rng)
{
    pair<MemoryType, VkDeviceSize> memoryTypeMaxSizePair;

    // Pick random memory type
    {
        vector<pair<MemoryType, VkDeviceSize>> memoryTypes;

        const VkDeviceSize availableInHeap = m_heap.size / MAX_MEMORY_USAGE_DIV - m_usage;
        const bool isUMA                   = m_limits.totalDeviceLocalMemory == 0;
        const MemoryClass memClass         = getMemoryClass();

        // Collect memory types that can be allocated and the maximum size of allocation.
        // Memory type can be only allocated if minimal memory allocation is less than available memory.
        for (size_t memoryTypeNdx = 0; memoryTypeNdx < m_memoryTypes.size(); memoryTypeNdx++)
        {
            const MemoryType type       = m_memoryTypes[memoryTypeNdx];
            const VkDeviceSize atomSize = m_nonCoherentAtomSize;
            const VkDeviceSize allocationSizeGranularity =
                de::max(atomSize, memClass == MEMORY_CLASS_DEVICE ? m_limits.devicePageSize : getHostPageSize());
            const VkDeviceSize minAllocationSize = allocationSizeGranularity;
            const VkDeviceSize minReferenceSize  = minAllocationSize + divRoundUp<VkDeviceSize>(minAllocationSize, 8) +
                                                  divRoundUp<VkDeviceSize>(minAllocationSize, atomSize * 8);

            if (isUMA)
            {
                // Max memory size calculation is little tricky since reference memory requires 1/n bits per byte.
                const VkDeviceSize totalUsage    = m_totalMemTracker.getTotalUsage();
                const VkDeviceSize totalSysMem   = (VkDeviceSize)m_limits.totalSystemMemory;
                const VkDeviceSize availableBits = (totalSysMem - totalUsage) * 8;
                // availableBits == maxAllocationSizeBits + maxAllocationReferenceSizeBits
                // maxAllocationReferenceSizeBits == maxAllocationSizeBits + (maxAllocationSizeBits / 8) + (maxAllocationSizeBits / atomSizeBits)
                // availableBits == maxAllocationSizeBits + maxAllocationSizeBits + (maxAllocationSizeBits / 8) + (maxAllocationSizeBits / atomSizeBits)
                // availableBits == 2 * maxAllocationSizeBits + (maxAllocationSizeBits / 8) + (maxAllocationSizeBits / atomSizeBits)
                // availableBits == (2 + 1/8 + 1/atomSizeBits) * maxAllocationSizeBits
                // 8 * availableBits == (16 + 1 + 8/atomSizeBits) * maxAllocationSizeBits
                // atomSizeBits * 8 * availableBits == (17 * atomSizeBits + 8) * maxAllocationSizeBits
                // maxAllocationSizeBits == atomSizeBits * 8 * availableBits / (17 * atomSizeBits + 8)
                // maxAllocationSizeBytes == maxAllocationSizeBits / 8
                // maxAllocationSizeBytes == atomSizeBits * availableBits / (17 * atomSizeBits + 8)
                // atomSizeBits = atomSize * 8
                // maxAllocationSizeBytes == atomSize * 8 * availableBits / (17 * atomSize * 8 + 8)
                // maxAllocationSizeBytes == atomSize * availableBits / (17 * atomSize + 1)
                //
                // Finally, the allocation size must be less than or equal to memory heap size
                const VkDeviceSize maxAllocationSize =
                    roundDownToMultiple(de::min((atomSize * availableBits) / (17 * atomSize + 1), availableInHeap),
                                        allocationSizeGranularity);

                DE_ASSERT(totalUsage <= totalSysMem);
                DE_ASSERT(maxAllocationSize <= totalSysMem);

                if (minAllocationSize + minReferenceSize <= (totalSysMem - totalUsage) &&
                    minAllocationSize <= availableInHeap)
                {
                    DE_ASSERT(maxAllocationSize >= minAllocationSize);
                    memoryTypes.push_back(std::make_pair(type, maxAllocationSize));
                }
            }
            else
            {
                // Max memory size calculation is little tricky since reference memory requires 1/n bits per byte.
                const VkDeviceSize totalUsage  = m_totalMemTracker.getTotalUsage();
                const VkDeviceSize totalSysMem = (VkDeviceSize)m_limits.totalSystemMemory;

                const VkDeviceSize totalMemClass =
                    memClass == MEMORY_CLASS_SYSTEM ? m_limits.totalSystemMemory : m_limits.totalDeviceLocalMemory;
                const VkDeviceSize usedMemClass = m_totalMemTracker.getUsage(memClass);
                // availableRefBits = maxRefBits + maxRefBits/8 + maxRefBits/atomSizeBits
                // availableRefBits = maxRefBits * (1 + 1/8 + 1/atomSizeBits)
                // 8 * availableRefBits = maxRefBits * (8 + 1 + 8/atomSizeBits)
                // 8 * atomSizeBits * availableRefBits = maxRefBits * (9 * atomSizeBits + 8)
                // maxRefBits = 8 * atomSizeBits * availableRefBits / (9 * atomSizeBits + 8)
                // atomSizeBits = atomSize * 8
                // maxRefBits = 8 * atomSize * 8 * availableRefBits / (9 * atomSize * 8 + 8)
                // maxRefBits = atomSize * 8 * availableRefBits / (9 * atomSize + 1)
                // maxRefBytes = atomSize * availableRefBits / (9 * atomSize + 1)
                //
                // Finally, the allocation size must be less than or equal to memory heap size
                const VkDeviceSize maxAllocationSize = roundDownToMultiple(
                    de::min(de::min(totalMemClass - usedMemClass,
                                    (atomSize * 8 * (totalSysMem - totalUsage)) / (9 * atomSize + 1)),
                            availableInHeap),
                    allocationSizeGranularity);

                DE_ASSERT(usedMemClass <= totalMemClass);

                if (minAllocationSize <= availableInHeap && minAllocationSize <= (totalMemClass - usedMemClass) &&
                    minReferenceSize <= (totalSysMem - totalUsage))
                {
                    DE_ASSERT(maxAllocationSize >= minAllocationSize);
                    memoryTypes.push_back(std::make_pair(type, maxAllocationSize));
                }
            }
        }

        memoryTypeMaxSizePair = rng.choose<pair<MemoryType, VkDeviceSize>>(memoryTypes.begin(), memoryTypes.end());
    }

    const MemoryType type                = memoryTypeMaxSizePair.first;
    const VkDeviceSize maxAllocationSize = memoryTypeMaxSizePair.second / MAX_MEMORY_ALLOC_DIV;
    const VkDeviceSize atomSize          = m_nonCoherentAtomSize;
    const VkDeviceSize allocationSizeGranularity =
        de::max(atomSize, getMemoryClass() == MEMORY_CLASS_DEVICE ? m_limits.devicePageSize : getHostPageSize());
    const VkDeviceSize size        = randomSize(rng, atomSize, maxAllocationSize);
    const VkDeviceSize memoryUsage = roundUpToMultiple(size, allocationSizeGranularity);
    const VkDeviceSize referenceMemoryUsage =
        size + divRoundUp<VkDeviceSize>(size, 8) + divRoundUp<VkDeviceSize>(size / atomSize, 8);

    DE_ASSERT(size <= maxAllocationSize);

    MemoryObject *const object =
        new MemoryObject(vkd, device, size, type.index, atomSize, memoryUsage, referenceMemoryUsage);

    m_usage += memoryUsage;
    m_totalMemTracker.allocate(getMemoryClass(), memoryUsage);
    m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, referenceMemoryUsage);
    m_objects.push_back(object);

    return object;
}

size_t getMemoryObjectSystemSize(Context &context)
{
    return computeDeviceMemorySystemMemFootprint(context.getDeviceInterface(), context.getDevice()) +
           sizeof(MemoryObject) + sizeof(de::SharedPtr<MemoryObject>);
}

size_t getMemoryMappingSystemSize(void)
{
    return sizeof(MemoryMapping) + sizeof(de::SharedPtr<MemoryMapping>);
}

struct RandomMappingConfig
{
    uint32_t seed;
    bool memoryMap2;
};

class RandomMemoryMappingInstance : public TestInstance
{
public:
    RandomMemoryMappingInstance(Context &context, const RandomMappingConfig &config)
        : TestInstance(context)
        , m_memoryObjectSysMemSize(getMemoryObjectSystemSize(context))
        , m_memoryMappingSysMemSize(getMemoryMappingSystemSize())
        , m_memoryLimits(tcu::getMemoryLimits(context.getTestContext().getPlatform()))
        , m_rng(config.seed)
        , m_opNdx(0)
        , m_map2(config.memoryMap2)
    {
        const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
        const InstanceInterface &vki          = context.getInstanceInterface();
        const VkPhysicalDeviceMemoryProperties memoryProperties =
            getPhysicalDeviceMemoryProperties(vki, physicalDevice);
        const VkDeviceSize nonCoherentAtomSize = context.getDeviceProperties().limits.nonCoherentAtomSize;

        // Initialize heaps
        {
            vector<vector<MemoryType>> memoryTypes(memoryProperties.memoryHeapCount);

            for (uint32_t memoryTypeNdx = 0; memoryTypeNdx < memoryProperties.memoryTypeCount; memoryTypeNdx++)
            {
                if (memoryProperties.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    memoryTypes[memoryProperties.memoryTypes[memoryTypeNdx].heapIndex].push_back(
                        MemoryType(memoryTypeNdx, memoryProperties.memoryTypes[memoryTypeNdx]));
            }

            for (uint32_t heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; heapIndex++)
            {
                const VkMemoryHeap heapInfo = memoryProperties.memoryHeaps[heapIndex];

                if (!memoryTypes[heapIndex].empty())
                {
                    const de::SharedPtr<MemoryHeap> heap(new MemoryHeap(
                        heapInfo, memoryTypes[heapIndex], m_memoryLimits, nonCoherentAtomSize, m_totalMemTracker));

                    TCU_CHECK_INTERNAL(!heap->full());

                    m_memoryHeaps.push_back(heap);
                }
            }
        }
    }

    ~RandomMemoryMappingInstance(void)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const size_t opCount                   = 100;
        const float memoryOpProbability        = 0.5f;  // 0.50
        const float flushInvalidateProbability = 0.4f;  // 0.20
        const float mapProbability             = 0.50f; // 0.15
        const float unmapProbability           = 0.25f; // 0.075

        const float allocProbability = 0.75f; // Versun free

        const VkDevice device      = m_context.getDevice();
        const DeviceInterface &vkd = m_context.getDeviceInterface();

        const VkDeviceSize sysMemUsage = (m_memoryLimits.totalDeviceLocalMemory == 0) ?
                                             m_totalMemTracker.getTotalUsage() :
                                             m_totalMemTracker.getUsage(MEMORY_CLASS_SYSTEM);

        if (!m_memoryMappings.empty() && m_rng.getFloat() < memoryOpProbability)
        {
            // Perform operations on mapped memory
            MemoryMapping *const mapping =
                m_rng.choose<MemoryMapping *>(m_memoryMappings.begin(), m_memoryMappings.end());

            enum Op
            {
                OP_READ = 0,
                OP_WRITE,
                OP_MODIFY,
                OP_LAST
            };

            const Op op = (Op)(m_rng.getUint32() % OP_LAST);

            switch (op)
            {
            case OP_READ:
                mapping->randomRead(m_rng);
                break;

            case OP_WRITE:
                mapping->randomWrite(m_rng);
                break;

            case OP_MODIFY:
                mapping->randomModify(m_rng);
                break;

            default:
                DE_FATAL("Invalid operation");
            }
        }
        else if (!m_mappedMemoryObjects.empty() && m_rng.getFloat() < flushInvalidateProbability)
        {
            MemoryObject *const object =
                m_rng.choose<MemoryObject *>(m_mappedMemoryObjects.begin(), m_mappedMemoryObjects.end());

            if (m_rng.getBool())
                object->randomFlush(vkd, device, m_rng);
            else
                object->randomInvalidate(vkd, device, m_rng);
        }
        else if (!m_mappedMemoryObjects.empty() && m_rng.getFloat() < unmapProbability)
        {
            // Unmap memory object
            MemoryObject *const object =
                m_rng.choose<MemoryObject *>(m_mappedMemoryObjects.begin(), m_mappedMemoryObjects.end());

            // Remove mapping
            removeFirstEqual(m_memoryMappings, object->getMapping());

            object->unmap(m_map2);
            removeFirstEqual(m_mappedMemoryObjects, object);
            m_nonMappedMemoryObjects.push_back(object);

            m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryMappingSysMemSize);
        }
        else if (!m_nonMappedMemoryObjects.empty() && (m_rng.getFloat() < mapProbability) &&
                 (sysMemUsage + m_memoryMappingSysMemSize <= (VkDeviceSize)m_memoryLimits.totalSystemMemory))
        {
            // Map memory object
            MemoryObject *const object =
                m_rng.choose<MemoryObject *>(m_nonMappedMemoryObjects.begin(), m_nonMappedMemoryObjects.end());
            MemoryMapping *mapping = object->mapRandom(vkd, device, m_rng, m_map2);

            m_memoryMappings.push_back(mapping);
            m_mappedMemoryObjects.push_back(object);
            removeFirstEqual(m_nonMappedMemoryObjects, object);

            m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryMappingSysMemSize);
        }
        else
        {
            // Sort heaps based on capacity (full or not)
            vector<MemoryHeap *> nonFullHeaps;
            vector<MemoryHeap *> nonEmptyHeaps;

            if (sysMemUsage + m_memoryObjectSysMemSize <= (VkDeviceSize)m_memoryLimits.totalSystemMemory)
            {
                // For the duration of sorting reserve MemoryObject space from system memory
                m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);

                for (vector<de::SharedPtr<MemoryHeap>>::const_iterator heapIter = m_memoryHeaps.begin();
                     heapIter != m_memoryHeaps.end(); ++heapIter)
                {
                    if (!(*heapIter)->full())
                        nonFullHeaps.push_back(heapIter->get());

                    if (!(*heapIter)->empty())
                        nonEmptyHeaps.push_back(heapIter->get());
                }

                m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);
            }
            else
            {
                // Not possible to even allocate MemoryObject from system memory, look for non-empty heaps
                for (vector<de::SharedPtr<MemoryHeap>>::const_iterator heapIter = m_memoryHeaps.begin();
                     heapIter != m_memoryHeaps.end(); ++heapIter)
                {
                    if (!(*heapIter)->empty())
                        nonEmptyHeaps.push_back(heapIter->get());
                }
            }

            if (!nonFullHeaps.empty() && (nonEmptyHeaps.empty() || m_rng.getFloat() < allocProbability))
            {
                // Reserve MemoryObject from sys mem first
                m_totalMemTracker.allocate(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);

                // Allocate more memory objects
                MemoryHeap *const heap     = m_rng.choose<MemoryHeap *>(nonFullHeaps.begin(), nonFullHeaps.end());
                MemoryObject *const object = heap->allocateRandom(vkd, device, m_rng);

                m_nonMappedMemoryObjects.push_back(object);
            }
            else
            {
                // Free memory objects
                MemoryHeap *const heap     = m_rng.choose<MemoryHeap *>(nonEmptyHeaps.begin(), nonEmptyHeaps.end());
                MemoryObject *const object = heap->getRandomObject(m_rng);

                // Remove mapping
                if (object->getMapping())
                {
                    removeFirstEqual(m_memoryMappings, object->getMapping());
                    m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, m_memoryMappingSysMemSize);
                }

                removeFirstEqual(m_mappedMemoryObjects, object);
                removeFirstEqual(m_nonMappedMemoryObjects, object);

                heap->free(object);
                m_totalMemTracker.free(MEMORY_CLASS_SYSTEM, (VkDeviceSize)m_memoryObjectSysMemSize);
            }
        }

        m_opNdx += 1;
        if (m_opNdx == opCount)
            return tcu::TestStatus::pass("Pass");
        else
            return tcu::TestStatus::incomplete();
    }

private:
    const size_t m_memoryObjectSysMemSize;
    const size_t m_memoryMappingSysMemSize;
    const tcu::PlatformMemoryLimits m_memoryLimits;

    de::Random m_rng;
    size_t m_opNdx;
    bool m_map2;

    TotalMemoryTracker m_totalMemTracker;
    vector<de::SharedPtr<MemoryHeap>> m_memoryHeaps;

    vector<MemoryObject *> m_mappedMemoryObjects;
    vector<MemoryObject *> m_nonMappedMemoryObjects;
    vector<MemoryMapping *> m_memoryMappings;
};

enum Op
{
    OP_NONE = 0,

    OP_FLUSH,
    OP_SUB_FLUSH,
    OP_SUB_FLUSH_SEPARATE,
    OP_SUB_FLUSH_OVERLAPPING,

    OP_INVALIDATE,
    OP_SUB_INVALIDATE,
    OP_SUB_INVALIDATE_SEPARATE,
    OP_SUB_INVALIDATE_OVERLAPPING,

    OP_REMAP,
    OP_IMPLICIT_UNMAP,

    OP_LAST
};

TestConfig subMappedConfig(VkDeviceSize allocationSize, const MemoryRange &mapping, Op op, uint32_t seed,
                           AllocationKind allocationKind, bool memoryMap2)
{
    TestConfig config;

    config.allocationSize = allocationSize;
    config.seed           = seed;
    config.mapping        = mapping;
    config.remap          = false;
    config.implicitUnmap  = false;
    config.allocationKind = allocationKind;
    config.memoryMap2     = memoryMap2;

    switch (op)
    {
    case OP_NONE:
        break;

    case OP_REMAP:
        config.remap = true;
        break;

    case OP_IMPLICIT_UNMAP:
        config.implicitUnmap = true;
        break;

    case OP_FLUSH:
        config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
        break;

    case OP_SUB_FLUSH:
        DE_ASSERT(mapping.size / 4 > 0);

        config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
        break;

    case OP_SUB_FLUSH_SEPARATE:
        DE_ASSERT(mapping.size / 2 > 0);

        config.flushMappings.push_back(
            MemoryRange(mapping.offset + mapping.size / 2, mapping.size - (mapping.size / 2)));
        config.flushMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

        break;

    case OP_SUB_FLUSH_OVERLAPPING:
        DE_ASSERT((mapping.size / 3) > 0);

        config.flushMappings.push_back(
            MemoryRange(mapping.offset + mapping.size / 3, mapping.size - (mapping.size / 2)));
        config.flushMappings.push_back(MemoryRange(mapping.offset, (2 * mapping.size) / 3));

        break;

    case OP_INVALIDATE:
        config.flushMappings      = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
        config.invalidateMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset, mapping.size));
        break;

    case OP_SUB_INVALIDATE:
        DE_ASSERT(mapping.size / 4 > 0);

        config.flushMappings = vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
        config.invalidateMappings =
            vector<MemoryRange>(1, MemoryRange(mapping.offset + mapping.size / 4, mapping.size / 2));
        break;

    case OP_SUB_INVALIDATE_SEPARATE:
        DE_ASSERT(mapping.size / 2 > 0);

        config.flushMappings.push_back(
            MemoryRange(mapping.offset + mapping.size / 2, mapping.size - (mapping.size / 2)));
        config.flushMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

        config.invalidateMappings.push_back(
            MemoryRange(mapping.offset + mapping.size / 2, mapping.size - (mapping.size / 2)));
        config.invalidateMappings.push_back(MemoryRange(mapping.offset, mapping.size / 2));

        break;

    case OP_SUB_INVALIDATE_OVERLAPPING:
        DE_ASSERT((mapping.size / 3) > 0);

        config.flushMappings.push_back(
            MemoryRange(mapping.offset + mapping.size / 3, mapping.size - (mapping.size / 2)));
        config.flushMappings.push_back(MemoryRange(mapping.offset, (2 * mapping.size) / 3));

        config.invalidateMappings.push_back(
            MemoryRange(mapping.offset + mapping.size / 3, mapping.size - (mapping.size / 2)));
        config.invalidateMappings.push_back(MemoryRange(mapping.offset, (2 * mapping.size) / 3));

        break;

    default:
        DE_FATAL("Unknown Op");
        return TestConfig();
    }
    for (size_t ndx = 0; ndx < config.flushMappings.size(); ndx++)
    {
        if (config.flushMappings[ndx].offset + config.flushMappings[ndx].size > mapping.size)
        {
            config.flushMappings[ndx].size = VK_WHOLE_SIZE;
        }
    }
    for (size_t ndx = 0; ndx < config.invalidateMappings.size(); ndx++)
    {
        if (config.invalidateMappings[ndx].offset + config.invalidateMappings[ndx].size > mapping.size)
        {
            config.invalidateMappings[ndx].size = VK_WHOLE_SIZE;
        }
    }
    return config;
}

TestConfig fullMappedConfig(VkDeviceSize allocationSize, Op op, uint32_t seed, AllocationKind allocationKind,
                            bool memoryMap2)
{
    return subMappedConfig(allocationSize, MemoryRange(0, allocationSize), op, seed, allocationKind, memoryMap2);
}

template <typename T>
void checkMapMemory2Support(Context &context, const T &config)
{
    if (config.memoryMap2)
        context.requireDeviceFunctionality("VK_KHR_map_memory2");
}

void checkSupport(Context &context, TestConfig config)
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");

    if (config.allocationKind == ALLOCATION_KIND_DEDICATED_IMAGE ||
        config.allocationKind == ALLOCATION_KIND_DEDICATED_BUFFER)
    {
        context.requireDeviceFunctionality("VK_KHR_dedicated_allocation");
    }

    checkMapMemory2Support(context, config);
}

void checkSupport(Context &context, RandomMappingConfig config)
{
    checkMapMemory2Support(context, config);
}

} // namespace

tcu::TestCaseGroup *createMappingTests(tcu::TestContext &testCtx)
{
    // Memory mapping tests.
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "mapping"));
    // Dedicated memory mapping tests.
    de::MovePtr<tcu::TestCaseGroup> dedicated(new tcu::TestCaseGroup(testCtx, "dedicated_alloc"));
    de::MovePtr<tcu::TestCaseGroup> sets[] = {
        de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "suballocation")),
        de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "buffer")),
        de::MovePtr<tcu::TestCaseGroup>(new tcu::TestCaseGroup(testCtx, "image"))};

    const VkDeviceSize allocationSizes[] = {
        0, 33, 257, 4087, 8095, 1 * 1024 * 1024 + 1,
    };

    const VkDeviceSize offsets[] = {0, 17, 129, 255, 1025, 32 * 1024 + 1};

    const VkDeviceSize sizes[] = {31, 255, 1025, 4085, 1 * 1024 * 1024 - 1};

    const struct
    {
        const Op op;
        const char *const name;
    } ops[] = {{OP_NONE, "simple"},
               {OP_REMAP, "remap"},
#ifndef CTS_USES_VULKANSC
               // implicit_unmap tests use VkAllocationCallbacks forbidden in Vulkan SC
               {OP_IMPLICIT_UNMAP, "implicit_unmap"},
#endif // CTS_USES_VULKANSC
               {OP_FLUSH, "flush"},
               {OP_SUB_FLUSH, "subflush"},
               {OP_SUB_FLUSH_SEPARATE, "subflush_separate"},
               {OP_SUB_FLUSH_SEPARATE, "subflush_overlapping"},
               {OP_INVALIDATE, "invalidate"},
               {OP_SUB_INVALIDATE, "subinvalidate"},
               {OP_SUB_INVALIDATE_SEPARATE, "subinvalidate_separate"},
               {OP_SUB_INVALIDATE_SEPARATE, "subinvalidate_overlapping"}};

    const struct
    {
        const bool memoryMap2;
        const char *nameSuffix;
    } mapFunctions[] = {
        {false, ""},
        {true, "_map2"},
    };

    // .full
    for (size_t allocationKindNdx = 0; allocationKindNdx < ALLOCATION_KIND_LAST; allocationKindNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> fullGroup(new tcu::TestCaseGroup(testCtx, "full"));

        for (size_t allocationSizeNdx = 0; allocationSizeNdx < DE_LENGTH_OF_ARRAY(allocationSizes); allocationSizeNdx++)
        {
            const VkDeviceSize allocationSize = allocationSizes[allocationSizeNdx];
            const string sizeGroupName        = (allocationSize == 0) ? "variable" : de::toString(allocationSize);
            de::MovePtr<tcu::TestCaseGroup> allocationSizeGroup(new tcu::TestCaseGroup(testCtx, sizeGroupName.c_str()));

            for (size_t opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(ops); opNdx++)
            {
                const Op op = ops[opNdx].op;

                // implicit_unmap ignores allocationSize
                if (((allocationSize == 0) && (op != OP_IMPLICIT_UNMAP)) ||
                    ((allocationSize != 0) && (op == OP_IMPLICIT_UNMAP)))
                    continue;

                for (auto function : mapFunctions)
                {
                    std::string name        = ops[opNdx].name + std::string(function.nameSuffix);
                    const uint32_t seed     = (uint32_t)(opNdx * allocationSizeNdx);
                    const TestConfig config = fullMappedConfig(
                        allocationSize, op, seed, static_cast<AllocationKind>(allocationKindNdx), function.memoryMap2);

                    addFunctionCase(allocationSizeGroup.get(), name, checkSupport, testMemoryMapping, config);
                }
            }

            fullGroup->addChild(allocationSizeGroup.release());
        }

        sets[allocationKindNdx]->addChild(fullGroup.release());
    }

    // .sub
    for (size_t allocationKindNdx = 0; allocationKindNdx < ALLOCATION_KIND_LAST; allocationKindNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(testCtx, "sub"));

        for (size_t allocationSizeNdx = 0; allocationSizeNdx < DE_LENGTH_OF_ARRAY(allocationSizes); allocationSizeNdx++)
        {
            const VkDeviceSize allocationSize = allocationSizes[allocationSizeNdx];
            const string sizeGroupName        = (allocationSize == 0) ? "variable" : de::toString(allocationSize);
            de::MovePtr<tcu::TestCaseGroup> allocationSizeGroup(new tcu::TestCaseGroup(testCtx, sizeGroupName.c_str()));

            for (size_t offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsets); offsetNdx++)
            {
                const VkDeviceSize offset = offsets[offsetNdx];

                if (offset >= allocationSize)
                    continue;

                de::MovePtr<tcu::TestCaseGroup> offsetGroup(
                    new tcu::TestCaseGroup(testCtx, ("offset_" + de::toString(offset)).c_str()));

                for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); sizeNdx++)
                {
                    const VkDeviceSize size = sizes[sizeNdx];

                    if (offset + size > allocationSize)
                        continue;

                    if (offset == 0 && size == allocationSize)
                        continue;

                    de::MovePtr<tcu::TestCaseGroup> sizeGroup(
                        new tcu::TestCaseGroup(testCtx, ("size_" + de::toString(size)).c_str()));

                    for (size_t opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(ops); opNdx++)
                    {
                        const Op op = ops[opNdx].op;

                        // implicit_unmap ignores allocationSize
                        if (((allocationSize == 0) && (op != OP_IMPLICIT_UNMAP)) ||
                            ((allocationSize != 0) && (op == OP_IMPLICIT_UNMAP)))
                            continue;

                        const uint32_t seed = (uint32_t)(opNdx * allocationSizeNdx);

                        for (auto function : mapFunctions)
                        {
                            std::string name = ops[opNdx].name + std::string(function.nameSuffix);
                            const TestConfig config =
                                subMappedConfig(allocationSize, MemoryRange(offset, size), op, seed,
                                                static_cast<AllocationKind>(allocationKindNdx), function.memoryMap2);

                            addFunctionCase(sizeGroup.get(), name, checkSupport, testMemoryMapping, config);
                        }
                    }

                    offsetGroup->addChild(sizeGroup.release());
                }

                allocationSizeGroup->addChild(offsetGroup.release());
            }

            subGroup->addChild(allocationSizeGroup.release());
        }

        sets[allocationKindNdx]->addChild(subGroup.release());
    }

    // .random
    {
        de::MovePtr<tcu::TestCaseGroup> randomGroup(new tcu::TestCaseGroup(testCtx, "random"));
        de::Random rng(3927960301u);
        for (size_t ndx = 0; ndx < 100; ndx++)
        {
            const uint32_t seed = rng.getUint32();

            for (auto function : mapFunctions)
            {
                std::string name                 = de::toString(ndx) + std::string(function.nameSuffix);
                const RandomMappingConfig config = {seed, function.memoryMap2};
                // Random case
                randomGroup->addChild(new InstanceFactory1WithSupport<RandomMemoryMappingInstance, RandomMappingConfig,
                                                                      FunctionSupport1<RandomMappingConfig>>(
                    testCtx, name, config, typename FunctionSupport1<RandomMappingConfig>::Args(checkSupport, config)));
            }
        }

        sets[static_cast<uint32_t>(ALLOCATION_KIND_SUBALLOCATED)]->addChild(randomGroup.release());
    }

    group->addChild(sets[0].release());
    dedicated->addChild(sets[1].release());
    dedicated->addChild(sets[2].release());
    group->addChild(dedicated.release());

    return group.release();
}

} // namespace memory
} // namespace vkt
