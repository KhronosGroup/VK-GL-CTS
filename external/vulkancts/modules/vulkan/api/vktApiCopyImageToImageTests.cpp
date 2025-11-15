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
 * \brief Vulkan Copy Image To Image Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyImageToImageTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

FormatSet dedicatedAllocationImageToImageFormatsToTestSet;
FormatSet dedicatedAllocationBlittingFormatsToTestSet;

struct CopyColorTestParams
{
    TestParams params;
    const std::vector<VkFormat> *compatibleFormats;
};

bool isAllowedImageToImageAllFormatsColorSrcFormatTests(const CopyColorTestParams &testParams)
{
    bool result = true;

    if (testParams.params.allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        DE_ASSERT(!dedicatedAllocationImageToImageFormatsToTestSet.empty());

        result = de::contains(dedicatedAllocationImageToImageFormatsToTestSet, testParams.params.dst.image.format) ||
                 de::contains(dedicatedAllocationImageToImageFormatsToTestSet, testParams.params.src.image.format);
    }

    return result;
}

class CopyImageToImage final : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    CopyImageToImage(Context &context, TestParams params);
    tcu::TestStatus iterate(void) override;

private:
    tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess()) override;
    void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region,
                                  uint32_t mipLevel = 0u) override;

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
};

CopyImageToImage::CopyImageToImage(Context &context, TestParams params)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

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
            m_params.src.image.tiling,                                         // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
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
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            m_params.dst.image.tiling,                                         // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, m_device, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::Any,
                                                *m_allocator, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus CopyImageToImage::iterate(void)
{
    const bool srcCompressed = isCompressedFormat(m_params.src.image.format);
    const bool dstCompressed = isCompressedFormat(m_params.dst.image.format);

    const tcu::TextureFormat srcTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.dst.image.format);

    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width, (int)m_params.src.image.extent.height,
                              (int)m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth,
                   m_params.clearDestinationWithRed ? FILL_MODE_RED : m_params.dst.image.fillMode);
    generateExpectedResult();

    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image, m_params.useGeneralLayout);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image,
                m_params.useGeneralLayout);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_device;

    VkQueue queue                    = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf           = VK_NULL_HANDLE;
    VkCommandPool cmdpool            = VK_NULL_HANDLE;
    std::tie(queue, cmdbuf, cmdpool) = activeExecutionCtx();

    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        VkImageCopy imageCopy = m_params.regions[i].imageCopy;

        // When copying between compressed and uncompressed formats the extent
        // members represent the texel dimensions of the source image.
        if (srcCompressed)
        {
            const uint32_t blockWidth  = getBlockWidth(m_params.src.image.format);
            const uint32_t blockHeight = getBlockHeight(m_params.src.image.format);

            imageCopy.srcOffset.x *= blockWidth;
            imageCopy.extent.width *= blockWidth;

            // VUID-vkCmdCopyImage-srcImage-00146
            if (m_params.src.image.imageType != vk::VK_IMAGE_TYPE_1D)
            {
                imageCopy.srcOffset.y *= blockHeight;
                imageCopy.extent.height *= blockHeight;
            }
        }

        if (dstCompressed)
        {
            const uint32_t blockWidth  = getBlockWidth(m_params.dst.image.format);
            const uint32_t blockHeight = getBlockHeight(m_params.dst.image.format);

            imageCopy.dstOffset.x *= blockWidth;

            // VUID-vkCmdCopyImage-dstImage-00152
            if (m_params.dst.image.imageType != vk::VK_IMAGE_TYPE_1D)
            {
                imageCopy.dstOffset.y *= blockHeight;
            }
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            imageCopies.push_back(imageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
        }
    }

    VkMemoryBarrier memoryBarriers[] = {
        // source image
        {makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)},
        // destination image
        {makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT)},
    };

    VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
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
             getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.src.image) // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
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
             getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.dst.image) // uint32_t arraySize;
         }},
    };

    VkCommandBuffer recordingBuf = cmdbuf;
    if (m_params.useSecondaryCmdBuffer)
    {
        beginSecondaryCommandBuffer(vk, *m_secondaryCmdBuffer);
        recordingBuf = *m_secondaryCmdBuffer;
    }
    else
    {
        beginCommandBuffer(vk, cmdbuf);
    }

    vk.cmdPipelineBarrier(recordingBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, (m_params.useGeneralLayout ? DE_LENGTH_OF_ARRAY(memoryBarriers) : 0),
                          memoryBarriers, 0, nullptr,
                          (m_params.useGeneralLayout ? 0 : DE_LENGTH_OF_ARRAY(imageBarriers)), imageBarriers);

    if (m_params.clearDestinationWithRed)
    {
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
        VkClearColorValue clearColor;

        clearColor.float32[0] = 1.0f;
        clearColor.float32[1] = 0.0f;
        clearColor.float32[2] = 0.0f;
        clearColor.float32[3] = 1.0f;
        vk.cmdClearColorImage(recordingBuf, m_destination.get(),
                              m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL :
                                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              &clearColor, 1u, &range);
        imageBarriers[0].oldLayout = imageBarriers[0].newLayout;
        imageBarriers[1].oldLayout = imageBarriers[1].newLayout;
        vk.cmdPipelineBarrier(
            recordingBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
            (m_params.useGeneralLayout ? DE_LENGTH_OF_ARRAY(memoryBarriers) : 0), memoryBarriers, 0, nullptr,
            (m_params.useGeneralLayout ? 0 : DE_LENGTH_OF_ARRAY(imageBarriers)), imageBarriers);
    }

    const VkImageLayout srcLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : m_params.src.image.operationLayout;
    const VkImageLayout dstLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : m_params.dst.image.operationLayout;
    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyImage(recordingBuf, m_source.get(), srcLayout, m_destination.get(), dstLayout,
                        (uint32_t)imageCopies.size(), imageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageInfo2KHR copyImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            m_source.get(),                          // VkImage srcImage;
            srcLayout,                               // VkImageLayout srcImageLayout;
            m_destination.get(),                     // VkImage dstImage;
            dstLayout,                               // VkImageLayout dstImageLayout;
            (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
            imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
        };

        vk.cmdCopyImage2(recordingBuf, &copyImageInfo2KHR);
    }

    endCommandBuffer(vk, recordingBuf);

    if (m_params.useSecondaryCmdBuffer)
    {
        beginCommandBuffer(vk, cmdbuf);
        vk.cmdExecuteCommands(cmdbuf, 1, &recordingBuf);
        endCommandBuffer(vk, cmdbuf);
    }

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, cmdbuf, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, cmdpool);

    if (m_params.useSecondaryCmdBuffer)
        m_context.resetCommandPoolForVKSC(vkDevice, *m_secondaryCmdPool);

    de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(*m_destination, m_params.dst.image);

    return checkTestResult(resultTextureLevel->getAccess());
}

tcu::TestStatus CopyImageToImage::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::Vec4 fThreshold(0.0f);
    const tcu::UVec4 uThreshold(0u);

    if (tcu::isCombinedDepthStencilType(result.getFormat().type))
    {
        if (tcu::hasDepthComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode     = tcu::Sampler::MODE_DEPTH;
            const tcu::ConstPixelBufferAccess depthResult = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult =
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }

        if (tcu::hasStencilComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode       = tcu::Sampler::MODE_STENCIL;
            const tcu::ConstPixelBufferAccess stencilResult = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult =
                tcu::getEffectiveDepthStencilAccess(m_expectedTextureLevel[0]->getAccess(), mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }
    }
    else
    {
        if (!tcu::bitwiseCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                 m_expectedTextureLevel[0]->getAccess(), result, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("CopiesAndBlitting test");
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopyImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    VkOffset3D srcOffset = region.imageCopy.srcOffset;
    VkOffset3D dstOffset = region.imageCopy.dstOffset;
    VkExtent3D extent    = region.imageCopy.extent;

    if (region.imageCopy.dstSubresource.baseArrayLayer > region.imageCopy.srcSubresource.baseArrayLayer)
    {
        dstOffset.z  = srcOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
    }

    if (region.imageCopy.dstSubresource.baseArrayLayer < region.imageCopy.srcSubresource.baseArrayLayer)
    {
        srcOffset.z  = dstOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());

        // Copy depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            tcu::copy(dstSubRegion, srcSubRegion);
        }

        // Copy stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
    else
    {
        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
        // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
        const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
        const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
            dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

        tcu::copy(dstSubRegion, srcSubRegion);
    }
}

class CopyImageToImageTestCase : public vkt::TestCase
{
public:
    CopyImageToImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyImageToImage(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.allocationKind == ALLOCATION_KIND_DEDICATED)
        {
            if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
                TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
        }

#ifndef CTS_USES_VULKANSC
        if (m_params.src.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.dst.image.format == VK_FORMAT_A8_UNORM_KHR ||
            m_params.src.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR ||
            m_params.dst.image.format == VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR)
            context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

        checkExtensionSupport(context, m_params.extensionFlags);

        const VkPhysicalDeviceLimits &limits = context.getDeviceProperties().limits;
        VkImageFormatProperties properties;

        VkImageCreateFlags srcCreateFlags = getCreateFlags(m_params.src.image);
        if (m_params.useSparseBinding)
            srcCreateFlags |= (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);

        const auto dstCreateFlags = getCreateFlags(m_params.dst.image);
        // Sparse is not used for the dst image.

        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                 m_params.src.image.tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, srcCreateFlags,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
            (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                 m_params.dst.image.tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT, dstCreateFlags,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        // Check maxImageDimension1D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.src.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D src image dimensions not supported");

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.dst.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D dst image dimensions not supported");
        }

        // Check maxImageDimension2D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.src.image.extent.width > limits.maxImageDimension2D ||
                 m_params.src.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension2D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D dst image dimensions not supported");
            }
        }

        // Check maxImageDimension3D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.src.image.extent.width > limits.maxImageDimension3D ||
                 m_params.src.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension3D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D dst image dimensions not supported");
            }
        }

        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
        {
            for (const auto &res : {m_params.src, m_params.dst})
                checkTransferQueueGranularity(context, res.image.extent, res.image.imageType);
            for (const auto &region : m_params.regions)
            {
                checkTransferQueueGranularity(context, region.imageCopy.extent, m_params.src.image.imageType);
                checkTransferQueueGranularity(context, region.imageCopy.extent, m_params.dst.image.imageType);
            }
        }
    }

