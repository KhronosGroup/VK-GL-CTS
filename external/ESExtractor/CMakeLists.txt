# cmake file for ESExtractor

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/lib/esextractor.h")
	set(DEFAULT_ESEXTRACTOR_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
else ()
	# Assume build inside Android source tree (TODO: video tests on Android)
	set(DEFAULT_ESEXTRACTOR_SRC_PATH "../esextractor")
endif ()

set(ESEXTRACTOR_SRC_PATH ${DEFAULT_ESEXTRACTOR_SRC_PATH} CACHE STRING "Path to esextractor source tree")
if (IS_ABSOLUTE ${ESEXTRACTOR_SRC_PATH})
	set(ESEXTRACTOR_ABS_PATH ${ESEXTRACTOR_SRC_PATH})
else ()
	set(ESEXTRACTOR_ABS_PATH "${CMAKE_SOURCE_DIR}/${ESEXTRACTOR_SRC_PATH}")
endif ()

if (EXISTS ${ESEXTRACTOR_ABS_PATH}/lib/esextractor.cpp)
	message(STATUS "ESExtractor found")
	set(CMAKE_C_FLAGS ${DE_3RD_PARTY_C_FLAGS})
	set(CMAKE_CXX_FLAGS ${DE_3RD_PARTY_CXX_FLAGS})

	set(ESEXTRACTOR_SRCS
		${ESEXTRACTOR_ABS_PATH}/lib/esextractor.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/esereader.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/esefilereader.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/esedatareader.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/esestream.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/eseivfstream.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/esenalstream.cpp
		${ESEXTRACTOR_ABS_PATH}/lib/esenalu.cpp
		)

	add_definitions(-DES_STATIC_COMPILATION)

	add_library(esextractor STATIC ${ESEXTRACTOR_SRCS})

	set(ESEXTRACTOR_INCLUDE_PATH	${ESEXTRACTOR_ABS_PATH}/lib	PARENT_SCOPE)
	set(ESEXTRACTOR_LIBRARY			esextractor					PARENT_SCOPE)
else ()
	message(FATAL_ERROR "esextractor not found")
endif ()
