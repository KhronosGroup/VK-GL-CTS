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
 * \brief Test new features in VK_KHR_shader_subgroup_uniform_control_flow
 *//*--------------------------------------------------------------------*/

#include <amber/amber.h>

#include "tcuDefs.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"
#include "vktSubgroupUniformControlFlowTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace subgroups
{
namespace
{

struct Case
{
    Case(const char *b, bool sw, bool use_ssc, vk::VkShaderStageFlagBits s, vk::VkSubgroupFeatureFlagBits o)
        : basename(b)
        , small_workgroups(sw)
        , use_subgroup_size_control(use_ssc)
        , stage(s)
    {
        operation = (vk::VkSubgroupFeatureFlagBits)(o | vk::VK_SUBGROUP_FEATURE_BASIC_BIT);
    }
    const char *basename;
    bool small_workgroups;
    bool use_subgroup_size_control;
    vk::VkShaderStageFlagBits stage;
    vk::VkSubgroupFeatureFlagBits operation;
};

struct CaseGroup
{
    CaseGroup(const char *the_data_dir, const char *the_subdir) : data_dir(the_data_dir), subdir(the_subdir)
    {
    }
    void add(const char *basename, bool small_workgroups, bool use_subgroup_size_control,
             vk::VkShaderStageFlagBits stage,
             vk::VkSubgroupFeatureFlagBits operation = vk::VK_SUBGROUP_FEATURE_BASIC_BIT)
    {
        cases.push_back(Case(basename, small_workgroups, use_subgroup_size_control, stage, operation));
    }

    const char *data_dir;
    const char *subdir;
    std::vector<Case> cases;
};

class SubgroupUniformControlFlowTestCase : public cts_amber::AmberTestCase
{
public:
    SubgroupUniformControlFlowTestCase(tcu::TestContext &testCtx, const char *name, const std::string &readFilename,
                                       bool small_workgroups, bool use_subgroup_size_control,
                                       vk::VkShaderStageFlagBits stage, vk::VkSubgroupFeatureFlagBits operation)
        : cts_amber::AmberTestCase(testCtx, name, "", readFilename)
        , m_small_workgroups(small_workgroups)
        , m_use_subgroup_size_control(use_subgroup_size_control)
        , m_stage(stage)
        , m_operation(operation)
    {
    }

    virtual void checkSupport(Context &ctx) const; // override
private:
    bool m_small_workgroups;
    bool m_use_subgroup_size_control;
    vk::VkShaderStageFlagBits m_stage;
    vk::VkSubgroupFeatureFlagBits m_operation;
};

void SubgroupUniformControlFlowTestCase::checkSupport(Context &ctx) const
{
    // Check required extensions.
    ctx.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    ctx.requireDeviceFunctionality("VK_KHR_shader_subgroup_uniform_control_flow");
    if (m_use_subgroup_size_control)
    {
        ctx.requireDeviceFunctionality("VK_EXT_subgroup_size_control");
    }

    vk::VkPhysicalDeviceSubgroupProperties subgroupProperties;
    subgroupProperties.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    subgroupProperties.pNext = nullptr;

    vk::VkPhysicalDeviceProperties2 properties2;
    properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &subgroupProperties;

    ctx.getInstanceInterface().getPhysicalDeviceProperties2(ctx.getPhysicalDevice(), &properties2);

    vk::VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
    subgroupSizeControlFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
    subgroupSizeControlFeatures.pNext = nullptr;

    vk::VkPhysicalDeviceFeatures2 features2;
    features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &subgroupSizeControlFeatures;

    ctx.getInstanceInterface().getPhysicalDeviceFeatures2(ctx.getPhysicalDevice(), &features2);

    // Check that the stage supports the required subgroup operations.
    if ((m_stage & subgroupProperties.supportedStages) == 0)
    {
        TCU_THROW(NotSupportedError, "Device does not support subgroup operations in this stage");
    }
    if ((m_operation & subgroupProperties.supportedOperations) != m_operation)
    {
        TCU_THROW(NotSupportedError, "Device does not support required operations");
    }

    // For the compute shader tests, there are variants for implementations
    // that support the subgroup size control extension and variants for those
    // that do not. It is expected that computeFullSubgroups must be set for
    // these tests if the extension is supported so tests are only supported
    // for the extension appropriate version.
    if (m_stage == vk::VK_SHADER_STAGE_COMPUTE_BIT)
    {
        if (m_use_subgroup_size_control)
        {
            if (subgroupSizeControlFeatures.computeFullSubgroups != VK_TRUE)
            {
                TCU_THROW(NotSupportedError, "Implementation does not support subgroup size control");
            }
        }
        else
        {
            if (subgroupSizeControlFeatures.computeFullSubgroups == VK_TRUE)
            {
                TCU_THROW(NotSupportedError, "These tests are not enabled for subgroup size control implementations");
            }
        }
    }

    // The are large and small variants of the tests. The large variants
    // require 256 invocations in a workgroup.
    if (!m_small_workgroups)
    {
        vk::VkPhysicalDeviceProperties properties;
        ctx.getInstanceInterface().getPhysicalDeviceProperties(ctx.getPhysicalDevice(), &properties);
        if (properties.limits.maxComputeWorkGroupInvocations < 256)
        {
            TCU_THROW(NotSupportedError, "Device supported fewer than 256 invocations per workgroup");
        }
    }
}

template <bool requirements>
void addTestsForAmberFiles(tcu::TestCaseGroup *tests, CaseGroup group)
{
    tcu::TestContext &testCtx = tests->getTestContext();
    const std::string data_dir(group.data_dir);
    const std::string subdir(group.subdir);
    const std::string category = data_dir + "/" + subdir;
    std::vector<Case> cases(group.cases);

    for (unsigned i = 0; i < cases.size(); ++i)
    {
        const std::string file = std::string(cases[i].basename) + ".amber";
        std::string readFilename("vulkan/amber/");
        readFilename.append(category);
        readFilename.append("/");
        readFilename.append(file);
        SubgroupUniformControlFlowTestCase *testCase = new SubgroupUniformControlFlowTestCase(
            testCtx, cases[i].basename, readFilename, cases[i].small_workgroups, cases[i].use_subgroup_size_control,
            cases[i].stage, cases[i].operation);
        DE_ASSERT(testCase != nullptr);
        if (requirements)
        {
            testCase->addRequirement("SubgroupSizeControl.computeFullSubgroups");
            testCase->addRequirement("SubgroupSizeControl.subgroupSizeControl");
        }
        tests->addChild(testCase);
    }
}

} // namespace

tcu::TestCaseGroup *createSubgroupUniformControlFlowTests(tcu::TestContext &testCtx)
{
    // There are four main groups of tests. Each group runs the same set of base
    // shaders with minor variations. The groups are with or without compute full
    // subgroups and a larger or smaller number of invocations. For each group of
    // tests, shaders test either odd or even subgroups reconverge after
    // diverging, without reconverging the whole workgroup. For the _partial
    // tests, the workgroup is launched without a full final subgroup (not enough
    // invocations).
    //
    // It is assumed that if an implementation does not support the compute full
    // subgroups feature, that it will always launch full subgroups. Therefore,
    // any given implementation only runs half of the tests. Implementations that
    // do not support compute full subgroups cannot support the tests that enable
    // it, while implementations that do support the feature will (likely) not
    // pass the tests that do not enable the feature.

    de::MovePtr<tcu::TestCaseGroup> uniformControlFlowTests(
        new tcu::TestCaseGroup(testCtx, "subgroup_uniform_control_flow"));

    // Location of the Amber script files under data/vulkan/amber source tree.
    const char *data_dir          = "subgroup_uniform_control_flow";
    const char *large_dir         = "large";
    const char *small_dir         = "small";
    const char *large_control_dir = "large_control";
    const char *small_control_dir = "small_control";

    std::vector<bool> controls = {false, true};
    for (unsigned c = 0; c < controls.size(); ++c)
    {
        // Full subgroups.
        bool small                      = false;
        bool control                    = controls[c];
        vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
        const char *subdir              = (control ? large_control_dir : large_dir);
        CaseGroup group(data_dir, subdir);
        // if/else diverge
        group.add("subgroup_reconverge00", small, control, stage);
        // do while diverge
        group.add("subgroup_reconverge01", small, control, stage);
        // while true with break
        group.add("subgroup_reconverge02", small, control, stage);
        // if/else diverge, volatile
        group.add("subgroup_reconverge03", small, control, stage);
        // early return and if/else diverge
        group.add("subgroup_reconverge04", small, control, stage);
        // early return and if/else volatile
        group.add("subgroup_reconverge05", small, control, stage);
        // while true with volatile conditional break and early return
        group.add("subgroup_reconverge06", small, control, stage);
        // while true return and break
        group.add("subgroup_reconverge07", small, control, stage);
        // for loop atomics with conditional break
        group.add("subgroup_reconverge08", small, control, stage);
        // diverge in for loop
        group.add("subgroup_reconverge09", small, control, stage);
        // diverge in for loop and break
        group.add("subgroup_reconverge10", small, control, stage);
        // diverge in for loop and continue
        group.add("subgroup_reconverge11", small, control, stage);
        // early return, divergent switch
        group.add("subgroup_reconverge12", small, control, stage);
        // early return, divergent switch more cases
        group.add("subgroup_reconverge13", small, control, stage);
        // divergent switch, some subgroups terminate
        group.add("subgroup_reconverge14", small, control, stage);
        // switch in switch
        group.add("subgroup_reconverge15", small, control, stage);
        // for loop unequal iterations
        group.add("subgroup_reconverge16", small, control, stage);
        // if/else with nested returns
        group.add("subgroup_reconverge17", small, control, stage);
        // if/else subgroup all equal
        group.add("subgroup_reconverge18", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // if/else subgroup any nested return
        group.add("subgroup_reconverge19", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // deeply nested
        group.add("subgroup_reconverge20", small, control, stage);
        const char *group_name = (control ? "large_full_control" : "large_full");
        // Large Full subgroups
        uniformControlFlowTests->addChild(createTestGroup(
            testCtx, group_name, control ? addTestsForAmberFiles<true> : addTestsForAmberFiles<false>, group));

        // Partial subgroup.
        group = CaseGroup(data_dir, subdir);
        // if/else diverge
        group.add("subgroup_reconverge_partial00", small, control, stage);
        // do while diverge
        group.add("subgroup_reconverge_partial01", small, control, stage);
        // while true with break
        group.add("subgroup_reconverge_partial02", small, control, stage);
        // if/else diverge, volatile
        group.add("subgroup_reconverge_partial03", small, control, stage);
        // early return and if/else diverge
        group.add("subgroup_reconverge_partial04", small, control, stage);
        // early return and if/else volatile
        group.add("subgroup_reconverge_partial05", small, control, stage);
        // while true with volatile conditional break and early return
        group.add("subgroup_reconverge_partial06", small, control, stage);
        // while true return and break
        group.add("subgroup_reconverge_partial07", small, control, stage);
        // for loop atomics with conditional break
        group.add("subgroup_reconverge_partial08", small, control, stage);
        // diverge in for loop
        group.add("subgroup_reconverge_partial09", small, control, stage);
        // diverge in for loop and break
        group.add("subgroup_reconverge_partial10", small, control, stage);
        // diverge in for loop and continue
        group.add("subgroup_reconverge_partial11", small, control, stage);
        // early return, divergent switch
        group.add("subgroup_reconverge_partial12", small, control, stage);
        // early return, divergent switch more cases
        group.add("subgroup_reconverge_partial13", small, control, stage);
        // divergent switch, some subgroups terminate
        group.add("subgroup_reconverge_partial14", small, control, stage);
        // switch in switch
        group.add("subgroup_reconverge_partial15", small, control, stage);
        // for loop unequal iterations
        group.add("subgroup_reconverge_partial16", small, control, stage);
        // if/else with nested returns
        group.add("subgroup_reconverge_partial17", small, control, stage);
        // if/else subgroup all equal
        group.add("subgroup_reconverge_partial18", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // if/else subgroup any nested return
        group.add("subgroup_reconverge_partial19", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // deeply nested
        group.add("subgroup_reconverge_partial20", small, control, stage);
        group_name = (control ? "large_partial_control" : "large_partial");
        // Large Partial subgroups
        uniformControlFlowTests->addChild(createTestGroup(
            testCtx, group_name, control ? addTestsForAmberFiles<true> : addTestsForAmberFiles<false>, group));
    }

    for (unsigned c = 0; c < controls.size(); ++c)
    {
        // Full subgroups.
        bool small                      = true;
        bool control                    = controls[c];
        vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
        const char *subdir              = (control ? small_control_dir : small_dir);
        CaseGroup group(data_dir, subdir);
        // if/else diverge
        group.add("small_subgroup_reconverge00", small, control, stage);
        // do while diverge
        group.add("small_subgroup_reconverge01", small, control, stage);
        // while true with break
        group.add("small_subgroup_reconverge02", small, control, stage);
        // if/else diverge, volatile
        group.add("small_subgroup_reconverge03", small, control, stage);
        // early return and if/else diverge
        group.add("small_subgroup_reconverge04", small, control, stage);
        // early return and if/else volatile
        group.add("small_subgroup_reconverge05", small, control, stage);
        // while true with volatile conditional break and early return
        group.add("small_subgroup_reconverge06", small, control, stage);
        // while true return and break
        group.add("small_subgroup_reconverge07", small, control, stage);
        // for loop atomics with conditional break
        group.add("small_subgroup_reconverge08", small, control, stage);
        // diverge in for loop
        group.add("small_subgroup_reconverge09", small, control, stage);
        // diverge in for loop and break
        group.add("small_subgroup_reconverge10", small, control, stage);
        // diverge in for loop and continue
        group.add("small_subgroup_reconverge11", small, control, stage);
        // early return, divergent switch
        group.add("small_subgroup_reconverge12", small, control, stage);
        // early return, divergent switch more cases
        group.add("small_subgroup_reconverge13", small, control, stage);
        // divergent switch, some subgroups terminate
        group.add("small_subgroup_reconverge14", small, control, stage);
        // switch in switch
        group.add("small_subgroup_reconverge15", small, control, stage);
        // for loop unequal iterations
        group.add("small_subgroup_reconverge16", small, control, stage);
        // if/else with nested returns
        group.add("small_subgroup_reconverge17", small, control, stage);
        // if/else subgroup all equal
        group.add("small_subgroup_reconverge18", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // if/else subgroup any nested return
        group.add("small_subgroup_reconverge19", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // deeply nested
        group.add("small_subgroup_reconverge20", small, control, stage);
        const char *group_name = (control ? "small_full_control" : "small_full");
        // Small Full subgroups
        uniformControlFlowTests->addChild(createTestGroup(
            testCtx, group_name, control ? addTestsForAmberFiles<true> : addTestsForAmberFiles<false>, group));

        // Partial subgroup.
        group = CaseGroup(data_dir, subdir);
        // if/else diverge
        group.add("small_subgroup_reconverge_partial00", small, control, stage);
        // do while diverge
        group.add("small_subgroup_reconverge_partial01", small, control, stage);
        // while true with break
        group.add("small_subgroup_reconverge_partial02", small, control, stage);
        // if/else diverge, volatile
        group.add("small_subgroup_reconverge_partial03", small, control, stage);
        // early return and if/else diverge
        group.add("small_subgroup_reconverge_partial04", small, control, stage);
        // early return and if/else volatile
        group.add("small_subgroup_reconverge_partial05", small, control, stage);
        // while true with volatile conditional break and early return
        group.add("small_subgroup_reconverge_partial06", small, control, stage);
        // while true return and break
        group.add("small_subgroup_reconverge_partial07", small, control, stage);
        // for loop atomics with conditional break
        group.add("small_subgroup_reconverge_partial08", small, control, stage);
        // diverge in for loop
        group.add("small_subgroup_reconverge_partial09", small, control, stage);
        // diverge in for loop and break
        group.add("small_subgroup_reconverge_partial10", small, control, stage);
        // diverge in for loop and continue
        group.add("small_subgroup_reconverge_partial11", small, control, stage);
        // early return, divergent switch
        group.add("small_subgroup_reconverge_partial12", small, control, stage);
        // early return, divergent switch more cases
        group.add("small_subgroup_reconverge_partial13", small, control, stage);
        // divergent switch, some subgroups terminate
        group.add("small_subgroup_reconverge_partial14", small, control, stage);
        // switch in switch
        group.add("small_subgroup_reconverge_partial15", small, control, stage);
        // for loop unequal iterations
        group.add("small_subgroup_reconverge_partial16", small, control, stage);
        // if/else with nested returns
        group.add("small_subgroup_reconverge_partial17", small, control, stage);
        // if/else subgroup all equal
        group.add("small_subgroup_reconverge_partial18", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // if/else subgroup any nested return
        group.add("small_subgroup_reconverge_partial19", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
        // deeply nested
        group.add("small_subgroup_reconverge_partial20", small, control, stage);
        group_name = (control ? "small_partial_control" : "small_partial");
        // Small Partial subgroups
        uniformControlFlowTests->addChild(createTestGroup(
            testCtx, group_name, control ? addTestsForAmberFiles<true> : addTestsForAmberFiles<false>, group));
    }

    // Discard test
    CaseGroup group(data_dir, "discard");
    // discard test
    group.add("subgroup_reconverge_discard00", true, false, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    // Discard tests
    uniformControlFlowTests->addChild(createTestGroup(testCtx, "discard", addTestsForAmberFiles<false>, group));

    return uniformControlFlowTests.release();
}

} // namespace subgroups
} // namespace vkt
