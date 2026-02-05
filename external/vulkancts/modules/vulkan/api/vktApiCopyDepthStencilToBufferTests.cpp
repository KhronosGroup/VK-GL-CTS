#ifndef _VKTAPICOPYIMAGETOBUFFERTESTS_HPP
#define _VKTAPICOPYIMAGETOBUFFERTESTS_HPP
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
 * \brief Vulkan Copy DepthStencil To Buffer Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyDepthStencilToBufferTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

class CopyDepthStencilToBuffer : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    CopyDepthStencilToBuffer(Context &context, TestParams testParams);
    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);

    tcu::TextureFormat m_textureFormat;
    VkDeviceSize m_bufferSize;

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    Move<VkBuffer> m_destination;
    de::MovePtr<Allocation> m_destinationBufferAlloc;
};

void CopyDepthStencilToBuffer::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                        CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize        = src.getFormat().getPixelSize();
    const VkExtent3D extent    = region.bufferImageCopy.imageExtent;
    const VkOffset3D srcOffset = region.bufferImageCopy.imageOffset;
    const int texelOffset      = (int)region.bufferImageCopy.bufferOffset / texelSize;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex                                 = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion = tcu::getSubregion(
                src, srcOffset.x, srcOffset.y + y, srcOffset.z + z, region.bufferImageCopy.imageExtent.width, 1, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);

            if (region.bufferImageCopy.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
            {
                tcu::copy(dstSubRegion, tcu::getEffectiveDepthStencilAccess(srcSubRegion, tcu::Sampler::MODE_DEPTH),
                          false);
            }
            else
            {
                tcu::copy(dstSubRegion, tcu::getEffectiveDepthStencilAccess(srcSubRegion, tcu::Sampler::MODE_STENCIL),
                          false);
            }
        }
    }
}

CopyDepthStencilToBuffer::CopyDepthStencilToBuffer(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.src.image.format))
    , m_bufferSize(0)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();
    const bool hasDepth                 = tcu::hasDepthComponent(mapVkFormat(m_params.src.image.format).order);
    const bool hasStencil               = tcu::hasStencilComponent(mapVkFormat(m_params.src.image.format).order);

    if (!isSupportedDepthStencilFormat(vki, vkPhysDevice, testParams.src.image.format))
    {
        TCU_THROW(NotSupportedError, "Image format not supported.");
    }

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
        if (!testParams.useSparseBinding)
        {
#endif
            m_source           = createImage(vk, m_device, &sourceImageParams);
            m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, m_source.get(), MemoryRequirement::Any,
                                               *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, m_source.get(), m_sourceImageAlloc->getMemory(),
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
            m_source          = createImage(vk, m_device, &sourceImageParams);
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, sourceImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(sourceImageParams.format), m_source.get());
        }
#endif
    }

    if (hasDepth)
    {
        glw::GLuint texelSize = m_textureFormat.getPixelSize();
        if (texelSize > sizeof(float))
        {
            // We must have D32F_S8 format, depth must be packed so we only need
            // to allocate space for the D32F part. Stencil will be separate
            texelSize = sizeof(float);
        }
        m_bufferSize += static_cast<VkDeviceSize>(m_params.src.image.extent.width) *
                        static_cast<VkDeviceSize>(m_params.src.image.extent.height) *
                        static_cast<VkDeviceSize>(texelSize);
    }
    if (hasStencil)
    {
        // Stencil is always 8bits and packed.
        m_bufferSize += static_cast<VkDeviceSize>(m_params.src.image.extent.width) *
                        static_cast<VkDeviceSize>(m_params.src.image.extent.height);
    }

    // Create source buffer, this is where the depth & stencil data will go that's used by test's regions.
    {
        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                // VkStructureType sType;
            nullptr,                                                             // const void* pNext;
            0u,                                                                  // VkBufferCreateFlags flags;
            m_bufferSize,                                                        // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                           // VkSharingMode sharingMode;
            0u,                                                                  // uint32_t queueFamilyIndexCount;
            nullptr,                                                             // const uint32_t* pQueueFamilyIndices;
        };

        m_destination            = createBuffer(vk, vkDevice, &sourceBufferParams);
        m_destinationBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, m_destination.get(),
                                                  MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, m_destination.get(), m_destinationBufferAlloc->getMemory(),
                                     m_destinationBufferAlloc->getOffset()));
    }
}

