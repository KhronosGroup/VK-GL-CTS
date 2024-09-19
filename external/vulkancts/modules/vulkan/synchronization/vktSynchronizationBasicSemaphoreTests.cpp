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
 * \brief Synchronization semaphore basic tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationBasicSemaphoreTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkRef.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include <thread>

#include "tcuCommandLine.hpp"

namespace vkt
{
namespace synchronization
{
namespace
{

using namespace vk;
using vkt::synchronization::VideoCodecOperationFlags;

struct TestConfig
{
    bool useTypeCreate;
    VkSemaphoreType semaphoreType;
    SynchronizationType type;
    VideoCodecOperationFlags videoCodecOperationFlags;
};

#ifdef CTS_USES_VULKANSC
static const int basicChainLength = 1024;
#else
static const int basicChainLength = 32768;
#endif

Move<VkSemaphore> createTestSemaphore(Context &context, const DeviceInterface &vk, const VkDevice device,
                                      const TestConfig &config)
{
    if (config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE && !context.getTimelineSemaphoreFeatures().timelineSemaphore)
        TCU_THROW(NotSupportedError, "Timeline semaphore not supported");

    return Move<VkSemaphore>(config.useTypeCreate ? createSemaphoreType(vk, device, config.semaphoreType) :
                                                    createSemaphore(vk, device));
}

#define FENCE_WAIT ~0ull

VideoDevice *getVideoDevice(Context &context, bool usingTimelineSemaphores, bool usingSync2,
                            VideoCodecOperationFlags videoCodecOperationFlags)
{
    DE_ASSERT(videoCodecOperationFlags != 0);

    VideoDevice::VideoDeviceFlags videoFlags = VideoDevice::VideoDeviceFlagBits::VIDEO_DEVICE_FLAG_NONE;
    if (usingTimelineSemaphores)
        videoFlags |= VideoDevice::VideoDeviceFlagBits::VIDEO_DEVICE_FLAG_REQUIRE_TIMELINE_OR_NOT_SUPPORTED;
    if (usingSync2)
        videoFlags |= VideoDevice::VideoDeviceFlagBits::VIDEO_DEVICE_FLAG_REQUIRE_SYNC2_OR_NOT_SUPPORTED;

    return new VideoDevice(context, videoCodecOperationFlags, videoFlags);
}

tcu::TestStatus basicOneQueueCase(Context &context, const TestConfig config)
{
    bool usingTimelineSemaphores = config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE;
    bool usingSync2              = config.type == SynchronizationType::SYNCHRONIZATION2;

    de::MovePtr<VideoDevice> videoDevice(
        config.videoCodecOperationFlags != 0 ?
            getVideoDevice(context, usingTimelineSemaphores, usingSync2, config.videoCodecOperationFlags) :
            nullptr);
    const DeviceInterface &vk       = getSyncDeviceInterface(videoDevice, context);
    const VkDevice device           = getSyncDevice(videoDevice, context);
    const VkQueue queue             = getSyncQueue(videoDevice, context);
    const uint32_t queueFamilyIndex = getSyncQueueFamilyIndex(videoDevice, context);
    const Unique<VkSemaphore> semaphore(createTestSemaphore(context, vk, device, config));
    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));
    const VkCommandBufferBeginInfo info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType                          sType;
        nullptr,                                      // const void*                              pNext;
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // VkCommandBufferUsageFlags                flags;
        nullptr,                                      // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
    };
    const uint64_t timelineValue = 1u;
    const Unique<VkFence> fence(createFence(vk, device));
    VkCommandBufferSubmitInfoKHR commandBufferInfo = makeCommonCommandBufferSubmitInfo(*cmdBuffer);
    SynchronizationWrapperPtr synchronizationWrapper =
        getSynchronizationWrapper(config.type, vk, usingTimelineSemaphores, 2u);
    VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo =
        makeCommonSemaphoreSubmitInfo(semaphore.get(), timelineValue, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR);
    VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
        makeCommonSemaphoreSubmitInfo(semaphore.get(), timelineValue, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);

    synchronizationWrapper->addSubmitInfo(
        0u,                         // uint32_t                                waitSemaphoreInfoCount
        nullptr,                    // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
        1u,                         // uint32_t                                commandBufferInfoCount
        &commandBufferInfo,         // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
        1u,                         // uint32_t                                signalSemaphoreInfoCount
        &signalSemaphoreSubmitInfo, // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
        false, usingTimelineSemaphores);
    synchronizationWrapper->addSubmitInfo(
        1u,                       // uint32_t                                waitSemaphoreInfoCount
        &waitSemaphoreSubmitInfo, // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
        1u,                       // uint32_t                                commandBufferInfoCount
        &commandBufferInfo,       // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
        0u,                       // uint32_t                                signalSemaphoreInfoCount
        nullptr,                  // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
        usingTimelineSemaphores, false);

    VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &info));
    endCommandBuffer(vk, *cmdBuffer);
    VK_CHECK(synchronizationWrapper->queueSubmit(queue, *fence));

    if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence.get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("Basic semaphore tests with one queue failed");

    return tcu::TestStatus::pass("Basic semaphore tests with one queue passed");
}

