#ifndef _VKTAPICOPYBUFFERTOIMAGETESTS_HPP
#define _VKTAPICOPYBUFFERTOIMAGETESTS_HPP
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
 * \brief Vulkan Copy Buffer To Depth Stencil Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyBufferToDepthStencilTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

class CopyBufferToDepthStencil : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    CopyBufferToDepthStencil(Context &context, TestParams testParams);

    virtual tcu::TestStatus iterate(void);

private:
    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);

    tcu::TextureFormat m_textureFormat;
    VkDeviceSize m_bufferSize;

    Move<VkBuffer> m_source;
    de::MovePtr<Allocation> m_sourceBufferAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
};

void CopyBufferToDepthStencil::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                        CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize        = dst.getFormat().getPixelSize();
    const VkExtent3D extent    = region.bufferImageCopy.imageExtent;
    const VkOffset3D dstOffset = region.bufferImageCopy.imageOffset;
    const int texelOffset      = (int)region.bufferImageCopy.bufferOffset / texelSize;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
            const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
                dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z, region.bufferImageCopy.imageExtent.width, 1, 1);

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

CopyBufferToDepthStencil::CopyBufferToDepthStencil(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.dst.image.format))
    , m_bufferSize(0)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();
    const VkDevice vkDevice             = m_device;
    Allocator &memAlloc                 = context.getDefaultAllocator();
    const bool hasDepth                 = tcu::hasDepthComponent(mapVkFormat(m_params.dst.image.format).order);
    const bool hasStencil               = tcu::hasStencilComponent(mapVkFormat(m_params.dst.image.format).order);

    if (!isSupportedDepthStencilFormat(vki, vkPhysDevice, testParams.dst.image.format))
    {
        TCU_THROW(NotSupportedError, "Image format not supported.");
    }

#ifndef CTS_USES_VULKANSC
    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        const auto &copyMemoryIndirectFeatures = m_context.getCopyMemoryIndirectFeatures();
        if (!copyMemoryIndirectFeatures.indirectMemoryToImageCopy)
            TCU_THROW(NotSupportedError, "Indirect memory copy to image feature not supported");

        VkPhysicalDeviceCopyMemoryIndirectPropertiesKHR copyMemoryIndirectProperties = {};
        copyMemoryIndirectProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_KHR;
        VkPhysicalDeviceProperties2 deviceProperties = {};
        deviceProperties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext                       = &copyMemoryIndirectProperties;
        vki.getPhysicalDeviceProperties2(vkPhysDevice, &deviceProperties);

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

    if (hasDepth)
    {
        glw::GLuint texelSize = m_textureFormat.getPixelSize();
        if (texelSize > sizeof(float))
        {
            // We must have D32F_S8 format, depth must be packed so we only need
            // to allocate space for the D32F part. Stencil will be separate
            texelSize = sizeof(float);
        }
        m_bufferSize += static_cast<VkDeviceSize>(m_params.dst.image.extent.width) *
                        static_cast<VkDeviceSize>(m_params.dst.image.extent.height) *
                        static_cast<VkDeviceSize>(texelSize);
    }
    if (hasStencil)
    {
        // Stencil is always 8bits and packed.
        m_bufferSize += static_cast<VkDeviceSize>(m_params.dst.image.extent.width) *
                        static_cast<VkDeviceSize>(m_params.dst.image.extent.height);
    }

    // Create source buffer, this is where the depth & stencil data will go that's used by test's regions.
    {
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (m_params.extensionFlags & INDIRECT_COPY)
            usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_bufferSize,                         // VkDeviceSize size;
            usageFlags,                           // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        m_source            = createBuffer(vk, vkDevice, &sourceBufferParams);
        m_sourceBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *m_source, MemoryRequirement::HostVisible,
                                             memAlloc, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_source, m_sourceBufferAlloc->getMemory(),
                                     m_sourceBufferAlloc->getOffset()));
    }

    // Create destination image
    {
        VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                               // VkStructureType sType;
            nullptr,                                                           // const void* pNext;
            getCreateFlags(m_params.dst.image),                                // VkImageCreateFlags flags;
            m_params.dst.image.imageType,                                      // VkImageType imageType;
            m_params.dst.image.format,                                         // VkFormat format;
            getExtent3D(m_params.dst.image),                                   // VkExtent3D extent;
            1u,                                                                // uint32_t mipLevels;
            getArraySize(m_params.dst.image),                                  // uint32_t arraySize;
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
            m_destination           = createImage(vk, m_device, &destinationImageParams);
            m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination,
                                                    MemoryRequirement::Any, *m_allocator, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                        m_destinationImageAlloc->getOffset()));
