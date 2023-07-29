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
 * \brief Device Generated Commands Compute Layout Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeLayoutTests.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtil.hpp"

#include "deRandom.hpp"

#include <numeric>
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

// Note the smoke tests already cover the case of the layout containing dispatches only, which is a bit challenging due to the lack
// of specialization for each dispatch. In these tests we'll check other cases in ways that allow us to specialize each dispatch and
// write results to different areas.
//
// "complementary" tests use a push constant, either the first one or the last one, that's fixed for all dispatches and pushed
// outside the indirect commands, complementing the set of push constants from the indirect commands.
enum class TestType
{
    PUSH_DISPATCH = 0,
    COMPLEMENTARY_PUSH_DISPATCH,
    PARTIAL_PUSH_DISPATCH,
    PIPELINE_DISPATCH,
    PIPELINE_PUSH_DISPATCH,
    PIPELINE_COMPLEMENTARY_PUSH_DISPATCH,
};

bool hasPipelineSwitch(TestType testType)
{
    const bool pipelineSwitch =
        (testType == TestType::PIPELINE_DISPATCH || testType == TestType::PIPELINE_PUSH_DISPATCH ||
         testType == TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH);
    return pipelineSwitch;
}

// Has a fourth push constant that's pushed outside the indirect commands stream.
bool hasComplementaryPush(TestType testType)
{
    return (testType == TestType::COMPLEMENTARY_PUSH_DISPATCH ||
            testType == TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH);
}

struct TestParams
{
    TestParams(TestType testType_, bool align4_, bool computeQueue_, bool captureReplay_)
        : testType(testType_)
        , align4(align4_)
        , computeQueue(computeQueue_)
        , captureReplay(captureReplay_)
    {
    }

    TestType testType;
    bool align4;        // Attempt to align pipeline addresses to 4 bytes instead of 8.
    bool computeQueue;  // Use the compute queue.
    bool captureReplay; // Use capture/replay for pipeline addresses.
};

// See the shader code below. This is the specialization data that will be used in each dispatch. It may be used as specialization
// constants, as push constants or both.
struct SpecializationData
{
    uint32_t dispatchOffset; // Offset in the array for this dispatch.
    uint32_t skipIndex;      // This invocation will not perform the write.
    uint32_t valueOffset;    // The local invocation index and workgroup index will be combined with this.
};

constexpr uint32_t kLocalInvocations = 64u;
constexpr uint32_t kSequenceCount    = 4u;

class LayoutTestCase : public vkt::TestCase
{
public:
    LayoutTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~LayoutTestCase(void)
    {
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

protected:
    const TestParams m_params;
};

class LayoutTestInstance : public vkt::TestInstance
{
public:
    LayoutTestInstance(Context &context, const TestParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
        , m_shaderStage(static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT))
        , m_bindPoint(VK_PIPELINE_BIND_POINT_COMPUTE)
        , m_constantSize(static_cast<uint32_t>(sizeof(uint32_t))) // All constants are uints in the shader.
        , m_pcTokenDataSize(0u)
        , m_layoutPcSize(0u)
        , m_pipelineLayout()
        , m_singlePipeline()
        , m_dgcPipelines()
        , m_commandsLayout()
    {
    }
    virtual ~LayoutTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    using DGCComputePipelinePtr             = std::unique_ptr<DGCComputePipeline>;
    using DGCComputePipelineMetaDataPoolPtr = std::unique_ptr<DGCComputePipelineMetaDataPool>;

    // Sets m_pipelineLayout, and m_singlePipeline or m_dgcPipelines depending on the test type.
    void createPipelines(const DeviceInterface &vkd, VkDevice device, vk::Allocator &allocator,
                         VkDescriptorSetLayout setLayout, const std::vector<SpecializationData> &specializationData);

    // Sets m_commandsLayout.
    void makeCommandsLayout(const DeviceInterface &vkd, VkDevice device);

    // Creates a "buffer" with the indirect commands.
    std::vector<uint32_t> makeIndirectCommands(const std::vector<uint32_t> &wgCounts,
                                               const std::vector<SpecializationData> &specializationData) const;

