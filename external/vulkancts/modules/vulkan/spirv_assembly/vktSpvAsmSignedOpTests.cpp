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
 * \brief SPIR-V signed instruction tests
 *//*--------------------------------------------------------------------*/

#include <string>

#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktSpvAsmSignedIntCompareTests.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

void createSignedOpTests (tcu::TestCaseGroup* tests, const char* data_dir)
{
#ifndef CTS_USES_VULKANSC
	tcu::TestContext& testCtx = tests->getTestContext();

	// Shader test files are saved in <path>/external/vulkancts/data/vulkan/amber/<data_dir>/<basename>.amber
	struct Case {
		const char* basename;
		const char* description;
	};
	const Case cases[] =
	{
		{ "glsl_int_findumsb", "32bit signed int with FindUMsb" },
		{ "glsl_int_uclamp", "32bit signed int with UClamp" },
		{ "glsl_int_umax", "32bit signed int with UMax" },
		{ "glsl_int_umin", "32bit signed int with UMin" },
		{ "glsl_uint_findsmsb", "32bit unsigned int with FindSMsb" },
		{ "glsl_uint_sabs", "32bit unsigned int with SAbs" },
		{ "glsl_uint_sclamp", "32bit unsigned int with SClamp" },
		{ "glsl_uint_smax", "32bit unsigned int with SMax" },
		{ "glsl_uint_smin", "32bit unsigned int with SMin" },
		{ "glsl_uint_ssign", "32bit unsigned int with SSign" },
		{ "int_atomicumax", "32bit unsigned int with UMax" },
		{ "int_atomicumin", "32bit unsigned int with UMin" },
		{ "int_ugreaterthan", "32bit unsigned int with UGreaterThanEqual" },
		{ "int_ugreaterthanequal", "32bit unsigned int with UGreaterThanEqual" },
		{ "int_ulessthan", "32bit unsigned int with ULessThan" },
		{ "int_ulessthanequal", "32bit unsigned int with ULessThanEqual" },
		{ "uint_atomicsmax", "32bit unsigned int with SMax" },
		{ "uint_atomicsmin", "32bit unsigned int with SMin" },
		{ "uint_sdiv", "32bit unsigned int with UMax" },
		{ "uint_smulextended", "32bit unsigned int with SMulExtended" },
		{ "uint_snegate", "32bit unsigned int with SNegate" },
	};

	for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]) ; ++i)
	{
		std::string					file		= std::string(cases[i].basename) + ".amber";
		cts_amber::AmberTestCase	*testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].basename, cases[i].description, data_dir, file);

		tests->addChild(testCase);
	}
#else
	DE_UNREF(tests);
	DE_UNREF(data_dir);
#endif
}

} // anonymous

tcu::TestCaseGroup* createSignedOpTestsGroup (tcu::TestContext& testCtx)
{
	// Location of the Amber script files under the data/vulkan/amber source tree.
	const char* data_dir = "spirv_assembly/instruction/compute/signed_op";
	return createTestGroup(testCtx, "signed_op", "Signed op over uint values", createSignedOpTests, data_dir);
}

} // SpirVAssembly
} // vkt
