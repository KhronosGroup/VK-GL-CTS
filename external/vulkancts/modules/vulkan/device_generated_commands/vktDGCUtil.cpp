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
 * \brief Device Generated Commands Utility Code
 *//*--------------------------------------------------------------------*/
#include "vktDGCUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"

#include <algorithm>
#include <map>

namespace vkt
{
namespace DGC
{

using namespace vk;

void checkDGCSupport(Context &context)
{
    context.requireDeviceFunctionality("VK_NV_device_generated_commands");
}

void checkDGCComputeSupport(Context &context, bool requirePipelines, bool requireCaptureReplay)
{
    context.requireDeviceFunctionality("VK_NV_device_generated_commands_compute");

    if (requirePipelines || requireCaptureReplay)
    {
        const auto &features = context.getDeviceGeneratedCommandsComputeFeaturesNV();

        if (requirePipelines && !features.deviceGeneratedComputePipelines)
            TCU_THROW(NotSupportedError, "deviceGeneratedComputePipelines not supported");

        if (requireCaptureReplay && !features.deviceGeneratedComputeCaptureReplay)
            TCU_THROW(NotSupportedError, "deviceGeneratedComputeCaptureReplay not supported");
    }
}

VkPipelineIndirectDeviceAddressInfoNV makePipelineIndirectDeviceAddressInfoNV(VkPipeline pipeline)
{
    const vk::VkPipelineIndirectDeviceAddressInfoNV info = {
        VK_STRUCTURE_TYPE_PIPELINE_INDIRECT_DEVICE_ADDRESS_INFO_NV, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        VK_PIPELINE_BIND_POINT_COMPUTE,                             // VkPipelineBindPoint pipelineBindPoint;
        pipeline,                                                   // VkPipeline pipeline;
    };
    return info;
}

VkGeneratedCommandsMemoryRequirementsInfoNV makeGeneratedCommandsMemoryRequirementsInfoNV(
    VkPipelineBindPoint bindPoint, VkPipeline pipeline, VkIndirectCommandsLayoutNV cmdLayout,
    uint32_t maxSequencesCount)
{
    const VkGeneratedCommandsMemoryRequirementsInfoNV info = {
        VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV, // VkStructureType sType;
        nullptr,                                                          // const void* pNext;
        bindPoint,                                                        // VkPipelineBindPoint pipelineBindPoint;
        pipeline,                                                         // VkPipeline pipeline;
        cmdLayout,         // VkIndirectCommandsLayoutNV indirectCommandsLayout;
        maxSequencesCount, // uint32_t maxSequencesCount;
    };
    return info;
}

VkMemoryRequirements getPipelineIndirectMemoryRequirementsNV(const DeviceInterface &vkd, VkDevice device,
                                                             const VkComputePipelineCreateInfo *createInfo)
{
    VkMemoryRequirements2 memReqs = initVulkanStructure();
    vkd.getPipelineIndirectMemoryRequirementsNV(device, createInfo, &memReqs);
    return memReqs.memoryRequirements;
}

VkMemoryRequirements getGeneratedCommandsMemoryRequirementsNV(
    const DeviceInterface &vkd, VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV *memReqsInfo)
{
    VkMemoryRequirements2 memReqs = initVulkanStructure();
    vkd.getGeneratedCommandsMemoryRequirementsNV(device, memReqsInfo, &memReqs);
    return memReqs.memoryRequirements;
}

bool equalMemoryRequirements(const VkMemoryRequirements &a, const VkMemoryRequirements &b)
{
    return (a.memoryTypeBits == b.memoryTypeBits && a.alignment == b.alignment && a.size == b.size);
}

void preprocessToExecuteBarrier(const vk::DeviceInterface &vkd, vk::VkCommandBuffer cmdBuffer)
{
    const auto preExecutionBarrier =
        makeMemoryBarrier(VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
    cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                             VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, &preExecutionBarrier);
}

void metadataUpdateToPreprocessBarrier(const DeviceInterface &vkd, VkCommandBuffer cmdBuffer)
{
    const auto barrier =
        makeMemoryBarrier2(VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                           VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV, VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV);
    VkDependencyInfo dependency   = initVulkanStructure();
    dependency.memoryBarrierCount = 1u;
    dependency.pMemoryBarriers    = &barrier;
    vkd.cmdPipelineBarrier2(cmdBuffer, &dependency);
}

DGCComputePipelineMetaDataPool::MetaDataLocation DGCComputePipelineMetaDataPool::getMetaDataLocation(
    const DeviceInterface &vkd, VkDevice device, Allocator &allocator, const VkComputePipelineCreateInfo &createInfo)
{
    const auto memReqs = getPipelineIndirectMemoryRequirementsNV(vkd, device, &createInfo);

    auto &bufferInfoPtr = m_bufferInfos[device];

    if (!bufferInfoPtr)
    {
        bufferInfoPtr.reset(new BufferInfo());
        auto &bufferInfo       = *bufferInfoPtr;
        auto allocationMemReqs = memReqs;
        allocationMemReqs.size *= m_multiplier;
        bufferInfo.allocationSize   = allocationMemReqs.size;
        const auto bufferUsage      = (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                  VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        const auto bufferCreateInfo = makeBufferCreateInfo(bufferInfo.allocationSize, bufferUsage);
        const auto crMemReq =
            (m_captureReplay ? MemoryRequirement::DeviceAddressCaptureReplay : MemoryRequirement::Any);
        bufferInfo.buffer     = makeBuffer(vkd, device, bufferCreateInfo);
        bufferInfo.allocation = allocator.allocate(allocationMemReqs, (MemoryRequirement::DeviceAddress | crMemReq));
        VK_CHECK(vkd.bindBufferMemory(device, *bufferInfo.buffer, bufferInfo.allocation->getMemory(),
                                      bufferInfo.allocation->getOffset()));

        const VkBufferDeviceAddressInfo deviceAddressInfo = {
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType sType;
            nullptr,                                      // const void* pNext;
            *bufferInfo.buffer,                           // VkBuffer buffer;
        };
        bufferInfo.address = vkd.getBufferDeviceAddress(device, &deviceAddressInfo);
    }

    auto &bufferInfo = *bufferInfoPtr;

    const auto dataOffset = de::roundUp(bufferInfo.nextOffset, memReqs.alignment);
    const auto dataSize   = memReqs.size;

    bufferInfo.nextOffset = dataOffset + dataSize;

    // If this assert fails it means we are trying to create too many pipelines and they are overflowing the pool. Try increasing
    // the multiplier for the pool instance. Also see m_requestCount.
    DE_ASSERT(bufferInfo.nextOffset <= bufferInfo.allocationSize);

    const MetaDataLocation dataLocation{bufferInfo.address + dataOffset, dataSize};
    ++m_requestCount;
    return dataLocation;
}

DGCComputePipeline::DGCComputePipeline(DGCComputePipelineMetaDataPool &metaDataPool, const DeviceInterface &vkd,
                                       VkDevice device, Allocator &allocator, VkPipelineCreateFlags pipelineFlags,
                                       VkPipelineLayout pipelineLayout, VkPipelineShaderStageCreateFlags shaderFlags,
                                       VkShaderModule module, const VkSpecializationInfo *specializationInfo,
                                       vk::VkDeviceAddress pipelineDeviceAddressCaptureReplay,
                                       VkPipeline basePipelineHandle, int32_t basePipelineIndex, uint32_t subgroupSize)

    : m_vkd(vkd)
    , m_device(device)
    , m_pipeline()
    , m_metaDataLocation()
    , m_indirectDeviceAddress(0ull)
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
        shaderFlags,                                         // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
        module,                                              // VkShaderModule module;
        "main",                                              // const char* pName;
        specializationInfo,                                  // const VkSpecializationInfo* pSpecializationInfo;
    };

