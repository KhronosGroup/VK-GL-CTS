/*------------------------------------------------------------------------
 *  Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffers Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferTests.hpp"
#include "gluVarType.hpp"
#include "deStringUtil.hpp"
#include "tcuTestLog.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "tcuPlatform.hpp"

#include <algorithm>
#include <limits>

namespace vkt
{
namespace api
{
namespace
{
using namespace vk;

enum AllocationKind
{
    ALLOCATION_KIND_SUBALLOCATED = 0,
    ALLOCATION_KIND_DEDICATED,

    ALLOCATION_KIND_LAST,
};

tcu::PlatformMemoryLimits getPlatformMemoryLimits(Context &context)
{
    tcu::PlatformMemoryLimits memoryLimits;

    context.getTestContext().getPlatform().getMemoryLimits(memoryLimits);

    return memoryLimits;
}

VkDeviceSize getMaxBufferSize(const VkDeviceSize &bufferSize, const VkDeviceSize &alignment,
                              const tcu::PlatformMemoryLimits &limits)
{
    VkDeviceSize size = bufferSize;

    if (limits.totalDeviceLocalMemory == 0)
    {
        // 'UMA' systems where device memory counts against system memory
        size = std::min(bufferSize, limits.totalSystemMemory - alignment);
    }
    else
    {
        // 'LMA' systems where device memory is local to the GPU
        size = std::min(bufferSize, limits.totalDeviceLocalMemory - alignment);
    }

    return size;
}

struct BufferCaseParameters
{
    VkBufferUsageFlags usage;
    VkBufferCreateFlags flags;
    VkSharingMode sharingMode;
};

class BufferTestInstance : public TestInstance
{
public:
    BufferTestInstance(Context &ctx, BufferCaseParameters testCase) : TestInstance(ctx), m_testCase(testCase)
    {
    }
    virtual tcu::TestStatus iterate(void);
    virtual tcu::TestStatus bufferCreateAndAllocTest(VkDeviceSize size);

protected:
    BufferCaseParameters m_testCase;
};

class DedicatedAllocationBufferTestInstance : public BufferTestInstance
{
public:
    DedicatedAllocationBufferTestInstance(Context &ctx, BufferCaseParameters testCase)
        : BufferTestInstance(ctx, testCase)
    {
    }
    virtual tcu::TestStatus bufferCreateAndAllocTest(VkDeviceSize size);
};

class BuffersTestCase : public TestCase
{
public:
    BuffersTestCase(tcu::TestContext &testCtx, const std::string &name, BufferCaseParameters testCase)
        : TestCase(testCtx, name)
        , m_testCase(testCase)
    {
    }

    virtual ~BuffersTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &ctx) const
    {
        tcu::TestLog &log = m_testCtx.getLog();
        log << tcu::TestLog::Message << getBufferUsageFlagsStr(m_testCase.usage) << tcu::TestLog::EndMessage;
        return new BufferTestInstance(ctx, m_testCase);
    }

    virtual void checkSupport(Context &ctx) const
    {
        const VkPhysicalDeviceFeatures &physicalDeviceFeatures =
            getPhysicalDeviceFeatures(ctx.getInstanceInterface(), ctx.getPhysicalDevice());

        if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) && !physicalDeviceFeatures.sparseBinding)
            TCU_THROW(NotSupportedError, "Sparse bindings feature is not supported");

        if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT) && !physicalDeviceFeatures.sparseResidencyBuffer)
            TCU_THROW(NotSupportedError, "Sparse buffer residency feature is not supported");

        if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT) && !physicalDeviceFeatures.sparseResidencyAliased)
            TCU_THROW(NotSupportedError, "Sparse aliased residency feature is not supported");
    }

private:
    BufferCaseParameters m_testCase;
};

class DedicatedAllocationBuffersTestCase : public TestCase
{
public:
    DedicatedAllocationBuffersTestCase(tcu::TestContext &testCtx, const std::string &name,
                                       BufferCaseParameters testCase)
        : TestCase(testCtx, name)
        , m_testCase(testCase)
    {
    }

    virtual ~DedicatedAllocationBuffersTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &ctx) const
    {
        tcu::TestLog &log = m_testCtx.getLog();
        log << tcu::TestLog::Message << getBufferUsageFlagsStr(m_testCase.usage) << tcu::TestLog::EndMessage;
        return new DedicatedAllocationBufferTestInstance(ctx, m_testCase);
    }

    virtual void checkSupport(Context &ctx) const
    {
        if (!ctx.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
            TCU_THROW(NotSupportedError, "Not supported");
    }

private:
    BufferCaseParameters m_testCase;
};

tcu::TestStatus BufferTestInstance::bufferCreateAndAllocTest(VkDeviceSize size)
{
    const VkPhysicalDevice vkPhysicalDevice = m_context.getPhysicalDevice();
    const InstanceInterface &vkInstance     = m_context.getInstanceInterface();
    const VkDevice vkDevice                 = m_context.getDevice();
    const DeviceInterface &vk               = m_context.getDeviceInterface();
    const uint32_t queueFamilyIndex         = m_context.getSparseQueueFamilyIndex();
    const VkPhysicalDeviceMemoryProperties memoryProperties =
        getPhysicalDeviceMemoryProperties(vkInstance, vkPhysicalDevice);
    const VkPhysicalDeviceLimits limits = getPhysicalDeviceProperties(vkInstance, vkPhysicalDevice).limits;
    Move<VkBuffer> buffer;
    Move<VkDeviceMemory> memory;
    VkMemoryRequirements memReqs;

    if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
    {
        size = std::min(size, limits.sparseAddressSpaceSize);
    }

    // Create the test buffer and a memory allocation for it
    {
        // Create a minimal buffer first to get the supported memory types
        VkBufferCreateInfo bufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            m_testCase.flags,                     // VkBufferCreateFlags flags;
            1u,                                   // VkDeviceSize size;
            m_testCase.usage,                     // VkBufferUsageFlags usage;
            m_testCase.sharingMode,               // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,                    // const uint32_t* pQueueFamilyIndices;
        };

        buffer = createBuffer(vk, vkDevice, &bufferParams);
        vk.getBufferMemoryRequirements(vkDevice, *buffer, &memReqs);

        const uint32_t heapTypeIndex  = (uint32_t)deCtz32(memReqs.memoryTypeBits);
        const VkMemoryType memoryType = memoryProperties.memoryTypes[heapTypeIndex];
        const VkMemoryHeap memoryHeap = memoryProperties.memoryHeaps[memoryType.heapIndex];
        const uint32_t shrinkBits     = 4u; // number of bits to shift when reducing the size with each iteration

        // Buffer size - Choose half of the reported heap size for the maximum buffer size, we
        // should attempt to test as large a portion as possible.
        //
        // However on a system where device memory is shared with the system, the maximum size
        // should be tested against the platform memory limits as significant portion of the heap
        // may already be in use by the operating system and other running processes.
        const VkDeviceSize availableBufferSize =
            getMaxBufferSize(memoryHeap.size, memReqs.alignment, getPlatformMemoryLimits(m_context));

        // For our test buffer size, halve the maximum available size and align
        const VkDeviceSize maxBufferSize = deAlign64(availableBufferSize >> 1, memReqs.alignment);

        size = std::min(size, maxBufferSize);

        while (*memory == VK_NULL_HANDLE)
        {
            // Create the buffer
            {
                VkResult result    = VK_ERROR_OUT_OF_HOST_MEMORY;
                VkBuffer rawBuffer = VK_NULL_HANDLE;

                bufferParams.size = size;
                buffer            = Move<VkBuffer>(); // free the previous buffer, if any
                result            = vk.createBuffer(vkDevice, &bufferParams, nullptr, &rawBuffer);

                if (result != VK_SUCCESS)
                {
                    size = deAlign64(size >> shrinkBits, memReqs.alignment);

                    if (size == 0 || bufferParams.size == memReqs.alignment)
                    {
                        return tcu::TestStatus::fail("Buffer creation failed! (" + de::toString(getResultName(result)) +
                                                     ")");
                    }

                    continue; // didn't work, try with a smaller buffer
                }

                buffer = Move<VkBuffer>(check<VkBuffer>(rawBuffer), Deleter<VkBuffer>(vk, vkDevice, nullptr));
            }

            vk.getBufferMemoryRequirements(vkDevice, *buffer, &memReqs); // get the proper size requirement

#ifdef CTS_USES_VULKANSC
            if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
            {
                if (size > memReqs.size)
                {
                    std::ostringstream errorMsg;
                    errorMsg << "Required memory size (" << memReqs.size << " bytes) smaller than the buffer's size ("
                             << size << " bytes)!";
                    return tcu::TestStatus::fail(errorMsg.str());
                }
            }

            // Allocate the memory
            {
                VkResult result          = VK_ERROR_OUT_OF_HOST_MEMORY;
                VkDeviceMemory rawMemory = VK_NULL_HANDLE;

                const VkMemoryAllocateInfo memAlloc = {
                    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType sType;
                    NULL,                                   // const void* pNext;
                    memReqs.size,                           // VkDeviceSize allocationSize;
                    heapTypeIndex,                          // uint32_t memoryTypeIndex;
                };

                result = vk.allocateMemory(vkDevice, &memAlloc, nullptr, &rawMemory);

                if (result != VK_SUCCESS)
                {
                    size = deAlign64(size >> shrinkBits, memReqs.alignment);

                    if (size == 0 || memReqs.size == memReqs.alignment)
                    {
                        return tcu::TestStatus::fail("Unable to allocate " + de::toString(memReqs.size) +
                                                     " bytes of memory");
                    }

                    continue; // didn't work, try with a smaller allocation (and a smaller buffer)
                }

                memory = Move<VkDeviceMemory>(check<VkDeviceMemory>(rawMemory),
                                              Deleter<VkDeviceMemory>(vk, vkDevice, nullptr));
            }
        } // while
    }

    // Bind the memory
#ifndef CTS_USES_VULKANSC
    if ((m_testCase.flags & (VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT |
                             VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)) != 0)
    {
        const VkQueue queue = m_context.getSparseQueue();

        const VkSparseMemoryBind sparseMemoryBind = {
            0,            // VkDeviceSize resourceOffset;
            memReqs.size, // VkDeviceSize size;
            *memory,      // VkDeviceMemory memory;
            0,            // VkDeviceSize memoryOffset;
            0             // VkSparseMemoryBindFlags flags;
        };

        const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo = {
            *buffer,          // VkBuffer buffer;
            1u,               // uint32_t bindCount;
            &sparseMemoryBind // const VkSparseMemoryBind* pBinds;
        };

        const VkBindSparseInfo bindSparseInfo = {
            VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, // VkStructureType sType;
            nullptr,                            // const void* pNext;
            0,                                  // uint32_t waitSemaphoreCount;
            nullptr,                            // const VkSemaphore* pWaitSemaphores;
            1u,                                 // uint32_t bufferBindCount;
            &sparseBufferMemoryBindInfo,        // const VkSparseBufferMemoryBindInfo* pBufferBinds;
            0,                                  // uint32_t imageOpaqueBindCount;
            nullptr,                            // const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
            0,                                  // uint32_t imageBindCount;
            nullptr,                            // const VkSparseImageMemoryBindInfo* pImageBinds;
            0,                                  // uint32_t signalSemaphoreCount;
            nullptr,                            // const VkSemaphore* pSignalSemaphores;
        };

        const vk::Unique<vk::VkFence> fence(vk::createFence(vk, vkDevice));

        if (vk.queueBindSparse(queue, 1, &bindSparseInfo, *fence) != VK_SUCCESS)
            return tcu::TestStatus::fail(
                "Bind sparse buffer memory failed! (requested memory size: " + de::toString(size) + ")");

        VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, ~(0ull) /* infinity */));
    }
    else if (vk.bindBufferMemory(vkDevice, *buffer, *memory, 0) != VK_SUCCESS)
        return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");
