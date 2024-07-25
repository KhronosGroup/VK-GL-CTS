/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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

#include "vktRandomUniformBlockCase.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace ubo
{

namespace
{

static std::string genName(char first, char last, int ndx)
{
    std::string str = "";
    int alphabetLen = last - first + 1;

    while (ndx > alphabetLen)
    {
        str.insert(str.begin(), (char)(first + ((ndx - 1) % alphabetLen)));
        ndx = (ndx - 1) / alphabetLen;
    }

    str.insert(str.begin(), (char)(first + (ndx % (alphabetLen + 1)) - 1));

    return str;
}

} // namespace

RandomUniformBlockCase::RandomUniformBlockCase(tcu::TestContext &testCtx, const std::string &name,
                                               BufferMode bufferMode, uint32_t features, uint32_t seed)
    : UniformBlockCase(testCtx, name, bufferMode, LOAD_FULL_MATRIX, (features & FEATURE_OUT_OF_ORDER_OFFSETS) != 0u)
    , m_features(features)
    , m_maxVertexBlocks((features & FEATURE_VERTEX_BLOCKS) ? 4 : 0)
    , m_maxFragmentBlocks((features & FEATURE_FRAGMENT_BLOCKS) ? 4 : 0)
    , m_maxSharedBlocks((features & FEATURE_SHARED_BLOCKS) ? 4 : 0)
    , m_maxInstances((features & FEATURE_INSTANCE_ARRAYS) ? 3 : 0)
    , m_maxArrayLength((features & FEATURE_ARRAYS) ? 8 : 0)
    , m_maxStructDepth((features & FEATURE_STRUCTS) ? 2 : 0)
    , m_maxBlockMembers(5)
    , m_maxStructMembers(4)
    , m_seed(seed)
    , m_blockNdx(1)
    , m_uniformNdx(1)
    , m_structNdx(1)
    , m_availableDescriptorUniformBuffers(12)
{
    de::Random rnd(m_seed);

    int numShared     = m_maxSharedBlocks > 0 ? rnd.getInt(1, m_maxSharedBlocks) : 0;
    int numVtxBlocks  = m_maxVertexBlocks - numShared > 0 ? rnd.getInt(1, m_maxVertexBlocks - numShared) : 0;
    int numFragBlocks = m_maxFragmentBlocks - numShared > 0 ? rnd.getInt(1, m_maxFragmentBlocks - numShared) : 0;

    // calculate how many additional descriptors we can use for arrays
    // this is needed for descriptor_indexing testing as we need to take in to account
    // maxPerStageDescriptorUniformBuffers limit and we can't query it as we need to
    // generate shaders before Context is created; minimal value of this limit is 12
    m_availableDescriptorUniformBuffers -= numVtxBlocks + numFragBlocks;

    for (int ndx = 0; ndx < numShared; ndx++)
        generateBlock(rnd, DECLARE_VERTEX | DECLARE_FRAGMENT);

    for (int ndx = 0; ndx < numVtxBlocks; ndx++)
        generateBlock(rnd, DECLARE_VERTEX);

    for (int ndx = 0; ndx < numFragBlocks; ndx++)
        generateBlock(rnd, DECLARE_FRAGMENT);

    init();
}

void RandomUniformBlockCase::generateBlock(de::Random &rnd, uint32_t layoutFlags)
{
    DE_ASSERT(m_blockNdx <= 'z' - 'a');

    const float instanceArrayWeight = 0.3f;
    UniformBlock &block             = m_interface.allocBlock(std::string("Block") + (char)('A' + m_blockNdx));
    int numInstances = (m_maxInstances > 0 && rnd.getFloat() < instanceArrayWeight) ? rnd.getInt(0, m_maxInstances) : 0;
    int numUniforms  = rnd.getInt(1, m_maxBlockMembers);

    if (m_features & FEATURE_DESCRIPTOR_INDEXING)
    {
        // generate arrays only when we are within the limit
        if (m_availableDescriptorUniformBuffers > 3)
            numInstances = rnd.getInt(2, 4);
        else if (m_availableDescriptorUniformBuffers > 1)
            numInstances = m_availableDescriptorUniformBuffers;
        else
            numInstances = 0;
        m_availableDescriptorUniformBuffers -= numInstances;
    }

    if (numInstances > 0)
        block.setArraySize(numInstances);

    if (numInstances > 0 || rnd.getBool())
        block.setInstanceName(std::string("block") + (char)('A' + m_blockNdx));

    // Layout flag candidates.
    std::vector<uint32_t> layoutFlagCandidates;
    layoutFlagCandidates.push_back(0);

    if (m_features & FEATURE_STD140_LAYOUT)
        layoutFlagCandidates.push_back(LAYOUT_STD140);

    if (m_features & FEATURE_STD430_LAYOUT)
        layoutFlagCandidates.push_back(LAYOUT_STD430);

    if (m_features & FEATURE_SCALAR_LAYOUT)
        layoutFlagCandidates.push_back(LAYOUT_SCALAR);

    if (m_features & FEATURE_16BIT_STORAGE)
        layoutFlags |= LAYOUT_16BIT_STORAGE;

    if (m_features & FEATURE_8BIT_STORAGE)
        layoutFlags |= LAYOUT_8BIT_STORAGE;

    if (m_features & FEATURE_DESCRIPTOR_INDEXING)
        layoutFlags |= LAYOUT_DESCRIPTOR_INDEXING;

    layoutFlags |= rnd.choose<uint32_t>(layoutFlagCandidates.begin(), layoutFlagCandidates.end());

    if (m_features & FEATURE_MATRIX_LAYOUT)
    {
        static const uint32_t matrixCandidates[] = {0, LAYOUT_ROW_MAJOR, LAYOUT_COLUMN_MAJOR};
        layoutFlags |=
            rnd.choose<uint32_t>(&matrixCandidates[0], &matrixCandidates[DE_LENGTH_OF_ARRAY(matrixCandidates)]);
    }

    block.setFlags(layoutFlags);

    for (int ndx = 0; ndx < numUniforms; ndx++)
        generateUniform(rnd, block, numInstances ? numInstances : 1);

    m_blockNdx += 1;
}

void RandomUniformBlockCase::generateUniform(de::Random &rnd, UniformBlock &block, uint32_t complexity)
{
    const float unusedVtxWeight  = 0.15f;
    const float unusedFragWeight = 0.15f;
    bool unusedOk                = (m_features & FEATURE_UNUSED_UNIFORMS) != 0;
    uint32_t flags               = 0;
    std::string name             = genName('a', 'z', m_uniformNdx);
    VarType type                 = generateType(rnd, 0, true, complexity);

    flags |= (unusedOk && rnd.getFloat() < unusedVtxWeight) ? UNUSED_VERTEX : 0;
    flags |= (unusedOk && rnd.getFloat() < unusedFragWeight) ? UNUSED_FRAGMENT : 0;

    block.addUniform(Uniform(name, type, flags));

    m_uniformNdx += 1;
}

VarType RandomUniformBlockCase::generateType(de::Random &rnd, int typeDepth, bool arrayOk, uint32_t complexity)
{
    const float structWeight = 0.1f;
    const float arrayWeight  = 0.1f;

    if (typeDepth < m_maxStructDepth && rnd.getFloat() < structWeight)
    {
        const float unusedVtxWeight  = 0.15f;
        const float unusedFragWeight = 0.15f;
        bool unusedOk                = (m_features & FEATURE_UNUSED_MEMBERS) != 0;
        std::vector<VarType> memberTypes;
        int numMembers = rnd.getInt(1, m_maxStructMembers);

        // Generate members first so nested struct declarations are in correct order.
        for (int ndx = 0; ndx < numMembers; ndx++)
            memberTypes.push_back(generateType(rnd, typeDepth + 1, true, complexity));

        StructType &structType = m_interface.allocStruct(std::string("s") + genName('A', 'Z', m_structNdx));
        m_structNdx += 1;

        DE_ASSERT(numMembers <= 'Z' - 'A');
        for (int ndx = 0; ndx < numMembers; ndx++)
        {
            uint32_t flags = 0;

            flags |= (unusedOk && rnd.getFloat() < unusedVtxWeight) ? UNUSED_VERTEX : 0;
            flags |= (unusedOk && rnd.getFloat() < unusedFragWeight) ? UNUSED_FRAGMENT : 0;

            structType.addMember(std::string("m") + (char)('A' + ndx), memberTypes[ndx], flags);
        }

        return VarType(&structType, m_shuffleUniformMembers ? static_cast<uint32_t>(LAYOUT_OFFSET) : 0u);
    }
    else if (m_maxArrayLength > 0 && arrayOk && rnd.getFloat() < arrayWeight)
    {
        const bool arraysOfArraysOk = (m_features & FEATURE_ARRAYS_OF_ARRAYS) != 0;
        int arrayLength             = rnd.getInt(1, m_maxArrayLength);

        if (complexity * arrayLength >= 70)
        {
            // Trim overly complicated cases (affects 18 cases out of 1576)
            arrayLength = 1;
        }

        VarType elementType = generateType(rnd, typeDepth, arraysOfArraysOk, complexity * arrayLength);
        return VarType(elementType, arrayLength);
    }
    else
    {
        std::vector<glu::DataType> typeCandidates;

        typeCandidates.push_back(glu::TYPE_FLOAT);
        typeCandidates.push_back(glu::TYPE_INT);
        typeCandidates.push_back(glu::TYPE_UINT);
        typeCandidates.push_back(glu::TYPE_BOOL);

        if (m_features & FEATURE_16BIT_STORAGE)
        {
            typeCandidates.push_back(glu::TYPE_UINT16);
            typeCandidates.push_back(glu::TYPE_INT16);
            typeCandidates.push_back(glu::TYPE_FLOAT16);
        }

        if (m_features & FEATURE_8BIT_STORAGE)
        {
            typeCandidates.push_back(glu::TYPE_UINT8);
            typeCandidates.push_back(glu::TYPE_INT8);
        }

        if (m_features & FEATURE_VECTORS)
        {
            typeCandidates.push_back(glu::TYPE_FLOAT_VEC2);
            typeCandidates.push_back(glu::TYPE_FLOAT_VEC3);
            typeCandidates.push_back(glu::TYPE_FLOAT_VEC4);
            typeCandidates.push_back(glu::TYPE_INT_VEC2);
            typeCandidates.push_back(glu::TYPE_INT_VEC3);
            typeCandidates.push_back(glu::TYPE_INT_VEC4);
            typeCandidates.push_back(glu::TYPE_UINT_VEC2);
            typeCandidates.push_back(glu::TYPE_UINT_VEC3);
            typeCandidates.push_back(glu::TYPE_UINT_VEC4);
            typeCandidates.push_back(glu::TYPE_BOOL_VEC2);
            typeCandidates.push_back(glu::TYPE_BOOL_VEC3);
            typeCandidates.push_back(glu::TYPE_BOOL_VEC4);
            if (m_features & FEATURE_16BIT_STORAGE)
            {
                typeCandidates.push_back(glu::TYPE_FLOAT16_VEC2);
                typeCandidates.push_back(glu::TYPE_FLOAT16_VEC3);
                typeCandidates.push_back(glu::TYPE_FLOAT16_VEC4);
                typeCandidates.push_back(glu::TYPE_INT16_VEC2);
                typeCandidates.push_back(glu::TYPE_INT16_VEC3);
                typeCandidates.push_back(glu::TYPE_INT16_VEC4);
                typeCandidates.push_back(glu::TYPE_UINT16_VEC2);
                typeCandidates.push_back(glu::TYPE_UINT16_VEC3);
                typeCandidates.push_back(glu::TYPE_UINT16_VEC4);
            }
            if (m_features & FEATURE_8BIT_STORAGE)
            {
                typeCandidates.push_back(glu::TYPE_INT8_VEC2);
                typeCandidates.push_back(glu::TYPE_INT8_VEC3);
                typeCandidates.push_back(glu::TYPE_INT8_VEC4);
                typeCandidates.push_back(glu::TYPE_UINT8_VEC2);
                typeCandidates.push_back(glu::TYPE_UINT8_VEC3);
                typeCandidates.push_back(glu::TYPE_UINT8_VEC4);
            }
        }

        if (m_features & FEATURE_MATRICES)
        {
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT2);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT2X3);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT3X2);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT3);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT3X4);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT4X2);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT4X3);
            typeCandidates.push_back(glu::TYPE_FLOAT_MAT4);
        }

        glu::DataType type = rnd.choose<glu::DataType>(typeCandidates.begin(), typeCandidates.end());
        uint32_t flags     = (m_shuffleUniformMembers ? static_cast<uint32_t>(LAYOUT_OFFSET) : 0u);

        if (glu::dataTypeSupportsPrecisionModifier(type))
        {
            // Precision.
            static const uint32_t precisionCandidates[] = {PRECISION_LOW, PRECISION_MEDIUM, PRECISION_HIGH};
            flags |= rnd.choose<uint32_t>(&precisionCandidates[0],
                                          &precisionCandidates[DE_LENGTH_OF_ARRAY(precisionCandidates)]);
        }

        return VarType(type, flags);
    }
}

} // namespace ubo
} // namespace vkt
