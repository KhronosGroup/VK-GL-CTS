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
 * \brief Vulkan Dynamic State Meta Operations Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingDynamicStateMetaOpsTests.hpp"

namespace vkt
{

namespace api
{

namespace
{
enum class MetaOperation
{
    META_OP_COPY = 0,
    META_OP_BLIT,
    META_OP_NONE
};
struct DynamicStateMetaOpsTestParams
{
    MetaOperation metaOp;
    VkFormat multisampledImageFormat;
    VkSampleCountFlagBits multisampledImageSampleCount;
};

struct PushConsts
{
    int32_t drawCount;
    int32_t width;
    int32_t height;
    int32_t numSamples;
};

class DynamicStateMetaOpsInstance final : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    DynamicStateMetaOpsInstance(Context &context, TestParams params, const DynamicStateMetaOpsTestParams metaOpsParams);
    tcu::TestStatus iterate(void) override;

private:
    void initDraw();
    void doDraw(const VkCommandBuffer &cmdBuffer, uint32_t drawCount);
    tcu::TestStatus verifyDraws();

    void initMetaOp();
    void doCopy(const VkCommandBuffer &cmdBuffer);
    void doBlit(const VkCommandBuffer &cmdBuffer);
    tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result = tcu::ConstPixelBufferAccess()) override;
    void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst, CopyRegion region,
                                  uint32_t mipLevel = 0u) override;
    bool checkNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
                                    const tcu::ConstPixelBufferAccess &source);
    tcu::TestStatus verifyMetaOp();

    de::MovePtr<ImageWithMemory> m_source;
    de::MovePtr<ImageWithMemory> m_destination;

    de::MovePtr<ImageWithMemory> m_multisampledImage;
    Move<VkImageView> m_multisampledImageView;
    std::vector<tcu::Vec4> m_vertices;
    de::MovePtr<BufferWithMemory> m_vertexBuffer;
    de::MovePtr<RenderPassWrapper> m_renderPass;
    de::MovePtr<PipelineLayoutWrapper> m_pipelineLayout;
    de::MovePtr<GraphicsPipelineWrapper> m_graphicsPipeline;

    const DynamicStateMetaOpsTestParams m_dynStateMetaOpsParams;
};

DynamicStateMetaOpsInstance::DynamicStateMetaOpsInstance(Context &context, TestParams params,
                                                         const DynamicStateMetaOpsTestParams dynStateMetaOpsParams)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
    , m_dynStateMetaOpsParams(dynStateMetaOpsParams)
{
    const DeviceInterface &vkd = context.getDeviceInterface();

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

        m_source = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vkd, m_device, *m_allocator, sourceImageParams, MemoryRequirement::Any));
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

        m_destination = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vkd, m_device, *m_allocator, destinationImageParams, MemoryRequirement::Any));
    }

    // Create a multisampled image
    {
        VkImageCreateInfo msImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                  // VkStructureType sType;
            nullptr,                                              // const void* pNext;
            VkImageCreateFlags(0u),                               // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                     // VkImageType imageType;
            m_dynStateMetaOpsParams.multisampledImageFormat,      // VkFormat format;
            defaultExtent,                                        // VkExtent3D extent;
            1u,                                                   // uint32_t mipLevels;
            1u,                                                   // uint32_t arrayLayers;
            m_dynStateMetaOpsParams.multisampledImageSampleCount, // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                              // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            0u,                                                                    // uint32_t queueFamilyIndexCount;
            nullptr,                   // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_multisampledImage = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vkd, m_device, *m_allocator, msImageParams, MemoryRequirement::Any));
    }
}

bool DynamicStateMetaOpsInstance::checkNearestFilteredResult(const tcu::ConstPixelBufferAccess &result,
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

tcu::TestStatus DynamicStateMetaOpsInstance::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    if (m_dynStateMetaOpsParams.metaOp == MetaOperation::META_OP_COPY)
    {
        if (!tcu::bitwiseCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison",
                                 m_expectedTextureLevel[0]->getAccess(), result, tcu::COMPARE_LOG_ON_ERROR))
            return tcu::TestStatus::fail("Copy test");
    }
    else
    {
        if (!checkNearestFilteredResult(result, m_sourceTextureLevel->getAccess()))
            return tcu::TestStatus::fail("Blit test");
    }

    return tcu::TestStatus::pass("Pass");
}

void DynamicStateMetaOpsInstance::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                           CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    if (m_dynStateMetaOpsParams.metaOp == MetaOperation::META_OP_COPY)
    {
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

        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
        // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
        const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
        const tcu::PixelBufferAccess dstSubRegion = tcu::getSubregion(
            dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z, extent.width, extent.height, extent.depth);

        tcu::copy(dstSubRegion, srcSubRegion);
    }
    else
    {
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

        const tcu::Sampler::FilterMode filter = tcu::Sampler::LINEAR;

        const tcu::ConstPixelBufferAccess srcSubRegion =
            tcu::getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, srcExtent.x, srcExtent.y, srcExtent.z);
        const tcu::PixelBufferAccess dstSubRegion =
            tcu::getSubregion(dst, dstOffset.x, dstOffset.y, dstOffset.z, dstExtent.x, dstExtent.y, dstExtent.z);
        blit(dstSubRegion, srcSubRegion, filter, mirrorMode);
    }
}

