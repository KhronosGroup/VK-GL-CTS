/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Utility classes
 *//*--------------------------------------------------------------------*/

#include "vktImageProcessingTestsUtil.hpp"

#include "vkTypeUtil.hpp"

#include "deDefs.h"
#include "deDefs.hpp"
#include "deInt32.h"
#include "deMath.h"

#include "tcuRGBA.hpp"
#include "tcuTexture.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"

using namespace vk;
using namespace tcu;

namespace vkt
{
namespace ImageProcessing
{

Vec4 applySubstitution(const TextureFormat format, const Vec4 &value)
{
    const int numComponents = getNumUsedChannels(format.order);

    return (numComponents == 1) ? Vec4(value.x(), 0.0f, 0.0f, 1.0f) :
           (numComponents == 2) ? Vec4(value.x(), value.y(), 0.0f, 1.0f) :
           (numComponents == 3) ? Vec4(value.x(), value.y(), value.z(), 1.0f) :
                                  value;
}

// Convert VkComponentMapping to an array of 4 VkComponentSwizzle elements.
Vector<VkComponentSwizzle, 4> makeComponentSwizzleVec(const VkComponentMapping &mapping)
{
    const Vector<VkComponentSwizzle, 4> result = {{mapping.r, mapping.g, mapping.b, mapping.a}};
    return result;
}

// Apply swizzling to an array of 4 elements.
template <typename T>
Vector<T, 4> applySwizzle(const Vector<T, 4> &orig, const VkComponentMapping &mapping)
{
    const auto swizzles = makeComponentSwizzleVec(mapping);
    Vector<T, 4> result;

    for (int i = 0; i < decltype(swizzles)::SIZE; ++i)
    {
        const auto cs = swizzles[i];
        DE_ASSERT(cs >= VK_COMPONENT_SWIZZLE_IDENTITY && cs <= VK_COMPONENT_SWIZZLE_A);

        if (cs == VK_COMPONENT_SWIZZLE_IDENTITY)
            result[i] = orig[i];
        else if (cs == VK_COMPONENT_SWIZZLE_ZERO)
            result[i] = static_cast<T>(0);
        else if (cs == VK_COMPONENT_SWIZZLE_ONE)
            result[i] = static_cast<T>(1);
        else
            result[i] = orig[cs - VK_COMPONENT_SWIZZLE_R];
    }

    return result;
}

ImageProcessingResult::ImageProcessingResult(const tcu::TextureFormat format, uint32_t width, uint32_t height,
                                             const VkSamplerAddressMode addressMode,
                                             const VkSamplerReductionMode reductionMode)
    : tcu::TextureLevel(format, width, height)
    , m_addressMode(addressMode)
    , m_reductionMode(reductionMode)
{
}

const Vec4 ImageProcessingResult::getBlockMatchingResult(const bool isSSD, const PixelBufferAccess &targetPixels,
                                                         const tcu::UVec2 &targetCoord,
                                                         const PixelBufferAccess &referencePixels,
                                                         const tcu::UVec2 &referenceCoord, const tcu::UVec2 &blockSize,
                                                         const VkComponentMapping &componentMapping)
{
    uint32_t i0 = targetCoord.x();
    uint32_t j0 = targetCoord.y();

    uint32_t k0 = referenceCoord.x();
    uint32_t l0 = referenceCoord.y();

    uint32_t blockWidth  = blockSize.x();
    uint32_t blockHeight = blockSize.y();

    const Vec4 borderColor(0.0f); // VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
    const uint32_t tgtWidth  = targetPixels.getWidth();
    const uint32_t tgtHeight = targetPixels.getHeight();

    Vec4 sum      = Vec4(0.0f);
    Vec4 minValue = Vec4(1.0f);
    Vec4 maxValue = Vec4(0.0f);

    for (uint32_t w = 0; w < blockWidth; w++)
    {
        for (uint32_t h = 0; h < blockHeight; h++)
        {
            uint32_t tgtPixCoordX = i0 + w;
            uint32_t tgtPixCoordY = j0 + h;

            bool useBorderColor = false;
            if (!de::inBounds(tgtPixCoordX, 0u, tgtWidth))
            {
                if (m_addressMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    tgtPixCoordX = de::clamp(tgtPixCoordX, 0u, tgtWidth - 1);
                else // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                    useBorderColor = true;
            }

            if (!de::inBounds(tgtPixCoordY, 0u, tgtHeight))
            {
                if (m_addressMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    tgtPixCoordY = de::clamp(tgtPixCoordY, 0u, tgtHeight - 1);
                else // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
                    useBorderColor = true;
            }

            Vec4 tgtPix     = useBorderColor ? borderColor : targetPixels.getPixel(tgtPixCoordX, tgtPixCoordY);
            Vec4 refPix     = referencePixels.getPixel(k0 + w, l0 + h);
            Vec4 currDiff   = absDiff(refPix, tgtPix);
            Vec4 sqCurrDiff = pow(currDiff, Vec4(2u));

            switch (m_reductionMode)
            {
            case VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE:
                sum += (isSSD ? sqCurrDiff : currDiff);
                break;
            case VK_SAMPLER_REDUCTION_MODE_MIN:
                minValue = min((isSSD ? sqCurrDiff : currDiff), minValue);
                break;
            case VK_SAMPLER_REDUCTION_MODE_MAX:
                maxValue = max((isSSD ? sqCurrDiff : currDiff), maxValue);
                break;
            default:
                DE_ASSERT(false);
            }
        }
    }

    // Select error metric
    Vec4 errorMetric = Vec4(0.0f);

    switch (m_reductionMode)
    {
    case VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE:
        errorMetric = sum;
        break;
    case VK_SAMPLER_REDUCTION_MODE_MIN:
        errorMetric = minValue;
        break;
    case VK_SAMPLER_REDUCTION_MODE_MAX:
        errorMetric = maxValue;
        break;
    default:
        DE_ASSERT(false);
    }

    errorMetric = applySubstitution(targetPixels.getFormat(), errorMetric);

    const Vec4 blockMatchingError = applySwizzle(errorMetric, componentMapping);

    auto pba            = getAccess();
    const Vec4 outColor = (blockMatchingError == Vec4(0.0f)) ?
                              tcu::RGBA::green().toVec() :
                              tcu::RGBA::red().toVec(); // red on mismatch, green on match

    for (int x = 0; x < getWidth(); ++x)
        for (int y = 0; y < getHeight(); ++y)
            pba.setPixel(outColor, x, y);

    return blockMatchingError;
}

DescriptorSetLayoutExtBuilder::DescriptorSetLayoutExtBuilder(void)
{
}

Move<VkDescriptorSetLayout> DescriptorSetLayoutExtBuilder::buildExt(const DeviceInterface &vk, VkDevice device,
                                                                    VkDescriptorSetLayoutCreateFlags extraFlags,
                                                                    VkDescriptorBindingFlags bindingFlag) const
{

    const bool updateAfterBind = ((extraFlags | VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT) ==
                                  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    const std::vector<VkDescriptorBindingFlags> bindingsFlags(m_bindings.size(), bindingFlag);
    const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                           // const void* pNext;
        de::sizeU32(m_bindings),                                           // uint32_t bindingCount;
        de::dataOrNull(bindingsFlags), // const VkDescriptorBindingFlags* pBindingFlags;
    };

    // Create new layout bindings with pImmutableSamplers updated
    std::vector<VkDescriptorSetLayoutBinding> bindings = m_bindings;

    for (size_t samplerInfoNdx = 0; samplerInfoNdx < m_immutableSamplerInfos.size(); samplerInfoNdx++)
    {
        const ImmutableSamplerInfo &samplerInfo = m_immutableSamplerInfos[samplerInfoNdx];
        uint32_t bindingNdx                     = 0;

        while (bindings[bindingNdx].binding != samplerInfo.bindingIndex)
        {
            bindingNdx++;

            if (bindingNdx >= (uint32_t)bindings.size())
                DE_FATAL("Immutable sampler not found");
        }

        bindings[bindingNdx].pImmutableSamplers = &m_immutableSamplers[samplerInfo.samplerBaseIndex];
    }

    const VkDescriptorSetLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        updateAfterBind ? &bindingFlagsInfo : nullptr,
        (VkDescriptorSetLayoutCreateFlags)extraFlags,         // VkDescriptorSetLayoutCreateFlags flags
        de::sizeU32(bindings),                                // uint32_t bindingCount
        (bindings.empty()) ? (nullptr) : (&bindings.front()), // const VkDescriptorSetLayoutBinding* pBinding
    };

    return createDescriptorSetLayout(vk, device, &createInfo);
}

const std::string getImageProcGLSLStr(const ImageProcOp op)
{
    const static std::string ImageProcOpGLSL[IMAGE_PROC_OP_LAST] = {
        "textureWeightedQCOM", "textureBoxFilterQCOM", "textureBlockMatchSADQCOM", "textureBlockMatchSSDQCOM"};
    return ImageProcOpGLSL[op];
}

VkImageViewType mapImageViewType(const ImageType imageType)
{
    switch (imageType)
    {
    case IMAGE_TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case IMAGE_TYPE_1D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case IMAGE_TYPE_2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case IMAGE_TYPE_2D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case IMAGE_TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case IMAGE_TYPE_CUBE:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case IMAGE_TYPE_CUBE_ARRAY:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

    default:
        DE_ASSERT(false);
        return VK_IMAGE_VIEW_TYPE_LAST;
    }
}

VkImageType mapImageType(const ImageType imageType)
{
    switch (imageType)
    {
    case IMAGE_TYPE_1D:
    case IMAGE_TYPE_1D_ARRAY:
    case IMAGE_TYPE_BUFFER:
        return VK_IMAGE_TYPE_1D;

    case IMAGE_TYPE_2D:
    case IMAGE_TYPE_2D_ARRAY:
    case IMAGE_TYPE_CUBE:
    case IMAGE_TYPE_CUBE_ARRAY:
        return VK_IMAGE_TYPE_2D;

    case IMAGE_TYPE_3D:
        return VK_IMAGE_TYPE_3D;

    default:
        DE_ASSERT(false);
        return VK_IMAGE_TYPE_LAST;
    }
}

VkImageCreateInfo makeImageCreateInfo(const ImageType &imageType, const UVec2 imageSize, const VkFormat format,
                                      const VkImageUsageFlags usage, const VkImageCreateFlags flags,
                                      const VkImageTiling tiling)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        flags,                                          // VkImageCreateFlags flags;
        mapImageType(imageType),                        // VkImageType imageType;
        format,                                         // VkFormat format;
        makeExtent3D(imageSize.x(), imageSize.y(), 1u), // VkExtent3D extent;
        1u,                                             // uint32_t mipLevels;
        1u,                                             // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                          // VkSampleCountFlagBits samples;
        tiling,                                         // VkImageTiling tiling;
        usage,                                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                      // VkSharingMode sharingMode;
        0u,                                             // uint32_t queueFamilyIndexCount;
        nullptr,                                        // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                      // VkImageLayout initialLayout;
    };
    return imageParams;
}

// Image view with identity component mapping
Move<VkImageView> makeImageViewUtil(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                    const VkImageViewType imageViewType, const VkFormat format,
                                    const VkImageSubresourceRange subresourceRange)
{
    const VkImageViewCreateInfo imageViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkImageViewCreateFlags flags;
        image,                                    // VkImage image;
        imageViewType,                            // VkImageViewType viewType;
        format,                                   // VkFormat format;
        makeComponentMappingIdentity(),           // VkComponentMapping components;
        subresourceRange,                         // VkImageSubresourceRange subresourceRange;
    };
    return createImageView(vk, vkDevice, &imageViewParams);
}

