/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 NVIDIA Corporation
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
 * \brief VK_EXT_present_timing extension tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiPresentTimingTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktNativeObjectsUtil.hpp"

#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"

#include "tcuPlatform.hpp"

#include "deStringUtil.hpp"
#include "deThread.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

#if (DE_OS == DE_OS_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef ABSOLUTE
#undef RELATIVE
#endif

namespace vkt
{
namespace wsi
{
namespace
{

using namespace vk;
using namespace vk::wsi;

enum class PresentAtMode
{
    NONE,
    ABSOLUTE,
    RELATIVE
};

constexpr uint32_t kDefaultRefreshCycleDurationNs = 16'000'000;
constexpr uint32_t kMaxQueryAttempts              = 100;
constexpr uint32_t kDefaultWindowWidth            = 128;
constexpr uint32_t kDefaultWindowHeight           = 128;
constexpr uint32_t kNumParallelThreads            = 4;
constexpr uint32_t kMaxPresentStageCount          = 4;
constexpr uint64_t kTargetTimeMarginNs            = 100'000;
constexpr uint64_t kCalibratedHostTimeMarginNs    = 10'000;
constexpr uint64_t kMinSleepTimeToDrainNs         = 5'000'000;
constexpr uint64_t kNsPerSec                      = 1'000'000'000;

constexpr vk::VkPresentStageFlagsEXT kAllPresentStages =
    VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT | VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT |
    VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT | VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT;

const tcu::UVec2 kDefaultWindowSize = {kDefaultWindowWidth, kDefaultWindowHeight};

typedef std::vector<vk::VkExtensionProperties> Extensions;
typedef de::SharedPtr<Unique<vk::VkCommandBuffer>> CommandBufferSp;
typedef de::SharedPtr<Unique<vk::VkFence>> FenceSp;
typedef de::SharedPtr<Unique<vk::VkSemaphore>> SemaphoreSp;

// An MSB function could be written to do this instead, however since there are only 4 bits to deal with, this is simpler
VkPresentStageFlagsEXT getLatestStageBit(VkPresentStageFlagsEXT mask)
{
    DE_ASSERT(mask);

    if (mask & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT)
    {
        return VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT;
    }
    if (mask & VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT)
    {
        return VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT;
    }
    if (mask & VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT)
    {
        return VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT;
    }
    if (mask & VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT)
    {
        return VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT;
    }

    // Unhandled stage bit
    DE_ASSERT(0);
    return 0;
}

template <typename T>
void checkAllSupported(const Extensions &supportedExtensions, const std::vector<T> &requiredExtensions)
{
    for (auto &requiredExtension : requiredExtensions)
    {
        if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(requiredExtension)))
            TCU_THROW(NotSupportedError, (std::string(requiredExtension) + " is not supported").c_str());
    }
}

// Wrapper class to help track the only acquired image for Shared Present modes
struct SwapchainAndImage
{
    SwapchainAndImage() = default;

    SwapchainAndImage(const DeviceInterface &vkd, vk::VkDevice device,
                      const vk::VkSwapchainCreateInfoKHR &swapchainInfo)
    {
        createSwapchain(vkd, device, swapchainInfo);
    }

    void createSwapchain(const DeviceInterface &vkd, vk::VkDevice device,
                         const vk::VkSwapchainCreateInfoKHR &swapchainInfo)
    {
        m_isSharedPresentMode = (swapchainInfo.presentMode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
                                 swapchainInfo.presentMode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR);
        m_swapchain           = createSwapchainKHR(vkd, device, &swapchainInfo);
    }

    vk::VkSwapchainKHR operator*()
    {
        return *m_swapchain;
    }

    Move<vk::VkSwapchainKHR> m_swapchain;
    bool m_isSharedPresentMode{};
    uint32_t m_sharedImageIndex{UINT32_MAX};
};

CustomInstance createInstanceWithWsi(Context &context, const Extensions &supportedExtensions, Type wsiType)
{
    std::vector<std::string> extensions;

    extensions.push_back("VK_KHR_surface");
    extensions.push_back(getExtensionName(wsiType));
    extensions.push_back("VK_KHR_get_surface_capabilities2");

    if (isDisplaySurface(wsiType))
        extensions.push_back("VK_KHR_display");

    checkAllSupported(supportedExtensions, extensions);

    return createCustomInstanceWithExtensions(context, extensions);
}

vk::VkPhysicalDeviceFeatures getDeviceNullFeatures(void)
{
    vk::VkPhysicalDeviceFeatures features;
    deMemset(&features, 0, sizeof(features));
    return features;
}

Move<vk::VkDevice> createDeviceWithWsi(const vk::PlatformInterface &vkp, vk::VkInstance instance,
                                       const InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                       const Extensions &supportedExtensions, const uint32_t queueFamilyIndex,
                                       PresentAtMode presentAtMethod, bool validationEnabled,
                                       const vk::VkAllocationCallbacks *pAllocator = nullptr)
{
    const float queuePriorities[]                  = {1.0f};
    const vk::VkDeviceQueueCreateInfo queueInfos[] = {{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr,
        (vk::VkDeviceQueueCreateFlags)0,
        queueFamilyIndex,
        DE_LENGTH_OF_ARRAY(queuePriorities),
        &queuePriorities[0],
    }};
    const vk::VkPhysicalDeviceFeatures features    = getDeviceNullFeatures();
    std::vector<const char *> extensions;

    extensions.push_back("VK_KHR_swapchain");
    extensions.push_back("VK_KHR_present_id2");
    extensions.push_back("VK_KHR_calibrated_timestamps");
    extensions.push_back("VK_EXT_present_timing");

    if (isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_KHR_shared_presentable_image")))
        extensions.push_back("VK_KHR_shared_presentable_image");

    if (isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_EXT_present_mode_fifo_latest_ready")))
        extensions.push_back("VK_EXT_present_mode_fifo_latest_ready");

    checkAllSupported(supportedExtensions, extensions);

    vk::VkPhysicalDevicePresentId2FeaturesKHR presentId2Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR,
        nullptr, // void* pNext;
        VK_TRUE  // VkBool32 presentId2
    };

    vk::VkPhysicalDevicePresentTimingFeaturesEXT presentTimingFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_TIMING_FEATURES_EXT,
        &presentId2Features,                                               // void* pNext;
        VK_TRUE,                                                           // VkBool32 presentTiming;
        (presentAtMethod == PresentAtMode::ABSOLUTE) ? VK_TRUE : VK_FALSE, // VkBool32 presentAtAbsoluteTime;
        (presentAtMethod == PresentAtMode::RELATIVE) ? VK_TRUE : VK_FALSE  // VkBool32 presentAtRelativeTime;
    };

    vk::VkPhysicalDeviceFeatures2 features2 = initVulkanStructure(&presentTimingFeatures);
    features2.features                      = features;

    vk::VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        &features2,
        (vk::VkDeviceCreateFlags)0,
        DE_LENGTH_OF_ARRAY(queueInfos),
        &queueInfos[0],
        0u,
        nullptr,
        (uint32_t)extensions.size(),
        extensions.empty() ? nullptr : &extensions[0],
        nullptr,
    };

    return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

vk::VkPresentTimingSurfaceCapabilitiesEXT getSurfacePresentTimingCapabilities(const InstanceInterface &vki,
                                                                              vk::VkPhysicalDevice physicalDevice,
                                                                              vk::VkSurfaceKHR surface)
{
    const vk::VkPhysicalDeviceSurfaceInfo2KHR info = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        nullptr,
        surface,
    };

    vk::VkPresentTimingSurfaceCapabilitiesEXT presentTimingCaps{
        VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT,
        nullptr, // void *pNext;
        false,   // VkBool32 presentTimingSupported;
        false,   // VkBool32 presentAtAbsoluteTimeSupported;
        false,   // VkBool32 presentAtRelativeTimeSupported;
        0,       // VkPresentStageFlagsEXT presentStageQueries;
    };

    vk::VkSurfaceCapabilitiesPresentId2KHR presentId2Caps{
        VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR,
        &presentTimingCaps, // void *pNext
        false               // VkBool32 presentId2Supported;
    };

    vk::VkSurfaceCapabilities2KHR capabilities{VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, &presentId2Caps,
                                               VkSurfaceCapabilitiesKHR()};

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

    if (!presentId2Caps.presentId2Supported)
        TCU_THROW(NotSupportedError, "VK_KHR_present_id2 not supported by surface");

    return presentTimingCaps;
}

vk::VkSwapchainCreateInfoKHR getBasicSwapchainParameters(vk::wsi::Type wsiType, const InstanceInterface &vki,
                                                         vk::VkPhysicalDevice physicalDevice, vk::VkSurfaceKHR surface,
                                                         const tcu::UVec2 &desiredSize,
                                                         vk::VkPresentModeKHR presentMode, uint32_t desiredImageCount)
{
    const vk::VkSurfaceCapabilitiesKHR capabilities =
        vk::wsi::getPhysicalDeviceSurfaceCapabilities(vki, physicalDevice, surface);
    const std::vector<vk::VkSurfaceFormatKHR> formats =
        vk::wsi::getPhysicalDeviceSurfaceFormats(vki, physicalDevice, surface);
    const vk::wsi::PlatformProperties &platformProperties = vk::wsi::getPlatformProperties(wsiType);
    const vk::VkSwapchainCreateInfoKHR parameters         = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT | VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR,
        surface,
        desiredImageCount,
        formats[0].format,
        formats[0].colorSpace,
        (platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE ?
                     capabilities.currentExtent :
                     makeExtent2D(desiredSize.x(), desiredSize.y())),
        1u,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        presentMode,
        VK_FALSE,
        VK_NULL_HANDLE,
    };

    return parameters;
}

std::vector<SemaphoreSp> allocateSemaphores(const DeviceInterface &vkd, const vk::VkDevice device,
                                            const size_t numSemaphores)
{
    std::vector<SemaphoreSp> semaphores(numSemaphores);

    vk::VkSemaphoreCreateInfo semaphoreCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        nullptr,
        0,
    };

    for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
        semaphores[ndx] = SemaphoreSp(new Unique<vk::VkSemaphore>(createSemaphore(vkd, device, &semaphoreCreateInfo)));

    return semaphores;
}

