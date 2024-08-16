#ifndef _VKTDGCUTIL_HPP
#define _VKTDGCUTIL_HPP
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

#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"

#include <cstdint>
#include <vector>
#include <map>
#include <memory>

namespace vkt
{
namespace DGC
{

void checkDGCSupport(Context &context);
void checkDGCComputeSupport(Context &context, bool requirePipelines, bool requireCaptureReplay);

vk::VkPipelineIndirectDeviceAddressInfoNV makePipelineIndirectDeviceAddressInfoNV(vk::VkPipeline);
vk::VkGeneratedCommandsMemoryRequirementsInfoNV makeGeneratedCommandsMemoryRequirementsInfoNV(
    vk::VkPipelineBindPoint, vk::VkPipeline, vk::VkIndirectCommandsLayoutNV, uint32_t);

// These are useful because it's easy to forget to initialize the sType and pNext members of VkMemoryRequirements2.
vk::VkMemoryRequirements getPipelineIndirectMemoryRequirementsNV(const vk::DeviceInterface &, vk::VkDevice,
                                                                 const vk::VkComputePipelineCreateInfo *);
vk::VkMemoryRequirements getGeneratedCommandsMemoryRequirementsNV(
    const vk::DeviceInterface &, vk::VkDevice, const vk::VkGeneratedCommandsMemoryRequirementsInfoNV *);

// Returns true if the two memory requirements structures are equal.
bool equalMemoryRequirements(const vk::VkMemoryRequirements &, const vk::VkMemoryRequirements &);

// Insert a memory barrier from the preprocessing stage to the execution stage.
void preprocessToExecuteBarrier(const vk::DeviceInterface &, vk::VkCommandBuffer);

// Metadata update to preprocess barrier.
void metadataUpdateToPreprocessBarrier(const vk::DeviceInterface &, vk::VkCommandBuffer);

// Class that manages a memory pool for pipeline metadata.
//
// When creating a large number of pipelines for DGC use, this pool avoids creating one allocation per pipeline. Since
// maxMemoryAllocationCount has a minimum value of just 4096 according to the spec, it wouldn't be that hard to reach such a limit
// depending on the number of pipelines.
//
// The recommendation is to use one pool per indirect commands layout, so all pipelines created to be used with the same commands
// layout have similar memory requirements (size, types, etc) and the multiplier factor used in the constructor works as a better
// prediction of how many piplines the pool will be able to hold.
//
// In practice, we've observed pipeline metadata to take <1KB of memory, so creating a pool with space for, more or less, 1024
// pipelines (the default) should only allocate <1MB of memory.
class DGCComputePipelineMetaDataPool
{
public:
    static constexpr uint32_t kDefaultMultiplier = 1024u;

    // The multplier argument roughly tells us how many pipelines we can potentially store (this is just an approximation, since
    // it's based on the first allocated pipeline and requirements vary per pipeline).
    DGCComputePipelineMetaDataPool(uint32_t multiplier = kDefaultMultiplier, bool captureReplay = false)
        : m_bufferInfos()
        , m_multiplier(multiplier)
        , m_captureReplay(captureReplay)
        , m_requestCount(0u)
    {
    }

    struct MetaDataLocation
    {
        vk::VkDeviceAddress address;
        vk::VkDeviceSize size;

        MetaDataLocation(vk::VkDeviceAddress address_ = 0ull, vk::VkDeviceSize size_ = 0ull)
            : address(address_)
            , size(size_)
        {
        }
    };

    // Requests memory for a new pipeline.
    MetaDataLocation getMetaDataLocation(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::Allocator &allocator,
                                         const vk::VkComputePipelineCreateInfo &createInfo);

    // Forbid accidental copy and assignment.
    DGCComputePipelineMetaDataPool(const DGCComputePipelineMetaDataPool &)            = delete;
    DGCComputePipelineMetaDataPool &operator=(const DGCComputePipelineMetaDataPool &) = delete;

protected:
    struct BufferInfo
    {
        vk::Move<vk::VkBuffer> buffer;
        vk::VkDeviceSize allocationSize;
        de::MovePtr<vk::Allocation> allocation;
        vk::VkDeviceAddress address;
        vk::VkDeviceSize nextOffset;

        BufferInfo() : buffer(), allocationSize(0ull), allocation(), address(0ull), nextOffset(0ull)
        {
        }
    };

    std::map<vk::VkDevice, std::unique_ptr<BufferInfo>> m_bufferInfos;
    const uint32_t m_multiplier;
    const bool m_captureReplay;
    uint32_t m_requestCount; // This is an internal counter mainly for debugging purposes.
};

// Class that helps create compute pipelines to be used with DGC.
class DGCComputePipeline
{
public:
    DGCComputePipeline(DGCComputePipelineMetaDataPool &metaDataPool, const vk::DeviceInterface &vkd,
                       vk::VkDevice device, vk::Allocator &allocator, vk::VkPipelineCreateFlags pipelineFlags,
                       vk::VkPipelineLayout pipelineLayout, vk::VkPipelineShaderStageCreateFlags shaderFlags,
                       vk::VkShaderModule module, const vk::VkSpecializationInfo *specializationInfo = nullptr,
                       vk::VkDeviceAddress pipelineDeviceAddressCaptureReplay = 0ull,
                       vk::VkPipeline basePipelineHandle = VK_NULL_HANDLE, int32_t basePipelineIndex = -1,
                       uint32_t subgroupSize = 0u);

