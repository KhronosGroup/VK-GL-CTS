/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 LunarG, Inc.
 * Copyright (c) 2025 Google LLC
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
 * \brief Draw Indirect Instanced Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawIndirectInstancedTests.hpp"

#include "vktDrawCreateInfoUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vkCmdUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "rrShaders.hpp"
#include "rrRenderer.hpp"
#include "tcuImageCompare.hpp"
#include "shader_object/vktShaderObjectCreateUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

struct TestParams
{
    SharedGroupParams groupParams;
    uint32_t drawCount;
};

template <typename T>
de::SharedPtr<Buffer> createAndUploadBuffer(const std::vector<T> data, const vk::DeviceInterface &vk,
                                            const Context &context, vk::VkBufferUsageFlags usage)
{
    const vk::VkDeviceSize dataSize = data.size() * sizeof(T);
    de::SharedPtr<Buffer> buffer =
        Buffer::createAndAlloc(vk, context.getDevice(), BufferCreateInfo(dataSize, usage),
                               context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(buffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &data[0], static_cast<size_t>(dataSize));
    vk::flushAlloc(vk, context.getDevice(), buffer->getBoundMemory());

    return buffer;
}

class TestVertShader : public rr::VertexShader
{
public:
    TestVertShader(int firstInstance) : rr::VertexShader(2, 1), m_firstInstance(firstInstance)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~TestVertShader()
    {
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            const tcu::Vec4 position       = rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx,
                                                                       packets[packetNdx]->vertexNdx, m_firstInstance);
            const tcu::Vec4 color          = rr::readVertexAttribFloat(inputs[1], packets[packetNdx]->instanceNdx,
                                                                       packets[packetNdx]->vertexNdx, m_firstInstance);
            packets[packetNdx]->position   = position;
            packets[packetNdx]->outputs[0] = color;
        }
    }

private:
    const int m_firstInstance;
};

class TestFragShader : public rr::FragmentShader
{
public:
    TestFragShader(void) : rr::FragmentShader(1, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~TestFragShader()
    {
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::FragmentPacket &packet = packets[packetNdx];
            for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
            {
                const tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            }
        }
    }
};

class DrawIndirectInstancedInstance : public TestInstance
{
public:
    DrawIndirectInstancedInstance(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

private:
    void prepareVertexData(int instanceCount, int firstInstance);
    void preRenderCommands(const vk::VkClearValue &clearColor);
    void draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer vertexBuffer, vk::VkBuffer instancedVertexBuffer,
              de::SharedPtr<Buffer> indirectBuffer, uint32_t drawCount);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(vk::VkRenderingFlagsKHR renderingFlags = 0u);
#endif // CTS_USES_VULKANSC

private:
    vk::VkFormat m_colorAttachmentFormat = vk::VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t m_width               = 128u;
    const uint32_t m_height              = 128u;
    const uint32_t m_quadGridSize        = 8u;

    const TestParams m_params;
    const vk::DeviceInterface &m_vk;

    vk::Move<vk::VkPipeline> m_pipeline;
    vk::Move<vk::VkPipelineLayout> m_pipelineLayout;

    de::SharedPtr<Image> m_colorTargetImage;
    vk::Move<vk::VkImageView> m_colorTargetView;

    vk::Move<vk::VkCommandPool> m_cmdPool;
    vk::Move<vk::VkCommandBuffer> m_cmdBuffer;
    vk::Move<vk::VkCommandBuffer> m_secCmdBuffer;

    vk::Move<vk::VkFramebuffer> m_framebuffer;
    vk::Move<vk::VkRenderPass> m_renderPass;

    std::vector<tcu::Vec4> m_vertexPosition;
    std::vector<tcu::Vec4> m_instancedColor;
};

