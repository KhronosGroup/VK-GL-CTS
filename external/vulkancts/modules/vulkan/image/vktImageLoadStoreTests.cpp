/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Image load/store Tests
 *//*--------------------------------------------------------------------*/

#include "vktImageLoadStoreTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vktImageLoadStoreUtil.hpp"
#include "vktImageTexture.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "deMath.h"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFloat.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuVectorUtil.hpp"

#include <string>
#include <vector>
#include <map>

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

enum DeviceScopeType
{
    DEVICESCOPE_NONE,
    DEVICESCOPE_STORE,
    DEVICESCOPE_LOAD
};

enum TestConfigurationType
{
    TC_NONE,
    TC_COMP_COMP,
    TC_COMP_DRAW
};

// Check for three-component (non-packed) format, i.e. pixel size is a multiple of 3.
bool formatHasThreeComponents(VkFormat format)
{
    const tcu::TextureFormat texFormat = mapVkFormat(format);
    return (getPixelSize(texFormat) % 3) == 0;
}

VkFormat getSingleComponentFormat(VkFormat format)
{
    tcu::TextureFormat texFormat           = mapVkFormat(format);
    tcu::TextureFormat::ChannelOrder order = isSrgbFormat(format) ? tcu::TextureFormat::sR : tcu::TextureFormat::R;
    texFormat                              = tcu::TextureFormat(order, texFormat.type);
    return mapTextureFormat(texFormat);
}

VkImageAspectFlags getImageAspect(VkFormat format)
{
    return isDepthStencilFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

inline VkBufferImageCopy makeBufferImageCopy(const Texture &texture)
{
    return image::makeBufferImageCopy(makeExtent3D(texture.layerSize()), texture.numLayers());
}

VkBufferImageCopy makeBufferImageCopy(const Texture &texture, const VkImageAspectFlags &aspect)
{
    const VkBufferImageCopy copyParams = {
        0ull,                                                            // VkDeviceSize bufferOffset;
        0u,                                                              // uint32_t bufferRowLength;
        0u,                                                              // uint32_t bufferImageHeight;
        makeImageSubresourceLayers(aspect, 0u, 0u, texture.numLayers()), // VkImageSubresourceLayers imageSubresource;
        makeOffset3D(0, 0, 0),                                           // VkOffset3D imageOffset;
        makeExtent3D(texture.layerSize()),                               // VkExtent3D imageExtent;
    };
    return copyParams;
}

std::string getShaderImageFormatQualifierStr(const VkFormat &format)
{
    VkFormat testFormat;
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
        testFormat = VK_FORMAT_R16_UNORM;
        break;
    case VK_FORMAT_D32_SFLOAT:
        testFormat = VK_FORMAT_R32_SFLOAT;
        break;
    default:
        testFormat = format;
    }
    return getShaderImageFormatQualifier(mapVkFormat(testFormat));
}

tcu::ConstPixelBufferAccess getLayerOrSlice(const Texture &texture, const tcu::ConstPixelBufferAccess access,
                                            const int layer)
{
    switch (texture.type())
    {
    case IMAGE_TYPE_1D:
    case IMAGE_TYPE_2D:
    case IMAGE_TYPE_BUFFER:
        // Not layered
        DE_ASSERT(layer == 0);
        return access;

    case IMAGE_TYPE_1D_ARRAY:
        return tcu::getSubregion(access, 0, layer, access.getWidth(), 1);

    case IMAGE_TYPE_2D_ARRAY:
    case IMAGE_TYPE_CUBE:
    case IMAGE_TYPE_CUBE_ARRAY:
    case IMAGE_TYPE_3D: // 3d texture is treated as if depth was the layers
        return tcu::getSubregion(access, 0, 0, layer, access.getWidth(), access.getHeight(), 1);

    default:
        DE_FATAL("Internal test error");
        return tcu::ConstPixelBufferAccess();
    }
}

//! \return the size in bytes of a given level of a mipmap image, including array layers.
vk::VkDeviceSize getMipmapLevelImageSizeBytes(const Texture &texture, const vk::VkFormat format,
                                              const uint32_t mipmapLevel)
{
    tcu::IVec3 size = texture.size(mipmapLevel);
    return tcu::getPixelSize(vk::mapVkFormat(format)) * size.x() * size.y() * size.z();
}

//! \return the size in bytes of the whole mipmap image, including all mipmap levels and array layers
vk::VkDeviceSize getMipmapImageTotalSizeBytes(const Texture &texture, const vk::VkFormat format)
{
    vk::VkDeviceSize size = 0u;
    int32_t levelCount    = 0u;

    do
    {
        size += getMipmapLevelImageSizeBytes(texture, format, levelCount);
        levelCount++;
    } while (levelCount < texture.numMipmapLevels());
    return size;
}

//! \return true if all layers match in both pixel buffers
bool comparePixelBuffers(tcu::TestLog &log, const Texture &texture, const VkFormat format,
                         const tcu::ConstPixelBufferAccess reference, const tcu::ConstPixelBufferAccess result,
                         const uint32_t mipmapLevel = 0u)
{
    DE_ASSERT(reference.getFormat() == result.getFormat());
    DE_ASSERT(reference.getSize() == result.getSize());

    const bool is3d             = (texture.type() == IMAGE_TYPE_3D);
    const int numLayersOrSlices = (is3d ? texture.size(mipmapLevel).z() : texture.numLayers());
    const int numCubeFaces      = 6;

    int passedLayers = 0;
    for (int layerNdx = 0; layerNdx < numLayersOrSlices; ++layerNdx)
    {
        const std::string comparisonName = "Comparison" + de::toString(layerNdx);
        const std::string comparisonDesc =
            "Image Comparison, " + (isCube(texture) ? "face " + de::toString(layerNdx % numCubeFaces) + ", cube " +
                                                          de::toString(layerNdx / numCubeFaces) :
                                    is3d ? "slice " + de::toString(layerNdx) :
                                           "layer " + de::toString(layerNdx) + " , level " + de::toString(mipmapLevel));

        const tcu::ConstPixelBufferAccess refLayer    = getLayerOrSlice(texture, reference, layerNdx);
        const tcu::ConstPixelBufferAccess resultLayer = getLayerOrSlice(texture, result, layerNdx);

        bool ok = false;

        switch (tcu::getTextureChannelClass(mapVkFormat(format).type))
        {
        case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        {
            ok = tcu::intThresholdCompare(log, comparisonName.c_str(), comparisonDesc.c_str(), refLayer, resultLayer,
                                          tcu::UVec4(0), tcu::COMPARE_LOG_RESULT);
            break;
        }

        case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        {
            // Allow error of minimum representable difference
            tcu::Vec4 threshold(
                1.0f /
                ((tcu::UVec4(1u) << tcu::getTextureFormatMantissaBitDepth(mapVkFormat(format)).cast<uint32_t>()) - 1u)
                    .cast<float>());

            // Add 1 ULP of fp32 imprecision to account for image comparison fp32 math with unorm->float conversions.
            threshold += tcu::Vec4(std::numeric_limits<float>::epsilon());

            ok = tcu::floatThresholdCompare(log, comparisonName.c_str(), comparisonDesc.c_str(), refLayer, resultLayer,
                                            threshold, tcu::COMPARE_LOG_RESULT);
            break;
        }

        case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        {
            const tcu::UVec4 bitDepth =
                tcu::getTextureFormatMantissaBitDepth(mapVkFormat(format)).cast<uint32_t>() - 1u;
            // To avoid bit-shifting with negative value, which is undefined behaviour.
            const tcu::UVec4 fixedBitDepth =
                tcu::select(bitDepth, tcu::UVec4(0u, 0u, 0u, 0u),
                            tcu::greaterThanEqual(bitDepth.cast<int32_t>(), tcu::IVec4(0, 0, 0, 0)));

            // Allow error of minimum representable difference
            const tcu::Vec4 threshold(1.0f / ((tcu::UVec4(1u) << fixedBitDepth) - 1u).cast<float>());

            ok = tcu::floatThresholdCompare(log, comparisonName.c_str(), comparisonDesc.c_str(), refLayer, resultLayer,
                                            threshold, tcu::COMPARE_LOG_RESULT);
            break;
        }

        case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
        {
            // Convert target format ulps to float ulps and allow 1 ulp difference
            const tcu::UVec4 threshold(
                tcu::UVec4(1u) << (tcu::UVec4(23) -
                                   tcu::getTextureFormatMantissaBitDepth(mapVkFormat(format)).cast<uint32_t>()));

            ok = tcu::floatUlpThresholdCompare(log, comparisonName.c_str(), comparisonDesc.c_str(), refLayer,
                                               resultLayer, threshold, tcu::COMPARE_LOG_RESULT);
            break;
        }

        default:
            DE_FATAL("Unknown channel class");
        }

        if (ok)
            ++passedLayers;
    }

    return passedLayers == numLayersOrSlices;
}

//!< Zero out invalid pixels in the image (denormalized, infinite, NaN values)
void replaceBadFloatReinterpretValues(const tcu::PixelBufferAccess access)
{
    DE_ASSERT(tcu::getTextureChannelClass(access.getFormat().type) == tcu::TEXTURECHANNELCLASS_FLOATING_POINT);

    for (int z = 0; z < access.getDepth(); ++z)
        for (int y = 0; y < access.getHeight(); ++y)
            for (int x = 0; x < access.getWidth(); ++x)
            {
                const tcu::Vec4 color(access.getPixel(x, y, z));
                tcu::Vec4 newColor = color;

                for (int i = 0; i < 4; ++i)
                {
                    if (access.getFormat().type == tcu::TextureFormat::HALF_FLOAT)
                    {
                        const tcu::Float16 f(color[i]);
                        if (f.isDenorm() || f.isInf() || f.isNaN())
                            newColor[i] = 0.0f;
                    }
                    else
                    {
                        const tcu::Float32 f(color[i]);
                        if (f.isDenorm() || f.isInf() || f.isNaN())
                            newColor[i] = 0.0f;
                    }
                }

                if (newColor != color)
                    access.setPixel(newColor, x, y, z);
            }
}

//!< replace invalid pixels in the image (-128)
void replaceSnormReinterpretValues(const tcu::PixelBufferAccess access)
{
    DE_ASSERT(tcu::getTextureChannelClass(access.getFormat().type) == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT);

    for (int z = 0; z < access.getDepth(); ++z)
        for (int y = 0; y < access.getHeight(); ++y)
            for (int x = 0; x < access.getWidth(); ++x)
            {
                const tcu::IVec4 color(access.getPixelInt(x, y, z));
                tcu::IVec4 newColor = color;

                for (int i = 0; i < 4; ++i)
                {
                    const int32_t oldColor(color[i]);
                    if (oldColor == -128)
                        newColor[i] = -127;
                }

                if (newColor != color)
                    access.setPixel(newColor, x, y, z);
            }
}

tcu::Vec4 getMiddleValue(VkFormat imageFormat)
{
    tcu::TextureFormat format      = mapVkFormat(imageFormat);
    tcu::TextureFormatInfo fmtInfo = tcu::getTextureFormatInfo(format);
    tcu::Vec4 val                  = (fmtInfo.valueMax - fmtInfo.valueMin) * tcu::Vec4(0.5f);

    if (isIntegerFormat(imageFormat))
        val = floor(val);

    return val;
}

tcu::TextureLevel generateReferenceImage(const tcu::IVec3 &imageSize, const VkFormat imageFormat,
                                         const VkFormat readFormat, bool constantValue = false)
{
    // Generate a reference image data using the storage format

    tcu::TextureLevel reference(mapVkFormat(imageFormat), imageSize.x(), imageSize.y(), imageSize.z());
    const tcu::PixelBufferAccess access = reference.getAccess();

    const float storeColorScale = computeStoreColorScale(imageFormat, imageSize);
    const float storeColorBias  = computeStoreColorBias(imageFormat);

    const bool srgbFormat          = isSrgbFormat(imageFormat);
    const bool intFormat           = isIntegerFormat(imageFormat);
    const bool storeNegativeValues = isSignedFormat(imageFormat) && (storeColorBias == 0);
    const int xMax                 = imageSize.x() - 1;
    const int yMax                 = imageSize.y() - 1;

    for (int z = 0; z < imageSize.z(); ++z)
        for (int y = 0; y < imageSize.y(); ++y)
            for (int x = 0; x < imageSize.x(); ++x)
            {
                if (constantValue)
                {
                    access.setPixel(getMiddleValue(imageFormat), x, y, z);
                }
                else
                {
                    tcu::IVec4 color =
                        tcu::IVec4(x ^ y ^ z, (xMax - x) ^ y ^ z, x ^ (yMax - y) ^ z, (xMax - x) ^ (yMax - y) ^ z);

                    if (storeNegativeValues)
                        color -= tcu::IVec4(deRoundFloatToInt32((float)de::max(xMax, yMax) / 2.0f));

                    if (intFormat)
                        access.setPixel(color, x, y, z);
                    else
                    {
                        if (srgbFormat)
                            access.setPixel(tcu::linearToSRGB(color.asFloat() * storeColorScale + storeColorBias), x, y,
                                            z);
                        else
                            access.setPixel(color.asFloat() * storeColorScale + storeColorBias, x, y, z);
                    }
                }
            }

    // If the image is to be accessed as a float texture, get rid of invalid values

    if (isFloatFormat(readFormat) && imageFormat != readFormat)
        replaceBadFloatReinterpretValues(
            tcu::PixelBufferAccess(mapVkFormat(readFormat), imageSize, access.getDataPtr()));
    if (isSnormFormat(readFormat) && imageFormat != readFormat)
        replaceSnormReinterpretValues(tcu::PixelBufferAccess(mapVkFormat(readFormat), imageSize, access.getDataPtr()));

    return reference;
}

inline tcu::TextureLevel generateReferenceImage(const tcu::IVec3 &imageSize, const VkFormat imageFormat,
                                                bool constantValue = false)
{
    return generateReferenceImage(imageSize, imageFormat, imageFormat, constantValue);
}

void flipHorizontally(const tcu::PixelBufferAccess access)
{
    const int xMax      = access.getWidth() - 1;
    const int halfWidth = access.getWidth() / 2;

    if (isIntegerFormat(mapTextureFormat(access.getFormat())))
        for (int z = 0; z < access.getDepth(); z++)
            for (int y = 0; y < access.getHeight(); y++)
                for (int x = 0; x < halfWidth; x++)
                {
                    const tcu::UVec4 temp = access.getPixelUint(xMax - x, y, z);
                    access.setPixel(access.getPixelUint(x, y, z), xMax - x, y, z);
                    access.setPixel(temp, x, y, z);
                }
    else
        for (int z = 0; z < access.getDepth(); z++)
            for (int y = 0; y < access.getHeight(); y++)
                for (int x = 0; x < halfWidth; x++)
                {
                    const tcu::Vec4 temp = access.getPixel(xMax - x, y, z);
                    access.setPixel(access.getPixel(x, y, z), xMax - x, y, z);
                    access.setPixel(temp, x, y, z);
                }
}

inline bool formatsAreCompatible(const VkFormat format0, const VkFormat format1)
{
    const bool isAlphaOnly = (isAlphaOnlyFormat(format0) || isAlphaOnlyFormat(format1));
    return format0 == format1 ||
           (mapVkFormat(format0).getPixelSize() == mapVkFormat(format1).getPixelSize() && !isAlphaOnly);
}

void commandImageWriteBarrierBetweenShaderInvocations(Context &context, const VkCommandBuffer cmdBuffer,
                                                      const VkImage image, const Texture &texture,
                                                      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    const VkImageSubresourceRange fullImageSubresourceRange =
        makeImageSubresourceRange(aspectMask, 0u, texture.numMipmapLevels(), 0u, texture.numLayers());
    const VkImageMemoryBarrier shaderWriteBarrier =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, 0u, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, image,
                               fullImageSubresourceRange);

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &shaderWriteBarrier);
}

void commandBufferWriteBarrierBeforeHostRead(Context &context, const VkCommandBuffer cmdBuffer, const VkBuffer buffer,
                                             const VkDeviceSize bufferSizeBytes)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    const VkBufferMemoryBarrier shaderWriteBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, buffer, 0ull, bufferSizeBytes);

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &shaderWriteBarrier, 0, nullptr);
}

//! Copy all layers of an image to a buffer.
void commandCopyImageToBuffer(Context &context, const VkCommandBuffer cmdBuffer, const VkImage image,
                              const VkBuffer buffer, const VkDeviceSize bufferSizeBytes, const Texture &texture,
                              VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    const VkImageSubresourceRange fullImageSubresourceRange =
        makeImageSubresourceRange(aspectMask, 0u, 1u, 0u, texture.numLayers());
    const VkImageMemoryBarrier prepareForTransferBarrier =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, fullImageSubresourceRange);

    const VkBufferImageCopy copyRegion = makeBufferImageCopy(texture, aspectMask);

    const VkBufferMemoryBarrier copyBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, buffer, 0ull, bufferSizeBytes);

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &prepareForTransferBarrier);
    vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1u, &copyRegion);
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1, &copyBarrier, 0, nullptr);
}

//! Copy all layers of a mipmap image to a buffer.
void commandCopyMipmapImageToBuffer(Context &context, const VkCommandBuffer cmdBuffer, const VkImage image,
                                    const VkFormat imageFormat, const VkBuffer buffer,
                                    const VkDeviceSize bufferSizeBytes, const Texture &texture)
{
    const DeviceInterface &vk = context.getDeviceInterface();

    const VkImageSubresourceRange fullImageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, texture.numMipmapLevels(), 0u, texture.numLayers());
    const VkImageMemoryBarrier prepareForTransferBarrier =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, fullImageSubresourceRange);

    std::vector<VkBufferImageCopy> copyRegions;
    VkDeviceSize bufferOffset = 0u;
    for (int32_t levelNdx = 0; levelNdx < texture.numMipmapLevels(); levelNdx++)
    {
        const VkBufferImageCopy copyParams = {
            bufferOffset, // VkDeviceSize bufferOffset;
            0u,           // uint32_t bufferRowLength;
            0u,           // uint32_t bufferImageHeight;
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, levelNdx, 0u,
                                       texture.numLayers()), // VkImageSubresourceLayers imageSubresource;
            makeOffset3D(0, 0, 0),                           // VkOffset3D imageOffset;
            makeExtent3D(texture.layerSize(levelNdx)),       // VkExtent3D imageExtent;
        };
        copyRegions.push_back(copyParams);
        bufferOffset += getMipmapLevelImageSizeBytes(texture, imageFormat, levelNdx);
    }

    const VkBufferMemoryBarrier copyBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, buffer, 0ull, bufferSizeBytes);

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &prepareForTransferBarrier);
    vk.cmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer,
                            (uint32_t)copyRegions.size(), copyRegions.data());
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1, &copyBarrier, 0, nullptr);
}

class StoreTest : public TestCase
{
public:
    enum TestFlags
    {
        FLAG_SINGLE_LAYER_BIND = 0x1, //!< Run the shader multiple times, each time binding a different layer.
        FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER = 0x2, //!< Declare the format of the images in the shader code
        FLAG_MINALIGN             = 0x4, //!< Use bufferview offset that matches the advertised minimum alignment
        FLAG_STORE_CONSTANT_VALUE = 0x8, //!< Store constant value
    };

