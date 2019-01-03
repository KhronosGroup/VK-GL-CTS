/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
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
 * \brief Functional tests using amber
 *//*--------------------------------------------------------------------*/

#include <amber/amber.h>

#include "vktAmberExampleTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace cts_amber
{
namespace
{

void createAmberTests (tcu::TestCaseGroup* tests)
{
	tcu::TestContext& testCtx = tests->getTestContext();

	// shader_test files are saved in <path>/external/vulkancts/data/vulkan/amber/<categoryname>/
	AmberTestCase *testCase = new AmberTestCase(testCtx, "clear", "Example clear test");

	// Make sure the input can be parsed before we use it.
	if (testCase->parse("example", "clear.amber"))
		tests->addChild(testCase);
	else
		delete testCase;
}

} // anonymous

tcu::TestCaseGroup* createExampleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "amber-example", "Amber Tests", createAmberTests);
}

} // cts_amber
} // vkt
