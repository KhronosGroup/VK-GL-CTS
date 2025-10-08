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
 * \brief Synchronization primitive tests with multi queue
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationOperationMultiQueueTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPlatform.hpp"
#include "vkCmdUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "tcuTestLog.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationOperationTestData.hpp"
#include "vktSynchronizationOperationResources.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuCommandLine.hpp"

#include <set>
#include <unordered_map>

namespace vkt
{

namespace synchronization
{

namespace
{
using namespace vk;
using de::MovePtr;
using de::SharedPtr;
using de::UniquePtr;

enum QueueType
{
    QUEUETYPE_WRITE,
    QUEUETYPE_READ
};

struct QueuePair
{
    QueuePair(const uint32_t familyWrite, const uint32_t familyRead, const VkQueue write, const VkQueue read)
        : familyIndexWrite(familyWrite)
        , familyIndexRead(familyRead)
        , queueWrite(write)
        , queueRead(read)
    {
    }

    uint32_t familyIndexWrite;
    uint32_t familyIndexRead;
    VkQueue queueWrite;
    VkQueue queueRead;
};

struct Queue
{
    Queue(const uint32_t familyOp, const VkQueue queueOp) : family(familyOp), queue(queueOp)
    {
    }

    uint32_t family;
    VkQueue queue;
};

bool checkQueueFlags(VkQueueFlags availableFlags, const VkQueueFlags neededFlags)
{
    if ((availableFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0)
        availableFlags |= VK_QUEUE_TRANSFER_BIT;

    return ((availableFlags & neededFlags) == neededFlags);
}

class MultiQueues
{
    struct QueueData
    {
        VkQueueFlags flags;
        std::vector<VkQueue> queue;
    };

    MultiQueues(Context &context, SynchronizationType type, bool timelineSemaphore, bool maintenance8)
#ifdef CTS_USES_VULKANSC
        : m_instance(createCustomInstanceFromContext(context))
        ,
#else
        :
#endif // CTS_USES_VULKANSC
        m_queueCount(0)
    {
#ifdef CTS_USES_VULKANSC
        const InstanceInterface &instanceDriver = m_instance.getDriver();
        const VkPhysicalDevice physicalDevice =
            chooseDevice(instanceDriver, m_instance, context.getTestContext().getCommandLine());
        const VkInstance instance = m_instance;
#else
        const InstanceInterface &instanceDriver = context.getInstanceInterface();
        const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();
        const VkInstance instance               = context.getInstance();
#endif // CTS_USES_VULKANSC
        const std::vector<VkQueueFamilyProperties> queueFamilyProperties =
            getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

        for (uint32_t queuePropertiesNdx = 0; queuePropertiesNdx < queueFamilyProperties.size(); ++queuePropertiesNdx)
        {
            addQueueIndex(queuePropertiesNdx, std::min(2u, queueFamilyProperties[queuePropertiesNdx].queueCount),
                          queueFamilyProperties[queuePropertiesNdx].queueFlags);
        }

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        const float queuePriorities[2] = {1.0f, 1.0f}; //get max 2 queues from one family

        for (std::map<uint32_t, QueueData>::iterator it = m_queues.begin(); it != m_queues.end(); ++it)
        {
            const VkDeviceQueueCreateInfo queueInfo = {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     //VkStructureType sType;
                nullptr,                                        //const void* pNext;
                (VkDeviceQueueCreateFlags)0u,                   //VkDeviceQueueCreateFlags flags;
                it->first,                                      //uint32_t queueFamilyIndex;
                static_cast<uint32_t>(it->second.queue.size()), //uint32_t queueCount;
                &queuePriorities[0]                             //const float* pQueuePriorities;
            };
            queueInfos.push_back(queueInfo);
        }

        {
            VkPhysicalDeviceFeatures2 createPhysicalFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr,
                                                            context.getDeviceFeatures()};
            VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, nullptr, true};
            VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, nullptr, true};
            void **nextPtr = &createPhysicalFeature.pNext;

            std::vector<const char *> deviceExtensions;
            if (timelineSemaphore)
            {
                if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_timeline_semaphore"))
                    deviceExtensions.push_back("VK_KHR_timeline_semaphore");
                addToChainVulkanStructure(&nextPtr, timelineSemaphoreFeatures);
            }
            if (type == SynchronizationType::SYNCHRONIZATION2)
            {
                deviceExtensions.push_back("VK_KHR_synchronization2");
                addToChainVulkanStructure(&nextPtr, synchronization2Features);
            }
            if (maintenance8)
                deviceExtensions.push_back("VK_KHR_maintenance8");

            void *pNext = &createPhysicalFeature;
#ifdef CTS_USES_VULKANSC
            VkDeviceObjectReservationCreateInfo memReservationInfo =
                context.getTestContext().getCommandLine().isSubProcess() ?
                    context.getResourceInterface()->getStatMax() :
                    resetDeviceObjectReservationCreateInfo();
            memReservationInfo.pNext = pNext;
            pNext                    = &memReservationInfo;

            VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
            sc10Features.pNext                              = pNext;
            pNext                                           = &sc10Features;

            VkPipelineCacheCreateInfo pcCI;
            std::vector<VkPipelinePoolSize> poolSizes;
            if (context.getTestContext().getCommandLine().isSubProcess())
            {
                if (context.getResourceInterface()->getCacheDataSize() > 0)
                {
                    pcCI = {
                        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                        nullptr,                                      // const void* pNext;
                        VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                            VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                        context.getResourceInterface()->getCacheDataSize(),       // uintptr_t initialDataSize;
                        context.getResourceInterface()->getCacheData()            // const void* pInitialData;
                    };
                    memReservationInfo.pipelineCacheCreateInfoCount = 1;
                    memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
                }

                poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
                if (!poolSizes.empty())
                {
                    memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
                    memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
                }
            }
#endif // CTS_USES_VULKANSC

            const VkDeviceCreateInfo deviceInfo = {
                VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,           //VkStructureType sType;
                pNext,                                          //const void* pNext;
                0u,                                             //VkDeviceCreateFlags flags;
                static_cast<uint32_t>(queueInfos.size()),       //uint32_t queueCreateInfoCount;
                &queueInfos[0],                                 //const VkDeviceQueueCreateInfo* pQueueCreateInfos;
                0u,                                             //uint32_t enabledLayerCount;
                nullptr,                                        //const char* const* ppEnabledLayerNames;
                static_cast<uint32_t>(deviceExtensions.size()), //uint32_t enabledExtensionCount;
                deviceExtensions.empty() ? nullptr : &deviceExtensions[0], //const char* const* ppEnabledExtensionNames;
                nullptr //const VkPhysicalDeviceFeatures* pEnabledFeatures;
            };

            m_logicalDevice = createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                                                 context.getPlatformInterface(), instance, instanceDriver,
                                                 physicalDevice, &deviceInfo);
#ifndef CTS_USES_VULKANSC
            m_deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), instance,
                                                                        *m_logicalDevice, context.getUsedApiVersion(),
                                                                        context.getTestContext().getCommandLine()));
#else
            m_deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
                new DeviceDriverSC(context.getPlatformInterface(), instance, *m_logicalDevice,
                                                 context.getTestContext().getCommandLine(), context.getResourceInterface(),
                                                 context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(),
                                                 context.getUsedApiVersion()),
                vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *m_logicalDevice));
#endif // CTS_USES_VULKANSC
            m_allocator = MovePtr<Allocator>(new SimpleAllocator(
                *m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instanceDriver, physicalDevice)));

            for (std::map<uint32_t, QueueData>::iterator it = m_queues.begin(); it != m_queues.end(); ++it)
                for (int queueNdx = 0; queueNdx < static_cast<int>(it->second.queue.size()); ++queueNdx)
                    m_deviceDriver->getDeviceQueue(*m_logicalDevice, it->first, queueNdx, &it->second.queue[queueNdx]);
        }
    }

    void addQueueIndex(const uint32_t queueFamilyIndex, const uint32_t count, const VkQueueFlags flags)
    {
        QueueData dataToPush;
        dataToPush.flags = flags;
        dataToPush.queue.resize(count);
        m_queues[queueFamilyIndex] = dataToPush;

        m_queueCount++;
    }

