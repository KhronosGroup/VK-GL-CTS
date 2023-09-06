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
 *--------------------------------------------------------------------*/

#include <string>

#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktSpvAsmPtrAccessChainTests.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

void createTests (tcu::TestCaseGroup* tests, const char* data_dir)
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
		{ "workgroup", "OpPtrAccessChain with correct ArrayStride decoration" },
		{ "workgroup_no_stride", "OpPtrAccessChain with no ArrayStride decoration" },
		{ "workgroup_bad_stride", "OpPtrAccessChain with incorrect ArrayStride decoration" },
	};

	for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]) ; ++i)
	{
		std::string					file		= std::string(cases[i].basename) + ".amber";
		cts_amber::AmberTestCase	*testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].basename, cases[i].description, data_dir, file);
		testCase->addRequirement("VariablePointerFeatures.variablePointers");

		tests->addChild(testCase);
	}
#else
	DE_UNREF(tests);
	DE_UNREF(data_dir);
#endif
}

} // anonymous

tcu::TestCaseGroup* createPtrAccessChainGroup (tcu::TestContext& testCtx)
{
	// Location of the Amber script files under the data/vulkan/amber source tree.
	const char* data_dir = "spirv_assembly/instruction/compute/ptr_access_chain";
	return createTestGroup(testCtx, "ptr_access_chain", "OpPtrAccessChain edge cases", createTests, data_dir);
}

} // SpirVAssembly
} // vkt
