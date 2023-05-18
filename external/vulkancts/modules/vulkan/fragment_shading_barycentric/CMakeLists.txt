include_directories(
        ..
        )

set(DEQP_VK_FRAGMENT_SHADING_BARYCENTRIC_SRCS
	vktFragmentShadingBarycentricTests.cpp
	vktFragmentShadingBarycentricTests.hpp
	)

set(DEQP_VK_FRAGMENT_SHADING_BARYCENTRIC_LIBS
	tcutil
	vkutil
	)

PCH(DEQP_VK_FRAGMENT_SHADING_BARYCENTRIC_SRCS ../pch.cpp)

add_library(deqp-vk-fragment-shading-barycentric STATIC ${DEQP_VK_FRAGMENT_SHADING_BARYCENTRIC_SRCS})
target_link_libraries(deqp-vk-fragment-shading-barycentric ${DEQP_VK_FRAGMENT_SHADING_BARYCENTRIC_LIBS})
