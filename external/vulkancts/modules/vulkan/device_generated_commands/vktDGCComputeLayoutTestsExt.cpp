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
 * \brief Device Generated Commands EXT Compute Layout Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputeLayoutTestsExt.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"
#include "vkShaderObjectUtil.hpp"

#include "deRandom.hpp"

#include <numeric>
#include <sstream>
#include <memory>
#include <vector>
#include <limits>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

// Note the smoke tests already cover the case of the layout containing dispatches only, which is a bit challenging due
// to the lack of specialization for each dispatch. In these tests we'll check other cases in ways that allow us to
// specialize each dispatch and write results to different areas.
//
// "complementary" tests use a push constant, either the first one or the last one, that's fixed for all dispatches and
// pushed outside the indirect commands, complementing the set of push constants from the indirect commands.
enum class TestType
{
    PUSH_DISPATCH = 0,
    COMPLEMENTARY_PUSH_DISPATCH,
    COMPLEMENTARY_PUSH_INDEX_DISPATCH,
    MULTI_PUSH_DISPATCH,
    OFFSET_EXECUTION_SET_DISPATCH, // Same as EXECUTION_SET_DISPATCH but the execution set token uses a nonzero offset.
    EXECUTION_SET_DISPATCH,
    EXECUTION_SET_PUSH_DISPATCH,
    EXECUTION_SET_INDEX_PUSH_DISPATCH,
    EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH,
};

bool hasExecutionSet(TestType testType)
{
    const bool pipelineSwitch =
        (testType == TestType::OFFSET_EXECUTION_SET_DISPATCH || testType == TestType::EXECUTION_SET_DISPATCH ||
         testType == TestType::EXECUTION_SET_PUSH_DISPATCH || testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH ||
         testType == TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH);
    return pipelineSwitch;
}

bool hasSequenceIndex(TestType testType)
{
    return (testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH ||
            testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH);
}

// Has a fourth push constant that's pushed outside the indirect commands stream.
bool hasComplementaryPush(TestType testType)
{
    return (testType == TestType::COMPLEMENTARY_PUSH_DISPATCH ||
            testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH ||
            testType == TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH);
}

struct TestParams
{
    TestParams(TestType testType_, bool shaderObjects_, bool computeQueue_, bool dynamicPipelineLayout_)
        : testType(testType_)
        , shaderObjects(shaderObjects_)
        , computeQueue(computeQueue_)
        , dynamicPipelineLayout(dynamicPipelineLayout_)
    {
    }

    TestType testType;
    bool shaderObjects;         // Use shader objects instead of pipelines.
    bool computeQueue;          // Use the compute queue.
    bool dynamicPipelineLayout; // Use dynamicGeneratedPipelineLayout.
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
        , m_shaderStageBit(VK_SHADER_STAGE_COMPUTE_BIT)
        , m_shaderStage(static_cast<VkShaderStageFlags>(m_shaderStageBit))
        , m_bindPoint(VK_PIPELINE_BIND_POINT_COMPUTE)
        , m_constantSize(static_cast<uint32_t>(sizeof(uint32_t))) // All constants are uints in the shader.
        , m_pcTokenDataSize(0u)
        , m_layoutPcSize(0u)
        , m_pcRanges()
        , m_setLayout()
        , m_pipelineLayout()
        , m_singlePipeline()
        , m_singleShader()
        , m_dgcPipelines()
        , m_dgcShaders()
        , m_commandsLayout()
    {
    }
    virtual ~LayoutTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    using DGCComputePipelinePtr  = std::unique_ptr<DGCComputePipelineExt>;
    using DGCComputeShaderExtPtr = std::unique_ptr<DGCComputeShaderExt>;

    // Sets m_pipelineLayout, and m_singlePipeline/m_singleShader or m_dgcPipelines/m_dgcShaders depending on the test type.
    void createPipelinesOrShaders(const DeviceInterface &vkd, VkDevice device,
                                  const std::vector<SpecializationData> &specializationData);

    // Returns a VkPipelineLayoutCreateInfo with the descriptor set layouts and the push constant ranges.
    VkPipelineLayoutCreateInfo getPipelineLayoutCreateInfo(void);

