/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief VK_EXT_surface_maintenance1 and VK_EXT_swapchain_maintenance1 extension tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiMaintenance1Tests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"

#include "deRandom.hpp"

#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include <limits>
#include <random>
#include <set>

#if (DE_OS == DE_OS_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace vkt
{
namespace wsi
{

namespace
{

using namespace vk;
using namespace vk::wsi;

typedef std::vector<VkExtensionProperties> Extensions;

constexpr uint64_t kMaxFenceWaitTimeout = 2000000000ul;

template <typename T>
void checkAllSupported(const Extensions &supportedExtensions, const std::vector<T> &requiredExtensions)
{
    for (auto &requiredExtension : requiredExtensions)
    {
        if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(requiredExtension)))
            TCU_THROW(NotSupportedError, (std::string(requiredExtension) + " is not supported").c_str());
    }
}

CustomInstance createInstanceWithWsi(Context &context, const Extensions &supportedExtensions, Type wsiType,
                                     bool requireDeviceGroup, const VkAllocationCallbacks *pAllocator = nullptr)
{
    const uint32_t version = context.getUsedApiVersion();
    std::vector<std::string> extensions;

    extensions.push_back("VK_KHR_surface");
    extensions.push_back(getExtensionName(wsiType));
    if (isDisplaySurface(wsiType))
        extensions.push_back("VK_KHR_display");

    if (!vk::isCoreInstanceExtension(version, "VK_KHR_get_physical_device_properties2"))
        extensions.push_back("VK_KHR_get_physical_device_properties2");

    if (isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_KHR_get_surface_capabilities2")))
        extensions.push_back("VK_KHR_get_surface_capabilities2");

    extensions.push_back("VK_EXT_surface_maintenance1");

    if (requireDeviceGroup)
        extensions.push_back("VK_KHR_device_group_creation");

    checkAllSupported(supportedExtensions, extensions);

    return createCustomInstanceWithExtensions(context, extensions, pAllocator);
}

VkPhysicalDeviceFeatures getDeviceFeaturesForWsi(void)
{
    VkPhysicalDeviceFeatures features;
    deMemset(&features, 0, sizeof(features));
    return features;
}

Move<VkDevice> createDeviceWithWsi(const vk::PlatformInterface &vkp, VkInstance instance, const InstanceInterface &vki,
                                   VkPhysicalDevice physicalDevice, const Extensions &supportedExtensions,
                                   const uint32_t queueFamilyIndex, const VkAllocationCallbacks *pAllocator,
                                   bool requireSwapchainMaintenance1, bool enableSwapchainMaintenance1Feature,
                                   bool requireDeviceGroup, bool validationEnabled)
{
    const float queuePriorities[]              = {1.0f};
    const VkDeviceQueueCreateInfo queueInfos[] = {{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        (VkDeviceQueueCreateFlags)0,
        queueFamilyIndex,
        DE_LENGTH_OF_ARRAY(queuePriorities),
        &queuePriorities[0],
    }};
    const VkPhysicalDeviceFeatures features    = getDeviceFeaturesForWsi();
    std::vector<const char *> extensions;

    extensions.push_back("VK_KHR_swapchain");
    if (requireSwapchainMaintenance1)
    {
        extensions.push_back("VK_EXT_swapchain_maintenance1");
    }
    if (requireDeviceGroup)
    {
        extensions.push_back("VK_KHR_device_group");
    }
    if (isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_KHR_shared_presentable_image")))
    {
        extensions.push_back("VK_KHR_shared_presentable_image");
    }

    checkAllSupported(supportedExtensions, extensions);

    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenance1Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT, // VkStructureType sType;
        nullptr,                                                                // void* pNext;
        VK_TRUE,                                                                // VkBool32 swapchainMaintenance1;
    };

    VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();
    features2.features                  = features;

    if (enableSwapchainMaintenance1Feature)
        features2.pNext = &swapchainMaintenance1Features;

    VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        &features2,
        (VkDeviceCreateFlags)0,
        DE_LENGTH_OF_ARRAY(queueInfos),
        &queueInfos[0],
        0u,      // enabledLayerCount
        nullptr, // ppEnabledLayerNames
        (uint32_t)extensions.size(),
        extensions.empty() ? nullptr : &extensions[0],
        nullptr,
    };

    return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

struct InstanceHelper
{
    const std::vector<VkExtensionProperties> supportedExtensions;
    const CustomInstance instance;
    const InstanceDriver &vki;

    InstanceHelper(Context &context, Type wsiType, bool requireDeviceGroup,
                   const VkAllocationCallbacks *pAllocator = nullptr)
        : supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr))
        , instance(createInstanceWithWsi(context, supportedExtensions, wsiType, requireDeviceGroup, pAllocator))
        , vki(instance.getDriver())
    {
    }
};

struct DeviceHelper
{
    const VkPhysicalDevice physicalDevice;
    const uint32_t queueFamilyIndex;
    const Unique<VkDevice> device;
    const DeviceDriver vkd;
    const VkQueue queue;

    DeviceHelper(Context &context, const InstanceInterface &vki, VkInstance instance, VkSurfaceKHR surface,
                 bool requireSwapchainMaintenance1, bool enableSwapchainMaintenance1Feature, bool requireDeviceGroup,
                 const VkAllocationCallbacks *pAllocator = nullptr)
        : physicalDevice(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
        , queueFamilyIndex(chooseQueueFamilyIndex(vki, physicalDevice, surface))
        , device(createDeviceWithWsi(context.getPlatformInterface(), instance, vki, physicalDevice,
                                     enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr), queueFamilyIndex,
                                     pAllocator, requireSwapchainMaintenance1, enableSwapchainMaintenance1Feature,
                                     requireDeviceGroup,
                                     context.getTestContext().getCommandLine().isValidationEnabled()))
        , vkd(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion(),
              context.getTestContext().getCommandLine())
        , queue(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
    {
    }
};

de::MovePtr<Display> createDisplay(const vk::Platform &platform, const Extensions &supportedExtensions, Type wsiType)
{
    try
    {
        return de::MovePtr<Display>(platform.createWsiDisplay(wsiType));
    }
    catch (const tcu::NotSupportedError &e)
    {
        if (isExtensionStructSupported(supportedExtensions, RequiredExtension(getExtensionName(wsiType))) &&
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

de::MovePtr<Window> createWindow(const Display &display, const tcu::Maybe<tcu::UVec2> &initialSize)
{
    try
    {
        return de::MovePtr<Window>(display.createWindow(initialSize));
    }
    catch (const tcu::NotSupportedError &e)
    {
        // See createDisplay - assuming that wsi::Display was supported platform port
        // should also support creating a window.
        throw tcu::TestError(e.getMessage());
    }
}

constexpr uint32_t kDefaultWindowWidth  = 128;
constexpr uint32_t kDefaultWindowHeight = 256;

struct TestNativeObjects
{
    const de::UniquePtr<Display> display;
    tcu::UVec2 windowSize;
    std::vector<de::MovePtr<Window>> windows;

    TestNativeObjects(Context &context, const Extensions &supportedExtensions, Type wsiType, uint32_t windowCount)
        : display(
              createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
        , windowSize(tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight))
    {
        for (uint32_t i = 0; i < windowCount; ++i)
        {
            windows.push_back(createWindow(*display, windowSize));
            windows.back()->setVisible(true);
            if (wsiType == TYPE_WIN32)
            {
                windows.back()->setForeground();
            }
        }
    }

    void resizeWindow(uint32_t windowIndex, const tcu::UVec2 newWindowSize)
    {
        windows[windowIndex]->resize(newWindowSize);
        windowSize = newWindowSize;
    }
};

VkSwapchainCreateInfoKHR getBasicSwapchainParameters(VkSurfaceKHR surface, VkSurfaceFormatKHR surfaceFormat,
                                                     const tcu::UVec2 &desiredSize, VkPresentModeKHR presentMode,
                                                     VkSurfaceTransformFlagBitsKHR transform,
                                                     uint32_t desiredImageCount, bool deferMemoryAllocation)
{
    const VkSwapchainCreateInfoKHR parameters = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        (VkSwapchainCreateFlagsKHR)(deferMemoryAllocation ? VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_EXT : 0),
        surface,
        desiredImageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        vk::makeExtent2D(desiredSize.x(), desiredSize.y()),
        1u, // imageArrayLayers
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        transform,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        presentMode,
        VK_FALSE,       // clipped
        VK_NULL_HANDLE, // oldSwapchain
    };

    return parameters;
}

VkSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities(const vk::InstanceInterface &vki,
                                                              VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                                              VkImageUsageFlags *sharedImageUsage)
{
    const VkPhysicalDeviceSurfaceInfo2KHR info = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        nullptr,
        surface,
    };
    VkSharedPresentSurfaceCapabilitiesKHR sharedCapabilities;
    VkSurfaceCapabilities2KHR capabilities;

    sharedCapabilities.sType = VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR;
    sharedCapabilities.pNext = nullptr;

    capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
    capabilities.pNext = sharedImageUsage ? &sharedCapabilities : nullptr;

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

    if (sharedImageUsage)
    {
        *sharedImageUsage = sharedCapabilities.sharedPresentSupportedUsageFlags;
    }

    return capabilities.surfaceCapabilities;
}

std::vector<VkPresentModeKHR> getSurfaceCompatiblePresentModes(const vk::InstanceInterface &vki,
                                                               VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                                               VkPresentModeKHR presentMode)
{
    VkSurfacePresentModeEXT presentModeInfo = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
        nullptr,
        presentMode,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR info = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        &presentModeInfo,
        surface,
    };

    // Currently there are 6 present modes, 100 should cover all future ones!
    std::vector<VkPresentModeKHR> compatibleModes(100);

    VkSurfacePresentModeCompatibilityEXT compatibility = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
        nullptr,
        (uint32_t)compatibleModes.size(),
        compatibleModes.data(),
    };
    VkSurfaceCapabilities2KHR capabilities = {
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        &compatibility,
        {},
    };

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

    compatibleModes.resize(compatibility.presentModeCount);
    return compatibleModes;
}

