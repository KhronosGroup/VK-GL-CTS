/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Implicit synchronization tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationImplicitTests.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkBarrierUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deRandom.hpp"
#include "deThread.hpp"
#include "deUniquePtr.hpp"

#include <cstdint>
#include <limits>
#include <set>

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;
using namespace vkt::ExternalMemoryUtil;
using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;

template <typename T>
inline SharedPtr<Move<T>> makeVkSharedPtr(Move<T> move)
{
    return SharedPtr<Move<T>>(new Move<T>(move));
}

template <typename T>
inline SharedPtr<T> makeSharedPtr(T *ptr)
{
    return SharedPtr<T>(ptr);
}

template <typename T>
inline SharedPtr<T> makeSharedPtr(de::MovePtr<T> move)
{
    return SharedPtr<T>(move.release());
}

struct QueueTimelineIteration
{
    QueueTimelineIteration(const SharedPtr<OperationSupport> &_opSupport, uint64_t lastValue, VkQueue _queue,
                           uint32_t _queueFamilyIdx, de::Random &rng)
        : opSupport(_opSupport)
        , queue(_queue)
        , queueFamilyIdx(_queueFamilyIdx)
    {
        timelineValue = lastValue + rng.getInt(1, 100);
    }
    ~QueueTimelineIteration()
    {
    }

    SharedPtr<OperationSupport> opSupport;
    VkQueue queue;
    uint32_t queueFamilyIdx;
    uint64_t timelineValue;
    SharedPtr<Operation> op;
};

struct QueueSubmitOrderIteration
{
    QueueSubmitOrderIteration()
    {
    }
    ~QueueSubmitOrderIteration()
    {
    }

    SharedPtr<Resource> resource;
    SharedPtr<Operation> writeOp;
    SharedPtr<Operation> readOp;
};

enum SubmitInfoElements
{
    SIE_WAIT,
    SIE_CMDBUFF,
    SIE_SIGNAL,
    SIE_MAX,
    SIE_NONE = SIE_MAX
};

struct QueueSubmitInfo
{
    SubmitInfoElements queueSubmitInfo[SIE_MAX];
};

// After receiving queue submit information permutation
// in queueSubmitInfos, for each submit info:
// if it has wait, then the number of waits is chosen
// randomly between 2-10.
// If it has command buffer, then the number of command
// buffers is chosen randomly between 2-10.
// If it has signal, then the number of signals is chosen
// randomly between 2-10.
// The counter part of each wait, command buffer and signal
// is then created.
// The command buffers have either read of write operation
// recorded in them.
// If the submit information has wait and command buffer,
// this means that command buffer will not execute before
// its wait is signaled.
// This implies that the command buffer should record a read
// operation and must have a counter part - a command buffer
// with write operation recorded in to it.
// This also implies that the write operation should signal
// one or more semaphores so that the waiting before
// the read operation could end.
// These implications set the basis for the counter part
// operations.
// Within the submit information,
//  All waits should be signaled.
//  All signals must be waited upon.
//  All read operations should have corresponding
//   write operations and vice versa.
// In case of timeline semaphores, one semaphore is
// shared between all waits and signal with different
// timeline values of a single submit info.
// In case of binary semaphore, each wait-signal pair
// in a single submit info has one semaphore each.

