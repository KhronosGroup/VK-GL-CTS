include_directories(
		..
		../amber
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

include_directories("../../../../../external/amber/src/include")
add_library(deqp-vk-rasterization STATIC ${DEQP_VK_RASTERIZATION_SRCS})
target_link_libraries(deqp-vk-rasterization ${DEQP_VK_RASTERIZATION_LIBS})
