#ifndef _VKQUERYUTIL_HPP
#define _VKQUERYUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by
 * Khronos, at which point this condition clause shall be removed.
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
 * \brief Vulkan query utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include <vector>

namespace vk
{

std::vector<VkPhysicalDevice>			enumeratePhysicalDevices				(const InstanceInterface& vk, VkInstance instance);
std::vector<VkQueueFamilyProperties>	getPhysicalDeviceQueueFamilyProperties	(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceFeatures				getPhysicalDeviceFeatures				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceMemoryProperties		getPhysicalDeviceMemoryProperties		(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);

VkMemoryRequirements					getBufferMemoryRequirements				(const DeviceInterface& vk, VkDevice device, VkBuffer buffer);
VkMemoryRequirements					getImageMemoryRequirements				(const DeviceInterface& vk, VkDevice device, VkImage image);

std::vector<VkLayerProperties>			enumerateInstanceLayerProperties		(const PlatformInterface& vkp);
std::vector<VkExtensionProperties>		enumerateInstanceExtensionProperties	(const PlatformInterface& vkp, const char* layerName);
std::vector<VkLayerProperties>			enumerateDeviceLayerProperties			(const InstanceInterface& vki, VkPhysicalDevice physicalDevice);
std::vector<VkExtensionProperties>		enumerateDeviceExtensionProperties		(const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const char* layerName);

bool									isShaderStageSupported					(const VkPhysicalDeviceFeatures& deviceFeatures, VkShaderStage stage);

} // vk

#endif // _VKQUERYUTIL_HPP
