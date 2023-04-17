#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2016 The Android Open Source Project
# Copyright (c) 2016 The Khronos Group Inc.
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

message("*** Default target")

set(DEQP_TARGET_NAME	"Default")

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
link_directories(/usr/local/lib)
endif ()

# For static linking
find_library(GLES2_LIBRARY		NAMES libGLESv2 GLESv2)
find_library(EGL_LIBRARY		NAMES libEGL EGL)

find_path(GLES2_INCLUDE_PATH	GLES2/gl2.h)
find_path(GLES3_INCLUDE_PATH	GLES3/gl3.h)
find_path(EGL_INCLUDE_PATH		EGL/egl.h)

if (GLES2_LIBRARY AND GLES2_INCLUDE_PATH)
	if (GLES_ALLOW_DIRECT_LINK)
		set(DEQP_GLES2_LIBRARIES ${GLES2_LIBRARY})
	endif ()
	include_directories(${GLES2_INCLUDE_PATH})
endif ()

if (GLES2_LIBRARY AND GLES3_INCLUDE_PATH AND GLES_ALLOW_DIRECT_LINK)
	# Assume that GLESv2 provides ES3 symbols if GLES3/gl3.h was found
	set(DEQP_GLES3_LIBRARIES ${GLES2_LIBRARY})
endif ()

if (EGL_LIBRARY AND EGL_INCLUDE_PATH)
	if (GLES_ALLOW_DIRECT_LINK)
		set(DEQP_EGL_LIBRARIES ${EGL_LIBRARY})
	endif ()
	include_directories(${EGL_INCLUDE_PATH})
endif ()

# X11 / GLX?
if (DE_OS_IS_UNIX)
	find_package(X11)
	if (X11_FOUND)
		set(DEQP_USE_X11		ON)
		set(DEQP_SUPPORT_GLX	ON)
	endif ()

	set(DEQP_PLATFORM_LIBRARIES ${X11_LIBRARIES})
	include_directories(${X11_INCLUDE_DIR})

	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/targets/default")

	# Use XCB target if available
	set(DEQP_USE_XCB OFF)
	find_package(XCB)
	if (XCB_FOUND)
		set(DEQP_USE_XCB ON)
		set(DEQP_PLATFORM_LIBRARIES ${XCB_LIBRARIES})
		include_directories(${XCB_INCLUDE_DIR})
	endif ()
	find_package(Wayland)
	if (WAYLAND_FOUND)
		set(DEQP_USE_WAYLAND ON)
		set(DEQP_PLATFORM_LIBRARIES ${WAYLAND_LIBRARIES})
		include_directories(${WAYLAND_INCLUDE_DIR})
	endif ()
endif ()

# Win32?
if (DE_OS_IS_WIN32)
	set(DEQP_SUPPORT_WGL ON)
endif ()

# MacOS?
if (DE_OS_IS_OSX)
	find_package(OpenGL REQUIRED)
	find_library(COCOA_LIBRARY Cocoa)
	find_library(QUARTZCORE_LIBRARY QuartzCore)
	set(DEQP_PLATFORM_LIBRARIES ${OPENGL_LIBRARIES} ${COCOA_LIBRARY} ${QUARTZCORE_LIBRARY})
	include_directories(${OPENGL_INCLUDE_DIRS})
endif()
