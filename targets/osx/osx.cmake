# OS X Target

set(DEQP_TARGET_NAME	"OS X")

set(DEQP_SUPPORT_OPENGL	ON)
set(DEQP_SUPPORT_OPENCL	ON)

find_package(OpenGL REQUIRED)
set(DEQP_PLATFORM_LIBRARIES ${OPENGL_LIBRARIES})
include_directories(${OPENGL_INCLUDE_DIRS})

find_path(OPENCL_INC NAMES CL/cl.h OpenCL/cl.h)
if (NOT OPENCL_INC)
	message(FATAL_ERROR "Can't find OpenCL headers")
endif ()
include_directories(${OPENCL_INC})

find_library(OPENCL_LIB NAMES OpenCL libOpenCL)
set(DEQP_OPENCL_LIBRARIES ${OPENCL_LIB})

# Apple versions of cl headers do not respect CL_USE_DEPRECATED_OPENCL_1_X_APIs
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
