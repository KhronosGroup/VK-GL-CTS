# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2024 The Android Open Source Project
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

import os
import posixpath
from fnmatch import fnmatch

from ctsbuild.common import DEQP_DIR, writeFile, which, execute

# source code to compile into static lib libkhronoscts_common
LIB_KHRONOS_CTS_COMMON_SRC_ROOTS = [
	"execserver",
	"executor",
	"framework/common",
	"framework/delibs",
	"framework/egl",
	"framework/opengl",
	"framework/qphelper",
	"framework/randomshaders",
	"framework/referencerenderer",
	"framework/xexml",
]

LIB_KHRONOS_CTS_COMMON_INCLUDE_PATTERNS = [
	"*.cpp",
	"*.c",
]

LIB_KHRONOS_CTS_COMMON_EXCLUDE_PATTERNS = [
	"execserver/xsWin32TestProcess.cpp",
	"framework/delibs/dethread/standalone_test.c",
	"framework/randomshaders/rsgTest.cpp",
	"execserver/tools/*",
	"executor/tools/*",
]

LIB_KHRONOS_CTS_COMMON_EXTRA_INCLUDE_DIRS = []

# source code to compile into static lib libkhronoscts_modules_gles
LIB_KHRONOS_CTS_MODULES_SRC_ROOTS = [
	"modules",
]

LIB_KHRONOS_CTS_MODULES_INCLUDE_PATTERNS = [
	"*.cpp",
	"*.c",
]

LIB_KHRONOS_CTS_MODULES_EXCLUDE_PATTERNS = [
	"modules/egl/teglTestPackageEntry.cpp",
	"modules/gles2/tes2TestPackageEntry.cpp",
	"modules/gles3/tes3TestPackageEntry.cpp",
	"modules/gles31/tes31TestPackageEntry.cpp",
	"modules/internal/ditTestPackageEntry.cpp",
]

LIB_KHRONOS_CTS_MODULES_EXTRA_INCLUDE_DIRS = [
	"external/vulkancts/framework/vulkan",
	"external/vulkancts/framework/vulkan/generated/vulkan",
]

# source code to compile into static lib libkhronoscts_openglcts
LIB_KHRONOS_CTS_OPENGLCTS_SRC_ROOTS = [
	"external/openglcts",
]

LIB_KHRONOS_CTS_OPENGLCTS_INCLUDE_PATTERNS = [
	"*.cpp",
	"*.c",
]

LIB_KHRONOS_CTS_OPENGLCTS_EXCLUDE_PATTERNS = [
	"external/openglcts/modules/gl/gl4cContextFlushControlTests.cpp",
	"external/openglcts/modules/glcTestPackageEntry.cpp",
	"external/openglcts/modules/runner/glcAndroidMain.cpp",
	"external/openglcts/modules/runner/glcAndroidTestActivity.cpp",
	"external/openglcts/modules/runner/glcTestRunner.cpp",
	"external/openglcts/modules/runner/glcTestRunnerMain.cpp",
]

LIB_KHRONOS_CTS_OPENGLCTS_EXTRA_INCLUDE_DIRS = [
]

# source code to compile into static lib libkhronoscts_vulkancts
LIB_KHRONOS_CTS_VULKANCTS_SRC_ROOTS = [
	"external/vulkancts",
]

LIB_KHRONOS_CTS_VULKANCTS_INCLUDE_PATTERNS = [
	"*.cpp",
	"*.c",
]

LIB_KHRONOS_CTS_VULKANCTS_EXCLUDE_PATTERNS = [
	"external/vulkancts/framework/vulkan/vkRenderDocUtil.cpp",
	"external/vulkancts/modules/vulkan/vktBuildPrograms.cpp",
	"external/vulkancts/modules/vulkan/sc/*",
	"external/vulkancts/modules/vulkan/vktTestPackageEntrySC.cpp",
	"external/vulkancts/vkscpc/*",
	"external/vulkancts/vkscserver/*",
	"external/vulkancts/modules/vulkan/video/*",
]

LIB_KHRONOS_CTS_VULKANCTS_EXTRA_INCLUDE_DIRS = [
	"external/vulkancts/framework/vulkan/generated/vulkan",
]

# source code to compile into static lib libkhronoscts_platform
LIB_KHRONOS_CTS_PLATFORM_SRC_ROOTS = [
	"framework/platform/android",
	"framework/platform/surfaceless",
	"external/openglcts/modules/runner",
]