class QueueSubmitImplicitTestInstance : public TestInstance
{
public:
    QueueSubmitImplicitTestInstance(Context &context, SynchronizationType type,
                                    const SharedPtr<OperationSupport> writeOpSupport,
                                    const SharedPtr<OperationSupport> readOpSupport,
                                    const ResourceDescription &resourceDesc,
                                    const std::vector<QueueSubmitInfo> &queueSubmitInfos, VkSemaphoreType semaphoreType,
                                    PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_writeOpSupport(writeOpSupport)
        , m_readOpSupport(readOpSupport)
        , m_resourceDesc(resourceDesc)
        , m_queueSubmitInfos(queueSubmitInfos)
        , m_queueSubmitInfoCnt(de::sizeU32(m_queueSubmitInfos))
        , m_semaphoreType(semaphoreType)
        , m_operationContext(new OperationContext(context, type, pipelineCacheData))
        , m_rng(1024)

    {
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const bool isTimelineSemaphore  = (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR);

        Unique<VkFence> fence(createFence(vk, device));
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        std::vector<SharedPtr<Move<VkCommandBuffer>>> ptrCmdBuffersW;
        std::vector<SharedPtr<Move<VkCommandBuffer>>> ptrCmdBuffersR;
        std::vector<VkCommandBuffer> cmdBuffersW;
        std::vector<VkCommandBuffer> cmdBuffersR;

        std::vector<QueueSubmitOrderIteration> iterations;
        std::vector<VkPipelineStageFlags2KHR> stageBits;

        // Variables names with suffix 'A' relate to the actual submit information from test parameter m_queueSubmitInfos
        // Variables names with suffix 'B' relate to the submit information that is automatically generated
        // for counter parts of A
        const uint32_t submitInfoCntA = m_queueSubmitInfoCnt;
        std::vector<std::vector<VkSemaphoreSubmitInfoKHR>> waitSemaphoreSubmitInfosA(submitInfoCntA);
        std::vector<std::vector<VkSemaphoreSubmitInfoKHR>> signalSemaphoreSubmitInfosA(submitInfoCntA);
        std::vector<std::vector<VkCommandBufferSubmitInfoKHR>> commandBufferSubmitInfosA(submitInfoCntA);
        std::vector<uint32_t> waitSemaphoresCountA(submitInfoCntA);
        std::vector<uint32_t> commandBuffersCountA(submitInfoCntA);
        std::vector<uint32_t> signalSemaphoresCountA(submitInfoCntA);

        std::vector<uint32_t> waitSemaphoresCountB(submitInfoCntA);
        std::vector<uint32_t> commandBuffersCountB(submitInfoCntA);
        std::vector<uint32_t> signalSemaphoresCountB(submitInfoCntA);

        std::vector<std::vector<Move<VkSemaphore>>> semaphoresA(submitInfoCntA);
        std::vector<std::vector<VkSemaphore>> semaphoreHandlesA(submitInfoCntA);
        std::vector<std::vector<uint64_t>> timelineValuesA(submitInfoCntA);

        // Find out the number of command buffers needed
        // And submit infos needed for corresponding writes, signals or waits
        uint32_t numOperationsRequired = 0;
        uint32_t submitInfoCntB        = 0;
        const uint32_t minOpsPerInfo   = 2;
        const uint32_t maxOpsPerInfo   = 10;
        for (uint32_t infoIdx = 0; infoIdx < submitInfoCntA; infoIdx++)
        {
            uint32_t noneCnt = 0;
            for (uint32_t elementIdx = 0; (elementIdx < SIE_MAX) && (noneCnt < SIE_MAX); elementIdx++)
            {
                switch (m_queueSubmitInfos[infoIdx].queueSubmitInfo[elementIdx])
                {
                case SIE_WAIT:
                {
                    DE_ASSERT(elementIdx == SIE_WAIT);
                    waitSemaphoresCountA[infoIdx] = signalSemaphoresCountB[infoIdx] =
                        m_rng.getInt(minOpsPerInfo, maxOpsPerInfo);
                }
                break;
                case SIE_CMDBUFF:
                {
                    DE_ASSERT(elementIdx == SIE_CMDBUFF);
                    commandBuffersCountA[infoIdx] = commandBuffersCountB[infoIdx] =
                        m_rng.getInt(minOpsPerInfo, maxOpsPerInfo);
                    numOperationsRequired += commandBuffersCountA[infoIdx];
                }
                break;
                case SIE_SIGNAL:
                {
                    DE_ASSERT(elementIdx == SIE_SIGNAL);
                    signalSemaphoresCountA[infoIdx] = waitSemaphoresCountB[infoIdx] =
                        m_rng.getInt(minOpsPerInfo, maxOpsPerInfo);

                    bool hasWait = m_queueSubmitInfos[infoIdx].queueSubmitInfo[0] == SIE_WAIT;
                    if (hasWait)
                        submitInfoCntB++;
                }
                break;
                case SIE_NONE:
                    noneCnt++;
                default:
                    break;
                }
            }
            DE_ASSERT(noneCnt < SIE_MAX);
            submitInfoCntB++;
        }

        std::vector<std::vector<VkSemaphoreSubmitInfoKHR>> waitSemaphoreSubmitInfosB(submitInfoCntB);
        std::vector<std::vector<VkSemaphoreSubmitInfoKHR>> signalSemaphoreSubmitInfosB(submitInfoCntB);
        std::vector<std::vector<VkCommandBufferSubmitInfoKHR>> commandBufferSubmitInfosB(submitInfoCntB);

        std::vector<std::vector<Move<VkSemaphore>>> semaphoresB(submitInfoCntB);
        std::vector<std::vector<VkSemaphore>> semaphoreHandlesB(submitInfoCntB);
        std::vector<std::vector<uint64_t>> timelineValuesB(submitInfoCntB);

        uint32_t totalSubmitCnt = submitInfoCntA + submitInfoCntB;

        SynchronizationWrapperPtr synchronizationWrapperA =
            getSynchronizationWrapper(m_type, vk, isTimelineSemaphore, totalSubmitCnt);

        // Create numOperationsRequired pair of write/read operations
        iterations.resize(numOperationsRequired);
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderIteration &iter = iterations[iterIdx];

            iter.resource = makeSharedPtr(new Resource(*m_operationContext, m_resourceDesc,
                                                       m_writeOpSupport->getOutResourceUsageFlags() |
                                                           m_readOpSupport->getInResourceUsageFlags()));

            iter.writeOp = makeSharedPtr(m_writeOpSupport->build(*m_operationContext, *iter.resource));
            iter.readOp  = makeSharedPtr(m_readOpSupport->build(*m_operationContext, *iter.resource));
        }

        // Record each write operation into its own command buffer
        // And each read operation in to its own command buffer
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderIteration &iter = iterations[iterIdx];

            // Writes
            ptrCmdBuffersW.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPool)));
            cmdBuffersW.push_back(**(ptrCmdBuffersW.back()));

            beginCommandBuffer(vk, cmdBuffersW.back());
            iter.writeOp->recordCommands(cmdBuffersW.back());

            {
                SynchronizationWrapperPtr syncWrap = getSynchronizationWrapper(m_type, vk, false);
                const SyncInfo writeSync           = iter.writeOp->getOutSyncInfo();
                const SyncInfo readSync            = iter.readOp->getInSyncInfo();
                const Resource &resource           = *iter.resource;

                if (resource.getType() == RESOURCE_TYPE_IMAGE)
                {
                    DE_ASSERT(writeSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
                    DE_ASSERT(readSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

                    const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
                        writeSync.stageMask,                 // VkPipelineStageFlags2KHR            srcStageMask
                        writeSync.accessMask,                // VkAccessFlags2KHR                srcAccessMask
                        readSync.stageMask,                  // VkPipelineStageFlags2KHR            dstStageMask
                        readSync.accessMask,                 // VkAccessFlags2KHR                dstAccessMask
                        writeSync.imageLayout,               // VkImageLayout                    oldLayout
                        readSync.imageLayout,                // VkImageLayout                    newLayout
                        resource.getImage().handle,          // VkImage                            image
                        resource.getImage().subresourceRange // VkImageSubresourceRange            subresourceRange
                    );
                    VkDependencyInfoKHR dependencyInfo =
                        makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2);
                    syncWrap->cmdPipelineBarrier(cmdBuffersW.back(), &dependencyInfo);
                }
                else
                {
                    const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
                        writeSync.stageMask,         // VkPipelineStageFlags2KHR            srcStageMask
                        writeSync.accessMask,        // VkAccessFlags2KHR                srcAccessMask
                        readSync.stageMask,          // VkPipelineStageFlags2KHR            dstStageMask
                        readSync.accessMask,         // VkAccessFlags2KHR                dstAccessMask
                        resource.getBuffer().handle, // VkBuffer                            buffer
                        0,                           // VkDeviceSize                        offset
                        VK_WHOLE_SIZE                // VkDeviceSize                        size
                    );
                    VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, &bufferMemoryBarrier2);
                    syncWrap->cmdPipelineBarrier(cmdBuffersW.back(), &dependencyInfo);
                }

                stageBits.push_back(writeSync.stageMask);
            }

            endCommandBuffer(vk, cmdBuffersW.back());

            // Reads
            ptrCmdBuffersR.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPool)));
            cmdBuffersR.push_back(**(ptrCmdBuffersR.back()));
            beginCommandBuffer(vk, cmdBuffersR.back());
            iter.readOp->recordCommands(cmdBuffersR.back());
            endCommandBuffer(vk, cmdBuffersR.back());
        }

        DE_ASSERT(de::sizeU32(cmdBuffersR) == de::sizeU32(iterations));
        DE_ASSERT(de::sizeU32(cmdBuffersW) == de::sizeU32(iterations));

        uint32_t cmdBufIdxW = 0;
        uint32_t cmdBufIdxR = 0;

        // Add submit infos that are not given in the test parameters
        // and is automatically generated to compensate for signals and waits
        for (uint32_t infoIdx = 0; infoIdx < submitInfoCntA; infoIdx++)
        {
            for (uint32_t elementIdx = 0; elementIdx < SIE_MAX; elementIdx++)
            {
                switch (m_queueSubmitInfos[infoIdx].queueSubmitInfo[elementIdx])
                {
                case SIE_WAIT:
                {
                    addSemaphoreSubmitInfos(signalSemaphoresCountB[infoIdx], signalSemaphoreSubmitInfosB[infoIdx], true,
                                            semaphoresB[infoIdx], semaphoreHandlesB[infoIdx], timelineValuesB[infoIdx]);
                }
                break;
                case SIE_CMDBUFF:
                {
                    SubmitInfoElements prevElement = m_queueSubmitInfos[infoIdx].queueSubmitInfo[0];
                    addCommandBufferSubmitInfos(commandBuffersCountB[infoIdx], commandBufferSubmitInfosB[infoIdx],
                                                prevElement, true, cmdBufIdxR, cmdBuffersR, cmdBufIdxW, cmdBuffersW);
                }
                break;
                case SIE_SIGNAL:
                case SIE_NONE:
                    break;
                }
            }
            if (commandBuffersCountB[infoIdx] || signalSemaphoresCountB[infoIdx])
                synchronizationWrapperA->addSubmitInfo(
                    0u,                            // uint32_t                               waitSemaphoreInfoCount
                    nullptr,                       // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
                    commandBuffersCountB[infoIdx], // uint32_t                               commandBufferInfoCount
                    de::dataOrNull(commandBufferSubmitInfosB
                                       [infoIdx]),   // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
                    signalSemaphoresCountB[infoIdx], // uint32_t                               signalSemaphoreInfoCount
                    de::dataOrNull(signalSemaphoreSubmitInfosB
                                       [infoIdx]), // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
                    false, (isTimelineSemaphore && signalSemaphoresCountB[infoIdx]));
        }

        for (uint32_t infoIdx = 0; infoIdx < submitInfoCntA; infoIdx++)
        {
            for (uint32_t elementIdx = 0; elementIdx < SIE_MAX; elementIdx++)
            {
                switch (m_queueSubmitInfos[infoIdx].queueSubmitInfo[elementIdx])
                {
                case SIE_WAIT:
                {
                    // Wait on semaphores created for signal in the previous pass
                    addSemaphoreSubmitInfos(waitSemaphoresCountA[infoIdx], waitSemaphoreSubmitInfosA[infoIdx], false,
                                            semaphoresB[infoIdx], semaphoreHandlesB[infoIdx], timelineValuesB[infoIdx]);
                }
                break;
                case SIE_CMDBUFF:
                {
                    SubmitInfoElements prevElement = m_queueSubmitInfos[infoIdx].queueSubmitInfo[0];
                    addCommandBufferSubmitInfos(commandBuffersCountA[infoIdx], commandBufferSubmitInfosA[infoIdx],
                                                prevElement, false, cmdBufIdxR, cmdBuffersR, cmdBufIdxW, cmdBuffersW);
                }
                break;
                case SIE_SIGNAL:
                {
                    addSemaphoreSubmitInfos(signalSemaphoresCountA[infoIdx], signalSemaphoreSubmitInfosA[infoIdx], true,
                                            semaphoresA[infoIdx], semaphoreHandlesA[infoIdx], timelineValuesA[infoIdx]);
                }
                break;
                case SIE_NONE:
                default:
                    break;
                }
            }

            if (waitSemaphoresCountA[infoIdx] || commandBuffersCountA[infoIdx] || signalSemaphoresCountA[infoIdx])
                synchronizationWrapperA->addSubmitInfo(
                    waitSemaphoresCountA[infoIdx], // uint32_t                               waitSemaphoreInfoCount
                    de::dataOrNull(waitSemaphoreSubmitInfosA
                                       [infoIdx]), // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
                    commandBuffersCountA[infoIdx], // uint32_t                               commandBufferInfoCount
                    de::dataOrNull(commandBufferSubmitInfosA
                                       [infoIdx]),   // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
                    signalSemaphoresCountA[infoIdx], // uint32_t                               signalSemaphoreInfoCount
                    de::dataOrNull(signalSemaphoreSubmitInfosA
                                       [infoIdx]), // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
                    (isTimelineSemaphore && waitSemaphoresCountA[infoIdx]),
                    (isTimelineSemaphore && signalSemaphoresCountA[infoIdx]));
        }

        uint32_t actionIdxWait = submitInfoCntA;
        for (uint32_t actionIdx = 0; actionIdx < submitInfoCntA; actionIdx++)
        {
            if (m_queueSubmitInfos[actionIdx].queueSubmitInfo[SIE_SIGNAL] == SIE_SIGNAL)
            {
                // Signal's submit info was already created and the semaphore was created with it
                // Wait will use the created semaphore
                bool hasWait = (m_queueSubmitInfos[actionIdx].queueSubmitInfo[SIE_WAIT] == SIE_WAIT);
                addSemaphoreSubmitInfos(
                    waitSemaphoresCountB[actionIdx], waitSemaphoreSubmitInfosB[hasWait ? actionIdxWait : actionIdx],
                    false, semaphoresA[actionIdx], semaphoreHandlesA[actionIdx], timelineValuesA[actionIdx]);

                if (waitSemaphoresCountB[actionIdx])
                    synchronizationWrapperA->addSubmitInfo(
                        waitSemaphoresCountB
                            [actionIdx], // uint32_t                               waitSemaphoreInfoCount
                        de::dataOrNull(
                            waitSemaphoreSubmitInfosB
                                [hasWait ? actionIdxWait++ :
                                           actionIdx]), // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
                        0u,                             // uint32_t                               commandBufferInfoCount
                        nullptr,                        // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
                        0u,      // uint32_t                               signalSemaphoreInfoCount
                        nullptr, // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
                        (isTimelineSemaphore && waitSemaphoresCountB[actionIdx]), false);
            }
        }

        VK_CHECK(synchronizationWrapperA->queueSubmit(queue, *fence));

        VK_CHECK(vk.waitForFences(device, 1, &fence.get(), VK_TRUE, ~0ull));

        // Verify the result of the operations
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderIteration &iter = iterations[iterIdx];
            const Data expected             = iter.writeOp->getData();
            const Data actual               = iter.readOp->getData();

            if (isIndirectBuffer(iter.resource->getType()))
            {
                const uint32_t expectedValue = reinterpret_cast<const uint32_t *>(expected.data)[0];
                const uint32_t actualValue   = reinterpret_cast<const uint32_t *>(actual.data)[0];

                if (actualValue < expectedValue)
                    return tcu::TestStatus::fail("Counter value is smaller than expected");
            }
            else
            {
                if (0 != deMemCmp(expected.data, actual.data, expected.size))
                    return tcu::TestStatus::fail("Memory contents don't match");
            }
        }

        return tcu::TestStatus::pass("Success");
    }

