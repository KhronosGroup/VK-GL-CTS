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
 * \brief Device Generated Commands EXT Compute Misc Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeMiscTestsExt.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktTestCaseUtil.hpp"

#include <numeric>
#include <sstream>
#include <vector>
#include <limits>
#include <string>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

constexpr uint32_t kTypicalWorkingGroupSize = 64u;

struct ManyDispatchesParams
{
    uint32_t dispatchCount; // Amount of executes to run.
    bool computeQueue;      // Use the compute queue.
};

struct TwoCmdBuffersParams
{
    bool useExecutionSet;
    bool computeQueue;
};

void generalCheckSupport(Context &context, bool pipelineBinds, bool computeQueue)
{
    const auto supportType = (pipelineBinds ? DGCComputeSupportType::BIND_PIPELINE : DGCComputeSupportType::BASIC);
    checkDGCExtComputeSupport(context, supportType);

    if (computeQueue)
        context.getComputeQueue();
}

void manyDispatchesCheckSupport(Context &context, ManyDispatchesParams params)
{
    generalCheckSupport(context, false, params.computeQueue);
}

void twoCmdBuffersCheckSupport(Context &context, TwoCmdBuffersParams params)
{
    generalCheckSupport(context, params.useExecutionSet, params.computeQueue);
}

void nullSetLayoutsInfoCheckSupport(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_shader_object");
    checkDGCExtComputeSupport(context, DGCComputeSupportType::BIND_SHADER);
}

// The idea here is that each command sequence will set the push constant to select an index and launch a single
// workgroup, which will increase the buffer value by 1 in each invocation, so every output buffer value ends up being
// kTypicalWorkingGroupSize.
void increaseValueByIndexPrograms(SourceCollections &dst)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << kTypicalWorkingGroupSize << ", local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[]; } outputBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint valueIndex; } pc;\n"
         << "void main (void) { atomicAdd(outputBuffer.values[pc.valueIndex], 1u); }\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void manyDispatchesInitPrograms(SourceCollections &dst, ManyDispatchesParams)
{
    increaseValueByIndexPrograms(dst);
}

void twoCmdBuffersInitPrograms(SourceCollections &dst, TwoCmdBuffersParams)
{
    increaseValueByIndexPrograms(dst);
}

// We will have two command sequences and two shaders. Both of them will work with an input buffer and an output buffer
// that contain an array of 128 integers, and each sequence will use a 64-threads working group to copy values from a
// region of the input buffer to the output buffer. The first sequence will copy them in sequential order. The second
// one will do it in reverse order. Dispatch size for each sequence is (1,1,1).
void nullSetLayoutsInfoPrograms(SourceCollections &dst)
{
    std::ostringstream commonHeaderStream;
    commonHeaderStream
        << "#version 460\n"
        << "layout (local_size_x=" << kTypicalWorkingGroupSize << ", local_size_y=1, local_size_z=1) in;\n"
        << "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[]; } outputBuffer;\n"
        << "layout (set=0, binding=1, std430) readonly buffer InputBlock { uint values[]; } inputBuffer;\n"
        << "layout (push_constant, std430) uniform PushConstantBlock { uint bufferOffset; } pc;\n";
    const auto commonHeader = commonHeaderStream.str();

    {
        std::ostringstream comp;
        comp << commonHeader << "void main(void) {\n"
             << "    const uint idx = gl_LocalInvocationIndex + pc.bufferOffset;\n"
             << "    outputBuffer.values[idx] = inputBuffer.values[idx];\n"
             << "}\n";
        dst.glslSources.add("comp1") << glu::ComputeSource(comp.str());
    }
    {
        std::ostringstream comp;
        comp << commonHeader << "void main(void) {\n"
             << "    const uint srcIdx = gl_LocalInvocationIndex + pc.bufferOffset;\n"
             << "    const uint dstIdx = (gl_WorkGroupSize.x - 1u - gl_LocalInvocationIndex) + pc.bufferOffset;\n"
             << "    outputBuffer.values[dstIdx] = inputBuffer.values[srcIdx];\n"
             << "}\n";
        dst.glslSources.add("comp2") << glu::ComputeSource(comp.str());
    }
}

