include_directories(
	..
    ../amber
	${DEQP_INL_DIR}
)

set(DEQP_VK_MEMORY_MODEL_SRCS
	vktMemoryModelTests.hpp
	vktMemoryModelMessagePassing.cpp
	vktMemoryModelPadding.hpp
	vktMemoryModelPadding.cpp
	vktMemoryModelSharedLayout.cpp
	vktMemoryModelSharedLayout.hpp
	vktMemoryModelSharedLayoutCase.cpp
	vktMemoryModelSharedLayoutCase.hpp
	../util/vktTypeComparisonUtil.hpp
	../util/vktTypeComparisonUtil.cpp
)

add_library(deqp-vk-memory-model STATIC ${DEQP_VK_MEMORY_MODEL_SRCS})
target_link_libraries(deqp-vk-memory-model tcutil vkutil)

add_library(deqp-vksc-memory-model STATIC ${DEQP_VK_MEMORY_MODEL_SRCS})
target_link_libraries(deqp-vksc-memory-model PUBLIC deqp-vksc-util tcutil vkscutil)