tcu::TestStatus CopyDepthStencilToBuffer::iterate(void)
{
    // Create source image layer for depth/stencil
    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.src.image.extent.width, m_params.src.image.extent.height,
                              m_params.src.image.extent.depth));

    // Fill image layer with 2D gradiant
    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.src.image.extent.depth);

    // Create destination buffer. Treat as 1D texture to get different pattern
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
    // Fill buffer with linear gradiant
    generateBuffer(m_destinationTextureLevel->getAccess(), (int)m_params.dst.buffer.size, 1, 1);

    // Fill m_extendedTextureLevel with copy of m_destinationTextureLevel
    // Then iterate over each of the regions given in m_params.regions and copy m_sourceTextureLevel content to m_extendedTextureLevel
    // This emulates what the HW will be doing.
    generateExpectedResult();

    // Upload our source depth/stencil content to the source buffer
    // This is the buffer that will be used by region commands
    std::vector<VkBufferImageCopy> bufferImageCopies;
    std::vector<VkBufferImageCopy2KHR> bufferImageCopies2KHR;
    VkDeviceSize bufferOffset  = 0;
    const VkDevice vkDevice    = m_device;
    const DeviceInterface &vk  = m_context.getDeviceInterface();
    char *dstPtr               = reinterpret_cast<char *>(m_destinationBufferAlloc->getHostPtr());
    bool depthLoaded           = false;
    bool stencilLoaded         = false;
    VkDeviceSize depthOffset   = 0;
    VkDeviceSize stencilOffset = 0;

    // To be able to test ordering depth & stencil differently
    // we take the given copy regions and use that as the desired order
    // and copy the appropriate data into place and compute the appropriate
    // data offsets to be used in the copy command.
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        tcu::ConstPixelBufferAccess bufferAccess = m_destinationTextureLevel->getAccess();
        uint32_t bufferSize        = bufferAccess.getWidth() * bufferAccess.getHeight() * bufferAccess.getDepth();
        VkBufferImageCopy copyData = m_params.regions[i].bufferImageCopy;
        char *srcPtr;

        if (copyData.imageSubresource.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT && !depthLoaded)
        {
            // Create level that is same component as depth buffer (e.g. D16, D24, D32F)
            tcu::TextureLevel depthTexture(mapCombinedToDepthTransferFormat(bufferAccess.getFormat()),
                                           bufferAccess.getWidth(), bufferAccess.getHeight(), bufferAccess.getDepth());
            bufferSize *= tcu::getPixelSize(depthTexture.getFormat());
            // Copy depth component only from source data. This gives us packed depth-only data.
            tcu::copy(depthTexture.getAccess(),
                      tcu::getEffectiveDepthStencilAccess(bufferAccess, tcu::Sampler::MODE_DEPTH));
            srcPtr = (char *)depthTexture.getAccess().getDataPtr();
            // Copy packed depth-only data to output buffer
            deMemcpy(dstPtr, srcPtr, bufferSize);
            depthLoaded = true;
            depthOffset = bufferOffset;
            dstPtr += bufferSize;
            bufferOffset += bufferSize;
            copyData.bufferOffset += depthOffset;
        }
        else if (!stencilLoaded)
        {
            // Create level that is same component as stencil buffer (always 8-bits)
            tcu::TextureLevel stencilTexture(
                tcu::getEffectiveDepthStencilTextureFormat(bufferAccess.getFormat(), tcu::Sampler::MODE_STENCIL),
                bufferAccess.getWidth(), bufferAccess.getHeight(), bufferAccess.getDepth());
            // Copy stencil component only from source data. This gives us packed stencil-only data.
            tcu::copy(stencilTexture.getAccess(),
                      tcu::getEffectiveDepthStencilAccess(bufferAccess, tcu::Sampler::MODE_STENCIL));
            srcPtr = (char *)stencilTexture.getAccess().getDataPtr();
            // Copy packed stencil-only data to output buffer
            deMemcpy(dstPtr, srcPtr, bufferSize);
            stencilLoaded = true;
            stencilOffset = bufferOffset;
            dstPtr += bufferSize;
            bufferOffset += bufferSize;

            // Reference image generation uses pixel offsets based on buffer offset.
            // We need to adjust the offset now that the stencil data is not interleaved.
            copyData.bufferOffset /= tcu::getPixelSize(m_textureFormat);

            copyData.bufferOffset += stencilOffset;
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            bufferImageCopies.push_back(copyData);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            bufferImageCopies2KHR.push_back(convertvkBufferImageCopyTovkBufferImageCopy2KHR(copyData));
        }
    }

    flushAlloc(vk, vkDevice, *m_destinationBufferAlloc);

    // Upload the depth/stencil data from m_destinationTextureLevel to initialize
    // depth and stencil to known values.
    // Uses uploadImageAspect so makes its own buffers for depth and stencil
    // aspects (as needed) and copies them with independent vkCmdCopyBufferToImage commands.
    uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image, m_params.useGeneralLayout);

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               nullptr,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               m_source.get(),                         // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
                                                   0u,                              // uint32_t baseMipLevel;
                                                   1u,                              // uint32_t mipLevels;
                                                   0u,                              // uint32_t baseArraySlice;
                                                   1u                               // uint32_t arraySize;
                                               }};

    const VkImageMemoryBarrier tempImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_source.get(),                         // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            1u                               // uint32_t arraySize;
        }};

    // Copy from depth/stencil image to buffer
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    beginCommandBuffer(vk, commandBuffer);
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            vk.cmdCopyImageToBuffer(commandBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    m_destination.get(), (uint32_t)m_params.regions.size(), bufferImageCopies.data());
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < bufferImageCopies.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &tempImageBarrier);

                vk.cmdCopyImageToBuffer(commandBuffer, m_source.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        m_destination.get(), 1, &bufferImageCopies[i]);
            }
        }
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);

        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
                nullptr,                                           // const void* pNext;
                m_source.get(),                                    // VkImage dstImage;
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,              // VkImageLayout dstImageLayout;
                m_destination.get(),                               // VkBuffer srcBuffer;
                (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
                bufferImageCopies2KHR.data()                       // const VkBufferImageCopy2KHR* pRegions;
            };
            vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < bufferImageCopies2KHR.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &tempImageBarrier);

                const VkCopyImageToBufferInfo2KHR copyImageToBufferInfo2KHR = {
                    VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR, // VkStructureType sType;
                    nullptr,                                           // const void* pNext;
                    m_source.get(),                                    // VkImage dstImage;
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,              // VkImageLayout dstImageLayout;
                    m_destination.get(),                               // VkBuffer srcBuffer;
                    1,                                                 // uint32_t regionCount;
                    &bufferImageCopies2KHR[i]                          // const VkBufferImageCopy2KHR* pRegions;
                };
                // Issue a single copy command with regions defined by the test.
                vk.cmdCopyImageToBuffer2(commandBuffer, &copyImageToBufferInfo2KHR);
            }
        }
    }

    endCommandBuffer(vk, commandBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    // Read buffer data
    de::MovePtr<tcu::TextureLevel> resultLevel(
        new tcu::TextureLevel(m_textureFormat, (int)m_params.dst.buffer.size, 1));
    invalidateAlloc(vk, vkDevice, *m_destinationBufferAlloc);

    if (depthLoaded)
    {
        tcu::TextureFormat depthFormat = mapCombinedToDepthTransferFormat(m_textureFormat);
        uint8_t *pDepthData            = (uint8_t *)m_destinationBufferAlloc->getHostPtr() + depthOffset;
        tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_DEPTH),
                  tcu::getEffectiveDepthStencilAccess(
                      tcu::ConstPixelBufferAccess(depthFormat, resultLevel->getSize(), pDepthData),
                      tcu::Sampler::MODE_DEPTH));
    }
    if (stencilLoaded)
    {
        tcu::TextureFormat stencilFormat =
            tcu::getEffectiveDepthStencilTextureFormat(resultLevel->getFormat(), tcu::Sampler::MODE_STENCIL);
        uint8_t *pStencilData = (uint8_t *)m_destinationBufferAlloc->getHostPtr() + stencilOffset;
        tcu::copy(tcu::getEffectiveDepthStencilAccess(resultLevel->getAccess(), tcu::Sampler::MODE_STENCIL),
                  tcu::getEffectiveDepthStencilAccess(
                      tcu::ConstPixelBufferAccess(stencilFormat, resultLevel->getSize(), pStencilData),
                      tcu::Sampler::MODE_STENCIL));
    }

    // For combined depth/stencil formats both aspects are checked even when the test only
    // copies one. Clear such aspects here for both the result and the reference.
    if (tcu::hasDepthComponent(m_textureFormat.order) && !depthLoaded)
    {
        tcu::clearDepth(m_expectedTextureLevel[0]->getAccess(), 0.0f);
        tcu::clearDepth(resultLevel->getAccess(), 0.0f);
    }
    if (tcu::hasStencilComponent(m_textureFormat.order) && !stencilLoaded)
    {
        tcu::clearStencil(m_expectedTextureLevel[0]->getAccess(), 0);
        tcu::clearStencil(resultLevel->getAccess(), 0);
    }

    return checkTestResult(resultLevel->getAccess());
}