LIB_KHRONOS_CTS_PLATFORM_INCLUDE_PATTERNS = [
	"*.cpp",
	"*.c",
]

LIB_KHRONOS_CTS_PLATFORM_EXCLUDE_PATTERNS = [
	"external/openglcts/modules/runner/glcAndroidMain.cpp",
	"framework/platform/android/tcuAndroidJNI.cpp",
	"framework/platform/android/tcuAndroidMain.cpp",
	"framework/platform/android/tcuAndroidPlatformCapabilityQueryJNI.cpp",
]

LIB_KHRONOS_CTS_PLATFORM_EXTRA_INCLUDE_DIRS = [
]

AUTO_GEN_WARNING = """
// WARNING: This is auto-generated file. Do not modify, since changes will
// be lost! Modify scripts/gen_khronos_cts_bp.py instead.
"""

CC_LIBRARY_KHRONOS_COMPILE_OPTION = """
cc_defaults {
    name: "khronosctscompilationflag_default",
    cppflags: [
        "-fexceptions",
        "-Wno-non-virtual-dtor",
        "-Wno-delete-non-virtual-dtor",
        "-Wno-implicit-int-conversion",
        "-Wno-implicit-float-conversion",
        "-Wno-unused-function",
        "-Wno-enum-float-conversion",
        "-Wno-missing-field-initializers",
        "-Wno-switch",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
    ],

    cpp_std: "c++17",
    cflags: [
        // Amber defines.
        "-DAMBER_CTS_VULKAN_HEADER=1",
        "-DAMBER_ENABLE_CLSPV=0",
        "-DAMBER_ENABLE_DXC=0",
        "-DAMBER_ENABLE_LODEPNG=1", // This has no effect.
        "-DAMBER_ENABLE_RTTI=1",
        "-DAMBER_ENABLE_SHADERC=0",
        "-DAMBER_ENABLE_SPIRV_TOOLS=0",
        "-DAMBER_ENABLE_VK_DEBUGGING=0",
        "-DAMBER_ENGINE_DAWN=0",
        "-DAMBER_ENGINE_VULKAN=1",

        // glslang defines:
        "-DENABLE_HLSL",
        "-DENABLE_OPT=0",
        "-DGLSLANG_OSINCLUDE_UNIX",

        // SPIRV-Tools defines:
        "-DSPIRV_ANDROID",
        "-DSPIRV_CHECK_CONTEXT",
        "-DSPIRV_COLOR_TERMINAL",
        "-DSPIRV_TIMER_ENABLED",

        // Android/Clang defines (not needed):
        // -D_FORTIFY_SOURCE=2
        // -DANDROID
        // -DNDEBUG

        // dEQP defines that we don't want/need:
        // -DDE_DEBUG
        // -DDEQP_USE_RELEASE_INFO_FILE
        // -DPNG_DEBUG

        // dEQP defines that are worked out in deDefs.h, without needing
        // explicit defs:
        // -DDE_PTR_SIZE=8
        // -DDE_CPU=DE_CPU_ARM_64
        //"-DDE_FENV_ACCESS_ON=_Pragma(\"STDC FENV_ACCESS ON\")",

        // dEQP defines:
        "-D_XOPEN_SOURCE=600",
        "-DDE_ANDROID_API=28",
        "-DDE_ASSERT_FAILURE_CALLBACK",
        "-DDE_COMPILER=DE_COMPILER_CLANG",
        "-DDE_MINGW=0",
        "-DDE_OS=DE_OS_ANDROID",
        "-DDEQP_GLES2_DIRECT_LINK=1",
        "-DDEQP_HAVE_RENDERDOC_HEADER=0", // Needs to be 0.
        "-DDEQP_EXCLUDE_VK_VIDEO_TESTS",
        "-DDEQP_SUPPORT_DRM=0",
        "-DDEQP_SUPPORT_GLES1=1",
        "-DDEQP_TARGET_NAME=\\"Android\\"",
        "-DQP_SUPPORT_PNG",
        "-DCTS_USES_VULKAN",

        "-Wall",
        "-Werror",
        "-Wconversion",

        "-fwrapv",

        "-Wno-implicit-fallthrough",
        "-Wno-sign-conversion",
        "-Wno-unused-private-field",
        "-Wno-shorten-64-to-32",
    ],

    include_dirs: [
        "external/deqp-deps/SPIRV-Headers/include",
    ],

    sdk_version: "current",
    rtti: true,
    stl: "c++_static",
}
"""

