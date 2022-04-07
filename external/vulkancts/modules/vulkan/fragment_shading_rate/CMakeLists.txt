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
	vktFragmentShadingRatePixelConsistency.hpp
	vktFragmentShadingRatePixelConsistency.cpp
)

set(DEQP_VK_FRAGMENT_SHADING_RATE_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-fragment-shading-rate STATIC ${DEQP_VK_FRAGMENT_SHADING_RATE_SRCS})
target_link_libraries(deqp-vk-fragment-shading-rate ${DEQP_VK_FRAGMENT_SHADING_RATE_LIBS})
