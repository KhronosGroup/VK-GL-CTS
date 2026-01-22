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
 * \brief Vulkan Copy Memory Indirect Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyMemoryIndirectTests.hpp"
#include "vktApiCopyImageToBufferTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

#ifndef CTS_USES_VULKANSC

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

class CopyMemoryToImageIndirect : public CopiesAndBlittingTestInstance
{
public:
    CopyMemoryToImageIndirect(Context &context, TestParams testParams);
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
    Move<VkSemaphore> m_sparseSemaphore;
};

CopyMemoryToImageIndirect::CopyMemoryToImageIndirect(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstance(context, testParams)
    , m_textureFormat(mapVkFormat(testParams.dst.image.format))
    , m_bufferSize(m_params.src.buffer.size * tcu::getPixelSize(m_textureFormat))
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source buffer
    {
        const VkBufferCreateInfo sourceBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_bufferSize,                         // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            (const uint32_t *)nullptr,            // const uint32_t* pQueueFamilyIndices;
        };

        m_source            = createBuffer(vk, m_device, &sourceBufferParams);
        m_sourceBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::HostVisible,
                                             *m_allocator, m_params.allocationKind);
        VK_CHECK(vk.bindBufferMemory(m_device, *m_source, m_sourceBufferAlloc->getMemory(),
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
            (const uint32_t *)nullptr,                                         // const uint32_t* pQueueFamilyIndices;
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

void CopyMemoryToImageIndirect::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                         CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    uint32_t rowLength = region.bufferImageCopy.bufferRowLength;
    if (!rowLength)
        rowLength = region.bufferImageCopy.imageExtent.width;

    uint32_t imageHeight = region.bufferImageCopy.bufferImageHeight;
    if (!imageHeight)
        imageHeight = region.bufferImageCopy.imageExtent.height;

    const int texelSize           = dst.getFormat().getPixelSize();
    const VkExtent3D extent       = region.bufferImageCopy.imageExtent;
    const VkOffset3D dstOffset    = region.bufferImageCopy.imageOffset;
    const int texelOffset         = (int)region.bufferImageCopy.bufferOffset / texelSize;
    const uint32_t baseArrayLayer = region.bufferImageCopy.imageSubresource.baseArrayLayer;

    for (uint32_t z = 0; z < extent.depth; z++)
    {
        for (uint32_t y = 0; y < extent.height; y++)
        {
            int texelIndex = texelOffset + (z * imageHeight + y) * rowLength;
            const tcu::ConstPixelBufferAccess srcSubRegion =
                tcu::getSubregion(src, texelIndex, 0, region.bufferImageCopy.imageExtent.width, 1);
            const tcu::PixelBufferAccess dstSubRegion =
                tcu::getSubregion(dst, dstOffset.x, dstOffset.y + y, dstOffset.z + z + baseArrayLayer,
                                  region.bufferImageCopy.imageExtent.width, 1, 1);
            tcu::copy(dstSubRegion, srcSubRegion);
        }
    }
}

tcu::TestStatus CopyMemoryToImageIndirect::iterate(void)
{
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(m_textureFormat, (int)m_params.src.buffer.size, 1));
    generateBuffer(m_sourceTextureLevel->getAccess(), (int)m_params.src.buffer.size, 1, 1,
                   m_params.src.buffer.fillMode);
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(m_textureFormat, m_params.dst.image.extent.width, m_params.dst.image.extent.height,
                              m_params.dst.image.extent.depth));

    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth, m_params.dst.image.fillMode);

    if (m_params.dst.image.imageType == VK_IMAGE_TYPE_3D)
    {
        CopyRegion copyRegion;
        {
            const VkBufferImageCopy bufferImageCopy = {
                0,  // VkDeviceSize bufferOffset;
                0u, // uint32_t bufferRowLength
                0u, // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    0,                         // uint32_t baseArrayLayer;
                    1,                         // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                m_params.dst.image.extent      // VkExtent3D imageExtent;
            };
            copyRegion.bufferImageCopy = bufferImageCopy;
        }
        generateExpectedResult(&copyRegion);
    }
    else
        generateExpectedResult();

    uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
    uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image, m_params.mipLevels, true,
                nullptr);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    const vk::InstanceInterface &vki            = m_context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice   = m_context.getPhysicalDevice();
    Allocator &memAlloc                         = m_context.getDefaultAllocator();
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    VkPhysicalDeviceCopyMemoryIndirectPropertiesKHR copyMemoryIndirectProperties = {};
    copyMemoryIndirectProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 deviceProperties = {};
    deviceProperties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties.pNext                       = &copyMemoryIndirectProperties;

    vki.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

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

    // Create a 4-byte host accessible buffer for conditional predicate
    Move<VkBuffer> predicateBuffer;
    de::MovePtr<Allocation> predicateAlloc;

    if (m_params.useConditionalRender)
    {
        const VkDeviceSize bufferSize             = 4; // 4 bytes
        const VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                     nullptr,
                                                     0u,
                                                     bufferSize,
                                                     VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT |
                                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                     VK_SHARING_MODE_EXCLUSIVE,
                                                     0u,
                                                     nullptr};

        predicateBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
        predicateAlloc  = allocateBuffer(vki, vk, physicalDevice, vkDevice, *predicateBuffer,
                                         MemoryRequirement::HostVisible, *m_allocator, m_params.allocationKind);

        VK_CHECK(
            vk.bindBufferMemory(vkDevice, *predicateBuffer, predicateAlloc->getMemory(), predicateAlloc->getOffset()));

        // Write the predicate value to the buffer
        uint32_t *hostPtr = static_cast<uint32_t *>(predicateAlloc->getHostPtr());
        *hostPtr          = m_params.conditionalPredicate;

        // Make sure the write is visible to the device
        flushAlloc(vk, vkDevice, *predicateAlloc);
    }

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
                                                   getArraySize(m_params.dst.image) // uint32_t arraySize;
                                               }};

    // Copy from buffer to image
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
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)nullptr, 0,
                          (const VkBufferMemoryBarrier *)nullptr, 1, &imageBarrier);

    VkBufferDeviceAddressInfo srcBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                   m_source.get()};
    VkDeviceAddress srcBufferAddress = vk.getBufferDeviceAddress(m_device, &srcBufferAddressInfo);

    // Use a different struct to verify stride > sizeof(VkCopyMemoryToImageIndirectCommandKHR)
    struct IndirectImageParams
    {
        VkCopyMemoryToImageIndirectCommandKHR args;
        uint32_t dummyparam1;
        uint32_t dummyparam2;
    };

    const VkDeviceSize indirectBufferSize = de::max(m_params.regions.size(), (size_t)1) * sizeof(IndirectImageParams);
    const BufferWithMemory indirectBuffer(
        vk, vkDevice, memAlloc, makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    // indirectBuffer Address
    VkBufferDeviceAddressInfo indirectBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                        indirectBuffer.get()};
    VkDeviceAddress indirectBufferAddress = vk.getBufferDeviceAddress(m_device, &indirectBufferAddressInfo);
    // indirectBuffer Commands
    std::vector<IndirectImageParams> indirectCommands;
    for (const auto &region : m_params.regions)
    {
        IndirectImageParams command    = {};
        command.args.srcAddress        = srcBufferAddress + region.bufferImageCopy.bufferOffset;
        command.args.bufferRowLength   = region.bufferImageCopy.bufferRowLength;
        command.args.bufferImageHeight = region.bufferImageCopy.bufferImageHeight;
        command.args.imageSubresource  = region.bufferImageCopy.imageSubresource;
        command.args.imageOffset       = region.bufferImageCopy.imageOffset;
        command.args.imageExtent       = region.bufferImageCopy.imageExtent;
        indirectCommands.push_back(command);
    }

    // Copy commands -> indirectBuffer
    const Allocation &bufferAllocation = indirectBuffer.getAllocation();
    invalidateAlloc(vk, vkDevice, bufferAllocation);
    uint8_t *hostPtr = (uint8_t *)bufferAllocation.getHostPtr();
    deMemcpy(hostPtr, indirectCommands.data(), (uint32_t)indirectBufferSize);

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

    VkStridedDeviceAddressRangeKHR addressRange                  = {indirectBufferAddress, indirectBufferSize,
                                                                    sizeof(IndirectImageParams)};
    VkCopyMemoryToImageIndirectInfoKHR memToImageIndirectInfoKHR = {};
    memToImageIndirectInfoKHR.sType              = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INDIRECT_INFO_KHR;
    memToImageIndirectInfoKHR.pNext              = nullptr;
    memToImageIndirectInfoKHR.srcCopyFlags       = VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR;
    memToImageIndirectInfoKHR.copyCount          = (uint32_t)m_params.regions.size();
    memToImageIndirectInfoKHR.copyAddressRange   = addressRange;
    memToImageIndirectInfoKHR.dstImage           = m_destination.get();
    memToImageIndirectInfoKHR.dstImageLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    memToImageIndirectInfoKHR.pImageSubresources = imageSubresourceLayers.data();

    if (m_params.useConditionalRender)
    {
        // Begin conditional rendering
        const VkConditionalRenderingBeginInfoEXT conditionalRenderingBeginInfo = {
            VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT, // VkStructureType sType
            nullptr,                                                // const void* pNext
            *predicateBuffer,                                       // VkBuffer buffer
            0,                                                      // VkDeviceSize offset
            0                                                       // VkConditionalRenderingFlagsEXT flags
        };
        vk.cmdBeginConditionalRenderingEXT(commandBuffer, &conditionalRenderingBeginInfo);
        vk.cmdCopyMemoryToImageIndirectKHR(commandBuffer, &memToImageIndirectInfoKHR);
        vk.cmdEndConditionalRenderingEXT(commandBuffer);
    }
    else
    {
        vk.cmdCopyMemoryToImageIndirectKHR(commandBuffer, &memToImageIndirectInfoKHR);
    }

    endCommandBuffer(vk, commandBuffer);

    if (m_params.extensionFlags & INDIRECT_COPY)
    {
        submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, nullptr, true);
    }
    else if (m_params.useSparseBinding)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer, false, 1u, 1u, &*m_sparseSemaphore, stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, queue, commandBuffer);
    }

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, m_params.dst.image, 0, true, nullptr);
    return checkTestResult(resultLevel->getAccess());
}

