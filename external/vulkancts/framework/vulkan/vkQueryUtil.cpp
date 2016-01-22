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

#include "vkQueryUtil.hpp"
#include "deMemory.h"

namespace vk
{

using std::vector;

vector<VkPhysicalDevice> enumeratePhysicalDevices (const InstanceInterface& vk, VkInstance instance)
{
	deUint32					numDevices	= 0;
	vector<VkPhysicalDevice>	devices;

	VK_CHECK(vk.enumeratePhysicalDevices(instance, &numDevices, DE_NULL));

	if (numDevices > 0)
	{
		devices.resize(numDevices);
		VK_CHECK(vk.enumeratePhysicalDevices(instance, &numDevices, &devices[0]));

		if ((size_t)numDevices != devices.size())
			TCU_FAIL("Returned device count changed between queries");
	}

	return devices;
}

vector<VkQueueFamilyProperties> getPhysicalDeviceQueueFamilyProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	deUint32						numQueues	= 0;
	vector<VkQueueFamilyProperties>	properties;

	vk.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, DE_NULL);

	if (numQueues > 0)
	{
		properties.resize(numQueues);
		vk.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, &properties[0]);

		if ((size_t)numQueues != properties.size())
			TCU_FAIL("Returned queue family count changes between queries");
	}

	return properties;
}

VkPhysicalDeviceFeatures getPhysicalDeviceFeatures (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceFeatures	features;

	deMemset(&features, 0, sizeof(features));

	vk.getPhysicalDeviceFeatures(physicalDevice, &features);
	return features;
}

VkPhysicalDeviceProperties getPhysicalDeviceProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceProperties	properties;

	deMemset(&properties, 0, sizeof(properties));

	vk.getPhysicalDeviceProperties(physicalDevice, &properties);
	return properties;
}

VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceMemoryProperties	properties;

	deMemset(&properties, 0, sizeof(properties));

	vk.getPhysicalDeviceMemoryProperties(physicalDevice, &properties);
	return properties;
}

VkFormatProperties getPhysicalDeviceFormatProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format)
{
	VkFormatProperties	properties;

	deMemset(&properties, 0, sizeof(properties));

	vk.getPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
	return properties;
}

VkImageFormatProperties getPhysicalDeviceImageFormatProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags)
{
	VkImageFormatProperties	properties;

	deMemset(&properties, 0, sizeof(properties));

	VK_CHECK(vk.getPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags, &properties));
	return properties;
}

VkMemoryRequirements getBufferMemoryRequirements (const DeviceInterface& vk, VkDevice device, VkBuffer buffer)
{
	VkMemoryRequirements req;
	vk.getBufferMemoryRequirements(device, buffer, &req);
	return req;
}

VkMemoryRequirements getImageMemoryRequirements (const DeviceInterface& vk, VkDevice device, VkImage image)
{
	VkMemoryRequirements req;
	vk.getImageMemoryRequirements(device, image, &req);
	return req;
}

vector<VkLayerProperties> enumerateInstanceLayerProperties (const PlatformInterface& vkp)
{
	vector<VkLayerProperties>	properties;
	deUint32					numLayers	= 0;

	VK_CHECK(vkp.enumerateInstanceLayerProperties(&numLayers, DE_NULL));

	if (numLayers > 0)
	{
		properties.resize(numLayers);
		VK_CHECK(vkp.enumerateInstanceLayerProperties(&numLayers, &properties[0]));
		TCU_CHECK((size_t)numLayers == properties.size());
	}

	return properties;
}

vector<VkExtensionProperties> enumerateInstanceExtensionProperties (const PlatformInterface& vkp, const char* layerName)
{
	vector<VkExtensionProperties>	properties;
	deUint32						numExtensions	= 0;

	VK_CHECK(vkp.enumerateInstanceExtensionProperties(layerName, &numExtensions, DE_NULL));

	if (numExtensions > 0)
	{
		properties.resize(numExtensions);
		VK_CHECK(vkp.enumerateInstanceExtensionProperties(layerName, &numExtensions, &properties[0]));
		TCU_CHECK((size_t)numExtensions == properties.size());
	}

	return properties;
}

vector<VkLayerProperties> enumerateDeviceLayerProperties (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
{
	vector<VkLayerProperties>	properties;
	deUint32					numLayers	= 0;

	VK_CHECK(vki.enumerateDeviceLayerProperties(physicalDevice, &numLayers, DE_NULL));

	if (numLayers > 0)
	{
		properties.resize(numLayers);
		VK_CHECK(vki.enumerateDeviceLayerProperties(physicalDevice, &numLayers, &properties[0]));
		TCU_CHECK((size_t)numLayers == properties.size());
	}

	return properties;
}

vector<VkExtensionProperties> enumerateDeviceExtensionProperties (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, const char* layerName)
{
	vector<VkExtensionProperties>	properties;
	deUint32						numExtensions	= 0;

	VK_CHECK(vki.enumerateDeviceExtensionProperties(physicalDevice, layerName, &numExtensions, DE_NULL));

	if (numExtensions > 0)
	{
		properties.resize(numExtensions);
		VK_CHECK(vki.enumerateDeviceExtensionProperties(physicalDevice, layerName, &numExtensions, &properties[0]));
		TCU_CHECK((size_t)numExtensions == properties.size());
	}

	return properties;
}

bool isShaderStageSupported (const VkPhysicalDeviceFeatures& deviceFeatures, VkShaderStageFlagBits stage)
{
	if (stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		return deviceFeatures.tessellationShader == VK_TRUE;
	else if (stage == VK_SHADER_STAGE_GEOMETRY_BIT)
		return deviceFeatures.geometryShader == VK_TRUE;
	else
		return true;
}

} // vk
