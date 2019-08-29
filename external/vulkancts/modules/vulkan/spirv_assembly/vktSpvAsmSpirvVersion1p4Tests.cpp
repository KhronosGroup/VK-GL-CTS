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
#include <vector>
#include <amber/amber.h>

#include "tcuDefs.hpp"

#include "vkDefs.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
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
	Case(const char* b, const char* d) : basename(b), description(d), requirements() { }
	Case(const char* b, const char* d, const std::vector<std::string>& e) : basename(b), description(d), requirements(e) { }
	const char *basename;
	const char *description;
	// Additional Vulkan requirements, if any.
	std::vector<std::string> requirements;
};
struct CaseGroup
{
	CaseGroup(const char* the_data_dir, const char* the_subdir) : data_dir(the_data_dir), subdir(the_subdir) { }
	void add(const char* basename, const char* description)
	{
		cases.push_back(Case(basename, description));
	}
	void add(const char* basename, const char* description, const std::vector<std::string>& requirements)
	{
		cases.push_back(Case(basename, description, requirements));
	}

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
		testCase->addRequirement("VK_KHR_spirv_1_4");
		// The tests often use StorageBuffer storage class.
		// We do not have to request VK_KHR_storage_buffer_storage_class because that extension
		// is about enabling use of SPV_KHR_storage_buffer_storage_class.  But SPIR-V 1.4 allows
		// use of StorageBuffer storage class without any further declarations of extensions
		// or capabilities.  This will also hold for tests that use features introduced by
		// extensions folded into SPIR-V 1.4 or earlier, and which don't require extra capabilities
		// to be enabled by Vulkan.  Other examples are functionality in SPV_GOOGLE_decorate_string,
		// SPV_GOOGLE_hlsl_functionality1, and SPV_KHR_no_integer_wrap_decoration.
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

tcu::TestCaseGroup* createSpirvVersion1p4Group (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> spirv1p4Tests(new tcu::TestCaseGroup(testCtx, "spirv1p4", "SPIR-V 1.4 new features"));

	// Location of the Amber script files under the data/vulkan/amber source tree.
	const char* data_dir = "spirv_assembly/instruction/spirv1p4";

	// Set up features used for various tests.
	std::vector<std::string> Geom;
	Geom.push_back("Features.geometryShader");

	std::vector<std::string> Tess;
	Tess.push_back("Features.tessellationShader");

	std::vector<std::string> Varptr_ssbo;
	Varptr_ssbo.push_back("VariablePointerFeatures.variablePointersStorageBuffer");

	std::vector<std::string> Varptr_full = Varptr_ssbo;
	Varptr_full.push_back("VariablePointerFeatures.variablePointers");

	std::vector<std::string> Int16;
	Int16.push_back("Features.shaderInt16");

	std::vector<std::string> Int64;
	Int64.push_back("Features.shaderInt64");

	// Define test groups

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

	group = CaseGroup(data_dir, "opptrdiff");
	group.add("ssbo_comparisons_diff", "pointer diff within an SSBO", Varptr_ssbo);
	group.add("variable_pointers_vars_ssbo_2_diff", "pointer diff in SSBO with full VariablePointers", Varptr_full);
	group.add("variable_pointers_vars_ssbo_diff", "pointer diff in SSBO, stored in private var", Varptr_ssbo);
	group.add("variable_pointers_vars_wg_diff", "pointer diff in workgroup storage, stored in private var", Varptr_full);
	group.add("wg_comparisons_diff", "pointer diff in workgroup storage", Varptr_full);
	spirv1p4Tests->addChild(createTestGroup(testCtx, "opptrdiff", "OpPtrDiff", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "opptrequal");
	group.add("different_ssbos_equal", "ptr equal against different SSBO variables", Varptr_full);
	group.add("different_wgs_equal", "ptr equal against different WG variables", Varptr_full);
	group.add("null_comparisons_ssbo_equal", "ptr equal null in SSBO", Varptr_ssbo);
	group.add("null_comparisons_wg_equal", "ptr equal null in Workgrop", Varptr_full);
	group.add("ssbo_comparisons_equal", "ptr equal in SSBO", Varptr_ssbo);
	group.add("variable_pointers_ssbo_2_equal", "ptr equal in SSBO, store pointers in Function var", Varptr_full);
	group.add("variable_pointers_ssbo_equal", "ptr equal in SSBO", Varptr_ssbo);
	group.add("variable_pointers_vars_ssbo_equal", "ptr equal in SSBO, store pointers in Private var ", Varptr_ssbo);
	group.add("variable_pointers_vars_wg_equal", "ptr equal in Workgrop, store pointers in Private var", Varptr_full);
	group.add("variable_pointers_wg_equal", "ptr equal in Workgrop", Varptr_full);
	group.add("wg_comparisons_equal", "ptr equal in Workgrop", Varptr_full);
	spirv1p4Tests->addChild(createTestGroup(testCtx, "opptrequal", "OpPtrEqual", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "opptrnotequal");
	group.add("different_ssbos_not_equal", "ptr not equal against different SSBO variables", Varptr_full);
	group.add("different_wgs_not_equal", "ptr not equal against different WG variables", Varptr_full);
	group.add("null_comparisons_ssbo_not_equal", "ptr not equal null SSBO", Varptr_ssbo);
	group.add("null_comparisons_wg_not_equal", "ptr not equal null SSBO", Varptr_full);
	group.add("ssbo_comparisons_not_equal", "ptr not equal SSBO", Varptr_ssbo);
	group.add("variable_pointers_ssbo_2_not_equal", "ptr not equal SSBO, store pointer in Function var", Varptr_full);
	group.add("variable_pointers_ssbo_not_equal", "ptr not equal SSBO, pointer from function return", Varptr_ssbo);
	group.add("variable_pointers_vars_ssbo_not_equal", "ptr not equal SSBO, store pointer in Private var", Varptr_ssbo);
	group.add("variable_pointers_vars_wg_not_equal", "ptr not equal Workgroup, store pointer in Private var", Varptr_ssbo);
	group.add("variable_pointers_wg_not_equal", "ptr not equal Workgroup", Varptr_full);
	group.add("wg_comparisons_not_equal", "ptr not equal Workgroup", Varptr_full);
	spirv1p4Tests->addChild(createTestGroup(testCtx, "opptrnotequal", "OpPtrNotEqual", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "opcopymemory");
	group.add("different_alignments", "different alignments");
	group.add("no_source_access_operands", "no source access operands");
	group.add("no_target_access_operands", "no target access operands");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "opcopymemory", "OpCopyMemory 2 memory access operands", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "uniformid");
	group.add("partially_active_uniform_id","workgroup uniform load result at consumption, in nonuniform control flow");
	group.add("subgroup_cfg_uniform_id","subgroup uniform compare result inside control flow"); // Assumes subgroup size <= LocalSize of 8
	group.add("subgroup_uniform","subgroup uniform load result"); // Assumes subgroup size <= LocalSize 8
	group.add("workgroup_cfg_uniform_id","workgroup uniform compare result");
	group.add("workgroup_uniform","workgroup uniform load result");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "uniformid", "UniformId decoration", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "nonwritable");
	group.add("function_2_nonwritable", "NonWritable decorates Function variables");
	group.add("function_nonwritable", "NonWritable decorates 2 Function variables");
	group.add("non_main_function_nonwritable", "NonWritable decorates Function variable in non-entrypoint function");
	group.add("private_2_nonwritable", "NonWritable decorates Private variables");
	group.add("private_nonwritable", "NonWritable decorates 2 Private variables");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "nonwritable", "NonWritable decoration", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "entrypoint");
	group.add("comp_pc_entry_point", "push constant on compute shader entry point");
	group.add("comp_ssbo_entry_point", "SSBO on compute shader entry point");
	group.add("comp_ubo_entry_point", "UBO on compute shader entry point");
	group.add("comp_workgroup_entry_point", "Workgroup var on compute shader entry point");
	group.add("frag_pc_entry_point", "push constant on fragment shader entry point");
	group.add("frag_ssbo_entry_point", "SSBO on fragment shader entry point");
	group.add("frag_ubo_entry_point", "UBO on fragment shader entry point");
	group.add("geom_pc_entry_point", "push constant on geometry shader entry point", Geom);
	group.add("geom_ssbo_entry_point", "SSBO on geometry shader entry point", Geom);
	group.add("geom_ubo_entry_point", "UBO on geometry shader entry point", Geom);
	group.add("tess_con_pc_entry_point", "push constant on tess control shader entry point", Tess);
	group.add("tess_con_ssbo_entry_point", "SSBO on tess control shader entry point", Tess);
	group.add("tess_con_ubo_entry_point", "UBO on tess control shader entry point", Tess);
	group.add("tess_eval_pc_entry_point", "push constant on tess eval shader entry point", Tess);
	group.add("tess_eval_ssbo_entry_point", "SSBO on tess eval shader entry point", Tess);
	group.add("tess_eval_ubo_entry_point", "UBO on tess eval shader entry point", Tess);
	group.add("vert_pc_entry_point", "push constant on vertex shader entry point");
	group.add("vert_ssbo_entry_point", "SSBO on vertex shader entry point");
	group.add("vert_ubo_entry_point", "UBO on vertex shader entry point");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "entrypoint", "EntryPoint lists all module-scope variables", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "hlsl_functionality1");
	group.add("counter_buffer", "CounterBuffer decoration");
	group.add("decorate_string", "OpDecorateString");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "hlsl_functionality1", "Features in SPV_GOOGLE_hlsl_functionality1 in SPIR-V 1.4", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "loop_control");
	group.add("iteration_multiple", "Loop control IterationMultiple");
	group.add("max_iterations", "Loop control IterationMultiple");
	group.add("min_iterations", "Loop control MinIterations");
	group.add("partial_count", "Loop control PartialCount");
	group.add("peel_count", "Loop control PeelCount");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "loop_control", "SPIR-V 1.4 loop controls", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "opselect");
	group.add("array_select", "OpSelect arrays, new in SPIR-V 1.4");
	group.add("array_stride_select", "OpSelect arrays with non-standard strides, new in SPIR-V 1.4");
	group.add("nested_array_select", "OpSelect structs with nested arrays, new in SPIR-V 1.4");
	group.add("nested_struct_select", "OpSelect structs with nested structs, new in SPIR-V 1.4");
	group.add("scalar_select", "OpSelect scalars, verify SPIR-V 1.0");
	group.add("ssbo_pointers_2_select", "OpSelect SSBO pointers to different buffers, verify SPIR-V 1.0", Varptr_full);
	group.add("ssbo_pointers_select", "OpSelect SSBO pointers to same buffer, verify SPIR-V 1.0", Varptr_ssbo);
	group.add("struct_select", "OpSelect structs, new in SPIR-V 1.4");
	group.add("vector_element_select", "OpSelect vector with vector selector, verify SPIR-V 1.0");
	group.add("vector_select", "OpSelect vector with scalar selector, new in SPIR-V 1.4");
	group.add("wg_pointers_2_select", "OpSelect Workgroup pointers to different buffers, verify SPIR-V 1.0", Varptr_full);
	group.add("wg_pointers_select", "OpSelect Workgroup pointers to same buffer, verify SPIR-V 1.0", Varptr_full);
	spirv1p4Tests->addChild(createTestGroup(testCtx, "opselect", "SPIR-V 1.4 OpSelect more cases", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "uconvert");
	group.add("spec_const_opt_extend_16_64_bit","uconvert small to int64", Int64);
	group.add("spec_const_opt_extend_16","uconvert from int16", Int16);
	group.add("spec_const_opt_extend_251658240_64_bits","uconvert large to int64", Int64);
	group.add("spec_const_opt_extend_61440", "uconvert large from int16", Int16);
	group.add("spec_const_opt_truncate_16_64_bit", "uconvert from int64", Int64);
	group.add("spec_const_opt_truncate_16", "uconvert small to int16", Int16);
	group.add("spec_const_opt_truncate_983040", "uconvert large to int16", Int16);
	group.add("spec_const_opt_zero_extend_n4096", "uconvert negative from int16", Int16);
	spirv1p4Tests->addChild(createTestGroup(testCtx, "uconvert", "SPIR-V 1.4 UConvert in OpSpecConstantOp", addTestsForAmberFiles, group));

	group = CaseGroup(data_dir, "wrap");
	group.add("no_signed_wrap", "Accept NoSignedWrap decoration");
	group.add("no_unsigned_wrap", "Accept NoUnsignedWrap decoration");
	spirv1p4Tests->addChild(createTestGroup(testCtx, "wrap", "SPIR-V 1.4 integer wrap decorations", addTestsForAmberFiles, group));

	return spirv1p4Tests.release();
}

} // SpirVAssembly
} // vkt
