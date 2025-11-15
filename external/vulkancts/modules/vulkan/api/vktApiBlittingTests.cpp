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
 * \brief Vulkan Blitting Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingUtil.hpp"
#include "vktApiBlittingTests.hpp"

namespace vkt
{

namespace api
{

namespace
{
// CompressedTextureForBlit is a helper class that stores compressed texture data.
// Implementation is based on pipeline::TestTexture2D but it allocates only one level
// and has special cases needed for blits to some formats.

FormatSet dedicatedAllocationImageToImageFormatsToTestSet;
FormatSet dedicatedAllocationBlittingFormatsToTestSet;

class CompressedTextureForBlit
{
public:
    CompressedTextureForBlit(const tcu::CompressedTexFormat &srcFormat, int width, int height, int depth);

    tcu::PixelBufferAccess getDecompressedAccess() const;
    const tcu::CompressedTexture &getCompressedTexture() const;

protected:
    tcu::CompressedTexture m_compressedTexture;
    de::ArrayBuffer<uint8_t> m_decompressedData;
    tcu::PixelBufferAccess m_decompressedAccess;
};

CompressedTextureForBlit::CompressedTextureForBlit(const tcu::CompressedTexFormat &srcFormat, int width, int height,
                                                   int depth)
    : m_compressedTexture(srcFormat, width, height, depth)
{
    de::Random random(123);

    const int compressedDataSize(m_compressedTexture.getDataSize());
    uint8_t *compressedData((uint8_t *)m_compressedTexture.getData());

    tcu::TextureFormat decompressedSrcFormat(tcu::getUncompressedFormat(srcFormat));
    const int decompressedDataSize(tcu::getPixelSize(decompressedSrcFormat) * width * height * depth);

    // generate random data for compresed textre
    if (tcu::isAstcFormat(srcFormat))
    {
        // comparison doesn't currently handle invalid blocks correctly so we use only valid blocks
        tcu::astc::generateRandomValidBlocks(compressedData, compressedDataSize / tcu::astc::BLOCK_SIZE_BYTES,
                                             srcFormat, tcu::TexDecompressionParams::ASTCMODE_LDR, random.getUint32());
    }
    else if ((srcFormat == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK) ||
             (srcFormat == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK))
    {
        // special case - when we are blitting compressed floating-point image we can't have both big and small values
        // in compressed image; to resolve this we are constructing source texture out of set of predefined compressed
        // blocks that after decompression will have components in proper range

        typedef std::array<uint32_t, 4> BC6HBlock;
        DE_STATIC_ASSERT(sizeof(BC6HBlock) == (4 * sizeof(uint32_t)));
        std::vector<BC6HBlock> validBlocks;

        if (srcFormat == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK)
        {
            // define set of few valid blocks that contain values from <0; 1> range
            validBlocks = {
                {{1686671500, 3957317723, 3010132342, 2420137890}}, {{3538027716, 298848033, 1925786021, 2022072301}},
                {{2614043466, 1636155440, 1023731774, 1894349986}}, {{3433039318, 1294346072, 1587319645, 1738449906}},
                {{1386298160, 1639492154, 1273285776, 361562050}},  {{1310110688, 526460754, 3630858047, 537617591}},
                {{3270356556, 2432993217, 2415924417, 1792488857}}, {{1204947583, 353249154, 3739153467, 2068076443}},
            };
        }
        else
        {
            // define set of few valid blocks that contain values from <-1; 1> range
            validBlocks = {
                {{2120678840, 3264271120, 4065378848, 3479743703}}, {{1479697556, 3480872527, 3369382558, 568252340}},
                {{1301480032, 1607738094, 3055221704, 3663953681}}, {{3531657186, 2285472028, 1429601507, 1969308187}},
                {{73229044, 650504649, 1120954865, 2626631975}},    {{3872486086, 15326178, 2565171269, 2857722432}},
                {{1301480032, 1607738094, 3055221704, 3663953681}}, {{73229044, 650504649, 1120954865, 2626631975}},
            };
        }

        uint32_t *compressedDataUint32 = reinterpret_cast<uint32_t *>(compressedData);
        const int blocksCount          = compressedDataSize / static_cast<int>(sizeof(BC6HBlock));

        // fill data using randomly selected valid blocks
        for (int blockNdx = 0; blockNdx < blocksCount; blockNdx++)
        {
            uint32_t selectedBlock = random.getUint32() % static_cast<uint32_t>(validBlocks.size());
            deMemcpy(compressedDataUint32, validBlocks[selectedBlock].data(), sizeof(BC6HBlock));
            compressedDataUint32 += 4;
        }
    }
    else if (srcFormat != tcu::COMPRESSEDTEXFORMAT_ETC1_RGB8)
    {
        // random initial values cause assertion during the decompression in case of COMPRESSEDTEXFORMAT_ETC1_RGB8 format
        for (int byteNdx = 0; byteNdx < compressedDataSize; byteNdx++)
            compressedData[byteNdx] = 0xFF & random.getUint32();
    }

    // alocate space for decompressed texture
    m_decompressedData.setStorage(decompressedDataSize);
    m_decompressedAccess =
        tcu::PixelBufferAccess(decompressedSrcFormat, width, height, depth, m_decompressedData.getPtr());

    // store decompressed data
    m_compressedTexture.decompress(m_decompressedAccess,
                                   tcu::TexDecompressionParams(tcu::TexDecompressionParams::ASTCMODE_LDR));
}

tcu::PixelBufferAccess CompressedTextureForBlit::getDecompressedAccess() const
{
    return m_decompressedAccess;
}

const tcu::CompressedTexture &CompressedTextureForBlit::getCompressedTexture() const
{
    return m_compressedTexture;
}

// Copy from image to image with scaling.

class BlittingImages : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    BlittingImages(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
    virtual void generateExpectedResult(void);
    void uploadCompressedImage(const VkImage &image, const ImageParms &parms);

private:
    bool checkNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                       const tcu::ConstPixelBufferAccess &clampedReference,
                                       const tcu::ConstPixelBufferAccess &unclampedReference,
                                       const tcu::TextureFormat &sourceFormat);
    bool checkNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                    const tcu::ConstPixelBufferAccess &source);

    bool checkCompressedNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                 const tcu::ConstPixelBufferAccess &clampedReference,
                                                 const tcu::ConstPixelBufferAccess &unclampedReference,
                                                 const tcu::CompressedTexFormat format);
    bool checkCompressedNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                              const tcu::ConstPixelBufferAccess &source,
                                              const tcu::CompressedTexFormat format);

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;

    de::MovePtr<tcu::TextureLevel> m_unclampedExpectedTextureLevel;

    // helper used only when bliting from compressed formats
    typedef de::SharedPtr<CompressedTextureForBlit> CompressedTextureForBlitSp;
    CompressedTextureForBlitSp m_sourceCompressedTexture;
    CompressedTextureForBlitSp m_destinationCompressedTexture;
};

// Helper to ease creating a VkImageSubresourceLayers structure.
VkImageSubresourceLayers makeDefaultSRL(uint32_t baseArrayLayer = 0u, uint32_t layerCount = 1u)
{
    return makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, baseArrayLayer, layerCount);
}

// Helper to create a blit from 3D to a 2D array image.
VkImageBlit make3Dto2DArrayBlit(VkExtent3D srcBaseSize, VkExtent3D dstBaseSize, uint32_t srcBaseSlice,
                                uint32_t dstBaseSlice)
{
    const VkImageBlit blit = {
        makeDefaultSRL(), // src subresource layers.
        {
            // src offsets.
            {0, 0, static_cast<int32_t>(srcBaseSlice)},
            {static_cast<int32_t>(srcBaseSize.width), static_cast<int32_t>(srcBaseSize.height),
             static_cast<int32_t>(srcBaseSlice + 1)},
        },
        makeDefaultSRL(dstBaseSlice, 1), // dst subresource layers
        {
            // dst offsets.
            {0, 0, 0},
            {static_cast<int32_t>(dstBaseSize.width), static_cast<int32_t>(dstBaseSize.height), 1},
        },
    };

    return blit;
}

BlittingImages::BlittingImages(Context &context, TestParams params)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();
    const auto imageUsage               = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto sparseFlags              = (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
    const auto srcCreateFlags = (getCreateFlags(m_params.src.image) | (m_params.useSparseBinding ? sparseFlags : 0u));
    const auto dstCreateFlags = getCreateFlags(m_params.dst.image);

    const VkImageCreateInfo sourceImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        srcCreateFlags,                      // VkImageCreateFlags flags;
        m_params.src.image.imageType,        // VkImageType imageType;
        m_params.src.image.format,           // VkFormat format;
        getExtent3D(m_params.src.image),     // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        getArraySize(m_params.src.image),    // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,               // uint32_t samples;
        m_params.src.image.tiling,           // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const VkImageCreateInfo destinationImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        dstCreateFlags,                      // VkImageCreateFlags flags;
        m_params.dst.image.imageType,        // VkImageType imageType;
        m_params.dst.image.format,           // VkFormat format;
        getExtent3D(m_params.dst.image),     // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        getArraySize(m_params.dst.image),    // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,               // uint32_t samples;
        m_params.dst.image.tiling,           // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create source image
    {
#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }

            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination image
    {
        m_destination           = createImage(vk, vkDevice, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any,
                                                memAlloc, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus BlittingImages::iterate(void)
{
    const DeviceInterface &vk        = m_context.getDeviceInterface();
    const VkDevice vkDevice          = m_device;
    const ImageParms &srcImageParams = m_params.src.image;
    const int srcWidth               = static_cast<int>(srcImageParams.extent.width);
    const int srcHeight              = static_cast<int>(srcImageParams.extent.height);
    const int srcDepth               = static_cast<int>(srcImageParams.extent.depth);
    const ImageParms &dstImageParams = m_params.dst.image;
    const int dstWidth               = static_cast<int>(dstImageParams.extent.width);
    const int dstHeight              = static_cast<int>(dstImageParams.extent.height);
    const int dstDepth               = static_cast<int>(dstImageParams.extent.depth);

    std::vector<VkImageBlit> regions;
    std::vector<VkImageBlit2KHR> regions2KHR;

    // When using maximum slices, we'll generate the copy region on the fly. This is because we don't know, at test
    // creation time, the exact size of the images.
    std::vector<CopyRegion> generatedRegions;

    // setup blit regions - they are also needed for reference generation
    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        regions.reserve(m_params.regions.size());
        for (const auto &r : m_params.regions)
            regions.push_back(r.imageBlit);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        regions2KHR.reserve(m_params.regions.size());
        for (const auto &r : m_params.regions)
            regions2KHR.push_back(convertvkImageBlitTovkImageBlit2KHR(r.imageBlit));
    }

    // generate source image
    if (isCompressedFormat(srcImageParams.format))
    {
        // for compressed images srcImageParams.fillMode is not used - we are using random data
        tcu::CompressedTexFormat compressedFormat = mapVkCompressedFormat(srcImageParams.format);
        m_sourceCompressedTexture =
            CompressedTextureForBlitSp(new CompressedTextureForBlit(compressedFormat, srcWidth, srcHeight, srcDepth));
        uploadCompressedImage(m_source.get(), srcImageParams);
    }
    else
    {
        // non-compressed image is filled with selected fillMode
        const tcu::TextureFormat srcTcuFormat = mapVkFormat(srcImageParams.format);
        m_sourceTextureLevel =
            de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat, srcWidth, srcHeight, srcDepth));
        generateBuffer(m_sourceTextureLevel->getAccess(), srcWidth, srcHeight, srcDepth, srcImageParams.fillMode);
        uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), srcImageParams, m_params.useGeneralLayout);
    }

    // generate destination image
    if (isCompressedFormat(dstImageParams.format))
    {
        // compressed images are filled with random data
        tcu::CompressedTexFormat compressedFormat = mapVkCompressedFormat(dstImageParams.format);
        m_destinationCompressedTexture =
            CompressedTextureForBlitSp(new CompressedTextureForBlit(compressedFormat, srcWidth, srcHeight, srcDepth));
        uploadCompressedImage(m_destination.get(), dstImageParams);
    }
    else
    {
        // non-compressed image is filled with white background
        const tcu::TextureFormat dstTcuFormat = mapVkFormat(dstImageParams.format);
        m_destinationTextureLevel =
            de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat, dstWidth, dstHeight, dstDepth));
        generateBuffer(m_destinationTextureLevel->getAccess(), dstWidth, dstHeight, dstDepth, dstImageParams.fillMode);
        uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), dstImageParams,
                    m_params.useGeneralLayout);
    }

    generateExpectedResult();

    // Barriers for copying images to buffer
    const VkImageMemoryBarrier imageBarriers[]{
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         nullptr,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         srcImageParams.operationLayout,         // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_source.get(),                         // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcImageParams.format), //   VkImageAspectFlags aspectMask;
             0u,                                    //   uint32_t baseMipLevel;
             1u,                                    //   uint32_t mipLevels;
             0u,                                    //   uint32_t baseArraySlice;
             getArraySize(m_params.src.image)       //   uint32_t arraySize;
         }},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         nullptr,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         dstImageParams.operationLayout,         // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_destination.get(),                    // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstImageParams.format), //   VkImageAspectFlags aspectMask;
             0u,                                    //   uint32_t baseMipLevel;
             1u,                                    //   uint32_t mipLevels;
             0u,                                    //   uint32_t baseArraySlice;
             getArraySize(m_params.dst.image)       //   uint32_t arraySize;
         }}};

    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 2, imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdBlitImage(*m_universalCmdBuffer, m_source.get(), srcImageParams.operationLayout, m_destination.get(),
                        dstImageParams.operationLayout, de::sizeU32(regions), de::dataOrNull(regions), m_params.filter);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkBlitImageInfo2KHR blitImageInfo2KHR{
            VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            m_source.get(),                          // VkImage srcImage;
            srcImageParams.operationLayout,          // VkImageLayout srcImageLayout;
            m_destination.get(),                     // VkImage dstImage;
            dstImageParams.operationLayout,          // VkImageLayout dstImageLayout;
            de::sizeU32(regions2KHR),                // uint32_t regionCount;
            de::dataOrNull(regions2KHR),             // const VkImageBlit2KHR* pRegions;
            m_params.filter,                         // VkFilter filter;
        };
        vk.cmdBlitImage2(*m_universalCmdBuffer, &blitImageInfo2KHR);
    }

    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, dstImageParams);
    tcu::PixelBufferAccess resultAccess        = resultLevel->getAccess();

    // if blit was done to a compressed format we need to decompress it to be able to verify it
    if (m_destinationCompressedTexture)
    {
        uint8_t *const compressedDataSrc(static_cast<uint8_t *>(resultAccess.getDataPtr()));
        const tcu::CompressedTexFormat dstCompressedFormat(mapVkCompressedFormat(dstImageParams.format));
        tcu::TextureLevel decompressedLevel(getUncompressedFormat(dstCompressedFormat), dstWidth, dstHeight, dstDepth);
        tcu::PixelBufferAccess decompressedAccess(decompressedLevel.getAccess());

        tcu::decompress(decompressedAccess, dstCompressedFormat, compressedDataSrc);

        return checkTestResult(decompressedAccess);
    }

    return checkTestResult(resultAccess);
}

