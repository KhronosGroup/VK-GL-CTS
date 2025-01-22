/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Dedicated tests for VK_KHR_device_address_commands
 *//*--------------------------------------------------------------------*/

#include "vktApiDeviceAddressCommandsTests.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"

namespace vkt::api
{
namespace
{
using namespace vk;

enum class CopyFlagTestMode
{
    // Copy to/from sparsely bound memory, with some unbound ranges of memory, using the SPARSE flag
    COPY_TO_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT,
    COPY_FROM_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT,

    // Copy to/from memory allocated in a sparse buffer, but fully bound to disjoint memory, without the SPARSE copy flag
    COPY_TO_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT,
    COPY_FROM_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT,

    // Copy to/from memory allocated in a non-sparse buffer, using the SPARSE flag
    COPY_TO_NON_SPARSE_BUFFER_USING_SPARSE_BIT,

    // Copy to memory allocated in a DEVICE_LOCAL heap without the DEVICE_LOCAL flag
    COPY_TO_NON_SPARSE_BUFFER_USING_PROTECTED_BIT,

    // Copy to/from memory allocated in a non-DEVICE_LOCAL heap with and without the DEVICE_LOCAL flag
    COPY_NON_LOCAL_MEMORY_USING_DEVICE_LOCAL_BIT,
    COPY_NON_LOCAL_MEMORY_USING_SPARSE_BIT,
};

struct TestParams
{
    CopyFlagTestMode mode;
};

class BufferAddressCopyFlags : public vkt::TestInstance
{
public:
    using AllocVect = std::vector<de::MovePtr<Allocation>>;

    BufferAddressCopyFlags(Context &context, const TestParams &params);

    tcu::TestStatus iterate(void) override;

private:
    Move<VkBuffer> constructBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBufferCreateFlags createFlag) const;
    void bindBufferMemory(VkBuffer buffer, VkDeviceSize size, AllocVect &allocVect, bool useSparse);

private:
    TestParams m_params;

    VkBufferCreateFlags m_srcBufferCreateFlag;
    VkBufferCreateFlags m_dstBufferCreateFlag;
    VkDeviceSize m_srcBufferSize;
    VkDeviceSize m_dstBufferSize;
    VkAddressCopyFlagsKHR m_srcCopyFlag;
    VkAddressCopyFlagsKHR m_dstCopyFlag;

    VkBufferCopy m_copyRegion;

    bool m_useSingleChunk;
    bool m_replaceSrcOffsetWithChunkSize;
    bool m_replaceDstOffsetWithChunkSize;

