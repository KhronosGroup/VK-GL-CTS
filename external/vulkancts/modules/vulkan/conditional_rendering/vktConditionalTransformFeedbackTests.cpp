/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Google LLC
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

#include "vktConditionalTransformFeedbackTests.hpp"

#include "vktConditionalRenderingTestUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"
#include "vktDrawBaseClass.hpp"
#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "vktDrawCreateInfoUtil.hpp"

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
    DRAW_COMMAND_TYPE_DRAW_MULTI_EXT,
    DRAW_COMMAND_TYPE_DRAW_MULTI_INDEXED_EXT,
    DRAW_COMMAND_TYPE_INDIRECT_BYTE_COUNT_EXT,
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
    case DRAW_COMMAND_TYPE_DRAW_MULTI_EXT:
        return "draw_multi_ext";
    case DRAW_COMMAND_TYPE_DRAW_MULTI_INDEXED_EXT:
        return "draw_multi_indexed_ext";
    case DRAW_COMMAND_TYPE_INDIRECT_BYTE_COUNT_EXT:
        return "draw_indirect_byte_count_ext";
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
};

class ConditionalTransformFeedbackDraw : public Draw::DrawTestsBaseClass
{
public:
    typedef ConditionalTestSpec TestSpec;

    ConditionalTransformFeedbackDraw(Context &context, ConditionalTestSpec testSpec);

    virtual tcu::TestStatus iterate(void);
    void createAndBindIndexBuffer(vk::VkCommandBuffer cmdBuffer);
    void createIndirectBuffer(void);
    void createIndexedIndirectBuffer(void);
    void createIndirectCountBuffer(void);
    void createCountBuffer(void);
    void createXfbBuffer(void);
    void createStreamPipeline(void);
    void recordDraw(vk::VkCommandBuffer cmdBuffer, uint32_t index);

protected:
    const DrawCommandType m_command;

    std::vector<uint32_t> m_indexes;
    de::SharedPtr<Draw::Buffer> m_indexBuffer;

    de::SharedPtr<Draw::Buffer> m_indirectBuffer;
    de::SharedPtr<Draw::Buffer> m_indirectCountBuffer;
    de::SharedPtr<Draw::Buffer> m_countBuffer;

    de::SharedPtr<Draw::Buffer> m_xfbBuffer;
    de::SharedPtr<Draw::Buffer> m_queryBuffer;

    vk::Move<vk::VkPipelineLayout> m_streamPipelineLayout;
    vk::Move<vk::VkPipeline> m_streamPipeline;
};

void checkSupport(Context &context, ConditionalTestSpec testSpec)
{
    context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
    context.requireDeviceFunctionality("VK_EXT_transform_feedback");

    if (context.getConditionalRenderingFeaturesEXT().conditionalRendering == VK_FALSE)
        TCU_THROW(NotSupportedError, "conditionalRendering feature not supported");

    if (testSpec.command == DRAW_COMMAND_TYPE_DRAW_INDIRECT_COUNT ||
        testSpec.command == DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT)
        context.requireDeviceFunctionality("VK_KHR_draw_indirect_count");

    if (testSpec.command == DRAW_COMMAND_TYPE_DRAW_MULTI_EXT ||
        testSpec.command == DRAW_COMMAND_TYPE_DRAW_MULTI_INDEXED_EXT)
        context.requireDeviceFunctionality("VK_EXT_multi_draw");

    if (context.getTransformFeedbackPropertiesEXT().transformFeedbackDraw == VK_FALSE)
        TCU_THROW(NotSupportedError, "transformFeedbackDraw feature not supported");
    if (context.getTransformFeedbackPropertiesEXT().maxTransformFeedbackBuffers < 4)
        TCU_THROW(NotSupportedError, "maxTransformFeedbackBuffers is less than required");
}

