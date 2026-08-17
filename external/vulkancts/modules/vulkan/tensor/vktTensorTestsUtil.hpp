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
        return static_cast<uint32_t>(
            std::accumulate(dimensions.begin(), dimensions.end(), static_cast<int64_t>(1), std::multiplies<int64_t>()));
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
VkPhysicalDeviceTensorFeaturesARM getTensorPhysicalDeviceFeatures(Context &context);
uint32_t getTensorMaxDimensionCount(const InstanceInterface &vki, const VkPhysicalDevice physicalDevice);
VkTensorFormatPropertiesARM getTensorFormatProperties(Context &context, const VkFormat format);

size_t getFormatSize(VkFormat format);
bool formatSupportTensorFlags(Context &context, VkFormat format, VkTensorTilingARM tiling, VkFormatFeatureFlags2 flags);
bool formatSupportImageFlags(Context &context, VkFormat format, VkImageTiling tiling, VkImageUsageFlags flags);
bool tensorSupportsDmaBufImport(Context &context, const VkTensorDescriptionARM description);
void requireTensorShapeSupported(Context &context, const TensorParameters &parameters);

bool deviceSupportsNonPackedTensors(Context &context);
bool deviceSupportsShaderTensorAccess(Context &context);
bool deviceSupportsShaderStagesTensorAccess(Context &context, const VkShaderStageFlags stages);

uint32_t selectMemoryTypeFromTypeBits(Context &context, uint32_t memoryTypeBits);

const char *tensorFormatShortName(const VkFormat format);
const char *tensorTilingShortName(const VkTensorTilingARM tiling);

std::string paramsToString(const TensorParameters &params);
std::string paramsToString(const TensorParameters &params, const AccessVariant &variant);
std::string paramsToString(const TensorParameters &params, const BooleanOperator &op);
std::string paramsToString(const TensorDimensions &dimensions);

std::ostream &operator<<(std::ostream &os, TensorParameters params);
std::ostream &operator<<(std::ostream &os, AccessVariant variant);
std::ostream &operator<<(std::ostream &os, BooleanOperator op);

// clang-format make these much harder to read
// clang-format off

/* Tensor VkFormat to host type mappings */
template <VkFormat Format> struct VkFormatToHostType;
template <> struct VkFormatToHostType<VK_FORMAT_R8_UINT> { using HostType = uint8_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R8_SINT> { using HostType = int8_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R16_UINT> { using HostType = uint16_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R16_SINT> { using HostType = int16_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R32_UINT> { using HostType = uint32_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R32_SINT> { using HostType = int32_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R64_UINT> { using HostType = uint64_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R64_SINT> { using HostType = int64_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R16_SFLOAT> { using HostType = tcu::Float16; };
template <> struct VkFormatToHostType<VK_FORMAT_R32_SFLOAT> { using HostType = tcu::Float32; };
template <> struct VkFormatToHostType<VK_FORMAT_R8_BOOL_ARM> { using HostType = uint8_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM> { using HostType = tcu::FloatE5M2; };
template <> struct VkFormatToHostType<VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM> { using HostType = tcu::FloatE4M3; };
template <> struct VkFormatToHostType<VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM> { using HostType = tcu::BrainFloat16; };

/* Additional image VkFormat to host type mappings */
template <> struct VkFormatToHostType<VK_FORMAT_R8G8_SINT> { using HostType = int8_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R8G8_UINT> { using HostType = uint8_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R8G8B8A8_SINT> { using HostType = int8_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R8G8B8A8_UINT> { using HostType = uint8_t; };

template <> struct VkFormatToHostType<VK_FORMAT_R16G16_SINT> { using HostType = int16_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R16G16_UINT> { using HostType = uint16_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R16G16B16A16_SINT> { using HostType = int16_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R16G16B16A16_UINT> { using HostType = uint16_t; };

template <> struct VkFormatToHostType<VK_FORMAT_R32G32_SINT> { using HostType = int32_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R32G32_UINT> { using HostType = uint32_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R32G32B32A32_SINT> { using HostType = int32_t; };
template <> struct VkFormatToHostType<VK_FORMAT_R32G32B32A32_UINT> { using HostType = uint32_t; };

#define TENSOR_FORMATS_REGULAR_INTS \
        VK_FORMAT_R8_UINT, \
        VK_FORMAT_R8_SINT, \
        VK_FORMAT_R16_UINT, \
        VK_FORMAT_R16_SINT, \
        VK_FORMAT_R32_UINT, \
        VK_FORMAT_R32_SINT, \
        VK_FORMAT_R64_UINT, \
        VK_FORMAT_R64_SINT

#define TENSOR_FORMATS_REGULAR_FLOATS \
        VK_FORMAT_R16_SFLOAT, \
        VK_FORMAT_R32_SFLOAT

#define TENSOR_FORMATS_ALL \
        TENSOR_FORMATS_REGULAR_INTS, \
        TENSOR_FORMATS_REGULAR_FLOATS, \
        VK_FORMAT_R8_BOOL_ARM, \
        VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E5M2_ARM, \
        VK_FORMAT_R8_SFLOAT_FPENCODING_FLOAT8E4M3_ARM, \
        VK_FORMAT_R16_SFLOAT_FPENCODING_BFLOAT16_ARM

// clang-format on

} // namespace tensor
} // namespace vkt

#endif // _VKTTENSORTESTSUTIL_HPP
