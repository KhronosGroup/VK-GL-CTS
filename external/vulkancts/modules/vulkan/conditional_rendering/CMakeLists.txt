include_directories(
	..
	../draw
	../compute
	${DEQP_INL_DIR}
)

set(DEQP_VK_CONDITIONAL_REDENDERING_SRCS
	vktConditionalRenderingTestUtil.hpp
	vktConditionalRenderingTestUtil.cpp
	vktConditionalDrawTests.hpp
	vktConditionalDrawTests.cpp
	vktConditionalDispatchTests.hpp
	vktConditionalDispatchTests.cpp
	vktConditionalClearAttachmentTests.hpp
	vktConditionalClearAttachmentTests.cpp
	vktConditionalTests.hpp
	vktConditionalTests.cpp
	vktConditionalDrawAndClearTests.hpp
	vktConditionalDrawAndClearTests.cpp
)

set(DEQP_VK_CONDITIONAL_REDENDERING_LIBS
	tcutil
	vkutil
	deqp-vk-draw
	deqp-vk-compute
)

add_library(deqp-vk-conditional-rendering STATIC ${DEQP_VK_CONDITIONAL_REDENDERING_SRCS})
target_link_libraries(deqp-vk-conditional-rendering ${DEQP_VK_CONDITIONAL_REDENDERING_LIBS})