VkSurfacePresentScalingCapabilitiesEXT getSurfaceScalingCapabilities(const vk::InstanceInterface &vki,
                                                                     VkPhysicalDevice physicalDevice,
                                                                     VkPresentModeKHR presentMode, VkSurfaceKHR surface)
{
    VkSurfacePresentModeEXT presentModeInfo = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
        nullptr,
        presentMode,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR info = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        &presentModeInfo,
        surface,
    };

    VkSurfacePresentScalingCapabilitiesEXT scaling = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_EXT, nullptr, 0, 0, 0, {}, {},
    };
    VkSurfaceCapabilities2KHR capabilities = {
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        &scaling,
        {},
    };

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

    return scaling;
}

VkSurfaceCapabilitiesKHR getPerPresentSurfaceCapabilities(const vk::InstanceInterface &vki,
                                                          VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                                          VkPresentModeKHR presentMode)
{
    VkSurfacePresentModeEXT presentModeInfo = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
        nullptr,
        presentMode,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR info = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        &presentModeInfo,
        surface,
    };

    VkSurfaceCapabilities2KHR capabilities = {
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        nullptr,
        {},
    };

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

    return capabilities.surfaceCapabilities;
}

typedef de::SharedPtr<Unique<VkCommandBuffer>> CommandBufferSp;
typedef de::SharedPtr<Unique<VkFence>> FenceSp;
typedef de::SharedPtr<Unique<VkSemaphore>> SemaphoreSp;
typedef de::SharedPtr<Unique<VkImage>> ImageSp;

std::vector<FenceSp> createFences(const DeviceInterface &vkd, const VkDevice device, size_t numFences, bool nullHandles,
                                  de::Random &rng)
{
    std::vector<FenceSp> fences(numFences);

    for (size_t ndx = 0; ndx < numFences; ++ndx)
        if (!nullHandles || rng.getUint32() % 4 != 0)
            fences[ndx] = FenceSp(new Unique<VkFence>(createFence(vkd, device)));

    return fences;
}

std::vector<SemaphoreSp> createSemaphores(const DeviceInterface &vkd, const VkDevice device, size_t numSemaphores)
{
    std::vector<SemaphoreSp> semaphores(numSemaphores);

    for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
        semaphores[ndx] = SemaphoreSp(new Unique<VkSemaphore>(createSemaphore(vkd, device)));

    return semaphores;
}

std::vector<CommandBufferSp> allocateCommandBuffers(const DeviceInterface &vkd, const VkDevice device,
                                                    const VkCommandPool commandPool, const VkCommandBufferLevel level,
                                                    const size_t numCommandBuffers)
{
    std::vector<CommandBufferSp> buffers(numCommandBuffers);

    for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
        buffers[ndx] =
            CommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

    return buffers;
}

Move<VkBuffer> createBufferAndBindMemory(const DeviceHelper &devHelper, SimpleAllocator &allocator,
                                         const tcu::UVec4 color, uint32_t count, de::MovePtr<Allocation> *pAlloc)
{
    const DeviceInterface &vkd = devHelper.vkd;
    const VkDevice device      = *devHelper.device;
    const uint32_t queueIndex  = devHelper.queueFamilyIndex;

    const VkBufferCreateInfo bufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType      sType;
        nullptr,                              // const void*          pNext;
        0u,                                   // VkBufferCreateFlags  flags;
        count * 4,                            // VkDeviceSize         size;
        vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // VkBufferUsageFlags   usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode        sharingMode;
        1u,                                   // uint32_t             queueFamilyCount;
        &queueIndex                           // const uint32_t*      pQueueFamilyIndices;
    };

    Move<VkBuffer> buffer = createBuffer(vkd, device, &bufferParams);

    *pAlloc = allocator.allocate(getBufferMemoryRequirements(vkd, device, *buffer), MemoryRequirement::HostVisible);
    VK_CHECK(vkd.bindBufferMemory(device, *buffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

    // Upload color to buffer.  Assuming RGBA, but surface format could be different, such as BGRA.  For the purposes of the test, that doesn't matter.
    const uint32_t color32 = color.x() | color.y() << 8 | color.z() << 16 | color.w() << 24;
    std::vector<uint32_t> colors(count, color32);
    deMemcpy((*pAlloc)->getHostPtr(), colors.data(), colors.size() * sizeof(colors[0]));
    flushAlloc(vkd, device, **pAlloc);

    return buffer;
}

void copyBufferToImage(const DeviceInterface &vkd, VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
                       const tcu::UVec2 offset, const tcu::UVec2 extent)
{
    const VkBufferImageCopy region = {
        0,
        0,
        0,
        {
            vk::VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            0,
            1,
        },
        {(int32_t)offset.x(), (int32_t)offset.y(), 0},
        {
            extent.x(),
            extent.y(),
            1u,
        },
    };

    vkd.cmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

struct PresentFenceTestConfig
{
    vk::wsi::Type wsiType;
    std::vector<VkPresentModeKHR> modes;
    bool deferMemoryAllocation;
    bool bindImageMemory;
    bool changePresentModes;
    bool verifyFenceOrdering;
    bool nullHandles;
    bool swapchainMaintenance1;
};

bool canDoMultiSwapchainPresent(vk::wsi::Type wsiType)
{
    // Android has a bug with the implementation of multi-swapchain present.
    // This bug has existed since Vulkan 1.0 and is unrelated to
    // VK_EXT_swapchain_maintenance1.  Once that bug is fixed, multi-swapchain
    // present tests can be enabled for this platform.
    return wsiType != TYPE_ANDROID;
}

uint32_t getIterations(std::vector<VkPresentModeKHR> presentModes,
                       std::vector<std::vector<VkPresentModeKHR>> compatiblePresentModes,
                       bool testResizesWindowsFrequently)
{
    // Look at all the modes that will be used by the test.
    bool hasFifo    = false;
    bool hasShared  = false;
    bool hasNoVsync = false;

    std::set<VkPresentModeKHR> allModes;

    for (VkPresentModeKHR mode : presentModes)
        allModes.insert(mode);

    for (const auto &compatibleModes : compatiblePresentModes)
        for (VkPresentModeKHR mode : compatibleModes)
            allModes.insert(mode);

    for (VkPresentModeKHR mode : allModes)
    {
        switch (mode)
        {
        case VK_PRESENT_MODE_FIFO_KHR:
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            hasFifo = true;
            break;
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
            hasShared = true;
            break;
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
        case VK_PRESENT_MODE_MAILBOX_KHR:
        default:
            hasNoVsync = true;
            break;
        }
    }

    // Return an iteration count that is as high as possible while keeping the test time and memory usage reasonable.
    //
    // - If FIFO is used, limit to 120 (~2s on 60Hz)
    // - Else, limit to 250

    if (hasFifo)
        return testResizesWindowsFrequently ? 60 : 120;

    (void)hasShared;
    (void)hasNoVsync;
    uint32_t iterations = 250;

    // If the test resizes windows frequently, reduce the testing time as that's a very slow operation.
    if (testResizesWindowsFrequently)
        iterations /= 50;

    return iterations;
}

ImageSp bindSingleImageMemory(const DeviceInterface &vkd, const VkDevice device, const VkSwapchainKHR swapchain,
                              const VkSwapchainCreateInfoKHR swapchainCreateInfo, uint32_t imageIndex)
{
    VkImageSwapchainCreateInfoKHR imageSwapchainCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        swapchain,
    };

    VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        &imageSwapchainCreateInfo,
        (VkImageCreateFlags)0u,          // flags
        VK_IMAGE_TYPE_2D,                // imageType
        swapchainCreateInfo.imageFormat, // format
        {
            // extent
            swapchainCreateInfo.imageExtent.width,  //   width
            swapchainCreateInfo.imageExtent.height, //   height
            1u,                                     //   depth
        },
        1u,                             // mipLevels
        1u,                             // arrayLayers
        VK_SAMPLE_COUNT_1_BIT,          // samples
        VK_IMAGE_TILING_OPTIMAL,        // tiling
        swapchainCreateInfo.imageUsage, // usage
        VK_SHARING_MODE_EXCLUSIVE,      // sharingMode
        0u,                             // queueFamilyIndexCount
        nullptr,                        // pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,      // initialLayout
    };

    ImageSp image = ImageSp(new Unique<VkImage>(createImage(vkd, device, &imageCreateInfo)));

    VkBindImageMemorySwapchainInfoKHR bimSwapchainInfo = {
        VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,
        nullptr,
        swapchain,
        imageIndex,
    };

    VkBindImageMemoryInfo bimInfo = {
        VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, &bimSwapchainInfo, **image, VK_NULL_HANDLE, 0u,
    };

    VK_CHECK(vkd.bindImageMemory2(device, 1, &bimInfo));

    return image;
}

std::vector<ImageSp> bindImageMemory(const DeviceInterface &vkd, const VkDevice device, const VkSwapchainKHR swapchain,
                                     const VkSwapchainCreateInfoKHR swapchainCreateInfo)
{
    uint32_t numImages = 0;
    VK_CHECK(vkd.getSwapchainImagesKHR(device, swapchain, &numImages, nullptr));

    std::vector<ImageSp> images(numImages);

    for (uint32_t i = 0; i < numImages; ++i)
    {
        images[i] = bindSingleImageMemory(vkd, device, swapchain, swapchainCreateInfo, i);
    }

    return images;
}

void verifyFenceSignalOrdering(const DeviceInterface &vkd, const VkDevice device, const std::vector<FenceSp> &fences,
                               const uint32_t stride, const uint32_t offset, const uint32_t lastKnownSignaled,
                               const uint32_t maxIndex, tcu::ResultCollector *results)
{
    // Go over fences from end to last-known-signaled.  Verify that fences are
    // signaled in order by making sure that a consecutive set of fences are
    // encountered that are not signaled, followed by potentially a number of
    // fences that are.
    bool visitedSignaledFence = false;
    for (uint32_t i = maxIndex; i > lastKnownSignaled; --i)
    {
        const VkFence fence = **fences[(i - 1) * stride + offset];
        bool isSignaled     = vkd.getFenceStatus(device, fence) != VK_NOT_READY;

        // Ordering guarantee is broken if an unsignaled fence is encountered when a later fence is signaled.
        results->check(isSignaled || !visitedSignaledFence,
                       "Encountered unsignaled fence while a later fence is signaled");

        if (isSignaled)
        {
            visitedSignaledFence = true;
        }
    }
}

