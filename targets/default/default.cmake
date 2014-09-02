
message("*** Default target")

set(DEQP_TARGET_NAME 	"Default")

# OpenGL (ES) tests do not require any libraries or headers
set(DEQP_SUPPORT_GLES2	ON)
set(DEQP_SUPPORT_GLES3	ON)
set(DEQP_SUPPORT_OPENGL	ON)

# For static linking
find_library(GLES2_LIBRARY		NAMES libGLESv2 GLESv2)
find_library(EGL_LIBRARY		NAMES libEGL EGL)

find_path(GLES2_INCLUDE_PATH	GLES2/gl2.h)
find_path(GLES3_INCLUDE_PATH	GLES3/gl3.h)
find_path(EGL_INCLUDE_PATH		EGL/egl.h)

if (GLES2_LIBRARY AND GLES2_INCLUDE_PATH)
	set(DEQP_GLES2_LIBRARIES ${GLES2_LIBRARY})
endif ()

if (GLES2_LIBRARY AND GLES3_INCLUDE_PATH)
	# Assume that GLESv2 provides ES3 symbols if GLES3/gl3.h was found
	set(DEQP_GLES3_LIBRARIES ${GLES2_LIBRARY})
endif ()

if (EGL_LIBRARY AND EGL_INCLUDE_PATH)
	set(DEQP_SUPPORT_EGL		ON)
	set(DEQP_EGL_LIBRARIES		${EGL_LIBRARY})
	include_directories(${EGL_INCLUDE_PATH})
endif ()

# OpenCL support?
find_library(OPENCL_LIBRARY		NAMES libOpenCL OpenCL)
find_path(OPENCL_INCLUDE_PATH	CL/cl.h OpenCL/cl.h)

message("OPENCL_LIBRARY = ${OPENCL_LIBRARY}")
message("OPENCL_INCLUDE_PATH = ${OPENCL_INCLUDE_PATH}")

if (OPENCL_LIBRARY AND OPENCL_INCLUDE_PATH)
	set(DEQP_SUPPORT_OPENCL		ON)
	set(DEQP_OPENCL_LIBRARIES	${OPENCL_LIBRARY})
	include_directories(${OPENCL_INCLUDE_PATH})
endif ()

# X11 / GLX?
if (DE_OS_IS_UNIX)
	find_package(X11)
	if (X11_FOUND)
		set(DEQP_USE_X11 ON)
	endif ()

	set(DEQP_PLATFORM_LIBRARIES ${X11_LIBRARIES})
	include_directories(${X11_INCLUDE_DIR})
endif ()
