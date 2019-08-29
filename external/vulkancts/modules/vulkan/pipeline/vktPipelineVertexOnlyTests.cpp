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
 * \brief Tests using only vertex shader in a graphics pipeline
 *//*--------------------------------------------------------------------*/

#include <string>

#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktPipelineVertexOnlyTests.hpp"

namespace vkt
{
namespace pipeline
{
namespace
{

void addTests (tcu::TestCaseGroup* tests, const char* data_dir)
{
	tcu::TestContext& testCtx = tests->getTestContext();

	// Shader test files are saved in <path>/external/vulkancts/data/vulkan/amber/<data_dir>/<basename>.amber
	struct Case {
		const char* basename;
		const char* description;
	};
	const Case cases[] =
	{
		{ "position_to_ssbo", "Write position data into ssbo" }
	};

	for (unsigned i = 0; i < DE_LENGTH_OF_ARRAY(cases) ; ++i)
	{
		std::string					file			= std::string(cases[i].basename) + ".amber";
		std::vector<std::string>	requirements	= std::vector<std::string>(1, "Features.vertexPipelineStoresAndAtomics");
		cts_amber::AmberTestCase	*testCase		= cts_amber::createAmberTestCase(testCtx, cases[i].basename, cases[i].description, data_dir, file, requirements);

		tests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createVertexOnlyTests (tcu::TestContext& testCtx)
{
	// Location of the Amber script files under the data/vulkan/amber source tree.
	const char* data_dir = "pipeline/vertex_only";
	return createTestGroup(testCtx, "vertex_only", "Tests using only vertex shader in a pipeline", addTests, data_dir);
}

} // SpirVAssembly
} // vkt