class CopyMemoryToImageIndirectTestCase : public vkt::TestCase
{
public:
    CopyMemoryToImageIndirectTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~CopyMemoryToImageIndirectTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyMemoryToImageIndirect(context, m_params);
    }

    virtual void checkFormatSupport(Context &context, VkFormat format) const
    {
        const vk::VkFormatProperties properties =
            vk::getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);

        const vk::VkFormatFeatureFlags features = properties.optimalTilingFeatures;

        if ((features & vk::VK_FORMAT_FEATURE_TRANSFER_DST_BIT) == 0)
            TCU_THROW(NotSupportedError, "Format doesn't support transfer operations");

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
#endif
    }

    virtual void checkSupport(Context &context) const
    {
        if (!context.isDeviceFunctionalitySupported("VK_KHR_copy_memory_indirect"))
            TCU_THROW(NotSupportedError,
                      "Copy memory indirect tests are not supported, no copy memory indirect extension present.");

        const auto &copyMemoryIndirectFeatures = context.getCopyMemoryIndirectFeatures();
        if (!copyMemoryIndirectFeatures.indirectMemoryToImageCopy)
            TCU_THROW(NotSupportedError, "Indirect memory copy to image feature not supported");

        checkExtensionSupport(context, m_params.extensionFlags);
        // Check queue transfer granularity requirements
        if (m_params.queueSelection == QueueSelectionOptions::TransferOnly)
        {
            checkTransferQueueGranularity(context, m_params.dst.image.extent, m_params.dst.image.imageType);
            for (const auto &region : m_params.regions)
            {
                checkTransferQueueGranularity(context, region.bufferImageCopy.imageExtent,
                                              m_params.dst.image.imageType);
            }
        }
        checkFormatSupport(context, m_params.dst.image.format);
    }

