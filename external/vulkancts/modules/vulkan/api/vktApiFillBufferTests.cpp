/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Fill Buffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiFillBufferTests.hpp"
#include "vktApiBufferAndImageAllocationUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCase.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "deSharedPtr.hpp"
#include <limits>

namespace vkt::api
{

using namespace vk;

namespace
{

enum class QueueType
{
    GRAPHICS_COMPUTE = 0,
    TRANSFER_ONLY,
    COMPUTE_ONLY,
};

struct TestParams
{
    enum
    {
        TEST_DATA_SIZE = 256
    };

    VkDeviceSize dstSize;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
    uint32_t testData[TEST_DATA_SIZE];
    de::SharedPtr<IBufferAllocator> bufferAllocator;
    QueueType queueType;
    bool useDeviceAddressCommands;
};

// Creates a device that has transfer only operations
Move<VkDevice> createCustomDevice(Context &context,
#ifdef CTS_USES_VULKANSC
                                  const vkt::CustomInstance &customInstance,
#endif // CTS_USES_VULKANSC
                                  uint32_t &queueFamilyIndex)
{
#ifdef CTS_USES_VULKANSC
    const vk::InstanceInterface &instanceDriver = customInstance.getDriver();
    const vk::VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, customInstance, context.getTestContext().getCommandLine());
#else
    const vk::VkInstance customInstance         = context.getInstance();
    const vk::InstanceInterface &instanceDriver = context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();
#endif // CTS_USES_VULKANSC

    queueFamilyIndex = findQueueFamilyIndexWithCaps(instanceDriver, physicalDevice, VK_QUEUE_TRANSFER_BIT,
                                                    VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    const std::vector<VkQueueFamilyProperties> queueFamilies =
        getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    // This must be found, findQueueFamilyIndexWithCaps would have
    // thrown a NotSupported exception if the requested queue type did
    // not exist. Similarly, this was written with the assumption the
    // "alternative" queue would be different to the universal queue.
    DE_ASSERT(queueFamilyIndex < queueFamilies.size() && queueFamilyIndex != context.getUniversalQueueFamilyIndex());
    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfos{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority,                             // const float* pQueuePriorities;
    };

    // Replicate default device extension list.
    const auto extensionNames     = context.getDeviceCreationExtensions();
    auto synchronization2Features = context.getSynchronization2Features();
    auto deviceFeatures2          = context.getDeviceFeatures2();
    void *pNext                   = &deviceFeatures2;

    if (context.isDeviceFunctionalitySupported("VK_KHR_synchronization2"))
    {
        if (context.getEquivalentApiVersion() < VK_API_VERSION_1_3)
        {
            synchronization2Features.pNext = &deviceFeatures2;
            pNext                          = &synchronization2Features;
        }
    }

#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

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

    const VkDeviceCreateInfo deviceCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,         // VkStructureType sType;
        pNext,                                        // const void* pNext;
        (VkDeviceCreateFlags)0u,                      // VkDeviceCreateFlags flags;
        1u,                                           // uint32_t queueCreateInfoCount;
        &deviceQueueCreateInfos,                      // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                           // uint32_t enabledLayerCount;
        nullptr,                                      // const char* const* ppEnabledLayerNames;
        static_cast<uint32_t>(extensionNames.size()), // uint32_t enabledExtensionCount;
        extensionNames.data(),                        // const char* const* ppEnabledExtensionNames;
        nullptr,                                      // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return vkt::createCustomDevice(context.getPlatformInterface(), customInstance, instanceDriver, physicalDevice,
                                   &deviceCreateInfo);
}

class FillWholeBufferTestInstance : public vkt::TestInstance
{
public:
    FillWholeBufferTestInstance(Context &context, const TestParams &testParams);
    virtual tcu::TestStatus iterate(void) override;

protected:
    // dstSize will be used as the buffer size.
    // dstOffset will be used as the offset for vkCmdFillBuffer.
    // size in vkCmdFillBuffer will always be VK_WHOLE_SIZE.
    const TestParams m_params;
#ifdef CTS_USES_VULKANSC
    const CustomInstance m_customInstance;
#endif // CTS_USES_VULKANSC
    Move<VkDevice> m_customDevice;
    de::MovePtr<Allocator> m_customAllocator;

