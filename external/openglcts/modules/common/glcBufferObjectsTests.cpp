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
 * \file  glcBufferObjectsTests.cpp
 * \brief Conformance tests for general buffer objects functionality.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"

#include "glcBufferObjectsTests.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "gluStateReset.hpp"
#include "glwEnums.hpp"
#include "gluContextInfo.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include <cstring>

using namespace glw;
using namespace glu;

namespace
{
constexpr GLuint BUFOBJ_TEXTURES = 2;

enum {
    BUFFER_TRIANGLES = 0,
    BUFFER_TRI_NORMALS,
    BUFFER_TRI_COLORS,
    BUFFER_ELEMENT_VERTICES,
    BUFFER_ELEMENT_INDICES,
    BUFFER_ELEMENT_COLORS,
    BUFFER_TEXTURE0,
    BUFFER_TEXTURE1,
    BUFFER_LAST_ENUM
};

// clang-format off
const GLfloat gTriNormalArray[] = {
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f
};

const GLfloat gTriColorArray[] = {
	1.0f, 0.0f, 0.0f, 1.0f,
	1.0f, 0.0f, 0.0f, 1.0f,
	1.0f, 0.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f
};

const GLfloat gTriTexCoordArray[] = {
	0.0f, 0.0f,
	1.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 1.0f
};

#define CHECKER2_REPEATS 4.0f
const GLfloat gTriTexCoordArray2[] = {
    0.0f, 0.0f,
    CHECKER2_REPEATS, 0.0f,
    CHECKER2_REPEATS, CHECKER2_REPEATS,
    0.0f, 0.0f,
    CHECKER2_REPEATS, CHECKER2_REPEATS,
    0.0f, CHECKER2_REPEATS
};

const GLfloat gElementColors[] = {
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f
};

const GLushort gElementIndices[] = { 0, 1, 5, 2, 3, 1 };

const GLushort gElementIndexSubData[] = { 3, 4, 5 };

const GLubyte gCheckerTextureData[] = {
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff
};

const GLubyte gChecker2TextureData[] = {
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff
};
// clang-format on

template <typename T>
void makeOrtho2DMatrix(T & mat, GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    if ((r - l)==0.f || (t - b)==0.f || (f - n)==0.f)
        return;

    GLfloat inv_width = 1.0f / (r - l);
    GLfloat inv_height = 1.0f / (t - b);
    GLfloat inv_depth = 1.0f / (f - n);

    mat.assign(mat.size(), 0);

    mat[0]  = 2.0f * inv_width;
    mat[5]  = 2.0f * inv_height;
    mat[10] = 2.0f * inv_depth;

    mat[12] = -(r + l) * inv_width;
    mat[13] = -(t + b) * inv_height;
    mat[14] = -(f + n) * inv_depth;
    mat[15] = 1.0f;
}

void ReadScreen(const glw::Functions& gl, GLint x, GLint y, GLsizei w, GLsizei h, GLenum type, GLubyte* buf)
{
	long repeat = 1;

	switch (type)
	{
	case GL_ALPHA:
	case GL_LUMINANCE:
		repeat = 1;
		break;
	case GL_LUMINANCE_ALPHA:
		repeat = 2;
		break;
	case GL_RGB:
		repeat = 3;
		break;
	case GL_RGBA:
	case GL_BGRA_EXT:
		repeat = 4;
		break;
	}

	memset(buf, 0, sizeof(GLubyte) * w * h * repeat);

	gl.pixelStorei(GL_PACK_ALIGNMENT, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "pixelStorei");

	gl.readPixels(x, y, w, h, type, GL_UNSIGNED_BYTE, (void*)buf);
	GLU_EXPECT_NO_ERROR(gl.getError(), "readPixels");
}

template <typename... Args>
inline void tcu_fail_msg(const std::string& src, const std::string& format, Args... args)
{
	std::string dst		 = src + format;
	int			str_size = std::snprintf(nullptr, 0, dst.c_str(), args...) + 1;
	if (str_size <= 0)
		throw std::runtime_error("Formatting error.");
	size_t					s = static_cast<size_t>(str_size);
	std::unique_ptr<char[]> buffer(new char[s]);
	std::snprintf(buffer.get(), s, dst.c_str(), args...);
	TCU_FAIL(buffer.get());
}

} // namespace

namespace glcts
{

/** Constructor.
 *
 *  @param context     Rendering context
 *  @param name        Name of the test
 *  @param desc        Test description
 */
BufferObjectsTestBase::BufferObjectsTestBase(deqp::Context& context, const char* name, const char* desc)
	: TestCase(context, name, desc)
	, m_isContextES(false)
	, m_isExtensionSupported(false)
	, m_buildBuffers(true)
	, m_matProjection(16)
	, m_window_size{0,0}
	, m_buffers(BUFFER_LAST_ENUM)
	, m_textures(BUFOBJ_TEXTURES)
	, m_vao(0)
{
	// clang-format off
	/** @brief default vertex shader source code for buffer objects functionality. */
	m_shader_vert =
		R"(${VERSION}
		uniform mat4 uModelViewProjectionMatrix;
		in vec4 inColor;
		in vec4 inVertex;
		out vec4 color;

		void main (void)
		{
			color = inColor;
			gl_Position = uModelViewProjectionMatrix * inVertex;
			gl_PointSize = 1.0;
		}
		)";