std::vector<CommandBufferSp> allocateCommandBuffers(const DeviceInterface &vkd, const vk::VkDevice device,
                                                    const vk::VkCommandPool commandPool,
                                                    const vk::VkCommandBufferLevel level,
                                                    const size_t numCommandBuffers)
{
    std::vector<CommandBufferSp> buffers(numCommandBuffers);

    for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
        buffers[ndx] =
            CommandBufferSp(new Unique<vk::VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

    return buffers;
}

void transitionImage(const DeviceInterface &vkd, vk::VkCommandBuffer cmdbuf, vk::VkImage image,
                     vk::VkImageLayout oldLayout, vk::VkImageLayout newLayout, vk::VkAccessFlags srcAccessMask,
                     vk::VkAccessFlags dstAccessMask, vk::VkPipelineStageFlags srcStageMask,
                     vk::VkPipelineStageFlags dstStageMask)
{
    vk::VkImageSubresourceRange subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT, // aspectMask
        0,                         // baseMipLevel
        1,                         // levelCount
        0,                         // baseArrayLayer
        1                          // layerCount
    };

    vk::VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // sType
        nullptr,                                // pNext
        srcAccessMask,                          // srcAccessMask
        dstAccessMask,                          // dstAccessMask
        oldLayout,                              // oldLayout
        newLayout,                              // newLayout
        VK_QUEUE_FAMILY_IGNORED,                // srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // dstQueueFamilyIndex
        image,                                  // image
        subresourceRange                        // subresourceRange
    };

    vkd.cmdPipelineBarrier(cmdbuf, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

struct PresentTimingTestConfig
{
    vk::wsi::Type wsiType;
    PresentAtMode presentAtMode;
    vk::VkPresentModeKHR presentMode;
    vk::VkPresentStageFlagsEXT presentStageQueries;
    vk::VkTimeDomainKHR timeDomain;
    vk::VkBool32 allowOutOfOrder;
    vk::VkBool32 allowPartial;
    vk::VkBool32 presentAtNearestRefreshCycle;
};

struct CalibrationTestConfig
{
    vk::wsi::Type wsiType;
    vk::VkTimeDomainKHR timeDomain;
};

struct PresentResult
{
    uint64_t presentId;
    uint64_t timeDomainId;
    vk::VkPresentStageFlagsEXT stages;
    uint32_t stageCount;
    std::map<VkPresentStageFlagsEXT, uint64_t> times;
};

struct InstanceHelper
{
    const std::vector<vk::VkExtensionProperties> supportedExtensions;
    const CustomInstance instance;
    const InstanceDriver &vki;

    InstanceHelper(Context &context, Type wsiType)
        : supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr))
        , instance(createInstanceWithWsi(context, supportedExtensions, wsiType))
        , vki(instance.getDriver())
    {
    }
};

struct DeviceHelper
{
    const vk::VkPhysicalDevice physicalDevice;
    const uint32_t queueFamilyIndex;
    const Unique<vk::VkDevice> device;
    const DeviceDriver vkd;
    const vk::VkQueue queue;

    DeviceHelper(Context &context, const InstanceInterface &vki, vk::VkInstance instance, vk::VkSurfaceKHR surface,
                 PresentAtMode presentAtMethod, const vk::VkAllocationCallbacks *pAllocator = nullptr)
        : physicalDevice(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
        , queueFamilyIndex(chooseQueueFamilyIndex(vki, physicalDevice, surface))
        , device(createDeviceWithWsi(context.getPlatformInterface(), instance, vki, physicalDevice,
                                     enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr), queueFamilyIndex,
                                     presentAtMethod, context.getTestContext().getCommandLine().isValidationEnabled(),
                                     pAllocator))
        , vkd(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion(),
              context.getTestContext().getCommandLine())
        , queue(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
    {
    }
};

struct PresentTimingHelper
{
    std::vector<PresentResult> results;
    std::vector<vk::VkPastPresentationTimingEXT> timings;
    std::vector<vk::VkPresentStageTimeEXT> stageTimes;
    uint32_t stageCount{};
    uint32_t queueSize{};
    uint64_t timingPropertiesCounter{};
    uint64_t timeDomainsCounter{};
    uint64_t refreshCycleDuration{kDefaultRefreshCycleDurationNs};
    VkPastPresentationTimingFlagsEXT pastPresentationTimingFlags{};

    PresentTimingHelper(const PresentTimingHelper &)            = delete;
    PresentTimingHelper &operator=(const PresentTimingHelper &) = delete;
    PresentTimingHelper(PresentTimingHelper &&)                 = delete;
    PresentTimingHelper &operator=(PresentTimingHelper &&)      = delete;

    PresentTimingHelper() = default;
    PresentTimingHelper(uint32_t maxQueueSize, size_t maxPresentStageCount, uint64_t timeDomainsCounter_)
    {
        init(maxQueueSize, maxPresentStageCount, timeDomainsCounter_);
    }

    void init(uint32_t maxQueueSize, size_t maxPresentStageCount, uint64_t timeDomainsCounter_)
    {
        DE_ASSERT(timings.empty());

        stageCount         = static_cast<uint32_t>(maxPresentStageCount);
        queueSize          = maxQueueSize;
        timeDomainsCounter = timeDomainsCounter_;

        timings.resize(queueSize);
        stageTimes.resize(queueSize * stageCount);

        for (uint32_t i = 0; i < timings.size(); i++)
        {
            timings[i].sType          = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT;
            timings[i].pNext          = nullptr;
            timings[i].pPresentStages = stageTimes.data() + i * stageCount;
        }
    }

    void sortResults()
    {
        std::sort(results.begin(), results.end(),
                  [](const PresentResult &a, const PresentResult &b) { return a.presentId < b.presentId; });
    }
};

class FrameStreamObjects
{
public:
    struct FrameObjects
    {
        const vk::VkFence &acquireFence;
        const vk::VkSemaphore &renderSemaphore;
        const vk::VkCommandBuffer &commandBuffer;
    };

    FrameStreamObjects(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkCommandPool cmdPool,
                       size_t maxQueuedFrames)
    {
        m_maxQueuedFrames = maxQueuedFrames;

        const vk::VkFenceCreateInfo fenceCreateInfo = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            nullptr,
            VK_FENCE_CREATE_SIGNALED_BIT,
        };
        m_acquireFence     = FenceSp(new Unique<vk::VkFence>(createFence(vkd, device, &fenceCreateInfo)));
        m_renderSemaphores = allocateSemaphores(vkd, device, maxQueuedFrames);
        m_commandBuffers =
            allocateCommandBuffers(vkd, device, cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames);
    }

    FrameObjects newFrame()
    {
        const size_t idx = m_nextFrame % m_maxQueuedFrames;
        FrameObjects ret = {
            **m_acquireFence,
            **m_renderSemaphores[idx],
            **m_commandBuffers[idx],
        };
        ++m_nextFrame;
        return ret;
    }

private:
    FenceSp m_acquireFence;
    std::vector<SemaphoreSp> m_renderSemaphores;
    std::vector<CommandBufferSp> m_commandBuffers;
    size_t m_maxQueuedFrames{};
    size_t m_nextFrame{};
};

struct TimeDomainHelper
{
    vk::VkDevice device;
    vk::VkSwapchainKHR swapchain;
    std::vector<vk::VkTimeDomainKHR> timeDomains{};
    std::vector<uint64_t> timeDomainIds{};
    uint64_t timeDomainsCounter{};

    std::map<vk::VkTimeDomainKHR, uint64_t> mapDomainToId{};

    TimeDomainHelper(const vk::DeviceInterface &vkd, vk::VkDevice dev, vk::VkSwapchainKHR swap)
        : device(dev)
        , swapchain(swap)
    {
        VK_CHECK(clearAndFetchProperties(vkd));
    }

    vk::VkResult fetchProperties(const vk::DeviceInterface &vkd)
    {
        vk::VkSwapchainTimeDomainPropertiesEXT timeDomainProps = {
            VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT,
            nullptr, // void* pNext;
            0,       // uint32_t timeDomainCount;
            nullptr, // VkTimeDomainKHR *pTimeDomains;
            nullptr, // uint64_t *pTimeDomainIds;
        };

        timeDomainProps.timeDomainCount = static_cast<uint32_t>(timeDomains.size());
        timeDomainProps.pTimeDomains    = timeDomains.data();
        timeDomainProps.pTimeDomainIds  = timeDomainIds.data();

        vk::VkResult result =
            vkd.getSwapchainTimeDomainPropertiesEXT(device, swapchain, &timeDomainProps, &timeDomainsCounter);

        // Create a map of ID to Domain for quick lookups
        mapDomainToId.clear();
        for (uint32_t i = 0; i < timeDomainIds.size(); i++)
            mapDomainToId[timeDomains[i]] = timeDomainIds[i];

        return result;
    }

    vk::VkResult clearAndFetchProperties(const vk::DeviceInterface &vkd)
    {
        VkResult result = VK_SUCCESS;

        timeDomains.clear();
        timeDomainIds.clear();
        mapDomainToId.clear();

        const uint32_t maxTries = 10;
        for (uint32_t i = 0; i < maxTries; i++)
        {
            vk::VkSwapchainTimeDomainPropertiesEXT timeDomainProps = {
                VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT,
                nullptr, // void *pNext;
                0,       // uint32_t timeDomainCount;
                0,       // VkTimeDomainKHR *pTimeDomains;
                nullptr, // uint64_t *pTimeDomainIds;
            };
            VK_CHECK(vkd.getSwapchainTimeDomainPropertiesEXT(device, swapchain, &timeDomainProps, nullptr));

            timeDomains.resize(timeDomainProps.timeDomainCount);
            timeDomainIds.resize(timeDomainProps.timeDomainCount);

            timeDomainProps.pTimeDomains   = timeDomains.data();
            timeDomainProps.pTimeDomainIds = timeDomainIds.data();

            // Retry if the number of properties available has grown since the size query
            result = fetchProperties(vkd);
            if (result == VK_INCOMPLETE)
                continue;
        }

        return result;
    }

    uint64_t getSwapchainTimeDomainId(vk::VkTimeDomainKHR desiredTimeDomain)
    {
        const auto it = mapDomainToId.find(desiredTimeDomain);
        if (it == mapDomainToId.end())
            return UINT64_MAX;

        return mapDomainToId[desiredTimeDomain];
    }

    bool hasUniqueIds()
    {
        std::set<uint64_t> uniqueTimeDomainIds(timeDomainIds.begin(), timeDomainIds.end());
        return (uniqueTimeDomainIds.size() == timeDomainIds.size());
    }

    bool compare(const std::vector<vk::VkTimeDomainKHR> &cmpTimeDomains, const std::vector<uint64_t> &cmpTimeDomainIds)
    {
        DE_ASSERT(cmpTimeDomains.size() == cmpTimeDomainIds.size());

        if (cmpTimeDomains.size() != timeDomains.size())
            return false;

        for (uint32_t i = 0; i < cmpTimeDomains.size(); i++)
        {
            uint64_t timeDomainId = getSwapchainTimeDomainId(cmpTimeDomains[i]);
            if (timeDomainId == UINT64_MAX)
                return false;

            if (timeDomainId != cmpTimeDomainIds[i])
                return false;
        }
        return true;
    }
};

