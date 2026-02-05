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
 * \brief Vulkan Copy Depth Stencil MSAA Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopyDepthStencilMSAATests.hpp"

namespace vkt
{

namespace api
{

namespace
{

class DepthStencilMSAA : public vkt::TestInstance
{
public:
    enum CopyOptions
    {
        COPY_WHOLE_IMAGE,
        COPY_ARRAY_TO_ARRAY,
        COPY_PARTIAL
    };

    struct TestParameters
    {
        AllocationKind allocationKind;
        uint32_t extensionFlags;
        CopyOptions copyOptions;
        VkSampleCountFlagBits samples;
        VkImageLayout srcImageLayout;
        VkImageLayout dstImageLayout;
        VkFormat imageFormat;
        VkImageAspectFlags copyAspect;
        bool imageOffset;
    };

    DepthStencilMSAA(Context &context, TestParameters testParameters);
    tcu::TestStatus iterate(void) override;

protected:
    tcu::TestStatus checkCopyResults(VkCommandBuffer cmdBuffer, const VkImageAspectFlagBits &aspectToVerify,
                                     VkImage srcImage, VkImage dstImage);

private:
    // Returns image aspects used in the copy regions.
    VkImageAspectFlags getUsedImageAspects()
    {
        auto aspectFlags = (VkImageAspectFlags)0;
        for (const auto &region : m_regions)
        {
            aspectFlags |= region.imageCopy.srcSubresource.aspectMask;
        }
        return aspectFlags;
    }

    ImageParms m_srcImage;
    ImageParms m_dstImage;
    std::vector<CopyRegion> m_regions;
    const TestParameters m_params;
    const float m_clearValue = 0.0f;
};

DepthStencilMSAA::DepthStencilMSAA(Context &context, TestParameters testParameters)
    : vkt::TestInstance(context)
    , m_params(testParameters)
{
    // params.src.image is the parameters used to create the copy source image
    m_srcImage.imageType       = VK_IMAGE_TYPE_2D;
    m_srcImage.format          = testParameters.imageFormat;
    m_srcImage.extent          = defaultExtent;
    m_srcImage.tiling          = VK_IMAGE_TILING_OPTIMAL;
    m_srcImage.operationLayout = testParameters.srcImageLayout;
    m_srcImage.createFlags     = 0u;

    // params.src.image is the parameters used to create the copy destination image
    m_dstImage.imageType       = VK_IMAGE_TYPE_2D;
    m_dstImage.format          = testParameters.imageFormat;
    m_dstImage.extent          = defaultExtent;
    m_dstImage.tiling          = VK_IMAGE_TILING_OPTIMAL;
    m_dstImage.operationLayout = testParameters.dstImageLayout;
    m_dstImage.createFlags     = 0u;

    const VkImageSubresourceLayers depthSubresourceLayers = {
        VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
        0u,                        // uint32_t mipLevel;
        0u,                        // uint32_t baseArrayLayer;
        1u                         // uint32_t layerCount;
    };

    const VkImageSubresourceLayers stencilSubresourceLayers = {
        VK_IMAGE_ASPECT_STENCIL_BIT, // VkImageAspectFlags aspectMask;
        0u,                          // uint32_t mipLevel;
        0u,                          // uint32_t baseArrayLayer;
        1u                           // uint32_t layerCount;
    };

    VkImageCopy depthCopy = {
        depthSubresourceLayers, // VkImageSubresourceLayers srcSubresource;
        {0, 0, 0},              // VkOffset3D srcOffset;
        depthSubresourceLayers, // VkImageSubresourceLayers dstSubresource;
        {0, 0, 0},              // VkOffset3D dstOffset;
        defaultExtent,          // VkExtent3D extent;
    };

    VkImageCopy stencilCopy = {
        stencilSubresourceLayers, // VkImageSubresourceLayers srcSubresource;
        {0, 0, 0},                // VkOffset3D srcOffset;
        stencilSubresourceLayers, // VkImageSubresourceLayers dstSubresource;
        {0, 0, 0},                // VkOffset3D dstOffset;
        defaultExtent,            // VkExtent3D extent;
    };

    if (testParameters.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY)
    {
        m_srcImage.extent.depth                   = 5u;
        depthCopy.srcSubresource.baseArrayLayer   = 2u;
        depthCopy.dstSubresource.baseArrayLayer   = 3u;
        stencilCopy.srcSubresource.baseArrayLayer = 2u;
        stencilCopy.dstSubresource.baseArrayLayer = 3u;
    }

    CopyRegion depthCopyRegion;
    CopyRegion stencilCopyRegion;
    depthCopyRegion.imageCopy   = depthCopy;
    stencilCopyRegion.imageCopy = stencilCopy;

    std::vector<CopyRegion> depthRegions;
    std::vector<CopyRegion> stencilRegions;

    if (testParameters.copyOptions == DepthStencilMSAA::COPY_PARTIAL)
    {
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            depthCopyRegion.imageCopy.extent = {defaultHalfSize, defaultHalfSize, 1};
            // Copy region from bottom right to bottom left
            depthCopyRegion.imageCopy.srcOffset = {defaultHalfSize, defaultHalfSize, 0};
            depthCopyRegion.imageCopy.dstOffset = {0, defaultHalfSize, 0};
            depthRegions.push_back(depthCopyRegion);
            // Copy region from top right to bottom right
            depthCopyRegion.imageCopy.srcOffset = {defaultHalfSize, 0, 0};
            depthCopyRegion.imageCopy.dstOffset = {defaultHalfSize, defaultHalfSize, 0};
            depthRegions.push_back(depthCopyRegion);
        }
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
        {
            stencilCopyRegion.imageCopy.extent = {defaultHalfSize, defaultHalfSize, 1};
            // Copy region from bottom right to bottom left
            stencilCopyRegion.imageCopy.srcOffset = {defaultHalfSize, defaultHalfSize, 0};
            stencilCopyRegion.imageCopy.dstOffset = {0, defaultHalfSize, 0};
            stencilRegions.push_back(stencilCopyRegion);
            // Copy region from top right to bottom right
            stencilCopyRegion.imageCopy.srcOffset = {defaultHalfSize, 0, 0};
            stencilCopyRegion.imageCopy.dstOffset = {defaultHalfSize, defaultHalfSize, 0};
            stencilRegions.push_back(stencilCopyRegion);
        }
    }
    else
    {
        // Copy the default region (full image)
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
        {
            depthRegions.push_back(depthCopyRegion);
        }
        if (testParameters.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
        {
            stencilRegions.push_back(stencilCopyRegion);
        }
    }

    if (testParameters.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        m_regions.insert(m_regions.end(), depthRegions.begin(), depthRegions.end());
    }

    if (testParameters.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        m_regions.insert(m_regions.end(), stencilRegions.begin(), stencilRegions.end());
    }
}

tcu::TestStatus DepthStencilMSAA::iterate(void)
{
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const InstanceInterface &vki        = m_context.getInstanceInterface();
    const VkDevice vkDevice             = m_context.getDevice();
    const VkPhysicalDevice vkPhysDevice = m_context.getPhysicalDevice();
    const VkQueue queue                 = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    Allocator &memAlloc                 = m_context.getDefaultAllocator();
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_srcImage.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(m_dstImage.format);
    VkImageAspectFlags aspectFlags        = getUsedImageAspects();
    uint32_t sourceArraySize              = getArraySize(m_srcImage);

    Move<VkImage> srcImage;
    de::MovePtr<Allocation> srcImageAlloc;
    Move<VkImage> dstImage;
    de::MovePtr<Allocation> dstImageAlloc;

    // 1. Create the images and draw a triangle to the source image.
    {
        const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                         VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        Move<VkShaderModule> vertexShaderModule =
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
        Move<VkShaderModule> fragmentShaderModule =
            createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
        std::vector<tcu::Vec4> vertices;
        Move<VkBuffer> vertexBuffer;
        de::MovePtr<Allocation> vertexBufferAlloc;
        Move<VkPipelineLayout> pipelineLayout;
        Move<VkPipeline> graphicsPipeline;
        Move<VkRenderPass> renderPass;

        // Create multisampled depth/stencil image (srcImage) and the copy destination image (dstImage).
        {
            VkImageCreateInfo multiSampledImageParams = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,        // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                getCreateFlags(m_srcImage),                 // VkImageCreateFlags flags;
                m_srcImage.imageType,                       // VkImageType imageType;
                m_srcImage.format,                          // VkFormat format;
                getExtent3D(m_srcImage),                    // VkExtent3D extent;
                1u,                                         // uint32_t mipLevels;
                getArraySize(m_srcImage),                   // uint32_t arrayLayers;
                m_params.samples,                           // VkSampleCountFlagBits samples;
                VK_IMAGE_TILING_OPTIMAL,                    // VkImageTiling tiling;
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT // VkImageUsageFlags usage;
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL |
                    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
                1u,                        // uint32_t queueFamilyIndexCount;
                &queueFamilyIndex,         // const uint32_t* pQueueFamilyIndices;
                VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
            };

            srcImage = createImage(vk, vkDevice, &multiSampledImageParams);

            VkMemoryRequirements req = getImageMemoryRequirements(vk, vkDevice, *srcImage);
            uint32_t offset          = m_params.imageOffset ? static_cast<uint32_t>(req.alignment) : 0;

            srcImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, srcImage.get(), MemoryRequirement::Any,
                                          memAlloc, m_params.allocationKind, offset);
            VK_CHECK(vk.bindImageMemory(vkDevice, srcImage.get(), srcImageAlloc->getMemory(),
                                        srcImageAlloc->getOffset() + offset));

            dstImage      = createImage(vk, vkDevice, &multiSampledImageParams);
            dstImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, dstImage.get(), MemoryRequirement::Any,
                                          memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(
                vk.bindImageMemory(vkDevice, dstImage.get(), dstImageAlloc->getMemory(), dstImageAlloc->getOffset()));
        }

