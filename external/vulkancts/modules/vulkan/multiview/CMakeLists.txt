include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_MULTI_VIEW_SRCS
	vktMultiViewTests.hpp
	vktMultiViewTests.cpp
	vktMultiViewRenderPassUtil.hpp
	vktMultiViewRenderPassUtil.cpp
	vktMultiViewRenderTests.hpp
	vktMultiViewRenderTests.cpp
	vktMultiViewRenderUtil.hpp
	vktMultiViewRenderUtil.cpp
)

set(DEQP_VK_MULTI_VIEW_LIBS
	tcutil
	vkutil
)

PCH(DEQP_VK_MULTI_VIEW_SRCS ../pch.cpp)

add_library(deqp-vk-multiview STATIC ${DEQP_VK_MULTI_VIEW_SRCS})
target_link_libraries(deqp-vk-multiview ${DEQP_VK_MULTI_VIEW_LIBS})
