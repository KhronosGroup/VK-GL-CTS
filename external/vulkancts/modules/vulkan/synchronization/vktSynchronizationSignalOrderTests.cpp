
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Signal ordering tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationSignalOrderTests.hpp"
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
inline SharedPtr<T> makeSharedPtr(de::MovePtr<T> move)
{
    return SharedPtr<T>(move.release());
}

template <typename T>
inline SharedPtr<T> makeSharedPtr(T *ptr)
{
    return SharedPtr<T>(ptr);
}

void hostSignal(const DeviceInterface &vk, const VkDevice &device, VkSemaphore semaphore, const uint64_t timelineValue)
{
    VkSemaphoreSignalInfoKHR ssi = {
        VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        semaphore,                               // VkSemaphore semaphore;
        timelineValue,                           // uint64_t value;
    };

    VK_CHECK(vk.signalSemaphore(device, &ssi));
}

// Waits for the device to be idle when destroying the guard object.
class DeviceWaitIdleGuard
{
public:
    DeviceWaitIdleGuard(const DeviceInterface &vkd, const VkDevice device) : m_vkd(vkd), m_device(device)
    {
    }

    ~DeviceWaitIdleGuard()
    {
        VK_CHECK(m_vkd.deviceWaitIdle(m_device));
    }

protected:
    const DeviceInterface &m_vkd;
    const VkDevice m_device;
};

Move<VkDevice> createTestDevice(const Context &context)
{
    const float priority = 0.0f;
    const std::vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
    std::vector<uint32_t> queueFamilyIndices(queueFamilyProperties.size(), 0xFFFFFFFFu);
    std::vector<const char *> extensions;

    VkPhysicalDeviceFeatures2 createPhysicalFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr,
                                                    context.getDeviceFeatures()};
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, nullptr, true};
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, nullptr, true};
    void **nextPtr = &createPhysicalFeature.pNext;

    if (context.isDeviceFunctionalitySupported("VK_KHR_timeline_semaphore"))
    {
        extensions.push_back("VK_KHR_timeline_semaphore");
        addToChainVulkanStructure(&nextPtr, timelineSemaphoreFeatures);
    }

    if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_external_semaphore"))
        extensions.push_back("VK_KHR_external_semaphore");
    if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_external_memory"))
        extensions.push_back("VK_KHR_external_memory");

    if (context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore_fd"))
        extensions.push_back("VK_KHR_external_semaphore_fd");

    if (context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore_win32"))
        extensions.push_back("VK_KHR_external_semaphore_win32");

    if (context.isDeviceFunctionalitySupported("VK_KHR_external_memory_win32"))
        extensions.push_back("VK_KHR_external_memory_win32");

    if (context.isDeviceFunctionalitySupported("VK_KHR_synchronization2"))
    {
        extensions.push_back("VK_KHR_synchronization2");
        addToChainVulkanStructure(&nextPtr, synchronization2Features);
    }

    try
    {
        uint32_t maxQueueCount = 1;
        for (const VkQueueFamilyProperties &qfp : queueFamilyProperties)
            maxQueueCount = deMaxu32(qfp.queueCount, maxQueueCount);

        std::vector<float> queuePriorities(maxQueueCount, priority);
        std::vector<VkDeviceQueueCreateInfo> queues;

        for (size_t ndx = 0; ndx < queueFamilyProperties.size(); ndx++)
        {
            const VkDeviceQueueCreateInfo createInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                        nullptr,
                                                        0u,

                                                        (uint32_t)ndx,
                                                        queueFamilyProperties[ndx].queueCount,
                                                        queuePriorities.data()};

            queues.push_back(createInfo);
        }

        const VkDeviceCreateInfo createInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                               &createPhysicalFeature,
                                               0u,

                                               (uint32_t)queues.size(),
                                               &queues[0],

                                               0u,
                                               nullptr,

                                               (uint32_t)extensions.size(),
                                               extensions.empty() ? nullptr : &extensions[0],
                                               0u};

        const auto validation = context.getTestContext().getCommandLine().isValidationEnabled();
        return createCustomDevice(validation, context.getPlatformInterface(), context.getInstance(),
                                  context.getInstanceInterface(), context.getPhysicalDevice(), &createInfo);
    }
    catch (const vk::Error &error)
    {
        if (error.getError() == VK_ERROR_EXTENSION_NOT_PRESENT)
            TCU_THROW(NotSupportedError, "Required extensions not supported");
        else
            throw;
    }
}

// Class to wrap a singleton instance and device
class SingletonDevice
{
    SingletonDevice(const Context &context) : m_logicalDevice(createTestDevice(context))
    {
    }

public:
    static const Unique<vk::VkDevice> &getDevice(const Context &context)
    {
        if (!m_singletonDevice)
            m_singletonDevice = SharedPtr<SingletonDevice>(new SingletonDevice(context));

        DE_ASSERT(m_singletonDevice);
        return m_singletonDevice->m_logicalDevice;
    }

    static void destroy()
    {
        m_singletonDevice.clear();
    }

private:
    const Unique<vk::VkDevice> m_logicalDevice;

    static SharedPtr<SingletonDevice> m_singletonDevice;
};
SharedPtr<SingletonDevice> SingletonDevice::m_singletonDevice;

static void cleanupGroup()
{
    // Destroy singleton object
    SingletonDevice::destroy();
}

class SimpleAllocation : public Allocation
{
public:
    SimpleAllocation(const DeviceInterface &vkd, VkDevice device, const VkDeviceMemory memory);
    ~SimpleAllocation(void);

private:
    const DeviceInterface &m_vkd;
    const VkDevice m_device;
};

SimpleAllocation::SimpleAllocation(const DeviceInterface &vkd, VkDevice device, const VkDeviceMemory memory)
    : Allocation(memory, 0, nullptr)
    , m_vkd(vkd)
    , m_device(device)
{
}

SimpleAllocation::~SimpleAllocation(void)
{
    m_vkd.freeMemory(m_device, getMemory(), nullptr);
}

vk::VkMemoryRequirements getMemoryRequirements(const DeviceInterface &vkd, VkDevice device, VkBuffer buffer)
{
    const VkBufferMemoryRequirementsInfo2 requirementInfo = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
                                                             nullptr, buffer};
    VkMemoryRequirements2 requirements                    = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
                                                             nullptr,
                                                             {
                                              0u,
                                              0u,
                                              0u,
                                          }};
    vkd.getBufferMemoryRequirements2(device, &requirementInfo, &requirements);
    return requirements.memoryRequirements;
}

vk::VkMemoryRequirements getMemoryRequirements(const DeviceInterface &vkd, VkDevice device, VkImage image)
{
    const VkImageMemoryRequirementsInfo2 requirementInfo = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr,
                                                            image};
    VkMemoryRequirements2 requirements                   = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
                                                            nullptr,
                                                            {
                                              0u,
                                              0u,
                                              0u,
                                          }};
    vkd.getImageMemoryRequirements2(device, &requirementInfo, &requirements);

    return requirements.memoryRequirements;
}

MovePtr<Allocation> importAndBindMemory(const DeviceInterface &vkd, VkDevice device, VkBuffer buffer,
                                        NativeHandle &nativeHandle, VkExternalMemoryHandleTypeFlagBits externalType,
                                        const uint32_t exportedMemoryTypeIndex)
{
    const VkMemoryRequirements requirements = getBufferMemoryRequirements(vkd, device, buffer);
    Move<VkDeviceMemory> memory;

    if (!!buffer)
        memory = importDedicatedMemory(vkd, device, buffer, requirements, externalType, exportedMemoryTypeIndex,
                                       nativeHandle);
    else
        memory = importMemory(vkd, device, requirements, externalType, exportedMemoryTypeIndex, nativeHandle);

    VK_CHECK(vkd.bindBufferMemory(device, buffer, *memory, 0u));

    return MovePtr<Allocation>(new SimpleAllocation(vkd, device, memory.disown()));
}

