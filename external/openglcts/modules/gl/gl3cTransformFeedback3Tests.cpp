/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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

/**
 */ /*!
 * \file  gl3cTransformFeedback3Tests.cpp
 * \brief Conformance tests for ARB_transform_feedback3 functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "gl3cTransformFeedback3Tests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace glu;

namespace
{

const float FLOAT_EPSILON = 1.0e-03F;

struct enumTypeTest
{
	GLenum		type = GL_NONE; //GL_FLOAT..
	int			bytesize = 0;
	int			w = 0;
	int			h = 0;
	const void* values = nullptr;
};

} // namespace

namespace gl3cts
{

// clang-format off
/** @brief Vertex shader source code for multiple streams transform feedback test. */
const glw::GLchar* gl3cts::TransformFeedbackMultipleStreamsTestCase::m_shader_mult_streams_vert =
	R"(${VERSION}
	in vec4 vertex;

	void main (void)
	{
		gl_Position = vertex;
	}
	)";

/** @brief Vertex shader source code  for multiple streams transform feedback test . */
const glw::GLchar* gl3cts::TransformFeedbackMultipleStreamsTestCase::m_shader_mult_streams_geom =
	R"(${VERSION}
	${EXTENSION}

	layout(points) in;
	layout(points, max_vertices = 8) out;

	layout(stream=0) out vec4 pos0;
	layout(stream=1) out vec4 pos1;

	void main() {
		pos0 = vec4(0.1, 0.2, 0.3, 0.4) * gl_in[0].gl_Position;
		gl_Position = vec4(0.9, 0.9, 0.0, 1.0) * gl_in[0].gl_Position;
		EmitStreamVertex(0);
		EndStreamPrimitive(0);

		pos1 = vec4(-0.1, -0.2, -0.3, -0.4) * gl_in[0].gl_Position;
		gl_Position = vec4(-0.9, -0.9, 0.0, 1.0) * gl_in[0].gl_Position;
		EmitStreamVertex(1);
		EndStreamPrimitive(1);
	}
	)";

/** @brief Vertex shader source code for multiple streams transform feedback test. */
const glw::GLchar* gl3cts::TransformFeedbackBaseTestCase::m_shader_vert =
	R"(${VERSION}
	in vec4 vertex;
	out vec4 value1;
	out vec4 value2;
	out vec4 value3;
	out vec4 value4;

	void main (void)
	{
		vec4 temp = vertex;

		//temp.xyz *= 0.5;

		gl_Position = temp;

		value1 = abs(temp) * 1.0;
		value2 = abs(temp) * 2.0;
		value3 = abs(temp) * 3.0;
		value4 = abs(temp) * 4.0;
	}
	)";

/** @brief Vertex shader source code  for multiple streams transform feedback test . */
const glw::GLchar* gl3cts::TransformFeedbackBaseTestCase::m_shader_frag =
	R"(${VERSION}

	void main (void)
	{
		gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
	)";
// clang-format on

/** Stub deinit method. */
void TransformFeedbackBaseTestCase::deinit()
{
	/* Left blank intentionally */
}

/** Stub init method */
void TransformFeedbackBaseTestCase::init()
{
	specializationMap["VERSION"] = "#version 150";
	specializationMap["EXTENSION"] = "";

	auto contextType = m_context.getRenderContext().getType();
	/* This test should only be executed if we're running a GL>=3.0 context */
	m_testSupported = (glu::contextSupports(contextType, glu::ApiType::core(3, 0)) &&
					   m_context.getContextInfo().isExtensionSupported("GL_ARB_transform_feedback3")) ||
					  glu::contextSupports(contextType, glu::ApiType::core(4, 0));

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();
	GLint value = 0;
	gl.getIntegerv(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS, &value);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

	m_testSupported = m_testSupported && (value >= 4);

	gl.getIntegerv(GL_MAX_VERTEX_STREAMS, &value);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

	m_testSupported = m_testSupported && (value >= 1);
}

