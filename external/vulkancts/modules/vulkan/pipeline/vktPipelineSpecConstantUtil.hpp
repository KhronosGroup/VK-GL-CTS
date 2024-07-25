#ifndef _VKTPIPELINESPECCONSTANTUTIL_HPP
#define _VKTPIPELINESPECCONSTANTUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 *//*!
 * \file
 * \brief Pipeline specialization constants test utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace pipeline
{

enum FeatureFlagBits
{
    FEATURE_TESSELLATION_SHADER                = 1u << 0,
    FEATURE_GEOMETRY_SHADER                    = 1u << 1,
    FEATURE_SHADER_FLOAT_64                    = 1u << 2,
    FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS = 1u << 3,
    FEATURE_FRAGMENT_STORES_AND_ATOMICS        = 1u << 4,
    FEATURE_SHADER_INT_64                      = 1u << 5,
    FEATURE_SHADER_INT_16                      = 1u << 6,
    FEATURE_SHADER_FLOAT_16                    = 1u << 7,
    FEATURE_SHADER_INT_8                       = 1u << 8,
};
typedef uint32_t FeatureFlags;

vk::VkImageCreateInfo makeImageCreateInfo(const tcu::IVec2 &size, const vk::VkFormat format,
                                          const vk::VkImageUsageFlags usage);
void requireFeatures(vkt::Context &context, const FeatureFlags flags);

// Ugly, brute-force replacement for the initializer list

template <typename T>
std::vector<T> makeVector(const T &o1)
{
    std::vector<T> vec;
    vec.reserve(1);
    vec.push_back(o1);
    return vec;
}

template <typename T>
std::vector<T> makeVector(const T &o1, const T &o2)
{
    std::vector<T> vec;
    vec.reserve(2);
    vec.push_back(o1);
    vec.push_back(o2);
    return vec;
}

template <typename T>
std::vector<T> makeVector(const T &o1, const T &o2, const T &o3)
{
    std::vector<T> vec;
    vec.reserve(3);
    vec.push_back(o1);
    vec.push_back(o2);
    vec.push_back(o3);
    return vec;
}

template <typename T>
std::vector<T> makeVector(const T &o1, const T &o2, const T &o3, const T &o4)
{
    std::vector<T> vec;
    vec.reserve(4);
    vec.push_back(o1);
    vec.push_back(o2);
    vec.push_back(o3);
    vec.push_back(o4);
    return vec;
}

} // namespace pipeline
} // namespace vkt

#endif // _VKTPIPELINESPECCONSTANTUTIL_HPP
