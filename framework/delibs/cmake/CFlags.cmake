#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

set(DE_COVERAGE_BUILD "OFF" CACHE STRING "Build with coverage instrumentation with GCC (ON/OFF)")

if (NOT DE_DEFS)
	message(FATAL_ERROR "Defs.cmake is not included")
endif ()

if (DE_OS_IS_ANDROID)
	set(GCC_COMMON_FLAGS		"-Wall -Wextra -Wshadow -Wundef -Wno-long-long")

	# For 3rd party sw disable all warnings
	set(DE_3RD_PARTY_C_FLAGS	"${CMAKE_C_FLAGS} -w")
	set(DE_3RD_PARTY_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -w")

	set(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} ${GCC_COMMON_FLAGS} -ansi -pedantic")
	set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} ${GCC_COMMON_FLAGS}") # Don't warn about va_list

elseif (DE_OS_IS_SYMBIAN)
	# \todo [kalle] Add our warning flags.
	# \todo [kalle] Soft-float WTF?
	# \todo [kalle] Emulator build
	set(WARNING_FLAGS           "-Wall -Wno-unknown-pragmas -Wno-parentheses -Wno-comment")

	if (DE_DEBUG)
		set(OPTIMIZATION_FLAGS "-g")
	else (DE_DEBUG)
		set(OPTIMIZATION_FLAGS "-O3")
	endif (DE_DEBUG)

    set(GCC_COMMON_FLAGS        "-march=armv5t -mapcs -pipe -nostdinc -msoft-float ${OPTIMIZATION_FLAGS} -include ${SYMBIAN_EPOCROOT}/include/gcce/gcce.h")

	# Should these be embedded directly into the flags? Is it possible that these lists get cleared?
	add_definitions("-D_UNICODE")
	add_definitions("-D__GCCE__ -D__MARM__ -D__MARM_ARMV5__ -D__EABI__")
	add_definitions("-D__SYMBIAN32__ -D__EPOC32__ -D__SERIES60_30__ -D__SERIES60_3X__")
	add_definitions("-D__EXE__")
	add_definitions("-D__PRODUCT_INLUDE__=${SYMBIAN_EPOCROOT}/include/variant/Symbian_OS.hrh")
	add_definitions("-DSYMBIAN_ENABLE_SPLIT_HEADERS")
	add_definitions("-D_POSIX_C_SOURCE=200112") # Value taken from epoc32/release/armv5/udeb/libcrt0.lib

	# \review [2011-08-31 pyry] CMake should set this automatically, right?
	if (DE_DEBUG)
		add_definitions("-D_DEBUG")
	endif (DE_DEBUG)

	include_directories(
		${SYMBIAN_EPOCROOT}/include/stdapis
		${SYMBIAN_EPOCROOT}/include/stdapis/stlportv5
		${SYMBIAN_EPOCROOT}/include/variant
		${SYMBIAN_EPOCROOT}/include
		${SYMBIAN_EPOCROOT}/include/platform
		${SYMBIAN_EPOCROOT}/include/esdl
		${SYMBIAN_EPOCROOT}/include/mw
		${SYMBIAN_EPOCROOT}/include/platform/mw
		${SYMBIAN_EPOCROOT}/include/libc
		${COMPILER_INCLUDE_DIR}
	)

	# -Wno-psabi == Don't warn about va_list
	# \note _STLP_USE_MALLOC=1 enables stlport malloc based allocator => our code needs to implement static function void* __malloc_alloc::allocate(size_t& __n)
	set(GCC_CXX_FLAGS           "${GCC_COMMON_FLAGS} -Wno-psabi -Wno-ctor-dtor-privacy -D__SUPPORT_CPP_EXCEPTIONS__ -D__SYMBIAN_STDCPP_SUPPORT__ -D__LEAVE_EQUALS_THROW__ -D_STLP_USE_MALLOC=1")
	# \todo [kalle] -ansi -pedantic
	set(CMAKE_C_FLAGS			"${CMAKE_C_FLAGS} ${WARNING_FLAGS} ${GCC_COMMON_FLAGS}")
	set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} ${GCC_CXX_FLAGS}")

	set(DE_3RD_PARTY_C_FLAGS	"${CMAKE_C_FLAGS} -w ${GCC_COMMON_FLAGS}")
	set(DE_3RD_PARTY_CXX_FLAGS	"${CMAKE_CXX_FLAGS} -w ${GCC_CXX_FLAGS}")

	# \todo [kalle] Add map file using TARGET_BASE variable?
	if (DE_DEBUG)
		set(SYMBIAN_PLATFORM_LINK_DIR "${SYMBIAN_EPOCROOT}/RELEASE/ARMV5/UDEB")
	else (DE_DEBUG)
		set(SYMBIAN_PLATFORM_LINK_DIR "${SYMBIAN_EPOCROOT}/RELEASE/ARMV5/UREL")
	endif (DE_DEBUG)
	set(CMAKE_C_LINK_FLAGS     "-Ttext 0x8000 -Tdata 0x400000 -Xlinker --target1-abs -Xlinker --no-undefined -Xlinker --default-symver -nostdlib -shared --entry _E32Startup -u _E32Startup ${SYMBIAN_LIB_DIR}/libc.dso ${SYMBIAN_LIB_DIR}/libm.dso ${SYMBIAN_LIB_DIR}/drtaeabi.dso ${SYMBIAN_LIB_DIR}/dfpaeabi.dso")

	set(CMAKE_CXX_LINK_FLAGS   "${CMAKE_C_LINK_FLAGS} ${SYMBIAN_LIB_DIR}/stdnew.dso ${SYMBIAN_LIB_DIR}/libstdcppv5.dso")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SYMBIAN_PLATFORM_LINK_DIR}/eexe.lib ${SYMBIAN_LIB_DIR}/euser.dso")
	link_directories(${COMPILER_LIB_DIR})

