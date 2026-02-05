/*-------------------------------------------------------------------------
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
 * \brief Tensor Basic Tests
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTensorTestsUtil.hpp"
#include "shaders/vktTensorShaders.hpp"
#include "shaders/vktTensorShaderUtil.hpp"
#include "vkTensorMemoryUtil.hpp"
#include "vkTensorWithMemory.hpp"

#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;
using namespace std::placeholders;

class TensorBooleanOpTestInstance : public TestInstance
{
public:
    TensorBooleanOpTestInstance(Context &testCtx, const TensorParameters &parameters, const BooleanOperator op,
                                const bool testValue)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_operator(op)
        , m_testValue(testValue)
    {
    }

    tcu::TestStatus iterate() override;

private:
    TensorParameters m_parameters;
    const BooleanOperator m_operator;
    const bool m_testValue;
};

tcu::TestStatus TensorBooleanOpTestInstance::iterate()
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
    const TensorWithMemory tensorOut(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any);

    const Move<vk::VkTensorViewARM> tensorView     = makeTensorView(vk, device, *tensor, m_parameters.format);
    const Move<vk::VkTensorViewARM> tensorView_out = makeTensorView(vk, device, *tensorOut, m_parameters.format);

    const bool optimalTilingTest = m_parameters.tiling == VK_TENSOR_TILING_OPTIMAL_ARM;

    de::MovePtr<TensorWithMemory> linearTensor;

    if (optimalTilingTest)
    {
        const VkTensorDescriptionARM linearTensorDesc =
            makeTensorDescription(VK_TENSOR_TILING_LINEAR_ARM, m_parameters.format, m_parameters.dimensions, {},
                                  VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
        const VkTensorCreateInfoARM linearTensorCreateInfo = makeTensorCreateInfo(&linearTensorDesc);
        linearTensor                                       = de::MovePtr(
            new TensorWithMemory(vk, device, allocator, linearTensorCreateInfo, vk::MemoryRequirement::Any));
    }

    StridedMemoryUtils<uint8_t> initialTensorData(m_parameters.dimensions, m_parameters.strides);

    initialTensorData.fill();

    if (!optimalTilingTest)
    {
        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, initialTensorData.data(),
                       initialTensorData.memorySize());
        clearTensor(vk, device, allocator, queue, queueFamilyIndex, tensorOut);
    }
    else
    {
        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *linearTensor, initialTensorData.data(),
                       initialTensorData.memorySize());
    }

    //Create Descriptor Set and Layout

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Bind tensors

    DescriptorSetUpdateBuilder updateBuilder;
    const VkWriteDescriptorSetTensorARM tensorDescriptorInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr,
                                                             1, &*tensorView};
    const VkWriteDescriptorSetTensorARM tensorDescriptorInfo_out{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM,
                                                                 nullptr, 1, &*tensorView_out};

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_TENSOR_ARM,
                     &tensorDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_TENSOR_ARM,
                     &tensorDescriptorInfo_out)
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

        beginCommandBuffer(vk, *cmdBuffer);
        if (optimalTilingTest)
        {
            VkTensorCopyARM tensorRegions{};
            tensorRegions.sType          = VK_STRUCTURE_TYPE_TENSOR_COPY_ARM;
            tensorRegions.dimensionCount = static_cast<uint32_t>(m_parameters.dimensions.size());

            VkCopyTensorInfoARM copyInfo{};
            copyInfo.sType       = VK_STRUCTURE_TYPE_COPY_TENSOR_INFO_ARM;
            copyInfo.srcTensor   = **linearTensor;
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
        vk.cmdDispatch(*cmdBuffer, elements, 1u, 1u);

        if (optimalTilingTest)
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
            copyInfo.srcTensor   = *tensorOut;
            copyInfo.dstTensor   = **linearTensor;
            copyInfo.pRegions    = &tensorRegions;
            copyInfo.regionCount = 1;

            vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);
        }
        {
            const VkTensorMemoryBarrierARM tensorBarrier = makeTensorMemoryBarrier(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                VK_ACCESS_HOST_READ_BIT, 0, 0, *(optimalTilingTest ? *linearTensor : tensorOut));

            VkDependencyInfo dependencyInfo{};
            dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependencyInfo.pNext = &tensorBarrier;
            vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
        }

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion
        submitCommandsAndWait(vk, device, queue, *cmdBuffer);

        {
            StridedMemoryUtils<uint8_t> result(m_parameters.dimensions, m_parameters.strides);
            if (!optimalTilingTest)
            {
                downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, tensorOut, result.data(),
                                   result.memorySize());
            }
            else
            {
                downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, *linearTensor, result.data(),
                                   result.memorySize());
            }

            for (size_t element_idx = 0; element_idx < elements; ++element_idx)
            {
                bool expected = false;
                switch (m_operator)
                {
                case BooleanOperator::AND:
                {
                    expected = initialTensorData[element_idx] && m_testValue;
                    break;
                }
                case BooleanOperator::XOR:
                {
                    expected = static_cast<bool>(initialTensorData[element_idx]) ^ m_testValue;
                    break;
                }
                case BooleanOperator::OR:
                {
                    expected = initialTensorData[element_idx] || m_testValue;
                    break;
                }
                case BooleanOperator::NOT:
                {
                    expected = !initialTensorData[element_idx];
                    break;
                }
                default:
                {
                    DE_ASSERT(false);
                }
                }
                if (static_cast<bool>(result[element_idx]) != expected)
                {

                    std::ostringstream msg;
                    msg << "Comparison failed at index " << element_idx << ": expected = " << int(expected)
                        << ", buffer = " << int(result[element_idx]);
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
    }
    return tcu::TestStatus::pass("Tensor test succeeded");
}

class TensorBooleanOpTestCase : public TestCase
{
public:
    TensorBooleanOpTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters, const BooleanOperator op,
                            const bool testValue)
        : TestCase(testCtx, paramsToString(parameters, op) + "_apply_" + de::toString(testValue))
        , m_parameters(parameters)
        , m_operator(op)
        , m_testValue(testValue)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new TensorBooleanOpTestInstance(ctx, m_parameters, m_operator, m_testValue);
    }

    void checkSupport(Context &ctx) const override
    {
        ctx.requireDeviceFunctionality("VK_ARM_tensors");

        if (m_parameters.rank() > getTensorPhysicalDeviceProperties(ctx).maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!deviceSupportsShaderTensorAccess(ctx))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(ctx, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }

        if (!formatSupportTensorFlags(ctx, m_parameters.format, m_parameters.tiling,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (!m_parameters.packed() && !deviceSupportsNonPackedTensors(ctx))
        {
            TCU_THROW(NotSupportedError, "Non-packed tensors not supported");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("comp")
            << glu::ComputeSource(genShaderBooleanOp(m_parameters.rank(), m_operator, m_testValue));
    }

private:
    TensorParameters m_parameters;
    const BooleanOperator m_operator;
    const bool m_testValue;
};

} // namespace

void addTensorBoolTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions shapes[] = {
        {71693},
        {263, 269},
        {37, 43, 47},
        {13, 17, 19, 23},
    };

    static constexpr VkFormat format = VK_FORMAT_R8_BOOL_ARM;
    const size_t elementSize         = getFormatSize(format);

    for (const TensorDimensions &shape : shapes)
    {
        const size_t rank                 = shape.size();
        const TensorStrides packedStrides = getTensorStrides(shape, elementSize);

        TensorStrides paddedStrides(rank);
        paddedStrides[rank - 1] = elementSize;
        for (size_t i = 2; i <= rank; ++i)
        {
            paddedStrides[rank - i] = paddedStrides[rank - i + 1] * shape[rank - i + 1] + 13 * elementSize;
        }

        for (const auto op : {BooleanOperator::AND, BooleanOperator::OR, BooleanOperator::NOT, BooleanOperator::XOR})
        {
            for (const bool testValue : {true, false})
            {
                // Implicit packed
                {
                    const TensorParameters parameters = {format, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                    testCaseGroup.addChild(
                        new TensorBooleanOpTestCase(testCaseGroup.getTestContext(), parameters, op, testValue));
                }

                // Explicit packed strides
                if (rank > 1)
                {
                    const TensorParameters parameters = {format, VK_TENSOR_TILING_LINEAR_ARM, shape, packedStrides};
                    testCaseGroup.addChild(
                        new TensorBooleanOpTestCase(testCaseGroup.getTestContext(), parameters, op, testValue));
                }

                // Explicit non-packed strides
                if (rank > 1)
                {
                    const TensorParameters parameters = {format, VK_TENSOR_TILING_LINEAR_ARM, shape, paddedStrides};
                    testCaseGroup.addChild(
                        new TensorBooleanOpTestCase(testCaseGroup.getTestContext(), parameters, op, testValue));
                }

                // Optimal
                {
                    const TensorParameters parameters = {format, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
                    testCaseGroup.addChild(
                        new TensorBooleanOpTestCase(testCaseGroup.getTestContext(), parameters, op, testValue));
                }
            }
        }
    }
}

tcu::TestCaseGroup *createTensorBoolTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> createTensorBoolTests(
        new tcu::TestCaseGroup(testCtx, "boolean", "tensor creation and memory Bool"));
    addTensorBoolTests(*createTensorBoolTests);

    return createTensorBoolTests.release();
}

} // namespace tensor
} // namespace vkt
