/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests for nested command buffers
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktRenderPassGroupParams.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"

namespace vkt
{
namespace renderpass
{

namespace
{

enum Extension
{
    EXT = 0,
    KHR,
};

struct TestParams
{
    SharedGroupParams groupParams;
    Extension ext;
    bool beginInline;
    bool endInline;
};

class NestedCommandBuffersTestInstance : public vkt::TestInstance
{
public:
    NestedCommandBuffersTestInstance(Context &context, const TestParams &testParams)
        : vkt::TestInstance(context)
        , m_testParams(testParams)
    {
    }
    virtual tcu::TestStatus iterate(void);

private:
    void createRenderPass(void);
    void beginRenderPass(void);
    void endRenderPass(void);

    de::MovePtr<vk::ImageWithMemory> m_image;
    vk::Move<vk::VkImageView> m_imageView;
    vk::Move<vk::VkRenderPass> m_renderPass;
    vk::Move<vk::VkFramebuffer> m_framebuffer;
    vk::Move<vk::VkCommandPool> m_cmdPool;
    vk::Move<vk::VkCommandBuffer> m_cmdBuffer;
    const TestParams m_testParams;

    const vk::VkFormat format = vk::VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t width      = 32u;
    const uint32_t height     = 32u;

    const vk::VkImageSubresourceRange outputSubresourceRange =
        makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const vk::VkImageSubresourceLayers outputSubresourceLayers =
        makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
};

void NestedCommandBuffersTestInstance::createRenderPass(void)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDevice device     = m_context.getDevice();
    auto &alloc                   = m_context.getDefaultAllocator();

    const vk::VkImageCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
        DE_NULL,                                 // const void*                pNext
        0u,                                      // VkImageCreateFlags        flags
        vk::VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
        format,                                  // VkFormat                    format
        {width, height, 1u},                     // VkExtent3D                extent
        1u,                                      // uint32_t                    mipLevels
        1u,                                      // uint32_t                    arrayLayers
        vk::VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        vk::VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags        usage
        vk::VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode            sharingMode
        0,                             // uint32_t                    queueFamilyIndexCount
        DE_NULL,                       // const uint32_t*            pQueueFamilyIndices
        vk::VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout            initialLayout
    };

    m_image = de::MovePtr<vk::ImageWithMemory>(
        new vk::ImageWithMemory(vk, device, alloc, createInfo, vk::MemoryRequirement::Any));

    vk::VkImageViewCreateInfo imageViewCreateInfo = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                      // const void* pNext;
        (vk::VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        **m_image,                                    // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        format,                                       // VkFormat format;
        vk::makeComponentMappingRGBA(),               // VkComponentMapping components;
        outputSubresourceRange                        // VkImageSubresourceRange subresourceRange;
    };
    m_imageView = createImageView(vk, device, &imageViewCreateInfo, NULL);

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        vk::VkAttachmentDescription2 attachmentDescription = {
            vk::VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType sType;
            DE_NULL,                                        // const void* pNext;
            (vk::VkAttachmentDescriptionFlags)0u,           // VkAttachmentDescriptionFlags flags;
            format,                                         // VkFormat format;
            vk::VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits samples;
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                // VkAttachmentLoadOp loadOp;
            vk::VK_ATTACHMENT_STORE_OP_STORE,               // VkAttachmentStoreOp storeOp;
            vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,            // VkAttachmentLoadOp stencilLoadOp;
            vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,           // VkAttachmentStoreOp stencilStoreOp;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout initialLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,       // VkImageLayout finalLayout;
        };

        vk::VkAttachmentReference2 colorAttachment = {
            vk::VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType sType;
            DE_NULL,                                      // const void* pNext;
            0u,                                           // uint32_t attachment;
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
            vk::VK_IMAGE_ASPECT_COLOR_BIT,                // VkImageAspectFlags aspectMask;
        };

