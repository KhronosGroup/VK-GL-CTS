/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Safety critical tests
 *//*--------------------------------------------------------------------*/

#include "vktSafetyCriticalTests.hpp"

#include "vktTestGroupUtil.hpp"

#include "vktSafetyCriticalApiTests.hpp"
#include "vktDeviceObjectReservationTests.hpp"
#include "vktPipelineIdentifierTests.hpp"
#include "vktPipelineCacheSCTests.hpp"
#include "vktFaultHandlingTests.hpp"
#include "vktCommandPoolMemoryReservationTests.hpp"
#include "vktObjectRefreshTests.hpp"
#include "vktApplicationParametersTests.hpp"

namespace vkt
{
namespace sc
{

namespace
{

void createChildren (tcu::TestCaseGroup* scTests)
{
	tcu::TestContext&	testCtx		= scTests->getTestContext();

	scTests->addChild(createSafetyCriticalAPITests				(testCtx));
	scTests->addChild(createDeviceObjectReservationTests		(testCtx));
	scTests->addChild(createPipelineIdentifierTests				(testCtx));
	scTests->addChild(createPipelineCacheTests					(testCtx));
	scTests->addChild(createFaultHandlingTests					(testCtx));
	scTests->addChild(createCommandPoolMemoryReservationTests	(testCtx));
	scTests->addChild(createObjectRefreshTests					(testCtx));
	scTests->addChild(createApplicationParametersTests			(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "sc", "SC Device Creation Tests", createChildren);
}

} // sc
} // vkt
