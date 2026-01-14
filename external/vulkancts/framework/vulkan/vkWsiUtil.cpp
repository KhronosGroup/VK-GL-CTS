/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Windowing System Integration (WSI) Utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDeviceUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkWsiUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deArrayUtil.hpp"
#include "deMemory.h"

#include <limits>
#include <vector>

using std::vector;

#if defined(DEQP_SUPPORT_X11)
#include <X11/Xlib.h>
#if defined(DEQP_SUPPORT_XCB)
#include <xcb/xcb.h>
#endif // DEQP_SUPPORT_XCB
#endif // DEQP_SUPPORT_X11

#if defined(DEQP_SUPPORT_WAYLAND)
#include "tcuLnxWayland.hpp"
#define WAYLAND_DISPLAY 0
#endif // DEQP_SUPPORT_WAYLAND

#if (DE_OS == DE_OS_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace vk
{
namespace wsi
{

//! Get canonical WSI name that should be used for example in test case and group names.
const char *getName(Type wsiType)
{
    static const char *const s_names[] = {
        "xlib", "xcb", "wayland", "android", "win32", "metal", "headless", "direct_drm", "direct",
    };
    return de::getSizedArrayElement<TYPE_LAST>(s_names, wsiType);
}

const char *getExtensionName(Type wsiType)
{
    static const char *const s_extNames[] = {
        "VK_KHR_xlib_surface",     "VK_KHR_xcb_surface",         "VK_KHR_wayland_surface",
        "VK_KHR_android_surface",  "VK_KHR_win32_surface",       "VK_EXT_metal_surface",
        "VK_EXT_headless_surface", "VK_EXT_acquire_drm_display", "VK_KHR_display",
    };
    return de::getSizedArrayElement<TYPE_LAST>(s_extNames, wsiType);
}

const PlatformProperties &getPlatformProperties(Type wsiType)
{
    // \note These are declared here (rather than queried through vk::Platform for example)
    //         on purpose. The behavior of a platform is partly defined by the platform spec,
    //         and partly by WSI extensions, and platform ports should not need to override
    //         that definition.

    const uint32_t noDisplayLimit = std::numeric_limits<uint32_t>::max();
    const uint32_t noWindowLimit  = std::numeric_limits<uint32_t>::max();

    static const PlatformProperties s_properties[] = {
        // VK_KHR_xlib_surface
        {
            PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE | PlatformProperties::FEATURE_RESIZE_WINDOW,
            PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
            noDisplayLimit,
            noWindowLimit,
        },
        // VK_KHR_xcb_surface
        {
            PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE | PlatformProperties::FEATURE_RESIZE_WINDOW,
            PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
            noDisplayLimit,
            noWindowLimit,
        },
        // VK_KHR_wayland_surface
        {
            0u,
            PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE,
            noDisplayLimit,
            noWindowLimit,
        },
        // VK_KHR_android_surface
        {
            PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE, PlatformProperties::SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE,
            1u,
            1u, // Only one window available
        },
        // VK_KHR_win32_surface
        {
            PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE | PlatformProperties::FEATURE_RESIZE_WINDOW,
            PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
            noDisplayLimit,
            noWindowLimit,
        },
        // VK_EXT_metal_surface
        {
            PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE | PlatformProperties::FEATURE_RESIZE_WINDOW,
            PlatformProperties::SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE,
            noDisplayLimit,
            noWindowLimit,
        },
        // VK_EXT_headless_surface
        {
            0u,
            PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE,
            noDisplayLimit,
            noWindowLimit,
        },
        // VK_EXT_acquire_drm_display
        {
            0u,
            PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
            1u,
            1u,
        },
        // VK_KHR_display
        {
            0u,
            PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
            1u,
            1u,
        },
    };

    return de::getSizedArrayElement<TYPE_LAST>(s_properties, wsiType);
}

#ifndef CTS_USES_VULKANSC
static VkResult createDisplaySurface(const InstanceInterface &vki, VkInstance instance, VkDisplayKHR display,
                                     const tcu::CommandLine &cmdLine, const VkAllocationCallbacks *pAllocator,
                                     VkSurfaceKHR *pSurface)
{
    VkPhysicalDevice physDevice = chooseDevice(vki, instance, cmdLine);

    vector<VkDisplayPlanePropertiesKHR> planeProperties;
    uint32_t planeCount = 0u;
    uint32_t planeIndex = 0u;
    bool planeFound     = false;
    VK_CHECK_SUPPORTED(vki.getPhysicalDeviceDisplayPlanePropertiesKHR(physDevice, &planeCount, nullptr));

    planeProperties.resize(planeCount);
    VK_CHECK_SUPPORTED(vki.getPhysicalDeviceDisplayPlanePropertiesKHR(physDevice, &planeCount, &planeProperties[0]));

    for (uint32_t i = 0; i < planeCount; ++i)
    {
        vector<VkDisplayKHR> supportedDisplays;
        uint32_t supportedDisplayCount = 0u;
        VK_CHECK_SUPPORTED(vki.getDisplayPlaneSupportedDisplaysKHR(physDevice, i, &supportedDisplayCount, nullptr));

        supportedDisplays.resize(supportedDisplayCount);
        VK_CHECK_SUPPORTED(
            vki.getDisplayPlaneSupportedDisplaysKHR(physDevice, i, &supportedDisplayCount, &supportedDisplays[0]));

        for (uint32_t j = 0; j < supportedDisplayCount; ++j)
        {
            if (display == supportedDisplays[i])
            {
                planeIndex = i;
                planeFound = true;
                break;
            }
        }

        if (planeFound)
            break;
    }
    if (!planeFound)
        TCU_THROW(NotSupportedError, "No supported displays for planes.");

    vector<VkDisplayModePropertiesKHR> displayModeProperties;
    uint32_t displayModeCount = 0u;
    VK_CHECK_SUPPORTED(vki.getDisplayModePropertiesKHR(physDevice, display, &displayModeCount, nullptr));
    if (displayModeCount < 1)
        TCU_THROW(NotSupportedError, "No display modes defined.");

    displayModeProperties.resize(displayModeCount);
    VK_CHECK_SUPPORTED(
        vki.getDisplayModePropertiesKHR(physDevice, display, &displayModeCount, &displayModeProperties[0]));

    const VkDisplaySurfaceCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR, // VkStructureType                    sType
        nullptr,                                           // const void*                        pNext
        0,                                                 // VkDisplaySurfaceCreateFlagsKHR    flags
        displayModeProperties[0].displayMode,              // VkDisplayModeKHR                    displayMode
        planeIndex,                                        // uint32_t                            planeIndex
        planeProperties[planeIndex].currentStackIndex,     // uint32_t                            planeStackIndex
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,             // VkSurfaceTransformFlagBitsKHR    transform
        1.0f,                                              // float                            globalAlpha
        VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,             // VkDisplayPlaneAlphaFlagBitsKHR    alphaMode
        displayModeProperties[0].parameters.visibleRegion, // VkExtent2D                        imageExtent
    };

    return vki.createDisplayPlaneSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
}
#endif // CTS_USES_VULKANSC

VkResult createSurface(const InstanceInterface &vki, VkInstance instance, Type wsiType, const Display &nativeDisplay,
                       const Window &nativeWindow, const tcu::CommandLine &cmdLine,
                       const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
    // Update this function if you add more WSI implementations
    DE_STATIC_ASSERT(TYPE_LAST == 9);

#ifdef CTS_USES_VULKANSC
    DE_UNREF(vki);
    DE_UNREF(instance);
    DE_UNREF(wsiType);
    DE_UNREF(nativeDisplay);
    DE_UNREF(nativeWindow);
    DE_UNREF(cmdLine);
    DE_UNREF(pAllocator);
    DE_UNREF(pSurface);

    TCU_THROW(NotSupportedError, "Vulkan SC does not support createSurface");
#else  // CTS_USES_VULKANSC
    switch (wsiType)
    {
    case TYPE_XLIB:
    {
        const XlibDisplayInterface &xlibDisplay     = dynamic_cast<const XlibDisplayInterface &>(nativeDisplay);
        const XlibWindowInterface &xlibWindow       = dynamic_cast<const XlibWindowInterface &>(nativeWindow);
        const VkXlibSurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, nullptr,
                                                       (VkXlibSurfaceCreateFlagsKHR)0, xlibDisplay.getNative(),
                                                       xlibWindow.getNative()};

        return vki.createXlibSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_XCB:
    {
        const XcbDisplayInterface &xcbDisplay      = dynamic_cast<const XcbDisplayInterface &>(nativeDisplay);
        const XcbWindowInterface &xcbWindow        = dynamic_cast<const XcbWindowInterface &>(nativeWindow);
        const VkXcbSurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, nullptr,
                                                      (VkXcbSurfaceCreateFlagsKHR)0, xcbDisplay.getNative(),
                                                      xcbWindow.getNative()};

        return vki.createXcbSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_WAYLAND:
    {
        const WaylandDisplayInterface &waylandDisplay  = dynamic_cast<const WaylandDisplayInterface &>(nativeDisplay);
        const WaylandWindowInterface &waylandWindow    = dynamic_cast<const WaylandWindowInterface &>(nativeWindow);
        const VkWaylandSurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, nullptr,
                                                          (VkWaylandSurfaceCreateFlagsKHR)0, waylandDisplay.getNative(),
                                                          waylandWindow.getNative()};

        return vki.createWaylandSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_ANDROID:
    {
        const AndroidWindowInterface &androidWindow    = dynamic_cast<const AndroidWindowInterface &>(nativeWindow);
        const VkAndroidSurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, nullptr,
                                                          (VkAndroidSurfaceCreateFlagsKHR)0, androidWindow.getNative()};

        return vki.createAndroidSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_WIN32:
    {
        const Win32DisplayInterface &win32Display    = dynamic_cast<const Win32DisplayInterface &>(nativeDisplay);
        const Win32WindowInterface &win32Window      = dynamic_cast<const Win32WindowInterface &>(nativeWindow);
        const VkWin32SurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, nullptr,
                                                        (VkWin32SurfaceCreateFlagsKHR)0, win32Display.getNative(),
                                                        win32Window.getNative()};

        return vki.createWin32SurfaceKHR(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_METAL:
    {
        const MetalWindowInterface &metalWindow      = dynamic_cast<const MetalWindowInterface &>(nativeWindow);
        const VkMetalSurfaceCreateInfoEXT createInfo = {
            VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, nullptr, (VkMetalSurfaceCreateFlagsEXT)0,
            // pt::CAMetalLayer is defined as a pointer, but the struct def uses a pointer to this pointer type.
            // *sigh*...
            reinterpret_cast<pt::CAMetalLayer *>(metalWindow.getNative().internal)};

        return vki.createMetalSurfaceEXT(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_HEADLESS:
    {
        const VkHeadlessSurfaceCreateInfoEXT createInfo = {VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT, nullptr,
                                                           (VkHeadlessSurfaceCreateFlagsEXT)0};

        return vki.createHeadlessSurfaceEXT(instance, &createInfo, pAllocator, pSurface);
    }

    case TYPE_DIRECT_DRM:
    {
        DirectDrmDisplayInterface &drmDisplay =
            dynamic_cast<DirectDrmDisplayInterface &>(const_cast<Display &>(nativeDisplay));
        drmDisplay.initializeDisplay(vki, instance, cmdLine);
        return createDisplaySurface(vki, instance, drmDisplay.getNative(), cmdLine, pAllocator, pSurface);
    }

    case TYPE_DIRECT:
    {
        DirectDisplayInterface &directDisplay =
            dynamic_cast<DirectDisplayInterface &>(const_cast<Display &>(nativeDisplay));
        directDisplay.initializeDisplay(vki, instance, cmdLine);
        return createDisplaySurface(vki, instance, directDisplay.getNative(), cmdLine, pAllocator, pSurface);
    }

    default:
        DE_FATAL("Unknown WSI type");
        return VK_ERROR_SURFACE_LOST_KHR;
    }
#endif // CTS_USES_VULKANSC
    return VK_ERROR_SURFACE_LOST_KHR;
}

Move<VkSurfaceKHR> createSurface(const InstanceInterface &vki, VkInstance instance, Type wsiType,
                                 const Display &nativeDisplay, const Window &nativeWindow,
                                 const tcu::CommandLine &cmdLine, const VkAllocationCallbacks *pAllocator)
{
    VkSurfaceKHR object = VK_NULL_HANDLE;
    VK_CHECK(createSurface(vki, instance, wsiType, nativeDisplay, nativeWindow, cmdLine, pAllocator, &object));
    return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vki, instance, pAllocator));
}

VkBool32 getPhysicalDeviceSurfaceSupport(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                         uint32_t queueFamilyIndex, VkSurfaceKHR surface)
{
    VkBool32 result = 0;

    VK_CHECK(vki.getPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &result));

    return result;
}

VkBool32 getPhysicalDevicePresentationSupport(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                              uint32_t queueFamilyIndex, Type wsiType, const Display &nativeDisplay)
{
#ifdef CTS_USES_VULKANSC
    DE_UNREF(vki);
    DE_UNREF(physicalDevice);
    DE_UNREF(queueFamilyIndex);
    DE_UNREF(wsiType);
    DE_UNREF(nativeDisplay);
    TCU_THROW(NotSupportedError, "Vulkan SC does not support getPhysicalDevicePresentationSupport");
#else // CTS_USES_VULKANSC
    switch (wsiType)
    {
    case TYPE_XLIB:
    {
        const XlibDisplayInterface &xlibDisplay = dynamic_cast<const XlibDisplayInterface &>(nativeDisplay);
        pt::XlibVisualID visualID(0U);
#if defined(DEQP_SUPPORT_X11)
        ::Display *displayPtr = (::Display *)(xlibDisplay.getNative().internal);
        visualID.internal     = (uint32_t)(::XDefaultVisual(displayPtr, 0)->visualid);
#endif
        return vki.getPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, queueFamilyIndex,
                                                               xlibDisplay.getNative(), visualID);
    }
    case TYPE_XCB:
    {
        const XcbDisplayInterface &xcbDisplay = dynamic_cast<const XcbDisplayInterface &>(nativeDisplay);
        pt::XcbVisualid visualID(0U);
#if defined(DEQP_SUPPORT_XCB)
        xcb_connection_t *connPtr = (xcb_connection_t *)(xcbDisplay.getNative().internal);
        xcb_screen_t *screen      = xcb_setup_roots_iterator(xcb_get_setup(connPtr)).data;
        visualID.internal         = (uint32_t)(screen->root_visual);
#endif
        return vki.getPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, queueFamilyIndex, xcbDisplay.getNative(),
                                                              visualID);
    }
    case TYPE_WAYLAND:
    {
        const WaylandDisplayInterface &waylandDisplay = dynamic_cast<const WaylandDisplayInterface &>(nativeDisplay);
        return vki.getPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, queueFamilyIndex,
                                                                  waylandDisplay.getNative());
    }
    case TYPE_WIN32:
    {
        return vki.getPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
    }
    case TYPE_HEADLESS:
    case TYPE_ANDROID:
    case TYPE_METAL:
    case TYPE_DIRECT_DRM:
    case TYPE_DIRECT:
    {
        return 1;
    }
    default:
        DE_FATAL("Unknown WSI type");
        return 0;
    }
