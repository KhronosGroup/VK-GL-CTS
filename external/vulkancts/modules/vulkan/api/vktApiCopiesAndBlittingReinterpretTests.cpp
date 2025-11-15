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
 * \brief Vulkan Copy And Blitting Reinterpret Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingUtil.hpp"

namespace vkt
{

namespace api
{

namespace
{

class ReinterpretTestInstance final : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    ReinterpretTestInstance(Context &context, TestParams params, const VkFormat viewFormat);
    tcu::TestStatus iterate(void) override;

private:
    tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess()) override;
    void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region,
                                  uint32_t mipLevel = 0u) override;
    void fillCompressedImages();
    tcu::TestStatus checkTestResult(const VkImage &testImage, const VkFormat testImageFormat,
                                    const VkImageType testImageType, const VkExtent3D &testImageExtent,
                                    const VkAccessFlags lastAccess, const VkImageLayout lastLayout,
                                    const VkPipelineStageFlags lastStage);

    Move<VkImage> m_source;
    de::MovePtr<Allocation> m_sourceImageAlloc;
    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;
    const VkFormat m_viewFormat;
};

ReinterpretTestInstance::ReinterpretTestInstance(Context &context, TestParams params, const VkFormat viewFormat)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
    , m_viewFormat(viewFormat)
{
    const InstanceInterface &vki        = context.getInstanceInterface();
    const DeviceInterface &vk           = context.getDeviceInterface();
    const VkPhysicalDevice vkPhysDevice = context.getPhysicalDevice();

    // Create source image
    {
        const bool srcCompressed = isCompressedFormat(m_params.src.image.format);

        VkImageCreateInfo sourceImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            getCreateFlags(m_params.src.image),  // VkImageCreateFlags flags;
            m_params.src.image.imageType,        // VkImageType imageType;
            m_params.src.image.format,           // VkFormat format;
            getExtent3D(m_params.src.image),     // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            getArraySize(m_params.src.image),    // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,               // uint32_t samples;
            m_params.src.image.tiling,           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        if (m_params.src.image.format != m_viewFormat)
            sourceImageParams.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if (srcCompressed)
            sourceImageParams.flags |=
                VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

        m_source           = createImage(vk, m_device, &sourceImageParams);
        m_sourceImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_source, MemoryRequirement::Any,
                                           *m_allocator, m_params.allocationKind, 0u);
        VK_CHECK(
            vk.bindImageMemory(m_device, *m_source, m_sourceImageAlloc->getMemory(), m_sourceImageAlloc->getOffset()));
    }

    // Create copy destination image
    {
        VkImageCreateInfo destinationImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            getCreateFlags(m_params.dst.image),  // VkImageCreateFlags flags;
            m_params.dst.image.imageType,        // VkImageType imageType;
            m_params.dst.image.format,           // VkFormat format;
            getExtent3D(m_params.dst.image),     // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            getArraySize(m_params.dst.image),    // uint32_t arraySize;
            VK_SAMPLE_COUNT_1_BIT,               // uint32_t samples;
            m_params.dst.image.tiling,           // VkImageTiling tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT, // VkImageUsageFlags usage;
            m_queueFamilyIndices.size() > 1 ? VK_SHARING_MODE_CONCURRENT :
                                              VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            (uint32_t)m_queueFamilyIndices.size(),                       // uint32_t queueFamilyIndexCount;
            m_queueFamilyIndices.data(),                                 // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                   // VkImageLayout initialLayout;
        };

        if (m_params.dst.image.format != m_viewFormat)
            destinationImageParams.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if (isCompressedFormat(m_params.dst.image.format))
            destinationImageParams.flags |=
                VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;

        m_destination           = createImage(vk, m_device, &destinationImageParams);
        m_destinationImageAlloc = allocateImage(vki, vk, vkPhysDevice, m_device, *m_destination, MemoryRequirement::Any,
                                                *m_allocator, m_params.allocationKind, 0u);
        VK_CHECK(vk.bindImageMemory(m_device, *m_destination, m_destinationImageAlloc->getMemory(),
                                    m_destinationImageAlloc->getOffset()));
    }
}

tcu::TestStatus ReinterpretTestInstance::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                    m_expectedTextureLevel[0]->getAccess(), result, tcu::Vec4(0.01f),
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Copy test");

    return tcu::TestStatus::pass("Pass");
}

void ReinterpretTestInstance::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
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

