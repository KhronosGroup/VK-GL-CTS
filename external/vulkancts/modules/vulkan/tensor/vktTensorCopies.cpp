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
 * \brief Tensor Copy Tests.
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vktTensorTestsUtil.hpp"

#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkTensorUtil.hpp"
#include "vkTensorMemoryUtil.hpp"

#include "deFloat16.h"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

#include <numeric>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

template <typename T>
class LinearTensorCopyTestInstance : public TestInstance
{
public:
    LinearTensorCopyTestInstance(Context &testCtx, const TensorParameters &srcParameters,
                                 const TensorParameters &dstParameters)
        : TestInstance(testCtx)
        , m_srcParameters(srcParameters)
        , m_dstParameters(dstParameters)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_srcParameters;
    const TensorParameters m_dstParameters;
};

template <typename T>
class LinearTensorCopyTestCase : public TestCase
{
public:
    LinearTensorCopyTestCase(tcu::TestContext &testCtx, const TensorParameters &srcParameters,
                             const TensorParameters &dstParameters)
        : TestCase(testCtx, paramsToString(srcParameters) + "_to_" + paramsToString(dstParameters))
        , m_srcParameters(srcParameters)
        , m_dstParameters(dstParameters)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new LinearTensorCopyTestInstance<T>(ctx, m_srcParameters, m_dstParameters);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        const uint32_t maxTensorDimensionCount = getTensorPhysicalDeviceProperties(context).maxTensorDimensionCount;

        if (m_srcParameters.rank() > maxTensorDimensionCount || m_dstParameters.rank() > maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if (!formatSupportTensorFlags(context, m_srcParameters.format, m_srcParameters.tiling,
                                      VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT))
        {
            TCU_THROW(NotSupportedError, "Source format not supported");
        }

        if (!formatSupportTensorFlags(context, m_dstParameters.format, m_dstParameters.tiling,
                                      VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT))
        {
            TCU_THROW(NotSupportedError, "Destination format not supported");
        }

        // Skip tests using explicit strides if device does not support non-packed tensors
        if ((!m_srcParameters.packed() || !m_dstParameters.packed()) && !deviceSupportsNonPackedTensors(context))
        {
            TCU_THROW(NotSupportedError, "Non-packed tensors not supported");
        }
    }

private:
    const TensorParameters m_srcParameters;
    const TensorParameters m_dstParameters;
};

