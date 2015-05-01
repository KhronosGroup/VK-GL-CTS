/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Functional Tests.
 *//*--------------------------------------------------------------------*/

#include "es31fFunctionalTests.hpp"

#include "glsShaderLibrary.hpp"
#include "es31fBasicComputeShaderTests.hpp"
#include "es31fComputeShaderBuiltinVarTests.hpp"
#include "es31fDrawTests.hpp"
#include "es31fShaderSharedVarTests.hpp"
#include "es31fAtomicCounterTests.hpp"
#include "es31fShaderAtomicOpTests.hpp"
#include "es31fShaderImageLoadStoreTests.hpp"
#include "es31fTessellationTests.hpp"
#include "es31fSSBOLayoutTests.hpp"
#include "es31fSSBOArrayLengthTests.hpp"
#include "es31fShaderCommonFunctionTests.hpp"
#include "es31fShaderPackingFunctionTests.hpp"
#include "es31fShaderIntegerFunctionTests.hpp"
#include "es31fStencilTexturingTests.hpp"
#include "es31fShaderTextureSizeTests.hpp"
#include "es31fShaderStateQueryTests.hpp"
#include "es31fLayoutBindingTests.hpp"
#include "es31fTextureLevelStateQueryTests.hpp"
#include "es31fIntegerStateQueryTests.hpp"
#include "es31fInternalFormatQueryTests.hpp"
#include "es31fBooleanStateQueryTests.hpp"
#include "es31fIndexedStateQueryTests.hpp"
#include "es31fTextureStateQueryTests.hpp"
#include "es31fFramebufferDefaultStateQueryTests.hpp"
#include "es31fProgramPipelineStateQueryTests.hpp"
#include "es31fProgramStateQueryTests.hpp"
#include "es31fSamplerStateQueryTests.hpp"
#include "es31fTextureFilteringTests.hpp"
#include "es31fTextureFormatTests.hpp"
#include "es31fTextureSpecificationTests.hpp"
#include "es31fTextureMultisampleTests.hpp"
#include "es31fMultisampleTests.hpp"
#include "es31fSynchronizationTests.hpp"
#include "es31fGeometryShaderTests.hpp"
#include "es31fSampleShadingTests.hpp"
#include "es31fSampleVariableTests.hpp"
#include "es31fIndirectComputeDispatchTests.hpp"
#include "es31fVertexAttributeBindingTests.hpp"
#include "es31fVertexAttributeBindingStateQueryTests.hpp"
#include "es31fShaderMultisampleInterpolationTests.hpp"
#include "es31fShaderMultisampleInterpolationStateQueryTests.hpp"
#include "es31fProgramUniformTests.hpp"
#include "es31fOpaqueTypeIndexingTests.hpp"
#include "es31fAdvancedBlendTests.hpp"
#include "es31fSeparateShaderTests.hpp"
#include "es31fUniformLocationTests.hpp"
#include "es31fBuiltinPrecisionTests.hpp"
#include "es31fTessellationGeometryInteractionTests.hpp"
#include "es31fUniformBlockTests.hpp"
#include "es31fDebugTests.hpp"
#include "es31fFboColorbufferTests.hpp"
#include "es31fFboNoAttachmentTests.hpp"
#include "es31fProgramInterfaceQueryTests.hpp"
#include "es31fTextureGatherTests.hpp"
#include "es31fTextureFormatTests.hpp"
#include "es31fTextureBufferTests.hpp"
#include "es31fTextureBorderClampTests.hpp"
#include "es31fShaderBuiltinConstantTests.hpp"
#include "es31fShaderHelperInvocationTests.hpp"
#include "es31fPrimitiveBoundingBoxTests.hpp"
#include "es31fAndroidExtensionPackES31ATests.hpp"
#include "es31fCopyImageTests.hpp"
#include "es31fDrawBuffersIndexedTests.hpp"
#include "es31fDefaultVertexArrayObjectTests.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{

class ShaderLibraryTest : public TestCaseGroup
{
public:
	ShaderLibraryTest (Context& context, const char* name, const char* description)
		: TestCaseGroup	(context, name, description)
		, m_filename	(name + std::string(".test"))
	{
	}

	ShaderLibraryTest (Context& context, const char* filename, const char* name, const char* description)
		: TestCaseGroup	(context, name, description)
		, m_filename	(filename)
	{
	}

