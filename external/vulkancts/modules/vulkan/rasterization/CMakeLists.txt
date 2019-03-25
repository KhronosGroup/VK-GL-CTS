include_directories(
		..
		../amber
		../../../../amber/src/include
		)

set(DEQP_VK_RASTERIZATION_SRCS
	vktRasterizationTests.cpp
	vktRasterizationTests.hpp
	)

set(DEQP_VK_RASTERIZATION_LIBS
	tcutil
	vkutil
	libamber
	)

add_library(deqp-vk-rasterization STATIC ${DEQP_VK_RASTERIZATION_SRCS})
target_link_libraries(deqp-vk-rasterization ${DEQP_VK_RASTERIZATION_LIBS})
