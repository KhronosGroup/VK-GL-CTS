#ifndef _VKTDGCUTILEXT_HPP
#define _VKTDGCUTILEXT_HPP
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

#include "vkMemUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktTestCase.hpp"

#include "tcuMaybe.hpp"

#include <cstdint>
#include <vector>
#include <memory>

namespace vkt
{
namespace DGC
{

void checkDGCExtSupport(Context &context, vk::VkShaderStageFlags stages, vk::VkShaderStageFlags bindStagesPipeline = 0u,
                        vk::VkShaderStageFlags bindStagesShaderObject          = 0u,
                        vk::VkIndirectCommandsInputModeFlagsEXT inputModeFlags = 0u, bool transformFeedback = false);

void checkDGCExtComputeSupport(Context &context, bool requireBinds);

vk::VkIndirectExecutionSetInfoEXT makeIndirectExecutionSetInfo(
    const vk::VkIndirectExecutionSetPipelineInfoEXT &pipelineInfo);
vk::VkIndirectExecutionSetInfoEXT makeIndirectExecutionSetInfo(
    const vk::VkIndirectExecutionSetShaderInfoEXT &shaderInfo);

// Class that helps create and update an indirect execution set.
// You probably don't want to create these objects directly. Instead, use one of the makeExecutionSetManager* functions.
class ExecutionSetManager
{
public:
    ExecutionSetManager(const vk::DeviceInterface &vkd, vk::VkDevice device,
                        const vk::VkIndirectExecutionSetPipelineInfoEXT &pipelineInfo);
    ExecutionSetManager(const vk::DeviceInterface &vkd, vk::VkDevice device,
                        const vk::VkIndirectExecutionSetShaderInfoEXT &shaderInfo);

    // Add an element to be updated for pipeline-based indirect execution sets.
    // See update().
    void addPipeline(uint32_t index, vk::VkPipeline pipeline);

    // Add an element to be updated for shader-object-based indirect execution sets.
    // See update().
    void addShader(uint32_t index, vk::VkShaderEXT shader);

    // Run pending vkUpdateIndirectExecutionSet* calls. Clear queue of pending updates.
    void update(void);

    // Get the indirect execution set handle.
    vk::VkIndirectExecutionSetEXT get(bool requireNoPendingWrites = true) const;
    vk::VkIndirectExecutionSetEXT operator*(void) const
    {
        return get();
    }

protected:
    const vk::DeviceInterface &m_vkd;
    const vk::VkDevice m_device;
    vk::Move<vk::VkIndirectExecutionSetEXT> m_executionSet;
    bool m_pipelines;
    bool m_shaderObjects;

    // Helps make sure we don't miss the update() call.
    // Note we can still call get() at any moment but, by default, not while there are updates pending.
    void assertNoPendingWrites(void) const
    {
        DE_ASSERT(m_pipelineWrites.empty());
        DE_ASSERT(m_shaderWrites.empty());
    }

    std::vector<vk::VkWriteIndirectExecutionSetPipelineEXT> m_pipelineWrites;
    std::vector<vk::VkWriteIndirectExecutionSetShaderEXT> m_shaderWrites;
};

using ExecutionSetManagerPtr = std::unique_ptr<ExecutionSetManager>;

ExecutionSetManagerPtr makeExecutionSetManagerPipeline(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                       vk::VkPipeline initialPipeline, uint32_t maxPipelineCount);

// Info about a shader stage used when creating an indirect execution set with shader objects.
struct IESStageInfo
{
    IESStageInfo(const vk::VkShaderEXT &shader_, const std::vector<vk::VkDescriptorSetLayout> &setLayouts_)
        : shader(shader_)
        , setLayouts(setLayouts_)
    {
    }