private:
    void addSemaphoreSubmitInfos(uint32_t numSubmitInfos, std::vector<VkSemaphoreSubmitInfoKHR> &semaphoreSubmitInfo,
                                 bool createSemaphore, std::vector<Move<VkSemaphore>> &semaphores,
                                 std::vector<VkSemaphore> &semaphoreHandles, std::vector<uint64_t> &timelineValues);
    void addCommandBufferSubmitInfos(uint32_t numSubmitInfos, std::vector<VkCommandBufferSubmitInfoKHR> &cbSubmitInfo,
                                     SubmitInfoElements prevElement, bool isOpposite, uint32_t &cmdBufIdxR,
                                     std::vector<VkCommandBuffer> &cmdBuffersR, uint32_t &cmdBufIdxW,
                                     std::vector<VkCommandBuffer> &cmdBuffersW);
    void addSemaphore(const DeviceInterface &vk, VkDevice device, std::vector<Move<VkSemaphore>> &semaphores,
                      std::vector<VkSemaphore> &semaphoreHandles, std::vector<uint64_t> &timelineValues,
                      uint64_t firstTimelineValue)
    {
        Move<VkSemaphore> semaphore;

        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR)
        {
            // Only allocate a single semaphore
            if (semaphores.empty())
            {
                semaphores.push_back(createSemaphoreType(vk, device, m_semaphoreType));
            }
        }
        else
        {
            semaphores.push_back(createSemaphoreType(vk, device, m_semaphoreType));
        }

        semaphoreHandles.push_back(*semaphores.back());
        timelineValues.push_back((timelineValues.empty() ? firstTimelineValue : timelineValues.back()) +
                                 m_rng.getInt(1, 100));
    }

    SynchronizationType m_type;
    SharedPtr<OperationSupport> m_writeOpSupport;
    SharedPtr<OperationSupport> m_readOpSupport;
    const ResourceDescription &m_resourceDesc;
    const std::vector<QueueSubmitInfo> m_queueSubmitInfos;
    const uint32_t m_queueSubmitInfoCnt;
    VkSemaphoreType m_semaphoreType;
    UniquePtr<OperationContext> m_operationContext;
    de::Random m_rng;
};

