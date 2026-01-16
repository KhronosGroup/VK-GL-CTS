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
 * \brief Vulkan Copy Image To Buffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyImageToBufferTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

using TestTextureSp = de::SharedPtr<pipeline::TestTexture>;

TestTextureSp makeCompressedTestTextureFromSrcImage(const TestParams &params)
{
    if (params.src.image.imageType == VK_IMAGE_TYPE_2D)
    {
        DE_ASSERT(params.src.image.extent.depth == 1u);

        return TestTextureSp(new pipeline::TestTexture2DArray(mapVkCompressedFormat(params.src.image.format),
                                                              params.src.image.extent.width,
                                                              params.src.image.extent.height, params.arrayLayers));
    }
    else if (params.src.image.imageType == VK_IMAGE_TYPE_1D)
    {
        DE_ASSERT(params.src.image.extent.depth == 1u);
        DE_ASSERT(params.src.image.extent.height == 1u);

        return TestTextureSp(new pipeline::TestTexture1DArray(mapVkCompressedFormat(params.src.image.format),
                                                              params.src.image.extent.width, params.arrayLayers));
    }
    else
    {
        return TestTextureSp(new pipeline::TestTexture3D(mapVkCompressedFormat(params.src.image.format),
                                                         params.src.image.extent.width, params.src.image.extent.height,
                                                         params.src.image.extent.depth));
    }

    return TestTextureSp();
}

class CopyImageToBuffer : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    CopyImageToBuffer(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);

    tcu::TextureFormat m_textureFormat;
    VkDeviceSize m_bufferSize;

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkBuffer> m_destination;
    de::MovePtr<Allocation> m_destinationBufferAlloc;

    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
};

CopyImageToBuffer::CopyImageToBuffer(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.src.image.format))
    , m_bufferSize(m_params.dst.buffer.size * tcu::getPixelSize(m_textureFormat))
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
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode sharingMode;
            0u,                                                                // uint32_t queueFamilyIndexCount;
            nullptr,                                                           // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                         // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (!testParams.useSparseBinding)
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

    // Create destination buffer
    {
        const VkBufferCreateInfo destinationBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_bufferSize,                         // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        m_destination = createBuffer(vk, m_device, &destinationBufferParams);
        m_destinationBufferAlloc =
            allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::HostVisible,
                           *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_destination, m_destinationBufferAlloc->getMemory(),
                                     m_destinationBufferAlloc->getOffset()));
    }
}

tcu::TestStatus CopyImageToBuffer::iterate(void)
{
    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.src.image.extent.width, m_params.src.image.extent.height,
                              m_params.src.image.extent.depth));
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth, m_params.src.image.fillMode);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
    generateBuffer(m_destinationTextureLevel->getAccess(), (int)m_params.dst.buffer.size, 1, 1,
                   m_params.dst.buffer.fillMode);

    generateExpectedResult();

    uploadImage(m_sourceTextureLevel->getAccess(), *m_source, m_params.src.image, m_params.useGeneralLayout);
    uploadBuffer(m_destinationTextureLevel->getAccess(), *m_destinationBufferAlloc);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Barriers for copying image to buffer
    const VkMemoryBarrier memoryBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                          // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,     // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,      // VkAccessFlags dstAccessMask;
    };
    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               nullptr,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               *m_source,                              // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
                                                   0u,                              // uint32_t baseMipLevel;
                                                   1u,                              // uint32_t mipLevels;
                                                   0u,                              // uint32_t baseArraySlice;
                                                   getArraySize(m_params.src.image) // uint32_t arraySize;
                                               }};

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        *m_destination,                          // VkBuffer buffer;
        0u,                                      // VkDeviceSize offset;
        m_bufferSize                             // VkDeviceSize size;
    };

    // Copy from image to buffer
    std::vector<VkBufferImageCopy> bufferImageCopies;
    std::vector<VkBufferImageCopy2KHR> bufferImageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferImageCopies.push_back(m_params.regions[i].bufferImageCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferImageCopies2KHR.push_back(
                convertvkBufferImageCopyTovkBufferImageCopy2KHR(m_params.regions[i].bufferImageCopy));
        }
    }

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, (m_params.useGeneralLayout ? 1 : 0), &memoryBarrier, 0, nullptr,
                          (m_params.useGeneralLayout ? 0 : 1), &imageBarrier);

    const VkImageLayout layout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyImageToBuffer(commandBuffer, m_source.get(), layout, m_destination.get(),
                                (uint32_t)m_params.regions.size(), &bufferImageCopies[0]);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                           // const void* pNext;
            m_source.get(),                                    // VkImage srcImage;
            layout,                                            // VkImageLayout srcImageLayout;
            m_destination.get(),                               // VkBuffer dstBuffer;
            (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
            &bufferImageCopies2KHR[0]                          // const VkBufferImageCopy2KHR* pRegions;
        };

        vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
    }

    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
    endCommandBuffer(vk, commandBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
    invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);
    tcu::copy(*resultLevel, tcu::ConstPixelBufferAccess(resultLevel->getFormat(), resultLevel->getSize(),
                                                        m_destinationBufferAlloc->getHostPtr()));

    return checkTestResult(resultLevel->getAccess());
}

void CopyImageToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                 CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize           = src.getFormat().getPixelSize();
    const VkExtent3D extent       = region.bufferImageCopy.imageExtent;
    const VkOffset3D srcOffset    = region.bufferImageCopy.imageOffset;
    const int texelOffset         = (int)region.bufferImageCopy.bufferOffset / texelSize;
    const uint32_t baseArrayLayer = region.bufferImageCopy.imageSubresource.baseArrayLayer;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, srcOffset.x, srcOffset.y + y, srcOffset.z + z + baseArrayLayer,
                                  region.bufferImageCopy.imageExtent.width, 1, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
}