        vk::VkSubpassDescription2 subpass = {
            vk::VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType sType;
            DE_NULL,                                     // const void* pNext;
            (vk::VkSubpassDescriptionFlags)0u,           // VkSubpassDescriptionFlags flags;
            vk::VK_PIPELINE_BIND_POINT_GRAPHICS,         // VkPipelineBindPoint pipelineBindPoint;
            0x0,                                         // uint32_t viewMask;
            0u,                                          // uint32_t inputAttachmentCount;
            DE_NULL,                                     // const VkAttachmentReference2* pInputAttachments;
            1u,                                          // uint32_t colorAttachmentCount;
            &colorAttachment,                            // const VkAttachmentReference2* pColorAttachments;
            DE_NULL,                                     // const VkAttachmentReference2* pResolveAttachments;
            DE_NULL,                                     // const VkAttachmentReference2* pDepthStencilAttachment;
            0u,                                          // uint32_t preserveAttachmentCount;
            DE_NULL,                                     // const uint32_t* pPreserveAttachments;
        };

        vk::VkRenderPassCreateInfo2 renderPassCreateInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, // VkStructureType sType;
            DE_NULL,                                         // const void* pNext;
            (vk::VkRenderPassCreateFlags)0u,                 // VkRenderPassCreateFlags flags;
            1u,                                              // uint32_t attachmentCount;
            &attachmentDescription,                          // const VkAttachmentDescription2* pAttachments;
            1u,                                              // uint32_t subpassCount;
            &subpass,                                        // const VkSubpassDescription2* pSubpasses;
            0u,                                              // uint32_t dependencyCount;
            DE_NULL,                                         // const VkSubpassDependency2* pDependencies;
            0u,                                              // uint32_t correlatedViewMaskCount;
            DE_NULL,                                         // const uint32_t* pCorrelatedViewMasks;
        };

        m_renderPass = vk::createRenderPass2(vk, device, &renderPassCreateInfo);
    }
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
    {
        vk::VkAttachmentDescription attachmentDescription = {
            (vk::VkAttachmentDescriptionFlags)0u,     // VkAttachmentDescriptionFlags flags;
            format,                                   // VkFormat format;
            vk::VK_SAMPLE_COUNT_1_BIT,                // VkSampleCountFlagBits samples;
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,          // VkAttachmentLoadOp loadOp;
            vk::VK_ATTACHMENT_STORE_OP_STORE,         // VkAttachmentStoreOp storeOp;
            vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,      // VkAttachmentLoadOp stencilLoadOp;
            vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,     // VkAttachmentStoreOp stencilStoreOp;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,            // VkImageLayout initialLayout;
            vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // VkImageLayout finalLayout;
        };

        vk::VkAttachmentReference colorAttachment = {
            0u,                                           // uint32_t attachment;
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
        };

        vk::VkSubpassDescription subpass = {
            (vk::VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags flags;
            vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                                  // uint32_t inputAttachmentCount;
            DE_NULL,                             // const VkAttachmentReference2* pInputAttachments;
            1u,                                  // uint32_t colorAttachmentCount;
            &colorAttachment,                    // const VkAttachmentReference2* pColorAttachments;
            DE_NULL,                             // const VkAttachmentReference2* pResolveAttachments;
            DE_NULL,                             // const VkAttachmentReference2* pDepthStencilAttachment;
            0u,                                  // uint32_t preserveAttachmentCount;
            DE_NULL,                             // const uint32_t* pPreserveAttachments;
        };

        vk::VkRenderPassCreateInfo renderPassCreateInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                       // const void* pNext;
            (vk::VkRenderPassCreateFlags)0u,               // VkRenderPassCreateFlags flags;
            1u,                                            // uint32_t attachmentCount;
            &attachmentDescription,                        // const VkAttachmentDescription* pAttachments;
            1u,                                            // uint32_t subpassCount;
            &subpass,                                      // const VkSubpassDescription2* pSubpasses;
            0u,                                            // uint32_t dependencyCount;
            DE_NULL,                                       // const VkSubpassDependency2* pDependencies;
        };

        m_renderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);
    }

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2 ||
        m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
    {
        m_framebuffer = makeFramebuffer(vk, device, *m_renderPass, *m_imageView, width, height);
    }
}

