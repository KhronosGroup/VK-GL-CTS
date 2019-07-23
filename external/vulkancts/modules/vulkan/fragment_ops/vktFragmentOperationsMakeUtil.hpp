#ifndef _VKTFRAGMENTOPERATIONSMAKEUTIL_HPP
#define _VKTFRAGMENTOPERATIONSMAKEUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Object creation utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkObjUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuVector.hpp"

namespace vkt
{
namespace FragmentOperations
{

vk::Move<vk::VkPipeline>		makeComputePipeline		(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkShaderModule shaderModule, const vk::VkSpecializationInfo* specInfo);

} // FragmentOperations
} // vkt

#endif // _VKTFRAGMENTOPERATIONSMAKEUTIL_HPP
