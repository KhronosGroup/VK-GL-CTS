/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization primitive tests with single queue
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationOperationSingleQueueTests.hpp"
#include "deSTLUtil.hpp"
#include "deSharedPtr.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"
#include <cstdint>
#include <string>

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;
using tcu::TestLog;

class BaseTestInstance : public TestInstance
{
public:
    BaseTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                     const OperationSupport &writeOp, const OperationSupport &readOp,
                     PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_opContext(context, type, pipelineCacheData)
        , m_resource(new Resource(m_opContext, resourceDesc,
                                  writeOp.getOutResourceUsageFlags() | readOp.getInResourceUsageFlags()))
        , m_writeOp(writeOp.build(m_opContext, *m_resource))
        , m_readOp(readOp.build(m_opContext, *m_resource))
    {
    }

protected:
    SynchronizationType m_type;
    OperationContext m_opContext;
    const de::UniquePtr<Resource> m_resource;
    const de::UniquePtr<Operation> m_writeOp;
    const de::UniquePtr<Operation> m_readOp;
};

class EventTestInstance : public BaseTestInstance
{
    bool m_maintenance9;

public:
    EventTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                      const OperationSupport &writeOp, const OperationSupport &readOp,
                      PipelineCacheData &pipelineCacheData, bool maintenance9)
        : BaseTestInstance(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
        , m_maintenance9(maintenance9)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));
        const VkEventCreateFlagBits eventFlags(m_type == SynchronizationType::SYNCHRONIZATION2 ?
                                                   VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR :
                                                   (VkEventCreateFlagBits)0);
        const Unique<VkEvent> event(createEvent(vk, device, eventFlags));
        const SyncInfo writeSync                         = m_writeOp->getOutSyncInfo();
        const SyncInfo readSync                          = m_readOp->getInSyncInfo();
        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_type, vk, false);

        beginCommandBuffer(vk, *cmdBuffer);

        m_writeOp->recordCommands(*cmdBuffer);

        if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
        {
            const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
                writeSync.stageMask,                    // VkPipelineStageFlags2KHR            srcStageMask
                writeSync.accessMask,                   // VkAccessFlags2KHR                srcAccessMask
                readSync.stageMask,                     // VkPipelineStageFlags2KHR            dstStageMask
                readSync.accessMask,                    // VkAccessFlags2KHR                dstAccessMask
                writeSync.imageLayout,                  // VkImageLayout                    oldLayout
                readSync.imageLayout,                   // VkImageLayout                    newLayout
                m_resource->getImage().handle,          // VkImage                            image
                m_resource->getImage().subresourceRange // VkImageSubresourceRange            subresourceRange
            );
            VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2, true);
            synchronizationWrapper->cmdSetEvent(*cmdBuffer, *event, &dependencyInfo);
            synchronizationWrapper->cmdWaitEvents(*cmdBuffer, 1u, &event.get(), &dependencyInfo);
        }
        else
        {
            const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 =
                makeBufferMemoryBarrier2(writeSync.stageMask,  // VkPipelineStageFlags2KHR            srcStageMask
                                         writeSync.accessMask, // VkAccessFlags2KHR                srcAccessMask
                                         readSync.stageMask,   // VkPipelineStageFlags2KHR            dstStageMask
                                         readSync.accessMask,  // VkAccessFlags2KHR                dstAccessMask
                                         m_resource->getBuffer().handle, // VkBuffer                            buffer
                                         m_resource->getBuffer().offset, // VkDeviceSize                        offset
                                         m_resource->getBuffer().size    // VkDeviceSize                        size
                );
            VkDependencyInfoKHR dependencyInfo =
                makeCommonDependencyInfo(nullptr, &bufferMemoryBarrier2, nullptr, true);
            if (m_maintenance9)
            {
                const VkMemoryBarrier2 memoryBarrier = makeMemoryBarrier2(writeSync.stageMask, VK_ACCESS_2_NONE,
                                                                          VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
                VkDependencyInfoKHR setDependencyInfo =
                    makeCommonDependencyInfo(&memoryBarrier, nullptr, nullptr, true);
#ifndef CTS_USES_VULKANSC
                setDependencyInfo.dependencyFlags |= VK_DEPENDENCY_ASYMMETRIC_EVENT_BIT_KHR;
                dependencyInfo.dependencyFlags |= VK_DEPENDENCY_ASYMMETRIC_EVENT_BIT_KHR;
#endif
                synchronizationWrapper->cmdSetEvent(*cmdBuffer, *event, &setDependencyInfo);
            }
            else
            {
                synchronizationWrapper->cmdSetEvent(*cmdBuffer, *event, &dependencyInfo);
            }
            synchronizationWrapper->cmdWaitEvents(*cmdBuffer, 1u, &event.get(), &dependencyInfo);
        }

        m_readOp->recordCommands(*cmdBuffer);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(synchronizationWrapper, vk, device, queue, *cmdBuffer);

        {
            const Data expected = m_writeOp->getData();
            const Data actual   = m_readOp->getData();

            if (isIndirectBuffer(m_resource->getType()))
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

        return tcu::TestStatus::pass("OK");
    }
};

class EventsTestInstance : public TestInstance
{
public:
    EventsTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc1,
                       const ResourceDescription &resourceDesc2, const OperationSupport &writeOp1,
                       const OperationSupport &readOp1, const OperationSupport &writeOp2,
                       const OperationSupport &readOp2, PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_opContext(context, type, pipelineCacheData)
        , m_opContext2(context, type, pipelineCacheData)
        , m_hasNop(false)
        , m_isFirstEventNop(m_hasNop)
    {
        const de::SharedPtr<Resource> res1(new Resource(
            m_opContext, resourceDesc1, writeOp1.getOutResourceUsageFlags() | readOp1.getInResourceUsageFlags()));
        const de::SharedPtr<Operation> wOp1(writeOp1.build(m_opContext, *res1).release());
        const de::SharedPtr<Operation> rOp1(readOp1.build(m_opContext, *res1).release());
        const de::SharedPtr<Resource> res2(new Resource(
            m_opContext2, resourceDesc2, writeOp2.getOutResourceUsageFlags() | readOp2.getInResourceUsageFlags()));
        const de::SharedPtr<Operation> wOp2(writeOp2.build(m_opContext2, *res2).release());
        const de::SharedPtr<Operation> rOp2(readOp2.build(m_opContext2, *res2).release());
        m_resources.push_back(res1);
        m_writeOps.push_back(wOp1);
        m_readOps.push_back(rOp1);
        m_resources.push_back(res2);
        m_writeOps.push_back(wOp2);
        m_readOps.push_back(rOp2);
    }

