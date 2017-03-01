#ifndef _VKTSPVASMUTILS_HPP
#define _VKTSPVASMUTILS_HPP
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

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace SpirVAssembly
{

enum Extension16BitStorageFeatureBits
{
	EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK	= (1u << 1),
	EXT16BITSTORAGEFEATURES_UNIFORM					= (1u << 2),
	EXT16BITSTORAGEFEATURES_PUSH_CONSTANT			= (1u << 3),
	EXT16BITSTORAGEFEATURES_INPUT_OUTPUT			= (1u << 4),
};
typedef deUint32 Extension16BitStorageFeatures;

struct ExtensionFeatures
{
	Extension16BitStorageFeatures	ext16BitStorage;

	ExtensionFeatures	(void)
		: ext16BitStorage	(0)
	{}
	explicit ExtensionFeatures	(Extension16BitStorageFeatures	ext16BitStorage_)
		: ext16BitStorage	(ext16BitStorage_)
	{}
};

// Returns true if the given 16bit storage extension features in `toCheck` are all supported.
bool is16BitStorageFeaturesSupported (const vk::InstanceInterface&	vkInstance,
									  vk::VkPhysicalDevice			device,
									  const std::vector<std::string>& instanceExtensions,
									  Extension16BitStorageFeatures	toCheck);

// Creates a Vulkan logical device with the requiredExtensions enabled and all other extensions disabled.
// The logical device will be created from the instance and physical device in the given context.
// A single queue will be created from the given queue family.
vk::Move<vk::VkDevice> createDeviceWithExtensions (Context&							context,
												   deUint32							queueFamilyIndex,
												   const std::vector<std::string>&	supportedExtensions,
												   const std::vector<std::string>&	requiredExtensions);

// Creates a SimpleAllocator on the given device.
vk::Allocator* createAllocator (const vk::InstanceInterface& instanceInterface,
								const vk::VkPhysicalDevice physicalDevice,
								const vk::DeviceInterface& deviceInterface,
								const vk::VkDevice device);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMUTILS_HPP
