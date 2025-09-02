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
 * \brief Device Generated Commands EXT Utility Code
 *//*--------------------------------------------------------------------*/
#include "vktDGCUtilExt.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkShaderObjectUtil.hpp"

#include <algorithm>
#include <iterator>
#include <bitset>

namespace vkt
{
namespace DGC
{

using namespace vk;

void checkDGCExtSupport(Context &context, VkShaderStageFlags stages, VkShaderStageFlags bindStagesPipeline,
                        VkShaderStageFlags bindStagesShaderObject, VkIndirectCommandsInputModeFlagsEXT inputModeFlags,
                        bool transformFeedback)
{
    context.requireDeviceFunctionality("VK_EXT_device_generated_commands");

    const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
    if ((dgcProperties.supportedIndirectCommandsShaderStages & stages) != stages)
        TCU_THROW(NotSupportedError, "Required DGC stages not supported");

    if ((dgcProperties.supportedIndirectCommandsShaderStagesPipelineBinding & bindStagesPipeline) != bindStagesPipeline)
        TCU_THROW(NotSupportedError, "Required DGC pipeline bind stages not supported");

    if ((dgcProperties.supportedIndirectCommandsShaderStagesShaderBinding & bindStagesShaderObject) !=
        bindStagesShaderObject)
        TCU_THROW(NotSupportedError, "Required DGC shader object bind stages not supported");

    if ((dgcProperties.supportedIndirectCommandsInputModes & inputModeFlags) != inputModeFlags)
        TCU_THROW(NotSupportedError, "Required DGC index buffer input modes not supported");

    if (transformFeedback && !dgcProperties.deviceGeneratedCommandsTransformFeedback)
        TCU_THROW(NotSupportedError, "DGC transform feedback not supported");
}

void checkDGCExtComputeSupport(Context &context, DGCComputeSupportType supportType)
{
    const auto stages                 = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);
    const auto bindStagesPipeline     = ((supportType == DGCComputeSupportType::BIND_PIPELINE) ? stages : 0u);
    const auto bindStagesShaderObject = ((supportType == DGCComputeSupportType::BIND_SHADER) ? stages : 0u);

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);
}

VkIndirectExecutionSetInfoEXT makeIndirectExecutionSetInfo(const VkIndirectExecutionSetPipelineInfoEXT &pipelineInfo)
{
    VkIndirectExecutionSetInfoEXT info;
    info.pPipelineInfo = &pipelineInfo;
    return info;
}

VkIndirectExecutionSetInfoEXT makeIndirectExecutionSetInfo(const VkIndirectExecutionSetShaderInfoEXT &shaderInfo)
{
    VkIndirectExecutionSetInfoEXT info;
    info.pShaderInfo = &shaderInfo;
    return info;
}

ExecutionSetManager::ExecutionSetManager(const DeviceInterface &vkd, VkDevice device,
                                         const VkIndirectExecutionSetPipelineInfoEXT &pipelineInfo)
    : m_vkd(vkd)
    , m_device(device)
    , m_executionSet()
    , m_pipelines(true)
    , m_shaderObjects(false)
    , m_pipelineWrites()
    , m_shaderWrites()
{
    const VkIndirectExecutionSetCreateInfoEXT createInfo = {
        VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                                  // void* pNext;
        VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT,        // VkIndirectExecutionSetInfoTypeEXT type;
        makeIndirectExecutionSetInfo(pipelineInfo),               // VkIndirectExecutionSetInfoEXT info;
    };

    m_executionSet = createIndirectExecutionSetEXT(vkd, device, &createInfo);
}

ExecutionSetManager::ExecutionSetManager(const DeviceInterface &vkd, VkDevice device,
                                         const VkIndirectExecutionSetShaderInfoEXT &shaderInfo)
    : m_vkd(vkd)
    , m_device(device)
    , m_executionSet()
    , m_pipelines(false)
    , m_shaderObjects(true)
    , m_pipelineWrites()
    , m_shaderWrites()
{
    const VkIndirectExecutionSetCreateInfoEXT createInfo = {
        VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT, // VkStructureType sType;
        nullptr,                                                  // void* pNext;
        VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT,   // VkIndirectExecutionSetInfoTypeEXT type;
        makeIndirectExecutionSetInfo(shaderInfo),                 // VkIndirectExecutionSetInfoEXT info;
    };

    m_executionSet = createIndirectExecutionSetEXT(vkd, device, &createInfo);
}

void ExecutionSetManager::addPipeline(uint32_t index, VkPipeline pipeline)
{
    DE_ASSERT(m_pipelines);

    // Avoid duplicating indices, which is illegal as per some VUs.
    for (const auto &pipelineWrite : m_pipelineWrites)
    {
        if (pipelineWrite.index == index)
        {
            DE_ASSERT(pipelineWrite.pipeline == pipeline);
            return;
        }
    }

    VkWriteIndirectExecutionSetPipelineEXT write = initVulkanStructure();
    write.index                                  = index;
    write.pipeline                               = pipeline;
    m_pipelineWrites.push_back(write);
}