    EventsTestInstance(Context &context, SynchronizationType type, const bool isFirstEventNop,
                       const ResourceDescription &resourceDesc1, const OperationSupport &writeOp1,
                       const OperationSupport &readOp1, PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_opContext(context, type, pipelineCacheData)
        , m_opContext2(context, type, pipelineCacheData)
        , m_hasNop(true)
        , m_isFirstEventNop(isFirstEventNop)
    {
        const de::SharedPtr<Resource> res1(new Resource(
            m_opContext, resourceDesc1, writeOp1.getOutResourceUsageFlags() | readOp1.getInResourceUsageFlags()));
        const de::SharedPtr<Operation> wOp1(writeOp1.build(m_opContext, *res1).release());
        const de::SharedPtr<Operation> rOp1(readOp1.build(m_opContext, *res1).release());
        m_resources.push_back(res1);
        m_writeOps.push_back(wOp1);
        m_readOps.push_back(rOp1);
    }

protected:
    const SynchronizationType m_type;
    OperationContext m_opContext;
    OperationContext m_opContext2;
    const bool m_hasNop;
    const bool m_isFirstEventNop;
    std::vector<de::SharedPtr<Operation>> m_writeOps;
    std::vector<de::SharedPtr<Operation>> m_readOps;
    std::vector<de::SharedPtr<Resource>> m_resources;

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const uint32_t numEvents        = 2u; // set 2 events, wait on 2 events
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_type, vk, false);

        std::vector<Move<VkEvent>> events;
        std::vector<VkEvent> eventHandles;
        std::vector<VkDependencyInfoKHR> dependencyInfos;
        dependencyInfos.resize(numEvents);
        VkDependencyInfoKHR nullDependencyInfo = makeCommonDependencyInfo(nullptr, nullptr, nullptr, true);

        std::vector<VkImageMemoryBarrier2KHR> imgBarriers;
        imgBarriers.resize(numEvents);
        std::vector<VkBufferMemoryBarrier2KHR> buffBarriers;
        buffBarriers.resize(numEvents);

        beginCommandBuffer(vk, *cmdBuffer);

        uint32_t skipEventIdx = (m_isFirstEventNop ? 0u : 1u);

        uint32_t opsIdx = 0;
        for (uint32_t eventIdx = 0; eventIdx < numEvents; eventIdx++)
        {
            events.push_back(createEvent(vk, device));

            if (m_hasNop && skipEventIdx == eventIdx) // add an empty event and dependency
            {
                dependencyInfos[eventIdx] = nullDependencyInfo;
            }
            else
            {
                const de::SharedPtr<Operation> writeOp = m_writeOps[opsIdx];
                const de::SharedPtr<Operation> readOp  = m_readOps[opsIdx];
                const de::SharedPtr<Resource> resource = m_resources[opsIdx];

                const SyncInfo writeSync = writeOp->getOutSyncInfo();
                const SyncInfo readSync  = readOp->getInSyncInfo();

                writeOp->recordCommands(*cmdBuffer);

                if (m_resources[opsIdx]->getType() == RESOURCE_TYPE_IMAGE)
                {
                    imgBarriers[opsIdx] = makeImageMemoryBarrier2(
                        writeSync.stageMask,                    // VkPipelineStageFlags2KHR            srcStageMask
                        writeSync.accessMask,                   // VkAccessFlags2KHR                srcAccessMask
                        readSync.stageMask,                     // VkPipelineStageFlags2KHR            dstStageMask
                        readSync.accessMask,                    // VkAccessFlags2KHR                dstAccessMask
                        writeSync.imageLayout,                  // VkImageLayout                    oldLayout
                        readSync.imageLayout,                   // VkImageLayout                    newLayout
                        m_resources[opsIdx]->getImage().handle, // VkImage                            image
                        m_resources[opsIdx]
                            ->getImage()
                            .subresourceRange // VkImageSubresourceRange            subresourceRange
                    );
                    dependencyInfos[eventIdx] = makeCommonDependencyInfo(nullptr, nullptr, &imgBarriers[opsIdx], true);
                }
                else
                {
                    buffBarriers[opsIdx] = makeBufferMemoryBarrier2(
                        writeSync.stageMask,                     // VkPipelineStageFlags2KHR            srcStageMask
                        writeSync.accessMask,                    // VkAccessFlags2KHR                srcAccessMask
                        readSync.stageMask,                      // VkPipelineStageFlags2KHR            dstStageMask
                        readSync.accessMask,                     // VkAccessFlags2KHR                dstAccessMask
                        m_resources[opsIdx]->getBuffer().handle, // VkBuffer                            buffer
                        m_resources[opsIdx]->getBuffer().offset, // VkDeviceSize                        offset
                        m_resources[opsIdx]->getBuffer().size    // VkDeviceSize                        size
                    );
                    dependencyInfos[eventIdx] = makeCommonDependencyInfo(nullptr, &buffBarriers[opsIdx], nullptr, true);
                }

                opsIdx++;
            }
            synchronizationWrapper->cmdSetEvent(*cmdBuffer, *(events.back()), &dependencyInfos[eventIdx]);
            eventHandles.push_back(*(events.back()));
        }

        synchronizationWrapper->cmdWaitEvents(*cmdBuffer, numEvents, de::dataOrNull(eventHandles),
                                              de::dataOrNull(dependencyInfos));