void DynamicStateMetaOpsInstance::initDraw()
{
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vkd   = m_context.getDeviceInterface();
    const auto phyDevice         = m_context.getPhysicalDevice();
    const VkDevice device        = m_device;
    Allocator &alloc             = *m_allocator;

    const bool withDynamicRendering     = true;
    const auto pipelineConstructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;
    const auto colorSubresourceRange    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkRect2D renderArea           = makeRect2D(defaultExtent.width, defaultExtent.height);

    // Initialize vertices
    {
        const tcu::Vec4 a(-1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 b(1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 c(1.0f, 1.0f, 0.0f, 1.0f);
        const tcu::Vec4 d(-1.0f, 1.0f, 0.0f, 1.0f);

        m_vertices.push_back(a);
        m_vertices.push_back(c);
        m_vertices.push_back(b);
        m_vertices.push_back(a);
        m_vertices.push_back(c);
        m_vertices.push_back(d);
    }

    // Create vertex buffer
    const VkDeviceSize vertexDataSize = m_vertices.size() * sizeof(tcu::Vec4);

    m_vertexBuffer = static_cast<de::MovePtr<BufferWithMemory>>(new BufferWithMemory(
        vkd, device, alloc, makeBufferCreateInfo(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    {
        const auto &vertexBufferAlloc = m_vertexBuffer->getAllocation();
        const auto vertexDataPtr =
            reinterpret_cast<char *>(vertexBufferAlloc.getHostPtr()) + vertexBufferAlloc.getOffset();
        deMemcpy(vertexDataPtr, de::dataOrNull(m_vertices), static_cast<size_t>(vertexDataSize));
        flushAlloc(vkd, device, vertexBufferAlloc);
    }

    // Push constants
    const auto pushConstantSize = static_cast<uint32_t>(sizeof(PushConsts));

    // Shader modules
    const auto vertexModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule   = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Render pass with VK_ATTACHMENT_LOAD_OP_LOAD
    {
        const VkAttachmentDescription colorAttachment = {
            0u,                                                   // VkAttachmentDescriptionFlags flags;
            m_dynStateMetaOpsParams.multisampledImageFormat,      // VkFormat format;
            m_dynStateMetaOpsParams.multisampledImageSampleCount, // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,                           // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                         // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,                      // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                     // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,                              // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL              // VkImageLayout finalLayout;
        };

        const VkAttachmentReference colorRef = {
            0u,                                       // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
        };

        const VkSubpassDescription subpass = {
            0u,                              // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference* pInputAttachments;
            1u,                              // uint32_t colorAttachmentCount;
            &colorRef,                       // const VkAttachmentReference* pColorAttachments;
            nullptr,                         // const VkAttachmentReference* pResolveAttachments;
            nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            nullptr,                         // const uint32_t* pPreserveAttachments;
        };

        const VkRenderPassCreateInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            1u,                                        // uint32_t attachmentCount;
            &colorAttachment,                          // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpass,                                  // const VkSubpassDescription* pSubpasses;
            0u,                                        // uint32_t dependencyCount;
            nullptr,                                   // const VkSubpassDependency* pDependencies;
        };

        m_renderPass = static_cast<de::MovePtr<RenderPassWrapper>>(
            new RenderPassWrapper(vkd, device, &renderPassInfo, withDynamicRendering));
    }

    // Framebuffer
    m_multisampledImageView = makeImageView(vkd, device, m_multisampledImage->get(), VK_IMAGE_VIEW_TYPE_2D,
                                            m_dynStateMetaOpsParams.multisampledImageFormat, colorSubresourceRange);

    m_renderPass->createFramebuffer(vkd, device, 1u, &(m_multisampledImage->get()), &m_multisampledImageView.get(),
                                    defaultExtent.width, defaultExtent.height, defaultExtent.depth);

    // Pipeline
    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        pushConstantSize,             // uint32_t size;
    };
    m_pipelineLayout = static_cast<de::MovePtr<PipelineLayoutWrapper>>(
        new PipelineLayoutWrapper(pipelineConstructionType, vkd, device, VK_NULL_HANDLE, &pushConstantRange));
    m_graphicsPipeline = static_cast<de::MovePtr<GraphicsPipelineWrapper>>(new GraphicsPipelineWrapper(
        vki, vkd, phyDevice, device, m_context.getDeviceExtensions(), pipelineConstructionType));

    {
        const std::vector<VkViewport> viewports{makeViewport(defaultExtent)};
        const std::vector<VkRect2D> scissors{renderArea};

        const std::vector<VkDynamicState> dynamicStates = {
#ifndef CTS_USES_VULKANSC
            VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
#endif // CTS_USES_VULKANSC
        };

        const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                              // const void* pNext;
            0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
            de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
            de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
        };

        PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
#ifndef CTS_USES_VULKANSC
        VkPipelineRenderingCreateInfoKHR renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                             nullptr,
                                                             0u,
                                                             1u,
                                                             &m_dynStateMetaOpsParams.multisampledImageFormat,
                                                             VK_FORMAT_UNDEFINED,
                                                             VK_FORMAT_UNDEFINED};

        renderingCreateInfoWrapper.ptr = withDynamicRendering ? &renderingCreateInfo : nullptr;
#endif // CTS_USES_VULKANSC

        const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
            VK_TRUE,                             // VkBool32 blendEnable;
            VK_BLEND_FACTOR_SRC_ALPHA,           // VkBlendFactor srcColorBlendFactor;
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // VkBlendFactor dstColorBlendFactor;
            VK_BLEND_OP_ADD,                     // VkBlendOp colorBlendOp;
            VK_BLEND_FACTOR_ZERO,                // VkBlendFactor srcAlphaBlendFactor;
            VK_BLEND_FACTOR_ONE,                 // VkBlendFactor dstAlphaBlendFactor;
            VK_BLEND_OP_ADD,                     // VkBlendOp alphaBlendOp;
            (                                    // VkColorComponentFlags colorWriteMask;
                VK_COLOR_COMPONENT_R_BIT | vk::VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT),
        };

        const VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
            VK_FALSE,                                                 // VkBool32 logicOpEnable;
            VK_LOGIC_OP_NO_OP,                                        // VkLogicOp logicOp;
            1u,                                                       // uint32_t attachmentCount;
            &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
            {0.0f, 0.0f, 0.0f, 0.0f},   // float blendConstants[4];
        };

        m_graphicsPipeline
            ->setDynamicState(&dynamicStateCreateInfo)
#ifndef CTS_USES_VULKANSC
            .setRenderingColorAttachmentsInfo(renderingCreateInfoWrapper)
#endif
            .setDefaultDepthStencilState()
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultVertexInputState(true)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, *m_pipelineLayout, **m_renderPass, 0u, vertexModule,
                                              nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr,
                                              nullptr, renderingCreateInfoWrapper)
            .setupFragmentShaderState(*m_pipelineLayout, **m_renderPass, 0u, fragModule)
            .setupFragmentOutputState(**m_renderPass, 0u, &colorBlendInfo)
            .setMonolithicPipelineLayout(*m_pipelineLayout)
            .buildPipeline();
    }

    // // Initialize result buffer
    // const VkDeviceSize resultBufferSize = static_cast<VkDeviceSize>(
    //     static_cast<uint32_t>(getPixelSize(mapVkFormat(m_metaOpsParams.multisampledImageFormat))) * defaultExtent.width * defaultExtent.height * defaultExtent.depth);
    // const auto resultBufferInfo = makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    // m_resultBuffer = static_cast<de::MovePtr<BufferWithMemory>>(new BufferWithMemory(vkd, device, alloc, resultBufferInfo, MemoryRequirement::HostVisible));
}