private:
    TestParams m_params;
};

void add1dMemoryToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.buffer.size           = defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.buffer.size           = bufferImageHeight;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.buffer.size           = defaultSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultSize * pixelSize * arrayLayerNdx;
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
                default1dExtent                // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_not_all_remaining_layers", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.buffer.size           = defaultSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = default1dExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultSize * pixelSize * arrayLayerNdx;
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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_larger_buffer", params));
    }
}

void add1dFormatTestCase(tcu::TestCaseGroup *group, tcu::TestContext &testCtx, VkFormat format, const char *testName,
                         TestGroupParamsPtr testGroupParams)
{
    TestParams params;
    params.src.buffer.size           = defaultSize;
    params.dst.image.imageType       = VK_IMAGE_TYPE_1D;
    params.dst.image.format          = format;
    params.dst.image.extent          = default1dExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = testGroupParams->allocationKind;
    params.extensionFlags            = testGroupParams->extensionFlags;
    params.queueSelection            = testGroupParams->queueSelection;
    params.useSparseBinding          = testGroupParams->useSparseBinding;

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

    group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, testName, params));
}

void add1dAdditionalFormatsTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    add1dFormatTestCase(group, testCtx, VK_FORMAT_R8G8_UNORM, "r8g8_unorm", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R8G8_UINT, "r8g8_uint", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_A2R10G10B10_UNORM_PACK32, "a2r10g10b10_unorm", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R16_UINT, "r16_uint", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R16_SFLOAT, "r16_sfloat", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R16G16_UNORM, "r16g16_unorm", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R16G16B16A16_SNORM, "r16g16b16a16_snorm", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R32G32_UINT, "r32g32_uint", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R32G32_SFLOAT, "r32g32_sfloat", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_UINT, "r32g32b32_uint", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_SINT, "r32g32b32_sint", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_SFLOAT, "r32g32b32_sfloat", testGroupParams);
    add1dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32A32_UINT, "r32g32b32a32_uint", testGroupParams);
}

void addMemoryTo2DMipImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        const VkFormat mipFormats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8_UINT,     VK_FORMAT_R8G8_UNORM,
                                       VK_FORMAT_R16G16_UNORM,   VK_FORMAT_R32G32_UINT, VK_FORMAT_UNDEFINED};

        VkExtent3D extents[] = {
            {64, 64, 1},
            {64, 192, 1},
        };

        uint32_t arrayLayers[] = {1, 2, 5};

        auto getCaseName = [](VkFormat format, VkExtent3D extent, uint32_t numLayers)
        {
            std::string caseName = "mip_copies_" + getFormatCaseName(format) + "_" + std::to_string(extent.width) +
                                   "x" + std::to_string(extent.height);
            if (numLayers > 1)
                caseName.append("_" + std::to_string(numLayers) + "_layers");
            return caseName;
        };

        for (const auto &extent : extents)
        {
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
                params.arrayLayers               = numLayers;

                for (const VkFormat *format = mipFormats; *format != VK_FORMAT_UNDEFINED; format++)
                {
                    params.src.image.format = *format;
                    {
#ifndef CTS_USES_VULKANSC
                        params.queueSelection = QueueSelectionOptions::Universal;
                        params.extensionFlags = INDIRECT_COPY;
                        group->addChild(new CopyMipmappedImageToBufferTestCase(
                            testCtx, getCaseName(*format, params.src.image.extent, numLayers), params));
#endif
                    }
                }
            }
        }
    }
}

void add2dMemoryToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UINT;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "whole", params));

        params.useConditionalRender = true;
        params.conditionalPredicate = 0;
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "conditional_off", params));

        params.useConditionalRender = true;
        params.conditionalPredicate = 1;
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "conditional_on", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        CopyRegion region;
        uint32_t divisor = 1;
        for (int offset = 0; (offset + defaultQuarterSize / divisor < defaultSize) && (defaultQuarterSize > divisor);
             offset += defaultQuarterSize / divisor++)
        {
            const VkBufferImageCopy bufferImageCopy = {
                0u,                           // VkDeviceSize bufferOffset;
                0u,                           // uint32_t bufferRowLength;
                0u,                           // uint32_t bufferImageHeight;
                defaultSourceLayer,           // VkImageSubresourceLayers imageSubresource;
                {offset, defaultHalfSize, 0}, // VkOffset3D imageOffset;
                {defaultQuarterSize / divisor, defaultQuarterSize / divisor, 1} // VkExtent3D imageExtent;
            };
            region.bufferImageCopy = bufferImageCopy;
            params.regions.push_back(region);
        }

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "regions", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize,                          // VkDeviceSize bufferOffset;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "buffer_offset", params));
    }

    if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
    {
        TestParams params;
        params.src.buffer.size           = defaultSize * defaultSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize + 1u,                     // VkDeviceSize bufferOffset;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
            defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "buffer_offset_relaxed", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "tightly_sized_buffer", params));
    }

    {
        TestParams params;
        uint32_t bufferImageHeight       = defaultSize + 1u;
        params.src.buffer.size           = defaultSize * bufferImageHeight;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            0u,                 // VkDeviceSize bufferOffset;
            defaultSize,        // uint32_t bufferRowLength;
            bufferImageHeight,  // uint32_t bufferImageHeight;
            defaultSourceLayer, // VkImageSubresourceLayers imageSubresource;
            {0, 0, 0},          // VkOffset3D imageOffset;
            defaultHalfExtent   // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "larger_buffer", params));
    }

    {
        TestParams params;
        params.src.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const VkBufferImageCopy bufferImageCopy = {
            defaultQuarterSize,                          // VkDeviceSize bufferOffset;
            defaultSize,                                 // uint32_t bufferRowLength;
            defaultSize,                                 // uint32_t bufferImageHeight;
            defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
            {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
            defaultHalfExtent                            // VkExtent3D imageExtent;
        };
        CopyRegion copyRegion;
        copyRegion.bufferImageCopy = bufferImageCopy;

        params.regions.push_back(copyRegion);

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "tightly_sized_buffer_offset", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
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
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        uint32_t bufferImageHeight       = defaultHalfSize + 1u;
        params.src.buffer.size           = defaultHalfSize * bufferImageHeight * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * bufferImageHeight * pixelSize * arrayLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset,            // VkDeviceSize bufferOffset;
                defaultHalfSize,   // uint32_t bufferRowLength;
                bufferImageHeight, // uint32_t bufferImageHeight;
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
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_larger_buffer", params));
    }

    {
        TestParams params;
        uint32_t arrayLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = arrayLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * arrayLayerNdx;
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
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_tightly_sized_buffer", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_all_remaining_layers", params));
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
        params.src.buffer.fillMode       = FILL_MODE_RED;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.dst.image.fillMode        = FILL_MODE_RED;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;
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

        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "array_not_all_remaining_layers", params));
    }
}

