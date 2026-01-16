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
 * \brief Vulkan Copy Buffer To Buffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyBufferToBufferTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

class CopyBufferToBuffer : public CopiesAndBlittingTestInstance
{
public:
    CopyBufferToBuffer(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion,
                                          uint32_t mipLevel = 0u);
    Move<VkBuffer> m_source;
    de::MovePtr<Allocation> m_sourceBufferAlloc;
    Move<VkBuffer> m_destination;
    de::MovePtr<Allocation> m_destinationBufferAlloc;
};

CopyBufferToBuffer::CopyBufferToBuffer(Context &context, TestParams params)
    : CopiesAndBlittingTestInstance(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source buffer
    {
        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_params.src.buffer.size,             // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        m_source            = createBuffer(vk, m_device, &sourceBufferParams);
        m_sourceBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::HostVisible,
                                             *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_source, m_sourceBufferAlloc->getMemory(),
                                     m_sourceBufferAlloc->getOffset()));
    }

    // Create destination buffer
    {
        const VkBufferCreateInfo destinationBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_params.dst.buffer.size,             // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        m_destination = createBuffer(vk, m_device, &destinationBufferParams);
        m_destinationBufferAlloc =
            allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::HostVisible,
                           *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_destination, m_destinationBufferAlloc->getMemory(),
                                     m_destinationBufferAlloc->getOffset()));
    }
}

tcu::TestStatus CopyBufferToBuffer::iterate(void)
{
    const int srcLevelWidth = (int)(m_params.src.buffer.size /
                                    4); // Here the format is VK_FORMAT_R32_UINT, we need to divide the buffer size by 4
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), srcLevelWidth, 1));
    generateBuffer(m_sourceTextureLevel->getAccess(), srcLevelWidth, 1, 1, FILL_MODE_RED);

    const int dstLevelWidth = (int)(m_params.dst.buffer.size / 4);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
    generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1, FILL_MODE_BLACK);

    generateExpectedResult();

    uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    const VkBufferMemoryBarrier srcBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_HOST_WRITE_BIT,                // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_source,                               // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        m_params.src.buffer.size                 // VkDeviceSize size;
    };

    const VkBufferMemoryBarrier dstBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        m_params.dst.buffer.size                 // VkDeviceSize size;
    };

    std::vector<VkBufferCopy> bufferCopies;
    std::vector<VkBufferCopy2KHR> bufferCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferCopies.push_back(m_params.regions[i].bufferCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferCopies2KHR.push_back(convertvkBufferCopyTovkBufferCopy2KHR(m_params.regions[i].bufferCopy));
        }
    }

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &srcBufferBarrier, 0, nullptr);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyBuffer(commandBuffer, m_source.get(), m_destination.get(), (uint32_t)m_params.regions.size(),
                         &bufferCopies[0]);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyBufferInfo2KHR copyBufferInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            m_source.get(),                           // VkBuffer srcBuffer;
            m_destination.get(),                      // VkBuffer dstBuffer;
            (uint32_t)m_params.regions.size(),        // uint32_t regionCount;
            &bufferCopies2KHR[0]                      // const VkBufferCopy2KHR* pRegions;
        };

        vk.cmdCopyBuffer2(commandBuffer, &copyBufferInfo2KHR);
    }

    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &dstBufferBarrier, 0, nullptr);
    endCommandBuffer(vk, commandBuffer);
    submitCommandsAndWaitWithSync(vk, m_device, queue, commandBuffer);
    m_context.resetCommandPoolForVKSC(m_device, commandPool);

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
    invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

void CopyBufferToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                  CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    deMemcpy((uint8_t *)dst.getDataPtr() + region.bufferCopy.dstOffset,
             (uint8_t *)src.getDataPtr() + region.bufferCopy.srcOffset, (size_t)region.bufferCopy.size);
}

class BufferToBufferTestCase : public vkt::TestCase
{
public:
    BufferToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyBufferToBuffer(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);
    }

private:
    TestParams m_params;
};

struct BufferOffsetParams
{
    static constexpr uint32_t kMaxOffset = 8u;

    uint32_t srcOffset;
    uint32_t dstOffset;
};

void checkZerosAt(const std::vector<uint8_t> &bufferData, uint32_t from, uint32_t count)
{
    constexpr uint8_t zero{0};
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto &val = bufferData[from + i];
        if (val != zero)
        {
            std::ostringstream msg;
            msg << "Unexpected non-zero byte found at position " << (from + i) << ": " << static_cast<int>(val);
            TCU_FAIL(msg.str());
        }
    }
}

