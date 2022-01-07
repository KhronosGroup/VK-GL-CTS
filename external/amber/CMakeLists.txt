# cmake file for amber

set(AMBER_ENABLE_SHARED_CRT TRUE)
set(AMBER_SKIP_TESTS TRUE)
set(AMBER_SKIP_SPIRV_TOOLS TRUE)
set(AMBER_SKIP_SHADERC TRUE)
set(AMBER_SKIP_SAMPLES TRUE)
set(AMBER_ENABLE_SHARED_CRT TRUE)
set(AMBER_ENABLE_RTTI TRUE CACHE BOOL "Enable runtime type information" FORCE)
set(AMBER_CTS_INL_DIR ${PROJECT_BINARY_DIR}/external/vulkancts/framework/vulkan)
set(Vulkan-Headers_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vulkancts/framework/vulkan/)

add_subdirectory(src)