void ExecutionSetManager::addShader(uint32_t index, VkShaderEXT shader)
{
    DE_ASSERT(m_shaderObjects);

    // Avoid duplicating indices, which is illegal as per some VUs.
    for (const auto &shaderWrite : m_shaderWrites)
    {
        if (shaderWrite.index == index)
        {
            DE_ASSERT(shaderWrite.shader == shader);
            return;
        }
    }

    VkWriteIndirectExecutionSetShaderEXT write = initVulkanStructure();
    write.index                                = index;
    write.shader                               = shader;
    m_shaderWrites.push_back(write);
}

void ExecutionSetManager::update(void)
{
    if (m_pipelines)
    {
        if (!m_pipelineWrites.empty())
        {
            m_vkd.updateIndirectExecutionSetPipelineEXT(m_device, *m_executionSet, de::sizeU32(m_pipelineWrites),
                                                        de::dataOrNull(m_pipelineWrites));
            m_pipelineWrites.clear();
        }
    }
    else if (m_shaderObjects)
    {
        if (!m_shaderWrites.empty())
        {
            m_vkd.updateIndirectExecutionSetShaderEXT(m_device, *m_executionSet, de::sizeU32(m_shaderWrites),
                                                      de::dataOrNull(m_shaderWrites));
            m_shaderWrites.clear();
        }
    }
    else
        DE_ASSERT(false);
}

VkIndirectExecutionSetEXT ExecutionSetManager::get(bool requireNoPendingWrites) const
{
    if (requireNoPendingWrites)
        assertNoPendingWrites();
    return m_executionSet.get();
}

ExecutionSetManagerPtr makeExecutionSetManagerPipeline(const DeviceInterface &vkd, VkDevice device,
                                                       VkPipeline initialPipeline, uint32_t maxPipelineCount)
{
    VkIndirectExecutionSetPipelineInfoEXT info = initVulkanStructure();
    info.initialPipeline                       = initialPipeline;
    info.maxPipelineCount                      = maxPipelineCount;

    ExecutionSetManagerPtr ptr;
    ptr.reset(new ExecutionSetManager(vkd, device, info));
    return ptr;
}

ExecutionSetManagerPtr makeExecutionSetManagerShader(const DeviceInterface &vkd, VkDevice device,
                                                     const std::vector<IESStageInfo> &stages,
                                                     const std::vector<VkPushConstantRange> &pushConstantRanges,
                                                     uint32_t maxShaderCount)
{
    VkIndirectExecutionSetShaderInfoEXT info = initVulkanStructure();

    info.pushConstantRangeCount = de::sizeU32(pushConstantRanges);
    info.pPushConstantRanges    = de::dataOrNull(pushConstantRanges);

    // Unzip information in the stages vector into individual arrays.

    std::vector<VkShaderEXT> shaders;
    shaders.reserve(stages.size());
    std::transform(begin(stages), end(stages), std::back_inserter(shaders),
                   [](const IESStageInfo &shaderInfo) { return shaderInfo.shader; });

    std::vector<VkIndirectExecutionSetShaderLayoutInfoEXT> setLayoutInfos;
    setLayoutInfos.reserve(stages.size());
    std::transform(begin(stages), end(stages), std::back_inserter(setLayoutInfos),
                   [](const IESStageInfo &shaderInfo)
                   {
                       VkIndirectExecutionSetShaderLayoutInfoEXT item = initVulkanStructure();
                       item.setLayoutCount                            = de::sizeU32(shaderInfo.setLayouts);
                       item.pSetLayouts                               = de::dataOrNull(shaderInfo.setLayouts);
                       return item;
                   });

    info.shaderCount     = de::sizeU32(stages);
    info.pInitialShaders = de::dataOrNull(shaders);
    info.maxShaderCount  = maxShaderCount;
    info.pSetLayoutInfos = de::dataOrNull(setLayoutInfos);

    ExecutionSetManagerPtr ptr;
    ptr.reset(new ExecutionSetManager(vkd, device, info));
    return ptr;
}

DGCMemReqsInfo::DGCMemReqsInfo(vk::VkIndirectExecutionSetEXT ies, vk::VkIndirectCommandsLayoutEXT cmdsLayout,
                               uint32_t maxSeqCount, uint32_t maxDrawCount, vk::VkPipeline pipeline,
                               const std::vector<vk::VkShaderEXT> *shaders)
    : m_memReqs(initVulkanStructure())
    , m_pipelineInfo(initVulkanStructure())
    , m_shadersInfo(initVulkanStructure())
    , m_shaders()
{
    // Make sure we do not pass both.
    DE_ASSERT(pipeline == VK_NULL_HANDLE || shaders == nullptr);

    if (ies == VK_NULL_HANDLE)
    {
        if (pipeline != VK_NULL_HANDLE)
        {
            m_pipelineInfo.pipeline = pipeline;
            m_memReqs.pNext         = &m_pipelineInfo;
        }
        else if (shaders != nullptr)
        {
            DE_ASSERT(!shaders->empty());
            m_shaders                 = *shaders;
            m_shadersInfo.shaderCount = de::sizeU32(m_shaders);
            m_shadersInfo.pShaders    = de::dataOrNull(m_shaders);
            m_memReqs.pNext           = &m_shadersInfo;
        }
        else
            DE_ASSERT(false);
    }

    m_memReqs.indirectExecutionSet   = ies;
    m_memReqs.indirectCommandsLayout = cmdsLayout;
    m_memReqs.maxSequenceCount       = maxSeqCount;
    m_memReqs.maxDrawCount           = maxDrawCount;
}

