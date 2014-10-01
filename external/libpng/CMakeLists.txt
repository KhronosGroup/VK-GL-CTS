# cmake file for libpng

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(PNG_SRC_PATH "../../../libpng" CACHE STRING "Path to libpng source tree")

set(PNG_SRCS
	${PNG_SRC_PATH}/png.h
	${PNG_SRC_PATH}/pngconf.h
	${PNG_SRC_PATH}/png.c
	${PNG_SRC_PATH}/pngset.c
	${PNG_SRC_PATH}/pngget.c
	${PNG_SRC_PATH}/pngrutil.c
	${PNG_SRC_PATH}/pngtrans.c
	${PNG_SRC_PATH}/pngwutil.c
	${PNG_SRC_PATH}/pngread.c
	${PNG_SRC_PATH}/pngrio.c
	${PNG_SRC_PATH}/pngwio.c
	${PNG_SRC_PATH}/pngwrite.c
	${PNG_SRC_PATH}/pngrtran.c
	${PNG_SRC_PATH}/pngwtran.c
	${PNG_SRC_PATH}/pngmem.c
	${PNG_SRC_PATH}/pngerror.c
	${PNG_SRC_PATH}/pngpread.c
	)

if (DE_CPU_IS_ARM_64)
	# armv8-a has always neon
	set(PNG_SRCS
		${PNG_SRCS}
		${PNG_SRC_PATH}/arm/arm_init.c
		${PNG_SRC_PATH}/arm/filter_neon_intrinsics.c
		${PNG_SRC_PATH}/arm/filter_neon.S
		)
endif ()

if (DE_DEBUG EQUAL 1)
	add_definitions(-DPNG_DEBUG)
endif ()

if (DE_OS_IS_UNIX)
	# for snprintf()
	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

set(CMAKE_C_FLAGS ${DE_3RD_PARTY_C_FLAGS})

add_library(png STATIC ${PNG_SRCS})
target_link_libraries(png ${ZLIB_LIBRARY})

if (IS_ABSOLUTE ${PNG_SRC_PATH})
	set(PNG_INCLUDE_PATH ${PNG_SRC_PATH} PARENT_SCOPE)
else ()
	set(PNG_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${PNG_SRC_PATH}" PARENT_SCOPE)
endif ()

set(PNG_LIBRARY png PARENT_SCOPE)