#ifndef CTS_USES_VULKANSC
        }
        else
        {
            destinationImageParams.flags |=
                (vk::VK_IMAGE_CREATE_SPARSE_BINDING_BIT | vk::VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
            vk::VkImageFormatProperties imageFormatProperties;
            if (vki.getPhysicalDeviceImageFormatProperties(
                    vkPhysDevice, destinationImageParams.format, destinationImageParams.imageType,
                    destinationImageParams.tiling, destinationImageParams.usage, destinationImageParams.flags,
                    &imageFormatProperties) == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format not supported");
            }
            m_destination     = createImage(vk, m_device, &destinationImageParams);
            m_sparseSemaphore = createSemaphore(vk, m_device);
            allocateAndBindSparseImage(vk, m_device, vkPhysDevice, vki, destinationImageParams, m_sparseSemaphore.get(),
                                       context.getSparseQueue(), *m_allocator, m_sparseAllocations,
                                       mapVkFormat(destinationImageParams.format), m_destination.get());
        }
#endif
    }
}

tcu::TestStatus CopyBufferToDepthStencil::iterate(void)
{
    // Create source depth/stencil content. Treat as 1D texture to get different pattern
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
    // Fill buffer with linear gradiant
    generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1);

    // Create image layer for depth/stencil
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.dst.image.extent.width, m_params.dst.image.extent.height,
                              m_params.dst.image.extent.depth));

    // Fill image layer with 2D gradiant
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth);

    // Fill m_extendedTextureLevel with copy of m_destinationTextureLevel
    // Then iterate over each of the regions given in m_params.regions and copy m_sourceTextureLevel content to m_extendedTextureLevel
    // This emulates what the HW will be doing.
    generateExpectedResult();

    // Upload our source depth/stencil content to the source buffer
    // This is the buffer that will be used by region commands
    std::vector<VkBufferImageCopy> bufferImageCopies;
    std::vector<VkBufferImageCopy2KHR> bufferImageCopies2KHR;
#ifndef CTS_USES_VULKANSC
    std::vector<VkCopyMemoryToImageIndirectCommandKHR> memoryImageCopiesKHR;
    VkDeviceAddress srcBufferAddress      = 0;
    VkDeviceAddress indirectBufferAddress = 0;
#endif
    VkDeviceSize bufferOffset  = 0;
    const VkDevice vkDevice    = m_device;
    const DeviceInterface &vk  = m_context.getDeviceInterface();
    char *dstPtr               = reinterpret_cast<char *>(m_sourceBufferAlloc->getHostPtr());
    bool depthLoaded           = false;
    bool stencilLoaded         = false;
    VkDeviceSize depthOffset   = 0;
    VkDeviceSize stencilOffset = 0;

#ifndef CTS_USES_VULKANSC
    const VkDeviceSize indirectBufferSize =
        de::max(m_params.regions.size(), (size_t)1) * sizeof(VkCopyMemoryToImageIndirectCommandKHR);
    Allocator &memAlloc = m_context.getDefaultAllocator();
    const BufferWithMemory indirectBuffer(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        VkBufferDeviceAddressInfo srcBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                       m_source.get()};
        srcBufferAddress = vk.getBufferDeviceAddress(m_device, &srcBufferAddressInfo);

        // indirectBuffer Address
        VkBufferDeviceAddressInfo indirectBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                            indirectBuffer.get()};
        indirectBufferAddress = vk.getBufferDeviceAddress(m_device, &indirectBufferAddressInfo);
    }
#endif

    // To be able to test ordering depth & stencil differently
    // we take the given copy regions and use that as the desired order
    // and copy the appropriate data into place and compute the appropriate
    // data offsets to be used in the copy command.
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        tcu::ConstPixelBufferAccess bufferAccess = m_sourceTextureLevel->getAccess();
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