    const TestParams m_params;
    const VkShaderStageFlags m_shaderStage;
    const VkPipelineBindPoint m_bindPoint;
    const uint32_t m_constantSize;

    uint32_t m_pcTokenDataSize;
    uint32_t m_layoutPcSize;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_singlePipeline; // Used when the test case contains no pipeline switches.
    DGCComputePipelineMetaDataPoolPtr m_metaDataPool;
    std::vector<DGCComputePipelinePtr> m_dgcPipelines; // Used when the commands layout contains pipeline switch tokens.
    Move<VkIndirectCommandsLayoutNV> m_commandsLayout;
};

void LayoutTestCase::checkSupport(Context &context) const
{
    const auto requirePipelineSupport = hasPipelineSwitch(m_params.testType);
    checkDGCComputeSupport(context, requirePipelineSupport, m_params.captureReplay);

    if (m_params.captureReplay)
        DE_ASSERT(requirePipelineSupport); // Otherwise the test would not make sense.

    if (m_params.align4)
    {
        const auto &properties = context.getDeviceGeneratedCommandsProperties();
        if (properties.minIndirectCommandsBufferOffsetAlignment > 4u)
            TCU_THROW(NotSupportedError, "minIndirectCommandsBufferOffsetAlignment greater than 4");
    }

    if (m_params.computeQueue)
        context.getComputeQueue(); // Will throw NotSupportedError if not available.
}

void LayoutTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream constants;
    std::string dispatchOffsetPrefix;
    std::string skipIndexPrefix;
    std::string valueOffsetPrefix;
    std::string valueOffset2Prefix;

    const bool complementaryPush             = (m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH);
    const bool complementaryPushWithPipeline = (m_params.testType == TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH);

    // Note the constants will match the SpecializationData structure.
    if (m_params.testType == TestType::PUSH_DISPATCH || m_params.testType == TestType::PARTIAL_PUSH_DISPATCH ||
        complementaryPush)
    {
        // Push constants only.
        constants << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                  << "    uint dispatchOffset;\n"
                  << "    uint skipIndex;\n"
                  << "    uint valueOffset;\n"
                  << (complementaryPush ? "    uint valueOffset2;\n" : "") // Complementary push constant last.
                  << "} pc;\n";
        dispatchOffsetPrefix = skipIndexPrefix = valueOffsetPrefix = valueOffset2Prefix = "pc.";
    }
    else if (m_params.testType == TestType::PIPELINE_DISPATCH)
    {
        // Specialization constants only.
        constants << "layout (constant_id=0) const uint pc_dispatchOffset = 0u;\n"
                  << "layout (constant_id=1) const uint pc_skipIndex = 0u;\n"
                  << "layout (constant_id=2) const uint pc_valueOffset = 0u;\n";
        dispatchOffsetPrefix = skipIndexPrefix = valueOffsetPrefix = "pc_";
    }
    else if (m_params.testType == TestType::PIPELINE_PUSH_DISPATCH || complementaryPushWithPipeline)
    {
        // Mixed: some push constants, some specialization.
        constants << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                  << (complementaryPushWithPipeline ? "    uint valueOffset2;\n" :
                                                      "") // Complementary push constant first.
                  << "    uint dispatchOffset;\n"
                  << "    uint skipIndex;\n"
                  << "} pc;\n"
                  << "layout (constant_id=2) const uint pc_valueOffset = 0u;\n" // Note constant_id=2
            ;
        dispatchOffsetPrefix = skipIndexPrefix = valueOffset2Prefix = "pc.";
        valueOffsetPrefix                                           = "pc_";
    }
    else
        DE_ASSERT(false);

    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << kLocalInvocations << ", local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0, std430) buffer StorageBlock { uint values[]; } storageBuffer;\n"
         << constants.str() << "void main (void) {\n"
         << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z + "
            "gl_NumWorkGroups.x * gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
         << "    const uint valueIndex = " << dispatchOffsetPrefix
         << "dispatchOffset + workGroupIndex * gl_WorkGroupSize.x + gl_LocalInvocationIndex;\n"
         << "    const uint storageValue = " << valueOffsetPrefix
         << "valueOffset + (workGroupIndex << 10) + gl_LocalInvocationIndex\n";

