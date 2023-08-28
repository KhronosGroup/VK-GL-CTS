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
	Case(const char*	b, const char* d,	bool sw,	bool use_ssc,	vk::VkShaderStageFlagBits s, vk::VkSubgroupFeatureFlagBits o) :
		basename(b),
		description(d),
		small_workgroups(sw),
		use_subgroup_size_control(use_ssc),
		stage(s)
	{
		operation = (vk::VkSubgroupFeatureFlagBits)(o | vk::VK_SUBGROUP_FEATURE_BASIC_BIT);
	}
	const char* basename;
	const char* description;
	bool small_workgroups;
	bool use_subgroup_size_control;
	vk::VkShaderStageFlagBits stage;
	vk::VkSubgroupFeatureFlagBits operation;
};

struct CaseGroup
{
	CaseGroup(const char*	the_data_dir, const char*	the_subdir) : data_dir(the_data_dir),	subdir(the_subdir) { }
	void add(const char*	basename,	const char*	description,	bool small_workgroups,	bool use_subgroup_size_control, vk::VkShaderStageFlagBits stage, vk::VkSubgroupFeatureFlagBits operation = vk::VK_SUBGROUP_FEATURE_BASIC_BIT)
	{
		cases.push_back(Case(basename, description, small_workgroups, use_subgroup_size_control, stage, operation));
	}

	const char*	data_dir;
	const char*	subdir;
	std::vector<Case>	cases;
};

class SubgroupUniformControlFlowTestCase : public cts_amber::AmberTestCase
{
public:
	SubgroupUniformControlFlowTestCase(tcu::TestContext&	testCtx,
									   const char*	name,
									   const char*	description,
									   const std::string&	readFilename,
									   bool	small_workgroups,
									   bool	use_subgroup_size_control,
									   vk::VkShaderStageFlagBits stage,
									   vk::VkSubgroupFeatureFlagBits operation) :
		cts_amber::AmberTestCase(testCtx, name, description, readFilename),
		m_small_workgroups(small_workgroups),
		m_use_subgroup_size_control(use_subgroup_size_control),
		m_stage(stage),
		m_operation(operation)
	{ }

	virtual void checkSupport(Context&	ctx) const;	// override
private:
	bool	m_small_workgroups;
	bool	m_use_subgroup_size_control;
	vk::VkShaderStageFlagBits	m_stage;
	vk::VkSubgroupFeatureFlagBits	m_operation;
};

void SubgroupUniformControlFlowTestCase::checkSupport(Context& ctx) const
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
	subgroupProperties.pNext = DE_NULL;

	vk::VkPhysicalDeviceProperties2 properties2;
	properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &subgroupProperties;

	ctx.getInstanceInterface().getPhysicalDeviceProperties2(ctx.getPhysicalDevice(), &properties2);

	vk::VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroupSizeControlFeatures;
	subgroupSizeControlFeatures.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT;
	subgroupSizeControlFeatures.pNext = DE_NULL;

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

template<bool requirements> void addTestsForAmberFiles(tcu::TestCaseGroup* tests, CaseGroup group)
{
	tcu::TestContext&	testCtx = tests->getTestContext();
	const std::string	data_dir(group.data_dir);
	const std::string	subdir(group.subdir);
	const std::string	category = data_dir + "/" + subdir;
	std::vector<Case> cases(group.cases);

	for (unsigned i = 0; i < cases.size(); ++i)
	{
		const std::string file = std::string(cases[i].basename) + ".amber";
		std::string readFilename("vulkan/amber/");
		readFilename.append(category);
		readFilename.append("/");
		readFilename.append(file);
		SubgroupUniformControlFlowTestCase*	testCase =
			new SubgroupUniformControlFlowTestCase(testCtx,
													cases[i].basename,
													cases[i].description,
													readFilename,
													cases[i].small_workgroups,
													cases[i].use_subgroup_size_control,
													cases[i].stage,
													cases[i].operation);
		DE_ASSERT(testCase != DE_NULL);
		if (requirements)
		{
			testCase->addRequirement("SubgroupSizeControl.computeFullSubgroups");
			testCase->addRequirement("SubgroupSizeControl.subgroupSizeControl");
		}
		tests->addChild(testCase);
	}
}

} // anonymous

