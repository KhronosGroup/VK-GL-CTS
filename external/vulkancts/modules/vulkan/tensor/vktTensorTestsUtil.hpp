#ifndef _VKTTENSORTESTSUTIL_HPP
#define _VKTTENSORTESTSUTIL_HPP
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

#include "vkDefs.hpp"

#include "vkTensorUtil.hpp"
#include "vkTensorWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include "vktTestCase.hpp"

#include <iostream>
#include <numeric>
#include <algorithm>
#include <functional>
#include <ctime> // For generating a random seed for Tensor dimensions

namespace vkt
{
namespace tensor
{

using namespace vk;
using namespace std::placeholders;

enum AccessVariant
{
    WRITE_TO_BUFFER,
    READ_FROM_BUFFER,
    ARRAY_READ,
    ARRAY_WRITE,
};

enum BooleanOperator
{
    AND,
    OR,
    NOT,
    XOR
};

size_t getFormatSize(VkFormat format);

struct TensorParameters
{
public:
    VkFormat format;
    VkTensorTilingARM tiling;
    TensorDimensions dimensions;
    TensorStrides strides;

    uint32_t rank() const
    {
        return static_cast<uint32_t>(dimensions.size());
    }
    uint32_t elements() const
    {
        return std::accumulate(dimensions.begin(), dimensions.end(), 1, std::multiplies<uint32_t>());
    }

    /* returns the size required to store all the tensor elements */
    size_t hostDataSize() const
    {
        return elements() * getFormatSize(format);
    }

    bool packed() const
    {
        if (tiling == VK_TENSOR_TILING_LINEAR_ARM && !strides.empty())
        {
            const TensorStrides packedStrides = getTensorStrides(dimensions, getFormatSize(format));
            return strides == packedStrides;
        }

        return true;
    }
};

VkPhysicalDeviceTensorPropertiesARM getTensorPhysicalDeviceProperties(Context &context);
VkPhysicalDeviceTensorPropertiesARM getTensorPhysicalDeviceProperties(const InstanceInterface &vki,
                                                                      const VkPhysicalDevice physicalDevice);
uint32_t getTensorMaxDimensionCount(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice);

size_t getFormatSize(VkFormat format);
bool formatSupportTensorFlags(Context &context, VkFormat format, VkTensorTilingARM tiling, VkFormatFeatureFlags2 flags);

bool deviceSupportsNonPackedTensors(Context &context);
bool deviceSupportsShaderTensorAccess(Context &context);
bool deviceSupportsShaderStagesTensorAccess(Context &context, const VkShaderStageFlags stages);

uint32_t selectMemoryTypeFromTypeBits(Context &context, uint32_t memoryTypeBits);

template <typename T>
const std::vector<VkFormat> getTestFormats();

const std::vector<VkFormat> getAllTestFormats();

const char *tensorFormatShortName(const VkFormat format);
const char *tensorTilingShortName(const VkTensorTilingARM tiling);

std::string paramsToString(const TensorParameters &params);
std::string paramsToString(const TensorParameters &params, const AccessVariant &variant);
std::string paramsToString(const TensorParameters &params, const BooleanOperator &op);
std::string paramsToString(const TensorDimensions &dimensions);

std::ostream &operator<<(std::ostream &os, TensorParameters params);
std::ostream &operator<<(std::ostream &os, AccessVariant variant);
std::ostream &operator<<(std::ostream &os, BooleanOperator op);

} // namespace tensor
} // namespace vkt

#endif // _VKTTENSORTESTSUTIL_HPP
