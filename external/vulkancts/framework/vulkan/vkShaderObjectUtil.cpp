/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Test Case Utilities
 *//*--------------------------------------------------------------------*/

#include "vkShaderObjectUtil.hpp"
#include "vkQueryUtil.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC
inline Move<VkShaderEXT> createShader(const DeviceInterface& vk, VkDevice device, const vk::VkShaderCreateInfoEXT& shaderCreateInfo)
{
	VkShaderEXT object;
	VK_CHECK(vk.createShadersEXT(device, 1u, &shaderCreateInfo, DE_NULL, &object));
	return Move<VkShaderEXT>(check<VkShaderEXT>(object), Deleter<VkShaderEXT>(vk, device, DE_NULL));
}

std::vector<std::string> removeUnsupportedShaderObjectExtensions (const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physicalDevice, const std::vector<std::string>& deviceExtensions)
{
	std::vector<std::string> extensions = deviceExtensions;

	const auto extensionProperties = vk::enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	deUint32 discardRectanglesVersion = 0;
	deUint32 scissorExclusiveVersion = 0;
	for (const auto& extProp : extensionProperties)
	{
		if (strcmp(extProp.extensionName, "VK_EXT_discard_rectangles") == 0)
			discardRectanglesVersion = extProp.specVersion;

		if (strcmp(extProp.extensionName, "VK_NV_scissor_exclusive") == 0)
			scissorExclusiveVersion = extProp.specVersion;
	}

	if (discardRectanglesVersion < 2 && std::find(extensions.begin(), extensions.end(), "VK_EXT_discard_rectangles") != extensions.end())
		extensions.erase(std::remove(extensions.begin(), extensions.end(), "VK_EXT_discard_rectangles"), extensions.end());
	if (scissorExclusiveVersion < 2 && std::find(extensions.begin(), extensions.end(), "VK_NV_scissor_exclusive") != extensions.end())
		extensions.erase(std::remove(extensions.begin(), extensions.end(), "VK_NV_scissor_exclusive"), extensions.end());

	return extensions;
}
#endif

} // vk