    vk::VkShaderEXT shader;
    std::vector<vk::VkDescriptorSetLayout> setLayouts;
};

ExecutionSetManagerPtr makeExecutionSetManagerShader(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                     const std::vector<IESStageInfo> &stages,
                                                     const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
                                                     uint32_t maxShaderCount);

// Handles creating a memory requirements info structure with the proper pNext chain.
class DGCMemReqsInfo
{
public:
    DGCMemReqsInfo(vk::VkIndirectExecutionSetEXT ies, vk::VkIndirectCommandsLayoutEXT cmdsLayout, uint32_t maxSeqCount,
                   uint32_t maxDrawCount, vk::VkPipeline pipeline = VK_NULL_HANDLE,
                   const std::vector<vk::VkShaderEXT> *shaders = nullptr);

    void setCommandsLayout(vk::VkIndirectCommandsLayoutEXT cmdsLayout)
    {
        m_memReqs.indirectCommandsLayout = cmdsLayout;
    }
    void setMaxSequenceCount(uint32_t maxSeqCount)
    {
        m_memReqs.maxSequenceCount = maxSeqCount;
    }

    const vk::VkGeneratedCommandsMemoryRequirementsInfoEXT &get(void) const
    {
        return m_memReqs;
    }
    const vk::VkGeneratedCommandsMemoryRequirementsInfoEXT &operator*(void) const
    {
        return get();
    }

    // pNext pointer would be wrong, do not bother.
    DGCMemReqsInfo(const DGCMemReqsInfo &) = delete;

protected:
    vk::VkGeneratedCommandsMemoryRequirementsInfoEXT m_memReqs;
    vk::VkGeneratedCommandsPipelineInfoEXT m_pipelineInfo;
    vk::VkGeneratedCommandsShaderInfoEXT m_shadersInfo;
    std::vector<vk::VkShaderEXT> m_shaders;
};

// Handles creating a VkGeneratedCommandsInfoEXT structure with the proper pNext chain.
class DGCGenCmdsInfo
{
public:
    DGCGenCmdsInfo(vk::VkShaderStageFlags shaderStages, vk::VkIndirectExecutionSetEXT ies,
                   vk::VkIndirectCommandsLayoutEXT indirectCommandsLayout, vk::VkDeviceAddress indirectAddress,
                   vk::VkDeviceSize indirectAddressSize, vk::VkDeviceAddress preprocessAddress,
                   vk::VkDeviceSize preprocessSize, uint32_t maxSequenceCount, vk::VkDeviceAddress sequenceCountAddress,
                   uint32_t maxDrawCount, vk::VkPipeline pipeline = VK_NULL_HANDLE,
                   const std::vector<vk::VkShaderEXT> *shaders = nullptr);

    DGCGenCmdsInfo(const DGCGenCmdsInfo &); // Needed for some tests.

    const vk::VkGeneratedCommandsInfoEXT &get(void) const
    {
        return m_genCmdsInfo;
    }
    const vk::VkGeneratedCommandsInfoEXT &operator*(void) const
    {
        return get();
    }

protected:
    vk::VkGeneratedCommandsInfoEXT m_genCmdsInfo;
    vk::VkGeneratedCommandsPipelineInfoEXT m_pipelineInfo;
    vk::VkGeneratedCommandsShaderInfoEXT m_shadersInfo;
    std::vector<vk::VkShaderEXT> m_shaders;
};

// Useful because it's easy to forget to initialize the sType and pNext members of VkMemoryRequirements2.
vk::VkMemoryRequirements getGeneratedCommandsMemoryRequirementsExt(
    const vk::DeviceInterface &, vk::VkDevice, const vk::VkGeneratedCommandsMemoryRequirementsInfoEXT &);

// Insert a memory barrier from the preprocessing stage to the execution stage.
void preprocessToExecuteBarrierExt(const vk::DeviceInterface &, vk::VkCommandBuffer);

// Class to help build VkIndirectCommandsLayoutEXT objects.
class IndirectCommandsLayoutBuilderExt
{
public:
    IndirectCommandsLayoutBuilderExt(vk::VkIndirectCommandsLayoutUsageFlagsEXT, vk::VkShaderStageFlags,
                                     vk::VkPipelineLayout,
                                     const vk::VkPipelineLayoutCreateInfo *pPipelineLayout = nullptr);
    IndirectCommandsLayoutBuilderExt(const IndirectCommandsLayoutBuilderExt &)            = delete;
    IndirectCommandsLayoutBuilderExt &operator=(const IndirectCommandsLayoutBuilderExt &) = delete;

