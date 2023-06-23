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

include_directories(${PROJECT_BINARY_DIR}/external/vulkancts/framework/vulkan)

if (DE_OS_IS_ANDROID)
	include_directories(${PROJECT_SOURCE_DIR}/framework/platform/android)
	# The Android platform file includes the Vulkan header, which needs the generated files
	# to complete the header. SC is not supported, so include the non-SC generated folder.
	include_directories(${PROJECT_SOURCE_DIR}/external/vulkancts/framework/vulkan/generated/vulkan)

	set(GLCTS_RUNNER_SRCS
		${GLCTS_RUNNER_SRCS}
		glcAndroidTestActivity.cpp
		glcAndroidTestActivity.hpp
		)
endif ()

add_library(glcts-runner STATIC ${GLCTS_RUNNER_SRCS})
add_dependencies(glcts-runner deqp-vk-inl)
