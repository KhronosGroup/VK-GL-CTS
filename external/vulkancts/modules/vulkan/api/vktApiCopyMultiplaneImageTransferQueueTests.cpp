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
 * \brief Vulkan Copy Multiplane Image Transfer Queue Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyMultiplaneImageTransferQueueTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

using namespace ycbcr;
using tcu::TestLog;
using tcu::UVec2;

struct ImageConfig
{
    ImageConfig(vk::VkFormat format_, vk::VkImageTiling tiling_, bool disjoint_, const UVec2 &size_)
        : format(format_)
        , tiling(tiling_)
        , disjoint(disjoint_)
        , size(size_)
    {
    }

    vk::VkFormat format;
    vk::VkImageTiling tiling;
    bool disjoint;
    tcu::UVec2 size;
};

struct TestConfig
{
    TestConfig(const ImageConfig &src_, const ImageConfig &dst_, const bool intermediateBuffer_)
        : src(src_)
        , dst(dst_)
        , intermediateBuffer(intermediateBuffer_)
    {
    }

    ImageConfig src;
    ImageConfig dst;
    bool intermediateBuffer;
};

void checkFormatSupport(Context &context, const ImageConfig &config)
{
    const auto &instInt(context.getInstanceInterface());

    {
        const vk::VkPhysicalDeviceImageFormatInfo2 imageFormatInfo = {
            vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,            // sType;
            nullptr,                                                              // pNext;
            config.format,                                                        // format;
            vk::VK_IMAGE_TYPE_2D,                                                 // type;
            vk::VK_IMAGE_TILING_OPTIMAL,                                          // tiling;
            vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_SAMPLED_BIT, // usage;
            (vk::VkImageCreateFlags)0u                                            // flags
        };

        vk::VkImageFormatProperties2 imageFormatProperties = {};
        imageFormatProperties.sType                        = vk::VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;

        vk::VkResult result = instInt.getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(),
                                                                              &imageFormatInfo, &imageFormatProperties);
        if (result == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            TCU_THROW(NotSupportedError, "Format not supported.");
        VK_CHECK(result);

        // Check for plane compatible format support when the disjoint flag is being used
        if (config.disjoint)
        {
            const vk::PlanarFormatDescription formatDescription = vk::getPlanarFormatDescription(config.format);

            for (uint32_t channelNdx = 0; channelNdx < 4; ++channelNdx)
            {
                if (!formatDescription.hasChannelNdx(channelNdx))
                    continue;
                uint32_t planeNdx                  = formatDescription.channels[channelNdx].planeNdx;
                vk::VkFormat planeCompatibleFormat = getPlaneCompatibleFormat(formatDescription, planeNdx);

                const vk::VkPhysicalDeviceImageFormatInfo2 planeImageFormatInfo = {
                    vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,            // sType;
                    nullptr,                                                              // pNext;
                    planeCompatibleFormat,                                                // format;
                    vk::VK_IMAGE_TYPE_2D,                                                 // type;
                    vk::VK_IMAGE_TILING_OPTIMAL,                                          // tiling;
                    vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT | vk::VK_IMAGE_USAGE_SAMPLED_BIT, // usage;
                    (vk::VkImageCreateFlags)0u                                            // flags
                };

                vk::VkResult planesResult = instInt.getPhysicalDeviceImageFormatProperties2(
                    context.getPhysicalDevice(), &planeImageFormatInfo, &imageFormatProperties);
                if (planesResult == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
                    TCU_THROW(NotSupportedError, "Plane compatibile format not supported.");
                VK_CHECK(planesResult);
            }
        }
    }

    {
        const vk::VkFormatProperties properties(vk::getPhysicalDeviceFormatProperties(
            context.getInstanceInterface(), context.getPhysicalDevice(), config.format));
        const vk::VkFormatFeatureFlags features(config.tiling == vk::VK_IMAGE_TILING_OPTIMAL ?
                                                    properties.optimalTilingFeatures :
                                                    properties.linearTilingFeatures);

        if ((features & vk::VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) == 0 &&
            (features & vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)
        {
            TCU_THROW(NotSupportedError, "Format doesn't support copies");
        }

        if (config.disjoint && ((features & vk::VK_FORMAT_FEATURE_DISJOINT_BIT) == 0))
            TCU_THROW(NotSupportedError, "Format doesn't support disjoint planes");
    }
}

void checkSupport(Context &context, const TestConfig config)
{
    const vk::VkPhysicalDeviceLimits limits = context.getDeviceProperties().limits;

    if (context.getTransferQueueFamilyIndex() == -1)
        TCU_THROW(NotSupportedError, "Device does not have dedicated transfer queues");

    if (config.src.size.x() > limits.maxImageDimension2D || config.src.size.y() > limits.maxImageDimension2D ||
        config.dst.size.x() > limits.maxImageDimension2D || config.dst.size.y() > limits.maxImageDimension2D)
    {
        TCU_THROW(NotSupportedError, "Requested image dimensions not supported");
    }

    checkFormatSupport(context, config.src);
    checkFormatSupport(context, config.dst);
}

bool isCompatible(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
    if (srcFormat == dstFormat)
        return true;
    else
    {
        DE_ASSERT(srcFormat != VK_FORMAT_UNDEFINED && dstFormat != VK_FORMAT_UNDEFINED);

        if (de::contains(formats::compatibleFormats8Bit, srcFormat) &&
            de::contains(formats::compatibleFormats8Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats16Bit, srcFormat) &&
            de::contains(formats::compatibleFormats16Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats24Bit, srcFormat) &&
            de::contains(formats::compatibleFormats24Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats32Bit, srcFormat) &&
            de::contains(formats::compatibleFormats32Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats48Bit, srcFormat) &&
            de::contains(formats::compatibleFormats48Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats64Bit, srcFormat) &&
            de::contains(formats::compatibleFormats64Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats96Bit, srcFormat) &&
            de::contains(formats::compatibleFormats96Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats128Bit, srcFormat) &&
            de::contains(formats::compatibleFormats128Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats192Bit, srcFormat) &&
            de::contains(formats::compatibleFormats192Bit, dstFormat))
            return true;

        if (de::contains(formats::compatibleFormats256Bit, srcFormat) &&
            de::contains(formats::compatibleFormats256Bit, dstFormat))
            return true;

        return false;
    }
}

UVec2 randomUVec2(de::Random &rng, const UVec2 &min, const UVec2 &max)
{
    UVec2 result;

    result[0] = min[0] + (rng.getUint32() % (1 + max[0] - min[0]));
    result[1] = min[1] + (rng.getUint32() % (1 + max[1] - min[1]));

    return result;
}

void genCopies(de::Random &rng, size_t copyCount, vk::VkFormat srcFormat, const UVec2 &srcSize, vk::VkFormat dstFormat,
               const UVec2 &dstSize, const UVec2 &granularity, std::vector<vk::VkImageCopy> *copies)
{
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    const vk::PlanarFormatDescription srcPlaneInfo(vk::getPlanarFormatDescription(srcFormat));
    const vk::PlanarFormatDescription dstPlaneInfo(vk::getPlanarFormatDescription(dstFormat));

    for (uint32_t srcPlaneNdx = 0; srcPlaneNdx < srcPlaneInfo.numPlanes; srcPlaneNdx++)
    {
        for (uint32_t dstPlaneNdx = 0; dstPlaneNdx < dstPlaneInfo.numPlanes; dstPlaneNdx++)
        {
            const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcPlaneInfo, srcPlaneNdx));
            const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstPlaneInfo, dstPlaneNdx));

            if (isCompatible(srcPlaneFormat, dstPlaneFormat))
                pairs.push_back(std::make_pair(srcPlaneNdx, dstPlaneNdx));
        }
    }

    DE_ASSERT(!pairs.empty());

    copies->reserve(copyCount);

    for (size_t copyNdx = 0; copyNdx < copyCount; copyNdx++)
    {
        const std::pair<uint32_t, uint32_t> planes(
            rng.choose<std::pair<uint32_t, uint32_t>>(pairs.begin(), pairs.end()));

        const uint32_t srcPlaneNdx(planes.first);
        const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcPlaneInfo, srcPlaneNdx));
        const UVec2 srcBlockExtent(getBlockExtent(srcPlaneFormat));
        const UVec2 srcPlaneExtent(getPlaneExtent(srcPlaneInfo, srcSize, srcPlaneNdx, 0));
        const UVec2 srcPlaneBlockExtent(srcPlaneExtent / srcBlockExtent);

        const uint32_t dstPlaneNdx(planes.second);
        const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstPlaneInfo, dstPlaneNdx));
        const UVec2 dstBlockExtent(getBlockExtent(dstPlaneFormat));
        const UVec2 dstPlaneExtent(getPlaneExtent(dstPlaneInfo, dstSize, dstPlaneNdx, 0));
        const UVec2 dstPlaneBlockExtent(dstPlaneExtent / dstBlockExtent);

        UVec2 copyBlockExtent(randomUVec2(rng, UVec2(1u, 1u), tcu::min(srcPlaneBlockExtent, dstPlaneBlockExtent)));
        copyBlockExtent[0] = deIntRoundToPow2(copyBlockExtent[0], granularity[0]);
        copyBlockExtent[1] = deIntRoundToPow2(copyBlockExtent[1], granularity[1]);
        UVec2 srcOffset(srcBlockExtent * randomUVec2(rng, UVec2(0u, 0u), srcPlaneBlockExtent - copyBlockExtent));
        srcOffset[0] = srcOffset[0] & ~(granularity[0] - 1u);
        srcOffset[1] = srcOffset[1] & ~(granularity[1] - 1u);
        UVec2 dstOffset(dstBlockExtent * randomUVec2(rng, UVec2(0u, 0u), dstPlaneBlockExtent - copyBlockExtent));
        dstOffset[0] = dstOffset[0] & ~(granularity[0] - 1u);
        dstOffset[1] = dstOffset[1] & ~(granularity[1] - 1u);
        const UVec2 copyExtent(copyBlockExtent * srcBlockExtent);
        const vk::VkImageCopy copy = {
            // src
            {static_cast<vk::VkImageAspectFlags>(srcPlaneInfo.numPlanes > 1 ? vk::getPlaneAspect(srcPlaneNdx) :
                                                                              vk::VK_IMAGE_ASPECT_COLOR_BIT),
             0u, 0u, 1u},
            {
                (int32_t)srcOffset.x(),
                (int32_t)srcOffset.y(),
                0,
            },
            // dst
            {static_cast<vk::VkImageAspectFlags>(dstPlaneInfo.numPlanes > 1 ? vk::getPlaneAspect(dstPlaneNdx) :
                                                                              vk::VK_IMAGE_ASPECT_COLOR_BIT),
             0u, 0u, 1u},
            {
                (int32_t)dstOffset.x(),
                (int32_t)dstOffset.y(),
                0,
            },
            // size
            {copyExtent.x(), copyExtent.y(), 1u}};

        copies->push_back(copy);
    }
}

