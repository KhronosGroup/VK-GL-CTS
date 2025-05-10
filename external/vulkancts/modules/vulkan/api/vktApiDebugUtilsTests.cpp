/*-------------------------------------------------------------------------
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
 * \brief Debug utils Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiTests.hpp"
#include "vktApiDebugUtilsTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktCustomInstancesDevices.hpp"
#include <vector>
#include <string>

namespace vkt::api
{

namespace
{

using namespace vk;

tcu::TestStatus testLongDebugLabelsTest(Context &context)
{
    // create custom instance to test debug_utils regardles of validation beeing enabled
    const int queueFamilyIndex   = 0;
    const uint32_t apiVersion    = context.getUsedApiVersion();
    const auto &cmdLine          = context.getTestContext().getCommandLine();
    const PlatformInterface &vkp = context.getPlatformInterface();
    std::vector<std::string> enabledExtensions{"VK_EXT_debug_utils"};
    const Unique<VkInstance> instance(createDefaultInstance(vkp, apiVersion, {}, enabledExtensions, cmdLine));
    const InstanceDriver vki(vkp, *instance);
    const VkPhysicalDevice physicalDevice = chooseDevice(vki, *instance, cmdLine);

    void *pNextForDeviceCreateInfo                = nullptr;
    void *pNextForCommandPoolCreateInfo           = nullptr;
    const float queuePriority                     = 1.0f;
    const auto queueFamilyProperties              = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
    VkDeviceQueueCreateInfo deviceQueueCreateInfo = initVulkanStructure();
    deviceQueueCreateInfo.queueCount              = 1;
    deviceQueueCreateInfo.pQueuePriorities        = &queuePriority;

#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = initVulkanStructure();
    memReservationInfo.commandBufferRequestCount           = 1;
    memReservationInfo.fenceRequestCount                   = 1;
    memReservationInfo.deviceMemoryRequestCount            = 1;
    memReservationInfo.bufferRequestCount                  = 1;
    memReservationInfo.commandPoolRequestCount             = 1;

    VkPhysicalDeviceVulkanSC10Features sc10Features = initVulkanStructure(&memReservationInfo);
    pNextForDeviceCreateInfo                        = &sc10Features;

    VkCommandPoolMemoryReservationCreateInfo memoryReservationCreateInfo = initVulkanStructure();
    memoryReservationCreateInfo.commandPoolReservedSize                  = 64u * cmdLine.getCommandDefaultSize();
    memoryReservationCreateInfo.commandPoolMaxCommandBuffers             = 1;
    pNextForCommandPoolCreateInfo                                        = &memoryReservationCreateInfo;
#endif // CTS_USES_VULKANSC

    VkDeviceCreateInfo deviceCreateInfo   = initVulkanStructure(pNextForDeviceCreateInfo);
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos    = &deviceQueueCreateInfo;

    const Unique<VkDevice> device(createCustomDevice(false, vkp, *instance, vki, physicalDevice, &deviceCreateInfo));
    const DeviceDriver vk(vkp, *instance, *device, apiVersion, cmdLine);
    const VkQueue queue = getDeviceQueue(vk, *device, queueFamilyIndex, 0);
    SimpleAllocator allocator(vk, *device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

    std::string longName(64 * 1024 + 1, 'x');

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto bufferInfo    = makeBufferCreateInfo(1024, usage);
    BufferWithMemory testBuffer(vk, *device, allocator, bufferInfo, MemoryRequirement::HostVisible);

    // test extremely long debug object names
    VkDebugUtilsObjectNameInfoEXT nameInfo = initVulkanStructure();
    nameInfo.objectType                    = VK_OBJECT_TYPE_BUFFER;
    nameInfo.objectHandle                  = (*testBuffer).getInternal();
    nameInfo.pObjectName                   = longName.c_str();
    vk.setDebugUtilsObjectNameEXT(*device, &nameInfo);

    VkDebugUtilsLabelEXT insertLabelInfo = initVulkanStructure();
    insertLabelInfo.pLabelName           = longName.c_str();

    VkCommandPoolCreateInfo cmdPoolCreateInfo = initVulkanStructure(pNextForCommandPoolCreateInfo);
    cmdPoolCreateInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.queueFamilyIndex        = queueFamilyIndex;
    auto cmdPool                              = createCommandPool(vk, *device, &cmdPoolCreateInfo);
    auto cmdBuffer = allocateCommandBuffer(vk, *device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdInsertDebugUtilsLabelEXT(*cmdBuffer, &insertLabelInfo);
    vk.cmdFillBuffer(*cmdBuffer, *testBuffer, 0, VK_WHOLE_SIZE, 1985);
    endCommandBuffer(vk, *cmdBuffer);

    vk.queueInsertDebugUtilsLabelEXT(queue, &insertLabelInfo);

    submitCommandsAndWait(vk, *device, queue, *cmdBuffer);

    return tcu::TestStatus::pass("Pass");
}

void checkDebugUtilsSupport(Context &context)
{
    context.requireInstanceFunctionality("VK_EXT_debug_utils");
}

} // namespace

tcu::TestCaseGroup *createDebugUtilsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> debugUtilsTests(new tcu::TestCaseGroup(testCtx, "debug_utils"));

    addFunctionCase(debugUtilsTests.get(), "long_labels", checkDebugUtilsSupport, testLongDebugLabelsTest);

    return debugUtilsTests.release();
}

} // namespace vkt::api
