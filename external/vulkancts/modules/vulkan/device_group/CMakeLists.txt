include_directories(..)

set(DEQP_VK_DEVICE_GROUP_SRCS
	vktDeviceGroupTests.hpp
	vktDeviceGroupRendering.cpp
)

set(DEQP_VK_DEVICE_GROUP_LIBS
	tcutil
	vkutil
)

add_library(deqp-vk-device-group STATIC ${DEQP_VK_DEVICE_GROUP_SRCS})
target_link_libraries(deqp-vk-device-group ${DEQP_VK_DEVICE_GROUP_LIBS})
