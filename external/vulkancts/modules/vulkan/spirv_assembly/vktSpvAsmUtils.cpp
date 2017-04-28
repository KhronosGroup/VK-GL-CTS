/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Utilities for Vulkan SPIR-V assembly tests
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmUtils.hpp"

#include "deMemory.h"
#include "deSTLUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;

bool is16BitStorageFeaturesSupported (const InstanceInterface& vki, VkPhysicalDevice device, const std::vector<std::string>& instanceExtensions, Extension16BitStorageFeatures toCheck)
{
	VkPhysicalDevice16BitStorageFeaturesKHR	extensionFeatures	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,	// sType
		DE_NULL,														// pNext
		false,															// storageUniformBufferBlock16
		false,															// storageUniform16
		false,															// storagePushConstant16
		false,															// storageInputOutput16
	};
	VkPhysicalDeviceFeatures2KHR			features;

	deMemset(&features, 0, sizeof(features));
	features.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
	features.pNext	= &extensionFeatures;

	// Call the getter only if supported. Otherwise above "zero" defaults are used
	if (de::contains(instanceExtensions.begin(), instanceExtensions.end(), "VK_KHR_get_physical_device_properties2"))
	{
		vki.getPhysicalDeviceFeatures2KHR(device, &features);
	}

	if ((toCheck & EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK) != 0 && extensionFeatures.storageUniformBufferBlock16 == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_UNIFORM) != 0 && extensionFeatures.storageUniform16 == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_PUSH_CONSTANT) != 0 && extensionFeatures.storagePushConstant16 == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_INPUT_OUTPUT) != 0 && extensionFeatures.storageInputOutput16 == VK_FALSE)
		return false;

	return true;
}

} // SpirVAssembly
} // vkt