DGCGenCmdsInfo::DGCGenCmdsInfo(vk::VkShaderStageFlags shaderStages, vk::VkIndirectExecutionSetEXT ies,
                               vk::VkIndirectCommandsLayoutEXT indirectCommandsLayout,
                               vk::VkDeviceAddress indirectAddress, vk::VkDeviceSize indirectAddressSize,
                               vk::VkDeviceAddress preprocessAddress, vk::VkDeviceSize preprocessSize,
                               uint32_t maxSequenceCount, vk::VkDeviceAddress sequenceCountAddress,
                               uint32_t maxDrawCount, vk::VkPipeline pipeline,
                               const std::vector<vk::VkShaderEXT> *shaders)
    : m_genCmdsInfo(initVulkanStructure())
    , m_pipelineInfo(initVulkanStructure())
    , m_shadersInfo(initVulkanStructure())
    , m_shaders()
{
    // Make sure we do not pass both.
    DE_ASSERT(pipeline == VK_NULL_HANDLE || shaders == nullptr);

    if (ies == VK_NULL_HANDLE)
    {
        if (pipeline != VK_NULL_HANDLE)
        {
            m_pipelineInfo.pipeline = pipeline;
            m_genCmdsInfo.pNext     = &m_pipelineInfo;
        }
        else if (shaders != nullptr)
        {
            DE_ASSERT(!shaders->empty());
            m_shaders                 = *shaders;
            m_shadersInfo.shaderCount = de::sizeU32(m_shaders);
            m_shadersInfo.pShaders    = de::dataOrNull(m_shaders);
            m_genCmdsInfo.pNext       = &m_shadersInfo;
        }
        else
            DE_ASSERT(false);
    }

    m_genCmdsInfo.shaderStages           = shaderStages;
    m_genCmdsInfo.indirectExecutionSet   = ies;
    m_genCmdsInfo.indirectCommandsLayout = indirectCommandsLayout;
    m_genCmdsInfo.indirectAddress        = indirectAddress;
    m_genCmdsInfo.indirectAddressSize    = indirectAddressSize;
    m_genCmdsInfo.preprocessAddress      = preprocessAddress;
    m_genCmdsInfo.preprocessSize         = preprocessSize;
    m_genCmdsInfo.maxSequenceCount       = maxSequenceCount;
    m_genCmdsInfo.sequenceCountAddress   = sequenceCountAddress;
    m_genCmdsInfo.maxDrawCount           = maxDrawCount;
}

DGCGenCmdsInfo::DGCGenCmdsInfo(const DGCGenCmdsInfo &other)
    : m_genCmdsInfo(other.m_genCmdsInfo)
    , m_pipelineInfo(other.m_pipelineInfo)
    , m_shadersInfo(other.m_shadersInfo)
    , m_shaders(other.m_shaders)
{
    // Fix shaders pointer.
    if (!m_shaders.empty())
        m_shadersInfo.pShaders = de::dataOrNull(m_shaders);

    // Fix pNext pointer so it points to *our* structure.
    if (other.m_genCmdsInfo.pNext == reinterpret_cast<const void *>(&other.m_pipelineInfo))
        m_genCmdsInfo.pNext = &m_pipelineInfo;
    else if (other.m_genCmdsInfo.pNext == reinterpret_cast<const void *>(&other.m_shadersInfo))
        m_genCmdsInfo.pNext = &m_shadersInfo;

    DE_ASSERT(m_pipelineInfo.pNext == nullptr);
    DE_ASSERT(m_shadersInfo.pNext == nullptr);
}

VkMemoryRequirements getGeneratedCommandsMemoryRequirementsExt(const DeviceInterface &vkd, VkDevice device,
                                                               const VkGeneratedCommandsMemoryRequirementsInfoEXT &info)
{
    VkMemoryRequirements2 memReqs = initVulkanStructure();
    vkd.getGeneratedCommandsMemoryRequirementsEXT(device, &info, &memReqs);
    return memReqs.memoryRequirements;
}

void preprocessToExecuteBarrierExt(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer)
{
    const auto preExecutionBarrier =
        makeMemoryBarrier(VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_EXT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_EXT,
                             VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, &preExecutionBarrier);
}