void DynamicStateMetaOpsInstance::doDraw(const VkCommandBuffer &cmdBuffer, uint32_t drawCount)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    const VkRect2D renderArea             = makeRect2D(defaultExtent.width, defaultExtent.height);
    const VkDeviceSize vertexBufferOffset = 0u;

    const struct PushConsts pushConstantData = {
        static_cast<int32_t>(drawCount),
        static_cast<int32_t>(defaultExtent.width),
        static_cast<int32_t>(defaultExtent.height),
        static_cast<int32_t>(m_dynStateMetaOpsParams.multisampledImageSampleCount),
    };

    const auto pushConstantSize = static_cast<uint32_t>(sizeof(PushConsts));
    const tcu::Vec4 clearColor(tcu::RGBA::red().toVec());
    const auto clearColorValue       = makeClearValueColorVec4(clearColor);
    const auto colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    const auto msImageBarrierPreClear =
        makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **m_multisampledImage, colorSubresourceRange);

    const auto msImageBarrierPostClear = makeImageMemoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL, **m_multisampledImage, colorSubresourceRange);

    const auto msImageBarrierPostDraw =
        makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               (drawCount == 0u) ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **m_multisampledImage,
                               colorSubresourceRange);

    // const auto colorSubresourceLayers  = makeDefaultImageSubresourceLayers();
    // const VkBufferImageCopy copyRegion = {
    //     0u,                                        // VkDeviceSize                bufferOffset
    //     0u,                                        // uint32_t                    bufferRowLength
    //     0u,                                        // uint32_t                    bufferImageHeight
    //     colorSubresourceLayers,                    // VkImageSubresourceLayers    imageSubresource
    //     makeOffset3D(0u, 0u, 0u),                  // VkOffset3D                    imageOffset
    //     defaultExtent                              // VkExtent3D                    imageExtent
    // };

    // Execute draw commands
    {
        if (!drawCount)
        {
            vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &msImageBarrierPreClear);

            vkd.cmdClearColorImage(cmdBuffer, **m_multisampledImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearColorValue.color, 1u, &colorSubresourceRange);

            vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                   (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &msImageBarrierPostClear);
        }

        m_renderPass->begin(vkd, cmdBuffer, renderArea);

        m_graphicsPipeline->bind(cmdBuffer);

        vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer->get(), &vertexBufferOffset);

        vkd.cmdPushConstants(cmdBuffer, m_pipelineLayout->get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                             &pushConstantData);

#ifndef CTS_USES_VULKANSC
        if (!drawCount)
            vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, m_dynStateMetaOpsParams.multisampledImageSampleCount);
#endif // CTS_USES_VULKANSC
        vkd.cmdDraw(cmdBuffer, de::sizeU32(m_vertices), 1u, 0u, 0u);

        m_renderPass->end(vkd, cmdBuffer);

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u,
                               &msImageBarrierPostDraw);

        // if (drawCount)
        // {
        //     vkd.cmdCopyImageToBuffer(cmdBuffer, **m_multisampledImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resultBuffer.get(),
        //                             1u, &copyRegion);
        // }
    }
}

