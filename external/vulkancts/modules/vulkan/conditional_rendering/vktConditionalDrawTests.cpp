/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Test for conditional rendering of vkCmdDraw* functions
 *//*--------------------------------------------------------------------*/

#include "vktConditionalDrawTests.hpp"
#include "vktConditionalRenderingTestUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "vkTypeUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"

namespace vkt
{
namespace conditional
{
namespace
{

enum DrawCommandType
{
    DRAW_COMMAND_TYPE_DRAW = 0,
    DRAW_COMMAND_TYPE_DRAW_INDEXED,
    DRAW_COMMAND_TYPE_DRAW_INDIRECT,
    DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT,
    DRAW_COMMAND_TYPE_DRAW_INDIRECT_COUNT,
    DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT,

    DRAW_COMMAND_TYPE_DRAW_LAST
};

const char *getDrawCommandTypeName(DrawCommandType command)
{
    switch (command)
    {
    case DRAW_COMMAND_TYPE_DRAW:
        return "draw";
    case DRAW_COMMAND_TYPE_DRAW_INDEXED:
        return "draw_indexed";
    case DRAW_COMMAND_TYPE_DRAW_INDIRECT:
        return "draw_indirect";
    case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT:
        return "draw_indexed_indirect";
    case DRAW_COMMAND_TYPE_DRAW_INDIRECT_COUNT:
        return "draw_indirect_count";
    case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT:
        return "draw_indexed_indirect_count";
    default:
        DE_ASSERT(false);
    }
    return "";
}

struct ConditionalTestSpec : public Draw::TestSpecBase
{
    DrawCommandType command;
    uint32_t drawCalls;
    ConditionalData conditionalData;
};

class ConditionalDraw : public Draw::DrawTestsBaseClass
{
public:
    typedef ConditionalTestSpec TestSpec;

    ConditionalDraw(Context &context, ConditionalTestSpec testSpec);

    virtual tcu::TestStatus iterate(void);
    void createAndBindIndexBuffer(vk::VkCommandBuffer cmdBuffer);
    void createIndirectBuffer(void);
    void createIndexedIndirectBuffer(void);
    void createIndirectCountBuffer(void);
    void recordDraw(vk::VkCommandBuffer cmdBuffer);

protected:
    void createRenderPassWithClear(void);

    const DrawCommandType m_command;
    const uint32_t m_drawCalls;

    const ConditionalData m_conditionalData;
    de::SharedPtr<Draw::Buffer> m_conditionalBuffer;

    vk::Move<vk::VkCommandBuffer> m_secondaryCmdBuffer;
    vk::Move<vk::VkCommandBuffer> m_nestedCmdBuffer;

    std::vector<uint32_t> m_indexes;
    de::SharedPtr<Draw::Buffer> m_indexBuffer;

    de::SharedPtr<Draw::Buffer> m_indirectBuffer;
    de::SharedPtr<Draw::Buffer> m_indirectCountBuffer;

