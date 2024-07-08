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
 * \brief Device Generated Commands Compute "vkGet*" Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeGetInfoTests.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "deRandom.hpp"

#include <sstream>
#include <memory>
#include <vector>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

inline void checkDGCComputePipelineSupport(Context &context)
{
    checkDGCComputeSupport(context, true, false);
}

inline void checkDGCComputeCaptureReplaySupport(Context &context)
{
    checkDGCComputeSupport(context, true, true);
}

void initBasicProgram(vk::SourceCollections &dst)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (set=0, binding=0) buffer OutputBufferBlock { uint results[]; } output_buffer;\n"
         << "layout (local_size_x=64, local_size_y=1, local_size_z=1) in;\n"
         << "void main (void)\n"
         << "{\n"
         << "    const uint totalWorkGroupSize     = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
         << "    const uint workGroupIndex         = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + "
            "gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
         << "    const uint globalInvocationIndex  = workGroupIndex * totalWorkGroupSize + gl_LocalInvocationIndex;\n"
         << "\n"
         << "    output_buffer.results[globalInvocationIndex] = uint(sqrt(float(globalInvocationIndex)));\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

// Make a basic descriptor set layout that matches the basic compute program above (1 binding for a storage buffer).
Move<VkDescriptorSetLayout> makeBasicDescriptorSetLayout(const DeviceInterface &vkd, const VkDevice device)
{
    DescriptorSetLayoutBuilder builder;
    builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    return builder.build(vkd, device);
}