tcu::TestStatus DynamicStateMetaOpsInstance::verifyDraws()
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_device;
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    auto &alloc           = *m_allocator;

    const auto numInputAttachments = 1u; // previously drawn multisampled image
    constexpr auto numSets         = 2u; // 1 for the output buffer, 1 for the input attachments
    const auto fbWidth             = defaultExtent.width;
    const auto fbHeight            = defaultExtent.height;

    const VkRect2D renderArea             = makeRect2D(defaultExtent.width, defaultExtent.height);
    const VkDeviceSize vertexBufferOffset = 0u;

    // Push constants
    const std::array<int, 3> pushConstantData = {{
        static_cast<int>(fbWidth),
        static_cast<int>(fbHeight),
        static_cast<int>(m_dynStateMetaOpsParams.multisampledImageSampleCount),
    }};

    const auto pushConstantSize =
        static_cast<uint32_t>(pushConstantData.size() * sizeof(decltype(pushConstantData)::value_type));

    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        pushConstantSize,             // uint32_t size;
    };

    // Shader modules
    const auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"));
    const auto fragModule   = createShaderModule(vkd, device, m_context.getBinaryCollection().get("fragVerify"));

    // Descriptor sets
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numInputAttachments);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);

    DescriptorSetLayoutBuilder layoutBuilderBuffer;
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto outputBufferSetLayout = layoutBuilderBuffer.build(vkd, device);

    DescriptorSetLayoutBuilder layoutBuilderAttachments;
    layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto inputAttachmentsSetLayout = layoutBuilderAttachments.build(vkd, device);

    const auto descriptorSetBuffer = makeDescriptorSet(vkd, device, descriptorPool.get(), outputBufferSetLayout.get());
    const auto descriptorSetAttachments =
        makeDescriptorSet(vkd, device, descriptorPool.get(), inputAttachmentsSetLayout.get());

    // Array with raw descriptor sets
    const std::array<VkDescriptorSet, numSets> descriptorSets = {{
        descriptorSetBuffer.get(),
        descriptorSetAttachments.get(),
    }};

    const std::array<VkDescriptorSetLayout, numSets> setLayouts = {{
        outputBufferSetLayout.get(),
        inputAttachmentsSetLayout.get(),
    }};

    // Storage buffer
    const auto bufferCount =
        static_cast<size_t>(fbWidth * fbHeight * m_dynStateMetaOpsParams.multisampledImageSampleCount);
    const auto bufferSize = static_cast<VkDeviceSize>(bufferCount * sizeof(tcu::Vec4) /*sizeof(int32_t)*/);
    BufferWithMemory buffer(vkd, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                            MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    void *bufferData  = bufferAlloc.getHostPtr();
    BufferWithMemory buffer2(vkd, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                             MemoryRequirement::HostVisible);
    auto &bufferAlloc2 = buffer2.getAllocation();
    void *bufferData2  = bufferAlloc2.getHostPtr();

    // Update descriptor set 0
    DescriptorSetUpdateBuilder updater;

    const auto bufferInfo  = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);
    const auto bufferInfo2 = makeDescriptorBufferInfo(buffer2.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo2);

    // Input attachment
    const auto imageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *m_multisampledImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    updater.writeSingle(descriptorSetAttachments.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfo);

    // Update descriptor set 1
    updater.update(vkd, device);

    // Pipeline layout
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        static_cast<uint32_t>(setLayouts.size()),      // uint32_t setLayoutCount;
        setLayouts.data(),                             // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                            // uint32_t pushConstantRangeCount;
        &pushConstantRange,                            // const VkPushConstantRange* pPushConstantRanges;
    };

    const auto pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutInfo);

    // Render pass
    const VkAttachmentDescription inputAttachmentDescription = {
        0u,                                                   // VkAttachmentDescriptionFlags flags;
        m_dynStateMetaOpsParams.multisampledImageFormat,      // VkFormat format;
        m_dynStateMetaOpsParams.multisampledImageSampleCount, // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_LOAD,                           // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                         // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                      // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                     // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_GENERAL,                              // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,             // VkImageLayout finalLayout;
    };

    std::vector<VkAttachmentDescription> attachmentDescriptions;
    attachmentDescriptions.push_back(inputAttachmentDescription);

    const VkAttachmentReference inputAttachmentReference = {0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    const VkSubpassDescription subpassDescription = {
        0u,                              // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        1u,                              // uint32_t inputAttachmentCount;
        &inputAttachmentReference,       // const VkAttachmentReference* pInputAttachments;
        0u,                              // uint32_t colorAttachmentCount;
        nullptr,                         // const VkAttachmentReference* pColorAttachments;
        nullptr,                         // const VkAttachmentReference* pResolveAttachments;
        nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                              // uint32_t preserveAttachmentCount;
        nullptr,                         // const uint32_t* pPreserveAttachments;
    };

    // self-dependency - load op is considered to write the attachment
    const VkSubpassDependency subpassDependency{
        0,                                             // uint32_t srcSubpass;
        0,                                             // uint32_t dstSubpass;
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags srcStageMask;
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags dstStageMask;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags srcAccessMask;
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,           // VkAccessFlags dstAccessMask;
        VK_DEPENDENCY_BY_REGION_BIT                    // VkDependencyFlags dependencyFlags;
    };

    const VkRenderPassCreateInfo renderPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescriptions),       // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescriptions),    // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
        1u,                                        // uint32_t dependencyCount;
        &subpassDependency,                        // const VkSubpassDependency* pDependencies;
    };

    const auto renderPass = createRenderPass(vkd, device, &renderPassInfo);

    // Framebuffer
    const auto framebuffer =
        makeFramebuffer(vkd, device, renderPass.get(), 1u, &*m_multisampledImageView, fbWidth, fbHeight);

    // Graphics pipeline
    const std::vector<VkViewport> viewports(1, makeViewport(defaultExtent));
    const std::vector<VkRect2D> scissors(1, makeRect2D(defaultExtent));

    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    const auto graphicsPipeline = makeGraphicsPipeline(
        vkd,                                  // const DeviceInterface&                        vk
        device,                               // const VkDevice                                device
        pipelineLayout.get(),                 // const VkPipelineLayout                        pipelineLayout
        vertexModule.get(),                   // const VkShaderModule                          vertexShaderModule
        VK_NULL_HANDLE,                       // const VkShaderModule                          tessellationControlModule
        VK_NULL_HANDLE,                       // const VkShaderModule                          tessellationEvalModule
        VK_NULL_HANDLE,                       // const VkShaderModule                          geometryShaderModule
        fragModule.get(),                     // const VkShaderModule                          fragmentShaderModule
        renderPass.get(),                     // const VkRenderPass                            renderPass
        viewports,                            // const std::vector<VkViewport>&                viewports
        scissors,                             // const std::vector<VkRect2D>&                  scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // const VkPrimitiveTopology                     topology
        0u,                                   // const uint32_t                                subpass
        0u,                                   // const uint32_t                                patchControlPoints
        nullptr,                  // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        nullptr,                  // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
        &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo

    // Command buffer
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Make sure input attachment can be read by the shader after the loadop is executed at the start of the renderpass
    const auto loadBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    beginCommandBuffer(vkd, cmdBuffer);
    {
        beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), renderArea);

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1u,
                               &loadBarrier, 0u, nullptr, 0u, nullptr);

        vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());

        vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer->get(), &vertexBufferOffset);

        vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                             pushConstantData.data());

        vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u,
                                  static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);

        vkd.cmdDraw(cmdBuffer, de::sizeU32(m_vertices), 1u, 0u, 0u);

        endRenderPass(vkd, cmdBuffer);

        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                               &bufferBarrier, 0u, nullptr, 0u, nullptr);
    }

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWaitWithTransferSync(vkd, device, m_universalQueue, cmdBuffer, &m_sparseSemaphore);
    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    // Verify results
    invalidateAlloc(vkd, device, bufferAlloc);
    invalidateAlloc(vkd, device, bufferAlloc2);
    std::vector<tcu::Vec4> outputFlags(bufferCount, tcu::Vec4(0.0f));
    std::vector<tcu::Vec4> expectedFlags(bufferCount, tcu::Vec4(0.0f));
    deMemcpy(outputFlags.data(), bufferData, static_cast<size_t>(bufferSize));
    deMemcpy(expectedFlags.data(), bufferData2, static_cast<size_t>(bufferSize));

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Verifying multisample dynamic state results" << tcu::TestLog::EndMessage;

    const auto sampleCount = static_cast<uint32_t>(m_dynStateMetaOpsParams.multisampledImageSampleCount);

    for (uint32_t x = 0u; x < fbWidth; ++x)
        for (uint32_t y = 0u; y < fbHeight; ++y)
            for (uint32_t s = 0u; s < sampleCount; ++s)
            {
                // if (s % 2 == 0) continue;
                const auto index = (y * fbWidth + x) * sampleCount + s;
                // if (!outputFlags[index])
                tcu::Vec4 diff = abs(expectedFlags[index] - outputFlags[index]);
                bool isOk      = boolAll(lessThanEqual(diff, tcu::Vec4(0.01f)));

                if (!isOk)
                {
                    std::ostringstream msg;
                    msg << "Verification failed for coordinates (" << x << ", " << y << ") sample " << s
                        << " output: " << outputFlags[index] << " expected: " << expectedFlags[index];
                    return tcu::TestStatus::fail(msg.str());
                }
            }

    log << tcu::TestLog::Message << "Verification passed" << tcu::TestLog::EndMessage;
    return tcu::TestStatus::pass("Pass");
}

