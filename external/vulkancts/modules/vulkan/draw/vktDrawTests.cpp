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

#include "vktDrawGroupParams.hpp"
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
#include "vktDrawDepthClampTests.hpp"
#include "vktDrawMultipleClearsWithinRenderPass.hpp"
#include "vktDrawSampleAttributeTests.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktDrawOutputLocationTests.hpp"
#include "vktDrawDepthBiasTests.hpp"
#include "vktDrawAhbTests.hpp"
#include "vktDrawMultiExtTests.hpp"
#endif // CTS_USES_VULKANSC

namespace vkt
{
namespace Draw
{

namespace
{

void createChildren (tcu::TestContext& testCtx, tcu::TestCaseGroup* group, const SharedGroupParams groupParams)
{
	group->addChild(new ConcurrentDrawTests						(testCtx, groupParams));
	group->addChild(new SimpleDrawTests							(testCtx, groupParams));
	group->addChild(new DrawIndexedTests						(testCtx, groupParams));
	group->addChild(new IndirectDrawTests						(testCtx, groupParams));
	group->addChild(createBasicDrawTests						(testCtx, groupParams));
	group->addChild(new InstancedTests							(testCtx, groupParams));
	group->addChild(new ShaderDrawParametersTests				(testCtx, groupParams));
	group->addChild(createNegativeViewportHeightTests			(testCtx, groupParams));
	group->addChild(createZeroViewportHeightTests				(testCtx, groupParams));
	group->addChild(createOffScreenViewportTests				(testCtx, groupParams));
	group->addChild(createInvertedDepthRangesTests				(testCtx, groupParams));
	group->addChild(createDifferingInterpolationTests			(testCtx, groupParams));
	group->addChild(createShaderLayerTests						(testCtx, groupParams));
	group->addChild(createShaderViewportIndexTests				(testCtx, groupParams));
	group->addChild(createScissorTests							(testCtx, groupParams));
	group->addChild(createMultipleInterpolationTests			(testCtx, groupParams));
	group->addChild(createMultisampleLinearInterpolationTests	(testCtx, groupParams));
	group->addChild(createDiscardRectanglesTests				(testCtx, groupParams));
	group->addChild(createExplicitVertexParameterTests			(testCtx, groupParams));
	group->addChild(createDepthClampTests						(testCtx, groupParams));
	group->addChild(new MultipleClearsWithinRenderPassTests		(testCtx, groupParams));
	group->addChild(createSampleAttributeTests					(testCtx, groupParams));
	// NOTE: all new draw tests should handle SharedGroupParams

#ifndef CTS_USES_VULKANSC
	group->addChild(createDrawMultiExtTests						(testCtx, groupParams));

	if (!groupParams->useDynamicRendering)
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

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> mainGroup						(new tcu::TestCaseGroup(testCtx, name.c_str(), "Simple Draw tests"));
	de::MovePtr<tcu::TestCaseGroup> renderpassGroup					(new tcu::TestCaseGroup(testCtx, "renderpass", "Draw using renderpass object"));

	createChildren(testCtx, renderpassGroup.get(), SharedGroupParams(
		new GroupParams
		{
			false,			// bool useDynamicRendering;
			false,			// bool useSecondaryCmdBuffer;
			false,			// bool secondaryCmdBufferCompletelyContainsDynamicRenderpass;
		}));
	mainGroup->addChild(renderpassGroup.release());

#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup			(new tcu::TestCaseGroup(testCtx, "dynamic_rendering", "Draw using VK_KHR_dynamic_rendering"));
	de::MovePtr<tcu::TestCaseGroup> drPrimaryCmdBuffGroup			(new tcu::TestCaseGroup(testCtx, "primary_cmd_buff", ""));
	de::MovePtr<tcu::TestCaseGroup> drPartialSecondaryCmdBuffGroup	(new tcu::TestCaseGroup(testCtx, "partial_secondary_cmd_buff", "Secondary command buffer doesn't include begin/endRendering"));
	de::MovePtr<tcu::TestCaseGroup> drCompleteSecondaryCmdBuffGroup	(new tcu::TestCaseGroup(testCtx, "complete_secondary_cmd_buff", "Secondary command buffer contains completely dynamic renderpass"));

	createChildren(testCtx, drPrimaryCmdBuffGroup.get(), SharedGroupParams(
		new GroupParams
		{
			true,			// bool useDynamicRendering;
			false,			// bool useSecondaryCmdBuffer;
			false,			// bool secondaryCmdBufferCompletelyContainsDynamicRenderpass;
		}));
	createChildren(testCtx, drPartialSecondaryCmdBuffGroup.get(), SharedGroupParams(
		new GroupParams
		{
			true,			// bool useDynamicRendering;
			true,			// bool useSecondaryCmdBuffer;
			false,			// bool secondaryCmdBufferCompletelyContainsDynamicRenderpass;
		}));
	createChildren(testCtx, drCompleteSecondaryCmdBuffGroup.get(), SharedGroupParams(
		new GroupParams
		{
			true,			// bool useDynamicRendering;
			true,			// bool useSecondaryCmdBuffer;
			true,			// bool secondaryCmdBufferCompletelyContainsDynamicRenderpass;
		}));

	dynamicRenderingGroup->addChild(drPrimaryCmdBuffGroup.release());
	dynamicRenderingGroup->addChild(drPartialSecondaryCmdBuffGroup.release());
	dynamicRenderingGroup->addChild(drCompleteSecondaryCmdBuffGroup.release());
	mainGroup->addChild(dynamicRenderingGroup.release());
#endif // CTS_USES_VULKANSC

	return mainGroup.release();
}

} // Draw
} // vkt