IndirectCommandsLayoutBuilderExt::IndirectCommandsLayoutBuilderExt(
    vk::VkIndirectCommandsLayoutUsageFlagsEXT usageFlags, vk::VkShaderStageFlags stageFlags,
    vk::VkPipelineLayout pipelineLayout, const vk::VkPipelineLayoutCreateInfo *pPipelineLayout)
    : m_layoutUsageFlags(usageFlags)
    , m_shaderStages(stageFlags)
    , m_pipelineLayout(pipelineLayout)
    , m_layoutCreateInfoPtr(pPipelineLayout)
{
}

IndirectCommandsLayoutBuilderExt::InternalToken &IndirectCommandsLayoutBuilderExt::pushBackEmptyToken(void)
{
    m_tokens.emplace_back();
    return m_tokens.back();
}

void IndirectCommandsLayoutBuilderExt::addSimpleToken(uint32_t offset, vk::VkIndirectCommandsTokenTypeEXT tokenType)
{
    auto &internalToken  = pushBackEmptyToken();
    internalToken.offset = offset;
    internalToken.type   = tokenType;
}

void IndirectCommandsLayoutBuilderExt::addPushConstantToken(uint32_t offset, const VkPushConstantRange &pcRange)
{
    auto &token  = pushBackEmptyToken();
    token.type   = VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT;
    token.offset = offset;
    token.pPushConstant.reset(new VkIndirectCommandsPushConstantTokenEXT{pcRange});
}

void IndirectCommandsLayoutBuilderExt::addSequenceIndexToken(uint32_t offset, const VkPushConstantRange &pcRange)
{
    DE_ASSERT(pcRange.size == 4u); // Must be fixed by the spec.

    auto &token  = pushBackEmptyToken();
    token.type   = vk::VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT;
    token.offset = offset;
    token.pPushConstant.reset(new VkIndirectCommandsPushConstantTokenEXT{pcRange});
}

void IndirectCommandsLayoutBuilderExt::addVertexBufferToken(uint32_t offset, uint32_t bindingNumber)
{
    auto &token  = pushBackEmptyToken();
    token.type   = VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT;
    token.offset = offset;
    token.pVertexBuffer.reset(new VkIndirectCommandsVertexBufferTokenEXT{bindingNumber});
}

void IndirectCommandsLayoutBuilderExt::addIndexBufferToken(uint32_t offset,
                                                           vk::VkIndirectCommandsInputModeFlagBitsEXT mode)
{
    auto &token  = pushBackEmptyToken();
    token.type   = VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT;
    token.offset = offset;
    token.pIndexBuffer.reset(new VkIndirectCommandsIndexBufferTokenEXT{
        mode,
    });
}

void IndirectCommandsLayoutBuilderExt::addExecutionSetToken(uint32_t offset,
                                                            vk::VkIndirectExecutionSetInfoTypeEXT setType,
                                                            vk::VkShaderStageFlags stages)
{
    auto &token  = pushBackEmptyToken();
    token.type   = VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT;
    token.offset = offset;
    token.pExecutionSet.reset(new VkIndirectCommandsExecutionSetTokenEXT{
        setType,
        stages,
    });
}

void IndirectCommandsLayoutBuilderExt::addComputePipelineToken(uint32_t offset)
{
    addExecutionSetToken(offset, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT,
                         static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT));
}

void IndirectCommandsLayoutBuilderExt::addComputeShaderObjectToken(uint32_t offset)
{
    addExecutionSetToken(offset, VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT,
                         static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT));
}

void IndirectCommandsLayoutBuilderExt::addDrawIndexedToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawIndexedCountToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_COUNT_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawCountToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_COUNT_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawMeshTasksCountNvToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_NV_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawMeshTasksCountToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDispatchToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawMeshTasksNvToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV_EXT);
}

void IndirectCommandsLayoutBuilderExt::addDrawMeshTasksToken(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_EXT);
}

void IndirectCommandsLayoutBuilderExt::addTraceRays2Token(uint32_t offset)
{
    addSimpleToken(offset, VK_INDIRECT_COMMANDS_TOKEN_TYPE_TRACE_RAYS2_EXT);
}

void IndirectCommandsLayoutBuilderExt::setStreamStride(uint32_t stride)
{
    // Save the manual stride for later use.
    m_manualStride = tcu::just(stride);
}

uint32_t IndirectCommandsLayoutBuilderExt::getStreamStride(void) const
{
    if (static_cast<bool>(m_manualStride))
        return *m_manualStride;
    return getStreamRange();
}

IndirectCommandsLayoutBuilderExt::InternalToken::InternalToken()
    : type(VK_INDIRECT_COMMANDS_TOKEN_TYPE_MAX_ENUM_EXT)
    , offset(std::numeric_limits<uint32_t>::max())
    , pPushConstant()
    , pVertexBuffer()
    , pIndexBuffer()
    , pExecutionSet()
{
}