	/** @brief default fragment shader source code for buffer objects functionality. */
	m_shader_frag = R"(
		${VERSION}
		${PRECISION}

		in vec4 color;
		out vec4 fragColor;

		void main (void)
		{
			fragColor = color;
		}
		)";
	// clang-format on
}

/** Stub deinit method. */
void BufferObjectsTestBase::deinit()
{
	if(m_buildBuffers)
	{
		release_buffers();
	}
}

/** Stub init method */
void BufferObjectsTestBase::init()
{
	glu::resetState(m_context.getRenderContext(), m_context.getContextInfo());

	const glu::RenderContext& renderContext = m_context.getRenderContext();
	glu::GLSLVersion		  glslVersion	= glu::getContextTypeGLSLVersion(renderContext.getType());
	m_isContextES							= glu::isContextTypeES(renderContext.getType());

	specializationMap["VERSION"]   = glu::getGLSLVersionDeclaration(glslVersion);
	specializationMap["PRECISION"] = "";

	auto contextType = m_context.getRenderContext().getType();
	if (m_isContextES)
	{
		specializationMap["PRECISION"] = "precision highp float;";
	}
	else
	{
		m_isExtensionSupported = m_context.getContextInfo().isExtensionSupported("GL_ARB_ES2_compatibility") ||
								 glu::contextSupports(contextType, glu::ApiType::core(3, 0));
	}

	if (!m_shader_vert.empty() && !m_shader_frag.empty())
	{
		const glw::Functions& gl	  = m_context.getRenderContext().getFunctions();
		std::string			  vshader = tcu::StringTemplate(m_shader_vert).specialize(specializationMap);
		std::string			  fshader = tcu::StringTemplate(m_shader_frag).specialize(specializationMap);

		ProgramSources sources = makeVtxFragSources(vshader, fshader);

		m_program = std::make_unique<ShaderProgram>(gl, sources);

		if (!m_program->isOk())
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Shader build failed.\n"
							   << "Vertex: " << m_program->getShaderInfo(SHADERTYPE_VERTEX).infoLog << "\n"
							   << m_program->getShader(SHADERTYPE_VERTEX)->getSource() << "\n"
							   << "Fragment: " << m_program->getShaderInfo(SHADERTYPE_FRAGMENT).infoLog << "\n"
							   << m_program->getShader(SHADERTYPE_FRAGMENT)->getSource() << "\n"
							   << "Program: " << m_program->getProgramInfo().infoLog << tcu::TestLog::EndMessage;
			TCU_FAIL("Compile failed");
		}
	}

	m_window_size[0] = m_context.getRenderTarget().getWidth();
	m_window_size[1] = m_context.getRenderTarget().getHeight();

	GLfloat windowWidth	 = m_window_size[0];
	GLfloat windowHeight = m_window_size[1];

	m_triVertexArray[0]	= windowWidth / 4.0f;
	m_triVertexArray[1]	= windowHeight / 4.0f;
	m_triVertexArray[2]	= 3.0f * windowWidth / 4.0f;
	m_triVertexArray[3]	= windowHeight / 4.0f;
	m_triVertexArray[4]	= 3.0f * windowWidth / 4.0f;
	m_triVertexArray[5]	= 3.0f * windowHeight / 4.0f;
	m_triVertexArray[6]	= windowWidth / 4.0f;
	m_triVertexArray[7]	= windowHeight / 4.0f;
	m_triVertexArray[8]	= 3.0f * windowWidth / 4.0f;
	m_triVertexArray[9]	= 3.0f * windowHeight / 4.0f;
	m_triVertexArray[10] = windowWidth / 4.0f;
	m_triVertexArray[11] = 3.0f * windowHeight / 4.0f;

	m_triSubDataVertexArray[0] = windowWidth / 2.0f;
	m_triSubDataVertexArray[1] = windowHeight / 2.0f;
	m_triSubDataVertexArray[2] = 3.0f * windowWidth / 4.0f;
	m_triSubDataVertexArray[3] = 3.0f * windowHeight / 4.0f;
	m_triSubDataVertexArray[4] = windowWidth / 4.0f;
	m_triSubDataVertexArray[5] = 3.0f * windowHeight / 4.0f;

	m_pointVertices[0] = windowWidth / 4.0f;
	m_pointVertices[1] = 3.0f * windowHeight / 4.0f;
	m_pointVertices[2] = 3.0f * windowWidth / 4.0f;
	m_pointVertices[3] = 3.0f * windowHeight / 4.0f;
	m_pointVertices[4] = windowWidth / 4.0f;
	m_pointVertices[5] = windowHeight / 4.0f;
	m_pointVertices[6] = 3.0f * windowWidth / 4.0f;
	m_pointVertices[7] = windowHeight / 4.0f;

	m_elementVertices[0]	 = windowWidth / 4.0f;
	m_elementVertices[1]	 = windowHeight / 4.0f;
	m_elementVertices[2]	 = windowWidth / 2.0f;
	m_elementVertices[3]	 = windowHeight / 2.0f;
	m_elementVertices[4]	 = 3.0f * windowWidth / 4.0f;
	m_elementVertices[5]	 = windowHeight / 4.0f;
	m_elementVertices[6]	 = 3.0f * windowWidth / 4.0f;
	m_elementVertices[7]	 = 3.0f * windowHeight / 4.0f;
	m_elementVertices[8]	 = windowWidth / 2.0f;
	m_elementVertices[9]	 = windowHeight / 2.0f;
	m_elementVertices[10] = windowWidth / 4.0f;
	m_elementVertices[11] = 3.0f * windowHeight / 4.0f;

	if (m_buildBuffers && !build_buffers())
	{
		TCU_FAIL("Buffer objects creation failed");
	}
}

