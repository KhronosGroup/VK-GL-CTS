cmake_minimum_required(VERSION 3.11)
project(video-generator-wrapper)

option(BUILD_EXAMPLES "Build examples" OFF)

# Suppress warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wno-error=deprecated-declarations -Wno-error=strict-prototypes)
  add_compile_options(-Wno-deprecated-declarations -Wno-strict-prototypes)
endif()

add_subdirectory(src)

set(VIDEO_GENERATOR_INCLUDE_DIR ${VIDEO_GENERATOR_INCLUDE_DIR} CACHE PATH "Path to video generator include directory" FORCE)
set(VIDEO_GENERATOR_LIB ${VIDEO_GENERATOR_LIB} CACHE STRING "The name of video generator library" FORCE)
set(VIDEO_GENERATOR_STATIC_LIB ${VIDEO_GENERATOR_STATIC_LIB} CACHE STRING "The name of static video generator library" FORCE)

message(STATUS "VIDEO_GENERATOR_INCLUDE_DIR: ${VIDEO_GENERATOR_INCLUDE_DIR}")
message(STATUS "VIDEO_GENERATOR_LIB: ${VIDEO_GENERATOR_LIB}")
message(STATUS "VIDEO_GENERATOR_STATIC_LIB: ${VIDEO_GENERATOR_STATIC_LIB}")