DrawIndirectInstancedInstance::DrawIndirectInstancedInstance(Context &context, TestParams params)
    : TestInstance(context)
    , m_params(params)
    , m_vk(context.getDeviceInterface())
{
    const vk::VkDevice device       = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(0, nullptr);
    m_pipelineLayout = vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

    const vk::VkExtent3D targetImageExtent = {m_width, m_height, 1};
    const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1u,
                                                1u, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                                vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                    vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                    vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(),
                                               m_context.getUniversalQueueFamilyIndex());

    ImageSubresourceRange subresourceRange = ImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT);

    const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                  m_colorAttachmentFormat, subresourceRange);
    m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

    if (!m_params.groupParams->useDynamicRendering)
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(
            m_colorAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
            vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
            vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL));

        const vk::VkAttachmentReference colorAttachmentReference = {0, vk::VK_IMAGE_LAYOUT_GENERAL};

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                           &colorAttachmentReference, nullptr, AttachmentReference(), 0,
                                                           nullptr));

        m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

        // create framebuffer
        std::vector<vk::VkImageView> colorAttachments{*m_colorTargetView};
        const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, colorAttachments, m_width, m_height, 1);
        m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
    }

    const vk::VkVertexInputBindingDescription vertexInputBindingDescription[2] = {
        {
            0u,
            (uint32_t)sizeof(tcu::Vec4),
            vk::VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            1u,
            (uint32_t)sizeof(tcu::Vec4),
            vk::VK_VERTEX_INPUT_RATE_INSTANCE,
        },
    };

    const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
        {
            1u,
            1u,
            vk::VK_FORMAT_R32G32B32A32_SFLOAT,
            0,
        }};

    PipelineCreateInfo::VertexInputState vertexInputState = PipelineCreateInfo::VertexInputState(
        2, vertexInputBindingDescription, DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),
        vertexInputAttributeDescriptions);

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    m_cmdPool   = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
    m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    if (m_params.groupParams->useSecondaryCmdBuffer)
        m_secCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    const vk::Unique<vk::VkShaderModule> vs(
        createShaderModule(m_vk, device, m_context.getBinaryCollection().get("vert"), 0));
    const vk::Unique<vk::VkShaderModule> fs(
        createShaderModule(m_vk, device, m_context.getBinaryCollection().get("frag"), 0));

    const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

    vk::VkViewport viewport = vk::makeViewport(m_width, m_height);
    vk::VkRect2D scissor    = vk::makeRect2D(m_width, m_height);

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
    pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport),
                                                                  std::vector<vk::VkRect2D>(1, scissor)));
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
    pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));

#ifndef CTS_USES_VULKANSC
    vk::VkPipelineRenderingCreateInfoKHR renderingFormatCreateInfo{
        vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        nullptr,
        0u,
        1u,
        &m_colorAttachmentFormat,
        vk::VK_FORMAT_UNDEFINED,
        vk::VK_FORMAT_UNDEFINED};

    if (m_params.groupParams->useDynamicRendering)
    {
        pipelineCreateInfo.pNext = &renderingFormatCreateInfo;
    }
