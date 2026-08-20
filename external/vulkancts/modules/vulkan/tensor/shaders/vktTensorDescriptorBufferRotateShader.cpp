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

std::string genShaderTensorDescriptorBufferRotate(const size_t descriptorCount, const size_t shiftIndex)
{
    std::ostringstream shader;

    shader << R"(
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_scalar_block_layout : require
    )";

    shader << "\nlayout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;\n";
    shader << "layout(buffer_reference, std430, buffer_reference_align = 16) buffer buff { uint8_t loc []; };\n";
    shader << "layout(push_constant, std430) uniform pushConstants { buff data; } descBuff;\n";
    shader << "layout(constant_id=0) const uint tensDescSize = 0u;\n\n";

    // descByte tensor descriptor byte stride to iterate through each tensor descriptor
    // cycleIdx index for tracking number of cycles

    shader << "void main()\n{\n";
    shader << "\tuint descByte = gl_GlobalInvocationID.x;\n";
    shader << "\tuint cycleIdx = gl_GlobalInvocationID.y;\n\n";
    shader << "\tif (descByte >= tensDescSize) return;\n\n";

    // Temporary variables for juggling cycle

    shader << "\tuint currIdx = cycleIdx;\n";
    shader << "\tuint8_t tmp = descBuff.data.loc[currIdx * tensDescSize + descByte];\n\n";

    // Run loop

    shader << "\twhile (true)\n\t{\n";
    shader << "\t\tuint nextIdx = (currIdx + " << shiftIndex << ") % " << descriptorCount << ";\n\n";

    // rotation is complete

    shader << "\t\tif (nextIdx == cycleIdx)\n";
    shader << "\t\t\t\tbreak;\n\n";

    // buffer reference starts at tensor binding offset in descriptor buffer
    // index at current tensor descriptor

    shader << "\t\tdescBuff.data.loc[currIdx * tensDescSize + descByte] = descBuff.data.loc[nextIdx * tensDescSize + "
              "descByte];\n\n";
    shader << "\t\tcurrIdx = nextIdx;\n";
    shader << "\t};\n\n";
    shader << "\tdescBuff.data.loc[currIdx * tensDescSize + descByte] = tmp;\n";
    shader << "}\n";

    return shader.str();
}

} // namespace tensor
} // namespace vkt
