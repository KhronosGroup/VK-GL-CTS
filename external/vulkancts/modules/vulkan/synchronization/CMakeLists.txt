include_directories(..)

set(DEQP_VK_SYNCHRONIZATION_SRCS
	vktSynchronizationTests.cpp
	vktSynchronizationTests.hpp
	vktSynchronizationUtil.cpp
	vktSynchronizationUtil.hpp
	vktSynchronizationSmokeTests.cpp
	vktSynchronizationSmokeTests.hpp
	)

set(DEQP_VK_SYNCHRONIZATION_LIBS
	deqp-vk-common
	tcutil
	vkutil
	)

add_library(deqp-vk-synchronization STATIC ${DEQP_VK_SYNCHRONIZATION_SRCS})
target_link_libraries(deqp-vk-synchronization ${DEQP_VK_SYNCHRONIZATION_LIBS})