public:
    ~MultiQueues()
    {
    }

    std::vector<QueuePair> getQueuesPairs(const VkQueueFlags flagsWrite, const VkQueueFlags flagsRead,
                                          bool requireDifferent) const
    {
        std::map<uint32_t, QueueData> queuesWrite;
        std::map<uint32_t, QueueData> queuesRead;
        std::vector<QueuePair> queuesPairs;

        for (std::map<uint32_t, QueueData>::const_iterator it = m_queues.begin(); it != m_queues.end(); ++it)
        {
            const bool writeQueue = checkQueueFlags(it->second.flags, flagsWrite);
            const bool readQueue  = checkQueueFlags(it->second.flags, flagsRead);

            if (!(writeQueue || readQueue))
                continue;

            if (writeQueue && readQueue)
            {
                queuesWrite[it->first] = it->second;
                queuesRead[it->first]  = it->second;
            }
            else if (writeQueue)
                queuesWrite[it->first] = it->second;
            else if (readQueue)
                queuesRead[it->first] = it->second;
        }

        for (std::map<uint32_t, QueueData>::iterator write = queuesWrite.begin(); write != queuesWrite.end(); ++write)
            for (std::map<uint32_t, QueueData>::iterator read = queuesRead.begin(); read != queuesRead.end(); ++read)
            {
                const int writeSize = static_cast<int>(write->second.queue.size());
                const int readSize  = static_cast<int>(read->second.queue.size());

                for (int writeNdx = 0; writeNdx < writeSize; ++writeNdx)
                    for (int readNdx = 0; readNdx < readSize; ++readNdx)
                    {
                        if (write->second.queue[writeNdx] != read->second.queue[readNdx] &&
                            (!requireDifferent || write->first != read->first))
                        {
                            queuesPairs.push_back(QueuePair(write->first, read->first, write->second.queue[writeNdx],
                                                            read->second.queue[readNdx]));
                            writeNdx = readNdx = std::max(writeSize, readSize); //exit from the loops
                        }
                    }
            }

        if (queuesPairs.empty())
            TCU_THROW(NotSupportedError, "Queue not found");

        return queuesPairs;
    }

    Queue getDefaultQueue(const VkQueueFlags flagsOp) const
    {
        for (std::map<uint32_t, QueueData>::const_iterator it = m_queues.begin(); it != m_queues.end(); ++it)
        {
            if (checkQueueFlags(it->second.flags, flagsOp))
                return Queue(it->first, it->second.queue[0]);
        }

        TCU_THROW(NotSupportedError, "Queue not found");
    }

    Queue getQueue(const uint32_t familyIdx, const uint32_t queueIdx)
    {
        return Queue(familyIdx, m_queues[familyIdx].queue[queueIdx]);
    }

    VkQueueFlags getQueueFamilyFlags(const uint32_t familyIdx)
    {
        return m_queues[familyIdx].flags;
    }

    uint32_t queueFamilyCount(const uint32_t familyIdx)
    {
        return (uint32_t)m_queues[familyIdx].queue.size();
    }

    uint32_t familyCount(void) const
    {
        return (uint32_t)m_queues.size();
    }

    uint32_t totalQueueCount(void)
    {
        uint32_t count = 0;

        for (uint32_t familyIdx = 0; familyIdx < familyCount(); familyIdx++)
        {
            count += queueFamilyCount(familyIdx);
        }

        return count;
    }

    VkDevice getDevice(void) const
    {
        return *m_logicalDevice;
    }

    const DeviceInterface &getDeviceInterface(void) const
    {
        return *m_deviceDriver;
    }

    Allocator &getAllocator(void)
    {
        return *m_allocator;
    }

    static SharedPtr<MultiQueues> getInstance(Context &context, SynchronizationType type, bool timelineSemaphore,
                                              bool maintenance8)
    {
        uint32_t index = ((uint32_t)type << 2) | ((uint32_t)timelineSemaphore << 1) | ((uint32_t)maintenance8);
        if (!m_multiQueues[index])
            m_multiQueues[index] =
                SharedPtr<MultiQueues>(new MultiQueues(context, type, timelineSemaphore, maintenance8));

        return m_multiQueues[index];
    }
    static void destroy()
    {
        m_multiQueues.clear();
    }

private:
#ifdef CTS_USES_VULKANSC
    CustomInstance m_instance;
#endif // CTS_USES_VULKANSC
    Move<VkDevice> m_logicalDevice;
#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> m_deviceDriver;
#else
    de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter> m_deviceDriver;
#endif // CTS_USES_VULKANSC
    MovePtr<Allocator> m_allocator;
    std::map<uint32_t, QueueData> m_queues;
    uint32_t m_queueCount;

    static std::unordered_map<uint32_t, SharedPtr<MultiQueues>> m_multiQueues;
};
std::unordered_map<uint32_t, SharedPtr<MultiQueues>> MultiQueues::m_multiQueues;

// Record simple pipeline memory barrier between two stages.
void recordSimpleBarrier(SynchronizationWrapperPtr synchronizationWrapper, VkCommandBuffer cmdBuffer,
                         const SyncInfo &writeSync, const SyncInfo &readSync)
{
    const VkMemoryBarrier2 memoryBarrier =
        makeMemoryBarrier2(writeSync.stageMask, writeSync.accessMask, readSync.stageMask, readSync.accessMask);
    const auto dependencyInfo = makeCommonDependencyInfo(&memoryBarrier);
    synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
}

// Records a barrier and returns the semaphore signal or wait pipeline stage flags.
VkPipelineStageFlags2 createBarrierMultiQueue(SynchronizationWrapperPtr synchronizationWrapper,
                                              const VkCommandBuffer &cmdBuffer, const SyncInfo &writeSync,
                                              const SyncInfo &readSync, const Resource &resource,
                                              const uint32_t writeFamily, const uint32_t readFamily,
                                              const VkSharingMode sharingMode, bool useAllStages,
                                              const bool secondQueue = false)
{
    VkPipelineStageFlags2 pipelineFlags =
        (secondQueue ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR : VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);

    if (useAllStages)
        DE_ASSERT(writeFamily != readFamily && VK_SHARING_MODE_EXCLUSIVE == sharingMode);

    if (resource.getType() == RESOURCE_TYPE_IMAGE)
    {
        VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
            secondQueue ?
                (useAllStages ? readSync.stageMask : VkPipelineStageFlags(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)) :
                writeSync.stageMask,
            secondQueue ? 0u : writeSync.accessMask,
            !secondQueue ?
                (useAllStages ? writeSync.stageMask : VkPipelineStageFlags(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)) :
                readSync.stageMask,
            !secondQueue ? 0u : readSync.accessMask, writeSync.imageLayout, readSync.imageLayout,
            resource.getImage().handle, resource.getImage().subresourceRange);

        if (useAllStages)
        {
            DE_ASSERT(imageMemoryBarrier2.srcStageMask == imageMemoryBarrier2.dstStageMask);
            pipelineFlags = imageMemoryBarrier2.srcStageMask;
        }

        if (writeFamily != readFamily && VK_SHARING_MODE_EXCLUSIVE == sharingMode)
        {
            imageMemoryBarrier2.srcQueueFamilyIndex = writeFamily;
            imageMemoryBarrier2.dstQueueFamilyIndex = readFamily;

            VkDependencyInfoKHR dependencyInfo =
                makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2, false, useAllStages);
            synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
        }
        else if (!secondQueue)
        {
            VkDependencyInfoKHR dependencyInfo =
                makeCommonDependencyInfo(nullptr, nullptr, &imageMemoryBarrier2, false, useAllStages);
            synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
        }
    }
    else
    {
        VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
            secondQueue ?
                (useAllStages ? readSync.stageMask : VkPipelineStageFlags(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)) :
                writeSync.stageMask,
            secondQueue ? 0u : writeSync.accessMask,
            !secondQueue ?
                (useAllStages ? writeSync.stageMask : VkPipelineStageFlags(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)) :
                readSync.stageMask,
            !secondQueue ? 0u : readSync.accessMask, resource.getBuffer().handle, resource.getBuffer().offset,
            resource.getBuffer().size);

        if (writeFamily != readFamily && VK_SHARING_MODE_EXCLUSIVE == sharingMode)
        {
            bufferMemoryBarrier2.srcQueueFamilyIndex = writeFamily;
            bufferMemoryBarrier2.dstQueueFamilyIndex = readFamily;
        }

        if (useAllStages)
        {
            DE_ASSERT(bufferMemoryBarrier2.srcStageMask == bufferMemoryBarrier2.dstStageMask);
            pipelineFlags = bufferMemoryBarrier2.srcStageMask;
        }

        VkDependencyInfoKHR dependencyInfo =
            makeCommonDependencyInfo(nullptr, &bufferMemoryBarrier2, nullptr, false, useAllStages);
        synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
    }

    if (useAllStages)
        DE_ASSERT(pipelineFlags != VK_PIPELINE_STAGE_2_NONE_KHR &&
                  pipelineFlags != VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR);

    return pipelineFlags;
}