#ifndef CTS_USES_VULKANSC
        if (m_params.extensionFlags & INDIRECT_COPY)
        {
            memoryImageCopiesKHR.push_back(convertvkBufferImageCopyTovkMemoryImageCopyKHR(srcBufferAddress, copyData));
        }
        else
#endif
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

    flushAlloc(vk, vkDevice, *m_sourceBufferAlloc);

    // Upload the depth/stencil data from m_destinationTextureLevel to initialize
    // depth and stencil to known values.
    // Uses uploadImageAspect so makes its own buffers for depth and stencil
    // aspects (as needed) and copies them with independent vkCmdCopyBufferToImage commands.
    uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image, m_params.useGeneralLayout);

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                               nullptr,                                // const void* pNext;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                                               VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
                                               VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
                                               *m_destination,                         // VkImage image;
                                               {
                                                   // VkImageSubresourceRange subresourceRange;
                                                   getAspectFlags(m_textureFormat), // VkImageAspectFlags aspectMask;
                                                   0u,                              // uint32_t baseMipLevel;
                                                   1u,                              // uint32_t mipLevels;
                                                   0u,                              // uint32_t baseArraySlice;
                                                   1u                               // uint32_t arraySize;
                                               }};

    // Copy from buffer to depth/stencil image
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    beginCommandBuffer(vk, commandBuffer);
    // Copy from buffer to depth/stencil image
    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

#ifndef CTS_USES_VULKANSC
    // Copy commands -> indirectBuffer
    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        const Allocation &bufferAllocation = indirectBuffer.getAllocation();
        invalidateAlloc(vk, vkDevice, bufferAllocation);
        uint8_t *hostPtr = (uint8_t *)bufferAllocation.getHostPtr();
        deMemcpy(hostPtr, memoryImageCopiesKHR.data(), (uint32_t)indirectBufferSize);

        std::vector<VkImageSubresourceLayers> imageSubresourceLayers;
        for (const auto &region : m_params.regions)
        {
            VkImageSubresourceLayers subresourceLayers = {};
            subresourceLayers.aspectMask               = region.bufferImageCopy.imageSubresource.aspectMask;
            subresourceLayers.mipLevel                 = region.bufferImageCopy.imageSubresource.mipLevel;
            subresourceLayers.baseArrayLayer           = region.bufferImageCopy.imageSubresource.baseArrayLayer;
            subresourceLayers.layerCount               = region.bufferImageCopy.imageSubresource.layerCount;
            imageSubresourceLayers.push_back(subresourceLayers);
        }

        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            VkStridedDeviceAddressRangeKHR addressRange                  = {indirectBufferAddress, indirectBufferSize,
                                                                            sizeof(VkCopyMemoryToImageIndirectCommandKHR)};
            VkCopyMemoryToImageIndirectInfoKHR memToImageIndirectInfoKHR = {};
            memToImageIndirectInfoKHR.sType              = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INDIRECT_INFO_KHR;
            memToImageIndirectInfoKHR.pNext              = nullptr;
            memToImageIndirectInfoKHR.srcCopyFlags       = VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR;
            memToImageIndirectInfoKHR.copyCount          = (uint32_t)m_params.regions.size();
            memToImageIndirectInfoKHR.copyAddressRange   = addressRange;
            memToImageIndirectInfoKHR.dstImage           = m_destination.get();
            memToImageIndirectInfoKHR.dstImageLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            memToImageIndirectInfoKHR.pImageSubresources = imageSubresourceLayers.data();
            vk.cmdCopyMemoryToImageIndirectKHR(*m_universalCmdBuffer, &memToImageIndirectInfoKHR);
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < memoryImageCopiesKHR.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                          1, &imageBarrier);

                VkDeviceSize stride                         = sizeof(VkCopyMemoryToImageIndirectCommandKHR);
                VkStridedDeviceAddressRangeKHR addressRange = {indirectBufferAddress + i * stride, indirectBufferSize,
                                                               stride};
                VkCopyMemoryToImageIndirectInfoKHR memToImageIndirectInfoKHR = {};
                memToImageIndirectInfoKHR.sType              = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INDIRECT_INFO_KHR;
                memToImageIndirectInfoKHR.pNext              = nullptr;
                memToImageIndirectInfoKHR.srcCopyFlags       = VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR;
                memToImageIndirectInfoKHR.copyCount          = 1;
                memToImageIndirectInfoKHR.copyAddressRange   = addressRange;
                memToImageIndirectInfoKHR.dstImage           = m_destination.get();
                memToImageIndirectInfoKHR.dstImageLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                memToImageIndirectInfoKHR.pImageSubresources = &imageSubresourceLayers[i];
                vk.cmdCopyMemoryToImageIndirectKHR(*m_universalCmdBuffer, &memToImageIndirectInfoKHR);
            }
        }
    }
    else
