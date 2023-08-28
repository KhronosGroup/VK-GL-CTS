# Robustness tests

include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_ROBUSTNESS_SRCS
	vktRobustnessTests.cpp
	vktRobustnessTests.hpp
	vktRobustnessBufferAccessTests.cpp
	vktRobustnessBufferAccessTests.hpp
	vktRobustBufferAccessWithVariablePointersTests.cpp
	vktRobustBufferAccessWithVariablePointersTests.hpp
	vktRobustnessUtil.cpp
	vktRobustnessUtil.hpp
	vktRobustnessVertexAccessTests.cpp
	vktRobustnessVertexAccessTests.hpp
	vktRobustnessIndexAccessTests.cpp
	vktRobustnessIndexAccessTests.hpp
	vktNonRobustBufferAccessTests.hpp
	vktNonRobustBufferAccessTests.cpp
	vktRobustnessExtsTests.cpp
	vktRobustnessExtsTests.hpp
	vktRobustness1VertexAccessTests.cpp
	vktRobustness1VertexAccessTests.hpp
	)

PCH(DEQP_VK_ROBUSTNESS_SRCS ../pch.cpp)

add_library(deqp-vk-robustness STATIC ${DEQP_VK_ROBUSTNESS_SRCS})
target_link_libraries(deqp-vk-robustness tcutil vkutil)

add_library(deqp-vksc-robustness STATIC ${DEQP_VK_ROBUSTNESS_SRCS})
target_link_libraries(deqp-vksc-robustness PUBLIC deqp-vksc-util tcutil vkscutil)
