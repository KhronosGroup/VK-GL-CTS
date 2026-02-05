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
#include <optional>

namespace vkt::api
{

namespace
{

class CopyBufferToBuffer : public CopiesAndBlittingTestInstance
{
public:
    CopyBufferToBuffer(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    Move<VkBuffer> constructBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBufferCreateFlags createFlag = 0,
                                   std::optional<uint32_t> queueFamilyIndex = {}) const;
    de::MovePtr<Allocation> bindBufferMemory(VkBuffer buffer, MemoryRequirement memoryRequirement) const;

    de::MovePtr<tcu::TextureLevel> generateFilledBuffer(VkDeviceSize size, FillMode fillMode) const;

    // to avoid ifdefs for SC use int for both copyFlags and asign 1 to them by default
    // which is equal to VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR
    void recordAndSubmitCommandBuffer(VkQueue queue, VkCommandBuffer commandBuffer, VkDeviceSize srcBufferSize,
                                      VkDeviceSize dstBufferSize, const std::vector<CopyRegion> &regions,
                                      uint32_t srcCopyFlag = 1, uint32_t dstCopyFlag = 1);

protected:
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
    const DeviceInterface &vk                = m_context.getDeviceInterface();
    auto [queue, commandBuffer, commandPool] = activeExecutionCtx();

    m_source                 = constructBuffer(m_params.src.buffer.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_sourceBufferAlloc      = bindBufferMemory(*m_source, MemoryRequirement::HostVisible);
    m_destination            = constructBuffer(m_params.dst.buffer.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    m_destinationBufferAlloc = bindBufferMemory(*m_destination, MemoryRequirement::HostVisible);

    m_sourceTextureLevel      = generateFilledBuffer(m_params.src.buffer.size, FILL_MODE_RED);
    m_destinationTextureLevel = generateFilledBuffer(m_params.dst.buffer.size, FILL_MODE_BLACK);

    generateExpectedResult();

    uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    recordAndSubmitCommandBuffer(queue, commandBuffer, m_params.src.buffer.size, m_params.dst.buffer.size,
                                 m_params.regions);

    m_context.resetCommandPoolForVKSC(m_device, commandPool);

    // Read buffer data
    using TexLevel          = tcu::TextureLevel;
    const int dstLevelWidth = (int)(m_params.dst.buffer.size / 4);
    de::MovePtr<TexLevel> resultLevel(new TexLevel(mapVkFormat(VK_FORMAT_R32_UINT), dstLevelWidth, 1));
    invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

Move<VkBuffer> CopyBufferToBuffer::constructBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                                   VkBufferCreateFlags createFlag,
                                                   std::optional<uint32_t> queueFamilyIndexOpt) const
{
    if (m_params.extensionFlags & DEVICE_ADDRESS_COMMANDS)
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    auto bufferParams  = makeBufferCreateInfo(size, usage);
    bufferParams.flags = createFlag;

    uint32_t queueFamilyIndex          = queueFamilyIndexOpt.value_or(0);
    bufferParams.queueFamilyIndexCount = queueFamilyIndexOpt.has_value();
    bufferParams.pQueueFamilyIndices   = &queueFamilyIndex;

    return createBuffer(m_context.getDeviceInterface(), m_device, &bufferParams);
}

de::MovePtr<Allocation> CopyBufferToBuffer::bindBufferMemory(VkBuffer buffer, MemoryRequirement memoryRequirement) const
{
    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();

    MemoryRequirement memReq = (m_params.extensionFlags & DEVICE_ADDRESS_COMMANDS) ?
                                   memoryRequirement | MemoryRequirement::DeviceAddress :
                                   memoryRequirement;

    auto bufferAlloc =
        allocateBuffer(vki, vk, vkPhysDevice, m_device, buffer, memReq, *m_allocator, m_params.allocationKind);
    vk.bindBufferMemory(m_device, buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset());

    return bufferAlloc;
}

de::MovePtr<tcu::TextureLevel> CopyBufferToBuffer::generateFilledBuffer(VkDeviceSize size, FillMode fillMode) const
{
    using TexLevel = tcu::TextureLevel;

    // Here the format is VK_FORMAT_R32_UINT, we need to divide the buffer size by 4
    const int levelWidth = static_cast<int>(size) / 4;
    de::MovePtr<TexLevel> textureLevel(new TexLevel(mapVkFormat(VK_FORMAT_R32_UINT), levelWidth, 1));
    generateBuffer(textureLevel->getAccess(), levelWidth, 1, 1, fillMode);

    return textureLevel;
}

void CopyBufferToBuffer::recordAndSubmitCommandBuffer(VkQueue queue, VkCommandBuffer commandBuffer,
                                                      VkDeviceSize srcBufferSize, VkDeviceSize dstBufferSize,
                                                      const std::vector<CopyRegion> &regions,
                                                      [[maybe_unused]] uint32_t srcCopyFlag,
                                                      [[maybe_unused]] uint32_t dstCopyFlag)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const auto srcBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *m_source, 0, srcBufferSize);