        // Create render pass.
        {
            const VkImageLayout initialLayout                   = m_params.copyOptions == COPY_ARRAY_TO_ARRAY ?
                                                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                                                      VK_IMAGE_LAYOUT_UNDEFINED;
            const VkAttachmentDescription attachmentDescription = {
                0u,                                              // VkAttachmentDescriptionFlags        flags
                m_srcImage.format,                               // VkFormat                            format
                m_params.samples,                                // VkSampleCountFlagBits            samples
                VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                loadOp
                VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                storeOp
                VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                stencilLoadOp
                VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                stencilStoreOp
                initialLayout,                                   // VkImageLayout                    initialLayout
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
            };

            const VkAttachmentReference attachmentReference = {
                0u,                                              // uint32_t            attachment
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout    layout
            };

            const VkSubpassDescription subpassDescription = {
                0u,                              // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint            pipelineBindPoint
                0u,                              // uint32_t                        inputAttachmentCount
                nullptr,                         // const VkAttachmentReference*    pInputAttachments
                0u,                              // uint32_t                        colorAttachmentCount
                nullptr,                         // const VkAttachmentReference*    pColorAttachments
                nullptr,                         // const VkAttachmentReference*    pResolveAttachments
                &attachmentReference,            // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                              // uint32_t                        preserveAttachmentCount
                nullptr                          // const VkAttachmentReference*    pPreserveAttachments
            };

            const VkRenderPassCreateInfo renderPassParams = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
                nullptr,                                   // const void* pNext;
                0u,                                        // VkRenderPassCreateFlags flags;
                1u,                                        // uint32_t attachmentCount;
                &attachmentDescription,                    // const VkAttachmentDescription* pAttachments;
                1u,                                        // uint32_t subpassCount;
                &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
                0u,                                        // uint32_t dependencyCount;
                nullptr                                    // const VkSubpassDependency* pDependencies;
            };

            renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
        }

        // Create pipeline layout
        {
            const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                0u,                                            // VkPipelineLayoutCreateFlags flags;
                0u,                                            // uint32_t setLayoutCount;
                nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
                0u,                                            // uint32_t pushConstantRangeCount;
                nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
            };

            pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
        }

        // Create upper half triangle.
        {
            // Add triangle.
            vertices.emplace_back(-1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(1.0f, -1.0f, 0.0f, 1.0f);
            vertices.emplace_back(1.0f, 1.0f, 0.0f, 1.0f);
        }

        // Create vertex buffer.
        {
            const VkDeviceSize vertexDataSize           = vertices.size() * sizeof(tcu::Vec4);
            const VkBufferCreateInfo vertexBufferParams = {
                VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
                nullptr,                              // const void* pNext;
                0u,                                   // VkBufferCreateFlags flags;
                vertexDataSize,                       // VkDeviceSize size;
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
                VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
                1u,                                   // uint32_t queueFamilyIndexCount;
                &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
            };

            vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
            vertexBufferAlloc = allocateBuffer(vki, vk, vkPhysDevice, vkDevice, *vertexBuffer,
                                               MemoryRequirement::HostVisible, memAlloc, m_params.allocationKind);
            VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexBufferAlloc->getMemory(),
                                         vertexBufferAlloc->getOffset()));

