include_directories(
	..
)

set(DEQP_VK_SHADERRENDER_SRCS
	vktShaderRender.cpp
	vktShaderRender.hpp
	vktShaderRenderTests.cpp
	vktShaderRenderTests.hpp

	vktShaderRenderDiscardTests.cpp
	vktShaderRenderDiscardTests.hpp
)

set(DEQP_VK_SHADERRENDER_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-shaderrender STATIC ${DEQP_VK_SHADERRENDER_SRCS})
target_link_libraries(deqp-vk-shaderrender ${DEQP_VK_SHADERRENDER_LIBS})