    StoreTest(tcu::TestContext &testCtx, const std::string &name, const Texture &texture, const VkFormat format,
              const VkImageTiling tiling, const uint32_t flags = FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

private:
    const Texture m_texture;
    const VkFormat m_format;
    const VkImageTiling m_tiling;
    const bool m_declareImageFormatInShader;
    const bool m_singleLayerBind;
    const bool m_minalign;
    const bool m_storeConstantValue;
};

StoreTest::StoreTest(tcu::TestContext &testCtx, const std::string &name, const Texture &texture, const VkFormat format,
                     const VkImageTiling tiling, const uint32_t flags)
    : TestCase(testCtx, name)
    , m_texture(texture)
    , m_format(format)
    , m_tiling(tiling)
    , m_declareImageFormatInShader((flags & FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER) != 0)
    , m_singleLayerBind((flags & FLAG_SINGLE_LAYER_BIND) != 0)
    , m_minalign((flags & FLAG_MINALIGN) != 0)
    , m_storeConstantValue((flags & FLAG_STORE_CONSTANT_VALUE) != 0)
{
    if (m_singleLayerBind)
        DE_ASSERT(m_texture.numLayers() > 1);
}

void StoreTest::checkSupport(Context &context) const
{
#ifndef CTS_USES_VULKANSC
    if (m_format == VK_FORMAT_A8_UNORM_KHR || m_format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");

    const VkFormatProperties3 formatProperties(context.getFormatProperties(m_format));

    const auto &tilingFeatures = (m_tiling == vk::VK_IMAGE_TILING_OPTIMAL) ? formatProperties.optimalTilingFeatures :
                                                                             formatProperties.linearTilingFeatures;

    if ((m_texture.type() == IMAGE_TYPE_BUFFER) && !m_declareImageFormatInShader &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR))
        TCU_THROW(NotSupportedError, "Format not supported for unformatted stores via storage buffer");

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !m_declareImageFormatInShader &&
        !(tilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR))
        TCU_THROW(NotSupportedError, "Format not supported for unformatted stores via storage images");

    if (m_texture.type() == IMAGE_TYPE_CUBE_ARRAY)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(tilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");

    if (m_texture.type() == IMAGE_TYPE_BUFFER &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");
#else
    const VkFormatProperties formatProperties(
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), m_format));
    const auto tilingFeatures = (m_tiling == vk::VK_IMAGE_TILING_OPTIMAL) ? formatProperties.optimalTilingFeatures :
                                                                            formatProperties.linearTilingFeatures;

    if (!m_declareImageFormatInShader)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT);

    if (m_texture.type() == IMAGE_TYPE_CUBE_ARRAY)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(tilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");

    if (m_texture.type() == IMAGE_TYPE_BUFFER &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");
#endif // CTS_USES_VULKANSC

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkImageFormatProperties imageFormatProperties;
    const auto result = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, m_format, mapImageType(m_texture.type()), m_tiling,
        (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), 0, &imageFormatProperties);

    if (result != VK_SUCCESS)
    {
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format unsupported for tiling");
        else
            TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
    }

    if (imageFormatProperties.maxArrayLayers < (uint32_t)m_texture.numLayers())
    {
        TCU_THROW(NotSupportedError, "This format and tiling combination does not support this number of aray layers");
    }

    if (imageFormatProperties.maxMipLevels < (uint32_t)m_texture.numMipmapLevels())
    {
        TCU_THROW(NotSupportedError, "This format and tiling combination does not support this number of miplevels");
    }
}

void StoreTest::initPrograms(SourceCollections &programCollection) const
{
    const float storeColorScale = computeStoreColorScale(m_format, m_texture.size());
    const float storeColorBias  = computeStoreColorBias(m_format);
    DE_ASSERT(colorScaleAndBiasAreValid(m_format, storeColorScale, storeColorBias));

    const uint32_t xMax                = m_texture.size().x() - 1;
    const uint32_t yMax                = m_texture.size().y() - 1;
    const std::string signednessPrefix = isUintFormat(m_format) ? "u" : isIntFormat(m_format) ? "i" : "";
    const bool storeNegativeValues     = isSignedFormat(m_format) && (storeColorBias == 0);
    bool useClamp                      = false;
    const std::string colorType        = signednessPrefix + "vec4";
    std::string colorBaseExpr          = colorType + "(";

    std::string colorExpr;

    if (m_storeConstantValue)
    {
        tcu::Vec4 val = getMiddleValue(m_format);

        if (isIntegerFormat(m_format))
        {
            colorExpr = colorBaseExpr + de::toString(static_cast<int64_t>(val.x())) + ", " +
                        de::toString(static_cast<int64_t>(val.y())) + ", " +
                        de::toString(static_cast<int64_t>(val.z())) + ", " +
                        de::toString(static_cast<int64_t>(val.w())) + ")";
        }
        else
        {
            colorExpr = colorBaseExpr + de::toString(val.x()) + ", " + de::toString(val.y()) + ", " +
                        de::toString(val.z()) + ", " + de::toString(val.w()) + ")";
        }
    }
    else
    {
        colorBaseExpr = colorBaseExpr + "gx^gy^gz, " + "(" + de::toString(xMax) + "-gx)^gy^gz, " + "gx^(" +
                        de::toString(yMax) + "-gy)^gz, " + "(" + de::toString(xMax) + "-gx)^(" + de::toString(yMax) +
                        "-gy)^gz)";

        // Large integer values may not be represented with formats with low bit depths
        if (isIntegerFormat(m_format))
        {
            const int64_t minStoreValue =
                storeNegativeValues ? 0 - deRoundFloatToInt64((float)de::max(xMax, yMax) / 2.0f) : 0;
            const int64_t maxStoreValue =
                storeNegativeValues ? deRoundFloatToInt64((float)de::max(xMax, yMax) / 2.0f) : de::max(xMax, yMax);

            useClamp = !isRepresentableIntegerValue(tcu::Vector<int64_t, 4>(minStoreValue), mapVkFormat(m_format)) ||
                       !isRepresentableIntegerValue(tcu::Vector<int64_t, 4>(maxStoreValue), mapVkFormat(m_format));
        }

        // Clamp if integer value cannot be represented with the current format
        if (useClamp)
        {
            const tcu::IVec4 bitDepths = tcu::getTextureFormatBitDepth(mapVkFormat(m_format));
            tcu::IVec4 minRepresentableValue;
            tcu::IVec4 maxRepresentableValue;

            switch (tcu::getTextureChannelClass(mapVkFormat(m_format).type))
            {
            case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
            {
                minRepresentableValue = tcu::IVec4(0);
                maxRepresentableValue = (tcu::IVec4(1) << bitDepths) - tcu::IVec4(1);
                break;
            }

            case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
            {
                minRepresentableValue = -(tcu::IVec4(1) << bitDepths - tcu::IVec4(1));
                maxRepresentableValue = (tcu::IVec4(1) << (bitDepths - tcu::IVec4(1))) - tcu::IVec4(1);
                break;
            }

            default:
                DE_ASSERT(isIntegerFormat(m_format));
            }

            colorBaseExpr = "clamp(" + colorBaseExpr + ", " + signednessPrefix + "vec4" +
                            de::toString(minRepresentableValue) + ", " + signednessPrefix + "vec4" +
                            de::toString(maxRepresentableValue) + ")";
        }

        colorExpr = colorBaseExpr + (storeColorScale == 1.0f ? "" : "*" + de::toString(storeColorScale)) +
                    (storeColorBias == 0.0f ? "" : " + float(" + de::toString(storeColorBias) + ")");

        if (storeNegativeValues)
            colorExpr += "-" + de::toString(deRoundFloatToInt32((float)deMax32(xMax, yMax) / 2.0f));
    }

    const int dimension             = (m_singleLayerBind ? m_texture.layerDimension() : m_texture.dimension());
    const std::string texelCoordStr = (dimension == 1 ? "gx" :
                                       dimension == 2 ? "ivec2(gx, gy)" :
                                       dimension == 3 ? "ivec3(gx, gy, gz)" :
                                                        "");

    const ImageType usedImageType =
        (m_singleLayerBind ? getImageTypeForSingleLayer(m_texture.type()) : m_texture.type());
    const std::string imageTypeStr = getShaderImageType(mapVkFormat(m_format), usedImageType);

    std::string maybeFmtQualStr = m_declareImageFormatInShader ? ", " + getShaderImageFormatQualifierStr(m_format) : "";

    std::ostringstream src;
    src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
        << "\n"
        << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        << "layout (binding = 0" << maybeFmtQualStr << ") writeonly uniform " << imageTypeStr << " u_image;\n";

    if (m_singleLayerBind)
        src << "layout (binding = 1) readonly uniform Constants {\n"
            << "    int u_layerNdx;\n"
            << "};\n";

    src << "\n"
        << "void main (void)\n"
        << "{\n"
        << "    int gx = int(gl_GlobalInvocationID.x);\n"
        << "    int gy = int(gl_GlobalInvocationID.y);\n"
        << "    int gz = " << (m_singleLayerBind ? "u_layerNdx" : "int(gl_GlobalInvocationID.z)") << ";\n"
        << "    " << colorType << " storedColor = " << colorExpr << ";\n"
        << "    imageStore(u_image, " << texelCoordStr << ", storedColor);\n"
        << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
}

//! Generic test iteration algorithm for image tests
class BaseTestInstance : public TestInstance
{
public:
    BaseTestInstance(Context &context, const Texture &texture, const VkFormat format,
                     const bool declareImageFormatInShader, const bool singleLayerBind, const bool minalign,
                     const bool bufferLoadUniform);

    virtual tcu::TestStatus iterate(void);

    virtual ~BaseTestInstance(void)
    {
    }

protected:
    virtual VkDescriptorSetLayout prepareDescriptors(void) = 0;
    virtual tcu::TestStatus verifyResult(void)             = 0;

    virtual void commandBeforeCompute(const VkCommandBuffer cmdBuffer)            = 0;
    virtual void commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer) = 0;
    virtual void commandAfterCompute(const VkCommandBuffer cmdBuffer)             = 0;

    virtual void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                                const int layerNdx) = 0;
    virtual uint32_t getViewOffset(Context &context, const VkFormat format, bool uniform);

    const Texture m_texture;
    const VkFormat m_format;
    const bool m_declareImageFormatInShader;
    const bool m_singleLayerBind;
    const bool m_minalign;
    const bool m_bufferLoadUniform;
    const uint32_t m_srcViewOffset;
    const uint32_t m_dstViewOffset;
};

BaseTestInstance::BaseTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                   const bool declareImageFormatInShader, const bool singleLayerBind,
                                   const bool minalign, const bool bufferLoadUniform)
    : TestInstance(context)
    , m_texture(texture)
    , m_format(format)
    , m_declareImageFormatInShader(declareImageFormatInShader)
    , m_singleLayerBind(singleLayerBind)
    , m_minalign(minalign)
    , m_bufferLoadUniform(bufferLoadUniform)
    , m_srcViewOffset(getViewOffset(context, format, m_bufferLoadUniform))
    , m_dstViewOffset(getViewOffset(
          context, formatHasThreeComponents(format) && m_bufferLoadUniform ? getSingleComponentFormat(format) : format,
          false))
{
}

tcu::TestStatus BaseTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));

    const VkDescriptorSetLayout descriptorSetLayout = prepareDescriptors();
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, descriptorSetLayout));
    const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    commandBeforeCompute(*cmdBuffer);

    const tcu::IVec3 workSize = (m_singleLayerBind ? m_texture.layerSize() : m_texture.size());
    const int loopNumLayers   = (m_singleLayerBind ? m_texture.numLayers() : 1);
    for (int layerNdx = 0; layerNdx < loopNumLayers; ++layerNdx)
    {
        commandBindDescriptorsForLayer(*cmdBuffer, *pipelineLayout, layerNdx);

        if (layerNdx > 0)
            commandBetweenShaderInvocations(*cmdBuffer);

        vk.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), workSize.z());
    }

    commandAfterCompute(*cmdBuffer);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    return verifyResult();
}

//! Base store test implementation
class StoreTestInstance : public BaseTestInstance
{
public:
    StoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                      const bool declareImageFormatInShader, const bool singleLayerBind, const bool minalign,
                      const bool storeConstantValue);

protected:
    virtual tcu::TestStatus verifyResult(void);

    // Add empty implementations for functions that might be not needed
    void commandBeforeCompute(const VkCommandBuffer)
    {
    }
    void commandBetweenShaderInvocations(const VkCommandBuffer)
    {
    }
    void commandAfterCompute(const VkCommandBuffer)
    {
    }

    de::MovePtr<BufferWithMemory> m_imageBuffer;
    const VkDeviceSize m_imageSizeBytes;
    bool m_storeConstantValue;
};

uint32_t BaseTestInstance::getViewOffset(Context &context, const VkFormat format, bool uniform)
{
    if (m_minalign)
    {
        if (!context.getTexelBufferAlignmentFeaturesEXT().texelBufferAlignment)
            return (uint32_t)context.getDeviceProperties().limits.minTexelBufferOffsetAlignment;

        VkPhysicalDeviceTexelBufferAlignmentPropertiesEXT alignmentProperties;
        deMemset(&alignmentProperties, 0, sizeof(alignmentProperties));
        alignmentProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT;

        VkPhysicalDeviceProperties2 properties2;
        deMemset(&properties2, 0, sizeof(properties2));
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &alignmentProperties;

        context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

        VkBool32 singleTexelAlignment = uniform ? alignmentProperties.uniformTexelBufferOffsetSingleTexelAlignment :
                                                  alignmentProperties.storageTexelBufferOffsetSingleTexelAlignment;
        VkDeviceSize align            = uniform ? alignmentProperties.uniformTexelBufferOffsetAlignmentBytes :
                                                  alignmentProperties.storageTexelBufferOffsetAlignmentBytes;

        VkDeviceSize texelSize = formatHasThreeComponents(format) && uniform ?
                                     tcu::getChannelSize(vk::mapVkFormat(format).type) :
                                     tcu::getPixelSize(vk::mapVkFormat(format));

        if (singleTexelAlignment)
            align = de::min(align, texelSize);

        return (uint32_t)align;
    }

    return 0;
}

StoreTestInstance::StoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                     const bool declareImageFormatInShader, const bool singleLayerBind,
                                     const bool minalign, const bool storeConstantValue)
    : BaseTestInstance(context, texture, format, declareImageFormatInShader, singleLayerBind, minalign, false)
    , m_imageSizeBytes(getImageSizeBytes(texture.size(), format))
    , m_storeConstantValue(storeConstantValue)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // A helper buffer with enough space to hold the whole image. Usage flags accommodate all derived test instances.

    m_imageBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(m_imageSizeBytes + m_dstViewOffset,
                             VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));
}

tcu::TestStatus StoreTestInstance::verifyResult(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const tcu::IVec3 imageSize        = m_texture.size();
    const tcu::TextureLevel reference = generateReferenceImage(imageSize, m_format, m_storeConstantValue);

    const Allocation &alloc = m_imageBuffer->getAllocation();
    invalidateAlloc(vk, device, alloc);
    const tcu::ConstPixelBufferAccess result(mapVkFormat(m_format), imageSize,
                                             (const char *)alloc.getHostPtr() + m_dstViewOffset);

    if (comparePixelBuffers(m_context.getTestContext().getLog(), m_texture, m_format, reference.getAccess(), result))
        return tcu::TestStatus::pass("Passed");
    else
        return tcu::TestStatus::fail("Image comparison failed");
}

//! Store test for images
class ImageStoreTestInstance : public StoreTestInstance
{
public:
    ImageStoreTestInstance(Context &context, const Texture &texture, const VkFormat format, const VkImageTiling tiling,
                           const bool declareImageFormatInShader, const bool singleLayerBind, const bool minalign,
                           const bool storeConstantValue);

protected:
    VkDescriptorSetLayout prepareDescriptors(void);
    void commandBeforeCompute(const VkCommandBuffer cmdBuffer);
    void commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer);
    void commandAfterCompute(const VkCommandBuffer cmdBuffer);

    void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const int layerNdx);

    de::MovePtr<Image> m_image;
    de::MovePtr<BufferWithMemory> m_constantsBuffer;
    const VkDeviceSize m_constantsBufferChunkSizeBytes;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    std::vector<SharedVkDescriptorSet> m_allDescriptorSets;
    std::vector<SharedVkImageView> m_allImageViews;
};

ImageStoreTestInstance::ImageStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                               const VkImageTiling tiling, const bool declareImageFormatInShader,
                                               const bool singleLayerBind, const bool minalign,
                                               const bool storeConstantValue)
    : StoreTestInstance(context, texture, format, declareImageFormatInShader, singleLayerBind, minalign,
                        storeConstantValue)
    , m_constantsBufferChunkSizeBytes(getOptimalUniformBufferChunkSize(context.getInstanceInterface(),
                                                                       context.getPhysicalDevice(), sizeof(uint32_t)))
    , m_allDescriptorSets(texture.numLayers())
    , m_allImageViews(texture.numLayers())
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    m_image = de::MovePtr<Image>(
        new Image(vk, device, allocator,
                  makeImageCreateInfo(m_texture, m_format,
                                      (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), 0u, tiling),
                  MemoryRequirement::Any));

    // This buffer will be used to pass constants to the shader

    const int numLayers                         = m_texture.numLayers();
    const VkDeviceSize constantsBufferSizeBytes = numLayers * m_constantsBufferChunkSizeBytes;
    m_constantsBuffer                           = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator, makeBufferCreateInfo(constantsBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    {
        const Allocation &alloc = m_constantsBuffer->getAllocation();
        uint8_t *const basePtr  = static_cast<uint8_t *>(alloc.getHostPtr());

        deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(constantsBufferSizeBytes));

        for (int layerNdx = 0; layerNdx < numLayers; ++layerNdx)
        {
            uint32_t *valuePtr = reinterpret_cast<uint32_t *>(basePtr + layerNdx * m_constantsBufferChunkSizeBytes);
            *valuePtr          = static_cast<uint32_t>(layerNdx);
        }

        flushAlloc(vk, device, alloc);
    }
}

VkDescriptorSetLayout ImageStoreTestInstance::prepareDescriptors(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const int numLayers   = m_texture.numLayers();
    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .build(vk, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numLayers)
                           .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numLayers)
                           .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numLayers);

    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);

    if (m_singleLayerBind)
    {
        for (int layerNdx = 0; layerNdx < numLayers; ++layerNdx)
        {
            m_allDescriptorSets[layerNdx] =
                makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
            m_allImageViews[layerNdx] = makeVkSharedPtr(makeImageView(
                vk, device, m_image->get(), mapImageViewType(getImageTypeForSingleLayer(m_texture.type())), m_format,
                makeImageSubresourceRange(aspectMask, 0u, 1u, layerNdx, 1u)));
        }
    }
    else // bind all layers at once
    {
        m_allDescriptorSets[0] =
            makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
        m_allImageViews[0] =
            makeVkSharedPtr(makeImageView(vk, device, m_image->get(), mapImageViewType(m_texture.type()), m_format,
                                          makeImageSubresourceRange(aspectMask, 0u, 1u, 0u, numLayers)));
    }

    return *m_descriptorSetLayout; // not passing the ownership
}