bool BlittingImages::checkNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                   const tcu::ConstPixelBufferAccess &clampedExpected,
                                                   const tcu::ConstPixelBufferAccess &unclampedExpected,
                                                   const tcu::TextureFormat &srcFormat)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    const tcu::TextureFormat dstFormat             = result.getFormat();
    const tcu::TextureChannelClass dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
    const tcu::TextureChannelClass srcChannelClass = tcu::getTextureChannelClass(srcFormat.type);
    bool isOk                                      = false;

    log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

    // if either of srcImage or dstImage stores values as a signed/unsigned integer,
    // the other must also store values a signed/unsigned integer
    // e.g. blit unorm to uscaled is not allowed as uscaled formats store data as integers
    // despite the fact that both formats are sampled as floats
    bool dstImageIsIntClass = dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    bool srcImageIsIntClass = srcChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              srcChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    if (dstImageIsIntClass != srcImageIsIntClass)
    {
        log << tcu::TestLog::EndSection;
        return false;
    }

    if (isFloatFormat(dstFormat))
    {
        const bool srcIsSRGB       = tcu::isSRGB(srcFormat);
        const tcu::Vec4 srcMaxDiff = getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
        const tcu::Vec4 dstMaxDiff = getFormatThreshold(dstFormat);
        const tcu::Vec4 threshold =
            (srcMaxDiff + dstMaxDiff) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f);

        isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold,
                                          tcu::COMPARE_LOG_ON_ERROR);
        log << tcu::TestLog::EndSection;

        if (!isOk)
        {
            log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
            isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold,
                                              tcu::COMPARE_LOG_ON_ERROR);
            log << tcu::TestLog::EndSection;
        }
    }
    else
    {
        tcu::UVec4 threshold;
        // Calculate threshold depending on channel width of destination format.
        const tcu::IVec4 dstBitDepth = tcu::getTextureFormatBitDepth(dstFormat);
        const tcu::IVec4 srcBitDepth = tcu::getTextureFormatBitDepth(srcFormat);
        for (uint32_t i = 0; i < 4; ++i)
        {
            DE_ASSERT(dstBitDepth[i] < std::numeric_limits<uint64_t>::digits);
            DE_ASSERT(srcBitDepth[i] < std::numeric_limits<uint64_t>::digits);
            uint64_t threshold64 =
                1 + de::max(((UINT64_C(1) << dstBitDepth[i]) - 1) /
                                de::clamp((UINT64_C(1) << srcBitDepth[i]) - 1, UINT64_C(1), UINT64_C(256)),
                            UINT64_C(1));
            DE_ASSERT(threshold64 <= std::numeric_limits<uint32_t>::max());
            threshold[i] = static_cast<uint32_t>(threshold64);
        }

        isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", clampedExpected, result, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR);
        log << tcu::TestLog::EndSection;

        if (!isOk)
        {
            log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
            isOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedExpected, result, threshold,
                                            tcu::COMPARE_LOG_ON_ERROR);
            log << tcu::TestLog::EndSection;
        }
    }

    return isOk;
}

bool BlittingImages::checkCompressedNonNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                             const tcu::ConstPixelBufferAccess &clampedReference,
                                                             const tcu::ConstPixelBufferAccess &unclampedReference,
                                                             const tcu::CompressedTexFormat format)
{
    tcu::TestLog &log                  = m_context.getTestContext().getLog();
    const tcu::TextureFormat dstFormat = result.getFormat();

    // there are rare cases wher one or few pixels have slightly bigger error
    // in one of channels this accepted error allows those casses to pass
    const tcu::Vec4 acceptedError(0.06f);

    const tcu::Vec4 srcMaxDiff = getCompressedFormatThreshold(format);
    const tcu::Vec4 dstMaxDiff =
        m_destinationCompressedTexture ?
            getCompressedFormatThreshold(m_destinationCompressedTexture->getCompressedTexture().getFormat()) :
            getFormatThreshold(dstFormat);
    const tcu::Vec4 threshold =
        (srcMaxDiff + dstMaxDiff) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f) + acceptedError;

    bool filteredResultVerification(false);
    tcu::Vec4 filteredResultMinValue(-6e6);
    tcu::Vec4 filteredResultMaxValue(6e6);
    tcu::TextureLevel filteredResult;
    tcu::TextureLevel filteredClampedReference;
    tcu::TextureLevel filteredUnclampedReference;

    if (((format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK) ||
         (format == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK)))
    {
        if ((dstFormat.type == tcu::TextureFormat::FLOAT) || (dstFormat.type == tcu::TextureFormat::HALF_FLOAT))
        {
            // for compressed formats we are using random data and for bc6h formats
            // this will give us also large color values; when we are bliting to
            // a format that accepts large values we can end up with large diferences
            // betwean filtered result and reference; to avoid that we need to remove
            // values that are to big from verification
            filteredResultVerification = true;
            filteredResultMinValue     = tcu::Vec4(-10.0f);
            filteredResultMaxValue     = tcu::Vec4(10.0f);
        }
        else if (dstFormat.type == tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV)
        {
            // we need to clamp some formats to <0;1> range as it has
            // small precision for big numbers compared to reference
            filteredResultVerification = true;
            filteredResultMinValue     = tcu::Vec4(0.0f);
            filteredResultMaxValue     = tcu::Vec4(1.0f);
        }
        // else don't use filtered verification
    }

    if (filteredResultVerification)
    {
        filteredResult.setStorage(dstFormat, result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess filteredResultAcccess(filteredResult.getAccess());

        filteredClampedReference.setStorage(dstFormat, result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess filteredClampedAcccess(filteredClampedReference.getAccess());

        filteredUnclampedReference.setStorage(dstFormat, result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess filteredUnclampedResultAcccess(filteredUnclampedReference.getAccess());

        for (int32_t z = 0; z < result.getDepth(); z++)
            for (int32_t y = 0; y < result.getHeight(); y++)
                for (int32_t x = 0; x < result.getWidth(); x++)
                {
                    tcu::Vec4 resultTexel             = result.getPixel(x, y, z);
                    tcu::Vec4 clampedReferenceTexel   = clampedReference.getPixel(x, y, z);
                    tcu::Vec4 unclampedReferenceTexel = unclampedReference.getPixel(x, y, z);

                    resultTexel = tcu::clamp(resultTexel, filteredResultMinValue, filteredResultMaxValue);
                    clampedReferenceTexel =
                        tcu::clamp(clampedReferenceTexel, filteredResultMinValue, filteredResultMaxValue);
                    unclampedReferenceTexel =
                        tcu::clamp(unclampedReferenceTexel, filteredResultMinValue, filteredResultMaxValue);

                    filteredResultAcccess.setPixel(resultTexel, x, y, z);
                    filteredClampedAcccess.setPixel(clampedReferenceTexel, x, y, z);
                    filteredUnclampedResultAcccess.setPixel(unclampedReferenceTexel, x, y, z);
                }
    }

    const tcu::ConstPixelBufferAccess clampedRef =
        filteredResultVerification ? filteredClampedReference.getAccess() : clampedReference;
    const tcu::ConstPixelBufferAccess res = filteredResultVerification ? filteredResult.getAccess() : result;

    log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");
    bool isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedRef, res, threshold,
                                           tcu::COMPARE_LOG_ON_ERROR);
    log << tcu::TestLog::EndSection;

    if (!isOk)
    {
        const tcu::ConstPixelBufferAccess unclampedRef =
            filteredResultVerification ? filteredUnclampedReference.getAccess() : unclampedReference;

        log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
        isOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedRef, res, threshold,
                                          tcu::COMPARE_LOG_ON_ERROR);
        log << tcu::TestLog::EndSection;
    }

    return isOk;
}

bool BlittingImages::checkNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                const tcu::ConstPixelBufferAccess &source)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    const tcu::TextureFormat dstFormat             = result.getFormat();
    const tcu::TextureFormat srcFormat             = source.getFormat();
    const tcu::TextureChannelClass dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
    const tcu::TextureChannelClass srcChannelClass = tcu::getTextureChannelClass(srcFormat.type);

    tcu::TextureLevel errorMaskStorage(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
                                       result.getWidth(), result.getHeight(), result.getDepth());
    tcu::PixelBufferAccess errorMask = errorMaskStorage.getAccess();
    tcu::Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    tcu::Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);
    bool ok = false;

    tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

    // if either of srcImage or dstImage stores values as a signed/unsigned integer,
    // the other must also store values a signed/unsigned integer
    // e.g. blit unorm to uscaled is not allowed as uscaled formats store data as integers
    // despite the fact that both formats are sampled as floats
    bool dstImageIsIntClass = dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    bool srcImageIsIntClass = srcChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
                              srcChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    if (dstImageIsIntClass != srcImageIsIntClass)
        return false;

    if (dstImageIsIntClass)
    {
        ok = intNearestBlitCompare(source, result, errorMask, m_params);
    }
    else
    {
        const tcu::Vec4 srcMaxDiff = getFloatOrFixedPointFormatThreshold(source.getFormat());
        const tcu::Vec4 dstMaxDiff = getFloatOrFixedPointFormatThreshold(result.getFormat());
        ok = floatNearestBlitCompare(source, result, srcMaxDiff, dstMaxDiff, errorMask, m_params);
    }

    if (result.getFormat() != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
        tcu::computePixelScaleBias(result, pixelScale, pixelBias);

    if (!ok)
    {
        log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
            << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
            << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    }
    else
    {
        log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
            << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << tcu::TestLog::EndImageSet;
    }

    return ok;
}

bool BlittingImages::checkCompressedNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                                          const tcu::ConstPixelBufferAccess &source,
                                                          const tcu::CompressedTexFormat format)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    tcu::TextureFormat errorMaskFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8);
    tcu::TextureLevel errorMaskStorage(errorMaskFormat, result.getWidth(), result.getHeight(), result.getDepth());
    tcu::PixelBufferAccess errorMask(errorMaskStorage.getAccess());
    tcu::Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
    tcu::Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);
    const tcu::TextureFormat &resultFormat(result.getFormat());
    VkFormat nativeResultFormat(mapTextureFormat(resultFormat));

    // there are rare cases wher one or few pixels have slightly bigger error
    // in one of channels this accepted error allows those casses to pass
    const tcu::Vec4 acceptedError(0.04f);
    const tcu::Vec4 srcMaxDiff(acceptedError + getCompressedFormatThreshold(format));
    const tcu::Vec4 dstMaxDiff(
        acceptedError +
        (m_destinationCompressedTexture ?
             getCompressedFormatThreshold(m_destinationCompressedTexture->getCompressedTexture().getFormat()) :
             getFloatOrFixedPointFormatThreshold(resultFormat)));

    tcu::TextureLevel clampedSourceLevel;
    bool clampSource(false);
    tcu::Vec4 clampSourceMinValue(-1.0f);
    tcu::Vec4 clampSourceMaxValue(1.0f);
    tcu::TextureLevel clampedResultLevel;
    bool clampResult(false);

    tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

    if (resultFormat != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
        tcu::computePixelScaleBias(result, pixelScale, pixelBias);

    log << tcu::TestLog::ImageSet("Compare", "Result comparsion")
        << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias);

    // for compressed formats source buffer access is not actual compressed format
    // but equivalent uncompressed format that is some cases needs additional
    // modifications so that sampling it will produce valid reference
    if ((format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK) ||
        (format == tcu::COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK))
    {
        if (resultFormat.type == tcu::TextureFormat::UNSIGNED_INT_11F_11F_10F_REV)
        {
            // for compressed formats we are using random data and for some formats it
            // can be outside of <-1;1> range - for cases where result is not a float
            // format we need to clamp source to <-1;1> range as this will be done on
            // the device but not in software sampler in framework
            clampSource = true;
            // for this format we also need to clamp the result as precision of
            // this format is smaller then precision of calculations in framework;
            // the biger color valus are the bigger errors can be
            clampResult = true;

            if (format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK)
                clampSourceMinValue = tcu::Vec4(0.0f);
        }
        else if ((resultFormat.type != tcu::TextureFormat::FLOAT) &&
                 (resultFormat.type != tcu::TextureFormat::HALF_FLOAT))
        {
            // clamp source for all non float formats
            clampSource = true;
        }
    }

    if (isUnormFormat(nativeResultFormat) || isUfloatFormat(nativeResultFormat))
    {
        // when tested compressed format is signed but the result format
        // is unsigned we need to clamp source to <0; x> so that proper
        // reference is calculated
        if ((format == tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11) ||
            (format == tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11) ||
            (format == tcu::COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK) ||
            (format == tcu::COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK) ||
            (format == tcu::COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK))
        {
            clampSource         = true;
            clampSourceMinValue = tcu::Vec4(0.0f);
        }
    }

    if (clampSource || clampResult)
    {
        if (clampSource)
        {
            clampedSourceLevel.setStorage(source.getFormat(), source.getWidth(), source.getHeight(), source.getDepth());
            tcu::PixelBufferAccess clampedSourceAcccess(clampedSourceLevel.getAccess());

            for (int32_t z = 0; z < source.getDepth(); z++)
                for (int32_t y = 0; y < source.getHeight(); y++)
                    for (int32_t x = 0; x < source.getWidth(); x++)
                    {
                        tcu::Vec4 texel = source.getPixel(x, y, z);
                        texel = tcu::clamp(texel, tcu::Vec4(clampSourceMinValue), tcu::Vec4(clampSourceMaxValue));
                        clampedSourceAcccess.setPixel(texel, x, y, z);
                    }
        }

        if (clampResult)
        {
            clampedResultLevel.setStorage(result.getFormat(), result.getWidth(), result.getHeight(), result.getDepth());
            tcu::PixelBufferAccess clampedResultAcccess(clampedResultLevel.getAccess());

            for (int32_t z = 0; z < result.getDepth(); z++)
                for (int32_t y = 0; y < result.getHeight(); y++)
                    for (int32_t x = 0; x < result.getWidth(); x++)
                    {
                        tcu::Vec4 texel = result.getPixel(x, y, z);
                        texel           = tcu::clamp(texel, tcu::Vec4(-1.0f), tcu::Vec4(1.0f));
                        clampedResultAcccess.setPixel(texel, x, y, z);
                    }
        }
    }

    const tcu::ConstPixelBufferAccess src = clampSource ? clampedSourceLevel.getAccess() : source;
    const tcu::ConstPixelBufferAccess res = clampResult ? clampedResultLevel.getAccess() : result;

    if (floatNearestBlitCompare(src, res, srcMaxDiff, dstMaxDiff, errorMask, m_params))
    {
        log << tcu::TestLog::EndImageSet;
        return true;
    }

    log << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
    return false;
}

