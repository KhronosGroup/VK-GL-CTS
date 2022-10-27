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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/*!
 * \file glcShaderNegativeTests.cpp
 * \brief Negative tests for shaders and interface matching.
 */ /*-------------------------------------------------------------------*/

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
using namespace glu;

struct ShaderVariants
{
	GLSLVersion minimum_supported_version;
	const char* vertex_precision;
	const char* vertex_body;
	const char* frag_precision;
	const char* frag_body;
	bool		should_link;
};

class ShaderUniformInitializeGlobalCase : public TestCase
{
public:
	ShaderUniformInitializeGlobalCase(Context& context, const char* name, const char* description,
									  GLSLVersion glslVersion)
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
		args["VERSION_DECL"] = getGLSLVersionDeclaration(m_glslVersion);

		std::string vertex_code   = tcu::StringTemplate(vertex_source_template).specialize(args);
		std::string fragment_code = tcu::StringTemplate(fragment_source_template).specialize(args);

		// Setup program.
		ShaderProgram program(m_context.getRenderContext(),
							  makeVtxFragSources(vertex_code.c_str(), fragment_code.c_str()));

		// GLSL ES does not allow initialization of global variables with non-constant
		// expressions, but GLSL does.
		// Check that either compilation or linking fails for ES, and that everything
		// succeeds for GL.
		bool vertexOk   = program.getShaderInfo(SHADERTYPE_VERTEX).compileOk;
		bool fragmentOk = program.getShaderInfo(SHADERTYPE_FRAGMENT).compileOk;
		bool linkOk		= program.getProgramInfo().linkOk;

		if (glslVersionIsES(m_glslVersion))
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
	GLSLVersion m_glslVersion;
};

class ShaderUniformPrecisionLinkCase : public TestCase
{
public:
	ShaderUniformPrecisionLinkCase(Context& context, const char* name, const char* description,
								   const ShaderVariants* shaderVariants, unsigned int shaderVariantsCount,
								   GLSLVersion glslVersion)
		: TestCase(context, name, description)
		, m_glslVersion(glslVersion)
		, m_shaderVariants(shaderVariants)
		, m_shaderVariantsCount(shaderVariantsCount)
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

		for (unsigned int i = 0; i < m_shaderVariantsCount; i++)
		{
			std::map<std::string, std::string> args;

			if (m_glslVersion <= m_shaderVariants[i].minimum_supported_version)
			{
				continue;
			}

			args["VERSION_DECL"]   = getGLSLVersionDeclaration(m_glslVersion);
			args["PREC_QUALIFIER"] = m_shaderVariants[i].vertex_precision;
			args["BODY"]		   = m_shaderVariants[i].vertex_body;
			std::string vcode	  = tcu::StringTemplate(vertex_source_template).specialize(args);

			args["PREC_QUALIFIER"] = m_shaderVariants[i].frag_precision;
			args["BODY"]		   = m_shaderVariants[i].frag_body;
			std::string fcode	  = tcu::StringTemplate(fragment_source_template).specialize(args);

			// Setup program.
			ShaderProgram program(m_context.getRenderContext(), makeVtxFragSources(vcode.c_str(), fcode.c_str()));

			// Check that compile/link results are what we expect.
			bool		vertexOk   = program.getShaderInfo(SHADERTYPE_VERTEX).compileOk;
			bool		fragmentOk = program.getShaderInfo(SHADERTYPE_FRAGMENT).compileOk;
			bool		linkOk	 = program.getProgramInfo().linkOk;
			const char* failReason = DE_NULL;

			if (!vertexOk || !fragmentOk)
			{
				failReason = "expected shaders to compile, but failed.";
			}
			else if (m_shaderVariants[i].should_link && !linkOk)
			{
				failReason = "expected shaders to link, but failed.";
			}
			else if (!m_shaderVariants[i].should_link && linkOk)
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
	GLSLVersion			  m_glslVersion;
	const ShaderVariants* m_shaderVariants;
	unsigned int		  m_shaderVariantsCount;
};

class ShaderConstantSequenceExpressionCase : public TestCase
{
public:
	ShaderConstantSequenceExpressionCase(Context& context, const char* name, const char* description,
										 GLSLVersion glslVersion)
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
		args["VERSION_DECL"] = getGLSLVersionDeclaration(m_glslVersion);

		std::string vertex_code   = tcu::StringTemplate(vertex_source_template).specialize(args);
		std::string fragment_code = tcu::StringTemplate(fragment_source_template).specialize(args);

