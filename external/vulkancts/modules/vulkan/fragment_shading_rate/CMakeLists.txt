include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_FRAGMENT_SHADING_RATE_SRCS
	vktAttachmentRateTests.cpp
	vktAttachmentRateTests.hpp
	vktFragmentShadingRateTests.cpp
	vktFragmentShadingRateTests.hpp
	vktFragmentShadingRateBasic.cpp
	vktFragmentShadingRateBasic.hpp
	vktFragmentShadingRatePixelConsistency.cpp
	vktFragmentShadingRatePixelConsistency.hpp
	vktFragmentShadingRateMiscTests.cpp
	vktFragmentShadingRateMiscTests.hpp
)

PCH(DEQP_VK_FRAGMENT_SHADING_RATE_SRCS ../pch.cpp)

add_library(deqp-vk-fragment-shading-rate STATIC ${DEQP_VK_FRAGMENT_SHADING_RATE_SRCS})
target_link_libraries(deqp-vk-fragment-shading-rate tcutil vkutil)

add_library(deqp-vksc-fragment-shading-rate STATIC ${DEQP_VK_FRAGMENT_SHADING_RATE_SRCS})
target_link_libraries(deqp-vksc-fragment-shading-rate PUBLIC tcutil vkscutil)
