/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024-2025 ARM Ltd.
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
        os << "no";
    }
    break;
    case ONE:
    {
        os << "one";
    }
    break;
    case MANY:
    {
        os << "many";
    }
    break;
    default:
        break;
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, TestParams params)
{
    os << de::toLower(params.instructionSet);

    os << "_" << params.cardinalities.inputs << "In";
    os << "_" << params.cardinalities.outputs << "Out";
    os << "_" << params.cardinalities.constants << "Const";

    os << "_" << (params.sessionMemory ? "session" : "noSession");

    os << "_" << params.formats;

    os << "_" << params.strides.inputs << "In";
    os << "_" << params.strides.outputs << "Out";
    os << "_" << params.strides.constants << "Const";

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

    os << (params.sparseConstants ? "_sparseConstants" : "");

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

bool TestParams::valid()
{
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
    if (cardinalities.constants == NONE && sparseConstants)
    {
        // if the graph does not contain constants, we cannot have sparse constants
        return false;
    }
    if (cardinalities.outputs == NONE)
    {
        // all graphs must have at least one output
        return false;
    }

    return true;
}

std::vector<TestParams> getTestParamsVariations(const std::vector<std::string> instructionSets,
                                                const std::vector<bool> sessionMemories,
                                                const std::vector<ResourcesCardinalities> resourcesCardinalities,
                                                const std::vector<ResourcesStrideModes> resourceStrideModes,
                                                const std::vector<bool> shuffledBindings,
                                                const std::vector<VkTensorTilingARM> tilings,
                                                const std::vector<bool> sparseConstants)
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
                            for (auto sparseConstant : sparseConstants)
                            {
                                TestParams params = {instructionSet,     sessionMemory,   resourcesCardinality,
                                                     resourceStrideMode, shuffledBinding, tiling,
                                                     sparseConstant};
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
    return paramsVariations;
}

} // namespace dataGraph

} // namespace vkt
