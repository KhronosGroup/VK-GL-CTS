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

static_assert(shaderTensorOoBAccessLineAccesses.size() <= std::numeric_limits<uint32_t>::max());
static constexpr uint32_t accessesPerLine = static_cast<uint32_t>(shaderTensorOoBAccessLineAccesses.size());

std::ostream &operator<<(std::ostream &os, const TensorCoordinates &coordinates)
{
    os << "[";
    for (size_t i = 0; i < coordinates.size(); ++i)
    {
        os << coordinates[i];
        if (i < coordinates.size() - 1)
        {
            os << ", ";
        }
    }
    os << "]";

    return os;
}

template <typename T>
tcu::TestStatus validateOoBReads(const StridedMemoryUtils<T> &tensor, const TensorDimensions &accessedShape,
                                 const StridedMemoryUtils<T> &buffer, const double eps = .01)
{
    static constexpr bool isFloat = std::is_same_v<T, tcu::Float32> || std::is_same_v<T, tcu::Float16> ||
                                    std::is_same_v<T, tcu::BrainFloat16> || std::is_same_v<T, tcu::FloatE5M2> ||
                                    std::is_same_v<T, tcu::FloatE4M3>;

    const TensorDimensions &realShape = tensor.shape();

    const size_t lineSize  = static_cast<size_t>(realShape.back());
    const size_t lineCount = static_cast<size_t>(std::accumulate(accessedShape.cbegin(), accessedShape.cend() - 1,
                                                                 static_cast<int64_t>(1), std::multiplies<int64_t>()));
    DE_ASSERT(lineCount * accessesPerLine == buffer.elementCount());

    for (size_t line = 0; line < lineCount; ++line)
    {
        // Coordinates to the first element of the line
        const TensorCoordinates accessedCoordLineBase =
            StridedMemoryUtils<T>(accessedShape, {}, nullptr).getCoordinates(line * lineSize);

        const auto coordAtLineIndex = [](const TensorCoordinates &coords, const uint64_t index)
        {
            auto result   = coords;
            result.back() = index;
            return result;
        };

        const bool lineIsOoB = [&accessedCoordLineBase, &realShape]
        {
            for (size_t i = 0; i < realShape.size() - 1; ++i)
            {
                if (accessedCoordLineBase[i] >= static_cast<uint64_t>(realShape[i]))
                {
                    return true;
                }
            }

            return false;
        }();

        const size_t bufferIndexBase = line * accessesPerLine;

        for (size_t accessIndex = 0; accessIndex < shaderTensorOoBAccessLineAccesses.size(); ++accessIndex)
        {
            const ShaderTensorOoBAccessLineAccess &access = shaderTensorOoBAccessLineAccesses[accessIndex];

            const bool lineOffsetIsOoB = access.offsetFromEndOfLine >= 0;
            const T expected =
                lineIsOoB || lineOffsetIsOoB ?
                    T(static_cast<double>(access.outOfBoundsValue)) :
                    tensor.at(coordAtLineIndex(accessedCoordLineBase, lineSize + access.offsetFromEndOfLine));

            const T &value = buffer[bufferIndexBase + accessIndex];

            if constexpr (!isFloat)
            {
                if (expected != value)
                {
                    std::ostringstream msg;
                    msg << "Comparison failed at read coordinate "
                        << coordAtLineIndex(accessedCoordLineBase, lineSize + access.offsetFromEndOfLine)
                        << ": expected = " << expected << ", read = " << value;
                    return tcu::TestStatus::fail(msg.str());
                }
            }
            else
            {
                const double valueAsDouble    = value.asDouble();
                const double expectedAsDouble = expected.asDouble();

                if ((expected.isNaN() && !value.isNaN()) || (!expected.isNaN() && value.isNaN()))
                {
                    std::ostringstream msg;
                    msg << "Only one of the values are NaN at coordinate "
                        << coordAtLineIndex(accessedCoordLineBase, lineSize + access.offsetFromEndOfLine)
                        << ": expected = " << expectedAsDouble << ", read = " << valueAsDouble;
                    return tcu::TestStatus::fail(msg.str());
                }

                const double error = std::abs(valueAsDouble - expectedAsDouble);
                if (error > eps)
                {
                    std::ostringstream msg;
                    msg << "Error between read value and expected is too large at coordinate "
                        << coordAtLineIndex(accessedCoordLineBase, lineSize + access.offsetFromEndOfLine) << ": Error "
                        << error << " > " << eps << ", expected = " << expectedAsDouble << ", read = " << valueAsDouble;
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

template <typename T>
tcu::TestStatus validateOoBWrites(const StridedMemoryUtils<T> &updatedTensor,
                                  const StridedMemoryUtils<T> &originalTensor, const TensorDimensions &accessedShape,
                                  const StridedMemoryUtils<T> &buffer, const double eps = .01)
{
    static constexpr bool isFloat = std::is_same_v<T, tcu::Float32> || std::is_same_v<T, tcu::Float16> ||
                                    std::is_same_v<T, tcu::BrainFloat16> || std::is_same_v<T, tcu::FloatE5M2> ||
                                    std::is_same_v<T, tcu::FloatE4M3>;

    // The shader has written only four elements per "line" of the innermost dimension:
    // One inbounds value at the end of the line
    // The out of bounds element directly following the end of the line
    // Out of bounds elements at offsets 13 and 65537 after the end of the line

    const TensorDimensions &realShape = originalTensor.shape();

    const size_t lineSize  = static_cast<size_t>(realShape.back());
    const size_t lineCount = static_cast<size_t>(std::accumulate(accessedShape.cbegin(), accessedShape.cend() - 1,
                                                                 static_cast<int64_t>(1), std::multiplies<int64_t>()));
    DE_ASSERT(lineCount * accessesPerLine == buffer.elementCount());

    // Iterate over every accesses line
    for (size_t line = 0; line < lineCount; ++line)
    {
        // Coordinates to the first element of the line
        const TensorCoordinates accessedCoordLineBase =
            StridedMemoryUtils<T>(accessedShape, {}, nullptr).getCoordinates(line * lineSize);

        const bool lineIsOoB = [&accessedCoordLineBase, &realShape]
        {
            for (size_t i = 0; i < realShape.size() - 1; ++i)
            {
                if (accessedCoordLineBase[i] >= static_cast<uint64_t>(realShape[i]))
                {
                    return true;
                }
            }

            return false;
        }();

        // We don't have anything to check for OoB lines
        if (lineIsOoB)
        {
            continue;
        }

        // First element of the buffer values for the line is the inbounds write
        const size_t bufferIndexBase = line * accessesPerLine;
        const T expectedUpdatedValue = buffer[bufferIndexBase];

        TensorCoordinates coordinate = accessedCoordLineBase;

        // Iterate over every inbounds element of the line
        for (size_t i = 0; i < lineSize; ++i)
        {
            // Update innermost dimension of coordinate
            coordinate.back() = i;

            // Only last element of each line should have been written to
            // The rest of the elements should be the same as the original tensor data
            const bool elementShouldHaveBeenUpdated = i == lineSize - 1;
            const T &expected = elementShouldHaveBeenUpdated ? expectedUpdatedValue : originalTensor.at(coordinate);
            const T &value    = updatedTensor.at(coordinate);

            if constexpr (!isFloat)
            {
                if (expected != value)
                {
                    std::ostringstream msg;
                    msg << "Comparison failed at read coordinate " << coordinate << ": expected = " << expected
                        << ", read = " << value;
                    return tcu::TestStatus::fail(msg.str());
                }
            }
            else
            {
                const double valueAsDouble    = value.asDouble();
                const double expectedAsDouble = expected.asDouble();
                const double error            = std::abs(valueAsDouble - expectedAsDouble);
                if (error > eps)
                {
                    std::ostringstream msg;
                    msg << "Error between read value and expected is too large at coordinate " << coordinate
                        << ": Error " << error << " > " << eps << ", expected = " << expectedAsDouble
                        << ", read = " << valueAsDouble;
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

template <typename T>
class OoBAccessTestInstance : public TestInstance
{
public:
    OoBAccessTestInstance(Context &testCtx, const TensorParameters &parameters, const AccessVariant &variant)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_variant(variant)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
    const AccessVariant m_variant;
};

template <typename T>
class OoBAccessTestCase : public TestCase
{
private:
    static std::string buildTestName(const TensorParameters &parameters, const AccessVariant &variant)
    {
        std::ostringstream name;
        name << "oob_coordinates_" << paramsToString(parameters, variant);
        return name.str();
    }

public:
    OoBAccessTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters, const AccessVariant &variant)
        : TestCase(testCtx, buildTestName(parameters, variant))
        , m_parameters(parameters)
        , m_variant(variant)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new OoBAccessTestInstance<T>(ctx, m_parameters, m_variant);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        requireTensorShapeSupported(context, m_parameters);

        if (m_parameters.format == VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM)
        {
            context.requireDeviceFunctionality("VK_KHR_shader_bfloat16");
        }

        if (m_parameters.format == VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM ||
            m_parameters.format == VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM)
        {
            context.requireDeviceFunctionality("VK_EXT_shader_float8");
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
        const size_t rank = m_parameters.rank();

        programCollection.glslSources.add("comp")
            << glu::ComputeSource(genShaderTensorOoBAccess(rank, m_parameters.format, m_variant));
    }

private:
    const TensorParameters m_parameters;
    const AccessVariant m_variant;
};

template <typename T>
tcu::TestStatus OoBAccessTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Shader accesses a shape which goes 1 coordinate further than the real
    // shape in all dimensions other than the innermost one
    const TensorDimensions accessedShape = [this]
    {
        TensorDimensions shape = m_parameters.dimensions;
        for (size_t i = 0; i < shape.size() - 1; ++i)
        {
            shape[i]++;
        }
        return shape;
    }();

    // Shader accesses one "line" for each shader invocation.
    // The "line" is one row of the innermost dimension.
    const uint32_t lineCount = static_cast<uint32_t>(std::accumulate(
        accessedShape.cbegin(), accessedShape.cend() - 1, static_cast<int64_t>(1), std::multiplies<int64_t>()));

    const uint32_t accessCount = lineCount * accessesPerLine;

    // Create a tensor and memory for it
    const VkTensorDescriptionARM tensorDesc =
        makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions, m_parameters.strides,
                              VK_TENSOR_USAGE_SHADER_BIT_ARM);
    VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);

    const TensorWithMemory tensor(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any);

    const Move<vk::VkTensorViewARM> tensorView = makeTensorView(vk, device, *tensor, m_parameters.format);

    // Create a buffer and host-visible memory for it
    const size_t bufferSize = accessCount * sizeof(T);
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Memory used to transfer data to/from tensor and to compare with the buffer during verification
    StridedMemoryUtils<T> tensorData(m_parameters.dimensions, m_parameters.strides);

    // Prepare tensor and buffer data
    {
        const Allocation &bufferAllocation = buffer.getAllocation();
        StridedMemoryUtils<T> bufferMemory({accessCount}, {}, bufferAllocation.getHostPtr());

        tensorData.fill();
        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, tensorData.data(),
                       tensorData.memorySize());

        if (m_variant == AccessVariant::WRITE_TO_BUFFER)
        {
            bufferMemory.clear();
        }
        else
        {
            bufferMemory.fill();
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
    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSize);
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

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);

        const uint32_t dispatchCount = singleDimensionWorkgroupCount(lineCount, shaderTensorOoBAccessWorkgroupSize);
        DE_ASSERT(dispatchCount <= dispatchWorkgroupCountLimit);
        vk.cmdDispatch(*cmdBuffer, dispatchCount, 1u, 1u);

        if (m_variant == AccessVariant::WRITE_TO_BUFFER)
        {
            const VkBufferMemoryBarrier bufferBarrier =
                makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0u, bufferSize);

            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                  nullptr, 1, &bufferBarrier, 0, nullptr);
        }

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    const Allocation &bufferAllocation = buffer.getAllocation();
    StridedMemoryUtils<T> bufferMemory({accessCount}, {}, bufferAllocation.getHostPtr());

    if (m_variant == AccessVariant::WRITE_TO_BUFFER)
    {
        invalidateAlloc(vk, device, bufferAllocation);
        return validateOoBReads(tensorData, accessedShape, bufferMemory);
    }
    else if (m_variant == AccessVariant::READ_FROM_BUFFER)
    {
        StridedMemoryUtils<T> updatedTensorData(m_parameters.dimensions, m_parameters.strides);
        downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, tensor, updatedTensorData.data(),
                           updatedTensorData.memorySize());
        return validateOoBWrites(updatedTensorData, tensorData, accessedShape, bufferMemory);
    }

    // Incorrect test variant
    DE_ASSERT(false);
    return tcu::TestStatus::fail("Invalid test variant");
}

template <VkFormat Format>
void addOoBAccessTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions shapes[] = {
        {71693}, {263, 269}, {37, 43, 47}, {13, 17, 19, 23}, {7, 11, 13, 17, 19, 23},
    };

    const size_t elementSize = getFormatSize(Format);
    using T                  = typename VkFormatToHostType<Format>::HostType;
    for (const TensorDimensions &shape : shapes)
    {
        const size_t rank = shape.size();

        // Implicitly packed linear
        {
            const TensorParameters params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
            testCaseGroup.addChild(
                new OoBAccessTestCase<T>(testCaseGroup.getTestContext(), params, AccessVariant::READ_FROM_BUFFER));
            testCaseGroup.addChild(
                new OoBAccessTestCase<T>(testCaseGroup.getTestContext(), params, AccessVariant::WRITE_TO_BUFFER));
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
            testCaseGroup.addChild(
                new OoBAccessTestCase<T>(testCaseGroup.getTestContext(), params, AccessVariant::READ_FROM_BUFFER));
            testCaseGroup.addChild(
                new OoBAccessTestCase<T>(testCaseGroup.getTestContext(), params, AccessVariant::WRITE_TO_BUFFER));
        }

        // Explicit packed strides
        {
            const TensorStrides packedStrides = getTensorStrides(shape, elementSize);
            const TensorParameters params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape, packedStrides};
            testCaseGroup.addChild(
                new OoBAccessTestCase<T>(testCaseGroup.getTestContext(), params, AccessVariant::READ_FROM_BUFFER));
            testCaseGroup.addChild(
                new OoBAccessTestCase<T>(testCaseGroup.getTestContext(), params, AccessVariant::WRITE_TO_BUFFER));
        }
    }
}

template <VkFormat... Formats>
void addOoBAccessTestsForFormats(tcu::TestCaseGroup &testCaseGroup)
{
    (addOoBAccessTests<Formats>(testCaseGroup), ...);
}

} // namespace

tcu::TestCaseGroup *createRobustnessTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "robustness", "Tensor shader robustness tests"));

    // clang-format make these harder to read
    // clang-format off
    addOoBAccessTestsForFormats<
        TENSOR_FORMATS_REGULAR_INTS,
        TENSOR_FORMATS_REGULAR_FLOATS,
        VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM,
        VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM,
        VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM>(*group);
    // clang-format on

    return group.release();
}

} // namespace tensor
} // namespace vkt