MovePtr<Allocation> importAndBindMemory(const DeviceInterface &vkd, VkDevice device, VkImage image,
                                        NativeHandle &nativeHandle, VkExternalMemoryHandleTypeFlagBits externalType,
                                        uint32_t exportedMemoryTypeIndex)
{
    const VkMemoryRequirements requirements = getImageMemoryRequirements(vkd, device, image);
    Move<VkDeviceMemory> memory;

    if (!!image)
        memory = importDedicatedMemory(vkd, device, image, requirements, externalType, exportedMemoryTypeIndex,
                                       nativeHandle);
    else
        memory = importMemory(vkd, device, requirements, externalType, exportedMemoryTypeIndex, nativeHandle);

    VK_CHECK(vkd.bindImageMemory(device, image, *memory, 0u));

    return MovePtr<Allocation>(new SimpleAllocation(vkd, device, memory.disown()));
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

de::MovePtr<Resource> importResource(const DeviceInterface &vkd, VkDevice device,
                                     const ResourceDescription &resourceDesc, const uint32_t queueFamilyIndex,
                                     const OperationSupport &readOp, const OperationSupport &writeOp,
                                     NativeHandle &nativeHandle, VkExternalMemoryHandleTypeFlagBits externalType,
                                     uint32_t exportedMemoryTypeIndex)
{
    if (resourceDesc.type == RESOURCE_TYPE_IMAGE)
    {
        const VkExtent3D extent = {(uint32_t)resourceDesc.size.x(), de::max(1u, (uint32_t)resourceDesc.size.y()),
                                   de::max(1u, (uint32_t)resourceDesc.size.z())};
        const VkImageSubresourceRange subresourceRange     = {resourceDesc.imageAspect, 0u, 1u, 0u, 1u};
        const VkImageSubresourceLayers subresourceLayers   = {resourceDesc.imageAspect, 0u, 0u, 1u};
        const VkExternalMemoryImageCreateInfo externalInfo = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                                                              nullptr, (VkExternalMemoryHandleTypeFlags)externalType};
        const VkImageTiling tiling                         = VK_IMAGE_TILING_OPTIMAL;
        const VkImageCreateInfo createInfo                 = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                              &externalInfo,
                                                              0u,

                                                              resourceDesc.imageType,
                                                              resourceDesc.imageFormat,
                                                              extent,
                                                              1u,
                                                              1u,
                                                              resourceDesc.imageSamples,
                                                              tiling,
                                                              readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags(),
                                                              VK_SHARING_MODE_EXCLUSIVE,

                                                              1u,
                                                              &queueFamilyIndex,
                                                              VK_IMAGE_LAYOUT_UNDEFINED};

        Move<VkImage> image = createImage(vkd, device, &createInfo);
        MovePtr<Allocation> allocation =
            importAndBindMemory(vkd, device, *image, nativeHandle, externalType, exportedMemoryTypeIndex);

        return MovePtr<Resource>(new Resource(image, allocation, extent, resourceDesc.imageType,
                                              resourceDesc.imageFormat, subresourceRange, subresourceLayers, tiling));
    }
    else
    {
        const VkDeviceSize offset      = 0u;
        const VkDeviceSize size        = static_cast<VkDeviceSize>(resourceDesc.size.x());
        const VkBufferUsageFlags usage = readOp.getInResourceUsageFlags() | writeOp.getOutResourceUsageFlags();
        const VkExternalMemoryBufferCreateInfo externalInfo = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
                                                               nullptr, (VkExternalMemoryHandleTypeFlags)externalType};
        const VkBufferCreateInfo createInfo                 = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                               &externalInfo,
                                                               0u,

                                                               size,
                                                               usage,
                                                               VK_SHARING_MODE_EXCLUSIVE,
                                                               1u,
                                                               &queueFamilyIndex};
        Move<VkBuffer> buffer                               = createBuffer(vkd, device, &createInfo);
        MovePtr<Allocation> allocation =
            importAndBindMemory(vkd, device, *buffer, nativeHandle, externalType, exportedMemoryTypeIndex);

        return MovePtr<Resource>(new Resource(resourceDesc.type, buffer, allocation, offset, size));
    }
}

struct QueueSubmitOrderSharedIteration
{
    QueueSubmitOrderSharedIteration()
    {
    }
    ~QueueSubmitOrderSharedIteration()
    {
    }

    SharedPtr<Resource> resourceA;
    SharedPtr<Resource> resourceB;

    SharedPtr<Operation> writeOp;
    SharedPtr<Operation> readOp;
};

// Verifies the signaling order of the semaphores in multiple
// VkSubmitInfo given to vkQueueSubmit() with queueA & queueB from a
// different VkDevice.
//
// vkQueueSubmit(queueA, [write0, write1, write2, ..., write6])
// vkQueueSubmit(queueB, [read0-6])
//
// With read0-6 waiting on write6, all the data should be available
// for reading given that signal operations are supposed to happen in
// order.
class QueueSubmitSignalOrderSharedTestInstance : public TestInstance
{
public:
    QueueSubmitSignalOrderSharedTestInstance(
        Context &context, SynchronizationType type, const SharedPtr<OperationSupport> writeOpSupport,
        const SharedPtr<OperationSupport> readOpSupport, const ResourceDescription &resourceDesc,
        VkExternalMemoryHandleTypeFlagBits memoryHandleType, VkSemaphoreType semaphoreType,
        VkExternalSemaphoreHandleTypeFlagBits semaphoreHandleType, PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_writeOpSupport(writeOpSupport)
        , m_readOpSupport(readOpSupport)
        , m_resourceDesc(resourceDesc)
        , m_memoryHandleType(memoryHandleType)
        , m_semaphoreType(semaphoreType)
        , m_semaphoreHandleType(semaphoreHandleType)
        , m_pipelineCacheData(pipelineCacheData)
        , m_rng(1234)

    {
        const InstanceInterface &vki                         = context.getInstanceInterface();
        const VkSemaphoreTypeCreateInfoKHR semaphoreTypeInfo = {
            VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR,
            nullptr,
            semaphoreType,
            0,
        };
        const VkPhysicalDeviceExternalSemaphoreInfo info = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
                                                            &semaphoreTypeInfo, semaphoreHandleType};
        VkExternalSemaphoreProperties properties = {VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES, nullptr, 0u, 0u,
                                                    0u};

        vki.getPhysicalDeviceExternalSemaphoreProperties(context.getPhysicalDevice(), &info, &properties);

        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR &&
            !context.getTimelineSemaphoreFeatures().timelineSemaphore)
            TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

        if ((properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT_KHR) == 0 ||
            (properties.externalSemaphoreFeatures & vk::VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHR) == 0)
            TCU_THROW(NotSupportedError, "Exporting and importing semaphore type not supported");

