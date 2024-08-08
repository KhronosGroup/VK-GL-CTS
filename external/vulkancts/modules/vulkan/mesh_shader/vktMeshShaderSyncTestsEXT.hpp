#ifndef _VKTMESHSHADERSYNCTESTSEXT_HPP
#define _VKTMESHSHADERSYNCTESTSEXT_HPP
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
 * \brief Mesh Shader Synchronization Tests for VK_EXT_mesh_shader
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace MeshShader
{
tcu::TestCaseGroup *createMeshShaderSyncTestsEXT(tcu::TestContext &testCtx);
} // namespace MeshShader
} // namespace vkt

#endif // _VKTMESHSHADERSYNCTESTSEXT_HPP
