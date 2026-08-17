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

VkPhysicalDeviceTensorFeaturesARM getTensorPhysicalDeviceFeatures(Context &context)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    VkPhysicalDeviceTensorFeaturesARM tensorFeatures = initVulkanStructure();
    VkPhysicalDeviceFeatures2 features               = initVulkanStructure(&tensorFeatures);
    vki.getPhysicalDeviceFeatures2(physicalDevice, &features);

    return tensorFeatures;
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
    case VK_FORMAT_R64_SFLOAT:
        return 8;
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
        return 4;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM:
        return 2;
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_BOOL_ARM:
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM:
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM:
        return 1;
    default:
        // unsupported formats
        DE_ASSERT(false);
        return 0;
    }
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
        return "R64_UINT";
    case VK_FORMAT_R64_SINT:
        return "R64_SINT";
    case VK_FORMAT_R64_SFLOAT:
        return "R64_SFLOAT";
    case VK_FORMAT_R32_UINT:
        return "R32_UINT";
    case VK_FORMAT_R32_SINT:
        return "R32_SINT";
    case VK_FORMAT_R32_SFLOAT:
        return "R32_SFLOAT";
    case VK_FORMAT_R16_UINT:
        return "R16_UINT";
    case VK_FORMAT_R16_SINT:
        return "R16_SINT";
    case VK_FORMAT_R16_SFLOAT:
        return "R16_SFLOAT";
    case VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM:
        return "R16_SFLOAT_FPENCODING_BFLOAT16";
    case VK_FORMAT_R8_UINT:
        return "R8_UINT";
    case VK_FORMAT_R8_SINT:
        return "R8_SINT";
    case VK_FORMAT_R8_BOOL_ARM:
        return "R8_BOOL";
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM:
        return "R8_SFLOAT_FPENCODING_FLOAT8E5M2";
    case VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM:
        return "R8_SFLOAT_FPENCODING_FLOAT8E4M3";
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

VkTensorFormatPropertiesARM getTensorFormatProperties(Context &context, const VkFormat format)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    VkTensorFormatPropertiesARM tensorFormatProp = initVulkanStructure();

    VkFormatProperties2 formatProp = initVulkanStructure();
    formatProp.pNext               = &tensorFormatProp;

    vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProp);

    return tensorFormatProp;
}

bool formatSupportTensorFlags(Context &context, VkFormat format, VkTensorTilingARM tiling, VkFormatFeatureFlags2 flags)
{
    const VkTensorFormatPropertiesARM tensorFormatProp = getTensorFormatProperties(context, format);

    if (tiling == VK_TENSOR_TILING_OPTIMAL_ARM)
    {
        return ((tensorFormatProp.optimalTilingTensorFeatures & flags) == flags);
    }
    else
    {
        return ((tensorFormatProp.linearTilingTensorFeatures & flags) == flags);
    }
}

bool formatSupportImageFlags(Context &context, VkFormat format, VkImageTiling tiling, VkImageUsageFlags flags)
{
    const auto &vki               = context.getInstanceInterface();
    const auto physicalDevice     = context.getPhysicalDevice();
    VkImageFormatProperties props = {};
    return VK_SUCCESS == vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D, tiling,
                                                                    flags, 0, &props);
}

void requireTensorShapeSupported(Context &context, const TensorParameters &parameters)
{
    const VkPhysicalDeviceTensorPropertiesARM tensorProps = getTensorPhysicalDeviceProperties(context);

    // Max dimension count
    if (parameters.rank() > tensorProps.maxTensorDimensionCount)
    {
        TCU_THROW(NotSupportedError, "Tensor dimension count is higher than device limit");
    }

    // Max per dimension elements
    for (const auto &dimensionSize : parameters.dimensions)
    {
        if (static_cast<uint64_t>(dimensionSize) > tensorProps.maxPerDimensionTensorElements)
        {
            TCU_THROW(NotSupportedError, "Tensor dimension element count is higher than device limit");
        }
    }

    // Max elements
    if (parameters.elements() > tensorProps.maxTensorElements)
    {
        TCU_THROW(NotSupportedError, "Tensor element count is higher than device limit");
    }

    // Max size
    if (parameters.strides.empty())
    {
        // If we have implicitly packed linear tensor, calculate how much space the elements take
        // If we have optimal tensor, we can't really know its real size, but it should be at least this
        if (parameters.hostDataSize() > tensorProps.maxTensorSize)
        {
            TCU_THROW(NotSupportedError, "Tensor size in bytes is higher than device limit");
        }
    }
    else
    {
        DE_ASSERT(parameters.strides[0] > 0 && parameters.dimensions[0] > 0);

        // We have explicit strides, so calculate the size in bytes from it and tensor shape
        if (static_cast<uint64_t>(parameters.strides[0] * parameters.dimensions[0]) > tensorProps.maxTensorSize)
        {
            TCU_THROW(NotSupportedError, "Tensor size in bytes is higher than device limit");
        }
    }

    // Max stride
    for (const auto &strideSize : parameters.strides)
    {
        if (strideSize > tensorProps.maxTensorStride)
        {
            TCU_THROW(NotSupportedError, "Tensor stride is higher than device limit");
        }
    }
}

bool deviceSupportsNonPackedTensors(Context &context)
{
    return getTensorPhysicalDeviceFeatures(context).tensorNonPacked;
}

bool deviceSupportsShaderTensorAccess(Context &context)
{
    return getTensorPhysicalDeviceFeatures(context).shaderTensorAccess;
}

bool deviceSupportsShaderStagesTensorAccess(Context &context, const VkShaderStageFlags stages)
{
    const VkPhysicalDeviceTensorPropertiesARM tensorProps = getTensorPhysicalDeviceProperties(context);
    return (tensorProps.shaderTensorSupportedStages & stages) == stages;
}

bool tensorSupportsDmaBufImport(Context &context, const VkTensorDescriptionARM description)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    vk::VkPhysicalDeviceExternalTensorInfoARM tensorInfo = vk::initVulkanStructure();
    tensorInfo.pDescription                              = &description;
    tensorInfo.handleType                                = vk::VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    vk::VkExternalTensorPropertiesARM externalProperties = vk::initVulkanStructure();
    vki.getPhysicalDeviceExternalTensorPropertiesARM(physicalDevice, &tensorInfo, &externalProperties);

    const auto &externalMemoryProperties = externalProperties.externalMemoryProperties;
    const auto &externalMemoryFeatures   = externalMemoryProperties.externalMemoryFeatures;

    return (externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) &&
           !(externalMemoryFeatures & vk::VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) &&
           (externalMemoryProperties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
}

} // namespace tensor
} // namespace vkt