tcu::TestStatus noneWaitSubmitTest(Context &context, const TestConfig config)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    const Unique<VkSemaphore> semaphore(createTestSemaphore(context, vk, device, config));
    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));

    const Unique<VkCommandBuffer> firstbuffer(makeCommandBuffer(vk, device, *cmdPool));
    const Unique<VkCommandBuffer> secondBuffer(makeCommandBuffer(vk, device, *cmdPool));

    const VkCommandBufferBeginInfo info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType                          sType;
        nullptr,                                      // const void*                              pNext;
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // VkCommandBufferUsageFlags                flags;
        nullptr,                                      // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
    };
    const Unique<VkFence> fence1(createFence(vk, device));
    const Unique<VkFence> fence2(createFence(vk, device));
    const Unique<VkEvent> event(createEvent(vk, device));

    VK_CHECK(vk.beginCommandBuffer(*firstbuffer, &info));
    endCommandBuffer(vk, *firstbuffer);

    const VkSubmitInfo firstSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, //VkStructureType sType
        nullptr,                       //const void* pNext
        0u,                            //uint32_t waitSemaphoreCount
        nullptr,                       //const VkSemaphore* pWaitSemaphores
        nullptr,                       //const VkPipelineStageFlags* pWaitDstStageMask
        1u,                            //uint32_t commandBufferCount
        &firstbuffer.get(),            //const VkCommandBuffer* pCommandBuffers
        1,                             //uint32_t signalSemaphoreCount
        &semaphore.get()               //const VkSemaphore* pSignalSemaphores
    };

    //check if waiting on an event in the none stage works as expected
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_NONE_KHR};

    const VkSubmitInfo secondSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, //VkStructureType sType
        nullptr,                       //const void* pNext
        1u,                            //uint32_t waitSemaphoreCount
        &semaphore.get(),              //const VkSemaphore* pWaitSemaphores
        waitStages,                    //const VkPipelineStageFlags* pWaitDstStageMask
        1u,                            //uint32_t commandBufferCount
        &secondBuffer.get(),           //const VkCommandBuffer* pCommandBuffers
        0,                             //uint32_t signalSemaphoreCount
        nullptr                        //const VkSemaphore* pSignalSemaphores
    };

    VK_CHECK(vk.beginCommandBuffer(*secondBuffer, &info));
    vk.cmdSetEvent(*secondBuffer, event.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    endCommandBuffer(vk, *secondBuffer);

    VK_CHECK(vk.queueSubmit(queue, 1, &firstSubmitInfo, fence1.get()));
    VK_CHECK(vk.queueSubmit(queue, 1, &secondSubmitInfo, fence2.get()));
    VK_CHECK(vk.queueWaitIdle(queue));

    if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence1.get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("None stage test failed, failed to wait for fence");

    if (VK_SUCCESS != vk.waitForFences(device, 1u, &fence2.get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("None stage test failed, failed to wait for the second fence");

    if (vk.getEventStatus(device, event.get()) != VK_EVENT_SET)
        return tcu::TestStatus::fail("None stage test failed, event isn't set");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus basicChainCase(Context &context, TestConfig config)
{
    VkResult err                 = VK_SUCCESS;
    bool usingTimelineSemaphores = config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE;
    bool usingSync2              = config.type == SynchronizationType::SYNCHRONIZATION2;

    de::MovePtr<VideoDevice> videoDevice(
        config.videoCodecOperationFlags != 0 ?
            getVideoDevice(context, usingTimelineSemaphores, usingSync2, config.videoCodecOperationFlags) :
            nullptr);

    const DeviceInterface &vk = getSyncDeviceInterface(videoDevice, context);
    const VkDevice device     = getSyncDevice(videoDevice, context);
    const VkQueue queue       = getSyncQueue(videoDevice, context);
    VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
    VkFenceCreateInfo fci     = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    VkFence fence;
    std::vector<VkSemaphoreSubmitInfoKHR> waitSemaphoreSubmitInfos(
        basicChainLength,
        makeCommonSemaphoreSubmitInfo(VK_NULL_HANDLE, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));
    std::vector<VkSemaphoreSubmitInfoKHR> signalSemaphoreSubmitInfos(
        basicChainLength, makeCommonSemaphoreSubmitInfo(VK_NULL_HANDLE, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
    VkSemaphoreSubmitInfoKHR *pWaitSemaphoreInfo   = nullptr;
    VkSemaphoreSubmitInfoKHR *pSignalSemaphoreInfo = signalSemaphoreSubmitInfos.data();

    for (int i = 0; err == VK_SUCCESS && i < basicChainLength; i++)
    {
        if (i % (basicChainLength / 4) == 0)
            context.getTestContext().touchWatchdog();

        err = vk.createSemaphore(device, &sci, nullptr, &pSignalSemaphoreInfo->semaphore);
        if (err != VK_SUCCESS)
            continue;

        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(config.type, vk, false);
        synchronizationWrapper->addSubmitInfo(
            !!pWaitSemaphoreInfo, // uint32_t                                waitSemaphoreInfoCount
            pWaitSemaphoreInfo,   // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
            0u,                   // uint32_t                                commandBufferInfoCount
            nullptr,              // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
            1u,                   // uint32_t                                signalSemaphoreInfoCount
            pSignalSemaphoreInfo  // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
        );

        err                           = synchronizationWrapper->queueSubmit(queue, VK_NULL_HANDLE);
        pWaitSemaphoreInfo            = &waitSemaphoreSubmitInfos[i];
        pWaitSemaphoreInfo->semaphore = pSignalSemaphoreInfo->semaphore;
        pSignalSemaphoreInfo++;
    }

    VK_CHECK(vk.createFence(device, &fci, nullptr, &fence));

    {
        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(config.type, vk, false);
        synchronizationWrapper->addSubmitInfo(1, pWaitSemaphoreInfo, 0, nullptr, 0, nullptr);
        VK_CHECK(synchronizationWrapper->queueSubmit(queue, fence));
    }

    vk.waitForFences(device, 1, &fence, VK_TRUE, ~(0ull));
    vk.destroyFence(device, fence, nullptr);

    for (const auto &s : signalSemaphoreSubmitInfos)
        vk.destroySemaphore(device, s.semaphore, nullptr);

    if (err == VK_SUCCESS)
        return tcu::TestStatus::pass("Basic semaphore chain test passed");

    return tcu::TestStatus::fail("Basic semaphore chain test failed");
}

tcu::TestStatus basicChainTimelineCase(Context &context, TestConfig config)
{
    VkResult err    = VK_SUCCESS;
    bool usingSync2 = config.type == SynchronizationType::SYNCHRONIZATION2;
    de::MovePtr<VideoDevice> videoDevice(
        config.videoCodecOperationFlags != 0 ?
            getVideoDevice(context, true, usingSync2, config.videoCodecOperationFlags) :
            nullptr);

    const DeviceInterface &vk      = getSyncDeviceInterface(videoDevice, context);
    const VkDevice device          = getSyncDevice(videoDevice, context);
    const VkQueue queue            = getSyncQueue(videoDevice, context);
    VkSemaphoreTypeCreateInfo scti = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr, VK_SEMAPHORE_TYPE_TIMELINE,
                                      0};
    VkSemaphoreCreateInfo sci      = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &scti, 0};
    VkFenceCreateInfo fci          = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    VkSemaphore semaphore;
    VkFence fence;

    VK_CHECK(vk.createSemaphore(device, &sci, nullptr, &semaphore));

    std::vector<VkSemaphoreSubmitInfoKHR> waitSemaphoreSubmitInfos(
        basicChainLength, makeCommonSemaphoreSubmitInfo(semaphore, 0u, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR));
    std::vector<VkSemaphoreSubmitInfoKHR> signalSemaphoreSubmitInfos(
        basicChainLength, makeCommonSemaphoreSubmitInfo(semaphore, 0u, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR));
    VkSemaphoreSubmitInfoKHR *pWaitSemaphoreInfo   = nullptr;
    VkSemaphoreSubmitInfoKHR *pSignalSemaphoreInfo = signalSemaphoreSubmitInfos.data();

    for (int i = 0; err == VK_SUCCESS && i < basicChainLength; i++)
    {
        if (i % (basicChainLength / 4) == 0)
            context.getTestContext().touchWatchdog();

        pSignalSemaphoreInfo->value = static_cast<uint64_t>(i + 1);

        SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(config.type, vk, true);
        synchronizationWrapper->addSubmitInfo(
            !!pWaitSemaphoreInfo, // uint32_t                                waitSemaphoreInfoCount
            pWaitSemaphoreInfo,   // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
            0u,                   // uint32_t                                commandBufferInfoCount
            nullptr,              // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
            1u,                   // uint32_t                                signalSemaphoreInfoCount
            pSignalSemaphoreInfo, // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
            !!pWaitSemaphoreInfo, true);

        err = synchronizationWrapper->queueSubmit(queue, VK_NULL_HANDLE);

        pWaitSemaphoreInfo        = &waitSemaphoreSubmitInfos[i];
        pWaitSemaphoreInfo->value = static_cast<uint64_t>(i);
        pSignalSemaphoreInfo++;
    }

    pWaitSemaphoreInfo->value                        = basicChainLength;
    SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(config.type, vk, true);
    synchronizationWrapper->addSubmitInfo(
        1u,                 // uint32_t                                waitSemaphoreInfoCount
        pWaitSemaphoreInfo, // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
        0u,                 // uint32_t                                commandBufferInfoCount
        nullptr,            // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
        0u,                 // uint32_t                                signalSemaphoreInfoCount
        nullptr,            // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
        true);

    VK_CHECK(vk.createFence(device, &fci, nullptr, &fence));
    VK_CHECK(synchronizationWrapper->queueSubmit(queue, fence));
    vk.waitForFences(device, 1, &fence, VK_TRUE, ~(0ull));

    vk.destroyFence(device, fence, nullptr);
    vk.destroySemaphore(device, semaphore, nullptr);

    if (err == VK_SUCCESS)
        return tcu::TestStatus::pass("Basic semaphore chain test passed");

    return tcu::TestStatus::fail("Basic semaphore chain test failed");
}

tcu::TestStatus basicThreadTimelineCase(Context &context, TestConfig config)
{
    const VkSemaphoreTypeCreateInfo scti = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
                                            VK_SEMAPHORE_TYPE_TIMELINE, 0};
    bool usingSync2                      = config.type == SynchronizationType::SYNCHRONIZATION2;
    de::MovePtr<VideoDevice> videoDevice(
        config.videoCodecOperationFlags != 0 ?
            getVideoDevice(context, true, usingSync2, config.videoCodecOperationFlags) :
            nullptr);
    const DeviceInterface &vk       = getSyncDeviceInterface(videoDevice, context);
    const VkDevice device           = getSyncDevice(videoDevice, context);
    const VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &scti, 0};
    const VkFenceCreateInfo fci     = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    const vk::Unique<vk::VkSemaphore> semaphore(createSemaphore(vk, device, &sci));
    const Unique<VkFence> fence(createFence(vk, device, &fci));
    const uint64_t waitTimeout = 50ull * 1000000ull; // miliseconds
    VkResult threadResult      = VK_SUCCESS;

    // helper creating VkSemaphoreSignalInfo
    auto makeSemaphoreSignalInfo = [&semaphore](uint64_t value) -> VkSemaphoreSignalInfo
    {
        return {
            VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, // VkStructureType                sType
            nullptr,                                 // const void*                    pNext
            *semaphore,                              // VkSemaphore                    semaphore
            value                                    // uint64_t                        value
        };
    };

    // helper creating VkSemaphoreWaitInfo
    auto makeSemaphoreWaitInfo = [&semaphore](uint64_t *valuePtr) -> VkSemaphoreWaitInfo
    {
        return {
            VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, // VkStructureType                sType
            nullptr,                               // const void*                    pNext
            VK_SEMAPHORE_WAIT_ANY_BIT,             // VkSemaphoreWaitFlags flags;
            1u,                                    // uint32_t semaphoreCount;
            &*semaphore,                           // const VkSemaphore* pSemaphores;
            valuePtr                               // const uint64_t* pValues;
        };
    };

    // start thread - semaphore has value 0
    de::MovePtr<std::thread> thread(new std::thread(
        [=, &vk, &threadResult]
        {
            // wait till semaphore has value 1
            uint64_t waitValue          = 1;
            VkSemaphoreWaitInfo waitOne = makeSemaphoreWaitInfo(&waitValue);
            threadResult                = vk.waitSemaphores(device, &waitOne, waitTimeout);

            if (threadResult == VK_SUCCESS)
            {
                // signal semaphore with value 2
                VkSemaphoreSignalInfo signalTwo = makeSemaphoreSignalInfo(2);
                threadResult                    = vk.signalSemaphore(device, &signalTwo);
            }
        }));

    // wait some time to give thread chance to start
    deSleep(1); // milisecond

    // signal semaphore with value 1
    VkSemaphoreSignalInfo signalOne = makeSemaphoreSignalInfo(1);
    vk.signalSemaphore(device, &signalOne);

    // wait till semaphore has value 2
    uint64_t waitValue          = 2;
    VkSemaphoreWaitInfo waitTwo = makeSemaphoreWaitInfo(&waitValue);
    VkResult mainResult         = vk.waitSemaphores(device, &waitTwo, waitTimeout);

    thread->join();

    if (mainResult == VK_SUCCESS)
        return tcu::TestStatus::pass("Pass");

    if ((mainResult == VK_TIMEOUT) || (threadResult == VK_TIMEOUT))
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Reached wait timeout");

    return tcu::TestStatus::fail("Fail");
}

VkResult basicWaitForTimelineValueHelper(Context &context, TestConfig config, VkSemaphoreWaitFlags wait_flags,
                                         uint64_t signal_value, uint64_t wait_value)
{
    const VkSemaphoreTypeCreateInfo scti = {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
                                            VK_SEMAPHORE_TYPE_TIMELINE, 0};
    bool usingSync2                      = config.type == SynchronizationType::SYNCHRONIZATION2;
    de::MovePtr<VideoDevice> videoDevice(
        config.videoCodecOperationFlags != 0 ?
            getVideoDevice(context, true, usingSync2, config.videoCodecOperationFlags) :
            nullptr);
    const DeviceInterface &vk       = getSyncDeviceInterface(videoDevice, context);
    const VkDevice device           = getSyncDevice(videoDevice, context);
    const VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &scti, 0};
    const VkFenceCreateInfo fci     = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
    const vk::Unique<vk::VkSemaphore> semaphore(createSemaphore(vk, device, &sci));
    const Unique<VkFence> fence(createFence(vk, device, &fci));
    const uint64_t waitTimeout = 0; // return immediately

    // helper creating VkSemaphoreSignalInfo
    auto makeSemaphoreSignalInfo = [&semaphore](uint64_t value) -> VkSemaphoreSignalInfo
    {
        return {
            VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, // VkStructureType                sType
            nullptr,                                 // const void*                    pNext
            *semaphore,                              // VkSemaphore                    semaphore
            value                                    // uint64_t                        value
        };
    };

    // helper creating VkSemaphoreWaitInfo
    auto makeSemaphoreWaitInfo = [&semaphore](VkSemaphoreWaitFlags flags, uint64_t *valuePtr) -> VkSemaphoreWaitInfo
    {
        return {
            VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO, // VkStructureType                sType
            nullptr,                               // const void*                    pNext
            flags,                                 // VkSemaphoreWaitFlags flags;
            1u,                                    // uint32_t semaphoreCount;
            &*semaphore,                           // const VkSemaphore* pSemaphores;
            valuePtr                               // const uint64_t* pValues;
        };
    };

    VkSemaphoreSignalInfo signalTheValue = makeSemaphoreSignalInfo(signal_value);
    vk.signalSemaphore(device, &signalTheValue);

    VkSemaphoreWaitInfo waitForTheValue = makeSemaphoreWaitInfo(wait_flags, &wait_value);
    return vk.waitSemaphores(device, &waitForTheValue, waitTimeout);
}

