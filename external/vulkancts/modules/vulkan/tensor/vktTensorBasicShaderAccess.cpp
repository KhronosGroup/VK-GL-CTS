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
 * \brief Basic tensor compute shader read/write tests
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTensorTestsUtil.hpp"
#include "vktTestCase.hpp"

#include "vktTestGroupUtil.hpp"
#include "shaders/vktTensorShaders.hpp"
#include "shaders/vktTensorShaderUtil.hpp"
#include "vkTensorMemoryUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkTensorUtil.hpp"

#include "deMemory.h"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResource.hpp"

#include <cstdint>
#include <iostream>
#include <algorithm>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

TensorParameters calculateMaxDimensionCountParameters(const TensorParameters &baseParameters,
                                                      const vk::InstanceInterface &vki,
                                                      const vk::VkPhysicalDevice physicalDevice)
{
    const uint32_t rank = getTensorMaxDimensionCount(vki, physicalDevice);

    TensorParameters maxRankParameters = baseParameters;

    maxRankParameters.dimensions           = TensorDimensions(rank, 1);
    maxRankParameters.dimensions[0]        = 151;
    maxRankParameters.dimensions[rank - 2] = 3;
    maxRankParameters.dimensions[rank - 1] = 157;

    return maxRankParameters;
}

template <typename T>
class LinearTensorAccessTestInstance : public TestInstance
{
public:
    LinearTensorAccessTestInstance(Context &testCtx, const TensorParameters &parameters, const AccessVariant &variant,
                                   const vk::VkDeviceSize tensorOffset, const bool forceStagingBuffers)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_variant(variant)
        , m_tensorOffset(tensorOffset)
        , m_forceStagingBuffers(forceStagingBuffers)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
    const AccessVariant m_variant;
    const vk::VkDeviceSize m_tensorOffset;
    const bool m_forceStagingBuffers;
};

template <typename T>
class OptimalTensorAccessTestInstance : public TestInstance
{
public:
    OptimalTensorAccessTestInstance(Context &testCtx, const TensorParameters &parameters,
                                    const vk::VkDeviceSize tensorOffset)

        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_tensorOffset(tensorOffset)
    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
    const vk::VkDeviceSize m_tensorOffset;
};

template <typename T>
class LinearTensorAccessTestCase : public TestCase
{
private:
    static std::string buildTestName(const TensorParameters &parameters, const AccessVariant &variant,
                                     const vk::VkDeviceSize tensorOffset, const bool forceStagingBuffers)
    {
        std::ostringstream name;
        name << paramsToString(parameters, variant);
        if (tensorOffset != 0)
        {
            name << "_offset_" << tensorOffset;
        }
        if (forceStagingBuffers)
        {
            name << "_forced_staging";
        }

        return name.str();
    }

public:
    LinearTensorAccessTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters,
                               const AccessVariant &variant, const vk::VkDeviceSize tensorOffset = 0,
                               const bool forceStagingBuffers = false)
        : TestCase(testCtx, buildTestName(parameters, variant, tensorOffset, forceStagingBuffers))
        , m_parameters(parameters)
        , m_variant(variant)
        , m_tensorOffset(tensorOffset)
        , m_forceStagingBuffers(forceStagingBuffers)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        const TensorParameters *parameters = &m_parameters;

        // If no tensor shape was provided, it is a test of the maximum dimension count
        // Query the max dimension count the implementation supports and set up a shape accordingly
        TensorParameters maxRankParameters{};
        if (m_parameters.rank() == 0)
        {
            maxRankParameters =
                calculateMaxDimensionCountParameters(m_parameters, ctx.getInstanceInterface(), ctx.getPhysicalDevice());

            parameters = &maxRankParameters;
        }

        return new LinearTensorAccessTestInstance<T>(ctx, *parameters, m_variant, m_tensorOffset,
                                                     m_forceStagingBuffers);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        if (m_parameters.rank() > getTensorPhysicalDeviceProperties(context).maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!formatSupportTensorFlags(context, m_parameters.format, m_parameters.tiling,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }

        if (!m_parameters.packed() && !deviceSupportsNonPackedTensors(context))
        {
            TCU_THROW(NotSupportedError, "Non-packed tensors not supported");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        size_t rank = m_parameters.rank();

        // If no tensor shape was provided, it is a test of the maximum dimension count
        // Query the max dimension count the implementation supports
        if (rank == 0)
        {
            de::SharedPtr<const ContextManager> contextManager = getContextManager();

            // We can't generate the source for max dimension count without a
            // context which has a physical device
            if (!contextManager || contextManager->getPhysicalDevice() == VK_NULL_HANDLE)
            {
                return;
            }

            rank =
                getTensorMaxDimensionCount(contextManager->getInstanceInterface(), contextManager->getPhysicalDevice());
        }

        programCollection.glslSources.add("comp")
            << glu::ComputeSource(genShaderTensorAccess(rank, m_parameters.format, m_variant));
    }

private:
    const TensorParameters m_parameters;
    const AccessVariant m_variant;
    const vk::VkDeviceSize m_tensorOffset;
    const bool m_forceStagingBuffers;
};

