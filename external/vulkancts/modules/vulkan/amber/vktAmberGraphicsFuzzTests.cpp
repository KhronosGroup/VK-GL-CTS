/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2018 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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

#include "vktAmberGraphicsFuzzTests.hpp"
#include "vktAmberTestCaseUtil.hpp"

namespace vkt
{
namespace cts_amber
{
namespace
{

void createAmberTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	static const struct
	{
		const std::string	filename;
		const char*			name;
		const char*			description;
	}
	tests[] =
	{
		{	"continue-and-merge.amber",				"continue-and-merge",			"A fragment shader with two nested loops"										},
		{	"control-flow-switch.amber",			"control-flow-switch",			"A fragment shader with somewhat complex control flow and a switch"				},
		{	"dead-struct-init.amber",				"dead-struct-init",				"A fragment shader that uses struct initializers"								},
		{	"fragcoord-control-flow.amber",			"fragcoord-control-flow",		"A fragment shader that uses FragCoord and somewhat complex control flow"		},
		{	"fragcoord-control-flow-2.amber",		"fragcoord-control-flow-2",		"A fragment shader that uses FragCoord and somewhat complex control flow"		},
		{	"mat-array-deep-control-flow.amber",	"mat-array-deep-control-flow",	"A fragment shader that uses an array of matrices and has deep control flow"	},
		{	"mat-array-distance.amber",				"mat-array-distance",			"A fragment shader that uses an array of matrices and distance"					},
		{	"pow-vec4.amber",						"pow-vec4",						"A fragment shader that uses pow"												},
		{	"swizzle-struct-init-min.amber",		"swizzle-struct-init-min",		"A fragment shader that uses vector swizzles, struct initializers, and min"		},
		{	"while-inside-switch.amber",			"while-inside-switch",			"A fragment shader that uses a while loop inside a switch"						},
	};

	for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++)
		group->addChild(createAmberTestCase(testCtx, tests[i].name, tests[i].description, "graphicsfuzz", tests[i].filename));
}

} // anonymous

tcu::TestCaseGroup* createGraphicsFuzzTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "graphicsfuzz", "Amber GraphicsFuzz Tests", createAmberTests);
}

} // cts_amber
} // vkt