tcu::TestStatus presentFenceTest(Context &context, const PresentFenceTestConfig testParams)
{
    tcu::TestLog &log = context.getTestContext().getLog();
    tcu::ResultCollector results(log);

    const uint32_t surfaceCount = (uint32_t)testParams.modes.size();
    const InstanceHelper instHelper(context, testParams.wsiType, testParams.bindImageMemory);
    const TestNativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType, surfaceCount);
    std::vector<Move<VkSurfaceKHR>> surfaces;
    for (uint32_t i = 0; i < surfaceCount; ++i)
    {
        surfaces.push_back(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display,
                                         *native.windows[i], context.getTestContext().getCommandLine()));
    }

    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surfaces[0], true,
                                 testParams.swapchainMaintenance1, testParams.bindImageMemory);
    const DeviceInterface &vkd = devHelper.vkd;
    const VkDevice device      = *devHelper.device;

    for (uint32_t i = 0; i < surfaceCount; ++i)
    {
        const std::vector<VkPresentModeKHR> presentModes =
            getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surfaces[i]);
        if (std::find(presentModes.begin(), presentModes.end(), testParams.modes[i]) == presentModes.end())
            TCU_THROW(NotSupportedError, "Present mode not supported");
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats =
        getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surfaces[0]);
    if (surfaceFormats.empty())
        return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

    std::vector<bool> isSharedPresentMode(surfaceCount);

    for (uint32_t i = 0; i < surfaceCount; ++i)
    {
        isSharedPresentMode[i] = testParams.modes[i] == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
                                 testParams.modes[i] == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
    }

    std::vector<VkSwapchainCreateInfoKHR> swapchainInfo;
    std::vector<Move<VkSwapchainKHR>> swapchains;
    std::vector<VkSwapchainKHR> swapchainHandles;
    std::vector<std::vector<VkImage>> swapchainImages;
    std::vector<std::vector<ImageSp>> bimImages;
    std::vector<std::vector<VkPresentModeKHR>> compatiblePresentModes;
    for (uint32_t i = 0; i < surfaceCount; ++i)
    {
        VkImageUsageFlags sharedImageUsage = 0;
        const VkSurfaceCapabilitiesKHR capabilities =
            getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surfaces[i],
                                                 isSharedPresentMode[i] ? &sharedImageUsage : nullptr);
        const VkSurfaceTransformFlagBitsKHR transform =
            (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ?
                VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR :
                capabilities.currentTransform;

        if (isSharedPresentMode[i] && (sharedImageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
            TCU_THROW(NotSupportedError, "Transfer dst with shared present mode not supported");

        swapchainInfo.push_back(getBasicSwapchainParameters(
            *surfaces[i], surfaceFormats[0], native.windowSize, testParams.modes[i], transform,
            isSharedPresentMode[i] ? 1 : capabilities.minImageCount, testParams.deferMemoryAllocation));

        VkSwapchainPresentModesCreateInfoEXT compatibleModesCreateInfo = {
            VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,
            nullptr,
            0,
            nullptr,
        };
        if (testParams.changePresentModes)
        {
            compatiblePresentModes.push_back(getSurfaceCompatiblePresentModes(instHelper.vki, devHelper.physicalDevice,
                                                                              *surfaces[i], testParams.modes[i]));

            compatibleModesCreateInfo.presentModeCount = (uint32_t)compatiblePresentModes.back().size();
            compatibleModesCreateInfo.pPresentModes    = compatiblePresentModes.back().data();
            swapchainInfo.back().pNext                 = &compatibleModesCreateInfo;
        }

        swapchains.push_back(createSwapchainKHR(vkd, device, &swapchainInfo.back()));
        swapchainHandles.push_back(*swapchains.back());

        if (testParams.bindImageMemory)
        {
            uint32_t numImages = 0;
            VK_CHECK(vkd.getSwapchainImagesKHR(device, *swapchains.back(), &numImages, nullptr));
            swapchainImages.push_back(std::vector<VkImage>(numImages, VK_NULL_HANDLE));

            // If memory allocation is deferred, bind image memory lazily at acquire time.
            if (testParams.deferMemoryAllocation)
            {
                bimImages.push_back(std::vector<ImageSp>(numImages));
            }
            else
            {
                bimImages.push_back(bindImageMemory(vkd, device, *swapchains.back(), swapchainInfo.back()));
                for (size_t j = 0; j < bimImages.back().size(); ++j)
                {
                    swapchainImages.back()[j] = **bimImages.back()[j];
                }
            }
        }
        else
        {
            swapchainImages.push_back(getSwapchainImages(vkd, device, *swapchains.back()));
        }
    }

    const Unique<VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

    const uint32_t iterations = getIterations(testParams.modes, compatiblePresentModes, false);

    const uint32_t configHash =
        (uint32_t)testParams.wsiType | (uint32_t)testParams.modes[0] << 4 |
        (uint32_t)testParams.deferMemoryAllocation << 28 | (uint32_t)testParams.bindImageMemory << 29 |
        (uint32_t)testParams.changePresentModes << 30 | (uint32_t)testParams.verifyFenceOrdering << 31;
    de::Random rng(0x53A4C8A1u ^ configHash);

    // Do iterations presents, each with an associated fence.  Destroy the wait semaphores as soon as the corresponding fence signals.
    const std::vector<FenceSp> presentFences(
        createFences(vkd, device, iterations * surfaceCount, testParams.nullHandles, rng));
    const std::vector<SemaphoreSp> acquireSems(createSemaphores(vkd, device, iterations * surfaceCount));
    std::vector<SemaphoreSp> presentSems(createSemaphores(vkd, device, iterations));

    const std::vector<CommandBufferSp> commandBuffers(
        allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, iterations));

    const uint64_t foreverNs = 0xFFFFFFFFFFFFFFFFul;

    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
    };

    try
    {
        std::vector<uint32_t> nextUnfinishedPresent(surfaceCount, 0);

        for (uint32_t i = 0; i < iterations; ++i)
        {
            const VkSemaphore *presentSem = &**presentSems[i];
            std::vector<VkSemaphore> acquireSem;
            std::vector<VkFence> presentFence;
            std::vector<uint32_t> imageIndex(surfaceCount, 0x12345); // initialize to junk value
            // Acquire an image and clear it
            beginCommandBuffer(vkd, **commandBuffers[i], 0u);

            VkImageMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0,
                0,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                VK_NULL_HANDLE,
                range,
            };

            for (uint32_t j = 0; j < surfaceCount; ++j)
            {
                acquireSem.push_back(**acquireSems[i * surfaceCount + j]);
                if (presentFences[i * surfaceCount + j])
                    presentFence.push_back(**presentFences[i * surfaceCount + j]);
                else
                    presentFence.push_back(VK_NULL_HANDLE);

                VK_CHECK(vkd.acquireNextImageKHR(device, *swapchains[j], foreverNs, acquireSem[j], VK_NULL_HANDLE,
                                                 &imageIndex[j]));

                // If memory allocation is deferred and bind image memory is used, lazily bind image memory now if this is the first time the image is acquired.
                VkImage &acquiredImage = swapchainImages[j][imageIndex[j]];
                if (acquiredImage == VK_NULL_HANDLE)
                {
                    DE_ASSERT(testParams.bindImageMemory && testParams.deferMemoryAllocation);
                    DE_ASSERT(!bimImages[j][imageIndex[j]]);

                    bimImages[j][imageIndex[j]] =
                        bindSingleImageMemory(vkd, device, *swapchains[j], swapchainInfo[j], imageIndex[j]);
                    acquiredImage = **bimImages[j][imageIndex[j]];
                }

                barrier.newLayout =
                    isSharedPresentMode[j] ? VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                barrier.image = acquiredImage;

                vkd.cmdPipelineBarrier(**commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0, nullptr, 0, nullptr, 1, &barrier);
            }

            for (uint32_t j = 0; j < surfaceCount; ++j)
            {
                VkClearColorValue clearValue;
                clearValue.float32[0] = static_cast<float>((i + j * 5) % 33) / 32.0f;
                clearValue.float32[1] = static_cast<float>(((i + j * 5) + 7) % 33) / 32.0f;
                clearValue.float32[2] = static_cast<float>(((i + j * 5) + 17) % 33) / 32.0f;
                clearValue.float32[3] = 1.0f;

                vkd.cmdClearColorImage(**commandBuffers[i], swapchainImages[j][imageIndex[j]],
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
            }

            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            for (uint32_t j = 0; j < surfaceCount; ++j)
            {
                if (!isSharedPresentMode[j])
                {
                    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                }
                else
                {
                    barrier.oldLayout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
                }
                barrier.image = swapchainImages[j][imageIndex[j]];

                vkd.cmdPipelineBarrier(**commandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0, nullptr, 0, nullptr, 1, &barrier);
            }

            endCommandBuffer(vkd, **commandBuffers[i]);

            // Submit the command buffer
            std::vector<VkPipelineStageFlags> waitStages(surfaceCount, VK_PIPELINE_STAGE_TRANSFER_BIT);
            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, surfaceCount, acquireSem.data(), waitStages.data(), 1u,
                &**commandBuffers[i],          1u,      presentSem,
            };
            VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, VK_NULL_HANDLE));

            // Present the frame
            VkSwapchainPresentFenceInfoEXT presentFenceInfo = {
                VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
                nullptr,
                surfaceCount,
                presentFence.data(),
            };
            std::vector<VkResult> result(surfaceCount);

            VkSwapchainPresentModeInfoEXT presentModeInfo = {
                VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT,
                nullptr,
                surfaceCount,
                nullptr,
            };
            std::vector<VkPresentModeKHR> presentModes;
            if (testParams.changePresentModes && rng.getUint32() % 10 != 0)
            {
                presentModes.resize(surfaceCount);
                presentModeInfo.pPresentModes = presentModes.data();
                presentFenceInfo.pNext        = &presentModeInfo;

                // Randomly switch modes.  This is randomly not done to test that the driver doens't expect it to be specified every time.
                for (uint32_t j = 0; j < surfaceCount; ++j)
                {
                    uint32_t randomIndex = rng.getUint32() % (uint32_t)compatiblePresentModes[j].size();
                    presentModes[j]      = compatiblePresentModes[j][randomIndex];
                }
            }

            const VkPresentInfoKHR presentInfo = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                &presentFenceInfo,
                1u,
                presentSem,
                surfaceCount,
                swapchainHandles.data(),
                imageIndex.data(),
                result.data(),
            };
            VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
            for (uint32_t j = 0; j < surfaceCount; ++j)
            {
                VK_CHECK_WSI(result[j]);
            }

            for (uint32_t j = 0; j < surfaceCount; ++j)
            {
                // Check previous presents; if any is signaled, immediatey destroy its wait semaphore
                while (nextUnfinishedPresent[j] < i)
                {
                    const auto unfinishedPresent = nextUnfinishedPresent[j];
                    const auto &fence            = presentFences[unfinishedPresent * surfaceCount + j];
                    if (!fence)
                        ++nextUnfinishedPresent[j];

                    if (!fence || vkd.getFenceStatus(device, **fence) == VK_NOT_READY)
                        break;

                    presentSems[unfinishedPresent].clear();
                    ++nextUnfinishedPresent[j];
                }

                if (testParams.verifyFenceOrdering)
                    verifyFenceSignalOrdering(vkd, device, presentFences, surfaceCount, j, nextUnfinishedPresent[j],
                                              iterations, &results);
            }
        }

        // Wait for outstanding presents and destroy their wait semaphores
        for (uint32_t j = 0; j < surfaceCount; ++j)
        {
            if (testParams.verifyFenceOrdering)
                verifyFenceSignalOrdering(vkd, device, presentFences, surfaceCount, j, nextUnfinishedPresent[j],
                                          iterations, &results);

            while (nextUnfinishedPresent[j] < iterations)
            {
                const auto &fence = presentFences[nextUnfinishedPresent[j] * surfaceCount + j];
                if (fence)
                {
                    VK_CHECK(vkd.waitForFences(device, 1u, &**fence, VK_TRUE, kMaxFenceWaitTimeout));
                    presentSems[nextUnfinishedPresent[j]].clear();
                }
                ++nextUnfinishedPresent[j];
            }
        }
    }
    catch (...)
    {
        // Make sure device is idle before destroying resources
        vkd.deviceWaitIdle(device);
        throw;
    }

    for (uint32_t i = 0; i < surfaceCount; ++i)
    {
        native.windows[i]->setVisible(false);
    }

    return tcu::TestStatus(results.getResult(), results.getMessage());
}

