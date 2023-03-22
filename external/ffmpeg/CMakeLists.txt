# cmake file for ffmpeg

if (NOT DE_DEFS)
	message(FATAL_ERROR "Include Defs.cmake")
endif ()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/include/libavformat/avformat.h")
	set(DEFAULT_FFMPEG_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
else ()
	# Assume build inside Android source tree
	set(DEFAULT_FFMPEG_SRC_PATH "../ffmpeg")
endif ()

set(FFMPEG_SRC_PATH ${DEFAULT_FFMPEG_SRC_PATH} CACHE STRING "Path to ffmpeg source tree")

if (IS_ABSOLUTE ${FFMPEG_SRC_PATH})
	set(FFMPEG_ABS_PATH ${FFMPEG_SRC_PATH})
else ()
	set(FFMPEG_ABS_PATH "${CMAKE_SOURCE_DIR}/${FFMPEG_SRC_PATH}")
endif ()

#set(FFMPEG_SRCS
#	${FFMPEG_ABS_PATH}/include/libavformat/avformat.h
#	${FFMPEG_ABS_PATH}/include/libavformat/avio.h
#	${FFMPEG_ABS_PATH}/include/libavcodec/avcodec.h
#	)

#set(FFMPEG_INCLUDE_PATH	${FFMPEG_ABS_PATH}/include	PARENT_SCOPE)

if (DE_OS_IS_WIN32)
	file(GLOB FFMPEG_LIBRARIES_FILES_FULL ${FFMPEG_ABS_PATH}/lib/*.lib)
elseif (DE_OS_IS_UNIX)
	file(GLOB FFMPEG_LIBRARIES_FILES_FULL ${FFMPEG_ABS_PATH}/lib/*)
else ()
	message( FATAL_ERROR "System is not supported" )
endif ()

#set(FFMPEG_LIBRARIES_FILES_SHORT )
#foreach(FFMPEG_LIBRARY_FILE_FULL ${FFMPEG_LIBRARIES_FILES_FULL})
#	get_filename_component(FFMPEG_LIBRARY_FILE "${FFMPEG_LIBRARY_FILE_FULL}" NAME_WE)
#	set(FFMPEG_LIBRARIES_FILES_SHORT ${FFMPEG_LIBRARY_FILE} ${FFMPEG_LIBRARIES_FILES_SHORT})
#endforeach()

#message(WARNING ${FFMPEG_LIBRARIES_FILES_SHORT})

set(FFMPEG_LIBRARIES		${FFMPEG_LIBRARIES_FILES_FULL}	PARENT_SCOPE)
set(FFMPEG_LIBRARIES_PATH	${FFMPEG_ABS_PATH}/lib			PARENT_SCOPE)

#message(WARNING ${FFMPEG_LIBRARIES_FILES_FULL})

#
#file(GLOB ConfigFiles ${FFMPEG_ABS_PATH}/lib/*.lib
#                      ${FFMPEG_ABS_PATH}/bin/*.dll)
#
#add_custom_target(ffmpeg)
#foreach(ConfigFile ${ConfigFiles})
#    add_custom_command(TARGET ffmpeg PRE_BUILD
#                       COMMAND ${CMAKE_COMMAND} -E
#                               copy ${ConfigFile} ${CMAKE_CURRENT_BINARY_DIR}/.)
#endforeach()

if (DE_OS_IS_UNIX)
	add_custom_target(ffmpeg ALL)
	foreach(FFMPEG_LIBRARIES_FILE ${FFMPEG_LIBRARIES_FILES_FULL})
		#message(WARNING ${FFMPEG_LIBRARIES_FILE})
		#add_custom_command(TARGET ffmpeg PRE_BUILD COMMAND ${CMAKE_COMMAND} -E	copy ${FFMPEG_LIBRARIES_FILE} ${CMAKE_BINARY_DIR})
		add_custom_command(TARGET ffmpeg PRE_BUILD COMMAND ${CMAKE_COMMAND} -E	copy ${FFMPEG_LIBRARIES_FILE} ${CMAKE_BINARY_DIR}/external/ffmpeg)
	endforeach()
	#message(WARNING ${CMAKE_BINARY_DIR})
endif ()
