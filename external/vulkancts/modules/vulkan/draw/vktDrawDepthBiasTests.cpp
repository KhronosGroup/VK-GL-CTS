/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021-2022 Google Inc.
 * Copyright (c) 2021-2022 The Khronos Group Inc.
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
 * \brief Depth bias tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawDepthBiasTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "amber/vktAmberTestCase.hpp"

#include "tcuTestCase.hpp"

#include <string>

namespace vkt
{
namespace Draw
{
namespace
{

void createTests (tcu::TestCaseGroup* testGroup)
{
	tcu::TestContext&			testCtx		= testGroup->getTestContext();
	static const char			dataDir[]	= "draw/depth_bias";

	struct depthBiasCase
	{
		std::string					testName;
		std::vector<std::string>	testRequirements;
	};

	static const depthBiasCase	cases[] =
	{
		{ "depth_bias_triangle_list_fill",		{} },
		{ "depth_bias_triangle_list_line",		{ "Features.fillModeNonSolid" } },
		{ "depth_bias_triangle_list_point",		{ "Features.fillModeNonSolid" } },
		{ "depth_bias_patch_list_tri_fill",		{ "Features.tessellationShader" } },
		{ "depth_bias_patch_list_tri_line",		{ "Features.tessellationShader", "Features.fillModeNonSolid" } },
		{ "depth_bias_patch_list_tri_point",	{ "Features.tessellationShader", "Features.fillModeNonSolid" } }
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		std::vector<std::string>	requirements	= cases[i].testRequirements;
		const std::string			fileName		= cases[i].testName + ".amber";
		cts_amber::AmberTestCase*	testCase		= cts_amber::createAmberTestCase(testCtx, cases[i].testName.c_str(), "", dataDir, fileName, requirements);
		testGroup->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createDepthBiasTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "depth_bias", "Depth bias tests", createTests);
}

}	// Draw
}	// vkt
