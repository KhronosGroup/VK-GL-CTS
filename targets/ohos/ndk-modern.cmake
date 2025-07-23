#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2016 The Android Open Source Project
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

# Delegate most things to the NDK's cmake toolchain script

if (NOT DEFINED OHOS_NDK_PATH)
	message(FATAL_ERROR "Please provide OHOS_NDK_PATH")
endif ()

set(OHOS_PLATFORM "ohos-${DE_OHOS_API}")
set(OHOS_STL c++_static)
set(OHOS_CPP_FEATURES "rtti exceptions")

include(${OHOS_NDK_PATH}/build/linux/native/build/cmake/ohos.toolchain.cmake)

# The try_compile() used to verify the C/C++ compilers are sane tries to
# generate an executable, but doesn't seem to use the right compiler/linker
# options when cross-compiling, so it fails even when building an actual
# shared library or executable succeeds.
#
# I don't know why this doesn't affect simpler projects that use the NDK
# toolchain.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Set variables used by other parts of dEQP's build scripts

set(DE_OS "DE_OS_OHOS")

if (NOT DEFINED DE_COMPILER)
	set(DE_COMPILER	"DE_COMPILER_CLANG")
endif ()

if (OHOS_ABI STREQUAL "armeabi" OR
		OHOS_ABI STREQUAL "armeabi-v7a")
	set(DE_CPU "DE_CPU_ARM")
elseif (OHOS_ABI STREQUAL "arm64-v8a")
	set(DE_CPU "DE_CPU_ARM_64")
else ()
	message(FATAL_ERROR "Unknown ABI \"${OHOS_ABI}\"")
endif ()
