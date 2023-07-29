#ifndef _VKTAPIBUFFERCOMPUTEINSTANCE_HPP
#define _VKTAPIBUFFERCOMPUTEINSTANCE_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVectorType.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace api
{

vk::Move<vk::VkBuffer> createDataBuffer(vkt::Context &context, uint32_t offset, uint32_t bufferSize, uint32_t initData,
                                        uint32_t initDataSize, uint32_t uninitData,
                                        de::MovePtr<vk::Allocation> *outAllocation);

vk::Move<vk::VkBuffer> createColorDataBuffer(uint32_t offset, uint32_t bufferSize, const tcu::Vec4 &color1,
                                             const tcu::Vec4 &color2, de::MovePtr<vk::Allocation> *outAllocation,
                                             vkt::Context &context);

vk::Move<vk::VkDescriptorSetLayout> createDescriptorSetLayout(vkt::Context &context);

vk::Move<vk::VkDescriptorPool> createDescriptorPool(vkt::Context &context);

vk::Move<vk::VkDescriptorSet> createDescriptorSet(vkt::Context &context, vk::VkDescriptorPool pool,
                                                  vk::VkDescriptorSetLayout layout, vk::VkBuffer buffer,
                                                  uint32_t offset, vk::VkBuffer resBuf);

vk::Move<vk::VkDescriptorSet> createDescriptorSet(vk::VkDescriptorPool pool, vk::VkDescriptorSetLayout layout,
                                                  vk::VkBuffer viewA, uint32_t offsetA, vk::VkBuffer viewB,
                                                  uint32_t offsetB, vk::VkBuffer resBuf, vkt::Context &context);

} // namespace api
} // namespace vkt

#endif // _VKTAPIBUFFERCOMPUTEINSTANCE_HPP
