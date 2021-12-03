include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_DEVICE_GROUP_SRCS
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

set(DEQP_VK_DEVICE_GROUP_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-memory-model STATIC ${DEQP_VK_DEVICE_GROUP_SRCS})
target_link_libraries(deqp-vk-memory-model ${DEQP_VK_DEVICE_GROUP_LIBS})
