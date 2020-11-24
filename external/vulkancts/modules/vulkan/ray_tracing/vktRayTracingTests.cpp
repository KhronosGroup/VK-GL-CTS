/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Ray Tracing tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingTests.hpp"
#include "vktRayTracingBuiltinTests.hpp"
#include "vktRayTracingBuildLargeTests.hpp"
#include "vktRayTracingBuildTests.hpp"
#include "vktRayTracingCallableShadersTests.hpp"
#include "vktRayTracingTraceRaysTests.hpp"
#include "vktRayTracingShaderBindingTableTests.hpp"
#include "vktRayTracingTraversalControlTests.hpp"
#include "vktRayTracingAccelerationStructuresTests.hpp"
#include "vktRayTracingBuildIndirectTests.hpp"
#include "vktRayTracingWatertightnessTests.hpp"
#include "vktRayTracingPipelineLibraryTests.hpp"
#include "vktRayTracingMemGuaranteeTests.hpp"
#include "vktRayTracingNullASTests.hpp"
#include "vktRayTracingCaptureReplayTests.hpp"
#include "vktRayTracingMiscTests.hpp"
#include "vktRayTracingComplexControlFlowTests.hpp"
#include "vktRayTracingBarrierTests.hpp"

#include "deUniquePtr.hpp"

#include "tcuTestCase.hpp"

namespace vkt
{
namespace RayTracing
{

tcu::TestCaseGroup*	createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "ray_tracing_pipeline", "Ray tracing tests"));

	group->addChild(createBuiltinTests(testCtx));
	group->addChild(createSpecConstantTests(testCtx));
	group->addChild(createBuildLargeShaderSetTests(testCtx));
	group->addChild(createBuildTests(testCtx));
	group->addChild(createCallableShadersTests(testCtx));
	group->addChild(createTraceRaysTests(testCtx));
	group->addChild(createShaderBindingTableTests(testCtx));
	group->addChild(createTraversalControlTests(testCtx));
	group->addChild(createAccelerationStructuresTests(testCtx));
	group->addChild(createBuildIndirectTests(testCtx));
	group->addChild(createWatertightnessTests(testCtx));
	group->addChild(createPipelineLibraryTests(testCtx));
	group->addChild(createMemGuaranteeTests(testCtx));
	group->addChild(createNullAccelerationStructureTests(testCtx));
	group->addChild(createCaptureReplayTests(testCtx));
	group->addChild(createMiscTests(testCtx));
	group->addChild(createComplexControlFlowTests(testCtx));
	group->addChild(createBarrierTests(testCtx));

	return group.release();
}

}	// RayTracing
}	// vkt