void ImageStoreTestInstance::commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer,
                                                            const VkPipelineLayout pipelineLayout, const int layerNdx)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const VkDescriptorSet descriptorSet = **m_allDescriptorSets[layerNdx];
    const VkImageView imageView         = **m_allImageViews[layerNdx];

    const VkDescriptorImageInfo descriptorImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, imageView, VK_IMAGE_LAYOUT_GENERAL);

    // Set the next chunk of the constants buffer. Each chunk begins with layer index that we've set before.
    const VkDescriptorBufferInfo descriptorConstantsBufferInfo = makeDescriptorBufferInfo(
        m_constantsBuffer->get(), layerNdx * m_constantsBufferChunkSizeBytes, m_constantsBufferChunkSizeBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &descriptorImageInfo)
        .writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorConstantsBufferInfo)
        .update(vk, device);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                             nullptr);
}

void ImageStoreTestInstance::commandBeforeCompute(const VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk            = m_context.getDeviceInterface();
    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);

    const VkImageSubresourceRange fullImageSubresourceRange =
        makeImageSubresourceRange(aspectMask, 0u, 1u, 0u, m_texture.numLayers());
    const VkImageMemoryBarrier setImageLayoutBarrier =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               m_image->get(), fullImageSubresourceRange);

    const VkDeviceSize constantsBufferSize            = m_texture.numLayers() * m_constantsBufferChunkSizeBytes;
    const VkBufferMemoryBarrier writeConstantsBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, m_constantsBuffer->get(), 0ull, constantsBufferSize);

    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &writeConstantsBarrier, 1, &setImageLayoutBarrier);
}

void ImageStoreTestInstance::commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer)
{
    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);
    commandImageWriteBarrierBetweenShaderInvocations(m_context, cmdBuffer, m_image->get(), m_texture, aspectMask);
}

void ImageStoreTestInstance::commandAfterCompute(const VkCommandBuffer cmdBuffer)
{
    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);
    commandCopyImageToBuffer(m_context, cmdBuffer, m_image->get(), m_imageBuffer->get(), m_imageSizeBytes, m_texture,
                             aspectMask);
}

//! Store test for buffers
class BufferStoreTestInstance : public StoreTestInstance
{
public:
    BufferStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                            const bool declareImageFormatInShader, const bool minalign, const bool storeConstantValue);

protected:
    VkDescriptorSetLayout prepareDescriptors(void);
    void commandAfterCompute(const VkCommandBuffer cmdBuffer);

    void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const int layerNdx);

    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    Move<VkBufferView> m_bufferView;
};

BufferStoreTestInstance::BufferStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                                 const bool declareImageFormatInShader, const bool minalign,
                                                 const bool storeConstantValue)
    : StoreTestInstance(context, texture, format, declareImageFormatInShader, false, minalign, storeConstantValue)
{
}

VkDescriptorSetLayout BufferStoreTestInstance::prepareDescriptors(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .build(vk, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
                           .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);
    m_bufferView    = makeBufferView(vk, device, m_imageBuffer->get(), m_format, m_dstViewOffset, m_imageSizeBytes);

    return *m_descriptorSetLayout; // not passing the ownership
}

void BufferStoreTestInstance::commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer,
                                                             const VkPipelineLayout pipelineLayout, const int layerNdx)
{
    DE_ASSERT(layerNdx == 0);
    DE_UNREF(layerNdx);

    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();

    DescriptorSetUpdateBuilder()
        .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, &m_bufferView.get())
        .update(vk, device);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &m_descriptorSet.get(),
                             0u, nullptr);
}

void BufferStoreTestInstance::commandAfterCompute(const VkCommandBuffer cmdBuffer)
{
    commandBufferWriteBarrierBeforeHostRead(m_context, cmdBuffer, m_imageBuffer->get(),
                                            m_imageSizeBytes + m_dstViewOffset);
}

class LoadStoreTest : public TestCase
{
public:
    enum TestFlags
    {
        FLAG_SINGLE_LAYER_BIND = (1 << 0), //!< Run the shader multiple times, each time binding a different layer.
        FLAG_RESTRICT_IMAGES   = (1 << 1), //!< If given, images in the shader will be qualified with "restrict".
        FLAG_DECLARE_FORMAT_IN_SHADER_READS  = (1 << 2), //!< Declare the format of images being read in the shader code
        FLAG_DECLARE_FORMAT_IN_SHADER_WRITES = (1 << 3), //!< Declare the format of images being read in the shader code
        FLAG_MINALIGN             = (1 << 4), //!< Use bufferview offset that matches the advertised minimum alignment
        FLAG_UNIFORM_TEXEL_BUFFER = (1 << 5), //!< Load from a uniform texel buffer rather than a storage texel buffer
    };

    LoadStoreTest(tcu::TestContext &testCtx, const std::string &name, const Texture &texture, const VkFormat format,
                  const VkFormat imageFormat, const VkImageTiling tiling,
                  const uint32_t flags = (FLAG_DECLARE_FORMAT_IN_SHADER_READS | FLAG_DECLARE_FORMAT_IN_SHADER_WRITES),
                  const bool imageLoadStoreLodAMD = false);

    virtual void checkSupport(Context &context) const;
    void makePrograms(SourceCollections &programCollection, const DeviceScopeType deviceScopeType = DEVICESCOPE_NONE,
                      const bool draw = false, const uint32_t verticesCount = 0u) const;
    virtual void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

protected:
    const Texture m_texture;
    const VkFormat m_format;      //!< Format as accessed in the shader
    const VkFormat m_imageFormat; //!< Storage format
    const VkImageTiling m_tiling; //!< Image Tiling
    const bool
        m_declareFormatInShaderReads; //!< Whether the shader will specify the format layout qualifier of images being read from.
    const bool
        m_declareFormatInShaderWrites; //!< Whether the shader will specify the format layout qualifier of images being written to.
    const bool m_singleLayerBind;
    const bool m_restrictImages;
    const bool m_minalign;
    bool m_bufferLoadUniform;
    const bool m_imageLoadStoreLodAMD;
};

LoadStoreTest::LoadStoreTest(tcu::TestContext &testCtx, const std::string &name, const Texture &texture,
                             const VkFormat format, const VkFormat imageFormat, const VkImageTiling tiling,
                             const uint32_t flags, const bool imageLoadStoreLodAMD)
    : TestCase(testCtx, name)
    , m_texture(texture)
    , m_format(format)
    , m_imageFormat(imageFormat)
    , m_tiling(tiling)
    , m_declareFormatInShaderReads((flags & FLAG_DECLARE_FORMAT_IN_SHADER_READS) != 0)
    , m_declareFormatInShaderWrites((flags & FLAG_DECLARE_FORMAT_IN_SHADER_WRITES) != 0)
    , m_singleLayerBind((flags & FLAG_SINGLE_LAYER_BIND) != 0)
    , m_restrictImages((flags & FLAG_RESTRICT_IMAGES) != 0)
    , m_minalign((flags & FLAG_MINALIGN) != 0)
    , m_bufferLoadUniform((flags & FLAG_UNIFORM_TEXEL_BUFFER) != 0)
    , m_imageLoadStoreLodAMD(imageLoadStoreLodAMD)
{
    if (m_singleLayerBind)
        DE_ASSERT(m_texture.numLayers() > 1);

    DE_ASSERT(formatsAreCompatible(m_format, m_imageFormat));
}

