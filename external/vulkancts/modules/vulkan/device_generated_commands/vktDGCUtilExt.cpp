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

    if (needed())
    {
        // Take into account the requested offset when allocating memory and saving the device address.
        const VkMemoryRequirements allocationReqs = {m_size + offset, origMemReqs.alignment,
                                                     origMemReqs.memoryTypeBits};
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
            allocationReqs.size,                  // VkDeviceSize size;
            0u,                                   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            0u,                                   // uint32_t queueFamilyIndexCount;
            nullptr,                              // const uint32_t* pQueueFamilyIndices;
        };

        m_buffer           = createBuffer(vkd, device, &preprocessBufferCreateInfo);
        m_bufferAllocation = allocator.allocate(allocationReqs, MemoryRequirement::DeviceAddress);
        VK_CHECK(
            vkd.bindBufferMemory(device, *m_buffer, m_bufferAllocation->getMemory(), m_bufferAllocation->getOffset()));

        const VkBufferDeviceAddressInfo deviceAddressInfo = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            *m_buffer,                                    // VkBuffer buffer;
        };
        m_deviceAddress = vkd.getBufferDeviceAddress(device, &deviceAddressInfo) + offset;
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
                           const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
                           const vk::VkSpecializationInfo *specializationInfo, const void *pNext)

    : m_shader()
{
    init(vkd, device, stage, shaderFlags, shaderBinary, setLayouts, pushConstantRanges, specializationInfo, pNext);
}

DGCShaderExt::DGCShaderExt(void) : m_shader()
{
}

void DGCShaderExt::init(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
                        vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                        const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                        const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
                        const vk::VkSpecializationInfo *specializationInfo, const void *pNext)
{
    if (shaderBinary.getFormat() != PROGRAM_FORMAT_SPIRV)
        TCU_THROW(InternalError, "Program format not supported");

    // Make sure not to forget the mandatory flag.
    const auto createFlags = (shaderFlags | VK_SHADER_CREATE_INDIRECT_BINDABLE_BIT_EXT);

    const VkShaderCreateInfoEXT shaderCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, // VkStructureType sType;
        pNext,                                    // const void* pNext;
        createFlags,                              // VkShaderCreateFlagsEXT flags;
        stage,                                    // VkShaderStageFlagBits stage;
        0u,                                       // VkShaderStageFlags nextStage;
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

    init(vkd, device, VK_SHADER_STAGE_COMPUTE_BIT, shaderFlags, shaderBinary, setLayouts, pushConstantRanges,
         specializationInfo, pNext);
}

namespace
{

VkVertexInputBindingDescription2EXT makeVertexInputBindingDescription2(
    const VkVertexInputBindingDescription &description)
{
    const VkVertexInputBindingDescription2EXT desc2 = {
        VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, // VkStructureType sType;
        nullptr,                                                  // void* pNext;
        description.binding,                                      // uint32_t binding;
        description.stride,                                       // uint32_t stride;
        description.inputRate,                                    // VkVertexInputRate inputRate;
        1u,                                                       // uint32_t divisor;
    };
    return desc2;
}

VkVertexInputAttributeDescription2EXT makeVertexInputAttributeDescription2(
    const VkVertexInputAttributeDescription &description)
{
    const VkVertexInputAttributeDescription2EXT desc2 = {
        VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, // VkStructureType sType;
        nullptr,                                                    // void* pNext;
        description.location,                                       // uint32_t location;
        description.binding,                                        // uint32_t binding;
        description.format,                                         // VkFormat format;
        description.offset,                                         // uint32_t offset;
    };
    return desc2;
}

} // anonymous namespace

std::vector<std::string> getDeviceCreationExtensions(Context &context)
{
    const auto &extList = context.getDeviceCreationExtensions();
    std::vector<std::string> ret(begin(extList), end(extList));
    return ret;
}

