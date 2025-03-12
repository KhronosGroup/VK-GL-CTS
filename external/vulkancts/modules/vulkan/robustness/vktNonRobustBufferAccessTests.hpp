#ifndef _VKTNONROBUSTBUFFERACCESSTESTS_HPP
#define _VKTNONROBUSTBUFFERACCESSTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
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
 * \brief Tests for non robust oob buffer access in unexecuted shder code
 *        paths.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace robustness
{

tcu::TestCaseGroup *createNonRobustBufferAccessTests(tcu::TestContext &);

} // namespace robustness
} // namespace vkt

#endif // _VKTNONROBUSTBUFFERACCESSTESTS_HPP