        const uint32_t opsCount = opsIdx;
        for (uint32_t idx = 0; idx < opsCount; idx++)
        {
            const de::SharedPtr<Operation> readOp = m_readOps[idx];
            readOp->recordCommands(*cmdBuffer);
        }

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(synchronizationWrapper, vk, device, queue, *cmdBuffer);

        for (uint32_t idx = 0; idx < opsCount; idx++)
        {
            const de::SharedPtr<Operation> writeOp = m_writeOps[idx];
            const de::SharedPtr<Operation> readOp  = m_readOps[idx];
            const de::SharedPtr<Resource> resource = m_resources[idx];

            const Data expected = writeOp->getData();
            const Data actual   = readOp->getData();

            if (isIndirectBuffer(resource->getType()))
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
        return tcu::TestStatus::pass("OK");
    }
};

class BarrierTestInstance : public BaseTestInstance
{
public:
    BarrierTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                        const OperationSupport &writeOp, const OperationSupport &readOp,
                        PipelineCacheData &pipelineCacheData)
        : BaseTestInstance(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        const Move<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));
        const SyncInfo writeSync                         = m_writeOp->getOutSyncInfo();
        const SyncInfo readSync                          = m_readOp->getInSyncInfo();
        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_type, vk, false);

        beginCommandBuffer(vk, *cmdBuffer);

        m_writeOp->recordCommands(*cmdBuffer);

        if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
        {
            const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
                writeSync.stageMask,                    // VkPipelineStageFlags2KHR            srcStageMask
                writeSync.accessMask,                   // VkAccessFlags2KHR                srcAccessMask
                readSync.stageMask,                     // VkPipelineStageFlags2KHR            dstStageMask
                readSync.accessMask,                    // VkAccessFlags2KHR                dstAccessMask
                writeSync.imageLayout,                  // VkImageLayout                    oldLayout
                readSync.imageLayout,                   // VkImageLayout                    newLayout
                m_resource->getImage().handle,          // VkImage                            image
                m_resource->getImage().subresourceRange // VkImageSubresourceRange            subresourceRange
            );
            VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2);
            synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
        }
        else
        {
            const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 =
                makeBufferMemoryBarrier2(writeSync.stageMask,  // VkPipelineStageFlags2KHR            srcStageMask
                                         writeSync.accessMask, // VkAccessFlags2KHR                srcAccessMask
                                         readSync.stageMask,   // VkPipelineStageFlags2KHR            dstStageMask
                                         readSync.accessMask,  // VkAccessFlags2KHR                dstAccessMask
                                         m_resource->getBuffer().handle, // VkBuffer                            buffer
                                         m_resource->getBuffer().offset, // VkDeviceSize                        offset
                                         m_resource->getBuffer().size    // VkDeviceSize                        size
                );
            VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, &bufferMemoryBarrier2);
            synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
        }

        m_readOp->recordCommands(*cmdBuffer);

        endCommandBuffer(vk, *cmdBuffer);

        submitCommandsAndWait(synchronizationWrapper, vk, device, queue, *cmdBuffer);

        {
            const Data expected = m_writeOp->getData();
            const Data actual   = m_readOp->getData();

            if (isIndirectBuffer(m_resource->getType()))
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

        return tcu::TestStatus::pass("OK");
    }
};

class BinarySemaphoreTestInstance : public BaseTestInstance
{
public:
    BinarySemaphoreTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                                const OperationSupport &writeOp, const OperationSupport &readOp,
                                PipelineCacheData &pipelineCacheData)
        : BaseTestInstance(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
    {
    }

    tcu::TestStatus iterate(void)
    {
        enum
        {
            WRITE = 0,
            READ,
            COUNT
        };
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const Unique<VkSemaphore> semaphore(createSemaphore(vk, device));
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        const Move<VkCommandBuffer> ptrCmdBuffer[COUNT]  = {makeCommandBuffer(vk, device, *cmdPool),
                                                            makeCommandBuffer(vk, device, *cmdPool)};
        VkCommandBuffer cmdBuffers[COUNT]                = {*ptrCmdBuffer[WRITE], *ptrCmdBuffer[READ]};
        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_type, vk, false, 2u);
        const SyncInfo writeSync                         = m_writeOp->getOutSyncInfo();
        const SyncInfo readSync                          = m_readOp->getInSyncInfo();
        VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo =
            makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
        VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
            makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
        VkCommandBufferSubmitInfoKHR commandBufferSubmitInfo[]{makeCommonCommandBufferSubmitInfo(cmdBuffers[WRITE]),
                                                               makeCommonCommandBufferSubmitInfo(cmdBuffers[READ])};

        synchronizationWrapper->addSubmitInfo(0u, nullptr, 1u, &commandBufferSubmitInfo[WRITE], 1u,
                                              &signalSemaphoreSubmitInfo);
        synchronizationWrapper->addSubmitInfo(1u, &waitSemaphoreSubmitInfo, 1u, &commandBufferSubmitInfo[READ], 0u,
                                              nullptr);

        beginCommandBuffer(vk, cmdBuffers[WRITE]);

        m_writeOp->recordCommands(cmdBuffers[WRITE]);

        if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
        {
            const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
                writeSync.stageMask,                    // VkPipelineStageFlags2KHR            srcStageMask
                writeSync.accessMask,                   // VkAccessFlags2KHR                srcAccessMask
                readSync.stageMask,                     // VkPipelineStageFlags2KHR            dstStageMask
                readSync.accessMask,                    // VkAccessFlags2KHR                dstAccessMask
                writeSync.imageLayout,                  // VkImageLayout                    oldLayout
                readSync.imageLayout,                   // VkImageLayout                    newLayout
                m_resource->getImage().handle,          // VkImage                            image
                m_resource->getImage().subresourceRange // VkImageSubresourceRange            subresourceRange
            );
            VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2);
            synchronizationWrapper->cmdPipelineBarrier(cmdBuffers[WRITE], &dependencyInfo);
        }
        else
        {
            const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 =
                makeBufferMemoryBarrier2(writeSync.stageMask,  // VkPipelineStageFlags2KHR            srcStageMask
                                         writeSync.accessMask, // VkAccessFlags2KHR                srcAccessMask
                                         readSync.stageMask,   // VkPipelineStageFlags2KHR            dstStageMask
                                         readSync.accessMask,  // VkAccessFlags2KHR                dstAccessMask
                                         m_resource->getBuffer().handle, // VkBuffer                            buffer
                                         0,                              // VkDeviceSize                        offset
                                         VK_WHOLE_SIZE                   // VkDeviceSize                        size
                );
            VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, &bufferMemoryBarrier2);
            synchronizationWrapper->cmdPipelineBarrier(cmdBuffers[WRITE], &dependencyInfo);
        }

        endCommandBuffer(vk, cmdBuffers[WRITE]);

        beginCommandBuffer(vk, cmdBuffers[READ]);

        m_readOp->recordCommands(cmdBuffers[READ]);

        endCommandBuffer(vk, cmdBuffers[READ]);

        VK_CHECK(synchronizationWrapper->queueSubmit(queue, VK_NULL_HANDLE));
        VK_CHECK(vk.queueWaitIdle(queue));

        {
            const Data expected = m_writeOp->getData();
            const Data actual   = m_readOp->getData();

            if (isIndirectBuffer(m_resource->getType()))
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

        return tcu::TestStatus::pass("OK");
    }
};

