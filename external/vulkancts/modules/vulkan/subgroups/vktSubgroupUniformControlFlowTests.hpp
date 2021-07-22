#ifndef _VKTSUBGROUPUNIFORMCONTROLFLOWTESTS_HPP
#define _VKTSUBGROUPUNIFORMCONTROLFLOWTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Google LLC
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief VK_KHR_shader_subgroup_uniform_control_flow tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace subgroups
{

tcu::TestCaseGroup*	createSubgroupUniformControlFlowTests (tcu::TestContext& testCtx);

} // subgroups
} // vkt

#endif // _VKTSUBGROUPUNIFORMCONTROLFLOWTESTS_HPP
