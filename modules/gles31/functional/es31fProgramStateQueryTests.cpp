/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.1 Module
 * -------------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Program State Query tests.
 *//*--------------------------------------------------------------------*/

#include "es31fProgramStateQueryTests.hpp"
#include "glsStateQueryUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluObjectWrapper.hpp"
#include "gluShaderProgram.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

namespace deqp
{
namespace gles31
{
namespace Functional
{
namespace
{

using namespace gls::StateQueryUtil;

static const char* getVerifierSuffix (QueryType type)
{
	switch (type)
	{
		case QUERY_PROGRAM_INTEGER_VEC3:
		case QUERY_PROGRAM_INTEGER:
			return "get_programiv";

		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

class ProgramSeparableCase : public TestCase
{
public:
						ProgramSeparableCase	(Context& context, QueryType verifier, const char* name, const char* desc);
	IterateResult		iterate					(void);

private:
	const QueryType		m_verifier;
};

ProgramSeparableCase::ProgramSeparableCase (Context& context, QueryType verifier, const char* name, const char* desc)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
{
}

ProgramSeparableCase::IterateResult ProgramSeparableCase::iterate (void)
{
	static const char* const s_vertexSource = 	"#version 310 es\n"
												"out highp vec4 v_color;\n"
												"void main()\n"
												"{\n"
												"	gl_Position = vec4(float(gl_VertexID) * 0.5, float(gl_VertexID+1) * 0.5, 0.0, 1.0);\n"
												"	v_color = vec4(float(gl_VertexID), 1.0, 0.0, 1.0);\n"
												"}\n";
	static const char* const s_fragmentSource = "#version 310 es\n"
												"in highp vec4 v_color;\n"
												"layout(location=0) out highp vec4 o_color;\n"
												"void main()\n"
												"{\n"
												"	o_color = v_color;\n"
												"}\n";

	glu::CallLogWrapper		gl			(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result		(m_testCtx.getLog(), " // ERROR: ");
	glu::Shader				vtxShader	(m_context.getRenderContext(), glu::SHADERTYPE_VERTEX);
	glu::Shader				frgShader	(m_context.getRenderContext(), glu::SHADERTYPE_FRAGMENT);

	vtxShader.setSources(1, &s_vertexSource, DE_NULL);
	frgShader.setSources(1, &s_fragmentSource, DE_NULL);

	vtxShader.compile();
	frgShader.compile();

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "VtxShader", "Vertex shader");
		m_testCtx.getLog() << vtxShader;
	}

	{
		const tcu::ScopedLogSection section(m_testCtx.getLog(), "FrgShader", "Fragment shader");
		m_testCtx.getLog() << frgShader;
	}

	if (!vtxShader.getCompileStatus() || !frgShader.getCompileStatus())
		throw tcu::TestError("failed to build shaders");

	gl.enableLogging(true);

	{
		const tcu::ScopedLogSection	section	(m_testCtx.getLog(), "Initial", "Initial");
		glu::Program				program	(m_context.getRenderContext());

		verifyStateProgramInteger(result, gl, program.getProgram(), GL_PROGRAM_SEPARABLE, 0, m_verifier);
	}

	{
		const tcu::ScopedLogSection section		(m_testCtx.getLog(), "SetFalse", "SetFalse");
		glu::Program				program		(m_context.getRenderContext());
		int							linkStatus	= 0;

		gl.glAttachShader(program.getProgram(), vtxShader.getShader());
		gl.glAttachShader(program.getProgram(), frgShader.getShader());
		gl.glProgramParameteri(program.getProgram(), GL_PROGRAM_SEPARABLE, GL_FALSE);
		gl.glLinkProgram(program.getProgram());
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "setup program");

		gl.glGetProgramiv(program.getProgram(), GL_LINK_STATUS, &linkStatus);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "query link status");

		if (linkStatus == GL_FALSE)
			throw tcu::TestError("failed to link program");

		verifyStateProgramInteger(result, gl, program.getProgram(), GL_PROGRAM_SEPARABLE, 0, m_verifier);
	}

	{
		const tcu::ScopedLogSection section		(m_testCtx.getLog(), "SetTrue", "SetTrue");
		glu::Program				program		(m_context.getRenderContext());
		int							linkStatus	= 0;

		gl.glAttachShader(program.getProgram(), vtxShader.getShader());
		gl.glAttachShader(program.getProgram(), frgShader.getShader());
		gl.glProgramParameteri(program.getProgram(), GL_PROGRAM_SEPARABLE, GL_TRUE);
		gl.glLinkProgram(program.getProgram());
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "setup program");

