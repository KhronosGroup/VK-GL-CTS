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
 * \brief Device Generated Commands EXT Compute "vkGet*" Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeGetInfoTestsExt.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktTestCaseUtil.hpp"

#include <memory>
#include <vector>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

enum class ConstantCommandsMemoryReqsCase
{
    BASIC_CASE,
    BASIC_CASE_WITH_PIPELINE,
    INCREASE_COUNT,
    MAX_SEQUENCE_COUNT,
    IGNORE_UNORDERED,
};

bool pushConstantsNeeded(ConstantCommandsMemoryReqsCase memReqsCase)
{
    return (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT);
}

void initBasicProgram(vk::SourceCollections &dst, bool usePushConstants)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (set=0, binding=0) buffer OutputBufferBlock { uint results[]; } output_buffer;\n"
         << "layout (local_size_x=64, local_size_y=1, local_size_z=1) in;\n"
         << (usePushConstants ? "layout (push_constant, std430) uniform PCBlock { uint value; } pc;\n" : "")
         << "void main (void)\n"
         << "{\n"
         << "    const uint totalWorkGroupSize     = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
         << "    const uint workGroupIndex         = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + "
            "gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
         << "    const uint globalInvocationIndex  = workGroupIndex * totalWorkGroupSize + gl_LocalInvocationIndex;\n"
         << "\n"
         << "    const uint offset = " << (usePushConstants ? "pc.value" : "0") << ";\n"
         << "    output_buffer.results[globalInvocationIndex] = uint(sqrt(float(globalInvocationIndex))) + offset;\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

void initPrograms(vk::SourceCollections &dst, ConstantCommandsMemoryReqsCase cmdMemCase)
{
    initBasicProgram(dst, pushConstantsNeeded(cmdMemCase));
}

// Make a basic descriptor set layout that matches the basic compute program above (1 binding for a storage buffer).
Move<VkDescriptorSetLayout> makeBasicDescriptorSetLayout(const DeviceInterface &vkd, const VkDevice device)
{
    DescriptorSetLayoutBuilder builder;
    builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    return builder.build(vkd, device);
}

inline void checkDGCComputeBasicSupportCmd(Context &context, ConstantCommandsMemoryReqsCase)
{
    checkDGCExtComputeSupport(context, DGCComputeSupportType::BASIC);
}

inline void checkDGCComputePipelineSupportCmd(Context &context, ConstantCommandsMemoryReqsCase)
{
    checkDGCExtComputeSupport(context, DGCComputeSupportType::BIND_PIPELINE);
}