    // Sets m_commandsLayout.
    void makeCommandsLayout(const DeviceInterface &vkd, VkDevice device, VkPipelineLayout pipelineLayout,
                            const VkPipelineLayoutCreateInfo *createInfo = nullptr);

    // Creates a "buffer" with the indirect commands.
    std::vector<uint32_t> makeIndirectCommands(const std::vector<uint32_t> &wgCounts,
                                               const std::vector<SpecializationData> &specializationData) const;

    const TestParams m_params;
    const VkShaderStageFlagBits m_shaderStageBit;
    const VkShaderStageFlags m_shaderStage;
    const VkPipelineBindPoint m_bindPoint;
    const uint32_t m_constantSize;

    uint32_t m_pcTokenDataSize;
    uint32_t m_layoutPcSize;
    std::vector<VkPushConstantRange> m_pcRanges;
    Move<VkDescriptorSetLayout> m_setLayout;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_singlePipeline;                 // Used when the test case contains no execution sets.
    Move<VkShaderEXT> m_singleShader;                  // Ditto.
    std::vector<DGCComputePipelinePtr> m_dgcPipelines; // Used when the commands layout contains pipeline switch tokens.
    std::vector<DGCComputeShaderExtPtr> m_dgcShaders;  // Ditto.
    Move<VkIndirectCommandsLayoutEXT> m_commandsLayout;
    VkDeviceSize m_commandsStride;
};

void LayoutTestCase::checkSupport(Context &context) const
{
    const bool requireBinds = hasExecutionSet(m_params.testType);
    checkDGCExtComputeSupport(context, requireBinds);

    if (m_params.shaderObjects)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");
        if (requireBinds)
        {
            const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
            if (dgcProperties.maxIndirectShaderObjectCount == 0u)
                TCU_THROW(NotSupportedError, "maxIndirectShaderObjectCount is zero");
        }
    }

    if (m_params.computeQueue)
        context.getComputeQueue(); // Will throw NotSupportedError if not available.

    if (m_params.dynamicPipelineLayout)
    {
        const auto &dgcFeatures = context.getDeviceGeneratedCommandsFeaturesEXT();
        if (!dgcFeatures.dynamicGeneratedPipelineLayout)
            TCU_THROW(NotSupportedError, "dynamicGeneratedPipelineLayout not supported");
    }
}

void LayoutTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream constants;
    std::string dispatchOffsetPrefix;
    std::string skipIndexPrefix;
    std::string valueOffsetPrefix;
    std::string valueOffset2Prefix;

    const bool sequenceIndex     = hasSequenceIndex(m_params.testType);
    const bool complementaryPush = (m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH ||
                                    m_params.testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH);
    const bool complementaryPushWithExecutionSet =
        (m_params.testType == TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH);

    // Note the constants will match the SpecializationData structure.
    if (m_params.testType == TestType::PUSH_DISPATCH || m_params.testType == TestType::MULTI_PUSH_DISPATCH ||
        complementaryPush)
    {
        // Push constants only.
        constants << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                  << "    uint dispatchOffset;\n"
                  << "    uint skipIndex;\n"
                  << "    uint valueOffset;\n"
                  << (complementaryPush ? "    uint valueOffset2;\n" : "") // Complementary push constant last.
                  << (sequenceIndex ? "    uint sequenceIndex;\n" : "") << "} pc;\n";
        dispatchOffsetPrefix = skipIndexPrefix = valueOffsetPrefix = valueOffset2Prefix = "pc.";
    }
    else if (m_params.testType == TestType::EXECUTION_SET_DISPATCH ||
             m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH)
    {
        // Specialization constants only.
        constants << "layout (constant_id=0) const uint pc_dispatchOffset = 0u;\n"
                  << "layout (constant_id=1) const uint pc_skipIndex = 0u;\n"
                  << "layout (constant_id=2) const uint pc_valueOffset = 0u;\n";
        dispatchOffsetPrefix = skipIndexPrefix = valueOffsetPrefix = "pc_";
    }
    else if (m_params.testType == TestType::EXECUTION_SET_PUSH_DISPATCH ||
             m_params.testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH || complementaryPushWithExecutionSet)
    {
        // Mixed: some push constants, some specialization.
        constants << "layout (push_constant, std430) uniform PushConstantBlock {\n"
                  << (complementaryPushWithExecutionSet ? "    uint valueOffset2;\n" :
                                                          "") // Complementary push constant first.
                  << "    uint dispatchOffset;\n"
                  << "    uint skipIndex;\n"
                  << (sequenceIndex ? "    uint sequenceIndex;\n" : "") << "} pc;\n"
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

    if (complementaryPush || complementaryPushWithExecutionSet)
        comp << "        + " << valueOffset2Prefix << "valueOffset2\n";

    if (sequenceIndex)
        comp << "        + pc.sequenceIndex\n";

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

// This is used to create non-DGC shader objects. For DGC shader objects we have a separate class.
VkShaderCreateInfoEXT makeComputeShaderCreateInfo(VkShaderCreateFlagsEXT flags, const ProgramBinary &binary,
                                                  const VkDescriptorSetLayout &setLayout,
                                                  const VkPushConstantRange &pcRange)
{
    if (binary.getFormat() != PROGRAM_FORMAT_SPIRV)
        TCU_THROW(InternalError, "Program format not supported");

    const VkShaderCreateInfoEXT info = {
        VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, //    VkStructureType                 sType;
        nullptr,                                  //    const void*                     pNext;
        flags,                                    //    VkShaderCreateFlagsEXT          flags;
        VK_SHADER_STAGE_COMPUTE_BIT,              //    VkShaderStageFlagBits           stage;
        0u,                                       //    VkShaderStageFlags              nextStage;
        VK_SHADER_CODE_TYPE_SPIRV_EXT,            //    VkShaderCodeTypeEXT             codeType;
        binary.getSize(),                         //    size_t                          codeSize;
        binary.getBinary(),                       //    const void*                     pCode;
        "main",                                   //    const char*                     pName;
        1u,                                       //    uint32_t                        setLayoutCount;
        &setLayout,                               //    const VkDescriptorSetLayout*    pSetLayouts;
        1u,                                       //    uint32_t                        pushConstantRangeCount;
        &pcRange,                                 //    const VkPushConstantRange*      pPushConstantRanges;
        nullptr,                                  //    const VkSpecializationInfo*     pSpecializationInfo;
    };

    binary.setUsed();

    return info;
}

void LayoutTestInstance::createPipelinesOrShaders(const DeviceInterface &vkd, VkDevice device,
                                                  const std::vector<SpecializationData> &specializationData)
{
    const bool usesExecutionSet = hasExecutionSet(m_params.testType);
    const bool extraPush        = hasComplementaryPush(m_params.testType);
    const bool sequenceIndex    = hasSequenceIndex(m_params.testType);
    const auto &binaries        = m_context.getBinaryCollection();
    const auto shaderBinary     = binaries.get("comp");

    Move<VkShaderModule> compModule;
    if (!m_params.shaderObjects)
        compModule = createShaderModule(vkd, device, shaderBinary);

    const std::vector<VkDescriptorSetLayout> setLayouts{*m_setLayout};

    if (usesExecutionSet)
    {
        // The sequence includes a set-pipeline token, so we'll populate m_dgcPipelines with one different pipeline per dispatch, or
        // m_dgcShaders with one different shader per dispatch.

        // Specialization constant ids used by the shader.
        std::vector<uint32_t> constantIds;

        if (m_params.testType == TestType::EXECUTION_SET_DISPATCH ||
            m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH)
        {
            // 3 constants with ids 0, 1, 2; see shader code.
            constantIds.resize(3u);
            std::iota(begin(constantIds), end(constantIds), 0u);
        }
        else if (m_params.testType == TestType::EXECUTION_SET_PUSH_DISPATCH || sequenceIndex || extraPush)
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
        if (m_params.testType == TestType::EXECUTION_SET_DISPATCH ||
            m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH)
        {
            // Specialization constants only.
        }
        else if (m_params.testType == TestType::EXECUTION_SET_PUSH_DISPATCH ||
                 m_params.testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH || extraPush)
        {
            // See shader: mix of push constants and spec constants. 2 push constants (+1 for complementary push, +1 for sequence index) and 1 spec constant.
            const auto pcItems = 2u;
            m_pcTokenDataSize  = pcItems * m_constantSize;
            m_layoutPcSize     = m_pcTokenDataSize;
            if (sequenceIndex)
                m_layoutPcSize += m_constantSize;
            if (extraPush)
                m_layoutPcSize += m_constantSize;

            const auto pcRange = makePushConstantRange(m_shaderStage, 0u, m_layoutPcSize);
            m_pcRanges.push_back(pcRange);
        }
        else
            DE_ASSERT(false);

        m_pipelineLayout = makePipelineLayout(vkd, device, de::sizeU32(setLayouts), de::dataOrNull(setLayouts),
                                              de::sizeU32(m_pcRanges), de::dataOrNull(m_pcRanges));

        for (size_t i = 0; i < specializationData.size(); ++i)
        {
            const auto &data = specializationData.at(i);
            const auto specializationInfo =
                makeSpecializationInfo(de::sizeU32(mapEntries), de::dataOrNull(mapEntries), sizeof(data), &data);

            if (m_params.shaderObjects)
            {
                m_dgcShaders.emplace_back(new DGCComputeShaderExt(vkd, device, 0u, shaderBinary, setLayouts, m_pcRanges,
                                                                  &specializationInfo));
            }
            else
            {
                m_dgcPipelines.emplace_back(new DGCComputePipelineExt(vkd, device, 0u, *m_pipelineLayout, 0u,
                                                                      *compModule, &specializationInfo, 0ull));
            }
        }
    }
    else
    {
        // Create m_singlePipeline. This case uses push constants only.
        m_pcTokenDataSize = static_cast<uint32_t>(sizeof(SpecializationData));
        m_layoutPcSize    = m_pcTokenDataSize;
        if (extraPush)
        {
            const auto extraConstants = (sequenceIndex ? 2u : 1u);
            m_layoutPcSize += m_constantSize * extraConstants; // Partially outside the cmd stream.
        }
        const auto pcRange = makePushConstantRange(m_shaderStage, 0u, m_layoutPcSize);

        m_pcRanges.push_back(pcRange);
        m_pipelineLayout = makePipelineLayout(vkd, device, *m_setLayout, &pcRange);

        if (m_params.shaderObjects)
        {
            const auto shaderCreateInfo = makeComputeShaderCreateInfo(0u, binaries.get("comp"), *m_setLayout, pcRange);
            m_singleShader              = createShader(vkd, device, shaderCreateInfo);
        }
        else
        {
            m_singlePipeline = makeComputePipeline(vkd, device, *m_pipelineLayout, *compModule);
        }
    }
}

VkPipelineLayoutCreateInfo LayoutTestInstance::getPipelineLayoutCreateInfo(void)
{
    DE_ASSERT(*m_setLayout != VK_NULL_HANDLE);

    const VkPipelineLayoutCreateInfo info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, //  VkStructureType              sType;
        nullptr,                                       //  const void*                  pNext;
        0u,                                            //  VkPipelineLayoutCreateFlags  flags;
        1u,                                            //  uint32_t                     setLayoutCount;
        &m_setLayout.get(),                            //  const VkDescriptorSetLayout* pSetLayouts;
        de::sizeU32(m_pcRanges),                       //  uint32_t                     pushConstantRangeCount;
        de::dataOrNull(m_pcRanges),                    //  const VkPushConstantRange*   pPushConstantRanges;
    };
    return info;
}