template <typename T>
tcu::TestStatus LinearTensorCopyTestInstance<T>::iterate()
{
    DE_ASSERT(m_srcParameters.dimensions == m_dstParameters.dimensions);

    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create two tensors and memory for them
    const VkTensorDescriptionARM srcTensorDesc =
        makeTensorDescription(m_srcParameters.tiling, m_srcParameters.format, m_srcParameters.dimensions,
                              m_srcParameters.strides, VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM);
    const VkTensorCreateInfoARM srcTensorInfo = makeTensorCreateInfo(&srcTensorDesc);
    const TensorWithMemory srcTensor(vk, device, allocator, srcTensorInfo, vk::MemoryRequirement::Any);

    const VkTensorDescriptionARM dstTensorDesc =
        makeTensorDescription(m_dstParameters.tiling, m_dstParameters.format, m_dstParameters.dimensions,
                              m_dstParameters.strides, VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM dstTensorInfo = makeTensorCreateInfo(&dstTensorDesc);
    const TensorWithMemory dstTensor(vk, device, allocator, dstTensorInfo, vk::MemoryRequirement::Any);

    // prepare tensors' memory

    StridedMemoryUtils<T> inputData(m_srcParameters.dimensions, m_srcParameters.strides);
    inputData.fill();

    uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, srcTensor, inputData.data(), inputData.memorySize());

    clearTensor(vk, device, allocator, queue, queueFamilyIndex, dstTensor);

    // Perform the computation

    {
        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        // Tensor barrier

        const VkTensorMemoryBarrierARM copyToHostAccess =
            makeTensorMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, 0, 0, *dstTensor);

        VkDependencyInfo tensorDependencyInfo{};
        tensorDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        tensorDependencyInfo.pNext = &copyToHostAccess;

        // Copy tensors

        VkTensorCopyARM tensorRegions{};
        tensorRegions.sType          = VK_STRUCTURE_TYPE_TENSOR_COPY_ARM;
        tensorRegions.dimensionCount = static_cast<uint32_t>(m_srcParameters.dimensions.size());

        VkCopyTensorInfoARM copyInfo{};
        copyInfo.sType       = VK_STRUCTURE_TYPE_COPY_TENSOR_INFO_ARM;
        copyInfo.srcTensor   = *srcTensor;
        copyInfo.dstTensor   = *dstTensor;
        copyInfo.pRegions    = &tensorRegions;
        copyInfo.regionCount = 1;

        vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);
        vk.cmdPipelineBarrier2(*cmdBuffer, &tensorDependencyInfo);

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    {
        StridedMemoryUtils<T> result(m_dstParameters.dimensions, m_dstParameters.strides);
        downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, dstTensor, result.data(),
                           result.memorySize());

        for (size_t element_idx = 0; element_idx < inputData.elementCount(); ++element_idx)
        {
            if (inputData[element_idx] != result[element_idx])
            {
                std::ostringstream msg;
                msg << "Comparison failed at index " << element_idx << ": source = " << int(inputData[element_idx])
                    << ", destination = " << int(result[element_idx]);
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
}

template <typename T>
class OptimalTensorCopyTestInstance : public TestInstance
{
public:
    OptimalTensorCopyTestInstance(Context &testCtx, const TensorParameters &srcParameters,
                                  const TensorParameters &dstParameters)
        : TestInstance(testCtx)
        , m_srcParameters(srcParameters)
        , m_dstParameters(dstParameters)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_srcParameters;
    const TensorParameters m_dstParameters;
};

static bool checkSupportLinearSrcStorageTensor(Context &context, const VkFormat &format)
{
    return formatSupportTensorFlags(context, format, VK_TENSOR_TILING_LINEAR_ARM, VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT);
}

static bool checkSupportLinearDstStorageTensor(Context &context, const VkFormat &format)
{
    return formatSupportTensorFlags(context, format, VK_TENSOR_TILING_LINEAR_ARM, VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT);
}

static bool checkSupportOptimalStorageTensor(Context &context, const VkFormat &format)
{
    return formatSupportTensorFlags(context, format, VK_TENSOR_TILING_OPTIMAL_ARM,
                                    VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT);
}

template <typename T>
class OptimalTensorCopyTestCase : public TestCase
{
public:
    OptimalTensorCopyTestCase(tcu::TestContext &testCtx, const TensorParameters &srcParameters,
                              const TensorParameters &dstParameters)
        : TestCase(testCtx, paramsToString(srcParameters) + "_to_" + paramsToString(dstParameters))
        , m_srcParameters(srcParameters)
        , m_dstParameters(dstParameters)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new OptimalTensorCopyTestInstance<T>(ctx, m_srcParameters, m_dstParameters);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        const uint32_t maxTensorDimensionCount = getTensorPhysicalDeviceProperties(context).maxTensorDimensionCount;

        if (m_srcParameters.rank() > maxTensorDimensionCount || m_dstParameters.rank() > maxTensorDimensionCount)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension count is higher than what the implementation supports");
        }

        if ((!checkSupportLinearSrcStorageTensor(context, m_srcParameters.format)) ||
            (!checkSupportOptimalStorageTensor(context, m_srcParameters.format)) ||
            (!checkSupportOptimalStorageTensor(context, m_dstParameters.format)) ||
            (!checkSupportLinearDstStorageTensor(context, m_dstParameters.format)))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }
    }

private:
    const TensorParameters m_srcParameters;
    const TensorParameters m_dstParameters;
};

