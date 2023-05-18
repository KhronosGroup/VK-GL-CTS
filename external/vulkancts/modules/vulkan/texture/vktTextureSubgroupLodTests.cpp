/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
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
 * \brief Subgroup LOD tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureSubgroupLodTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace texture
{
namespace
{

void populateSubgroupLodTests (tcu::TestCaseGroup* group)
{
#ifndef CTS_USES_VULKANSC
	tcu::TestContext&			testCtx			= group->getTestContext();
	cts_amber::AmberTestCase*	testCaseLod		= cts_amber::createAmberTestCase(testCtx, "texturelod", "", "texture/subgroup_lod", "texture_lod.amber");
	cts_amber::AmberTestCase*	testCaseGrad	= cts_amber::createAmberTestCase(testCtx, "texturegrad", "", "texture/subgroup_lod", "texture_grad.amber");
	cts_amber::AmberTestCase*	testCaseFetch	= cts_amber::createAmberTestCase(testCtx, "texelfetch", "", "texture/subgroup_lod", "texel_fetch.amber");

	group->addChild(testCaseLod);
	group->addChild(testCaseGrad);
	group->addChild(testCaseFetch);
#else
	DE_UNREF(group);
#endif
}

} // anonymous

tcu::TestCaseGroup* createTextureSubgroupLodTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "subgroup_lod", "Texture subgroup LOD tests.", populateSubgroupLodTests);
}

} // texture
} // vkt
