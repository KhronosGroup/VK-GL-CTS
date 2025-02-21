#GL CTS Makefile

if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	# -Wconversion generates a LOT of warnings in this code base
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion")

	# Same for -Wunused-function
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
endif ()

include_directories(
	include
	framework/egl/wrapper
	)

add_subdirectory(modules)

# Pass DEQP_MODULE_LIBRARIES and DEQP_MODULE_ENTRY_POINTS
set(DEQP_MODULE_LIBRARIES ${DEQP_MODULE_LIBRARIES} PARENT_SCOPE)
set(DEQP_MODULE_ENTRY_POINTS ${DEQP_MODULE_ENTRY_POINTS} PARENT_SCOPE)
