#ifndef _VKTTESTCASEDEFS_HPP
#define _VKTTESTCASEDEFS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 * \brief Vulkan test case definitions
 *//*--------------------------------------------------------------------*/

namespace vkt
{
enum DeviceCoreFeature
{
    DEVICE_CORE_FEATURE_ROBUST_BUFFER_ACCESS                         = 0,
    DEVICE_CORE_FEATURE_FULL_DRAW_INDEX_UINT32                       = 1,
    DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY                             = 2,
    DEVICE_CORE_FEATURE_INDEPENDENT_BLEND                            = 3,
    DEVICE_CORE_FEATURE_GEOMETRY_SHADER                              = 4,
    DEVICE_CORE_FEATURE_TESSELLATION_SHADER                          = 5,
    DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING                          = 6,
    DEVICE_CORE_FEATURE_DUAL_SRC_BLEND                               = 7,
    DEVICE_CORE_FEATURE_LOGIC_OP                                     = 8,
    DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT                          = 9,
    DEVICE_CORE_FEATURE_DRAW_INDIRECT_FIRST_INSTANCE                 = 10,
    DEVICE_CORE_FEATURE_DEPTH_CLAMP                                  = 11,
    DEVICE_CORE_FEATURE_DEPTH_BIAS_CLAMP                             = 12,
    DEVICE_CORE_FEATURE_FILL_MODE_NON_SOLID                          = 13,
    DEVICE_CORE_FEATURE_DEPTH_BOUNDS                                 = 14,
    DEVICE_CORE_FEATURE_WIDE_LINES                                   = 15,
    DEVICE_CORE_FEATURE_LARGE_POINTS                                 = 16,
    DEVICE_CORE_FEATURE_ALPHA_TO_ONE                                 = 17,
    DEVICE_CORE_FEATURE_MULTI_VIEWPORT                               = 18,
    DEVICE_CORE_FEATURE_SAMPLER_ANISOTROPY                           = 19,
    DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ETC2                     = 20,
    DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ASTC_LDR                 = 21,
    DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_BC                       = 22,
    DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE                      = 23,
    DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY                    = 24,
    DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS           = 25,
    DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS                  = 26,
    DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE  = 27,
    DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED                 = 28,
    DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_EXTENDED_FORMATS        = 29,
    DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_MULTISAMPLE             = 30,
    DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT     = 31,
    DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT    = 32,
    DEVICE_CORE_FEATURE_SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING = 33,
    DEVICE_CORE_FEATURE_SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING  = 34,
    DEVICE_CORE_FEATURE_SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING = 35,
    DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING  = 36,
    DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE                         = 37,
    DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE                         = 38,
    DEVICE_CORE_FEATURE_SHADER_FLOAT64                               = 39,
    DEVICE_CORE_FEATURE_SHADER_INT64                                 = 40,
    DEVICE_CORE_FEATURE_SHADER_INT16                                 = 41,
    DEVICE_CORE_FEATURE_SHADER_RESOURCE_RESIDENCY                    = 42,
    DEVICE_CORE_FEATURE_SHADER_RESOURCE_MIN_LOD                      = 43,
    DEVICE_CORE_FEATURE_SPARSE_BINDING                               = 44,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER                      = 45,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE2D                     = 46,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE3D                     = 47,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY2_SAMPLES                    = 48,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY4_SAMPLES                    = 49,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY8_SAMPLES                    = 50,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY16_SAMPLES                   = 51,
    DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED                     = 52,
    DEVICE_CORE_FEATURE_VARIABLE_MULTISAMPLE_RATE                    = 53,
    DEVICE_CORE_FEATURE_INHERITED_QUERIES                            = 54,
};

} // namespace vkt

#endif // _VKTTESTCASEDEFS_HPP
