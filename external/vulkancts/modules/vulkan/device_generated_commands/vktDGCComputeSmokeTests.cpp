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
 * \brief Device Generated Commands Compute "Smoke" Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeSmokeTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtil.hpp"

#include "deRandom.hpp"

#include <sstream>
#include <vector>
#include <memory>
#include <map>
#include <utility>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

struct SmokeTestParams
{
    const uint32_t sequenceCount; // Number of sequences to generate.
    const bool hostVisible;       // Make the indirect commands buffer host-visible.
    const bool preCompute;        // Generate the indirect commands from another compute shader.
    const bool preProcess;        // Do an explicit preprocessing stage.
    const bool computeQueue;      // Attempt to use a compute queue instead of a universal queue.
    const bool unordered;         // Include the VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_NV bit?
};

const uint32_t kLocalInvocations = 64u;
const uint32_t kMaxWorkGroups    = 256u;

class SmokeTestInstance : public vkt::TestInstance
{
public:
    SmokeTestInstance(Context &context, const SmokeTestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~SmokeTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    const SmokeTestParams m_params;
};

class SmokeTestCase : public vkt::TestCase
{
public:
    SmokeTestCase(tcu::TestContext &testCtx, const std::string &name, const SmokeTestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~SmokeTestCase(void)
    {
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new SmokeTestInstance(context, m_params);
    }
    void checkSupport(Context &context) const override;

protected:
    const SmokeTestParams m_params;
};

// These smoke tests will use dispatches but not pipeline binds nor push constants. This makes it a bit challenging to verify work
// is being dispatched correctly given that, when processing a number of sequences, each of the sequences will dispatch some work
// that will lack any "customization" for each dispatch. The absence of push constants does not let us select an output buffer or
// output buffer range that would allow invocations from a particular dispatch to leave a "work-completed-mark" in a custom location
// for that dispatch.
//
// The main mechanism used is, then, the following: each dispatch will launch a pseudorandom number of workgroups in a predefined
// short range like [1, 256]. The major dimension (x, y or z) of each workgroup will also be chosen pseudorandomly. In each
// dispatch, the local invocations (fixed number: 64) will atomically increase an atomic counter inside a buffer by 1. The key is
// that all invocations in a workgroup will act on a specific counter accessed using the workgroup index.
//
// Because each dispatch will have a pseudorandom number of workgroups, this means some counters will be increased more than others,
// and the end result should be fairly unique per group of sequences.
//
// For example, lets imagine we execute 4 sequences (i.e. 4 dispatches), with 22, 7, 53 and 30 workgroups each. The result should
// be:
//
// * Counters in the range [0, 7)   should end up with value 64*4 (affected by all 4 dispatches).
// * Counters in the range [7, 22)  should end up with value 64*3 (only affected by 3 dispatches).
// * Counters in the range [22, 30) should end up with value 64*2 (only affected by 2 dispatches).
// * Counters in the range [30, 53) should end up with value 64*1 (inly affected by 1 dispatch).
// * Other counters should stay at zero.
//
void SmokeTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3, 0u);

    std::ostringstream comp;
    comp << "#version 460\n"
         << "#extension GL_KHR_memory_scope_semantics : enable\n"
         << "layout (set=0, binding=0, std430) buffer AtomicCountersBlock {\n"
         << "    uint value[" << kMaxWorkGroups << "];\n"
         << "} atomicCounters;\n"
         << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
         << "void main ()\n"
         << "{\n"
         << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + "
            "gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
         << "    atomicAdd(atomicCounters.value[workGroupIndex], 1u, gl_ScopeQueueFamily, gl_StorageSemanticsBuffer, "
            "(gl_SemanticsAcquireRelease | gl_SemanticsMakeAvailable | gl_SemanticsMakeVisible));\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str()) << buildOptions;

