/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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
 */ /*!
 * \file
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "vktSubgroupsTests.hpp"
#include "vktSubgroupsBuiltinVarTests.hpp"
#include "vktSubgroupsBuiltinMaskVarTests.hpp"
#include "vktSubgroupsBasicTests.hpp"
#include "vktSubgroupsVoteTests.hpp"
#include "vktSubgroupsBallotTests.hpp"
#include "vktSubgroupsBallotBroadcastTests.hpp"
#include "vktSubgroupsBallotOtherTests.hpp"
#include "vktSubgroupsArithmeticTests.hpp"
#include "vktSubgroupsClusteredTests.hpp"
#include "vktSubgroupsShuffleTests.hpp"
#include "vktSubgroupsQuadTests.hpp"
#include "vktSubgroupsShapeTests.hpp"
#include "vktSubgroupsBallotMasksTests.hpp"
#include "vktSubgroupsSizeControlTests.hpp"
#ifndef CTS_USES_VULKANSC
#include "vktSubgroupsPartitionedTests.hpp"
#include "vktSubgroupUniformControlFlowTests.hpp"
#endif // CTS_USES_VULKANSC
#include "vktSubgroupsMultipleDispatchesUniformSubgroupSizeTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace subgroups
{

namespace
{

void createChildren(tcu::TestCaseGroup* subgroupsTests)
{
	tcu::TestContext& testCtx = subgroupsTests->getTestContext();

	subgroupsTests->addChild(createSubgroupsBuiltinVarTests(testCtx));
	subgroupsTests->addChild(createSubgroupsBuiltinMaskVarTests(testCtx));
	subgroupsTests->addChild(createSubgroupsBasicTests(testCtx));
	subgroupsTests->addChild(createSubgroupsVoteTests(testCtx));
	subgroupsTests->addChild(createSubgroupsBallotTests(testCtx));
	subgroupsTests->addChild(createSubgroupsBallotBroadcastTests(testCtx));
	subgroupsTests->addChild(createSubgroupsBallotOtherTests(testCtx));
	subgroupsTests->addChild(createSubgroupsArithmeticTests(testCtx));
	subgroupsTests->addChild(createSubgroupsClusteredTests(testCtx));
#ifndef CTS_USES_VULKANSC
	subgroupsTests->addChild(createSubgroupsPartitionedTests(testCtx));
#endif
	subgroupsTests->addChild(createSubgroupsShuffleTests(testCtx));
	subgroupsTests->addChild(createSubgroupsQuadTests(testCtx));
	subgroupsTests->addChild(createSubgroupsShapeTests(testCtx));
	subgroupsTests->addChild(createSubgroupsBallotMasksTests(testCtx));
	subgroupsTests->addChild(createMultipleDispatchesUniformSubgroupSizeTests(testCtx));
	subgroupsTests->addChild(createSubgroupsSizeControlTests(testCtx));
#ifndef CTS_USES_VULKANSC
	subgroupsTests->addChild(createSubgroupUniformControlFlowTests(testCtx));
#endif // CTS_USES_VULKANSC
}

} // anonymous

tcu::TestCaseGroup* createTests(tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(
			   testCtx, name.c_str(), "Subgroups tests", createChildren);
}

} // subgroups
} // vkt