        if (!isResourceExportable())
            TCU_THROW(NotSupportedError, "Resource not exportable");
    }

    Move<VkImage> createImage(const vk::DeviceInterface &vkd, vk::VkDevice device, const vk::VkExtent3D &extent,
                              uint32_t queueFamilyIndex, vk::VkImageTiling tiling)
    {
        const VkExternalMemoryImageCreateInfo externalInfo = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                                                              nullptr,
                                                              (VkExternalMemoryHandleTypeFlags)m_memoryHandleType};
        const VkImageCreateInfo createInfo                 = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                              &externalInfo,
                                                              0u,

                                                              m_resourceDesc.imageType,
                                                              m_resourceDesc.imageFormat,
                                                              extent,
                                                              1u,
                                                              1u,
                                                              m_resourceDesc.imageSamples,
                                                              tiling,
                                                              m_readOpSupport->getInResourceUsageFlags() |
                                                                  m_writeOpSupport->getOutResourceUsageFlags(),
                                                              VK_SHARING_MODE_EXCLUSIVE,

                                                              1u,
                                                              &queueFamilyIndex,
                                                              VK_IMAGE_LAYOUT_UNDEFINED};

        return vk::createImage(vkd, device, &createInfo);
    }

    Move<VkBuffer> createBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device, const vk::VkDeviceSize &size,
                                uint32_t queueFamilyIndex)
    {
        const VkExternalMemoryBufferCreateInfo externalInfo = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
                                                               nullptr,
                                                               (VkExternalMemoryHandleTypeFlags)m_memoryHandleType};
        const VkBufferCreateInfo createInfo                 = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                               &externalInfo,
                                                               0u,

                                                               size,
                                                               m_readOpSupport->getInResourceUsageFlags() |
                                                                   m_writeOpSupport->getOutResourceUsageFlags(),
                                                               VK_SHARING_MODE_EXCLUSIVE,
                                                               1u,
                                                               &queueFamilyIndex};
        return vk::createBuffer(vkd, device, &createInfo);
    }

    tcu::TestStatus iterate(void)
    {
        // We're using 2 devices to make sure we have 2 queues even on
        // implementations that only have a single queue.
        const bool isTimelineSemaphore(m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR);
        const VkDevice &deviceA = m_context.getDevice();
        const Unique<VkDevice> &deviceB(SingletonDevice::getDevice(m_context));
        const DeviceInterface &vkA = m_context.getDeviceInterface();
        const DeviceDriver vkB(m_context.getPlatformInterface(), m_context.getInstance(), *deviceB,
                               m_context.getUsedApiVersion(), m_context.getTestContext().getCommandLine());
        UniquePtr<SimpleAllocator> allocatorA(new SimpleAllocator(
            vkA, deviceA,
            vk::getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice())));
        UniquePtr<SimpleAllocator> allocatorB(new SimpleAllocator(
            vkB, *deviceB,
            vk::getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice())));
        UniquePtr<OperationContext> operationContextA(
            new OperationContext(m_context, m_type, vkA, deviceA, *allocatorA, m_pipelineCacheData));
        UniquePtr<OperationContext> operationContextB(
            new OperationContext(m_context, m_type, vkB, *deviceB, *allocatorB, m_pipelineCacheData));
        const uint32_t universalQueueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        const VkQueue queueA                     = m_context.getUniversalQueue();
        const VkQueue queueB = getDeviceQueue(vkB, *deviceB, m_context.getUniversalQueueFamilyIndex(), 0);
        Unique<VkFence> fenceA(createFence(vkA, deviceA));
        Unique<VkFence> fenceB(createFence(vkB, *deviceB));
        const Unique<VkCommandPool> cmdPoolA(createCommandPool(
            vkA, deviceA, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, universalQueueFamilyIndex));
        const Unique<VkCommandPool> cmdPoolB(createCommandPool(
            vkB, *deviceB, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, universalQueueFamilyIndex));
        std::vector<SharedPtr<Move<VkCommandBuffer>>> ptrCmdBuffersA;
        SharedPtr<Move<VkCommandBuffer>> ptrCmdBufferB;
        std::vector<VkCommandBuffer> cmdBuffersA;
        VkCommandBuffer cmdBufferB;
        std::vector<Move<VkSemaphore>> semaphoresA;
        std::vector<Move<VkSemaphore>> semaphoresB;
        std::vector<VkSemaphore> semaphoreHandlesA;
        std::vector<VkSemaphore> semaphoreHandlesB;
        std::vector<uint64_t> timelineValuesA;
        std::vector<uint64_t> timelineValuesB;
        std::vector<QueueSubmitOrderSharedIteration> iterations(12);
        std::vector<VkPipelineStageFlags2KHR> stageBits;

        // These guards will wait for the device to be idle before tearing down the resources above.
        const DeviceWaitIdleGuard idleGuardA(vkA, deviceA);
        const DeviceWaitIdleGuard idleGuardB(vkB, *deviceB);

        // Create a dozen of set of write/read operations.
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderSharedIteration &iter = iterations[iterIdx];
            uint32_t memoryTypeIndex;
            NativeHandle nativeMemoryHandle;

            if (m_resourceDesc.type == RESOURCE_TYPE_IMAGE)
            {
                const VkExtent3D extent                          = {(uint32_t)m_resourceDesc.size.x(),
                                                                    de::max(1u, (uint32_t)m_resourceDesc.size.y()),
                                                                    de::max(1u, (uint32_t)m_resourceDesc.size.z())};
                const VkImageSubresourceRange subresourceRange   = {m_resourceDesc.imageAspect, 0u, 1u, 0u, 1u};
                const VkImageSubresourceLayers subresourceLayers = {m_resourceDesc.imageAspect, 0u, 0u, 1u};

                const vk::VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
                Move<VkImage> image            = createImage(vkA, deviceA, extent, universalQueueFamilyIndex, tiling);
                const vk::VkMemoryRequirements requirements = getMemoryRequirements(vkA, deviceA, *image);
                memoryTypeIndex                             = chooseMemoryType(requirements.memoryTypeBits);
                vk::Move<vk::VkDeviceMemory> memory         = allocateExportableMemory(
                    vkA, deviceA, requirements.size, memoryTypeIndex, m_memoryHandleType, *image);

                VK_CHECK(vkA.bindImageMemory(deviceA, *image, *memory, 0u));

                MovePtr<Allocation> allocation(new SimpleAllocation(vkA, deviceA, memory.disown()));
                iter.resourceA = makeSharedPtr(new Resource(image, allocation, extent, m_resourceDesc.imageType,
                                                            m_resourceDesc.imageFormat, subresourceRange,
                                                            subresourceLayers, tiling));
            }
            else
            {
                const VkDeviceSize offset = 0u;
                const VkDeviceSize size   = static_cast<VkDeviceSize>(m_resourceDesc.size.x());
                Move<VkBuffer> buffer     = createBuffer(vkA, deviceA, size, universalQueueFamilyIndex);
                const vk::VkMemoryRequirements requirements = getMemoryRequirements(vkA, deviceA, *buffer);
                memoryTypeIndex                             = chooseMemoryType(requirements.memoryTypeBits);
                vk::Move<vk::VkDeviceMemory> memory         = allocateExportableMemory(
                    vkA, deviceA, requirements.size, memoryTypeIndex, m_memoryHandleType, *buffer);

                VK_CHECK(vkA.bindBufferMemory(deviceA, *buffer, *memory, 0u));

                MovePtr<Allocation> allocation(new SimpleAllocation(vkA, deviceA, memory.disown()));
                iter.resourceA = makeSharedPtr(new Resource(m_resourceDesc.type, buffer, allocation, offset, size));
            }

            getMemoryNative(vkA, deviceA, iter.resourceA->getMemory(), m_memoryHandleType, nativeMemoryHandle);
            iter.resourceB = makeSharedPtr(importResource(vkB, *deviceB, m_resourceDesc, universalQueueFamilyIndex,
                                                          *m_readOpSupport, *m_writeOpSupport, nativeMemoryHandle,
                                                          m_memoryHandleType, memoryTypeIndex));

            iter.writeOp = makeSharedPtr(m_writeOpSupport->build(*operationContextA, *iter.resourceA));
            iter.readOp  = makeSharedPtr(m_readOpSupport->build(*operationContextB, *iter.resourceB));
        }

        // Record each write operation into its own command buffer.
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderSharedIteration &iter = iterations[iterIdx];
            const Resource &resource              = *iter.resourceA;
            const SyncInfo writeSync              = iter.writeOp->getOutSyncInfo();
            const SyncInfo readSync               = iter.readOp->getInSyncInfo();

            ptrCmdBuffersA.push_back(makeVkSharedPtr(makeCommandBuffer(vkA, deviceA, *cmdPoolA)));

            cmdBuffersA.push_back(**(ptrCmdBuffersA.back()));

            beginCommandBuffer(vkA, cmdBuffersA.back());

            iter.writeOp->recordCommands(cmdBuffersA.back());

            {
                SynchronizationWrapperPtr synchronizationWrapper =
                    getSynchronizationWrapper(m_type, vkA, isTimelineSemaphore);

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
                    synchronizationWrapper->cmdPipelineBarrier(cmdBuffersA.back(), &dependencyInfo);
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
                    synchronizationWrapper->cmdPipelineBarrier(cmdBuffersA.back(), &dependencyInfo);
                }

                stageBits.push_back(writeSync.stageMask);
            }

            endCommandBuffer(vkA, cmdBuffersA.back());

            addSemaphore(vkA, deviceA, semaphoresA, semaphoreHandlesA, timelineValuesA,
                         iterIdx == (iterations.size() - 1), 2u);
        }

        DE_ASSERT(stageBits.size() == iterations.size());
        DE_ASSERT(semaphoreHandlesA.size() == iterations.size());

        // Record all read operations into a single command buffer and record the union of their stage masks.
        VkPipelineStageFlags2KHR readStages = 0;
        ptrCmdBufferB                       = makeVkSharedPtr(makeCommandBuffer(vkB, *deviceB, *cmdPoolB));
        cmdBufferB                          = **(ptrCmdBufferB);
        beginCommandBuffer(vkB, cmdBufferB);
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderSharedIteration &iter = iterations[iterIdx];
            readStages |= iter.readOp->getInSyncInfo().stageMask;
            iter.readOp->recordCommands(cmdBufferB);
        }
        endCommandBuffer(vkB, cmdBufferB);

        // Export the last semaphore for use on deviceB and create another semaphore to signal on deviceB.
        {
            VkSemaphore lastSemaphoreA = semaphoreHandlesA.back();
            NativeHandle nativeSemaphoreHandle;

            addSemaphore(vkB, *deviceB, semaphoresB, semaphoreHandlesB, timelineValuesB, true, timelineValuesA.back());

            getSemaphoreNative(vkA, deviceA, lastSemaphoreA, m_semaphoreHandleType, nativeSemaphoreHandle);
            importSemaphore(vkB, *deviceB, semaphoreHandlesB.back(), m_semaphoreHandleType, nativeSemaphoreHandle, 0u);

            addSemaphore(vkB, *deviceB, semaphoresB, semaphoreHandlesB, timelineValuesB, false, timelineValuesA.back());
        }

        // Submit writes, each in its own VkSubmitInfo. With binary
        // semaphores, submission don't wait on anything, with
        // timeline semaphores, submissions wait on a host signal
        // operation done below.
        {
            std::vector<VkCommandBufferSubmitInfoKHR> cmdBuffersInfo(iterations.size(),
                                                                     makeCommonCommandBufferSubmitInfo(0u));
            std::vector<VkSemaphoreSubmitInfoKHR> waitSemaphoreSubmitInfos(
                iterations.size(),
                makeCommonSemaphoreSubmitInfo(VK_NULL_HANDLE, 1u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
            std::vector<VkSemaphoreSubmitInfoKHR> signalSemaphoreSubmitInfos(
                iterations.size(),
                makeCommonSemaphoreSubmitInfo(VK_NULL_HANDLE, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));
            SynchronizationWrapperPtr synchronizationWrapper =
                getSynchronizationWrapper(m_type, vkA, isTimelineSemaphore, static_cast<uint32_t>(iterations.size()));

            for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
            {
                waitSemaphoreSubmitInfos[iterIdx].semaphore   = semaphoreHandlesA.front();
                waitSemaphoreSubmitInfos[iterIdx].stageMask   = stageBits[iterIdx];
                signalSemaphoreSubmitInfos[iterIdx].semaphore = semaphoreHandlesA[iterIdx];
                signalSemaphoreSubmitInfos[iterIdx].value     = timelineValuesA[iterIdx];
                cmdBuffersInfo[iterIdx].commandBuffer         = cmdBuffersA[iterIdx];

                synchronizationWrapper->addSubmitInfo(
                    isTimelineSemaphore, isTimelineSemaphore ? &waitSemaphoreSubmitInfos[iterIdx] : nullptr, 1u,
                    &cmdBuffersInfo[iterIdx], 1u, &signalSemaphoreSubmitInfos[iterIdx], isTimelineSemaphore,
                    isTimelineSemaphore);
            }

            VK_CHECK(synchronizationWrapper->queueSubmit(queueA, *fenceA));
        }

        // Submit reads, only waiting waiting on the last write
        // operations, ordering of signaling should guarantee that
        // when read operations kick in all writes have completed.
        {
            VkCommandBufferSubmitInfoKHR cmdBuffersInfo = makeCommonCommandBufferSubmitInfo(cmdBufferB);
            VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
                makeCommonSemaphoreSubmitInfo(semaphoreHandlesB.front(), timelineValuesA.back(), readStages);
            VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo = makeCommonSemaphoreSubmitInfo(
                semaphoreHandlesB.back(), timelineValuesB.back(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
            SynchronizationWrapperPtr synchronizationWrapper =
                getSynchronizationWrapper(m_type, vkB, isTimelineSemaphore);

            synchronizationWrapper->addSubmitInfo(1u, &waitSemaphoreSubmitInfo, 1u, &cmdBuffersInfo, 1u,
                                                  &signalSemaphoreSubmitInfo, isTimelineSemaphore, isTimelineSemaphore);

            VK_CHECK(synchronizationWrapper->queueSubmit(queueB, *fenceB));

            if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR)
            {
                const VkSemaphoreWaitInfo waitInfo = {
                    VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, // VkStructureType sType;
                    nullptr,                               // const void* pNext;
                    0u,                                    // VkSemaphoreWaitFlagsKHR flags;
                    1u,                                    // uint32_t semaphoreCount;
                    &semaphoreHandlesB.back(),             // const VkSemaphore* pSemaphores;
                    &timelineValuesB.back(),               // const uint64_t* pValues;
                };

                // Unblock the whole lot.
                hostSignal(vkA, deviceA, semaphoreHandlesA.front(), 2);

                VK_CHECK(vkB.waitSemaphores(*deviceB, &waitInfo, ~0ull));
            }
            else
            {
                VK_CHECK(vkB.waitForFences(*deviceB, 1, &fenceB.get(), VK_TRUE, ~0ull));
            }
        }

        // Verify the result of the operations.
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderSharedIteration &iter = iterations[iterIdx];
            const Data expected                   = iter.writeOp->getData();
            const Data actual                     = iter.readOp->getData();

            if (isIndirectBuffer(iter.resourceA->getType()))
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
    void addSemaphore(const DeviceInterface &vk, VkDevice device, std::vector<Move<VkSemaphore>> &semaphores,
                      std::vector<VkSemaphore> &semaphoreHandles, std::vector<uint64_t> &timelineValues,
                      bool exportable, uint64_t firstTimelineValue)
    {
        Move<VkSemaphore> semaphore;

        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR)
        {
            // Only allocate a single exportable semaphore.
            if (semaphores.empty())
            {
                semaphores.push_back(createExportableSemaphoreType(vk, device, m_semaphoreType, m_semaphoreHandleType));
            }
        }
        else
        {
            if (exportable)
                semaphores.push_back(createExportableSemaphoreType(vk, device, m_semaphoreType, m_semaphoreHandleType));
            else
                semaphores.push_back(createSemaphoreType(vk, device, m_semaphoreType));
        }

        semaphoreHandles.push_back(*semaphores.back());
        timelineValues.push_back((timelineValues.empty() ? firstTimelineValue : timelineValues.back()) +
                                 m_rng.getInt(1, 100));
    }

    bool isResourceExportable()
    {
        const InstanceInterface &vki    = m_context.getInstanceInterface();
        VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

        if (m_resourceDesc.type == RESOURCE_TYPE_IMAGE)
        {
            const VkPhysicalDeviceExternalImageFormatInfo externalInfo = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO, nullptr, m_memoryHandleType};
            const VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                &externalInfo,
                m_resourceDesc.imageFormat,
                m_resourceDesc.imageType,
                VK_IMAGE_TILING_OPTIMAL,
                m_readOpSupport->getInResourceUsageFlags() | m_writeOpSupport->getOutResourceUsageFlags(),
                0u};
            VkExternalImageFormatProperties externalProperties = {
                VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES, nullptr, {0u, 0u, 0u}};
            VkImageFormatProperties2 formatProperties = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
                                                         &externalProperties,
                                                         {
                                                             {0u, 0u, 0u},
                                                             0u,
                                                             0u,
                                                             0u,
                                                             0u,
                                                         }};

            {
                const VkResult res =
                    vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &formatProperties);

                if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
                    return false;

                VK_CHECK(res); // Check other errors
            }

            if ((externalProperties.externalMemoryProperties.externalMemoryFeatures &
                 VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR) == 0)
                return false;

            if ((externalProperties.externalMemoryProperties.externalMemoryFeatures &
                 VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR) == 0)
                return false;

            return true;
        }
        else
        {
            const VkPhysicalDeviceExternalBufferInfo info = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO, nullptr,

                0u, m_readOpSupport->getInResourceUsageFlags() | m_writeOpSupport->getOutResourceUsageFlags(),
                m_memoryHandleType};
            VkExternalBufferProperties properties = {
                VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES, nullptr, {0u, 0u, 0u}};
            vki.getPhysicalDeviceExternalBufferProperties(physicalDevice, &info, &properties);

            if ((properties.externalMemoryProperties.externalMemoryFeatures &
                 VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR) == 0 ||
                (properties.externalMemoryProperties.externalMemoryFeatures &
                 VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR) == 0)
                return false;

            return true;
        }
    }

    SynchronizationType m_type;
    SharedPtr<OperationSupport> m_writeOpSupport;
    SharedPtr<OperationSupport> m_readOpSupport;
    const ResourceDescription &m_resourceDesc;
    VkExternalMemoryHandleTypeFlagBits m_memoryHandleType;
    VkSemaphoreType m_semaphoreType;
    VkExternalSemaphoreHandleTypeFlagBits m_semaphoreHandleType;
    PipelineCacheData &m_pipelineCacheData;
    de::Random m_rng;
};