private:
    TestParams m_params;
};

class CopyImageToImageMipmap : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    CopyImageToImageMipmap(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

protected:
    tcu::TestStatus checkResult(tcu::ConstPixelBufferAccess result, tcu::ConstPixelBufferAccess expected);

private:
    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;

    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
};

CopyImageToImageMipmap::CopyImageToImageMipmap(Context &context, TestParams params)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source image
    {
        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            getExtent3D(m_params.src.image),                                   // VkExtent3D extent;
            params.mipLevels,                                                  // uint32_t mipLevels;
            getArraySize(m_params.src.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
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
            params.mipLevels,                                                  // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_destination           = createImage(vk, m_device, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::Any,
                                                *m_allocator, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus CopyImageToImageMipmap::iterate(void)
{
    const tcu::TextureFormat srcTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = getSizeCompatibleTcuTextureFormat(m_params.dst.image.format);

    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width, (int)m_params.src.image.extent.height,
                              (int)m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image, m_params.useGeneralLayout,
                m_params.mipLevels);

    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth, FILL_MODE_RED);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image,
                m_params.useGeneralLayout, m_params.mipLevels);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        VkImageCopy imageCopy = m_params.regions[i].imageCopy;
        uint32_t blockWidth, blockHeight;
        std::tie(blockWidth, blockHeight) = m_params.src.image.texelBlockDimensions();
        if (blockWidth != 1 || blockHeight != 1)
        {
            imageCopy.srcOffset.x *= blockWidth;
            imageCopy.srcOffset.y *= blockHeight;
            // When copying between compressed and uncompressed formats the extent
            // members represent the texel dimensions of the source image.
            imageCopy.extent.width *= blockWidth;
            imageCopy.extent.height *= blockHeight;
        }

        std::tie(blockWidth, blockHeight) = m_params.dst.image.texelBlockDimensions();
        if (blockWidth != 1 || blockHeight != 1)
        {
            imageCopy.dstOffset.x *= blockWidth;
            imageCopy.dstOffset.y *= blockHeight;
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            imageCopies.push_back(imageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(imageCopy));
        }
    }

    VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
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
             getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             m_params.mipLevels,              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.src.image) // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
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
             getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             m_params.mipLevels,              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.dst.image) // uint32_t arraySize;
         }},
    };

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, DE_LENGTH_OF_ARRAY(imageBarriers),
                          imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyImage(commandBuffer, m_source.get(), m_params.src.image.operationLayout, m_destination.get(),
                        m_params.dst.image.operationLayout, (uint32_t)imageCopies.size(), imageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageInfo2KHR copyImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            m_source.get(),                          // VkImage srcImage;
            m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
            m_destination.get(),                     // VkImage dstImage;
            m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
            (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
            imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
        };

        vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHR);
    }

    endCommandBuffer(vk, commandBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    for (uint32_t miplevel = 0; miplevel < m_params.mipLevels; miplevel++)
    {
        de::MovePtr<tcu::TextureLevel> resultTextureLevel   = readImage(*m_destination, m_params.dst.image, miplevel);
        de::MovePtr<tcu::TextureLevel> expectedTextureLevel = readImage(*m_source, m_params.src.image, miplevel);

        tcu::TestStatus result = checkResult(resultTextureLevel->getAccess(), expectedTextureLevel->getAccess());
        if (result.getCode() != QP_TEST_RESULT_PASS)
            return result;
    }
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus CopyImageToImageMipmap::checkResult(tcu::ConstPixelBufferAccess result,
                                                    tcu::ConstPixelBufferAccess expected)
{
    const tcu::Vec4 fThreshold(0.0f);
    const tcu::UVec4 uThreshold(0u);

    if (tcu::isCombinedDepthStencilType(result.getFormat().type))
    {
        if (tcu::hasDepthComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode        = tcu::Sampler::MODE_DEPTH;
            const tcu::ConstPixelBufferAccess depthResult    = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult = tcu::getEffectiveDepthStencilAccess(expected, mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, depthResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, depthResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }

        if (tcu::hasStencilComponent(result.getFormat().order))
        {
            const tcu::Sampler::DepthStencilMode mode        = tcu::Sampler::MODE_STENCIL;
            const tcu::ConstPixelBufferAccess stencilResult  = tcu::getEffectiveDepthStencilAccess(result, mode);
            const tcu::ConstPixelBufferAccess expectedResult = tcu::getEffectiveDepthStencilAccess(expected, mode);

            if (isFloatFormat(result.getFormat()))
            {
                if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                                expectedResult, stencilResult, fThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
            else
            {
                if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                              expectedResult, stencilResult, uThreshold, tcu::COMPARE_LOG_RESULT))
                    return tcu::TestStatus::fail("CopiesAndBlitting test");
            }
        }
    }
    else
    {
        if (isFloatFormat(result.getFormat()))
        {
            if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                            expected, result, fThreshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
        else if (isSnormFormat(mapTextureFormat(result.getFormat())))
        {
            // There may be an ambiguity between two possible binary representations of 1.0.
            // Get rid of that by expanding the data to floats and re-normalizing again.

            tcu::TextureLevel resultSnorm(result.getFormat(), result.getWidth(), result.getHeight(), result.getDepth());
            {
                tcu::TextureLevel resultFloat(
                    tcu::TextureFormat(resultSnorm.getFormat().order, tcu::TextureFormat::FLOAT),
                    resultSnorm.getWidth(), resultSnorm.getHeight(), resultSnorm.getDepth());

                tcu::copy(resultFloat.getAccess(), result);
                tcu::copy(resultSnorm, resultFloat.getAccess());
            }

            tcu::TextureLevel expectedSnorm(expected.getFormat(), expected.getWidth(), expected.getHeight(),
                                            expected.getDepth());

            {
                tcu::TextureLevel expectedFloat(
                    tcu::TextureFormat(expectedSnorm.getFormat().order, tcu::TextureFormat::FLOAT),
                    expectedSnorm.getWidth(), expectedSnorm.getHeight(), expectedSnorm.getDepth());

                tcu::copy(expectedFloat.getAccess(), m_expectedTextureLevel[0]->getAccess());
                tcu::copy(expectedSnorm, expectedFloat.getAccess());
            }

            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                          expectedSnorm.getAccess(), resultSnorm.getAccess(), uThreshold,
                                          tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
        else
        {
            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", expected,
                                          result, uThreshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void CopyImageToImageMipmap::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                      CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    VkOffset3D srcOffset = region.imageCopy.srcOffset;
    VkOffset3D dstOffset = region.imageCopy.dstOffset;
    VkExtent3D extent    = region.imageCopy.extent;

    if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D && m_params.dst.image.imageType == VK_IMAGE_TYPE_2D)
    {
        dstOffset.z  = srcOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.dstSubresource.layerCount);
    }
    if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D && m_params.dst.image.imageType == VK_IMAGE_TYPE_3D)
    {
        srcOffset.z  = dstOffset.z;
        extent.depth = std::max(region.imageCopy.extent.depth, region.imageCopy.srcSubresource.layerCount);
    }

    if (tcu::isCombinedDepthStencilType(src.getFormat().type))
    {
        DE_ASSERT(src.getFormat() == dst.getFormat());

        // Copy depth.
        if (tcu::hasDepthComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_DEPTH);
            tcu::copy(dstSubRegion, srcSubRegion);
        }

        // Copy stencil.
        if (tcu::hasStencilComponent(src.getFormat().order))
        {
            const tcu::ConstPixelBufferAccess srcSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            const tcu::PixelBufferAccess dstSubRegion =
                getEffectiveDepthStencilAccess(tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z,
                                                                 extent.width, extent.height, extent.depth),
                                               tcu::Sampler::MODE_STENCIL);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
    else
    {
        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
        // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
        const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
        const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
            dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

        tcu::copy(dstSubRegion, srcSubRegion);
    }
}

class CopyImageToImageMipmapTestCase : public vkt::TestCase
{
public:
    CopyImageToImageMipmapTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyImageToImageMipmap(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.allocationKind == ALLOCATION_KIND_DEDICATED)
        {
            if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
                TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        const VkPhysicalDeviceLimits limits = context.getDeviceProperties().limits;
        VkImageFormatProperties properties;

        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED) ||
            (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        // Check maxImageDimension1D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.src.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D src image dimensions not supported");

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_1D &&
                m_params.dst.image.extent.width > limits.maxImageDimension1D)
                TCU_THROW(NotSupportedError, "Requested 1D dst image dimensions not supported");
        }

        // Check maxImageDimension2D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.src.image.extent.width > limits.maxImageDimension2D ||
                 m_params.src.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_2D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension2D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension2D))
            {
                TCU_THROW(NotSupportedError, "Requested 2D dst image dimensions not supported");
            }
        }

        // Check maxImageDimension3D
        {
            if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.src.image.extent.width > limits.maxImageDimension3D ||
                 m_params.src.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D src image dimensions not supported");
            }

            if (m_params.dst.image.imageType == VK_IMAGE_TYPE_3D &&
                (m_params.dst.image.extent.width > limits.maxImageDimension3D ||
                 m_params.dst.image.extent.height > limits.maxImageDimension3D ||
                 m_params.src.image.extent.depth > limits.maxImageDimension3D))
            {
                TCU_THROW(NotSupportedError, "Requested 3D dst image dimensions not supported");
            }
        }

        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
            for (const auto &res : {m_params.src, m_params.dst})
                checkTransferQueueGranularity(context, res.image.extent, res.image.imageType);
    }