tcu::TestStatus basicWaitForAnyCurrentTimelineValueCase(Context &context, TestConfig config)
{
    VkResult mainResult = basicWaitForTimelineValueHelper(context, config, VK_SEMAPHORE_WAIT_ANY_BIT, 1, 1);
    if (mainResult == VK_SUCCESS)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus basicWaitForAnyLesserTimelineValueCase(Context &context, TestConfig config)
{
    VkResult mainResult = basicWaitForTimelineValueHelper(context, config, VK_SEMAPHORE_WAIT_ANY_BIT, 4, 1);
    if (mainResult == VK_SUCCESS)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus basicWaitForAllCurrentTimelineValueCase(Context &context, TestConfig config)
{
    VkResult mainResult = basicWaitForTimelineValueHelper(context, config, 0, 1, 1);
    if (mainResult == VK_SUCCESS)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus basicWaitForAllLesserTimelineValueCase(Context &context, TestConfig config)
{
    VkResult mainResult = basicWaitForTimelineValueHelper(context, config, 0, 4, 1);
    if (mainResult == VK_SUCCESS)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus basicMultiQueueCase(Context &context, TestConfig config)
{
    enum
    {
        NO_MATCH_FOUND = ~((uint32_t)0)
    };
    enum QueuesIndexes
    {
        FIRST = 0,
        SECOND,
        COUNT
    };

    struct Queues
    {
        VkQueue queue;
        uint32_t queueFamilyIndex;
    };

#ifndef CTS_USES_VULKANSC
    const VkInstance instance                  = context.getInstance();
    const InstanceInterface &instanceInterface = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice      = context.getPhysicalDevice();
    bool usingTimelineSemaphores               = config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE;
    bool usingSync2                            = config.type == SynchronizationType::SYNCHRONIZATION2;

    std::vector<VkQueueFamilyVideoPropertiesKHR> videoQueueFamilyProperties2;
#else
    const CustomInstance instance(createCustomInstanceFromContext(context));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, context.getTestContext().getCommandLine());
    const InstanceInterface &instanceInterface = instanceDriver;
// const DeviceInterface& vk = context.getDeviceInterface();
// const InstanceInterface& instance = context.getInstanceInterface();
// const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
#endif // CTS_USES_VULKANSC
    vk::Move<vk::VkDevice> logicalDevice;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties2;
    VkDeviceCreateInfo deviceInfo;
    VkPhysicalDeviceFeatures deviceFeatures;
    const float queuePriorities[COUNT] = {1.0f, 1.0f};
    VkDeviceQueueCreateInfo queueInfos[COUNT];
    Queues queues[COUNT]                = {{nullptr, (uint32_t)NO_MATCH_FOUND}, {nullptr, (uint32_t)NO_MATCH_FOUND}};
    const VkCommandBufferBeginInfo info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  // VkStructureType                          sType;
        nullptr,                                      // const void*                              pNext;
        VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // VkCommandBufferUsageFlags                flags;
        nullptr,                                      // const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
    };

    const bool isTimelineSemaphore = config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE;

    if (config.videoCodecOperationFlags != 0)
    {
#ifndef CTS_USES_VULKANSC
        uint32_t queueFamilyPropertiesCount = 0;

        instanceInterface.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, nullptr);

        if (queueFamilyPropertiesCount > 0)
        {
            queueFamilyProperties2.resize(queueFamilyPropertiesCount);
            videoQueueFamilyProperties2.resize(queueFamilyPropertiesCount);

            for (size_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
            {
                queueFamilyProperties2[ndx].sType      = vk::VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
                queueFamilyProperties2[ndx].pNext      = &videoQueueFamilyProperties2[ndx];
                videoQueueFamilyProperties2[ndx].sType = vk::VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
                videoQueueFamilyProperties2[ndx].pNext = nullptr;
                videoQueueFamilyProperties2[ndx].videoCodecOperations = 0;
            }

            instanceInterface.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount,
                                                                      queueFamilyProperties2.data());

            if (queueFamilyPropertiesCount != queueFamilyProperties2.size())
                TCU_FAIL("Device returns less queue families than initially reported");

            queueFamilyProperties.reserve(queueFamilyPropertiesCount);

            for (size_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
                queueFamilyProperties.push_back(queueFamilyProperties2[ndx].queueFamilyProperties);
        }
#endif // CTS_USES_VULKANSC
    }
    else
    {
        queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instanceInterface, physicalDevice);
    }

    for (uint32_t queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
    {
#ifndef CTS_USES_VULKANSC
        const bool usableQueue =
            videoQueueFamilyProperties2.empty() ||
            (videoQueueFamilyProperties2[queueNdx].videoCodecOperations & config.videoCodecOperationFlags) != 0;

        if (!usableQueue)
            continue;
#endif // CTS_USES_VULKANSC

        if (NO_MATCH_FOUND == queues[FIRST].queueFamilyIndex)
            queues[FIRST].queueFamilyIndex = queueNdx;

        if (queues[FIRST].queueFamilyIndex != queueNdx || queueFamilyProperties[queueNdx].queueCount > 1u)
        {
            queues[SECOND].queueFamilyIndex = queueNdx;
            break;
        }
    }

    if (queues[FIRST].queueFamilyIndex == NO_MATCH_FOUND || queues[SECOND].queueFamilyIndex == NO_MATCH_FOUND)
        TCU_THROW(NotSupportedError, "Queues couldn't be created");

    for (int queueNdx = 0; queueNdx < COUNT; ++queueNdx)
    {
        VkDeviceQueueCreateInfo queueInfo;
        deMemset(&queueInfo, 0, sizeof(queueInfo));

        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.pNext            = nullptr;
        queueInfo.flags            = (VkDeviceQueueCreateFlags)0u;
        queueInfo.queueFamilyIndex = queues[queueNdx].queueFamilyIndex;
        queueInfo.queueCount       = (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex) ? 2 : 1;
        queueInfo.pQueuePriorities = queuePriorities;

        queueInfos[queueNdx] = queueInfo;

        if (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex)
            break;
    }

    deMemset(&deviceInfo, 0, sizeof(deviceInfo));
    instanceInterface.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    VkPhysicalDeviceFeatures2 createPhysicalFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr,
                                                    deviceFeatures};
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, nullptr, true};
    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, nullptr, true};
    void **nextPtr = &createPhysicalFeature.pNext;

    std::vector<const char *> deviceExtensions;

    if (config.videoCodecOperationFlags != 0)
        VideoDevice::addVideoDeviceExtensions(deviceExtensions, context.getUsedApiVersion(),
                                              VideoDevice::getQueueFlags(config.videoCodecOperationFlags),
                                              config.videoCodecOperationFlags);

    if (isTimelineSemaphore)
    {
        if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_timeline_semaphore"))
            deviceExtensions.push_back("VK_KHR_timeline_semaphore");
        addToChainVulkanStructure(&nextPtr, timelineSemaphoreFeatures);
    }
    if (config.type == SynchronizationType::SYNCHRONIZATION2)
    {
        deviceExtensions.push_back("VK_KHR_synchronization2");
        addToChainVulkanStructure(&nextPtr, synchronization2Features);
    }

    void *pNext = &createPhysicalFeature;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

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

    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = pNext;
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
    deviceInfo.enabledLayerCount       = 0u;
    deviceInfo.ppEnabledLayerNames     = nullptr;
    deviceInfo.pEnabledFeatures        = 0u;
    deviceInfo.queueCreateInfoCount = (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex) ? 1 : COUNT;
    deviceInfo.pQueueCreateInfos    = queueInfos;

    logicalDevice =
        createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                           context.getPlatformInterface(), instance, instanceInterface, physicalDevice, &deviceInfo);