void NestedCommandBuffersTestInstance::beginRenderPass(void)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    const vk::VkClearValue attachmentClearValue = vk::makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);
    const vk::VkRect2D renderArea               = {{0, 0}, {width, height}};

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        const auto preImageBarrier = makeImageMemoryBarrier(
            vk::VK_ACCESS_NONE, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_IMAGE_LAYOUT_UNDEFINED,
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, **m_image, outputSubresourceRange);
        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (vk::VkDependencyFlags)0u, 0u,
                              (const vk::VkMemoryBarrier *)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier *)DE_NULL, 1u,
                              &preImageBarrier);

        vk::VkRenderingAttachmentInfo colorAttachment = {
            vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
            DE_NULL,                                             // const void* pNext;
            *m_imageView,                                        // VkImageView imageView;
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
            vk::VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
            DE_NULL,                                             // VkImageView resolveImageView;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
            vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp loadOp;
            vk::VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
            attachmentClearValue                                 // VkClearValue clearValue;
        };

        vk::VkRenderingInfo renderingInfo = {
            vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            DE_NULL,
            vk::VK_RENDERING_CONTENTS_INLINE_BIT_EXT |
                vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT, // VkRenderingFlagsKHR flags;
            renderArea,                                                  // VkRect2D renderArea;
            1u,                                                          // uint32_t layerCount;
            0x0,                                                         // uint32_t viewMask;
            1u,                                                          // uint32_t colorAttachmentCount;
            &colorAttachment, // const VkRenderingAttachmentInfoKHR* pColorAttachments;
            DE_NULL,          // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
            DE_NULL,          // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
        };
        vk.cmdBeginRendering(*m_cmdBuffer, &renderingInfo);
    }
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        vk::VkRenderPassBeginInfo renderPassBeginInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
            DE_NULL,                                      // const void* pNext;
            *m_renderPass,                                // VkRenderPass renderPass;
            *m_framebuffer,                               // VkFramebuffer framebuffer;
            renderArea,                                   // VkRect2D renderArea;
            1u,                                           // uint32_t clearValueCount;
            &attachmentClearValue,                        // const VkClearValue* pClearValues;
        };

        vk::VkSubpassBeginInfo subpassBeginInfo = {
            vk::VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,                         // VkStructureType sType;
            DE_NULL,                                                          // const void* pNext;
            vk::VK_SUBPASS_CONTENTS_INLINE_AND_SECONDARY_COMMAND_BUFFERS_KHR, // VkSubpassContents contents;
        };

        vk.cmdBeginRenderPass2(*m_cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);
    }
    else
    {
        vk::VkRenderPassBeginInfo renderPassBeginInfo = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
            DE_NULL,                                      // const void* pNext;
            *m_renderPass,                                // VkRenderPass renderPass;
            *m_framebuffer,                               // VkFramebuffer framebuffer;
            renderArea,                                   // VkRect2D renderArea;
            1u,                                           // uint32_t clearValueCount;
            &attachmentClearValue,                        // const VkClearValue* pClearValues;
        };
        vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo,
                              vk::VK_SUBPASS_CONTENTS_INLINE_AND_SECONDARY_COMMAND_BUFFERS_KHR);
    }
}

void NestedCommandBuffersTestInstance::endRenderPass(void)
{
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        vk.cmdEndRendering(*m_cmdBuffer);

        const auto postImageBarrier =
            makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT,
                                   vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_image, outputSubresourceRange);
        vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u,
                              (const vk::VkMemoryBarrier *)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier *)DE_NULL, 1u,
                              &postImageBarrier);
    }
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        vk::VkSubpassEndInfo subpassEndInfo = {
            vk::VK_STRUCTURE_TYPE_SUBPASS_END_INFO, // VkStructureType sType;
            DE_NULL,                                // const void* pNext;
        };

        vk.cmdEndRenderPass2(*m_cmdBuffer, &subpassEndInfo);
    }
    else
    {
        vk.cmdEndRenderPass(*m_cmdBuffer);
    }
}

