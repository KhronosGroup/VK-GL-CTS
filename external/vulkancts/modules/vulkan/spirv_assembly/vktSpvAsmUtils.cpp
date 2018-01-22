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

namespace
{

VkPhysicalDeviceFeatures filterDefaultDeviceFeatures (const VkPhysicalDeviceFeatures& deviceFeatures)
{
	VkPhysicalDeviceFeatures enabledDeviceFeatures = deviceFeatures;

	// Disable robustness by default, as it has an impact on performance on some HW.
	enabledDeviceFeatures.robustBufferAccess = false;

	return enabledDeviceFeatures;
}

VkPhysicalDevice8BitStorageFeaturesKHR	querySupported8BitStorageFeatures (const deUint32 apiVersion, const InstanceInterface& vki, VkPhysicalDevice device, const std::vector<std::string>& instanceExtensions)
{
	VkPhysicalDevice8BitStorageFeaturesKHR	extensionFeatures	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR,	// VkStructureType	sType;
		DE_NULL,														// void*			pNext;
		false,															// VkBool32			storageBuffer8BitAccess;
		false,															// VkBool32			uniformAndStorageBuffer8BitAccess;
		false,															// VkBool32			storagePushConstant8;
	};
	VkPhysicalDeviceFeatures2			features;

	deMemset(&features, 0, sizeof(features));
	features.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext	= &extensionFeatures;

	// Call the getter only if supported. Otherwise above "zero" defaults are used
	if(isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		vki.getPhysicalDeviceFeatures2(device, &features);
	}

	return extensionFeatures;
}

VkPhysicalDevice16BitStorageFeatures	querySupported16BitStorageFeatures (const deUint32 apiVersion, const InstanceInterface& vki, VkPhysicalDevice device, const std::vector<std::string>& instanceExtensions)
{
	VkPhysicalDevice16BitStorageFeatures	extensionFeatures	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR,	// sType
		DE_NULL,														// pNext
		false,															// storageUniformBufferBlock16
		false,															// storageUniform16
		false,															// storagePushConstant16
		false,															// storageInputOutput16
	};
	VkPhysicalDeviceFeatures2			features;

	deMemset(&features, 0, sizeof(features));
	features.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext	= &extensionFeatures;

	// Call the getter only if supported. Otherwise above "zero" defaults are used
	if(isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		vki.getPhysicalDeviceFeatures2(device, &features);
	}

	return extensionFeatures;
}

VkPhysicalDeviceVariablePointerFeatures querySupportedVariablePointersFeatures (const deUint32 apiVersion, const InstanceInterface& vki, VkPhysicalDevice device, const std::vector<std::string>& instanceExtensions)
{
	VkPhysicalDeviceVariablePointerFeatures extensionFeatures	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR,	// sType
		DE_NULL,															// pNext
		false,																// variablePointersStorageBuffer
		false,																// variablePointers
	};

	VkPhysicalDeviceFeatures2	features;
	deMemset(&features, 0, sizeof(features));
	features.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext	= &extensionFeatures;

	// Call the getter only if supported. Otherwise above "zero" defaults are used
	if(isInstanceExtensionSupported(apiVersion, instanceExtensions, "VK_KHR_get_physical_device_properties2"))
	{
		vki.getPhysicalDeviceFeatures2(device, &features);
	}

	return extensionFeatures;
}

} // anonymous

bool is8BitStorageFeaturesSupported (const deUint32 apiVersion, const InstanceInterface& vki, VkPhysicalDevice device, const std::vector<std::string>& instanceExtensions, Extension8BitStorageFeatures toCheck)
{
	VkPhysicalDevice8BitStorageFeaturesKHR extensionFeatures	= querySupported8BitStorageFeatures(apiVersion, vki, device, instanceExtensions);

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

Move<VkDevice> createDeviceWithExtensions (Context&							context,
										   const deUint32					queueFamilyIndex,
										   const std::vector<std::string>&	supportedExtensions,
										   const std::vector<std::string>&	requiredExtensions)
{
	const InstanceInterface&					vki							= context.getInstanceInterface();
	const VkPhysicalDevice						physicalDevice				= context.getPhysicalDevice();
	std::vector<const char*>					extensions;
	void*										pExtension					= DE_NULL;
	const VkPhysicalDeviceFeatures				deviceFeatures				= getPhysicalDeviceFeatures(vki, physicalDevice);
	VkPhysicalDevice16BitStorageFeatures		ext16BitStorageFeatures;
	VkPhysicalDeviceVariablePointerFeatures		extVariablePointerFeatures;

	for (deUint32 extNdx = 0; extNdx < requiredExtensions.size(); ++extNdx)
	{
		const std::string&	ext = requiredExtensions[extNdx];

		// Check that all required extensions are supported first.
		if (!isDeviceExtensionSupported(context.getUsedApiVersion(), supportedExtensions, ext))
		{
			TCU_THROW(NotSupportedError, (std::string("Device extension not supported: ") + ext).c_str());
		}

		// Currently don't support enabling multiple extensions at the same time.
		if (ext == "VK_KHR_16bit_storage")
		{
			// For the 16bit storage extension, we have four features to test. Requesting all features supported.
			// Note that we don't throw NotImplemented errors here if a specific feature is not supported;
			// that should be done when actually trying to use that specific feature.
			ext16BitStorageFeatures	= querySupported16BitStorageFeatures(context.getUsedApiVersion(), vki, physicalDevice, context.getInstanceExtensions());
			pExtension = &ext16BitStorageFeatures;
		}
		else if (ext == "VK_KHR_variable_pointers")
		{
			// For the VariablePointers extension, we have two features to test. Requesting all features supported.
			extVariablePointerFeatures	= querySupportedVariablePointersFeatures(context.getUsedApiVersion(), vki, physicalDevice, context.getInstanceExtensions());
			pExtension = &extVariablePointerFeatures;
		}

		if (!isCoreDeviceExtension(context.getUsedApiVersion(), ext))
			extensions.push_back(ext.c_str());
	}

	const float						queuePriorities[]	= { 1.0f };
	const VkDeviceQueueCreateInfo	queueInfos[]		=
	{
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
			(VkDeviceQueueCreateFlags)0,
			queueFamilyIndex,
			DE_LENGTH_OF_ARRAY(queuePriorities),
			&queuePriorities[0]
		}
	};
	const VkPhysicalDeviceFeatures	features			= filterDefaultDeviceFeatures(deviceFeatures);
	const VkDeviceCreateInfo		deviceParams		=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		pExtension,
		(VkDeviceCreateFlags)0,
		DE_LENGTH_OF_ARRAY(queueInfos),
		&queueInfos[0],
		0u,
		DE_NULL,
		(deUint32)extensions.size(),
		extensions.empty() ? DE_NULL : &extensions[0],
		&features
	};

	return vk::createDevice(context.getPlatformInterface(), context.getInstance(), vki, physicalDevice, &deviceParams);
}

Allocator* createAllocator (const InstanceInterface& instanceInterface, const VkPhysicalDevice physicalDevice, const DeviceInterface& deviceInterface, const VkDevice device)
{
	const VkPhysicalDeviceMemoryProperties memoryProperties = getPhysicalDeviceMemoryProperties(instanceInterface, physicalDevice);

	// \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
	return new SimpleAllocator(deviceInterface, device, memoryProperties);
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