#ifndef CTS_USES_VULKANSC
    de::MovePtr<VideoDevice> videoDevice(
        config.videoCodecOperationFlags != 0 ?
            getVideoDevice(context, usingTimelineSemaphores, usingSync2, config.videoCodecOperationFlags) :
            DE_NULL);

    de::MovePtr<vk::DeviceDriver> deviceDriver = de::MovePtr<DeviceDriver>(
        new DeviceDriver(context.getPlatformInterface(), instance, *logicalDevice, context.getUsedApiVersion(),
                         context.getTestContext().getCommandLine()));

    const DeviceInterface &vk = (videoDevice != DE_NULL) ? getSyncDeviceInterface(videoDevice, context) : *deviceDriver;

#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver =
        de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
            new DeviceDriverSC(context.getPlatformInterface(), instance, *logicalDevice,
                               context.getTestContext().getCommandLine(), context.getResourceInterface(),
                               context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(),
                               context.getUsedApiVersion()),
            vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *logicalDevice));
    const DeviceInterface &vk = *deviceDriver;
#endif // CTS_USES_VULKANSC

    for (uint32_t queueReqNdx = 0; queueReqNdx < COUNT; ++queueReqNdx)
    {
        if (queues[FIRST].queueFamilyIndex == queues[SECOND].queueFamilyIndex)
            vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, queueReqNdx,
                              &queues[queueReqNdx].queue);
        else
            vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, 0u, &queues[queueReqNdx].queue);
    }

    Move<VkSemaphore> semaphore;
    Move<VkCommandPool> cmdPool[COUNT];
    Move<VkCommandBuffer> cmdBuffer[COUNT];
    uint64_t timelineValues[COUNT] = {1ull, 2ull};
    Move<VkFence> fence[COUNT];

    semaphore         = (createTestSemaphore(context, vk, *logicalDevice, config));
    cmdPool[FIRST]    = (createCommandPool(vk, *logicalDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                           queues[FIRST].queueFamilyIndex));
    cmdPool[SECOND]   = (createCommandPool(vk, *logicalDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                           queues[SECOND].queueFamilyIndex));
    cmdBuffer[FIRST]  = (makeCommandBuffer(vk, *logicalDevice, *cmdPool[FIRST]));
    cmdBuffer[SECOND] = (makeCommandBuffer(vk, *logicalDevice, *cmdPool[SECOND]));

    VK_CHECK(vk.beginCommandBuffer(*cmdBuffer[FIRST], &info));
    endCommandBuffer(vk, *cmdBuffer[FIRST]);
    VK_CHECK(vk.beginCommandBuffer(*cmdBuffer[SECOND], &info));
    endCommandBuffer(vk, *cmdBuffer[SECOND]);

    fence[FIRST]  = (createFence(vk, *logicalDevice));
    fence[SECOND] = (createFence(vk, *logicalDevice));

    VkCommandBufferSubmitInfoKHR commandBufferInfo[]{makeCommonCommandBufferSubmitInfo(*cmdBuffer[FIRST]),
                                                     makeCommonCommandBufferSubmitInfo(*cmdBuffer[SECOND])};

    VkSemaphoreSubmitInfoKHR signalSemaphoreSubmitInfo[]{
        makeCommonSemaphoreSubmitInfo(semaphore.get(), timelineValues[FIRST],
                                      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR),
        makeCommonSemaphoreSubmitInfo(semaphore.get(), timelineValues[SECOND],
                                      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR)};
    VkSemaphoreSubmitInfoKHR waitSemaphoreSubmitInfo =
        makeCommonSemaphoreSubmitInfo(semaphore.get(), timelineValues[FIRST], VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR);

    {
        SynchronizationWrapperPtr synchronizationWrapper[]{
            getSynchronizationWrapper(config.type, vk, isTimelineSemaphore),
            getSynchronizationWrapper(config.type, vk, isTimelineSemaphore)};
        synchronizationWrapper[FIRST]->addSubmitInfo(
            0u,                                // uint32_t                                waitSemaphoreInfoCount
            nullptr,                           // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
            1u,                                // uint32_t                                commandBufferInfoCount
            &commandBufferInfo[FIRST],         // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
            1u,                                // uint32_t                                signalSemaphoreInfoCount
            &signalSemaphoreSubmitInfo[FIRST], // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
            false, isTimelineSemaphore);
        synchronizationWrapper[SECOND]->addSubmitInfo(
            1u,                                 // uint32_t                                waitSemaphoreInfoCount
            &waitSemaphoreSubmitInfo,           // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
            1u,                                 // uint32_t                                commandBufferInfoCount
            &commandBufferInfo[SECOND],         // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
            1u,                                 // uint32_t                                signalSemaphoreInfoCount
            &signalSemaphoreSubmitInfo[SECOND], // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
            isTimelineSemaphore, isTimelineSemaphore);
        VK_CHECK(synchronizationWrapper[FIRST]->queueSubmit(queues[FIRST].queue, *fence[FIRST]));
        VK_CHECK(synchronizationWrapper[SECOND]->queueSubmit(queues[SECOND].queue, *fence[SECOND]));
    }

    if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[FIRST].get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

    if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[SECOND].get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

    if (isTimelineSemaphore)
    {
        signalSemaphoreSubmitInfo[FIRST].value  = 3ull;
        signalSemaphoreSubmitInfo[SECOND].value = 4ull;
        waitSemaphoreSubmitInfo.value           = 3ull;
    }

    // swap semaphore info compared to above submits
    {
        SynchronizationWrapperPtr synchronizationWrapper[]{
            getSynchronizationWrapper(config.type, vk, isTimelineSemaphore),
            getSynchronizationWrapper(config.type, vk, isTimelineSemaphore)};
        synchronizationWrapper[FIRST]->addSubmitInfo(
            1u,                                 // uint32_t                                waitSemaphoreInfoCount
            &waitSemaphoreSubmitInfo,           // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
            1u,                                 // uint32_t                                commandBufferInfoCount
            &commandBufferInfo[FIRST],          // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
            1u,                                 // uint32_t                                signalSemaphoreInfoCount
            &signalSemaphoreSubmitInfo[SECOND], // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
            isTimelineSemaphore, isTimelineSemaphore);
        synchronizationWrapper[SECOND]->addSubmitInfo(
            isTimelineSemaphore ? 0u : 1u, // uint32_t                                waitSemaphoreInfoCount
            isTimelineSemaphore ?
                nullptr :
                &waitSemaphoreSubmitInfo,      // const VkSemaphoreSubmitInfoKHR*        pWaitSemaphoreInfos
            1u,                                // uint32_t                                commandBufferInfoCount
            &commandBufferInfo[SECOND],        // const VkCommandBufferSubmitInfoKHR*    pCommandBufferInfos
            1u,                                // uint32_t                                signalSemaphoreInfoCount
            &signalSemaphoreSubmitInfo[FIRST], // const VkSemaphoreSubmitInfoKHR*        pSignalSemaphoreInfos
            false, isTimelineSemaphore);

        VK_CHECK(vk.resetFences(*logicalDevice, 1u, &fence[FIRST].get()));
        VK_CHECK(vk.resetFences(*logicalDevice, 1u, &fence[SECOND].get()));
        VK_CHECK(synchronizationWrapper[SECOND]->queueSubmit(queues[SECOND].queue, *fence[SECOND]));
        VK_CHECK(synchronizationWrapper[FIRST]->queueSubmit(queues[FIRST].queue, *fence[FIRST]));
    }

    if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[FIRST].get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

    if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence[SECOND].get(), true, FENCE_WAIT))
        return tcu::TestStatus::fail("Basic semaphore tests with multi queue failed");

    return tcu::TestStatus::pass("Basic semaphore tests with multi queue passed");
}

