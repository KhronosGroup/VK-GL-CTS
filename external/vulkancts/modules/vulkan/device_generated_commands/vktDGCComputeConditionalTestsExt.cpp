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

#include "vktDGCComputeConditionalTestsExt.hpp"
#include "vkBarrierUtil.hpp"
#include "vktDGCUtilExt.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <vector>
#include <memory>
#include <sstream>
#include <string>
#include <limits>

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
    bool computeQueue;        // Use the compute queue instead of the universal one.
};

struct ConditionalPreprocessParams
{
    bool conditionValue;
    bool inverted;
    bool executeOnCompute;
};

inline void checkConditionalRenderingExt(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_conditional_rendering");
}

void checkConditionalDGCComputeSupport(Context &context, TestParams params)
{
    checkDGCExtComputeSupport(context, params.pipelineToken);
    checkConditionalRenderingExt(context);

    if (params.computeQueue)
        context.getComputeQueue(); // Will throw NotSupportedError if not available.
}

void checkConditionalPreprocessSupport(Context &context, ConditionalPreprocessParams params)
{
    checkDGCExtComputeSupport(context, false);
    checkConditionalRenderingExt(context);

    if (params.executeOnCompute)
        context.getComputeQueue(); // Will throw NotSupportedError if not available.
}

// Store the push constant value in the output buffer.
void storePushConstantProgram(SourceCollections &dst)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint value; } outputBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint value; } pc;\n"
         << "void main (void) { outputBuffer.value = pc.value; }\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void storePushConstantProgramParams(SourceCollections &dst, TestParams)
{
    storePushConstantProgram(dst);
}

void storePushConstantProgramPreprocessParams(SourceCollections &dst, ConditionalPreprocessParams)
{
    storePushConstantProgram(dst);
}

void shaderToHostBarrier(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer)
{
    const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &barrier);
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
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto qfIndex    = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue      = (params.computeQueue ? context.getComputeQueue() : ctx.queue);

    // Output buffer.
    const auto outputBufferSize       = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    void *outputBufferData  = outputBufferAlloc.getHostPtr();

    deMemset(outputBufferData, 0, static_cast<size_t>(outputBufferSize));
    flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

    // Descriptor set layout, pool and set preparation.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                 &outputBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Push constants
    const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
    const auto pcValue = 777u; // Arbitrary.
    const auto pcSize  = u32Size;
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline, multiple options.
    DGCComputePipelinePtr dgcPipeline;
    Move<VkPipeline> normalPipeline;

    if (params.pipelineToken)
        dgcPipeline.reset(new DGCComputePipelineExt(ctx.vkd, ctx.device, 0u, *pipelineLayout, 0u, *compModule));
    else
        normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Indirect commands layout. Push constant followed by dispatch, optionally preceded by a pipeline bind.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (params.pipelineToken)
        cmdsLayoutBuilder.addComputePipelineToken(0u);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(5u /*pipeline index, push constant and dispatch*/);
    if (params.pipelineToken)
        genCmdsData.push_back(0u);
    genCmdsData.push_back(pcValue);
    genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
    genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
    genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z

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
        executionSetManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline->get(), 1u);
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
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    {
        // Everything is recorded on the primary command buffer.
        beginCommandBuffer(ctx.vkd, cmdBuffer);
        beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        bindPipelineIfPresent(ctx.vkd, cmdBuffer, bindPoint, *normalPipeline,
                              (dgcPipeline ? dgcPipeline->get() : VK_NULL_HANDLE));
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
        ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
        shaderToHostBarrier(ctx.vkd, cmdBuffer);
        endCommandBuffer(ctx.vkd, cmdBuffer);
    }

    // Verify results.
    uint32_t outputValue = 0u;
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

    // Note the expected value is a logical xor of the condition value and the inverted flag.
    const auto expectedValue = ((params.conditionValue != params.inverted) ? pcValue : 0u);

    if (outputValue != expectedValue)
    {
        std::ostringstream msg;
        msg << "Unexpected value found in output buffer; expected " << expectedValue << " but found " << outputValue;
        return tcu::TestStatus::fail(msg.str());
    }
    return tcu::TestStatus::pass("Pass");
}

// Creates a buffer memory barrier structure to sync access from preprocessing to execution.
VkBufferMemoryBarrier makePreprocessToExecuteBarrier(VkBuffer buffer, VkDeviceSize size, uint32_t srcQueueIndex,
                                                     uint32_t dstQueueIndex)
{
    return makeBufferMemoryBarrier(VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_EXT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                   buffer, 0ull, size, srcQueueIndex, dstQueueIndex);
}