    VkDeviceSize m_chunkSize;
    AllocVect m_srcBufferAllocVect;
    AllocVect m_dstBufferAllocVect;
};

BufferAddressCopyFlags::BufferAddressCopyFlags(Context &context, const TestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
    , m_srcBufferCreateFlag(0)
    , m_dstBufferCreateFlag(0)
    , m_srcBufferSize(64ull)
    , m_dstBufferSize(64ull)
    , m_srcCopyFlag(VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR)
    , m_dstCopyFlag(VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR)
    , m_copyRegion{0u, 0u, 512u}
    , m_useSingleChunk(false)
    , m_replaceSrcOffsetWithChunkSize(false)
    , m_replaceDstOffsetWithChunkSize(false)
    , m_chunkSize(0)
{
    // setup configuration parameters based on the test mode
    switch (m_params.mode)
    {
    case CopyFlagTestMode::COPY_TO_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT:
        m_srcBufferSize                 = 512ull;
        m_dstBufferSize                 = 1 << 18;
        m_dstCopyFlag                   = VK_ADDRESS_COPY_SPARSE_BIT_KHR;
        m_dstBufferCreateFlag           = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        m_copyRegion                    = {0u, 0u, 512u};
        m_replaceDstOffsetWithChunkSize = true;
        m_useSingleChunk                = true;
        break;

    case CopyFlagTestMode::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT:
        m_srcBufferSize                 = 1 << 18;
        m_dstBufferSize                 = 512ull;
        m_srcCopyFlag                   = VK_ADDRESS_COPY_SPARSE_BIT_KHR;
        m_srcBufferCreateFlag           = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        m_copyRegion                    = {0u, 0u, 512u};
        m_replaceSrcOffsetWithChunkSize = true;
        m_useSingleChunk                = true;
        break;

    case CopyFlagTestMode::COPY_TO_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT:
        m_srcBufferSize       = 512ull;
        m_dstBufferSize       = 1 << 18;
        m_dstBufferCreateFlag = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        m_copyRegion          = {0u, 0u, 512u};
        break;

    case CopyFlagTestMode::COPY_FROM_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT:
        m_srcBufferSize       = static_cast<vk::VkDeviceSize>(1) << 18;
        m_dstBufferSize       = 512ull;
        m_srcBufferCreateFlag = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        m_copyRegion          = {0u, 0u, 512u};
        break;

    case CopyFlagTestMode::COPY_TO_NON_SPARSE_BUFFER_USING_SPARSE_BIT:
        m_srcBufferSize = 64ull;
        m_dstBufferSize = 64ull;
        m_srcCopyFlag   = VK_ADDRESS_COPY_SPARSE_BIT_KHR;
        m_dstCopyFlag   = VK_ADDRESS_COPY_SPARSE_BIT_KHR;
        m_copyRegion    = {0u, 0u, 64u};
        break;

    case CopyFlagTestMode::COPY_TO_NON_SPARSE_BUFFER_USING_PROTECTED_BIT:
        m_srcBufferSize = 64ull;
        m_dstBufferSize = 64ull;
        m_srcCopyFlag   = VK_ADDRESS_COPY_PROTECTED_BIT_KHR;
        m_dstCopyFlag   = VK_ADDRESS_COPY_PROTECTED_BIT_KHR;
        m_copyRegion    = {0u, 0u, 64u};
        break;

    case CopyFlagTestMode::COPY_NON_LOCAL_MEMORY_USING_DEVICE_LOCAL_BIT:
        m_srcBufferSize = 64ull;
        m_dstBufferSize = 64ull;
        m_copyRegion    = {0u, 0u, 64u};
        break;

    case CopyFlagTestMode::COPY_NON_LOCAL_MEMORY_USING_SPARSE_BIT:
        m_srcBufferSize = 64ull;
        m_dstBufferSize = 64ull;
        m_srcCopyFlag   = VK_ADDRESS_COPY_SPARSE_BIT_KHR;
        m_dstCopyFlag   = VK_ADDRESS_COPY_SPARSE_BIT_KHR;
        m_copyRegion    = {0u, 0u, 64u};
        break;
    };
}

tcu::TestStatus BufferAddressCopyFlags::iterate()
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    auto device               = m_context.getDevice();
    auto queue                = m_context.getUniversalQueue();
    auto queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    auto &allocator           = m_context.getDefaultAllocator();
    VkDeviceSize size         = m_copyRegion.size;

    const bool useSparseSrc = (m_srcBufferCreateFlag & VK_BUFFER_CREATE_SPARSE_BINDING_BIT);
    const bool useSparseDst = (m_dstBufferCreateFlag & VK_BUFFER_CREATE_SPARSE_BINDING_BIT);

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto srcBuffer           = constructBuffer(m_srcBufferSize, usage, m_srcBufferCreateFlag);
    auto dstBuffer           = constructBuffer(m_dstBufferSize, usage, m_dstBufferCreateFlag);

    bindBufferMemory(*srcBuffer, m_srcBufferSize, m_srcBufferAllocVect, useSparseSrc);
    bindBufferMemory(*dstBuffer, m_dstBufferSize, m_dstBufferAllocVect, useSparseDst);

    // now that we know what is chunk size we can replace copyRegion with it if needed
    if (m_replaceSrcOffsetWithChunkSize)
        m_copyRegion.srcOffset = m_chunkSize;
    if (m_replaceDstOffsetWithChunkSize)
        m_copyRegion.dstOffset = m_chunkSize;

    // create staging buffer used to upload data to sparse buffer chunk
    VkBufferCreateInfo bufferCreateInfo = initVulkanStructure();
    bufferCreateInfo.size               = std::max(m_chunkSize, m_srcBufferSize);
    bufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    BufferWithMemory stagingBuffer(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    // write initial data to buffers
    const uint8_t testValue = 253;
    Allocation &srcAlloc    = useSparseSrc ? stagingBuffer.getAllocation() : *m_srcBufferAllocVect[0];
    Allocation &dstAlloc    = useSparseDst ? stagingBuffer.getAllocation() : *m_dstBufferAllocVect[0];
    deMemset(srcAlloc.getHostPtr(), testValue, static_cast<std::size_t>(size));
    flushAlloc(vk, device, srcAlloc);
    deMemset(dstAlloc.getHostPtr(), 0, static_cast<std::size_t>(size));
    flushAlloc(vk, device, dstAlloc);

    VkDeviceAddress srcBufferDeviceAddress = getBufferDeviceAddress(vk, device, *srcBuffer);
    VkDeviceAddress dstBufferDeviceAddress = getBufferDeviceAddress(vk, device, *dstBuffer);

    VkDeviceAddressRangeKHR srcAddressRange{srcBufferDeviceAddress + m_copyRegion.srcOffset, m_copyRegion.size};
    VkDeviceAddressRangeKHR dstAddressRange{dstBufferDeviceAddress + m_copyRegion.dstOffset, m_copyRegion.size};

    VkDeviceMemoryCopyKHR memoryCopy2KHR = initVulkanStructure();
    memoryCopy2KHR.srcRange              = srcAddressRange;
    memoryCopy2KHR.srcCopyFlags          = m_srcCopyFlag;
    memoryCopy2KHR.dstRange              = dstAddressRange;
    memoryCopy2KHR.dstCopyFlags          = m_dstCopyFlag;

    const auto cmdPool(createCommandPool(vk, device, (VkCommandPoolCreateFlags)0, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // helper function to create a buffer memory barrier
    auto cmdBufferBarrier = [&](VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask, VkBuffer buffer,
                                VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask)
    {
        const auto bb = makeBufferMemoryBarrier(srcAccessMask, dstAccessMask, buffer, 0, size);
        vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 1, &bb, 0, nullptr);
    };

    beginCommandBuffer(vk, *cmdBuffer);

    auto initialBuffer = (useSparseSrc ? *stagingBuffer : *srcBuffer);
    cmdBufferBarrier(VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, initialBuffer,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    // copy staging buffer to sparse buffer chunk
    if (useSparseSrc)
    {
        VkBufferCopy bufferCopy = {0, m_copyRegion.srcOffset, size};
        vk.cmdCopyBuffer(*cmdBuffer, *stagingBuffer, *srcBuffer, 1, &bufferCopy);

        cmdBufferBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *srcBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    }

    // use cmdCopyMemoryKHR instead of vk.cmdCopyBuffer(*cmdBuffer, *srcBuffer, *dstBuffer, 1, &m_copyRegion);
    VkCopyDeviceMemoryInfoKHR memorInfo = initVulkanStructure();
    memorInfo.regionCount               = 1u;
    memorInfo.pRegions                  = &memoryCopy2KHR;
    vk.cmdCopyMemoryKHR(*cmdBuffer, &memorInfo);

    cmdBufferBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *dstBuffer,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    if (useSparseDst)
    {
        VkBufferCopy bufferCopy = {m_copyRegion.dstOffset, 0, size};
        vk.cmdCopyBuffer(*cmdBuffer, *dstBuffer, *stagingBuffer, 1, &bufferCopy);

        cmdBufferBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *stagingBuffer,
                         VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    }

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    const auto &alloc = useSparseDst ? stagingBuffer.getAllocation() : *m_dstBufferAllocVect[0];
    invalidateAlloc(vk, device, alloc);

    VkDeviceSize validDataCount = 0;
    uint8_t *ptr                = (uint8_t *)alloc.getHostPtr();
    for (auto i = 0ULL; i < size; ++i)
        validDataCount += (ptr[i] == testValue);

    if (validDataCount == size)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Fail");
}

Move<VkBuffer> BufferAddressCopyFlags::constructBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                                       VkBufferCreateFlags createFlag) const
{
    auto device = m_context.getDevice();
    uint32_t queueFamilyIndex[]{m_context.getUniversalQueueFamilyIndex(), m_context.getSparseQueueFamilyIndex()};

    usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    auto bufferParams  = makeBufferCreateInfo(size, usage);
    bufferParams.flags = createFlag;

    if (createFlag & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
    {
        bufferParams.queueFamilyIndexCount = 2;
        bufferParams.pQueueFamilyIndices   = queueFamilyIndex;
    }

    return createBuffer(m_context.getDeviceInterface(), device, &bufferParams);
}

void BufferAddressCopyFlags::bindBufferMemory(VkBuffer buffer, VkDeviceSize size, AllocVect &allocVect, bool useSparse)
{
    const DeviceInterface &vk                     = m_context.getDeviceInterface();
    auto device                                   = m_context.getDevice();
    auto &allocator                               = m_context.getDefaultAllocator();
    const VkMemoryRequirements memoryRequirements = getBufferMemoryRequirements(vk, device, buffer);

    if (useSparse)
    {
        m_chunkSize                 = memoryRequirements.alignment;
        uint32_t chunkCount         = static_cast<uint32_t>(size / m_chunkSize);
        VkDeviceSize resourceOffset = 0;
        if (m_useSingleChunk)
        {
            chunkCount = 1;

            // we bind memory just to second chunk when we use single chunk
            resourceOffset = m_chunkSize;
        }

        std::vector<VkSparseMemoryBind> sparseMemoryBindVect(chunkCount);

        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            allocVect.push_back(allocator.allocate(memoryRequirements, MemoryRequirement::Any));

            sparseMemoryBindVect[i] = {i * m_chunkSize + resourceOffset, // resourceOffset
                                       m_chunkSize,                      // size
                                       allocVect[i]->getMemory(),        // memory
                                       0,                                // memoryOffset
                                       0};                               // flags
        }

        const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo{
            buffer,
            chunkCount,
            sparseMemoryBindVect.data(),
        };

        VkBindSparseInfo bindInfo = initVulkanStructure();
        bindInfo.bufferBindCount  = 1;
        bindInfo.pBufferBinds     = &sparseBufferMemoryBindInfo;

        const Unique<VkFence> fence(createFence(vk, device));
        VK_CHECK(vk.queueBindSparse(m_context.getSparseQueue(), 1u, &bindInfo, *fence));
        VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), VK_TRUE, ~0ull));
    }
    else
    {
        bool useNonLocalMemory = (m_params.mode == CopyFlagTestMode::COPY_NON_LOCAL_MEMORY_USING_DEVICE_LOCAL_BIT ||
                                  m_params.mode == CopyFlagTestMode::COPY_NON_LOCAL_MEMORY_USING_SPARSE_BIT);
        auto mr                = MemoryRequirement::DeviceAddress | MemoryRequirement::HostVisible |
                  (useNonLocalMemory ? MemoryRequirement::NonLocal : MemoryRequirement::Any);

        allocVect.push_back(allocator.allocate(memoryRequirements, mr));
        auto &bAlloc = allocVect.back();
        vk.bindBufferMemory(device, buffer, bAlloc->getMemory(), bAlloc->getOffset());
    }
}

