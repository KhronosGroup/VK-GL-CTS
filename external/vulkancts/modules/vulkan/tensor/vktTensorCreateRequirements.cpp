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
 * \brief Test creating tensors and sanity check tensor memory requirement
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTensorTestsUtil.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"

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

const std::vector<TensorParameters> getMaxTensorParameters(Context &context, const VkFormat &format,
                                                           const VkTensorTilingARM tiling)
{
    const auto props = getTensorPhysicalDeviceProperties(context);

    const size_t elementSize   = getFormatSize(format);
    const uint64_t maxElements = std::min(props.maxTensorElements, props.maxTensorSize / elementSize);

    std::vector<TensorParameters> maxParametersList;

    // Packed tensor max elements
    for (uint64_t dimensionCount = 1; dimensionCount <= props.maxTensorDimensionCount; ++dimensionCount)
    {
        maxParametersList.push_back({format, tiling, {}, {}});
        TensorParameters &parameters = maxParametersList.back();

        uint64_t remaining_elements = maxElements;
        for (uint64_t dimIdx = 0; dimIdx < dimensionCount; ++dimIdx)
        {
            if (remaining_elements > props.maxPerDimensionTensorElements)
            {
                parameters.dimensions.push_back(props.maxPerDimensionTensorElements);
                remaining_elements /= props.maxPerDimensionTensorElements;
            }
            else
            {
                parameters.dimensions.push_back(static_cast<uint32_t>(remaining_elements));
                remaining_elements = 1;
            }
        }
    }

    // We can only provide custom strides for linear tiling, and if the implementation supports non-packed tensors
    if (tiling == VK_TENSOR_TILING_LINEAR_ARM && deviceSupportsNonPackedTensors(context))
    {
        // Minimum value limit is checked and reported as error in separate test.
        // Here we just want to make sure we're not trying to use nonsensical strides in the test.
        static constexpr int64_t maxTensorStrideMinimumLimit = 65536;
        const int64_t maxTensorStride = std::max(props.maxTensorStride, maxTensorStrideMinimumLimit);

        const int64_t maxStrideAligned = maxTensorStride - (maxTensorStride % elementSize);
        const uint64_t maxSizeAligned  = props.maxTensorSize - (props.maxTensorSize % elementSize);

        const bool maxSizeAlignedFitsInStride =
            maxSizeAligned <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        const int64_t maxStride =
            (maxSizeAlignedFitsInStride && static_cast<int64_t>(maxSizeAligned) < maxStrideAligned) ? maxSizeAligned :
                                                                                                      maxStrideAligned;

        // Maximum tensor strides
        for (uint64_t dimensionCount = 1; dimensionCount <= props.maxTensorDimensionCount; ++dimensionCount)
        {
            maxParametersList.push_back({format, VK_TENSOR_TILING_LINEAR_ARM, {}, {}});
            TensorParameters &parameters = maxParametersList.back();

            for (uint64_t dimIdx = 0; dimIdx < dimensionCount; ++dimIdx)
            {
                parameters.dimensions.push_back(1);
                if (dimIdx == (dimensionCount - 1))
                {
                    parameters.strides.push_back(getFormatSize(format));
                }
                else
                {
                    parameters.strides.push_back(maxStride);
                }
            }
        }
    }

    return maxParametersList;
}

class TensorRequirementsTestInstance : public TestInstance
{
public:
    TensorRequirementsTestInstance(Context &testCtx, const VkFormat format, const VkTensorTilingARM tiling)
        : TestInstance(testCtx)
        , m_format(format)
        , m_tiling(tiling)
    {
    }