void populatePresentFenceGroup(tcu::TestCaseGroup *testGroup, Type wsiType)
{
    const struct
    {
        VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
        {VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
        {VK_PRESENT_MODE_FIFO_KHR, "fifo"},
        {VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
        {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
        {VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    };

    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name));

        PresentFenceTestConfig config;
        config.wsiType               = wsiType;
        config.modes                 = std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
        config.deferMemoryAllocation = false;
        config.bindImageMemory       = false;
        config.changePresentModes    = false;
        config.verifyFenceOrdering   = false;
        config.nullHandles           = false;
        config.swapchainMaintenance1 = true;

        // Basic present fence test
        addFunctionCase(&*presentModeGroup, "basic", presentFenceTest, config);

        config.verifyFenceOrdering = true;
        // Test ordering guarantee of present fence signals
        addFunctionCase(&*presentModeGroup, "ordering", presentFenceTest, config);

        if (canDoMultiSwapchainPresent(wsiType))
        {
            config.verifyFenceOrdering = false;
            config.modes               = std::vector<VkPresentModeKHR>(3, presentModes[presentModeNdx].mode);
            // Present fence test with multiple swapchains
            addFunctionCase(&*presentModeGroup, "multi_swapchain", presentFenceTest, config);

            config.verifyFenceOrdering = true;
            // Test ordering guarantee of present fence signals with multiple swapchains
            addFunctionCase(&*presentModeGroup, "mult_swapchain_ordering", presentFenceTest, config);

            config.modes               = std::vector<VkPresentModeKHR>(5, presentModes[presentModeNdx].mode);
            config.verifyFenceOrdering = false;
            config.nullHandles         = true;
            addFunctionCase(&*presentModeGroup, "null_handles", presentFenceTest, config);
        }

        config.modes                 = std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
        config.nullHandles           = false;
        config.swapchainMaintenance1 = false;
        addFunctionCase(&*presentModeGroup, "maintenance1_disabled", presentFenceTest, config);

        testGroup->addChild(presentModeGroup.release());
    }
}

struct PresentModesTestConfig
{
    vk::wsi::Type wsiType;
    VkPresentModeKHR mode;
};

tcu::TestStatus verifyCompatiblePresentModes(const std::vector<VkPresentModeKHR> &supportedModes,
                                             const VkPresentModeKHR queryMode,
                                             const std::vector<VkPresentModeKHR> &compatibleModes,
                                             const std::vector<VkPresentModeKHR> *previouslyQueriedCompatibleModes)
{
    // Every returned compatible mode must be supported by the surface
    for (size_t i = 0; i < compatibleModes.size(); ++i)
        if (std::find(supportedModes.begin(), supportedModes.end(), compatibleModes[i]) == supportedModes.end())
            return tcu::TestStatus::fail("Returned compatible present mode " + de::toString(compatibleModes[i]) +
                                         " is not a supported present mode");

    // The original mode being queried must always be in the compatible list
    if (!compatibleModes.empty() &&
        std::find(compatibleModes.begin(), compatibleModes.end(), queryMode) == compatibleModes.end())
        return tcu::TestStatus::fail("Returned compatible present modes does not include the mode used in the query");

    // There should be no duplicates in the returned modes
    std::set<VkPresentModeKHR> visitedModes;
    for (VkPresentModeKHR compatibleMode : compatibleModes)
    {
        if (visitedModes.find(compatibleMode) != visitedModes.end())
            return tcu::TestStatus::fail("Duplicate mode " + de::toString(compatibleMode) +
                                         " returned in list of compatible present modes");
        visitedModes.insert(compatibleMode);
    }

    // If provided, the returned list of modes should match the last previous query
    if (previouslyQueriedCompatibleModes)
    {
        for (VkPresentModeKHR previousCompatibleMode : *previouslyQueriedCompatibleModes)
            if (visitedModes.find(previousCompatibleMode) == visitedModes.end())
                return tcu::TestStatus::fail("Different sets of compatible modes returned on re-query (present mode " +
                                             de::toString(previousCompatibleMode) + " missing on requery)");
    }

    return tcu::TestStatus::pass("");
}

tcu::TestStatus presentModesQueryTest(Context &context, const PresentModesTestConfig testParams)
{
    const InstanceHelper instHelper(context, testParams.wsiType, false);
    const TestNativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType, 1);
    Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display,
                                               *native.windows[0], context.getTestContext().getCommandLine()));
    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, false, false, false);

    const std::vector<VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    // Get the compatible present modes with the given one.
    VkSurfacePresentModeEXT presentModeInfo = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
        nullptr,
        testParams.mode,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        &presentModeInfo,
        *surface,
    };
    VkSurfacePresentModeCompatibilityEXT compatibility = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
        nullptr,
        0,
        nullptr,
    };
    VkSurfaceCapabilities2KHR capabilities = {
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        &compatibility,
        {},
    };

    // Test that querying only the count works.
    VK_CHECK(
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

    // Sometime ICD selected will not support the instance extensions got in enumerateInstanceExtensionProperties.
    // In this case the struct varible compatibility queried in getPhysicalDeviceSurfaceCapabilities2KHR will keep unchanged.
    if (compatibility.presentModeCount < 1)
        TCU_THROW(NotSupportedError, "Empty compatible present mode list, VK_EXT_surface_maintenance1 not supported.");

    // Test again providing a buffer that's too small
    constexpr VkPresentModeKHR invalidValue = (VkPresentModeKHR)0x1234;
    std::vector<VkPresentModeKHR> compatibleModes(compatibility.presentModeCount, invalidValue);
    compatibility.pPresentModes = compatibleModes.data();

    uint32_t originalCompatibleModesCount = compatibility.presentModeCount;

    // Check result when count is 0
    compatibility.presentModeCount = 0;
    VkResult result =
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities);
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Wrong result when the size is 0");

    // Check result when count is too small
    compatibility.presentModeCount = originalCompatibleModesCount - 1;
    result =
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities);
    if (result != VK_SUCCESS)
        return tcu::TestStatus::fail("Wrong result when the size is too small");

    // Make sure whatever _is_ returned is valid.
    if (compatibility.presentModeCount > originalCompatibleModesCount - 1)
        return tcu::TestStatus::fail("Re-query returned more results than provided");

    // Ensure the rest of the array is not overwritten
    for (size_t i = compatibility.presentModeCount; i < compatibleModes.size(); ++i)
    {
        if (compatibleModes[i] != invalidValue)
            return tcu::TestStatus::fail("Query overwrote beyond returned count");
    }
    compatibleModes.resize(compatibility.presentModeCount);
    tcu::TestStatus status = verifyCompatiblePresentModes(presentModes, testParams.mode, compatibleModes, nullptr);
    if (status.isFail())
        return status;

    // Check result when count is correct
    compatibility.presentModeCount = originalCompatibleModesCount;
    std::vector<VkPresentModeKHR> compatibleModes2(compatibility.presentModeCount, invalidValue);
    compatibility.pPresentModes = compatibleModes2.data();

    VK_CHECK(
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

    // Make sure returned modes are valid.
    if (compatibility.presentModeCount != originalCompatibleModesCount)
        return tcu::TestStatus::fail("Re-query returned different results count than provided");

    status = verifyCompatiblePresentModes(presentModes, testParams.mode, compatibleModes2, &compatibleModes);
    if (status.isFail())
        return status;

    // Check that querying with a count higher than supported still returns as many results as before.
    compatibility.presentModeCount = originalCompatibleModesCount * 2;
    std::vector<VkPresentModeKHR> compatibleModes3(compatibility.presentModeCount, invalidValue);
    compatibility.pPresentModes = compatibleModes3.data();

    VK_CHECK(
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

    // Make sure returned modes are the same as before.
    if (compatibility.presentModeCount != originalCompatibleModesCount)
        return tcu::TestStatus::fail("Re-query returned different results count than provided");

    // Ensure the rest of the array is not overwritten
    for (size_t i = compatibility.presentModeCount; i < compatibleModes3.size(); ++i)
    {
        if (compatibleModes3[i] != invalidValue)
            return tcu::TestStatus::fail("Query overwrote beyond returned count");
    }

    compatibleModes3.resize(compatibility.presentModeCount);
    status = verifyCompatiblePresentModes(presentModes, testParams.mode, compatibleModes3, &compatibleModes2);
    if (status.isFail())
        return status;

    return tcu::TestStatus::pass("Tests ran successfully");
}

void populatePresentModesGroup(tcu::TestCaseGroup *testGroup, Type wsiType)
{
    const struct
    {
        VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
        {VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
        {VK_PRESENT_MODE_FIFO_KHR, "fifo"},
        {VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
        {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
        {VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    };

    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name));

        {
            PresentModesTestConfig config;
            config.wsiType = wsiType;
            config.mode    = presentModes[presentModeNdx].mode;

            // Query compatible present modes
            addFunctionCase(&*presentModeGroup, "query", presentModesQueryTest, config);
        }

        {
            PresentFenceTestConfig config;
            config.wsiType               = wsiType;
            config.modes                 = std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
            config.deferMemoryAllocation = false;
            config.bindImageMemory       = false;
            config.changePresentModes    = true;
            config.verifyFenceOrdering   = false;
            config.nullHandles           = false;
            config.swapchainMaintenance1 = true;

            // Switch between compatible modes
            addFunctionCase(&*presentModeGroup, "change_modes", presentFenceTest, config);

            if (canDoMultiSwapchainPresent(wsiType))
            {
                config.modes = std::vector<VkPresentModeKHR>(4, presentModes[presentModeNdx].mode);

                // Switch between compatible modes with multiple swapchains
                addFunctionCase(&*presentModeGroup, "change_modes_multi_swapchain", presentFenceTest, config);

                config.modes                 = std::vector<VkPresentModeKHR>(2, presentModes[presentModeNdx].mode);
                config.deferMemoryAllocation = true;

                // Switch between compatible modes while swapchain uses deferred allocation
                addFunctionCase(&*presentModeGroup, "change_modes_with_deferred_alloc", presentFenceTest, config);
            }
        }

        testGroup->addChild(presentModeGroup.release());
    }

    if (canDoMultiSwapchainPresent(wsiType))
    {
        // Switch between compatible modes with multiple swapchains in different modes
        de::MovePtr<tcu::TestCaseGroup> heterogenousGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), "heterogenous"));

        std::vector<VkPresentModeKHR> modes(3);
        for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(presentModes); i++)
        {
            for (size_t j = 0; j < DE_LENGTH_OF_ARRAY(presentModes); j++)
            {
                for (size_t k = 0; k < DE_LENGTH_OF_ARRAY(presentModes); k++)
                {
                    // Skip if not actually heterogenous
                    if (i == j && i == k)
                        continue;

                    std::string testName = presentModes[i].name;
                    testName += "_";
                    testName += presentModes[j].name;
                    testName += "_";
                    testName += presentModes[k].name;

                    modes[0] = presentModes[i].mode;
                    modes[1] = presentModes[j].mode;
                    modes[2] = presentModes[k].mode;

                    PresentFenceTestConfig config;
                    config.wsiType               = wsiType;
                    config.modes                 = modes;
                    config.deferMemoryAllocation = false;
                    config.bindImageMemory       = false;
                    config.changePresentModes    = true;
                    config.verifyFenceOrdering   = false;
                    config.nullHandles           = false;
                    config.swapchainMaintenance1 = true;

                    addFunctionCase(&*heterogenousGroup, testName, presentFenceTest, config);
                }
            }
        }

        testGroup->addChild(heterogenousGroup.release());
    }
}