CC_LIBRARY_STATIC_TEMPLATE = """

cc_library_static {
    name: "{CC_STATIC_LIB_NAME}",

    defaults: ["khronosctscompilationflag_default"],

    srcs: [
{SRC_FILES} ],
    export_include_dirs: [
{EXPORT_INCLUDES} ],
    static_libs: [
{STATIC_LIBS} ],
}

"""[1:-1]

def matchesAny (filename, patterns):
	for ptrn in patterns:
		if fnmatch(filename, ptrn):
			return True
	return False

def isSourceFile (filename, include_patterns, exclude_patterns):
	return matchesAny(filename, include_patterns) and not matchesAny(filename, exclude_patterns)

def toPortablePath (nativePath):
	# os.path is so convenient...
	head, tail	= os.path.split(nativePath)
	components	= [tail]

	while head != None and head != '':
		head, tail = os.path.split(head)
		components.append(tail)

	components.reverse()

	portablePath = ""
	for component in components:
		portablePath = posixpath.join(portablePath, component)

	return portablePath

def getSourceFiles (src_roots, include_patterns, exclude_patterns):
	sources = []

	for srcRoot in src_roots:
		baseDir = os.path.join(DEQP_DIR, srcRoot)
		for root, dirs, files in os.walk(baseDir):
			for file in files:
				absPath			= os.path.join(root, file)
				nativeRelPath	= os.path.relpath(absPath, DEQP_DIR)
				portablePath	= toPortablePath(nativeRelPath)

				if isSourceFile(portablePath, include_patterns, exclude_patterns):
					sources.append(portablePath)

	sources.sort()

	return sources

def getSourceDirs (sourceFiles, extra_include_dirs):
	seenDirs	= set()
	sourceDirs	= []

	for sourceFile in sourceFiles:
		sourceDir = posixpath.dirname(sourceFile)

		if not sourceDir in seenDirs:
			sourceDirs.append(sourceDir)
			seenDirs.add(sourceDir)

	sourceDirs.extend(extra_include_dirs)
	sourceDirs.sort()

	return sourceDirs

def genBpStringList (items):
	src = ""

	for item in items:
		src += "        \"%s\",\n" % item

	return src

def genCCStaticLibrary (ccStaticLibName, sourceDirs, sourceFiles, staticLibs):
	src = CC_LIBRARY_STATIC_TEMPLATE
	src = src.replace("{EXPORT_INCLUDES}", genBpStringList(sourceDirs))
	src = src.replace("{SRC_FILES}", genBpStringList(sourceFiles))
	src = src.replace("{CC_STATIC_LIB_NAME}", ccStaticLibName)
	src = src.replace("{STATIC_LIBS}", genBpStringList(staticLibs))

	return src

