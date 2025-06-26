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
 * \brief Tensor Tests Utility Classes
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTestsUtil.hpp"

#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deFloat16.h"

#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>

#include <inttypes.h>
#include <ostream>

namespace vkt
{
namespace tensor
{

using namespace vk;
using namespace std::placeholders;

const std::vector<VkFormat> getAllTestFormats()
{
    static const std::vector<VkFormat> testFormatList = {
        VK_FORMAT_R8_UINT,  VK_FORMAT_R8_SINT,  VK_FORMAT_R16_UINT, VK_FORMAT_R16_SINT,
        VK_FORMAT_R32_UINT, VK_FORMAT_R32_SINT, VK_FORMAT_R64_UINT, VK_FORMAT_R64_SINT,
    };

    return testFormatList;
}

VkPhysicalDeviceTensorPropertiesARM getTensorPhysicalDeviceProperties(const InstanceInterface &vki,
                                                                      const VkPhysicalDevice physicalDevice)
{
    vk::VkPhysicalDeviceTensorPropertiesARM tensorProperties = initVulkanStructure();

    vk::VkPhysicalDeviceProperties2 physicalDeviceProperties = initVulkanStructure();
    physicalDeviceProperties.pNext                           = &tensorProperties;
    vki.getPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);

    return tensorProperties;
}

VkPhysicalDeviceTensorPropertiesARM getTensorPhysicalDeviceProperties(Context &context)
{
    return getTensorPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice());
}

uint32_t getTensorMaxDimensionCount(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice)
{
    return getTensorPhysicalDeviceProperties(vki, physicalDevice).maxTensorDimensionCount;
}

size_t getFormatSize(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
        return 8;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
        return 4;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
        return 2;
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_BOOL_ARM:
        return 1;
    default:
        // unsupported formats
        DE_ASSERT(false);
        return 0;
    }
}

template <>
const std::vector<VkFormat> getTestFormats<uint64_t>()
{
    static const std::vector<VkFormat> testFormatList = {
        VK_FORMAT_R64_UINT,
        VK_FORMAT_R64_SINT,
    };

    return testFormatList;
}

template <>
const std::vector<VkFormat> getTestFormats<uint32_t>()
{
    static const std::vector<VkFormat> testFormatList = {
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32_SINT,
    };

    return testFormatList;
}

template <>
const std::vector<VkFormat> getTestFormats<uint16_t>()
{
    static const std::vector<VkFormat> testFormatList = {
        VK_FORMAT_R16_UINT,
        VK_FORMAT_R16_SINT,
    };

    return testFormatList;
}

template <>
const std::vector<VkFormat> getTestFormats<uint8_t>()
{
    static const std::vector<VkFormat> testFormatList = {
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
    };

    return testFormatList;
}

namespace
{
template <typename T>
std::string printList(std::vector<T> list)
{
    std::ostringstream strList;

    for (auto it = list.cbegin(); it != list.cend();)
    {
        strList << *it;

        ++it;

        if (it != list.cend())
        {
            strList << "_";
        }
    }

    return strList.str();
};
} // namespace

const char *tensorFormatShortName(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R64_UINT:
        return "r64_uint";
    case VK_FORMAT_R64_SINT:
        return "r64_sint";
    case VK_FORMAT_R64_SFLOAT:
        return "r64_sfloat";
    case VK_FORMAT_R32_UINT:
        return "r32_uint";
    case VK_FORMAT_R32_SINT:
        return "r32_sint";
    case VK_FORMAT_R32_SFLOAT:
        return "r32_sfloat";
    case VK_FORMAT_R16_UINT:
        return "r16_uint";
    case VK_FORMAT_R16_SINT:
        return "r16_sint";
    case VK_FORMAT_R16_SFLOAT:
        return "r16_sfloat";
    case VK_FORMAT_R8_UINT:
        return "r8_uint";
    case VK_FORMAT_R8_SINT:
        return "r8_sint";
    case VK_FORMAT_R8_BOOL_ARM:
        return "r8_bool";
    default:
        // unsupported formats
        DE_ASSERT(false);
        return nullptr;
    }
}

const char *tensorTilingShortName(const VkTensorTilingARM tiling)
{
    switch (tiling)
    {
    case VK_TENSOR_TILING_LINEAR_ARM:
        return "linear";
    case VK_TENSOR_TILING_OPTIMAL_ARM:
        return "optimal";
    default:
        // Unsupported tiling
        DE_ASSERT(false);
        return nullptr;
    }
}

