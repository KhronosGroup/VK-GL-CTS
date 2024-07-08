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
 * \brief Device Generated Commands Compute Preprocess Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputePreprocessTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <vector>
#include <memory>
#include <map>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

enum class Method
{
    UNIVERSAL_QUEUE = 0,
    COMPUTE_QUEUE,
    PREPROCESS_COMPUTE_EXECUTE_UNIVERSAL,
    PREPROCESS_UNIVERSAL_EXECUTE_COMPUTE,
};

enum class CountBuffer
{
    NO = 0,       // No count buffer.
    YES,          // Using a count buffer.
    YES_BUT_ZERO, // Using a count buffer, but the count is zero.
};

struct PreprocessParams
{
    Method method;
    CountBuffer countBuffer;
};

bool usesUniversalQueue(Method method)
{
    return (method != Method::COMPUTE_QUEUE);
}

bool usesComputeQueue(Method method)
{
    return (method != Method::UNIVERSAL_QUEUE);
}

void checkDGCComputeAndQueueSupport(Context &context, PreprocessParams params)
{
    checkDGCComputeSupport(context, false, false);

    if (usesComputeQueue(params.method))
        context.getComputeQueue(); // Throws NotSupportedError if not available.
}

// Store the push constant value in the output buffer.
void storePushConstantProgram(SourceCollections &dst, PreprocessParams)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint value; } outputBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint value; } pc;\n"
         << "void main (void) { outputBuffer.value = pc.value; }\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

// Record a memory barrier from compute shader writes to host reads.
void shaderWriteToHostBarrier(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer)
{
    const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &preHostBarrier);
}

// Creates a buffer memory barrier structure to sync access from preprocessing to execution.
VkBufferMemoryBarrier makePreprocessToExecuteBarrier(VkBuffer buffer, VkDeviceSize size, uint32_t srcQueueIndex,
                                                     uint32_t dstQueueIndex)
{
    return makeBufferMemoryBarrier(VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                   buffer, 0ull, size, srcQueueIndex, dstQueueIndex);
}