#endif
        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            vk.cmdCopyBufferToImage(commandBuffer, m_source.get(), m_destination.get(),
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)m_params.regions.size(),
                                    bufferImageCopies.data());
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < bufferImageCopies.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

                vk.cmdCopyBufferToImage(commandBuffer, m_source.get(), m_destination.get(),
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopies[i]);
            }
        }
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);

        if (m_params.singleCommand)
        {
            // Issue a single copy command with regions defined by the test.
            const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR, // VkStructureType sType;
                nullptr,                                           // const void* pNext;
                m_source.get(),                                    // VkBuffer srcBuffer;
                m_destination.get(),                               // VkImage dstImage;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,              // VkImageLayout dstImageLayout;
                (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
                bufferImageCopies2KHR.data()                       // const VkBufferImageCopy2KHR* pRegions;
            };
            vk.cmdCopyBufferToImage2(commandBuffer, &copyBufferToImageInfo2KHR);
        }
        else
        {
            // Issue a a copy command per region defined by the test.
            for (uint32_t i = 0; i < bufferImageCopies2KHR.size(); i++)
            {
                if (i > 0)
                    vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

                const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR = {
                    VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR, // VkStructureType sType;
                    nullptr,                                           // const void* pNext;
                    m_source.get(),                                    // VkBuffer srcBuffer;
                    m_destination.get(),                               // VkImage dstImage;
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,              // VkImageLayout dstImageLayout;
                    1,                                                 // uint32_t regionCount;
                    &bufferImageCopies2KHR[i]                          // const VkBufferImageCopy2KHR* pRegions;
                };
                // Issue a single copy command with regions defined by the test.
                vk.cmdCopyBufferToImage2(commandBuffer, &copyBufferToImageInfo2KHR);
            }
        }
    }

    endCommandBuffer(vk, commandBuffer);

    const bool indirectCopy = (m_params.extensionFlags & INDIRECT_COPY);
    submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore, indirectCopy);

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, m_params.dst.image);

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