    tcu::TestStatus iterate() override
    {
        const std::vector<TensorParameters> &parameterList = getMaxTensorParameters(m_context, m_format, m_tiling);
        const auto device                                  = m_context.getDevice();
        const auto &vk                                     = m_context.getDeviceInterface();

        for (const auto &parameters : parameterList)
        {
            if (!formatSupportTensorFlags(m_context, m_format, m_tiling, VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
            {
                // Device does not support storage tensor of this format and tiling
                continue;
            }

            const VkTensorDescriptionARM tensorDesc =
                makeTensorDescription(m_tiling, m_format, parameters.dimensions, parameters.strides);
            const VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
            const auto tensor(createTensorARM(vk, device, &tensorCreateInfo));

            VkTensorMemoryRequirementsInfoARM tensorReqInfo{};
            tensorReqInfo.sType  = VK_STRUCTURE_TYPE_TENSOR_MEMORY_REQUIREMENTS_INFO_ARM;
            tensorReqInfo.tensor = *tensor;

            VkMemoryRequirements2 memReqInfo{};
            memReqInfo.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

            vk.getTensorMemoryRequirementsARM(device, &tensorReqInfo, &memReqInfo);

            /* Ensure at least one memory type is set */
            if (0 == memReqInfo.memoryRequirements.memoryTypeBits)
            {
                return tcu::TestStatus::fail("No memory type bits set");
            }

            // Check that required memory size is at least big enough to contain the tensor data
            // We don't make any assumptions about size for optimally tiled tensors
            if (m_tiling == VK_TENSOR_TILING_LINEAR_ARM)
            {
                VkDeviceSize expectedSize = 0;
                if (parameters.strides.empty())
                {
                    expectedSize = std::accumulate(parameters.dimensions.cbegin(), parameters.dimensions.cend(),
                                                   static_cast<VkDeviceSize>(1), std::multiplies<VkDeviceSize>()) *
                                   getFormatSize(parameters.format);
                }
                else
                {
                    DE_ASSERT(parameters.strides[0] > 0);
                    expectedSize = static_cast<VkDeviceSize>(parameters.strides[0]) * parameters.dimensions[0];
                }

                if (expectedSize > memReqInfo.memoryRequirements.size)
                {
                    std::ostringstream msg;
                    msg << "Unexpected memory requirement size. Expected " << expectedSize << " got "
                        << memReqInfo.memoryRequirements.size;
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }

        return tcu::TestStatus::pass("Tensor test succeeded");
    }

private:
    const VkFormat m_format;
    const VkTensorTilingARM m_tiling;
};

class TensorRequirementsTestCase : public TestCase
{
public:
    TensorRequirementsTestCase(tcu::TestContext &testCtx, const VkFormat format, const VkTensorTilingARM tiling)
        : TestCase(testCtx, std::string(tensorTilingShortName(tiling)) + "_" + tensorFormatShortName(format))
        , m_format(format)
        , m_tiling(tiling)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new TensorRequirementsTestInstance(ctx, m_format, m_tiling);
    }

    void checkSupport(Context &ctx) const override
    {
        ctx.requireDeviceFunctionality("VK_ARM_tensors");

        if (!formatSupportTensorFlags(ctx, m_format, m_tiling, VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }
    }

private:
    const VkFormat m_format;
    const VkTensorTilingARM m_tiling;
};

} // namespace

void addCreateRequirementTests(tcu::TestCaseGroup &testCaseGroup)
{
    const auto &formatList = getAllTestFormats();
    for (const auto &format : formatList)
    {
        for (const VkTensorTilingARM tiling : {VK_TENSOR_TILING_LINEAR_ARM, VK_TENSOR_TILING_OPTIMAL_ARM})
        {
            testCaseGroup.addChild(new TensorRequirementsTestCase(testCaseGroup.getTestContext(), format, tiling));
        }
    }
}

tcu::TestCaseGroup *createTensorCreateRequirementsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> createRequirementsTensorTests(
        new tcu::TestCaseGroup(testCtx, "creation_and_requirements"));
    addCreateRequirementTests(*createRequirementsTensorTests);

    return createRequirementsTensorTests.release();
}

} // namespace tensor
} // namespace vkt
