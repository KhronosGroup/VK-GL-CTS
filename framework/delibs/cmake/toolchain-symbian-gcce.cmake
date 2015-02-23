
# DE defines
set(DE_COMPILER	"DE_COMPILER_GCC")
set(DE_OS       "DE_OS_SYMBIAN")
set(DE_CPU      "DE_CPU_ARM")

# switch off the compiler tests -- these error out unnecessarily otherwise
SET(CMAKE_C_COMPILER_WORKS    1)
SET(CMAKE_C_COMPILER_FORCED   1)
SET(CMAKE_CXX_COMPILER_WORKS  1)
SET(CMAKE_CXX_COMPILER_FORCED 1)

# Set prefixes and suffixes for targets
SET(CMAKE_STATIC_LIBRARY_PREFIX "")
SET(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")
SET(CMAKE_SHARED_LIBRARY_PREFIX "")
SET(CMAKE_SHARED_LIBRARY_SUFFIX ".dso")
SET(CMAKE_IMPORT_LIBRARY_PREFIX "")
SET(CMAKE_IMPORT_LIBRARY_SUFFIX ".lib")
SET(CMAKE_EXECUTABLE_SUFFIX ".exe")
SET(CMAKE_LINK_LIBRARY_SUFFIX ".lib")
SET(CMAKE_DL_LIBS "")

set(CMAKE_SYSTEM_NAME "Symbian")

set(CMAKE_FIND_LIBRARY_PREFIXES "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dso")

# Symbian Epoc32 directory setup.
set(SYMBIAN_SDK_PATH	"c:/Nokia/devices/Nokia_Symbian3_SDK_v1.0" CACHE STRING "Symbian SDK root directory")
set(SYMBIAN_EPOCROOT	${SYMBIAN_SDK_PATH}/Epoc32)

# Codesourcery compiler setup
set(ARM_CC_BASE				"c:/Program Files/CodeSourcery/Sourcery G++ Lite" CACHE STRING "CodeSourcery ARM ELF compiler path")
set(CROSS_COMPILER_PREFIX	"${ARM_CC_BASE}/bin/arm-none-symbianelf-" CACHE STRING "Cross compiler prefix")

# CMAKE compiler executables
set(CMAKE_C_COMPILER "${CROSS_COMPILER_PREFIX}gcc.exe")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILER_PREFIX}g++.exe")
set(CMAKE_LINKER 		"${CROSS_COMPILER_PREFIX}ld.exe")

# Some shortcut variables for later use. See CFlags.cmake
# TODO: Should these be in CFlags.cmake instead?
set(COMPILER_INCLUDE_DIR "${ARM_CC_BASE}/lib/gcc/arm-none-symbianelf/4.4.1/include")
set(COMPILER_LIB_DIR ${ARM_CC_BASE}/lib/gcc/arm-none-symbianelf/4.4.1 ${ARM_CC_BASE}/arm-none-symbianelf/lib)

set(SYMBIAN_LIB_DIR ${SYMBIAN_EPOCROOT}/release/armv5/lib)

# Search libs and include files (but not programs) from toolchain dir.
set(CMAKE_FIND_ROOT_PATH ${SYMBIAN_EPOCROOT}/lib/gcc/arm-none-symbianelf/4.4.1 ${SYMBIAN_EPOCROOT} ${SYMBIAN_EPOCROOT}/release/armv5 ${SYMBIAN_EPOCROOT}/release/armv5/lib)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# for nmake make long command lines are redirected to a file
# with the following syntax, see Windows-bcc32.cmake for use
if (CMAKE_GENERATOR MATCHES "NMake")
	set(CMAKE_START_TEMP_FILE "@<<\n")
	set(CMAKE_END_TEMP_FILE "\n<<")
endif (CMAKE_GENERATOR MATCHES "NMake")
