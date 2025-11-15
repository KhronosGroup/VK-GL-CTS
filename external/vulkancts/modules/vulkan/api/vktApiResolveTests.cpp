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
 * \brief Vulkan Resolve Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiResolveTests.hpp"

namespace vkt
{

namespace api
{

namespace
{

enum ResolveImageToImageOptions
{
    NO_OPTIONAL_OPERATION = 0,
    COPY_MS_IMAGE_TO_MS_IMAGE,
    COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE,
    COPY_MS_IMAGE_LAYER_TO_MS_IMAGE,
    COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION,
    COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB,
    COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE,
    COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER
};

class ResolveImageToImage : public CopiesAndBlittingTestInstanceWithSparseSemaphore
{
public:
    ResolveImageToImage(Context &context, TestParams params, ResolveImageToImageOptions options);
    virtual tcu::TestStatus iterate(void);

    static inline bool shouldVerifyIntermediateResults(ResolveImageToImageOptions option)
    {
        return option == COPY_MS_IMAGE_TO_MS_IMAGE || option == COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE ||
               option == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || option == COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE ||
               option == COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER;
    }

protected:
    virtual tcu::TestStatus checkTestResult(tcu::ConstPixelBufferAccess result);
    void copyMSImageToMSImage(uint32_t copyArraySize);
    tcu::TestStatus checkIntermediateCopy(void);

private:
    de::MovePtr<vk::Allocator> m_alternativeAllocator;
    Move<VkImage> m_multisampledImage;
    de::MovePtr<Allocation> m_multisampledImageAlloc;

    Move<VkImage> m_destination;
    de::MovePtr<Allocation> m_destinationImageAlloc;
    std::vector<de::SharedPtr<Allocation>> m_sparseAllocations;

    Move<VkImage> m_multisampledCopyImage;
    de::MovePtr<Allocation> m_multisampledCopyImageAlloc;
    Move<VkImage> m_multisampledCopyNoCabImage;
    de::MovePtr<Allocation> m_multisampledCopyImageNoCabAlloc;

    const TestParams m_params;
    const ResolveImageToImageOptions m_options;

    virtual void copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                          CopyRegion region, uint32_t mipLevel = 0u);
};

ResolveImageToImage::ResolveImageToImage(Context &context, TestParams params, const ResolveImageToImageOptions options)
    : CopiesAndBlittingTestInstanceWithSparseSemaphore(context, params)
    , m_params(params)
    , m_options(options)
{
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vk    = m_context.getDeviceInterface();

    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    Allocator &memAlloc                           = *m_allocator;
    const VkPhysicalDevice vkPhysDevice           = m_context.getPhysicalDevice();
    const VkDevice vkDevice                       = m_device;
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    Move<VkRenderPass> renderPass;

    Move<VkShaderModule> vertexShaderModule =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert"), 0);
    Move<VkShaderModule> fragmentShaderModule =
        createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag"), 0);
    std::vector<tcu::Vec4> vertices;

    Move<VkBuffer> vertexBuffer;
    de::MovePtr<Allocation> vertexBufferAlloc;

    Move<VkPipelineLayout> pipelineLayout;
    Move<VkPipeline> graphicsPipeline;

    const VkSampleCountFlagBits rasterizationSamples = m_params.samples;

    // Create color image.
    {
        VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            getCreateFlags(m_params.src.image),  // VkImageCreateFlags flags;
            m_params.src.image.imageType,        // VkImageType imageType;
            m_params.src.image.format,           // VkFormat format;
            getExtent3D(m_params.src.image),     // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            getArraySize(m_params.src.image),    // uint32_t arrayLayers;
            rasterizationSamples,                // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT  // VkImageUsageFlags usage;
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            0u,                        // uint32_t queueFamilyIndexCount;
            nullptr,                   // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
        };

        m_multisampledImage      = createImage(vk, vkDevice, &colorImageParams);
        VkMemoryRequirements req = getImageMemoryRequirements(vk, vkDevice, *m_multisampledImage);

        // Allocate and bind color image memory.
        uint32_t offset          = m_params.imageOffset ? static_cast<uint32_t>(req.alignment) : 0u;
        m_multisampledImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledImage,
                                                 MemoryRequirement::Any, memAlloc, m_params.allocationKind, offset);

        VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledImage, m_multisampledImageAlloc->getMemory(), offset));

        switch (m_options)
        {
        case COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION:
        case COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE:
        case COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER:
        case COPY_MS_IMAGE_TO_MS_IMAGE:
        {
            colorImageParams.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            m_multisampledCopyImage = createImage(vk, vkDevice, &colorImageParams);
            // Allocate and bind color image memory.
            m_multisampledCopyImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage,
                                                         MemoryRequirement::Any, memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(),
                                        m_multisampledCopyImageAlloc->getOffset()));
            break;
        }
        case COPY_MS_IMAGE_LAYER_TO_MS_IMAGE:
        case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
        {
            colorImageParams.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            colorImageParams.arrayLayers = getArraySize(m_params.dst.image);
            m_multisampledCopyImage      = createImage(vk, vkDevice, &colorImageParams);
            // Allocate and bind color image memory.
            m_multisampledCopyImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage,
                                                         MemoryRequirement::Any, memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(),
                                        m_multisampledCopyImageAlloc->getOffset()));
            break;
        }
        case COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB:
        {
            colorImageParams.usage =
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            colorImageParams.arrayLayers = getArraySize(m_params.dst.image);
            m_multisampledCopyImage      = createImage(vk, vkDevice, &colorImageParams);
            m_multisampledCopyNoCabImage = createImage(vk, vkDevice, &colorImageParams);
            // Allocate and bind color image memory.
            m_multisampledCopyImageAlloc = allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyImage,
                                                         MemoryRequirement::Any, memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyImage, m_multisampledCopyImageAlloc->getMemory(),
                                        m_multisampledCopyImageAlloc->getOffset()));
            m_multisampledCopyImageNoCabAlloc =
                allocateImage(vki, vk, vkPhysDevice, vkDevice, *m_multisampledCopyNoCabImage, MemoryRequirement::Any,
                              memAlloc, m_params.allocationKind, 0u);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_multisampledCopyNoCabImage,
                                        m_multisampledCopyImageNoCabAlloc->getMemory(),
                                        m_multisampledCopyImageNoCabAlloc->getOffset()));
            break;
        }

        default:
            break;
        }
    }

    // Create destination image.
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
        if (!params.useSparseBinding)
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

    // Barriers for image clearing.
    std::vector<VkImageMemoryBarrier> srcImageBarriers;

    const VkImageMemoryBarrier m_multisampledImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        0u,                                     // VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_WRITE_BIT,             // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledImage.get(),              // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            VK_IMAGE_ASPECT_COLOR_BIT,       // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.src.image) // uint32_t arraySize;
        }};
    const VkImageMemoryBarrier m_multisampledCopyImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        0u,                                     // VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_WRITE_BIT,             // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyImage.get(),          // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            VK_IMAGE_ASPECT_COLOR_BIT,       // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.dst.image) // uint32_t arraySize;
        }};
    const VkImageMemoryBarrier m_multisampledCopyImageNoCabBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        0u,                                     // VkAccessFlags srcAccessMask;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyNoCabImage.get(),     // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            VK_IMAGE_ASPECT_COLOR_BIT,       // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.dst.image) // uint32_t arraySize;
        }};

    // Only use one barrier if no options have been given.
    if (m_options != NO_OPTIONAL_OPERATION)
    {
        srcImageBarriers.push_back(m_multisampledImageBarrier);
        srcImageBarriers.push_back(m_multisampledCopyImageBarrier);
        // Add the third barrier if option is as below.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
            srcImageBarriers.push_back(m_multisampledCopyImageNoCabBarrier);
    }
    else
    {
        srcImageBarriers.push_back(m_multisampledImageBarrier);
    }

    // Create render pass.
    {
        const VkAttachmentDescription attachmentDescription = {
            0u,                                   // VkAttachmentDescriptionFlags flags;
            m_params.src.image.format,            // VkFormat format;
            rasterizationSamples,                 // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,          // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,         // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,      // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,     // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL  // VkImageLayout finalLayout;
        };

        const VkAttachmentReference colorAttachmentReference = {
            0u,                                      // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };

        const VkSubpassDescription subpassDescription = {
            0u,                              // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference* pInputAttachments;
            1u,                              // uint32_t colorAttachmentCount;
            &colorAttachmentReference,       // const VkAttachmentReference* pColorAttachments;
            nullptr,                         // const VkAttachmentReference* pResolveAttachments;
            nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            nullptr                          // const VkAttachmentReference* pPreserveAttachments;
        };

        // Subpass dependency is used to synchronize the memory access of the image clear and color attachment write in some test cases.
        const VkSubpassDependency subpassDependency = {
            VK_SUBPASS_EXTERNAL,                           //uint32_t srcSubpass;
            0u,                                            //uint32_t dstSubpass;
            VK_PIPELINE_STAGE_TRANSFER_BIT,                //VkPipelineStageFlags srcStageMask;
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //VkPipelineStageFlags dstStageMask;
            VK_ACCESS_TRANSFER_WRITE_BIT,                  //VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          //VkAccessFlags dstAccessMask;
            0u                                             //VkDependencyFlags dependencyFlags;
        };

        const bool useSubpassDependency =
            m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION;
        const VkRenderPassCreateInfo renderPassParams = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            1u,                                        // uint32_t attachmentCount;
            &attachmentDescription,                    // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
            useSubpassDependency ? 1u : 0u,            // uint32_t dependencyCount;
            &subpassDependency                         // const VkSubpassDependency* pDependencies;
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
        const tcu::Vec4 a(-1.0, -1.0, 0.0, 1.0);
        const tcu::Vec4 b(1.0, -1.0, 0.0, 1.0);
        const tcu::Vec4 c(1.0, 1.0, 0.0, 1.0);
        // Add triangle.
        vertices.push_back(a);
        vertices.push_back(c);
        vertices.push_back(b);
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
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
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

        uint32_t baseArrayLayer = m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE ? 2u : 0u;

        // Create color attachment view.
        {
            const VkImageViewCreateInfo colorAttachmentViewParams = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,               // VkStructureType sType;
                nullptr,                                                // const void* pNext;
                0u,                                                     // VkImageViewCreateFlags flags;
                *m_multisampledImage,                                   // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                                  // VkImageViewType viewType;
                m_params.src.image.format,                              // VkFormat format;
                componentMappingRGBA,                                   // VkComponentMapping components;
                {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, baseArrayLayer, 1u} // VkImageSubresourceRange subresourceRange;
            };
            sourceAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
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
                m_params.src.image.extent.width,           // uint32_t width;
                m_params.src.image.extent.height,          // uint32_t height;
                1u                                         // uint32_t layers;
            };

            framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
        }

        // Create pipeline
        {
            const std::vector<VkViewport> viewports(1, makeViewport(m_params.src.image.extent));
            const std::vector<VkRect2D> scissors(1, makeRect2D(m_params.src.image.extent));

            const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                  // const void* pNext;
                0u,                   // VkPipelineMultisampleStateCreateFlags flags;
                rasterizationSamples, // VkSampleCountFlagBits rasterizationSamples;
                VK_FALSE,             // VkBool32 sampleShadingEnable;
                0.0f,                 // float minSampleShading;
                nullptr,              // const VkSampleMask* pSampleMask;
                VK_FALSE,             // VkBool32 alphaToCoverageEnable;
                VK_FALSE              // VkBool32 alphaToOneEnable;
            };

            graphicsPipeline = makeGraphicsPipeline(
                vk,                    // const DeviceInterface&                        vk
                vkDevice,              // const VkDevice                                device
                *pipelineLayout,       // const VkPipelineLayout                        pipelineLayout
                *vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
                VK_NULL_HANDLE,        // const VkShaderModule                          tessellationControlModule
                VK_NULL_HANDLE,        // const VkShaderModule                          tessellationEvalModule
                VK_NULL_HANDLE,        // const VkShaderModule                          geometryShaderModule
                *fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
                *renderPass,           // const VkRenderPass                            renderPass
                viewports,             // const std::vector<VkViewport>&                viewports
                scissors,              // const std::vector<VkRect2D>&                  scissors
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
                0u,                                  // const uint32_t                                subpass
                0u,                                  // const uint32_t                                patchControlPoints
                nullptr,                  // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
                nullptr,                  // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
                &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
        }

        // Create command buffer
        {
            beginCommandBuffer(vk, *m_universalCmdBuffer, 0u);

            if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION)
            {
                // Change the image layouts.
                vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                      (uint32_t)srcImageBarriers.size(), srcImageBarriers.data());

                // Clear the 'm_multisampledImage'.
                {
                    const VkClearColorValue clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
                    const auto clearRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                                                      m_params.src.image.extent.depth);
                    vk.cmdClearColorImage(*m_universalCmdBuffer, m_multisampledImage.get(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &clearRange);
                }

                // Clear the 'm_multisampledCopyImage' with different color.
                {
                    const VkClearColorValue clearValue = {{1.0f, 1.0f, 1.0f, 1.0f}};
                    const auto clearRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                                                      m_params.src.image.extent.depth);
                    vk.cmdClearColorImage(*m_universalCmdBuffer, m_multisampledCopyImage.get(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1u, &clearRange);
                }
            }
            else
            {
                // Change the image layouts.
                vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, nullptr,
                                      0, nullptr, (uint32_t)srcImageBarriers.size(), srcImageBarriers.data());
            }

            beginRenderPass(vk, *m_universalCmdBuffer, *renderPass, *framebuffer,
                            makeRect2D(0, 0, m_params.src.image.extent.width, m_params.src.image.extent.height),
                            tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));

            const VkDeviceSize vertexBufferOffset = 0u;

            vk.cmdBindPipeline(*m_universalCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
            vk.cmdBindVertexBuffers(*m_universalCmdBuffer, 0, 1, &vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_universalCmdBuffer, (uint32_t)vertices.size(), 1, 0, 0);

            endRenderPass(vk, *m_universalCmdBuffer);
            endCommandBuffer(vk, *m_universalCmdBuffer);
        }

        submitCommandsAndWaitWithTransferSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer,
                                              &m_sparseSemaphore);

        m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
    }
}