ConditionalTransformFeedbackDraw::ConditionalTransformFeedbackDraw(Context &context, ConditionalTestSpec testSpec)
    : Draw::DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX],
                               testSpec.shaders[glu::SHADERTYPE_FRAGMENT],
                               Draw::SharedGroupParams(new Draw::GroupParams{false, false, false, false}),
                               vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    , m_command(testSpec.command)
{
    checkSupport(context, testSpec);

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-0.3f, 0.3f, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-0.3f, -0.3f, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.3f, 0.3f, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-0.3f, -0.3f, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.3f, 0.3f, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.3f, -0.3f, 0.5f, 1.0f), tcu::RGBA::blue().toVec(), 0));

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-0.3f, 0.3f, 0.5f, 1.0f), tcu::RGBA::black().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-0.3f, -0.3f, 0.5f, 1.0f), tcu::RGBA::black().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.3f, 0.3f, 0.5f, 1.0f), tcu::RGBA::black().toVec(), 0));

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(-0.3f, -0.3f, 0.5f, 1.0f), tcu::RGBA::black().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.3f, 0.3f, 0.5f, 1.0f), tcu::RGBA::black().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.3f, -0.3f, 0.5f, 1.0f), tcu::RGBA::black().toVec(), 0));

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(5.3f, 6.3f, 0.5f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(5.3f, 5.3f, 0.5f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(6.3f, 6.3f, 0.5f, 1.0f), tcu::RGBA::red().toVec(), 0));

    m_data.push_back(Draw::VertexElementData(tcu::Vec4(5.3f, 5.3f, 0.5f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(6.3f, 6.3f, 0.5f, 1.0f), tcu::RGBA::red().toVec(), 0));
    m_data.push_back(Draw::VertexElementData(tcu::Vec4(6.3f, 5.3f, 0.5f, 1.0f), tcu::RGBA::red().toVec(), 0));

    for (uint32_t index = 0; index < m_data.size(); index++)
    {
        m_indexes.push_back(index);
    }

    initialize();
}

void ConditionalTransformFeedbackDraw::createAndBindIndexBuffer(vk::VkCommandBuffer cmdBuffer)
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

void ConditionalTransformFeedbackDraw::createIndirectBuffer(void)
{
    const vk::VkDrawIndirectCommand drawCommands[] = {
        {6u, 1u, 0u, 0u},
        {6u, 1u, 6u, 0u},
        {6u, 1u, 12u, 0u},
    };

    m_indirectBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(),
        Draw::BufferCreateInfo(sizeof(drawCommands), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_indirectBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &drawCommands[0], static_cast<size_t>(sizeof(drawCommands)));

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());
}

void ConditionalTransformFeedbackDraw::createIndexedIndirectBuffer(void)
{
    const vk::VkDrawIndexedIndirectCommand drawCommand[] = {
        {
            6u,
            1u,
            0u,
            0u,
            0u,
        },
        {
            6u,
            1u,
            6u,
            0u,
            0u,
        },
        {
            6u,
            1u,
            12u,
            0u,
            0u,
        },
    };

    m_indirectBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(),
        Draw::BufferCreateInfo(sizeof(drawCommand), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_indirectBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, &drawCommand, static_cast<size_t>(sizeof(drawCommand)));

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectBuffer->getBoundMemory());
}

