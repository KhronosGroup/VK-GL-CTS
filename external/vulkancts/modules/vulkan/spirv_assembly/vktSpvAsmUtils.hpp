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

enum Extension8BitStorageFeatureBits
{
	EXT8BITSTORAGEFEATURES_STORAGE_BUFFER			= (1u << 1),
	EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER	= (1u << 2),
	EXT8BITSTORAGEFEATURES_PUSH_CONSTANT			= (1u << 3),
};
typedef deUint32 Extension8BitStorageFeatures;

enum Extension16BitStorageFeatureBits
{
	EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK	= (1u << 1),
	EXT16BITSTORAGEFEATURES_UNIFORM					= (1u << 2),
	EXT16BITSTORAGEFEATURES_PUSH_CONSTANT			= (1u << 3),
	EXT16BITSTORAGEFEATURES_INPUT_OUTPUT			= (1u << 4),
};
typedef deUint32 Extension16BitStorageFeatures;

enum ExtensionVariablePointersFeaturesBits
{
	EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER	= (1u << 1),
	EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS				= (1u << 2),
};
typedef deUint32 ExtensionVariablePointersFeatures;

struct VulkanFeatures
{
	Extension8BitStorageFeatures		ext8BitStorage;
	Extension16BitStorageFeatures		ext16BitStorage;
	ExtensionVariablePointersFeatures	extVariablePointers;
	vk::VkPhysicalDeviceFeatures		coreFeatures;

	VulkanFeatures				(void)
		: ext8BitStorage		(0)
		, ext16BitStorage		(0)
		, extVariablePointers	(0)
		, coreFeatures			(vk::VkPhysicalDeviceFeatures())
	{}
};

// Returns true if the given 8bit storage extension features in `toCheck` are all supported.
bool is8BitStorageFeaturesSupported (const Context&						context,
									  Extension8BitStorageFeatures		toCheck);

// Returns true if the given 16bit storage extension features in `toCheck` are all supported.
bool is16BitStorageFeaturesSupported (const Context&				context,
									  Extension16BitStorageFeatures	toCheck);

// Returns true if the given variable pointers extension features in `toCheck` are all supported.
bool isVariablePointersFeaturesSupported (const Context&					context,
										  ExtensionVariablePointersFeatures	toCheck);

deUint32 getMinRequiredVulkanVersion (const vk::SpirvVersion version);

std::string	getVulkanName (const deUint32 version);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMUTILS_HPP
