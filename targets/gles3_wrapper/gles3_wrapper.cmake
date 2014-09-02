
message("*** Using GLES3 Wrapper")

set(DEQP_TARGET_NAME 	"GLES3 Wrapper")
set(DEQP_SUPPORT_GLES2	OFF)
set(DEQP_SUPPORT_EGL	OFF)
set(DEQP_SUPPORT_GLES3	ON)

add_definitions(-DKHRONOS_STATIC_LIB)
add_definitions(-DDEQP_USE_GLES3_WRAPPER)
include_directories(
	wrappers/gles3/inc
	wrappers/gles3 # Required by platform integration
	)
add_subdirectory(wrappers/gles3)
set(DEQP_GLES3_LIBRARIES	GLESv3)
set(DEQP_PLATFORM_LIBRARIES	GLESv3) # \note Always link to GLESv3 since platform integration requires it.

if (DE_OS_IS_WIN32)
	set(TCUTIL_PLATFORM_SRCS
		win32/tcuWGL.cpp
		win32/tcuWGL.hpp
		win32/tcuWin32API.h
		win32/tcuWin32Window.cpp
		win32/tcuWin32Window.hpp
		win32/tcuWin32GLES3Platform.cpp
		win32/tcuWin32GLES3Platform.hpp
		tcuMain.cpp
		)
else ()
	message(FATAL_ERROR "GLES3 Wrapper is not supported on this OS")
endif ()