    // For cases where we want to clear the attachment in the render pass begin operation.
    vk::Move<vk::VkRenderPass> m_rpWithClear;
    vk::Move<vk::VkFramebuffer> m_fbWithClear;
};

void checkSupport(Context &context, DrawCommandType command)
{
    if (command == DRAW_COMMAND_TYPE_DRAW_INDIRECT_COUNT || command == DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT)
        context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");
}

ConditionalDraw::ConditionalDraw(Context &context, ConditionalTestSpec testSpec)
    : Draw::DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX],
                               testSpec.shaders[glu::SHADERTYPE_FRAGMENT],
                               Draw::SharedGroupParams(new Draw::GroupParams{false, false, false, false}),
                               vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    , m_command(testSpec.command)
    , m_drawCalls(testSpec.drawCalls)
    , m_conditionalData(testSpec.conditionalData)
{
    checkConditionalRenderingCapabilities(context, m_conditionalData);
    checkNestedRenderPassCapabilities(context);
    checkSupport(context, m_command);

    const float minX     = -0.3f;
    const float maxX     = 0.3f;
    const float drawStep = 0.6f / static_cast<float>(m_drawCalls);

    for (uint32_t drawIdx = 0; drawIdx < m_drawCalls; drawIdx++)
    {
        const float minY = minX + static_cast<float>(drawIdx) * drawStep;
        const float maxY = minY + drawStep;

        m_data.push_back(Draw::VertexElementData(tcu::Vec4(minX, maxY, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
        m_data.push_back(Draw::VertexElementData(tcu::Vec4(minX, minY, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
        m_data.push_back(Draw::VertexElementData(tcu::Vec4(maxX, maxY, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));

        m_data.push_back(Draw::VertexElementData(tcu::Vec4(minX, minY, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
        m_data.push_back(Draw::VertexElementData(tcu::Vec4(maxX, maxY, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
        m_data.push_back(Draw::VertexElementData(tcu::Vec4(maxX, minY, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
    }

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::RGBA::red().toVec(), 0));

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::RGBA::red().toVec(), 0));

    for (uint32_t index = 0; index < m_data.size(); index++)
    {
        m_indexes.push_back(index);
    }

    initialize();

    DE_ASSERT(!(m_conditionalData.clearInRenderPass && m_conditionalData.conditionInSecondaryCommandBuffer));

    if (m_conditionalData.clearInRenderPass)
        createRenderPassWithClear();

    m_secondaryCmdBuffer =
        vk::allocateCommandBuffer(m_vk, m_context.getDevice(), *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    m_nestedCmdBuffer =
        vk::allocateCommandBuffer(m_vk, m_context.getDevice(), *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

void ConditionalDraw::createRenderPassWithClear(void)
{
    const auto device = m_context.getDevice();
    Draw::RenderPassCreateInfo renderPassCreateInfo;

    renderPassCreateInfo.addAttachment(Draw::AttachmentDescription(
        m_colorAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT,
        vk::VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear with the render pass.
        vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
        vk::VK_IMAGE_LAYOUT_UNDEFINED, vk::VK_IMAGE_LAYOUT_GENERAL));

    const vk::VkAttachmentReference colorAttachmentReference{0, vk::VK_IMAGE_LAYOUT_GENERAL};

    renderPassCreateInfo.addSubpass(Draw::SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                             &colorAttachmentReference, nullptr,
                                                             Draw::AttachmentReference(), 0, nullptr));

    m_rpWithClear = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

    // Framebuffer.
    std::vector<vk::VkImageView> colorAttachments{*m_colorTargetView};
    const Draw::FramebufferCreateInfo framebufferCreateInfo(*m_rpWithClear, colorAttachments, WIDTH, HEIGHT, 1);

    m_fbWithClear = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
}

void ConditionalDraw::createAndBindIndexBuffer(vk::VkCommandBuffer cmdBuffer)
{
    const vk::VkDeviceSize indexDataSize = m_indexes.size() * sizeof(uint32_t);
    m_indexBuffer                        = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(), Draw::BufferCreateInfo(indexDataSize, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *indexBufferPtr = reinterpret_cast<uint8_t *>(m_indexBuffer->getBoundMemory().getHostPtr());
    deMemcpy(indexBufferPtr, &m_indexes[0], static_cast<size_t>(indexDataSize));

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indexBuffer->getBoundMemory());

    const vk::VkBuffer indexBuffer = m_indexBuffer->object();
    m_vk.cmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, vk::VK_INDEX_TYPE_UINT32);
}

void ConditionalDraw::createIndirectBuffer(void)
{
    const vk::VkDrawIndirectCommand badDrawCommand = {
        6u,               // vertexCount
        1u,               // instanceCount
        m_drawCalls * 6u, // firstVertex
        0u                // firstInstance
    };

    std::vector<vk::VkDrawIndirectCommand> drawCommands;
    for (uint32_t drawIdx = 0; drawIdx < m_drawCalls; drawIdx++)
    {
        const vk::VkDrawIndirectCommand goodDrawCommand = {
            6u,           // vertexCount
            1u,           // instanceCount
            6u * drawIdx, // firstVertex
            0u            // firstInstance
        };

        drawCommands.push_back(goodDrawCommand);
        // *Bad* commands should not be rendered by vkCmdDrawIndirectCountKHR
        drawCommands.push_back(badDrawCommand);
        drawCommands.push_back(badDrawCommand);
    }

    const vk::VkDeviceSize drawCommandsSize = drawCommands.size() * sizeof(vk::VkDrawIndirectCommand);

    m_indirectBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(), Draw::BufferCreateInfo(drawCommandsSize, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_indirectBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &drawCommands[0], static_cast<size_t>(drawCommandsSize));

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());
}

void ConditionalDraw::createIndexedIndirectBuffer(void)
{
    const vk::VkDrawIndexedIndirectCommand badDrawCommand = {
        6u,               // indexCount
        1u,               // instanceCount
        m_drawCalls * 6u, // firstIndex
        0u,               // vertexOffset
        0u,               // firstInstance
    };

    std::vector<vk::VkDrawIndexedIndirectCommand> drawCommands;
    for (uint32_t drawIdx = 0; drawIdx < m_drawCalls; drawIdx++)
    {
        const vk::VkDrawIndexedIndirectCommand goodDrawCommand = {
            6u,           // indexCount
            1u,           // instanceCount
            6u * drawIdx, // firstIndex
            0u,           // vertexOffset
            0u,           // firstInstance
        };

        drawCommands.push_back(goodDrawCommand);
        // *Bad* commands should not be rendered by vkCmdDrawIndexedIndirectCountKHR
        drawCommands.push_back(badDrawCommand);
        drawCommands.push_back(badDrawCommand);
    }

    const vk::VkDeviceSize drawCommandsSize = drawCommands.size() * sizeof(vk::VkDrawIndexedIndirectCommand);

    m_indirectBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(), Draw::BufferCreateInfo(drawCommandsSize, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_indirectBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &drawCommands[0], static_cast<size_t>(drawCommandsSize));

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());
}

void ConditionalDraw::createIndirectCountBuffer(void)
{
    m_indirectCountBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(), Draw::BufferCreateInfo(sizeof(uint32_t), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *countBufferPtr       = reinterpret_cast<uint8_t *>(m_indirectCountBuffer->getBoundMemory().getHostPtr());
    *(uint32_t *)(countBufferPtr) = 1;

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectCountBuffer->getBoundMemory());
}

void ConditionalDraw::recordDraw(vk::VkCommandBuffer cmdBuffer)
{
    for (uint32_t drawIdx = 0; drawIdx < m_drawCalls; drawIdx++)
    {
        /* Indirect buffer has next layout:
         * goodCommand badCommand badCommand goodCommand badCommand badCommand ...
         */
        const vk::VkDeviceSize indirectOffset        = sizeof(vk::VkDrawIndirectCommand) * drawIdx * 3;
        const vk::VkDeviceSize indexedIndirectOffset = sizeof(vk::VkDrawIndexedIndirectCommand) * drawIdx * 3;
        switch (m_command)
        {
        case DRAW_COMMAND_TYPE_DRAW:
        {
            m_vk.cmdDraw(cmdBuffer, 6, 1, 6 * drawIdx, 0);
            break;
        }
        case DRAW_COMMAND_TYPE_DRAW_INDEXED:
        {
            m_vk.cmdDrawIndexed(cmdBuffer, 6, 1, 6 * drawIdx, 0, 0);
            break;
        }
        case DRAW_COMMAND_TYPE_DRAW_INDIRECT:
        {
            m_vk.cmdDrawIndirect(cmdBuffer, m_indirectBuffer->object(), indirectOffset, 1, 0);
            break;
        }
        case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT:
        {
            m_vk.cmdDrawIndexedIndirect(cmdBuffer, m_indirectBuffer->object(), indexedIndirectOffset, 1, 0);
            break;
        }
        case DRAW_COMMAND_TYPE_DRAW_INDIRECT_COUNT:
        {
            m_vk.cmdDrawIndirectCount(cmdBuffer, m_indirectBuffer->object(), indirectOffset,
                                      m_indirectCountBuffer->object(), 0, 3, sizeof(vk::VkDrawIndirectCommand));
            break;
        }
        case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT:
        {
            m_vk.cmdDrawIndexedIndirectCount(cmdBuffer, m_indirectBuffer->object(), indexedIndirectOffset,
                                             m_indirectCountBuffer->object(), 0, 3,
                                             sizeof(vk::VkDrawIndexedIndirectCommand));
            break;
        }
        default:
            DE_ASSERT(false);
        }
    }
}

tcu::TestStatus ConditionalDraw::iterate(void)
{
    tcu::TestLog &log         = m_context.getTestContext().getLog();
    const vk::VkQueue queue   = m_context.getUniversalQueue();
    const vk::VkDevice device = m_context.getDevice();

    // We will clear to a different color to be sure.
    const auto clearColor =
        (m_conditionalData.clearInRenderPass ? tcu::RGBA::white().toVec() : tcu::RGBA::black().toVec());

    m_conditionalBuffer = createConditionalRenderingBuffer(m_context, m_conditionalData);

    beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
    preRenderBarriers();

    const bool useSecondaryCmdBuffer =
        m_conditionalData.conditionInherited || m_conditionalData.conditionInSecondaryCommandBuffer;
    const auto subpassContents =
        (useSecondaryCmdBuffer ? vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : vk::VK_SUBPASS_CONTENTS_INLINE);

    if (m_conditionalData.clearInRenderPass)
    {
        // When clearing in the render pass we want to check the render pass clear is executed properly.
        beginConditionalRendering(m_vk, *m_cmdBuffer, *m_conditionalBuffer, m_conditionalData);
        vk::beginRenderPass(m_vk, *m_cmdBuffer, *m_rpWithClear, *m_fbWithClear, vk::makeRect2D(WIDTH, HEIGHT),
                            clearColor, subpassContents);
    }
    else
    {
        beginLegacyRender(*m_cmdBuffer, subpassContents);
    }

    vk::VkCommandBuffer targetCmdBuffer = *m_cmdBuffer;

    if (useSecondaryCmdBuffer)
    {
        const vk::VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT, nullptr,
            m_conditionalData.conditionInherited ? VK_TRUE : VK_FALSE // conditionalRenderingEnable
        };

        const vk::VkCommandBufferInheritanceInfo inheritanceInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
            &conditionalRenderingInheritanceInfo,
            *m_renderPass,                         // renderPass
            0u,                                    // subpass
            *m_framebuffer,                        // framebuffer
            VK_FALSE,                              // occlusionQueryEnable
            (vk::VkQueryControlFlags)0u,           // queryFlags
            (vk::VkQueryPipelineStatisticFlags)0u, // pipelineStatistics
        };

        const vk::VkCommandBufferBeginInfo commandBufferBeginInfo = {
            vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritanceInfo};

        if (m_conditionalData.secondaryCommandBufferNested)
        {
            VK_CHECK(m_vk.beginCommandBuffer(*m_nestedCmdBuffer, &commandBufferBeginInfo));
        }

        VK_CHECK(m_vk.beginCommandBuffer(*m_secondaryCmdBuffer, &commandBufferBeginInfo));

        targetCmdBuffer = *m_secondaryCmdBuffer;
    }

    const vk::VkDeviceSize vertexBufferOffset = 0;
    const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();

    m_vk.cmdBindVertexBuffers(targetCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

    switch (m_command)
    {
    case DRAW_COMMAND_TYPE_DRAW:
    {
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED:
    {
        createAndBindIndexBuffer(targetCmdBuffer);
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDIRECT:
    {
        createIndirectBuffer();
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT:
    {
        createAndBindIndexBuffer(targetCmdBuffer);
        createIndexedIndirectBuffer();
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDIRECT_COUNT:
    {
        createIndirectBuffer();
        createIndirectCountBuffer();
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT:
    {
        createAndBindIndexBuffer(targetCmdBuffer);
        createIndexedIndirectBuffer();
        createIndirectCountBuffer();
        break;
    }
    default:
        DE_ASSERT(false);
    }

    m_vk.cmdBindPipeline(targetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    if (m_conditionalData.conditionInSecondaryCommandBuffer)
    {
        beginConditionalRendering(m_vk, *m_secondaryCmdBuffer, *m_conditionalBuffer, m_conditionalData);
        recordDraw(*m_secondaryCmdBuffer);
        m_vk.cmdEndConditionalRenderingEXT(*m_secondaryCmdBuffer);
        m_vk.endCommandBuffer(*m_secondaryCmdBuffer);
    }
    else if (m_conditionalData.conditionInherited)
    {
        recordDraw(*m_secondaryCmdBuffer);
        m_vk.endCommandBuffer(*m_secondaryCmdBuffer);
    }

    if (useSecondaryCmdBuffer && m_conditionalData.secondaryCommandBufferNested)
    {
        m_vk.cmdExecuteCommands(*m_nestedCmdBuffer, 1, &m_secondaryCmdBuffer.get());
        m_vk.endCommandBuffer(*m_nestedCmdBuffer);
    }

    if (m_conditionalData.conditionInPrimaryCommandBuffer)
    {
        if (!m_conditionalData.clearInRenderPass)
            beginConditionalRendering(m_vk, *m_cmdBuffer, *m_conditionalBuffer, m_conditionalData);

        if (m_conditionalData.conditionInherited)
        {
            if (m_conditionalData.secondaryCommandBufferNested)
            {
                m_vk.cmdExecuteCommands(*m_cmdBuffer, 1, &m_nestedCmdBuffer.get());
            }
            else
            {
                m_vk.cmdExecuteCommands(*m_cmdBuffer, 1, &m_secondaryCmdBuffer.get());
            }
        }
        else
        {
            recordDraw(*m_cmdBuffer);
        }

        if (!m_conditionalData.clearInRenderPass)
            m_vk.cmdEndConditionalRenderingEXT(*m_cmdBuffer);
    }
    else if (useSecondaryCmdBuffer)
    {
        if (m_conditionalData.secondaryCommandBufferNested)
        {
            m_vk.cmdExecuteCommands(*m_cmdBuffer, 1, &m_nestedCmdBuffer.get());
        }
        else
        {
            m_vk.cmdExecuteCommands(*m_cmdBuffer, 1, &m_secondaryCmdBuffer.get());
        }
    }

    if (m_conditionalData.clearInRenderPass)
    {
        // Finish conditional rendering outside the render pass.
        vk::endRenderPass(m_vk, *m_cmdBuffer);
        m_vk.cmdEndConditionalRenderingEXT(*m_cmdBuffer);
    }
    else
    {
        endLegacyRender(*m_cmdBuffer);
    }

    endCommandBuffer(m_vk, *m_cmdBuffer);

    submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

    // Validation
    tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)),
                                  (int)(0.5f + static_cast<float>(HEIGHT)));
    referenceFrame.allocLevel(0);

    const int32_t frameWidth  = referenceFrame.getWidth();
    const int32_t frameHeight = referenceFrame.getHeight();

    tcu::clear(referenceFrame.getLevel(0), clearColor);

    const tcu::Vec4 drawColor      = tcu::RGBA::blue().toVec();
    const tcu::Vec4 referenceColor = m_conditionalData.expectCommandExecution ? drawColor : clearColor;

    Draw::ReferenceImageCoordinates refCoords;

    for (int y = 0; y < frameHeight; y++)
    {
        const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

        for (int x = 0; x < frameWidth; x++)
        {
            const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

            if ((yCoord >= refCoords.bottom && yCoord <= refCoords.top && xCoord >= refCoords.left &&
                 xCoord <= refCoords.right))
                referenceFrame.getLevel(0).setPixel(referenceColor, x, y);
        }
    }

    const vk::VkOffset3D zeroOffset = {0, 0, 0};
    const tcu::ConstPixelBufferAccess renderedFrame =
        m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset,
                                        WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

    qpTestResult res = QP_TEST_RESULT_PASS;

    if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame, 0.05f,
                           tcu::COMPARE_LOG_RESULT))
    {
        res = QP_TEST_RESULT_FAIL;
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

} // namespace

ConditionalDrawTests::ConditionalDrawTests(tcu::TestContext &testCtx) : TestCaseGroup(testCtx, "draw")
{
    /* Left blank on purpose */
}

ConditionalDrawTests::~ConditionalDrawTests(void)
{
}

void ConditionalDrawTests::init(void)
{
    for (int conditionNdx = 0; conditionNdx < DE_LENGTH_OF_ARRAY(conditional::s_testsData); conditionNdx++)
    {
        const ConditionalData &conditionData = conditional::s_testsData[conditionNdx];

        tcu::TestCaseGroup *conditionalDrawRootGroup =
            new tcu::TestCaseGroup(m_testCtx, de::toString(conditionData).c_str());

        for (uint32_t commandTypeIdx = 0; commandTypeIdx < DRAW_COMMAND_TYPE_DRAW_LAST; ++commandTypeIdx)
        {
            const DrawCommandType command = DrawCommandType(commandTypeIdx);

            ConditionalTestSpec testSpec;
            testSpec.command                           = command;
            testSpec.drawCalls                         = 4;
            testSpec.conditionalData                   = conditionData;
            testSpec.shaders[glu::SHADERTYPE_VERTEX]   = "vulkan/dynamic_state/VertexFetch.vert";
            testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";

            conditionalDrawRootGroup->addChild(
                new Draw::InstanceFactory<ConditionalDraw>(m_testCtx, getDrawCommandTypeName(command), testSpec));
        }

        addChild(conditionalDrawRootGroup);
    }
}

} // namespace conditional
} // namespace vkt
