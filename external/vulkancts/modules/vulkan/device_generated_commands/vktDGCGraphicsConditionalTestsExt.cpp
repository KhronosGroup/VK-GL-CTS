/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands EXT Conditional Rendering Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsConditionalTestsExt.hpp"
#include "vkImageUtil.hpp"
#include "vktDGCUtilExt.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDGCUtilCommon.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <vector>
#include <memory>
#include <sstream>
#include <string>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

using DGCComputePipelinePtr = std::unique_ptr<DGCComputePipelineExt>;
using DGCBufferPtr          = std::unique_ptr<DGCBuffer>;

struct TestParams
{
    bool pipelineToken;       // Use a DGC indirect pipeline.
    bool indirectCountBuffer; // Use an indirect count buffer.
    bool conditionValue;      // Value for the condition buffer.
    bool inverted;            // Inverted condition?
};

struct ConditionalPreprocessParams
{
    bool conditionValue;
    bool inverted;
};

inline void checkConditionalRenderingExt(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void checkDGCGraphicsSupport(Context &context, bool pipelineToken)
{
    const VkShaderStageFlags shaderStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const VkShaderStageFlags bindStages   = (pipelineToken ? shaderStages : 0u);
    checkDGCExtSupport(context, shaderStages, bindStages);
}

void checkConditionalDGCGraphicsSupport(Context &context, TestParams params)
{
    checkDGCGraphicsSupport(context, params.pipelineToken);
    checkConditionalRenderingExt(context);
}

void checkConditionalPreprocessSupport(Context &context, ConditionalPreprocessParams)
{
    checkDGCGraphicsSupport(context, false);
    checkConditionalRenderingExt(context);
}

// Store the push constant value in the output buffer.
void fullScreenTrianglePrograms(SourceCollections &dst)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[3] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2( 3.0, -1.0),\n"
         << "    vec2(-1.0,  3.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (push_constant, std430) uniform PCBlock { vec4 color; } pc;\n"
         << "void main (void) {\n"
         << "    outColor = pc.color;\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void storePushConstantProgramParams(SourceCollections &dst, TestParams)
{
    fullScreenTrianglePrograms(dst);
}

void storePushConstantProgramPreprocessParams(SourceCollections &dst, ConditionalPreprocessParams)
{
    fullScreenTrianglePrograms(dst);
}

void beginConditionalRendering(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkBuffer conditionBuffer,
                               bool inverted)
{
    uint32_t flags = 0u;
    if (inverted)
        flags |= VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;

    const VkConditionalRenderingBeginInfoEXT beginInfo = {
        VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT, //	VkStructureType					sType;
        nullptr,                                                //	const void*						pNext;
        conditionBuffer,                                        //	VkBuffer						buffer;
        0ull,                                                   //	VkDeviceSize					offset;
        flags,                                                  //	VkConditionalRenderingFlagsEXT	flags;
    };
    vkd.cmdBeginConditionalRenderingEXT(cmdBuffer, &beginInfo);
}

// Binds the normal (non-DGC) pipeline if it's not the null handle.
void bindPipelineIfPresent(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint,
                           VkPipeline normalPipeline, VkPipeline dgcPipeline)
{
    DE_ASSERT(normalPipeline != VK_NULL_HANDLE || dgcPipeline != VK_NULL_HANDLE);
    DE_ASSERT((normalPipeline == VK_NULL_HANDLE) != (dgcPipeline == VK_NULL_HANDLE));

    if (normalPipeline != VK_NULL_HANDLE)
        vkd.cmdBindPipeline(cmdBuffer, bindPoint, normalPipeline);
    if (dgcPipeline != VK_NULL_HANDLE)
        vkd.cmdBindPipeline(cmdBuffer, bindPoint, dgcPipeline);
}

tcu::TestStatus conditionalDispatchRun(Context &context, TestParams params)
{
    const auto &ctx       = context.getContextCommonData();
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageType = VK_IMAGE_TYPE_2D;
    const auto u32Size   = DE_SIZEOF32(uint32_t);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType);

    // Push constants
    const tcu::Vec4 pcValue(0.0f, 0.0f, 1.0f, 1.0f); // Blue.
    const auto pcSize   = DE_SIZEOF32(pcValue);
    const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    // Shaders.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // Render pass and framebuffer.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);
    const tcu::Vec4 clearValue(0.0f, 0.0f, 0.0f, 1.0f);

    // Pipeline, multiple options.
    Move<VkPipeline> dgcPipeline;
    Move<VkPipeline> normalPipeline;

    // These will be used for the DGC pipeline case, but not the normal case.
    const auto pipelineCreateFlags =
        static_cast<VkPipelineCreateFlags2KHR>(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT);

    const VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlagsInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, //   VkStructureType             sType;
        nullptr,                                                   //   const void*                 pNext;
        pipelineCreateFlags,                                       //   VkPipelineCreateFlags2KHR   flags;
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    {
        Move<VkPipeline> &createdPipeline = (params.pipelineToken ? dgcPipeline : normalPipeline);

        // pNext for flags.
        const void *pNext = (params.pipelineToken ? &pipelineCreateFlagsInfo : nullptr);

        createdPipeline = makeGraphicsPipeline(
            ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
            *fragModule, *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
            &vertexInputStateCreateInfo, nullptr, nullptr, nullptr, nullptr, nullptr, pNext);
    }

    // Indirect commands layout. Push constant followed by dispatch, optionally preceded by a pipeline bind.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (params.pipelineToken)
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(),
                                               VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, stageFlags);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(cmdsLayoutBuilder.getStreamStride() / u32Size);
    if (params.pipelineToken)
        genCmdsData.push_back(0u);
    pushBackElement(genCmdsData, pcValue);
    {
        // Draw full-screen triangle.
        const VkDrawIndirectCommand drawCmd{3u, 1u, 0u, 0u};
        pushBackElement(genCmdsData, drawCmd);
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Conditional rendering buffer.
    const auto conditionBufferSize = u32Size;
    const auto conditionBufferInfo =
        makeBufferCreateInfo(conditionBufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
    BufferWithMemory conditionBuffer(ctx.vkd, ctx.device, ctx.allocator, conditionBufferInfo,
                                     MemoryRequirement::HostVisible);
    const uint32_t conditionBufferValue =
        (params.conditionValue ? 2u : 0u); // Avoid using value 1, just to make it interesting.
    auto &conditionBufferAlloc = conditionBuffer.getAllocation();
    void *conditionBufferData  = conditionBufferAlloc.getHostPtr();

    deMemcpy(conditionBufferData, &conditionBufferValue, sizeof(conditionBufferValue));
    flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

    // Indirect execution set.
    ExecutionSetManagerPtr executionSetManager;
    if (params.pipelineToken)
    {
        executionSetManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline.get(), 1u);
        // Lets rely on the initial value.
        //executionSetManager->addPipeline(0u, dgcPipeline->get());
        executionSetManager->update();
    }
    const VkIndirectExecutionSetEXT executionSetHandle =
        (executionSetManager ? executionSetManager->get() : VK_NULL_HANDLE);

    // Preprocess buffer for 256 sequences (actually only using one, but we'll pretend we may use more).
    // Note the minimum property requirements are large enough so that 256 sequences should fit.
    const auto potentialSequenceCount = 256u;
    const auto actualSequenceCount    = 1u;
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, executionSetHandle, *cmdsLayout,
                                         potentialSequenceCount, 0u, *normalPipeline);

    // (Optional) Sequence count buffer.
    DGCBufferPtr sequenceCountBuffer;
    if (params.indirectCountBuffer)
    {
        sequenceCountBuffer.reset(new DGCBuffer(ctx.vkd, ctx.device, ctx.allocator, u32Size));

        auto &allocation = sequenceCountBuffer->getAllocation();
        void *dataptr    = allocation.getHostPtr();

        deMemcpy(dataptr, &actualSequenceCount, sizeof(actualSequenceCount));
        flushAlloc(ctx.vkd, ctx.device, allocation);
    }

    // Generated commands info.
    const auto sequenceCountBufferAddress =
        (params.indirectCountBuffer ? sequenceCountBuffer->getDeviceAddress() : 0ull);
    const auto infoSequencesCount = (params.indirectCountBuffer ? potentialSequenceCount : actualSequenceCount);

    const DGCGenCmdsInfo cmdsInfo(stageFlags, executionSetHandle, *cmdsLayout, genCmdsBuffer.getDeviceAddress(),
                                  genCmdsBufferSize, preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  infoSequencesCount, sequenceCountBufferAddress, 0u, *normalPipeline);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    {
        // Everything is recorded on the primary command buffer.
        beginCommandBuffer(ctx.vkd, cmdBuffer);
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearValue);
        beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
        bindPipelineIfPresent(ctx.vkd, cmdBuffer, bindPoint, *normalPipeline, *dgcPipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
        endRenderPass(ctx.vkd, cmdBuffer);
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
        endCommandBuffer(ctx.vkd, cmdBuffer);
    }

    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess     = referenceLevel.getAccess();
    const auto expectedValue = ((params.conditionValue != params.inverted) ? pcValue : clearValue);
    tcu::clear(referenceAccess, expectedValue);

    auto &bufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
    const void *resultData = bufferAlloc.getHostPtr();
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultData);

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected output found in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// These tests try to check conditional rendering does not affect preprocessing.
tcu::TestStatus conditionalPreprocessRun(Context &context, ConditionalPreprocessParams params)
{
    const auto &ctx      = context.getContextCommonData();
    const auto dgcStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto seqCount  = 1u;

    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto imageType = VK_IMAGE_TYPE_2D;
    const auto u32Size   = DE_SIZEOF32(uint32_t);

    // Color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType);

    // Push constants
    const tcu::Vec4 pcValue(0.0f, 0.0f, 1.0f, 1.0f); // Blue.
    const auto pcSize   = DE_SIZEOF32(pcValue);
    const auto pcStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    // Shaders.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // Render pass and framebuffer.
    const auto renderPass = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);
    const tcu::Vec4 clearValue(0.0f, 0.0f, 0.0f, 1.0f);

    // Pipeline, multiple options.
    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    const auto normalPipeline = makeGraphicsPipeline(
        ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputStateCreateInfo);

    // Indirect commands layout. Push constant followed by dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT,
                                                       dgcStages, *pipelineLayout);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDrawToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(cmdsLayoutBuilder.getStreamStride() / u32Size);
    pushBackElement(genCmdsData, pcValue);
    {
        // Draw full-screen triangle.
        const VkDrawIndirectCommand drawCmd{3u, 1u, 0u, 0u};
        pushBackElement(genCmdsData, drawCmd);
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Conditional rendering buffer.
    const auto conditionBufferSize = u32Size;
    const auto conditionBufferInfo =
        makeBufferCreateInfo(conditionBufferSize, VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT);
    BufferWithMemory conditionBuffer(ctx.vkd, ctx.device, ctx.allocator, conditionBufferInfo,
                                     MemoryRequirement::HostVisible);
    const uint32_t conditionValue =
        (params.conditionValue ? 512u : 0u); // Avoid using value 1 to make things interesting.
    auto &conditionBufferAlloc = conditionBuffer.getAllocation();
    void *conditionBufferData  = conditionBufferAlloc.getHostPtr();

    deMemcpy(conditionBufferData, &conditionValue, sizeof(conditionValue));
    flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, seqCount, 0u,
                                         *normalPipeline);

    // Generated commands info.
    const DGCGenCmdsInfo cmdsInfo(dgcStages, VK_NULL_HANDLE, *cmdsLayout, genCmdsBuffer.getDeviceAddress(),
                                  genCmdsBufferSize, preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  seqCount, 0ull, 0u, *normalPipeline);

    // Command pool and buffer.
    const auto cmdPool = makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto preprocessCmdBuffer =
        allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto executeCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    auto cmdBuffer = *preprocessCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
    beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
    ctx.vkd.cmdPreprocessGeneratedCommandsEXT(cmdBuffer, &cmdsInfo.get(), cmdBuffer);
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    preprocessToExecuteBarrierExt(ctx.vkd, cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    cmdBuffer = *executeCmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearValue);
    beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_TRUE, &cmdsInfo.get());
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto referenceAccess      = referenceLevel.getAccess();
    const auto &expectedValue = ((params.conditionValue != params.inverted) ? pcValue : clearValue);
    tcu::clear(referenceAccess, expectedValue);

    auto &bufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
    const void *resultData = bufferAlloc.getHostPtr();
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resultData);

    auto &log = context.getTestContext().getLog();
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected output found in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCGraphicsConditionalTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "conditional_rendering"));
    GroupPtr generalGroup(new tcu::TestCaseGroup(testCtx, "general"));
    GroupPtr preprocessGroup(new tcu::TestCaseGroup(testCtx, "preprocess"));

    for (const auto pipelineToken : {false, true})
        for (const auto indirectCountBuffer : {false, true})
            for (const auto conditionValue : {false, true})
                for (const auto inverted : {false, true})
                {
                    const TestParams params{
                        pipelineToken,
                        indirectCountBuffer,
                        conditionValue,
                        inverted,
                    };

                    const std::string testName =
                        std::string() + (pipelineToken ? "pipeline_token" : "classic_bind") +
                        (indirectCountBuffer ? "_with_count_buffer" : "_without_count_buffer") +
                        (conditionValue ? "_condition_true" : "_condition_false") + (inverted ? "_inverted_flag" : "");

                    addFunctionCaseWithPrograms(generalGroup.get(), testName, checkConditionalDGCGraphicsSupport,
                                                storePushConstantProgramParams, conditionalDispatchRun, params);
                }

    // Preprocessing tests.
    for (const auto conditionValue : {false, true})
        for (const auto inverted : {false, true})
        {
            const ConditionalPreprocessParams params{
                conditionValue,
                inverted,
            };

            const std::string testName = std::string() + (conditionValue ? "condition_true" : "condition_false") +
                                         (inverted ? "_inverted_flag" : "");

            addFunctionCaseWithPrograms(preprocessGroup.get(), testName, checkConditionalPreprocessSupport,
                                        storePushConstantProgramPreprocessParams, conditionalPreprocessRun, params);
        }

    mainGroup->addChild(generalGroup.release());
    mainGroup->addChild(preprocessGroup.release());
    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