            // Load vertices into vertex buffer.
            deMemcpy(vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexDataSize);
            flushAlloc(vk, vkDevice, *vertexBufferAlloc);
        }

        {
            Move<VkFramebuffer> framebuffer;
            Move<VkImageView> sourceAttachmentView;

            // Create depth/stencil attachment view.
            {
                const uint32_t arrayLayer = m_params.copyOptions == COPY_ARRAY_TO_ARRAY ? 2u : 0u;
                const VkImageViewCreateInfo depthStencilAttachmentViewParams = {
                    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                  // const void* pNext;
                    0u,                                       // VkImageViewCreateFlags flags;
                    *srcImage,                                // VkImage image;
                    VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                    m_srcImage.format,                        // VkFormat format;
                    componentMappingRGBA,                     // VkComponentMapping components;
                    {aspectFlags, 0u, 1u, arrayLayer, 1u}     // VkImageSubresourceRange subresourceRange;
                };
                sourceAttachmentView = createImageView(vk, vkDevice, &depthStencilAttachmentViewParams);
            }

            // Create framebuffer
            {
                const VkFramebufferCreateInfo framebufferParams = {
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                   // const void* pNext;
                    0u,                                        // VkFramebufferCreateFlags flags;
                    *renderPass,                               // VkRenderPass renderPass;
                    1u,                                        // uint32_t attachmentCount;
                    &sourceAttachmentView.get(),               // const VkImageView* pAttachments;
                    m_srcImage.extent.width,                   // uint32_t width;
                    m_srcImage.extent.height,                  // uint32_t height;
                    1u                                         // uint32_t layers;
                };

                framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
            }

            // Create pipeline
            {
                const std::vector<VkViewport> viewports(1, makeViewport(m_srcImage.extent));
                const std::vector<VkRect2D> scissors(1, makeRect2D(m_srcImage.extent));

                const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                                  // const void* pNext;
                    0u,               // VkPipelineMultisampleStateCreateFlags flags;
                    m_params.samples, // VkSampleCountFlagBits rasterizationSamples;
                    VK_FALSE,         // VkBool32 sampleShadingEnable;
                    0.0f,             // float minSampleShading;
                    nullptr,          // const VkSampleMask* pSampleMask;
                    VK_FALSE,         // VkBool32 alphaToCoverageEnable;
                    VK_FALSE          // VkBool32 alphaToOneEnable;
                };

                const VkStencilOpState stencilOpState = {
                    VK_STENCIL_OP_KEEP,    // VkStencilOp    failOp
                    VK_STENCIL_OP_REPLACE, // VkStencilOp    passOp
                    VK_STENCIL_OP_KEEP,    // VkStencilOp    depthFailOp
                    VK_COMPARE_OP_ALWAYS,  // VkCompareOp    compareOp
                    0,                     // uint32_t        compareMask
                    0xFF,                  // uint32_t        writeMask
                    0xFF                   // uint32_t        reference
                };

                const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault = {
                    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                            sType
                    nullptr, // const void*                                pNext
                    0u,      // VkPipelineDepthStencilStateCreateFlags    flags
                    aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ?
                        VK_TRUE :
                        VK_FALSE, // VkBool32                                    depthTestEnable
                    aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ?
                        VK_TRUE :
                        VK_FALSE,         // VkBool32                                    depthWriteEnable
                    VK_COMPARE_OP_ALWAYS, // VkCompareOp                                depthCompareOp
                    VK_FALSE,             // VkBool32                                    depthBoundsTestEnable
                    aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT ?
                        VK_TRUE :
                        VK_FALSE,   // VkBool32                                    stencilTestEnable
                    stencilOpState, // VkStencilOpState                            front
                    stencilOpState, // VkStencilOpState                            back
                    0.0f,           // float                                    minDepthBounds
                    1.0f,           // float                                    maxDepthBounds
                };

                graphicsPipeline = makeGraphicsPipeline(
                    vk,                  // const DeviceInterface&                            vk
                    vkDevice,            // const VkDevice                                    device
                    *pipelineLayout,     // const VkPipelineLayout                            pipelineLayout
                    *vertexShaderModule, // const VkShaderModule                                vertexShaderModule
                    VK_NULL_HANDLE, // const VkShaderModule                                tessellationControlModule
                    VK_NULL_HANDLE, // const VkShaderModule                                tessellationEvalModule
                    VK_NULL_HANDLE, // const VkShaderModule                                geometryShaderModule
                    *fragmentShaderModule, // const VkShaderModule                                fragmentShaderModule
                    *renderPass,           // const VkRenderPass                                renderPass
                    viewports,             // const std::vector<VkViewport>&                    viewports
                    scissors,              // const std::vector<VkRect2D>&                        scissors
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
                    0u,                                  // const uint32_t                                    subpass
                    0u,      // const uint32_t                                    patchControlPoints
                    nullptr, // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
                    nullptr, // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
                    &multisampleStateParams, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
                    &depthStencilStateCreateInfoDefault); // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            }

            // Create command buffer
            {
                beginCommandBuffer(vk, *cmdBuffer, 0u);

                const VkClearValue srcImageClearValue = makeClearValueDepthStencil(0.1f, 0x10);

                // Change the layout of each layer of the depth / stencil image to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL and clear the images.
                const VkClearValue copiedImageClearValue =
                    makeClearValueDepthStencil(m_clearValue, (uint32_t)m_clearValue);
                const auto subResourceRange = makeImageSubresourceRange( // VkImageSubresourceRange    subresourceRange
                    (getAspectFlags(m_srcImage.format)),                 // VkImageAspectFlags        aspectMask
                    0u,                                                  // uint32_t                    baseMipLevel
                    1u,                                                  // uint32_t                    levelCount
                    0u,                                                  // uint32_t                    baseArrayLayer
                    getArraySize(m_srcImage));

                const VkImageMemoryBarrier preClearBarrier =
                    makeImageMemoryBarrier(0u,                           // VkAccessFlags            srcAccessMask
                                           VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags            dstAccessMask
                                           VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout            oldLayout
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout            newLayout
                                           srcImage.get(),                       // VkImage                    image
                                           subResourceRange); // VkImageSubresourceRange    subresourceRange
                std::vector<VkImageMemoryBarrier> preClearBarriers(2u, preClearBarrier);
                preClearBarriers[1].image = dstImage.get();
                vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 2, preClearBarriers.data());

                vk.cmdClearDepthStencilImage(
                    *cmdBuffer,                           // VkCommandBuffer                    commandBuffer
                    srcImage.get(),                       // VkImage                            image
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout                    imageLayout
                    &srcImageClearValue.depthStencil,     // const VkClearDepthStencilValue*    pDepthStencil
                    1u,                                   // uint32_t                            rangeCount
                    &subResourceRange);                   // const VkImageSubresourceRange*    pRanges

                vk.cmdClearDepthStencilImage(
                    *cmdBuffer,                           // VkCommandBuffer                    commandBuffer
                    dstImage.get(),                       // VkImage                            image
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout                    imageLayout
                    &copiedImageClearValue.depthStencil,  // const VkClearDepthStencilValue*    pDepthStencil
                    1u,                                   // uint32_t                            rangeCount
                    &subResourceRange);                   // const VkImageSubresourceRange*    pRanges

                // Post clear barrier
                const auto dstAccess = static_cast<VkAccessFlags>(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
                const auto dstStages = static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                                                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

                const VkImageMemoryBarrier postClearBarrier = makeImageMemoryBarrier(
                    VK_ACCESS_TRANSFER_WRITE_BIT,                     // VkAccessFlags            srcAccessMask
                    dstAccess,                                        // VkAccessFlags            dstAccessMask
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,             // VkImageLayout            oldLayout
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            newLayout
                    srcImage.get(),                                   // VkImage                    image
                    subResourceRange);                                // VkImageSubresourceRange    subresourceRange

                vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStages, (VkDependencyFlags)0, 0,
                                      nullptr, 0, nullptr, 1, &postClearBarrier);

                beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
                                makeRect2D(0, 0, m_srcImage.extent.width, m_srcImage.extent.height), 1u,
                                &srcImageClearValue);

                const VkDeviceSize vertexBufferOffset = 0u;

                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
                vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
                vk.cmdDraw(*cmdBuffer, (uint32_t)vertices.size(), 1, 0, 0);

                endRenderPass(vk, *cmdBuffer);
                endCommandBuffer(vk, *cmdBuffer);
            }

            submitCommandsAndWaitWithSync(vk, vkDevice, queue, *cmdBuffer);
            m_context.resetCommandPoolForVKSC(vkDevice, *cmdPool);
        }
    }

    // 2. Record a command buffer that contains the copy operation(s).
    beginCommandBuffer(vk, *cmdBuffer);
    {
        // Change the image layouts and synchronize the memory access before copying
        {
            const VkImageMemoryBarrier imageBarriers[] = {
                // srcImage
                makeImageMemoryBarrier(
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            srcAccessMask
                    VK_ACCESS_TRANSFER_READ_BIT,                      // VkAccessFlags            dstAccessMask
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout            oldLayout
                    m_srcImage.operationLayout,                       // VkImageLayout            newLayout
                    srcImage.get(),                                   // VkImage                    image
                    makeImageSubresourceRange(                        // VkImageSubresourceRange    subresourceRange
                        getAspectFlags(srcTcuFormat),                 // VkImageAspectFlags    aspectMask
                        0u,                                           // uint32_t                baseMipLevel
                        1u,                                           // uint32_t                levelCount
                        0u,                                           // uint32_t                baseArrayLayer
                        sourceArraySize                               // uint32_t                layerCount
                        )),
                // dstImage
                makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags            srcAccessMask
                                       VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags            dstAccessMask
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout            oldLayout
                                       m_dstImage.operationLayout,           // VkImageLayout            newLayout
                                       dstImage.get(),                       // VkImage                    image
                                       makeImageSubresourceRange(        // VkImageSubresourceRange    subresourceRange
                                           getAspectFlags(dstTcuFormat), // VkImageAspectFlags    aspectMask
                                           0u,                           // uint32_t                baseMipLevel
                                           1u,                           // uint32_t                levelCount
                                           0u,                           // uint32_t                baseArrayLayer
                                           sourceArraySize               // uint32_t                layerCount
                                           )),
            };
            vk.cmdPipelineBarrier(
                *cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 2u, imageBarriers);
        }

        std::vector<VkImageCopy> imageCopies;
        std::vector<VkImageCopy2KHR> imageCopies2KHR;
        for (const auto &region : m_regions)
        {
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageCopies.push_back(region.imageCopy);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageCopies2KHR.push_back(convertvkImageCopyTovkImageCopy2KHR(region.imageCopy));
            }
        }

        if (!(m_params.extensionFlags & COPY_COMMANDS_2))
        {
            vk.cmdCopyImage(*cmdBuffer, srcImage.get(), m_srcImage.operationLayout, dstImage.get(),
                            m_dstImage.operationLayout, (uint32_t)imageCopies.size(), imageCopies.data());
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            const VkCopyImageInfo2KHR copyImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
                nullptr,                                 // const void* pNext;
                srcImage.get(),                          // VkImage srcImage;
                m_srcImage.operationLayout,              // VkImageLayout srcImageLayout;
                dstImage.get(),                          // VkImage dstImage;
                m_dstImage.operationLayout,              // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
                imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
            };

            vk.cmdCopyImage2(*cmdBuffer, &copyImageInfo2KHR);
        }
    }
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWaitWithSync(vk, vkDevice, queue, *cmdBuffer);
    m_context.resetCommandPoolForVKSC(vkDevice, *cmdPool);

    // Verify that all samples have been copied properly from all aspects.
    const auto usedImageAspects = getUsedImageAspects();
    if (usedImageAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        auto copyResult = checkCopyResults(cmdBuffer.get(), VK_IMAGE_ASPECT_DEPTH_BIT, srcImage.get(), dstImage.get());
        if (copyResult.getCode() != QP_TEST_RESULT_PASS)
            return copyResult;
    }
    if (usedImageAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        auto copyResult =
            checkCopyResults(cmdBuffer.get(), VK_IMAGE_ASPECT_STENCIL_BIT, srcImage.get(), dstImage.get());
        if (copyResult.getCode() != QP_TEST_RESULT_PASS)
            return copyResult;
    }
    return tcu::TestStatus::pass("pass");
}