class CopyImageToBufferTestCase : public vkt::TestCase
{
public:
    CopyImageToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyImageToBuffer(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.allocationKind == ALLOCATION_KIND_DEDICATED)
        {
            if (!context.isDeviceFunctionalitySupported("VK_KHR_dedicated_allocation"))
                TCU_THROW(NotSupportedError, "VK_KHR_dedicated_allocation is not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        VkImageFormatProperties properties;

        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                m_params.src.image.tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (properties.maxArrayLayers < getArraySize(m_params.src.image))
            TCU_THROW(NotSupportedError, "maxArrayLayers too small");

        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
        {
            checkTransferQueueGranularity(context, m_params.src.image.extent, m_params.src.image.imageType);
            for (const auto &region : m_params.regions)
            {
                checkTransferQueueGranularity(context, region.bufferImageCopy.imageExtent,
                                              m_params.src.image.imageType);
            }
        }
    }

private:
    TestParams m_params;
};

class CopyCompressedImageToBuffer final : public CopiesAndBlittingTestInstance
{
public:
    CopyCompressedImageToBuffer(Context &context, const TestParams &testParams);

    virtual tcu::TestStatus iterate(void) override;

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion,
                                          uint32_t) override
    {
        TCU_THROW(InternalError, "copyRegionToTextureLevel not implemented for CopyCompressedImageToBuffer");
    }

    // Contains a randomly generated compressed texture pyramid.
    TestTextureSp m_texture;
    de::MovePtr<ImageWithMemory> m_source;
    de::MovePtr<BufferWithMemory> m_sourceBuffer;
    de::MovePtr<BufferWithMemory> m_destination;
};

CopyCompressedImageToBuffer::CopyCompressedImageToBuffer(Context &context, const TestParams &testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_texture(makeCompressedTestTextureFromSrcImage(testParams))
{
}

tcu::TestStatus CopyCompressedImageToBuffer::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
#ifndef CTS_USES_VULKANSC
    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
#endif
    const VkDevice vkDevice          = m_device;
    Allocator &memAlloc              = *m_allocator;
    const ImageParms &srcImageParams = m_params.src.image;

    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Create source image, containing all the mip levels.
    {
        const VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            m_params.src.image.extent,                                         // VkExtent3D extent;
            (uint32_t)m_texture->getNumLevels(),                               // uint32_t mipLevels;
            m_params.arrayLayers,                                              // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_source = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, vkDevice, memAlloc, sourceImageParams, vk::MemoryRequirement::Any));
    }

    // Upload the compressed image.
    // FIXME: This could be a utility.
    //    pipeline::uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, m_source->get(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // Does not allow using an external command pool, the utilities there could fruitfully be generalised.
    m_sourceBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(m_texture->getCompressedSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        vk::MemoryRequirement::HostVisible));
    m_texture->write(reinterpret_cast<uint8_t *>(m_sourceBuffer->getAllocation().getHostPtr()));
    flushAlloc(vk, vkDevice, m_sourceBuffer->getAllocation());
    std::vector<VkBufferImageCopy> copyRegions = m_texture->getBufferCopyRegions();

    const VkImageLayout initialLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
#ifndef CTS_USES_VULKANSC
    if (m_params.extensionFlags & INDIRECT_COPY)
    {

        if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D)
        {
            // For 3D images, cmdCopyMemoryToImageIndirectKHR uses baseArrayLayer/layerCount instead of image.extent.depth
            for (auto &region : copyRegions)
            {
                region.imageSubresource.baseArrayLayer = region.imageOffset.z;
                region.imageSubresource.layerCount     = region.imageExtent.depth;
            }
        }
        copyBufferToImageIndirect(vk, vki, vkPhysDevice, vkDevice, queue, activeQueueFamilyIndex(),
                                  m_sourceBuffer->get(), m_texture->getCompressedSize(), copyRegions, nullptr,
                                  VK_IMAGE_ASPECT_COLOR_BIT, m_texture->getNumLevels(), m_texture->getArraySize(),
                                  m_source->get(), initialLayout, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_ACCESS_TRANSFER_READ_BIT, &commandPool, 0);
    }
    else
#endif
    {
        copyBufferToImage(vk, vkDevice, queue, activeQueueFamilyIndex(), m_sourceBuffer->get(),
                          m_texture->getCompressedSize(), copyRegions, nullptr, VK_IMAGE_ASPECT_COLOR_BIT,
                          m_texture->getNumLevels(), m_texture->getArraySize(), m_source->get(), initialLayout,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, &commandPool, 0);
    }

    // VKSC requires static allocation, so allocate a large enough buffer for each individual mip level of
    // the compressed source image, rather than creating a corresponding buffer for each level in the loop
    // below.
    auto level0BuferSize = m_texture->getCompressedLevel(0, 0).getDataSize();
    m_destination        = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(level0BuferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    // Copy each miplevel of the uploaded image into a buffer, and
    // check the buffer matches the appropriate test texture level.
    for (uint32_t mipLevelToCheckIdx = 0; mipLevelToCheckIdx < (uint32_t)m_texture->getNumLevels();
         mipLevelToCheckIdx++)
        for (uint32_t arrayLayerToCheckIdx = 0; arrayLayerToCheckIdx < (uint32_t)m_texture->getArraySize();
             arrayLayerToCheckIdx++)
        {
            const tcu::CompressedTexture compressedMipLevelToCheck =
                m_texture->getCompressedLevel(mipLevelToCheckIdx, arrayLayerToCheckIdx);
            uint32_t bufferSize = compressedMipLevelToCheck.getDataSize();

            // Clear the buffer to zero before copying into it as a precaution.
            deMemset(m_destination->getAllocation().getHostPtr(), 0, bufferSize);
            flushAlloc(vk, vkDevice, m_destination->getAllocation());

            const bool useMemoryBarrier = m_params.useGeneralLayout;
            const auto memoryBarrier    = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            // Barrier to get the source image's selected mip-level / layer in the right format for transfer.
            const auto imageBarrier = makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_source->get(),
                {
                    // VkImageSubresourceRange subresourceRange;
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    mipLevelToCheckIdx,        // uint32_t baseMipLevel;
                    1u,                        // uint32_t mipLevels;
                    arrayLayerToCheckIdx,      // uint32_t baseArraySlice;
                    1u,                        // uint32_t arraySize;
                });

            // Barrier to wait for the transfer from image to buffer to complete.
            const auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                               m_destination->get(), 0, bufferSize);

            // Copy from image to buffer
            VkBufferImageCopy copyRegion;
            copyRegion = makeBufferImageCopy(
                mipLevelExtents(srcImageParams.extent, mipLevelToCheckIdx),
                makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mipLevelToCheckIdx, arrayLayerToCheckIdx, 1));

            VkBufferImageCopy bufferImageCopy;
            VkBufferImageCopy2KHR bufferImageCopy2KHR;
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                bufferImageCopy = copyRegion;
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                bufferImageCopy2KHR = convertvkBufferImageCopyTovkBufferImageCopy2KHR(copyRegion);
            }

            beginCommandBuffer(vk, commandBuffer);
            // Transition the selected miplevel to the right format for the transfer.
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, useMemoryBarrier ? 1 : 0, &memoryBarrier, 0, nullptr,
                                  useMemoryBarrier ? 0 : 1, &imageBarrier);

            // Copy the mip level to the buffer.
            const VkImageLayout copyLayout =
                m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdCopyImageToBuffer(commandBuffer, m_source->get(), copyLayout, m_destination->get(), 1u,
                                        &bufferImageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
                    VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
                    nullptr,                                           // const void* pNext;
                    m_source->get(),                                   // VkImage srcImage;
                    copyLayout,                                        // VkImageLayout srcImageLayout;
                    m_destination->get(),                              // VkBuffer dstBuffer;
                    1u,                                                // uint32_t regionCount;
                    &bufferImageCopy2KHR                               // const VkBufferImageCopy2KHR* pRegions;
                };

                vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
            }

            // Prepare to read from the host visible barrier.
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
            endCommandBuffer(vk, commandBuffer);

            submitCommandsAndWaitWithSync(vk, vkDevice, queue, commandBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

            invalidateAlloc(vk, vkDevice, m_destination->getAllocation());
            // Read and compare buffer data.
            const uint8_t *referenceData = (uint8_t *)compressedMipLevelToCheck.getData();
            const uint8_t *resultData    = (uint8_t *)m_destination->getAllocation().getHostPtr();
            int result                   = deMemCmp(referenceData, resultData, bufferSize);
            if (result != 0)
            {
                std::ostringstream msg;
                msg << "Incorrect data retrieved for mip level " << mipLevelToCheckIdx << ", layer "
                    << arrayLayerToCheckIdx << " - extents (" << compressedMipLevelToCheck.getWidth() << ", "
                    << compressedMipLevelToCheck.getHeight() << ")";
                return tcu::TestStatus::fail(msg.str());
            }
        }

    return tcu::TestStatus::pass("OK");
}