void DynamicStateMetaOpsInstance::initMetaOp()
{
    const ImageParms &srcImageParams = m_params.src.image;
    const int srcWidth               = static_cast<int>(srcImageParams.extent.width);
    const int srcHeight              = static_cast<int>(srcImageParams.extent.height);
    const int srcDepth               = static_cast<int>(srcImageParams.extent.depth);
    const ImageParms &dstImageParams = m_params.dst.image;
    const int dstWidth               = static_cast<int>(dstImageParams.extent.width);
    const int dstHeight              = static_cast<int>(dstImageParams.extent.height);
    const int dstDepth               = static_cast<int>(dstImageParams.extent.depth);

    // Initialize source
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(srcImageParams.format);
    m_sourceTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(srcTcuFormat, srcWidth, srcHeight, srcDepth));
    generateBuffer(m_sourceTextureLevel->getAccess(), srcWidth, srcHeight, srcDepth, srcImageParams.fillMode);
    uploadImage(m_sourceTextureLevel->getAccess(), m_source->get(), srcImageParams, m_params.useGeneralLayout);

    // Initialize destination
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(dstImageParams.format);
    m_destinationTextureLevel =
        de::MovePtr<tcu::TextureLevel>(new tcu::TextureLevel(dstTcuFormat, dstWidth, dstHeight, dstDepth));
    generateBuffer(m_destinationTextureLevel->getAccess(), dstWidth, dstHeight, dstDepth, dstImageParams.fillMode);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination->get(), dstImageParams,
                m_params.useGeneralLayout);

    // Expected result
    generateExpectedResult();
}

