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
 * \brief Tensor shader array access tests
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

uint32_t calculateMaxArraySizeSupported(const vk::InstanceInterface &vki, const vk::VkPhysicalDevice physicalDevice,
                                        const TensorParameters &params)
{
    const uint32_t elementSize                                 = static_cast<uint32_t>(getFormatSize(params.format));
    const VkPhysicalDeviceTensorPropertiesARM tensorProperties = getTensorPhysicalDeviceProperties(vki, physicalDevice);

    const uint32_t maxArrayAccessSizeInElements = tensorProperties.maxTensorShaderAccessSize / elementSize;

    return std::min(maxArrayAccessSizeInElements, tensorProperties.maxTensorShaderAccessArrayLength);
}

template <typename T>
class TensorArrayReadWriteTestInstance : public TestInstance
{
public:
    TensorArrayReadWriteTestInstance(Context &testCtx, const TensorParameters &parameters, const AccessVariant variant,
                                     const uint32_t arraySize)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_variant(variant)
        , m_arraySize(arraySize)
    {
    }

    tcu::TestStatus iterate() override;

private:
    size_t idiv_round_up_size_t(const size_t a, const size_t b)
    {
        DE_ASSERT(a < SIZE_MAX - b);
        return static_cast<size_t>((a + b - 1) / b);
    }

    const TensorParameters &m_parameters;
    const AccessVariant m_variant;
    const uint32_t m_arraySize;
};

template <typename T>
class TensorArrayReadWriteTestCase : public TestCase
{
public:
    TensorArrayReadWriteTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters, AccessVariant variant,
                                 const uint32_t arraySize)
        : TestCase(testCtx, paramsToString(parameters, variant) + "_array_size_" +
                                (arraySize == 0 ? std::string("max") : de::toString(arraySize)))
        , m_parameters(parameters)
        , m_variant(variant)
        , m_arraySize(arraySize)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        uint32_t arraySize = m_arraySize;

        // If provided arraySize is 0, test the maximum array size supported by implementation
        if (arraySize == 0)
        {
            arraySize =
                calculateMaxArraySizeSupported(ctx.getInstanceInterface(), ctx.getPhysicalDevice(), m_parameters);
        }

        return new TensorArrayReadWriteTestInstance<T>(ctx, m_parameters, m_variant, arraySize);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        const VkPhysicalDeviceTensorPropertiesARM tensorProperties = getTensorPhysicalDeviceProperties(context);

        if (m_parameters.rank() > tensorProperties.maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }
        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }
        if (!formatSupportTensorFlags(context, m_parameters.format, m_parameters.tiling,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Device does not support the tensor flags for this tiling and format");
        }

        if (m_arraySize > tensorProperties.maxTensorShaderAccessArrayLength)
        {
            TCU_THROW(NotSupportedError, "Device does not support this access array length");
        }

        if (m_arraySize * getFormatSize(m_parameters.format) > tensorProperties.maxTensorShaderAccessSize)
        {
            TCU_THROW(NotSupportedError, "Device does not support this access size");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        uint32_t arraySize = m_arraySize;

        // If provided arraySize is 0, test the maximum array size supported by implementation
        if (arraySize == 0)
        {
            // We can't generate the source for max array size without a
            // context which has a physical device
            de::SharedPtr<const ContextManager> contextManager = getContextManager();

            if (!contextManager || contextManager->getPhysicalDevice() == VK_NULL_HANDLE)
            {
                return;
            }

            arraySize = calculateMaxArraySizeSupported(contextManager->getInstanceInterface(),
                                                       contextManager->getPhysicalDevice(), m_parameters);
        }

        programCollection.glslSources.add("comp") << glu::ComputeSource(
            genShaderArrayAccess(m_parameters.dimensions.size(), m_variant, m_parameters.format, arraySize));
    }

private:
    TensorParameters m_parameters;
    const AccessVariant m_variant;
    const uint32_t m_arraySize;
};

template <typename T>
class OptimalTensorArrayReadWriteTestInstance : public TestInstance
{
public:
    OptimalTensorArrayReadWriteTestInstance(Context &testCtx, const TensorParameters &parameters,
                                            const AccessVariant variant, const uint32_t arraySize)

        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_variant(variant)
        , m_arraySize(arraySize)
    {
    }

    tcu::TestStatus iterate() override;

private:
    size_t idiv_round_up_size_t(const size_t a, const size_t b)
    {
        DE_ASSERT(a < SIZE_MAX - b);
        return static_cast<size_t>((a + b - 1) / b);
    }

