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

struct TestParams
{
    VkQueueFlags required;
    VkQueueFlags excluded;
};

tcu::TestStatus testLongDebugLabelsTest(Context &context, TestParams params)
{
    // create custom instance to test debug_utils regardles of validation beeing enabled
    const auto instance = InstanceWrapper(createCustomInstanceWithExtension(context, "VK_EXT_debug_utils"));
    const auto &vki     = instance.getDriver();
    const VkPhysicalDevice physicalDevice = instance.getPhysicalDevice();
    int queueFamilyIndex = findQueueFamilyIndexWithCaps(vki, physicalDevice, params.required, params.excluded);

    void *pNextForDeviceCreateInfo                = nullptr;
    void *pNextForCommandPoolCreateInfo           = nullptr;
    const float queuePriority                     = 1.0f;
    const auto queueFamilyProperties              = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
    VkDeviceQueueCreateInfo deviceQueueCreateInfo = initVulkanStructure();
    deviceQueueCreateInfo.queueFamilyIndex        = queueFamilyIndex;
    deviceQueueCreateInfo.queueCount              = 1;
    deviceQueueCreateInfo.pQueuePriorities        = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo   = initVulkanStructure(pNextForDeviceCreateInfo);
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos    = &deviceQueueCreateInfo;

    const auto device        = instance.createCustomDevice(physicalDevice, &deviceCreateInfo);
    const auto &vk           = device.getDriver();
    const VkQueue queue      = getDeviceQueue(vk, *device, queueFamilyIndex, 0);
    vk::Allocator &allocator = device.getAllocator();

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
    if (params.required & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
    {
        vk.cmdFillBuffer(*cmdBuffer, *testBuffer, 0, VK_WHOLE_SIZE, 1985);
    }
    endCommandBuffer(vk, *cmdBuffer);

    vk.queueInsertDebugUtilsLabelEXT(queue, &insertLabelInfo);

    submitCommandsAndWait(vk, *device, queue, *cmdBuffer);

    return tcu::TestStatus::pass("Pass");
}

#ifndef CTS_USES_VULKANSC
tcu::TestStatus testDebugMarker(Context &context, TestParams)
{
    const auto &vk  = context.getDeviceInterface();
    auto device     = context.getDevice();
    auto &allocator = context.getDefaultAllocator();

    std::string longName(64 * 1024 + 1, 'x');

    // create a small buffer to name/tag
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto bufferInfo    = makeBufferCreateInfo(256u, usage);
    BufferWithMemory testBuffer(vk, device, allocator, bufferInfo, MemoryRequirement::HostVisible);

    // set object name
    VkDebugMarkerObjectNameInfoEXT nameInfo = initVulkanStructure();
    nameInfo.objectType                     = static_cast<VkDebugReportObjectTypeEXT>(VK_OBJECT_TYPE_BUFFER);
    nameInfo.object                         = (*testBuffer).getInternal();
    nameInfo.pObjectName                    = longName.c_str();
    vk.debugMarkerSetObjectNameEXT(device, &nameInfo);

    // set object tag
    const uint64_t tagName                = 0xCAFEBABEULL;
    uint32_t tagData                      = 0x12345678u;
    VkDebugMarkerObjectTagInfoEXT tagInfo = initVulkanStructure();
    tagInfo.objectType                    = nameInfo.objectType;
    tagInfo.object                        = nameInfo.object;
    tagInfo.tagName                       = tagName;
    tagInfo.tagSize                       = sizeof(tagData);
    tagInfo.pTag                          = &tagData;
    vk.debugMarkerSetObjectTagEXT(device, &tagInfo);

    // create command pool and buffer and use marker commands
    auto cmdPool   = makeCommandPool(vk, device, context.getUniversalQueueFamilyIndex());
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkDebugMarkerMarkerInfoEXT markerInfo = initVulkanStructure();
    markerInfo.pMarkerName                = longName.c_str();
    markerInfo.color[0]                   = 0.0f;
    markerInfo.color[1]                   = 1.0f;
    markerInfo.color[2]                   = 0.0f;
    markerInfo.color[3]                   = 1.0f;

    // do a simple command between begin/insert/end
    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdDebugMarkerBeginEXT(*cmdBuffer, &markerInfo);
    vk.cmdFillBuffer(*cmdBuffer, *testBuffer, 0, VK_WHOLE_SIZE, 0xDEADBEEFu);
    vk.cmdDebugMarkerInsertEXT(*cmdBuffer, &markerInfo);
    vk.cmdDebugMarkerEndEXT(*cmdBuffer);
    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

    return tcu::TestStatus::pass("Pass");
}
#endif // CTS_USES_VULKANSC

void checkDebugUtilsSupport(Context &context, TestParams params)
{
    context.requireInstanceFunctionality("VK_EXT_debug_utils");

    findQueueFamilyIndexWithCaps(context.getInstanceInterface(), context.getPhysicalDevice(), params.required,
                                 params.excluded);
}

#ifndef CTS_USES_VULKANSC
void checkDebugMarkerSupport(Context &context, TestParams)
{
    context.requireDeviceFunctionality("VK_EXT_debug_marker");
}
#endif // CTS_USES_VULKANSC

} // namespace

tcu::TestCaseGroup *createDebugUtilsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> debugUtilsTests(new tcu::TestCaseGroup(testCtx, "debug_utils"));

    TestParams params;
    params.required = VK_QUEUE_GRAPHICS_BIT;
    params.excluded = 0;
    addFunctionCase(debugUtilsTests.get(), "long_labels_graphics", checkDebugUtilsSupport, testLongDebugLabelsTest,
                    params);

#ifndef CTS_USES_VULKANSC
    addFunctionCase(debugUtilsTests.get(), "debug_marker_graphics", checkDebugMarkerSupport, testDebugMarker, params);
#endif // CTS_USES_VULKANSC

    params.required = VK_QUEUE_TRANSFER_BIT;
    params.excluded = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    addFunctionCase(debugUtilsTests.get(), "long_labels_transfer", checkDebugUtilsSupport, testLongDebugLabelsTest,
                    params);

#ifndef CTS_USES_VULKANSC
    params.required = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
    params.excluded = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    addFunctionCase(debugUtilsTests.get(), "long_labels_video_decode", checkDebugUtilsSupport, testLongDebugLabelsTest,
                    params);
#endif

    return debugUtilsTests.release();
}

} // namespace vkt::api
