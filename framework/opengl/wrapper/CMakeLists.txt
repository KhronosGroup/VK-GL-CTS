# GL Wrapper

set(GLWRAPPER_SRCS
	glw.h
	glwDefs.cpp
	glwDefs.hpp
	glwEnums.hpp
	glwFunctionLoader.hpp
	glwFunctions.hpp
	glwFunctions.cpp
	glwInitES20Direct.hpp
	glwInitES20Direct.cpp
	glwInitES30Direct.hpp
	glwInitES30Direct.cpp
	glwInitFunctions.hpp
	glwInitFunctions.cpp
	glwWrapper.hpp
	glwWrapper.cpp
	)

set(GLWRAPPER_LIBS
	debase
	deutil
	decpp
	${DEQP_GLES2_LIBRARIES}
	${DEQP_GLES3_LIBRARIES}
	)

add_library(glwrapper STATIC ${GLWRAPPER_SRCS})
target_link_libraries(glwrapper ${GLWRAPPER_LIBS})