/* creates transform feedback buffer at certain index. leaves buffer bound */
bool TransformFeedbackBaseTestCase::createTransformBuffer(const int size, const GLint buffer,
																	 const GLint index)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STATIC_READ);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	return true;
}

/* creates and set up vertex buffers related to given program */
void TransformFeedbackBaseTestCase::createVertexBuffers(const glw::GLuint& program, std::vector<GLfloat>& verts)
{
	if (program != 0)
	{
		const glw::Functions& gl = m_context.getRenderContext().getFunctions();

		gl.genVertexArrays(1, &m_vao);
		GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
		gl.bindVertexArray(m_vao);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

		gl.genBuffers(1, &m_vbo);
		GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
		gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.bufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * verts.size(), (GLvoid*)verts.data(), GL_STATIC_DRAW);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

		gl.useProgram(program);
		GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

		GLint locVertices = gl.getAttribLocation(program, "vertex");
		GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");
		if (locVertices != -1)
		{
			gl.enableVertexAttribArray(0);
			GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

			gl.vertexAttribPointer(locVertices, 4, GL_FLOAT, GL_FALSE, 0, DE_NULL);
			GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
		}
	}
}

/* function releases vertex buffers */
void TransformFeedbackBaseTestCase::releaseVertexBuffers()
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();
	gl.disableVertexAttribArray(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

	if (m_vbo)
	{
		gl.deleteBuffers(1, &m_vbo);
		GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
		m_vbo = 0;
	}

	if (m_vao)
	{
		gl.deleteVertexArrays(1, &m_vao);
		GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");
		m_vao = 0;
	}
}

/* copies data from transform feedback buffer */
bool TransformFeedbackBaseTestCase::readBuffer(const int size, const GLint buffer, std::vector<char>& data)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	data.resize(size);

	gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer, 0, size);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferRange");

	char* ret = (char*)gl.mapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size, GL_MAP_READ_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "mapBufferRange");

	if (!ret)
		return false;

	data.assign(ret, ret + size);

	gl.unmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
	GLU_EXPECT_NO_ERROR(gl.getError(), "unmapBuffer");

	return true;
}

/* utility functions for transform_feedback3 tests */
/* compares contents of two arrays */
bool TransformFeedbackBaseTestCase::compareArrays(const GLfloat* const d1, const int s1, const GLfloat* d2,
															 int s2)
{
	if (!d1 || !d2)
		return false;

	if (s1 != s2)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "compareArrays:arrays are different sizes (" << s1 << ", " << s2
						   << ")" << tcu::TestLog::EndMessage;
		return false;
	}

	for (int i = 0; i < s1; ++i)
	{
		if (fabs(d1[i] - d2[i]) > FLOAT_EPSILON)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "compareArrays(GLfloat):index " << i << " value " << d1[i]
							   << " != " << d2[i] << tcu::TestLog::EndMessage;
			return false;
		}
	}
	return true;
}