#endif // CTS_USES_VULKANSC
    return 1;
}

VkSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities(const InstanceInterface &vki,
                                                              VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR capabilities;

    deMemset(&capabilities, 0, sizeof(capabilities));

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));

    return capabilities;
}

VkSurfaceCapabilities2EXT getPhysicalDeviceSurfaceCapabilities2EXT(const InstanceInterface &vki,
                                                                   VkPhysicalDevice physicalDevice,
                                                                   VkSurfaceKHR surface)
{
    VkSurfaceCapabilities2EXT capabilities;

    deMemset(&capabilities, 0, sizeof(capabilities));
    capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT;

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2EXT(physicalDevice, surface, &capabilities));

    return capabilities;
}

bool sameSurfaceCapabilities(const VkSurfaceCapabilitiesKHR &khr, const VkSurfaceCapabilities2EXT &ext)
{
    return (
        khr.minImageCount == ext.minImageCount && khr.maxImageCount == ext.maxImageCount &&
        khr.currentExtent.width == ext.currentExtent.width && khr.currentExtent.height == ext.currentExtent.height &&
        khr.minImageExtent.width == ext.minImageExtent.width &&
        khr.minImageExtent.height == ext.minImageExtent.height &&
        khr.maxImageExtent.width == ext.maxImageExtent.width &&
        khr.maxImageExtent.height == ext.maxImageExtent.height && khr.maxImageArrayLayers == ext.maxImageArrayLayers &&
        khr.supportedTransforms == ext.supportedTransforms && khr.currentTransform == ext.currentTransform &&
        khr.supportedCompositeAlpha == ext.supportedCompositeAlpha &&
        khr.supportedUsageFlags == ext.supportedUsageFlags);
}

