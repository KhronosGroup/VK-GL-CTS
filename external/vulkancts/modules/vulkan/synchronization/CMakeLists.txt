include_directories(
	..
	../util
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_SYNCHRONIZATION_SRCS
	vktSynchronizationDefs.hpp
	vktSynchronizationTests.cpp
	vktSynchronizationTests.hpp
	vktSynchronizationUtil.cpp
	vktSynchronizationUtil.hpp
	vktSynchronizationSmokeTests.cpp
	vktSynchronizationSmokeTests.hpp
	vktSynchronizationBasicFenceTests.cpp
	vktSynchronizationBasicFenceTests.hpp
	vktSynchronizationBasicSemaphoreTests.cpp
	vktSynchronizationBasicSemaphoreTests.hpp
	vktSynchronizationBasicEventTests.cpp
	vktSynchronizationBasicEventTests.hpp
	vktSynchronizationOperationSingleQueueTests.cpp
	vktSynchronizationOperationSingleQueueTests.hpp
	vktSynchronizationOperationMultiQueueTests.cpp
	vktSynchronizationOperationMultiQueueTests.hpp
	vktSynchronizationOperation.cpp
	vktSynchronizationOperation.hpp
	vktSynchronizationInternallySynchronizedObjectsTests.cpp
	vktSynchronizationInternallySynchronizedObjectsTests.hpp
	vktSynchronizationOperationTestData.hpp
	vktSynchronizationTimelineSemaphoreTests.cpp
	vktSynchronizationTimelineSemaphoreTests.hpp
	vktSynchronizationImageLayoutTransitionTests.cpp
	vktSynchronizationImageLayoutTransitionTests.cpp
	)

set(DEQP_VK_SYNCHRONIZATION_SRCS
	vktSynchronizationCrossInstanceSharingTests.cpp
	vktSynchronizationCrossInstanceSharingTests.hpp
	vktSynchronizationNoneStageTests.cpp
	vktSynchronizationNoneStageTests.hpp
	vktSynchronizationSignalOrderTests.cpp
	vktSynchronizationSignalOrderTests.hpp
	vktSynchronizationWin32KeyedMutexTests.cpp
	vktSynchronizationWin32KeyedMutexTests.hpp
    vktGlobalPriorityQueueTests.cpp
    vktGlobalPriorityQueueTests.hpp
    vktGlobalPriorityQueueUtils.cpp
    vktGlobalPriorityQueueUtils.hpp
	)

PCH(DEQP_VK_SYNCHRONIZATION_SRCS ../pch.cpp)

add_library(deqp-vk-synchronization STATIC ${DEQP_VK_VKSC_SYNCHRONIZATION_SRCS} ${DEQP_VK_SYNCHRONIZATION_SRCS})
target_link_libraries(deqp-vk-synchronization tcutil vkutil)

add_library(deqp-vksc-synchronization STATIC ${DEQP_VK_VKSC_SYNCHRONIZATION_SRCS})
target_link_libraries(deqp-vksc-synchronization PUBLIC deqp-vksc-util tcutil vkscutil)