void DynamicStateMetaOpsInstance::doCopy(const VkCommandBuffer &cmdBuffer)
{
    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;
    for (uint32_t i = 0; i < m_params.regions.size(); i++)
    {
        VkImageCopy imageCopy = m_params.regions[i].imageCopy;

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

    const ImageParms &srcImageParams      = m_params.src.image;
    const ImageParms &dstImageParams      = m_params.dst.image;
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(srcImageParams.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(srcImageParams.format);

    // Barriers
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
         srcImageParams.operationLayout,         // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_source->get(),                        // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
             0u,                           // uint32_t baseMipLevel;
             1u,                           // uint32_t mipLevels;
             0u,                           // uint32_t baseArraySlice;
             getArraySize(srcImageParams)  // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         nullptr,                                // const void* pNext;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
         dstImageParams.operationLayout,         // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
         m_destination->get(),                   // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstTcuFormat), // VkImageAspectFlags aspectMask;
             0u,                           // uint32_t baseMipLevel;
             1u,                           // uint32_t mipLevels;
             0u,                           // uint32_t baseArraySlice;
             getArraySize(dstImageParams)  // uint32_t arraySize;
         }},
    };

    // Execute copy
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           (VkDependencyFlags)0, (m_params.useGeneralLayout ? DE_LENGTH_OF_ARRAY(memoryBarriers) : 0),
                           memoryBarriers, 0, nullptr,
                           (m_params.useGeneralLayout ? 0 : DE_LENGTH_OF_ARRAY(imageBarriers)), imageBarriers);

    const VkImageLayout srcLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : m_params.src.image.operationLayout;
    const VkImageLayout dstLayout =
        m_params.useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : m_params.dst.image.operationLayout;
    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vkd.cmdCopyImage(cmdBuffer, m_source->get(), srcLayout, m_destination->get(), dstLayout,
                         (uint32_t)imageCopies.size(), imageCopies.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkCopyImageInfo2KHR copyImageInfo2KHR = {
            VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            m_source->get(),                         // VkImage srcImage;
            srcLayout,                               // VkImageLayout srcImageLayout;
            m_destination->get(),                    // VkImage dstImage;
            dstLayout,                               // VkImageLayout dstImageLayout;
            (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
            imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
        };

        vkd.cmdCopyImage2(cmdBuffer, &copyImageInfo2KHR);
    }
}

void DynamicStateMetaOpsInstance::doBlit(const VkCommandBuffer &cmdBuffer)
{
    std::vector<VkImageBlit> regions;
    std::vector<VkImageBlit2KHR> regions2KHR;

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

    const ImageParms &srcImageParams = m_params.src.image;
    const ImageParms &dstImageParams = m_params.dst.image;

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
         m_source->get(),                        // VkImage image;
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
         m_destination->get(),                   // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(dstImageParams.format), //   VkImageAspectFlags aspectMask;
             0u,                                    //   uint32_t baseMipLevel;
             1u,                                    //   uint32_t mipLevels;
             0u,                                    //   uint32_t baseArraySlice;
             getArraySize(m_params.dst.image)       //   uint32_t arraySize;
         }}};

    // Execute blit
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 2u, imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vkd.cmdBlitImage(cmdBuffer, m_source->get(), srcImageParams.operationLayout, m_destination->get(),
                         dstImageParams.operationLayout, de::sizeU32(regions), de::dataOrNull(regions),
                         m_params.filter);
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkBlitImageInfo2KHR blitImageInfo2KHR{
            VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            m_source->get(),                         // VkImage srcImage;
            srcImageParams.operationLayout,          // VkImageLayout srcImageLayout;
            m_destination->get(),                    // VkImage dstImage;
            dstImageParams.operationLayout,          // VkImageLayout dstImageLayout;
            de::sizeU32(regions2KHR),                // uint32_t regionCount;
            de::dataOrNull(regions2KHR),             // const VkImageBlit2KHR* pRegions;
            m_params.filter,                         // VkFilter filter;
        };
        vkd.cmdBlitImage2(cmdBuffer, &blitImageInfo2KHR);
    }
}

tcu::TestStatus DynamicStateMetaOpsInstance::verifyMetaOp()
{
    de::MovePtr<tcu::TextureLevel> resultLevel = readImage(m_destination->get(), m_params.dst.image);
    tcu::PixelBufferAccess resultAccess        = resultLevel->getAccess();
    return checkTestResult(resultAccess);
}

tcu::TestStatus DynamicStateMetaOpsInstance::iterate(void)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_device;

    uint32_t drawCount = 0;

    initMetaOp();
    initDraw();

    VkQueue queue                    = VK_NULL_HANDLE;
    VkCommandBuffer cmdbuf           = VK_NULL_HANDLE;
    VkCommandPool cmdpool            = VK_NULL_HANDLE;
    std::tie(queue, cmdbuf, cmdpool) = activeExecutionCtx();

    beginCommandBuffer(vkd, cmdbuf);

    // Draw to multisampled
    doDraw(cmdbuf, drawCount);
    drawCount++;

    // Copy/blit
    if (m_dynStateMetaOpsParams.metaOp == MetaOperation::META_OP_COPY)
        doCopy(cmdbuf);
    else
        doBlit(cmdbuf);

    // Draw to multisampled
    doDraw(cmdbuf, drawCount);
    drawCount++;

    endCommandBuffer(vkd, cmdbuf);
    submitCommandsAndWait(vkd, device, queue, cmdbuf);

    // Check result of meta op (destination) and draws (multisampled image)
    tcu::TestStatus metaOpStatus = verifyMetaOp();
    if (metaOpStatus.isFail())
        return metaOpStatus;

    tcu::TestStatus drawsStatus = verifyDraws();
    if (drawsStatus.isFail())
        return drawsStatus;

    return tcu::TestStatus::pass("Pass");
}