// Helper function to create a test case for a specific 2D image format
void add2dFormatTestCase(tcu::TestCaseGroup *group, tcu::TestContext &testCtx, VkFormat format, const char *testName,
                         TestGroupParamsPtr testGroupParams, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL)
{
    TestParams params;
    params.src.buffer.size           = defaultSize * defaultSize;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = format;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = tiling;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = testGroupParams->allocationKind;
    params.extensionFlags            = testGroupParams->extensionFlags;
    params.queueSelection            = testGroupParams->queueSelection;
    params.useSparseBinding          = testGroupParams->useSparseBinding;

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

    group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, testName, params));
}

void add2dAdditionalFormatsTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    add2dFormatTestCase(group, testCtx, VK_FORMAT_R8G8_UNORM, "r8g8_unorm", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R8G8_UINT, "r8g8_uint", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_A2R10G10B10_UNORM_PACK32, "a2r10g10b10_unorm", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R16_UINT, "r16_uint", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R16_SFLOAT, "r16_sfloat", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R16G16_UNORM, "r16g16_unorm", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R16G16B16A16_SNORM, "r16g16b16a16_snorm", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32_UINT, "r32g32_uint", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32_SFLOAT, "r32g32_sfloat", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_UINT, "r32g32b32_uint", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_SINT, "r32g32b32_sint", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_SFLOAT, "r32g32b32_sfloat", testGroupParams);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_UINT, "r32g32b32_uint_linear", testGroupParams,
                        VK_IMAGE_TILING_LINEAR);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_SINT, "r32g32b32_sint_linear", testGroupParams,
                        VK_IMAGE_TILING_LINEAR);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32_SFLOAT, "r32g32b32_sfloat_linear", testGroupParams,
                        VK_IMAGE_TILING_LINEAR);
    add2dFormatTestCase(group, testCtx, VK_FORMAT_R32G32B32A32_UINT, "r32g32b32a32_uint", testGroupParams);
}