class BaseTestInstance : public TestInstance
{
public:
    BaseTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                     const OperationSupport &writeOp, const OperationSupport &readOp,
                     PipelineCacheData &pipelineCacheData, bool timelineSemaphore, bool maintenance8,
                     const bool maintenance9)
        : TestInstance(context)
        , m_type(type)
        , m_queues(MultiQueues::getInstance(context, type, timelineSemaphore, maintenance8))
        , m_opContext(new OperationContext(context, type, m_queues->getDeviceInterface(), m_queues->getDevice(),
                                           m_queues->getAllocator(), pipelineCacheData))
        , m_resourceDesc(resourceDesc)
        , m_writeOp(writeOp)
        , m_readOp(readOp)
        , m_maintenance9(maintenance9)
    {
    }

    bool queueFamilyOwnershipTransferRequired(const Resource &resource, const uint32_t qf1, const uint32_t qf2)
    {
#ifndef CTS_USES_VULKANSC
        if (!m_maintenance9)
            return true;

        if (resource.getType() == RESOURCE_TYPE_IMAGE && resource.getImage().tiling == VK_IMAGE_TILING_OPTIMAL)
        {
            if ((m_writeOp.getOutResourceUsageFlags() | m_readOp.getInResourceUsageFlags()) &
                (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT |
                 VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR))
            {
                return true;
            }
            else
            {
                const InstanceInterface &vki   = m_opContext->getInstanceInterface();
                const VkPhysicalDevice physDev = m_opContext->getPhysicalDevice();
                VkQueueFamilyOwnershipTransferPropertiesKHR queueFamilyOwnershipTransferProperties =
                    vk::initVulkanStructure();
                VkQueueFamilyProperties2 queueFamilyProperties =
                    vk::initVulkanStructure(&queueFamilyOwnershipTransferProperties);
                uint32_t count = 1u;
                vki.getPhysicalDeviceQueueFamilyProperties2(physDev, &count, &queueFamilyProperties);
                uint32_t requiredQueueFamilyIndices = qf1 | qf2;
                if ((queueFamilyOwnershipTransferProperties.optimalImageTransferToQueueFamilies &
                     requiredQueueFamilyIndices) != requiredQueueFamilyIndices)
                {
                    return true;
                }
            }
        }
        return false;
#else
        (void)resource;
        (void)qf1;
        (void)qf2;
        return true;
#endif
    }

protected:
    const SynchronizationType m_type;
    const SharedPtr<MultiQueues> m_queues;
    const UniquePtr<OperationContext> m_opContext;
    const ResourceDescription m_resourceDesc;
    const OperationSupport &m_writeOp;
    const OperationSupport &m_readOp;
    const bool m_maintenance9;
};

class BinarySemaphoreTestInstance : public BaseTestInstance
{
public:
    BinarySemaphoreTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                                const OperationSupport &writeOp, const OperationSupport &readOp,
                                PipelineCacheData &pipelineCacheData, const VkSharingMode sharingMode,
                                bool useAllStages, const bool maintenance9)
        : BaseTestInstance(context, type, resourceDesc, writeOp, readOp, pipelineCacheData, false, useAllStages,
                           maintenance9)
        , m_sharingMode(sharingMode)
        , m_useAllStages(useAllStages)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk               = m_opContext->getDeviceInterface();
        const VkDevice device                   = m_opContext->getDevice();
        const std::vector<QueuePair> queuePairs = m_queues->getQueuesPairs(
            m_writeOp.getQueueFlags(*m_opContext), m_readOp.getQueueFlags(*m_opContext), m_useAllStages);

        if (queuePairs.empty())
            TCU_THROW(NotSupportedError, "No suitable queue pairs found");

        for (uint32_t pairNdx = 0; pairNdx < static_cast<uint32_t>(queuePairs.size()); ++pairNdx)
        {
            const UniquePtr<Resource> resource(
                new Resource(*m_opContext, m_resourceDesc,
                             m_writeOp.getOutResourceUsageFlags() | m_readOp.getInResourceUsageFlags()));
            const UniquePtr<Operation> writeOp(m_writeOp.build(*m_opContext, *resource));
            const UniquePtr<Operation> readOp(m_readOp.build(*m_opContext, *resource));

            const Move<VkCommandPool> cmdPool[] = {
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  queuePairs[pairNdx].familyIndexWrite),
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  queuePairs[pairNdx].familyIndexRead)};
            const Move<VkCommandBuffer> ptrCmdBuffer[] = {makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_WRITE]),
                                                          makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_READ])};
            const VkCommandBufferSubmitInfoKHR cmdBufferInfos[] = {
                makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_WRITE]),
                makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_READ]),
            };
            SynchronizationWrapperPtr synchronizationWrapper[]{
                getSynchronizationWrapper(m_type, vk, false),
                getSynchronizationWrapper(m_type, vk, false),
            };

            const SyncInfo writeSync       = writeOp->getOutSyncInfo();
            const SyncInfo readSync        = readOp->getInSyncInfo();
            VkCommandBuffer writeCmdBuffer = cmdBufferInfos[QUEUETYPE_WRITE].commandBuffer;
            VkCommandBuffer readCmdBuffer  = cmdBufferInfos[QUEUETYPE_READ].commandBuffer;

            beginCommandBuffer(vk, writeCmdBuffer);
            writeOp->recordCommands(writeCmdBuffer);
            VkPipelineStageFlags2 writeStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
            bool perform_qfot = queueFamilyOwnershipTransferRequired(*resource, queuePairs[pairNdx].familyIndexWrite,
                                                                     queuePairs[pairNdx].familyIndexRead);
            if (perform_qfot)
            {
                writeStageMask =
                    createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync,
                                            readSync, *resource, queuePairs[pairNdx].familyIndexWrite,
                                            queuePairs[pairNdx].familyIndexRead, m_sharingMode, m_useAllStages);
            }
            else
            {
                writeStageMask = createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer,
                                                         writeSync, readSync, *resource, VK_QUEUE_FAMILY_IGNORED,
                                                         VK_QUEUE_FAMILY_IGNORED, m_sharingMode, m_useAllStages);
            }
            endCommandBuffer(vk, writeCmdBuffer);

            beginCommandBuffer(vk, readCmdBuffer);
            VkPipelineStageFlags2 readStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
            if (perform_qfot)
            {
                readStageMask =
                    createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_READ], readCmdBuffer, writeSync, readSync,
                                            *resource, queuePairs[pairNdx].familyIndexWrite,
                                            queuePairs[pairNdx].familyIndexRead, m_sharingMode, m_useAllStages, true);
            }
            readOp->recordCommands(readCmdBuffer);
            endCommandBuffer(vk, readCmdBuffer);

            const Unique<VkSemaphore> semaphore(createSemaphore(vk, device));

            VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo =
                makeCommonSemaphoreSubmitInfo(*semaphore, 0u, writeStageMask);
            synchronizationWrapper[QUEUETYPE_WRITE]->addSubmitInfo(0u, nullptr, 1u, &cmdBufferInfos[QUEUETYPE_WRITE],
                                                                   1u, &signalSemaphoreSubmitInfo);

            VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
                makeCommonSemaphoreSubmitInfo(*semaphore, 0u, readStageMask);
            synchronizationWrapper[QUEUETYPE_READ]->addSubmitInfo(1u, &waitSemaphoreSubmitInfo, 1u,
                                                                  &cmdBufferInfos[QUEUETYPE_READ], 0u, nullptr);

            VK_CHECK(
                synchronizationWrapper[QUEUETYPE_WRITE]->queueSubmit(queuePairs[pairNdx].queueWrite, VK_NULL_HANDLE));
            VK_CHECK(
                synchronizationWrapper[QUEUETYPE_READ]->queueSubmit(queuePairs[pairNdx].queueRead, VK_NULL_HANDLE));
            VK_CHECK(vk.queueWaitIdle(queuePairs[pairNdx].queueWrite));
            VK_CHECK(vk.queueWaitIdle(queuePairs[pairNdx].queueRead));

            {
                const Data expected = writeOp->getData();
                const Data actual   = readOp->getData();

#ifdef CTS_USES_VULKANSC
                if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
                {
                    if (isIndirectBuffer(m_resourceDesc.type))
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
            }
        }
        return tcu::TestStatus::pass("OK");
    }

