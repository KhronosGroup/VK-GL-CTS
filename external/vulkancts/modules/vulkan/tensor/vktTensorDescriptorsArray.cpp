/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 ARM Ltd.
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
 * \brief Tensor descriptors array update tests
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <sstream>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

enum UpdateRangeBits
{
    RANGE_START = 0x1,
    RANGE_MID   = 0x2,
    RANGE_END   = 0x4,
};

using UpdateRangeFlags = uint32_t;

template <typename T>
class DescriptorsArrayTestInstance : public TestInstance
{
public:
    DescriptorsArrayTestInstance(Context &testCtx, const TensorParameters &parameters, const uint32_t &descriptorCount,
                                 const UpdateRangeFlags &updateRangeFlags)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_descriptorCount(descriptorCount)
        , m_updateRange(updateRangeFlags)
    {
    }

    void initialiseDescriptorUpdateMask(std::vector<bool> &descriptorUpdateMask);

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
    const uint32_t m_descriptorCount;
    const UpdateRangeFlags m_updateRange;
};

template <typename T>
void DescriptorsArrayTestInstance<T>::initialiseDescriptorUpdateMask(std::vector<bool> &descriptorUpdateMask)
{
    const uint32_t b0 = m_descriptorCount / 3;
    const uint32_t b1 = (2 * m_descriptorCount) / 3;

    if (m_updateRange & RANGE_START)
    {
        for (uint32_t i = 0; i < b0; ++i)
        {
            descriptorUpdateMask[i] = true;
        }
    }

    if (m_updateRange & RANGE_MID)
    {
        for (uint32_t i = b0; i < b1; ++i)
        {
            descriptorUpdateMask[i] = true;
        }
    }

    if (m_updateRange & RANGE_END)
    {
        for (uint32_t i = b1; i < m_descriptorCount; ++i)
        {
            descriptorUpdateMask[i] = true;
        }
    }
}