void QueueSubmitImplicitTestInstance::addSemaphoreSubmitInfos(
    uint32_t numSubmitInfos, std::vector<VkSemaphoreSubmitInfoKHR> &semaphoreSubmitInfo, bool createSemaphore,
    std::vector<Move<VkSemaphore>> &semaphores, std::vector<VkSemaphore> &semaphoreHandles,
    std::vector<uint64_t> &timelineValues)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    for (uint32_t infoIdx = 0; infoIdx < numSubmitInfos; infoIdx++)
    {
        if (createSemaphore)
            addSemaphore(vk, device, semaphores, semaphoreHandles, timelineValues, 2u);

        bool isSignal            = createSemaphore;
        VkSemaphoreSubmitInfo si = makeCommonSemaphoreSubmitInfo(
            (createSemaphore ? *semaphores.back() : semaphoreHandles[infoIdx]), timelineValues[infoIdx],
            isSignal ? VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR : VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
        semaphoreSubmitInfo.push_back(si);
    }
}

void QueueSubmitImplicitTestInstance::addCommandBufferSubmitInfos(
    uint32_t numSubmitInfos, std::vector<VkCommandBufferSubmitInfoKHR> &cbSubmitInfo, SubmitInfoElements prevElement,
    bool isOpposite, uint32_t &cmdBufIdxR, std::vector<VkCommandBuffer> &cmdBuffersR, uint32_t &cmdBufIdxW,
    std::vector<VkCommandBuffer> &cmdBuffersW)
{
    for (uint32_t infoIdx = 0; infoIdx < numSubmitInfos; infoIdx++)
    {
        VkCommandBufferSubmitInfoKHR si;

        VkCommandBuffer cmdBuf;
        if (prevElement == SIE_WAIT)
        {
            cmdBuf = isOpposite ? cmdBuffersW[cmdBufIdxW++] : cmdBuffersR[cmdBufIdxR++];
        }
        else
        {
            cmdBuf = isOpposite ? cmdBuffersR[cmdBufIdxR++] : cmdBuffersW[cmdBufIdxW++];
        }

        si = makeCommonCommandBufferSubmitInfo(cmdBuf);
        cbSubmitInfo.push_back(si);
    }
}

