# Robustness tests

include_directories(..)

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
	)

set(DEQP_VK_ROBUSTNESS_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_ROBUSTNESS_SRCS ../pch.cpp)

add_library(deqp-vk-robustness STATIC ${DEQP_VK_ROBUSTNESS_SRCS})
target_link_libraries(deqp-vk-robustness ${DEQP_VK_ROBUSTNESS_LIBS})