template <typename T>
tcu::TestStatus DescriptorsArrayTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Mark tensor descriptors for update

    std::vector<bool> descriptorUpdateMask(m_descriptorCount, false);
    initialiseDescriptorUpdateMask(descriptorUpdateMask);

    // Create containers for tensors, tensor views and expected tensor elements

    std::vector<de::MovePtr<TensorWithMemory>> tensors(m_descriptorCount);
    std::vector<Move<vk::VkTensorViewARM>> tensorViews(m_descriptorCount);
    std::vector<StridedMemoryUtils<T>> expectedTensorData(m_descriptorCount);

    // Generate tensor descriptor indices

    std::vector<uint32_t> tensorDescriptorIndices(m_descriptorCount);
    std::iota(tensorDescriptorIndices.begin(), tensorDescriptorIndices.end(), 0);

    // Fill tensors

    for (const auto &descIdx : tensorDescriptorIndices)
    {
        const VkTensorDescriptionARM tensorDesc =
            makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions,
                                  m_parameters.strides, VK_TENSOR_USAGE_SHADER_BIT_ARM);
        VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
        tensors[descIdx]                       = de::MovePtr<TensorWithMemory>(
            new TensorWithMemory(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any));
        tensorViews[descIdx] =
            Move<VkTensorViewARM>(makeTensorView(vk, device, tensors[descIdx]->get(), m_parameters.format));

        expectedTensorData[descIdx] = StridedMemoryUtils<T>(m_parameters.dimensions, m_parameters.strides);

        // Tensors marked for update start fill with host-type max/2 + descriptor index
        // All other tensors start fill at descriptor index

        const T descValue   = static_cast<T>(descIdx);
        const T updateValue = static_cast<T>(std::numeric_limits<T>::max()) / 2 + descValue;
        const T fillValue   = descriptorUpdateMask[descIdx] ? updateValue : descValue;
        expectedTensorData[descIdx].fill(fillValue);

        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensors[descIdx],
                       expectedTensorData[descIdx].data(), expectedTensorData[descIdx].memorySize());
    }

    // Create separate container for tensors, tensor views and expected elements marked for update

    const auto validateCount = std::count(descriptorUpdateMask.begin(), descriptorUpdateMask.end(), true);

    std::vector<de::MovePtr<TensorWithMemory>> updateTensors(validateCount);
    std::vector<Move<vk::VkTensorViewARM>> updateTensorViews(validateCount);
    std::vector<StridedMemoryUtils<T>> updateTensorData(validateCount);

    // Fill update tensors using descriptor index as fillValue

    {
        uint32_t updateIdx = 0;
        for (const auto &descIdx : tensorDescriptorIndices)
        {
            if (!descriptorUpdateMask[descIdx])
            {
                continue;
            }

            const VkTensorDescriptionARM tensorDesc =
                makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions,
                                      m_parameters.strides, VK_TENSOR_USAGE_SHADER_BIT_ARM);
            VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
            updateTensors[updateIdx]               = de::MovePtr<TensorWithMemory>(
                new TensorWithMemory(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any));
            updateTensorViews[updateIdx] =
                Move<VkTensorViewARM>(makeTensorView(vk, device, updateTensors[updateIdx]->get(), m_parameters.format));

            updateTensorData[updateIdx] = StridedMemoryUtils<T>(m_parameters.dimensions, m_parameters.strides);

            updateTensorData[updateIdx].fill(static_cast<T>(descIdx));

            uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *updateTensors[updateIdx],
                           updateTensorData[updateIdx].data(), updateTensorData[updateIdx].memorySize());

            ++updateIdx;
        }
    }

    // Create buffer to write elements for tensors

    const size_t tensorElementCount = static_cast<size_t>(std::accumulate(
        m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), int64_t{1}, std::multiplies<int64_t>()));
    const size_t totalElementCount  = tensorElementCount * m_descriptorCount;
    const size_t bufferSize         = totalElementCount * sizeof(T);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Clear storage buffer memory

    {
        const Allocation &bufferAllocation = buffer.getAllocation();
        StridedMemoryUtils<T> bufferMemory({static_cast<int64_t>(totalElementCount)}, {},
                                           bufferAllocation.getHostPtr());
        bufferMemory.clear();
        flushAlloc(vk, device, bufferAllocation);
    }

    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSize);

    // Create uniform buffer to write descriptor indices for indexing an array of tensors

    const size_t uBufferSize = tensorDescriptorIndices.size() * sizeof(uint32_t);
    const BufferWithMemory uBuffer(vk, device, allocator,
                                   makeBufferCreateInfo(uBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                                   MemoryRequirement::HostVisible);

    // Clear memory and copy descriptor index to uniform buffer

    {
        const Allocation &uBufferAllocation = uBuffer.getAllocation();
        auto *ptr                           = uBuffer.getAllocation().getHostPtr();
        deMemset(ptr, 0u, static_cast<size_t>(uBufferSize));
        deMemcpy(ptr, tensorDescriptorIndices.data(), uBufferSize);
        flushAlloc(vk, device, uBufferAllocation);
    }

    const VkDescriptorBufferInfo uBufferDescriptorInfo = makeDescriptorBufferInfo(*uBuffer, 0ull, uBufferSize);

    // Create descriptor set layout

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addArrayBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, m_descriptorCount, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    // Create descriptor pool

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, m_descriptorCount)
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    // Create descriptor set

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Create tensor descriptors

    {
        std::vector<VkTensorViewARM> tensorViewHandles(m_descriptorCount);

        for (const auto &descIdx : tensorDescriptorIndices)
        {
            tensorViewHandles[descIdx] = *tensorViews[descIdx];
        }

        const VkWriteDescriptorSetTensorARM tensorDescriptorInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM,
                                                                 nullptr, m_descriptorCount, tensorViewHandles.data()};

        // Set the bindings

        DescriptorSetUpdateBuilder()
            .writeArray(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_TENSOR_ARM, m_descriptorCount, &tensorDescriptorInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uBufferDescriptorInfo)
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
            .update(vk, device);
    }

    // Perform the computation

    {
        // Build shader

        const ProgramBinary &binary = m_context.getBinaryCollection().get("comp");
        const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, binary, 0u));

        // Setup pipeline

        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
        const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

        // Create a command pool

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));

        // Prepare the command buffer

        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Update descriptors

        uint32_t updateIdx = 0; // track index for update containers

        for (const auto &descIdx : tensorDescriptorIndices)
        {
            if (!descriptorUpdateMask[descIdx])
            {
                continue;
            }

            const VkWriteDescriptorSetTensorARM updateTensorDescriptorInfo{
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr, 1, &*updateTensorViews[updateIdx]};

            DescriptorSetUpdateBuilder()
                .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::bindingArrayElement(0u, descIdx),
                             VK_DESCRIPTOR_TYPE_TENSOR_ARM, &updateTensorDescriptorInfo)
                .update(vk, device);

            // Update expected tensor elements for tensor by index

            expectedTensorData[descIdx]   = updateTensorData[updateIdx];
            descriptorUpdateMask[descIdx] = false;
            ++updateIdx;

            // Start recording

            beginCommandBuffer(vk, *cmdBuffer);

            // Bind pipeline

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

            // Bind descriptor set

            vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                     &descriptorSet.get(), 0u, nullptr);

            // Dispatch
            const uint32_t dispatchCountX =
                singleDimensionWorkgroupCount(static_cast<uint32_t>(tensorElementCount), 128);
            DE_ASSERT(dispatchCountX <= dispatchWorkgroupCountLimit);
            const uint32_t dispatchCountY = m_descriptorCount;
            DE_ASSERT(dispatchCountY <= dispatchWorkgroupCountLimit);

            vk.cmdDispatch(*cmdBuffer, dispatchCountX, dispatchCountY, 1u);

            // Buffer memory barrier

            const VkBufferMemoryBarrier bufferBarrier =
                makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

            // Pipeline barrier

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                  nullptr, 1, &bufferBarrier, 0, nullptr);

            // End command buffer recording

            endCommandBuffer(vk, *cmdBuffer);

            // Wait for completion

            submitCommandsAndWait(vk, device, queue, *cmdBuffer);

            // Reset command buffer for next dispatch

            vk.resetCommandBuffer(*cmdBuffer, 0u);

            // Validate the result

            {
                const Allocation &bufferAllocation = buffer.getAllocation();
                invalidateAlloc(vk, device, bufferAllocation);

                // For each tensor descriptor, every element written to the buffer must match expected tensor element

                for (uint32_t i = 0; i < m_descriptorCount; ++i)
                {
                    const auto &expected = expectedTensorData[i];
                    T *base = static_cast<T *>(bufferAllocation.getHostPtr()) + expected.elementCount() * i;

                    StridedMemoryUtils<T> bufferMemoryView({static_cast<int64_t>(expected.elementCount())}, {}, base);

                    auto status = compareStridedMemory(expected, bufferMemoryView);
                    if (status.isFail())
                    {
                        return status;
                    }
                }
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
};

