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
 * \brief Vulkan Copy Buffer To Image Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyBufferToImageTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

class CopyBufferToImage : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    CopyBufferToImage(Context &context, TestParams testParams);
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

CopyBufferToImage::CopyBufferToImage(Context &context, TestParams testParams)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, testParams)
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
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
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
            m_params.dst.image.tiling,                                         // VkImageTiling tiling;
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

tcu::TestStatus CopyBufferToImage::iterate(void)
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

    generateExpectedResult();

    uploadBuffer(m_sourceTextureLevel->getAccess(), *m_sourceBufferAlloc);
    uploadImage(m_destinationTextureLevel->getAccess(), *m_destination, m_params.dst.image, m_params.useGeneralLayout);

    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice vkDevice                     = m_device;
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    const VkMemoryBarrier memoryBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
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
                          (VkDependencyFlags)0, (m_params.useGeneralLayout ? 1 : 0), &memoryBarrier, 0, nullptr,
                          (m_params.useGeneralLayout ? 0 : 1), &imageBarrier);

    const VkImageLayout layout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdCopyBufferToImage(commandBuffer, m_source.get(), m_destination.get(), layout,
                                (uint32_t)m_params.regions.size(), bufferImageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyBufferToImageInfo2KHR copyBufferToImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                           // const void* pNext;
            m_source.get(),                                    // VkBuffer srcBuffer;
            m_destination.get(),                               // VkImage dstImage;
            layout,                                            // VkImageLayout dstImageLayout;
            (uint32_t)m_params.regions.size(),                 // uint32_t regionCount;
            bufferImageCopies2KHR.data()                       // const VkBufferImageCopy2KHR* pRegions;
        };

        vk.cmdCopyBufferToImage2(commandBuffer, &copyBufferToImageInfo2KHR);
    }

    endCommandBuffer(vk, commandBuffer);

    submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore);

    m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(*m_destination, m_params.dst.image);
    return checkTestResult(resultLevel->getAccess());
}

class CopyBufferToImageTestCase : public vkt::TestCase
{
public:
    CopyBufferToImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~CopyBufferToImageTestCase(void)
    {
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new CopyBufferToImage(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
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

        VkImageFormatProperties formatProperties;
        const auto ctx        = context.getContextCommonData();
        const auto imageUsage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        const auto res        = ctx.vki.getPhysicalDeviceImageFormatProperties(
            ctx.physicalDevice, m_params.dst.image.format, m_params.dst.image.imageType, m_params.dst.image.tiling,
            imageUsage, getCreateFlags(m_params.dst.image), &formatProperties);

        if (res != VK_SUCCESS)
        {
            if (res == VK_ERROR_FORMAT_NOT_SUPPORTED)
                TCU_THROW(NotSupportedError, "Format does not support the required parameters");
            TCU_FAIL("vkGetPhysicalDeviceImageFormatProperties returned an unexpected error code");
        }

        if (formatProperties.maxArrayLayers < getArraySize(m_params.dst.image))
            TCU_THROW(NotSupportedError, "maxArrayLayers too small");
    }

private:
    TestParams m_params;
};

void CopyBufferToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
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

void add1dBufferToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    // We only run these tests on a restricted list of formats to avoid combinatory explosions.
    struct FormatAndSuffix
    {
        VkFormat format;
        VkImageTiling tiling;
        const char *suffix;
    };

    const std::vector<FormatAndSuffix> restrictedFormatList{
        {VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, ""}, // Default format with no suffix
        {VK_FORMAT_R8G8B8A8_UINT, VK_IMAGE_TILING_OPTIMAL, "_rgba8_uint"},
        // 96-bit formats are considered worth testing on some implementations because they use separate paths.
        // On some implementations, the hardware does not natively support these, so we try linear too.
        {VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, "_rgb32_sfloat"},
        {VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_TILING_LINEAR, "_rgb32_sfloat_linear"},
    };

    const auto imageType = VK_IMAGE_TYPE_1D;