tcu::TestStatus DepthStencilMSAA::checkCopyResults(VkCommandBuffer cmdBuffer,
                                                   const VkImageAspectFlagBits &aspectToVerify, VkImage srcImage,
                                                   VkImage dstImage)
{
    DE_ASSERT((aspectToVerify & VK_IMAGE_ASPECT_DEPTH_BIT) || (aspectToVerify & VK_IMAGE_ASPECT_STENCIL_BIT));

    const auto &vkd                = m_context.getDeviceInterface();
    const auto device              = m_context.getDevice();
    const auto queue               = m_context.getUniversalQueue();
    auto &alloc                    = m_context.getDefaultAllocator();
    const auto layerCount          = getArraySize(m_srcImage);
    const auto numInputAttachments = layerCount + 1u; // +1 for the source image.
    const auto numOutputBuffers    = 2u;              // 1 for the reference and 1 for the copied values.
    const auto numSets             = 2u;              // 1 for the output buffers, 1 for the input attachments.
    const auto fbWidth             = m_srcImage.extent.width;
    const auto fbHeight            = m_srcImage.extent.height;
    const auto aspectFlags         = getUsedImageAspects();

    // Shader modules.
    const auto vertexModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto verificationModule =
        createShaderModule(vkd, device,
                           m_context.getBinaryCollection().get(
                               aspectToVerify & VK_IMAGE_ASPECT_DEPTH_BIT ? "verify_depth" : "verify_stencil"),
                           0u);

    // Descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numOutputBuffers);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numInputAttachments);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);

    DescriptorSetLayoutBuilder layoutBuilderBuffer;
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilderBuffer.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto outputBufferSetLayout = layoutBuilderBuffer.build(vkd, device);

    DescriptorSetLayoutBuilder layoutBuilderAttachments;
    for (uint32_t i = 0u; i < numInputAttachments; ++i)
        layoutBuilderAttachments.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto inputAttachmentsSetLayout = layoutBuilderAttachments.build(vkd, device);

    const auto descriptorSetBuffer = makeDescriptorSet(vkd, device, descriptorPool.get(), outputBufferSetLayout.get());
    const auto descriptorSetAttachments =
        makeDescriptorSet(vkd, device, descriptorPool.get(), inputAttachmentsSetLayout.get());

    // Array with raw descriptor sets.
    const std::array<VkDescriptorSet, numSets> descriptorSets = {{
        descriptorSetBuffer.get(),
        descriptorSetAttachments.get(),
    }};

    // Pipeline layout.
    const std::array<VkDescriptorSetLayout, numSets> setLayouts = {{
        outputBufferSetLayout.get(),
        inputAttachmentsSetLayout.get(),
    }};

    // Push constants.
    std::array<int, 3> pushConstantData = {{
        static_cast<int>(fbWidth),
        static_cast<int>(fbHeight),
        static_cast<int>(m_params.samples),
    }};

    const auto pushConstantSize =
        static_cast<uint32_t>(pushConstantData.size() * sizeof(decltype(pushConstantData)::value_type));

    const VkPushConstantRange pushConstantRange = {
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        pushConstantSize,             // uint32_t size;
    };

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

    // Render pass.
    const VkAttachmentDescription commonAttachmentDescription = {
        0u,                                   // VkAttachmentDescriptionFlags flags;
        m_srcImage.format,                    // VkFormat format;
        m_params.samples,                     // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_LOAD,           // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,         // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,      // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,     // VkAttachmentStoreOp stencilStoreOp;
        m_dstImage.operationLayout,           // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // VkImageLayout finalLayout;
    };

    std::vector<VkAttachmentDescription> attachmentDescriptions(numInputAttachments, commonAttachmentDescription);
    // Set the first attachment's (m_srcImage) initial layout to match the layout it was left after copying.
    attachmentDescriptions[0].initialLayout = m_srcImage.operationLayout;

    std::vector<VkAttachmentReference> inputAttachmentReferences;
    inputAttachmentReferences.reserve(numInputAttachments);
    for (uint32_t i = 0u; i < numInputAttachments; ++i)
    {
        const VkAttachmentReference reference = {i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        inputAttachmentReferences.push_back(reference);
    }

    const VkSubpassDescription subpassDescription = {
        0u,                                                      // VkSubpassDescriptionFlags flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS,                         // VkPipelineBindPoint pipelineBindPoint;
        static_cast<uint32_t>(inputAttachmentReferences.size()), // uint32_t inputAttachmentCount;
        inputAttachmentReferences.data(),                        // const VkAttachmentReference* pInputAttachments;
        0u,                                                      // uint32_t colorAttachmentCount;
        nullptr,                                                 // const VkAttachmentReference* pColorAttachments;
        nullptr,                                                 // const VkAttachmentReference* pResolveAttachments;
        nullptr, // const VkAttachmentReference* pDepthStencilAttachment;
        0u,      // uint32_t preserveAttachmentCount;
        nullptr, // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        attachmentDescriptions.data(),                        // const VkAttachmentDescription* pAttachments;
        1u,                                                   // uint32_t subpassCount;
        &subpassDescription,                                  // const VkSubpassDescription* pSubpasses;
        0u,                                                   // uint32_t dependencyCount;
        nullptr,                                              // const VkSubpassDependency* pDependencies;
    };

    const auto renderPass = createRenderPass(vkd, device, &renderPassInfo);

    // Framebuffer.
    std::vector<Move<VkImageView>> imageViews;
    std::vector<VkImageView> imageViewsRaw;

    const uint32_t srcArrayLayer = m_params.copyOptions == COPY_ARRAY_TO_ARRAY ? 2u : 0u;
    imageViews.push_back(makeImageView(vkd, device, srcImage, VK_IMAGE_VIEW_TYPE_2D, m_srcImage.format,
                                       makeImageSubresourceRange(aspectFlags, 0u, 1u, srcArrayLayer, 1u)));
    for (uint32_t i = 0u; i < layerCount; ++i)
    {
        const auto subresourceRange = makeImageSubresourceRange(aspectFlags, 0u, 1u, i, 1u);
        imageViews.push_back(
            makeImageView(vkd, device, dstImage, VK_IMAGE_VIEW_TYPE_2D, m_srcImage.format, subresourceRange));
    }

    imageViewsRaw.reserve(imageViews.size());
    std::transform(begin(imageViews), end(imageViews), std::back_inserter(imageViewsRaw),
                   [](const Move<VkImageView> &ptr) { return ptr.get(); });

    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<uint32_t>(imageViewsRaw.size()),
                                             imageViewsRaw.data(), fbWidth, fbHeight);

    // Create storage buffers for both original and copied multisampled depth/stencil images.
    const auto bufferCount = static_cast<size_t>(fbWidth * fbHeight * m_params.samples);
    const auto bufferSize  = static_cast<VkDeviceSize>(bufferCount * sizeof(float));
    BufferWithMemory bufferOriginal(vkd, device, alloc,
                                    makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                    MemoryRequirement::HostVisible);
    BufferWithMemory bufferCopied(vkd, device, alloc,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);
    auto &bufferOriginalAlloc = bufferOriginal.getAllocation();
    auto &bufferCopiedAlloc   = bufferCopied.getAllocation();

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto bufferOriginalInfo = makeDescriptorBufferInfo(bufferOriginal.get(), 0ull, bufferSize);
    const auto bufferCopiedInfo   = makeDescriptorBufferInfo(bufferCopied.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferOriginalInfo);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(1u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferCopiedInfo);

    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(imageViewsRaw.size());
    for (size_t i = 0; i < imageViewsRaw.size(); ++i)
    {
        imageInfos.push_back(
            makeDescriptorImageInfo(VK_NULL_HANDLE, imageViewsRaw[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        updater.writeSingle(descriptorSetAttachments.get(),
                            DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i)),
                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[i]);
    }

    updater.update(vkd, device);

    // Vertex buffer.
    std::vector<tcu::Vec4> fullScreenQuad;
    {
        // Full screen quad so every framebuffer pixel and sample location is verified by the shader.
        const tcu::Vec4 topLeft(-1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 topRight(1.0f, -1.0f, 0.0f, 1.0f);
        const tcu::Vec4 bottomLeft(-1.0f, 1.0f, 0.0f, 1.0f);
        const tcu::Vec4 bottomRight(1.0f, 1.0f, 0.0f, 1.0f);

        fullScreenQuad.reserve(6u);
        fullScreenQuad.push_back(topLeft);
        fullScreenQuad.push_back(topRight);
        fullScreenQuad.push_back(bottomRight);
        fullScreenQuad.push_back(topLeft);
        fullScreenQuad.push_back(bottomRight);
        fullScreenQuad.push_back(bottomLeft);
    }

    const auto vertexBufferSize =
        static_cast<VkDeviceSize>(fullScreenQuad.size() * sizeof(decltype(fullScreenQuad)::value_type));
    const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const BufferWithMemory vertexBuffer(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
    const VkDeviceSize vertexBufferOffset = 0ull;

    deMemcpy(vertexBuffer.getAllocation().getHostPtr(), fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
    flushAlloc(vkd, device, vertexBuffer.getAllocation());

    // Graphics pipeline.
    const std::vector<VkViewport> viewports(1, makeViewport(m_srcImage.extent));
    const std::vector<VkRect2D> scissors(1, makeRect2D(m_srcImage.extent));

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
        vkd,                      // const DeviceInterface&                            vk
        device,                   // const VkDevice                                    device
        pipelineLayout.get(),     // const VkPipelineLayout                            pipelineLayout
        vertexModule.get(),       // const VkShaderModule                                vertexShaderModule
        VK_NULL_HANDLE,           // const VkShaderModule                                tessellationControlModule
        VK_NULL_HANDLE,           // const VkShaderModule                                tessellationEvalModule
        VK_NULL_HANDLE,           // const VkShaderModule                                geometryShaderModule
        verificationModule.get(), // const VkShaderModule                                fragmentShaderModule
        renderPass.get(),         // const VkRenderPass                                renderPass
        viewports,                // const std::vector<VkViewport>&                    viewports
        scissors,                 // const std::vector<VkRect2D>&                        scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                        topology
        0u,                                  // const uint32_t                                    subpass
        0u,                                  // const uint32_t                                    patchControlPoints
        nullptr,                  // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
        nullptr,                  // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
        &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo

    // Make sure multisample copy data is available to the fragment shader.
    const auto imagesBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

    // Record and submit command buffer.
    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u,
                           &imagesBarrier, 0u, nullptr, 0u, nullptr);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), makeRect2D(m_srcImage.extent));
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
    vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                         pushConstantData.data());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u,
                              static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);
    vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(fullScreenQuad.size()), 1u, 0u, 0u);

    endRenderPass(vkd, cmdBuffer);

    // Make sure verification buffer data is available on the host.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWaitWithSync(vkd, device, queue, cmdBuffer);

    // Verify intermediate results.
    invalidateAlloc(vkd, device, bufferOriginalAlloc);
    invalidateAlloc(vkd, device, bufferCopiedAlloc);
    std::vector<float> outputOriginal(bufferCount, 0);
    std::vector<float> outputCopied(bufferCount, 0);
    deMemcpy(outputOriginal.data(), bufferOriginalAlloc.getHostPtr(), static_cast<size_t>(bufferSize));
    deMemcpy(outputCopied.data(), bufferCopiedAlloc.getHostPtr(), static_cast<size_t>(bufferSize));

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Verifying intermediate multisample copy results" << tcu::TestLog::EndMessage;

    const auto sampleCount = static_cast<uint32_t>(m_params.samples);

    // Verify copied region(s)
    for (const auto &region : m_regions)
    {
        for (uint32_t x = 0u; x < region.imageCopy.extent.width; ++x)
            for (uint32_t y = 0u; y < region.imageCopy.extent.height; ++y)
                for (uint32_t s = 0u; s < sampleCount; ++s)
                {
                    tcu::UVec2 srcCoord(x + region.imageCopy.srcOffset.x, y + region.imageCopy.srcOffset.y);
                    tcu::UVec2 dstCoord(x + region.imageCopy.dstOffset.x, y + region.imageCopy.dstOffset.y);
                    const auto srcIndex = (srcCoord.y() * fbWidth + srcCoord.x()) * sampleCount + s;
                    const auto dstIndex = (dstCoord.y() * fbWidth + dstCoord.x()) * sampleCount + s;
                    if (outputOriginal[srcIndex] != outputCopied[dstIndex])
                    {
                        std::ostringstream msg;
                        msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample "
                            << s << ". "
                            << "result: " << outputCopied[dstIndex] << " expected: " << outputOriginal[srcIndex];
                        return tcu::TestStatus::fail(msg.str());
                    }
                }
    }

    if (m_params.copyOptions == COPY_PARTIAL)
    {
        // In the partial copy tests the destination image contains copied data only in the bottom half of the image.
        // Verify that the upper half of the image is left at it's clear value (0).
        for (uint32_t x = 0u; x < m_srcImage.extent.width; x++)
            for (uint32_t y = 0u; y < m_srcImage.extent.height / 2; y++)
                for (uint32_t s = 0u; s < sampleCount; ++s)
                {
                    const auto bufferIndex = (y * fbWidth + x) * sampleCount + s;
                    if (outputCopied[bufferIndex] != m_clearValue)
                    {
                        std::ostringstream msg;
                        msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample "
                            << s << ". "
                            << "result: " << outputCopied[bufferIndex] << " expected: 0.0";
                        return tcu::TestStatus::fail(msg.str());
                    }
                }
    }

    log << tcu::TestLog::Message << "Intermediate multisample copy verification passed" << tcu::TestLog::EndMessage;
    return tcu::TestStatus::pass("Pass");
}