enum class SwapchainWindowSize
{
    Identical,
    SwapchainBigger,
    SwapchainSmaller,
};

enum class SwapchainWindowAspect
{
    Identical,
    SwapchainTaller,
    SwapchainWider,
};

struct ScalingQueryTestConfig
{
    vk::wsi::Type wsiType;
    VkPresentModeKHR mode;
};

struct ScalingTestConfig
{
    vk::wsi::Type wsiType;
    VkPresentModeKHR mode;
    VkPresentScalingFlagsEXT scaling;
    VkPresentGravityFlagsEXT gravityX;
    VkPresentGravityFlagsEXT gravityY;
    SwapchainWindowSize size;
    SwapchainWindowAspect aspect;
    // Either have the swapchain be created with a different size, or resize the window after swapchain creation
    bool resizeWindow;
};

tcu::TestStatus scalingQueryTest(Context &context, const ScalingQueryTestConfig testParams)
{
    const InstanceHelper instHelper(context, testParams.wsiType, false);
    const TestNativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType, 1);
    Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display,
                                               *native.windows[0], context.getTestContext().getCommandLine()));
    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, false, false, false);

    const std::vector<VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    // Query the scaling capabilities and make sure they only report acceptable values.
    VkSurfacePresentScalingCapabilitiesEXT scaling =
        getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, testParams.mode, *surface);

    constexpr VkPresentScalingFlagsEXT scalingFlags = VK_PRESENT_SCALING_ONE_TO_ONE_BIT_EXT |
                                                      VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_EXT |
                                                      VK_PRESENT_SCALING_STRETCH_BIT_EXT;
    constexpr VkPresentGravityFlagsEXT gravityFlags =
        VK_PRESENT_GRAVITY_MIN_BIT_EXT | VK_PRESENT_GRAVITY_MAX_BIT_EXT | VK_PRESENT_GRAVITY_CENTERED_BIT_EXT;

    if ((scaling.supportedPresentScaling & ~scalingFlags) != 0)
        return tcu::TestStatus::fail("Invalid bits in scaling flags");

    if ((scaling.supportedPresentGravityX & ~gravityFlags) != 0)
        return tcu::TestStatus::fail("Invalid bits in gravity flags (x axis)");

    if ((scaling.supportedPresentGravityY & ~gravityFlags) != 0)
        return tcu::TestStatus::fail("Invalid bits in gravity flags (y axis)");

    return tcu::TestStatus::pass("Tests ran successfully");
}

tcu::TestStatus scalingQueryCompatibleModesTest(Context &context, const ScalingQueryTestConfig testParams)
{
    const InstanceHelper instHelper(context, testParams.wsiType, false);
    const TestNativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType, 1);
    Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display,
                                               *native.windows[0], context.getTestContext().getCommandLine()));
    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, false, false, false);

    const std::vector<VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    // Query compatible present modes, and scaling capabilities for each mode.  They must all be identical.
    VkSurfacePresentModeEXT presentModeInfo = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
        nullptr,
        testParams.mode,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        &presentModeInfo,
        *surface,
    };
    VkSurfacePresentModeCompatibilityEXT compatibility = {
        VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
        nullptr,
        0,
        nullptr,
    };
    VkSurfaceCapabilities2KHR capabilities = {
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        &compatibility,
        {},
    };

    VK_CHECK(
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));
    std::vector<VkPresentModeKHR> compatibleModes(compatibility.presentModeCount, (VkPresentModeKHR)0x5678);
    compatibility.pPresentModes = compatibleModes.data();

    VK_CHECK(
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

    std::vector<VkSurfacePresentScalingCapabilitiesEXT> scaling(compatibility.presentModeCount);

    for (uint32_t i = 0; i < compatibility.presentModeCount; ++i)
        scaling[i] =
            getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, compatibleModes[i], *surface);

    for (uint32_t i = 1; i < compatibility.presentModeCount; ++i)
    {
        if (scaling[i].supportedPresentScaling != scaling[0].supportedPresentScaling)
            return tcu::TestStatus::fail("Different scaling flags for compatible present modes is not allowed");

        if (scaling[i].supportedPresentGravityX != scaling[0].supportedPresentGravityX)
            return tcu::TestStatus::fail(
                "Different gravity flags (x axis) for compatible present modes is not allowed");

        if (scaling[i].supportedPresentGravityY != scaling[0].supportedPresentGravityY)
            return tcu::TestStatus::fail(
                "Different gravity flags (y axis) for compatible present modes is not allowed");
    }

    return tcu::TestStatus::pass("Tests ran successfully");
}

