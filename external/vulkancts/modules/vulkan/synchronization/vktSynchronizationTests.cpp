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
#include "vktGlobalPriorityQueueTests.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace synchronization
{

namespace
{

tcu::TestCaseGroup* createBasicTests (tcu::TestContext& testCtx, SynchronizationType type, VideoCodecOperationFlags videoCodecOperation)
{
	de::MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "basic", ""));

	if (type == SynchronizationType::LEGACY)
	{
		group->addChild(createBasicEventTests(testCtx, videoCodecOperation));
		group->addChild(createBasicFenceTests(testCtx, videoCodecOperation));
	}
	else
	{
		group->addChild(createSynchronization2BasicEventTests(testCtx, videoCodecOperation));
	}

	group->addChild(createBasicBinarySemaphoreTests		(testCtx, type, videoCodecOperation));
	group->addChild(createBasicTimelineSemaphoreTests	(testCtx, type, videoCodecOperation));

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

const std::pair<std::string, std::string> getGroupName (SynchronizationType type, const std::string& name, VideoCodecOperationFlags videoCodecOperation)
{
	if (videoCodecOperation == 0)
	{
		const bool	isSynchronization2	(type == SynchronizationType::SYNCHRONIZATION2);
		const char*	groupDescription[]	{ "Synchronization tests",	"VK_KHR_synchronization2 tests" };

		return std::pair<std::string, std::string>(name, groupDescription[isSynchronization2]);
	}

#ifndef CTS_USES_VULKANSC
	return std::pair<std::string, std::string>(name, "");
#else
	TCU_THROW(InternalError, "Video support is not implemented in Vulkan SC");
#endif
}

tcu::TestCaseGroup* createTestsInternal (tcu::TestContext& testCtx, SynchronizationType type, const std::string& name, VideoCodecOperationFlags videoCodecOperation)
{
	const bool									isSynchronization2	(type == SynchronizationType::SYNCHRONIZATION2);
	const std::pair<std::string, std::string>	groupName			= getGroupName(type, name, videoCodecOperation);

	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, groupName.first.c_str(), groupName.second.c_str()));

	if (videoCodecOperation == 0)
	{
		if (isSynchronization2)
		{
			testGroup->addChild(createSynchronization2SmokeTests(testCtx));
			testGroup->addChild(createSynchronization2TimelineSemaphoreTests(testCtx));
#ifndef CTS_USES_VULKANSC
			testGroup->addChild(createNoneStageTests(testCtx));
#endif // CTS_USES_VULKANSC
			testGroup->addChild(createImageLayoutTransitionTests(testCtx));
		}
		else // legacy synchronization
		{
			testGroup->addChild(createSmokeTests(testCtx));
			testGroup->addChild(createTimelineSemaphoreTests(testCtx));

			testGroup->addChild(createInternallySynchronizedObjects(testCtx));
#ifndef CTS_USES_VULKANSC
			testGroup->addChild(createWin32KeyedMutexTest(testCtx));
			testGroup->addChild(createGlobalPriorityQueueTests(testCtx));
#endif // CTS_USES_VULKANSC
		}
	}

	testGroup->addChild(createBasicTests(testCtx, type, videoCodecOperation));

	if (videoCodecOperation == 0)
	{
		testGroup->addChild(new OperationTests(testCtx, type));
#ifndef CTS_USES_VULKANSC
		testGroup->addChild(createCrossInstanceSharingTest(testCtx, type));
		testGroup->addChild(createSignalOrderTests(testCtx, type));
#endif // CTS_USES_VULKANSC
	}

	return testGroup.release();
}
} // anonymous

} // synchronization

tcu::TestCaseGroup* createSynchronizationTests(tcu::TestContext& testCtx, const std::string& name)
{
	return createSynchronizationTests(testCtx, name, 0u);
}

tcu::TestCaseGroup* createSynchronization2Tests(tcu::TestContext& testCtx, const std::string& name)
{
	return createSynchronization2Tests(testCtx, name, 0u);
}

tcu::TestCaseGroup* createSynchronizationTests (tcu::TestContext& testCtx, const std::string& name, synchronization::VideoCodecOperationFlags videoCodecOperation)
{
	using namespace synchronization;

	de::MovePtr<tcu::TestCaseGroup> testGroup	(createTestsInternal(testCtx, SynchronizationType::LEGACY, name, videoCodecOperation));

	return testGroup.release();
}

tcu::TestCaseGroup* createSynchronization2Tests (tcu::TestContext& testCtx, const std::string& name, synchronization::VideoCodecOperationFlags videoCodecOperation)
{
	using namespace synchronization;

	de::MovePtr<tcu::TestCaseGroup> testGroup(createTestsInternal(testCtx, SynchronizationType::SYNCHRONIZATION2, name, videoCodecOperation));

	return testGroup.release();
}

} // vkt
