#ifndef _VKAPPPARAMSUTIL_HPP
#define _VKAPPPARAMSUTIL_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2022 NVIDIA CORPORATION, Inc.
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
 * \brief Vulkan SC utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "tcuCommandLine.hpp"
#include <vector>

#ifdef CTS_USES_VULKANSC

namespace vk
{

bool readApplicationParameters (std::vector<VkApplicationParametersEXT>& appParams, const tcu::CommandLine& cmdLine, const bool readInstanceAppParams);

} //vk

#endif // CTS_USES_VULKANSC

#endif // _VKAPPPARAMSUTIL_HPP
