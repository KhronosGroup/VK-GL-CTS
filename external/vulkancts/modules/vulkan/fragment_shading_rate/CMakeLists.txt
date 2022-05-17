include_directories(
	..
	${DEQP_INL_DIR}
)


set(DEQP_VK_VKSC_FRAGMENT_SHADING_RATE_SRCS
	vktAttachmentRateTests.cpp
	vktAttachmentRateTests.hpp
	vktFragmentShadingRateTests.cpp
	vktFragmentShadingRateTests.hpp
	vktFragmentShadingRatePixelConsistency.hpp
	vktFragmentShadingRatePixelConsistency.cpp
	vktFragmentShadingRateGroupParams.hpp
)

set(DEQP_VK_FRAGMENT_SHADING_RATE_SRCS
	vktFragmentShadingRateBasic.cpp
	vktFragmentShadingRateBasic.hpp
)

PCH(DEQP_VK_FRAGMENT_SHADING_RATE_SRCS ../pch.cpp)

add_library(deqp-vk-fragment-shading-rate STATIC ${DEQP_VK_VKSC_FRAGMENT_SHADING_RATE_SRCS} ${DEQP_VK_FRAGMENT_SHADING_RATE_SRCS})
target_link_libraries(deqp-vk-fragment-shading-rate tcutil vkutil)

add_library(deqp-vksc-fragment-shading-rate STATIC ${DEQP_VK_VKSC_FRAGMENT_SHADING_RATE_SRCS})
target_include_directories(deqp-vksc-fragment-shading-rate BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-fragment-shading-rate PUBLIC tcutil vkscutil)