template <typename T>
class DescriptorsArrayTestCase : public TestCase
{
private:
    std::string buildTestName(const TensorParameters &parameters, const UpdateRangeFlags &updateRangeFlags)
    {
        std::string range("range_");

        if (updateRangeFlags & RANGE_START)
        {
            range += "start_";
        }
        if (updateRangeFlags & RANGE_MID)
        {
            range += "mid_";
        }
        if (updateRangeFlags & RANGE_END)
        {
            range += "end_";
        }

        // Generate string

        std::ostringstream name;
        name << range;
        name << "dynamic_uniform_indexing_";
        name << paramsToString(parameters);
        return name.str();
    }

public:
    DescriptorsArrayTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters,
                             const uint32_t &descriptorCount, const UpdateRangeFlags &updateRangeFlags)
        : TestCase(testCtx, buildTestName(parameters, updateRangeFlags))
        , m_parameters(parameters)
        , m_descriptorCount(descriptorCount)
        , m_updateRange(updateRangeFlags)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        const TensorParameters *parameters = &m_parameters;

        return new DescriptorsArrayTestInstance<T>(ctx, *parameters, m_descriptorCount, m_updateRange);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        requireTensorShapeSupported(context, m_parameters);

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

        if (!deviceSupportsShaderStorageTensorArrayDynamicIndexing(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support indexing arrays of storage tensors using dynamically "
                                         "uniform integer expressions in shader code");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("comp")
            << glu::ComputeSource(genShaderTensorDescriptorsArrayAccess(m_parameters.rank(), m_parameters.format,
                                                                        static_cast<size_t>(m_descriptorCount)))
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion,
                                      vk::getBaselineSpirvVersion(programCollection.usedVulkanVersion),
                                      vk::ShaderBuildOptions::FLAG_ALLOW_STD430_UBOS);
    }

private:
    const TensorParameters m_parameters;
    const uint32_t m_descriptorCount;
    const UpdateRangeFlags m_updateRange;
};

template <VkFormat Format>
void addDescriptorsArrayTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions &shape_4d = {16, 8, 12, 4};
    using T                          = typename VkFormatToHostType<Format>::HostType;

    // Implicitly packed linear
    const TensorParameters params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape_4d, {}};

    // Dynamic Uniform Indexing

    testCaseGroup.addChild(
        new DescriptorsArrayTestCase<T>(testCaseGroup.getTestContext(), params, 8, RANGE_START | RANGE_END));

    testCaseGroup.addChild(new DescriptorsArrayTestCase<T>(testCaseGroup.getTestContext(), params, 6, RANGE_MID));
}

} // namespace

template <VkFormat... Formats>
void addDescriptorsArrayTestsForFormats(tcu::TestCaseGroup &testCaseGroup)
{
    (addDescriptorsArrayTests<Formats>(testCaseGroup), ...);
}

tcu::TestCaseGroup *createTensorDescriptorsArrayTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "descriptors_array_update", "Tensor descriptors array index update tests"));

    addDescriptorsArrayTestsForFormats<TENSOR_FORMATS_REGULAR_INTS>(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