protected:
    const VkSharingMode m_sharingMode;
    const bool m_useAllStages;
};

class IntermediateBarrierInstance : public BinarySemaphoreTestInstance
{
public:
    IntermediateBarrierInstance(Context &context, const ResourceDescription &resourceDesc,
                                const OperationSupport &writeOp, const OperationSupport &readOp,
                                const OperationSupport &extraReadOp, const OperationSupport &extraWriteOp,
                                PipelineCacheData &pipelineCacheData, const bool maintenance9)
        : BinarySemaphoreTestInstance(context, SynchronizationType::SYNCHRONIZATION2, resourceDesc, writeOp, readOp,
                                      pipelineCacheData, VK_SHARING_MODE_EXCLUSIVE, true, maintenance9)
        , m_extraReadOp(extraReadOp)
        , m_extraWriteOp(extraWriteOp)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk = m_opContext->getDeviceInterface();
        const VkDevice device     = m_opContext->getDevice();
        const auto queueFlagsWrite =
            (m_writeOp.getQueueFlags(*m_opContext) | m_extraReadOp.getQueueFlags(*m_opContext));
        const auto queueFlagsRead = (m_readOp.getQueueFlags(*m_opContext) | m_extraWriteOp.getQueueFlags(*m_opContext));
        const std::vector<QueuePair> queuePairs =
            m_queues->getQueuesPairs(queueFlagsWrite, queueFlagsRead, m_useAllStages);

        if (queuePairs.empty())
            TCU_THROW(NotSupportedError, "No suitable queue pairs found");

        for (uint32_t pairNdx = 0; pairNdx < static_cast<uint32_t>(queuePairs.size()); ++pairNdx)
        {
            // Resources.
            const UniquePtr<Resource> resource(
                new Resource(*m_opContext, m_resourceDesc,
                             m_writeOp.getOutResourceUsageFlags() | m_readOp.getInResourceUsageFlags()));
            const UniquePtr<Resource> extraReadResource(
                new Resource(*m_opContext, m_resourceDesc, m_extraReadOp.getInResourceUsageFlags()));
            const UniquePtr<Resource> extraWriteResource(
                new Resource(*m_opContext, m_resourceDesc, m_extraWriteOp.getOutResourceUsageFlags()));

            // Operations.
            const UniquePtr<Operation> writeOp(m_writeOp.build(*m_opContext, *resource));
            const UniquePtr<Operation> readOp(m_readOp.build(*m_opContext, *resource));

            const UniquePtr<Operation> extraReadOp(m_extraReadOp.build(*m_opContext, *extraReadResource));
            const UniquePtr<Operation> extraWriteOp(m_extraWriteOp.build(*m_opContext, *extraWriteResource));

            const Move<VkCommandPool> cmdPool[] = {
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  queuePairs[pairNdx].familyIndexWrite),
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  queuePairs[pairNdx].familyIndexRead)};
            const Move<VkCommandBuffer> ptrCmdBuffer[] = {makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_WRITE]),
                                                          makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_READ])};
            const VkCommandBufferSubmitInfoKHR cmdBufferInfos[] = {
                makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_WRITE]),
                makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_READ]),
            };
            SynchronizationWrapperPtr synchronizationWrapper[]{
                getSynchronizationWrapper(m_type, vk, false),
                getSynchronizationWrapper(m_type, vk, false),
            };

            const SyncInfo writeSync       = writeOp->getOutSyncInfo();
            const SyncInfo readSync        = readOp->getInSyncInfo();
            const SyncInfo extraReadSync   = extraReadOp->getInSyncInfo();
            const SyncInfo extraWriteSync  = extraWriteOp->getOutSyncInfo();
            VkCommandBuffer writeCmdBuffer = cmdBufferInfos[QUEUETYPE_WRITE].commandBuffer;
            VkCommandBuffer readCmdBuffer  = cmdBufferInfos[QUEUETYPE_READ].commandBuffer;

            // Transition extra resource images to the general layout.
            if (m_resourceDesc.type == RESOURCE_TYPE_IMAGE)
            {
                // Write queue chosen arbitrarily. Note we'll wait for the operation to complete in any case.
                const auto layoutCmdBuffer = makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_WRITE]);
                const auto cmdBuffer       = *layoutCmdBuffer;
                DE_ASSERT(m_type == SynchronizationType::SYNCHRONIZATION2);

                const std::vector<Resource *> resourceVec{extraReadResource.get(), extraWriteResource.get()};
                std::vector<VkImageMemoryBarrier2KHR> barriers;
                barriers.reserve(resourceVec.size());

                beginCommandBuffer(vk, cmdBuffer);
                for (const auto resourceItem : resourceVec)
                {
                    const VkImageMemoryBarrier2KHR barrier = {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
                        nullptr,
                        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                        0u,
                        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
                        0u,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL,
                        queuePairs[pairNdx].familyIndexWrite,
                        queuePairs[pairNdx].familyIndexWrite,
                        resourceItem->getImage().handle,
                        resourceItem->getImage().subresourceRange,
                    };
                    barriers.push_back(barrier);
                }
                const VkDependencyInfoKHR dependencyInfo = {
                    VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
                    nullptr,
                    0u,
                    0u,
                    nullptr,
                    0u,
                    nullptr,
                    de::sizeU32(barriers),
                    de::dataOrNull(barriers),
                };
#ifndef CTS_USES_VULKANSC
                vk.cmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
#else
                vk.cmdPipelineBarrier2KHR(cmdBuffer, &dependencyInfo);
#endif // CTS_USES_VULKANSC
                endCommandBuffer(vk, cmdBuffer);
                submitCommandsAndWait(vk, device, queuePairs[pairNdx].queueWrite, cmdBuffer);
            }

            beginCommandBuffer(vk, writeCmdBuffer);

            writeOp->recordCommands(writeCmdBuffer);

            // Transfer ownership of the shared resource from the write queue to the read queue, at the write stage.
            createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync, readSync,
                                    *resource, queuePairs[pairNdx].familyIndexWrite,
                                    queuePairs[pairNdx].familyIndexRead, m_sharingMode, m_useAllStages);

            // At this point, create a simple barrier from the first write stage to the extra read stage.
            // Then, record the reading commands from the extra read resource. Note this resource is not used for
            // anything, but we will pretend we will be using it for something.
            recordSimpleBarrier(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync, extraReadSync);
            extraReadOp->recordCommands(writeCmdBuffer);

            endCommandBuffer(vk, writeCmdBuffer);

            beginCommandBuffer(vk, readCmdBuffer);

            // At this point, pretend to do something first with an extra write before reading the shared resource.
            // Then, create a simple barrier from this extra write stage to the actual read stage.
            extraWriteOp->recordCommands(readCmdBuffer);
            recordSimpleBarrier(synchronizationWrapper[QUEUETYPE_READ], readCmdBuffer, extraWriteSync, readSync);

            // Receiving end of the ownership transfer, at the read stage.
            createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_READ], readCmdBuffer, writeSync, readSync,
                                    *resource, queuePairs[pairNdx].familyIndexWrite,
                                    queuePairs[pairNdx].familyIndexRead, m_sharingMode, m_useAllStages, true);
            readOp->recordCommands(readCmdBuffer);
            endCommandBuffer(vk, readCmdBuffer);

            const Unique<VkSemaphore> semaphore(createSemaphore(vk, device));

            // Semaphore signals late, at the extra read stage.
            VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo =
                makeCommonSemaphoreSubmitInfo(*semaphore, 0u, extraReadSync.stageMask);

            // Semaphore waits early, at the extra write stage.
            VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
                makeCommonSemaphoreSubmitInfo(*semaphore, 0u, extraWriteSync.stageMask);

            synchronizationWrapper[QUEUETYPE_WRITE]->addSubmitInfo(0u, nullptr, 1u, &cmdBufferInfos[QUEUETYPE_WRITE],
                                                                   1u, &signalSemaphoreSubmitInfo);
            synchronizationWrapper[QUEUETYPE_READ]->addSubmitInfo(1u, &waitSemaphoreSubmitInfo, 1u,
                                                                  &cmdBufferInfos[QUEUETYPE_READ], 0u, nullptr);

            VK_CHECK(
                synchronizationWrapper[QUEUETYPE_WRITE]->queueSubmit(queuePairs[pairNdx].queueWrite, VK_NULL_HANDLE));
            VK_CHECK(
                synchronizationWrapper[QUEUETYPE_READ]->queueSubmit(queuePairs[pairNdx].queueRead, VK_NULL_HANDLE));
            VK_CHECK(vk.queueWaitIdle(queuePairs[pairNdx].queueWrite));
            VK_CHECK(vk.queueWaitIdle(queuePairs[pairNdx].queueRead));

            {
                const Data expected = writeOp->getData();
                const Data actual   = readOp->getData();

#ifdef CTS_USES_VULKANSC
                if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
                {
                    if (isIndirectBuffer(m_resourceDesc.type))
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
            }
        }

        return tcu::TestStatus::pass("OK");
    }