tcu::TestStatus constantPipelineMemoryRequirements(Context &context)
{
    const auto ctx = context.getContextCommonData();

    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    const auto setLayout      = makeBasicDescriptorSetLayout(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
        *compModule,                                         // VkShaderModule module;
        "main",                                              // const char* pName;
        nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
    };

    VkComputePipelineCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV,    // VkPipelineCreateFlags flags;
        shaderStageCreateInfo,                          // VkPipelineShaderStageCreateInfo stage;
        *pipelineLayout,                                // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        -1,                                             // int32_t basePipelineIndex;
    };

    // Retrieve the base memory requirements for a simple call.
    const auto baseMemReqs = getPipelineIndirectMemoryRequirementsNV(ctx.vkd, ctx.device, &createInfo);

    // Add a VkComputePipelineIndirectBufferInfoNV structure to the chain that should be ignored, according to spec.
    de::Random rnd(1706013938u);
    VkComputePipelineIndirectBufferInfoNV garbageBufferInfo = initVulkanStructure();
    de::fillWithRandomData(rnd, &garbageBufferInfo.deviceAddress);
    de::fillWithRandomData(rnd, &garbageBufferInfo.size);
    de::fillWithRandomData(rnd, &garbageBufferInfo.pipelineDeviceAddressCaptureReplay);

    VkComputePipelineIndirectBufferInfoNV bufferInfoBackup;
    deMemcpy(&bufferInfoBackup, &garbageBufferInfo, sizeof(garbageBufferInfo));

    createInfo.pNext              = &garbageBufferInfo;
    const auto withBufferInfoReqs = getPipelineIndirectMemoryRequirementsNV(ctx.vkd, ctx.device, &createInfo);

    // The structure should not have been modified.
    if (deMemCmp(&garbageBufferInfo, &bufferInfoBackup, sizeof(bufferInfoBackup)) != 0)
        return tcu::TestStatus::fail(
            "VkComputePipelineIndirectBufferInfoNV was modified in vkGetPipelineIndirectMemoryRequirementsNV");

    // Finally request the base memory requirements again.
    createInfo.pNext        = nullptr;
    const auto retryMemReqs = getPipelineIndirectMemoryRequirementsNV(ctx.vkd, ctx.device, &createInfo);

    if (!equalMemoryRequirements(baseMemReqs, withBufferInfoReqs) ||
        !equalMemoryRequirements(baseMemReqs, retryMemReqs))
    {
        auto &log = context.getTestContext().getLog();
        log << tcu::TestLog::Message << "Base memory requirements: " << baseMemReqs << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Memory requirements with buffer info: " << withBufferInfoReqs
            << tcu::TestLog::EndMessage;
        log << tcu::TestLog::Message << "Memory requirements on retry: " << retryMemReqs << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail(
            "Indirect memory requirements are not constant for the same pipeline; check log for details");
    }

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus constantPipelineDeviceAddress(Context &context)
{
    const auto ctx = context.getContextCommonData();

    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    const auto setLayout      = makeBasicDescriptorSetLayout(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    DGCComputePipelineMetaDataPool metaDataPool;
    DGCComputePipeline pipeline(metaDataPool, ctx.vkd, ctx.device, ctx.allocator, 0u, *pipelineLayout, 0u, *compModule);

    // Retrieve the pipeline device address manually and check it matches the saved one.
    const auto info    = makePipelineIndirectDeviceAddressInfoNV(*pipeline);
    const auto address = ctx.vkd.getPipelineIndirectDeviceAddressNV(ctx.device, &info);

    if (address != pipeline.getIndirectDeviceAddress())
        return tcu::TestStatus::fail("Pipeline indirect device address is not constant");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus constantPipelineCaptureReplayAddress(Context &context)
{
    const auto ctx = context.getContextCommonData();

    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    const auto setLayout      = makeBasicDescriptorSetLayout(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    vk::VkDeviceAddress captureAddress = 0ull;
    vk::VkDeviceAddress replayAddress  = 0ull;

    // Retrieve the capture replay address first.
    // Note metadata pools have the capture/replay flag enabled.
    {
        DGCComputePipelineMetaDataPool metaDataPool(1u, true);
        DGCComputePipeline pipeline(metaDataPool, ctx.vkd, ctx.device, ctx.allocator, 0u, *pipelineLayout, 0u,
                                    *compModule);
        captureAddress = pipeline.getIndirectDeviceAddress();

        // Note the pipeline and pool go out of scope here, destroying resources.
    }

    // Re-create the pipeline providing the capture/replay address.
    // Make sure both match.
    DGCComputePipelineMetaDataPool metaDataPool(1u, true);
    DGCComputePipeline pipeline(metaDataPool, ctx.vkd, ctx.device, ctx.allocator, 0u, *pipelineLayout, 0u, *compModule,
                                nullptr, captureAddress);
    replayAddress = pipeline.getIndirectDeviceAddress();

    if (captureAddress != replayAddress)
        return tcu::TestStatus::fail("Capture and replay addresses are not equal");

    return tcu::TestStatus::pass("Pass");
}

enum class ConstantCommandsMemoryReqsCase
{
    BASIC_CASE,
    BASIC_CASE_WITH_PIPELINE,
    INCREASE_COUNT,
    MAX_SEQUENCE_COUNT,
    IGNORE_UNORDERED,
};

inline void checkDGCComputeBasicSupportCmd(Context &context, ConstantCommandsMemoryReqsCase)
{
    checkDGCComputeSupport(context, false, false);
}

inline void checkDGCComputePipelineSupportCmd(Context &context, ConstantCommandsMemoryReqsCase)
{
    checkDGCComputeSupport(context, true, false);
}

inline void initBasicProgramCmd(vk::SourceCollections &dst, ConstantCommandsMemoryReqsCase)
{
    initBasicProgram(dst);
}

tcu::TestStatus constantCommandsMemReqs(Context &context, ConstantCommandsMemoryReqsCase memReqsCase)
{
    const auto ctx         = context.getContextCommonData();
    const auto shaderStage = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_COMPUTE;

    // We need an actual pipeline for those cases where the sequence does not include a pipeline switch token.
    // Since this pipeline would presumably be set outside the generated commands, it can be a normal compute pipeline.
    const bool pipelineNeeded = (memReqsCase != ConstantCommandsMemoryReqsCase::BASIC_CASE_WITH_PIPELINE);

    // We'll pretend to use push constants in one particular case, even if they're not used by the compute shader.
    const bool needsPushConstant = (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT);
    const auto pcSize            = static_cast<uint32_t>(sizeof(uint32_t));
    const auto pcRange           = makePushConstantRange(shaderStage, 0u, pcSize);

    // We will pretend to use preprocess in some cases.
    const bool needsPreprocess = (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE);

    // For the flag ignore test, we'll need two actual indirect command layouts.
    const auto builderCount = (memReqsCase == ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED ? 2u : 1u);

    Move<VkDescriptorSetLayout> setLayout;
    Move<VkPipelineLayout> pipelineLayout;
    Move<VkShaderModule> compModule;
    Move<VkPipeline> pipeline;

    if (pipelineNeeded)
    {
        const auto &binaries = context.getBinaryCollection();

        compModule     = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
        setLayout      = makeBasicDescriptorSetLayout(ctx.vkd, ctx.device);
        pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, (needsPushConstant ? &pcRange : nullptr));
        pipeline       = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);
    }

    // Now build a command sequence. It will be different depending on the case. The flags will also vary a bit.
    const auto layoutUsageFlags = (needsPreprocess ? static_cast<VkIndirectCommandsLayoutUsageFlagsNV>(
                                                         VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_NV) :
                                                     0u);

    using BuilderPtr = std::unique_ptr<IndirectCommandsLayoutBuilder>;

    std::vector<BuilderPtr> builders;
    for (uint32_t i = 0u; i < builderCount; ++i)
    {
        const auto extraFlags = ((i > 0u) ? static_cast<VkIndirectCommandsLayoutUsageFlagsNV>(
                                                VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_NV) :
                                            0u);
        builders.emplace_back(new IndirectCommandsLayoutBuilder((layoutUsageFlags | extraFlags), bindPoint));
        auto &currentBuilder = *builders.back();

        if (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE)
        {
            currentBuilder.addDispatchToken(0u, 0u);
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::BASIC_CASE_WITH_PIPELINE)
        {
            currentBuilder.addPipelineToken(0u, 0u);
            currentBuilder.addDispatchToken(0u, currentBuilder.getStreamRange(0u));
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT)
        {
            currentBuilder.addPushConstantToken(0u, 0u, *pipelineLayout, pcRange.stageFlags, pcRange.offset,
                                                pcRange.size);
            currentBuilder.addDispatchToken(0u, currentBuilder.getStreamRange(0u));
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::MAX_SEQUENCE_COUNT)
        {
            currentBuilder.addDispatchToken(0u, 0u);
        }
        else if (memReqsCase == ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED)
        {
            currentBuilder.addDispatchToken(0u, 0u);
        }
        else
            DE_ASSERT(false);
    }

    std::vector<Move<VkIndirectCommandsLayoutNV>> cmdLayouts;
    for (const auto &builderPtr : builders)
        cmdLayouts.push_back(builderPtr->build(ctx.vkd, ctx.device));

    // For the maxSequencesCount value, we'll use something reasonable.
    auto maxSequencesCount = 0u;
    if (memReqsCase == ConstantCommandsMemoryReqsCase::MAX_SEQUENCE_COUNT)
    {
        const auto &properties = context.getDeviceGeneratedCommandsProperties();
        maxSequencesCount      = properties.maxIndirectSequenceCount;
    }
    else
        maxSequencesCount = 1024u;

    // Get an initial set of requirements.
    auto memReqsInfo =
        makeGeneratedCommandsMemoryRequirementsInfoNV(bindPoint, *pipeline, *cmdLayouts.at(0u), maxSequencesCount);
    const auto memReqs = getGeneratedCommandsMemoryRequirementsNV(ctx.vkd, ctx.device, &memReqsInfo);

    // Now request it a second time, varying some parameters if needed.
    if (memReqsCase == ConstantCommandsMemoryReqsCase::INCREASE_COUNT)
        memReqsInfo.maxSequencesCount *= 2u;

    if (memReqsCase == ConstantCommandsMemoryReqsCase::IGNORE_UNORDERED)
        memReqsInfo.indirectCommandsLayout = *cmdLayouts.at(1u);

    // Get a second set of memory requirements.
    const auto otherMemReqs = getGeneratedCommandsMemoryRequirementsNV(ctx.vkd, ctx.device, &memReqsInfo);

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

tcu::TestCaseGroup *createDGCComputeGetInfoTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "get_info"));

    addFunctionCaseWithPrograms(mainGroup.get(), "constant_pipeline_memory_requirements",
                                checkDGCComputePipelineSupport, initBasicProgram, constantPipelineMemoryRequirements);
    addFunctionCaseWithPrograms(mainGroup.get(), "constant_pipeline_device_address", checkDGCComputePipelineSupport,
                                initBasicProgram, constantPipelineDeviceAddress);
    addFunctionCaseWithPrograms(mainGroup.get(), "constant_pipeline_capture_replay_address",
                                checkDGCComputeCaptureReplaySupport, initBasicProgram,
                                constantPipelineCaptureReplayAddress);

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
        addFunctionCaseWithPrograms(mainGroup.get(), testName, supportCheck, initBasicProgramCmd,
                                    constantCommandsMemReqs, testCase.cmdMemCase);
    }

    return mainGroup.release();
}
} // namespace DGC
} // namespace vkt