tcu::TestCaseGroup* createSubgroupUniformControlFlowTests(tcu::TestContext&	testCtx)
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

	de::MovePtr<tcu::TestCaseGroup>	uniformControlFlowTests(new	tcu::TestCaseGroup(testCtx,	"subgroup_uniform_control_flow", "VK_KHR_shader_subgroup_uniform_control_flow tests"));

	// Location of the Amber script files under data/vulkan/amber source tree.
	const char* data_dir = "subgroup_uniform_control_flow";
	const char*	large_dir = "large";
	const char*	small_dir = "small";
	const char*	large_control_dir = "large_control";
	const char* small_control_dir = "small_control";

	std::vector<bool> controls = {false, true};
	for (unsigned c = 0; c < controls.size(); ++c)
	{
		// Full subgroups.
		bool small = false;
		bool control = controls[c];
		vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
		const char*	subdir = (control ? large_control_dir : large_dir);
		CaseGroup group(data_dir, subdir);
		group.add("subgroup_reconverge00", "if/else diverge", small, control, stage);
		group.add("subgroup_reconverge01", "do while diverge", small, control, stage);
		group.add("subgroup_reconverge02", "while true with break", small, control, stage);
		group.add("subgroup_reconverge03", "if/else diverge, volatile", small, control, stage);
		group.add("subgroup_reconverge04", "early return and if/else diverge", small, control, stage);
		group.add("subgroup_reconverge05", "early return and if/else volatile", small, control, stage);
		group.add("subgroup_reconverge06", "while true with volatile conditional break and early return", small, control, stage);
		group.add("subgroup_reconverge07", "while true return and break", small, control, stage);
		group.add("subgroup_reconverge08", "for loop atomics with conditional break", small, control, stage);
		group.add("subgroup_reconverge09", "diverge in for loop", small, control, stage);
		group.add("subgroup_reconverge10", "diverge in for loop and break", small, control, stage);
		group.add("subgroup_reconverge11", "diverge in for loop and continue", small, control, stage);
		group.add("subgroup_reconverge12", "early return, divergent switch", small, control, stage);
		group.add("subgroup_reconverge13", "early return, divergent switch more cases", small, control, stage);
		group.add("subgroup_reconverge14", "divergent switch, some subgroups terminate", small, control, stage);
		group.add("subgroup_reconverge15", "switch in switch", small, control, stage);
		group.add("subgroup_reconverge16", "for loop unequal iterations", small, control, stage);
		group.add("subgroup_reconverge17", "if/else with nested returns", small, control, stage);
		group.add("subgroup_reconverge18", "if/else subgroup all equal", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("subgroup_reconverge19", "if/else subgroup any nested return", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("subgroup_reconverge20", "deeply nested", small, control, stage);
		const char*	group_name = (control ? "large_full_control" : "large_full");
		uniformControlFlowTests->addChild(createTestGroup(testCtx, group_name,
														  "Large Full subgroups",
														  control?addTestsForAmberFiles<true>:addTestsForAmberFiles<false>, group));

		// Partial subgroup.
		group = CaseGroup(data_dir, subdir);
		group.add("subgroup_reconverge_partial00", "if/else diverge", small, control, stage);
		group.add("subgroup_reconverge_partial01", "do while diverge", small, control, stage);
		group.add("subgroup_reconverge_partial02", "while true with break", small, control, stage);
		group.add("subgroup_reconverge_partial03", "if/else diverge, volatile", small, control, stage);
		group.add("subgroup_reconverge_partial04", "early return and if/else diverge", small, control, stage);
		group.add("subgroup_reconverge_partial05", "early return and if/else volatile", small, control, stage);
		group.add("subgroup_reconverge_partial06", "while true with volatile conditional break and early return", small, control, stage);
		group.add("subgroup_reconverge_partial07", "while true return and break", small, control, stage);
		group.add("subgroup_reconverge_partial08", "for loop atomics with conditional break", small, control, stage);
		group.add("subgroup_reconverge_partial09", "diverge in for loop", small, control, stage);
		group.add("subgroup_reconverge_partial10", "diverge in for loop and break", small, control, stage);
		group.add("subgroup_reconverge_partial11", "diverge in for loop and continue", small, control, stage);
		group.add("subgroup_reconverge_partial12", "early return, divergent switch", small, control, stage);
		group.add("subgroup_reconverge_partial13", "early return, divergent switch more cases", small, control, stage);
		group.add("subgroup_reconverge_partial14", "divergent switch, some subgroups terminate", small, control, stage);
		group.add("subgroup_reconverge_partial15", "switch in switch", small, control, stage);
		group.add("subgroup_reconverge_partial16", "for loop unequal iterations", small, control, stage);
		group.add("subgroup_reconverge_partial17", "if/else with nested returns", small, control, stage);
		group.add("subgroup_reconverge_partial18", "if/else subgroup all equal", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("subgroup_reconverge_partial19", "if/else subgroup any nested return", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("subgroup_reconverge_partial20", "deeply nested", small, control, stage);
		group_name = (control ? "large_partial_control" : "large_partial");
		uniformControlFlowTests->addChild(createTestGroup(testCtx, group_name,
														  "Large Partial subgroups",
														  control?addTestsForAmberFiles<true>:addTestsForAmberFiles<false>, group));
	}

	for (unsigned c = 0; c < controls.size(); ++c)
	{
		// Full subgroups.
		bool small = true;
		bool control = controls[c];
		vk::VkShaderStageFlagBits stage = vk::VK_SHADER_STAGE_COMPUTE_BIT;
		const char*	subdir = (control ? small_control_dir : small_dir);
		CaseGroup group(data_dir, subdir);
		group.add("small_subgroup_reconverge00", "if/else diverge", small, control, stage);
		group.add("small_subgroup_reconverge01", "do while diverge", small, control, stage);
		group.add("small_subgroup_reconverge02", "while true with break", small, control, stage);
		group.add("small_subgroup_reconverge03", "if/else diverge, volatile", small, control, stage);
		group.add("small_subgroup_reconverge04", "early return and if/else diverge", small, control, stage);
		group.add("small_subgroup_reconverge05", "early return and if/else volatile", small, control, stage);
		group.add("small_subgroup_reconverge06", "while true with volatile conditional break and early return", small, control, stage);
		group.add("small_subgroup_reconverge07", "while true return and break", small, control, stage);
		group.add("small_subgroup_reconverge08", "for loop atomics with conditional break", small, control, stage);
		group.add("small_subgroup_reconverge09", "diverge in for loop", small, control, stage);
		group.add("small_subgroup_reconverge10", "diverge in for loop and break", small, control, stage);
		group.add("small_subgroup_reconverge11", "diverge in for loop and continue", small, control, stage);
		group.add("small_subgroup_reconverge12", "early return, divergent switch", small, control, stage);
		group.add("small_subgroup_reconverge13", "early return, divergent switch more cases", small, control, stage);
		group.add("small_subgroup_reconverge14", "divergent switch, some subgroups terminate", small, control, stage);
		group.add("small_subgroup_reconverge15", "switch in switch", small, control, stage);
		group.add("small_subgroup_reconverge16", "for loop unequal iterations", small, control, stage);
		group.add("small_subgroup_reconverge17", "if/else with nested returns", small, control, stage);
		group.add("small_subgroup_reconverge18", "if/else subgroup all equal", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("small_subgroup_reconverge19", "if/else subgroup any nested return", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("small_subgroup_reconverge20", "deeply nested", small, control, stage);
		const char*	group_name = (control ? "small_full_control" : "small_full");
		uniformControlFlowTests->addChild(createTestGroup(testCtx, group_name,
														  "Small Full subgroups",
														  control?addTestsForAmberFiles<true>:addTestsForAmberFiles<false>, group));

		// Partial subgroup.
		group = CaseGroup(data_dir, subdir);
		group.add("small_subgroup_reconverge_partial00", "if/else diverge", small, control, stage);
		group.add("small_subgroup_reconverge_partial01", "do while diverge", small, control, stage);
		group.add("small_subgroup_reconverge_partial02", "while true with break", small, control, stage);
		group.add("small_subgroup_reconverge_partial03", "if/else diverge, volatile", small, control, stage);
		group.add("small_subgroup_reconverge_partial04", "early return and if/else diverge", small, control, stage);
		group.add("small_subgroup_reconverge_partial05", "early return and if/else volatile", small, control, stage);
		group.add("small_subgroup_reconverge_partial06", "while true with volatile conditional break and early return", small, control, stage);
		group.add("small_subgroup_reconverge_partial07", "while true return and break", small, control, stage);
		group.add("small_subgroup_reconverge_partial08", "for loop atomics with conditional break", small, control, stage);
		group.add("small_subgroup_reconverge_partial09", "diverge in for loop", small, control, stage);
		group.add("small_subgroup_reconverge_partial10", "diverge in for loop and break", small, control, stage);
		group.add("small_subgroup_reconverge_partial11", "diverge in for loop and continue", small, control, stage);
		group.add("small_subgroup_reconverge_partial12", "early return, divergent switch", small, control, stage);
		group.add("small_subgroup_reconverge_partial13", "early return, divergent switch more cases", small, control, stage);
		group.add("small_subgroup_reconverge_partial14", "divergent switch, some subgroups terminate", small, control, stage);
		group.add("small_subgroup_reconverge_partial15", "switch in switch", small, control, stage);
		group.add("small_subgroup_reconverge_partial16", "for loop unequal iterations", small, control, stage);
		group.add("small_subgroup_reconverge_partial17", "if/else with nested returns", small, control, stage);
		group.add("small_subgroup_reconverge_partial18", "if/else subgroup all equal", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("small_subgroup_reconverge_partial19", "if/else subgroup any nested return", small, control, stage, vk::VK_SUBGROUP_FEATURE_VOTE_BIT);
		group.add("small_subgroup_reconverge_partial20", "deeply nested", small, control, stage);
		group_name = (control ? "small_partial_control" : "small_partial");
		uniformControlFlowTests->addChild(createTestGroup(testCtx, group_name,
														  "Small Partial subgroups",
														  control?addTestsForAmberFiles<true>:addTestsForAmberFiles<false>, group));
	}

	// Discard test
	CaseGroup group(data_dir, "discard");
	group.add("subgroup_reconverge_discard00", "discard test", true, false, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
	uniformControlFlowTests->addChild(createTestGroup(testCtx, "discard",
														"Discard tests",
														addTestsForAmberFiles<false>, group));

	return uniformControlFlowTests.release();
}

} // subgroups
} // vkt
