#ifndef _VKTCOOPERATIVEVECTORBASICTESTS_HPP
#define _VKTCOOPERATIVEVECTORBASICTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
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
 * \brief Vulkan Cooperative Matrix Tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace cooperative_vector
{
tcu::TestCaseGroup *createCooperativeVectorBasicTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createCooperativeVectorMatrixMulTests(tcu::TestContext &testCtx);

tcu::TestCaseGroup *createCooperativeVectorTrainingTests(tcu::TestContext &testCtx);

} // namespace cooperative_vector
} // namespace vkt

#endif // _VKTCOOPERATIVEVECTORBASICTESTS_HPP
