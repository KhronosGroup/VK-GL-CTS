if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_video_decoder/CMakeLists.txt")
	set(DEFAULT_NV_VIDEO_SAMPLES_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
else ()
	message(FATAL_ERROR "nvidia-video-samples source code not found")
endif ()

set(NV_VIDEO_SAMPLES_SRC_PATH ${DEFAULT_NV_VIDEO_SAMPLES_SRC_PATH} CACHE STRING "Path to nvidia-video-samples source tree")

if (IS_ABSOLUTE ${NV_VIDEO_SAMPLES_SRC_PATH})
	set(NV_VIDEO_SAMPLES_ABS_PATH ${NV_VIDEO_SAMPLES_SRC_PATH})
else ()
	set(NV_VIDEO_SAMPLES_ABS_PATH "${CMAKE_SOURCE_DIR}/${NV_VIDEO_SAMPLES_SRC_PATH}")
endif ()

if (EXISTS "${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/libs/NvVideoParser/CMakeLists.txt")
	message(STATUS "nvidia-video-samples parser found")
	set(NV_VIDEO_PARSER_SOURCE "${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/libs/NvVideoParser")

	set (PARSER_SOURCES
		${NV_VIDEO_PARSER_SOURCE}/src/VulkanH264Parser.cpp
		${NV_VIDEO_PARSER_SOURCE}/src/VulkanH265Parser.cpp
		${NV_VIDEO_PARSER_SOURCE}/src/VulkanVideoDecoder.cpp
	)

	set(CMAKE_C_FLAGS ${DE_3RD_PARTY_C_FLAGS})
	set(CMAKE_CXX_FLAGS ${DE_3RD_PARTY_CXX_FLAGS})

	set(PARSER_INCLUDE_DIRS
		"${NV_VIDEO_SAMPLES_ABS_PATH}/common/include" # RefCountBase and friends
		"${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/include"
		"${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/include/NvVideoParser"
		"${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/include/vkvideo_parser"
		"${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/libs" # ReferenceCountedPool, drop this if possbile, could do without the complexity
		"${NV_VIDEO_SAMPLES_ABS_PATH}/vk_video_decoder/libs/NvVideoParser/include"
		)

	set(NV_VIDEO_PARSER_INCLUDE_DIRS ${PARSER_INCLUDE_DIRS} PARENT_SCOPE)
	set(NV_VIDEO_PARSER_SOURCES ${PARSER_SOURCES} PARENT_SCOPE)
else ()
	message(FATAL_ERROR "nvidia-video-samples not found")
endif ()
