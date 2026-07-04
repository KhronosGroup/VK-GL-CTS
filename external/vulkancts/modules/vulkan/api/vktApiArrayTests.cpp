/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief Tests for edge cases of api calls enumerating arrays
*//*--------------------------------------------------------------------*/

#include <array>

#include "vktApiArrayTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkWsiUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;

enum class ArrayFunction
{
    EnumeratePhysicalDevices,
    EnumerateInstanceLayerProperties,
    EnumerateInstanceExtensionProperties,
    EnumerateDeviceExtensionProperties,
    GetPipelineCacheData,
    CreatePipelineBinaries,
    EnumeratePhysicalDeviceGroups,
    GetPhysicalDeviceCalibrateableTimeDomains,
    GetPhysicalDeviceCooperativeMatrixProperties,
    GetPipelineExecutableProperties,
    GetPipelineExecutableStatistics,
    GetPipelineExecutableInternalRepresentations,
    GetPhysicalDeviceToolProperties,
    GetPhysicalDeviceFragmentShadingRates,

    Count,
};

enum class WsiArrayFunction
{
    GetPhysicalDeviceDisplayProperties,
    GetPhysicalDeviceDisplayPlaneProperties,
    GetDisplayPlaneSupportedDisplays,
    GetDisplayModeProperties,
    GetPhysicalDeviceSurfaceFormats,
    GetPhysicalDeviceSurfacePresentModes,
    GetSwapchainImages,
    GetPhysicalDevicePresentRectangles,
    GetPhysicalDeviceSurfaceFormats2,
    GetPhysicalDeviceDisplayProperties2,
    GetPhysicalDeviceDisplayPlaneProperties2,
    GetDisplayModeProperties2,
    GetPhysicalDeviceSurfacePresentModes2,

    Count,
};

struct WsiTestParams
{
    WsiArrayFunction func;
    wsi::Type wsiType;
};

template <typename T, typename R>
VkResult arrayFunc(Context &context, T handle, size_t &count, R *array);