class BufferAddressCopyFlagsTestCase : public vkt::TestCase
{
public:
    BufferAddressCopyFlagsTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~BufferAddressCopyFlagsTestCase(void) = default;

    virtual TestInstance *createInstance(Context &context) const override
    {
        return new BufferAddressCopyFlags(context, m_params);
    }

    virtual void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_KHR_device_address_commands");

        using TM = CopyFlagTestMode;
        if ((m_params.mode == TM::COPY_TO_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT) ||
            (m_params.mode == TM::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT) ||
            (m_params.mode == TM::COPY_TO_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT) ||
            (m_params.mode == TM::COPY_FROM_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT))
        {
            context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
            context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER);
        }

        if ((m_params.mode == TM::COPY_TO_NON_SPARSE_BUFFER_USING_PROTECTED_BIT) &&
            !context.getProtectedMemoryFeatures().protectedMemory)
        {
            TCU_THROW(NotSupportedError, "Protected Memory feature not supported by the device");
        }
    }

private:
    TestParams m_params;
};

} // namespace

tcu::TestCaseGroup *createDeviceAddressCommandsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "device_address"));

    using TM = CopyFlagTestMode;
    de::MovePtr<tcu::TestCaseGroup> copyFlags(new tcu::TestCaseGroup(testCtx, "copy_flags"));
    const std::vector<std::pair<std::string, TM>> copyFlagCaseVect{
        {"copy_to_memory_with_unbound_ranges_using_sparse_bit",
         TM::COPY_TO_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT},
        {"copy_from_memory_with_unbound_ranges_using_sparse_bit",
         TM::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES_USING_SPARSE_BIT},
        {"copy_to_disjoint_memory_using_device_local_bit", TM::COPY_TO_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT},
        {"copy_from_disjoint_memory_using_device_local_bit", TM::COPY_FROM_DISJOINT_MEMORY_USING_DEVICE_LOCAL_BIT},
        {"copy_to_non_sparse_buffer_using_sparse_bit", TM::COPY_TO_NON_SPARSE_BUFFER_USING_SPARSE_BIT},
        {"copy_to_non_sparse_buffer_using_protected_bit", TM::COPY_TO_NON_SPARSE_BUFFER_USING_PROTECTED_BIT},
        {"copy_non_local_memory_using_device_local_bit", TM::COPY_NON_LOCAL_MEMORY_USING_DEVICE_LOCAL_BIT},
        {"copy_to_non_local_memory_using_sparse_bit", TM::COPY_NON_LOCAL_MEMORY_USING_SPARSE_BIT},
    };
    for (auto &[name, mode] : copyFlagCaseVect)
    {
        TestParams params{
            mode,
        };
        copyFlags->addChild(new BufferAddressCopyFlagsTestCase(testCtx, name, params));
    }
    mainGroup->addChild(copyFlags.release());

    return mainGroup.release();
}

} // namespace vkt::api
