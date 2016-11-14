/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
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
 *//*!
 * \file
 * \brief
 *//*--------------------------------------------------------------------*/

/**
 */ /*!
 * \file  gl4cParallelShaderCompileTests.cpp
 * \brief Conformance tests for the GL_ARB_parallel_shader_compile functionality.
 */ /*--------------------------------------------------------------------*/

#include "gl4cParallelShaderCompileTests.hpp"
#include "deClock.h"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

using namespace glu;
using namespace glw;

namespace gl4cts
{

static const char* vShader = "#version 450\n"
							 "\n"
							 "in vec3 vertex;\n"
							 "\n"
							 "int main() {\n"
							 "    gl_Position = vec4(vertex, 1);\n"
							 "}\n";

static const char* fShader = "#version 450\n"
							 "\n"
							 "out ver4 fragColor;\n"
							 "\n"
							 "int main() {\n"
							 "    fragColor = vec4(1, 1, 1, 1);\n"
							 "}\n";

/** Constructor.
 *
 *  @param context     Rendering context
 *  @param name        Test name
 *  @param description Test description
 */
SimpleQueriesTest::SimpleQueriesTest(deqp::Context& context)
	: TestCase(context, "SimpleQueriesTest",
			   "Tests verifies if simple queries works as expected for MAX_SHADER_COMPILER_THREADS_ARB <pname>")
{
	/* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult SimpleQueriesTest::iterate()
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_parallel_shader_compile"))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
		return STOP;
	}

	const Functions& gl = m_context.getRenderContext().getFunctions();

	GLboolean boolValue;
	GLint	 intValue;
	GLint64   int64Value;
	GLfloat   floatValue;
	GLdouble  doubleValue;

	gl.getBooleanv(GL_MAX_SHADER_COMPILER_THREADS_ARB, &boolValue);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

	gl.getIntegerv(GL_MAX_SHADER_COMPILER_THREADS_ARB, &intValue);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

	gl.getInteger64v(GL_MAX_SHADER_COMPILER_THREADS_ARB, &int64Value);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getInteger64v");

	gl.getFloatv(GL_MAX_SHADER_COMPILER_THREADS_ARB, &floatValue);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getFloatv");

	gl.getDoublev(GL_MAX_SHADER_COMPILER_THREADS_ARB, &doubleValue);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getDoublev");

	if (boolValue != (intValue != 0) || intValue != (GLint)int64Value || intValue != (GLint)floatValue ||
		intValue != (GLint)doubleValue)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Simple queries returned different values: "
						   << "bool(" << (int)boolValue << "), "
						   << "int(" << intValue << "), "
						   << "int64(" << int64Value << "), "
						   << "float(" << floatValue << "), "
						   << "double(" << doubleValue << ")" << tcu::TestLog::EndMessage;

		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
		return STOP;
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 *  @param name        Test name
 *  @param description Test description
 */
MaxShaderCompileThreadsTest::MaxShaderCompileThreadsTest(deqp::Context& context)
	: TestCase(context, "MaxShaderCompileThreadsTest",
			   "Tests verifies if MaxShaderCompileThreadsARB function works as expected")
{
	/* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult MaxShaderCompileThreadsTest::iterate()
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_parallel_shader_compile"))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
		return STOP;
	}

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	GLint intValue;

	gl.maxShaderCompilerThreadsARB(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "maxShaderCompilerThreadsARB");

	gl.getIntegerv(GL_MAX_SHADER_COMPILER_THREADS_ARB, &intValue);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

	if (intValue != 0)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Failed to disable parallel shader compilation.");
		return STOP;
	}

	gl.maxShaderCompilerThreadsARB(0xFFFFFFFF);
	GLU_EXPECT_NO_ERROR(gl.getError(), "maxShaderCompilerThreadsARB");

	gl.getIntegerv(GL_MAX_SHADER_COMPILER_THREADS_ARB, &intValue);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

	if (intValue != GLint(0xFFFFFFFF))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Failed to set maximum shader compiler threads.");
		return STOP;
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 *  @param name        Test name
 *  @param description Test description
 */
CompilationCompletionNonParallelTest::CompilationCompletionNonParallelTest(deqp::Context& context)
	: TestCase(context, "CompilationCompletionNonParallelTest",
			   "Tests verifies if shader COMPLETION_STATUS query works as expected for non parallel compilation")
{
	/* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult CompilationCompletionNonParallelTest::iterate()
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_parallel_shader_compile"))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
		return STOP;
	}

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	GLint completionStatus;

	gl.maxShaderCompilerThreadsARB(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "maxShaderCompilerThreadsARB");

	{
		Program program(gl);
		Shader  vertexShader(gl, SHADERTYPE_VERTEX);
		Shader  fragmentShader(gl, SHADERTYPE_FRAGMENT);

		const char* vSources[] = { vShader };
		const int   vLengths[] = { int(strlen(vShader)) };
		vertexShader.setSources(1, vSources, vLengths);

		const char* fSources[] = { fShader };
		const int   fLengths[] = { int(strlen(fShader)) };
		fragmentShader.setSources(1, fSources, fLengths);

		gl.compileShader(vertexShader.getShader());
		GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");
		gl.compileShader(fragmentShader.getShader());
		GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");

		gl.getShaderiv(fragmentShader.getShader(), GL_COMPLETION_STATUS_ARB, &completionStatus);
		GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");
		if (!completionStatus)
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
									"Failed reading completion status for non parallel shader compiling");
			return STOP;
		}

		program.attachShader(vertexShader.getShader());
		program.attachShader(fragmentShader.getShader());
		gl.linkProgram(program.getProgram());

		gl.getProgramiv(program.getProgram(), GL_COMPLETION_STATUS_ARB, &completionStatus);
		GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramiv");
		if (!completionStatus)
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
									"Failed reading completion status for non parallel program linking");
			return STOP;
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 *  @param name        Test name
 *  @param description Test description
 */
CompilationCompletionParallelTest::CompilationCompletionParallelTest(deqp::Context& context)
	: TestCase(context, "CompilationCompletionParallelTest",
			   "Tests verifies if shader COMPLETION_STATUS query works as expected for parallel compilation")
{
	/* Left blank intentionally */
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult CompilationCompletionParallelTest::iterate()
{
	if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_parallel_shader_compile"))
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
		return STOP;
	}

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	GLint completionStatus;

	gl.maxShaderCompilerThreadsARB(8);
	GLU_EXPECT_NO_ERROR(gl.getError(), "maxShaderCompilerThreadsARB");

	{
		Shader   vertexShader(gl, SHADERTYPE_VERTEX);
		deUint32 fragmentShader[8];
		deUint32 program[8];

		for (int i = 0; i < 8; ++i)
		{
			fragmentShader[i] = gl.createShader(GL_FRAGMENT_SHADER);
			program[i]		  = gl.createProgram();
		}

		const char* vSources[] = { vShader };
		const int   vLengths[] = { int(strlen(vShader)) };
		vertexShader.setSources(1, vSources, vLengths);

		//Compilation test
		for (int i = 0; i < 8; ++i)
		{
			const char* fSources[] = { fShader };
			const int   fLengths[] = { int(strlen(fShader)) };
			gl.shaderSource(fragmentShader[i], 1, fSources, fLengths);
		}

		gl.compileShader(vertexShader.getShader());
		GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");
		for (int i = 0; i < 8; ++i)
		{
			gl.compileShader(fragmentShader[i]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");
		}

		{
			int		 completion  = 0;
			deUint64 shLoopStart = deGetMicroseconds();
			while (completion != 8 && deGetMicroseconds() < shLoopStart + 1000000)
			{
				completion = 0;
				for (int i = 0; i < 8; ++i)
				{
					gl.getShaderiv(fragmentShader[i], GL_COMPLETION_STATUS_ARB, &completionStatus);
					GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");
					if (completionStatus)
						completion++;
				}
			}
			if (completion != 8)
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
										"Failed reading completion status for parallel shader compiling");
				for (int i = 0; i < 8; ++i)
				{
					gl.deleteProgram(program[i]);
					gl.deleteShader(fragmentShader[i]);
				}
				return STOP;
			}
		}

		for (int i = 0; i < 8; ++i)
		{
			gl.attachShader(program[i], vertexShader.getShader());
			GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");
			gl.attachShader(program[i], fragmentShader[i]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");
		}

		//Linking test
		for (int i = 0; i < 8; ++i)
		{
			gl.linkProgram(program[i]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "linkProgram");
		}

		{
			int		 completion  = 0;
			deUint64 prLoopStart = deGetMicroseconds();
			while (completion != 8 && deGetMicroseconds() < prLoopStart + 1000000)
			{
				completion = 0;
				for (int i = 0; i < 8; ++i)
				{
					gl.getProgramiv(program[i], GL_COMPLETION_STATUS_ARB, &completionStatus);
					GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramiv");
					if (completionStatus)
						completion++;
				}
			}
			if (completion != 8)
			{
				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
										"Failed reading completion status for parallel program linking");
				for (int i = 0; i < 8; ++i)
				{
					gl.deleteProgram(program[i]);
					gl.deleteShader(fragmentShader[i]);
				}
				return STOP;
			}
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
ParallelShaderCompileTests::ParallelShaderCompileTests(deqp::Context& context)
	: TestCaseGroup(context, "parallel_shader_compile",
					"Verify conformance of CTS_ARB_parallel_shader_compile implementation")
{
}

/** Initializes the test group contents. */
void ParallelShaderCompileTests::init()
{
	addChild(new SimpleQueriesTest(m_context));
	addChild(new MaxShaderCompileThreadsTest(m_context));
	addChild(new CompilationCompletionNonParallelTest(m_context));
	addChild(new CompilationCompletionParallelTest(m_context));
}

} /* gl4cts namespace */
