include_directories(
	..
	../draw
	../util
	${DEQP_INL_DIR}
)

set(DEQP_VK_DYNAMIC_STATE_SRCS
	vktDynamicStateTestCaseUtil.hpp
	vktDynamicStateBaseClass.hpp
	vktDynamicStateBaseClass.cpp
	vktDynamicStateCBTests.cpp
	vktDynamicStateCBTests.hpp
	vktDynamicStateClearTests.cpp
	vktDynamicStateClearTests.hpp
	vktDynamicStateComputeTests.cpp
	vktDynamicStateComputeTests.hpp
	vktDynamicStateDSTests.cpp
	vktDynamicStateDSTests.hpp
	vktDynamicStateGeneralTests.cpp
	vktDynamicStateGeneralTests.hpp
	vktDynamicStateInheritanceTests.cpp
	vktDynamicStateInheritanceTests.hpp
	vktDynamicStateRSTests.cpp
	vktDynamicStateRSTests.hpp
	vktDynamicStateTests.cpp
	vktDynamicStateTests.hpp
	vktDynamicStateVPTests.cpp
	vktDynamicStateVPTests.hpp
	vktDynamicStateDiscardTests.cpp
	vktDynamicStateDiscardTests.hpp
	vktDynamicStateLineWidthTests.cpp
	vktDynamicStateLineWidthTests.hpp
)

PCH(DEQP_VK_DYNAMIC_STATE_SRCS ../pch.cpp)

add_library(deqp-vk-dynamic-state STATIC ${DEQP_VK_DYNAMIC_STATE_SRCS})
target_link_libraries(deqp-vk-dynamic-state deqp-vk-draw tcutil vkutil)

add_library(deqp-vksc-dynamic-state STATIC ${DEQP_VK_DYNAMIC_STATE_SRCS})
target_link_libraries(deqp-vksc-dynamic-state PUBLIC deqp-vksc-draw deqp-vksc-util tcutil vkscutil)
