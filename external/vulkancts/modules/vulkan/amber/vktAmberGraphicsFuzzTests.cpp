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

#include "vktTestGroupUtil.hpp"
#include "vktAmberGraphicsFuzzTests.hpp"
#include "vktAmberTestCase.hpp"

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
		{	"barrier-in-loop-with-break.amber",				"barrier-in-loop-with-break",			"A compute shader with a barrier in a loop with a break"								},
		{	"color-write-in-loop.amber",					"color-write-in-loop",					"A fragment shader that writes to color in a loop"										},
		{	"continue-and-merge.amber",						"continue-and-merge",					"A fragment shader with two nested loops"												},
		{	"control-flow-switch.amber",					"control-flow-switch",					"A fragment shader with somewhat complex control flow and a switch"						},
		{	"dead-barriers-in-loops.amber",					"dead-barriers-in-loops",				"A compute shader with dead barriers"													},
		{	"dead-struct-init.amber",						"dead-struct-init",						"A fragment shader that uses struct initializers"										},
		{	"do-while-loop-in-conditionals.amber",			"do-while-loop-in-conditionals",		"A fragment shader with do-while loop in conditional nest"								},
		{	"early-return-and-barrier.amber",				"early-return-and-barrier",				"A compute shader with an early return and a barrier"									},
		{	"for-condition-always-false.amber",				"for-condition-always-false",			"A fragment shader that uses a for loop with condition always false"					},
		{	"fragcoord-control-flow.amber",					"fragcoord-control-flow",				"A fragment shader that uses FragCoord and somewhat complex control flow"				},
		{	"fragcoord-control-flow-2.amber",				"fragcoord-control-flow-2",				"A fragment shader that uses FragCoord and somewhat complex control flow"				},
		{	"if-and-switch.amber",							"if-and-switch",						"A fragment shader with a switch and some data flow"									},
		{	"mat-array-deep-control-flow.amber",			"mat-array-deep-control-flow",			"A fragment shader that uses an array of matrices and has deep control flow"			},
		{	"mat-array-distance.amber",						"mat-array-distance",					"A fragment shader that uses an array of matrices and distance"							},
		{	"matrices-and-return-in-loop.amber",			"matrices-and-return-in-loop",			"A fragment shader with matrices and a return in a loop"								},
		{	"nested-ifs-and-return-in-for-loop.amber",		"nested-ifs-and-return-in-for-loop",	"A fragment shader with return in nest of ifs, inside loop"								},
		{	"pow-vec4.amber",								"pow-vec4",								"A fragment shader that uses pow"														},
		{	"return-in-loop-in-function.amber",				"return-in-loop-in-function",			"A fragment shader with early return from loop in function"								},
		{	"struct-used-as-temporary.amber",				"struct-used-as-temporary",				"A fragment shader that uses a temporary struct variable"								},
		{	"swizzle-struct-init-min.amber",				"swizzle-struct-init-min",				"A fragment shader that uses vector swizzles, struct initializers, and min"				},
		{	"unreachable-barrier-in-loops.amber",			"unreachable-barrier-in-loops",			"A compute shader with an unreachable barrier in a loop nest"							},
		{	"unreachable-loops.amber",						"unreachable-loops",					"Fragment shader that writes red despite unreachable loops"								},
		{	"unreachable-loops-in-switch.amber",			"unreachable-loops-in-switch",			"A fragment shader with unreachable loops in a switch"									},
		{	"while-inside-switch.amber",					"while-inside-switch",					"A fragment shader that uses a while loop inside a switch"								},
		{	"write-before-break.amber",						"write-before-break",					"Fragment shader that writes red before loop break"										},
		{	"write-red-after-search.amber",					"write-red-after-search",				"A fragment shader performing a search computation, then writing red regardless"		},
		{	"write-red-in-loop-nest.amber",					"write-red-in-loop-nest",				"A fragment shader that writes red in a nest of loops"									},
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
