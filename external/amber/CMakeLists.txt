# cmake file for amber

set(AMBER_ENABLE_SHARED_CRT TRUE)
set(AMBER_SKIP_TESTS TRUE)
set(AMBER_SKIP_SPIRV_TOOLS TRUE)
set(AMBER_SKIP_SHADERC TRUE)
set(AMBER_SKIP_SAMPLES TRUE)
set(AMBER_ENABLE_SHARED_CRT TRUE)
set(AMBER_ENABLE_RTTI TRUE CACHE BOOL "Enable runtime type information" FORCE)
set(Vulkan-Headers_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vulkancts/framework/vulkan/)

if (DE_COMPILER_IS_CLANG)
	# Fixed upstream in 4d0115cccfcb3b73d20b6513b1c40748e6403c50
	add_compile_options(-Wno-suggest-destructor-override)
elseif (DE_COMPILER_IS_GCC)
	# Fixed upstream in aa69a0ac23ea7f68dd32bbef210546a5d84c1734
	add_compile_options(-Wno-uninitialized)
endif ()

add_subdirectory(src)
