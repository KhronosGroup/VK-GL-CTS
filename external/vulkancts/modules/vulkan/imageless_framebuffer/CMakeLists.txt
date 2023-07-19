include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS
	vktImagelessFramebufferTests.hpp
	vktImagelessFramebufferTests.cpp
)

PCH(DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS ../pch.cpp)

add_library(deqp-vk-imageless-framebuffer STATIC ${DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS})
target_link_libraries(deqp-vk-imageless-framebuffer tcutil vkutil)

add_library(deqp-vksc-imageless-framebuffer STATIC ${DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS})
target_link_libraries(deqp-vksc-imageless-framebuffer PUBLIC deqp-vksc-util tcutil vkscutil)