struct CalibratedTimestampHelper
{
    vk::VkDevice m_device;
    uint64_t m_freq;

    struct Timestamp
    {
        uint64_t host{};
        uint64_t swapchain{};
        std::map<VkPresentStageFlagsEXT, uint64_t> presentStages{};

        uint64_t deviation{};
    };

    // Processed results, per frame
    std::vector<Timestamp> timestamps{};

    CalibratedTimestampHelper(vk::VkDevice dev, uint64_t freq) : m_device(dev), m_freq(freq)
    {
    }

    uint64_t convertHostTimestampToNs(uint64_t hostTimestamp, uint64_t freq)
    {
#if (DE_OS == DE_OS_WIN32)
        uint64_t secs = hostTimestamp / freq;
        uint64_t ns   = ((hostTimestamp % freq) * kNsPerSec) / freq;

        return ((secs * kNsPerSec) + ns);
#else
        (void)freq;
        (void)kNsPerSec;
        return hostTimestamp;
#endif
    }

    void getCalibratedTimestamps(const DeviceInterface &vkd, const std::vector<VkTimeDomainKHR> &domains,
                                 const std::vector<VkSwapchainCalibratedTimestampInfoEXT> &swapchainCalibratedTimeInfos)
    {
        std::vector<VkCalibratedTimestampInfoKHR> infos;

        for (auto domain : domains)
        {
            VkCalibratedTimestampInfoKHR info;
            info.sType      = getStructureType<VkCalibratedTimestampInfoKHR>();
            info.pNext      = nullptr;
            info.timeDomain = domain;
            if (domain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT && !swapchainCalibratedTimeInfos.empty())
            {
                for (const auto &swapchainCalibratedTimesInfo : swapchainCalibratedTimeInfos)
                {
                    info.pNext = &swapchainCalibratedTimesInfo;
                    infos.push_back(info);
                }
            }
            else
            {
                infos.push_back(info);
            }
        }

        std::vector<uint64_t> curTimestamps(infos.size());
        uint64_t deviation;

        VK_CHECK(vkd.getCalibratedTimestampsKHR(m_device, static_cast<uint32_t>(infos.size()), infos.data(),
                                                curTimestamps.data(), &deviation));

        timestamps.push_back({});
        timestamps.back().deviation = deviation;

        // Add timestamps to results
        uint32_t presentStageTimestampIdx = UINT32_MAX;
        for (size_t i = 0; i < infos.size(); ++i)
        {
            switch (infos[i].timeDomain)
            {
            case VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR:
            case VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR:
            case VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR:
                timestamps.back().host = convertHostTimestampToNs(curTimestamps[i], m_freq);
                break;

            case VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT:
                timestamps.back().swapchain = curTimestamps[i];
                break;

            case VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT:
                if (presentStageTimestampIdx == UINT32_MAX)
                {
                    presentStageTimestampIdx = static_cast<uint32_t>(i);
                }

                timestamps.back().presentStages.emplace(
                    swapchainCalibratedTimeInfos[i - presentStageTimestampIdx].presentStage, curTimestamps[i]);
                break;

            default:
                // Unused by test
                break;
            }
        }
    }
};

void updateSwapchainTimingProperties(const DeviceInterface &vkd, vk::VkDevice device, vk::VkSwapchainKHR swapchain,
                                     PresentTimingHelper &pth)
{
    vk::VkSwapchainTimingPropertiesEXT swapchainTimingProperties = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT,
        nullptr, // void *pNext;
        0,       // uint64_t refreshDuration;
        0,       // uint64_t refreshInterval;
    };

    VK_CHECK(vkd.getSwapchainTimingPropertiesEXT(device, swapchain, &swapchainTimingProperties,
                                                 &pth.timingPropertiesCounter));
    if (swapchainTimingProperties.refreshDuration == 0)
        pth.refreshCycleDuration = kDefaultRefreshCycleDurationNs;
    else
        pth.refreshCycleDuration = swapchainTimingProperties.refreshDuration;
}

vk::VkResult presentWithTimingInfo(const DeviceInterface &vkd, vk::VkQueue queue, vk::VkSemaphore waitSemaphore,
                                   vk::VkSwapchainKHR swapchain, uint32_t imageIndex,
                                   const vk::VkPresentTimingInfoEXT &timingInfo, const uint64_t presentId)
{
    vk::VkPresentTimingsInfoEXT presentTimingsInfo = {
        VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT,
        nullptr,
        1,
        &timingInfo,
    };

    vk::VkPresentId2KHR presentIdInfo = {VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR, nullptr, 1, &presentId};

    if (presentId != 0)
        presentTimingsInfo.pNext = &presentIdInfo;

    uint32_t waitSemaphoreCount      = waitSemaphore != VK_NULL_HANDLE ? 1u : 0u;
    vk::VkSemaphore *pWaitSemaphores = waitSemaphore != VK_NULL_HANDLE ? &waitSemaphore : nullptr;

    vk::VkPresentInfoKHR presentInfo = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // sType
        &presentTimingsInfo,                // pNext
        waitSemaphoreCount,                 // waitSemaphoreCount
        pWaitSemaphores,                    // pWaitSemaphores
        1,                                  // swapchainCount
        &swapchain,                         // pSwapchains
        &imageIndex,                        // pImageIndices
        nullptr                             // pResults
    };

    return vkd.queuePresentKHR(queue, &presentInfo);
}

void recordAndSubmitFrame(const DeviceInterface &vkd, vk::VkQueue queue, vk::VkCommandBuffer cmdbuf, vk::VkImage image,
                          vk::VkSemaphore renderSemaphore)
{
    beginCommandBuffer(vkd, cmdbuf, 0);

    transitionImage(vkd, cmdbuf, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

    const vk::VkClearColorValue clearValue             = {{1.0f, 0.0f, 0.0f, 1.0f}};
    const vk::VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkd.cmdClearColorImage(cmdbuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &subresourceRange);

    transitionImage(vkd, cmdbuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    endCommandBuffer(vkd, cmdbuf);

    uint32_t signalSemaphoreCount      = renderSemaphore != VK_NULL_HANDLE ? 1u : 0u;
    vk::VkSemaphore *pSignalSemaphores = renderSemaphore != VK_NULL_HANDLE ? &renderSemaphore : nullptr;

    const vk::VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,          0, nullptr, nullptr, 1, &cmdbuf,
        signalSemaphoreCount,          pSignalSemaphores};
    VK_CHECK(vkd.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
}

uint32_t acquireNextImage(const DeviceInterface &vkd, vk::VkDevice device, SwapchainAndImage &swapchain,
                          vk::VkFence acquireFence)
{
    if (swapchain.m_isSharedPresentMode && swapchain.m_sharedImageIndex != UINT32_MAX)
        return swapchain.m_sharedImageIndex;

    uint32_t imageIndex;
    VK_CHECK(vkd.resetFences(device, 1u, &acquireFence));
    VK_CHECK_WSI(vkd.acquireNextImageKHR(device, *swapchain, UINT64_MAX, VK_NULL_HANDLE, acquireFence, &imageIndex));
    VK_CHECK(vkd.waitForFences(device, 1u, &acquireFence, VK_TRUE, UINT64_MAX));

    if (swapchain.m_isSharedPresentMode)
        swapchain.m_sharedImageIndex = imageIndex;

    return imageIndex;
}

bool isPresentModeSupported(const InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice, vk::VkSurfaceKHR surface,
                            vk::VkPresentModeKHR presentMode)
{
    bool presentModeSupported = false;
    std::vector<vk::VkPresentModeKHR> const supportedPresentModes =
        vk::wsi::getPhysicalDeviceSurfacePresentModes(vki, physicalDevice, surface);
    for (auto const &supportedPresentMode : supportedPresentModes)
    {
        if (presentMode == supportedPresentMode)
        {
            presentModeSupported = true;
            break;
        }
    }

    return presentModeSupported;
}

uint32_t getPastPresentationTiming(const DeviceInterface &vkd, vk::VkDevice device, vk::VkSwapchainKHR swapchain,
                                   PresentTimingHelper &pth)
{
    vk::VkPastPresentationTimingInfoEXT pastTimingInfo = {VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT, nullptr,
                                                          pth.pastPresentationTimingFlags, swapchain};

    vk::VkPastPresentationTimingPropertiesEXT pastTimingProperties = {
        VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT,
        nullptr,
        pth.timingPropertiesCounter, // timingPropertiesCounter
        pth.timeDomainsCounter,      // timeDomainsCounter
        pth.queueSize,               // presentationTimingCount
        pth.timings.data()           // pPresentationTimings
    };

    VkResult vkResult = vkd.getPastPresentationTimingEXT(device, &pastTimingInfo, &pastTimingProperties);
    if (vkResult != VK_INCOMPLETE)
    {
        VK_CHECK(vkResult);
    }

    if (pastTimingProperties.timingPropertiesCounter != pth.timingPropertiesCounter)
        updateSwapchainTimingProperties(vkd, device, swapchain, pth);

    uint32_t resultCount = 0;

    for (uint32_t i = 0; i < pastTimingProperties.presentationTimingCount; i++)
    {
        if (pth.timings[i].presentStageCount < 1)
            TCU_FAIL("Unexpected present stage count");

        if (pth.timings[i].reportComplete)
        {
            if (!(pth.pastPresentationTimingFlags & VK_PAST_PRESENTATION_TIMING_ALLOW_PARTIAL_RESULTS_BIT_EXT) &&
                !pth.timings[i].reportComplete)
                TCU_FAIL("Received partial result when disallowed");

            PresentResult result;

            if (pth.timings[i].presentStageCount != pth.stageCount)
                TCU_FAIL("Unexpected present stage count");

            result.presentId    = pth.timings[i].presentId;
            result.timeDomainId = pth.timings[i].timeDomainId;
            result.stageCount   = pth.timings[i].presentStageCount;
            result.stages       = 0;
            for (uint32_t j = 0; j < result.stageCount && j < kMaxPresentStageCount; j++)
            {
                result.times.emplace(pth.timings[i].pPresentStages[j].stage, pth.timings[i].pPresentStages[j].time);
                result.stages |= pth.timings[i].pPresentStages[j].stage;
            }

            pth.results.push_back(result);
            resultCount++;
        }
    }

    return resultCount;
}

uint32_t drainPresentationTimingResults(const DeviceInterface &vkd, vk::VkDevice device, vk::VkSwapchainKHR swapchain,
                                        PresentTimingHelper &pth, uint32_t minCount)
{
    uint32_t resultCount = 0;

    for (uint32_t attempt = 0; attempt < kMaxQueryAttempts; attempt++)
    {
        resultCount += getPastPresentationTiming(vkd, device, swapchain, pth);

        if (resultCount < minCount)
        {
            const uint64_t sleepNs = std::max(pth.refreshCycleDuration, kMinSleepTimeToDrainNs);
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleepNs));
        }
        else
        {
            break;
        }
    }

    return resultCount;
}

