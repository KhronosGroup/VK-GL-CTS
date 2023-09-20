/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
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
 * \brief Pipeline Derivative Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineDerivativeTests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "deUniquePtr.hpp"
#include "deMemory.h"
#include "tcuTestLog.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

void checkSupport (Context& context, bool useMaintenance5)
{
	if (useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

void initComputeDerivativePrograms (SourceCollections& sources, bool)
{
	std::ostringstream computeSource;

	// Trivial do-nothing compute shader
	computeSource <<
		"#version 310 es\n"
		"layout(local_size_x=1) in;\n"
		"void main (void)\n"
		"{\n"
		"}\n";

	sources.glslSources.add("comp") << glu::ComputeSource(computeSource.str());
}

tcu::TestStatus testComputeDerivativeByHandle (Context& context, bool useMaintenance5)
{
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkDevice				vkDevice		= context.getDevice();
	Move<VkShaderModule>		shaderModule	= createShaderModule(vk, vkDevice, context.getBinaryCollection().get("comp"), 0);

	Move<VkPipelineLayout>		layout			= makePipelineLayout(vk, vkDevice);

	VkComputePipelineCreateInfo	cpci			= {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			shaderModule.get(),
			"main",
			DE_NULL
		},
		layout.get(),
		0,
		-1
	};

#ifndef CTS_USES_VULKANSC
	VkPipelineCreateFlags2CreateInfoKHR flags2CreateInfo = initVulkanStructure();
	if (useMaintenance5)
	{
		flags2CreateInfo.flags	= VK_PIPELINE_CREATE_2_ALLOW_DERIVATIVES_BIT_KHR;
		cpci.flags				= 0;
		cpci.pNext				= &flags2CreateInfo;
	}
#else
	DE_UNREF(useMaintenance5);
#endif // CTS_USES_VULKANSC

	Move<VkPipeline>			basePipeline	= createComputePipeline(vk, vkDevice, DE_NULL, &cpci);

	// Create second (identical) pipeline based on first
	cpci.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	cpci.basePipelineHandle = basePipeline.get();

#ifndef CTS_USES_VULKANSC
	if (useMaintenance5)
	{
		flags2CreateInfo.flags	= VK_PIPELINE_CREATE_2_DERIVATIVE_BIT_KHR;
		cpci.flags				= 0;
	}
#endif // CTS_USES_VULKANSC

	Move<VkPipeline>			derivedPipeline	= createComputePipeline(vk, vkDevice, DE_NULL, &cpci);

	// If we got here without crashing, success.
	return tcu::TestStatus::pass("OK");
}

tcu::TestStatus testComputeDerivativeByIndex (Context& context, bool)
{
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkDevice				vkDevice		= context.getDevice();
	Move<VkShaderModule>		shaderModule	= createShaderModule(vk, vkDevice, context.getBinaryCollection().get("comp"), 0);

	Move<VkPipelineLayout>		layout			= makePipelineLayout(vk, vkDevice);

	VkComputePipelineCreateInfo	cpci[2]			= { {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			shaderModule.get(),
			"main",
			DE_NULL
		},
		layout.get(),
		0,
		-1
	}, {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		VK_PIPELINE_CREATE_DERIVATIVE_BIT,
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			shaderModule.get(),
			"main",
			DE_NULL
		},
		layout.get(),
		0,
		0,
	} };

	std::vector<VkPipeline>		rawPipelines(2);
	vk.createComputePipelines(vkDevice, 0, 2, cpci, DE_NULL, rawPipelines.data());

	for (deUint32 i = 0; i < rawPipelines.size(); i++) {
		vk.destroyPipeline(vkDevice, rawPipelines[i], DE_NULL);
	}

	// If we got here without crashing, success.
	return tcu::TestStatus::pass("OK");
}

} // anonymous

tcu::TestCaseGroup* createDerivativeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> derivativeTests (new tcu::TestCaseGroup(testCtx, "derivative", "pipeline derivative tests"));
	de::MovePtr<tcu::TestCaseGroup> computeTests (new tcu::TestCaseGroup(testCtx, "compute", "compute tests"));

	addFunctionCaseWithPrograms(computeTests.get(),
								"derivative_by_handle",
								"",
								initComputeDerivativePrograms,
								testComputeDerivativeByHandle,
								false);
#ifndef CTS_USES_VULKANSC
	addFunctionCaseWithPrograms(computeTests.get(),
								"derivative_by_handle_maintenance5",
								"",
								checkSupport,
								initComputeDerivativePrograms,
								testComputeDerivativeByHandle,
								true);
#endif // CTS_USES_VULKANSC
	addFunctionCaseWithPrograms(computeTests.get(),
								"derivative_by_index",
								"",
								initComputeDerivativePrograms,
								testComputeDerivativeByIndex,
								false);

	derivativeTests->addChild(computeTests.release());
	return derivativeTests.release();
}

} // pipeline

} // vkt
