include_directories(
	..
)

set(DEQP_VK_SHADERRENDERCASE_SRCS
	vktShaderRenderCase.cpp
	vktShaderRenderCase.hpp
	vktShaderRenderCaseTests.cpp
	vktShaderRenderCaseTests.hpp

	vktTexture.cpp
	vktTexture.hpp

	vktShaderDiscardTests.cpp
	vktShaderDiscardTests.hpp
)

set(DEQP_VK_SHADERRENDERCASE_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-shaderrendercase STATIC ${DEQP_VK_SHADERRENDERCASE_SRCS})
target_link_libraries(deqp-vk-shaderrendercase ${DEQP_VK_SHADERRENDERCASE_LIBS})