class QueueSubmitSignalOrderSharedTestCase : public TestCase
{
public:
    QueueSubmitSignalOrderSharedTestCase(tcu::TestContext &testCtx, SynchronizationType type, const std::string &name,
                                         OperationName writeOp, OperationName readOp,
                                         const ResourceDescription &resourceDesc,
                                         VkExternalMemoryHandleTypeFlagBits memoryHandleType,
                                         VkSemaphoreType semaphoreType,
                                         VkExternalSemaphoreHandleTypeFlagBits semaphoreHandleType,
                                         PipelineCacheData &pipelineCacheData)
        : TestCase(testCtx, name.c_str())
        , m_type(type)
        , m_writeOpSupport(makeOperationSupport(writeOp, resourceDesc).release())
        , m_readOpSupport(makeOperationSupport(readOp, resourceDesc).release())
        , m_resourceDesc(resourceDesc)
        , m_memoryHandleType(memoryHandleType)
        , m_semaphoreType(semaphoreType)
        , m_semaphoreHandleType(semaphoreHandleType)
        , m_pipelineCacheData(pipelineCacheData)
    {
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR &&
            !context.getTimelineSemaphoreFeatures().timelineSemaphore)
            TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

        if ((m_semaphoreHandleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT ||
             m_semaphoreHandleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) &&
            !context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore_fd"))
            TCU_THROW(NotSupportedError, "VK_KHR_external_semaphore_fd not supported");

        if ((m_semaphoreHandleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
             m_semaphoreHandleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT) &&
            !context.isDeviceFunctionalitySupported("VK_KHR_external_semaphore_win32"))
            TCU_THROW(NotSupportedError, "VK_KHR_external_semaphore_win32 not supported");

        if (m_type == SynchronizationType::SYNCHRONIZATION2)
            context.requireDeviceFunctionality("VK_KHR_synchronization2");
    }