/* Compiles and links transform feedback program. */
void TransformFeedbackBaseTestCase::buildTransformFeedbackProgram(const char* vsSource,
																  const char* gsSource,
																  const char* fsSource)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	GLint status;

	m_program = gl.createProgram();

	GLuint vShader = gl.createShader(GL_VERTEX_SHADER);
	gl.shaderSource(vShader, 1, (const char**)&vsSource, NULL);
	gl.compileShader(vShader);
	gl.getShaderiv(vShader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint infoLogLength = 0;
		gl.getShaderiv(vShader, GL_INFO_LOG_LENGTH, &infoLogLength);

		std::vector<char> infoLogBuf(infoLogLength + 1);
		gl.getShaderInfoLog(vShader, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);

		std::string infoLog = &infoLogBuf[0];
		m_testCtx.getLog().writeShader(QP_SHADER_TYPE_VERTEX, vsSource, false, infoLog.c_str());

		gl.deleteShader(vShader);

		TCU_FAIL("Failed to compile transform feedback vertex shader");
	}
	gl.attachShader(m_program, vShader);
	gl.deleteShader(vShader);

	if (gsSource)
	{
		GLuint gShader = gl.createShader(GL_GEOMETRY_SHADER);
		gl.shaderSource(gShader, 1, (const char**)&gsSource, NULL);
		gl.compileShader(gShader);
		gl.getShaderiv(gShader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint infoLogLength = 0;
			gl.getShaderiv(gShader, GL_INFO_LOG_LENGTH, &infoLogLength);

			std::vector<char> infoLogBuf(infoLogLength + 1);
			gl.getShaderInfoLog(gShader, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);

			std::string infoLog = &infoLogBuf[0];
			m_testCtx.getLog().writeShader(QP_SHADER_TYPE_GEOMETRY, gsSource, false, infoLog.c_str());

			gl.deleteShader(gShader);

			TCU_FAIL("Failed to compile transform feedback geometry shader");
		}
		gl.attachShader(m_program, gShader);
		gl.deleteShader(gShader);
	}

	if (fsSource)
	{
		GLuint fShader = gl.createShader(GL_FRAGMENT_SHADER);
		gl.shaderSource(fShader, 1, (const char**)&fsSource, NULL);
		gl.compileShader(fShader);
		gl.getShaderiv(fShader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE)
		{
			GLint infoLogLength = 0;
			gl.getShaderiv(fShader, GL_INFO_LOG_LENGTH, &infoLogLength);

			std::vector<char> infoLogBuf(infoLogLength + 1);
			gl.getShaderInfoLog(fShader, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);

			std::string infoLog = &infoLogBuf[0];
			m_testCtx.getLog().writeShader(QP_SHADER_TYPE_FRAGMENT, fsSource, false, infoLog.c_str());

			gl.deleteShader(fShader);

			TCU_FAIL("Failed to compile transform feedback fragment shader");
		}
		gl.attachShader(m_program, fShader);
		gl.deleteShader(fShader);
	}

	gl.transformFeedbackVaryings(m_program, varyingsCount(), varyings(), bufferMode());
	gl.linkProgram(m_program);
	gl.getProgramiv(m_program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint infoLogLength = 0;
		gl.getProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLogLength);

		std::vector<char> infoLogBuf(infoLogLength + 1);
		gl.getProgramInfoLog(m_program, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);

		std::string infoLog = &infoLogBuf[0];
		m_testCtx.getLog().writeShader(QP_SHADER_TYPE_VERTEX, vsSource, true, infoLog.c_str());

		TCU_FAIL("Failed to link transform feedback program");
	}

	gl.useProgram(m_program);
}


/*-------------------------------------------------------------------*/
/** Constructor.
 *
 *  @param context     Rendering context
 */
TransformFeedbackMultipleStreamsTestCase::TransformFeedbackMultipleStreamsTestCase(deqp::Context& context)
	: TransformFeedbackBaseTestCase(context, "multiple_streams",
									"Verifies two streams writing to the same buffer object functionality")
{
}

/** Stub init method */
void TransformFeedbackMultipleStreamsTestCase::init()
{
	TransformFeedbackBaseTestCase::init();

	if (!m_testSupported)
		return;

	auto contextType = m_context.getRenderContext().getType();
	if (glu::contextSupports(contextType, glu::ApiType::core(4, 0)))
	{
			specializationMap["VERSION"] = "#version 400";
			specializationMap["EXTENSION"] = "";
	}
	else // 3.0 context supported, verify extension
	{
		if (!m_context.getContextInfo().isExtensionSupported("GL_ARB_gpu_shader5"))
		{
			m_testSupported = false;
		}
		else
		{
			specializationMap["EXTENSION"] = "#extension GL_ARB_gpu_shader5 : enable";
		}
	}

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();
	GLint value = 0;
	gl.getIntegerv(GL_MAX_VERTEX_STREAMS, &value);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

	// the test requires two vertex streams
	m_testSupported = m_testSupported && (value >= 2);
}

