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
    case VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM:
        return "bfloat16_t";
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
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM:
        return "floate5m2_t";
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM:
        return "floate4m3_t";
    default:
        DE_FATAL("Unexpected tensor format");
        return "error";
    }
}

uint32_t getFormatComponents(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R16_UNORM:
        return 1;

    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16G16_UNORM:
        return 2;

    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
        return 4;

    default:
        DE_FATAL("Unexpected image format");
        return 0;
    }
}

bool isFormat64BitInteger(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_UINT:
        return true;
    default:
        return false;
    }

    return false;
}

std::string getImageFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_SINT:
        return "r8i";
    case VK_FORMAT_R16_SINT:
        return "r16i";
    case VK_FORMAT_R32_SINT:
        return "r32i";
    case VK_FORMAT_R64_SINT:
        return "r64i";
    case VK_FORMAT_R8_UINT:
        return "r8ui";
    case VK_FORMAT_R16_UINT:
        return "r16ui";
    case VK_FORMAT_R32_UINT:
        return "r32ui";
    case VK_FORMAT_R64_UINT:
        return "r64ui";
    case VK_FORMAT_R8_SNORM:
        return "r8_snorm";
    case VK_FORMAT_R16_SNORM:
        return "r16_snorm";
    case VK_FORMAT_R8_UNORM:
        return "r8";
    case VK_FORMAT_R16_UNORM:
        return "r16";

    case VK_FORMAT_R8G8_SINT:
        return "rg8i";
    case VK_FORMAT_R16G16_SINT:
        return "rg16i";
    case VK_FORMAT_R32G32_SINT:
        return "rg32i";
    case VK_FORMAT_R8G8_UINT:
        return "rg8ui";
    case VK_FORMAT_R16G16_UINT:
        return "rg16ui";
    case VK_FORMAT_R32G32_UINT:
        return "rg32ui";
    case VK_FORMAT_R8G8_SNORM:
        return "rg8_snorm";
    case VK_FORMAT_R16G16_SNORM:
        return "rg16_snorm";
    case VK_FORMAT_R8G8_UNORM:
        return "rg8";
    case VK_FORMAT_R16G16_UNORM:
        return "rg16";

    case VK_FORMAT_R8G8B8A8_SINT:
        return "rgba8i";
    case VK_FORMAT_R16G16B16A16_SINT:
        return "rgba16i";
    case VK_FORMAT_R32G32B32A32_SINT:
        return "rgba32i";
    case VK_FORMAT_R8G8B8A8_UINT:
        return "rgba8ui";
    case VK_FORMAT_R16G16B16A16_UINT:
        return "rgba16ui";
    case VK_FORMAT_R32G32B32A32_UINT:
        return "rgba32ui";
    case VK_FORMAT_R8G8B8A8_SNORM:
        return "rgba8_snorm";
    case VK_FORMAT_R16G16B16A16_SNORM:
        return "rgba16_snorm";
    case VK_FORMAT_R8G8B8A8_UNORM:
        return "rgba8";
    case VK_FORMAT_R16G16B16A16_UNORM:
        return "rgba16";

    default:
        DE_FATAL("Unexpected image format");
        return "error";
    }
}

std::string getImagePrefix(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
        return "i";

    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
        return "u";

    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
        return "";

    case VK_FORMAT_R64_SINT:
        return "i64";

    case VK_FORMAT_R64_UINT:
        return "u64";

    default:
        DE_FATAL("Unexpected image format");
        return "error";
    }
}

std::string getImageType(VkFormat format, const size_t image_rank)
{
    std::ostringstream ss;
    ss << getImagePrefix(format) << "image" << image_rank << "D";
    return ss.str();
}

VkFormat imageToTensorFormat(VkFormat imageFormat)
{
    switch (imageFormat)
    {
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8_SINT:
        return VK_FORMAT_R8_SINT;

    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
        return VK_FORMAT_R16_SINT;

    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
        return VK_FORMAT_R32_SINT;

    case VK_FORMAT_R64_SINT:
        return VK_FORMAT_R64_SINT;

    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
        return VK_FORMAT_R8_UINT;

    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
        return VK_FORMAT_R16_UINT;

    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
        return VK_FORMAT_R32_UINT;

    case VK_FORMAT_R64_UINT:
        return VK_FORMAT_R64_UINT;

    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8_UINT;

    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
        return VK_FORMAT_R16_SFLOAT;

    default:
        DE_FATAL("Unexpected image format");
        return VK_FORMAT_UNDEFINED;
    }
}

std::string imageCoordinates(std::string prefix, size_t rank)
{
    std::ostringstream src;

    src << "ivec" << rank << "(";
    for (size_t idx = 0; idx < rank; ++idx)
    {
        if (idx != 0)
        {
            src << ", ";
        }
        src << prefix << rank - (idx + 1);
    }
    src << ")";

    return src.str();
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
