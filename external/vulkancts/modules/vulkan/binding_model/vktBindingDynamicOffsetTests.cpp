/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
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
 * \brief Dynamic offset tests.
 *//*--------------------------------------------------------------------*/

#include "vktBindingDynamicOffsetTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace BindingModel
{
namespace
{

void populateDynamicOffsetTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(cts_amber::createAmberTestCase(testCtx, "shader_reuse_differing_layout_compute", "", "binding_model/dynamic_offset", "shader_reuse_differing_layout_compute.amber"));
	group->addChild(cts_amber::createAmberTestCase(testCtx, "shader_reuse_differing_layout_graphics", "", "binding_model/dynamic_offset", "shader_reuse_differing_layout_graphics.amber"));
}

} // anonymous

tcu::TestCaseGroup* createDynamicOffsetTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "dynamic_offset", "Dynamic offset tests.", populateDynamicOffsetTests);
}

} // BindingModel
} // vkt
