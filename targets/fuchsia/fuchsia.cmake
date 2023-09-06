message("*** Fuchsia")
set(DEQP_TARGET_NAME	"Fuchsia")

# Fuchsia doesn't support OpenGL natively.
set(DEQP_SUPPORT_GLES2	OFF)
set(DEQP_SUPPORT_GLES3	OFF)
set(DEQP_SUPPORT_OPENGL	OFF)
set(DEQP_SUPPORT_EGL	OFF)

set(DEQP_PLATFORM_LIBRARIES "")
