##################
# Static library approach #
##################

cmake_minimum_required(VERSION 3.11)
project(vk-video-samples-wrapper)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(COMMON_COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers  -Wno-string-conversion -Wno-conversion")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -fno-strict-aliasing -fno-builtin-memcmp")


    if (APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${COMMON_COMPILE_FLAGS}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c++17 -fno-rtti")
    if (UNIX)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    endif()
endif()

set(VK_VIDEO_ENCODER_LIBS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_video_encoder/libs" CACHE STRING "Root path to common vkvideo encoder libs sources")
set(VK_VIDEO_COMMON_LIBS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/common/libs" CACHE STRING "Root path to common vkvideo libs sources")
set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/scripts")

# Include CMake modules from VVS
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/cmake")

# Use FindShaderc.cmake to find or build shaderc
include(FetchContent)
unset(USE_SYSTEM_SHADERC CACHE)
set(USE_SYSTEM_SHADERC OFF CACHE BOOL "Use system installed shaderc" FORCE)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
include(FindShaderc_modified)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../external/vulkancts/framework/vulkan/generated/vulkan
    )

# Add shaderc includes if found by FindShaderc.cmake
if(SHADERC_INCLUDE_DIR)
    include_directories(${SHADERC_INCLUDE_DIR})
    message(STATUS "VVS: Using shaderc includes from ${SHADERC_INCLUDE_DIR}")
endif()

macro(generate_dispatch_table out)
    add_custom_command(OUTPUT ${out}
        COMMAND ${_Python3_EXECUTABLE} ${SCRIPTS_DIR}/generate-dispatch-table.py ${out}
        DEPENDS ${SCRIPTS_DIR}/generate-dispatch-table.py
    )
endmacro()

# Generate the dispatch table files
generate_dispatch_table(${VK_VIDEO_COMMON_LIBS_SOURCE_ROOT}/VkCodecUtils/HelpersDispatchTable.h)
generate_dispatch_table(${VK_VIDEO_COMMON_LIBS_SOURCE_ROOT}/VkCodecUtils/HelpersDispatchTable.cpp)

add_custom_target(GenerateDispatchTables
    DEPENDS
        ${VK_VIDEO_COMMON_LIBS_SOURCE_ROOT}/VkCodecUtils/HelpersDispatchTable.h
        ${VK_VIDEO_COMMON_LIBS_SOURCE_ROOT}/VkCodecUtils/HelpersDispatchTable.cpp
)

############ VULKAN_VIDEO_ENCODER_INCLUDE ######################################
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/libs/VkVideoEncoder")
    set(LIBVKVIDEOENCODER_BINARY_ROOT "${CMAKE_BINARY_DIR}/libs" CACHE STRING "User defined path to the encoder library binaries for this project")
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

add_compile_definitions(VK_USE_VIDEO_QUEUE VK_USE_VIDEO_DECODE_QUEUE VK_USE_VIDEO_ENCODE_QUEUE VVS_CTS_VULKAN_HEADER)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
# Do not set VK_USE_PLATFORM_WIN32_KHR as it will enable vulkan functions
# such as CreateWin32SurfaceKHR and GetPhysicalDeviceWin32PresentationSupportKHR
# which are not defined by vkVulkan_inl.c and not necessary in the context of CTS.
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX="1")
endif()

add_subdirectory(src/vk_video_encoder/libs)

get_target_property(ENCODER_INCLUDE_DIRS vkvideo-encoder-static INCLUDE_DIRECTORIES)

set(VULKAN_VIDEO_ENCODER_INCLUDE_DIRS ${ENCODER_INCLUDE_DIRS} CACHE INTERNAL "")
set(VULKAN_VIDEO_ENCODER_LIBRARY vkvideo-encoder-static CACHE INTERNAL "")


############ VULKAN_VIDEO_PARSER_INCLUDE ######################################
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_video_decoder/libs/NvVideoParser")
    set(LIBNVPARSER_BINARY_ROOT "${CMAKE_BINARY_DIR}/libs/NvVideoParser" CACHE STRING "User defined path to the the parser library binaries for this project")
else()
   set(LIBNVPARSER_BINARY_ROOT "${VULKAN_VIDEO_TESTS_SOURCE_DIR}/bin/libs/nv_vkvideo_parser/${LIB_ARCH_DIR}" CACHE STRING "User defined path to the the parser library binaries for this project")
endif()
message(STATUS "LIBNVPARSER_BINARY_ROOT is at ${LIBNVPARSER_BINARY_ROOT}" )

if (DEFINED ENV{VULKAN_VIDEO_PARSER_INCLUDE_PATH})
    MESSAGE(STATUS "VULKAN_VIDEO_PARSER_INCLUDE_PATH ENV VAR is set to $ENV{VULKAN_VIDEO_PARSER_INCLUDE_PATH}")
    set(VULKAN_VIDEO_PARSER_INCLUDE  "$ENV{VULKAN_VIDEO_PARSER_INCLUDE_PATH}" CACHE PATH "Path to Vulkan Video Parser include directory" FORCE)
else()
    set(VULKAN_VIDEO_PARSER_INCLUDE  "${VULKAN_VIDEO_PARSER_INCLUDE}" CACHE PATH "Path to Vulkan Video Parser include directory")
endif()

if (EXISTS "${VULKAN_VIDEO_PARSER_INCLUDE}")
    MESSAGE(STATUS "VULKAN_VIDEO_PARSER_INCLUDE is set and valid ${VULKAN_VIDEO_PARSER_INCLUDE}")
else()
    MESSAGE(STATUS "VULKAN_VIDEO_PARSER_INCLUDE path is not set. Setting the default path location to ${VULKAN_VIDEO_TESTS_SOURCE_DIR}/include/vkvideo_parser")
    set(VULKAN_VIDEO_PARSER_INCLUDE  "${CMAKE_CURRENT_SOURCE_DIR}/src/vk_video_decoder/include/vkvideo_parser" CACHE PATH "Path to Vulkan Video Parser include directory" FORCE)
endif()
############ VULKAN_VIDEO_PARSER_INCLUDE ######################################

############ VULKAN_VIDEO_PARSER_LIB_PATH ######################################
if (DEFINED ENV{VULKAN_VIDEO_PARSER_LIB_DIR_PATH})
    MESSAGE(STATUS "VULKAN_VIDEO_PARSER_LIB_DIR_PATH ENV VAR is set to $ENV{VULKAN_VIDEO_PARSER_LIB_DIR_PATH}")
    set(VULKAN_VIDEO_PARSER_LIB_PATH  "$ENV{VULKAN_VIDEO_PARSER_LIB_DIR_PATH}" CACHE PATH "Path to Vulkan Video Parser library directory" FORCE)
else()
    set(VULKAN_VIDEO_PARSER_LIB_PATH  "${LIBNVPARSER_BINARY_ROOT}" CACHE PATH "Path to Vulkan Video Parser library directory")
endif()

set(VULKAN_VIDEO_PARSER_LIB vkvideo-parser CACHE PATH "The name of Vulkan Video Parser library")
set(VULKAN_VIDEO_PARSER_STATIC_LIB vkvideo-parser-static CACHE PATH "The name of Vulkan Video Parser library")
set(VULKAN_VIDEO_PARSER_LIB_VERSION 1 CACHE PATH "The version of Vulkan Video Parser library")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/NvVideoParser")

else()
    if (EXISTS "${VULKAN_VIDEO_PARSER_LIB_PATH}")
        message(STATUS "Trying to find Vulkan Video Parser library at ${VULKAN_VIDEO_PARSER_LIB_PATH}" )
        find_library(VULKAN_VIDEO_PARSER_LIB_WITH_PATH NAMES ${VULKAN_VIDEO_PARSER_LIB} PATHS ${VULKAN_VIDEO_PARSER_LIB_PATH})
        message(STATUS "Vulkan Video Parser Lib: ${VULKAN_VIDEO_PARSER_LIB_WITH_PATH}" )
    endif()
endif()
############ VULKAN_VIDEO_PARSER_LIB_PATH ######################################

add_subdirectory(src/vk_video_decoder/libs/NvVideoParser)

get_target_property(PARSER_INCLUDE_DIRS vkvideo-parser-static INCLUDE_DIRECTORIES)

set(VULKAN_VIDEO_PARSER_INCLUDE_DIRS ${PARSER_INCLUDE_DIRS} CACHE INTERNAL "")
set(VULKAN_VIDEO_PARSER_LIBRARY vkvideo-parser-static CACHE INTERNAL "")

message(STATUS "VULKAN_VIDEO_ENCODER_INCLUDE_DIRS: ${VULKAN_VIDEO_ENCODER_INCLUDE_DIRS}")
message(STATUS "VULKAN_VIDEO_ENCODER_LIBRARY: ${VULKAN_VIDEO_ENCODER_LIBRARY}")
message(STATUS "VULKAN_VIDEO_PARSER_INCLUDE_DIRS: ${VULKAN_VIDEO_PARSER_INCLUDE_DIRS}")
message(STATUS "VULKAN_VIDEO_PARSER_LIBRARY: ${VULKAN_VIDEO_PARSER_LIBRARY}")

# Report shaderc status
if(SHADERC_SHARED_LIBRARY)
    message(STATUS "VVS: Using shaderc library: ${SHADERC_SHARED_LIBRARY}")
elseif(SHADERC_LIB)
    message(STATUS "VVS: Building shaderc from source as: ${SHADERC_LIB}")
else()
    message(WARNING "VVS: Shaderc not found - shader compilation will not be available")
endif()
