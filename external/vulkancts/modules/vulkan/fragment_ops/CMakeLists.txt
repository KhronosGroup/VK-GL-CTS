include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_FRAGMENT_OPS_SRCS
	vktFragmentOperationsEarlyFragmentTests.cpp
	vktFragmentOperationsEarlyFragmentTests.hpp
	vktFragmentOperationsOcclusionQueryTests.cpp
	vktFragmentOperationsOcclusionQueryTests.hpp
	vktFragmentOperationsScissorMultiViewportTests.cpp
	vktFragmentOperationsScissorMultiViewportTests.hpp
	vktFragmentOperationsScissorTests.cpp
	vktFragmentOperationsScissorTests.hpp
	vktFragmentOperationsTests.cpp
	vktFragmentOperationsTests.hpp
	vktFragmentOperationsTransientAttachmentTests.cpp
	vktFragmentOperationsTransientAttachmentTests.hpp
	)

PCH(DEQP_VK_FRAGMENT_OPS_SRCS ../pch.cpp)

add_library(deqp-vk-fragment-ops STATIC ${DEQP_VK_FRAGMENT_OPS_SRCS})
target_link_libraries(deqp-vk-fragment-ops tcutil vkutil)

add_library(deqp-vksc-fragment-ops STATIC ${DEQP_VK_FRAGMENT_OPS_SRCS})
target_link_libraries(deqp-vksc-fragment-ops PUBLIC deqp-vksc-util tcutil vkscutil)
