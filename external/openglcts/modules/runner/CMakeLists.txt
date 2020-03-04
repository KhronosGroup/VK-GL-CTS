set(GLCTS_RUNNER_SRCS
	glcTestRunner.cpp
	glcTestRunner.hpp
	)

if (DEQP_USE_X11)
	add_definitions(-DDEQP_SUPPORT_X11=1)
endif ()

if (DEQP_SUPPORT_WGL)
	add_definitions(-DDEQP_SUPPORT_WGL=1)
endif ()

if (DE_OS_IS_ANDROID)
	include_directories(${PROJECT_SOURCE_DIR}/framework/platform/android)

	set(GLCTS_RUNNER_SRCS
		${GLCTS_RUNNER_SRCS}
		glcAndroidTestActivity.cpp
		glcAndroidTestActivity.hpp
		)
endif ()

add_library(glcts-runner STATIC ${GLCTS_RUNNER_SRCS})
