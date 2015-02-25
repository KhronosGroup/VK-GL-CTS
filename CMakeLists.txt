# dEQP cmake file

cmake_minimum_required(VERSION 2.6)

# dEQP Target.
set(DEQP_TARGET "default" CACHE STRING "dEQP Target (default, android...)")

project(dEQP-Core-${DEQP_TARGET})

include(framework/delibs/cmake/Defs.cmake NO_POLICY_SCOPE)
include(framework/delibs/cmake/CFlags.cmake)

add_definitions(-DDE_ASSERT_FAILURE_CALLBACK)

# dEQP-specific configuration. Target file should override these.
set(DEQP_TARGET_NAME		"UNKNOWN")		# Target name

set(DEQP_SUPPORT_GLES1		OFF)			# Is GLESv1 supported
set(DEQP_GLES1_LIBRARIES	)				# GLESv1 libraries

set(DEQP_SUPPORT_GLES2		OFF)			# Is GLESv2 supported
set(DEQP_GLES2_LIBRARIES	)				# GLESv2 libraries. If empty, run-time linking is used

set(DEQP_SUPPORT_GLES3		OFF)			# Is GLESv3 supported
set(DEQP_GLES3_LIBRARIES	)				# GLESv3 libraries. If empty, run-time linking is used

set(DEQP_SUPPORT_VG			OFF)			# Is VG supported
set(DEQP_VG_LIBRARIES		)				# VG libraries

set(DEQP_SUPPORT_EGL		OFF)			# Is EGL supported
set(DEQP_EGL_LIBRARIES		)				# EGL libraries

set(DEQP_SUPPORT_GLX		OFF)			# Is GLX supported
set(DEQP_SUPPORT_WGL		OFF)			# Is WGL supported

set(DEQP_PLATFORM_LIBRARIES	)				# Other platform libraries

set(DEQP_SUPPORT_OPENGL		OFF)			# Is OpenGL supported on platform
											# \note OpenGL is always loaded on run-time

set(DEQP_PLATFORM_COPY_LIBRARIES	)		# Libraries / binaries that need to be copied to binary directory

# Delibs include directories
include_directories(
	framework/delibs/debase
	framework/delibs/decpp
	framework/delibs/depool
	framework/delibs/dethread
	framework/delibs/deutil
	framework/delibs/destream
	)

# Include target-specific definitions
include(targets/${DEQP_TARGET}/${DEQP_TARGET}.cmake)

# zlib
find_path(ZLIB_INCLUDE_PATH	zlib.h)
find_library(ZLIB_LIBRARY	z)

if (NOT ZLIB_INCLUDE_PATH OR NOT ZLIB_LIBRARY)
	message(STATUS "System version of zlib not found, using external/zlib")
	add_subdirectory(external/zlib)
	# \note ZLIB_LIBRARY and ZLIB_INCLUDE_PATH are promoted from external/zlib/CMakeLists.txt
endif ()

include_directories(${ZLIB_INCLUDE_PATH})

# libpng
find_path(PNG_INCLUDE_PATH	libpng.h)
find_library(PNG_LIBRARY	png)

if (NOT PNG_INCLUDE_PATH OR NOT PNG_LIBRARY)
	message(STATUS "System version of libpng not found, using external/libpng")
	add_subdirectory(external/libpng)
	# \note PNG_LIBRARY and PNG_INCLUDE_PATH are promoted from external/libpng/CMakeLists.txt
endif ()

include_directories(${PNG_INCLUDE_PATH})

message(STATUS "DEQP_TARGET_NAME        = ${DEQP_TARGET_NAME}")
message(STATUS "DEQP_SUPPORT_GLES1      = ${DEQP_SUPPORT_GLES1}")
message(STATUS "DEQP_GLES1_LIBRARIES    = ${DEQP_GLES1_LIBRARIES}")
message(STATUS "DEQP_SUPPORT_GLES2      = ${DEQP_SUPPORT_GLES2}")
message(STATUS "DEQP_GLES2_LIBRARIES    = ${DEQP_GLES2_LIBRARIES}")
message(STATUS "DEQP_SUPPORT_GLES3      = ${DEQP_SUPPORT_GLES3}")
message(STATUS "DEQP_GLES3_LIBRARIES    = ${DEQP_GLES3_LIBRARIES}")
message(STATUS "DEQP_SUPPORT_VG         = ${DEQP_SUPPORT_VG}")
message(STATUS "DEQP_VG_LIBRARIES       = ${DEQP_VG_LIBRARIES}")
message(STATUS "DEQP_SUPPORT_EGL        = ${DEQP_SUPPORT_EGL}")
message(STATUS "DEQP_EGL_LIBRARIES      = ${DEQP_EGL_LIBRARIES}")
message(STATUS "DEQP_SUPPORT_OPENGL     = ${DEQP_SUPPORT_OPENGL}")
message(STATUS "DEQP_PLATFORM_LIBRARIES = ${DEQP_PLATFORM_LIBRARIES}")
message(STATUS "DEQP_SUPPORT_WGL        = ${DEQP_SUPPORT_WGL}")
message(STATUS "DEQP_SUPPORT_GLX        = ${DEQP_SUPPORT_GLX}")