    TestInstance *createInstance(Context &context) const
    {
        return new QueueSubmitSignalOrderSharedTestInstance(context, m_type, m_writeOpSupport, m_readOpSupport,
                                                            m_resourceDesc, m_memoryHandleType, m_semaphoreType,
                                                            m_semaphoreHandleType, m_pipelineCacheData);
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
    VkExternalMemoryHandleTypeFlagBits m_memoryHandleType;
    VkSemaphoreType m_semaphoreType;
    VkExternalSemaphoreHandleTypeFlagBits m_semaphoreHandleType;
    PipelineCacheData &m_pipelineCacheData;
};

class QueueSubmitSignalOrderSharedTests : public tcu::TestCaseGroup
{
public:
    QueueSubmitSignalOrderSharedTests(tcu::TestContext &testCtx, SynchronizationType type,
                                      VkSemaphoreType semaphoreType, const char *name)
        : tcu::TestCaseGroup(testCtx, name)
        , m_type(type)
        , m_semaphoreType(semaphoreType)
    {
    }

    void init(void)
    {
        static const OperationName writeOps[] = {
            OPERATION_NAME_WRITE_COPY_BUFFER,
            OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE,
            OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER,
            OPERATION_NAME_WRITE_COPY_IMAGE,
            OPERATION_NAME_WRITE_BLIT_IMAGE,
            OPERATION_NAME_WRITE_SSBO_VERTEX,
            OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL,
            OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION,
            OPERATION_NAME_WRITE_SSBO_GEOMETRY,
            OPERATION_NAME_WRITE_SSBO_FRAGMENT,
            OPERATION_NAME_WRITE_SSBO_COMPUTE,
            OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT,
            OPERATION_NAME_WRITE_IMAGE_VERTEX,
            OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL,
            OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION,
            OPERATION_NAME_WRITE_IMAGE_GEOMETRY,
            OPERATION_NAME_WRITE_IMAGE_FRAGMENT,
            OPERATION_NAME_WRITE_IMAGE_COMPUTE,
            OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT,
        };
        static const OperationName readOps[] = {
            OPERATION_NAME_READ_COPY_BUFFER,
            OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE,
            OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER,
            OPERATION_NAME_READ_COPY_IMAGE,
            OPERATION_NAME_READ_BLIT_IMAGE,
            OPERATION_NAME_READ_UBO_VERTEX,
            OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL,
            OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION,
            OPERATION_NAME_READ_UBO_GEOMETRY,
            OPERATION_NAME_READ_UBO_FRAGMENT,
            OPERATION_NAME_READ_UBO_COMPUTE,
            OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT,
            OPERATION_NAME_READ_SSBO_VERTEX,
            OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL,
            OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION,
            OPERATION_NAME_READ_SSBO_GEOMETRY,
            OPERATION_NAME_READ_SSBO_FRAGMENT,
            OPERATION_NAME_READ_SSBO_COMPUTE,
            OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT,
            OPERATION_NAME_READ_IMAGE_VERTEX,
            OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL,
            OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION,
            OPERATION_NAME_READ_IMAGE_GEOMETRY,
            OPERATION_NAME_READ_IMAGE_FRAGMENT,
            OPERATION_NAME_READ_IMAGE_COMPUTE,
            OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH,
            OPERATION_NAME_READ_VERTEX_INPUT,
        };
        static const struct
        {
            VkExternalMemoryHandleTypeFlagBits memoryType;
            VkExternalSemaphoreHandleTypeFlagBits semaphoreType;
        } exportCases[] = {
            // Only semaphore handle types having reference semantic
            // are valid for this test.
            {
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
            },
            {
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
                VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
            },
            {
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
                VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
            },
        };

        for (uint32_t writeOpIdx = 0; writeOpIdx < DE_LENGTH_OF_ARRAY(writeOps); writeOpIdx++)
            for (uint32_t readOpIdx = 0; readOpIdx < DE_LENGTH_OF_ARRAY(readOps); readOpIdx++)
            {
                const OperationName writeOp   = writeOps[writeOpIdx];
                const OperationName readOp    = readOps[readOpIdx];
                const std::string opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
                bool empty                    = true;

                de::MovePtr<tcu::TestCaseGroup> opGroup(new tcu::TestCaseGroup(m_testCtx, opGroupName.c_str()));

                for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
                {
                    const ResourceDescription &resource = s_resources[resourceNdx];

                    if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
                    {
                        for (uint32_t exportIdx = 0; exportIdx < DE_LENGTH_OF_ARRAY(exportCases); exportIdx++)
                        {
                            std::string caseName = getResourceName(resource) + "_" +
                                                   externalSemaphoreTypeToName(exportCases[exportIdx].semaphoreType);

                            opGroup->addChild(new QueueSubmitSignalOrderSharedTestCase(
                                m_testCtx, m_type, caseName, writeOp, readOp, resource,
                                exportCases[exportIdx].memoryType, m_semaphoreType,
                                exportCases[exportIdx].semaphoreType, m_pipelineCacheData));
                            empty = false;
                        }
                    }
                }
                if (!empty)
                    addChild(opGroup.release());
            }
    }

