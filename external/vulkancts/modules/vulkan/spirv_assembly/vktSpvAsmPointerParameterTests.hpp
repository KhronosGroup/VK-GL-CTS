#ifndef _VKTSPVASMPOINTERPARAMETERTESTS_HPP
#define _VKTSPVASMPOINTERPARAMETERTESTS_HPP
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
 * \brief SPIR-V Assembly Tests for pointers as function parameters.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace SpirVAssembly
{

tcu::TestCaseGroup *createPointerParameterComputeGroup(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createPointerParameterGraphicsGroup(tcu::TestContext &testCtx);

} // namespace SpirVAssembly
} // namespace vkt

#endif // _VKTSPVASMPOINTERPARAMETERTESTS_HPP
