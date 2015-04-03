
message("*** Using null context target")

set(DEQP_TARGET_NAME 	"Null")
set(DEQP_SUPPORT_EGL	ON)
set(DEQP_SUPPORT_GLES2	ON)
set(DEQP_SUPPORT_GLES3	ON)
set(DEQP_SUPPORT_OPENGL	ON)

set(TCUTIL_PLATFORM_SRCS
	null/tcuNullPlatform.cpp
	null/tcuNullPlatform.hpp
	null/tcuNullRenderContext.cpp
	null/tcuNullRenderContext.hpp
	)
