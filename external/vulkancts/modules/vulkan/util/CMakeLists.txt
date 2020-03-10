include_directories(
	..
)

set(DEQP_VK_UTIL_SRCS
	vktDrawUtil.cpp
	vktDrawUtil.hpp
	vktExternalMemoryUtil.cpp
	vktExternalMemoryUtil.hpp
	vktConstexprVectorUtil.hpp
)

set(DEQP_VK_UTIL_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-util STATIC ${DEQP_VK_UTIL_SRCS})
target_link_libraries(deqp-vk-util ${DEQP_VK_UTIL_LIBS})
