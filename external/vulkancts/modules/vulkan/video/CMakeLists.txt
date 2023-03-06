include_directories(..)
if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX)
	include_directories(${ESEXTRACTOR_INCLUDE_PATH})
	add_definitions(-DES_STATIC_COMPILATION)
	add_compile_definitions(DE_BUILD_VIDEO)
endif()


set(DEQP_VK_VIDEO_SRCS
	extESExtractor.hpp
	extNvidiaVideoParserIf.hpp
	vktVideoSessionNvUtils.cpp
	vktVideoSessionNvUtils.hpp
	vktVideoBaseDecodeUtils.hpp
	vktVideoBaseDecodeUtils.cpp
	vktVideoDecodeTests.cpp
	vktVideoDecodeTests.hpp
	vktVideoPictureUtils.cpp
	vktVideoPictureUtils.hpp
	vktVideoCapabilitiesTests.cpp
	vktVideoCapabilitiesTests.hpp
	vktVideoTestUtils.cpp
	vktVideoTestUtils.hpp
	vktVideoTests.cpp
	vktVideoTests.hpp
	)


set(DEQP_VK_VIDEO_LIBS
	tcutil
	vkutil
	${ESEXTRACTOR_LIBRARY}
	)

PCH(DEQP_VK_VIDEO_SRCS ../pch.cpp)

add_library(deqp-vk-video STATIC ${DEQP_VK_VIDEO_SRCS})
target_link_libraries(deqp-vk-video ${DEQP_VK_VIDEO_LIBS})