void LoadStoreTest::checkSupport(Context &context) const
{
#ifndef CTS_USES_VULKANSC
    if (m_format == VK_FORMAT_A8_UNORM_KHR || m_format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
    if (m_imageFormat == VK_FORMAT_A8_UNORM_KHR || m_imageFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");

    const VkFormatProperties3 formatProperties(context.getFormatProperties(m_format));
    const VkFormatProperties3 imageFormatProperties(context.getFormatProperties(m_imageFormat));

    const auto &tilingFeatures = (m_tiling == vk::VK_IMAGE_TILING_OPTIMAL) ? formatProperties.optimalTilingFeatures :
                                                                             formatProperties.linearTilingFeatures;
    const auto &imageTilingFeatures = (m_tiling == vk::VK_IMAGE_TILING_OPTIMAL) ?
                                          imageFormatProperties.optimalTilingFeatures :
                                          imageFormatProperties.linearTilingFeatures;

    if (m_imageLoadStoreLodAMD)
        context.requireDeviceFunctionality("VK_AMD_shader_image_load_store_lod");

    if (!m_bufferLoadUniform && !m_declareFormatInShaderReads &&
        !(tilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for unformatted loads via storage images");

    if (m_texture.type() == IMAGE_TYPE_BUFFER && !m_declareFormatInShaderReads &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for unformatted loads via buffers");

    if (!m_declareFormatInShaderWrites && !(tilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for unformatted stores via storage images");

    if (m_texture.type() == IMAGE_TYPE_BUFFER && !m_declareFormatInShaderWrites &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for unformatted stores via buffers");

    if (m_texture.type() == IMAGE_TYPE_CUBE_ARRAY)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(tilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(imageTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");

    if (m_texture.type() == IMAGE_TYPE_BUFFER &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(imageTilingFeatures))
        TCU_THROW(NotSupportedError, "Underlying format not supported at all for images");

    if ((m_texture.type() == IMAGE_TYPE_BUFFER) && !(imageFormatProperties.bufferFeatures))
        TCU_THROW(NotSupportedError, "Underlying format not supported at all for buffers");

    if (formatHasThreeComponents(m_format) && m_bufferLoadUniform)
    {
        // When the source buffer is three-component, the destination buffer is single-component.
        VkFormat dstFormat = getSingleComponentFormat(m_format);
        const VkFormatProperties3 dstFormatProperties(context.getFormatProperties(dstFormat));

        if (m_texture.type() == IMAGE_TYPE_BUFFER &&
            !(dstFormatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
            TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");
    }
    else if (m_texture.type() == IMAGE_TYPE_BUFFER &&
             !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");

    if (m_bufferLoadUniform && m_texture.type() == IMAGE_TYPE_BUFFER &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for uniform texel buffers");
#else
    const vk::VkFormatProperties formatProperties(
        vk::getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), m_format));
    const vk::VkFormatProperties imageFormatProperties(vk::getPhysicalDeviceFormatProperties(
        context.getInstanceInterface(), context.getPhysicalDevice(), m_imageFormat));

    const auto tilingFeatures = (m_tiling == vk::VK_IMAGE_TILING_OPTIMAL) ? formatProperties.optimalTilingFeatures :
                                                                            formatProperties.linearTilingFeatures;
    const auto imageTilingFeatures = (m_tiling == vk::VK_IMAGE_TILING_OPTIMAL) ?
                                         imageFormatProperties.optimalTilingFeatures :
                                         imageFormatProperties.linearTilingFeatures;

    if (m_imageLoadStoreLodAMD)
        context.requireDeviceFunctionality("VK_AMD_shader_image_load_store_lod");

    if (!m_bufferLoadUniform && !m_declareFormatInShaderReads)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT);

    if (!m_declareFormatInShaderWrites)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT);

    if (m_texture.type() == IMAGE_TYPE_CUBE_ARRAY)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(tilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(imageTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");

    if (m_texture.type() == IMAGE_TYPE_BUFFER &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");

    if ((m_texture.type() != IMAGE_TYPE_BUFFER) && !(imageTilingFeatures))
        TCU_THROW(NotSupportedError, "Underlying format not supported at all for images");

    if ((m_texture.type() == IMAGE_TYPE_BUFFER) && !(imageFormatProperties.bufferFeatures))
        TCU_THROW(NotSupportedError, "Underlying format not supported at all for buffers");

    if (formatHasThreeComponents(m_format) && m_bufferLoadUniform)
    {
        // When the source buffer is three-component, the destination buffer is single-component.
        VkFormat dstFormat = getSingleComponentFormat(m_format);
        const vk::VkFormatProperties dstFormatProperties(vk::getPhysicalDeviceFormatProperties(
            context.getInstanceInterface(), context.getPhysicalDevice(), dstFormat));

        if (m_texture.type() == IMAGE_TYPE_BUFFER &&
            !(dstFormatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
            TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");
    }
    else if (m_texture.type() == IMAGE_TYPE_BUFFER &&
             !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage texel buffers");

    if (m_bufferLoadUniform && m_texture.type() == IMAGE_TYPE_BUFFER &&
        !(formatProperties.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for uniform texel buffers");
#endif // CTS_USES_VULKANSC

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkImageFormatProperties vkImageFormatProperties;
    const auto result = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, m_imageFormat, mapImageType(m_texture.type()), m_tiling,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &vkImageFormatProperties);

    if (result != VK_SUCCESS)
    {
        if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format unsupported for tiling");
        else
            TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
    }

    if (vkImageFormatProperties.maxArrayLayers < (uint32_t)m_texture.numLayers())
    {
        TCU_THROW(NotSupportedError, "This format and tiling combination does not support this number of aray layers");
    }

    if (vkImageFormatProperties.maxMipLevels < (uint32_t)m_texture.numMipmapLevels())
    {
        TCU_THROW(NotSupportedError, "This format and tiling combination does not support this number of miplevels");
    }
}

void LoadStoreTest::makePrograms(SourceCollections &programCollection, const DeviceScopeType deviceScopeType,
                                 const bool draw, const uint32_t verticesCount) const
{
    const tcu::TextureFormat texFormat = mapVkFormat(m_format);
    const int dimension                = (m_singleLayerBind ? m_texture.layerDimension() : m_texture.dimension());
    const ImageType usedImageType =
        (m_singleLayerBind ? getImageTypeForSingleLayer(m_texture.type()) : m_texture.type());
    const bool noFormats                    = (!m_declareFormatInShaderReads && !m_declareFormatInShaderWrites);
    const std::string formatQualifierStr    = (noFormats ? "" : getShaderImageFormatQualifierStr(m_format));
    const std::string uniformTypeStr        = getFormatPrefix(texFormat) + "textureBuffer";
    const std::string imageTypeStr          = getShaderImageType(texFormat, usedImageType);
    const std::string maybeRestrictStr      = (m_restrictImages ? "restrict " : "");
    const std::string xMax                  = de::toString(m_texture.size().x() - 1);
    const std::string xMaxSub               = (deviceScopeType == DEVICESCOPE_LOAD) ? "" : (xMax + "-");
    const std::string maybeFmtQualStrReads  = m_declareFormatInShaderReads ? ", " + formatQualifierStr : "";
    const std::string maybeFmtQualStrWrites = m_declareFormatInShaderWrites ? ", " + formatQualifierStr : "";

    // For device scope tests, the output of first shader will become the input for the second shader
    const std::string inputBinding  = (deviceScopeType == DEVICESCOPE_LOAD) ? "1" : "0";
    const std::string outputBinding = (deviceScopeType == DEVICESCOPE_LOAD) ? "0" : "1";

    const std::string deviceScopeCommonHeader("#pragma use_vulkan_memory_model\n"
                                              "#extension GL_KHR_memory_scope_semantics : enable\n");
    if (!draw)
    {
        std::string shaderName = "comp";

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n";

        if (deviceScopeType != DEVICESCOPE_NONE)
        {
            src << deviceScopeCommonHeader;
        }

        if (!m_declareFormatInShaderReads || !m_declareFormatInShaderWrites)
        {
            src << "#extension GL_EXT_shader_image_load_formatted : require\n";
        }

        if (m_imageLoadStoreLodAMD)
        {
            src << "#extension GL_AMD_shader_image_load_store_lod : require\n";
        }

        src << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";

        if (m_bufferLoadUniform)
            src << "layout (binding = " << inputBinding << ") uniform " << uniformTypeStr << " u_image0;\n";
        else
            src << "layout (binding = " << inputBinding << maybeFmtQualStrReads << ") " << maybeRestrictStr
                << "readonly uniform " << imageTypeStr << " u_image0;\n";

        // For three-component formats used with UNIFORM_TEXEL_BUFFER, the dst buffer is single-component and the shader uses this
        // dst buffer to create 3 component-wise stores. We always use the format qualifier for the dst buffer, except when splitting
        // it up.
        if (formatHasThreeComponents(m_format) && m_bufferLoadUniform)
            src << "layout (binding = " << outputBinding << ") " << maybeRestrictStr << "writeonly uniform "
                << imageTypeStr << " u_image1;\n";
        else
            src << "layout (binding = " << outputBinding << maybeFmtQualStrWrites << ") " << maybeRestrictStr
                << "writeonly uniform " << imageTypeStr << " u_image1;\n";

        src << "\n"
            << "void main (void)\n"
            << "{\n";

        if (deviceScopeType == DEVICESCOPE_LOAD)
        {
            src << "    memoryBarrier(gl_ScopeDevice, gl_StorageSemanticsImage, gl_SemanticsAcquireRelease | "
                   "gl_SemanticsMakeVisible);\n";
        }

        switch (dimension)
        {
        default:
            DE_ASSERT(0); // fallthrough
        case 1:
            if (m_bufferLoadUniform)
            {
                // Expand the store into 3 component-wise stores.
                std::string type = getFormatPrefix(texFormat) + "vec4";
                src << "    int pos = int(gl_GlobalInvocationID.x);\n"
                       "    "
                    << type << " t = texelFetch(u_image0, " + xMax + "-pos);\n";
                if (formatHasThreeComponents(m_format))
                {
                    src << "    imageStore(u_image1, 3*pos+0, " << type << "(t.x));\n";
                    src << "    imageStore(u_image1, 3*pos+1, " << type << "(t.y));\n";
                    src << "    imageStore(u_image1, 3*pos+2, " << type << "(t.z));\n";
                }
                else
                    src << "    imageStore(u_image1, pos, t);\n";
            }
            else if (m_imageLoadStoreLodAMD)
            {
                src << "    int pos = int(gl_GlobalInvocationID.x);\n";

                for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
                {
                    std::string xMaxSize = de::toString(deMax32(((m_texture.layerSize().x() >> levelNdx) - 1), 1u));
                    src << "    imageStoreLodAMD(u_image1, pos, " + de::toString(levelNdx) +
                               ", imageLoadLodAMD(u_image0, " + xMaxSize + "-pos, " + de::toString(levelNdx) + "));\n";
                }
            }
            else
            {
                src << "    int pos = int(gl_GlobalInvocationID.x);\n"
                       "    imageStore(u_image1, pos, imageLoad(u_image0, " +
                           xMaxSub + "pos));\n";
            }
            break;
        case 2:
            if (m_imageLoadStoreLodAMD)
            {
                src << "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n";

                for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
                {
                    std::string xMaxSize = de::toString(deMax32(((m_texture.layerSize().x() >> levelNdx) - 1), 1u));
                    src << "    imageStoreLodAMD(u_image1, pos, " + de::toString(levelNdx) +
                               ", imageLoadLodAMD(u_image0, ivec2(" + xMaxSize + "-pos.x, pos.y), " +
                               de::toString(levelNdx) + "));\n";
                }
            }
            else
            {
                src << "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
                       "    imageStore(u_image1, pos, imageLoad(u_image0, ivec2(" +
                           xMaxSub + "pos.x, pos.y)));\n";
            }
            break;
        case 3:
            if (m_imageLoadStoreLodAMD)
            {
                src << "    ivec3 pos = ivec3(gl_GlobalInvocationID);\n";

                for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
                {
                    std::string xMaxSize = de::toString(deMax32(((m_texture.layerSize().x() >> levelNdx) - 1), 1u));
                    src << "    imageStoreLodAMD(u_image1, pos, " + de::toString(levelNdx) +
                               ", imageLoadLodAMD(u_image0, ivec3(" + xMaxSize + "-pos.x, pos.y, pos.z), " +
                               de::toString(levelNdx) + "));\n";
                }
            }
            else
            {
                src << "    ivec3 pos = ivec3(gl_GlobalInvocationID);\n"
                       "    imageStore(u_image1, pos, imageLoad(u_image0, ivec3(" +
                           xMaxSub + "pos.x, pos.y, pos.z)));\n";
            }
            break;
        }

        if (deviceScopeType == DEVICESCOPE_STORE)
        {
            src << "    memoryBarrier(gl_ScopeDevice, gl_StorageSemanticsImage, gl_SemanticsAcquireRelease | "
                   "gl_SemanticsMakeAvailable);\n";
        }

        src << "}\n";

        if (deviceScopeType == DEVICESCOPE_STORE)
            shaderName += "_store";
        else if (deviceScopeType == DEVICESCOPE_LOAD)
            shaderName += "_load";

        programCollection.glslSources.add(shaderName) << glu::ComputeSource(src.str());
    }
    else
    {
        std::ostringstream srcVert;

        srcVert << glu::getGLSLVersionDeclaration(glu::GLSLVersion::GLSL_VERSION_450) << "\n";

        srcVert << "vec4 positions[" << verticesCount << "] = vec4[](\n"
                << "    vec4(-1.0f, -1.0f, 1.0f, 1.0f),\n"
                << "    vec4( 1.0f, -1.0f, 1.0f, 1.0f),\n"
                << "    vec4(-1.0f,  1.0f, 1.0f, 1.0f),\n"
                << "    vec4(-1.0f,  1.0f, 1.0f, 1.0f),\n"
                << "    vec4( 1.0f, -1.0f, 1.0f, 1.0f),\n"
                << "    vec4( 1.0f,  1.0f, 1.0f, 1.0f)\n"
                << ");\n"
                << "\n"
                << "void main() {\n"
                << "    gl_Position = positions[gl_VertexIndex];\n"
                << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(srcVert.str());

        std::ostringstream srcFrag;

        srcFrag << glu::getGLSLVersionDeclaration(glu::GLSLVersion::GLSL_VERSION_450) << "\n";
        srcFrag << deviceScopeCommonHeader;

        srcFrag << "layout (binding = " << inputBinding << maybeFmtQualStrReads << ") " << maybeRestrictStr
                << "readonly uniform " << imageTypeStr << " u_image0;\n";

        const std::string signednessPrefix = isUintFormat(m_format) ? "u" : isIntFormat(m_format) ? "i" : "";
        const std::string colorType        = signednessPrefix + "vec4";

        srcFrag << "layout (location=0) out " << colorType << " outcolor;\n";
        srcFrag << "\n"
                << "void main (void)\n"
                << "{\n";

        if (deviceScopeType == DEVICESCOPE_LOAD)
        {
            srcFrag << "    memoryBarrier(gl_ScopeDevice, gl_StorageSemanticsImage, gl_SemanticsAcquireRelease | "
                       "gl_SemanticsMakeVisible);\n";
        }

        switch (dimension)
        {
        default:
            DE_ASSERT(0); // fallthrough
        case 1:
        {
            srcFrag << "    int pos = int(gl_FragCoord.x);\n"
                       "    outcolor = imageLoad(u_image0, pos);\n";
        }
        break;
        case 2:
        {
            srcFrag << "    ivec2 pos = ivec2(gl_FragCoord.xy);\n"
                       "    outcolor = imageLoad(u_image0, ivec2(pos.x, pos.y));\n";
        }
        break;
        case 3:
        {
            srcFrag << "    ivec3 pos = ivec3(gl_FragCoord.xyz);\n"
                       "    outcolor = imageLoad(u_image0, ivec3(pos.x, pos.y, pos.z));\n";
        }
        break;
        }
        srcFrag << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(srcFrag.str());
    }
}

void LoadStoreTest::initPrograms(SourceCollections &programCollection) const
{
    makePrograms(programCollection);
}

//! Load/store test base implementation
class LoadStoreTestInstance : public BaseTestInstance
{
public:
    LoadStoreTestInstance(Context &context, const Texture &texture, const VkFormat format, const VkFormat imageFormat,
                          const bool declareImageFormatInShader, const bool singleLayerBind, const bool minalign,
                          const bool bufferLoadUniform);

protected:
    virtual BufferWithMemory *getResultBuffer(void) const = 0; //!< Get the buffer that contains the result image

    tcu::TestStatus verifyResult(void);

    // Add empty implementations for functions that might be not needed
    void commandBeforeCompute(const VkCommandBuffer)
    {
    }
    void commandBetweenShaderInvocations(const VkCommandBuffer)
    {
    }
    void commandAfterCompute(const VkCommandBuffer)
    {
    }

    de::MovePtr<BufferWithMemory> m_imageBuffer; //!< Source data and helper buffer
    const VkDeviceSize m_imageSizeBytes;
    const VkFormat m_imageFormat;       //!< Image format (for storage, may be different than texture format)
    tcu::TextureLevel m_referenceImage; //!< Used as input data and later to verify result image

    bool m_bufferLoadUniform;
    VkDescriptorType m_bufferLoadDescriptorType;
    VkBufferUsageFlagBits m_bufferLoadUsageBit;
};

LoadStoreTestInstance::LoadStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                             const VkFormat imageFormat, const bool declareImageFormatInShader,
                                             const bool singleLayerBind, const bool minalign,
                                             const bool bufferLoadUniform)
    : BaseTestInstance(context, texture, format, declareImageFormatInShader, singleLayerBind, minalign,
                       bufferLoadUniform)
    , m_imageSizeBytes(getImageSizeBytes(texture.size(), format))
    , m_imageFormat(imageFormat)
    , m_referenceImage(generateReferenceImage(texture.size(), imageFormat, format))
    , m_bufferLoadUniform(bufferLoadUniform)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    m_bufferLoadDescriptorType =
        m_bufferLoadUniform ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    m_bufferLoadUsageBit =
        m_bufferLoadUniform ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

    // A helper buffer with enough space to hold the whole image.

    m_imageBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator,
                             makeBufferCreateInfo(m_imageSizeBytes + m_srcViewOffset,
                                                  m_bufferLoadUsageBit | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                             MemoryRequirement::HostVisible));

    // Copy reference data to buffer for subsequent upload to image.

    const Allocation &alloc = m_imageBuffer->getAllocation();
    deMemcpy((char *)alloc.getHostPtr() + m_srcViewOffset, m_referenceImage.getAccess().getDataPtr(),
             static_cast<size_t>(m_imageSizeBytes));
    flushAlloc(vk, device, alloc);
}

tcu::TestStatus LoadStoreTestInstance::verifyResult(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    // Apply the same transformation as done in the shader
    const tcu::PixelBufferAccess reference = m_referenceImage.getAccess();
    flipHorizontally(reference);

    const Allocation &alloc = getResultBuffer()->getAllocation();
    invalidateAlloc(vk, device, alloc);
    const tcu::ConstPixelBufferAccess result(mapVkFormat(m_imageFormat), m_texture.size(),
                                             (const char *)alloc.getHostPtr() + m_dstViewOffset);

    if (comparePixelBuffers(m_context.getTestContext().getLog(), m_texture, m_imageFormat, reference, result))
        return tcu::TestStatus::pass("Passed");
    else
        return tcu::TestStatus::fail("Image comparison failed");
}

//! Load/store test for images
class ImageLoadStoreTestInstance : public LoadStoreTestInstance
{
public:
    ImageLoadStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                               const VkFormat imageFormat, const VkImageTiling tiling,
                               const bool declareImageFormatInShader, const bool singleLayerBind, const bool minalign,
                               const bool bufferLoadUniform);

protected:
    VkDescriptorSetLayout commonPrepareDescriptors(const VkShaderStageFlags stageFlags);
    VkDescriptorSetLayout prepareDescriptors(void);
    void commandBeforeCompute(const VkCommandBuffer cmdBuffer);
    void commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer);
    void commandAfterCompute(const VkCommandBuffer cmdBuffer);

    void commonBindDescriptors(const VkCommandBuffer cmdBuffer, const VkPipelineBindPoint pipelineBinding,
                               const VkPipelineLayout pipelineLayout, const int layerNdx);
    void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const int layerNdx);

    BufferWithMemory *getResultBuffer(void) const
    {
        return m_imageBuffer.get();
    }

    de::MovePtr<Image> m_imageSrc;
    de::MovePtr<Image> m_imageDst;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    std::vector<SharedVkDescriptorSet> m_allDescriptorSets;
    std::vector<SharedVkImageView> m_allSrcImageViews;
    std::vector<SharedVkImageView> m_allDstImageViews;
};

ImageLoadStoreTestInstance::ImageLoadStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                                       const VkFormat imageFormat, const VkImageTiling tiling,
                                                       const bool declareImageFormatInShader,
                                                       const bool singleLayerBind, const bool minalign,
                                                       const bool bufferLoadUniform)
    : LoadStoreTestInstance(context, texture, format, imageFormat, declareImageFormatInShader, singleLayerBind,
                            minalign, bufferLoadUniform)
    , m_allDescriptorSets(texture.numLayers())
    , m_allSrcImageViews(texture.numLayers())
    , m_allDstImageViews(texture.numLayers())
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const VkImageCreateFlags imageFlags =
        (m_format == m_imageFormat ? 0u : (VkImageCreateFlags)VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);

    m_imageSrc =
        de::MovePtr<Image>(new Image(vk, device, allocator,
                                     makeImageCreateInfo(m_texture, m_imageFormat,
                                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                         imageFlags, tiling),
                                     MemoryRequirement::Any));

    m_imageDst =
        de::MovePtr<Image>(new Image(vk, device, allocator,
                                     makeImageCreateInfo(m_texture, m_imageFormat,
                                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                                         imageFlags, tiling),
                                     MemoryRequirement::Any));
}

VkDescriptorSetLayout ImageLoadStoreTestInstance::commonPrepareDescriptors(const VkShaderStageFlags stageFlags)
{
    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const int numLayers   = m_texture.numLayers();
    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stageFlags)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stageFlags)
                                .build(vk, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numLayers)
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numLayers)
                           .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numLayers);

    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);

    if (m_singleLayerBind)
    {
        for (int layerNdx = 0; layerNdx < numLayers; ++layerNdx)
        {
            const VkImageViewType viewType = mapImageViewType(getImageTypeForSingleLayer(m_texture.type()));
            const VkImageSubresourceRange subresourceRange =
                makeImageSubresourceRange(aspectMask, 0u, 1u, layerNdx, 1u);

            m_allDescriptorSets[layerNdx] =
                makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
            m_allSrcImageViews[layerNdx] =
                makeVkSharedPtr(makeImageView(vk, device, m_imageSrc->get(), viewType, m_format, subresourceRange));
            m_allDstImageViews[layerNdx] =
                makeVkSharedPtr(makeImageView(vk, device, m_imageDst->get(), viewType, m_format, subresourceRange));
        }
    }
    else // bind all layers at once
    {
        const VkImageViewType viewType                 = mapImageViewType(m_texture.type());
        const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(aspectMask, 0u, 1u, 0u, numLayers);

        m_allDescriptorSets[0] =
            makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
        m_allSrcImageViews[0] =
            makeVkSharedPtr(makeImageView(vk, device, m_imageSrc->get(), viewType, m_format, subresourceRange));
        m_allDstImageViews[0] =
            makeVkSharedPtr(makeImageView(vk, device, m_imageDst->get(), viewType, m_format, subresourceRange));
    }

    return *m_descriptorSetLayout; // not passing the ownership
}

VkDescriptorSetLayout ImageLoadStoreTestInstance::prepareDescriptors(void)
{
    return commonPrepareDescriptors(VK_SHADER_STAGE_COMPUTE_BIT);
}

void ImageLoadStoreTestInstance::commonBindDescriptors(const VkCommandBuffer cmdBuffer,
                                                       const VkPipelineBindPoint pipelineBinding,
                                                       const VkPipelineLayout pipelineLayout, const int layerNdx)
{
    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkDescriptorSet descriptorSet = **m_allDescriptorSets[layerNdx];
    const VkImageView srcImageView      = **m_allSrcImageViews[layerNdx];
    const VkImageView dstImageView      = **m_allDstImageViews[layerNdx];

    const VkDescriptorImageInfo descriptorSrcImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, srcImageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorImageInfo descriptorDstImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, dstImageView, VK_IMAGE_LAYOUT_GENERAL);

    DescriptorSetUpdateBuilder()
        .writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &descriptorSrcImageInfo)
        .writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &descriptorDstImageInfo)
        .update(vk, device);
    vk.cmdBindDescriptorSets(cmdBuffer, pipelineBinding, pipelineLayout, 0u, 1u, &descriptorSet, 0u, nullptr);
}

void ImageLoadStoreTestInstance::commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer,
                                                                const VkPipelineLayout pipelineLayout,
                                                                const int layerNdx)
{
    commonBindDescriptors(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, layerNdx);
}

void ImageLoadStoreTestInstance::commandBeforeCompute(const VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk            = m_context.getDeviceInterface();
    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);

    const VkImageSubresourceRange fullImageSubresourceRange =
        makeImageSubresourceRange(aspectMask, 0u, 1u, 0u, m_texture.numLayers());
    {
        const VkImageMemoryBarrier preCopyImageBarriers[] = {
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_imageSrc->get(), fullImageSubresourceRange),
            makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   m_imageDst->get(), fullImageSubresourceRange)};

        const VkBufferMemoryBarrier barrierFlushHostWriteBeforeCopy =
            makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, m_imageBuffer->get(), 0ull,
                                    m_imageSizeBytes + m_srcViewOffset);

        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &barrierFlushHostWriteBeforeCopy,
                              DE_LENGTH_OF_ARRAY(preCopyImageBarriers), preCopyImageBarriers);
    }
    {
        const VkImageMemoryBarrier barrierAfterCopy = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, m_imageSrc->get(), fullImageSubresourceRange);

        const VkBufferImageCopy copyRegion = makeBufferImageCopy(m_texture, aspectMask);

        vk.cmdCopyBufferToImage(cmdBuffer, m_imageBuffer->get(), m_imageSrc->get(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &barrierAfterCopy);
    }
}

void ImageLoadStoreTestInstance::commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer)
{
    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);
    commandImageWriteBarrierBetweenShaderInvocations(m_context, cmdBuffer, m_imageDst->get(), m_texture, aspectMask);
}

void ImageLoadStoreTestInstance::commandAfterCompute(const VkCommandBuffer cmdBuffer)
{
    const VkImageAspectFlags &aspectMask = getImageAspect(m_format);
    commandCopyImageToBuffer(m_context, cmdBuffer, m_imageDst->get(), m_imageBuffer->get(), m_imageSizeBytes, m_texture,
                             aspectMask);
}

//! Load/store Lod AMD test for images
class ImageLoadStoreLodAMDTestInstance : public BaseTestInstance
{
public:
    ImageLoadStoreLodAMDTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                     const VkFormat imageFormat, const bool declareImageFormatInShader,
                                     const bool singleLayerBind, const bool minalign, const bool bufferLoadUniform);

protected:
    VkDescriptorSetLayout prepareDescriptors(void);
    void commandBeforeCompute(const VkCommandBuffer cmdBuffer);
    void commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer);
    void commandAfterCompute(const VkCommandBuffer cmdBuffer);

    void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const int layerNdx);

    BufferWithMemory *getResultBuffer(void) const
    {
        return m_imageBuffer.get();
    }
    tcu::TestStatus verifyResult(void);

    de::MovePtr<BufferWithMemory> m_imageBuffer; //!< Source data and helper buffer
    const VkDeviceSize m_imageSizeBytes;
    const VkFormat m_imageFormat; //!< Image format (for storage, may be different than texture format)
    std::vector<tcu::TextureLevel> m_referenceImages; //!< Used as input data and later to verify result image

    bool m_bufferLoadUniform;
    VkDescriptorType m_bufferLoadDescriptorType;
    VkBufferUsageFlagBits m_bufferLoadUsageBit;

    de::MovePtr<Image> m_imageSrc;
    de::MovePtr<Image> m_imageDst;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    std::vector<SharedVkDescriptorSet> m_allDescriptorSets;
    std::vector<SharedVkImageView> m_allSrcImageViews;
    std::vector<SharedVkImageView> m_allDstImageViews;
};

ImageLoadStoreLodAMDTestInstance::ImageLoadStoreLodAMDTestInstance(Context &context, const Texture &texture,
                                                                   const VkFormat format, const VkFormat imageFormat,
                                                                   const bool declareImageFormatInShader,
                                                                   const bool singleLayerBind, const bool minalign,
                                                                   const bool bufferLoadUniform)
    : BaseTestInstance(context, texture, format, declareImageFormatInShader, singleLayerBind, minalign,
                       bufferLoadUniform)
    , m_imageSizeBytes(getMipmapImageTotalSizeBytes(texture, format))
    , m_imageFormat(imageFormat)
    , m_bufferLoadUniform(bufferLoadUniform)
    , m_allDescriptorSets(texture.numLayers())
    , m_allSrcImageViews(texture.numLayers())
    , m_allDstImageViews(texture.numLayers())
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const VkImageCreateFlags imageFlags =
        (m_format == m_imageFormat ? 0u : (VkImageCreateFlags)VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);

    const VkSampleCountFlagBits samples = static_cast<VkSampleCountFlagBits>(
        m_texture.numSamples()); // integer and bit mask are aligned, so we can cast like this

    for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
    {
        tcu::TextureLevel referenceImage = generateReferenceImage(texture.size(levelNdx), imageFormat, format);
        m_referenceImages.push_back(referenceImage);
    }

    m_bufferLoadDescriptorType =
        m_bufferLoadUniform ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    m_bufferLoadUsageBit =
        m_bufferLoadUniform ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

    // A helper buffer with enough space to hold the whole image.
    m_imageBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator,
                             makeBufferCreateInfo(m_imageSizeBytes + m_srcViewOffset,
                                                  m_bufferLoadUsageBit | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                             MemoryRequirement::HostVisible));

    // Copy reference data to buffer for subsequent upload to image.
    {
        const Allocation &alloc   = m_imageBuffer->getAllocation();
        VkDeviceSize bufferOffset = 0u;
        for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
        {
            deMemcpy((char *)alloc.getHostPtr() + m_srcViewOffset + bufferOffset,
                     m_referenceImages[levelNdx].getAccess().getDataPtr(),
                     static_cast<size_t>(getMipmapLevelImageSizeBytes(m_texture, m_imageFormat, levelNdx)));
            bufferOffset += getMipmapLevelImageSizeBytes(m_texture, m_imageFormat, levelNdx);
        }
        flushAlloc(vk, device, alloc);
    }

    {
        const VkImageCreateInfo imageParamsSrc = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            (isCube(m_texture) ? (VkImageCreateFlags)VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) |
                imageFlags,                                               // VkImageCreateFlags flags;
            mapImageType(m_texture.type()),                               // VkImageType imageType;
            m_imageFormat,                                                // VkFormat format;
            makeExtent3D(m_texture.layerSize()),                          // VkExtent3D extent;
            (uint32_t)m_texture.numMipmapLevels(),                        // uint32_t mipLevels;
            (uint32_t)m_texture.numLayers(),                              // uint32_t arrayLayers;
            samples,                                                      // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                      // VkImageTiling tiling;
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                    // VkSharingMode sharingMode;
            0u,                                                           // uint32_t queueFamilyIndexCount;
            nullptr,                                                      // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                    // VkImageLayout initialLayout;
        };

        m_imageSrc = de::MovePtr<Image>(new Image(vk, device, allocator, imageParamsSrc, MemoryRequirement::Any));
    }

    {
        const VkImageCreateInfo imageParamsDst = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            (isCube(m_texture) ? (VkImageCreateFlags)VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0u) |
                imageFlags,                                               // VkImageCreateFlags flags;
            mapImageType(m_texture.type()),                               // VkImageType imageType;
            m_imageFormat,                                                // VkFormat format;
            makeExtent3D(m_texture.layerSize()),                          // VkExtent3D extent;
            (uint32_t)m_texture.numMipmapLevels(),                        // uint32_t mipLevels;
            (uint32_t)m_texture.numLayers(),                              // uint32_t arrayLayers;
            samples,                                                      // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                      // VkImageTiling tiling;
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                    // VkSharingMode sharingMode;
            0u,                                                           // uint32_t queueFamilyIndexCount;
            nullptr,                                                      // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                    // VkImageLayout initialLayout;
        };

        m_imageDst = de::MovePtr<Image>(new Image(vk, device, allocator, imageParamsDst, MemoryRequirement::Any));
    }
}