// These tests try to check conditional rendering does not affect preprocessing.
tcu::TestStatus conditionalPreprocessRun(Context &context, ConditionalPreprocessParams params)
{
    const auto &ctx       = context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto seqCount   = 1u;

    // Output buffer.
    const auto outputBufferSize       = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    void *outputBufferData  = outputBufferAlloc.getHostPtr();

    deMemset(outputBufferData, 0, static_cast<size_t>(outputBufferSize));
    flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

    // Descriptor set layout, pool and set preparation.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                 &outputBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Push constants
    const auto u32Size = static_cast<uint32_t>(sizeof(uint32_t));
    const auto pcValue = 777u; // Arbitrary.
    const auto pcSize  = u32Size;
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline, multiple options.
    const auto normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Indirect commands layout. Push constant followed by dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT,
                                                       stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(4u /*push constant and dispatch*/);
    genCmdsData.push_back(pcValue);
    genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
    genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
    genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z

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
    const uint32_t conditionValue = (params.conditionValue ? 256u : 0u); // Avoid using value 1.
    auto &conditionBufferAlloc    = conditionBuffer.getAllocation();
    void *conditionBufferData     = conditionBufferAlloc.getHostPtr();

    deMemcpy(conditionBufferData, &conditionValue, sizeof(conditionValue));
    flushAlloc(ctx.vkd, ctx.device, conditionBufferAlloc);

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, seqCount, 0u,
                                         *normalPipeline);

    // Generated commands info.
    const DGCGenCmdsInfo cmdsInfo(stageFlags, VK_NULL_HANDLE, *cmdsLayout, genCmdsBuffer.getDeviceAddress(),
                                  genCmdsBufferSize, preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  seqCount, 0ull, 0u, *normalPipeline);

    // Command pool and buffer.
    using CommandPoolWithBufferPtr = std::unique_ptr<CommandPoolWithBuffer>;
    CommandPoolWithBufferPtr executeCmd;
    uint32_t compQueueIndex = std::numeric_limits<uint32_t>::max();
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);

    // These will be used to transfer buffers from the preprocess queue to the execution queue if needed.
    std::vector<VkBufferMemoryBarrier> ownershipBarriers;

    auto queue     = ctx.queue;
    auto cmdBuffer = *cmd.cmdBuffer;
    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Record the preprocessing step with conditional rendering enabled.
    beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
    ctx.vkd.cmdPreprocessGeneratedCommandsEXT(cmdBuffer, &cmdsInfo.get(), cmdBuffer);
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    preprocessToExecuteBarrierExt(ctx.vkd, cmdBuffer);

    if (params.executeOnCompute)
    {
        compQueueIndex = static_cast<uint32_t>(context.getComputeQueueFamilyIndex());

        ownershipBarriers.push_back(
            makePreprocessToExecuteBarrier(outputBuffer.get(), outputBufferSize, ctx.qfIndex, compQueueIndex));
        ownershipBarriers.push_back(
            makePreprocessToExecuteBarrier(genCmdsBuffer.get(), genCmdsBufferSize, ctx.qfIndex, compQueueIndex));

        if (preprocessBuffer.get() != VK_NULL_HANDLE)
        {
            ownershipBarriers.push_back(makePreprocessToExecuteBarrier(
                preprocessBuffer.get(), preprocessBuffer.getSize(), ctx.qfIndex, compQueueIndex));
        }

        ctx.vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT,
                                   VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u, nullptr, de::sizeU32(ownershipBarriers),
                                   de::dataOrNull(ownershipBarriers), 0u, nullptr);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Execute on a separate command buffer.
    executeCmd.reset(
        new CommandPoolWithBuffer(ctx.vkd, ctx.device, (params.executeOnCompute ? compQueueIndex : ctx.qfIndex)));
    cmdBuffer = *executeCmd->cmdBuffer;
    queue     = (params.executeOnCompute ? context.getComputeQueue() : ctx.queue);

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    if (params.executeOnCompute)
    {
        // This is the "acquire" barrier to transfer buffer ownership for execution. See above.
        ctx.vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT,
                                   VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u, nullptr, de::sizeU32(ownershipBarriers),
                                   de::dataOrNull(ownershipBarriers), 0u, nullptr);
    }

    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
    beginConditionalRendering(ctx.vkd, cmdBuffer, *conditionBuffer, params.inverted);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_TRUE, &cmdsInfo.get());
    ctx.vkd.cmdEndConditionalRenderingEXT(cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Verify results.
    uint32_t outputValue = 0u;
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

    const auto expectedValue = ((params.inverted != params.conditionValue) ? pcValue : 0u);
    if (outputValue != expectedValue)
    {
        std::ostringstream msg;
        msg << "Unexpected value found in output buffer; expected " << expectedValue << " but found " << outputValue;
        return tcu::TestStatus::fail(msg.str());
    }
    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCComputeConditionalTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "conditional_rendering"));
    GroupPtr generalGroup(new tcu::TestCaseGroup(testCtx, "general"));
    GroupPtr preprocessGroup(new tcu::TestCaseGroup(testCtx, "preprocess"));

    for (const auto pipelineToken : {false, true})
        for (const auto indirectCountBuffer : {false, true})
            for (const auto conditionValue : {false, true})
                for (const auto inverted : {false, true})
                    for (const auto computeQueue : {false, true})
                    {
                        const TestParams params{
                            pipelineToken, indirectCountBuffer, conditionValue, inverted, computeQueue,
                        };

                        const std::string testName =
                            std::string() + (pipelineToken ? "pipeline_token" : "classic_bind") +
                            (indirectCountBuffer ? "_with_count_buffer" : "_without_count_buffer") +
                            (conditionValue ? "_condition_true" : "_condition_false") +
                            (inverted ? "_inverted_flag" : "") + (computeQueue ? "_cq" : "_uq");

                        addFunctionCaseWithPrograms(generalGroup.get(), testName, checkConditionalDGCComputeSupport,
                                                    storePushConstantProgramParams, conditionalDispatchRun, params);
                    }

    // Preprocessing tests.
    for (const auto conditionValue : {false, true})
        for (const auto inverted : {false, true})
            for (const auto execOnCompute : {false, true})
            {
                const ConditionalPreprocessParams params{
                    conditionValue,
                    inverted,
                    execOnCompute,
                };

                const std::string testName = std::string() + (conditionValue ? "condition_true" : "condition_false") +
                                             (inverted ? "_inverted_flag" : "") +
                                             (execOnCompute ? "_exec_on_compute" : "");

                addFunctionCaseWithPrograms(preprocessGroup.get(), testName, checkConditionalPreprocessSupport,
                                            storePushConstantProgramPreprocessParams, conditionalPreprocessRun, params);
            }

    mainGroup->addChild(generalGroup.release());
    mainGroup->addChild(preprocessGroup.release());
    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
