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

std::string genShaderTensorDescriptorsArrayAccess(const size_t rank, const VkFormat tensorFormat,
                                                  const size_t descriptorCount)
{
    const std::string glslType = getTensorFormat(tensorFormat);

    std::ostringstream shader;

    shader << R"(
#version 450
#extension GL_ARM_tensors : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_scalar_block_layout : require
    )";

    shader << "layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(set=0, binding=0) uniform tensorARM<" << glslType << ", " << rank << "> tens[" << descriptorCount
           << "];\n";
    shader << "layout(set=0, binding=1, std430) uniform desc_idx_buff { uint desc_arr[" << descriptorCount << "]; };\n";
    shader << "layout(set=0, binding=2, std430) buffer buff { " << glslType << " data[]; };\n";

    shader << "void main()\n{\n";

    shader << "\tconst uint element_idx = gl_GlobalInvocationID.x;\n";
    shader << "\tconst uint tens_idx = gl_GlobalInvocationID.y;\n";
    shader << "\tconst uint desc_idx = desc_arr[tens_idx];\n\n";

    // Query tensor dimensions size for use during tensor coordinate calculation
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "\tconst uint size_d" << i << " = tensorSizeARM(tens[desc_idx], " << i << ");\n";
    }

    // Check that the shader invocation is not after the end of the tensor
    // If it is, end early
    shader << "\tconst uint tens_element_count = ";
    for (size_t i = 0; i < rank; ++i)
    {
        shader << "size_d" << i << (i == rank - 1 ? "" : " * ");
    }
    shader << ";\n\n";
    shader << "\tif (gl_GlobalInvocationID.x >= tens_element_count) return;\n\n";

    // Calculate tensor coordinates based on global invocation ID and tensor shape
    for (size_t j = 0; j < rank; ++j)
    {
        shader << "\tconst uint coord_" << j << " = element_idx / (1";
        for (size_t k = j + 1; k < rank; ++k)
        {
            shader << " * size_d" << k;
        }
        shader << ") % size_d" << j << ";\n";
    }

    // calculate output buffer index from tensor elements
    shader << "\n\tconst uint out_idx = tens_idx * tens_element_count + element_idx;\n";

    shader << "\ttensorReadARM(tens[desc_idx], uint[](";
    for (size_t l = 0; l < rank; ++l)
    {
        shader << "coord_" << l << (l == rank - 1 ? "" : ", ");
    }
    shader << "), data[out_idx]);\n";
    shader << "}\n";
    return shader.str();
}

} // namespace tensor
} // namespace vkt