    VkDevice m_device;
    Allocator *m_allocator;
    uint32_t m_queueFamilyIndex;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;

    Move<VkBuffer> m_destination;
    de::MovePtr<Allocation> m_destinationBufferAlloc;
};

FillWholeBufferTestInstance::FillWholeBufferTestInstance(Context &context, const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_params(testParams)
#ifdef CTS_USES_VULKANSC
    , m_customInstance(createCustomInstanceFromContext(context))
#endif // CTS_USES_VULKANSC
{
#ifdef CTS_USES_VULKANSC
    const vk::InstanceInterface &vki = m_customInstance.getDriver();
    const VkPhysicalDevice physDevice =
        vk::chooseDevice(vki, m_customInstance, m_context.getTestContext().getCommandLine());
#else
    const vk::InstanceInterface &vki            = m_context.getInstanceInterface();
    const VkPhysicalDevice physDevice           = m_context.getPhysicalDevice();
#endif // CTS_USES_VULKANSC
    const DeviceInterface &vk = m_context.getDeviceInterface();

    if (testParams.queueType == QueueType::TRANSFER_ONLY)
    {
        m_customDevice = createCustomDevice(context,
#ifdef CTS_USES_VULKANSC
                                            m_customInstance,
#endif
                                            m_queueFamilyIndex);
        m_customAllocator = de::MovePtr<Allocator>(
            new SimpleAllocator(vk, *m_customDevice, getPhysicalDeviceMemoryProperties(vki, physDevice)));

        m_device    = *m_customDevice;
        m_allocator = &(*m_customAllocator);
    }
    else
    {
        m_device    = context.getDevice();
        m_allocator = &context.getDefaultAllocator();

        if (testParams.queueType == QueueType::COMPUTE_ONLY)
            m_queueFamilyIndex = context.getComputeQueueFamilyIndex();
        else
            m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (m_params.useDeviceAddressCommands)
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    MemoryRequirement memReq = m_params.useDeviceAddressCommands ?
                                   MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress :
                                   MemoryRequirement::HostVisible;

    m_cmdPool   = createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    testParams.bufferAllocator->createTestBuffer(vk, m_device, m_params.dstSize, usage, context, *m_allocator,
                                                 m_destination, memReq, m_destinationBufferAlloc);
}

tcu::TestStatus FillWholeBufferTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkQueue queue       = getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

    // use synchronization2 when testing transfer only queue or device address commands
    bool useSynchronization2 = false;
    if (m_context.isDeviceFunctionalitySupported("VK_KHR_synchronization2"))
        useSynchronization2 = (m_params.queueType == QueueType::TRANSFER_ONLY) || m_params.useDeviceAddressCommands;

    // Make sure some stuff below will work.
    DE_ASSERT(m_params.dstSize >= sizeof(uint32_t));
    DE_ASSERT(m_params.dstSize < static_cast<VkDeviceSize>(std::numeric_limits<size_t>::max()));
    DE_ASSERT(m_params.dstOffset < m_params.dstSize);

    // Fill buffer from the host and flush buffer memory.
    uint8_t *bytes = reinterpret_cast<uint8_t *>(m_destinationBufferAlloc->getHostPtr());
    deMemset(bytes, 0xff, static_cast<size_t>(m_params.dstSize));
    flushAlloc(vk, m_device, *m_destinationBufferAlloc);

    const VkBufferMemoryBarrier gpuToHostBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        VK_WHOLE_SIZE                            // VkDeviceSize size;
    };

#ifndef CTS_USES_VULKANSC
    using DependencyInfo          = VkDependencyInfo;
    using CommandBufferSubmitInfo = VkCommandBufferSubmitInfo;
    using SubmitInfo2             = VkSubmitInfo2;
    auto cmdPipelineBarrier2Fun   = &DeviceInterface::cmdPipelineBarrier2;
    auto queueSubmit2Fun          = &DeviceInterface::queueSubmit2;
#else
    using DependencyInfo                        = VkDependencyInfoKHR;
    using CommandBufferSubmitInfo               = VkCommandBufferSubmitInfoKHR;
    using SubmitInfo2                           = VkSubmitInfo2KHR;
    auto cmdPipelineBarrier2Fun                 = &DeviceInterface::cmdPipelineBarrier2KHR;
    auto queueSubmit2Fun                        = &DeviceInterface::queueSubmit2KHR;
#endif // CTS_USES_VULKANSC

    auto gpuToHostBarrier2 = makeBufferMemoryBarrier2(
        VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, VK_PIPELINE_STAGE_2_HOST_BIT_KHR,
        VK_ACCESS_2_HOST_READ_BIT_KHR, *m_destination, 0, VK_WHOLE_SIZE);

    DependencyInfo depInfo           = initVulkanStructure();
    depInfo.bufferMemoryBarrierCount = 1;
    depInfo.pBufferMemoryBarriers    = &gpuToHostBarrier2;

#ifndef CTS_USES_VULKANSC
    VkMemoryRangeBarrierKHR memoryRangeBarrier           = initVulkanStructure();
    VkMemoryRangeBarriersInfoKHR memoryRangeBarriersInfo = initVulkanStructure();

    if (m_params.useDeviceAddressCommands)
    {
        memoryRangeBarrier.srcStageMask         = gpuToHostBarrier2.srcStageMask;
        memoryRangeBarrier.srcAccessMask        = gpuToHostBarrier2.srcAccessMask;
        memoryRangeBarrier.dstStageMask         = gpuToHostBarrier2.dstStageMask;
        memoryRangeBarrier.dstAccessMask        = gpuToHostBarrier2.dstAccessMask;
        memoryRangeBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        memoryRangeBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        memoryRangeBarrier.addressRange.address = getBufferDeviceAddress(vk, m_device, *m_destination);
        memoryRangeBarrier.addressRange.size    = m_params.dstSize;

        memoryRangeBarriersInfo.memoryRangeBarrierCount = 1u;
        memoryRangeBarriersInfo.pMemoryRangeBarriers    = &memoryRangeBarrier;

        // memset DependencyInfo to 0 to make sure data from VkMemoryRangeBarrierKHR is used
        depInfo = initVulkanStructure(&memoryRangeBarriersInfo);
    }
#endif

    // Fill buffer using VK_WHOLE_SIZE.
    beginCommandBuffer(vk, *m_cmdBuffer);
    vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, VK_WHOLE_SIZE, uint32_t{0x01010101});

