include_directories(..)

set(DEQP_VK_TEXTURE_SRCS
	vktTextureTests.cpp
	vktTextureTests.hpp
	vktTextureTestUtil.cpp
	vktTextureTestUtil.hpp
	vktTextureFilteringTests.cpp
	vktTextureFilteringTests.hpp
	)

set(DEQP_VK_TEXTURE_LIBS
	deqp-vk-common
	tcutil
	vkutil
	)

add_library(deqp-vk-texture STATIC ${DEQP_VK_TEXTURE_SRCS})
target_link_libraries(deqp-vk-texture ${DEQP_VK_TEXTURE_LIBS})