template <typename T>
inline de::SharedPtr<Move<T>> makeVkSharedPtr(Move<T> move)
{
    return de::SharedPtr<Move<T>>(new Move<T>(move));
}

class TimelineSemaphoreTestInstance : public TestInstance
{
public:
    TimelineSemaphoreTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                                  const de::SharedPtr<OperationSupport> &writeOp,
                                  const de::SharedPtr<OperationSupport> &readOp, PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_opContext(context, type, pipelineCacheData)
    {

        // Create a chain operation copying data from one resource to
        // another, each of the operation will be executing with a
        // dependency on the previous using timeline points.
        m_opSupports.push_back(writeOp);
        for (uint32_t copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
        {
            if (isResourceSupported(s_copyOps[copyOpNdx], resourceDesc))
                m_opSupports.push_back(de::SharedPtr<OperationSupport>(
                    makeOperationSupport(s_copyOps[copyOpNdx], resourceDesc, false).release()));
        }
        m_opSupports.push_back(readOp);

        for (uint32_t opNdx = 0; opNdx < (m_opSupports.size() - 1); opNdx++)
        {
            uint32_t usage =
                m_opSupports[opNdx]->getOutResourceUsageFlags() | m_opSupports[opNdx + 1]->getInResourceUsageFlags();

            m_resources.push_back(de::SharedPtr<Resource>(new Resource(m_opContext, resourceDesc, usage)));
        }

        m_ops.push_back(de::SharedPtr<Operation>(m_opSupports[0]->build(m_opContext, *m_resources[0]).release()));
        for (uint32_t opNdx = 1; opNdx < (m_opSupports.size() - 1); opNdx++)
            m_ops.push_back(de::SharedPtr<Operation>(
                m_opSupports[opNdx]->build(m_opContext, *m_resources[opNdx - 1], *m_resources[opNdx]).release()));
        m_ops.push_back(de::SharedPtr<Operation>(
            m_opSupports[m_opSupports.size() - 1]->build(m_opContext, *m_resources.back()).release()));
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        de::Random rng(1234);
        const Unique<VkSemaphore> semaphore(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        std::vector<de::SharedPtr<Move<VkCommandBuffer>>> ptrCmdBuffers;
        std::vector<VkCommandBufferSubmitInfoKHR> cmdBuffersInfo(m_ops.size(), makeCommonCommandBufferSubmitInfo(0u));
        std::vector<VkSemaphoreSubmitInfoKHR> waitSemaphoreSubmitInfos(
            m_ops.size(), makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
        std::vector<VkSemaphoreSubmitInfoKHR> signalSemaphoreSubmitInfos(
            m_ops.size(), makeCommonSemaphoreSubmitInfo(*semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));
        SynchronizationWrapperPtr synchronizationWrapper =
            getSynchronizationWrapper(m_type, vk, true, static_cast<uint32_t>(m_ops.size()));
        uint64_t increment = 0u;

        for (uint32_t opNdx = 0; opNdx < m_ops.size(); opNdx++)
        {
            ptrCmdBuffers.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPool)));
            cmdBuffersInfo[opNdx].commandBuffer = **(ptrCmdBuffers.back());
        }

        for (uint32_t opNdx = 0; opNdx < m_ops.size(); opNdx++)
        {
            increment += (1 + rng.getUint8());
            signalSemaphoreSubmitInfos[opNdx].value = increment;
            waitSemaphoreSubmitInfos[opNdx].value   = increment;

            synchronizationWrapper->addSubmitInfo(
                opNdx == 0 ? 0u : 1u, opNdx == 0 ? nullptr : &waitSemaphoreSubmitInfos[opNdx - 1], 1u,
                &cmdBuffersInfo[opNdx], 1u, &signalSemaphoreSubmitInfos[opNdx], opNdx == 0 ? false : true, true);

            VkCommandBuffer cmdBuffer = cmdBuffersInfo[opNdx].commandBuffer;
            beginCommandBuffer(vk, cmdBuffer);

            if (opNdx > 0)
            {
                const SyncInfo lastSync    = m_ops[opNdx - 1]->getOutSyncInfo();
                const SyncInfo currentSync = m_ops[opNdx]->getInSyncInfo();
                const Resource &resource   = *m_resources[opNdx - 1].get();

                if (resource.getType() == RESOURCE_TYPE_IMAGE)
                {
                    DE_ASSERT(lastSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);
                    DE_ASSERT(currentSync.imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

                    const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
                        lastSync.stageMask,                  // VkPipelineStageFlags2KHR            srcStageMask
                        lastSync.accessMask,                 // VkAccessFlags2KHR                srcAccessMask
                        currentSync.stageMask,               // VkPipelineStageFlags2KHR            dstStageMask
                        currentSync.accessMask,              // VkAccessFlags2KHR                dstAccessMask
                        lastSync.imageLayout,                // VkImageLayout                    oldLayout
                        currentSync.imageLayout,             // VkImageLayout                    newLayout
                        resource.getImage().handle,          // VkImage                            image
                        resource.getImage().subresourceRange // VkImageSubresourceRange            subresourceRange
                    );
                    VkDependencyInfoKHR dependencyInfo =
                        makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2);
                    synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
                }
                else
                {
                    const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
                        lastSync.stageMask,          // VkPipelineStageFlags2KHR            srcStageMask
                        lastSync.accessMask,         // VkAccessFlags2KHR                srcAccessMask
                        currentSync.stageMask,       // VkPipelineStageFlags2KHR            dstStageMask
                        currentSync.accessMask,      // VkAccessFlags2KHR                dstAccessMask
                        resource.getBuffer().handle, // VkBuffer                            buffer
                        0,                           // VkDeviceSize                        offset
                        VK_WHOLE_SIZE                // VkDeviceSize                        size
                    );
                    VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, &bufferMemoryBarrier2);
                    synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
                }
            }

            m_ops[opNdx]->recordCommands(cmdBuffer);

            endCommandBuffer(vk, cmdBuffer);
        }

        VK_CHECK(synchronizationWrapper->queueSubmit(queue, VK_NULL_HANDLE));
        VK_CHECK(vk.queueWaitIdle(queue));

        {
            const Data expected = m_ops.front()->getData();
            const Data actual   = m_ops.back()->getData();

            if (isIndirectBuffer(m_resources[0]->getType()))
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

        return tcu::TestStatus::pass("OK");
    }

