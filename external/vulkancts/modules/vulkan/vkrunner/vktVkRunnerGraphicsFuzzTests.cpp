/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2018 Google LLC
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
 * \brief GraphicsFuzz tests
 *//*--------------------------------------------------------------------*/

#include <vkrunner/vkrunner.h>

#include "vktVkRunnerGraphicsFuzzTests.hpp"
#include "vktVkRunnerTestCase.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace vkrunner
{
namespace
{

void createVkRunnerTests (tcu::TestCaseGroup* graphicsFuzzTests)
{
	tcu::TestContext&	testCtx	= graphicsFuzzTests->getTestContext();

	static const struct
	{
		const char *filename, *name, *description;
	} tests[] =
	{
		{	"continue-and-merge.shader_test",			"continue-and-merge",			"A fragment shader with two nested loops"										},
		{	"control-flow-switch.shader_test",			"control-flow-switch",			"A fragment shader with somewhat complex control flow and a switch"				},
		{	"dead-struct-init.shader_test",				"dead-struct-init",				"A fragment shader that uses struct initializers"								},
		{	"mat-array-deep-control-flow.shader_test",	"mat-array-deep-control-flow",	"A fragment shader that uses an array of matrices and has deep control flow"	},
		{	"mat-array-distance.shader_test",			"mat-array-distance",			"A fragment shader that uses an array of matrices and distance"					},
		{	"pow-vec4.shader_test",						"pow-vec4",						"A fragment shader that uses pow"												},
		{	"swizzle-struct-init-min.shader_test",		"swizzle-struct-init-min",		"A fragment shader that uses vector swizzles, struct initializers, and min"		},
		{	"while-inside-switch.shader_test",			"while-inside-switch",			"A fragment shader that uses a while loop inside a switch"						},
	};

	for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++)
	{
		/* shader_test files are saved in <path>/external/vulkancts/data/vulkan/vkrunner/<categoryname>/ */
		VkRunnerTestCase *testCase = new VkRunnerTestCase(testCtx,
														  "graphicsfuzz",
														  tests[i].filename,
														  tests[i].name,
														  tests[i].description);
		/* Need to call getShaders() manually to detect any issue in the
		 * shader test file, like invalid test commands or the file doesn't exist.
		 */
		testCase->getShaders();
		graphicsFuzzTests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createGraphicsFuzzTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "graphicsfuzz", "VkRunner GraphicsFuzz Tests", createVkRunnerTests);
}

} // vkrunner
} // vkt