elseif (DE_COMPILER_IS_GCC)
	# Compiler flags for gcc

	# \note Add -Wconversion and -Wundef for more warnings
	set(GCC_COMMON_FLAGS "-Wall -Wextra -Wno-long-long -Wshadow -Wundef")

	if (DE_CPU_IS_X86)
		set(GCC_MACHINE_FLAGS "-m32")
	elseif (DE_CPU_IS_X86_64)
		set(GCC_MACHINE_FLAGS "-m64")
	endif ()

	if (DE_COVERAGE_BUILD)
		add_definitions("-DDE_COVERAGE_BUILD")
		set(GCC_MACHINE_FLAGS "${GCC_MACHINE_FLAGS} -fprofile-arcs -ftest-coverage")
		set(LINK_FLAGS "${LINK_FLAGS} -lgcov")
	endif ()

	# For 3rd party sw disable all warnings
	set(DE_3RD_PARTY_C_FLAGS   "${CMAKE_C_FLAGS} -w ${GCC_MACHINE_FLAGS}")
	set(DE_3RD_PARTY_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w ${GCC_MACHINE_FLAGS}")

	set(CMAKE_C_FLAGS	       "${CMAKE_C_FLAGS} ${GCC_COMMON_FLAGS} ${GCC_MACHINE_FLAGS} -ansi -pedantic ")
	set(CMAKE_CXX_FLAGS	       "${CMAKE_CXX_FLAGS} ${GCC_COMMON_FLAGS} ${GCC_MACHINE_FLAGS}")

elseif (DE_COMPILER_IS_MSC)
	# Compiler flags for msc

	# \note Following unnecessary nagging warnings are disabled:
	# 4820: automatic padding added after data
	# 4255: no function prototype given (from system headers)
	# 4668: undefined identifier in preprocessor expression (from system headers)
	# 4738: storing 32-bit float result in memory
	# 4711: automatic inline expansion
	set(MSC_BASE_FLAGS "/DWIN32 /D_WINDOWS /D_CRT_SECURE_NO_WARNINGS")
	set(MSC_WARNING_FLAGS "/W3 /wd4820 /wd4255 /wd4668 /wd4738 /wd4711")

	# For 3rd party sw disable all warnings
	set(DE_3RD_PARTY_C_FLAGS    "${CMAKE_C_FLAGS} ${MSC_BASE_FLAGS} /W0")
	set(DE_3RD_PARTY_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${MSC_BASE_FLAGS} /W0")

	set(CMAKE_C_FLAGS	         "${CMAKE_C_FLAGS} ${MSC_BASE_FLAGS} ${MSC_WARNING_FLAGS}")
	set(CMAKE_CXX_FLAGS	         "${CMAKE_CXX_FLAGS} ${MSC_BASE_FLAGS} ${MSC_WARNING_FLAGS}")

elseif (DE_COMPILER_IS_CLANG)
	# Compiler flags for clang

	# \note Add -Wconversion and -Wundef for more warnings
	set(CLANG_COMMON_FLAGS "-Wall -Wextra -Wno-long-long -Wshadow -Wundef")

	if (DE_CPU_IS_X86)
		set(CLANG_MACHINE_FLAGS "-m32")
	elseif (DE_CPU_IS_X86_64)
		set(CLANG_MACHINE_FLAGS "-m64")
	endif ()

	# For 3rd party sw disable all warnings
	set(DE_3RD_PARTY_C_FLAGS   "${CMAKE_C_FLAGS} -w ${CLANG_MACHINE_FLAGS}")
	set(DE_3RD_PARTY_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w ${CLANG_MACHINE_FLAGS} -frtti -fexceptions")

	set(CMAKE_C_FLAGS	       "${CMAKE_C_FLAGS} ${CLANG_COMMON_FLAGS} ${CLANG_MACHINE_FLAGS} -ansi -pedantic ")
	set(CMAKE_CXX_FLAGS	       "${CMAKE_CXX_FLAGS} ${CLANG_COMMON_FLAGS} ${CLANG_MACHINE_FLAGS} -frtti -fexceptions")

else ()
	message(FATAL_ERROR "DE_COMPILER is not valid")
endif ()
