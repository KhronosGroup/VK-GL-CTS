# Common OpenGL CTS Test Code

if (DEQP_SUPPORT_WGL)
	include_directories(${PROJECT_SOURCE_DIR}/framework/platform/win32)
	add_definitions(-DGLCTS_SUPPORT_WGL=1)
endif ()

add_subdirectory(subgroups)
include_directories(subgroups)

set(GLCTS_COMMON_SRCS
	glcAggressiveShaderOptimizationsTests.cpp
	glcAggressiveShaderOptimizationsTests.hpp
	glcBlendEquationAdvancedTests.cpp
	glcBlendEquationAdvancedTests.hpp
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
	glcContextFlagsTests.cpp
	glcContextFlagsTests.hpp
	glcExposedExtensionsTests.cpp
	glcExposedExtensionsTests.hpp
	glcFragDepthTests.cpp
	glcFragDepthTests.hpp
	glcInfoTests.cpp
	glcInfoTests.hpp
	glcInternalformatTests.cpp
	glcInternalformatTests.hpp
	glcMisc.cpp
	glcMisc.hpp
	glcKHRDebugTests.cpp
	glcKHRDebugTests.hpp
	glcMultipleContextsTests.cpp
	glcMultipleContextsTests.hpp
	glcNearestEdgeTests.cpp
	glcNearestEdgeTests.hpp
	glcNoErrorTests.cpp
	glcNoErrorTests.hpp
	glcRobustnessTests.cpp
	glcRobustnessTests.hpp
	glcPackedPixelsTests.cpp
	glcPackedPixelsTests.hpp
	glcPolygonOffsetClampTests.cpp
	glcPolygonOffsetClampTests.hpp
	glcPackedDepthStencilTests.cpp
	glcPackedDepthStencilTests.hpp
	glcParallelShaderCompileTests.cpp
	glcParallelShaderCompileTests.hpp
	glcPixelStorageModesTests.cpp
	glcPixelStorageModesTests.hpp
	glcRobustBufferAccessBehaviorTests.cpp
	glcRobustBufferAccessBehaviorTests.hpp
	glcSeparableProgramsTransformFeedbackTests.cpp
	glcSeparableProgramsTransformFeedbackTests.hpp
	glcShaderConstExprTests.hpp
	glcShaderConstExprTests.cpp
	glcShaderFunctionTests.cpp
	glcShaderFunctionTests.hpp
	glcShaderGroupVoteTests.cpp
	glcShaderGroupVoteTests.hpp
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
	glcShaderMacroTests.cpp
	glcShaderMacroTests.hpp
	glcShaderMultisampleInterpolationTests.cpp
	glcShaderMultisampleInterpolationTests.hpp
	glcShaderRenderCase.cpp
	glcShaderRenderCase.hpp
	glcShaderStructTests.cpp
	glcShaderStructTests.hpp
	glcSampleVariablesTests.cpp
	glcSampleVariablesTests.hpp
	glcSpirvUtils.cpp
	glcSpirvUtils.hpp
	glcTestCase.cpp
	glcTestCase.hpp
	glcTestCaseWrapper.cpp
	glcTestCaseWrapper.hpp
	glcTestPackage.cpp
	glcTestPackage.hpp
	glcTextureCompatibilityTests.cpp
	glcTextureCompatibilityTests.hpp
	glcTextureFilterAnisotropicTests.cpp
	glcTextureFilterAnisotropicTests.hpp
	glcTextureRepeatModeTests.cpp
	glcTextureRepeatModeTests.hpp
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
	glcLayoutLocationTests.cpp
	glcLayoutLocationTests.hpp
	glcLimitTest.inl
	glcLimitTest.hpp
	glcWaiver.hpp
	glcGLSLVectorConstructorTests.cpp
	glcGLSLVectorConstructorTests.hpp
	glcFramebufferCompleteness.cpp
	glcFramebufferCompleteness.hpp
	glcCompressedFormatTests_data.inl
	glcCompressedFormatTests.cpp
	glcCompressedFormatTests.hpp
	)

set(GLCTS_COMMON_LIBS
	glutil
	tcutil
	tcutil-platform
	eglutil
	deqp-gl-subgroups
	)

# Add glslang
include_directories(${GLSLANG_INCLUDE_PATH})

# \note Code interfacing with glslang needs to include third-party headers
#       that cause all sorts of warnings to appear.
if (DE_COMPILER_IS_GCC OR DE_COMPILER_IS_CLANG)
	set_source_files_properties(
		FILES glcSpirvUtils.cpp
		PROPERTIES COMPILE_FLAGS "${DE_3RD_PARTY_CXX_FLAGS}")
endif ()

set(GLCTS_COMMON_LIBS ${GLCTS_COMMON_LIBS} ${GLSLANG_LIBRARIES})

# Add spirv-tools
include_directories(${spirv-tools_SOURCE_DIR}/include)
include_directories(${spirv-tools_SOURCE_DIR}/external/include)
include_directories(${PROJECT_BINARY_DIR}/external/vulkancts/framework/vulkan)

set(GLCTS_COMMON_LIBS ${GLCTS_COMMON_LIBS} SPIRV-Tools)


PCH(GLCTS_COMMON_SRCS ../pch.cpp)

add_library(glcts-common STATIC ${GLCTS_COMMON_SRCS})
add_dependencies(glcts-common deqp-vk-inl)
target_link_libraries(glcts-common ${GLCTS_COMMON_LIBS})

set(GLCTS_COMMON_NOCONTEXTPACKAGE_SRCS
	glcSingleConfigTestPackage.cpp
	glcSingleConfigTestPackage.hpp
	glcNoDefaultContextPackage.cpp
	glcNoDefaultContextPackage.hpp
	)
add_library(glcts-common-nocontext-package STATIC ${GLCTS_COMMON_NOCONTEXTPACKAGE_SRCS})