void add3dMemoryToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    {
        TestParams params;
        uint32_t depthLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * depthLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = depthLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t depthLayerNdx = 0; depthLayerNdx < depthLayers; depthLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * depthLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset, // VkDeviceSize bufferOffset;
                0u,     // uint32_t bufferRowLength;
                0u,     // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    depthLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "r8g8b8a8_copy_per_slice", params));
    }

    {
        TestParams params;
        uint32_t depthLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * depthLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = depthLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize          = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        const int32_t quadrantWidth  = static_cast<int32_t>(defaultHalfExtent.width / 2);
        const int32_t quadrantHeight = static_cast<int32_t>(defaultHalfExtent.height / 2);

        // For each depth layer
        for (uint32_t depthLayerNdx = 0; depthLayerNdx < depthLayers; depthLayerNdx++)
        {
            // For each quadrant in the slice
            for (int32_t quadY = 0; quadY < 2; quadY++)
            {
                for (int32_t quadX = 0; quadX < 2; quadX++)
                {
                    // Calculate buffer offset for this quadrant, each quadrant is 1/4 of a slice in size
                    const uint32_t baseSliceOffset = defaultHalfSize * defaultHalfSize * pixelSize * depthLayerNdx;
                    const uint32_t quadrantOffset =
                        static_cast<uint32_t>(quadY) *
                            (static_cast<uint32_t>(quadrantHeight) * defaultHalfSize * pixelSize) +
                        static_cast<uint32_t>(quadX) * (static_cast<uint32_t>(quadrantWidth) * pixelSize);
                    const VkDeviceSize offset = baseSliceOffset + quadrantOffset;

                    const VkBufferImageCopy bufferImageCopy = {
                        offset,          // VkDeviceSize bufferOffset;
                        defaultHalfSize, // uint32_t bufferRowLength; - full width stride in buffer
                        0u,              // uint32_t bufferImageHeight;
                        {
                            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                            0u,                        // uint32_t mipLevel;
                            depthLayerNdx,             // uint32_t baseArrayLayer;
                            1u,                        // uint32_t layerCount;
                        },                             // VkImageSubresourceLayers imageSubresource;
                        {                              // VkOffset3D imageOffset;
                         quadX * quadrantWidth, quadY * quadrantHeight, static_cast<int32_t>(depthLayerNdx)},
                        {// VkExtent3D imageExtent;
                         static_cast<uint32_t>(quadrantWidth), static_cast<uint32_t>(quadrantHeight), 1u}};
                    CopyRegion copyRegion;
                    copyRegion.bufferImageCopy = bufferImageCopy;

                    params.regions.push_back(copyRegion);
                }
            }
        }
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "r8g8b8a8_quadrant_copies", params));
    }

    {
        TestParams params;
        uint32_t depthLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * depthLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params.dst.image.format          = VK_FORMAT_R32G32_SFLOAT;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = depthLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        const int pixelSize = tcu::getPixelSize(mapVkFormat(params.dst.image.format));
        for (uint32_t depthLayerNdx = 0; depthLayerNdx < depthLayers; depthLayerNdx++)
        {
            const VkDeviceSize offset               = defaultHalfSize * defaultHalfSize * pixelSize * depthLayerNdx;
            const VkBufferImageCopy bufferImageCopy = {
                offset, // VkDeviceSize bufferOffset;
                0u,     // uint32_t bufferRowLength;
                0u,     // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    depthLayerNdx,             // uint32_t baseArrayLayer;
                    1u,                        // uint32_t layerCount;
                },                             // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},                     // VkOffset3D imageOffset;
                defaultHalfExtent              // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "r32g32_sfloat_copy_per_slice", params));
    }

    {
        TestParams params;
        uint32_t depthLayers             = 16u;
        params.src.buffer.size           = defaultHalfSize * defaultHalfSize * depthLayers;
        params.dst.image.imageType       = VK_IMAGE_TYPE_3D;
        params.dst.image.extent          = defaultHalfExtent;
        params.dst.image.extent.depth    = depthLayers;
        params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = testGroupParams->allocationKind;
        params.extensionFlags            = testGroupParams->extensionFlags;
        params.queueSelection            = testGroupParams->queueSelection;
        params.useSparseBinding          = testGroupParams->useSparseBinding;

        {
            const VkDeviceSize offset               = 0;
            const VkBufferImageCopy bufferImageCopy = {
                offset, // VkDeviceSize bufferOffset;
                0u,     // uint32_t bufferRowLength
                0u,     // uint32_t bufferImageHeight;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t mipLevel;
                    0,                         // uint32_t baseArrayLayer;
                    depthLayers, // uint32_t layerCount;    // For 3D images, cmdCopyMemoryToImageIndirectKHR uses baseArrayLayer/layerCount instead of image.extent.depth
                },               // VkImageSubresourceLayers imageSubresource;
                {0, 0, 0},       // VkOffset3D imageOffset;
                defaultHalfExtent // VkExtent3D imageExtent; // For 3D images, cmdCopyMemoryToImageIndirectKHR uses baseArrayLayer/layerCount instead of image.extent.depth
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);
        }
        params.dst.image.format = VK_FORMAT_R8G8B8A8_UNORM;
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "r8g8b8a8_all_slices_at_once", params));

        params.dst.image.format = VK_FORMAT_R8G8_SINT;
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "r8g8_sint_all_slices_at_once", params));

        params.dst.image.format = VK_FORMAT_R32G32_SFLOAT;
        group->addChild(new CopyMemoryToImageIndirectTestCase(testCtx, "r32g32_sfloat_all_slices_at_once", params));
    }
}

typedef struct
{
    uint32_t copyCount;
    uint32_t stride;
    uint32_t copyOffset;
    uint32_t copySize;
    QueueSelectionOptions queue;
    bool useProtectedMemory;
} CopyParams;

struct IndirectParams
{
    VkCopyMemoryIndirectCommandKHR cmd;
    uint32_t dummyparam1;
    uint32_t dummyparam2;
    uint32_t dummyparam3;
};

// CopyMemoryIndirect
class CopyMemoryIndirectTestInstance : public TestInstance
{
public:
    CopyMemoryIndirectTestInstance(Context &context, const CopyParams copyParams)
        : TestInstance(context)
        , m_copyParams(copyParams)
    {
        init();
    }