template <typename T>
tcu::TestStatus OptimalTensorCopyTestInstance<T>::iterate()
{

    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create four tensors (Linear > Optimal > Optimal > Linear). The initial and final tensors must have linear strides in order to be able to write and read them manually.

    const VkTensorDescriptionARM srcTensorDescLinear =
        makeTensorDescription(VK_TENSOR_TILING_LINEAR_ARM, m_srcParameters.format, m_srcParameters.dimensions,
                              m_srcParameters.strides, VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM);
    const VkTensorCreateInfoARM srcTensorInfoLinear = makeTensorCreateInfo(&srcTensorDescLinear);
    const TensorWithMemory srcTensorLinear(vk, device, allocator, srcTensorInfoLinear, vk::MemoryRequirement::Any);

    const VkTensorDescriptionARM srcTensorDescOptimal = makeTensorDescription(
        VK_TENSOR_TILING_OPTIMAL_ARM, m_srcParameters.format, m_srcParameters.dimensions, m_srcParameters.strides,
        VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM srcTensorInfoOptimal = makeTensorCreateInfo(&srcTensorDescOptimal);
    const TensorWithMemory srcTensorOptimal(vk, device, allocator, srcTensorInfoOptimal, vk::MemoryRequirement::Any);

    const VkTensorDescriptionARM dstTensorDescOptimal = makeTensorDescription(
        VK_TENSOR_TILING_OPTIMAL_ARM, m_dstParameters.format, m_dstParameters.dimensions, m_dstParameters.strides,
        VK_TENSOR_USAGE_TRANSFER_SRC_BIT_ARM | VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM dstTensorInfoOptimal = makeTensorCreateInfo(&dstTensorDescOptimal);
    const TensorWithMemory dstTensorOptimal(vk, device, allocator, dstTensorInfoOptimal, vk::MemoryRequirement::Any);

    const VkTensorDescriptionARM dstTensorDescLinear =
        makeTensorDescription(VK_TENSOR_TILING_LINEAR_ARM, m_dstParameters.format, m_dstParameters.dimensions,
                              m_dstParameters.strides, VK_TENSOR_USAGE_TRANSFER_DST_BIT_ARM);
    const VkTensorCreateInfoARM dstTensorInfoLinear = makeTensorCreateInfo(&dstTensorDescLinear);
    const TensorWithMemory dstTensorLinear(vk, device, allocator, dstTensorInfoLinear, vk::MemoryRequirement::Any);

    // prepare tensors' memory

    StridedMemoryUtils<T> inputData(m_srcParameters.dimensions, m_srcParameters.strides);
    inputData.fill();

    uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, srcTensorLinear, inputData.data(),
                   inputData.memorySize());

    clearTensor(vk, device, allocator, queue, queueFamilyIndex, dstTensorLinear);

    // Perform the computation

    {
        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        // memory barrier

        const VkMemoryBarrier interCopiesBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
                                                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};

        // tensor barrier

        const VkTensorMemoryBarrierARM copyToHostAccess =
            makeTensorMemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, 0, 0, *dstTensorLinear);
        VkDependencyInfo tensorDependencyInfo{};
        tensorDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        tensorDependencyInfo.pNext = &copyToHostAccess;

        // Copy Linear > Optimal

        VkTensorCopyARM tensorRegions{};
        tensorRegions.sType          = VK_STRUCTURE_TYPE_TENSOR_COPY_ARM;
        tensorRegions.dimensionCount = static_cast<uint32_t>(m_srcParameters.dimensions.size());

        VkCopyTensorInfoARM copyInfo{};
        copyInfo.sType       = VK_STRUCTURE_TYPE_COPY_TENSOR_INFO_ARM;
        copyInfo.srcTensor   = *srcTensorLinear;
        copyInfo.dstTensor   = *srcTensorOptimal;
        copyInfo.pRegions    = &tensorRegions;
        copyInfo.regionCount = 1;

        vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                              &interCopiesBarrier, 0, nullptr, 0, nullptr);

        // Copy Optimal > Optimal

        copyInfo.srcTensor   = *srcTensorOptimal;
        copyInfo.dstTensor   = *dstTensorOptimal;
        copyInfo.pRegions    = &tensorRegions;
        copyInfo.regionCount = 1;

        vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
                              &interCopiesBarrier, 0, nullptr, 0, nullptr);

        // Copy Optimal > Linear

        copyInfo.srcTensor   = *dstTensorOptimal;
        copyInfo.dstTensor   = *dstTensorLinear;
        copyInfo.pRegions    = &tensorRegions;
        copyInfo.regionCount = 1;

        vk.cmdCopyTensorARM(*cmdBuffer, &copyInfo);
        vk.cmdPipelineBarrier2(*cmdBuffer, &tensorDependencyInfo);

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    {
        StridedMemoryUtils<T> result(m_dstParameters.dimensions, m_dstParameters.strides);
        downloadFromTensor(vk, device, allocator, queue, queueFamilyIndex, dstTensorLinear, result.data(),
                           result.memorySize());

        for (size_t element_idx = 0; element_idx < inputData.elementCount(); ++element_idx)
        {
            if (inputData[element_idx] != result[element_idx])
            {
                std::ostringstream msg;
                msg << "Comparison failed at index " << element_idx << ": source = " << int(inputData[element_idx])
                    << ", destination = " << int(result[element_idx]);
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
}

} // namespace

template <typename T>
void addTensorCopyTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions shapes[] = {
        {71693},
        {263, 269},
        {37, 43, 47},
        {13, 17, 19, 23},
    };

    for (const TensorDimensions &shape : shapes)
    {
        for (const VkFormat srcFormat : getTestFormats<T>())
        {
            for (const VkFormat dstFormat : getTestFormats<T>())
            {
                // Packed to packed
                {
                    const TensorParameters srcParams{srcFormat, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                    const TensorParameters dstParams{dstFormat, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                    testCaseGroup.addChild(
                        new LinearTensorCopyTestCase<T>(testCaseGroup.getTestContext(), srcParams, dstParams));
                }

                const size_t rank        = shape.size();
                const size_t elementSize = getFormatSize(srcFormat);

                // Non-packed strides to use for test involving those
                TensorStrides paddedStrides(rank);
                paddedStrides[rank - 1] = elementSize;
                for (size_t i = 2; i <= rank; ++i)
                {
                    paddedStrides[rank - i] = paddedStrides[rank - i + 1] * shape[rank - i + 1] + 13 * elementSize;
                }

                // Packed to non-packed
                if (rank > 1)
                {
                    const TensorParameters srcParams{srcFormat, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                    const TensorParameters dstParams{dstFormat, VK_TENSOR_TILING_LINEAR_ARM, shape, paddedStrides};
                    testCaseGroup.addChild(
                        new LinearTensorCopyTestCase<T>(testCaseGroup.getTestContext(), srcParams, dstParams));
                }

                // Non-packed to packed
                if (rank > 1)
                {
                    const TensorParameters srcParams{srcFormat, VK_TENSOR_TILING_LINEAR_ARM, shape, paddedStrides};
                    const TensorParameters dstParams{dstFormat, VK_TENSOR_TILING_LINEAR_ARM, shape, {}};
                    testCaseGroup.addChild(
                        new LinearTensorCopyTestCase<T>(testCaseGroup.getTestContext(), srcParams, dstParams));
                }

                // Optimal, includes copies between linear packed and optimal tensors of same format
                {
                    const TensorParameters srcParams{srcFormat, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
                    const TensorParameters dstParams{dstFormat, VK_TENSOR_TILING_OPTIMAL_ARM, shape, {}};
                    testCaseGroup.addChild(
                        new OptimalTensorCopyTestCase<T>(testCaseGroup.getTestContext(), srcParams, dstParams));
                }
            }
        }
    }
}

tcu::TestCaseGroup *createTensorCopyTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> tensorCopyTests(new tcu::TestCaseGroup(testCtx, "copies"));

    addTensorCopyTests<uint64_t>(*tensorCopyTests);
    addTensorCopyTests<uint32_t>(*tensorCopyTests);
    addTensorCopyTests<uint16_t>(*tensorCopyTests);
    addTensorCopyTests<uint8_t>(*tensorCopyTests);

    return tensorCopyTests.release();
}

} // namespace tensor
} // namespace vkt
