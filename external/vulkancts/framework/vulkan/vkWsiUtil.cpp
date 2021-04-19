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

#if defined (DEQP_SUPPORT_X11)
#	include <X11/Xlib.h>
#	if defined (DEQP_SUPPORT_XCB)
#		include <xcb/xcb.h>
#	endif // DEQP_SUPPORT_XCB
#endif // DEQP_SUPPORT_X11

#if defined (DEQP_SUPPORT_WAYLAND)
#	include "tcuLnxWayland.hpp"
#	define WAYLAND_DISPLAY DE_NULL
#endif // DEQP_SUPPORT_WAYLAND

#if ( DE_OS == DE_OS_WIN32 )
	#define NOMINMAX
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

namespace vk
{
namespace wsi
{

//! Get canonical WSI name that should be used for example in test case and group names.
const char* getName (Type wsiType)
{
	static const char* const s_names[] =
	{
		"xlib",
		"xcb",
		"wayland",
		"android",
		"win32",
		"macos",
		"headless"
	};
	return de::getSizedArrayElement<TYPE_LAST>(s_names, wsiType);
}

const char* getExtensionName (Type wsiType)
{
	static const char* const s_extNames[] =
	{
		"VK_KHR_xlib_surface",
		"VK_KHR_xcb_surface",
		"VK_KHR_wayland_surface",
		"VK_KHR_android_surface",
		"VK_KHR_win32_surface",
		"VK_MVK_macos_surface",
		"VK_EXT_headless_surface"
	};
	return de::getSizedArrayElement<TYPE_LAST>(s_extNames, wsiType);
}

const PlatformProperties& getPlatformProperties (Type wsiType)
{
	// \note These are declared here (rather than queried through vk::Platform for example)
	//		 on purpose. The behavior of a platform is partly defined by the platform spec,
	//		 and partly by WSI extensions, and platform ports should not need to override
	//		 that definition.

	const deUint32	noDisplayLimit	= std::numeric_limits<deUint32>::max();
	const deUint32	noWindowLimit	= std::numeric_limits<deUint32>::max();

	static const PlatformProperties s_properties[] =
	{
		// VK_KHR_xlib_surface
		{
			PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE|PlatformProperties::FEATURE_RESIZE_WINDOW,
			PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
			noDisplayLimit,
			noWindowLimit,
		},
		// VK_KHR_xcb_surface
		{
			PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE|PlatformProperties::FEATURE_RESIZE_WINDOW,
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
			PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE,
			PlatformProperties::SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE,
			1u,
			1u, // Only one window available
		},
		// VK_KHR_win32_surface
		{
			PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE|PlatformProperties::FEATURE_RESIZE_WINDOW,
			PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
			noDisplayLimit,
			noWindowLimit,
		},
		// VK_MVK_macos_surface
		{
			PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE|PlatformProperties::FEATURE_RESIZE_WINDOW,
			PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE,
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
	};

	return de::getSizedArrayElement<TYPE_LAST>(s_properties, wsiType);
}

VkResult createSurface (const InstanceInterface&		vki,
						VkInstance						instance,
						Type							wsiType,
						const Display&					nativeDisplay,
						const Window&					nativeWindow,
						const VkAllocationCallbacks*	pAllocator,
						VkSurfaceKHR*					pSurface)
{
	// Update this function if you add more WSI implementations
	DE_STATIC_ASSERT(TYPE_LAST == 7);

	switch (wsiType)
	{
		case TYPE_XLIB:
		{
			const XlibDisplayInterface&			xlibDisplay		= dynamic_cast<const XlibDisplayInterface&>(nativeDisplay);
			const XlibWindowInterface&			xlibWindow		= dynamic_cast<const XlibWindowInterface&>(nativeWindow);
			const VkXlibSurfaceCreateInfoKHR	createInfo		=
			{
				VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
				DE_NULL,
				(VkXlibSurfaceCreateFlagsKHR)0,
				xlibDisplay.getNative(),
				xlibWindow.getNative()
			};

			return vki.createXlibSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
		}

		case TYPE_XCB:
		{
			const XcbDisplayInterface&			xcbDisplay		= dynamic_cast<const XcbDisplayInterface&>(nativeDisplay);
			const XcbWindowInterface&			xcbWindow		= dynamic_cast<const XcbWindowInterface&>(nativeWindow);
			const VkXcbSurfaceCreateInfoKHR		createInfo		=
			{
				VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
				DE_NULL,
				(VkXcbSurfaceCreateFlagsKHR)0,
				xcbDisplay.getNative(),
				xcbWindow.getNative()
			};

			return vki.createXcbSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
		}

		case TYPE_WAYLAND:
		{
			const WaylandDisplayInterface&		waylandDisplay	= dynamic_cast<const WaylandDisplayInterface&>(nativeDisplay);
			const WaylandWindowInterface&		waylandWindow	= dynamic_cast<const WaylandWindowInterface&>(nativeWindow);
			const VkWaylandSurfaceCreateInfoKHR	createInfo		=
			{
				VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
				DE_NULL,
				(VkWaylandSurfaceCreateFlagsKHR)0,
				waylandDisplay.getNative(),
				waylandWindow.getNative()
			};

			return vki.createWaylandSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
		}

		case TYPE_ANDROID:
		{
			const AndroidWindowInterface&		androidWindow	= dynamic_cast<const AndroidWindowInterface&>(nativeWindow);
			const VkAndroidSurfaceCreateInfoKHR	createInfo		=
			{
				VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
				DE_NULL,
				(VkAndroidSurfaceCreateFlagsKHR)0,
				androidWindow.getNative()
			};

			return vki.createAndroidSurfaceKHR(instance, &createInfo, pAllocator, pSurface);
		}

		case TYPE_WIN32:
		{
			const Win32DisplayInterface&		win32Display	= dynamic_cast<const Win32DisplayInterface&>(nativeDisplay);
			const Win32WindowInterface&			win32Window		= dynamic_cast<const Win32WindowInterface&>(nativeWindow);
			const VkWin32SurfaceCreateInfoKHR	createInfo		=
			{
				VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				DE_NULL,
				(VkWin32SurfaceCreateFlagsKHR)0,
				win32Display.getNative(),
				win32Window.getNative()
			};

			return vki.createWin32SurfaceKHR(instance, &createInfo, pAllocator, pSurface);
		}

		case TYPE_MACOS:
		{
			const MacOSWindowInterface&			macOSWindow		= dynamic_cast<const MacOSWindowInterface&>(nativeWindow);
			const VkMacOSSurfaceCreateInfoMVK	createInfo		=
			{
				VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
				DE_NULL,
				(VkMacOSSurfaceCreateFlagsMVK)0,
				macOSWindow.getNative()
			};

			return vki.createMacOSSurfaceMVK(instance, &createInfo, pAllocator, pSurface);
		}

		case TYPE_HEADLESS:
		{
			const VkHeadlessSurfaceCreateInfoEXT	createInfo		=
			{
				VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
				DE_NULL,
				(VkHeadlessSurfaceCreateFlagsEXT)0
			};

			return vki.createHeadlessSurfaceEXT(instance, &createInfo, pAllocator, pSurface);
		}

		default:
			DE_FATAL("Unknown WSI type");
			return VK_ERROR_SURFACE_LOST_KHR;
	}
}

Move<VkSurfaceKHR> createSurface (const InstanceInterface&		vki,
								  VkInstance					instance,
								  Type							wsiType,
								  const Display&				nativeDisplay,
								  const Window&					nativeWindow,
								  const VkAllocationCallbacks*	pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(createSurface(vki, instance, wsiType, nativeDisplay, nativeWindow, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vki, instance, pAllocator));
}

VkBool32 getPhysicalDeviceSurfaceSupport (const InstanceInterface&	vki,
										  VkPhysicalDevice			physicalDevice,
										  deUint32					queueFamilyIndex,
										  VkSurfaceKHR				surface)
{
	VkBool32 result = 0;

	VK_CHECK(vki.getPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &result));

	return result;
}

VkBool32 getPhysicalDevicePresentationSupport (const InstanceInterface&	vki,
											   VkPhysicalDevice			physicalDevice,
											   deUint32					queueFamilyIndex,
											   Type						wsiType,
											   const Display&			nativeDisplay)
{
	switch (wsiType)
	{
		case TYPE_XLIB:
		{
			const XlibDisplayInterface&		xlibDisplay	= dynamic_cast<const XlibDisplayInterface&>(nativeDisplay);
			pt::XlibVisualID				visualID	(0U);
#if defined (DEQP_SUPPORT_X11)
			::Display*						displayPtr	= (::Display*)(xlibDisplay.getNative().internal);
			visualID.internal							= (deUint32)(::XDefaultVisual(displayPtr,0)->visualid);
#endif
			return vki.getPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, queueFamilyIndex, xlibDisplay.getNative(), visualID);
		}
		case TYPE_XCB:
		{
			const XcbDisplayInterface&		xcbDisplay	= dynamic_cast<const XcbDisplayInterface&>(nativeDisplay);
			pt::XcbVisualid					visualID	(0U);
#if defined (DEQP_SUPPORT_XCB)
			xcb_connection_t*				connPtr		= (xcb_connection_t*)(xcbDisplay.getNative().internal);
			xcb_screen_t*					screen		= xcb_setup_roots_iterator(xcb_get_setup(connPtr)).data;
			visualID.internal							= (deUint32)(screen->root_visual);
#endif
			return vki.getPhysicalDeviceXcbPresentationSupportKHR(physicalDevice, queueFamilyIndex, xcbDisplay.getNative(), visualID);
		}
		case TYPE_WAYLAND:
		{
			const WaylandDisplayInterface&	waylandDisplay	= dynamic_cast<const WaylandDisplayInterface&>(nativeDisplay);
			return vki.getPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, queueFamilyIndex, waylandDisplay.getNative());
		}
		case TYPE_WIN32:
		{
			return vki.getPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, queueFamilyIndex);
		}
		case TYPE_HEADLESS:
		case TYPE_ANDROID:
		case TYPE_MACOS:
		{
			return 1;
		}
		default:
			DE_FATAL("Unknown WSI type");
			return 0;
	}
	return 1;
}

VkSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities (const InstanceInterface&		vki,
															   VkPhysicalDevice				physicalDevice,
															   VkSurfaceKHR					surface)
{
	VkSurfaceCapabilitiesKHR capabilities;

	deMemset(&capabilities, 0, sizeof(capabilities));

	VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));

	return capabilities;
}

VkSurfaceCapabilities2EXT getPhysicalDeviceSurfaceCapabilities2EXT (const InstanceInterface&		vki,
																	VkPhysicalDevice				physicalDevice,
																	VkSurfaceKHR					surface)
{
	VkSurfaceCapabilities2EXT capabilities;

	deMemset(&capabilities, 0, sizeof(capabilities));
	capabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT;

	VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2EXT(physicalDevice, surface, &capabilities));

	return capabilities;
}

bool sameSurfaceCapabilities (const VkSurfaceCapabilitiesKHR&	khr,
							  const VkSurfaceCapabilities2EXT&	ext)
{
	return (	khr.minImageCount			== ext.minImageCount &&
				khr.maxImageCount			== ext.maxImageCount &&
				khr.currentExtent.width		== ext.currentExtent.width &&
				khr.currentExtent.height	== ext.currentExtent.height &&
				khr.minImageExtent.width	== ext.minImageExtent.width &&
				khr.minImageExtent.height	== ext.minImageExtent.height &&
				khr.maxImageExtent.width	== ext.maxImageExtent.width &&
				khr.maxImageExtent.height	== ext.maxImageExtent.height &&
				khr.maxImageArrayLayers		== ext.maxImageArrayLayers &&
				khr.supportedTransforms		== ext.supportedTransforms &&
				khr.currentTransform		== ext.currentTransform &&
				khr.supportedCompositeAlpha	== ext.supportedCompositeAlpha &&
				khr.supportedUsageFlags		== ext.supportedUsageFlags	);
}