    void deinit(void)
    {
        cleanupGroup();
    }

private:
    SynchronizationType m_type;
    VkSemaphoreType m_semaphoreType;
    // synchronization.op tests share pipeline cache data to speed up test
    // execution.
    PipelineCacheData m_pipelineCacheData;
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

// Verifies the signaling order of the semaphores in multiple
// VkSubmitInfo given to vkQueueSubmit() with queueA & queueB from the
// same VkDevice.
//
// vkQueueSubmit(queueA, [write0, write1, write2, ..., write6])
// vkQueueSubmit(queueB, [read0-6])
//
// With read0-6 waiting on write6, all the data should be available
// for reading given that signal operations are supposed to happen in
// order.
class QueueSubmitSignalOrderTestInstance : public TestInstance
{
public:
    QueueSubmitSignalOrderTestInstance(Context &context, SynchronizationType type,
                                       const SharedPtr<OperationSupport> writeOpSupport,
                                       const SharedPtr<OperationSupport> readOpSupport,
                                       const ResourceDescription &resourceDesc, VkSemaphoreType semaphoreType,
                                       PipelineCacheData &pipelineCacheData)
        : TestInstance(context)
        , m_type(type)
        , m_writeOpSupport(writeOpSupport)
        , m_readOpSupport(readOpSupport)
        , m_resourceDesc(resourceDesc)
        , m_semaphoreType(semaphoreType)
        , m_device(SingletonDevice::getDevice(context))
        , m_deviceInterface(context.getPlatformInterface(), context.getInstance(), *m_device,
                            context.getUsedApiVersion(), context.getTestContext().getCommandLine())
        , m_allocator(new SimpleAllocator(
              m_deviceInterface, *m_device,
              getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())))
        , m_operationContext(
              new OperationContext(context, type, m_deviceInterface, *m_device, *m_allocator, pipelineCacheData))
        , m_queueA(nullptr)
        , m_queueB(nullptr)
        , m_rng(1234)

    {
        const std::vector<VkQueueFamilyProperties> queueFamilyProperties =
            getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR &&
            !context.getTimelineSemaphoreFeatures().timelineSemaphore)
            TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

