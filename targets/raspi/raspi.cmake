# Raspberry Pi target
message("*** Using Raspberry Pi")
set(DEQP_TARGET_NAME	"Raspberry Pi")
set(DEQP_SUPPORT_GLES2	ON)
set(DEQP_SUPPORT_VG		ON)
set(DEQP_SUPPORT_EGL	ON)

find_path(SYSTEM_INCLUDE bcm_host.h PATHS /usr/include /opt/vc/include)
include_directories(
	${SYSTEM_INCLUDE}
	${SYSTEM_INCLUDE}/interface/vcos/pthreads
	)

# GLESv2 lib
find_library(GLES2_LIBRARY GLESv2 PATHS /usr/lib /opt/vc/lib)
set(DEQP_GLES2_LIBRARIES ${GLES2_LIBRARY})

# OpenVG lib
find_library(OPENVG_LIBRARY OpenVG PATHS /usr/lib /opt/vc/lib)
set(DEQP_VG_LIBRARIES ${OPENVG_LIBRARY})

# EGL lib
find_library(EGL_LIBRARY EGL PATHS /usr/lib /opt/vc/lib)
set(DEQP_EGL_LIBRARIES ${EGL_LIBRARY})

# Platform libs
find_library(BCM_HOST_LIBRARY NAMES bcm_host PATHS /usr/lib /opt/vc/lib)
set(DEQP_PLATFORM_LIBRARIES ${DEQP_PLATFORM_LIBRARIES} ${BCM_HOST_LIBRARY} ${GLES2_LIBRARY} ${EGL_LIBRARY})

get_filename_component(SYSLIB_PATH ${BCM_HOST_LIBRARY} PATH)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${SYSLIB_PATH}")

# Platform sources
set(TCUTIL_PLATFORM_SRCS
	raspi/tcuRaspiPlatform.cpp
	raspi/tcuRaspiPlatform.hpp
	tcuMain.cpp
	)