tcu::TestStatus scalingTest(Context &context, const ScalingTestConfig testParams)
{
    const InstanceHelper instHelper(context, testParams.wsiType, false);
    TestNativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType, 1);
    Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display,
                                               *native.windows[0], context.getTestContext().getCommandLine()));

    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, true, true, false);
    const DeviceInterface &vkd = devHelper.vkd;
    const VkDevice device      = *devHelper.device;
    SimpleAllocator allocator(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));

    std::vector<VkSurfaceFormatKHR> surfaceFormats =
        getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surface);
    if (surfaceFormats.empty())
        return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

    const VkSurfaceCapabilitiesKHR capabilities =
        getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface, nullptr);
    const VkSurfaceTransformFlagBitsKHR transform =
        (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ?
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR :
            capabilities.currentTransform;

    const std::vector<VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    // Skip if configuration is not supported
    VkSurfacePresentScalingCapabilitiesEXT scaling =
        getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, testParams.mode, *surface);

    if ((scaling.supportedPresentScaling & testParams.scaling) == 0)
        TCU_THROW(NotSupportedError, "Scaling mode is not supported");
    if (testParams.scaling != VK_PRESENT_SCALING_STRETCH_BIT_EXT)
    {
        if ((scaling.supportedPresentGravityX & testParams.gravityX) == 0)
            TCU_THROW(NotSupportedError, "Gravity mode is not supported (x axis)");
        if ((scaling.supportedPresentGravityY & testParams.gravityY) == 0)
            TCU_THROW(NotSupportedError, "Gravity mode is not supported (y axis)");
    }

    tcu::UVec2 swapchainSize = native.windowSize;
    if (!testParams.resizeWindow)
    {
        switch (testParams.size)
        {
        case SwapchainWindowSize::SwapchainBigger:
            swapchainSize.x() *= 2;
            swapchainSize.y() *= 2;
            break;
        case SwapchainWindowSize::SwapchainSmaller:
            swapchainSize.x() /= 2;
            swapchainSize.y() /= 2;
            break;
        default:
            break;
        }
        switch (testParams.aspect)
        {
        case SwapchainWindowAspect::SwapchainTaller:
            swapchainSize.y() += swapchainSize.y() / 2;
            break;
        case SwapchainWindowAspect::SwapchainWider:
            swapchainSize.x() += swapchainSize.x() / 2;
            break;
        default:
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(
        *surface, surfaceFormats[0], swapchainSize, testParams.mode, transform, capabilities.minImageCount, false);

    VkSwapchainPresentScalingCreateInfoEXT scalingInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,
        nullptr,
        testParams.scaling,
        testParams.gravityX,
        testParams.gravityY,
    };
    swapchainInfo.pNext = &scalingInfo;

    const Unique<VkSwapchainKHR> swapchain(createSwapchainKHR(vkd, device, &swapchainInfo));
    std::vector<VkImage> swapchainImages = getSwapchainImages(vkd, device, *swapchain);

    const Unique<VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

    constexpr uint32_t iterations = 100;

    // Do testParams.iterations presents, with a fence associated with the last one.
    FenceSp presentFence = FenceSp(new Unique<VkFence>(createFence(vkd, device)));
    const std::vector<SemaphoreSp> acquireSems(createSemaphores(vkd, device, iterations));
    const std::vector<SemaphoreSp> presentSems(createSemaphores(vkd, device, iterations));

    const std::vector<CommandBufferSp> commandBuffers(
        allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, iterations));

    const uint64_t foreverNs = 0xFFFFFFFFFFFFFFFFul;

    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
    };

    tcu::UVec2 windowSize = tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight);
    if (testParams.resizeWindow)
    {
        switch (testParams.size)
        {
        case SwapchainWindowSize::SwapchainBigger:
            windowSize.x() /= 2;
            windowSize.y() /= 2;
            break;
        case SwapchainWindowSize::SwapchainSmaller:
            windowSize.x() *= 2;
            windowSize.y() *= 2;
            break;
        default:
            break;
        }
        switch (testParams.aspect)
        {
        case SwapchainWindowAspect::SwapchainTaller:
            windowSize.x() += windowSize.x() / 2;
            break;
        case SwapchainWindowAspect::SwapchainWider:
            windowSize.y() += windowSize.y() / 2;
            break;
        default:
            break;
        }

        native.resizeWindow(0, windowSize);
    }

    const uint32_t quarterPixels = swapchainSize.x() * swapchainSize.y() / 4;
    const tcu::UVec4 red(255, 30, 20, 255);
    const tcu::UVec4 green(0, 255, 50, 255);
    const tcu::UVec4 blue(40, 60, 255, 255);
    const tcu::UVec4 yellow(200, 220, 20, 255);
    de::MovePtr<Allocation> redMemory;
    de::MovePtr<Allocation> greenMemory;
    de::MovePtr<Allocation> blueMemory;
    de::MovePtr<Allocation> yellowMemory;
    const vk::Move<vk::VkBuffer> redBuffer =
        createBufferAndBindMemory(devHelper, allocator, red, quarterPixels, &redMemory);
    const vk::Move<vk::VkBuffer> greenBuffer =
        createBufferAndBindMemory(devHelper, allocator, green, quarterPixels, &greenMemory);
    const vk::Move<vk::VkBuffer> blueBuffer =
        createBufferAndBindMemory(devHelper, allocator, blue, quarterPixels, &blueMemory);
    const vk::Move<vk::VkBuffer> yellowBuffer =
        createBufferAndBindMemory(devHelper, allocator, yellow, quarterPixels, &yellowMemory);

    try
    {
        for (uint32_t i = 0; i < iterations; ++i)
        {
            const VkSemaphore presentSem = **presentSems[i];
            const VkSemaphore acquireSem = **acquireSems[i];
            uint32_t imageIndex          = 0x12345; // initialize to junk value

            VK_CHECK(vkd.acquireNextImageKHR(device, *swapchain, foreverNs, acquireSem, VK_NULL_HANDLE, &imageIndex));

            beginCommandBuffer(vkd, **commandBuffers[i], 0u);

            VkImageMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                nullptr,
                0,
                0,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                swapchainImages[imageIndex],
                range,
            };

            vkd.cmdPipelineBarrier(**commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0, nullptr, 0, nullptr, 1, &barrier);

            const tcu::UVec2 halfSwapchainSize = swapchainSize / 2u;
            copyBufferToImage(vkd, **commandBuffers[i], *redBuffer, swapchainImages[imageIndex], tcu::UVec2(0, 0),
                              halfSwapchainSize);
            copyBufferToImage(vkd, **commandBuffers[i], *greenBuffer, swapchainImages[imageIndex],
                              tcu::UVec2(halfSwapchainSize.x(), 0),
                              tcu::UVec2(swapchainSize.x() - halfSwapchainSize.x(), halfSwapchainSize.y()));
            copyBufferToImage(vkd, **commandBuffers[i], *blueBuffer, swapchainImages[imageIndex],
                              tcu::UVec2(0, halfSwapchainSize.y()),
                              tcu::UVec2(halfSwapchainSize.x(), swapchainSize.y() - halfSwapchainSize.y()));
            copyBufferToImage(
                vkd, **commandBuffers[i], *yellowBuffer, swapchainImages[imageIndex], halfSwapchainSize,
                tcu::UVec2(swapchainSize.x() - halfSwapchainSize.x(), swapchainSize.y() - halfSwapchainSize.y()));

            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkd.cmdPipelineBarrier(**commandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0, nullptr, 0, nullptr, 1, &barrier);

            endCommandBuffer(vkd, **commandBuffers[i]);

            // Submit the command buffer
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            const VkSubmitInfo submitInfo  = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1,           &acquireSem, &waitStage, 1u,
                &**commandBuffers[i],          1u,      &presentSem,
            };
            VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, VK_NULL_HANDLE));

            // Present the frame
            const VkSwapchainPresentFenceInfoEXT presentFenceInfo = {
                VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
                nullptr,
                1,
                &**presentFence,
            };
            VkResult result;

            const VkPresentInfoKHR presentInfo = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                // Signal the present fence on the last present.
                i + 1 == iterations ? &presentFenceInfo : nullptr,
                1u,
                &presentSem,
                1,
                &*swapchain,
                &imageIndex,
                &result,
            };
            VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
            VK_CHECK_WSI(result);

            // TODO: wait for present, capture the screen and verify that scaling is done correctly.
        }

        // Wait for all presents before terminating the test (when semaphores are destroyed)
        VK_CHECK(vkd.waitForFences(device, 1u, &**presentFence, VK_TRUE, kMaxFenceWaitTimeout));
    }
    catch (...)
    {
        // Make sure device is idle before destroying resources
        vkd.deviceWaitIdle(device);
        throw;
    }

    native.windows[0]->setVisible(false);

    return tcu::TestStatus::pass("Tests ran successfully");
}