void ReinterpretTestInstance::fillCompressedImages()
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_device;

    // Descriptors for storage images
    const auto colorSubresourceRange    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    Move<VkImageView> srcImageImageView = makeImageView(
        vkd, device, *m_source, mapImageViewType(m_params.src.image.imageType), m_viewFormat, colorSubresourceRange);
    Move<VkImageView> dstImageImageView =
        makeImageView(vkd, device, *m_destination, mapImageViewType(m_params.dst.image.imageType), m_viewFormat,
                      colorSubresourceRange);

    DescriptorSetLayoutBuilder descSetLayoutBuilder;
    descSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    descSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    Move<VkDescriptorSetLayout> descSetLayout = descSetLayoutBuilder.build(vkd, device);

    DescriptorPoolBuilder descPoolBuilder;
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Move<VkDescriptorPool> descPool =
        descPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSet> descSet = makeDescriptorSet(vkd, device, descPool.get(), descSetLayout.get());

    DescriptorSetUpdateBuilder descSetUpdateBuilder;

    const auto srcImageDescInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *srcImageImageView, VK_IMAGE_LAYOUT_GENERAL);
    const auto dstImageDescInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *dstImageImageView, VK_IMAGE_LAYOUT_GENERAL);

    descSetUpdateBuilder.writeSingle(descSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcImageDescInfo);
    descSetUpdateBuilder.writeSingle(descSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstImageDescInfo);

    descSetUpdateBuilder.update(vkd, device);

    // Compute pipeline
    Move<VkShaderModule> compModule =
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("compFill"), 0u);

    const Move<VkPipelineLayout> computePipelineLayout = makePipelineLayout(vkd, device, 1, &(*descSetLayout));
    Move<VkPipeline> computePipeline = makeComputePipeline(vkd, device, *computePipelineLayout, *compModule);

    DE_ASSERT(m_params.src.image.format == m_params.dst.image.format);

    const tcu::IVec3 size =
        getSizeInBlocks(m_params.src.image.format, m_params.src.image.imageType, m_params.src.image.extent);

    VkQueue queue                    = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf           = VK_NULL_HANDLE;
    VkCommandPool cmdpool            = VK_NULL_HANDLE;
    std::tie(queue, cmdbuf, cmdpool) = activeExecutionCtx();

    const auto srcImageBarrierPre =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               m_source.get(), colorSubresourceRange);
    const auto dstImageBarrierPre =
        makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                               m_destination.get(), colorSubresourceRange);

    const auto srcImageBarrierPost =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_source.get(), colorSubresourceRange);
    const auto dstImageBarrierPost =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_destination.get(), colorSubresourceRange);

    // Execute commands to fill the images
    {
        beginCommandBuffer(vkd, cmdbuf);

        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1u, &srcImageBarrierPre);
        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1u, &dstImageBarrierPre);

        vkd.cmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
        vkd.cmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u,
                                  &descSet.get(), 0u, nullptr);
        vkd.cmdDispatch(cmdbuf, size.x(), size.y(), 1u);

        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1u, &srcImageBarrierPost);
        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1u, &dstImageBarrierPost);

        endCommandBuffer(vkd, cmdbuf);
        submitCommandsAndWait(vkd, device, queue, cmdbuf);
    }

    m_context.resetCommandPoolForVKSC(device, cmdpool);
}