tcu::TestStatus ImageLoadStoreLodAMDTestInstance::verifyResult(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const Allocation &alloc = getResultBuffer()->getAllocation();
    invalidateAlloc(vk, device, alloc);

    VkDeviceSize bufferOffset = 0;
    for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
    {
        // Apply the same transformation as done in the shader
        const tcu::PixelBufferAccess reference = m_referenceImages[levelNdx].getAccess();
        flipHorizontally(reference);

        const tcu::ConstPixelBufferAccess result(mapVkFormat(m_imageFormat), m_texture.size(levelNdx),
                                                 (const char *)alloc.getHostPtr() + m_dstViewOffset + bufferOffset);

        if (!comparePixelBuffers(m_context.getTestContext().getLog(), m_texture, m_imageFormat, reference, result,
                                 levelNdx))
        {
            std::ostringstream errorMessage;
            errorMessage << "Image Level " << levelNdx << " comparison failed";
            return tcu::TestStatus::fail(errorMessage.str());
        }
        bufferOffset += getMipmapLevelImageSizeBytes(m_texture, m_imageFormat, levelNdx);
    }

    return tcu::TestStatus::pass("Passed");
}

VkDescriptorSetLayout ImageLoadStoreLodAMDTestInstance::prepareDescriptors(void)
{
    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const int numLayers   = m_texture.numLayers();
    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                .build(vk, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numLayers)
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numLayers)
                           .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numLayers);

    if (m_singleLayerBind)
    {
        for (int layerNdx = 0; layerNdx < numLayers; ++layerNdx)
        {
            const VkImageViewType viewType = mapImageViewType(getImageTypeForSingleLayer(m_texture.type()));
            const VkImageSubresourceRange subresourceRange =
                makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, m_texture.numMipmapLevels(), layerNdx, 1u);

            m_allDescriptorSets[layerNdx] =
                makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
            m_allSrcImageViews[layerNdx] =
                makeVkSharedPtr(makeImageView(vk, device, m_imageSrc->get(), viewType, m_format, subresourceRange));
            m_allDstImageViews[layerNdx] =
                makeVkSharedPtr(makeImageView(vk, device, m_imageDst->get(), viewType, m_format, subresourceRange));
        }
    }
    else // bind all layers at once
    {
        const VkImageViewType viewType = mapImageViewType(m_texture.type());
        const VkImageSubresourceRange subresourceRange =
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, m_texture.numMipmapLevels(), 0u, numLayers);

        m_allDescriptorSets[0] =
            makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
        m_allSrcImageViews[0] =
            makeVkSharedPtr(makeImageView(vk, device, m_imageSrc->get(), viewType, m_format, subresourceRange));
        m_allDstImageViews[0] =
            makeVkSharedPtr(makeImageView(vk, device, m_imageDst->get(), viewType, m_format, subresourceRange));
    }

    return *m_descriptorSetLayout; // not passing the ownership
}

void ImageLoadStoreLodAMDTestInstance::commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer,
                                                                      const VkPipelineLayout pipelineLayout,
                                                                      const int layerNdx)
{
    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkDescriptorSet descriptorSet = **m_allDescriptorSets[layerNdx];
    const VkImageView srcImageView      = **m_allSrcImageViews[layerNdx];
    const VkImageView dstImageView      = **m_allDstImageViews[layerNdx];

    const VkDescriptorImageInfo descriptorSrcImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, srcImageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorImageInfo descriptorDstImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, dstImageView, VK_IMAGE_LAYOUT_GENERAL);

    DescriptorSetUpdateBuilder()
        .writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &descriptorSrcImageInfo)
        .writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &descriptorDstImageInfo)
        .update(vk, device);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                             nullptr);
}

void ImageLoadStoreLodAMDTestInstance::commandBeforeCompute(const VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk                               = m_context.getDeviceInterface();
    const VkImageSubresourceRange fullImageSubresourceRange = makeImageSubresourceRange(
        VK_IMAGE_ASPECT_COLOR_BIT, 0u, m_texture.numMipmapLevels(), 0u, m_texture.numLayers());
    {
        const VkImageMemoryBarrier preCopyImageBarriers[] = {
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_imageSrc->get(), fullImageSubresourceRange),
            makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   m_imageDst->get(), fullImageSubresourceRange)};

        const VkBufferMemoryBarrier barrierFlushHostWriteBeforeCopy =
            makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, m_imageBuffer->get(), 0ull,
                                    m_imageSizeBytes + m_srcViewOffset);

        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &barrierFlushHostWriteBeforeCopy,
                              DE_LENGTH_OF_ARRAY(preCopyImageBarriers), preCopyImageBarriers);
    }
    {
        const VkImageMemoryBarrier barrierAfterCopy = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, m_imageSrc->get(), fullImageSubresourceRange);

        std::vector<VkBufferImageCopy> copyRegions;
        VkDeviceSize bufferOffset = 0u;
        for (int32_t levelNdx = 0; levelNdx < m_texture.numMipmapLevels(); levelNdx++)
        {
            const VkBufferImageCopy copyParams = {
                bufferOffset, // VkDeviceSize bufferOffset;
                0u,           // uint32_t bufferRowLength;
                0u,           // uint32_t bufferImageHeight;
                makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, levelNdx, 0u,
                                           m_texture.numLayers()), // VkImageSubresourceLayers imageSubresource;
                makeOffset3D(0, 0, 0),                             // VkOffset3D imageOffset;
                makeExtent3D(m_texture.layerSize(levelNdx)),       // VkExtent3D imageExtent;
            };
            copyRegions.push_back(copyParams);
            bufferOffset += getMipmapLevelImageSizeBytes(m_texture, m_imageFormat, levelNdx);
        }

        vk.cmdCopyBufferToImage(cmdBuffer, m_imageBuffer->get(), m_imageSrc->get(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copyRegions.size(), copyRegions.data());
        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &barrierAfterCopy);
    }
}

void ImageLoadStoreLodAMDTestInstance::commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer)
{
    commandImageWriteBarrierBetweenShaderInvocations(m_context, cmdBuffer, m_imageDst->get(), m_texture);
}

void ImageLoadStoreLodAMDTestInstance::commandAfterCompute(const VkCommandBuffer cmdBuffer)
{
    commandCopyMipmapImageToBuffer(m_context, cmdBuffer, m_imageDst->get(), m_imageFormat, m_imageBuffer->get(),
                                   m_imageSizeBytes, m_texture);
}

//! Load/store test for buffers
class BufferLoadStoreTestInstance : public LoadStoreTestInstance
{
public:
    BufferLoadStoreTestInstance(Context &context, const Texture &texture, const VkFormat format,
                                const VkFormat imageFormat, const bool declareImageFormatInShader, const bool minalign,
                                const bool bufferLoadUniform);

protected:
    VkDescriptorSetLayout prepareDescriptors(void);
    void commandAfterCompute(const VkCommandBuffer cmdBuffer);

    void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const int layerNdx);

    BufferWithMemory *getResultBuffer(void) const
    {
        return m_imageBufferDst.get();
    }

    de::MovePtr<BufferWithMemory> m_imageBufferDst;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    Move<VkBufferView> m_bufferViewSrc;
    Move<VkBufferView> m_bufferViewDst;
};

BufferLoadStoreTestInstance::BufferLoadStoreTestInstance(Context &context, const Texture &texture,
                                                         const VkFormat format, const VkFormat imageFormat,
                                                         const bool declareImageFormatInShader, const bool minalign,
                                                         const bool bufferLoadUniform)
    : LoadStoreTestInstance(context, texture, format, imageFormat, declareImageFormatInShader, false, minalign,
                            bufferLoadUniform)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // Create a destination buffer.

    m_imageBufferDst = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(m_imageSizeBytes + m_dstViewOffset, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT),
        MemoryRequirement::HostVisible));
}

VkDescriptorSetLayout BufferLoadStoreTestInstance::prepareDescriptors(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(m_bufferLoadDescriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .build(vk, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(m_bufferLoadDescriptorType)
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
                           .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    VkFormat dstFormat =
        formatHasThreeComponents(m_format) && m_bufferLoadUniform ? getSingleComponentFormat(m_format) : m_format;

    m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);
    m_bufferViewSrc = makeBufferView(vk, device, m_imageBuffer->get(), m_format, m_srcViewOffset, m_imageSizeBytes);
    m_bufferViewDst = makeBufferView(vk, device, m_imageBufferDst->get(), dstFormat, m_dstViewOffset, m_imageSizeBytes);

    return *m_descriptorSetLayout; // not passing the ownership
}

void BufferLoadStoreTestInstance::commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer,
                                                                 const VkPipelineLayout pipelineLayout,
                                                                 const int layerNdx)
{
    DE_ASSERT(layerNdx == 0);
    DE_UNREF(layerNdx);

    const VkDevice device     = m_context.getDevice();
    const DeviceInterface &vk = m_context.getDeviceInterface();

    DescriptorSetUpdateBuilder()
        .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), m_bufferLoadDescriptorType,
                     &m_bufferViewSrc.get())
        .writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, &m_bufferViewDst.get())
        .update(vk, device);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &m_descriptorSet.get(),
                             0u, nullptr);
}

void BufferLoadStoreTestInstance::commandAfterCompute(const VkCommandBuffer cmdBuffer)
{
    commandBufferWriteBarrierBeforeHostRead(m_context, cmdBuffer, m_imageBufferDst->get(),
                                            m_imageSizeBytes + m_dstViewOffset);
}

TestInstance *StoreTest::createInstance(Context &context) const
{
    if (m_texture.type() == IMAGE_TYPE_BUFFER)
        return new BufferStoreTestInstance(context, m_texture, m_format, m_declareImageFormatInShader, m_minalign,
                                           m_storeConstantValue);
    else
        return new ImageStoreTestInstance(context, m_texture, m_format, m_tiling, m_declareImageFormatInShader,
                                          m_singleLayerBind, m_minalign, m_storeConstantValue);
}

TestInstance *LoadStoreTest::createInstance(Context &context) const
{
    if (m_imageLoadStoreLodAMD)
        return new ImageLoadStoreLodAMDTestInstance(context, m_texture, m_format, m_imageFormat,
                                                    m_declareFormatInShaderReads, m_singleLayerBind, m_minalign,
                                                    m_bufferLoadUniform);

    if (m_texture.type() == IMAGE_TYPE_BUFFER)
        return new BufferLoadStoreTestInstance(context, m_texture, m_format, m_imageFormat,
                                               m_declareFormatInShaderReads, m_minalign, m_bufferLoadUniform);
    else
        return new ImageLoadStoreTestInstance(context, m_texture, m_format, m_imageFormat, m_tiling,
                                              m_declareFormatInShaderReads, m_singleLayerBind, m_minalign,
                                              m_bufferLoadUniform);
}

class ImageExtendOperandTestInstance : public BaseTestInstance
{
public:
    ImageExtendOperandTestInstance(Context &context, const Texture &texture, const VkFormat readFormat,
                                   const VkFormat writeFormat, bool relaxedPrecision);

    virtual ~ImageExtendOperandTestInstance(void)
    {
    }

protected:
    VkDescriptorSetLayout prepareDescriptors(void);
    void commandBeforeCompute(const VkCommandBuffer cmdBuffer);
    void commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer);
    void commandAfterCompute(const VkCommandBuffer cmdBuffer);

    void commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const int layerNdx);

    tcu::TestStatus verifyResult(void);

protected:
    bool m_isSigned;
    tcu::TextureLevel m_inputImageData;

    de::MovePtr<Image> m_imageSrc; // source image
    SharedVkImageView m_imageSrcView;
    VkDeviceSize m_imageSrcSize;

    de::MovePtr<Image> m_imageDst; // dest image
    SharedVkImageView m_imageDstView;
    VkFormat m_imageDstFormat;
    VkDeviceSize m_imageDstSize;

    de::MovePtr<BufferWithMemory> m_buffer; // result buffer

    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    SharedVkDescriptorSet m_descriptorSet;

    bool m_relaxedPrecision;
};

ImageExtendOperandTestInstance::ImageExtendOperandTestInstance(Context &context, const Texture &texture,
                                                               const VkFormat readFormat, const VkFormat writeFormat,
                                                               bool relaxedPrecision)
    : BaseTestInstance(context, texture, readFormat, true, true, false, false)
    , m_imageDstFormat(writeFormat)
    , m_relaxedPrecision(relaxedPrecision)
{
    const DeviceInterface &vk              = m_context.getDeviceInterface();
    const VkDevice device                  = m_context.getDevice();
    Allocator &allocator                   = m_context.getDefaultAllocator();
    const int32_t width                    = texture.size().x();
    const int32_t height                   = texture.size().y();
    const tcu::TextureFormat textureFormat = mapVkFormat(m_format);

    // Generate reference image
    m_isSigned = (getTextureChannelClass(textureFormat.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);
    m_inputImageData.setStorage(textureFormat, width, height, 1);

    const tcu::PixelBufferAccess access = m_inputImageData.getAccess();
    const int valueStart                = (m_isSigned ? (-width / 2) : 0);

    for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
        {
            const tcu::IVec4 color(valueStart + x, valueStart + y, valueStart, valueStart);
            access.setPixel(color, x, y);
        }

    // Create source image
    m_imageSrc = de::MovePtr<Image>(new Image(
        vk, device, allocator,
        makeImageCreateInfo(m_texture, m_format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0u),
        MemoryRequirement::Any));

    // Create destination image
    m_imageDst = de::MovePtr<Image>(
        new Image(vk, device, allocator,
                  makeImageCreateInfo(m_texture, m_imageDstFormat,
                                      VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u),
                  MemoryRequirement::Any));

    // Compute image and buffer sizes
    m_imageSrcSize               = width * height * tcu::getPixelSize(textureFormat);
    m_imageDstSize               = width * height * tcu::getPixelSize(mapVkFormat(m_imageDstFormat));
    VkDeviceSize bufferSizeBytes = de::max(m_imageSrcSize, m_imageDstSize);

    // Create helper buffer able to store input data and image write result
    m_buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    const Allocation &alloc = m_buffer->getAllocation();
    deMemcpy(alloc.getHostPtr(), m_inputImageData.getAccess().getDataPtr(), static_cast<size_t>(m_imageSrcSize));
    flushAlloc(vk, device, alloc);
}

VkDescriptorSetLayout ImageExtendOperandTestInstance::prepareDescriptors(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                .build(vk, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
                           .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

    const VkImageViewType viewType = mapImageViewType(m_texture.type());
    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    m_descriptorSet = makeVkSharedPtr(makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout));
    m_imageSrcView =
        makeVkSharedPtr(makeImageView(vk, device, m_imageSrc->get(), viewType, m_format, subresourceRange));
    m_imageDstView =
        makeVkSharedPtr(makeImageView(vk, device, m_imageDst->get(), viewType, m_imageDstFormat, subresourceRange));

    return *m_descriptorSetLayout; // not passing the ownership
}

void ImageExtendOperandTestInstance::commandBindDescriptorsForLayer(const VkCommandBuffer cmdBuffer,
                                                                    const VkPipelineLayout pipelineLayout,
                                                                    const int layerNdx)
{
    DE_UNREF(layerNdx);

    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkDevice device               = m_context.getDevice();
    const VkDescriptorSet descriptorSet = **m_descriptorSet;

    const VkDescriptorImageInfo descriptorSrcImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, **m_imageSrcView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorImageInfo descriptorDstImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, **m_imageDstView, VK_IMAGE_LAYOUT_GENERAL);

    typedef DescriptorSetUpdateBuilder::Location DSUBL;
    DescriptorSetUpdateBuilder()
        .writeSingle(descriptorSet, DSUBL::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorSrcImageInfo)
        .writeSingle(descriptorSet, DSUBL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorDstImageInfo)
        .update(vk, device);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                             nullptr);
}

void ImageExtendOperandTestInstance::commandBeforeCompute(const VkCommandBuffer cmdBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    const VkImageSubresourceRange fullImageSubresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_texture.numLayers());
    {
        const VkImageMemoryBarrier preCopyImageBarriers[] = {
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_imageSrc->get(), fullImageSubresourceRange),
            makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   m_imageDst->get(), fullImageSubresourceRange)};

        const VkBufferMemoryBarrier barrierFlushHostWriteBeforeCopy = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, m_buffer->get(), 0ull, m_imageSrcSize);

        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &barrierFlushHostWriteBeforeCopy,
                              DE_LENGTH_OF_ARRAY(preCopyImageBarriers), preCopyImageBarriers);
    }
    {
        const VkImageMemoryBarrier barrierAfterCopy = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, m_imageSrc->get(), fullImageSubresourceRange);

        const VkBufferImageCopy copyRegion = makeBufferImageCopy(m_texture);

        vk.cmdCopyBufferToImage(cmdBuffer, m_buffer->get(), m_imageSrc->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                                &copyRegion);
        vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &barrierAfterCopy);
    }
}

void ImageExtendOperandTestInstance::commandBetweenShaderInvocations(const VkCommandBuffer cmdBuffer)
{
    commandImageWriteBarrierBetweenShaderInvocations(m_context, cmdBuffer, m_imageDst->get(), m_texture);
}

void ImageExtendOperandTestInstance::commandAfterCompute(const VkCommandBuffer cmdBuffer)
{
    commandCopyImageToBuffer(m_context, cmdBuffer, m_imageDst->get(), m_buffer->get(), m_imageDstSize, m_texture);
}