		gl.glGetProgramiv(program.getProgram(), GL_LINK_STATUS, &linkStatus);
		GLU_EXPECT_NO_ERROR(gl.glGetError(), "query link status");

		if (linkStatus == GL_FALSE)
			throw tcu::TestError("failed to link program");

		verifyStateProgramInteger(result, gl, program.getProgram(), GL_PROGRAM_SEPARABLE, GL_TRUE, m_verifier);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ComputeWorkGroupSizeCase : public TestCase
{
public:
						ComputeWorkGroupSizeCase	(Context& context, QueryType verifier, const char* name, const char* desc);
	IterateResult		iterate						(void);

private:
	const QueryType		m_verifier;
};

ComputeWorkGroupSizeCase::ComputeWorkGroupSizeCase (Context& context, QueryType verifier, const char* name, const char* desc)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
{
}

ComputeWorkGroupSizeCase::IterateResult ComputeWorkGroupSizeCase::iterate (void)
{
	static const char* const s_computeSource1D =	"#version 310 es\n"
													"layout (local_size_x = 3) in;\n"
													"layout(binding = 0) buffer Output\n"
													"{\n"
													"	highp float val;\n"
													"} sb_out;\n"
													"\n"
													"void main (void)\n"
													"{\n"
													"	sb_out.val = 1.0;\n"
													"}\n";
	static const char* const s_computeSource2D =	"#version 310 es\n"
													"layout (local_size_x = 3, local_size_y = 2) in;\n"
													"layout(binding = 0) buffer Output\n"
													"{\n"
													"	highp float val;\n"
													"} sb_out;\n"
													"\n"
													"void main (void)\n"
													"{\n"
													"	sb_out.val = 1.0;\n"
													"}\n";
	static const char* const s_computeSource3D =	"#version 310 es\n"
													"layout (local_size_x = 3, local_size_y = 2, local_size_z = 4) in;\n"
													"layout(binding = 0) buffer Output\n"
													"{\n"
													"	highp float val;\n"
													"} sb_out;\n"
													"\n"
													"void main (void)\n"
													"{\n"
													"	sb_out.val = 1.0;\n"
													"}\n";

	glu::CallLogWrapper		gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result	(m_testCtx.getLog(), " // ERROR: ");

	gl.enableLogging(true);

	{
		const tcu::ScopedLogSection section		(m_testCtx.getLog(), "OneDimensional", "1D");
		glu::ShaderProgram			program		(m_context.getRenderContext(), glu::ProgramSources() << glu::ComputeSource(s_computeSource1D));

		m_testCtx.getLog() << program;
		if (!program.isOk())
			throw tcu::TestError("failed to build program");

		verifyStateProgramIntegerVec3(result, gl, program.getProgram(), GL_COMPUTE_WORK_GROUP_SIZE, tcu::IVec3(3, 1, 1), m_verifier);
	}

	{
		const tcu::ScopedLogSection section		(m_testCtx.getLog(), "TwoDimensional", "2D");
		glu::ShaderProgram			program		(m_context.getRenderContext(), glu::ProgramSources() << glu::ComputeSource(s_computeSource2D));

		m_testCtx.getLog() << program;
		if (!program.isOk())
			throw tcu::TestError("failed to build program");

		verifyStateProgramIntegerVec3(result, gl, program.getProgram(), GL_COMPUTE_WORK_GROUP_SIZE, tcu::IVec3(3, 2, 1), m_verifier);
	}

	{
		const tcu::ScopedLogSection section		(m_testCtx.getLog(), "TreeDimensional", "3D");
		glu::ShaderProgram			program		(m_context.getRenderContext(), glu::ProgramSources() << glu::ComputeSource(s_computeSource3D));

		m_testCtx.getLog() << program;
		if (!program.isOk())
			throw tcu::TestError("failed to build program");

		verifyStateProgramIntegerVec3(result, gl, program.getProgram(), GL_COMPUTE_WORK_GROUP_SIZE, tcu::IVec3(3, 2, 4), m_verifier);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

class ActiveAtomicCounterBuffersCase : public TestCase
{
public:
						ActiveAtomicCounterBuffersCase	(Context& context, QueryType verifier, const char* name, const char* desc);
	IterateResult		iterate							(void);

private:
	const QueryType		m_verifier;
};

ActiveAtomicCounterBuffersCase::ActiveAtomicCounterBuffersCase (Context& context, QueryType verifier, const char* name, const char* desc)
	: TestCase		(context, name, desc)
	, m_verifier	(verifier)
{
}

ActiveAtomicCounterBuffersCase::IterateResult ActiveAtomicCounterBuffersCase::iterate (void)
{
	static const char* const s_computeSource0 =	"#version 310 es\n"
												"layout (local_size_x = 3) in;\n"
												"layout(binding = 0) buffer Output\n"
												"{\n"
												"	highp float val;\n"
												"} sb_out;\n"
												"\n"
												"void main (void)\n"
												"{\n"
												"	sb_out.val = 1.0;\n"
												"}\n";
	static const char* const s_computeSource1 =	"#version 310 es\n"
												"layout (local_size_x = 3) in;\n"
												"layout(binding = 0) uniform highp atomic_uint u_counters[2];\n"
												"layout(binding = 0) buffer Output\n"
												"{\n"
												"	highp float val;\n"
												"} sb_out;\n"
												"\n"
												"void main (void)\n"
												"{\n"
												"	sb_out.val = float(atomicCounterIncrement(u_counters[0])) + float(atomicCounterIncrement(u_counters[1]));\n"
												"}\n";

	glu::CallLogWrapper		gl		(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());
	tcu::ResultCollector	result	(m_testCtx.getLog(), " // ERROR: ");

	gl.enableLogging(true);

	{
		const tcu::ScopedLogSection	section		(m_testCtx.getLog(), "Initial", "Initial");
		glu::Program				program		(m_context.getRenderContext());

		verifyStateProgramInteger(result, gl, program.getProgram(), GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, 0, m_verifier);
	}

	{
		const tcu::ScopedLogSection	section		(m_testCtx.getLog(), "NoBuffers", "No buffers");
		glu::ShaderProgram			program		(m_context.getRenderContext(), glu::ProgramSources() << glu::ComputeSource(s_computeSource0));

		m_testCtx.getLog() << program;
		if (!program.isOk())
			throw tcu::TestError("failed to build program");

		verifyStateProgramInteger(result, gl, program.getProgram(), GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, 0, m_verifier);
	}

	{
		const tcu::ScopedLogSection	section		(m_testCtx.getLog(), "OneBuffer", "One buffer");
		glu::ShaderProgram			program		(m_context.getRenderContext(), glu::ProgramSources() << glu::ComputeSource(s_computeSource1));

		m_testCtx.getLog() << program;
		if (!program.isOk())
			throw tcu::TestError("failed to build program");

		verifyStateProgramInteger(result, gl, program.getProgram(), GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, 1, m_verifier);
	}

	result.setTestContextResult(m_testCtx);
	return STOP;
}

} // anonymous

ProgramStateQueryTests::ProgramStateQueryTests (Context& context)
	: TestCaseGroup(context, "program", "Program State Query tests")
{
}

ProgramStateQueryTests::~ProgramStateQueryTests (void)
{
}

void ProgramStateQueryTests::init (void)
{
	static const QueryType intVerifiers[] =
	{
		QUERY_PROGRAM_INTEGER,
	};
	static const QueryType intVec3Verifiers[] =
	{
		QUERY_PROGRAM_INTEGER_VEC3,
	};

#define FOR_EACH_INT_VERIFIER(X) \
	for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(intVerifiers); ++verifierNdx)	\
	{																							\
		const char* verifierSuffix = getVerifierSuffix(intVerifiers[verifierNdx]);				\
		const QueryType verifier = intVerifiers[verifierNdx];									\
		this->addChild(X);																		\
	}

#define FOR_EACH_VEC_VERIFIER(X) \
	for (int verifierNdx = 0; verifierNdx < DE_LENGTH_OF_ARRAY(intVec3Verifiers); ++verifierNdx)	\
	{																								\
		const char* verifierSuffix = getVerifierSuffix(intVec3Verifiers[verifierNdx]);				\
		const QueryType verifier = intVec3Verifiers[verifierNdx];									\
		this->addChild(X);																			\
	}

	FOR_EACH_INT_VERIFIER(new ProgramSeparableCase				(m_context, verifier, (std::string("program_separable_") + verifierSuffix).c_str(),				"Test PROGRAM_SEPARABLE"));
	FOR_EACH_VEC_VERIFIER(new ComputeWorkGroupSizeCase			(m_context, verifier, (std::string("compute_work_group_size_") + verifierSuffix).c_str(),		"Test COMPUTE_WORK_GROUP_SIZE"));
	FOR_EACH_INT_VERIFIER(new ActiveAtomicCounterBuffersCase	(m_context, verifier, (std::string("active_atomic_counter_buffers_") + verifierSuffix).c_str(),	"Test ACTIVE_ATOMIC_COUNTER_BUFFERS"));

#undef FOR_EACH_INT_VERIFIER
#undef FOR_EACH_VEC_VERIFIER
}

} // Functional
} // gles31
} // deqp
