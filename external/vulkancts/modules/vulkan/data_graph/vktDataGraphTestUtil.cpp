/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024-2026 ARM Ltd.
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

#include "vktDataGraphTestUtil.hpp"
#include "vktDataGraphTestProvider.hpp"
#include "vktTestCase.hpp"

#include "deStringUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTensorUtil.hpp"

#include <functional>
#include <memory>
#include <numeric>
#include <type_traits>
#include <vector>

namespace vkt
{

namespace dataGraph
{

std::ostream &operator<<(std::ostream &os, StrideModes strideModes)
{
    switch (strideModes)
    {
    case TENSOR_STRIDES_IMPLICIT:
        os << "implicit";
        break;
    case TENSOR_STRIDES_PACKED:
        os << "packed";
        break;
    case TENSOR_STRIDES_NOT_PACKED:
        os << "notPacked";
        break;
    case TENSOR_STRIDES_IMAGE_ALIASING:
        os << "ImageAliased";
        break;
    default:
        break;
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, ResourceCardinality cardinality)
{
    switch (cardinality)
    {
    case NONE:
    {
        os << "No";
    }
    break;
    case ONE:
    {
        os << "One";
    }
    break;
    case MANY:
    {
        os << "Many";
    }
    break;
    default:
        break;
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, SparsityVariation sparsity)
{
    switch (sparsity)
    {
    case SparsityVariation::NONE:
        break;
    case SparsityVariation::VARIATION_1:
        os << "Sparse1";
        break;
    case SparsityVariation::VARIATION_2:
        os << "Sparse2";
        break;
    case SparsityVariation::VARIATION_3:
        os << "Sparse3";
        break;
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, TestParams params)
{
    os << de::toLower(params.instructionSet);

    // Inputs, outputs and constants specific parameters
    os << "_"
       << "in" << params.cardinalities.inputs
       << (params.cardinalities.inputs != NONE ? de::toString(params.strides.inputs) : "");
    os << "_"
       << "out" << params.cardinalities.outputs
       << (params.cardinalities.outputs != NONE ? de::toString(params.strides.outputs) : "");
    os << "_"
       << "const" << params.cardinalities.constants
       << (params.cardinalities.constants != NONE ? de::toString(params.strides.constants) : "") << params.sparsity;

    os << "_" << (params.sessionMemory ? "session" : "noSession");

    os << "_" << params.formats;

    os << (params.shuffleBindings ? "_unorderedBindings" : "_orderedBindings");

    switch (params.tiling)
    {
    case VK_TENSOR_TILING_LINEAR_ARM:
    {
        os << "_linearTiling";
    }
    break;
    case VK_TENSOR_TILING_OPTIMAL_ARM:
    {
        os << "_optimalTiling";
    }
    break;
    default:
        break;
    }

    switch (params.specConstants)
    {
    case SpecConstantTest::NONE:
        break;
    case SpecConstantTest::BASIC:
        os << "_specializationConstant";
        break;
    case SpecConstantTest::BOOL:
        os << "_specializationConstantBool";
        break;
    case SpecConstantTest::COMPOSITE:
        os << "_specializationConstantComposite";
        break;
    case SpecConstantTest::COMPOSITE_REPLICATED:
        os << "_specializationConstantCompositeReplicated";
        break;
    case SpecConstantTest::OP:
        os << "_specializationConstantOp";
        break;
    default:
        TCU_THROW(InternalError, "Unhandled SpecConstantTest value");
    }

    return os;
}

std::ostream &operator<<(std::ostream &os, ResourceType type)
{
    switch (type)
    {
    case RESOURCE_TYPE_INPUT:
        os << "INPUT";
        break;
    case RESOURCE_TYPE_OUTPUT:
        os << "OUTPUT";
        break;
    case RESOURCE_TYPE_CONSTANT:
        os << "CONSTANT";
        break;
    default:
        break;
    }

    return os;
}

std::string getImageAliasingFillShaderName(VkFormat format)
{
    return "fill_image_aliased_tensor_" + getFormatSimpleName(format);
}

std::string getImageAliasingVerifyShaderName(VkFormat format)
{
    return "verify_image_aliased_tensor_" + getFormatSimpleName(format);
}

bool TestParams::valid() const
{
    if (specConstants != SpecConstantTest::NONE)
    {
        if (sparsity != SparsityVariation::NONE || sessionMemory || cardinalities.inputs != ONE ||
            cardinalities.outputs != ONE || cardinalities.constants != NONE)
        {
            return false;
        }
    }

    if (imageAliasing)
    {
        if (cardinalities.inputs == ONE && cardinalities.outputs == ONE &&
            (cardinalities.constants == NONE || cardinalities.constants == MANY) && !sessionMemory &&
            tiling == VK_TENSOR_TILING_LINEAR_ARM && strides.inputs == TENSOR_STRIDES_IMAGE_ALIASING &&
            strides.outputs == TENSOR_STRIDES_IMAGE_ALIASING)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    if (tiling == VK_TENSOR_TILING_OPTIMAL_ARM && explictStrides())
    {
        // optimal tiling does not support explicit strides
        return false;
    }
    if (strides.constants == TENSOR_STRIDES_NOT_PACKED)
    {
        // constants can only be packed
        return false;
    }
    if (cardinalities.constants == NONE && strides.constants != TENSOR_STRIDES_IMPLICIT)
    {
        // if the graph does not contain constants, the only value valid for constants' strides is implicit
        return false;
    }
    if (cardinalities.inputs == NONE && strides.inputs != TENSOR_STRIDES_IMPLICIT)
    {
        // if the graph does not contain inputs, the only value valid for inputs' strides is implicit
        return false;
    }
    if (cardinalities.constants == NONE && sparsity != SparsityVariation::NONE)
    {
        // if the graph does not contain constants, we cannot have sparse constants
        return false;
    }
    if (cardinalities.outputs == NONE)
    {
        // all graphs must have at least one output
        return false;
    }

    if ((strides.inputs == TENSOR_STRIDES_IMAGE_ALIASING || strides.outputs == TENSOR_STRIDES_IMAGE_ALIASING) &&
        !imageAliasing)
    {
        // image aliasing strides are only supported in image aliasing tests
        return false;
    }

    if ((strides.inputs != TENSOR_STRIDES_IMAGE_ALIASING || strides.outputs != TENSOR_STRIDES_IMAGE_ALIASING) &&
        imageAliasing)
    {
        // image aliasing tests requires both input and output to use image aliasing strides
        return false;
    }

    return true;
}

std::vector<TestParams> getTestParamsVariations(
    const std::vector<std::string> instructionSets, const std::vector<bool> sessionMemories,
    const std::vector<ResourcesCardinalities> resourcesCardinalities,
    const std::vector<ResourcesStrideModes> resourceStrideModes, const std::vector<bool> shuffledBindings,
    const std::vector<VkTensorTilingARM> tilings, const std::vector<SparsityVariation> sparsityVariations,
    const bool imageAliasing, const std::vector<SpecConstantTest> specConstants)
{
    std::vector<TestParams> paramsVariations{};

    for (auto &instructionSet : instructionSets)
    {
        for (auto sessionMemory : sessionMemories)
        {
            for (auto resourcesCardinality : resourcesCardinalities)
            {
                for (auto resourceStrideMode : resourceStrideModes)
                {
                    for (auto shuffledBinding : shuffledBindings)
                    {
                        for (auto tiling : tilings)
                        {
                            for (auto sparsityVariation : sparsityVariations)
                            {
                                for (auto specConstant : specConstants)
                                {
                                    TestParams params = {instructionSet,     sessionMemory,   resourcesCardinality,
                                                         resourceStrideMode, shuffledBinding, tiling,
                                                         sparsityVariation,  imageAliasing,   specConstant};
                                    if (params.valid())
                                    {
                                        const auto &supportedFormats =
                                            DataGraphTestProvider::getSupportedFormats(instructionSet, params);
                                        for (const auto &formats : supportedFormats)
                                        {
                                            params.formats = formats;
                                            paramsVariations.push_back(params);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return paramsVariations;
}

} // namespace dataGraph

} // namespace vkt
