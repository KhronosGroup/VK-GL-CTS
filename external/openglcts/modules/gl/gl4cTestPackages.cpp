/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief OpenGL 4.x Test Packages.
 */ /*-------------------------------------------------------------------*/

#include "gl4cTestPackages.hpp"

#include "gl4cBufferStorageTests.hpp"
#include "gl4cClipControlTests.hpp"
#include "gl4cComputeShaderTests.hpp"
#include "gl4cConditionalRenderInvertedTests.hpp"
#include "gl4cContextFlushControlTests.hpp"
#include "gl4cCopyImageTests.hpp"
#include "gl4cDirectStateAccessTests.hpp"
#include "gl4cES31CompatibilityTests.hpp"
#include "gl4cEnhancedLayoutsTests.hpp"
#include "gl4cGPUShaderFP64Tests.hpp"
#include "gl4cGetTextureSubImageTests.hpp"
#include "gl4cIncompleteTextureAccessTests.hpp"
#include "gl4cKHRDebugTests.hpp"
#include "gl4cMapBufferAlignmentTests.hpp"
#include "gl4cMultiBindTests.hpp"
#include "gl4cParallelShaderCompileTests.hpp"
#include "gl4cPostDepthCoverageTests.hpp"
#include "gl4cProgramInterfaceQueryTests.hpp"
#include "gl4cRobustnessTests.hpp"
#include "gl4cShaderAtomicCountersTests.hpp"
#include "gl4cShaderImageLoadStoreTests.hpp"
#include "gl4cShaderImageSizeTests.hpp"
#include "gl4cShaderStorageBufferObjectTests.hpp"
#include "gl4cShaderSubroutineTests.hpp"
#include "gl4cShaderTextureImageSamplesTests.hpp"
#include "gl4cShadingLanguage420PackTests.hpp"
#include "gl4cSparseBufferTests.hpp"
#include "gl4cSparseTexture2Tests.hpp"
#include "gl4cSparseTextureTests.hpp"
#include "gl4cStencilTexturingTests.hpp"
#include "gl4cSyncTests.hpp"
#include "gl4cTextureBarrierTests.hpp"
#include "gl4cTextureGatherTests.hpp"
#include "gl4cTextureViewTests.hpp"
#include "gl4cVertexAttrib64BitTest.hpp"
#include "gl4cVertexAttribBindingTests.hpp"
#include "gl4cViewportArrayTests.hpp"
#include "glcInfoTests.hpp"
#include "glcRobustBufferAccessBehaviorTests.hpp"
#include "glcShaderLibrary.hpp"

#include "../gles31/es31cArrayOfArraysTests.hpp"
#include "../gles31/es31cBlendEquationAdvancedTests.hpp"
#include "../gles31/es31cDrawIndirectTests.hpp"
#include "../gles31/es31cExplicitUniformLocationTest.hpp"
#include "../gles31/es31cLayoutBindingTests.hpp"
#include "../gles31/es31cSampleShadingTests.hpp"
#include "../gles31/es31cSampleVariablesTests.hpp"
#include "../gles31/es31cSeparateShaderObjsTests.hpp"
#include "../gles31/es31cShaderBitfieldOperationTests.hpp"
#include "../gles31/es31cShaderMultisampleInterpolationTests.hpp"
#include "../glesext/geometry_shader/esextcGeometryShaderTests.hpp"
#include "../glesext/gpu_shader5/esextcGPUShader5Tests.hpp"
#include "../glesext/tessellation_shader/esextcTessellationShaderTests.hpp"
#include "../glesext/texture_border_clamp/esextcTextureBorderClampTests.hpp"
#include "../glesext/texture_buffer/esextcTextureBufferTests.hpp"
#include "../glesext/texture_cube_map_array/esextcTextureCubeMapArrayTests.hpp"

