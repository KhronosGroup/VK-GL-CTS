#cmake file for spirv-tools

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

# Assume build inside Android source tree
set(DEFAULT_SPIRV_TOOLS_SRC_PATH "../spirv-tools")

set(SPIRV_TOOLS_SRC_PATH ${DEFAULT_SPIRV_TOOLS_SRC_PATH} CACHE STRING "Path to spirv-tools source tree")
if (IS_ABSOLUTE ${SPIRV_TOOLS_SRC_PATH})
	set(SPIRV_TOOLS_ABS_PATH ${SPIRV_TOOLS_SRC_PATH})
else ()
	set(SPIRV_TOOLS_ABS_PATH "${CMAKE_SOURCE_DIR}/${SPIRV_TOOLS_SRC_PATH}")
endif ()

if (EXISTS ${SPIRV_TOOLS_ABS_PATH}/source/opcode.cpp)
	message(STATUS "spirv-tools found; building with DEQP_HAVE_SPIRV_TOOLS")
	set(CMAKE_C_FLAGS ${DE_3RD_PARTY_C_FLAGS})
	set(CMAKE_CXX_FLAGS ${DE_3RD_PARTY_CXX_FLAGS})

	set(DEQP_HAVE_SPIRV_TOOLS		ON					PARENT_SCOPE)
	set(SPIRV_SKIP_EXECUTABLES		ON)
	add_subdirectory(${SPIRV_TOOLS_ABS_PATH} spirv-tools)
else()
	message(STATUS "spirv-tools not found; SPIR-V assembly not available")
	set(DEQP_HAVE_SPIRV_TOOLS		OFF					PARENT_SCOPE)
endif()
