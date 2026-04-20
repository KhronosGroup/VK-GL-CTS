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
 * \brief Tensor update after bind tests
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

#include <cstddef>
#include <cstdint>
#include <sstream>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

template <typename T>
class UpdateAfterBindTestInstance : public TestInstance
{
public:
    UpdateAfterBindTestInstance(Context &testCtx, const TensorParameters &parameters, const bool &emptyDescriptorSet)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_empty_descriptor_set(emptyDescriptorSet)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
    const bool m_empty_descriptor_set;
};

template <typename T>
tcu::TestStatus UpdateAfterBindTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create containers to store tensors and tensor views

    const uint32_t tensorCount = m_empty_descriptor_set ? 1 : 2;

    std::vector<de::MovePtr<TensorWithMemory>> tensors(tensorCount);
    std::vector<Move<vk::VkTensorViewARM>> tensorViews(tensorCount);

    for (uint32_t i = 0; i < tensorCount; ++i)
    {
        const VkTensorDescriptionARM tensorDesc =
            makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions,
                                  m_parameters.strides, VK_TENSOR_USAGE_SHADER_BIT_ARM);
        VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
        tensors[i]                             = de::MovePtr<TensorWithMemory>(
            new TensorWithMemory(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any));
        tensorViews[i] = Move<VkTensorViewARM>(makeTensorView(vk, device, tensors[i]->get(), m_parameters.format));

        // Single 4D shape is tested

        StridedMemoryUtils<T> tensorData(m_parameters.dimensions, m_parameters.strides);
        tensorData.fill(static_cast<T>(i));

        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensors[i], tensorData.data(),
                       tensorData.memorySize());
    }

    // Create buffer

    const size_t elementCount = static_cast<size_t>(std::accumulate(
        m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), int64_t{1}, std::multiplies<int64_t>()));
    const size_t bufferSize   = elementCount * sizeof(T);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Clear buffer memory

    {
        const Allocation &bufferAllocation = buffer.getAllocation();
        StridedMemoryUtils<T> bufferMemory({static_cast<int64_t>(elementCount)}, {}, bufferAllocation.getHostPtr());
        bufferMemory.clear();
        flushAlloc(vk, device, bufferAllocation);
    }

    // Create a descriptor set layout

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_COMPUTE_BIT,
                              VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT));

    // Create a descriptor pool

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device,
                   VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                   1u));

    // Create a descriptor set

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Set the bindings

    const VkDescriptorBufferInfo bufferDescriptorInfo =
        makeDescriptorBufferInfo(*buffer, 0ull, elementCount * sizeof(T));

    if (!m_empty_descriptor_set)
    {
        const VkWriteDescriptorSetTensorARM tensorDescriptorInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM,
                                                                 nullptr, 1, &*tensorViews[1]};
        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_TENSOR_ARM, &tensorDescriptorInfo)
            .update(vk, device);
    }

    DescriptorSetUpdateBuilder()
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

        // Start recording

        beginCommandBuffer(vk, *cmdBuffer);

        // Buffer memory barrier

        const VkBufferMemoryBarrier bufferBarrier =
            makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

        // Bind pipeline

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

        // Bind descriptor set

        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);

        const uint32_t dispatchCount =
            singleDimensionWorkgroupCount(static_cast<uint32_t>(elementCount), shaderTensorAccessWorkgroupSize);
        DE_ASSERT(dispatchCount <= dispatchWorkgroupCountLimit);
        vk.cmdDispatch(*cmdBuffer, dispatchCount, 1u, 1u);

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                              nullptr, 1, &bufferBarrier, 0, nullptr);

        endCommandBuffer(vk, *cmdBuffer);

        // Update descriptor set with new tensor descriptor

        {
            const VkWriteDescriptorSetTensorARM newTensorDescriptorInfo{
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr, 1, &*tensorViews[0]};

            DescriptorSetUpdateBuilder()
                .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                             VK_DESCRIPTOR_TYPE_TENSOR_ARM, &newTensorDescriptorInfo)
                .update(vk, device);
        }

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    {
        const Allocation &bufferAllocation = buffer.getAllocation();
        invalidateAlloc(vk, device, bufferAllocation);
        StridedMemoryUtils<T> bufferMemory({static_cast<int64_t>(elementCount)}, {}, bufferAllocation.getHostPtr());

        // Single 4D shape is tested
        StridedMemoryUtils<T> tensorData(m_parameters.dimensions, m_parameters.strides);

        downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *tensors[0], tensorData.data(),
                           tensorData.memorySize());

        return compareStridedMemory(tensorData, bufferMemory);
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
};

template <typename T>
class UpdateAfterBindTestCase : public TestCase
{
private:
    std::string buildTestName(const TensorParameters &parameters, const bool &emptyDescriptorSet)
    {
        std::ostringstream name;
        name << "single_update_";
        name << (emptyDescriptorSet ? "empty" : "single");
        name << "_binding_";
        name << paramsToString(parameters);
        return name.str();
    }

public:
    UpdateAfterBindTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters,
                            const bool &emptyDescriptorSet)
        : TestCase(testCtx, buildTestName(parameters, emptyDescriptorSet))
        , m_parameters(parameters)
        , m_empty_descriptor_set(emptyDescriptorSet)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        const TensorParameters *parameters = &m_parameters;

        return new UpdateAfterBindTestInstance<T>(ctx, *parameters, m_empty_descriptor_set);
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

        if (!deviceSupportsStorageTensorUpdateAfterBind(context))
        {
            TCU_THROW(NotSupportedError,
                      "Device does not support updating storage tensor descriptors after a set is bound");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("comp") << glu::ComputeSource(
            genShaderTensorAccess(m_parameters.rank(), m_parameters.format, AccessVariant::WRITE_TO_BUFFER));
    }

private:
    const TensorParameters m_parameters;
    const bool m_empty_descriptor_set;
};

template <VkFormat Format>
void addUpdateAfterBindTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions &shape_4d = {16, 8, 12, 4};

    using T = typename VkFormatToHostType<Format>::HostType;

    // Implicitly packed linear
    const TensorParameters params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape_4d, {}};

    // creates an empty descriptor set
    testCaseGroup.addChild(new UpdateAfterBindTestCase<T>(testCaseGroup.getTestContext(), params, true));

    // creates a descriptor set with one tensor binding
    testCaseGroup.addChild(new UpdateAfterBindTestCase<T>(testCaseGroup.getTestContext(), params, false));
}

} // namespace

template <VkFormat... Formats>
void addUpdateAfterBindTestsForFormats(tcu::TestCaseGroup &testCaseGroup)
{
    (addUpdateAfterBindTests<Formats>(testCaseGroup), ...);
}

tcu::TestCaseGroup *createUpdateAfterBindTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "update_after_bind", "Tensor descriptor update after bind tests"));

    addUpdateAfterBindTestsForFormats<TENSOR_FORMATS_REGULAR_INTS>(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