tcu::TestStatus ResolveImageToImage::iterate(void)
{
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
    const tcu::TextureFormat dstTcuFormat = mapVkFormat(m_params.dst.image.format);

    // upload the destination image
    m_destinationTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(dstTcuFormat, (int)m_params.dst.image.extent.width, (int)m_params.dst.image.extent.height,
                              (int)m_params.dst.image.extent.depth));
    generateBuffer(m_destinationTextureLevel->getAccess(), m_params.dst.image.extent.width,
                   m_params.dst.image.extent.height, m_params.dst.image.extent.depth);
    uploadImage(m_destinationTextureLevel->getAccess(), m_destination.get(), m_params.dst.image,
                m_params.useGeneralLayout);

    m_sourceTextureLevel = de::MovePtr<tcu::TextureLevel>(
        new tcu::TextureLevel(srcTcuFormat, (int)m_params.src.image.extent.width, (int)m_params.src.image.extent.height,
                              (int)m_params.dst.image.extent.depth));

    generateBuffer(m_sourceTextureLevel->getAccess(), m_params.src.image.extent.width, m_params.src.image.extent.height,
                   m_params.dst.image.extent.depth, FILL_MODE_MULTISAMPLE);
    generateExpectedResult();

    VkImage sourceImage      = m_multisampledImage.get();
    uint32_t sourceArraySize = getArraySize(m_params.src.image);

    switch (m_options)
    {
    case COPY_MS_IMAGE_LAYER_TO_MS_IMAGE:
    case COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE:
        // Duplicate the multisampled image to a multisampled image array
        sourceArraySize = getArraySize(m_params.dst.image); // fall through
    case COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION:
    case COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB:
    case COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE:
    case COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER:
    case COPY_MS_IMAGE_TO_MS_IMAGE:
        copyMSImageToMSImage(sourceArraySize);
        sourceImage = m_multisampledCopyImage.get();
        break;
    default:
        break;
    }

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_device;

    std::vector<VkImageResolve> imageResolves;
    std::vector<VkImageResolve2KHR> imageResolves2KHR;
    for (CopyRegion region : m_params.regions)
    {
        // If copying multiple regions, make sure that the same regions are
        // used for resolving as the ones used for copying.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION)
        {
            VkExtent3D partialExtent = {getExtent3D(m_params.src.image).width / 2,
                                        getExtent3D(m_params.src.image).height / 2,
                                        getExtent3D(m_params.src.image).depth};

            const VkImageResolve imageResolve = {
                region.imageResolve.srcSubresource, // VkImageSubresourceLayers srcSubresource;
                region.imageResolve.dstOffset,      // VkOffset3D srcOffset;
                region.imageResolve.dstSubresource, // VkImageSubresourceLayers dstSubresource;
                region.imageResolve.dstOffset,      // VkOffset3D dstOffset;
                partialExtent,                      // VkExtent3D extent;
            };

            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageResolves.push_back(imageResolve);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageResolves2KHR.push_back(convertvkImageResolveTovkImageResolve2KHR(imageResolve));
            }
        }
        else
        {
            if (!(m_params.extensionFlags & COPY_COMMANDS_2))
            {
                imageResolves.push_back(region.imageResolve);
            }
            else
            {
                DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
                imageResolves2KHR.push_back(convertvkImageResolveTovkImageResolve2KHR(region.imageResolve));
            }
        }
    }

    const VkImageMemoryBarrier imageBarriers[] = {
        // source image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         nullptr,                                // const void* pNext;
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags srcAccessMask;
         VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
         m_options == NO_OPTIONAL_OPERATION ? m_params.dst.image.operationLayout :
                                              m_params.src.image.operationLayout, // VkImageLayout oldLayout;
         m_params.src.image.operationLayout,                                      // VkImageLayout newLayout;
         VK_QUEUE_FAMILY_IGNORED,                                                 // uint32_t srcQueueFamilyIndex;
         VK_QUEUE_FAMILY_IGNORED,                                                 // uint32_t dstQueueFamilyIndex;
         sourceImage,                                                             // VkImage image;
         {
             // VkImageSubresourceRange subresourceRange;
             getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
             0u,                           // uint32_t baseMipLevel;
             1u,                           // uint32_t mipLevels;
             0u,                           // uint32_t baseArraySlice;
             sourceArraySize               // uint32_t arraySize;
         }},
        // destination image
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
         nullptr,                                // const void* pNext;
         0u,                                     // VkAccessFlags srcAccessMask;
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

    const VkImageMemoryBarrier postImageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                                                   nullptr,                                // const void* pNext;
                                                   VK_ACCESS_TRANSFER_WRITE_BIT,       // VkAccessFlags srcAccessMask;
                                                   VK_ACCESS_HOST_READ_BIT,            // VkAccessFlags dstAccessMask;
                                                   m_params.dst.image.operationLayout, // VkImageLayout oldLayout;
                                                   m_params.dst.image.operationLayout, // VkImageLayout newLayout;
                                                   VK_QUEUE_FAMILY_IGNORED,            // uint32_t srcQueueFamilyIndex;
                                                   VK_QUEUE_FAMILY_IGNORED,            // uint32_t dstQueueFamilyIndex;
                                                   m_destination.get(),                // VkImage image;
                                                   {
                                                       // VkImageSubresourceRange subresourceRange;
                                                       getAspectFlags(dstTcuFormat), // VkImageAspectFlags aspectMask;
                                                       0u,                           // uint32_t baseMipLevel;
                                                       1u,                           // uint32_t mipLevels;
                                                       0u,                           // uint32_t baseArraySlice;
                                                       getArraySize(m_params.dst.image) // uint32_t arraySize;
                                                   }};

    beginCommandBuffer(vk, *m_universalCmdBuffer);
    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                          DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        vk.cmdResolveImage(*m_universalCmdBuffer, sourceImage, m_params.src.image.operationLayout, m_destination.get(),
                           m_params.dst.image.operationLayout, (uint32_t)m_params.regions.size(), imageResolves.data());
    }
    else
    {
        DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
        const VkResolveImageInfo2KHR ResolveImageInfo2KHR = {
            VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2_KHR, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            sourceImage,                                // VkImage srcImage;
            m_params.src.image.operationLayout,         // VkImageLayout srcImageLayout;
            m_destination.get(),                        // VkImage dstImage;
            m_params.dst.image.operationLayout,         // VkImageLayout dstImageLayout;
            (uint32_t)m_params.regions.size(),          // uint32_t regionCount;
            imageResolves2KHR.data()                    // const  VkImageResolve2KHR* pRegions;
        };
        vk.cmdResolveImage2(*m_universalCmdBuffer, &ResolveImageInfo2KHR);
    }

    vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);
    endCommandBuffer(vk, *m_universalCmdBuffer);
    submitCommandsAndWaitWithTransferSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer, &m_sparseSemaphore);
    m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);

    de::MovePtr<tcu::TextureLevel> resultTextureLevel = readImage(*m_destination, m_params.dst.image);

    if (shouldVerifyIntermediateResults(m_options))
    {
        // Verify the intermediate multisample copy operation happens properly instead of, for example, shuffling samples around or
        // resolving the image and giving every sample the same value.
        const auto intermediateResult = checkIntermediateCopy();
        if (intermediateResult.getCode() != QP_TEST_RESULT_PASS)
            return intermediateResult;
    }

    return checkTestResult(resultTextureLevel->getAccess());
}

tcu::TestStatus ResolveImageToImage::checkTestResult(tcu::ConstPixelBufferAccess result)
{
    const tcu::ConstPixelBufferAccess expected = m_expectedTextureLevel[0]->getAccess();
    const float fuzzyThreshold                 = 0.01f;

    if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
    {
        // Check that all the layers that have not been written to are solid white.
        tcu::Vec4 expectedColor(1.0f, 1.0f, 1.0f, 1.0f);
        for (int arrayLayerNdx = 0; arrayLayerNdx < (int)getArraySize(m_params.dst.image) - 1; ++arrayLayerNdx)
        {
            const tcu::ConstPixelBufferAccess resultSub =
                getSubregion(result, 0u, 0u, arrayLayerNdx, result.getWidth(), result.getHeight(), 1u);
            if (resultSub.getPixel(0, 0) != expectedColor)
                return tcu::TestStatus::fail("CopiesAndBlitting test. Layers image differs from initialized value.");
        }

        // Check that the layer that has been copied to is the same as the layer that has been copied from.
        const tcu::ConstPixelBufferAccess expectedSub =
            getSubregion(expected, 0u, 0u, 2u, expected.getWidth(), expected.getHeight(), 1u);
        const tcu::ConstPixelBufferAccess resultSub =
            getSubregion(result, 0u, 0u, 4u, result.getWidth(), result.getHeight(), 1u);
        if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedSub,
                               resultSub, fuzzyThreshold, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("CopiesAndBlitting test");
    }
    else
    {
        for (int arrayLayerNdx = 0; arrayLayerNdx < (int)getArraySize(m_params.dst.image); ++arrayLayerNdx)
        {
            const tcu::ConstPixelBufferAccess expectedSub =
                getSubregion(expected, 0u, 0u, arrayLayerNdx, expected.getWidth(), expected.getHeight(), 1u);
            const tcu::ConstPixelBufferAccess resultSub =
                getSubregion(result, 0u, 0u, arrayLayerNdx, result.getWidth(), result.getHeight(), 1u);
            if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "Compare", "Result comparsion", expectedSub,
                                   resultSub, fuzzyThreshold, tcu::COMPARE_LOG_RESULT))
                return tcu::TestStatus::fail("CopiesAndBlitting test");
        }
    }

    return tcu::TestStatus::pass("CopiesAndBlitting test");
}

void ResolveImageToImage::copyRegionToTextureLevel(tcu::ConstPixelBufferAccess src, tcu::PixelBufferAccess dst,
                                                   CopyRegion region, uint32_t mipLevel)
{
    DE_UNREF(mipLevel);

    VkOffset3D srcOffset = region.imageResolve.srcOffset;
    srcOffset.z          = region.imageResolve.srcSubresource.baseArrayLayer;
    VkOffset3D dstOffset = region.imageResolve.dstOffset;
    dstOffset.z          = region.imageResolve.dstSubresource.baseArrayLayer;
    VkExtent3D extent    = region.imageResolve.extent;
    extent.depth         = (region.imageResolve.srcSubresource.layerCount == VK_REMAINING_ARRAY_LAYERS) ?
                               (src.getDepth() - region.imageResolve.srcSubresource.baseArrayLayer) :
                               region.imageResolve.srcSubresource.layerCount;

    const tcu::ConstPixelBufferAccess srcSubRegion =
        getSubregion(src, srcOffset.x, srcOffset.y, srcOffset.z, extent.width, extent.height, extent.depth);
    // CopyImage acts like a memcpy. Replace the destination format with the srcformat to use a memcpy.
    const tcu::PixelBufferAccess dstWithSrcFormat(srcSubRegion.getFormat(), dst.getSize(), dst.getDataPtr());
    const tcu::PixelBufferAccess dstSubRegion = getSubregion(dstWithSrcFormat, dstOffset.x, dstOffset.y, dstOffset.z,
                                                             extent.width, extent.height, extent.depth);

    tcu::copy(dstSubRegion, srcSubRegion);
}

