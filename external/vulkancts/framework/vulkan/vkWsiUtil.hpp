#ifndef _VKWSIUTIL_HPP
#define _VKWSIUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Windowing System Integration (WSI) Utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkWsiPlatform.hpp"
#include "vkRef.hpp"

#include <vector>

namespace vk
{
namespace wsi
{

struct PlatformProperties
{
	enum FeatureFlags
	{
		FEATURE_INITIAL_WINDOW_SIZE		= (1<<0),		//!< Platform honors initial window size request
		FEATURE_RESIZE_WINDOW			= (1<<1),		//!< Platform supports resizing window
	};

	enum SwapchainExtent
	{
		SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE = 0,	//!< Swapchain extent must match window size
		SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE,				//!< Window will be resized to swapchain size when first image is presented
		SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE,			//!< Presented image contents will be scaled to window size

		SWAPCHAIN_EXTENT_LAST
	};

	deUint32		features;
	SwapchainExtent	swapchainExtent;
	deUint32		maxDisplays;
	deUint32		maxWindowsPerDisplay;
};

const char*						getName									(Type wsiType);
const char*						getExtensionName						(Type wsiType);

const PlatformProperties&		getPlatformProperties					(Type wsiType);

VkResult						createSurface							(const InstanceInterface&		vki,
																		 VkInstance						instance,
																		 Type							wsiType,
																		 const Display&					nativeDisplay,
																		 const Window&					nativeWindow,
																		 const VkAllocationCallbacks*	pAllocator,
																		 VkSurfaceKHR*					pSurface);

Move<VkSurfaceKHR>				createSurface							(const InstanceInterface&		vki,
																		 VkInstance						instance,
																		 Type							wsiType,
																		 const Display&					nativeDisplay,
																		 const Window&					nativeWindow,
																		 const VkAllocationCallbacks*	pAllocator = DE_NULL);

VkBool32						getPhysicalDeviceSurfaceSupport			(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 deUint32						queueFamilyIndex,
																		 VkSurfaceKHR					surface);

VkSurfaceCapabilitiesKHR		getPhysicalDeviceSurfaceCapabilities	(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

std::vector<VkSurfaceFormatKHR>	getPhysicalDeviceSurfaceFormats			(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

std::vector<VkPresentModeKHR>	getPhysicalDeviceSurfacePresentModes	(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

} // wsi
} // vk

#endif // _VKWSIUTIL_HPP