std::vector<VkSurfaceFormatKHR> getPhysicalDeviceSurfaceFormats (const InstanceInterface&		vki,
																 VkPhysicalDevice				physicalDevice,
																 VkSurfaceKHR					surface)
{
	deUint32	numFormats	= 0;

	VK_CHECK(vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numFormats, DE_NULL));

	if (numFormats > 0)
	{
		std::vector<VkSurfaceFormatKHR>	formats	(numFormats);

		VK_CHECK(vki.getPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numFormats, &formats[0]));

		return formats;
	}
	else
		return std::vector<VkSurfaceFormatKHR>();
}

std::vector<VkPresentModeKHR> getPhysicalDeviceSurfacePresentModes (const InstanceInterface&		vki,
																	VkPhysicalDevice				physicalDevice,
																	VkSurfaceKHR					surface)
{
	deUint32	numModes	= 0;

	VK_CHECK(vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModes, DE_NULL));

	if (numModes > 0)
	{
		std::vector<VkPresentModeKHR>	modes	(numModes);

		VK_CHECK(vki.getPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModes, &modes[0]));

		return modes;
	}
	else
		return std::vector<VkPresentModeKHR>();
}

std::vector<VkImage> getSwapchainImages (const DeviceInterface&			vkd,
										 VkDevice						device,
										 VkSwapchainKHR					swapchain)
{
	deUint32	numImages	= 0;

	VK_CHECK(vkd.getSwapchainImagesKHR(device, swapchain, &numImages, DE_NULL));

	if (numImages > 0)
	{
		std::vector<VkImage>	images	(numImages);

		VK_CHECK(vkd.getSwapchainImagesKHR(device, swapchain, &numImages, &images[0]));

		return images;
	}
	else
		return std::vector<VkImage>();
}