    const TensorParameters &m_parameters;
    const AccessVariant m_variant;
    const uint32_t m_arraySize;
};

template <typename T>
class OptimalTensorArrayReadWriteTestCase : public TestCase
{
public:
    OptimalTensorArrayReadWriteTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters,
                                        AccessVariant variant, const uint32_t arraySize)
        : TestCase(testCtx, paramsToString(parameters, variant) + "_array_size_" +
                                (arraySize == 0 ? std::string("max") : de::toString(arraySize)))
        , m_parameters(parameters)
        , m_variant(variant)
        , m_arraySize(arraySize)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        uint32_t arraySize = m_arraySize;

        // If provided arraySize is 0, test the maximum array size supported by implementation
        if (arraySize == 0)
        {
            arraySize =
                calculateMaxArraySizeSupported(ctx.getInstanceInterface(), ctx.getPhysicalDevice(), m_parameters);
        }

        return new OptimalTensorArrayReadWriteTestInstance<T>(ctx, m_parameters, m_variant, arraySize);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        const VkPhysicalDeviceTensorPropertiesARM tensorProperties = getTensorPhysicalDeviceProperties(context);

        if (m_parameters.rank() > tensorProperties.maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }

        if (!formatSupportTensorFlags(context, m_parameters.format, m_parameters.tiling,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Device does not support the tensor flags for this tiling and format");
        }

        if (!m_parameters.packed() && !deviceSupportsNonPackedTensors(context))
        {
            TCU_THROW(NotSupportedError, "Non-packed tensors not supported");
        }

        if (m_arraySize > tensorProperties.maxTensorShaderAccessArrayLength)
        {
            TCU_THROW(NotSupportedError, "Device does not support this access array length");
        }

        if (m_arraySize * getFormatSize(m_parameters.format) > tensorProperties.maxTensorShaderAccessSize)
        {
            TCU_THROW(NotSupportedError, "Device does not support this access size");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        uint32_t arraySize = m_arraySize;

        // If provided arraySize is 0, test the maximum array size supported by implementation
        if (arraySize == 0)
        {
            // We can't generate the source for max array size without a
            // context which has a physical device
            de::SharedPtr<const ContextManager> contextManager = getContextManager();

            if (!contextManager || contextManager->getPhysicalDevice() == VK_NULL_HANDLE)
            {
                return;
            }

            arraySize = calculateMaxArraySizeSupported(contextManager->getInstanceInterface(),
                                                       contextManager->getPhysicalDevice(), m_parameters);
        }

        programCollection.glslSources.add("comp") << glu::ComputeSource(
            genShaderArrayAccess(m_parameters.dimensions.size(), m_variant, m_parameters.format, arraySize));
    }

private:
    TensorParameters m_parameters;
    const AccessVariant m_variant;
    const uint32_t m_arraySize;
};

template <typename T>
tcu::TestStatus TensorArrayReadWriteTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create a Tensor

    const uint32_t elements =
        std::accumulate(m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), 1, std::multiplies<size_t>());

    const VkTensorDescriptionARM tensorDesc =
        makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions, m_parameters.strides,
                              VK_TENSOR_USAGE_SHADER_BIT_ARM);
    const VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
    const TensorWithMemory tensor(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any);

    const Move<vk::VkTensorViewARM> tensorView = makeTensorView(vk, device, *tensor, m_parameters.format);

    // Create a buffer
    const size_t bufferSize = elements * sizeof(T);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Memory used to transfer data to/from tensor and to compare with the buffer during verification
    StridedMemoryUtils<T> tensorData(m_parameters.dimensions, m_parameters.strides);

    {
        const Allocation &bufferAllocation = buffer.getAllocation();

        StridedMemoryUtils<T> bufferMemory({uint32_t(elements)}, {}, bufferAllocation.getHostPtr());

        if (m_variant == AccessVariant::ARRAY_READ)
        {
            // Fill input tensor
            tensorData.fill();
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, tensorData.data(),
                           tensorData.memorySize());
            bufferMemory.clear();
        }
        else
        {
            tensorData.clear();
            bufferMemory.fill();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, tensor);
        }

        flushAlloc(vk, device, bufferAllocation);
    }

    // Create Descriptor Set and Layout

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

    // Bind tensor and buffer

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

        // Size of the innermost dimension
        const uint64_t innermost_elements = *(m_parameters.dimensions.end() - 1);
        // How many arrays of size m_arraySize are required to read/write the entire innermost dimension
        const size_t innermost_arrays = idiv_round_up_size_t(static_cast<size_t>(innermost_elements), m_arraySize);

        const size_t outer_count = static_cast<size_t>(elements / innermost_elements);
        const size_t inner_count = innermost_arrays;

        beginCommandBuffer(vk, *cmdBuffer);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        vk.cmdDispatch(*cmdBuffer, static_cast<uint32_t>(inner_count), static_cast<uint32_t>(outer_count), 1u);

        if (m_variant == AccessVariant::ARRAY_READ)
        {
            const VkBufferMemoryBarrier bufferBarrier =
                makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                  nullptr, 1, &bufferBarrier, 0, nullptr);
        }
        else // ARRAY_WRITE
        {
            const VkTensorMemoryBarrierARM tensorBarrier =
                makeTensorMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, 0, 0, *tensor);

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.pNext = &tensorBarrier;
            vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
        }

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        {
            const Allocation &bufferAllocation = buffer.getAllocation();

            invalidateAlloc(vk, device, bufferAllocation);

            if (m_variant == AccessVariant::ARRAY_WRITE)
            {
                downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, tensorData.data(),
                                   tensorData.memorySize());
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
    }
    return tcu::TestStatus::pass("Tensor test succeeded");
}

