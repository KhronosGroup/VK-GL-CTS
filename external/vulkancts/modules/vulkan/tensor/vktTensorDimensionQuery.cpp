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
 * \brief Test querying size of dimensions of tensor inside compute shader
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

namespace vkt
{
namespace tensor
{

namespace
{

class TensorDimensionsQueriesTestInstance : public TestInstance
{
public:
    TensorDimensionsQueriesTestInstance(Context &testCtx, const VkFormat &format, const TensorDimensions &dimensions,
                                        const VkTensorTilingARM tiling)
        : TestInstance(testCtx)
        , m_format(format)
        , m_dimensions(dimensions)
        , m_tiling(tiling)
    {
        if (VK_TENSOR_TILING_LINEAR_ARM == m_tiling)
        {
            m_strides = getTensorStrides(dimensions, getFormatSize(format));
        }
    }

    tcu::TestStatus iterate() override;

private:
    const VkFormat m_format;
    const TensorDimensions &m_dimensions;
    const VkTensorTilingARM m_tiling;
    TensorStrides m_strides = {};
};

tcu::TestStatus TensorDimensionsQueriesTestInstance::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create a tensor and memory for it

    const VkTensorDescriptionARM tensorDesc =
        makeTensorDescription(m_tiling, m_format, m_dimensions, m_strides, VK_TENSOR_USAGE_SHADER_BIT_ARM);
    const VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
    const TensorWithMemory tensor(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any);
    const Move<vk::VkTensorViewARM> tensorView = makeTensorView(vk, device, *tensor, m_format);

    // Create a buffer to copy dimensions into

    const size_t buffer_elements = m_dimensions.size();
    const size_t bufferSize      = buffer_elements * sizeof(uint32_t);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // prepare tensor and buffer

    {
        const Allocation &bufferAllocation = buffer.getAllocation();
        StridedMemoryUtils<uint32_t> bufferMemory({uint32_t(buffer_elements)}, {}, bufferAllocation.getHostPtr());
        bufferMemory.clear();
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

    const VkDescriptorBufferInfo bufferDescriptorInfo =
        makeDescriptorBufferInfo(*buffer, 0ull, buffer_elements * sizeof(uint32_t));
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

        const VkBufferMemoryBarrier bufferBarrier =
            makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);
        vk.cmdDispatch(*cmdBuffer, 1, 1u, 1u);

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                              nullptr, 1, &bufferBarrier, 0, nullptr);

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    {
        const Allocation &bufferAllocation = buffer.getAllocation();

        invalidateAlloc(vk, device, bufferAllocation);

        StridedMemoryUtils<uint32_t> bufferMemory({uint32_t(buffer_elements)}, {}, bufferAllocation.getHostPtr());
        for (size_t element_idx = 0; element_idx < m_dimensions.size(); ++element_idx)
        {
            // Compare buffer value with expected tensor dimension
            if (bufferMemory[element_idx] != m_dimensions[element_idx])
            {

                std::ostringstream msg;
                msg << "Comparison failed at index " << element_idx << ": expected = " << int(m_dimensions[element_idx])
                    << ", buffer = " << int(bufferMemory[element_idx]);
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
}

class TensorDimensionQueriesTestCase : public TestCase
{
public:
    TensorDimensionQueriesTestCase(tcu::TestContext &testCtx, const VkFormat &format, const TensorDimensions &dimension,
                                   const VkTensorTilingARM tiling)
        : TestCase(testCtx, paramsToString(TensorParameters{format, tiling, dimension, {}}))
        , m_format(format)
        , m_dimension(dimension)
        , m_tiling(tiling)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new TensorDimensionsQueriesTestInstance(ctx, m_format, m_dimension, m_tiling);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        if (m_dimension.size() > getTensorPhysicalDeviceProperties(context).maxTensorDimensionCount)
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

        if (!formatSupportTensorFlags(context, m_format, m_tiling, VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Device does not support the tensor flags for this tiling and format");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("comp")
            << glu::ComputeSource(genShaderQueryDimensions(m_dimension.size(), m_format));
    }

private:
    const VkFormat m_format;
    const TensorDimensions m_dimension;
    const VkTensorTilingARM m_tiling;
};

void addDimensionQueriesTestCases(tcu::TestCaseGroup &testCaseGroup)
{

    const std::vector<TensorDimensions> testDimensions = {{1}, {2, 1}, {4, 2, 1}, {8, 4, 2, 1}, {4, 8, 16, 2, 1}};
    for (const auto &format : getAllTestFormats())
    {
        for (const auto &dimension : testDimensions)
        {
            testCaseGroup.addChild(new TensorDimensionQueriesTestCase(testCaseGroup.getTestContext(), format, dimension,
                                                                      VK_TENSOR_TILING_LINEAR_ARM));
            testCaseGroup.addChild(new TensorDimensionQueriesTestCase(testCaseGroup.getTestContext(), format, dimension,
                                                                      VK_TENSOR_TILING_OPTIMAL_ARM));
        }
    }
}

} // namespace

tcu::TestCaseGroup *createDimensionQueryTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "dimension_query", "Tensor dimension query shader tests"));

    addDimensionQueriesTestCases(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
