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
#include "vktPipelineMiscTests.hpp"

namespace vkt
{
namespace pipeline
{
namespace
{

enum AmberFeatureBits
{
	AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS	= (1 <<	0),
	AMBER_FEATURE_TESSELATION_SHADER					= (1 <<	1),
	AMBER_FEATURE_GEOMETRY_SHADER						= (1 <<	2),
};

using AmberFeatureFlags = deUint32;

std::vector<std::string> getFeatureList (AmberFeatureFlags flags)
{
	std::vector<std::string> requirements;

	if (flags & AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS)
		requirements.push_back("Features.vertexPipelineStoresAndAtomics");

	if (flags & AMBER_FEATURE_TESSELATION_SHADER)
		requirements.push_back("Features.tessellationShader");

	if (flags & AMBER_FEATURE_GEOMETRY_SHADER)
		requirements.push_back("Features.geometryShader");

	return requirements;
}

void addTests (tcu::TestCaseGroup* tests, const char* data_dir)
{
	tcu::TestContext& testCtx = tests->getTestContext();

	// Shader test files are saved in <path>/external/vulkancts/data/vulkan/amber/<data_dir>/<basename>.amber
	struct Case {
		const char*			basename;
		const char*			description;
		AmberFeatureFlags	flags;
	};

	const Case cases[] =
	{
		{
			"position_to_ssbo",
			"Write position data into ssbo using only the vertex shader in a pipeline",
			(AMBER_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS),
		},
		{
			"primitive_id_from_tess",
			"Read primitive id from tessellation shaders without a geometry shader",
			(AMBER_FEATURE_TESSELATION_SHADER | AMBER_FEATURE_GEOMETRY_SHADER),
		},
	};

	for (unsigned i = 0; i < DE_LENGTH_OF_ARRAY(cases) ; ++i)
	{
		std::string					file			= std::string(cases[i].basename) + ".amber";
		std::vector<std::string>	requirements	= getFeatureList(cases[i].flags);
		cts_amber::AmberTestCase	*testCase		= cts_amber::createAmberTestCase(testCtx, cases[i].basename, cases[i].description, data_dir, file, requirements);

		tests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createMiscTests (tcu::TestContext& testCtx)
{
	// Location of the Amber script files under the data/vulkan/amber source tree.
	const char* data_dir = "pipeline";
	return createTestGroup(testCtx, "misc", "Miscellaneous pipeline tests", addTests, data_dir);
}

} // SpirVAssembly
} // vkt