tcu::TestStatus surfaceCapabilitiesTest(Context &context, Type wsiType)
{
    const InstanceHelper instHelper(context, wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(kDefaultWindowSize));
    Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(),
                                                   native.getWindow(), context.getTestContext().getCommandLine()));
    vk::VkPresentTimingSurfaceCapabilitiesEXT caps = getSurfacePresentTimingCapabilities(
        instHelper.vki, chooseDevice(instHelper.vki, instHelper.instance, context.getTestContext().getCommandLine()),
        *surface);

    if (caps.presentTimingSupported && (caps.presentStageQueries & VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT) == 0)
        TCU_FAIL("VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT must be supported if presentTimingSupported is true");

    return tcu::TestStatus::pass("Tests ran successfully");
}

void showWindow(const NativeObjects &native, vk::wsi::Type wsiType)
{
    if (wsiType == TYPE_HEADLESS)
        return;

    try
    {
        native.getWindow().setVisible(true);
        if (wsiType == TYPE_WIN32)
        {
            native.getWindow().setForeground();
        }
    }
    catch (const tcu::InternalError &e)
    {
        // Convert errors thrown by the Window class into a warning, since this might not affect the test on some platforms
        TCU_THROW(QualityWarning, e.getMessage());
    }
}

tcu::TestStatus timingQueueTest(Context &context, vk::wsi::Type wsiType)
{
    const InstanceHelper instHelper(context, wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(kDefaultWindowSize));
    Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(),
                                                   native.getWindow(), context.getTestContext().getCommandLine()));
    DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, PresentAtMode::NONE);
    const DeviceInterface &vkd = devHelper.vkd;
    const vk::VkDevice device  = *devHelper.device;

    vk::VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(
        wsiType, instHelper.vki, devHelper.physicalDevice, *surface, kDefaultWindowSize, VK_PRESENT_MODE_FIFO_KHR, 2);

    SwapchainAndImage swapchain(vkd, device, swapchainInfo);

    vk::VkResult result;
    const uint32_t minQueueSize = 2;
    const uint32_t maxQueueSize = 4;

    // Initial allocation
    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, minQueueSize));

    // Grow queue size
    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, maxQueueSize));

    // Shrink queue size
    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, minQueueSize));

    // Shrink queue size back to 0
    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, 0));

    // Grow queue size back to maxQueueSize and start filling it up
    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, maxQueueSize));

    const Unique<vk::VkCommandPool> commandPool(createCommandPool(vkd, device, 0, devHelper.queueFamilyIndex));
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, maxQueueSize);
    const std::vector<vk::VkImage> images = getSwapchainImages(vkd, device, *swapchain);

    TimeDomainHelper timeDomainsHelper(vkd, device, *swapchain);
    const uint64_t timeDomainId = timeDomainsHelper.getSwapchainTimeDomainId(VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT);
    if (timeDomainId == UINT64_MAX)
        TCU_FAIL("VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT not supported");

    showWindow(native, wsiType);

    vk::VkPresentTimingInfoEXT timingInfo = {
        VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT,
        nullptr,                                       // pNext
        0,                                             // flags
        0,                                             // targetTime
        timeDomainId,                                  // timeDomainId
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT, // presentStageQueries
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT  // targetTimeDomainPresentStage
    };

    for (uint32_t i = 0; i < maxQueueSize; i++)
    {
        uint32_t imageIndex;
        FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();

        imageIndex = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

        recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, images[imageIndex], frame.renderSemaphore);

        VK_CHECK_WSI(
            presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex, timingInfo, 0));
    }

    // Present queue is now full. Present one additional time to check for VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT
    uint32_t imageIndex;
    imageIndex = acquireNextImage(vkd, device, swapchain, frameStreamObjects.newFrame().acquireFence);

    result = presentWithTimingInfo(vkd, devHelper.queue, VK_NULL_HANDLE, *swapchain, imageIndex, timingInfo, 0);
    if (result != VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT)
        TCU_FAIL("Failed to trigger VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT");

    // We should be able to present with an empty stage mask though.
    timingInfo.presentStageQueries = 0;
    VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, VK_NULL_HANDLE, *swapchain, imageIndex, timingInfo, 0));
    timingInfo.presentStageQueries = VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT;

    // Try shrinking queue size and check for VK_NOT_READY
    result = vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, 2);
    if (result != VK_NOT_READY)
        TCU_FAIL("Unexpected result from vkd.setSwapchainPresentTimingQueueSizeEXT");

    PresentTimingHelper pth(maxQueueSize, std::bitset<32>(timingInfo.presentStageQueries).count(),
                            timeDomainsHelper.timeDomainsCounter);

    // Retrieve at least 1 result to make space in the results queue.
    uint32_t resultsCount = drainPresentationTimingResults(vkd, device, *swapchain, pth, 1);
    if (resultsCount == 0)
        TCU_FAIL("Failed to retrieve all timing results");

    // Present again, it should work now.
    imageIndex = acquireNextImage(vkd, device, swapchain, frameStreamObjects.newFrame().acquireFence);
    VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, VK_NULL_HANDLE, *swapchain, imageIndex, timingInfo, 0));

    return tcu::TestStatus::pass("Tests ran successfully");
}

tcu::TestStatus timingTest(Context &context, PresentTimingTestConfig config)
{
    const InstanceHelper instHelper(context, config.wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, config.wsiType, 1u,
                               tcu::just(kDefaultWindowSize));
    Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, config.wsiType,
                                                   native.getDisplay(), native.getWindow(),
                                                   context.getTestContext().getCommandLine()));
    DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, PresentAtMode::NONE);
    const DeviceInterface &vkd = devHelper.vkd;
    const vk::VkDevice device  = *devHelper.device;

    // Check if the requested present mode and present stage are supported
    std::vector<vk::VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), config.presentMode) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    vk::VkPresentTimingSurfaceCapabilitiesEXT surfaceCaps =
        getSurfacePresentTimingCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);
    if ((surfaceCaps.presentStageQueries & config.presentStageQueries) == 0)
        TCU_THROW(NotSupportedError, "Present stage not supported for queries");

    vk::VkSwapchainCreateInfoKHR swapchainInfo =
        getBasicSwapchainParameters(config.wsiType, instHelper.vki, devHelper.physicalDevice, *surface,
                                    tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight), config.presentMode, 3);
    SwapchainAndImage swapchain(vkd, device, swapchainInfo);
    const std::vector<vk::VkImage> images = getSwapchainImages(vkd, device, *swapchain);
    const uint32_t imageCount             = static_cast<uint32_t>(images.size());

    // Set present timing queue size to 2x image count to give the presentation engine some time to report results
    const uint32_t presentQueueSize = imageCount * 2;
    vk::VkResult result             = vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, presentQueueSize);
    if (result != VK_SUCCESS)
        TCU_FAIL("Failed to set swapchain present timing queue size");

    TimeDomainHelper timeDomainsHelper(vkd, device, *swapchain);
    const uint64_t timeDomainId = timeDomainsHelper.getSwapchainTimeDomainId(config.timeDomain);
    if (timeDomainId == UINT64_MAX)
        TCU_THROW(NotSupportedError, "Requested time domain not supported");

    const Unique<vk::VkCommandPool> commandPool(createCommandPool(vkd, device, 0, devHelper.queueFamilyIndex));
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, presentQueueSize);
    const uint32_t frameCount       = 10;
    const uint64_t initialPresentId = 1;
    const uint64_t presentIdStep    = 3;
    uint64_t currentPresentId       = initialPresentId;
    uint32_t pendingResults         = 0;
    PresentTimingHelper pth(presentQueueSize, 1, timeDomainsHelper.timeDomainsCounter);
    updateSwapchainTimingProperties(vkd, device, *swapchain, pth);

    showWindow(native, config.wsiType);

    vk::VkPresentTimingInfoEXT timingInfo = {
        VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT,
        nullptr,                    // pNext
        0,                          // flags
        0,                          // targetTime
        timeDomainId,               // timeDomainId
        config.presentStageQueries, // presentStageQueries
        config.presentStageQueries  // targetTimeDomainPresentStage
    };

    for (uint32_t frameIdx = 0; frameIdx < frameCount; frameIdx++)
    {
        FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();

        uint32_t imageIndex = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

        recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, images[imageIndex], frame.renderSemaphore);

        VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex,
                                           timingInfo, currentPresentId));
        pendingResults++;

        uint32_t resultCount = getPastPresentationTiming(vkd, device, *swapchain, pth);
        pendingResults -= resultCount;

        if (pendingResults == presentQueueSize)
            pendingResults -= drainPresentationTimingResults(vkd, device, *swapchain, pth, 1);

        currentPresentId += presentIdStep;
    }

    if (drainPresentationTimingResults(vkd, device, *swapchain, pth, pendingResults) != pendingResults)
        TCU_FAIL("Failed to retrieve all timing results");

    pth.sortResults();

    // Verify the timing data report is complete for each result
    for (uint32_t i = 0; i < frameCount; i++)
    {
        uint64_t expectedPresentId = initialPresentId + i * presentIdStep;
        if (pth.results[i].presentId != expectedPresentId)
        {
            TCU_FAIL("Unexpected present ID");
        }

        if (i > 0 && pth.results[i].times.begin()->second != 0 && pth.results[i - 1].times.begin()->second != 0 &&
            pth.results[i].times.begin()->second < pth.results[i - 1].times.begin()->second)
        {
            TCU_FAIL("Times are not increasing");
        }

        if (pth.results[i].stageCount != 1)
            TCU_FAIL("Unexpected present stage count");

        if (pth.results[i].stages != config.presentStageQueries)
            TCU_FAIL("Unexpected present stage");
    }

    return tcu::TestStatus::pass("All tests ran successfully");
}

struct GetPastPresentTimingThreadSharedState
{
    GetPastPresentTimingThreadSharedState(const DeviceInterface &vkd, vk::VkDevice device, vk::VkSwapchainKHR swapchain,
                                          uint32_t presentQueueSize, size_t numSupportedPresentStages,
                                          uint64_t timeDomainsCounter, uint32_t frameCount)
        : m_vkd(vkd)
        , m_device(device)
        , m_swapchain(swapchain)
        , m_pendingResults(frameCount)
        , m_threadsException(m_pths.size())
    {
        for (uint32_t i = 0; i < m_pths.size(); i++)
            m_pths[i].init(presentQueueSize, numSupportedPresentStages, timeDomainsCounter);

        for (auto &pth : m_pths)
            updateSwapchainTimingProperties(m_vkd, m_device, m_swapchain, pth);
    }