    const auto dstBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                          *m_destination, 0, dstBufferSize);

    std::vector<VkBufferCopy> bufferCopies;
    std::vector<VkBufferCopy2KHR> bufferCopies2KHR;
#ifndef CTS_USES_VULKANSC
    std::vector<VkDeviceMemoryCopyKHR> memoryCopies2KHR;
#endif // CTS_USES_VULKANSC

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &srcBufferBarrier, 0, nullptr);

    if (m_params.extensionFlags & DEVICE_ADDRESS_COMMANDS)
    {
#ifndef CTS_USES_VULKANSC

        VkDeviceAddress srcBufferDeviceAddress = getBufferDeviceAddress(vk, m_device, *m_source);
        VkDeviceAddress dstBufferDeviceAddress = getBufferDeviceAddress(vk, m_device, *m_destination);

        memoryCopies2KHR.reserve(regions.size());
        for (const auto &r : regions)
        {
            const auto &bc = r.bufferCopy;
            VkDeviceAddressRangeKHR srcAddressRange{srcBufferDeviceAddress + bc.srcOffset, bc.size};
            VkDeviceAddressRangeKHR dstAddressRange{dstBufferDeviceAddress + bc.dstOffset, bc.size};

            memoryCopies2KHR.push_back({VK_STRUCTURE_TYPE_DEVICE_MEMORY_COPY_KHR, nullptr, srcAddressRange, srcCopyFlag,
                                        dstAddressRange, dstCopyFlag});
        }

        VkCopyDeviceMemoryInfoKHR memorInfo = initVulkanStructure();
        memorInfo.regionCount               = (uint32_t)regions.size();
        memorInfo.pRegions                  = memoryCopies2KHR.data();

        vk.cmdCopyMemoryKHR(commandBuffer, &memorInfo);
#endif // CTS_USES_VULKANSC
    }
    else if (m_params.extensionFlags & COPY_COMMANDS_2)
    {
        bufferCopies2KHR.reserve(regions.size());
        for (const auto &r : regions)
            bufferCopies2KHR.push_back(convertvkBufferCopyTovkBufferCopy2KHR(r.bufferCopy));

        const VkCopyBufferInfo2KHR copyBufferInfo2KHR{
            VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            *m_source,                                // VkBuffer srcBuffer;
            *m_destination,                           // VkBuffer dstBuffer;
            (uint32_t)regions.size(),                 // uint32_t regionCount;
            &bufferCopies2KHR[0]                      // const VkBufferCopy2KHR* pRegions;
        };

        vk.cmdCopyBuffer2(commandBuffer, &copyBufferInfo2KHR);
    }
    else
    {
        DE_ASSERT(~m_params.extensionFlags & (COPY_COMMANDS_2 | DEVICE_ADDRESS_COMMANDS));

        bufferCopies.reserve(regions.size());
        for (const auto &r : regions)
            bufferCopies.push_back(r.bufferCopy);

        vk.cmdCopyBuffer(commandBuffer, *m_source, *m_destination, (uint32_t)regions.size(), &bufferCopies[0]);
    }

    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &dstBufferBarrier, 0, nullptr);
    endCommandBuffer(vk, commandBuffer);
    submitCommandsAndWaitWithSync(vk, m_device, queue, commandBuffer);
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

} // namespace vkt::api
