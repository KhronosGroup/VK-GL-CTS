#ifndef _VKTPIPELINEBLENDTESTSCOMMON_HPP
#define _VKTPIPELINEBLENDTESTSCOMMON_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \brief Blending Tests Common Things
 *//*--------------------------------------------------------------------*/

#include "vktPipelineBlendTestsCommon.hpp"
#include "deStringUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTextureUtil.hpp"
#include <string>

namespace vkt
{
namespace pipeline
{
namespace blending_common
{
using namespace vk;

// Formats that are dEQP-compatible, non-integer and uncompressed
const std::vector<VkFormat> &getBlendFormats()
{
    static const std::vector<VkFormat> blendFormats{
        VK_FORMAT_R4G4_UNORM_PACK8,
        VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        VK_FORMAT_R5G6B5_UNORM_PACK16,
        VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_SRGB,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_SNORM,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8_SNORM,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_SNORM,
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_SNORM,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16_SNORM,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SNORM,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
        VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,
        VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
    };
    return blendFormats;
}

const std::vector<VkBlendFactor> &getBlendFactors()
{
    static const std::vector<VkBlendFactor> blendFactors = {
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_SRC_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_DST_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        VK_BLEND_FACTOR_CONSTANT_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
        VK_BLEND_FACTOR_CONSTANT_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
    };
    return blendFactors;
}

const std::vector<VkBlendFactor> &getBlendWithDualSourceFactors()
{
    static const std::vector<VkBlendFactor> blendWithDualSourceFactors = {
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_SRC_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_DST_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        VK_BLEND_FACTOR_CONSTANT_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
        VK_BLEND_FACTOR_CONSTANT_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
        VK_BLEND_FACTOR_SRC1_COLOR,
        VK_BLEND_FACTOR_SRC1_ALPHA,
    };
    return blendWithDualSourceFactors;
}

const std::vector<VkBlendFactor> getDualSourceBlendFactors()
{
    const static std::vector<VkBlendFactor> dualSourceBlendFactors = {
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
        VK_BLEND_FACTOR_SRC1_COLOR,
        VK_BLEND_FACTOR_SRC1_ALPHA,
    };
    return dualSourceBlendFactors;
}

const std::vector<VkBlendOp> &getBlendOps()
{
    static const std::vector<VkBlendOp> blendOps = {
        VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX,
    };
    return blendOps;
}

std::string getFormatCaseName(VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

std::string getBlendStateName(const VkPipelineColorBlendAttachmentState &blendState)
{
    const char *shortBlendFactorNames[] = {
        "z",     // VK_BLEND_ZERO
        "o",     // VK_BLEND_ONE
        "sc",    // VK_BLEND_SRC_COLOR
        "1msc",  // VK_BLEND_ONE_MINUS_SRC_COLOR
        "dc",    // VK_BLEND_DEST_COLOR
        "1mdc",  // VK_BLEND_ONE_MINUS_DEST_COLOR
        "sa",    // VK_BLEND_SRC_ALPHA
        "1msa",  // VK_BLEND_ONE_MINUS_SRC_ALPHA
        "da",    // VK_BLEND_DEST_ALPHA
        "1mda",  // VK_BLEND_ONE_MINUS_DEST_ALPHA
        "cc",    // VK_BLEND_CONSTANT_COLOR
        "1mcc",  // VK_BLEND_ONE_MINUS_CONSTANT_COLOR
        "ca",    // VK_BLEND_CONSTANT_ALPHA
        "1mca",  // VK_BLEND_ONE_MINUS_CONSTANT_ALPHA
        "sas",   // VK_BLEND_SRC_ALPHA_SATURATE
        "1ms1c", // VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
        "1ms1a", // VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA
        "s1c",   // VK_BLEND_FACTOR_SRC1_COLOR
        "s1a"    // VK_BLEND_FACTOR_SRC1_ALPHA
    };

    const char *blendOpNames[] = {
        "add",  // VK_BLEND_OP_ADD
        "sub",  // VK_BLEND_OP_SUBTRACT
        "rsub", // VK_BLEND_OP_REVERSE_SUBTRACT
        "min",  // VK_BLEND_OP_MIN
        "max",  // VK_BLEND_OP_MAX
    };

    std::ostringstream shortName;

    shortName << "color_" << shortBlendFactorNames[blendState.srcColorBlendFactor] << "_"
              << shortBlendFactorNames[blendState.dstColorBlendFactor] << "_" << blendOpNames[blendState.colorBlendOp];
    shortName << "_alpha_" << shortBlendFactorNames[blendState.srcAlphaBlendFactor] << "_"
              << shortBlendFactorNames[blendState.dstAlphaBlendFactor] << "_" << blendOpNames[blendState.alphaBlendOp];

    return shortName.str();
}

bool isSupportedBlendFormat(const InstanceInterface &instanceInterface, VkPhysicalDevice device, VkFormat format)
{
    VkFormatProperties formatProps;

    instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) &&
           (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
}

bool isSupportedTransferFormat(const InstanceInterface &instanceInterface, VkPhysicalDevice device, VkFormat format)
{
    VkFormatProperties formatProps;

    instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) &&
           (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
}

bool isSrc1BlendFactor(vk::VkBlendFactor blendFactor)
{
    switch (blendFactor)
    {
    case vk::VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
    case vk::VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
    case vk::VK_BLEND_FACTOR_SRC1_ALPHA:
    case vk::VK_BLEND_FACTOR_SRC1_COLOR:
        return true;
    default:
        return false;
    }
}

bool isSrc1BlendFactor(const VkPipelineColorBlendAttachmentState &state)
{
    return isSrc1BlendFactor(state.srcColorBlendFactor) || isSrc1BlendFactor(state.dstColorBlendFactor) ||
           isSrc1BlendFactor(state.srcAlphaBlendFactor) || isSrc1BlendFactor(state.dstAlphaBlendFactor);
}

bool isAlphaBlendFactor(vk::VkBlendFactor blendFactor)
{
    switch (blendFactor)
    {
    case VK_BLEND_FACTOR_SRC_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
    case VK_BLEND_FACTOR_DST_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
    case VK_BLEND_FACTOR_CONSTANT_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
    case VK_BLEND_FACTOR_SRC1_ALPHA:
        return true;
    default:
        return false;
    }
}

bool isAlphaBlendFactor(const VkPipelineColorBlendAttachmentState &state)
{
    return isAlphaBlendFactor(state.srcColorBlendFactor) || isAlphaBlendFactor(state.dstColorBlendFactor);
}

float getNormChannelThreshold(const tcu::TextureFormat &format, int numBits, int scale)
{
    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return static_cast<float>(scale) / static_cast<float>((1 << numBits) - 1);
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        return static_cast<float>(scale) / static_cast<float>((1 << (numBits - 1)) - 1);
    default:
        break;
    }

    DE_ASSERT(false);
    return 0.0f;
}

tcu::Vec4 getFormatThreshold(const tcu::TextureFormat &format, int scale)
{
    using tcu::TextureFormat;
    using tcu::Vec4;

    Vec4 threshold(0.01f);

    switch (format.type)
    {
    case TextureFormat::UNORM_BYTE_44:
        threshold =
            Vec4(getNormChannelThreshold(format, 4, scale), getNormChannelThreshold(format, 4, scale), 1.0f, 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_565:
        threshold = Vec4(getNormChannelThreshold(format, 5, scale), getNormChannelThreshold(format, 6, scale),
                         getNormChannelThreshold(format, 5, scale), 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_555:
        threshold = Vec4(getNormChannelThreshold(format, 5, scale), getNormChannelThreshold(format, 5, scale),
                         getNormChannelThreshold(format, 5, scale), 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_4444:
        threshold = Vec4(getNormChannelThreshold(format, 4, scale));
        break;

    case TextureFormat::UNORM_SHORT_5551:
        threshold = Vec4(getNormChannelThreshold(format, 5, scale), getNormChannelThreshold(format, 5, scale),
                         getNormChannelThreshold(format, 5, scale), 0.1f);
        break;

    case TextureFormat::UNORM_SHORT_10:
        threshold = Vec4(getNormChannelThreshold(format, 10, scale));
        break;

    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::SNORM_INT_1010102_REV:
        threshold = Vec4(getNormChannelThreshold(format, 10, scale), getNormChannelThreshold(format, 10, scale),
                         getNormChannelThreshold(format, 10, scale), 0.34f);
        break;

    case TextureFormat::UNORM_INT8:
    case TextureFormat::SNORM_INT8:
        threshold = Vec4(getNormChannelThreshold(format, 8, scale));
        break;

    case TextureFormat::UNORM_INT16:
    case TextureFormat::SNORM_INT16:
        threshold = Vec4(getNormChannelThreshold(format, 16, scale));
        break;

    case TextureFormat::UNORM_INT32:
    case TextureFormat::SNORM_INT32:
        threshold = Vec4(getNormChannelThreshold(format, 32, scale));
        break;

    case TextureFormat::HALF_FLOAT:
        threshold = Vec4(0.005f);
        break;

    case TextureFormat::FLOAT:
        threshold = Vec4(0.00001f);
        break;

    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        threshold = Vec4(0.02f, 0.02f, 0.0625f, 1.0f);
        break;

    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        threshold = Vec4(0.05f, 0.05f, 0.05f, 1.0f);
        break;

    case TextureFormat::UNORM_SHORT_1555:
        threshold = Vec4(0.1f, getNormChannelThreshold(format, 5, scale), getNormChannelThreshold(format, 5, scale),
                         getNormChannelThreshold(format, 5, scale));
        break;

    default:
        DE_ASSERT(false);
    }

    // Return value matching the channel order specified by the format
    if (format.order == tcu::TextureFormat::BGR || format.order == tcu::TextureFormat::BGRA)
        return threshold.swizzle(2, 1, 0, 3);
    else
        return threshold;
}

} // namespace blending_common
} // namespace pipeline
} // namespace vkt

#endif // _VKTPIPELINEBLENDTESTSCOMMON_HPP