    if (useSynchronization2)
        (vk.*(cmdPipelineBarrier2Fun))(*m_cmdBuffer, &depInfo);
    else
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr,
                              1, &gpuToHostBarrier, 0, nullptr);

    endCommandBuffer(vk, *m_cmdBuffer);

    Move<VkFence> fence(createFence(vk, m_device));
    if (useSynchronization2)
    {
        CommandBufferSubmitInfo commandBufferInfos = initVulkanStructure();
        commandBufferInfos.commandBuffer           = *m_cmdBuffer;

        SubmitInfo2 submitInfo2            = initVulkanStructure();
        submitInfo2.commandBufferInfoCount = 1u;
        submitInfo2.pCommandBufferInfos    = &commandBufferInfos;

        (vk.*(queueSubmit2Fun))(queue, 1u, &submitInfo2, *fence);
    }
    else
    {
        VkSubmitInfo submitInfo       = initVulkanStructure();
        submitInfo.commandBufferCount = 1u;
        submitInfo.pCommandBuffers    = &m_cmdBuffer.get();

        VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
    }
    waitForFence(vk, m_device, *fence);

    // Invalidate buffer memory and check the buffer contains the expected results.
    invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);

    const VkDeviceSize startOfExtra = (m_params.dstSize / sizeof(uint32_t)) * sizeof(uint32_t);
    for (VkDeviceSize i = 0; i < m_params.dstSize; ++i)
    {
        const uint8_t expectedByte = ((i >= m_params.dstOffset && i < startOfExtra) ? 0x01 : 0xff);
        if (bytes[i] != expectedByte)
        {
            std::ostringstream msg;
            msg << "Invalid byte at position " << i << " in the buffer (found 0x" << std::hex
                << static_cast<int>(bytes[i]) << " but expected 0x" << static_cast<int>(expectedByte) << ")";
            return tcu::TestStatus::fail(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class FillWholeBufferTestCase : public vkt::TestCase
{
public:
    FillWholeBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override
    {
        if (m_params.useDeviceAddressCommands)
        {
            context.requireDeviceFunctionality("VK_KHR_device_address_commands");
            context.requireDeviceFunctionality("VK_KHR_synchronization2");
        }

        if ((m_params.queueType == QueueType::COMPUTE_ONLY) && (context.getComputeQueueFamilyIndex() == -1))
            TCU_THROW(NotSupportedError, "Exclusive compute queue not supported.");
    }

    virtual TestInstance *createInstance(Context &context) const override
    {
        return new FillWholeBufferTestInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

class FillBufferTestInstance : public vkt::TestInstance
{
public:
    FillBufferTestInstance(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void);

protected:
    const TestParams m_params;
#ifdef CTS_USES_VULKANSC
    const CustomInstance m_customInstance;
#endif // CTS_USES_VULKANSC
    Move<VkDevice> m_customDevice;
    de::MovePtr<Allocator> m_customAllocator;

    VkDevice m_device;
    Allocator *m_allocator;
    uint32_t m_queueFamilyIndex;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    de::MovePtr<tcu::TextureLevel> m_destinationTextureLevel;
    de::MovePtr<tcu::TextureLevel> m_expectedTextureLevel;

    VkCommandBufferBeginInfo m_cmdBufferBeginInfo;

    Move<VkBuffer> m_destination;
    VkDeviceAddress m_destinationDevicceAddress;
    de::MovePtr<Allocation> m_destinationBufferAlloc;

    void generateBuffer(tcu::PixelBufferAccess buffer, int width, int height, int depth = 1);
    virtual void generateExpectedResult(void);
    void uploadBuffer(tcu::ConstPixelBufferAccess bufferAccess, const Allocation &bufferAlloc);
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    uint32_t calculateSize(tcu::ConstPixelBufferAccess src) const
    {
        return src.getWidth() * src.getHeight() * src.getDepth() * tcu::getPixelSize(src.getFormat());
    }
};

FillBufferTestInstance::FillBufferTestInstance(Context &context, TestParams testParams)
    : vkt::TestInstance(context)
    , m_params(testParams)
#ifdef CTS_USES_VULKANSC
    , m_customInstance(createCustomInstanceFromContext(context))
#endif // CTS_USES_VULKANSC
    , m_destinationDevicceAddress(0ull)
{
    const InstanceInterface &vki      = m_context.getInstanceInterface();
    const DeviceInterface &vk         = m_context.getDeviceInterface();
    const VkPhysicalDevice physDevice = m_context.getPhysicalDevice();

    if (testParams.queueType == QueueType::TRANSFER_ONLY)
    {
        m_customDevice = createCustomDevice(context,
#ifdef CTS_USES_VULKANSC
                                            m_customInstance,
#endif // CTS_USES_VULKANSC
                                            m_queueFamilyIndex);
        m_customAllocator = de::MovePtr<Allocator>(
            new SimpleAllocator(vk, *m_customDevice, getPhysicalDeviceMemoryProperties(vki, physDevice)));

        m_device    = *m_customDevice;
        m_allocator = &(*m_customAllocator);
    }
    else
    {
        m_device    = context.getDevice();
        m_allocator = &context.getDefaultAllocator();

        if (testParams.queueType == QueueType::COMPUTE_ONLY)
            m_queueFamilyIndex = context.getComputeQueueFamilyIndex();
        else
            m_queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (m_params.useDeviceAddressCommands)
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // Create command pool
    m_cmdPool = createCommandPool(vk, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_queueFamilyIndex);

    // Create command buffer
    m_cmdBuffer = allocateCommandBuffer(vk, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    testParams.bufferAllocator->createTestBuffer(
        vk, m_device, m_params.dstSize, usage, context, *m_allocator, m_destination,
        m_params.useDeviceAddressCommands ? (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress) :
                                            MemoryRequirement::HostVisible,
        m_destinationBufferAlloc);

    if (m_params.useDeviceAddressCommands)
        m_destinationDevicceAddress = getBufferDeviceAddress(vk, m_device, *m_destination);
}

tcu::TestStatus FillBufferTestInstance::iterate(void)
{
    const int dstLevelWidth = (int)(m_params.dstSize / 4);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

    generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

    generateExpectedResult();

    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkQueue queue       = getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

    const VkBufferMemoryBarrier dstBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        m_params.dstOffset,                      // VkDeviceSize offset;
        VK_WHOLE_SIZE                            // VkDeviceSize size;
    };

    beginCommandBuffer(vk, *m_cmdBuffer);

    if (!m_params.useDeviceAddressCommands)
        vk.cmdFillBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData[0]);

#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        // use different valid addressFlags in some cases to test them
        VkAddressCommandFlagsKHR dstFlags = 0;
        if (m_params.dstOffset)
            dstFlags |= VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
        if (m_params.size < VK_WHOLE_SIZE)
            dstFlags |= VK_ADDRESS_COMMAND_UNKNOWN_STORAGE_BUFFER_USAGE_BIT_KHR;

        VkDeviceAddressRangeKHR dstRange{m_destinationDevicceAddress + m_params.dstOffset, m_params.size};
        vk.cmdFillMemoryKHR(*m_cmdBuffer, &dstRange, dstFlags, m_params.testData[0]);
    }
#endif

    vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &dstBufferBarrier, 0, nullptr);
    endCommandBuffer(vk, *m_cmdBuffer);

    submitCommandsAndWait(vk, m_device, queue, m_cmdBuffer.get());

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
    invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

void FillBufferTestInstance::generateBuffer(tcu::PixelBufferAccess buffer, int width, int height, int depth)
{
    for (int z = 0; z < depth; z++)
    {
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
                buffer.setPixel(tcu::UVec4(x, y, z, 255), x, y, z);
        }
    }
}

void FillBufferTestInstance::uploadBuffer(tcu::ConstPixelBufferAccess bufferAccess, const Allocation &bufferAlloc)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const uint32_t bufferSize = calculateSize(bufferAccess);

    // Write buffer data
    deMemcpy(bufferAlloc.getHostPtr(), bufferAccess.getDataPtr(), bufferSize);
    flushAlloc(vk, m_device, bufferAlloc);
}

tcu::TestStatus FillBufferTestInstance::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::ConstPixelBufferAccess expected = m_expectedTextureLevel->getAccess();
    const tcu::UVec4 threshold(0);

    if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expected, result,
                                  threshold, tcu::COMPARE_LOG_RESULT))
    {
        return tcu::TestStatus::fail("Fill and Update Buffer test");
    }

    return tcu::TestStatus::pass("Fill and Update Buffer test");
}

