#ifndef _VKSHADEROBJECTUTIL_HPP
#define _VKSHADEROBJECTUTIL_HPP
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

#include "vkDefs.hpp"
#include "vkRef.hpp"

#include <string>
#include <vector>

namespace vk
{

#ifndef CTS_USES_VULKANSC
Move<VkShaderEXT> createShader (const DeviceInterface& vk, VkDevice device, const vk::VkShaderCreateInfoEXT& shaderCreateInfo);
std::vector<std::string> removeUnsupportedShaderObjectExtensions (const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physicalDevice, const std::vector<std::string>& deviceExtensions);
#endif

} // vk

#endif // _VKSHADEROBJECTUTIL_HPP
