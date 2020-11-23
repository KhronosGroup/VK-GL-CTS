/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 * Copyright (c) 2019 NVIDIA Corporation
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
 * \brief Fragment shader interlock tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShaderInterlockTests.hpp"
#include "vktFragmentShaderInterlockBasic.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace FragmentShaderInterlock
{

namespace
{

static void createChildren (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx		= group->getTestContext();

	group->addChild(createBasicTests(testCtx));
}

static void cleanupGroup (tcu::TestCaseGroup* group)
{
	DE_UNREF(group);
	// Destroy singleton objects.
	cleanupDevice();
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "fragment_shader_interlock", "Fragment shader interlock tests", createChildren, cleanupGroup);
}

} // FragmentShaderInterlock
} // vkt
