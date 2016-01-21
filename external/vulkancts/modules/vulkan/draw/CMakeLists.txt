include_directories(..)

set(DEQP_VK_DRAW_SRCS
	vktDrawTests.hpp
	vktDrawTests.cpp
	vktDrawIndexedTest.hpp
	vktDrawIndexedTest.cpp
	vktDrawIndirectTest.hpp
	vktDrawIndirectTest.cpp
	vktDrawSimpleTest.hpp
	vktDrawSimpleTest.cpp
	vktDrawBaseClass.hpp
	vktDrawBaseClass.cpp
	vktDrawCreateInfoUtil.hpp
	vktDrawCreateInfoUtil.cpp
	vktDrawImageObjectUtil.hpp
	vktDrawImageObjectUtil.cpp
	vktDrawBufferObjectUtil.hpp
	vktDrawBufferObjectUtil.cpp
	vktDrawTestCaseUtil.hpp
)

set(DEQP_VK_DRAW_LIBS
	deqp-vk-common
	tcutil
	vkutil
)

add_library(deqp-vk-draw STATIC ${DEQP_VK_DRAW_SRCS})
target_link_libraries(deqp-vk-draw ${DEQP_VK_DRAW_LIBS})