void FillBufferTestInstance::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel->getAccess(), dst);

    uint32_t *currentPtr = (uint32_t *)m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;
    uint32_t *endPtr     = currentPtr + m_params.size / 4;

    while (currentPtr < endPtr)
    {
        *currentPtr = m_params.testData[0];
        currentPtr++;
    }
}

class FillBufferTestCase : public vkt::TestCase
{
public:
    FillBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.useDeviceAddressCommands)
            context.requireDeviceFunctionality("VK_KHR_device_address_commands");

        if ((m_params.queueType == QueueType::COMPUTE_ONLY) && (context.getComputeQueueFamilyIndex() == -1))
            TCU_THROW(NotSupportedError, "Exclusive compute queue not supported.");
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new FillBufferTestInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

// Update Buffer

class UpdateBufferTestInstance : public FillBufferTestInstance
{
public:
    UpdateBufferTestInstance(Context &context, TestParams testParams) : FillBufferTestInstance(context, testParams)
    {
    }
    virtual tcu::TestStatus iterate(void);

protected:
    virtual void generateExpectedResult(void);
};

tcu::TestStatus UpdateBufferTestInstance::iterate(void)
{
    const int dstLevelWidth = (int)(m_params.dstSize / 4);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(mapVkFormat(VK_FORMAT_R8G8B8A8_UINT), dstLevelWidth, 1));

    generateBuffer(m_destinationTextureLevel->getAccess(), dstLevelWidth, 1, 1);

    generateExpectedResult();

    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkQueue queue       = getDeviceQueue(vk, m_device, m_queueFamilyIndex, 0);

    const VkBufferMemoryBarrier dstBufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        m_params.dstOffset,                      // VkDeviceSize offset;
        VK_WHOLE_SIZE                            // VkDeviceSize size;
    };

    beginCommandBuffer(vk, *m_cmdBuffer);

    if (!m_params.useDeviceAddressCommands)
        vk.cmdUpdateBuffer(*m_cmdBuffer, *m_destination, m_params.dstOffset, m_params.size, m_params.testData);