    if (complementaryPush || complementaryPushWithPipeline)
        comp << "        + " << valueOffset2Prefix << "valueOffset2\n";

    comp << "        ;\n"
         << "    if (" << skipIndexPrefix << "skipIndex != gl_LocalInvocationIndex) {\n"
         << "        storageBuffer.values[valueIndex] = storageValue;\n"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

TestInstance *LayoutTestCase::createInstance(Context &context) const
{
    return new LayoutTestInstance(context, m_params);
}

void LayoutTestInstance::createPipelines(const DeviceInterface &vkd, VkDevice device, vk::Allocator &allocator,
                                         VkDescriptorSetLayout setLayout,
                                         const std::vector<SpecializationData> &specializationData)
{
    const bool pipelineSwitch = hasPipelineSwitch(m_params.testType);
    const bool extraPush      = hasComplementaryPush(m_params.testType);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compModule = createShaderModule(vkd, device, binaries.get("comp"));

    if (pipelineSwitch)
    {
        // The sequence includes a set-pipeline token, so we'll populate m_dgcPipelines with one different pipeline per dispatch.

        // Specialization constant ids used by the shader.
        std::vector<uint32_t> constantIds;

        if (m_params.testType == TestType::PIPELINE_DISPATCH)
        {
            // 3 constants with ids 0, 1, 2; see shader code.
            constantIds.resize(3u);
            std::iota(begin(constantIds), end(constantIds), 0u);
        }
        else if (m_params.testType == TestType::PIPELINE_PUSH_DISPATCH || extraPush)
            constantIds.push_back(2u); // Single constant with id 2; see shader code.
        else
            DE_ASSERT(false);

        // Specialization map entries, depending on constantIds. Note for the case of a single constant with id 2 (when the first 2
        // constants are passed as push constants), we don't use offset 0 in the map entry. This is because below, when creating
        // the specialization info, we always pass the base address of the SpecializationData structure as the base address for the
        // spec constant data, so the offset we pass here will match the position of the spec constant data in the structure.
        std::vector<VkSpecializationMapEntry> mapEntries;
        mapEntries.reserve(constantIds.size());
        for (const auto constantId : constantIds)
            mapEntries.push_back(makeSpecializationMapEntry(constantId, m_constantSize * constantId, m_constantSize));

        // Prepare the right layout.
        if (m_params.testType == TestType::PIPELINE_DISPATCH)
        {
            // Specialization constants only.
            m_pipelineLayout = makePipelineLayout(vkd, device, setLayout);
        }
        else if (m_params.testType == TestType::PIPELINE_PUSH_DISPATCH || extraPush)
        {
            // See shader: mix of push constants and spec constants. 2 push constants (3 for complementary push) and 1 spec constant.
            const auto pcItems = 2u;
            m_pcTokenDataSize  = pcItems * m_constantSize;
            m_layoutPcSize     = m_pcTokenDataSize;
            if (extraPush)
                m_layoutPcSize += m_constantSize;

            const auto pcRange = makePushConstantRange(m_shaderStage, 0u, m_layoutPcSize);

            m_pipelineLayout = makePipelineLayout(vkd, device, setLayout, &pcRange);
        }
        else
            DE_ASSERT(false);

        // Create the pipelines, optionally using capture/replay.
        std::vector<VkDeviceAddress> capturedAddresses(specializationData.size(), 0ull);

        if (m_params.captureReplay)
        {
            DGCComputePipelineMetaDataPool temporaryPool(DGCComputePipelineMetaDataPool::kDefaultMultiplier, true);
            std::vector<DGCComputePipelinePtr> temporaryPipelines;
            temporaryPipelines.reserve(specializationData.size());

            for (size_t i = 0; i < specializationData.size(); ++i)
            {
                const auto &data = specializationData.at(i);
                const auto specializationInfo =
                    makeSpecializationInfo(de::sizeU32(mapEntries), de::dataOrNull(mapEntries), sizeof(data), &data);
                temporaryPipelines.emplace_back(new DGCComputePipeline(
                    temporaryPool, vkd, device, allocator, 0u, *m_pipelineLayout, 0u, *compModule,
                    &specializationInfo)); // Note no capture/replay address given.

                // Save capture/replay address.
                capturedAddresses.at(i) = temporaryPipelines.back()->getIndirectDeviceAddress();
            }

            // The temporary pipelines and medatada pool will go out of scope here.
        }

        m_metaDataPool.reset(new DGCComputePipelineMetaDataPool(DGCComputePipelineMetaDataPool::kDefaultMultiplier,
                                                                m_params.captureReplay));
        for (size_t i = 0; i < specializationData.size(); ++i)
        {
            const auto &data = specializationData.at(i);
            const auto specializationInfo =
                makeSpecializationInfo(de::sizeU32(mapEntries), de::dataOrNull(mapEntries), sizeof(data), &data);
            m_dgcPipelines.emplace_back(new DGCComputePipeline(
                *m_metaDataPool, vkd, device, allocator, 0u, *m_pipelineLayout, 0u, *compModule, &specializationInfo,
                capturedAddresses.at(i))); // Capture/replay address if needed (it will be 0 otherwise).
        }
    }
    else
    {
        // Create m_singlePipeline. This case uses push constants only.
        m_pcTokenDataSize = static_cast<uint32_t>(sizeof(SpecializationData));
        m_layoutPcSize    = m_pcTokenDataSize;
        if (extraPush)
            m_layoutPcSize += m_constantSize; // An extra constant to be pushed outside the cmd stream.
        const auto pcRange = makePushConstantRange(m_shaderStage, 0u, m_layoutPcSize);

        m_pipelineLayout = makePipelineLayout(vkd, device, setLayout, &pcRange);
        m_singlePipeline = makeComputePipeline(vkd, device, *m_pipelineLayout, *compModule);
    }
}

void LayoutTestInstance::makeCommandsLayout(const DeviceInterface &vkd, VkDevice device)
{
    // Note we always add the dispatch token at the end.
    IndirectCommandsLayoutBuilder cmdsLayoutBuilder(0u, m_bindPoint);

    if (m_params.testType == TestType::PUSH_DISPATCH || m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH)
    {
        DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);
        DE_ASSERT(m_pcTokenDataSize != 0u);
        cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *m_pipelineLayout, m_shaderStage, 0u, m_pcTokenDataSize);
    }
    else if (m_params.testType == TestType::PARTIAL_PUSH_DISPATCH)
    {
        // We have 3 push constants and we'll update them in two steps: 0,1 followed by 1,2. This means both updates will be
        // partial, the value of the middle constant will be overwritten, and the first value we push will not be the correct one.
        DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);
        cmdsLayoutBuilder.addPushConstantToken(0u, 0u, *m_pipelineLayout, m_shaderStage, 0u, m_constantSize * 2u);
        cmdsLayoutBuilder.addPushConstantToken(0u, cmdsLayoutBuilder.getStreamRange(0u), *m_pipelineLayout,
                                               m_shaderStage, m_constantSize, m_constantSize * 2u);
    }
    else if (m_params.testType == TestType::PIPELINE_DISPATCH)
    {
        cmdsLayoutBuilder.addPipelineToken(0u, 0u);
    }
    else if (m_params.testType == TestType::PIPELINE_PUSH_DISPATCH ||
             m_params.testType == TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH)
    {
        DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);
        DE_ASSERT(m_pcTokenDataSize != 0u);
        const auto pcOffset =
            ((m_params.testType == TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH) ? m_constantSize : 0u);
        cmdsLayoutBuilder.addPipelineToken(0u, 0u);
        cmdsLayoutBuilder.addPushConstantToken(0u, cmdsLayoutBuilder.getStreamRange(0u), *m_pipelineLayout,
                                               m_shaderStage, pcOffset, m_pcTokenDataSize);
    }
    else
        DE_ASSERT(false);

    // The dispatch always comes last.
    cmdsLayoutBuilder.addDispatchToken(0u, cmdsLayoutBuilder.getStreamRange(0u));

    if (hasPipelineSwitch(m_params.testType) && !m_params.align4)
    {
        // Extend the stream stride manually to make sure pipeline addresses are always aligned to their native size.
        for (uint32_t i = 0u; i < cmdsLayoutBuilder.getStreamCount(); ++i)
        {
            const auto autoStride   = cmdsLayoutBuilder.getStreamRange(i);
            const auto manualStride = de::roundUp(autoStride, static_cast<uint32_t>(sizeof(VkDeviceAddress)));
            cmdsLayoutBuilder.setStreamStride(i, manualStride);
        }
    }

    m_commandsLayout = cmdsLayoutBuilder.build(vkd, device);
}