private:
    TestParams m_params;
};

void addImageToImageSimpleTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    for (const auto format : {VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT})
        for (const auto tiling : {VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR})
        {
            // Linear and sparse residency cannot be used together: VUID-VkImageCreateInfo-tiling-04121
            if (tiling == VK_IMAGE_TILING_LINEAR && testGroupParams->useSparseBinding)
                continue;

            TestParams params;
            params.src.image.imageType       = VK_IMAGE_TYPE_2D;
            params.src.image.format          = format;
            params.src.image.extent          = defaultExtent;
            params.src.image.tiling          = tiling;
            params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
            params.dst.image.format          = format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                    {0, 0, 0},          // VkOffset3D srcOffset;
                    defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                    {0, 0, 0},          // VkOffset3D dstOffset;
                    defaultExtent,      // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;
                params.regions.push_back(imageCopy);
            }

            std::string testName("whole_image");

            if (format != VK_FORMAT_R8G8B8A8_UINT)
                testName += "_" + getFormatCaseName(format);

            if (tiling == VK_IMAGE_TILING_LINEAR)
                testName += "_linear";

            group->addChild(new CopyImageToImageTestCase(testCtx, testName, params));
        }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R32_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        {
            const VkImageCopy testCopy = {
                defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},          // VkOffset3D srcOffset;
                defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},          // VkOffset3D dstOffset;
                defaultExtent,      // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "whole_image_diff_format", params));
    }

    for (const auto format : {VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT})
        for (const auto tiling : {VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR})
        {
            // Linear and sparse residency cannot be used together: VUID-VkImageCreateInfo-tiling-04121
            if (tiling == VK_IMAGE_TILING_LINEAR && testGroupParams->useSparseBinding)
                continue;

            TestParams params;
            params.src.image.imageType       = VK_IMAGE_TYPE_2D;
            params.src.image.format          = format;
            params.src.image.extent          = defaultExtent;
            params.src.image.tiling          = tiling;
            params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
            params.dst.image.format          = format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
                    {0, 0, 0},                                           // VkOffset3D srcOffset;
                    defaultSourceLayer,                                  // VkImageSubresourceLayers dstSubresource;
                    {defaultQuarterSize, defaultQuarterSize / 2, 0},     // VkOffset3D dstOffset;
                    {defaultQuarterSize / 2, defaultQuarterSize / 2, 1}, // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;
                params.regions.push_back(imageCopy);
            }

            std::string testName("partial_image");

            if (format != VK_FORMAT_R8G8B8A8_UINT)
                testName += "_" + getFormatCaseName(format);

            if (tiling == VK_IMAGE_TILING_LINEAR)
                testName += "_linear";

            group->addChild(new CopyImageToImageTestCase(testCtx, testName, params));
        }

    static const struct
    {
        std::string name;
        vk::VkFormat format1;
        vk::VkFormat format2;
    } formats[] = {{"diff_format", vk::VK_FORMAT_R32_UINT, vk::VK_FORMAT_R8G8B8A8_UNORM},
                   {"same_format", vk::VK_FORMAT_R8G8B8A8_UNORM, vk::VK_FORMAT_R8G8B8A8_UNORM}};
    static const struct
    {
        std::string name;
        vk::VkBool32 clear;
    } clears[] = {{"clear", VK_TRUE}, {"noclear", VK_FALSE}};
    static const struct
    {
        std::string name;
        VkExtent3D extent;
    } extents[] = {{"npot", {65u, 63u, 1u}}, {"pot", {64u, 64u, 1u}}};

    for (const auto &format : formats)
    {
        for (const auto &clear : clears)
        {
            if (testGroupParams->queueSelection == QueueSelectionOptions::TransferOnly)
                continue;

            for (const auto &extent : extents)
            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format.format1;
                params.src.image.extent          = extent.extent;
                params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
                params.dst.image.format          = format.format2;
                params.dst.image.extent          = extent.extent;
                params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
                params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;
                params.clearDestinationWithRed   = clear.clear;

                {
                    VkImageCopy testCopy = {
                        defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        {34, 34, 0},        // VkOffset3D srcOffset;
                        defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        {0, 0, 0},          // VkOffset3D dstOffset;
                        {31, 29, 1}         // VkExtent3D extent;
                    };

                    if (extent.name == "pot")
                    {
                        testCopy.srcOffset = {16, 16, 0};
                        testCopy.extent    = {32, 32, 1};
                    }

                    CopyRegion imageCopy;
                    imageCopy.imageCopy = testCopy;
                    params.regions.push_back(imageCopy);
                }

                // Example test case name: "partial_image_npot_diff_format_clear"
                const std::string testCaseName = "partial_image_" + extent.name + "_" + format.name + "_" + clear.name;

                group->addChild(new CopyImageToImageTestCase(testCtx, testCaseName, params));
            }
        }
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_D32_SFLOAT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_D32_SFLOAT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };
            const VkImageCopy testCopy = {
                sourceLayer,                                         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                           // VkOffset3D srcOffset;
                sourceLayer,                                         // VkImageSubresourceLayers dstSubresource;
                {defaultQuarterSize, defaultQuarterSize / 2, 0},     // VkOffset3D dstOffset;
                {defaultQuarterSize / 2, defaultQuarterSize / 2, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "depth", params));
    }

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_S8_UINT;
        params.src.image.extent          = defaultExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_S8_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSecondaryCmdBuffer     = testGroupParams->useSecondaryCmdBuffer;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_STENCIL_BIT, // VkImageAspectFlags aspectMask;
                0u,                          // uint32_t mipLevel;
                0u,                          // uint32_t baseArrayLayer;
                1u                           // uint32_t layerCount;
            };
            const VkImageCopy testCopy = {
                sourceLayer,                                         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                           // VkOffset3D srcOffset;
                sourceLayer,                                         // VkImageSubresourceLayers dstSubresource;
                {defaultQuarterSize, defaultQuarterSize / 2, 0},     // VkOffset3D dstOffset;
                {defaultQuarterSize / 2, defaultQuarterSize / 2, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "stencil", params));
    }
}

void addImageToImageAllFormatsColorSrcFormatDstFormatTests(tcu::TestCaseGroup *group, TestParamsPtr paramsPtr)
{
    const VkImageLayout copySrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout copyDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    TestParams params = *paramsPtr;
    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); ++srcLayoutNdx)
    {
        params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];

        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); ++dstLayoutNdx)
        {
            params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

            const std::string testName = getImageLayoutCaseName(params.src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(params.dst.image.operationLayout);
            const std::string description = "From layout " + getImageLayoutCaseName(params.src.image.operationLayout) +
                                            " to " + getImageLayoutCaseName(params.dst.image.operationLayout);

            group->addChild(new CopyImageToImageTestCase(group->getTestContext(), testName, params));
        }
    }
}