/* The varying name list contains all outputs in order. */
const char** TransformFeedbackMultipleStreamsTestCase::varyings()
{
	static const char* vars[] = {
		"pos0",
		"gl_SkipComponents4",
		"gl_NextBuffer",
		"gl_SkipComponents4",
		"pos1",
	};

	return vars;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TransformFeedbackMultipleStreamsTestCase::iterate()
{
	if (!m_testSupported)
	{
		throw tcu::NotSupportedError("Test multiple_streams is not supported");
		return STOP;
	}

	bool ret=true;
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	/* setup shader program */
	std::string vshader = tcu::StringTemplate(m_shader_mult_streams_vert).specialize(specializationMap);
	std::string gshader = tcu::StringTemplate(m_shader_mult_streams_geom).specialize(specializationMap);

	{
		ProgramSources sources;
		sources.sources[SHADERTYPE_VERTEX].push_back(vshader);
		sources.sources[SHADERTYPE_GEOMETRY].push_back(gshader);

		ShaderProgram checker_program(gl, sources);

		if (!checker_program.isOk())
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
							   << "Vertex: " << checker_program.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
							   << checker_program.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
							   << "Geometry: " << checker_program.getShaderInfo(SHADERTYPE_GEOMETRY).infoLog << "\n"
							   << checker_program.getShader(SHADERTYPE_GEOMETRY)->getSource() << "\n"
							   << "Program: " << checker_program.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
			TCU_FAIL("Compile failed");
		}
	}

	buildTransformFeedbackProgram(vshader.c_str(), gshader.c_str(), nullptr);

	// clang-format off
	std::vector<GLfloat> vertices = {
		-1.0f, -1.0f, -1.0f, 1.0f,
		 1.0f, -1.0f, -2.0f, 1.0f,
		-1.0f, 1.0f, -3.0f, 1.0f,
	};

	/* expected values */
	GLfloat correctvalues[] = {
		-0.1f, -0.2f, -0.3f, 0.4f,
		 0.1f,  0.2f, 0.3f,	-0.4f,
		 0.1f, -0.2f, -0.6f, 0.4f,
		-0.1f, 0.2f,  0.6f,	 -0.4f,
		-0.1f, 0.2f, -0.9f, 0.4f,
		 0.1f, -0.2f, 0.9f,  -0.4f,
	};
	// clang-format on

	createVertexBuffers(m_program, vertices);

	GLuint buffer=0, query=0, queryresult=0;

	gl.genBuffers(1, &buffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	GLuint buffer_size = sizeof(correctvalues);
	createTransformBuffer(buffer_size, buffer, 0);

	gl.genQueries(1, &query);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");

	{
		gl.enable(GL_RASTERIZER_DISCARD);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

		gl.clearColor(0.1f, 0.0f, 0.5f, 1.0f);
		GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

		gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

		gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer, 0, buffer_size);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferRange");

		gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, buffer, 0, buffer_size);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferRange");

		gl.beginQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 0, query);
		GLU_EXPECT_NO_ERROR(gl.getError(), "beginQueryIndexed");

		gl.beginTransformFeedback(GL_POINTS);
		GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");

		gl.drawArrays(GL_POINTS, 0, 3);
		GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

		gl.endTransformFeedback();
		GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

		gl.endQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "endQueryIndexed");

		gl.disable(GL_RASTERIZER_DISCARD);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
	}

	std::vector<char> data;
	if (!readBuffer(buffer_size, buffer, data))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Reading result buffer failed!" << tcu::TestLog::EndMessage;
		ret = false;
	}

	gl.getQueryObjectuiv(query, GL_QUERY_RESULT, &queryresult);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");

	if (queryresult != vertices.size() / 4)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Transform feedback query result not as expected!"
						   << tcu::TestLog::EndMessage;
		ret = false;
	}

	if (ret)
	{
		if (compareArrays((GLfloat*)data.data(), 4 * 3 * 2, correctvalues, sizeof(correctvalues) / sizeof(GLfloat)) ==
			false)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Result comparison failed!" << tcu::TestLog::EndMessage;
			ret = false;
		}
	}

	gl.deleteBuffers(1, &buffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	gl.deleteQueries(1, &query);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

	releaseVertexBuffers();

	if (ret)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	return STOP;
}

