include_directories(..)

set(DEQP_VK_VKSC_DRAW_SRCS
	vktDrawTests.hpp
	vktDrawTests.cpp
	vktDrawIndexedTest.hpp
	vktDrawIndexedTest.cpp
	vktDrawIndirectTest.hpp
	vktDrawIndirectTest.cpp
	vktDrawInstancedTests.hpp
	vktDrawInstancedTests.cpp
	vktDrawInvertedDepthRangesTests.hpp
	vktDrawInvertedDepthRangesTests.cpp
	vktDrawShaderDrawParametersTests.hpp
	vktDrawShaderDrawParametersTests.cpp
	vktDrawShaderLayerTests.hpp
	vktDrawShaderLayerTests.cpp
	vktDrawShaderViewportIndexTests.hpp
	vktDrawShaderViewportIndexTests.cpp
	vktDrawSimpleTest.hpp
	vktDrawSimpleTest.cpp
	vktDrawConcurrentTests.hpp
	vktDrawConcurrentTests.cpp
	vktDrawNegativeViewportHeightTests.hpp
	vktDrawNegativeViewportHeightTests.cpp
	vktDrawBaseClass.hpp
	vktDrawBaseClass.cpp
	vktDrawCreateInfoUtil.hpp
	vktDrawCreateInfoUtil.cpp
	vktDrawImageObjectUtil.hpp
	vktDrawImageObjectUtil.cpp
	vktDrawBufferObjectUtil.hpp
	vktDrawBufferObjectUtil.cpp
	vktDrawTestCaseUtil.hpp
	vktBasicDrawTests.hpp
	vktBasicDrawTests.cpp
	vktDrawDifferingInterpolationTests.hpp
	vktDrawDifferingInterpolationTests.cpp
	vktDrawScissorTests.hpp
	vktDrawScissorTests.cpp
	vktDrawMultipleInterpolationTests.hpp
	vktDrawMultipleInterpolationTests.cpp
	vktDrawDiscardRectanglesTests.hpp
	vktDrawDiscardRectanglesTests.cpp
	vktDrawExplicitVertexParameterTests.cpp
	vktDrawExplicitVertexParameterTests.hpp
	vktDrawDepthClampTests.cpp
	vktDrawDepthClampTests.hpp
	vktDrawMultipleClearsWithinRenderPass.cpp
	vktDrawMultipleClearsWithinRenderPass.hpp
)

set(DEQP_VK_DRAW_SRCS
	vktDrawOutputLocationTests.cpp
	vktDrawOutputLocationTests.hpp
	vktDrawAhbTests.cpp
	vktDrawAhbTests.hpp
	vktDrawMultiExtTests.hpp
	vktDrawMultiExtTests.cpp
)

PCH(DEQP_VK_DRAW_SRCS ../pch.cpp)

add_library(deqp-vk-draw STATIC ${DEQP_VK_VKSC_DRAW_SRCS} ${DEQP_VK_DRAW_SRCS})
target_link_libraries(deqp-vk-draw tcutil vkutil)

add_library(deqp-vksc-draw STATIC ${DEQP_VK_VKSC_DRAW_SRCS})
target_include_directories(deqp-vksc-draw BEFORE PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkansc)
target_link_libraries(deqp-vksc-draw PUBLIC deqp-vksc-util tcutil vkscutil)