void LayoutTestInstance::makeCommandsLayout(const DeviceInterface &vkd, VkDevice device,
                                            VkPipelineLayout pipelineLayout,
                                            const VkPipelineLayoutCreateInfo *createInfo)
{
    const auto pcTokenStage = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    // Note we always add the dispatch token at the end.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, m_shaderStage, pipelineLayout, createInfo);

    const bool sequenceIndex = hasSequenceIndex(m_params.testType);

    if (m_params.testType == TestType::PUSH_DISPATCH || m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH ||
        m_params.testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH)
    {
        DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);
        DE_ASSERT(m_pcTokenDataSize != 0u);
        cmdsLayoutBuilder.addPushConstantToken(0u, makePushConstantRange(pcTokenStage, 0u, m_pcTokenDataSize));

        if (sequenceIndex)
        {
            // COMPLEMENTARY_PUSH_INDEX_DISPATCH: indirect push constants + extra push constant + index.
            // In the indirect commands buffer, the sequence index placeholder comes after the other indirect push
            // constants, but the push constant offset has to take into account the complentary push constant.
            cmdsLayoutBuilder.addSequenceIndexToken(
                cmdsLayoutBuilder.getStreamRange(),
                makePushConstantRange(pcTokenStage, m_pcTokenDataSize + m_constantSize, m_constantSize));
        }
    }
    else if (m_params.testType == TestType::MULTI_PUSH_DISPATCH)
    {
        // We have 3 push constants and we'll update them in two steps: 1,2 followed by 0.
        DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);
        cmdsLayoutBuilder.addPushConstantToken(
            0u, makePushConstantRange(pcTokenStage, m_constantSize, m_constantSize * 2u));
        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(),
                                               makePushConstantRange(pcTokenStage, 0u, m_constantSize));
    }
    else if (m_params.testType == TestType::EXECUTION_SET_DISPATCH ||
             m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH)
    {
        const auto tokenOffset = (m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH ? 4u : 0u);
        if (m_params.shaderObjects)
            cmdsLayoutBuilder.addComputeShaderObjectToken(tokenOffset);
        else
            cmdsLayoutBuilder.addComputePipelineToken(tokenOffset);
    }
    else if (m_params.testType == TestType::EXECUTION_SET_PUSH_DISPATCH ||
             m_params.testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH ||
             m_params.testType == TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH)
    {
        DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);
        DE_ASSERT(m_pcTokenDataSize != 0u);

        if (m_params.shaderObjects)
            cmdsLayoutBuilder.addComputeShaderObjectToken(0u);
        else
            cmdsLayoutBuilder.addComputePipelineToken(0u);

        const auto pcOffset =
            ((m_params.testType == TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH) ? m_constantSize : 0u);

        if (sequenceIndex)
        {
            DE_ASSERT(pcOffset == 0u);
            cmdsLayoutBuilder.addSequenceIndexToken(
                cmdsLayoutBuilder.getStreamRange(),
                makePushConstantRange(pcTokenStage, pcOffset + m_pcTokenDataSize, m_constantSize));
        }

        cmdsLayoutBuilder.addPushConstantToken(cmdsLayoutBuilder.getStreamRange(),
                                               makePushConstantRange(pcTokenStage, pcOffset, m_pcTokenDataSize));
    }
    else
        DE_ASSERT(false);

    // The dispatch always comes last.
    cmdsLayoutBuilder.addDispatchToken(cmdsLayoutBuilder.getStreamRange());

    m_commandsLayout = cmdsLayoutBuilder.build(vkd, device);
    m_commandsStride = cmdsLayoutBuilder.getStreamStride();
}

