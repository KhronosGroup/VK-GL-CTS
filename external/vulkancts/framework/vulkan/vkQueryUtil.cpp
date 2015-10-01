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

std::vector<VkPhysicalDevice> enumeratePhysicalDevices (const InstanceInterface& vk, VkInstance instance)
{
	deUint32						numDevices	= 0;
	std::vector<VkPhysicalDevice>	devices;

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

std::vector<VkPhysicalDeviceQueueProperties> getPhysicalDeviceQueueProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	deUint32										numQueues	= 0;
	std::vector<VkPhysicalDeviceQueueProperties>	properties;

	VK_CHECK(vk.getPhysicalDeviceQueueCount(physicalDevice, &numQueues));

	if (numQueues > 0)
	{
		properties.resize(numQueues);
		VK_CHECK(vk.getPhysicalDeviceQueueProperties(physicalDevice, numQueues, &properties[0]));
	}

	return properties;
}

VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties (const InstanceInterface& vk, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceMemoryProperties	properties;

	deMemset(&properties, 0, sizeof(properties));

	VK_CHECK(vk.getPhysicalDeviceMemoryProperties(physicalDevice, &properties));
	return properties;
}

VkMemoryRequirements getBufferMemoryRequirements (const DeviceInterface& vk, VkDevice device, VkBuffer buffer)
{
	VkMemoryRequirements req;
	VK_CHECK(vk.getBufferMemoryRequirements(device, buffer, &req));
	return req;
}
VkMemoryRequirements getImageMemoryRequirements (const DeviceInterface& vk, VkDevice device, VkImage image)
{
	VkMemoryRequirements req;
	VK_CHECK(vk.getImageMemoryRequirements(device, image, &req));
	return req;
}

} // vk
