/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief Device Generated Commands EXT compute tests that use
 *        DGC push data to select a descriptor heap slot via
 *        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT.
 *//*--------------------------------------------------------------------*/

#include "vktDGCComputePushIndexHeapTestsExt.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"

#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

// One heap slot (and one backing storage buffer) per sequence.
constexpr uint32_t kNumSlots = 16u;

// Value the shader adds to the pre-filled slot content; large enough that the
// marked value (slot index + kMarker) is unambiguous for every slot.
constexpr uint32_t kMarker = (1u << 20);

struct TestParams
{
    VkShaderStageFlags pushStage;
};

VkPhysicalDeviceDescriptorHeapPropertiesEXT getDescriptorHeapProperties(Context &context)
{
    VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProps = initVulkanStructure();
    VkPhysicalDeviceProperties2 properties2               = initVulkanStructure();
    properties2.pNext                                     = &heapProps;

    context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);
    return heapProps;
}

class PushIndexHeapInstance : public vkt::TestInstance
{
public:
    PushIndexHeapInstance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~PushIndexHeapInstance() = default;

    tcu::TestStatus iterate() override;

protected:
    const TestParams m_params;
};

class PushIndexHeapCase : public vkt::TestCase
{
public:
    PushIndexHeapCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~PushIndexHeapCase() = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new PushIndexHeapInstance(context, m_params);
    }

protected:
    const TestParams m_params;
};

