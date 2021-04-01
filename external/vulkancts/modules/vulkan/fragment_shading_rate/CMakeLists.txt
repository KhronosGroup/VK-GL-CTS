include_directories(..)

set(DEQP_VK_FRAGMENT_SHADING_RATE_SRCS
	vktFragmentShadingRateTests.cpp
	vktFragmentShadingRateTests.hpp
	vktFragmentShadingRateBasic.cpp
	vktFragmentShadingRateBasic.hpp
	vktFragmentShadingRatePixelConsistency.hpp
	vktFragmentShadingRatePixelConsistency.cpp
)

add_library(deqp-vk-fragment-shading-rate STATIC ${DEQP_VK_FRAGMENT_SHADING_RATE_SRCS})
target_link_libraries(deqp-vk-fragment-shading-rate tcutil vkutil)

add_library(deqp-vksc-fragment-shading-rate STATIC ${DEQP_VK_FRAGMENT_SHADING_RATE_SRCS})
target_include_directories(deqp-vksc-fragment-shading-rate BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-fragment-shading-rate PUBLIC tcutil vkscutil)
