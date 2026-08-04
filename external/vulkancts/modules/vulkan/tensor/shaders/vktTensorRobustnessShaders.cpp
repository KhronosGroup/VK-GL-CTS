/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 ARM Ltd.
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

std::string genShaderTensorOoBAccess(const size_t rank, const VkFormat tensorFormat, const AccessVariant variant)
{
    const std::string glslType = getTensorFormat(tensorFormat);

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

    shader << "layout(local_size_x = " << shaderTensorOoBAccessWorkgroupSize
           << ", local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(set=0, binding = 0) uniform tensorARM<" << glslType << ", " << rank << "> tens;\n";
    shader << "layout(set=0, binding = 1, std430) buffer _buff { " << glslType << " data[]; };\n";

    shader << "void main()\n{\n";

    // Query tensor dimensions sizes for use during tensor coordinate calculation
    for (size_t i = 0; i < rank; ++i)
    {
        if (i < rank - 1)
        {
            shader << "\tconst uint size_d" << i << " = tensorSizeARM(tens, " << i << ") + 1;\n";
        }
        else
        {
            shader << "\tconst uint size_d" << i << " = tensorSizeARM(tens, " << i << ");\n";
        }
    }

    shader << "\tconst uint line = gl_GlobalInvocationID.x;\n";

    // Check that the shader invocation is not after the last line
    // If it is, end early
    shader << "\tconst uint line_count = 1";
    for (size_t i = 0; i < rank - 1; ++i)
    {
        shader << " * size_d" << i;
    }
    shader << ";\n";
    shader << "\tif (line >= line_count) return;\n";

    // Calculate tensor coordinates based on global invocation ID and tensor shape
    for (size_t i = 0; i < rank - 1; ++i)
    {
        shader << "\tconst uint coord_" << i << " = line / (1";
        for (size_t j = i + 1; j < rank - 1; ++j)
        {
            shader << " * size_d" << j;
        }
        shader << ") % size_d" << i << ";\n";
    }

    shader << "\tconst uint buffer_base_index = gl_GlobalInvocationID.x * 4;\n";

    // Perform a read or write operation using the calculated tensor coordinates
    const auto accessLineIndex = [&shader, &glslType, variant, rank](const int offset, const size_t bufferIndex,
                                                                     const unsigned int oobValue) -> void
    {
        if (variant == AccessVariant::WRITE_TO_BUFFER)
        {
            shader << "\ttensorReadARM(tens, uint[](";
        }
        else
        {
            shader << "\ttensorWriteARM(tens, uint[](";
        }

        for (size_t i = 0; i < rank - 1; ++i)
        {
            shader << "coord_" << i << ", ";
        }

        shader << "size_d" << rank - 1 << " + " << offset << "), data[buffer_base_index + " << bufferIndex << "]";

        // Set explicit OOB value if we're reading. If OOB value should be 0, use the implicit value
        if (oobValue != 0 && variant == AccessVariant::WRITE_TO_BUFFER)
        {
            shader << ", gl_TensorOperandsOutOfBoundsValueARM, " << glslType << "(" << oobValue << ")\n";
        }

        shader << ");\n";
    };

    for (size_t i = 0; i < shaderTensorOoBAccessLineAccesses.size(); ++i)
    {
        const ShaderTensorOoBAccessLineAccess &access = shaderTensorOoBAccessLineAccesses[i];
        accessLineIndex(access.offsetFromEndOfLine, i, access.outOfBoundsValue);
    }

    shader << "}\n";

    return shader.str();
}

} // namespace tensor
} // namespace vkt