tcu::TestStatus constantCommandsMemReqs(Context &context, ConstantCommandsMemoryReqsCase memReqsCase)
{
    const auto ctx         = context.getContextCommonData();
    const auto shaderStage = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    // We need an indirect execution set if the commands layout contains a pipeline token.
    const bool indirectExecutionSetNeeded = (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE_WITH_PIPELINE);
    const auto maxPipelineCount           = (indirectExecutionSetNeeded ? 64u : 0u); // Non-zero reasonable count.

    // We'll pretend to use push constants in one particular case, even if they're not used by the compute shader.
    const bool needsPushConstant = pushConstantsNeeded(memReqsCase);
    const auto pcSize            = static_cast<uint32_t>(sizeof(uint32_t));
    const auto pcRange           = makePushConstantRange(shaderStage, 0u, pcSize);

    // We will pretend to use preprocess in some cases.
    const bool needsPreprocess = (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE);

    // For the flag ignore test, we'll need two actual indirect command layouts.
    const auto builderCount = (memReqsCase == ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED ? 2u : 1u);

    // The set and pipeline layout are always needed to create the indirect commands layout.
    const auto setLayout = makeBasicDescriptorSetLayout(ctx.vkd, ctx.device);
    const auto pipelineLayout =
        makePipelineLayout(ctx.vkd, ctx.device, *setLayout, (needsPushConstant ? &pcRange : nullptr));

    using DGCComputePipelineExtPtr = std::unique_ptr<DGCComputePipelineExt>;

    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    ExecutionSetManagerPtr executionSetManager;
    VkIndirectExecutionSetEXT executionSetHandle = VK_NULL_HANDLE;
    Move<VkPipeline> pipeline;
    DGCComputePipelineExtPtr dgcPipeline;

    if (indirectExecutionSetNeeded)
    {
        dgcPipeline.reset(new DGCComputePipelineExt(ctx.vkd, ctx.device, 0ull, *pipelineLayout, 0u, *compModule));
        executionSetManager =
            makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, dgcPipeline->get(), maxPipelineCount);
        executionSetHandle = executionSetManager->get();
    }
    else
    {
        // Pipeline needed.
        pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);
    }

    // Now build a command sequence. It will be different depending on the case. The flags will also vary a bit.
    const auto layoutUsageFlags =
        (needsPreprocess ? static_cast<VkIndirectCommandsLayoutUsageFlagsEXT>(
                               VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT) :
                           0u);

    using BuilderPtr = std::unique_ptr<IndirectCommandsLayoutBuilderExt>;

    std::vector<BuilderPtr> builders;
    for (uint32_t i = 0u; i < builderCount; ++i)
    {
        const auto extraFlags = ((i > 0u) ? static_cast<VkIndirectCommandsLayoutUsageFlagsEXT>(
                                                VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT) :
                                            0u);
        builders.emplace_back(
            new IndirectCommandsLayoutBuilderExt((layoutUsageFlags | extraFlags), shaderStage, *pipelineLayout));
        auto &currentBuilder = *builders.back();

        if (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE)
        {
            currentBuilder.addDispatchToken(0u);
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE_WITH_PIPELINE)
        {
            currentBuilder.addComputePipelineToken(0u);
            currentBuilder.addDispatchToken(currentBuilder.getStreamRange());
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT)
        {
            currentBuilder.addPushConstantToken(0u, pcRange);
            currentBuilder.addDispatchToken(currentBuilder.getStreamRange());
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::MAX_SEQUENCE_COUNT)
        {
            currentBuilder.addDispatchToken(0u);
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED)
        {
            currentBuilder.addDispatchToken(0u);
        }
        else
            DE_ASSERT(false);
    }

    std::vector<Move<VkIndirectCommandsLayoutEXT>> cmdLayouts;
    for (const auto &builderPtr : builders)
        cmdLayouts.push_back(builderPtr->build(ctx.vkd, ctx.device));

    // For the maxSequencesCount value, we'll use something reasonable.
    auto maxSequencesCount = 0u;
    if (memReqsCase == ConstantCommandsMemoryReqsCase::MAX_SEQUENCE_COUNT)
    {
        const auto &properties = context.getDeviceGeneratedCommandsPropertiesEXT();
        maxSequencesCount      = properties.maxIndirectSequenceCount;
    }
    else
        maxSequencesCount = 1024u;

    // Get an initial set of requirements.
    auto memReqsInfo   = DGCMemReqsInfo(executionSetHandle, *cmdLayouts.at(0u), maxSequencesCount, 0u, *pipeline);
    const auto memReqs = getGeneratedCommandsMemoryRequirementsExt(ctx.vkd, ctx.device, *memReqsInfo);

    // Now request it a second time, varying some parameters if needed.
    if (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT)
        memReqsInfo.setMaxSequenceCount(memReqsInfo.get().maxSequenceCount * 2u);

    if (memReqsCase == ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED)
        memReqsInfo.setCommandsLayout(*cmdLayouts.at(1u));

    // Get a second set of memory requirements.
    const auto otherMemReqs = getGeneratedCommandsMemoryRequirementsExt(ctx.vkd, ctx.device, *memReqsInfo);

    // Compare requirements.
    auto &log = context.getTestContext().getLog();
    std::string errorMessage;

    if (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT)
    {
        if (memReqs.size > otherMemReqs.size)
            errorMessage = "Required memory size got smaller despite increasing maxSequencesCount";
    }
    else
    {
        if (memReqs.size != otherMemReqs.size)
            errorMessage = "Required memory size changed between calls";
    }

    if (errorMessage.empty() && memReqs.alignment != otherMemReqs.alignment)
        errorMessage = "Required memory alignment changed between calls";

    if (errorMessage.empty() && memReqs.memoryTypeBits != otherMemReqs.memoryTypeBits)
        errorMessage = "Required memory type bites changed between calls";

    if (!errorMessage.empty())
    {
        log << tcu::TestLog::Message << "First: " << memReqs << "\n"
            << "Second: " << otherMemReqs << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail(errorMessage);
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCComputeGetInfoTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "get_info"));

    const struct
    {
        ConstantCommandsMemoryReqsCase cmdMemCase;
        bool needsPipeline;
        const char *name;
    } cmdMemCases[] = {
        {ConstantCommandsMemoryReqsCase::BASIC_CASE, false, "basic_case"},
        {ConstantCommandsMemoryReqsCase::BASIC_CASE_WITH_PIPELINE, true, "basic_case_with_pipeline"},
        {ConstantCommandsMemoryReqsCase::INCREASE_COUNT, false, "increase_count"},
        {ConstantCommandsMemoryReqsCase::MAX_SEQUENCE_COUNT, false, "max_sequence_count"},
        {ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED, false, "ignore_unordered_flag"},
    };
    for (const auto &testCase : cmdMemCases)
    {
        const auto supportCheck =
            (testCase.needsPipeline ? checkDGCComputePipelineSupportCmd : checkDGCComputeBasicSupportCmd);
        const auto testName = std::string("constant_cmd_memory_requirements_") + testCase.name;
        addFunctionCaseWithPrograms(mainGroup.get(), testName, supportCheck, initPrograms, constantCommandsMemReqs,
                                    testCase.cmdMemCase);
    }

    return mainGroup.release();
}
} // namespace DGC
} // namespace vkt
