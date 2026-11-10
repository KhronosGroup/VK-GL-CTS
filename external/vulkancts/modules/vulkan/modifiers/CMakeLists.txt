include_directories(
	..
	../image
	../util
	)

set(DEQP_VK_MODIFIERS_SRCS
	vktModifiersTests.cpp
	vktModifiersTests.hpp
	)

set(DEQP_VK_MODIFIERS_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_MODIFIERS_SRCS ../pch.cpp)

add_library(deqp-vk-modifiers STATIC ${DEQP_VK_MODIFIERS_SRCS})
target_link_libraries(deqp-vk-modifiers ${DEQP_VK_MODIFIERS_LIBS})