#ifndef CTS_USES_VULKANSC
    if (m_params.useDeviceAddressCommands)
    {
        // use different valid addressFlags in some cases to test them
        VkAddressCommandFlagsKHR dstFlags = VK_ADDRESS_COMMAND_FULLY_BOUND_BIT_KHR;
        if (m_params.size < TestParams::TEST_DATA_SIZE)
            dstFlags |= VK_ADDRESS_COMMAND_UNKNOWN_STORAGE_BUFFER_USAGE_BIT_KHR;
        if (m_params.queueType == QueueType::TRANSFER_ONLY)
            dstFlags = 0;

        VkDeviceAddressRangeKHR dstRange{m_destinationDevicceAddress + m_params.dstOffset, m_params.size};
        vk.cmdUpdateMemoryKHR(*m_cmdBuffer, &dstRange, dstFlags, m_params.size, &m_params.testData);
    }
#endif

    vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &dstBufferBarrier, 0, nullptr);
    endCommandBuffer(vk, *m_cmdBuffer);

    submitCommandsAndWait(vk, m_device, queue, m_cmdBuffer.get());

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(m_destinationTextureLevel->getAccess().getFormat(), dstLevelWidth, 1));
    invalidateAlloc(vk, m_device, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

void UpdateBufferTestInstance::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel->getAccess(), dst);

    uint32_t *currentPtr = (uint32_t *)m_expectedTextureLevel->getAccess().getDataPtr() + m_params.dstOffset / 4;

    deMemcpy(currentPtr, m_params.testData, (size_t)m_params.size);
}

