include_directories(
	..
	../amber
	${DEQP_INL_DIR}
	)

set(DEQP_VK_VKSC_COOPERATIVE_VECTOR_SRCS
	vktCooperativeVectorTests.cpp
	vktCooperativeVectorTests.hpp
	)

set(DEQP_VK_COOPERATIVE_VECTOR_SRCS
	vktCooperativeVectorBasicTests.cpp
	vktCooperativeVectorBasicTests.hpp
	vktCooperativeVectorMatrixTests.cpp
	vktCooperativeVectorMatrixTests.hpp
	vktCooperativeVectorUtils.cpp
	vktCooperativeVectorUtils.hpp
	)

PCH(DEQP_VK_COOPERATIVE_VECTOR_SRCS ../pch.cpp)

add_library(deqp-vk-cooperative_vector STATIC ${DEQP_VK_VKSC_COOPERATIVE_VECTOR_SRCS} ${DEQP_VK_COOPERATIVE_VECTOR_SRCS})
target_link_libraries(deqp-vk-cooperative_vector tcutil vkutil)

add_library(deqp-vksc-cooperative_vector STATIC ${DEQP_VK_VKSC_COOPERATIVE_VECTOR_SRCS})
target_include_directories(deqp-vksc-cooperative_vector BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-cooperative_vector PUBLIC deqp-vksc-util tcutil vkscutil)
