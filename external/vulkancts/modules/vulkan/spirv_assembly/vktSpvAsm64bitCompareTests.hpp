#ifndef _VKTSPVASM64BITCOMPARETESTS_HPP
#define _VKTSPVASM64BITCOMPARETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief 64-bit data type comparison operations.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace SpirVAssembly
{

tcu::TestCaseGroup *create64bitCompareGraphicsGroup(tcu::TestContext &testCtx);
tcu::TestCaseGroup *create64bitCompareComputeGroup(tcu::TestContext &testCtx);

} // namespace SpirVAssembly
} // namespace vkt

#endif // _VKTSPVASM64BITCOMPARETESTS_HPP
