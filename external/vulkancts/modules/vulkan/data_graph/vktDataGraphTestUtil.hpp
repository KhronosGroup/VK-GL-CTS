#ifndef _VKTDATAGRAPHTESTUTIL_HPP
#define _VKTDATAGRAPHTESTUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Arm Ltd.
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
 * \brief DataGraph test utilities
 */
/*--------------------------------------------------------------------*/

#include "../tensor/vktTensorTestsUtil.hpp"

#include "vkTensorMemoryUtil.hpp"
#include "vkDataGraphUtil.hpp"

#include <bitset>
#include <cmath>

namespace vkt
{
namespace dataGraph
{

using namespace vk;

#define VK_CHECK_INCOMPLETE(EXPR) checkExpectedResult<VK_INCOMPLETE>((EXPR), #EXPR, __FILE__, __LINE__)
#define VK_CHECK_COMPILE_REQUIRED(EXPR) \
    checkExpectedResult<VK_PIPELINE_COMPILE_REQUIRED_EXT>((EXPR), #EXPR, __FILE__, __LINE__)

/**
 * @brief Checks that a Vulkan operation's result matches the expected value.
 *
 * @tparam ExpectedResult The expected VkResult value to compare against.
 * @param result The VkResult returned from a Vulkan operation.
 * @param msg Optional custom error message to prepend to the error details.
 * @param file The source file name where this check is performed.
 * @param line The line number in the source file corresponding to this check.
 *
 * @throws Error if result does not equal ExpectedResult.
 */
template <VkResult ExpectedResult>
void checkExpectedResult(VkResult result, const char *msg, const char *file, int line)
{
    if (result != ExpectedResult)
    {
        std::ostringstream msgStr;
        if (msg)
            msgStr << msg << ": ";

        msgStr << getResultStr(result);

        throw Error(result, msgStr.str().c_str(), nullptr, file, line);
    }
}

/**
 * @brief Checks if the provided Vulkan handle is a null handle.
 *
 * @tparam T The type of the object (typically a Vulkan handle) to check.
 * @param object The Vulkan handle to validate; expected to be VK_NULL_HANDLE.
 *
 * @throws TestError Thrown if object is not equal to VK_NULL_HANDLE.
 */
template <typename T>
void checkIsNull(T object)
{
    if (object != VK_NULL_HANDLE)
    {
        throw tcu::TestError("Object check() failed", (std::string(getTypeName<T>()) + " != VK_NULL_HANDLE").c_str(),
                             __FILE__, __LINE__);
    }
}

enum ResourceType
{
    RESOURCE_TYPE_INPUT = 0,
    RESOURCE_TYPE_OUTPUT,
    RESOURCE_TYPE_CONSTANT,
    RESOURCE_TYPE_COUNT
};

enum ResourceCardinality
{
    NONE,
    ONE,
    MANY,
};

struct ResourcesCardinalities
{
    ResourceCardinality inputs;
    ResourceCardinality outputs;
    ResourceCardinality constants;
};

enum StrideModes
{
    TENSOR_STRIDES_IMPLICIT,
    TENSOR_STRIDES_PACKED,
    TENSOR_STRIDES_NOT_PACKED,
};

struct ResourcesStrideModes
{
    StrideModes inputs;
    StrideModes outputs;
    StrideModes constants;
};

// skip combinations where output cardinalities is none
const std::vector<ResourcesCardinalities> allResourceCardinalityCombinations = {
    {NONE, ONE, NONE}, {NONE, ONE, ONE}, {NONE, ONE, MANY}, {NONE, MANY, NONE}, {NONE, MANY, ONE}, {NONE, MANY, MANY},
    {ONE, ONE, NONE},  {ONE, ONE, ONE},  {ONE, ONE, MANY},  {ONE, MANY, NONE},  {ONE, MANY, ONE},  {ONE, MANY, MANY},
    {MANY, ONE, NONE}, {MANY, ONE, ONE}, {MANY, ONE, MANY}, {MANY, MANY, NONE}, {MANY, MANY, ONE}, {MANY, MANY, MANY},
};

const std::vector<ResourcesStrideModes> allStrideModesCombinations = {
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED, TENSOR_STRIDES_NOT_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_IMPLICIT},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_PACKED},
    {TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED, TENSOR_STRIDES_NOT_PACKED},
};

struct DataGraphTestResource
{
    tensor::TensorDimensions dimensions;
    tensor::TensorStrides strides;
    VkTensorDescriptionARM desc;
    de::MovePtr<TensorWithMemory> tensor;
    Move<VkTensorViewARM> view;
    VkWriteDescriptorSetTensorARM writeDesc;
};
struct TestParams
{
public:
    std::string instructionSet{};

