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

	if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC)
		group->addChild(createDynamicStateComputeTests	(testCtx));
}

static void cleanupGroup(tcu::TestCaseGroup*, vk::PipelineConstructionType)
{
	// Destroy singleton objects.
	cleanupDevice();
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> monolithicGroup			(createTestGroup(testCtx, "monolithic",				"Monolithic pipeline tests",					createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, cleanupGroup));
	de::MovePtr<tcu::TestCaseGroup> pipelineLibraryGroup	(createTestGroup(testCtx, "pipeline_library",		"Graphics pipeline library tests",				createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY, cleanupGroup));
	de::MovePtr<tcu::TestCaseGroup> fastLinkedLibraryGroup	(createTestGroup(testCtx, "fast_linked_library",	"Fast linked graphics pipeline library tests",	createChildren, vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY, cleanupGroup));

	de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "dynamic_state", "Dynamic State Tests"));
	mainGroup->addChild(monolithicGroup.release());
	mainGroup->addChild(pipelineLibraryGroup.release());
	mainGroup->addChild(fastLinkedLibraryGroup.release());
	return mainGroup.release();
}

} // DynamicState
} // vkt
