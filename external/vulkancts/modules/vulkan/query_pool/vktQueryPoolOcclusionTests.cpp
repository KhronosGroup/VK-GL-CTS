/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Vulkan Occlusion Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolOcclusionTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include <vector>
#include <memory>

namespace vkt
{

namespace QueryPool
{

using namespace Draw;

namespace
{

vk::Move<vk::VkQueryPool> makeOcclusionQueryPool(const vk::DeviceInterface &vkd, const vk::VkDevice device,
                                                 uint32_t numQueries, bool createReset = false)
{
#ifdef CTS_USES_VULKANSC
    DE_UNREF(createReset);
#endif

    const vk::VkQueryPoolCreateInfo queryPoolCreateInfo = {
        vk::VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        nullptr,
#ifndef CTS_USES_VULKANSC
        vk::VkQueryPoolCreateFlags((createReset) ? vk::VK_QUERY_POOL_CREATE_RESET_BIT_KHR : 0),
#else
        0U,
#endif
        vk::VK_QUERY_TYPE_OCCLUSION,
        numQueries,
        0u,
    };
    return vk::createQueryPool(vkd, device, &queryPoolCreateInfo);
}

struct StateObjects
{
    StateObjects(const vk::DeviceInterface &vk, vkt::Context &context, const int numVertices,
                 vk::VkPrimitiveTopology primitive, const bool noColorAttachments);
    void setVertices(const vk::DeviceInterface &vk, std::vector<tcu::Vec4> vertices);

    enum
    {
        WIDTH  = 128,
        HEIGHT = 128
    };

    vkt::Context &m_context;

    vk::Move<vk::VkPipeline> m_pipeline;
    vk::Move<vk::VkPipelineLayout> m_pipelineLayout;

    de::SharedPtr<Image> m_colorAttachmentImage, m_DepthImage;
    vk::Move<vk::VkImageView> m_attachmentView;
    vk::Move<vk::VkImageView> m_depthView;

    vk::Move<vk::VkRenderPass> m_renderPass;
    vk::Move<vk::VkFramebuffer> m_framebuffer;

    de::SharedPtr<Buffer> m_vertexBuffer;

    vk::VkFormat m_colorAttachmentFormat;
};

StateObjects::StateObjects(const vk::DeviceInterface &vk, vkt::Context &context, const int numVertices,
                           vk::VkPrimitiveTopology primitive, const bool noColorAttachments)
    : m_context(context)
    , m_colorAttachmentFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)

{
    vk::VkFormat depthFormat  = vk::VK_FORMAT_D16_UNORM;
    const vk::VkDevice device = m_context.getDevice();

    vk::VkExtent3D imageExtent = {
        WIDTH,  // width;
        HEIGHT, // height;
        1       // depth;
    };

    ImageCreateInfo depthImageCreateInfo(vk::VK_IMAGE_TYPE_2D, depthFormat, imageExtent, 1, 1,
                                         vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                         vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    m_DepthImage = Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(),
                                         m_context.getUniversalQueueFamilyIndex());

    // Construct a depth  view from depth image
    const ImageViewCreateInfo depthViewInfo(m_DepthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, depthFormat);
    m_depthView = vk::createImageView(vk, device, &depthViewInfo);

    // Renderpass and Framebuffer
    if (noColorAttachments)
    {
        RenderPassCreateInfo renderPassCreateInfo;

        renderPassCreateInfo.addAttachment(
            AttachmentDescription(depthFormat,                                            // format
                                  vk::VK_SAMPLE_COUNT_1_BIT,                              // samples
                                  vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                        // loadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // storeOp
                                  vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,                    // stencilLoadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // stencilLoadOp
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   // initialLauout
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)); // finalLayout

        const vk::VkAttachmentReference depthAttachmentReference = {
            0,                                                   // attachment
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // layout
        };

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
                                                           0,                                   // flags
                                                           0,                                   // inputCount
                                                           nullptr,                             // pInputAttachments
                                                           0,                                   // colorCount
                                                           nullptr,                             // pColorAttachments
                                                           nullptr,                             // pResolveAttachments
                                                           depthAttachmentReference, // depthStencilAttachment
                                                           0,                        // preserveCount
                                                           nullptr));                // preserveAttachments

        m_renderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);

