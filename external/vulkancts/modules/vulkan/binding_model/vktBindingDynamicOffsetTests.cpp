/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
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
 * \brief Dynamic offset tests.
 *//*--------------------------------------------------------------------*/

#include "vktBindingDynamicOffsetTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuStringTemplate.hpp"

#include <sstream>

using namespace vk;

namespace vkt
{
namespace BindingModel
{
namespace
{

struct DynamicOffsetPCParams
{
    bool separateOffsets; // Both pipelines use different offsets.
    bool pcFirst;         // Set push constants right at the start, before binding sets.
    bool singleLayout;    // Both pipelines use push constants and share pipeline layouts.
    bool differentSets;   // Assumes singleLayout is false. Pipelines use different sets.

    // Large push constants are interesting for some implementations.
    uint32_t getPCItemCount() const
    {
        return 8u;
    }
};

class DynamicOffsetPCInstance : public vkt::TestInstance
{
public:
    DynamicOffsetPCInstance(Context &context, const DynamicOffsetPCParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~DynamicOffsetPCInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const DynamicOffsetPCParams m_params;
};

class DynamicOffsetPCCase : public vkt::TestCase
{
public:
    DynamicOffsetPCCase(tcu::TestContext &testCtx, const std::string &name, const DynamicOffsetPCParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~DynamicOffsetPCCase(void) = default;

    void checkSupport(Context &context) const override
    {
        DE_UNREF(context);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        std::ostringstream comp;
        comp << "#version 460\n"
             << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout (set=${IB_SET}, binding=${IB_BINDING}) uniform InputBlock { vec4 color; } ib;\n"
             << "layout (set=${OB_SET}, binding=${OB_BINDING}) buffer OutputBlock { vec4 color; } ob;\n"
             << "${EXTRA_DECLARATIONS}"
             << "void main(void) {\n"
             << "    vec4 color = ib.color;\n"
             << "${EXTRA_STATEMENTS}"
             << "    ob.color = color;\n"
             << "}\n";
        tcu::StringTemplate shaderTemplate = comp.str();

        const auto pcItems = m_params.getPCItemCount();
        const std::string pcDecl =
            "layout (push_constant) uniform PCBlock { vec4 color[" + std::to_string(pcItems) + "]; } pc;\n";

        {
            std::map<std::string, std::string> comp0Map;
            comp0Map["IB_SET"]             = "0";
            comp0Map["IB_BINDING"]         = "0";
            comp0Map["OB_SET"]             = "0";
            comp0Map["OB_BINDING"]         = "1";
            comp0Map["EXTRA_DECLARATIONS"] = (m_params.singleLayout ? pcDecl : "");
            comp0Map["EXTRA_STATEMENTS"]   = "";
            const auto comp0               = shaderTemplate.specialize(comp0Map);
            programCollection.glslSources.add("comp0") << glu::ComputeSource(comp0);
        }

        {
            std::map<std::string, std::string> comp1Map;
            comp1Map["IB_SET"]     = "0";
            comp1Map["IB_BINDING"] = "0";
            if (m_params.differentSets)
            {
                DE_ASSERT(!m_params.singleLayout);
                comp1Map["OB_SET"]     = "1";
                comp1Map["OB_BINDING"] = "0";
            }
            else
            {
                comp1Map["OB_SET"]     = "0";
                comp1Map["OB_BINDING"] = "1";
            }
            comp1Map["EXTRA_DECLARATIONS"] = pcDecl;
            {
                // Note the first item should be the one we want, and the rest should be zeros.
                std::ostringstream statements;
                statements << "    color = pc.color[0];\n";
                for (uint32_t i = 1; i < pcItems; ++i)
                    statements << "    color = color + pc.color[" << i << "];\n";
                comp1Map["EXTRA_STATEMENTS"] = statements.str();
            }
            const auto comp1 = shaderTemplate.specialize(comp1Map);
            programCollection.glslSources.add("comp1") << glu::ComputeSource(comp1);
        }
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new DynamicOffsetPCInstance(context, m_params);
    }

protected:
    const DynamicOffsetPCParams m_params;
};

tcu::TestStatus DynamicOffsetPCInstance::iterate(void)
{
    const auto ctx       = m_context.getContextCommonData();
    const auto itemSize  = DE_SIZEOF32(tcu::Vec4);
    const auto itemCount = 3u; // 1st item: not used, 2nd item: first run, 3rd item (maybe): second run.

    // Due to minUniformBufferOffsetAlignment and minStorageBufferOffsetAlignment, we may need to adjust the buffer size
    // so that each item is properly aligned.
    const auto &properties                      = m_context.getDeviceProperties();
    const auto &minUniformBufferOffsetAlignment = properties.limits.minUniformBufferOffsetAlignment;
    const auto &minStorageBufferOffsetAlignment = properties.limits.minStorageBufferOffsetAlignment;

    const auto itemSizeUniform = de::roundUp(itemSize, static_cast<uint32_t>(minUniformBufferOffsetAlignment));
    const auto itemSizeStorage = de::roundUp(itemSize, static_cast<uint32_t>(minStorageBufferOffsetAlignment));

    const auto dataSizeUniform = itemSizeUniform * itemCount;
    const auto dataSizeStorage = itemSizeStorage * itemCount;

    const auto pcItemCount = m_params.getPCItemCount();
    const auto pcSize      = itemSize * pcItemCount;

    const auto inputBufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const auto inputBufferInfo  = makeBufferCreateInfo(dataSizeUniform, inputBufferUsage);
    BufferWithMemory inputBuffer(ctx.vkd, ctx.device, ctx.allocator, inputBufferInfo, HostIntent::W);
    {
        auto &alloc = inputBuffer.getAllocation();
        const std::vector<tcu::Vec4> inputBufferData{
            tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
            tcu::Vec4(1.0f, 2.0f, 3.0f, 4.0f),
            tcu::Vec4(5.0f, 6.0f, 7.0f, 8.0f),
        };
        DE_ASSERT(de::sizeU32(inputBufferData) == itemCount);
        uint8_t *bufferBytes = reinterpret_cast<uint8_t *>(alloc.getHostPtr());

        memset(bufferBytes, 0, dataSizeUniform);
        for (uint32_t i = 0u; i < itemCount; ++i)
        {
            auto itemPtr = bufferBytes + itemSizeUniform * i;
            memcpy(itemPtr, &inputBufferData.at(i), itemSize);
        }

        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    const auto outputBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto outputBufferInfo  = makeBufferCreateInfo(dataSizeStorage, outputBufferUsage);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, HostIntent::RW);
    {
        auto &alloc = outputBuffer.getAllocation();
        memset(alloc.getHostPtr(), 0, dataSizeStorage);
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    const auto &binaries   = m_context.getBinaryCollection();
    const auto compShader0 = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp0"));
    const auto compShader1 = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp1"));

    const auto stageFlags = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    DescriptorSetLayoutBuilder setLayoutBuilder0;
    setLayoutBuilder0.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, stageFlags);
    setLayoutBuilder0.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, stageFlags);
    const auto setLayout0                  = setLayoutBuilder0.build(ctx.vkd, ctx.device);
    VkDescriptorSetLayout setLayoutHandle0 = *setLayout0;