void addImageToImageAllFormatsColorSrcFormatTests(tcu::TestCaseGroup *group, CopyColorTestParams testParams)
{
    // If testParams.compatibleFormats is nullptr, the destination format will be copied from the source format.
    const std::vector<VkFormat> srcFormatOnly{testParams.params.src.image.format};
    const std::vector<VkFormat> &formatList =
        (testParams.compatibleFormats ? *testParams.compatibleFormats : srcFormatOnly);

    for (auto format : formatList)
    {
        testParams.params.dst.image.format = format;

        const VkFormat srcFormat = testParams.params.src.image.format;
        const VkFormat dstFormat = testParams.params.dst.image.format;

        if (!isSupportedByFramework(dstFormat) && !isCompressedFormat(dstFormat))
            continue;

        if (!isAllowedImageToImageAllFormatsColorSrcFormatTests(testParams))
            continue;

        if (isCompressedFormat(srcFormat) && isCompressedFormat(dstFormat))
            if ((getBlockWidth(srcFormat) != getBlockWidth(dstFormat)) ||
                (getBlockHeight(srcFormat) != getBlockHeight(dstFormat)))
                continue;

        TestParamsPtr paramsPtr(new TestParams(testParams.params));
        const std::string description = "Copy to destination format " + getFormatCaseName(dstFormat);
        addTestGroup(group, getFormatCaseName(dstFormat), addImageToImageAllFormatsColorSrcFormatDstFormatTests,
                     paramsPtr);
    }
}

#ifndef CTS_USES_VULKANSC
const std::vector<VkFormat> compatibleFormats8BitA{VK_FORMAT_A8_UNORM_KHR};
#endif // CTS_USES_VULKANSC

const std::vector<std::vector<VkFormat>> colorImageFormatsToTest{
#ifndef CTS_USES_VULKANSC
    compatibleFormats8BitA,
#endif // CTS_USES_VULKANSC
    formats::compatibleFormats8Bit,   formats::compatibleFormats16Bit,  formats::compatibleFormats24Bit,
    formats::compatibleFormats32Bit,  formats::compatibleFormats48Bit,  formats::compatibleFormats64Bit,
    formats::compatibleFormats96Bit,  formats::compatibleFormats128Bit, formats::compatibleFormats192Bit,
    formats::compatibleFormats256Bit,
};

const VkFormat dedicatedAllocationImageToImageFormatsToTest[] = {
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

void addImageToImageAllFormatsColorTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    if (testGroupParams->allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        const int numOfDedicatedAllocationImageToImageFormatsToTest =
            DE_LENGTH_OF_ARRAY(dedicatedAllocationImageToImageFormatsToTest);
        for (int compatibleFormatsIndex = 0; compatibleFormatsIndex < numOfDedicatedAllocationImageToImageFormatsToTest;
             ++compatibleFormatsIndex)
            dedicatedAllocationImageToImageFormatsToTestSet.insert(
                dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
    }

    // 1D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = default1dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;

        for (int32_t i = defaultQuarterSize; i < defaultSize; i += defaultSize / 2)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                  // VkOffset3D srcOffset;
                defaultSourceLayer,         // VkImageSubresourceLayers dstSubresource;
                {i, 0, 0},                  // VkOffset3D dstOffset;
                {defaultQuarterSize, 1, 1}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = defaultRootExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (uint32_t i = 0; i < defaultRootSize; ++i)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                // VkImageSubresourceLayers srcSubresource;
                {static_cast<int32_t>(defaultRootSize * i), 0, 0}, // VkOffset3D srcOffset;
                defaultSourceLayer,                                // VkImageSubresourceLayers dstSubresource;
                {0, static_cast<int32_t>(i), 0},                   // VkOffset3D dstOffset;
                {defaultRootSize, 1, 1},                           // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_1D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = default1dExtent;
        params.dst.image.extent    = default3dSmallExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            for (int32_t j = 0; j < defaultSixteenthSize; ++j)
            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
                    {i * defaultQuarterSize + j * defaultSixteenthSize, 0, 0}, // VkOffset3D srcOffset;
                    defaultSourceLayer,               // VkImageSubresourceLayers dstSubresource;
                    {0, j, i % defaultSixteenthSize}, // VkOffset3D dstOffset;
                    {defaultSixteenthSize, 1, 1},     // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;

                params.regions.push_back(imageCopy);
            }
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams;
                testParams.params            = params;
                testParams.compatibleFormats = nullptr;

                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = defaultQuarterExtent;
        params.dst.image.extent    = default1dQuarterSquaredExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (int32_t i = 0; i < defaultQuarterSize; ++i)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,             // VkImageSubresourceLayers srcSubresource;
                {0, i, 0},                      // VkOffset3D srcOffset;
                defaultSourceLayer,             // VkImageSubresourceLayers dstSubresource;
                {i * defaultQuarterSize, 0, 0}, // VkOffset3D dstOffset;
                {defaultQuarterSize, 1, 1},     // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams{params, &formatArray};
                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = defaultExtent;
        params.dst.image.extent    = defaultExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_RANDOM_GRAY;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;

        for (int32_t i = 0; i < defaultSize; i += defaultQuarterSize)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                           // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                    // VkOffset3D srcOffset;
                defaultSourceLayer,                           // VkImageSubresourceLayers dstSubresource;
                {i, defaultSize - i - defaultQuarterSize, 0}, // VkOffset3D dstOffset;
                {defaultQuarterSize, defaultQuarterSize, 1},  // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams{params, &formatArray};
                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_2D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = defaultExtent;
        params.dst.image.extent    = default3dSmallExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_RANDOM_GRAY;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_1;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                      // VkImageSubresourceLayers srcSubresource;
                {i * defaultSixteenthSize, i % defaultSixteenthSize, 0}, // VkOffset3D srcOffset;
                defaultSourceLayer,                                      // VkImageSubresourceLayers dstSubresource;
                {0, 0, i},                                               // VkOffset3D dstOffset;
                {defaultSixteenthSize, defaultSixteenthSize, 1},         // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams{params, &formatArray};
                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_1d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_1D;
        params.src.image.extent    = default3dSmallExtent;
        params.dst.image.extent    = default1dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_5;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            for (int32_t j = 0; j < defaultSixteenthSize; ++j)
            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer,                                      // VkImageSubresourceLayers srcSubresource;
                    {0, j % defaultSixteenthSize, i % defaultSixteenthSize}, // VkOffset3D srcOffset;
                    defaultSourceLayer,                                      // VkImageSubresourceLayers dstSubresource;
                    {j * defaultSixteenthSize + i * defaultQuarterSize, 0, 0}, // VkOffset3D dstOffset;
                    {defaultSixteenthSize, 1, 1},                              // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;

                params.regions.push_back(imageCopy);
            }
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams{params, nullptr};
                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_2d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        params.src.image.extent    = default3dExtent;
        params.dst.image.extent    = defaultExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;
        params.extensionFlags |= MAINTENANCE_1;

        for (int32_t i = 0; i < defaultSixteenthSize; ++i)
        {
            for (int32_t j = 0; j < defaultSixteenthSize; ++j)
            {
                const VkImageCopy testCopy = {
                    defaultSourceLayer,                                  // VkImageSubresourceLayers srcSubresource;
                    {0, 0, i * defaultSixteenthSize + j},                // VkOffset3D srcOffset;
                    defaultSourceLayer,                                  // VkImageSubresourceLayers dstSubresource;
                    {j * defaultQuarterSize, i * defaultQuarterSize, 0}, // VkOffset3D dstOffset;
                    {defaultQuarterSize, defaultQuarterSize, 1},         // VkExtent3D extent;
                };

                CopyRegion imageCopy;
                imageCopy.imageCopy = testCopy;

                params.regions.push_back(imageCopy);
            }
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams{params, nullptr};
                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_3d"));

        TestParams params;
        params.src.image.imageType = VK_IMAGE_TYPE_3D;
        params.dst.image.imageType = VK_IMAGE_TYPE_3D;
        params.src.image.extent    = default3dExtent;
        params.dst.image.extent    = default3dExtent;
        params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.fillMode  = FILL_MODE_WHITE;
        params.dst.image.fillMode  = FILL_MODE_GRADIENT;
        params.allocationKind      = testGroupParams->allocationKind;
        params.extensionFlags      = testGroupParams->extensionFlags;
        params.queueSelection      = testGroupParams->queueSelection;

        for (int32_t i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
        {
            const VkImageCopy testCopy = {
                defaultSourceLayer,                                    // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                                             // VkOffset3D srcOffset;
                defaultSourceLayer,                                    // VkImageSubresourceLayers dstSubresource;
                {i, defaultQuarterSize - i - defaultSixteenthSize, i}, // VkOffset3D dstOffset;
                {defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize}, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params.regions.push_back(imageCopy);
        }

        for (const auto &formatArray : colorImageFormatsToTest)
        {
            for (auto format : formatArray)
            {
                params.src.image.format = format;
                if (!isSupportedByFramework(params.src.image.format) && !isCompressedFormat(params.src.image.format))
                    continue;

                CopyColorTestParams testParams{params, nullptr};
                const std::string testName = getFormatCaseName(params.src.image.format);
                addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsColorSrcFormatTests, testParams);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addImageToImageDimensionsTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const std::vector<std::vector<VkFormat>> testFormats{// From compatibleFormats8Bit
                                                         {VK_FORMAT_R4G4_UNORM_PACK8, VK_FORMAT_R8_SRGB},
                                                         // From compatibleFormats16Bit
                                                         {
                                                             VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                                                             VK_FORMAT_R16_SFLOAT,
                                                         },
                                                         // From compatibleFormats24Bit
                                                         {VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_B8G8R8_SRGB},
                                                         // From compatibleFormats32Bit
                                                         {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_SFLOAT},
                                                         // From compatibleFormats48Bit
                                                         {VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16_SFLOAT},
                                                         // From compatibleFormats64Bit
                                                         {VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_R64_SFLOAT},
                                                         // From compatibleFormats96Bit
                                                         {VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT},
                                                         // From compatibleFormats128Bit
                                                         {VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R64G64_SFLOAT},
                                                         // From compatibleFormats192Bit
                                                         {
                                                             VK_FORMAT_R64G64B64_UINT,
                                                             VK_FORMAT_R64G64B64_SFLOAT,
                                                         },
                                                         // From compatibleFormats256Bit
                                                         {VK_FORMAT_R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_SFLOAT}};

    const tcu::UVec2 imageDimensions[] = {
        // large pot x small pot
        tcu::UVec2(4096, 4u), tcu::UVec2(8192, 4u), tcu::UVec2(16384, 4u), tcu::UVec2(32768, 4u),

        // large pot x small npot
        tcu::UVec2(4096, 6u), tcu::UVec2(8192, 6u), tcu::UVec2(16384, 6u), tcu::UVec2(32768, 6u),

        // small pot x large pot
        tcu::UVec2(4u, 4096), tcu::UVec2(4u, 8192), tcu::UVec2(4u, 16384), tcu::UVec2(4u, 32768),

        // small npot x large pot
        tcu::UVec2(6u, 4096), tcu::UVec2(6u, 8192), tcu::UVec2(6u, 16384), tcu::UVec2(6u, 32768)};

    const VkImageLayout copySrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    const VkImageLayout copyDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    if (testGroupParams->allocationKind == ALLOCATION_KIND_DEDICATED)
    {
        for (int compatibleFormatsIndex = 0;
             compatibleFormatsIndex < DE_LENGTH_OF_ARRAY(dedicatedAllocationImageToImageFormatsToTest);
             compatibleFormatsIndex++)
            dedicatedAllocationImageToImageFormatsToTestSet.insert(
                dedicatedAllocationImageToImageFormatsToTest[compatibleFormatsIndex]);
    }

    // Image dimensions
    for (size_t dimensionNdx = 0; dimensionNdx < DE_LENGTH_OF_ARRAY(imageDimensions); dimensionNdx++)
    {
        CopyRegion copyRegion;
        CopyColorTestParams testParams;

        const VkExtent3D extent = {imageDimensions[dimensionNdx].x(), imageDimensions[dimensionNdx].y(), 1};

        const VkImageCopy testCopy = {
            defaultSourceLayer, // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},          // VkOffset3D srcOffset;
            defaultSourceLayer, // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},          // VkOffset3D dstOffset;
            extent,             // VkExtent3D extent;
        };

        testParams.params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        testParams.params.src.image.imageType = VK_IMAGE_TYPE_2D;
        testParams.params.src.image.extent    = extent;
        testParams.params.src.image.fillMode  = FILL_MODE_PYRAMID;

        testParams.params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
        testParams.params.dst.image.imageType = VK_IMAGE_TYPE_2D;
        testParams.params.dst.image.extent    = extent;

        copyRegion.imageCopy               = testCopy;
        testParams.params.allocationKind   = testGroupParams->allocationKind;
        testParams.params.extensionFlags   = testGroupParams->extensionFlags;
        testParams.params.queueSelection   = testGroupParams->queueSelection;
        testParams.params.useSparseBinding = testGroupParams->useSparseBinding;
        testParams.params.useGeneralLayout = testGroupParams->useGeneralLayout;

        testParams.params.regions.push_back(copyRegion);

        const std::string dimensionStr = "src" + de::toString(testParams.params.src.image.extent.width) + "x" +
                                         de::toString(testParams.params.src.image.extent.height) + "_dst" +
                                         de::toString(testParams.params.dst.image.extent.width) + "x" +
                                         de::toString(testParams.params.dst.image.extent.height);
        tcu::TestCaseGroup *imageSizeGroup = new tcu::TestCaseGroup(testCtx, dimensionStr.c_str());

        // Compatible formats for copying
        for (auto &compatibleFormats : testFormats)
        {
            testParams.compatibleFormats = &compatibleFormats;

            // Source image format
            for (auto srcFormat : compatibleFormats)
            {
                if (!isSupportedByFramework(srcFormat) && !isCompressedFormat(srcFormat))
                    continue;

                testParams.params.src.image.format = srcFormat;
                auto *srcFormatGroup = new tcu::TestCaseGroup(testCtx, getFormatCaseName(srcFormat).c_str());

                // Destination image format
                for (auto dstFormat : compatibleFormats)
                {
                    if (!isSupportedByFramework(dstFormat) && !isCompressedFormat(dstFormat))
                        continue;

                    if (!isAllowedImageToImageAllFormatsColorSrcFormatTests(testParams))
                        continue;

                    if (isCompressedFormat(srcFormat) && isCompressedFormat(dstFormat))
                    {
                        if ((getBlockWidth(srcFormat) != getBlockWidth(dstFormat)) ||
                            (getBlockHeight(srcFormat) != getBlockHeight(dstFormat)))
                            continue;
                    }

                    testParams.params.dst.image.format = dstFormat;
                    auto *dstFormatGroup = new tcu::TestCaseGroup(testCtx, getFormatCaseName(dstFormat).c_str());

                    // Source/destionation image layouts
                    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); srcLayoutNdx++)
                    {
                        testParams.params.src.image.operationLayout = copySrcLayouts[srcLayoutNdx];

                        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); dstLayoutNdx++)
                        {
                            testParams.params.dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

                            const std::string testName =
                                getImageLayoutCaseName(testParams.params.src.image.operationLayout) + "_" +
                                getImageLayoutCaseName(testParams.params.dst.image.operationLayout);
                            const TestParams params = testParams.params;

                            dstFormatGroup->addChild(new CopyImageToImageTestCase(testCtx, testName, params));
                        }
                    }

                    srcFormatGroup->addChild(dstFormatGroup);
                }

                imageSizeGroup->addChild(srcFormatGroup);
            }
        }

        group->addChild(imageSizeGroup);
    }
}

