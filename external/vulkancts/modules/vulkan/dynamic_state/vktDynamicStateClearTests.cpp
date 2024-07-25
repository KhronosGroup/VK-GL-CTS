/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 LunarG, Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC
 * Copyright (c) 2023 Nintendo
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
 * \brief Dynamic State Clear Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateClearTests.hpp"

#include "vktDynamicStateBaseClass.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class CmdBaseCase : public DynamicStateBaseClass
{
public:
    CmdBaseCase(Context &context, vk::PipelineConstructionType pipelineConstructionType, const char *vertexShaderName,
                const char *fragmentShaderName)
        : DynamicStateBaseClass(context, pipelineConstructionType, vertexShaderName, fragmentShaderName)
    {
        m_topology = vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

        m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

        m_attachmentState.blendEnable         = VK_TRUE;
        m_attachmentState.srcColorBlendFactor = vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        m_attachmentState.dstColorBlendFactor = vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        m_attachmentState.colorBlendOp        = vk::VK_BLEND_OP_ADD;
        m_attachmentState.srcAlphaBlendFactor = vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        m_attachmentState.dstAlphaBlendFactor = vk::VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
        m_attachmentState.alphaBlendOp        = vk::VK_BLEND_OP_ADD;
    }

    virtual tcu::Texture2D buildReferenceFrame(int lineWidth)
    {
        (void)lineWidth;
        DE_ASSERT(false);
        return tcu::Texture2D(tcu::TextureFormat(), 0, 0);
    }

    virtual void command(bool)
    {
        DE_ASSERT(false);
    }

    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log                           = m_context.getTestContext().getLog();
        const vk::InstanceInterface &vkInstance     = m_context.getInstanceInterface();
        const vk::VkPhysicalDevice vkPhysicalDevice = m_context.getPhysicalDevice();
        const vk::VkQueue queue                     = m_context.getUniversalQueue();
        const vk::VkDevice device                   = m_context.getDevice();

        const float lineWidth = getPhysicalDeviceProperties(vkInstance, vkPhysicalDevice).limits.lineWidthRange[1];

        vk::beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

        // set dynamic states
        const vk::VkViewport viewport = {0.0f, 0.0f, static_cast<float>(WIDTH / 2), static_cast<float>(HEIGHT / 2),
                                         0.0f, 0.0f};
        const vk::VkRect2D scissor    = {{0, 0}, {WIDTH, HEIGHT}};

        setDynamicViewportState(1, &viewport, &scissor);
        setDynamicRasterizationState(lineWidth);
        setDynamicBlendState(0.75f, 0.75f, 0.75f, 0.75f);
        setDynamicDepthStencilState(0.0f, 1.0f);

        const vk::VkExtent3D imageExtent = {WIDTH, HEIGHT, 1};

        vk::VkImageFormatProperties imageFormatProperties(getPhysicalDeviceImageFormatProperties(
            vkInstance, vkPhysicalDevice, m_colorAttachmentFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL,
            vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            0));
        if ((imageFormatProperties.sampleCounts & m_samples) == 0)
            TCU_THROW(NotSupportedError, "Color image type not supported");

        const ImageCreateInfo imageCreateInfo(
            vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, m_samples, vk::VK_IMAGE_TILING_OPTIMAL,
            vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        m_image = Image::createAndAlloc(m_vk, device, imageCreateInfo, m_context.getDefaultAllocator(),
                                        m_context.getUniversalQueueFamilyIndex());

        if (m_samples > 1)
        {
            transition2DImage(m_vk, *m_cmdBuffer, m_image->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT,
                              vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, 0u,
                              vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
            transition2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT,
                              vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, 0u,
                              vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
        }
        else
        {
            transition2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT,
                              vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, 0u,
                              vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
            transition2DImage(m_vk, *m_cmdBuffer, m_image->object(), vk::VK_IMAGE_ASPECT_COLOR_BIT,
                              vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL, 0u,
                              vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
        }

        // should not interfere with dynamic state
        command(false);

        const vk::VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
        beginRenderPassWithClearColor(clearColor, true, true);

        command(true);

        m_pipeline.bind(*m_cmdBuffer);

        const vk::VkDeviceSize vertexBufferOffset = 0;
        const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

        m_vk.cmdDraw(*m_cmdBuffer, 2, 1, 0, 0);

        m_renderPass.end(m_vk, *m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

        // validation
        {
            tcu::Texture2D referenceFrame = buildReferenceFrame(static_cast<int>(lineWidth));

            const vk::VkOffset3D zeroOffset = {0, 0, 0};
            const tcu::ConstPixelBufferAccess renderedFrame =
                m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                                zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

            if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame,
                                   0.05f, tcu::COMPARE_LOG_RESULT))
            {
                return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
            }

            return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
        }
    }

    de::SharedPtr<Draw::Image> m_image;
    vk::VkSampleCountFlagBits m_samples = vk::VK_SAMPLE_COUNT_1_BIT;
};

class ClearTestInstance : public CmdBaseCase
{
public:
    ClearTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType, ShaderMap shaders)
        : CmdBaseCase(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                      shaders[glu::SHADERTYPE_FRAGMENT])
    {
        DynamicStateBaseClass::initialize();
    }

    virtual void command(bool renderPassActive)
    {
        if (renderPassActive)
        {
            // Clear attachment
            vk::VkClearValue clearValue;
            clearValue.color.float32[0]                 = 1.0f;
            clearValue.color.float32[1]                 = 1.0f;
            clearValue.color.float32[2]                 = 1.0f;
            clearValue.color.float32[3]                 = 1.0f;
            const vk::VkClearAttachment clearAttachment = {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                0u,                            // uint32_t                colorAttachment
                clearValue                     // VkClearValue            clearValue
            };
            const vk::VkClearRect rect = {{{0, 0}, {WIDTH, HEIGHT}}, 0, 1};
            m_vk.cmdClearAttachments(*m_cmdBuffer, 1, &clearAttachment, 1, &rect);
        }
    }

    virtual tcu::Texture2D buildReferenceFrame(int lineWidth)
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT);
        referenceFrame.allocLevel(0);

        const int32_t frameWidth  = referenceFrame.getWidth();
        const int32_t frameHeight = referenceFrame.getHeight();

        tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        for (int y = 0; y < frameHeight; y++)
        {
            for (int x = 0; x < frameWidth; x++)
            {
                if (y < frameHeight / 2 && y >= 32 - lineWidth / 2 && y < 32 + lineWidth / 2 && x >= frameWidth / 4 &&
                    x < frameWidth / 2)
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.25f, 0.5f, 0.25f, 0.5f), x, y);
                else
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), x, y);
            }
        }

        return referenceFrame;
    }
};

