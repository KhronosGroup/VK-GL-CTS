# Vulkan video support libraries

if (NOT DEQP_DISABLE_VK_VIDEO_TESTS)
	# Video parser and encoder library
	add_subdirectory(${CMAKE_SOURCE_DIR}/external/vulkan-video-samples ${CMAKE_BINARY_DIR}/external/vulkan-video-samples EXCLUDE_FROM_ALL)
	# YUV Video generator library
	add_subdirectory(${CMAKE_SOURCE_DIR}/external/video_generator ${CMAKE_BINARY_DIR}/external/video_generator EXCLUDE_FROM_ALL)
endif()

include_directories(..)

if (NOT DEQP_DISABLE_VK_VIDEO_TESTS)
	include_directories(${VULKAN_VIDEO_PARSER_INCLUDE_DIRS})
	include_directories(${VULKAN_VIDEO_ENCODER_INCLUDE_DIRS})
	include_directories(${VIDEO_GENERATOR_INCLUDE_DIR})
endif()
include_directories(.)

if (NOT DEQP_DISABLE_VK_VIDEO_TESTS)
	add_compile_definitions(ENABLE_AV1_DECODER)
	add_compile_definitions(DE_BUILD_VIDEO)
	message(STATUS "Vulkan Video Tests: ENABLED (external dependencies required)")
else()
	message(STATUS "Vulkan Video Tests: DISABLED (tests will report NotSupported)")
endif()

set(DEQP_VK_VIDEO_SRC_TARGET_DEPENDENT
	vktDemuxer.hpp
	vktDemuxer.cpp
	vktBufferedReader.hpp
	vktVideoBaseDecodeUtils.hpp
	vktVideoBaseDecodeUtils.cpp
	vktVideoFrameBuffer.hpp
	vktVideoFrameBuffer.cpp
	vktBitstreamBufferImpl.hpp
	vktBitstreamBufferImpl.cpp
	)

# Exclude external dependency files when video tests are disabled
if (DEQP_DISABLE_VK_VIDEO_TESTS)
	list(REMOVE_ITEM DEQP_VK_VIDEO_SRC_TARGET_DEPENDENT
		vktDemuxer.hpp
		vktDemuxer.cpp
		vktBufferedReader.hpp
		vktVideoBaseDecodeUtils.cpp
		vktVideoFrameBuffer.hpp
		vktVideoFrameBuffer.cpp
		vktBitstreamBufferImpl.hpp
		vktBitstreamBufferImpl.cpp
	)
endif()

set(DEQP_VK_VIDEO_SRCS_TARGET_INDEPENDENT
	vktVideoCapabilitiesTests.cpp
	vktVideoCapabilitiesTests.hpp
	vktVideoDecodeTests.cpp
	vktVideoDecodeTests.hpp
	vktVideoEncodeTests.cpp
	vktVideoEncodeTests.hpp
	vktVideoEncodeTestsAV1.cpp
	vktVideoTestUtils.cpp
	vktVideoTestUtils.hpp
	vktVideoTests.cpp
	vktVideoTests.hpp
	vktVideoClipInfo.cpp
	vktVideoClipInfo.hpp
	)

set(DEQP_VK_VIDEO_SRCS
	${DEQP_VK_VIDEO_SRC_TARGET_DEPENDENT}
	${DEQP_VK_VIDEO_SRCS_TARGET_INDEPENDENT}
	)

set(DEQP_VK_VIDEO_LIBS
	tcutil
	vkutil
	)

if (NOT DEQP_DISABLE_VK_VIDEO_TESTS)
	list(APPEND DEQP_VK_VIDEO_LIBS
		${VULKAN_VIDEO_PARSER_LIBRARY}
		${VULKAN_VIDEO_ENCODER_LIBRARY}
		${VIDEO_GENERATOR_STATIC_LIB}
		)
endif()

PCH(DEQP_VK_VIDEO_SRCS ../pch.cpp)

add_library(deqp-vk-video STATIC ${DEQP_VK_VIDEO_SRCS})
target_link_libraries(deqp-vk-video deqp-vk-synchronization deqp-vk-image-util deqp-vk-ycbcr-util ${DEQP_VK_VIDEO_LIBS})

if (DE_OS_IS_UNIX)
	# TODO: The NVIDIA samples app has hundreds of these warnings.
	target_compile_options(deqp-vk-video PRIVATE "-Wno-conversion" "-Wno-sign-compare")
endif ()