    // Commands to add tokens to the layout.
    void addPushConstantToken(uint32_t offset, const vk::VkPushConstantRange &pcRange);
    void addSequenceIndexToken(uint32_t offset, const vk::VkPushConstantRange &pcRange);
    void addVertexBufferToken(uint32_t offset, uint32_t bindingNumber);
    void addIndexBufferToken(uint32_t offset, vk::VkIndirectCommandsInputModeFlagBitsEXT mode);
    void addExecutionSetToken(uint32_t offset, vk::VkIndirectExecutionSetInfoTypeEXT setType,
                              vk::VkShaderStageFlags stages);
    void addComputePipelineToken(uint32_t offset);     // Shortcut for addExecutionSetToken
    void addComputeShaderObjectToken(uint32_t offset); // Shortcut for addExecutionSetToken
    void addDrawIndexedToken(uint32_t offset);
    void addDrawToken(uint32_t offset);
    void addDrawIndexedCountToken(uint32_t offset);
    void addDrawCountToken(uint32_t offset);
    void addDrawMeshTasksCountNvToken(uint32_t offset);
    void addDrawMeshTasksCountToken(uint32_t offset);
    void addDispatchToken(uint32_t offset);
    void addDrawMeshTasksNvToken(uint32_t offset);
    void addDrawMeshTasksToken(uint32_t offset);
    void addTraceRays2Token(uint32_t offset);

    // Get the calculated range (amount of data so far) for the command stream, based on token data offsets and sizes.
    uint32_t getStreamRange(void) const;

    // The stream stride is calculated automatically by default but it can also be set manually with this method.
    // This can be useful for adding extra padding at the end of the items in a stream, or to do some more convoluted layouts.
    void setStreamStride(uint32_t stride);

    // Returns the stream stride: the manual value if it's been set or the automatic value otherwise.
    uint32_t getStreamStride(void) const;

    // Build the specified layout and return its handle.
    vk::Move<vk::VkIndirectCommandsLayoutEXT> build(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                    const vk::VkAllocationCallbacks *pAllocator = nullptr) const;

protected:
    const vk::VkIndirectCommandsLayoutUsageFlagsEXT m_layoutUsageFlags;
    const vk::VkShaderStageFlags m_shaderStages;
    const vk::VkPipelineLayout m_pipelineLayout;
    const vk::VkPipelineLayoutCreateInfo *const m_layoutCreateInfoPtr;
    tcu::Maybe<uint32_t> m_manualStride;

    // Similar to VkIndirectCommandsLayoutTokenEXT. Instead of having a union of pointers, it has a flat list of smart ones.
    // This allows us to manage memory for each token easily and converting it to VkIndirectCommandsLayoutTokenEXT is very simple.
    struct InternalToken
    {
        vk::VkIndirectCommandsTokenTypeEXT type;
        uint32_t offset;
        std::unique_ptr<vk::VkIndirectCommandsPushConstantTokenEXT> pPushConstant;
        std::unique_ptr<vk::VkIndirectCommandsVertexBufferTokenEXT> pVertexBuffer;
        std::unique_ptr<vk::VkIndirectCommandsIndexBufferTokenEXT> pIndexBuffer;
        std::unique_ptr<vk::VkIndirectCommandsExecutionSetTokenEXT> pExecutionSet;

        // Builds an empty token.
        InternalToken();

        // Converts internal token to a Vulkan token.
        vk::VkIndirectCommandsLayoutTokenEXT asVkToken(void) const;
    };

