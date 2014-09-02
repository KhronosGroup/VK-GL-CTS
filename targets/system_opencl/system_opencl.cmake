# System OpenCL

set(DEQP_TARGET_NAME "System OpenCL")

set(DEQP_SUPPORT_GLES2		OFF)
set(DEQP_SUPPORT_GLES1		OFF)
set(DEQP_SUPPORT_OPENVG		OFF)
set(DEQP_SUPPORT_EGL		OFF)
set(DEQP_SUPPORT_OPENCL		ON)

# Headers
find_path(OPENCL_INC NAMES CL/cl.h OpenCL/cl.h)
if (NOT OPENCL_INC)
	message(FATAL_ERROR "Can't find OpenCL headers")
endif ()
include_directories(${OPENCL_INC})

# Libraries
find_library(OPENCL_LIB NAMES OpenCL libOpenCL)
set(DEQP_OPENCL_LIBRARIES ${OPENCL_LIB})