    for (const auto &formatAndSuffix : restrictedFormatList)
    {
        {
            TestParams params;
            params.src.buffer.size           = defaultSize;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = default1dExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

            const auto testName = std::string("tightly_sized_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t bufferImageHeight       = defaultSize + 1u;
            params.src.buffer.size           = bufferImageHeight;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = default1dExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

            const auto testName = std::string("larger_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t arrayLayers             = 16u;
            params.src.buffer.size           = defaultSize * arrayLayers;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = default1dExtent;
            params.dst.image.extent.depth    = arrayLayers;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

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

            const auto testName = std::string("array_tightly_sized_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            const uint32_t baseLayer         = 0u;
            const uint32_t layerCount        = 16u;
            params.src.buffer.size           = defaultSize * layerCount;
            params.src.buffer.fillMode       = FILL_MODE_RED;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = default1dExtent;
            params.dst.image.extent.depth    = layerCount;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.dst.image.fillMode        = FILL_MODE_RED;
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

            const auto testName = std::string("array_all_remaining_layers") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            const uint32_t baseLayer         = 2u;
            const uint32_t layerCount        = 16u;
            params.src.buffer.size           = defaultSize * layerCount;
            params.src.buffer.fillMode       = FILL_MODE_RED;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = default1dExtent;
            params.dst.image.extent.depth    = layerCount;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.dst.image.fillMode        = FILL_MODE_RED;
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

            const auto testName = std::string("array_not_all_remaining_layers") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t arrayLayers             = 16u;
            uint32_t bufferImageHeight       = defaultSize + 1u;
            params.src.buffer.size           = defaultSize * arrayLayers;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = default1dExtent;
            params.dst.image.extent.depth    = arrayLayers;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

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

            const auto testName = std::string("array_larger_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }
    }
}

void add2dBufferToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    tcu::TestContext &testCtx = group->getTestContext();

    // We only run these tests on a restricted list of formats to avoid combinatory explosions.
    struct FormatAndSuffix
    {
        VkFormat format;
        VkImageTiling tiling;
        const char *suffix;
    };

    const std::vector<FormatAndSuffix> restrictedFormatList{
        {VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, ""}, // Default format with no suffix
        {VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL, "_r8_unorm"},
        {VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_LINEAR, "_r8_unorm_linear"},
        {VK_FORMAT_R8G8B8A8_UINT, VK_IMAGE_TILING_OPTIMAL, "_rgba8_uint"},
        // 96-bit formats are considered worth testing on some implementations because they use separate paths.
        // On some implementations, the hardware does not natively support these, so we try linear too.
        {VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, "_rgb32_sfloat"},
        {VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_TILING_LINEAR, "_rgb32_sfloat_linear"},
    };

    const auto imageType = VK_IMAGE_TYPE_2D;

    for (const auto &formatAndSuffix : restrictedFormatList)
    {
        const auto pixelSize = tcu::getPixelSize(mapVkFormat(formatAndSuffix.format));

        {
            TestParams params;
            params.src.buffer.size           = defaultSize * defaultSize;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

            const auto testName = std::string("whole") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            const auto bufferWidth  = defaultSize + 1u;
            const auto bufferHeight = defaultSize + 1u;

            TestParams params;
            params.src.buffer.size           = bufferWidth * bufferHeight;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

            const auto testName = std::string("whole_unaligned") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            params.src.buffer.size           = defaultSize * defaultSize;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            CopyRegion region;
            uint32_t divisor = 1;
            for (int offset = 0;
                 (offset + defaultQuarterSize / divisor < defaultSize) && (defaultQuarterSize > divisor);
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

            const auto testName = std::string("regions") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            params.src.buffer.size           = defaultSize * defaultSize;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            const auto offset = de::roundUp(defaultQuarterSize, pixelSize);

            const VkBufferImageCopy bufferImageCopy = {
                static_cast<VkDeviceSize>(offset),           // VkDeviceSize bufferOffset;
                defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
                defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
                defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                defaultHalfExtent                            // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);

            const auto testName = std::string("buffer_offset") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        if (testGroupParams->queueSelection == QueueSelectionOptions::Universal)
        {
            TestParams params;
            params.src.buffer.size           = defaultSize * defaultSize;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            const auto offset = de::roundUp(defaultQuarterSize + 1, pixelSize);

            const VkBufferImageCopy bufferImageCopy = {
                static_cast<VkDeviceSize>(offset),           // VkDeviceSize bufferOffset;
                defaultHalfSize + defaultQuarterSize,        // uint32_t bufferRowLength;
                defaultHalfSize + defaultQuarterSize,        // uint32_t bufferImageHeight;
                defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                defaultHalfExtent                            // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);

            const auto testName = std::string("buffer_offset_relaxed") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            params.src.buffer.size           = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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

            const auto testName = std::string("tightly_sized_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t bufferImageHeight       = defaultSize + 1u;
            params.src.buffer.size           = defaultSize * bufferImageHeight;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultExtent;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
                defaultHalfExtent   // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);

            const auto testName = std::string("larger_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            params.src.buffer.size     = (defaultHalfSize - 1u) * defaultSize + defaultHalfSize + defaultQuarterSize;
            params.dst.image.imageType = imageType;
            params.dst.image.format    = formatAndSuffix.format;
            params.dst.image.extent    = defaultExtent;
            params.dst.image.tiling    = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            const auto offset = defaultQuarterSize * pixelSize;

            const VkBufferImageCopy bufferImageCopy = {
                static_cast<VkDeviceSize>(offset),           // VkDeviceSize bufferOffset;
                defaultSize,                                 // uint32_t bufferRowLength;
                defaultSize,                                 // uint32_t bufferImageHeight;
                defaultSourceLayer,                          // VkImageSubresourceLayers imageSubresource;
                {defaultQuarterSize, defaultQuarterSize, 0}, // VkOffset3D imageOffset;
                defaultHalfExtent                            // VkExtent3D imageExtent;
            };
            CopyRegion copyRegion;
            copyRegion.bufferImageCopy = bufferImageCopy;

            params.regions.push_back(copyRegion);

            const auto testName = std::string("tightly_sized_buffer_offset") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t arrayLayers             = 16u;
            params.src.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultHalfExtent;
            params.dst.image.extent.depth    = arrayLayers;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

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

            const auto testName = std::string("array") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t arrayLayers             = 16u;
            uint32_t bufferImageHeight       = defaultHalfSize + 1u;
            params.src.buffer.size           = defaultHalfSize * bufferImageHeight * arrayLayers;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultHalfExtent;
            params.dst.image.extent.depth    = arrayLayers;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

            for (uint32_t arrayLayerNdx = 0; arrayLayerNdx < arrayLayers; arrayLayerNdx++)
            {
                const VkDeviceSize offset = defaultHalfSize * bufferImageHeight * pixelSize * arrayLayerNdx;
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

            const auto testName = std::string("array_larger_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            uint32_t arrayLayers             = 16u;
            params.src.buffer.size           = defaultHalfSize * defaultHalfSize * arrayLayers;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultHalfExtent;
            params.dst.image.extent.depth    = arrayLayers;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.allocationKind            = testGroupParams->allocationKind;
            params.extensionFlags            = testGroupParams->extensionFlags;
            params.queueSelection            = testGroupParams->queueSelection;
            params.useSparseBinding          = testGroupParams->useSparseBinding;
            params.useGeneralLayout          = testGroupParams->useGeneralLayout;

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

            const auto testName = std::string("array_tightly_sized_buffer") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            const uint32_t baseLayer         = 0u;
            const uint32_t layerCount        = 16u;
            params.src.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
            params.src.buffer.fillMode       = FILL_MODE_RED;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultHalfExtent;
            params.dst.image.extent.depth    = layerCount;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.dst.image.fillMode        = FILL_MODE_RED;
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

            const auto testName = std::string("array_all_remaining_layers") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }

        {
            TestParams params;
            const uint32_t baseLayer         = 2u;
            const uint32_t layerCount        = 16u;
            params.src.buffer.size           = defaultHalfSize * defaultHalfSize * layerCount;
            params.src.buffer.fillMode       = FILL_MODE_RED;
            params.dst.image.imageType       = imageType;
            params.dst.image.format          = formatAndSuffix.format;
            params.dst.image.extent          = defaultHalfExtent;
            params.dst.image.extent.depth    = layerCount;
            params.dst.image.tiling          = formatAndSuffix.tiling;
            params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            params.dst.image.fillMode        = FILL_MODE_RED;
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

            const auto testName = std::string("array_not_all_remaining_layers") + formatAndSuffix.suffix;
            group->addChild(new CopyBufferToImageTestCase(testCtx, testName, params));
        }
    }
}

} // namespace

void addCopyBufferToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams)
{
    addTestGroup(group, "1d_images", add1dBufferToImageTests, testGroupParams);
    addTestGroup(group, "2d_images", add2dBufferToImageTests, testGroupParams);
}

} // namespace api
} // namespace vkt
