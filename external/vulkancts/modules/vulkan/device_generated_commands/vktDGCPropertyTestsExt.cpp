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
 * \brief Device Generated Commands EXT Property Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCPropertyTestsExt.hpp"
#include "tcuImageCompare.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktTestCaseUtil.hpp"

#include "tcuTextureUtil.hpp"

#include <vector>
#include <sstream>
#include <limits>
#include <numeric>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

void checkDGCExtFunctionalitySupport(Context &context)
{
    checkDGCExtSupport(context, 0u);
}

void checkBasicDGCComputeSupport(Context &context)
{
    checkDGCExtComputeSupport(context, false);
}

void checkMaxIndirectCommandsTokenCountSupport(Context &context, uint32_t pcSizeBytes)
{
    checkBasicDGCComputeSupport(context);

    // Each item in the push constant array will be updated by a separate push constant token. Given the push constant
    // array length, we cannot run the test if we go over the max push constant size or the maximum number of tokens.
    const auto &stdProperties = context.getDeviceProperties();
    if (stdProperties.limits.maxPushConstantsSize < pcSizeBytes)
    {
        std::ostringstream msg;
        msg << "maxPushConstantsSize not large enough (" << stdProperties.limits.maxPushConstantsSize << " vs "
            << pcSizeBytes << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }

    const auto pcLen       = pcSizeBytes / DE_SIZEOF32(uint32_t);
    const auto totalTokens = pcLen + 1u; // For the dispatch token.

    const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
    if (dgcProperties.maxIndirectCommandsTokenCount < totalTokens)
    {
        std::ostringstream msg;
        msg << "maxIndirectCommandsTokenCount not large enough (" << dgcProperties.maxIndirectCommandsTokenCount
            << " vs " << totalTokens << ")";
        TCU_THROW(NotSupportedError, msg.str());
    }
}

void checkBasicDGCGraphicsSupport(Context &context)
{
    const VkShaderStageFlags stages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    checkDGCExtSupport(context, stages);
}

void basicGraphicsPrograms(SourceCollections &dst)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main (void) {\n"
         << "    gl_Position = inPos;\n"
         << "    gl_PointSize = 1.0;\n"
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

// Make sure the dispatch Z size is being read correctly. For that, we'll later dispatch 4 workgroups (1, 1, 4), and we'll use the
// invocation in each of them to store the push constant in each of the bytes of the output buffer value.
void storePushConstantBytesProgram(SourceCollections &dst)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint value; } outputBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint value; } pc;\n"
         << "void main (void) {\n"
         << "    atomicOr(outputBuffer.value, (pc.value << (8 * gl_WorkGroupID.z)));\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

// Store the push constant value in the output buffer position indicated by another push constant.
void storePushConstantWithIndexProgram(SourceCollections &dst)
{
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[]; } outputBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint index; uint value; } pc;\n"
         << "void main (void) { outputBuffer.values[pc.index] = pc.value; }\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

