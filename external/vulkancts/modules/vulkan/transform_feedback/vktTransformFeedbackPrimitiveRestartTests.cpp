/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Vulkan Transform Feedback Primitive Restart Tests
 *//*--------------------------------------------------------------------*/

#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTransformFeedbackPrimitiveRestartTests.hpp"
#include "deUniquePtr.hpp"

#include <vector>
#include <limits>
#include <sstream>
#include <utility>

namespace vkt
{

namespace
{

using namespace vk;

class PrimitiveRestartInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        bool dynamicPrimitiveRestart;
        bool dynamicPrimitiveTopology;
    };

    PrimitiveRestartInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~PrimitiveRestartInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class PrimitiveRestartCase : public vkt::TestCase
{
public:
    PrimitiveRestartCase(tcu::TestContext &testCtx, const std::string &name,
                         const PrimitiveRestartInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~PrimitiveRestartCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new PrimitiveRestartInstance(context, m_params);
    }

protected:
    const PrimitiveRestartInstance::Params m_params;
};

void PrimitiveRestartCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_transform_feedback");

    if (m_params.dynamicPrimitiveRestart)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state2");

    if (m_params.dynamicPrimitiveTopology)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");
}

constexpr uint16_t kRestartMarker = std::numeric_limits<uint16_t>::max();

void PrimitiveRestartCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout(xfb_buffer = 0, xfb_offset = 0) out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "void main(void) {\n"
         << "    const int vid = gl_VertexIndex;\n"
         << "    const int max16 = " << kRestartMarker << ";\n" // 16-bit indices.
         << "    gl_Position = ((vid == max16) ? vec4(-1.0, -1.0, -1.0, -1.0) : vec4(vid, vid, vid, vid));\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
}

