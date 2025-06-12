#ifndef _VKTCOOPERATIVEVECTORUTILS_HPP
#define _VKTCOOPERATIVEVECTORUTILS_HPP
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
 * \brief Cooperative Vector Shader Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

#include "vkDefs.hpp"

namespace vkt
{
namespace cooperative_vector
{

struct ComponentTypeInfo
{
    const char *typeName;
    const char *interpString;
    uint32_t bits;
};

ComponentTypeInfo const getComponentTypeInfo(uint32_t idx);
bool isFloatType(vk::VkComponentTypeKHR t);
bool isSIntType(vk::VkComponentTypeKHR t);
void GetFloatExpManBits(vk::VkComponentTypeKHR dt, uint32_t &expBits, uint32_t &manBits, uint32_t &byteSize);
void setDataFloat(void *base, vk::VkComponentTypeKHR dt, uint32_t i, float value);
float getDataFloat(void *base, vk::VkComponentTypeKHR dt, uint32_t i);
float getDataFloatOffsetIndex(void *base, vk::VkComponentTypeKHR dt, uint32_t offset, uint32_t index);
void setDataFloatOffsetIndex(void *base, vk::VkComponentTypeKHR dt, uint32_t offset, uint32_t index, float value);
void setDataInt(void *base, vk::VkComponentTypeKHR dt, uint32_t i, uint32_t value);
int64_t getDataInt(void *base, vk::VkComponentTypeKHR dt, uint32_t i);
int64_t getDataIntOffsetIndex(void *base, vk::VkComponentTypeKHR dt, uint32_t offset, uint32_t index);
void setDataIntOffsetIndex(void *base, vk::VkComponentTypeKHR dt, uint32_t offset, uint32_t index, uint32_t value);
int64_t truncInt(int64_t x, vk::VkComponentTypeKHR dt);

} // namespace cooperative_vector
} // namespace vkt

#endif // _VKTCOOPERATIVEVECTORUTILS_HPP