    std::vector<InternalToken> m_tokens;

    // Pushes back a new empty token and returns a reference to it.
    InternalToken &pushBackEmptyToken(void);

    // Adds a simple token (token with no meta-data) given the offset and type.
    void addSimpleToken(uint32_t offset, vk::VkIndirectCommandsTokenTypeEXT tokenType);
};

class PreprocessBufferExt
{
public:
    PreprocessBufferExt(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::Allocator &alloc,
                        vk::VkIndirectExecutionSetEXT indirectExecutionSet,
                        vk::VkIndirectCommandsLayoutEXT indirectCommandsLayout, uint32_t maxSequenceCount,
                        uint32_t maxDrawCount /*only used for COUNT-type tokens*/,
                        vk::VkPipeline pipeline = VK_NULL_HANDLE, const std::vector<vk::VkShaderEXT> *shaders = nullptr,
                        vk::VkDeviceSize offset = 0ull);

    vk::VkBuffer get(void) const
    {
        return *m_buffer;
    }
    vk::VkBuffer operator*(void) const
    {
        return get();
    }
    const vk::Allocation &getAllocation(void) const
    {
        return *m_bufferAllocation;
    }
    vk::VkDeviceAddress getDeviceAddress(void) const
    {
        return m_deviceAddress;
    }
    vk::VkDeviceSize getSize(void) const
    {
        return m_size;
    }
    bool needed(void) const
    {
        return (m_size > 0ull);
    }

    // Forbid accidental copy and assignment.
    PreprocessBufferExt(const PreprocessBufferExt &)            = delete;
    PreprocessBufferExt &operator=(const PreprocessBufferExt &) = delete;

protected:
    vk::VkDeviceSize m_offset;
    vk::Move<vk::VkBuffer> m_buffer;
    de::MovePtr<vk::Allocation> m_bufferAllocation;
    vk::VkDeviceSize m_size;
    vk::VkDeviceAddress m_deviceAddress;
};

vk::VkDeviceAddress getBufferDeviceAddress(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkBuffer buffer);

// General buffer with indirect usage and device-addressable memory.
class DGCBuffer
{
public:
    DGCBuffer(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
              const vk::VkDeviceSize size, const vk::VkBufferUsageFlags extraUsageFlags = 0u,
              const vk::MemoryRequirement extraMemReqs = vk::MemoryRequirement::HostVisible);

    const vk::VkBuffer &get(void) const
    {
        return m_buffer.get();
    }
    const vk::VkBuffer &operator*(void) const
    {
        return get();
    }
    vk::Allocation &getAllocation(void) const
    {
        return m_buffer.getAllocation();
    }
    vk::VkDeviceAddress getDeviceAddress(void) const
    {
        return m_address;
    }
    vk::VkDeviceSize getSize(void) const
    {
        return m_size;
    }

protected:
    vk::VkDeviceSize m_size;
    vk::BufferWithMemory m_buffer;
    vk::VkDeviceAddress m_address;
};

// Class that helps create compute pipelines to be used with DGC.
class DGCComputePipelineExt
{
public:
    DGCComputePipelineExt(const vk::DeviceInterface &vkd, vk::VkDevice device,
                          vk::VkPipelineCreateFlags2KHR pipelineFlags, vk::VkPipelineLayout pipelineLayout,
                          vk::VkPipelineShaderStageCreateFlags shaderStageCreateFlags, vk::VkShaderModule module,
                          const vk::VkSpecializationInfo *specializationInfo = nullptr,
                          vk::VkPipeline basePipelineHandle = VK_NULL_HANDLE, int32_t basePipelineIndex = -1,
                          uint32_t subgroupSize = 0u);

    vk::VkPipeline get(void) const;
    vk::VkPipeline operator*(void) const;