class CopyCompressedImageToBufferTestCase : public vkt::TestCase
{
public:
    CopyCompressedImageToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyCompressedImageToBuffer(context, m_params);
    }
    virtual void checkSupport(Context &context) const;

private:
    TestParams m_params;
};

void CopyCompressedImageToBufferTestCase::checkSupport(Context &context) const
{
    DE_ASSERT(m_params.src.image.tiling == VK_IMAGE_TILING_OPTIMAL);

    checkExtensionSupport(context, m_params.extensionFlags);

    VkFormatProperties formatProps;
    context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(),
                                                                     m_params.src.image.format, &formatProps);

    VkImageFormatProperties imageFormatProperties;

    const auto &instance = context.getInstanceInterface();
    if (instance.getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), m_params.src.image.format,
                                                        m_params.src.image.imageType, VK_IMAGE_TILING_OPTIMAL,
                                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                                                        &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Format not supported");
    }

    const uint32_t arrayLayers = getArraySize(m_params.src.image);
    uint32_t mipLevels         = 0;

    {
        const auto &width  = m_params.src.image.extent.width;
        const auto &height = m_params.src.image.extent.height;
        const auto &depth  = m_params.src.image.extent.depth;

        if (m_params.src.image.imageType == VK_IMAGE_TYPE_1D)
            mipLevels = deLog2Floor32(width) + 1u;
        else if (m_params.src.image.imageType == VK_IMAGE_TYPE_2D)
            mipLevels = deLog2Floor32(de::max(width, height)) + 1u;
        else if (m_params.src.image.imageType == VK_IMAGE_TYPE_3D)
            mipLevels = deLog2Floor32(de::max(width, de::max(height, depth))) + 1u;
        else
            DE_ASSERT(false);
    }

    if (imageFormatProperties.maxMipLevels < mipLevels)
        TCU_THROW(NotSupportedError, "Required number of mip levels not supported");

    if (imageFormatProperties.maxArrayLayers < arrayLayers)
        TCU_THROW(NotSupportedError, "Required number of layers not supported");

#ifndef CTS_USES_VULKANSC
    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        VkFormatProperties3 formatProps3;
        formatProps3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
        formatProps3.pNext = nullptr;

        VkFormatProperties2 formatProps2;
        formatProps2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        formatProps2.pNext = &formatProps3;
        instance.getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(), m_params.src.image.format,
                                                    &formatProps2);

        if (m_params.src.image.tiling == VK_IMAGE_TILING_OPTIMAL)
        {
            if (!(formatProps3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR))
                TCU_THROW(NotSupportedError, "Format feature is not supported on this format");
        }
        if (m_params.src.image.tiling == VK_IMAGE_TILING_LINEAR)
        {
            if (!(formatProps3.linearTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR))
                TCU_THROW(NotSupportedError, "Format feature is not supported on this format");
        }

        VkPhysicalDeviceCopyMemoryIndirectPropertiesKHR copyMemoryIndirectProperties = {};
        copyMemoryIndirectProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_KHR;
        VkPhysicalDeviceProperties2 deviceProperties = {};
        deviceProperties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext                       = &copyMemoryIndirectProperties;
        instance.getPhysicalDeviceProperties2(context.getPhysicalDevice(), &deviceProperties);

        switch (m_params.queueSelection)
        {
        case QueueSelectionOptions::Universal:
        {
            if (!(copyMemoryIndirectProperties.supportedQueues & VK_QUEUE_GRAPHICS_BIT))
            {
                TCU_THROW(NotSupportedError, "Graphics queue not supported!");
            }
            break;
        }
        case QueueSelectionOptions::TransferOnly:
        {
            if (!(copyMemoryIndirectProperties.supportedQueues & VK_QUEUE_TRANSFER_BIT))
            {
                TCU_THROW(NotSupportedError, "Transfer queue not supported!");
            }
            break;
        }
        case QueueSelectionOptions::ComputeOnly:
        {
            if (!(copyMemoryIndirectProperties.supportedQueues & VK_QUEUE_COMPUTE_BIT))
            {
                TCU_THROW(NotSupportedError, "Compute queue not supported!");
            }
            break;
        }
        }
    }