class QueueSubmitImplicitTestCase : public TestCase
{
public:
    QueueSubmitImplicitTestCase(tcu::TestContext &testCtx, SynchronizationType type, const std::string &name,
                                OperationName writeOp, OperationName readOp, const ResourceDescription &resourceDesc,
                                const std::vector<QueueSubmitInfo> &queueSubmissionCombo, VkSemaphoreType semaphoreType,
                                PipelineCacheData &pipelineCacheData)
        : TestCase(testCtx, name.c_str())
        , m_type(type)
        , m_writeOpSupport(makeOperationSupport(writeOp, resourceDesc).release())
        , m_readOpSupport(makeOperationSupport(readOp, resourceDesc).release())
        , m_resourceDesc(resourceDesc)
        , m_queueSubmissionCombo(queueSubmissionCombo)
        , m_semaphoreType(semaphoreType)
        , m_pipelineCacheData(pipelineCacheData)
    {
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR &&
            !context.getTimelineSemaphoreFeatures().timelineSemaphore)
            TCU_THROW(NotSupportedError, "Timeline semaphore not supported");
        if (m_type == SynchronizationType::SYNCHRONIZATION2)
            context.requireDeviceFunctionality("VK_KHR_synchronization2");
    }

    TestInstance *createInstance(Context &context) const
    {
        return new QueueSubmitImplicitTestInstance(context, m_type, m_writeOpSupport, m_readOpSupport, m_resourceDesc,
                                                   m_queueSubmissionCombo, m_semaphoreType, m_pipelineCacheData);
    }

    void initPrograms(SourceCollections &programCollection) const
    {
        m_writeOpSupport->initPrograms(programCollection);
        m_readOpSupport->initPrograms(programCollection);
    }