tcu::TestStatus NestedCommandBuffersTestInstance::iterate(void)
{
    const vk::InstanceInterface &vki          = m_context.getInstanceInterface();
    const vk::DeviceInterface &vk             = m_context.getDeviceInterface();
    const vk::VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const vk::VkDevice device                 = m_context.getDevice();
    const auto &deviceExtensions              = m_context.getDeviceExtensions();
    const uint32_t queueFamilyIndex           = m_context.getUniversalQueueFamilyIndex();
    const vk::VkQueue queue                   = m_context.getUniversalQueue();
    auto &alloc                               = m_context.getDefaultAllocator();
    tcu::TestLog &log                         = m_context.getTestContext().getLog();

    m_cmdPool   = createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const vk::Move<vk::VkCommandBuffer> secondaries[3] = {
        vk::allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY),
        vk::allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY),
        vk::allocateCommandBuffer(vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY),
    };

    createRenderPass();

    const std::vector<vk::VkViewport> viewports{vk::makeViewport(width, height)};
    const std::vector<vk::VkRect2D> scissors{vk::makeRect2D(width, height)};
    const vk::PipelineLayoutWrapper pipelineLayout(m_testParams.groupParams->pipelineConstructionType, vk, device);

    vk::VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    vk::PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
    if (m_testParams.groupParams->renderingType == RenderingType::RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        pipelineRenderingCreateInfo = {
            vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType    sType
            DE_NULL,                                              // const void*        pNext
            0u,                                                   // uint32_t            viewMask
            1u,                                                   // uint32_t            colorAttachmentCount
            &format,                                              // const VkFormat*    pColorAttachmentFormats
            vk::VK_FORMAT_UNDEFINED,                              // VkFormat            depthAttachmentFormat
            vk::VK_FORMAT_UNDEFINED                               // VkFormat            stencilAttachmentFormat
        };

        renderingCreateInfoWrapper.ptr = &pipelineRenderingCreateInfo;
    }

    vk::ShaderWrapper vert = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
    vk::ShaderWrapper frag = vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

    const vk::VkPipelineVertexInputStateCreateInfo vertexInput = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                       // const void* pNext;
        0u,                                                            // VkPipelineVertexInputStateCreateFlags flags;
        0u,                                                            // uint32_t vertexBindingDescriptionCount;
        DE_NULL, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        0u,      // uint32_t vertexAttributeDescriptionCount;
        DE_NULL, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    vk::GraphicsPipelineWrapper pipeline(vki, vk, physicalDevice, device, deviceExtensions,
                                         vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
    pipeline.setDefaultTopology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultDepthStencilState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInput)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *m_renderPass, 0u, vert, DE_NULL, {}, {},
                                          {}, DE_NULL, DE_NULL, renderingCreateInfoWrapper)
        .setupFragmentShaderState(pipelineLayout, *m_renderPass, 0u, frag)
        .setupFragmentOutputState(*m_renderPass)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    const vk::VkDeviceSize colorOutputBufferSize        = width * height * tcu::getPixelSize(vk::mapVkFormat(format));
    de::MovePtr<vk::BufferWithMemory> colorOutputBuffer = de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
        vk, device, alloc, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible));

    for (uint32_t i = 0; i < 3; ++i)
    {
        vk::VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO, // VkStructureType sType;
            DE_NULL,                                                         // const void* pNext;
            vk::VK_RENDERING_CONTENTS_INLINE_BIT_EXT |
                vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT, // VkRenderingFlags flags;
            0x0,                                                         // uint32_t viewMask;
            1u,                                                          // uint32_t colorAttachmentCount;
            &format,                                                     // const VkFormat* pColorAttachmentFormats;
            vk::VK_FORMAT_UNDEFINED,                                     // VkFormat depthAttachmentFormat;
            vk::VK_FORMAT_UNDEFINED,                                     // VkFormat stencilAttachmentFormat;
            vk::VK_SAMPLE_COUNT_1_BIT,                                   // VkSampleCountFlagBits rasterizationSamples;
        };

        vk::VkCommandBufferInheritanceInfo inheritanceInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
            DE_NULL,                                               // const void* pNext;
            *m_renderPass,                                         // VkRenderPass renderPass;
            0u,                                                    // uint32_t subpass;
            *m_framebuffer,                                        // VkFramebuffer framebuffer;
            VK_FALSE,                                              // VkBool32 occlusionQueryEnable;
            (vk::VkQueryControlFlags)0u,                           // VkQueryControlFlags queryFlags;
            (vk::VkQueryPipelineStatisticFlags)0u,                 // VkQueryPipelineStatisticFlags pipelineStatistics;
        };

        if (m_testParams.groupParams->renderingType == RenderingType::RENDERING_TYPE_DYNAMIC_RENDERING)
        {
            inheritanceInfo.pNext = &inheritanceRenderingInfo;
        }

        vk::VkCommandBufferBeginInfo commandBufferBeginInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
            DE_NULL,                                         // const void* pNext;
            (vk::VkCommandBufferUsageFlags)
                vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, // VkCommandBufferUsageFlags flags;
            &inheritanceInfo, // const VkCommandBufferInheritanceInfo* pInheritanceInfo;
        };

        vk.beginCommandBuffer(*secondaries[i], &commandBufferBeginInfo);
        vk.cmdBindPipeline(*secondaries[i], vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());

        vk.cmdDraw(*secondaries[i], 4u, 1u, 0u, i * 2);
        vk.endCommandBuffer(*secondaries[i]);
    }

    vk::beginCommandBuffer(vk, *m_cmdBuffer);
    vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
    beginRenderPass();

    if (m_testParams.beginInline)
    {
        vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 1u);
    }
    vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*secondaries[0]);
    vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
    if (!m_testParams.beginInline)
    {
        vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 1u);
    }
    vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 3u);
    vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*secondaries[1]);
    vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
    vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 3u);

    if (!m_testParams.endInline)
    {
        vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 5u);
    }
    vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*secondaries[2]);
    if (m_testParams.endInline)
    {
        vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
        vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 5u);
    }

    endRenderPass();
    const vk::VkBufferImageCopy copyRegion = {
        0u,                      // VkDeviceSize bufferOffset;
        0u,                      // uint32_t bufferRowLength;
        0u,                      // uint32_t bufferImageHeight;
        outputSubresourceLayers, // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},               // VkOffset3D imageOffset;
        {width, height, 1}       // VkExtent3D imageExtent;
    };
    vk.cmdCopyImageToBuffer(*m_cmdBuffer, **m_image, vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **colorOutputBuffer, 1u,
                            &copyRegion);
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, queue, m_cmdBuffer.get());

    tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(
        vk::mapVkFormat(format), width, height, 1, (const void *)colorOutputBuffer->getAllocation().getHostPtr());

    const tcu::Vec4 colors[] = {
        tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
        tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };

    for (uint32_t j = 0; j < height; ++j)
    {
        for (uint32_t i = 0; i < width; ++i)
        {
            const tcu::Vec4 color = resultBuffer.getPixel(i, j).asFloat();

            tcu::Vec4 expected;
            // 6 quads are drawn, find the color
            if (i >= width / 2)
            {
                if (j >= height / 2)
                {
                    expected = colors[0];
                    if (j < height / 4 * 3 && !m_testParams.beginInline)
                        expected = colors[1];
                }
                else if (j < height / 2)
                {
                    expected = colors[2];
                }
            }
            else
            {
                if (j >= height / 4 * 3)
                {
                    expected = colors[3];
                }
                else if (j >= height / 4)
                {
                    expected = colors[4];
                    if (j < height / 2 && m_testParams.endInline)
                        expected = colors[5];
                }
                else
                {
                    expected = colors[5];
                }
            }

            if (color != expected)
            {
                log << tcu::TestLog::Message << "Color at (" << i << ", " << j << ") is expected to be (" << expected
                    << "), but was (" << color << ")" << tcu::TestLog::EndMessage;
                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class NestedCommandBuffersTest : public vkt::TestCase
{
public:
    NestedCommandBuffersTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams)
        : vkt::TestCase(testContext, name)
        , m_testParams(testParams)
    {
    }
    virtual void initPrograms(vk::SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const
    {
        return new NestedCommandBuffersTestInstance(context, m_testParams);
    }
    virtual void checkSupport(Context &context) const;

private:
    const TestParams m_testParams;
};

void NestedCommandBuffersTest::initPrograms(vk::SourceCollections &sourceCollections) const
{
    std::stringstream vert;
    std::stringstream frag;

    vert << "#version 450\n"
         << "layout (location=0) flat out uint index;\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    pos.y -= 0.5f * (gl_InstanceIndex % 3);\n"
         << "    pos.x -= 1.0f * (gl_InstanceIndex / 3);\n"
         << "    gl_Position = vec4(pos, 0.0f, 1.0f);\n"
         << "    index = gl_InstanceIndex + 1;\n"
         << "}\n";

    frag << "#version 450\n"
         << "layout (location=0) flat in uint index;\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    float r = bool(index & 4) ? 1.0f : 0.0f;\n"
         << "    float g = bool(index & 2) ? 1.0f : 0.0f;\n"
         << "    float b = bool(index & 1) ? 1.0f : 0.0f;\n"
         << "    outColor = vec4(r, g, b, 1.0f);\n"
         << "}\n";

    sourceCollections.glslSources.add("vert") << glu::VertexSource(vert.str());
    sourceCollections.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void NestedCommandBuffersTest::checkSupport(Context &context) const
{
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    }
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
    }

    if (m_testParams.ext == Extension::EXT)
    {
        context.requireDeviceFunctionality("VK_EXT_nested_command_buffer");

        const auto &features =
            *vk::findStructure<vk::VkPhysicalDeviceNestedCommandBufferFeaturesEXT>(&context.getDeviceFeatures2());
        if (!features.nestedCommandBuffer)
            TCU_THROW(NotSupportedError, "nestedCommandBuffer is not supported");
        if (!features.nestedCommandBufferRendering)
            TCU_THROW(NotSupportedError, "nestedCommandBufferRendering is not supported, so "
                                         "VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT cannot be used");
    }
    else
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance7");

        const auto &features =
            *vk::findStructure<vk::VkPhysicalDeviceMaintenance7FeaturesKHR>(&context.getDeviceFeatures2());
        if (!features.maintenance7)
            TCU_THROW(NotSupportedError, "maintenance7 is not supported");
    }
}

} // namespace