        VkQueueFlags writeOpQueueFlags = m_writeOpSupport->getQueueFlags(*m_operationContext);
        for (uint32_t familyIdx = 0; familyIdx < queueFamilyProperties.size(); familyIdx++)
        {
            if (((queueFamilyProperties[familyIdx].queueFlags & writeOpQueueFlags) == writeOpQueueFlags) ||
                ((writeOpQueueFlags == VK_QUEUE_TRANSFER_BIT) &&
                 (((queueFamilyProperties[familyIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) ||
                  ((queueFamilyProperties[familyIdx].queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT))))
            {
                m_queueA            = getDeviceQueue(m_deviceInterface, *m_device, familyIdx, 0);
                m_queueFamilyIndexA = familyIdx;
                break;
            }
        }
        if (m_queueA == nullptr)
            TCU_THROW(NotSupportedError, "No queue supporting write operation");

        VkQueueFlags readOpQueueFlags = m_readOpSupport->getQueueFlags(*m_operationContext);
        for (uint32_t familyIdx = 0; familyIdx < queueFamilyProperties.size(); familyIdx++)
        {
            if (((queueFamilyProperties[familyIdx].queueFlags & readOpQueueFlags) == readOpQueueFlags) ||
                ((readOpQueueFlags == VK_QUEUE_TRANSFER_BIT) &&
                 (((queueFamilyProperties[familyIdx].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) ||
                  ((queueFamilyProperties[familyIdx].queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT))))
            {
                for (uint32_t queueIdx = 0; queueIdx < queueFamilyProperties[familyIdx].queueCount; queueIdx++)
                {
                    VkQueue queue = getDeviceQueue(m_deviceInterface, *m_device, familyIdx, queueIdx);

                    if (queue == m_queueA)
                        continue;

                    m_queueB            = queue;
                    m_queueFamilyIndexB = familyIdx;
                    break;
                }

                if (m_queueB != nullptr)
                    break;
            }
        }
        if (m_queueB == nullptr)
            TCU_THROW(NotSupportedError, "No queue supporting read operation");
    }

    tcu::TestStatus iterate(void)
    {
        const bool isTimelineSemaphore = (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR);
        const VkDevice &device         = *m_device;
        const DeviceInterface &vk      = m_deviceInterface;
        Unique<VkFence> fence(createFence(vk, device));
        const Unique<VkCommandPool> cmdPoolA(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_queueFamilyIndexA));
        const Unique<VkCommandPool> cmdPoolB(
            createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_queueFamilyIndexB));
        std::vector<SharedPtr<Move<VkCommandBuffer>>> ptrCmdBuffersA;
        SharedPtr<Move<VkCommandBuffer>> ptrCmdBufferB;
        std::vector<VkCommandBuffer> cmdBuffersA;
        VkCommandBuffer cmdBufferB;
        std::vector<Move<VkSemaphore>> semaphoresA;
        std::vector<Move<VkSemaphore>> semaphoresB;
        std::vector<VkSemaphore> semaphoreHandlesA;
        std::vector<VkSemaphore> semaphoreHandlesB;
        std::vector<uint64_t> timelineValuesA;
        std::vector<uint64_t> timelineValuesB;
        std::vector<QueueSubmitOrderIteration> iterations;
        std::vector<VkPipelineStageFlags2KHR> stageBits;
        std::vector<uint32_t> queueFamilies;
        SynchronizationWrapperPtr syncWrapper = getSynchronizationWrapper(m_type, vk, isTimelineSemaphore);

        // This guard will wait for the device to be idle before tearing down the resources above.
        const DeviceWaitIdleGuard idleGuard(vk, device);

        queueFamilies.push_back(m_queueFamilyIndexA);
        queueFamilies.push_back(m_queueFamilyIndexB);

        // Create a dozen of set of write/read operations.
        iterations.resize(12);
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderIteration &iter = iterations[iterIdx];

            iter.resource = makeSharedPtr(
                new Resource(*m_operationContext, m_resourceDesc,
                             m_writeOpSupport->getOutResourceUsageFlags() | m_readOpSupport->getInResourceUsageFlags(),
                             VK_SHARING_MODE_EXCLUSIVE, queueFamilies));

            iter.writeOp = makeSharedPtr(m_writeOpSupport->build(*m_operationContext, *iter.resource));
            iter.readOp  = makeSharedPtr(m_readOpSupport->build(*m_operationContext, *iter.resource));
        }

        // Record each write operation into its own command buffer.
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderIteration &iter = iterations[iterIdx];

            ptrCmdBuffersA.push_back(makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPoolA)));
            cmdBuffersA.push_back(**(ptrCmdBuffersA.back()));

            beginCommandBuffer(vk, cmdBuffersA.back());
            iter.writeOp->recordCommands(cmdBuffersA.back());

            {
                SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_type, vk, false);
                const SyncInfo writeSync                         = iter.writeOp->getOutSyncInfo();
                const SyncInfo readSync                          = iter.readOp->getInSyncInfo();
                const Resource &resource                         = *iter.resource;

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
                    synchronizationWrapper->cmdPipelineBarrier(cmdBuffersA.back(), &dependencyInfo);
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
                    synchronizationWrapper->cmdPipelineBarrier(cmdBuffersA.back(), &dependencyInfo);
                }

