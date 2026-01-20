/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tests DMA heap buffer memory import tests
 */
/*--------------------------------------------------------------------*/

#include "vktMemoryExternalDmaHeapTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "deUniquePtr.hpp"
#include "deMemory.h"

#include <string>

namespace vkt
{
namespace memory
{
namespace
{

void checkDmaHeapMemory(Context &context)
{
    const vk::InstanceInterface &vki          = context.getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_EXT_external_memory_dma_buf");

    const vk::VkPhysicalDeviceExternalBufferInfo info = {
        vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO, nullptr, 0,
        vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};

    vk::VkExternalBufferProperties properties = vk::initVulkanStructure();
    vki.getPhysicalDeviceExternalBufferProperties(physicalDevice, &info, &properties);

    if ((properties.externalMemoryProperties.externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) ==
        0)
    {
        TCU_THROW(NotSupportedError, "DMA_BUF imported buffer is not supported");
    }

    if ((properties.externalMemoryProperties.externalMemoryFeatures &
         vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0)
    {
        TCU_THROW(NotSupportedError, "DMA_BUF imported buffer requires dedicated allocation");
    }

    if (!vk::DmaHeapAllocator::isSupported())
    {
        TCU_THROW(NotSupportedError, "Target does not support DMA heap allocator");
    }
}

struct TestDmaHeapMemoryShaderAccessParams
{
    vk::VkDeviceSize offset;
};

void checkDmaHeapMemoryAccess(Context &context, const TestDmaHeapMemoryShaderAccessParams)
{
    checkDmaHeapMemory(context);
}

void initProgramsDmaHeapMemoryShaderAccess(vk::SourceCollections &dst, const TestDmaHeapMemoryShaderAccessParams)
{
    const std::string shader_write(
        R"(
    #version 450

    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(set=0, binding = 0, std430) readonly buffer _input { uint hostVisibleData[]; };
    layout(set=0, binding = 1, std430) writeonly buffer _output { uint dmaHeapMemoryData[]; };

    void main()
    {
        uint index = gl_GlobalInvocationID.x;
        dmaHeapMemoryData[index] = hostVisibleData[index];
    }
    )");

    const std::string shader_read(
        R"(
    #version 450

    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    layout(set=0, binding = 0, std430) writeonly buffer _output { uint hostVisibleData[]; };
    layout(set=0, binding = 1, std430) readonly buffer _input { uint dmaHeapMemoryData[]; };

    void main()
    {
        uint index = gl_GlobalInvocationID.x;
        hostVisibleData[index] = dmaHeapMemoryData[index];
    }
    )");
    dst.glslSources.add("dma_heap_memory_write") << glu::ComputeSource(shader_write);
    dst.glslSources.add("dma_heap_memory_read") << glu::ComputeSource(shader_read);
}

tcu::TestStatus testDmaHeapMemoryAllocateAndBind(Context &context)
{
    const vk::InstanceInterface &vki = context.getInstanceInterface();
    const vk::DeviceInterface &vk    = context.getDeviceInterface();
    const vk::VkDevice device        = context.getDevice();

    const vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    vk::VkPhysicalDeviceMemoryProperties memoryProperties{};
    vki.getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    vk::DmaHeapAllocator dmaHeapAllocator(vk, device, memoryProperties);

    static constexpr const vk::VkDeviceSize bufferSize = 8192;

    // Create a DMA heap memory backed buffer
    const vk::VkExternalMemoryBufferCreateInfo externalMemoryBufferInfo{
        vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const vk::BufferWithMemory dmaHeapMemoryBuffer(
        vk, device, dmaHeapAllocator,
        vk::makeBufferCreateInfo(bufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {}, 0, &externalMemoryBufferInfo),
        vk::MemoryRequirement::Any);

    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testDmaHeapMemoryShaderAccess(Context &context, const TestDmaHeapMemoryShaderAccessParams params)
{
    const vk::InstanceInterface &vki = context.getInstanceInterface();
    const vk::DeviceInterface &vk    = context.getDeviceInterface();
    const vk::VkDevice device        = context.getDevice();
    const vk::VkQueue queue          = context.getUniversalQueue();
    const uint32_t queueFamilyIndex  = context.getUniversalQueueFamilyIndex();
    vk::Allocator &allocator         = context.getDefaultAllocator();

    const vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    vk::VkPhysicalDeviceMemoryProperties memoryProperties{};
    vki.getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    vk::DmaHeapAllocator::OptionalOffsetParams offsetParams;

    if (params.offset != 0)
    {
        vk::VkPhysicalDeviceProperties physicalDeviceProperties{};
        vki.getPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        offsetParams = {physicalDeviceProperties.limits.nonCoherentAtomSize, params.offset};
    }

    vk::DmaHeapAllocator dmaHeapAllocator(vk, device, memoryProperties, offsetParams);

    using BufferElementType                   = uint32_t;
    const vk::VkDeviceSize bufferElementCount = 1024;
    const vk::VkDeviceSize bufferSize         = bufferElementCount * sizeof(BufferElementType);

    // Create a buffer and host-visible memory for it
    const vk::BufferWithMemory hostVisibleBuffer(
        vk, device, allocator,
        vk::makeBufferCreateInfo(bufferSize,
                                 vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        vk::MemoryRequirement::HostVisible);

    // Create a DMA heap memory backed buffer
    const vk::VkExternalMemoryBufferCreateInfo externalMemoryBufferInfo{
        vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const vk::BufferWithMemory dmaHeapMemoryBuffer(
        vk, device, dmaHeapAllocator,
        vk::makeBufferCreateInfo(bufferSize,
                                 vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT, {}, 0,
                                 &externalMemoryBufferInfo),
        vk::MemoryRequirement::Any);

    // Prepare input buffer

    const std::vector<BufferElementType> inputPattern(bufferElementCount, 42);
    DE_ASSERT(inputPattern.size() * sizeof(inputPattern[0]) == bufferSize);
    deMemcpy(hostVisibleBuffer.getAllocation().getHostPtr(), inputPattern.data(), bufferSize);
    vk::flushAlloc(vk, device, hostVisibleBuffer.getAllocation());

    // Create descriptor set

    const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
        vk::DescriptorSetLayoutBuilder()
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const vk::Unique<vk::VkDescriptorPool> descriptorPool(
        vk::DescriptorPoolBuilder()
            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const vk::Unique<vk::VkDescriptorSet> descriptorSet(
        makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Set the bindings

    vk::DescriptorSetUpdateBuilder updateBuilder;
    const vk::VkDescriptorBufferInfo inputBufferDescriptorInfo =
        makeDescriptorBufferInfo(*hostVisibleBuffer, 0ull, bufferSize);
    const vk::VkDescriptorBufferInfo outputBufferDescriptorInfo =
        makeDescriptorBufferInfo(*dmaHeapMemoryBuffer, 0ull, bufferSize);

    vk::DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferDescriptorInfo)
        .writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u),
                     vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
        .update(vk, device);

    // Build shader modules

    const vk::ProgramBinary &binaryWrite = context.getBinaryCollection().get("dma_heap_memory_write");
    const vk::Unique<vk::VkShaderModule> shaderModuleWrite(vk::createShaderModule(vk, device, binaryWrite, 0u));
    const vk::ProgramBinary &binaryRead = context.getBinaryCollection().get("dma_heap_memory_read");
    const vk::Unique<vk::VkShaderModule> shaderModuleRead(vk::createShaderModule(vk, device, binaryRead, 0u));

    // Setup pipeline

    const vk::Unique<vk::VkPipelineLayout> pipelineLayout(vk::makePipelineLayout(vk, device, *descriptorSetLayout));
    const vk::Unique<vk::VkPipeline> pipelineWrite(
        vk::makeComputePipeline(vk, device, *pipelineLayout, *shaderModuleWrite));
    const vk::Unique<vk::VkPipeline> pipelineRead(
        vk::makeComputePipeline(vk, device, *pipelineLayout, *shaderModuleRead));

    // Prepare the command buffer

    const vk::Unique<vk::VkCommandPool> cmdPool(vk::makeCommandPool(vk, device, queueFamilyIndex));
    const vk::Unique<vk::VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    vk::beginCommandBuffer(vk, *cmdBuffer);

    // Clear DMA heap memory buffer to known value before writing to it
    vk.cmdFillBuffer(*cmdBuffer, dmaHeapMemoryBuffer.get(), 0, VK_WHOLE_SIZE, 24);

    // Wait for fill to complete
    {
        const vk::VkBufferMemoryBarrier barrier =
            vk::makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT,
                                        dmaHeapMemoryBuffer.get(), 0, VK_WHOLE_SIZE);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    // Copy host visible buffer -> DMA heap memory buffer with compute shader
    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineWrite);
    vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                             &descriptorSet.get(), 0u, nullptr);
    vk.cmdDispatch(*cmdBuffer, bufferElementCount, 1u, 1u);

    // Wait for dispatch to complete
    {
        const vk::VkBufferMemoryBarrier barrier = vk::makeBufferMemoryBarrier(
            vk::VK_ACCESS_SHADER_READ_BIT, vk::VK_ACCESS_TRANSFER_WRITE_BIT, hostVisibleBuffer.get(), 0, VK_WHOLE_SIZE);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    // Clear host visible buffer to known value before writing to it
    vk.cmdFillBuffer(*cmdBuffer, hostVisibleBuffer.get(), 0, VK_WHOLE_SIZE, 12);

    // Wait for fill to complete
    {
        const vk::VkBufferMemoryBarrier barrier =
            vk::makeBufferMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT,
                                        hostVisibleBuffer.get(), 0, VK_WHOLE_SIZE);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0, 0, nullptr, 1, &barrier, 0, nullptr);
    }

    // Copy DMA heap memory buffer -> host visible buffer with compute shader
    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineRead);
    vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                             &descriptorSet.get(), 0u, nullptr);
    vk.cmdDispatch(*cmdBuffer, bufferElementCount, 1u, 1u);

    // Wait for dispatch to complete, and make result in host visible buffer available to host reads
    {
        const vk::VkBufferMemoryBarrier barrier = vk::makeBufferMemoryBarrier(
            vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, hostVisibleBuffer.get(), 0, VK_WHOLE_SIZE);
        vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0,
                              0, nullptr, 1, &barrier, 0, nullptr);
    }