#endif // CTS_USES_VULKANSC

    m_pipeline = vk::createGraphicsPipeline(m_vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus DrawIndirectInstancedInstance::iterate()
{
    const vk::VkQueue queue       = m_context.getUniversalQueue();
    const vk::VkDevice device     = m_context.getDevice();
    tcu::TestLog &log             = m_context.getTestContext().getLog();
    const vk::VkRect2D renderArea = vk::makeRect2D(m_width, m_height);
    qpTestResult res              = QP_TEST_RESULT_PASS;

    const uint32_t instanceCounts[]       = {0u, 1u, 2u, 4u, 20u};
    const uint32_t firstInstanceIndices[] = {1u, 3u, 4u, 20u};

    const vk::VkClearValue clearColor = vk::makeClearValueColor({0.0f, 0.0f, 0.0f, 1.0f});

    for (int instanceCountNdx = 0; instanceCountNdx < DE_LENGTH_OF_ARRAY(instanceCounts); instanceCountNdx++)
    {
        const uint32_t instanceCount = instanceCounts[instanceCountNdx];
        for (int firstInstanceIndexNdx = 0u; firstInstanceIndexNdx < DE_LENGTH_OF_ARRAY(firstInstanceIndices);
             firstInstanceIndexNdx++)
        {
            const uint32_t drawCount     = m_params.drawCount;
            const uint32_t vertexCount   = (uint32_t)m_vertexPosition.size() / drawCount;
            const uint32_t firstInstance = firstInstanceIndices[firstInstanceIndexNdx];
            const uint32_t prepareCount  = de::max(firstInstance + instanceCount * drawCount, 1u);

            std::vector<vk::VkDrawIndirectCommand> drawCommands;
            for (uint32_t i = 0; i < drawCount; i++)
            {
                drawCommands.push_back({
                    vertexCount,     // uint32_t vertexCount;
                    instanceCount,   // uint32_t instanceCount;
                    vertexCount * i, // uint32_t firstVertex;
                    firstInstance    // uint32_t firstInstance;
                });
            }

            prepareVertexData(prepareCount, firstInstance);
            const de::SharedPtr<Buffer> vertexBuffer =
                createAndUploadBuffer(m_vertexPosition, m_vk, m_context, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            const de::SharedPtr<Buffer> instancedVertexBuffer =
                createAndUploadBuffer(m_instancedColor, m_vk, m_context, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            de::SharedPtr<Buffer> indirectBuffer =
                createAndUploadBuffer(drawCommands, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

#ifndef CTS_USES_VULKANSC
            if (m_params.groupParams->useSecondaryCmdBuffer)
            {
                // record secondary command buffer
                if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                {
                    beginSecondaryCmdBuffer(vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
                    beginRendering(m_vk, *m_secCmdBuffer, *m_colorTargetView, renderArea, clearColor,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0u, 1u, 0x0);
                }
                else
                    beginSecondaryCmdBuffer();

                draw(*m_secCmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indirectBuffer,
                     drawCount);

                if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                    endRendering(m_vk, *m_secCmdBuffer);

                endCommandBuffer(m_vk, *m_secCmdBuffer);

                // record primary command buffer
                beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

                preRenderCommands(clearColor);

                if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                {
                    beginRendering(m_vk, *m_cmdBuffer, *m_colorTargetView, renderArea, clearColor,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
                                   vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR, 1u, 0x0);
                }

                m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

                if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                    endRendering(m_vk, *m_cmdBuffer);

                endCommandBuffer(m_vk, *m_cmdBuffer);
            }
            else if (m_params.groupParams->useDynamicRendering)
            {
                beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
                preRenderCommands(clearColor);

                beginRendering(m_vk, *m_cmdBuffer, *m_colorTargetView, renderArea, clearColor,
                               vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0u, 1u, 0x0);
                draw(*m_cmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indirectBuffer, drawCount);
                endRendering(m_vk, *m_cmdBuffer);

                endCommandBuffer(m_vk, *m_cmdBuffer);
            }
#endif // CTS_USES_VULKANSC

            if (!m_params.groupParams->useDynamicRendering)
            {
                beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
                preRenderCommands(clearColor);

                beginRenderPass(m_vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, renderArea);
                draw(*m_cmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indirectBuffer, drawCount);
                endRenderPass(m_vk, *m_cmdBuffer);

                endCommandBuffer(m_vk, *m_cmdBuffer);
            }

            submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
            m_context.resetCommandPoolForVKSC(device, *m_cmdPool);

            // Reference rendering
            tcu::TextureLevel refImage(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + m_width),
                                       (int)(0.5 + m_height));

            tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

            const TestVertShader vertShader(firstInstance);
            const TestFragShader fragShader;
            const rr::Program program(&vertShader, &fragShader);
            const rr::MultisamplePixelBufferAccess colorBuffer =
                rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refImage.getAccess());
            const rr::RenderTarget renderTarget(colorBuffer);
            const rr::RenderState renderState((rr::ViewportState(colorBuffer)),
                                              m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            const rr::Renderer renderer;

            const rr::VertexAttrib vertexAttribs[] = {
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &m_vertexPosition[0]),
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 1, &m_instancedColor[0])};

            const rr::PrimitiveList primitives =
                rr::PrimitiveList(rr::PRIMITIVETYPE_TRIANGLES, (int)m_vertexPosition.size(), 0);
            const rr::DrawCommand command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                          &vertexAttribs[0], primitives);
            renderer.drawInstanced(command, instanceCount);

            const vk::VkOffset3D zeroOffset = {0, 0, 0};
            const tcu::ConstPixelBufferAccess renderedFrame =
                m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                                zeroOffset, m_width, m_height, vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u);

            std::ostringstream resultDesc;
            resultDesc << "Instance count: " << instanceCount << " first instance index: " << firstInstance;

            if (!tcu::fuzzyCompare(log, "Result", resultDesc.str().c_str(), refImage.getAccess(), renderedFrame, 0.05f,
                                   tcu::COMPARE_LOG_RESULT))
                res = QP_TEST_RESULT_FAIL;
        }
    }
    return tcu::TestStatus(res, qpGetTestResultName(res));
}

void DrawIndirectInstancedInstance::prepareVertexData(int instanceCount, int firstInstance)
{
    m_vertexPosition.clear();
    m_instancedColor.clear();

    for (uint32_t y = 0; y < m_quadGridSize; ++y)
    {
        for (uint32_t x = 0; x < m_quadGridSize; ++x)
        {
            const float fx0 = -1.0f + (float)(x + 0) / (float)m_quadGridSize * 2.0f / (float)instanceCount;
            const float fx1 = -1.0f + (float)(x + 1) / (float)m_quadGridSize * 2.0f / (float)instanceCount;
            const float fy0 = -1.0f + (float)(y + 0) / (float)m_quadGridSize * 2.0f;
            const float fy1 = -1.0f + (float)(y + 1) / (float)m_quadGridSize * 2.0f;

            // Vertices of a quad's lower-left triangle: (fx0, fy0), (fx1, fy0) and (fx0, fy1)
            m_vertexPosition.push_back(tcu::Vec4(fx0, fy0, 1.0f, 1.0f));
            m_vertexPosition.push_back(tcu::Vec4(fx1, fy0, 1.0f, 1.0f));
            m_vertexPosition.push_back(tcu::Vec4(fx0, fy1, 1.0f, 1.0f));
        }
    }

    for (int i = 0; i < instanceCount + firstInstance; i++)
    {
        m_instancedColor.push_back(tcu::Vec4((float)i / (float)(instanceCount + firstInstance), 0.0, 0.0, 1.0));
    }
}