if __name__ == "__main__":
	# Android bp content for compiling static lib libkhronoscts_common
	libKhronosCTSCommonSourceFiles			= getSourceFiles(LIB_KHRONOS_CTS_COMMON_SRC_ROOTS,
															LIB_KHRONOS_CTS_COMMON_INCLUDE_PATTERNS,
															LIB_KHRONOS_CTS_COMMON_EXCLUDE_PATTERNS)
	libKhronosCTSCommonSourceDirs			= getSourceDirs(libKhronosCTSCommonSourceFiles,
															LIB_KHRONOS_CTS_COMMON_EXTRA_INCLUDE_DIRS)
	libKhronosCTSCommonStaticLibs			= ["libpng_ndk"]
	libKhronosCTSCommonAndroidBpText		= genCCStaticLibrary("libkhronoscts_common",
																libKhronosCTSCommonSourceDirs,
																libKhronosCTSCommonSourceFiles,
															libKhronosCTSCommonStaticLibs)

	# Android bp content for compiling static lib libkhronoscts_modules_gles
	libKhronosCTSModulesSourceFiles			= getSourceFiles(LIB_KHRONOS_CTS_MODULES_SRC_ROOTS,
														LIB_KHRONOS_CTS_MODULES_INCLUDE_PATTERNS,
														LIB_KHRONOS_CTS_MODULES_EXCLUDE_PATTERNS)
	libKhronosCTSModulesSourceDirs			= getSourceDirs(libKhronosCTSModulesSourceFiles,
																LIB_KHRONOS_CTS_MODULES_EXTRA_INCLUDE_DIRS)
	libKhronosCTSModulesStaticLibs			= ["libkhronoscts_common"]
	libKhronosCTSModulesAndroidBpText		= genCCStaticLibrary("libkhronoscts_modules_gles",
																	libKhronosCTSModulesSourceDirs,
																	libKhronosCTSModulesSourceFiles,
																	libKhronosCTSModulesStaticLibs)

	# Android bp content for compiling static lib libkhronoscts_openglcts
	libKhronosCTSOpenGLCTSSourceFiles		= getSourceFiles(LIB_KHRONOS_CTS_OPENGLCTS_SRC_ROOTS,
													LIB_KHRONOS_CTS_OPENGLCTS_INCLUDE_PATTERNS,
													LIB_KHRONOS_CTS_OPENGLCTS_EXCLUDE_PATTERNS)
	libKhronosCTSOpenGLCTSSourceDirs		= getSourceDirs(libKhronosCTSOpenGLCTSSourceFiles,
													LIB_KHRONOS_CTS_OPENGLCTS_EXTRA_INCLUDE_DIRS)
	libKhronosCTSOpenGLCTSStaticLibs		= ["libkhronoscts_common",
										"libkhronoscts_modules_gles",
										"deqp_glslang_SPIRV",
										"deqp_spirv-tools"]
	libKhronosCTSOpenGLCTSAndroidBpText		= genCCStaticLibrary("libkhronoscts_openglcts",
															libKhronosCTSOpenGLCTSSourceDirs,
															libKhronosCTSOpenGLCTSSourceFiles,
															libKhronosCTSOpenGLCTSStaticLibs)

	# Android bp content for compiling static lib libkhronoscts_vulkancts
	libKhronosCTSVulkanCTSSourceFiles		= getSourceFiles(LIB_KHRONOS_CTS_VULKANCTS_SRC_ROOTS,
													LIB_KHRONOS_CTS_VULKANCTS_INCLUDE_PATTERNS,
													LIB_KHRONOS_CTS_VULKANCTS_EXCLUDE_PATTERNS)
	libKhronosCTSVulkanCTSSourceDirs		= getSourceDirs(libKhronosCTSVulkanCTSSourceFiles,
													LIB_KHRONOS_CTS_VULKANCTS_EXTRA_INCLUDE_DIRS)
	libKhronosCTSVulkanCTSStaticLibs		= ["libkhronoscts_common",
										"deqp_glslang_glslang",
										"deqp_spirv-tools",
										"deqp_amber"]
	libKhronosCTSVulkanCTSAndroidBpText		= genCCStaticLibrary("libkhronoscts_vulkancts",
														libKhronosCTSVulkanCTSSourceDirs,
														libKhronosCTSVulkanCTSSourceFiles,
														libKhronosCTSVulkanCTSStaticLibs)

	# Android bp content for compiling libkhronoscts_platform
	libKhronosCTSPlatformSourceFiles		= getSourceFiles(LIB_KHRONOS_CTS_PLATFORM_SRC_ROOTS,
												LIB_KHRONOS_CTS_PLATFORM_INCLUDE_PATTERNS,
												LIB_KHRONOS_CTS_PLATFORM_EXCLUDE_PATTERNS)
	libKhronosCTSPlatformSourceDirs			= getSourceDirs(libKhronosCTSPlatformSourceFiles,
													LIB_KHRONOS_CTS_PLATFORM_EXTRA_INCLUDE_DIRS)
	libKhronosCTSPlatformStaticLibs			= ["libkhronoscts_common",
										"libkhronoscts_modules_gles",
										"libkhronoscts_vulkancts",
										"libkhronoscts_openglcts"]
	libKhronosCTSPlatformAndroidBpText		= genCCStaticLibrary("libkhronoscts_platform",
														libKhronosCTSPlatformSourceDirs,
														libKhronosCTSPlatformSourceFiles,
														libKhronosCTSPlatformStaticLibs)

	# put everything together into the final Android bp content
	libKhronosCTSAndroidBpText				= "\n".join([AUTO_GEN_WARNING,
											CC_LIBRARY_KHRONOS_COMPILE_OPTION,
											libKhronosCTSCommonAndroidBpText,
											libKhronosCTSModulesAndroidBpText,
											libKhronosCTSOpenGLCTSAndroidBpText,
											libKhronosCTSVulkanCTSAndroidBpText,
											libKhronosCTSPlatformAndroidBpText])
	writeFile(os.path.join(DEQP_DIR, "AndroidKhronosCTSGen.bp"), libKhronosCTSAndroidBpText)
