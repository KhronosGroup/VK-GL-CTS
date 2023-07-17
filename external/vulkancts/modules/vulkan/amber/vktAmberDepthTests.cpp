/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Valve Corporation.
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
 * \brief Amber tests in the GLSL group.
 *//*--------------------------------------------------------------------*/

#include "vktAmberDepthTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkQueryUtil.hpp"

#include "tcuCommandLine.hpp"

#include <vector>
#include <utility>
#include <string>

namespace vkt
{
namespace cts_amber
{

using namespace vk;

de::SharedPtr<Move<vk::VkDevice>> g_singletonDeviceDepthGroup;

class DepthTestCase : public AmberTestCase
{
	bool m_useCustomDevice;

public:
	DepthTestCase  (tcu::TestContext&	testCtx,
					const char*			name,
					const char*			description,
					bool				useCustomDevice,
					const std::string&	readFilename)
		:	AmberTestCase(testCtx, name, description, readFilename),
			m_useCustomDevice(useCustomDevice)
	{ }

	TestInstance* createInstance (Context& ctx) const
	{
		// Create a custom device to ensure that VK_EXT_depth_range_unrestricted is not enabled
		if (!g_singletonDeviceDepthGroup && m_useCustomDevice)
		{
			const float queuePriority = 1.0f;

			// Create a universal queue that supports graphics and compute
			const VkDeviceQueueCreateInfo queueParams =
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// sType
				DE_NULL,									// pNext
				0u,											// flags
				ctx.getUniversalQueueFamilyIndex(),			// queueFamilyIndex
				1u,											// queueCount
				&queuePriority								// pQueuePriorities
			};

			const char *ext = "VK_EXT_depth_clamp_zero_one";

			VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();

			VkPhysicalDeviceDepthClampZeroOneFeaturesEXT clampParams =
			{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT,	// sType
				DE_NULL,																// pNext
				VK_TRUE,																// depthClampZeroOne
			};

			features2.pNext = &clampParams;

			const auto&	vki				= ctx.getInstanceInterface();
			const auto	physicalDevice	= ctx.getPhysicalDevice();

			ctx.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
			vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);

			const VkDeviceCreateInfo deviceCreateInfo =
			{
				VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,		// sType
				&features2,									// pNext
				(VkDeviceCreateFlags)0u,					// flags
				1,											// queueRecordCount
				&queueParams,								// pRequestedQueues
				0,											// layerCount
				DE_NULL,									// ppEnabledLayerNames
				1,											// enabledExtensionCount
				&ext,										// ppEnabledExtensionNames
				DE_NULL,									// pEnabledFeatures
			};

			const bool		validation	= ctx.getTestContext().getCommandLine().isValidationEnabled();
			Move<VkDevice>	device		= createCustomDevice(validation, ctx.getPlatformInterface(), ctx.getInstance(), vki, physicalDevice, &deviceCreateInfo);

			g_singletonDeviceDepthGroup = de::SharedPtr<Move<VkDevice>>(new Move<VkDevice>(device));
		}
		return new AmberTestInstance(ctx, m_recipe, m_useCustomDevice ? g_singletonDeviceDepthGroup->get() : nullptr);
	}
};

struct TestInfo
{
	std::string					name;
	std::string					desc;
	std::vector<std::string>	base_required_features;
	bool						unrestricted;
};

DepthTestCase* createDepthTestCase (tcu::TestContext&   testCtx,
									const TestInfo&		testInfo,
									const char*			category,
									const std::string&	filename)

{
	// shader_test files are saved in <path>/external/vulkancts/data/vulkan/amber/<categoryname>/
	std::string readFilename("vulkan/amber/");
	readFilename.append(category);
	readFilename.append("/");
	readFilename.append(filename);

	DepthTestCase *testCase = new DepthTestCase(testCtx, testInfo.name.c_str(), testInfo.desc.c_str(), !testInfo.unrestricted, readFilename);

	for (auto req : testInfo.base_required_features)
		testCase->addRequirement(req);

	if (testInfo.unrestricted)
		testCase->addRequirement("VK_EXT_depth_range_unrestricted");

	return testCase;
}

static void createTests(tcu::TestCaseGroup *g)
{
	static const std::vector<TestInfo>	tests		=
	{
		{ "fs_clamp",						"Test fragment shader depth value clamping",					{ "VK_EXT_depth_clamp_zero_one", "Features.fragmentStoresAndAtomics", "Features.depthClamp" },	false },
		{ "out_of_range",					"Test late clamping of out-of-range depth values",				{ "VK_EXT_depth_clamp_zero_one" },																false },
		{ "ez_fs_clamp",					"Test fragment shader depth value with early fragment tests",	{ "VK_EXT_depth_clamp_zero_one", "Features.fragmentStoresAndAtomics", "Features.depthClamp" },	false },
		{ "bias_fs_clamp",					"Test fragment shader depth value with depthBias enabled",		{ "VK_EXT_depth_clamp_zero_one", "Features.fragmentStoresAndAtomics", "Features.depthClamp" },	false },
		{ "bias_outside_range",				"Test biasing depth values out of the depth range",				{ "VK_EXT_depth_clamp_zero_one", "Features.fragmentStoresAndAtomics" },							false },
		{ "bias_outside_range_fs_clamp",	"Test fragment shader depth value when biasing out of range",	{ "VK_EXT_depth_clamp_zero_one", "Features.fragmentStoresAndAtomics" },							false },

		// Rerun any tests that will get different results with VK_EXT_depth_range_unrestricted
		{ "out_of_range_unrestricted",					"Test late clamping of out-of-range depth values",				{ "VK_EXT_depth_clamp_zero_one" },										true },
		{ "bias_outside_range_fs_clamp_unrestricted",	"Test fragment shader depth value when biasing out of range",	{ "VK_EXT_depth_clamp_zero_one", "Features.fragmentStoresAndAtomics" },	true },
	};

   tcu::TestContext& testCtx = g->getTestContext();

	for (const auto& test : tests)
	{
		g->addChild(createDepthTestCase(testCtx, test, g->getName(), test.name + ".amber"));
	}
}

static void cleanupGroup(tcu::TestCaseGroup*)
{
	// Destroy custom device object
	g_singletonDeviceDepthGroup.clear();
}

tcu::TestCaseGroup*	createAmberDepthGroup (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "Depth pipeline test group", createTests, cleanupGroup);
}

} // cts_amber
} // vkt