std::vector<VkSurfaceFormatKHR> getPhysicalDeviceSurfaceFormats(const InstanceInterface &vki,
                                                                VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t numFormats = 0;

    VK_CHECK(vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numFormats, nullptr));

    if (numFormats > 0)
    {
        std::vector<VkSurfaceFormatKHR> formats(numFormats);

        VK_CHECK(vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numFormats, &formats[0]));

        return formats;
    }
    else
        return std::vector<VkSurfaceFormatKHR>();
}

std::vector<VkPresentModeKHR> getPhysicalDeviceSurfacePresentModes(const InstanceInterface &vki,
                                                                   VkPhysicalDevice physicalDevice,
                                                                   VkSurfaceKHR surface)
{
    uint32_t numModes = 0;

    VK_CHECK(vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModes, nullptr));

    if (numModes > 0)
    {
        std::vector<VkPresentModeKHR> modes(numModes);

        VK_CHECK(vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModes, &modes[0]));

        return modes;
    }
    else
        return std::vector<VkPresentModeKHR>();
}

std::vector<VkImage> getSwapchainImages(const DeviceInterface &vkd, VkDevice device, VkSwapchainKHR swapchain)
{
    uint32_t numImages = 0;

    VK_CHECK(vkd.getSwapchainImagesKHR(device, swapchain, &numImages, nullptr));

    if (numImages > 0)
    {
        std::vector<VkImage> images(numImages);

        VK_CHECK(vkd.getSwapchainImagesKHR(device, swapchain, &numImages, &images[0]));

        return images;
    }
    else
        return std::vector<VkImage>();
}

