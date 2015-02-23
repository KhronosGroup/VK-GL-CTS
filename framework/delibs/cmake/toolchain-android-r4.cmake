# Platform defines.
set(CMAKE_SYSTEM_NAME		Linux)
set(CMAKE_SYSTEM_PROCESSOR	arm-eabi)

set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

set(CMAKE_CROSSCOMPILING 1)

# dE defines
set(DE_CPU		"DE_CPU_ARM")
set(DE_OS		"DE_OS_ANDROID")
set(DE_COMPILER	"DE_COMPILER_GCC")

# NDK installation path
set(ANDROID_NDK_PATH	"/opt/android-ndk-r4/"	CACHE STRING "Android NDK installation path")
set(ANDROID_NDK_HOST_OS	"linux-x86"				CACHE STRING "Android ndk host os")
set(ANDROID_NDK_TARGET	"android-5"				CACHE STRING "Android target")
set(ANDROID_ABI			"armeabi-v7a"			CACHE STRING "Android ABI")

# \todo [pyry] Detect host type
set(ANDROID_CC_PATH	"${ANDROID_NDK_PATH}/build/prebuilt/${ANDROID_NDK_HOST_OS}/arm-eabi-4.4.0/")
set(CROSS_COMPILE	"${ANDROID_CC_PATH}bin/arm-eabi-")

include(CMakeForceCompiler)

if (ANDROID_NDK_HOST_OS STREQUAL "linux-x86")
	cmake_force_c_compiler("${CROSS_COMPILE}gcc"		GNU)
	cmake_force_cxx_compiler("${CROSS_COMPILE}g++"		GNU)
elseif (ANDROID_NDK_HOST_OS STREQUAL "windows")
	cmake_force_c_compiler("${CROSS_COMPILE}gcc.exe"	GNU)
	cmake_force_cxx_compiler("${CROSS_COMPILE}g++.exe"	GNU)
else ()
	message(FATAL_ERROR "Unknown ANDROID_NDK_HOST_OS")
endif ()

set(CMAKE_SHARED_LIBRARY_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "")

set(CMAKE_C_CREATE_SHARED_LIBRARY
  "<CMAKE_C_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_C_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
set(CMAKE_CXX_CREATE_SHARED_LIBRARY
  "<CMAKE_CXX_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")


# Search libs and include files (but not programs) from toolchain dir.
set(CMAKE_FIND_ROOT_PATH
	"${ANDROID_NDK_PATH}/build/platforms/${ANDROID_NDK_TARGET}/arch-arm"
	"${ANDROID_CC_PATH}arm-eabi"
	)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

include_directories(
	"${ANDROID_NDK_PATH}/build/platforms/${ANDROID_NDK_TARGET}/arch-arm/usr/include"
	)

link_directories(
	"${ANDROID_NDK_PATH}/build/platforms/${ANDROID_NDK_TARGET}/arch-arm/usr/lib"
	)

set(NDK_FLAGS "-fpic -mthumb-interwork -ffunction-sections -funwind-tables -fstack-protector -fno-short-enums -mfloat-abi=softfp -Os -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ -DANDROID -D__ANDROID__ -D__STDC_INT64__")

if (ANDROID_ABI STREQUAL "armeabi-v7a")
	# For armeabi-v7a
	set(NDK_FLAGS "${NDK_FLAGS} -march=armv7-a -mfpu=vfp")

elseif (ANDROID_ABI STREQUAL "armeabi")
	# For armeabi (ARMv5TE)
	set(NDK_FLAGS "${NDK_FLAGS} -march=armv5te")

else ()
	message(FATAL_ERROR "Unknown Android ABI \"${ANDROID_ABI}\"")
endif ()

# \note Hacky workaround for flags...
set(CMAKE_C_FLAGS	"${NDK_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS	"${NDK_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "-nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined" CACHE STRING "" FORCE)
