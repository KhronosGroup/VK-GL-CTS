# Platform defines.
set(CMAKE_SYSTEM_NAME Linux)

set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

set(CMAKE_CROSSCOMPILING 1)

# dE defines
set(DE_OS		"DE_OS_ANDROID")
set(DE_COMPILER	"DE_COMPILER_GCC")
if (NOT DEFINED DE_ANDROID_API)
	set(DE_ANDROID_API 5)
endif ()

# NDK installation path
set(ANDROID_NDK_PATH	"/opt/android-ndk-r8b"	CACHE STRING "Android NDK installation path")
set(ANDROID_NDK_HOST_OS	"linux-x86"				CACHE STRING "Android ndk host os")
set(ANDROID_ABI			"armeabi-v7a"			CACHE STRING "Android ABI")
set(ANDROID_NDK_TARGET	"android-${DE_ANDROID_API}")

# Select cpu
if (ANDROID_ABI STREQUAL "x86")
	set(DE_CPU					"DE_CPU_X86")
	set(CMAKE_SYSTEM_PROCESSOR	i686-android-linux)
else ()
	set(DE_CPU					"DE_CPU_ARM")
	set(CMAKE_SYSTEM_PROCESSOR	arm-linux-androideabi)
endif ()

# Cross-compiler, search paths and sysroot
if (ANDROID_ABI STREQUAL "x86")
	set(ANDROID_CC_PATH	"${ANDROID_NDK_PATH}/toolchains/x86-4.6/prebuilt/${ANDROID_NDK_HOST_OS}/")
	set(CROSS_COMPILE	"${ANDROID_CC_PATH}bin/i686-linux-android-")
	set(ANDROID_SYSROOT	"${ANDROID_NDK_PATH}/platforms/${ANDROID_NDK_TARGET}/arch-x86")

	set(CMAKE_FIND_ROOT_PATH
		"${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.6/libs/${ANDROID_ABI}"
		"${ANDROID_CC_PATH}i686-linux-android"
		"${ANDROID_CC_PATH}lib/gcc/i686-linux-android/4.6"
		${ANDROID_SYSROOT}
		)

else ()
	set(ANDROID_CC_PATH	"${ANDROID_NDK_PATH}/toolchains/arm-linux-androideabi-4.6/prebuilt/${ANDROID_NDK_HOST_OS}/")
	set(CROSS_COMPILE	"${ANDROID_CC_PATH}bin/arm-linux-androideabi-")
	set(ANDROID_SYSROOT	"${ANDROID_NDK_PATH}/platforms/${ANDROID_NDK_TARGET}/arch-arm")

	set(CMAKE_FIND_ROOT_PATH
		"${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.6/libs/${ANDROID_ABI}"
		"${ANDROID_CC_PATH}arm-linux-androideabi"
		${ANDROID_SYSROOT}
		)

endif ()

# crtbegin_so.o & crtend_so.o \todo [pyry] Is there some special CMake variable for these?
if (DE_ANDROID_API GREATER 8)
	set(CRTBEGIN_SO	"${ANDROID_SYSROOT}/usr/lib/crtbegin_so.o")
	set(CRTEND_SO	"${ANDROID_SYSROOT}/usr/lib/crtend_so.o")
endif ()

# libgcc
if (ANDROID_ABI STREQUAL "armeabi-v7a")
	set(LIBGCC "${ANDROID_CC_PATH}lib/gcc/arm-linux-androideabi/4.6.x-google/armv7-a/libgcc.a")
elseif (ANDROID_ABI STREQUAL "armeabi")
	set(LIBGCC "${ANDROID_CC_PATH}lib/gcc/arm-linux-androideabi/4.6.x-google/libgcc.a")
elseif (ANDROID_ABI STREQUAL "x86")
	set(LIBGCC "${ANDROID_CC_PATH}lib/gcc/i686-linux-android/4.6.x-google/libgcc.a")
else ()
	message(FATAL_ERROR "Don't know where libgcc.a is")
endif ()

# C++ library
set(LIBCPP "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.6/libs/${ANDROID_ABI}/libgnustl_static.a")

include(CMakeForceCompiler)

if (ANDROID_NDK_HOST_OS STREQUAL "linux-x86" OR
	ANDROID_NDK_HOST_OS STREQUAL "darwin-x86")
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
  "<CMAKE_CXX_COMPILER> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> -o <TARGET> ${CRTBEGIN_SO} <OBJECTS> <LINK_LIBRARIES> ${LIBCPP} ${LIBGCC} ${CRTEND_SO}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(COMMON_C_FLAGS		"--sysroot=${ANDROID_SYSROOT} -fpic -Os -DANDROID -D__ANDROID__ -D__STDC_INT64__")
set(COMMON_LINKER_FLAGS	"-nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined")
set(ARM_C_FLAGS			"-mfloat-abi=softfp -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ ")
set(ARM_LINKER_FLAGS	"-Wl,--fix-cortex-a8")

if (ANDROID_ABI STREQUAL "armeabi-v7a")
	# For armeabi-v7a
	set(TARGET_C_FLAGS		"${COMMON_C_FLAGS} ${ARM_C_FLAGS} -march=armv7-a -mfpu=vfpv3-d16")
	set(TARGET_LINKER_FLAGS	"${COMMON_LINKER_FLAGS} ${ARM_LINKER_FLAGS} -march=armv7-a")

elseif (ANDROID_ABI STREQUAL "armeabi")
	# For armeabi (ARMv5TE)
	set(TARGET_C_FLAGS		"${COMMON_C_FLAGS} ${ARM_C_FLAGS} -march=armv5te")
	set(TARGET_LINKER_FLAGS	"${COMMON_LINKER_FLAGS} ${ARM_LINKER_FLAGS} -march=armv5te")

elseif (ANDROID_ABI STREQUAL "x86")
	set(TARGET_C_FLAGS		"${COMMON_C_FLAGS} -march=i686 -msse3 -mstackrealign -mfpmath=sse")
	set(TARGET_LINKER_FLAGS	"${COMMON_LINKER_FLAGS}")

else ()
	message(FATAL_ERROR "Unknown Android ABI \"${ANDROID_ABI}\"")
endif ()

# \note Hacky workaround for flags...
set(CMAKE_C_FLAGS	"${TARGET_C_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS	"${TARGET_C_FLAGS} -I${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.6/include -I${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.6/libs/${ANDROID_ABI}/include" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${TARGET_LINKER_FLAGS}" CACHE STRING "" FORCE)