		// Setup program.
		ShaderProgram program(m_context.getRenderContext(),
							  makeVtxFragSources(vertex_code.c_str(), fragment_code.c_str()));

		// GLSL does not allow the sequence operator in a constant expression
		// Check that either compilation or linking fails
		bool vertexOk   = program.getShaderInfo(SHADERTYPE_VERTEX).compileOk;
		bool fragmentOk = program.getShaderInfo(SHADERTYPE_FRAGMENT).compileOk;
		bool linkOk		= program.getProgramInfo().linkOk;

		bool run_test_es	  = (glslVersionIsES(m_glslVersion) && m_glslVersion > GLSL_VERSION_100_ES);
		bool run_test_desktop = (m_glslVersion > GLSL_VERSION_420);
		if (run_test_es || run_test_desktop)
		{
			if (vertexOk && fragmentOk && linkOk)
				result = QP_TEST_RESULT_FAIL;
		}

		m_testCtx.setTestResult(result, qpGetTestResultName(result));

		return STOP;
	}

protected:
	GLSLVersion m_glslVersion;
};

class ShaderNonPrecisionQualifiersStructCase : public TestCase
{
public:
	ShaderNonPrecisionQualifiersStructCase(Context& context, const char* name, const char* description,
										 GLSLVersion glslVersion)
		: TestCase(context, name, description), m_glslVersion(glslVersion)
	{
	}

	~ShaderNonPrecisionQualifiersStructCase()
	{
		// empty
	}

	IterateResult iterate()
	{
		static const char* qualifier_values[] =
		{
			// Storage Qualifiers
			"const",
			"in",
			"out",
			"attribute",
			"uniform",
			"varying",
			"buffer",
			"shared",

			// Interpolation Qualifiers
			"smooth in",
			"flat in",
			"noperspective in",
			"smooth out",
			"flat out",
			"noperspective out",

			// Invariant Qualifier
			"invariant",

			// Precise Qualifier
			"precise",

			// Memory Qualifiers
			"coherent",
			"volatile",
			"restrict",
			"readonly",
			"writeonly",
		};
		static const unsigned qualifier_count = sizeof(qualifier_values) / sizeof(qualifier_values[0]);

		static const char* layout_values[] =
		{
			"(shared)",
			"(packed)",
			"(std140)",
			"(std430)",

			"(row_major)",
			"(column_major)",
		};
		static const unsigned layout_count = sizeof(layout_values) / sizeof(layout_values[0]);

		const std::string layout_str = "layout";

		std::map<std::string, std::string> args;
		args["VERSION_DECL"] = getGLSLVersionDeclaration(m_glslVersion);

		// Vertex and fragment shaders
		{
			// Layout qualifier test
			args["QUALIFIER"] = layout_str;
			for (unsigned i = 0; i < layout_count; ++i)
			{
				args["LAYOUT_VALUE"] = layout_values[i];
				if (testVertexFragment(args, layout_str + layout_values[i]))
					return STOP;
			}

			// Remaining qualifier tests
			args["LAYOUT_VALUE"] = "";
			for (unsigned i = 0; i < qualifier_count; ++i)
			{
				args["QUALIFIER"] = qualifier_values[i];
				if (testVertexFragment(args, qualifier_values[i]))
					return STOP;
			}
		}

		// Compute shader, not available for GLES2 and GLES3
		if (!glslVersionIsES(m_glslVersion) || m_glslVersion >= GLSL_VERSION_310_ES)
		{
			// Layout qualifier test
			args["QUALIFIER"] = layout_str;
			for (unsigned i = 0; i < layout_count; ++i)
			{
				args["LAYOUT_VALUE"] = layout_values[i];
				if (testCompute(args, layout_str + layout_values[i]))
					return STOP;
			}

			// Remaining qualifier tests
			args["LAYOUT_VALUE"] = "";
			for (unsigned i = 0; i < qualifier_count; ++i)
			{
				args["QUALIFIER"] = qualifier_values[i];
				if (testCompute(args, qualifier_values[i]))
					return STOP;
			}
		}

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, qpGetTestResultName(QP_TEST_RESULT_PASS));

		return STOP;
	}