// Clears the high bits of every pixel in the pixel buffer, leaving only the lowest 16 bits of each component.
void clearHighBits(const tcu::PixelBufferAccess &pixels, int width, int height)
{
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            auto color = pixels.getPixelUint(x, y);
            for (int c = 0; c < decltype(color)::SIZE; ++c)
                color[c] &= 0xFFFFull;
            pixels.setPixel(color, x, y);
        }
}

tcu::TestStatus ImageExtendOperandTestInstance::verifyResult(void)
{
    const DeviceInterface &vk                = m_context.getDeviceInterface();
    const VkDevice device                    = m_context.getDevice();
    const tcu::IVec3 imageSize               = m_texture.size();
    const tcu::PixelBufferAccess inputAccess = m_inputImageData.getAccess();
    const int32_t width                      = inputAccess.getWidth();
    const int32_t height                     = inputAccess.getHeight();
    tcu::TextureLevel refImage(mapVkFormat(m_imageDstFormat), width, height);
    tcu::PixelBufferAccess refAccess = refImage.getAccess();

    for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
        {
            tcu::IVec4 color = inputAccess.getPixelInt(x, y);
            refAccess.setPixel(color, x, y);
        }

    const Allocation &alloc = m_buffer->getAllocation();
    invalidateAlloc(vk, device, alloc);
    const tcu::PixelBufferAccess result(mapVkFormat(m_imageDstFormat), imageSize, alloc.getHostPtr());

    if (m_relaxedPrecision)
    {
        // Preserve the lowest 16 bits of the reference and result pixels only.
        clearHighBits(refAccess, width, height);
        clearHighBits(result, width, height);
    }

    if (tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Comparison", "Comparison", refAccess, result,
                                 tcu::UVec4(0), tcu::COMPARE_LOG_RESULT, true /*use64Bits*/))
        return tcu::TestStatus::pass("Passed");
    else
        return tcu::TestStatus::fail("Image comparison failed");
}

enum class ExtendTestType
{
    READ = 0,
    WRITE,
    WRITE_NONTEMPORAL,
};

enum class ExtendOperand
{
    SIGN_EXTEND = 0,
    ZERO_EXTEND = 1
};

class ImageExtendOperandTest : public TestCase
{
public:
    ImageExtendOperandTest(tcu::TestContext &testCtx, const std::string &name, const Texture texture,
                           const VkFormat readFormat, const VkFormat writeFormat, const bool signedInt,
                           const bool relaxedPrecision, ExtendTestType extendTestType);

    void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

private:
    bool isWriteTest() const
    {
        return (m_extendTestType == ExtendTestType::WRITE) || (m_extendTestType == ExtendTestType::WRITE_NONTEMPORAL);
    }

    const Texture m_texture;
    VkFormat m_readFormat;
    VkFormat m_writeFormat;
    bool m_operandForce; // Use an operand that doesn't match SampledType?
    bool m_relaxedPrecision;
    ExtendTestType m_extendTestType;
};

ImageExtendOperandTest::ImageExtendOperandTest(tcu::TestContext &testCtx, const std::string &name,
                                               const Texture texture, const VkFormat readFormat,
                                               const VkFormat writeFormat, const bool operandForce,
                                               const bool relaxedPrecision, ExtendTestType extendTestType)
    : TestCase(testCtx, name)
    , m_texture(texture)
    , m_readFormat(readFormat)
    , m_writeFormat(writeFormat)
    , m_operandForce(operandForce)
    , m_relaxedPrecision(relaxedPrecision)
    , m_extendTestType(extendTestType)
{
}

void checkFormatProperties(const Context &context, VkFormat format)
{
#ifndef CTS_USES_VULKANSC
    const VkFormatProperties3 formatProperties(context.getFormatProperties(format));

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");
#else
    const VkFormatProperties formatProperties(
        getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format));

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for storage images");
#endif // CTS_USES_VULKANSC
}

void check64BitSupportIfNeeded(Context &context, VkFormat readFormat, VkFormat writeFormat)
{
    if (is64BitIntegerFormat(readFormat) || is64BitIntegerFormat(writeFormat))
    {
        const auto &features = context.getDeviceFeatures();
        if (!features.shaderInt64)
            TCU_THROW(NotSupportedError, "64-bit integers not supported in shaders");
    }
}

void ImageExtendOperandTest::checkSupport(Context &context) const
{
    if (!context.requireDeviceFunctionality("VK_KHR_spirv_1_4"))
        TCU_THROW(NotSupportedError, "VK_KHR_spirv_1_4 not supported");

#ifndef CTS_USES_VULKANSC
    DE_ASSERT(m_readFormat != VK_FORMAT_A8_UNORM_KHR && m_writeFormat != VK_FORMAT_A8_UNORM_KHR);
    DE_ASSERT(m_readFormat != VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR &&
              m_writeFormat != VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR);

    if ((m_extendTestType == ExtendTestType::WRITE_NONTEMPORAL) && (context.getUsedApiVersion() < VK_API_VERSION_1_3))
        TCU_THROW(NotSupportedError, "Vulkan 1.3 or higher is required for this test to run");
#endif // CTS_USES_VULKANSC

    check64BitSupportIfNeeded(context, m_readFormat, m_writeFormat);

    checkFormatProperties(context, m_readFormat);
    checkFormatProperties(context, m_writeFormat);
}

void ImageExtendOperandTest::initPrograms(SourceCollections &programCollection) const
{
    tcu::StringTemplate shaderTemplate(
        "OpCapability Shader\n"
        "OpCapability StorageImageExtendedFormats\n"

        "${capability}"
        "${extension}"

        "%std450 = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint GLCompute %main \"main\" %id %src_image_ptr %dst_image_ptr\n"
        "OpExecutionMode %main LocalSize 1 1 1\n"

        // decorations
        "OpDecorate %id BuiltIn GlobalInvocationId\n"

        "OpDecorate %src_image_ptr DescriptorSet 0\n"
        "OpDecorate %src_image_ptr Binding 0\n"
        "OpDecorate %src_image_ptr NonWritable\n"

        "${relaxed_precision}"

        "OpDecorate %dst_image_ptr DescriptorSet 0\n"
        "OpDecorate %dst_image_ptr Binding 1\n"
        "OpDecorate %dst_image_ptr NonReadable\n"

        // types
        "%type_void                          = OpTypeVoid\n"
        "%type_i32                           = OpTypeInt 32 1\n"
        "%type_u32                           = OpTypeInt 32 0\n"
        "%type_vec2_i32                      = OpTypeVector %type_i32 2\n"
        "%type_vec2_u32                      = OpTypeVector %type_u32 2\n"
        "%type_vec3_i32                      = OpTypeVector %type_i32 3\n"
        "%type_vec3_u32                      = OpTypeVector %type_u32 3\n"
        "%type_vec4_i32                      = OpTypeVector %type_i32 4\n"
        "%type_vec4_u32                      = OpTypeVector %type_u32 4\n"
        "${extra_types}"

        "%type_fun_void                      = OpTypeFunction %type_void\n"

        "${image_types}"

        "%type_ptr_in_vec3_u32               = OpTypePointer Input %type_vec3_u32\n"
        "%type_ptr_in_u32                    = OpTypePointer Input %type_u32\n"

        "${image_uniforms}"

        // variables
        "%id                                 = OpVariable %type_ptr_in_vec3_u32 Input\n"

        "${image_variables}"

        // main function
        "%main                               = OpFunction %type_void None %type_fun_void\n"
        "%label                              = OpLabel\n"

        "${image_load}"

        "%idvec                              = OpLoad %type_vec3_u32 %id\n"
        "%id_xy                              = OpVectorShuffle %type_vec2_u32 %idvec %idvec 0 1\n"
        "%coord                              = OpBitcast %type_vec2_i32 %id_xy\n"
        "%value                              = OpImageRead ${sampled_type_vec4} %src_image %coord "
        "${read_extend_operand}\n"
        "                                      OpImageWrite %dst_image %coord %value ${write_extend_operand}\n"
        "                                      OpReturn\n"
        "                                      OpFunctionEnd\n");

    const auto testedFormat = mapVkFormat(isWriteTest() ? m_writeFormat : m_readFormat);
    const bool isSigned     = (getTextureChannelClass(testedFormat.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);

    const auto isRead64  = is64BitIntegerFormat(m_readFormat);
    const auto isWrite64 = is64BitIntegerFormat(m_writeFormat);
    DE_ASSERT(isRead64 == isWrite64);

    const bool using64Bits = (isRead64 || isWrite64);

    // Additional capabilities when needed.
    std::string capability;
    std::string extension;
    std::string extraTypes;

    if (using64Bits)
    {
        extension += "OpExtension \"SPV_EXT_shader_image_int64\"\n";
        capability += "OpCapability Int64\n"
                      "OpCapability Int64ImageEXT\n";
        extraTypes += "%type_i64                           = OpTypeInt 64 1\n"
                      "%type_u64                           = OpTypeInt 64 0\n"
                      "%type_vec3_i64                      = OpTypeVector %type_i64 3\n"
                      "%type_vec3_u64                      = OpTypeVector %type_u64 3\n"
                      "%type_vec4_i64                      = OpTypeVector %type_i64 4\n"
                      "%type_vec4_u64                      = OpTypeVector %type_u64 4\n";
    }

    std::string relaxed = "";
    if (m_relaxedPrecision)
        relaxed += "OpDecorate %src_image_ptr RelaxedPrecision\n";

    // Sampled type depends on the format sign and mismatch force flag.
    const bool signedSampleType          = ((isSigned && !m_operandForce) || (!isSigned && m_operandForce));
    const std::string bits               = (using64Bits ? "64" : "32");
    const std::string sampledTypePostfix = (signedSampleType ? "i" : "u") + bits;
    const std::string extendOperandStr   = (isSigned ? "SignExtend" : "ZeroExtend");

    std::map<std::string, std::string> specializations{
        {"image_type_id", "%type_image"},
        {"image_uni_ptr_type_id", "%type_ptr_uniform_const_image"},
        {"image_var_id", "%src_image_ptr"},
        {"image_id", "%src_image"},
        {"capability", capability},
        {"extension", extension},
        {"extra_types", extraTypes},
        {"relaxed_precision", relaxed},
        {"image_format", getSpirvFormat(m_readFormat)},
        {"sampled_type", (std::string("%type_") + sampledTypePostfix)},
        {"sampled_type_vec4", (std::string("%type_vec4_") + sampledTypePostfix)},
        {"read_extend_operand", (!isWriteTest() ? extendOperandStr : "")},
        {"write_extend_operand", (isWriteTest() ? extendOperandStr : "")},
    };

    SpirvVersion spirvVersion = SPIRV_VERSION_1_4;
    bool allowSpirv14         = true;
    if (m_extendTestType == ExtendTestType::WRITE_NONTEMPORAL)
    {
        spirvVersion                            = SPIRV_VERSION_1_6;
        allowSpirv14                            = false;
        specializations["write_extend_operand"] = "Nontemporal";
    }

    // Addidtional parametrization is needed for a case when source and destination textures have same format
    tcu::StringTemplate imageTypeTemplate(
        "${image_type_id}                     = OpTypeImage ${sampled_type} 2D 0 0 0 2 ${image_format}\n");
    tcu::StringTemplate imageUniformTypeTemplate(
        "${image_uni_ptr_type_id}   = OpTypePointer UniformConstant ${image_type_id}\n");
    tcu::StringTemplate imageVariablesTemplate(
        "${image_var_id}                      = OpVariable ${image_uni_ptr_type_id} UniformConstant\n");
    tcu::StringTemplate imageLoadTemplate(
        "${image_id}                          = OpLoad ${image_type_id} ${image_var_id}\n");

    std::string imageTypes;
    std::string imageUniformTypes;
    std::string imageVariables;
    std::string imageLoad;

    // If input image format is the same as output there is less spir-v definitions
    if (m_readFormat == m_writeFormat)
    {
        imageTypes        = imageTypeTemplate.specialize(specializations);
        imageUniformTypes = imageUniformTypeTemplate.specialize(specializations);
        imageVariables    = imageVariablesTemplate.specialize(specializations);
        imageLoad         = imageLoadTemplate.specialize(specializations);

        specializations["image_var_id"] = "%dst_image_ptr";
        specializations["image_id"]     = "%dst_image";
        imageVariables += imageVariablesTemplate.specialize(specializations);
        imageLoad += imageLoadTemplate.specialize(specializations);
    }
    else
    {
        specializations["image_type_id"]         = "%type_src_image";
        specializations["image_uni_ptr_type_id"] = "%type_ptr_uniform_const_src_image";
        imageTypes                               = imageTypeTemplate.specialize(specializations);
        imageUniformTypes                        = imageUniformTypeTemplate.specialize(specializations);
        imageVariables                           = imageVariablesTemplate.specialize(specializations);
        imageLoad                                = imageLoadTemplate.specialize(specializations);

        specializations["image_format"]          = getSpirvFormat(m_writeFormat);
        specializations["image_type_id"]         = "%type_dst_image";
        specializations["image_uni_ptr_type_id"] = "%type_ptr_uniform_const_dst_image";
        specializations["image_var_id"]          = "%dst_image_ptr";
        specializations["image_id"]              = "%dst_image";
        imageTypes += imageTypeTemplate.specialize(specializations);
        imageUniformTypes += imageUniformTypeTemplate.specialize(specializations);
        imageVariables += imageVariablesTemplate.specialize(specializations);
        imageLoad += imageLoadTemplate.specialize(specializations);
    }

    specializations["image_types"]     = imageTypes;
    specializations["image_uniforms"]  = imageUniformTypes;
    specializations["image_variables"] = imageVariables;
    specializations["image_load"]      = imageLoad;

    // Specialize whole shader and add it to program collection
    programCollection.spirvAsmSources.add("comp")
        << shaderTemplate.specialize(specializations)
        << vk::SpirVAsmBuildOptions(programCollection.usedVulkanVersion, spirvVersion, allowSpirv14);
}

TestInstance *ImageExtendOperandTest::createInstance(Context &context) const
{
    return new ImageExtendOperandTestInstance(context, m_texture, m_readFormat, m_writeFormat, m_relaxedPrecision);
}

static const Texture s_textures[] = {
    Texture(IMAGE_TYPE_1D, tcu::IVec3(64, 1, 1), 1),
    Texture(IMAGE_TYPE_1D_ARRAY, tcu::IVec3(64, 1, 1), 8),
    Texture(IMAGE_TYPE_2D, tcu::IVec3(64, 64, 1), 1),
    Texture(IMAGE_TYPE_2D_ARRAY, tcu::IVec3(64, 64, 1), 8),
    Texture(IMAGE_TYPE_3D, tcu::IVec3(64, 64, 8), 1),
    Texture(IMAGE_TYPE_CUBE, tcu::IVec3(64, 64, 1), 6),
    Texture(IMAGE_TYPE_CUBE_ARRAY, tcu::IVec3(64, 64, 1), 2 * 6),
    Texture(IMAGE_TYPE_BUFFER, tcu::IVec3(64, 1, 1), 1),
};

const Texture &getTestTexture(const ImageType imageType)
{
    for (int textureNdx = 0; textureNdx < DE_LENGTH_OF_ARRAY(s_textures); ++textureNdx)
        if (s_textures[textureNdx].type() == imageType)
            return s_textures[textureNdx];

    DE_FATAL("Internal error");
    return s_textures[0];
}

static const VkFormat s_formats[] = {VK_FORMAT_R32G32B32A32_SFLOAT,
                                     VK_FORMAT_R16G16B16A16_SFLOAT,
                                     VK_FORMAT_R32_SFLOAT,

                                     VK_FORMAT_R32G32B32A32_UINT,
                                     VK_FORMAT_R16G16B16A16_UINT,
                                     VK_FORMAT_R8G8B8A8_UINT,
                                     VK_FORMAT_R32_UINT,

                                     VK_FORMAT_R32G32B32A32_SINT,
                                     VK_FORMAT_R16G16B16A16_SINT,
                                     VK_FORMAT_R8G8B8A8_SINT,
                                     VK_FORMAT_R32_SINT,

                                     VK_FORMAT_R8G8B8A8_UNORM,

                                     VK_FORMAT_B8G8R8A8_UNORM,
                                     VK_FORMAT_B8G8R8A8_UINT,

                                     VK_FORMAT_R8G8B8A8_SNORM,

                                     VK_FORMAT_B10G11R11_UFLOAT_PACK32,

                                     VK_FORMAT_R32G32_SFLOAT,
                                     VK_FORMAT_R16G16_SFLOAT,
                                     VK_FORMAT_R16_SFLOAT,

                                     VK_FORMAT_A2B10G10R10_UINT_PACK32,
                                     VK_FORMAT_R32G32_UINT,
                                     VK_FORMAT_R16G16_UINT,
                                     VK_FORMAT_R16_UINT,
                                     VK_FORMAT_R8G8_UINT,
                                     VK_FORMAT_R8_UINT,

                                     VK_FORMAT_R32G32_SINT,
                                     VK_FORMAT_R16G16_SINT,
                                     VK_FORMAT_R16_SINT,
                                     VK_FORMAT_R8G8_SINT,
                                     VK_FORMAT_R8_SINT,

                                     VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                     VK_FORMAT_R16G16B16A16_UNORM,
                                     VK_FORMAT_R16G16B16A16_SNORM,
                                     VK_FORMAT_R16G16_UNORM,
                                     VK_FORMAT_R16_UNORM,
                                     VK_FORMAT_R8G8_UNORM,
                                     VK_FORMAT_R8_UNORM,
#ifndef CTS_USES_VULKANSC
                                     VK_FORMAT_A8_UNORM_KHR,
#endif // CTS_USES_VULKANSC

                                     VK_FORMAT_R16G16_SNORM,
                                     VK_FORMAT_R16_SNORM,
                                     VK_FORMAT_R8G8_SNORM,
                                     VK_FORMAT_R8_SNORM,

                                     VK_FORMAT_R10X6_UNORM_PACK16,
                                     VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
                                     VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,

                                     VK_FORMAT_R4G4_UNORM_PACK8,
                                     VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                                     VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                                     VK_FORMAT_R5G6B5_UNORM_PACK16,
                                     VK_FORMAT_B5G6R5_UNORM_PACK16,
                                     VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                                     VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                                     VK_FORMAT_A1R5G5B5_UNORM_PACK16,
#ifndef CTS_USES_VULKANSC
                                     VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
#endif // CTS_USES_VULKANSC

                                     VK_FORMAT_B8G8R8A8_SNORM,
                                     VK_FORMAT_B8G8R8A8_SINT,
                                     VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                                     VK_FORMAT_A8B8G8R8_SNORM_PACK32,
                                     VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                     VK_FORMAT_A8B8G8R8_SINT_PACK32,
                                     VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                     VK_FORMAT_A2R10G10B10_SNORM_PACK32,
                                     VK_FORMAT_A2R10G10B10_UINT_PACK32,
                                     VK_FORMAT_A2R10G10B10_SINT_PACK32,
                                     VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                                     VK_FORMAT_A2B10G10R10_SINT_PACK32,
                                     VK_FORMAT_R32G32B32_UINT,
                                     VK_FORMAT_R32G32B32_SINT,
                                     VK_FORMAT_R32G32B32_SFLOAT,
                                     VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

                                     VK_FORMAT_R8G8_SRGB,
                                     VK_FORMAT_R8G8B8_SRGB,
                                     VK_FORMAT_B8G8R8_SRGB,
                                     VK_FORMAT_R8G8B8A8_SRGB,
                                     VK_FORMAT_B8G8R8A8_SRGB,
                                     VK_FORMAT_A8B8G8R8_SRGB_PACK32};

static const VkFormat s_formatsThreeComponent[] = {
    VK_FORMAT_R8G8B8_UINT,      VK_FORMAT_R8G8B8_SINT,    VK_FORMAT_R8G8B8_UNORM,    VK_FORMAT_R8G8B8_SNORM,
    VK_FORMAT_R16G16B16_UINT,   VK_FORMAT_R16G16B16_SINT, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16_SNORM,
    VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SINT,  VK_FORMAT_R32G32B32_SFLOAT,
};

static const VkFormat d_formats[] = {VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT};

static const VkImageTiling s_tilings[] = {
    VK_IMAGE_TILING_OPTIMAL,
    VK_IMAGE_TILING_LINEAR,
};

const char *tilingSuffix(VkImageTiling tiling)
{
    switch (tiling)
    {
    case VK_IMAGE_TILING_OPTIMAL:
        return "";
    case VK_IMAGE_TILING_LINEAR:
        return "_linear";
    default:
        return "unknown";
    }
}

bool isAllowedDepthFormat(const Texture &texture, VkFormat format, const VkImageTiling tiling)
{
    if (isDepthStencilFormat(format) && (tiling == VK_IMAGE_TILING_OPTIMAL) &&
        ((texture.type() == IMAGE_TYPE_2D) || (texture.type() == IMAGE_TYPE_2D_ARRAY)))
    {
        return true;
    }
    return false;
}

// m_testType == TC_COMP_COMP
// First compute shader executes that will flip the input image, image0, horizontally in output image, image1.
// First compute shader makes image1 available in the shader domain with device scope barrier.
// Pipeline barrier executes that adds execution dependency: second shader will only execute after the first has completed.
// Then second compute shader executes making image1 visible in the shader domain and copies image1 back to image0 without any change.
// image0 is compared to the reference output that should be flipped horizontally.
//
// m_testType == TC_COMP_DRAW
// First compute shader executes that will flip the input image, image0, horizontally in output image, image1.
// First compute shader makes image1 available in the shader domain with device scope barrier.
// Pipeline barrier executes that adds execution dependency: second shader will only execute after first has completed.
// Then vertex shader executes and it does not change the image.
// Fragment compute shader executes making image1 visible in the shader domain and copies image1 to a color attachment without any change.
// color attachment is compared to the reference output that should be flipped horizontally.
class ImageDeviceScopeAccessTest : public LoadStoreTest
{
public:
    ImageDeviceScopeAccessTest(tcu::TestContext &testCtx, const TestConfigurationType testType, const std::string &name,
                               const Texture &texture, const VkFormat format, const VkImageTiling tiling);
    void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const TestConfigurationType m_testType;
    const uint32_t m_verticesCount;
};

class ImageDeviceScopeAccessTestInstance : public ImageLoadStoreTestInstance
{
public:
    ImageDeviceScopeAccessTestInstance(Context &context, const TestConfigurationType testType, const Texture &texture,
                                       const VkFormat format, const VkImageTiling tiling, const uint32_t verticesCount);

    tcu::TestStatus iterate(void);

private:
    const TestConfigurationType m_testType;
    const VkImageTiling m_tiling;
    const uint32_t m_verticesCount;
};

ImageDeviceScopeAccessTest::ImageDeviceScopeAccessTest(tcu::TestContext &testCtx, const TestConfigurationType testType,
                                                       const std::string &name, const Texture &texture,
                                                       const VkFormat format, const VkImageTiling tiling)
    : LoadStoreTest(testCtx, name, texture, format, format, tiling)
    , m_testType(testType)
    , m_verticesCount(6u)
{
    DE_ASSERT(texture.numLayers() == 1u);
    DE_ASSERT(m_singleLayerBind == false);
    DE_ASSERT(m_restrictImages == false);
    DE_ASSERT(m_minalign == false);
    DE_ASSERT(m_bufferLoadUniform == false);
    DE_ASSERT(m_imageLoadStoreLodAMD == false);
}

void ImageDeviceScopeAccessTest::initPrograms(SourceCollections &sourceCollections) const
{
    makePrograms(sourceCollections, DEVICESCOPE_STORE);
    makePrograms(sourceCollections, DEVICESCOPE_LOAD, (m_testType == TC_COMP_DRAW), m_verticesCount);
}

void ImageDeviceScopeAccessTest::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");

    VkPhysicalDeviceVulkanMemoryModelFeatures vkMemModelFeatures = context.getVulkanMemoryModelFeatures();
    if (!vkMemModelFeatures.vulkanMemoryModel)
        TCU_THROW(NotSupportedError, "vulkanMemoryModel not supported");

    if (!vkMemModelFeatures.vulkanMemoryModelDeviceScope)
        TCU_THROW(NotSupportedError, "vulkanMemoryModelDeviceScope not supported");

    if (context.getUsedApiVersion() < SPIRV_VERSION_1_5)
        TCU_THROW(NotSupportedError,
                  std::string("Vulkan higher than or equal to spirv 1.5 is required for this test to run").c_str());

    if ((m_testType == TC_COMP_DRAW) && context.getTestContext().getCommandLine().isComputeOnly())
        THROW_NOT_SUPPORTED_COMPUTE_ONLY();

    LoadStoreTest::checkSupport(context);

    if (m_testType == TC_COMP_DRAW)
    {
        const auto &vki           = context.getInstanceInterface();
        const auto physicalDevice = context.getPhysicalDevice();

        VkImageFormatProperties vkImageFormatProperties;
        const auto result = vki.getPhysicalDeviceImageFormatProperties(
            physicalDevice, m_format, mapImageType(m_texture.type()), m_tiling,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &vkImageFormatProperties);

        if (result != VK_SUCCESS)
        {
            if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Format unsupported for tiling");
            else
                TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned unexpected error");
        }
    }
}

