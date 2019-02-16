/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Intel Corporation
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
 * \brief Functional tests using vkrunner
 *//*--------------------------------------------------------------------*/

#include <vkrunner/vkrunner.h>

#include "vktVkRunnerExampleTests.hpp"
#include "vktVkRunnerTestCase.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace vkrunner
{
namespace
{

void createVkRunnerTests (tcu::TestCaseGroup* vkRunnerTests)
{
	tcu::TestContext&	testCtx	= vkRunnerTests->getTestContext();

	static const struct
	{
		const char *filename, *name, *description;
	} tests[] =
	{
		{ "spirv.shader_test", "spirv", "Example test using a SPIR-V shaders in text format" },
		{ "ubo.shader_test", "ubo", "Example test setting values in a UBO" },
		{ "vertex-data.shader_test", "vertex-data", "Example test using a vertex data section" },
	};

	for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++)
	{
		/* shader_test files are saved in <path>/external/vulkancts/data/vulkan/vkrunner/<categoryname>/ */
		VkRunnerTestCase *testCase = new VkRunnerTestCase(testCtx,
														  "example",
														  tests[i].filename,
														  tests[i].name,
														  tests[i].description);
		/* Need to call getShaders() manually to detect any issue in the
		 * shader test file, like invalid test commands or the file doesn't exist.
		 */
		testCase->getShaders();
		vkRunnerTests->addChild(testCase);
	}

	// Add some tests of the sqrt function using the templating mechanism
	for (int i = 1; i <= 8; i++)
	{
		std::stringstream testName;
		testName << "sqrt_" << i;
		VkRunnerTestCase *testCase = new VkRunnerTestCase(testCtx,
														  "example",
														  "sqrt.shader_test",
														  testName.str().c_str(),
														  "Example test using the templating mechanism");
		std::stringstream inputString;
		inputString << (i * i);
		std::stringstream outputString;
		outputString << i;
		testCase->addTokenReplacement("<INPUT>", inputString.str().c_str());
		testCase->addTokenReplacement("<OUTPUT>", outputString.str().c_str());
		/* Call getShaders() after doing the token
		 * replacements in the shader test. Otherwise, VkRunner will fail when found
		 * unknown commands or invalid sentences when processing the shader test file.
		 */
		testCase->getShaders();
		vkRunnerTests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "vkrunner-example", "VkRunner Tests", createVkRunnerTests);
}

} // vkrunner
} // vkt