struct SlicedImageLogGuard
{
    SlicedImageLogGuard(tcu::TestLog &log) : m_log(log), m_origValue(log.isSeparateSlices())
    {
        m_log.separateSlices(true);
    }

    ~SlicedImageLogGuard()
    {
        m_log.separateSlices(m_origValue);
    }

private:
    tcu::TestLog &m_log;
    bool m_origValue;
};

tcu::TestStatus BlittingImages::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR ||
              m_params.filter == VK_FILTER_CUBIC_EXT);
    const std::string failMessage("Result image is incorrect");

    SlicedImageLogGuard slicedImageLogGuard(m_context.getTestContext().getLog());

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        if (tcu::isCombinedDepthStencilType(result.getFormat().type))
        {
            if (tcu::hasDepthComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode     = tcu::Sampler::MODE_DEPTH;
                const tcu::ConstPixelBufferAccess depthResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess clampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
                const tcu::ConstPixelBufferAccess unclampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
                const tcu::TextureFormat sourceFormat =
                    tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

                if (!checkNonNearestFilteredResult(depthResult, clampedExpected, unclampedExpected, sourceFormat))
                    return tcu::TestStatus::fail(failMessage);
            }

            if (tcu::hasStencilComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode       = tcu::Sampler::MODE_STENCIL;
                const tcu::ConstPixelBufferAccess stencilResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess clampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);
                const tcu::ConstPixelBufferAccess unclampedExpected =
                    tcu::getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel->getAccess(), mode);
                const tcu::TextureFormat sourceFormat =
                    tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode);

                if (!checkNonNearestFilteredResult(stencilResult, clampedExpected, unclampedExpected, sourceFormat))
                    return tcu::TestStatus::fail(failMessage);
            }
        }
        else if (m_sourceCompressedTexture)
        {
            const tcu::CompressedTexture &compressedLevel = m_sourceCompressedTexture->getCompressedTexture();
            if (!checkCompressedNonNearestFilteredResult(result, m_expectedTextureLevel[0]->getAccess(),
                                                         m_unclampedExpectedTextureLevel->getAccess(),
                                                         compressedLevel.getFormat()))
                return tcu::TestStatus::fail(failMessage);
        }
        else
        {
            const tcu::TextureFormat sourceFormat = mapVkFormat(m_params.src.image.format);
            if (!checkNonNearestFilteredResult(result, m_expectedTextureLevel[0]->getAccess(),
                                               m_unclampedExpectedTextureLevel->getAccess(), sourceFormat))
                return tcu::TestStatus::fail(failMessage);
        }
    }
    else // NEAREST filtering
    {
        if (tcu::isCombinedDepthStencilType(result.getFormat().type))
        {
            if (tcu::hasDepthComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode     = tcu::Sampler::MODE_DEPTH;
                const tcu::ConstPixelBufferAccess depthResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess depthSource =
                    tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode);

                if (!checkNearestFilteredResult(depthResult, depthSource))
                    return tcu::TestStatus::fail(failMessage);
            }

            if (tcu::hasStencilComponent(result.getFormat().order))
            {
                const tcu::Sampler::DepthStencilMode mode       = tcu::Sampler::MODE_STENCIL;
                const tcu::ConstPixelBufferAccess stencilResult = tcu::getEffectiveDepthStencilAccess(result, mode);
                const tcu::ConstPixelBufferAccess stencilSource =
                    tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode);

                if (!checkNearestFilteredResult(stencilResult, stencilSource))
                    return tcu::TestStatus::fail(failMessage);
            }
        }
        else if (m_sourceCompressedTexture)
        {
            const tcu::CompressedTexture &compressedLevel   = m_sourceCompressedTexture->getCompressedTexture();
            const tcu::PixelBufferAccess &decompressedLevel = m_sourceCompressedTexture->getDecompressedAccess();

            if (!checkCompressedNearestFilteredResult(result, decompressedLevel, compressedLevel.getFormat()))
                return tcu::TestStatus::fail(failMessage);
        }
        else if (!checkNearestFilteredResult(result, m_sourceTextureLevel->getAccess()))
            return tcu::TestStatus::fail(failMessage);
    }

    return tcu::TestStatus::pass("Pass");
}

void BlittingImages::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                              CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    const MirrorMode mirrorMode = getMirrorMode(region.imageBlit.srcOffsets[0], region.imageBlit.srcOffsets[1],
                                                region.imageBlit.dstOffsets[0], region.imageBlit.dstOffsets[1]);

    flipCoordinates(region, mirrorMode);

    const VkOffset3D srcOffset = region.imageBlit.srcOffsets[0];
    const VkOffset3D srcExtent = {
        region.imageBlit.srcOffsets[1].x - srcOffset.x,
        region.imageBlit.srcOffsets[1].y - srcOffset.y,
        region.imageBlit.srcOffsets[1].z - srcOffset.z,
    };

    VkOffset3D dstOffset = region.imageBlit.dstOffsets[0];

    VkOffset3D dstExtent = {
        region.imageBlit.dstOffsets[1].x - dstOffset.x,
        region.imageBlit.dstOffsets[1].y - dstOffset.y,
        region.imageBlit.dstOffsets[1].z - dstOffset.z,
    };

    if (m_params.dst.image.imageType == VK_IMAGE_TYPE_2D)
    {
        // Without taking layers into account.
        DE_ASSERT(dstOffset.z == 0u && dstExtent.z == 1);

        // Modify offset and extent taking layers into account. This is used for the 3D-to-2D_ARRAY case.
        dstOffset.z += region.imageBlit.dstSubresource.baseArrayLayer;
        dstExtent.z = region.imageBlit.dstSubresource.layerCount;
    }

    tcu::Sampler::FilterMode filter;
    switch (m_params.filter)
    {
    case VK_FILTER_LINEAR:
        filter = tcu::Sampler::LINEAR;
        break;
    case VK_FILTER_CUBIC_EXT:
        filter = tcu::Sampler::CUBIC;
        break;
    case VK_FILTER_NEAREST:
    default:
        filter = tcu::Sampler::NEAREST;
        break;
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());

        // Scale depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z),
                tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                tcu::Sampler::MODE_DEPTH);
            tcu::scale(dstSubRegion, srcSubRegion, filter);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess depthSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y,
                                      dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                    tcu::Sampler::MODE_DEPTH);
                scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter, mirrorMode);
            }
        }

        // Scale stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z),
                tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                tcu::Sampler::MODE_STENCIL);
            blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess stencilSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y,
                                      dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z),
                    tcu::Sampler::MODE_STENCIL);
                scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter, mirrorMode);
            }
        }
    }
    else
    {
        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z);
        const tcu::PixelBufferAccess dstSubRegion =
            tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z);
        blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

        if (filter != tcu::Sampler::NEAREST)
        {
            const tcu::PixelBufferAccess unclampedSubRegion =
                tcu::getSubregion(m_unclampedExpectedTextureLevel->getAccess(), dstOffset.x, dstOffset.y, dstOffset.z,
                                  dstExtent.x, dstExtent.y, dstExtent.z);
            scaleFromWholeSrcBuffer(unclampedSubRegion, src, srcOffset, srcExtent, filter, mirrorMode);
        }
    }
}

void BlittingImages::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess src = m_sourceCompressedTexture ?
                                                m_sourceCompressedTexture->getDecompressedAccess() :
                                                m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationCompressedTexture ?
                                                m_destinationCompressedTexture->getDecompressedAccess() :
                                                m_destinationTextureLevel->getAccess();

    m_expectedTextureLevel[0] = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
    tcu::copy(m_expectedTextureLevel[0]->getAccess(), dst);

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        m_unclampedExpectedTextureLevel = de::MovePtr<tcu::TextureLevel>(
            new tcu::TextureLevel(dst.getFormat(), dst.getWidth(), dst.getHeight(), dst.getDepth()));
        tcu::copy(m_unclampedExpectedTextureLevel->getAccess(), dst);
    }

    for (uint32_t i = 0; i < de::sizeU32(m_params.regions); i++)
    {
        CopyRegion region = m_params.regions[i];
        copyRegionToTextureLevel(src, m_expectedTextureLevel[0]->getAccess(), region);
    }
}

void BlittingImages::uploadCompressedImage(const VkImage &image, const ImageParms &parms)
{
    DE_ASSERT(m_sourceCompressedTexture);

    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = *m_allocator;
    Move<VkBuffer> buffer;
    const uint32_t bufferSize = m_sourceCompressedTexture->getCompressedTexture().getDataSize();
    de::MovePtr<Allocation> bufferAlloc;
    const uint32_t arraySize = getArraySize(parms);
    const VkExtent3D imageExtent{
        parms.extent.width,
        (parms.imageType != VK_IMAGE_TYPE_1D) ? parms.extent.height : 1u,
        (parms.imageType == VK_IMAGE_TYPE_3D) ? parms.extent.depth : 1u,
    };

    // Create source buffer
    {
        const VkBufferCreateInfo bufferParams{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        buffer      = createBuffer(vk, vkDevice, &bufferParams);
        bufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *buffer, MemoryRequirement::HostVisible, memAlloc,
                                     m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
    }

    // Barriers for copying buffer to image
    const VkBufferMemoryBarrier preBufferBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_HOST_WRITE_BIT,                // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *buffer,                                 // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        bufferSize                               // VkDeviceSize size;
    };

    const VkImageMemoryBarrier preImageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               nullptr,                                // const void* pNext;
                                               0u,                                     // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               image,                                  // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                                                   0u,                        // uint32_t baseMipLevel;
                                                   1u,                        // uint32_t mipLevels;
                                                   0u,                        // uint32_t baseArraySlice;
                                                   arraySize,                 // uint32_t arraySize;
                                               }};

    const VkImageMemoryBarrier postImageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                                nullptr,                                // const void* pNext;
                                                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                                image,                                  // VkImage image;
                                                {
                                                    // VkImageSubresourceRange subresourceRange;
                                                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
                                                    0u,                        // uint32_t baseMipLevel;
                                                    1u,                        // uint32_t mipLevels;
                                                    0u,                        // uint32_t baseArraySlice;
                                                    arraySize,                 // uint32_t arraySize;
                                                }};

    const VkExtent3D copyExtent{imageExtent.width, imageExtent.height, imageExtent.depth};

    VkBufferImageCopy copyRegion{
        0u,                // VkDeviceSize bufferOffset;
        copyExtent.width,  // uint32_t bufferRowLength;
        copyExtent.height, // uint32_t bufferImageHeight;
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            arraySize,                 // uint32_t layerCount;
        },                             // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                     // VkOffset3D imageOffset;
        copyExtent                     // VkExtent3D imageExtent;
    };

    // Write buffer data
    deMemcpy(bufferAlloc->getHostPtr(), m_sourceCompressedTexture->getCompressedTexture().getData(), bufferSize);
    flushAlloc(vk, vkDevice, *bufferAlloc);

    // Copy buffer to image
    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &preBufferBarrier, 1, &preImageBarrier);
    vk.cmdCopyBufferToImage(*m_universalCmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                            &copyRegion);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
}

class BlitImageTestCase : public vkt::TestCase
{
public:
    BlitImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new BlittingImages(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {

#ifndef CTS_USES_VULKANSC
        if (m_params.src.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.dst.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.src.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR ||
            m_params.dst.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
            context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

        VkImageFormatProperties properties;
        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                m_params.src.image.tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Source format not supported");
        }
        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                m_params.dst.image.tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Destination format not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        VkFormatProperties srcFormatProperties;
        context.getInstanceInterface().getPhysicalDeviceFormatProperties(
            context.getPhysicalDevice(), m_params.src.image.format, &srcFormatProperties);
        VkFormatFeatureFlags srcFormatFeatures = m_params.src.image.tiling == VK_IMAGE_TILING_LINEAR ?
                                                     srcFormatProperties.linearTilingFeatures :
                                                     srcFormatProperties.optimalTilingFeatures;
        if (!(srcFormatFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit source not supported");
        }

        VkFormatProperties dstFormatProperties;
        context.getInstanceInterface().getPhysicalDeviceFormatProperties(
            context.getPhysicalDevice(), m_params.dst.image.format, &dstFormatProperties);
        VkFormatFeatureFlags dstFormatFeatures = m_params.dst.image.tiling == VK_IMAGE_TILING_LINEAR ?
                                                     dstFormatProperties.linearTilingFeatures :
                                                     dstFormatProperties.optimalTilingFeatures;
        if (!(dstFormatFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
        }

        if (m_params.filter == VK_FILTER_LINEAR &&
            !(srcFormatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
        {
            TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");
        }

        if (m_params.filter == VK_FILTER_CUBIC_EXT)
        {
            context.requireDeviceFunctionality("VK_EXT_filter_cubic");

            if (!(srcFormatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))
            {
                TCU_THROW(NotSupportedError, "Source format feature sampled image filter cubic not supported");
            }
        }

        checkExtensionSupport(context, m_params.extensionFlags);
    }

private:
    TestParams m_params;
};

class BlittingMipmaps : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    BlittingMipmaps(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess());
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
    virtual void generateExpectedResult(void);

private:
    bool checkNonNearestFilteredResult(void);
    bool checkNearestFilteredResult(void);

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;

    de::MovePtr<tcu::TextureLevel> m_unclampedExpectedTextureLevel[16];
};

BlittingMipmaps::BlittingMipmaps(Context &context, TestParams params)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            nullptr,                                                           // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!params.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(),
                                        m_sourceImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            sourceImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(vkPhysDevice, sourceImageParams.format,
                                                           sourceImageParams.imageType, sourceImageParams.tiling,
                                                           sourceImageParams.usage, sourceImageParams.flags,
                                                           &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_source = createImage(
                vk, m_device,
                &sourceImageParams); //de::MovePtr<SparseImage>(new SparseImage(vk, vk, vkPhysDevice, vki, sourceImageParams, m_queue, *m_allocator, mapVkFormat(sourceImageParams.format)));
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    // Create destination image
    {
        const VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            m_params.mipLevels,                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            nullptr,                                                           // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, vkDevice, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_destination, MemoryRequirement::Any,
                                                memAlloc, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus BlittingMipmaps::iterate(void)
{
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(m_params.dst.image.format);
    m_sourceTextureLevel                  = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, m_params.src.image.extent.width, m_params.src.image.extent.height,
                                               m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.dst.image.fillMode);
    generateExpectedResult();

    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image, m_params.useGeneralLayout);

    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image,
                m_params.useGeneralLayout, m_params.mipLevels);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_device;

    std::vector<VkImageBlit> regions;
    std::vector<VkImageBlit2KHR> regions2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            regions.push_back(m_params.regions[i].imageBlit);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            regions2KHR.push_back(convertvkImageBlitTovkImageBlit2KHR(m_params.regions[i].imageBlit));
        }
    }

    // Copy source image to mip level 0 when generating mipmaps with multiple blit commands
    if (!m_params.singleCommand)
        uploadImage(m_sourceTextureLevel->getAccess(), m_destination.get(), m_params.dst.image,
                    m_params.useGeneralLayout, 1u);

    beginCommandBuffer(vk, *m_universalCmdBuffer);

    // Blit all mip levels with a single blit command
    if (m_params.singleCommand)
    {
        {
            // Source image layout
            const VkImageMemoryBarrier srcImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                m_params.src.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_source.get(),                         // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(srcTcuFormat),    // VkImageAspectFlags   aspectMask;
                    0u,                              // uint32_t baseMipLevel;
                    1u,                              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.src.image) // uint32_t arraySize;
                }};