private:
    SynchronizationType m_type;
    SharedPtr<OperationSupport> m_writeOpSupport;
    SharedPtr<OperationSupport> m_readOpSupport;
    const ResourceDescription &m_resourceDesc;
    const std::vector<QueueSubmitInfo> m_queueSubmissionCombo;
    VkSemaphoreType m_semaphoreType;
    PipelineCacheData &m_pipelineCacheData;
};

// Create combinations of different queue submit
// information structures VkSubmitInfo.
// These combinations are then collectively submitted to
// vkQueueSubmit() on the same queue.
// Base cases are listed in  queueSumbitInfoTypes
//  0: Wait only
//  1: Wait and Command buffer
//  2: Wait and Signal
//  3: Wait, Command buffer and Signal
// Permutations of these cases are created in
// four VkSubmitInfos and passed as test parameters.
// The test itself generates more VkSubmitInfos as counter
// parts of each of the above cases respectively.
//  0: Signal only
//  1: Command buffer and Signal
//  2-a: Signal
//  2-b: Wait
//  3-a: Command buffer and Signal
//  3-b: Wait
// In this way, all cases and orders of submit information
// are covered.
// See comments with QueueSubmitImplicitTestInstance for
// more details.
class QueueSubmitImplicitTests : public tcu::TestCaseGroup
{
public:
    QueueSubmitImplicitTests(tcu::TestContext &testCtx, SynchronizationType type, VkSemaphoreType semaphoreType,
                             const char *name)
        : tcu::TestCaseGroup(testCtx, name)
        , m_type(type)
        , m_semaphoreType(semaphoreType)
    {
    }