protected:
	bool testVertexFragment(const std::map<std::string, std::string>& args, const std::string& qualifier_name)
	{
		static const char* vertex_source_template = "${VERSION_DECL}\n"
													"precision mediump float;\n"
													"struct Base\n"
													"{\n"
													"  ${QUALIFIER} ${LAYOUT_VALUE} mat4 some_matrix;\n"
													"};\n"
													"\n"
													"void main(void)\n"
													"{\n"
													"  gl_Position = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
													"}\n";

		static const char* fragment_source_template = "${VERSION_DECL}\n"
													  "precision mediump float;\n"
													  "struct Base\n"
													  "{\n"
													  "  ${QUALIFIER} ${LAYOUT_VALUE} mat4 some_matrix;\n"
													  "};\n"
													  "\n"
													  "void main(void) { }\n";

		std::string vertex_code   = tcu::StringTemplate(vertex_source_template).specialize(args);
		std::string fragment_code = tcu::StringTemplate(fragment_source_template).specialize(args);
		ShaderProgram program(m_context.getRenderContext(), makeVtxFragSources(vertex_code.c_str(), fragment_code.c_str()));
		if (program.getShaderInfo(SHADERTYPE_VERTEX).compileOk || program.getShaderInfo(SHADERTYPE_FRAGMENT).compileOk)
		{
			m_testCtx.getLog() << TestLog::Message << "ERROR: expected shaders not to compile, but failed with \'"
							   << qualifier_name << "\' qualifier." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, qpGetTestResultName(QP_TEST_RESULT_FAIL));
			return true;
		}
		return false;
	}

	bool testCompute(const std::map<std::string, std::string>& args, const std::string& qualifier_name)
	{
		static const char* compute_source_template = "${VERSION_DECL}\n"
													 "precision mediump float;\n"
													 "struct Base\n"
													 "{\n"
													 "  ${QUALIFIER} ${LAYOUT_VALUE} mat4 some_matrix;\n"
													 "};\n"
													 "\n"
													 "void main(void) { }\n";

		std::string compute_code   = tcu::StringTemplate(compute_source_template).specialize(args);
		ProgramSources sources;
		sources.sources[SHADERTYPE_COMPUTE].emplace_back(compute_code);
		ShaderProgram program(m_context.getRenderContext(), sources);
		if (program.getShaderInfo(SHADERTYPE_COMPUTE).compileOk)
		{
			m_testCtx.getLog() << TestLog::Message << "ERROR: expected compute shader not to compile, but failed with \'"
							   << qualifier_name << "\' qualifier." << TestLog::EndMessage;
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, qpGetTestResultName(QP_TEST_RESULT_FAIL));
			return true;
		}
		return false;
	}

	GLSLVersion m_glslVersion;
};

ShaderNegativeTests::ShaderNegativeTests(Context& context, GLSLVersion glslVersion)
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

	addChild(new ShaderConstantSequenceExpressionCase(
		m_context, "constant_sequence", "Verify that the sequence operator cannot be used as a constant expression.",
		m_glslVersion));

	addChild(new ShaderNonPrecisionQualifiersStructCase(
		m_context, "non_precision_qualifiers_in_struct_members", "Verify non-precision qualifiers in struct members are not allowed.",
		m_glslVersion));

	if (isGLSLVersionSupported(m_context.getRenderContext().getType(), GLSL_VERSION_320_ES))
	{
		static const ShaderVariants used_variables_variants[] = {
			/* These variants should pass since the precision qualifiers match.
			 * These variants require highp to be supported, so will not be run for GLSL_VERSION_100_ES.
			 */
			{ GLSL_VERSION_300_ES, "", "gl_Position = vec4(1.0) + value;", "highp", "result = value;", true },
			{ GLSL_VERSION_300_ES, "highp", "gl_Position = vec4(1.0) + value;", "highp", "result = value;", true },

			/* Use highp in vertex shaders, mediump in fragment shaders. Check variations as above.
			 * These variants should fail since the precision qualifiers do not match, and matching is done
			 * based on declaration - independent of static use.
			 */
			{ GLSL_VERSION_100_ES, "", "gl_Position = vec4(1.0) + value;", "mediump", "result = value;", false },
			{ GLSL_VERSION_100_ES, "highp", "gl_Position = vec4(1.0) + value;", "mediump", "result = value;", false },
		};
		unsigned int used_variables_variants_count = sizeof(used_variables_variants) / sizeof(ShaderVariants);

		addChild(new ShaderUniformPrecisionLinkCase(
			m_context, "used_uniform_precision_matching",
			"Verify that linking fails if precision qualifiers on default uniform do not match",
			used_variables_variants, used_variables_variants_count, m_glslVersion));
	}
}

} // deqp