#endif

    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        TCU_THROW(NotSupportedError, "TRANSFER_SRC is not supported on this image type");
}

class CopyMipmappedImageToBuffer final : public CopiesAndBlittingTestInstance
{
public:
    CopyMipmappedImageToBuffer(Context &context, TestParams testParams);

    virtual tcu::TestStatus iterate(void) override;

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess, tcu::PixelBufferAccess, CopyRegion,
                                          uint32_t) override
    {
        TCU_THROW(InternalError, "copyRegionToTextureLevel not implemented for CopyMipmappedImageToBuffer");
    }

    using TestTexture2DSp = de::SharedPtr<pipeline::TestTexture2DArray>;
    TestTexture2DSp m_texture;
    de::MovePtr<ImageWithMemory> m_source;
    de::MovePtr<BufferWithMemory> m_sourceBuffer;
    de::MovePtr<BufferWithMemory> m_destination;
};

CopyMipmappedImageToBuffer::CopyMipmappedImageToBuffer(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_texture(TestTexture2DSp(
          new pipeline::TestTexture2DArray(mapVkFormat(testParams.src.image.format), testParams.src.image.extent.width,
                                           testParams.src.image.extent.height, testParams.arrayLayers)))
{
}

tcu::TestStatus CopyMipmappedImageToBuffer::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
#ifndef CTS_USES_VULKANSC
    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
#endif
    const VkDevice vkDevice          = m_device;
    Allocator &memAlloc              = *m_allocator;
    const ImageParms &srcImageParams = m_params.src.image;

    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Create source image, containing all the mip levels.
    {
        const VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            getCreateFlags(m_params.src.image),                                // VkImageCreateFlags flags;
            m_params.src.image.imageType,                                      // VkImageType imageType;
            m_params.src.image.format,                                         // VkFormat format;
            m_params.src.image.extent,                                         // VkExtent3D extent;
            (uint32_t)m_texture->getNumLevels(),                               // uint32_t mipLevels;
            m_params.arrayLayers,                                              // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,                                             // uint32_t samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        m_source = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, vkDevice, memAlloc, sourceImageParams, vk::MemoryRequirement::Any));
    }

    m_sourceBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, vkDevice, memAlloc,
                             makeBufferCreateInfo(m_texture->getSize(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
                             vk::MemoryRequirement::HostVisible | vk::MemoryRequirement::DeviceAddress));
    m_texture->write(reinterpret_cast<uint8_t *>(m_sourceBuffer->getAllocation().getHostPtr()));
    flushAlloc(vk, vkDevice, m_sourceBuffer->getAllocation());
    std::vector<VkBufferImageCopy> copyRegions = m_texture->getBufferCopyRegions();

#ifndef CTS_USES_VULKANSC
    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        copyBufferToImageIndirect(vk, vki, vkPhysDevice, vkDevice, queue, activeQueueFamilyIndex(),
                                  m_sourceBuffer->get(), m_texture->getSize(), copyRegions, nullptr,
                                  VK_IMAGE_ASPECT_COLOR_BIT, m_texture->getNumLevels(), m_texture->getArraySize(),
                                  m_source->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_ACCESS_TRANSFER_READ_BIT, &commandPool, 0);
    }
    else
#endif
    {
        copyBufferToImage(vk, vkDevice, queue, activeQueueFamilyIndex(), m_sourceBuffer->get(), m_texture->getSize(),
                          copyRegions, nullptr, VK_IMAGE_ASPECT_COLOR_BIT, m_texture->getNumLevels(),
                          m_texture->getArraySize(), m_source->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, &commandPool, 0);
    }

    // VKSC requires static allocation, so allocate a large enough buffer for each individual mip level of
    // the source image, rather than creating a corresponding buffer for each level in the loop
    // below.
    const tcu::ConstPixelBufferAccess level0 = m_texture->getLevel(0, 0);
    auto level0BufferSize =
        level0.getWidth() * level0.getHeight() * level0.getDepth() * level0.getFormat().getPixelSize();
    m_destination = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(level0BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    // Copy each miplevel of the uploaded image into a buffer, and
    // check the buffer matches the appropriate test texture level.
    for (uint32_t mipLevelToCheckIdx = 0; mipLevelToCheckIdx < (uint32_t)m_texture->getNumLevels();
         mipLevelToCheckIdx++)
        for (uint32_t arrayLayerToCheckIdx = 0; arrayLayerToCheckIdx < (uint32_t)m_texture->getArraySize();
             arrayLayerToCheckIdx++)
        {
            const tcu::ConstPixelBufferAccess mipLevelToCheck =
                m_texture->getLevel(mipLevelToCheckIdx, arrayLayerToCheckIdx);
            uint32_t bufferSize = mipLevelToCheck.getWidth() * mipLevelToCheck.getHeight() *
                                  mipLevelToCheck.getDepth() * mipLevelToCheck.getFormat().getPixelSize();

            // Clear the buffer to zero before copying into it as a precaution.
            deMemset(m_destination->getAllocation().getHostPtr(), 0, bufferSize);
            flushAlloc(vk, vkDevice, m_destination->getAllocation());

            // Barrier to get the source image's selected mip-level / layer in the right format for transfer.
            const auto imageBarrier = makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_source->get(),
                {
                    // VkImageSubresourceRange subresourceRange;
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    mipLevelToCheckIdx,        // uint32_t baseMipLevel;
                    1u,                        // uint32_t mipLevels;
                    arrayLayerToCheckIdx,      // uint32_t baseArraySlice;
                    1u,                        // uint32_t arraySize;
                });

            // Barrier to wait for the transfer from image to buffer to complete.
            const auto bufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                                                               m_destination->get(), 0, bufferSize);

            // Copy from image to buffer
            VkBufferImageCopy copyRegion;
            copyRegion = makeBufferImageCopy(
                mipLevelExtents(srcImageParams.extent, mipLevelToCheckIdx),
                makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, mipLevelToCheckIdx, arrayLayerToCheckIdx, 1));

            VkBufferImageCopy bufferImageCopy;
            VkBufferImageCopy2KHR bufferImageCopy2KHR;
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                bufferImageCopy = copyRegion;
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                bufferImageCopy2KHR = convertvkBufferImageCopyTovkBufferImageCopy2KHR(copyRegion);
            }

            beginCommandBuffer(vk, commandBuffer);
            // Transition the selected miplevel to the right format for the transfer.
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

            // Copy the mip level to the buffer.
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vk.cmdCopyImageToBuffer(commandBuffer, m_source->get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        m_destination->get(), 1u, &bufferImageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
                    VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
                    nullptr,                                           // const void* pNext;
                    m_source->get(),                                   // VkImage srcImage;
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,              // VkImageLayout srcImageLayout;
                    m_destination->get(),                              // VkBuffer dstBuffer;
                    1u,                                                // uint32_t regionCount;
                    &bufferImageCopy2KHR                               // const VkBufferImageCopy2KHR* pRegions;
                };

                vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
            }

            // Prepare to read from the host visible barrier.
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
            endCommandBuffer(vk, commandBuffer);

            submitCommandsAndWaitWithSync(vk, vkDevice, queue, commandBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

            invalidateAlloc(vk, vkDevice, m_destination->getAllocation());
            // Read and compare buffer data.
            const uint8_t *referenceData = (uint8_t *)mipLevelToCheck.getDataPtr();
            const uint8_t *resultData    = (uint8_t *)m_destination->getAllocation().getHostPtr();
            int result                   = deMemCmp(referenceData, resultData, bufferSize);
            if (result != 0)
            {
                std::ostringstream msg;
                msg << "Incorrect data retrieved for mip level " << mipLevelToCheckIdx << ", layer "
                    << arrayLayerToCheckIdx << " - extents (" << mipLevelToCheck.getWidth() << ", "
                    << mipLevelToCheck.getHeight() << ")";
                return tcu::TestStatus::fail(msg.str());
            }
        }

    return tcu::TestStatus::pass("OK");
}