// We'll use a single sequence buffer and a single storage buffer, but we'll work with 2 indirect commands executions by using
// offsets into them.
tcu::TestStatus parallelPreprocessRun(Context &context, PreprocessParams params)
{
    const auto &ctx           = context.getContextCommonData();
    const auto descType       = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags     = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint      = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto u32Size        = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto kExecuteCount  = 2u;
    auto &log                 = context.getTestContext().getLog();
    const bool useCountBuffer = (params.countBuffer != CountBuffer::NO);
    const auto fakeCount      = 100u; // For the info structure when using a count buffer.
    const auto realCount      = 1u;

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

    // Sequence count buffer.
    std::vector<BufferWithMemoryPtr> sequencesCountBuffers;

    if (useCountBuffer)
    {
        sequencesCountBuffers.reserve(kExecuteCount);
        const auto createInfo = makeBufferCreateInfo(u32Size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        for (uint32_t i = 0u; i < kExecuteCount; ++i)
        {
            sequencesCountBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));

            auto &buffer     = *sequencesCountBuffers.back();
            auto &allocation = buffer.getAllocation();
            void *dataPtr    = allocation.getHostPtr();

            const uint32_t count = ((params.countBuffer == CountBuffer::YES_BUT_ZERO) ? 0u : realCount);
            deMemcpy(dataPtr, &count, sizeof(count));
            flushAlloc(ctx.vkd, ctx.device, allocation);
        }
    }

    // Output buffers.
    const auto outputBufferCreateInfo = makeBufferCreateInfo(u32Size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    std::vector<BufferWithMemoryPtr> outputBuffers;
    outputBuffers.reserve(kExecuteCount);

    for (uint32_t i = 0u; i < kExecuteCount; ++i)
    {
        outputBuffers.emplace_back(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo,
                                                        MemoryRequirement::HostVisible));

        auto &buffer     = *outputBuffers.back();
        auto &allocation = buffer.getAllocation();
        void *data       = allocation.getHostPtr();

        deMemset(data, 0, static_cast<size_t>(u32Size));
        flushAlloc(ctx.vkd, ctx.device, allocation);
    }

    // Descriptor set layout, pool and sets preparation.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, stageFlags);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device); // The layout is the same for both executions.

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType, kExecuteCount);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, kExecuteCount);

    std::vector<Move<VkDescriptorSet>> descriptorSets;
    descriptorSets.reserve(kExecuteCount);
    for (uint32_t i = 0u; i < kExecuteCount; ++i)
        descriptorSets.push_back(makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout));

    DescriptorSetUpdateBuilder setUpdateBuilder;
    {
        for (uint32_t i = 0u; i < kExecuteCount; ++i)
        {
            const auto outputBufferDescInfo = makeDescriptorBufferInfo(outputBuffers.at(i)->get(), 0ull, u32Size);
            setUpdateBuilder.writeSingle(descriptorSets.at(i).get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                         descType, &outputBufferDescInfo);
        }
    }
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Push constants
    const auto pcSize  = static_cast<uint32_t>(u32Size);
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Generated commands layout: push constant followed by a dispatch, with preprocessing.
    IndirectCommandsLayoutBuilder cmdsLayoutBuilder(VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV,
                                                    bindPoint);
    cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *pipelineLayout, stageFlags, 0u, pcSize);
    cmdsLayoutBuilder.addDispatchToken(0u, pcSize);
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generate value for push constant "i". We'll reuse this in the verification part.
    const auto getPushConstant = [](uint32_t i) { return i + 100u; };

    // Generated indirect commands buffer contents. Note we'll split this in two executions.
    const auto kSequenceItems = 4u; // push constant + indirect dispatch args

    std::vector<std::vector<uint32_t>> genCmdDataVecs;
    genCmdDataVecs.reserve(kExecuteCount);

    for (uint32_t i = 0u; i < kExecuteCount; ++i)
    {
        genCmdDataVecs.emplace_back();
        auto &data = genCmdDataVecs.back();
        data.reserve(kSequenceItems);

        data.push_back(getPushConstant(i));
        data.push_back(1u); // VkDispatchIndirectCommand::x
        data.push_back(1u); // VkDispatchIndirectCommand::y
        data.push_back(1u); // VkDispatchIndirectCommand::z
    }

    // Buffers for generated indirect commands.
    const auto genCmdsBufferSize       = static_cast<VkDeviceSize>(de::dataSize(genCmdDataVecs.at(0u)));
    const auto genCmdsBufferCreateInfo = makeBufferCreateInfo(genCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

    std::vector<BufferWithMemoryPtr> genCmdBuffers;
    for (uint32_t i = 0; i < kExecuteCount; ++i)
    {
        const auto &cmdData = genCmdDataVecs.at(i);
        genCmdBuffers.emplace_back(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferCreateInfo,
                                                        MemoryRequirement::HostVisible));

        auto &cmdsBuffer = *genCmdBuffers.back();
        auto &alloc      = cmdsBuffer.getAllocation();
        void *ptr        = alloc.getHostPtr();

        deMemcpy(ptr, de::dataOrNull(cmdData), de::dataSize(cmdData));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Preprocess buffers for 1 sequence. We'll make them separate in this case.
    using PreprocessBufferPtr = std::unique_ptr<PreprocessBuffer>;

    std::vector<PreprocessBufferPtr> preprocessBuffers;
    preprocessBuffers.reserve(kExecuteCount);
    for (uint32_t i = 0u; i < kExecuteCount; ++i)
    {
        // We need to pass the fake count here in some cases due to VUID-VkGeneratedCommandsInfoNV-sequencesCount-02917.
        const auto preprocessSequencesCount = (useCountBuffer ? fakeCount : realCount);
        preprocessBuffers.emplace_back(new PreprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, bindPoint, *pipeline,
                                                            *cmdsLayout, preprocessSequencesCount));
    }

    std::vector<VkIndirectCommandsStreamNV> streamInfos;
    std::vector<VkGeneratedCommandsInfoNV> cmdInfos;

    // These calls are critical so the pointers to the VkIndirectCommandsStreamNV structs don't change.
    streamInfos.reserve(kExecuteCount);
    cmdInfos.reserve(kExecuteCount);

    for (uint32_t i = 0u; i < kExecuteCount; ++i)
    {
        const auto &preprocessBuffer  = *(preprocessBuffers.at(i).get());
        const auto &countBuffer       = (useCountBuffer ? sequencesCountBuffers.at(i)->get() : VK_NULL_HANDLE);
        const auto infoSequencesCount = (useCountBuffer ? fakeCount : realCount);

        streamInfos.push_back(makeIndirectCommandsStreamNV(genCmdBuffers.at(i)->get(), 0ull));

        cmdInfos.push_back(VkGeneratedCommandsInfoNV{
            VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            bindPoint,                                    // VkPipelineBindPoint pipelineBindPoint;
            *pipeline,                                    // VkPipeline pipeline;
            *cmdsLayout,                                  // VkIndirectCommandsLayoutNV indirectCommandsLayout;
            1u,                                           // uint32_t streamCount;
            &streamInfos.back(),                          // const VkIndirectCommandsStreamNV* pStreams;
            infoSequencesCount,                           // uint32_t sequencesCount;
            preprocessBuffer.get(),                       // VkBuffer preprocessBuffer;
            0ull,                                         // VkDeviceSize preprocessOffset;
            preprocessBuffer.getSize(),                   // VkDeviceSize preprocessSize;
            countBuffer,                                  // VkBuffer sequencesCountBuffer;
            0ull,                                         // VkDeviceSize sequencesCountOffset;
            VK_NULL_HANDLE,                               // VkBuffer sequencesIndexBuffer;
            0ull,                                         // VkDeviceSize sequencesIndexOffset;
        });
    }

    // Keys used for the map below, which may contain one or more sets of info.
    const auto kUniversalKey = 0u;
    const auto kComputeKey   = 1u;

    struct QueueInfo
    {
        uint32_t qfIndex;
        VkQueue queue;
        VkCommandPool cmdPool;
    };

    std::vector<Move<VkCommandPool>> commandPools;
    std::map<uint32_t, QueueInfo> queueInfoMap;

    const auto needsUniversal = usesUniversalQueue(params.method);
    const auto needsCompute   = usesComputeQueue(params.method);

    if (needsUniversal)
    {
        commandPools.emplace_back(makeCommandPool(ctx.vkd, ctx.device, ctx.qfIndex));
        queueInfoMap[kUniversalKey] = QueueInfo{ctx.qfIndex, ctx.queue, commandPools.back().get()};
    }

    if (needsCompute)
    {
        const auto qfIndex = static_cast<uint32_t>(context.getComputeQueueFamilyIndex());
        commandPools.emplace_back(makeCommandPool(ctx.vkd, ctx.device, qfIndex));
        queueInfoMap[kComputeKey] = QueueInfo{qfIndex, context.getComputeQueue(), commandPools.back().get()};
    }

    const auto &preprocessQueueInfo =
        ((params.method == Method::UNIVERSAL_QUEUE || params.method == Method::PREPROCESS_UNIVERSAL_EXECUTE_COMPUTE) ?
             queueInfoMap[kUniversalKey] :
             queueInfoMap[kComputeKey]);

    const auto &executeQueueInfo =
        ((params.method == Method::UNIVERSAL_QUEUE || params.method == Method::PREPROCESS_COMPUTE_EXECUTE_UNIVERSAL) ?
             queueInfoMap[kUniversalKey] :
             queueInfoMap[kComputeKey]);

    const bool queueSwitch = (executeQueueInfo.qfIndex != preprocessQueueInfo.qfIndex);

    // These may be used to transfer buffers from the preprocess queue to the execution queue.
    std::vector<VkBufferMemoryBarrier> ownershipBarriers;

    // We will preprocess one sequence first, and wait for the preprocessing to finish using a fence.
    {
        const auto cmdBufferPtr =
            allocateCommandBuffer(ctx.vkd, ctx.device, preprocessQueueInfo.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer = cmdBufferPtr.get();

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.at(0u).get(), 0u,
                                      nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        ctx.vkd.cmdPreprocessGeneratedCommandsNV(cmdBuffer, &cmdInfos.at(0u));

        if (queueSwitch)
        {
            // When using queue switches, the preprocessing queue will not be the same as the execution queue and buffers need to be
            // transferred between queues. This is the "release" barrier for those buffers. Make sure we cover every buffer in use.

            const auto &preprocessInfo = cmdInfos.at(0u);
            DE_UNREF(preprocessInfo); // For release builds.
            DE_ASSERT(preprocessInfo.sequencesIndexBuffer == VK_NULL_HANDLE);

            ownershipBarriers.push_back(makePreprocessToExecuteBarrier(
                outputBuffers.at(0u)->get(), u32Size, preprocessQueueInfo.qfIndex, executeQueueInfo.qfIndex));
            ownershipBarriers.push_back(makePreprocessToExecuteBarrier(
                genCmdBuffers.at(0u)->get(), genCmdsBufferSize, preprocessQueueInfo.qfIndex, executeQueueInfo.qfIndex));
            ownershipBarriers.push_back(
                makePreprocessToExecuteBarrier(preprocessBuffers.at(0u)->get(), preprocessBuffers.at(0u)->getSize(),
                                               preprocessQueueInfo.qfIndex, executeQueueInfo.qfIndex));
            if (useCountBuffer)
                ownershipBarriers.push_back(makePreprocessToExecuteBarrier(sequencesCountBuffers.at(0u)->get(), u32Size,
                                                                           preprocessQueueInfo.qfIndex,
                                                                           executeQueueInfo.qfIndex));

            ctx.vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                                       VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u, nullptr,
                                       de::sizeU32(ownershipBarriers), de::dataOrNull(ownershipBarriers), 0u, nullptr);
        }

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, preprocessQueueInfo.queue, cmdBuffer);
    }

    // Separately, execute the preprocessed commands and preprocess+execute the second sequence.
    {
        Move<VkCommandBuffer> preprocessCmdBufferPtr;     // Separate command buffer needed sometimes.
        Move<VkFence> executionFence;                     // Separate fence needed sometimes.
        VkCommandBuffer executeCmdBuffer{VK_NULL_HANDLE}; // Saved for later sometimes.
        VkCommandBuffer cmdBuffer{VK_NULL_HANDLE};        // The current command buffer we are recording.

        const auto executeCmdBufferPtr =
            allocateCommandBuffer(ctx.vkd, ctx.device, executeQueueInfo.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        cmdBuffer = *executeCmdBufferPtr;

        beginCommandBuffer(ctx.vkd, cmdBuffer);

        if (queueSwitch)
        {
            // This is the "acquire" barrier to transfer buffer ownership for execution. See above.
            ctx.vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                                       VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u, nullptr,
                                       de::sizeU32(ownershipBarriers), de::dataOrNull(ownershipBarriers), 0u, nullptr);
        }

        // Execution of preprocessed commands.
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.at(0u).get(), 0u,
                                      nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_TRUE, &cmdInfos.at(0u));

        if (queueSwitch)
        {
            // Barrier to the host and finish.
            shaderWriteToHostBarrier(ctx.vkd, cmdBuffer);
            endCommandBuffer(ctx.vkd, cmdBuffer);
            executeCmdBuffer = cmdBuffer; // Save this for later.

            // Separate command buffer for the second sequence.
            // Note we call this the "preprocess" command buffer, but we'll do both preprocessing and execution.
            preprocessCmdBufferPtr = allocateCommandBuffer(ctx.vkd, ctx.device, preprocessQueueInfo.cmdPool,
                                                           VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            cmdBuffer              = *preprocessCmdBufferPtr;

            beginCommandBuffer(ctx.vkd, cmdBuffer);
        }

        // Preprocessing and execution of the second sequence.
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.at(1u).get(), 0u,
                                      nullptr);
        if (queueSwitch)
        {
            // Rebind pipeline. If there's no queue switch, the previous pipeline bind command above still applies because there's
            // no command buffer change.
            ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
        }
        ctx.vkd.cmdPreprocessGeneratedCommandsNV(cmdBuffer, &cmdInfos.at(1u));
        preprocessToExecuteBarrier(ctx.vkd, cmdBuffer);
        ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_TRUE, &cmdInfos.at(1u));
        shaderWriteToHostBarrier(ctx.vkd, cmdBuffer);
        endCommandBuffer(ctx.vkd, cmdBuffer);

        // Submit all pending command buffers "simultaneously".
        const bool extraSubmit = (executeCmdBuffer != VK_NULL_HANDLE);
        if (extraSubmit)
            executionFence = submitCommands(ctx.vkd, ctx.device, executeQueueInfo.queue, executeCmdBuffer);
        const auto preprocessingFence = submitCommands(ctx.vkd, ctx.device, preprocessQueueInfo.queue, cmdBuffer);

        // Wait on all fences.
        waitForFence(ctx.vkd, ctx.device, *preprocessingFence);
        if (extraSubmit)
            waitForFence(ctx.vkd, ctx.device, *executionFence);
    }

    // Verify results.
    const bool execution       = (params.countBuffer != CountBuffer::YES_BUT_ZERO);
    uint32_t outputBufferValue = 0u;
    bool fail                  = false;

    for (size_t i = 0u; i < outputBuffers.size(); ++i)
    {
        const auto &buffer = *outputBuffers.at(i);
        const auto &alloc  = buffer.getAllocation();
        invalidateAlloc(ctx.vkd, ctx.device, alloc);

        void *data = alloc.getHostPtr();
        deMemcpy(&outputBufferValue, data, sizeof(outputBufferValue));
        const auto reference = (execution ? getPushConstant(static_cast<uint32_t>(i)) : 0u);

        if (outputBufferValue != reference)
        {
            fail = true;
            log << tcu::TestLog::Message << "Unexpected value found in output buffer at position " << i << ": expected "
                << reference << " but found " << outputBufferValue << tcu::TestLog::EndMessage;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

} // namespace

// Note the smoke tests already contain some basic preprocessing cases.
tcu::TestCaseGroup *createDGCComputePreprocessTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "preprocess"));

    const struct
    {
        Method method;
        const char *name;
    } methodCases[] = {
        {Method::UNIVERSAL_QUEUE, "universal"},
        {Method::COMPUTE_QUEUE, "compute"},
        {Method::PREPROCESS_COMPUTE_EXECUTE_UNIVERSAL, "compute_with_universal_exec"},
        {Method::PREPROCESS_UNIVERSAL_EXECUTE_COMPUTE, "universal_with_compute_exec"},
    };

    const struct
    {
        CountBuffer countBuffer;
        const char *suffix;
    } countBufferCases[] = {
        {CountBuffer::NO, ""},
        {CountBuffer::YES, "_with_count_buffer"},
        {CountBuffer::YES_BUT_ZERO, "_with_count_buffer_zero_count"},
    };

    for (const auto &methodCase : methodCases)
        for (const auto &countBufferCase : countBufferCases)
        {
            const PreprocessParams params{methodCase.method, countBufferCase.countBuffer};
            addFunctionCaseWithPrograms(
                mainGroup.get(), std::string("parallel_preprocessing_") + methodCase.name + countBufferCase.suffix,
                checkDGCComputeAndQueueSupport, storePushConstantProgram, parallelPreprocessRun, params);
        }

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
