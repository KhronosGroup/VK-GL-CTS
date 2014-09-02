# Dynamic OpenCL

set(DEQP_TARGET_NAME "OpenCL (run-time linking)")

set(DEQP_SUPPORT_OPENGL		ON)
set(DEQP_SUPPORT_OPENCL		ON)

# Headers
include_directories(framework/opencl/inc)

# Library
add_subdirectory(wrappers/opencl_dynamic)
set(DEQP_OPENCL_LIBRARIES OpenCL)