void checkSupport(Context &context, TestConfig config)
{
    if (config.videoCodecOperationFlags != 0)
        VideoDevice::checkSupport(context, config.videoCodecOperationFlags);

    if (config.semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE)
        context.requireDeviceFunctionality("VK_KHR_timeline_semaphore");

    if (config.type == SynchronizationType::SYNCHRONIZATION2)
        context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

void checkCommandBufferSimultaneousUseSupport(Context &context, TestConfig config)
{
    checkSupport(context, config);

#ifdef CTS_USES_VULKANSC
    if (context.getDeviceVulkanSC10Properties().commandBufferSimultaneousUse == VK_FALSE)
        TCU_THROW(NotSupportedError, "commandBufferSimultaneousUse is not supported");
#endif
}

} // namespace

tcu::TestCaseGroup *createBasicBinarySemaphoreTests(tcu::TestContext &testCtx, SynchronizationType type,
                                                    VideoCodecOperationFlags videoCodecOperationFlags)
{
    de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "binary_semaphore"));

    TestConfig config = {
        0,
        VK_SEMAPHORE_TYPE_BINARY,
        type,
        videoCodecOperationFlags,
    };
    for (uint32_t typedCreate = 0; typedCreate < 2; typedCreate++)
    {
        config.useTypeCreate         = (typedCreate != 0);
        const std::string createName = config.useTypeCreate ? "_typed" : "";

        // Basic binary semaphore tests with one queue
        addFunctionCase(basicTests.get(), "one_queue" + createName, checkCommandBufferSimultaneousUseSupport,
                        basicOneQueueCase, config);
        // Basic binary semaphore tests with multi queue
        addFunctionCase(basicTests.get(), "multi_queue" + createName, checkCommandBufferSimultaneousUseSupport,
                        basicMultiQueueCase, config);
    }

    if (type == SynchronizationType::SYNCHRONIZATION2)
        // Test waiting on the none pipeline stage
        addFunctionCase(basicTests.get(), "none_wait_submit", checkCommandBufferSimultaneousUseSupport,
                        noneWaitSubmitTest, config);

    // Binary semaphore chain test
    addFunctionCase(basicTests.get(), "chain", checkSupport, basicChainCase, config);

    return basicTests.release();
}

