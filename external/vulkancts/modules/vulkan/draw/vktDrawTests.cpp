/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawTests.hpp"

#include "vktDrawSimpleTest.hpp"
#include "vktDrawConcurrentTests.hpp"
#include "vktDrawIndexedTest.hpp"
#include "vktDrawIndirectTest.hpp"
#include "vktDrawInstancedTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktBasicDrawTests.hpp"
#include "vktDrawShaderDrawParametersTests.hpp"
#include "vktDrawNegativeViewportHeightTests.hpp"
#include "vktDrawInvertedDepthRangesTests.hpp"
#include "vktDrawDifferingInterpolationTests.hpp"
#include "vktDrawShaderLayerTests.hpp"
#include "vktDrawShaderViewportIndexTests.hpp"
#include "vktDrawScissorTests.hpp"
#include "vktDrawMultipleInterpolationTests.hpp"
#include "vktDrawDiscardRectanglesTests.hpp"
#include "vktDrawExplicitVertexParameterTests.hpp"
#include "vktDrawOutputLocationTests.hpp"
#include "vktDrawDepthClampTests.hpp"

namespace vkt
{
namespace Draw
{

namespace
{

void createChildren (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx		= group->getTestContext();

	group->addChild(new ConcurrentDrawTests				(testCtx));
	group->addChild(new SimpleDrawTests					(testCtx));
	group->addChild(new DrawIndexedTests				(testCtx));
	group->addChild(new IndirectDrawTests				(testCtx));
	group->addChild(createBasicDrawTests				(testCtx));
	group->addChild(new InstancedTests					(testCtx));
	group->addChild(new ShaderDrawParametersTests		(testCtx));
	group->addChild(createNegativeViewportHeightTests	(testCtx));
	group->addChild(createZeroViewportHeightTests		(testCtx));
	group->addChild(createInvertedDepthRangesTests		(testCtx));
	group->addChild(createDifferingInterpolationTests	(testCtx));
	group->addChild(createShaderLayerTests				(testCtx));
	group->addChild(createShaderViewportIndexTests		(testCtx));
	group->addChild(createScissorTests					(testCtx));
	group->addChild(createMultipleInterpolationTests	(testCtx));
	group->addChild(createDiscardRectanglesTests		(testCtx));
	group->addChild(createExplicitVertexParameterTests	(testCtx));
	group->addChild(createOutputLocationTests	        (testCtx));
	group->addChild(createDepthClampTests				(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "draw", "Simple Draw tests", createChildren);
}

} // Draw
} // vkt