void bindShaderObjectState(const DeviceInterface &vkd, const std::vector<std::string> &deviceExtensions,
                           const VkCommandBuffer cmdBuffer, const std::vector<VkViewport> &viewports,
                           const std::vector<VkRect2D> &scissors, const VkPrimitiveTopology topology,
                           const uint32_t patchControlPoints,
                           const VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
                           const VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                           const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                           const VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                           const VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo)
{
    if (vertexInputStateCreateInfo)
    {
        // This is not used with mesh shaders.
        const auto &srcBindingDescs = vertexInputStateCreateInfo->pVertexBindingDescriptions;
        const auto &srcBindingCount = vertexInputStateCreateInfo->vertexBindingDescriptionCount;

        const auto &srcAttributeDescs = vertexInputStateCreateInfo->pVertexAttributeDescriptions;
        const auto &srcAttributeCount = vertexInputStateCreateInfo->vertexAttributeDescriptionCount;

        std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
        bindingDescriptions.reserve(srcBindingCount);
        std::transform(srcBindingDescs, srcBindingDescs + srcBindingCount, std::back_inserter(bindingDescriptions),
                       [](const VkVertexInputBindingDescription &description)
                       { return makeVertexInputBindingDescription2(description); });

        std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;
        attributeDescriptions.reserve(srcAttributeCount);
        std::transform(srcAttributeDescs, srcAttributeDescs + srcAttributeCount,
                       std::back_inserter(attributeDescriptions),
                       [](const VkVertexInputAttributeDescription &description)
                       { return makeVertexInputAttributeDescription2(description); });

        vkd.cmdSetVertexInputEXT(cmdBuffer, de::sizeU32(bindingDescriptions), de::dataOrNull(bindingDescriptions),
                                 de::sizeU32(attributeDescriptions), de::dataOrNull(attributeDescriptions));
    }

    if (vertexInputStateCreateInfo)
    {
        // This is not used with mesh shaders.
        vkd.cmdSetPrimitiveTopology(cmdBuffer, topology);
        vkd.cmdSetPrimitiveRestartEnable(cmdBuffer, VK_FALSE);

        if (patchControlPoints > 0u)
        {
            vkd.cmdSetPatchControlPointsEXT(cmdBuffer, patchControlPoints);
            vkd.cmdSetTessellationDomainOriginEXT(cmdBuffer, VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT);
        }
    }

    {
        vkd.cmdSetViewportWithCount(cmdBuffer, de::sizeU32(viewports), de::dataOrNull(viewports));
        vkd.cmdSetScissorWithCount(cmdBuffer, de::sizeU32(scissors), de::dataOrNull(scissors));
    }

    {
        const auto depthClampEnable =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthClampEnable : VK_FALSE);
        const auto rasterizerDiscardEnable =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->rasterizerDiscardEnable : VK_FALSE);
        const auto polygonMode =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->polygonMode : VK_POLYGON_MODE_FILL);
        const auto cullMode = (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->cullMode :
                                                              static_cast<VkCullModeFlags>(VK_CULL_MODE_NONE));
        const auto frontFace =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->frontFace : VK_FRONT_FACE_COUNTER_CLOCKWISE);
        const auto depthBiasEnable =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasEnable : VK_FALSE);
        const auto depthBiasConstantFactor =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasConstantFactor : 0.0f);
        const auto depthBiasClamp =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasClamp : 0.0f);
        const auto depthBiasSlopeFactor =
            (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->depthBiasSlopeFactor : 0.0f);
        const auto lineWidth = (rasterizationStateCreateInfo ? rasterizationStateCreateInfo->lineWidth : 1.0f);

        vkd.cmdSetDepthClampEnableEXT(cmdBuffer, depthClampEnable);
        vkd.cmdSetRasterizerDiscardEnable(cmdBuffer, rasterizerDiscardEnable);
        vkd.cmdSetPolygonModeEXT(cmdBuffer, polygonMode);
        vkd.cmdSetCullMode(cmdBuffer, cullMode);
        vkd.cmdSetFrontFace(cmdBuffer, frontFace);
        vkd.cmdSetDepthBiasEnable(cmdBuffer, depthBiasEnable);
        vkd.cmdSetDepthBias(cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        vkd.cmdSetLineWidth(cmdBuffer, lineWidth);
    }

    {
        const auto rasterizationSamples =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->rasterizationSamples : VK_SAMPLE_COUNT_1_BIT);
        const auto defaultSampleMask = 0xFFFFFFFFu;
        const auto pSampleMask =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->pSampleMask : &defaultSampleMask);
        const auto alphaToCoverageEnable =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->alphaToCoverageEnable : VK_FALSE);
        const auto alphaToOneEnable =
            (multisampleStateCreateInfo ? multisampleStateCreateInfo->alphaToOneEnable : VK_FALSE);

        vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, rasterizationSamples);
        vkd.cmdSetSampleMaskEXT(cmdBuffer, rasterizationSamples, pSampleMask);
        vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, alphaToCoverageEnable);
        vkd.cmdSetAlphaToOneEnableEXT(cmdBuffer, alphaToOneEnable);
    }

    {
        const auto defaultStencilOp = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                                         VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

        const auto depthTestEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthTestEnable : VK_FALSE);
        const auto depthWriteEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthWriteEnable : VK_FALSE);
        const auto depthCompareOp =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthCompareOp : VK_COMPARE_OP_NEVER);
        const auto depthBoundsTestEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->depthBoundsTestEnable : VK_FALSE);
        const auto stencilTestEnable =
            (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->stencilTestEnable : VK_FALSE);
        const auto stencilFront = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->front : defaultStencilOp);
        const auto stencilBack  = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->back : defaultStencilOp);
        const auto minDepthBounds = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->minDepthBounds : 0.0f);
        const auto maxDepthBounds = (depthStencilStateCreateInfo ? depthStencilStateCreateInfo->maxDepthBounds : 0.0f);

        vkd.cmdSetDepthTestEnable(cmdBuffer, depthTestEnable);
        vkd.cmdSetDepthWriteEnable(cmdBuffer, depthWriteEnable);
        vkd.cmdSetDepthCompareOp(cmdBuffer, depthCompareOp);
        vkd.cmdSetDepthBoundsTestEnable(cmdBuffer, depthBoundsTestEnable);
        vkd.cmdSetStencilTestEnable(cmdBuffer, stencilTestEnable);

        vkd.cmdSetStencilOp(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.failOp, stencilFront.passOp,
                            stencilFront.depthFailOp, stencilFront.compareOp);
        vkd.cmdSetStencilCompareMask(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.compareMask);
        vkd.cmdSetStencilWriteMask(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.writeMask);
        vkd.cmdSetStencilReference(cmdBuffer, VK_STENCIL_FACE_FRONT_BIT, stencilFront.reference);

        vkd.cmdSetStencilOp(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.failOp, stencilBack.passOp,
                            stencilBack.depthFailOp, stencilBack.compareOp);
        vkd.cmdSetStencilCompareMask(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.compareMask);
        vkd.cmdSetStencilWriteMask(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.writeMask);
        vkd.cmdSetStencilReference(cmdBuffer, VK_STENCIL_FACE_BACK_BIT, stencilBack.reference);

        vkd.cmdSetDepthBounds(cmdBuffer, minDepthBounds, maxDepthBounds);
    }

    {
        const auto logicOpEnable = (colorBlendStateCreateInfo ? colorBlendStateCreateInfo->logicOpEnable : VK_FALSE);
        const auto logicOp       = (colorBlendStateCreateInfo ? colorBlendStateCreateInfo->logicOp : VK_LOGIC_OP_CLEAR);

        vkd.cmdSetLogicOpEnableEXT(cmdBuffer, logicOpEnable);
        vkd.cmdSetLogicOpEXT(cmdBuffer, logicOp);

        std::vector<VkBool32> colorWriteEnables;
        std::vector<VkColorComponentFlags> colorWriteMasks;
        std::vector<VkBool32> colorBlendEnables;
        std::vector<VkColorBlendEquationEXT> colorBlendEquations;

        if (!colorBlendStateCreateInfo)
        {
            const auto defaultWriteMask = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
            const auto defaultBlendEq =
                makeColorBlendEquationEXT(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
                                          VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);

            colorWriteEnables.push_back(VK_TRUE);
            colorWriteMasks.push_back(defaultWriteMask);
            colorBlendEnables.push_back(VK_FALSE);
            colorBlendEquations.push_back(defaultBlendEq);
        }
        else
        {
            const auto &attCount     = colorBlendStateCreateInfo->attachmentCount;
            const auto &pAttachments = colorBlendStateCreateInfo->pAttachments;

            colorWriteEnables.reserve(attCount);
            colorWriteMasks.reserve(attCount);
            colorBlendEnables.reserve(attCount);
            colorBlendEquations.reserve(attCount);

            colorWriteEnables = std::vector<VkBool32>(attCount, VK_TRUE);

            std::transform(pAttachments, pAttachments + attCount, std::back_inserter(colorWriteMasks),
                           [](const VkPipelineColorBlendAttachmentState &attState) { return attState.colorWriteMask; });

            std::transform(pAttachments, pAttachments + attCount, std::back_inserter(colorBlendEnables),
                           [](const VkPipelineColorBlendAttachmentState &attState) { return attState.blendEnable; });

            std::transform(pAttachments, pAttachments + attCount, std::back_inserter(colorBlendEquations),
                           [](const VkPipelineColorBlendAttachmentState &attState)
                           {
                               return makeColorBlendEquationEXT(
                                   attState.srcColorBlendFactor, attState.dstColorBlendFactor, attState.colorBlendOp,
                                   attState.srcAlphaBlendFactor, attState.dstAlphaBlendFactor, attState.alphaBlendOp);
                           });
        }

        vkd.cmdSetColorWriteEnableEXT(cmdBuffer, de::sizeU32(colorWriteEnables), de::dataOrNull(colorWriteEnables));
        vkd.cmdSetColorWriteMaskEXT(cmdBuffer, 0u, de::sizeU32(colorWriteMasks), de::dataOrNull(colorWriteMasks));
        vkd.cmdSetColorBlendEnableEXT(cmdBuffer, 0u, de::sizeU32(colorBlendEnables), de::dataOrNull(colorBlendEnables));
        vkd.cmdSetColorBlendEquationEXT(cmdBuffer, 0u, de::sizeU32(colorBlendEquations),
                                        de::dataOrNull(colorBlendEquations));
    }

    // Extra states with default values depending on enabled extensions.
    const auto extraDynStates = getShaderObjectDynamicStatesFromExtensions(deviceExtensions);
    for (const auto dynState : extraDynStates)
    {
        switch (dynState)
        {
        case VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT:
            vkd.cmdSetRasterizationStreamEXT(cmdBuffer, 0u);
            break;
        case VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT:
            break;
        case VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT:
            vkd.cmdSetConservativeRasterizationModeEXT(cmdBuffer, VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV:
            vkd.cmdSetCoverageModulationModeNV(cmdBuffer, VK_COVERAGE_MODULATION_MODE_NONE_NV);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV:
            vkd.cmdSetCoverageModulationTableEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV:
            // CoverageModulationTableEnable is false, so we can skip this.
            //vkd.cmdSetCoverageModulationTableNV(cmdBuffer, 0u, nullptr);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV:
            vkd.cmdSetCoverageReductionModeNV(cmdBuffer, VK_COVERAGE_REDUCTION_MODE_MERGE_NV);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV:
            vkd.cmdSetCoverageToColorEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV:
            vkd.cmdSetCoverageToColorLocationNV(cmdBuffer, 0u);
            break;
        case VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT:
            vkd.cmdSetDepthClipEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT:
            vkd.cmdSetDepthClipNegativeOneToOneEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
            break;
        case VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT:
            vkd.cmdSetExtraPrimitiveOverestimationSizeEXT(cmdBuffer, 0.0f);
            break;
        case VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT:
            vkd.cmdSetLineRasterizationModeEXT(cmdBuffer, VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR);
            break;
        case VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT:
            vkd.cmdSetLineStippleEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
            break;
        case VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT:
            vkd.cmdSetProvokingVertexModeEXT(cmdBuffer, VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT);
            break;
        case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
        {
            const auto fsrSize                                      = makeExtent2D(1u, 1u);
            const VkFragmentShadingRateCombinerOpKHR combinerOps[2] = {VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
                                                                       VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR};
            vkd.cmdSetFragmentShadingRateKHR(cmdBuffer, &fsrSize, combinerOps);
        }
        break;
        case VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV:
            vkd.cmdSetRepresentativeFragmentTestEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT:
            vkd.cmdSetSampleLocationsEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
            break;
        case VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV:
        {
            const VkViewportSwizzleNV defaultSwizzle{
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV,
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV,
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV,
                VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV,
            };
            const std::vector<vk::VkViewportSwizzleNV> idSwizzles(viewports.size(), defaultSwizzle);
            vkd.cmdSetViewportSwizzleNV(cmdBuffer, 0u, de::sizeU32(idSwizzles), de::dataOrNull(idSwizzles));
        }
        break;
        case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV:
            vkd.cmdSetViewportWScalingEnableNV(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV:
            break;
        case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV:
        {
            const VkBool32 enable = VK_FALSE;
            vkd.cmdSetExclusiveScissorEnableNV(cmdBuffer, 0u, 1u, &enable);
        }
        break;
        case VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV:
            break;
        case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT:
            vkd.cmdSetDiscardRectangleEnableEXT(cmdBuffer, VK_FALSE);
            break;
        case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT:
            break;
        case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT:
            break;
        case VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT:
            vkd.cmdSetAttachmentFeedbackLoopEnableEXT(cmdBuffer, 0u);
            break;
        case VK_DYNAMIC_STATE_DEPTH_CLAMP_RANGE_EXT:
            vkd.cmdSetDepthClampRangeEXT(cmdBuffer, VK_DEPTH_CLAMP_MODE_VIEWPORT_RANGE_EXT, nullptr);
            break;
        default:
            DE_ASSERT(false);
            break;
        }
    }
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
