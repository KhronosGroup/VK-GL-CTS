/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2015-2016 The Khronos Group Inc.
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
 * \brief
 *//*--------------------------------------------------------------------*/

/*!
 * \file glcShaderNegativeTests.cpp
 * \brief Negative tests for shaders and interface matching.
 */ /*--------------------------------------------------------------------*/

#include "glcShaderNegativeTests.hpp"
#include "deString.h"
#include "deStringUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluShaderProgram.hpp"
#include "glw.h"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

namespace deqp
{

using tcu::TestLog;

class ShaderUniformInitializeGlobalCase : public TestCase
{
public:
	ShaderUniformInitializeGlobalCase(Context& context, const char* name, const char* description,
									  glu::GLSLVersion glslVersion)
		: TestCase(context, name, description), m_glslVersion(glslVersion)
	{
	}

	~ShaderUniformInitializeGlobalCase()
	{
		// empty
	}

	IterateResult iterate()
	{
		qpTestResult result = QP_TEST_RESULT_PASS;

		static const char vertex_source_template[] =
			"${VERSION_DECL}\n"
			"precision mediump float;\n"
			"uniform vec4 nonconstantexpression;\n"
			"vec4 globalconstant0 = vec4(1.0, 1.0, 1.0, 1.0);\n"
			"vec4 globalconstant1 = nonconstantexpression;\n"
			"\n"
			"void main(void) { gl_Position = globalconstant0+globalconstant1; }\n";
		static const char fragment_source_template[] = "${VERSION_DECL}\n"
													   "precision mediump float;\n"
													   "uniform vec4 nonconstantexpression;\n"
													   "vec4 globalconstant0 = vec4(1.0, 1.0, 1.0, 1.0);\n"
													   "vec4 globalconstant1 = nonconstantexpression;\n"
													   "\n"
													   "void main(void) { }\n";

		std::map<std::string, std::string> args;
		args["VERSION_DECL"] = glu::getGLSLVersionDeclaration(m_glslVersion);

		std::string vertex_code   = tcu::StringTemplate(vertex_source_template).specialize(args);
		std::string fragment_code = tcu::StringTemplate(fragment_source_template).specialize(args);

		// Setup program.
		glu::ShaderProgram program(m_context.getRenderContext(),
								   glu::makeVtxFragSources(vertex_code.c_str(), fragment_code.c_str()));

		// GLSL ES does not allow initialization of global variables with non-constant
		// expressions, but GLSL does.
		// Check that either compilation or linking fails for ES, and that everything
		// succeeds for GL.
		bool vertexOk   = program.getShaderInfo(glu::SHADERTYPE_VERTEX).compileOk;
		bool fragmentOk = program.getShaderInfo(glu::SHADERTYPE_FRAGMENT).compileOk;
		bool linkOk		= program.getProgramInfo().linkOk;

		if (glu::glslVersionIsES(m_glslVersion))
		{
			if (vertexOk && fragmentOk && linkOk)
				result = QP_TEST_RESULT_FAIL;
		}
		else
		{
			if (!vertexOk && !fragmentOk && !linkOk)
				result = QP_TEST_RESULT_FAIL;
		}

		m_testCtx.setTestResult(result, qpGetTestResultName(result));

		return STOP;
	}

protected:
	glu::GLSLVersion m_glslVersion;
};

class ShaderUniformPrecisionLinkCase : public TestCase
{
public:
	ShaderUniformPrecisionLinkCase(Context& context, const char* name, const char* description,
								   glu::GLSLVersion glslVersion)
		: TestCase(context, name, description), m_glslVersion(glslVersion)
	{
	}

	~ShaderUniformPrecisionLinkCase()
	{
		// empty
	}

