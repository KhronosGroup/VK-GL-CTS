/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Google LLC
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Test new features in VK_KHR_shader_terminate_invocation
 *//*--------------------------------------------------------------------*/

#include <string>
#include <vector>
#include <amber/amber.h>

#include "tcuDefs.hpp"

#include "vkDefs.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktSpvAsmTerminateInvocationTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

struct Case
{
	Case(const char* b, const char* d, bool v) : basename(b), description(d), spv1p3(v), requirements() { }
	Case(const char* b, const char* d, bool v, const std::vector<std::string>& e) : basename(b), description(d), spv1p3(v), requirements(e) { }
	const char *basename;
	const char *description;
	bool spv1p3;
	// Additional Vulkan requirements, if any.
	std::vector<std::string> requirements;
};
struct CaseGroup
{
	CaseGroup(const char* the_data_dir) : data_dir(the_data_dir) { }
	void add(const char* basename, const char* description, bool spv1p3)
	{
		cases.push_back(Case(basename, description, spv1p3));
	}
	void add(const char* basename, const char* description, bool spv1p3, const std::vector<std::string>& requirements)
	{
		cases.push_back(Case(basename, description, spv1p3, requirements));
	}

	const char* data_dir;
	std::vector<Case> cases;
};


void addTestsForAmberFiles (tcu::TestCaseGroup* tests, CaseGroup group)
{
	tcu::TestContext& testCtx = tests->getTestContext();
	const std::string data_dir(group.data_dir);
	const std::string category = data_dir;
	std::vector<Case> cases(group.cases);

	for (unsigned i = 0; i < cases.size() ; ++i)
	{
		deUint32 vulkan_version = cases[i].spv1p3 ? VK_MAKE_VERSION(1, 1, 0) : VK_MAKE_VERSION(1, 0, 0);
		vk::SpirvVersion spirv_version = cases[i].spv1p3 ? vk::SPIRV_VERSION_1_3 : vk::SPIRV_VERSION_1_0;
		vk::SpirVAsmBuildOptions asm_options(vulkan_version, spirv_version);

		const std::string file = std::string(cases[i].basename) + ".amber";
		cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(testCtx,
																			cases[i].basename,
																			cases[i].description,
																			category.c_str(),
																			file);
		DE_ASSERT(testCase != DE_NULL);
		testCase->addRequirement("VK_KHR_shader_terminate_invocation");
		const std::vector<std::string>& reqmts = cases[i].requirements;
		for (size_t r = 0; r < reqmts.size() ; ++r)
		{
			testCase->addRequirement(reqmts[r]);
		}

		testCase->setSpirVAsmBuildOptions(asm_options);
		tests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createTerminateInvocationGroup(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> terminateTests(new tcu::TestCaseGroup(testCtx, "terminate_invocation", "VK_KHR_shader_terminate_invocation tests"));

	const char* data_data = "spirv_assembly/instruction/terminate_invocation";

	std::vector<std::string> Stores;
	Stores.push_back("Features.fragmentStoresAndAtomics");

	std::vector<std::string> VarPtr;
	VarPtr.push_back("VariablePointerFeatures.variablePointersStorageBuffer");
	VarPtr.push_back("Features.fragmentStoresAndAtomics");

	std::vector<std::string> Vote;
	Vote.push_back("SubgroupSupportedOperations.vote");
	Vote.push_back("SubgroupSupportedStages.fragment");

	std::vector<std::string> Ballot;
	Ballot.push_back("SubgroupSupportedOperations.ballot");
	Ballot.push_back("SubgroupSupportedStages.fragment");

	CaseGroup group(data_data);
	group.add("no_output_write", "no write to after calling terminate invocation", false);
	group.add("no_output_write_before_terminate", "no write to output despite occurring before terminate invocation", false);
	group.add("no_ssbo_store", "no store to SSBO when it occurs after terminate invocation", false, Stores);
	group.add("no_ssbo_atomic", "no atomic update to SSBO when it occurs after terminate invocation", false, Stores);
	group.add("ssbo_store_before_terminate", "ssbo store commits when it occurs before terminate invocation", false, Stores);
	group.add("no_image_store", "no image write when it occurs after terminate invocation", false, Stores);
	group.add("no_image_atomic", "no image atomic updates when it occurs after terminate invocation", false, Stores);
	group.add("no_null_pointer_load", "null pointer should not be accessed by a load in a terminated invocation", false, VarPtr);
	group.add("no_null_pointer_store", "null pointer should not be accessed by a store in a terminated invocation", false, VarPtr);
	group.add("no_out_of_bounds_load", "out of bounds pointer should not be accessed by a load in a terminated invocation", false, VarPtr);
	group.add("no_out_of_bounds_store", "out of bounds pointer should not be accessed by a store in a terminated invocation", false, VarPtr);
	group.add("no_out_of_bounds_atomic", "out of bounds pointer should not be accessed by an atomic in a terminated invocation", false, VarPtr);
	group.add("terminate_loop", "\"inifinite\" loop that calls terminate invocation", false);
	group.add("subgroup_ballot", "checks that terminated invocations don't participate in the ballot", true, Ballot);
	group.add("subgroup_vote", "checks that a subgroup all does not include any terminated invocations", true, Vote);
	terminateTests->addChild(createTestGroup(testCtx, "terminate", "Terminate Invocation", addTestsForAmberFiles, group));

	return terminateTests.release();
}

} // SpirVAssembly
} // vkt