class DynamicStateMetaOpsTestCase : public vkt::TestCase
{
public:
    DynamicStateMetaOpsTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params,
                                const DynamicStateMetaOpsTestParams metaOpParams)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
        , m_dynStateMetaOpParams(metaOpParams)
    {
        DE_ASSERT(m_params.src.image.format == m_params.dst.image.format);
        DE_ASSERT(m_params.src.image.imageType == VK_IMAGE_TYPE_2D);
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
        return new DynamicStateMetaOpsInstance(context, m_params, m_dynStateMetaOpParams);
    }

    virtual void checkSupport(Context &context) const
    {
#ifndef CTS_USES_VULKANSC
        if (!context.getExtendedDynamicState3FeaturesEXT().extendedDynamicState3RasterizationSamples)
            TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#else
        DE_UNREF(context);
        TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#endif // CTS_USES_VULKANSC

        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

        VkImageFormatProperties properties;
        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.src.image.format, m_params.src.image.imageType,
                m_params.src.image.tiling, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, getCreateFlags(m_params.src.image),
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Source format not supported");
        }

        if (context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                context.getPhysicalDevice(), m_params.dst.image.format, m_params.dst.image.imageType,
                m_params.dst.image.tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT, getCreateFlags(m_params.src.image),
                &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Destination format not supported");
        }

        if (m_dynStateMetaOpParams.metaOp == MetaOperation::META_OP_BLIT)
        {
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
        }

        checkExtensionSupport(context, m_params.extensionFlags);

        // Check maxImageDimension2D
        {
            const VkPhysicalDeviceLimits &limits = context.getDeviceProperties().limits;

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

        // Check multisampled image
        {
            const InstanceInterface &vki          = context.getInstanceInterface();
            const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

            const VkImageUsageFlags msImageUsage =
                (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            const VkSampleCountFlagBits sampleCount = m_dynStateMetaOpParams.multisampledImageSampleCount;

            {
                VkImageFormatProperties msImageFormatProperties;
                const VkResult msImageFormatResult = vki.getPhysicalDeviceImageFormatProperties(
                    physicalDevice, m_dynStateMetaOpParams.multisampledImageFormat, VK_IMAGE_TYPE_2D,
                    VK_IMAGE_TILING_OPTIMAL, msImageUsage, (VkImageCreateFlags)0, &msImageFormatProperties);

                if (msImageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
                    TCU_THROW(NotSupportedError, "Image format is not supported");

                if ((msImageFormatProperties.sampleCounts & sampleCount) != sampleCount)
                    TCU_THROW(NotSupportedError, "Requested sample count is not supported");
            }
        }
    }

    virtual void initPrograms(SourceCollections &programCollection) const
    {
        std::ostringstream vert;
        {
            vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "layout(location=0) in vec4 inPosition;\n"
                 << "\n"
                 << "void main() {\n"
                 << "    gl_Position = inPosition;\n"
                 << "}\n";
        }
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::ostringstream frag;
        {
            frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "layout(location = 0) out vec4 outColor;\n"
                 << "\n"
                 << "layout(push_constant) uniform PushConsts {\n"
                 << "    int drawCount;\n"
                 << "    int width;\n"
                 << "    int height;\n"
                 << "    int numSamples;\n"
                 << "} pc;\n"
                 << "\n"
                 << "void main()\n"
                 << "{\n"
                 << "    int s = gl_SampleID;\n"
                 << "    if (((pc.drawCount == 0) && ((s % 2) == 0)) || ((pc.drawCount != 0) && ((s % 2) != 0))) {\n"
                 << "\n"
                 << "        float R = float(int(gl_FragCoord.x) + s) / float(pc.width + pc.numSamples);\n"
                 << "        float G = float(int(gl_FragCoord.y) + s) / float(pc.height + pc.numSamples);\n"
                 << "        float B = (pc.numSamples > 1) ? float(s) / float(pc.numSamples - 1) : 0.0f;\n"
                 << "        float A = 1.0f;\n"
                 << "\n"
                 << "        outColor = vec4(R, G, B, A);\n"
                 << "    }\n else outColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);"
                 << "}\n";
        }
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

        std::ostringstream fragVerify;
        {
            fragVerify << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                       << "\n"
                       << "layout(push_constant) uniform PushConsts {\n"
                       << "    int width;\n"
                       << "    int height;\n"
                       << "    int numSamples;\n"
                       << "} pc;\n"
                       << "layout(set=0, binding=0) buffer Results {\n"
                       << "    vec4 resultFlags[];\n"
                       << "};\n"
                       << "layout(set=0, binding=1) buffer Expects {\n"
                       << "    vec4 expectedFlags[];\n"
                       << "};\n"
                       << "layout(input_attachment_index=0, set=1, binding=0) uniform subpassInputMS msImageAtt;\n"
                       << "\n"
                       << "void main() {\n"
                       << "    for (int s = 0; s < pc.numSamples; ++s) {\n"
                       << "        vec4 resValue = subpassLoad(msImageAtt, s);\n"
                       << "\n"
                       << "        float R = float(int(gl_FragCoord.x) + s) / float(pc.width + pc.numSamples);\n"
                       << "        float G = float(int(gl_FragCoord.y) + s) / float(pc.height + pc.numSamples);\n"
                       << "        float B = (pc.numSamples > 1) ? float(s) / float(pc.numSamples - 1) : 0.0f;\n"
                       << "        float A = 1.0f;\n"
                       << "        vec4 expectedValue = vec4(R, G, B, A);\n"
                       << "\n"
                       << "        ivec3 coords  = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), s);\n"
                       << "        int bufferPos = (coords.y * pc.width + coords.x) * pc.numSamples + coords.z;\n"
                       // << "        resultFlags[bufferPos] = ((resValue == expectedValue) ? 1 : 0); \n"
                       << "        expectedFlags[bufferPos] = expectedValue; \n"
                       << "        resultFlags[bufferPos] = resValue; \n"
                       << "    }\n"
                       << "}\n";
        }
        programCollection.glslSources.add("fragVerify") << glu::FragmentSource(fragVerify.str());
    }

private:
    TestParams m_params;
    const DynamicStateMetaOpsTestParams m_dynStateMetaOpParams;
};

} // namespace