template <>
VkResult arrayFunc(Context &context, VkInstance instance, size_t &count, VkPhysicalDevice *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.enumeratePhysicalDevices(instance, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkInstance instance, size_t &count, VkLayerProperties *array)
{
    (void)instance;
    const PlatformInterface &vkp = context.getPlatformInterface();

    return vkp.enumerateInstanceLayerProperties((uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkInstance instance, size_t &count, VkExtensionProperties *array)
{
    (void)instance;
    const PlatformInterface &vkp = context.getPlatformInterface();

    return vkp.enumerateInstanceExtensionProperties(nullptr, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count, VkExtensionProperties *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.enumerateDeviceExtensionProperties(physicalDevice, nullptr, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkInstance instance, size_t &count, VkPhysicalDeviceGroupProperties *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.enumeratePhysicalDeviceGroups(instance, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPipelineCache pipelineCache, size_t &count, uint8_t *array)
{
    const VkDevice device     = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    return vk.getPipelineCacheData(device, pipelineCache, &count, (void *)array);
}

template <>
VkResult arrayFunc(Context &context, VkPipeline pipeline, size_t &count, VkPipelineBinaryKHR *array)
{
    const VkDevice device     = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    VkPipelineBinaryCreateInfoKHR pipelineBinaryCreateInfo = initVulkanStructure();
    pipelineBinaryCreateInfo.pipeline                      = pipeline;

    VkPipelineBinaryHandlesInfoKHR pipelineBinaryHandlesInfo = initVulkanStructure();
    pipelineBinaryHandlesInfo.pipelineBinaryCount            = (uint32_t)count;
    pipelineBinaryHandlesInfo.pPipelineBinaries              = array;

    VkResult result =
        vk.createPipelineBinariesKHR(device, &pipelineBinaryCreateInfo, nullptr, &pipelineBinaryHandlesInfo);

    count = pipelineBinaryHandlesInfo.pipelineBinaryCount;

    return result;
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count, VkTimeDomainKHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceCalibrateableTimeDomainsKHR(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count,
                   VkCooperativeMatrixPropertiesKHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPipeline pipeline, size_t &count, VkPipelineExecutablePropertiesKHR *array)
{
    const VkDevice device     = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    VkPipelineInfoKHR pipelineInfo = initVulkanStructure();
    pipelineInfo.pipeline          = pipeline;

    return vk.getPipelineExecutablePropertiesKHR(device, &pipelineInfo, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPipeline pipeline, size_t &count, VkPipelineExecutableStatisticKHR *array)
{
    const VkDevice device     = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    VkPipelineExecutableInfoKHR executableInfo = initVulkanStructure();
    executableInfo.pipeline                    = pipeline;
    executableInfo.executableIndex             = 0;

    return vk.getPipelineExecutableStatisticsKHR(device, &executableInfo, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPipeline pipeline, size_t &count,
                   VkPipelineExecutableInternalRepresentationKHR *array)
{
    const VkDevice device     = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    VkPipelineExecutableInfoKHR executableInfo = initVulkanStructure();
    executableInfo.pipeline                    = pipeline;
    executableInfo.executableIndex             = 0;

    return vk.getPipelineExecutableInternalRepresentationsKHR(device, &executableInfo, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count,
                   VkPhysicalDeviceToolProperties *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceToolProperties(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count,
                   VkPhysicalDeviceFragmentShadingRateKHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkSwapchainKHR swapchain, size_t &count, VkImage *array)
{
    const VkDevice device     = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    return vk.getSwapchainImagesKHR(device, swapchain, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count, VkDisplayPropertiesKHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceDisplayPropertiesKHR(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count, VkDisplayPlanePropertiesKHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count, VkDisplayKHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();
    uint32_t displayPane         = 0;

    return vki.getDisplayPlaneSupportedDisplaysKHR(physicalDevice, displayPane, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkDisplayKHR display, size_t &count, VkDisplayModePropertiesKHR *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getDisplayModePropertiesKHR(physicalDevice, display, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkSurfaceKHR surface, size_t &count, VkSurfaceFormatKHR *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkSurfaceKHR surface, size_t &count, VkPresentModeKHR *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkSurfaceKHR surface, size_t &count, VkRect2D *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getPhysicalDevicePresentRectanglesKHR(physicalDevice, surface, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDevice physicalDevice, size_t &count,
                   VkDisplayPlaneProperties2KHR *array)
{
    const InstanceInterface &vki = context.getInstanceInterface();

    return vki.getPhysicalDeviceDisplayPlaneProperties2KHR(physicalDevice, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkDisplayKHR display, size_t &count, VkDisplayModeProperties2KHR *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getDisplayModeProperties2KHR(physicalDevice, display, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo, size_t &count,
                   VkSurfaceFormat2KHR *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, (uint32_t *)&count, array);
}

template <>
VkResult arrayFunc(Context &context, VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo, size_t &count,
                   VkPresentModeKHR *array)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const InstanceInterface &vki          = context.getInstanceInterface();

    return vki.getPhysicalDeviceSurfacePresentModes2EXT(physicalDevice, &surfaceInfo, (uint32_t *)&count, array);
}

Move<VkPipeline> createGraphicsPipeline(Context &context)
{
    const VkDevice vkDevice   = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    const tcu::IVec2 renderSize(256, 256);

    const Unique<VkRenderPass> renderPass(makeRenderPass(vk, vkDevice, VK_FORMAT_R8G8B8A8_UNORM));

    // Pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutParams = initVulkanStructure();
    const Unique<VkPipelineLayout> pipelineLayout(createPipelineLayout(vk, vkDevice, &pipelineLayoutParams));

    const Unique<VkShaderModule> vertShaderModule(
        createShaderModule(vk, vkDevice, context.getBinaryCollection().get("vert"), 0));
    const Unique<VkShaderModule> fragShaderModule(
        createShaderModule(vk, vkDevice, context.getBinaryCollection().get("frag"), 0));

    // Pipeline
    const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
    const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

    VkPipelineCreateFlags2CreateInfo pipelineCreateFlags2 = initVulkanStructure();

    pipelineCreateFlags2.flags = VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR |
                                 VK_PIPELINE_CREATE_2_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR |
                                 VK_PIPELINE_CREATE_2_CAPTURE_STATISTICS_BIT_KHR;

    return makeGraphicsPipeline(vk,                // const DeviceInterface&            vk
                                vkDevice,          // const VkDevice                    device
                                *pipelineLayout,   // const VkPipelineLayout            pipelineLayout
                                *vertShaderModule, // const VkShaderModule              vertexShaderModule
                                VK_NULL_HANDLE,    // const VkShaderModule              tessellationControlModule
                                VK_NULL_HANDLE,    // const VkShaderModule              tessellationEvalModule
                                VK_NULL_HANDLE,    // const VkShaderModule              geometryShaderModule
                                *fragShaderModule, // const VkShaderModule              fragmentShaderModule
                                *renderPass,       // const VkRenderPass                renderPass
                                viewports,         // const std::vector<VkViewport>&    viewports
                                scissors,          // const std::vector<VkRect2D>&      scissors
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology topology
                                0u,                                  // const uint32_t subpass = 0u
                                0u,                                  // const uint32_t patchControlPoints = 0u
                                nullptr, // const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo
                                nullptr, // const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo
                                nullptr, // const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo
                                nullptr, // const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo
                                nullptr, // const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo
                                nullptr, // const VkPipelineDynamicStateCreateInfo *dynamicStateCreateInfo
                                (const void *)&pipelineCreateFlags2); // const void *pNext,
}

template <typename T>
T poisonVulkanStructure()
{
    T s;
    deMemset(&s, 0xDE, sizeof(T));
    s.sType = getStructureType<T>();
    s.pNext = nullptr;
    return s;
}

template <typename T>
T poison()
{
    T t;
    deMemset(&t, 0xDE, sizeof(T));
    return t;
}

template <>
VkPhysicalDeviceGroupProperties poison()
{
    return poisonVulkanStructure<VkPhysicalDeviceGroupProperties>();
}

template <>
VkCooperativeMatrixPropertiesKHR poison()
{
    return poisonVulkanStructure<VkCooperativeMatrixPropertiesKHR>();
}

template <>
VkPipelineExecutablePropertiesKHR poison()
{
    return poisonVulkanStructure<VkPipelineExecutablePropertiesKHR>();
}

template <>
VkPipelineExecutableStatisticKHR poison()
{
    return poisonVulkanStructure<VkPipelineExecutableStatisticKHR>();
}

template <>
VkPipelineExecutableInternalRepresentationKHR poison()
{
    VkPipelineExecutableInternalRepresentationKHR s =
        poisonVulkanStructure<VkPipelineExecutableInternalRepresentationKHR>();

    s.dataSize = 0;
    s.pData    = nullptr;

    return s;
}

template <>
VkPhysicalDeviceToolProperties poison()
{
    return poisonVulkanStructure<VkPhysicalDeviceToolProperties>();
}

template <>
VkPhysicalDeviceFragmentShadingRateKHR poison()
{
    return poisonVulkanStructure<VkPhysicalDeviceFragmentShadingRateKHR>();
}

template <>
VkDisplayModeProperties2KHR poison()
{
    return poisonVulkanStructure<VkDisplayModeProperties2KHR>();
}

template <>
VkDisplayPlaneProperties2KHR poison()
{
    return poisonVulkanStructure<VkDisplayPlaneProperties2KHR>();
}

template <>
VkSurfaceFormat2KHR poison()
{
    return poisonVulkanStructure<VkSurfaceFormat2KHR>();
}

template <typename T, typename R>
size_t getOversizedArraySize(size_t arraySize)
{
    return arraySize * 64;
}

template <>
size_t getOversizedArraySize<VkPipelineCache, uint8_t>(size_t arraySize)
{
    return arraySize + 512;
}

template <typename R>
void validateArray(tcu::TestLog &log, size_t implementionCount, size_t writtenCount, std::vector<R> &array)
{
    DE_UNREF(implementionCount);

    const R poisonValue = poison<R>();

    for (size_t i = 0; i < writtenCount; i++)
    {
        if (deMemCmp(&array[i], &poisonValue, sizeof(R)) == 0)
        {
            TCU_FAIL("Implementation did not write entry " + std::to_string(i) + " in the array");
            log << tcu::TestLog::Message << "array[" << i << "] = " << array[i] << tcu::TestLog::EndMessage;
        }
    }
}

template <>
void validateArray(tcu::TestLog &log, size_t implementionCount, size_t writtenCount, std::vector<uint8_t> &array)
{
    const uint8_t poisonValue = poison<uint8_t>();

    if (writtenCount <= implementionCount)
        return;

    // Since the poison pattern is so small, it is easy for the poison value to naturarlly occur in valid data
    // so only validate between implementationCount and writtenCount
    for (size_t i = implementionCount; i < writtenCount; i++)
    {
        if (deMemCmp(&array[i], &poisonValue, sizeof(uint8_t)) == 0)
        {
            TCU_FAIL("Implementation did not write entry " + std::to_string(i) + " in the array");
            log << tcu::TestLog::Message << "array[" << i << "] = " << array[i] << tcu::TestLog::EndMessage;
        }
    }
}

template <typename R>
void destroyArray(Context &context, size_t writtenCount, std::vector<R> &array)
{
    DE_UNREF(context);
    DE_UNREF(writtenCount);
    DE_UNREF(array);
}

template <>
void destroyArray(Context &context, size_t writtenCount, std::vector<VkPipelineBinaryKHR> &array)
{
    const VkDevice vkDevice   = context.getDevice();
    const DeviceInterface &vk = context.getDeviceInterface();

    for (uint32_t i = 0; i < writtenCount; i++)
    {
        vk.destroyPipelineBinaryKHR(vkDevice, array[i], nullptr);
    }
}

template <typename T, typename R>
void oversizedArrayTestImpl(Context &context, T handle)
{
    VkResult result;
    tcu::TestLog &log = context.getTestContext().getLog();

    // Query 1 to get the base count
    size_t implementationCount = 0;
    result                     = arrayFunc<T, R>(context, handle, implementationCount, nullptr);
    VK_CHECK(result);

    log << tcu::TestLog::Message << "Queried count = " << implementationCount << tcu::TestLog::EndMessage;

    if (implementationCount == 0)
        TCU_THROW(NotSupportedError, "Function returned 0 count");

    // Increase the array size signifcantly incase the number queried increases between the first and second query
    size_t arraySize = getOversizedArraySize<T, R>(implementationCount);

    log << tcu::TestLog::Message << "Test array size = " << arraySize << tcu::TestLog::EndMessage;

    // Poison the array, so that we know which entries the implementation has written
    const R poisonValue = poison<R>();
    std::vector<R> array(arraySize, poisonValue);

    // Query 2 which the implementation should only be writting a subset of the array
    size_t writtenCount = arraySize;
    result              = arrayFunc<T, R>(context, handle, writtenCount, array.data());
    VK_CHECK(result);

    log << tcu::TestLog::Message << "Written count = " << writtenCount << tcu::TestLog::EndMessage;

    // The written count must not be greater than the size of the array we passed in
    if (writtenCount > arraySize)
        TCU_FAIL("More entries written (" + std::to_string(writtenCount) + ") than the provided array size (" +
                 std::to_string(arraySize) + ")");

    // Check the implementation has actually written all the entries it has claimed to have
    validateArray(log, implementationCount, writtenCount, array);

    /*
     * writtenCount should equal implementationCount, but that isn't guarenteed as the actual count may have changed
     * between calls, so this is only a warning, not an error.
     */
    if (writtenCount > implementationCount)
        TCU_THROW(QualityWarning, "The written array count may not have been updated");

    destroyArray(context, writtenCount, array);
}

tcu::TestStatus oversizedArrayTest(Context &context, ArrayFunction func)
{
    const VkInstance instance              = context.getInstance();
    const VkPhysicalDevice physical_device = context.getPhysicalDevice();
    const VkDevice device                  = context.getDevice();

    const DeviceInterface &vk = context.getDeviceInterface();

    switch (func)
    {
    case ArrayFunction::EnumeratePhysicalDevices:
    {
        oversizedArrayTestImpl<VkInstance, VkPhysicalDevice>(context, instance);
        break;
    }

    case ArrayFunction::EnumerateInstanceLayerProperties:
    {
        oversizedArrayTestImpl<VkInstance, VkLayerProperties>(context, instance);
        break;
    }

    case ArrayFunction::EnumerateInstanceExtensionProperties:
    {
        oversizedArrayTestImpl<VkInstance, VkExtensionProperties>(context, instance);
        break;
    }

    case ArrayFunction::EnumerateDeviceExtensionProperties:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkExtensionProperties>(context, physical_device);
        break;
    }

    case ArrayFunction::GetPipelineCacheData:
    {
        const VkPipelineCacheCreateInfo pipelineCacheCreateInfo = initVulkanStructure();
        const Unique<VkPipelineCache> pipelineCache(createPipelineCache(vk, device, &pipelineCacheCreateInfo));

        oversizedArrayTestImpl<VkPipelineCache, uint8_t>(context, *pipelineCache);
        break;
    }

    case ArrayFunction::CreatePipelineBinaries:
    {
        const Unique<VkPipeline> pipeline(createGraphicsPipeline(context));
        oversizedArrayTestImpl<VkPipeline, VkPipelineBinaryKHR>(context, *pipeline);
        break;
    }

    case ArrayFunction::EnumeratePhysicalDeviceGroups:
    {
        oversizedArrayTestImpl<VkInstance, VkPhysicalDeviceGroupProperties>(context, instance);
        break;
    }

    case ArrayFunction::GetPhysicalDeviceCalibrateableTimeDomains:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkTimeDomainKHR>(context, physical_device);
        break;
    }

    case ArrayFunction::GetPhysicalDeviceCooperativeMatrixProperties:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkCooperativeMatrixPropertiesKHR>(context, physical_device);
        break;
    }

    case ArrayFunction::GetPipelineExecutableProperties:
    {
        const Unique<VkPipeline> pipeline(createGraphicsPipeline(context));
        oversizedArrayTestImpl<VkPipeline, VkPipelineExecutablePropertiesKHR>(context, *pipeline);
        break;
    }

    case ArrayFunction::GetPipelineExecutableStatistics:
    {
        const Unique<VkPipeline> pipeline(createGraphicsPipeline(context));

        // Check that we have at least 1 executable to use, arrayFunc<VkPipeline, VkPipelineExecutableStatisticKHR>
        // will always query for executable 0
        VkPipelineInfoKHR pipelineInfo = initVulkanStructure();
        pipelineInfo.pipeline          = *pipeline;

        uint32_t pipelineExecutableCount;
        vk.getPipelineExecutablePropertiesKHR(device, &pipelineInfo, (uint32_t *)&pipelineExecutableCount, nullptr);

        if (pipelineExecutableCount == 0)
            TCU_THROW(NotSupportedError, "No pipeline executables for test pipeline");

        oversizedArrayTestImpl<VkPipeline, VkPipelineExecutableStatisticKHR>(context, *pipeline);
        break;
    }

    case ArrayFunction::GetPipelineExecutableInternalRepresentations:
    {
        const Unique<VkPipeline> pipeline(createGraphicsPipeline(context));

        // Check that we have at least 1 executable to use, arrayFunc<VkPipeline, VkPipelineExecutableStatisticKHR>
        // will always query for executable 0
        VkPipelineInfoKHR pipelineInfo = initVulkanStructure();
        pipelineInfo.pipeline          = *pipeline;

        uint32_t pipelineExecutableCount;
        vk.getPipelineExecutablePropertiesKHR(device, &pipelineInfo, (uint32_t *)&pipelineExecutableCount, nullptr);

        if (pipelineExecutableCount == 0)
            TCU_THROW(NotSupportedError, "No pipeline executables for test pipeline");

        oversizedArrayTestImpl<VkPipeline, VkPipelineExecutableInternalRepresentationKHR>(context, *pipeline);
        break;
    }

    case ArrayFunction::GetPhysicalDeviceToolProperties:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkPhysicalDeviceToolProperties>(context, physical_device);
        break;
    }

    case ArrayFunction::GetPhysicalDeviceFragmentShadingRates:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkPhysicalDeviceFragmentShadingRateKHR>(context, physical_device);
        break;
    }

    case ArrayFunction::Count:
        TCU_FAIL("Unimplemented enumeration function");
    }

    return tcu::TestStatus::pass("Pass");
}

typedef std::vector<VkExtensionProperties> Extensions;

de::MovePtr<wsi::Display> createDisplay(const vk::Platform &platform, const Extensions &supportedExtensions,
                                        wsi::Type wsiType)
{
    try
    {
        return de::MovePtr<wsi::Display>(platform.createWsiDisplay(wsiType));
    }
    catch (const tcu::NotSupportedError &e)
    {
        if (isExtensionStructSupported(supportedExtensions, RequiredExtension(wsi::getExtensionName(wsiType))) &&
            platform.hasDisplay(wsiType))
        {
            // If VK_KHR_{platform}_surface was supported, vk::Platform implementation
            // must support creating native display & window for that WSI type.
            throw tcu::TestError(e.getMessage());
        }
        else
            throw;
    }
}

de::MovePtr<wsi::Window> createWindow(const wsi::Display &display, const tcu::Maybe<tcu::UVec2> &initialSize)
{
    try
    {
        return de::MovePtr<wsi::Window>(display.createWindow(initialSize));
    }
    catch (const tcu::NotSupportedError &e)
    {
        // See createDisplay - assuming that wsi::Display was supported platform port
        // should also support creating a window.
        throw tcu::TestError(e.getMessage());
    }
}

struct NativeObjects
{
    const de::UniquePtr<wsi::Display> display;
    const de::UniquePtr<wsi::Window> window;

    NativeObjects(Context &context, const Extensions &supportedExtensions, wsi::Type wsiType,
                  const tcu::Maybe<tcu::UVec2> &initialWindowSize = tcu::Nothing)
        : display(
              createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
        , window(createWindow(*display, initialWindowSize))
    {
    }
};

VkDisplayKHR getDisplay(wsi::Type wsiType, const NativeObjects &native)
{
    switch (wsiType)
    {
    case wsi::Type::TYPE_DIRECT:
    {
        const wsi::DirectDisplayInterface *interface =
            reinterpret_cast<const wsi::DirectDisplayInterface *>(native.display.get());
        return interface->getNative();
    }

    case wsi::Type::TYPE_DIRECT_DRM:
    {
        const wsi::DirectDrmDisplayInterface *interface =
            reinterpret_cast<const wsi::DirectDrmDisplayInterface *>(native.display.get());
        return interface->getNative();
    }

    default:
        TCU_FAIL("Invalid display type for function");
    }
}

Move<VkSwapchainKHR> createSwapchain(vk::wsi::Type wsiType, Context &context, VkSurfaceKHR surface)
{
    const InstanceInterface &vki            = context.getInstanceInterface();
    const DeviceInterface &vk               = context.getDeviceInterface();
    const VkDevice vkDevice                 = context.getDevice();
    const VkPhysicalDevice vkPhysicalDevice = context.getPhysicalDevice();

    const VkSurfaceCapabilitiesKHR capabilities =
        wsi::getPhysicalDeviceSurfaceCapabilities(vki, vkPhysicalDevice, surface);

    if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        TCU_THROW(NotSupportedError, "supportedUsageFlags does not contain VK_IMAGE_USAGE_TRANSFER_DST_BIT");

    const std::vector<VkSurfaceFormatKHR> surfaceFormats =
        wsi::getPhysicalDeviceSurfaceFormats(vki, vkPhysicalDevice, surface);

    const VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];

    const VkExtent2D swapchainExtent = {
        de::clamp(16u, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        de::clamp(16u, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};

    static const std::array<VkCompositeAlphaFlagBitsKHR, 4> compositeAlphaValues({
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    });

    VkCompositeAlphaFlagBitsKHR compositeAlpha{};

    for (VkCompositeAlphaFlagBitsKHR compositAlphaBit : compositeAlphaValues)
    {
        if (capabilities.supportedCompositeAlpha & compositAlphaBit)
        {
            compositeAlpha = compositAlphaBit;
            break;
        }
    }

    const VkSwapchainCreateInfoKHR swapchainParams = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, // VkStructureType               sType
        nullptr,                                     // const void*                   pNext
        0u,                                          // VkSwapchainCreateFlagsKHR     flags
        surface,                                     // VkSurfaceKHR                  surface
        std::max(1u, capabilities.minImageCount),    // uint32_t                      minImageCount
        surfaceFormat.format,                        // VkFormat                      imageFormat
        surfaceFormat.colorSpace,                    // VkColorSpaceKHR               imageColorSpace
        swapchainExtent,                             // VkExtent2D                    imageExtent
        1,                                           // uint32_t                      imageArrayLayers
        VK_IMAGE_USAGE_TRANSFER_DST_BIT,             // VkImageUsageFlags             imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode                 imageSharingMode
        0u,                                          // uint32_t                      queueFamilyIndexCount
        nullptr,                                     // const uint32_t*               pQueueFamilyIndices
        capabilities.currentTransform,               // VkSurfaceTransformFlagBitsKHR preTransform
        compositeAlpha,                              // VkCompositeAlphaFlagBitsKHR   compositeAlpha
        VK_PRESENT_MODE_FIFO_KHR,                    // VkPresentModeKHR              presentMode
        VK_FALSE,                                    // VkBool32                      clipped
        VK_NULL_HANDLE                               // VkSwapchainKHR                oldSwapchain
    };

    return createWsiSwapchain(wsiType, vk, vkDevice, &swapchainParams);
}

tcu::TestStatus oversizedArrayWsiTest(Context &context, WsiTestParams params)
{
    const InstanceInterface &vki            = context.getInstanceInterface();
    const VkInstance vkInstance             = context.getInstance();
    const VkPhysicalDevice vkPhysicalDevice = context.getPhysicalDevice();

    const NativeObjects native(context, enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr),
                               params.wsiType);
    const Unique<VkSurfaceKHR> surface(wsi::createSurface(vki, vkInstance, params.wsiType, *native.display,
                                                          *native.window, context.getTestContext().getCommandLine()));
    const Unique<VkSwapchainKHR> swapchain(createSwapchain(params.wsiType, context, *surface));

    switch (params.func)
    {
    case WsiArrayFunction::GetPhysicalDeviceDisplayProperties:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkDisplayPropertiesKHR>(context, vkPhysicalDevice);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkDisplayPlanePropertiesKHR>(context, vkPhysicalDevice);
        break;
    }
    case WsiArrayFunction::GetDisplayPlaneSupportedDisplays:
    {
        // Check that we have at least 1 display plane to use, arrayFunc<VkPhysicalDevice, VkDisplayKHR>
        // will always query for plane 0
        uint32_t displayPaneCount = 0;
        vki.getPhysicalDeviceDisplayPlanePropertiesKHR(vkPhysicalDevice, (uint32_t *)&displayPaneCount, nullptr);

        if (displayPaneCount == 0)
            TCU_THROW(NotSupportedError, "No display panes supported");

        oversizedArrayTestImpl<VkPhysicalDevice, VkDisplayKHR>(context, vkPhysicalDevice);
        break;
    }
    case WsiArrayFunction::GetDisplayModeProperties:
    {
        VkDisplayKHR display = getDisplay(params.wsiType, native);
        oversizedArrayTestImpl<VkDisplayKHR, VkDisplayModePropertiesKHR>(context, display);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceSurfaceFormats:
    {
        oversizedArrayTestImpl<VkSurfaceKHR, VkSurfaceFormatKHR>(context, *surface);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceSurfacePresentModes:
    {
        oversizedArrayTestImpl<VkSurfaceKHR, VkPresentModeKHR>(context, *surface);
        break;
    }
    case WsiArrayFunction::GetPhysicalDevicePresentRectangles:
    {
        oversizedArrayTestImpl<VkSurfaceKHR, VkRect2D>(context, *surface);
        break;
    }
    case WsiArrayFunction::GetSwapchainImages:
    {
        oversizedArrayTestImpl<VkSwapchainKHR, VkImage>(context, *swapchain);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceSurfaceFormats2:
    {
        VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = initVulkanStructure();
        surfaceInfo.surface                         = *surface;

        oversizedArrayTestImpl<VkPhysicalDeviceSurfaceInfo2KHR, VkSurfaceFormat2KHR>(context, surfaceInfo);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceDisplayProperties2:
    {
        oversizedArrayTestImpl<VkSwapchainKHR, VkImage>(context, *swapchain);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties2:
    {
        oversizedArrayTestImpl<VkPhysicalDevice, VkDisplayPlaneProperties2KHR>(context, vkPhysicalDevice);
        break;
    }
    case WsiArrayFunction::GetDisplayModeProperties2:
    {
        VkDisplayKHR display = getDisplay(params.wsiType, native);
        oversizedArrayTestImpl<VkDisplayKHR, VkDisplayModeProperties2KHR>(context, display);
        break;
    }
    case WsiArrayFunction::GetPhysicalDeviceSurfacePresentModes2:
    {
        VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = initVulkanStructure();
        surfaceInfo.surface                         = *surface;

        oversizedArrayTestImpl<VkPhysicalDeviceSurfaceInfo2KHR, VkPresentModeKHR>(context, surfaceInfo);
        break;
    }

    case WsiArrayFunction::Count:
        TCU_FAIL("Unimplemented enumeration function");
    }

    return tcu::TestStatus::pass("Pass");
}

std::string GetTestName(ArrayFunction func)
{
    switch (func)
    {
    case ArrayFunction::EnumeratePhysicalDevices:
        return "enumerate_physical_devices";
    case ArrayFunction::EnumerateInstanceLayerProperties:
        return "enumerate_instance_layer_properties";
    case ArrayFunction::EnumerateInstanceExtensionProperties:
        return "enumerate_instance_extension_properties";
    case ArrayFunction::EnumerateDeviceExtensionProperties:
        return "enumerate_device_extension_properties";
    case ArrayFunction::GetPipelineCacheData:
        return "get_pipeline_cache_data";
    case ArrayFunction::CreatePipelineBinaries:
        return "create_pipeline_binaries";
    case ArrayFunction::EnumeratePhysicalDeviceGroups:
        return "enumerate_physical_device_groups";
    case ArrayFunction::GetPhysicalDeviceCalibrateableTimeDomains:
        return "get_physical_device_calibrateable_time_domains";
    case ArrayFunction::GetPhysicalDeviceCooperativeMatrixProperties:
        return "get_physical_device_cooperative_matrix_properties";
    case ArrayFunction::GetPipelineExecutableProperties:
        return "get_pipeline_executable_properties";
    case ArrayFunction::GetPipelineExecutableStatistics:
        return "get_pipeline_executable_statistics";
    case ArrayFunction::GetPipelineExecutableInternalRepresentations:
        return "get_pipeline_executable_internal_representations";
    case ArrayFunction::GetPhysicalDeviceToolProperties:
        return "get_physical_device_tool_properties";
    case ArrayFunction::GetPhysicalDeviceFragmentShadingRates:
        return "get_physical_device_fragment_shading_rates";

    case ArrayFunction::Count:
        return "invalid_function";
    }

    return "invalid_function";
}

std::string GetTestName(WsiArrayFunction func)
{
    switch (func)
    {
    case WsiArrayFunction::GetPhysicalDeviceDisplayProperties:
        return "get_physical_device_display_properties";
    case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties:
        return "get_physical_device_display_plane_properties";
    case WsiArrayFunction::GetDisplayPlaneSupportedDisplays:
        return "get_display_plane_supported_displays";
    case WsiArrayFunction::GetDisplayModeProperties:
        return "get_display_mode_properties";
    case WsiArrayFunction::GetPhysicalDeviceSurfaceFormats:
        return "get_physical_device_surface_formats";
    case WsiArrayFunction::GetPhysicalDeviceSurfacePresentModes:
        return "get_physical_device_surface_present_modes";
    case WsiArrayFunction::GetSwapchainImages:
        return "get_swapchain_images";
    case WsiArrayFunction::GetPhysicalDevicePresentRectangles:
        return "get_physical_device_present_rectangles";
    case WsiArrayFunction::GetPhysicalDeviceSurfaceFormats2:
        return "get_physical_device_surface_formats2";
    case WsiArrayFunction::GetPhysicalDeviceDisplayProperties2:
        return "get_physical_device_display_properties2";
    case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties2:
        return "get_physical_device_display_plane_properties2";
    case WsiArrayFunction::GetDisplayModeProperties2:
        return "get_display_mode_properties2";
    case WsiArrayFunction::GetPhysicalDeviceSurfacePresentModes2:
        return "get_physical_device_surface_present_modes2";

    case WsiArrayFunction::Count:
        return "invalid_function";
    }

    return "invalid_function";
}

void checkSupport(Context &context, ArrayFunction func)
{
    switch (func)
    {
    case ArrayFunction::CreatePipelineBinaries:
        context.requireDeviceFunctionality("VK_KHR_pipeline_binary");
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
        break;

    case ArrayFunction::EnumeratePhysicalDeviceGroups:
        context.requireDeviceFunctionality("VK_KHR_device_group");
        break;

    case ArrayFunction::GetPhysicalDeviceCalibrateableTimeDomains:
        if (!context.isDeviceFunctionalitySupported("VK_KHR_calibrated_timestamps") &&
            !context.isDeviceFunctionalitySupported("VK_EXT_calibrated_timestamps"))
            TCU_THROW(NotSupportedError,
                      "VK_KHR_calibrated_timestamps and VK_EXT_calibrated_timestamps are not supported");
        break;

    case ArrayFunction::GetPhysicalDeviceCooperativeMatrixProperties:
        context.requireDeviceFunctionality("VK_KHR_cooperative_matrix");
        break;

    case ArrayFunction::GetPipelineExecutableProperties:
    case ArrayFunction::GetPipelineExecutableStatistics:
    case ArrayFunction::GetPipelineExecutableInternalRepresentations:
        context.requireDeviceFunctionality("VK_KHR_pipeline_executable_properties");
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
        break;

    case ArrayFunction::GetPhysicalDeviceToolProperties:
        context.requireDeviceFunctionality("VK_EXT_tooling_info");
        break;

    case ArrayFunction::GetPhysicalDeviceFragmentShadingRates:
        context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");
        break;

    default:
        break;
    }
}

void checkSupportWsi(Context &context, WsiTestParams params)
{
    context.requireInstanceFunctionality("VK_KHR_surface");
    context.requireInstanceFunctionality(vk::wsi::getExtensionName(params.wsiType));
    context.requireDeviceFunctionality("VK_KHR_swapchain");

    switch (params.func)
    {
    case WsiArrayFunction::GetPhysicalDevicePresentRectangles:
        context.requireDeviceFunctionality("VK_KHR_device_group");
        break;
    case WsiArrayFunction::GetPhysicalDeviceSurfaceFormats2:
        context.requireInstanceFunctionality("VK_KHR_get_surface_capabilities2");
        break;
    case WsiArrayFunction::GetPhysicalDeviceDisplayProperties2:
    case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties2:
    case WsiArrayFunction::GetDisplayModeProperties2:
        context.requireInstanceFunctionality("VK_KHR_get_display_properties2");
        break;
    case WsiArrayFunction::GetPhysicalDeviceSurfacePresentModes2:
        context.requireDeviceFunctionality("VK_EXT_full_screen_exclusive");
        break;
    default:
        break;
    }
}

void initPrograms(SourceCollections &dst, ArrayFunction func)
{
    switch (func)
    {
    case ArrayFunction::CreatePipelineBinaries:
    case ArrayFunction::GetPipelineExecutableProperties:
    case ArrayFunction::GetPipelineExecutableStatistics:
    case ArrayFunction::GetPipelineExecutableInternalRepresentations:
        dst.glslSources.add("vert") << glu::VertexSource("#version 310 es\n"
                                                         "layout(location = 0) in highp vec4 a_position;\n"
                                                         "void main (void) { gl_Position = a_position; }\n");
        dst.glslSources.add("frag") << glu::FragmentSource(
            "#version 310 es\n"
            "layout(location = 0) out lowp vec4 o_color;\n"
            "void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
        break;

    default:
        break;
    }
}

void createOversizedArrayCases(tcu::TestCaseGroup *group)
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(ArrayFunction::Count); i++)
    {
        ArrayFunction func = static_cast<ArrayFunction>(i);
        addFunctionCaseWithPrograms(group, GetTestName(func), checkSupport, initPrograms, oversizedArrayTest, func);
    }
}

void createOversizedArrayWsiCases(tcu::TestCaseGroup *group, wsi::Type wsiType)
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(WsiArrayFunction::Count); i++)
    {
        WsiArrayFunction func = static_cast<WsiArrayFunction>(i);

        /* Don't generate tests for functions that are not supported by a specific WSI type */
        switch (func)
        {
        case WsiArrayFunction::GetPhysicalDeviceDisplayProperties:
        case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties:
        case WsiArrayFunction::GetPhysicalDeviceDisplayProperties2:
        case WsiArrayFunction::GetPhysicalDeviceDisplayPlaneProperties2:
        case WsiArrayFunction::GetDisplayPlaneSupportedDisplays:
        case WsiArrayFunction::GetDisplayModeProperties:
        case WsiArrayFunction::GetDisplayModeProperties2:
            if (!(wsiType == wsi::Type::TYPE_DIRECT || wsiType == wsi::Type::TYPE_DIRECT_DRM))
                continue;
            break;

        default:
            break;
        }

        WsiTestParams testParams = {
            func,
            wsiType,
        };
        addFunctionCase(group, GetTestName(func), checkSupportWsi, oversizedArrayWsiTest, testParams);
    }
}

void createOversizedArrayWsiGroups(tcu::TestCaseGroup *group)
{
    for (uint32_t i = 0; i < wsi::TYPE_LAST; i++)
    {
        wsi::Type wsiType = static_cast<wsi::Type>(i);
        addTestGroup(group, wsi::getName(wsiType), createOversizedArrayWsiCases, wsiType);
    }
}

void createOversizedArrayGroups(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "core", createOversizedArrayCases);
    addTestGroup(group, "wsi", createOversizedArrayWsiGroups);
}

void createTestCases(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "oversized_array", createOversizedArrayGroups);
}

} // namespace

tcu::TestCaseGroup *createArrayTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "array", createTestCases);
}

} // namespace api
} // namespace vkt