        std::vector<vk::VkImageView> attachments(1);
        attachments[0] = *m_depthView;
        FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
        m_framebuffer = vk::createFramebuffer(vk, device, &framebufferCreateInfo);
    }
    else
    {
        const ImageCreateInfo colorImageCreateInfo(
            vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT,
            vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        m_colorAttachmentImage =
            Image::createAndAlloc(vk, device, colorImageCreateInfo, m_context.getDefaultAllocator(),
                                  m_context.getUniversalQueueFamilyIndex());

        const ImageViewCreateInfo attachmentViewInfo(m_colorAttachmentImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                     m_colorAttachmentFormat);

        m_attachmentView = vk::createImageView(vk, device, &attachmentViewInfo);

        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,              // format
                                                                 vk::VK_SAMPLE_COUNT_1_BIT,            // samples
                                                                 vk::VK_ATTACHMENT_LOAD_OP_CLEAR,      // loadOp
                                                                 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE, // storeOp
                                                                 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
                                                                 vk::VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilLoadOp
                                                                 vk::VK_IMAGE_LAYOUT_GENERAL,          // initialLauout
                                                                 vk::VK_IMAGE_LAYOUT_GENERAL));        // finalLayout

        renderPassCreateInfo.addAttachment(
            AttachmentDescription(depthFormat,                                            // format
                                  vk::VK_SAMPLE_COUNT_1_BIT,                              // samples
                                  vk::VK_ATTACHMENT_LOAD_OP_CLEAR,                        // loadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // storeOp
                                  vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,                    // stencilLoadOp
                                  vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,                   // stencilLoadOp
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,   // initialLauout
                                  vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)); // finalLayout

        const vk::VkAttachmentReference colorAttachmentReference = {
            0,                          // attachment
            vk::VK_IMAGE_LAYOUT_GENERAL // layout
        };

        const vk::VkAttachmentReference depthAttachmentReference = {
            1,                                                   // attachment
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // layout
        };

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
                                                           0,                                   // flags
                                                           0,                                   // inputCount
                                                           nullptr,                             // pInputAttachments
                                                           1,                                   // colorCount
                                                           &colorAttachmentReference,           // pColorAttachments
                                                           nullptr,                             // pResolveAttachments
                                                           depthAttachmentReference, // depthStencilAttachment
                                                           0,                        // preserveCount
                                                           nullptr));                // preserveAttachments

        m_renderPass = vk::createRenderPass(vk, device, &renderPassCreateInfo);

        std::vector<vk::VkImageView> attachments(2);
        attachments[0] = *m_attachmentView;
        attachments[1] = *m_depthView;

        FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
        m_framebuffer = vk::createFramebuffer(vk, device, &framebufferCreateInfo);
    }

    {
        // Pipeline

        vk::Unique<vk::VkShaderModule> vs(
            vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
        vk::Unique<vk::VkShaderModule> fs(
            vk::createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

        const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

        const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
        m_pipelineLayout = vk::createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

        const vk::VkVertexInputBindingDescription vf_binding_desc = {
            0,                              // binding;
            4 * (uint32_t)sizeof(float),    // stride;
            vk::VK_VERTEX_INPUT_RATE_VERTEX // inputRate
        };

        const vk::VkVertexInputAttributeDescription vf_attribute_desc = {
            0,                                 // location;
            0,                                 // binding;
            vk::VK_FORMAT_R32G32B32A32_SFLOAT, // format;
            0                                  // offset;
        };

        const vk::VkPipelineVertexInputStateCreateInfo vf_info = {
            // sType;
            vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // pNext;
            NULL,                                                          // flags;
            0u,                                                            // vertexBindingDescriptionCount;
            1,                                                             // pVertexBindingDescriptions;
            &vf_binding_desc,                                              // vertexAttributeDescriptionCount;
            1,                                                             // pVertexAttributeDescriptions;
            &vf_attribute_desc};

        PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
        pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(primitive));
        pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));
        const vk::VkViewport viewport = vk::makeViewport(WIDTH, HEIGHT);
        const vk::VkRect2D scissor    = vk::makeRect2D(WIDTH, HEIGHT);
        pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport),
                                                                      std::vector<vk::VkRect2D>(1, scissor)));
        pipelineCreateInfo.addState(
            PipelineCreateInfo::DepthStencilState(true, true, vk::VK_COMPARE_OP_GREATER_OR_EQUAL));
        pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
        pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
        pipelineCreateInfo.addState(vf_info);
        m_pipeline = vk::createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    {
        // Vertex buffer
        const size_t kBufferSize = numVertices * sizeof(tcu::Vec4);
        m_vertexBuffer =
            Buffer::createAndAlloc(vk, device, BufferCreateInfo(kBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                   m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
    }
}

void StateObjects::setVertices(const vk::DeviceInterface &vk, std::vector<tcu::Vec4> vertices)
{
    const vk::VkDevice device = m_context.getDevice();

    tcu::Vec4 *ptr = reinterpret_cast<tcu::Vec4 *>(m_vertexBuffer->getBoundMemory().getHostPtr());
    std::copy(vertices.begin(), vertices.end(), ptr);

    vk::flushAlloc(vk, device, m_vertexBuffer->getBoundMemory());
}

enum OcclusionQueryResultSize
{
    RESULT_SIZE_64_BIT,
    RESULT_SIZE_32_BIT,
};

enum OcclusionQueryWait
{
    WAIT_QUEUE,
    WAIT_QUERY,
    WAIT_NONE
};

enum OcclusionQueryResultsMode
{
    RESULTS_MODE_GET,
    RESULTS_MODE_GET_RESET,
    RESULTS_MODE_GET_CREATE_RESET,
    RESULTS_MODE_COPY,
    RESULTS_MODE_COPY_RESET
};

enum OcculusionQueryClearOp
{
    CLEAR_NOOP,
    CLEAR_COLOR,
    CLEAR_DEPTH
};

enum OcclusionQueryStride
{
    STRIDE_RESULT_SIZE,
    STRIDE_ZERO,
    STRIDE_MAX,
};

struct OcclusionQueryTestVector
{
    vk::VkQueryControlFlags queryControlFlags;
    OcclusionQueryResultSize queryResultSize;
    OcclusionQueryWait queryWait;
    OcclusionQueryResultsMode queryResultsMode;
    vk::VkDeviceSize queryResultsStride;
    bool queryResultsAvailability;
    vk::VkPrimitiveTopology primitiveTopology;
    bool discardHalf;
    bool queryResultsDstOffset;
    OcculusionQueryClearOp clearOp;
    bool noColorAttachments;
    OcclusionQueryStride stride;
};

class BasicOcclusionQueryTestInstance : public vkt::TestInstance
{
public:
    BasicOcclusionQueryTestInstance(vkt::Context &context, const OcclusionQueryTestVector &testVector);
    ~BasicOcclusionQueryTestInstance(void)
    {
    }

private:
    tcu::TestStatus iterate(void);

    enum
    {
        NUM_QUERIES_IN_POOL          = 2,
        QUERY_INDEX_CAPTURE_EMPTY    = 0,
        QUERY_INDEX_CAPTURE_DRAWCALL = 1,
        NUM_VERTICES_IN_DRAWCALL     = 3
    };

    OcclusionQueryTestVector m_testVector;
    std::unique_ptr<StateObjects> m_stateObjects;
    vk::Move<vk::VkQueryPool> m_queryPool;
};

BasicOcclusionQueryTestInstance::BasicOcclusionQueryTestInstance(vkt::Context &context,
                                                                 const OcclusionQueryTestVector &testVector)
    : TestInstance(context)
    , m_testVector(testVector)
    , m_queryPool()
{
    DE_ASSERT(
        testVector.queryResultSize == RESULT_SIZE_64_BIT && testVector.queryWait == WAIT_QUEUE &&
        (testVector.queryResultsMode == RESULTS_MODE_GET || testVector.queryResultsMode == RESULTS_MODE_GET_RESET) &&
        testVector.queryResultsStride == sizeof(uint64_t) && testVector.queryResultsAvailability == false &&
        testVector.primitiveTopology == vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    if ((m_testVector.queryControlFlags & vk::VK_QUERY_CONTROL_PRECISE_BIT) &&
        !m_context.getDeviceFeatures().occlusionQueryPrecise)
        throw tcu::NotSupportedError("Precise occlusion queries are not supported");

    m_stateObjects.reset(new StateObjects(m_context.getDeviceInterface(), m_context, NUM_VERTICES_IN_DRAWCALL,
                                          m_testVector.primitiveTopology, m_testVector.noColorAttachments));

    const vk::VkDevice device     = m_context.getDevice();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    m_queryPool = makeOcclusionQueryPool(vk, device, NUM_QUERIES_IN_POOL);

    std::vector<tcu::Vec4> vertices(NUM_VERTICES_IN_DRAWCALL);
    vertices[0] = tcu::Vec4(0.5, 0.5, 0.0, 1.0);
    vertices[1] = tcu::Vec4(0.5, 0.0, 0.0, 1.0);
    vertices[2] = tcu::Vec4(0.0, 0.5, 0.0, 1.0);
    m_stateObjects->setVertices(vk, vertices);
}

tcu::TestStatus BasicOcclusionQueryTestInstance::iterate(void)
{
    tcu::TestLog &log             = m_context.getTestContext().getLog();
    const vk::VkDevice device     = m_context.getDevice();
    const vk::VkQueue queue       = m_context.getUniversalQueue();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    if (m_testVector.queryResultsMode == RESULTS_MODE_GET_RESET)
    {
        // Check VK_EXT_host_query_reset is supported
        m_context.requireDeviceFunctionality("VK_EXT_host_query_reset");
        if (m_context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
            throw tcu::NotSupportedError(
                std::string("Implementation doesn't support resetting queries from the host").c_str());
    }

    const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
    vk::Move<vk::VkCommandPool> cmdPool = vk::createCommandPool(vk, device, &cmdPoolCreateInfo);

    vk::Unique<vk::VkCommandBuffer> cmdBuffer(
        vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    if (!m_testVector.noColorAttachments)
        initialTransitionColor2DImage(vk, *cmdBuffer, m_stateObjects->m_colorAttachmentImage->object(),
                                      vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    initialTransitionDepth2DImage(
        vk, *cmdBuffer, m_stateObjects->m_DepthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    std::vector<vk::VkClearValue> renderPassClearValues(2);
    deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(vk::VkClearValue));

    if (m_testVector.queryResultsMode != RESULTS_MODE_GET_RESET)
        vk.cmdResetQueryPool(*cmdBuffer, *m_queryPool, 0, NUM_QUERIES_IN_POOL);

    beginRenderPass(vk, *cmdBuffer, *m_stateObjects->m_renderPass, *m_stateObjects->m_framebuffer,
                    vk::makeRect2D(0, 0, StateObjects::WIDTH, StateObjects::HEIGHT),
                    (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_pipeline);

    vk::VkBuffer vertexBuffer                 = m_stateObjects->m_vertexBuffer->object();
    const vk::VkDeviceSize vertexBufferOffset = 0;
    vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

    vk.cmdBeginQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_EMPTY, m_testVector.queryControlFlags);
    vk.cmdEndQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_EMPTY);

    vk.cmdBeginQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_DRAWCALL, m_testVector.queryControlFlags);
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, 0, 0);
    vk.cmdEndQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_DRAWCALL);

    endRenderPass(vk, *cmdBuffer);

    if (!m_testVector.noColorAttachments)
        transition2DImage(vk, *cmdBuffer, m_stateObjects->m_colorAttachmentImage->object(),
                          vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_GENERAL,
                          vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

    endCommandBuffer(vk, *cmdBuffer);

    if (m_testVector.queryResultsMode == RESULTS_MODE_GET_RESET)
        vk.resetQueryPool(device, *m_queryPool, 0, NUM_QUERIES_IN_POOL);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    uint64_t queryResults[NUM_QUERIES_IN_POOL] = {0};
    size_t queryResultsSize                    = sizeof(queryResults);

    vk::VkResult queryResult = vk::VK_NOT_READY;

    if (m_testVector.stride == STRIDE_RESULT_SIZE)
    {
        queryResult = vk.getQueryPoolResults(device, *m_queryPool, 0, NUM_QUERIES_IN_POOL, queryResultsSize,
                                             queryResults, sizeof(queryResults[0]), vk::VK_QUERY_RESULT_64_BIT);
    }
    else
    {
        for (uint32_t i = 0; i < NUM_QUERIES_IN_POOL; ++i)
        {
            if (m_testVector.stride == STRIDE_ZERO)
            {
                queryResult = vk.getQueryPoolResults(device, *m_queryPool, i, 1, queryResultsSize, &queryResults[i], 0,
                                                     vk::VK_QUERY_RESULT_64_BIT);
            }
            else if (m_testVector.stride == STRIDE_MAX)
            {
                const uint64_t stride = std::numeric_limits<uint64_t>::max() / 8u * 8u;
                queryResult = vk.getQueryPoolResults(device, *m_queryPool, i, 1, queryResultsSize, &queryResults[i],
                                                     stride, vk::VK_QUERY_RESULT_64_BIT);
            }
        }
    }

    if (queryResult == vk::VK_NOT_READY)
    {
        TCU_FAIL("Query result not avaliable, but vkWaitIdle() was called.");
    }

    VK_CHECK(queryResult);

    log << tcu::TestLog::Section("OcclusionQueryResults", "Occlusion query results");
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(queryResults); ++ndx)
    {
        log << tcu::TestLog::Message << "query[slot == " << ndx << "] result == " << queryResults[ndx]
            << tcu::TestLog::EndMessage;
    }

    bool passed = true;

    for (int queryNdx = 0; queryNdx < DE_LENGTH_OF_ARRAY(queryResults); ++queryNdx)
    {

        uint64_t expectedValue;

        switch (queryNdx)
        {
        case QUERY_INDEX_CAPTURE_EMPTY:
            expectedValue = 0;
            break;
        case QUERY_INDEX_CAPTURE_DRAWCALL:
            expectedValue = NUM_VERTICES_IN_DRAWCALL;
            break;
        }

        if ((m_testVector.queryControlFlags & vk::VK_QUERY_CONTROL_PRECISE_BIT) || expectedValue == 0)
        {
            // require precise value
            if (queryResults[queryNdx] != expectedValue)
            {
                log << tcu::TestLog::Message
                    << "vkGetQueryPoolResults returned "
                       "wrong value of query for index "
                    << queryNdx << ", expected " << expectedValue << ", got " << queryResults[0] << "."
                    << tcu::TestLog::EndMessage;
                passed = false;
            }
        }
        else
        {
            // require imprecize value > 0
            if (queryResults[queryNdx] == 0)
            {
                log << tcu::TestLog::Message
                    << "vkGetQueryPoolResults returned "
                       "wrong value of query for index "
                    << queryNdx << ", expected any non-zero value, got " << queryResults[0] << "."
                    << tcu::TestLog::EndMessage;
                passed = false;
            }
        }
    }
    log << tcu::TestLog::EndSection;

    if (passed)
    {
        return tcu::TestStatus(QP_TEST_RESULT_PASS, "Query result verification passed");
    }
    return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Query result verification failed");
}

class OcclusionQueryTestInstance : public vkt::TestInstance
{
public:
    OcclusionQueryTestInstance(vkt::Context &context, const OcclusionQueryTestVector &testVector);
    ~OcclusionQueryTestInstance(void)
    {
    }

private:
    tcu::TestStatus iterate(void);

    bool hasSeparateResetCmdBuf(void) const;
    bool hasSeparateCopyCmdBuf(void) const;
    void commandClearAttachment(const vk::DeviceInterface &vk, const vk::VkCommandBuffer commandBuffer);

    vk::Move<vk::VkCommandBuffer> recordQueryPoolReset(vk::VkCommandPool commandPool);
    vk::Move<vk::VkCommandBuffer> recordRender(vk::VkCommandPool commandPool);
    vk::Move<vk::VkCommandBuffer> recordCopyResults(vk::VkCommandPool commandPool);

    void captureResults(uint64_t *retResults, uint64_t *retAvailability, bool allowNotReady);
    void logResults(const uint64_t *results, const uint64_t *availability);
    bool validateResults(const uint64_t *results, const uint64_t *availability, bool allowUnavailable,
                         vk::VkPrimitiveTopology primitiveTopology);

    enum
    {
        NUM_QUERIES_IN_POOL                    = 3,
        QUERY_INDEX_CAPTURE_ALL                = 0,
        QUERY_INDEX_CAPTURE_PARTIALLY_OCCLUDED = 1,
        QUERY_INDEX_CAPTURE_OCCLUDED           = 2
    };
    enum
    {
        NUM_VERTICES_IN_DRAWCALL                    = 3,
        NUM_VERTICES_IN_PARTIALLY_OCCLUDED_DRAWCALL = 3,
        NUM_VERTICES_IN_OCCLUDER_DRAWCALL           = 3,
        NUM_VERTICES =
            NUM_VERTICES_IN_DRAWCALL + NUM_VERTICES_IN_PARTIALLY_OCCLUDED_DRAWCALL + NUM_VERTICES_IN_OCCLUDER_DRAWCALL
    };
    enum
    {
        START_VERTEX                    = 0,
        START_VERTEX_PARTIALLY_OCCLUDED = START_VERTEX + NUM_VERTICES_IN_DRAWCALL,
        START_VERTEX_OCCLUDER           = START_VERTEX_PARTIALLY_OCCLUDED + NUM_VERTICES_IN_PARTIALLY_OCCLUDED_DRAWCALL
    };

    OcclusionQueryTestVector m_testVector;

    const vk::VkQueryResultFlags m_queryResultFlags;

    std::unique_ptr<StateObjects> m_stateObjects;
    vk::Move<vk::VkQueryPool> m_queryPool;
    de::SharedPtr<Buffer> m_queryPoolResultsBuffer;

    vk::Move<vk::VkCommandPool> m_commandPool;
    vk::Move<vk::VkCommandBuffer> m_queryPoolResetCommandBuffer;
    vk::Move<vk::VkCommandBuffer> m_renderCommandBuffer;
    vk::Move<vk::VkCommandBuffer> m_copyResultsCommandBuffer;
};

OcclusionQueryTestInstance::OcclusionQueryTestInstance(vkt::Context &context,
                                                       const OcclusionQueryTestVector &testVector)
    : vkt::TestInstance(context)
    , m_testVector(testVector)
    , m_queryResultFlags(
          ((m_testVector.queryWait == WAIT_QUERY && m_testVector.queryResultsMode != RESULTS_MODE_COPY_RESET) ?
               vk::VK_QUERY_RESULT_WAIT_BIT :
               0) |
          (m_testVector.queryResultSize == RESULT_SIZE_64_BIT ? vk::VK_QUERY_RESULT_64_BIT : 0) |
          (m_testVector.queryResultsAvailability ? vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT : 0))
{
    const vk::VkDevice device     = m_context.getDevice();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    if ((m_testVector.queryControlFlags & vk::VK_QUERY_CONTROL_PRECISE_BIT) &&
        !m_context.getDeviceFeatures().occlusionQueryPrecise)
        throw tcu::NotSupportedError("Precise occlusion queries are not supported");

    m_stateObjects.reset(new StateObjects(m_context.getDeviceInterface(), m_context,
                                          NUM_VERTICES_IN_DRAWCALL + NUM_VERTICES_IN_PARTIALLY_OCCLUDED_DRAWCALL +
                                              NUM_VERTICES_IN_OCCLUDER_DRAWCALL,
                                          m_testVector.primitiveTopology, m_testVector.noColorAttachments));
    m_queryPool = makeOcclusionQueryPool(vk, device, NUM_QUERIES_IN_POOL,
                                         (m_testVector.queryResultsMode == RESULTS_MODE_GET_CREATE_RESET));

    if (m_testVector.queryResultsMode == RESULTS_MODE_COPY || m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET)
    {
        uint32_t numQueriesinPool = NUM_QUERIES_IN_POOL + (m_testVector.queryResultsDstOffset ? 1 : 0);
        const vk::VkDeviceSize elementSize =
            m_testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t);
        const vk::VkDeviceSize resultsBufferSize =
            m_testVector.queryResultsStride == 0 ?
                (elementSize + elementSize * m_testVector.queryResultsAvailability) * numQueriesinPool :
                m_testVector.queryResultsStride * numQueriesinPool;
        m_queryPoolResultsBuffer = Buffer::createAndAlloc(
            vk, device, BufferCreateInfo(resultsBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);
    }

    const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
    m_commandPool         = vk::createCommandPool(vk, device, &cmdPoolCreateInfo);
    m_renderCommandBuffer = recordRender(*m_commandPool);

    if (hasSeparateResetCmdBuf())
    {
        m_queryPoolResetCommandBuffer = recordQueryPoolReset(*m_commandPool);
    }

    if (hasSeparateCopyCmdBuf())
    {
        m_copyResultsCommandBuffer = recordCopyResults(*m_commandPool);
    }
}

tcu::TestStatus OcclusionQueryTestInstance::iterate(void)
{
    const vk::VkQueue queue       = m_context.getUniversalQueue();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    tcu::TestLog &log             = m_context.getTestContext().getLog();
    std::vector<tcu::Vec4> vertices(NUM_VERTICES);

    if (m_testVector.queryResultsMode == RESULTS_MODE_GET_RESET)
    {
        // Check VK_EXT_host_query_reset is supported
        m_context.requireDeviceFunctionality("VK_EXT_host_query_reset");
        if (m_context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
            throw tcu::NotSupportedError(
                std::string("Implementation doesn't support resetting queries from the host").c_str());
    }
#ifndef CTS_USES_VULKANSC
    else if (m_testVector.queryResultsMode == RESULTS_MODE_GET_CREATE_RESET)
    {
        // Check VK_KHR_maintenance9 is supported
        m_context.requireDeviceFunctionality("VK_KHR_maintenance9");
        if (m_context.getMaintenance9Features().maintenance9 == VK_FALSE)
            throw tcu::NotSupportedError(std::string("Implementation doesn't support creating reset queries").c_str());
    }
#endif

    // 1st triangle
    vertices[START_VERTEX + 0] = tcu::Vec4(0.5, 0.5, 0.5, 1.0);
    vertices[START_VERTEX + 1] = tcu::Vec4(0.5, -0.5, 0.5, 1.0);
    vertices[START_VERTEX + 2] = tcu::Vec4(-0.5, 0.5, 0.5, 1.0);
    // 2nd triangle - partially occluding the scene
    vertices[START_VERTEX_PARTIALLY_OCCLUDED + 0] = tcu::Vec4(-0.5, -0.5, 1.0, 1.0);
    vertices[START_VERTEX_PARTIALLY_OCCLUDED + 1] = tcu::Vec4(0.5, -0.5, 1.0, 1.0);
    vertices[START_VERTEX_PARTIALLY_OCCLUDED + 2] = tcu::Vec4(-0.5, 0.5, 1.0, 1.0);
    // 3nd triangle - fully occluding the scene
    vertices[START_VERTEX_OCCLUDER + 0] = tcu::Vec4(0.5, 0.5, 1.0, 1.0);
    vertices[START_VERTEX_OCCLUDER + 1] = tcu::Vec4(0.5, -0.5, 1.0, 1.0);
    vertices[START_VERTEX_OCCLUDER + 2] = tcu::Vec4(-0.5, 0.5, 1.0, 1.0);

    m_stateObjects->setVertices(vk, vertices);

    if (hasSeparateResetCmdBuf())
    {
        const vk::VkSubmitInfo submitInfoReset = {
            vk::VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
            nullptr,                           // const void* pNext;
            0u,                                // uint32_t waitSemaphoreCount;
            nullptr,                           // const VkSemaphore* pWaitSemaphores;
            nullptr,
            1u,                                   // uint32_t commandBufferCount;
            &m_queryPoolResetCommandBuffer.get(), // const VkCommandBuffer* pCommandBuffers;
            0u,                                   // uint32_t signalSemaphoreCount;
            nullptr                               // const VkSemaphore* pSignalSemaphores;
        };

        vk.queueSubmit(queue, 1, &submitInfoReset, VK_NULL_HANDLE);

        // Trivially wait for reset to complete. This is to ensure the query pool is in reset state before
        // host accesses, so as to not insert any synchronization before capturing the results needed for WAIT_NONE
        // variant of test.
        VK_CHECK(vk.queueWaitIdle(queue));
    }

    {
        const vk::VkSubmitInfo submitInfoRender = {
            vk::VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
            nullptr,                           // const void* pNext;
            0,                                 // uint32_t waitSemaphoreCount;
            nullptr,                           // const VkSemaphore* pWaitSemaphores;
            nullptr,
            1,                            // uint32_t commandBufferCount;
            &m_renderCommandBuffer.get(), // const VkCommandBuffer* pCommandBuffers;
            0,                            // uint32_t signalSemaphoreCount;
            nullptr                       // const VkSemaphore* pSignalSemaphores;
        };

        if (!hasSeparateResetCmdBuf() && m_testVector.queryResultsMode == RESULTS_MODE_GET_RESET)
            vk.resetQueryPool(m_context.getDevice(), *m_queryPool, 0, NUM_QUERIES_IN_POOL);
        vk.queueSubmit(queue, 1, &submitInfoRender, VK_NULL_HANDLE);
    }

    if (m_testVector.queryWait == WAIT_QUEUE)
    {
        VK_CHECK(vk.queueWaitIdle(queue));
    }

    if (hasSeparateCopyCmdBuf())
    {
        // In case of WAIT_QUEUE test variant, the previously submitted m_renderCommandBuffer did not
        // contain vkCmdCopyQueryResults, so additional cmd buffer is needed.

        // In the case of WAIT_NONE or WAIT_QUERY, vkCmdCopyQueryResults is stored in m_renderCommandBuffer.

        const vk::VkSubmitInfo submitInfo = {
            vk::VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
            nullptr,                           // const void* pNext;
            0,                                 // uint32_t waitSemaphoreCount;
            nullptr,                           // const VkSemaphore* pWaitSemaphores;
            nullptr,
            1,                                 // uint32_t commandBufferCount;
            &m_copyResultsCommandBuffer.get(), // const VkCommandBuffer* pCommandBuffers;
            0,                                 // uint32_t signalSemaphoreCount;
            nullptr                            // const VkSemaphore* pSignalSemaphores;
        };
        vk.queueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    }

    if (m_testVector.queryResultsMode == RESULTS_MODE_COPY || m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET)
    {
        // In case of vkCmdCopyQueryResults is used, test must always wait for it
        // to complete before we can read the result buffer.

        VK_CHECK(vk.queueWaitIdle(queue));
    }

    uint64_t queryResults[NUM_QUERIES_IN_POOL];
    uint64_t queryAvailability[NUM_QUERIES_IN_POOL];

    // Allow not ready results only if nobody waited before getting the query results
    const bool allowNotReady = (m_testVector.queryWait == WAIT_NONE);

    captureResults(queryResults, queryAvailability, allowNotReady);

    log << tcu::TestLog::Section("OcclusionQueryResults", "Occlusion query results");

    logResults(queryResults, queryAvailability);
    bool passed = validateResults(queryResults, queryAvailability, allowNotReady, m_testVector.primitiveTopology);

    log << tcu::TestLog::EndSection;

    if (m_testVector.queryResultsMode != RESULTS_MODE_COPY && m_testVector.queryResultsMode != RESULTS_MODE_COPY_RESET)
    {
        VK_CHECK(vk.queueWaitIdle(queue));
    }

    if (passed)
    {
        return tcu::TestStatus(QP_TEST_RESULT_PASS, "Query result verification passed");
    }
    return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Query result verification failed");
}

bool OcclusionQueryTestInstance::hasSeparateResetCmdBuf(void) const
{
    // Determine if resetting query pool should be performed in separate command buffer
    // to avoid race condition between host query access and device query reset.

    if (m_testVector.queryResultsMode == RESULTS_MODE_COPY || m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET)
    {
        // We copy query results on device, so there is no race condition between
        // host and device
        return false;
    }
    if (m_testVector.queryWait == WAIT_QUEUE)
    {
        // We wait for queue to be complete before accessing query results
        return false;
    }

    // Separate command buffer with reset must be submitted & completed before
    // host accesses the query results
    return true;
}

bool OcclusionQueryTestInstance::hasSeparateCopyCmdBuf(void) const
{
    // Copy query results must go into separate command buffer, if we want to wait on queue before that
    return ((m_testVector.queryResultsMode == RESULTS_MODE_COPY ||
             m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET) &&
            m_testVector.queryWait == WAIT_QUEUE);
}

vk::Move<vk::VkCommandBuffer> OcclusionQueryTestInstance::recordQueryPoolReset(vk::VkCommandPool cmdPool)
{
    const vk::VkDevice device     = m_context.getDevice();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    DE_ASSERT(hasSeparateResetCmdBuf());

    vk::Move<vk::VkCommandBuffer> cmdBuffer(
        vk::allocateCommandBuffer(vk, device, cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdResetQueryPool(*cmdBuffer, *m_queryPool, 0, NUM_QUERIES_IN_POOL);
    endCommandBuffer(vk, *cmdBuffer);

    return cmdBuffer;
}

void OcclusionQueryTestInstance::commandClearAttachment(const vk::DeviceInterface &vk,
                                                        const vk::VkCommandBuffer commandBuffer)
{
    if (m_testVector.clearOp == CLEAR_NOOP)
        return;

    const vk::VkOffset2D offset = vk::makeOffset2D(0, 0);
    const vk::VkExtent2D extent = vk::makeExtent2D(StateObjects::WIDTH, StateObjects::HEIGHT);

    const vk::VkClearAttachment attachment = {
        m_testVector.clearOp == CLEAR_COLOR ?
            (vk::VkImageAspectFlags)vk::VK_IMAGE_ASPECT_COLOR_BIT :
            (vk::VkImageAspectFlags)vk::VK_IMAGE_ASPECT_DEPTH_BIT, // VkImageAspectFlags aspectMask;
        m_testVector.clearOp == CLEAR_COLOR ? 0u : 1u,             // uint32_t colorAttachment;
        m_testVector.clearOp == CLEAR_COLOR ? vk::makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f)) :
                                              vk::makeClearValueDepthStencil(0.0f, 0u) // VkClearValue clearValue;
    };

    const vk::VkClearRect rect = {
        {offset, extent}, // VkRect2D rect;
        0u,               // uint32_t baseArrayLayer;
        1u,               // uint32_t layerCount;
    };

    vk.cmdClearAttachments(commandBuffer, 1u, &attachment, 1u, &rect);
}

vk::Move<vk::VkCommandBuffer> OcclusionQueryTestInstance::recordRender(vk::VkCommandPool cmdPool)
{
    const vk::VkDevice device     = m_context.getDevice();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    vk::Move<vk::VkCommandBuffer> cmdBuffer(
        vk::allocateCommandBuffer(vk, device, cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    if (!m_testVector.noColorAttachments)
        initialTransitionColor2DImage(vk, *cmdBuffer, m_stateObjects->m_colorAttachmentImage->object(),
                                      vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    initialTransitionDepth2DImage(
        vk, *cmdBuffer, m_stateObjects->m_DepthImage->object(), vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    std::vector<vk::VkClearValue> renderPassClearValues(2);
    deMemset(&renderPassClearValues[0], 0, static_cast<int>(renderPassClearValues.size()) * sizeof(vk::VkClearValue));

    if (!hasSeparateResetCmdBuf() && m_testVector.queryResultsMode != RESULTS_MODE_GET_RESET &&
        m_testVector.queryResultsMode != RESULTS_MODE_GET_CREATE_RESET)
    {
        vk.cmdResetQueryPool(*cmdBuffer, *m_queryPool, 0, NUM_QUERIES_IN_POOL);
    }

    beginRenderPass(vk, *cmdBuffer, *m_stateObjects->m_renderPass, *m_stateObjects->m_framebuffer,
                    vk::makeRect2D(0, 0, StateObjects::WIDTH, StateObjects::HEIGHT),
                    (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_pipeline);

    vk::VkBuffer vertexBuffer                 = m_stateObjects->m_vertexBuffer->object();
    const vk::VkDeviceSize vertexBufferOffset = 0;
    vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

    // Draw un-occluded geometry
    vk.cmdBeginQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_ALL, m_testVector.queryControlFlags);
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, START_VERTEX, 0);
    commandClearAttachment(vk, *cmdBuffer);
    vk.cmdEndQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_ALL);

    endRenderPass(vk, *cmdBuffer);

    beginRenderPass(vk, *cmdBuffer, *m_stateObjects->m_renderPass, *m_stateObjects->m_framebuffer,
                    vk::makeRect2D(0, 0, StateObjects::WIDTH, StateObjects::HEIGHT),
                    (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_pipeline);

    // Draw un-occluded geometry
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, START_VERTEX, 0);

    // Partially occlude geometry
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_PARTIALLY_OCCLUDED_DRAWCALL, 1, START_VERTEX_PARTIALLY_OCCLUDED, 0);

    // Draw partially-occluded geometry
    vk.cmdBeginQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_PARTIALLY_OCCLUDED, m_testVector.queryControlFlags);
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, START_VERTEX, 0);
    commandClearAttachment(vk, *cmdBuffer);
    vk.cmdEndQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_PARTIALLY_OCCLUDED);

    endRenderPass(vk, *cmdBuffer);

    beginRenderPass(vk, *cmdBuffer, *m_stateObjects->m_renderPass, *m_stateObjects->m_framebuffer,
                    vk::makeRect2D(0, 0, StateObjects::WIDTH, StateObjects::HEIGHT),
                    (uint32_t)renderPassClearValues.size(), &renderPassClearValues[0]);

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_stateObjects->m_pipeline);

    // Draw un-occluded geometry
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, START_VERTEX, 0);

    // Partially occlude geometry
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_PARTIALLY_OCCLUDED_DRAWCALL, 1, START_VERTEX_PARTIALLY_OCCLUDED, 0);

    // Occlude geometry
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_OCCLUDER_DRAWCALL, 1, START_VERTEX_OCCLUDER, 0);

    // Draw occluded geometry
    vk.cmdBeginQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_OCCLUDED, m_testVector.queryControlFlags);
    vk.cmdDraw(*cmdBuffer, NUM_VERTICES_IN_DRAWCALL, 1, START_VERTEX, 0);
    commandClearAttachment(vk, *cmdBuffer);
    vk.cmdEndQuery(*cmdBuffer, *m_queryPool, QUERY_INDEX_CAPTURE_OCCLUDED);

    endRenderPass(vk, *cmdBuffer);

    if (m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET)
    {
        vk.cmdResetQueryPool(*cmdBuffer, *m_queryPool, 0, NUM_QUERIES_IN_POOL);
    }

    if ((m_testVector.queryResultsMode == RESULTS_MODE_COPY ||
         m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET) &&
        !hasSeparateCopyCmdBuf())
    {
        vk::VkDeviceSize dstOffset = m_testVector.queryResultsDstOffset ? m_testVector.queryResultsStride : 0u;

        if (m_testVector.queryResultsStride != 0u)
        {
            vk.cmdCopyQueryPoolResults(*cmdBuffer, *m_queryPool, 0, NUM_QUERIES_IN_POOL,
                                       m_queryPoolResultsBuffer->object(), dstOffset, m_testVector.queryResultsStride,
                                       m_queryResultFlags);
        }
        else
        {
            const vk::VkDeviceSize elementSize =
                m_testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t);
            const vk::VkDeviceSize strideSize = elementSize + elementSize * m_testVector.queryResultsAvailability;

            for (int queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; queryNdx++)
            {
                vk.cmdCopyQueryPoolResults(*cmdBuffer, *m_queryPool, queryNdx, 1, m_queryPoolResultsBuffer->object(),
                                           strideSize * queryNdx, 0, m_queryResultFlags);
            }
        }

        bufferBarrier(vk, *cmdBuffer, m_queryPoolResultsBuffer->object(), vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                      vk::VK_ACCESS_HOST_READ_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT);
    }

    if (!m_testVector.noColorAttachments)
        transition2DImage(vk, *cmdBuffer, m_stateObjects->m_colorAttachmentImage->object(),
                          vk::VK_IMAGE_ASPECT_COLOR_BIT, vk::VK_IMAGE_LAYOUT_GENERAL,
                          vk::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

    endCommandBuffer(vk, *cmdBuffer);

    return cmdBuffer;
}