    // Make sure the required flag is always passed.
    const auto creationFlags = (pipelineFlags | VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV);

    VkComputePipelineCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        creationFlags,                                  // VkPipelineCreateFlags flags;
        shaderStageCreateInfo,                          // VkPipelineShaderStageCreateInfo stage;
        pipelineLayout,                                 // VkPipelineLayout layout;
        basePipelineHandle,                             // VkPipeline basePipelineHandle;
        basePipelineIndex,                              // int32_t basePipelineIndex;
    };

    m_metaDataLocation = metaDataPool.getMetaDataLocation(vkd, device, allocator, createInfo);

    // Create pipeline.
    const VkComputePipelineIndirectBufferInfoNV metaDataBufferInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_INDIRECT_BUFFER_INFO_NV, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        m_metaDataLocation.address,                                 // VkDeviceAddress deviceAddress;
        m_metaDataLocation.size,                                    // VkDeviceSize size;
        pipelineDeviceAddressCaptureReplay, // VkDeviceAddress pipelineDeviceAddressCaptureReplay;
    };
    createInfo.pNext = &metaDataBufferInfo;
    m_pipeline       = createComputePipeline(vkd, device, VK_NULL_HANDLE, &createInfo);

    // Save pipeline indirect device address.
    const auto pipelineIndirectDeviceAddressInfo = makePipelineIndirectDeviceAddressInfoNV(*m_pipeline);
    m_indirectDeviceAddress = vkd.getPipelineIndirectDeviceAddressNV(device, &pipelineIndirectDeviceAddressInfo);
}