Move<VkImageView> makeImageViewUtil(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                    const VkImageViewType imageViewType, const VkFormat format,
                                    const VkImageSubresourceRange subresourceRange, const VkComponentMapping components)
{
    const VkImageViewCreateInfo imageViewParams = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        0u,                                       // VkImageViewCreateFlags flags;
        image,                                    // VkImage image;
        imageViewType,                            // VkImageViewType viewType;
        format,                                   // VkFormat format;
        components,                               // VkComponentMapping components;
        subresourceRange,                         // VkImageSubresourceRange subresourceRange;
    };
    return createImageView(vk, vkDevice, &imageViewParams);
}

std::string getFormatPrefix(const TextureFormat &format)
{
    return getTextureChannelClass(format.type) == TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" :
           getTextureChannelClass(format.type) == TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" :
                                                                                         "";
}

std::string getFormatShortString(const vk::VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

std::string getImageTypeName(const ImageType imageType)
{
    switch (imageType)
    {
    case IMAGE_TYPE_1D:
        return "1d";
    case IMAGE_TYPE_1D_ARRAY:
        return "1d_array";
    case IMAGE_TYPE_2D:
        return "2d";
    case IMAGE_TYPE_2D_ARRAY:
        return "2d_array";
    case IMAGE_TYPE_3D:
        return "3d";
    case IMAGE_TYPE_CUBE:
        return "cube";
    case IMAGE_TYPE_CUBE_ARRAY:
        return "cube_array";
    case IMAGE_TYPE_BUFFER:
        return "buffer";

    default:
        DE_ASSERT(false);
        return "";
    }
}

std::vector<VkFormat> getOpSupportedFormats(const ImageProcOp op)
{
    std::vector<VkFormat> supportedFormats;

    switch (op)
    {
    case ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SAD:
    case ImageProcOp::IMAGE_PROC_OP_BLOCK_MATCH_SSD:
    {
        supportedFormats = {
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R8G8_UNORM,
            VK_FORMAT_R8G8B8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_A8B8G8R8_UNORM_PACK32,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32,
            // TODO
            // VK_FORMAT_G8B8G8R8_422_UNORM,
            // VK_FORMAT_B8G8R8G8_422_UNORM
        };
    }
    break;
    default:
        DE_ASSERT(false);
    }

    return supportedFormats;
}

const std::string getStageNames(const VkShaderStageFlags stageMask)
{
    VkShaderStageFlags mask = stageMask;
    std::string names       = "";

    while (mask)
    {
        const VkShaderStageFlagBits stageBit = static_cast<VkShaderStageFlagBits>(stageMask & 0x00000001);
        switch (stageBit)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
        {
            names += "vertex";
            break;
        }
        default:
            DE_ASSERT(false); // Not supported
        }

        mask >>= 1;
        names += (mask ? "_" : "");
    }
    return names;
}

} // namespace ImageProcessing
} // namespace vkt
