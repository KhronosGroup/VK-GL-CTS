# vksc-server

set(VKSC_SERVER_SRCS
	vksCommon.hpp
	vksNetwork.cpp
	vksNetwork.hpp
	vksEndian.hpp
	vksSerializer.hpp
	vksSerializerVKSC.hpp
	vksJson.hpp
	vksJson.cpp
	vksCacheBuilder.hpp
	vksCacheBuilder.cpp
	vksProtocol.hpp
	vksStore.hpp
	vksServices.cpp
	vksServices.hpp
	server.cpp
	)

set(VKSC_SERVER_UTIL_SRCS
	vksCommon.hpp
	vksNetwork.cpp
	vksNetwork.hpp
	vksProtocol.hpp
	vksEndian.hpp
	vksSerializer.hpp
	vksSerializerVKSC.hpp
	vksJson.hpp
	vksJson.cpp
	vksIPC.hpp
	vksIPC.cpp
	vksStore.hpp
	pcreader.hpp
	vksCacheBuilder.hpp
	vksCacheBuilder.cpp
	vksClient.hpp
	)

set(VKSC_SERVER_TESTS_SRCS
	tests.cpp
	)

add_executable(vksc-server ${VKSC_SERVER_SRCS})
target_include_directories(vksc-server PUBLIC ${CMAKE_SOURCE_DIR}/external/vulkancts/modules/vulkan)
target_link_libraries(vksc-server decpp tcutil tcutil-sc-platform deqp-vksc-package vkscutil jsoncpp_static)

add_library(vksc-server-util ${VKSC_SERVER_UTIL_SRCS})
target_link_libraries(vksc-server-util decpp vkscutil jsoncpp_static)

add_executable(vksc-server-tests ${VKSC_SERVER_TESTS_SRCS})
target_link_libraries(vksc-server-tests vksc-server-util decpp)
