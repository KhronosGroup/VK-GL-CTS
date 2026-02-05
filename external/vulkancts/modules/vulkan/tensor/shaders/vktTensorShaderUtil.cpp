/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor Shader Utility Classes
 */
/*--------------------------------------------------------------------*/

#include "deStringUtil.hpp"
#include "../vktTensorTestsUtil.hpp"
#include "vktTensorShaderUtil.hpp"

#include <iostream>

namespace vkt
{
namespace tensor
{

using namespace vk;

std::string getTensorFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R64_SFLOAT:
        return "float64_t";
    case VK_FORMAT_R64_UINT:
        return "uint64_t";
    case VK_FORMAT_R64_SINT:
        return "int64_t";
    case VK_FORMAT_R32_SFLOAT:
        return "float32_t";
    case VK_FORMAT_R32_UINT:
        return "uint32_t";
    case VK_FORMAT_R32_SINT:
        return "int32_t";
    case VK_FORMAT_R16_SFLOAT:
        return "float16_t";
    case VK_FORMAT_R16_UINT:
        return "uint16_t";
    case VK_FORMAT_R16_SINT:
        return "int16_t";
    case VK_FORMAT_R8_UINT:
        return "uint8_t";
    case VK_FORMAT_R8_SINT:
        return "int8_t";
    case VK_FORMAT_R8_BOOL_ARM:
        return "bool";
    default:
        DE_FATAL("Unexpected tensor format");
        return "error";
    }
}

std::string getBooleanOp(BooleanOperator op)
{
    switch (op)
    {
    case BooleanOperator::AND:
        return "&&";
    case BooleanOperator::NOT:
        return "!";
    case BooleanOperator::XOR:
        return "^^";
    case BooleanOperator::OR:
        return "||";
    default:
        DE_FATAL("Unexpected tensor format");
        return "error";
    }
}

} // namespace tensor
} // namespace vkt