    const DeviceInterface &m_vkd;
    vk::VkDevice m_device;
    vk::VkSwapchainKHR m_swapchain;

    std::atomic<bool> m_presentingDone;
    std::atomic<uint32_t> m_pendingResults;

    std::array<PresentTimingHelper, kNumParallelThreads> m_pths;
    std::vector<std::exception_ptr> m_threadsException;
};

class GetPastPresentTimingThread : public de::Thread
{
public:
    GetPastPresentTimingThread(uint32_t threadIdx, GetPastPresentTimingThreadSharedState &sharedState)
        : m_sharedState(sharedState)
        , m_exception(sharedState.m_threadsException[threadIdx])
        , m_pth(sharedState.m_pths[threadIdx])
    {
    }

    void run(void)
    {
        try
        {
            while (!m_sharedState.m_presentingDone.load())
            {
                uint32_t numResults = getPastPresentationTiming(m_sharedState.m_vkd, m_sharedState.m_device,
                                                                m_sharedState.m_swapchain, m_pth);
                if (m_sharedState.m_pendingResults < numResults)
                    TCU_FAIL("Retrieved more results than presented");

                m_sharedState.m_pendingResults -= numResults;
                deSleep(1);
            }
        }
        catch (...)
        {
            m_exception = std::current_exception();
        }
    }

private:
    GetPastPresentTimingThreadSharedState &m_sharedState;
    std::exception_ptr &m_exception;
    PresentTimingHelper &m_pth;
};

typedef de::SharedPtr<GetPastPresentTimingThread> GetPastPresentTimingThreadSp;

tcu::TestStatus timingTestWithBackgroundQueryThreads(Context &context, Type wsiType)
{
    const InstanceHelper instHelper(context, wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(kDefaultWindowSize));
    Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(),
                                                   native.getWindow(), context.getTestContext().getCommandLine()));
    DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, PresentAtMode::NONE);
    const DeviceInterface &vkd = devHelper.vkd;
    const vk::VkDevice device  = *devHelper.device;

    vk::VkPresentTimingSurfaceCapabilitiesEXT surfaceCaps =
        getSurfacePresentTimingCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);

    std::vector<vk::VkPresentModeKHR> presentModes =
        getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_FIFO_KHR) == presentModes.end())
        TCU_THROW(NotSupportedError, "Present mode not supported");

    vk::VkSwapchainCreateInfoKHR swapchainInfo =
        getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface,
                                    tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight), VK_PRESENT_MODE_FIFO_KHR, 3);
    SwapchainAndImage swapchain(vkd, device, swapchainInfo);
    const std::vector<vk::VkImage> images = getSwapchainImages(vkd, device, *swapchain);

    const uint32_t frameCount       = 10;
    const uint32_t presentQueueSize = frameCount;
    vk::VkResult result             = vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, presentQueueSize);
    if (result != VK_SUCCESS)
        TCU_FAIL("Failed to set swapchain present timing queue size");

    TimeDomainHelper timeDomainsHelper(vkd, device, *swapchain);
    const uint64_t timeDomainId = timeDomainsHelper.getSwapchainTimeDomainId(VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT);
    if (timeDomainId == UINT64_MAX)
        TCU_THROW(NotSupportedError, "Requested time domain not supported");

    const size_t numSupportedPresentStages = std::bitset<32>(surfaceCaps.presentStageQueries).count();
    GetPastPresentTimingThreadSharedState sharedState(vkd, device, *swapchain, presentQueueSize,
                                                      numSupportedPresentStages, timeDomainsHelper.timeDomainsCounter,
                                                      frameCount);

    vk::VkPresentTimingInfoEXT timingInfo = {
        VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT,
        nullptr,                                      // pNext
        0,                                            // flags
        0,                                            // targetTime
        timeDomainId,                                 // timeDomainId
        surfaceCaps.presentStageQueries,              // presentStageQueries
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT // targetTimeDomainPresentStage
    };

    showWindow(native, wsiType);

    const uint64_t initialPresentId = 1;
    const uint64_t presentIdStep    = 3;
    uint64_t currentPresentId       = initialPresentId;
    const Unique<vk::VkCommandPool> commandPool(createCommandPool(vkd, device, 0, devHelper.queueFamilyIndex));
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, presentQueueSize);

    // Launch several background threads for querying timing results
    std::vector<GetPastPresentTimingThreadSp> timingQueryThreads(kNumParallelThreads);
    for (uint32_t i = 0; i < kNumParallelThreads; ++i)
    {
        timingQueryThreads[i] = GetPastPresentTimingThreadSp(new GetPastPresentTimingThread(i, sharedState));
        timingQueryThreads[i]->start();
    }

    std::exception_ptr mainThreadException;
    try
    {
        // Present frames
        for (uint32_t frameIdx = 0; frameIdx < frameCount; frameIdx++)
        {
            FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();
            uint32_t imageIndex                    = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

            recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, images[imageIndex], frame.renderSemaphore);

            VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex,
                                               timingInfo, currentPresentId));

            currentPresentId += presentIdStep;
        }
    }
    catch (...)
    {
        // If an exception is thrown by the main thread, then we need to tear down the background threads, otherwise
        // the background threads' work will use resources while the main thread will be destroying them
        mainThreadException = std::current_exception();
    }

    // Signal threads to stop and wait for them to exit
    sharedState.m_presentingDone = true;
    for (uint32_t i = 0; i < timingQueryThreads.size(); ++i)
        timingQueryThreads[i]->join();

    // If any of the threads encountered an exception, rethrow one of them
    if (mainThreadException)
        std::rethrow_exception(mainThreadException);
    for (uint32_t i = 0; i < timingQueryThreads.size(); ++i)
    {
        if (sharedState.m_threadsException[i])
            std::rethrow_exception(sharedState.m_threadsException[i]);
    }
    timingQueryThreads.clear();

    // Drain any remaining results after presenting is done
    PresentTimingHelper &pth = sharedState.m_pths[0];
    if (drainPresentationTimingResults(vkd, device, *swapchain, pth, sharedState.m_pendingResults) !=
        sharedState.m_pendingResults)
        TCU_FAIL("Failed to retrieve all timing results");

    // Merge all the presenting time results into the first pth
    for (uint32_t i = 1; i < sharedState.m_pths.size(); ++i)
        pth.results.insert(pth.results.end(), sharedState.m_pths[i].results.begin(),
                           sharedState.m_pths[i].results.end());

    pth.sortResults();
    if (pth.results.size() != frameCount)
        TCU_FAIL("Did not receive correct number of results");

    // Verify the timing data report is complete for each result
    for (uint32_t i = 0; i < frameCount; i++)
    {
        uint64_t expectedPresentId = initialPresentId + i * presentIdStep;
        if (pth.results[i].presentId != expectedPresentId)
            TCU_FAIL("Unexpected present ID");

        if (i > 0 && pth.results[i].times.begin()->second != 0 && pth.results[i - 1].times.begin()->second != 0 &&
            pth.results[i].times.begin()->second < pth.results[i - 1].times.begin()->second)
            TCU_FAIL("Times are not increasing");

        if (pth.results[i].stageCount != numSupportedPresentStages)
            TCU_FAIL("Unexpected present stage count");

        if (pth.results[i].stages != surfaceCaps.presentStageQueries)
            TCU_FAIL("Unexpected present stage");
    }

    return tcu::TestStatus::pass("All tests ran successfully");
}

tcu::TestStatus retiredSwapchainTest(Context &context, vk::wsi::Type wsiType)
{
    const InstanceHelper instHelper(context, wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(kDefaultWindowSize));
    const Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, wsiType,
                                                         native.getDisplay(), native.getWindow(),
                                                         context.getTestContext().getCommandLine()));

    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, surface.get(), PresentAtMode::NONE);
    const DeviceInterface &vkd                 = devHelper.vkd;
    const vk::VkDevice device                  = *devHelper.device;
    vk::VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(
        wsiType, instHelper.vki, devHelper.physicalDevice, *surface, kDefaultWindowSize, VK_PRESENT_MODE_FIFO_KHR, 2);

    const vk::VkPresentTimingSurfaceCapabilitiesEXT surfaceCaps =
        getSurfacePresentTimingCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);

    // Verify support for running with the given test parameters
    if (!surfaceCaps.presentTimingSupported)
        TCU_THROW(NotSupportedError, "Present Timing is not supported");

    const Unique<vk::VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, 3);

    const vk::VkPresentTimingInfoEXT presentTimingInfo = {
        VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT,
        nullptr,                                      // pNext
        0,                                            // flags
        0,                                            // targetTime
        0,                                            // timeDomainId
        kAllPresentStages,                            // presentStageQueries
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT // targetTimeDomainPresentStage
    };

    showWindow(native, wsiType);

    const uint32_t frameCount = 10;
    std::array<SwapchainAndImage, 2> swapchains{};
    std::array<uint64_t, 2> timeDomainCounters{};
    for (uint32_t swapchainIdx = 0; swapchainIdx < 2; swapchainIdx++)
    {
        swapchains[swapchainIdx].createSwapchain(vkd, device, swapchainInfo);
        vk::VkSwapchainKHR swapchain = *swapchains[swapchainIdx];

        TimeDomainHelper timeDomainsHelper(vkd, device, swapchain);
        timeDomainCounters[swapchainIdx] = timeDomainsHelper.timeDomainsCounter;

        VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, swapchain, frameCount));

        const std::vector<vk::VkImage> images = vk::wsi::getSwapchainImages(vkd, device, swapchain);

        for (uint32_t frameIdx = 0; frameIdx < frameCount; frameIdx++)
        {
            FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();

            uint32_t imageIndex = acquireNextImage(vkd, device, swapchains[swapchainIdx], frame.acquireFence);

            recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, images[imageIndex], frame.renderSemaphore);
            VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, swapchain, imageIndex,
                                               presentTimingInfo, frameIdx + 1));
        }

        swapchainInfo.oldSwapchain = *swapchains[swapchainIdx];
    }

    // Query and verify Present Timing Data
    for (uint32_t swapchainIdx = 0; swapchainIdx < 2; swapchainIdx++)
    {
        PresentTimingHelper pth(frameCount, kMaxPresentStageCount, timeDomainCounters[swapchainIdx]);

        updateSwapchainTimingProperties(vkd, device, *swapchains[swapchainIdx], pth);

        uint32_t resultCount = drainPresentationTimingResults(vkd, device, *swapchains[swapchainIdx], pth, frameCount);

        if (resultCount != frameCount)
            TCU_FAIL("Received incorrect number of present timings");

        // Verify the timing data report is complete for each result
        for (const auto &result : pth.results)
        {
            if (result.stageCount == 0)
                TCU_FAIL("No present stages returned");
        }

        // Explicitly trigger the destruction of the swapchain
        swapchains[swapchainIdx].m_swapchain = {};
    }

    return tcu::TestStatus::pass("Tests ran successfully");
}