/** Prepare rendering resources for the test */
bool BufferObjectsTestBase::build_buffers()
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

	makeOrtho2DMatrix(m_matProjection, 0.0f, (GLfloat)m_window_size[0], 0.0f, (GLfloat)m_window_size[1], 1.0f, -1.0f);

	gl.genVertexArrays(1, &m_vao);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
	gl.bindVertexArray(m_vao);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

	for (int i = 0; i < BUFFER_LAST_ENUM; i++)
		m_buffers[i] = 0;

	gl.genBuffers(BUFFER_LAST_ENUM, m_buffers.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	/* Separate triangles */
	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRIANGLES]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	size_t verts_sizeof = sizeof(decltype(m_triVertexArray)::value_type) * m_triVertexArray.size();

	/* Test that NULL pointer doesn't crash and sets buffer to expected size */
	gl.bufferData(GL_ARRAY_BUFFER, verts_sizeof, 0, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	int bufSize = 0;

	gl.getBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bufSize);
	GLU_EXPECT_NO_ERROR(gl.getError(), "getBufferParameteriv");

	if (size_t(bufSize) != verts_sizeof)
		TCU_FAIL("BufferObjectsTestBase::build_buffers: Failed to create buffer store of correct size.");

	/* Now store the real data */
	gl.bufferData(GL_ARRAY_BUFFER, verts_sizeof, m_triVertexArray.data(), GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* Triangle normals */
	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRI_NORMALS]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(gTriNormalArray), gTriNormalArray, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* Triangle colors */
	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRI_COLORS]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(gTriColorArray), gTriColorArray, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* --- */

	/* Texture coordinate array #1 */
	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TEXTURE0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(gTriTexCoordArray), gTriTexCoordArray, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* Texture coordinate array #2 */
	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TEXTURE1]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(gTriTexCoordArray2), gTriTexCoordArray2, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* --- */

	/* Vertices for the DrawElements calls */
	size_t elems_sizeof = sizeof(decltype(m_elementVertices)::value_type) * m_elementVertices.size();

	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_ELEMENT_VERTICES]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, elems_sizeof, m_elementVertices.data(), GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* Colors for the DrawElements calls */
	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_ELEMENT_COLORS]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(gElementColors), gElementColors, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	/* Indices for the DrawElements calls */
	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffers[BUFFER_ELEMENT_INDICES]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(gElementIndices), gElementIndices, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	/* TEXTURES */

	gl.activeTexture(GL_TEXTURE0); /* TODO: Assumes this is always bound to 0 */
	GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

	gl.genTextures(BUFOBJ_TEXTURES, m_textures.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");

	gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

	gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, gCheckerTextureData);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	/* Modulate, will be used for lighting */

	gl.activeTexture(GL_TEXTURE1); /* TODO: Assumes this is always bound to 0 */
	GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

	gl.bindTexture(GL_TEXTURE_2D, m_textures[1]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

	gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, gChecker2TextureData);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri");

	/*  Modulate, will be used to multiply the color values
		in a simple check for the multitexturing */

	gl.activeTexture(GL_TEXTURE0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

	return true;
}

