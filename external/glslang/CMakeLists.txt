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

find_package(BISON)

# \todo [2015-06-24 pyry] Full C++11 support on Android requires using CLang + libc++
if (NOT BISON_FOUND AND DE_OS_IS_WIN32 AND EXISTS ${GLSLANG_ABS_PATH}/tools/bison.exe)
	message(STATUS "Using pre-built bison executable")
	set(BISON_EXECUTABLE ${GLSLANG_ABS_PATH}/tools/bison.exe)
	set(BISON_FOUND ON)
endif ()

if (BISON_FOUND AND EXISTS ${GLSLANG_ABS_PATH}/glslang/GenericCodeGen/CodeGen.cpp AND NOT DE_OS_IS_ANDROID)
	message(STATUS "glslang found; building with DEQP_SUPPORT_GLSLANG")

	include_directories(
		${GLSLANG_ABS_PATH}
		${GLSLANG_ABS_PATH}/glslang
		${GLSLANG_ABS_PATH}/glslang/Include
		${GLSLANG_ABS_PATH}/glslang/Public
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent
		${GLSLANG_ABS_PATH}/glslang/GenericCodeGen
		${GLSLANG_ABS_PATH}/glslang/OSDependent
		${GLSLANG_ABS_PATH}/OGLCompilersDLL
		${GLSLANG_ABS_PATH}/SPIRV
		${CMAKE_CURRENT_BINARY_DIR}
		)

	set(GLSLANG_SRCS
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/Constant.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/InfoSink.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/Initialize.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/IntermTraverse.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/Intermediate.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/ParseHelper.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/PoolAlloc.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/RemoveTree.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/Scan.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/ShaderLang.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/SymbolTable.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/Versions.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/intermOut.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/limits.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/linkValidate.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/parseConst.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/reflection.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/Pp.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/PpAtom.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/PpContext.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/PpMemory.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/PpScanner.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/PpSymbols.cpp
		${GLSLANG_ABS_PATH}/glslang/MachineIndependent/preprocessor/PpTokens.cpp
		${GLSLANG_ABS_PATH}/glslang/GenericCodeGen/CodeGen.cpp
		${GLSLANG_ABS_PATH}/glslang/GenericCodeGen/Link.cpp
		${GLSLANG_ABS_PATH}/OGLCompilersDLL/InitializeDll.cpp

		${GLSLANG_ABS_PATH}/SPIRV/GlslangToSpv.cpp
		${GLSLANG_ABS_PATH}/SPIRV/SpvBuilder.cpp
		${GLSLANG_ABS_PATH}/SPIRV/SPVRemapper.cpp
		${GLSLANG_ABS_PATH}/SPIRV/doc.cpp
		${GLSLANG_ABS_PATH}/SPIRV/disassemble.cpp

		${CMAKE_CURRENT_BINARY_DIR}/glslang_tab.cpp
		${CMAKE_CURRENT_BINARY_DIR}/glslang_tab.cpp.h

		osinclude.cpp
		)

	set(CMAKE_C_FLAGS	${DE_3RD_PARTY_C_FLAGS})
	set(CMAKE_CXX_FLAGS	${DE_3RD_PARTY_CXX_FLAGS})

	if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	endif ()

	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/glslang_tab.cpp ${CMAKE_CURRENT_BINARY_DIR}/glslang_tab.cpp.h
					   COMMAND ${BISON_EXECUTABLE} --defines=${CMAKE_CURRENT_BINARY_DIR}/glslang_tab.cpp.h -t MachineIndependent/glslang.y -o ${CMAKE_CURRENT_BINARY_DIR}/glslang_tab.cpp
					   MAIN_DEPENDENCY ${GLSLANG_ABS_PATH}/glslang/MachineIndependent/glslang.y
					   WORKING_DIRECTORY ${GLSLANG_ABS_PATH}/glslang)

	add_library(glslang STATIC ${GLSLANG_SRCS})
	target_link_libraries(glslang dethread ${ZLIB_LIBRARY})

	set(GLSLANG_INCLUDE_PATH	${GLSLANG_ABS_PATH}	PARENT_SCOPE)
	set(GLSLANG_LIBRARY			glslang				PARENT_SCOPE)
	set(DEQP_HAVE_GLSLANG		ON					PARENT_SCOPE)

else ()
	message(STATUS "glslang not found; GLSL to SPIR-V compilation not available")

	set(DEQP_HAVE_GLSLANG		OFF					PARENT_SCOPE)

endif ()