vk::Move<vk::VkCommandBuffer> OcclusionQueryTestInstance::recordCopyResults(vk::VkCommandPool cmdPool)
{
    const vk::VkDevice device     = m_context.getDevice();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();

    vk::Move<vk::VkCommandBuffer> cmdBuffer(
        vk::allocateCommandBuffer(vk, device, cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    vk::VkDeviceSize dstOffset = m_testVector.queryResultsDstOffset ? m_testVector.queryResultsStride : 0u;

    if (m_testVector.queryResultsStride != 0u)
    {
        vk.cmdCopyQueryPoolResults(*cmdBuffer, *m_queryPool, 0, NUM_QUERIES_IN_POOL, m_queryPoolResultsBuffer->object(),
                                   dstOffset, m_testVector.queryResultsStride, m_queryResultFlags);
    }
    else
    {
        const vk::VkDeviceSize elementSize =
            m_testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t);
        const vk::VkDeviceSize strideSize = elementSize + elementSize * m_testVector.queryResultsAvailability;

        for (int queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; queryNdx++)
        {
            vk.cmdCopyQueryPoolResults(*cmdBuffer, *m_queryPool, queryNdx, 1, m_queryPoolResultsBuffer->object(),
                                       strideSize * queryNdx, 0, m_queryResultFlags);
        }
    }

    bufferBarrier(vk, *cmdBuffer, m_queryPoolResultsBuffer->object(), vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                  vk::VK_ACCESS_HOST_READ_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT);

    endCommandBuffer(vk, *cmdBuffer);

    return cmdBuffer;
}

void OcclusionQueryTestInstance::captureResults(uint64_t *retResults, uint64_t *retAvailAbility, bool allowNotReady)
{
    const vk::VkDevice device     = m_context.getDevice();
    const vk::DeviceInterface &vk = m_context.getDeviceInterface();
    const vk::VkDeviceSize elementSize =
        m_testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t);
    const vk::VkDeviceSize resultsSize = m_testVector.queryResultsStride == 0 ?
                                             elementSize + elementSize * m_testVector.queryResultsAvailability :
                                             m_testVector.queryResultsStride;
    std::vector<uint8_t> resultsBuffer(static_cast<size_t>(resultsSize * NUM_QUERIES_IN_POOL));

    if (m_testVector.queryResultsMode == RESULTS_MODE_GET || m_testVector.queryResultsMode == RESULTS_MODE_GET_RESET ||
        m_testVector.queryResultsMode == RESULTS_MODE_GET_CREATE_RESET)
    {
        vk::VkResult queryResult =
            vk.getQueryPoolResults(device, *m_queryPool, 0, NUM_QUERIES_IN_POOL, resultsBuffer.size(),
                                   &resultsBuffer[0], m_testVector.queryResultsStride, m_queryResultFlags);
        if (queryResult == vk::VK_NOT_READY && !allowNotReady)
        {
            TCU_FAIL("getQueryPoolResults returned VK_NOT_READY, but results should be already available.");
        }
        else
        {
            VK_CHECK(queryResult);
        }
    }
    else if (m_testVector.queryResultsMode == RESULTS_MODE_COPY ||
             m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET)
    {
        const vk::Allocation &allocation = m_queryPoolResultsBuffer->getBoundMemory();
        const uint8_t *allocationData    = static_cast<uint8_t *>(allocation.getHostPtr());
        const int32_t indexData = m_testVector.queryResultsDstOffset ? (int32_t)m_testVector.queryResultsStride : 0u;

        vk::invalidateAlloc(vk, device, allocation);

        deMemcpy(&resultsBuffer[0], &allocationData[indexData], resultsBuffer.size());
    }

    for (int queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; queryNdx++)
    {
        const void *srcPtr = &resultsBuffer[queryNdx * static_cast<size_t>(resultsSize)];

        if (m_testVector.queryResultSize == RESULT_SIZE_32_BIT)
        {
            const uint32_t *srcPtrTyped = static_cast<const uint32_t *>(srcPtr);
            retResults[queryNdx]        = *srcPtrTyped;
            if (m_testVector.queryResultsAvailability)
            {
                retAvailAbility[queryNdx] = *(srcPtrTyped + 1);
            }
        }
        else if (m_testVector.queryResultSize == RESULT_SIZE_64_BIT)
        {
            const uint64_t *srcPtrTyped = static_cast<const uint64_t *>(srcPtr);
            retResults[queryNdx]        = *srcPtrTyped;

            if (m_testVector.queryResultsAvailability)
            {
                retAvailAbility[queryNdx] = *(srcPtrTyped + 1);
            }
        }
        else
        {
            TCU_FAIL("Wrong m_testVector.queryResultSize");
        }
    }

    if (m_testVector.queryResultsMode == RESULTS_MODE_GET_RESET)
    {
        vk.resetQueryPool(device, *m_queryPool, 0, NUM_QUERIES_IN_POOL);

        vk::VkResult queryResult =
            vk.getQueryPoolResults(device, *m_queryPool, 0, NUM_QUERIES_IN_POOL, resultsBuffer.size(),
                                   &resultsBuffer[0], m_testVector.queryResultsStride, m_queryResultFlags);

        if (queryResult != vk::VK_NOT_READY)
        {
            TCU_FAIL("getQueryPoolResults did not return VK_NOT_READY");
        }

        /* From Vulkan spec:
         *
         * If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
         * for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
         * However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
         */
        for (int queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; queryNdx++)
        {
            const void *srcPtr = &resultsBuffer[queryNdx * static_cast<size_t>(resultsSize)];
            if (m_testVector.queryResultSize == RESULT_SIZE_32_BIT)
            {
                const uint32_t *srcPtrTyped = static_cast<const uint32_t *>(srcPtr);
                if (*srcPtrTyped != retResults[queryNdx])
                {
                    TCU_FAIL("getQueryPoolResults returned modified values");
                }

                if (m_testVector.queryResultsAvailability && *(srcPtrTyped + 1) != 0)
                {
                    TCU_FAIL("resetQueryPool did not disable availability bit");
                }
            }
            else if (m_testVector.queryResultSize == RESULT_SIZE_64_BIT)
            {
                const uint64_t *srcPtrTyped = static_cast<const uint64_t *>(srcPtr);
                if (*srcPtrTyped != retResults[queryNdx])
                {
                    TCU_FAIL("getQueryPoolResults returned modified values");
                }

                if (m_testVector.queryResultsAvailability && *(srcPtrTyped + 1) != 0)
                {
                    TCU_FAIL("resetQueryPool did not disable availability bit");
                }
            }
            else
            {
                TCU_FAIL("Wrong m_testVector.queryResultSize");
            }
        }
    }
}

