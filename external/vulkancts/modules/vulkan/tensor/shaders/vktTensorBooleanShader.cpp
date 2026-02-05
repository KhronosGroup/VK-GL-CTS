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

#include "vktTensorShaderUtil.hpp"

#include "../vktTensorTestsUtil.hpp"

#include <sstream>
#include <string>

namespace vkt
{
namespace tensor
{

using namespace vk;

std::string genShaderBooleanOp(size_t rank, BooleanOperator op, const bool test_value)
{
    std::ostringstream shader;

    shader << R"(
#version 450
#extension GL_ARM_tensors : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
)";

    shader << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(set=0, binding = 0) uniform tensorARM<bool, " << rank << "> tens;\n";
    shader << "layout(set=0, binding = 1) uniform tensorARM<bool, " << rank << "> tens_out;\n";

    shader << "void main()\n{\n";

    // Query tensor dimensions sizes for use during tensor coordinate calculation
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "\tconst uint size_d" << i << " = tensorSizeARM(tens, " << i << ");\n";
    }

    // Calculate tensor coordinates based on global invocation ID and tensor shape
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "\tconst uint coord_" << i << " = gl_GlobalInvocationID.x / (1";
        for (size_t j = i + 1; j < rank; ++j)
        {
            shader << " * size_d" << j;
        }
        shader << ") % size_d" << i << ";\n";
    }

    shader << "\tbool tens_val;\n";

    // Read boolean value from input tensor into local variable
    shader << "\ttensorReadARM(tens, uint[](";
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "coord_" << i << (i == rank - 1 ? "" : ", ");
    }
    shader << "), tens_val);\n";

    // Perform a logical operation on boolean
    switch (op)
    {
    case BooleanOperator::NOT:
        shader << "\tbool res = !tens_val;\n";
        break;
    case BooleanOperator::AND:
        shader << "\tbool res = tens_val && " << (test_value ? "true" : "false") << ";\n";
        break;
    case BooleanOperator::OR:
        shader << "\tbool res = tens_val || " << (test_value ? "true" : "false") << ";\n";
        break;
    case BooleanOperator::XOR:
        shader << "\tbool res = tens_val ^^ " << (test_value ? "true" : "false") << ";\n";
        break;
    default:
        DE_ASSERT(false);
    }

    // Write the resulting boolean to output tensor
    shader << "\ttensorWriteARM(tens_out, uint[](";
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "coord_" << i << (i == rank - 1 ? "" : ", ");
    }
    shader << "), res);\n";

    shader << "}\n";

    return shader.str();
}

} // namespace tensor
} // namespace vkt
