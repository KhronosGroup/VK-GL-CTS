/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Google Inc.
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
 * \file  vktSparseResourcesImageBlockShapes.cpp
 * \brief Standard block shape tests.
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

class ImageBlockShapesCase : public TestCase
{
public:
    ImageBlockShapesCase(tcu::TestContext &testCtx, const std::string &name, const ImageType imageType,
                         const tcu::UVec3 &imageSize, const VkFormat format, uint32_t numSamples);

    void initPrograms(SourceCollections &sourceCollections) const
    {
        DE_UNREF(sourceCollections);
    }
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const ImageType m_imageType;
    const tcu::UVec3 m_imageSize;
    const VkFormat m_format;
    const uint32_t m_numSamples;
};

ImageBlockShapesCase::ImageBlockShapesCase(tcu::TestContext &testCtx, const std::string &name,
                                           const ImageType imageType, const tcu::UVec3 &imageSize,
                                           const VkFormat format, uint32_t numSamples)
    : TestCase(testCtx, name)
    , m_imageType(imageType)
    , m_imageSize(imageSize)
    , m_format(format)
    , m_numSamples(numSamples)
{
}

void ImageBlockShapesCase::checkSupport(Context &context) const
{
    const InstanceInterface &instance     = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    // Check the image size does not exceed device limits
    if (!isImageSizeSupported(instance, physicalDevice, m_imageType, m_imageSize))
        TCU_THROW(NotSupportedError, "Image size not supported for device");

    // Check if device supports sparse operations for image type
    if (!checkSparseSupportForImageType(instance, physicalDevice, m_imageType))
        TCU_THROW(NotSupportedError, "Sparse residency for image type is not supported");

    {
        const VkPhysicalDeviceFeatures features = context.getDeviceFeatures();
        bool sparseSamplesSupported             = false;
        switch (m_numSamples)
        {
        case VK_SAMPLE_COUNT_1_BIT:
            sparseSamplesSupported = features.sparseResidencyImage2D;
            break;
        case VK_SAMPLE_COUNT_2_BIT:
            sparseSamplesSupported = features.sparseResidency2Samples;
            break;
        case VK_SAMPLE_COUNT_4_BIT:
            sparseSamplesSupported = features.sparseResidency4Samples;
            break;
        case VK_SAMPLE_COUNT_8_BIT:
            sparseSamplesSupported = features.sparseResidency8Samples;
            break;
        case VK_SAMPLE_COUNT_16_BIT:
            sparseSamplesSupported = features.sparseResidency16Samples;
            break;
        default:
            break;
        }

        if (!sparseSamplesSupported)
            throw tcu::NotSupportedError("Unsupported number of samples for sparse residency");
    }

    if (formatIsR64(m_format))
    {
        context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

        if (context.getShaderImageAtomicInt64FeaturesEXT().sparseImageInt64Atomics == VK_FALSE)
        {
            TCU_THROW(NotSupportedError, "sparseImageInt64Atomics is not supported for device");
        }
    }
}

class ImageBlockShapesInstance : public SparseResourcesBaseInstance
{
public:
    ImageBlockShapesInstance(Context &context, const ImageType imageType, const tcu::UVec3 &imageSize,
                             const VkFormat format, uint32_t numSamples);

    tcu::TestStatus iterate(void);

private:
    const ImageType m_imageType;
    const tcu::UVec3 m_imageSize;
    const VkFormat m_format;
    const uint32_t m_numSamples;
};

ImageBlockShapesInstance::ImageBlockShapesInstance(Context &context, const ImageType imageType,
                                                   const tcu::UVec3 &imageSize, const VkFormat format,
                                                   uint32_t numSamples)
    : SparseResourcesBaseInstance(context)
    , m_imageType(imageType)
    , m_imageSize(imageSize)
    , m_format(format)
    , m_numSamples(numSamples)
{
}

