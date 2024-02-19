# cmake file for vulkan-validationlayers

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/layers/VkLayer_khronos_validation.def")
	set(DEFAULT_VULKAN_VALIDATIONLAYERS_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
else ()
	set(DEFAULT_VULKAN_VALIDATIONLAYERS_SRC_PATH "../vulkan-validationlayers")
endif ()

set(VULKAN_VALIDATIONLAYERS_SRC_PATH ${DEFAULT_VULKAN_VALIDATIONLAYERS_SRC_PATH} CACHE STRING "Path to vulkan-validationlayers source tree")
if (IS_ABSOLUTE ${VULKAN_VALIDATIONLAYERS_SRC_PATH})
	set(VULKAN_VALIDATIONLAYERS_ABS_PATH ${VULKAN_VALIDATIONLAYERS_SRC_PATH})
else ()
	set(VULKAN_VALIDATIONLAYERS_ABS_PATH "${CMAKE_SOURCE_DIR}/${VULKAN_VALIDATIONLAYERS_SRC_PATH}")
endif ()

if (NOT DE_OS_IS_ANDROID AND NOT DE_OS_IS_IOS)
	if (EXISTS ${VULKAN_VALIDATIONLAYERS_ABS_PATH}/CMakeLists.txt)
		message(STATUS "vulkan-validationlayers found")
		set(CMAKE_C_FLAGS ${DE_3RD_PARTY_C_FLAGS})
		set(CMAKE_CXX_FLAGS ${DE_3RD_PARTY_CXX_FLAGS})

		# Make sure VVL uses its own deps.  The Vulkan headers / SPIR-V Tools
		# in CTS may update more frequently than VVL, and this avoids
		# unnecessary churn.
		set(UPDATE_DEPS ON)

		# Remove dependencies to things not needed for the validation layers themselves
		set(BUILD_WSI_XCB_SUPPORT OFF)
		set(BUILD_WSI_XLIB_SUPPORT OFF)
		set(BUILD_WSI_WAYLAND_SUPPORT OFF)

		add_subdirectory(${VULKAN_VALIDATIONLAYERS_ABS_PATH} vulkan-validationlayers)
	else ()
		message(FATAL_ERROR "vulkan-validationlayers not found")
	endif ()
endif ()
