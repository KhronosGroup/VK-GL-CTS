# cmake file for glslang

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/SPIRV/GlslangToSpv.cpp")
	set(DEFAULT_GLSLANG_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
else ()
	set(DEFAULT_GLSLANG_SRC_PATH "../glslang")
endif ()

set(GLSLANG_SRC_PATH ${DEFAULT_GLSLANG_SRC_PATH} CACHE STRING "Path to glslang source tree")

if (IS_ABSOLUTE ${GLSLANG_SRC_PATH})
	set(GLSLANG_ABS_PATH ${GLSLANG_SRC_PATH})
else ()
	set(GLSLANG_ABS_PATH "${CMAKE_SOURCE_DIR}/${GLSLANG_SRC_PATH}")
endif ()

if (EXISTS ${GLSLANG_ABS_PATH}/glslang/GenericCodeGen/CodeGen.cpp)
	message(STATUS "glslang found")

	# Disable glslang install as part of cmake install.
	set(SKIP_GLSLANG_INSTALL OFF CACHE BOOL "" FORCE)

	# Do not build glslang binaries.
	set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)

	set(ALLOW_EXTERNAL_SPIRV_TOOLS "../spirv-tools")

	# Overwrite the CMake flags for glslang with the our 3rd party ones.
	set(CMAKE_C_FLAGS	${DE_3RD_PARTY_C_FLAGS})
	set(CMAKE_CXX_FLAGS	${DE_3RD_PARTY_CXX_FLAGS})

	add_subdirectory(${GLSLANG_ABS_PATH})

	add_definitions(-DENABLE_HLSL)

	set(GLSLANG_INCLUDE_PATH	${GLSLANG_ABS_PATH}			PARENT_SCOPE)
	set(GLSLANG_LIBRARIES		glslang SPIRV SPVRemapper	PARENT_SCOPE)
else ()
	message(FATAL_ERROR "glslang not found")
endif ()
