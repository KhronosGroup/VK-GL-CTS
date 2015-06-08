# cmake file for debase

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(DEBASE_SRCS
	deDefs.c
	deDefs.h
	deFloat16.c
	deFloat16.h
	deInt32.c
	deInt32.h
	deInt32Test.c
	deMath.c
	deMath.h
	deMathTest.c
	deMemory.c
	deMemory.h
	deRandom.c
	deRandom.h
	deString.c
	deString.h
	)

add_library(debase STATIC ${DEBASE_SRCS})

# link debase to libm on unix systems
if (DE_OS_IS_UNIX)
	find_library(MATH_LIBRARY NAMES libm m PATHS /usr/lib /lib)
	if (NOT MATH_LIBRARY)
		message(FATAL_ERROR "Can't find libm.")
	endif (NOT MATH_LIBRARY)
	target_link_libraries(debase ${MATH_LIBRARY})

	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

if (DE_OS_IS_ANDROID)
	find_library(C_LIBRARY NAMES c PATHS /usr/lib /lib)
	find_library(M_LIBRARY NAMES m PATHS /usr/lib /lib)
	find_library(LOG_LIBRARY NAMES log PATHS /usr/lib)
	target_link_libraries(debase ${C_LIBRARY} ${M_LIBRARY} ${LOG_LIBRARY} gcc)
endif ()

if (DE_OS_IS_SYMBIAN)
    target_link_libraries(debase gcc)
endif()