	void init (void)
	{
		gls::ShaderLibrary			shaderLibrary(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo());
		std::string					fileName	= "shaders/" + m_filename;
		std::vector<tcu::TestNode*>	children	= shaderLibrary.loadShaderFile(fileName.c_str());

		for (int i = 0; i < (int)children.size(); i++)
			addChild(children[i]);
	}

private:
	const std::string m_filename;
};

class ShaderBuiltinVarTests : public TestCaseGroup
{
public:
	ShaderBuiltinVarTests (Context& context)
		: TestCaseGroup(context, "builtin_var", "Shader Built-in Variable Tests")
	{
	}

	void init (void)
	{
		addChild(new ComputeShaderBuiltinVarTests(m_context));
	}
};

class ShaderBuiltinFunctionTests : public TestCaseGroup
{
public:
	ShaderBuiltinFunctionTests (Context& context)
		: TestCaseGroup(context, "builtin_functions", "Built-in Function Tests")
	{
	}

	void init (void)
	{
		addChild(new ShaderCommonFunctionTests	(m_context));
		addChild(new ShaderPackingFunctionTests	(m_context));
		addChild(new ShaderIntegerFunctionTests	(m_context));
		addChild(new ShaderTextureSizeTests		(m_context));
		addChild(createBuiltinPrecisionTests	(m_context));
	}
};

class ShaderLinkageTests : public TestCaseGroup
{
public:
	ShaderLinkageTests (Context& context)
		: TestCaseGroup(context,  "linkage", "Linkage Tests")
	{
	}

	void init (void)
	{
		addChild(new ShaderLibraryTest(m_context, "linkage_geometry.test", "geometry", "Geometry shader"));
		addChild(new ShaderLibraryTest(m_context, "linkage_tessellation.test", "tessellation", "Tessellation shader"));
		addChild(new ShaderLibraryTest(m_context, "linkage_tessellation_geometry.test", "tessellation_geometry", "Tessellation and geometry shader"));
		addChild(new ShaderLibraryTest(m_context, "linkage_shader_storage_block.test", "shader_storage_block", "Shader storage blocks"));
		addChild(new ShaderLibraryTest(m_context, "linkage_io_block.test", "io_block", "Shader io blocks"));
	}
};

class ShaderTests : public TestCaseGroup
{
public:
	ShaderTests (Context& context)
		: TestCaseGroup(context, "shaders", "Shading Language Tests")
	{
	}

	void init (void)
	{
		addChild(new ShaderBuiltinVarTests				(m_context));
		addChild(new ShaderBuiltinFunctionTests			(m_context));
		addChild(new SampleVariableTests				(m_context));
		addChild(new ShaderMultisampleInterpolationTests(m_context));
		addChild(new OpaqueTypeIndexingTests			(m_context));
		addChild(new ShaderLibraryTest					(m_context, "functions", "Function Tests"));
		addChild(new ShaderLibraryTest					(m_context, "arrays", "Arrays Tests"));
		addChild(new ShaderLibraryTest					(m_context, "arrays_of_arrays", "Arrays of Arrays Tests"));
		addChild(new ShaderLinkageTests					(m_context));
		addChild(new ShaderBuiltinConstantTests			(m_context));
		addChild(new ShaderHelperInvocationTests		(m_context));
		addChild(new ShaderLibraryTest					(m_context, "implicit_conversions", "GL_EXT_shader_implicit_conversions Tests"));
		addChild(new ShaderLibraryTest					(m_context, "uniform_block", "Uniform block tests"));
	}
};

class ComputeTests : public TestCaseGroup
{
public:
	ComputeTests (Context& context)
		: TestCaseGroup(context, "compute", "Compute Shader Tests")
	{
	}

	void init (void)
	{
		addChild(new BasicComputeShaderTests		(m_context));
		addChild(new ShaderSharedVarTests			(m_context));
		addChild(new IndirectComputeDispatchTests	(m_context));
	}
};

class SSBOTests : public TestCaseGroup
{
public:
	SSBOTests (Context& context)
		: TestCaseGroup(context, "ssbo", "Shader Storage Buffer Object Tests")
	{
	}

