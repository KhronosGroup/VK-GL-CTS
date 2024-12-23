#ifndef _VKTPOSTMORTEMTESTSUTILS_HPP
#define _VKTPOSTMORTEMTESTSUTILS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Postmortem Tests Utils
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace postmortem
{

vk::VkResult getDeviceFaultInfoKHR(vk::VkDevice device, vk::VkDeviceFaultCountsEXT *pFaultCounts,
                                   vk::VkDeviceFaultInfoEXT *pFaultInfo);

} // namespace postmortem
} // namespace vkt

#endif // _VKTPOSTMORTEMTESTSUTILS_HPP