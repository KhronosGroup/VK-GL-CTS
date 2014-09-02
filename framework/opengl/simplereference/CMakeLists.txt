# Simple GLESx reference

set(SGLR_SRCS
	sglrContext.cpp
	sglrContext.hpp
	sglrContextUtil.hpp
	sglrContextUtil.cpp
	sglrContextWrapper.cpp
	sglrContextWrapper.hpp
	sglrReferenceContext.cpp
	sglrReferenceContext.hpp
	sglrReferenceUtils.cpp
	sglrReferenceUtils.hpp
	sglrShaderProgram.cpp
	sglrShaderProgram.hpp
	)

if (DEQP_SUPPORT_GLES2 OR DEQP_SUPPORT_GLES3 OR DEQP_SUPPORT_OPENGL)
	set(SGLR_SRCS
		${SGLR_SRCS}
		sglrGLContext.cpp
		sglrGLContext.hpp
		)
endif ()

add_library(glutil-sglr STATIC ${SGLR_SRCS})
target_link_libraries(glutil-sglr glutil tcutil referencerenderer ${DEQP_GLES2_LIBRARIES})
