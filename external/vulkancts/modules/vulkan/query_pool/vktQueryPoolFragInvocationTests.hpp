#ifndef _VKTQUERYPOOLFRAGINVOCATIONTESTS_HPP
#define _VKTQUERYPOOLFRAGINVOCATIONTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation.
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
 * \brief Vulkan Fragment Shader Invocation and Sample Cound Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

namespace vkt
{
namespace QueryPool
{

tcu::TestCaseGroup* createFragInvocationTests (tcu::TestContext& testContext);

} // QueryPool
} // vkt

#endif // _VKTQUERYPOOLFRAGINVOCATIONTESTS_HPP
