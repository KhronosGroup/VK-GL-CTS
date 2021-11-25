/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization tests
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktSynchronizationSmokeTests.hpp"
#include "vktSynchronizationBasicFenceTests.hpp"
#include "vktSynchronizationBasicSemaphoreTests.hpp"
#include "vktSynchronizationBasicEventTests.hpp"
#include "vktSynchronizationOperationSingleQueueTests.hpp"
#include "vktSynchronizationOperationMultiQueueTests.hpp"
#include "vktSynchronizationInternallySynchronizedObjectsTests.hpp"
#include "vktSynchronizationCrossInstanceSharingTests.hpp"
#include "vktSynchronizationSignalOrderTests.hpp"
#include "vktSynchronizationTimelineSemaphoreTests.hpp"
#include "vktSynchronizationWin32KeyedMutexTests.hpp"
#include "vktSynchronizationNoneStageTests.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktSynchronizationImageLayoutTransitionTests.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace synchronization
{

namespace
{

tcu::TestCaseGroup* createBasicTests (tcu::TestContext& testCtx, SynchronizationType type)
{
	de::MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "basic", ""));

	if (type == SynchronizationType::LEGACY)
	{
		group->addChild(createBasicEventTests(testCtx));
		group->addChild(createBasicFenceTests(testCtx));
	}
	else
	{
		group->addChild(createSynchronization2BasicEventTests(testCtx));
	}

	group->addChild(createBasicBinarySemaphoreTests		(testCtx, type));
	group->addChild(createBasicTimelineSemaphoreTests	(testCtx, type));

	return group.release();
}

class OperationTests : public tcu::TestCaseGroup
{
public:
	OperationTests (tcu::TestContext& testCtx, SynchronizationType type)
		: tcu::TestCaseGroup(testCtx, "op", "Synchronization of a memory-modifying operation")
		, m_type(type)
	{
	}

	void init (void)
	{
		addChild(createSynchronizedOperationSingleQueueTests(m_testCtx, m_type, m_pipelineCacheData));
		addChild(createSynchronizedOperationMultiQueueTests (m_testCtx, m_type, m_pipelineCacheData));
	}

private:
	SynchronizationType	m_type;

	// synchronization.op tests share pipeline cache data to speed up test execution.
	PipelineCacheData	m_pipelineCacheData;
};

tcu::TestCaseGroup* createTestsInternal (tcu::TestContext& testCtx, SynchronizationType type)
{
	const bool		isSynchronization2	(type == SynchronizationType::SYNCHRONIZATION2);
	const char*		groupName[]			{ "synchronization",		"synchronization2" };
	const char*		groupDescription[]	{ "Synchronization tests",	"VK_KHR_synchronization2 tests" };

	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, groupName[isSynchronization2], groupDescription[isSynchronization2]));

	if (isSynchronization2)
	{
		testGroup->addChild(createSynchronization2SmokeTests(testCtx));
		testGroup->addChild(createSynchronization2TimelineSemaphoreTests(testCtx));
		testGroup->addChild(createNoneStageTests(testCtx));
		testGroup->addChild(createImageLayoutTransitionTests(testCtx));
	}
	else // legacy synchronization
	{
		testGroup->addChild(createSmokeTests(testCtx));
		testGroup->addChild(createTimelineSemaphoreTests(testCtx));

		testGroup->addChild(createInternallySynchronizedObjects(testCtx));
		testGroup->addChild(createWin32KeyedMutexTest(testCtx));
	}

	testGroup->addChild(createBasicTests(testCtx, type));
	testGroup->addChild(new OperationTests(testCtx, type));
	testGroup->addChild(createCrossInstanceSharingTest(testCtx, type));
	testGroup->addChild(createSignalOrderTests(testCtx, type));

	return testGroup.release();
}

} // anonymous

} // synchronization

tcu::TestCaseGroup* createSynchronizationTests (tcu::TestContext& testCtx)
{
	using namespace synchronization;
	return createTestsInternal(testCtx, SynchronizationType::LEGACY);
}

tcu::TestCaseGroup* createSynchronization2Tests(tcu::TestContext& testCtx)
{
	using namespace synchronization;
	return createTestsInternal(testCtx, SynchronizationType::SYNCHRONIZATION2);
}

} // vkt
