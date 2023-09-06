include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_UBO_SRCS
	vktUniformBlockCase.cpp
	vktUniformBlockCase.hpp
	vktRandomUniformBlockCase.cpp
	vktRandomUniformBlockCase.hpp
	vktUniformBlockTests.cpp
	vktUniformBlockTests.hpp
)

PCH(DEQP_VK_UBO_SRCS ../pch.cpp)

add_library(deqp-vk-ubo STATIC ${DEQP_VK_UBO_SRCS})
target_link_libraries(deqp-vk-ubo tcutil vkutil)

add_library(deqp-vksc-ubo STATIC ${DEQP_VK_UBO_SRCS})
target_link_libraries(deqp-vksc-ubo PUBLIC deqp-vksc-util tcutil vkscutil)
