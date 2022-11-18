#ifndef _VKTPIPELINEMAKEUTIL_HPP
#define _VKTPIPELINEMAKEUTIL_HPP
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
#include "vkMemUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuVector.hpp"

namespace vkt
{
namespace pipeline
{

vk::Move<vk::VkCommandBuffer>	makeCommandBuffer		(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkCommandPool commandPool);
de::MovePtr<vk::Allocation>		bindImageDedicated		(const vk::InstanceInterface& vki, const vk::DeviceInterface& vkd, const vk::VkPhysicalDevice physDevice, const vk::VkDevice device, const vk::VkImage image, const vk::MemoryRequirement requirement);
de::MovePtr<vk::Allocation>		bindBufferDedicated		(const vk::InstanceInterface& vki, const vk::DeviceInterface& vkd, const vk::VkPhysicalDevice physDevice, const vk::VkDevice device, const vk::VkBuffer buffer, const vk::MemoryRequirement requirement);

template<typename T>
inline const T* dataOrNullPtr(const std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

template<typename T>
inline T* dataOrNullPtr(std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

} // pipeline
} // vkt

#endif // _VKTPIPELINEMAKEUTIL_HPP
