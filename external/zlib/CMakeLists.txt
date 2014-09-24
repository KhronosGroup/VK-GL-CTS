# cmake file for zlib

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

set(ZLIB_SRC_PATH "../../../zlib/src" CACHE STRING "Path to zlib source tree")

set(ZLIB_SRCS
	${ZLIB_SRC_PATH}/adler32.c
	${ZLIB_SRC_PATH}/compress.c
	${ZLIB_SRC_PATH}/crc32.c
	${ZLIB_SRC_PATH}/crc32.h
	${ZLIB_SRC_PATH}/deflate.c
	${ZLIB_SRC_PATH}/deflate.h
	${ZLIB_SRC_PATH}/gzclose.c
	${ZLIB_SRC_PATH}/gzguts.h
	${ZLIB_SRC_PATH}/gzlib.c
	${ZLIB_SRC_PATH}/gzread.c
	${ZLIB_SRC_PATH}/gzwrite.c
	${ZLIB_SRC_PATH}/infback.c
	${ZLIB_SRC_PATH}/inffast.c
	${ZLIB_SRC_PATH}/inffast.h
	${ZLIB_SRC_PATH}/inffixed.h
	${ZLIB_SRC_PATH}/inflate.c
	${ZLIB_SRC_PATH}/inflate.h
	${ZLIB_SRC_PATH}/inftrees.c
	${ZLIB_SRC_PATH}/inftrees.h
	${ZLIB_SRC_PATH}/trees.c
	${ZLIB_SRC_PATH}/trees.h
	${ZLIB_SRC_PATH}/uncompr.c
	${ZLIB_SRC_PATH}/zconf.h
	${ZLIB_SRC_PATH}/zlib.h
	${ZLIB_SRC_PATH}/zutil.c
	${ZLIB_SRC_PATH}/zutil.h
	)

if (DE_COMPILER_IS_MSC)
	# 4127: conditional expression is constant
	# 4131: 'x': uses old style declarator
	# 4242, 4244: conversion from 'x' to 'y', possible loss of data
	# 4996: deprecated POSIX name
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall /wd4127 /wd4131 /wd4242 /wd4244 /wd4996")
endif ()

if (DE_OS_IS_UNIX)
	add_definitions(-D_XOPEN_SOURCE=600)
endif ()

add_library(z STATIC ${ZLIB_SRCS})

if (IS_ABSOLUTE ${ZLIB_SRC_PATH})
	set(ZLIB_INCLUDE_PATH ${ZLIB_SRC_PATH} PARENT_SCOPE)
else ()
	set(ZLIB_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${ZLIB_SRC_PATH}" PARENT_SCOPE)
endif ()

set(ZLIB_LIBRARY z PARENT_SCOPE)