/** Release rendering resources for the test */
bool BufferObjectsTestBase::release_buffers()
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.deleteBuffers(BUFFER_LAST_ENUM, m_buffers.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	gl.deleteTextures(BUFOBJ_TEXTURES, m_textures.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

	if (m_vao)
	{
		gl.deleteVertexArrays(1, &m_vao);
		GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");
		m_vao = 0;
	}

	return true;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult BufferObjectsTestBase::iterate()
{
	if (!m_isContextES)
	{
		if (!m_isExtensionSupported)
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
			/* This test should only be executed if we're running a GL2.0 context */
			throw tcu::NotSupportedError("GL_ARB_ES2_compatibility is not supported");
		}
	}

	return run_test();
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
BufferObjectsTestGenBuffersCase::BufferObjectsTestGenBuffersCase(deqp::Context& context) :
	BufferObjectsTestBase(context, "gen_buffers", "Test generation buffer objects functionality")
{
	// no shaders needed for this test
	m_shader_vert.clear();
	m_shader_frag.clear();

	m_buildBuffers = false;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult BufferObjectsTestGenBuffersCase::run_test()
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();
	const int N_TESTED_BUFS = 2;
	const GLint targets[] = { GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER };
	const GLint usages[]  = { GL_STATIC_DRAW, GL_DYNAMIC_DRAW };

	GLuint bufs[N_TESTED_BUFS];
	GLuint twinBufs[2] = { 0, 0 };
	GLuint dummy0	   = 0;
	GLuint buf		   = 128;

	memset(bufs, 0, N_TESTED_BUFS * sizeof(GLuint));

	gl.genBuffers(N_TESTED_BUFS, bufs);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	/* Test different targets and different binds */
	for (size_t usg = 0; usg < sizeof(usages) / sizeof(usages[0]); usg++) /* Loop both usage types */
	{
		for (size_t tg = 0; tg < sizeof(targets) / sizeof(targets[0]); tg++) /* Loop both target types */
		{
			for (int i = 0; i < N_TESTED_BUFS; i++)
			{
				gl.bindBuffer(targets[tg], bufs[i]);
				GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

				if (!gl.isBuffer(bufs[i]))
					TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: glIs buffer not functioning properly");
			}
			gl.bindBuffer(targets[tg], 0); /* Must free the target. */
			GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

			size_t vec_sizeof = sizeof(decltype(m_triVertexArray)::value_type) * m_triVertexArray.size();

			/* Test that the data cannot be set: gl.bufferData */
			gl.bufferData(targets[tg], vec_sizeof, m_triVertexArray.data(), usages[usg]);
			if (gl.getError() == GL_NO_ERROR)
				TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: glBufferData not returning failure state.");

			/* glBufferSubData */
			gl.bufferSubData(targets[tg], 0, vec_sizeof, m_triVertexArray.data());
			if (gl.getError() == GL_NO_ERROR)
				TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: glBufferSubData not returning failure state.");
		}
	}

	gl.deleteBuffers(N_TESTED_BUFS, bufs);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	/* Test the case, where object is still bound when trying to delete */
	gl.genBuffers(2, twinBufs);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	gl.bindBuffer(GL_ARRAY_BUFFER, twinBufs[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, twinBufs[1]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.deleteBuffers(N_TESTED_BUFS, twinBufs);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.deleteBuffers(N_TESTED_BUFS, bufs);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	gl.genBuffers(-1, bufs);
	if (gl.getError() == GL_NO_ERROR)
		TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: No error on invalid number of generated buffers.");

	gl.deleteBuffers(-1, bufs);
	if (gl.getError() == GL_NO_ERROR)
		TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: No error on invalid number of generated buffers.");

	/* Test "0", should be a NOP */
	gl.deleteBuffers(1, &dummy0); /* Quiet ignore */
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	/* Test the case with binding buffer without generating it first */
	auto contextType = m_context.getRenderContext().getType();
	for (size_t tg = 0; tg < sizeof(targets) / sizeof(targets[0]); tg++) /* Loop both target types */
	{
		gl.bindBuffer(targets[tg], buf);

		if (m_isContextES)
		{
			if (gl.getError() != GL_NO_ERROR)
			{
				TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: Error when binding not generated buffer");
			}
			else
			{
				gl.deleteBuffers(1, &buf);
				GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
			}
		}
		else if (glu::contextSupports(contextType, glu::ApiType::core(3, 1)))
		{
			if (gl.getError() != GL_INVALID_OPERATION)
				TCU_FAIL("BufferObjectsTestGenBuffersCase::run_test: No error when binding not generated buffer");
		}
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
BufferObjectsTestTrianglesCase::BufferObjectsTestTrianglesCase(deqp::Context& context) :
	BufferObjectsTestBase(context, "triangles", "Test triangle rendering with buffer objects functionality")
{
	// clang-format off
	/** @brief vertex shader source code for buffer objects functionality. */
	m_shader_vert =
		R"(${VERSION}

		in vec4 inColor;
		in vec4 inVertex;
		in vec3 inNormal;
		in vec4 inMultiTexCoord0;

		uniform mat4 uModelViewProjectionMatrix;
		uniform mat3 uNormalMatrix;

		out vec4 texCoord[1];
		out vec4 color;

		vec4 Ambient;
		vec4 Diffuse;
		vec4 Specular;

		const vec3 lightPosition = vec3(0.0, 0.0, 1.0);
		const vec3 spotDirection = vec3(0.0, 0.0, -1.0);
		const float spotCutoff = 180.0;
		const float spotExponent = 0.0;

		const float lightAttenuationConstant = 1.0;
		const float lightAttenuationLinear = 0.0;
		const float lightAttenuationQuadratic = 0.0;

		const vec4 lightAmbient = vec4(0.0, 0.0, 0.0, 0.0);
		vec4 lightDiffuse = vec4(1.0, 1.0, 1.0, 1.0);
		vec4 lightSpecular = vec4(1.0, 1.0, 1.0, 1.0);

		const float materialShininess = 0.0;

		const vec4 sceneColor = vec4(0.0, 0.0, 0.0, 0.0);

		void spotLight(in int i, in vec3 normal, in vec3 eye, in vec3 ecPosition3)
		{
			float nDotVP;           // normal . light direction
			float nDotHV;           // normal . light half vector
			float pf;               // power factor
			float spotDot;          // cosine of angle between spotlight
			float spotAttenuation;  // spotlight attenuation factor
			float attenuation;      // computed attenuation factor
			float d;                // distance from surface to light source
			vec3 VP;                // direction from surface to light position
			vec3 halfVector;        // direction of maximum highlights

			// Compute vector from surface to light position
			VP = lightPosition - ecPosition3;

			// Compute distance between surface and light position
			d = length(VP);

			// Normalize the vector from surface to light position
			VP = normalize(VP);

			// Compute attenuation
			attenuation = 1.0 / (lightAttenuationConstant +
			lightAttenuationLinear * d +
			lightAttenuationQuadratic * d * d);

			// See if point on surface is inside cone of illumination
			spotDot = dot(-VP, normalize(spotDirection));

			if (spotDot < cos(radians(spotCutoff)))
				spotAttenuation = 0.0; // light adds no contribution
			else
				spotAttenuation = pow(spotDot, spotExponent);

			// Combine the spotlight and distance attenuation.
			attenuation *= spotAttenuation;

			halfVector = normalize(VP + eye);

			nDotVP = max(0.0, dot(normal, VP));
			nDotHV = max(0.0, dot(normal, halfVector));

			if (nDotVP == 0.0)
				pf = 0.0;
			else
				pf = pow(nDotHV, materialShininess);

			Ambient  += lightAmbient * attenuation;
			Diffuse  += lightDiffuse * nDotVP * attenuation;
			Specular += lightSpecular * pf * attenuation;
		}

		vec3 fnormal(void)
		{
			//Compute the normal
			vec3 normal = uNormalMatrix * inNormal;
			normal = normalize(normal);

			return normal;
		}

		void flight(in vec3 normal, in vec4 ecPosition, float alphaFade)
		{
			vec3 ecPosition3;
			vec3 eye;

			ecPosition3 = (vec3 (ecPosition)) / ecPosition.w;
			eye = vec3 (0.0, 0.0, 1.0);

			// Clear the light intensity accumulators
			Ambient  = vec4 (0.0);
			Diffuse  = vec4 (0.0);
			Specular = vec4 (0.0);

			spotLight(0, normal, eye, ecPosition3);

			color = sceneColor +
			Ambient  * inColor +
			Diffuse  * inColor;
			color += Specular * inColor;
			color = clamp( color, 0.0, 1.0 );

			color.a *= alphaFade;
		}

		void main (void)
		{
			vec3  transformedNormal;
			float alphaFade = 1.0;

			vec4 ecPosition = inVertex;

			color = inColor;
			texCoord[0] = inMultiTexCoord0;
			gl_Position = uModelViewProjectionMatrix * inVertex;
			transformedNormal = fnormal();
			flight(transformedNormal, ecPosition, alphaFade);
		}
		)";

	/** @brief fragment shader source code for buffer objects functionality. */
	m_shader_frag = R"(
		${VERSION}
		${PRECISION}

		uniform sampler2D uTexture0;
		in vec4 color;
		in vec4 texCoord[1];
		out vec4 fragColor;

		void main (void)
		{
			fragColor = texture(uTexture0, texCoord[0].st, 1.0) * color;
		}
		)";
	// clang-format on
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult BufferObjectsTestTrianglesCase::run_test()
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	GLint		 locVertices = 0, locColors = 0, locNormals = 0, locTexCoords = 0;
	GLfloat		 matNormal[9]	  = { 0.f };
	/* Test triangle rendering with normals, texture, colors */
	static const char elementsErrorFmt[] =
		"Incorrectly rasterized buffer object: expected [%.6f, %.6f, %.6f], got [%.6f, %.6f, %.6f]";
	GLubyte buf[4]	  = { 0 };
	GLubyte bufCmp[4] = { 0 };

	GLint windowWidth  = m_window_size[0];
	GLint windowHeight = m_window_size[1];

	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

	GLint locMatProjection = gl.getUniformLocation(m_program->getProgram(), "uModelViewProjectionMatrix");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locMatProjection != -1)
	{
		gl.uniformMatrix4fv(locMatProjection, 1, 0, m_matProjection.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");
	}

	GLint locMatNormal = gl.getUniformLocation(m_program->getProgram(), "uNormalMatrix");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locMatNormal != -1)
	{
		memset(matNormal, 0, sizeof(matNormal));
		matNormal[0] = matNormal[4] = matNormal[8] = 1.0f;

		gl.uniformMatrix3fv(locMatNormal, 1, GL_FALSE, matNormal);
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix3fv");
	}

	locVertices = gl.getAttribLocation(m_program->getProgram(), "inVertex");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	locColors = gl.getAttribLocation(m_program->getProgram(), "inColor");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	locNormals = gl.getAttribLocation(m_program->getProgram(), "inNormal");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	locTexCoords = gl.getAttribLocation(m_program->getProgram(), "inMultiTexCoord0");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	gl.enableVertexAttribArray(locVertices);
	GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

	gl.enableVertexAttribArray(locColors);
	GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

	gl.enableVertexAttribArray(locNormals);
	GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

	gl.enableVertexAttribArray(locTexCoords);
	GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

	GLint locTextures0 = gl.getUniformLocation(m_program->getProgram(), "uTexture0");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locTextures0 != -1)
	{
		gl.activeTexture(GL_TEXTURE0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

		gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

		gl.uniform1i(locTextures0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
	}

	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRI_NORMALS]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.vertexAttribPointer(locNormals, 3, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRI_COLORS]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.vertexAttribPointer(locColors, 4, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TEXTURE0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.vertexAttribPointer(locTexCoords, 2, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRIANGLES]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.vertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

	gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.drawArrays(GL_TRIANGLES, 0, 6);
	GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.disableVertexAttribArray(locVertices);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

	gl.disableVertexAttribArray(locColors);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

	gl.disableVertexAttribArray(locNormals);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

	gl.disableVertexAttribArray(locTexCoords);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

    /* See that:
       - The vertices are contained correctly and (at least) a triangle is rendered
       - The texture is applied correctly
       - The colors have been applied correctly.
    */
	ReadScreen(gl, windowWidth / 4 + 1, 3 * windowHeight / 4 - 1, 1, 1, GL_RGBA, buf);
	if ((GLfloat)fabs(buf[0] - 0.0f) > 8.0f || (GLfloat)fabs(buf[1] - 255.0f) > 8.0f ||
		(GLfloat)fabs(buf[2] - 0.0f) > 8.0f)
		tcu_fail_msg("BufferObjectsTestTrianglesCase::run_test: ", elementsErrorFmt, .0f, 1.0f, .0f, buf[0], buf[1],
					 buf[2]);

	ReadScreen(gl, 3 * windowWidth / 8 + 1, 3 * windowHeight / 8 + 1, 1, 1, GL_RGBA, buf);
	if ((GLfloat)fabs(buf[0] - 0.0f) > 8.0f || (GLfloat)fabs(buf[1] - 0.0f) > 8.0f ||
		(GLfloat)fabs(buf[2] - 0.0f) > 8.0f)
		tcu_fail_msg("BufferObjectsTestTrianglesCase::run_test: ", elementsErrorFmt, .0f, 1.0f, .0f, buf[0], buf[1],
					 buf[2]);

	/* See that the normals are applied correctly. */
	ReadScreen(gl, 3 * windowWidth / 4 - 1, windowHeight / 4 + 1, 1, 1, GL_RGBA, buf);
	ReadScreen(gl, 5 * windowWidth / 8, 3 * windowHeight / 8 + 1, 1, 1, GL_RGBA, bufCmp);
	if ((bufCmp[0] - buf[0]) < 2.0f * 8.0f || bufCmp[0] < 2.0f * 8.0f || (GLfloat)fabs(buf[1] - 0.0f) > 8.0f ||
		(GLfloat)fabs(buf[2] - 0.0f) > 8.0f)
		TCU_FAIL("BufferObjectsTestTrianglesCase::run_test: Buffer object incorrectly wraps a normal.");

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
BufferObjectsTestElementsCase::BufferObjectsTestElementsCase(deqp::Context& context)
	: BufferObjectsTestBase(context, "elements", "Test DrawElements on buffer objects functionality")
{
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult BufferObjectsTestElementsCase::run_test()
{
	/* Test DrawElements on buffer objects. */
	const char elementsErrorFmt[] =
		"Incorrectly rasterized buffer object: expected [%.6f, %.6f, %.6f], got [%.6f, %.6f, %.6f]";
	GLubyte buf[4] = { 0, 0, 0, 0 };

	GLfloat windowWidth	 = m_window_size[0];
	GLfloat windowHeight = m_window_size[1];

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

	// Render Code
	//--------------------------------------------------------
	GLint locMatProjection = gl.getUniformLocation(m_program->getProgram(), "uModelViewProjectionMatrix");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locMatProjection != -1)
	{
		gl.uniformMatrix4fv(locMatProjection, 1, 0, m_matProjection.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");
	}

	GLint locVertices = gl.getAttribLocation(m_program->getProgram(), "inVertex");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locVertices != -1)
	{
		gl.enableVertexAttribArray(locVertices);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_ELEMENT_VERTICES]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	GLint locColors = gl.getAttribLocation(m_program->getProgram(), "inColor");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locColors != -1)
	{
		gl.enableVertexAttribArray(locColors);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_ELEMENT_COLORS]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locColors, 4, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffers[BUFFER_ELEMENT_INDICES]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "drawElements");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	if (locVertices != -1)
	{
		gl.disableVertexAttribArray(locVertices);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	}

	if (locColors != -1)
	{
		gl.disableVertexAttribArray(locColors);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	}
	//--------------------------------------------------------

	/* Test four pixels from the pattern, two of them should be white, and two
	   black */
	ReadScreen(gl, windowWidth / 2, 3 * windowHeight / 4 - 1, 1, 1, GL_RGBA, buf);
	if (abs(buf[0] - 0) > 8 || abs(buf[1] - 0) > 8 || abs(buf[2] - 0) > 8)
		tcu_fail_msg("BufferObjectsTestElementsCase::run_test: ", elementsErrorFmt, .0f, .0f, .0f, buf[0], buf[1],
					 buf[2]);

	ReadScreen(gl, windowWidth / 2, windowHeight / 4, 1, 1, GL_RGBA, buf);
	if (abs(buf[0] - 0) > 8 || abs(buf[1] - 0) > 8 || abs(buf[2] - 0) > 8)
		tcu_fail_msg("BufferObjectsTestElementsCase::run_test: ", elementsErrorFmt, .0f, .0f, .0f, buf[0], buf[1],
					 buf[2]);

	ReadScreen(gl, windowWidth / 4 + 1, windowHeight / 2, 1, 1, GL_RGBA, buf);
	if (abs(buf[0] - 255) > 8 || abs(buf[1] - 255) > 8 || abs(buf[2] - 255) > 8)
		tcu_fail_msg("BufferObjectsTestElementsCase::run_test: ", elementsErrorFmt, 1.0f, 1.0f, 1.0f, buf[0], buf[1],
					 buf[2]);

	ReadScreen(gl, 3 * windowWidth / 4 - 1, windowHeight / 2, 1, 1, GL_RGBA, buf);
	if (abs(buf[0] - 255) > 8 || abs(buf[1] - 255) > 8 || abs(buf[2] - 255) > 8)
		tcu_fail_msg("BufferObjectsTestElementsCase::run_test: ", elementsErrorFmt, 1.0f, 1.0f, 1.0f, buf[0], buf[1],
					 buf[2]);

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
BufferObjectsTestMultiTexturingCase::BufferObjectsTestMultiTexturingCase(deqp::Context& context)
	: BufferObjectsTestBase(context, "multi_texture", "Test multi texturing on buffer objects functionality")
{
	// clang-format off
	/** @brief default vertex shader source code for buffer objects functionality. */
	m_shader_vert =
		R"(${VERSION}
		uniform mat4 uModelViewProjectionMatrix;

		in vec4 inColor;
		in vec4 inVertex;
		in vec4 inMultiTexCoord0;
		in vec4 inMultiTexCoord1;

		out vec4 color;
		out vec4 texCoord[2];

		void main (void)
		{
			color = inColor;
			texCoord[0] = inMultiTexCoord0;
			texCoord[1] = inMultiTexCoord1;
			gl_Position = uModelViewProjectionMatrix * inVertex;
		}
		)";

	/** @brief default fragment shader source code for buffer objects functionality. */
	m_shader_frag = R"(
		${VERSION}
		${PRECISION}

		uniform sampler2D uTexture0;
		uniform sampler2D uTexture1;

		in vec4 color;
		in vec4 texCoord[2];
		out vec4 fragColor;

		void main (void)
		{
			fragColor = texture(uTexture0, texCoord[0].st, 1.0);
			fragColor += texture(uTexture1, texCoord[1].st, 1.0);
		}
		)";
		// clang-format on
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult BufferObjectsTestMultiTexturingCase::run_test()
{
	GLfloat windowWidth	 = m_window_size[0];
	GLfloat windowHeight = m_window_size[1];

	std::vector<GLubyte> bufColors((windowWidth + 3) / 2 * 4);

	int prevPx	   = 0;
	int whiteCount = 0;
	int blackCount = 0;

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

	// Render Code
	//--------------------------------------------------------
	GLint locMatProjection = gl.getUniformLocation(m_program->getProgram(), "uModelViewProjectionMatrix");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locMatProjection != -1)
	{
		gl.uniformMatrix4fv(locMatProjection, 1, 0, m_matProjection.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");
	}

	GLint locVertices = gl.getAttribLocation(m_program->getProgram(), "inVertex");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locVertices != -1)
	{
		gl.enableVertexAttribArray(locVertices);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		/* setup the vertex buffer object. */
		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRIANGLES]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	GLint locColors = gl.getAttribLocation(m_program->getProgram(), "inColor");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locColors != -1)
	{
		gl.enableVertexAttribArray(locColors);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRI_COLORS]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locColors, 4, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	GLint locTexCoords0 = gl.getAttribLocation(m_program->getProgram(), "inMultiTexCoord0");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locTexCoords0 != -1)
	{
		gl.enableVertexAttribArray(locTexCoords0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TEXTURE0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locTexCoords0, 2, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	GLint locTexCoords1 = gl.getAttribLocation(m_program->getProgram(), "inMultiTexCoord1");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locTexCoords1 != -1)
	{
		gl.enableVertexAttribArray(locTexCoords1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TEXTURE1]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locTexCoords1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	/* ---  First multitexture unit: Plain multitexturing. */
	GLint locTextures0 = gl.getUniformLocation(m_program->getProgram(), "uTexture0");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locTextures0 != -1)
	{
		gl.activeTexture(GL_TEXTURE0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

		/*  Bind the buffer object containing the texture coords for the first
			checker texture. Set the ClientActiveTexture accordingly. Set the pointer.
		*/
		gl.bindTexture(GL_TEXTURE_2D, m_textures[0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

		gl.uniform1i(locTextures0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
	}

	/*  Same for the second texturing unit. */
	GLint locTextures1 = gl.getUniformLocation(m_program->getProgram(), "uTexture1");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locTextures1 != -1)
	{
		gl.activeTexture(GL_TEXTURE1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

		gl.bindTexture(GL_TEXTURE_2D, m_textures[1]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

		gl.uniform1i(locTextures1, 1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniform1i");
	}

	gl.drawArrays(GL_TRIANGLES, 0, 6);
	GLU_EXPECT_NO_ERROR(gl.getError(), "drawArrays");

	/* Reset the state */
	gl.activeTexture(GL_TEXTURE1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

	gl.bindTexture(GL_TEXTURE_2D, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

	gl.activeTexture(GL_TEXTURE0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

	gl.bindTexture(GL_TEXTURE_2D, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	if (locVertices != -1)
	{
		gl.disableVertexAttribArray(locVertices);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	}

	if (locColors != -1)
	{
		gl.disableVertexAttribArray(locColors);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	}

	if (locTexCoords0 != -1)
	{
		gl.disableVertexAttribArray(locTexCoords0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	}

	if (locTexCoords1 != -1)
	{
		gl.disableVertexAttribArray(locTexCoords1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	}
	//--------------------------------------------------------

	/* FIXME: This test fails if the width is an odd number */
	ReadScreen(gl, windowWidth / 4 + 1, 3 * windowHeight / 4 - 3, (windowWidth + 3) / 2, 1, GL_RGBA,
			   (GLubyte*)bufColors.data());

	prevPx = 1; /* We should start from a black pixel */
	for (int i = 0; i < (windowWidth + 3) / 2; i++)
	{
		if (abs(bufColors[i * 4 + 0] - 0) < 8 && prevPx != 0)
		{
			prevPx = 0;
			blackCount++;
		}
		if (abs(bufColors[i * 4 + 0] - 255) < 8 && prevPx != 1)
		{
			prevPx = 1;
			whiteCount++;
		}
	}

	if (blackCount != 3 || whiteCount != 2)
	{
		TCU_FAIL("BufferObjectsTestMultiTexturingCase::run_test: Multitexturing with buffer objects failed.");
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context     Rendering context
 */
BufferObjectsTestSubDataCase::BufferObjectsTestSubDataCase(deqp::Context& context)
	: BufferObjectsTestBase(context, "sub_data", "Test buffering of sub data functionality")
{
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult BufferObjectsTestSubDataCase::run_test()
{
	GLfloat windowWidth	 = m_window_size[0];
	GLfloat windowHeight = m_window_size[1];

	GLubyte buf[4]	   = { 0, 0, 0, 0 };
	GLuint	tempObject = 0;

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "clear");

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

	// Render Code
	//--------------------------------------------------------
	GLint locMatProjection = gl.getUniformLocation(m_program->getProgram(), "uModelViewProjectionMatrix");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformLocation");

	if (locMatProjection != -1)
	{
		gl.uniformMatrix4fv(locMatProjection, 1, 0, m_matProjection.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "uniformMatrix4fv");
	}

	GLint locVertices = gl.getAttribLocation(m_program->getProgram(), "inVertex");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locVertices != -1)
	{
		gl.enableVertexAttribArray(locVertices);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		/* setup the vertex buffer object. */
		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRIANGLES]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	GLint locColors = gl.getAttribLocation(m_program->getProgram(), "inColor");
	GLU_EXPECT_NO_ERROR(gl.getError(), "getAttribLocation");

	if (locColors != -1)
	{
		gl.enableVertexAttribArray(locColors);
		GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");

		gl.bindBuffer(GL_ARRAY_BUFFER, m_buffers[BUFFER_TRI_COLORS]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

		gl.vertexAttribPointer(locColors, 4, GL_FLOAT, GL_FALSE, 0, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
	}

	gl.genBuffers(1, &tempObject);
	GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, tempObject);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(gElementIndices), gElementIndices, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

	gl.bufferSubData(GL_ELEMENT_ARRAY_BUFFER, 3 * sizeof(GLushort), 3 * sizeof(GLushort), gElementIndexSubData);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bufferSubData");

	gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "drawElements");

	gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

	gl.deleteBuffers(1, &tempObject);
	GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

	gl.disableVertexAttribArray(locVertices);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

	gl.disableVertexAttribArray(locColors);
	GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");
	//--------------------------------------------------------

	/* Some ASCII art. If the glBufferSubData call above took effect, we expect the framebuffer to look like this:

	   +--------------+
	   |              |
	   |   +------.   |
	   |   |  1  /    |
	   |   |    /     |
	   |   |   / 2    |  <-- The digits represent the pixels we read below.
	   |   |  / \     |
	   |   | /   \    |
	   |   |/_____\   |
	   |              |
	   +--------------+

	   If the glBufferSubData call above did not actually do anything, then the geometry will look like this instead:

	   +--------------+
	   |              |
	   |   .      .   |
	   |   |\  1 /|   |
	   |   | \  / |   |
	   |   |  \/ 2|   |  <-- The digits represent the pixels we read below.
	   |   |  /   |   |
	   |   | /    |   |
	   |   |/_____|   |
	   |              |
	   +--------------+

	*/

	auto IsBlack = [](GLubyte* b) { return !b[0] && !b[1] && !b[2]; };

	/* The sample is #1. It must not be black */
	ReadScreen(gl, windowWidth / 2, 2 * windowHeight / 3, 1, 1, GL_RGBA, buf);
	if (IsBlack(buf))
	{
		TCU_FAIL("BufferObjectsTestSubDataCase::run_test: BufferSubData did not replace buffer object data correctly "
				 "(not black).");
	}

	/* The sample is #2. It must be black */
	ReadScreen(gl, 2 * windowWidth / 3, windowHeight / 2, 1, 1, GL_RGBA, buf);
	if (!IsBlack(buf))
	{
		TCU_FAIL("BufferObjectsTestSubDataCase::run_test: BufferSubData did not replace buffer object data correctly "
				 "(not black).");
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
BufferObjectsTests::BufferObjectsTests(deqp::Context& context)
	: TestCaseGroup(context, "buffer_objects", "Test ES2 compatibility with buffer objects functionality")
{
}

/** Initializes the test group contents. */
void BufferObjectsTests::init()
{
	addChild(new BufferObjectsTestGenBuffersCase(m_context));
	addChild(new BufferObjectsTestTrianglesCase(m_context));
	addChild(new BufferObjectsTestElementsCase(m_context));
	addChild(new BufferObjectsTestMultiTexturingCase(m_context));
	addChild(new BufferObjectsTestSubDataCase(m_context));
}

} // namespace glcts
