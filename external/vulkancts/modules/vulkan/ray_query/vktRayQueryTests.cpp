/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Ray Query tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryTests.hpp"
#include "vktRayQueryBuiltinTests.hpp"
#include "vktRayQueryTraversalControlTests.hpp"
#include "vktRayQueryAccelerationStructuresTests.hpp"
#include "vktRayQueryProceduralGeometryTests.hpp"
#include "vktRayQueryWatertightnessTests.hpp"
#include "vktRayQueryCullRayFlagsTests.hpp"
#include "vktRayQueryMiscTests.hpp"
#include "vktRayQueryDirectionTests.hpp"
#include "vktRayQueryBarycentricCoordinatesTests.hpp"
#include "vktRayQueryNonUniformArgsTests.hpp"

#include "deUniquePtr.hpp"

#include "tcuTestCase.hpp"

namespace vkt
{
namespace RayQuery
{

tcu::TestCaseGroup*	createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "ray_query", "Ray query tests"));

	group->addChild(createBuiltinTests(testCtx));
	group->addChild(createTraversalControlTests(testCtx));
	group->addChild(createAccelerationStructuresTests(testCtx));
	group->addChild(createProceduralGeometryTests(testCtx));
	group->addChild(createAdvancedTests(testCtx));
	group->addChild(createWatertightnessTests(testCtx));
	group->addChild(createCullRayFlagsTests(testCtx));
	group->addChild(createMiscTests(testCtx));
	group->addChild(createDirectionLengthTests(testCtx));
	group->addChild(createInsideAABBsTests(testCtx));
	group->addChild(createBarycentricCoordinatesTests(testCtx));
	group->addChild(createNonUniformArgsTests(testCtx));

	return group.release();
}

}	// RayQuery

}	// vkt
