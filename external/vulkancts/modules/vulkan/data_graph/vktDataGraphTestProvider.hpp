#ifndef _VKTDATAGRAPHTESTPROVIDER_HPP
#define _VKTDATAGRAPHTESTPROVIDER_HPP
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

#include "../tensor/vktTensorTestsUtil.hpp"
#include "vktDataGraphTestUtil.hpp"
#include "tosa/vktDataGraphTosaUtil.hpp"

#include "vkTensorMemoryUtil.hpp"

namespace vkt
{
namespace dataGraph
{

using namespace vk;

class DataGraphTestProvider
{
public:
    static DataGraphTest *getDataGraphTest(Context &testCtx, std::string instructionSet, TestParams params)
    {
        DataGraphTest *test = nullptr;

        if (instructionSet == "TOSA")
        {
            test = DataGraphTestProviderTosa::getDataGraphTest(testCtx, params);
        }

        /* Add here tests for other instruction sets */

        if (!test)
        {
            TCU_THROW(NotSupportedError, "No test available for " + instructionSet + " and given test parameters");
        }

        validate(test, params);

        return test;
    }

    static const std::vector<std::string> &getSupportedFormats(std::string instructionSet, TestParams params)
    {
        static const std::vector<std::string> emptyFormats = {};

        if (instructionSet == "TOSA")
        {
            return DataGraphTestProviderTosa::getSupportedFormats(params);
        }

        return emptyFormats;
    }

private:
    static void validate(const DataGraphTest *test, TestParams &params);

    /* No need to instantiate the class as we only expose static methods */
    DataGraphTestProvider() = default;
};

} // namespace dataGraph
} // namespace vkt

#endif // _VKTDATAGRAPHTESTPROVIDER_HPP
