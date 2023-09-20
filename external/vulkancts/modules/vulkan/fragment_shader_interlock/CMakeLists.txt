include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_FRAGMENT_SHADER_INTERLOCK_SRCS
	vktFragmentShaderInterlockTests.cpp
	vktFragmentShaderInterlockTests.hpp
	vktFragmentShaderInterlockBasic.cpp
	vktFragmentShaderInterlockBasic.hpp
)

add_library(deqp-vk-fragment-shader-interlock STATIC ${DEQP_VK_FRAGMENT_SHADER_INTERLOCK_SRCS})
target_link_libraries(deqp-vk-fragment-shader-interlock tcutil vkutil)

add_library(deqp-vksc-fragment-shader-interlock STATIC ${DEQP_VK_FRAGMENT_SHADER_INTERLOCK_SRCS})
target_link_libraries(deqp-vksc-fragment-shader-interlock PUBLIC deqp-vksc-util tcutil vkscutil)