namespace
{

std::vector<deUint32> getSupportedQueueFamilyIndices (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	deUint32 numTotalFamilyIndices;
	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, DE_NULL);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(numTotalFamilyIndices);
	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, &queueFamilyProperties[0]);

	std::vector<deUint32> supportedFamilyIndices;
	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numTotalFamilyIndices; ++queueFamilyNdx)
	{
		if (getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, surface) != VK_FALSE)
			supportedFamilyIndices.push_back(queueFamilyNdx);
	}

	return supportedFamilyIndices;
}

std::vector<deUint32> getSortedSupportedQueueFamilyIndices (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, vk::VkSurfaceKHR surface)
{
	std::vector<deUint32> indices = getSupportedQueueFamilyIndices(vki, physicalDevice, surface);
	std::sort(begin(indices), end(indices));
	return indices;
}

} // anonymous

deUint32 chooseQueueFamilyIndex (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const std::vector<vk::VkSurfaceKHR>& surfaces)
{
	auto indices = getCompatibleQueueFamilyIndices(vki, physicalDevice, surfaces);

	if (indices.empty())
		TCU_THROW(NotSupportedError, "Device does not support presentation to the given surfaces");

	return indices[0];
}

deUint32 chooseQueueFamilyIndex (const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, vk::VkSurfaceKHR surface)
{
	return chooseQueueFamilyIndex(vki, physicalDevice, std::vector<vk::VkSurfaceKHR>(1u, surface));
}

std::vector<deUint32> getCompatibleQueueFamilyIndices (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const std::vector<VkSurfaceKHR>& surfaces)
{
	DE_ASSERT(!surfaces.empty());

	auto indices = getSortedSupportedQueueFamilyIndices(vki, physicalDevice, surfaces[0]);

	for (size_t i = 1; i < surfaces.size(); ++i)
	{
		auto newIndices = getSortedSupportedQueueFamilyIndices(vki, physicalDevice, surfaces[i]);

		// Set intersection and overwrite.
		decltype(indices) intersection;
		std::set_intersection(begin(indices), end(indices), begin(newIndices), end(newIndices), std::back_inserter(intersection));
		indices = std::move(intersection);
	}

	return indices;
}

