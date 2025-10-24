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
    Case(const char *b) : basename(b), requirements()
    {
    }
    Case(const char *b, const std::vector<std::string> &e) : basename(b), requirements(e)
    {
    }
    const char *basename;
    // Additional Vulkan requirements, if any.
    std::vector<std::string> requirements;
};
struct CaseGroup
{
    CaseGroup(const char *the_data_dir, const char *the_subdir) : data_dir(the_data_dir), subdir(the_subdir)
    {
    }
    void add(const char *basename)
    {
        cases.push_back(Case(basename));
    }
    void add(const char *basename, const std::vector<std::string> &requirements)
    {
        cases.push_back(Case(basename, requirements));
    }

    const char *data_dir;
    const char *subdir;
    std::vector<Case> cases;
};

void addTestsForAmberFiles(tcu::TestCaseGroup *tests, CaseGroup group)
{
#ifndef CTS_USES_VULKANSC
    tcu::TestContext &testCtx = tests->getTestContext();
    const std::string data_dir(group.data_dir);
    const std::string subdir(group.subdir);
    const std::string category = data_dir + "/" + subdir;
    std::vector<Case> cases(group.cases);
    vk::SpirVAsmBuildOptions asm_options(VK_MAKE_API_VERSION(0, 1, 1, 0), vk::SPIRV_VERSION_1_4);
    asm_options.supports_VK_KHR_spirv_1_4 = true;

    for (unsigned i = 0; i < cases.size(); ++i)
    {

        const std::string file = std::string(cases[i].basename) + ".amber";
        cts_amber::AmberTestCase *testCase =
            cts_amber::createAmberTestCase(testCtx, cases[i].basename, category.c_str(), file);
        DE_ASSERT(testCase != nullptr);
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
        const std::vector<std::string> &reqmts = cases[i].requirements;
        for (size_t r = 0; r < reqmts.size(); ++r)
        {
            testCase->addRequirement(reqmts[r]);
        }

        testCase->setSpirVAsmBuildOptions(asm_options);
        tests->addChild(testCase);
    }
#else
    DE_UNREF(tests);
    DE_UNREF(group);
#endif
}

} // namespace