    ~CopyMemoryIndirectTestInstance(void)
    {
    }

private:
    void loadDataFromFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::binary);

        // Check if the file was opened successfully
        if (!file)
        {
            TCU_THROW(TestError, "Error opening file!");
        }

        // Seek to the end of the file to get the file size
        file.seekg(0, std::ios::end);
        std::streamsize fileSize = static_cast<std::streamsize>(file.tellg());
        file.seekg(0, std::ios::beg);

        m_copyData.resize(fileSize);

        // Read the file contents into the vector
        if (!file.read(m_copyData.data(), fileSize))
        {
            TCU_THROW(TestError, "Error reading from file!");
        }

        file.close();
    }

    void init(void)
    {
        const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
        const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

        if (!m_context.isDeviceFunctionalitySupported("VK_KHR_copy_memory_indirect"))
            TCU_THROW(NotSupportedError,
                      "Copy memory indirect tests are not supported, no copy memory indirect extension present.");

        const auto &copyMemoryIndirectFeatures = m_context.getCopyMemoryIndirectFeatures();
        if (!copyMemoryIndirectFeatures.indirectMemoryCopy)
            TCU_THROW(NotSupportedError, "Indirect memory copy feature not supported");

        if (m_copyParams.useProtectedMemory)
        {
            VkPhysicalDeviceProtectedMemoryFeatures protectedMemoryFeature = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES, // VkStructureType sType;
                nullptr,                                                     // void* pNext;
                VK_FALSE                                                     // VkBool32 protectedMemory;
            };

            VkPhysicalDeviceFeatures2 features2;
            deMemset(&features2, 0, sizeof(features2));
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &protectedMemoryFeature;

            vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
            if (protectedMemoryFeature.protectedMemory == VK_FALSE)
                TCU_THROW(NotSupportedError, "Protected memory feature is not supported");
        }

        const std::string fileName = "./vulkan/data/copy_memory_indirect/sample_text.txt";
        loadDataFromFile(fileName);

        // 64-aligned
        while (m_copyData.size() & 0x63)
        {
            m_copyData.push_back('.');
        }
    }

    struct Queue
    {
        const VkQueue queue;
        const int queueFamilyIndex;
    };

    Queue DetermineQueue(QueueSelectionOptions queue) const
    {
        switch (queue)
        {
        case QueueSelectionOptions::ComputeOnly:
        {
            return Queue{m_context.getComputeQueue(), m_context.getComputeQueueFamilyIndex()};
        }
        case QueueSelectionOptions::TransferOnly:
        {
            return Queue{m_context.getTransferQueue(), m_context.getTransferQueueFamilyIndex()};
        }
        default:
        case QueueSelectionOptions::Universal:
        {
            return Queue{m_context.getUniversalQueue(), (int)m_context.getUniversalQueueFamilyIndex()};
        }
        }
        return Queue{m_context.getUniversalQueue(), (int)m_context.getUniversalQueueFamilyIndex()};
    }

    tcu::TestStatus iterate(void) override
    {
        // Necessary Parameters
        const VkDevice device                     = m_context.getDevice();
        const DeviceInterface &vkd                = m_context.getDeviceInterface();
        Queue queueInfo                           = DetermineQueue(m_copyParams.queue);
        const VkQueue queue                       = queueInfo.queue;
        const int queueFamilyIndex                = queueInfo.queueFamilyIndex;
        Allocator &allocator                      = m_context.getDefaultAllocator();
        const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
        const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

        VkPhysicalDeviceCopyMemoryIndirectPropertiesKHR copyMemoryIndirectProperties = {};
        copyMemoryIndirectProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_KHR;

        VkPhysicalDeviceProperties2 deviceProperties = {};
        deviceProperties.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        deviceProperties.pNext                       = &copyMemoryIndirectProperties;

        vki.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties);

        // Check whether current queue type is supported
        switch (m_copyParams.queue)
        {
        case QueueSelectionOptions::Universal:
        {
            if (queueFamilyIndex == -1)
            {
                TCU_THROW(NotSupportedError, "Device does not have dedicated universal queue.");
            }
            if (!(copyMemoryIndirectProperties.supportedQueues & VK_QUEUE_GRAPHICS_BIT))
            {
                TCU_THROW(NotSupportedError, "Graphics queue not supported!");
            }
            break;
        }
        case QueueSelectionOptions::TransferOnly:
        {
            if (queueFamilyIndex == -1)
            {
                TCU_THROW(NotSupportedError, "Device does not have dedicated transfer queue.");
            }
            if (!(copyMemoryIndirectProperties.supportedQueues & VK_QUEUE_TRANSFER_BIT))
            {
                TCU_THROW(NotSupportedError, "Transfer queue not supported!");
            }
            break;
        }
        case QueueSelectionOptions::ComputeOnly:
        {
            if (queueFamilyIndex == -1)
            {
                TCU_THROW(NotSupportedError, "Device does not have dedicated compute queue.");
            }
            if (!(copyMemoryIndirectProperties.supportedQueues & VK_QUEUE_COMPUTE_BIT))
            {
                TCU_THROW(NotSupportedError, "Compute queue not supported!");
            }
            break;
        }
        }

        const size_t bufferSize = m_copyData.size();
        const size_t copySize   = m_copyParams.copySize ? m_copyParams.copySize - m_copyParams.copyOffset :
                                                          m_copyData.size() - m_copyParams.copyOffset;

        VkBufferCreateFlags bufferCreateFlags = 0;
        if (m_copyParams.useProtectedMemory)
        {
            bufferCreateFlags |= VK_BUFFER_CREATE_PROTECTED_BIT;
        }
        vk::MemoryRequirement memReqs = m_copyParams.useProtectedMemory ?
                                            vk::MemoryRequirement::HostVisible | vk::MemoryRequirement::DeviceAddress |
                                                vk::MemoryRequirement::Protected :
                                            vk::MemoryRequirement::HostVisible | vk::MemoryRequirement::DeviceAddress;

        // Buffers
        const BufferWithMemory srcBuffer(
            vkd, device, allocator,
            makeBufferCreateInfo(bufferSize,
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, // Buffer usage flag
                                 std::vector<uint32_t>(),
                                 bufferCreateFlags), // Buffer create flag
            memReqs);

        const BufferWithMemory dstBuffer(
            vkd, device, allocator,
            makeBufferCreateInfo(std::max(m_copyParams.copyCount, (uint32_t)1) * bufferSize,
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, std::vector<uint32_t>(),
                                 bufferCreateFlags), // Buffer create flag
            memReqs);

        const VkDeviceSize indirectBufferSize = std::max(m_copyParams.copyCount, 1U) * m_copyParams.stride;
        const BufferWithMemory indirectBuffer(
            vkd, device, allocator,
            makeBufferCreateInfo(indirectBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, std::vector<uint32_t>(),
                                 bufferCreateFlags), // Buffer create flag
            memReqs);

        // Buffer Information
        VkBufferDeviceAddressInfo srcBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                       srcBuffer.get()};
        VkDeviceAddress srcBufferAddress = vkd.getBufferDeviceAddress(device, &srcBufferAddressInfo);
        VkBufferDeviceAddressInfo dstBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                       dstBuffer.get()};
        VkDeviceAddress dstBufferAddress = vkd.getBufferDeviceAddress(device, &dstBufferAddressInfo);
        VkBufferDeviceAddressInfo indirectBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                            indirectBuffer.get()};
        VkDeviceAddress indirectBufferAddress = vkd.getBufferDeviceAddress(device, &indirectBufferAddressInfo);

        // Create CMI Parameters
        std::vector<VkCopyMemoryIndirectCommandKHR> cmiRegions(m_copyParams.copyCount);
        for (uint32_t i = 0; i < m_copyParams.copyCount; ++i)
        {
            cmiRegions[i].srcAddress = srcBufferAddress + m_copyParams.copyOffset;
            cmiRegions[i].dstAddress = dstBufferAddress + m_copyParams.copyOffset + (i * copySize);
            cmiRegions[i].size       = copySize;
        }

        // Copy Data -> srcBuffer
        {
            const Allocation &bufferAllocation = srcBuffer.getAllocation();
            invalidateAlloc(vkd, device, bufferAllocation);
            deMemcpy(bufferAllocation.getHostPtr(), m_copyData.data(), bufferSize);
        }

        // Copy Commands -> indirectBuffer
        {
            const Allocation &bufferAllocation = indirectBuffer.getAllocation();
            invalidateAlloc(vkd, device, bufferAllocation);
            uint8_t *hostPtr = (uint8_t *)bufferAllocation.getHostPtr();
            if (m_copyParams.stride == sizeof(VkCopyMemoryIndirectCommandKHR))
            {
                deMemcpy(hostPtr, cmiRegions.data(), m_copyParams.stride * m_copyParams.copyCount);
            }
            else
            {
                // Create other indirect parameters
                std::vector<IndirectParams> cmiLongRegions(m_copyParams.copyCount);
                for (uint32_t i = 0; i < m_copyParams.copyCount; ++i)
                {
                    cmiLongRegions[i].cmd = cmiRegions[i];
                }
                deMemcpy(hostPtr, cmiLongRegions.data(), m_copyParams.stride * m_copyParams.copyCount);
            }
        }

        // dstBuffer
        {
            const Allocation &bufferAllocation = dstBuffer.getAllocation();
            invalidateAlloc(vkd, device, bufferAllocation);
            deMemset(bufferAllocation.getHostPtr(), 0xFF, std::max(m_copyParams.copyCount, (uint32_t)1) * bufferSize);
        }

        const Unique<VkCommandPool> cmdPool(
            makeCommandPool(vkd, device, queueFamilyIndex,
                            m_copyParams.useProtectedMemory ?
                                static_cast<VkCommandPoolCreateFlags>(VK_COMMAND_POOL_CREATE_PROTECTED_BIT) :
                                static_cast<VkCommandPoolCreateFlags>(0u)));

        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        beginCommandBuffer(vkd, *cmdBuffer);

        VkStridedDeviceAddressRangeKHR addressRange = {indirectBufferAddress, indirectBufferSize, m_copyParams.stride};
        VkCopyMemoryIndirectInfoKHR copyMemoryIndirectKHR = {};
        copyMemoryIndirectKHR.sType                       = VK_STRUCTURE_TYPE_COPY_MEMORY_INDIRECT_INFO_KHR;
        copyMemoryIndirectKHR.pNext                       = nullptr;
        copyMemoryIndirectKHR.copyAddressRange            = addressRange;
        copyMemoryIndirectKHR.srcCopyFlags =
            m_copyParams.useProtectedMemory ? VK_ADDRESS_COPY_PROTECTED_BIT_KHR : VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR;
        copyMemoryIndirectKHR.dstCopyFlags =
            m_copyParams.useProtectedMemory ? VK_ADDRESS_COPY_PROTECTED_BIT_KHR : VK_ADDRESS_COPY_DEVICE_LOCAL_BIT_KHR;
        copyMemoryIndirectKHR.copyCount = m_copyParams.copyCount;
        vkd.cmdCopyMemoryIndirectKHR(*cmdBuffer, &copyMemoryIndirectKHR);

        VkBufferMemoryBarrier bufferBarrier = {};
        bufferBarrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.dstAccessMask         = VK_ACCESS_TRANSFER_READ_BIT;
        bufferBarrier.buffer                = dstBuffer.get();
        bufferBarrier.offset                = 0;
        bufferBarrier.size                  = VK_WHOLE_SIZE;

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 1, &bufferBarrier, 0, nullptr);

        endCommandBuffer(vkd, *cmdBuffer);
        if (m_copyParams.useProtectedMemory)
        {
            const VkProtectedSubmitInfo protectedSubmitInfo = {
                VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO, // sType
                nullptr,                                 // pNext
                VK_TRUE                                  // protectedSubmit
            };

            const VkSubmitInfo submitInfo = {
                VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
                &protectedSubmitInfo,          // pNext
                0u,                            // waitSemaphoreCount
                nullptr,                       // pWaitSemaphores
                nullptr,                       // pWaitDstStageMask
                1u,                            // commandBufferCount
                &(*cmdBuffer),                 // pCommandBuffers
                0u,                            // signalSemaphoreCount
                nullptr                        // pSignalSemaphores
            };

            const Unique<VkFence> fence(createFence(vkd, device));
            VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
            VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), VK_TRUE, ~0ull));
        }
        else
        {
            submitCommandsAndWaitWithTransferSync(vkd, device, queue, *cmdBuffer, nullptr, true);
        }

        // Validate
        bool testPassed = true;
        {
            std::vector<char> copiedData(copySize, 0);
            const Allocation &bufferAllocation = dstBuffer.getAllocation();
            invalidateAlloc(vkd, device, bufferAllocation);
            uint8_t *hostPtr    = (uint8_t *)bufferAllocation.getHostPtr();
            uint32_t copyOffset = m_copyParams.copyOffset;
            for (uint32_t copyNum = 0; copyNum < m_copyParams.copyCount; ++copyNum)
            {
                deMemcpy(copiedData.data(), hostPtr + copyOffset + (copyNum * copySize), copySize);
                testPassed = testPassed && !(deMemCmp(hostPtr + copyOffset + (copyNum * copySize),
                                                      m_copyData.data() + copyOffset, copySize));
            }
            if (m_copyParams.copyCount == 0)
                testPassed = copiedData[0] != m_copyData[0];
        }

        return (testPassed ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail"));
    }

    std::vector<char> m_copyData{};

    CopyParams m_copyParams;
};