    if (m_params.preCompute)
    {
        // When using preCompute, we'll launch the following "generation" shader, which will simply copy the indirect commands from
        // the initial buffer to a second buffer. The code below supposes only one workgroup will be dispatched, and the local
        // invocations of the workgroup will copy one chunk of the commands each.
        const auto commandsPerInv = ((m_params.sequenceCount + (kLocalInvocations - 1)) / kLocalInvocations);
        std::ostringstream gen;
        gen << "#version 460\n"
            << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
            << "struct VkDispatchIndirectCommand {\n"
            << "    uint x;\n"
            << "    uint y;\n"
            << "    uint z;\n"
            << "};\n"
            << "const uint sequenceCount = " << m_params.sequenceCount << "u;\n"
            << "layout (set=0, binding=0, std430) readonly buffer HostBufferCommands {\n"
            << "    VkDispatchIndirectCommand commands[sequenceCount];\n"
            << "} hostBuffer;\n"
            << "layout (set=0, binding=1, std430) buffer IndirectCommandsBlock {\n"
            << "    VkDispatchIndirectCommand commands[sequenceCount];\n"
            << "} cmdBuffer;\n"
            << "void main (void) {\n"
            << "    const uint commandsPerInv = " << commandsPerInv << ";\n"
            << "    const uint firstCommand = gl_LocalInvocationIndex * commandsPerInv;\n"
            << "    for (uint i = 0u; i < commandsPerInv; ++i) {\n"
            << "        const uint cmdIndex = firstCommand + i;\n"
            << "        if (cmdIndex < sequenceCount) {\n"
            << "            cmdBuffer.commands[cmdIndex] = hostBuffer.commands[cmdIndex];\n"
            << "        }\n"
            << "    }\n"
            << "}\n";
        programCollection.glslSources.add("gen") << glu::ComputeSource(gen.str());
    }
}

void SmokeTestCase::checkSupport(Context &context) const
{
    checkDGCComputeSupport(context, false, false);

    // This is needed for the scopes and semantics.
    const auto &mmFeatures = context.getVulkanMemoryModelFeatures();
    if (!mmFeatures.vulkanMemoryModel)
        TCU_THROW(NotSupportedError, "vulkanMemoryModel not supported");

    if (m_params.computeQueue)
        context.getComputeQueue(); // Will throw NotSupportedError if no such queue is available.
}

