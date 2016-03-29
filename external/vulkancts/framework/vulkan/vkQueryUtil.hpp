#ifndef _VKQUERYUTIL_HPP
#define _VKQUERYUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Vulkan query utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "deMemory.h"

#include <vector>

namespace vk
{

std::vector<VkPhysicalDevice>			enumeratePhysicalDevices				(const InstanceInterface& vk, VkInstance instance);
std::vector<VkQueueFamilyProperties>	getPhysicalDeviceQueueFamilyProperties	(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceFeatures				getPhysicalDeviceFeatures				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceProperties				getPhysicalDeviceProperties				(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkPhysicalDeviceMemoryProperties		getPhysicalDeviceMemoryProperties		(const InstanceInterface& vk, VkPhysicalDevice physicalDevice);
VkFormatProperties						getPhysicalDeviceFormatProperties		(const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format);
VkImageFormatProperties					getPhysicalDeviceImageFormatProperties	(const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags);

VkMemoryRequirements					getBufferMemoryRequirements				(const DeviceInterface& vk, VkDevice device, VkBuffer buffer);
VkMemoryRequirements					getImageMemoryRequirements				(const DeviceInterface& vk, VkDevice device, VkImage image);

std::vector<VkLayerProperties>			enumerateInstanceLayerProperties		(const PlatformInterface& vkp);
std::vector<VkExtensionProperties>		enumerateInstanceExtensionProperties	(const PlatformInterface& vkp, const char* layerName);
std::vector<VkLayerProperties>			enumerateDeviceLayerProperties			(const InstanceInterface& vki, VkPhysicalDevice physicalDevice);
std::vector<VkExtensionProperties>		enumerateDeviceExtensionProperties		(const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const char* layerName);

bool									isShaderStageSupported					(const VkPhysicalDeviceFeatures& deviceFeatures, VkShaderStageFlagBits stage);

template <typename Context, typename Interface, typename Type>
bool validateInitComplete(Context context, void (Interface::*Function)(Context, Type*)const, const Interface& interface)
{
	Type vec[2];
	deMemset(&vec[0], 0x00, sizeof(Type));
	deMemset(&vec[1], 0xFF, sizeof(Type));

	(interface.*Function)(context, &vec[0]);
	(interface.*Function)(context, &vec[1]);

	for (size_t ndx = 0; ndx < sizeof(Type); ndx++)
	{
		if (reinterpret_cast<deUint8*>(&vec[0])[ndx] != reinterpret_cast<deUint8*>(&vec[1])[ndx])
			return false;
	}

	return true;
}

} // vk

#endif // _VKQUERYUTIL_HPP