    // Forbid accidental copy and assignment.
    DGCComputePipelineExt(const DGCComputePipelineExt &other)       = delete;
    DGCComputePipelineExt &operator=(const DGCComputePipelineExt &) = delete;

protected:
    vk::Move<vk::VkPipeline> m_pipeline;
};

// Class that helps create shaders to be used with DGC.
class DGCShaderExt
{
public:
    DGCShaderExt(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
                 vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
                 const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                 const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
                 const vk::VkSpecializationInfo *specializationInfo = nullptr, const void *pNext = nullptr);

    virtual ~DGCShaderExt()
    {
    }

    const vk::VkShaderEXT &get(void) const
    {
        return *m_shader;
    }
    const vk::VkShaderEXT &operator*(void) const
    {
        return get();
    }

    // Forbid accidental copy and assignment.
    DGCShaderExt(const DGCShaderExt &other)       = delete;
    DGCShaderExt &operator=(const DGCShaderExt &) = delete;

protected:
    // Default constructor, not public but available to derived classes so they can use it before preparing the init data.
    DGCShaderExt(void);

    // Real constructor, used to be able to build the pNext structure before calling it.
    void init(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderStageFlagBits stage,
              vk::VkShaderCreateFlagsEXT shaderFlags, const vk::ProgramBinary &shaderBinary,
              const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
              const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
              const vk::VkSpecializationInfo *specializationInfo = nullptr, const void *pNext = nullptr);

    vk::Move<vk::VkShaderEXT> m_shader;
};

// Class that helps create compute shaders to be used with DGC.
class DGCComputeShaderExt : public DGCShaderExt
{
public:
    DGCComputeShaderExt(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkShaderCreateFlagsEXT shaderFlags,
                        const vk::ProgramBinary &shaderBinary, const std::vector<vk::VkDescriptorSetLayout> &setLayouts,
                        const std::vector<vk::VkPushConstantRange> &pushConstantRanges,
                        const vk::VkSpecializationInfo *specializationInfo = nullptr, uint32_t subgroupSize = 0u);

    // Forbid accidental copy and assignment.
    DGCComputeShaderExt(const DGCComputeShaderExt &other)       = delete;
    DGCComputeShaderExt &operator=(const DGCComputeShaderExt &) = delete;
};

// The original method from the context returns a vector of const char*. This transforms it into the preferred format.
std::vector<std::string> getDeviceCreationExtensions(Context &context);

// Helper function for tests using shader objects.
void bindShaderObjectState(const vk::DeviceInterface &vkd, const std::vector<std::string> &extensions,
                           const vk::VkCommandBuffer cmdBuffer, const std::vector<vk::VkViewport> &viewports,
                           const std::vector<vk::VkRect2D> &scissors, const vk::VkPrimitiveTopology topology,
                           const uint32_t patchControlPoints,
                           const vk::VkPipelineVertexInputStateCreateInfo *vertexInputStateCreateInfo,
                           const vk::VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo,
                           const vk::VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                           const vk::VkPipelineDepthStencilStateCreateInfo *depthStencilStateCreateInfo,
                           const vk::VkPipelineColorBlendStateCreateInfo *colorBlendStateCreateInfo);

// Helper struct for D3D12 compatibility, needed to test VK_INDIRECT_COMMANDS_INPUT_MODE_DXGI_INDEX_BUFFER_EXT.
struct IndexBufferViewD3D12
{
    uint64_t bufferAddress;
    uint32_t size;
    int32_t indexType;

    IndexBufferViewD3D12(vk::VkDeviceAddress address, uint32_t size, vk::VkIndexType indexType);
};

// Helper that allows us to submit a separate preprocess and a normal command buffer with proper sync.
void submitAndWaitWithPreprocess(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkQueue queue,
                                 vk::VkCommandBuffer cmdBuffer,
                                 vk::VkCommandBuffer preprocessCmdBuffer = VK_NULL_HANDLE);

} // namespace DGC
} // namespace vkt

#endif // _VKTDGCUTILEXT_HPP