	IterateResult iterate()
	{
		TestLog&	 log	= m_testCtx.getLog();
		qpTestResult result = QP_TEST_RESULT_PASS;

		static const char vertex_source_template[] = "${VERSION_DECL}\n"
													 "uniform ${PREC_QUALIFIER} vec4 value;\n"
													 "\n"
													 "void main(void) { ${BODY} }\n";

		static const char fragment_source_template[] = "${VERSION_DECL}\n"
													   "out highp vec4 result;\n"
													   "uniform ${PREC_QUALIFIER} vec4 value;\n"
													   "\n"
													   "void main(void) { ${BODY} }\n";

		static const struct
		{
			glu::GLSLVersion minimum_supported_version;
			const char*		 vertex_precision;
			const char*		 vertex_body;
			const char*		 frag_precision;
			const char*		 frag_body;
			bool			 should_link;
		} shader_variants[] = {
			/* Use highp on both stages. Check variations of:
			 *  - of implicit and explicit precision qualifiers
			 *  - used and unused variables
			 * These variants should pass since the precision qualifiers match.
			 *
			 * These variants require highp to be supported, so will not be run for GLSL_VERSION_100_ES.
			 */
			{ glu::GLSL_VERSION_300_ES, "", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0);", "highp", "result = value;",
			  true },
			{ glu::GLSL_VERSION_300_ES, "", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0) + value;", "highp",
			  "result = value;", true },
			{ glu::GLSL_VERSION_300_ES, "highp", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0);", "highp", "result = value;",
			  true },
			{ glu::GLSL_VERSION_300_ES, "highp", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0) + value;", "highp",
			  "result = value;", true },

			/* Use highp in vertex shaders, mediump in fragment shaders. Check variations as above.
			 * These variants should fail since the precision qualifiers do not match, and matching is done
			 * based on declaration - independent of static use.
			 */
			{ glu::GLSL_VERSION_100_ES, "", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0);", "mediump", "result = value;",
			  false },
			{ glu::GLSL_VERSION_100_ES, "", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0) + value;", "mediump",
			  "result = value;", false },
			{ glu::GLSL_VERSION_100_ES, "highp", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0);", "mediump",
			  "result = value;", false },
			{ glu::GLSL_VERSION_100_ES, "highp", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0) + value;", "mediump",
			  "result = value;", false },

			/* Use mediump in vertex shaders, highp in fragment shaders. Check variations as above.
			 * These variations should fail for the same reason as above.
			 */
			{ glu::GLSL_VERSION_300_ES, "mediump", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0);", "highp",
			  "result = vec4(1.0, 1.0, 1.0, 1.0);", false },
			{ glu::GLSL_VERSION_300_ES, "mediump", "gl_Position = vec4(1.0, 1.0, 1.0, 1.0);", "highp",
			  "result = value;", false },
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(shader_variants); i++)
		{
			std::map<std::string, std::string> args;

			if (m_glslVersion <= shader_variants[i].minimum_supported_version)
			{
				continue;
			}

			args["VERSION_DECL"]   = glu::getGLSLVersionDeclaration(m_glslVersion);
			args["PREC_QUALIFIER"] = shader_variants[i].vertex_precision;
			args["BODY"]		   = shader_variants[i].vertex_body;
			std::string vcode	  = tcu::StringTemplate(vertex_source_template).specialize(args);

			args["PREC_QUALIFIER"] = shader_variants[i].frag_precision;
			args["BODY"]		   = shader_variants[i].frag_body;
			std::string fcode	  = tcu::StringTemplate(fragment_source_template).specialize(args);

			// Setup program.
			glu::ShaderProgram program(m_context.getRenderContext(),
									   glu::makeVtxFragSources(vcode.c_str(), fcode.c_str()));

			// Check that compile/link results are what we expect.
			bool		vertexOk   = program.getShaderInfo(glu::SHADERTYPE_VERTEX).compileOk;
			bool		fragmentOk = program.getShaderInfo(glu::SHADERTYPE_FRAGMENT).compileOk;
			bool		linkOk	 = program.getProgramInfo().linkOk;
			const char* failReason = DE_NULL;

			if (!vertexOk || !fragmentOk)
			{
				failReason = "expected shaders to compile, but failed.";
			}
			else if (shader_variants[i].should_link && !linkOk)
			{
				failReason = "expected shaders to link, but failed.";
			}
			else if (!shader_variants[i].should_link && linkOk)
			{
				failReason = "expected shaders to fail linking, but succeeded.";
			}

			if (failReason != DE_NULL)
			{
				log << TestLog::Message << "ERROR: " << failReason << TestLog::EndMessage;
				result = QP_TEST_RESULT_FAIL;
			}
		}

		m_testCtx.setTestResult(result, qpGetTestResultName(result));

		return STOP;
	}

