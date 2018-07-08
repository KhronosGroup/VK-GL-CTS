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
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;

bool is8BitStorageFeaturesSupported (const Context& context, Extension8BitStorageFeatures toCheck)
{
	VkPhysicalDevice8BitStorageFeaturesKHR extensionFeatures = context.get8BitStorageFeatures();

	if ((toCheck & EXT8BITSTORAGEFEATURES_STORAGE_BUFFER) != 0 && extensionFeatures.storageBuffer8BitAccess == VK_FALSE)
		TCU_FAIL("storageBuffer8BitAccess has to be supported");

	if ((toCheck & EXT8BITSTORAGEFEATURES_UNIFORM_STORAGE_BUFFER) != 0 && extensionFeatures.uniformAndStorageBuffer8BitAccess == VK_FALSE)
		return false;

	if ((toCheck & EXT8BITSTORAGEFEATURES_PUSH_CONSTANT) != 0 && extensionFeatures.storagePushConstant8 == VK_FALSE)
		return false;

	return true;
}

bool is16BitStorageFeaturesSupported (const Context& context, Extension16BitStorageFeatures toCheck)
{
	const VkPhysicalDevice16BitStorageFeatures& extensionFeatures = context.get16BitStorageFeatures();

	if ((toCheck & EXT16BITSTORAGEFEATURES_UNIFORM_BUFFER_BLOCK) != 0 && extensionFeatures.storageBuffer16BitAccess == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_UNIFORM) != 0 && extensionFeatures.uniformAndStorageBuffer16BitAccess == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_PUSH_CONSTANT) != 0 && extensionFeatures.storagePushConstant16 == VK_FALSE)
		return false;

	if ((toCheck & EXT16BITSTORAGEFEATURES_INPUT_OUTPUT) != 0 && extensionFeatures.storageInputOutput16 == VK_FALSE)
		return false;

	return true;
}

bool isVariablePointersFeaturesSupported (const Context& context, ExtensionVariablePointersFeatures toCheck)
{
	const VkPhysicalDeviceVariablePointerFeatures& extensionFeatures = context.getVariablePointerFeatures();

	if ((toCheck & EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER) != 0 && extensionFeatures.variablePointersStorageBuffer == VK_FALSE)
		return false;

	if ((toCheck & EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS) != 0 && extensionFeatures.variablePointers == VK_FALSE)
		return false;

	return true;
}

deUint32 getMinRequiredVulkanVersion (const SpirvVersion version)
{
	switch(version)
	{
	case SPIRV_VERSION_1_0:
		return VK_API_VERSION_1_0;
	case SPIRV_VERSION_1_1:
	case SPIRV_VERSION_1_2:
	case SPIRV_VERSION_1_3:
		return VK_API_VERSION_1_1;
	default:
		DE_ASSERT(0);
	}
	return 0u;
}

std::string	getVulkanName (const deUint32 version)
{
	return std::string(version == VK_API_VERSION_1_1 ? "1.1" : "1.0");
}

} // SpirVAssembly
} // vkt