    vk::VkPipeline get(void) const;
    vk::VkPipeline operator*(void) const;

    vk::VkDeviceAddress getIndirectDeviceAddress(void) const;

    // Forbid accidental copy and assignment.
    DGCComputePipeline(const DGCComputePipeline &other)       = delete;
    DGCComputePipeline &operator=(const DGCComputePipeline &) = delete;

protected:
    const vk::DeviceInterface &m_vkd;
    const vk::VkDevice m_device;
    vk::Move<vk::VkPipeline> m_pipeline;
    DGCComputePipelineMetaDataPool::MetaDataLocation m_metaDataLocation;
    vk::VkDeviceAddress m_indirectDeviceAddress;
};

// Class to help build VkIndirectCommandsLayoutNV objects.
class IndirectCommandsLayoutBuilder
{
public:
    IndirectCommandsLayoutBuilder(vk::VkIndirectCommandsLayoutUsageFlagsNV, vk::VkPipelineBindPoint);
    IndirectCommandsLayoutBuilder(const IndirectCommandsLayoutBuilder &)            = delete;
    IndirectCommandsLayoutBuilder &operator=(const IndirectCommandsLayoutBuilder &) = delete;

    // Commands to add token to the layout.
    void addPushConstantToken(uint32_t stream, uint32_t offset, vk::VkPipelineLayout, vk::VkShaderStageFlags,
                              uint32_t pcOffset, uint32_t pcSize);
    void addPipelineToken(uint32_t stream, uint32_t offset);
    void addDispatchToken(uint32_t stream, uint32_t offset);

    // Get the amount of streams used by the layout.
    uint32_t getStreamCount(void) const;

    // Get the calculated range (amount of data so far) for the given stream.
    uint32_t getStreamRange(uint32_t stream) const;

    // Stream strides are calculated automatically by default but they can also be set manually with this method.
    // This can be useful for adding extra padding at the end of the items in a stream, or to do some more convoluted layouts.
    void setStreamStride(uint32_t stream, uint32_t stride);

    // Build the specified layout and return its handle.
    vk::Move<vk::VkIndirectCommandsLayoutNV> build(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                   const vk::VkAllocationCallbacks *pAllocator = nullptr) const;

protected:
    const vk::VkIndirectCommandsLayoutUsageFlagsNV m_layoutUsageFlags;
    const vk::VkPipelineBindPoint m_bindPoint;
    std::vector<vk::VkIndirectCommandsLayoutTokenNV> m_tokens;
    std::map<uint32_t, uint32_t> m_manualStrides;
};

class PreprocessBuffer
{
public:
    PreprocessBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::Allocator &alloc,
                     vk::VkPipelineBindPoint bindPoint, vk::VkPipeline pipeline,
                     vk::VkIndirectCommandsLayoutNV cmdLayout, uint32_t maxSequences);

    const vk::VkBuffer &get(void) const
    {
        return *m_buffer;
    }
    const vk::VkBuffer &operator*(void) const
    {
        return get();
    }
    const vk::Allocation &getAllocation(void) const
    {
        return *m_bufferAllocation;
    }
    vk::VkDeviceSize getSize(void) const
    {
        return m_memReqs.size;
    }

    // Forbid accidental copy and assignment.
    PreprocessBuffer(const PreprocessBuffer &)            = delete;
    PreprocessBuffer &operator=(const PreprocessBuffer &) = delete;

protected:
    vk::Move<vk::VkBuffer> m_buffer;
    de::MovePtr<vk::Allocation> m_bufferAllocation;
    vk::VkMemoryRequirements m_memReqs;
};

// Push back a device address onto an std::vector (of uint8_t, uint32_t, etc).
// This is helpful to push a pipeline address when preparing the command stream data.
template <typename T>
void pushBackDeviceAddress(std::vector<T> &out, vk::VkDeviceAddress address)
{
    constexpr auto vecItemSize = sizeof(T);
    constexpr auto addressSize = sizeof(vk::VkDeviceAddress);
    constexpr auto neededItems = (addressSize + vecItemSize - 1u) / vecItemSize;

    DE_ASSERT(neededItems > 0u);
    const auto prevSize = out.size();
    out.resize(prevSize + neededItems);
    const auto basePtr = &out.at(prevSize); // Important to take this after resizing, not before.
    deMemcpy(basePtr, &address, sizeof(address));
};

} // namespace DGC
} // namespace vkt

#endif // _VKTDGCUTIL_HPP
