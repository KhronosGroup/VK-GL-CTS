include_directories(..)
if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX)
	include_directories(${ESEXTRACTOR_INCLUDE_PATH})
	include_directories(${NV_VIDEO_PARSER_INCLUDE_DIRS})
	include_directories(.)

	add_definitions(-DES_STATIC_COMPILATION)
	add_compile_definitions(DE_BUILD_VIDEO)
endif()


set(DEQP_VK_VIDEO_SRC_TARGET_DEPENDENT
	extESExtractor.hpp
	extNvidiaVideoParserIf.hpp
	vktVideoBaseDecodeUtils.hpp
	vktVideoBaseDecodeUtils.cpp
	vktVideoFrameBuffer.hpp
	vktVideoFrameBuffer.cpp
	vktBitstreamBufferImpl.hpp
	vktBitstreamBufferImpl.cpp
	)

set(DEQP_VK_VIDEO_SRCS_TARGET_INDEPENDENT
	vktVideoCapabilitiesTests.cpp
	vktVideoCapabilitiesTests.hpp
	vktVideoDecodeTests.cpp
	vktVideoDecodeTests.hpp
	vktVideoTestUtils.cpp
	vktVideoTestUtils.hpp
	vktVideoTests.cpp
	vktVideoTests.hpp
	vktVideoClipInfo.cpp
	vktVideoClipInfo.hpp
	)

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX)
	set(DEQP_VK_VIDEO_SRCS
		${DEQP_VK_VIDEO_SRC_TARGET_DEPENDENT}
		${DEQP_VK_VIDEO_SRCS_TARGET_INDEPENDENT}
		${NV_VIDEO_PARSER_SOURCES}
		)
else()
	set(DEQP_VK_VIDEO_SRCS ${DEQP_VK_VIDEO_SRCS_TARGET_INDEPENDENT})
endif()

set(DEQP_VK_VIDEO_LIBS
	tcutil
	vkutil
	)

if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX)
	list(APPEND DEQP_VK_VIDEO_LIBS ${ESEXTRACTOR_LIBRARY})
endif ()

PCH(DEQP_VK_VIDEO_SRCS ../pch.cpp)

add_library(deqp-vk-video STATIC ${DEQP_VK_VIDEO_SRCS})
target_link_libraries(deqp-vk-video ${DEQP_VK_VIDEO_LIBS})
if (DE_OS_IS_UNIX)
	# TODO: The NVIDIA samples app has hundreds of these warnings.
	target_compile_options(deqp-vk-video PRIVATE "-Wno-conversion" "-Wno-sign-compare")
endif ()