# Defines
add_definitions(-DDEQP_TARGET_NAME="${DEQP_TARGET_NAME}")

if (DEQP_SUPPORT_GLES1)
	add_definitions(-DDEQP_SUPPORT_GLES1=1)
endif ()

if (DEQP_SUPPORT_GLES2)
	add_definitions(-DDEQP_SUPPORT_GLES2=1)
endif ()

if (DEQP_SUPPORT_GLES3)
	add_definitions(-DDEQP_SUPPORT_GLES3=1)
endif ()

if (DEQP_SUPPORT_VG)
	add_definitions(-DDEQP_SUPPORT_VG=1)
endif ()

if (DEQP_SUPPORT_EGL)
	add_definitions(-DDEQP_SUPPORT_EGL=1)
endif ()

if (DEQP_SUPPORT_OPENGL)
	add_definitions(-DDEQP_SUPPORT_OPENGL=1)
endif ()

if (DEQP_SUPPORT_WGL)
	add_definitions(-DDEQP_SUPPORT_WGL=1)
endif ()

if (DEQP_SUPPORT_GLX)
	add_definitions(-DDEQP_SUPPORT_GLX=1)
endif ()

# Check runtime linking support
if (DEQP_SUPPORT_GLES1 AND NOT DEFINED DEQP_GLES1_LIBRARIES)
	message(FATAL_ERROR "Run-time loading of GLES1 is not supported (DEQP_GLES1_LIBRARIES is not set)")
endif ()

if (DEQP_SUPPORT_GLES2 AND NOT DEFINED DEQP_GLES2_LIBRARIES)
	add_definitions(-DDEQP_GLES2_RUNTIME_LOAD=1)
endif ()

if (DEQP_SUPPORT_GLES3 AND NOT DEFINED DEQP_GLES3_LIBRARIES)
	add_definitions(-DDEQP_GLES3_RUNTIME_LOAD=1)
endif ()

if (DEQP_SUPPORT_VG AND NOT DEFINED DEQP_VG_LIBRARIES)
	message(FATAL_ERROR "Run-time loading of VG is not supported (DEQP_VG_LIBRARIES is not set)")
endif ()

if (DEQP_SUPPORT_EGL AND NOT DEFINED DEQP_EGL_LIBRARIES)
	add_definitions(-DDEQP_EGL_RUNTIME_LOAD=1)
endif ()

# OpenGL is always loaded on run-time
if (DEQP_SUPPORT_OPENGL)
	add_definitions(-DDEQP_OPENGL_RUNTIME_LOAD=1)
endif ()

if (DE_COMPILER_IS_MSC)
	# Don't nag about std::copy for example
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_SCL_SECURE_NO_WARNINGS")
endif ()

# delibs projects
add_subdirectory(framework/delibs/debase)
add_subdirectory(framework/delibs/depool)
add_subdirectory(framework/delibs/dethread)
add_subdirectory(framework/delibs/destream)
add_subdirectory(framework/delibs/deutil)
add_subdirectory(framework/delibs/decpp)

# ExecServer
add_subdirectory(execserver)

# Executor framework and tools
if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/executor)
	add_subdirectory(executor)
endif ()

# Test framework include directories
include_directories(
	framework/common
	framework/qphelper
	framework/opengl
	framework/opengl/wrapper
	framework/referencerenderer
	framework/opengl/simplereference
	framework/randomshaders
	framework/egl
	framework/egl/wrapper
	)

if (DE_OS_IS_ANDROID OR DE_OS_IS_IOS)
	# On Android deqp modules are compiled as libraries and linked into final .so
	set(DEQP_MODULE_LIBRARIES )
	set(DEQP_MODULE_ENTRY_POINTS )
endif ()

if (DE_OS_IS_WIN32)
	include_directories(framework/platform/win32)
endif ()

# Macro for adding targets for copying binaries (usually target libraries) to the target destination dir
macro (target_copy_files target dep_name files)
	if (NOT "${files}" STREQUAL "")
		set(COPY_TARGETS )
		foreach (SRCNAME ${files})
			get_filename_component(BASENAME ${SRCNAME} NAME)
			set(DSTNAME "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}")
			add_custom_command(OUTPUT ${DSTNAME}
							   COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SRCNAME} ${DSTNAME})
			set(COPY_TARGETS ${COPY_TARGETS} ${DSTNAME})
		endforeach ()

		add_custom_target(${dep_name} ALL DEPENDS ${COPY_TARGETS})
		add_dependencies(${target} ${dep_name})
	endif ()
endmacro (target_copy_files)

