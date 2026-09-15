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
#include <cmath>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

template <typename T>
class TensorFrameBoundaryTestInstance : public TestInstance
{
public:
    TensorFrameBoundaryTestInstance(Context &testCtx, const TensorParameters &parameters)

        : TestInstance(testCtx)
        , m_parameters(parameters)
    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
};

template <typename T>
class TensorFrameBoundaryTestCase : public TestCase
{
private:
    static std::string buildTestName(const TensorParameters &parameters)
    {
        std::ostringstream name;
        name << paramsToString(parameters);

        return name.str();
    }

public:
    TensorFrameBoundaryTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters)
        : TestCase(testCtx, buildTestName(parameters))
        , m_parameters(parameters)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new TensorFrameBoundaryTestInstance<T>(ctx, m_parameters);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");
        context.requireDeviceFunctionality("VK_EXT_frame_boundary");

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

        if (!m_parameters.packed() && !deviceSupportsNonPackedTensors(context))
        {
            TCU_THROW(NotSupportedError, "Non-packed tensors not supported");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("read_buffer_comp") << glu::ComputeSource(
            genShaderTensorAccess(m_parameters.rank(), m_parameters.format, AccessVariant::READ_FROM_BUFFER));
        programCollection.glslSources.add("write_buffer_comp") << glu::ComputeSource(
            genShaderTensorAccess(m_parameters.rank(), m_parameters.format, AccessVariant::WRITE_TO_BUFFER));
    }

private:
    TensorParameters m_parameters;
};