tcu::TestStatus ImageBlockShapesInstance::iterate(void)
{
    const InstanceInterface &instance                         = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                     = m_context.getPhysicalDevice();
    const VkPhysicalDeviceProperties physicalDeviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);
    VkImageCreateInfo imageCreateInfo;
    std::vector<VkSparseImageMemoryRequirements> sparseMemoryRequirements;
    const VkPhysicalDeviceSparseProperties sparseProperties = physicalDeviceProperties.sparseProperties;
    const bool isCompressedFmt                              = isCompressedFormat(m_format);
    const uint32_t bitsPerPixel                             = 8u;

    imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                 = nullptr;
    imageCreateInfo.flags                 = VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
    imageCreateInfo.imageType             = mapImageType(m_imageType);
    imageCreateInfo.format                = m_format;
    imageCreateInfo.extent                = makeExtent3D(getLayerSize(m_imageType, m_imageSize));
    imageCreateInfo.mipLevels             = 1u;
    imageCreateInfo.arrayLayers           = getNumLayers(m_imageType, m_imageSize);
    imageCreateInfo.samples               = static_cast<VkSampleCountFlagBits>(m_numSamples);
    imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage                 = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0u;
    imageCreateInfo.pQueueFamilyIndices   = nullptr;

    if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
    {
        imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    // Check the format supports given number of samples
    VkImageFormatProperties imageFormatProperties;

    if (instance.getPhysicalDeviceImageFormatProperties(
            physicalDevice, imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling,
            imageCreateInfo.usage, imageCreateInfo.flags, &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Image format does not support sparse operations");
    }

    if (!(imageFormatProperties.sampleCounts & imageCreateInfo.samples))
        TCU_THROW(NotSupportedError, "The image format does not support the number of samples specified");

    // Check if device supports sparse operations for image format
    if (!checkSparseSupportForImageFormat(instance, physicalDevice, imageCreateInfo))
        TCU_THROW(NotSupportedError, "The image format does not support sparse operations");

    {
        QueueRequirementsVec queueRequirements;
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));

        createDeviceSupportingQueues(queueRequirements);
    }

    {
        const DeviceInterface &deviceInterface = getDeviceInterface();

        // Create sparse image
        const Unique<VkImage> imageSparse(createImage(deviceInterface, getDevice(), &imageCreateInfo));

        // Get sparse image sparse memory requirements
        sparseMemoryRequirements = getImageSparseMemoryRequirements(deviceInterface, getDevice(), *imageSparse);

        DE_ASSERT(sparseMemoryRequirements.size() != 0);
    }

    const uint32_t numPlanes = isCompressedFmt ? 1u : getPlanarFormatDescription(m_format).numPlanes;

    for (uint32_t planeNdx = 0; planeNdx < numPlanes; ++planeNdx)
    {
        const VkImageAspectFlags aspect = (numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;
        const uint32_t aspectIndex      = getSparseAspectRequirementsIndex(sparseMemoryRequirements, aspect);

        if (aspectIndex == NO_MATCH_FOUND)
            TCU_THROW(NotSupportedError, "Not supported image aspect");

        VkSparseImageMemoryRequirements aspectRequirements = sparseMemoryRequirements[aspectIndex];
        VkExtent3D imageGranularity                        = aspectRequirements.formatProperties.imageGranularity;

        uint32_t pixelSize = 0u;
        VkExtent3D expectedGranularity;

        if (isCompressedFmt)
        {
            pixelSize = getBlockSizeInBytes(m_format) * bitsPerPixel;
        }
        else
        {
            pixelSize = static_cast<uint32_t>(getPlanarFormatDescription(m_format).planes[planeNdx].elementSizeBytes) *
                        bitsPerPixel;
        }

        if (m_imageType == IMAGE_TYPE_3D)
        {
            if (!sparseProperties.residencyStandard3DBlockShape)
                return tcu::TestStatus::pass("Pass (residencyStandard3DBlockShape disabled)");

            switch (pixelSize)
            {
            case 8:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 32;
                expectedGranularity.depth  = 32;
                break;
            case 16:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 32;
                expectedGranularity.depth  = 32;
                break;
            case 32:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 32;
                expectedGranularity.depth  = 16;
                break;
            case 64:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 16;
                expectedGranularity.depth  = 16;
                break;
            default:
                DE_ASSERT(pixelSize == 128);
                expectedGranularity.width  = 16;
                expectedGranularity.height = 16;
                expectedGranularity.depth  = 16;
                break;
            }
        }
        else if (m_numSamples == 2)
        {
            if (!sparseProperties.residencyStandard2DMultisampleBlockShape)
                return tcu::TestStatus::pass("Pass (residencyStandard2DMultisampleBlockShape disabled)");

            expectedGranularity.depth = 1;

            switch (pixelSize)
            {
            case 8:
                expectedGranularity.width  = 128;
                expectedGranularity.height = 256;
                break;
            case 16:
                expectedGranularity.width  = 128;
                expectedGranularity.height = 128;
                break;
            case 32:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 128;
                break;
            case 64:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 64;
                break;
            default:
                DE_ASSERT(pixelSize == 128);
                expectedGranularity.width  = 32;
                expectedGranularity.height = 64;
                break;
            }
        }
        else if (m_numSamples == 4)
        {
            if (!sparseProperties.residencyStandard2DMultisampleBlockShape)
                return tcu::TestStatus::pass("Pass (residencyStandard2DMultisampleBlockShape disabled)");

            expectedGranularity.depth = 1;

            switch (pixelSize)
            {
            case 8:
                expectedGranularity.width  = 128;
                expectedGranularity.height = 128;
                break;
            case 16:
                expectedGranularity.width  = 128;
                expectedGranularity.height = 64;
                break;
            case 32:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 64;
                break;
            case 64:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 32;
                break;
            default:
                DE_ASSERT(pixelSize == 128);
                expectedGranularity.width  = 32;
                expectedGranularity.height = 32;
                break;
            }
        }
        else if (m_numSamples == 8)
        {
            if (!sparseProperties.residencyStandard2DMultisampleBlockShape)
                return tcu::TestStatus::pass("Pass (residencyStandard2DMultisampleBlockShape disabled)");

            expectedGranularity.depth = 1;

            switch (pixelSize)
            {
            case 8:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 128;
                break;
            case 16:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 64;
                break;
            case 32:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 64;
                break;
            case 64:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 32;
                break;
            default:
                DE_ASSERT(pixelSize == 128);
                expectedGranularity.width  = 16;
                expectedGranularity.height = 32;
                break;
            }
        }
        else if (m_numSamples == 16)
        {
            if (!sparseProperties.residencyStandard2DMultisampleBlockShape)
                return tcu::TestStatus::pass("Pass (residencyStandard2DMultisampleBlockShape disabled)");

            expectedGranularity.depth = 1;

            switch (pixelSize)
            {
            case 8:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 64;
                break;
            case 16:
                expectedGranularity.width  = 64;
                expectedGranularity.height = 32;
                break;
            case 32:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 32;
                break;
            case 64:
                expectedGranularity.width  = 32;
                expectedGranularity.height = 16;
                break;
            default:
                DE_ASSERT(pixelSize == 128);
                expectedGranularity.width  = 16;
                expectedGranularity.height = 16;
                break;
            }
        }
        else
        {
            DE_ASSERT(m_numSamples == 1);

            if (!sparseProperties.residencyStandard2DBlockShape)
                return tcu::TestStatus::pass("Pass (residencyStandard2DBlockShape disabled)");

            expectedGranularity.depth = 1;

            switch (pixelSize)
            {
            case 8:
                expectedGranularity.width  = 256;
                expectedGranularity.height = 256;
                break;
            case 16:
                expectedGranularity.width  = 256;
                expectedGranularity.height = 128;
                break;
            case 32:
                expectedGranularity.width  = 128;
                expectedGranularity.height = 128;
                break;
            case 64:
                expectedGranularity.width  = 128;
                expectedGranularity.height = 64;
                break;
            default:
                DE_ASSERT(pixelSize == 128);
                expectedGranularity.width  = 64;
                expectedGranularity.height = 64;
                break;
            }
        }

        if (isCompressedFmt)
        {
            expectedGranularity.width *= getBlockWidth(m_format);
            expectedGranularity.height *= getBlockHeight(m_format);
        }

        if (isYCbCr422Format(m_format))
        {
            const tcu::UVec2 blkExt = getBlockExtent(m_format);
            expectedGranularity.width *= blkExt.x();
            expectedGranularity.height *= blkExt.y();
        }

        if (imageGranularity.width != expectedGranularity.width ||
            imageGranularity.height != expectedGranularity.height ||
            imageGranularity.depth != expectedGranularity.depth)
        {
            return tcu::TestStatus::fail("Non-standard block shape used");
        }
    }

    return tcu::TestStatus::pass("Passed");
}