VkIndirectCommandsLayoutTokenEXT IndirectCommandsLayoutBuilderExt::InternalToken::asVkToken(void) const
{
    VkIndirectCommandsTokenDataEXT tokenData;

    if (type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT)
        tokenData.pPushConstant = pPushConstant.get();
    else if (type == vk::VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT)
        tokenData.pPushConstant = pPushConstant.get();
    else if (type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT)
        tokenData.pVertexBuffer = pVertexBuffer.get();
    else if (type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT)
        tokenData.pIndexBuffer = pIndexBuffer.get();
    else if (type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT)
        tokenData.pExecutionSet = pExecutionSet.get();
    else
        deMemset(&tokenData, 0, sizeof(tokenData));

    const VkIndirectCommandsLayoutTokenEXT vkToken = {
        VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT, // VkStructureType sType;
        nullptr,                                              // void* pNext;
        type,                                                 // VkIndirectCommandsTokenTypeEXT type;
        tokenData,                                            // VkIndirectCommandsTokenDataEXT data;
        offset,                                               // uint32_t offset;
    };
    return vkToken;
}

namespace
{
bool isWorkProvokingToken(VkIndirectCommandsTokenTypeEXT token)
{
    bool isWorkProvoking = true;

    switch (token)
    {
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT:
        isWorkProvoking = false;
        break;
    default:
        break;
    }

    return isWorkProvoking;
}

uint32_t tokenDataSize(const VkIndirectCommandsLayoutTokenEXT &token)
{
    static constexpr uint32_t kU32Size  = 4u;
    static constexpr uint32_t kFlagBits = 32u;

    switch (token.type)
    {
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_EXECUTION_SET_EXT:
    {
        // When using pipelines, we only need 1 index. When using shader
        // objects, we need one index per stage indicated in the token.
        const auto indexCount =
            (token.data.pExecutionSet->type == VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT) ?
                1u :
                static_cast<uint32_t>(std::bitset<kFlagBits>(token.data.pExecutionSet->shaderStages).count());
        return kU32Size * indexCount;
    }
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT:
        return token.data.pPushConstant->updateRange.size;
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_INDEX_BUFFER_EXT:
        return DE_SIZEOF32(VkBindIndexBufferIndirectCommandEXT);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_VERTEX_BUFFER_EXT:
        return DE_SIZEOF32(VkBindVertexBufferIndirectCommandEXT);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT:
        return DE_SIZEOF32(VkDrawIndexedIndirectCommand);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_EXT:
        return DE_SIZEOF32(VkDrawIndirectCommand);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_COUNT_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_COUNT_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_NV_EXT:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_COUNT_EXT:
        // Note double indirection: the buffer specified here will contain different things for the different commands.
        return DE_SIZEOF32(VkDrawIndirectCountIndirectCommandEXT);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_EXT:
        return DE_SIZEOF32(VkDispatchIndirectCommand);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV_EXT:
        return DE_SIZEOF32(VkDrawMeshTasksIndirectCommandNV);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_EXT:
        return DE_SIZEOF32(VkDrawMeshTasksIndirectCommandEXT);
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_TRACE_RAYS2_EXT:
        return DE_SIZEOF32(VkTraceRaysIndirectCommand2KHR);
    default:
        break;
    }

    DE_ASSERT(false);
    return 0u;
}

} // namespace

uint32_t IndirectCommandsLayoutBuilderExt::getStreamRange(void) const
{
    uint32_t maxRange = 0u;
    std::for_each(begin(m_tokens), end(m_tokens),
                  [&maxRange](const InternalToken &token)
                  {
                      const auto vkToken = token.asVkToken();
                      const auto range   = vkToken.offset + tokenDataSize(vkToken);

                      if (maxRange < range)
                          maxRange = range;
                  });
    return maxRange;
}