template <typename T>
class OptimalTensorAccessTestCase : public TestCase
{
private:
    static std::string buildTestName(const TensorParameters &parameters, const vk::VkDeviceSize tensorOffset)
    {
        std::ostringstream name;
        name << paramsToString(parameters);
        if (tensorOffset != 0)
        {
            name << "_offset_" << tensorOffset;
        }

        return name.str();
    }

public:
    OptimalTensorAccessTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters,
                                const vk::VkDeviceSize tensorOffset = 0)
        : TestCase(testCtx, buildTestName(parameters, tensorOffset))
        , m_parameters(parameters)
        , m_tensorOffset(tensorOffset)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        const TensorParameters *parameters = &m_parameters;

        // If no tensor shape was provided, it is a test of the maximum dimension count
        // Query the max dimension count the implementation supports and set up a shape accordingly
        TensorParameters maxRankParameters{};
        if (m_parameters.rank() == 0)
        {
            maxRankParameters =
                calculateMaxDimensionCountParameters(m_parameters, ctx.getInstanceInterface(), ctx.getPhysicalDevice());

            parameters = &maxRankParameters;
        }

        return new OptimalTensorAccessTestInstance<T>(ctx, *parameters, m_tensorOffset);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        if (m_parameters.rank() > getTensorPhysicalDeviceProperties(context).maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!formatSupportTensorFlags(context, m_parameters.format, m_parameters.tiling,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        size_t rank = m_parameters.rank();

        // If no tensor shape was provided, it is a test of the maximum dimension count
        // Query the max dimension count the implementation supports
        if (rank == 0)
        {
            // We can't generate the source for max dimension count without a
            // context which has a physical device
            de::SharedPtr<const ContextManager> contextManager = getContextManager();

            if (!contextManager || contextManager->getPhysicalDevice() == VK_NULL_HANDLE)
            {
                return;
            }

            rank =
                getTensorMaxDimensionCount(contextManager->getInstanceInterface(), contextManager->getPhysicalDevice());
        }

        programCollection.glslSources.add("read_buffer_comp")
            << glu::ComputeSource(genShaderTensorAccess(rank, m_parameters.format, AccessVariant::READ_FROM_BUFFER));
        programCollection.glslSources.add("write_buffer_comp")
            << glu::ComputeSource(genShaderTensorAccess(rank, m_parameters.format, AccessVariant::WRITE_TO_BUFFER));
    }

private:
    TensorParameters m_parameters;
    const vk::VkDeviceSize m_tensorOffset;
};