tcu::TestStatus ResolveImageToImage::checkIntermediateCopy(void)
{
    const auto &vkd          = m_context.getDeviceInterface();
    const auto device        = m_device;
    const auto queueIndex    = m_context.getUniversalQueueFamilyIndex();
    auto &alloc              = *m_allocator;
    const auto currentLayout = m_params.src.image.operationLayout;
    const auto numDstLayers  = getArraySize(m_params.dst.image);
    const auto numInputAttachments =
        m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE ? 2u : numDstLayers + 1u; // For the source image.
    constexpr auto numSets = 2u; // 1 for the output buffer, 1 for the input attachments.
    const auto fbWidth     = m_params.src.image.extent.width;
    const auto fbHeight    = m_params.src.image.extent.height;

    // Push constants.
    const std::array<int, 3> pushConstantData = {{
        static_cast<int>(fbWidth),
        static_cast<int>(fbHeight),
        static_cast<int>(m_params.samples),
    }};
    const auto pushConstantSize =
        static_cast<uint32_t>(pushConstantData.size() * sizeof(decltype(pushConstantData)::value_type));

    // Shader modules.
    const auto vertexModule       = createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto verificationModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("verify"), 0u);

    // Descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numInputAttachments);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, numSets);

    DescriptorSetLayoutBuilder layoutBuilderBuffer;
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
        0u,                               // VkAttachmentDescriptionFlags flags;
        m_params.src.image.format,        // VkFormat format;
        m_params.samples,                 // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
        currentLayout,                    // VkImageLayout initialLayout;
        currentLayout,                    // VkImageLayout finalLayout;
    };
    const std::vector<VkAttachmentDescription> attachmentDescriptions(numInputAttachments, commonAttachmentDescription);

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
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        attachmentDescriptions.data(),                        // const VkAttachmentDescription* pAttachments;
        1u,                                                   // uint32_t subpassCount;
        &subpassDescription,                                  // const VkSubpassDescription* pSubpasses;
        1u,                                                   // uint32_t dependencyCount;
        &subpassDependency,                                   // const VkSubpassDependency* pDependencies;
    };

    const auto renderPass = createRenderPass(vkd, device, &renderPassInfo);

    // Framebuffer.
    std::vector<Move<VkImageView>> imageViews;
    std::vector<VkImageView> imageViewsRaw;

    if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
    {
        imageViews.push_back(makeImageView(vkd, device, m_multisampledImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           m_params.src.image.format,
                                           makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 2u, 1u)));
        imageViews.push_back(makeImageView(vkd, device, m_multisampledCopyImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           m_params.src.image.format,
                                           makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 4u, 1u)));
    }
    else
    {
        imageViews.push_back(makeImageView(vkd, device, m_multisampledImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                           m_params.src.image.format,
                                           makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));
        for (uint32_t i = 0u; i < numDstLayers; ++i)
        {
            const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, i, 1u);
            imageViews.push_back(makeImageView(vkd, device, m_multisampledCopyImage.get(), VK_IMAGE_VIEW_TYPE_2D,
                                               m_params.dst.image.format, subresourceRange));
        }
    }

    imageViewsRaw.reserve(imageViews.size());
    std::transform(begin(imageViews), end(imageViews), std::back_inserter(imageViewsRaw),
                   [](const Move<VkImageView> &ptr) { return ptr.get(); });

    const auto framebuffer = makeFramebuffer(vkd, device, renderPass.get(), static_cast<uint32_t>(imageViewsRaw.size()),
                                             imageViewsRaw.data(), fbWidth, fbHeight);

    // Storage buffer.
    const auto bufferCount = static_cast<size_t>(fbWidth * fbHeight * m_params.samples);
    const auto bufferSize  = static_cast<VkDeviceSize>(bufferCount * sizeof(int32_t));
    BufferWithMemory buffer(vkd, device, alloc, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                            MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    void *bufferData  = bufferAlloc.getHostPtr();

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto bufferInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);

    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(imageViewsRaw.size());
    for (size_t i = 0; i < imageViewsRaw.size(); ++i)
        imageInfos.push_back(
            makeDescriptorImageInfo(VK_NULL_HANDLE, imageViewsRaw[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

    for (size_t i = 0; i < imageInfos.size(); ++i)
        updater.writeSingle(descriptorSetAttachments.get(),
                            DescriptorSetUpdateBuilder::Location::binding(static_cast<uint32_t>(i)),
                            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &imageInfos[i]);

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
    const auto vertexBufferHandler        = vertexBuffer.get();
    auto &vertexBufferAlloc               = vertexBuffer.getAllocation();
    void *vertexBufferData                = vertexBufferAlloc.getHostPtr();
    const VkDeviceSize vertexBufferOffset = 0ull;

    deMemcpy(vertexBufferData, fullScreenQuad.data(), static_cast<size_t>(vertexBufferSize));
    flushAlloc(vkd, device, vertexBufferAlloc);

    // Graphics pipeline.
    const std::vector<VkViewport> viewports(1, makeViewport(m_params.src.image.extent));
    const std::vector<VkRect2D> scissors(1, makeRect2D(m_params.src.image.extent));

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
        vkd,                                 // const DeviceInterface&                        vk
        device,                              // const VkDevice                                device
        pipelineLayout.get(),                // const VkPipelineLayout                        pipelineLayout
        vertexModule.get(),                  // const VkShaderModule                          vertexShaderModule
        VK_NULL_HANDLE,                      // const VkShaderModule                          tessellationControlModule
        VK_NULL_HANDLE,                      // const VkShaderModule                          tessellationEvalModule
        VK_NULL_HANDLE,                      // const VkShaderModule                          geometryShaderModule
        verificationModule.get(),            // const VkShaderModule                          fragmentShaderModule
        renderPass.get(),                    // const VkRenderPass                            renderPass
        viewports,                           // const std::vector<VkViewport>&                viewports
        scissors,                            // const std::vector<VkRect2D>&                  scissors
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
        0u,                                  // const uint32_t                                subpass
        0u,                                  // const uint32_t                                patchControlPoints
        nullptr,                             // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
        nullptr,                  // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
        &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo

    // Command buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    // Make sure multisample copy data is available to the fragment shader.
    const auto imagesBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);

    // Make sure input attachment can be read by the shader after the loadop is executed at the start of the renderpass
    const auto loadBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    // Make sure verification buffer data is available on the host.
    const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    // Record and submit command buffer.
    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u,
                           &imagesBarrier, 0u, nullptr, 0u, nullptr);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), makeRect2D(m_params.src.image.extent));
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1u, &loadBarrier,
                           0u, nullptr, 0u, nullptr);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());
    vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBufferHandler, &vertexBufferOffset);
    vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0u, pushConstantSize,
                         pushConstantData.data());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u,
                              static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0u, nullptr);
    vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(fullScreenQuad.size()), 1u, 0u, 0u);
    endRenderPass(vkd, cmdBuffer);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &bufferBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWaitWithTransferSync(vkd, device, m_universalQueue, cmdBuffer, &m_sparseSemaphore);
    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    // Verify intermediate results.
    invalidateAlloc(vkd, device, bufferAlloc);
    std::vector<int32_t> outputFlags(bufferCount, 0);
    deMemcpy(outputFlags.data(), bufferData, static_cast<size_t>(bufferSize));

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Message << "Verifying intermediate multisample copy results" << tcu::TestLog::EndMessage;

    const auto sampleCount = static_cast<uint32_t>(m_params.samples);

    for (uint32_t x = 0u; x < fbWidth; ++x)
        for (uint32_t y = 0u; y < fbHeight; ++y)
            for (uint32_t s = 0u; s < sampleCount; ++s)
            {
                const auto index = (y * fbWidth + x) * sampleCount + s;
                if (!outputFlags[index])
                {
                    std::ostringstream msg;
                    msg << "Intermediate verification failed for coordinates (" << x << ", " << y << ") sample " << s;
                    return tcu::TestStatus::fail(msg.str());
                }
            }

    log << tcu::TestLog::Message << "Intermediate multisample copy verification passed" << tcu::TestLog::EndMessage;
    return tcu::TestStatus::pass("Pass");
}

