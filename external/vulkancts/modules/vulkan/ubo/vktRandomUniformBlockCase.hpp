#ifndef _VKTRANDOMUNIFORMBLOCKCASE_HPP
#define _VKTRANDOMUNIFORMBLOCKCASE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Random uniform block layout case.
 *//*--------------------------------------------------------------------*/

#include "vktUniformBlockCase.hpp"

namespace de
{
class Random;
} // namespace de

namespace vkt
{
namespace ubo
{

enum FeatureBits
{
    FEATURE_VECTORS              = (1 << 0),
    FEATURE_MATRICES             = (1 << 1),
    FEATURE_ARRAYS               = (1 << 2),
    FEATURE_STRUCTS              = (1 << 3),
    FEATURE_NESTED_STRUCTS       = (1 << 4),
    FEATURE_INSTANCE_ARRAYS      = (1 << 5),
    FEATURE_VERTEX_BLOCKS        = (1 << 6),
    FEATURE_FRAGMENT_BLOCKS      = (1 << 7),
    FEATURE_SHARED_BLOCKS        = (1 << 8),
    FEATURE_UNUSED_UNIFORMS      = (1 << 9),
    FEATURE_UNUSED_MEMBERS       = (1 << 10),
    FEATURE_PACKED_LAYOUT        = (1 << 12),
    FEATURE_SHARED_LAYOUT        = (1 << 13),
    FEATURE_STD140_LAYOUT        = (1 << 14),
    FEATURE_MATRIX_LAYOUT        = (1 << 15), //!< Matrix layout flags.
    FEATURE_ARRAYS_OF_ARRAYS     = (1 << 16),
    FEATURE_OUT_OF_ORDER_OFFSETS = (1 << 17),
    FEATURE_16BIT_STORAGE        = (1 << 18),
    FEATURE_8BIT_STORAGE         = (1 << 19),
    FEATURE_STD430_LAYOUT        = (1 << 20),
    FEATURE_SCALAR_LAYOUT        = (1 << 21),
    FEATURE_DESCRIPTOR_INDEXING  = (1 << 22),
};

class RandomUniformBlockCase : public UniformBlockCase
{
public:
    RandomUniformBlockCase(tcu::TestContext &testCtx, const std::string &name, BufferMode bufferMode, uint32_t features,
                           uint32_t seed);

private:
    void generateBlock(de::Random &rnd, uint32_t layoutFlags);
    void generateUniform(de::Random &rnd, UniformBlock &block, uint32_t complexity);
    VarType generateType(de::Random &rnd, int typeDepth, bool arrayOk, uint32_t complexity);

    const uint32_t m_features;
    const int m_maxVertexBlocks;
    const int m_maxFragmentBlocks;
    const int m_maxSharedBlocks;
    const int m_maxInstances;
    const int m_maxArrayLength;
    const int m_maxStructDepth;
    const int m_maxBlockMembers;
    const int m_maxStructMembers;
    const uint32_t m_seed;

    int m_blockNdx;
    int m_uniformNdx;
    int m_structNdx;
    int m_availableDescriptorUniformBuffers;
};

} // namespace ubo
} // namespace vkt

#endif // _VKTRANDOMUNIFORMBLOCKCASE_HPP