tcu::TestCaseGroup *createBasicTimelineSemaphoreTests(tcu::TestContext &testCtx, SynchronizationType type,
                                                      VideoCodecOperationFlags videoCodecOperationFlags)
{
    // Basic timeline semaphore tests
    de::MovePtr<tcu::TestCaseGroup> basicTests(new tcu::TestCaseGroup(testCtx, "timeline_semaphore"));
    const TestConfig config = {
        true,
        VK_SEMAPHORE_TYPE_TIMELINE,
        type,
        videoCodecOperationFlags,
    };

    // Basic timeline semaphore tests with one queue
    addFunctionCase(basicTests.get(), "one_queue", checkCommandBufferSimultaneousUseSupport, basicOneQueueCase, config);
    // Basic timeline semaphore tests with multi queue
    addFunctionCase(basicTests.get(), "multi_queue", checkCommandBufferSimultaneousUseSupport, basicMultiQueueCase,
                    config);
    // Timeline semaphore chain test
    addFunctionCase(basicTests.get(), "chain", checkSupport, basicChainTimelineCase, config);

    // dont repeat this test for synchronization2
    if (type == SynchronizationType::LEGACY)
    {
        // Timeline semaphore used by two threads
        addFunctionCase(basicTests.get(), "two_threads", checkSupport, basicThreadTimelineCase, config);
        // Wait for the currently signalled timeline semaphore value (wait for any)
        addFunctionCase(basicTests.get(), "wait_for_any_current_value", checkSupport,
                        basicWaitForAnyCurrentTimelineValueCase, config);
        // Wait for a value less than the currently signalled timeline semaphore value (wait for any)
        addFunctionCase(basicTests.get(), "wait_for_any_lesser_value", checkSupport,
                        basicWaitForAnyLesserTimelineValueCase, config);
        // Wait for the currently signalled timeline semaphore value (wait for all)
        addFunctionCase(basicTests.get(), "wait_for_all_current_value", checkSupport,
                        basicWaitForAllCurrentTimelineValueCase, config);
        // Wait for a value less than the currently signalled timeline semaphore value (wait for all)
        addFunctionCase(basicTests.get(), "wait_for_all_lesser_value", checkSupport,
                        basicWaitForAllLesserTimelineValueCase, config);
    }

    return basicTests.release();
}

} // namespace synchronization
} // namespace vkt
