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

std::string genShaderTensorAccess(const size_t rank, const VkFormat tensorFormat, const AccessVariant variant)
{
    const std::string glslType = getTensorFormat(tensorFormat);

    const bool isFloat = tensorFormat == VK_FORMAT_R32_SFLOAT || tensorFormat == VK_FORMAT_R16_SFLOAT ||
                         tensorFormat == VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM ||
                         tensorFormat == VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM ||
                         tensorFormat == VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM;

    std::ostringstream shader;

    shader << R"(
#version 450
#extension GL_ARM_tensors : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
)";

    switch (tensorFormat)
    {
    case VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM:
        shader << "#extension GL_EXT_bfloat16: require\n";
        shader << "#extension GL_ARM_tensors_bfloat16 : require\n";
        break;
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM:
        shader << "#extension GL_EXT_float_e5m2 : require\n";
        shader << "#extension GL_ARM_tensors_float_e5m2 : require\n";
        break;
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM:
        shader << "#extension GL_EXT_float_e4m3 : require\n";
        shader << "#extension GL_ARM_tensors_float_e4m3 : require\n";
        break;
    default:;
    }

    shader << "layout(local_size_x = " << shaderTensorAccessWorkgroupSize
           << ", local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(set=0, binding = 0) uniform tensorARM<" << glslType << ", " << rank << "> tens;\n";
    shader << "layout(set=0, binding = 1, std430) buffer _buff { " << glslType << " data[]; };\n";

    shader << "void main()\n{\n";

    // Query tensor dimensions sizes for use during tensor coordinate calculation
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "\tconst uint size_d" << i << " = tensorSizeARM(tens, " << i << ");\n";
    }

    // Check that the shader invocation is not after the end of the tensor
    // If it is, end early
    shader << "\tconst uint tensor_element_count = ";
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "size_d" << i << (i == rank - 1 ? "" : " * ");
    }
    shader << ";\n";
    shader << "\tif (gl_GlobalInvocationID.x >= tensor_element_count) return;\n";

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

    // Perform a read or write operation using the calculated tensor coordinates
    shader << "\tconst uint index = gl_GlobalInvocationID.x;\n";

    if (isFloat)
    {
        shader << "\t" << glslType << " value;\n";
        if (variant == AccessVariant::WRITE_TO_BUFFER)
        {
            shader << "\ttensorReadARM(tens, uint[](";
        }
        else
        {
            shader << "\tfloat floatValue = float(data[index]);\n";
            shader << "\tvalue = " << glslType << "(floatValue);\n";
            shader << "\ttensorWriteARM(tens, uint[](";
        }
        for (size_t i = 0; i < rank; ++i)
        {
            shader << "coord_" << i << (i == rank - 1 ? "" : ", ");
        }
        shader << "), value);\n";
        if (variant == AccessVariant::WRITE_TO_BUFFER)
        {
            shader << "\tdata[index] = " << glslType << "(float(value));\n";
        }
    }
    else
    {
        if (variant == AccessVariant::WRITE_TO_BUFFER)
        {
            shader << "\ttensorReadARM(tens, uint[](";
        }
        else
        {
            shader << "\ttensorWriteARM(tens, uint[](";
        }
        for (size_t i = 0; i < rank; ++i)
        {
            shader << "coord_" << i << (i == rank - 1 ? "" : ", ");
        }
        shader << "), data[index]);\n";
    }

    shader << "}\n";

    return shader.str();
}

} // namespace tensor
} // namespace vkt
