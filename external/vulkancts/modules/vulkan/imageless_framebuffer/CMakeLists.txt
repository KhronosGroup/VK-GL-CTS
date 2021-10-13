include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS
	vktImagelessFramebufferTests.hpp
	vktImagelessFramebufferTests.cpp
)

set(DEQP_VK_IMAGELESS_FRAMEBUFFER_LIBS
	tcutil
	vkutil
)

PCH(DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS ../pch.cpp)

add_library(deqp-vk-imageless-framebuffer STATIC ${DEQP_VK_IMAGELESS_FRAMEBUFFER_SRCS})
target_link_libraries(deqp-vk-imageless-framebuffer ${DEQP_VK_IMAGELESS_FRAMEBUFFER_LIBS})
