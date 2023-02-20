include_directories(..)
if ((DE_OS_IS_WIN32 OR DE_OS_IS_UNIX) AND NOT DEQP_USE_SURFACELESS AND NOT DEQP_USE_NULLWS)
	include_directories(${FFMPEG_INCLUDE_PATH})
	add_compile_definitions(DE_BUILD_VIDEO)
endif()

set(DEQP_VK_VIDEO_SRCS
	extFFmpegDemuxer.h
	extNvidiaVideoParserIf.hpp
	vktVideoSessionNvUtils.cpp
	vktVideoSessionNvUtils.hpp
	vktVideoSessionFfmpegUtils.cpp
	vktVideoSessionFfmpegUtils.hpp
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
	)

PCH(DEQP_VK_VIDEO_SRCS ../pch.cpp)

add_library(deqp-vk-video STATIC ${DEQP_VK_VIDEO_SRCS})
target_link_libraries(deqp-vk-video ${DEQP_VK_VIDEO_LIBS})