	void init (void)
	{
		addChild(new SSBOLayoutTests			(m_context));
		addChild(new ShaderAtomicOpTests		(m_context, "atomic", ATOMIC_OPERAND_BUFFER_VARIABLE));
		addChild(new SSBOArrayLengthTests		(m_context));
	}
};

class TextureTests : public TestCaseGroup
{
public:
	TextureTests (Context& context)
		: TestCaseGroup(context, "texture", "Texture tests")
	{
	}

	void init (void)
	{
		addChild(new TextureFilteringTests		(m_context));
		addChild(new TextureFormatTests			(m_context));
		addChild(new TextureSpecificationTests	(m_context));
		addChild(new TextureMultisampleTests	(m_context));
		addChild(new TextureGatherTests			(m_context));
		addChild(createTextureBufferTests		(m_context));
		addChild(new TextureBorderClampTests	(m_context));
	}
};

class StateQueryTests : public TestCaseGroup
{
public:
	StateQueryTests (Context& context)
		: TestCaseGroup(context, "state_query", "State query tests")
	{
	}

	void init (void)
	{
		addChild(new BooleanStateQueryTests							(m_context));
		addChild(new IntegerStateQueryTests							(m_context));
		addChild(new IndexedStateQueryTests							(m_context));
		addChild(new TextureStateQueryTests							(m_context));
		addChild(new TextureLevelStateQueryTests					(m_context));
		addChild(new SamplerStateQueryTests							(m_context));
		addChild(new ShaderStateQueryTests							(m_context));
		addChild(new InternalFormatQueryTests						(m_context));
		addChild(new VertexAttributeBindingStateQueryTests			(m_context));
		addChild(new ShaderMultisampleInterpolationStateQueryTests	(m_context));
		addChild(new FramebufferDefaultStateQueryTests				(m_context));
		addChild(new ProgramStateQueryTests							(m_context));
		addChild(new ProgramPipelineStateQueryTests					(m_context));
	}
};

class FboTests : public TestCaseGroup
{
public:
	FboTests (Context& context)
		: TestCaseGroup(context, "fbo", "Framebuffer Object Tests")
	{
	}

	void init (void)
	{
		addChild(new FboColorTests						(m_context));
		addChild(createFboNoAttachmentTests				(m_context));
		addChild(createFboNoAttachmentCompletenessTests	(m_context));
	}
};

FunctionalTests::FunctionalTests (Context& context)
	: TestCaseGroup(context, "functional", "Functionality Tests")
{
}

FunctionalTests::~FunctionalTests (void)
{
}

void FunctionalTests::init (void)
{
	addChild(new ShaderTests							(m_context));
	addChild(new ComputeTests							(m_context));
	addChild(new DrawTests								(m_context));
	addChild(new TessellationTests						(m_context));
	addChild(new SSBOTests								(m_context));
	addChild(new UniformBlockTests						(m_context));
	addChild(new ShaderImageLoadStoreTests				(m_context));
	addChild(new AtomicCounterTests						(m_context));
	addChild(new StencilTexturingTests					(m_context));
	addChild(new TextureTests							(m_context));
	addChild(new StateQueryTests						(m_context));
	addChild(new MultisampleTests						(m_context));
	addChild(new SynchronizationTests					(m_context));
	addChild(new GeometryShaderTests					(m_context));
	addChild(new SampleShadingTests						(m_context));
	addChild(new VertexAttributeBindingTests			(m_context));
	addChild(new ProgramUniformTests					(m_context));
	addChild(new AdvancedBlendTests						(m_context));
	addChild(createSeparateShaderTests					(m_context));
	addChild(new UniformLocationTests					(m_context));
	addChild(new TessellationGeometryInteractionTests	(m_context));
	addChild(new DebugTests								(m_context));
	addChild(new FboTests								(m_context));
	addChild(new ProgramInterfaceQueryTests				(m_context));
	addChild(new LayoutBindingTests						(m_context));
	addChild(new PrimitiveBoundingBoxTests				(m_context));
	addChild(new AndroidExtensionPackES31ATests			(m_context));
	addChild(createCopyImageTests						(m_context));
	addChild(createDrawBuffersIndexedTests				(m_context));
	addChild(new DefaultVertexArrayObjectTests			(m_context));
}

} // Functional
} // gles31
} // deqp