void populateScalingTests(tcu::TestCaseGroup *testGroup, Type wsiType, bool resizeWindow)
{
    const struct
    {
        VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
        {VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
        {VK_PRESENT_MODE_FIFO_KHR, "fifo"},
        {VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
        {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
        {VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    };

    const struct
    {
        VkPresentScalingFlagBitsEXT scaling;
        const char *name;
    } scalingFlags[] = {
        {VK_PRESENT_SCALING_ONE_TO_ONE_BIT_EXT, "one_to_one"},
        {VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_EXT, "aspect_stretch"},
        {VK_PRESENT_SCALING_STRETCH_BIT_EXT, "stretch"},
    };

    const struct
    {
        VkPresentGravityFlagBitsEXT gravity;
        const char *name;
    } gravityFlags[] = {
        {VK_PRESENT_GRAVITY_MIN_BIT_EXT, "min"},
        {VK_PRESENT_GRAVITY_MAX_BIT_EXT, "max"},
        {VK_PRESENT_GRAVITY_CENTERED_BIT_EXT, "center"},
    };

    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name));

        {
            ScalingQueryTestConfig config;
            config.wsiType = wsiType;
            config.mode    = presentModes[presentModeNdx].mode;

            // Query supported scaling modes
            de::MovePtr<tcu::TestCaseGroup> queryGroup(new tcu::TestCaseGroup(testGroup->getTestContext(), "query"));
            // Basic test
            addFunctionCase(&*queryGroup, "basic", scalingQueryTest, config);
            // Verify compatible present modes have the same scaling capabilities
            addFunctionCase(&*queryGroup, "verify_compatible_present_modes", scalingQueryCompatibleModesTest, config);
            presentModeGroup->addChild(queryGroup.release());
        }

        for (size_t scalingFlagNdx = 0; scalingFlagNdx < DE_LENGTH_OF_ARRAY(scalingFlags); scalingFlagNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> scalingFlagGroup(
                new tcu::TestCaseGroup(testGroup->getTestContext(), scalingFlags[scalingFlagNdx].name));

            const bool isStretch = scalingFlags[scalingFlagNdx].scaling == VK_PRESENT_SCALING_STRETCH_BIT_EXT;

            for (size_t gravityFlagXNdx = 0; gravityFlagXNdx < DE_LENGTH_OF_ARRAY(gravityFlags); gravityFlagXNdx++)
            {
                for (size_t gravityFlagYNdx = 0; gravityFlagYNdx < DE_LENGTH_OF_ARRAY(gravityFlags); gravityFlagYNdx++)
                {
                    std::string testName = gravityFlags[gravityFlagXNdx].name;
                    testName += "_";
                    testName += gravityFlags[gravityFlagYNdx].name;

                    de::MovePtr<tcu::TestCaseGroup> gravityFlagsGroup(
                        new tcu::TestCaseGroup(scalingFlagGroup->getTestContext(), testName.c_str()));

                    ScalingTestConfig config;
                    config.wsiType      = wsiType;
                    config.mode         = presentModes[presentModeNdx].mode;
                    config.scaling      = scalingFlags[scalingFlagNdx].scaling;
                    config.gravityX     = gravityFlags[gravityFlagXNdx].gravity;
                    config.gravityY     = gravityFlags[gravityFlagYNdx].gravity;
                    config.size         = SwapchainWindowSize::Identical;
                    config.aspect       = SwapchainWindowAspect::Identical;
                    config.resizeWindow = resizeWindow;

                    // Gravity does not apply to stretch
                    de::MovePtr<tcu::TestCaseGroup> *group = isStretch ? &scalingFlagGroup : &gravityFlagsGroup;

                    // Basic test without actual scaling
                    addFunctionCase(&**group, "same_size_and_aspect", scalingTest, config);

                    config.size = SwapchainWindowSize::SwapchainBigger;
                    // Swapchain is bigger than window, but has same aspect
                    addFunctionCase(&**group, "swapchain_bigger_same_aspect", scalingTest, config);

                    config.size = SwapchainWindowSize::SwapchainSmaller;
                    // Swapchain is smaller than window, but has same aspect
                    addFunctionCase(&**group, "swapchain_smaller_same_aspect", scalingTest, config);

                    config.size   = SwapchainWindowSize::Identical;
                    config.aspect = SwapchainWindowAspect::SwapchainTaller;
                    // Swapchain has same width, but is taller than window
                    addFunctionCase(&**group, "swapchain_taller", scalingTest, config);

                    config.size = SwapchainWindowSize::SwapchainBigger;
                    // Swapchain is bigger than window, and is taller in aspect ratio
                    addFunctionCase(&**group, "swapchain_bigger_taller_aspect", scalingTest, config);

                    config.size = SwapchainWindowSize::SwapchainSmaller;
                    // Swapchain is smaller than window, but is taller in aspect ratio
                    addFunctionCase(&**group, "swapchain_smaller_taller_aspect", scalingTest, config);

                    config.size   = SwapchainWindowSize::Identical;
                    config.aspect = SwapchainWindowAspect::SwapchainWider;
                    // Swapchain has same height, but is wider than window
                    addFunctionCase(&**group, "swapchain_wider", scalingTest, config);

                    config.size = SwapchainWindowSize::SwapchainBigger;
                    // Swapchain is bigger than window, and is wider in aspect ratio
                    addFunctionCase(&**group, "swapchain_bigger_wider_aspect", scalingTest, config);

                    config.size = SwapchainWindowSize::SwapchainSmaller;
                    // Swapchain is smaller than window, but is wider in aspect ratio
                    addFunctionCase(&**group, "swapchain_smaller_wider_aspect", scalingTest, config);

                    if (isStretch)
                    {
                        break;
                    }

                    scalingFlagGroup->addChild(gravityFlagsGroup.release());
                }

                if (isStretch)
                {
                    break;
                }
            }

            presentModeGroup->addChild(scalingFlagGroup.release());
        }

        testGroup->addChild(presentModeGroup.release());
    }
}

void populateScalingGroup(tcu::TestCaseGroup *testGroup, Type wsiType)
{
    populateScalingTests(testGroup, wsiType, false);

    de::MovePtr<tcu::TestCaseGroup> resizeWindowGroup(
        new tcu::TestCaseGroup(testGroup->getTestContext(), "resize_window"));
    populateScalingTests(&*resizeWindowGroup, wsiType, true);
    testGroup->addChild(resizeWindowGroup.release());
}

void populateDeferredAllocGroup(tcu::TestCaseGroup *testGroup, Type wsiType)
{
    const struct
    {
        VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
        {VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
        {VK_PRESENT_MODE_FIFO_KHR, "fifo"},
        {VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
        {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
        {VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    };

    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name));

        PresentFenceTestConfig config;
        config.wsiType               = wsiType;
        config.modes                 = std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
        config.deferMemoryAllocation = true;
        config.bindImageMemory       = false;
        config.changePresentModes    = false;
        config.verifyFenceOrdering   = false;
        config.nullHandles           = false;
        config.swapchainMaintenance1 = true;

        // Basic deferred allocation test
        addFunctionCase(&*presentModeGroup, "basic", presentFenceTest, config);

        config.bindImageMemory = true;

        // Bind image memory + shared present mode crashing on some drivers for unrelated reasons to VK_EXT_swapchain_maintenance1.  Will enable this test separately.
        if (presentModes[presentModeNdx].mode != VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR &&
            presentModes[presentModeNdx].mode != VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
        {
            // Bind image with VkBindImageMemorySwapchainInfoKHR
            addFunctionCase(&*presentModeGroup, "bind_image", presentFenceTest, config);
        }

        if (canDoMultiSwapchainPresent(wsiType))
        {
            config.modes = std::vector<VkPresentModeKHR>(2, presentModes[presentModeNdx].mode);

            // Bind image with VkBindImageMemorySwapchainInfoKHR with multiple swapchains
            addFunctionCase(&*presentModeGroup, "bind_image_multi_swapchain", presentFenceTest, config);
        }

        testGroup->addChild(presentModeGroup.release());
    }
}

enum class ResizeWindow
{
    No,
    BeforeAcquire,
    BeforePresent,
};

struct ReleaseImagesTestConfig
{
    vk::wsi::Type wsiType;
    VkPresentModeKHR mode;
    VkPresentScalingFlagsEXT scaling;
    ResizeWindow resizeWindow;
    bool releaseBeforePresent;
    bool releaseBeforeRetire;
};

tcu::TestStatus releaseImagesTest(Context &context, const ReleaseImagesTestConfig testParams)
{
    const InstanceHelper instHelper(context, testParams.wsiType, false);
    TestNativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType, 1);
    Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display,
                                               *native.windows[0], context.getTestContext().getCommandLine()));

    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, true, true, false);
    const DeviceInterface &vkd = devHelper.vkd;
    const VkDevice device      = *devHelper.device;

    std::vector<VkSurfaceFormatKHR> surfaceFormats =
        getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surface);
    if (surfaceFormats.empty())
        return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

    const std::vector<VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    const VkSurfaceCapabilitiesKHR capabilities =
        getPerPresentSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface, testParams.mode);
    const VkSurfaceTransformFlagBitsKHR transform =
        (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ?
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR :
            capabilities.currentTransform;

    if (testParams.scaling != 0)
    {
        // Skip if configuration is not supported
        VkSurfacePresentScalingCapabilitiesEXT scaling =
            getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, testParams.mode, *surface);

        if ((scaling.supportedPresentScaling & testParams.scaling) == 0)
            TCU_THROW(NotSupportedError, "Scaling mode is not supported");
    }

    const bool isSharedPresentMode = testParams.mode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
                                     testParams.mode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
    if (isSharedPresentMode && (capabilities.minImageCount != 1 || capabilities.maxImageCount != 1))
    {
        return tcu::TestStatus::fail("min and max image count for shared present modes must be 1");
    }

    uint32_t imageCount = capabilities.minImageCount + 10;
    if (capabilities.maxImageCount > 0)
        imageCount = de::min(imageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(*surface, surfaceFormats[0], native.windowSize,
                                                                         testParams.mode, transform, imageCount, false);

    VkSwapchainPresentScalingCreateInfoEXT scalingInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT, nullptr, testParams.scaling, 0, 0,
    };
    swapchainInfo.pNext = &scalingInfo;

    Move<VkSwapchainKHR> swapchain(createSwapchainKHR(vkd, device, &swapchainInfo));
    std::vector<VkImage> swapchainImages = getSwapchainImages(vkd, device, *swapchain);

    const Unique<VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

    const uint32_t iterations = getIterations({testParams.mode}, {}, testParams.resizeWindow != ResizeWindow::No);

    // Do testParams.iterations presents, with a fence associated with the last one.
    FenceSp presentFence = FenceSp(new Unique<VkFence>(createFence(vkd, device)));
    const std::vector<SemaphoreSp> acquireSems(createSemaphores(vkd, device, iterations));
    const std::vector<SemaphoreSp> presentSems(createSemaphores(vkd, device, iterations));

    const std::vector<CommandBufferSp> commandBuffers(
        allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, iterations));

    const uint64_t foreverNs = 0xFFFFFFFFFFFFFFFFul;

    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1,
    };

    const uint32_t configHash = (uint32_t)testParams.wsiType | (uint32_t)testParams.mode << 4 |
                                (uint32_t)testParams.scaling << 24 | (uint32_t)testParams.resizeWindow << 28 |
                                (uint32_t)testParams.releaseBeforePresent << 30 |
                                (uint32_t)testParams.releaseBeforeRetire << 31;
    de::Random rng(0x53A4C8A1u ^ configHash);

    try
    {
        for (uint32_t i = 0; i < iterations; ++i)
        {
            // Decide on how many acquires to do, and whether a presentation is to be done.  Presentation is always done for the last iteration, to facilitate clean up (by adding a present fence).
            const uint32_t maxAllowedAcquires = (uint32_t)swapchainImages.size() - capabilities.minImageCount + 1;
            const uint32_t acquireCount       = rng.getUint32() % maxAllowedAcquires + 1;
            const bool doPresent              = i + 1 == iterations || rng.getUint32() % 10 != 0;
            const bool doResize         = testParams.resizeWindow != ResizeWindow::No && rng.getUint32() % 10 != 0;
            const uint32_t presentIndex = doPresent ? rng.getUint32() % acquireCount : acquireCount;

            // Resize the window if requested.
            if (doResize && testParams.resizeWindow == ResizeWindow::BeforeAcquire)
            {
                tcu::UVec2 windowSize = tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight);
                windowSize.x()        = windowSize.x() - 20 + rng.getUint32() % 41;
                windowSize.y()        = windowSize.y() - 20 + rng.getUint32() % 41;

                native.resizeWindow(0, windowSize);
            }

            // Acquire N times
            const VkSemaphore presentSem = **presentSems[i];
            const VkSemaphore acquireSem = **acquireSems[i];
            std::vector<uint32_t> acquiredIndices(acquireCount, 0x12345);
            FenceSp acquireFenceSp      = FenceSp(new Unique<VkFence>(createFence(vkd, device)));
            const VkFence &acquireFence = **acquireFenceSp;

            VkResult result =
                vkd.acquireNextImageKHR(device, *swapchain, foreverNs, presentIndex == 0 ? acquireSem : VK_NULL_HANDLE,
                                        acquireFence, &acquiredIndices[0]);
            if (result == VK_SUCCESS)
            {
                VK_CHECK(vkd.waitForFences(device, 1u, &acquireFence, VK_TRUE, kMaxFenceWaitTimeout));
                VK_CHECK(vkd.resetFences(device, 1u, &acquireFence));
            }

            // If out of date, recreate the swapchain and reacquire.
            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                if (testParams.scaling == 0)
                {
                    swapchainInfo.imageExtent = vk::makeExtent2D(native.windowSize.x(), native.windowSize.y());
                }

                swapchainInfo.oldSwapchain = *swapchain;
                Move<VkSwapchainKHR> newSwapchain(createSwapchainKHR(vkd, device, &swapchainInfo));
                swapchain = std::move(newSwapchain);

                const size_t previousImageCount = swapchainImages.size();
                swapchainImages                 = getSwapchainImages(vkd, device, *swapchain);
                if (previousImageCount != swapchainImages.size())
                    TCU_THROW(InternalError,
                              "Unexpected change in number of swapchain images when recreated during window resize");

                result = vkd.acquireNextImageKHR(device, *swapchain, foreverNs,
                                                 presentIndex == 0 ? acquireSem : VK_NULL_HANDLE, acquireFence,
                                                 &acquiredIndices[0]);
                if (result == VK_SUCCESS)
                {
                    VK_CHECK(vkd.waitForFences(device, 1u, &acquireFence, VK_TRUE, kMaxFenceWaitTimeout));
                    VK_CHECK(vkd.resetFences(device, 1u, &acquireFence));
                }
            }

            VK_CHECK_WSI(result);

            for (uint32_t j = 1; j < acquireCount; ++j)
            {
                VK_CHECK_WSI(vkd.acquireNextImageKHR(device, *swapchain, foreverNs,
                                                     presentIndex == j ? acquireSem : VK_NULL_HANDLE, acquireFence,
                                                     &acquiredIndices[j]));
                VK_CHECK(vkd.waitForFences(device, 1u, &acquireFence, VK_TRUE, kMaxFenceWaitTimeout));
                VK_CHECK(vkd.resetFences(device, 1u, &acquireFence));
            }

            // Construct a list of image indices to be released.  That is every index except the one being presented, if any.
            std::vector<uint32_t> releaseIndices = acquiredIndices;
            if (doPresent)
            {
                releaseIndices.erase(releaseIndices.begin() + presentIndex);
            }
            size_t imageReleaseSize = releaseIndices.size();

            // Randomize the indices to be released.
            rng.shuffle(releaseIndices.begin(), releaseIndices.end());

            if (doResize && testParams.resizeWindow == ResizeWindow::BeforePresent)
            {
                tcu::UVec2 windowSize = tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight);
                windowSize.x()        = windowSize.x() - 20 + rng.getUint32() % 41;
                windowSize.y()        = windowSize.y() - 20 + rng.getUint32() % 41;

                native.resizeWindow(0, windowSize);
            }

            if (doPresent)
            {
                beginCommandBuffer(vkd, **commandBuffers[i], 0u);

                VkImageMemoryBarrier barrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    nullptr,
                    0,
                    0,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    swapchainImages[acquiredIndices[presentIndex]],
                    range,
                };
                vkd.cmdPipelineBarrier(**commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0, nullptr, 0, nullptr, 1, &barrier);

                VkClearColorValue clearValue;
                clearValue.float32[0] = static_cast<float>(i % 33) / 32.0f;
                clearValue.float32[1] = static_cast<float>((i + 7) % 33) / 32.0f;
                clearValue.float32[2] = static_cast<float>((i + 17) % 33) / 32.0f;
                clearValue.float32[3] = 1.0f;

                vkd.cmdClearColorImage(**commandBuffers[i], swapchainImages[acquiredIndices[presentIndex]],
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

                barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                vkd.cmdPipelineBarrier(**commandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0, nullptr, 0, nullptr, 1, &barrier);

                endCommandBuffer(vkd, **commandBuffers[i]);

                // Submit the command buffer
                VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                const VkSubmitInfo submitInfo  = {
                    VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1,           &acquireSem, &waitStage, 1u,
                    &**commandBuffers[i],          1u,      &presentSem,
                };
                VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, VK_NULL_HANDLE));
            }

            // If asked to release before present, do so now.
            const VkReleaseSwapchainImagesInfoEXT releaseInfo = {
                VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT,
                nullptr,
                *swapchain,
                (uint32_t)imageReleaseSize,
                releaseIndices.data(),
            };

            bool imagesReleased = false;
            if (testParams.releaseBeforePresent && imageReleaseSize > 0)
            {
                VK_CHECK(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
                imagesReleased = true;
            }

            // Present the frame
            if (doPresent)
            {
                const VkSwapchainPresentFenceInfoEXT presentFenceInfo = {
                    VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
                    nullptr,
                    1,
                    &**presentFence,
                };

                const VkPresentInfoKHR presentInfo = {
                    VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                    // Signal the present fence on the last present.
                    i + 1 == iterations ? &presentFenceInfo : nullptr,
                    1u,
                    &presentSem,
                    1,
                    &*swapchain,
                    &acquiredIndices[presentIndex],
                    &result,
                };
                VkResult aggregateResult = vkd.queuePresentKHR(devHelper.queue, &presentInfo);
                if (aggregateResult == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    // If OUT_OF_DATE is returned from present, recreate the swapchain and release images to the retired swapchain.
                    if (!imagesReleased && testParams.releaseBeforeRetire && imageReleaseSize > 0)
                    {
                        VK_CHECK(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
                        imagesReleased = true;
                    }

                    if (testParams.scaling == 0)
                    {
                        const VkSurfaceCapabilitiesKHR currentCapabilities = getPhysicalDeviceSurfaceCapabilities(
                            instHelper.vki, devHelper.physicalDevice, *surface, nullptr);
                        swapchainInfo.imageExtent = vk::makeExtent2D(currentCapabilities.minImageExtent.width,
                                                                     currentCapabilities.minImageExtent.height);
                    }

                    swapchainInfo.oldSwapchain = *swapchain;
                    Move<VkSwapchainKHR> newSwapchain(createSwapchainKHR(vkd, device, &swapchainInfo));

                    if (!imagesReleased && !testParams.releaseBeforeRetire && imageReleaseSize > 0)
                    {
                        // Release the images to the retired swapchain before deleting it (as part of move assignment below)
                        VK_CHECK(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
                        imagesReleased = true;
                    }

                    // Must have released old swapchain's images before destruction
                    DE_ASSERT(imagesReleased || imageReleaseSize == 0);
                    swapchain = std::move(newSwapchain);

                    const size_t previousImageCount = swapchainImages.size();
                    swapchainImages                 = getSwapchainImages(vkd, device, *swapchain);
                    if (previousImageCount != swapchainImages.size())
                        TCU_THROW(
                            InternalError,
                            "Unexpected change in number of swapchain images when recreated during window resize");
                }
                else
                {
                    VK_CHECK_WSI(result);
                    VK_CHECK_WSI(result);
                }
            }

            // If asked to release after present, do it now.
            if (!imagesReleased && imageReleaseSize > 0)
            {
                VK_CHECK_WSI(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
            }
        }

        // Wait for all presents before terminating the test (when semaphores are destroyed)
        VK_CHECK(vkd.waitForFences(device, 1u, &**presentFence, VK_TRUE, kMaxFenceWaitTimeout));
    }
    catch (...)
    {
        // Make sure device is idle before destroying resources
        vkd.deviceWaitIdle(device);
        throw;
    }

    native.windows[0]->setVisible(false);

    return tcu::TestStatus::pass("Tests ran successfully");
}

void populateReleaseImagesGroup(tcu::TestCaseGroup *testGroup, Type wsiType)
{
    const struct
    {
        VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
        {VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
        {VK_PRESENT_MODE_FIFO_KHR, "fifo"},
        {VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
        {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
        {VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    };

    const struct
    {
        VkPresentScalingFlagsEXT scaling;
        const char *name;
    } scalingFlags[] = {
        {0, "no_scaling"},
        {VK_PRESENT_SCALING_STRETCH_BIT_EXT, "stretch"},
    };

    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name));

        for (size_t scalingFlagNdx = 0; scalingFlagNdx < DE_LENGTH_OF_ARRAY(scalingFlags); scalingFlagNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> scalingFlagGroup(
                new tcu::TestCaseGroup(testGroup->getTestContext(), scalingFlags[scalingFlagNdx].name));

            ReleaseImagesTestConfig config;
            config.wsiType              = wsiType;
            config.mode                 = presentModes[presentModeNdx].mode;
            config.scaling              = scalingFlags[scalingFlagNdx].scaling;
            config.resizeWindow         = ResizeWindow::No;
            config.releaseBeforePresent = false;
            config.releaseBeforeRetire  = false;

            // Basic release acquired images test
            addFunctionCase(&*scalingFlagGroup, "basic", releaseImagesTest, config);

            config.releaseBeforePresent = true;
            // Basic release acquired images test where release happens before presenting an image
            addFunctionCase(&*scalingFlagGroup, "release_before_present", releaseImagesTest, config);

            config.releaseBeforePresent = false;
            config.resizeWindow         = ResizeWindow::BeforeAcquire;
            // Release acquired images after a window resize before acquire
            addFunctionCase(&*scalingFlagGroup, "resize_window", releaseImagesTest, config);

            config.resizeWindow = ResizeWindow::BeforePresent;
            // Release acquired images after a window resize after acquire
            addFunctionCase(&*scalingFlagGroup, "resize_window_after_acquire", releaseImagesTest, config);

            config.releaseBeforeRetire = true;
            // Release acquired images after a window resize after acquire, but release the images before retiring the swapchain
            addFunctionCase(&*scalingFlagGroup, "resize_window_after_acquire_release_before_retire", releaseImagesTest,
                            config);

            presentModeGroup->addChild(scalingFlagGroup.release());
        }

        testGroup->addChild(presentModeGroup.release());
    }
}

} // namespace

void createMaintenance1Tests(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    // Present fence
    addTestGroup(testGroup, "present_fence", populatePresentFenceGroup, wsiType);
    // Change present modes
    addTestGroup(testGroup, "present_modes", populatePresentModesGroup, wsiType);
    // Scaling and gravity
    addTestGroup(testGroup, "scaling", populateScalingGroup, wsiType);
    // Deferred allocation
    addTestGroup(testGroup, "deferred_alloc", populateDeferredAllocGroup, wsiType);
    // Release acquired images
    addTestGroup(testGroup, "release_images", populateReleaseImagesGroup, wsiType);
}

} // namespace wsi

} // namespace vkt