    void init(void)
    {
        static const OperationName writeOps[] = {
            OPERATION_NAME_WRITE_COPY_BUFFER,
            OPERATION_NAME_WRITE_SSBO_VERTEX,

        };
        static const OperationName readOps[] = {
            OPERATION_NAME_READ_COPY_BUFFER,
            OPERATION_NAME_READ_SSBO_VERTEX,
        };

        const std::vector<QueueSubmitInfo> queueSumbitInfoTypes{
            {SIE_WAIT, SIE_NONE, SIE_NONE},
            {SIE_WAIT, SIE_CMDBUFF, SIE_NONE},
            {SIE_WAIT, SIE_NONE, SIE_SIGNAL},
            {SIE_WAIT, SIE_CMDBUFF, SIE_SIGNAL}
            // rest of the combinations will be added automatically
        };

        const uint32_t numQueueSumbitInfoTypes = de::sizeU32(queueSumbitInfoTypes);

        uint32_t comboCnt = 0;
        for (uint32_t writeOpIdx = 0; writeOpIdx < DE_LENGTH_OF_ARRAY(writeOps); writeOpIdx++)
            for (uint32_t readOpIdx = 0; readOpIdx < DE_LENGTH_OF_ARRAY(readOps); readOpIdx++)
            {
                const OperationName writeOp   = writeOps[writeOpIdx];
                const OperationName readOp    = readOps[readOpIdx];
                const std::string opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
                bool empty                    = true;

                de::MovePtr<tcu::TestCaseGroup> opGroup(new tcu::TestCaseGroup(m_testCtx, opGroupName.c_str()));

                // each ops combination shall be tested with just one resource
                for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
                {
                    const ResourceDescription &resource = s_resources[resourceNdx];

                    if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
                    {
                        const std::string resName = getResourceName(resource);
                        de::MovePtr<tcu::TestCaseGroup> resGroup(new tcu::TestCaseGroup(m_testCtx, resName.c_str()));

                        uint32_t idx0 = comboCnt % numQueueSumbitInfoTypes;
                        for (uint32_t idx1 = 0; idx1 < numQueueSumbitInfoTypes; idx1++)
                            for (uint32_t idx2 = 0; idx2 < numQueueSumbitInfoTypes; idx2++)
                                for (uint32_t idx3 = 0; idx3 < numQueueSumbitInfoTypes; idx3++)
                                {
                                    const std::vector<QueueSubmitInfo> queueSubmitInfos = {
                                        queueSumbitInfoTypes[idx0], queueSumbitInfoTypes[idx1],
                                        queueSumbitInfoTypes[idx2], queueSumbitInfoTypes[idx3]};

                                    const std::string testName = de::toString(idx0) + de::toString(idx1) +
                                                                 de::toString(idx2) + de::toString(idx3);
                                    resGroup->addChild(new QueueSubmitImplicitTestCase(
                                        m_testCtx, m_type, testName, writeOp, readOp, resource, queueSubmitInfos,
                                        m_semaphoreType, m_pipelineCacheData));
                                    empty = false;
                                }
                        if (!empty)
                            opGroup->addChild(resGroup.release());
                        comboCnt++;
                        break;
                    }
                }
                if (!empty)
                    addChild(opGroup.release());
            }
    }

private:
    SynchronizationType m_type;
    VkSemaphoreType m_semaphoreType;
    // synchronization.op tests share pipeline cache data to speed up test
    // execution.
    PipelineCacheData m_pipelineCacheData;
};

} // namespace

tcu::TestCaseGroup *createImplicitSyncTests(tcu::TestContext &testCtx, SynchronizationType type)
{
    de::MovePtr<tcu::TestCaseGroup> implicitSyncTests(new tcu::TestCaseGroup(testCtx, "implicit"));

    implicitSyncTests->addChild(
        new QueueSubmitImplicitTests(testCtx, type, VK_SEMAPHORE_TYPE_BINARY_KHR, "binary_semaphore"));

    implicitSyncTests->addChild(
        new QueueSubmitImplicitTests(testCtx, type, VK_SEMAPHORE_TYPE_TIMELINE_KHR, "timeline_semaphore"));

    return implicitSyncTests.release();
}

} // namespace synchronization
} // namespace vkt