std::vector<uint32_t> LayoutTestInstance::makeIndirectCommands(
    const std::vector<uint32_t> &wgCounts, const std::vector<SpecializationData> &specializationData) const
{
    DE_ASSERT(wgCounts.size() == specializationData.size());
    std::vector<uint32_t> indirectCmds;

    const auto pushDispatchIndirectCommand = [&indirectCmds](uint32_t count)
    {
        // These will be interpreted as a VkDispatchIndirectCommand: .x=count .y=1 .z=1
        indirectCmds.push_back(count);
        indirectCmds.push_back(1u);
        indirectCmds.push_back(1u);
    };

    if (m_params.testType == TestType::PUSH_DISPATCH || m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH ||
        m_params.testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH)
    {
        const bool hasIndex = (m_params.testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH);

        // 6-7 uint32_t in total per dispatch: 3 push constants, sequence index and the indirect dispatch dimensions.
        indirectCmds.reserve(wgCounts.size() * 7u);

        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &data    = specializationData.at(i);
            const auto &wgCount = wgCounts.at(i);

            indirectCmds.push_back(data.dispatchOffset);
            indirectCmds.push_back(data.skipIndex);
            indirectCmds.push_back(data.valueOffset);
            if (hasIndex)
                indirectCmds.push_back(std::numeric_limits<uint32_t>::max()); // Sequence index placeholder.
            pushDispatchIndirectCommand(wgCount);
        }
    }
    else if (m_params.testType == TestType::MULTI_PUSH_DISPATCH)
    {
        // We have 3 push constants and we'll update them in two steps: 1,2 followed by 0.
        // 6 uint32_t in total per dispatch: 3 push constants and the indirect dispatch dimensions.
        indirectCmds.reserve(wgCounts.size() * 7u);

        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &data    = specializationData.at(i);
            const auto &wgCount = wgCounts.at(i);

            indirectCmds.push_back(data.skipIndex);
            indirectCmds.push_back(data.valueOffset);
            indirectCmds.push_back(data.dispatchOffset);
            pushDispatchIndirectCommand(wgCount);
        }
    }
    else if (m_params.testType == TestType::EXECUTION_SET_DISPATCH ||
             m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH)
    {
        // We have 4 uint32_t per dispatch: 1 for the pipeline/shader index and 3 for the indirect dispatch command.
        // However, there may be one extra uint32_t when using an offset.
        if (m_params.shaderObjects)
            DE_ASSERT(m_dgcShaders.size() == wgCounts.size());
        else
            DE_ASSERT(m_dgcPipelines.size() == wgCounts.size());

        const bool withOffset   = (m_params.testType == TestType::OFFSET_EXECUTION_SET_DISPATCH);
        const uint32_t seqItems = 4u + (withOffset ? 1u : 0u);

        indirectCmds.reserve(wgCounts.size() * seqItems);
        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            if (withOffset)
                indirectCmds.push_back(std::numeric_limits<uint32_t>::max());
            indirectCmds.push_back(static_cast<uint32_t>(i));
            const auto &wgCount = wgCounts.at(i);
            pushDispatchIndirectCommand(wgCount);
        }
    }
    else if (m_params.testType == TestType::EXECUTION_SET_PUSH_DISPATCH ||
             m_params.testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH ||
             m_params.testType == TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH)
    {
        const bool hasIndex = (m_params.testType == TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH);

        // We have 6-7 uint32_t per dispatch: 1 for the pipeline index, 2 for the push constants, 1 for sequence index and
        // 3 for the indirect dispatch command.
        if (m_params.shaderObjects)
            DE_ASSERT(m_dgcShaders.size() == wgCounts.size());
        else
            DE_ASSERT(m_dgcPipelines.size() == wgCounts.size());

        indirectCmds.reserve(wgCounts.size() * 7u);
        for (size_t i = 0; i < wgCounts.size(); ++i)
        {
            const auto &wgCount = wgCounts.at(i);
            const auto &data    = specializationData.at(i);

            indirectCmds.push_back(static_cast<uint32_t>(i)); // Pipeline index.
            if (hasIndex)
                indirectCmds.push_back(std::numeric_limits<uint32_t>::max()); // Sequence index placeholder.
            indirectCmds.push_back(data.dispatchOffset);
            indirectCmds.push_back(data.skipIndex);
            // valueOffset provided as a specialization constant in the shader.
            pushDispatchIndirectCommand(wgCount);
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
    const bool hasSeqIndex  = hasSequenceIndex(m_params.testType);
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
                (prevWGs * kLocalInvocations), //   uint32_t dispatchOffset;
                static_cast<uint32_t>(rnd.getInt(0, static_cast<int>(kLocalInvocations) - 1)), //   uint32_t skipIndex;
                static_cast<uint32_t>((i + 1) << 20), //   uint32_t valueOffset;
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
    m_setLayout = setLayoutBuilder.build(ctx.vkd, ctx.device);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(outputBufferType);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *m_setLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSize);
    setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), outputBufferType,
                                 &outputBufferDescInfo);
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Create the pipelines.
    createPipelinesOrShaders(ctx.vkd, ctx.device, specializationData);
    DE_ASSERT(*m_pipelineLayout != VK_NULL_HANDLE);

    // Create the indirect execution set.
    ExecutionSetManagerPtr executionSet;
    if (hasExecutionSet(m_params.testType))
    {
        if (m_params.shaderObjects)
        {
            const std::vector<VkDescriptorSetLayout> setLayouts{*m_setLayout};
            // Initialize with the last one, then overwrite.
            const std::vector<IESStageInfo> stages{IESStageInfo(m_dgcShaders.back()->get(), setLayouts)};
            executionSet =
                makeExecutionSetManagerShader(ctx.vkd, ctx.device, stages, m_pcRanges, de::sizeU32(m_dgcShaders));
            for (size_t i = 0u; i < m_dgcShaders.size(); ++i)
                executionSet->addShader(static_cast<uint32_t>(i), m_dgcShaders.at(i)->get());
        }
        else
        {
            // Initialize with the last one, then overwrite.
            executionSet = makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, m_dgcPipelines.back()->get(),
                                                           de::sizeU32(m_dgcPipelines));
            for (size_t i = 0u; i < m_dgcPipelines.size(); ++i)
                executionSet->addPipeline(static_cast<uint32_t>(i), m_dgcPipelines.at(i)->get());
        }

        // To make things a bit more interesting, we're going to defer updating the execution set until we've allocated the
        // preprocess buffer. This means the memory requirements will have to be calculated without knowing the specific pipelines
        // or shader objects.
        //executionSet->update();
    }

    // Make the commands layout.
    {
        const auto pipelineLayoutCreateInfo = getPipelineLayoutCreateInfo();
        const auto pipelineLayout           = (m_params.dynamicPipelineLayout ? VK_NULL_HANDLE : *m_pipelineLayout);
        const auto createInfoPtr            = (m_params.dynamicPipelineLayout ? &pipelineLayoutCreateInfo : nullptr);

        makeCommandsLayout(ctx.vkd, ctx.device, pipelineLayout, createInfoPtr);
    }

    // Create indirect commands buffer contents.
    const auto indirectCommands = makeIndirectCommands(wgCounts, specializationData);

    // Create a host-visible buffer to store them.
    const auto indirectCmdsBufferSize = static_cast<VkDeviceSize>(de::dataSize(indirectCommands));
    DGCBuffer indirectCmdsBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectCmdsBufferSize);
    auto &indirectCmdsBufferAlloc = indirectCmdsBuffer.getAllocation();
    void *indirectCmdsBufferData  = indirectCmdsBufferAlloc.getHostPtr();

    deMemcpy(indirectCmdsBufferData, de::dataOrNull(indirectCommands), de::dataSize(indirectCommands));
    flushAlloc(ctx.vkd, ctx.device, indirectCmdsBufferAlloc);

    // Create a preprocess buffer. Note we need requireNoPendingWrites=false because we're still missing the update() call.
    const VkIndirectExecutionSetEXT executionSetHandle =
        (executionSet ? executionSet->get(/*requireNoPendingWrites=*/false) : VK_NULL_HANDLE);
    const std::vector<VkShaderEXT> shaderVec{*m_singleShader};
    const std::vector<VkShaderEXT> *shaderVecPtr = (*m_singleShader != VK_NULL_HANDLE ? &shaderVec : nullptr);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, executionSetHandle, *m_commandsLayout,
                                         de::sizeU32(wgCounts), 0u,
                                         *m_singlePipeline, // This will be VK_NULL_HANDLE when appropriate.
                                         shaderVecPtr);

    // Now update the indirect execution set. See above for the reason to wait a bit to do this.
    if (executionSet)
        executionSet->update();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, m_bindPoint, *m_pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);

    // Bind or prepare pipelines.
    if (*m_singlePipeline != VK_NULL_HANDLE)
        ctx.vkd.cmdBindPipeline(cmdBuffer, m_bindPoint, *m_singlePipeline);
    else if (!m_dgcPipelines.empty())
        ctx.vkd.cmdBindPipeline(cmdBuffer, m_bindPoint, m_dgcPipelines.at(0u)->get());
    else if (*m_singleShader != VK_NULL_HANDLE)
        ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &m_shaderStageBit, &m_singleShader.get());
    else if (!m_dgcShaders.empty())
        ctx.vkd.cmdBindShadersEXT(cmdBuffer, 1u, &m_shaderStageBit, &m_dgcShaders.at(0u)->get());
    else
        DE_ASSERT(false);

    if (extraPush)
    {
        // Must match the offset and size of valueOffset2 in the shaders.
        const auto noIES    = (m_params.testType == TestType::COMPLEMENTARY_PUSH_DISPATCH ||
                            m_params.testType == TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH);
        const auto pcOffset = (noIES ? m_pcTokenDataSize : 0u);
        ctx.vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, m_shaderStage, pcOffset, m_constantSize, &valueOffset2);
    }

    // Execute indirect commands.
    DGCGenCmdsInfo generatedCommandsInfo(
        m_shaderStage,                         //   VkShaderStageFlags          shaderStages;
        executionSetHandle,                    //   VkIndirectExecutionSetEXT   indirectExecutionSet;
        *m_commandsLayout,                     //   VkIndirectCommandsLayoutEXT indirectCommandsLayout;
        indirectCmdsBuffer.getDeviceAddress(), //   VkDeviceAddress             indirectAddress;
        indirectCmdsBufferSize,                //   VkDeviceSize                indirectAddressSize;
        preprocessBuffer.getDeviceAddress(),   //   VkDeviceAddress             preprocessAddress;
        preprocessBuffer.getSize(),            //   VkDeviceSize                preprocessSize;
        de::sizeU32(wgCounts),                 //   uint32_t                    maxSequenceCount;
        0ull,                                  //   VkDeviceAddress             sequenceCountAddress;
        0u,                                    //   uint32_t                    maxDrawCount;
        *m_singlePipeline, shaderVecPtr);
    ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &generatedCommandsInfo.get());

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
                    ((invocationIdx == skipIndex) ? 0u :
                                                    (valueOffset + (wgIdx << 10) + invocationIdx + valueOffset2 +
                                                     (hasSeqIndex ? static_cast<uint32_t>(dispatchIdx) : 0u)));
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