VkPipeline DGCComputePipeline::get(void) const
{
    return *m_pipeline;
}
VkPipeline DGCComputePipeline::operator*(void) const
{
    return get();
}
VkDeviceAddress DGCComputePipeline::getIndirectDeviceAddress(void) const
{
    return m_indirectDeviceAddress;
}

IndirectCommandsLayoutBuilder::IndirectCommandsLayoutBuilder(VkIndirectCommandsLayoutUsageFlagsNV flags,
                                                             VkPipelineBindPoint bindPoint)
    : m_layoutUsageFlags(flags)
    , m_bindPoint(bindPoint)
    , m_tokens()
    , m_manualStrides()
{
    // We don't support anything else currently.
    DE_ASSERT(bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE);
}

void IndirectCommandsLayoutBuilder::addPushConstantToken(uint32_t stream, uint32_t offset,
                                                         VkPipelineLayout pipelineLayout, VkShaderStageFlags stageFlags,
                                                         uint32_t pcOffset, uint32_t pcSize)
{
    m_tokens.emplace_back(VkIndirectCommandsLayoutTokenNV{
        VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV,    // VkIndirectCommandsTokenTypeNV tokenType;
        stream,                                              // uint32_t stream;
        offset,                                              // uint32_t offset;
        0u,                                                  // uint32_t vertexBindingUnit;
        VK_FALSE,                                            // VkBool32 vertexDynamicStride;
        pipelineLayout,                                      // VkPipelineLayout pushconstantPipelineLayout;
        stageFlags,                                          // VkShaderStageFlags pushconstantShaderStageFlags;
        pcOffset,                                            // uint32_t pushconstantOffset;
        pcSize,                                              // uint32_t pushconstantSize;
        0u,                                                  // VkIndirectStateFlagsNV indirectStateFlags;
        0u,                                                  // uint32_t indexTypeCount;
        nullptr,                                             // const VkIndexType* pIndexTypes;
        nullptr,                                             // const uint32_t* pIndexTypeValues;
    });
}

