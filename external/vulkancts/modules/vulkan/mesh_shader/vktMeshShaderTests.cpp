/*-------------------------------------------------------------------------
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
 * \brief Mesh Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderTests.hpp"
#include "vktMeshShaderSmokeTests.hpp"
#include "vktMeshShaderSyncTests.hpp"
#include "vktMeshShaderApiTests.hpp"
#include "vktMeshShaderPropertyTests.hpp"
#include "vktMeshShaderBuiltinTests.hpp"
#include "vktMeshShaderMiscTests.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace MeshShader
{

namespace
{
using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
}

tcu::TestCaseGroup*	createTests	(tcu::TestContext& testCtx)
{
	GroupPtr mainGroup	(new tcu::TestCaseGroup(testCtx, "mesh_shader", "Mesh Shader Tests"));
	GroupPtr nvGroup	(new tcu::TestCaseGroup(testCtx, "nv", "Tests for VK_NV_mesh_shader"));

	nvGroup->addChild(createMeshShaderSmokeTests(testCtx));
	nvGroup->addChild(createMeshShaderApiTests(testCtx));
	nvGroup->addChild(createMeshShaderSyncTests(testCtx));
	nvGroup->addChild(createMeshShaderPropertyTests(testCtx));
	nvGroup->addChild(createMeshShaderBuiltinTests(testCtx));
	nvGroup->addChild(createMeshShaderMiscTests(testCtx));
	nvGroup->addChild(createMeshShaderInOutTests(testCtx));

	mainGroup->addChild(nvGroup.release());
	return mainGroup.release();
}

} // MeshShader
} // vkt
