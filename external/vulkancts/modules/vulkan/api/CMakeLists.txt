# API layer tests

include_directories(..)

set(DEQP_VK_API_SRCS
	vktApiTests.cpp
	vktApiTests.hpp
	vktApiSmokeTests.cpp
	vktApiSmokeTests.hpp
	vktApiDeviceInitializationTests.cpp
	vktApiDeviceInitializationTests.hpp
	vktApiObjectManagementTests.cpp
	vktApiObjectManagementTests.hpp
	vktApiBufferTests.cpp
	vktApiBufferTests.hpp
	vktApiBufferViewCreateTests.cpp
	vktApiBufferViewCreateTests.hpp
	vktApiBufferViewAccessTests.cpp
	vktApiBufferViewAccessTests.hpp
	)

set(DEQP_VK_API_LIBS
	tcutil
	vkutil
	)

add_library(deqp-vk-api STATIC ${DEQP_VK_API_SRCS})
target_link_libraries(deqp-vk-api ${DEQP_VK_API_LIBS})