tcu::TestStatus SmokeTestInstance::iterate(void)
{
    const auto ctx       = m_context.getContextCommonData();
    const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto stages    = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto descType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // Used by all buffers in shaders.
    const auto qfIndex   = (m_params.computeQueue ? m_context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue     = (m_params.computeQueue ? m_context.getComputeQueue() : ctx.queue);

    const uint32_t randomizerSeed =
        ((m_params.sequenceCount << 4) | (static_cast<uint32_t>(m_params.hostVisible) << 3) |
         (static_cast<uint32_t>(m_params.preCompute) << 2) | (static_cast<uint32_t>(m_params.preProcess) << 1) |
         (static_cast<uint32_t>(m_params.computeQueue)));

    // Indirect commands layout.
    VkIndirectCommandsLayoutUsageFlagsNV cmdLayoutUsageFlags = 0u;

    if (m_params.preProcess)
        cmdLayoutUsageFlags |= static_cast<VkIndirectCommandsLayoutUsageFlagsNV>(
            VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV);

    // Note compute pipelines are unordered by default. Explicitly adding the flag should be a no-op and nothing should crash.
    if (m_params.unordered)
        cmdLayoutUsageFlags |= static_cast<VkIndirectCommandsLayoutUsageFlagsNV>(
            VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_NV);

    IndirectCommandsLayoutBuilder cmdLayoutBuilder(cmdLayoutUsageFlags, bindPoint);
    cmdLayoutBuilder.addDispatchToken(0u, 0u);
    const auto cmdLayout = cmdLayoutBuilder.build(ctx.vkd, ctx.device);

    // Pipelines, built in advance.
    Move<VkDescriptorSetLayout> compSetLayout;
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(descType, stages);
        compSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }

    Move<VkDescriptorSetLayout> genSetLayout;
    if (m_params.preCompute)
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(descType, stages);
        setLayoutBuilder.addSingleBinding(descType, stages);
        genSetLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);
    }

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto genModule =
        (m_params.preCompute ? createShaderModule(ctx.vkd, ctx.device, binaries.get("gen")) : Move<VkShaderModule>());

    const auto compPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *compSetLayout);
    const auto genPipelineLayout =
        (m_params.preCompute ? makePipelineLayout(ctx.vkd, ctx.device, *genSetLayout) : Move<VkPipelineLayout>());

    const auto compPipeline = makeComputePipeline(ctx.vkd, ctx.device, *compPipelineLayout, *compModule);
    const auto genPipeline =
        (m_params.preCompute ? makeComputePipeline(ctx.vkd, ctx.device, *genPipelineLayout, *genModule) :
                               Move<VkPipeline>());

    // Create a buffer to contain execution results.
    std::vector<uint32_t> results(kMaxWorkGroups, 0u);

    const auto resultsBufferSize       = static_cast<VkDeviceSize>(de::dataSize(results));
    const auto resultsBufferUsage      = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto resultsBufferCreateInfo = makeBufferCreateInfo(resultsBufferSize, resultsBufferUsage);

    BufferWithMemory resultsBuffer(ctx.vkd, ctx.device, ctx.allocator, resultsBufferCreateInfo,
                                   MemoryRequirement::HostVisible);
    auto &resultsBufferAlloc = resultsBuffer.getAllocation();
    void *resultsBufferData  = resultsBufferAlloc.getHostPtr();

    deMemcpy(resultsBufferData, de::dataOrNull(results), de::dataSize(results));
    flushAlloc(ctx.vkd, ctx.device, resultsBufferAlloc);

    // Pseudorandomly generate indirect dispatch commands.
    de::Random rnd(randomizerSeed);
    std::vector<VkDispatchIndirectCommand> indirectCommands;
    indirectCommands.reserve(m_params.sequenceCount);
    for (uint32_t i = 0; i < m_params.sequenceCount; ++i)
    {
        const auto dispatchSize = static_cast<uint32_t>(rnd.getInt(1, static_cast<int>(kMaxWorkGroups)));
        const auto majorDim     = rnd.getInt(0, 2);

        VkDispatchIndirectCommand cmd{1u, 1u, 1u};
        auto &axis = (majorDim == 0 ? cmd.x : (majorDim == 1 ? cmd.y : cmd.z));
        axis       = dispatchSize;
        indirectCommands.push_back(cmd);
    }

    // Depending on m_params.hostVisible and m_params.preCompute, we will have one or more buffers, with different usage flags.
    // We will always have a host-visible initial buffer that we will use to copy the above commands into.
    //
    // if (preCompute)
    //   if (hostVisible)
    //     2 buffers, with the second one also being host-visible, used as the dst buffer by the "gen" shader.
    //   else
    //     Same as above, but the second one is not host-visible.
    // else
    //   if (hostVisible)
    //     Single buffer, host visible.
    //   else
    //     2 buffers, the second one device-local and a transfer between them.
    const auto kMemoryRequirementHostVisible = MemoryRequirement::HostVisible;
    const auto kMemoryRequirementLocal       = MemoryRequirement::Local;

    bool secondBuffer                           = false;
    VkBufferUsageFlags firstBufferUsage         = 0u;
    VkBufferUsageFlags secondBufferUsage        = 0u;
    const MemoryRequirement *firstBufferMemReq  = &kMemoryRequirementHostVisible;
    const MemoryRequirement *secondBufferMemReq = &kMemoryRequirementLocal;

    if (m_params.preCompute)
    {
        secondBuffer      = true;
        firstBufferUsage  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        secondBufferUsage = (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

        if (m_params.hostVisible)
            secondBufferMemReq = &kMemoryRequirementHostVisible;
    }
    else
    {
        if (m_params.hostVisible)
        {
            firstBufferUsage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }
        else
        {
            secondBuffer      = true;
            firstBufferUsage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            secondBufferUsage = (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        }
    }

    const auto indirectCmdsBufferSize = static_cast<VkDeviceSize>(de::dataSize(indirectCommands));

    const auto firstBufferCreateInfo = makeBufferCreateInfo(indirectCmdsBufferSize, firstBufferUsage);
    BufferWithMemory firstCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, firstBufferCreateInfo, *firstBufferMemReq);
    auto &firstCmdsBufferAlloc            = firstCmdsBuffer.getAllocation();
    void *firstCmdsBufferData             = firstCmdsBufferAlloc.getHostPtr();
    VkBuffer indirectCommandsBufferHandle = firstCmdsBuffer.get();

    // Copy indirect commands to the first commands buffer.
    deMemcpy(firstCmdsBufferData, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
    flushAlloc(ctx.vkd, ctx.device, firstCmdsBufferAlloc);

    // If needed, create a second buffer to hold indirect commands.
    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
    BufferWithMemoryPtr secondCmdsBuffer;
    if (secondBuffer)
    {
        const auto secondCmdsBufferCreateInfo = makeBufferCreateInfo(indirectCmdsBufferSize, secondBufferUsage);
        secondCmdsBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, secondCmdsBufferCreateInfo, *secondBufferMemReq));
        indirectCommandsBufferHandle = secondCmdsBuffer->get();
    }

    // Create preprocess buffer.
    PreprocessBuffer preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, bindPoint, *compPipeline, *cmdLayout,
                                      m_params.sequenceCount);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

    // Prepare descriptor sets.
    Move<VkDescriptorPool> genDescriptorPool;
    Move<VkDescriptorPool> compDescriptorPool;
    Move<VkDescriptorSet> genDescriptorSet;
    Move<VkDescriptorSet> compDescriptorSet;

    if (m_params.preCompute)
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType, 2u);
        genDescriptorPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        genDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *genDescriptorPool, *genSetLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        {
            const auto firstBufferDescInfo = makeDescriptorBufferInfo(*firstCmdsBuffer, 0ull, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*genDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                      &firstBufferDescInfo);
        }
        {
            DE_ASSERT(secondBuffer);
            const auto secondBufferDescInfo = makeDescriptorBufferInfo(secondCmdsBuffer->get(), 0ull, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*genDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), descType,
                                      &secondBufferDescInfo);
        }
        updateBuilder.update(ctx.vkd, ctx.device);
    }
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType);
        compDescriptorPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        compDescriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *compDescriptorPool, *compSetLayout);

        DescriptorSetUpdateBuilder updateBuilder;
        const auto resultsBufferDescInfo = makeDescriptorBufferInfo(*resultsBuffer, 0ull, VK_WHOLE_SIZE);
        updateBuilder.writeSingle(*compDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descType,
                                  &resultsBufferDescInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (m_params.preCompute)
    {
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *genPipelineLayout, 0u, 1u, &genDescriptorSet.get(), 0u,
                                      nullptr);
        ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *genPipeline);
        ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
        // Synchronize precompute writes with indirect dispatches.
        const auto postComputeBarrier =
            makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, &postComputeBarrier);
    }
    else if (!m_params.hostVisible)
    {
        const auto copyRegion = makeBufferCopy(0ull, 0ull, indirectCmdsBufferSize);
        ctx.vkd.cmdCopyBuffer(cmdBuffer, firstCmdsBuffer.get(), secondCmdsBuffer->get(), 1u, &copyRegion);
        // Synchronize transfer with indirect dispatches.
        const auto postTransferBarrier =
            makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, &postTransferBarrier);
    }
    const auto indirectCommandsStream = makeIndirectCommandsStreamNV(indirectCommandsBufferHandle, 0ull);
    const VkGeneratedCommandsInfoNV generatedCommandsInfo = {
        VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        bindPoint,                                    // VkPipelineBindPoint pipelineBindPoint;
        *compPipeline,                                // VkPipeline pipeline;
        *cmdLayout,                                   // VkIndirectCommandsLayoutNV indirectCommandsLayout;
        1u,                                           // uint32_t streamCount;
        &indirectCommandsStream,                      // const VkIndirectCommandsStreamNV* pStreams;
        m_params.sequenceCount,                       // uint32_t sequencesCount;
        *preprocessBuffer,                            // VkBuffer preprocessBuffer;
        0ull,                                         // VkDeviceSize preprocessOffset;
        preprocessBuffer.getSize(),                   // VkDeviceSize preprocessSize;
        VK_NULL_HANDLE,                               // VkBuffer sequencesCountBuffer;
        0ull,                                         // VkDeviceSize sequencesCountOffset;
        VK_NULL_HANDLE,                               // VkBuffer sequencesIndexBuffer;
        0ull,                                         // VkDeviceSize sequencesIndexOffset;
    };
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *compPipelineLayout, 0u, 1u, &compDescriptorSet.get(), 0u,
                                  nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *compPipeline);
    if (m_params.preProcess)
    {
        ctx.vkd.cmdPreprocessGeneratedCommandsNV(cmdBuffer, &generatedCommandsInfo);
        preprocessToExecuteBarrier(ctx.vkd, cmdBuffer);
    }
    ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, makeVkBool(m_params.preProcess), &generatedCommandsInfo);
    // Make results buffer available on the host.
    {
        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, resultsBufferAlloc);
    deMemcpy(de::dataOrNull(results), resultsBufferData, de::dataSize(results));

    // How many commands have a particular work group count. E.g. if the pseudorandom work group counts generated above are 21, 43,
    // 21 and 15, the map will end up containing:
    //     15 -> 1
    //     21 -> 2
    //     43 -> 1
    std::map<uint32_t, uint32_t> counterMap;
    for (const auto &indirectCmd : indirectCommands)
    {
        const auto numWorkGroups = indirectCmd.x * indirectCmd.y * indirectCmd.z;
        auto itr                 = counterMap.find(numWorkGroups);
        if (itr == end(counterMap))
        {
            auto insertionResult = counterMap.insert(std::make_pair(numWorkGroups, 0u));
            itr                  = insertionResult.first;
        }
        ++(itr->second);
    }
    // Now accumulate results in reverse order to calculate how many dispatches contain that count or more. The map ends up with:
    //     15 -> 4 dispatches
    //     21 -> 3 dispatches
    //     43 -> 1 dispatch
    {
        uint32_t accumulated = 0u;
        for (auto itr = counterMap.rbegin(); itr != counterMap.rend(); ++itr)
        {
            itr->second += accumulated;
            accumulated = itr->second;
        }
    }
    // Now we now that results in the range [0, 15) have been affected by 4 dispatches, values in [15, 21) by 3 and values in the
    // range [21, 43) by 1. The rest stay at 0. For this last point, insert a new element in the map if needed.
    {
        const auto itr = counterMap.rbegin();
        if (itr != counterMap.rend() && itr->first != kMaxWorkGroups)
            counterMap[kMaxWorkGroups] = 0u;
    }

    // Verify results are as expected.
    auto &log   = m_context.getTestContext().getLog();
    bool testOk = true;

    uint32_t rangeBegin = 0u;
    uint32_t rangeEnd   = 0u;

    for (const auto &rangeEndDispatches : counterMap)
    {
        rangeEnd                 = rangeEndDispatches.first;
        const auto expectedValue = rangeEndDispatches.second * kLocalInvocations;
        log << tcu::TestLog::Message << "Verifying range [" << rangeBegin << ", " << rangeEnd << ") contains "
            << expectedValue << tcu::TestLog::EndMessage;
        for (uint32_t i = rangeBegin; i < rangeEnd; ++i)
        {
            if (results.at(i) != expectedValue)
            {
                testOk = false;
                log << tcu::TestLog::Message << "results[" << i << "] = " << results.at(i) << " but expected "
                    << expectedValue << tcu::TestLog::EndMessage;
            }
        }
        rangeBegin = rangeEnd;
    }

    if (!testOk)
    {
        // Log the full indirect commands list and the full results buffer.
        log << tcu::TestLog::Message << "Indirect commands:" << tcu::TestLog::EndMessage;
        for (size_t i = 0u; i < indirectCommands.size(); ++i)
        {
            const auto &indirectCmd = indirectCommands.at(i);
            log << tcu::TestLog::Message << "  indirectCommands[" << i << "] = {" << indirectCmd.x << ", "
                << indirectCmd.y << ", " << indirectCmd.z << "}" << tcu::TestLog::EndMessage;
        }

        log << tcu::TestLog::Message << "Results buffer:" << tcu::TestLog::EndMessage;
        for (size_t i = 0u; i < results.size(); ++i)
            log << tcu::TestLog::Message << "  results[" << i << "] = " << results.at(i) << tcu::TestLog::EndMessage;

        return tcu::TestStatus::fail("Unexpected results; check log for details");
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCComputeSmokeTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "smoke"));

    for (const auto sequenceCount : {4u, 1024u})
        for (const auto hostVisible : {false, true})
            for (const auto preCompute : {false, true})
                for (const auto preProcess : {false, true})
                    for (const auto computeQueue : {false, true})
                    {
                        const bool unordered = (sequenceCount == 1024u); // Some variants will set the unordered bit.
                        SmokeTestParams params{sequenceCount, hostVisible,  preCompute,
                                               preProcess,    computeQueue, unordered};

                        const auto hvString = (hostVisible ? "host_visible" : "device_local");
                        const auto pcString = (preCompute ? "from_compute" : "from_host");
                        const auto ppString = (preProcess ? "explicit_preprocess" : "implicit_preprocess");
                        const auto cqString = (computeQueue ? "compute_queue" : "universal_queue");
                        const auto testName = std::to_string(params.sequenceCount) + "_sequences_" + hvString + "_" +
                                              pcString + "_" + ppString + "_" + cqString;

                        mainGroup->addChild(new SmokeTestCase(testCtx, testName, params));
                    }

    return mainGroup.release();
}
} // namespace DGC
} // namespace vkt