TestInstance *ImageDeviceScopeAccessTest::createInstance(Context &context) const
{
    return new ImageDeviceScopeAccessTestInstance(context, m_testType, m_texture, m_format, m_tiling, m_verticesCount);
}

ImageDeviceScopeAccessTestInstance::ImageDeviceScopeAccessTestInstance(Context &context,
                                                                       const TestConfigurationType testType,
                                                                       const Texture &texture, const VkFormat format,
                                                                       const VkImageTiling tiling,
                                                                       const uint32_t verticesCount)
    : ImageLoadStoreTestInstance(context, texture, format, format, tiling, true, false, false, false)
    , m_testType(testType)
    , m_tiling(tiling)
    , m_verticesCount(verticesCount)
{
}

tcu::TestStatus ImageDeviceScopeAccessTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();
    const tcu::IVec3 workSize       = m_texture.size();

    Move<VkShaderModule> shaderModuleLoad;
    Move<VkPipeline> pipelineLoad;

    // Variables for draw call
    Move<VkShaderModule> shaderModuleVert;
    Move<VkRenderPass> renderPass;
    de::MovePtr<Image> colorAttachmentImage;
    Move<VkImageView> colorAttachment;
    Move<VkFramebuffer> framebuffer;

    const Unique<VkShaderModule> shaderModuleStore(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp_store")));

    VkShaderStageFlags descriptorStageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    if (m_testType == TC_COMP_DRAW)
        descriptorStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    const VkDescriptorSetLayout descriptorSetLayout = commonPrepareDescriptors(descriptorStageFlags);
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, descriptorSetLayout));
    const Unique<VkPipeline> pipelineStore(makeComputePipeline(vk, device, *pipelineLayout, *shaderModuleStore));

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    // First compute shader that loads reference image and copies to another image
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineStore);

    commandBeforeCompute(*cmdBuffer);

    commandBindDescriptorsForLayer(*cmdBuffer, *pipelineLayout, 0u);

    vk.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), workSize.z());

    // Barrier between shaders/pipelines
    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_texture.numLayers());
    const VkImageMemoryBarrier imageBarrierBetweenShaders =
        makeImageMemoryBarrier(VK_ACCESS_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                               m_imageSrc->get(), subresourceRange);
    const VkPipelineStageFlags dstStageMask =
        (m_testType == TC_COMP_DRAW) ? (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) :
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStageMask, (VkDependencyFlags)0, 0,
                          nullptr, 0, nullptr, 1, &imageBarrierBetweenShaders);

    // Switch to the second shader program

    if (m_testType == TC_COMP_DRAW)
    {
        shaderModuleVert = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
        shaderModuleLoad = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));

        renderPass = makeRenderPass(vk, device, m_format);

        const uint32_t colorAttachmentCount = 1u;

        colorAttachmentImage = de::MovePtr<Image>(new Image(
            vk, device, allocator,
            makeImageCreateInfo(m_texture, m_format,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, m_tiling),
            MemoryRequirement::Any));

        colorAttachment =
            makeImageView(vk, device, colorAttachmentImage->get(), mapImageViewType(m_texture.type()), m_format,
                          makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_texture.numLayers()));

        const VkExtent2D extent = makeExtent2D(m_texture.size().x(), m_texture.size().y());

        const VkFramebufferCreateInfo fbCreateInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType                sType
            nullptr,                                   // const void*                    pNext
            (VkFramebufferCreateFlags)0,               // VkFramebufferCreateFlags flags;
            *renderPass,                               // VkRenderPass                    renderPass
            colorAttachmentCount,                      // uint32_t                        attachmentCount
            &*colorAttachment,                         // const VkImageView*            pAttachments
            extent.width,                              // uint32_t                        width
            extent.height,                             // uint32_t                        height
            1u                                         // uint32_t                        layers
        };
        framebuffer = createFramebuffer(vk, device, &fbCreateInfo);

        const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
            nullptr,                                  // const void*                                 pNext
            (VkPipelineVertexInputStateCreateFlags)0, // VkPipelineVertexInputStateCreateFlags       flags
            0u,      // uint32_t                                    vertexBindingDescriptionCount
            nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
            0u,      // uint32_t                                    vertexAttributeDescriptionCount
            nullptr  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
        };

        const VkViewport viewport = makeViewport(extent.width, extent.height);
        const VkRect2D scissor    = makeRect2D(extent.width, extent.height);

        const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
            nullptr,                                               // const void*                                 pNext
            (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
            1u,        // uint32_t                                    viewportCount
            &viewport, // const VkViewport*                           pViewports
            1u,        // uint32_t                                    scissorCount
            &scissor   // const VkRect2D*                             pScissors
        };
        pipelineLoad =
            vk::makeGraphicsPipeline(vk, device, *pipelineLayout, *shaderModuleVert, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                     VK_NULL_HANDLE, *shaderModuleLoad, *renderPass, 0u, &vertexInputStateCreateInfo,
                                     nullptr, nullptr, &viewportStateCreateInfo);

        const tcu::Vec4 clearColor(0.0f, 1.0f, 0.0f, 1.0f);
        beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, extent.width, extent.height),
                        clearColor);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLoad);

        const uint32_t layerNdx             = 0u;
        const VkDescriptorSet descriptorSet = **m_allDescriptorSets[layerNdx];
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet,
                                 0u, nullptr);

        vk.cmdDraw(*cmdBuffer, m_verticesCount, 1u, 0u, 0u);

        endRenderPass(vk, *cmdBuffer);

        copyImageToBuffer(vk, *cmdBuffer, colorAttachmentImage->get(), m_imageBuffer->get(),
                          tcu::IVec2(extent.width, extent.height));
    }
    else
    {
        shaderModuleLoad = createShaderModule(vk, device, m_context.getBinaryCollection().get("comp_load"));

        pipelineLoad = makeComputePipeline(vk, device, *pipelineLayout, *shaderModuleLoad);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLoad);

        // descriptors already bound to command buffer on first dispatch
        vk.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), workSize.z());

        const VkImageAspectFlags &aspectMask = getImageAspect(m_format);
        commandCopyImageToBuffer(m_context, *cmdBuffer, m_imageSrc->get(), m_imageBuffer->get(), m_imageSizeBytes,
                                 m_texture, aspectMask);
    }

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    return verifyResult();
}
} // namespace

tcu::TestCaseGroup *createImageStoreTests(tcu::TestContext &testCtx)
{
    // Plain imageStore() cases
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "store"));
    // Declare a format layout qualifier for write images
    de::MovePtr<tcu::TestCaseGroup> testGroupWithFormat(new tcu::TestCaseGroup(testCtx, "with_format"));
    de::MovePtr<tcu::TestCaseGroup> testGroupWithoutFormat(new tcu::TestCaseGroup(testCtx, "without_format"));

    for (int textureNdx = 0; textureNdx < DE_LENGTH_OF_ARRAY(s_textures); ++textureNdx)
    {
        const Texture &texture = s_textures[textureNdx];
        de::MovePtr<tcu::TestCaseGroup> groupWithFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, getImageTypeName(texture.type()).c_str()));
        de::MovePtr<tcu::TestCaseGroup> groupWithoutFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, getImageTypeName(texture.type()).c_str()));
        const bool isLayered = (texture.numLayers() > 1);

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_formats); ++formatNdx)
        {
            for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(s_tilings); tilingNdx++)
            {
                const bool hasSpirvFmt = hasSpirvFormat(s_formats[formatNdx]);
                const char *suffix     = tilingSuffix(s_tilings[tilingNdx]);

                if (hasSpirvFmt)
                {
                    groupWithFormatByImageViewType->addChild(
                        new StoreTest(testCtx, getFormatShortString(s_formats[formatNdx]) + suffix, texture,
                                      s_formats[formatNdx], s_tilings[tilingNdx]));
                    // Additional tests where the shader uses constant data for imageStore.
                    groupWithFormatByImageViewType->addChild(new StoreTest(
                        testCtx, getFormatShortString(s_formats[formatNdx]) + "_constant" + suffix, texture,
                        s_formats[formatNdx], s_tilings[tilingNdx],
                        StoreTest::FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER | StoreTest::FLAG_STORE_CONSTANT_VALUE));
                }
                groupWithoutFormatByImageViewType->addChild(
                    new StoreTest(testCtx, getFormatShortString(s_formats[formatNdx]) + suffix, texture,
                                  s_formats[formatNdx], s_tilings[tilingNdx], 0));

                if (isLayered && hasSpirvFmt)
                    groupWithFormatByImageViewType->addChild(new StoreTest(
                        testCtx, getFormatShortString(s_formats[formatNdx]) + "_single_layer" + suffix, texture,
                        s_formats[formatNdx], VK_IMAGE_TILING_OPTIMAL,
                        StoreTest::FLAG_SINGLE_LAYER_BIND | StoreTest::FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER));

                if (texture.type() == IMAGE_TYPE_BUFFER)
                {
                    if (hasSpirvFmt)
                        groupWithFormatByImageViewType->addChild(
                            new StoreTest(testCtx, getFormatShortString(s_formats[formatNdx]) + "_minalign" + suffix,
                                          texture, s_formats[formatNdx], s_tilings[tilingNdx],
                                          StoreTest::FLAG_MINALIGN | StoreTest::FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER));
                    groupWithoutFormatByImageViewType->addChild(
                        new StoreTest(testCtx, getFormatShortString(s_formats[formatNdx]) + "_minalign" + suffix,
                                      texture, s_formats[formatNdx], s_tilings[tilingNdx], StoreTest::FLAG_MINALIGN));
                }
            }
        }

        {
            // Test depth formats with storage image

            // Depth formats for storage images are only allowed with optimal tiling
            const auto testTilingType = VK_IMAGE_TILING_OPTIMAL;

            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(d_formats); ++formatNdx)
            {
                const char *suffix           = tilingSuffix(testTilingType);
                const auto testFormat        = d_formats[formatNdx];
                const auto formatShortString = getFormatShortString(testFormat);

                if (!isAllowedDepthFormat(texture, testFormat, testTilingType))
                    continue;

                groupWithoutFormatByImageViewType->addChild(
                    new StoreTest(testCtx, formatShortString + suffix, texture, testFormat, testTilingType, 0));

#if 0
                /* 'with_format' tests require format declaration in shader.
                   Since depth format has no spirv equivalent, r16 can be used for d16 and r32f can be used for d32_sfloat.
                   The validation layers do not complain and it works on some implementations */

                groupWithFormatByImageViewType->addChild(new StoreTest(testCtx, formatShortString + suffix, texture, testFormat, testTilingType));
                // Additional tests where the shader uses constant data for imageStore.
                groupWithFormatByImageViewType->addChild(new StoreTest(testCtx, formatShortString + "_constant" + suffix, texture, testFormat, testTilingType, StoreTest::FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER | StoreTest::FLAG_STORE_CONSTANT_VALUE));

                if (isLayered)
                    groupWithFormatByImageViewType->addChild(new StoreTest(testCtx, formatShortString + "_single_layer" + suffix,
                                                                            texture, testFormat, testTilingType,
                                                                            StoreTest::FLAG_SINGLE_LAYER_BIND | StoreTest::FLAG_DECLARE_IMAGE_FORMAT_IN_SHADER));
#endif
            }
        }

        testGroupWithFormat->addChild(groupWithFormatByImageViewType.release());
        testGroupWithoutFormat->addChild(groupWithoutFormatByImageViewType.release());
    }

    testGroup->addChild(testGroupWithFormat.release());
    testGroup->addChild(testGroupWithoutFormat.release());

    return testGroup.release();
}