namespace
{

std::vector<uint32_t> getSupportedQueueFamilyIndices(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                     VkSurfaceKHR surface)
{
    uint32_t numTotalFamilyIndices;
    vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(numTotalFamilyIndices);
    vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, &queueFamilyProperties[0]);

    std::vector<uint32_t> supportedFamilyIndices;
    for (uint32_t queueFamilyNdx = 0; queueFamilyNdx < numTotalFamilyIndices; ++queueFamilyNdx)
    {
        if (getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, surface) != VK_FALSE)
            supportedFamilyIndices.push_back(queueFamilyNdx);
    }

    return supportedFamilyIndices;
}

std::vector<uint32_t> getSortedSupportedQueueFamilyIndices(const vk::InstanceInterface &vki,
                                                           vk::VkPhysicalDevice physicalDevice,
                                                           vk::VkSurfaceKHR surface)
{
    std::vector<uint32_t> indices = getSupportedQueueFamilyIndices(vki, physicalDevice, surface);
    std::sort(begin(indices), end(indices));
    return indices;
}

} // namespace

uint32_t chooseQueueFamilyIndex(const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                const std::vector<vk::VkSurfaceKHR> &surfaces)
{
    auto indices = getCompatibleQueueFamilyIndices(vki, physicalDevice, surfaces);

    if (indices.empty())
        TCU_THROW(NotSupportedError, "Device does not support presentation to the given surfaces");

    return indices[0];
}

uint32_t chooseQueueFamilyIndex(const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                vk::VkSurfaceKHR surface)
{
    return chooseQueueFamilyIndex(vki, physicalDevice, std::vector<vk::VkSurfaceKHR>(1u, surface));
}

std::vector<uint32_t> getCompatibleQueueFamilyIndices(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                      const std::vector<VkSurfaceKHR> &surfaces)
{
    DE_ASSERT(!surfaces.empty());

    auto indices = getSortedSupportedQueueFamilyIndices(vki, physicalDevice, surfaces[0]);

    for (size_t i = 1; i < surfaces.size(); ++i)
    {
        auto newIndices = getSortedSupportedQueueFamilyIndices(vki, physicalDevice, surfaces[i]);

        // Set intersection and overwrite.
        decltype(indices) intersection;
        std::set_intersection(begin(indices), end(indices), begin(newIndices), end(newIndices),
                              std::back_inserter(intersection));
        indices = std::move(intersection);
    }

    return indices;
}