Move<VkIndirectCommandsLayoutEXT> IndirectCommandsLayoutBuilderExt::build(const DeviceInterface &vkd, VkDevice device,
                                                                          const VkAllocationCallbacks *pAllocator) const
{
    // Make sure we only have a work-provoking token and it's the last one in the sequence.
    DE_ASSERT(!m_tokens.empty());
    DE_ASSERT(isWorkProvokingToken(m_tokens.back().type));

    const auto wpTokenCount = std::count_if(
        begin(m_tokens), end(m_tokens), [](const InternalToken &token) { return isWorkProvokingToken(token.type); });
    DE_UNREF(wpTokenCount); // For release builds.
    DE_ASSERT(wpTokenCount == 1u);

    // Transform internal tokens into Vulkan tokens.
    std::vector<VkIndirectCommandsLayoutTokenEXT> vkTokens;
    vkTokens.reserve(m_tokens.size());

    std::transform(begin(m_tokens), end(m_tokens), std::back_inserter(vkTokens),
                   [](const InternalToken &token) { return token.asVkToken(); });

    // We must pass the layout if needed.
    {
        const auto pipelineLayoutNeeded = [](const InternalToken &token)
        {
            return (token.type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT ||
                    token.type == VK_INDIRECT_COMMANDS_TOKEN_TYPE_SEQUENCE_INDEX_EXT);
        };
        DE_UNREF(pipelineLayoutNeeded); // For release builds.
        if (std::any_of(begin(m_tokens), end(m_tokens), pipelineLayoutNeeded))
            DE_ASSERT(m_layoutCreateInfoPtr != nullptr || m_pipelineLayout != VK_NULL_HANDLE);
    }
    // But we can't pass both at the same time.
    DE_ASSERT((m_layoutCreateInfoPtr == nullptr) || (m_pipelineLayout == VK_NULL_HANDLE));

    // Finally create the commands layout.
    const VkIndirectCommandsLayoutCreateInfoEXT createInfo = {
        VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT, // VkStructureType sType;
        m_layoutCreateInfoPtr,                                      // const void* pNext;
        m_layoutUsageFlags,                                         // VkIndirectCommandsLayoutUsageFlagsEXT flags;
        m_shaderStages,                                             // VkShaderStageFlags shaderStages;
        getStreamStride(),                                          // uint32_t indirectStride;
        m_pipelineLayout,                                           // VkPipelineLayout pipelineLayout;
        de::sizeU32(vkTokens),                                      // uint32_t tokenCount;
        de::dataOrNull(vkTokens),                                   // const VkIndirectCommandsLayoutTokenEXT* pTokens;
    };

    return createIndirectCommandsLayoutEXT(vkd, device, &createInfo, pAllocator);
}

PreprocessBufferExt::PreprocessBufferExt(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                         VkIndirectExecutionSetEXT indirectExecutionSet,
                                         VkIndirectCommandsLayoutEXT indirectCommandsLayout, uint32_t maxSequenceCount,
                                         uint32_t maxDrawCount, VkPipeline pipeline,
                                         const std::vector<vk::VkShaderEXT> *shaders, VkDeviceSize offset)
    : m_offset(offset)
    , m_buffer()
    , m_bufferAllocation()
    , m_size(0ull)
    , m_deviceAddress(0ull)
{
    const auto genCmdMemReqsInfo =
        DGCMemReqsInfo(indirectExecutionSet, indirectCommandsLayout, maxSequenceCount, maxDrawCount, pipeline, shaders);
    const auto origMemReqs = getGeneratedCommandsMemoryRequirementsExt(vkd, device, *genCmdMemReqsInfo);

    // Save the original required size. This is used by getSize() and others.
    m_size = origMemReqs.size;

    // Align the requested offset to a multiple of the required alignment.
    if (offset > 0ull)
        m_offset = de::roundUp(offset, origMemReqs.alignment);

    if (needed())
    {
        // Calculate total buffer size based on the requested size and offset.
        const VkDeviceSize preprocessSize = m_size + m_offset;

        const VkBufferUsageFlags2KHR bufferUsage =
            (VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_2_PREPROCESS_BUFFER_BIT_EXT);

        const VkBufferUsageFlags2CreateInfoKHR usageFlags2CreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                                // const void* pNext;
            bufferUsage,                                            // VkBufferUsageFlags2KHR usage;
        };

        const VkBufferCreateInfo preprocessBufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            &usageFlags2CreateInfo,               // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            preprocessSize,                       // VkDeviceSize size;
            0u,                                   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        m_buffer = createBuffer(vkd, device, &preprocessBufferCreateInfo);

        VkMemoryRequirements bufferMemReqs;
        vkd.getBufferMemoryRequirements(device, *m_buffer, &bufferMemReqs);

        // The buffer, created for preprocessing with the corresponding usage flags, should not have a required size
        // that's smaller than the original size.
        if (bufferMemReqs.size < preprocessSize)
            TCU_FAIL("DGC memory requirements size larger than preprocess buffer requirements size");

        // The buffer alignment requirement must not be lower than the DGC alignment requirement.
        if (bufferMemReqs.alignment < origMemReqs.alignment)
            TCU_FAIL("DGC alignment requirement larger than preprocess buffer alignment requirement");

        // Find the largest alignment of the two.
        bufferMemReqs.alignment = de::lcm(bufferMemReqs.alignment, origMemReqs.alignment);

        // Find the common memory types.
        bufferMemReqs.memoryTypeBits &= origMemReqs.memoryTypeBits;

        m_bufferAllocation = allocator.allocate(bufferMemReqs, MemoryRequirement::DeviceAddress);
        VK_CHECK(
            vkd.bindBufferMemory(device, *m_buffer, m_bufferAllocation->getMemory(), m_bufferAllocation->getOffset()));

        const VkBufferDeviceAddressInfo deviceAddressInfo = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            *m_buffer,                                    // VkBuffer buffer;
        };

        // Take the offset into account when calculating the base device address.
        m_deviceAddress = vkd.getBufferDeviceAddress(device, &deviceAddressInfo) + m_offset;
    }
}

