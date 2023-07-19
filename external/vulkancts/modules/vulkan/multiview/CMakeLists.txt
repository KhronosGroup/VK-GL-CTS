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

PCH(DEQP_VK_MULTI_VIEW_SRCS ../pch.cpp)

add_library(deqp-vk-multiview STATIC ${DEQP_VK_MULTI_VIEW_SRCS})
target_link_libraries(deqp-vk-multiview tcutil vkutil)

add_library(deqp-vksc-multiview STATIC ${DEQP_VK_MULTI_VIEW_SRCS})
target_link_libraries(deqp-vksc-multiview PUBLIC deqp-vksc-util tcutil vkscutil)
