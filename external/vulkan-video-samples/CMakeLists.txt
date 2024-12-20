##################
# Static library approach #
##################

cmake_minimum_required(VERSION 3.10)
project(vk-video-samples-av1-wrapper)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(COMMON_COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers  -Wno-string-conversion")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -fno-strict-aliasing -fno-builtin-memcmp")


    if (APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_COMPILE_FLAGS}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c++11 -fno-rtti")
    if (UNIX)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    endif()
endif()

set(VK_VIDEO_ENCODER_LIBS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_video_encoder/libs" CACHE STRING "Root path to common vkvideo encoder libs sources")
set(VK_VIDEO_COMMON_LIBS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/common/libs" CACHE STRING "Root path to common vkvideo libs sources")
set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/scripts")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/common/include")

############ VULKAN_VIDEO_ENCODER_INCLUDE ######################################
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/VkVideoEncoder")
    set(LIBVKVIDEOENCODER_BINARY_ROOT "${CMAKE_BINARY_DIR}/libs" CACHE STRING "User defined path to the the encoder library binaries for this project")
else()
   set(LIBVKVIDEOENCODER_BINARY_ROOT "${VULKAN_VIDEO_TESTS_SOURCE_DIR}/bin/libs/vk_video_encoder/${LIB_ARCH_DIR}" CACHE STRING "User defined path to the the encoder library binaries for this project")
endif()
message(STATUS "LIBVKVIDEOENCODER_BINARY_ROOT is at ${LIBVKVIDEOENCODER_BINARY_ROOT}" )

if (DEFINED ENV{VULKAN_VIDEO_ENCODER_INCLUDE_PATH})
    MESSAGE(STATUS "VULKAN_VIDEO_ENCODER_INCLUDE_PATH ENV VAR is set to $ENV{VULKAN_VIDEO_ENCODER_INCLUDE_PATH}")
    set(VULKAN_VIDEO_ENCODER_INCLUDE  "$ENV{VULKAN_VIDEO_ENCODER_INCLUDE_PATH}" CACHE PATH "Path to Vulkan Video Encoder include directory" FORCE)
else()
    set(VULKAN_VIDEO_ENCODER_INCLUDE  "${VULKAN_VIDEO_ENCODER_INCLUDE}" CACHE PATH "Path to Vulkan Video Encoder include directory")
endif()

if (EXISTS "${VULKAN_VIDEO_ENCODER_INCLUDE}")
    MESSAGE(STATUS "VULKAN_VIDEO_ENCODER_INCLUDE is set and valid ${VULKAN_VIDEO_ENCODER_INCLUDE}")
else()
    MESSAGE(STATUS "VULKAN_VIDEO_ENCODER_INCLUDE path is not set. Setting the default path location to ${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(VULKAN_VIDEO_ENCODER_INCLUDE  "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_video_encoder/include" CACHE PATH "Path to Vulkan Video Encoder include directory" FORCE)
endif()
############ VULKAN_VIDEO_ENCODER_INCLUDE ######################################

############ VULKAN_VIDEO_ENCODER_LIB_PATH ######################################
if (DEFINED ENV{VULKAN_VIDEO_ENCODER_LIB_DIR_PATH})
    MESSAGE(STATUS "VULKAN_VIDEO_ENCODER_LIB_DIR_PATH ENV VAR is set to $ENV{VULKAN_VIDEO_ENCODER_LIB_DIR_PATH}")
    set(VULKAN_VIDEO_ENCODER_LIB_PATH  "$ENV{VULKAN_VIDEO_ENCODER_LIB_DIR_PATH}" CACHE PATH "Path to Vulkan Video Encoder library directory" FORCE)
else()
    set(VULKAN_VIDEO_ENCODER_LIB_PATH  "${LIBVKVIDEOENCODER_BINARY_ROOT}" CACHE PATH "Path to Vulkan Video Encoder library directory")
endif()

set(VULKAN_VIDEO_ENCODER_LIB vkvideo-encoder CACHE PATH "The name of Vulkan Video Encoder library")
set(VULKAN_VIDEO_ENCODER_STATIC_LIB vkvideo-encoder-static CACHE PATH "The name of Vulkan Video Encoder library")
set(VULKAN_VIDEO_ENCODER_LIB_VERSION 1 CACHE PATH "The version of Vulkan Video Encoder library")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/VkVideoEncoder")

else()
    if (EXISTS "${VULKAN_VIDEO_ENCODER_LIB_PATH}")
        message(STATUS "Trying to find Vulkan Video Encoder library at ${VULKAN_VIDEO_ENCODER_LIB_PATH}" )
        find_library(VULKAN_VIDEO_ENCODER_LIB_WITH_PATH NAMES ${VULKAN_VIDEO_ENCODER_LIB} PATHS ${VULKAN_VIDEO_ENCODER_LIB_PATH})
        message(STATUS "Vulkan Video Encoder Lib: ${VULKAN_VIDEO_ENCODER_LIB_WITH_PATH}" )
    endif()
endif()
############ VULKAN_VIDEO_ENCODER_LIB_PATH ######################################

add_subdirectory(src/vk_video_encoder/libs)

get_target_property(ENCODER_INCLUDE_DIRS vkvideo-encoder-static INCLUDE_DIRECTORIES)

set(NV_VIDEO_ENCODER_INCLUDE_DIRS ${ENCODER_INCLUDE_DIRS} CACHE INTERNAL "")
set(NV_VIDEO_ENCODER_LIBRARY vkvideo-encoder-static CACHE INTERNAL "")

message(STATUS "NV_VIDEO_ENCODER_INCLUDE_DIRS: ${NV_VIDEO_ENCODER_INCLUDE_DIRS}")
message(STATUS "NV_VIDEO_ENCODER_LIBRARY: ${NV_VIDEO_ENCODER_LIBRARY}")