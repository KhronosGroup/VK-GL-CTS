/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \file vktBindingImageArrayMaintenance11Tests.cpp
 * \brief Tests using arrayed and non-arrayed views with non-arrayed and
          arrayed descriptors, respectively.
 *//*--------------------------------------------------------------------*/

#include "vktBindingImageArrayMaintenance11Tests.hpp"
#include "vktTestCase.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deRandom.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace vkt::BindingModel
{
namespace
{

using namespace vk;

std::string getImageFormatQualifier(VkFormat format)
{
    std::string qualifier;

    // clang-format off
    switch (format)
    {
    case VK_FORMAT_R8_UNORM:            qualifier = "r8";       break;
    case VK_FORMAT_R8_UINT:             qualifier = "r8ui";     break;
    case VK_FORMAT_R8_SINT:             qualifier = "r8i";      break;
    case VK_FORMAT_R8G8_UNORM:          qualifier = "rg8";      break;
    case VK_FORMAT_R8G8_UINT:           qualifier = "rg8ui";    break;
    case VK_FORMAT_R8G8_SINT:           qualifier = "rg8i";     break;
    case VK_FORMAT_R8G8B8A8_UNORM:      qualifier = "rgba8";    break;
    case VK_FORMAT_R8G8B8A8_UINT:       qualifier = "rgba8ui";  break;
    case VK_FORMAT_R8G8B8A8_SINT:       qualifier = "rgba8i";   break;
    case VK_FORMAT_R16_UNORM:           qualifier = "r16";      break;
    case VK_FORMAT_R16_UINT:            qualifier = "r16ui";    break;
    case VK_FORMAT_R16_SINT:            qualifier = "r16i";     break;
    case VK_FORMAT_R16G16_UNORM:        qualifier = "rg16";     break;
    case VK_FORMAT_R16G16_UINT:         qualifier = "rg16ui";   break;
    case VK_FORMAT_R16G16_SINT:         qualifier = "rg16i";    break;
    case VK_FORMAT_R16G16B16A16_UNORM:  qualifier = "rgba16";   break;
    case VK_FORMAT_R16G16B16A16_UINT:   qualifier = "rgba16ui"; break;
    case VK_FORMAT_R16G16B16A16_SINT:   qualifier = "rgba16i";  break;
    case VK_FORMAT_R32_SFLOAT:          qualifier = "r32f";     break;
    case VK_FORMAT_R32_UINT:            qualifier = "r32ui";    break;
    case VK_FORMAT_R32_SINT:            qualifier = "r32i";     break;
    case VK_FORMAT_R32G32_SFLOAT:       qualifier = "rg32f";    break;
    case VK_FORMAT_R32G32_UINT:         qualifier = "rg32ui";   break;
    case VK_FORMAT_R32G32_SINT:         qualifier = "rg32i";    break;
    case VK_FORMAT_R32G32B32A32_SFLOAT: qualifier = "rgba32f";  break;
    case VK_FORMAT_R32G32B32A32_UINT:   qualifier = "rgba32ui"; break;
    case VK_FORMAT_R32G32B32A32_SINT:   qualifier = "rgba32i";  break;

    default:
        DE_ASSERT(false);
        break;
    }
    // clang-format on

    return qualifier;
}

struct TestParams
{
    VkFormat imageFormat;
    VkImageType imageType;           // 1D or 2D.
    VkImageViewType viewType;        // 1D or 2D or array of them.
    VkDescriptorType descriptorType; // Sampled or storage image.
    uint32_t layer;                  // Layer used to create the view.
    int lod;                         // Lod level (only for sampled images).
    bool store;                      // Store data in the image instead of reading from it (for storage images).

    uint32_t getSeed() const
    {
        //
        // MSB | Byte 3: format (8 bits: bits 24-31)
        //     | Byte 2: lod (7 bits: 17-23) store (bit 16)
        //     | Byte 1: image type (2 bits: 14-15) view type (3 bits: 11-13) desc type (4 bits: 8-11)
        // LSB | Byte 0: <empty>
        return (((static_cast<uint32_t>(imageFormat) & 0xFFu) << 24) | ((static_cast<uint32_t>(lod) & 0x3Fu) << 17) |
                ((static_cast<uint32_t>(store) & 0x1u) << 16) | ((static_cast<uint32_t>(imageType) & 0x3u) << 14) |
                ((static_cast<uint32_t>(viewType) & 0x7u) << 11) |
                ((static_cast<uint32_t>(descriptorType) & 0xFu) << 8));
    }

    VkImageTiling getTiling() const
    {
        return VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageUsageFlags getImageUsage() const
    {
        VkImageUsageFlags usage = 0u;

        if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        else
            DE_ASSERT(false);

        if (store)
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        else
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        return usage;
    }

    VkExtent3D getMipLevelExtent() const
    {
        VkExtent3D extent{0u, 0u, 0u};

        if (imageType == VK_IMAGE_TYPE_1D)
            extent = VkExtent3D{4096u, 1u, 1u};
        else if (imageType == VK_IMAGE_TYPE_2D)
            extent = VkExtent3D{64u, 64u, 1u};
        else
            DE_ASSERT(false);

        return extent;
    }

    uint32_t getMipLevelCount() const
    {
        return static_cast<uint32_t>(lod + 1);
    }

    VkExtent3D getCreationExtent() const
    {
        const uint32_t multiplier = (1u << lod);
        auto extent               = getMipLevelExtent();
        extent.width *= multiplier;
        if (imageType == VK_IMAGE_TYPE_2D)
            extent.height *= multiplier;
        return extent;
    }

    uint32_t getLayerCount() const
    {
        return layer + 1u;
    }

    uint32_t getWorkGroupSize() const
    {
        return 64u;
    }

    VkImageLayout getImageLayoutShader() const
    {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            layout = VK_IMAGE_LAYOUT_GENERAL;
        else
            DE_ASSERT(false);

        return layout;
    }
};

class ImageArrayInstance : public vkt::TestInstance
{
public:
    ImageArrayInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~ImageArrayInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class ImageArrayCase : public vkt::TestCase
{
public:
    ImageArrayCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~ImageArrayCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new ImageArrayInstance(context, m_params);
    }

protected:
    const TestParams m_params;
};

void ImageArrayCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_maintenance11");

    const auto ctx    = context.getContextCommonData();
    const auto tiling = m_params.getTiling();
    const auto usage  = m_params.getImageUsage();

    VkImageFormatProperties formatProperties;
    const auto result = ctx.vki.getPhysicalDeviceImageFormatProperties(
        ctx.physicalDevice, m_params.imageFormat, m_params.imageType, tiling, usage, 0u, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format not supported for requested usage");
    else if (result != VK_SUCCESS)
        TCU_FAIL(std::string("vkGetPhysicalDeviceImageFormatProperties returned ") + getResultName(result));

    const auto extent     = m_params.getCreationExtent();
    const auto &maxExtent = formatProperties.maxExtent;

    if (extent.width > maxExtent.width || extent.height > maxExtent.height || extent.depth > maxExtent.depth)
    {
        std::ostringstream msg;
        msg << "Extent limits (" << maxExtent.width << ", " << maxExtent.height << ", " << maxExtent.depth
            << " too small for target extent (" << extent.width << ", " << extent.height << ", " << extent.depth << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }

    const auto layerCount = m_params.getLayerCount();
    if (layerCount > formatProperties.maxArrayLayers)
    {
        std::ostringstream msg;
        msg << "maxArrayLayers (" << formatProperties.maxArrayLayers << ") too small for target layers (" << layerCount
            << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }
}

void ImageArrayCase::initPrograms(vk::SourceCollections &programCollection) const
{
    DE_ASSERT(m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
              m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    if (m_params.store)
        DE_ASSERT(m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    std::string descTypePrefix;
    if (isIntFormat(m_params.imageFormat))
        descTypePrefix = "i";
    else if (isUintFormat(m_params.imageFormat))
        descTypePrefix = "u";

    bool arrayInShader = false;
    if (m_params.viewType == VK_IMAGE_VIEW_TYPE_1D || m_params.viewType == VK_IMAGE_VIEW_TYPE_2D)
        arrayInShader = true; // We make the descriptor type mismatch the image view type.

    std::string descTypeSuffix;
    if (arrayInShader)
        descTypeSuffix = "Array";

    std::string descDim;
    if (m_params.imageType == VK_IMAGE_TYPE_1D)
        descDim = "1D";
    else if (m_params.imageType == VK_IMAGE_TYPE_2D)
        descDim = "2D";
    else
        DE_ASSERT(false);

    std::string descType;
    if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        descType = "sampler";
    else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        descType = "image";
    else
        DE_ASSERT(false);

    std::string formatQualifier;
    if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        formatQualifier = ", " + getImageFormatQualifier(m_params.imageFormat);

    const auto wgSize = m_params.getWorkGroupSize();

    std::string coordsType;
    std::string coords;
    std::string normCoordsType;

    if (m_params.imageType == VK_IMAGE_TYPE_1D)
    {
        coordsType     = (arrayInShader ? "ivec2" : "int");
        normCoordsType = (arrayInShader ? "vec2" : "float");
        coords         = (arrayInShader ? "ivec2(invCol, 0)" : "int(invCol)");
    }
    else if (m_params.imageType == VK_IMAGE_TYPE_2D)
    {
        coordsType     = (arrayInShader ? "ivec3" : "ivec2");
        normCoordsType = (arrayInShader ? "vec3" : "vec2");
        coords         = (arrayInShader ? "ivec3(invCol, invRow, 0)" : "ivec2(invCol, invRow)");
    }
    else
        DE_ASSERT(false);
    const std::string normCoords =
        "((" + normCoordsType + "(coords) + " + normCoordsType + "(0.5)) / " + normCoordsType + "(imageSize))";

    std::string work = "        ";
    if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
    {
        if (m_params.store)
            work += "imageStore(img, coords, ssbo.values[globalInvocationIndex]);\n";
        else
            work += "ssbo.values[globalInvocationIndex] = imageLoad(img, coords);\n";
    }
    else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    {
        work +=
            "ssbo.values[globalInvocationIndex] = textureLod(img, normCoords, " + std::to_string(m_params.lod) + ");\n";
    }
    else
        DE_ASSERT(false);

    // Get the image size from the descriptor itself.
    std::string imageSizeExpr          = "    const uvec3 imageSize = uvec3(";
    const auto wantedSizeFuncCompCount = 3u; // We want 3 components for the uvec3 above.
    uint32_t sizeFuncCompCount         = 0u; // How many components the size function will return.
    if (m_params.imageType == VK_IMAGE_TYPE_1D)
        sizeFuncCompCount = 1u;
    else if (m_params.imageType == VK_IMAGE_TYPE_2D)
        sizeFuncCompCount = 2u;
    else
        DE_ASSERT(false);
    if (arrayInShader)
        ++sizeFuncCompCount;

    // We will fill the missing components manually.
    const auto missingSizeFuncCompCount = wantedSizeFuncCompCount - sizeFuncCompCount;
    std::string missingSizeFuncCompValues;
    for (uint32_t i = 0u; i < missingSizeFuncCompCount; ++i)
        missingSizeFuncCompValues += ", 1";

    // We want the result as uints, so we will cast the size function result to uint/uvecN.
    const auto imageSizeCastType = (sizeFuncCompCount == 1 ? "uint" : "uvec" + std::to_string(sizeFuncCompCount));

    // Get the result as a uint3 by calling the image query and adding the missing components manually.
    if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        imageSizeExpr += imageSizeCastType + "(imageSize(img))" + missingSizeFuncCompValues + ");\n";
    else if (m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        imageSizeExpr += imageSizeCastType + "(textureSize(img, " + std::to_string(m_params.lod) + "))" +
                         missingSizeFuncCompValues + ");\n";
    else
        DE_ASSERT(false);

    const auto mipExtent = m_params.getMipLevelExtent();

    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << wgSize << ", local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0" << formatQualifier << ") uniform " << descTypePrefix << descType << descDim
         << descTypeSuffix << " img;\n"
         << "layout (set=0, binding=1) buffer BufferBlock { " << descTypePrefix << "vec4 values[]; } ssbo;\n"
         << "void main (void) {\n"
         << imageSizeExpr << "    const bool sizeOK = (imageSize == uvec3(" << mipExtent.width << ", "
         << mipExtent.height << ", " << mipExtent.depth << "));\n"
         << "    const uint pixelCount = imageSize.x * imageSize.y;\n"
         << "    const uint globalInvocationIndex = gl_WorkGroupID.x * gl_WorkGroupSize.x + gl_LocalInvocationIndex;\n"
         << "    if (sizeOK && globalInvocationIndex < pixelCount) {\n"
         << "        const uint invRow = globalInvocationIndex / imageSize.x;\n"
         << "        const uint invCol = globalInvocationIndex % imageSize.x;\n"
         << "        const " << coordsType << " coords = " << coords << ";\n"
         << "        const " << normCoordsType << " normCoords = " << normCoords << ";\n"
         << work << "    }\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

// Generates int values for color channels, based on minimum and maximum values.
tcu::IVec4 genIntValues(de::Random &rng, const tcu::IVec4 &bitDepth, const tcu::IVec4 &minIntValues,
                        const tcu::IVec4 &maxIntValues)
{
    return tcu::IVec4(((bitDepth.x() > 0) ? rng.getInt(minIntValues.x(), maxIntValues.x()) : 0),
                      ((bitDepth.y() > 0) ? rng.getInt(minIntValues.y(), maxIntValues.y()) : 0),
                      ((bitDepth.z() > 0) ? rng.getInt(minIntValues.z(), maxIntValues.z()) : 0),
                      ((bitDepth.w() > 0) ? rng.getInt(minIntValues.w(), maxIntValues.w()) : 1));
}

// Generates uint values for color channels, based on maximum allowed values.
tcu::UVec4 genUintValues(de::Random &rng, const tcu::IVec4 &bitDepth, const tcu::UVec4 &maxUintValues)
{
    return tcu::UVec4(((bitDepth.x() > 0) ? (rng.getUint32() & maxUintValues.x()) : 0u),
                      ((bitDepth.y() > 0) ? (rng.getUint32() & maxUintValues.y()) : 0u),
                      ((bitDepth.z() > 0) ? (rng.getUint32() & maxUintValues.z()) : 0u),
                      ((bitDepth.w() > 0) ? (rng.getUint32() & maxUintValues.w()) : 1u));
}

// Generates float values for color channels, in the [0,1] range.
tcu::Vec4 genFloatValues(de::Random &rng, const tcu::IVec4 &bitDepth)
{
    return tcu::Vec4(((bitDepth.x() > 0) ? (rng.getFloat()) : 0.0f), ((bitDepth.y() > 0) ? (rng.getFloat()) : 0.0f),
                     ((bitDepth.z() > 0) ? (rng.getFloat()) : 0.0f), ((bitDepth.w() > 0) ? (rng.getFloat()) : 1.0f));
}

tcu::TestStatus ImageArrayInstance::iterate(void)
{
    const auto ctx       = m_context.getContextCommonData();
    const auto tcuFormat = mapVkFormat(m_params.imageFormat);
    const auto aspects   = getImageAspectFlags(tcuFormat);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        VK_IMAGE_CREATE_ALIAS_SINGLE_LAYER_DESCRIPTOR_BIT_KHR,
        m_params.imageType,
        m_params.imageFormat,
        m_params.getCreationExtent(),
        m_params.getMipLevelCount(),
        m_params.getLayerCount(),
        VK_SAMPLE_COUNT_1_BIT,
        m_params.getTiling(),
        m_params.getImageUsage(),
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory image(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
    const auto imageSRR    = makeImageSubresourceRange(aspects, 0u, m_params.getMipLevelCount(), m_params.layer, 1u);
    const auto mipLayerSRL = makeImageSubresourceLayers(aspects, m_params.lod, m_params.layer, 1u);
    const auto imageView =
        makeImageView(ctx.vkd, ctx.device, *image, m_params.viewType, m_params.imageFormat, imageSRR);

    const bool useInts   = isIntFormat(m_params.imageFormat);
    const bool useUints  = isUintFormat(m_params.imageFormat);
    const bool useFloats = (!useInts && !useUints);

    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        static_cast<float>(m_params.lod),
        (useFloats ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_BLACK),
        VK_FALSE,
    };
    const auto useSampler = (m_params.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    const auto sampler    = (useSampler ? createSampler(ctx.vkd, ctx.device, &samplerCreateInfo) : Move<VkSampler>());

    const auto mipExtent  = m_params.getMipLevelExtent();
    const auto uExtent    = tcu::UVec3(mipExtent.width, mipExtent.height, mipExtent.depth);
    const auto iExtent    = uExtent.asInt();
    const auto pixelCount = mipExtent.width * mipExtent.height * mipExtent.depth;

    const auto vecSize       = DE_SIZEOF32(tcu::Vec4);
    const auto vecBufferSize = pixelCount * vecSize;

    const auto pixelSize       = static_cast<uint32_t>(tcu::getPixelSize(tcuFormat));
    const auto pixelBufferSize = pixelCount * pixelSize;

    // We need 2 buffers: a buffer with "expanded" values, i.e. in vec form as used by the shaders, and a buffer
    // containing pixel values in "image" form, i.e. the type that you would use with copyImageToBuffer or
    // copyBufferToImage.
    //
    // If we're testing reads from the image, we need the pixel buffer as input, to run copyBufferToImage, and the
    // expanded buffer as output, to see what we got out.
    //
    // If we're testing writes to the image, we need the expanded buffer as input in the shader and the pixel buffer as
    // the output, where we will copy results.
    //
    // Verification of both buffers is not identical, of course.

    // Expanded or "vec" buffer. This is the one used in the shader, so it's a storage buffer.
    const auto vecBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto vecBufferCreateInfo = makeBufferCreateInfo(vecBufferSize, vecBufferUsage);
    BufferWithMemory vecBuffer(ctx.vkd, ctx.device, ctx.allocator, vecBufferCreateInfo,
                               (m_params.store ? HostIntent::W : HostIntent::R));

    // Pixel buffer, potentially smaller, used to copy image data in or out.
    const auto pxBufferUsage = (m_params.store ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const auto pxBufferCreateInfo = makeBufferCreateInfo(pixelBufferSize, pxBufferUsage);
    BufferWithMemory pxBuffer(ctx.vkd, ctx.device, ctx.allocator, pxBufferCreateInfo,
                              (m_params.store ? HostIntent::R : HostIntent::W));

    // Fill source buffer(s).
    const auto seed = m_params.getSeed();

    // Bit depths and value ranges.
    const auto bitDepth      = tcu::getTextureFormatBitDepth(tcuFormat);
    const auto minIntValues  = (useInts ? tcu::getFormatMinIntValue(tcuFormat) : tcu::IVec4(0));
    const auto maxIntValues  = (useInts ? tcu::getFormatMaxIntValue(tcuFormat) : tcu::IVec4(0));
    const auto maxUintValues = (useUints ? tcu::getFormatMaxUintValue(tcuFormat) : tcu::UVec4(0));

    de::Random rnd(seed);

    std::vector<uint8_t> vecBytes(static_cast<size_t>(vecBufferSize), uint8_t{0});
    tcu::TextureLevel pxLevel(tcuFormat, iExtent.x(), iExtent.y(), iExtent.z());
    tcu::PixelBufferAccess pxAccess = pxLevel.getAccess();

    if (m_params.store)
    {
        for (uint32_t i = 0u; i < pixelCount; ++i)
        {
            const auto byteIdx = i * vecSize;
            const auto bytePtr = vecBytes.data() + byteIdx;

            if (useInts)
            {
                const auto values = genIntValues(rnd, bitDepth, minIntValues, maxIntValues);
                memcpy(bytePtr, &values, sizeof(values));
            }
            else if (useUints)
            {
                const auto values = genUintValues(rnd, bitDepth, maxUintValues);
                memcpy(bytePtr, &values, sizeof(values));
            }
            else
            {
                const auto values = genFloatValues(rnd, bitDepth);
                memcpy(bytePtr, &values, sizeof(values));
            }
        }

        auto &vecAlloc = vecBuffer.getAllocation();
        memcpy(vecAlloc.getHostPtr(), de::dataOrNull(vecBytes), de::dataSize(vecBytes));
        flushAlloc(ctx.vkd, ctx.device, vecAlloc);
    }
    else
    {
        for (int z = 0; z < iExtent.z(); ++z)
            for (int y = 0; y < iExtent.y(); ++y)
                for (int x = 0; x < iExtent.x(); ++x)
                {
                    if (useInts)
                    {
                        const auto values = genIntValues(rnd, bitDepth, minIntValues, maxIntValues);
                        pxAccess.setPixel(values, x, y, z);
                    }
                    else if (useUints)
                    {
                        const auto values = genUintValues(rnd, bitDepth, maxUintValues);
                        pxAccess.setPixel(values, x, y, z);
                    }
                    else
                    {
                        const auto values = genFloatValues(rnd, bitDepth);
                        pxAccess.setPixel(values, x, y, z);
                    }
                }

        auto &pxAlloc = pxBuffer.getAllocation();
        memcpy(pxAlloc.getHostPtr(), pxAccess.getDataPtr(), pixelBufferSize);
        flushAlloc(ctx.vkd, ctx.device, pxAlloc);
    }

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(m_params.descriptorType);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    const auto shaderStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(m_params.descriptorType, shaderStages);
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStages);
    const auto setLayout     = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto binding = DescriptorSetUpdateBuilder::Location::binding;

    const auto imgLayout = m_params.getImageLayoutShader();
    const auto imgDesc   = makeDescriptorImageInfo(*sampler, *imageView, imgLayout);
    setUpdateBuilder.writeSingle(*descriptorSet, binding(0u), m_params.descriptorType, &imgDesc);

    const auto bufferDesc = makeDescriptorBufferInfo(*vecBuffer, 0ull, VK_WHOLE_SIZE);
    setUpdateBuilder.writeSingle(*descriptorSet, binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDesc);

    setUpdateBuilder.update(ctx.vkd, ctx.device);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto pipeline   = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compShader);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (m_params.store)
    {
        // The vector buffer already has the right contents, ready to be copied to the image.
        const auto srcAccess = 0u;
        const auto dstAccess = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT);
        const auto srcStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        const auto dstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        const auto barrier =
            makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED, imgLayout, *image, imageSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, &barrier);
    }
    else
    {
        // Copy the pixel buffer to the first layer of the image's mip level so it's read in the shader.
        const auto preCopySrcAccess = 0u;
        const auto preCopyDstAccess = static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT);
        const auto preCopySrcStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        const auto preCopyDstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto preCopyBarrier =
            makeImageMemoryBarrier(preCopySrcAccess, preCopyDstAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *image, imageSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, preCopySrcStages, preCopyDstStages, &preCopyBarrier);

        const auto region = makeBufferImageCopy(mipExtent, mipLayerSRL);
        ctx.vkd.cmdCopyBufferToImage(cmdBuffer, *pxBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

        const auto postCopySrcAccess = static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT);
        const auto postCopyDstAccess = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_READ_BIT);
        const auto postCopySrcStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto postCopyDstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        const auto postCopyBarrier   = makeImageMemoryBarrier(
            postCopySrcAccess, postCopyDstAccess, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imgLayout, *image, imageSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, postCopySrcStages, postCopyDstStages, &postCopyBarrier);
    }
    {
        const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        const auto wgCount = (pixelCount + m_params.getWorkGroupSize() - 1u) / m_params.getWorkGroupSize();
        ctx.vkd.cmdDispatch(cmdBuffer, wgCount, 1u, 1u);
    }
    if (m_params.store)
    {
        // Copy first layer of the image's mip level to the pixel buffer, for verification.
        const auto preCopySrcAccess = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT);
        const auto preCopyDstAccess = static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_READ_BIT);
        const auto preCopySrcStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        const auto preCopyDstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto preCopyBarrier   = makeImageMemoryBarrier(preCopySrcAccess, preCopyDstAccess, imgLayout,
                                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *image, imageSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, preCopySrcStages, preCopyDstStages, &preCopyBarrier);

        const auto region = makeBufferImageCopy(mipExtent, mipLayerSRL);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *pxBuffer, 1u, &region);
    }
    {
        // Reads from any buffer to the host.
        const auto srcAccess = static_cast<VkAccessFlags>(VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const auto dstAccess = static_cast<VkAccessFlags>(VK_ACCESS_HOST_READ_BIT);
        const auto srcStages =
            static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT);
        const auto dstStages      = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_HOST_BIT);
        const auto preHostBarrier = makeMemoryBarrier(srcAccess, dstAccess);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    if (m_params.store)
    {
        // Verify contents of the pixel buffer.
        auto &alloc = pxBuffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        memcpy(pxAccess.getDataPtr(), alloc.getHostPtr(), pixelBufferSize);
    }
    else
    {
        // Verify contents of the vec buffer.
        auto &alloc = vecBuffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        memcpy(de::dataOrNull(vecBytes), alloc.getHostPtr(), de::dataSize(vecBytes));
    }

    // Make sure both buffers match. Create views into the vec buffer as a texture.
    const tcu::TextureFormat iVecFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::ChannelType::SIGNED_INT32);
    const tcu::TextureFormat uVecFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::ChannelType::UNSIGNED_INT32);
    const tcu::TextureFormat fVecFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::ChannelType::FLOAT);

    tcu::ConstPixelBufferAccess iVecAccess(iVecFormat, iExtent, vecBuffer.getAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess uVecAccess(uVecFormat, iExtent, vecBuffer.getAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess fVecAccess(fVecFormat, iExtent, vecBuffer.getAllocation().getHostPtr());

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    // For integers, values should be stored without errors.
    const auto integerThreshold = tcu::UVec4(0u, 0u, 0u, 0u);

    // For floats, when loading we get exact results normally because the value comes from the pixel buffer and is
    // stored in a precise vec buffer. However, when storing we depend on the image format precision, so we use the
    // lowest precision just in case.
    const auto unormThreshold = tcu::Vec4(bitDepth[0] > 0 ? 1.0f / ((float)(1 << bitDepth[0]) - 1.0f) : 0.0f,
                                          bitDepth[1] > 0 ? 1.0f / ((float)(1 << bitDepth[1]) - 1.0f) : 0.0f,
                                          bitDepth[2] > 0 ? 1.0f / ((float)(1 << bitDepth[2]) - 1.0f) : 0.0f,
                                          bitDepth[3] > 0 ? 1.0f / ((float)(1 << bitDepth[3]) - 1.0f) : 0.0f);

    // 0.0000002 has proven to be a good value for other tests and is way less than a single 16-bit unit.
    const auto sfloatThreshold = tcu::Vec4(bitDepth[0] > 0 ? 0.0000002f : 0.0f, bitDepth[1] > 0 ? 0.0000002f : 0.0f,
                                           bitDepth[2] > 0 ? 0.0000002f : 0.0f, bitDepth[3] > 0 ? 0.0000002f : 0.0f);

    const auto channelClass = tcu::getTextureChannelClass(tcuFormat.type);
    const auto floatThreshold =
        (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ? sfloatThreshold : unormThreshold);

    if (useInts)
    {
        const auto &ref = (m_params.store ? iVecAccess : pxAccess);
        const auto &res = (m_params.store ? pxAccess : iVecAccess);
        if (!tcu::intThresholdCompare(log, "Ints", "", ref, res, integerThreshold, tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }
    else if (useUints)
    {
        const auto &ref = (m_params.store ? uVecAccess : pxAccess);
        const auto &res = (m_params.store ? pxAccess : uVecAccess);
        if (!tcu::intThresholdCompare(log, "Uints", "", ref, res, integerThreshold, tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }
    else if (useFloats)
    {
        const auto &ref = (m_params.store ? fVecAccess : pxAccess);
        const auto &res = (m_params.store ? pxAccess : fVecAccess);
        if (!tcu::floatThresholdCompare(log, "Floats", "", ref, res, floatThreshold, tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }
    else
        DE_ASSERT(false);

    if (fail)
        TCU_FAIL("Unexpected results in output buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // namespace

tcu::TestCaseGroup *createImageArrayMaintenance11Tests(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "image_array_m11"));

    const std::vector<VkFormat> formatList{
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8_UINT,
        VK_FORMAT_R8G8_SINT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_FORMAT_R8G8B8A8_SINT,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16_UINT,
        VK_FORMAT_R16G16_SINT,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32_SINT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32A32_SINT,
    };

    const std::vector<VkImageType> imageTypes{
        VK_IMAGE_TYPE_1D,
        VK_IMAGE_TYPE_2D,
    };

    const std::vector<VkImageViewType> imageViewTypes{
        VK_IMAGE_VIEW_TYPE_1D,
        VK_IMAGE_VIEW_TYPE_1D_ARRAY,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    };

    const std::vector<VkDescriptorType> descriptorTypes{
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    };

    for (const auto &format : formatList)
    {
        const auto formatGroupName = getFormatSimpleName(format);
        GroupPtr formatGroup(new tcu::TestCaseGroup(testCtx, formatGroupName.c_str()));

        for (const auto &imageType : imageTypes)
            for (const auto &imageViewType : imageViewTypes)
            {
                if (imageType == VK_IMAGE_TYPE_2D)
                {
                    if (imageViewType != VK_IMAGE_VIEW_TYPE_2D && imageViewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY)
                        continue;
                }
                else if (imageType == VK_IMAGE_TYPE_1D)
                {
                    if (imageViewType != VK_IMAGE_VIEW_TYPE_1D && imageViewType != VK_IMAGE_VIEW_TYPE_1D_ARRAY)
                        continue;
                }
                else
                    DE_ASSERT(false);

                static size_t viewPrefixLen = strlen("VK_IMAGE_");
                const auto viewTypeGroupName =
                    de::toLower(std::string(getImageViewTypeName(imageViewType)).substr(viewPrefixLen));
                GroupPtr viewTypeGroup(new tcu::TestCaseGroup(testCtx, viewTypeGroupName.c_str()));

                for (const auto &descriptorType : descriptorTypes)
                    for (const auto layer : {0u, 1u})
                    {
                        for (const auto lod : {0, 1, 2})
                        {
                            if (lod != 0u && descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                continue;

                            for (const bool store : {false, true})
                            {
                                if (store && descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                    continue;

                                const TestParams params{
                                    format, imageType, imageViewType, descriptorType, layer, lod, store,
                                };

                                std::string testName;
                                if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                    testName = "storage_img_" + std::to_string(layer) +
                                               std::string(store ? "_store" : "_load");
                                else if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                    testName = "sampler_lod_" + std::to_string(layer) + std::to_string(lod);
                                else
                                    DE_ASSERT(false);

                                viewTypeGroup->addChild(new ImageArrayCase(testCtx, testName, params));
                            }
                        }
                    }

                formatGroup->addChild(viewTypeGroup.release());
            }

        mainGroup->addChild(formatGroup.release());
    }

    return mainGroup.release();
}

} // namespace vkt::BindingModel