    Move<VkDescriptorSetLayout> setLayout1;
    VkDescriptorSetLayout setLayoutHandle1 = VK_NULL_HANDLE;

    // To simplify, when using separate sets the second pipeline will reuse the layout of the first set, despite the
    // fact that it only uses binding 0, and the output buffer will be available in set 1 in addition to set 0.
    if (m_params.differentSets)
    {
        DescriptorSetLayoutBuilder setLayoutBuilder1;
        setLayoutBuilder1.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, stageFlags);
        setLayout1       = setLayoutBuilder1.build(ctx.vkd, ctx.device);
        setLayoutHandle1 = *setLayout1;
    }
    else
        setLayoutHandle1 = setLayoutHandle0;

    const auto pcRange = makePushConstantRange(stageFlags, 0u, pcSize);
    std::vector<tcu::Vec4> pcItemsVec(pcItemCount, tcu::Vec4(0.0f));
    pcItemsVec.front()  = tcu::Vec4(100.0f, 200.0f, 300.0f, 400.0f);
    const auto pcValues = de::dataOrNull(pcItemsVec);

    const auto pipelineLayout0 = makePipelineLayout(ctx.vkd, ctx.device, setLayoutHandle0);

    // Reuse set layout 0 despite the fact that we don't use all bindings.
    std::vector<VkDescriptorSetLayout> setLayoutHandles1;
    setLayoutHandles1.push_back(setLayoutHandle0);
    if (m_params.differentSets)
        setLayoutHandles1.push_back(setLayoutHandle1);
    const auto pipelineLayout1 = makePipelineLayout(ctx.vkd, ctx.device, de::sizeU32(setLayoutHandles1),
                                                    de::dataOrNull(setLayoutHandles1), 1u, &pcRange);

    const VkPipelineLayout pipelineLayoutHandle0 = (m_params.singleLayout ? *pipelineLayout1 : *pipelineLayout0);
    const VkPipelineLayout pipelineLayoutHandle1 = *pipelineLayout1;

    const auto pipeline0 = makeComputePipeline(ctx.vkd, ctx.device, pipelineLayoutHandle0, *compShader0);
    const auto pipeline1 = makeComputePipeline(ctx.vkd, ctx.device, pipelineLayoutHandle1, *compShader1);

    // Prepare descriptor sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2u);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2u);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);
    const auto descriptorSet0 = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, setLayoutHandle0);

    Move<VkDescriptorSet> descriptorSet1;
    if (m_params.differentSets)
        descriptorSet1 = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, setLayoutHandle1);

    DescriptorSetUpdateBuilder updateBuilder;
    const auto binding              = DescriptorSetUpdateBuilder::Location::binding;
    const auto inputBufferDescInfo  = makeDescriptorBufferInfo(*inputBuffer, 0ull, itemSize);
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, itemSize);
    updateBuilder.writeSingle(*descriptorSet0, binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                              &inputBufferDescInfo);
    updateBuilder.writeSingle(*descriptorSet0, binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                              &outputBufferDescInfo);
    if (m_params.differentSets)
    {
        // Make output buffer also available on binding 0 of the second set.
        updateBuilder.writeSingle(*descriptorSet1, binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                  &outputBufferDescInfo);
    }
    updateBuilder.update(ctx.vkd, ctx.device);