/*-------------------------------------------------------------------*/
/** Constructor.
*
*  @param context     Rendering context
*/
TransformFeedbackSkipComponentsTestCase::TransformFeedbackSkipComponentsTestCase(deqp::Context& context)
	: TransformFeedbackBaseTestCase(context, "skip_components",
									"Verifies functionality of skipping components of transform feedback buffer")
{
}

/* The varying name list contains all outputs in order. */
const char** TransformFeedbackSkipComponentsTestCase::varyings()
{
	static const char* vars[] = { "gl_SkipComponents1",
								  "value1",
								  "gl_SkipComponents2",
								  "gl_SkipComponents1",
								  "value2",
								  "gl_SkipComponents3",
								  "gl_SkipComponents2",
								  "value3",
								  "gl_SkipComponents4",
								  "value4" };

	return vars;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TransformFeedbackSkipComponentsTestCase::iterate()
{
	if (!m_testSupported)
	{
		throw tcu::NotSupportedError("Test skip_components is not supported");
		return STOP;
	}

	bool				  ret = true;
	const glw::Functions& gl  = m_context.getRenderContext().getFunctions();

	/* setup shader program */
	std::string vshader = tcu::StringTemplate(m_shader_vert).specialize(specializationMap);
	std::string fshader = tcu::StringTemplate(m_shader_frag).specialize(specializationMap);

	{
		ProgramSources sources = makeVtxFragSources(vshader, fshader);

		ShaderProgram checker_program(gl, sources);

		if (!checker_program.isOk())
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
							   << "Vertex: " << checker_program.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
							   << checker_program.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
							   << "Fragment: " << checker_program.getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
							   << checker_program.getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
							   << "Program: " << checker_program.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
			TCU_FAIL("Compile failed");
		}
	}

	buildTransformFeedbackProgram(vshader.c_str(), nullptr, fshader.c_str());

	/* total number of componenets*/
	GLuint numberOfComponents = 4 * 4 +				   /* values */
								1 + 2 + 3 + 4 + 1 + 2; /* skipped components */

	// clang-format off
	std::vector<GLfloat> vertices = {
		-1.0f, -1.0f, -1.0f, 1.0f,
		1.0f,  -1.0f, -2.0f, 1.0f,
		-1.0f, 1.0f,  -3.0f, 1.0f,

		1.0f,  1.0f,  4.0f, 1.0f,
		-1.0f, 1.0f,  5.0f, 1.0f,
		1.0f,  -1.0f, 6.0f, 1.0f,
	};

	/* expected values */
	GLfloat correctvalues[] = {
		-1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		-6.0f, -7.0f, -8.0f,
		2.0f, 2.0f, 2.0f, 2.0f,
		-13.0f, -14.0f, -15.0f, -16.0f, -17.0f,
		3.0f, 3.0f, 3.0f, 3.0f,
		-22.0f, -23.0f, -24.0f, -25.0f,
		4.0f, 4.0f, 4.0f, 4.0f,

		-30.0f,
		1.0f, 1.0f, 2.0f, 1.0f,
		-35.0f, -36.0f, -37.0f,
		2.0f, 2.0f, 4.0f, 2.0f,
		-42.0f, -43.0f, -44.0f, -45.0f, -46.0f,
		3.0f, 3.0f, 6.0f, 3.0f,
		-51.0f, -52.0f, -53.0f, -54.0f,
		4.0f, 4.0f, 8.0f, 4.0f,

		-59.0f,
		1.0f, 1.0f, 3.0f, 1.0f,
		-64.0f, -65.0f, -66.0f,
		2.0f, 2.0f, 6.0f, 2.0f,
		-71.0f, -72.0f, -73.0f, -74.0f, -75.0f,
		3.0f, 3.0f, 9.0f, 3.0f,
		-80.0f, -81.0f, -82.0f, -83.0f,
		4.0f, 4.0f, 12.0f, 4.0f,

		-88.0f,
		1.0f, 1.0f, 4.0f, 1.0f,
		-93.0f, -94.0f, -95.0f,
		2.0f, 2.0f, 8.0f, 2.0f,
		-100.0f, -101.0f, -102.0f, -103.0f, -104.0f,
		3.0f, 3.0f, 12.0f, 3.0f,
		-109.0f, -110.0f, -111.0f, -112.0f,
		4.0f, 4.0f, 16.0f, 4.0f,

		-117.0f,
		1.0f, 1.0f, 5.0f, 1.0f,
		-122.0f, -123.0f, -124.0f,
		2.0f, 2.0f, 10.0f, 2.0f,
		-129.0f, -130.0f, -131.0f, -132.0f, -133.0f,
		3.0f, 3.0f, 15.0f, 3.0f,
		-138.0f, -139.0f, -140.0f, -141.0f,
		4.0f, 4.0f, 20.0f, 4.0f,

		-146.0f,
		1.0f, 1.0f, 6.0f, 1.0f,
		-151.0f, -152.0f, -153.0f,
		2.0f, 2.0f, 12.0f, 2.0f,
		-158.0f, -159.0f, -160.0f, -161.0f, -162.0f,
		3.0f, 3.0f, 18.0f, 3.0f,
		-167.0f, -168.0f, -169.0f, -170.0f,
		4.0f, 4.0f, 24.0f, 4.0f,
	};
	// clang-format on

	createVertexBuffers(m_program, vertices);

	GLuint buffers[1];
	gl.genBuffers(1, buffers);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	createTransformBuffer(sizeof(GLfloat) * 4 * 6 * numberOfComponents, buffers[0], 0);

	std::vector<GLfloat> buffer_data(6 * numberOfComponents);

	for (GLuint i = 0; i < 6 * numberOfComponents; ++i)
	{
		buffer_data[i] = -1.0f - (GLfloat)i;
	}

	gl.bindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * numberOfComponents, buffer_data.data(), GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	{
		gl.enable(GL_RASTERIZER_DISCARD);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

		gl.clearColor(0.1f, 0.0f, 0.5f, 1.0f);
		GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

		gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

		gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffers[0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");

		gl.beginTransformFeedback(GL_TRIANGLES);
		GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");

		gl.drawArrays(GL_TRIANGLES, 0, 6);
		GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

		gl.endTransformFeedback();
		GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

		gl.disable(GL_RASTERIZER_DISCARD);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
	}

	std::vector<char> data;
	if (!readBuffer(sizeof(GLfloat) * 6 * numberOfComponents, buffers[0], data))
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Reading result buffer failed!" << tcu::TestLog::EndMessage;
		ret = false;
	}

	if (ret)
	{
		if (!compareArrays((GLfloat*)data.data(), sizeof(correctvalues) / sizeof(GLfloat), correctvalues,
						   sizeof(correctvalues) / sizeof(GLfloat)))
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Result comparison failed!" << tcu::TestLog::EndMessage;
			ret = false;
		}
	}

	gl.deleteBuffers(1, buffers);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	releaseVertexBuffers();

	if (ret)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	return STOP;
}