    vk::endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    vk::submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    vk::invalidateAlloc(vk, device, hostVisibleBuffer.getAllocation());

    // Validate the results

    {
        std::vector<BufferElementType> outputPattern(bufferElementCount);
        deMemcpy(outputPattern.data(), hostVisibleBuffer.getAllocation().getHostPtr(), bufferSize);

        for (size_t i = 0; i < bufferElementCount; ++i)
        {
            if (inputPattern[i] != outputPattern[i])
            {
                std::ostringstream msg;
                msg << "Comparison failed at index " << i << ": input = " << inputPattern[i]
                    << ", output = " << outputPattern[i];
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createDmaHeapTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "dma_heap_memory"));
    addFunctionCase(group.get(), std::string("allocate_and_bind"), checkDmaHeapMemory,
                    testDmaHeapMemoryAllocateAndBind);
    addFunctionCaseWithPrograms(group.get(), std::string("shader_access"), checkDmaHeapMemoryAccess,
                                initProgramsDmaHeapMemoryShaderAccess, testDmaHeapMemoryShaderAccess,
                                TestDmaHeapMemoryShaderAccessParams{0});
    addFunctionCaseWithPrograms(group.get(), std::string("shader_access_offset"), checkDmaHeapMemoryAccess,
                                initProgramsDmaHeapMemoryShaderAccess, testDmaHeapMemoryShaderAccess,
                                TestDmaHeapMemoryShaderAccessParams{20000});

    return group.release();
}

} // namespace memory
} // namespace vkt