class CopyDepthStencilToBufferTestCase : public vkt::TestCase
{
public:
    CopyDepthStencilToBufferTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyDepthStencilToBuffer(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);

#ifndef CTS_USES_VULKANSC
        if (m_params.queueSelection != QueueSelectionOptions::Universal)
        {
            context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

            vk::VkFormatProperties3 formatProperties3{};
            formatProperties3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
            vk::VkFormatProperties2 formatProperties{};
            formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            formatProperties.pNext = &formatProperties3;
            context.getInstanceInterface().getPhysicalDeviceFormatProperties2(
                context.getPhysicalDevice(), m_params.src.image.format, &formatProperties);

            VkImageAspectFlags requiredAspects = 0U;
            for (auto const &region : m_params.regions)
                requiredAspects |= region.bufferImageCopy.imageSubresource.aspectMask;

            // The get*Queue() methods will throw NotSupportedError if the queue is not available.
            if (m_params.queueSelection == QueueSelectionOptions::ComputeOnly)
            {
                context.getComputeQueue();

                if (isDepthStencilFormat(m_params.src.image.format))
                {
                    tcu::TextureFormat format = mapVkFormat(m_params.src.image.format);
                    if (tcu::hasDepthComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.src.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }

                    if (tcu::hasStencilComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.src.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }
                }
            }
            else if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
            {
                context.getTransferQueue();

                if (isDepthStencilFormat(m_params.src.image.format))
                {
                    tcu::TextureFormat format = mapVkFormat(m_params.src.image.format);
                    if (tcu::hasDepthComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.src.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }

                    if (tcu::hasStencilComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.src.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }
                }
            }
        }
#endif // CTS_USES_VULKANSC
    }