class CopyMipmappedImageToBufferTestCase : public vkt::TestCase
{
public:
    CopyMipmappedImageToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyMipmappedImageToBuffer(context, m_params);
    }
    virtual void checkSupport(Context &context) const;

private:
    TestParams m_params;
};

void CopyMipmappedImageToBufferTestCase::checkSupport(Context &context) const
{
    DE_ASSERT(m_params.src.image.tiling == VK_IMAGE_TILING_OPTIMAL);
    DE_ASSERT(m_params.src.image.imageType == vk::VK_IMAGE_TYPE_2D);

    checkExtensionSupport(context, m_params.extensionFlags);

    VkFormatProperties formatProps;
    context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(),
                                                                     m_params.src.image.format, &formatProps);

    VkImageFormatProperties imageFormatProperties;

    const auto &instance = context.getInstanceInterface();
    if (instance.getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), m_params.src.image.format,
                                                        m_params.src.image.imageType, VK_IMAGE_TILING_OPTIMAL,
                                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0,
                                                        &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, "Format not supported");
    }

    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
        TCU_THROW(NotSupportedError, "TRANSFER_SRC is not supported on this image type");

#ifndef CTS_USES_VULKANSC
    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        VkFormatProperties3 formatProps3;
        formatProps3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
        formatProps3.pNext = nullptr;

        VkFormatProperties2 formatProps2;
        formatProps2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        formatProps2.pNext = &formatProps3;
        context.getInstanceInterface().getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(),
                                                                          m_params.src.image.format, &formatProps2);

        if (m_params.src.image.tiling == VK_IMAGE_TILING_OPTIMAL)
        {
            if (!(formatProps3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR))
                TCU_THROW(NotSupportedError, "Format feature is not supported on this format");
        }
        if (m_params.src.image.tiling == VK_IMAGE_TILING_LINEAR)
        {
            if (!(formatProps3.linearTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR))
                TCU_THROW(NotSupportedError, "Format feature is not supported on this format");
        }
    }
#endif
}

