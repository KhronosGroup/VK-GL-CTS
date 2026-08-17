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

#include "deStringUtil.hpp"
#include "../vktTensorTestsUtil.hpp"

#include <sstream>

namespace vkt
{
namespace tensor
{

using namespace vk;

std::string genShaderQueryDimensions(size_t rank, VkFormat tensorFormat)
{
    std::ostringstream src;

    src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
        << "#extension GL_ARM_tensors : require\n"
        << "#extension GL_EXT_shader_explicit_arithmetic_types : require\n"
        << "\n";

    switch (tensorFormat)
    {
    case VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM:
        src << "#extension GL_EXT_bfloat16: require\n";
        src << "#extension GL_ARM_tensors_bfloat16 : require\n";
        break;
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM:
        src << "#extension GL_EXT_float_e5m2 : require\n";
        src << "#extension GL_ARM_tensors_float_e5m2 : require\n";
        break;
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM:
        src << "#extension GL_EXT_float_e4m3 : require\n";
        src << "#extension GL_ARM_tensors_float_e4m3 : require\n";
        break;
    default:;
    }

    src << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        << "layout(set=0, binding=0) uniform tensorARM<" << getTensorFormat(tensorFormat) << ", " << rank << "> tens;\n"
        << "layout(set=0, binding=1, std430) buffer _buff { uint data[]; };\n"
        << "\n"

        << "void main()\n"
        << "{\n";

    for (size_t idx = 0; idx < rank; ++idx)
    {
        src << "\tdata[" << idx << "] = tensorSizeARM(tens, " << idx << ");\n";
    }

    src << "}";

    return src.str();
}

} // namespace tensor
} // namespace vkt
