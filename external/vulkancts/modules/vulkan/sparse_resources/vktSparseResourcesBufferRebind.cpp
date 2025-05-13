/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \file  vktSparseResourcesBufferRebind.cpp
 * \brief Sparse buffer memory rebind tests
 *
 * Summary of the test:
 *
 * Creates a sparse buffer and two backing device memory objects.
 * 1) Binds the first memory fully to the buffer and fill it with data.
 * 2) Binds the second memory fully to the buffer and fill it with different data.
 * 3) Binds the first memory partially, starting with a offset of one page and with half size.
 * 4) Copies data out of sparse buffer into host accessible buffer.
 * 5) verifies if the data in the host accesible buffer is correct.
 *
 * For example, with buffer of size 256KB an aligment of 64 KB, the final binding will be:
 *
 *  256 KB
 * +----------------------------------------------+
 * | buffer                                       |
 * +-----------+----------------------+-----------+
 * | memory 2  | memory 1             | memory 2  |
 * +-----------+----------------------+-----------+
 *   64 KB       128 KB                 64 KB
 *
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferRebind.hpp"
#include "deDefs.h"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

constexpr uint32_t kMemoryPatterns[] = {0xAAAAAAAAu, 0x55555555u};

class BufferSparseRebindCase : public TestCase
{
public:
    BufferSparseRebindCase(tcu::TestContext &testCtx, const std::string &name, const uint32_t bufferSize,
                           const bool useDeviceGroups);

    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const uint32_t m_bufferSize;
    const bool m_useDeviceGroups;
};

BufferSparseRebindCase::BufferSparseRebindCase(tcu::TestContext &testCtx, const std::string &name,
                                               const uint32_t bufferSize, const bool useDeviceGroups)
    : TestCase(testCtx, name)
    , m_bufferSize(bufferSize)
    , m_useDeviceGroups(useDeviceGroups)
{
}

void BufferSparseRebindCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
}

class BufferSparseRebindInstance : public SparseResourcesBaseInstance
{
public:
    BufferSparseRebindInstance(Context &context, const uint32_t bufferSize, const bool useDeviceGroups);

    tcu::TestStatus iterate(void);

private:
    const uint32_t m_bufferSize;
    const uint32_t m_useDeviceGroups;
};

BufferSparseRebindInstance::BufferSparseRebindInstance(Context &context, const uint32_t bufferSize,
                                                       const bool useDeviceGroups)
    : SparseResourcesBaseInstance(context, useDeviceGroups)
    , m_bufferSize(bufferSize)
    , m_useDeviceGroups(useDeviceGroups)
{
}