tcu::TestStatus twoCmdBuffersRun(Context &context, TwoCmdBuffersParams params)
{
    const auto &ctx             = context.getContextCommonData();
    const auto descType         = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags       = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint        = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto qfIndex          = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue            = (params.computeQueue ? context.getComputeQueue() : ctx.queue);
    const auto dispatchCount    = 4u;
    const auto normalDispatches = 1u;
    const auto dgcDispatches    = dispatchCount - normalDispatches;

    // Output buffer.
    const auto valueSize              = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto outputBufferSize       = dispatchCount * valueSize;
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
    const auto pcSize  = DE_SIZEOF32(uint32_t);
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    using DGCComputePipelineExtPtr = std::unique_ptr<DGCComputePipelineExt>;
    DGCComputePipelineExtPtr dgcPipeline;
    if (params.useExecutionSet)
        dgcPipeline.reset(new DGCComputePipelineExt(ctx.vkd, ctx.device, 0u, *pipelineLayout, 0u, *compModule));

    // Indirect execution set.
    ExecutionSetManagerPtr executionSetManager;
    if (params.useExecutionSet)
    {
        executionSetManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline->get(), 1u);
        // We do not need to update anything because we only have 1 pipeline.
    }

    // Generated commands layout: push constant and dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    if (params.useExecutionSet)
        cmdsLayoutBuilder.addComputePipelineToken(cmdsLayoutBuilder.getStreamRange());
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    // Increase the value index (indicated by the push constant) in each sequence, then dispatch one workgroup.
    const auto genCmdsItemCount = ((cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t)) * dgcDispatches);
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(genCmdsItemCount);
    for (uint32_t i = normalDispatches; i < dispatchCount; ++i)
    {
        if (params.useExecutionSet)
            genCmdsData.push_back(0u);

        genCmdsData.push_back(i);  // PC data.
        genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
        genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
        genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Preprocess buffer.
    const auto ies                = (executionSetManager ? executionSetManager->get() : VK_NULL_HANDLE);
    const auto preprocessPipeline = (ies == VK_NULL_HANDLE ? *normalPipeline : VK_NULL_HANDLE);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, ies, *cmdsLayout, dgcDispatches, 0u,
                                         preprocessPipeline);

    // Command pool and buffers.
    const auto cmdPool         = makeCommandPool(ctx.vkd, ctx.device, qfIndex);
    const auto normalCmdBuffer = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto dgcCmdBuffer    = allocateCommandBuffer(ctx.vkd, ctx.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // Record normal dispatch and DGC dispatch in separate command buffers, submitting both at the same time.
    const auto postDispatchBarrier =
        makeMemoryBarrier((VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
                          (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT));
    {
        const auto cmdBuffer = *normalCmdBuffer;
        beginCommandBuffer(ctx.vkd, cmdBuffer);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipeline);
        ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT),
                                 &postDispatchBarrier);
        endCommandBuffer(ctx.vkd, cmdBuffer);
    }
    {
        const auto cmdBuffer = *dgcCmdBuffer;
        beginCommandBuffer(ctx.vkd, cmdBuffer);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        const auto boundPipeline = (dgcPipeline ? dgcPipeline->get() : *normalPipeline);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, boundPipeline);
        const auto cmdsInfoPipeline = (dgcPipeline ? VK_NULL_HANDLE : *normalPipeline);
        const DGCGenCmdsInfo cmdsInfo(
            stageFlags,                          //  VkShaderStageFlags          shaderStages;
            ies,                                 //  VkIndirectExecutionSetEXT   indirectExecutionSet;
            *cmdsLayout,                         //  VkIndirectCommandsLayoutEXT indirectCommandsLayout;
            genCmdsBuffer.getDeviceAddress(),    //  VkDeviceAddress             indirectAddress;
            genCmdsBuffer.getSize(),             //  VkDeviceSize                indirectAddressSize;
            preprocessBuffer.getDeviceAddress(), //  VkDeviceAddress             preprocessAddress;
            preprocessBuffer.getSize(),          //  VkDeviceSize                preprocessSize;
            dgcDispatches,                       //  uint32_t                    maxSequenceCount;
            0ull,                                //  VkDeviceAddress             sequenceCountAddress;
            0u,                                  //  uint32_t                    maxDrawCount;
            cmdsInfoPipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT),
                                 &postDispatchBarrier);
        endCommandBuffer(ctx.vkd, cmdBuffer);
    }

    {
        const std::vector<VkCommandBuffer> cmdBuffers{*normalCmdBuffer, *dgcCmdBuffer};

        const VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
            nullptr,                       // const void* pNext;
            0u,                            // uint32_t waitSemaphoreCount;
            nullptr,                       // const VkSemaphore* pWaitSemaphores;
            nullptr,                       // const VkPipelineStageFlags* pWaitDstStageMask;
            de::sizeU32(cmdBuffers),       // uint32_t commandBufferCount;
            de::dataOrNull(cmdBuffers),    // const VkCommandBuffer* pCommandBuffers;
            0u,                            // uint32_t signalSemaphoreCount;
            nullptr,                       // const VkSemaphore* pSignalSemaphores;
        };

        const auto fence(createFence(ctx.vkd, ctx.device));
        VK_CHECK(ctx.vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
        waitForFence(ctx.vkd, ctx.device, *fence);
    }

    // Verify results.
    std::vector<uint32_t> outputValues(dispatchCount, std::numeric_limits<uint32_t>::max());
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

    bool fail = false;
    auto &log = context.getTestContext().getLog();

    for (uint32_t i = 0; i < dispatchCount; ++i)
    {
        const auto &result = outputValues.at(i);
        if (result != kTypicalWorkingGroupSize)
        {
            log << tcu::TestLog::Message << "Error at execution " << i << ": expected " << kTypicalWorkingGroupSize
                << " but found " << result << tcu::TestLog::EndMessage;
            fail = true;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus manyExecutesRun(Context &context, ManyDispatchesParams params)
{
    const auto &ctx       = context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto qfIndex    = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue      = (params.computeQueue ? context.getComputeQueue() : ctx.queue);

    // Output buffer.
    const auto valueSize              = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto outputBufferSize       = params.dispatchCount * valueSize;
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
    const auto pcSize  = DE_SIZEOF32(uint32_t);
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Generated commands layout: push constant and dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addPushConstantToken(0u, pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    // Increase the value index (indicated by the push constant) in each sequence, then dispatch one workgroup.
    const auto genCmdsItemCount = (4u /*push constant + dispatch arguments*/ * params.dispatchCount);
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(genCmdsItemCount);
    for (uint32_t i = 0u; i < params.dispatchCount; ++i)
    {
        genCmdsData.push_back(i);  // PC data.
        genCmdsData.push_back(1u); // VkDispatchIndirectCommand::x
        genCmdsData.push_back(1u); // VkDispatchIndirectCommand::y
        genCmdsData.push_back(1u); // VkDispatchIndirectCommand::z
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Critical for the test: instead of running all these sequences in parallel, we execute one sequence at a time.
    // For the preprocess buffer, we'll use a region of the same large buffer in each execution.

    // Preprocess buffer.
    const VkDeviceSize preprocessAlignment = 4ull;

    VkMemoryRequirements preprocessBufferReqs;
    VkDeviceSize preprocessBufferStride;
    Move<VkBuffer> preprocessBuffer;
    de::MovePtr<Allocation> preprocessBufferAlloc;
    VkDeviceAddress preprocessBufferAddress = 0ull;

    {
        const DGCMemReqsInfo genCmdMemReqsInfo(VK_NULL_HANDLE, *cmdsLayout, 1u, 0u, *pipeline);
        preprocessBufferReqs = getGeneratedCommandsMemoryRequirementsExt(ctx.vkd, ctx.device, *genCmdMemReqsInfo);

        // Round up to the proper alignment, and multiply by the number of executions.
        preprocessBufferStride    = de::roundUp(preprocessBufferReqs.size, preprocessAlignment);
        preprocessBufferReqs.size = preprocessBufferStride * params.dispatchCount;

        if (preprocessBufferReqs.size > 0ull)
        {
            auto preprocessBufferCreateInfo = makeBufferCreateInfo(preprocessBufferReqs.size, 0u);

            const VkBufferUsageFlags2KHR bufferUsage =
                (VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_PREPROCESS_BUFFER_BIT_EXT);

            const VkBufferUsageFlags2CreateInfoKHR usageFlags2CreateInfo = {
                VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR, // VkStructureType sType;
                nullptr,                                                // const void* pNext;
                bufferUsage,                                            // VkBufferUsageFlags2KHR usage;
            };

            preprocessBufferCreateInfo.pNext = &usageFlags2CreateInfo;

            preprocessBuffer = createBuffer(ctx.vkd, ctx.device, &preprocessBufferCreateInfo);

            VkMemoryRequirements bufferMemReqs;
            ctx.vkd.getBufferMemoryRequirements(ctx.device, *preprocessBuffer, &bufferMemReqs);
            bufferMemReqs.memoryTypeBits &= preprocessBufferReqs.memoryTypeBits;
            bufferMemReqs.alignment = de::lcm(bufferMemReqs.alignment, preprocessBufferReqs.alignment);

            preprocessBufferAlloc = ctx.allocator.allocate(bufferMemReqs, MemoryRequirement::DeviceAddress);
            VK_CHECK(ctx.vkd.bindBufferMemory(ctx.device, *preprocessBuffer, preprocessBufferAlloc->getMemory(),
                                              preprocessBufferAlloc->getOffset()));

            preprocessBufferAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, *preprocessBuffer);
        }
    }

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto mainCmdBuffer = *cmd.cmdBuffer;

    // Main command buffer contents.
    beginCommandBuffer(ctx.vkd, mainCmdBuffer);

    ctx.vkd.cmdBindDescriptorSets(mainCmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(mainCmdBuffer, bindPoint, *pipeline);

    // Again, key for the test: run multiple executions instead of a single one.
    const auto genCmdsStride  = static_cast<VkDeviceSize>(cmdsLayoutBuilder.getStreamStride());
    const auto genCmdsAddress = genCmdsBuffer.getDeviceAddress();

    for (uint32_t i = 0u; i < params.dispatchCount; ++i)
    {
        // Specify a per-execution offset in the commands stream and preprocess buffer..
        const auto genCmdsBufferOffset = genCmdsStride * i;
        const auto preprocessOffset    = (preprocessBufferStride * i);
        const auto indirectAddress     = genCmdsAddress + genCmdsBufferOffset;
        const auto preprocessAddress   = preprocessBufferAddress + preprocessOffset;

        const DGCGenCmdsInfo cmdsInfo(stageFlags,             //    VkShaderStageFlags          shaderStages;
                                      VK_NULL_HANDLE,         //    VkIndirectExecutionSetEXT   indirectExecutionSet;
                                      *cmdsLayout,            //    VkIndirectCommandsLayoutEXT indirectCommandsLayout;
                                      indirectAddress,        //    VkDeviceAddress             indirectAddress;
                                      genCmdsStride,          //    VkDeviceSize                indirectAddressSize;
                                      preprocessAddress,      //    VkDeviceAddress             preprocessAddress;
                                      preprocessBufferStride, //    VkDeviceSize                preprocessSize;
                                      1u,                     //    uint32_t                    maxSequenceCount;
                                      0ull,                   //    VkDeviceAddress             sequenceCountAddress;
                                      0u,                     //    uint32_t                    maxDrawCount;
                                      *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(mainCmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, mainCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT, &barrier);
    }
    endCommandBuffer(ctx.vkd, mainCmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, mainCmdBuffer);

    // Verify results.
    std::vector<uint32_t> outputValues(params.dispatchCount, std::numeric_limits<uint32_t>::max());
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

    bool fail = false;
    auto &log = context.getTestContext().getLog();

    for (uint32_t i = 0; i < params.dispatchCount; ++i)
    {
        const auto &result = outputValues.at(i);
        if (result != kTypicalWorkingGroupSize)
        {
            log << tcu::TestLog::Message << "Error at execution " << i << ": expected " << kTypicalWorkingGroupSize
                << " but found " << result << tcu::TestLog::EndMessage;
            fail = true;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus manySequencesRun(Context &context, ManyDispatchesParams params)
{
    const auto &ctx       = context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto qfIndex    = (params.computeQueue ? context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue      = (params.computeQueue ? context.getComputeQueue() : ctx.queue);

    // Output buffer.
    const auto valueSize              = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto outputBufferSize       = params.dispatchCount * valueSize;
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
    const auto pcSize  = DE_SIZEOF32(uint32_t);
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Generated commands layout: push constant and dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addSequenceIndexToken(0u, pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    // Increase the value index (indicated by the push constant) in each sequence, then dispatch one workgroup.
    const auto genCmdsItemCount = (4u /*push constant + dispatch arguments*/ * params.dispatchCount);
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(genCmdsItemCount);
    for (uint32_t i = 0u; i < params.dispatchCount; ++i)
    {
        genCmdsData.push_back(std::numeric_limits<uint32_t>::max()); // Placeholder value for the sequence index.
        genCmdsData.push_back(1u);                                   // VkDispatchIndirectCommand::x
        genCmdsData.push_back(1u);                                   // VkDispatchIndirectCommand::y
        genCmdsData.push_back(1u);                                   // VkDispatchIndirectCommand::z
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout,
                                         params.dispatchCount, 0u, *pipeline);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto mainCmdBuffer = *cmd.cmdBuffer;

    // Main command buffer contents.
    beginCommandBuffer(ctx.vkd, mainCmdBuffer);

    ctx.vkd.cmdBindDescriptorSets(mainCmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(mainCmdBuffer, bindPoint, *pipeline);

    {
        const DGCGenCmdsInfo cmdsInfo(
            stageFlags,                          // VkShaderStageFlags          shaderStages;
            VK_NULL_HANDLE,                      // VkIndirectExecutionSetEXT   indirectExecutionSet;
            *cmdsLayout,                         // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
            genCmdsBuffer.getDeviceAddress(),    // VkDeviceAddress             indirectAddress;
            genCmdsBuffer.getSize(),             // VkDeviceSize                indirectAddressSize;
            preprocessBuffer.getDeviceAddress(), // VkDeviceAddress             preprocessAddress;
            preprocessBuffer.getSize(),          // VkDeviceSize                preprocessSize;
            params.dispatchCount,                // uint32_t                    maxSequenceCount;
            0ull,                                // VkDeviceAddress             sequenceCountAddress;
            0u,                                  // uint32_t                    maxDrawCount;
            *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(mainCmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, mainCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT, &barrier);
    }
    endCommandBuffer(ctx.vkd, mainCmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, mainCmdBuffer);

    // Verify results.
    std::vector<uint32_t> outputValues(params.dispatchCount, std::numeric_limits<uint32_t>::max());
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

    bool fail = false;
    auto &log = context.getTestContext().getLog();

    for (uint32_t i = 0; i < params.dispatchCount; ++i)
    {
        const auto &result = outputValues.at(i);
        if (result != kTypicalWorkingGroupSize)
        {
            log << tcu::TestLog::Message << "Error at execution " << i << ": expected " << kTypicalWorkingGroupSize
                << " but found " << result << tcu::TestLog::EndMessage;
            fail = true;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus nullSetLayoutsInfoRun(Context &context)
{
    const auto ctx          = context.getContextCommonData();
    const auto descType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlagBit = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto stageFlags   = static_cast<VkShaderStageFlags>(stageFlagBit);
    const auto bindPoint    = VK_PIPELINE_BIND_POINT_COMPUTE;

    // Input and output buffers.
    const auto valueCount  = kTypicalWorkingGroupSize * 2u;
    const auto valueOffset = 1000u;

    std::vector<uint32_t> inputValues(valueCount, 0u);
    std::iota(begin(inputValues), end(inputValues), valueOffset);

    const auto bufferSize       = static_cast<VkDeviceSize>(de::dataSize(inputValues));
    const auto bufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

    BufferWithMemory inputBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, HostIntent::W);
    {
        auto &alloc = inputBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(inputValues), de::dataSize(inputValues));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, HostIntent::R);
    {
        auto &alloc = outputBuffer.getAllocation();
        memset(alloc.getHostPtr(), 0, de::dataSize(inputValues));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Descriptor set layout.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    // Push constants.
    const auto pcSize   = DE_SIZEOF32(uint32_t);
    const auto pcStages = stageFlags;
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, 2u);
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descSet  = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    // Update descriptor set.
    DescriptorSetUpdateBuilder updateBuilder;
    const VkBuffer buffersArray[] = {*outputBuffer, *inputBuffer};
    const auto binding            = DescriptorSetUpdateBuilder::Location::binding;
    for (uint32_t i = 0u; i < 2u; ++i)
    {
        const auto descInfo = makeDescriptorBufferInfo(buffersArray[i], 0ull, VK_WHOLE_SIZE);
        updateBuilder.writeSingle(*descSet, binding(i), descType, &descInfo);
    }
    updateBuilder.update(ctx.vkd, ctx.device);

    // Shaders.
    const auto &binaries = context.getBinaryCollection();
    const std::vector<VkDescriptorSetLayout> shaderSetLayouts{*setLayout};
    const std::vector<VkPushConstantRange> shaderPCRanges{pcRange};
    DGCComputeShaderExt comp1Shader(ctx.vkd, ctx.device, 0u, binaries.get("comp1"), shaderSetLayouts, shaderPCRanges);
    DGCComputeShaderExt comp2Shader(ctx.vkd, ctx.device, 0u, binaries.get("comp2"), shaderSetLayouts, shaderPCRanges);

    // Indirect execution set, created manually in this case to be able to use a null pSetLayoutsInfo.
    const VkIndirectExecutionSetShaderInfoEXT iesShaderInfo = {
        VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_SHADER_INFO_EXT,
        nullptr,
        1u,
        &comp1Shader.get(),
        nullptr, // THIS IS THE KEY OF THE TEST. THE IMPLEMENTATION WOULD HAVE TO FETCH THIS INFO FROM THE SHADER.
        2u,
        1u,
        &pcRange,
    };

    VkIndirectExecutionSetInfoEXT iesInfo;
    iesInfo.pShaderInfo = &iesShaderInfo;

    const VkIndirectExecutionSetCreateInfoEXT iesCreateInfo = {
        VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT,
        nullptr,
        VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT,
        iesInfo,
    };

    const auto ies = createIndirectExecutionSetEXT(ctx.vkd, ctx.device, &iesCreateInfo);

    const VkWriteIndirectExecutionSetShaderEXT iesUpdate = {
        VK_STRUCTURE_TYPE_WRITE_INDIRECT_EXECUTION_SET_SHADER_EXT,
        nullptr,
        1u,
        comp2Shader.get(),
    };
    ctx.vkd.updateIndirectExecutionSetShaderEXT(ctx.device, *ies, 1u, &iesUpdate);

    // Create the commands layout and DGC buffer.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addComputeShaderObjectToken(cmdsLayoutBuilder.getStreamRange());
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    const auto sequenceCount = 2u;
    const auto dgcBufferSize = sequenceCount * cmdsLayoutBuilder.getStreamStride();

    std::vector<uint32_t> dgcData;
    dgcData.reserve(dgcBufferSize / DE_SIZEOF32(uint32_t));
    dgcData.push_back(0u); // Choose comp1
    dgcData.push_back(0u); // Value offset for the first sequence.
    dgcData.push_back(1u); // Dispatch
    dgcData.push_back(1u);
    dgcData.push_back(1u);
    dgcData.push_back(1u);                       // Choose comp2
    dgcData.push_back(kTypicalWorkingGroupSize); // Value offset for the second sequence.
    dgcData.push_back(1u);                       // Dispatch
    dgcData.push_back(1u);
    dgcData.push_back(1u);

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc = dgcBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, *ies, *cmdsLayout, sequenceCount, 0u);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    // Main command buffer contents.
    beginCommandBuffer(ctx.vkd, cmdBuffer);

    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &stageFlagBit, &comp1Shader.get());

    {
        const DGCGenCmdsInfo cmdsInfo(
            stageFlags,                          // VkShaderStageFlags          shaderStages;
            *ies,                                // VkIndirectExecutionSetEXT   indirectExecutionSet;
            *cmdsLayout,                         // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
            dgcBuffer.getDeviceAddress(),        // VkDeviceAddress             indirectAddress;
            dgcBuffer.getSize(),                 // VkDeviceSize                indirectAddressSize;
            preprocessBuffer.getDeviceAddress(), // VkDeviceAddress             preprocessAddress;
            preprocessBuffer.getSize(),          // VkDeviceSize                preprocessSize;
            sequenceCount,                       // uint32_t                    maxSequenceCount;
            0ull,                                // VkDeviceAddress             sequenceCountAddress;
            0u);                                 // uint32_t                    maxDrawCount;
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Check output buffer.
    {
        auto &alloc = outputBuffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        std::vector<uint32_t> outputValues(inputValues.size());
        memcpy(de::dataOrNull(outputValues), alloc.getHostPtr(), de::dataSize(outputValues));

        std::vector<uint32_t> expectedValues(inputValues.size());
        for (uint32_t i = 0u; i < valueCount; ++i)
        {
            if (i < kTypicalWorkingGroupSize)
                expectedValues.at(i) = valueOffset + i;
            else
                expectedValues.at(i) = valueOffset + (valueCount - 1u - (i - kTypicalWorkingGroupSize));
        }

        bool fail = false;
        auto &log = context.getTestContext().getLog();

        for (uint32_t i = 0u; i < valueCount; ++i)
        {
            const auto &ref = expectedValues.at(i);
            const auto &res = outputValues.at(i);

            if (ref != res)
            {
                fail = true;
                std::ostringstream msg;
                msg << "Unexpected value at index " << i << ": expected " << ref << " but found " << res;
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            }
        }

        if (fail)
            TCU_FAIL("Unexpected values found in output buffer; check log for details --");
    }

    return tcu::TestStatus::pass("Pass");
}

class ScratchSpaceInstance : public vkt::TestInstance
{
public:
    ScratchSpaceInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    virtual ~ScratchSpaceInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;
};

class ScratchSpaceCase : public vkt::TestCase
{
public:
    ScratchSpaceCase(tcu::TestContext &testCtx, const std::string &name) : vkt::TestCase(testCtx, name)
    {
    }
    virtual ~ScratchSpaceCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
};

void ScratchSpaceCase::checkSupport(Context &context) const
{
    checkDGCExtComputeSupport(context, DGCComputeSupportType::BIND_PIPELINE);
    context.getComputeQueue(); // Throws NotSupportedError if not available.
}

// The goal of this large shader is to make sure some scratch space is needed due to register spilling, and that this scratch space
// is allocated correctly. Register spilling is attempted to be guaranteed due to the amount of combinations from input vars and the
// non-uniform control flow from the shader used below.
void ScratchSpaceCase::initPrograms(SourceCollections &dst) const
{
    const std::string code = ShaderSourceProvider::getSource(
        m_testCtx.getArchive(), "vulkan/device_generated_commands/ScratchSpace.comp.spvasm");
    dst.spirvAsmSources.add("comp") << code;
}

TestInstance *ScratchSpaceCase::createInstance(Context &context) const
{
    return new ScratchSpaceInstance(context);
}

constexpr int32_t kScratchSpaceLocalInvocations = 4; // Must match ScratchSpace.comp

tcu::TestStatus ScratchSpaceInstance::iterate()
{
    // Must match ScratchSpace.comp: these were obtained in practice by converting the shader to C.
    static const std::vector<int32_t> expectedOutputs{
        -256,
        -46,
        -327,
        -722,
    };

    const auto &ctx        = m_context.getContextCommonData();
    const auto bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto descType    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags  = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_COMPUTE;

    // Output buffer.
    std::vector<int32_t> outputValues(kScratchSpaceLocalInvocations, 0);
    const auto outputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(outputValues));
    const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, bufferUsage);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    void *outputBufferData  = outputBufferAlloc.getHostPtr();

    deMemcpy(outputBufferData, de::dataOrNull(outputValues), de::dataSize(outputValues));
    flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

    // Input buffer.
    std::vector<int32_t> inputValues;
    inputValues.resize(kScratchSpaceLocalInvocations);
    std::iota(begin(inputValues), end(inputValues), 0);

    const auto inputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(inputValues));
    const auto inputBufferCreateInfo = makeBufferCreateInfo(inputBufferSize, bufferUsage);
    BufferWithMemory inputBuffer(ctx.vkd, ctx.device, ctx.allocator, inputBufferCreateInfo,
                                 MemoryRequirement::HostVisible);
    auto &inputBufferAlloc = inputBuffer.getAllocation();
    void *inputBufferData  = inputBufferAlloc.getHostPtr();

    deMemcpy(inputBufferData, de::dataOrNull(inputValues), de::dataSize(inputValues));
    flushAlloc(ctx.vkd, ctx.device, inputBufferAlloc);

    // Descriptor set layout, pool and set preparation.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, 2u /*input and output buffers*/);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto inputBufferDescInfo  = makeDescriptorBufferInfo(*inputBuffer, 0ull, inputBufferSize);
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                 &inputBufferDescInfo);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descType,
                                 &outputBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    // Shader.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // DGC Pipeline.
    const DGCComputePipelineExt dgcPipeline(ctx.vkd, ctx.device, 0u, *pipelineLayout, 0u, *compModule);

    // Uncomment this to verify the shader properties if needed.
#if 0
    {
        auto& log = m_context.getTestContext().getLog();
        VkPipelineInfoKHR pipelineInfo = initVulkanStructure();
        pipelineInfo.pipeline = *dgcPipeline;
        uint32_t executableCount = 0u;
        VK_CHECK(ctx.vkd.getPipelineExecutablePropertiesKHR(ctx.device, &pipelineInfo, &executableCount, nullptr));

        for (uint32_t i = 0; i < executableCount; ++i)
        {
            VkPipelineExecutableInfoKHR executableInfo = initVulkanStructure();
            executableInfo.pipeline = *dgcPipeline;
            executableInfo.executableIndex = i;
            uint32_t statsCount = 0u;
            VK_CHECK(ctx.vkd.getPipelineExecutableStatisticsKHR(ctx.device, &executableInfo, &statsCount, nullptr));
            if (statsCount == 0u)
                continue;
            std::vector<VkPipelineExecutableStatisticKHR> stats (statsCount);
            VK_CHECK(ctx.vkd.getPipelineExecutableStatisticsKHR(ctx.device, &executableInfo, &statsCount, de::dataOrNull(stats)));
            std::string valueStr;
            for (uint32_t j = 0u; j < statsCount; ++j)
            {
                const auto& stat = stats.at(j);
                if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR)
                    valueStr = std::to_string(stat.value.b32);
                else if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR)
                    valueStr = std::to_string(stat.value.i64);
                else if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR)
                    valueStr = std::to_string(stat.value.u64);
                else if (stat.format == VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR)
                    valueStr = std::to_string(stat.value.f64);
                else
                    DE_ASSERT(false);
                log << tcu::TestLog::Message << stat.name << " (" << stat.description << "): " << valueStr << tcu::TestLog::EndMessage;
            }
        }
    }
#endif

    // Indirect commands layout: pipeline token followed by dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addComputePipelineToken(0u);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(4u /*pipeline bind + dispatch*/);
    genCmdsData.push_back(0u); // Pipeline index.
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

    // Create indirect execution set.
    ExecutionSetManagerPtr executionSetManager =
        makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline.get(), 1u);
    executionSetManager->addPipeline(0u, dgcPipeline.get());
    executionSetManager->update();
    const auto executionSet = executionSetManager->get();

    // Preprocess buffer for 1 sequence.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, executionSet, *cmdsLayout, 1u, 0u);

    // Submit the dispatch to the compute queue.
    {
        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, m_context.getComputeQueueFamilyIndex());
        const auto cmdBuffer = *cmd.cmdBuffer;

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, dgcPipeline.get());
        {
            const DGCGenCmdsInfo cmdsInfo(
                stageFlags,                          // VkShaderStageFlags          shaderStages;
                executionSet,                        // VkIndirectExecutionSetEXT   indirectExecutionSet;
                *cmdsLayout,                         // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
                genCmdsBuffer.getDeviceAddress(),    // VkDeviceAddress             indirectAddress;
                genCmdsBufferSize,                   // VkDeviceSize                indirectAddressSize;
                preprocessBuffer.getDeviceAddress(), // VkDeviceAddress             preprocessAddress;
                preprocessBuffer.getSize(),          // VkDeviceSize                preprocessSize;
                1u,                                  // uint32_t                    maxSequenceCount;
                0ull,                                // VkDeviceAddress             sequenceCountAddress;
                0u);                                 // uint32_t                    maxDrawCount;
            ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
        }
        {
            const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_HOST_BIT, &barrier);
        }
        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, m_context.getComputeQueue(), cmdBuffer);
    }

    // Verify results.
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(de::dataOrNull(outputValues), outputBufferData, de::dataSize(outputValues));

    auto &log = m_context.getTestContext().getLog();
    bool fail = false;

    DE_ASSERT(expectedOutputs.size() == outputValues.size());
    for (size_t i = 0u; i < outputValues.size(); ++i)
    {
        const auto reference = expectedOutputs.at(i);
        const auto result    = outputValues.at(i);

        if (result != reference)
        {
            std::ostringstream msg;
            msg << "Unexpected value found in output buffer at position " << i << ": expected " << reference
                << " but found " << result;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            fail = true;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected values in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

// Test the maximum amount of push constants possible.
class MaxPushConstantRangeInstance : public vkt::TestInstance
{
public:
    struct Params
    {
        uint32_t pcBytes;
        bool partial; // Update the range partly by DGC and partly by external push constants, or all by DGC.
        bool preprocess;
        bool useExecutionSet;
        bool pushDescriptor;
        bool useComputeQueue;

        uint32_t itemCount(void) const
        {
            // Each item is goint to be a uint32_t, so given pcBytes we can calculate the number of items.
            const auto itemSize  = DE_SIZEOF32(uint32_t);
            const auto itemCount = pcBytes / itemSize;
            return itemCount;
        }
    };

    MaxPushConstantRangeInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~MaxPushConstantRangeInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class MaxPushConstantRangeCase : public vkt::TestCase
{
public:
    MaxPushConstantRangeCase(tcu::TestContext &testCtx, const std::string &name,
                             const MaxPushConstantRangeInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~MaxPushConstantRangeCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const MaxPushConstantRangeInstance::Params m_params;
};

void MaxPushConstantRangeCase::checkSupport(Context &context) const
{
    if (m_params.useComputeQueue)
        context.getComputeQueue(); // Will throw if not available.

    const auto supportType =
        (m_params.useExecutionSet ? DGCComputeSupportType::BIND_PIPELINE : DGCComputeSupportType::BASIC);
    checkDGCExtComputeSupport(context, supportType);

    const auto &properties = context.getDeviceProperties();
    if (properties.limits.maxPushConstantsSize < m_params.pcBytes)
        TCU_THROW(NotSupportedError, "maxPushConstantsSize below required size");

    const auto dispatchSize = m_params.itemCount();
    if (properties.limits.maxComputeWorkGroupCount[0] < dispatchSize)
        TCU_THROW(NotSupportedError, "maxComputeWorkGroupCount below required limit");

    if (m_params.pushDescriptor)
        context.requireDeviceFunctionality("VK_KHR_push_descriptor");
}

void MaxPushConstantRangeCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto arraySize = m_params.itemCount();

    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1) in;\n"
         << "layout (push_constant, std430) uniform PCBlock {\n"
         << "   uint values[" << arraySize << "];\n"
         << "} pc;\n"
         << "layout (set=0, binding=0, std430) buffer Output {\n"
         << "   uint values[" << arraySize << "];\n"
         << "} ob;\n"
         << "\n"
         << "uint getWorkGroupIndex (void) {\n"
         << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z +\n"
         << "                                gl_NumWorkGroups.x * gl_WorkGroupID.y +\n"
         << "                                gl_WorkGroupID.x;\n"
         << "    return workGroupIndex;\n"
         << "}\n"
         << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    const uint wgIndex = getWorkGroupIndex();\n"
         << "    const uint pcValue = pc.values[wgIndex];\n"
         << "    ob.values[wgIndex] = pcValue;\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *MaxPushConstantRangeCase::createInstance(Context &context) const
{
    return new MaxPushConstantRangeInstance(context, m_params);
}

tcu::TestStatus MaxPushConstantRangeInstance::iterate(void)
{
    const auto ctx          = m_context.getContextCommonData();
    const auto itemCount    = m_params.itemCount();
    const auto shaderStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto descType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Expected output buffer values.
    std::vector<uint32_t> expectedValues(itemCount, 0u);
    for (uint32_t i = 0u; i < itemCount; ++i)
    {
        const bool external      = (m_params.partial && i > 0u && i < itemCount - 1u);
        const uint32_t baseValue = (external ? 1000000u : 2000000u);
        expectedValues.at(i)     = baseValue + i;
    }

    // Prepare output buffer.
    const auto outputBufferSize  = static_cast<VkDeviceSize>(de::dataSize(expectedValues));
    const auto outputBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto outputBufferInfo  = makeBufferCreateInfo(outputBufferSize, outputBufferUsage);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, MemoryRequirement::HostVisible);
    deMemset(outputBuffer.getAllocation().getHostPtr(), 0, de::dataSize(expectedValues));

    // Descriptor set layout and pipeline layout.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, shaderStages);
    const auto setLayoutFlags = (m_params.pushDescriptor ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device, setLayoutFlags);
    const auto pcRange        = makePushConstantRange(shaderStages, 0u, m_params.pcBytes);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Prepare descriptor pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet =
        (m_params.pushDescriptor ? Move<VkDescriptorSet>() :
                                   makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout));

    const auto bufferDescInfo = makeDescriptorBufferInfo(outputBuffer.get(), 0ull, VK_WHOLE_SIZE);
    if (!m_params.pushDescriptor)
    {
        // Update descriptor set.
        using Location = DescriptorSetUpdateBuilder::Location;
        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(*descriptorSet, Location::binding(0u), descType, &bufferDescInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Pipeline, normal or DGC.
    using DGCComputePipelineExtPtr = std::unique_ptr<DGCComputePipelineExt>;
    DGCComputePipelineExtPtr dgcPipeline;
    Move<VkPipeline> normalPipeline;

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    if (m_params.useExecutionSet)
        dgcPipeline.reset(new DGCComputePipelineExt(ctx.vkd, ctx.device, 0u, *pipelineLayout, 0u, *compModule));
    else
        normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT ies = VK_NULL_HANDLE;
    if (m_params.useExecutionSet)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline->get(), 1u);
        iesManager->addPipeline(0u, dgcPipeline->get());
        iesManager->update();
        ies = iesManager->get();
    }

    // DGC Commands layout and buffer.
    const auto itemSize         = DE_SIZEOF32(uint32_t);
    const auto firstItemRange   = makePushConstantRange(shaderStages, 0u, itemSize);
    const auto lastItemRange    = makePushConstantRange(shaderStages, (itemCount - 1u) * itemSize, itemSize);
    const auto middleItemsRange = makePushConstantRange(shaderStages, itemSize, (itemCount - 2u) * itemSize);

    const auto cmdsLayoutFlags = static_cast<VkIndirectCommandsLayoutUsageFlagsEXT>(
        m_params.preprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, *pipelineLayout);
    if (m_params.useExecutionSet)
        cmdsLayoutBuilder.addComputePipelineToken(0u);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), lastItemRange);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), firstItemRange);
    if (!m_params.partial)
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), middleItemsRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() / itemSize);
    if (m_params.useExecutionSet)
        dgcData.push_back(0u);
    dgcData.push_back(expectedValues.back());
    dgcData.push_back(expectedValues.front());
    if (!m_params.partial)
        dgcData.insert(dgcData.end(), ++expectedValues.begin(), --expectedValues.end());
    {
        const auto dispatchSize = m_params.itemCount();
        dgcData.push_back(dispatchSize);
        dgcData.push_back(1u);
        dgcData.push_back(1u);
    }

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    deMemcpy(dgcBuffer.getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, ies, *cmdsLayout, 1u, 0u, *normalPipeline);

    const auto qfIndex = (m_params.useComputeQueue ? m_context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue   = (m_params.useComputeQueue ? m_context.getComputeQueue() : ctx.queue);
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const DGCGenCmdsInfo cmdsInfo(shaderStages, ies, *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                  preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), 1u, 0ull, 0u,
                                  *normalPipeline);

    const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (m_params.pushDescriptor)
    {
        const VkWriteDescriptorSet pushWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, //  VkStructureType                 sType;
            nullptr,                                //  const void*                     pNext;
            VK_NULL_HANDLE,                         //  VkDescriptorSet                 dstSet;
            0u,                                     //  uint32_t                        dstBinding;
            0u,                                     //  uint32_t                        dstArrayElement;
            1u,                                     //  uint32_t                        descriptorCount;
            descType,                               //  VkDescriptorType                descriptorType;
            nullptr,                                //  const VkDescriptorImageInfo*    pImageInfo;
            &bufferDescInfo,                        //  const VkDescriptorBufferInfo*   pBufferInfo;
            nullptr,                                //  const VkBufferView*             pTexelBufferView;
        };
        ctx.vkd.cmdPushDescriptorSet(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &pushWrite);
    }
    else
    {
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    }
    if (m_params.partial)
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, shaderStages, middleItemsRange.offset,
                                 middleItemsRange.size, &expectedValues.at(1u));
    {
        const auto pipeline = (m_params.useExecutionSet ? dgcPipeline->get() : *normalPipeline);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline);
    }
    if (m_params.preprocess)
    {
        ctx.vkd.cmdPreprocessGeneratedCommandsEXT(cmdBuffer, &cmdsInfo.get(), cmdBuffer);
        preprocessToExecuteBarrierExt(ctx.vkd, cmdBuffer);
    }
    {
        const auto isPreprocessed = makeVkBool(m_params.preprocess);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdsInfo.get());
    }
    {
        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Verify results
    invalidateAlloc(ctx.vkd, ctx.device, outputBuffer.getAllocation());
    std::vector<uint32_t> resultValues(expectedValues.size(), 0u);
    DE_ASSERT(de::dataSize(resultValues) == static_cast<size_t>(outputBufferSize));
    deMemcpy(de::dataOrNull(resultValues), outputBuffer.getAllocation().getHostPtr(), de::dataSize(resultValues));

    bool resultOK = true;
    auto &log     = m_context.getTestContext().getLog();
    for (uint32_t i = 0u; i < itemCount; ++i)
    {
        const auto result   = resultValues.at(i);
        const auto expected = expectedValues.at(i);
        if (result != expected)
        {
            resultOK = false;
            log << tcu::TestLog::Message << "Unexpected value at position " << i << ": found " << result
                << " and expected " << expected << tcu::TestLog::EndMessage;
        }
    }

    if (!resultOK)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

// Test multiple descriptor sets with IES and compute (RADV had bugs related to this at some point).
class MultipleSetsInstance : public vkt::TestInstance
{
public:
    static constexpr uint32_t kLocalSize      = 32u;
    static constexpr uint32_t kItemCount      = 1024u;
    static constexpr uint32_t kWorkGroupCount = kItemCount / kLocalSize;

    struct Params
    {
        bool preprocess;
        bool useComputeQueue;
    };

    MultipleSetsInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~MultipleSetsInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class MultipleSetsCase : public vkt::TestCase
{
public:
    MultipleSetsCase(tcu::TestContext &testCtx, const std::string &name, const MultipleSetsInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~MultipleSetsCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const MultipleSetsInstance::Params m_params;
};

void MultipleSetsCase::checkSupport(Context &context) const
{
    if (m_params.useComputeQueue)
        context.getComputeQueue(); // Will throw if not available.

    checkDGCExtComputeSupport(context, DGCComputeSupportType::BIND_PIPELINE);

    const auto &properties = context.getDeviceProperties();

    if (properties.limits.maxComputeWorkGroupSize[0] < MultipleSetsInstance::kLocalSize)
        TCU_THROW(NotSupportedError, "maxComputeWorkGroupSize below required limit");

    if (properties.limits.maxComputeWorkGroupCount[0] < MultipleSetsInstance::kWorkGroupCount)
        TCU_THROW(NotSupportedError, "maxComputeWorkGroupCount below required limit");
}

void MultipleSetsCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << MultipleSetsInstance::kLocalSize << ") in;\n"
         << "layout (set=0, binding=0, std430) readonly buffer Input {\n"
         << "   uint values[];\n"
         << "} ib;\n"
         << "layout (set=1, binding=0, std430) buffer Output {\n"
         << "   uint values[];\n"
         << "} ob;\n"
         << "\n"
         << "uint getWorkGroupSize (void) {\n"
         << "    const uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
         << "    return workGroupSize;\n"
         << "}\n"
         << "\n"
         << "uint getWorkGroupIndex (void) {\n"
         << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z +\n"
         << "                                gl_NumWorkGroups.x * gl_WorkGroupID.y +\n"
         << "                                gl_WorkGroupID.x;\n"
         << "    return workGroupIndex;\n"
         << "}\n"
         << "\n"
         << "uint getGlobalInvocationIndex (void) {\n"
         << "    const uint globalInvocationIndex = getWorkGroupIndex() * getWorkGroupSize() + "
            "gl_LocalInvocationIndex;\n"
         << "    return globalInvocationIndex;\n"
         << "}\n"
         << "\n"
         << "void main (void)\n"
         << "{\n"
         << "    const uint globalInvocationIndex = getGlobalInvocationIndex();\n"
         << "    const uint value = ib.values[globalInvocationIndex];\n"
         << "    ob.values[globalInvocationIndex] = value;\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *MultipleSetsCase::createInstance(Context &context) const
{
    return new MultipleSetsInstance(context, m_params);
}

tcu::TestStatus MultipleSetsInstance::iterate(void)
{
    const auto ctx          = m_context.getContextCommonData();
    const auto shaderStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto descType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Input buffer values.
    std::vector<uint32_t> inputValues(kItemCount, 0u);
    std::iota(begin(inputValues), end(inputValues), 1000000u);

    // Prepare input buffer.
    const auto bufferSize       = static_cast<VkDeviceSize>(de::dataSize(inputValues));
    const auto bufferUsage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

    BufferWithMemory inputBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    deMemcpy(inputBuffer.getAllocation().getHostPtr(), de::dataOrNull(inputValues), de::dataSize(inputValues));
    deMemset(outputBuffer.getAllocation().getHostPtr(), 0, de::dataSize(inputValues));

    // Descriptor set layouts and pipeline layout.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, shaderStages);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const std::vector<VkDescriptorSetLayout> setLayouts{*setLayout, *setLayout}; // Same layout for both sets.
    const auto setCount       = de::sizeU32(setLayouts);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, setLayouts);

    // Prepare descriptor pool and sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, setCount);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, setCount);
    std::vector<Move<VkDescriptorSet>> descriptorSets(setCount);
    for (size_t i = 0u; i < descriptorSets.size(); ++i)
        descriptorSets.at(i) = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, setLayouts.at(i));
    std::vector<VkDescriptorSet> setRaws(setCount, VK_NULL_HANDLE);
    std::transform(begin(descriptorSets), end(descriptorSets), begin(setRaws),
                   [](const Move<VkDescriptorSet> &s) { return s.get(); });

    using Location = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder updateBuilder;
    const std::vector<VkBuffer> buffers{*inputBuffer, *outputBuffer};

    DE_ASSERT(descriptorSets.size() == buffers.size());
    for (size_t i = 0u; i < descriptorSets.size(); ++i)
    {
        const auto bufferDescInfo = makeDescriptorBufferInfo(buffers.at(i), 0ull, VK_WHOLE_SIZE);
        updateBuilder.writeSingle(descriptorSets.at(i).get(), Location::binding(0u), descType, &bufferDescInfo);
    }
    updateBuilder.update(ctx.vkd, ctx.device);

    // Pipeline.
    const auto &binaries  = m_context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    DGCComputePipelineExt dgcPipeline(ctx.vkd, ctx.device, 0u, *pipelineLayout, 0u, *compModule);

    ExecutionSetManagerPtr iesManager;
    iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline.get(), 1u);
    // Rely on the initial value.
    //iesManager->addPipeline(0u, dgcPipeline.get());
    //iesManager->update();

    // DGC Commands layout and buffer.
    const auto cmdsLayoutFlags = static_cast<VkIndirectCommandsLayoutUsageFlagsEXT>(
        m_params.preprocess ? VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT : 0);
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, shaderStages, *pipelineLayout);
    cmdsLayoutBuilder.addComputePipelineToken(0u);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve(cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t));
    dgcData.push_back(0u);
    dgcData.push_back(1u);
    dgcData.push_back(1u);
    dgcData.push_back(kWorkGroupCount);

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    deMemcpy(dgcBuffer.getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesManager->get(), *cmdsLayout, 1u, 0u,
                                         VK_NULL_HANDLE);

    const auto qfIndex = (m_params.useComputeQueue ? m_context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue   = (m_params.useComputeQueue ? m_context.getComputeQueue() : ctx.queue);
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const DGCGenCmdsInfo cmdsInfo(shaderStages, iesManager->get(), *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                  dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                  1u, 0ull, 0u, VK_NULL_HANDLE);

    const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, de::sizeU32(setRaws),
                                  de::dataOrNull(setRaws), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, dgcPipeline.get());
    if (m_params.preprocess)
    {
        ctx.vkd.cmdPreprocessGeneratedCommandsEXT(cmdBuffer, &cmdsInfo.get(), cmdBuffer);
        preprocessToExecuteBarrierExt(ctx.vkd, cmdBuffer);
    }
    {
        const auto isPreprocessed = makeVkBool(m_params.preprocess);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdsInfo.get());
    }
    {
        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Verify results
    invalidateAlloc(ctx.vkd, ctx.device, outputBuffer.getAllocation());
    std::vector<uint32_t> resultValues(inputValues.size(), 0u);
    DE_ASSERT(de::dataSize(resultValues) == static_cast<size_t>(bufferSize));
    deMemcpy(de::dataOrNull(resultValues), outputBuffer.getAllocation().getHostPtr(), de::dataSize(resultValues));

    bool resultOK = true;
    auto &log     = m_context.getTestContext().getLog();
    for (uint32_t i = 0u; i < kItemCount; ++i)
    {
        const auto result   = resultValues.at(i);
        const auto expected = inputValues.at(i);
        if (result != expected)
        {
            resultOK = false;
            log << tcu::TestLog::Message << "Unexpected value at position " << i << ": found " << result
                << " and expected " << expected << tcu::TestLog::EndMessage;
        }
    }

    if (!resultOK)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

class IUBUsageInstance : public vkt::TestInstance
{
public:
    static constexpr uint32_t kBlockSize = 128u;
    static constexpr uint32_t kItemSize  = DE_SIZEOF32(tcu::UVec4); // 16
    static constexpr uint32_t kItemCount = kBlockSize / kItemSize;  // 8
    static constexpr uint32_t kIUBCount  = 2u;

    struct Params
    {
        bool useExecutionSet;
        bool splitSets;
        bool useComputeQueue;
    };

    IUBUsageInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~IUBUsageInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class IUBUsageCase : public vkt::TestCase
{
public:
    IUBUsageCase(tcu::TestContext &testCtx, const std::string &name, const IUBUsageInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~IUBUsageCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const IUBUsageInstance::Params m_params;
};

void IUBUsageCase::checkSupport(Context &context) const
{
    const auto supportType =
        (m_params.useExecutionSet ? DGCComputeSupportType::BIND_PIPELINE : DGCComputeSupportType::BASIC);
    checkDGCExtComputeSupport(context, supportType);
    context.requireDeviceFunctionality("VK_EXT_inline_uniform_block");
}

void IUBUsageCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::vector<uint32_t> iubSets(IUBUsageInstance::kIUBCount, std::numeric_limits<uint32_t>::max());
    std::vector<uint32_t> iubBindings(IUBUsageInstance::kIUBCount, std::numeric_limits<uint32_t>::max());

    if (m_params.splitSets)
    {
        iubSets.at(0u)     = 0u;
        iubSets.at(1u)     = 1u;
        iubBindings.at(0u) = 0u;
        iubBindings.at(1u) = 0u;
    }
    else
    {
        iubSets.at(0u)     = 0u;
        iubSets.at(1u)     = 0u;
        iubBindings.at(0u) = 0u;
        iubBindings.at(1u) = 2u; // Leave space for the output storage buffers.
    }

    for (uint32_t i = 0u; i < IUBUsageInstance::kIUBCount; ++i)
    {
        const bool reverse    = (i > 0u);
        const auto kItemCount = IUBUsageInstance::kItemCount;

        std::ostringstream comp;
        comp << "#version 460\n"
             << "layout(local_size_x=" << kItemCount << ") in;\n"
             << "layout(set=" << iubSets.at(i) << ", binding=" << iubBindings.at(i) << ") uniform IUBBlock" << i
             << " { uvec4 items[" << kItemCount << "]; } iub" << i << ";\n"
             << "layout(set=" << iubSets.at(i) << ", binding=" << (iubBindings.at(i) + 1u) << ") buffer OutBlock" << i
             << " { uvec4 items[" << kItemCount << "]; } ob" << i << ";\n"
             << "uint getWorkGroupSize (void) {\n"
             << "    const uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
             << "    return workGroupSize;\n"
             << "}\n"
             << "void main (void) {\n"
             << (reverse ? "    const uint wgSize = getWorkGroupSize();\n" : "") << "    const uint srcIndex = "
             << (reverse ? "(wgSize - gl_LocalInvocationIndex - 1u)" : "gl_LocalInvocationIndex") << ";\n"
             << "    const uint dstIndex = gl_LocalInvocationIndex;\n"
             << "    ob" << i << ".items[dstIndex] = iub" << i << ".items[srcIndex];\n"
             << "}\n";
        const auto shaderName = "comp" + std::to_string(i);
        programCollection.glslSources.add(shaderName) << glu::ComputeSource(comp.str());
    }
}

TestInstance *IUBUsageCase::createInstance(Context &context) const
{
    return new IUBUsageInstance(context, m_params);
}

tcu::TestStatus IUBUsageInstance::iterate(void)
{
    const auto ctx          = m_context.getContextCommonData();
    const auto shaderStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    // Input values in each IUB.
    using ItemVec    = std::vector<tcu::UVec4>;
    using ItemVecPtr = std::unique_ptr<ItemVec>;

    std::vector<ItemVecPtr> inputValues;
    inputValues.reserve(kIUBCount);

    for (uint32_t i = 0u; i < kIUBCount; ++i)
    {
        inputValues.emplace_back(new ItemVec);

        auto &values = *inputValues.back();
        values.reserve(kItemCount);

        for (uint32_t j = 0u; j < kItemCount; ++j)
        {
            values.push_back(tcu::UVec4((((i + 1u) << 16) | (j << 8) | 0u), (((i + 1u) << 16) | (j << 8) | 1u),
                                        (((i + 1u) << 16) | (j << 8) | 2u), (((i + 1u) << 16) | (j << 8) | 3u)));
        }
    }

    // Output buffers.
    const auto bufferSize       = static_cast<VkDeviceSize>(de::dataSize(*inputValues.front()));
    const auto bufferUsage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, bufferUsage);

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    std::vector<BufferWithMemoryPtr> outputBuffers;
    outputBuffers.reserve(kIUBCount);

    for (uint32_t i = 0u; i < kIUBCount; ++i)
    {
        outputBuffers.emplace_back(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
        void *dataPtr = outputBuffers.back()->getAllocation().getHostPtr();
        deMemset(dataPtr, 0, static_cast<size_t>(bufferSize));
    }

    // Descriptor set layouts and pipeline layout. This must match the shaders.
    std::vector<Move<VkDescriptorSetLayout>> setLayouts;
    setLayouts.reserve(kIUBCount);

    std::unique_ptr<DescriptorSetLayoutBuilder> setLayoutBuilder;
    for (uint32_t i = 0u; i < kIUBCount; ++i)
    {
        if (setLayoutBuilder.get() == nullptr)
            setLayoutBuilder.reset(new DescriptorSetLayoutBuilder);

        setLayoutBuilder->addArrayBinding(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, kBlockSize, shaderStages);
        setLayoutBuilder->addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shaderStages);

        if (m_params.splitSets)
        {
            setLayouts.emplace_back(setLayoutBuilder->build(ctx.vkd, ctx.device));
            setLayoutBuilder.reset(nullptr);
        }
    }

    if (setLayoutBuilder)
    {
        setLayouts.emplace_back(setLayoutBuilder->build(ctx.vkd, ctx.device));
        setLayoutBuilder.reset(nullptr);
    }

    // Prepare descriptor pool and sets.
    const VkDescriptorPoolInlineUniformBlockCreateInfo iubPoolInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                                            //  const void*     pNext;
        kIUBCount, //  uint32_t        maxInlineUniformBlockBindings;
    };
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, kIUBCount * kBlockSize);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kIUBCount);
    const auto setCount       = de::sizeU32(setLayouts);
    const auto descriptorPool = poolBuilder.build(
        ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, setCount, &iubPoolInfo);

    std::vector<Move<VkDescriptorSet>> descriptorSets(setCount);
    for (size_t i = 0u; i < descriptorSets.size(); ++i)
        descriptorSets.at(i) = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayouts.at(i));
    std::vector<VkDescriptorSet> setRaws(setCount, VK_NULL_HANDLE);
    std::transform(begin(descriptorSets), end(descriptorSets), begin(setRaws),
                   [](const Move<VkDescriptorSet> &s) { return s.get(); });

    {
        std::vector<VkWriteDescriptorSetInlineUniformBlock> iubWrites;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkWriteDescriptorSet> writes;

        iubWrites.reserve(kIUBCount);
        bufferInfos.reserve(kIUBCount);
        writes.reserve(kIUBCount * 2u); // One for the IUB and one for the associated storage buffer.

        uint32_t setIndex     = 0u;
        uint32_t bindingIndex = 0u;

        for (uint32_t i = 0u; i < kIUBCount; ++i)
        {
            const VkWriteDescriptorSetInlineUniformBlock iubWrite = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK, //  VkStructureType sType;
                nullptr,                                                     //  const void*     pNext;
                kBlockSize,                                                  //  uint32_t        dataSize;
                inputValues.at(i)->data(),                                   //  const void*     pData;
            };
            iubWrites.push_back(iubWrite);
            const VkWriteDescriptorSet iubGenWrite = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  //  VkStructureType                 sType;
                &iubWrites.back(),                       //  const void*                     pNext;
                *descriptorSets.at(setIndex),            //  VkDescriptorSet                 dstSet;
                bindingIndex++,                          //  uint32_t                        dstBinding;
                0u,                                      //  uint32_t                        dstArrayElement;
                kBlockSize,                              //  uint32_t                        descriptorCount;
                VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, //  VkDescriptorType                descriptorType;
                nullptr,                                 //  const VkDescriptorImageInfo*    pImageInfo;
                nullptr,                                 //  const VkDescriptorBufferInfo*   pBufferInfo;
                nullptr,                                 //  const VkBufferView*             pTexelBufferView;
            };
            writes.push_back(iubGenWrite);

            bufferInfos.push_back(makeDescriptorBufferInfo(outputBuffers.at(i)->get(), 0ull, VK_WHOLE_SIZE));
            const VkWriteDescriptorSet storageGenWrite = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, //  VkStructureType                 sType;
                nullptr,                                //  const void*                     pNext;
                *descriptorSets.at(setIndex),           //  VkDescriptorSet                 dstSet;
                bindingIndex++,                         //  uint32_t                        dstBinding;
                0u,                                     //  uint32_t                        dstArrayElement;
                1u,                                     //  uint32_t                        descriptorCount;
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      //  VkDescriptorType                descriptorType;
                nullptr,                                //  const VkDescriptorImageInfo*    pImageInfo;
                &bufferInfos.back(),                    //  const VkDescriptorBufferInfo*   pBufferInfo;
                nullptr,                                //  const VkBufferView*             pTexelBufferView;
            };
            writes.push_back(storageGenWrite);

            if (m_params.splitSets)
            {
                ++setIndex;
                bindingIndex = 0u;
            }
        }

        ctx.vkd.updateDescriptorSets(ctx.device, de::sizeU32(writes), de::dataOrNull(writes), 0u, nullptr);
    }

    // Pipelines.
    std::vector<VkDescriptorSetLayout> rawSetLayouts;
    rawSetLayouts.reserve(setLayouts.size());
    for (uint32_t i = 0u; i < de::sizeU32(setLayouts); ++i)
        rawSetLayouts.push_back(setLayouts.at(i).get());

    const auto pipelineLayout =
        makePipelineLayout(ctx.vkd, ctx.device, de::sizeU32(rawSetLayouts), de::dataOrNull(rawSetLayouts));

    using DGCComputePipelineExtPtr = std::unique_ptr<DGCComputePipelineExt>;

    std::vector<Move<VkPipeline>> normalPipelines;
    std::vector<DGCComputePipelineExtPtr> dgcPipelines;

    normalPipelines.reserve(kIUBCount);
    dgcPipelines.reserve(kIUBCount);

    const auto &binaries = m_context.getBinaryCollection();
    std::vector<Move<VkShaderModule>> compModules;
    compModules.reserve(kIUBCount);

    for (uint32_t i = 0; i < kIUBCount; ++i)
    {
        const auto shaderName = "comp" + std::to_string(i);
        compModules.emplace_back(createShaderModule(ctx.vkd, ctx.device, binaries.get(shaderName)));

        if (m_params.useExecutionSet)
            dgcPipelines.emplace_back(
                new DGCComputePipelineExt(ctx.vkd, ctx.device, 0u, *pipelineLayout, 0u, *compModules.back()));
        else
            normalPipelines.emplace_back(
                makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModules.back()));
    }

    ExecutionSetManagerPtr iesManager;
    if (m_params.useExecutionSet)
    {
        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipelines.at(0u)->get(), kIUBCount);
        for (uint32_t i = 0u; i < kIUBCount; ++i)
            iesManager->addPipeline(i, dgcPipelines.at(i)->get());
        iesManager->update();
    }

    // DGC Commands layout and buffer.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStages, *pipelineLayout);
    if (m_params.useExecutionSet)
        cmdsLayoutBuilder.addComputePipelineToken(0u);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    using DGCBufferPtr        = std::unique_ptr<DGCBuffer>;
    using PreprocessBufferPtr = std::unique_ptr<PreprocessBufferExt>;

    std::vector<DGCBufferPtr> dgcBuffers;
    std::vector<PreprocessBufferPtr> preprocessBuffers;

    if (m_params.useExecutionSet)
    {
        // We can use a single DGC buffer for all sequences.
        std::vector<uint32_t> dgcData;
        dgcData.reserve(cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t));

        for (uint32_t i = 0u; i < kIUBCount; ++i)
        {
            dgcData.push_back(i);  // Pipeline index.
            dgcData.push_back(1u); // Dispatch
            dgcData.push_back(1u);
            dgcData.push_back(1u);
        }

        const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
        dgcBuffers.emplace_back(new DGCBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize));
        deMemcpy(dgcBuffers.back()->getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

        // And a single preprocess buffer.
        preprocessBuffers.emplace_back(new PreprocessBufferExt(ctx.vkd, ctx.device, ctx.allocator, iesManager->get(),
                                                               *cmdsLayout, kIUBCount, 0u, dgcPipelines.at(0)->get()));
    }
    else
    {
        // Multiple DGC buffers (but with the same contents)
        std::vector<uint32_t> dgcData;
        dgcData.reserve(cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t));
        dgcData.push_back(1u); // Dispatch.
        dgcData.push_back(1u);
        dgcData.push_back(1u);

        const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
        for (uint32_t i = 0u; i < kIUBCount; ++i)
        {
            dgcBuffers.emplace_back(new DGCBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize));
            deMemcpy(dgcBuffers.back()->getAllocation().getHostPtr(), de::dataOrNull(dgcData), de::dataSize(dgcData));

            preprocessBuffers.emplace_back(new PreprocessBufferExt(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE,
                                                                   *cmdsLayout, 1u, 0u, *normalPipelines.at(i)));
        }
    }

    const auto qfIndex = (m_params.useComputeQueue ? m_context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue   = (m_params.useComputeQueue ? m_context.getComputeQueue() : ctx.queue);
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    using DGCGenCmdsInfoPtr = std::unique_ptr<DGCGenCmdsInfo>;
    std::vector<DGCGenCmdsInfoPtr> cmdInfos;
    if (m_params.useExecutionSet)
    {
        const auto &dgcBuffer        = *dgcBuffers.at(0u);
        const auto &preprocessBuffer = *preprocessBuffers.at(0u);

        cmdInfos.emplace_back(new DGCGenCmdsInfo(
            shaderStages, iesManager->get(), *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
            preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), kIUBCount, 0ull, 0u));
    }
    else
    {
        for (uint32_t i = 0u; i < kIUBCount; ++i)
        {
            const auto &dgcBuffer        = *dgcBuffers.at(i);
            const auto &preprocessBuffer = *preprocessBuffers.at(i);

            cmdInfos.emplace_back(new DGCGenCmdsInfo(shaderStages, VK_NULL_HANDLE, *cmdsLayout,
                                                     dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                                     preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                                     1u, 0ull, 0u, normalPipelines.at(i).get()));
        }
    }

    const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, de::sizeU32(setRaws),
                                  de::dataOrNull(setRaws), 0u, nullptr);
    if (m_params.useExecutionSet)
    {
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, dgcPipelines.at(0u)->get());
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdInfos.at(0u)->get());
    }
    else
    {
        for (uint32_t i = 0u; i < kIUBCount; ++i)
        {
            ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *normalPipelines.at(i));
            ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdInfos.at(i)->get());
        }
    }
    {
        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Verify results
    bool resultOK = true;
    auto &log     = m_context.getTestContext().getLog();

    for (uint32_t i = 0u; i < IUBUsageInstance::kIUBCount; ++i)
    {
        const bool reverse = (i > 0u);

        const auto &outputBuffer = *outputBuffers.at(i);
        const auto &valuesVec    = *inputValues.at(i);

        ItemVec resultValues(kItemCount, tcu::UVec4(0u, 0u, 0u, 0u));
        invalidateAlloc(ctx.vkd, ctx.device, outputBuffer.getAllocation());
        DE_ASSERT(de::dataSize(resultValues) == static_cast<size_t>(bufferSize));
        deMemcpy(de::dataOrNull(resultValues), outputBuffer.getAllocation().getHostPtr(), de::dataSize(resultValues));

        for (uint32_t j = 0u; j < kItemCount; ++j)
        {
            const auto expectedIdx = (reverse ? (kItemCount - j - 1u) : j);
            const auto result      = resultValues.at(j);
            const auto expected    = valuesVec.at(expectedIdx);

            if (result != expected)
            {
                resultOK = false;
                log << tcu::TestLog::Message << "Unexpected value in output buffer " << i << " at position " << j
                    << ": found " << result << " and expected " << expected << tcu::TestLog::EndMessage;
            }
        }
    }

    if (!resultOK)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

// Descriptor buffers and push descriptors combined with DGC.
class DBPDInstance : public vkt::TestInstance
{
public:
    static constexpr uint32_t kLocalSize     = 64u;
    static constexpr uint32_t kSequenceCount = 2u;
    static constexpr uint32_t kItemCount     = kSequenceCount * kLocalSize;

    struct Params
    {
        bool useExecutionSet;
    };

    DBPDInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~DBPDInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class DBPDCase : public vkt::TestCase
{
public:
    DBPDCase(tcu::TestContext &testCtx, const std::string &name, const DBPDInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DBPDCase(void) = default;

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const DBPDInstance::Params m_params;
};

void DBPDCase::checkSupport(Context &context) const
{
    const auto stages     = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindStages = (m_params.useExecutionSet ? stages : 0u);
    checkDGCExtSupport(context, stages, bindStages);

    context.requireDeviceFunctionality("VK_EXT_descriptor_buffer");
    context.requireDeviceFunctionality("VK_KHR_push_descriptor");

    const auto &dbFeatures = context.getDescriptorBufferFeaturesEXT();
    if (!dbFeatures.descriptorBufferPushDescriptors)
        TCU_THROW(NotSupportedError, "descriptorBufferPushDescriptors not supported");
}

void DBPDCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << DBPDInstance::kLocalSize << ") in;\n"
         << "layout (constant_id=0) const uint valueOffset = 0u;\n"
         << "layout (set=0, binding=0) readonly buffer InBufferBlock { uint values[" << DBPDInstance::kItemCount
         << "]; } ib;\n"
         << "layout (set=0, binding=1) buffer OutBufferBlock { uint values[" << DBPDInstance::kItemCount << "]; } ob;\n"
         << "layout (push_constant, std430) uniform PCBlock { uint indexOffset; } pc;\n"
         << "void main (void) {\n"
         << "    const uint index = gl_LocalInvocationIndex + pc.indexOffset;\n"
         << "    ob.values[index] = ib.values[index] + valueOffset;\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *DBPDCase::createInstance(Context &context) const
{
    return new DBPDInstance(context, m_params);
}

tcu::TestStatus DBPDInstance::iterate(void)
{
    const auto ctx               = m_context.getContextCommonData();
    const auto kInitialValueBase = 1000u;
    const auto kValueOffset      = 10000;
    const auto stages            = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto descriptorType    = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto bindPoint         = VK_PIPELINE_BIND_POINT_COMPUTE;

    std::vector<uint32_t> initialValues(kItemCount, 0u);
    std::iota(begin(initialValues), end(initialValues), kInitialValueBase);

    const auto bufferSize       = de::dataSize(initialValues);
    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    BufferWithMemory inputBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc   = inputBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(initialValues), de::dataSize(initialValues));
    }

    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, MemoryRequirement::HostVisible);
    {
        auto &alloc   = outputBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemset(dataPtr, 0, bufferSize);
    }

    std::vector<uint32_t> valueOffsets;
    valueOffsets.reserve(kSequenceCount);
    valueOffsets.push_back(0u);
    if (m_params.useExecutionSet)
        valueOffsets.push_back(kValueOffset);
    else
        valueOffsets.push_back(0u); // Value offset will be zero for all items.
    DE_ASSERT(valueOffsets.size() == kSequenceCount);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Set layout and pipeline layout.
    const auto pcSize  = DE_SIZEOF32(uint32_t);
    const auto pcRange = makePushConstantRange(stages, 0u, pcSize);

    const auto setLayoutFlags = (VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT |
                                 VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descriptorType, stages);
    setLayoutBuilder.addSingleBinding(descriptorType, stages);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device, setLayoutFlags);

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    using DGCComputePipelineExtPtr = std::unique_ptr<DGCComputePipelineExt>;
    Move<VkPipeline> normalPipeline;
    std::vector<DGCComputePipelineExtPtr> dgcPipelines;

    ExecutionSetManagerPtr iesManager;
    VkIndirectExecutionSetEXT iesHandle = VK_NULL_HANDLE;

    const VkSpecializationMapEntry specMapEntry{
        0u,
        0u,
        sizeof(uint32_t),
    };

    if (m_params.useExecutionSet)
    {
        const auto pipelineCreateFlags = VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

        for (uint32_t i = 0u; i < kSequenceCount; ++i)
        {
            // Take spec constant value from valueOffsets.at(i),
            const VkSpecializationInfo specInfo{
                1u,
                &specMapEntry,
                sizeof(uint32_t),
                &valueOffsets.at(i),
            };

            dgcPipelines.emplace_back(new DGCComputePipelineExt(ctx.vkd, ctx.device, pipelineCreateFlags,
                                                                *pipelineLayout, 0u, *compModule, &specInfo));
        }

        iesManager = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipelines.front()->get(), kSequenceCount);
        for (uint32_t i = 0u; i < kSequenceCount; ++i)
            iesManager->addPipeline(i, dgcPipelines.at(i)->get());
        iesManager->update();
        iesHandle = iesManager->get();
    }
    else
    {
        const VkSpecializationInfo specInfo{
            1u,
            &specMapEntry,
            sizeof(uint32_t),
            &valueOffsets.at(0u),
        };

        const auto pipelineCreateFlags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        normalPipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, pipelineCreateFlags, nullptr,
                                             *compModule, 0u, &specInfo);
    }

    // Create descriptor buffer.
    const auto &dbProperties = m_context.getDescriptorBufferPropertiesEXT();
    const auto &bufferlessPD = dbProperties.bufferlessPushDescriptors;

    VkDeviceSize setLayoutSize;
    ctx.vkd.getDescriptorSetLayoutSizeEXT(ctx.device, *setLayout, &setLayoutSize);

    // This helps set a minimum in case the implementation returns 0.
    setLayoutSize = std::max(setLayoutSize, dbProperties.descriptorBufferOffsetAlignment);

    const VkBufferUsageFlags descriptorBufferUsage =
        (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
         (bufferlessPD ? 0 : VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT));

    const auto descriptorBufferCreateInfo = makeBufferCreateInfo(setLayoutSize, descriptorBufferUsage);
    BufferWithMemory descriptorBuffer(ctx.vkd, ctx.device, ctx.allocator, descriptorBufferCreateInfo,
                                      (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));
    const auto descriptorBufferAddress = getBufferDeviceAddress(ctx.vkd, ctx.device, *descriptorBuffer);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    VkDescriptorBufferBindingPushDescriptorBufferHandleEXT descriptorBufferPDHandle = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT,
        NULL,
        descriptorBuffer.get(),
    };

    const VkDescriptorBufferBindingInfoEXT bindingInfos = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
        (bufferlessPD ? nullptr : &descriptorBufferPDHandle),
        descriptorBufferAddress,
        descriptorBufferUsage,
    };
    ctx.vkd.cmdBindDescriptorBuffersEXT(cmdBuffer, 1u, &bindingInfos);

    // Start of the buffer.
    const uint32_t setBufferIndices     = 0u;
    const VkDeviceSize setBufferOffsets = 0ull;
    ctx.vkd.cmdSetDescriptorBufferOffsetsEXT(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &setBufferIndices,
                                             &setBufferOffsets);

    // For non-push descriptors, we would get the descriptor info for each buffer and store those into the descriptor
    // buffer at specific binding offsets obtained with vkGetDescriptorSetLayoutBindingOffsetEXT. However, for push
    // descriptors we just push that information.
    const std::vector<VkDescriptorBufferInfo> descBufferInfos = {
        VkDescriptorBufferInfo{
            inputBuffer.get(),
            0ull,
            bufferSize,
        },
        VkDescriptorBufferInfo{
            outputBuffer.get(),
            0ull,
            bufferSize,
        },
    };

    std::vector<VkWriteDescriptorSet> pushWrites;
    pushWrites.reserve(descBufferInfos.size());
    for (uint32_t i = 0u; i < de::sizeU32(descBufferInfos); ++i)
    {
        pushWrites.emplace_back(VkWriteDescriptorSet{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            VK_NULL_HANDLE, // Not used for descriptor buffer.
            i,
            0u,
            1u,
            descriptorType,
            nullptr,
            &descBufferInfos.at(i),
            nullptr,
        });
    }
    ctx.vkd.cmdPushDescriptorSet(cmdBuffer, bindPoint, *pipelineLayout, 0u, de::sizeU32(pushWrites),
                                 de::dataOrNull(pushWrites));

    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stages, *pipelineLayout);
    if (m_params.useExecutionSet)
        cmdsLayoutBuilder.addExecutionSetToken(0u, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT, stages);
    cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(), pcRange);
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    std::vector<uint32_t> dgcData;
    dgcData.reserve(kSequenceCount * cmdsLayoutBuilder.getStreamStride() / DE_SIZEOF32(uint32_t));

    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        if (m_params.useExecutionSet)
            dgcData.push_back(i); // Pipeline index in the execution set.

        const uint32_t indexOffset = i * kLocalSize;
        dgcData.push_back(indexOffset); // Push constant value.

        dgcData.push_back(1u); // Dispatch size x, y, z
        dgcData.push_back(1u);
        dgcData.push_back(1u);
    }

    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, de::dataSize(dgcData));
    {
        auto &alloc   = dgcBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(dgcData), de::dataSize(dgcData));
    }

    const auto preprocessPipeline = (m_params.useExecutionSet ? VK_NULL_HANDLE : *normalPipeline);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, iesHandle, *cmdsLayout, kSequenceCount, 0u,
                                         preprocessPipeline);

    const DGCGenCmdsInfo cmdsInfo(stages, iesHandle, *cmdsLayout, dgcBuffer.getDeviceAddress(), dgcBuffer.getSize(),
                                  preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(), kSequenceCount, 0ull,
                                  0u, preprocessPipeline);

