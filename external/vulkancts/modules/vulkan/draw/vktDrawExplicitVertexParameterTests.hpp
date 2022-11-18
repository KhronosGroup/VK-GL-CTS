#ifndef _VKTDRAWEXPLICITVERTEXPARAMETERTESTS_HPP
#define _VKTDRAWEXPLICITVERTEXPARAMETERTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation
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
 * \brief VK_AMD_shader_explicit_vertex_parameter tests
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktDrawGroupParams.hpp"

namespace vkt
{
namespace Draw
{

tcu::TestCaseGroup*	createExplicitVertexParameterTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams);

} // Draw
} // vkt

#endif // _VKTDRAWEXPLICITVERTEXPARAMETERTESTS_HPP
