#ifndef _VKTMESHSHADERPROVOKINGVERTEXTESTSEXT_HPP
#define _VKTMESHSHADERPROVOKINGVERTEXTESTSEXT_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Tests mixing VK_EXT_mesh_shader and VK_EXT_provoking_vertex
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace MeshShader
{
tcu::TestCaseGroup *createMeshShaderProvokingVertexTestsEXT(tcu::TestContext &testCtx);
} // namespace MeshShader
} // namespace vkt

#endif // _VKTMESHSHADERPROVOKINGVERTEXTESTSEXT_HPP
