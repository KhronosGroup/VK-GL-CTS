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

#include "vktTensorShaders.hpp"

#include "../vktTensorTestsUtil.hpp"

#include <sstream>
#include <string>

namespace vkt
{
namespace tensor
{

using namespace vk;

std::string genShaderArrayAccess(size_t rank, AccessVariant variant, VkFormat format, const int arraySize)
{
    const std::string glslType = getTensorFormat(format);

    std::ostringstream shader;

    shader << R"(
#version 450
#extension GL_ARM_tensors : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
)";

    shader << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(set=0, binding = 0) uniform tensorARM<" << glslType << ", " << rank << "> tens;\n";
    shader << "layout(set=0, binding = 1, std430) buffer _buff { " << glslType << " data[]; };\n";

    shader << "void main()\n{\n";

    // Query tensor dimensions sizes for use during tensor coordinate calculation
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "\tconst uint size_d" << i << " = tensorSizeARM(tens, " << i << ");\n";
    }

    shader << "\tconst uint offset_x = " << arraySize << " * gl_GlobalInvocationID.x;\n";
    shader << "\tconst uint offset_y = gl_GlobalInvocationID.y;\n";

    // Calculate tensor coordinates based on global invocation ID and tensor shape
    for (size_t i = 0; i < rank - 1; ++i)
    {
        shader << "\tconst uint coord_" << i << " = offset_y / (1";
        for (size_t j = i + 1; j < rank - 1; ++j)
        {
            shader << " * size_d" << j;
        }
        shader << ") % size_d" << i << ";\n";
    }

    shader << "\tconst uint coord_" << rank - 1 << " = offset_x;\n";
    shader << "\tconst uint buffer_index = size_d" << rank - 1 << " * gl_GlobalInvocationID.y + " << arraySize
           << " * gl_GlobalInvocationID.x;\n";

    shader << "\t" << glslType << " tmp[" << arraySize << "];\n";
    switch (variant)
    {
    case ARRAY_READ:
        shader << "\ttensorReadARM(tens, uint[](";
        for (size_t i = 0; i < rank; ++i)
        {
            shader << "coord_" << i << (i == rank - 1 ? "" : ", ");
        }
        shader << "), tmp);\n";
        shader << "\tfor (int i = 0; (i < " << arraySize << ") && (coord_" << rank - 1 << " + i < size_d" << rank - 1
               << "); ++i)\n";
        shader << "\t{\n";
        shader << "\t\tdata[buffer_index + i] = tmp[i];";
        shader << "\t}\n";
        break;
    case ARRAY_WRITE:
        shader << "\tfor (int i = 0; (i < " << arraySize << ") && (coord_" << rank - 1 << " + i < size_d" << rank - 1
               << "); ++i)\n";
        shader << "\t{\n";
        shader << "\t\ttmp[i] = data[buffer_index + i];\n";
        shader << "\t}\n";
        shader << "\ttensorWriteARM(tens, uint[](";
        for (size_t i = 0; i < rank; ++i)
        {
            shader << "coord_" << i << (i == rank - 1 ? "" : ", ");
        }
        shader << "), tmp);\n";
        break;
    default:
        DE_ASSERT(false);
    }
    shader << "}\n";

    return shader.str();
}

} // namespace tensor
} // namespace vkt