tcu::TestStatus BufferSparseRebindInstance::iterate(void)
{
    const InstanceInterface &instance = m_context.getInstanceInterface();
    {
        // Create logical device supporting both sparse and transfer operations
        QueueRequirementsVec queueRequirements;
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_TRANSFER_BIT, 1u));

        createDeviceSupportingQueues(queueRequirements);
    }
    const vk::VkPhysicalDevice &physicalDevice = getPhysicalDevice();
    const DeviceInterface &deviceInterface     = getDeviceInterface();
    const Queue &sparseQueue                   = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
    const Queue &transferQueue                 = getQueue(VK_QUEUE_TRANSFER_BIT, 0);

    // Go through all physical devices
    for (uint32_t physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
    {
        const uint32_t firstDeviceID  = physDevID;
        const uint32_t secondDeviceID = (firstDeviceID + 1) % m_numPhysicalDevices;

        VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                // VkStructureType sType;
            nullptr,                                                             // const void* pNext;
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT,                                 // VkBufferCreateFlags flags;
            m_bufferSize,                                                        // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                           // VkSharingMode sharingMode;
            0u,                                                                  // uint32_t queueFamilyIndexCount;
            nullptr                                                              // const uint32_t* pQueueFamilyIndices;
        };

        const uint32_t queueFamilyIndices[] = {sparseQueue.queueFamilyIndex, transferQueue.queueFamilyIndex};

        if (sparseQueue.queueFamilyIndex != transferQueue.queueFamilyIndex)
        {
            bufferCreateInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
            bufferCreateInfo.queueFamilyIndexCount = 2u;
            bufferCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }

        // Create sparse buffer
        const Unique<VkBuffer> sparseBuffer(createBuffer(deviceInterface, getDevice(), &bufferCreateInfo));

        // Create sparse buffer memory bind semaphore
        const Unique<VkSemaphore> bufferMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

        const VkMemoryRequirements bufferMemRequirements =
            getBufferMemoryRequirements(deviceInterface, getDevice(), *sparseBuffer);

        if (bufferMemRequirements.size >
            getPhysicalDeviceProperties(instance, physicalDevice).limits.sparseAddressSpaceSize)
            TCU_THROW(NotSupportedError, "Required memory size for sparse resources exceeds device limits");

        DE_ASSERT((bufferMemRequirements.size % bufferMemRequirements.alignment) == 0);

        // Mark as not supported if the buffer is too small and cannot be backed by two memories
        if ((bufferMemRequirements.size / bufferMemRequirements.alignment) < 2)
        {
            TCU_THROW(NotSupportedError, "Buffer size is too small for partial binding");
        }

        const VkDeviceSize partialBindOffset = bufferMemRequirements.alignment;
        const VkDeviceSize partialBindSize   = bufferMemRequirements.size / 2;

        const uint32_t memoryType = findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID),
                                                           bufferMemRequirements, MemoryRequirement::Any);

        if (memoryType == NO_MATCH_FOUND)
        {
            return tcu::TestStatus::fail("No matching memory type found");
        }

        if (firstDeviceID != secondDeviceID)
        {
            VkPeerMemoryFeatureFlags peerMemoryFeatureFlags = (VkPeerMemoryFeatureFlags)0;
            const uint32_t heapIndex =
                getHeapIndexForMemoryType(instance, getPhysicalDevice(secondDeviceID), memoryType);
            deviceInterface.getDeviceGroupPeerMemoryFeatures(getDevice(), heapIndex, firstDeviceID, secondDeviceID,
                                                             &peerMemoryFeatureFlags);

            if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) == 0) ||
                ((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) == 0))
            {
                TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC and GENERIC_DST");
            }
        }

        // Create two memories which will be backing the buffer
        const VkSparseMemoryBind sparseMemoryBindFull[] = {
            makeSparseMemoryBind(deviceInterface, getDevice(), bufferMemRequirements.size, memoryType, 0u),
            makeSparseMemoryBind(deviceInterface, getDevice(), bufferMemRequirements.size, memoryType, 0u)};

        std::vector<Move<VkDeviceMemory>> deviceMemories;
        deviceMemories.push_back(Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBindFull[0].memory),
                                                      Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr)));
        deviceMemories.push_back(Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBindFull[1].memory),
                                                      Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr)));

        // Command pool for command buffers used by the test
        const Unique<VkCommandPool> commandPool(
            makeCommandPool(deviceInterface, getDevice(), transferQueue.queueFamilyIndex));
        const VkPipelineStageFlags waitStageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};

        // Bind the memories fully and fill them with a pattern
        for (uint32_t memoryIdx = 0; memoryIdx < 2; memoryIdx++)
        {
            // First bind the memory
            VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo = {
                *sparseBuffer,                   // VkBuffer buffer;
                1u,                              // uint32_t bindCount;
                &sparseMemoryBindFull[memoryIdx] // const VkSparseMemoryBind* pBinds;
            };

            const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo = {
                VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, // VkStructureType sType;
                nullptr,                                         // const void* pNext;
                firstDeviceID,                                   // uint32_t resourceDeviceIndex;
                secondDeviceID,                                  // uint32_t memoryDeviceIndex;
            };

            const VkBindSparseInfo bindSparseInfo = {
                VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,                    // VkStructureType sType;
                m_useDeviceGroups ? &devGroupBindSparseInfo : nullptr, // const void* pNext;
                0u,                                                    // uint32_t waitSemaphoreCount;
                nullptr,                                               // const VkSemaphore* pWaitSemaphores;
                1u,                                                    // uint32_t bufferBindCount;
                &sparseBufferMemoryBindInfo,     // const VkSparseBufferMemoryBindInfo* pBufferBinds;
                0u,                              // uint32_t imageOpaqueBindCount;
                nullptr,                         // const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,                              // uint32_t imageBindCount;
                nullptr,                         // const VkSparseImageMemoryBindInfo* pImageBinds;
                1u,                              // uint32_t signalSemaphoreCount;
                &bufferMemoryBindSemaphore.get() // const VkSemaphore* pSignalSemaphores;
            };

            // Submit sparse bind commands for execution
            VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, VK_NULL_HANDLE));

            // And then fill the buffer with data on the device
            const Unique<VkCommandBuffer> commandBufferFill(
                allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

            beginCommandBuffer(deviceInterface, *commandBufferFill);
            deviceInterface.cmdFillBuffer(*commandBufferFill, *sparseBuffer, 0, VK_WHOLE_SIZE,
                                          kMemoryPatterns[memoryIdx]);
            endCommandBuffer(deviceInterface, *commandBufferFill);

            // Wait for the sparse bind operation semaphore, submit and wait on host for the transfer stage
            // In case of device groups, submit on the physical device with the resource
            submitCommandsAndWait(deviceInterface,                  // DeviceInterface&            vk,
                                  getDevice(),                      // VkDevice                    device,
                                  transferQueue.queueHandle,        // VkQueue                    queue,
                                  *commandBufferFill,               // VkCommandBuffer            commandBuffer,
                                  1u,                               // uint32_t                    waitSemaphoreCount,
                                  &bufferMemoryBindSemaphore.get(), // VkSemaphore*                pWaitSemaphores,
                                  waitStageBits,                    // VkPipelineStageFlags*    pWaitDstStageMask,
                                  0,                                // uint32_t                    signalSemaphoreCount,
                                  nullptr,                          // VkSemaphore*                pSignalSemaphores,
                                  m_useDeviceGroups,                // bool                        useDeviceGroups,
                                  firstDeviceID                     // uint32_t                    physicalDeviceID
            );
        }

        // The final binding would be half and half between memory 1 and memory 2 starting with a slight offset from the start
        {
            const VkSparseMemoryBind sparseMemoryBindPartial = {
                partialBindOffset,          // VkDeviceSize resourceOffset;
                partialBindSize,            // VkDeviceSize size;
                *deviceMemories[0],         // VkDeviceMemory memory;
                partialBindOffset,          // VkDeviceSize memoryOffset;
                (VkSparseMemoryBindFlags)0, // VkSparseMemoryBindFlags flags;
            };

            VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo = {
                *sparseBuffer,           // VkBuffer buffer;
                1u,                      // uint32_t bindCount;
                &sparseMemoryBindPartial // const VkSparseMemoryBind* pBinds;
            };

            const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo = {
                VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, // VkStructureType sType;
                nullptr,                                         // const void* pNext;
                firstDeviceID,                                   // uint32_t resourceDeviceIndex;
                secondDeviceID,                                  // uint32_t memoryDeviceIndex;
            };

            const VkBindSparseInfo bindSparseInfo = {
                VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,                    // VkStructureType sType;
                m_useDeviceGroups ? &devGroupBindSparseInfo : nullptr, // const void* pNext;
                0u,                                                    // uint32_t waitSemaphoreCount;
                nullptr,                                               // const VkSemaphore* pWaitSemaphores;
                1u,                                                    // uint32_t bufferBindCount;
                &sparseBufferMemoryBindInfo,     // const VkSparseBufferMemoryBindInfo* pBufferBinds;
                0u,                              // uint32_t imageOpaqueBindCount;
                nullptr,                         // const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,                              // uint32_t imageBindCount;
                nullptr,                         // const VkSparseImageMemoryBindInfo* pImageBinds;
                1u,                              // uint32_t signalSemaphoreCount;
                &bufferMemoryBindSemaphore.get() // const VkSemaphore* pSignalSemaphores;
            };

            // Submit sparse bind commands for execution, no need for a waitSemaphore as the host waited for the previous submit.
            VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, VK_NULL_HANDLE));
        }

        // And verify the result by copying sparse buffer data into a new host-visible buffer

        // Create output buffer
        const VkBufferCreateInfo outputBufferCreateInfo =
            makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const Unique<VkBuffer> outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
        const de::UniquePtr<Allocation> outputBufferAlloc(
            bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

        const Unique<VkCommandBuffer> commandBufferCopy(
            allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        beginCommandBuffer(deviceInterface, *commandBufferCopy);

        {
            const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_bufferSize);

            deviceInterface.cmdCopyBuffer(*commandBufferCopy, *sparseBuffer, *outputBuffer, 1u, &bufferCopy);
        }

        // Make the changes visible to the host
        {
            const VkBufferMemoryBarrier outputBufferHostBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, m_bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBufferCopy, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                                               &outputBufferHostBarrier, 0u, nullptr);
        }

        endCommandBuffer(deviceInterface, *commandBufferCopy);

        // Submit commands for execution and wait for completion
        // In case of device groups, submit on the physical device with the resource
        submitCommandsAndWait(deviceInterface, getDevice(), transferQueue.queueHandle, *commandBufferCopy, 1u,
                              &bufferMemoryBindSemaphore.get(), waitStageBits, 0, nullptr, m_useDeviceGroups,
                              firstDeviceID);

        // Retrieve data from output buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

        const uint32_t *outputData = static_cast<const uint32_t *>(outputBufferAlloc->getHostPtr());

        // Wait for sparse queue to become idle
        deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

        // Prepare reference data
        std::vector<uint32_t> referenceData;
        referenceData.resize(m_bufferSize / sizeof(uint32_t));

        for (uint32_t i = 0; i < referenceData.size(); i++)
        {
            if (i * sizeof(uint32_t) >= partialBindOffset &&
                i * sizeof(uint32_t) < (partialBindOffset + partialBindSize))
            {
                referenceData[i] = kMemoryPatterns[0];
            }
            else
            {
                referenceData[i] = kMemoryPatterns[1];
            }
        }

        // Compare reference data with output data
        if (deMemCmp(&referenceData[0], outputData, m_bufferSize) != 0)
            return tcu::TestStatus::fail("Failed");
    }
    return tcu::TestStatus::pass("Passed");
}

TestInstance *BufferSparseRebindCase::createInstance(Context &context) const
{
    return new BufferSparseRebindInstance(context, m_bufferSize, m_useDeviceGroups);
}

} // namespace

void addBufferSparseRebindTests(tcu::TestCaseGroup *group, const bool useDeviceGroups)
{
    group->addChild(new BufferSparseRebindCase(group->getTestContext(), "buffer_size_2_16", 1 << 16, useDeviceGroups));
    group->addChild(new BufferSparseRebindCase(group->getTestContext(), "buffer_size_2_18", 1 << 18, useDeviceGroups));
    group->addChild(new BufferSparseRebindCase(group->getTestContext(), "buffer_size_2_20", 1 << 20, useDeviceGroups));
    group->addChild(new BufferSparseRebindCase(group->getTestContext(), "buffer_size_2_24", 1 << 24, useDeviceGroups));
}

} // namespace sparse
} // namespace vkt
