# iOS Target

set(DEQP_TARGET_NAME	"iOS")
set(DEQP_SUPPORT_GLES2	ON)
set(DEQP_SUPPORT_GLES3	ON)
set(DEQP_SUPPORT_EGL	OFF)

# Libraries
find_library(GLES2_LIBRARY			NAMES	OpenGLES		PATHS /System/Library/Frameworks)
find_library(FOUNDATION_LIBRARY		NAMES	Foundation		PATHS /System/Library/Frameworks)
find_library(UIKIT_LIBRARY			NAMES	UIKit			PATHS /System/Library/Frameworks)
find_library(COREGRAPHICS_LIBRARY	NAMES	CoreGraphics	PATHS /System/Library/Frameworks)
find_library(QUARTZCORE_LIBRARY		NAMES	QuartzCore		PATHS /System/Library/Frameworks)

set(DEQP_GLES2_LIBRARIES		${GLES2_LIBRARY})
set(DEQP_GLES3_LIBRARIES		${GLES2_LIBRARY})
set(DEQP_PLATFORM_LIBRARIES		${FOUNDATION_LIBRARY} ${UIKIT_LIBRARY} ${COREGRAPHICS_LIBRARY} ${QUARTZCORE_LIBRARY})

# Execserver is compiled in
include_directories(execserver)
