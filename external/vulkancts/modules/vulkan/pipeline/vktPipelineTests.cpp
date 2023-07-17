/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Pipeline Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineTests.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineStencilTests.hpp"
#include "vktPipelineBlendTests.hpp"
#include "vktPipelineDepthTests.hpp"
#include "vktPipelineDescriptorLimitsTests.hpp"
#include "vktPipelineDynamicOffsetTests.hpp"
#include "vktPipelineDynamicVertexAttributeTests.hpp"
#include "vktPipelineEarlyDestroyTests.hpp"
#include "vktPipelineLogicOpTests.hpp"
#include "vktPipelineImageTests.hpp"
#include "vktPipelineInputAssemblyTests.hpp"
#include "vktPipelineInterfaceMatchingTests.hpp"
#include "vktPipelineSamplerTests.hpp"
#include "vktPipelineImageViewTests.hpp"
#include "vktPipelineImage2DViewOf3DTests.hpp"
#include "vktPipelinePushConstantTests.hpp"
#include "vktPipelinePushDescriptorTests.hpp"
#include "vktPipelineSpecConstantTests.hpp"
#include "vktPipelineMatchedAttachmentsTests.hpp"
#include "vktPipelineMultisampleTests.hpp"
#include "vktPipelineMultisampleInterpolationTests.hpp"
#include "vktPipelineMultisampleShaderBuiltInTests.hpp"
#include "vktPipelineVertexInputTests.hpp"
#include "vktPipelineTimestampTests.hpp"
#include "vktPipelineCacheTests.hpp"
#include "vktPipelineRenderToImageTests.hpp"
#include "vktPipelineFramebufferAttachmentTests.hpp"
#include "vktPipelineStencilExportTests.hpp"
#include "vktPipelineCreationFeedbackTests.hpp"
#include "vktPipelineDepthRangeUnrestrictedTests.hpp"
#include "vktPipelineExecutablePropertiesTests.hpp"
#include "vktPipelineMiscTests.hpp"
#include "vktPipelineMaxVaryingsTests.hpp"
#include "vktPipelineBlendOperationAdvancedTests.hpp"
#include "vktPipelineExtendedDynamicStateTests.hpp"
#include "vktPipelineDynamicControlPoints.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktPipelineCreationCacheControlTests.hpp"
#include "vktPipelineBindPointTests.hpp"
#include "vktPipelineDerivativeTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktPipelineNoPositionTests.hpp"
#include "vktPipelineColorWriteEnableTests.hpp"
#include "vktPipelineLibraryTests.hpp"
#include "vktPipelineAttachmentFeedbackLoopLayoutTests.hpp"
#include "vktPipelineShaderModuleIdentifierTests.hpp"
#include "vktPipelineImageSlicedViewOf3DTests.hpp"
#include "vktPipelineBindVertexBuffers2Tests.hpp"
#include "vktPipelineRobustnessCacheTests.hpp"
#include "vktPipelineInputAttributeOffsetTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