class UpdateBufferTestCase : public vkt::TestCase
{
public:
    UpdateBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.useDeviceAddressCommands)
            context.requireDeviceFunctionality("VK_KHR_device_address_commands");

        if ((m_params.queueType == QueueType::COMPUTE_ONLY) && (context.getComputeQueueFamilyIndex() == -1))
            TCU_THROW(NotSupportedError, "Exclusive compute queue not supported.");
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new UpdateBufferTestInstance(context, m_params);
    }

private:
    TestParams m_params;
};

} // namespace

tcu::TestCaseGroup *createFillAndUpdateBufferTests(tcu::TestContext &testCtx)
{
    const de::SharedPtr<IBufferAllocator> bufferAllocators[]{
        de::SharedPtr<BufferSuballocation>(new BufferSuballocation()),
        de::SharedPtr<BufferDedicatedAllocation>(new BufferDedicatedAllocation())};

    de::MovePtr<tcu::TestCaseGroup> fillAndUpdateBufferTests(new tcu::TestCaseGroup(testCtx, "fill_and_update_buffer"));

    struct TestGroupData
    {
        const char *name;
        bool useDedicatedAllocation;
        QueueType queueType;
    };
    const TestGroupData testGroupData[]{
        // BufferView Fill and Update Tests for Suballocated Objects
        {"suballocation", false, QueueType::GRAPHICS_COMPUTE},
        // BufferView Fill and Update Tests for Suballocated Objects on transfer only queue
        {"suballocation_transfer_queue", false, QueueType::TRANSFER_ONLY},
        // BufferView Fill and Update Tests for Dedicatedly Allocated Objects
        {"dedicated_alloc", true, QueueType::GRAPHICS_COMPUTE},
    };

    TestParams params;
    for (const auto &groupData : testGroupData)
    {
        de::MovePtr<tcu::TestCaseGroup> currentTestsGroup(new tcu::TestCaseGroup(testCtx, groupData.name));

        params.dstSize                  = TestParams::TEST_DATA_SIZE;
        params.bufferAllocator          = bufferAllocators[groupData.useDedicatedAllocation];
        params.queueType                = groupData.queueType;
        params.useDeviceAddressCommands = false;

        uint8_t *data = (uint8_t *)params.testData;
        for (uint32_t b = 0u; b < (params.dstSize * sizeof(params.testData[0])); b++)
            data[b] = (uint8_t)(b % 255);

        {
            const std::string testName("buffer_whole");

            params.dstOffset = 0;
            params.size      = params.dstSize;

            currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, params));
            currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, params));
        }

        // limit number of tests repeated for device_address_commands
        if (groupData.useDedicatedAllocation)
        {
            const std::string testName("buffer_whole_device_address");

            params.dstOffset                = 0;
            params.size                     = params.dstSize;
            params.useDeviceAddressCommands = true;

            currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, params));
            currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, params));
            params.useDeviceAddressCommands = false;
        }

        {
            const std::string testName("buffer_first_one");

            params.dstOffset = 0;
            params.size      = 4;

            currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, params));
            currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, params));
        }

        {
            const std::string testName("buffer_second_one");

            params.dstOffset = 4;
            params.size      = 4;

            currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, params));
            currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, params));
        }

        {
            const std::string testName("buffer_second_part");

            params.dstOffset = params.dstSize / 2;
            params.size      = params.dstSize / 2;

            currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, params));
            currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, params));
        }

        {
            const std::string testName("buffer_second_part_device_address");

            params.dstOffset                = params.dstSize / 2;
            params.size                     = params.dstSize / 2;
            params.useDeviceAddressCommands = true;

            currentTestsGroup->addChild(new FillBufferTestCase(testCtx, "fill_" + testName, params));
            currentTestsGroup->addChild(new UpdateBufferTestCase(testCtx, "update_" + testName, params));
            params.useDeviceAddressCommands = false;
        }

        // VK_WHOLE_SIZE tests.
        {
            params.useDeviceAddressCommands = false;
            for (VkDeviceSize i = 0; i < sizeof(uint32_t); ++i)
            {
                for (VkDeviceSize j = 0; j < sizeof(uint32_t); ++j)
                {
                    params.dstSize   = TestParams::TEST_DATA_SIZE + i;
                    params.dstOffset = j * sizeof(uint32_t);
                    params.size      = VK_WHOLE_SIZE;

                    const VkDeviceSize extraBytes = params.dstSize % sizeof(uint32_t);
                    const std::string name        = "fill_buffer_vk_whole_size_" + de::toString(extraBytes) +
                                             "_extra_bytes_offset_" + de::toString(params.dstOffset);

                    currentTestsGroup->addChild(new FillWholeBufferTestCase{testCtx, name, params});
                }
            }

            params.dstSize                  = TestParams::TEST_DATA_SIZE;
            params.dstOffset                = sizeof(uint32_t);
            params.size                     = VK_WHOLE_SIZE;
            params.useDeviceAddressCommands = true;

            // when using dedicated allocation test also compute only queue
            if (groupData.useDedicatedAllocation)
                params.queueType = QueueType::COMPUTE_ONLY;

            currentTestsGroup->addChild(
                new FillWholeBufferTestCase{testCtx, "fill_buffer_vk_whole_size_device_address", params});
        }

        fillAndUpdateBufferTests->addChild(currentTestsGroup.release());
    }

    return fillAndUpdateBufferTests.release();
}

} // namespace vkt::api
