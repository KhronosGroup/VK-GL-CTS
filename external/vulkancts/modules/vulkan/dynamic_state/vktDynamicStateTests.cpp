/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Dynamic State Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateTests.hpp"

#include "vktDynamicStateVPTests.hpp"
#include "vktDynamicStateRSTests.hpp"
#include "vktDynamicStateCBTests.hpp"
#include "vktDynamicStateDSTests.hpp"
#include "vktDynamicStateGeneralTests.hpp"
#include "vktDynamicStateComputeTests.hpp"
#include "vktDynamicStateInheritanceTests.hpp"
#include "vktDynamicStateClearTests.hpp"
#include "vktDynamicStateDiscardTests.hpp"
#include "vktDynamicStateLineWidthTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace DynamicState
{

namespace
{

void createChildren (tcu::TestCaseGroup* group, vk::PipelineConstructionType pipelineConstructionType)
{
	tcu::TestContext&	testCtx		= group->getTestContext();

	group->addChild(new DynamicStateVPTests				(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateRSTests				(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateCBTests				(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateDSTests				(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateGeneralTests		(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateInheritanceTests	(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateClearTests			(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateDiscardTests		(testCtx, pipelineConstructionType));
	group->addChild(new DynamicStateLWTests				(testCtx, pipelineConstructionType));

	if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC || pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV)
		group->addChild(createDynamicStateComputeTests	(testCtx, pipelineConstructionType));
}

void cleanupGroup(tcu::TestCaseGroup*)
{
	// Destroy singleton objects.
	cleanupDevice();
}

void initDynamicStateTestGroup (tcu::TestCaseGroup* mainGroup)
{
	auto& testCtx = mainGroup->getTestContext();

	de::MovePtr<tcu::TestCaseGroup> monolithicGroup					(createTestGroup(testCtx, "monolithic",						"Monolithic pipeline tests",					createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC));
	de::MovePtr<tcu::TestCaseGroup> pipelineLibraryGroup			(createTestGroup(testCtx, "pipeline_library",				"Graphics pipeline library tests",				createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY));
	de::MovePtr<tcu::TestCaseGroup> fastLinkedLibraryGroup			(createTestGroup(testCtx, "fast_linked_library",			"Fast linked graphics pipeline library tests",	createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectUnlinkedSpirvGroup	(createTestGroup(testCtx, "shader_object_unlinked_spirv",	"Unlinked spirv shader object tests",			createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectUnlinkedBinaryGroup	(createTestGroup(testCtx, "shader_object_unlinked_binary",	"Unlinked binary shader object tests",			createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_BINARY));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectLinkedSpirvGroup	(createTestGroup(testCtx, "shader_object_linked_spirv",		"Linked spirv shader object tests",				createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_SPIRV));
	de::MovePtr<tcu::TestCaseGroup> shaderObjectLinkedBinaryGroup	(createTestGroup(testCtx, "shader_object_linked_binary",	"Linked binary shader object tests",			createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY));

	mainGroup->addChild(monolithicGroup.release());
	mainGroup->addChild(pipelineLibraryGroup.release());
	mainGroup->addChild(fastLinkedLibraryGroup.release());
	mainGroup->addChild(shaderObjectUnlinkedSpirvGroup.release());
	mainGroup->addChild(shaderObjectUnlinkedBinaryGroup.release());
	mainGroup->addChild(shaderObjectLinkedSpirvGroup.release());
	mainGroup->addChild(shaderObjectLinkedBinaryGroup.release());
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "Dynamic State Tests", initDynamicStateTestGroup, cleanupGroup);
}

} // DynamicState
} // vkt