void createChildren (tcu::TestCaseGroup* group, PipelineConstructionType pipelineConstructionType)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(createDynamicControlPointTests		(testCtx, pipelineConstructionType));
	group->addChild(createStencilTests					(testCtx, pipelineConstructionType));
	group->addChild(createBlendTests					(testCtx, pipelineConstructionType));
	group->addChild(createDepthTests					(testCtx, pipelineConstructionType));
	group->addChild(createDescriptorLimitsTests			(testCtx, pipelineConstructionType));
	group->addChild(createDynamicOffsetTests			(testCtx, pipelineConstructionType));
	group->addChild(createDynamicVertexAttributeTests	(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createEarlyDestroyTests				(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC
	group->addChild(createImageTests					(testCtx, pipelineConstructionType));
	group->addChild(createSamplerTests					(testCtx, pipelineConstructionType));
	group->addChild(createImageViewTests				(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createImage2DViewOf3DTests			(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC
	group->addChild(createLogicOpTests					(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createPushConstantTests				(testCtx, pipelineConstructionType));
	group->addChild(createPushDescriptorTests			(testCtx, pipelineConstructionType));
	group->addChild(createMatchedAttachmentsTests		(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC
	group->addChild(createSpecConstantTests				(testCtx, pipelineConstructionType));
	group->addChild(createMultisampleTests				(testCtx, pipelineConstructionType, false));
	group->addChild(createMultisampleTests				(testCtx, pipelineConstructionType, true));
	group->addChild(createMultisampleInterpolationTests	(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	// Input attachments aren't supported for dynamic rendering and shader objects
	if (!vk::isConstructionTypeShaderObject(pipelineConstructionType))
	{
		group->addChild(createMultisampleShaderBuiltInTests(testCtx, pipelineConstructionType));
	}
#endif // CTS_USES_VULKANSC
	group->addChild(createTestGroup						(testCtx, "vertex_input", "", createVertexInputTests, pipelineConstructionType));
	group->addChild(createInputAssemblyTests			(testCtx, pipelineConstructionType));
	group->addChild(createInterfaceMatchingTests		(testCtx, pipelineConstructionType));
	group->addChild(createTimestampTests				(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createCacheTests					(testCtx, pipelineConstructionType));
	group->addChild(createFramebufferAttachmentTests	(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC
	group->addChild(createRenderToImageTests			(testCtx, pipelineConstructionType));
	group->addChild(createStencilExportTests			(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createCreationFeedbackTests			(testCtx, pipelineConstructionType));
	group->addChild(createDepthRangeUnrestrictedTests	(testCtx, pipelineConstructionType));
	if (!isConstructionTypeShaderObject(pipelineConstructionType))
	{
		group->addChild(createExecutablePropertiesTests(testCtx, pipelineConstructionType));
	}
#endif // CTS_USES_VULKANSC
	group->addChild(createMaxVaryingsTests				(testCtx, pipelineConstructionType));
	group->addChild(createBlendOperationAdvancedTests	(testCtx, pipelineConstructionType));
	group->addChild(createExtendedDynamicStateTests		(testCtx, pipelineConstructionType));
	group->addChild(createNoPositionTests				(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createBindPointTests				(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC
	group->addChild(createColorWriteEnableTests			(testCtx, pipelineConstructionType));
#ifndef CTS_USES_VULKANSC
	group->addChild(createAttachmentFeedbackLoopLayoutTests (testCtx, pipelineConstructionType));
	if (!isConstructionTypeShaderObject(pipelineConstructionType))
	{
		group->addChild(createShaderModuleIdentifierTests	(testCtx, pipelineConstructionType));
	}
	group->addChild(createPipelineRobustnessCacheTests	(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC
	group->addChild(createColorWriteEnable2Tests		(testCtx, pipelineConstructionType));
	group->addChild(createMiscTests						(testCtx, pipelineConstructionType));
	group->addChild(createCmdBindBuffers2Tests			(testCtx, pipelineConstructionType));
	group->addChild(createInputAttributeOffsetTests		(testCtx, pipelineConstructionType));

	// NOTE: all new pipeline tests should use GraphicsPipelineWrapper for pipeline creation
	// ShaderWrapper for shader creation
	// PipelineLayoutWrapper for pipeline layout creation
	// RenderPassWrapper for render pass creation

	if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
	{
#ifndef CTS_USES_VULKANSC
		// compute pipeline tests should not be repeated basing on pipelineConstructionType
		group->addChild(createDerivativeTests				(testCtx));

		// dont repeat tests requiring timing execution of vkCreate*Pipelines
		group->addChild(createCacheControlTests				(testCtx));

		// No need to repeat tests checking sliced view of 3D images for different construction types.
		group->addChild(createImageSlicedViewOf3DTests		(testCtx));
#endif // CTS_USES_VULKANSC
	}
#ifndef CTS_USES_VULKANSC
	else if (pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY)
	{
		// execute pipeline library specific tests only once
		group->addChild(createPipelineLibraryTests		(testCtx));
	}
#endif // CTS_USES_VULKANSC
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> monolithicGroup					(createTestGroup(testCtx, "monolithic",						"Monolithic pipeline tests",					createChildren, PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC));
	de::MovePtr<tcu::TestCaseGroup> pipelineLibraryGroup			(createTestGroup(testCtx, "pipeline_library",				"Graphics pipeline library tests",				createChildren, PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY));
	de::MovePtr<tcu::TestCaseGroup> fastLinkedLibraryGroup			(createTestGroup(testCtx, "fast_linked_library",			"Fast linked graphics pipeline library tests",	createChildren, PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectUnlinkedSpirvGroup	(createTestGroup(testCtx, "shader_object_unlinked_spirv",	"Unlinked spirv shader object tests",			createChildren, PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectUnlinkedBinaryGroup	(createTestGroup(testCtx, "shader_object_unlinked_binary",	"Unlinked binary shader object tests",			createChildren, PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectLinkedSpirvGroup	(createTestGroup(testCtx, "shader_object_linked_spirv",		"Linked spirv shader object tests",				createChildren, PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectLinkedBinaryGroup	(createTestGroup(testCtx, "shader_object_linked_binary",	"Linked binary shader object tests",			createChildren, PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY));

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), "Pipeline Tests"));
	mainGroup->addChild(monolithicGroup.release());
	mainGroup->addChild(pipelineLibraryGroup.release());
	mainGroup->addChild(fastLinkedLibraryGroup.release());
	mainGroup->addChild(shaderObjectUnlinkedSpirvGroup.release());
	mainGroup->addChild(shaderObjectUnlinkedBinaryGroup.release());
	mainGroup->addChild(shaderObjectLinkedSpirvGroup.release());
	mainGroup->addChild(shaderObjectLinkedBinaryGroup.release());
	return mainGroup.release();
}

} // pipeline
} // vkt