std::vector<uint32_t> LayoutTestInstance::makeIndirectCommands(
    const std::vector<uint32_t> &wgCounts, const std::vector<SpecializationData> &specializationData) const
{
    DE_ASSERT(wgCounts.size() == specializationData.size());
    std::vector<uint32_t> indirectCmds;

    const auto pushDispatchIndirectCommand = [&indirectCmds](uint32_t count, bool addPadding)
    {
        // These will be interpreted as a VkDispatchIndirectCommand: .x=count .y=1 .z=1
        indirectCmds.push_back(count);
        indirectCmds.push_back(1u);
        indirectCmds.push_back(1u);

        if (addPadding)
            indirectCmds.push_back(0xA1B2C3D4u);
    };

    const bool align8 = (!m_params.align4);

    if (m_params.testType == TestType::PUSH_DISPATCH || m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH)
    {
        // 6 uint32_t in total per dispatch: 3 push constants and the indirect dispatch dimensions.
        indirectCmds.reserve(wgCounts.size() * 6u);

        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &data    = specializationData.at(i);
            const auto &wgCount = wgCounts.at(i);

            indirectCmds.push_back(data.dispatchOffset);
            indirectCmds.push_back(data.skipIndex);
            indirectCmds.push_back(data.valueOffset);
            pushDispatchIndirectCommand(wgCount, false); // No padding needed.
        }
    }
    else if (m_params.testType == TestType::PARTIAL_PUSH_DISPATCH)
    {
        // We have 3 push constants and we'll update them in two steps: 0,1 followed by 1,2. This means both updates will be
        // partial, the value of the middle constant will be overwritten, and the first value we push will not be the correct one.
        // We have 7 uint32_t in total per dispatch: 4 push constants (with overlap) and the indirect dispatch dimensions.
        indirectCmds.reserve(wgCounts.size() * 7u);

        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &data    = specializationData.at(i);
            const auto &wgCount = wgCounts.at(i);

            indirectCmds.push_back(data.dispatchOffset);
            indirectCmds.push_back(kLocalInvocations - data.skipIndex - 1u); // Bad value on purpose.
            indirectCmds.push_back(data.skipIndex);
            indirectCmds.push_back(data.valueOffset);
            pushDispatchIndirectCommand(wgCount, false); // No padding needed.
        }
    }
    else if (m_params.testType == TestType::PIPELINE_DISPATCH)
    {
        // We have 5 uint32_t per dispatch: 2 for the pipeline address (which is 1 VkDeviceAddress), 3 for the indirect dispatch
        // command. We'd need one more uint32_t for padding to make sure pipeline addresses are aligned. See makeCommandsLayout().
        DE_ASSERT(m_dgcPipelines.size() == wgCounts.size());
        indirectCmds.reserve(wgCounts.size() * 6u);

        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &dgcPipeline  = m_dgcPipelines.at(i);
            const auto &wgCount      = wgCounts.at(i);
            const auto deviceAddress = dgcPipeline->getIndirectDeviceAddress();

            pushBackDeviceAddress(indirectCmds, deviceAddress);
            pushDispatchIndirectCommand(wgCount, align8); // Padding may be added.
        }
    }
    else if (m_params.testType == TestType::PIPELINE_PUSH_DISPATCH ||
             m_params.testType == TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH)
    {
        // We have 7 uint32_t per dispatch: 2 for the pipeline address (which is 1 VkDeviceAddress), 2 for the push constants and 3
        // for the indirect dispatch command. We'd need one more uint32_t for padding to make sure pipeline addresses are aligned.
        // See makeCommandsLayout().
        DE_ASSERT(m_dgcPipelines.size() == wgCounts.size());
        indirectCmds.reserve(wgCounts.size() * 8u);

        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &wgCount      = wgCounts.at(i);
            const auto &data         = specializationData.at(i);
            const auto &dgcPipeline  = m_dgcPipelines.at(i);
            const auto deviceAddress = dgcPipeline->getIndirectDeviceAddress();

            pushBackDeviceAddress(indirectCmds, deviceAddress);
            indirectCmds.push_back(data.dispatchOffset);
            indirectCmds.push_back(data.skipIndex);
            // valueOffset provided as a specialization constant in the shader.
            pushDispatchIndirectCommand(wgCount, align8); // Padding may be added.
        }
    }
    else
        DE_ASSERT(false);

    return indirectCmds;
}