protected:
	glu::GLSLVersion m_glslVersion;
};

class ShaderConstantSequenceExpressionCase : public TestCase
{
public:
	ShaderConstantSequenceExpressionCase(Context& context, const char* name, const char* description,
										 glu::GLSLVersion glslVersion)
		: TestCase(context, name, description), m_glslVersion(glslVersion)
	{
	}

	~ShaderConstantSequenceExpressionCase()
	{
		// empty
	}

	IterateResult iterate()
	{
		qpTestResult result = QP_TEST_RESULT_PASS;

		static const char vertex_source_template[] = "${VERSION_DECL}\n"
													 "precision mediump float;\n"
													 "const int test = (1, 2);\n"
													 "\n"
													 "void main(void) { gl_Position = vec4(test); }\n";

		static const char fragment_source_template[] = "${VERSION_DECL}\n"
													   "precision mediump float;\n"
													   "\n"
													   "void main(void) { }\n";

		std::map<std::string, std::string> args;
		args["VERSION_DECL"] = glu::getGLSLVersionDeclaration(m_glslVersion);

		std::string vertex_code   = tcu::StringTemplate(vertex_source_template).specialize(args);
		std::string fragment_code = tcu::StringTemplate(fragment_source_template).specialize(args);

		// Setup program.
		glu::ShaderProgram program(m_context.getRenderContext(),
								   glu::makeVtxFragSources(vertex_code.c_str(), fragment_code.c_str()));

		// GLSL does not allow the sequence operator in a constant expression
		// Check that either compilation or linking fails
		bool vertexOk   = program.getShaderInfo(glu::SHADERTYPE_VERTEX).compileOk;
		bool fragmentOk = program.getShaderInfo(glu::SHADERTYPE_FRAGMENT).compileOk;
		bool linkOk		= program.getProgramInfo().linkOk;

		bool run_test_es	  = (glu::glslVersionIsES(m_glslVersion) && m_glslVersion > glu::GLSL_VERSION_100_ES);
		bool run_test_desktop = (m_glslVersion > glu::GLSL_VERSION_420);
		if (run_test_es || run_test_desktop)
		{
			if (vertexOk && fragmentOk && linkOk)
				result = QP_TEST_RESULT_FAIL;
		}

		m_testCtx.setTestResult(result, qpGetTestResultName(result));

		return STOP;
	}

protected:
	glu::GLSLVersion m_glslVersion;
};

ShaderNegativeTests::ShaderNegativeTests(Context& context, glu::GLSLVersion glslVersion)
	: TestCaseGroup(context, "negative", "Shader Negative tests"), m_glslVersion(glslVersion)
{
	// empty
}

ShaderNegativeTests::~ShaderNegativeTests()
{
	// empty
}

void ShaderNegativeTests::init(void)
{
	addChild(new ShaderUniformInitializeGlobalCase(
		m_context, "initialize", "Verify initialization of globals with non-constant expressions fails on ES.",
		m_glslVersion));
#if 0
	/*
	 * This test is disabled for now since not all existing implementations follow these rules
	 * and enforcing them now would break existing applications, which is unacceptable.
	 *
	 * See https://cvs.khronos.org/bugzilla/show_bug.cgi?id=13922 for further details.
	 */
	addChild(new ShaderUniformPrecisionLinkCase(m_context, "uniform_precision_matching",
			"Verify that linking fails if precision qualifiers on default uniform do not match",
			m_glslVersion));
#endif
	addChild(new ShaderConstantSequenceExpressionCase(
		m_context, "constant_sequence", "Verify that the sequence operator cannot be used as a constant expression.",
		m_glslVersion));
}

} // deqp
