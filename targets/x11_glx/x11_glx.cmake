
message("*** Using X11 GLX target")
set(DEQP_TARGET_NAME 	"X11 GLX")
set(DEQP_SUPPORT_GLES2	ON)
set(DEQP_SUPPORT_GLES3	ON)
set(DEQP_SUPPORT_OPENGL	ON)
set(DEQP_SUPPORT_EGL	OFF)
set(DEQP_SUPPORT_GLX	ON)

# Use X11 target
set(DEQP_USE_X11		ON)

find_package(X11)
if (NOT X11_FOUND)
	message(FATAL_ERROR "X11 development package not found")
endif ()

set(DEQP_PLATFORM_LIBRARIES ${X11_LIBRARIES})
include_directories(${X11_INCLUDE_DIR})