VkDeviceAddress getBufferDeviceAddress(const DeviceInterface &vkd, VkDevice device, VkBuffer buffer)
{
    if (buffer == VK_NULL_HANDLE)
        return 0ull;

    const VkBufferDeviceAddressInfo deviceAddressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType    sType
        nullptr,                                      // const void*        pNext
        buffer                                        // VkBuffer           buffer;
    };
    return vkd.getBufferDeviceAddress(device, &deviceAddressInfo);
}

DGCBuffer::DGCBuffer(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                     const vk::VkDeviceSize size, const vk::VkBufferUsageFlags extraUsageFlags,
                     const vk::MemoryRequirement extraMemReqs)
    : m_size(size)
    , m_buffer(vk, device, allocator,
               makeBufferCreateInfo(size, (extraUsageFlags | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)),
               (MemoryRequirement::DeviceAddress | extraMemReqs))
    , m_address(getBufferDeviceAddress(vk, device, m_buffer.get()))
{
}

DGCComputePipelineExt::DGCComputePipelineExt(const DeviceInterface &vkd, VkDevice device,
                                             VkPipelineCreateFlags2KHR pipelineFlags, VkPipelineLayout pipelineLayout,
                                             VkPipelineShaderStageCreateFlags shaderStageCreateFlags,
                                             VkShaderModule module, const VkSpecializationInfo *specializationInfo,
                                             VkPipeline basePipelineHandle, int32_t basePipelineIndex,
                                             uint32_t subgroupSize)

    : m_pipeline()
{
    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroupSizeInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                                    // void* pNext;
        subgroupSize,                                                               // uint32_t requiredSubgroupSize;
    };

    const auto shaderPNext = (subgroupSize > 0u ? &subgroupSizeInfo : nullptr);

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        shaderPNext,                                         // const void* pNext;
        shaderStageCreateFlags,                              // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
        module,                                              // VkShaderModule module;
        "main",                                              // const char* pName;
        specializationInfo,                                  // const VkSpecializationInfo* pSpecializationInfo;
    };

    // Make sure the required flag is always passed.
    const auto creationFlags = (pipelineFlags | VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT);

    const VkPipelineCreateFlags2CreateInfoKHR pipelineFlagsCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        creationFlags,                                             // VkPipelineCreateFlags2KHR flags;
    };

    const VkComputePipelineCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        &pipelineFlagsCreateInfo,                       // const void* pNext;
        0u,                                             // VkPipelineCreateFlags flags;
        shaderStageCreateInfo,                          // VkPipelineShaderStageCreateInfo stage;
        pipelineLayout,                                 // VkPipelineLayout layout;
        basePipelineHandle,                             // VkPipeline basePipelineHandle;
        basePipelineIndex,                              // int32_t basePipelineIndex;
    };

    m_pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &createInfo);
}

VkPipeline DGCComputePipelineExt::get(void) const
{
    return *m_pipeline;
}
VkPipeline DGCComputePipelineExt::operator*(void) const
{
    return get();
}

DGCShaderExt::DGCShaderExt(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
                           vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                           const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                           const std::vector<vk::VkPushConstantRange> &pushConstantRanges, bool tessellationFeature,
                           bool geometryFeature, const vk::VkSpecializationInfo *specializationInfo, const void *pNext)

    : m_shader()
{
    init(vkd, device, stage, shaderFlags, shaderBinary, setLayouts, pushConstantRanges, tessellationFeature,
         geometryFeature, specializationInfo, pNext);
}

DGCShaderExt::DGCShaderExt(void) : m_shader()
{
}

void DGCShaderExt::init(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
                        vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                        const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                        const std::vector<vk::VkPushConstantRange> &pushConstantRanges, bool tessellationFeature,
                        bool geometryFeature, const vk::VkSpecializationInfo *specializationInfo, const void *pNext)
{
    if (shaderBinary.getFormat() != PROGRAM_FORMAT_SPIRV)
        TCU_THROW(InternalError, "Program format not supported");

    // Make sure not to forget the mandatory flag.
    const auto createFlags = (shaderFlags | VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT);

    VkShaderStageFlags nextStage = 0u;
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        if (tessellationFeature)
            nextStage |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (geometryFeature)
            nextStage |= VK_SHADER_STAGE_GEOMETRY_BIT;
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        DE_ASSERT(tessellationFeature);
        nextStage |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        DE_ASSERT(tessellationFeature);
        if (geometryFeature)
            nextStage |= VK_SHADER_STAGE_GEOMETRY_BIT;
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        DE_ASSERT(geometryFeature);
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        nextStage |= VK_SHADER_STAGE_MESH_BIT_EXT;
        break;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    default:
        break;
    }

    const VkShaderCreateInfoEXT shaderCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
        pNext,                                    // const void* pNext;
        createFlags,                              // VkShaderCreateFlagsEXT flags;
        stage,                                    // VkShaderStageFlagBits stage;
        nextStage,                                // VkShaderStageFlags nextStage;
        VK_SHADER_CODE_TYPE_SPIRV_EXT,            // VkShaderCodeTypeEXT codeType;
        shaderBinary.getSize(),                   // size_t codeSize;
        shaderBinary.getBinary(),                 // const void* pCode;
        "main",                                   // const char* pName;
        de::sizeU32(setLayouts),                  // uint32_t setLayoutCount;
        de::dataOrNull(setLayouts),               // const VkDescriptorSetLayout* pSetLayouts;
        de::sizeU32(pushConstantRanges),          // uint32_t pushConstantRangeCount;
        de::dataOrNull(pushConstantRanges),       // const VkPushConstantRange* pPushConstantRanges;
        specializationInfo,                       // const VkSpecializationInfo* pSpecializationInfo;
    };

    shaderBinary.setUsed();
    m_shader = createShader(vkd, device, shaderCreateInfo);
}