class CopyMemoryIndirectTestCase : public TestCase
{
public:
    CopyMemoryIndirectTestCase(tcu::TestContext &context, const char *testName, CopyParams copyParams)
        : TestCase(context, testName)
        , m_copyParams(copyParams)
    {
    }

private:
    TestInstance *createInstance(Context &context) const override
    {
        return new CopyMemoryIndirectTestInstance(context, m_copyParams);
    }

    CopyParams m_copyParams;
};

#endif

} // namespace

#ifndef CTS_USES_VULKANSC

void addCopyImageToBufferIndirectTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "1d_images", add1dImageToBufferTests, testGroupParams);
    // 2D images are exercised with addMemoryToImageTests
    addTestGroup(group, "3d_images", add3dImageToBufferTests, testGroupParams);
}

void addCopyMemoryToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "1d_images", add1dMemoryToImageTests, testGroupParams);
    addTestGroup(group, "1d_additional_formats", add1dAdditionalFormatsTests, testGroupParams);
    addTestGroup(group, "2d_images", add2dMemoryToImageTests, testGroupParams);
    addTestGroup(group, "2d_mipmap_images", addMemoryTo2DMipImageTests, testGroupParams);
    addTestGroup(group, "2d_additional_formats", add2dAdditionalFormatsTests, testGroupParams);
    addTestGroup(group, "3d_images", add3dMemoryToImageTests, testGroupParams);
}