            // Destination image layout
            const VkImageMemoryBarrier dstImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags   aspectMask;
                    0u,                              // uint32_t baseMipLevel;
                    m_params.mipLevels,              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.dst.image) // uint32_t arraySize;
                }};

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &srcImageBarrier);
            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &dstImageBarrier);

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdBlitImage(*m_universalCmdBuffer, m_source.get(), m_params.src.image.operationLayout,
                                m_destination.get(), m_params.dst.image.operationLayout,
                                (uint32_t)m_params.regions.size(), &regions[0], m_params.filter);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkBlitImageInfo2KHR BlitImageInfo2KHR = {
                    VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
                    nullptr,                                 // const void* pNext;
                    m_source.get(),                          // VkImage srcImage;
                    m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                    m_destination.get(),                     // VkImage dstImage;
                    m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
                    (uint32_t)m_params.regions.size(),       // uint32_t regionCount;
                    &regions2KHR[0],                         // const VkImageBlit2KHR* pRegions;
                    m_params.filter                          // VkFilter filter;
                };
                vk.cmdBlitImage2(*m_universalCmdBuffer, &BlitImageInfo2KHR);
            }
        }
    }
    // Blit mip levels with multiple blit commands
    else
    {
        // Prepare all mip levels for reading
        {
            for (uint32_t barrierno = 0; barrierno < m_params.barrierCount; barrierno++)
            {
                VkImageMemoryBarrier preImageBarrier = {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                    nullptr,                                // const void* pNext;
                    VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                    VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                    m_params.src.image.operationLayout,     // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                    m_destination.get(),                    // VkImage image;
                    {
                        // VkImageSubresourceRange subresourceRange;
                        getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                        0u,                              // uint32_t baseMipLevel;
                        VK_REMAINING_MIP_LEVELS,         // uint32_t mipLevels;
                        0u,                              // uint32_t baseArraySlice;
                        getArraySize(m_params.src.image) // uint32_t arraySize;
                    }};

                if (getArraySize(m_params.src.image) == 1)
                {
                    DE_ASSERT(barrierno < m_params.mipLevels);
                    preImageBarrier.subresourceRange.baseMipLevel = barrierno;
                    preImageBarrier.subresourceRange.levelCount =
                        (barrierno + 1 < m_params.barrierCount) ? 1 : VK_REMAINING_MIP_LEVELS;
                }
                else
                {
                    preImageBarrier.subresourceRange.baseArrayLayer = barrierno;
                    preImageBarrier.subresourceRange.layerCount =
                        (barrierno + 1 < m_params.barrierCount) ? 1 : VK_REMAINING_ARRAY_LAYERS;
                }
                vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                                      &preImageBarrier);
            }
        }

        for (uint32_t regionNdx = 0u; regionNdx < (uint32_t)m_params.regions.size(); regionNdx++)
        {
            const uint32_t mipLevel = m_params.regions[regionNdx].imageBlit.dstSubresource.mipLevel;

            // Prepare single mip level for writing
            const VkImageMemoryBarrier preImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                m_params.src.image.operationLayout,     // VkImageLayout oldLayout;
                m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                    mipLevel,                        // uint32_t baseMipLevel;
                    1u,                              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.dst.image) // uint32_t arraySize;
                }};

            // Prepare single mip level for reading
            const VkImageMemoryBarrier postImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                m_params.dst.image.operationLayout,     // VkImageLayout oldLayout;
                m_params.src.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                    mipLevel,                        // uint32_t baseMipLevel;
                    1u,                              // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.src.image) // uint32_t arraySize;
                }};

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &preImageBarrier);

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdBlitImage(*m_universalCmdBuffer, m_destination.get(), m_params.src.image.operationLayout,
                                m_destination.get(), m_params.dst.image.operationLayout, 1u, &regions[regionNdx],
                                m_params.filter);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkBlitImageInfo2KHR BlitImageInfo2KHR = {
                    VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
                    nullptr,                                 // const void* pNext;
                    m_destination.get(),                     // VkImage srcImage;
                    m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                    m_destination.get(),                     // VkImage dstImage;
                    m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
                    1u,                                      // uint32_t regionCount;
                    &regions2KHR[regionNdx],                 // const VkImageBlit2KHR* pRegions;
                    m_params.filter                          // VkFilter filter;
                };
                vk.cmdBlitImage2(*m_universalCmdBuffer, &BlitImageInfo2KHR);
            }

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);
        }

        // Prepare all mip levels for writing
        {
            const VkImageMemoryBarrier postImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                nullptr,                                // const void* pNext;
                VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags srcAccessMask;
                VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                m_params.src.image.operationLayout,     // VkImageLayout oldLayout;
                m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                m_destination.get(),                    // VkImage image;
                {
                    // VkImageSubresourceRange subresourceRange;
                    getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
                    0u,                              // uint32_t baseMipLevel;
                    VK_REMAINING_MIP_LEVELS,         // uint32_t mipLevels;
                    0u,                              // uint32_t baseArraySlice;
                    getArraySize(m_params.dst.image) // uint32_t arraySize;
                }};

            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);
        }
    }

    endCommandBuffer(vk, *m_universalCmdBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    return checkTestResult();
}

bool BlittingMipmaps::checkNonNearestFilteredResult(void)
{
    tcu::TestLog &log(m_context.getTestContext().getLog());
    bool allLevelsOk = true;

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
    {
        // Update reference results with previous results that have been verified.
        // This needs to be done such that accumulated errors don't exceed the fixed threshold.
        for (uint32_t i = 0; i < m_params.regions.size(); i++)
        {
            const CopyRegion region    = m_params.regions[i];
            const uint32_t srcMipLevel = m_params.regions[i].imageBlit.srcSubresource.mipLevel;
            const uint32_t dstMipLevel = m_params.regions[i].imageBlit.dstSubresource.mipLevel;
            de::MovePtr<tcu::TextureLevel> prevResultLevel;
            tcu::ConstPixelBufferAccess src;
            if (srcMipLevel < mipLevelNdx)
            {
                // Generate expected result from rendered result that was previously verified
                prevResultLevel = readImage(*m_destination, m_params.dst.image, srcMipLevel);
                src             = prevResultLevel->getAccess();
            }
            else
            {
                // Previous reference mipmaps might have changed, so recompute expected result
                src = m_expectedTextureLevel[srcMipLevel]->getAccess();
            }
            copyRegionToTextureLevel(src, m_expectedTextureLevel[dstMipLevel]->getAccess(), region, dstMipLevel);
        }

        de::MovePtr<tcu::TextureLevel> resultLevel      = readImage(*m_destination, m_params.dst.image, mipLevelNdx);
        const tcu::ConstPixelBufferAccess &resultAccess = resultLevel->getAccess();

        const tcu::Sampler::DepthStencilMode mode =
            tcu::hasDepthComponent(resultAccess.getFormat().order)   ? tcu::Sampler::MODE_DEPTH :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ? tcu::Sampler::MODE_STENCIL :
                                                                       tcu::Sampler::MODE_LAST;
        const tcu::ConstPixelBufferAccess result = tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                   tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                       resultAccess;
        const tcu::ConstPixelBufferAccess clampedLevel =
            tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
                m_expectedTextureLevel[mipLevelNdx]->getAccess();
        const tcu::ConstPixelBufferAccess unclampedLevel =
            tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                getEffectiveDepthStencilAccess(m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess(), mode) :
                m_unclampedExpectedTextureLevel[mipLevelNdx]->getAccess();
        const tcu::TextureFormat srcFormat =
            tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode) :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilTextureFormat(mapVkFormat(m_params.src.image.format), mode) :
                mapVkFormat(m_params.src.image.format);

        const tcu::TextureFormat dstFormat = result.getFormat();
        bool singleLevelOk                 = false;
        std::vector<CopyRegion> mipLevelRegions;

        for (size_t regionNdx = 0u; regionNdx < m_params.regions.size(); regionNdx++)
            if (m_params.regions.at(regionNdx).imageBlit.dstSubresource.mipLevel == mipLevelNdx)
                mipLevelRegions.push_back(m_params.regions.at(regionNdx));

        log << tcu::TestLog::Section("ClampedSourceImage", "Region with clamped edges on source image.");

        if (isFloatFormat(dstFormat))
        {
            const bool srcIsSRGB       = tcu::isSRGB(srcFormat);
            const tcu::Vec4 srcMaxDiff = getFormatThreshold(srcFormat) * tcu::Vec4(srcIsSRGB ? 2.0f : 1.0f);
            const tcu::Vec4 dstMaxDiff = getFormatThreshold(dstFormat);
            const tcu::Vec4 threshold =
                (srcMaxDiff + dstMaxDiff) * ((m_params.filter == VK_FILTER_CUBIC_EXT) ? 1.5f : 1.0f);

            singleLevelOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", clampedLevel, result,
                                                       threshold, tcu::COMPARE_LOG_RESULT);
            log << tcu::TestLog::EndSection;

            if (!singleLevelOk)
            {
                log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
                singleLevelOk = tcu::floatThresholdCompare(log, "Compare", "Result comparsion", unclampedLevel, result,
                                                           threshold, tcu::COMPARE_LOG_RESULT);
                log << tcu::TestLog::EndSection;
            }
        }
        else
        {
            tcu::UVec4 threshold;
            // Calculate threshold depending on channel width of destination format.
            const tcu::IVec4 dstBitDepth = tcu::getTextureFormatBitDepth(dstFormat);
            const tcu::IVec4 srcBitDepth = tcu::getTextureFormatBitDepth(srcFormat);
            for (uint32_t i = 0; i < 4; ++i)
            {
                DE_ASSERT(dstBitDepth[i] < std::numeric_limits<uint64_t>::digits);
                DE_ASSERT(srcBitDepth[i] < std::numeric_limits<uint64_t>::digits);
                uint64_t threshold64 =
                    1 + de::max(((UINT64_C(1) << dstBitDepth[i]) - 1) /
                                    de::clamp((UINT64_C(1) << srcBitDepth[i]) - 1, UINT64_C(1), UINT64_C(256)),
                                UINT64_C(1));
                DE_ASSERT(threshold64 <= std::numeric_limits<uint32_t>::max());
                threshold[i] = static_cast<uint32_t>(threshold64);
            }

            singleLevelOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", clampedLevel, result,
                                                     threshold, tcu::COMPARE_LOG_RESULT);
            log << tcu::TestLog::EndSection;

            if (!singleLevelOk)
            {
                log << tcu::TestLog::Section("NonClampedSourceImage", "Region with non-clamped edges on source image.");
                singleLevelOk = tcu::intThresholdCompare(log, "Compare", "Result comparsion", unclampedLevel, result,
                                                         threshold, tcu::COMPARE_LOG_RESULT);
                log << tcu::TestLog::EndSection;
            }
        }
        allLevelsOk &= singleLevelOk;
    }

    return allLevelsOk;
}

bool BlittingMipmaps::checkNearestFilteredResult(void)
{
    bool allLevelsOk = true;
    tcu::TestLog &log(m_context.getTestContext().getLog());

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
    {
        de::MovePtr<tcu::TextureLevel> resultLevel      = readImage(*m_destination, m_params.dst.image, mipLevelNdx);
        const tcu::ConstPixelBufferAccess &resultAccess = resultLevel->getAccess();

        const tcu::Sampler::DepthStencilMode mode =
            tcu::hasDepthComponent(resultAccess.getFormat().order)   ? tcu::Sampler::MODE_DEPTH :
            tcu::hasStencilComponent(resultAccess.getFormat().order) ? tcu::Sampler::MODE_STENCIL :
                                                                       tcu::Sampler::MODE_LAST;
        const tcu::ConstPixelBufferAccess result = tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                   tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                                                       getEffectiveDepthStencilAccess(resultAccess, mode) :
                                                       resultAccess;
        const tcu::ConstPixelBufferAccess source =
            (m_params.singleCommand || mipLevelNdx == 0) ? //  Read from source image
                tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode) :
                tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_sourceTextureLevel->getAccess(), mode) :
                m_sourceTextureLevel->getAccess()
                //  Read from destination image
                :
                tcu::hasDepthComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess(), mode) :
                tcu::hasStencilComponent(resultAccess.getFormat().order) ?
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess(), mode) :
                m_expectedTextureLevel[mipLevelNdx - 1u]->getAccess();
        const tcu::TextureFormat dstFormat             = result.getFormat();
        const tcu::TextureChannelClass dstChannelClass = tcu::getTextureChannelClass(dstFormat.type);
        bool singleLevelOk                             = false;
        std::vector<CopyRegion> mipLevelRegions;

        for (size_t regionNdx = 0u; regionNdx < m_params.regions.size(); regionNdx++)
            if (m_params.regions.at(regionNdx).imageBlit.dstSubresource.mipLevel == mipLevelNdx)
                mipLevelRegions.push_back(m_params.regions.at(regionNdx));

        // Use the calculated regions instead of the original ones.
        TestParams newParams = m_params;
        newParams.regions    = mipLevelRegions;

        tcu::TextureLevel errorMaskStorage(tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8),
                                           result.getWidth(), result.getHeight(), result.getDepth());
        tcu::PixelBufferAccess errorMask = errorMaskStorage.getAccess();
        tcu::Vec4 pixelBias(0.0f, 0.0f, 0.0f, 0.0f);
        tcu::Vec4 pixelScale(1.0f, 1.0f, 1.0f, 1.0f);

        tcu::clear(errorMask, tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0));

        if (dstChannelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER ||
            dstChannelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
        {
            singleLevelOk = intNearestBlitCompare(source, result, errorMask, newParams);
        }
        else
        {
            const tcu::Vec4 srcMaxDiff = getFloatOrFixedPointFormatThreshold(source.getFormat());
            const tcu::Vec4 dstMaxDiff = getFloatOrFixedPointFormatThreshold(result.getFormat());

            singleLevelOk = floatNearestBlitCompare(source, result, srcMaxDiff, dstMaxDiff, errorMask, newParams);
        }

        if (dstFormat != tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8))
            tcu::computePixelScaleBias(result, pixelScale, pixelBias);

        if (!singleLevelOk)
        {
            log << tcu::TestLog::ImageSet("Compare", "Result comparsion, level " + de::toString(mipLevelNdx))
                << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
                << tcu::TestLog::Image("Reference", "Reference", source, pixelScale, pixelBias)
                << tcu::TestLog::Image("ErrorMask", "Error mask", errorMask) << tcu::TestLog::EndImageSet;
        }
        else
        {
            log << tcu::TestLog::ImageSet("Compare", "Result comparsion, level " + de::toString(mipLevelNdx))
                << tcu::TestLog::Image("Result", "Result", result, pixelScale, pixelBias) << tcu::TestLog::EndImageSet;
        }

        allLevelsOk &= singleLevelOk;
    }

    return allLevelsOk;
}