protected:
    const OperationSupport &m_extraReadOp;
    const OperationSupport &m_extraWriteOp;
};

template <typename T>
inline SharedPtr<Move<T>> makeVkSharedPtr(Move<T> move)
{
    return SharedPtr<Move<T>>(new Move<T>(move));
}

class TimelineSemaphoreTestInstance : public BaseTestInstance
{
public:
    TimelineSemaphoreTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                                  const SharedPtr<OperationSupport> &writeOp, const SharedPtr<OperationSupport> &readOp,
                                  PipelineCacheData &pipelineCacheData, const VkSharingMode sharingMode,
                                  const bool maintenance9)
        : BaseTestInstance(context, type, resourceDesc, *writeOp, *readOp, pipelineCacheData, true, false, maintenance9)
        , m_sharingMode(sharingMode)
    {
        uint32_t maxQueues = 0;
        std::vector<uint32_t> queueFamilies;

        if (m_queues->totalQueueCount() < 2)
            TCU_THROW(NotSupportedError, "Not enough queues");

        for (uint32_t familyNdx = 0; familyNdx < m_queues->familyCount(); familyNdx++)
        {
            maxQueues = std::max(m_queues->queueFamilyCount(familyNdx), maxQueues);
            queueFamilies.push_back(familyNdx);
        }

        // Create a chain of operations copying data from one resource
        // to another across at least every single queue of the system
        // at least once. Each of the operation will be executing with
        // a dependency on the previous using timeline points.
        m_opSupports.push_back(writeOp);
        m_opQueues.push_back(m_queues->getDefaultQueue(writeOp->getQueueFlags(*m_opContext)));

        for (uint32_t queueIdx = 0; queueIdx < maxQueues; queueIdx++)
        {
            for (uint32_t familyIdx = 0; familyIdx < m_queues->familyCount(); familyIdx++)
            {
                for (uint32_t copyOpIdx = 0; copyOpIdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpIdx++)
                {
                    if (isResourceSupported(s_copyOps[copyOpIdx], resourceDesc))
                    {
                        SharedPtr<OperationSupport> opSupport(
                            makeOperationSupport(s_copyOps[copyOpIdx], m_resourceDesc).release());

                        if (!checkQueueFlags(m_queues->getQueueFamilyFlags(familyIdx),
                                             opSupport->getQueueFlags(*m_opContext)))
                            continue;

                        m_opSupports.push_back(opSupport);
                        m_opQueues.push_back(
                            m_queues->getQueue(familyIdx, queueIdx % m_queues->queueFamilyCount(familyIdx)));
                        break;
                    }
                }
            }
        }

        m_opSupports.push_back(readOp);
        m_opQueues.push_back(m_queues->getDefaultQueue(readOp->getQueueFlags(*m_opContext)));

        // Now create the resources with the usage associated to the
        // operation performed on the resource.
        for (uint32_t opIdx = 0; opIdx < (m_opSupports.size() - 1); opIdx++)
        {
            uint32_t usage =
                m_opSupports[opIdx]->getOutResourceUsageFlags() | m_opSupports[opIdx + 1]->getInResourceUsageFlags();

            m_resources.push_back(
                SharedPtr<Resource>(new Resource(*m_opContext, m_resourceDesc, usage, m_sharingMode, queueFamilies)));
        }

        // Finally create the operations using the resources.
        m_ops.push_back(SharedPtr<Operation>(m_opSupports[0]->build(*m_opContext, *m_resources[0]).release()));
        for (uint32_t opIdx = 1; opIdx < (m_opSupports.size() - 1); opIdx++)
            m_ops.push_back(SharedPtr<Operation>(
                m_opSupports[opIdx]->build(*m_opContext, *m_resources[opIdx - 1], *m_resources[opIdx]).release()));
        m_ops.push_back(SharedPtr<Operation>(
            m_opSupports[m_opSupports.size() - 1]->build(*m_opContext, *m_resources.back()).release()));
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk = m_opContext->getDeviceInterface();
        const VkDevice device     = m_opContext->getDevice();
        de::Random rng(1234);
        const Unique<VkSemaphore> semaphore(createSemaphoreType(vk, device, VK_SEMAPHORE_TYPE_TIMELINE));
        std::vector<SharedPtr<Move<VkCommandPool>>> cmdPools;
        std::vector<SharedPtr<Move<VkCommandBuffer>>> ptrCmdBuffers;
        std::vector<VkCommandBufferSubmitInfoKHR> cmdBufferInfos;
        std::vector<uint64_t> timelineValues;

        cmdPools.resize(m_queues->familyCount());
        for (uint32_t familyIdx = 0; familyIdx < m_queues->familyCount(); familyIdx++)
            cmdPools[familyIdx] = makeVkSharedPtr(
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, familyIdx));

        ptrCmdBuffers.resize(m_ops.size());
        cmdBufferInfos.resize(m_ops.size());
        for (uint32_t opIdx = 0; opIdx < m_ops.size(); opIdx++)
        {
            uint64_t increment = 1 + rng.getUint8();

            ptrCmdBuffers[opIdx] = makeVkSharedPtr(makeCommandBuffer(vk, device, **cmdPools[m_opQueues[opIdx].family]));
            cmdBufferInfos[opIdx] = makeCommonCommandBufferSubmitInfo(**ptrCmdBuffers[opIdx]);

            timelineValues.push_back(timelineValues.empty() ? increment : (timelineValues.back() + increment));
        }

        for (uint32_t opIdx = 0; opIdx < m_ops.size(); opIdx++)
        {
            VkCommandBuffer cmdBuffer                        = cmdBufferInfos[opIdx].commandBuffer;
            VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo = makeCommonSemaphoreSubmitInfo(
                *semaphore, (opIdx == 0 ? 0u : timelineValues[opIdx - 1]), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
            VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo = makeCommonSemaphoreSubmitInfo(
                *semaphore, timelineValues[opIdx], VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
            SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_type, vk, true);

            synchronizationWrapper->addSubmitInfo(opIdx == 0 ? 0u : 1u, &waitSemaphoreSubmitInfo, 1u,
                                                  &cmdBufferInfos[opIdx], 1u, &signalSemaphoreSubmitInfo,
                                                  opIdx == 0 ? false : true, true);

            beginCommandBuffer(vk, cmdBuffer);

            if (opIdx > 0)
            {
                const SyncInfo writeSync = m_ops[opIdx - 1]->getOutSyncInfo();
                const SyncInfo readSync  = m_ops[opIdx]->getInSyncInfo();
                const Resource &resource = *m_resources[opIdx - 1].get();

                bool perform_qfot = queueFamilyOwnershipTransferRequired(resource, m_opQueues[opIdx - 1].family,
                                                                         m_opQueues[opIdx].family);
                if (perform_qfot)
                {
                    createBarrierMultiQueue(synchronizationWrapper, cmdBuffer, writeSync, readSync, resource,
                                            m_opQueues[opIdx - 1].family, m_opQueues[opIdx].family, m_sharingMode,
                                            false, true);
                }
                else
                {
                    createBarrierMultiQueue(synchronizationWrapper, cmdBuffer, writeSync, readSync, resource,
                                            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_sharingMode, false,
                                            true);
                }
            }

            m_ops[opIdx]->recordCommands(cmdBuffer);

            if (opIdx < (m_ops.size() - 1))
            {
                const SyncInfo writeSync = m_ops[opIdx]->getOutSyncInfo();
                const SyncInfo readSync  = m_ops[opIdx + 1]->getInSyncInfo();
                const Resource &resource = *m_resources[opIdx].get();

                bool perform_qfot = queueFamilyOwnershipTransferRequired(resource, m_opQueues[opIdx].family,
                                                                         m_opQueues[opIdx + 1].family);
                if (perform_qfot)
                {
                    createBarrierMultiQueue(synchronizationWrapper, cmdBuffer, writeSync, readSync, resource,
                                            m_opQueues[opIdx].family, m_opQueues[opIdx + 1].family, m_sharingMode,
                                            false);
                }
                else
                {
                    createBarrierMultiQueue(synchronizationWrapper, cmdBuffer, writeSync, readSync, resource,
                                            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_sharingMode, false);
                }
            }

            endCommandBuffer(vk, cmdBuffer);

            VK_CHECK(synchronizationWrapper->queueSubmit(m_opQueues[opIdx].queue, VK_NULL_HANDLE));
        }

        VK_CHECK(vk.queueWaitIdle(m_opQueues.back().queue));

        {
            const Data expected = m_ops.front()->getData();
            const Data actual   = m_ops.back()->getData();

            if (isIndirectBuffer(m_resourceDesc.type))
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

        // Make the validation layers happy.
        for (uint32_t opIdx = 0; opIdx < m_opQueues.size(); opIdx++)
            VK_CHECK(vk.queueWaitIdle(m_opQueues[opIdx].queue));

        return tcu::TestStatus::pass("OK");
    }

private:
    const VkSharingMode m_sharingMode;
    std::vector<SharedPtr<OperationSupport>> m_opSupports;
    std::vector<SharedPtr<Operation>> m_ops;
    std::vector<SharedPtr<Resource>> m_resources;
    std::vector<Queue> m_opQueues;
};