class BlitTestInstance : public CmdBaseCase
{
public:
    BlitTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType, ShaderMap shaders)
        : CmdBaseCase(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                      shaders[glu::SHADERTYPE_FRAGMENT])
    {
        DynamicStateBaseClass::initialize();
    }

    virtual void command(bool renderPassActive)
    {
        if (!renderPassActive)
        {
            const vk::VkImageBlit blitRegion = {// Src
                                                {
                                                    vk::VK_IMAGE_ASPECT_COLOR_BIT,
                                                    0, // mipLevel
                                                    0, // arrayLayer
                                                    1  // layerCount
                                                },
                                                {
                                                    {0, 0, 0},
                                                    {WIDTH, HEIGHT, 1},
                                                },

                                                // Dst
                                                {
                                                    vk::VK_IMAGE_ASPECT_COLOR_BIT,
                                                    0, // mipLevel
                                                    0, // arrayLayer
                                                    1  // layerCount
                                                },
                                                {{0, 0, 0}, {WIDTH, HEIGHT, 1u}}};
            m_vk.cmdBlitImage(*m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
                              m_image->object(), vk::VK_IMAGE_LAYOUT_GENERAL, 1, &blitRegion, vk::VK_FILTER_NEAREST);
        }
    }

    virtual tcu::Texture2D buildReferenceFrame(int lineWidth)
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT);
        referenceFrame.allocLevel(0);

        const int32_t frameWidth  = referenceFrame.getWidth();
        const int32_t frameHeight = referenceFrame.getHeight();

        tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        for (int y = 0; y < frameHeight; y++)
        {
            for (int x = 0; x < frameWidth; x++)
            {
                if (y < frameHeight / 2 && y >= 32 - lineWidth / 2 && y < 32 + lineWidth / 2 && x >= frameWidth / 4 &&
                    x < frameWidth / 2)
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.25f, 0.0f, 0.5f), x, y);
            }
        }

        return referenceFrame;
    }
};

class CopyTestInstance : public CmdBaseCase
{
public:
    CopyTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType, ShaderMap shaders)
        : CmdBaseCase(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                      shaders[glu::SHADERTYPE_FRAGMENT])
    {
        DynamicStateBaseClass::initialize();
    }

    virtual void command(bool renderPassActive)
    {
        if (!renderPassActive)
        {
            const vk::VkImageSubresourceLayers imgSubResLayers = {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags  aspectMask;
                0u,                            // uint32_t            mipLevel;
                0u,                            // uint32_t            baseArrayLayer;
                1u,                            // uint32_t            layerCount;
            };
            const vk::VkOffset3D offset = {0, 0, 0};
            const vk::VkExtent3D extent = {WIDTH, HEIGHT, 1};

            const vk::VkImageCopy copyRegion = {
                imgSubResLayers, // VkImageSubresourceCopy  srcSubresource;
                offset,          // VkOffset3D              srcOffset;
                imgSubResLayers, // VkImageSubresourceCopy  destSubresource;
                offset,          // VkOffset3D              destOffset;
                extent,          // VkExtent3D              extent;
            };

            m_vk.cmdCopyImage(*m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
                              m_image->object(), vk::VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
        }
    }

    virtual tcu::Texture2D buildReferenceFrame(int lineWidth)
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT);
        referenceFrame.allocLevel(0);

        const int32_t frameWidth  = referenceFrame.getWidth();
        const int32_t frameHeight = referenceFrame.getHeight();

        tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        for (int y = 0; y < frameHeight; y++)
        {
            for (int x = 0; x < frameWidth; x++)
            {
                if (y < frameHeight / 2 && y >= 32 - lineWidth / 2 && y < 32 + lineWidth / 2 && x >= frameWidth / 4 &&
                    x < frameWidth / 2)
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.25f, 0.0f, 0.5f), x, y);
            }
        }

        return referenceFrame;
    }
};