                stageBits.push_back(writeSync.stageMask);
            }

            endCommandBuffer(vk, cmdBuffersA.back());

            addSemaphore(vk, device, semaphoresA, semaphoreHandlesA, timelineValuesA, 2u);
        }

        DE_ASSERT(stageBits.size() == iterations.size());
        DE_ASSERT(semaphoreHandlesA.size() == iterations.size());

        // Record all read operations into a single command buffer and track the union of their execution stages.
        ptrCmdBufferB = makeVkSharedPtr(makeCommandBuffer(vk, device, *cmdPoolB));
        cmdBufferB    = **(ptrCmdBufferB);
        beginCommandBuffer(vk, cmdBufferB);
        for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
        {
            QueueSubmitOrderIteration &iter = iterations[iterIdx];
            iter.readOp->recordCommands(cmdBufferB);
        }
        endCommandBuffer(vk, cmdBufferB);

        addSemaphore(vk, device, semaphoresB, semaphoreHandlesB, timelineValuesB, timelineValuesA.back());

        // Submit writes, each in its own VkSubmitInfo. With binary
        // semaphores, submission don't wait on anything, with
        // timeline semaphores, submissions wait on a host signal
        // operation done below.
        {
            VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
                makeCommonSemaphoreSubmitInfo(semaphoreHandlesA.front(), 1u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
            std::vector<VkSemaphoreSubmitInfoKHR> signalSemaphoreSubmitInfo(
                iterations.size(),
                makeCommonSemaphoreSubmitInfo(VK_NULL_HANDLE, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));
            std::vector<VkCommandBufferSubmitInfoKHR> commandBufferSubmitInfos(iterations.size(),
                                                                               makeCommonCommandBufferSubmitInfo(0));
            SynchronizationWrapperPtr synchronizationWrapper =
                getSynchronizationWrapper(m_type, vk, isTimelineSemaphore, (uint32_t)iterations.size());

            for (uint32_t iterIdx = 0; iterIdx < iterations.size(); iterIdx++)
            {
                commandBufferSubmitInfos[iterIdx].commandBuffer = cmdBuffersA[iterIdx];
                signalSemaphoreSubmitInfo[iterIdx].semaphore    = semaphoreHandlesA[iterIdx];
                signalSemaphoreSubmitInfo[iterIdx].value        = timelineValuesA[iterIdx];

                synchronizationWrapper->addSubmitInfo(
                    isTimelineSemaphore, isTimelineSemaphore ? &waitSemaphoreSubmitInfo : nullptr, 1u,
                    &commandBufferSubmitInfos[iterIdx], 1u, &signalSemaphoreSubmitInfo[iterIdx], isTimelineSemaphore,
                    isTimelineSemaphore);
            }

            VK_CHECK(synchronizationWrapper->queueSubmit(m_queueA, VK_NULL_HANDLE));
        }

        // Submit reads, only waiting waiting on the last write
        // operations, ordering of signaling should guarantee that
        // when read operations kick in all writes have completed.
        {
            VkCommandBufferSubmitInfoKHR commandBufferSubmitInfos = makeCommonCommandBufferSubmitInfo(cmdBufferB);
            VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo      = makeCommonSemaphoreSubmitInfo(
                semaphoreHandlesA.back(), timelineValuesA.back(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);
            VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo = makeCommonSemaphoreSubmitInfo(
                semaphoreHandlesB.back(), timelineValuesB.back(), VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
            SynchronizationWrapperPtr synchronizationWrapper =
                getSynchronizationWrapper(m_type, vk, isTimelineSemaphore);

            synchronizationWrapper->addSubmitInfo(
                1u,                         // uint32_t                                waitSemaphoreInfoCount
                &waitSemaphoreSubmitInfo,   // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
                1u,                         // uint32_t                                commandBufferInfoCount
                &commandBufferSubmitInfos,  // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
                1u,                         // uint32_t                                signalSemaphoreInfoCount
                &signalSemaphoreSubmitInfo, // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
                isTimelineSemaphore, isTimelineSemaphore);

            VK_CHECK(synchronizationWrapper->queueSubmit(m_queueB, *fence));

            if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR)
            {
                const VkSemaphoreWaitInfo waitInfo = {
                    VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, // VkStructureType sType;
                    nullptr,                               // const void* pNext;
                    0u,                                    // VkSemaphoreWaitFlagsKHR flags;
                    1u,                                    // uint32_t semaphoreCount;
                    &semaphoreHandlesB.back(),             // const VkSemaphore* pSemaphores;
                    &timelineValuesB.back(),               // const uint64_t* pValues;
                };

                // Unblock the whole lot.
                hostSignal(vk, device, semaphoreHandlesA.front(), 1);

                VK_CHECK(vk.waitSemaphores(device, &waitInfo, ~0ull));
            }
            else
            {
                VK_CHECK(vk.waitForFences(device, 1, &fence.get(), VK_TRUE, ~0ull));
            }
        }

        // Verify the result of the operations.
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
    void addSemaphore(const DeviceInterface &vk, VkDevice device, std::vector<Move<VkSemaphore>> &semaphores,
                      std::vector<VkSemaphore> &semaphoreHandles, std::vector<uint64_t> &timelineValues,
                      uint64_t firstTimelineValue)
    {
        Move<VkSemaphore> semaphore;

        if (m_semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR)
        {
            // Only allocate a single exportable semaphore.
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
    VkSemaphoreType m_semaphoreType;
    const Unique<VkDevice> &m_device;
    const DeviceDriver m_deviceInterface;
    UniquePtr<SimpleAllocator> m_allocator;
    UniquePtr<OperationContext> m_operationContext;
    VkQueue m_queueA;
    VkQueue m_queueB;
    uint32_t m_queueFamilyIndexA;
    uint32_t m_queueFamilyIndexB;
    de::Random m_rng;
};

class QueueSubmitSignalOrderTestCase : public TestCase
{
public:
    QueueSubmitSignalOrderTestCase(tcu::TestContext &testCtx, SynchronizationType type, const std::string &name,
                                   OperationName writeOp, OperationName readOp, const ResourceDescription &resourceDesc,
                                   VkSemaphoreType semaphoreType, PipelineCacheData &pipelineCacheData)
        : TestCase(testCtx, name.c_str())
        , m_type(type)
        , m_writeOpSupport(makeOperationSupport(writeOp, resourceDesc).release())
        , m_readOpSupport(makeOperationSupport(readOp, resourceDesc).release())
        , m_resourceDesc(resourceDesc)
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
        return new QueueSubmitSignalOrderTestInstance(context, m_type, m_writeOpSupport, m_readOpSupport,
                                                      m_resourceDesc, m_semaphoreType, m_pipelineCacheData);
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
    VkSemaphoreType m_semaphoreType;
    PipelineCacheData &m_pipelineCacheData;
};

class QueueSubmitSignalOrderTests : public tcu::TestCaseGroup
{
public:
    QueueSubmitSignalOrderTests(tcu::TestContext &testCtx, SynchronizationType type, VkSemaphoreType semaphoreType,
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
            OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE,
            OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER,
            OPERATION_NAME_WRITE_COPY_IMAGE,
            OPERATION_NAME_WRITE_BLIT_IMAGE,
            OPERATION_NAME_WRITE_SSBO_VERTEX,
            OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL,
            OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION,
            OPERATION_NAME_WRITE_SSBO_GEOMETRY,
            OPERATION_NAME_WRITE_SSBO_FRAGMENT,
            OPERATION_NAME_WRITE_SSBO_COMPUTE,
            OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT,
            OPERATION_NAME_WRITE_IMAGE_VERTEX,
            OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL,
            OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION,
            OPERATION_NAME_WRITE_IMAGE_GEOMETRY,
            OPERATION_NAME_WRITE_IMAGE_FRAGMENT,
            OPERATION_NAME_WRITE_IMAGE_COMPUTE,
            OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT,
        };
        static const OperationName readOps[] = {
            OPERATION_NAME_READ_COPY_BUFFER,
            OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE,
            OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER,
            OPERATION_NAME_READ_COPY_IMAGE,
            OPERATION_NAME_READ_BLIT_IMAGE,
            OPERATION_NAME_READ_UBO_VERTEX,
            OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL,
            OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION,
            OPERATION_NAME_READ_UBO_GEOMETRY,
            OPERATION_NAME_READ_UBO_FRAGMENT,
            OPERATION_NAME_READ_UBO_COMPUTE,
            OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT,
            OPERATION_NAME_READ_SSBO_VERTEX,
            OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL,
            OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION,
            OPERATION_NAME_READ_SSBO_GEOMETRY,
            OPERATION_NAME_READ_SSBO_FRAGMENT,
            OPERATION_NAME_READ_SSBO_COMPUTE,
            OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT,
            OPERATION_NAME_READ_IMAGE_VERTEX,
            OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL,
            OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION,
            OPERATION_NAME_READ_IMAGE_GEOMETRY,
            OPERATION_NAME_READ_IMAGE_FRAGMENT,
            OPERATION_NAME_READ_IMAGE_COMPUTE,
            OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED,
            OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH,
            OPERATION_NAME_READ_VERTEX_INPUT,
        };

        for (uint32_t writeOpIdx = 0; writeOpIdx < DE_LENGTH_OF_ARRAY(writeOps); writeOpIdx++)
            for (uint32_t readOpIdx = 0; readOpIdx < DE_LENGTH_OF_ARRAY(readOps); readOpIdx++)
            {
                const OperationName writeOp   = writeOps[writeOpIdx];
                const OperationName readOp    = readOps[readOpIdx];
                const std::string opGroupName = getOperationName(writeOp) + "_" + getOperationName(readOp);
                bool empty                    = true;

                de::MovePtr<tcu::TestCaseGroup> opGroup(new tcu::TestCaseGroup(m_testCtx, opGroupName.c_str()));

                for (int resourceNdx = 0; resourceNdx < DE_LENGTH_OF_ARRAY(s_resources); ++resourceNdx)
                {
                    const ResourceDescription &resource = s_resources[resourceNdx];

                    if (isResourceSupported(writeOp, resource) && isResourceSupported(readOp, resource))
                    {
                        opGroup->addChild(
                            new QueueSubmitSignalOrderTestCase(m_testCtx, m_type, getResourceName(resource), writeOp,
                                                               readOp, resource, m_semaphoreType, m_pipelineCacheData));
                        empty = false;
                    }
                }
                if (!empty)
                    addChild(opGroup.release());
            }
    }

    void deinit(void)
    {
        cleanupGroup();
    }

private:
    SynchronizationType m_type;
    VkSemaphoreType m_semaphoreType;
    // synchronization.op tests share pipeline cache data to speed up test
    // execution.
    PipelineCacheData m_pipelineCacheData;
};

} // namespace

tcu::TestCaseGroup *createSignalOrderTests(tcu::TestContext &testCtx, SynchronizationType type)
{
    de::MovePtr<tcu::TestCaseGroup> orderingTests(new tcu::TestCaseGroup(testCtx, "signal_order"));

    orderingTests->addChild(
        new QueueSubmitSignalOrderTests(testCtx, type, VK_SEMAPHORE_TYPE_BINARY_KHR, "binary_semaphore"));
    orderingTests->addChild(
        new QueueSubmitSignalOrderTests(testCtx, type, VK_SEMAPHORE_TYPE_TIMELINE_KHR, "timeline_semaphore"));
    orderingTests->addChild(
        new QueueSubmitSignalOrderSharedTests(testCtx, type, VK_SEMAPHORE_TYPE_BINARY_KHR, "shared_binary_semaphore"));
    orderingTests->addChild(new QueueSubmitSignalOrderSharedTests(testCtx, type, VK_SEMAPHORE_TYPE_TIMELINE_KHR,
                                                                  "shared_timeline_semaphore"));

    return orderingTests.release();
}

} // namespace synchronization
} // namespace vkt