tcu::TestStatus LayoutTestInstance::iterate(void)
{
    const auto ctx          = m_context.getContextCommonData();
    const auto qfIndex      = (m_params.computeQueue ? m_context.getComputeQueueFamilyIndex() : ctx.qfIndex);
    const auto queue        = (m_params.computeQueue ? m_context.getComputeQueue() : ctx.queue);
    const bool extraPush    = hasComplementaryPush(m_params.testType);
    const auto valueOffset2 = (extraPush ? kLocalInvocations : 0u);

    // Generate the work group count for each dispatch.
    const int minDispatchSize = 1;
    const int maxDispatchSize = 16;

    const uint32_t seed = (0xff0000u | static_cast<uint32_t>(m_params.testType));
    de::Random rnd(seed);

    // Work group count for each dispatch.
    std::vector<uint32_t> wgCounts;
    wgCounts.reserve(kSequenceCount);
    for (uint32_t i = 0u; i < kSequenceCount; ++i)
        wgCounts.push_back(static_cast<uint32_t>(rnd.getInt(minDispatchSize, maxDispatchSize)));

    // Specialization data for each dispatch.
    std::vector<SpecializationData> specializationData;
    specializationData.reserve(wgCounts.size());
    {
        uint32_t prevWGs = 0u;
        for (size_t i = 0u; i < wgCounts.size(); ++i)
        {
            const SpecializationData data{
                (prevWGs * kLocalInvocations), // uint32_t dispatchOffset;
                static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(kLocalInvocations) - 1)), // uint32_t skipIndex;
                static_cast<uint32_t>((i + 1) << 20),                                          // uint32_t valueOffset;
            };
            specializationData.push_back(data);
            prevWGs += wgCounts.at(i);
        }
    }

    // Calculate the required size of the output buffer.
    const uint32_t totalNumWorkGroups = std::accumulate(begin(wgCounts), end(wgCounts), 0u);
    const uint32_t totalInvocations   = kLocalInvocations * totalNumWorkGroups;
    const VkDeviceSize outputBufferSize =
        static_cast<VkDeviceSize>(totalInvocations) * static_cast<VkDeviceSize>(sizeof(uint32_t));

    // Create a host-visible output buffer.
    const auto outputBufferType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto outputBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto outputBufferInfo  = makeBufferCreateInfo(outputBufferSize, outputBufferUsage);
    BufferWithMemory outputBuffer(ctx.vkd, ctx.device, ctx.allocator, outputBufferInfo, MemoryRequirement::HostVisible);
    auto &outputBufferAlloc = outputBuffer.getAllocation();
    void *outputBufferData  = outputBufferAlloc.getHostPtr();

    std::vector<uint32_t> outputBufferValues(totalInvocations, 0u);
    deMemcpy(outputBufferData, de::dataOrNull(outputBufferValues), de::dataSize(outputBufferValues));
    flushAlloc(ctx.vkd, ctx.device, outputBufferAlloc);

    // Create the descriptor set layout, descriptor set and update it.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(outputBufferType, m_shaderStage);
    const auto setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(outputBufferType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), outputBufferType,
                                 &outputBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Create the pipelines.
    createPipelines(ctx.vkd, ctx.device, ctx.allocator, *setLayout, specializationData);

    // Make the commands layout.
    makeCommandsLayout(ctx.vkd, ctx.device);

    // Create indirect commands buffer contents.
    const auto indirectCommands = makeIndirectCommands(wgCounts, specializationData);

    // Create a host-visible buffer to store them.
    const auto indirectCmdsBufferSize = static_cast<VkDeviceSize>(de::dataSize(indirectCommands));
    const auto indirectCmdsBufferInfo =
        makeBufferCreateInfo(indirectCmdsBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    BufferWithMemory indirectCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectCmdsBufferInfo,
                                        MemoryRequirement::HostVisible);
    auto &indirectCmdsBufferAlloc = indirectCmdsBuffer.getAllocation();
    void *indirectCmdsBufferData  = indirectCmdsBufferAlloc.getHostPtr();

    deMemcpy(indirectCmdsBufferData, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
    flushAlloc(ctx.vkd, ctx.device, indirectCmdsBufferAlloc);

    // Create a preprocess buffer.
    // Note m_singlePipeline will be VK_NULL_HANDLE when using multiple pipelines, which is exactly what we need.
    PreprocessBuffer preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, m_bindPoint, *m_singlePipeline,
                                      *m_commandsLayout, de::sizeU32(wgCounts));

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, m_bindPoint, *m_pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);

    // Bind or prepare pipelines.
    if (*m_singlePipeline != VK_NULL_HANDLE)
        ctx.vkd.cmdBindPipeline(cmdBuffer, m_bindPoint, *m_singlePipeline);
    else
    {
        DE_ASSERT(!m_dgcPipelines.empty());
        for (const auto &dgcPipeline : m_dgcPipelines)
            ctx.vkd.cmdUpdatePipelineIndirectBufferNV(cmdBuffer, m_bindPoint, dgcPipeline->get());

        metadataUpdateToPreprocessBarrier(ctx.vkd, cmdBuffer);
    }

    if (extraPush)
    {
        // Must match the offset and size of valueOffset2 in the shaders.
        const auto pcOffset = ((m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH) ? m_pcTokenDataSize : 0u);
        ctx.vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, m_shaderStage, pcOffset, m_constantSize, &valueOffset2);
    }

    // Execute indirect commands.
    // Note m_singlePipeline will be VK_NULL_HANDLE when using multiple pipelines, which is exactly what we want.
    const VkIndirectCommandsStreamNV streamInfo           = makeIndirectCommandsStreamNV(*indirectCmdsBuffer, 0ull);
    const VkGeneratedCommandsInfoNV generatedCommandsInfo = {
        VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        m_bindPoint,                                  // VkPipelineBindPoint pipelineBindPoint;
        *m_singlePipeline,                            // VkPipeline pipeline;
        *m_commandsLayout,                            // VkIndirectCommandsLayoutNV indirectCommandsLayout;
        1u,                                           // uint32_t streamCount;
        &streamInfo,                                  // const VkIndirectCommandsStreamNV* pStreams;
        de::sizeU32(wgCounts),                        // uint32_t sequencesCount;
        *preprocessBuffer,                            // VkBuffer preprocessBuffer;
        0ull,                                         // VkDeviceSize preprocessOffset;
        preprocessBuffer.getSize(),                   // VkDeviceSize preprocessSize;
        VK_NULL_HANDLE,                               // VkBuffer sequencesCountBuffer;
        0ull,                                         // VkDeviceSize sequencesCountOffset;
        VK_NULL_HANDLE,                               // VkBuffer sequencesIndexBuffer;
        0ull,                                         // VkDeviceSize sequencesIndexOffset;
    };
    ctx.vkd.cmdExecuteGeneratedCommandsNV(cmdBuffer, VK_FALSE, &generatedCommandsInfo);

    // Sync writes to the output buffer.
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, queue, cmdBuffer);

    // Retrieve output values.
    invalidateAlloc(ctx.vkd, ctx.device, outputBufferAlloc);
    deMemcpy(de::dataOrNull(outputBufferValues), outputBufferData, de::dataSize(outputBufferValues));

    // Verify results.
    bool fail           = false;
    uint32_t valueIndex = 0u;
    auto &log           = m_context.getTestContext().getLog();

    for (size_t dispatchIdx = 0u; dispatchIdx < wgCounts.size(); ++dispatchIdx)
    {
        const auto &wgCount     = wgCounts.at(dispatchIdx);
        const auto &data        = specializationData.at(dispatchIdx);
        const auto &skipIndex   = data.skipIndex;
        const auto &valueOffset = data.valueOffset;

        for (uint32_t wgIdx = 0u; wgIdx < wgCount; ++wgIdx)
        {
            for (uint32_t invocationIdx = 0u; invocationIdx < kLocalInvocations; ++invocationIdx)
            {
                // Must match the calculation in the shader, obviously.
                const auto expected =
                    ((invocationIdx == skipIndex) ? 0u : (valueOffset + (wgIdx << 10) + invocationIdx + valueOffset2));
                const auto result = outputBufferValues.at(valueIndex);

                if (expected != result)
                {
                    log << tcu::TestLog::Message << "Unexpected value at index " << valueIndex << "; expected "
                        << expected << " but found " << result << " ; dispatchIndex=" << dispatchIdx
                        << " workGroupIndex=" << wgIdx << " invocationIndex=" << invocationIdx
                        << " skipIndex=" << skipIndex << " valueOffset=" << valueOffset << tcu::TestLog::EndMessage;
                    fail = true;
                }

                ++valueIndex;
            }
        }
    }

    if (fail)
    {
        log << tcu::TestLog::Message << "Dispatch sizes:" << tcu::TestLog::EndMessage;
        for (const auto wgCount : wgCounts)
            log << tcu::TestLog::Message << "    " << wgCount << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Unexpected output values found; check log for details");
    }
    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDGCComputeLayoutTests(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "layout"));

    const struct
    {
        TestType testType;
        bool align4;
        bool captureReplay;
        const char *name;
    } testTypesTable[] = {
        {TestType::PUSH_DISPATCH, false, false, "push_dispatch"},
        {TestType::COMPLEMENTARY_PUSH_DISPATCH, false, false, "complementary_push_dispatch"},
        {TestType::PARTIAL_PUSH_DISPATCH, false, false, "partial_push_dispatch"},
        {TestType::PIPELINE_DISPATCH, false, false, "pipeline_dispatch"},
        {TestType::PIPELINE_PUSH_DISPATCH, false, false, "pipeline_push_dispatch"},
        {TestType::PIPELINE_PUSH_DISPATCH, false, true, "pipeline_push_dispatch_capture_replay"},
        {TestType::PIPELINE_DISPATCH, true, false, "pipeline_dispatch_align4"},
        {TestType::PIPELINE_PUSH_DISPATCH, true, false, "pipeline_push_dispatch_align4"},
        {TestType::PIPELINE_COMPLEMENTARY_PUSH_DISPATCH, false, false, "pipeline_complementary_push_dispatch"},
    };

    for (const auto useComputeQueue : {false, true})
        for (const auto &testCase : testTypesTable)
        {
            TestParams params{testCase.testType, testCase.align4, useComputeQueue, testCase.captureReplay};
            const auto testName = std::string(testCase.name) + (useComputeQueue ? "_cq" : "");
            mainGroup->addChild(new LayoutTestCase(testCtx, testName, params));
        }

    return mainGroup.release();
}
} // namespace DGC
} // namespace vkt