tcu::SeedBuilder &operator<<(tcu::SeedBuilder &builder, const ImageConfig &config)
{

    builder << (uint32_t)config.format << (uint32_t)config.tiling << config.disjoint << config.size[0]
            << config.size[1];
    return builder;
}

void logImageInfo(TestLog &log, const ImageConfig &config)
{
    log << TestLog::Message << "Format: " << config.format << TestLog::EndMessage;
    log << TestLog::Message << "Tiling: " << config.tiling << TestLog::EndMessage;
    log << TestLog::Message << "Size: " << config.size << TestLog::EndMessage;
    log << TestLog::Message << "Disjoint: " << (config.disjoint ? "true" : "false") << TestLog::EndMessage;
}

void logTestCaseInfo(TestLog &log, const TestConfig &config, const std::vector<vk::VkImageCopy> &copies)
{
    {
        const tcu::ScopedLogSection section(log, "SourceImage", "SourceImage");
        logImageInfo(log, config.src);
    }

    {
        const tcu::ScopedLogSection section(log, "DestinationImage", "DestinationImage");
        logImageInfo(log, config.dst);
    }
    {
        const tcu::ScopedLogSection section(log, "Copies", "Copies");

        for (size_t copyNdx = 0; copyNdx < copies.size(); copyNdx++)
            log << TestLog::Message << copies[copyNdx] << TestLog::EndMessage;
    }
}