void IndirectCommandsLayoutBuilder::addPipelineToken(uint32_t stream, uint32_t offset)
{
    m_tokens.emplace_back(VkIndirectCommandsLayoutTokenNV{
        VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        VK_INDIRECT_COMMANDS_TOKEN_TYPE_PIPELINE_NV,         // VkIndirectCommandsTokenTypeNV tokenType;
        stream,                                              // uint32_t stream;
        offset,                                              // uint32_t offset;
        0u,                                                  // uint32_t vertexBindingUnit;
        VK_FALSE,                                            // VkBool32 vertexDynamicStride;
        VK_NULL_HANDLE,                                      // VkPipelineLayout pushconstantPipelineLayout;
        0u,                                                  // VkShaderStageFlags pushconstantShaderStageFlags;
        0u,                                                  // uint32_t pushconstantOffset;
        0u,                                                  // uint32_t pushconstantSize;
        0u,                                                  // VkIndirectStateFlagsNV indirectStateFlags;
        0u,                                                  // uint32_t indexTypeCount;
        nullptr,                                             // const VkIndexType* pIndexTypes;
        nullptr,                                             // const uint32_t* pIndexTypeValues;
    });
}

void IndirectCommandsLayoutBuilder::addDispatchToken(uint32_t stream, uint32_t offset)
{
    m_tokens.emplace_back(VkIndirectCommandsLayoutTokenNV{
        VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV,         // VkIndirectCommandsTokenTypeNV tokenType;
        stream,                                              // uint32_t stream;
        offset,                                              // uint32_t offset;
        0u,                                                  // uint32_t vertexBindingUnit;
        VK_FALSE,                                            // VkBool32 vertexDynamicStride;
        VK_NULL_HANDLE,                                      // VkPipelineLayout pushconstantPipelineLayout;
        0u,                                                  // VkShaderStageFlags pushconstantShaderStageFlags;
        0u,                                                  // uint32_t pushconstantOffset;
        0u,                                                  // uint32_t pushconstantSize;
        0u,                                                  // VkIndirectStateFlagsNV indirectStateFlags;
        0u,                                                  // uint32_t indexTypeCount;
        nullptr,                                             // const VkIndexType* pIndexTypes;
        nullptr,                                             // const uint32_t* pIndexTypeValues;
    });
}

void IndirectCommandsLayoutBuilder::setStreamStride(uint32_t stream, uint32_t stride)
{
    // Save the manual stride for later use.
    m_manualStrides[stream] = stride;
}

namespace
{
bool isWorkProvokingToken(VkIndirectCommandsTokenTypeNV token)
{
    bool isWorkProvoking = false;

    switch (token)
    {
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_NV:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_TASKS_NV:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_MESH_TASKS_NV:
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV:
        isWorkProvoking = true;
        break;
    default:
        break;
    }

    return isWorkProvoking;
}

uint32_t tokenDataSize(const VkIndirectCommandsLayoutTokenNV &token)
{
    switch (token.tokenType)
    {
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PIPELINE_NV:
        return static_cast<uint32_t>(sizeof(VkBindPipelineIndirectCommandNV));
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV:
        return token.pushconstantSize;
    case VK_INDIRECT_COMMANDS_TOKEN_TYPE_DISPATCH_NV:
        return static_cast<uint32_t>(sizeof(VkDispatchIndirectCommand));
    default:
        break;
    }

    DE_ASSERT(false);
    return 0u;
}

} // namespace

uint32_t IndirectCommandsLayoutBuilder::getStreamCount(void) const
{
    // For the stream count, we'll find the highest stream index in the array of tokens.
    uint32_t highestStreamIndex = 0u;
    std::for_each(begin(m_tokens), end(m_tokens),
                  [&highestStreamIndex](const VkIndirectCommandsLayoutTokenNV &token)
                  {
                      if (token.stream > highestStreamIndex)
                          highestStreamIndex = token.stream;
                  });
    const uint32_t streamCount = highestStreamIndex + 1u;
    return streamCount;
}

