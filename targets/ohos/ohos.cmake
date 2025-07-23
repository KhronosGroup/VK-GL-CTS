#-------------------------------------------------------------------------
# drawElements CMake utilities
# ----------------------------
#
# Copyright 2016 The OHOS Open Source Project
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

# OHOS
message("*** Using OHOS")
set(DEQP_TARGET_NAME	"OHOS")
# set(DEQP_SUPPORT_GLES1	ON)

# [wshi] TODO: 
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
