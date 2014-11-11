# Platform defines.
set(CMAKE_SYSTEM_NAME Linux)

set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

set(CMAKE_CROSSCOMPILING 1)

# NDK installation path
if (NOT DEFINED ANDROID_NDK_PATH)
	message(FATAL_ERROR "Please provide ANDROID_NDK_PATH")
endif ()

# Host os (for toolchain binaries)
if (NOT DEFINED ANDROID_NDK_HOST_OS)
	message(STATUS "Warning: ANDROID_NDK_HOST_OS is not set")
	if (WIN32)
		set(ANDROID_NDK_HOST_OS "windows")
	elseif (UNIX)
		set(ANDROID_NDK_HOST_OS "linux-86")
	endif ()
endif ()

# Compile target
set(ANDROID_ABI			"armeabi-v7a"			CACHE STRING "Android ABI")
set(ANDROID_NDK_TARGET	"android-${DE_ANDROID_API}")

# dE defines
set(DE_OS		"DE_OS_ANDROID")
set(DE_COMPILER	"DE_COMPILER_GCC")
if (NOT DEFINED DE_ANDROID_API)
	set(DE_ANDROID_API 9)
endif ()

set(COMMON_C_FLAGS		"-mandroid -D__STDC_INT64__")
set(COMMON_CXX_FLAGS	"${COMMON_C_FLAGS} -frtti -fexceptions")
set(COMMON_LINKER_FLAGS	"")
set(ARM_C_FLAGS			"-D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ ")

# ABI-dependent bits
if (ANDROID_ABI STREQUAL "x86")
	set(DE_CPU					"DE_CPU_X86")
	set(CMAKE_SYSTEM_PROCESSOR	i686-android-linux)

	set(ANDROID_CC_PATH			"${ANDROID_NDK_PATH}/toolchains/x86-4.9/prebuilt/${ANDROID_NDK_HOST_OS}/")
	set(CROSS_COMPILE			"${ANDROID_CC_PATH}bin/i686-linux-android-")
	set(ANDROID_SYSROOT			"${ANDROID_NDK_PATH}/platforms/${ANDROID_NDK_TARGET}/arch-x86")
	set(LIBGCC					"${ANDROID_CC_PATH}lib/gcc/i686-linux-android/4.9/libgcc.a")

	set(CMAKE_FIND_ROOT_PATH
		"${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.9/libs/${ANDROID_ABI}"
		"${ANDROID_CC_PATH}i686-linux-android"
		"${ANDROID_CC_PATH}lib/gcc/i686-linux-android/4.9"
		${ANDROID_SYSROOT}
		)

	set(TARGET_C_FLAGS			"-march=i686 -msse3 -mstackrealign -mfpmath=sse")
	set(TARGET_LINKER_FLAGS		"")

elseif (ANDROID_ABI STREQUAL "armeabi" OR
		ANDROID_ABI STREQUAL "armeabi-v7a")
	set(DE_CPU					"DE_CPU_ARM")
	set(CMAKE_SYSTEM_PROCESSOR	arm-linux-androideabi)

	set(ANDROID_CC_PATH	"${ANDROID_NDK_PATH}/toolchains/arm-linux-androideabi-4.9/prebuilt/${ANDROID_NDK_HOST_OS}/")
	set(CROSS_COMPILE	"${ANDROID_CC_PATH}bin/arm-linux-androideabi-")
	set(ANDROID_SYSROOT	"${ANDROID_NDK_PATH}/platforms/${ANDROID_NDK_TARGET}/arch-arm")

	if (ANDROID_ABI STREQUAL "armeabi-v7a")
		set(TARGET_C_FLAGS		"${ARM_C_FLAGS} -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=softfp")
		set(TARGET_LINKER_FLAGS	"-Wl,--fix-cortex-a8 -march=armv7-a")
		set(LIBGCC				"${ANDROID_CC_PATH}lib/gcc/arm-linux-androideabi/4.9/armv7-a/libgcc.a")

	else () # armeabi
		set(TARGET_C_FLAGS		"${ARM_C_FLAGS} -march=armv5te -mfloat-abi=softfp")
		set(TARGET_LINKER_FLAGS	"-Wl,--fix-cortex-a8 -march=armv5te")
		set(LIBGCC				"${ANDROID_CC_PATH}lib/gcc/arm-linux-androideabi/4.9/libgcc.a")
	endif ()

	set(CMAKE_FIND_ROOT_PATH
		"${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.9/libs/${ANDROID_ABI}"
		"${ANDROID_CC_PATH}arm-linux-androideabi"
		${ANDROID_SYSROOT}
		)