uint32_t IndirectCommandsLayoutBuilder::getStreamRange(uint32_t stream) const
{
    uint32_t maxRange = 0u;
    std::for_each(begin(m_tokens), end(m_tokens),
                  [&maxRange, stream](const VkIndirectCommandsLayoutTokenNV &token)
                  {
                      if (token.stream == stream)
                      {
                          const auto range = token.offset + tokenDataSize(token);
                          if (maxRange < range)
                              maxRange = range;
                      }
                  });
    return maxRange;
}

Move<VkIndirectCommandsLayoutNV> IndirectCommandsLayoutBuilder::build(const DeviceInterface &vkd, VkDevice device,
                                                                      const vk::VkAllocationCallbacks *pAllocator) const
{
    // Make sure we only have a work-provoking token and it's the last one in the sequence.
    DE_ASSERT(!m_tokens.empty());
    DE_ASSERT(isWorkProvokingToken(m_tokens.back().tokenType));

    const auto wpTokenCount = std::count_if(begin(m_tokens), end(m_tokens),
                                            [](const VkIndirectCommandsLayoutTokenNV &token)
                                            { return isWorkProvokingToken(token.tokenType); });
    DE_UNREF(wpTokenCount); // For release builds.
    DE_ASSERT(wpTokenCount == 1u);

    const uint32_t streamCount = getStreamCount();

    // For the stream strides, we'll find, for each stream, the largest sum of offset + dataSize, where offset comes from the token
    // and dataSize is known per-token and depends on the token type. That will be the stream stride.
    //
    // Then, if the user has set a manual stride for some streams, we'll replace the calculated ones with the manual ones.
    std::vector<uint32_t> strides(static_cast<size_t>(streamCount), 0u);
    std::for_each(begin(m_tokens), end(m_tokens),
                  [&strides](const VkIndirectCommandsLayoutTokenNV &token)
                  {
                      const auto range = token.offset + tokenDataSize(token);
                      if (strides.at(token.stream) < range)
                          strides.at(token.stream) = range;
                  });

    for (const auto &manualStride : m_manualStrides)
    {
        // stream = manualStride.first; stride = manualStride.second
        strides.at(manualStride.first) = manualStride.second;
    }

    // Finally create the commands layout.
    const VkIndirectCommandsLayoutCreateInfoNV createInfo = {
        VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        m_layoutUsageFlags,                                        // VkIndirectCommandsLayoutUsageFlagsNV flags;
        m_bindPoint,                                               // VkPipelineBindPoint pipelineBindPoint;
        de::sizeU32(m_tokens),                                     // uint32_t tokenCount;
        de::dataOrNull(m_tokens),                                  // const VkIndirectCommandsLayoutTokenNV* pTokens;
        de::sizeU32(strides),                                      // uint32_t streamCount;
        de::dataOrNull(strides)                                    // const uint32_t* pStreamStrides;
    };

    return createIndirectCommandsLayoutNV(vkd, device, &createInfo, pAllocator);
}

PreprocessBuffer::PreprocessBuffer(const DeviceInterface &vkd, VkDevice device, Allocator &allocator,
                                   VkPipelineBindPoint bindPoint, VkPipeline pipeline,
                                   VkIndirectCommandsLayoutNV cmdLayout, uint32_t maxSequences)
    : m_buffer()
    , m_bufferAllocation()
    , m_memReqs()
{
    const auto genCmdMemReqsInfo =
        makeGeneratedCommandsMemoryRequirementsInfoNV(bindPoint, pipeline, cmdLayout, maxSequences);
    m_memReqs = getGeneratedCommandsMemoryRequirementsNV(vkd, device, &genCmdMemReqsInfo);

    const auto preprocessBufferCreateInfo = makeBufferCreateInfo(m_memReqs.size, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    m_buffer                              = createBuffer(vkd, device, &preprocessBufferCreateInfo);
    m_bufferAllocation                    = allocator.allocate(m_memReqs, MemoryRequirement::Any);
    VK_CHECK(vkd.bindBufferMemory(device, *m_buffer, m_bufferAllocation->getMemory(), m_bufferAllocation->getOffset()));
}

} // namespace DGC
} // namespace vkt