    std::vector<VkDescriptorSet> allDescriptorSets;
    allDescriptorSets.push_back(*descriptorSet0);
    if (m_params.differentSets)
        allDescriptorSets.push_back(*descriptorSet1);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    std::vector<uint32_t> dynamicOffsets{
        itemSizeUniform,
        itemSizeStorage,
    };

    auto dynamicOffsets0 = dynamicOffsets;
    auto dynamicOffsets1 = dynamicOffsets;

    if (m_params.separateOffsets)
    {
        for (auto &offset : dynamicOffsets1)
            offset *= 2u;
    }

    // With different sets, we actually have 3 descriptors and the last offset (output buffer) needs to appear twice.
    if (m_params.differentSets)
        dynamicOffsets1.push_back(dynamicOffsets1.back());

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (m_params.pcFirst)
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayoutHandle1, stageFlags, 0u, pcSize, pcValues);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayoutHandle0, 0u, 1u, &descriptorSet0.get(),
                                  de::sizeU32(dynamicOffsets0), de::dataOrNull(dynamicOffsets0));
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline0);
    if (m_params.singleLayout && !m_params.pcFirst)
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayoutHandle0, stageFlags, 0u, pcSize, pcValues);
    ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &barrier);
    }
    if (!m_params.singleLayout || m_params.separateOffsets)
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayoutHandle1, 0u, de::sizeU32(allDescriptorSets),
                                      de::dataOrNull(allDescriptorSets), de::sizeU32(dynamicOffsets1),
                                      de::dataOrNull(dynamicOffsets1));
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, *pipeline1);
    if (!m_params.pcFirst && !m_params.singleLayout)
        ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayoutHandle1, stageFlags, 0u, pcSize, pcValues);
    ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    auto &outputAlloc = outputBuffer.getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, outputAlloc);

    std::vector<tcu::Vec4> expected(itemCount, tcu::Vec4(0.0f));
    std::vector<tcu::Vec4> result(itemCount, tcu::Vec4(0.0f));

    memcpy(de::dataOrNull(result), outputAlloc.getHostPtr(), de::dataSize(result));
    {
        auto &inputBufferAlloc          = inputBuffer.getAllocation();
        const uint8_t *inputBufferBytes = reinterpret_cast<const uint8_t *>(inputBufferAlloc.getHostPtr());
        uint8_t *dstBytes               = reinterpret_cast<uint8_t *>(de::dataOrNull(expected));

        // Copy data from input buffer at the first input offset into the first output offset.
        memcpy(dstBytes + dynamicOffsets0.back(), inputBufferBytes + dynamicOffsets0.front(), itemSize);

        // Copy data from push constants into the second output offset.
        memcpy(dstBytes + dynamicOffsets1.back(), pcValues, itemSize);
    }

    DE_ASSERT(expected.size() == result.size());
    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    for (uint32_t i = 0u; i < de::sizeU32(expected); ++i)
    {
        const auto &expItem = expected.at(i);
        const auto &resItem = result.at(i);

        if (expItem != resItem)
        {
            fail = true;
            std::ostringstream msg;
            msg << "Mismatch at item " << i << ": expected " << expItem << " but got " << resItem;
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected results in output buffer; check log for details --");
    return tcu::TestStatus::pass("Pass");
}

void populateDynamicOffsetTests(tcu::TestCaseGroup *group)
{
    tcu::TestContext &testCtx = group->getTestContext();

    group->addChild(cts_amber::createAmberTestCase(testCtx, "shader_reuse_differing_layout_compute",
                                                   "binding_model/dynamic_offset",
                                                   "shader_reuse_differing_layout_compute.amber"));
    group->addChild(cts_amber::createAmberTestCase(testCtx, "shader_reuse_differing_layout_graphics",
                                                   "binding_model/dynamic_offset",
                                                   "shader_reuse_differing_layout_graphics.amber"));

    for (const bool separateOffsets : {false, true})
        for (const bool pcFirst : {false, true})
            for (const bool singleLayout : {false, true})
                for (const bool differentSets : {false, true})
                {
                    // Invalid combination.
                    if (differentSets && singleLayout)
                        continue;

                    const DynamicOffsetPCParams params{
                        separateOffsets,
                        pcFirst,
                        singleLayout,
                        differentSets,
                    };
                    const auto testName = std::string("two_pipelines") + (separateOffsets ? "_separate_offsets" : "") +
                                          (pcFirst ? "_pc_first" : "") + (singleLayout ? "_single_layout" : "") +
                                          (differentSets ? "_different_sets" : "");
                    group->addChild(new DynamicOffsetPCCase(testCtx, testName, params));
                }
}

} // namespace

tcu::TestCaseGroup *createDynamicOffsetTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "dynamic_offset", populateDynamicOffsetTests);
}

} // namespace BindingModel
} // namespace vkt