tcu::UVec2 getFullScreenSize(const vk::wsi::Type wsiType, const vk::wsi::Display &display,
                             const tcu::UVec2 &fallbackSize)
{
    tcu::UVec2 result = fallbackSize;

    switch (wsiType)
    {
    case TYPE_XLIB:
    {
#if defined(DEQP_SUPPORT_X11)
        const XlibDisplayInterface &xlibDisplay = dynamic_cast<const XlibDisplayInterface &>(display);
        ::Display *displayPtr                   = (::Display *)(xlibDisplay.getNative().internal);
        const Screen *screen                    = ScreenOfDisplay(displayPtr, 0);
        result.x()                              = uint32_t(screen->width);
        result.y()                              = uint32_t(screen->height);
#endif
        break;
    }
    case TYPE_XCB:
    {
#if defined(DEQP_SUPPORT_XCB)
// const XcbDisplayInterface& xcbDisplay = dynamic_cast<const XcbDisplayInterface&>(display);
// xcb_connection_t* connPtr = (xcb_connection_t*)(xcbDisplay.getNative().internal);
// xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(connPtr)).data;
// result.x() = uint32_t(screen->width_in_pixels);
// result.y() = uint32_t(screen->height_in_pixels);
#endif
        break;
    }
    case TYPE_WAYLAND:
    {
#if defined(DEQP_SUPPORT_WAYLAND)
#endif
        break;
    }
    case TYPE_ANDROID:
    {
#if (DE_OS == DE_OS_ANDROID)
#endif
        break;
    }
    case TYPE_WIN32:
    {
#if (DE_OS == DE_OS_WIN32)
        de::MovePtr<Window> nullWindow(display.createWindow(tcu::Nothing));
        const Win32WindowInterface &win32Window = dynamic_cast<const Win32WindowInterface &>(*nullWindow);
        HMONITOR hMonitor =
            (HMONITOR)MonitorFromWindow((HWND)win32Window.getNative().internal, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMonitor, &monitorInfo);
        result.x() = uint32_t(abs(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left));
        result.y() = uint32_t(abs(monitorInfo.rcMonitor.top - monitorInfo.rcMonitor.bottom));
#endif
        break;
    }

    case TYPE_METAL:
    {
#if (DE_OS == DE_OS_OSX)
#endif
        break;
    }

    default:
        DE_FATAL("Unknown WSI type");
        break;
    }

    DE_UNREF(display);
    return result;
}

VkBool32 isDisplaySurface(Type wsiType)
{
    switch (wsiType)
    {
    case TYPE_XLIB:
    case TYPE_XCB:
    case TYPE_WAYLAND:
    case TYPE_ANDROID:
    case TYPE_WIN32:
    case TYPE_METAL:
    case TYPE_HEADLESS:
        return 0;
    case TYPE_DIRECT_DRM:
    case TYPE_DIRECT:
        return 1;
    default:
        DE_FATAL("Unknown WSI type");
        return 0;
    }
}

Move<VkSwapchainKHR> createWsiSwapchain(Type wsiType, const DeviceInterface &vk, VkDevice device,
                                        const VkSwapchainCreateInfoKHR *pCreateInfo,
                                        const VkAllocationCallbacks *pAllocator)
{
    try
    {
        return createSwapchainKHR(vk, device, pCreateInfo, pAllocator);
    }
    catch (const vk::Error &error)
    {
        switch (wsiType)
        {
        case TYPE_XLIB:
        case TYPE_XCB:
        case TYPE_WAYLAND:
        case TYPE_ANDROID:
        case TYPE_WIN32:
        case TYPE_METAL:
        case TYPE_HEADLESS:
            throw error;

        case TYPE_DIRECT_DRM:
        case TYPE_DIRECT:
            // "Swapchain creation may fail if that VkDisplayKHR is not acquired by the application.
            // In this scenario VK_ERROR_INITIALIZATION_FAILED is returned."
            if (error.getError() == VK_ERROR_INITIALIZATION_FAILED)
                TCU_THROW(NotSupportedError,
                          "Swapchain creation on VkDisplayKHR not acquired by the application is unsupported");
            throw error;

        default:
            DE_FATAL("Unknown WSI type");
            break;
        }
    }
    catch (...)
    {
        throw;
    }
    DE_FATAL("Unreachable");
    VkSwapchainKHR object = VK_NULL_HANDLE;
    return Move<VkSwapchainKHR>(check<VkSwapchainKHR>(object), Deleter<VkSwapchainKHR>(vk, device, pAllocator));
}

VkResult createWsiSwapchain(Type wsiType, const DeviceInterface &vk, VkDevice device,
                            const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator,
                            VkSwapchainKHR *object)
{
    VkResult result = vk.createSwapchainKHR(device, pCreateInfo, pAllocator, object);
    switch (wsiType)
    {
    case TYPE_XLIB:
    case TYPE_XCB:
    case TYPE_WAYLAND:
    case TYPE_ANDROID:
    case TYPE_WIN32:
    case TYPE_METAL:
    case TYPE_HEADLESS:
        return result;

    case TYPE_DIRECT_DRM:
    case TYPE_DIRECT:
        // "Swapchain creation may fail if that VkDisplayKHR is not acquired by the application.
        // In this scenario VK_ERROR_INITIALIZATION_FAILED is returned."
        if (result == VK_ERROR_INITIALIZATION_FAILED)
            TCU_THROW(NotSupportedError,
                      "Swapchain creation on VkDisplayKHR not acquired by the application is unsupported");
        return result;

    default:
        DE_FATAL("Unknown WSI type");
        break;
    }
    return result;
}