tcu::TestCaseGroup *createImageLoadStoreTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "load_store"));
    de::MovePtr<tcu::TestCaseGroup> testGroupWithFormat(new tcu::TestCaseGroup(testCtx, "with_format"));
    de::MovePtr<tcu::TestCaseGroup> testGroupWithoutFormat(new tcu::TestCaseGroup(testCtx, "without_format"));
    de::MovePtr<tcu::TestCaseGroup> testGroupWithoutAnyFormat(new tcu::TestCaseGroup(testCtx, "without_any_format"));

    for (int textureNdx = 0; textureNdx < DE_LENGTH_OF_ARRAY(s_textures); ++textureNdx)
    {
        const Texture &texture   = s_textures[textureNdx];
        const auto imageTypeName = getImageTypeName(texture.type());
        const bool isLayered     = (texture.numLayers() > 1);

        de::MovePtr<tcu::TestCaseGroup> groupWithFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, imageTypeName.c_str()));
        de::MovePtr<tcu::TestCaseGroup> groupWithoutFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, imageTypeName.c_str()));
        de::MovePtr<tcu::TestCaseGroup> groupWithoutAnyFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, imageTypeName.c_str()));

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_formats); ++formatNdx)
        {
            const auto formatShortString = getFormatShortString(s_formats[formatNdx]);
            const auto hasSpvFormat      = hasSpirvFormat(s_formats[formatNdx]);

            for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(s_tilings); tilingNdx++)
            {
                const char *suffix = tilingSuffix(s_tilings[tilingNdx]);

                if (hasSpvFormat)
                {
                    groupWithFormatByImageViewType->addChild(
                        new LoadStoreTest(testCtx, formatShortString + suffix, texture, s_formats[formatNdx],
                                          s_formats[formatNdx], s_tilings[tilingNdx]));
                    groupWithoutFormatByImageViewType->addChild(new LoadStoreTest(
                        testCtx, formatShortString + suffix, texture, s_formats[formatNdx], s_formats[formatNdx],
                        s_tilings[tilingNdx], LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
                }

                groupWithoutAnyFormatByImageViewType->addChild(
                    new LoadStoreTest(testCtx, formatShortString + suffix, texture, s_formats[formatNdx],
                                      s_formats[formatNdx], s_tilings[tilingNdx], 0u));

                if (isLayered && hasSpvFormat)
                    groupWithFormatByImageViewType->addChild(new LoadStoreTest(
                        testCtx, formatShortString + "_single_layer" + suffix, texture, s_formats[formatNdx],
                        s_formats[formatNdx], s_tilings[tilingNdx],
                        LoadStoreTest::FLAG_SINGLE_LAYER_BIND | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS |
                            LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));

                if (texture.type() == IMAGE_TYPE_BUFFER)
                {
                    if (hasSpvFormat)
                    {
                        groupWithFormatByImageViewType->addChild(new LoadStoreTest(
                            testCtx, formatShortString + "_minalign" + suffix, texture, s_formats[formatNdx],
                            s_formats[formatNdx], s_tilings[tilingNdx],
                            LoadStoreTest::FLAG_MINALIGN | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS |
                                LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
                        groupWithFormatByImageViewType->addChild(new LoadStoreTest(
                            testCtx, formatShortString + "_minalign_uniform" + suffix, texture, s_formats[formatNdx],
                            s_formats[formatNdx], s_tilings[tilingNdx],
                            LoadStoreTest::FLAG_MINALIGN | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS |
                                LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES |
                                LoadStoreTest::FLAG_UNIFORM_TEXEL_BUFFER));
                        groupWithoutFormatByImageViewType->addChild(new LoadStoreTest(
                            testCtx, formatShortString + "_minalign" + suffix, texture, s_formats[formatNdx],
                            s_formats[formatNdx], s_tilings[tilingNdx],
                            LoadStoreTest::FLAG_MINALIGN | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
                        groupWithoutFormatByImageViewType->addChild(
                            new LoadStoreTest(testCtx, formatShortString + "_minalign_uniform" + suffix, texture,
                                              s_formats[formatNdx], s_formats[formatNdx], s_tilings[tilingNdx],
                                              LoadStoreTest::FLAG_MINALIGN | LoadStoreTest::FLAG_UNIFORM_TEXEL_BUFFER |
                                                  LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
                    }
                    groupWithoutAnyFormatByImageViewType->addChild(new LoadStoreTest(
                        testCtx, formatShortString + "_minalign" + suffix, texture, s_formats[formatNdx],
                        s_formats[formatNdx], s_tilings[tilingNdx], LoadStoreTest::FLAG_MINALIGN));
                    groupWithoutAnyFormatByImageViewType->addChild(
                        new LoadStoreTest(testCtx, formatShortString + "_minalign_uniform" + suffix, texture,
                                          s_formats[formatNdx], s_formats[formatNdx], s_tilings[tilingNdx],
                                          LoadStoreTest::FLAG_MINALIGN | LoadStoreTest::FLAG_UNIFORM_TEXEL_BUFFER));
                }
            }
        }

        if (texture.type() == IMAGE_TYPE_BUFFER)
        {
            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_formatsThreeComponent); ++formatNdx)
            {
                const auto formatShortString = getFormatShortString(s_formatsThreeComponent[formatNdx]);

                for (int tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(s_tilings); tilingNdx++)
                {
                    const char *suffix = tilingSuffix(s_tilings[tilingNdx]);

                    groupWithoutFormatByImageViewType->addChild(new LoadStoreTest(
                        testCtx, formatShortString + "_uniform" + suffix, texture, s_formatsThreeComponent[formatNdx],
                        s_formatsThreeComponent[formatNdx], s_tilings[tilingNdx],
                        LoadStoreTest::FLAG_UNIFORM_TEXEL_BUFFER |
                            LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
                    groupWithoutFormatByImageViewType->addChild(new LoadStoreTest(
                        testCtx, formatShortString + "_minalign_uniform" + suffix, texture,
                        s_formatsThreeComponent[formatNdx], s_formatsThreeComponent[formatNdx], s_tilings[tilingNdx],
                        LoadStoreTest::FLAG_MINALIGN | LoadStoreTest::FLAG_UNIFORM_TEXEL_BUFFER |
                            LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
                }
            }
        }

        {
            // Test depth formats with storage image

            // Depth formats for storage images are only allowed with optimal tiling
            const auto testTilingType = VK_IMAGE_TILING_OPTIMAL;

            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(d_formats); ++formatNdx)
            {
                const char *suffix           = tilingSuffix(testTilingType);
                const auto testFormat        = d_formats[formatNdx];
                const auto formatShortString = getFormatShortString(testFormat);

                if (!isAllowedDepthFormat(texture, testFormat, testTilingType))
                    continue;

                groupWithoutAnyFormatByImageViewType->addChild(new LoadStoreTest(
                    testCtx, formatShortString + suffix, texture, testFormat, testFormat, testTilingType, 0u));
#if 0
                /* 'with_format' tests and tests that have FLAG_DECLARE_FORMAT_IN_SHADER_* flags require format declaration in shader.
                   Since depth format has no spirv equivalent, r16 can be used for d16 and r32f can be used for d32_sfloat.
                   The validation layers do not complain and it works on some implementations */

                groupWithFormatByImageViewType->addChild(new LoadStoreTest(testCtx, formatShortString + suffix, texture, testFormat, testFormat, testTilingType));
                groupWithoutFormatByImageViewType->addChild(new LoadStoreTest(testCtx, formatShortString + suffix, texture, testFormat, testFormat, testTilingType, LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));

                if (isLayered)
                    groupWithFormatByImageViewType->addChild(new LoadStoreTest(testCtx, formatShortString + "_single_layer" + suffix,
                                                             texture, s_formats[formatNdx], s_formats[formatNdx], testTilingType,
                                                             LoadStoreTest::FLAG_SINGLE_LAYER_BIND | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
#endif
            }
        }

        testGroupWithFormat->addChild(groupWithFormatByImageViewType.release());
        testGroupWithoutFormat->addChild(groupWithoutFormatByImageViewType.release());
        testGroupWithoutAnyFormat->addChild(groupWithoutAnyFormatByImageViewType.release());
    }

    testGroup->addChild(testGroupWithFormat.release());
    testGroup->addChild(testGroupWithoutFormat.release());
    testGroup->addChild(testGroupWithoutAnyFormat.release());

    return testGroup.release();
}

tcu::TestCaseGroup *createImageLoadStoreLodAMDTests(tcu::TestContext &testCtx)
{
    static const Texture textures[] = {
        Texture(IMAGE_TYPE_1D_ARRAY, tcu::IVec3(64, 1, 1), 8, 1, 6),
        Texture(IMAGE_TYPE_1D, tcu::IVec3(64, 1, 1), 1, 1, 6),
        Texture(IMAGE_TYPE_2D, tcu::IVec3(64, 64, 1), 1, 1, 6),
        Texture(IMAGE_TYPE_2D_ARRAY, tcu::IVec3(64, 64, 1), 8, 1, 6),
        Texture(IMAGE_TYPE_3D, tcu::IVec3(64, 64, 8), 1, 1, 6),
        Texture(IMAGE_TYPE_CUBE, tcu::IVec3(64, 64, 1), 6, 1, 6),
        Texture(IMAGE_TYPE_CUBE_ARRAY, tcu::IVec3(64, 64, 1), 2 * 6, 1, 6),
    };

    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "load_store_lod"));
    de::MovePtr<tcu::TestCaseGroup> testGroupWithFormat(new tcu::TestCaseGroup(testCtx, "with_format"));
    de::MovePtr<tcu::TestCaseGroup> testGroupWithoutFormat(new tcu::TestCaseGroup(testCtx, "without_format"));

    for (int textureNdx = 0; textureNdx < DE_LENGTH_OF_ARRAY(textures); ++textureNdx)
    {
        const Texture &texture = textures[textureNdx];
        de::MovePtr<tcu::TestCaseGroup> groupWithFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, getImageTypeName(texture.type()).c_str()));
        de::MovePtr<tcu::TestCaseGroup> groupWithoutFormatByImageViewType(
            new tcu::TestCaseGroup(testCtx, getImageTypeName(texture.type()).c_str()));
        const bool isLayered = (texture.numLayers() > 1);

        if (texture.type() == IMAGE_TYPE_BUFFER)
            continue;

        for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_formats); ++formatNdx)
        {
            // These tests always require a SPIR-V format for the write image, even if the read
            // image is being used without a format.
            if (!hasSpirvFormat(s_formats[formatNdx]))
                continue;

            groupWithFormatByImageViewType->addChild(
                new LoadStoreTest(testCtx, getFormatShortString(s_formats[formatNdx]), texture, s_formats[formatNdx],
                                  s_formats[formatNdx], VK_IMAGE_TILING_OPTIMAL,
                                  (LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS |
                                   LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES),
                                  true));
            groupWithoutFormatByImageViewType->addChild(
                new LoadStoreTest(testCtx, getFormatShortString(s_formats[formatNdx]), texture, s_formats[formatNdx],
                                  s_formats[formatNdx], VK_IMAGE_TILING_OPTIMAL,
                                  LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES, true));

            if (isLayered)
                groupWithFormatByImageViewType->addChild(new LoadStoreTest(
                    testCtx, getFormatShortString(s_formats[formatNdx]) + "_single_layer", texture,
                    s_formats[formatNdx], s_formats[formatNdx], VK_IMAGE_TILING_OPTIMAL,
                    LoadStoreTest::FLAG_SINGLE_LAYER_BIND | LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS |
                        LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES,
                    true));
        }

        testGroupWithFormat->addChild(groupWithFormatByImageViewType.release());
        testGroupWithoutFormat->addChild(groupWithoutFormatByImageViewType.release());
    }

    testGroup->addChild(testGroupWithFormat.release());
    testGroup->addChild(testGroupWithoutFormat.release());

    return testGroup.release();
}

tcu::TestCaseGroup *createImageFormatReinterpretTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "format_reinterpret"));

    for (int textureNdx = 0; textureNdx < DE_LENGTH_OF_ARRAY(s_textures); ++textureNdx)
    {
        const Texture &texture = s_textures[textureNdx];
        de::MovePtr<tcu::TestCaseGroup> groupByImageViewType(
            new tcu::TestCaseGroup(testCtx, getImageTypeName(texture.type()).c_str()));

        for (int imageFormatNdx = 0; imageFormatNdx < DE_LENGTH_OF_ARRAY(s_formats); ++imageFormatNdx)
            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_formats); ++formatNdx)
            {
                if (!hasSpirvFormat(s_formats[formatNdx]))
                    continue;

                const std::string caseName =
                    getFormatShortString(s_formats[imageFormatNdx]) + "_" + getFormatShortString(s_formats[formatNdx]);
                if (imageFormatNdx != formatNdx &&
                    formatsAreCompatible(s_formats[imageFormatNdx], s_formats[formatNdx]))
                    groupByImageViewType->addChild(new LoadStoreTest(testCtx, caseName, texture, s_formats[formatNdx],
                                                                     s_formats[imageFormatNdx],
                                                                     VK_IMAGE_TILING_OPTIMAL));
            }
        testGroup->addChild(groupByImageViewType.release());
    }

    return testGroup.release();
}

de::MovePtr<TestCase> createImageQualifierRestrictCase(tcu::TestContext &testCtx, const ImageType imageType,
                                                       const std::string &name)
{
    const VkFormat format  = VK_FORMAT_R32G32B32A32_UINT;
    const Texture &texture = getTestTexture(imageType);
    return de::MovePtr<TestCase>(new LoadStoreTest(testCtx, name, texture, format, format, VK_IMAGE_TILING_OPTIMAL,
                                                   LoadStoreTest::FLAG_RESTRICT_IMAGES |
                                                       LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_READS |
                                                       LoadStoreTest::FLAG_DECLARE_FORMAT_IN_SHADER_WRITES));
}

namespace
{

bool relaxedOK(VkFormat format)
{
    tcu::IVec4 bitDepth = tcu::getTextureFormatBitDepth(mapVkFormat(format));
    int maxBitDepth     = deMax32(deMax32(bitDepth[0], bitDepth[1]), deMax32(bitDepth[2], bitDepth[3]));
    return maxBitDepth <= 16;
}

// Get a format used for reading or writing in extension operand tests. These formats allow representing the shader sampled type to
// verify results from read or write operations.
VkFormat getShaderExtensionOperandFormat(bool isSigned, bool is64Bit)
{
    const VkFormat formats[] = {
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R64_UINT,
        VK_FORMAT_R64_SINT,
    };
    return formats[2u * (is64Bit ? 1u : 0u) + (isSigned ? 1u : 0u)];
}

// INT or UINT format?
bool isIntegralFormat(VkFormat format)
{
    return (isIntFormat(format) || isUintFormat(format));
}

// Return the list of formats used for the extension operand tests (SignExten/ZeroExtend).
std::vector<VkFormat> getExtensionOperandFormatList(void)
{
    std::vector<VkFormat> formatList;

    for (auto format : s_formats)
    {
        if (isIntegralFormat(format))
            formatList.push_back(format);
    }

    formatList.push_back(VK_FORMAT_R64_SINT);
    formatList.push_back(VK_FORMAT_R64_UINT);

    return formatList;
}

} // namespace

tcu::TestCaseGroup *createImageExtendOperandsTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr testGroup(new tcu::TestCaseGroup(testCtx, "extend_operands_spirv1p4"));

    const struct
    {
        ExtendTestType testType;
        const char *name;
    } testTypes[] = {
        {ExtendTestType::READ, "read"},
        {ExtendTestType::WRITE, "write"},
    };

    const auto texture    = Texture(IMAGE_TYPE_2D, tcu::IVec3(8, 8, 1), 1);
    const auto formatList = getExtensionOperandFormatList();

    for (const auto format : formatList)
    {
        const auto isInt     = isIntFormat(format);
        const auto isUint    = isUintFormat(format);
        const auto use64Bits = is64BitIntegerFormat(format);

        DE_ASSERT(isInt || isUint);

        GroupPtr formatGroup(new tcu::TestCaseGroup(testCtx, getFormatShortString(format).c_str()));

        for (const auto &testType : testTypes)
        {
            GroupPtr testTypeGroup(new tcu::TestCaseGroup(testCtx, testType.name));

            for (int match = 0; match < 2; ++match)
            {
                const bool mismatched      = (match == 1);
                const char *matchGroupName = (mismatched ? "mismatched_sign" : "matched_sign");

                // SPIR-V does not allow this kind of sampled type override.
                if (mismatched && isUint)
                    continue;

                GroupPtr matchGroup(new tcu::TestCaseGroup(testCtx, matchGroupName));

                for (int prec = 0; prec < 2; prec++)
                {
                    const bool relaxedPrecision = (prec != 0);

                    const char *precisionName = (relaxedPrecision ? "relaxed_precision" : "normal_precision");
                    const auto signedOther    = ((isInt && !mismatched) || (isUint && mismatched));
                    const auto otherFormat    = getShaderExtensionOperandFormat(signedOther, use64Bits);
                    const auto readFormat     = (testType.testType == ExtendTestType::READ ? format : otherFormat);
                    const auto writeFormat    = (testType.testType == ExtendTestType::WRITE ? format : otherFormat);

                    if (relaxedPrecision && !relaxedOK(readFormat))
                        continue;

                    if (!hasSpirvFormat(readFormat) || !hasSpirvFormat(writeFormat))
                        continue;

                    matchGroup->addChild(new ImageExtendOperandTest(testCtx, precisionName, texture, readFormat,
                                                                    writeFormat, mismatched, relaxedPrecision,
                                                                    testType.testType));
                }

                testTypeGroup->addChild(matchGroup.release());
            }

            formatGroup->addChild(testTypeGroup.release());
        }

        testGroup->addChild(formatGroup.release());
    }

    return testGroup.release();
}

tcu::TestCaseGroup *createImageNontemporalOperandTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "nontemporal_operand"));

    const auto texture = Texture(IMAGE_TYPE_2D, tcu::IVec3(8, 8, 1), 1);

    // using just integer formats for tests so that ImageExtendOperandTest could be reused
    const auto formatList = getExtensionOperandFormatList();

    for (const auto format : formatList)
    {
        const std::string caseName = getFormatShortString(format);
        const auto readFormat      = format;
        const auto writeFormat     = getShaderExtensionOperandFormat(isIntFormat(format), is64BitIntegerFormat(format));

        if (!hasSpirvFormat(readFormat) || !hasSpirvFormat(writeFormat))
            continue;

        // note: just testing OpImageWrite as OpImageRead is tested with addComputeImageSamplerTest
        testGroup->addChild(new ImageExtendOperandTest(testCtx, caseName, texture, readFormat, writeFormat, false,
                                                       false, ExtendTestType::WRITE_NONTEMPORAL));
    }

    return testGroup.release();
}

tcu::TestCaseGroup *createImageDeviceScopeAccessTests(tcu::TestContext &testCtx)
{
    const Texture testTextures[] = {
        Texture(IMAGE_TYPE_1D, tcu::IVec3(64, 1, 1), 1),
        Texture(IMAGE_TYPE_2D, tcu::IVec3(64, 64, 1), 1),
        Texture(IMAGE_TYPE_3D, tcu::IVec3(64, 64, 8), 1, 1, 1),
    };

    const struct TestConfiguration
    {
        TestConfigurationType type;
        const std::string typeName;

    } testConfigurations[] = {{TC_COMP_COMP, "comp_comp"}, {TC_COMP_DRAW, "comp_draw"}};

    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "device_scope_access"));

    for (const auto &testConfig : testConfigurations)
    {
        de::MovePtr<tcu::TestCaseGroup> groupTestType(new tcu::TestCaseGroup(testCtx, testConfig.typeName.c_str()));

        for (uint32_t textureNdx = 0; textureNdx < DE_LENGTH_OF_ARRAY(testTextures); ++textureNdx)
        {
            const Texture &texture   = testTextures[textureNdx];
            const auto imageTypeName = getImageTypeName(texture.type());

            if ((testConfig.type == TC_COMP_DRAW) && (testTextures[textureNdx].type() == IMAGE_TYPE_3D))
                continue;

            de::MovePtr<tcu::TestCaseGroup> groupImageType(new tcu::TestCaseGroup(testCtx, imageTypeName.c_str()));

            for (uint32_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_formats); ++formatNdx)
            {
                const auto formatShortString = getFormatShortString(s_formats[formatNdx]);
                const auto hasSpvFormat      = hasSpirvFormat(s_formats[formatNdx]);

                for (uint32_t tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(s_tilings); tilingNdx++)
                {
                    const char *suffix = tilingSuffix(s_tilings[tilingNdx]);

                    if (hasSpvFormat)
                    {
                        groupImageType->addChild(
                            new ImageDeviceScopeAccessTest(testCtx, testConfig.type, formatShortString + suffix,
                                                           texture, s_formats[formatNdx], s_tilings[tilingNdx]));
                    }
                }
            }

            groupTestType->addChild(groupImageType.release());
        }
        testGroup->addChild(groupTestType.release());
    }

    return testGroup.release();
}

} // namespace image
} // namespace vkt