class DepthStencilMSAATestCase : public vkt::TestCase
{
public:
    DepthStencilMSAATestCase(tcu::TestContext &testCtx, const std::string &name,
                             const DepthStencilMSAA::TestParameters testParams)
        : vkt::TestCase(testCtx, name)
        , m_params(testParams)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;

    virtual TestInstance *createInstance(Context &context) const
    {
        return new DepthStencilMSAA(context, m_params);
    }

    virtual void checkSupport(Context &context) const
    {
        checkExtensionSupport(context, m_params.extensionFlags);

        const VkSampleCountFlagBits rasterizationSamples = m_params.samples;

        if (!context.getDeviceFeatures().fragmentStoresAndAtomics)
            TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");

        if ((m_params.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT) &&
            !(context.getDeviceProperties().limits.framebufferDepthSampleCounts & rasterizationSamples))
            TCU_THROW(NotSupportedError, "Unsupported number of depth samples");

        if ((m_params.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT) &&
            !(context.getDeviceProperties().limits.framebufferDepthSampleCounts & rasterizationSamples))
            TCU_THROW(NotSupportedError, "Unsupported number of stencil samples");

        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

        VkImageFormatProperties properties;
        if ((context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 context.getPhysicalDevice(), m_params.imageFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                 usageFlags, 0, &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }
    }

private:
    uint32_t getArrayLayerCount() const
    {
        return (m_params.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY) ? 5u : 1u;
    }

    void createVerificationShader(std::ostringstream &shaderCode, const VkImageAspectFlagBits attachmentAspect) const
    {
        DE_ASSERT(attachmentAspect & (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT));
        // The shader copies the sample values from the source and destination image to output buffers OriginalValue and
        // CopiedValues, respectively. If the dst image contains multiple array layers, only one layer has the copied data
        // and the rest should be filled with the clear value (0). This is also verified in this shader.
        // Array layer cases need an image view per layer in the copied image.
        // Set 0 contains the output buffers.
        // Set 1 contains the input attachments.

        std::string inputAttachmentPrefix = attachmentAspect == VK_IMAGE_ASPECT_STENCIL_BIT ? "u" : "";

        shaderCode << "#version 450\n"
                   << "\n"
                   << "layout (push_constant, std430) uniform PushConstants {\n"
                   << "    int width;\n"
                   << "    int height;\n"
                   << "    int samples;\n"
                   << "};\n"
                   << "layout (set=0, binding=0) buffer OriginalValues {\n"
                   << "    float outputOriginal[];\n"
                   << "};\n"
                   << "layout (set=0, binding=1) buffer CopiedValues {\n"
                   << "    float outputCopied[];\n"
                   << "};\n"
                   << "layout (input_attachment_index=0, set=1, binding=0) uniform " << inputAttachmentPrefix
                   << "subpassInputMS attachment0;\n";

        const auto layerCount = getArrayLayerCount();
        for (uint32_t layerNdx = 0u; layerNdx < layerCount; ++layerNdx)
        {
            const auto i = layerNdx + 1u;
            shaderCode << "layout (input_attachment_index=" << i << ", set=1, binding=" << i << ") uniform "
                       << inputAttachmentPrefix << "subpassInputMS attachment" << i << ";\n";
        }

        // Using a loop to iterate over each sample avoids the need for the sampleRateShading feature. The pipeline needs to be
        // created with a single sample.
        shaderCode << "\n"
                   << "void main() {\n"
                   << "    for (int sampleID = 0; sampleID < samples; ++sampleID) {\n"
                   << "        ivec3 coords  = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), sampleID);\n"
                   << "        int bufferPos = (coords.y * width + coords.x) * samples + coords.z;\n"
                   << "        " << inputAttachmentPrefix << "vec4 orig = subpassLoad(attachment0, sampleID);\n"
                   << "        outputOriginal[bufferPos] = orig.r;\n";

        for (uint32_t layerNdx = 0u; layerNdx < layerCount; ++layerNdx)
        {
            const auto i = layerNdx + 1u;
            shaderCode << "        " << inputAttachmentPrefix << "vec4 copy" << i << " = subpassLoad(attachment" << i
                       << ", sampleID);\n";
        }

        std::ostringstream testCondition;
        std::string layerToVerify = m_params.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY ? "copy4" : "copy1";
        shaderCode << "\n"
                   << "        outputCopied[bufferPos] = " << layerToVerify << ".r; \n";

        if (m_params.copyOptions == DepthStencilMSAA::COPY_ARRAY_TO_ARRAY)
        {
            // In array layer copy tests the copied image should be in the layer 3 and other layers should be value of 0 or 0.0 depending on the format.
            // This verifies that all the samples in the other layers have proper values.
            shaderCode << "        bool equalEmptyLayers = ";
            for (uint32_t layerNdx = 0u; layerNdx < layerCount; ++layerNdx)
            {
                if (layerNdx == 3)
                    continue;
                const auto i = layerNdx + 1u;
                shaderCode << "copy" << i << ".r == " << (attachmentAspect == VK_IMAGE_ASPECT_STENCIL_BIT ? "0" : "0.0")
                           << (layerNdx < 4u ? " && " : ";\n");
            }
            shaderCode << "        if (!equalEmptyLayers)\n"
                       << "            outputCopied[bufferPos]--; \n";
        }

        shaderCode << "    }\n"
                   << "}\n";
    }

    DepthStencilMSAA::TestParameters m_params;
};

void DepthStencilMSAATestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert") << glu::VertexSource("#version 310 es\n"
                                                                   "layout (location = 0) in highp vec4 a_position;\n"
                                                                   "void main()\n"
                                                                   "{\n"
                                                                   "    gl_Position = vec4(a_position.xy, 1.0, 1.0);\n"
                                                                   "}\n");

    programCollection.glslSources.add("frag") << glu::FragmentSource("#version 310 es\n"
                                                                     "void main()\n"
                                                                     "{}\n");

    // Create the verifying shader for the depth aspect if the depth is used.
    if (m_params.copyAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        std::ostringstream verificationShader;
        // All the depth formats are float types, so the input attachment prefix is not used.
        createVerificationShader(verificationShader, VK_IMAGE_ASPECT_DEPTH_BIT);
        programCollection.glslSources.add("verify_depth") << glu::FragmentSource(verificationShader.str());
    }

    // Create the verifying shader for the stencil aspect if the stencil is used.
    if (m_params.copyAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
    {
        std::ostringstream verificationShader;
        // All the stencil formats are uint types, so the input attachment prefix is "u".
        createVerificationShader(verificationShader, VK_IMAGE_ASPECT_STENCIL_BIT);
        programCollection.glslSources.add("verify_stencil") << glu::FragmentSource(verificationShader.str());
    }
}