void OcclusionQueryTestInstance::logResults(const uint64_t *results, const uint64_t *availability)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    for (int ndx = 0; ndx < NUM_QUERIES_IN_POOL; ++ndx)
    {
        if (!m_testVector.queryResultsAvailability)
        {
            log << tcu::TestLog::Message << "query[slot == " << ndx << "] result == " << results[ndx]
                << tcu::TestLog::EndMessage;
        }
        else
        {
            log << tcu::TestLog::Message << "query[slot == " << ndx << "] result == " << results[ndx]
                << ", availability == " << availability[ndx] << tcu::TestLog::EndMessage;
        }
    }
}

bool OcclusionQueryTestInstance::validateResults(const uint64_t *results, const uint64_t *availability,
                                                 bool allowUnavailable, vk::VkPrimitiveTopology primitiveTopology)
{
    bool passed       = true;
    tcu::TestLog &log = m_context.getTestContext().getLog();

    for (int queryNdx = 0; queryNdx < NUM_QUERIES_IN_POOL; ++queryNdx)
    {
        uint64_t expectedValueMin = 0;
        uint64_t expectedValueMax = 0;

        if (m_testVector.queryResultsMode == RESULTS_MODE_COPY_RESET)
        {
            DE_ASSERT(m_testVector.queryResultsAvailability);
            if (availability[queryNdx] != 0)
            {
                // In copy-reset mode results should always be unavailable due to the reset command issued before copying results.
                log << tcu::TestLog::Message << "query results availability was nonzero for index " << queryNdx
                    << " when resetting the query before copying results" << tcu::TestLog::EndMessage;
                passed = false;
            }

            // Not interested in the actual results.
            continue;
        }
        else if (m_testVector.queryResultsAvailability && availability[queryNdx] == 0)
        {
            // query result was not available
            if (!allowUnavailable)
            {
                log << tcu::TestLog::Message << "query results availability was 0 for index " << queryNdx
                    << ", expected any value greater than 0." << tcu::TestLog::EndMessage;
                passed = false;
                continue;
            }
        }
        else
        {
            // query is available, so expect proper result values
            if (primitiveTopology == vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
            {
                switch (queryNdx)
                {
                case QUERY_INDEX_CAPTURE_OCCLUDED:
                    expectedValueMin = 0;
                    expectedValueMax = 0;
                    break;
                case QUERY_INDEX_CAPTURE_PARTIALLY_OCCLUDED:
                    expectedValueMin = 1;
                    expectedValueMax = 1;
                    break;
                case QUERY_INDEX_CAPTURE_ALL:
                    expectedValueMin = NUM_VERTICES_IN_DRAWCALL;
                    expectedValueMax = NUM_VERTICES_IN_DRAWCALL;
                    break;
                }
            }
            else if (primitiveTopology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            {
                switch (queryNdx)
                {
                case QUERY_INDEX_CAPTURE_OCCLUDED:
                    expectedValueMin = 0;
                    expectedValueMax = 0;
                    break;
                case QUERY_INDEX_CAPTURE_PARTIALLY_OCCLUDED:
                case QUERY_INDEX_CAPTURE_ALL:
                {
                    const int primWidth  = StateObjects::WIDTH / 2;
                    const int primHeight = StateObjects::HEIGHT / 2;
                    const int primArea   = primWidth * primHeight / 2;

                    if (m_testVector.discardHalf)
                    {
                        expectedValueMin = (int)(0.95f * primArea * 0.5f);
                        expectedValueMax = (int)(1.05f * primArea * 0.5f);
                    }
                    else
                    {
                        expectedValueMin = (int)(0.97f * primArea);
                        expectedValueMax = (int)(1.03f * primArea);
                    }
                }
                }
            }
            else
            {
                TCU_FAIL("Unsupported primitive topology");
            }
        }

        if ((m_testVector.queryControlFlags & vk::VK_QUERY_CONTROL_PRECISE_BIT) ||
            (expectedValueMin == 0 && expectedValueMax == 0))
        {
            // require precise value
            if (results[queryNdx] < expectedValueMin || results[queryNdx] > expectedValueMax)
            {
                log << tcu::TestLog::Message << "wrong value of query for index " << queryNdx
                    << ", expected the value minimum of " << expectedValueMin << ", maximum of " << expectedValueMax
                    << " got " << results[queryNdx] << "." << tcu::TestLog::EndMessage;
                passed = false;
            }
        }
        else
        {
            // require imprecise value greater than 0
            if (results[queryNdx] == 0)
            {
                log << tcu::TestLog::Message << "wrong value of query for index " << queryNdx
                    << ", expected any non-zero value, got " << results[queryNdx] << "." << tcu::TestLog::EndMessage;
                passed = false;
            }
        }
    }
    return passed;
}

template <class Instance>
class QueryPoolOcclusionTest : public vkt::TestCase
{
public:
    QueryPoolOcclusionTest(tcu::TestContext &context, const char *name, const OcclusionQueryTestVector &testVector)
        : TestCase(context, name)
        , m_testVector(testVector)
    {
    }

private:
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new Instance(context, m_testVector);
    }

    void initPrograms(vk::SourceCollections &programCollection) const
    {
        const char *const discard = "    if ((int(gl_FragCoord.x) % 2) == (int(gl_FragCoord.y) % 2))\n"
                                    "        discard;\n";

        const std::string fragSrc = std::string("#version 400\n"
                                                "layout(location = 0) out vec4 out_FragColor;\n"
                                                "void main()\n"
                                                "{\n"
                                                "    out_FragColor = vec4(0.07, 0.48, 0.75, 1.0);\n") +
                                    std::string(m_testVector.discardHalf ? discard : "") + "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc.c_str());

        programCollection.glslSources.add("vert")
            << glu::VertexSource("#version 430\n"
                                 "layout(location = 0) in vec4 in_Position;\n"
                                 "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
                                 "void main() {\n"
                                 "    gl_Position  = in_Position;\n"
                                 "    gl_PointSize = 1.0;\n"
                                 "}\n");
    }

    OcclusionQueryTestVector m_testVector;
};

struct NoAttachmentsParams
{
    bool multiSample;

    vk::VkSampleCountFlagBits getSampleCount(void) const
    {
        return (multiSample ? vk::VK_SAMPLE_COUNT_4_BIT : vk::VK_SAMPLE_COUNT_1_BIT);
    }
};

void initNoAttachmentsPrograms(vk::SourceCollections &dst, NoAttachmentsParams)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main (void) {\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void noAttachmentsSupport(Context &context, NoAttachmentsParams params)
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE);

    const auto &properties = context.getDeviceProperties();
    const auto sampleCount = params.getSampleCount();

    if ((properties.limits.framebufferNoAttachmentsSampleCounts & sampleCount) != sampleCount)
        TCU_THROW(NotSupportedError, "Required sample count not supported");
}