namespace gl4cts
{

// GL40TestPackage

GL40TestPackage::GL40TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
								 glu::ContextType renderContextType)
	: GL33TestPackage(testCtx, packageName, packageName, renderContextType)
{
	(void)description;
}

GL40TestPackage::~GL40TestPackage(void)
{
}

void GL40TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	GL33TestPackage::init();

	try
	{
		addChild(new gl4cts::GPUShaderFP64Tests(getContext()));
		addChild(new gl4cts::TextureGatherTests(getContext()));
		addChild(new glcts::DrawIndirectTestsGL40(getContext()));
		addChild(new gl4cts::ClipControlTests(getContext(), gl4cts::ClipControlTests::API_GL_ARB_clip_control));
		addChild(new gl4cts::ShaderSubroutineTests(getContext()));
		addChild(
			new gl4cts::TextureBarrierTests(getContext(), gl4cts::TextureBarrierTests::API_GL_ARB_texture_barrier));
	}
	catch (...)
	{
		// Destroy context.
		TestPackage::deinit();
		throw;
	}
}

// GL41TestPackage

GL41TestPackage::GL41TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
								 glu::ContextType renderContextType)
	: GL40TestPackage(testCtx, packageName, packageName, renderContextType)
{
	(void)description;
}

GL41TestPackage::~GL41TestPackage(void)
{
}

void GL41TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	GL40TestPackage::init();

	try
	{
		addChild(new gl4cts::VertexAttrib64BitTests(getContext()));
		addChild(new gl4cts::ViewportArrayTests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		TestPackage::deinit();
		throw;
	}
}

// GL42TestPackage

GL42TestPackage::GL42TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
								 glu::ContextType renderContextType)
	: GL41TestPackage(testCtx, packageName, packageName, renderContextType)
{
	(void)description;
}

GL42TestPackage::~GL42TestPackage(void)
{
}

void GL42TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	GL41TestPackage::init();

	try
	{
		addChild(new gl4cts::MapBufferAlignmentTests(getContext()));
		addChild(new gl4cts::ShaderAtomicCountersTests(getContext()));
		addChild(new gl4cts::ShaderImageLoadStoreTests(getContext()));
		addChild(new gl4cts::ShadingLanguage420PackTests(getContext()));
		addChild(new gl4cts::TextureViewTests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		TestPackage::deinit();
		throw;
	}
}

// GL43TestPackage

GL43TestPackage::GL43TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
								 glu::ContextType renderContextType)
	: GL42TestPackage(testCtx, packageName, packageName, renderContextType)
{
	(void)description;
}

GL43TestPackage::~GL43TestPackage(void)
{
}

void GL43TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	GL42TestPackage::init();

	try
	{
		addChild(new glcts::ArrayOfArraysTestGroupGL(getContext()));
		addChild(new gl4cts::CopyImageTests(getContext()));
		addChild(new glcts::DrawIndirectTestsGL43(getContext()));
		addChild(new gl4cts::KHRDebugTests(getContext()));
		addChild(new gl4cts::ProgramInterfaceQueryTests(getContext()));
		addChild(new gl4cts::ComputeShaderTests(getContext()));
		addChild(new deqp::RobustBufferAccessBehaviorTests(getContext()));
		addChild(new gl4cts::ShaderStorageBufferObjectTests(getContext()));
		addChild(new gl4cts::VertexAttribBindingTests(getContext()));
		addChild(new gl4cts::ShaderImageSizeTests(getContext()));
		addChild(new glcts::ExplicitUniformLocationGLTests(getContext()));
		addChild(new glcts::BlendEquationAdvancedTests(getContext(), glu::GLSL_VERSION_430));
		addChild(new glcts::ShaderBitfieldOperationTests(getContext(), glu::GLSL_VERSION_430));
		addChild(new gl4cts::StencilTexturingTests(getContext()));
		addChild(new gl4cts::SparseBufferTests(getContext()));
		addChild(new gl4cts::SparseTextureTests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		TestPackage::deinit();
		throw;
	}
}

