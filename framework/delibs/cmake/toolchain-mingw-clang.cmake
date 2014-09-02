# Platform defines.
set(CMAKE_SYSTEM_NAME Windows)
set(DE_CPU		"DE_CPU_X86")
set(DE_OS		"DE_OS_WIN32")
set(DE_COMPILER	"DE_COMPILER_CLANG")

# Base directories.
set(MINGW_BASE "C:/mingw"		CACHE STRING "MinGW base directory")

set(CMAKE_C_COMPILER	"${MINGW_BASE}/bin/clang.exe")
set(CMAKE_CXX_COMPILER	"${MINGW_BASE}/bin/clang++.exe")

# Search and include files (but not programs or libs) from toolchain dir.
set(CMAKE_FIND_ROOT_PATH "${MINGW_BASE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY FIRST)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE FIRST)