    // graph characteristics
    bool sessionMemory;
    ResourcesCardinalities cardinalities{};

    // graph parameters
    ResourcesStrideModes strides{};
    bool shuffleBindings{false};
    VkTensorTilingARM tiling{VK_TENSOR_TILING_LINEAR_ARM};
    bool sparseConstants{false};

    std::string formats{};

    bool packedInputs()
    {
        return strides.inputs != TENSOR_STRIDES_NOT_PACKED;
    }

    bool packedConstants()
    {
        return strides.constants != TENSOR_STRIDES_NOT_PACKED;
    }

    bool packedOutputs()
    {
        return strides.outputs != TENSOR_STRIDES_NOT_PACKED;
    }

    bool packedResources()
    {
        return strides.inputs != TENSOR_STRIDES_NOT_PACKED && strides.outputs != TENSOR_STRIDES_NOT_PACKED &&
               strides.constants != TENSOR_STRIDES_NOT_PACKED;
    }

    bool explictStrides()
    {
        return strides.inputs != TENSOR_STRIDES_IMPLICIT || strides.outputs != TENSOR_STRIDES_IMPLICIT ||
               strides.constants != TENSOR_STRIDES_IMPLICIT;
    }

    bool valid();

    static void checkSupport(Context &ctx, TestParams params)
    {
        const auto &vki           = ctx.getInstanceInterface();
        const auto physicalDevice = ctx.getPhysicalDevice();

        VkPhysicalDeviceDataGraphFeaturesARM dataGraphFeaturesProp = initVulkanStructure();
        VkPhysicalDeviceTensorFeaturesARM tensorFeaturesProp       = initVulkanStructure(&dataGraphFeaturesProp);
        VkPhysicalDeviceFeatures2 featuresProp                     = initVulkanStructure(&tensorFeaturesProp);

        vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

        if (!dataGraphFeaturesProp.dataGraph)
        {
            TCU_THROW(NotSupportedError, "dataGraph feature not present");
        }

        if (!dataGraphFeaturesProp.dataGraphShaderModule)
        {
            TCU_THROW(NotSupportedError, "dataGraphShaderModule feature not present");
        }

        if (!tensorFeaturesProp.tensors)
        {
            TCU_THROW(NotSupportedError, "tensors feature not present");
        }

        if (!tensorFeaturesProp.shaderTensorAccess)
        {
            TCU_THROW(NotSupportedError, "shaderTensorAccess feature not present");
        }

        if (!params.packedResources() && !tensorFeaturesProp.tensorNonPacked)
        {
            TCU_THROW(NotSupportedError, "tensorNonPacked feature not present");
        }
    }
};

struct ResourceInformation
{
public:
    ResourceType type;
    tensor::TensorParameters params;
    uint32_t binding;
    uint32_t descriptorSet;
    uint32_t id;
    void *hostData;
    std::vector<DataGraphConstantSparsityHint> sparsityInfo;
    std::string label;

    bool isTensor() const
    {
        return (type == RESOURCE_TYPE_INPUT || type == RESOURCE_TYPE_OUTPUT);
    }

    bool isInput() const
    {
        return (type == RESOURCE_TYPE_INPUT);
    }

    bool isOutput() const
    {
        return (type == RESOURCE_TYPE_OUTPUT);
    }

    bool isConstant() const
    {
        return (type == RESOURCE_TYPE_CONSTANT);
    }

    bool requiresVerify() const
    {
        return (type == RESOURCE_TYPE_OUTPUT);
    }
};

std::ostream &operator<<(std::ostream &os, TestParams params);
std::ostream &operator<<(std::ostream &os, ResourceType type);
std::ostream &operator<<(std::ostream &os, ResourceInformation resInfo);

struct InitDataOptions
{
    uint8_t startingValue;
    std::vector<DataGraphConstantSparsityHint> sparsityInfo;
};

class DataGraphTest
{
public:
    DataGraphTest(Context &context, size_t numResources) : m_context{context}
    {
        m_resInfo.resize(numResources);
    }

    DataGraphTest(Context &context, std::vector<ResourceInformation> resInfo) : m_context{context}, m_resInfo{resInfo}
    {
    }

    virtual ~DataGraphTest(){};

    virtual void initData(size_t id, TensorWithMemory *tensor = nullptr, InitDataOptions options = {0, {{}}}) = 0;
    virtual tcu::TestStatus verifyData(size_t id, TensorWithMemory *tensor)                                   = 0;

