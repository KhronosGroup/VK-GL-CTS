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

set(DEQP_VK_FRAGMENT_SHADER_INTERLOCK_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-fragment-shader-interlock STATIC ${DEQP_VK_FRAGMENT_SHADER_INTERLOCK_SRCS})
target_link_libraries(deqp-vk-fragment-shader-interlock ${DEQP_VK_FRAGMENT_SHADER_INTERLOCK_LIBS})
