# API layer tests

include_directories(..)

set(DEQP_VK_API_SRCS
	vktApiTests.cpp
	vktApiTests.hpp
	)

set(DEQP_VK_API_LIBS
	tcutil
	vkutil
	)

add_library(deqp-vk-api STATIC ${DEQP_VK_API_SRCS})
target_link_libraries(deqp-vk-api ${DEQP_VK_API_LIBS})
