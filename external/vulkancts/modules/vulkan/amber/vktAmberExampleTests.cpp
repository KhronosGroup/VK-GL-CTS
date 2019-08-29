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

#include "vktTestGroupUtil.hpp"
#include "vktAmberExampleTests.hpp"
#include "vktAmberTestCase.hpp"

namespace vkt
{
namespace cts_amber
{
namespace
{

void createAmberTests (tcu::TestCaseGroup* tests)
{
	tcu::TestContext& testCtx = tests->getTestContext();

	tests->addChild(createAmberTestCase(testCtx,				// tcu::TestContext		testCtx
										"clear",				// const char*			name
										"Example clear test",	// const char*			description
										"example",				// const char*			category
										"clear.amber"));		// const std::string&	filename
}

} // anonymous

tcu::TestCaseGroup* createExampleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "amber-example", "Amber Tests", createAmberTests);
}

} // cts_amber
} // vkt