void ResolveImageToImage::copyMSImageToMSImage(uint32_t copyArraySize)
{
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice vkDevice               = m_device;
    const tcu::TextureFormat srcTcuFormat = mapVkFormat(m_params.src.image.format);
    std::vector<VkImageCopy> imageCopies;
    std::vector<VkImageCopy2KHR> imageCopies2KHR;

    if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
    {
        const VkImageSubresourceLayers sourceSubresourceLayers = {
            getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
            0u,                           // uint32_t mipLevel;
            2u,                           // uint32_t baseArrayLayer;
            1u                            // uint32_t layerCount;
        };

        const VkImageSubresourceLayers destinationSubresourceLayers = {
            getAspectFlags(srcTcuFormat), // VkImageAspectFlags    aspectMask;//getAspectFlags(dstTcuFormat)
            0u,                           // uint32_t mipLevel;
            4u,                           // uint32_t baseArrayLayer;
            1u                            // uint32_t layerCount;
        };

        const VkImageCopy imageCopy = {
            sourceSubresourceLayers,         // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},                       // VkOffset3D srcOffset;
            destinationSubresourceLayers,    // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},                       // VkOffset3D dstOffset;
            getExtent3D(m_params.src.image), // VkExtent3D extent;
        };

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
    else if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION)
    {
        VkExtent3D partialExtent = {getExtent3D(m_params.src.image).width / 2,
                                    getExtent3D(m_params.src.image).height / 2, getExtent3D(m_params.src.image).depth};

        for (CopyRegion region : m_params.regions)
        {
            const VkImageCopy imageCopy = {
                region.imageResolve.srcSubresource, // VkImageSubresourceLayers srcSubresource;
                region.imageResolve.srcOffset,      // VkOffset3D srcOffset;
                region.imageResolve.dstSubresource, // VkImageSubresourceLayers dstSubresource;
                region.imageResolve.dstOffset,      // VkOffset3D dstOffset;
                partialExtent,                      // VkExtent3D extent;
            };

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
    }
    else
    {
        for (uint32_t layerNdx = 0; layerNdx < copyArraySize; ++layerNdx)
        {
            const VkImageSubresourceLayers sourceSubresourceLayers = {
                getAspectFlags(srcTcuFormat), // VkImageAspectFlags aspectMask;
                0u,                           // uint32_t mipLevel;
                0u,                           // uint32_t baseArrayLayer;
                1u                            // uint32_t layerCount;
            };

            const VkImageSubresourceLayers destinationSubresourceLayers = {
                getAspectFlags(srcTcuFormat), // VkImageAspectFlags    aspectMask;//getAspectFlags(dstTcuFormat)
                0u,                           // uint32_t mipLevel;
                layerNdx,                     // uint32_t baseArrayLayer;
                1u                            // uint32_t layerCount;
            };

            const VkImageCopy imageCopy = {
                sourceSubresourceLayers,         // VkImageSubresourceLayers srcSubresource;
                {0, 0, 0},                       // VkOffset3D srcOffset;
                destinationSubresourceLayers,    // VkImageSubresourceLayers dstSubresource;
                {0, 0, 0},                       // VkOffset3D dstOffset;
                getExtent3D(m_params.src.image), // VkExtent3D extent;
            };

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
    }

    VkImageSubresourceRange subresourceRange = {
        getAspectFlags(srcTcuFormat), // VkImageAspectFlags    aspectMask
        0u,                           // uint32_t                baseMipLevel
        1u,                           // uint32_t                mipLevels
        0u,                           // uint32_t                baseArraySlice
        copyArraySize                 // uint32_t                arraySize
    };

    // m_multisampledImage
    const VkImageMemoryBarrier m_multisampledImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
        m_params.src.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledImage.get(),              // VkImage image;
        {
            // VkImageSubresourceRange subresourceRange;
            getAspectFlags(srcTcuFormat),    // VkImageAspectFlags aspectMask;
            0u,                              // uint32_t baseMipLevel;
            1u,                              // uint32_t mipLevels;
            0u,                              // uint32_t baseArraySlice;
            getArraySize(m_params.src.image) // uint32_t arraySize;
        }};
    // m_multisampledCopyImage
    VkImageMemoryBarrier m_multisampledCopyImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        0,                                      // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
        m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyImage.get(),          // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    // m_multisampledCopyNoCabImage (no USAGE_COLOR_ATTACHMENT_BIT)
    const VkImageMemoryBarrier m_multisampledCopyNoCabImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        0,                                      // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout oldLayout;
        m_params.dst.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyNoCabImage.get(),     // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    // destination image
    const VkImageMemoryBarrier multisampledCopyImagePostBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
        VK_ACCESS_MEMORY_READ_BIT,              // VkAccessFlags dstAccessMask;
        m_params.dst.image.operationLayout,     // VkImageLayout oldLayout;
        m_params.src.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyImage.get(),          // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    // destination image (no USAGE_COLOR_ATTACHMENT_BIT)
    const VkImageMemoryBarrier betweenCopyImageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
        m_params.dst.image.operationLayout,     // VkImageLayout oldLayout;
        m_params.src.image.operationLayout,     // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t dstQueueFamilyIndex;
        m_multisampledCopyNoCabImage.get(),     // VkImage image;
        subresourceRange                        // VkImageSubresourceRange subresourceRange;
    };

    uint32_t familyIndex                        = activeQueueFamilyIndex();
    VkQueue queue                               = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkCommandPool commandPool                   = VK_NULL_HANDLE;
    std::tie(queue, commandBuffer, commandPool) = activeExecutionCtx();

    // Queue family ownership transfer. Move ownership of the m_multisampledImage and m_multisampledImageCopy to the compute/transfer queue.
    if (m_params.queueSelection != QueueSelectionOptions::Universal)
    {
        // Release ownership from graphics queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            // Barrier for m_multisampledImage
            VkImageMemoryBarrier releaseBarrier = m_multisampledImageBarrier;
            releaseBarrier.dstAccessMask        = 0u; // dstAccessMask is ignored in ownership release operation.
            releaseBarrier.srcQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            releaseBarrier.dstQueueFamilyIndex  = familyIndex;
            barriers.push_back(releaseBarrier);

            // Barrier for m_multisampledCopyImage
            releaseBarrier                     = m_multisampledCopyImageBarrier;
            releaseBarrier.dstAccessMask       = 0u; // dstAccessMask is ignored in ownership release operation.
            releaseBarrier.srcQueueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            releaseBarrier.dstQueueFamilyIndex = familyIndex;
            barriers.push_back(releaseBarrier);

            beginCommandBuffer(vk, *m_universalCmdBuffer);
            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                  (uint32_t)barriers.size(), barriers.data());
            endCommandBuffer(vk, *m_universalCmdBuffer);

            // As this is a queue ownership transfer, we do not bother with the sparse semaphore here.
            submitCommandsAndWaitWithSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);

            m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
        }

        // Acquire ownership to compute / transfer queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            // Barrier for m_multisampledImage
            VkImageMemoryBarrier acquireBarrier = m_multisampledImageBarrier;
            acquireBarrier.srcAccessMask        = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.srcQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            acquireBarrier.dstQueueFamilyIndex  = familyIndex;
            barriers.push_back(acquireBarrier);

            // Barrier for m_multisampledImage
            acquireBarrier                     = m_multisampledCopyImageBarrier;
            acquireBarrier.srcAccessMask       = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.srcQueueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
            acquireBarrier.dstQueueFamilyIndex = familyIndex;
            barriers.push_back(acquireBarrier);

            beginCommandBuffer(vk, commandBuffer);
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(),
                                  barriers.data());
            endCommandBuffer(vk, commandBuffer);

            // As this is a queue ownership transfer, we do not bother with the sparse semaphore here.
            submitCommandsAndWaitWithSync(vk, vkDevice, queue, commandBuffer);

            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);
        }

        beginCommandBuffer(vk, commandBuffer);
    }
    else
    {
        // Universal queue

        std::vector<VkImageMemoryBarrier> imageBarriers;

        imageBarriers.push_back(m_multisampledImageBarrier);
        // Only use one barrier if no options have been given.
        if (m_options != NO_OPTIONAL_OPERATION)
        {
            imageBarriers.push_back(m_multisampledCopyImageBarrier);
            // Add the third barrier if option is as below.
            if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
                imageBarriers.push_back(m_multisampledCopyNoCabImageBarrier);
        }

        beginCommandBuffer(vk, commandBuffer);
        vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                              (uint32_t)imageBarriers.size(), imageBarriers.data());
    }

    if (!(m_params.extensionFlags & COPY_COMMANDS_2))
    {
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
        {
            vk.cmdCopyImage(commandBuffer, m_multisampledImage.get(), m_params.src.image.operationLayout,
                            m_multisampledCopyNoCabImage.get(), m_params.dst.image.operationLayout,
                            (uint32_t)imageCopies.size(), imageCopies.data());
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &betweenCopyImageBarrier);
            vk.cmdCopyImage(commandBuffer, m_multisampledCopyNoCabImage.get(), m_params.src.image.operationLayout,
                            m_multisampledCopyImage.get(), m_params.dst.image.operationLayout,
                            (uint32_t)imageCopies.size(), imageCopies.data());
        }
        else
        {
            vk.cmdCopyImage(commandBuffer, m_multisampledImage.get(), m_params.src.image.operationLayout,
                            m_multisampledCopyImage.get(), m_params.dst.image.operationLayout,
                            (uint32_t)imageCopies.size(), imageCopies.data());
        }
    }
    else
    {
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB)
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            const VkCopyImageInfo2KHR copyImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
                nullptr,                                 // const void* pNext;
                m_multisampledImage.get(),               // VkImage srcImage;
                m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                m_multisampledCopyNoCabImage.get(),      // VkImage dstImage;
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
                imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
            };
            const VkCopyImageInfo2KHR copyImageInfo2KHRCopy = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR,  // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                m_multisampledCopyNoCabImage.get(),       // VkImage srcImage;
                vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // VkImageLayout srcImageLayout;
                m_multisampledCopyImage.get(),            // VkImage dstImage;
                m_params.dst.image.operationLayout,       // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),         // uint32_t regionCount;
                imageCopies2KHR.data()                    // const VkImageCopy2KHR* pRegions;
            };

            vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHR);
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &betweenCopyImageBarrier);
            vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHRCopy);
        }
        else
        {
            DE_ASSERT(m_params.extensionFlags & COPY_COMMANDS_2);
            const VkCopyImageInfo2KHR copyImageInfo2KHR = {
                VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR, // VkStructureType sType;
                nullptr,                                 // const void* pNext;
                m_multisampledImage.get(),               // VkImage srcImage;
                m_params.src.image.operationLayout,      // VkImageLayout srcImageLayout;
                m_multisampledCopyImage.get(),           // VkImage dstImage;
                m_params.dst.image.operationLayout,      // VkImageLayout dstImageLayout;
                (uint32_t)imageCopies2KHR.size(),        // uint32_t regionCount;
                imageCopies2KHR.data()                   // const VkImageCopy2KHR* pRegions;
            };
            vk.cmdCopyImage2(commandBuffer, &copyImageInfo2KHR);
        }
    }

    if (m_params.queueSelection != QueueSelectionOptions::Universal)
    {
        endCommandBuffer(vk, commandBuffer);
        submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore);
        m_context.resetCommandPoolForVKSC(vkDevice, commandPool);

        VkImageMemoryBarrier srcImageBarrier = makeImageMemoryBarrier(
            0u, 0u, m_params.src.image.operationLayout, m_params.src.image.operationLayout, m_multisampledImage.get(),
            m_multisampledImageBarrier.subresourceRange, familyIndex, m_context.getUniversalQueueFamilyIndex());
        // Release ownership from compute / transfer queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            VkImageMemoryBarrier releaseBarrier = multisampledCopyImagePostBarrier;
            releaseBarrier.dstAccessMask        = 0u; // dstAccessMask is ignored in ownership release operation.
            releaseBarrier.srcQueueFamilyIndex  = familyIndex;
            releaseBarrier.dstQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            barriers.push_back(releaseBarrier);

            releaseBarrier               = srcImageBarrier;
            releaseBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            releaseBarrier.dstAccessMask = 0u; // dstAccessMask is ignored in ownership release operation.
            barriers.push_back(releaseBarrier);

            beginCommandBuffer(vk, commandBuffer);
            vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                  (VkDependencyFlags)0, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(),
                                  barriers.data());
            endCommandBuffer(vk, commandBuffer);

            // Queue ownership transfer, so we do not bother with the sparse semaphore here.
            submitCommandsAndWaitWithSync(vk, vkDevice, queue, commandBuffer);

            m_context.resetCommandPoolForVKSC(vkDevice, commandPool);
        }

        // Move ownership back to graphics queue.
        {
            std::vector<VkImageMemoryBarrier> barriers;
            barriers.reserve(2);

            VkImageMemoryBarrier acquireBarrier = multisampledCopyImagePostBarrier;
            acquireBarrier.srcAccessMask        = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.srcQueueFamilyIndex  = familyIndex;
            acquireBarrier.dstQueueFamilyIndex  = m_context.getUniversalQueueFamilyIndex();
            barriers.push_back(acquireBarrier);

            acquireBarrier               = srcImageBarrier;
            acquireBarrier.srcAccessMask = 0u; // srcAccessMask is ignored in ownership acquire operation.
            acquireBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barriers.push_back(acquireBarrier);

            beginCommandBuffer(vk, *m_universalCmdBuffer);
            vk.cmdPipelineBarrier(*m_universalCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                                  (uint32_t)barriers.size(), barriers.data());
            endCommandBuffer(vk, *m_universalCmdBuffer);

            // Queue ownership transfer, so we do not bother with the sparse semaphore here.
            submitCommandsAndWaitWithSync(vk, vkDevice, m_universalQueue, *m_universalCmdBuffer);

            m_context.resetCommandPoolForVKSC(vkDevice, *m_universalCmdPool);
        }
    }
    else
    {
        vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1u, &multisampledCopyImagePostBarrier);
        endCommandBuffer(vk, commandBuffer);
        submitCommandsAndWaitWithTransferSync(vk, vkDevice, queue, commandBuffer, &m_sparseSemaphore);
        m_context.resetCommandPoolForVKSC(vkDevice, commandPool);
    }
}