DGCComputeShaderExt::DGCComputeShaderExt(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                         vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                                         const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                                         const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
                                         const vk::VkSpecializationInfo *specializationInfo, uint32_t subgroupSize)

    : DGCShaderExt()
{
    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroupSizeInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                                    // void* pNext;
        subgroupSize,                                                               // uint32_t requiredSubgroupSize;
    };

    const auto pNext = (subgroupSize > 0u ? &subgroupSizeInfo : nullptr);

    init(vkd, device, VK_SHADER_STAGE_COMPUTE_BIT, shaderFlags, shaderBinary, setLayouts, pushConstantRanges, false,
         false, specializationInfo, pNext);
}

namespace
{

int32_t toDX12Format(VkIndexType indexType)
{
    // From https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format
    // DXGI_FORMAT_R32_UINT = 42,
    // DXGI_FORMAT_R16_UINT = 57,
    static constexpr int32_t kDXR32Uint = 42;
    static constexpr int32_t kDXR16Uint = 57;

    switch (indexType)
    {
    case VK_INDEX_TYPE_UINT32:
        return kDXR32Uint;
    case VK_INDEX_TYPE_UINT16:
        return kDXR16Uint;
    default:
        break;
    }

    DE_ASSERT(false);
    return 0;
}

} // anonymous namespace

IndexBufferViewD3D12::IndexBufferViewD3D12(vk::VkDeviceAddress address_, uint32_t size_, vk::VkIndexType indexType_)
    : bufferAddress(static_cast<uint64_t>(address_))
    , size(size_)
    , indexType(toDX12Format(indexType_))
{
}

void submitAndWaitWithPreprocess(const DeviceInterface &vkd, VkDevice device, VkQueue queue, VkCommandBuffer cmdBuffer,
                                 VkCommandBuffer preprocessCmdBuffer)
{
    const bool hasPreprocess = (preprocessCmdBuffer != VK_NULL_HANDLE);

    std::vector<VkSubmitInfo> submitInfos;
    submitInfos.reserve(2u); // (Optional) Preprocess and execute.

    Move<VkSemaphore> preprocessSemaphore;
    std::vector<VkSemaphore> signalWaitSemaphores;
    std::vector<VkPipelineStageFlags> waitStages;

    if (hasPreprocess)
    {
        preprocessSemaphore = createSemaphore(vkd, device);
        signalWaitSemaphores.push_back(*preprocessSemaphore);
        waitStages.push_back(VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

        submitInfos.push_back(VkSubmitInfo{
            VK_STRUCTURE_TYPE_SUBMIT_INFO,        // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // uint32_t waitSemaphoreCount;
            nullptr,                              // const VkSemaphore* pWaitSemaphores;
            nullptr,                              // const VkPipelineStageFlags* pWaitDstStageMask;
            1u,                                   // uint32_t commandBufferCount;
            &preprocessCmdBuffer,                 // const VkCommandBuffer* pCommandBuffers;
            de::sizeU32(signalWaitSemaphores),    // uint32_t signalSemaphoreCount;
            de::dataOrNull(signalWaitSemaphores), // const VkSemaphore* pSignalSemaphores;
        });
    }

    DE_ASSERT(signalWaitSemaphores.size() == waitStages.size());

    submitInfos.push_back(VkSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,        // VkStructureType sType;
        nullptr,                              // const void* pNext;
        de::sizeU32(signalWaitSemaphores),    // uint32_t waitSemaphoreCount;
        de::dataOrNull(signalWaitSemaphores), // const VkSemaphore* pWaitSemaphores;
        de::dataOrNull(waitStages),           // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                                   // uint32_t commandBufferCount;
        &cmdBuffer,                           // const VkCommandBuffer* pCommandBuffers;
        0u,                                   // uint32_t signalSemaphoreCount;
        nullptr,                              // const VkSemaphore* pSignalSemaphores;
    });

    const auto fence(createFence(vkd, device));
    VK_CHECK(vkd.queueSubmit(queue, de::sizeU32(submitInfos), de::dataOrNull(submitInfos), *fence));
    waitForFence(vkd, device, *fence);
}

} // namespace DGC
} // namespace vkt