class FenceTestInstance : public BaseTestInstance
{
public:
    FenceTestInstance(Context &context, SynchronizationType type, const ResourceDescription &resourceDesc,
                      const OperationSupport &writeOp, const OperationSupport &readOp,
                      PipelineCacheData &pipelineCacheData, const VkSharingMode sharingMode, const bool maintenance9)
        : BaseTestInstance(context, type, resourceDesc, writeOp, readOp, pipelineCacheData, false, false, maintenance9)
        , m_sharingMode(sharingMode)
    {
    }

    tcu::TestStatus iterate(void)
    {
        const DeviceInterface &vk               = m_opContext->getDeviceInterface();
        const VkDevice device                   = m_opContext->getDevice();
        const std::vector<QueuePair> queuePairs = m_queues->getQueuesPairs(m_writeOp.getQueueFlags(*m_opContext),
                                                                           m_readOp.getQueueFlags(*m_opContext), false);

        for (uint32_t pairNdx = 0; pairNdx < static_cast<uint32_t>(queuePairs.size()); ++pairNdx)
        {
            const UniquePtr<Resource> resource(
                new Resource(*m_opContext, m_resourceDesc,
                             m_writeOp.getOutResourceUsageFlags() | m_readOp.getInResourceUsageFlags()));
            const UniquePtr<Operation> writeOp(m_writeOp.build(*m_opContext, *resource));
            const UniquePtr<Operation> readOp(m_readOp.build(*m_opContext, *resource));
            const Move<VkCommandPool> cmdPool[]{
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  queuePairs[pairNdx].familyIndexWrite),
                createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  queuePairs[pairNdx].familyIndexRead)};
            const Move<VkCommandBuffer> ptrCmdBuffer[]{makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_WRITE]),
                                                       makeCommandBuffer(vk, device, *cmdPool[QUEUETYPE_READ])};
            const VkCommandBufferSubmitInfoKHR cmdBufferInfos[]{
                makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_WRITE]),
                makeCommonCommandBufferSubmitInfo(*ptrCmdBuffer[QUEUETYPE_READ])};
            SynchronizationWrapperPtr synchronizationWrapper[]{
                getSynchronizationWrapper(m_type, vk, false),
                getSynchronizationWrapper(m_type, vk, false),
            };
            const SyncInfo writeSync       = writeOp->getOutSyncInfo();
            const SyncInfo readSync        = readOp->getInSyncInfo();
            VkCommandBuffer writeCmdBuffer = cmdBufferInfos[QUEUETYPE_WRITE].commandBuffer;
            VkCommandBuffer readCmdBuffer  = cmdBufferInfos[QUEUETYPE_READ].commandBuffer;

            beginCommandBuffer(vk, writeCmdBuffer);
            writeOp->recordCommands(writeCmdBuffer);

            bool perform_qfot = queueFamilyOwnershipTransferRequired(*resource, queuePairs[pairNdx].familyIndexWrite,
                                                                     queuePairs[pairNdx].familyIndexRead);
            if (perform_qfot)
            {
                createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync, readSync,
                                        *resource, queuePairs[pairNdx].familyIndexWrite,
                                        queuePairs[pairNdx].familyIndexRead, m_sharingMode, false);
            }
            else
            {
                createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_WRITE], writeCmdBuffer, writeSync, readSync,
                                        *resource, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_sharingMode,
                                        false);
            }
            endCommandBuffer(vk, writeCmdBuffer);

            submitCommandsAndWait(synchronizationWrapper[QUEUETYPE_WRITE], vk, device, queuePairs[pairNdx].queueWrite,
                                  writeCmdBuffer);

            beginCommandBuffer(vk, readCmdBuffer);
            if (perform_qfot)
            {
                createBarrierMultiQueue(synchronizationWrapper[QUEUETYPE_READ], readCmdBuffer, writeSync, readSync,
                                        *resource, queuePairs[pairNdx].familyIndexWrite,
                                        queuePairs[pairNdx].familyIndexRead, m_sharingMode, false, true);
            }
            readOp->recordCommands(readCmdBuffer);
            endCommandBuffer(vk, readCmdBuffer);

            submitCommandsAndWait(synchronizationWrapper[QUEUETYPE_READ], vk, device, queuePairs[pairNdx].queueRead,
                                  readCmdBuffer);

            {
                const Data expected = writeOp->getData();
                const Data actual   = readOp->getData();

#ifdef CTS_USES_VULKANSC
                if (m_context.getTestContext().getCommandLine().isSubProcess())
#endif // CTS_USES_VULKANSC
                {
                    if (isIndirectBuffer(m_resourceDesc.type))
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
            }
        }
        return tcu::TestStatus::pass("OK");
    }

private:
    const VkSharingMode m_sharingMode;
};

class BaseTestCase : public TestCase
{
public:
    BaseTestCase(tcu::TestContext &testCtx, const std::string &name, SynchronizationType type,
                 const SyncPrimitive syncPrimitive, const ResourceDescription resourceDesc, const OperationName writeOp,
                 const OperationName readOp, const VkSharingMode sharingMode, const bool maintenance9,
                 PipelineCacheData &pipelineCacheData, bool useAllStages)
        : TestCase(testCtx, name)
        , m_type(type)
        , m_resourceDesc(resourceDesc)
        , m_writeOp(makeOperationSupport(writeOp, resourceDesc).release())
        , m_readOp(makeOperationSupport(readOp, resourceDesc).release())
        , m_syncPrimitive(syncPrimitive)
        , m_sharingMode(sharingMode)
        , m_maintenance9(maintenance9)
        , m_pipelineCacheData(pipelineCacheData)
        , m_useAllStages(useAllStages)
    {
        if (m_useAllStages)
        {
            DE_ASSERT(type == SynchronizationType::SYNCHRONIZATION2);
            DE_ASSERT(syncPrimitive ==
                      SYNC_PRIMITIVE_BINARY_SEMAPHORE);          // Not *required* but we'll restrict cases to this.
            DE_ASSERT(sharingMode == VK_SHARING_MODE_EXCLUSIVE); // These cases are about QFOT.
        }
    }

