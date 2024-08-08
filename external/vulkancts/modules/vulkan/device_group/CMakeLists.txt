include_directories(
	..
	${DEQP_INL_DIR}
)

set(DEQP_VK_DEVICE_GROUP_SRCS
	vktDeviceGroupTests.hpp
	vktDeviceGroupRendering.cpp
)

add_library(deqp-vk-device-group STATIC ${DEQP_VK_DEVICE_GROUP_SRCS})
target_link_libraries(deqp-vk-device-group tcutil vkutil)

add_library(deqp-vksc-device-group STATIC ${DEQP_VK_DEVICE_GROUP_SRCS})
target_link_libraries(deqp-vksc-device-group PUBLIC referencerenderer deqp-vksc-util tcutil vkscutil)