tcu::TestStatus BlittingMipmaps::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    DE_UNREF(result);
    DE_ASSERT(m_params.filter == VK_FILTER_NEAREST || m_params.filter == VK_FILTER_LINEAR ||
              m_params.filter == VK_FILTER_CUBIC_EXT);
    const std::string failMessage("Result image is incorrect");

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        if (!checkNonNearestFilteredResult())
            return tcu::TestStatus::fail(failMessage);
    }
    else // NEAREST filtering
    {
        if (!checkNearestFilteredResult())
            return tcu::TestStatus::fail(failMessage);
    }

    return tcu::TestStatus::pass("Pass");
}

void BlittingMipmaps::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                               CopyRegion region, uint32_t mipLevel)
{
    DE_ASSERT(src.getDepth() == dst.getDepth());

    const MirrorMode mirrorMode = getMirrorMode(region.imageBlit.srcOffsets[0], region.imageBlit.srcOffsets[1],
                                                region.imageBlit.dstOffsets[0], region.imageBlit.dstOffsets[1]);

    flipCoordinates(region, mirrorMode);

    const VkOffset3D srcOffset = region.imageBlit.srcOffsets[0];
    const VkOffset3D srcExtent = {region.imageBlit.srcOffsets[1].x - srcOffset.x,
                                  region.imageBlit.srcOffsets[1].y - srcOffset.y,
                                  region.imageBlit.srcOffsets[1].z - srcOffset.z};
    const VkOffset3D dstOffset = region.imageBlit.dstOffsets[0];
    const VkOffset3D dstExtent = {region.imageBlit.dstOffsets[1].x - dstOffset.x,
                                  region.imageBlit.dstOffsets[1].y - dstOffset.y,
                                  region.imageBlit.dstOffsets[1].z - dstOffset.z};

    tcu::Sampler::FilterMode filter;
    switch (m_params.filter)
    {
    case VK_FILTER_LINEAR:
        filter = tcu::Sampler::LINEAR;
        break;
    case VK_FILTER_CUBIC_EXT:
        filter = tcu::Sampler::CUBIC;
        break;
    case VK_FILTER_NEAREST:
    default:
        filter = tcu::Sampler::NEAREST;
        break;
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());
        // Scale depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_DEPTH);
            tcu::scale(dstSubRegion, srcSubRegion, filter);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess depthSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_DEPTH);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel[0]->getAccess(), dstOffset.x, dstOffset.y,
                                      dstExtent.x, dstExtent.y),
                    tcu::Sampler::MODE_DEPTH);
                scaleFromWholeSrcBuffer(unclampedSubRegion, depthSrc, srcOffset, srcExtent, filter);
            }
        }

        // Scale stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcExtent.x, srcExtent.y), tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion = getEffectiveDepthStencilAccess(
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstExtent.x, dstExtent.y), tcu::Sampler::MODE_STENCIL);
            blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::ConstPixelBufferAccess stencilSrc =
                    getEffectiveDepthStencilAccess(src, tcu::Sampler::MODE_STENCIL);
                const tcu::PixelBufferAccess unclampedSubRegion = getEffectiveDepthStencilAccess(
                    tcu::getSubregion(m_unclampedExpectedTextureLevel[0]->getAccess(), dstOffset.x, dstOffset.y,
                                      dstExtent.x, dstExtent.y),
                    tcu::Sampler::MODE_STENCIL);
                scaleFromWholeSrcBuffer(unclampedSubRegion, stencilSrc, srcOffset, srcExtent, filter);
            }
        }
    }
    else
    {
        for (int layerNdx = 0u; layerNdx < src.getDepth(); layerNdx++)
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, srcOffset.x, srcOffset.y, layerNdx, srcExtent.x, srcExtent.y, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y, layerNdx, dstExtent.x, dstExtent.y, 1);
            blit(dstSubRegion, srcSubRegion, filter, mirrorMode);

            if (filter != tcu::Sampler::NEAREST)
            {
                const tcu::PixelBufferAccess unclampedSubRegion =
                    tcu::getSubregion(m_unclampedExpectedTextureLevel[mipLevel]->getAccess(), dstOffset.x, dstOffset.y,
                                      layerNdx, dstExtent.x, dstExtent.y, 1);
                scaleFromWholeSrcBuffer(unclampedSubRegion, srcSubRegion, srcOffset, srcExtent, filter);
            }
        }
    }
}

void BlittingMipmaps::generateExpectedResult(void)
{
    const tcu::ConstPixelBufferAccess src = m_sourceTextureLevel->getAccess();
    const tcu::ConstPixelBufferAccess dst = m_destinationTextureLevel->getAccess();

    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
        m_expectedTextureLevel[mipLevelNdx] = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(
            dst.getFormat(), dst.getWidth() >> mipLevelNdx, dst.getHeight() >> mipLevelNdx, dst.getDepth()));

    tcu::copy(m_expectedTextureLevel[0]->getAccess(), src);

    if (m_params.filter != VK_FILTER_NEAREST)
    {
        for (uint32_t mipLevelNdx = 0u; mipLevelNdx < m_params.mipLevels; mipLevelNdx++)
            m_unclampedExpectedTextureLevel[mipLevelNdx] = de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(
                dst.getFormat(), dst.getWidth() >> mipLevelNdx, dst.getHeight() >> mipLevelNdx, dst.getDepth()));

        tcu::copy(m_unclampedExpectedTextureLevel[0]->getAccess(), src);
    }

    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        CopyRegion region = m_params.regions[i];
        copyRegionToTextureLevel(
            m_expectedTextureLevel[m_params.regions[i].imageBlit.srcSubresource.mipLevel]->getAccess(),
            m_expectedTextureLevel[m_params.regions[i].imageBlit.dstSubresource.mipLevel]->getAccess(), region,
            m_params.regions[i].imageBlit.dstSubresource.mipLevel);
    }
}

class BlitMipmapTestCase : public vkt::TestCase
{
public:
    BlitMipmapTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new BlittingMipmaps(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        const InstanceInterface &vki        = context.getInstanceInterface();
        const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
        {
            VkImageFormatProperties properties;
            if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                    context.getPhysicalDevice(), m_params.src.image.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Format not supported");
            }
            else if ((m_params.src.image.extent.width > properties.maxExtent.width) ||
                     (m_params.src.image.extent.height > properties.maxExtent.height) ||
                     (m_params.src.image.extent.depth > properties.maxArrayLayers))
            {
                TCU_THROW(NotSupportedError, "Image size not supported");
            }
        }

        {
            VkImageFormatProperties properties;
            if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                    context.getPhysicalDevice(), m_params.dst.image.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Format not supported");
            }
            else if ((m_params.dst.image.extent.width > properties.maxExtent.width) ||
                     (m_params.dst.image.extent.height > properties.maxExtent.height) ||
                     (m_params.dst.image.extent.depth > properties.maxArrayLayers))
            {
                TCU_THROW(NotSupportedError, "Image size not supported");
            }
            else if (m_params.mipLevels > properties.maxMipLevels)
            {
                TCU_THROW(NotSupportedError, "Number of mip levels not supported");
            }

            checkExtensionSupport(context, m_params.extensionFlags);
        }

        const VkFormatProperties srcFormatProperties =
            getPhysicalDeviceFormatProperties(vki, vkPhysDevice, m_params.src.image.format);
        if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit source not supported");
        }

        const VkFormatProperties dstFormatProperties =
            getPhysicalDeviceFormatProperties(vki, vkPhysDevice, m_params.dst.image.format);
        if (!(dstFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        {
            TCU_THROW(NotSupportedError, "Format feature blit destination not supported");
        }

        if (m_params.filter == VK_FILTER_LINEAR &&
            !(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
            TCU_THROW(NotSupportedError, "Source format feature sampled image filter linear not supported");

        if (m_params.filter == VK_FILTER_CUBIC_EXT)
        {
            context.requireDeviceFunctionality("VK_EXT_filter_cubic");

            if (!(srcFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT))
            {
                TCU_THROW(NotSupportedError, "Source format feature sampled image filter cubic not supported");
            }
        }
    }

private:
    TestParams m_params;
};

void addBlittingImageSimpleTests(tcu::TestCaseGroup *group, TestParams &params)
{
    tcu::TestContext &testCtx = group->getTestContext();

    // Filter is VK_FILTER_NEAREST.
    {
        params.filter = VK_FILTER_NEAREST;

        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, "nearest", params));

        params.dst.image.format = VK_FORMAT_R32_SFLOAT;
        group->addChild(
            new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", params));

        params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
        group->addChild(
            new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_nearest", params));
    }

    // Filter is VK_FILTER_LINEAR.
    {
        params.filter = VK_FILTER_LINEAR;

        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, "linear", params));

        params.dst.image.format = VK_FORMAT_R32_SFLOAT;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", params));

        params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_linear", params));
    }

    // Filter is VK_FILTER_CUBIC_EXT.
    // Cubic filtering can only be used with 2D images.
    if (params.dst.image.imageType == VK_IMAGE_TYPE_2D)
    {
        params.filter = VK_FILTER_CUBIC_EXT;

        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, "cubic", params));

        params.dst.image.format = VK_FORMAT_R32_SFLOAT;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_cubic", params));

        params.dst.image.format = VK_FORMAT_B8G8R8A8_UNORM;
        group->addChild(new BlitImageTestCase(testCtx, getFormatCaseName(params.dst.image.format) + "_cubic", params));
    }
}

void addBlittingImageSimpleWholeTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageArrayTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);

    tcu::TestContext &testCtx = group->getTestContext();

    {
        const uint32_t baseLayer      = 0u;
        const uint32_t layerCount     = 16u;
        params.dst.image.format       = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent       = defaultExtent;
        params.dst.image.extent       = defaultExtent;
        params.src.image.extent.depth = layerCount;
        params.dst.image.extent.depth = layerCount;
        params.filter                 = VK_FILTER_NEAREST;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageBlit imageBlit = {
            defaultLayer,                               // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}}, // VkOffset3D srcOffsets[2];

            defaultLayer,                              // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;

        params.regions.push_back(region);

        group->addChild(new BlitImageTestCase(testCtx, "all_remaining_layers", params));
    }

    params.regions.clear();

    {
        const uint32_t baseLayer      = 2u;
        const uint32_t layerCount     = 16u;
        params.dst.image.format       = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent       = defaultExtent;
        params.dst.image.extent       = defaultExtent;
        params.src.image.extent.depth = layerCount;
        params.dst.image.extent.depth = layerCount;
        params.filter                 = VK_FILTER_NEAREST;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageBlit imageBlit = {
            defaultLayer,                               // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}}, // VkOffset3D srcOffsets[2];

            defaultLayer,                              // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;

        params.regions.push_back(region);

        group->addChild(new BlitImageTestCase(testCtx, "not_all_remaining_layers", params));
    }
}

std::string getFilterSuffix(VkFilter filter)
{
    static const size_t prefixLen = std::strlen("VK_FILTER_");
    return de::toLower(std::string(getFilterName(filter)).substr(prefixLen));
}

void addBlittingImage3DTo2DArrayTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    tcu::TestContext &testCtx = group->getTestContext();
    TestParams params         = *paramsPtr;

    const uint32_t layerCount     = 16u;
    params.dst.image.format       = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = layerCount;
    params.dst.image.extent.depth = layerCount;
    params.extensionFlags |= MAINTENANCE_8;

    for (const auto filter : {VK_FILTER_NEAREST, VK_FILTER_LINEAR})
    {
        params.filter            = filter;
        const std::string suffix = getFilterSuffix(filter);

        // Attempt to blit a single slice into a cube.
        {
            const auto cubeLayers             = 6u;
            TestParams cubeParams             = params;
            cubeParams.src.image.extent.depth = cubeLayers;
            cubeParams.dst.image.extent.depth = cubeLayers;

            const std::vector<VkImageBlit> blits{
                make3Dto2DArrayBlit(cubeParams.src.image.extent, cubeParams.dst.image.extent, 3u, 1u),
            };

            cubeParams.regions.clear();
            cubeParams.regions.reserve(blits.size());

            for (const auto &blit : blits)
            {
                CopyRegion region;
                region.imageBlit = blit;
                cubeParams.regions.push_back(region);
            }

            group->addChild(new BlitImageTestCase(testCtx, "cube_slice_" + suffix, cubeParams));
        }

        // Attempt to blit one layer at a time, for multiple layers.
        {
            const std::vector<VkImageBlit> blits{
                make3Dto2DArrayBlit(params.src.image.extent, params.dst.image.extent, 2u, 5u),
                make3Dto2DArrayBlit(params.src.image.extent, params.dst.image.extent, 4u, 11u),
                make3Dto2DArrayBlit(params.src.image.extent, params.dst.image.extent, 7u, 2u),
                make3Dto2DArrayBlit(params.src.image.extent, params.dst.image.extent, 13u, 0u),
            };

            params.regions.clear();
            params.regions.reserve(blits.size());

            for (const auto &blit : blits)
            {
                CopyRegion region;
                region.imageBlit = blit;
                params.regions.push_back(region);
            }

            group->addChild(new BlitImageTestCase(testCtx, "single_slices_" + suffix, params));
        }

        // Blit a slice into a smaller slice of a cube image.
        {
            auto blit = make3Dto2DArrayBlit(params.src.image.extent, params.dst.image.extent, 3u, 7u);

            blit.dstOffsets[0].x = defaultSize / 4;
            blit.dstOffsets[0].y = defaultSize / 2;

            blit.dstOffsets[1].x = defaultSize / 4 + defaultSize / 2;
            blit.dstOffsets[1].y = defaultSize;

            {
                CopyRegion region;
                region.imageBlit = blit;
                params.regions.clear();
                params.regions.push_back(region);
            }

            group->addChild(new BlitImageTestCase(testCtx, "complex_blit_" + suffix, params));
        }
    }
}

void addBlittingImageSimpleMirrorXYTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{defaultSize, defaultSize, 0}, {0, 0, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorXTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{defaultSize, 0, 0}, {0, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorYTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, defaultSize, 0}, {defaultSize, 0, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorZTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    DE_ASSERT(params.src.image.imageType == VK_IMAGE_TYPE_3D);
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = defaultSize;
    params.dst.image.extent.depth = defaultSize;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                   // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, defaultSize}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                  // VkImageSubresourceLayers dstSubresource;
            {{0, 0, defaultSize}, {defaultSize, defaultSize, 0}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleMirrorSubregionsTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    // No mirroring.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                          // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                         // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    // Flipping y coordinates.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{defaultHalfSize, 0, 0}, {defaultSize, defaultHalfSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{defaultHalfSize, defaultHalfSize, 0}, {defaultSize, 0, imageDepth}} // VkOffset3D dstOffset[2];
        };
        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    // Flipping x coordinates.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{0, defaultHalfSize, 0}, {defaultHalfSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{defaultHalfSize, defaultHalfSize, 0}, {0, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    // Flipping x and y coordinates.
    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{defaultHalfSize, defaultHalfSize, 0},
             {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{defaultSize, defaultSize, 0}, {defaultHalfSize, defaultHalfSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingWhole1Tests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    const int32_t halfImageDepth  = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultHalfSize : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultHalfExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = halfImageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                             // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, halfImageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingWhole2Tests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    const int32_t halfImageDepth  = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultHalfSize : 1;
    params.src.image.extent       = defaultHalfExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = halfImageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer,                                              // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultHalfSize, defaultHalfSize, halfImageDepth}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleScalingAndOffsetTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const int32_t imageDepth      = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultSize : 1;
    const int32_t srcDepthOffset  = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultQuarterSize : 0;
    const int32_t srcDepthSize    = params.src.image.imageType == VK_IMAGE_TYPE_3D ? defaultQuarterSize * 3 : 1;
    params.src.image.extent       = defaultExtent;
    params.dst.image.extent       = defaultExtent;
    params.src.image.extent.depth = imageDepth;
    params.dst.image.extent.depth = imageDepth;

    {
        const VkImageBlit imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{defaultQuarterSize, defaultQuarterSize, srcDepthOffset},
             {defaultQuarterSize * 3, defaultQuarterSize * 3, srcDepthSize}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer,                                 // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, imageDepth}} // VkOffset3D dstOffset[2];
        };

        CopyRegion region;
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleWithoutScalingPartialTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params = *paramsPtr;
    DE_ASSERT(params.src.image.imageType == params.dst.image.imageType);
    const bool is3dBlit     = params.src.image.imageType == VK_IMAGE_TYPE_3D;
    params.src.image.extent = defaultExtent;
    params.dst.image.extent = defaultExtent;

    if (is3dBlit)
    {
        params.src.image.extent.depth = defaultSize;
        params.dst.image.extent.depth = defaultSize;
    }

    {
        CopyRegion region;
        for (int i = 0; i < defaultSize; i += defaultQuarterSize)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {{defaultSize - defaultQuarterSize - i, defaultSize - defaultQuarterSize - i,
                  is3dBlit ? defaultSize - defaultQuarterSize - i : 0},
                 {defaultSize - i, defaultSize - i, is3dBlit ? defaultSize - i : 1}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {{i, i, is3dBlit ? i : 0},
                 {i + defaultQuarterSize, i + defaultQuarterSize,
                  is3dBlit ? i + defaultQuarterSize : 1}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }
    }

    addBlittingImageSimpleTests(group, params);
}

void addBlittingImageSimpleTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    TestParamsPtr params2D(new TestParams(params));
    addTestGroup(group, "whole", addBlittingImageSimpleWholeTests, params2D);
    addTestGroup(group, "array", addBlittingImageArrayTests, params2D);
    addTestGroup(group, "mirror_xy", addBlittingImageSimpleMirrorXYTests, params2D);
    addTestGroup(group, "mirror_x", addBlittingImageSimpleMirrorXTests, params2D);
    addTestGroup(group, "mirror_y", addBlittingImageSimpleMirrorYTests, params2D);
    addTestGroup(group, "mirror_subregions", addBlittingImageSimpleMirrorSubregionsTests, params2D);
    addTestGroup(group, "scaling_whole1", addBlittingImageSimpleScalingWhole1Tests, params2D);
    addTestGroup(group, "scaling_whole2", addBlittingImageSimpleScalingWhole2Tests, params2D);
    addTestGroup(group, "scaling_and_offset", addBlittingImageSimpleScalingAndOffsetTests, params2D);
    addTestGroup(group, "without_scaling_partial", addBlittingImageSimpleWithoutScalingPartialTests, params2D);

    params.src.image.imageType = VK_IMAGE_TYPE_3D;
    params.dst.image.imageType = VK_IMAGE_TYPE_3D;
    TestParamsPtr params3D(new TestParams(params));
    addTestGroup(group, "whole_3d", addBlittingImageSimpleWholeTests, params3D);
    addTestGroup(group, "mirror_xy_3d", addBlittingImageSimpleMirrorXYTests, params3D);
    addTestGroup(group, "mirror_x_3d", addBlittingImageSimpleMirrorXTests, params3D);
    addTestGroup(group, "mirror_y_3d", addBlittingImageSimpleMirrorYTests, params3D);
    addTestGroup(group, "mirror_z_3d", addBlittingImageSimpleMirrorZTests, params3D);
    addTestGroup(group, "mirror_subregions_3d", addBlittingImageSimpleMirrorSubregionsTests, params3D);
    addTestGroup(group, "scaling_whole1_3d", addBlittingImageSimpleScalingWhole1Tests, params3D);
    addTestGroup(group, "scaling_whole2_3d", addBlittingImageSimpleScalingWhole2Tests, params3D);
    addTestGroup(group, "scaling_and_offset_3d", addBlittingImageSimpleScalingAndOffsetTests, params3D);
    addTestGroup(group, "without_scaling_partial_3d", addBlittingImageSimpleWithoutScalingPartialTests, params3D);

    params.src.image.imageType = VK_IMAGE_TYPE_3D;
    params.dst.image.imageType = VK_IMAGE_TYPE_2D;
    TestParamsPtr params3D2D(new TestParams(params));
    addTestGroup(group, "3d_to_2d_array", addBlittingImage3DTo2DArrayTests, params3D2D);
}

enum FilterMaskBits
{
    FILTER_MASK_NEAREST = 0, // Always tested.
    FILTER_MASK_LINEAR  = (1u << 0),
    FILTER_MASK_CUBIC   = (1u << 1),
};

using FilterMask = uint32_t;

FilterMask makeFilterMask(bool onlyNearest, bool discardCubicFilter)
{
    FilterMask mask = FILTER_MASK_NEAREST;

    if (!onlyNearest)
    {
        mask |= FILTER_MASK_LINEAR;
        if (!discardCubicFilter)
            mask |= FILTER_MASK_CUBIC;
    }

    return mask;
}

struct BlitColorTestParams
{
    TestParams params;
    const std::vector<VkFormat> *compatibleFormats;
    FilterMask testFilters;
};

bool isAllowedBlittingAllFormatsColorSrcFormatTests(const BlitColorTestParams &testParams)
{
    bool result = true;

    if (testParams.params.allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        DE_ASSERT(!dedicatedAllocationBlittingFormatsToTestSet.empty());

        result = de::contains(dedicatedAllocationBlittingFormatsToTestSet, testParams.params.dst.image.format) ||
                 de::contains(dedicatedAllocationBlittingFormatsToTestSet, testParams.params.src.image.format);
    }

    return result;
}

const VkFormat linearOtherImageFormatsToTest[] = {
    // From compatibleFormats8Bit
    VK_FORMAT_R4G4_UNORM_PACK8,
    VK_FORMAT_R8_SRGB,

    // From compatibleFormats16Bit
    VK_FORMAT_R4G4B4A4_UNORM_PACK16,
    VK_FORMAT_R16_SFLOAT,

    // From compatibleFormats24Bit
    VK_FORMAT_R8G8B8_UNORM,
    VK_FORMAT_B8G8R8_SRGB,

    // From compatibleFormats32Bit
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_R32_SFLOAT,

    // From compatibleFormats48Bit
    VK_FORMAT_R16G16B16_UNORM,
    VK_FORMAT_R16G16B16_SFLOAT,

    // From compatibleFormats64Bit
    VK_FORMAT_R16G16B16A16_UNORM,
    VK_FORMAT_R64_SFLOAT,

    // From compatibleFormats96Bit
    VK_FORMAT_R32G32B32_UINT,
    VK_FORMAT_R32G32B32_SFLOAT,

    // From compatibleFormats128Bit
    VK_FORMAT_R32G32B32A32_UINT,
    VK_FORMAT_R64G64_SFLOAT,

    // From compatibleFormats192Bit
    VK_FORMAT_R64G64B64_UINT,
    VK_FORMAT_R64G64B64_SFLOAT,

    // From compatibleFormats256Bit
    VK_FORMAT_R64G64B64A64_UINT,
    VK_FORMAT_R64G64B64A64_SFLOAT,
};

std::string getBlitImageTilingLayoutCaseName(VkImageTiling tiling, VkImageLayout layout)
{
    switch (tiling)
    {
    case VK_IMAGE_TILING_OPTIMAL:
        return getImageLayoutCaseName(layout);
    case VK_IMAGE_TILING_LINEAR:
        return "linear";
    default:
        DE_ASSERT(false);
        return "";
    }
}

void addBlittingImageAllFormatsColorSrcFormatDstFormatTests(tcu::TestCaseGroup *group, BlitColorTestParams testParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    FormatSet linearOtherImageFormatsToTestSet;
    const int numOfOtherImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(linearOtherImageFormatsToTest);
    for (int otherImageFormatsIndex = 0; otherImageFormatsIndex < numOfOtherImageFormatsToTestFilter;
         ++otherImageFormatsIndex)
        linearOtherImageFormatsToTestSet.insert(linearOtherImageFormatsToTest[otherImageFormatsIndex]);

    const VkImageTiling blitSrcTilings[] = {
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_TILING_LINEAR,
    };
    const VkImageLayout blitSrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageTiling blitDstTilings[] = {
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_TILING_LINEAR,
    };
    const VkImageLayout blitDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcTilingNdx = 0u; srcTilingNdx < DE_LENGTH_OF_ARRAY(blitSrcTilings); ++srcTilingNdx)
    {
        testParams.params.src.image.tiling = blitSrcTilings[srcTilingNdx];

        for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
        {
            testParams.params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];

            // Don't bother testing VK_IMAGE_TILING_LINEAR + VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it's likely to be the same as VK_IMAGE_LAYOUT_GENERAL
            if (testParams.params.src.image.tiling == VK_IMAGE_TILING_LINEAR &&
                testParams.params.src.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                continue;

            for (int dstTilingNdx = 0u; dstTilingNdx < DE_LENGTH_OF_ARRAY(blitDstTilings); ++dstTilingNdx)
            {
                testParams.params.dst.image.tiling = blitDstTilings[dstTilingNdx];

                for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
                {
                    testParams.params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];

                    // Don't bother testing VK_IMAGE_TILING_LINEAR + VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL as it's likely to be the same as VK_IMAGE_LAYOUT_GENERAL
                    if (testParams.params.dst.image.tiling == VK_IMAGE_TILING_LINEAR &&
                        testParams.params.dst.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                        continue;

                    if ((testParams.params.dst.image.tiling == VK_IMAGE_TILING_LINEAR &&
                         !de::contains(linearOtherImageFormatsToTestSet, testParams.params.src.image.format)) ||
                        (testParams.params.src.image.tiling == VK_IMAGE_TILING_LINEAR &&
                         !de::contains(linearOtherImageFormatsToTestSet, testParams.params.dst.image.format)))
                        continue;

                    testParams.params.filter = VK_FILTER_NEAREST;
                    const std::string testName =
                        getBlitImageTilingLayoutCaseName(testParams.params.src.image.tiling,
                                                         testParams.params.src.image.operationLayout) +
                        "_" +
                        getBlitImageTilingLayoutCaseName(testParams.params.dst.image.tiling,
                                                         testParams.params.dst.image.operationLayout);
                    group->addChild(new BlitImageTestCase(testCtx, testName + "_nearest", testParams.params));

                    if (testParams.testFilters & FILTER_MASK_LINEAR)
                    {
                        testParams.params.filter = VK_FILTER_LINEAR;
                        group->addChild(new BlitImageTestCase(testCtx, testName + "_linear", testParams.params));
                    }

                    if (testParams.testFilters & FILTER_MASK_CUBIC)
                    {
                        testParams.params.filter = VK_FILTER_CUBIC_EXT;
                        group->addChild(new BlitImageTestCase(testCtx, testName + "_cubic", testParams.params));
                    }

                    if ((testParams.params.src.image.imageType == VK_IMAGE_TYPE_3D) &&
                        !isCompressedFormat(testParams.params.src.image.format))
                    {
                        const struct
                        {
                            FillMode mode;
                            const char *name;
                        } modeList[] = {
                            {FILL_MODE_BLUE_RED_X, "x"},
                            {FILL_MODE_BLUE_RED_Y, "y"},
                            {FILL_MODE_BLUE_RED_Z, "z"},
                        };

                        auto otherParams                      = testParams;
                        otherParams.params.dst.image.fillMode = FILL_MODE_WHITE;

                        for (int i = 0; i < DE_LENGTH_OF_ARRAY(modeList); ++i)
                        {
                            otherParams.params.src.image.fillMode = modeList[i].mode;

                            otherParams.params.filter = VK_FILTER_LINEAR;
                            group->addChild(new BlitImageTestCase(
                                testCtx, testName + "_linear_stripes_" + modeList[i].name, otherParams.params));

                            otherParams.params.filter = VK_FILTER_NEAREST;
                            group->addChild(new BlitImageTestCase(
                                testCtx, testName + "_nearest_stripes_" + modeList[i].name, otherParams.params));
                        }
                    }
                }
            }
        }
    }
}

void addBlittingImageAllFormatsColorSrcFormatTests(tcu::TestCaseGroup *group, BlitColorTestParams testParams)
{
    VkFormat srcFormat = testParams.params.src.image.format;

    if (testParams.compatibleFormats)
    {
        for (auto format : *testParams.compatibleFormats)
        {
            testParams.params.dst.image.format = format;
            if (!isSupportedByFramework(format))
                continue;

            if (!isAllowedBlittingAllFormatsColorSrcFormatTests(testParams))
                continue;

            addTestGroup(group, getFormatCaseName(format), addBlittingImageAllFormatsColorSrcFormatDstFormatTests,
                         testParams);
        }
    }

    // If testParams.compatibleFormats is nullptr, the destination format will be copied from the source format
    // When testParams.compatibleFormats is not nullptr but format is compressed we also need to add that format
    // as it is not on compatibleFormats list
    if (!testParams.compatibleFormats || isCompressedFormat(srcFormat))
    {
        testParams.params.dst.image.format = srcFormat;

        addTestGroup(group, getFormatCaseName(srcFormat), addBlittingImageAllFormatsColorSrcFormatDstFormatTests,
                     testParams);
    }
}

const VkFormat dedicatedAllocationBlittingFormatsToTest[] = {
    // compatibleFormatsUInts
    VK_FORMAT_R8_UINT,
    VK_FORMAT_R64G64B64A64_UINT,

    // compatibleFormatsSInts
    VK_FORMAT_R8_SINT,
    VK_FORMAT_R64G64B64A64_SINT,

    // compatibleFormatsFloats
    VK_FORMAT_R4G4_UNORM_PACK8,
    VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,

    // compatibleFormatsSrgb
    VK_FORMAT_R8_SRGB,
    VK_FORMAT_A8B8G8R8_SRGB_PACK32,
};

// skip cubic filtering test for the following data formats
const FormatSet onlyNearestAndLinearFormatsToTest = {VK_FORMAT_A8B8G8R8_USCALED_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SSCALED_PACK32, VK_FORMAT_A8B8G8R8_UINT_PACK32,
                                                     VK_FORMAT_A8B8G8R8_SINT_PACK32};

// astc formats have diferent block sizes and thus require diferent resolutions for images
enum class AstcImageSizeType
{
    SIZE_64_64 = 0,
    SIZE_60_64,
    SIZE_64_60,
    SIZE_60_60,
};

const std::map<VkFormat, AstcImageSizeType> astcSizes{
    {VK_FORMAT_ASTC_4x4_SRGB_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_5x4_SRGB_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_5x5_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x5_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x6_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_8x5_SRGB_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x6_SRGB_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, AstcImageSizeType::SIZE_64_60},
    {VK_FORMAT_ASTC_8x8_SRGB_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, AstcImageSizeType::SIZE_64_64},
    {VK_FORMAT_ASTC_10x5_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x6_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x8_SRGB_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, AstcImageSizeType::SIZE_60_64},
    {VK_FORMAT_ASTC_10x10_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x10_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x12_SRGB_BLOCK, AstcImageSizeType::SIZE_60_60},
    {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, AstcImageSizeType::SIZE_60_60}};

std::vector<CopyRegion> create2DCopyRegions(int32_t srcWidth, int32_t srcHeight, int32_t dstWidth, int32_t dstHeight)
{
    CopyRegion region;
    std::vector<CopyRegion> regionsVector;

    int32_t fourthOfSrcWidth  = srcWidth / 4;
    int32_t fourthOfSrcHeight = srcHeight / 4;
    int32_t fourthOfDstWidth  = dstWidth / 4;
    int32_t fourthOfDstHeight = dstHeight / 4;

    // to the top of resulting image copy whole source image but with increasingly smaller sizes
    for (int i = 0, j = 1; (i + fourthOfDstWidth / j < dstWidth) && (fourthOfDstWidth > j); i += fourthOfDstWidth / j++)
    {
        region.imageBlit = {
            defaultSourceLayer,                    // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {srcWidth, srcHeight, 1}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{i, 0, 0}, {i + fourthOfDstWidth / j, fourthOfDstHeight / j, 1}} // VkOffset3D dstOffset[2];
        };
        regionsVector.push_back(region);
    }

    // to the bottom of resulting image copy parts of source image;
    for (int i = 0; i < 4; ++i)
    {
        int srcX = i * fourthOfSrcWidth;
        int srcY = i * fourthOfSrcHeight;
        int dstX = i * fourthOfDstWidth;

        region.imageBlit = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {{srcX, srcY, 0}, {srcX + fourthOfSrcWidth, srcY + fourthOfSrcHeight, 1}}, // VkOffset3D srcOffsets[2];

            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {{dstX, 2 * fourthOfDstHeight, 0},
             {dstX + fourthOfDstWidth, 3 * fourthOfDstHeight, 1}} // VkOffset3D dstOffset[2];
        };

        regionsVector.push_back(region);
    }

    return regionsVector;
}

void addBlittingImageAllFormatsColorTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                          uint32_t extensionFlags)
{
    const struct
    {
        const std::vector<VkFormat> *sourceFormats;
        const std::vector<VkFormat> *destinationFormats;
        const bool onlyNearest;
    } colorImageFormatsToTestBlit[]{
        {&formats::compatibleFormatsUInts, &formats::compatibleFormatsUInts, true},
        {&formats::compatibleFormatsSInts, &formats::compatibleFormatsSInts, true},
        {&formats::compatibleFormatsFloats, &formats::compatibleFormatsFloats, false},
        {&formats::compressedFormatsFloats, &formats::compatibleFormatsFloats, false},
        {&formats::compatibleFormatsSrgb, &formats::compatibleFormatsSrgb, false},
        {&formats::compressedFormatsSrgb, &formats::compatibleFormatsSrgb, false},
    };

    if (allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter;
             ++compatibleFormatsIndex)
            dedicatedAllocationBlittingFormatsToTestSet.insert(
                dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
    }

    // 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = defaultExtent;
        params.dst.image.extent    = defaultExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.allocationKind      = allocationKind;
        params.extensionFlags      = extensionFlags;

        // create all required copy regions
        const std::map<AstcImageSizeType, std::vector<CopyRegion>> imageRegions{
            {AstcImageSizeType::SIZE_64_64, create2DCopyRegions(64, 64, 64, 64)},
            {AstcImageSizeType::SIZE_60_64, create2DCopyRegions(60, 64, 60, 64)},
            {AstcImageSizeType::SIZE_64_60, create2DCopyRegions(64, 60, 64, 60)},
            {AstcImageSizeType::SIZE_60_60, create2DCopyRegions(60, 60, 60, 60)},
        };

        for (const auto &compatibleFormats : colorImageFormatsToTestBlit)
        {
            for (const auto srcFormat : *compatibleFormats.sourceFormats)
            {
                params.src.image.format = srcFormat;

                const bool onlyNearestAndLinear = de::contains(onlyNearestAndLinearFormatsToTest, srcFormat);

                // most of tests are using regions caluculated for 64x64 size but astc formats require custom regions
                params.regions = imageRegions.at(AstcImageSizeType::SIZE_64_64);
                if (isCompressedFormat(srcFormat) && isAstcFormat(mapVkCompressedFormat(srcFormat)))
                    params.regions = imageRegions.at(astcSizes.at(srcFormat));

                // use the fact that first region contains the size of full source image
                // and make source and destination the same size - this is needed for astc formats
                const VkOffset3D &srcImageSize = params.regions[0].imageBlit.srcOffsets[1];
                VkExtent3D &srcImageExtent     = params.src.image.extent;
                VkExtent3D &dstImageExtent     = params.dst.image.extent;
                srcImageExtent.width           = srcImageSize.x;
                srcImageExtent.height          = srcImageSize.y;
                dstImageExtent.width           = srcImageSize.x;
                dstImageExtent.height          = srcImageSize.y;

                BlitColorTestParams testParams{params, compatibleFormats.destinationFormats,
                                               makeFilterMask(compatibleFormats.onlyNearest, onlyNearestAndLinear)};

                addTestGroup(subGroup.get(), getFormatCaseName(srcFormat),
                             addBlittingImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = default1dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.allocationKind      = allocationKind;
        params.extensionFlags      = extensionFlags;

        CopyRegion region;
        for (int i = 0; i < defaultSize; i += defaultSize / 2)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer,               // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultSize, 1, 1}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer,                         // VkImageSubresourceLayers dstSubresource;
                {{i, 0, 0}, {i + defaultQuarterSize, 1, 1}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }

        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer,                      // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultQuarterSize, 1, 1}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer,                                          // VkImageSubresourceLayers dstSubresource;
                {{defaultQuarterSize, 0, 0}, {2 * defaultQuarterSize, 1, 1}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }

        for (const auto &compatibleFormats : colorImageFormatsToTestBlit)
        {
            const auto *sourceFormats = compatibleFormats.sourceFormats;
            const bool onlyNearest    = compatibleFormats.onlyNearest;
            for (auto srcFormat : *sourceFormats)
            {
                params.src.image.format = srcFormat;
                if (!isSupportedByFramework(srcFormat))
                    continue;

                // Cubic filtering can only be used with 2D images.
                const bool onlyNearestAndLinear = true;

                BlitColorTestParams testParams{params, nullptr, makeFilterMask(onlyNearest, onlyNearestAndLinear)};

                addTestGroup(subGroup.get(), getFormatCaseName(srcFormat),
                             addBlittingImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = default3dExtent;
        params.dst.image.extent    = default3dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.allocationKind      = allocationKind;
        params.extensionFlags      = extensionFlags;

        CopyRegion region;
        for (int i = 0, j = 1; (i + defaultSixteenthSize / j < defaultQuarterSize) && (defaultSixteenthSize > j);
             i += defaultSixteenthSize / j++)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultQuarterSize, defaultQuarterSize, defaultQuarterSize}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {{i, 0, i},
                 {i + defaultSixteenthSize / j, defaultSixteenthSize / j,
                  i + defaultSixteenthSize / j}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }
        for (int i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {{i, i, i},
                 {i + defaultSixteenthSize, i + defaultSixteenthSize,
                  i + defaultSixteenthSize}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {{i, defaultQuarterSize / 2, i},
                 {i + defaultSixteenthSize, defaultQuarterSize / 2 + defaultSixteenthSize,
                  i + defaultSixteenthSize}} // VkOffset3D dstOffset[2];
            };
            region.imageBlit = imageBlit;
            params.regions.push_back(region);
        }

        for (const auto &compatibleFormats : colorImageFormatsToTestBlit)
        {
            const auto *sourceFormats = compatibleFormats.sourceFormats;
            const bool onlyNearest    = compatibleFormats.onlyNearest;
            for (auto srcFormat : *sourceFormats)
            {
                params.src.image.format = srcFormat;
                if (!isSupportedByFramework(srcFormat))
                    continue;

                // Cubic filtering can only be used with 2D images.
                const bool onlyNearestAndLinear = true;

                BlitColorTestParams testParams{params, nullptr, makeFilterMask(onlyNearest, onlyNearestAndLinear)};

                addTestGroup(subGroup.get(), getFormatCaseName(srcFormat),
                             addBlittingImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addBlittingImageAllFormatsDepthStencilFormatsTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    TestParams params                    = *paramsPtr;
    const VkImageLayout blitSrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout blitDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
    {
        params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];

        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
        {
            params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];
            params.filter                    = VK_FILTER_NEAREST;

            const std::string testName = getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(params.dst.image.operationLayout);

            group->addChild(new BlitImageTestCase(group->getTestContext(), testName + "_nearest", params));
        }
    }
}

void addBlittingImageAllFormatsDepthStencilTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                 uint32_t extensionFlags)
{
    const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
    const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
    const VkImageSubresourceLayers defaultDSSourceLayer = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u,
                                                           0u, 1u};

    // 2D tests
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = defaultExtent;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.src.image.format    = format;
            params.dst.image.extent    = defaultExtent;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.format    = params.src.image.format;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = allocationKind;
            params.extensionFlags      = extensionFlags;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            CopyRegion region;
            for (int i = 0, j = 1; (i + defaultQuarterSize / j < defaultSize) && (defaultQuarterSize > j);
                 i += defaultQuarterSize / j++)
            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultSize, defaultSize, 1};
                const VkOffset3D dstOffset0 = {i, 0, 0};
                const VkOffset3D dstOffset1 = {i + defaultQuarterSize / j, defaultQuarterSize / j, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}, // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1},  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }
            for (int i = 0; i < defaultSize; i += defaultQuarterSize)
            {
                const VkOffset3D srcOffset0 = {i, i, 0};
                const VkOffset3D srcOffset1 = {i + defaultQuarterSize, i + defaultQuarterSize, 1};
                const VkOffset3D dstOffset0 = {i, defaultSize / 2, 0};
                const VkOffset3D dstOffset1 = {i + defaultQuarterSize, defaultSize / 2 + defaultQuarterSize, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}   // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasDepth && hasStencil)
                {
                    const VkOffset3D dstDSOffset0 = {i, 3 * defaultQuarterSize, 0};
                    const VkOffset3D dstDSOffset1 = {i + defaultQuarterSize, defaultSize, 1};
                    const VkImageBlit imageBlit   = {
                        defaultDSSourceLayer,        // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},    // VkOffset3D srcOffsets[2];
                        defaultDSSourceLayer,        // VkImageSubresourceLayers dstSubresource;
                        {dstDSOffset0, dstDSOffset1} // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addBlittingImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addBlittingImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D tests
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = default1dExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = allocationKind;
            params.extensionFlags      = extensionFlags;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            CopyRegion region;
            for (int i = 0; i < defaultSize; i += defaultSize / 2)
            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultSize, 1, 1};
                const VkOffset3D dstOffset0 = {i, 0, 0};
                const VkOffset3D dstOffset1 = {i + defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}, // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1},  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultQuarterSize, 1, 1};
                const VkOffset3D dstOffset0 = {defaultQuarterSize, 0, 0};
                const VkOffset3D dstOffset1 = {2 * defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}   // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasDepth && hasStencil)
                {
                    const VkOffset3D dstDSOffset0 = {3 * defaultQuarterSize, 0, 0};
                    const VkOffset3D dstDSOffset1 = {3 * defaultQuarterSize + defaultQuarterSize / 2, 1, 1};
                    const VkImageBlit imageBlit   = {
                        defaultDSSourceLayer,        // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},    // VkOffset3D srcOffsets[2];
                        defaultDSSourceLayer,        // VkImageSubresourceLayers dstSubresource;
                        {dstDSOffset0, dstDSOffset1} // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addBlittingImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addBlittingImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = default3dExtent;
            params.dst.image.extent    = default3dExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = allocationKind;
            params.extensionFlags      = extensionFlags;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            CopyRegion region;
            for (int i = 0, j = 1; (i + defaultSixteenthSize / j < defaultQuarterSize) && (defaultSixteenthSize > j);
                 i += defaultSixteenthSize / j++)
            {
                const VkOffset3D srcOffset0 = {0, 0, 0};
                const VkOffset3D srcOffset1 = {defaultQuarterSize, defaultQuarterSize, defaultQuarterSize};
                const VkOffset3D dstOffset0 = {i, 0, i};
                const VkOffset3D dstOffset1 = {i + defaultSixteenthSize / j, defaultSixteenthSize / j,
                                               i + defaultSixteenthSize / j};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}, // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1},  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }
            for (int i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
            {
                const VkOffset3D srcOffset0 = {i, i, i};
                const VkOffset3D srcOffset1 = {i + defaultSixteenthSize, i + defaultSixteenthSize,
                                               i + defaultSixteenthSize};
                const VkOffset3D dstOffset0 = {i, defaultQuarterSize / 2, i};
                const VkOffset3D dstOffset1 = {i + defaultSixteenthSize, defaultQuarterSize / 2 + defaultSixteenthSize,
                                               i + defaultSixteenthSize};

                if (hasDepth)
                {
                    const VkImageBlit imageBlit = {
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1}, // VkOffset3D srcOffsets[2];
                        defaultDepthSourceLayer,  // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}  // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasStencil)
                {
                    const VkImageBlit imageBlit = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},  // VkOffset3D srcOffsets[2];
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {dstOffset0, dstOffset1}   // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
                if (hasDepth && hasStencil)
                {
                    const VkOffset3D dstDSOffset0 = {i, 3 * defaultSixteenthSize, i};
                    const VkOffset3D dstDSOffset1 = {i + defaultSixteenthSize, defaultQuarterSize,
                                                     i + defaultSixteenthSize};
                    const VkImageBlit imageBlit   = {
                        defaultDSSourceLayer,        // VkImageSubresourceLayers srcSubresource;
                        {srcOffset0, srcOffset1},    // VkOffset3D srcOffsets[2];
                        defaultDSSourceLayer,        // VkImageSubresourceLayers dstSubresource;
                        {dstDSOffset0, dstDSOffset1} // VkOffset3D dstOffset[2];
                    };
                    region.imageBlit = imageBlit;
                    params.regions.push_back(region);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addBlittingImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addBlittingImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addBlittingImageAllFormatsMipmapFormatTests(tcu::TestCaseGroup *group, BlitColorTestParams testParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const VkImageLayout blitSrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout blitDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(blitSrcLayouts); ++srcLayoutNdx)
    {
        testParams.params.src.image.operationLayout = blitSrcLayouts[srcLayoutNdx];
        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(blitDstLayouts); ++dstLayoutNdx)
        {
            testParams.params.dst.image.operationLayout = blitDstLayouts[dstLayoutNdx];

            testParams.params.filter   = VK_FILTER_NEAREST;
            const std::string testName = getImageLayoutCaseName(testParams.params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
            group->addChild(new BlitMipmapTestCase(testCtx, testName + "_nearest", testParams.params));

            if (testParams.testFilters & FILTER_MASK_LINEAR)
            {
                testParams.params.filter = VK_FILTER_LINEAR;
                group->addChild(new BlitMipmapTestCase(testCtx, testName + "_linear", testParams.params));
            }

            if (testParams.testFilters & FILTER_MASK_CUBIC)
            {
                testParams.params.filter = VK_FILTER_CUBIC_EXT;
                group->addChild(new BlitMipmapTestCase(testCtx, testName + "_cubic", testParams.params));
            }
        }
    }
}

void addBlittingImageAllFormatsBaseLevelMipmapTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                    uint32_t extensionFlags)
{
    const struct
    {
        const std::vector<VkFormat> &compatibleFormats;
        const bool onlyNearest;
    } colorImageFormatsToTestBlit[] = {
        {formats::compatibleFormatsUInts, true},
        {formats::compatibleFormatsSInts, true},
        {formats::compatibleFormatsFloats, false},
        {formats::compatibleFormatsSrgb, false},
    };

    const int layerCountsToTest[] = {1, 6};

    TestParams params;
    params.src.image.imageType = VK_IMAGE_TYPE_2D;
    params.src.image.extent    = defaultExtent;
    params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.imageType = VK_IMAGE_TYPE_2D;
    params.dst.image.extent    = defaultExtent;
    params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.allocationKind      = allocationKind;
    params.extensionFlags      = extensionFlags;
    params.mipLevels           = deLog2Floor32(deMaxu32(defaultExtent.width, defaultExtent.height)) + 1u;
    params.singleCommand       = true;

    CopyRegion region;
    for (uint32_t mipLevelNdx = 0u; mipLevelNdx < params.mipLevels; mipLevelNdx++)
    {
        VkImageSubresourceLayers destLayer = defaultSourceLayer;
        destLayer.mipLevel                 = mipLevelNdx;

        const VkImageBlit imageBlit = {
            defaultSourceLayer,                         // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0}, {defaultSize, defaultSize, 1}}, // VkOffset3D srcOffsets[2];

            destLayer, // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize >> mipLevelNdx, defaultSize >> mipLevelNdx, 1}} // VkOffset3D dstOffset[2];
        };
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    if (allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter;
             ++compatibleFormatsIndex)
            dedicatedAllocationBlittingFormatsToTestSet.insert(
                dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
    }

    for (int layerCountIndex = 0; layerCountIndex < DE_LENGTH_OF_ARRAY(layerCountsToTest); layerCountIndex++)
    {
        const int layerCount             = layerCountsToTest[layerCountIndex];
        const std::string layerGroupName = "layercount_" + de::toString(layerCount);

        de::MovePtr<tcu::TestCaseGroup> layerCountGroup(
            new tcu::TestCaseGroup(group->getTestContext(), layerGroupName.c_str()));

        for (const auto &formatsData : colorImageFormatsToTestBlit)
        {
            const auto &compatibleFormats = formatsData.compatibleFormats;
            const bool onlyNearest        = formatsData.onlyNearest;

            for (auto srcFormat : compatibleFormats)
            {
                params.src.image.format = srcFormat;
                params.dst.image.format = srcFormat;

                if (!isSupportedByFramework(params.src.image.format))
                    continue;

                const bool onlyNearestAndLinear =
                    de::contains(onlyNearestAndLinearFormatsToTest, params.src.image.format);

                BlitColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = &compatibleFormats;
                testParams.testFilters       = makeFilterMask(onlyNearest, onlyNearestAndLinear);

                testParams.params.src.image.extent.depth = layerCount;
                testParams.params.dst.image.extent.depth = layerCount;

                for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
                {
                    testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
                    testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
                }

                addTestGroup(layerCountGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsMipmapFormatTests, testParams);
            }
        }
        group->addChild(layerCountGroup.release());
    }
}

void addBlittingImageAllFormatsPreviousLevelMipmapTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                        uint32_t extensionFlags)
{
    const struct
    {
        const std::vector<VkFormat> &compatibleFormats;
        const bool onlyNearest;
    } colorImageFormatsToTestBlit[] = {
        {formats::compatibleFormatsUInts, true},
        {formats::compatibleFormatsSInts, true},
        {formats::compatibleFormatsFloats, false},
        {formats::compatibleFormatsSrgb, false},
    };

    const int layerCountsToTest[] = {1, 6};

    TestParams params;
    params.src.image.imageType = VK_IMAGE_TYPE_2D;
    params.src.image.extent    = defaultExtent;
    params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.imageType = VK_IMAGE_TYPE_2D;
    params.dst.image.extent    = defaultExtent;
    params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
    params.allocationKind      = allocationKind;
    params.extensionFlags      = extensionFlags;
    params.mipLevels           = deLog2Floor32(deMaxu32(defaultExtent.width, defaultExtent.height)) + 1u;
    params.singleCommand       = false;

    CopyRegion region;
    for (uint32_t mipLevelNdx = 1u; mipLevelNdx < params.mipLevels; mipLevelNdx++)
    {
        VkImageSubresourceLayers srcLayer  = defaultSourceLayer;
        VkImageSubresourceLayers destLayer = defaultSourceLayer;

        srcLayer.mipLevel  = mipLevelNdx - 1u;
        destLayer.mipLevel = mipLevelNdx;

        const VkImageBlit imageBlit = {
            srcLayer, // VkImageSubresourceLayers srcSubresource;
            {{0, 0, 0},
             {defaultSize >> (mipLevelNdx - 1u), defaultSize >> (mipLevelNdx - 1u), 1}}, // VkOffset3D srcOffsets[2];

            destLayer, // VkImageSubresourceLayers dstSubresource;
            {{0, 0, 0}, {defaultSize >> mipLevelNdx, defaultSize >> mipLevelNdx, 1}} // VkOffset3D dstOffset[2];
        };
        region.imageBlit = imageBlit;
        params.regions.push_back(region);
    }

    if (allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfColorImageFormatsToTestFilter = DE_LENGTH_OF_ARRAY(dedicatedAllocationBlittingFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfColorImageFormatsToTestFilter;
             ++compatibleFormatsIndex)
            dedicatedAllocationBlittingFormatsToTestSet.insert(
                dedicatedAllocationBlittingFormatsToTest[compatibleFormatsIndex]);
    }

    for (int layerCountIndex = 0; layerCountIndex < DE_LENGTH_OF_ARRAY(layerCountsToTest); layerCountIndex++)
    {
        const int layerCount             = layerCountsToTest[layerCountIndex];
        const std::string layerGroupName = "layercount_" + de::toString(layerCount);

        de::MovePtr<tcu::TestCaseGroup> layerCountGroup(
            new tcu::TestCaseGroup(group->getTestContext(), layerGroupName.c_str()));

        for (const auto &formatsData : colorImageFormatsToTestBlit)
        {
            const auto &compatibleFormats = formatsData.compatibleFormats;
            const bool onlyNearest        = formatsData.onlyNearest;

            for (auto srcFormat : compatibleFormats)
            {
                params.src.image.format = srcFormat;
                params.dst.image.format = srcFormat;

                if (!isSupportedByFramework(srcFormat))
                    continue;

                const bool onlyNearestAndLinear = de::contains(onlyNearestAndLinearFormatsToTest, srcFormat);

                BlitColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = &compatibleFormats;
                testParams.testFilters       = makeFilterMask(onlyNearest, onlyNearestAndLinear);

                testParams.params.src.image.extent.depth = layerCount;
                testParams.params.dst.image.extent.depth = layerCount;

                for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
                {
                    testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
                    testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
                }

                addTestGroup(layerCountGroup.get(), getFormatCaseName(params.src.image.format),
                             addBlittingImageAllFormatsMipmapFormatTests, testParams);
            }
        }
        group->addChild(layerCountGroup.release());
    }

    for (int multiLayer = 0; multiLayer < 2; multiLayer++)
    {
        const int layerCount = multiLayer ? 6 : 1;

        for (int barrierCount = 1; barrierCount < 4; barrierCount++)
        {
            if (layerCount != 1 || barrierCount != 1)
            {
                const std::string barrierGroupName =
                    (multiLayer ? "layerbarriercount_" : "mipbarriercount_") + de::toString(barrierCount);

                de::MovePtr<tcu::TestCaseGroup> barrierCountGroup(
                    new tcu::TestCaseGroup(group->getTestContext(), barrierGroupName.c_str()));

                params.barrierCount = barrierCount;

                // Only go through a few common formats
                for (int srcFormatIndex = 2; srcFormatIndex < 6; ++srcFormatIndex)
                {
                    params.src.image.format = formats::compatibleFormatsUInts[srcFormatIndex];
                    params.dst.image.format = formats::compatibleFormatsUInts[srcFormatIndex];

                    if (!isSupportedByFramework(params.src.image.format))
                        continue;

                    BlitColorTestParams testParams;
                    testParams.params            = params;
                    testParams.compatibleFormats = &formats::compatibleFormatsUInts;
                    testParams.testFilters       = FILTER_MASK_NEAREST;

                    testParams.params.src.image.extent.depth = layerCount;
                    testParams.params.dst.image.extent.depth = layerCount;

                    for (size_t regionNdx = 0; regionNdx < testParams.params.regions.size(); regionNdx++)
                    {
                        testParams.params.regions[regionNdx].imageBlit.srcSubresource.layerCount = layerCount;
                        testParams.params.regions[regionNdx].imageBlit.dstSubresource.layerCount = layerCount;
                    }

                    addTestGroup(barrierCountGroup.get(), getFormatCaseName(params.src.image.format),
                                 addBlittingImageAllFormatsMipmapFormatTests, testParams);
                }
                group->addChild(barrierCountGroup.release());
            }
        }
    }
}

void addBlittingImageAllFormatsMipmapTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                           uint32_t extensionFlags)
{
    addTestGroup(group, "from_base_level", addBlittingImageAllFormatsBaseLevelMipmapTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "from_previous_level", addBlittingImageAllFormatsPreviousLevelMipmapTests, allocationKind,
                 extensionFlags);
}

void addBlittingImageAllFormatsTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    addTestGroup(group, "color", addBlittingImageAllFormatsColorTests, allocationKind, extensionFlags);
    addTestGroup(group, "depth_stencil", addBlittingImageAllFormatsDepthStencilTests, allocationKind, extensionFlags);
    addTestGroup(group, "generate_mipmaps", addBlittingImageAllFormatsMipmapTests, allocationKind, extensionFlags);
}

} // namespace

void addBlittingImageTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    addTestGroup(group, "simple_tests", addBlittingImageSimpleTests, allocationKind, extensionFlags);
    addTestGroup(group, "all_formats", addBlittingImageAllFormatsTests, allocationKind, extensionFlags);
}

} // namespace api
} // namespace vkt