tcu::UVec2 getFullScreenSize (const vk::wsi::Type wsiType, const vk::wsi::Display& display, const tcu::UVec2& fallbackSize)
{
	tcu::UVec2 result = fallbackSize;

	switch (wsiType)
	{
		case TYPE_XLIB:
		{
#if defined (DEQP_SUPPORT_X11)
			const XlibDisplayInterface&			xlibDisplay		= dynamic_cast<const XlibDisplayInterface&>(display);
			::Display*							displayPtr		= (::Display*)(xlibDisplay.getNative().internal);
			const Screen*						screen			= ScreenOfDisplay(displayPtr, 0);
			result.x()											= deUint32(screen->width);
			result.y()											= deUint32(screen->height);
#endif
			break;
		}
		case TYPE_XCB:
		{
#if defined (DEQP_SUPPORT_XCB)
//			const XcbDisplayInterface&			xcbDisplay		= dynamic_cast<const XcbDisplayInterface&>(display);
//			xcb_connection_t*					connPtr			= (xcb_connection_t*)(xcbDisplay.getNative().internal);
//			xcb_screen_t*						screen			= xcb_setup_roots_iterator(xcb_get_setup(connPtr)).data;
//			result.x()											= deUint32(screen->width_in_pixels);
//			result.y()											= deUint32(screen->height_in_pixels);
#endif
			break;
		}
		case TYPE_WAYLAND:
		{
#if defined (DEQP_SUPPORT_WAYLAND)
#endif
			break;
		}
		case TYPE_ANDROID:
		{
#if ( DE_OS == DE_OS_ANDROID )
#endif
			break;
		}
		case TYPE_WIN32:
		{
#if ( DE_OS == DE_OS_WIN32 )
			de::MovePtr<Window>					nullWindow		(display.createWindow(tcu::nothing<tcu::UVec2>()));
			const Win32WindowInterface&			win32Window		= dynamic_cast<const Win32WindowInterface&>(*nullWindow);
			HMONITOR							hMonitor		= (HMONITOR)MonitorFromWindow((HWND)win32Window.getNative().internal, MONITOR_DEFAULTTONEAREST);
			MONITORINFO							monitorInfo;
			monitorInfo.cbSize									= sizeof(MONITORINFO);
			GetMonitorInfo(hMonitor, &monitorInfo);
			result.x()											= deUint32(abs(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left));
			result.y()											= deUint32(abs(monitorInfo.rcMonitor.top - monitorInfo.rcMonitor.bottom));
#endif
			break;
		}

		case TYPE_MACOS:
		{
#if ( DE_OS == DE_OS_OSX )
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

Move<VkRenderPass> WsiTriangleRenderer::createRenderPass (const DeviceInterface&	vkd,
														  const VkDevice			device,
														  const VkFormat			colorAttachmentFormat,
														  const bool				explicitLayoutTransitions)
{
	const VkAttachmentDescription	colorAttDesc		=
	{
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
	const VkAttachmentReference		colorAttRef			=
	{
		0u,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	const VkSubpassDescription		subpassDesc			=
	{
		(VkSubpassDescriptionFlags)0u,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0u,							// inputAttachmentCount
		DE_NULL,					// pInputAttachments
		1u,							// colorAttachmentCount
		&colorAttRef,				// pColorAttachments
		DE_NULL,					// pResolveAttachments
		DE_NULL,					// depthStencilAttachment
		0u,							// preserveAttachmentCount
		DE_NULL,					// pPreserveAttachments
	};
	const VkSubpassDependency		dependencies[]		=
	{
		{
			VK_SUBPASS_EXTERNAL,	// srcSubpass
			0u,						// dstSubpass
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			VK_DEPENDENCY_BY_REGION_BIT
		},
		{
			0u,						// srcSubpass
			VK_SUBPASS_EXTERNAL,	// dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			VK_ACCESS_MEMORY_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT
		},
	};
	const VkRenderPassCreateInfo	renderPassParams	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		DE_NULL,
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

Move<VkPipelineLayout> WsiTriangleRenderer::createPipelineLayout (const DeviceInterface&	vkd,
																  const VkDevice			device)
{
	const VkPushConstantRange						pushConstantRange		=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		0u,											// offset
		(deUint32)sizeof(deUint32),					// size
	};
	const VkPipelineLayoutCreateInfo				pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,											// setLayoutCount
		DE_NULL,									// pSetLayouts
		1u,
		&pushConstantRange,
	};

	return vk::createPipelineLayout(vkd, device, &pipelineLayoutParams);
}

Move<VkPipeline> WsiTriangleRenderer::createPipeline (const DeviceInterface&	vkd,
													  const VkDevice			device,
													  const VkRenderPass		renderPass,
													  const VkPipelineLayout	pipelineLayout,
													  const BinaryCollection&	binaryCollection,
													  const tcu::UVec2&			renderSize)
{
	// \note VkShaderModules are fully consumed by vkCreateGraphicsPipelines()
	//		 and can be deleted immediately following that call.
	const Unique<VkShaderModule>					vertShaderModule		(createShaderModule(vkd, device, binaryCollection.get("tri-vert"), 0));
	const Unique<VkShaderModule>					fragShaderModule		(createShaderModule(vkd, device, binaryCollection.get("tri-frag"), 0));
	const std::vector<VkViewport>					viewports				(1, makeViewport(renderSize));
	const std::vector<VkRect2D>						scissors				(1, makeRect2D(renderSize));

	return vk::makeGraphicsPipeline(vkd,				// const DeviceInterface&            vk
									device,				// const VkDevice                    device
									pipelineLayout,		// const VkPipelineLayout            pipelineLayout
									*vertShaderModule,	// const VkShaderModule              vertexShaderModule
									DE_NULL,			// const VkShaderModule              tessellationControlShaderModule
									DE_NULL,			// const VkShaderModule              tessellationEvalShaderModule
									DE_NULL,			// const VkShaderModule              geometryShaderModule
									*fragShaderModule,	// const VkShaderModule              fragmentShaderModule
									renderPass,			// const VkRenderPass                renderPass
									viewports,			// const std::vector<VkViewport>&    viewports
									scissors);			// const std::vector<VkRect2D>&      scissors
}

Move<VkImageView> WsiTriangleRenderer::createAttachmentView (const DeviceInterface&	vkd,
															 const VkDevice			device,
															 const VkImage			image,
															 const VkFormat			format)
{
	const VkImageViewCreateInfo		viewParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		(VkImageViewCreateFlags)0,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		format,
		vk::makeComponentMappingRGBA(),
		{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0u,						// baseMipLevel
			1u,						// levelCount
			0u,						// baseArrayLayer
			1u,						// layerCount
		},
	};

	return vk::createImageView(vkd, device, &viewParams);
}

Move<VkFramebuffer> WsiTriangleRenderer::createFramebuffer	(const DeviceInterface&		vkd,
															 const VkDevice				device,
															 const VkRenderPass			renderPass,
															 const VkImageView			colorAttachment,
															 const tcu::UVec2&			renderSize)
{
	const VkFramebufferCreateInfo	framebufferParams	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		(VkFramebufferCreateFlags)0,
		renderPass,
		1u,
		&colorAttachment,
		renderSize.x(),
		renderSize.y(),
		1u,							// layers
	};

	return vk::createFramebuffer(vkd, device, &framebufferParams);
}

Move<VkBuffer> WsiTriangleRenderer::createBuffer (const DeviceInterface&	vkd,
												  VkDevice					device,
												  VkDeviceSize				size,
												  VkBufferUsageFlags		usage)
{
	const VkBufferCreateInfo	bufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(VkBufferCreateFlags)0,
		size,
		usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		DE_NULL
	};

	return vk::createBuffer(vkd, device, &bufferParams);
}

WsiTriangleRenderer::WsiTriangleRenderer (const DeviceInterface&	vkd,
										  const VkDevice			device,
										  Allocator&				allocator,
										  const BinaryCollection&	binaryRegistry,
										  bool						explicitLayoutTransitions,
										  const vector<VkImage>		swapchainImages,
										  const vector<VkImage>		aliasImages,
										  const VkFormat			framebufferFormat,
										  const tcu::UVec2&			renderSize)
	: m_vkd							(vkd)
	, m_explicitLayoutTransitions	(explicitLayoutTransitions)
	, m_swapchainImages				(swapchainImages)
	, m_aliasImages					(aliasImages)
	, m_renderSize					(renderSize)
	, m_renderPass					(createRenderPass(vkd, device, framebufferFormat, m_explicitLayoutTransitions))
	, m_pipelineLayout				(createPipelineLayout(vkd, device))
	, m_pipeline					(createPipeline(vkd, device, *m_renderPass, *m_pipelineLayout, binaryRegistry, renderSize))
	, m_vertexBuffer				(createBuffer(vkd, device, (VkDeviceSize)(sizeof(float)*4*3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
	, m_vertexBufferMemory			(allocator.allocate(getBufferMemoryRequirements(vkd, device, *m_vertexBuffer),
									 MemoryRequirement::HostVisible))
{
	m_attachmentViews.resize(swapchainImages.size());
	m_attachmentLayouts.resize(swapchainImages.size());
	m_framebuffers.resize(swapchainImages.size());

	for (size_t imageNdx = 0; imageNdx < swapchainImages.size(); ++imageNdx)
	{
		m_attachmentViews[imageNdx]		= ImageViewSp(new Unique<VkImageView>(createAttachmentView(vkd, device, swapchainImages[imageNdx], framebufferFormat)));
		m_attachmentLayouts[imageNdx]	= VK_IMAGE_LAYOUT_UNDEFINED;
		m_framebuffers[imageNdx]		= FramebufferSp(new Unique<VkFramebuffer>(createFramebuffer(vkd, device, *m_renderPass, **m_attachmentViews[imageNdx], renderSize)));
	}

	VK_CHECK(vkd.bindBufferMemory(device, *m_vertexBuffer, m_vertexBufferMemory->getMemory(), m_vertexBufferMemory->getOffset()));

	{
		const VkMappedMemoryRange	memRange	=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			DE_NULL,
			m_vertexBufferMemory->getMemory(),
			m_vertexBufferMemory->getOffset(),
			VK_WHOLE_SIZE
		};
		const tcu::Vec4				vertices[]	=
		{
			tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
			tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
			tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
		};
		DE_STATIC_ASSERT(sizeof(vertices) == sizeof(float)*4*3);

		deMemcpy(m_vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
		VK_CHECK(vkd.flushMappedMemoryRanges(device, 1u, &memRange));
	}
}

WsiTriangleRenderer::WsiTriangleRenderer (WsiTriangleRenderer&& other)
	: m_vkd					(other.m_vkd)
	, m_explicitLayoutTransitions	(other.m_explicitLayoutTransitions)
	, m_swapchainImages		(other.m_swapchainImages)
	, m_aliasImages			(other.m_aliasImages)
	, m_renderSize			(other.m_renderSize)
	, m_renderPass			(other.m_renderPass)
	, m_pipelineLayout		(other.m_pipelineLayout)
	, m_pipeline			(other.m_pipeline)
	, m_vertexBuffer		(other.m_vertexBuffer)
	, m_vertexBufferMemory	(other.m_vertexBufferMemory)
	, m_attachmentViews		(other.m_attachmentViews)
	, m_attachmentLayouts	(other.m_attachmentLayouts)
	, m_framebuffers		(other.m_framebuffers)
{
}

WsiTriangleRenderer::~WsiTriangleRenderer (void)
{
}

void WsiTriangleRenderer::recordFrame (VkCommandBuffer	cmdBuffer,
									   deUint32			imageNdx,
									   deUint32			frameNdx) const
{
	const VkFramebuffer	curFramebuffer	= **m_framebuffers[imageNdx];

	beginCommandBuffer(m_vkd, cmdBuffer, 0u);

	if (m_explicitLayoutTransitions || m_attachmentLayouts[imageNdx] == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		const auto range		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const auto newLayout	= (m_explicitLayoutTransitions ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		const auto srcStage		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		const auto srcMask		= 0u;
		const auto dstStage		= (m_explicitLayoutTransitions ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		const auto dstMask		= (m_explicitLayoutTransitions ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0);

		const auto barrier = makeImageMemoryBarrier(srcMask, dstMask, m_attachmentLayouts[imageNdx], newLayout, m_aliasImages[imageNdx], range);
		m_vkd.cmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);

		m_attachmentLayouts[imageNdx] = newLayout;
	}

	beginRenderPass(m_vkd, cmdBuffer, *m_renderPass, curFramebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), tcu::Vec4(0.125f, 0.25f, 0.75f, 1.0f));

	m_vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	{
		const VkDeviceSize bindingOffset = 0;
		m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &bindingOffset);
	}

	m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, (deUint32)sizeof(deUint32), &frameNdx);
	m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	endRenderPass(m_vkd, cmdBuffer);

	if (m_explicitLayoutTransitions)
	{
		VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		const VkImageMemoryBarrier	barrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
																		 m_attachmentLayouts[imageNdx], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
																		 m_aliasImages[imageNdx], range);
		m_vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
		m_attachmentLayouts[imageNdx] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}

	endCommandBuffer(m_vkd, cmdBuffer);
}

void WsiTriangleRenderer::recordDeviceGroupFrame (VkCommandBuffer	cmdBuffer,
												  deUint32			firstDeviceID,
												  deUint32			secondDeviceID,
												  deUint32			devicesCount,
												  deUint32			imageNdx,
												  deUint32			frameNdx) const
{
	const VkFramebuffer	curFramebuffer	= **m_framebuffers[imageNdx];

	beginCommandBuffer(m_vkd, cmdBuffer, 0u);

	if (m_explicitLayoutTransitions || m_attachmentLayouts[imageNdx] == VK_IMAGE_LAYOUT_UNDEFINED)
	{
		const auto range		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const auto newLayout	= (m_explicitLayoutTransitions ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		const auto srcStage		= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		const auto srcMask		= 0u;
		const auto dstStage		= (m_explicitLayoutTransitions ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		const auto dstMask		= (m_explicitLayoutTransitions ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0);

		const auto barrier = makeImageMemoryBarrier(srcMask, dstMask, m_attachmentLayouts[imageNdx], newLayout, m_aliasImages[imageNdx], range);
		m_vkd.cmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);

		m_attachmentLayouts[imageNdx] = newLayout;
	}

	// begin renderpass
	{
		const VkClearValue clearValue = makeClearValueColorF32(0.125f, 0.25f, 0.75f, 1.0f);

		VkRect2D zeroRect = { { 0, 0, },{ 0, 0, } };
		vector<VkRect2D> renderAreas;
		for (deUint32 i = 0; i < devicesCount; i++)
			renderAreas.push_back(zeroRect);

		// Render completely if there is only 1 device
		if (devicesCount == 1u)
		{
			renderAreas[0].extent.width = (deInt32)m_renderSize.x();
			renderAreas[0].extent.height = (deInt32)m_renderSize.y();
		}
		else
		{
			// Split into 2 vertical halves
			renderAreas[firstDeviceID].extent.width		= (deInt32)m_renderSize.x() / 2;
			renderAreas[firstDeviceID].extent.height	= (deInt32)m_renderSize.y();
			renderAreas[secondDeviceID]					= renderAreas[firstDeviceID];
			renderAreas[secondDeviceID].offset.x		= (deInt32)m_renderSize.x() / 2;
		}

		const VkDeviceGroupRenderPassBeginInfo deviceGroupRPBeginInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,
			DE_NULL,
			(deUint32)((1 << devicesCount) - 1),
			devicesCount,
			&renderAreas[0]
		};

		const VkRenderPassBeginInfo passBeginParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,						// sType
			&deviceGroupRPBeginInfo,										// pNext
			*m_renderPass,													// renderPass
			curFramebuffer,													// framebuffer
			{
				{ 0, 0 },
				{ m_renderSize.x(), m_renderSize.y() }
			},																// renderArea
			1u,																// clearValueCount
			&clearValue,													// pClearValues
		};
		m_vkd.cmdBeginRenderPass(cmdBuffer, &passBeginParams, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	{
		const VkDeviceSize bindingOffset = 0;
		m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &bindingOffset);
	}

	m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, (deUint32)sizeof(deUint32), &frameNdx);
	m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	endRenderPass(m_vkd, cmdBuffer);

	if (m_explicitLayoutTransitions)
	{
		VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		const VkImageMemoryBarrier	barrier	= makeImageMemoryBarrier	(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
																		 m_attachmentLayouts[imageNdx], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
																		 m_aliasImages[imageNdx], range);
		m_vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
		m_attachmentLayouts[imageNdx] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}

	endCommandBuffer(m_vkd, cmdBuffer);
}

void WsiTriangleRenderer::getPrograms (SourceCollections& dst)
{
	dst.glslSources.add("tri-vert") << glu::VertexSource(
		"#version 310 es\n"
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

} // wsi
} // vk