class ResolveImageToImageTestCase : public vkt::TestCase
{
public:
    ResolveImageToImageTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params,
                                const ResolveImageToImageOptions options = NO_OPTIONAL_OPERATION)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
        , m_options(options)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;

    virtual TestInstance *createInstance(Context &context) const
    {
        return new ResolveImageToImage(context, m_params, m_options);
    }

    virtual void checkSupport(Context &context) const
    {
        const VkSampleCountFlagBits rasterizationSamples = m_params.samples;

        // Intermediate result check uses fragmentStoresAndAtomics.
        if (ResolveImageToImage::shouldVerifyIntermediateResults(m_options) &&
            !context.getDeviceFeatures().fragmentStoresAndAtomics)
        {
            TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics not supported");
        }

        if (!(context.getDeviceProperties().limits.framebufferColorSampleCounts & rasterizationSamples))
            throw tcu::NotSupportedError("Unsupported number of rasterization samples");

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

        checkExtensionSupport(context, m_params.extensionFlags);

        // Find at least one queue family that supports compute queue but does NOT support graphics queue.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE)
        {
            if (context.getComputeQueueFamilyIndex() == -1)
                TCU_THROW(NotSupportedError, "No queue family found that only supports compute queue.");
        }

        // Find at least one queue family that supports transfer queue but does NOT support graphics and compute queue.
        if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER)
        {
            if (context.getTransferQueueFamilyIndex() == -1)
                TCU_THROW(NotSupportedError, "No queue family found that only supports transfer queue.");
        }
    }

private:
    TestParams m_params;
    const ResolveImageToImageOptions m_options;
};

void ResolveImageToImageTestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert") << glu::VertexSource("#version 310 es\n"
                                                                   "layout (location = 0) in highp vec4 a_position;\n"
                                                                   "void main()\n"
                                                                   "{\n"
                                                                   "    gl_Position = a_position;\n"
                                                                   "}\n");

    programCollection.glslSources.add("frag") << glu::FragmentSource("#version 310 es\n"
                                                                     "layout (location = 0) out highp vec4 o_color;\n"
                                                                     "void main()\n"
                                                                     "{\n"
                                                                     "    o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
                                                                     "}\n");

    if (m_options == COPY_MS_IMAGE_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE ||
        m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION ||
        m_options == COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE || m_options == COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER)
    {
        // The shader verifies all layers in the copied image are the same as the source image.
        // This needs an image view per layer in the copied image.
        // Set 0 contains the output buffer.
        // Set 1 contains the input attachments.

        std::ostringstream verificationShader;

        verificationShader
            << "#version 450\n"
            << "\n"
            << "layout (push_constant, std430) uniform PushConstants {\n"
            << "    int width;\n"
            << "    int height;\n"
            << "    int samples;\n"
            << "};\n"
            << "layout (set=0, binding=0) buffer VerificationResults {\n"
            << "    int verificationFlags[];\n"
            << "};\n"
            << "layout (input_attachment_index=0, set=1, binding=0) uniform subpassInputMS attachment0;\n";

        const auto dstLayers = getArraySize(m_params.dst.image);

        if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
        {
            verificationShader
                << "layout (input_attachment_index=1, set=1, binding=1) uniform subpassInputMS attachment1;\n";
        }
        else
        {
            for (uint32_t layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
            {
                const auto i = layerNdx + 1u;
                verificationShader << "layout (input_attachment_index=" << i << ", set=1, binding=" << i
                                   << ") uniform subpassInputMS attachment" << i << ";\n";
            }
        }

        // Using a loop to iterate over each sample avoids the need for the sampleRateShading feature. The pipeline needs to be
        // created with a single sample.
        verificationShader << "\n"
                           << "void main() {\n"
                           << "    for (int sampleID = 0; sampleID < samples; ++sampleID) {\n"
                           << "        vec4 orig = subpassLoad(attachment0, sampleID);\n";

        std::ostringstream testCondition;
        if (m_options == COPY_MS_IMAGE_LAYER_TO_MS_IMAGE)
        {
            verificationShader << "        vec4 copy = subpassLoad(attachment1, sampleID);\n";
            testCondition << "orig == copy";
        }
        else
        {
            for (uint32_t layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
            {
                const auto i = layerNdx + 1u;
                verificationShader << "        vec4 copy" << i << " = subpassLoad(attachment" << i << ", sampleID);\n";
            }

            for (uint32_t layerNdx = 0u; layerNdx < dstLayers; ++layerNdx)
            {
                const auto i = layerNdx + 1u;
                testCondition << (layerNdx == 0u ? "" : " && ") << "orig == copy" << i;
            }
        }

        verificationShader << "\n"
                           << "        ivec3 coords  = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), sampleID);\n"
                           << "        int bufferPos = (coords.y * width + coords.x) * samples + coords.z;\n"
                           << "\n"
                           << "        verificationFlags[bufferPos] = ((" << testCondition.str() << ") ? 1 : 0); \n"
                           << "    }\n"
                           << "}\n";

        programCollection.glslSources.add("verify") << glu::FragmentSource(verificationShader.str());
    }
}

const VkSampleCountFlagBits samples[] = {VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
                                         VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT};
const VkExtent3D resolveExtent        = {256u, 256u, 1};

void addResolveImageWholeTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };
        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            resolveExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.imageOffset = false;
        params.samples     = samples[samplesIndex];
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params));
        }
    }
}

void addResolveImagePartialTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };
        const VkImageResolve testResolve = {
            sourceLayer,      // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},        // VkOffset3D srcOffset;
            sourceLayer,      // VkImageSubresourceLayers dstSubresource;
            {64u, 64u, 0},    // VkOffset3D dstOffset;
            {128u, 128u, 1u}, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params));
        }
    }
}

void addResolveImageWithRegionsTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;
    params.imageOffset               = allocationKind != ALLOCATION_KIND_DEDICATED;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        for (int i = 0; i < 256; i += 64)
        {
            const VkImageResolve testResolve = {
                sourceLayer,    // VkImageSubresourceLayers srcSubresource;
                {i, i, 0},      // VkOffset3D srcOffset;
                sourceLayer,    // VkImageSubresourceLayers dstSubresource;
                {i, 0, 0},      // VkOffset3D dstOffset;
                {64u, 64u, 1u}, // VkExtent3D extent;
            };

            CopyRegion imageResolve;
            imageResolve.imageResolve = testResolve;
            params.regions.push_back(imageResolve);
        }
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples = samples[samplesIndex];
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params));
    }
}

void addResolveImageWholeCopyBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                  uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(
            group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]), params, COPY_MS_IMAGE_TO_MS_IMAGE));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_MS_IMAGE));
        }
    }
}

void addComputeAndTransferQueueTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    de::MovePtr<tcu::TestCaseGroup> computeGroup(
        new tcu::TestCaseGroup(group->getTestContext(), "whole_copy_before_resolving_compute"));
    de::MovePtr<tcu::TestCaseGroup> transferGroup(
        new tcu::TestCaseGroup(group->getTestContext(), "whole_copy_before_resolving_transfer"));

    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (const auto &sample : samples)
    {
        params.samples = sample;

        params.queueSelection = QueueSelectionOptions::ComputeOnly;
        computeGroup->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(sample),
                                                               params, COPY_MS_IMAGE_TO_MS_IMAGE_COMPUTE));

        params.queueSelection = QueueSelectionOptions::TransferOnly;
        transferGroup->addChild(new ResolveImageToImageTestCase(group->getTestContext(), getSampleCountCaseName(sample),
                                                                params, COPY_MS_IMAGE_TO_MS_IMAGE_TRANSFER));
    }

    group->addChild(computeGroup.release());
    group->addChild(transferGroup.release());
}

void addResolveImageWholeCopyWithoutCabBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                            uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_MS_IMAGE_NO_CAB));
        }
    }
}

void addResolveImageWholeCopyDiffLayoutsBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                             uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    const struct
    {
        VkImageLayout layout;
        std::string name;
    } imageLayouts[] = {{VK_IMAGE_LAYOUT_GENERAL, "general"},
                        {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, "transfer_src_optimal"},
                        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, "transfer_dst_optimal"}};

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        for (int srcLayoutIndex = 0; srcLayoutIndex < DE_LENGTH_OF_ARRAY(imageLayouts); ++srcLayoutIndex)
            for (int dstLayoutIndex = 0; dstLayoutIndex < DE_LENGTH_OF_ARRAY(imageLayouts); ++dstLayoutIndex)
            {
                params.src.image.operationLayout = imageLayouts[srcLayoutIndex].layout;
                params.dst.image.operationLayout = imageLayouts[dstLayoutIndex].layout;
                if (params.src.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ||
                    params.dst.image.operationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                    continue;
                params.samples       = samples[samplesIndex];
                std::string testName = getSampleCountCaseName(samples[samplesIndex]) + "_" +
                                       imageLayouts[srcLayoutIndex].name + "_" + imageLayouts[dstLayoutIndex].name;
                params.imageOffset = false;
                group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), testName, params,
                                                                COPY_MS_IMAGE_TO_MS_IMAGE));
                params.imageOffset = true;
                if (allocationKind != ALLOCATION_KIND_DEDICATED)
                {
                    group->addChild(new ResolveImageToImageTestCase(group->getTestContext(), testName + "_bind_offset",
                                                                    params, COPY_MS_IMAGE_TO_MS_IMAGE));
                }
            }
}

void addResolveImageLayerCopyBeforeResolvingTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                  uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.extent.depth    = 5u;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.extent.depth    = 5u;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    for (uint32_t layerNdx = 0; layerNdx < params.src.image.extent.depth; ++layerNdx)
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            layerNdx,                  // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_LAYER_TO_MS_IMAGE));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_LAYER_TO_MS_IMAGE));
        }
    }
}

void addResolveCopyImageWithRegionsTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                         uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = resolveExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = resolveExtent;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    int32_t imageHalfWidth     = getExtent3D(params.src.image).width / 2;
    int32_t imageHalfHeight    = getExtent3D(params.src.image).height / 2;
    VkExtent3D halfImageExtent = {resolveExtent.width / 2, resolveExtent.height / 2, 1u};

    // Lower right corner to lower left corner.
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,                          // VkImageSubresourceLayers srcSubresource;
            {imageHalfWidth, imageHalfHeight, 0}, // VkOffset3D srcOffset;
            sourceLayer,                          // VkImageSubresourceLayers dstSubresource;
            {0, imageHalfHeight, 0},              // VkOffset3D dstOffset;
            halfImageExtent,                      // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    // Upper right corner to lower right corner.
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,                          // VkImageSubresourceLayers srcSubresource;
            {imageHalfWidth, 0, 0},               // VkOffset3D srcOffset;
            sourceLayer,                          // VkImageSubresourceLayers dstSubresource;
            {imageHalfWidth, imageHalfHeight, 0}, // VkOffset3D dstOffset;
            halfImageExtent,                      // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_MS_IMAGE_MULTIREGION));
        }
    }
}

void addResolveImageWholeArrayImageTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                         uint32_t extensionFlags)
{
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.extent          = defaultExtent;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.extent          = defaultExtent;
    params.dst.image.extent.depth    = 5u;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    for (uint32_t layerNdx = 0; layerNdx < params.dst.image.extent.depth; ++layerNdx)
    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            layerNdx,                  // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
    {
        params.samples     = samples[samplesIndex];
        params.imageOffset = false;
        group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                        getSampleCountCaseName(samples[samplesIndex]), params,
                                                        COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
        params.imageOffset = true;
        if (allocationKind != ALLOCATION_KIND_DEDICATED)
        {
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
        }
    }
}

void addResolveImageWholeArrayImageSingleRegionTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                                     uint32_t extensionFlags)
{
    {
        TestParams params;
        const uint32_t layerCount        = 5u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = allocationKind;
        params.extensionFlags            = extensionFlags;

        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0,                         // uint32_t baseArrayLayer;
            layerCount                 // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);

        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples     = samples[samplesIndex];
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                            getSampleCountCaseName(samples[samplesIndex]), params,
                                                            COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(
                    group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_bind_offset", params,
                    COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            }
        }
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 0u;
        const uint32_t layerCount        = 5u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = allocationKind;
        params.extensionFlags            = extensionFlags;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);

        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples     = samples[samplesIndex];
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_all_remaining_layers",
                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                                getSampleCountCaseName(samples[samplesIndex]) +
                                                                    "_all_remaining_layers_bind_offset",
                                                                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            }
        }
    }

    {
        TestParams params;
        const uint32_t baseLayer         = 2u;
        const uint32_t layerCount        = 5u;
        params.src.image.imageType       = VK_IMAGE_TYPE_2D;
        params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.src.image.extent          = defaultExtent;
        params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
        params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
        params.dst.image.extent          = defaultExtent;
        params.dst.image.extent.depth    = layerCount;
        params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        params.allocationKind            = allocationKind;
        params.extensionFlags            = extensionFlags;
        params.extensionFlags |= MAINTENANCE_5;

        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            baseLayer,                 // uint32_t baseArrayLayer;
            VK_REMAINING_ARRAY_LAYERS  // uint32_t layerCount;
        };

        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            defaultExtent, // VkExtent3D extent;
        };

        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);

        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples     = samples[samplesIndex];
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(
                group->getTestContext(), getSampleCountCaseName(samples[samplesIndex]) + "_not_all_remaining_layers",
                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(group->getTestContext(),
                                                                getSampleCountCaseName(samples[samplesIndex]) +
                                                                    "_not_all_remaining_layers_bind_offset",
                                                                params, COPY_MS_IMAGE_TO_ARRAY_MS_IMAGE));
            }
        }
    }
}

void addResolveImageDiffImageSizeTests(tcu::TestCaseGroup *group, AllocationKind allocationKind,
                                       uint32_t extensionFlags)
{
    tcu::TestContext &testCtx = group->getTestContext();
    TestParams params;
    params.src.image.imageType       = VK_IMAGE_TYPE_2D;
    params.src.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.src.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.src.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    params.dst.image.imageType       = VK_IMAGE_TYPE_2D;
    params.dst.image.format          = VK_FORMAT_R8G8B8A8_UNORM;
    params.dst.image.tiling          = VK_IMAGE_TILING_OPTIMAL;
    params.dst.image.operationLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    params.allocationKind            = allocationKind;
    params.extensionFlags            = extensionFlags;

    {
        const VkImageSubresourceLayers sourceLayer = {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        };
        const VkImageResolve testResolve = {
            sourceLayer,   // VkImageSubresourceLayers srcSubresource;
            {0, 0, 0},     // VkOffset3D srcOffset;
            sourceLayer,   // VkImageSubresourceLayers dstSubresource;
            {0, 0, 0},     // VkOffset3D dstOffset;
            resolveExtent, // VkExtent3D extent;
        };
        CopyRegion imageResolve;
        imageResolve.imageResolve = testResolve;
        params.regions.push_back(imageResolve);
    }

    const VkExtent3D imageExtents[] = {{resolveExtent.width + 10, resolveExtent.height, resolveExtent.depth},
                                       {resolveExtent.width, resolveExtent.height * 2, resolveExtent.depth},
                                       {resolveExtent.width, resolveExtent.height, resolveExtent.depth + 10}};

    for (int srcImageExtentIndex = 0; srcImageExtentIndex < DE_LENGTH_OF_ARRAY(imageExtents); ++srcImageExtentIndex)
    {
        const VkExtent3D &srcImageSize = imageExtents[srcImageExtentIndex];
        params.src.image.extent        = srcImageSize;
        params.dst.image.extent        = resolveExtent;
        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples = samples[samplesIndex];
            std::ostringstream testName;
            testName << "src_" << srcImageSize.width << "_" << srcImageSize.height << "_" << srcImageSize.depth << "_"
                     << getSampleCountCaseName(samples[samplesIndex]);
            std::ostringstream description;
            group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str(), params));
        }
    }
    for (int dstImageExtentIndex = 0; dstImageExtentIndex < DE_LENGTH_OF_ARRAY(imageExtents); ++dstImageExtentIndex)
    {
        const VkExtent3D &dstImageSize = imageExtents[dstImageExtentIndex];
        params.src.image.extent        = resolveExtent;
        params.dst.image.extent        = dstImageSize;
        for (int samplesIndex = 0; samplesIndex < DE_LENGTH_OF_ARRAY(samples); ++samplesIndex)
        {
            params.samples = samples[samplesIndex];
            std::ostringstream testName;
            testName << "dst_" << dstImageSize.width << "_" << dstImageSize.height << "_" << dstImageSize.depth << "_"
                     << getSampleCountCaseName(samples[samplesIndex]);
            std::ostringstream description;
            params.imageOffset = false;
            group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str(), params));
            params.imageOffset = true;
            if (allocationKind != ALLOCATION_KIND_DEDICATED)
            {
                group->addChild(new ResolveImageToImageTestCase(testCtx, testName.str() + "_bind_offset", params));
            }
        }
    }
}

} // namespace

void addResolveImageTests(tcu::TestCaseGroup *group, AllocationKind allocationKind, uint32_t extensionFlags)
{
    addTestGroup(group, "whole", addResolveImageWholeTests, allocationKind, extensionFlags);
    addTestGroup(group, "partial", addResolveImagePartialTests, allocationKind, extensionFlags);
    addTestGroup(group, "with_regions", addResolveImageWithRegionsTests, allocationKind, extensionFlags);
    addTestGroup(group, "whole_copy_before_resolving", addResolveImageWholeCopyBeforeResolvingTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "whole_copy_before_resolving_no_cab", addResolveImageWholeCopyWithoutCabBeforeResolvingTests,
                 allocationKind, extensionFlags);
    addComputeAndTransferQueueTests(group, allocationKind, extensionFlags);
    addTestGroup(group, "diff_layout_copy_before_resolving", addResolveImageWholeCopyDiffLayoutsBeforeResolvingTests,
                 allocationKind, extensionFlags);
    addTestGroup(group, "layer_copy_before_resolving", addResolveImageLayerCopyBeforeResolvingTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "copy_with_regions_before_resolving", addResolveCopyImageWithRegionsTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "whole_array_image", addResolveImageWholeArrayImageTests, allocationKind, extensionFlags);
    addTestGroup(group, "whole_array_image_one_region", addResolveImageWholeArrayImageSingleRegionTests, allocationKind,
                 extensionFlags);
    addTestGroup(group, "diff_image_size", addResolveImageDiffImageSizeTests, allocationKind, extensionFlags);
}

} // namespace api
} // namespace vkt