tcu::TestStatus noAttachmentsTest(Context &context, NoAttachmentsParams params)
{
    using namespace vk;

    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(2, 2, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto sampleCount = params.getSampleCount();

    // Vertices.
    const std::vector<tcu::Vec4> vertices{
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),
        tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f),
    };

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto vbAlloc  = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));
    flushAlloc(ctx.vkd, ctx.device, vbAlloc); // strictly speaking, not needed.

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const auto renderPass     = makeRenderPass(ctx.vkd, ctx.device);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, 0u, nullptr, vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto fullRect = makeRect2D(vkExtent);
    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(
        1u, makeRect2D(0, 0, vkExtent.width / 2u, vkExtent.height)); // Halve framebuffer with the scissor.

    // Extra pipeline state. Notably the multisample state is important.
    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = initVulkanStructure();

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = initVulkanStructure();
    multisampleStateCreateInfo.rasterizationSamples                 = sampleCount;

    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, nullptr,
                                               &rasterizationStateCreateInfo, &multisampleStateCreateInfo);

    // Query pool.
    const auto queryPool = makeOcclusionQueryPool(ctx.vkd, ctx.device, 1u);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdResetQueryPool(cmdBuffer, *queryPool, 0u, 1u);
    ctx.vkd.cmdBeginQuery(cmdBuffer, *queryPool, 0u, VK_QUERY_CONTROL_PRECISE_BIT);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, fullRect);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdEndQuery(cmdBuffer, *queryPool, 0u);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify query sample count.
    std::vector<uint32_t> queryResults(2u, 0u); // 2 slots: query result and availability bit.
    const auto resultFlags = (VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    ctx.vkd.getQueryPoolResults(ctx.device, *queryPool, 0u, 1u, de::dataSize(queryResults),
                                de::dataOrNull(queryResults), de::dataSize(queryResults), resultFlags);

    // Availability bit.
    if (queryResults.at(1u) == 0u)
        TCU_FAIL("Unexpected result in availability bit");

    // Half the samples should be covered (see scissor).
    const uint32_t reference = (vkExtent.width * vkExtent.height * static_cast<uint32_t>(sampleCount)) / 2u;
    const auto &result       = queryResults.at(0u);

    if (result != reference)
    {
        std::ostringstream msg;
        msg << "Unexpected occlusion query results: found " << result << " but expected " << reference;
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

QueryPoolOcclusionTests::QueryPoolOcclusionTests(tcu::TestContext &testCtx) : TestCaseGroup(testCtx, "occlusion_query")
{
    /* Left blank on purpose */
}

QueryPoolOcclusionTests::~QueryPoolOcclusionTests(void)
{
    /* Left blank on purpose */
}

void QueryPoolOcclusionTests::init(void)
{
    OcclusionQueryTestVector baseTestVector;
    baseTestVector.queryControlFlags        = 0;
    baseTestVector.queryResultSize          = RESULT_SIZE_64_BIT;
    baseTestVector.queryWait                = WAIT_QUEUE;
    baseTestVector.queryResultsMode         = RESULTS_MODE_GET;
    baseTestVector.queryResultsStride       = sizeof(uint64_t);
    baseTestVector.queryResultsAvailability = false;
    baseTestVector.primitiveTopology        = vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    baseTestVector.discardHalf              = false;
    baseTestVector.clearOp                  = CLEAR_NOOP;
    baseTestVector.noColorAttachments       = false;
    baseTestVector.stride                   = STRIDE_RESULT_SIZE;

    //Basic tests
    {
        OcclusionQueryTestVector testVector = baseTestVector;
        testVector.queryControlFlags        = 0;
        addChild(
            new QueryPoolOcclusionTest<BasicOcclusionQueryTestInstance>(m_testCtx, "basic_conservative", testVector));
        testVector.queryControlFlags = vk::VK_QUERY_CONTROL_PRECISE_BIT;
        addChild(new QueryPoolOcclusionTest<BasicOcclusionQueryTestInstance>(m_testCtx, "basic_precise", testVector));
    }

    {
        // STRIDE_RESULT_SIZE is omitted because it is already covered in the basic tests above
        const OcclusionQueryStride queryStrides[] = {STRIDE_ZERO, STRIDE_MAX};
        const char *const queryStridesStr[]       = {"stride_zero", "stride_max"};
        for (int queryStrideIdx = 0; queryStrideIdx < DE_LENGTH_OF_ARRAY(queryStrides); ++queryStrideIdx)
        {
            OcclusionQueryTestVector testVector = baseTestVector;
            testVector.stride                   = queryStrides[queryStrideIdx];
            addChild(new QueryPoolOcclusionTest<BasicOcclusionQueryTestInstance>(
                m_testCtx, queryStridesStr[queryStrideIdx], testVector));
        }
    }

    // No attachment cases.
    for (const bool multiSample : {false, true})
    {
        const auto testName = std::string("no_attachments_") + (multiSample ? "multisample" : "single_sample");
        NoAttachmentsParams params{multiSample};
        addFunctionCaseWithPrograms(this, testName, noAttachmentsSupport, initNoAttachmentsPrograms, noAttachmentsTest,
                                    params);
    }

    // Functional test
    {
        const vk::VkQueryControlFlags controlFlags[] = {0, vk::VK_QUERY_CONTROL_PRECISE_BIT};
        const char *const controlFlagsStr[]          = {"conservative", "precise"};

        for (int controlFlagIdx = 0; controlFlagIdx < DE_LENGTH_OF_ARRAY(controlFlags); ++controlFlagIdx)
        {

            const vk::VkPrimitiveTopology primitiveTopology[] = {vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
                                                                 vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
            const char *const primitiveTopologyStr[]          = {"points", "triangles"};
            for (int primitiveTopologyIdx = 0; primitiveTopologyIdx < DE_LENGTH_OF_ARRAY(primitiveTopology);
                 ++primitiveTopologyIdx)
            {

                const OcclusionQueryResultSize resultSize[] = {RESULT_SIZE_32_BIT, RESULT_SIZE_64_BIT};
                const char *const resultSizeStr[]           = {"32", "64"};

                for (int resultSizeIdx = 0; resultSizeIdx < DE_LENGTH_OF_ARRAY(resultSize); ++resultSizeIdx)
                {

                    const OcclusionQueryWait wait[] = {WAIT_QUEUE, WAIT_QUERY};
                    const char *const waitStr[]     = {"queue", "query"};

                    for (int waitIdx = 0; waitIdx < DE_LENGTH_OF_ARRAY(wait); ++waitIdx)
                    {
                        const OcclusionQueryResultsMode resultsMode[] = {RESULTS_MODE_GET, RESULTS_MODE_GET_RESET,
                                                                         RESULTS_MODE_GET_CREATE_RESET,
                                                                         RESULTS_MODE_COPY, RESULTS_MODE_COPY_RESET};
                        const char *const resultsModeStr[]            = {"get", "get_reset", "get_create_reset", "copy",
                                                                         "copy_reset"};

                        for (int resultsModeIdx = 0; resultsModeIdx < DE_LENGTH_OF_ARRAY(resultsMode); ++resultsModeIdx)
                        {
                            if (wait[waitIdx] == WAIT_QUERY && resultsMode[resultsModeIdx] == RESULTS_MODE_GET_RESET)
                            {
                                /* In RESULTS_MODE_GET_RESET we are going to reset the queries and get the query pool results again
                                 * without issueing them, in order to check the availability field. In Vulkan spec it mentions that
                                 * vkGetQueryPoolResults may not return in finite time. Because of that, we skip those tests.
                                 */
                                continue;
                            }

                            const bool testAvailability[]           = {false, true};
                            const char *const testAvailabilityStr[] = {"without", "with"};

                            for (int testAvailabilityIdx = 0;
                                 testAvailabilityIdx < DE_LENGTH_OF_ARRAY(testAvailability); ++testAvailabilityIdx)
                            {
                                if (resultsMode[resultsModeIdx] == RESULTS_MODE_COPY_RESET &&
                                    (!testAvailability[testAvailabilityIdx]))
                                {
                                    /* In RESULTS_MODE_COPY_RESET mode we will reset queries and make sure the availability flag is
                                     * set to zero. It does not make sense to run in this mode without obtaining the availability
                                     * flag.
                                     */
                                    continue;
                                }

                                const bool discardHalf[]           = {false, true};
                                const char *const discardHalfStr[] = {"", "_discard"};

                                for (int discardHalfIdx = 0; discardHalfIdx < DE_LENGTH_OF_ARRAY(discardHalf);
                                     ++discardHalfIdx)
                                {
                                    OcclusionQueryTestVector testVector = baseTestVector;
                                    testVector.queryControlFlags        = controlFlags[controlFlagIdx];
                                    testVector.queryResultSize          = resultSize[resultSizeIdx];
                                    testVector.queryWait                = wait[waitIdx];
                                    testVector.queryResultsMode         = resultsMode[resultsModeIdx];
                                    testVector.queryResultsStride = testVector.queryResultSize == RESULT_SIZE_32_BIT ?
                                                                        sizeof(uint32_t) :
                                                                        sizeof(uint64_t);
                                    testVector.queryResultsAvailability = testAvailability[testAvailabilityIdx];
                                    testVector.primitiveTopology        = primitiveTopology[primitiveTopologyIdx];
                                    testVector.discardHalf              = discardHalf[discardHalfIdx];

                                    if (testVector.discardHalf &&
                                        testVector.primitiveTopology == vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                                        continue; // Discarding half of the pixels in fragment shader doesn't make sense with one-pixel-sized points.

                                    if (testVector.queryResultsAvailability)
                                    {
                                        testVector.queryResultsStride *= 2;
                                    }

                                    std::ostringstream testName;

                                    testName << resultsModeStr[resultsModeIdx] << "_results"
                                             << "_" << controlFlagsStr[controlFlagIdx] << "_size_"
                                             << resultSizeStr[resultSizeIdx] << "_wait_" << waitStr[waitIdx] << "_"
                                             << testAvailabilityStr[testAvailabilityIdx] << "_availability"
                                             << "_draw_" << primitiveTopologyStr[primitiveTopologyIdx]
                                             << discardHalfStr[discardHalfIdx];

                                    addChild(new QueryPoolOcclusionTest<OcclusionQueryTestInstance>(
                                        m_testCtx, testName.str().c_str(), testVector));
                                }
                            }
                        }

                        /* Tests for clear operation within a occulusion query activated.
                         * The query shouldn't count internal driver operations relevant to the clear operations.
                         */
                        const OcculusionQueryClearOp clearOp[] = {CLEAR_COLOR, CLEAR_DEPTH};
                        const char *const clearOpStr[]         = {"clear_color", "clear_depth"};

                        for (int clearOpIdx = 0; clearOpIdx < DE_LENGTH_OF_ARRAY(clearOp); ++clearOpIdx)
                        {
                            OcclusionQueryTestVector testVector = baseTestVector;
                            testVector.queryControlFlags        = controlFlags[controlFlagIdx];
                            testVector.queryResultSize          = resultSize[resultSizeIdx];
                            testVector.queryWait                = wait[waitIdx];
                            testVector.queryResultsMode         = RESULTS_MODE_GET;
                            testVector.queryResultsStride =
                                testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t);
                            testVector.primitiveTopology = primitiveTopology[primitiveTopologyIdx];
                            testVector.clearOp           = clearOp[clearOpIdx];

                            std::ostringstream testName;

                            testName << "get_results"
                                     << "_" << controlFlagsStr[controlFlagIdx] << "_size_"
                                     << resultSizeStr[resultSizeIdx] << "_wait_" << waitStr[waitIdx]
                                     << "_without_availability"
                                     << "_draw_" << primitiveTopologyStr[primitiveTopologyIdx] << "_"
                                     << clearOpStr[clearOpIdx];

                            addChild(new QueryPoolOcclusionTest<OcclusionQueryTestInstance>(
                                m_testCtx, testName.str().c_str(), testVector));
                        }

                        // Tests with no color attachments.
                        {
                            OcclusionQueryTestVector testVector = baseTestVector;
                            testVector.queryControlFlags        = controlFlags[controlFlagIdx];
                            testVector.queryResultSize          = resultSize[resultSizeIdx];
                            testVector.queryWait                = wait[waitIdx];
                            testVector.queryResultsMode         = RESULTS_MODE_GET;
                            testVector.queryResultsStride =
                                testVector.queryResultSize == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t);
                            testVector.primitiveTopology  = primitiveTopology[primitiveTopologyIdx];
                            testVector.noColorAttachments = true;

                            std::ostringstream testName;

                            testName << "get_results"
                                     << "_" << controlFlagsStr[controlFlagIdx] << "_size_"
                                     << resultSizeStr[resultSizeIdx] << "_wait_" << waitStr[waitIdx]
                                     << "_without_availability"
                                     << "_draw_" << primitiveTopologyStr[primitiveTopologyIdx]
                                     << "_no_color_attachments";

                            addChild(new QueryPoolOcclusionTest<OcclusionQueryTestInstance>(
                                m_testCtx, testName.str().c_str(), testVector));
                        }
                    }
                }
            }
        }
    }
    // Test different strides
    {
        const OcclusionQueryResultsMode resultsMode[] = {RESULTS_MODE_GET, RESULTS_MODE_GET_RESET, RESULTS_MODE_COPY,
                                                         RESULTS_MODE_COPY_RESET};
        const char *const resultsModeStr[]            = {"get", "get_reset", "copy", "copy_reset"};

        for (int resultsModeIdx = 0; resultsModeIdx < DE_LENGTH_OF_ARRAY(resultsMode); ++resultsModeIdx)
        {
            const OcclusionQueryResultSize resultSizes[] = {RESULT_SIZE_32_BIT, RESULT_SIZE_64_BIT};
            const char *const resultSizeStr[]            = {"32", "64"};

            const bool copyQueryDstOffset[]           = {true, false};
            const char *const copyQueryDstOffsetStr[] = {"_dstoffset", ""};

            const bool testAvailability[]           = {false, true};
            const char *const testAvailabilityStr[] = {"without", "with"};

            for (int testAvailabilityIdx = 0; testAvailabilityIdx < DE_LENGTH_OF_ARRAY(testAvailability);
                 ++testAvailabilityIdx)
            {
                if (resultsMode[resultsModeIdx] == RESULTS_MODE_COPY_RESET && (!testAvailability[testAvailabilityIdx]))
                {
                    /* In RESULTS_MODE_COPY_RESET mode we will reset queries and make sure the availability flag is set to zero. It
                     * does not make sense to run in this mode without obtaining the availability flag.
                     */
                    continue;
                }

                for (int resultSizeIdx = 0; resultSizeIdx < DE_LENGTH_OF_ARRAY(resultSizes); ++resultSizeIdx)
                {
                    const vk::VkDeviceSize resultSize =
                        (resultSizes[resultSizeIdx] == RESULT_SIZE_32_BIT ? sizeof(uint32_t) : sizeof(uint64_t));

                    // \todo [2015-12-18 scygan] Ensure only stride values aligned to resultSize are allowed. Otherwise test should be extended.
                    const vk::VkDeviceSize strides[] = {0u,
                                                        1 * resultSize,
                                                        2 * resultSize,
                                                        3 * resultSize,
                                                        4 * resultSize,
                                                        5 * resultSize,
                                                        13 * resultSize,
                                                        1024 * resultSize};

                    for (int dstOffsetIdx = 0; dstOffsetIdx < DE_LENGTH_OF_ARRAY(copyQueryDstOffset); dstOffsetIdx++)
                    {
                        for (int strideIdx = 0; strideIdx < DE_LENGTH_OF_ARRAY(strides); strideIdx++)
                        {
                            OcclusionQueryTestVector testVector = baseTestVector;
                            testVector.queryResultsMode         = resultsMode[resultsModeIdx];
                            testVector.queryResultSize          = resultSizes[resultSizeIdx];
                            testVector.queryResultsAvailability = testAvailability[testAvailabilityIdx];
                            testVector.queryResultsStride       = strides[strideIdx];
                            testVector.queryResultsDstOffset    = copyQueryDstOffset[dstOffsetIdx];

                            const vk::VkDeviceSize elementSize =
                                (testVector.queryResultsAvailability ? resultSize * 2 : resultSize);

                            if (elementSize > testVector.queryResultsStride && strides[strideIdx] != 0)
                            {
                                continue;
                            }

                            if (strides[strideIdx] == 0)
                            {
                                // Due to the nature of the test, the dstOffset is tested automatically when stride size is 0.
                                if (testVector.queryResultsDstOffset)
                                {
                                    continue;
                                }

                                // We are testing only VkCmdCopyQueryPoolResults with stride 0.
                                if (testVector.queryResultsMode != RESULTS_MODE_COPY)
                                {
                                    continue;
                                }
                            }

                            std::ostringstream testName;

                            testName << resultsModeStr[resultsModeIdx] << "_results_size_"
                                     << resultSizeStr[resultSizeIdx] << "_stride_" << strides[strideIdx] << "_"
                                     << testAvailabilityStr[testAvailabilityIdx] << "_availability"
                                     << copyQueryDstOffsetStr[dstOffsetIdx];

                            addChild(new QueryPoolOcclusionTest<OcclusionQueryTestInstance>(
                                m_testCtx, testName.str().c_str(), testVector));
                        }
                    }
                }
            }
        }
    }
}

} // namespace QueryPool
} // namespace vkt