tcu::TestStatus presentAtTest(Context &context, PresentTimingTestConfig config)
{
    const InstanceHelper instHelper(context, config.wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, config.wsiType, 1u,
                               tcu::just(kDefaultWindowSize));
    const Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, config.wsiType,
                                                         native.getDisplay(), native.getWindow(),
                                                         context.getTestContext().getCommandLine()));

    const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, surface.get(), config.presentAtMode);
    const DeviceInterface &vkd = devHelper.vkd;
    const vk::VkDevice device  = *devHelper.device;
    SimpleAllocator allocator(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
    const vk::VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(
        config.wsiType, instHelper.vki, devHelper.physicalDevice, *surface, kDefaultWindowSize, config.presentMode, 2);
    const vk::VkPresentTimingSurfaceCapabilitiesEXT surfaceCaps =
        getSurfacePresentTimingCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);

    // Verify support for running with the given test parameters
    if (!surfaceCaps.presentTimingSupported)
        TCU_THROW(NotSupportedError, "Present Timing is not supported");
    if (config.presentAtMode == PresentAtMode::ABSOLUTE && !surfaceCaps.presentAtAbsoluteTimeSupported)
        TCU_THROW(NotSupportedError, "presentAtAbsoluteTime is not supported");
    if (config.presentAtMode == PresentAtMode::RELATIVE && !surfaceCaps.presentAtRelativeTimeSupported)
        TCU_THROW(NotSupportedError, "presentAtRelativeTime is not supported");
    if (!isPresentModeSupported(instHelper.vki, devHelper.physicalDevice, surface.get(), config.presentMode))
        TCU_THROW(NotSupportedError, "Present Mode not supported");

    const uint32_t frameCount = 10;
    SwapchainAndImage swapchain(vkd, device, swapchainInfo);

    TimeDomainHelper timeDomainsHelper(vkd, device, *swapchain);
    const uint64_t timeDomainId = timeDomainsHelper.getSwapchainTimeDomainId(config.timeDomain);
    if (timeDomainId == UINT64_MAX)
        TCU_THROW(NotSupportedError, "Time Domain not supported");

    PresentTimingHelper pth(frameCount, kMaxPresentStageCount, timeDomainsHelper.timeDomainsCounter);
    if (config.allowOutOfOrder)
        pth.pastPresentationTimingFlags |= VK_PAST_PRESENTATION_TIMING_ALLOW_OUT_OF_ORDER_RESULTS_BIT_EXT;
    if (config.allowPartial)
        pth.pastPresentationTimingFlags |= VK_PAST_PRESENTATION_TIMING_ALLOW_PARTIAL_RESULTS_BIT_EXT;

    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, frameCount));

    const std::vector<vk::VkImage> swapchainImages = vk::wsi::getSwapchainImages(vkd, device, *swapchain);
    const Unique<vk::VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

    const size_t maxQueuedFrames = swapchainImages.size() * 2;
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, maxQueuedFrames);

    VkPresentTimingInfoFlagsEXT presentAtInfoFlags{};
    if (config.presentAtMode == PresentAtMode::RELATIVE)
        presentAtInfoFlags |= VK_PRESENT_TIMING_INFO_PRESENT_AT_RELATIVE_TIME_BIT_EXT;
    if (config.presentAtNearestRefreshCycle)
        presentAtInfoFlags |= VK_PRESENT_TIMING_INFO_PRESENT_AT_NEAREST_REFRESH_CYCLE_BIT_EXT;

    VkPresentStageFlagsEXT targetTimeDomainPresentStage = config.timeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT ?
                                                              getLatestStageBit(surfaceCaps.presentStageQueries) :
                                                              0;
    vk::VkPresentTimingInfoEXT presentTimingInfo        = {
        VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT,
        nullptr,                     // pNext
        presentAtInfoFlags,          // flags
        0,                           // targetTime
        timeDomainId,                // timeDomainId
        kAllPresentStages,           // presentStageQueries
        targetTimeDomainPresentStage // targetTimeDomainPresentStage
    };

    uint64_t presentId       = 1;
    uint32_t pendingResults  = 0;
    PresentResult baseResult = {};

    showWindow(native, config.wsiType);

    if (config.presentAtMode == PresentAtMode::ABSOLUTE)
    {
        // Present frames until we have our first non-zero result
        for (; presentId < kMaxQueryAttempts && baseResult.presentId == 0; presentId++)
        {
            FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();
            const uint32_t imageIndex              = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

            recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, swapchainImages[imageIndex],
                                 frame.renderSemaphore);

            VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex,
                                               presentTimingInfo, presentId));
            pendingResults++;

            pth.results.clear();
            int32_t resultCount = getPastPresentationTiming(vkd, device, *swapchain, pth);

            if (resultCount == 0 && pendingResults == frameCount)
                resultCount = drainPresentationTimingResults(vkd, device, *swapchain, pth, 1);

            pendingResults -= resultCount;

            pth.sortResults();

            // Check for non-zero result, starting from the end
            for (int32_t i = resultCount - 1; i >= 0; i--)
            {
                if (pth.results[i].times.begin()->second != 0)
                {
                    baseResult = pth.results[i];
                    break;
                }
            }
        }

        if (presentId == kMaxQueryAttempts)
            TCU_THROW(TestError, "Failed to get base present timing info");
    }
    else
    {
        baseResult.presentId = presentId;
    }

    // Multiply refresh cycle by a factor so we don't always just target the next vblank
    const uint64_t refreshCycleDurationFactor = 2;

    auto calculateTargetPresentTime =
        [refreshCycleDurationFactor = refreshCycleDurationFactor](
            uint64_t basePresentId, uint64_t baseTime, uint64_t targetPresentId, uint64_t refreshCycleDuration)
    {
        const uint64_t step       = refreshCycleDurationFactor * refreshCycleDuration;
        const uint64_t targetTime = baseTime + step * (targetPresentId - basePresentId);
        return targetTime;
    };

    // Use the base result to build our test results
    const uint64_t basePresentId = baseResult.presentId;

    struct PresentAt
    {
        uint64_t presentId;
        uint64_t targetTime;
        uint64_t refreshCycleDuration;
    };
    std::vector<PresentAt> targetTimes(frameCount);

    const uint64_t skippedPresentId = presentId + 4;
    for (uint32_t frameIdx = 0; frameIdx < frameCount; frameIdx++)
    {
        FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();
        const uint32_t imageIndex              = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

        recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, swapchainImages[imageIndex],
                             frame.renderSemaphore);

        if (config.presentAtMode == PresentAtMode::ABSOLUTE)
            presentTimingInfo.targetTime = calculateTargetPresentTime(basePresentId, baseResult.times.begin()->second,
                                                                      presentId, pth.refreshCycleDuration);
        else
            presentTimingInfo.targetTime = pth.refreshCycleDuration * refreshCycleDurationFactor;

        targetTimes[frameIdx].presentId            = presentId;
        targetTimes[frameIdx].refreshCycleDuration = pth.refreshCycleDuration;
        targetTimes[frameIdx].targetTime           = presentTimingInfo.targetTime;

        // The spec allows performing PresentAt without requesting timing info, so do one present without
        // setting a stage, which should result in not receiving feedback on it
        if (presentId == skippedPresentId)
            presentTimingInfo.presentStageQueries = 0;
        VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex,
                                           presentTimingInfo, presentId));
        if (presentId != skippedPresentId)
            pendingResults++;
        presentTimingInfo.presentStageQueries = kAllPresentStages;
        presentId++;

        uint32_t resultCount = getPastPresentationTiming(vkd, device, *swapchain, pth);
        if (resultCount == 0 && pendingResults == frameCount)
            resultCount = drainPresentationTimingResults(vkd, device, *swapchain, pth, 1);

        pendingResults -= resultCount;
    }

    if (drainPresentationTimingResults(vkd, device, *swapchain, pth, pendingResults) != pendingResults)
        TCU_FAIL("Failed to retrieve all timing results");

    pth.sortResults();

    // We expect to receive one less result than the frameCount due to the skipped timing info request
    const uint32_t numExpectedTimingResults = (frameCount - 1);
    const uint32_t firstResultIdx           = static_cast<uint32_t>(pth.results.size()) - numExpectedTimingResults;

    // Check results
    for (uint32_t i = 0; i < numExpectedTimingResults; ++i)
    {
        const PresentResult &result = pth.results[firstResultIdx + i];

        // Verify all of the presentIds are returned, excluding the one from the 'skipped' frame which didn't request present timing info
        uint64_t expectedPresentId = pth.results[firstResultIdx].presentId + i;
        if (expectedPresentId >= skippedPresentId)
            expectedPresentId++;

        if (result.presentId != expectedPresentId)
            TCU_FAIL("Unexpected present id");

        VkPresentStageFlagsEXT presentStages = result.stages;
        while (presentStages)
        {
            const VkPresentStageFlagsEXT presentStage = presentStages & -static_cast<int32_t>(presentStages);
            presentStages &= ~presentStage;

            const uint64_t actualPresentTime = result.times.at(presentStage);
            if (actualPresentTime == 0)
                continue;

            if (i > 0)
            {
                const PresentResult &prevResult = pth.results[firstResultIdx + i - 1];
                const uint64_t prevPresentTime  = prevResult.times.at(presentStage);

                if (!config.allowOutOfOrder && prevPresentTime != 0 && actualPresentTime <= prevPresentTime)
                    TCU_FAIL("Frames presented out of order when disallowed");

                // Check that Present landed after the requested time, according to the VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT stage timing
                if (presentStage == VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT)
                {
                    const uint64_t requestedPresentTime = config.presentAtMode == PresentAtMode::ABSOLUTE ?
                                                              targetTimes[i].targetTime :
                                                              targetTimes[i].targetTime + prevPresentTime;
                    if (actualPresentTime < requestedPresentTime)
                    {
                        const uint64_t early = requestedPresentTime - actualPresentTime;
                        const uint64_t max   = config.presentAtNearestRefreshCycle ?
                                                   kTargetTimeMarginNs + targetTimes[i].refreshCycleDuration :
                                                   kTargetTimeMarginNs;
                        if (early >= max)
                            TCU_FAIL("Frame was presented earlier than expected");
                    }
                }
            }
        }
    }

    return tcu::TestStatus::pass("Tests ran successfully");
}