class ResolveTestInstance : public CmdBaseCase
{
public:
    ResolveTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType, ShaderMap shaders)
        : CmdBaseCase(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                      shaders[glu::SHADERTYPE_FRAGMENT])
    {
        DynamicStateBaseClass::initialize();

        m_samples = vk::VK_SAMPLE_COUNT_2_BIT;
    }

    virtual void command(bool renderPassActive)
    {
        if (!renderPassActive)
        {
            const vk::VkImageSubresourceLayers imgSubResLayers = {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags  aspectMask;
                0u,                            // uint32_t            mipLevel;
                0u,                            // uint32_t            baseArrayLayer;
                1u,                            // uint32_t            layerCount;
            };
            const vk::VkOffset3D offset = {0, 0, 0};
            const vk::VkExtent3D extent = {WIDTH, HEIGHT, 1};

            const vk::VkImageResolve resolveRegion = {
                imgSubResLayers, // VkImageSubresourceLayers srcSubresource;
                offset,          // VkOffset3D srcOffset;
                imgSubResLayers, // VkImageSubresourceLayers dstSubresource;
                offset,          // VkOffset3D dstOffset;
                extent,          // VkExtent3D extent;
            };
            m_vk.cmdResolveImage(*m_cmdBuffer, m_image->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                 m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, 1, &resolveRegion);

            const vk::VkImageSubresourceRange subresourceRange = {
                vk::VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                            // uint32_t baseMipLevel;
                1u,                            // uint32_t levelCount;
                0u,                            // uint32_t baseArrayLayer;
                1u,                            // uint32_t layerCount;
            };
            const vk::VkImageMemoryBarrier imageBarrier = {
                vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
                DE_NULL,                                    // const void* pNext;
                vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags srcAccessMask;
                vk::VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags dstAccessMask;
                vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout oldLayout;
                vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout newLayout;
                VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
                VK_QUEUE_FAMILY_IGNORED,                    // uint32_t destQueueFamilyIndex;
                m_image->object(),                          // VkImage image;
                subresourceRange,                           // VkImageSubresourceRange subresourceRange;
            };
            m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    vk::VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u,
                                    &imageBarrier);
        }
    }

    virtual tcu::Texture2D buildReferenceFrame(int lineWidth)
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT);
        referenceFrame.allocLevel(0);

        const int32_t frameWidth  = referenceFrame.getWidth();
        const int32_t frameHeight = referenceFrame.getHeight();

        tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        for (int y = 0; y < frameHeight; y++)
        {
            for (int x = 0; x < frameWidth; x++)
            {
                if (y < frameHeight / 2 && y >= 32 - lineWidth / 2 && y < 32 + lineWidth / 2 && x >= frameWidth / 4 &&
                    x < frameWidth / 2)
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.25f, 0.0f, 0.5f), x, y);
            }
        }

        return referenceFrame;
    }
};

} // namespace

DynamicStateClearTests::DynamicStateClearTests(tcu::TestContext &testCtx,
                                               vk::PipelineConstructionType pipelineConstructionType)
    : TestCaseGroup(testCtx, "image")
    , m_pipelineConstructionType(pipelineConstructionType)
{
    /* Left blank on purpose */
}

DynamicStateClearTests::~DynamicStateClearTests()
{
}

void DynamicStateClearTests::init(void)
{
    ShaderMap shaderPaths;
    shaderPaths[glu::SHADERTYPE_VERTEX]   = "vulkan/dynamic_state/VertexFetch.vert";
    shaderPaths[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";

    // Clear attachment after setting dynamic states
    addChild(new InstanceFactory<ClearTestInstance>(m_testCtx, "clear", m_pipelineConstructionType, shaderPaths));
    // Blit image after setting dynamic states
    addChild(new InstanceFactory<BlitTestInstance>(m_testCtx, "blit", m_pipelineConstructionType, shaderPaths));
    // Copy image after setting dynamic states
    addChild(new InstanceFactory<CopyTestInstance>(m_testCtx, "copy", m_pipelineConstructionType, shaderPaths));
    // Resolve image after setting dynamic states
    addChild(new InstanceFactory<ResolveTestInstance>(m_testCtx, "resolve", m_pipelineConstructionType, shaderPaths));
}

} // namespace DynamicState
} // namespace vkt