void DrawIndirectInstancedInstance::preRenderCommands(const vk::VkClearValue &clearColor)
{
    const ImageSubresourceRange subresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

    initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

    m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor.color,
                            1, &subresourceRange);

    const vk::VkMemoryBarrier memBarrier{
        vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, vk::VK_ACCESS_TRANSFER_WRITE_BIT,
        vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

    m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                            vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, nullptr, 0,
                            nullptr);
}

void DrawIndirectInstancedInstance::draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer vertexBuffer,
                                         vk::VkBuffer instancedVertexBuffer, de::SharedPtr<Buffer> indirectBuffer,
                                         uint32_t drawCount)
{
    m_vk.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    const vk::VkBuffer vertexBuffers[]{vertexBuffer, instancedVertexBuffer};
    const vk::VkDeviceSize vertexBufferOffsets[]{0, 0};

    m_vk.cmdBindVertexBuffers(cmdBuffer, 0, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

    m_vk.cmdDrawIndirect(cmdBuffer, indirectBuffer->object(), 0, drawCount, sizeof(vk::VkDrawIndirectCommand));
}

#ifndef CTS_USES_VULKANSC
void DrawIndirectInstancedInstance::beginSecondaryCmdBuffer(vk::VkRenderingFlagsKHR renderingFlags)
{
    const vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                             // const void* pNext;
        renderingFlags,                                                      // VkRenderingFlagsKHR flags;
        0u,                                                                  // uint32_t viewMask;
        1u,                                                                  // uint32_t colorAttachmentCount;
        &m_colorAttachmentFormat,                                            // const VkFormat* pColorAttachmentFormats;
        vk::VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        vk::VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        vk::VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits rasterizationSamples;
    };

    const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        &inheritanceRenderingInfo,                             // const void* pNext;
        VK_NULL_HANDLE,                                        // VkRenderPass renderPass;
        0u,                                                    // uint32_t subpass;
        VK_NULL_HANDLE,                                        // VkFramebuffer framebuffer;
        VK_FALSE,                                              // VkBool32 occlusionQueryEnable;
        (vk::VkQueryControlFlags)0u,                           // VkQueryControlFlags queryFlags;
        (vk::VkQueryPipelineStatisticFlags)0u                  // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const vk::VkCommandBufferBeginInfo commandBufBeginParams{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        usageFlags,                                      // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    VK_CHECK(m_vk.beginCommandBuffer(*m_secCmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

class DrawIndirectInstancedCase : public TestCase
{
public:
    DrawIndirectInstancedCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual void checkSupport(Context &context) const override;
    virtual void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new DrawIndirectInstancedInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

void DrawIndirectInstancedCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DRAW_INDIRECT_FIRST_INSTANCE);

    if (m_params.groupParams->useDynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

void DrawIndirectInstancedCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string vertSrc = "#version 430\n"
                          "layout(location = 0) in vec4 in_position;\n"
                          "layout(location = 1) in vec4 in_color;\n"
                          "layout(location = 0) out vec4 out_color;\n"
                          "void main() {\n"
                          "    gl_Position  = in_position;\n"
                          "    out_color    = in_color;\n"
                          "}\n";

    std::string fragSrc = "#version 430\n"
                          "layout(location = 0) in vec4 in_color;\n"
                          "layout(location = 0) out vec4 out_color;\n"
                          "void main()\n"
                          "{\n"
                          "    out_color = in_color;\n"
                          "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vertSrc);
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc);
}

} // namespace

tcu::TestCaseGroup *createIndirectInstancedTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    de::MovePtr<tcu::TestCaseGroup> indirectInstancedTests(new tcu::TestCaseGroup(testCtx, "indirect_instanced"));

    struct
    {
        uint32_t drawCount;
        const char *name;
    } drawCountTests[] = {{2, "2"}, {4, "4"}, {16, "16"}};

    for (const auto &drawCountTest : drawCountTests)
    {
        TestParams params;
        params.groupParams = groupParams;
        params.drawCount   = drawCountTest.drawCount;

        indirectInstancedTests->addChild(new DrawIndirectInstancedCase(testCtx, drawCountTest.name, params));
    }

    return indirectInstancedTests.release();
}

} // namespace Draw
} // namespace vkt