# Macro for adding dEQP module
macro (add_deqp_module MODULE_NAME SRCS LIBS ENTRY)
	if (DE_OS_IS_ANDROID OR DE_OS_IS_IOS)
		# Single-binary targets
		add_library(${MODULE_NAME} STATIC ${SRCS})
		target_link_libraries(${MODULE_NAME} ${LIBS})

		set(DEQP_MODULE_LIBRARIES		${DEQP_MODULE_LIBRARIES} ${MODULE_NAME})
		set(DEQP_MODULE_ENTRY_POINTS	${DEQP_MODULE_ENTRY_POINTS} "${CMAKE_CURRENT_SOURCE_DIR}/${ENTRY}")

		# Forward to parent scope
		set(DEQP_MODULE_LIBRARIES		${DEQP_MODULE_LIBRARIES} PARENT_SCOPE)
		set(DEQP_MODULE_ENTRY_POINTS	${DEQP_MODULE_ENTRY_POINTS} PARENT_SCOPE)

	else ()
		# Separate binary per target
		add_executable(${MODULE_NAME} ${CMAKE_SOURCE_DIR}/framework/platform/tcuMain.cpp ${ENTRY} ${SRCS})
		target_link_libraries(${MODULE_NAME} tcutil-platform ${LIBS})
		target_copy_files(${MODULE_NAME} platform-libs-${MODULE_NAME} "${DEQP_PLATFORM_COPY_LIBRARIES}")
	endif ()

	# Data file target
	add_custom_target(${MODULE_NAME}-data)
	add_dependencies(${MODULE_NAME} ${MODULE_NAME}-data)
endmacro (add_deqp_module)

# Macro for adding data dirs to module
macro (add_data_dir MODULE_NAME SRC_DIR DST_DIR)
	if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX)
		add_custom_command(TARGET ${MODULE_NAME}-data POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${DST_DIR})

	elseif (DE_OS_IS_ANDROID)
		add_custom_command(TARGET ${MODULE_NAME}-data POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR} ${CMAKE_BINARY_DIR}/assets/${DST_DIR})

	elseif (DE_OS_IS_IOS)
		add_custom_command(TARGET ${MODULE_NAME}-data POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR} ${CMAKE_BINARY_DIR}/\${CONFIGURATION}\${EFFECTIVE_PLATFORM_NAME}/deqp.app/${DST_DIR})
	endif ()
endmacro (add_data_dir)

# Macro for adding individual data files to module
macro (add_data_file MODULE_NAME SRC_FILE DST_FILE)
	if (DE_OS_IS_WIN32 OR DE_OS_IS_UNIX OR DE_OS_IS_OSX)
		add_custom_command(TARGET ${MODULE_NAME}-data POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${DST_FILE})

	elseif (DE_OS_IS_ANDROID)
		add_custom_command(TARGET ${MODULE_NAME}-data POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE} ${CMAKE_BINARY_DIR}/assets/${DST_FILE})

	elseif (DE_OS_IS_IOS)
		add_custom_command(TARGET ${MODULE_NAME}-data POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FILE} ${CMAKE_BINARY_DIR}/\${CONFIGURATION}\${EFFECTIVE_PLATFORM_NAME}/deqp.app/${DST_FILE})
	endif ()
endmacro (add_data_file)

add_subdirectory(framework)

if (DE_COMPILER_IS_MSC)
	add_compile_options(/bigobj) # Required by glsBuiltinPrecisionTests.cpp
endif ()

add_subdirectory(modules)

# Single-binary targets
if (DE_OS_IS_ANDROID)
	include_directories(executor)

	add_library(deqp SHARED framework/platform/android/tcuAndroidMain.cpp framework/platform/android/tcuAndroidJNI.cpp framework/platform/android/tcuAndroidPlatformCapabilityQueryJNI.cpp framework/platform/android/tcuTestLogParserJNI.cpp ${DEQP_MODULE_ENTRY_POINTS})
	target_link_libraries(deqp tcutil-platform xecore ${DEQP_MODULE_LIBRARIES})

elseif (DE_OS_IS_IOS)
	# Code sign identity
	set(DEQP_IOS_CODE_SIGN_IDENTITY "drawElements" CACHE STRING "Code sign identity for iOS build")

	set(MACOSX_BUNDLE_PRODUCT_NAME "\${PRODUCT_NAME}")
	set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.drawelements.\${PRODUCT_NAME:identifier}")

	include_directories(framework/platform/ios)
	set(TESTERCORE_SRC_FILES
		framework/platform/ios/tcuEAGLView.h
		framework/platform/ios/tcuEAGLView.m
		framework/platform/ios/tcuIOSAppDelegate.h
		framework/platform/ios/tcuIOSAppDelegate.m
		framework/platform/ios/tcuIOSViewController.h
		framework/platform/ios/tcuIOSViewController.m
		framework/platform/ios/tcuIOSMain.m
		)
	set_source_files_properties(${TESTERCORE_SRC_FILES} COMPILE_FLAGS "-std=c99")

	add_executable(deqp MACOSX_BUNDLE ${TESTERCORE_SRC_FILES} ${DEQP_MODULE_ENTRY_POINTS})
	target_link_libraries(deqp tcutil-platform xscore ${DEQP_MODULE_LIBRARIES})
	set_target_properties(deqp PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
	set_target_properties(deqp PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer: ${DEQP_IOS_CODE_SIGN_IDENTITY}")
endif ()