    /* data graph, in form of a pointer to pNext structure to append at pipeline creation */
    virtual std::vector<uint32_t> spirvBinary() = 0;

    Move<VkShaderModule> shaderModule()
    {
        const DeviceInterface &vk = m_context.getDeviceInterface();
        const VkDevice device     = m_context.getDevice();

        const auto binary = spirvBinary();
        de::MovePtr<ProgramBinary> programBinary =
            de::MovePtr<ProgramBinary>(new ProgramBinary(vk::PROGRAM_FORMAT_SPIRV, sizeof(uint32_t) * binary.size(),
                                                         reinterpret_cast<const uint8_t *>(binary.data())));
        return createShaderModule(vk, device, *programBinary);
    }

    size_t numTensors() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isTensor(); });
    }

    size_t numInputs() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isInput(); });
    }

    size_t numOutputs() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isOutput(); });
    }

    size_t numConstants() const
    {
        return std::count_if(m_resInfo.begin(), m_resInfo.end(), [](const auto &r) { return r.isConstant(); });
    }

    size_t numResources()
    {
        return m_resInfo.size();
    }

    ResourceInformation resourceInfo(size_t id)
    {
        return m_resInfo.at(id);
    }

    const std::vector<ResourceInformation> &resourceInfos() const
    {
        return m_resInfo;
    }

    /* Compile-time lookup of VkFormat properties */
    template <VkFormat T>
    struct vkFormatInfo;

protected:
    template <typename T>
    tcu::TestStatus verifyTensor(const tensor::StridedMemoryUtils<T> &outData,
                                 const tensor::StridedMemoryUtils<T> &refData)
    {
        if constexpr (std::is_floating_point_v<T> || std::is_same_v<T, vk::Float16>)
        {
            // for floating point types use `SNR` rather than direct comparison
            float signalPower = 0.0;
            float noisePower  = 0.0;

            for (size_t i = 0; i < outData.elementCount(); i++)
            {
                float noise = outData[i] - refData[i];
                signalPower += std::pow(refData[i], 2.0f);
                noisePower += std::pow(noise, 2.0f);
            }

            if (noisePower > 0.0)
            {
                /* we do not divide signalPower and noisePower by N as we are only interested in the ratio */
                const float snr    = 10.0f * std::log10(signalPower / noisePower);
                const float minSnr = 140.0f;

                if (snr < minSnr)
                {
                    std::ostringstream msg;
                    msg << "Elements in tensor has too low SNR (min=" << minSnr << " dB, actual=" << snr << " dB)\n";
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
        else
        {
            for (size_t i = 0; i < outData.elementCount(); i++)
            {
                if (outData[i] != refData[i])
                {
                    std::ostringstream msg;
                    msg << "Comparison failed at index " << i << ": tensor = " << outData[i]
                        << ", reference = " << refData[i];
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
        return tcu::TestStatus::pass("");
    }

    const Context &m_context;
    std::vector<ResourceInformation> m_resInfo;

    size_t m_numTensors{0};
    size_t m_numConstants{0};
};

std::vector<TestParams> getTestParamsVariations(
    const std::vector<std::string> instructionSets = {"TOSA"}, const std::vector<bool> sessionMemories = {false, true},
    const std::vector<ResourcesCardinalities> resourcesCardinalities = {allResourceCardinalityCombinations},
    const std::vector<ResourcesStrideModes> strideModes              = {allStrideModesCombinations},
    const std::vector<bool> shuffledBindings                         = {false, true},
    const std::vector<VkTensorTilingARM> tilings = {VK_TENSOR_TILING_LINEAR_ARM, VK_TENSOR_TILING_OPTIMAL_ARM},
    const std::vector<bool> sparseConstants      = {false, true});

/**
 * @brief Returns host type corresponding to the VkFormat
 */
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R8_BOOL_ARM>
{
    using hostType = uint8_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R8_UINT>
{
    using hostType = uint8_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R8_SINT>
{
    using hostType = int8_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R16_UINT>
{
    using hostType = uint16_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R16_SINT>
{
    using hostType = int16_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R16_SFLOAT>
{
    using hostType = vk::Float16;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R32_UINT>
{
    using hostType = uint32_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R32_SINT>
{
    using hostType = int32_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R32_SFLOAT>
{
    using hostType = float;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R64_UINT>
{
    using hostType = uint64_t;
};
template <>
struct DataGraphTest::vkFormatInfo<VK_FORMAT_R64_SINT>
{
    using hostType = int64_t;
};

} // namespace dataGraph
} // namespace vkt

#endif // _VKTDATAGRAPHTESTUTIL_HPP
