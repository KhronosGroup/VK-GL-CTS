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
#include "vktDrawShaderInvocationTests.hpp"
#include "vktDrawNegativeViewportHeightTests.hpp"
#include "vktDrawInvertedDepthRangesTests.hpp"
#include "vktDrawDifferingInterpolationTests.hpp"
#include "vktDrawShaderLayerTests.hpp"
#include "vktDrawShaderViewportIndexTests.hpp"
#include "vktDrawScissorTests.hpp"
#include "vktDrawMultipleInterpolationTests.hpp"
#include "vktDrawMultisampleLinearInterpolationTests.hpp"
#include "vktDrawDiscardRectanglesTests.hpp"
#include "vktDrawExplicitVertexParameterTests.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktDrawOutputLocationTests.hpp"
#include "vktDrawDepthBiasTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktDrawDepthClampTests.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktDrawAhbTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktDrawMultipleClearsWithinRenderPass.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktDrawMultiExtTests.hpp"
#endif // CTS_USES_VULKANSC

namespace vkt
{
namespace Draw
{

namespace
{

void createChildren (tcu::TestContext& testCtx, tcu::TestCaseGroup* group, bool useDynamicRendering)
{
	group->addChild(new ConcurrentDrawTests						(testCtx, useDynamicRendering));
	group->addChild(new SimpleDrawTests							(testCtx, useDynamicRendering));
	group->addChild(new DrawIndexedTests						(testCtx, useDynamicRendering));
	group->addChild(new IndirectDrawTests						(testCtx, useDynamicRendering));
	group->addChild(createBasicDrawTests						(testCtx, useDynamicRendering));
	group->addChild(new InstancedTests							(testCtx, useDynamicRendering));
	group->addChild(new ShaderDrawParametersTests				(testCtx, useDynamicRendering));
	group->addChild(createNegativeViewportHeightTests			(testCtx, useDynamicRendering));
	group->addChild(createZeroViewportHeightTests				(testCtx, useDynamicRendering));
	group->addChild(createInvertedDepthRangesTests				(testCtx, useDynamicRendering));
	group->addChild(createDifferingInterpolationTests			(testCtx, useDynamicRendering));
	group->addChild(createShaderLayerTests						(testCtx, useDynamicRendering));
	group->addChild(createShaderViewportIndexTests				(testCtx, useDynamicRendering));
	group->addChild(createScissorTests							(testCtx, useDynamicRendering));
	group->addChild(createMultipleInterpolationTests			(testCtx, useDynamicRendering));
	group->addChild(createMultisampleLinearInterpolationTests	(testCtx, useDynamicRendering));
	group->addChild(createDiscardRectanglesTests				(testCtx, useDynamicRendering));
	group->addChild(createExplicitVertexParameterTests			(testCtx, useDynamicRendering));
	group->addChild(createDepthClampTests						(testCtx, useDynamicRendering));
	group->addChild(new MultipleClearsWithinRenderPassTests		(testCtx, useDynamicRendering));
#ifndef CTS_USES_VULKANSC
	group->addChild(createDrawMultiExtTests						(testCtx, useDynamicRendering));

	if (!useDynamicRendering)
	{
		// amber tests - no support for dynamic rendering
		group->addChild(createDepthBiasTests				(testCtx));
		group->addChild(createOutputLocationTests			(testCtx));
		group->addChild(createShaderInvocationTests			(testCtx));

		// subpasses can't be translated to dynamic rendering
		group->addChild(createAhbTests						(testCtx));
	}
#endif // CTS_USES_VULKANSC
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> mainGroup				(new tcu::TestCaseGroup(testCtx, "draw", "Simple Draw tests"));
	de::MovePtr<tcu::TestCaseGroup> renderpassGroup			(new tcu::TestCaseGroup(testCtx, "renderpass", "Draw using render pass object"));
#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup	(new tcu::TestCaseGroup(testCtx, "dynamic_rendering", "Draw using VK_KHR_dynamic_rendering"));
#endif // CTS_USES_VULKANSC

	createChildren(testCtx, renderpassGroup.get(), false);
#ifndef CTS_USES_VULKANSC
	createChildren(testCtx, dynamicRenderingGroup.get(), true);
#endif // CTS_USES_VULKANSC

	mainGroup->addChild(renderpassGroup.release());
#ifndef CTS_USES_VULKANSC
	mainGroup->addChild(dynamicRenderingGroup.release());
#endif // CTS_USES_VULKANSC

	return mainGroup.release();
}

} // Draw
} // vkt