#ifndef CTS_USES_VULKANSC

tcu::TestCaseGroup *createDynamicStateMetaOperationsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> metaOpsGroup(new tcu::TestCaseGroup(testCtx, "dynamic_state"));

    TestParams copyParams;
    {
        copyParams.src.image.imageType       = VK_IMAGE_TYPE_2D;
        copyParams.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        copyParams.src.image.extent          = defaultExtent;
        copyParams.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        copyParams.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyParams.src.image.fillMode        = FILL_MODE_RED;
        copyParams.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        copyParams.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        copyParams.dst.image.extent          = defaultExtent;
        copyParams.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        copyParams.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyParams.dst.image.fillMode        = FILL_MODE_BLACK;
        copyParams.allocationKind            = ALLOCATION_KIND_SUBALLOCATED;

        // Whole image
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
            copyParams.regions.push_back(imageCopy);
        }
    }

    TestParams blitParams;
    {
        blitParams.src.image.imageType       = VK_IMAGE_TYPE_2D;
        blitParams.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        blitParams.src.image.extent          = defaultExtent;
        blitParams.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        blitParams.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitParams.src.image.fillMode        = FILL_MODE_RED;
        blitParams.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        blitParams.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        blitParams.dst.image.extent          = defaultExtent;
        blitParams.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
        blitParams.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitParams.dst.image.fillMode        = FILL_MODE_BLACK;
        blitParams.allocationKind            = ALLOCATION_KIND_SUBALLOCATED;

        // Whole image
        {
            const VkImageBlit imageBlit = {
                defaultSourceLayer,                          // VkImageSubresourceLayers srcSubresource;
                {{0, 0, 0}, {defaultSize, defaultSize, 1u}}, // VkOffset3D srcOffsets[2];

                defaultSourceLayer,                         // VkImageSubresourceLayers dstSubresource;
                {{0, 0, 0}, {defaultSize, defaultSize, 1u}} // VkOffset3D dstOffset[2];
            };

            CopyRegion region;
            region.imageBlit = imageBlit;
            blitParams.regions.push_back(region);
        }
    }

    const struct
    {
        const char *name;
        const TestParams params;

    } metaOpsParams[]               = {{"copy", copyParams}, {"blit", blitParams}};
    const VkFormat msImageFormats[] = {VK_FORMAT_R8G8B8A8_UNORM};

    const VkSampleCountFlagBits msImageSampleCounts[] = {
        VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT,
    };

    for (uint32_t metaOpIdx = 0u; metaOpIdx < uint32_t(MetaOperation::META_OP_NONE); metaOpIdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> metaOpGroup(new tcu::TestCaseGroup(testCtx, metaOpsParams[metaOpIdx].name));

        for (uint32_t fmtIdx = 0; fmtIdx < DE_LENGTH_OF_ARRAY(msImageFormats); fmtIdx++)
        {
            for (uint32_t sampleCountIdx = 0; sampleCountIdx < DE_LENGTH_OF_ARRAY(msImageSampleCounts);
                 sampleCountIdx++)
            {
                DynamicStateMetaOpsTestParams dynStateMetaOpsTestParams = {
                    MetaOperation(metaOpIdx), msImageFormats[fmtIdx], msImageSampleCounts[sampleCountIdx]};

                const std::string testName =
                    "draw_multisampled_image_" +
                    de::toLower(
                        std::string(getFormatName(dynStateMetaOpsTestParams.multisampledImageFormat)).substr(10)) +
                    "_samples_" +
                    de::toString(static_cast<int>(dynStateMetaOpsTestParams.multisampledImageSampleCount));
                metaOpGroup->addChild(new DynamicStateMetaOpsTestCase(
                    testCtx, testName, metaOpsParams[metaOpIdx].params, dynStateMetaOpsTestParams));
            }
        }

        metaOpsGroup->addChild(metaOpGroup.release());
    }

    return metaOpsGroup.release();
}
#endif // CTS_USES_VULKANSC

} // namespace api
} // namespace vkt