TestInstance *ImageBlockShapesCase::createInstance(Context &context) const
{
    return new ImageBlockShapesInstance(context, m_imageType, m_imageSize, m_format, m_numSamples);
}

} // namespace

std::vector<TestFormat> getImageTestFormats(const ImageType &imageType)
{
    std::vector<TestFormat> results = getTestFormats(imageType);

    {
        std::vector<TestFormat> blockCompressedFormats = {
            {VK_FORMAT_BC1_RGB_UNORM_BLOCK},       {VK_FORMAT_BC1_RGB_SRGB_BLOCK},
            {VK_FORMAT_BC1_RGBA_UNORM_BLOCK},      {VK_FORMAT_BC1_RGBA_SRGB_BLOCK},
            {VK_FORMAT_BC2_UNORM_BLOCK},           {VK_FORMAT_BC2_SRGB_BLOCK},
            {VK_FORMAT_BC3_UNORM_BLOCK},           {VK_FORMAT_BC3_SRGB_BLOCK},
            {VK_FORMAT_BC4_UNORM_BLOCK},           {VK_FORMAT_BC4_SNORM_BLOCK},
            {VK_FORMAT_BC5_UNORM_BLOCK},           {VK_FORMAT_BC5_SNORM_BLOCK},
            {VK_FORMAT_BC6H_UFLOAT_BLOCK},         {VK_FORMAT_BC6H_SFLOAT_BLOCK},
            {VK_FORMAT_BC7_UNORM_BLOCK},           {VK_FORMAT_BC7_SRGB_BLOCK},
            {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK},   {VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK},
            {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK}, {VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK},
            {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK}, {VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK},
            {VK_FORMAT_EAC_R11_UNORM_BLOCK},       {VK_FORMAT_EAC_R11_SNORM_BLOCK},
            {VK_FORMAT_EAC_R11G11_UNORM_BLOCK},    {VK_FORMAT_EAC_R11G11_SNORM_BLOCK},
            {VK_FORMAT_ASTC_4x4_UNORM_BLOCK},      {VK_FORMAT_ASTC_4x4_SRGB_BLOCK},
            {VK_FORMAT_ASTC_5x4_UNORM_BLOCK},      {VK_FORMAT_ASTC_5x4_SRGB_BLOCK},
            {VK_FORMAT_ASTC_5x5_UNORM_BLOCK},      {VK_FORMAT_ASTC_5x5_SRGB_BLOCK},
            {VK_FORMAT_ASTC_6x5_UNORM_BLOCK},      {VK_FORMAT_ASTC_6x5_SRGB_BLOCK},
            {VK_FORMAT_ASTC_6x6_UNORM_BLOCK},      {VK_FORMAT_ASTC_6x6_SRGB_BLOCK},
            {VK_FORMAT_ASTC_8x5_UNORM_BLOCK},      {VK_FORMAT_ASTC_8x5_SRGB_BLOCK},
            {VK_FORMAT_ASTC_8x6_UNORM_BLOCK},      {VK_FORMAT_ASTC_8x6_SRGB_BLOCK},
            {VK_FORMAT_ASTC_8x8_UNORM_BLOCK},      {VK_FORMAT_ASTC_8x8_SRGB_BLOCK},
            {VK_FORMAT_ASTC_10x5_UNORM_BLOCK},     {VK_FORMAT_ASTC_10x5_SRGB_BLOCK},
            {VK_FORMAT_ASTC_10x6_UNORM_BLOCK},     {VK_FORMAT_ASTC_10x6_SRGB_BLOCK},
            {VK_FORMAT_ASTC_10x8_UNORM_BLOCK},     {VK_FORMAT_ASTC_10x8_SRGB_BLOCK},
            {VK_FORMAT_ASTC_10x10_UNORM_BLOCK},    {VK_FORMAT_ASTC_10x10_SRGB_BLOCK},
            {VK_FORMAT_ASTC_12x10_UNORM_BLOCK},    {VK_FORMAT_ASTC_12x10_SRGB_BLOCK},
            {VK_FORMAT_ASTC_12x12_UNORM_BLOCK},    {VK_FORMAT_ASTC_12x12_SRGB_BLOCK},
        };
        std::copy(begin(blockCompressedFormats), end(blockCompressedFormats), std::back_inserter(results));
    }

    return results;
}

