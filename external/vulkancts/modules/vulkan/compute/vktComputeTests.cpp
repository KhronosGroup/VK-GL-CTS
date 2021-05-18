/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
#include "vktComputeWorkgroupMemoryExplicitLayoutTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace compute
{

namespace
{

void createChildren (tcu::TestCaseGroup* computeTests)
{
	tcu::TestContext&	testCtx		= computeTests->getTestContext();

	computeTests->addChild(createBasicComputeShaderTests(testCtx));
	computeTests->addChild(createBasicDeviceGroupComputeShaderTests(testCtx));
	computeTests->addChild(createCooperativeMatrixTests(testCtx));
	computeTests->addChild(createIndirectComputeDispatchTests(testCtx));
	computeTests->addChild(createComputeShaderBuiltinVarTests(testCtx));
	computeTests->addChild(createZeroInitializeWorkgroupMemoryTests(testCtx));
	computeTests->addChild(createWorkgroupMemoryExplicitLayoutTests(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "compute", "Compute shader tests", createChildren);
}

} // compute
} // vkt
