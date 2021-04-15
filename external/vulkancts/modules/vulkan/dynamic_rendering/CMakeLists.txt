include_directories(
	..
	../draw
)

set(DEQP_VK_DYNAMIC_RENDERING_SRCS
	vktDynamicRenderingTests.hpp
	vktDynamicRenderingTests.cpp
)

set(DEQP_VK_DYNAMIC_RENDERING_LIBS
	deqp-vk-draw
	tcutil
	vkutil
)

PCH(DEQP_VK_DYNAMIC_RENDERING_SRCS ../pch.cpp)

add_library(deqp-vk-dynamic-rendering STATIC ${DEQP_VK_DYNAMIC_RENDERING_SRCS})
target_link_libraries(deqp-vk-dynamic-rendering ${DEQP_VK_DYNAMIC_RENDERING_LIBS})