void ConditionalTransformFeedbackDraw::createIndirectCountBuffer(void)
{
    m_indirectCountBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(), Draw::BufferCreateInfo(sizeof(uint32_t), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *countBufferPtr       = reinterpret_cast<uint8_t *>(m_indirectCountBuffer->getBoundMemory().getHostPtr());
    *(uint32_t *)(countBufferPtr) = 1;

    vk::flushAlloc(m_vk, m_context.getDevice(), m_indirectCountBuffer->getBoundMemory());
}

void ConditionalTransformFeedbackDraw::createCountBuffer(void)
{
    m_countBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(),
        Draw::BufferCreateInfo(sizeof(uint32_t) * 2, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint32_t *countBufferPtr = reinterpret_cast<uint32_t *>(m_countBuffer->getBoundMemory().getHostPtr());
    countBufferPtr[0]        = 6u * 4;
    countBufferPtr[1]        = 0u;

    vk::flushAlloc(m_vk, m_context.getDevice(), m_countBuffer->getBoundMemory());
}

void ConditionalTransformFeedbackDraw::createXfbBuffer(void)
{
    const uint32_t output_count = 4 * 6; // 4 stream, 6 points
    m_xfbBuffer                 = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(),
        Draw::BufferCreateInfo(sizeof(float) * output_count, vk::VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    float *xfbBufferPtr = reinterpret_cast<float *>(m_xfbBuffer->getBoundMemory().getHostPtr());
    for (uint32_t i = 0; i < output_count; ++i)
        xfbBufferPtr[i] = 0.0f;

    vk::flushAlloc(m_vk, m_context.getDevice(), m_xfbBuffer->getBoundMemory());
}

void ConditionalTransformFeedbackDraw::createStreamPipeline(void)
{
    using namespace vkt::Draw;

    vk::VkPushConstantRange push_const_range;
    push_const_range.stageFlags = vk::VK_SHADER_STAGE_GEOMETRY_BIT;
    push_const_range.offset     = 0u;
    push_const_range.size       = sizeof(int);

    PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1u;
    pipelineLayoutCreateInfo.pPushConstantRanges    = &push_const_range;
    m_streamPipelineLayout = vk::createPipelineLayout(m_vk, m_context.getDevice(), &pipelineLayoutCreateInfo);

    const vk::Unique<vk::VkShaderModule> vs(
        createShaderModule(m_vk, m_context.getDevice(), m_context.getBinaryCollection().get(m_vertexShaderName), 0));
    std::string geom_name = m_context.getDeviceFeatures().shaderTessellationAndGeometryPointSize ?
                                "VertexFetchWritePoint.geom" :
                                "VertexFetch.geom";
    const vk::Unique<vk::VkShaderModule> gs(
        createShaderModule(m_vk, m_context.getDevice(), m_context.getBinaryCollection().get(geom_name), 0));

    const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

    vk::VkViewport viewport = vk::makeViewport(WIDTH, HEIGHT);
    vk::VkRect2D scissor    = vk::makeRect2D(WIDTH, HEIGHT);

    const vk::VkVertexInputBindingDescription vertexInputBindingDescription = {0, sizeof(VertexElementData),
                                                                               vk::VK_VERTEX_INPUT_RATE_VERTEX};

    const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
        {1u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(sizeof(tcu::Vec4))},
        {2u, 0u, vk::VK_FORMAT_R32_SINT, static_cast<uint32_t>(sizeof(tcu::Vec4)) * 2}};

    const auto vertexInputState = PipelineCreateInfo::VertexInputState(
        1, &vertexInputBindingDescription, DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),
        vertexInputAttributeDescriptions);

    PipelineCreateInfo pipelineCreateInfo(*m_streamPipelineLayout, *m_renderPass, 0, 0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*gs, "main", vk::VK_SHADER_STAGE_GEOMETRY_BIT));
    pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
    pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport),
                                                                  std::vector<vk::VkRect2D>(1, scissor)));
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

#ifndef CTS_USES_VULKANSC
    const auto viewMask = getDefaultViewMask();

    vk::VkPipelineRenderingCreateInfoKHR renderingCreateInfo{vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                             nullptr,
                                                             viewMask,
                                                             1u,
                                                             &m_colorAttachmentFormat,
                                                             vk::VK_FORMAT_UNDEFINED,
                                                             vk::VK_FORMAT_UNDEFINED};

    if (m_groupParams->useDynamicRendering)
        pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

    m_streamPipeline = vk::createGraphicsPipeline(m_vk, m_context.getDevice(), VK_NULL_HANDLE, &pipelineCreateInfo);
}

