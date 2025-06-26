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
 * \brief DataGraph Test Provider
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphTestProvider.hpp"

namespace vkt
{
namespace dataGraph
{

using namespace vk;

void DataGraphTestProvider::validate(const DataGraphTest *test, TestParams &params)
{
    if (params.cardinalities.constants != NONE && test->numConstants() == 0)
    {
        TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) +
                                     "'. No constants among the reported test resources.");
    }

    if (params.cardinalities.inputs == MANY && test->numInputs() < 2)
    {
        TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) +
                                     "'. No multiple inputs among the reported test resources.");
    }

    if (params.cardinalities.outputs == MANY && test->numOutputs() < 2)
    {
        TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) +
                                     "'. No multiple inputs among the reported test resources.");
    }

    // check tensor tiling
    for (const auto &ri : test->resourceInfos())
    {
        if (params.tiling != ri.params.tiling)
        {
            TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) +
                                         "'. Resources tiling differ from the requirements.");
        }
    }

    {
        std::array<bool, RESOURCE_TYPE_COUNT> isStridePacked{true, true, true};

        for (const auto &ri : test->resourceInfos())
        {

            if (ri.params.strides.size() == 0)
            {
                // no explicit strides means packed strides
                continue;
            }

            const TensorStrides packedStrides =
                getTensorStrides(ri.params.dimensions, tensor::getFormatSize(ri.params.format));

            for (size_t s = 0; s < ri.params.strides.size(); s++)
            {
                if (packedStrides.at(s) != ri.params.strides.at(s))
                {
                    isStridePacked.at(ri.type) = false;
                }
            }
        }

        if (params.packedInputs() != isStridePacked.at(RESOURCE_TYPE_INPUT))
        {
            TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) + "'. Wrong input strides.");
        }

        if (params.packedOutputs() != isStridePacked.at(RESOURCE_TYPE_OUTPUT))
        {
            TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) + "'. Wrong output strides.");
        }

        if (params.packedConstants() != isStridePacked.at(RESOURCE_TYPE_CONSTANT))
        {
            TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) + "'. Wrong constant strides.");
        }
    }

    if (params.sparseConstants)
    {
        bool foundSparsityInfo = false;
        for (const auto &ri : test->resourceInfos())
        {
            if (ri.isConstant() && ri.sparsityInfo.size() > 0)
            {
                foundSparsityInfo = true;
                for (const auto si : ri.sparsityInfo)
                {
                    if (si.dimension >= ri.params.dimensions.size())
                    {
                        TCU_THROW(InternalError,
                                  "Invalid test for params '" + de::toString(params) +
                                      "'. Sparsity info refers to a dimension that is bigger than the tensor shape.");
                    }
                    if ((ri.params.dimensions.at(si.dimension) % si.groupSize) != 0)
                    {
                        TCU_THROW(InternalError, "Invalid test for params '" + de::toString(params) + "'. Dimension " +
                                                     de::toString(si.dimension) +
                                                     " is not a multiple of the sparsity group size.");
                    }
                }
            }
        }
        if (!foundSparsityInfo)
        {
            TCU_THROW(InternalError,
                      "Invalid test for params '" + de::toString(params) + "'. No sparsity hints provided.");
        }
    }
}

} // namespace dataGraph
} // namespace vkt
