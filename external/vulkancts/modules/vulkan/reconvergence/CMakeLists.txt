include_directories(
	..
	${DEQP_INL_DIR}
    ../amber
	)

set(DEQP_VK_RECONVERGENCE_SRCS
	vktReconvergenceTests.cpp
	vktReconvergenceTests.hpp
    vktReconvergenceTerminateInvocationTests.cpp
    vktReconvergenceTerminateInvocationTests.hpp
	)

set(DEQP_VK_RECONVERGENCE_LIBS
	tcutil
	vkutil
    libamber
	)

PCH(DEQP_VK_RECONVERGENCE_SRCS ../pch.cpp)

add_library(deqp-vk-reconvergence STATIC ${DEQP_VK_RECONVERGENCE_SRCS})
target_link_libraries(deqp-vk-reconvergence ${DEQP_VK_RECONVERGENCE_LIBS})