void ConditionalTransformFeedbackDraw::recordDraw(vk::VkCommandBuffer cmdBuffer, uint32_t index)
{
    const uint32_t firstVertex = 6u * index;
    const uint32_t firstIndex  = 6u * index;
    vk::VkMultiDrawInfoEXT multiDrawInfo;
    multiDrawInfo.firstVertex = firstVertex;
    multiDrawInfo.vertexCount = 6u;
    vk::VkMultiDrawIndexedInfoEXT multiDrawIndexedInfo;
    multiDrawIndexedInfo.firstIndex              = firstVertex;
    multiDrawIndexedInfo.indexCount              = 6u;
    multiDrawIndexedInfo.vertexOffset            = 0u;
    const int32_t vertexOffset                   = 0u;
    const vk::VkDeviceSize indirectOffset        = sizeof(vk::VkDrawIndirectCommand) * index;
    const vk::VkDeviceSize indexedIndirectOffset = sizeof(vk::VkDrawIndexedIndirectCommand) * index;
    switch (m_command)
    {
    case DRAW_COMMAND_TYPE_DRAW:
    {
        m_vk.cmdDraw(cmdBuffer, 6, 1, firstVertex, 0);
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED:
    {
        m_vk.cmdDrawIndexed(cmdBuffer, 6, 1, firstIndex, 0, 0);
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDIRECT:
    {
        m_vk.cmdDrawIndirect(cmdBuffer, m_indirectBuffer->object(), indirectOffset, 1,
                             sizeof(vk::VkDrawIndirectCommand));
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_MULTI_EXT:
    {
        m_vk.cmdDrawMultiEXT(cmdBuffer, 1u, &multiDrawInfo, 1, 0, sizeof(vk::VkMultiDrawInfoEXT));
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_MULTI_INDEXED_EXT:
    {
        m_vk.cmdDrawMultiIndexedEXT(cmdBuffer, 1u, &multiDrawIndexedInfo, 1, 0, sizeof(vk::VkMultiDrawIndexedInfoEXT),
                                    &vertexOffset);
        break;
    }
    case DRAW_COMMAND_TYPE_INDIRECT_BYTE_COUNT_EXT:
    {
        m_vk.cmdDrawIndirectByteCountEXT(cmdBuffer, 1, 0, m_countBuffer->object(), (index - 1u) * 4u, 0u, 4u);
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
                                  m_indirectCountBuffer->object(), 0, 1, sizeof(vk::VkDrawIndirectCommand));
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT_COUNT:
    {
        m_vk.cmdDrawIndexedIndirectCount(cmdBuffer, m_indirectBuffer->object(), indexedIndirectOffset,
                                         m_indirectCountBuffer->object(), 0, 1,
                                         sizeof(vk::VkDrawIndexedIndirectCommand));
        break;
    }
    default:
        DE_ASSERT(false);
    }
}

tcu::TestStatus ConditionalTransformFeedbackDraw::iterate(void)
{
    tcu::TestLog &log         = m_context.getTestContext().getLog();
    const vk::VkQueue queue   = m_context.getUniversalQueue();
    const vk::VkDevice device = m_context.getDevice();

    createStreamPipeline();

    vk::VkQueryPoolCreateInfo queryPoolInfo{
        vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        (vk::VkQueryPoolCreateFlags)0,                // VkQueryPoolCreateFlags flags;
        vk::VK_QUERY_TYPE_OCCLUSION,                  // VkQueryType queryType;
        2u,                                           // uint32_t queryCount;
        0u,                                           // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    vk::Move<vk::VkQueryPool> queryPool = vk::createQueryPool(m_vk, device, &queryPoolInfo);

    m_queryBuffer = Draw::Buffer::createAndAlloc(
        m_vk, m_context.getDevice(),
        Draw::BufferCreateInfo(sizeof(uint32_t) * 2u, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                          vk::VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT),
        m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    createXfbBuffer();

    beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
    preRenderBarriers();

    m_vk.cmdResetQueryPool(*m_cmdBuffer, *queryPool, 0u, 2u);
    beginLegacyRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_INLINE);

    const vk::VkDeviceSize vertexBufferOffset = 0;
    const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();

    m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

    m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    switch (m_command)
    {
    case DRAW_COMMAND_TYPE_DRAW:
    {
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED:
    {
        createAndBindIndexBuffer(*m_cmdBuffer);
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDIRECT:
    {
        createIndirectBuffer();
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT:
    {
        createAndBindIndexBuffer(*m_cmdBuffer);
        createIndexedIndirectBuffer();
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_MULTI_EXT:
    {
        break;
    }
    case DRAW_COMMAND_TYPE_DRAW_MULTI_INDEXED_EXT:
    {
        createAndBindIndexBuffer(*m_cmdBuffer);
        break;
    }
    case DRAW_COMMAND_TYPE_INDIRECT_BYTE_COUNT_EXT:
    {
        createCountBuffer();
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
        createAndBindIndexBuffer(*m_cmdBuffer);
        createIndexedIndirectBuffer();
        createIndirectCountBuffer();
        break;
    }
    default:
        DE_ASSERT(false);
    }

    m_vk.cmdBeginQuery(*m_cmdBuffer, *queryPool, 0u, 0u);
    recordDraw(*m_cmdBuffer, 2);
    m_vk.cmdEndQuery(*m_cmdBuffer, *queryPool, 0u);
    m_vk.cmdBeginQuery(*m_cmdBuffer, *queryPool, 1u, 0u);
    recordDraw(*m_cmdBuffer, 1);
    m_vk.cmdEndQuery(*m_cmdBuffer, *queryPool, 1u);

    endLegacyRender(*m_cmdBuffer);
    m_vk.cmdCopyQueryPoolResults(*m_cmdBuffer, *queryPool, 0u, 2u, m_queryBuffer->object(), 0u, sizeof(uint32_t),
                                 vk::VK_QUERY_RESULT_WAIT_BIT);

    vk::VkBufferMemoryBarrier bufferMemoryBarrier =
        vk::makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
                                    m_queryBuffer->object(), 0u, sizeof(uint32_t) * 2);

    m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                            vk::VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT, 0, 0, nullptr, 1, &bufferMemoryBarrier,
                            0, nullptr);

    vk::VkConditionalRenderingBeginInfoEXT conditionalRenderingBeginInfo;
    conditionalRenderingBeginInfo.sType  = vk::VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    conditionalRenderingBeginInfo.pNext  = nullptr;
    conditionalRenderingBeginInfo.buffer = m_queryBuffer->object();
    conditionalRenderingBeginInfo.offset = sizeof(uint32_t);
    conditionalRenderingBeginInfo.flags  = 0;

    beginLegacyRender(*m_cmdBuffer, vk::VK_SUBPASS_CONTENTS_INLINE);

    m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_streamPipeline);
    const vk::VkDeviceSize xfbSize = sizeof(float) * 6;
    const vk::VkBuffer xfbBuffer   = m_xfbBuffer->object();
    for (uint32_t stream = 0u; stream < 4u; ++stream)
    {
        const vk::VkDeviceSize xfbOffset = stream * sizeof(float) * 6;
        m_vk.cmdBindTransformFeedbackBuffersEXT(*m_cmdBuffer, stream, 1u, &xfbBuffer, &xfbOffset, &xfbSize);
        m_vk.cmdPushConstants(*m_cmdBuffer, *m_streamPipelineLayout, vk::VK_SHADER_STAGE_GEOMETRY_BIT, 0u, sizeof(int),
                              &stream);

        conditionalRenderingBeginInfo.offset = sizeof(uint32_t) * (stream % 2);
        m_vk.cmdBeginConditionalRenderingEXT(*m_cmdBuffer, &conditionalRenderingBeginInfo);
        m_vk.cmdBeginTransformFeedbackEXT(*m_cmdBuffer, 0u, 0u, nullptr, nullptr);
        recordDraw(*m_cmdBuffer, 1u);
        m_vk.cmdEndTransformFeedbackEXT(*m_cmdBuffer, 0u, 0u, nullptr, nullptr);
        m_vk.cmdEndConditionalRenderingEXT(*m_cmdBuffer);
    }
    endLegacyRender(*m_cmdBuffer);
    const vk::VkMemoryBarrier tfMemoryBarrier =
        vk::makeMemoryBarrier(vk::VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, vk::VK_ACCESS_HOST_READ_BIT);
    m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
                            vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(m_vk, *m_cmdBuffer);
    submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

    invalidateAlloc(m_vk, device, m_xfbBuffer->getBoundMemory());

    uint32_t *queryBufferPtr = reinterpret_cast<uint32_t *>(m_queryBuffer->getBoundMemory().getHostPtr());
    float *xfbBufferPtr      = reinterpret_cast<float *>(m_xfbBuffer.get()->getHostPtr());

    if (queryBufferPtr[0] != 0)
    {
        log << tcu::TestLog::Message << "Occlusion query 0 expected result was 0, but query reported "
            << queryBufferPtr[0] << tcu::TestLog::EndMessage;
    }
    else if (queryBufferPtr[1] == 0)
    {
        log << tcu::TestLog::Message << "Occlusion query 1 expected result was not 0, but query reported "
            << queryBufferPtr[1] << tcu::TestLog::EndMessage;
    }
    for (uint32_t i = 0; i < 24; ++i)
    {
        float expected = 0.0f;
        if (i >= 6 && i < 12)
            expected = 2.0f;
        else if (i >= 18)
            expected = 4.0f;
        if (xfbBufferPtr[i] != expected)
        {
            log << tcu::TestLog::Message << "Expected value at index " << i << "was " << expected
                << ", but actual value was " << xfbBufferPtr[i] << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

struct AddProgramsDraw
{
    void init(vk::SourceCollections &sources, ConditionalTestSpec testParams) const
    {
        DE_UNREF(testParams);

        const char *const vertexShader = "#version 450\n"

                                         "layout(location = 0) in vec4 in_position;\n"
                                         "layout(location = 1) in vec4 in_color;\n"

                                         "layout(location = 0) out vec4 out_color;\n"
                                         "out gl_PerVertex{ vec4 gl_Position; };\n"

                                         "void main() {\n"
                                         "    gl_Position = in_position;\n"
                                         "    out_color = in_color;\n"
                                         "}\n";

        sources.glslSources.add("VertexFetch.vert") << glu::VertexSource(vertexShader);

        for (uint32_t i = 0; i < 2; ++i)
        {
            std::string geometryShader =
                "#version 450\n"

                "layout (points) in;\n"
                "layout(points, max_vertices = 1) out;\n"
                "layout(location = 0, stream = 0, xfb_offset = 0, xfb_stride = 4, xfb_buffer = 0) out float output1;\n"
                "layout(location = 1, stream = 1, xfb_offset = 0, xfb_stride = 4, xfb_buffer = 1) out float output2;\n"
                "layout(location = 2, stream = 2, xfb_offset = 0, xfb_stride = 4, xfb_buffer = 2) out float output3;\n"
                "layout(location = 3, stream = 3, xfb_offset = 0, xfb_stride = 4, xfb_buffer = 3) out float output4;\n"
                "layout(push_constant) uniform PushConst {\n"
                "    int stream;\n"
                "} pushConst;\n"

                "void main() {\n"
                "    if (pushConst.stream == 0) {\n"
                "        output1 = 1.0;\n"
                "        EmitStreamVertex(0);\n"
                "        EndStreamPrimitive(0);\n"
                "    }\n"
                "    if (pushConst.stream == 1) {\n"
                "        output2 = 2.0;\n"
                "        EmitStreamVertex(1);\n"
                "        EndStreamPrimitive(1);\n"
                "    }\n"
                "    if (pushConst.stream == 2) {\n"
                "        output3 = 3.0;\n"
                "        EmitStreamVertex(2);\n"
                "        EndStreamPrimitive(2);\n"
                "    }\n"
                "    if (pushConst.stream == 3) {\n"
                "        output4 = 4.0;\n"
                "        EmitStreamVertex(3);\n"
                "        EndStreamPrimitive(3);\n"
                "    }\n";
            if (i == 1)
            {
                geometryShader += "gl_PointSize = 1.0f;\n";
            }
            geometryShader += "}\n";

            std::string name = i == 0 ? "VertexFetch.geom" : "VertexFetchWritePoint.geom";
            sources.glslSources.add(name) << glu::GeometrySource(geometryShader);
        }

        const char *const fragmentShader = "#version 450\n"

                                           "layout(location = 0) in vec4 in_color;\n"
                                           "layout(location = 0) out vec4 out_color;\n"

                                           "void main()\n"
                                           "{\n"
                                           "    out_color = in_color;\n"
                                           "}\n";

        sources.glslSources.add("VertexFetch.frag") << glu::FragmentSource(fragmentShader);
    }
};

} // namespace

ConditionalTransformFeedbackTests::ConditionalTransformFeedbackTests(tcu::TestContext &testCtx)
    : TestCaseGroup(testCtx, "transform_feedback")
{
}

ConditionalTransformFeedbackTests::~ConditionalTransformFeedbackTests(void)
{
}

void ConditionalTransformFeedbackTests::init(void)
{
    for (uint32_t commandTypeIdx = 0; commandTypeIdx < DRAW_COMMAND_TYPE_DRAW_LAST; ++commandTypeIdx)
    {
        const DrawCommandType command = DrawCommandType(commandTypeIdx);

        ConditionalTestSpec testSpec;
        testSpec.command                           = command;
        testSpec.shaders[glu::SHADERTYPE_VERTEX]   = "VertexFetch.vert";
        testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "VertexFetch.frag";

        addChild(new InstanceFactory1WithSupport<ConditionalTransformFeedbackDraw, ConditionalTestSpec,
                                                 FunctionSupport1<ConditionalTestSpec>, AddProgramsDraw>(
            m_testCtx, std::string(getDrawCommandTypeName(command)), AddProgramsDraw(), testSpec,
            FunctionSupport1<ConditionalTestSpec>::Args(checkSupport, testSpec)));
    }
}

} // namespace conditional
} // namespace vkt