tcu::TestCaseGroup *createSpirvVersion1p4Group(tcu::TestContext &testCtx)
{
    // SPIR-V 1.4 new features
    de::MovePtr<tcu::TestCaseGroup> spirv1p4Tests(new tcu::TestCaseGroup(testCtx, "spirv1p4"));

    // Location of the Amber script files under the data/vulkan/amber source tree.
    const char *data_dir = "spirv_assembly/instruction/spirv1p4";

    // Set up features used for various tests.
    std::vector<std::string> Geom;
    Geom.push_back("Features.geometryShader");

    std::vector<std::string> Tess;
    Tess.push_back("Features.tessellationShader");

    std::vector<std::string> Varptr_ssbo;
    Varptr_ssbo.push_back("VariablePointerFeatures.variablePointersStorageBuffer");

    std::vector<std::string> Varptr_full = Varptr_ssbo;
    Varptr_full.push_back("VariablePointerFeatures.variablePointers");

    std::vector<std::string> Varptr_full_explicitLayout = Varptr_full;
    Varptr_full_explicitLayout.push_back("VK_KHR_workgroup_memory_explicit_layout");

    std::vector<std::string> Int16;
    Int16.push_back("Features.shaderInt16");

    std::vector<std::string> Int16_storage = Int16;
    Int16_storage.push_back("VK_KHR_16bit_storage");
    Int16_storage.push_back("Storage16BitFeatures.storageBuffer16BitAccess");

    std::vector<std::string> Int64;
    Int64.push_back("Features.shaderInt64");

    // Define test groups

    CaseGroup group(data_dir, "opcopylogical");
    // different matrix layout
    group.add("different_matrix_layout");
    // different matrix strides
    group.add("different_matrix_strides");
    // nested_arrays_different_inner_stride
    group.add("nested_arrays_different_inner_stride");
    // nested_arrays_different_inner_stride
    group.add("nested_arrays_different_outer_stride");
    // nested_arrays_different_strides
    group.add("nested_arrays_different_strides");
    // same array two ids
    group.add("same_array_two_ids");
    // same struct two ids
    group.add("same_struct_two_ids");
    // ssbo_to_ubo
    group.add("ssbo_to_ubo");
    // two_arrays_different_stride_1
    group.add("two_arrays_different_stride_1");
    // two_arrays_different_stride_2
    group.add("two_arrays_different_stride_2");
    // ubo_to_ssbo
    group.add("ubo_to_ssbo");
    spirv1p4Tests->addChild(createTestGroup(testCtx, "opcopylogical", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "opptrdiff");
    // pointer diff within an SSBO
    group.add("ssbo_comparisons_diff", Varptr_ssbo);
    // pointer diff in SSBO with full VariablePointers
    group.add("variable_pointers_vars_ssbo_2_diff", Varptr_ssbo);
    // pointer diff in SSBO, stored in private var
    group.add("variable_pointers_vars_ssbo_diff", Varptr_ssbo);
    // pointer diff in workgroup storage, stored in private var
    group.add("variable_pointers_vars_wg_diff", Varptr_full);
    // pointer diff in workgroup storage
    group.add("wg_comparisons_diff", Varptr_full);
    spirv1p4Tests->addChild(createTestGroup(testCtx, "opptrdiff", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "opptrequal");
    // ptr equal against different SSBO variables
    group.add("different_ssbos_equal", Varptr_full);
    // ptr equal against different WG variables
    group.add("different_wgs_equal", Varptr_full);
    // ptr equal null in SSBO
    group.add("null_comparisons_ssbo_equal", Varptr_ssbo);
    // ptr equal null in Workgrop
    group.add("null_comparisons_wg_equal", Varptr_full);
    // ptr equal in SSBO
    group.add("ssbo_comparisons_equal", Varptr_ssbo);
    // ptr equal in SSBO, store pointers in Function var
    group.add("variable_pointers_ssbo_2_equal", Varptr_full);
    // ptr equal in SSBO
    group.add("variable_pointers_ssbo_equal", Varptr_ssbo);
    // ptr equal in SSBO, store pointers in Private var
    group.add("variable_pointers_vars_ssbo_equal", Varptr_ssbo);
    // ptr equal between simple data primitives in SSBOs
    group.add("simple_variable_pointers_ptr_equal", Varptr_ssbo);
    // ptr equal in Workgrop, store pointers in Private var
    group.add("variable_pointers_vars_wg_equal", Varptr_full);
    // ptr equal in Workgrop
    group.add("variable_pointers_wg_equal", Varptr_full);
    // ptr equal in Workgrop
    group.add("wg_comparisons_equal", Varptr_full);
    spirv1p4Tests->addChild(createTestGroup(testCtx, "opptrequal", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "opptrnotequal");
    // ptr not equal against different SSBO variables
    group.add("different_ssbos_not_equal", Varptr_full);
    // ptr not equal against different WG variables
    group.add("different_wgs_not_equal", Varptr_full);
    // ptr not equal null SSBO
    group.add("null_comparisons_ssbo_not_equal", Varptr_ssbo);
    // ptr not equal null SSBO
    group.add("null_comparisons_wg_not_equal", Varptr_full);
    // ptr not equal SSBO
    group.add("ssbo_comparisons_not_equal", Varptr_ssbo);
    // ptr not equal SSBO, store pointer in Function var
    group.add("variable_pointers_ssbo_2_not_equal", Varptr_full);
    // ptr not equal SSBO, pointer from function return
    group.add("variable_pointers_ssbo_not_equal", Varptr_ssbo);
    // ptr not equal between simple data primitives in SSBOs
    group.add("simple_variable_pointers_ptr_not_equal", Varptr_ssbo);
    // ptr not equal SSBO, store pointer in Private var
    group.add("variable_pointers_vars_ssbo_not_equal", Varptr_ssbo);
    // ptr not equal Workgroup, store pointer in Private var
    group.add("variable_pointers_vars_wg_not_equal", Varptr_full);
    // ptr not equal Workgroup
    group.add("variable_pointers_wg_not_equal", Varptr_full);
    // ptr not equal Workgroup
    group.add("wg_comparisons_not_equal", Varptr_full);
    spirv1p4Tests->addChild(createTestGroup(testCtx, "opptrnotequal", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "opcopymemory");
    // different alignments
    group.add("different_alignments");
    // no source access operands
    group.add("no_source_access_operands");
    // no target access operands
    group.add("no_target_access_operands");
    spirv1p4Tests->addChild(createTestGroup(testCtx, "opcopymemory", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "uniformid");
    // workgroup uniform load result at consumption, in nonuniform control flow
    group.add("partially_active_uniform_id");
    // subgroup uniform compare result inside control flow
    group.add("subgroup_cfg_uniform_id"); // Assumes subgroup size <= LocalSize of 8
    // subgroup uniform load result
    group.add("subgroup_uniform"); // Assumes subgroup size <= LocalSize 8
    // workgroup uniform compare result
    group.add("workgroup_cfg_uniform_id");
    // workgroup uniform load result
    group.add("workgroup_uniform");
    spirv1p4Tests->addChild(createTestGroup(testCtx, "uniformid", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "nonwritable");
    // NonWritable decorates Function variables
    group.add("function_2_nonwritable");
    // NonWritable decorates 2 Function variables
    group.add("function_nonwritable");
    // NonWritable decorates Function variable in non-entrypoint function
    group.add("non_main_function_nonwritable");
    // NonWritable decorates Private variables
    group.add("private_2_nonwritable");
    // NonWritable decorates 2 Private variables
    group.add("private_nonwritable");
    spirv1p4Tests->addChild(createTestGroup(testCtx, "nonwritable", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "entrypoint");
    // push constant on compute shader entry point
    group.add("comp_pc_entry_point");
    // SSBO on compute shader entry point
    group.add("comp_ssbo_entry_point");
    // UBO on compute shader entry point
    group.add("comp_ubo_entry_point");
    // Workgroup var on compute shader entry point
    group.add("comp_workgroup_entry_point");
    // push constant on fragment shader entry point
    group.add("frag_pc_entry_point");
    // SSBO on fragment shader entry point
    group.add("frag_ssbo_entry_point");
    // UBO on fragment shader entry point
    group.add("frag_ubo_entry_point");
    // push constant on geometry shader entry point
    group.add("geom_pc_entry_point", Geom);
    // SSBO on geometry shader entry point
    group.add("geom_ssbo_entry_point", Geom);
    // UBO on geometry shader entry point
    group.add("geom_ubo_entry_point", Geom);
    // push constant on tess control shader entry point
    group.add("tess_con_pc_entry_point", Tess);
    // SSBO on tess control shader entry point
    group.add("tess_con_ssbo_entry_point", Tess);
    // UBO on tess control shader entry point
    group.add("tess_con_ubo_entry_point", Tess);
    // push constant on tess eval shader entry point
    group.add("tess_eval_pc_entry_point", Tess);
    // SSBO on tess eval shader entry point
    group.add("tess_eval_ssbo_entry_point", Tess);
    // UBO on tess eval shader entry point
    group.add("tess_eval_ubo_entry_point", Tess);
    // push constant on vertex shader entry point
    group.add("vert_pc_entry_point");
    // SSBO on vertex shader entry point
    group.add("vert_ssbo_entry_point");
    // UBO on vertex shader entry point
    group.add("vert_ubo_entry_point");
    // EntryPoint lists all module-scope variables
    spirv1p4Tests->addChild(createTestGroup(testCtx, "entrypoint", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "hlsl_functionality1");
    // CounterBuffer decoration
    group.add("counter_buffer");
    // OpDecorateString
    group.add("decorate_string");
    // OpMemberDecorateString
    group.add("member_decorate_string");
    // Features in SPV_GOOGLE_hlsl_functionality1 in SPIR-V 1.4
    spirv1p4Tests->addChild(createTestGroup(testCtx, "hlsl_functionality1", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "loop_control");
    // Loop control IterationMultiple
    group.add("iteration_multiple");
    // Loop control MaxIterations
    group.add("max_iterations");
    // Loop control MinIterations
    group.add("min_iterations");
    // Loop control PartialCount
    group.add("partial_count");
    // Loop control PeelCount
    group.add("peel_count");
    // SPIR-V 1.4 loop controls
    spirv1p4Tests->addChild(createTestGroup(testCtx, "loop_control", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "opselect");
    // OpSelect arrays, new in SPIR-V 1.4
    group.add("array_select");
    // OpSelect arrays with non-standard strides, new in SPIR-V 1.4
    group.add("array_stride_select");
    // OpSelect structs with nested arrays, new in SPIR-V 1.4
    group.add("nested_array_select");
    // OpSelect structs with nested structs, new in SPIR-V 1.4
    group.add("nested_struct_select");
    // OpSelect scalars, verify SPIR-V 1.0
    group.add("scalar_select");
    // OpSelect SSBO pointers to different buffers, verify SPIR-V 1.0
    group.add("ssbo_pointers_2_select", Varptr_full);
    // OpSelect SSBO pointers to same buffer, verify SPIR-V 1.0
    group.add("ssbo_pointers_select", Varptr_ssbo);
    // OpSelect structs, new in SPIR-V 1.4
    group.add("struct_select");
    // OpSelect vector with vector selector, verify SPIR-V 1.0
    group.add("vector_element_select");
    // OpSelect vector with scalar selector, new in SPIR-V 1.4
    group.add("vector_select");
    // OpSelect Workgroup pointers to different buffers, verify SPIR-V 1.0
    group.add("wg_pointers_2_select", Varptr_full_explicitLayout);
    // OpSelect Workgroup pointers to same buffer, verify SPIR-V 1.0
    group.add("wg_pointers_select", Varptr_full_explicitLayout);
    // SPIR-V 1.4 OpSelect more cases
    spirv1p4Tests->addChild(createTestGroup(testCtx, "opselect", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "uconvert");
    // uconvert small to int64
    group.add("spec_const_opt_extend_16_64_bit", Int64);
    // uconvert from int16
    group.add("spec_const_opt_extend_16", Int16);
    // uconvert large to int64
    group.add("spec_const_opt_extend_251658240_64_bits", Int64);
    // uconvert large from int16
    group.add("spec_const_opt_extend_61440", Int16);
    // uconvert from int64
    group.add("spec_const_opt_truncate_16_64_bit", Int64);
    // uconvert small to int16
    group.add("spec_const_opt_truncate_16", Int16_storage);
    // uconvert large to int16
    group.add("spec_const_opt_truncate_983040", Int16_storage);
    // uconvert negative from int16
    group.add("spec_const_opt_zero_extend_n4096", Int16);
    // SPIR-V 1.4 UConvert in OpSpecConstantOp
    spirv1p4Tests->addChild(createTestGroup(testCtx, "uconvert", addTestsForAmberFiles, group));

    group = CaseGroup(data_dir, "wrap");
    // Accept NoSignedWrap decoration
    group.add("no_signed_wrap");
    // Accept NoUnsignedWrap decoration
    group.add("no_unsigned_wrap");
    // SPIR-V 1.4 integer wrap decorations
    spirv1p4Tests->addChild(createTestGroup(testCtx, "wrap", addTestsForAmberFiles, group));

    return spirv1p4Tests.release();
}

} // namespace SpirVAssembly
} // namespace vkt