void add2dImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    for (const auto format :
         {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8_UNORM, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT})
        for (const auto tiling : {VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TILING_LINEAR})
        {
            const auto tcuFormat = mapVkFormat(format);

            const auto testNameSuffix =
                ((format != VK_FORMAT_R8G8B8A8_UNORM) ? ("_" + getFormatCaseName(format)) : std::string()) +
                ((tiling == VK_IMAGE_TILING_LINEAR) ? "_linear" : "");

            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = defaultSize * defaultSize;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const VkBufferImageCopy bufferImageCopy = {
                    0u,                 // VkDeviceSize bufferOffset;
                    0u,                 // uint32_t bufferRowLength;
                    0u,                 // uint32_t bufferImageHeight;
                    defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
                    {0, 0, 0},          // VkOffset3D imageOffset;
                    defaultExtent       // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(new CopyImageToBufferTestCase(testCtx, "whole" + testNameSuffix, params));
            }

            {
                const auto bufferWidth  = defaultSize + 1u;
                const auto bufferHeight = defaultSize + 1u;

                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = bufferWidth * bufferHeight;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const VkBufferImageCopy bufferImageCopy = {
                    0u,                 // VkDeviceSize bufferOffset;
                    bufferWidth,        // uint32_t bufferRowLength;
                    bufferHeight,       // uint32_t bufferImageHeight;
                    defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
                    {0, 0, 0},          // VkOffset3D imageOffset;
                    defaultExtent       // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(new CopyImageToBufferTestCase(testCtx, "whole_unaligned" + testNameSuffix, params));
            }

            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = defaultSize * defaultSize;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const auto bufferOffset = de::roundUp(defaultSize * defaultHalfSize, tcu::getPixelSize(tcuFormat));

                const VkBufferImageCopy bufferImageCopy = {
                    static_cast<VkDeviceSize>(bufferOffset),     // VkDeviceSize bufferOffset;
                    0u,                                          // uint32_t bufferRowLength;
                    0u,                                          // uint32_t bufferImageHeight;
                    defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                    {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                    defaultHalfExtent                            // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(new CopyImageToBufferTestCase(testCtx, "buffer_offset" + testNameSuffix, params));
            }

            if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = defaultSize * defaultSize;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const auto bufferOffset = de::roundUp(defaultSize * defaultHalfSize + 1, tcu::getPixelSize(tcuFormat));

                const VkBufferImageCopy bufferImageCopy = {
                    static_cast<VkDeviceSize>(bufferOffset),     // VkDeviceSize bufferOffset;
                    0u,                                          // uint32_t bufferRowLength;
                    0u,                                          // uint32_t bufferImageHeight;
                    defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                    {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                    defaultHalfExtent                            // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(
                    new CopyImageToBufferTestCase(testCtx, "buffer_offset_relaxed" + testNameSuffix, params));
            }

            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = defaultSize * defaultSize;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const int pixelSize           = tcu::getPixelSize(mapVkFormat(params.src.image.format));
                const VkDeviceSize bufferSize = pixelSize * params.dst.buffer.size;
                const VkDeviceSize offsetSize = pixelSize * defaultQuarterSize * defaultQuarterSize;
                uint32_t divisor              = 1;
                for (VkDeviceSize offset = 0; offset < bufferSize - offsetSize; offset += offsetSize, ++divisor)
                {
                    const uint32_t bufferRowLength   = defaultQuarterSize;
                    const uint32_t bufferImageHeight = defaultQuarterSize;
                    const VkExtent3D imageExtent     = {defaultQuarterSize / divisor, defaultQuarterSize, 1};
                    DE_ASSERT(!bufferRowLength || bufferRowLength >= imageExtent.width);
                    DE_ASSERT(!bufferImageHeight || bufferImageHeight >= imageExtent.height);
                    DE_ASSERT(imageExtent.width * imageExtent.height * imageExtent.depth <= offsetSize);

                    CopyRegion region;
                    const VkBufferImageCopy bufferImageCopy = {
                        offset,             // VkDeviceSize bufferOffset;
                        bufferRowLength,    // uint32_t bufferRowLength;
                        bufferImageHeight,  // uint32_t bufferImageHeight;
                        defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
                        {0, 0, 0},          // VkOffset3D imageOffset;
                        imageExtent         // VkExtent3D imageExtent;
                    };
                    region.bufferImageCopy = bufferImageCopy;
                    params.regions.push_back(region);
                }

                group->addChild(new CopyImageToBufferTestCase(testCtx, "regions" + testNameSuffix, params));
            }

            {
                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const VkBufferImageCopy bufferImageCopy = {
                    0u,                                          // VkDeviceSize bufferOffset;
                    defaultSize,                                 // uint32_t bufferRowLength;
                    defaultSize,                                 // uint32_t bufferImageHeight;
                    defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                    {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                    defaultHalfExtent                            // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(
                    new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer" + testNameSuffix, params));
            }

            {
                TestParams params;
                uint32_t bufferImageHeight       = defaultSize + 1u;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size           = bufferImageHeight * defaultSize;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const VkBufferImageCopy bufferImageCopy = {
                    0u,                 // VkDeviceSize bufferOffset;
                    defaultSize,        // uint32_t bufferRowLength;
                    bufferImageHeight,  // uint32_t bufferImageHeight;
                    defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
                    {0, 0, 0},          // VkOffset3D imageOffset;
                    defaultExtent       // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(new CopyImageToBufferTestCase(testCtx, "larger_buffer" + testNameSuffix, params));
            }

            {
                const auto bufferOffset = de::roundUp(defaultQuarterSize, tcu::getPixelSize(tcuFormat));

                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultExtent;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                params.dst.buffer.size  = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + bufferOffset;
                params.allocationKind   = testGroupParams->allocationKind;
                params.extensionFlags   = testGroupParams->extensionFlags;
                params.queueSelection   = testGroupParams->queueSelection;
                params.useSparseBinding = testGroupParams->useSparseBinding;
                params.useGeneralLayout = testGroupParams->useGeneralLayout;

                const VkBufferImageCopy bufferImageCopy = {
                    static_cast<VkDeviceSize>(bufferOffset),     // VkDeviceSize bufferOffset;
                    defaultSize,                                 // uint32_t bufferRowLength;
                    defaultSize,                                 // uint32_t bufferImageHeight;
                    defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                    {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                    defaultHalfExtent                            // VkExtent3D imageExtent;
                };
                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(
                    new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer_offset" + testNameSuffix, params));
            }

            {
                TestParams params;
                uint32_t arrayLayers             = 16u;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultHalfExtent;
                params.src.image.extent.depth    = arrayLayers;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
                for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
                {
                    const VkDeviceSize offset = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
                    const VkBufferImageCopy bufferImageCopy = {
                        offset, // VkDeviceSize bufferOffset;
                        0u,     // uint32_t bufferRowLength;
                        0u,     // uint32_t bufferImageHeight;
                        {
                            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                            0u,                        // uint32_t mipLevel;
                            arrayLayerNdx,             // uint32_t baseArrayLayer;
                            1u,                        // uint32_t layerCount;
                        },                             // VkImageSubresourceLayers imageSubresource;
                        {0, 0, 0},                     // VkOffset3D imageOffset;
                        defaultHalfExtent              // VkExtent3D imageExtent;
                    };
                    CopyRegion copyRegion;
                    copyRegion.bufferImageCopy = bufferImageCopy;

                    params.regions.push_back(copyRegion);
                }
                group->addChild(new CopyImageToBufferTestCase(testCtx, "array" + testNameSuffix, params));
            }

            {
                TestParams params;
                uint32_t arrayLayers             = 16u;
                uint32_t imageBufferHeight       = defaultHalfSize + 1u;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultHalfExtent;
                params.src.image.extent.depth    = arrayLayers;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.dst.buffer.size           = defaultHalfSize * imageBufferHeight * arrayLayers;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
                for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
                {
                    const VkDeviceSize offset = defaultHalfSize * imageBufferHeight * pixelSize * arrayLayerNdx;
                    const VkBufferImageCopy bufferImageCopy = {
                        offset,            // VkDeviceSize bufferOffset;
                        0u,                // uint32_t bufferRowLength;
                        imageBufferHeight, // uint32_t bufferImageHeight;
                        {
                            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                            0u,                        // uint32_t mipLevel;
                            arrayLayerNdx,             // uint32_t baseArrayLayer;
                            1u,                        // uint32_t layerCount;
                        },                             // VkImageSubresourceLayers imageSubresource;
                        {0, 0, 0},                     // VkOffset3D imageOffset;
                        defaultHalfExtent              // VkExtent3D imageExtent;
                    };
                    CopyRegion copyRegion;
                    copyRegion.bufferImageCopy = bufferImageCopy;

                    params.regions.push_back(copyRegion);
                }
                group->addChild(new CopyImageToBufferTestCase(testCtx, "array_larger_buffer" + testNameSuffix, params));
            }

            {
                TestParams params;
                uint32_t arrayLayers             = 16u;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultHalfExtent;
                params.src.image.extent.depth    = arrayLayers;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
                for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
                {
                    const VkDeviceSize offset = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
                    const VkBufferImageCopy bufferImageCopy = {
                        offset,          // VkDeviceSize bufferOffset;
                        defaultHalfSize, // uint32_t bufferRowLength;
                        defaultHalfSize, // uint32_t bufferImageHeight;
                        {
                            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                            0u,                        // uint32_t mipLevel;
                            arrayLayerNdx,             // uint32_t baseArrayLayer;
                            1u,                        // uint32_t layerCount;
                        },                             // VkImageSubresourceLayers imageSubresource;
                        {0, 0, 0},                     // VkOffset3D imageOffset;
                        defaultHalfExtent              // VkExtent3D imageExtent;
                    };
                    CopyRegion copyRegion;
                    copyRegion.bufferImageCopy = bufferImageCopy;

                    params.regions.push_back(copyRegion);
                }
                group->addChild(
                    new CopyImageToBufferTestCase(testCtx, "array_tightly_sized_buffer" + testNameSuffix, params));
            }

            {
                TestParams params;
                const uint32_t baseLayer         = 0u;
                const uint32_t layerCount        = 16u;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultHalfExtent;
                params.src.image.extent.depth    = layerCount;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.src.image.fillMode        = FILL_MODE_RED;
                params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
                params.dst.buffer.fillMode       = FILL_MODE_RED;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;
                params.extensionFlags |= MAINTENANCE_5;

                const VkImageSubresourceLayers defaultLayer = {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    baseLayer,                 // uint32_t baseArrayLayer;
                    VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
                };

                const VkBufferImageCopy bufferImageCopy = {
                    0,                // VkDeviceSize bufferOffset;
                    0,                // uint32_t bufferRowLength;
                    0,                // uint32_t bufferImageHeight;
                    defaultLayer,     // VkImageSubresourceLayers imageSubresource;
                    {0, 0, 0},        // VkOffset3D imageOffset;
                    defaultHalfExtent // VkExtent3D imageExtent;
                };

                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(
                    new CopyImageToBufferTestCase(testCtx, "array_all_remaining_layers" + testNameSuffix, params));
            }

            {
                TestParams params;
                const uint32_t baseLayer         = 2u;
                const uint32_t layerCount        = 16u;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = defaultHalfExtent;
                params.src.image.extent.depth    = layerCount;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.src.image.fillMode        = FILL_MODE_RED;
                params.dst.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
                params.dst.buffer.fillMode       = FILL_MODE_RED;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;
                params.extensionFlags |= MAINTENANCE_5;

                const VkImageSubresourceLayers defaultLayer = {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    baseLayer,                 // uint32_t baseArrayLayer;
                    VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
                };

                const VkBufferImageCopy bufferImageCopy = {
                    0,                // VkDeviceSize bufferOffset;
                    0,                // uint32_t bufferRowLength;
                    0,                // uint32_t bufferImageHeight;
                    defaultLayer,     // VkImageSubresourceLayers imageSubresource;
                    {0, 0, 0},        // VkOffset3D imageOffset;
                    defaultHalfExtent // VkExtent3D imageExtent;
                };

                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;

                params.regions.push_back(copyRegion);

                group->addChild(
                    new CopyImageToBufferTestCase(testCtx, "array_not_all_remaining_layers" + testNameSuffix, params));
            }

            // this test applies only to linear images we limit also repeating it to non-sparse images with standard layouts
            if ((tiling == VK_IMAGE_TILING_LINEAR) && !testGroupParams->useSparseBinding &&
                !testGroupParams->useGeneralLayout)
            {
                // check if padding bytes are not overwritten between rows or images
                // when the rowPitch is larger than a row size of the copy, or the same for imageHeight

                TestParams params;
                params.src.image.imageType       = VK_IMAGE_TYPE_2D;
                params.src.image.format          = format;
                params.src.image.extent          = {2u, 2u, 1u}; // small extent to trigger padding bytes
                params.src.image.extent.depth    = 1u;
                params.src.image.tiling          = tiling;
                params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                params.src.image.fillMode        = FILL_MODE_RED;
                params.dst.buffer.size           = defaultSize * defaultSize;
                params.dst.buffer.fillMode       = FILL_MODE_RANDOM_GRAY;
                params.allocationKind            = testGroupParams->allocationKind;
                params.extensionFlags            = testGroupParams->extensionFlags;
                params.queueSelection            = testGroupParams->queueSelection;
                params.useSparseBinding          = testGroupParams->useSparseBinding;
                params.useGeneralLayout          = testGroupParams->useGeneralLayout;

                const VkImageSubresourceLayers defaultLayer{
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    0u,                        // uint32_t baseArrayLayer;
                    1u                         // uint32_t layerCount;
                };

                const VkBufferImageCopy bufferImageCopy{
                    0,                      // VkDeviceSize bufferOffset;
                    8,                      // uint32_t bufferRowLength;
                    8,                      // uint32_t bufferImageHeight;
                    defaultLayer,           // VkImageSubresourceLayers imageSubresource;
                    {0, 0, 0},              // VkOffset3D imageOffset;
                    params.src.image.extent // VkExtent3D imageExtent;
                };

                CopyRegion copyRegion;
                copyRegion.bufferImageCopy = bufferImageCopy;
                params.regions.push_back(copyRegion);

                group->addChild(new CopyImageToBufferTestCase(testCtx, "padding_bytes" + testNameSuffix, params));
            }
        }

    VkExtent3D extents[] = {
        // Most miplevels will be multiples of four. All power-of-2 edge sizes. Never a weird mip level with extents smaller than the blockwidth.
        {64, 64, 1},
        // Odd mip edge multiples, two lowest miplevels on the y-axis will have widths of 3 and 1 respectively, less than the compression blocksize, and potentially tricky.
        {64, 192, 1},
    };

    uint32_t arrayLayers[] = {1, 2, 5};

    auto getCaseName = [](VkFormat format, VkExtent3D extent, uint32_t numLayers, std::string suffix)
    {
        std::string caseName = "mip_copies_" + getFormatCaseName(format) + "_" + std::to_string(extent.width) + "x" +
                               std::to_string(extent.height);
        if (numLayers > 1)
            caseName.append("_" + std::to_string(numLayers) + "_layers");
        return caseName.append(suffix);
    };

    for (const auto &extent : extents)
        for (const auto numLayers : arrayLayers)
        {
            TestParams params;
            params.src.image.imageType       = VK_IMAGE_TYPE_2D;
            params.src.image.extent          = extent;
            params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
            params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;
            params.arrayLayers               = numLayers;

            for (const VkFormat format : formats::compressedFormatsFloats)
            {
                params.src.image.format = format;
                group->addChild(new CopyCompressedImageToBufferTestCase(
                    testCtx, getCaseName(format, params.src.image.extent, numLayers, ""), params));
#ifndef CTS_USES_VULKANSC
                params.extensionFlags = INDIRECT_COPY;
                group->addChild(new CopyCompressedImageToBufferTestCase(
                    testCtx, getCaseName(format, params.src.image.extent, numLayers, "indirect"), params));
                params.extensionFlags = NONE;
#endif
            }
        }
}

} // namespace

void add1dImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            0u,                 // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            default1dExtent     // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = bufferImageHeight;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            0u,                 // uint32_t bufferRowLength;
            bufferImageHeight,  // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            default1dExtent     // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = defaultSize * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultSize * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,      // VkDeviceSize bufferOffset;
                0u,          // uint32_t bufferRowLength;
                defaultSize, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = arrayLayers;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.buffer.size           = bufferImageHeight * arrayLayers;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.src.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = bufferImageHeight * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,            // VkDeviceSize bufferOffset;
                0u,                // uint32_t bufferRowLength;
                bufferImageHeight, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    arrayLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_larger_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = layerCount;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.src.image.fillMode        = FILL_MODE_RED;
        params.dst.buffer.size           = defaultSize * layerCount;
        params.dst.buffer.fillMode       = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0u,             // VkDeviceSize bufferOffset;
            0u,             // uint32_t bufferRowLength;
            0u,             // uint32_t bufferImageHeight;
            defaultLayer,   // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},      // VkOffset3D imageOffset;
            default1dExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.image.imageType       = VK_IMAGE_TYPE_1D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = default1dExtent;
        params.src.image.extent.depth    = layerCount;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.src.image.fillMode        = FILL_MODE_RED;
        params.dst.buffer.size           = defaultSize * layerCount;
        params.dst.buffer.fillMode       = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers defaultLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkBufferImageCopy bufferImageCopy = {
            0u,             // VkDeviceSize bufferOffset;
            0u,             // uint32_t bufferRowLength;
            0u,             // uint32_t bufferImageHeight;
            defaultLayer,   // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},      // VkOffset3D imageOffset;
            default1dExtent // VkExtent3D imageExtent;
        };

        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyImageToBufferTestCase(testCtx, "array_not_all_remaining_layers", params));
    }

    VkExtent3D extents[] = {
        // Most miplevels will be multiples of four. All power-of-2 edge sizes. Never a weird mip level with extents smaller than the blockwidth.
        {64, 64, 1},
        // Odd mip edge multiples, two lowest miplevels on the y-axis will have widths of 3 and 1 respectively, less than the compression blocksize, and potentially tricky.
        {64, 192, 1},
    };

    uint32_t arrayLayers[] = {1, 2, 5};

    auto getCaseName = [](VkFormat format, VkExtent3D extent, uint32_t numLayers)
    {
        std::string caseName = "mip_copies_" + getFormatCaseName(format) + "_" + std::to_string(extent.width) + "x" +
                               std::to_string(extent.height);
        if (numLayers > 1)
            caseName.append("_" + std::to_string(numLayers) + "_layers");
        return caseName;
    };

    for (const auto &extent : extents)
        for (const auto numLayers : arrayLayers)
        {
            TestParams params;
            params.src.image.imageType       = VK_IMAGE_TYPE_2D;
            params.src.image.extent          = extent;
            params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
            params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;
            params.arrayLayers               = numLayers;
            params.extensionFlags            = INDIRECT_COPY;

            for (const VkFormat format : formats::compressedFormatsFloats)
            {
                params.src.image.format = format;
                {
                    group->addChild(new CopyCompressedImageToBufferTestCase(
                        testCtx, getCaseName(format, params.src.image.extent, numLayers), params));
                }
            }
        }
}

void add3dImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    VkExtent3D extents[] = {
        // A power of 2 and a non-power.
        {16, 16, 16},
        {16, 8, 24},
    };

    auto getCaseName = [](VkFormat format, VkExtent3D extent)
    {
        std::string caseName = "mip_copies_" + getFormatCaseName(format) + "_" + std::to_string(extent.width) + "x" +
                               std::to_string(extent.height) + "x" + std::to_string(extent.depth);
        return caseName;
    };

    for (const auto &extent : extents)
    {
        TestParams params;
        params.src.image.imageType       = VK_IMAGE_TYPE_3D;
        params.src.image.extent          = extent;
        params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
        params.useGeneralLayout          = testGroupParams->useGeneralLayout;
        params.arrayLayers               = 1u;

        for (const VkFormat format : formats::compressedFormatsFloats)
        {
            params.src.image.format = format;
            group->addChild(
                new CopyCompressedImageToBufferTestCase(testCtx, getCaseName(format, params.src.image.extent), params));
        }
    }
}

void addCopyImageToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "1d_images", add1dImageToBufferTests, testGroupParams);
    addTestGroup(group, "2d_images", add2dImageToBufferTests, testGroupParams);
    addTestGroup(group, "3d_images", add3dImageToBufferTests, testGroupParams);
}

} // namespace api
} // namespace vkt
