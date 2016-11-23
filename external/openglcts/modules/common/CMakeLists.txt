# Common OpenGL CTS Test Code

if (DEQP_SUPPORT_WGL)
	include_directories(${PROJECT_SOURCE_DIR}/framework/platform/win32)
	add_definitions(-DGLCTS_SUPPORT_WGL=1)
endif ()

set(GLCTS_COMMON_SRCS
	glcConfigList.hpp
	glcConfigList.cpp
	glcConfigListCase.cpp
	glcConfigListCase.hpp
	glcConfigListEGL.hpp
	glcConfigListEGL.cpp
	glcConfigListWGL.hpp
	glcConfigListWGL.cpp
	glcConfigPackage.cpp
	glcConfigPackage.hpp
	glcContext.cpp
	glcContext.hpp
	glcFragDepthTests.cpp
	glcFragDepthTests.hpp
	glcInfoTests.cpp
	glcInfoTests.hpp
	glcRobustBufferAccessBehaviorTests.cpp
	glcRobustBufferAccessBehaviorTests.hpp
	glcShaderIndexingTests.cpp
	glcShaderIndexingTests.hpp
	glcShaderIntegerMixTests.cpp
	glcShaderIntegerMixTests.hpp
	glcShaderLibrary.cpp
	glcShaderLibrary.hpp
	glcShaderLibraryCase.cpp
	glcShaderLibraryCase.hpp
	glcShaderLoopTests.cpp
	glcShaderLoopTests.hpp
	glcShaderNegativeTests.hpp
	glcShaderNegativeTests.cpp
	glcShaderMultisampleInterpolationTests.cpp
	glcShaderMultisampleInterpolationTests.hpp
	glcShaderRenderCase.cpp
	glcShaderRenderCase.hpp
	glcShaderStructTests.cpp
	glcShaderStructTests.hpp
	glcShaderSwitchTests.cpp
	glcShaderSwitchTests.hpp
	glcSampleVariablesTests.cpp
	glcSampleVariablesTests.hpp
	glcTestCase.cpp
	glcTestCase.hpp
	glcTestCaseWrapper.cpp
	glcTestCaseWrapper.hpp
	glcTestPackage.cpp
	glcTestPackage.hpp
	glcUniformBlockCase.cpp
	glcUniformBlockCase.hpp
	glcUniformBlockTests.cpp
	glcUniformBlockTests.hpp
	glcViewportArrayTests.cpp
	glcViewportArrayTests.hpp
	glcTestSubcase.cpp
	glcTestSubcase.hpp
	glcExtTokens.cpp
	glcExtTokens.hpp
	)

set(GLCTS_COMMON_LIBS
	glutil
	tcutil
	eglutil
	)

add_library(glcts-common STATIC ${GLCTS_COMMON_SRCS})
target_link_libraries(glcts-common ${GLCTS_COMMON_LIBS})
