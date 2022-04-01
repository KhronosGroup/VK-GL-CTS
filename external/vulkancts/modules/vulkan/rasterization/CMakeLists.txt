include_directories(
	..
	../amber
	${DEQP_INL_DIR}
	)

set(DEQP_VK_RASTERIZATION_SRCS
	vktRasterizationTests.cpp
	vktRasterizationTests.hpp
	vktRasterizationFragShaderSideEffectsTests.cpp
	vktRasterizationFragShaderSideEffectsTests.hpp
	vktRasterizationProvokingVertexTests.cpp
	vktRasterizationProvokingVertexTests.hpp
	vktRasterizationOrderAttachmentAccessTests.cpp
	vktRasterizationOrderAttachmentAccessTests.hpp
	)

set(DEQP_VK_RASTERIZATION_LIBS
	tcutil
	vkutil
	libamber
	)

include_directories("../../../../../external/amber/src/include")
add_library(deqp-vk-rasterization STATIC ${DEQP_VK_RASTERIZATION_SRCS})
target_link_libraries(deqp-vk-rasterization ${DEQP_VK_RASTERIZATION_LIBS})