    void initPrograms(SourceCollections &programCollection) const override
    {
        m_writeOp->initPrograms(programCollection);
        m_readOp->initPrograms(programCollection);

        if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE)
        {
            for (uint32_t copyOpNdx = 0; copyOpNdx < DE_LENGTH_OF_ARRAY(s_copyOps); copyOpNdx++)
            {
                if (isResourceSupported(s_copyOps[copyOpNdx], m_resourceDesc))
                    makeOperationSupport(s_copyOps[copyOpNdx], m_resourceDesc)->initPrograms(programCollection);
            }
        }
    }

    void checkImageResourceSupport(const InstanceInterface &vki, const VkPhysicalDevice &physicalDevice,
                                   uint32_t usage) const
    {
        VkImageFormatProperties imageFormatProperties;

        const VkResult formatResult = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_resourceDesc.imageFormat, m_resourceDesc.imageType, VK_IMAGE_TILING_OPTIMAL, usage,
            (VkImageCreateFlags)0, &imageFormatProperties);

        if (formatResult != VK_SUCCESS)
            TCU_THROW(NotSupportedError, "Image format is not supported");

        if ((imageFormatProperties.sampleCounts & m_resourceDesc.imageSamples) != m_resourceDesc.imageSamples)
            TCU_THROW(NotSupportedError, "Requested sample count is not supported");
    }

    void checkSupport(Context &context) const override
    {
        if (m_type == SynchronizationType::SYNCHRONIZATION2)
            context.requireDeviceFunctionality("VK_KHR_synchronization2");
        if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE)
            context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");

        const InstanceInterface &instance     = context.getInstanceInterface();
        const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
        const std::vector<VkQueueFamilyProperties> queueFamilyProperties =
            getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);
        if (m_sharingMode == VK_SHARING_MODE_CONCURRENT && queueFamilyProperties.size() < 2)
            TCU_THROW(NotSupportedError, "Concurrent requires more than 1 queue family");

        if (m_syncPrimitive == SYNC_PRIMITIVE_TIMELINE_SEMAPHORE &&
            !context.getTimelineSemaphoreFeatures().timelineSemaphore)
            TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

        if (m_resourceDesc.type == RESOURCE_TYPE_IMAGE)
        {
            const uint32_t usage = (m_writeOp->getOutResourceUsageFlags() | m_readOp->getInResourceUsageFlags());
            checkImageResourceSupport(instance, physicalDevice, usage);
        }

        if (m_useAllStages)
            context.requireDeviceFunctionality("VK_KHR_maintenance8");

        if (m_maintenance9)
            context.requireDeviceFunctionality("VK_KHR_maintenance9");
    }

    TestInstance *createInstance(Context &context) const override
    {
        switch (m_syncPrimitive)
        {
        case SYNC_PRIMITIVE_FENCE:
            return new FenceTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp, m_pipelineCacheData,
                                         m_sharingMode, m_maintenance9);
        case SYNC_PRIMITIVE_BINARY_SEMAPHORE:
            return new BinarySemaphoreTestInstance(context, m_type, m_resourceDesc, *m_writeOp, *m_readOp,
                                                   m_pipelineCacheData, m_sharingMode, m_useAllStages, m_maintenance9);
        case SYNC_PRIMITIVE_TIMELINE_SEMAPHORE:
            return new TimelineSemaphoreTestInstance(context, m_type, m_resourceDesc, m_writeOp, m_readOp,
                                                     m_pipelineCacheData, m_sharingMode, m_maintenance9);
        default:
            DE_ASSERT(0);
            return nullptr;
        }
    }

protected:
    const SynchronizationType m_type;
    const ResourceDescription m_resourceDesc;
    const SharedPtr<OperationSupport> m_writeOp;
    const SharedPtr<OperationSupport> m_readOp;
    const SyncPrimitive m_syncPrimitive;
    const VkSharingMode m_sharingMode;
    const bool m_maintenance9;
    PipelineCacheData &m_pipelineCacheData;
    const bool m_useAllStages;
};

class IntermediateBarrierCase : public BaseTestCase
{
public:
    IntermediateBarrierCase(tcu::TestContext &testCtx, const std::string &name, const ResourceDescription resourceDesc,
                            const OperationName writeOp, const OperationName readOp, const OperationName extraReadOp,
                            const OperationName extraWriteOp, PipelineCacheData &pipelineCacheData,
                            const bool maintenance9)
        : BaseTestCase(testCtx, name, SynchronizationType::SYNCHRONIZATION2, SYNC_PRIMITIVE_BINARY_SEMAPHORE,
                       resourceDesc, writeOp, readOp, VK_SHARING_MODE_EXCLUSIVE, maintenance9, pipelineCacheData, true)
        , m_extraReadOp(makeOperationSupport(extraReadOp, resourceDesc).release())
        , m_extraWriteOp(makeOperationSupport(extraWriteOp, resourceDesc).release())
    {
    }

    void initPrograms(SourceCollections &programCollection) const
    {
        m_writeOp->initPrograms(programCollection);
        m_readOp->initPrograms(programCollection);
        m_extraReadOp->initPrograms(programCollection);
        m_extraWriteOp->initPrograms(programCollection);
    }

    void checkSupport(Context &context) const
    {
        DE_ASSERT(m_useAllStages);
        context.requireDeviceFunctionality("VK_KHR_maintenance8");

        DE_ASSERT(m_type == SynchronizationType::SYNCHRONIZATION2);
        context.requireDeviceFunctionality("VK_KHR_synchronization2");

        DE_ASSERT(m_syncPrimitive == SYNC_PRIMITIVE_BINARY_SEMAPHORE);

        const InstanceInterface &vki   = context.getInstanceInterface();
        const VkPhysicalDevice physDev = context.getPhysicalDevice();

        DE_ASSERT(m_sharingMode == VK_SHARING_MODE_EXCLUSIVE);

        if (m_resourceDesc.type == RESOURCE_TYPE_IMAGE)
        {
            const uint32_t sharedUsage = (m_writeOp->getOutResourceUsageFlags() | m_readOp->getInResourceUsageFlags());
            checkImageResourceSupport(vki, physDev, sharedUsage);

            const uint32_t extraReadUsage = m_extraReadOp->getInResourceUsageFlags();
            checkImageResourceSupport(vki, physDev, extraReadUsage);

            const uint32_t extraWriteUsage = m_extraWriteOp->getOutResourceUsageFlags();
            checkImageResourceSupport(vki, physDev, extraWriteUsage);
        }

        if (m_maintenance9)
            context.requireDeviceFunctionality("VK_KHR_maintenance9");
    }

    TestInstance *createInstance(Context &context) const
    {
        return new IntermediateBarrierInstance(context, m_resourceDesc, *m_writeOp, *m_readOp, *m_extraReadOp,
                                               *m_extraWriteOp, m_pipelineCacheData, m_maintenance9);
    }

protected:
    const SharedPtr<OperationSupport> m_extraReadOp;
    const SharedPtr<OperationSupport> m_extraWriteOp;
};

struct TestData
{
    SynchronizationType type;
    PipelineCacheData *pipelineCacheData;
};