vk::VkFormat chooseFloatFormat(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
    const std::vector<vk::VkFormat> floatFormats = {
        vk::VK_FORMAT_B10G11R11_UFLOAT_PACK32, vk::VK_FORMAT_R16_SFLOAT,
        vk::VK_FORMAT_R16G16_SFLOAT,           vk::VK_FORMAT_R16G16B16_SFLOAT,
        vk::VK_FORMAT_R16G16B16A16_SFLOAT,     vk::VK_FORMAT_R32_SFLOAT,
        vk::VK_FORMAT_R32G32_SFLOAT,           vk::VK_FORMAT_R32G32B32_SFLOAT,
        vk::VK_FORMAT_R32G32B32A32_SFLOAT,     vk::VK_FORMAT_R64_SFLOAT,
        vk::VK_FORMAT_R64G64_SFLOAT,           vk::VK_FORMAT_R64G64B64_SFLOAT,
        vk::VK_FORMAT_R64G64B64A64_SFLOAT,
    };

    if (std::find(floatFormats.begin(), floatFormats.end(), srcFormat) != floatFormats.end())
        return srcFormat;

    return dstFormat;
}

vk::Move<vk::VkImage> createImage(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFormat format,
                                  const UVec2 &size, bool disjoint, vk::VkImageTiling tiling)
{
    const vk::VkImageCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        disjoint ? (vk::VkImageCreateFlags)vk::VK_IMAGE_CREATE_DISJOINT_BIT : (vk::VkImageCreateFlags)0u,

        vk::VK_IMAGE_TYPE_2D,
        format,
        vk::makeExtent3D(size.x(), size.y(), 1u),
        1u,
        1u,
        vk::VK_SAMPLE_COUNT_1_BIT,
        tiling,
        vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        vk::VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        tiling == vk::VK_IMAGE_TILING_LINEAR ? vk::VK_IMAGE_LAYOUT_PREINITIALIZED : vk::VK_IMAGE_LAYOUT_UNDEFINED,
    };

    return vk::createImage(vkd, device, &createInfo);
}