template <typename T>
tcu::TestStatus OptimalTensorArrayReadWriteTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create a Tensor

    const uint32_t elements =
        std::accumulate(m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), 1, std::multiplies<size_t>());

    const VkTensorDescriptionARM tensorDesc = makeTensorDescription(
        m_parameters.tiling, m_parameters.format, m_parameters.dimensions, m_parameters.strides,
        VK_TENSOR_USAGE_SHADER_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
    const TensorWithMemory tensor(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any);

    // Staging linear tensor to copy to or from the optimal tensor
    const VkTensorDescriptionARM linearTensorDesc =
        makeTensorDescription(VK_TENSOR_TILING_LINEAR_ARM, m_parameters.format, m_parameters.dimensions, {},
                              VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM linearTensorCreateInfo = makeTensorCreateInfo(&linearTensorDesc);
    const TensorWithMemory linearTensor(vk, device, allocator, linearTensorCreateInfo, vk::MemoryRequirement::Any);

    const Move<vk::VkTensorViewARM> tensorView = makeTensorView(vk, device, *tensor, m_parameters.format);

    // Create a buffer
    const size_t bufferSize = elements * sizeof(T);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Memory used to transfer data to/from tensor and to compare with the buffer during verification
    StridedMemoryUtils<T> tensorData(m_parameters.dimensions, m_parameters.strides);

    {
        const Allocation &bufferAllocation = buffer.getAllocation();

        StridedMemoryUtils<T> bufferMemory({uint32_t(elements)}, {}, bufferAllocation.getHostPtr());

        if (m_variant == AccessVariant::ARRAY_READ)
        {
            // Fill input tensor
            tensorData.fill();
            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, linearTensor, tensorData.data(),
                           tensorData.memorySize());
            bufferMemory.clear();
        }
        else
        {
            tensorData.clear();
            clearTensor(vk, device, allocator, queue, queueFamilyIndex, linearTensor);
            bufferMemory.fill();
        }

        flushAlloc(vk, device, bufferAllocation);
    }

    // Create Descriptor Set and Layout

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

    // Bind tensor and buffer

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

        // Size of the innermost dimension
        const uint64_t innermost_elements = *(m_parameters.dimensions.end() - 1);
        // How many arrays of size m_arraySize are required to read/write the entire innermost dimension
        const size_t innermost_arrays = idiv_round_up_size_t(static_cast<size_t>(innermost_elements), m_arraySize);

        const size_t outer_count = static_cast<size_t>(elements / innermost_elements);
        const size_t inner_count = innermost_arrays;

        beginCommandBuffer(vk, *cmdBuffer);

        // Initialize the optimal tensor
        {
            VkTensorCopyARM tensorRegions{};
            tensorRegions.sType          = VK_STRUCTURE_TYPE_TENSOR_COPY_ARM;
            tensorRegions.dimensionCount = static_cast<uint32_t>(m_parameters.dimensions.size());

            VkCopyTensorInfoARM copyInfo{};
            copyInfo.sType       = VK_STRUCTURE_TYPE_COPY_TENSOR_INFO_ARM;
            copyInfo.srcTensor   = *linearTensor;
            copyInfo.dstTensor   = *tensor;
            copyInfo.pRegions    = &tensorRegions;
            copyInfo.regionCount = 1;

            vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);

            const VkTensorMemoryBarrierARM tensorInitBarrier =
                makeTensorMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, 0, 0, *tensor);

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.pNext = &tensorInitBarrier;
            vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
        }

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        vk.cmdDispatch(*cmdBuffer, static_cast<uint32_t>(inner_count), static_cast<uint32_t>(outer_count), 1u);

        if (m_variant == AccessVariant::ARRAY_READ)
        {
            const VkBufferMemoryBarrier bufferBarrier =
                makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                  nullptr, 1, &bufferBarrier, 0, nullptr);
        }
        else // ARRAY_WRITE
        {
            const VkTensorMemoryBarrierARM tensorReadbackBarrier =
                makeTensorMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0, 0, *tensor);

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.pNext = &tensorReadbackBarrier;
            vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

            VkTensorCopyARM tensorRegions{};
            tensorRegions.sType          = VK_STRUCTURE_TYPE_TENSOR_COPY_ARM;
            tensorRegions.dimensionCount = static_cast<uint32_t>(m_parameters.dimensions.size());

            VkCopyTensorInfoARM copyInfo{};
            copyInfo.sType       = VK_STRUCTURE_TYPE_COPY_TENSOR_INFO_ARM;
            copyInfo.srcTensor   = *tensor;
            copyInfo.dstTensor   = *linearTensor;
            copyInfo.pRegions    = &tensorRegions;
            copyInfo.regionCount = 1;

            vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);
        }

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        {
            const Allocation &bufferAllocation = buffer.getAllocation();

            invalidateAlloc(vk, device, bufferAllocation);

            if (m_variant == AccessVariant::ARRAY_WRITE)
            {
                downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, linearTensor, tensorData.data(),
                                   tensorData.memorySize());
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
    }
    return tcu::TestStatus::pass("Tensor test succeeded");
}