protected:
    SynchronizationType m_type;
    OperationContext m_opContext;
    std::vector<de::SharedPtr<OperationSupport>> m_opSupports;
    std::vector<de::SharedPtr<Operation>> m_ops;
    std::vector<de::SharedPtr<Resource>> m_resources;
};

class FenceTestInstance : public BaseTestInstance
{
public:
    FenceTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                      const OperationSupport &writeOp, const OperationSupport &readOp,
                      PipelineCacheData &pipelineCacheData)
        : BaseTestInstance(context, type, resourceDesc, writeOp, readOp, pipelineCacheData)
    {
    }

    tcu::TestStatus iterate(void)
    {
        enum
        {
            WRITE = 0,
            READ,
            COUNT
        };
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const VkQueue queue             = m_context.getUniversalQueue();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const Unique<VkCommandPool> cmdPool(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
        const Move<VkCommandBuffer> ptrCmdBuffer[COUNT] = {makeCommandBuffer(vk, device, *cmdPool),
                                                           makeCommandBuffer(vk, device, *cmdPool)};
        VkCommandBuffer cmdBuffers[COUNT]               = {*ptrCmdBuffer[WRITE], *ptrCmdBuffer[READ]};
        const SyncInfo writeSync                        = m_writeOp->getOutSyncInfo();
        const SyncInfo readSync                         = m_readOp->getInSyncInfo();
        SynchronizationWrapperPtr synchronizationWrapper[COUNT]{getSynchronizationWrapper(m_type, vk, false),
                                                                getSynchronizationWrapper(m_type, vk, false)};

        beginCommandBuffer(vk, cmdBuffers[WRITE]);

        m_writeOp->recordCommands(cmdBuffers[WRITE]);

        if (m_resource->getType() == RESOURCE_TYPE_IMAGE)
        {
            const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
                writeSync.stageMask,                    // VkPipelineStageFlags2KHR            srcStageMask
                writeSync.accessMask,                   // VkAccessFlags2KHR                srcAccessMask
                readSync.stageMask,                     // VkPipelineStageFlags2KHR            dstStageMask
                readSync.accessMask,                    // VkAccessFlags2KHR                dstAccessMask
                writeSync.imageLayout,                  // VkImageLayout                    oldLayout
                readSync.imageLayout,                   // VkImageLayout                    newLayout
                m_resource->getImage().handle,          // VkImage                            image
                m_resource->getImage().subresourceRange // VkImageSubresourceRange            subresourceRange
            );
            VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2);
            synchronizationWrapper[WRITE]->cmdPipelineBarrier(cmdBuffers[WRITE], &dependencyInfo);
        }

        endCommandBuffer(vk, cmdBuffers[WRITE]);

        submitCommandsAndWait(synchronizationWrapper[WRITE], vk, device, queue, cmdBuffers[WRITE]);

        beginCommandBuffer(vk, cmdBuffers[READ]);

        m_readOp->recordCommands(cmdBuffers[READ]);

        endCommandBuffer(vk, cmdBuffers[READ]);

        submitCommandsAndWait(synchronizationWrapper[READ], vk, device, queue, cmdBuffers[READ]);

        {
            const Data expected = m_writeOp->getData();
            const Data actual   = m_readOp->getData();

            if (isIndirectBuffer(m_resource->getType()))
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

        return tcu::TestStatus::pass("OK");
    }
};

class SyncTestCase : public TestCase
{
public:
    SyncTestCase(tcu::TestContext &testCtx, const std::string &name, SynchronizationType type,
                 const SyncPrimitive syncPrimitive, const ResourceDescription resourceDesc, const OperationName writeOp,
                 const OperationName readOp, const bool specializedAccess, PipelineCacheData &pipelineCacheData,
                 bool maintenance9)
        : TestCase(testCtx, name)
        , m_type(type)
        , m_resourceDesc(resourceDesc)
        , m_writeOp(makeOperationSupport(writeOp, resourceDesc, specializedAccess).release())
        , m_readOp(makeOperationSupport(readOp, resourceDesc, specializedAccess).release())
        , m_syncPrimitive(syncPrimitive)
        , m_pipelineCacheData(pipelineCacheData)
        , m_maintenance9(maintenance9)
    {
    }