private:
    TestParams m_params;
};

} // namespace

void addCopyDepthStencilToBufferTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    const struct
    {
        const char *name;
        const VkFormat format;
    } depthAndStencilFormats[] = {{"s8_uint", VK_FORMAT_S8_UINT},
                                  {"d16_unorm", VK_FORMAT_D16_UNORM},
                                  {"x8_d24_unorm_pack32", VK_FORMAT_X8_D24_UNORM_PACK32},
                                  {"d32_sfloat", VK_FORMAT_D32_SFLOAT},
                                  {"d16_unorm_s8_uint", VK_FORMAT_D16_UNORM_S8_UINT},
                                  {"d24_unorm_s8_uint", VK_FORMAT_D24_UNORM_S8_UINT},
                                  {"d32_sfloat_s8_uint", VK_FORMAT_D32_SFLOAT_S8_UINT}};

    const VkImageSubresourceLayers depthSourceLayer = {
        VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
        0u,                        // uint32_t mipLevel;
        0u,                        // uint32_t baseArrayLayer;
        1u,                        // uint32_t layerCount;
    };

    const VkBufferImageCopy bufferDepthCopy = {
        0u,               // VkDeviceSize bufferOffset;
        0u,               // uint32_t bufferRowLength;
        0u,               // uint32_t bufferImageHeight;
        depthSourceLayer, // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},        // VkOffset3D imageOffset;
        defaultExtent     // VkExtent3D imageExtent;
    };

    const VkBufferImageCopy bufferDepthCopyOffset = {
        0u,                                          // VkDeviceSize bufferOffset;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
        depthSourceLayer,                            // VkImageSubresourceLayers imageSubresource;
        {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
        defaultHalfExtent                            // VkExtent3D imageExtent;
    };

    const VkImageSubresourceLayers stencilSourceLayer = {
        VK_IMAGE_ASPECT_STENCIL_BIT, // VkImageAspectFlags aspectMask;
        0u,                          // uint32_t mipLevel;
        0u,                          // uint32_t baseArrayLayer;
        1u,                          // uint32_t layerCount;
    };

    const VkBufferImageCopy bufferStencilCopy = {
        0u,                 // VkDeviceSize bufferOffset;
        0u,                 // uint32_t bufferRowLength;
        0u,                 // uint32_t bufferImageHeight;
        stencilSourceLayer, // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},          // VkOffset3D imageOffset;
        defaultExtent       // VkExtent3D imageExtent;
    };

    const VkBufferImageCopy bufferStencilCopyOffset = {
        32,                                          // VkDeviceSize bufferOffset;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
        defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
        stencilSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
        {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
        defaultHalfExtent                            // VkExtent3D imageExtent;
    };

    const bool useOffset[] = {false, true};

    for (const auto &config : depthAndStencilFormats)
    {
        for (const auto offset : useOffset)
        {
            CopyRegion copyDepthRegion;
            CopyRegion copyStencilRegion;
            TestParams params;
            const tcu::TextureFormat format = mapVkFormat(config.format);
            const bool hasDepth             = tcu::hasDepthComponent(format.order);
            const bool hasStencil           = tcu::hasStencilComponent(format.order);
            std::string testName            = config.name;

            if (offset)
            {
                copyDepthRegion.bufferImageCopy   = bufferDepthCopyOffset;
                copyStencilRegion.bufferImageCopy = bufferStencilCopyOffset;
                testName                          = "buffer_offset_" + testName;
                params.dst.buffer.size = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
            }
            else
            {
                copyDepthRegion.bufferImageCopy   = bufferDepthCopy;
                copyStencilRegion.bufferImageCopy = bufferStencilCopy;
                params.dst.buffer.size            = defaultSize * defaultSize;
            }

            params.src.image.imageType       = VK_IMAGE_TYPE_2D;
            params.src.image.format          = config.format;
            params.src.image.extent          = defaultExtent;
            params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
            params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;

            if (hasDepth && hasStencil)
            {
                params.singleCommand = true;

                // DS order
                params.regions.push_back(copyDepthRegion);
                params.regions.push_back(copyStencilRegion);
                group->addChild(new CopyDepthStencilToBufferTestCase(testCtx, testName + "_DS", params));

                // Separate commands
                params.singleCommand = false;
                group->addChild(new CopyDepthStencilToBufferTestCase(testCtx, testName + "_D_S", params));

                // SD order
                params.regions.clear();
                params.regions.push_back(copyStencilRegion);
                params.regions.push_back(copyDepthRegion);
                group->addChild(new CopyDepthStencilToBufferTestCase(testCtx, testName + "_SD", params));

                // Combined SD
                params.singleCommand = true;
                group->addChild(new CopyDepthStencilToBufferTestCase(testCtx, testName + "_SD_combined", params));
            }

            if (hasDepth)
            {
                params.regions.clear();
                params.regions.push_back(copyDepthRegion);
                group->addChild(new CopyDepthStencilToBufferTestCase(testCtx, testName + "_D", params));
            }

            if (hasStencil)
            {
                params.regions.clear();
                params.regions.push_back(copyStencilRegion);
                group->addChild(new CopyDepthStencilToBufferTestCase(testCtx, testName + "_S", params));
            }
        }
    }
}

} // namespace api
} // namespace vkt

#endif // _VKTAPICOPYIMAGETOBUFFERTESTS_HPP
