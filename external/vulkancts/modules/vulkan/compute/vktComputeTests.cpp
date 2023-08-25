/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Compute Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeTests.hpp"
#include "vktComputeBasicComputeShaderTests.hpp"
#include "vktComputeCooperativeMatrixTests.hpp"
#include "vktComputeIndirectComputeDispatchTests.hpp"
#include "vktComputeShaderBuiltinVarTests.hpp"
#include "vktComputeZeroInitializeWorkgroupMemoryTests.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktComputeWorkgroupMemoryExplicitLayoutTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktTestGroupUtil.hpp"
#include "vkComputePipelineConstructionUtil.hpp"

namespace vkt
{
namespace compute
{

using namespace vk;

namespace
{

void createChildren (tcu::TestCaseGroup* computeTests, ComputePipelineConstructionType computePipelineConstructionType)
{
	tcu::TestContext&	testCtx		= computeTests->getTestContext();

	computeTests->addChild(createBasicComputeShaderTests(testCtx, computePipelineConstructionType));
	computeTests->addChild(createBasicDeviceGroupComputeShaderTests(testCtx, computePipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	computeTests->addChild(createCooperativeMatrixTests(testCtx, computePipelineConstructionType));
#endif
	computeTests->addChild(createIndirectComputeDispatchTests(testCtx, computePipelineConstructionType));
	computeTests->addChild(createComputeShaderBuiltinVarTests(testCtx, computePipelineConstructionType));
	computeTests->addChild(createZeroInitializeWorkgroupMemoryTests(testCtx, computePipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	computeTests->addChild(createWorkgroupMemoryExplicitLayoutTests(testCtx, computePipelineConstructionType));
#endif // CTS_USES_VULKANSC
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> pipelineGroup			(createTestGroup(testCtx, "pipeline", "Compute pipeline tests", createChildren, COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup> shaderObjectSpirvGroup	(createTestGroup(testCtx, "shader_object_spirv", "Compute spirv shader object tests", createChildren, COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_SPIRV));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectBinaryGroup	(createTestGroup(testCtx, "shader_object_binary", "Compute binary shader object tests", createChildren, COMPUTE_PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_BINARY));
#endif

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), "Compute shader tests"));
	mainGroup->addChild(pipelineGroup.release());
#ifndef CTS_USES_VULKANSC
	mainGroup->addChild(shaderObjectSpirvGroup.release());
	mainGroup->addChild(shaderObjectBinaryGroup.release());
#endif
	return mainGroup.release();
}

} // compute
} // vkt