template <typename T>
tcu::TestStatus TensorFrameBoundaryTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create a tensor and its support memory
    const uint32_t elements =
        static_cast<uint32_t>(std::accumulate(m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(),
                                              static_cast<int64_t>(1), std::multiplies<int64_t>()));

    const VkTensorDescriptionARM tensorDesc =
        makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions, m_parameters.strides,
                              VK_TENSOR_USAGE_SHADER_BIT_ARM);
    VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);

    const TensorWithMemory tensor(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any);

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

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, 2)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u));

    const Unique<VkDescriptorSet> bufferToTensorDescriptorSet(
        makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    const Unique<VkDescriptorSet> tensorToBufferDescriptorSet(
        makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

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
        const Unique<VkCommandBuffer> cmdBufferFirst(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        const Unique<VkCommandBuffer> cmdBufferLast(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        const uint32_t dispatchCount = singleDimensionWorkgroupCount(elements, shaderTensorAccessWorkgroupSize);
        DE_ASSERT(dispatchCount <= dispatchWorkgroupCountLimit);

        // Record command buffers

        {
            beginCommandBuffer(vk, *cmdBufferFirst);

            vk.cmdBindPipeline(*cmdBufferFirst, VK_PIPELINE_BIND_POINT_COMPUTE, *bufferToTensorPipeline);
            vk.cmdBindDescriptorSets(*cmdBufferFirst, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                     &bufferToTensorDescriptorSet.get(), 0u, nullptr);

            vk.cmdDispatch(*cmdBufferFirst, dispatchCount, 1u, 1u);

            endCommandBuffer(vk, *cmdBufferFirst);
        }

        {
            beginCommandBuffer(vk, *cmdBufferLast);

            const VkTensorMemoryBarrierARM tensorBarrier = makeTensorMemoryBarrier(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT, queueFamilyIndex, queueFamilyIndex, *tensor);

            VkDependencyInfo dependencyInfo = initVulkanStructure();
            dependencyInfo.pNext            = &tensorBarrier;
            vk.cmdPipelineBarrier2(*cmdBufferLast, &dependencyInfo);

            vk.cmdBindPipeline(*cmdBufferLast, VK_PIPELINE_BIND_POINT_COMPUTE, *tensorToBufferPipeline);
            vk.cmdBindDescriptorSets(*cmdBufferLast, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                     &tensorToBufferDescriptorSet.get(), 0u, nullptr);
            vk.cmdDispatch(*cmdBufferLast, dispatchCount, 1u, 1u);

            const VkBufferMemoryBarrier bufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *dstBuffer, 0u, bufferSize);

            vk.cmdPipelineBarrier(*cmdBufferLast, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                                  0, nullptr, 1, &bufferBarrier, 0, nullptr);

            endCommandBuffer(vk, *cmdBufferLast);
        }

        const auto fence = createFence(vk, device);

        // Submit command buffers
        {
            const uint64_t frameId = 42;

            std::array<VkSubmitInfo, 2> submitInfos;

            VkFrameBoundaryEXT frameBoundary = initVulkanStructure();
            frameBoundary.frameID            = frameId;

            submitInfos[0]                    = initVulkanStructure();
            submitInfos[0].pNext              = &frameBoundary;
            submitInfos[0].commandBufferCount = 1;
            submitInfos[0].pCommandBuffers    = &*cmdBufferFirst;

            VkFrameBoundaryTensorsARM frameBoundaryTensors = initVulkanStructure();
            frameBoundaryTensors.tensorCount               = 1;
            frameBoundaryTensors.pTensors                  = &*tensor;

            VkFrameBoundaryEXT frameBoundaryEnd = initVulkanStructure();
            frameBoundaryEnd.pNext              = &frameBoundaryTensors;
            frameBoundaryEnd.frameID            = frameId;
            frameBoundaryEnd.flags              = VK_FRAME_BOUNDARY_FRAME_END_BIT_EXT;

            submitInfos[1]                    = initVulkanStructure();
            submitInfos[1].pNext              = &frameBoundaryEnd;
            submitInfos[1].commandBufferCount = 1;
            submitInfos[1].pCommandBuffers    = &*cmdBufferLast;

            VK_CHECK(vk.queueSubmit(queue, static_cast<uint32_t>(submitInfos.size()), submitInfos.data(), *fence));
        }

        waitForFence(vk, device, *fence);
    }

    // Validate the results

    const Allocation &srcBufferAllocation = srcBuffer.getAllocation();
    const Allocation &dstBufferAllocation = dstBuffer.getAllocation();

    invalidateAlloc(vk, device, srcBufferAllocation);
    invalidateAlloc(vk, device, dstBufferAllocation);

    StridedMemoryUtils<T> srcBufferMemory({uint32_t(elements)}, {}, srcBufferAllocation.getHostPtr());
    StridedMemoryUtils<T> dstBufferMemory({uint32_t(elements)}, {}, dstBufferAllocation.getHostPtr());

    return compareStridedMemory(srcBufferMemory, dstBufferMemory);
}

template <VkFormat Format>
void addShaderAccessTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions shapes[] = {{8191}, {83, 97}, {3, 11, 13, 17}};

    const size_t elementSize = getFormatSize(Format);
    using T                  = typename VkFormatToHostType<Format>::HostType;
    for (const TensorDimensions &shape : shapes)
    {
        const size_t rank = shape.size();

        // Implicitly packed linear
        {
            const TensorParameters params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
            testCaseGroup.addChild(new TensorFrameBoundaryTestCase<T>(testCaseGroup.getTestContext(), params));
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

            const TensorParameters params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape, paddedStrides};
            testCaseGroup.addChild(new TensorFrameBoundaryTestCase<T>(testCaseGroup.getTestContext(), params));
        }

        // Optimal
        {
            const TensorParameters params{Format, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
            testCaseGroup.addChild(new TensorFrameBoundaryTestCase<T>(testCaseGroup.getTestContext(), params));
        }
    }
}

template <VkFormat... Formats>
void addShaderAccessTestsForFormats(tcu::TestCaseGroup &testCaseGroup)
{
    (addShaderAccessTests<Formats>(testCaseGroup), ...);
}

} // namespace

tcu::TestCaseGroup *createFrameBoundaryTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "frame_boundary", "Compute dispatch with tensor resource and frame boundary"));

    addShaderAccessTestsForFormats<TENSOR_FORMATS_REGULAR_INTS, TENSOR_FORMATS_REGULAR_FLOATS>(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