class CopyBufferToDepthStencilTestCase : public vkt::TestCase
{
public:
    CopyBufferToDepthStencilTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~CopyBufferToDepthStencilTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyBufferToDepthStencil(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);
        context.requireDeviceFunctionality("VK_KHR_format_feature_flags2");

#ifndef CTS_USES_VULKANSC
        if (m_params.queueSelection != QueueSelectionOptions::Universal)
        {
            vk::VkFormatProperties3 formatProperties3{};
            formatProperties3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
            vk::VkFormatProperties2 formatProperties{};
            formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            formatProperties.pNext = &formatProperties3;
            context.getInstanceInterface().getPhysicalDeviceFormatProperties2(
                context.getPhysicalDevice(), m_params.dst.image.format, &formatProperties);

            VkImageAspectFlags requiredAspects = 0U;
            for (auto const &region : m_params.regions)
                requiredAspects |= region.bufferImageCopy.imageSubresource.aspectMask;

            // The get*Queue() methods will throw NotSupportedError if the queue is not available.
            if (m_params.queueSelection == QueueSelectionOptions::ComputeOnly)
            {
                context.getComputeQueue();

                if (isDepthStencilFormat(m_params.dst.image.format))
                {
                    tcu::TextureFormat format = mapVkFormat(m_params.dst.image.format);
                    if (tcu::hasDepthComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.dst.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }

                    if (tcu::hasStencilComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.dst.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_COMPUTE_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }
                }
            }
            else if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
            {
                context.getTransferQueue();

                if (isDepthStencilFormat(m_params.dst.image.format))
                {
                    tcu::TextureFormat format = mapVkFormat(m_params.dst.image.format);
                    if (tcu::hasDepthComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.dst.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_DEPTH_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }

                    if (tcu::hasStencilComponent(format.order) && (requiredAspects & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                        (formatProperties3.optimalTilingFeatures &
                         VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR) == 0ULL)
                    {
                        std::ostringstream msg;
                        msg << "Format " << getFormatName(m_params.dst.image.format)
                            << " does not support VK_FORMAT_FEATURE_2_STENCIL_COPY_ON_TRANSFER_QUEUE_BIT_KHR";
                        TCU_THROW(NotSupportedError, msg.str());
                    }
                }
            }
        }
        if (m_params.extensionFlags & INDIRECT_COPY)
        {
            VkFormatProperties3 formatProps3;
            formatProps3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
            formatProps3.pNext = nullptr;

            VkFormatProperties2 formatProps2;
            formatProps2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            formatProps2.pNext = &formatProps3;
            context.getInstanceInterface().getPhysicalDeviceFormatProperties2(context.getPhysicalDevice(),
                                                                              m_params.dst.image.format, &formatProps2);

            if (m_params.dst.image.tiling == VK_IMAGE_TILING_OPTIMAL)
            {
                if (!(formatProps3.optimalTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR))
                    TCU_THROW(NotSupportedError, "Format feature is not supported on this format");
            }
            if (m_params.dst.image.tiling == VK_IMAGE_TILING_LINEAR)
            {
                if (!(formatProps3.linearTilingFeatures & VK_FORMAT_FEATURE_2_COPY_IMAGE_INDIRECT_DST_BIT_KHR))
                    TCU_THROW(NotSupportedError, "Format feature is not supported on this format");
            }
        }
#endif // CTS_USES_VULKANSC
    }

private:
    TestParams m_params;
};

} // namespace

void addCopyBufferToDepthStencilTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

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
        32,                                          // VkDeviceSize bufferOffset;
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

    // Note: Depth stencil tests I want to do
    // Formats: D16, D24S8, D32FS8
    // Test writing each component with separate CopyBufferToImage commands
    // Test writing both components in one CopyBufferToImage command
    // Swap order of writes of Depth & Stencil
    // whole surface, subimages?
    // Similar tests as BufferToImage?
    for (const VkFormat format : formats::depthAndStencilFormats)
        for (const auto offset : useOffset)
        {
            // TODO: Check that this format is supported before creating tests?
            //if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))

            CopyRegion copyDepthRegion;
            CopyRegion copyStencilRegion;
            TestParams params;
            const tcu::TextureFormat texFormat = mapVkFormat(format);
            const bool hasDepth                = tcu::hasDepthComponent(texFormat.order);
            const bool hasStencil              = tcu::hasStencilComponent(texFormat.order);
            std::string testName               = getFormatCaseName(format);

            if (offset)
            {
                copyDepthRegion.bufferImageCopy   = bufferDepthCopyOffset;
                copyStencilRegion.bufferImageCopy = bufferStencilCopyOffset;
                testName                          = "buffer_offset_" + testName;
                params.src.buffer.size = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
            }
            else
            {
                copyDepthRegion.bufferImageCopy   = bufferDepthCopy;
                copyStencilRegion.bufferImageCopy = bufferStencilCopy;
                params.src.buffer.size            = defaultSize * defaultSize;
            }

            params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
            params.dst.image.format          = format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;

            if (hasDepth && hasStencil)
            {
                params.singleCommand = true;

                params.regions.push_back(copyDepthRegion);
                params.regions.push_back(copyStencilRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_DS", params));

                params.singleCommand = false;

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_D_S", params));

                params.regions.clear();
                params.regions.push_back(copyStencilRegion);
                params.regions.push_back(copyDepthRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_S_D", params));

                params.singleCommand = true;
                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_SD", params));
            }

            if (hasStencil)
            {
                params.regions.clear();
                params.regions.push_back(copyStencilRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_S", params));
            }

            if (hasDepth)
            {
                params.regions.clear();
                params.regions.push_back(copyDepthRegion);

                group->addChild(new CopyBufferToDepthStencilTestCase(testCtx, testName + "_D", params));
            }
        }
}

} // namespace api
} // namespace vkt

#endif // _VKTAPICOPYBUFFERTOBUFFERTESTS_HPP