tcu::TestCaseGroup *createDGCComputeLayoutTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "layout"));

    const struct
    {
        TestType testType;
        const char *name;
    } testTypesTable[] = {
        {TestType::PUSH_DISPATCH, "push_dispatch"},
        {TestType::COMPLEMENTARY_PUSH_DISPATCH, "complementary_push_dispatch"},
        {TestType::COMPLEMENTARY_PUSH_INDEX_DISPATCH, "complementary_push_index_dispatch"},
        {TestType::MULTI_PUSH_DISPATCH, "multi_push_dispatch"},
        {TestType::OFFSET_EXECUTION_SET_DISPATCH, "offset_execution_set_dispatch"},
        {TestType::EXECUTION_SET_DISPATCH, "execution_set_dispatch"},
        {TestType::EXECUTION_SET_PUSH_DISPATCH, "execution_set_push_dispatch"},
        {TestType::EXECUTION_SET_INDEX_PUSH_DISPATCH, "execution_set_index_push_dispatch"},
        {TestType::EXECUTION_SET_COMPLEMENTARY_PUSH_DISPATCH, "execution_set_complementary_push_dispatch"},
    };

    for (const auto useComputeQueue : {false, true})
        for (const auto useShaderObjects : {false, true})
            for (const auto dynamicPipelineLayout : {false, true})
                for (const auto &testCase : testTypesTable)
                {
                    TestParams params(testCase.testType, useShaderObjects, useComputeQueue, dynamicPipelineLayout);
                    const auto testName = std::string(testCase.name) + (useShaderObjects ? "_shader_objects" : "") +
                                          (useComputeQueue ? "_cq" : "") +
                                          (dynamicPipelineLayout ? "_dynamic_pipeline_layout" : "");
                    mainGroup->addChild(new LayoutTestCase(testCtx, testName, params));
                }

    return mainGroup.release();
}
} // namespace DGC
} // namespace vkt