tcu::TestStatus PrimitiveRestartInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto apiExtent = makeExtent3D(fbExtent);
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto xfbStage  = VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;

    // Index buffer.
    const std::vector<uint16_t> indices{
        0,
        1,
        kRestartMarker, // No triangle formed when using primitive restarts.
        9,
        kRestartMarker,
        kRestartMarker, // Same here.
        2000,
        3000,
        4000, // Only valid triangle with primitive restart.
    };
    const auto indexBufferSize = static_cast<VkDeviceSize>(de::dataSize(indices));
    const auto indexBufferInfo = makeBufferCreateInfo(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    BufferWithMemory indexBuffer(ctx.vkd, ctx.device, ctx.allocator, indexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = indexBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(indices), de::dataSize(indices));
    }

    // Render pass and framebuffer.
    const bool hasPipelineB = (!(m_params.dynamicPrimitiveRestart && m_params.dynamicPrimitiveTopology));

    const VkSubpassDescription subpass = {
        0u, bindPoint, 0u, nullptr, 0u, nullptr, nullptr, nullptr, 0u, nullptr,
    };

    std::vector<VkSubpassDependency> subpassDependencies;
    if (hasPipelineB)
    {
        subpassDependencies.push_back(VkSubpassDependency{
            0u,
            0u,
            xfbStage,
            xfbStage,
            VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
            VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
            0u,
        });
    }

    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0u,
        0u,
        nullptr,
        1u,
        &subpass,
        de::sizeU32(subpassDependencies),
        de::dataOrNull(subpassDependencies),
    };
    const auto renderPass = createRenderPass(ctx.vkd, ctx.device, &renderPassCreateInfo);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, 0u, nullptr, apiExtent.width, apiExtent.height);

    // Pipelines.
    const auto vertModule     = createShaderModule(ctx.vkd, ctx.device, m_context.getBinaryCollection().get("vert"));
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const std::vector<VkViewport> viewports(1u, makeViewport(apiExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(apiExtent));

    // Pipeline A will draw with primitive restart enabled (triangle strip).
    // Pipeline B will draw with primitive restart disabled (triangle list).
    // However, if both states are dynamic, only pipeline A will be used.
    VkPrimitiveTopology topologyA = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPrimitiveTopology topologyB = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (m_params.dynamicPrimitiveTopology)
        topologyA = topologyB = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;

    VkBool32 restartA = VK_TRUE;
    VkBool32 restartB = VK_FALSE;
    if (m_params.dynamicPrimitiveRestart)
        std::swap(restartA, restartB);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0u, topologyA, restartA,
    };

    std::vector<VkDynamicState> dynamicStates;
    if (m_params.dynamicPrimitiveRestart)
        dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE);
    if (m_params.dynamicPrimitiveTopology)
        dynamicStates.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    const auto pipelineA = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                                VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *renderPass, 0u,
                                                &vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo, nullptr,
                                                nullptr, nullptr, nullptr, nullptr, nullptr, &dynamicStateCreateInfo);

    Move<VkPipeline> pipelineB;

    if (hasPipelineB)
    {
        inputAssemblyStateCreateInfo.topology               = topologyB;
        inputAssemblyStateCreateInfo.primitiveRestartEnable = restartB;

        pipelineB = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                         VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *renderPass, 0u,
                                         &vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, nullptr, &dynamicStateCreateInfo);
    }

    // clang-format off
    const std::vector<tcu::Vec4> expectedResults {
        // First draw: only the last strip forms a triangle.
        tcu::Vec4(2000.0f, 2000.0f, 2000.0f, 2000.0f),
        tcu::Vec4(3000.0f, 3000.0f, 3000.0f, 3000.0f),
        tcu::Vec4(4000.0f, 4000.0f, 4000.0f, 4000.0f),

        // Second draw: everything is drawn, using -1.0 for some vertex positions (see shader), using triangle lists.
        tcu::Vec4(   0.0f,    0.0f,    0.0f,    0.0f),
        tcu::Vec4(   1.0f,    1.0f,    1.0f,    1.0f),
        tcu::Vec4(  -1.0f,   -1.0f,   -1.0f,   -1.0f),
        tcu::Vec4(   9.0f,    9.0f,    9.0f,    9.0f),
        tcu::Vec4(  -1.0f,   -1.0f,   -1.0f,   -1.0f),
        tcu::Vec4(  -1.0f,   -1.0f,   -1.0f,   -1.0f),
        tcu::Vec4(2000.0f, 2000.0f, 2000.0f, 2000.0f),
        tcu::Vec4(3000.0f, 3000.0f, 3000.0f, 3000.0f),
        tcu::Vec4(4000.0f, 4000.0f, 4000.0f, 4000.0f),

        // Third draw: same as the first one.
        tcu::Vec4(2000.0f, 2000.0f, 2000.0f ,2000.0f),
        tcu::Vec4(3000.0f, 3000.0f, 3000.0f ,3000.0f),
        tcu::Vec4(4000.0f, 4000.0f, 4000.0f ,4000.0f),

        // Padding in case primitives are not restarted properly.
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
        tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
    };
    // clang-format on

    std::vector<tcu::Vec4> actualResults(expectedResults.size(), {0.0f, 0.0f, 0.0f, 0.0f});

    // Transform feedback buffer.
    const auto xfbBufferSize   = static_cast<VkDeviceSize>(de::dataSize(actualResults));
    const auto xfbBufferInfo   = makeBufferCreateInfo(xfbBufferSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
    const auto xfbBufferOffset = static_cast<VkDeviceSize>(0);
    BufferWithMemory xfbBuffer(ctx.vkd, ctx.device, ctx.allocator, xfbBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc = xfbBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(actualResults), de::dataSize(actualResults));
    }

    // Transform feedback counter buffer.
    const auto xfbCounterBufferInfo =
        makeBufferCreateInfo(sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);
    BufferWithMemory xfbCounterBuffer(ctx.vkd, ctx.device, ctx.allocator, xfbCounterBufferInfo,
                                      MemoryRequirement::HostVisible);
    {
        auto &alloc = xfbCounterBuffer.getAllocation();
        memset(alloc.getHostPtr(), 0, sizeof(uint32_t));
    }

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u));

    ctx.vkd.cmdBindTransformFeedbackBuffersEXT(cmdBuffer, 0u, 1u, &xfbBuffer.get(), &xfbBufferOffset, nullptr);
    ctx.vkd.cmdBindIndexBuffer(cmdBuffer, *indexBuffer, 0ull, VK_INDEX_TYPE_UINT16);

    // First draw.
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelineA);
    // XFB begins after binding the pipeline due to VUID-vkCmdBindPipeline-None-02323.
    ctx.vkd.cmdBeginTransformFeedbackEXT(cmdBuffer, 0u, 1u, &xfbCounterBuffer.get(), &xfbBufferOffset);
    if (m_params.dynamicPrimitiveTopology)
        ctx.vkd.cmdSetPrimitiveTopology(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    if (m_params.dynamicPrimitiveRestart)
        ctx.vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_TRUE);
    ctx.vkd.cmdDrawIndexed(cmdBuffer, de::sizeU32(indices), 1u, 0u, 0, 0u);

    // Second draw.
    if (hasPipelineB)
    {
        // We need to stop and resume due to VUID-vkCmdBindPipeline-None-02323.
        ctx.vkd.cmdEndTransformFeedbackEXT(cmdBuffer, 0u, 1u, &xfbCounterBuffer.get(), &xfbBufferOffset);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelineB);
        const auto resumeBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
                                                     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, xfbStage, xfbStage, &resumeBarrier);
        ctx.vkd.cmdBeginTransformFeedbackEXT(cmdBuffer, 0u, 1u, &xfbCounterBuffer.get(), &xfbBufferOffset);
    }
    if (m_params.dynamicPrimitiveTopology)
        ctx.vkd.cmdSetPrimitiveTopology(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    if (m_params.dynamicPrimitiveRestart)
        ctx.vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_FALSE);
    ctx.vkd.cmdDrawIndexed(cmdBuffer, de::sizeU32(indices), 1u, 0u, 0, 0u);

    // Third draw.
    if (hasPipelineB)
    {
        ctx.vkd.cmdEndTransformFeedbackEXT(cmdBuffer, 0u, 1u, &xfbCounterBuffer.get(), &xfbBufferOffset);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipelineA); // Back to pipeline A if needed.
        const auto resumeBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
                                                     VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, xfbStage, xfbStage, &resumeBarrier);
        ctx.vkd.cmdBeginTransformFeedbackEXT(cmdBuffer, 0u, 1u, &xfbCounterBuffer.get(), &xfbBufferOffset);
    }
    if (m_params.dynamicPrimitiveTopology)
        ctx.vkd.cmdSetPrimitiveTopology(cmdBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    if (m_params.dynamicPrimitiveRestart)
        ctx.vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_TRUE);
    ctx.vkd.cmdDrawIndexed(cmdBuffer, de::sizeU32(indices), 1u, 0u, 0, 0u);

    ctx.vkd.cmdEndTransformFeedbackEXT(cmdBuffer, 0u, 1u, &xfbCounterBuffer.get(), &xfbBufferOffset);

    endRenderPass(ctx.vkd, cmdBuffer);
    {
        const auto writeAccess =
            (VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT);
        const auto readAccess = VK_ACCESS_HOST_READ_BIT;
        const auto barrier    = makeMemoryBarrier(writeAccess, readAccess);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, xfbStage, VK_PIPELINE_STAGE_HOST_BIT, &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, xfbCounterBuffer.getAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, xfbBuffer.getAllocation());

    const uint32_t expectedPositions = 15u; // The first part of expectedResults before the padding.
    const uint32_t expectedCounter   = expectedPositions * DE_SIZEOF32(tcu::Vec4);
    uint32_t counter                 = 0u;
    memcpy(&counter, xfbCounterBuffer.getAllocation().getHostPtr(), sizeof(counter));

    if (counter != expectedCounter)
    {
        std::ostringstream msg;
        msg << "Unexpected value in XFB counter buffer: got " << counter << " and expected " << expectedCounter;
        TCU_FAIL(msg.str());
    }

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    memcpy(de::dataOrNull(actualResults), xfbBuffer.getAllocation().getHostPtr(), de::dataSize(actualResults));
    for (uint32_t i = 0u; i < expectedPositions; ++i)
    {
        const auto &ref = expectedResults.at(i);
        const auto &res = actualResults.at(i);

        if (ref != res)
        {
            std::ostringstream msg;
            msg << "Unexpected gl_Position value at index " << i << ": expected " << ref << " and got " << res;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            fail = true;
        }
    }
    if (fail)
        TCU_FAIL("Unexpected results in XFB buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

std::string testNamePrefix(bool dynamic)
{
    return (dynamic ? "dynamic_" : "static_");
}

namespace TransformFeedback
{

tcu::TestCaseGroup *createTransformFeedbackPrimitiveRestartTests(tcu::TestContext &testCtx)
{
    using TestGroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    TestGroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "primitive_restart"));

    for (const bool dynamicPrimitiveRestart : {false, true})
        for (const bool dynamicPrimitiveTopology : {false, true})
        {
            const PrimitiveRestartInstance::Params params{dynamicPrimitiveRestart, dynamicPrimitiveTopology};
            const auto testName = testNamePrefix(dynamicPrimitiveRestart) + "primitive_restart_" +
                                  testNamePrefix(dynamicPrimitiveTopology) + "primitive_topology";
            mainGroup->addChild(new PrimitiveRestartCase(testCtx, testName, params));
        }
    return mainGroup.release();
}

} // namespace TransformFeedback
} // namespace vkt
