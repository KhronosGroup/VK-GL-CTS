include_directories(
		..
		../amber
		)

set(DEQP_VK_VKSC_RASTERIZATION_SRCS
	vktRasterizationTests.cpp
	vktRasterizationTests.hpp
	vktRasterizationFragShaderSideEffectsTests.cpp
	vktRasterizationFragShaderSideEffectsTests.hpp
	)

set(DEQP_VK_RASTERIZATION_SRCS
	vktRasterizationProvokingVertexTests.cpp
	vktRasterizationProvokingVertexTests.hpp
)

include_directories("../../../../../external/amber/src/include")
add_library(deqp-vk-rasterization STATIC ${DEQP_VK_VKSC_RASTERIZATION_SRCS} ${DEQP_VK_RASTERIZATION_SRCS})
target_link_libraries(deqp-vk-rasterization tcutil vkutil libamber)

add_library(deqp-vksc-rasterization STATIC ${DEQP_VK_VKSC_RASTERIZATION_SRCS})
target_include_directories(deqp-vksc-rasterization BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-rasterization PUBLIC deqp-vksc-util tcutil vkscutil)