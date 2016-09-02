include_directories(..)

set(DEQP_VK_FRAGMENT_OPS_SRCS
	vktFragmentOperationsTests.cpp
	vktFragmentOperationsTests.hpp
	vktFragmentOperationsScissorTests.cpp
	vktFragmentOperationsScissorTests.hpp
	vktFragmentOperationsScissorMultiViewportTests.cpp
	vktFragmentOperationsScissorMultiViewportTests.hpp
	vktFragmentOperationsMakeUtil.cpp
	vktFragmentOperationsMakeUtil.hpp
	)

set(DEQP_VK_FRAGMENT_OPS_LIBS
	deqp-vk-common
	tcutil
	vkutil
	)

add_library(deqp-vk-fragment-ops STATIC ${DEQP_VK_FRAGMENT_OPS_SRCS})
target_link_libraries(deqp-vk-fragment-ops ${DEQP_VK_FRAGMENT_OPS_LIBS})
