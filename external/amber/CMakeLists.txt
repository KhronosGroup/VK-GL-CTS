# cmake file for amber

set(AMBER_ENABLE_SHARED_CRT TRUE)
set(AMBER_SKIP_TESTS TRUE)
set(AMBER_SKIP_SPIRV_TOOLS TRUE)
set(AMBER_SKIP_SHADERC TRUE)
set(AMBER_SKIP_SAMPLES TRUE)
set(AMBER_ENABLE_SHARED_CRT TRUE)
set(AMBER_ENABLE_RTTI TRUE CACHE BOOL "Enable runtime type information" FORCE)
set(AMBER_CTS_INL_DIR ${PROJECT_BINARY_DIR}/external/vulkancts/framework/vulkan/generated/vulkan)
set(Vulkan-Headers_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vulkancts/framework/vulkan/generated/vulkan/)

if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
       add_compile_options(-w)
elseif (DE_COMPILER_IS_MSC)
       add_compile_options(/W0)
endif ()



add_subdirectory(src)