/*-------------------------------------------------------------------*/
/** Constructor.
*
*  @param context     Rendering context
*/
TransformFeedbackSkipMultipleBuffersTestCase::TransformFeedbackSkipMultipleBuffersTestCase(deqp::Context& context)
	: TransformFeedbackBaseTestCase(context, "skip_multiple_buffers",
									"Verifies functionality of skipping whole transform feedback buffer")
{
}

/* The varying name list contains all outputs in order. */
const char** TransformFeedbackSkipMultipleBuffersTestCase::varyings()
{
	static const char* vars[] = {
		"gl_SkipComponents1", "value1",
		"gl_NextBuffer",
		"gl_SkipComponents2", "value2",
		"gl_NextBuffer",
		"gl_SkipComponents3", "value3",
		"gl_NextBuffer",
		"gl_SkipComponents4", "value4",
	};

	return vars;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult TransformFeedbackSkipMultipleBuffersTestCase::iterate()
{
	if (!m_testSupported)
	{
		throw tcu::NotSupportedError("Test skip_multiple_buffers is not supported");
		return STOP;
	}

	bool				  ret = true;
	const glw::Functions& gl  = m_context.getRenderContext().getFunctions();

	/* setup shader program */
	std::string vshader = tcu::StringTemplate(m_shader_vert).specialize(specializationMap);
	std::string fshader = tcu::StringTemplate(m_shader_frag).specialize(specializationMap);

	{
		ProgramSources sources = makeVtxFragSources(vshader, fshader);

		ShaderProgram checker_program(gl, sources);

		if (!checker_program.isOk())
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
							   << "Vertex: " << checker_program.getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
							   << checker_program.getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
							   << "Fragment: " << checker_program.getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
							   << checker_program.getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
							   << "Program: " << checker_program.getProgramInfo().infoLog << tcu::TestLog::EndMessage;
			TCU_FAIL("Compile failed");
		}
	}

	buildTransformFeedbackProgram(vshader.c_str(), nullptr, fshader.c_str());

	// clang-format off
	std::vector<GLfloat> vertices = {
		-1.0f, -1.0f, -1.0f, 1.0f,
		1.0f,  -1.0f, -2.0f, 1.0f,
		-1.0f, 1.0f,  -3.0f, 1.0f,

		1.0f,  1.0f,  4.0f, 1.0f,
		-1.0f, 1.0f,  5.0f, 1.0f,
		1.0f,  -1.0f, 6.0f, 1.0f,
	};

	/* expected values */
	GLfloat correctvalues0[] = {
		-100.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		-105.0f, 1.0f, 1.0f, 2.0f, 1.0f,
		-110.0f, 1.0f, 1.0f, 3.0f, 1.0f,
		-115.0f, 1.0f, 1.0f, 4.0f, 1.0f,
		-120.0f, 1.0f, 1.0f, 5.0f, 1.0f,
		-125.0f, 1.0f, 1.0f, 6.0f, 1.0f,
	};

	GLfloat correctvalues1[] = {
		-100.0f, -101.0f, 2.0f, 2.0f, 2.0f, 2.0f,
		-106.0f, -107.0f, 2.0f, 2.0f, 4.0f, 2.0f,
		-112.0f, -113.0f, 2.0f, 2.0f, 6.0f, 2.0f,
		-118.0f, -119.0f, 2.0f, 2.0f, 8.0f, 2.0f,
		-124.0f, -125.0f, 2.0f, 2.0f, 10.0f, 2.0f,
		-130.0f, -131.0f, 2.0f, 2.0f, 12.0f, 2.0f,
	};

	GLfloat correctvalues2[] = {
		-100.0f, -101.0f, -102.0f, 3.0f, 3.0f, 3.0f, 3.0f,
		-107.0f, -108.0f, -109.0f, 3.0f, 3.0f, 6.0f, 3.0f,
		-114.0f, -115.0f, -116.0f, 3.0f, 3.0f, 9.0f, 3.0f,
		-121.0f, -122.0f, -123.0f, 3.0f, 3.0f, 12.0f, 3.0f,
		-128.0f, -129.0f, -130.0f, 3.0f, 3.0f, 15.0f, 3.0f,
		-135.0f, -136.0f, -137.0f, 3.0f, 3.0f, 18.0f, 3.0f,
	};

	GLfloat correctvalues3[] = {
		-100.0f, -101.0f, -102.0f, -103.0f, 4.0f, 4.0f, 4.0f, 4.0f,
		-108.0f, -109.0f, -110.0f, -111.0f, 4.0f, 4.0f, 8.0f, 4.0f,
		-116.0f, -117.0f, -118.0f, -119.0f, 4.0f, 4.0f, 12.0f, 4.0f,
		-124.0f, -125.0f, -126.0f, -127.0f, 4.0f, 4.0f, 16.0f, 4.0f,
		-132.0f, -133.0f, -134.0f, -135.0f, 4.0f, 4.0f, 20.0f, 4.0f,
		-140.0f, -141.0f, -142.0f, -143.0f, 4.0f, 4.0f, 24.0f, 4.0f,
	};
	// clang-format on

	createVertexBuffers(m_program, vertices);

	enumTypeTest typeTests[] = { { GL_FLOAT, sizeof(GLfloat), 5, 6, (void*)&correctvalues0 },
								 { GL_FLOAT, sizeof(GLfloat), 6, 6, (void*)&correctvalues1 },
								 { GL_FLOAT, sizeof(GLfloat), 7, 6, (void*)&correctvalues2 },
								 { GL_FLOAT, sizeof(GLfloat), 8, 6, (void*)&correctvalues3 } };

	std::vector<GLfloat> buffer_data(6*12);

	for (int i = 0; i < 6 * 12; ++i)
	{
		buffer_data[i] = -100.0f - (GLfloat)i;
	}

	GLuint buffers[4] = {0,0,0,0};
	gl.genBuffers(4, buffers);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	for (int i = 0; i < 4; ++i)
	{
		int bytecount = typeTests[i].bytesize * typeTests[i].w * typeTests[i].h;

		createTransformBuffer(bytecount, buffers[i], 0);

		gl.bindBuffer(GL_ARRAY_BUFFER, buffers[i]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.bufferData(GL_ARRAY_BUFFER, bytecount, buffer_data.data(), GL_STATIC_DRAW);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

		gl.bindBuffer(GL_ARRAY_BUFFER, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
	}

	{
		gl.enable(GL_RASTERIZER_DISCARD);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

		gl.clearColor(0.1f, 0.0f, 0.5f, 1.0f);
		GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

		gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

		for(int i = 0; i < 4; ++i)
		{
			gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, i, buffers[i]);
			GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
		}

		gl.beginTransformFeedback(GL_TRIANGLES);
		GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");

		gl.drawArrays(GL_TRIANGLES, 0, 6);
		GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

		gl.endTransformFeedback();
		GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

		gl.disable(GL_RASTERIZER_DISCARD);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disable");
	}

	std::vector<char> data;
	for (int i = 0; i < 4; ++i)
	{
		struct enumTypeTest* t = &typeTests[i];

		if (!readBuffer(t->w * t->h * t->bytesize, buffers[i], data))
		{
			ret = false;
			m_testCtx.getLog() << tcu::TestLog::Message << "Reading result buffer[ " << i << "] failed!"
							   << tcu::TestLog::EndMessage;
			break;
		}

		if (!compareArrays((GLfloat*)data.data(), t->w * t->h, (const GLfloat*)t->values, t->w * t->h))
		{
			ret = false;
			m_testCtx.getLog() << tcu::TestLog::Message << "Result comparison at buffer index " << i << " failed!"
							   << tcu::TestLog::EndMessage;
			break;
		}
	}

	gl.deleteBuffers(4, buffers);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	releaseVertexBuffers();

	if (ret)
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	else
		m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
TransformFeedback3Tests::TransformFeedback3Tests(deqp::Context& context)
	: TestCaseGroup(context, "transform_feedback3", "Verify conformance of ARB_transform_feedback3 functionality")
{
}

/** Initializes the test group contents. */
void TransformFeedback3Tests::init()
{
	addChild(new TransformFeedbackMultipleStreamsTestCase(m_context));
	addChild(new TransformFeedbackSkipComponentsTestCase(m_context));
	addChild(new TransformFeedbackSkipMultipleBuffersTestCase(m_context));
}

} // namespace gl3cts