Move<VkRenderPass> WsiTriangleRenderer::createRenderPass(const DeviceInterface &vkd, const VkDevice device,
                                                         const VkFormat colorAttachmentFormat,
                                                         const bool explicitLayoutTransitions)
{
    const VkAttachmentDescription colorAttDesc = {
        (VkAttachmentDescriptionFlags)0,
        colorAttachmentFormat,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        (explicitLayoutTransitions) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        (explicitLayoutTransitions) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    const VkAttachmentReference colorAttRef = {
        0u,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpassDesc = {
        (VkSubpassDescriptionFlags)0u,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0u,           // inputAttachmentCount
        nullptr,      // pInputAttachments
        1u,           // colorAttachmentCount
        &colorAttRef, // pColorAttachments
        nullptr,      // pResolveAttachments
        nullptr,      // depthStencilAttachment
        0u,           // preserveAttachmentCount
        nullptr,      // pPreserveAttachments
    };
    const VkSubpassDependency dependencies[] = {
        {VK_SUBPASS_EXTERNAL, // srcSubpass
         0u,                  // dstSubpass
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_READ_BIT,
         (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), VK_DEPENDENCY_BY_REGION_BIT},
        {0u,                  // srcSubpass
         VK_SUBPASS_EXTERNAL, // dstSubpass
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
         (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), VK_ACCESS_MEMORY_READ_BIT,
         VK_DEPENDENCY_BY_REGION_BIT},
    };
    const VkRenderPassCreateInfo renderPassParams = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        (VkRenderPassCreateFlags)0,
        1u,
        &colorAttDesc,
        1u,
        &subpassDesc,
        DE_LENGTH_OF_ARRAY(dependencies),
        dependencies,
    };

    return vk::createRenderPass(vkd, device, &renderPassParams);
}

Move<VkPipelineLayout> WsiTriangleRenderer::createPipelineLayout(const DeviceInterface &vkd, const VkDevice device)
{
    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_VERTEX_BIT,
        0u,                         // offset
        (uint32_t)sizeof(uint32_t), // size
    };
    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        (vk::VkPipelineLayoutCreateFlags)0,
        0u,      // setLayoutCount
        nullptr, // pSetLayouts
        1u,
        &pushConstantRange,
    };

    return vk::createPipelineLayout(vkd, device, &pipelineLayoutParams);
}

Move<VkPipeline> WsiTriangleRenderer::createPipeline(const DeviceInterface &vkd, const VkDevice device,
                                                     const VkRenderPass renderPass,
                                                     const VkPipelineLayout pipelineLayout,
                                                     const BinaryCollection &binaryCollection,
                                                     const tcu::UVec2 &renderSize)
{
    // \note VkShaderModules are fully consumed by vkCreateGraphicsPipelines()
    //         and can be deleted immediately following that call.
    const Unique<VkShaderModule> vertShaderModule(createShaderModule(vkd, device, binaryCollection.get("tri-vert"), 0));
    const Unique<VkShaderModule> fragShaderModule(createShaderModule(vkd, device, binaryCollection.get("tri-frag"), 0));
    const std::vector<VkViewport> viewports(1, makeViewport(renderSize));
    const std::vector<VkRect2D> scissors(1, makeRect2D(renderSize));

    return vk::makeGraphicsPipeline(vkd,               // const DeviceInterface&            vk
                                    device,            // const VkDevice                    device
                                    pipelineLayout,    // const VkPipelineLayout            pipelineLayout
                                    *vertShaderModule, // const VkShaderModule              vertexShaderModule
                                    VK_NULL_HANDLE, // const VkShaderModule              tessellationControlShaderModule
                                    VK_NULL_HANDLE, // const VkShaderModule              tessellationEvalShaderModule
                                    VK_NULL_HANDLE, // const VkShaderModule              geometryShaderModule
                                    *fragShaderModule, // const VkShaderModule              fragmentShaderModule
                                    renderPass,        // const VkRenderPass                renderPass
                                    viewports,         // const std::vector<VkViewport>&    viewports
                                    scissors);         // const std::vector<VkRect2D>&      scissors
}

Move<VkImageView> WsiTriangleRenderer::createAttachmentView(const DeviceInterface &vkd, const VkDevice device,
                                                            const VkImage image, const VkFormat format)
{
    const VkImageViewCreateInfo viewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        (VkImageViewCreateFlags)0,
        image,
        VK_IMAGE_VIEW_TYPE_2D,
        format,
        vk::makeComponentMappingRGBA(),
        {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0u, // baseMipLevel
            1u, // levelCount
            0u, // baseArrayLayer
            1u, // layerCount
        },
    };

    return vk::createImageView(vkd, device, &viewParams);
}

Move<VkFramebuffer> WsiTriangleRenderer::createFramebuffer(const DeviceInterface &vkd, const VkDevice device,
                                                           const VkRenderPass renderPass,
                                                           const VkImageView colorAttachment,
                                                           const tcu::UVec2 &renderSize)
{
    const VkFramebufferCreateInfo framebufferParams = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        (VkFramebufferCreateFlags)0,
        renderPass,
        1u,
        &colorAttachment,
        renderSize.x(),
        renderSize.y(),
        1u, // layers
    };

    return vk::createFramebuffer(vkd, device, &framebufferParams);
}