void PushIndexHeapCase::checkSupport(Context &context) const
{
    checkDGCExtComputeSupport(context, DGCComputeSupportType::BASIC);
    context.requireDeviceFunctionality(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
}

void PushIndexHeapCase::initPrograms(vk::SourceCollections &programCollection) const
{
    const tcu::StringTemplate compTemplate(R"glsl(
#version 460
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout (set = 0, binding = 0) buffer OutputBlock { uint value; } outBuffer;
void main (void)
{
    outBuffer.value += ${MARKER}u;
}
)glsl");

    std::map<std::string, std::string> params;
    params["MARKER"] = std::to_string(kMarker);

    programCollection.glslSources.add("comp") << glu::ComputeSource(compTemplate.specialize(params));
}

tcu::TestStatus PushIndexHeapInstance::iterate()
{
    const auto ctx         = m_context.getContextCommonData();
    const auto &vkd        = ctx.vkd;
    const auto device      = ctx.device;
    const auto shaderStage = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    // One backing storage buffer per heap slot, slot i pre-filled with value i.
    const auto bufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto bufferInfo = makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    std::vector<std::unique_ptr<BufferWithMemory>> outputBuffers;
    outputBuffers.reserve(kNumSlots);
    for (uint32_t i = 0u; i < kNumSlots; ++i)
    {
        auto buffer = std::make_unique<BufferWithMemory>(
            vkd, device, ctx.allocator, bufferInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
        auto &alloc                                       = buffer->getAllocation();
        *reinterpret_cast<uint32_t *>(alloc.getHostPtr()) = i;
        flushAlloc(vkd, device, alloc);
        outputBuffers.push_back(std::move(buffer));
    }

    // Resource heap holding one storage-buffer descriptor per slot.
    const auto heapProps = getDescriptorHeapProperties(m_context);
    const auto descriptorStride =
        static_cast<VkDeviceSize>(deAlign64(static_cast<int64_t>(heapProps.bufferDescriptorSize),
                                            static_cast<int64_t>(heapProps.bufferDescriptorAlignment)));
    VkDeviceSize userHeapSize = descriptorStride * kNumSlots;
    userHeapSize              = static_cast<VkDeviceSize>(
        deAlign64(static_cast<int64_t>(userHeapSize), static_cast<int64_t>(heapProps.resourceHeapAlignment)));
    const VkDeviceSize resourceHeapSize = userHeapSize + heapProps.minResourceHeapReservedRange;

    VkBufferCreateInfo heapBufferInfo = initVulkanStructure();
    heapBufferInfo.size               = resourceHeapSize;
    heapBufferInfo.usage       = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    heapBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    BufferWithMemory resourceHeap(vkd, device, ctx.allocator, heapBufferInfo,
                                  MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    VkBufferDeviceAddressInfo heapAddressInfo = initVulkanStructure();
    heapAddressInfo.buffer                    = resourceHeap.get();
    const VkDeviceAddress heapAddress         = vkd.getBufferDeviceAddress(device, &heapAddressInfo);

    VkBindHeapInfoEXT bindHeapInfo   = initVulkanStructure();
    bindHeapInfo.heapRange.address   = heapAddress;
    bindHeapInfo.heapRange.size      = resourceHeapSize;
    bindHeapInfo.reservedRangeOffset = userHeapSize;
    bindHeapInfo.reservedRangeSize   = heapProps.minResourceHeapReservedRange;

    // Write one storage-buffer descriptor per slot, slot i pointing at buffer i.
    auto *const heapHostPtr = reinterpret_cast<uint8_t *>(resourceHeap.getAllocation().getHostPtr());
    for (uint32_t i = 0u; i < kNumSlots; ++i)
    {
        VkBufferDeviceAddressInfo bufferAddressInfo = initVulkanStructure();
        bufferAddressInfo.buffer                    = outputBuffers.at(i)->get();

        VkDeviceAddressRangeEXT addressRange{};
        addressRange.address = vkd.getBufferDeviceAddress(device, &bufferAddressInfo);
        addressRange.size    = bufferSize;

        VkHostAddressRangeEXT descriptorHostRange{};
        descriptorHostRange.address = heapHostPtr + i * descriptorStride;
        descriptorHostRange.size    = static_cast<size_t>(heapProps.bufferDescriptorSize);

        VkResourceDescriptorInfoEXT resourceDescriptorInfo = initVulkanStructure();
        resourceDescriptorInfo.type                        = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        resourceDescriptorInfo.data.pAddressRange          = &addressRange;

        VK_CHECK(vkd.writeResourceDescriptorsEXT(device, 1u, &resourceDescriptorInfo, &descriptorHostRange));
    }

    // Map set 0, binding 0 to a heap slot selected by the pushed index.
    VkDescriptorSetAndBindingMappingEXT mapping = initVulkanStructure();
    // mapping.descriptorSet                    = 0u;
    // mapping.firstBinding                     = 0u;
    mapping.bindingCount = 1u;
    mapping.resourceMask = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    mapping.source       = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;
    // mapping.sourceData.pushIndex.heapOffset  = 0u;
    // mapping.sourceData.pushIndex.pushOffset  = 0u;
    mapping.sourceData.pushIndex.heapIndexStride = static_cast<uint32_t>(descriptorStride);
    mapping.sourceData.pushIndex.heapArrayStride = static_cast<uint32_t>(descriptorStride);

    VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo = initVulkanStructure();
    mappingInfo.mappingCount                                  = 1u;
    mappingInfo.pMappings                                     = &mapping;

    // Compute pipeline created directly for the descriptor heap (no pipeline layout).
    const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"));

    VkPipelineCreateFlags2CreateInfo createFlags2 = initVulkanStructure();
    createFlags2.flags                            = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    VkComputePipelineCreateInfo pipelineCreateInfo = initVulkanStructure();
    pipelineCreateInfo.pNext                       = &createFlags2;
    pipelineCreateInfo.stage                       = initVulkanStructure();
    pipelineCreateInfo.stage.pNext                 = &mappingInfo;
    pipelineCreateInfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineCreateInfo.stage.module                = *compModule;
    pipelineCreateInfo.stage.pName                 = "main";
    pipelineCreateInfo.layout                      = VK_NULL_HANDLE;

    const auto pipeline = createComputePipeline(vkd, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    // Indirect commands layout: PUSH_DATA token (the index) followed by DISPATCH.
    // The push range offset must match the mapping's pushOffset.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(0u, shaderStage, VK_NULL_HANDLE);
    cmdsLayoutBuilder.addPushDataToken(
        0u, makePushConstantRange(m_params.pushStage, 0u, static_cast<uint32_t>(sizeof(uint32_t))));
    const auto dispatchTokenOffset = cmdsLayoutBuilder.getStreamRange();
    cmdsLayoutBuilder.addDispatchToken(dispatchTokenOffset);
    const auto cmdsLayout = cmdsLayoutBuilder.build(vkd, device);
    const auto stride     = cmdsLayoutBuilder.getStreamStride();

    // Command stream: sequence i pushes index i and dispatches a single work group.
    std::vector<uint8_t> cmdData(static_cast<size_t>(kNumSlots) * stride, 0u);
    for (uint32_t i = 0u; i < kNumSlots; ++i)
    {
        uint8_t *const base                  = cmdData.data() + i * stride;
        const uint32_t index                 = i;
        const VkDispatchIndirectCommand disp = {1u, 1u, 1u};
        deMemcpy(base, &index, sizeof(index));
        deMemcpy(base + dispatchTokenOffset, &disp, sizeof(disp));
    }

    const auto cmdDataSize = static_cast<VkDeviceSize>(de::dataSize(cmdData));
    DGCBuffer indirectBuffer(vkd, device, ctx.allocator, cmdDataSize);
    auto &indirectAlloc = indirectBuffer.getAllocation();
    deMemcpy(indirectAlloc.getHostPtr(), de::dataOrNull(cmdData), de::dataSize(cmdData));
    flushAlloc(vkd, device, indirectAlloc);

    PreprocessBufferExt preprocessBuffer(vkd, device, ctx.allocator, VK_NULL_HANDLE, *cmdsLayout, kNumSlots, 0u,
                                         *pipeline);

    DGCGenCmdsInfo generatedCommandsInfo(shaderStage, VK_NULL_HANDLE, *cmdsLayout, indirectBuffer.getDeviceAddress(),
                                         cmdDataSize, preprocessBuffer.getDeviceAddress(), preprocessBuffer.getSize(),
                                         kNumSlots, 0ull, 0u, *pipeline);

    CommandPoolWithBuffer cmd(vkd, device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(vkd, cmdBuffer);
    vkd.cmdBindResourceHeapEXT(cmdBuffer, &bindHeapInfo);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, VK_FALSE, &generatedCommandsInfo.get());
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, ctx.queue, cmdBuffer);

    // Slot i must have been selected by the sequence that pushed index i, so its
    // backing buffer holds (i + kMarker). Any missed or misdirected selection
    // leaves some buffer at its pre-filled value.
    for (uint32_t i = 0u; i < kNumSlots; ++i)
    {
        auto &alloc = outputBuffers.at(i)->getAllocation();
        invalidateAlloc(vkd, device, alloc);
        const uint32_t result   = *reinterpret_cast<const uint32_t *>(alloc.getHostPtr());
        const uint32_t expected = i + kMarker;
        if (result != expected)
        {
            std::ostringstream msg;
            msg << "Slot " << i << ": expected " << expected << " but got " << result;
            return tcu::TestStatus::fail(msg.str());
        }
    }

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createDGCComputePushIndexHeapTestsExt(tcu::TestContext &testCtx)
{
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "push_index_heap"));

    mainGroup->addChild(new PushIndexHeapCase(testCtx, "stage_compute", TestParams{VK_SHADER_STAGE_COMPUTE_BIT}));

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