    void initPrograms(SourceCollections &programCollection) const
    {
        m_writeOp->initPrograms(programCollection);
        m_readOp->initPrograms(programCollection);

        if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE)
        {
            for (uint32_t copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
            {
                if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
                    makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc, false)->initPrograms(programCollection);
            }
        }
    }

    void checkSupport(Context &context) const
    {
        if (m_type == SynchronizationType::SYNCHRONIZATION2)
            context.requireDeviceFunctionality("VK_KHR_synchronization2");

#ifndef CTS_USES_VULKANSC
        if (SYNC_PRIMITIVE_EVENT == m_syncPrimitive &&
            context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
            !context.getPortabilitySubsetFeatures().events)
        {
            TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
        }
#endif // CTS_USES_VULKANSC

        if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE &&
            !context.getTimelineSemaphoreFeatures().timelineSemaphore)
            TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

        if (m_resourceDesc.type == RESOURCE_TYPE_IMAGE)
        {
            VkImageFormatProperties imageFormatProperties;
            const uint32_t usage = m_writeOp->getOutResourceUsageFlags() | m_readOp->getInResourceUsageFlags();
            const InstanceInterface &instance     = context.getInstanceInterface();
            const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
            const VkResult formatResult           = instance.getPhysicalDeviceImageFormatProperties(
                physicalDevice, m_resourceDesc.imageFormat, m_resourceDesc.imageType, VK_IMAGE_TILING_OPTIMAL, usage,
                (VkImageCreateFlags)0, &imageFormatProperties);

            if (formatResult != VK_SUCCESS)
                TCU_THROW(NotSupportedError, "Image format is not supported");

            if ((imageFormatProperties.sampleCounts & m_resourceDesc.imageSamples) != m_resourceDesc.imageSamples)
                TCU_THROW(NotSupportedError, "Requested sample count is not supported");
        }

        if (m_maintenance9)
            context.requireDeviceFunctionality("VK_KHR_maintenance9");
    }

    TestInstance *createInstance(Context &context) const
    {
        switch (m_syncPrimitive)
        {
        case SYNC_PRIMITIVE_FENCE:
            return new FenceTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData);
        case SYNC_PRIMITIVE_BINARY_SEMAPHORE:
            return new BinarySemaphoreTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp,
                                                   m_pipelineCacheData);
        case SYNC_PRIMITIVE_TIMELINE_SEMAPHORE:
            return new TimelineSemaphoreTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp,
                                                     m_pipelineCacheData);
        case SYNC_PRIMITIVE_BARRIER:
            return new BarrierTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData);
        case SYNC_PRIMITIVE_EVENT:
            return new EventTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData,
                                         m_maintenance9);
        }

        DE_ASSERT(0);
        return nullptr;
    }

private:
    SynchronizationType m_type;
    const ResourceDescription m_resourceDesc;
    const de::SharedPtr<OperationSupport> m_writeOp;
    const de::SharedPtr<OperationSupport> m_readOp;
    const SyncPrimitive m_syncPrimitive;
    PipelineCacheData &m_pipelineCacheData;
    const bool m_maintenance9;
};

class SyncEventsTestCase : public TestCase
{
public:
    SyncEventsTestCase(tcu::TestContext &testCtx, const std::string &name, SynchronizationType type,
                       const ResourceDescription resourceDesc1, const OperationName writeOp1,
                       const OperationName readOp1, const ResourceDescription resourceDesc2,
                       const OperationName writeOp2, const OperationName readOp2, PipelineCacheData &pipelineCacheData)
        : TestCase(testCtx, name)
        , m_type(type)
        , m_hasNop(false)
        , m_isFirstEventNop(m_hasNop)
        , m_resourceDesc1(resourceDesc1)
        , m_resourceDesc2(resourceDesc2)
        , m_writeOp1(makeOperationSupport(writeOp1, resourceDesc1, false).release())
        , m_readOp1(makeOperationSupport(readOp1, resourceDesc1, false).release())
        , m_writeOp2(makeOperationSupport(writeOp2, resourceDesc2, false).release())
        , m_readOp2(makeOperationSupport(readOp2, resourceDesc2, false).release())
        , m_pipelineCacheData(pipelineCacheData)
    {
    }

    SyncEventsTestCase(tcu::TestContext &testCtx, const std::string &name, SynchronizationType type,
                       bool isFirstEventNop, const ResourceDescription resourceDesc, const OperationName writeOp,
                       const OperationName readOp, PipelineCacheData &pipelineCacheData)
        : TestCase(testCtx, name)
        , m_type(type)
        , m_hasNop(true)
        , m_isFirstEventNop(isFirstEventNop)
        , m_resourceDesc1(resourceDesc)
        , m_resourceDesc2()
        , m_writeOp1(makeOperationSupport(writeOp, resourceDesc, false).release())
        , m_readOp1(makeOperationSupport(readOp, resourceDesc, false).release())
        , m_writeOp2(nullptr)
        , m_readOp2(nullptr)
        , m_pipelineCacheData(pipelineCacheData)
    {
    }

    void initPrograms(SourceCollections &programCollection) const
    {
        m_writeOp1->initPrograms(programCollection);
        m_readOp1->initPrograms(programCollection);

        if (!m_hasNop)
        {
            m_writeOp2->initPrograms(programCollection);
            m_readOp2->initPrograms(programCollection);
        }
    }

    void checkSupport(Context &context) const
    {
        if (m_type == SynchronizationType::SYNCHRONIZATION2)
            context.requireDeviceFunctionality("VK_KHR_synchronization2");

#ifndef CTS_USES_VULKANSC
        if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
            !context.getPortabilitySubsetFeatures().events)
        {
            TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
        }
#endif // CTS_USES_VULKANSC