#if 0
    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        const auto pipeline = (m_params.useExecutionSet ? dgcPipelines.at(i)->get() : *normalPipeline);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline);
        const uint32_t indexOffset = i * kLocalSize;
        ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, stages, 0u, pcSize, &indexOffset);
        ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    }
#else
    const auto preBoundPipeline = (m_params.useExecutionSet ? dgcPipelines.at(0u)->get() : *normalPipeline);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, preBoundPipeline);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
#endif

    const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &preHostBarrier);

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    std::vector<uint32_t> outputValues(kItemCount, 0u);
    {
        auto &alloc = outputBuffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);
        void *dataPtr = alloc.getHostPtr();

        DE_ASSERT(de::dataSize(outputValues) == bufferSize);
        deMemcpy(de::dataOrNull(outputValues), dataPtr, de::dataSize(outputValues));
    }

    bool resultOK = true;
    auto &log     = m_context.getTestContext().getLog();

    for (uint32_t i = 0u; i < kSequenceCount; ++i)
        for (uint32_t j = 0u; j < kLocalSize; ++j)
        {
            const uint32_t index   = i * kLocalSize + j;
            const auto valueOffset = valueOffsets.at(i);
            const auto expected    = initialValues.at(index) + valueOffset;
            const auto result      = outputValues.at(index);

            if (expected != result)
            {
                log << tcu::TestLog::Message << "Unexpected value in output buffer position " << index << ": expected "
                    << expected << " but found " << result << tcu::TestLog::EndMessage;
                resultOK = false;
            }
        }

    if (!resultOK)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCComputeMiscTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "misc"));

    for (const auto executeCount : {64u, 1024u, 8192u})
        for (const auto useComputeQueue : {false, true})
        {
            const ManyDispatchesParams params{executeCount, useComputeQueue};
            const auto queueVariant = (useComputeQueue ? "_compute_queue" : "_universal_queue");
            const auto testName     = std::string("execute_many_") + std::to_string(executeCount) + queueVariant;
            addFunctionCaseWithPrograms(mainGroup.get(), testName, manyDispatchesCheckSupport,
                                        manyDispatchesInitPrograms, manyExecutesRun, params);
        }

    for (const auto executeCount : {64u, 1024u, 8192u, 131072u})
        for (const auto useComputeQueue : {false, true})
        {
            const ManyDispatchesParams params{executeCount, useComputeQueue};
            const auto queueVariant = (useComputeQueue ? "_compute_queue" : "_universal_queue");
            const auto testName     = std::string("many_sequences_") + std::to_string(executeCount) + queueVariant;
            addFunctionCaseWithPrograms(mainGroup.get(), testName, manyDispatchesCheckSupport,
                                        manyDispatchesInitPrograms, manySequencesRun, params);
        }

    mainGroup->addChild(new ScratchSpaceCase(testCtx, "scratch_space"));

    for (const uint32_t pcBytes : {128u, 256u, 4096u})
        for (const bool partial : {false, true})
            for (const bool preprocess : {false, true})
                for (const bool useExecutionSet : {false, true})
                    for (const bool useComputeQueue : {false, true})
                        for (const bool pushDescriptor : {false, true})
                        {
                            const MaxPushConstantRangeInstance::Params params{
                                pcBytes, partial, preprocess, useExecutionSet, pushDescriptor, useComputeQueue};

                            const std::string testName =
                                std::string("max_pc_range_") + std::to_string(pcBytes) +
                                (partial ? "_partial" : "_full") + (preprocess ? "_preprocess" : "") +
                                (useExecutionSet ? "_with_execution_set" : "") +
                                (pushDescriptor ? "_push_descriptor" : "") + (useComputeQueue ? "_cq" : "");

                            mainGroup->addChild(new MaxPushConstantRangeCase(testCtx, testName, params));
                        }

    for (const bool preprocess : {false, true})
        for (const bool useComputeQueue : {false, true})
        {
            const MultipleSetsInstance::Params params{
                preprocess,
                useComputeQueue,
            };

            const std::string testName =
                std::string("multiple_sets") + (preprocess ? "_preprocess" : "") + (useComputeQueue ? "_cq" : "");

            mainGroup->addChild(new MultipleSetsCase(testCtx, testName, params));
        }

    for (const bool useExecutionSet : {false, true})
        for (const bool splitSets : {false, true})
            for (const bool useComputeQueue : {false, true})
            {
                const IUBUsageInstance::Params params{
                    useExecutionSet,
                    splitSets,
                    useComputeQueue,
                };

                const std::string testName = std::string("iubs") + (useExecutionSet ? "_with_ies" : "") +
                                             (splitSets ? "_multiset" : "") + (useComputeQueue ? "_cq" : "");

                mainGroup->addChild(new IUBUsageCase(testCtx, testName, params));
            }

    for (const bool useExecutionSet : {false, true})
        for (const bool useComputeQueue : {false, true})
        {
            const TwoCmdBuffersParams params{useExecutionSet, useComputeQueue};
            const std::string testName =
                std::string("two_cmd_buffers") + (useComputeQueue ? "_cq" : "") + (useExecutionSet ? "_with_ies" : "");
            addFunctionCaseWithPrograms(mainGroup.get(), testName, twoCmdBuffersCheckSupport, twoCmdBuffersInitPrograms,
                                        twoCmdBuffersRun, params);
        }

    for (const bool useExecutionSet : {false, true})
    {
        const DBPDInstance::Params params{useExecutionSet};
        const std::string testName =
            std::string("descriptor_buffer_push_descriptor") + (useExecutionSet ? "_with_ies" : "");
        mainGroup->addChild(new DBPDCase(testCtx, testName, params));
    }

    addFunctionCaseWithPrograms(mainGroup.get(), "null_set_layouts_info", nullSetLayoutsInfoCheckSupport,
                                nullSetLayoutsInfoPrograms, nullSetLayoutsInfoRun);

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