Move<VkBuffer> WsiTriangleRenderer::createBuffer(const DeviceInterface &vkd, VkDevice device, VkDeviceSize size,
                                                 VkBufferUsageFlags usage)
{
    const VkBufferCreateInfo bufferParams = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                             nullptr,
                                             (VkBufferCreateFlags)0,
                                             size,
                                             usage,
                                             VK_SHARING_MODE_EXCLUSIVE,
                                             0,
                                             nullptr};

    return vk::createBuffer(vkd, device, &bufferParams);
}

WsiTriangleRenderer::WsiTriangleRenderer(const DeviceInterface &vkd, const VkDevice device, Allocator &allocator,
                                         const BinaryCollection &binaryRegistry, bool explicitLayoutTransitions,
                                         const vector<VkImage> swapchainImages, const vector<VkImage> aliasImages,
                                         const VkFormat framebufferFormat, const tcu::UVec2 &renderSize)
    : m_vkd(vkd)
    , m_explicitLayoutTransitions(explicitLayoutTransitions)
    , m_swapchainImages(swapchainImages)
    , m_aliasImages(aliasImages)
    , m_renderSize(renderSize)
    , m_renderPass(createRenderPass(vkd, device, framebufferFormat, m_explicitLayoutTransitions))
    , m_pipelineLayout(createPipelineLayout(vkd, device))
    , m_pipeline(createPipeline(vkd, device, *m_renderPass, *m_pipelineLayout, binaryRegistry, renderSize))
    , m_vertexBuffer(
          createBuffer(vkd, device, (VkDeviceSize)(sizeof(float) * 4 * 3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
    , m_vertexBufferMemory(
          allocator.allocate(getBufferMemoryRequirements(vkd, device, *m_vertexBuffer), MemoryRequirement::HostVisible))
{
    m_attachmentViews.resize(swapchainImages.size());
    m_attachmentLayouts.resize(swapchainImages.size());
    m_framebuffers.resize(swapchainImages.size());

    for (size_t imageNdx = 0; imageNdx < swapchainImages.size(); ++imageNdx)
    {
        m_attachmentViews[imageNdx] = ImageViewSp(
            new Unique<VkImageView>(createAttachmentView(vkd, device, swapchainImages[imageNdx], framebufferFormat)));
        m_attachmentLayouts[imageNdx] = VK_IMAGE_LAYOUT_UNDEFINED;
        m_framebuffers[imageNdx]      = FramebufferSp(new Unique<VkFramebuffer>(
            createFramebuffer(vkd, device, *m_renderPass, **m_attachmentViews[imageNdx], renderSize)));
    }

    VK_CHECK(vkd.bindBufferMemory(device, *m_vertexBuffer, m_vertexBufferMemory->getMemory(),
                                  m_vertexBufferMemory->getOffset()));

    {
        const VkMappedMemoryRange memRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr,
                                              m_vertexBufferMemory->getMemory(), m_vertexBufferMemory->getOffset(),
                                              VK_WHOLE_SIZE};
        const tcu::Vec4 vertices[]         = {tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f), tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
                                              tcu::Vec4(0.0f, +0.5f, 0.0f, 1.0f)};
        DE_STATIC_ASSERT(sizeof(vertices) == sizeof(float) * 4 * 3);

        deMemcpy(m_vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
        VK_CHECK(vkd.flushMappedMemoryRanges(device, 1u, &memRange));
    }
}

WsiTriangleRenderer::WsiTriangleRenderer(WsiTriangleRenderer &&other)
    : m_vkd(other.m_vkd)
    , m_explicitLayoutTransitions(other.m_explicitLayoutTransitions)
    , m_swapchainImages(other.m_swapchainImages)
    , m_aliasImages(other.m_aliasImages)
    , m_renderSize(other.m_renderSize)
    , m_renderPass(other.m_renderPass)
    , m_pipelineLayout(other.m_pipelineLayout)
    , m_pipeline(other.m_pipeline)
    , m_vertexBuffer(other.m_vertexBuffer)
    , m_vertexBufferMemory(other.m_vertexBufferMemory)
    , m_attachmentViews(other.m_attachmentViews)
    , m_attachmentLayouts(other.m_attachmentLayouts)
    , m_framebuffers(other.m_framebuffers)
{
}

WsiTriangleRenderer::~WsiTriangleRenderer(void)
{
}

void WsiTriangleRenderer::recordFrame(VkCommandBuffer cmdBuffer, uint32_t imageNdx, uint32_t frameNdx) const
{
    const VkFramebuffer curFramebuffer = **m_framebuffers[imageNdx];

    beginCommandBuffer(m_vkd, cmdBuffer, 0u);

    if (m_explicitLayoutTransitions || m_attachmentLayouts[imageNdx] == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        const auto range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
        const auto newLayout =
            (m_explicitLayoutTransitions ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        const auto srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const auto srcMask  = 0u;
        const auto dstStage = (m_explicitLayoutTransitions ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT :
                                                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        const auto dstMask  = (m_explicitLayoutTransitions ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0);

        const auto barrier = makeImageMemoryBarrier(srcMask, dstMask, m_attachmentLayouts[imageNdx], newLayout,
                                                    m_aliasImages[imageNdx], range);
        m_vkd.cmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);

        m_attachmentLayouts[imageNdx] = newLayout;
    }

    beginRenderPass(m_vkd, cmdBuffer, *m_renderPass, curFramebuffer,
                    makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), tcu::Vec4(0.125f, 0.25f, 0.75f, 1.0f));

    m_vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    {
        const VkDeviceSize bindingOffset = 0;
        m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &bindingOffset);
    }

    m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, (uint32_t)sizeof(uint32_t),
                           &frameNdx);
    m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(m_vkd, cmdBuffer);

    if (m_explicitLayoutTransitions)
    {
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const VkImageMemoryBarrier barrier =
            makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, m_attachmentLayouts[imageNdx],
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, m_aliasImages[imageNdx], range);
        m_vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
        m_attachmentLayouts[imageNdx] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    endCommandBuffer(m_vkd, cmdBuffer);
}