        if (m_resourceDesc1.type == RESOURCE_TYPE_IMAGE)
        {
            VkImageFormatProperties imageFormatProperties;
            const uint32_t usage = m_writeOp1->getOutResourceUsageFlags() | m_readOp1->getInResourceUsageFlags();
            const InstanceInterface &instance     = context.getInstanceInterface();
            const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
            const VkResult formatResult           = instance.getPhysicalDeviceImageFormatProperties(
                physicalDevice, m_resourceDesc1.imageFormat, m_resourceDesc1.imageType, VK_IMAGE_TILING_OPTIMAL, usage,
                (VkImageCreateFlags)0, &imageFormatProperties);

            if (formatResult != VK_SUCCESS)
                TCU_THROW(NotSupportedError, "Image format is not supported");

            if ((imageFormatProperties.sampleCounts & m_resourceDesc1.imageSamples) != m_resourceDesc1.imageSamples)
                TCU_THROW(NotSupportedError, "Requested sample count is not supported");
        }

        if (!m_hasNop && m_resourceDesc2.type == RESOURCE_TYPE_IMAGE)
        {
            VkImageFormatProperties imageFormatProperties;
            const uint32_t usage = m_writeOp2->getOutResourceUsageFlags() | m_readOp2->getInResourceUsageFlags();
            const InstanceInterface &instance     = context.getInstanceInterface();
            const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
            const VkResult formatResult           = instance.getPhysicalDeviceImageFormatProperties(
                physicalDevice, m_resourceDesc2.imageFormat, m_resourceDesc2.imageType, VK_IMAGE_TILING_OPTIMAL, usage,
                (VkImageCreateFlags)0, &imageFormatProperties);

            if (formatResult != VK_SUCCESS)
                TCU_THROW(NotSupportedError, "Image format is not supported");

            if ((imageFormatProperties.sampleCounts & m_resourceDesc2.imageSamples) != m_resourceDesc2.imageSamples)
                TCU_THROW(NotSupportedError, "Requested sample count is not supported");
        }
    }

    TestInstance *createInstance(Context &context) const
    {
        if (m_hasNop)
            return new EventsTestInstance(context, m_type, m_isFirstEventNop, m_resourceDesc1, *m_writeOp1, *m_readOp1,
                                          m_pipelineCacheData);
        else
            return new EventsTestInstance(context, m_type, m_resourceDesc1, m_resourceDesc2, *m_writeOp1, *m_readOp1,
                                          *m_writeOp2, *m_readOp2, m_pipelineCacheData);
    }

private:
    const SynchronizationType m_type;
    const bool m_hasNop;
    const bool m_isFirstEventNop;
    const ResourceDescription m_resourceDesc1;
    const ResourceDescription m_resourceDesc2;
    const de::SharedPtr<OperationSupport> m_writeOp1;
    const de::SharedPtr<OperationSupport> m_readOp1;
    const de::SharedPtr<OperationSupport> m_writeOp2;
    const de::SharedPtr<OperationSupport> m_readOp2;
    PipelineCacheData &m_pipelineCacheData;
};

struct TestData
{
    SynchronizationType type;
    PipelineCacheData *pipelineCacheData;
};

struct TestCombo
{
    const OperationName writeOpName;
    const OperationName readOpName;
    const ResourceDescription &resource;
    const std::string resourceName;
};