tcu::TestCaseGroup *createNestedCommandBufferTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    de::MovePtr<tcu::TestCaseGroup> nestedCommandBuffersGroup(
        new tcu::TestCaseGroup(testCtx, "nested_command_buffers"));

    constexpr struct ExtTest
    {
        Extension ext;
        const char *name;
    } extensionTests[] = {
        {Extension::EXT, "ext"},
        {Extension::KHR, "khr"},
    };

    constexpr struct CommandTest
    {
        bool secondary;
        const char *name;
    } commandTests[] = {
        {false, "inline_secondary"},
        {true, "secondary_inline"},
    };

    for (const auto &extension : extensionTests)
    {
        tcu::TestCaseGroup *const extensionGroup = new tcu::TestCaseGroup(testCtx, extension.name);

        for (const auto &firstCommand : commandTests)
        {
            tcu::TestCaseGroup *const firstCommandGroup = new tcu::TestCaseGroup(testCtx, firstCommand.name);

            for (const auto &lastCommand : commandTests)
            {
                TestParams params;
                params.groupParams = groupParams;
                params.ext         = extension.ext;
                params.beginInline = !firstCommand.secondary;
                params.endInline   = !lastCommand.secondary;

                firstCommandGroup->addChild(new NestedCommandBuffersTest(testCtx, lastCommand.name, params));
            }
            extensionGroup->addChild(firstCommandGroup);
        }
        nestedCommandBuffersGroup->addChild(extensionGroup);
    }

    return nestedCommandBuffersGroup.release();
}

} // namespace renderpass
} // namespace vkt
