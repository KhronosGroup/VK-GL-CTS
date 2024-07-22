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
    Case(const char *b, bool v) : basename(b), spv1p3(v), requirements()
    {
    }
    Case(const char *b, bool v, const std::vector<std::string> &e) : basename(b), spv1p3(v), requirements(e)
    {
    }
    const char *basename;
    bool spv1p3;
    // Additional Vulkan requirements, if any.
    std::vector<std::string> requirements;
};
struct CaseGroup
{
    CaseGroup(const char *the_data_dir) : data_dir(the_data_dir)
    {
    }
    void add(const char *basename, bool spv1p3)
    {
        cases.push_back(Case(basename, spv1p3));
    }
    void add(const char *basename, bool spv1p3, const std::vector<std::string> &requirements)
    {
        cases.push_back(Case(basename, spv1p3, requirements));
    }

    const char *data_dir;
    std::vector<Case> cases;
};

#ifndef CTS_USES_VULKANSC

void addTestsForAmberFiles(tcu::TestCaseGroup *tests, CaseGroup group)
{
    tcu::TestContext &testCtx = tests->getTestContext();
    const std::string data_dir(group.data_dir);
    const std::string category = data_dir;
    std::vector<Case> cases(group.cases);

    for (unsigned i = 0; i < cases.size(); ++i)
    {
        uint32_t vulkan_version = cases[i].spv1p3 ? VK_MAKE_API_VERSION(0, 1, 1, 0) : VK_MAKE_API_VERSION(0, 1, 0, 0);
        vk::SpirvVersion spirv_version = cases[i].spv1p3 ? vk::SPIRV_VERSION_1_3 : vk::SPIRV_VERSION_1_0;
        vk::SpirVAsmBuildOptions asm_options(vulkan_version, spirv_version);

        const std::string file = std::string(cases[i].basename) + ".amber";
        cts_amber::AmberTestCase *testCase =
            cts_amber::createAmberTestCase(testCtx, cases[i].basename, category.c_str(), file);
        DE_ASSERT(testCase != nullptr);
        testCase->addRequirement("VK_KHR_shader_terminate_invocation");
        const std::vector<std::string> &reqmts = cases[i].requirements;
        for (size_t r = 0; r < reqmts.size(); ++r)
        {
            testCase->addRequirement(reqmts[r]);
        }

        testCase->setSpirVAsmBuildOptions(asm_options);
        tests->addChild(testCase);
    }
}

#endif // CTS_USES_VULKANSC

} // namespace

tcu::TestCaseGroup *createTerminateInvocationGroup(tcu::TestContext &testCtx)
{
    // VK_KHR_shader_terminate_invocation tests
    de::MovePtr<tcu::TestCaseGroup> terminateTests(new tcu::TestCaseGroup(testCtx, "terminate_invocation"));

    const char *data_data = "spirv_assembly/instruction/terminate_invocation";

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
    // no write to after calling terminate invocation
    group.add("no_output_write", false);
    // no write to output despite occurring before terminate invocation
    group.add("no_output_write_before_terminate", false);
    // no store to SSBO when it occurs after terminate invocation
    group.add("no_ssbo_store", false, Stores);
    // no atomic update to SSBO when it occurs after terminate invocation
    group.add("no_ssbo_atomic", false, Stores);
    // ssbo store commits when it occurs before terminate invocation
    group.add("ssbo_store_before_terminate", false, Stores);
    // no image write when it occurs after terminate invocation
    group.add("no_image_store", false, Stores);
    // no image atomic updates when it occurs after terminate invocation
    group.add("no_image_atomic", false, Stores);
    // null pointer should not be accessed by a load in a terminated invocation
    group.add("no_null_pointer_load", false, VarPtr);
    // null pointer should not be accessed by a store in a terminated invocation
    group.add("no_null_pointer_store", false, VarPtr);
    // out of bounds pointer should not be accessed by a load in a terminated invocation
    group.add("no_out_of_bounds_load", false, VarPtr);
    // out of bounds pointer should not be accessed by a store in a terminated invocation
    group.add("no_out_of_bounds_store", false, VarPtr);
    // out of bounds pointer should not be accessed by an atomic in a terminated invocation
    group.add("no_out_of_bounds_atomic", false, VarPtr);
    // "infinite" loop that calls terminate invocation
    group.add("terminate_loop", false);
    // checks that terminated invocations don't participate in the ballot
    group.add("subgroup_ballot", true, Ballot);
    // checks that a subgroup all does not include any terminated invocations
    group.add("subgroup_vote", true, Vote);
#ifndef CTS_USES_VULKANSC
    terminateTests->addChild(createTestGroup(testCtx, "terminate", addTestsForAmberFiles, group));
#endif // CTS_USES_VULKANSC

    return terminateTests.release();
}

} // namespace SpirVAssembly
} // namespace vkt
