# Platform port library

# Target file may define TCUTIL_PLATFORM_SRCS
if (NOT DEFINED TCUTIL_PLATFORM_SRCS)
	if (DE_OS_IS_WIN32)
			set(TCUTIL_PLATFORM_SRCS
				win32/tcuWin32Platform.hpp
				win32/tcuWin32Platform.cpp
				win32/tcuWGLContextFactory.hpp
				win32/tcuWGLContextFactory.cpp
				win32/tcuWGL.hpp
				win32/tcuWGL.cpp
				win32/tcuWin32API.h
				win32/tcuWin32Window.cpp
				win32/tcuWin32Window.hpp
			)

		if (DEQP_SUPPORT_EGL)
			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				win32/tcuWin32EGLNativeDisplayFactory.hpp
				win32/tcuWin32EGLNativeDisplayFactory.cpp
				)
		endif()
	elseif ((DE_OS_IS_UNIX OR DE_OS_IS_OSX) AND DEQP_USE_X11)
		set(TCUTIL_PLATFORM_SRCS
			X11/tcuX11.cpp
			X11/tcuX11.hpp
			X11/tcuX11Platform.hpp
			X11/tcuX11Platform.cpp
			)
		if (DEQP_SUPPORT_EGL)
			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				X11/tcuX11EglPlatform.hpp
				X11/tcuX11EglPlatform.cpp
				)
		endif()
		if (DEQP_SUPPORT_GLX)
			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				X11/tcuX11GlxPlatform.hpp
				X11/tcuX11GlxPlatform.cpp
				)
		endif()
		if (NOT (DEQP_SUPPORT_EGL OR DEQP_SUPPORT_GLX))
		  message(FATAL_ERROR "At least one of EGL and GLX must be enabled for X11")
		endif ()
	elseif (DE_OS_IS_ANDROID)
		set(TCUTIL_PLATFORM_SRCS
			android/tcuAndroidExecService.cpp
			android/tcuAndroidExecService.hpp
			)

		if (DE_ANDROID_API GREATER 8)
			# Add NativeActivity code
			set(TCUTIL_PLATFORM_SRCS
				${TCUTIL_PLATFORM_SRCS}
				android/tcuAndroidAssets.cpp
				android/tcuAndroidAssets.hpp
				android/tcuAndroidInternals.cpp
				android/tcuAndroidInternals.hpp
				android/tcuAndroidNativeActivity.cpp
				android/tcuAndroidNativeActivity.hpp
				android/tcuAndroidPlatform.cpp
				android/tcuAndroidPlatform.hpp
				android/tcuAndroidRenderActivity.cpp
				android/tcuAndroidRenderActivity.hpp
				android/tcuAndroidTestActivity.cpp
				android/tcuAndroidTestActivity.hpp
				android/tcuAndroidUtil.cpp
				android/tcuAndroidUtil.hpp
				android/tcuAndroidWindow.cpp
				android/tcuAndroidWindow.hpp
				)
		endif ()

	elseif (DE_OS_IS_IOS)
		set(TCUTIL_PLATFORM_SRCS
			ios/tcuIOSApp.mm
			ios/tcuIOSApp.h
			ios/tcuIOSPlatform.mm
			ios/tcuIOSPlatform.hh
			)

	elseif (DE_OS_IS_OSX)
		set(TCUTIL_PLATFORM_SRCS
			osx/tcuOSXPlatform.cpp
			osx/tcuOSXPlatform.hpp
			)

	else ()
		set(TCUTIL_PLATFORM_SRCS
			vanilla/tcuVanillaPlatform.cpp
			)

	endif ()
endif ()

add_library(tcutil-platform STATIC ${TCUTIL_PLATFORM_SRCS})
target_link_libraries(tcutil-platform tcutil ${TCUTIL_PLATFORM_LIBS})

# Always link to glutil as some platforms such as Win32 always support GL
target_link_libraries(tcutil-platform glutil)

# Link to eglutil if platform supports EGL
if (DEQP_SUPPORT_EGL)
	target_link_libraries(tcutil-platform eglutil eglwrapper)
endif ()

# X11 libraries
if (DEQP_USE_X11)
	find_package(X11 REQUIRED)
	target_link_libraries(tcutil-platform ${X11_LIBRARIES})
	if (DEQP_SUPPORT_GLX)
	  # GLX functions don't currently have wrappers, so link directly to libGL.
	  target_link_libraries(tcutil-platform GL)
	endif ()
endif ()
