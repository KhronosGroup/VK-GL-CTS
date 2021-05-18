include_directories(..)

set(DEQP_VK_DRAW_SRCS
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
	vktDrawOutputLocationTests.cpp
	vktDrawOutputLocationTests.hpp
	vktDrawDepthClampTests.cpp
	vktDrawDepthClampTests.hpp
	vktDrawAhbTests.cpp
	vktDrawAhbTests.hpp
	vktDrawMultipleClearsWithinRenderPass.cpp
	vktDrawMultipleClearsWithinRenderPass.hpp
)

set(DEQP_VK_DRAW_LIBS
	tcutil
	vkutil
)

PCH(DEQP_VK_DRAW_SRCS ../pch.cpp)

add_library(deqp-vk-draw STATIC ${DEQP_VK_DRAW_SRCS})
target_link_libraries(deqp-vk-draw ${DEQP_VK_DRAW_LIBS})