template <typename T>
tcu::TestStatus LinearTensorAccessTestInstance<T>::iterate()
{

    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice device                 = m_context.getDevice();
    const VkQueue queue                   = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator                  = m_context.getDefaultAllocator();

    const bool needCustomTensorAllocator = m_tensorOffset != 0;

    // Allocate custom allocator tensor if we are allocating at an offset
    std::unique_ptr<Allocator> customTensorAllocator;
    if (needCustomTensorAllocator)
    {
        vk::VkPhysicalDeviceProperties physicalDeviceProperties{};
        vki.getPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vki.getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        vk::SimpleAllocator::OptionalOffsetParams offsetParams;
        if (m_tensorOffset != 0)
        {
            offsetParams = {physicalDeviceProperties.limits.nonCoherentAtomSize, m_tensorOffset};
        }

        customTensorAllocator.reset(new SimpleAllocator(vk, device, memoryProperties, offsetParams));
    }

    Allocator &tensorAllocator = needCustomTensorAllocator ? *customTensorAllocator : allocator;

    // Create a tensor and memory for it

    const uint32_t elements =
        std::accumulate(m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), 1, std::multiplies<size_t>());

    const VkTensorDescriptionARM tensorDesc =
        makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions, m_parameters.strides,
                              VK_TENSOR_USAGE_SHADER_BIT_ARM);
    VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);

    const TensorWithMemory tensor(vk, device, tensorAllocator, tensorCreateInfo, vk::MemoryRequirement::Any);

    const Move<vk::VkTensorViewARM> tensorView = makeTensorView(vk, device, *tensor, m_parameters.format);

    // Create a buffer and host-visible memory for it
    const size_t bufferSize = elements * sizeof(T);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // prepare tensor and buffer

    // Memory used to transfer data to/from tensor and to compare with the buffer during verification
    StridedMemoryUtils<T> tensorData(m_parameters.dimensions, m_parameters.strides);

    {
        const Allocation &bufferAllocation = buffer.getAllocation();
        StridedMemoryUtils<T> bufferMemory({uint32_t(elements)}, {}, bufferAllocation.getHostPtr());

        if (m_variant == AccessVariant::WRITE_TO_BUFFER)
        {
            tensorData.fill();
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, tensorData.data(),
                           tensorData.memorySize(), m_forceStagingBuffers);
            bufferMemory.clear();
        }
        else
        {
            tensorData.clear();
            bufferMemory.fill();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, m_forceStagingBuffers);
        }

        flushAlloc(vk, device, bufferAllocation);
    }

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Set the bindings

    DescriptorSetUpdateBuilder updateBuilder;
    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, elements * sizeof(T));
    const VkWriteDescriptorSetTensorARM tensorDescriptorInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr,
                                                             1, &*tensorView};

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_TENSOR_ARM,
                     &tensorDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .update(vk, device);

    // Perform the computation

    {
        // Build shader

        const ProgramBinary &binary = m_context.getBinaryCollection().get("comp");
        const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, binary, 0u));

        // Setup pipeline

        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
        const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        const VkTensorMemoryBarrierARM tensorBarrier =
            makeTensorMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, 0, 0, *tensor);

        const VkBufferMemoryBarrier bufferBarrier =
            makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        vk.cmdDispatch(*cmdBuffer, elements, 1u, 1u);

        if (m_variant == AccessVariant::WRITE_TO_BUFFER)
        {
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                  nullptr, 1, &bufferBarrier, 0, nullptr);
        }
        else // READ_FROM_BUFFER
        {
            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.pNext = &tensorBarrier;
            vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
        }

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    {
        const Allocation &bufferAllocation = buffer.getAllocation();

        invalidateAlloc(vk, device, bufferAllocation);

        if (m_variant == AccessVariant::READ_FROM_BUFFER)
        {
            downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, tensorData.data(),
                               tensorData.memorySize(), m_forceStagingBuffers);
        }

        StridedMemoryUtils<T> bufferMemory({uint32_t(elements)}, {}, bufferAllocation.getHostPtr());

        for (size_t element_idx = 0; element_idx < elements; ++element_idx)
        {
            if (tensorData[element_idx] != bufferMemory[element_idx])
            {

                std::ostringstream msg;
                msg << "Comparison failed at index " << element_idx << ": tensor = " << int(tensorData[element_idx])
                    << ", buffer = " << int(bufferMemory[element_idx]);
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
}

template <typename T>
tcu::TestStatus OptimalTensorAccessTestInstance<T>::iterate()
{
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice device                 = m_context.getDevice();
    const VkQueue queue                   = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex       = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator                  = m_context.getDefaultAllocator();

    const bool needCustomTensorAllocator = m_tensorOffset != 0;

    // Allocate custom allocator tensor if we are allocating at an offset
    std::unique_ptr<Allocator> customTensorAllocator;
    if (needCustomTensorAllocator)
    {
        vk::VkPhysicalDeviceProperties physicalDeviceProperties{};
        vki.getPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vki.getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        vk::SimpleAllocator::OptionalOffsetParams offsetParams;
        if (m_tensorOffset != 0)
        {
            offsetParams = {physicalDeviceProperties.limits.nonCoherentAtomSize, m_tensorOffset};
        }

        customTensorAllocator.reset(new SimpleAllocator(vk, device, memoryProperties, offsetParams));
    }

    Allocator &tensorAllocator = needCustomTensorAllocator ? *customTensorAllocator : allocator;

    // Create a tensor and its support memory

    const uint32_t elements =
        std::accumulate(m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), 1, std::multiplies<size_t>());

    const VkTensorDescriptionARM tensorDesc =
        makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions, m_parameters.strides,
                              VK_TENSOR_USAGE_SHADER_BIT_ARM);
    VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);

    const TensorWithMemory tensor(vk, device, tensorAllocator, tensorCreateInfo, vk::MemoryRequirement::Any);

    const Move<vk::VkTensorViewARM> tensorView = makeTensorView(vk, device, *tensor, m_parameters.format);

    // Create two buffers and host-visible memory for them

    const size_t bufferSize = elements * sizeof(T);
    const BufferWithMemory srcBuffer(vk, device, allocator,
                                     makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                     MemoryRequirement::HostVisible);
    const BufferWithMemory dstBuffer(vk, device, allocator,
                                     makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                     MemoryRequirement::HostVisible);

    // prepare buffers

    {
        const Allocation &srcBufferAllocation = srcBuffer.getAllocation();
        const Allocation &dstBufferAllocation = dstBuffer.getAllocation();

        StridedMemoryUtils<T> srcBufferMemory({uint32_t(elements)}, {}, srcBufferAllocation.getHostPtr());
        StridedMemoryUtils<T> dstBufferMemory({uint32_t(elements)}, {}, dstBufferAllocation.getHostPtr());

        srcBufferMemory.fill();
        dstBufferMemory.clear();

        flushAlloc(vk, device, srcBufferAllocation);
        flushAlloc(vk, device, dstBufferAllocation);
    }

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> bufferToTensorDescriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorPool> tensorToBufferDescriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> bufferToTensorDescriptorSet(
        makeDescriptorSet(vk, device, *bufferToTensorDescriptorPool, *descriptorSetLayout));
    const Unique<VkDescriptorSet> tensorToBufferDescriptorSet(
        makeDescriptorSet(vk, device, *tensorToBufferDescriptorPool, *descriptorSetLayout));

    // Set the bindings

    DescriptorSetUpdateBuilder updateBuilder;
    const VkDescriptorBufferInfo srcBufferDescriptorInfo =
        makeDescriptorBufferInfo(*srcBuffer, 0ull, elements * sizeof(T));
    const VkDescriptorBufferInfo dstBufferDescriptorInfo =
        makeDescriptorBufferInfo(*dstBuffer, 0ull, elements * sizeof(T));
    const VkWriteDescriptorSetTensorARM tensorDescriptorInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr,
                                                             1, &*tensorView};

    DescriptorSetUpdateBuilder()
        .writeSingle(*bufferToTensorDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_TENSOR_ARM, &tensorDescriptorInfo)
        .writeSingle(*bufferToTensorDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &srcBufferDescriptorInfo)
        .update(vk, device);

    DescriptorSetUpdateBuilder()
        .writeSingle(*tensorToBufferDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_TENSOR_ARM, &tensorDescriptorInfo)
        .writeSingle(*tensorToBufferDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dstBufferDescriptorInfo)
        .update(vk, device);

    // Perform the computation

    {
        // Build shaders

        const ProgramBinary &bufferToTensorBinary = m_context.getBinaryCollection().get("read_buffer_comp");
        const ProgramBinary &tensorToBufferBinary = m_context.getBinaryCollection().get("write_buffer_comp");

        const Unique<VkShaderModule> bufferToTensorShaderModule(
            createShaderModule(vk, device, bufferToTensorBinary, 0u));
        const Unique<VkShaderModule> tensorToBufferShaderModule(
            createShaderModule(vk, device, tensorToBufferBinary, 0u));

        // Setup pipeline

        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

        const Unique<VkPipeline> bufferToTensorPipeline(
            makeComputePipeline(vk, device, *pipelineLayout, *bufferToTensorShaderModule));
        const Unique<VkPipeline> tensorToBufferPipeline(
            makeComputePipeline(vk, device, *pipelineLayout, *tensorToBufferShaderModule));

        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        const VkTensorMemoryBarrierARM tensorBarrier =
            makeTensorMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, 0, 0, *tensor);

        const VkBufferMemoryBarrier bufferBarrier =
            makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *dstBuffer, 0u, bufferSize);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *bufferToTensorPipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &bufferToTensorDescriptorSet.get(), 0u, nullptr);
        vk.cmdDispatch(*cmdBuffer, elements, 1u, 1u);

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = &tensorBarrier;
        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *tensorToBufferPipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &tensorToBufferDescriptorSet.get(), 0u, nullptr);
        vk.cmdDispatch(*cmdBuffer, elements, 1u, 1u);

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                              nullptr, 1, &bufferBarrier, 0, nullptr);

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    {
        const Allocation &srcBufferAllocation = srcBuffer.getAllocation();
        const Allocation &dstBufferAllocation = dstBuffer.getAllocation();

        invalidateAlloc(vk, device, srcBufferAllocation);
        invalidateAlloc(vk, device, dstBufferAllocation);

        StridedMemoryUtils<T> srcBufferMemory({uint32_t(elements)}, {}, srcBufferAllocation.getHostPtr());
        StridedMemoryUtils<T> dstBufferMemory({uint32_t(elements)}, {}, dstBufferAllocation.getHostPtr());

        for (size_t element_idx = 0; element_idx < elements; ++element_idx)
        {
            if (srcBufferMemory[element_idx] != dstBufferMemory[element_idx])
            {
                std::ostringstream msg;
                msg << "Comparison failed at index " << element_idx
                    << ": source buffer = " << int(srcBufferMemory[element_idx])
                    << ", destination buffer = " << int(dstBufferMemory[element_idx]);
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
}

template <typename T>
void addShaderAccessTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions shapes[] = {
        {71693},
        {263, 269},
        {37, 43, 47},
        {13, 17, 19, 23},
    };

    const TensorDimensions &shape_4d = shapes[3];

    for (const VkFormat format : getTestFormats<T>())
    {
        for (const TensorDimensions &shape : shapes)
        {
            const size_t rank        = shape.size();
            const size_t elementSize = getFormatSize(format);

            // Implicitly packed linear
            {
                const TensorParameters params{format, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                         AccessVariant::READ_FROM_BUFFER));
                testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                         AccessVariant::WRITE_TO_BUFFER));
            }

            // Explicit non-packed strides, not applicable to rank 1 tensors
            if (rank > 1)
            {
                TensorStrides paddedStrides(rank);
                paddedStrides[rank - 1] = elementSize;
                for (size_t i = 2; i <= rank; ++i)
                {
                    paddedStrides[rank - i] = paddedStrides[rank - i + 1] * shape[rank - i + 1] + 13 * elementSize;
                }

                const TensorParameters params{format, VK_TENSOR_TILING_LINEAR_ARM, shape, paddedStrides};
                testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                         AccessVariant::READ_FROM_BUFFER));
                testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                         AccessVariant::WRITE_TO_BUFFER));
            }

            // Explicit packed strides
            {
                const TensorStrides packedStrides = getTensorStrides(shape, elementSize);
                const TensorParameters params{format, VK_TENSOR_TILING_LINEAR_ARM, shape, packedStrides};
                testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                         AccessVariant::READ_FROM_BUFFER));
                testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                         AccessVariant::WRITE_TO_BUFFER));
            }

            // Optimal
            {
                const TensorParameters params{format, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
                testCaseGroup.addChild(new OptimalTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params));
            }
        }
    }

    // Tests to force use of staging buffer even when tensor memory is host visible
    {
        const TensorParameters forcedStagingBufferParameters{
            getTestFormats<T>()[0], VK_TENSOR_TILING_LINEAR_ARM, shape_4d, {}};
        testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(
            testCaseGroup.getTestContext(), forcedStagingBufferParameters, AccessVariant::WRITE_TO_BUFFER, 0, true));
        testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(
            testCaseGroup.getTestContext(), forcedStagingBufferParameters, AccessVariant::READ_FROM_BUFFER, 0, true));
    }

    // Tests binding tensor to offset within allocation
    {
        const TensorParameters forcedStagingBufferParameters{
            getTestFormats<T>()[0], VK_TENSOR_TILING_LINEAR_ARM, shape_4d, {}};
        testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(
            testCaseGroup.getTestContext(), forcedStagingBufferParameters, AccessVariant::WRITE_TO_BUFFER, 2000));
        testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(
            testCaseGroup.getTestContext(), forcedStagingBufferParameters, AccessVariant::READ_FROM_BUFFER, 2000));
    }

    // Test max dimension count supported by implementation
    for (const VkFormat format : getTestFormats<T>())
    {
        // Linear packed
        {
            const TensorParameters params{format, VK_TENSOR_TILING_LINEAR_ARM, {}, {}};
            testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                     AccessVariant::WRITE_TO_BUFFER));
            testCaseGroup.addChild(new LinearTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                     AccessVariant::READ_FROM_BUFFER));
        }

        // Optimal
        {
            const TensorParameters params{format, VK_TENSOR_TILING_OPTIMAL_ARM, {}, {}};
            testCaseGroup.addChild(new OptimalTensorAccessTestCase<T>(testCaseGroup.getTestContext(), params));
        }
    }
}

} // namespace

tcu::TestCaseGroup *createBasicAccessTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "basic_access", "Basic tensor shader access tests"));

    addShaderAccessTests<uint64_t>(*group);
    addShaderAccessTests<uint32_t>(*group);
    addShaderAccessTests<uint16_t>(*group);
    addShaderAccessTests<uint8_t>(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
