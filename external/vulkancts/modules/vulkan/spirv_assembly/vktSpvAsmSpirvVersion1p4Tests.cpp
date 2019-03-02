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
 * \brief Test new features in SPIR-V 1.4.
 *//*--------------------------------------------------------------------*/

#include <string>
#include <amber/amber.h>

#include "tcuDefs.hpp"

#include "vkDefs.hpp"
#include "vktAmberTestCase.hpp"
#include "vktAmberTestCaseUtil.hpp"
#include "vktSpvAsmSpirvVersion1p4Tests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

struct Case
{
	Case(const char* b, const char* d) : basename(b), description(d) { }
	const char *basename;
	const char *description;
};
struct CaseGroup
{
	CaseGroup(const char* the_data_dir, const char* the_subdir) : data_dir(the_data_dir), subdir(the_subdir) { }
	void add(const char* basename, const char* description) { cases.push_back(Case(basename, description)); }

	const char* data_dir;
	const char* subdir;
	std::vector<Case> cases;
};


void addTestsForAmberFiles (tcu::TestCaseGroup* tests, CaseGroup group)
{
	tcu::TestContext& testCtx = tests->getTestContext();
	const std::string data_dir(group.data_dir);
	const std::string subdir(group.subdir);
	const std::string category = data_dir + "/" + subdir;
	std::vector<Case> cases(group.cases);
	vk::SpirVAsmBuildOptions asm_options(VK_MAKE_VERSION(1, 1, 0), vk::SPIRV_VERSION_1_4);
	asm_options.supports_VK_KHR_spirv_1_4 = true;

	for (unsigned i = 0; i < cases.size() ; ++i)
	{

		const std::string file = std::string(cases[i].basename) + ".amber";
		cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(testCtx,
																			cases[i].basename,
																			cases[i].description,
																			category.c_str(),
																			file);
		DE_ASSERT(testCase != DE_NULL);
		// Add Vulkan extension requirements.
		// VK_KHR_spirv_1_4 requires Vulkan 1.1, which includes many common extensions.
		// So for, example, these tests never have to request VK_KHR_storage_buffer_storage_class,
		// or VK_KHR_variable_pointers since those extensions were promoted to core features
		// in Vulkan 1.1.  Note that feature bits may still be optional.
		testCase->addRequiredDeviceExtension("VK_KHR_spirv_1_4");
		// The tests often use StorageBuffer storage class.
		// We do not have to request VK_KHR_storage_buffer_storage_class because that extension
		// is about enabling use of SPV_KHR_storage_buffer_storage_class.  But SPIR-V 1.4 allows
		// use of StorageBuffer storage class without any further declarations of extensions
		// or capabilities.  This will also hold for tests that use features introduced by
		// extensions folded into SPIR-V 1.4 or earlier, and which don't require extra capabilities
		// to be enabled by Vulkan.  Other examples are functionality in SPV_GOOGLE_decorate_string,
		// SPV_GOOGLE_hlsl_functionality1, and SPV_KHR_no_integer_wrap_decoration.

		testCase->setSpirVAsmBuildOptions(asm_options);
		tests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createSpirvVersion1p4Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> spirv1p4Tests(new tcu::TestCaseGroup(testCtx, "spirv1p4", "SPIR-V 1.4 new features"));

	// Location of the Amber script files under the data/vulkan/amber source tree.
	const char* data_dir = "spirv_assembly/instruction/spirv1p4";

	CaseGroup group(data_dir, "opcopylogical");
	group.add("different_matrix_layout","different matrix layout");
	group.add("different_matrix_strides","different matrix strides");
	group.add("nested_arrays_different_inner_stride","nested_arrays_different_inner_stride");
	group.add("nested_arrays_different_outer_stride","nested_arrays_different_inner_stride");
	group.add("nested_arrays_different_strides","nested_arrays_different_strides");
	group.add("same_array_two_ids","same array two ids");
	group.add("same_struct_two_ids","same struct two ids");
	group.add("ssbo_to_ubo","ssbo_to_ubo");
	group.add("two_arrays_different_stride_1","two_arrays_different_stride_1");
	group.add("two_arrays_different_stride_2","two_arrays_different_stride_2");
	group.add("ubo_to_ssbo","ubo_to_ssbo");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "opcopylogical", "OpCopyLogical", addTestsForAmberFiles, group));

	return spirv1p4Tests.release();
}

} // SpirVAssembly
} // vkt