tcu::TestCaseGroup *createImageBlockShapesTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_block_shapes"));

    const std::vector<TestImageParameters> imageParameters{
        {IMAGE_TYPE_2D, {tcu::UVec3(512u, 256u, 1u)}, getImageTestFormats(IMAGE_TYPE_2D)},
        {IMAGE_TYPE_2D_ARRAY, {tcu::UVec3(512u, 256u, 6u)}, getImageTestFormats(IMAGE_TYPE_2D_ARRAY)},
        {IMAGE_TYPE_CUBE, {tcu::UVec3(256u, 256u, 1u)}, getImageTestFormats(IMAGE_TYPE_CUBE)},
        {IMAGE_TYPE_CUBE_ARRAY, {tcu::UVec3(256u, 256u, 6u)}, getImageTestFormats(IMAGE_TYPE_CUBE_ARRAY)},
        {IMAGE_TYPE_3D, {tcu::UVec3(512u, 256u, 16u)}, getImageTestFormats(IMAGE_TYPE_3D)}};

    static const uint32_t sampleCounts[] = {1u, 2u, 4u, 8u, 16u};

    for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
    {
        const ImageType imageType = imageParameters[imageTypeNdx].imageType;
        de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(
            new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str()));

        for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
        {
            VkFormat format = imageParameters[imageTypeNdx].formats[formatNdx].format;

            de::MovePtr<tcu::TestCaseGroup> formatGroup(
                new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str()));

            for (int32_t sampleCountNdx = 0; sampleCountNdx < DE_LENGTH_OF_ARRAY(sampleCounts); ++sampleCountNdx)
            {
                for (size_t imageSizeNdx = 0; imageSizeNdx < imageParameters[imageTypeNdx].imageSizes.size();
                     ++imageSizeNdx)
                {
                    const tcu::UVec3 imageSize = imageParameters[imageTypeNdx].imageSizes[imageSizeNdx];

                    // skip test for images with odd sizes for some YCbCr formats
                    if (isYCbCrFormat(format))
                    {
                        tcu::UVec3 imageSizeAlignment = getImageSizeAlignment(format);
                        if ((imageSize.x() % imageSizeAlignment.x()) != 0)
                            continue;
                        if ((imageSize.y() % imageSizeAlignment.y()) != 0)
                            continue;
                    }

                    const uint32_t sampleCount = sampleCounts[sampleCountNdx];

                    if ((imageType != IMAGE_TYPE_2D && imageType != IMAGE_TYPE_2D_ARRAY) && (sampleCount > 1u))
                        continue;

                    const std::string name = std::string("samples_") + de::toString(sampleCount);

                    formatGroup->addChild(
                        new ImageBlockShapesCase(testCtx, name.c_str(), imageType, imageSize, format, sampleCount));
                }
            }
            imageTypeGroup->addChild(formatGroup.release());
        }
        testGroup->addChild(imageTypeGroup.release());
    }

    return testGroup.release();
}

} // namespace sparse
} // namespace vkt
