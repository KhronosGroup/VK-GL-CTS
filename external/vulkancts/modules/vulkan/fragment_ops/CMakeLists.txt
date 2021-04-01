include_directories(..)

set(DEQP_VK_FRAGMENT_OPS_SRCS
	vktFragmentOperationsEarlyFragmentTests.cpp
	vktFragmentOperationsEarlyFragmentTests.hpp
	vktFragmentOperationsMakeUtil.cpp
	vktFragmentOperationsMakeUtil.hpp
	vktFragmentOperationsScissorMultiViewportTests.cpp
	vktFragmentOperationsScissorMultiViewportTests.hpp
	vktFragmentOperationsScissorTests.cpp
	vktFragmentOperationsScissorTests.hpp
	vktFragmentOperationsTests.cpp
	vktFragmentOperationsTests.hpp
	)

PCH(DEQP_VK_FRAGMENT_OPS_SRCS ../pch.cpp)

add_library(deqp-vk-fragment-ops STATIC ${DEQP_VK_FRAGMENT_OPS_SRCS})
target_link_libraries(deqp-vk-fragment-ops tcutil vkutil)

add_library(deqp-vksc-fragment-ops STATIC ${DEQP_VK_FRAGMENT_OPS_SRCS})
target_include_directories(deqp-vksc-fragment-ops BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-fragment-ops PUBLIC deqp-vksc-util tcutil vkscutil)