// Test time domain enumeration and properties
tcu::TestStatus timeDomainPropertiesTest(Context &context, Type wsiType)
{
    const InstanceHelper instHelper(context, wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(kDefaultWindowSize));
    Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(),
                                                   native.getWindow(), context.getTestContext().getCommandLine()));
    DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, PresentAtMode::NONE);
    const DeviceInterface &vkd = devHelper.vkd;
    const vk::VkDevice device  = *devHelper.device;

    vk::VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(
        wsiType, instHelper.vki, devHelper.physicalDevice, *surface, kDefaultWindowSize, VK_PRESENT_MODE_FIFO_KHR, 2);

    SwapchainAndImage swapchain(vkd, device, swapchainInfo);

    // Call vkGetSwapchainTimeDomainPropertiesEXT
    TimeDomainHelper timeDomainsHelper(vkd, device, *swapchain);

    // Validate time domain counter behavior
    const std::vector<vk::VkImage> swapchainImages = vk::wsi::getSwapchainImages(vkd, device, *swapchain);
    const Unique<vk::VkCommandPool> commandPool(
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

    const uint32_t maxQueuedFrames = 30;
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, maxQueuedFrames);

    const uint64_t timeDomainId = timeDomainsHelper.getSwapchainTimeDomainId(VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT);
    vk::VkPresentTimingInfoEXT timingInfo = {
        getStructureType<VkPresentTimingInfoEXT>(),
        nullptr,                                       // pNext
        0,                                             // flags
        0,                                             // targetTime
        timeDomainId,                                  // timeDomainId
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT, // presentStageQueries
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT  // targetTimeDomainPresentStage
    };

    VK_CHECK(vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, static_cast<uint32_t>(maxQueuedFrames)));
    PresentTimingHelper pth(maxQueuedFrames, 1U, static_cast<uint32_t>(timeDomainsHelper.timeDomainsCounter));
    updateSwapchainTimingProperties(vkd, device, *swapchain, pth);

    showWindow(native, wsiType);

    // Present a bunch of frames, query the time domain properties after each present, and verify returned values are valid if time domains have changed
    uint32_t handledResults = 0;
    for (uint32_t frameIdx = 0; frameIdx < maxQueuedFrames; frameIdx++)
    {
        // Verify VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT is always supported
        if (timeDomainsHelper.getSwapchainTimeDomainId(VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT) == UINT64_MAX)
            TCU_FAIL(
                "VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT not returned during time domain query despite being mandatory");

        // Test timeDomainId uniqueness within swapchain
        if (!timeDomainsHelper.hasUniqueIds())
            TCU_FAIL("Queried time domains contain non-unique IDs");

        // Cache previous values
        uint64_t prevTimeDomainsCounter                        = timeDomainsHelper.timeDomainsCounter;
        const std::vector<vk::VkTimeDomainKHR> prevTimeDomains = timeDomainsHelper.timeDomains;
        const std::vector<uint64_t> prevTimeDomainIds          = timeDomainsHelper.timeDomainIds;

        // Submit new work and present
        FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();
        const uint32_t imageIndex              = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

        recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, swapchainImages[imageIndex],
                             frame.renderSemaphore);

        const uint64_t presentId = frameIdx + 1;
        VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex,
                                           timingInfo, presentId));

        // Fetch new domain propterties and check the data returned is valid
        VkResult result = timeDomainsHelper.fetchProperties(vkd);
        if (result == VK_INCOMPLETE)
        {
            if (timeDomainsHelper.timeDomainsCounter <= prevTimeDomainsCounter)
                TCU_FAIL("Swapchain Time Domain Properties changed but timeDomainsCounter did not");

            result = timeDomainsHelper.clearAndFetchProperties(vkd);
        }
        VK_CHECK(result);

        if (timeDomainsHelper.timeDomainsCounter < prevTimeDomainsCounter)
            TCU_FAIL("timeDomainsCounter regressed");

        if (timeDomainsHelper.timeDomainsCounter == prevTimeDomainsCounter &&
            !timeDomainsHelper.compare(prevTimeDomains, prevTimeDomainIds))
            TCU_FAIL("Queried time domain data differs from previous despite timeDomainsCounter not having changed");

        // If results have returned, check if there is a different domainId than the one requested
        bool unknownTimeDomainId = false;
        uint32_t resultsCount    = getPastPresentationTiming(vkd, device, *swapchain, pth);
        for (uint32_t resultIdx = 0; resultIdx < resultsCount; resultIdx++)
        {
            if (pth.results[handledResults + resultIdx].timeDomainId != timingInfo.timeDomainId)
            {
                unknownTimeDomainId = true;
                break;
            }
        }
        handledResults += resultsCount;

        // If an unknown timeDomainId was returned in present timing info, confirm that time domains counter has changed,
        // drain the outstanding presents, and request subsequent presents with latest timeDomainId
        if (unknownTimeDomainId)
        {
            // Confirm that time domains counter has changed if an unknown time domain Id was received via present timing info
            VK_CHECK(timeDomainsHelper.clearAndFetchProperties(vkd));
            if (pth.timeDomainsCounter == timeDomainsHelper.timeDomainsCounter)
                TCU_FAIL(
                    "Present timing info reveived a new time domain, however time domain counter was never updated");

            const uint32_t outstandingPresents = frameIdx - handledResults;
            resultsCount = drainPresentationTimingResults(vkd, device, *swapchain, pth, outstandingPresents);
            if (resultsCount != outstandingPresents)
                TCU_FAIL("Failed to drain all remaining presents");

            // Use latest time domains in subsequent presents
            VK_CHECK(timeDomainsHelper.clearAndFetchProperties(vkd));
            timingInfo.timeDomainId =
                timeDomainsHelper.getSwapchainTimeDomainId(VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT);
            pth.timeDomainsCounter = timeDomainsHelper.timeDomainsCounter;
        }
    }

    return tcu::TestStatus::pass("Tests ran successfully");
}

// Test time domain calibration
tcu::TestStatus timeDomainCalibrationTest(Context &context, CalibrationTestConfig config)
{
    const InstanceHelper instHelper(context, config.wsiType);
    const NativeObjects native(context, instHelper.supportedExtensions, config.wsiType, 1u,
                               tcu::just(kDefaultWindowSize));
    Unique<vk::VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, config.wsiType,
                                                   native.getDisplay(), native.getWindow(),
                                                   context.getTestContext().getCommandLine()));
    DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, PresentAtMode::NONE);
    const DeviceInterface &vkd = devHelper.vkd;
    const vk::VkDevice device  = *devHelper.device;

    vk::VkSwapchainCreateInfoKHR swapchainInfo =
        getBasicSwapchainParameters(config.wsiType, instHelper.vki, devHelper.physicalDevice, *surface,
                                    tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight), VK_PRESENT_MODE_FIFO_KHR, 3);
    SwapchainAndImage swapchain(vkd, device, swapchainInfo);
    const std::vector<vk::VkImage> images = getSwapchainImages(vkd, device, *swapchain);

    const uint32_t frameCount = 3;
    const Unique<vk::VkCommandPool> commandPool(createCommandPool(vkd, device, 0, devHelper.queueFamilyIndex));
    FrameStreamObjects frameStreamObjects(vkd, device, *commandPool, frameCount);

    // Set present timing queue size to fit data for all the presents
    const uint32_t presentQueueSize = frameCount;
    vk::VkResult result             = vkd.setSwapchainPresentTimingQueueSizeEXT(device, *swapchain, presentQueueSize);
    if (result != VK_SUCCESS)
        TCU_FAIL("Failed to set swapchain present timing queue size");

    VkPresentStageFlagsEXT supportedPresentStageQueries{};
    if (config.timeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT)
    {
        vk::VkPresentTimingSurfaceCapabilitiesEXT surfaceCaps =
            getSurfacePresentTimingCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);
        if ((surfaceCaps.presentStageQueries & VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT) == 0)
            TCU_FAIL("VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT must be supported");

        supportedPresentStageQueries = surfaceCaps.presentStageQueries;
    }

    // Get calibreatable time domains.
    uint32_t domainCount;
    VK_CHECK(
        instHelper.vki.getPhysicalDeviceCalibrateableTimeDomainsKHR(devHelper.physicalDevice, &domainCount, nullptr));
    if (domainCount == 0)
        throw tcu::NotSupportedError("No calibrateable time domains found");

    std::vector<VkTimeDomainKHR> supportedDomains(domainCount);
    VK_CHECK(instHelper.vki.getPhysicalDeviceCalibrateableTimeDomainsKHR(devHelper.physicalDevice, &domainCount,
                                                                         supportedDomains.data()));
    if (std::find(supportedDomains.begin(), supportedDomains.end(), config.timeDomain) == supportedDomains.end())
        throw tcu::NotSupportedError("Time domain not calibrateable");

    std::vector<VkTimeDomainKHR> preferredHostDomains;
    uint64_t freq = 0;
#if (DE_OS == DE_OS_WIN32)
    LARGE_INTEGER qpcFreq{};
    if (!QueryPerformanceFrequency(&qpcFreq))
        TCU_THROW(ResourceError, "Unable to get clock frequency with QueryPerformanceFrequency");

    if (qpcFreq.QuadPart <= 0)
        TCU_THROW(ResourceError, "QueryPerformanceFrequency did not return a positive number");

    freq = static_cast<uint64_t>(qpcFreq.QuadPart);

    preferredHostDomains.push_back(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR);
#else
    preferredHostDomains.push_back(VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR);
    preferredHostDomains.push_back(VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR);