tcu::TestStatus bufferOffsetTest(Context &ctx, BufferOffsetParams params)
{
    // Try to copy blocks of sizes 1 to kMaxOffset. Each copy region will use a block of kMaxOffset*2 bytes to take into account srcOffset and dstOffset.
    constexpr auto kMaxOffset  = BufferOffsetParams::kMaxOffset;
    constexpr auto kBlockSize  = kMaxOffset * 2u;
    constexpr auto kBufferSize = kMaxOffset * kBlockSize;

    DE_ASSERT(params.srcOffset < kMaxOffset);
    DE_ASSERT(params.dstOffset < kMaxOffset);

    const auto &vkd   = ctx.getDeviceInterface();
    const auto device = ctx.getDevice();
    auto &alloc       = ctx.getDefaultAllocator();
    const auto qIndex = ctx.getUniversalQueueFamilyIndex();
    const auto queue  = ctx.getUniversalQueue();

    const auto srcBufferInfo = makeBufferCreateInfo(kBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto dstBufferInfo = makeBufferCreateInfo(kBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    BufferWithMemory srcBuffer(vkd, device, alloc, srcBufferInfo, MemoryRequirement::HostVisible);
    BufferWithMemory dstBuffer(vkd, device, alloc, dstBufferInfo, MemoryRequirement::HostVisible);
    auto &srcAlloc = srcBuffer.getAllocation();
    auto &dstAlloc = dstBuffer.getAllocation();

    // Zero-out destination buffer.
    deMemset(dstAlloc.getHostPtr(), 0, kBufferSize);
    flushAlloc(vkd, device, dstAlloc);

    // Fill source buffer with nonzero bytes.
    std::vector<uint8_t> srcData;
    srcData.reserve(kBufferSize);
    for (uint32_t i = 0; i < kBufferSize; ++i)
        srcData.push_back(static_cast<uint8_t>(100u + i));
    deMemcpy(srcAlloc.getHostPtr(), srcData.data(), de::dataSize(srcData));
    flushAlloc(vkd, device, srcAlloc);

    // Copy regions.
    std::vector<VkBufferCopy> copies;
    copies.reserve(kMaxOffset);
    for (uint32_t i = 0; i < kMaxOffset; ++i)
    {
        const auto blockStart = kBlockSize * i;
        const auto copySize   = i + 1u;
        const auto bufferCopy = makeBufferCopy(params.srcOffset + blockStart, params.dstOffset + blockStart, copySize);
        copies.push_back(bufferCopy);
    }

    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdCopyBuffer(cmdBuffer, srcBuffer.get(), dstBuffer.get(), static_cast<uint32_t>(copies.size()), copies.data());
    const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier, 0u,
                           nullptr, 0u, nullptr);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWaitWithSync(vkd, device, queue, cmdBuffer);
    invalidateAlloc(vkd, device, dstAlloc);

    // Verify destination buffer data.
    std::vector<uint8_t> dstData(kBufferSize);
    deMemcpy(dstData.data(), dstAlloc.getHostPtr(), de::dataSize(dstData));

    for (uint32_t blockIdx = 0; blockIdx < kMaxOffset; ++blockIdx)
    {
        const auto blockStart = kBlockSize * blockIdx;
        const auto copySize   = blockIdx + 1u;

        // Verify no data has been written before dstOffset.
        checkZerosAt(dstData, blockStart, params.dstOffset);

        // Verify copied block.
        for (uint32_t i = 0; i < copySize; ++i)
        {
            const auto &dstVal = dstData[blockStart + params.dstOffset + i];
            const auto &srcVal = srcData[blockStart + params.srcOffset + i];
            if (dstVal != srcVal)
            {
                std::ostringstream msg;
                msg << "Unexpected value found at position " << (blockStart + params.dstOffset + i) << ": expected "
                    << static_cast<int>(srcVal) << " but found " << static_cast<int>(dstVal);
                TCU_FAIL(msg.str());
            }
        }

        // Verify no data has been written after copy block.
        checkZerosAt(dstData, blockStart + params.dstOffset + copySize, kBlockSize - (params.dstOffset + copySize));
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

void addCopyBufferToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.buffer.size  = defaultSize;
        params.dst.buffer.size  = defaultSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;
        params.useGeneralLayout = testGroupParams->useGeneralLayout;

        const VkBufferCopy bufferCopy = {
            0u,          // VkDeviceSize srcOffset;
            0u,          // VkDeviceSize dstOffset;
            defaultSize, // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "whole", params));
    }

    // Filter is VK_FILTER_NEAREST.
    {
        TestParams params;
        params.src.buffer.size  = defaultQuarterSize;
        params.dst.buffer.size  = defaultQuarterSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;
        params.useGeneralLayout = testGroupParams->useGeneralLayout;

        const VkBufferCopy bufferCopy = {
            12u, // VkDeviceSize srcOffset;
            4u,  // VkDeviceSize dstOffset;
            1u,  // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "partial", params));
    }

    {
        const uint32_t size = 16;
        TestParams params;
        params.src.buffer.size  = size;
        params.dst.buffer.size  = size * (size + 1);
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;
        params.useGeneralLayout = testGroupParams->useGeneralLayout;

        // Copy region with size 1..size
        for (unsigned int i = 1; i <= size; i++)
        {
            const VkBufferCopy bufferCopy = {
                0,        // VkDeviceSize srcOffset;
                i * size, // VkDeviceSize dstOffset;
                i,        // VkDeviceSize size;
            };

            CopyRegion copyRegion;
            copyRegion.bufferCopy = bufferCopy;
            params.regions.push_back(copyRegion);
        }

        group->addChild(new BufferToBufferTestCase(testCtx, "regions", params));
    }

    {
        TestParams params;
        params.src.buffer.size  = 32;
        params.dst.buffer.size  = 32;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;
        params.useGeneralLayout = testGroupParams->useGeneralLayout;

        // Copy four unaligned regions
        for (unsigned int i = 0; i < 4; i++)
        {
            const VkBufferCopy bufferCopy{
                3 + i * 3, // VkDeviceSize    srcOffset;    3  6   9  12
                1 + i * 5, // VkDeviceSize    dstOffset;    1  6  11  16
                2 + i,     // VkDeviceSize    size;        2  3   4   5
            };

            CopyRegion copyRegion;
            copyRegion.bufferCopy = bufferCopy;
            params.regions.push_back(copyRegion);
        }

        group->addChild(new BufferToBufferTestCase(testCtx, "unaligned_regions", params));
    }

    // Whole large
    {
        TestParams params;
        params.src.buffer.size  = defaultLargeSize;
        params.dst.buffer.size  = defaultLargeSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        const VkBufferCopy bufferCopy = {
            0u,               // VkDeviceSize srcOffset;
            0u,               // VkDeviceSize dstOffset;
            defaultLargeSize, // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "whole_large", params));
    }

    // Partial large
    {
        TestParams params;
        params.src.buffer.size  = defaultLargeSize;
        params.dst.buffer.size  = defaultLargeSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        const VkBufferCopy bufferCopy = {
            1024u,                // VkDeviceSize srcOffset;
            defaultLargeSize / 2, // VkDeviceSize dstOffset;
            defaultLargeSize / 2, // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "partial_large", params));
    }

    // Partial large unaligned size
    {
        TestParams params;
        params.src.buffer.size  = 2 * defaultLargeSize;
        params.dst.buffer.size  = 2 * defaultLargeSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        const VkBufferCopy bufferCopy = {
            1024u,                // VkDeviceSize srcOffset;
            defaultLargeSize / 2, // VkDeviceSize dstOffset;
            1 + defaultLargeSize, // VkDeviceSize size;
        };

        CopyRegion copyRegion;
        copyRegion.bufferCopy = bufferCopy;
        params.regions.push_back(copyRegion);

        group->addChild(new BufferToBufferTestCase(testCtx, "partial_large_unaligned_size", params));
    }

    // Unaligned regions large
    {
        TestParams params;
        params.src.buffer.size  = 2 * defaultLargeSize;
        params.dst.buffer.size  = 2 * defaultLargeSize;
        params.allocationKind   = testGroupParams->allocationKind;
        params.extensionFlags   = testGroupParams->extensionFlags;
        params.queueSelection   = testGroupParams->queueSelection;
        params.useSparseBinding = testGroupParams->useSparseBinding;

        for (unsigned int i = 0; i < 5; i++)
        {
            const VkBufferCopy bufferCopy{
                3 + i * 512,  // VkDeviceSize    srcOffset;
                1 + i * 1024, // VkDeviceSize    dstOffset;
                2 + i * 256,  // VkDeviceSize    size;
            };

            CopyRegion copyRegion;
            copyRegion.bufferCopy = bufferCopy;
            params.regions.push_back(copyRegion);
        }

        group->addChild(new BufferToBufferTestCase(testCtx, "unaligned_regions_large", params));
    }
}

void addCopyBufferToBufferOffsetTests(tcu::TestCaseGroup *group)
{
    de::MovePtr<tcu::TestCaseGroup> subGroup(
        new tcu::TestCaseGroup(group->getTestContext(), "buffer_to_buffer_with_offset"));

    for (uint32_t srcOffset = 0u; srcOffset < BufferOffsetParams::kMaxOffset; ++srcOffset)
        for (uint32_t dstOffset = 0u; dstOffset < BufferOffsetParams::kMaxOffset; ++dstOffset)
        {
            BufferOffsetParams params{srcOffset, dstOffset};
            addFunctionCase(subGroup.get(), de::toString(srcOffset) + "_" + de::toString(dstOffset), bufferOffsetTest,
                            params);
        }

    group->addChild(subGroup.release());
}

} // namespace api
} // namespace vkt