uint32_t getBlockByteSize(vk::VkFormat format)
{
    switch (format)
    {
    case vk::VK_FORMAT_B8G8R8G8_422_UNORM:
    case vk::VK_FORMAT_G8B8G8R8_422_UNORM:
        return 4u;

    case vk::VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
    case vk::VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
    case vk::VK_FORMAT_B16G16R16G16_422_UNORM:
    case vk::VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
    case vk::VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
    case vk::VK_FORMAT_G16B16G16R16_422_UNORM:
    case vk::VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
    case vk::VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
    case vk::VK_FORMAT_R16G16B16A16_UNORM:
        return 4u * 2u;

    case vk::VK_FORMAT_R10X6_UNORM_PACK16:
    case vk::VK_FORMAT_R12X4_UNORM_PACK16:
        return 2u;

    case vk::VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
    case vk::VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        return 2u * 2u;

    case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        return 3u * 2u;

    case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case vk::VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case vk::VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case vk::VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case vk::VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case vk::VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case vk::VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
    case vk::VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case vk::VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case vk::VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case vk::VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case vk::VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case vk::VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT:
    case vk::VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT:
    case vk::VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT:
    case vk::VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT:
        DE_FATAL("Plane formats not supported");
        return ~0u;

    default:
        return (uint32_t)vk::mapVkFormat(format).getPixelSize();
    }
}

bool isCopyCompatible(vk::VkFormat srcFormat, vk::VkFormat dstFormat)
{
    if (isYCbCrFormat(srcFormat) && isYCbCrFormat(dstFormat))
    {
        const vk::PlanarFormatDescription srcPlaneInfo(vk::getPlanarFormatDescription(srcFormat));
        const vk::PlanarFormatDescription dstPlaneInfo(vk::getPlanarFormatDescription(dstFormat));

        for (uint32_t srcPlaneNdx = 0; srcPlaneNdx < srcPlaneInfo.numPlanes; srcPlaneNdx++)
        {
            for (uint32_t dstPlaneNdx = 0; dstPlaneNdx < dstPlaneInfo.numPlanes; dstPlaneNdx++)
            {
                const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcFormat, srcPlaneNdx));
                const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstFormat, dstPlaneNdx));

                if (isCompatible(srcPlaneFormat, dstPlaneFormat))
                    return true;
            }
        }
    }
    else if (isYCbCrFormat(srcFormat))
    {
        const vk::PlanarFormatDescription srcPlaneInfo(vk::getPlanarFormatDescription(srcFormat));

        for (uint32_t srcPlaneNdx = 0; srcPlaneNdx < srcPlaneInfo.numPlanes; srcPlaneNdx++)
        {
            const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(srcFormat, srcPlaneNdx));

            if (isCompatible(srcPlaneFormat, dstFormat))
                return true;
        }
    }
    else if (isYCbCrFormat(dstFormat))
    {
        const vk::PlanarFormatDescription dstPlaneInfo(vk::getPlanarFormatDescription(dstFormat));

        for (uint32_t dstPlaneNdx = 0; dstPlaneNdx < dstPlaneInfo.numPlanes; dstPlaneNdx++)
        {
            const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(dstFormat, dstPlaneNdx));

            if (isCompatible(dstPlaneFormat, srcFormat))
                return true;
        }
    }
    else
        return isCompatible(srcFormat, dstFormat);

    return false;
}

