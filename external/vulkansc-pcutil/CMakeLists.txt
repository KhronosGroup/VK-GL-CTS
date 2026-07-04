# cmake file for vulkansc-pcutil

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/library/pcjson/vksc_pipeline_json_base.hpp")
	set(DEFAULT_VULKANSC_PCUTIL_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
else ()
	set(DEFAULT_VULKANSC_PCUTIL_SRC_PATH "../vulkansc-pcutil")
endif ()

set(VULKANSC_PCUTIL_SRC_PATH ${DEFAULT_VULKANSC_PCUTIL_SRC_PATH} CACHE STRING "Path to vulkansc-pcutil source tree")
if (IS_ABSOLUTE ${VULKANSC_PCUTIL_SRC_PATH})
	set(VULKANSC_PCUTIL_ABS_PATH ${VULKANSC_PCUTIL_SRC_PATH})
else ()
	set(VULKANSC_PCUTIL_ABS_PATH "${CMAKE_SOURCE_DIR}/${VULKANSC_PCUTIL_SRC_PATH}")
endif ()

include(ExternalProject)
set_directory_properties(PROPERTIES EP_PREFIX "${VULKANSC_PCUTIL_ABS_PATH}/..")

if (EXISTS ${VULKANSC_PCUTIL_ABS_PATH}/CMakeLists.txt)
	message(STATUS "vulkansc-pcutil found")
	set(CMAKE_C_FLAGS ${DE_3RD_PARTY_C_FLAGS})
	set(CMAKE_CXX_FLAGS ${DE_3RD_PARTY_CXX_FLAGS})
	set(VULKANSC_PCUTIL_CACHE_ARGS
		"-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
		"-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
		"-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}"
		"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}"
		"-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
		"-DBUILD_VKSCPCINFO:BOOL=OFF"
		"-DBUILD_JSON_GEN_LAYER:BOOL=OFF"
		"-DUPDATE_DEPS:BOOL=ON"
		"-DBUILD_TESTS:BOOL=OFF"
		"-DPCU_WERROR:BOOL=OFF"
	)
	set(SOURCE_DIR "${VULKANSC_PCUTIL_SRC_PATH}")
	set(BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/src/VulkanSC-pcutil-build")
	get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
	# ExternalProject does an extremely poor job of propagating even the most basic built-in variables to sub-builds.
	# Vulkan ecosystem repositories rely on CMAKE_BUILD_TYPE for driving the build type of external deps when using
	# UPDATE_DEPS=ON. This isn't really multi-config friendly, but we do our best in making sure most builds just work.
	if(GENERATOR_IS_MULTI_CONFIG)
		foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
			string(TOUPPER ${CONFIG} CONFIG_UPPER)
			set(VulkanSCPCJson_LIBRARY_${CONFIG_UPPER} "${BINARY_DIR}/library/pcjson/${CONFIG}/${CMAKE_STATIC_LIBRARY_PREFIX}vkscpcjson${CMAKE_STATIC_LIBRARY_SUFFIX}")
		endforeach()
		set(VulkanSCPCJson_LIBRARY "${BINARY_DIR}/library/pcjson/$<CONFIG>/${CMAKE_STATIC_LIBRARY_PREFIX}vkscpcjson${CMAKE_STATIC_LIBRARY_SUFFIX}")
		list(APPEND VULKANSC_PCUTIL_CACHE_ARGS "-DCMAKE_CONFIGURATION_TYPES:STRING=${CMAKE_CONFIGURATION_TYPES}")
		if(CMAKE_BUILD_TYPE)
			list(APPEND VULKANSC_PCUTIL_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}")
		else()
			if(NOT CMAKE_DEFAULT_BUILD_TYPE)
				list(GET CMAKE_CONFIGURATION_TYPES 0 CMAKE_DEFAULT_BUILD_TYPE)
			endif()
			list(APPEND VULKANSC_PCUTIL_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_DEFAULT_BUILD_TYPE}")
		endif()
	else()
		set(VulkanSCPCJson_LIBRARY "${BINARY_DIR}/library/pcjson/${CMAKE_STATIC_LIBRARY_PREFIX}vkscpcjson${CMAKE_STATIC_LIBRARY_SUFFIX}")
		if(CMAKE_BUILD_TYPE)
			list(APPEND VULKANSC_PCUTIL_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}")
		else()
			list(APPEND VULKANSC_PCUTIL_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=Debug")
		endif()
	endif()
	ExternalProject_Add(VulkanSC-pcutil
		SOURCE_DIR "${SOURCE_DIR}"
		PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
		CMAKE_CACHE_ARGS "${VULKANSC_PCUTIL_CACHE_ARGS}"
		INSTALL_COMMAND "" # Consuming the install-tree would restrict us to a single configuration,
		                   # therefor we consume the build tree.
		BUILD_BYPRODUCTS "${VulkanSCPCJson_LIBRARY}"
		#BUILD_ALWAYS TRUE # Remove comment if you want to build local changes
	)

	add_library(VulkanSCPCJson STATIC IMPORTED GLOBAL)
	add_library(VulkanSC::PCJson ALIAS VulkanSCPCJson)
	add_dependencies(VulkanSCPCJson VulkanSC-pcutil)
	set_target_properties(VulkanSCPCJson PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SOURCE_DIR}/include")
	if(GENERATOR_IS_MULTI_CONFIG)
		foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
			string(TOUPPER ${CONFIG} CONFIG_UPPER)
			set_target_properties(VulkanSCPCJson PROPERTIES IMPORTED_LOCATION_${CONFIG_UPPER} "${VulkanSCPCJson_LIBRARY_${CONFIG_UPPER}}")
		endforeach()
	else()
		set_target_properties(VulkanSCPCJson PROPERTIES IMPORTED_LOCATION "${VulkanSCPCJson_LIBRARY}")
	endif()
else ()
	message(FATAL_ERROR "vulkansc-pcutil not found")
endif ()