// Tests for waiting on two events
// Each event consists of a write and read operation
// Order of events:
//  execute first write operation, set event 1 , execute second write operation, set event 2
//  wait on event 1 and 2
//  after wait, execute first read operation, second read operation
void createMultipleEventsTests(tcu::TestCaseGroup *group, TestData data)
{
    if (data.type == SynchronizationType::SYNCHRONIZATION2)
    {
        tcu::TestContext &testCtx = group->getTestContext();

        de::MovePtr<tcu::TestCaseGroup> multiEventsGroup(new tcu::TestCaseGroup(testCtx, "multi_events"));

        static const OperationName e_writeOps[] = {
            OPERATION_NAME_WRITE_FILL_BUFFER,
            OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE,
            OPERATION_NAME_WRITE_BLIT_IMAGE,
            OPERATION_NAME_WRITE_SSBO_VERTEX,
        };

        static const OperationName e_readOps[] = {
            OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE,
            OPERATION_NAME_READ_BLIT_IMAGE,
            OPERATION_NAME_READ_UBO_FRAGMENT,
            OPERATION_NAME_READ_SSBO_VERTEX,
        };

        static const ResourceDescription e_resources[] = {
            {RESOURCE_TYPE_BUFFER, tcu::IVec4(0x4000, 0, 0, 0), vk::VK_IMAGE_TYPE_LAST, vk::VK_FORMAT_UNDEFINED,
             (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT}, // 16 KiB (min max UBO range)
            {RESOURCE_TYPE_BUFFER, tcu::IVec4(0x40000, 0, 0, 0), vk::VK_IMAGE_TYPE_LAST, vk::VK_FORMAT_UNDEFINED,
             (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT}, // 256 KiB
            {RESOURCE_TYPE_IMAGE, tcu::IVec4(128, 128, 0, 0), vk::VK_IMAGE_TYPE_2D, vk::VK_FORMAT_R8G8B8A8_UNORM,
             vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_SAMPLE_COUNT_1_BIT},
            {RESOURCE_TYPE_IMAGE, tcu::IVec4(128, 128, 0, 0), vk::VK_IMAGE_TYPE_2D, vk::VK_FORMAT_R32G32B32A32_SFLOAT,
             vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_SAMPLE_COUNT_1_BIT},
            {RESOURCE_TYPE_IMAGE, tcu::IVec4(64, 64, 8, 0), vk::VK_IMAGE_TYPE_3D, vk::VK_FORMAT_R32_SFLOAT,
             vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_SAMPLE_COUNT_1_BIT},
        };

        std::vector<TestCombo> opSets;

        // Create valid combinations of write/read operation pairs
        for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(e_writeOps); ++writeOpNdx)
            for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(e_readOps); ++readOpNdx)
            {
                const OperationName writeOpName = e_writeOps[writeOpNdx];
                const OperationName readOpName  = e_readOps[readOpNdx];

                for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(e_resources); ++resourceNdx)
                {
                    const ResourceDescription &resource = e_resources[resourceNdx];
                    std::string resourceName            = getResourceName(resource);

                    if (isResourceSupported(writeOpName, resource) && isResourceSupported(readOpName, resource))
                    {
                        const TestCombo opSet = {writeOpName, readOpName, resource, resourceName};
                        opSets.push_back(opSet);
                    }
                }
            }

        // Using the above combinations, create tests with two events, each having a write/read operation pair
        for (uint32_t firstEvtIdx = 0; firstEvtIdx < opSets.size(); firstEvtIdx++)
        {
            const TestCombo *firstEvent = &opSets[firstEvtIdx];
            const std::string firstEventName =
                getOperationName(firstEvent->writeOpName) + "_" + getOperationName(firstEvent->readOpName);

            for (uint32_t secondEvtIdx = 0; secondEvtIdx < opSets.size(); secondEvtIdx++)
            {
                const TestCombo *secondEvent = &opSets[secondEvtIdx];
                const std::string secondEventName =
                    getOperationName(secondEvent->writeOpName) + "_" + getOperationName(secondEvent->readOpName);
                const std::string testName = firstEventName + "__" + secondEventName + "_res_" +
                                             firstEvent->resourceName + "_" + secondEvent->resourceName;

                multiEventsGroup->addChild(new SyncEventsTestCase(
                    testCtx, testName, data.type, firstEvent->resource, firstEvent->writeOpName, firstEvent->readOpName,
                    secondEvent->resource, secondEvent->writeOpName, secondEvent->readOpName, *data.pipelineCacheData));
            }
        }

        // Create tests where one of the events does not depend on any work
        // The no-op event will do no work and will have no dependency
        for (uint32_t evtIdx = 0; evtIdx < opSets.size(); evtIdx++)
        {
            const TestCombo *evt      = &opSets[evtIdx];
            const std::string evtName = getOperationName(evt->writeOpName) + "_" + getOperationName(evt->readOpName);

            for (const auto &isFirstEventNop : {true, false})
            {
                const std::string firstEvtName = (isFirstEventNop ? "nop" : evtName);
                const std::string secEvtName   = (isFirstEventNop ? evtName : "nop");
                const std::string firstResName = (isFirstEventNop ? "none" : evt->resourceName);
                const std::string secResName   = (isFirstEventNop ? evt->resourceName : "none");
                const std::string testName =
                    firstEvtName + "__" + secEvtName + "_res_" + firstResName + "_" + secResName;

                multiEventsGroup->addChild(new SyncEventsTestCase(testCtx, testName, data.type, isFirstEventNop,
                                                                  evt->resource, evt->writeOpName, evt->readOpName,
                                                                  *data.pipelineCacheData));
            }
        }
        group->addChild(multiEventsGroup.release());
    }
}
void createTests(tcu::TestCaseGroup *group, TestData data)
{
    tcu::TestContext &testCtx = group->getTestContext();

    static const struct
    {
        const char *name;
        SyncPrimitive syncPrimitive;
        int numOptions;
    } groups[] = {
        {
            "fence",
            SYNC_PRIMITIVE_FENCE,
            0,
        },
        {
            "binary_semaphore",
            SYNC_PRIMITIVE_BINARY_SEMAPHORE,
            0,
        },
        {
            "timeline_semaphore",
            SYNC_PRIMITIVE_TIMELINE_SEMAPHORE,
            0,
        },
        {
            "barrier",
            SYNC_PRIMITIVE_BARRIER,
            1,
        },
        {
            "event",
            SYNC_PRIMITIVE_EVENT,
            1,
        },
    };

    for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groups); ++groupNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> synchGroup(new tcu::TestCaseGroup(testCtx, groups[groupNdx].name));

        for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(s_writeOps); ++writeOpNdx)
            for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(s_readOps); ++readOpNdx)
            {
                const OperationName writeOp   = s_writeOps[writeOpNdx];
                const OperationName readOp    = s_readOps[readOpNdx];
                const std::string opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
                bool empty                    = true;

                de::MovePtr<tcu::TestCaseGroup> opGroup(new tcu::TestCaseGroup(testCtx, opGroupName.c_str()));

                for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
                {
                    const ResourceDescription &resource = s_resources[resourceNdx];
                    std::string name                    = getResourceName(resource);

                    if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
                    {
                        if (data.type == SynchronizationType::SYNCHRONIZATION2)
                        {
                            if ((isSpecializedAccessFlagSupported(writeOp) || isSpecializedAccessFlagSupported(readOp)))
                            {
                                const std::string nameSp = name + "_specialized_access_flag";
                                opGroup->addChild(new SyncTestCase(testCtx, nameSp, data.type,
                                                                   groups[groupNdx].syncPrimitive, resource, writeOp,
                                                                   readOp, true, *data.pipelineCacheData, false));
                            }
#ifndef CTS_USES_VULKANSC
                            if (groups[groupNdx].syncPrimitive == SYNC_PRIMITIVE_EVENT)
                            {
                                const std::string nameSp = name + "_maintenance9";
                                opGroup->addChild(new SyncTestCase(testCtx, nameSp, data.type,
                                                                   groups[groupNdx].syncPrimitive, resource, writeOp,
                                                                   readOp, false, *data.pipelineCacheData, true));
                            }
#endif
                        }

                        opGroup->addChild(new SyncTestCase(testCtx, name, data.type, groups[groupNdx].syncPrimitive,
                                                           resource, writeOp, readOp, false, *data.pipelineCacheData,
                                                           false));

                        empty = false;
                    }
                }
                if (!empty)
                    synchGroup->addChild(opGroup.release());
            }

        group->addChild(synchGroup.release());
    }

    createMultipleEventsTests(group, data);
}

} // namespace

tcu::TestCaseGroup *createSynchronizedOperationSingleQueueTests(tcu::TestContext &testCtx, SynchronizationType type,
                                                                PipelineCacheData &pipelineCacheData)
{
    TestData data{type, &pipelineCacheData};

    // Synchronization of a memory-modifying operation
    return createTestGroup(testCtx, "single_queue", createTests, data);
}

} // namespace synchronization
} // namespace vkt