tcu::TestStatus testCopies(Context &context, TestConfig config)
{
    const auto queueIndex = context.getTransferQueueFamilyIndex();
    DE_ASSERT(queueIndex != -1);
    const std::vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());
    DE_ASSERT((int)queueProps.size() > queueIndex);
    const VkQueueFamilyProperties *xferProps = &queueProps[queueIndex];
    const UVec2 granularity(xferProps->minImageTransferGranularity.width,
                            xferProps->minImageTransferGranularity.height);

    const size_t copyCount = 10;
    auto &log(context.getTestContext().getLog());

    MultiPlaneImageData srcData(config.src.format, config.src.size);
    MultiPlaneImageData dstData(config.dst.format, config.dst.size);
    MultiPlaneImageData result(config.dst.format, config.dst.size);

    std::vector<vk::VkImageCopy> copies;

    tcu::SeedBuilder builder;
    builder << 6792903u << config.src << config.dst;

    de::Random rng(builder.get());
    const bool noNan = true;
    genCopies(rng, copyCount, config.src.format, config.src.size, config.dst.format, config.dst.size, granularity,
              &copies);
    logTestCaseInfo(log, config, copies);

    // To avoid putting NaNs in dst in the image copy
    fillRandom(&rng, &srcData, chooseFloatFormat(config.src.format, config.dst.format), noNan);
    fillRandom(&rng, &dstData, config.dst.format, noNan);

    {
        const vk::DeviceInterface &vkd(context.getDeviceInterface());
        const vk::VkDevice device(context.getDevice());

        const vk::Unique<vk::VkImage> srcImage(
            createImage(vkd, device, config.src.format, config.src.size, config.src.disjoint, config.src.tiling));
        const vk::MemoryRequirement srcMemoryRequirement(config.src.tiling == vk::VK_IMAGE_TILING_OPTIMAL ?
                                                             vk::MemoryRequirement::Any :
                                                             vk::MemoryRequirement::HostVisible);
        const vk::VkImageCreateFlags srcCreateFlags(config.src.disjoint ? vk::VK_IMAGE_CREATE_DISJOINT_BIT :
                                                                          (vk::VkImageCreateFlagBits)0u);
        const std::vector<AllocationSp> srcImageMemory(
            allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *srcImage, config.src.format,
                                       srcCreateFlags, srcMemoryRequirement));

        const vk::Unique<vk::VkImage> dstImage(
            createImage(vkd, device, config.dst.format, config.dst.size, config.dst.disjoint, config.dst.tiling));
        const vk::MemoryRequirement dstMemoryRequirement(config.dst.tiling == vk::VK_IMAGE_TILING_OPTIMAL ?
                                                             vk::MemoryRequirement::Any :
                                                             vk::MemoryRequirement::HostVisible);
        const vk::VkImageCreateFlags dstCreateFlags(config.dst.disjoint ? vk::VK_IMAGE_CREATE_DISJOINT_BIT :
                                                                          (vk::VkImageCreateFlagBits)0u);
        const std::vector<AllocationSp> dstImageMemory(
            allocateAndBindImageMemory(vkd, device, context.getDefaultAllocator(), *dstImage, config.dst.format,
                                       dstCreateFlags, dstMemoryRequirement));

        if (config.src.tiling == vk::VK_IMAGE_TILING_OPTIMAL)
            uploadImage(vkd, device, context.getUniversalQueueFamilyIndex(), context.getDefaultAllocator(), *srcImage,
                        srcData, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        else
            fillImageMemory(vkd, device, context.getUniversalQueueFamilyIndex(), *srcImage, srcImageMemory, srcData,
                            vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        if (config.dst.tiling == vk::VK_IMAGE_TILING_OPTIMAL)
            uploadImage(vkd, device, context.getUniversalQueueFamilyIndex(), context.getDefaultAllocator(), *dstImage,
                        dstData, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        else
            fillImageMemory(vkd, device, context.getUniversalQueueFamilyIndex(), *dstImage, dstImageMemory, dstData,
                            vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        {
            const uint32_t transferQueueFamilyNdx(context.getTransferQueueFamilyIndex());
            const vk::VkQueue transferQueue(context.getTransferQueue());
            const vk::Unique<vk::VkCommandPool> transferCmdPool(
                createCommandPool(vkd, device, (vk::VkCommandPoolCreateFlags)0, transferQueueFamilyNdx));
            const vk::Unique<vk::VkCommandBuffer> transferCmdBuffer(
                allocateCommandBuffer(vkd, device, *transferCmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

            beginCommandBuffer(vkd, *transferCmdBuffer);

            std::vector<de::MovePtr<vk::BufferWithMemory>> buffers(copies.size());

            for (size_t i = 0; i < copies.size(); i++)
            {
                const uint32_t srcPlaneNdx(
                    copies[i].srcSubresource.aspectMask != vk::VK_IMAGE_ASPECT_COLOR_BIT ?
                        vk::getAspectPlaneNdx((vk::VkImageAspectFlagBits)copies[i].srcSubresource.aspectMask) :
                        0u);

                const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(config.src.format, srcPlaneNdx));

                const uint32_t blockSizeBytes(getBlockByteSize(srcPlaneFormat));
                const vk::VkDeviceSize bufferSize = config.src.size.x() * config.src.size.y() * blockSizeBytes;
                const vk::VkBufferCreateInfo bufferCreateInfo = {
                    vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                  // const void* pNext;
                    0u,                                       // VkBufferCreateFlags flags;
                    bufferSize,                               // VkDeviceSize size;
                    vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
                    vk::VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
                    0u,                                       // uint32_t queueFamilyIndexCount;
                    nullptr,                                  // const uint32_t* pQueueFamilyIndices;
                };
                buffers[i] = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
                    vkd, device, context.getDefaultAllocator(), bufferCreateInfo, vk::MemoryRequirement::Any));

                if (config.intermediateBuffer)
                {
                    const vk::VkBufferImageCopy imageToBufferCopy = {
                        0u,                       // VkDeviceSize bufferOffset;
                        0u,                       // uint32_t bufferRowLength;
                        0u,                       // uint32_t bufferImageHeight;
                        copies[i].srcSubresource, // VkImageSubresourceLayers imageSubresource;
                        copies[i].srcOffset,      // VkOffset3D imageOffset;
                        copies[i].extent,         // VkExtent3D imageExtent;
                    };
                    vkd.cmdCopyImageToBuffer(*transferCmdBuffer, *srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             **buffers[i], 1, &imageToBufferCopy);

                    const vk::VkBufferMemoryBarrier bufferBarrier = {
                        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
                        nullptr,                                     // const void* pNext;
                        vk::VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
                        vk::VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags dstAccessMask;
                        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
                        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
                        **buffers[i],                                // VkBuffer buffer;
                        0u,                                          // VkDeviceSize offset;
                        VK_WHOLE_SIZE,                               // VkDeviceSize size;
                    };

                    vkd.cmdPipelineBarrier(*transferCmdBuffer,
                                           (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           (vk::VkDependencyFlags)0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);

                    const vk::VkBufferImageCopy bufferToImageCopy = {
                        0u,                       // VkDeviceSize bufferOffset;
                        0u,                       // uint32_t bufferRowLength;
                        0u,                       // uint32_t bufferImageHeight;
                        copies[i].dstSubresource, // VkImageSubresourceLayers imageSubresource;
                        copies[i].dstOffset,      // VkOffset3D imageOffset;
                        copies[i].extent,         // VkExtent3D imageExtent;
                    };
                    vkd.cmdCopyBufferToImage(*transferCmdBuffer, **buffers[i], *dstImage,
                                             vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferToImageCopy);
                }
                else
                {
                    vkd.cmdCopyImage(*transferCmdBuffer, *srcImage, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstImage,
                                     vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copies[i]);
                }

                const vk::VkImageMemoryBarrier preCopyBarrier = {vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                                                 nullptr,
                                                                 vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                 vk::VK_ACCESS_TRANSFER_READ_BIT |
                                                                     vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                                                                 vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                 vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                 VK_QUEUE_FAMILY_IGNORED,
                                                                 VK_QUEUE_FAMILY_IGNORED,
                                                                 *dstImage,
                                                                 {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

                vkd.cmdPipelineBarrier(*transferCmdBuffer, (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       (vk::VkPipelineStageFlags)vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       (vk::VkDependencyFlags)0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
            }

            endCommandBuffer(vkd, *transferCmdBuffer);

            submitCommandsAndWaitWithSync(vkd, device, transferQueue, *transferCmdBuffer);
        }

        if (config.dst.tiling == vk::VK_IMAGE_TILING_OPTIMAL)
            downloadImage(vkd, device, context.getTransferQueueFamilyIndex(), context.getDefaultAllocator(), *dstImage,
                          &result, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        else
            readImageMemory(vkd, device, context.getTransferQueueFamilyIndex(), *dstImage, dstImageMemory, &result,
                            vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    {
        MultiPlaneImageData reference(dstData);
        const size_t maxErrorCount = 30;
        size_t errorCount          = 0;

        for (size_t copyNdx = 0; copyNdx < copies.size(); copyNdx++)
        {
            const vk::VkImageCopy &copy(copies[copyNdx]);

            const uint32_t srcPlaneNdx(
                copy.srcSubresource.aspectMask != vk::VK_IMAGE_ASPECT_COLOR_BIT ?
                    vk::getAspectPlaneNdx((vk::VkImageAspectFlagBits)copy.srcSubresource.aspectMask) :
                    0u);
            const UVec2 srcPlaneExtent(getPlaneExtent(srcData.getDescription(), config.src.size, srcPlaneNdx, 0));

            const vk::VkFormat srcPlaneFormat(getPlaneCompatibleFormat(config.src.format, srcPlaneNdx));
            const UVec2 srcBlockExtent(getBlockExtent(srcPlaneFormat));

            const uint32_t blockSizeBytes(getBlockByteSize(srcPlaneFormat));

            const UVec2 srcPlaneBlockExtent(srcPlaneExtent / srcBlockExtent);
            const UVec2 srcBlockOffset(copy.srcOffset.x / srcBlockExtent.x(), copy.srcOffset.y / srcBlockExtent.y());
            const UVec2 srcBlockPitch(blockSizeBytes, blockSizeBytes * srcPlaneBlockExtent.x());

            const uint32_t dstPlaneNdx(
                copy.dstSubresource.aspectMask != vk::VK_IMAGE_ASPECT_COLOR_BIT ?
                    vk::getAspectPlaneNdx((vk::VkImageAspectFlagBits)copy.dstSubresource.aspectMask) :
                    0u);
            const UVec2 dstPlaneExtent(getPlaneExtent(dstData.getDescription(), config.dst.size, dstPlaneNdx, 0));

            const vk::VkFormat dstPlaneFormat(getPlaneCompatibleFormat(config.dst.format, dstPlaneNdx));
            const UVec2 dstBlockExtent(getBlockExtent(dstPlaneFormat));

            const UVec2 dstPlaneBlockExtent(dstPlaneExtent / dstBlockExtent);
            const UVec2 dstBlockOffset(copy.dstOffset.x / dstBlockExtent.x(), copy.dstOffset.y / dstBlockExtent.y());
            const UVec2 dstBlockPitch(blockSizeBytes, blockSizeBytes * dstPlaneBlockExtent.x());

            const UVec2 blockExtent(copy.extent.width / srcBlockExtent.x(), copy.extent.height / srcBlockExtent.y());

            DE_ASSERT(blockSizeBytes == getBlockByteSize(dstPlaneFormat));

            for (uint32_t y = 0; y < blockExtent.y(); y++)
            {
                const uint32_t size   = blockExtent.x() * blockSizeBytes;
                const uint32_t srcPos = tcu::dot(srcBlockPitch, UVec2(srcBlockOffset.x(), srcBlockOffset.y() + y));
                const uint32_t dstPos = tcu::dot(dstBlockPitch, UVec2(dstBlockOffset.x(), dstBlockOffset.y() + y));

                deMemcpy(((uint8_t *)reference.getPlanePtr(dstPlaneNdx)) + dstPos,
                         ((const uint8_t *)srcData.getPlanePtr(srcPlaneNdx)) + srcPos, size);
            }
        }

        bool ignoreLsb6Bits = areLsb6BitsDontCare(srcData.getFormat(), dstData.getFormat());
        bool ignoreLsb4Bits = areLsb4BitsDontCare(srcData.getFormat(), dstData.getFormat());

        for (uint32_t planeNdx = 0; planeNdx < result.getDescription().numPlanes; ++planeNdx)
        {
            uint32_t planeSize = vk::getPlaneSizeInBytes(result.getDescription(), result.getSize(), planeNdx, 0u, 1u);
            for (size_t byteNdx = 0; byteNdx < planeSize; byteNdx++)
            {
                const uint8_t res = ((const uint8_t *)result.getPlanePtr(planeNdx))[byteNdx];
                const uint8_t ref = ((const uint8_t *)reference.getPlanePtr(planeNdx))[byteNdx];

                uint8_t mask = 0xFF;
                if (!(byteNdx & 0x01) && (ignoreLsb6Bits))
                    mask = 0xC0;
                else if (!(byteNdx & 0x01) && (ignoreLsb4Bits))
                    mask = 0xF0;

                if ((res & mask) != (ref & mask))
                {
                    log << TestLog::Message << "Plane: " << planeNdx << ", Offset: " << byteNdx
                        << ", Expected: " << (uint32_t)(ref & mask) << ", Got: " << (uint32_t)(res & mask)
                        << TestLog::EndMessage;
                    errorCount++;

                    if (errorCount > maxErrorCount)
                        break;
                }
            }

            if (errorCount > maxErrorCount)
                break;
        }

        if (errorCount > 0)
            return tcu::TestStatus::fail(
                "Failed, found " +
                (errorCount > maxErrorCount ? de::toString(maxErrorCount) + "+" : de::toString(errorCount)) +
                " incorrect bytes");
        else
            return tcu::TestStatus::pass("Pass");
    }
}

} // namespace

tcu::TestCaseGroup *createCopyMultiplaneImageTransferQueueTests(tcu::TestContext &testCtx)
{
    const struct
    {
        VkImageTiling value;
        const char *name;
    } tilings[] = {{VK_IMAGE_TILING_OPTIMAL, "optimal"}, {VK_IMAGE_TILING_LINEAR, "linear"}};

    const VkFormat multiplaneFormats[] = {VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
                                          VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                                          VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
                                          VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
                                          VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
                                          VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
                                          VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
                                          VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
                                          VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
                                          VK_FORMAT_G16_B16R16_2PLANE_422_UNORM};

    std::vector<VkImageCreateFlags> createFlags{
        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        VK_IMAGE_CREATE_ALIAS_BIT
        /* VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT - present tests use only one physical device */
        ,
        VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT
        /* VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT not apply with planar formats */
        ,
        VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
        VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT};

    de::MovePtr<tcu::TestCaseGroup> multiplaneGroup(new tcu::TestCaseGroup(testCtx, "multiplanar_xfer"));

    for (size_t srcFormatNdx = 0; srcFormatNdx < de::arrayLength(multiplaneFormats); srcFormatNdx++)
    {
        const vk::VkFormat srcFormat(multiplaneFormats[srcFormatNdx]);
        const UVec2 srcSize(isYCbCrFormat(srcFormat) ? UVec2(64u, 64u) : UVec2(23u, 17u));
        const std::string srcFormatName(de::toLower(std::string(getFormatName(srcFormat)).substr(10)));
        de::MovePtr<tcu::TestCaseGroup> srcFormatGroup(new tcu::TestCaseGroup(testCtx, srcFormatName.c_str()));
        for (size_t dstFormatNdx = 0; dstFormatNdx < DE_LENGTH_OF_ARRAY(multiplaneFormats); dstFormatNdx++)
        {
            const vk::VkFormat dstFormat(multiplaneFormats[dstFormatNdx]);
            const UVec2 dstSize(isYCbCrFormat(dstFormat) ? UVec2(64u, 64u) : UVec2(23u, 17u));
            const std::string dstFormatName(de::toLower(std::string(getFormatName(dstFormat)).substr(10)));

            if ((!vk::isYCbCrFormat(srcFormat) && !vk::isYCbCrFormat(dstFormat)) ||
                !isCopyCompatible(srcFormat, dstFormat))
                continue;

            de::MovePtr<tcu::TestCaseGroup> dstFormatGroup(new tcu::TestCaseGroup(testCtx, dstFormatName.c_str()));
            for (size_t srcTilingNdx = 0; srcTilingNdx < DE_LENGTH_OF_ARRAY(tilings); srcTilingNdx++)
            {
                const vk::VkImageTiling srcTiling = tilings[srcTilingNdx].value;
                const char *const srcTilingName   = tilings[srcTilingNdx].name;

                for (size_t dstTilingNdx = 0; dstTilingNdx < DE_LENGTH_OF_ARRAY(tilings); dstTilingNdx++)
                {
                    const vk::VkImageTiling dstTiling = tilings[dstTilingNdx].value;
                    const char *const dstTilingName   = tilings[dstTilingNdx].name;

                    if (srcTiling == VK_IMAGE_TILING_LINEAR || dstTiling == VK_IMAGE_TILING_LINEAR)
                        continue;

                    for (size_t srcDisjointNdx = 0; srcDisjointNdx < 2; srcDisjointNdx++)
                        for (size_t dstDisjointNdx = 0; dstDisjointNdx < 2; dstDisjointNdx++)
                            for (size_t useBufferNdx = 0; useBufferNdx < 2; useBufferNdx++)
                            {
                                const bool srcDisjoint = srcDisjointNdx == 1;
                                const bool dstDisjoint = dstDisjointNdx == 1;
                                const bool useBuffer   = useBufferNdx == 1;
                                const TestConfig config(ImageConfig(srcFormat, srcTiling, srcDisjoint, srcSize),
                                                        ImageConfig(dstFormat, dstTiling, dstDisjoint, dstSize),
                                                        useBuffer);

                                addFunctionCase(dstFormatGroup.get(),
                                                std::string(srcTilingName) + (srcDisjoint ? "_disjoint_" : "_") +
                                                    (useBuffer ? "buffer_" : "") + std::string(dstTilingName) +
                                                    (dstDisjoint ? "_disjoint" : ""),
                                                checkSupport, testCopies, config);
                            }
                }
            }
            srcFormatGroup->addChild(dstFormatGroup.release());
        }

        multiplaneGroup->addChild(srcFormatGroup.release());
    }

    return multiplaneGroup.release();
}

} // namespace api
} // namespace vkt