elseif (ANDROID_ABI STREQUAL "arm64-v8a")
	set(DE_CPU					"DE_CPU_ARM_64")
	set(CMAKE_SYSTEM_PROCESSOR	aarch64-linux-android)
	set(CMAKE_SIZEOF_VOID_P		8)

	set(ANDROID_CC_PATH	"${ANDROID_NDK_PATH}/toolchains/aarch64-linux-android-4.9/prebuilt/${ANDROID_NDK_HOST_OS}/")
	set(CROSS_COMPILE	"${ANDROID_CC_PATH}bin/aarch64-linux-android-")
	set(ANDROID_SYSROOT	"${ANDROID_NDK_PATH}/platforms/${ANDROID_NDK_TARGET}/arch-arm64")
	set(LIBGCC			"${ANDROID_CC_PATH}lib/gcc/aarch64-linux-android/4.9/libgcc.a")

	set(CMAKE_FIND_ROOT_PATH
		"${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.9/libs/${ANDROID_ABI}"
		"${ANDROID_CC_PATH}arm-linux-androideabi"
		${ANDROID_SYSROOT}
		)

	set(TARGET_C_FLAGS		"-march=armv8-a -mabi=lp64")
	set(TARGET_LINKER_FLAGS	"-march=armv8-a")

else ()
	message(FATAL_ERROR "Unknown ABI \"${ANDROID_ABI}\"")
endif ()

# crtbegin_so.o & crtend_so.o \todo [pyry] Is there some special CMake variable for these?
if (DE_ANDROID_API GREATER 8)
	set(CRTBEGIN_SO	"${ANDROID_SYSROOT}/usr/lib/crtbegin_so.o")
	set(CRTEND_SO	"${ANDROID_SYSROOT}/usr/lib/crtend_so.o")
endif ()

# C++ library
set(LIBCPP "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.9/libs/${ANDROID_ABI}/libgnustl_static.a")

include(CMakeForceCompiler)

if (ANDROID_NDK_HOST_OS STREQUAL "linux-x86" OR
	ANDROID_NDK_HOST_OS STREQUAL "linux-x86_64" OR
	ANDROID_NDK_HOST_OS STREQUAL "darwin-x86")
	cmake_force_c_compiler("${CROSS_COMPILE}gcc"		GNU)
	cmake_force_cxx_compiler("${CROSS_COMPILE}g++"		GNU)
elseif (ANDROID_NDK_HOST_OS STREQUAL "windows")
	cmake_force_c_compiler("${CROSS_COMPILE}gcc.exe"	GNU)
	cmake_force_cxx_compiler("${CROSS_COMPILE}g++.exe"	GNU)
else ()
	message(FATAL_ERROR "Unknown ANDROID_NDK_HOST_OS")
endif ()

set(CMAKE_SHARED_LIBRARY_C_FLAGS	"")
set(CMAKE_SHARED_LIBRARY_CXX_FLAGS	"")

set(CMAKE_C_CREATE_SHARED_LIBRARY	"<CMAKE_C_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_C_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
set(CMAKE_CXX_CREATE_SHARED_LIBRARY	"<CMAKE_CXX_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> -o <TARGET> ${CRTBEGIN_SO} <OBJECTS> <LINK_LIBRARIES> ${LIBCPP} ${LIBGCC} ${CRTEND_SO}")
set(CMAKE_CXX_LINK_EXECUTABLE		"<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES> ${LIBCPP} ${LIBGCC}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# \note Without CACHE STRING FORCE cmake ignores these.
set(CMAKE_C_FLAGS				"--sysroot=${ANDROID_SYSROOT} ${COMMON_C_FLAGS} ${TARGET_C_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS				"--sysroot=${ANDROID_SYSROOT} ${COMMON_CXX_FLAGS} ${TARGET_C_FLAGS} -I${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.9/include -I${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.9/libs/${ANDROID_ABI}/include" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS	"-nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined ${COMMON_LINKER_FLAGS} ${TARGET_LINKER_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS		"${COMMON_LINKER_FLAGS} ${TARGET_LINKER_FLAGS}" CACHE STRING "" FORCE)