void WsiTriangleRenderer::recordDeviceGroupFrame(VkCommandBuffer cmdBuffer, uint32_t firstDeviceID,
                                                 uint32_t secondDeviceID, uint32_t devicesCount, uint32_t imageNdx,
                                                 uint32_t frameNdx) const
{
    const VkFramebuffer curFramebuffer = **m_framebuffers[imageNdx];

    beginCommandBuffer(m_vkd, cmdBuffer, 0u);

    if (m_explicitLayoutTransitions || m_attachmentLayouts[imageNdx] == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        const auto range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
        const auto newLayout =
            (m_explicitLayoutTransitions ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        const auto srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const auto srcMask  = 0u;
        const auto dstStage = (m_explicitLayoutTransitions ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT :
                                                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        const auto dstMask  = (m_explicitLayoutTransitions ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0);

        const auto barrier = makeImageMemoryBarrier(srcMask, dstMask, m_attachmentLayouts[imageNdx], newLayout,
                                                    m_aliasImages[imageNdx], range);
        m_vkd.cmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);

        m_attachmentLayouts[imageNdx] = newLayout;
    }

    // begin renderpass
    {
        const VkClearValue clearValue = makeClearValueColorF32(0.125f, 0.25f, 0.75f, 1.0f);

        VkRect2D zeroRect = {{
                                 0,
                                 0,
                             },
                             {
                                 0,
                                 0,
                             }};
        vector<VkRect2D> renderAreas;
        for (uint32_t i = 0; i < devicesCount; i++)
            renderAreas.push_back(zeroRect);

        // Render completely if there is only 1 device
        if (devicesCount == 1u)
        {
            renderAreas[0].extent.width  = (int32_t)m_renderSize.x();
            renderAreas[0].extent.height = (int32_t)m_renderSize.y();
        }
        else
        {
            // Split into 2 vertical halves
            renderAreas[firstDeviceID].extent.width  = (int32_t)m_renderSize.x() / 2;
            renderAreas[firstDeviceID].extent.height = (int32_t)m_renderSize.y();
            renderAreas[secondDeviceID]              = renderAreas[firstDeviceID];
            renderAreas[secondDeviceID].offset.x     = (int32_t)m_renderSize.x() / 2;
        }

        const VkDeviceGroupRenderPassBeginInfo deviceGroupRPBeginInfo = {
            VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO, nullptr, (uint32_t)((1 << devicesCount) - 1),
            devicesCount, &renderAreas[0]};

        const VkRenderPassBeginInfo passBeginParams = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,       // sType
            &deviceGroupRPBeginInfo,                        // pNext
            *m_renderPass,                                  // renderPass
            curFramebuffer,                                 // framebuffer
            {{0, 0}, {m_renderSize.x(), m_renderSize.y()}}, // renderArea
            1u,                                             // clearValueCount
            &clearValue,                                    // pClearValues
        };
        m_vkd.cmdBeginRenderPass(cmdBuffer, &passBeginParams, VK_SUBPASS_CONTENTS_INLINE);
    }

    m_vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    {
        const VkDeviceSize bindingOffset = 0;
        m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &bindingOffset);
    }

    m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, (uint32_t)sizeof(uint32_t),
                           &frameNdx);
    m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(m_vkd, cmdBuffer);

    if (m_explicitLayoutTransitions)
    {
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const VkImageMemoryBarrier barrier =
            makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, m_attachmentLayouts[imageNdx],
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, m_aliasImages[imageNdx], range);
        m_vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
        m_attachmentLayouts[imageNdx] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    endCommandBuffer(m_vkd, cmdBuffer);
}

void WsiTriangleRenderer::getPrograms(SourceCollections &dst)
{
    dst.glslSources.add("tri-vert") << glu::VertexSource("#version 310 es\n"
                                                         "layout(location = 0) in highp vec4 a_position;\n"
                                                         "layout(push_constant) uniform FrameData\n"
                                                         "{\n"
                                                         "    highp uint frameNdx;\n"
                                                         "} frameData;\n"
                                                         "void main (void)\n"
                                                         "{\n"
                                                         "    highp float angle = float(frameData.frameNdx) / 100.0;\n"
                                                         "    highp float c     = cos(angle);\n"
                                                         "    highp float s     = sin(angle);\n"
                                                         "    highp mat4  t     = mat4( c, -s,  0,  0,\n"
                                                         "                              s,  c,  0,  0,\n"
                                                         "                              0,  0,  1,  0,\n"
                                                         "                              0,  0,  0,  1);\n"
                                                         "    gl_Position = t * a_position;\n"
                                                         "}\n");
    dst.glslSources.add("tri-frag") << glu::FragmentSource(
        "#version 310 es\n"
        "layout(location = 0) out lowp vec4 o_color;\n"
        "void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
}

} // namespace wsi
} // namespace vk