// GL44TestPackage

class GL44ShaderTests : public deqp::TestCaseGroup
{
public:
	GL44ShaderTests(deqp::Context& context) : TestCaseGroup(context, "shaders44", "Shading Language Tests")
	{
	}

	void init(void)
	{
		addChild(
			new deqp::ShaderLibraryGroup(m_context, "preprocessor", "Preprocessor Tests", "gl44/preprocessor.test"));
	}
};

GL44TestPackage::GL44TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
								 glu::ContextType renderContextType)
	: GL43TestPackage(testCtx, packageName, packageName, renderContextType)
{
	(void)description;
}

GL44TestPackage::~GL44TestPackage(void)
{
}

void GL44TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	GL43TestPackage::init();

	try
	{
		addChild(new GL44ShaderTests(getContext()));
		addChild(new gl4cts::BufferStorageTests(getContext()));
		addChild(new gl4cts::EnhancedLayoutsTests(getContext()));
		addChild(new glcts::LayoutBindingTests(getContext(), glu::GLSL_VERSION_440));
		addChild(new gl4cts::MultiBindTests(getContext()));
		addChild(new glcts::SeparateShaderObjsTests(getContext(), glu::GLSL_VERSION_440));
		addChild(new glcts::SampleShadingTests(getContext(), glu::GLSL_VERSION_440));
		addChild(new glcts::SampleVariablesTests(getContext(), glu::GLSL_VERSION_440));
		addChild(new glcts::ShaderMultisampleInterpolationTests(getContext(), glu::GLSL_VERSION_440));
		addChild(new glcts::ShaderTextureImageSamplesTests(getContext()));

		glcts::ExtParameters extParams(glu::GLSL_VERSION_440, glcts::EXTENSIONTYPE_NONE);
		addChild(new glcts::GeometryShaderTests(getContext(), extParams));
		addChild(new glcts::GPUShader5Tests(getContext(), extParams));
		addChild(new glcts::TessellationShaderTests(getContext(), extParams));
		addChild(new glcts::TextureCubeMapArrayTests(getContext(), extParams));
		addChild(new glcts::TextureBorderClampTests(getContext(), extParams));
		addChild(new glcts::TextureBufferTests(getContext(), extParams));

		//addChild(new gl4cts::ContextFlushControl::Tests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		TestPackage::deinit();
		throw;
	}
}

// GL45TestPackage

GL45TestPackage::GL45TestPackage(tcu::TestContext& testCtx, const char* packageName, const char* description,
								 glu::ContextType renderContextType)
	: GL44TestPackage(testCtx, packageName, packageName, renderContextType)
{
	(void)description;
}

GL45TestPackage::~GL45TestPackage(void)
{
}

void GL45TestPackage::init(void)
{
	// Call init() in parent - this creates context.
	GL44TestPackage::init();

	try
	{
		addChild(new gl4cts::ClipControlTests(getContext(), gl4cts::ClipControlTests::API_GL_45core));
		addChild(new gl4cts::DirectStateAccess::Tests(getContext()));
		addChild(new gl4cts::GetTextureSubImage::Tests(getContext()));
		addChild(new gl4cts::RobustnessTests(getContext()));
		addChild(new gl4cts::TextureBarrierTests(getContext(), gl4cts::TextureBarrierTests::API_GL_45core));
		addChild(new gl4cts::ConditionalRenderInverted::Tests(getContext()));
		addChild(new gl4cts::Sync::Tests(getContext()));
		addChild(new gl4cts::IncompleteTextureAccess::Tests(getContext()));
		addChild(new gl4cts::ParallelShaderCompileTests(getContext()));
		addChild(new gl4cts::PostDepthCoverage(getContext()));
		addChild(new gl4cts::SparseTexture2Tests(getContext()));
	}
	catch (...)
	{
		// Destroy context.
		TestPackage::deinit();
		throw;
	}
}

} // gl4cts