#endif

    // Populate domains with the test domain, and a host domain
    std::vector<VkTimeDomainKHR> domains(1, config.timeDomain);
    auto it = std::find_first_of(preferredHostDomains.begin(), preferredHostDomains.end(), supportedDomains.begin(),
                                 supportedDomains.end());
    if (it != preferredHostDomains.end())
    {
        domains.push_back(*it);
    }

    TimeDomainHelper timeDomainsHelper(vkd, device, *swapchain);
    const uint64_t timeDomainId = timeDomainsHelper.getSwapchainTimeDomainId(config.timeDomain);
    if (timeDomainId == UINT64_MAX)
        TCU_THROW(NotSupportedError, "Failed to query time domain ID");

    // With the swapchain domain, we'll still need a slot even though a specific present stage won't be queried
    const size_t numSupportedPresentStages =
        std::max<size_t>(1U, std::bitset<32>(supportedPresentStageQueries).count());
    PresentTimingHelper pth(presentQueueSize, numSupportedPresentStages, timeDomainsHelper.timeDomainsCounter);
    updateSwapchainTimingProperties(vkd, device, *swapchain, pth);

    std::vector<VkSwapchainCalibratedTimestampInfoEXT> swapchainCalibratedTimesInfos{};
    VkPresentStageFlagsEXT presentStageQueryMask = supportedPresentStageQueries;
    do
    {
        const VkPresentStageFlagsEXT presentStage =
            presentStageQueryMask & -static_cast<int32_t>(presentStageQueryMask);
        presentStageQueryMask &= ~presentStage;

        VkSwapchainCalibratedTimestampInfoEXT info = {
            getStructureType<VkSwapchainCalibratedTimestampInfoEXT>(),
            nullptr,      // pNext
            *swapchain,   // swapchain
            presentStage, // presentStage
            timeDomainId, // timeDomainId
        };

        swapchainCalibratedTimesInfos.push_back(info);
    } while (presentStageQueryMask);

    vk::VkPresentTimingInfoEXT timingInfo = {
        getStructureType<VkPresentTimingInfoEXT>(),
        nullptr,                                      // pNext
        0,                                            // flags
        0,                                            // targetTime
        timeDomainId,                                 // timeDomainId
        supportedPresentStageQueries,                 // presentStageQueries
        VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT // targetTimeDomainPresentStage
    };

    showWindow(native, config.wsiType);

    CalibratedTimestampHelper befores(device, freq);
    CalibratedTimestampHelper afters(device, freq);
    for (uint32_t frameIdx = 0; frameIdx < frameCount; frameIdx++)
    {
        FrameStreamObjects::FrameObjects frame = frameStreamObjects.newFrame();

        uint32_t imageIndex = acquireNextImage(vkd, device, swapchain, frame.acquireFence);

        befores.getCalibratedTimestamps(vkd, domains, swapchainCalibratedTimesInfos);

        recordAndSubmitFrame(vkd, devHelper.queue, frame.commandBuffer, images[imageIndex], frame.renderSemaphore);

        const uint64_t presentId = frameIdx + 1;
        VK_CHECK_WSI(presentWithTimingInfo(vkd, devHelper.queue, frame.renderSemaphore, *swapchain, imageIndex,
                                           timingInfo, presentId));

        uint32_t resultsCount = drainPresentationTimingResults(vkd, device, *swapchain, pth, 1);
        if (resultsCount != 1)
            TCU_FAIL("Failed to retrieve all timing results");

        afters.getCalibratedTimestamps(vkd, domains, swapchainCalibratedTimesInfos);
    }

    pth.sortResults();

    // Verify the timing data report is complete for each result
    for (uint32_t i = 0; i < frameCount; i++)
    {
        // Check that each presented timestamp falls between the before/after calibrated timestamp
        VkPresentStageFlagsEXT presentStages = supportedPresentStageQueries;
        do
        {
            const VkPresentStageFlagsEXT presentStage = presentStages & -static_cast<int32_t>(presentStages);
            presentStages &= ~presentStage;

            // If the time domain ID has changed, skip it
            if (pth.results[i].timeDomainId != timeDomainId)
                continue;

            const uint64_t first  = befores.timestamps[i].presentStages.at(presentStage);
            const uint64_t second = pth.results[i].times[presentStage];
            const uint64_t third  = afters.timestamps[i].presentStages.at(presentStage);

            if (second == 0 || third == 0)
                continue;

            if (second < first || second > third)
                TCU_FAIL("Calibrated timestamps not monotonic");
        } while (presentStages);

        // Check timestamp diff between the calibrated device and present stage are the same (within deviation), if
        // device timestamps were successfully fetched
        if (afters.timestamps[i].host == 0 || befores.timestamps[i].host == 0)
            continue;

        const uint64_t hostDiff = afters.timestamps[i].host - befores.timestamps[i].host;
        const uint64_t presentDiff =
            afters.timestamps[i].presentStages.begin()->second - befores.timestamps[i].presentStages.begin()->second;
        const uint64_t absDiff = (hostDiff > presentDiff) ? (hostDiff - presentDiff) : (presentDiff - hostDiff);
        const uint64_t maxDiff =
            std::max(kCalibratedHostTimeMarginNs, befores.timestamps[i].deviation + afters.timestamps[i].deviation);

        if (absDiff > maxDiff)
            TCU_FAIL("Device timestamps differs from present timestamps more than expected deviation");
    }

    return tcu::TestStatus::pass("Tests ran successfully");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Test cases
//////////////////////////////////////////////////////////////////////////////////////////////////////

static const struct
{
    PresentAtMode mode;
    const char *name;
} presentAtModes[] = {
    {PresentAtMode::ABSOLUTE, "absolute"},
    {PresentAtMode::RELATIVE, "relative"},
};

static const struct
{
    vk::VkPresentModeKHR mode;
    const char *name;
} presentModes[] = {
    {VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
    {VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
    {VK_PRESENT_MODE_FIFO_KHR, "fifo"},
    {VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
    {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
    {VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    {VK_PRESENT_MODE_FIFO_LATEST_READY_EXT, "fifo_latest_ready"},
};

static const struct
{
    vk::VkPresentStageFlagsEXT stage;
    const char *name;
} presentStages[] = {
    {VK_PRESENT_STAGE_QUEUE_OPERATIONS_END_BIT_EXT, "queue_operations_end"},
    {VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT, "request_dequeued"},
    {VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT, "image_first_pixel_out"},
    {VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT, "image_first_pixel_visible"},
};

static const struct
{
    vk::VkTimeDomainKHR timeDomain;
    const char *name;
} timeDomains[] = {
    {VK_TIME_DOMAIN_DEVICE_KHR, "device"},
    {VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR, "clock_monotonic"},
    {VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR, "clock_monotonic_raw"},
    {VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, "query_performance_counter"},
    {VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT, "present_stage_local"},
    {VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT, "swapchain_local"},
};

static const struct
{
    vk::VkBool32 allow;
    const char *name;
} outOfOrderResults[] = {
    {true, "allow_out_of_order_results"},
    {false, "disallow_out_of_order_results"},
};

static const struct
{
    vk::VkBool32 allow;
    const char *name;
} partialResults[] = {
    {true, "allow_partial_results"},
    {false, "disallow_partial_results"},
};

void populateBasicGroup(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    addFunctionCase(testGroup, "surface_capabilities", surfaceCapabilitiesTest, wsiType);
    addFunctionCase(testGroup, "timing_queue", timingQueueTest, wsiType);
    addFunctionCase(testGroup, "retired_swapchain", retiredSwapchainTest, wsiType);
}

void populateQueryGroup(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    for (auto presentMode : presentModes)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentMode.name));

        for (auto presentStageQueries : presentStages)
        {
            de::MovePtr<tcu::TestCaseGroup> presentStageGroup(
                new tcu::TestCaseGroup(testGroup->getTestContext(), presentStageQueries.name));

            for (auto timeDomain : timeDomains)
            {
                PresentTimingTestConfig config = {
                    wsiType, {},   presentMode.mode, presentStageQueries.stage, timeDomain.timeDomain, false,
                    false,   false};

                addFunctionCase(&*presentStageGroup, timeDomain.name, timingTest, config);
            }

            presentModeGroup->addChild(presentStageGroup.release());
        }

        testGroup->addChild(presentModeGroup.release());
    }

    // A function can't be added to a test group which already has children groups, so we must create
    // this group for our parallel test case
    de::MovePtr<tcu::TestCaseGroup> parallelGroup(new tcu::TestCaseGroup(testGroup->getTestContext(), "parallel"));
    addFunctionCase(&*parallelGroup, "parallel", timingTestWithBackgroundQueryThreads, wsiType);
    testGroup->addChild(parallelGroup.release());
}

void populateTimeDomainGroup(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    addFunctionCase(testGroup, "properties", timeDomainPropertiesTest, wsiType);
    addFunctionCase(testGroup, "present_stage_calibration", timeDomainCalibrationTest,
                    CalibrationTestConfig{wsiType, VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT});
    addFunctionCase(testGroup, "swapchain_calibration", timeDomainCalibrationTest,
                    CalibrationTestConfig{wsiType, VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT});
}

void populatePresentAtGroup(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    // PresentAt is restricted to a few present modes
    vk::VkPresentModeKHR allowedPresentModes[] = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                                                  VK_PRESENT_MODE_FIFO_LATEST_READY_EXT};

    auto isPresentModeAllowed = [&allowedPresentModes](vk::VkPresentModeKHR presentMode)
    {
        for (const auto allowedMode : allowedPresentModes)
        {
            if (presentMode == allowedMode)
                return true;
        }
        return false;
    };

    for (auto &presentAtMode : presentAtModes)
    {
        de::MovePtr<tcu::TestCaseGroup> presentAtModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentAtMode.name));

        for (auto &presentMode : presentModes)
        {
            if (!isPresentModeAllowed(presentMode.mode))
            {
                continue;
            }

            de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
                new tcu::TestCaseGroup(testGroup->getTestContext(), presentMode.name));

            for (auto &timeDomain : timeDomains)
            {
                de::MovePtr<tcu::TestCaseGroup> timeDomainGroup(
                    new tcu::TestCaseGroup(testGroup->getTestContext(), timeDomain.name));

                for (auto &outOfOrderResult : outOfOrderResults)
                {
                    de::MovePtr<tcu::TestCaseGroup> outOfOrderResultsGroup(
                        new tcu::TestCaseGroup(testGroup->getTestContext(), outOfOrderResult.name));

                    for (auto &partial : partialResults)
                    {
                        de::MovePtr<tcu::TestCaseGroup> partialResultsGroup(
                            new tcu::TestCaseGroup(testGroup->getTestContext(), partial.name));

                        PresentTimingTestConfig config = {wsiType,
                                                          presentAtMode.mode,
                                                          presentMode.mode,
                                                          0,
                                                          timeDomain.timeDomain,
                                                          outOfOrderResult.allow,
                                                          partial.allow,
                                                          true};
                        addFunctionCase(&*partialResultsGroup, "nearest", presentAtTest, config);

                        config.presentAtNearestRefreshCycle = false;
                        addFunctionCase(&*partialResultsGroup, "after", presentAtTest, config);

                        outOfOrderResultsGroup->addChild(partialResultsGroup.release());
                    }

                    timeDomainGroup->addChild(outOfOrderResultsGroup.release());
                }

                presentModeGroup->addChild(timeDomainGroup.release());
            }

            presentAtModeGroup->addChild(presentModeGroup.release());
        }

        testGroup->addChild(presentAtModeGroup.release());
    }
}

} // namespace

void createPresentTimingTests(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    addTestGroup(testGroup, "basic", populateBasicGroup, wsiType);
    addTestGroup(testGroup, "query", populateQueryGroup, wsiType);
    addTestGroup(testGroup, "time_domain", populateTimeDomainGroup, wsiType);
    addTestGroup(testGroup, "present_at", populatePresentAtGroup, wsiType);
}

} // namespace wsi
} // namespace vkt
