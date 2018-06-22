# cmake file for VkRunner

include(CheckCCompilerFlag)

foreach(FLAG -std=c99 -Wno-extra -Wno-pedantic -Wno-conversion -Wno-shadow)
  string(REGEX REPLACE "[=-]" "_" FLAG_VAR "${FLAG}")
  CHECK_C_COMPILER_FLAG("${FLAG}" HAVE_FLAG_${FLAG_VAR})
  if(${HAVE_FLAG_${FLAG_VAR}})
    add_definitions("${FLAG}")
  endif()
endforeach()

set(VULKAN_HEADER "\"vkDefs.h\"")
add_definitions("-D_DEFAULT_SOURCE")
include_directories(../../external/vulkancts/framework/vulkan/)
add_subdirectory(src)