tcu::TestCaseGroup *createCopyMemoryIndirectTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "copy_memory_indirect"));

    struct CopyCount
    {
        int numCopies;
        const char *name;
    };
    const CopyCount copyCounts[] = {{0, "count_0"}, {1, "count_1"}, {2, "count_2"}, {63, "count_63"}};

    struct CopySize
    {
        uint32_t size;
        const char *name;
    };
    const CopySize copySizes[] = {{4, "size_4"}, {12, "size_12"}, {0, "size_full"}};

    struct CopyOffset
    {
        uint32_t offset;
        const char *name;
    };
    const CopyOffset copyOffsets[] = {{0, "offset_0"}, {4, "offset_4"}};

    struct Stride
    {
        int stride;
        const char *name;
    };
    const Stride strides[] = {{sizeof(VkCopyMemoryIndirectCommandKHR), "normal_stride"},
                              {sizeof(IndirectParams), "long_stride"}};

    struct Queue
    {
        QueueSelectionOptions queue;
        std::string queueName;
    };
    const Queue queues[] = {{QueueSelectionOptions::Universal, "graphics"},
                            {QueueSelectionOptions::TransferOnly, "transfer"},
                            {QueueSelectionOptions::ComputeOnly, "compute"}};

    for (int copySizeIdx = 0; copySizeIdx < DE_LENGTH_OF_ARRAY(copySizes); ++copySizeIdx)
    {
        de::MovePtr<tcu::TestCaseGroup> copySizeGroup(new tcu::TestCaseGroup(testCtx, copySizes[copySizeIdx].name));
        for (int copyOffsetIdx = 0; copyOffsetIdx < DE_LENGTH_OF_ARRAY(copyOffsets); ++copyOffsetIdx)
        {
            de::MovePtr<tcu::TestCaseGroup> copyOffsetGroup(
                new tcu::TestCaseGroup(testCtx, copyOffsets[copyOffsetIdx].name));
            for (int copyCountIdx = 0; copyCountIdx < DE_LENGTH_OF_ARRAY(copyCounts); ++copyCountIdx)
            {
                de::MovePtr<tcu::TestCaseGroup> copyCountGroup(
                    new tcu::TestCaseGroup(testCtx, copyCounts[copyCountIdx].name));
                for (int strideIdx = 0; strideIdx < DE_LENGTH_OF_ARRAY(strides); ++strideIdx)
                {
                    de::MovePtr<tcu::TestCaseGroup> strideGroup(
                        new tcu::TestCaseGroup(testCtx, strides[strideIdx].name));
                    for (int queueIdx = 0; queueIdx < DE_LENGTH_OF_ARRAY(queues); ++queueIdx)
                    {
                        if (copyOffsets[copyOffsetIdx].offset >= copySizes[copySizeIdx].size)
                            continue;

                        CopyParams params{
                            static_cast<uint32_t>(copyCounts[copyCountIdx].numCopies),
                            static_cast<uint32_t>(strides[strideIdx].stride),
                            static_cast<uint32_t>(copyOffsets[copyOffsetIdx].offset),
                            static_cast<uint32_t>(copySizes[copySizeIdx].size),
                            queues[queueIdx].queue,
                            false //useProtectedMemory
                        };
                        strideGroup->addChild(
                            new CopyMemoryIndirectTestCase(testCtx, queues[queueIdx].queueName.c_str(), params));
                    };
                    copyCountGroup->addChild(strideGroup.release());
                }
                copyOffsetGroup->addChild(copyCountGroup.release());
            }
            copySizeGroup->addChild(copyOffsetGroup.release());
        }
        group->addChild(copySizeGroup.release());
    }

    // Add a test for protected memory
    de::MovePtr<tcu::TestCaseGroup> protectedGroup(new tcu::TestCaseGroup(testCtx, "protected_memory"));

    // Create a specific test case with count=1, size_full, offset_0, and normal_stride
    CopyParams protectedParams{
        1,                                      // copyCount
        sizeof(VkCopyMemoryIndirectCommandKHR), // stride (normal_stride)
        0,                                      // copyOffset
        0,                                      // copySize (full size - will use the whole buffer)
        QueueSelectionOptions::Universal,       // queue
        true                                    // useProtectedMemory
    };

    protectedGroup->addChild(new CopyMemoryIndirectTestCase(testCtx, "graphics", protectedParams));
    group->addChild(protectedGroup.release());

    return group.release();
}

#endif

} // namespace api
} // namespace vkt