// Store array of push constant values in an output buffer array.
void storePushConstantArray(SourceCollections &dst, uint32_t pcSizeBytes)
{
    const uint32_t pcLen = pcSizeBytes / DE_SIZEOF32(uint32_t);

    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer OutputBlock { uint values[" << pcLen << "]; } outputBuffer;\n"
         << "layout (push_constant, std430) uniform PushConstantBlock { uint values[" << pcLen << "]; } pc;\n"
         << "void main (void) {\n"
         << "    for (uint i = 0u; i < " << pcLen << "; ++i) {\n"
         << "        outputBuffer.values[i] = pc.values[i];\n"
         << "    }\n"
         << "}\n";
    dst.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus validLimits(Context &context)
{
    const auto &properties = context.getDeviceGeneratedCommandsPropertiesEXT();

    // Common with NV.
    if (properties.maxIndirectSequenceCount < (1u << 20))
        TCU_FAIL("maxIndirectSequenceCount not in required range");

    if (properties.maxIndirectCommandsTokenCount < (16u))
        TCU_FAIL("maxIndirectCommandsTokenCount not in required range");

    if (properties.maxIndirectCommandsTokenOffset < (2047u))
        TCU_FAIL("maxIndirectCommandsTokenOffset not in required range");

    if (properties.maxIndirectCommandsIndirectStride < (2048u))
        TCU_FAIL("maxIndirectCommandsIndirectStride not in required range");

    // Particular to EXT.
    if (properties.maxIndirectPipelineCount < (1u << 12))
        TCU_FAIL("maxIndirectPipelineCount not in required range");

    if (properties.maxIndirectShaderObjectCount > 0u && properties.maxIndirectShaderObjectCount < (1u << 12))
        TCU_FAIL("maxIndirectShaderObjectCount not in required range");

    if ((properties.supportedIndirectCommandsInputModes & VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT) ==
        0u)
        TCU_FAIL("supportedIndirectCommandsInputModes missing required bits");

    {
        const auto requiredStages =
            (VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        if ((properties.supportedIndirectCommandsShaderStages & requiredStages) != requiredStages)
            TCU_FAIL("supportedIndirectCommandsShaderStages missing required bits");
    }

    // supportedIndirectCommandsShaderStagesBinding has no required bits.

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus maxIndirectCommandsTokenCountRun(Context &context, uint32_t pcSizeBytes)
{
    const auto &ctx          = context.getContextCommonData();
    const auto descType      = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags    = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint     = VK_PIPELINE_BIND_POINT_COMPUTE;
    auto &log                = context.getTestContext().getLog();
    const auto itemSize      = DE_SIZEOF32(uint32_t);
    const auto pcLen         = pcSizeBytes / itemSize;
    const auto pcValueOffset = 1000u;

    // Output buffer.
    const auto outputBufferSize       = static_cast<VkDeviceSize>(pcSizeBytes);
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
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSizeBytes);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Push constants followed by dispatch.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    for (uint32_t i = 0u; i < pcLen; ++i)
    {
        const auto offset      = i * itemSize;
        const auto updateRange = makePushConstantRange(stageFlags, offset, itemSize);
        cmdsLayoutBuilder.addPushConstantToken(offset, updateRange);
    }
    cmdsLayoutBuilder.addDispatchToken(pcLen * itemSize);
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint32_t> genCmdsData;
    genCmdsData.reserve(cmdsLayoutBuilder.getStreamStride());
    for (uint32_t i = 0u; i < pcLen; ++i)
        genCmdsData.push_back(i + pcValueOffset); // Push constant.
    genCmdsData.push_back(1u);                    // VkDispatchIndirectCommand::x
    genCmdsData.push_back(1u);                    // VkDispatchIndirectCommand::y
    genCmdsData.push_back(1u);                    // VkDispatchIndirectCommand::z

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Preprocess buffer for 1 sequence.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, 1u, 0u,
                                         *pipeline);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    {
        const DGCGenCmdsInfo cmdsInfo(stageFlags,     // VkShaderStageFlags shaderStages;
                                      VK_NULL_HANDLE, // VkIndirectExecutionSetEXT indirectExecutionSet;
                                      *cmdsLayout,    // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
                                      genCmdsBuffer.getDeviceAddress(),    // VkDeviceAddress indirectAddress;
                                      genCmdsBufferSize,                   // VkDeviceSize indirectAddressSize;
                                      preprocessBuffer.getDeviceAddress(), // VkDeviceAddress preprocessAddress;
                                      preprocessBuffer.getSize(),          // VkDeviceSize preprocessSize;
                                      1u,                                  // uint32_t maxSequenceCount;
                                      0ull,                                // VkDeviceAddress sequenceCountAddress;
                                      0u,                                  // uint32_t maxDrawCount;
                                      *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    std::vector<uint32_t> expected(pcLen, 0u);
    std::vector<uint32_t> result(pcLen, 0u);

    std::iota(begin(expected), end(expected), pcValueOffset);

    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(result.data(), outputBufferData, pcSizeBytes);

    bool pass = true;
    for (uint32_t i = 0u; i < pcLen; ++i)
    {
        const auto &exp = expected.at(i);
        const auto &res = result.at(i);

        if (res != exp)
        {
            std::ostringstream msg;
            msg << "Unexpected value found in output buffer at position " << i << ": expected " << exp << " but found "
                << res;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            pass = false;
        }
    }

    if (!pass)
        return tcu::TestStatus::fail("Unexpected values found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus maxIndirectCommandsTokenOffsetRun(Context &context)
{
    const auto &ctx       = context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    auto &log             = context.getTestContext().getLog();

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
    const auto u32Size     = static_cast<uint32_t>(sizeof(uint32_t));
    const auto u32SizeVkSz = static_cast<VkDeviceSize>(u32Size);
    const auto pcValue     = 0x77u; // Arbitrary, fits in a byte.
    const auto pcSize      = u32Size;
    const auto pcRange     = makePushConstantRange(stageFlags, 0u, pcSize);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // Generated commands layout: test the token offset limit. We'll use two tokens: the push constant one and the dispatch. Note
    // we're also affected by the stream stride limit.
    //
    // Lets start with the max stride first and then reduce that for the offset if needed. Also, set a maximum reasonable limit so
    // we don't allocate a huge amount of memory.
    const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
    const auto &maxStride     = dgcProperties.maxIndirectCommandsIndirectStride;
    const auto maxTokenOffset = static_cast<VkDeviceSize>(dgcProperties.maxIndirectCommandsTokenOffset);
    constexpr VkDeviceSize kHardMax =
        1024u * 1024u; // 1MB is a lot for a single sequence. Note we'll likely use this limit.

    constexpr uint32_t minRequiredOffset = pcSize; // No less than this for the dispatch token.
    constexpr auto dispatchTokenSize     = static_cast<VkDeviceSize>(sizeof(VkDispatchIndirectCommand));
    constexpr uint32_t minRequiredStride = pcSize + dispatchTokenSize;

    if (maxStride < minRequiredStride)
        TCU_FAIL("maxIndirectCommandsIndirectStride too low");

    if (maxTokenOffset < minRequiredOffset)
        TCU_FAIL("maxIndirectCommandsTokenOffset too low");

    // The offset of the dispatch token is the lowest of the max stride - sizeof(uint32_t) and the max token offset property.
    // Note we round the max token offset down to make sure the dispatch is aligned.
    const auto dispatchTokenOffset =
        std::min(kHardMax, de::roundDown(std::min(maxStride - dispatchTokenSize, maxTokenOffset), u32SizeVkSz));
    const auto streamStride = dispatchTokenOffset + dispatchTokenSize;

    log << tcu::TestLog::Message << "maxIndirectCommandsTokenOffset:    " << maxTokenOffset << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "maxIndirectCommandsIndirectStride: " << maxStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Hard maximum for the test:         " << kHardMax << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Chosen token offset:               " << dispatchTokenOffset
        << tcu::TestLog::EndMessage;

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addPushConstantToken(0u, pcRange);
    cmdsLayoutBuilder.addDispatchToken(static_cast<uint32_t>(dispatchTokenOffset));
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint8_t> genCmdsData(static_cast<size_t>(streamStride), 0);
    const VkDispatchIndirectCommand dispatchCmd{1u, 1u, 4u};
    deMemcpy(de::dataOrNull(genCmdsData), &pcValue, sizeof(pcValue));
    deMemcpy(de::dataOrNull(genCmdsData) + dispatchTokenOffset, &dispatchCmd, sizeof(dispatchCmd));

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Preprocess buffer for 1 sequence.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, 1u, 0u,
                                         *pipeline);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    {
        const DGCGenCmdsInfo cmdsInfo(stageFlags,     // VkShaderStageFlags shaderStages;
                                      VK_NULL_HANDLE, // VkIndirectExecutionSetEXT indirectExecutionSet;
                                      *cmdsLayout,    // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
                                      genCmdsBuffer.getDeviceAddress(),    // VkDeviceAddress indirectAddress;
                                      genCmdsBufferSize,                   // VkDeviceSize indirectAddressSize;
                                      preprocessBuffer.getDeviceAddress(), // VkDeviceAddress preprocessAddress;
                                      preprocessBuffer.getSize(),          // VkDeviceSize preprocessSize;
                                      1u,                                  // uint32_t maxSequenceCount;
                                      0ull,                                // VkDeviceAddress sequenceCountAddress;
                                      0u,                                  // uint32_t maxDrawCount;
                                      *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    uint32_t expectedValue = 0u;
    for (size_t i = 0u; i < sizeof(expectedValue); ++i)
        expectedValue |= (pcValue << (8 * i)); // Must match shader.

    uint32_t outputValue = 0u;
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(&outputValue, outputBufferData, sizeof(outputValue));

    if (outputValue != expectedValue)
    {
        std::ostringstream msg;
        msg << "Unexpected value found in output buffer; expected " << expectedValue << " but found " << outputValue;
        return tcu::TestStatus::fail(msg.str());
    }
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus maxIndirectCommandsIndirectStrideRun(Context &context)
{
    const auto &ctx       = context.getContextCommonData();
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_COMPUTE;
    auto &log             = context.getTestContext().getLog();
    const auto u32Size    = static_cast<uint32_t>(sizeof(uint32_t));

    // Push constants. This must match the shader.
    struct PushConstants
    {
        uint32_t index;
        uint32_t value;
    };

    const auto pcSize  = static_cast<uint32_t>(sizeof(PushConstants));
    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);

    const std::vector<PushConstants> pcValues{
        {0u, 555u},
        {1u, 777u},
    };

    std::vector<uint32_t> outputBufferValues{0u, 0u};
    DE_ASSERT(outputBufferValues.size() == pcValues.size());

    // Output buffer.
    const auto outputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(outputBufferValues));
    const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferCreateInfo,
                                  MemoryRequirement::HostVisible);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    void *outputBufferData  = outputBufferAlloc.getHostPtr();

    deMemcpy(outputBufferData, de::dataOrNull(outputBufferValues), de::dataSize(outputBufferValues));
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

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout, &pcRange);

    // Shader.
    const auto &binaries  = context.getBinaryCollection();
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipeline.
    const auto pipeline = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compModule);

    // To test the maximum stride, we'll generate a couple of dispatches and set them apart by the chosen stream stride.
    // Both dispatches will have to be emitted and will use the push constant values to update the buffer.
    const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
    const auto maxStride      = static_cast<uint32_t>(dgcProperties.maxIndirectCommandsIndirectStride);
    constexpr uint32_t kHardMax =
        1024u * 1024u; // 1MB is a lot for a single sequence. Note we'll likely use this limit.
    constexpr uint32_t minRequiredStride = pcSize + static_cast<uint32_t>(sizeof(VkDispatchIndirectCommand));

    if (maxStride < minRequiredStride)
        TCU_FAIL("maxIndirectCommandsIndirectStride too low");

    // We need to round down the chosen stride to make sure push constants and dispatch commands are aligned.
    const auto chosenStride = de::roundDown(std::min(kHardMax, maxStride), u32Size);
    const auto dataSize     = chosenStride * de::sizeU32(pcValues);

    log << tcu::TestLog::Message << "maxIndirectCommandsIndirectStride: " << maxStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Hard maximum for the test:         " << kHardMax << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Chosen stride:                     " << chosenStride << tcu::TestLog::EndMessage;

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addPushConstantToken(0u, pcRange);
    cmdsLayoutBuilder.addDispatchToken(pcSize);
    cmdsLayoutBuilder.setStreamStride(chosenStride);
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Generated indirect commands buffer contents.
    std::vector<uint8_t> genCmdsData(dataSize, 0);
    const VkDispatchIndirectCommand dispatchCmd{1u, 1u, 1u};

    for (size_t i = 0u; i < pcValues.size(); ++i)
    {
        const auto offset = i * chosenStride;
        deMemcpy(de::dataOrNull(genCmdsData) + offset, &pcValues.at(i), pcSize);
        deMemcpy(de::dataOrNull(genCmdsData) + offset + pcSize, &dispatchCmd, sizeof(dispatchCmd));
    }

    // Generated indirect commands buffer.
    const auto genCmdsBufferSize = de::dataSize(genCmdsData);
    DGCBuffer genCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, genCmdsBufferSize);
    auto &genCmdsBufferAlloc = genCmdsBuffer.getAllocation();
    void *genCmdsBufferData  = genCmdsBufferAlloc.getHostPtr();

    deMemcpy(genCmdsBufferData, de::dataOrNull(genCmdsData), de::dataSize(genCmdsData));
    flushAlloc(ctx.vkd, ctx.device, genCmdsBufferAlloc);

    // Preprocess buffer.
    const auto maxSequences = de::sizeU32(pcValues);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, maxSequences,
                                         0u, *pipeline);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    {
        const DGCGenCmdsInfo cmdsInfo(stageFlags,     // VkShaderStageFlags shaderStages;
                                      VK_NULL_HANDLE, // VkIndirectExecutionSetEXT indirectExecutionSet;
                                      *cmdsLayout,    // VkIndirectCommandsLayoutEXT indirectCommandsLayout;
                                      genCmdsBuffer.getDeviceAddress(),    // VkDeviceAddress indirectAddress;
                                      genCmdsBufferSize,                   // VkDeviceSize indirectAddressSize;
                                      preprocessBuffer.getDeviceAddress(), // VkDeviceAddress preprocessAddress;
                                      preprocessBuffer.getSize(),          // VkDeviceSize preprocessSize;
                                      maxSequences,                        // uint32_t maxSequenceCount;
                                      0ull,                                // VkDeviceAddress sequenceCountAddress;
                                      0u,                                  // uint32_t maxDrawCount;
                                      *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify results.
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(de::dataOrNull(outputBufferValues), outputBufferData, de::dataSize(outputBufferValues));

    bool fail = false;
    for (size_t i = 0u; i < outputBufferValues.size(); ++i)
    {
        const auto &result = outputBufferValues.at(i);

        // Find the reference value in the push constants.
        bool hasRef        = false;
        uint32_t reference = std::numeric_limits<uint32_t>::max();

        for (size_t j = 0u; j < pcValues.size(); ++j)
        {
            if (pcValues.at(j).index == i)
            {
                hasRef    = true;
                reference = pcValues.at(j).value;
                break;
            }
        }

        if (!hasRef)
            DE_ASSERT(false);

        if (reference != result)
        {
            fail = true;
            log << tcu::TestLog::Message << "Unexpected value found at index " << i << ": expected " << reference
                << " but found " << result << tcu::TestLog::EndMessage;
        }
    }

    if (fail)
        return tcu::TestStatus::fail("Unexpected value found in output buffer; check log for details");
    return tcu::TestStatus::pass("Pass");
}

// maxIndirectSequenceCount has a minimum value of 2**20, so we'll use a 1024x1024 framebuffer, one point per pixel.
tcu::TestStatus maxIndirectSequenceCountRun(Context &context)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(1024, 1024, 1);
    const auto floatExtent = fbExtent.asFloat();
    const auto pixelCount  = static_cast<uint32_t>(fbExtent.x() * fbExtent.y() * fbExtent.z());
    const auto vkExtent    = makeExtent3D(fbExtent);
    const auto fbFormat    = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(fbFormat);
    const auto fbUsage     = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match fragment shader.
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    // Color buffer with verification buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

    // Vertices.
    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float xCenter = (static_cast<float>(x) + 0.5f) / floatExtent.x() * 2.0f - 1.0f;
            const float yCenter = (static_cast<float>(y) + 0.5f) / floatExtent.y() * 2.0f - 1.0f;
            vertices.emplace_back(xCenter, yCenter, 0.0f, 1.0f);
        }

    // Vertex buffer
    const auto vbSize = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vbInfo = makeBufferCreateInfo(vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vbInfo, MemoryRequirement::HostVisible);
    const auto &vbAlloc = vertexBuffer.getAllocation();
    void *vbData        = vbAlloc.getHostPtr();
    const auto vbOffset = static_cast<VkDeviceSize>(0);

    deMemcpy(vbData, de::dataOrNull(vertices), de::dataSize(vertices));

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);
    const auto renderPass     = makeRenderPass(ctx.vkd, ctx.device, fbFormat);
    const auto framebuffer =
        makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    // The default values works for the current setup, including the vertex input data format.
    const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModule, VK_NULL_HANDLE,
                                               VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule, *renderPass, viewports,
                                               scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);

    // Indirect commands layout.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, stageFlags, *pipelineLayout);
    cmdsLayoutBuilder.addDrawToken(0u);
    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // DGC Buffer.
    std::vector<VkDrawIndirectCommand> drawCmds;
    drawCmds.reserve(pixelCount);
    for (size_t i = 0u; i < vertices.size(); ++i)
        drawCmds.push_back(VkDrawIndirectCommand{1u, 1u, static_cast<uint32_t>(i), 0u});

    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(drawCmds));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    auto &dgcBufferAlloc = dgcBuffer.getAllocation();
    void *dgcBufferData  = dgcBufferAlloc.getHostPtr();
    deMemcpy(dgcBufferData, de::dataOrNull(drawCmds), de::dataSize(drawCmds));

    // Preprocess buffer.
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout,
                                         de::sizeU32(drawCmds), 0u, *pipeline);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), clearColor);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vbOffset);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline);
    {
        DGCGenCmdsInfo cmdsInfo(stageFlags, VK_NULL_HANDLE, *cmdsLayout, dgcBuffer.getDeviceAddress(),
                                dgcBuffer.getSize(), preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                de::sizeU32(drawCmds), 0ull, 0u, *pipeline);
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &cmdsInfo.get());
    }
    endRenderPass(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, colorBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, geomColor);

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCPropertyTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "properties"));
    addFunctionCase(mainGroup.get(), "valid_limits", checkDGCExtFunctionalitySupport, validLimits);

    // For maxIndirectCommandsTokenCount we depend on the push constant limits.
    for (const auto tokenCount : {16u, 32u})
    {
        const auto pcTokens    = tokenCount - 1u;
        const auto pcSizeBytes = pcTokens * DE_SIZEOF32(uint32_t);
        const auto testName    = "maxIndirectCommandsTokenCount_" + std::to_string(tokenCount);

        addFunctionCaseWithPrograms(mainGroup.get(), testName, checkMaxIndirectCommandsTokenCountSupport,
                                    storePushConstantArray, maxIndirectCommandsTokenCountRun, pcSizeBytes);
    }

    addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectCommandsTokenOffset", checkBasicDGCComputeSupport,
                                storePushConstantBytesProgram, maxIndirectCommandsTokenOffsetRun);
    addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectCommandsStreamIndirect", checkBasicDGCComputeSupport,
                                storePushConstantWithIndexProgram, maxIndirectCommandsIndirectStrideRun);
    addFunctionCaseWithPrograms(mainGroup.get(), "maxIndirectSequenceCount", checkBasicDGCGraphicsSupport,
                                basicGraphicsPrograms, maxIndirectSequenceCountRun);

    // Not tested:
    // maxIndirectPipelineCount: likely too much, minimum of 4096 pipelines.
    // maxIndirectShaderObjectCount: similar to the pipeline count.

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
