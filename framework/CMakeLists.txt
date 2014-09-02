# Test framework cmake file

# C integration library
add_subdirectory(qphelper)

# Common test util (tcutil).
add_subdirectory(common)

# Common OpenGL utilities
add_subdirectory(opengl)

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/randomshaders)
	add_subdirectory(randomshaders)
endif ()

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/referencerenderer)
	add_subdirectory(referencerenderer)
endif ()

if (DEQP_SUPPORT_EGL)
	add_subdirectory(egl)
endif ()

# Platform library
add_subdirectory(platform)