void addImageToImageAllFormatsDepthStencilFormatsTests(tcu::TestCaseGroup *group, TestParamsPtr params)
{
    const VkImageLayout copySrcLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout copyDstLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (int srcLayoutNdx = 0u; srcLayoutNdx < DE_LENGTH_OF_ARRAY(copySrcLayouts); ++srcLayoutNdx)
    {
        params->src.image.operationLayout = copySrcLayouts[srcLayoutNdx];
        for (int dstLayoutNdx = 0u; dstLayoutNdx < DE_LENGTH_OF_ARRAY(copyDstLayouts); ++dstLayoutNdx)
        {
            params->dst.image.operationLayout = copyDstLayouts[dstLayoutNdx];

            const std::string testName = getImageLayoutCaseName(params->src.image.operationLayout) + "_" +
                                         getImageLayoutCaseName(params->dst.image.operationLayout);
            TestParams testParams = *params;
            group->addChild(new CopyImageToImageTestCase(group->getTestContext(), testName, testParams));
        }
    }
}

void addImageToImageAllFormatsDepthStencilTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    // 1D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_1d"));

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
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.queueSelection      = testGroupParams->queueSelection;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = defaultQuarterSize; i < defaultSize; i += defaultSize / 2)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, 0, 0};
                const VkOffset3D dstOffset = {i, 0, 0};
                const VkExtent3D extent    = {defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_2d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = defaultRootExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (uint32_t i = 0; i < defaultRootSize; ++i)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {static_cast<int32_t>(i * defaultRootSize), 0, 0};
                const VkOffset3D dstOffset = {0, static_cast<int32_t>(i), 0};
                const VkExtent3D extent    = {defaultRootSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 1D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "1d_to_3d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_1D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = default1dExtent;
            params.dst.image.extent    = default3dSmallExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                for (int32_t j = 0; j < defaultSixteenthSize; ++j)
                {
                    CopyRegion copyRegion;
                    const VkOffset3D srcOffset = {i * defaultQuarterSize + j * defaultSixteenthSize, 0, 0};
                    const VkOffset3D dstOffset = {0, j, i};
                    const VkExtent3D extent    = {defaultSixteenthSize, 1, 1};

                    if (hasDepth)
                    {
                        const VkImageCopy testCopy = {
                            defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,               // VkOffset3D srcOffset;
                            defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,               // VkOffset3D dstOffset;
                            extent,                  // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                    if (hasStencil)
                    {
                        const VkImageCopy testCopy = {
                            defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,                 // VkOffset3D srcOffset;
                            defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,                 // VkOffset3D dstOffset;
                            extent,                    // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_1d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = defaultQuarterExtent;
            params.dst.image.extent    = default1dQuarterSquaredExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultDSSourceLayer      = {
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultQuarterSize; ++i)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, i, 0};
                const VkOffset3D dstOffset = {i * defaultQuarterSize, 0, 0};
                const VkExtent3D extent    = {defaultQuarterSize, 1, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);

                // DS Image copy
                {
                    params.extensionFlags &= ~SEPARATE_DEPTH_STENCIL_LAYOUT;
                    // Clear previous vkImageCopy elements
                    params.regions.clear();

                    for (int32_t i = 0; i < defaultQuarterSize; ++i)
                    {
                        CopyRegion copyRegion;
                        const VkOffset3D srcOffset = {0, i, 0};
                        const VkOffset3D dstOffset = {i * defaultQuarterSize, 0, 0};
                        const VkExtent3D extent    = {defaultQuarterSize, 1, 1};

                        const VkImageCopy testCopy = {
                            defaultDSSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,            // VkOffset3D srcOffset;
                            defaultDSSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,            // VkOffset3D dstOffset;
                            extent,               // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }

                    const std::string testName3 = getFormatCaseName(params.src.image.format) + "_" +
                                                  getFormatCaseName(params.dst.image.format) + "_depth_stencil_aspects";
                    TestParamsPtr paramsPtr3(new TestParams(params));
                    addTestGroup(subGroup.get(), testName3, addImageToImageAllFormatsDepthStencilFormatsTests,
                                 paramsPtr3);
                }
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_2d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = defaultExtent;
            params.dst.image.extent    = defaultExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultDSSourceLayer      = {
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSize; i += defaultQuarterSize)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, 0, 0};
                const VkOffset3D dstOffset = {i, defaultSize - i - defaultQuarterSize, 0};
                const VkExtent3D extent    = {defaultQuarterSize, defaultQuarterSize, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);

                // DS Image copy
                {
                    params.extensionFlags &= ~SEPARATE_DEPTH_STENCIL_LAYOUT;
                    // Clear previous vkImageCopy elements
                    params.regions.clear();

                    for (int32_t i = 0; i < defaultSize; i += defaultQuarterSize)
                    {
                        CopyRegion copyRegion;
                        const VkOffset3D srcOffset = {0, 0, 0};
                        const VkOffset3D dstOffset = {i, defaultSize - i - defaultQuarterSize, 0};
                        const VkExtent3D extent    = {defaultQuarterSize, defaultQuarterSize, 1};

                        const VkImageCopy testCopy = {
                            defaultDSSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,            // VkOffset3D srcOffset;
                            defaultDSSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,            // VkOffset3D dstOffset;
                            extent,               // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }

                    const std::string testName3 = getFormatCaseName(params.src.image.format) + "_" +
                                                  getFormatCaseName(params.dst.image.format) + "_depth_stencil_aspects";
                    TestParamsPtr paramsPtr3(new TestParams(params));
                    addTestGroup(subGroup.get(), testName3, addImageToImageAllFormatsDepthStencilFormatsTests,
                                 paramsPtr3);
                }
            }
        }

        group->addChild(subGroup.release());
    }

    // 2D to 3D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "2d_to_3d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_2D;
            params.dst.image.imageType = VK_IMAGE_TYPE_3D;
            params.src.image.extent    = defaultExtent;
            params.dst.image.extent    = default3dSmallExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.extensionFlags |= MAINTENANCE_1;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultDSSourceLayer      = {
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {i * defaultSixteenthSize, i % defaultSixteenthSize, 0};
                const VkOffset3D dstOffset = {0, 0, i};
                const VkExtent3D extent    = {defaultSixteenthSize, defaultSixteenthSize, 1};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);

                // DS Image copy
                {
                    params.extensionFlags &= ~SEPARATE_DEPTH_STENCIL_LAYOUT;
                    // Clear previous vkImageCopy elements
                    params.regions.clear();

                    for (int32_t i = 0; i < defaultSixteenthSize; ++i)
                    {
                        CopyRegion copyRegion;
                        const VkOffset3D srcOffset = {i * defaultSixteenthSize, i % defaultSixteenthSize, 0};
                        const VkOffset3D dstOffset = {0, 0, i};
                        const VkExtent3D extent    = {defaultSixteenthSize, defaultSixteenthSize, 1};

                        const VkImageCopy testCopy = {
                            defaultDSSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,            // VkOffset3D srcOffset;
                            defaultDSSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,            // VkOffset3D dstOffset;
                            extent,               // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }

                    const std::string testName3 = getFormatCaseName(params.src.image.format) + "_" +
                                                  getFormatCaseName(params.dst.image.format) + "_depth_stencil_aspects";
                    TestParamsPtr paramsPtr3(new TestParams(params));
                    addTestGroup(subGroup.get(), testName3, addImageToImageAllFormatsDepthStencilFormatsTests,
                                 paramsPtr3);
                }
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 1D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_1d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_1D;
            params.src.image.extent    = default3dSmallExtent;
            params.dst.image.extent    = default1dExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.extensionFlags |= MAINTENANCE_5;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                for (int32_t j = 0; j < defaultSixteenthSize; ++j)
                {
                    CopyRegion copyRegion;
                    const VkOffset3D srcOffset = {0, j % defaultSixteenthSize, i % defaultSixteenthSize};
                    const VkOffset3D dstOffset = {j * defaultSixteenthSize + i * defaultQuarterSize, 0, 0};
                    const VkExtent3D extent    = {defaultSixteenthSize, 1, 1};

                    if (hasDepth)
                    {
                        const VkImageCopy testCopy = {
                            defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,               // VkOffset3D srcOffset;
                            defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,               // VkOffset3D dstOffset;
                            extent,                  // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                    if (hasStencil)
                    {
                        const VkImageCopy testCopy = {
                            defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,                 // VkOffset3D srcOffset;
                            defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,                 // VkOffset3D dstOffset;
                            extent,                    // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D to 2D tests.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_2d"));

        for (const VkFormat format : formats::depthAndStencilFormats)
        {
            TestParams params;
            params.src.image.imageType = VK_IMAGE_TYPE_3D;
            params.dst.image.imageType = VK_IMAGE_TYPE_2D;
            params.src.image.extent    = default3dExtent;
            params.dst.image.extent    = defaultExtent;
            params.src.image.format    = format;
            params.dst.image.format    = params.src.image.format;
            params.src.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.tiling    = VK_IMAGE_TILING_OPTIMAL;
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;
            params.extensionFlags |= MAINTENANCE_1;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultSixteenthSize; ++i)
            {
                for (int32_t j = 0; j < defaultSixteenthSize; ++j)
                {
                    CopyRegion copyRegion;
                    const VkOffset3D srcOffset = {0, 0, i % defaultSixteenthSize + j};
                    const VkOffset3D dstOffset = {j * defaultQuarterSize, i * defaultQuarterSize, 0};
                    const VkExtent3D extent    = {defaultQuarterSize, defaultQuarterSize, 1};

                    if (hasDepth)
                    {
                        const VkImageCopy testCopy = {
                            defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,               // VkOffset3D srcOffset;
                            defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,               // VkOffset3D dstOffset;
                            extent,                  // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                    if (hasStencil)
                    {
                        const VkImageCopy testCopy = {
                            defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                            srcOffset,                 // VkOffset3D srcOffset;
                            defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                            dstOffset,                 // VkOffset3D dstOffset;
                            extent,                    // VkExtent3D extent;
                        };

                        copyRegion.imageCopy = testCopy;
                        params.regions.push_back(copyRegion);
                    }
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }

    // 3D tests. Note we use smaller dimensions here for performance reasons.
    {
        de::MovePtr<tcu::TestCaseGroup> subGroup(new tcu::TestCaseGroup(group->getTestContext(), "3d_to_3d"));

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
            params.allocationKind      = testGroupParams->allocationKind;
            params.extensionFlags      = testGroupParams->extensionFlags;
            params.queueSelection      = testGroupParams->queueSelection;
            params.useSparseBinding    = testGroupParams->useSparseBinding;
            params.useGeneralLayout    = testGroupParams->useGeneralLayout;

            bool hasDepth   = tcu::hasDepthComponent(mapVkFormat(params.src.image.format).order);
            bool hasStencil = tcu::hasStencilComponent(mapVkFormat(params.src.image.format).order);

            const VkImageSubresourceLayers defaultDepthSourceLayer   = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, 1u};
            const VkImageSubresourceLayers defaultStencilSourceLayer = {VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, 1u};

            for (int32_t i = 0; i < defaultQuarterSize; i += defaultSixteenthSize)
            {
                CopyRegion copyRegion;
                const VkOffset3D srcOffset = {0, 0, 0};
                const VkOffset3D dstOffset = {i, defaultQuarterSize - i - defaultSixteenthSize, i};
                const VkExtent3D extent    = {defaultSixteenthSize, defaultSixteenthSize, defaultSixteenthSize};

                if (hasDepth)
                {
                    const VkImageCopy testCopy = {
                        defaultDepthSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,               // VkOffset3D srcOffset;
                        defaultDepthSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,               // VkOffset3D dstOffset;
                        extent,                  // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
                if (hasStencil)
                {
                    const VkImageCopy testCopy = {
                        defaultStencilSourceLayer, // VkImageSubresourceLayers srcSubresource;
                        srcOffset,                 // VkOffset3D srcOffset;
                        defaultStencilSourceLayer, // VkImageSubresourceLayers dstSubresource;
                        dstOffset,                 // VkOffset3D dstOffset;
                        extent,                    // VkExtent3D extent;
                    };

                    copyRegion.imageCopy = testCopy;
                    params.regions.push_back(copyRegion);
                }
            }

            const std::string testName =
                getFormatCaseName(params.src.image.format) + "_" + getFormatCaseName(params.dst.image.format);
            TestParamsPtr paramsPtr(new TestParams(params));
            addTestGroup(subGroup.get(), testName, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr);

            if (hasDepth && hasStencil)
            {
                params.extensionFlags |= SEPARATE_DEPTH_STENCIL_LAYOUT;
                const std::string testName2 = getFormatCaseName(params.src.image.format) + "_" +
                                              getFormatCaseName(params.dst.image.format) + "_separate_layouts";
                TestParamsPtr paramsPtr2(new TestParams(params));
                addTestGroup(subGroup.get(), testName2, addImageToImageAllFormatsDepthStencilFormatsTests, paramsPtr2);
            }
        }

        group->addChild(subGroup.release());
    }
}

void addImageToImageAllFormatsTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "color", addImageToImageAllFormatsColorTests, testGroupParams);
    addTestGroup(group, "depth_stencil", addImageToImageAllFormatsDepthStencilTests, testGroupParams);
}

void addImageToImage3dImagesTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params3DTo2D;
        const uint32_t slicesLayers            = 16u;
        params3DTo2D.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params3DTo2D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.src.image.extent          = defaultHalfExtent;
        params3DTo2D.src.image.extent.depth    = slicesLayers;
        params3DTo2D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params3DTo2D.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params3DTo2D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.dst.image.extent          = defaultHalfExtent;
        params3DTo2D.dst.image.extent.depth    = slicesLayers;
        params3DTo2D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params3DTo2D.allocationKind            = testGroupParams->allocationKind;
        params3DTo2D.extensionFlags            = testGroupParams->extensionFlags;
        params3DTo2D.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                      // VkImageSubresourceLayers srcSubresource;
                {0, 0, (int32_t)slicesLayersNdx}, // VkOffset3D srcOffset;
                destinationLayer,                 // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                        // VkOffset3D dstOffset;
                defaultHalfExtent,                // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params3DTo2D.regions.push_back(imageCopy);
        }
        group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_by_slices", params3DTo2D));
    }

    {
        TestParams params2DTo3D;
        const uint32_t slicesLayers            = 16u;
        params2DTo3D.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params2DTo3D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.src.image.extent          = defaultHalfExtent;
        params2DTo3D.src.image.extent.depth    = slicesLayers;
        params2DTo3D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params2DTo3D.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params2DTo3D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.dst.image.extent          = defaultHalfExtent;
        params2DTo3D.dst.image.extent.depth    = slicesLayers;
        params2DTo3D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params2DTo3D.allocationKind            = testGroupParams->allocationKind;
        params2DTo3D.extensionFlags            = testGroupParams->extensionFlags;
        params2DTo3D.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                        // VkOffset3D srcOffset;
                destinationLayer,                 // VkImageSubresourceLayers dstSubresource;
                {0, 0, (int32_t)slicesLayersNdx}, // VkOffset3D dstOffset;
                defaultHalfExtent,                // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params2DTo3D.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_by_layers", params2DTo3D));
    }

    {
        TestParams params3DTo2D;
        const uint32_t slicesLayers            = 16u;
        params3DTo2D.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params3DTo2D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.src.image.extent          = defaultHalfExtent;
        params3DTo2D.src.image.extent.depth    = slicesLayers;
        params3DTo2D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params3DTo2D.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params3DTo2D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.dst.image.extent          = defaultHalfExtent;
        params3DTo2D.dst.image.extent.depth    = slicesLayers;
        params3DTo2D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params3DTo2D.allocationKind            = testGroupParams->allocationKind;
        params3DTo2D.extensionFlags            = testGroupParams->extensionFlags;
        params3DTo2D.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0,                         // uint32_t baseArrayLayer;
                slicesLayers               // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                  // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                    // VkOffset3D srcOffset;
                destinationLayer,             // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                    // VkOffset3D dstOffset;
                params3DTo2D.src.image.extent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params3DTo2D.regions.push_back(imageCopy);
        }
        group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_whole", params3DTo2D));
    }

    {
        TestParams params2DTo3D;
        const uint32_t slicesLayers            = 16u;
        params2DTo3D.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params2DTo3D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.src.image.extent          = defaultHalfExtent;
        params2DTo3D.src.image.extent.depth    = slicesLayers;
        params2DTo3D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params2DTo3D.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params2DTo3D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.dst.image.extent          = defaultHalfExtent;
        params2DTo3D.dst.image.extent.depth    = slicesLayers;
        params2DTo3D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params2DTo3D.allocationKind            = testGroupParams->allocationKind;
        params2DTo3D.extensionFlags            = testGroupParams->extensionFlags;
        params2DTo3D.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                slicesLayers               // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                   // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                     // VkOffset3D srcOffset;
                destinationLayer,              // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                     // VkOffset3D dstOffset;
                params2DTo3D.src.image.extent, // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params2DTo3D.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_whole", params2DTo3D));
    }

    {
        TestParams params3DTo2D;
        const uint32_t slicesLayers            = 16u;
        params3DTo2D.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params3DTo2D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.src.image.extent          = defaultHalfExtent;
        params3DTo2D.src.image.extent.depth    = slicesLayers;
        params3DTo2D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params3DTo2D.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params3DTo2D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params3DTo2D.dst.image.extent          = defaultHalfExtent;
        params3DTo2D.dst.image.extent.depth    = slicesLayers;
        params3DTo2D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params3DTo2D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params3DTo2D.allocationKind            = testGroupParams->allocationKind;
        params3DTo2D.extensionFlags            = testGroupParams->extensionFlags;
        params3DTo2D.queueSelection            = testGroupParams->queueSelection;

        const uint32_t regionWidth  = defaultHalfExtent.width / slicesLayers - 1;
        const uint32_t regionHeight = defaultHalfExtent.height / slicesLayers - 1;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer, // VkImageSubresourceLayers srcSubresource;
                {0, (int32_t)(regionHeight * slicesLayersNdx), (int32_t)slicesLayersNdx}, // VkOffset3D srcOffset;
                destinationLayer,                                 // VkImageSubresourceLayers dstSubresource;
                {(int32_t)(regionWidth * slicesLayersNdx), 0, 0}, // VkOffset3D dstOffset;
                {(defaultHalfExtent.width - regionWidth * slicesLayersNdx),
                 (defaultHalfExtent.height - regionHeight * slicesLayersNdx), 1} // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;
            params3DTo2D.regions.push_back(imageCopy);
        }
        group->addChild(new CopyImageToImageTestCase(testCtx, "3d_to_2d_regions", params3DTo2D));
    }

    {
        TestParams params2DTo3D;
        const uint32_t slicesLayers            = 16u;
        params2DTo3D.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params2DTo3D.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.src.image.extent          = defaultHalfExtent;
        params2DTo3D.src.image.extent.depth    = slicesLayers;
        params2DTo3D.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params2DTo3D.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params2DTo3D.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params2DTo3D.dst.image.extent          = defaultHalfExtent;
        params2DTo3D.dst.image.extent.depth    = slicesLayers;
        params2DTo3D.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params2DTo3D.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params2DTo3D.allocationKind            = testGroupParams->allocationKind;
        params2DTo3D.extensionFlags            = testGroupParams->extensionFlags;
        params2DTo3D.queueSelection            = testGroupParams->queueSelection;

        const uint32_t regionWidth  = defaultHalfExtent.width / slicesLayers - 1;
        const uint32_t regionHeight = defaultHalfExtent.height / slicesLayers - 1;

        for (uint32_t slicesLayersNdx = 0; slicesLayersNdx < slicesLayers; ++slicesLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                slicesLayersNdx,           // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,                                      // VkImageSubresourceLayers srcSubresource;
                {(int32_t)(regionWidth * slicesLayersNdx), 0, 0}, // VkOffset3D srcOffset;
                destinationLayer,                                 // VkImageSubresourceLayers dstSubresource;
                {0, (int32_t)(regionHeight * slicesLayersNdx), (int32_t)(slicesLayersNdx)}, // VkOffset3D dstOffset;
                {defaultHalfExtent.width - regionWidth * slicesLayersNdx,
                 defaultHalfExtent.height - regionHeight * slicesLayersNdx, 1} // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            params2DTo3D.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "2d_to_3d_regions", params2DTo3D));
    }
}

void addImageToImageCubeTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams paramsCubeToArray;
        const uint32_t arrayLayers                  = 6u;
        paramsCubeToArray.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.src.image.extent          = defaultHalfExtent;
        paramsCubeToArray.src.image.extent.depth    = arrayLayers;
        paramsCubeToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToArray.dst.image.createFlags     = 0;
        paramsCubeToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.dst.image.extent          = defaultHalfExtent;
        paramsCubeToArray.dst.image.extent.depth    = arrayLayers;
        paramsCubeToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToArray.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToArray.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_array_layers", paramsCubeToArray));
    }

    {
        TestParams paramsCubeToArray;
        const uint32_t arrayLayers                  = 6u;
        paramsCubeToArray.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.src.image.extent          = defaultHalfExtent;
        paramsCubeToArray.src.image.extent.depth    = arrayLayers;
        paramsCubeToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToArray.dst.image.createFlags     = 0;
        paramsCubeToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.dst.image.extent          = defaultHalfExtent;
        paramsCubeToArray.dst.image.extent.depth    = arrayLayers;
        paramsCubeToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToArray.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToArray.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_array_whole", paramsCubeToArray));
    }

    {
        TestParams paramsArrayToCube;
        const uint32_t arrayLayers                  = 6u;
        paramsArrayToCube.src.image.createFlags     = 0;
        paramsArrayToCube.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.src.image.extent          = defaultHalfExtent;
        paramsArrayToCube.src.image.extent.depth    = arrayLayers;
        paramsArrayToCube.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToCube.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsArrayToCube.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.dst.image.extent          = defaultHalfExtent;
        paramsArrayToCube.dst.image.extent.depth    = arrayLayers;
        paramsArrayToCube.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToCube.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToCube.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToCube.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToCube.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToCube.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_cube_layers", paramsArrayToCube));
    }

    {
        TestParams paramsArrayToCube;
        const uint32_t arrayLayers                  = 6u;
        paramsArrayToCube.src.image.createFlags     = 0;
        paramsArrayToCube.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.src.image.extent          = defaultHalfExtent;
        paramsArrayToCube.src.image.extent.depth    = arrayLayers;
        paramsArrayToCube.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToCube.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsArrayToCube.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToCube.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToCube.dst.image.extent          = defaultHalfExtent;
        paramsArrayToCube.dst.image.extent.depth    = arrayLayers;
        paramsArrayToCube.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToCube.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToCube.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToCube.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToCube.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToCube.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToCube.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_cube_whole", paramsArrayToCube));
    }

    {
        TestParams paramsCubeToArray;
        const uint32_t arrayLayers                  = 6u;
        paramsCubeToArray.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.src.image.extent          = defaultHalfExtent;
        paramsCubeToArray.src.image.extent.depth    = arrayLayers;
        paramsCubeToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToArray.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToArray.dst.image.extent          = defaultHalfExtent;
        paramsCubeToArray.dst.image.extent.depth    = arrayLayers;
        paramsCubeToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToArray.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToArray.queueSelection            = testGroupParams->queueSelection;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_cube_layers", paramsCubeToArray));
    }

    {
        TestParams paramsCubeToCube;
        const uint32_t arrayLayers                 = 6u;
        paramsCubeToCube.src.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToCube.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToCube.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToCube.src.image.extent          = defaultHalfExtent;
        paramsCubeToCube.src.image.extent.depth    = arrayLayers;
        paramsCubeToCube.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToCube.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsCubeToCube.dst.image.createFlags     = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        paramsCubeToCube.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsCubeToCube.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsCubeToCube.dst.image.extent          = defaultHalfExtent;
        paramsCubeToCube.dst.image.extent.depth    = arrayLayers;
        paramsCubeToCube.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsCubeToCube.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsCubeToCube.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsCubeToCube.allocationKind            = testGroupParams->allocationKind;
        paramsCubeToCube.extensionFlags            = testGroupParams->extensionFlags;
        paramsCubeToCube.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsCubeToCube.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "cube_to_cube_whole", paramsCubeToCube));
    }
}

void addImageToImageArrayTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.useSparseBinding          = testGroupParams->useSparseBinding;
        paramsArrayToArray.useGeneralLayout          = testGroupParams->useGeneralLayout;

        for (uint32_t arrayLayersNdx = 0; arrayLayersNdx < arrayLayers; ++arrayLayersNdx)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                arrayLayersNdx,            // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_array_layers", paramsArrayToArray));
    }

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(new CopyImageToImageTestCase(testCtx, "array_to_array_whole", paramsArrayToArray));
    }

    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.extensionFlags |= MAINTENANCE_5;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(
            new CopyImageToImageTestCase(testCtx, "array_to_array_whole_remaining_layers", paramsArrayToArray));
    }

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.extensionFlags |= MAINTENANCE_5;

        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                3u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t mipLevel;
                3u,                        // uint32_t baseArrayLayer;
                VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                defaultHalfExtent // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        group->addChild(
            new CopyImageToImageTestCase(testCtx, "array_to_array_partial_remaining_layers", paramsArrayToArray));
    }

    {
        TestParams paramsArrayToArray;
        const uint32_t arrayLayers                   = 16u;
        paramsArrayToArray.src.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.src.image.extent          = defaultHalfExtent;
        paramsArrayToArray.src.image.extent.depth    = arrayLayers;
        paramsArrayToArray.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        paramsArrayToArray.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        paramsArrayToArray.dst.image.extent          = defaultHalfExtent;
        paramsArrayToArray.dst.image.extent.depth    = arrayLayers;
        paramsArrayToArray.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        paramsArrayToArray.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        paramsArrayToArray.dst.image.fillMode        = FILL_MODE_GRADIENT;
        paramsArrayToArray.allocationKind            = testGroupParams->allocationKind;
        paramsArrayToArray.extensionFlags            = testGroupParams->extensionFlags;
        paramsArrayToArray.queueSelection            = testGroupParams->queueSelection;
        paramsArrayToArray.useSparseBinding          = testGroupParams->useSparseBinding;
        paramsArrayToArray.useGeneralLayout          = testGroupParams->useGeneralLayout;
        paramsArrayToArray.mipLevels = deLog2Floor32(deMaxu32(defaultHalfExtent.width, defaultHalfExtent.height)) + 1u;

        for (uint32_t mipLevelNdx = 0u; mipLevelNdx < paramsArrayToArray.mipLevels; mipLevelNdx++)
        {
            const VkImageSubresourceLayers sourceLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                mipLevelNdx,               // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationLayer = {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                mipLevelNdx,               // uint32_t mipLevel;
                0u,                        // uint32_t baseArrayLayer;
                arrayLayers                // uint32_t layerCount;
            };

            const VkExtent3D extent = {
                (uint32_t)deMax(defaultHalfExtent.width >> mipLevelNdx, 1),  // uint32_t    width;
                (uint32_t)deMax(defaultHalfExtent.height >> mipLevelNdx, 1), // uint32_t    height;
                1u,                                                          // uint32_t    depth;
            };

            const VkImageCopy testCopy = {
                sourceLayer,      // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},        // VkOffset3D srcOffset;
                destinationLayer, // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},        // VkOffset3D dstOffset;
                extent            // VkExtent3D extent;
            };

            CopyRegion imageCopy;
            imageCopy.imageCopy = testCopy;

            paramsArrayToArray.regions.push_back(imageCopy);
        }

        VkFormat imageFormats[] = {VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM,
                                   VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_S8_UINT};

        for (uint32_t imageFormatsNdx = 0; imageFormatsNdx < DE_LENGTH_OF_ARRAY(imageFormats); imageFormatsNdx++)
        {
            paramsArrayToArray.src.image.format = imageFormats[imageFormatsNdx];
            paramsArrayToArray.dst.image.format = imageFormats[imageFormatsNdx];
            for (uint32_t regionNdx = 0u; regionNdx < paramsArrayToArray.regions.size(); regionNdx++)
            {
                paramsArrayToArray.regions[regionNdx].imageCopy.srcSubresource.aspectMask =
                    getImageAspectFlags(mapVkFormat(imageFormats[imageFormatsNdx]));
                paramsArrayToArray.regions[regionNdx].imageCopy.dstSubresource.aspectMask =
                    getImageAspectFlags(mapVkFormat(imageFormats[imageFormatsNdx]));
            }
            std::ostringstream testName;
            const std::string formatName = getFormatName(imageFormats[imageFormatsNdx]);
            testName << "array_to_array_whole_mipmap_" << de::toLower(formatName.substr(10));
            group->addChild(new CopyImageToImageMipmapTestCase(testCtx, testName.str(), paramsArrayToArray));
        }
    }
}

} // namespace

void addCopyImageToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "simple_tests", addImageToImageSimpleTests, testGroupParams);
    if (!testGroupParams->useSparseBinding)
        addTestGroup(group, "all_formats", addImageToImageAllFormatsTests, testGroupParams);
    addTestGroup(group, "3d_images", addImageToImage3dImagesTests, testGroupParams);
    if (!testGroupParams->useSparseBinding)
        addTestGroup(group, "dimensions", addImageToImageDimensionsTests, testGroupParams);
    addTestGroup(group, "cube", addImageToImageCubeTests, testGroupParams);
    addTestGroup(group, "array", addImageToImageArrayTests, testGroupParams);
}

void addCopyImageToImageTestsSimpleOnly(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "simple_tests", addImageToImageSimpleTests, testGroupParams);
}

} // namespace api
} // namespace vkt