#else
    if (vk.bindBufferMemory(vkDevice, *buffer, *memory, 0) != VK_SUCCESS)
        return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");
#endif // CTS_USES_VULKANSC

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus BufferTestInstance::iterate(void)
{
    const VkDeviceSize testSizes[] = {
        1,     1181, 15991, 16384,
#ifndef CTS_USES_VULKANSC
        ~0ull, // try to exercise a very large buffer too (will be clamped to a sensible size later)
#endif         // CTS_USES_VULKANSC
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(testSizes); ++i)
    {
        const tcu::TestStatus testStatus = bufferCreateAndAllocTest(testSizes[i]);

        if (testStatus.getCode() != QP_TEST_RESULT_PASS)
            return testStatus;
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus DedicatedAllocationBufferTestInstance::bufferCreateAndAllocTest(VkDeviceSize size)
{
    const VkPhysicalDevice vkPhysicalDevice = m_context.getPhysicalDevice();
    const InstanceInterface &vkInstance     = m_context.getInstanceInterface();
    const VkDevice vkDevice                 = m_context.getDevice();
    const DeviceInterface &vk               = m_context.getDeviceInterface();
    const uint32_t queueFamilyIndex         = m_context.getUniversalQueueFamilyIndex();
    const VkPhysicalDeviceMemoryProperties memoryProperties =
        getPhysicalDeviceMemoryProperties(vkInstance, vkPhysicalDevice);
    const VkPhysicalDeviceLimits limits = getPhysicalDeviceProperties(vkInstance, vkPhysicalDevice).limits;

    VkMemoryDedicatedRequirements dedicatedRequirements = {
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        false,                                           // VkBool32                    prefersDedicatedAllocation
        false                                            // VkBool32                    requiresDedicatedAllocation
    };
    VkMemoryRequirements2 memReqs = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, // VkStructureType            sType
        &dedicatedRequirements,                  // void*                    pNext
        {0, 0, 0}                                // VkMemoryRequirements        memoryRequirements
    };

    if ((m_testCase.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
        size = std::min(size, limits.sparseAddressSpaceSize);

    // Create a minimal buffer first to get the supported memory types
    VkBufferCreateInfo bufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType            sType
        nullptr,                              // const void*                pNext
        m_testCase.flags,                     // VkBufferCreateFlags        flags
        1u,                                   // VkDeviceSize                size
        m_testCase.usage,                     // VkBufferUsageFlags        usage
        m_testCase.sharingMode,               // VkSharingMode            sharingMode
        1u,                                   // uint32_t                    queueFamilyIndexCount
        &queueFamilyIndex,                    // const uint32_t*            pQueueFamilyIndices
    };

    Move<VkBuffer> buffer = createBuffer(vk, vkDevice, &bufferParams);

    VkBufferMemoryRequirementsInfo2 info = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, // VkStructureType            sType
        nullptr,                                             // const void*                pNext
        *buffer                                              // VkBuffer                    buffer
    };

    vk.getBufferMemoryRequirements2(vkDevice, &info, &memReqs);

    if (dedicatedRequirements.requiresDedicatedAllocation == VK_TRUE)
    {
        std::ostringstream errorMsg;
        errorMsg << "Nonexternal objects cannot require dedicated allocation.";
        return tcu::TestStatus::fail(errorMsg.str());
    }

    if (memReqs.memoryRequirements.memoryTypeBits == 0)
        return tcu::TestStatus::fail("memoryTypeBits is 0");

    const uint32_t heapTypeIndex  = static_cast<uint32_t>(deCtz32(memReqs.memoryRequirements.memoryTypeBits));
    const VkMemoryType memoryType = memoryProperties.memoryTypes[heapTypeIndex];
    const VkMemoryHeap memoryHeap = memoryProperties.memoryHeaps[memoryType.heapIndex];
    const uint32_t shrinkBits     = 4u; // number of bits to shift when reducing the size with each iteration

    // Buffer size - Choose half of the reported heap size for the maximum buffer size, we
    // should attempt to test as large a portion as possible.
    //
    // However on a system where device memory is shared with the system, the maximum size
    // should be tested against the platform memory limits as a significant portion of the heap
    // may already be in use by the operating system and other running processes.
    const VkDeviceSize maxBufferSize =
        getMaxBufferSize(memoryHeap.size, memReqs.memoryRequirements.alignment, getPlatformMemoryLimits(m_context));

    Move<VkDeviceMemory> memory;
    size = deAlign64(std::min(size, maxBufferSize >> 1), memReqs.memoryRequirements.alignment);
    while (*memory == VK_NULL_HANDLE)
    {
        // Create the buffer
        {
            VkResult result    = VK_ERROR_OUT_OF_HOST_MEMORY;
            VkBuffer rawBuffer = VK_NULL_HANDLE;

            bufferParams.size = size;
            buffer            = Move<VkBuffer>(); // free the previous buffer, if any
            result            = vk.createBuffer(vkDevice, &bufferParams, nullptr, &rawBuffer);

            if (result != VK_SUCCESS)
            {
                size = deAlign64(size >> shrinkBits, memReqs.memoryRequirements.alignment);

                if (size == 0 || bufferParams.size == memReqs.memoryRequirements.alignment)
                    return tcu::TestStatus::fail("Buffer creation failed! (" + de::toString(getResultName(result)) +
                                                 ")");

                continue; // didn't work, try with a smaller buffer
            }

            buffer = Move<VkBuffer>(check<VkBuffer>(rawBuffer), Deleter<VkBuffer>(vk, vkDevice, nullptr));
        }

        info.buffer = *buffer;
        vk.getBufferMemoryRequirements2(vkDevice, &info, &memReqs); // get the proper size requirement

        if (size > memReqs.memoryRequirements.size)
        {
            std::ostringstream errorMsg;
            errorMsg << "Requied memory size (" << memReqs.memoryRequirements.size
                     << " bytes) smaller than the buffer's size (" << size << " bytes)!";
            return tcu::TestStatus::fail(errorMsg.str());
        }

        // Allocate the memory
        {
            VkResult result          = VK_ERROR_OUT_OF_HOST_MEMORY;
            VkDeviceMemory rawMemory = VK_NULL_HANDLE;

            vk::VkMemoryDedicatedAllocateInfo dedicatedInfo = {
                VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, // VkStructureType            sType
                nullptr,                                          // const void*                pNext
                VK_NULL_HANDLE,                                   // VkImage                    image
                *buffer                                           // VkBuffer                    buffer
            };

            VkMemoryAllocateInfo memoryAllocateInfo = {
                VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // VkStructureType            sType
                &dedicatedInfo,                         // const void*                pNext
                memReqs.memoryRequirements.size,        // VkDeviceSize                allocationSize
                heapTypeIndex,                          // uint32_t                    memoryTypeIndex
            };

            result = vk.allocateMemory(vkDevice, &memoryAllocateInfo, nullptr, &rawMemory);

            if (result != VK_SUCCESS)
            {
                size = deAlign64(size >> shrinkBits, memReqs.memoryRequirements.alignment);

                if (size == 0 || memReqs.memoryRequirements.size == memReqs.memoryRequirements.alignment)
                    return tcu::TestStatus::fail("Unable to allocate " + de::toString(memReqs.memoryRequirements.size) +
                                                 " bytes of memory");

                continue; // didn't work, try with a smaller allocation (and a smaller buffer)
            }

            memory =
                Move<VkDeviceMemory>(check<VkDeviceMemory>(rawMemory), Deleter<VkDeviceMemory>(vk, vkDevice, nullptr));
        }
    } // while

    if (vk.bindBufferMemory(vkDevice, *buffer, *memory, 0) != VK_SUCCESS)
        return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");

    return tcu::TestStatus::pass("Pass");
}

std::string getBufferUsageFlagsName(const VkBufferUsageFlags flags)
{
    switch (flags)
    {
    case VK_BUFFER_USAGE_TRANSFER_SRC_BIT:
        return "transfer_src";
    case VK_BUFFER_USAGE_TRANSFER_DST_BIT:
        return "transfer_dst";
    case VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT:
        return "uniform_texel";
    case VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT:
        return "storage_texel";
    case VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT:
        return "uniform";
    case VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:
        return "storage";
    case VK_BUFFER_USAGE_INDEX_BUFFER_BIT:
        return "index";
    case VK_BUFFER_USAGE_VERTEX_BUFFER_BIT:
        return "vertex";
    case VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT:
        return "indirect";
    default:
        DE_FATAL("Unknown buffer usage flag");
        return "";
    }
}

std::string getBufferCreateFlagsName(const VkBufferCreateFlags flags)
{
    std::ostringstream name;

    if (flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
        name << "_binding";
    if (flags & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT)
        name << "_residency";
    if (flags & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
        name << "_aliased";
    if (flags == 0u)
        name << "_zero";

    DE_ASSERT(!name.str().empty());

    return name.str().substr(1);
}

// Create all VkBufferUsageFlags combinations recursively
void createBufferUsageCases(tcu::TestCaseGroup &testGroup, const uint32_t firstNdx, const uint32_t bufferUsageFlags,
                            const AllocationKind allocationKind)
{
    const VkBufferUsageFlags bufferUsageModes[] = {
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};

    tcu::TestContext &testCtx = testGroup.getTestContext();

    // Add test groups
    for (uint32_t currentNdx = firstNdx; currentNdx < DE_LENGTH_OF_ARRAY(bufferUsageModes); currentNdx++)
    {
        const uint32_t newBufferUsageFlags = bufferUsageFlags | bufferUsageModes[currentNdx];
        const std::string newGroupName     = getBufferUsageFlagsName(bufferUsageModes[currentNdx]);
        de::MovePtr<tcu::TestCaseGroup> newTestGroup(new tcu::TestCaseGroup(testCtx, newGroupName.c_str()));

        createBufferUsageCases(*newTestGroup, currentNdx + 1u, newBufferUsageFlags, allocationKind);
        testGroup.addChild(newTestGroup.release());
    }

    // Add test cases
    if (bufferUsageFlags != 0u)
    {
        // \note SPARSE_RESIDENCY and SPARSE_ALIASED have to be used together with the SPARSE_BINDING flag.
        const VkBufferCreateFlags bufferCreateFlags[] = {
            0,
#ifndef CTS_USES_VULKANSC
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT |
                VK_BUFFER_CREATE_SPARSE_ALIASED_BIT,
#endif // CTS_USES_VULKANSC
        };

        // Dedicated allocation does not support sparse feature
        const int numBufferCreateFlags =
            (allocationKind == ALLOCATION_KIND_SUBALLOCATED) ? DE_LENGTH_OF_ARRAY(bufferCreateFlags) : 1;

        de::MovePtr<tcu::TestCaseGroup> newTestGroup(new tcu::TestCaseGroup(testCtx, "create"));

        for (int bufferCreateFlagsNdx = 0; bufferCreateFlagsNdx < numBufferCreateFlags; bufferCreateFlagsNdx++)
        {
            const BufferCaseParameters testParams = {bufferUsageFlags, bufferCreateFlags[bufferCreateFlagsNdx],
                                                     VK_SHARING_MODE_EXCLUSIVE};

            const std::string allocStr =
                (allocationKind == ALLOCATION_KIND_SUBALLOCATED) ? "suballocation of " : "dedicated alloc. of ";
            const std::string caseName = getBufferCreateFlagsName(bufferCreateFlags[bufferCreateFlagsNdx]);

            switch (allocationKind)
            {
            case ALLOCATION_KIND_SUBALLOCATED:
                newTestGroup->addChild(new BuffersTestCase(testCtx, caseName.c_str(), testParams));
                break;
            case ALLOCATION_KIND_DEDICATED:
                newTestGroup->addChild(new DedicatedAllocationBuffersTestCase(testCtx, caseName.c_str(), testParams));
                break;
            default:
                DE_FATAL("Unknown test type");
            }
        }
        testGroup.addChild(newTestGroup.release());
    }
}

tcu::TestStatus testDepthStencilBufferFeatures(Context &context, VkFormat format)
{
    const InstanceInterface &vki    = context.getInstanceInterface();
    VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    VkFormatProperties formatProperties;

    vki.getPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

    if (formatProperties.bufferFeatures == 0x0)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("Fail");
}

struct LargeBufferParameters
{
    uint64_t bufferSize;
    bool useMaxBufferSize;
    VkBufferCreateFlags flags;
};

#ifndef CTS_USES_VULKANSC
tcu::TestStatus testLargeBuffer(Context &context, LargeBufferParameters params)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice vkDevice         = context.getDevice();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    const VkPhysicalDeviceLimits limits =
        getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits;
    VkBuffer rawBuffer = VK_NULL_HANDLE;

#ifndef CTS_USES_VULKANSC
    if (params.useMaxBufferSize)
        params.bufferSize = context.getMaintenance4Properties().maxBufferSize;
#endif // CTS_USES_VULKANSC

    if ((params.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0)
        params.bufferSize = std::min(params.bufferSize, limits.sparseAddressSpaceSize);

    VkBufferCreateInfo bufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                              // const void* pNext;
        params.flags,                         // VkBufferCreateFlags flags;
        params.bufferSize,                    // VkDeviceSize size;
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        1u,                                   // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                    // const uint32_t* pQueueFamilyIndices;
    };

    VkResult result = vk.createBuffer(vkDevice, &bufferParams, nullptr, &rawBuffer);

    // if buffer creation succeeds verify that the correct amount of memory was bound to it
    if (result == VK_SUCCESS)
    {
        VkMemoryRequirements memoryRequirements;
        vk.getBufferMemoryRequirements(vkDevice, rawBuffer, &memoryRequirements);
        vk.destroyBuffer(vkDevice, rawBuffer, nullptr);

        if (memoryRequirements.size >= params.bufferSize)
            return tcu::TestStatus::pass("Pass");
        return tcu::TestStatus::fail("Fail");
    }

    // check if one of the allowed errors was returned
    if ((result == VK_ERROR_OUT_OF_DEVICE_MEMORY) || (result == VK_ERROR_OUT_OF_HOST_MEMORY))
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
void checkMaintenance4Support(Context &context, LargeBufferParameters params)
{
    if (params.useMaxBufferSize)
        context.requireDeviceFunctionality("VK_KHR_maintenance4");
    else if (context.isDeviceFunctionalitySupported("VK_KHR_maintenance4") &&
             params.bufferSize > context.getMaintenance4Properties().maxBufferSize)
        TCU_THROW(NotSupportedError, "vkCreateBuffer with a size larger than maxBufferSize is not valid usage");

    const VkPhysicalDeviceFeatures &physicalDeviceFeatures =
        getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice());
    if ((params.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) && !physicalDeviceFeatures.sparseBinding)
        TCU_THROW(NotSupportedError, "Sparse bindings feature is not supported");
}
#endif // CTS_USES_VULKANSC

} // namespace

tcu::TestCaseGroup *createBufferTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> buffersTests(new tcu::TestCaseGroup(testCtx, "buffer"));

    {
        de::MovePtr<tcu::TestCaseGroup> regularAllocation(new tcu::TestCaseGroup(testCtx, "suballocation"));
        createBufferUsageCases(*regularAllocation, 0u, 0u, ALLOCATION_KIND_SUBALLOCATED);
        buffersTests->addChild(regularAllocation.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> dedicatedAllocation(new tcu::TestCaseGroup(testCtx, "dedicated_alloc"));
        createBufferUsageCases(*dedicatedAllocation, 0u, 0u, ALLOCATION_KIND_DEDICATED);
        buffersTests->addChild(dedicatedAllocation.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "basic"));
#ifndef CTS_USES_VULKANSC
        // Creating buffer using maxBufferSize limit.
        addFunctionCase(basicTests.get(), "max_size", checkMaintenance4Support, testLargeBuffer,
                        LargeBufferParameters{0u, true, 0u});
        // Creating sparse buffer using maxBufferSize limit.
        addFunctionCase(basicTests.get(), "max_size_sparse", checkMaintenance4Support, testLargeBuffer,
                        LargeBufferParameters{0u, true, VK_BUFFER_CREATE_SPARSE_BINDING_BIT});
        // Creating a ULLONG_MAX buffer and verify that it either succeeds or returns one of the allowed errors.
        addFunctionCase(basicTests.get(), "size_max_uint64", checkMaintenance4Support, testLargeBuffer,
                        LargeBufferParameters{std::numeric_limits<uint64_t>::max(), false, 0u});
#endif // CTS_USES_VULKANSC
        buffersTests->addChild(basicTests.release());
    }

    {
        static const VkFormat dsFormats[] = {VK_FORMAT_S8_UINT,
                                             VK_FORMAT_D16_UNORM,
                                             VK_FORMAT_D16_UNORM_S8_UINT,
                                             VK_FORMAT_D24_UNORM_S8_UINT,
                                             VK_FORMAT_D32_SFLOAT,
                                             VK_FORMAT_D32_SFLOAT_S8_UINT,
                                             VK_FORMAT_X8_D24_UNORM_PACK32};

        // Checks that drivers are not exposing undesired format features for depth/stencil formats.
        de::MovePtr<tcu::TestCaseGroup> invalidBufferFeatures(
            new tcu::TestCaseGroup(testCtx, "invalid_buffer_features"));

        for (const auto &testFormat : dsFormats)
        {
            std::string formatName = de::toLower(getFormatName(testFormat));

            addFunctionCase(invalidBufferFeatures.get(), formatName, testDepthStencilBufferFeatures, testFormat);
        }

        buffersTests->addChild(invalidBufferFeatures.release());
    }

    return buffersTests.release();
}

} // namespace api
} // namespace vkt
