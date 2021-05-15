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
#include "vktDrawAhbTests.hpp"
#include "vktDrawMultipleClearsWithinRenderPass.hpp"
#include "vktDrawMultiExtTests.hpp"

namespace vkt
{
namespace Draw
{

namespace
{

tcu::TestCaseGroup* createTestsInternal (tcu::TestContext& testCtx, bool useDynamicRendering)
{
	const char*		groupName[]			{ "draw",				"draw_with_dynamic_rendering" };
	const char*		groupDescription[]	{ "Simple Draw tests",	"Simple Draw tests using VK_KHR_dynamic_rendering" };

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, groupName[useDynamicRendering], groupDescription[useDynamicRendering]));

	group->addChild(new ConcurrentDrawTests					(testCtx, useDynamicRendering));
	group->addChild(new SimpleDrawTests						(testCtx, useDynamicRendering));
	group->addChild(new DrawIndexedTests					(testCtx, useDynamicRendering));
	group->addChild(new IndirectDrawTests					(testCtx, useDynamicRendering));
	group->addChild(createBasicDrawTests					(testCtx, useDynamicRendering));
	group->addChild(new InstancedTests						(testCtx, useDynamicRendering));
	group->addChild(new ShaderDrawParametersTests			(testCtx, useDynamicRendering));
	group->addChild(createNegativeViewportHeightTests		(testCtx, useDynamicRendering));
	group->addChild(createZeroViewportHeightTests			(testCtx, useDynamicRendering));
	group->addChild(createInvertedDepthRangesTests			(testCtx, useDynamicRendering));
	group->addChild(createDifferingInterpolationTests		(testCtx, useDynamicRendering));
	group->addChild(createShaderLayerTests					(testCtx, useDynamicRendering));
	group->addChild(createShaderViewportIndexTests			(testCtx, useDynamicRendering));
	group->addChild(createScissorTests						(testCtx, useDynamicRendering));
	group->addChild(createMultipleInterpolationTests		(testCtx, useDynamicRendering));
	group->addChild(createDiscardRectanglesTests			(testCtx, useDynamicRendering));
	group->addChild(createExplicitVertexParameterTests		(testCtx, useDynamicRendering));
	group->addChild(createDepthClampTests					(testCtx, useDynamicRendering));
	group->addChild(new MultipleClearsWithinRenderPassTests	(testCtx, useDynamicRendering));
	group->addChild(createDrawMultiExtTests					(testCtx, useDynamicRendering));

	if (!useDynamicRendering)
	{
		// amber tests - no support for dynamic rendering
		group->addChild(createOutputLocationTests			(testCtx));

		// subpasses can't be translated to dynamic rendering
		group->addChild(createAhbTests						(testCtx));
	}

	return group.release();
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestsInternal(testCtx, false);
}

tcu::TestCaseGroup* createDynamicRenderingTests (tcu::TestContext& testCtx)
{
	return createTestsInternal(testCtx, true);
}

} // Draw
} // vkt