const VkSampleCountFlagBits samples[] = {VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
                                         VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT};

void addDepthStencilCopyMSAATest(tcu::TestCaseGroup *group, DepthStencilMSAA::TestParameters testCreateParams)
{
    // Run all the tests with one of the bare depth format and one bare stencil format + mandatory combined formats.
    const struct
    {
        const std::string name;
        const VkFormat vkFormat;
    } dsFormats[] = {
        {"d32_sfloat", VK_FORMAT_D32_SFLOAT},
        {"s8_uint", VK_FORMAT_S8_UINT},
        {"d16_unorm_s8_uint", VK_FORMAT_D16_UNORM_S8_UINT},
        {"d24_unorm_s8_uint", VK_FORMAT_D24_UNORM_S8_UINT},
    };

    // Both image layouts will be tested only with full image copy tests to limit the number of tests.
    const VkImageLayout srcImageLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};
    const VkImageLayout dstImageLayouts[] = {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL};

    for (const auto &srcLayout : srcImageLayouts)
    {
        for (const auto &dstLayout : dstImageLayouts)
        {
            testCreateParams.srcImageLayout = srcLayout;
            testCreateParams.dstImageLayout = dstLayout;
            for (const auto &format : dsFormats)
            {
                testCreateParams.imageFormat = format.vkFormat;
                const auto textureFormat     = mapVkFormat(format.vkFormat);
                bool hasDepth                = tcu::hasDepthComponent(textureFormat.order);
                bool hasStencil              = tcu::hasStencilComponent(textureFormat.order);
                std::string testNameBase =
                    format.name + "_" +
                    (testCreateParams.copyOptions == DepthStencilMSAA::COPY_WHOLE_IMAGE ?
                         getImageLayoutCaseName(srcLayout) + "_" + getImageLayoutCaseName(dstLayout) + "_" :
                         "");

                if (hasDepth)
                {
                    testCreateParams.copyAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                    for (auto sample : samples)
                    {
                        testCreateParams.samples     = sample;
                        testCreateParams.imageOffset = false;
                        group->addChild(new DepthStencilMSAATestCase(
                            group->getTestContext(), testNameBase + "D_" + getSampleCountCaseName(sample),
                            testCreateParams));
                        testCreateParams.imageOffset = true;
                        if (testCreateParams.allocationKind != ALLOCATION_KIND_DEDICATED)
                        {
                            group->addChild(new DepthStencilMSAATestCase(
                                group->getTestContext(),
                                testNameBase + "D_" + getSampleCountCaseName(sample) + "_bind_offset",
                                testCreateParams));
                        }
                    }
                }

                if (hasStencil)
                {
                    testCreateParams.copyAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
                    for (auto sample : samples)
                    {
                        testCreateParams.samples     = sample;
                        testCreateParams.imageOffset = false;
                        group->addChild(new DepthStencilMSAATestCase(
                            group->getTestContext(), testNameBase + "S_" + getSampleCountCaseName(sample),
                            testCreateParams));
                        testCreateParams.imageOffset = true;
                        if (testCreateParams.allocationKind != ALLOCATION_KIND_DEDICATED)
                        {
                            group->addChild(new DepthStencilMSAATestCase(
                                group->getTestContext(),
                                testNameBase + "S_" + getSampleCountCaseName(sample) + "_bind_offset",
                                testCreateParams));
                        }
                    }
                }
            }
            if (testCreateParams.copyOptions != DepthStencilMSAA::COPY_WHOLE_IMAGE)
                break;
        }
        if (testCreateParams.copyOptions != DepthStencilMSAA::COPY_WHOLE_IMAGE)
            break;
    }
}

} // namespace

void addCopyDepthStencilMSAATests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    // Allocation kind, extension use copy option parameters are defined here. Rest of the parameters are defined in `addDepthStencilCopyMSAATest` function.
    DepthStencilMSAA::TestParameters testParams = {};
    testParams.allocationKind                   = allocationKind;
    testParams.extensionFlags                   = extensionFlags;

    testParams.copyOptions = DepthStencilMSAA::COPY_WHOLE_IMAGE;
    addTestGroup(group, "whole", addDepthStencilCopyMSAATest, testParams);

    testParams.copyOptions = DepthStencilMSAA::COPY_PARTIAL;
    addTestGroup(group, "partial", addDepthStencilCopyMSAATest, testParams);

    testParams.copyOptions = DepthStencilMSAA::COPY_ARRAY_TO_ARRAY;
    addTestGroup(group, "array_to_array", addDepthStencilCopyMSAATest, testParams);
}

} // namespace api
} // namespace vkt
