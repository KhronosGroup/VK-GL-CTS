/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 LunarG, Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Independent Sets Utils
 *//*--------------------------------------------------------------------*/

#include "vktIndependentSetsUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuVectorUtil.hpp"

#include "deRandom.hpp"

#include <memory>

namespace vkt
{
namespace IndependentSets
{

#ifndef CTS_USES_VULKANSC

using namespace vk;

constexpr VkFormat imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

using IndexStageMap = std::map<uint32_t, VkShaderStageFlagBits>;

// Gets the top index from a stage index map.
uint32_t getTopIndex(const StageIndexMap &map)
{
    uint32_t top = 0u;

    for (const auto &stageIndex : map)
    {
        if (stageIndex.second > top)
            top = stageIndex.second;
    }

    return top;
}

// Reverses roles in a StageIndexMap, making it an IndexStageMap.
IndexStageMap reverseStageIndexMap(const StageIndexMap &inputMap)
{
    IndexStageMap map;
    for (const auto &stageIndex : inputMap)
        map[stageIndex.second] = stageIndex.first;
    return map;
}

// Returns true if the descriptor can be written to.
bool canWriteTo(VkDescriptorType descriptorType)
{
    return (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
            descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
            descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
            descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}

// General information for a descriptor on a set.
DescriptorInfo::DescriptorInfo(VkDescriptorType descriptorType_, uint32_t count_, uint32_t attIndex_,
                               uint32_t offsetFactor_, bool isArray_, bool write_)
    : descryptorType(descriptorType_)
    , count(count_)
    , attIndex(attIndex_)
    , offsetFactor(offsetFactor_)
    , isArray(isArray_)
    , write(write_)
{
}

StrideAndOffset DescriptorInfo::getStrideAndOffset(const vk::VkPhysicalDeviceLimits &limits) const
{
    VkDeviceSize stride = static_cast<VkDeviceSize>(sizeof(tcu::Vec4)); // Base stride.

    // Modify stride for dynamic buffers.
    if (descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        stride = std::max(stride, limits.minUniformBufferOffsetAlignment);
    else if (descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        stride = std::max(stride, limits.minStorageBufferOffsetAlignment);

    VkDeviceSize offset = stride * static_cast<VkDeviceSize>(offsetFactor);

    // Buffer size will implicitly be stride + offset.
    return StrideAndOffset(stride, offset);
}

SetInfo::SetInfo(bool isPush_) : descriptors(), isPush(isPush_)
{
}

bool needsIndependentSampler(const std::vector<DescriptorInfo> &descriptors)
{
    for (const auto &descriptorInfo : descriptors)
    {
        if (descriptorInfo.descryptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
            return true;
    }
    return false;
}

// Each set may contain up to a few descriptors, and the last binding may be an array of up to a few descriptors itself.
// In the IO buffer, values for all those descriptors are stored in chunks per set. If the last binding is an array,
// those values are stored separately from the non-arrayed bindings. If any binding needs a separate sampler, the
// separate sampler becomes the first binding in the set.
constexpr int maxDescriptorsPerSet   = 9;
constexpr int firstArrayValue        = 10;
constexpr int maxDescriptorArraySize = 10;
constexpr int valuesPerSet           = firstArrayValue + maxDescriptorArraySize; // 20

// Returns true if the stage is included in `stages`.
bool Params::hasStage(VkShaderStageFlagBits stage) const
{
    return ((stages & stage) != 0u);
}

std::string Params::getStageNames() const
{
    static const std::map<VkShaderStageFlagBits, std::string> nameMap{
        std::make_pair(VK_SHADER_STAGE_VERTEX_BIT, "vert"),
        std::make_pair(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc"),
        std::make_pair(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese"),
        std::make_pair(VK_SHADER_STAGE_GEOMETRY_BIT, "geom"),
        std::make_pair(VK_SHADER_STAGE_FRAGMENT_BIT, "frag"),
        std::make_pair(VK_SHADER_STAGE_TASK_BIT_EXT, "task"),
        std::make_pair(VK_SHADER_STAGE_MESH_BIT_EXT, "mesh"),
    };

    std::string stageNames;

    const auto checkAndExtendName = [&](VkShaderStageFlagBits stage)
    {
        if (hasStage(stage))
            stageNames += (stageNames.empty() ? "" : "_") + nameMap.at(stage);
    };

    if (hasStage(VK_SHADER_STAGE_VERTEX_BIT))
    {
        stageNames += nameMap.at(VK_SHADER_STAGE_VERTEX_BIT);
        checkAndExtendName(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        checkAndExtendName(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        checkAndExtendName(VK_SHADER_STAGE_GEOMETRY_BIT);
    }
    else
    {
        checkAndExtendName(VK_SHADER_STAGE_TASK_BIT_EXT);
        checkAndExtendName(VK_SHADER_STAGE_MESH_BIT_EXT);
    }
    checkAndExtendName(VK_SHADER_STAGE_FRAGMENT_BIT);

    return stageNames;
}

// The sets associated to each shader stage may be offsetted by 1 when the first set is used for the IO buffer.
uint32_t Params::getSetIndexOffset() const
{
    return (ioFirst ? 1u : 0u);
}

// Assign a set index to each shader stage, according to the contents of `stages`.
StageIndexMap Params::getSetIndexMap() const
{
    StageIndexMap map;
    uint32_t nextIndex = getSetIndexOffset();

    const auto checkAssignNext = [&](VkShaderStageFlagBits stage)
    {
        if (hasStage(stage))
            map[stage] = nextIndex++;
    };

    if (hasStage(VK_SHADER_STAGE_MESH_BIT_EXT))
    {
        checkAssignNext(VK_SHADER_STAGE_TASK_BIT_EXT);
        map[VK_SHADER_STAGE_MESH_BIT_EXT] = nextIndex++;
    }
    else
    {
        DE_ASSERT(hasStage(VK_SHADER_STAGE_VERTEX_BIT));
        map[VK_SHADER_STAGE_VERTEX_BIT] = nextIndex++;
        checkAssignNext(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        checkAssignNext(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        checkAssignNext(VK_SHADER_STAGE_GEOMETRY_BIT);
    }
    checkAssignNext(VK_SHADER_STAGE_FRAGMENT_BIT);

    return map;
}

// Generate pseudorandom descriptor set information according to the current parameters. We make it part of the
// parameters to be able to use this information both in the Test Case as well as the Test Instance, guaranteeing
// we will obtain the same result in both calls.
std::vector<SetInfo> Params::genSetInfos() const
{
    static const std::vector<VkDescriptorType> descryptorTypeChoices{
        // clang-format off
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        // clang-format on
    };

    // These limits are at or below the minimums required by the spec.
    // Limiting this number of descriptors makes sure everybody can run most test combinations.
    static constexpr uint32_t maxDynamicUniformBuffers = 4u;
    static constexpr uint32_t maxDynamicStorageBuffers = 4u;
    static constexpr uint32_t maxInputAttachments      = 4u;
    uint32_t dynamicUniformBufferCount                 = 0u;
    uint32_t dynamicStorageBufferCount                 = 0u;
    uint32_t inputAttachmentCount                      = 0u;

    std::vector<SetInfo> setInfos;

    const auto setIndexOffset = getSetIndexOffset();
    const auto setIndexMap    = getSetIndexMap();
    const auto indexStageMap  = reverseStageIndexMap(setIndexMap);
    const auto topIndex       = getTopIndex(setIndexMap);
    const auto seed           = getSeed();

    de::Random rnd(seed);
    setInfos.reserve(topIndex + 1u);
    uint32_t globalAttIndex = 0u;

    for (uint32_t i = setIndexOffset; i <= topIndex; ++i)
    {
        const bool isPush = (i == topIndex && rnd.getBool());
        setInfos.emplace_back(isPush);
        auto &setInfo = setInfos.back();

        const auto stage          = indexStageMap.at(i);
        const int descriptorCount = rnd.getInt(1, maxDescriptorsPerSet);
        setInfo.descriptors.reserve(static_cast<size_t>(descriptorCount));

        for (int j = 0; j < descriptorCount; ++j)
        {
            VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            for (;;)
            {
                descriptorType =
                    rnd.choose<VkDescriptorType>(descryptorTypeChoices.begin(), descryptorTypeChoices.end());
                if (isPush && (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                               descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
                    continue;
                if (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT && stage != VK_SHADER_STAGE_FRAGMENT_BIT)
                    continue;
                if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                {
                    if (dynamicUniformBufferCount >= maxDynamicUniformBuffers)
                        continue;
                }
                else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                {
                    if (dynamicStorageBufferCount >= maxDynamicStorageBuffers)
                        continue;
                }
                else if (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                {
                    if (inputAttachmentCount >= maxInputAttachments)
                        continue;
                }
                break;
            }

            const bool isAtt   = (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
            const bool isArray = (!isAtt && j == descriptorCount - 1 && rnd.getBool());

            int maxArraySize = maxDescriptorArraySize;
            if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                maxArraySize = static_cast<int>(maxDynamicUniformBuffers - dynamicUniformBufferCount);
            else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                maxArraySize = static_cast<int>(maxDynamicStorageBuffers - dynamicStorageBufferCount);
            else if (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                maxArraySize = static_cast<int>(maxInputAttachments - inputAttachmentCount);

            const uint32_t count = (isArray ? static_cast<uint32_t>(rnd.getInt(1, maxArraySize)) : 1u);
            const bool write     = (canWriteTo(descriptorType) && rnd.getBool());
            const auto attIndex  = (isAtt ? globalAttIndex++ : 0u);

            if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                dynamicUniformBufferCount += count;
            else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                dynamicStorageBufferCount += count;
            else if (descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                inputAttachmentCount += count;

            uint32_t offsetFactor = 0u;
            if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                offsetFactor = static_cast<uint32_t>(rnd.getInt(1, 10));
            setInfo.descriptors.emplace_back(descriptorType, count, attIndex, offsetFactor, isArray, write);
        }

        // If we need an independent sampler for some of the descriptors, insert it as the first descriptor.
        if (needsIndependentSampler(setInfo.descriptors))
            setInfo.descriptors.insert(setInfo.descriptors.begin(),
                                       DescriptorInfo(VK_DESCRIPTOR_TYPE_SAMPLER, 1u, 0u, 0u, false, false));
    }

    return setInfos;
}

// Obtain a seed for the RNG.
uint32_t Params::getSeed() const
{
    return ((stages << 16) | (caseIndex & 0xFFFFu));
}

Instance::Instance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
{
}

Case::Case(tcu::TestContext &testCtx, const std::string &name, const Params &params)
    : vkt::TestCase(testCtx, name)
    , m_params(params)
{
}

TestInstance *Case::createInstance(Context &context) const
{
    return new Instance(context, m_params);
}

void Case::checkSupport(Context &context) const
{
    const bool isESO = isConstructionTypeShaderObject(m_params.constructionType);

    // Require maintenance11 unless skipped.
    if (isESO && !m_params.avoidMaintenance11)
        context.requireDeviceFunctionality("VK_KHR_maintenance11");

    // This is needed to create the "general" pipeline layout with VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT.
    // Monolithic cases and ESO cases without maintenance11 will not use the flag.
    if (!(m_params.constructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
          (isESO && !m_params.avoidMaintenance11)))
        context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);

    if (m_params.hasStage(VK_SHADER_STAGE_MESH_BIT_EXT))
        context.requireDeviceFunctionality("VK_EXT_mesh_shader");

    if (m_params.hasStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
        m_params.hasStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    if (m_params.hasStage(VK_SHADER_STAGE_GEOMETRY_BIT))
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

    const auto setIndexMap = m_params.getSetIndexMap();
    const auto topIndex    = getTopIndex(setIndexMap);
    const auto setCount    = ((topIndex + 1) + 1 /*for the IO buffer that will be used*/);

    const auto &properties = context.getDeviceProperties();
    if (setCount > properties.limits.maxBoundDescriptorSets)
        TCU_THROW(NotSupportedError, "maxBoundDescriptorSets too low");

    const auto setInfos = m_params.genSetInfos();
    if (setInfos.back().isPush)
        context.requireDeviceFunctionality("VK_KHR_push_descriptor");

    bool hasInputAttachments = false;
    for (const auto &setInfo : setInfos)
        for (const auto &descInfo : setInfo.descriptors)
        {
            if (descInfo.descryptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            {
                hasInputAttachments = true;
                goto input_attachments_check_end;
            }
        }
input_attachments_check_end:
    if (hasInputAttachments && isESO) // If not using ESO, classic render passes will be used.
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
    if (m_params.hasStage(VK_SHADER_STAGE_FRAGMENT_BIT))
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
}

// Calculates the position of a given item in the IO buffer.
uint32_t getIOIndex(uint32_t setInfoIndex, uint32_t binding, bool isArray, uint32_t arrayIndex)
{
    return (valuesPerSet * setInfoIndex + (isArray ? firstArrayValue : binding) + arrayIndex);
}

std::string getDescriptorName(uint32_t setIndex, uint32_t binding, VkDescriptorType descType)
{
    if (descType == VK_DESCRIPTOR_TYPE_SAMPLER)
        return "separateSampler";
    return "desc_" + std::to_string(setIndex) + "_" + std::to_string(binding);
}

std::string getDescriptorDecls(uint32_t setIndex, const std::vector<DescriptorInfo> &descriptors)
{
    std::ostringstream decls;

    for (size_t i = 0u; i < descriptors.size(); ++i)
    {
        const auto &desc = descriptors.at(i);
        decls << "layout (set=" << setIndex << ", binding=" << i;
        if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
            desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            decls << ", rgba32f";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            decls << ", input_attachment_index=" << desc.attIndex;
        decls << ") ";

        if (desc.descryptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
            decls << "uniform sampler";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            decls << "uniform sampler2D";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
            decls << "uniform texture2D";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            decls << "uniform image2D";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
            decls << "uniform textureBuffer";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
            decls << "uniform imageBuffer";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                 desc.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            decls << "uniform UBO_Block_" << setIndex << "_" << i << " { vec4 value; }";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                 desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            decls << "buffer SSBO_Block_" << setIndex << "_" << i << " { vec4 value; }";
        else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            decls << "uniform subpassInput";
        else
            DE_ASSERT(false);

        decls << " " << getDescriptorName(setIndex, static_cast<uint32_t>(i), desc.descryptorType)
              << (desc.isArray ? "[]" : "") << ";\n";
    }

    return decls.str();
}

std::string getDescriptorStatements(uint32_t setIndex, uint32_t setIndexOffset,
                                    const std::vector<DescriptorInfo> &descriptors, const std::string &ioBufferName)
{
    const auto setInfoIndex = setIndex - setIndexOffset;
    std::ostringstream stmts;

    for (uint32_t binding = 0u; binding < de::sizeU32(descriptors); ++binding)
    {
        const auto &desc        = descriptors.at(binding);
        const auto descBaseName = getDescriptorName(setIndex, binding, desc.descryptorType);

        for (uint32_t arrayIndex = 0u; arrayIndex < desc.count; ++arrayIndex)
        {
            // Position of the value associated to this descriptor in the global IO buffer.
            const auto ioIndex     = getIOIndex(setInfoIndex, binding, desc.isArray, arrayIndex);
            const auto valueAccess = ioBufferName + ".values[" + std::to_string(ioIndex) + "]";
            const auto descName    = descBaseName + (desc.isArray ? "[" + std::to_string(arrayIndex) + "]" : "");

            stmts << "        ";

            if (desc.descryptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                ; // Samplers will be combined with sampled images but nothing else.
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                stmts << valueAccess << " = texelFetch(" << descName << ", ivec2(0, 0), 0)";
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
            {
                const auto combinedDesc = "sampler2D(" + descName + ", separateSampler)";
                stmts << valueAccess << " = texelFetch(" << combinedDesc << ", ivec2(0, 0), 0)";
            }
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
                if (desc.write)
                    stmts << "imageStore(" << descName << ", ivec2(0, 0), " << valueAccess << ")";
                else
                    stmts << valueAccess << " = imageLoad(" << descName << ", ivec2(0, 0))";
            }
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                stmts << valueAccess << " = texelFetch(" << descName << ", 0)";
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
            {
                if (desc.write)
                    stmts << "imageStore(" << descName << ", 0, " << valueAccess << ")";
                else
                    stmts << valueAccess << " = imageLoad(" << descName << ", 0)";
            }
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                     desc.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                stmts << valueAccess << " = " << descName << ".value";
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                     desc.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            {
                if (desc.write)
                    stmts << descName << ".value = " << valueAccess;
                else
                    stmts << valueAccess << " = " << descName << ".value";
            }
            else if (desc.descryptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
            {
                stmts << valueAccess << " = subpassLoad(" << descName << ")";
            }
            else
                DE_ASSERT(false);

            stmts << ";\n";
        }
    }

    return stmts.str();
}

void Case::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto setInfos       = m_params.genSetInfos();
    const auto setIndexMap    = m_params.getSetIndexMap();
    const auto topIndex       = getTopIndex(setIndexMap);
    const auto setIndexOffset = m_params.getSetIndexOffset();
    const auto ioBufferName = std::string("io_ssbo"); // Contains input or output values for the different descriptors.
    const auto ioBufferSetIndex = (m_params.ioFirst ? 0u : topIndex + 1u);
    const auto ioBufferDecl     = "layout (set=" + std::to_string(ioBufferSetIndex) +
                              ", binding=0) buffer ValuesBlock { vec4 values[]; } " + ioBufferName + ";\n";

    if (m_params.hasStage(VK_SHADER_STAGE_VERTEX_BIT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_VERTEX_BIT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::ostringstream vert;
        vert << "#version 460\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl
             << "layout (location=0) in vec4 inPos;\n"
             << "void main(void) {\n"
             << "    gl_Position = inPos;\n"
             << "    if (gl_VertexIndex == 0) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    }

    if (m_params.hasStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::ostringstream tesc;
        tesc << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "layout(vertices=3) out;\n"
             << "in gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex\n"
             << "{\n"
             << "    vec4 gl_Position;\n"
             << "} gl_out[];\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl << "void main() {\n"
             << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
             << "    gl_TessLevelOuter[0] = 1.0;\n"
             << "    gl_TessLevelOuter[1] = 1.0;\n"
             << "    gl_TessLevelOuter[2] = 1.0;\n"
             << "    gl_TessLevelOuter[3] = 1.0;\n"
             << "    gl_TessLevelInner[0] = 1.0;\n"
             << "    gl_TessLevelInner[1] = 1.0;\n"
             << "    if (gl_InvocationID == 0 && gl_PrimitiveID == 0) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "}\n";
        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
    }

    if (m_params.hasStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::ostringstream tese;
        tese << "#version 460\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "layout(triangles) in;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[gl_MaxPatchVertices];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl << "void main() {\n"
             << "    gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x + \n"
             << "                   gl_in[1].gl_Position * gl_TessCoord.y + \n"
             << "                   gl_in[2].gl_Position * gl_TessCoord.z);\n"
             << "    if (gl_PrimitiveID == 0 && gl_TessCoord.x == 1.0) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }

    if (m_params.hasStage(VK_SHADER_STAGE_GEOMETRY_BIT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_GEOMETRY_BIT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::ostringstream geom;
        geom << "#version 460\n"
             << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=3) out;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[3];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl << "void main() {\n"
             << "    for (uint i = 0; i < 3; ++i) {\n"
             << "        gl_Position = gl_in[i].gl_Position;\n"
             << "        EmitVertex();\n"
             << "    }\n"
             << "    if (gl_PrimitiveIDIn == 0) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }

    if (m_params.hasStage(VK_SHADER_STAGE_FRAGMENT_BIT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_FRAGMENT_BIT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::ostringstream frag;
        frag << "#version 460\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl << "void main() {\n"
             << "    const ivec2 pixCoord = ivec2(gl_FragCoord.xy);\n"
             << "    if (pixCoord == ivec2(0, 0)) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }

    const ShaderBuildOptions taskMeshBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

    if (m_params.hasStage(VK_SHADER_STAGE_TASK_BIT_EXT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_TASK_BIT_EXT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::stringstream task;
        task << "#version 460\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl << "void main()\n"
             << "{\n"
             << "    if (gl_LocalInvocationIndex == 0) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "    EmitMeshTasksEXT(1, 1, 1);\n"
             << "}\n";
        programCollection.glslSources.add("task") << glu::TaskSource(task.str()) << taskMeshBuildOptions;
    }

    if (m_params.hasStage(VK_SHADER_STAGE_MESH_BIT_EXT))
    {
        const auto setIndex = setIndexMap.at(VK_SHADER_STAGE_MESH_BIT_EXT);
        const auto &setInfo = setInfos.at(setIndex - setIndexOffset);

        std::stringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout(triangles) out;\n"
             << "layout(max_vertices=4, max_primitives=2) out;\n"
             << getDescriptorDecls(setIndex, setInfo.descriptors) << ioBufferDecl << "void main()\n"
             << "{\n"
             << "    SetMeshOutputsEXT(4, 2);\n"
             << "    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
             << "    gl_MeshVerticesEXT[1].gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
             << "    gl_MeshVerticesEXT[2].gl_Position = vec4( 1.0, -1.0, 0.0, 1.0);\n"
             << "    gl_MeshVerticesEXT[3].gl_Position = vec4( 1.0,  1.0, 0.0, 1.0);\n"
             << "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
             << "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(2, 1, 3);\n"
             << "    if (gl_LocalInvocationIndex == 0) {\n"
             << getDescriptorStatements(setIndex, setIndexOffset, setInfo.descriptors, ioBufferName) << "    }\n"
             << "}\n";
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << taskMeshBuildOptions;
    }
}

struct ResourceLocation
{
    ResourceLocation(uint32_t setIndex_, uint32_t binding_, uint32_t arrayIndex_, bool isArray_)
        : setIndex(setIndex_)
        , binding(binding_)
        , arrayIndex(arrayIndex_)
        , isArray(isArray_)
    {
    }

    uint32_t setIndex;
    uint32_t binding;
    uint32_t arrayIndex;
    bool isArray;

    bool operator<(const ResourceLocation &other) const
    {
        if (setIndex != other.setIndex)
            return (setIndex < other.setIndex);
        else if (binding != other.binding)
            return (binding < other.binding);
        else if (arrayIndex != other.arrayIndex)
            return (arrayIndex < other.arrayIndex);
        return isArray < other.isArray;
    }
};

// Using shared pointers inside ResourceInfo makes it possible to use the class with std::map, as we do below.
// std::unique_ptr, and directly using Move objects, removes the const copy constructor from ResourceInfo and causes
// issues with std::pair as used for map values.
using BufferWithMemoryPtr = std::shared_ptr<BufferWithMemory>;
using ImageWithMemoryPtr  = std::shared_ptr<ImageWithBuffer>;

// Holds buffers or images.
class ResourceInfo
{
protected:
    enum class ResourceType
    {
        NONE   = 0,
        BUFFER = 1,
        IMAGE  = 2
    };

    ResourceType resourceType;
    union
    {
        BufferWithMemoryPtr buffer;
        ImageWithMemoryPtr image;
    };
    uint32_t offset; // In practice, only used for dynamic buffers.
    std::shared_ptr<Move<VkBufferView>> bufferViewPtr;

    void clear()
    {
        if (resourceType == ResourceType::BUFFER)
        {
            bufferViewPtr.reset(new Move<VkBufferView>());
            buffer.reset();
        }
        else if (resourceType == ResourceType::IMAGE)
            image.reset();
        offset       = 0u;
        resourceType = ResourceType::NONE;
    }

public:
    ResourceInfo() : resourceType(ResourceType::NONE), buffer(), offset(0u), bufferViewPtr()
    {
    }

    ~ResourceInfo()
    {
        clear();
    }

    ResourceInfo &operator=(BufferWithMemory *bufferPtr)
    {
        clear();
        if (bufferPtr != nullptr)
        {
            resourceType = ResourceType::BUFFER;
            buffer.reset(bufferPtr);
        }
        return *this;
    }

    ResourceInfo &operator=(ImageWithBuffer *imagePtr)
    {
        clear();
        if (imagePtr != nullptr)
        {
            resourceType = ResourceType::IMAGE;
            image.reset(imagePtr);
        }
        return *this;
    }

    bool hasBuffer() const
    {
        return (resourceType == ResourceType::BUFFER);
    }

    bool hasImage() const
    {
        return (resourceType == ResourceType::IMAGE);
    }

    BufferWithMemory &getBuffer() const
    {
        if (!hasBuffer())
            TCU_THROW(InternalError, "ResourceInfo: getting buffer but resource does not have a buffer");
        return *buffer.get();
    }

    ImageWithBuffer &getImage() const
    {
        if (!hasImage())
            TCU_THROW(InternalError, "ResourceInfo: getting image but resource does not have an image");
        return *image.get();
    }

    void makeBufferView(const DeviceInterface &vkd, VkDevice device, VkFormat format)
    {
        if (!hasBuffer())
            TCU_THROW(InternalError, "ResourceInfo: creating buffer view but resource does not have a buffer");
        // Note vk:: namespace prefix in call to avoid compiler errors.
        bufferViewPtr.reset(
            new Move<VkBufferView>(vk::makeBufferView(vkd, device, buffer->get(), format, 0ull, VK_WHOLE_SIZE)));
    }

    const VkBufferView &getBufferView() const
    {
        if (!hasBuffer())
            TCU_THROW(InternalError, "ResourceInfo: getting buffer view but resource does not have a buffer");
        return bufferViewPtr->get();
    }

    void setOffset(uint32_t offset_)
    {
        offset = offset_;
    }

    uint32_t getOffset() const
    {
        return offset;
    }
};

VkImageUsageFlags getImageUsageFlags(VkDescriptorType descriptorType)
{
    VkImageUsageFlags usageFlags = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        usageFlags |= (VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    return usageFlags;
}

VkBufferUsageFlags getBufferUsageFlags(VkDescriptorType descriptorType)
{
    VkImageUsageFlags usageFlags = 0u;

    switch (descriptorType)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        usageFlags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    return usageFlags;
}

// Converts shader stage to pipeline stage.
VkPipelineStageFlagBits getPipelineStage(VkShaderStageFlagBits shaderStage)
{
    switch (shaderStage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        return VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        return VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    default:
        break;
    }

    DE_ASSERT(false);
    return VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;
}

tcu::TestStatus Instance::iterate()
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 extent(1, 1, 1);
    const auto extentVk    = makeExtent3D(extent);
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto colorSRR    = makeDefaultImageSubresourceRange();
    const auto colorSRL    = makeDefaultImageSubresourceLayers();
    const auto copyRegion  = makeBufferImageCopy(extentVk, colorSRL);
    const auto sampleCount = VK_SAMPLE_COUNT_1_BIT;
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;

    const auto isESO          = isConstructionTypeShaderObject(m_params.constructionType);
    const auto isLib          = isConstructionTypeLibrary(m_params.constructionType);
    const auto isFastLib      = (m_params.constructionType == PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY);
    const auto setInfos       = m_params.genSetInfos();
    const auto stageIndexMap  = m_params.getSetIndexMap();
    const auto indexStageMap  = reverseStageIndexMap(stageIndexMap);
    const auto totalSetCount  = de::sizeU32(setInfos) + 1u; // For the IO buffer set.
    const auto setIndexOffset = m_params.getSetIndexOffset();
    const auto inputAttLayout =
        (isESO ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    using DescriptorSetLayoutPtr = std::unique_ptr<Move<VkDescriptorSetLayout>>;

    // Descriptor pool and set layouts.
    DescriptorPoolBuilder poolBuilder;
    std::vector<DescriptorSetLayoutPtr> setLayouts;
    setLayouts.reserve(totalSetCount);

    bool hasPushDescriptors = false;

    // For the IO bufer set.
    const auto makeIOSetLayout = [&]()
    {
        const auto descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolBuilder.addType(descType);

        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(descType, m_params.stages); // Used in all stages.
        setLayouts.emplace_back(new Move<VkDescriptorSetLayout>(setLayoutBuilder.build(ctx.vkd, ctx.device)));
    };

    if (m_params.ioFirst)
        makeIOSetLayout();
    for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
    {
        DescriptorSetLayoutBuilder setLayoutBuilder;

        const auto setIndex   = infoIndex + setIndexOffset;
        const auto &setInfo   = setInfos.at(infoIndex);
        const auto stageFlags = static_cast<VkShaderStageFlags>(indexStageMap.at(setIndex));

        for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
        {
            const auto &descInfo = setInfo.descriptors.at(bindingIndex);
            poolBuilder.addType(descInfo.descryptorType, descInfo.count);
            if (descInfo.isArray)
                setLayoutBuilder.addArrayBinding(descInfo.descryptorType, descInfo.count, stageFlags);
            else
                setLayoutBuilder.addSingleBinding(descInfo.descryptorType, stageFlags);
        }

        VkDescriptorSetLayoutCreateFlags createFlags = 0u;
        if (setInfo.isPush)
        {
            hasPushDescriptors = true;
            createFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        }
        setLayouts.emplace_back(
            new Move<VkDescriptorSetLayout>(setLayoutBuilder.build(ctx.vkd, ctx.device, createFlags)));
    }
    if (!m_params.ioFirst)
        makeIOSetLayout();
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, totalSetCount);

    // Allocate descriptor sets based on those layouts.
    using DescriptorSetPtr = std::unique_ptr<Move<VkDescriptorSet>>;
    std::vector<DescriptorSetPtr> descriptorSets;
    descriptorSets.reserve(setLayouts.size());
    for (uint32_t i = 0u; i < de::sizeU32(setLayouts); ++i)
    {
        const auto infoIndex = i - setIndexOffset;
        // Push descriptor sets are not allocated and do not get a VkDescriptorSet handle.
        if (infoIndex < de::sizeU32(setInfos) && setInfos.at(infoIndex).isPush)
            descriptorSets.emplace_back(new Move<VkDescriptorSet>());
        else
            descriptorSets.emplace_back(new Move<VkDescriptorSet>(
                makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, setLayouts.at(i)->get())));
    }

    // Allocate resources, indexing them by set, binding and array element.
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    const auto ioItemCount        = static_cast<uint32_t>(valuesPerSet) * de::sizeU32(setInfos);
    const auto ioBufferSize       = static_cast<VkDeviceSize>(ioItemCount * DE_SIZEOF32(tcu::Vec4));
    const auto ioBufferCreateInfo = makeBufferCreateInfo(ioBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory ioBuffer(ctx.vkd, ctx.device, ctx.allocator, ioBufferCreateInfo, HostIntent::RW);
    // We will populate the contents later.

    // Other resources in descriptor sets.
    const auto &deviceLimits = m_context.getDeviceProperties().limits;
    std::map<ResourceLocation, ResourceInfo> resources;
    for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
    {
        const auto setIndex = infoIndex + setIndexOffset;
        const auto &setInfo = setInfos.at(infoIndex);

        for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
        {
            const auto &descInfo = setInfo.descriptors.at(bindingIndex);
            for (uint32_t arrayIndex = 0u; arrayIndex < descInfo.count; ++arrayIndex)
            {
                const auto resLoc = ResourceLocation(setIndex, bindingIndex, arrayIndex, descInfo.isArray);

                switch (descInfo.descryptorType)
                {
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                {
                    const auto usageFlags = getImageUsageFlags(descInfo.descryptorType);
                    resources[resLoc] =
                        new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, imageFormat, usageFlags,
                                            imageType, makeDefaultImageSubresourceRange(), 1u, sampleCount,
                                            VK_IMAGE_TILING_OPTIMAL, 1u, VK_SHARING_MODE_EXCLUSIVE, HostIntent::RW);
                }
                break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                {
                    const auto strideAndOffset  = descInfo.getStrideAndOffset(deviceLimits);
                    const auto bufferSize       = strideAndOffset.first + strideAndOffset.second;
                    const auto usageFlags       = getBufferUsageFlags(descInfo.descryptorType);
                    const auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, usageFlags);

                    resources[resLoc] =
                        new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferCreateInfo, HostIntent::RW);

                    if (descInfo.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
                        descInfo.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
                    {
                        resources[resLoc].makeBufferView(ctx.vkd, ctx.device, imageFormat);
                    }

                    // Store offset with the resource information, which makes things easier later.
                    resources[resLoc].setOffset(static_cast<uint32_t>(strideAndOffset.second));
                }
                break;
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    break;
                default:
                    DE_ASSERT(false);
                    break;
                }
            }
        }
    }

    // Fill contents of the different buffers: if the resource is going to be written to, we need to set a value in the
    // IO buffer. If the resource will be read from, we need to place a value either in the buffer directly, or in the
    // image staging buffer. The values will be pseudorandom and, at verification time, the IO buffer will have to match
    // the different buffer resources.
    std::vector<tcu::Vec4> ioBufferItems(ioItemCount, tcu::Vec4(0.0f));
    de::Random rng(m_params.getSeed());

    for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
    {
        const auto setIndex = infoIndex + setIndexOffset;
        const auto &setInfo = setInfos.at(infoIndex);

        for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
        {
            const auto &descInfo = setInfo.descriptors.at(bindingIndex);

            // No values associated to the sampler.
            if (descInfo.descryptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
                continue;

            for (uint32_t arrayIndex = 0u; arrayIndex < descInfo.count; ++arrayIndex)
            {
                const tcu::Vec4 value(rng.getFloat(), rng.getFloat(), rng.getFloat(), 1.0f);

                if (descInfo.write)
                {
                    const auto ioIndex        = getIOIndex(infoIndex, bindingIndex, descInfo.isArray, arrayIndex);
                    ioBufferItems.at(ioIndex) = value;
                }
                else
                {
                    const ResourceLocation resLoc(setIndex, bindingIndex, arrayIndex, descInfo.isArray);
                    const auto &resourceInfo = resources.at(resLoc);
                    auto &bufferAlloc        = (resourceInfo.hasBuffer() ? resourceInfo.getBuffer().getAllocation() :
                                                                           resourceInfo.getImage().getBufferAllocation());
                    const auto bufferBytes   = reinterpret_cast<uint8_t *>(bufferAlloc.getHostPtr());
                    const auto offset        = resourceInfo.getOffset();
                    memcpy(bufferBytes + offset, &value, sizeof(value));
                    flushAlloc(ctx.vkd, ctx.device, bufferAlloc);
                }
            }
        }
    }
    {
        auto &ioBufferAlloc = ioBuffer.getAllocation();
        memcpy(ioBufferAlloc.getHostPtr(), de::dataOrNull(ioBufferItems), de::dataSize(ioBufferItems));
        flushAlloc(ctx.vkd, ctx.device, ioBufferAlloc);
    }

    // Update descriptor sets.
    DescriptorSetUpdateBuilder setUpdateBuilder;
    DescriptorSetUpdateBuilder setUpdateBuilderWithPush;

    for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
    {
        const auto setIndex = infoIndex + setIndexOffset;
        const auto &setInfo = setInfos.at(infoIndex);

        auto &updater = (setInfo.isPush ? setUpdateBuilderWithPush : setUpdateBuilder);

        for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
        {
            const auto &descInfo       = setInfo.descriptors.at(bindingIndex);
            const auto strideAndOffset = descInfo.getStrideAndOffset(deviceLimits);

            for (uint32_t arrayIndex = 0u; arrayIndex < descInfo.count; ++arrayIndex)
            {
                VkDescriptorImageInfo imageInfo;
                VkDescriptorBufferInfo bufferInfo;

                const VkDescriptorImageInfo *pImageInfo   = nullptr;
                const VkDescriptorBufferInfo *pBufferInfo = nullptr;
                const VkBufferView *pBufferView           = nullptr;

                const ResourceLocation resLoc(setIndex, bindingIndex, arrayIndex, descInfo.isArray);

                // The given descriptor may or may not have a resource associated (samplers do not).
                const ResourceInfo *resourceInfo = nullptr;
                auto infoItr                     = resources.find(resLoc);
                if (infoItr != resources.end())
                    resourceInfo = &(infoItr->second);

                switch (descInfo.descryptorType)
                {
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    imageInfo.sampler     = *sampler;
                    imageInfo.imageView   = resourceInfo->getImage().getImageView();
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    pImageInfo            = &imageInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    imageInfo.sampler     = VK_NULL_HANDLE;
                    imageInfo.imageView   = resourceInfo->getImage().getImageView();
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    pImageInfo            = &imageInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                    imageInfo.sampler     = VK_NULL_HANDLE;
                    imageInfo.imageView   = resourceInfo->getImage().getImageView();
                    imageInfo.imageLayout = inputAttLayout;
                    pImageInfo            = &imageInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    imageInfo.sampler     = VK_NULL_HANDLE;
                    imageInfo.imageView   = resourceInfo->getImage().getImageView();
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    pImageInfo            = &imageInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                    pBufferView = &resourceInfo->getBufferView();
                    break;
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    bufferInfo.buffer = resourceInfo->getBuffer().get();
                    bufferInfo.offset = 0ull;
                    bufferInfo.range  = strideAndOffset.first;
                    pBufferInfo       = &bufferInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    imageInfo.sampler     = *sampler;
                    imageInfo.imageView   = VK_NULL_HANDLE;
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    pImageInfo            = &imageInfo;
                    break;
                default:
                    DE_ASSERT(false);
                    break;
                }

                updater.write(descriptorSets.at(setIndex)->get(), bindingIndex, arrayIndex, 1u, descInfo.descryptorType,
                              pImageInfo, pBufferInfo, pBufferView);
            }
        }
    }
    {
        // Update IO buffer descriptor.
        const auto bufferInfo = makeDescriptorBufferInfo(ioBuffer.get(), 0ull, VK_WHOLE_SIZE);
        const auto ioSet      = (m_params.ioFirst ? descriptorSets.front()->get() : descriptorSets.back()->get());
        setUpdateBuilder.write(ioSet, 0u, 0u, 1u, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr);
    }

    // Push descriptors will be pushed later.
    setUpdateBuilder.update(ctx.vkd, ctx.device);

    // Final pipeline layout. This is the union needed for binding descriptor sets and such.
    std::vector<VkDescriptorSetLayout> setLayoutHandles;
    setLayoutHandles.reserve(setLayouts.size());
    for (const auto &setLayoutPtr : setLayouts)
        setLayoutHandles.push_back(setLayoutPtr->get());
    const VkPipelineLayoutCreateFlags indSetsPipelineLayoutFlags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
    VkPipelineLayoutCreateFlags unionPipelineLayoutFlags         = 0u;
    if (isESO && !m_params.avoidMaintenance11)
    {
        unionPipelineLayoutFlags |= indSetsPipelineLayoutFlags;
        if (m_params.hasStage(VK_SHADER_STAGE_MESH_BIT_EXT) && !m_params.hasStage(VK_SHADER_STAGE_TASK_BIT_EXT))
            unionPipelineLayoutFlags |= VK_PIPELINE_LAYOUT_CREATE_NO_TASK_SHADER_BIT_KHR;
    }
    if (isFastLib)
        unionPipelineLayoutFlags |= indSetsPipelineLayoutFlags;
    PipelineLayoutWrapper unionPipelineLayout(m_params.constructionType, ctx.vkd, ctx.device,
                                              de::sizeU32(setLayoutHandles), de::dataOrNull(setLayoutHandles), 0u,
                                              nullptr, unionPipelineLayoutFlags, true);

    // Prepare shader objects.
    const auto &binaries = m_context.getBinaryCollection();

    ShaderWrapper vertShader;
    ShaderWrapper tescShader;
    ShaderWrapper teseShader;
    ShaderWrapper geomShader;
    ShaderWrapper meshShader;
    ShaderWrapper taskShader;
    ShaderWrapper fragShader;

    // Pseudo pipeline layouts for each shader.
    PipelineLayoutWrapper vertPipelineLayout;
    PipelineLayoutWrapper tescPipelineLayout;
    PipelineLayoutWrapper tesePipelineLayout;
    PipelineLayoutWrapper geomPipelineLayout;
    PipelineLayoutWrapper meshPipelineLayout;
    PipelineLayoutWrapper taskPipelineLayout;
    PipelineLayoutWrapper fragPipelineLayout;

    // Pipeline layouts for each pipeline piece.
    PipelineLayoutWrapper preRasterPiecePipelineLayout;
    PipelineLayoutWrapper fragPiecePipelineLayout;

    // We start with a common vector of sets, with only the IO buffer set layout specified.
    std::vector<VkDescriptorSetLayout> commonSetLayouts(totalSetCount, VK_NULL_HANDLE);
    if (m_params.ioFirst)
        commonSetLayouts.front() = setLayouts.front()->get();
    else
        commonSetLayouts.back() = setLayouts.back()->get();

    // Set layout arrays for the different stages will specify one more layout as needed.
    auto vertSetLayouts = commonSetLayouts;
    auto tescSetLayouts = commonSetLayouts;
    auto teseSetLayouts = commonSetLayouts;
    auto geomSetLayouts = commonSetLayouts;
    auto meshSetLayouts = commonSetLayouts;
    auto taskSetLayouts = commonSetLayouts;
    auto fragSetLayouts = commonSetLayouts;

    // Set layouts array for each pipeline piece.
    auto preRasterPieceSetLayouts = commonSetLayouts;
    auto fragPieceSetLayouts      = commonSetLayouts;

    // Removes null handles at the end of a descriptor set layout vector.
    const auto trimSetLayoutVec = [](std::vector<VkDescriptorSetLayout> &vec)
    {
        while (!vec.empty() && vec.back() == VK_NULL_HANDLE)
            vec.pop_back();
    };

    // Prepare a shader for the given stage. The function will overwrite the wrapper and modify other arguments, as
    // needed, for the given stage.
    const auto maybeMakeShader = [&](ShaderWrapper &shader, VkShaderStageFlagBits stage, const char *shaderName,
                                     std::vector<VkDescriptorSetLayout> &setLayoutsVec,
                                     PipelineLayoutWrapper &layoutWrapper)
    {
        if (m_params.hasStage(stage))
        {
            shader = ShaderWrapper(ctx.vkd, ctx.device, binaries.get(shaderName));

            // Save the set layout for this shader stage.
            const auto setIndex        = stageIndexMap.at(stage);
            setLayoutsVec.at(setIndex) = setLayouts.at(setIndex)->get();

            // Trim the list to make the cases more interesting: different descriptor set layout counts per stage.
            trimSetLayoutVec(setLayoutsVec);

            // Create and save a pipeline layout for this shader stage.
            layoutWrapper = PipelineLayoutWrapper(m_params.constructionType, ctx.vkd, ctx.device,
                                                  de::sizeU32(setLayoutsVec), de::dataOrNull(setLayoutsVec));
            shader.setPipelineLayout(m_params.avoidMaintenance11 ? &unionPipelineLayout : &layoutWrapper);

            auto &pieceSetLayouts =
                ((stage != VK_SHADER_STAGE_FRAGMENT_BIT) ? preRasterPieceSetLayouts : fragPieceSetLayouts);
            pieceSetLayouts.at(setIndex) = setLayouts.at(setIndex)->get();
        }
    };

    maybeMakeShader(vertShader, VK_SHADER_STAGE_VERTEX_BIT, "vert", vertSetLayouts, vertPipelineLayout);
    maybeMakeShader(tescShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc", tescSetLayouts, tescPipelineLayout);
    maybeMakeShader(teseShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese", teseSetLayouts,
                    tesePipelineLayout);
    maybeMakeShader(geomShader, VK_SHADER_STAGE_GEOMETRY_BIT, "geom", geomSetLayouts, geomPipelineLayout);
    maybeMakeShader(taskShader, VK_SHADER_STAGE_TASK_BIT_EXT, "task", taskSetLayouts, taskPipelineLayout);
    maybeMakeShader(meshShader, VK_SHADER_STAGE_MESH_BIT_EXT, "mesh", meshSetLayouts, meshPipelineLayout);
    maybeMakeShader(fragShader, VK_SHADER_STAGE_FRAGMENT_BIT, "frag", fragSetLayouts, fragPipelineLayout);

    trimSetLayoutVec(preRasterPieceSetLayouts);
    trimSetLayoutVec(fragPieceSetLayouts);

    // Avoid using VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT when the driver may not support GPL.
    {
        const auto preRasterLayoutFlags = (isLib ? indSetsPipelineLayoutFlags : 0u);
        preRasterPiecePipelineLayout =
            PipelineLayoutWrapper(m_params.constructionType, ctx.vkd, ctx.device, de::sizeU32(preRasterPieceSetLayouts),
                                  de::dataOrNull(preRasterPieceSetLayouts), 0u, nullptr, preRasterLayoutFlags);
    }
    {
        const auto fragLayoutFlags = (isLib ? indSetsPipelineLayoutFlags : 0u);
        fragPiecePipelineLayout =
            PipelineLayoutWrapper(m_params.constructionType, ctx.vkd, ctx.device, de::sizeU32(fragPieceSetLayouts),
                                  de::dataOrNull(fragPieceSetLayouts), 0u, nullptr, fragLayoutFlags);
    }

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params.constructionType);

    if (!m_params.avoidMaintenance11)
        pipeline.setShaderCreateFlags(VK_SHADER_CREATE_INDEPENDENT_SETS_BIT_KHR);

    // These are only used for non-mesh pipelines.
    BufferWithMemoryPtr vertexBuffer;
    const auto topology =
        (m_params.hasStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
                                                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const auto vertexBindingDescription =
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttributeDescription =
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        1u,
        &vertexBindingDescription,
        1u,
        &vertexAttributeDescription,
    };

    // Common.
    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    const std::vector<tcu::Vec4> vertices{
        // clang-format off
        tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
        tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
        tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
        tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f),
        // clang-format on
    };

    // Map input attachments by index. This will be helpful later and allows us to prepare the color blend state.
    std::map<uint32_t, VkImageView> inputAttachmentMap;
    for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
    {
        const auto setIndex = infoIndex + setIndexOffset;
        const auto &setInfo = setInfos.at(infoIndex);

        for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
        {
            const auto &descInfo = setInfo.descriptors.at(bindingIndex);
            if (descInfo.descryptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                continue;

            for (uint32_t arrayIndex = 0u; arrayIndex < descInfo.count; ++arrayIndex)
            {
                const ResourceLocation resLoc(setIndex, bindingIndex, arrayIndex, descInfo.isArray);
                const auto &resourceInfo = resources[resLoc];

                inputAttachmentMap[descInfo.attIndex] = resourceInfo.getImage().getImageView();
            }
        }
    }

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (!isESO)
    {
        std::vector<VkAttachmentDescription> attDescriptions;
        std::vector<VkImageView> fbViews;

        attDescriptions.reserve(inputAttachmentMap.size());
        fbViews.reserve(inputAttachmentMap.size());

        for (const auto &indexView : inputAttachmentMap)
        {
            attDescriptions.push_back(makeAttachmentDescription(
                0u, imageFormat, sampleCount,
                VK_ATTACHMENT_LOAD_OP_LOAD,   // Similar to dynamic rendering.
                VK_ATTACHMENT_STORE_OP_STORE, // Ditto.
                VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, inputAttLayout, inputAttLayout));

            fbViews.push_back(indexView.second);
        }

        std::vector<VkAttachmentReference> attRefs;
        attRefs.reserve(attDescriptions.size());
        for (uint32_t i = 0u; i < de::sizeU32(attDescriptions); ++i)
            attRefs.push_back(makeAttachmentReference(i, inputAttLayout));

        const auto subpassDesc = makeSubpassDescription(0u, bindPoint, de::sizeU32(attRefs), de::dataOrNull(attRefs),
                                                        0u, nullptr, nullptr, nullptr, 0u, nullptr);

        const VkRenderPassCreateInfo rpCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0u,
            de::sizeU32(attDescriptions),
            de::dataOrNull(attDescriptions),
            1u,
            &subpassDesc,
            0u,
            nullptr,
        };

        renderPass  = createRenderPass(ctx.vkd, ctx.device, &rpCreateInfo);
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews), de::dataOrNull(fbViews),
                                      extentVk.width, extentVk.height);
    }

    if (m_params.hasStage(VK_SHADER_STAGE_VERTEX_BIT))
    {
        // Make a vertex buffer, similar to what we use in the mesh shader, that can be used for patches or triangles.
        const auto vertexBufferSize       = static_cast<VkDeviceSize>(de::dataSize(vertices));
        const auto vertexBufferUsage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        const auto vertexBufferCreateInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
        vertexBuffer.reset(
            new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, vertexBufferCreateInfo, HostIntent::W));
        {
            auto &alloc = vertexBuffer->getAllocation();
            memcpy(alloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
            flushAlloc(ctx.vkd, ctx.device, alloc);
        }

        pipeline.setDefaultTopology(topology);
        pipeline.setupVertexInputState(&vertexInputStateCreateInfo, nullptr);
        pipeline.setDefaultRasterizationState();
        pipeline.setupPreRasterizationShaderState(viewports, scissors, preRasterPiecePipelineLayout, *renderPass, 0u,
                                                  vertShader, nullptr, tescShader, teseShader, geomShader);
    }
    else
    {
        pipeline.setDefaultRasterizationState();
        pipeline.setupPreRasterizationMeshShaderState(viewports, scissors, preRasterPiecePipelineLayout, *renderPass,
                                                      0u, taskShader, meshShader);
    }

    VkPipelineColorBlendAttachmentState defaultBlendState;
    memset(&defaultBlendState, 0, sizeof(defaultBlendState));
    defaultBlendState.colorWriteMask =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    const std::vector<VkPipelineColorBlendAttachmentState> blendStates(inputAttachmentMap.size(), defaultBlendState);

    // When using ESO, attachments are color and input at the same time, so the attachment count must match the input
    // attachment count. However, when using libs and classic render passes, there are no color attachments because
    // everything is an input attachment.
    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
    colorBlendState.attachmentCount                     = (isESO ? de::sizeU32(blendStates) : 0u);
    colorBlendState.pAttachments                        = (isESO ? de::dataOrNull(blendStates) : nullptr);

    pipeline.setDefaultDepthStencilState();
    pipeline.setDefaultMultisampleState();
    {
        pipeline.setupFragmentShaderState(fragPiecePipelineLayout, *renderPass, 0u, fragShader);
        pipeline.setupFragmentOutputState(*renderPass, 0u, &colorBlendState);
    }
    pipeline.setMonolithicPipelineLayout(unionPipelineLayout);
    pipeline.buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    // Deal with expected image layouts and contents.
    {
        std::vector<VkImageMemoryBarrier> preTransferBarriers;
        std::vector<VkImageMemoryBarrier> postTransferBarriers;
        preTransferBarriers.reserve(ioItemCount);  // This is an estimated upper bound.
        postTransferBarriers.reserve(ioItemCount); // This is an estimated upper bound.

        // We need to copy the staging buffer contents for some images into the image resource.
        // Remember which buffers will be copied to which images.
        std::map<VkBuffer, VkImage> copyOps;

        const auto xferAccess                      = VK_ACCESS_TRANSFER_WRITE_BIT;
        VkPipelineStageFlags postTransferDstStages = 0u;

        for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
        {
            const auto setIndex = infoIndex + setIndexOffset;
            const auto &setInfo = setInfos.at(infoIndex);

            const auto shaderStage   = indexStageMap.at(setIndex);
            const auto pipelineStage = getPipelineStage(shaderStage);

            for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
            {
                const auto &descInfo = setInfo.descriptors.at(bindingIndex);
                for (uint32_t arrayIndex = 0u; arrayIndex < descInfo.count; ++arrayIndex)
                {
                    const ResourceLocation resLoc(setIndex, bindingIndex, arrayIndex, descInfo.isArray);

                    // The given descriptor may or may not have a resource associated (samplers do not).
                    const ResourceInfo *resourceInfo = nullptr;
                    auto infoItr                     = resources.find(resLoc);
                    if (infoItr != resources.end())
                        resourceInfo = &(infoItr->second);

                    switch (descInfo.descryptorType)
                    {
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                        preTransferBarriers.push_back(makeImageMemoryBarrier(
                            0u, xferAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            resourceInfo->getImage().getImage(), colorSRR));
                        postTransferBarriers.push_back(makeImageMemoryBarrier(
                            xferAccess, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, resourceInfo->getImage().getImage(), colorSRR));
                        postTransferDstStages |= pipelineStage;
                        copyOps[resourceInfo->getImage().getBuffer()] = resourceInfo->getImage().getImage();
                        break;
                    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                        preTransferBarriers.push_back(makeImageMemoryBarrier(
                            0u, xferAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            resourceInfo->getImage().getImage(), colorSRR));
                        postTransferBarriers.push_back(
                            makeImageMemoryBarrier(xferAccess,
                                                   (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, inputAttLayout,
                                                   resourceInfo->getImage().getImage(), colorSRR));
                        postTransferDstStages |= (pipelineStage | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                        copyOps[resourceInfo->getImage().getBuffer()] = resourceInfo->getImage().getImage();
                        break;
                    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                        preTransferBarriers.push_back(makeImageMemoryBarrier(
                            0u, xferAccess, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            resourceInfo->getImage().getImage(), colorSRR));
                        postTransferBarriers.push_back(
                            makeImageMemoryBarrier(xferAccess, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                                   resourceInfo->getImage().getImage(), colorSRR));
                        postTransferDstStages |= pipelineStage;
                        if (!descInfo.write)
                            copyOps[resourceInfo->getImage().getBuffer()] = resourceInfo->getImage().getImage();
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        if (!preTransferBarriers.empty())
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, preTransferBarriers.data(),
                                          preTransferBarriers.size());

        for (const auto &bufferImage : copyOps)
            ctx.vkd.cmdCopyBufferToImage(cmdBuffer, bufferImage.first, bufferImage.second,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);

        if (!postTransferBarriers.empty())
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, postTransferDstStages,
                                          postTransferBarriers.data(), postTransferBarriers.size());
    }

    if (isESO)
    {
        std::vector<VkRenderingAttachmentInfo> inputAttachments;
        inputAttachments.reserve(inputAttachmentMap.size());
        for (const auto &indexView : inputAttachmentMap)
        {
            inputAttachments.push_back(VkRenderingAttachmentInfo{
                // clang-format off
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                indexView.second,
                inputAttLayout,
                VK_RESOLVE_MODE_NONE,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_ATTACHMENT_STORE_OP_STORE,
                makeClearValueColor(tcu::Vec4(0.0f)), // Not used.
                // clang-format on
            });
        }

        const VkRenderingInfo renderingInfo = {
            // clang-format off
            VK_STRUCTURE_TYPE_RENDERING_INFO,
            nullptr,
            0u,
            scissors.front(),
            1u,
            0u,
            de::sizeU32(inputAttachments),
            de::dataOrNull(inputAttachments),
            nullptr,
            nullptr,
            // clang-format on
        };

        ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
    }
    else
    {
        const VkRenderPassBeginInfo rpBeginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, *renderPass, *framebuffer, scissors.front(), 0u, nullptr,
        };

        ctx.vkd.cmdBeginRenderPass(cmdBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    pipeline.bind(cmdBuffer);

    {
        std::vector<VkDescriptorSet> descriptorSetHandles;
        descriptorSetHandles.reserve(descriptorSets.size());
        for (const auto &descSetPtr : descriptorSets)
            descriptorSetHandles.push_back(descSetPtr->get());

        std::vector<uint32_t> dynamicOffsets;
        for (const auto &setInfo : setInfos)
            for (const auto &descInfo : setInfo.descriptors)
            {
                if (descInfo.descryptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                    descInfo.descryptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                {
                    const auto strideAndOffset = descInfo.getStrideAndOffset(deviceLimits);
                    const auto offset          = static_cast<uint32_t>(strideAndOffset.second);
                    dynamicOffsets.resize(dynamicOffsets.size() + descInfo.count, offset);
                }
            }

        if (!descriptorSetHandles.empty() || !dynamicOffsets.empty())
        {
            ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, unionPipelineLayout.get(), 0u,
                                          de::sizeU32(descriptorSetHandles), de::dataOrNull(descriptorSetHandles),
                                          de::sizeU32(dynamicOffsets), de::dataOrNull(dynamicOffsets));
        }

        if (hasPushDescriptors)
        {
            DE_ASSERT(setInfos.back().isPush);
            const auto pushSetIndex = de::sizeU32(setInfos) - 1u + setIndexOffset;
            setUpdateBuilderWithPush.updateWithPush(ctx.vkd, cmdBuffer, bindPoint, unionPipelineLayout.get(),
                                                    pushSetIndex);
        }
    }

    if (m_params.hasStage(VK_SHADER_STAGE_VERTEX_BIT))
    {
        const VkDeviceSize vertexBufferOffset = 0ull;
        ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer->get(), &vertexBufferOffset);
        ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(vertices), 1u, 0u, 0u);
    }
    else
    {
        ctx.vkd.cmdDrawMeshTasksEXT(cmdBuffer, 1u, 1u, 1u);
    }

    if (isESO)
        ctx.vkd.cmdEndRendering(cmdBuffer);
    else
        ctx.vkd.cmdEndRenderPass(cmdBuffer);

    // Storage images that are written to need to transition to the TRANSFER_SRC layout and copy their contents to their
    // corresponding staging buffer.
    {
        std::map<VkImage, VkBuffer> copyOps;
        VkPipelineStageFlags srcStages = 0u;

        std::vector<VkImageMemoryBarrier> preCopyBarriers;
        preCopyBarriers.reserve(ioItemCount); // Upper bound.

        for (uint32_t infoIndex = 0u; infoIndex < de::sizeU32(setInfos); ++infoIndex)
        {
            const auto setIndex      = infoIndex + setIndexOffset;
            const auto &setInfo      = setInfos.at(infoIndex);
            const auto shaderStage   = indexStageMap.at(setIndex);
            const auto pipelineStage = getPipelineStage(shaderStage);

            for (uint32_t bindingIndex = 0u; bindingIndex < de::sizeU32(setInfo.descriptors); ++bindingIndex)
            {
                const auto &descInfo = setInfo.descriptors.at(bindingIndex);

                // Only storage images.
                if (descInfo.descryptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    continue;

                for (uint32_t arrayIndex = 0u; arrayIndex < descInfo.count; ++arrayIndex)
                {
                    const ResourceLocation resLoc(setIndex, bindingIndex, arrayIndex, descInfo.isArray);
                    const auto &resourceInfo = resources.at(resLoc);

                    // Changing the layout is not needed but we need the barrier anyway so it doesn't hurt.
                    preCopyBarriers.push_back(makeImageMemoryBarrier(
                        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resourceInfo.getImage().getImage(), colorSRR));

                    copyOps[resourceInfo.getImage().getImage()] = resourceInfo.getImage().getBuffer();

                    srcStages |= pipelineStage;
                }
            }
        }

        if (!preCopyBarriers.empty())
            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          preCopyBarriers.data(), preCopyBarriers.size());

        for (const auto &imageBuffer : copyOps)
            ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, imageBuffer.first, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         imageBuffer.second, 1u, &copyRegion);

        const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &preHostBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Invalidate all host buffer allocations. This is easier than filtering.
    for (const auto &resourceLocInfo : resources)
    {
        const auto &resInfo = resourceLocInfo.second;
        if (resInfo.hasBuffer())
            invalidateAlloc(ctx.vkd, ctx.device, resInfo.getBuffer().getAllocation());
        else if (resInfo.hasImage())
            invalidateAlloc(ctx.vkd, ctx.device, resInfo.getImage().getBufferAllocation());
        else
            DE_ASSERT(false);
    }
    invalidateAlloc(ctx.vkd, ctx.device, ioBuffer.getAllocation());

    // After this, the IO buffer and the host buffers need to have the same contents.
    bool fail = false;
    auto &log = m_context.getTestContext().getLog();
    {
        memcpy(de::dataOrNull(ioBufferItems), ioBuffer.getAllocation().getHostPtr(), de::dataSize(ioBufferItems));

        // Image format uses floats and does not perform any arithmetic, so we expect exact copies of values.
        const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

        for (const auto &resourceLocInfo : resources)
        {
            const auto &resInfo = resourceLocInfo.second;
            const auto &resLoc  = resourceLocInfo.first;

            // Value from the resource buffer.
            tcu::Vec4 resourceData(0.0f);
            if (resInfo.hasBuffer())
            {
                auto &bufferAlloc      = resInfo.getBuffer().getAllocation();
                const auto bufferBytes = reinterpret_cast<const uint8_t *>(bufferAlloc.getHostPtr());
                const auto offset      = resInfo.getOffset();
                memcpy(&resourceData, bufferBytes + offset, sizeof(resourceData));
            }
            else if (resInfo.hasImage())
                memcpy(&resourceData, resInfo.getImage().getBufferAllocation().getHostPtr(), sizeof(resourceData));
            else
                DE_ASSERT(false);

            // Value from the IO buffer.
            const auto setInfoIndex = resLoc.setIndex - setIndexOffset;

            const auto ioIndex = getIOIndex(setInfoIndex, resLoc.binding, resLoc.isArray, resLoc.arrayIndex);
            const auto &ioData = ioBufferItems.at(ioIndex);

            if (!tcu::boolAll(tcu::lessThanEqual(tcu::absDiff(resourceData, ioData), threshold)))
            {
                fail = true;
                std::ostringstream msg;
                msg << "ERROR: unexpected value in set=" << resLoc.setIndex << " binding=" << resLoc.binding
                    << " descriptor=" << resLoc.arrayIndex << ": descriptor=" << resourceData << " io_ssbo=" << ioData
                    << " threshold=" << threshold << ")";
                log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
            }
        }
    }

    if (fail)
        TCU_FAIL("Found mismatches between IO buffer and descriptor contents; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

tcu::TestCaseGroup *createRandomTests(tcu::TestContext &testCtx, const std::string &groupName,
                                      const std::vector<vk::PipelineConstructionType> &constructionTypes)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, groupName.c_str()));

    static const std::map<PipelineConstructionType, std::string> subGroupNames{
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV, "unlinked_spirv"),
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY, "unlinked_binary"),
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV, "linked_spirv"),
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY, "linked_binary"),
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, "fast_lib"),
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY, "optimized_lib"),
        std::make_pair(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, "monolithic"),
    };

    constexpr uint32_t kCasesPerGroup = 10u; // How many pseudorandom cases per group.

    for (const auto &constructionType : constructionTypes)
    {
        const bool isESO         = isConstructionTypeShaderObject(constructionType);
        const auto &subGroupName = subGroupNames.at(constructionType);
        GroupPtr constructionGroup(new tcu::TestCaseGroup(testCtx, subGroupName.c_str()));

        // Classic vertex pipeline tests.
        {
            for (const bool useTess : {false, true})
                for (const bool useGeom : {false, true})
                    for (const bool useFrag : {false, true})
                    {
                        VkShaderStageFlags stages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT);

                        if (useTess)
                        {
                            stages |= (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                       VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
                        }
                        if (useGeom)
                            stages |= VK_SHADER_STAGE_GEOMETRY_BIT;
                        if (useFrag)
                            stages |= VK_SHADER_STAGE_FRAGMENT_BIT;

                        IndependentSets::Params params{constructionType, stages, 0u, false, false};
                        const auto stageNames = params.getStageNames();

                        GroupPtr stageGroup(new tcu::TestCaseGroup(testCtx, stageNames.c_str()));

                        for (uint32_t i = 0u; i < kCasesPerGroup; ++i)
                        {
                            params.caseIndex = i;
                            for (const bool ioFirst : {false, true})
                            {
                                params.ioFirst = ioFirst;
                                std::vector<bool> avoidMaint11Cases(1u, false);
                                if (isESO && !ioFirst)
                                    avoidMaint11Cases.push_back(true);
                                for (const bool avoidMaint11 : avoidMaint11Cases)
                                {
                                    params.avoidMaintenance11 = avoidMaint11;
                                    auto testName = "case_" + std::to_string(i) + (ioFirst ? "_io_ssbo_first" : "") +
                                                    (avoidMaint11 ? "_no_maint11" : "");
                                    stageGroup->addChild(new IndependentSets::Case(testCtx, testName, params));
                                }
                            }
                        }

                        constructionGroup->addChild(stageGroup.release());
                    }
        }

        // Mesh pipeline tests.
        {
            for (const bool useTask : {false, true})
                for (const bool useFrag : {false, true})
                {
                    VkShaderStageFlags stages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_MESH_BIT_EXT);

                    if (useTask)
                        stages |= VK_SHADER_STAGE_TASK_BIT_EXT;
                    if (useFrag)
                        stages |= VK_SHADER_STAGE_FRAGMENT_BIT;

                    IndependentSets::Params params{constructionType, stages, 0u, false, false};
                    const auto stageNames = params.getStageNames();

                    GroupPtr stageGroup(new tcu::TestCaseGroup(testCtx, stageNames.c_str()));

                    for (uint32_t i = 0u; i < kCasesPerGroup; ++i)
                    {
                        params.caseIndex = i;
                        for (const bool ioFirst : {false, true})
                        {
                            params.ioFirst = ioFirst;
                            std::vector<bool> avoidMaint11Cases(1u, false);
                            if (isESO && !ioFirst)
                                avoidMaint11Cases.push_back(true);
                            for (const bool avoidMaint11 : avoidMaint11Cases)
                            {
                                params.avoidMaintenance11 = avoidMaint11;
                                auto testName = "case_" + std::to_string(i) + (ioFirst ? "_io_ssbo_first" : "") +
                                                (avoidMaint11 ? "_no_maint11" : "");
                                stageGroup->addChild(new IndependentSets::Case(testCtx, testName, params));
                            }
                        }
                    }

                    constructionGroup->addChild(stageGroup.release());
                }
        }

        mainGroup->addChild(constructionGroup.release());
    }

    return mainGroup.release();
}

#endif // CTS_USES_VULKANSC

} // namespace IndependentSets
} // namespace vkt