std::string paramsToString(const TensorParameters &params)
{
    std::ostringstream testName;
    testName << tensorFormatShortName(params.format) << "_" << tensorTilingShortName(params.tiling);
    if (params.dimensions.size() > 0)
    {
        testName << "_shape_" << printList(params.dimensions);
        if (!params.strides.empty())
        {
            testName << "_strides_" << printList(params.strides);
        }
    }
    else
    {
        testName << "_max_rank";
    }
    return testName.str();
}

std::string paramsToString(const TensorParameters &params, const AccessVariant &variant)
{
    std::ostringstream testName;
    testName << paramsToString(params) << "_" << variant;
    return testName.str();
}

std::string paramsToString(const TensorParameters &params, const BooleanOperator &op)
{
    std::ostringstream testName;
    testName << paramsToString(params) << "_operator_" << op;
    return testName.str();
}

std::string paramsToString(const TensorDimensions &dimensions)
{
    std::ostringstream testName;
    testName << "_dim_" + printList(dimensions);
    return testName.str();
}

std::ostream &operator<<(std::ostream &os, AccessVariant variant)
{
    switch (variant)
    {
    case AccessVariant::WRITE_TO_BUFFER:
        os << "shader_read";
        break;
    case AccessVariant::READ_FROM_BUFFER:
        os << "shader_write";
        break;
    case AccessVariant::ARRAY_WRITE:
        os << "array_write";
        break;
    case AccessVariant::ARRAY_READ:
        os << "array_read";
        break;
    default:
        // unsupported formats
        DE_ASSERT(false);
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, BooleanOperator op)
{
    switch (op)
    {
    case BooleanOperator::AND:
        os << "and";
        break;
    case BooleanOperator::OR:
        os << "or";
        break;
    case BooleanOperator::XOR:
        os << "xor";
        break;
    case BooleanOperator::NOT:
        os << "not";
        break;
    default:
        // unsupported formats
        DE_ASSERT(false);
    }

    return os;
}

uint32_t selectMemoryTypeFromTypeBits(Context &context, uint32_t memoryTypeBits)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceMemoryProperties memProperties{};
    vki.getPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    unsigned memoryType = 0;
    for (; memoryType < memProperties.memoryTypeCount; memoryType++)
    {
        // If the memory type meets the requirements, use it.
        if ((1U << memoryType) & memoryTypeBits)
        {
            break;
        }
    }
    DE_ASSERT(memoryType < memProperties.memoryTypeCount);
    return memoryType;
}

bool formatSupportTensorFlags(Context &context, VkFormat format, VkTensorTilingARM tiling, VkFormatFeatureFlags2 flags)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkTensorFormatPropertiesARM tensorFormatProp{};
    tensorFormatProp.sType = VK_STRUCTURE_TYPE_TENSOR_FORMAT_PROPERTIES_ARM;

    VkFormatProperties2 formatProp{};
    formatProp.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    formatProp.pNext = &tensorFormatProp;

    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProp);

    if (tiling == VK_TENSOR_TILING_OPTIMAL_ARM)
    {
        return ((tensorFormatProp.optimalTilingTensorFeatures & flags) == flags);
    }
    else
    {
        return ((tensorFormatProp.linearTilingTensorFeatures & flags) == flags);
    }
}

bool deviceSupportsNonPackedTensors(Context &context)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceTensorFeaturesARM tensorFeaturesProp{};
    tensorFeaturesProp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TENSOR_FEATURES_ARM;

    VkPhysicalDeviceFeatures2 featuresProp{};
    featuresProp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    featuresProp.pNext = &tensorFeaturesProp;

    vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

    return tensorFeaturesProp.tensorNonPacked;
}

bool deviceSupportsShaderTensorAccess(Context &context)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceTensorFeaturesARM tensorFeaturesProp{};
    tensorFeaturesProp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TENSOR_FEATURES_ARM;

    VkPhysicalDeviceFeatures2 featuresProp{};
    featuresProp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    featuresProp.pNext = &tensorFeaturesProp;

    vki.getPhysicalDeviceFeatures2(physicalDevice, &featuresProp);

    return tensorFeaturesProp.shaderTensorAccess;
}

bool deviceSupportsShaderStagesTensorAccess(Context &context, const VkShaderStageFlags stages)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceTensorPropertiesARM tensorProps = initVulkanStructure();

    VkPhysicalDeviceProperties2 props = initVulkanStructure();
    props.pNext                       = &tensorProps;

    vki.getPhysicalDeviceProperties2(physicalDevice, &props);

    return (tensorProps.shaderTensorSupportedStages & stages) == stages;
}

} // namespace tensor
} // namespace vkt