void createTests(tcu::TestCaseGroup *group, TestData data)
{
    tcu::TestContext &testCtx = group->getTestContext();

    static const struct
    {
        const char *name;
        SyncPrimitive syncPrimitive;
        int numOptions;
    } groups[] = {{"fence", SYNC_PRIMITIVE_FENCE, 1},
                  {"binary_semaphore", SYNC_PRIMITIVE_BINARY_SEMAPHORE, 1},
                  {"timeline_semaphore", SYNC_PRIMITIVE_TIMELINE_SEMAPHORE, 1}};

    for (int groupNdx = 0; groupNdx < DE_LENGTH_OF_ARRAY(groups); ++groupNdx)
    {
        MovePtr<tcu::TestCaseGroup> synchGroup(new tcu::TestCaseGroup(testCtx, groups[groupNdx].name));

        for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(s_writeOps); ++writeOpNdx)
            for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(s_readOps); ++readOpNdx)
            {
                const OperationName writeOp   = s_writeOps[writeOpNdx];
                const OperationName readOp    = s_readOps[readOpNdx];
                const std::string opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
                bool empty                    = true;

                MovePtr<tcu::TestCaseGroup> opGroup(new tcu::TestCaseGroup(testCtx, opGroupName.c_str()));

                for (int optionNdx = 0; optionNdx <= groups[groupNdx].numOptions; ++optionNdx)
                    for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
                        for (const bool useAllStages : {false, true})
                        {
                            const ResourceDescription &resource = s_resources[resourceNdx];
                            if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
                            {
                                std::string name          = getResourceName(resource);
                                VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                                // queue family sharing mode used for resource
                                if (optionNdx)
                                {
                                    name += "_concurrent";
                                    sharingMode = VK_SHARING_MODE_CONCURRENT;
                                }
                                else
                                    name += "_exclusive";

                                if (useAllStages)
                                {
#ifdef CTS_USES_VULKANSC
                                    // VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR is not
                                    // available in VulkanSC.
                                    continue;
#endif // CTS_USES_VULKANSC
                                    if (data.type != SynchronizationType::SYNCHRONIZATION2)
                                        continue;

                                    if (groups[groupNdx].syncPrimitive != SYNC_PRIMITIVE_BINARY_SEMAPHORE)
                                        continue;

                                    if (sharingMode != VK_SHARING_MODE_EXCLUSIVE)
                                        continue;

                                    if (resource.type != RESOURCE_TYPE_IMAGE && resource.type != RESOURCE_TYPE_BUFFER)
                                        continue;

                                    // This OP has an invalid write pipeline stage for our use case.
                                    if (writeOp == OPERATION_NAME_WRITE_CLEAR_ATTACHMENTS)
                                        continue;

                                    name += "_use_all_stages";
                                }

                                opGroup->addChild(new BaseTestCase(
                                    testCtx, name, data.type, groups[groupNdx].syncPrimitive, resource, writeOp, readOp,
                                    sharingMode, false, *data.pipelineCacheData, useAllStages));

#ifndef CTS_USES_VULKANSC
                                if (sharingMode == VK_SHARING_MODE_CONCURRENT)
                                {
                                    name += "_maintenance9";
                                    opGroup->addChild(new BaseTestCase(
                                        testCtx, name, data.type, groups[groupNdx].syncPrimitive, resource, writeOp,
                                        readOp, sharingMode, true, *data.pipelineCacheData, useAllStages));
                                }
#endif
                                empty = false;
                            }
                        }
                if (!empty)
                    synchGroup->addChild(opGroup.release());
            }
        group->addChild(synchGroup.release());
    }

#ifndef CTS_USES_VULKANSC
    // VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR is not available in VulkanSC.

    if (data.type == SynchronizationType::SYNCHRONIZATION2)
    {
        // We'll use a subset of operations and resources for the extra stages to avoid combinatorial explosions.

        const std::vector<OperationName> extraWriteStages{
            OPERATION_NAME_WRITE_FILL_BUFFER,       OPERATION_NAME_WRITE_BLIT_IMAGE,
            OPERATION_NAME_WRITE_SSBO_FRAGMENT,     OPERATION_NAME_WRITE_SSBO_COMPUTE,
            OPERATION_NAME_WRITE_IMAGE_VERTEX,      OPERATION_NAME_WRITE_IMAGE_FRAGMENT,
            OPERATION_NAME_WRITE_CLEAR_COLOR_IMAGE, OPERATION_NAME_WRITE_DRAW_INDEXED,
        };

        const std::vector<OperationName> extraReadStages{
            OPERATION_NAME_READ_COPY_BUFFER,
            OPERATION_NAME_READ_UBO_VERTEX,
            OPERATION_NAME_READ_UBO_FRAGMENT,
            OPERATION_NAME_READ_UBO_COMPUTE,
            OPERATION_NAME_READ_IMAGE_FRAGMENT,
            OPERATION_NAME_READ_IMAGE_COMPUTE,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH,
            OPERATION_NAME_READ_VERTEX_INPUT,
            OPERATION_NAME_READ_INDEX_INPUT,
        };

        const std::vector<ResourceDescription> resourceDescriptions{
            {RESOURCE_TYPE_BUFFER, tcu::IVec4(0x4000, 0, 0, 0), vk::VK_IMAGE_TYPE_LAST, vk::VK_FORMAT_UNDEFINED,
             (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT}, // 16 KiB (min max UBO range)

            {RESOURCE_TYPE_IMAGE, tcu::IVec4(128, 128, 0, 0), vk::VK_IMAGE_TYPE_2D, vk::VK_FORMAT_R8G8B8A8_UNORM,
             vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_SAMPLE_COUNT_1_BIT},

            {RESOURCE_TYPE_INDIRECT_BUFFER_DRAW, tcu::IVec4(sizeof(vk::VkDrawIndirectCommand), 0, 0, 0),
             vk::VK_IMAGE_TYPE_LAST, vk::VK_FORMAT_UNDEFINED, (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT},
            {RESOURCE_TYPE_INDIRECT_BUFFER_DRAW_INDEXED, tcu::IVec4(sizeof(vk::VkDrawIndexedIndirectCommand), 0, 0, 0),
             vk::VK_IMAGE_TYPE_LAST, vk::VK_FORMAT_UNDEFINED, (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT},
            {RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH, tcu::IVec4(sizeof(vk::VkDispatchIndirectCommand), 0, 0, 0),
             vk::VK_IMAGE_TYPE_LAST, vk::VK_FORMAT_UNDEFINED, (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT},
            {RESOURCE_TYPE_INDEX_BUFFER, tcu::IVec4(sizeof(uint32_t) * 5, 0, 0, 0), vk::VK_IMAGE_TYPE_LAST,
             vk::VK_FORMAT_UNDEFINED, (vk::VkImageAspectFlags)0, vk::VK_SAMPLE_COUNT_1_BIT},
        };

        const auto groupName = "intermediate_barrier_use_all";
        MovePtr<tcu::TestCaseGroup> interBarrierGroup(new tcu::TestCaseGroup(testCtx, groupName));

        for (const auto &resource : resourceDescriptions)
        {
            for (int writeOpNdx = 0; writeOpNdx < DE_LENGTH_OF_ARRAY(s_writeOps); ++writeOpNdx)
            {
                const OperationName writeOp = s_writeOps[writeOpNdx];
                if (!isResourceSupported(writeOp, resource))
                    continue;

                for (int readOpNdx = 0; readOpNdx < DE_LENGTH_OF_ARRAY(s_readOps); ++readOpNdx)
                {
                    const OperationName readOp = s_readOps[readOpNdx];
                    if (!isResourceSupported(readOp, resource))
                        continue;

                    const std::string opGroupName =
                        getOperationName(writeOp) + "_" + getOperationName(readOp) + "_" + getResourceName(resource);
                    bool empty = true;

                    MovePtr<tcu::TestCaseGroup> opGroup(new tcu::TestCaseGroup(testCtx, opGroupName.c_str()));

                    for (const auto extraReadOp : extraReadStages)
                    {
                        if (extraReadOp == readOp)
                            continue;

                        if (!isResourceSupported(extraReadOp, resource))
                            continue;

                        for (const auto extraWriteOp : extraWriteStages)
                        {
                            if (extraWriteOp == writeOp)
                                continue;

                            if (!isResourceSupported(extraWriteOp, resource))
                                continue;

                            std::string caseName = getOperationName(extraReadOp) + "_" + getOperationName(extraWriteOp);

                            opGroup->addChild(new IntermediateBarrierCase(testCtx, caseName, resource, writeOp, readOp,
                                                                          extraReadOp, extraWriteOp,
                                                                          *data.pipelineCacheData, false));

#ifndef CTS_USES_VULKANSC
                            caseName += "_maintenance9";
                            opGroup->addChild(new IntermediateBarrierCase(testCtx, caseName, resource, writeOp, readOp,
                                                                          extraReadOp, extraWriteOp,
                                                                          *data.pipelineCacheData, true));
#endif

                            empty = false;
                        }
                    }

                    if (!empty)
                        interBarrierGroup->addChild(opGroup.release());
                }
            }
        }
        group->addChild(interBarrierGroup.release());
    }
#endif // CTS_USES_VULKANSC
}

void cleanupGroup(tcu::TestCaseGroup *group, TestData data)
{
    DE_UNREF(group);
    DE_UNREF(data.pipelineCacheData);
    // Destroy singleton object
    MultiQueues::destroy();
}

} // namespace

tcu::TestCaseGroup *createSynchronizedOperationMultiQueueTests(tcu::TestContext &testCtx, SynchronizationType type,
                                                               PipelineCacheData &pipelineCacheData)
{
    TestData data{type, &pipelineCacheData};

    // Synchronization of a memory-modifying operation
    return createTestGroup(testCtx, "multi_queue", createTests, data, cleanupGroup);
}

} // namespace synchronization
} // namespace vkt
