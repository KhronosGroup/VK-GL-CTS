include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_UTIL_SRCS
	vktDrawUtil.cpp
	vktDrawUtil.hpp
	vktExternalMemoryUtil.cpp
	vktExternalMemoryUtil.hpp
	vktConstexprVectorUtil.hpp
)

add_library(deqp-vk-util STATIC ${DEQP_VK_UTIL_SRCS})
target_link_libraries(deqp-vk-util PUBLIC tcutil vkutil)

add_library(deqp-vksc-util STATIC ${DEQP_VK_UTIL_SRCS})
target_link_libraries(deqp-vksc-util PUBLIC tcutil vkscutil)