/* Verify results of copy or sampling via compute shader when source
   and destination images are of compressed format.
*/
tcu::TestStatus ReinterpretTestInstance::checkTestResult(const VkImage &testImage, const VkFormat testImageFormat,
                                                         const VkImageType testImageType,
                                                         const VkExtent3D &testImageExtent,
                                                         const VkAccessFlags lastAccess, const VkImageLayout lastLayout,
                                                         const VkPipelineStageFlags lastStage)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_device;
    Allocator &alloc           = *m_allocator;

    const tcu::IVec3 size = getSizeInBlocks(testImageFormat, testImageType, testImageExtent);

    // Color output from shader
    const auto outputFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageCreateInfo outputImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0u),              // VkImageCreateFlags flags;
        testImageType,                       // VkImageType imageType;
        outputFormat,                        // VkFormat format;
        makeExtent3D(size),                  // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    const ImageWithMemory outputImage{vkd, device, alloc, outputImageParams, MemoryRequirement::Any};

    // Descriptors for storage images
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    Move<VkImageView> testImageView =
        makeImageView(vkd, device, testImage, mapImageViewType(testImageType), m_viewFormat, colorSubresourceRange);
    Move<VkImageView> outputImageView =
        makeImageView(vkd, device, *outputImage, mapImageViewType(testImageType), outputFormat, colorSubresourceRange);

    DescriptorSetLayoutBuilder descSetLayoutBuilder;
    descSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    descSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    Move<VkDescriptorSetLayout> descSetLayout = descSetLayoutBuilder.build(vkd, device);

    DescriptorPoolBuilder descPoolBuilder;
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Move<VkDescriptorPool> descPool =
        descPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSet> descSet = makeDescriptorSet(vkd, device, descPool.get(), descSetLayout.get());

    DescriptorSetUpdateBuilder descSetUpdateBuilder;

    const auto testImageDescInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *testImageView, VK_IMAGE_LAYOUT_GENERAL);

    const auto outputImageDescInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *outputImageView, VK_IMAGE_LAYOUT_GENERAL);

    descSetUpdateBuilder.writeSingle(descSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &testImageDescInfo);
    descSetUpdateBuilder.writeSingle(descSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outputImageDescInfo);

    descSetUpdateBuilder.update(vkd, device);

    // Compute pipeline
    Move<VkShaderModule> compModule =
        createShaderModule(vkd, device, m_context.getBinaryCollection().get("compVerify"), 0u);

    const Move<VkPipelineLayout> computePipelineLayout = makePipelineLayout(vkd, device, 1, &(*descSetLayout));
    Move<VkPipeline> computePipeline = makeComputePipeline(vkd, device, *computePipelineLayout, *compModule);

    VkQueue queue                    = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf           = VK_NULL_HANDLE;
    VkCommandPool cmdpool            = VK_NULL_HANDLE;
    std::tie(queue, cmdbuf, cmdpool) = activeExecutionCtx();

    const auto testImageBarrierPre = makeImageMemoryBarrier(lastAccess, VK_ACCESS_SHADER_WRITE_BIT, lastLayout,
                                                            VK_IMAGE_LAYOUT_GENERAL, testImage, colorSubresourceRange);

    const auto outputImageBarrierPost =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, outputImage.get(), colorSubresourceRange);

    // Result buffer for shader output
    const VkDeviceSize resultBufferSize = static_cast<VkDeviceSize>(
        static_cast<uint32_t>(getPixelSize(mapVkFormat(outputFormat))) * static_cast<uint32_t>(size.x()) *
        static_cast<uint32_t>(size.y()) * static_cast<uint32_t>(size.z()));
    const tcu::IVec2 resultSize{size.x(), size.y()};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, alloc, resultBufferInfo, MemoryRequirement::HostVisible};

    const tcu::Vec4 outputClearColor(tcu::RGBA::black().toVec());
    const auto outputClearColorValue = makeClearValueColorVec4(outputClearColor);
    const auto outputImageBarrierPreClear =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, outputImage.get(), colorSubresourceRange);
    const auto outputImageBarrierPostClear = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL, outputImage.get(), colorSubresourceRange);

    // Execute commands to fill the images
    {
        beginCommandBuffer(vkd, cmdbuf);

        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &outputImageBarrierPreClear);
        vkd.cmdClearColorImage(cmdbuf, outputImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               &outputClearColorValue.color, 1u, &colorSubresourceRange);
        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &outputImageBarrierPostClear);

        vkd.cmdPipelineBarrier(cmdbuf, lastStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1u,
                               &testImageBarrierPre);

        vkd.cmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
        vkd.cmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u,
                                  &descSet.get(), 0u, nullptr);
        vkd.cmdDispatch(cmdbuf, size.x(), size.y(), 1u);

        vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                               nullptr, 0, nullptr, 1u, &outputImageBarrierPost);

        copyImageToBuffer(vkd, cmdbuf, outputImage.get(), resultBuffer.get(), resultSize, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        endCommandBuffer(vkd, cmdbuf);
        submitCommandsAndWait(vkd, device, queue, cmdbuf);
    }

    m_context.resetCommandPoolForVKSC(device, cmdpool);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);

    tcu::TextureLevel reference(mapVkFormat(outputFormat), resultSize[0], resultSize[1]);
    tcu::clear(reference.getAccess(), tcu::RGBA::green().toVec());
    const auto resultBufferPtr =
        reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
    const tcu::ConstPixelBufferAccess resultPixels{mapVkFormat(outputFormat), resultSize[0], resultSize[1], 1,
                                                   resultBufferPtr};
    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                    reference.getAccess(), resultPixels, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Fail");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus ReinterpretTestInstance::iterate(void)
{
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vkd   = m_context.getDeviceInterface();
    const auto phyDevice         = m_context.getPhysicalDevice();
    const VkDevice device        = m_device;
    Allocator &alloc             = *m_allocator;

    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto outputFormat          = m_viewFormat;

    const tcu::TextureFormat srcTcuFormat       = getSizeCompatibleTcuTextureFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat       = getSizeCompatibleTcuTextureFormat(m_params.dst.image.format);
    const tcu::TextureFormat outputDstTcuFormat = getSizeCompatibleTcuTextureFormat(outputFormat);
    const bool srcCompressed                    = isCompressedFormat(m_params.src.image.format);
    const bool dstCompressed                    = isCompressedFormat(m_params.dst.image.format);

    tcu::IVec3 outputImageSize = {static_cast<int>(m_params.src.image.extent.width),
                                  static_cast<int>(m_params.src.image.extent.height), 1};
    if (srcCompressed)
        outputImageSize =
            getSizeInBlocks(m_params.src.image.format, m_params.src.image.imageType, m_params.src.image.extent);

    const auto renderArea              = makeRect2D(tcu::IVec2(outputImageSize.x(), outputImageSize.y()));
    const VkExtent3D outputImageExtent = makeExtent3D(outputImageSize.x(), outputImageSize.y(), outputImageSize.z());

    const tcu::Vec4 renderClearColor(tcu::RGBA::white().toVec());

    de::MovePtr<tcu::TextureLevel> outputTexureLevel;
    de::MovePtr<tcu::ConstPixelBufferAccess> outputTexureLevelPixels;

    // Initialize the src and dst images
    {
        if (!srcCompressed)
        {
            m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
                new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width,
                                      (int)m_params.src.image.extent.height, (int)m_params.src.image.extent.depth));
            generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width,
                           m_params.src.image.extent.height, m_params.src.image.extent.depth,
                           m_params.src.image.fillMode);
            m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
                new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width,
                                      (int)m_params.dst.image.extent.height, (int)m_params.dst.image.extent.depth));
            generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                           m_params.dst.image.extent.height, m_params.dst.image.extent.depth,
                           m_params.clearDestinationWithRed ? FILL_MODE_RED : m_params.dst.image.fillMode);
            generateExpectedResult();

            uploadImage(m_sourceTextureLevel->getAccess(), m_source.get(), m_params.src.image,
                        m_params.useGeneralLayout);
            uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image,
                        m_params.useGeneralLayout);

            outputTexureLevel = de::MovePtr<tcu::TextureLevel>(
                new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width,
                                      (int)m_params.src.image.extent.height, (int)m_params.src.image.extent.depth));
            generateBuffer(outputTexureLevel->getAccess(), m_params.src.image.extent.width,
                           m_params.src.image.extent.height, m_params.src.image.extent.depth,
                           m_params.src.image.fillMode);
            // Same buffer, different format - view format
            outputTexureLevelPixels = de::MovePtr<tcu::ConstPixelBufferAccess>(new tcu::ConstPixelBufferAccess(
                outputDstTcuFormat, m_params.src.image.extent.width, m_params.src.image.extent.height,
                m_params.src.image.extent.depth, outputTexureLevel->getAccess().getDataPtr()));
        }
        else
        {
            fillCompressedImages();
        }
    }

    // Image copy areas
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

    // Barriers for copy
    VkMemoryBarrier memoryBarriers[] = {
        // source image
        {makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)},
        // destination image
        {makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT)},
    };

    VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,                                    // VkStructureType sType;
         nullptr,                                                                   // const void* pNext;
         srcCompressed ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,                                               // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                                      // VkImageLayout oldLayout;
         m_params.src.image.operationLayout,                                        // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                                                   // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                                                   // uint32_t dstQueueFamilyIndex;
         m_source.get(),                                                            // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.src.image) // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,                                    // VkStructureType sType;
         nullptr,                                                                   // const void* pNext;
         dstCompressed ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,                                              // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,                                      // VkImageLayout oldLayout;
         m_params.dst.image.operationLayout,                                        // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                                                   // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                                                   // uint32_t dstQueueFamilyIndex;
         m_destination.get(),                                                       // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstTcuFormat),    // VkImageAspectFlags aspectMask;
             0u,                              // uint32_t baseMipLevel;
             1u,                              // uint32_t mipLevels;
             0u,                              // uint32_t baseArraySlice;
             getArraySize(m_params.dst.image) // uint32_t arraySize;
         }},
    };

    // Image layouts for copy
    const VkImageLayout srcLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : m_params.src.image.operationLayout;
    const VkImageLayout dstLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : m_params.dst.image.operationLayout;

    // Image view in view format
    Move<VkImageView> imageView =
        makeImageView(vkd, device, m_source.get(), mapImageViewType(m_params.src.image.imageType), m_viewFormat,
                      colorSubresourceRange);

    // Sampler
    VkSamplerCreateInfo samplerCreateInfo = makeSamplerCreateInfo();
    Move<VkSampler> imageSampler          = createSampler(vkd, device, &samplerCreateInfo);

    // Descriptor for combined image sampler
    DescriptorSetLayoutBuilder descSetLayoutBuilder;
    descSetLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    Move<VkDescriptorSetLayout> descSetLayout = descSetLayoutBuilder.build(vkd, device);

    DescriptorPoolBuilder descPoolBuilder;
    descPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Move<VkDescriptorPool> descPool =
        descPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    Move<VkDescriptorSet> descSet = makeDescriptorSet(vkd, device, descPool.get(), descSetLayout.get());

    DescriptorSetUpdateBuilder descSetUpdateBuilder;

    const auto combinedImageSampler =
        makeDescriptorImageInfo(imageSampler.get(), imageView.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    descSetUpdateBuilder.writeSingle(descSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &combinedImageSampler);

    descSetUpdateBuilder.update(vkd, device);

    // Shader modules
    const auto vertexModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule   = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Color output from shader
    VkImageCreateInfo outputImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0u),              // VkImageCreateFlags flags;
        m_params.src.image.imageType,        // VkImageType imageType;
        outputFormat,                        // VkFormat format;
        outputImageExtent,                   // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,      // VkSharingMode sharingMode;
        0u,                             // uint32_t queueFamilyIndexCount;
        nullptr,                        // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,      // VkImageLayout initialLayout;
    };

    const ImageWithMemory outputImage{vkd, device, alloc, outputImageParams, MemoryRequirement::Any};
    RenderPassWrapper renderPass(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vkd, device, outputFormat);

    // Framebuffer
    const auto outputImageView =
        makeImageView(vkd, device, outputImage.get(), mapImageViewType(m_params.src.image.imageType), outputFormat,
                      colorSubresourceRange);

    renderPass.createFramebuffer(vkd, device, 1u, &outputImage.get(), &outputImageView.get(), outputImageExtent.width,
                                 outputImageExtent.height, outputImageExtent.depth);

    // Pipeline
    const PipelineLayoutWrapper pipelineLayout(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vkd, device, descSetLayout.get());
    GraphicsPipelineWrapper graphicsPipeline(vki, vkd, phyDevice, device, m_context.getDeviceExtensions(),
                                             PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);

    {
        const std::vector<VkViewport> viewports{makeViewport(outputImageExtent)};
        const std::vector<VkRect2D> scissors{makeRect2D(tcu::IVec2(outputImageExtent.width, outputImageExtent.height))};
        const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

        graphicsPipeline.setDefaultDepthStencilState()
            .setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultColorBlendState()
            .setDefaultMultisampleState()
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertexModule)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragModule)
            .setupFragmentOutputState(*renderPass)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();
    }

    // Barrier omitting VK_ACCESS_SHADER_READ_BIT
    const VkImageMemoryBarrier inputImageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, 0u, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_source.get(), colorSubresourceRange);

    // Result buffer for shader output
    const VkDeviceSize resultBufferSize =
        static_cast<VkDeviceSize>(static_cast<uint32_t>(getPixelSize(outputDstTcuFormat)) * outputImageExtent.width *
                                  outputImageExtent.height * outputImageExtent.depth);
    const tcu::IVec2 resultSize{static_cast<int>(outputImageExtent.width), static_cast<int>(outputImageExtent.height)};
    const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory resultBuffer{vkd, device, alloc, resultBufferInfo, MemoryRequirement::HostVisible};

    VkQueue queue                    = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf           = VK_NULL_HANDLE;
    VkCommandPool cmdpool            = VK_NULL_HANDLE;
    std::tie(queue, cmdbuf, cmdpool) = activeExecutionCtx();

    // Execute copy from source image and then sample the source image
    {
        beginCommandBuffer(vkd, cmdbuf);

        // Copy
        {
            vkd.cmdPipelineBarrier(
                cmdbuf, srcCompressed ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
                (m_params.useGeneralLayout ? DE_LENGTH_OF_ARRAY(memoryBarriers) : 0), memoryBarriers, 0, nullptr,
                (m_params.useGeneralLayout ? 0 : DE_LENGTH_OF_ARRAY(imageBarriers)), imageBarriers);

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                vkd.cmdCopyImage(cmdbuf, m_source.get(), srcLayout, m_destination.get(), dstLayout,
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

                vkd.cmdCopyImage2(cmdbuf, &copyImageInfo2KHR);
            }
        }

        // Sample
        {
            vkd.cmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                   (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &inputImageBarrier);

            renderPass.begin(vkd, cmdbuf, renderArea, renderClearColor);

            graphicsPipeline.bind(cmdbuf);

            vkd.cmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                      &descSet.get(), 0u, nullptr);
            vkd.cmdDraw(cmdbuf, 6u, 1u, 0u, 0u);

            renderPass.end(vkd, cmdbuf);

            if (!srcCompressed)
            {
                copyImageToBuffer(vkd, cmdbuf, outputImage.get(), resultBuffer.get(), resultSize,
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
        }

        endCommandBuffer(vkd, cmdbuf);

        submitCommandsAndWaitWithTransferSync(vkd, device, queue, cmdbuf, &m_sparseSemaphore);
    }

    m_context.resetCommandPoolForVKSC(device, cmdpool);

    // Get results
    const auto &resultBufferAlloc = resultBuffer.getAllocation();
    invalidateAlloc(vkd, device, resultBufferAlloc);

    // Check results
    tcu::TestStatus copyTestStatus(QP_TEST_RESULT_PENDING, "Pending");
    if (!srcCompressed)
    {
        de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(*m_destination, m_params.dst.image);
        copyTestStatus                                    = checkTestResult(resultTextureLevel->getAccess());
    }
    else
    {
        copyTestStatus = checkTestResult(m_destination.get(), m_params.dst.image.format, m_params.dst.image.imageType,
                                         m_params.dst.image.extent, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    if (copyTestStatus.isFail())
        return tcu::TestStatus::fail("Copy test");
    ;

    tcu::TestStatus samplingTestStatus(QP_TEST_RESULT_PENDING, "Pending");
    if (!srcCompressed)
    {
        const auto resultBufferPtr =
            reinterpret_cast<const char *>(resultBufferAlloc.getHostPtr()) + resultBufferAlloc.getOffset();
        const tcu::ConstPixelBufferAccess resultPixels{outputDstTcuFormat, resultSize[0], resultSize[1], 1,
                                                       resultBufferPtr};

        if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                        *outputTexureLevelPixels.get(), resultPixels, tcu::Vec4(0.01f),
                                        tcu::COMPARE_LOG_ON_ERROR))
            samplingTestStatus = tcu::TestStatus::fail("Sampling test");
    }
    else
    {
        samplingTestStatus =
            checkTestResult(outputImage.get(), m_params.src.image.format, m_params.src.image.imageType,
                            m_params.src.image.extent, VK_ACCESS_SHADER_WRITE_BIT,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
    if (samplingTestStatus.isFail())
        return samplingTestStatus;

    return tcu::TestStatus::pass("Pass");
}

inline bool isIntegerFormat(const vk::VkFormat format)
{
    return isIntFormat(format) || isUintFormat(format);
}

std::string getColorFormatStr(const int numComponents, const bool isUint, const bool isSint)
{
    std::ostringstream str;
    if (numComponents == 1)
        str << (isUint ? "uint" : isSint ? "int" : "float");
    else
        str << (isUint ? "u" : isSint ? "i" : "") << "vec" << numComponents;

    return str.str();
}

inline bool formatsAreCompatible(const VkFormat format0, const VkFormat format1)
{
    return format0 == format1 || mapVkFormat(format0).getPixelSize() == mapVkFormat(format1).getPixelSize();
}

class ReinterpretTestCase : public vkt::TestCase
{
public:
    ReinterpretTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params,
                        const VkFormat viewFormat)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
        , m_viewFormat(viewFormat)
    {
        DE_ASSERT(m_params.src.image.format == m_params.dst.image.format);
        DE_ASSERT(m_params.src.image.tiling == VK_IMAGE_TILING_OPTIMAL);
        DE_ASSERT(m_params.allocationKind == ALLOCATION_KIND_SUBALLOCATED);
        DE_ASSERT(m_params.queueSelection == QueueSelectionOptions::Universal);
        DE_ASSERT(m_params.clearDestinationWithRed == false);
        DE_ASSERT(m_params.samples == VK_SAMPLE_COUNT_1_BIT);
        DE_ASSERT(m_params.imageOffset == false);
        DE_ASSERT(m_params.useSecondaryCmdBuffer == false);
        DE_ASSERT(m_params.useSparseBinding == false);
        DE_ASSERT(m_params.useGeneralLayout == false);
    }

    virtual TestInstance *createInstance(Context &context) const
    {
        return new ReinterpretTestInstance(context, m_params, m_viewFormat);
    }

    virtual void checkSupport(Context &context) const
    {
        const InstanceInterface &vki = context.getInstanceInterface();

        const auto usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        VkImageCreateFlags creationFlags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if (isCompressedFormat(m_params.src.image.format))
        {
            creationFlags |= (VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT);

            if (!context.isDeviceFunctionalitySupported("VK_KHR_maintenance2"))
                TCU_THROW(NotSupportedError,
                          "Device does not support extended image usage flags nor overriding implicit usage flags");
        }

        VkImageFormatProperties imageFormatProperties;

        if (vki.getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), m_params.src.image.format,
                                                       m_params.src.image.imageType, m_params.src.image.tiling,
                                                       usageFlags, creationFlags,
                                                       &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Image format not supported");
        }

        // Output
        if (vki.getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_viewFormat, m_params.src.image.imageType, VK_IMAGE_TILING_OPTIMAL,
                (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
                0u, &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Image format not supported");
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        const VkPhysicalDeviceLimits &limits = context.getDeviceProperties().limits;

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
    }

    virtual void initPrograms(SourceCollections &programCollection) const
    {
        const bool srcCompressed = isCompressedFormat(m_params.src.image.format);
        const bool isImageType1d = (m_params.src.image.imageType == VK_IMAGE_TYPE_1D); // !isImageType1d = 2D

        const std::string texCoordTypeStr = isImageType1d ? "float" : "vec2";

        std::ostringstream vert;
        {
            vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "layout(location=0) out " << texCoordTypeStr << " texCoord;\n"
                 << "vec2 positions[6] = vec2[](\n"
                 << "    vec2(-1.0f, 1.0f),\n"
                 << "    vec2(1.0f, 1.0f),\n"
                 << "    vec2(-1.0f, -1.0f),\n"
                 << "    vec2(-1.0f, -1.0f),\n"
                 << "    vec2(1.0f, 1.0f),\n"
                 << "    vec2(1.0f, -1.0f)\n"
                 << ");\n"
                 << "\n"
                 << "void main() {\n"
                 << "    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
                 << "    texCoord.x = (positions[gl_VertexIndex].x < 0.0) ? 0.0 : positions[gl_VertexIndex].x;\n";

            if (!isImageType1d)
                vert << "    texCoord.y = (positions[gl_VertexIndex].y < 0.0) ? 0.0 : positions[gl_VertexIndex].y;\n";

            vert << "}\n";
        }
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        const int numComponents              = getNumUsedChannels(mapVkFormat(m_viewFormat).order);
        const bool isUint                    = isUintFormat(m_viewFormat);
        const bool isSint                    = isIntFormat(m_viewFormat);
        const std::string dataTypeStr        = getColorFormatStr(numComponents, isUint, isSint);
        const std::string colorComponentsStr = (numComponents == 1 ? "r" :
                                                numComponents == 2 ? "rg" :
                                                numComponents == 3 ? "rgb" :
                                                                     "rgba");

        const std::string signStr = isIntegerFormat(m_viewFormat) ? (isUintFormat(m_viewFormat) ? "u" : "i") : "";

        std::ostringstream frag;
        {
            const std::string samplerStr = isImageType1d ? "sampler1D" : "sampler2D";

            tcu::IVec3 renderSize = {static_cast<int>(m_params.src.image.extent.width),
                                     static_cast<int>(m_params.src.image.extent.height), 1};
            if (srcCompressed)
                renderSize =
                    getSizeInBlocks(m_params.src.image.format, m_params.src.image.imageType, m_params.src.image.extent);

            const std::string fetchCoord =
                std::string(isImageType1d ? "int" : "ivec2") + "(texCoord.x * " + de::toString(renderSize.x()) +
                (isImageType1d ? "" : (", texCoord.y * " + de::toString(renderSize.y()))) + ")";

            frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "layout(set=0, binding=0) uniform " << signStr << samplerStr << " tex;\n"
                 << "layout(location=0) in " << texCoordTypeStr << " texCoord;\n"
                 << "layout(location=0) out " << dataTypeStr << " outColor;\n"
                 << "\n"
                 << "void main() {\n"
                 << "    " << signStr << "vec4 texColor = texelFetch(tex, " << fetchCoord << ", 0);\n"
                 << "    outColor = " << dataTypeStr << "(texColor." << colorComponentsStr << ");\n"
                 << "}\n";
        }
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

        if (srcCompressed)
        {
            DE_ASSERT(isUint);
            DE_ASSERT(numComponents >= 2); // Only 64b view formats supported

            const bool is64b = (getBlockSizeInBytes(m_params.src.image.format) == 8u); // true: 64b, false: 128b
            const std::string layoutFmtStr = getShaderImageFormatQualifier(mapVkFormat(m_viewFormat));

            const std::string bc1Red4  = "4160813056u, 0u, 4160813056u, 0u";
            const std::string bc1Blue2 = "2031647, 0u";
            const std::string bc1Blue4 = bc1Blue2 + ", " + bc1Blue2;
            const std::string bc3Red4  = "4294967295u, 4294967295u, 4160813056u, 0u";
            const std::string bc3Blue4 = "4294967295u, 4294967295u, 2031647, 0u";

            const std::string red = is64b ? bc1Red4 : bc3Red4;

            const std::string imageTypeStr = isImageType1d ? "image1D" : "image2D";
            const std::string fetchCoord   = std::string(isImageType1d ? "int" : "ivec2") + "(gl_GlobalInvocationID.x" +
                                           std::string(isImageType1d ? "" : "y") + ")";

            // Compute shader for filling compressed images
            std::ostringstream compFill;
            {
                const std::string blue = is64b ? bc1Blue4 : bc3Blue4;

                compFill << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                         << "layout(set = 0, binding = 0, " << layoutFmtStr << ") uniform highp " << signStr
                         << imageTypeStr << " srcImg;\n"
                         << "layout(set = 0, binding = 1, " << layoutFmtStr << ") uniform highp " << signStr
                         << imageTypeStr << " dstImg;\n"
                         << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                         << "void main() {\n"
                         << "    " << signStr << "vec4 srcColor = " << signStr << "vec4(" << blue << "); // blue\n"
                         << "    " << signStr << "vec4 dstColor = " << signStr << "vec4(" << red << "); // red\n"
                         << "    imageStore(srcImg, " << fetchCoord << ", srcColor);\n"
                         << "    imageStore(dstImg, " << fetchCoord << ", dstColor);\n"
                         << "}\n";
            }
            programCollection.glslSources.add("compFill") << glu::ComputeSource(compFill.str());

            // Compute shader for verifying compressed images
            std::ostringstream compVerify;
            {
                const std::string blue = is64b ? bc1Blue2 : bc3Blue4;

                compVerify << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                           << "layout(set = 0, binding = 0, " << layoutFmtStr << ") uniform highp " << signStr
                           << imageTypeStr << " dstImg;\n"
                           << "layout(set = 0, binding = 1, rgba8) uniform highp " << imageTypeStr << " outputImg;\n"
                           << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
                           << "void main() {\n"
                           << "    " << dataTypeStr << " color = " << dataTypeStr << "(" << blue << "); // blue\n"
                           << "    vec4 green = vec4(0.0f, 1.0f, 0.0f, 1.0f);\n"
                           << "    vec4 red = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
                           << "    " << dataTypeStr << " dstColor = imageLoad(dstImg, " << fetchCoord << ")."
                           << colorComponentsStr << ";\n"
                           << "    imageStore(outputImg, " << fetchCoord << ", color == dstColor ? green : red );\n"
                           << "}\n";
            }
            programCollection.glslSources.add("compVerify") << glu::ComputeSource(compVerify.str());
        }
    }

private:
    TestParams m_params;
    const VkFormat m_viewFormat;
};

} // namespace

tcu::TestCaseGroup *createReinterpretationTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> reinterpretGroup(new tcu::TestCaseGroup(testCtx, "reinterpret"));

    {
        const struct FormatPairs
        {
            VkFormat imageFormat;
            VkFormat viewFormat;
        } fmtPairs[] = {
            {VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_R16G16_SFLOAT},
            {VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_R32G32_UINT},
            {VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_R32G32B32A32_UINT},
        };

        const struct
        {
            VkImageType imageType;
            const VkExtent3D imageExtent;
            const char *imageTypeName;
        } imageTypes[] = {{VK_IMAGE_TYPE_1D, default1dExtent, "1d"}, {VK_IMAGE_TYPE_2D, defaultExtent, "2d"}};

        for (uint32_t imgTypeIdx = 0; imgTypeIdx < DE_LENGTH_OF_ARRAY(imageTypes); imgTypeIdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> dimGroup(
                new tcu::TestCaseGroup(testCtx, imageTypes[imgTypeIdx].imageTypeName));

            for (uint32_t fmtIdx = 0; fmtIdx < DE_LENGTH_OF_ARRAY(fmtPairs); fmtIdx++)
            {
                const VkFormat imageFormat = fmtPairs[fmtIdx].imageFormat;
                const VkFormat viewFormat  = fmtPairs[fmtIdx].viewFormat;

                if ((!isCompressedFormat(imageFormat) && formatsAreCompatible(imageFormat, viewFormat)) ||
                    isCompressedFormat(imageFormat))
                {

                    TestParams copyParams;
                    {
                        // Tests require same type of source and destination images
                        copyParams.src.image.imageType       = imageTypes[imgTypeIdx].imageType;
                        copyParams.src.image.format          = imageFormat;
                        copyParams.src.image.extent          = imageTypes[imgTypeIdx].imageExtent;
                        copyParams.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
                        copyParams.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                        copyParams.src.image.fillMode        = FILL_MODE_RED; // Unused in case of compressed
                        copyParams.dst.image.imageType       = copyParams.src.image.imageType;
                        copyParams.dst.image.format          = copyParams.src.image.format;
                        copyParams.dst.image.extent          = copyParams.src.image.extent;
                        copyParams.dst.image.tiling          = copyParams.src.image.tiling;
                        copyParams.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                        copyParams.dst.image.fillMode        = FILL_MODE_BLACK; // Unused in case of compressed
                        copyParams.allocationKind            = ALLOCATION_KIND_SUBALLOCATED;

                        {
                            const VkImageCopy testCopy = {
                                defaultSourceLayer,                 // VkImageSubresourceLayers srcSubresource;
                                {0, 0, 0},                          // VkOffset3D srcOffset;
                                defaultSourceLayer,                 // VkImageSubresourceLayers dstSubresource;
                                {0, 0, 0},                          // VkOffset3D dstOffset;
                                imageTypes[imgTypeIdx].imageExtent, // VkExtent3D extent;
                            };

                            CopyRegion imageCopy;
                            imageCopy.imageCopy = testCopy;
                            copyParams.regions.push_back(imageCopy);
                        }
                    }

                    const auto imageFmtStr     = de::toLower(std::string(getFormatName(imageFormat)).substr(10));
                    const auto viewFmtStr      = de::toLower(std::string(getFormatName(viewFormat)).substr(10));
                    const std::string testName = "copy_" + imageFmtStr + "_sample_" + viewFmtStr;
                    dimGroup->addChild(new ReinterpretTestCase(testCtx, testName, copyParams, viewFormat));
                }
            }
            reinterpretGroup->addChild(dimGroup.release());
        }
    }

    return reinterpretGroup.release();
}

} // namespace api
} // namespace vkt