template <typename T>
void addTensorArrayTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions shape{13, 17, 19, 23};

    for (const VkFormat format : getTestFormats<T>())
    {
        for (const unsigned int arraySize : {2, 3, 4})
        {
            // Implicitly packed linear
            {
                const TensorParameters params{format, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                testCaseGroup.addChild(new TensorArrayReadWriteTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                           AccessVariant::ARRAY_READ, arraySize));
                testCaseGroup.addChild(new TensorArrayReadWriteTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                           AccessVariant::ARRAY_WRITE, arraySize));
            }

            // Optimal
            {
                const TensorParameters params{format, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
                testCaseGroup.addChild(new OptimalTensorArrayReadWriteTestCase<T>(
                    testCaseGroup.getTestContext(), params, AccessVariant::ARRAY_READ, arraySize));
                testCaseGroup.addChild(new OptimalTensorArrayReadWriteTestCase<T>(
                    testCaseGroup.getTestContext(), params, AccessVariant::ARRAY_WRITE, arraySize));
            }
        }

        // Test max array accesses supported by implementation
        {
            const unsigned int arraySize = 0; // Zero means test the max

            // Implicitly packed linear
            {
                const TensorParameters params{format, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                testCaseGroup.addChild(new TensorArrayReadWriteTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                           AccessVariant::ARRAY_READ, arraySize));
                testCaseGroup.addChild(new TensorArrayReadWriteTestCase<T>(testCaseGroup.getTestContext(), params,
                                                                           AccessVariant::ARRAY_WRITE, arraySize));
            }

            // Optimal
            {
                const TensorParameters params{format, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
                testCaseGroup.addChild(new OptimalTensorArrayReadWriteTestCase<T>(
                    testCaseGroup.getTestContext(), params, AccessVariant::ARRAY_READ, arraySize));
                testCaseGroup.addChild(new OptimalTensorArrayReadWriteTestCase<T>(
                    testCaseGroup.getTestContext(), params, AccessVariant::ARRAY_WRITE, arraySize));
            }
        }
    }
}

} // namespace

tcu::TestCaseGroup *createArrayAccessTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "array_access", "Tensor shader array access tests"));

    addTensorArrayTests<uint64_t>(*group);
    addTensorArrayTests<uint32_t>(*group);
    addTensorArrayTests<uint16_t>(*group);
    addTensorArrayTests<uint8_t>(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
