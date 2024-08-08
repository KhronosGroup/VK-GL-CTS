#ifndef _VKDEVICEUTIL_HPP
#define _VKDEVICEUTIL_HPP
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
 * \brief Instance and device initialization utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkDebugReportUtil.hpp"

#include <vector>
#include <string>

namespace tcu
{
class CommandLine;
}

namespace vk
{

Move<VkInstance> createDefaultInstance(const PlatformInterface &vkPlatform, uint32_t apiVersion,
                                       const tcu::CommandLine &cmdLine);

Move<VkInstance> createDefaultInstance(const PlatformInterface &vkPlatform, uint32_t apiVersion,
                                       const std::vector<std::string> &enabledLayers,
                                       const std::vector<std::string> &enabledExtensions,
                                       const tcu::CommandLine &cmdLine,
#ifndef CTS_USES_VULKANSC
                                       DebugReportRecorder *recorder = nullptr,
#endif // CTS_USES_VULKANSC
                                       const VkAllocationCallbacks *pAllocator = nullptr);

uint32_t chooseDeviceIndex(const InstanceInterface &vkInstance, const VkInstance instance,
                           const tcu::CommandLine &cmdLine);

VkPhysicalDevice chooseDevice(const InstanceInterface &vkInstance, const VkInstance instance,
                              const tcu::CommandLine &cmdLine);

} // namespace vk

#endif // _VKDEVICEUTIL_HPP
