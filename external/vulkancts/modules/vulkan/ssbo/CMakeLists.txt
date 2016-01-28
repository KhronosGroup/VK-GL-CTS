include_directories(
	..
)

set(DEQP_VK_SSBO_SRCS
	vktSSBOLayoutCase.cpp
	vktSSBOLayoutCase.hpp
	vktSSBOLayoutTests.cpp
	vktSSBOLayoutTests.cpp
)

set(DEQP_VK_SSBO_LIBS
	deqp-vk-common
	tcutil
	vkutil
)

add_library(deqp-vk-ssbo STATIC ${DEQP_VK_SSBO_SRCS})
target_link_libraries(deqp-vk-ssbo ${DEQP_VK_SSBO_LIBS})
