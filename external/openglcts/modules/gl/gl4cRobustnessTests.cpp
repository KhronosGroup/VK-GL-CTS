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

#include "gl4cRobustnessTests.hpp"
#include "gluContextInfo.hpp"
#include "gluPlatform.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"
#include <cstring>

using namespace glw;
using namespace deqp::RobustBufferAccessBehavior;

namespace gl4cts
{

/** Extensions test method
 * Checks if necessary extension is supported by context
 **/
static bool checkExtension(deqp::Context& context, const char* extensionName)
{
	/* If OpenGL 4.5 is available - feature shall be in Coree Profile. */
	if (glu::contextSupports(context.getRenderContext().getType(), glu::ApiType::core(4, 5)))
	{
		return true;
	}

	/* OpenGL 4.5 is not available - check extensions. */
	return context.getContextInfo().isExtensionSupported(extensionName);
}

namespace ResetNotificationStrategy
{

class RobustnessBase : public deqp::TestCase
{
private:
	glu::RenderContext* m_robustContext;

public:
	RobustnessBase(deqp::Context& context, const char* name, const char* description)
		: deqp::TestCase(context, name, description), m_robustContext(NULL)
	{
	}

	void createRobustContext(glu::ResetNotificationStrategy reset);
	void releaseRobustContext(void);

	glu::RenderContext* getRobustContext(void)
	{
		return m_robustContext;
	}
};

void RobustnessBase::createRobustContext(glu::ResetNotificationStrategy reset)
{
	glu::RenderConfig	renderCfg	(glu::ContextType(m_context.getRenderContext().getType().getAPI(), glu::CONTEXT_ROBUST));

	glu::parseRenderConfig(&renderCfg, m_context.getTestContext().getCommandLine());

	renderCfg.resetNotificationStrategy	= reset;
	renderCfg.surfaceType				= glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC;

	m_robustContext = glu::createRenderContext(m_testCtx.getPlatform(), m_testCtx.getCommandLine(), renderCfg);
}

void RobustnessBase::releaseRobustContext(void)
{
	if (m_robustContext)
	{
		delete m_robustContext;
		m_robustContext = NULL;
	}
}

class NoResetNotificationCase : public RobustnessBase
{
	typedef glw::GLenum(GLW_APIENTRY* PFNGLGETGRAPHICSRESETSTATUS)();

public:
	NoResetNotificationCase(deqp::Context& context, const char* name, const char* description)
		: RobustnessBase(context, name, description)
	{
	}

	virtual IterateResult iterate(void)
	{
		if (!checkExtension(m_context, "GL_KHR_robustness"))
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
			return STOP;
		}

		createRobustContext(glu::RESET_NOTIFICATION_STRATEGY_NO_RESET_NOTIFICATION);
		getRobustContext()->makeCurrent();

		PFNGLGETGRAPHICSRESETSTATUS pGetGraphicsResetStatus =
			(PFNGLGETGRAPHICSRESETSTATUS)m_context.getRenderContext().getProcAddress("glGetGraphicsResetStatus");

		if (DE_NULL == pGetGraphicsResetStatus)
		{
			m_context.getTestContext().setTestResult(QP_TEST_RESULT_INTERNAL_ERROR,
													 "Pointer to function glGetGraphicsResetStatus is NULL.");
			return STOP;
		}

		glw::GLint reset = 0;

		const glw::Functions& gl = getRobustContext()->getFunctions();
		gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &reset);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv");

		if (reset != GL_NO_RESET_NOTIFICATION)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! glGet returned wrong value [" << reset
							   << ", expected " << GL_NO_RESET_NOTIFICATION << "]." << tcu::TestLog::EndMessage;

			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		glw::GLint status = pGetGraphicsResetStatus();
		if (status != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message
							   << "Test failed! glGetGraphicsResetStatus returned wrong value [" << status
							   << ", expected " << GL_NO_ERROR << "]." << tcu::TestLog::EndMessage;

			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		releaseRobustContext();
		m_context.getRenderContext().makeCurrent();

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

class LoseContextOnResetCase : public RobustnessBase
{
public:
	LoseContextOnResetCase(deqp::Context& context, const char* name, const char* description)
		: RobustnessBase(context, name, description)
	{
	}

	virtual IterateResult iterate(void)
	{
		if (!checkExtension(m_context, "GL_KHR_robustness"))
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
			return STOP;
		}

		createRobustContext(glu::RESET_NOTIFICATION_STRATEGY_LOSE_CONTEXT_ON_RESET);
		getRobustContext()->makeCurrent();

		glw::GLint reset = 0;

		const glw::Functions& gl = getRobustContext()->getFunctions();
		gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &reset);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv");

		if (reset != GL_LOSE_CONTEXT_ON_RESET)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! glGet returned wrong value [" << reset
							   << ", expected " << GL_LOSE_CONTEXT_ON_RESET << "]." << tcu::TestLog::EndMessage;

			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			return STOP;
		}

		releaseRobustContext();
		m_context.getRenderContext().makeCurrent();

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		return STOP;
	}
};

} // ResetNotificationStrategy namespace

namespace RobustnessRobustBufferAccessBehavior
{
/* Buffer constants */
const GLuint Buffer::m_invalid_id = -1;

const GLenum Buffer::m_targets[Buffer::m_n_targets] = {
	GL_ARRAY_BUFFER,			  /*  0 */
	GL_ATOMIC_COUNTER_BUFFER,	 /*  1 */
	GL_COPY_READ_BUFFER,		  /*  2 */
	GL_COPY_WRITE_BUFFER,		  /*  3 */
	GL_DISPATCH_INDIRECT_BUFFER,  /*  4 */
	GL_DRAW_INDIRECT_BUFFER,	  /*  5 */
	GL_ELEMENT_ARRAY_BUFFER,	  /*  6 */
	GL_PIXEL_PACK_BUFFER,		  /*  7 */
	GL_PIXEL_UNPACK_BUFFER,		  /*  8 */
	GL_QUERY_BUFFER,			  /*  9 */
	GL_SHADER_STORAGE_BUFFER,	 /* 10 */
	GL_TRANSFORM_FEEDBACK_BUFFER, /* 11 */
	GL_UNIFORM_BUFFER,			  /* 12 */
};

/** Constructor.
 *
 * @param context CTS context.
 **/
Buffer::Buffer(deqp::Context& context) : m_id(m_invalid_id), m_context(context), m_target(GL_ARRAY_BUFFER)
{
}

/** Destructor
 *
 **/
Buffer::~Buffer()
{
	Release();
}

/** Initialize buffer instance
 *
 * @param target Buffer target
 * @param usage  Buffer usage enum
 * @param size   <size> parameter
 * @param data   <data> parameter
 **/
void Buffer::InitData(glw::GLenum target, glw::GLenum usage, glw::GLsizeiptr size, const glw::GLvoid* data)
{
	/* Delete previous buffer instance */
	Release();

	m_target = target;

	const Functions& gl = m_context.getRenderContext().getFunctions();

	Generate(gl, m_id);
	Bind(gl, m_id, m_target);
	Data(gl, m_target, usage, size, data);
}

/** Release buffer instance
 *
 **/
void Buffer::Release()
{
	if (m_invalid_id != m_id)
	{
		const Functions& gl = m_context.getRenderContext().getFunctions();

		gl.deleteBuffers(1, &m_id);
		m_id = m_invalid_id;
	}
}

/** Binds buffer to its target
 *
 **/
void Buffer::Bind() const
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	Bind(gl, m_id, m_target);
}

/** Binds indexed buffer
 *
 * @param index <index> parameter
 **/
void Buffer::BindBase(glw::GLuint index) const
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	BindBase(gl, m_id, m_target, index);
}

/** Bind buffer to given target
 *
 * @param gl     GL functions
 * @param id     Id of buffer
 * @param target Buffer target
 **/
void Buffer::Bind(const glw::Functions& gl, glw::GLuint id, glw::GLenum target)
{
	gl.bindBuffer(target, id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindBuffer");
}

/** Binds indexed buffer
 *
 * @param gl     GL functions
 * @param id     Id of buffer
 * @param target Buffer target
 * @param index  <index> parameter
 **/
void Buffer::BindBase(const glw::Functions& gl, glw::GLuint id, glw::GLenum target, glw::GLuint index)
{
	gl.bindBufferBase(target, index, id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindBufferBase");
}

/** Allocate memory for buffer and sends initial content
 *
 * @param gl     GL functions
 * @param target Buffer target
 * @param usage  Buffer usage enum
 * @param size   <size> parameter
 * @param data   <data> parameter
 **/
void Buffer::Data(const glw::Functions& gl, glw::GLenum target, glw::GLenum usage, glw::GLsizeiptr size,
				  const glw::GLvoid* data)
{
	gl.bufferData(target, size, data, usage);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BufferData");
}

/** Generate buffer
 *
 * @param gl     GL functions
 * @param out_id Id of buffer
 **/
void Buffer::Generate(const glw::Functions& gl, glw::GLuint& out_id)
{
	GLuint id = m_invalid_id;

	gl.genBuffers(1, &id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenBuffers");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Got invalid id");
	}

	out_id = id;
}

/** Update range of buffer
 *
 * @param gl     GL functions
 * @param target Buffer target
 * @param offset Offset in buffer
 * @param size   <size> parameter
 * @param data   <data> parameter
 **/
void Buffer::SubData(const glw::Functions& gl, glw::GLenum target, glw::GLintptr offset, glw::GLsizeiptr size,
					 glw::GLvoid* data)
{
	gl.bufferSubData(target, offset, size, data);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BufferSubData");
}

/* Framebuffer constants */
const GLuint Framebuffer::m_invalid_id = -1;

/** Constructor.
 *
 * @param context CTS context.
 **/
Framebuffer::Framebuffer(deqp::Context& context) : m_id(m_invalid_id), m_context(context)
{
	/* Nothing to done here */
}

/** Destructor
 *
 **/
Framebuffer::~Framebuffer()
{
	Release();
}

/** Release texture instance
 *
 **/
void Framebuffer::Release()
{
	if (m_invalid_id != m_id)
	{
		const Functions& gl = m_context.getRenderContext().getFunctions();

		gl.deleteFramebuffers(1, &m_id);
		m_id = m_invalid_id;
	}
}

/** Attach texture to specified attachment
 *
 * @param gl         GL functions
 * @param target     Framebuffer target
 * @param attachment Attachment
 * @param texture_id Texture id
 * @param level      Level of mipmap
 * @param width      Texture width
 * @param height     Texture height
 **/
void Framebuffer::AttachTexture(const glw::Functions& gl, glw::GLenum target, glw::GLenum attachment,
								glw::GLuint texture_id, glw::GLint level, glw::GLuint width, glw::GLuint height)
{
	gl.framebufferTexture(target, attachment, texture_id, level);
	GLU_EXPECT_NO_ERROR(gl.getError(), "FramebufferTexture");

	gl.viewport(0 /* x */, 0 /* y */, width, height);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Viewport");
}

/** Binds framebuffer to DRAW_FRAMEBUFFER
 *
 * @param gl     GL functions
 * @param target Framebuffer target
 * @param id     ID of framebuffer
 **/
void Framebuffer::Bind(const glw::Functions& gl, glw::GLenum target, glw::GLuint id)
{
	gl.bindFramebuffer(target, id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindFramebuffer");
}

/** Generate framebuffer
 *
 **/
void Framebuffer::Generate(const glw::Functions& gl, glw::GLuint& out_id)
{
	GLuint id = m_invalid_id;

	gl.genFramebuffers(1, &id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenFramebuffers");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Invalid id");
	}

	out_id = id;
}

/* Program constants */
const GLuint Program::m_invalid_id = 0;

/** Constructor.
 *
 * @param context CTS context.
 **/
Program::Program(deqp::Context& context)
	: m_id(m_invalid_id)
	, m_compute(context)
	, m_fragment(context)
	, m_geometry(context)
	, m_tess_ctrl(context)
	, m_tess_eval(context)
	, m_vertex(context)
	, m_context(context)
{
	/* Nothing to be done here */
}

/** Destructor
 *
 **/
Program::~Program()
{
	Release();
}

/** Initialize program instance
 *
 * @param compute_shader                Compute shader source code
 * @param fragment_shader               Fragment shader source code
 * @param geometry_shader               Geometry shader source code
 * @param tesselation_control_shader    Tesselation control shader source code
 * @param tesselation_evaluation_shader Tesselation evaluation shader source code
 * @param vertex_shader                 Vertex shader source code
 **/
void Program::Init(const std::string& compute_shader, const std::string& fragment_shader,
				   const std::string& geometry_shader, const std::string& tesselation_control_shader,
				   const std::string& tesselation_evaluation_shader, const std::string& vertex_shader)
{
	/* Delete previous program */
	Release();

	/* GL entry points */
	const Functions& gl = m_context.getRenderContext().getFunctions();

	/* Initialize shaders */
	m_compute.Init(GL_COMPUTE_SHADER, compute_shader);
	m_fragment.Init(GL_FRAGMENT_SHADER, fragment_shader);
	m_geometry.Init(GL_GEOMETRY_SHADER, geometry_shader);
	m_tess_ctrl.Init(GL_TESS_CONTROL_SHADER, tesselation_control_shader);
	m_tess_eval.Init(GL_TESS_EVALUATION_SHADER, tesselation_evaluation_shader);
	m_vertex.Init(GL_VERTEX_SHADER, vertex_shader);

	/* Create program, set up transform feedback and attach shaders */
	Create(gl, m_id);
	Attach(gl, m_id, m_compute.m_id);
	Attach(gl, m_id, m_fragment.m_id);
	Attach(gl, m_id, m_geometry.m_id);
	Attach(gl, m_id, m_tess_ctrl.m_id);
	Attach(gl, m_id, m_tess_eval.m_id);
	Attach(gl, m_id, m_vertex.m_id);

	/* Link program */
	Link(gl, m_id);
}

/** Release program instance
 *
 **/
void Program::Release()
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	if (m_invalid_id != m_id)
	{
		Use(gl, m_invalid_id);

		gl.deleteProgram(m_id);
		m_id = m_invalid_id;
	}

	m_compute.Release();
	m_fragment.Release();
	m_geometry.Release();
	m_tess_ctrl.Release();
	m_tess_eval.Release();
	m_vertex.Release();
}

/** Set program as active
 *
 **/
void Program::Use() const
{
	const Functions& gl = m_context.getRenderContext().getFunctions();

	Use(gl, m_id);
}

/** Attach shader to program
 *
 * @param gl         GL functions
 * @param program_id Id of program
 * @param shader_id  Id of shader
 **/
void Program::Attach(const glw::Functions& gl, glw::GLuint program_id, glw::GLuint shader_id)
{
	/* Sanity checks */
	if ((m_invalid_id == program_id) || (Shader::m_invalid_id == shader_id))
	{
		return;
	}

	gl.attachShader(program_id, shader_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "AttachShader");
}

/** Create program instance
 *
 * @param gl     GL functions
 * @param out_id Id of program
 **/
void Program::Create(const glw::Functions& gl, glw::GLuint& out_id)
{
	const GLuint id = gl.createProgram();
	GLU_EXPECT_NO_ERROR(gl.getError(), "CreateProgram");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Failed to create program");
	}

	out_id = id;
}

/** Link program
 *
 * @param gl GL functions
 * @param id Id of program
 **/
void Program::Link(const glw::Functions& gl, glw::GLuint id)
{
	GLint status = GL_FALSE;

	gl.linkProgram(id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "LinkProgram");

	/* Get link status */
	gl.getProgramiv(id, GL_LINK_STATUS, &status);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GetProgramiv");

	/* Log link error */
	if (GL_TRUE != status)
	{
		glw::GLint  length = 0;
		std::string message;

		/* Get error log length */
		gl.getProgramiv(id, GL_INFO_LOG_LENGTH, &length);
		GLU_EXPECT_NO_ERROR(gl.getError(), "GetProgramiv");

		message.resize(length, 0);

		/* Get error log */
		gl.getProgramInfoLog(id, length, 0, &message[0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "GetProgramInfoLog");

		TCU_FAIL(message.c_str());
	}
}

/** Use program
 *
 * @param gl GL functions
 * @param id Id of program
 **/
void Program::Use(const glw::Functions& gl, glw::GLuint id)
{
	gl.useProgram(id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "UseProgram");
}

/* Shader's constants */
const GLuint Shader::m_invalid_id = 0;

/** Constructor.
 *
 * @param context CTS context.
 **/
Shader::Shader(deqp::Context& context) : m_id(m_invalid_id), m_context(context)
{
	/* Nothing to be done here */
}

/** Destructor
 *
 **/
Shader::~Shader()
{
	Release();
}

/** Initialize shader instance
 *
 * @param stage  Shader stage
 * @param source Source code
 **/
void Shader::Init(glw::GLenum stage, const std::string& source)
{
	if (true == source.empty())
	{
		/* No source == no shader */
		return;
	}

	/* Delete any previous shader */
	Release();

	/* Create, set source and compile */
	const Functions& gl = m_context.getRenderContext().getFunctions();

	Create(gl, stage, m_id);
	Source(gl, m_id, source);

	Compile(gl, m_id);
}

/** Release shader instance
 *
 **/
void Shader::Release()
{
	if (m_invalid_id != m_id)
	{
		const Functions& gl = m_context.getRenderContext().getFunctions();

		gl.deleteShader(m_id);
		m_id = m_invalid_id;
	}
}

/** Compile shader
 *
 * @param gl GL functions
 * @param id Shader id
 **/
void Shader::Compile(const glw::Functions& gl, glw::GLuint id)
{
	GLint status = GL_FALSE;

	/* Compile */
	gl.compileShader(id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "CompileShader");

	/* Get compilation status */
	gl.getShaderiv(id, GL_COMPILE_STATUS, &status);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GetShaderiv");

	/* Log compilation error */
	if (GL_TRUE != status)
	{
		glw::GLint  length = 0;
		std::string message;

		/* Error log length */
		gl.getShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		GLU_EXPECT_NO_ERROR(gl.getError(), "GetShaderiv");

		/* Prepare storage */
		message.resize(length, 0);

		/* Get error log */
		gl.getShaderInfoLog(id, length, 0, &message[0]);
		GLU_EXPECT_NO_ERROR(gl.getError(), "GetShaderInfoLog");

		TCU_FAIL(message.c_str());
	}
}

/** Create shader
 *
 * @param gl     GL functions
 * @param stage  Shader stage
 * @param out_id Shader id
 **/
void Shader::Create(const glw::Functions& gl, glw::GLenum stage, glw::GLuint& out_id)
{
	const GLuint id = gl.createShader(stage);
	GLU_EXPECT_NO_ERROR(gl.getError(), "CreateShader");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Failed to create shader");
	}

	out_id = id;
}

/** Set shader's source code
 *
 * @param gl     GL functions
 * @param id     Shader id
 * @param source Shader source code
 **/
void Shader::Source(const glw::Functions& gl, glw::GLuint id, const std::string& source)
{
	const GLchar* code = source.c_str();

	gl.shaderSource(id, 1 /* count */, &code, 0 /* lengths */);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ShaderSource");
}

/* Texture static fields */
const GLuint Texture::m_invalid_id = -1;

/** Constructor.
 *
 * @param context CTS context.
 **/
Texture::Texture(deqp::Context& context) : m_id(m_invalid_id), m_context(context)
{
	/* Nothing to done here */
}

/** Destructor
 *
 **/
Texture::~Texture()
{
	Release();
}

/** Release texture instance
 *
 **/
void Texture::Release()
{
	if (m_invalid_id != m_id)
	{
		const Functions& gl = m_context.getRenderContext().getFunctions();

		gl.deleteTextures(1, &m_id);
		m_id = m_invalid_id;
	}
}

/** Bind texture to target
 *
 * @param gl       GL functions
 * @param id       Id of texture
 * @param tex_type Type of texture
 **/
void Texture::Bind(const glw::Functions& gl, glw::GLuint id, glw::GLenum target)
{
	gl.bindTexture(target, id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindTexture");
}

/** Set contents of compressed texture
 *
 * @param gl              GL functions
 * @param target          Texture target
 * @param level           Mipmap level
 * @param internal_format Format of data
 * @param width           Width of texture
 * @param height          Height of texture
 * @param depth           Depth of texture
 * @param image_size      Size of data
 * @param data            Buffer with image data
 **/
void Texture::CompressedImage(const glw::Functions& gl, glw::GLenum target, glw::GLint level,
							  glw::GLenum internal_format, glw::GLuint width, glw::GLuint height, glw::GLuint depth,
							  glw::GLsizei image_size, const glw::GLvoid* data)
{
	switch (target)
	{
	case GL_TEXTURE_1D:
		gl.compressedTexImage1D(target, level, internal_format, width, 0 /* border */, image_size, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "CompressedTexImage1D");
		break;

	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
		gl.compressedTexImage2D(target, level, internal_format, width, height, 0 /* border */, image_size, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "CompressedTexImage2D");
		break;

	case GL_TEXTURE_CUBE_MAP:
		gl.compressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format, width, height, 0 /* border */,
								image_size, data);
		gl.compressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level, internal_format, width, height, 0 /* border */,
								image_size, data);
		gl.compressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, internal_format, width, height, 0 /* border */,
								image_size, data);
		gl.compressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level, internal_format, width, height, 0 /* border */,
								image_size, data);
		gl.compressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, level, internal_format, width, height, 0 /* border */,
								image_size, data);
		gl.compressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internal_format, width, height, 0 /* border */,
								image_size, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "CompressedTexImage2D");
		break;

	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		gl.compressedTexImage3D(target, level, internal_format, width, height, depth, 0 /* border */, image_size, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "CompressedTexImage3D");
		break;

	default:
		TCU_FAIL("Invliad enum");
		break;
	}
}

/** Generate texture instance
 *
 * @param gl     GL functions
 * @param out_id Id of texture
 **/
void Texture::Generate(const glw::Functions& gl, glw::GLuint& out_id)
{
	GLuint id = m_invalid_id;

	gl.genTextures(1, &id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenTextures");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Invalid id");
	}

	out_id = id;
}

/** Get texture data
 *
 * @param gl       GL functions
 * @param target   Texture target
 * @param format   Format of data
 * @param type     Type of data
 * @param out_data Buffer for data
 **/
void Texture::GetData(const glw::Functions& gl, glw::GLint level, glw::GLenum target, glw::GLenum format,
					  glw::GLenum type, glw::GLvoid* out_data)
{
	gl.getTexImage(target, level, format, type, out_data);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GetTexImage");
}

/** Get texture data
 *
 * @param gl       GL functions
 * @param id       Texture id
 * @param level    Mipmap level
 * @param width    Texture width
 * @param height   Texture height
 * @param format   Format of data
 * @param type     Type of data
 * @param out_data Buffer for data
 **/
void Texture::GetData(const glw::Functions& gl, glw::GLuint id, glw::GLint level, glw::GLuint width, glw::GLuint height,
					  glw::GLenum format, glw::GLenum type, glw::GLvoid* out_data)
{
	GLuint fbo;
	gl.genFramebuffers(1, &fbo);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenFramebuffers");
	gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindFramebuffer");
	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, level);
	GLU_EXPECT_NO_ERROR(gl.getError(), "FramebufferTexture2D");

	gl.readPixels(0, 0, width, height, format, type, out_data);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ReadPixels");

	gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
}

/** Generate texture instance
 *
 * @param gl     GL functions
 * @param target Texture target
 * @param level  Mipmap level
 * @param pname  Parameter to query
 * @param param  Result of query
 **/
void Texture::GetLevelParameter(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLenum pname,
								glw::GLint* param)
{
	gl.getTexLevelParameteriv(target, level, pname, param);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GetTexLevelParameteriv");
}

/** Set contents of texture
 *
 * @param gl              GL functions
 * @param target          Texture target
 * @param level           Mipmap level
 * @param internal_format Format of data
 * @param width           Width of texture
 * @param height          Height of texture
 * @param depth           Depth of texture
 * @param format          Format of data
 * @param type            Type of data
 * @param data            Buffer with image data
 **/
void Texture::Image(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLenum internal_format,
					glw::GLuint width, glw::GLuint height, glw::GLuint depth, glw::GLenum format, glw::GLenum type,
					const glw::GLvoid* data)
{
	switch (target)
	{
	case GL_TEXTURE_1D:
		gl.texImage1D(target, level, internal_format, width, 0 /* border */, format, type, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexImage1D");
		break;

	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
		gl.texImage2D(target, level, internal_format, width, height, 0 /* border */, format, type, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexImage2D");
		break;

	case GL_TEXTURE_CUBE_MAP:
		gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format, width, height, 0 /* border */, format,
					  type, data);
		gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level, internal_format, width, height, 0 /* border */, format,
					  type, data);
		gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, internal_format, width, height, 0 /* border */, format,
					  type, data);
		gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level, internal_format, width, height, 0 /* border */, format,
					  type, data);
		gl.texImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, level, internal_format, width, height, 0 /* border */, format,
					  type, data);
		gl.texImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internal_format, width, height, 0 /* border */, format,
					  type, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexImage2D");
		break;

	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		gl.texImage3D(target, level, internal_format, width, height, depth, 0 /* border */, format, type, data);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexImage3D");
		break;

	default:
		TCU_FAIL("Invliad enum");
		break;
	}
}

/** Allocate storage for texture
 *
 * @param gl              GL functions
 * @param target          Texture target
 * @param levels          Number of levels
 * @param internal_format Internal format of texture
 * @param width           Width of texture
 * @param height          Height of texture
 * @param depth           Depth of texture
 **/
void Texture::Storage(const glw::Functions& gl, glw::GLenum target, glw::GLsizei levels, glw::GLenum internal_format,
					  glw::GLuint width, glw::GLuint height, glw::GLuint depth)
{
	switch (target)
	{
	case GL_TEXTURE_1D:
		gl.texStorage1D(target, levels, internal_format, width);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage1D");
		break;

	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
	case GL_TEXTURE_CUBE_MAP:
		gl.texStorage2D(target, levels, internal_format, width, height);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage2D");
		break;

	case GL_TEXTURE_2D_MULTISAMPLE:
		gl.texStorage2DMultisample(target, levels, internal_format, width, height, GL_FALSE);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage2DMultisample");
		break;

	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		gl.texStorage3D(target, levels, internal_format, width, height, depth);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexStorage3D");
		break;

	default:
		TCU_FAIL("Invliad enum");
		break;
	}
}

/** Set contents of texture
 *
 * @param gl              GL functions
 * @param target          Texture target
 * @param level           Mipmap level
 * @param x               X offset
 * @param y               Y offset
 * @param z               Z offset
 * @param width           Width of texture
 * @param height          Height of texture
 * @param depth           Depth of texture
 * @param format          Format of data
 * @param type            Type of data
 * @param pixels          Buffer with image data
 **/
void Texture::SubImage(const glw::Functions& gl, glw::GLenum target, glw::GLint level, glw::GLint x, glw::GLint y,
					   glw::GLint z, glw::GLsizei width, glw::GLsizei height, glw::GLsizei depth, glw::GLenum format,
					   glw::GLenum type, const glw::GLvoid* pixels)
{
	switch (target)
	{
	case GL_TEXTURE_1D:
		gl.texSubImage1D(target, level, x, width, format, type, pixels);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexSubImage1D");
		break;

	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
		gl.texSubImage2D(target, level, x, y, width, height, format, type, pixels);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexSubImage2D");
		break;

	case GL_TEXTURE_CUBE_MAP:
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, level, x, y, width, height, format, type, pixels);
		gl.texSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, x, y, width, height, format, type, pixels);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexSubImage2D");
		break;
	case GL_TEXTURE_3D:
	case GL_TEXTURE_2D_ARRAY:
		gl.texSubImage3D(target, level, x, y, z, width, height, depth, format, type, pixels);
		GLU_EXPECT_NO_ERROR(gl.getError(), "TexSubImage3D");
		break;

	default:
		TCU_FAIL("Invliad enum");
		break;
	}
}

/* VertexArray constants */
const GLuint VertexArray::m_invalid_id = -1;

/** Constructor.
 *
 * @param context CTS context.
 **/
VertexArray::VertexArray(deqp::Context& context) : m_id(m_invalid_id), m_context(context)
{
}

/** Destructor
 *
 **/
VertexArray::~VertexArray()
{
	Release();
}

/** Release vertex array object instance
 *
 **/
void VertexArray::Release()
{
	if (m_invalid_id != m_id)
	{
		const Functions& gl = m_context.getRenderContext().getFunctions();

		Bind(gl, 0);

		gl.deleteVertexArrays(1, &m_id);

		m_id = m_invalid_id;
	}
}

/** Binds Vertex array object
 *
 * @param gl GL functions
 * @param id ID of vertex array object
 **/
void VertexArray::Bind(const glw::Functions& gl, glw::GLuint id)
{
	gl.bindVertexArray(id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindVertexArray");
}

/** Generates Vertex array object
 *
 * @param gl     GL functions
 * @param out_id ID of vertex array object
 **/
void VertexArray::Generate(const glw::Functions& gl, glw::GLuint& out_id)
{
	GLuint id = m_invalid_id;

	gl.genVertexArrays(1, &id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenVertexArrays");

	if (m_invalid_id == id)
	{
		TCU_FAIL("Invalid id");
	}

	out_id = id;
}

/** Constructor
 *
 * @param context Test context
 **/
GetnUniformTest::GetnUniformTest(deqp::Context& context)
	: deqp::TestCase(context, "getnuniform", "Verifies if read uniform variables to the buffer with bufSize less than "
											 "expected result with GL_INVALID_OPERATION")
	, m_pGetnUniformfv(DE_NULL)
	, m_pGetnUniformiv(DE_NULL)
	, m_pGetnUniformuiv(DE_NULL)
{
	/* Nothing to be done here */
}

/** Execute test
 *
 * @return tcu::TestNode::STOP
 **/
tcu::TestNode::IterateResult GetnUniformTest::iterate()
{
	if (!checkExtension(m_context, "GL_KHR_robustness") ||
		!checkExtension(m_context, "GL_KHR_robust_buffer_access_behavior"))
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
		return STOP;
	}

	m_pGetnUniformfv  = (PFNGLGETNUNIFORMFV)m_context.getRenderContext().getProcAddress("glGetnUniformfv");
	m_pGetnUniformiv  = (PFNGLGETNUNIFORMIV)m_context.getRenderContext().getProcAddress("glGetnUniformiv");
	m_pGetnUniformuiv = (PFNGLGETNUNIFORMUIV)m_context.getRenderContext().getProcAddress("glGetnUniformuiv");

	if ((DE_NULL == m_pGetnUniformfv) || (DE_NULL == m_pGetnUniformiv) || (DE_NULL == m_pGetnUniformuiv))
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_INTERNAL_ERROR,
												 "Pointer to function glGetnUniform* is NULL.");
		return STOP;
	}

	const Functions& gl = m_context.getRenderContext().getFunctions();

	const GLfloat input4f[]  = { 1.0f, 5.4f, 3.14159f, 1.28f };
	const GLint   input3i[]  = { 10, -20, -30 };
	const GLuint  input4ui[] = { 10, 20, 30, 40 };

	/* Test result indicator */
	bool test_result = true;

	/* Iterate over all cases */
	Program program(m_context);

	/* Compute Shader */
	const std::string& cs = getComputeShader();

	/* Shaders initialization */
	program.Init(cs /* cs */, "" /* fs */, "" /* gs */, "" /* tcs */, "" /* tes */, "" /* vs */);
	program.Use();

	/* passing uniform values */
	gl.programUniform4fv(program.m_id, 11, 1, input4f);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ProgramUniform4fv");

	gl.programUniform3iv(program.m_id, 12, 1, input3i);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ProgramUniform3iv");

	gl.programUniform4uiv(program.m_id, 13, 1, input4ui);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ProgramUniform4uiv");

	gl.dispatchCompute(1, 1, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "DispatchCompute");

	/* veryfing gfetnUniform error messages */
	GLfloat result4f[4];
	GLint   result3i[3];
	GLuint  result4ui[4];

	m_pGetnUniformfv(program.m_id, 11, sizeof(GLfloat) * 4, result4f);
	test_result = test_result &&
				  verifyResult((void*)input4f, (void*)result4f, sizeof(GLfloat) * 4, "getnUniformfv [false negative]");
	test_result = test_result && verifyError(gl.getError(), GL_NO_ERROR, "getnUniformfv [false negative]");

	m_pGetnUniformfv(program.m_id, 11, sizeof(GLfloat) * 3, result4f);
	test_result = test_result && verifyError(gl.getError(), GL_INVALID_OPERATION, "getnUniformfv [false positive]");

	m_pGetnUniformiv(program.m_id, 12, sizeof(GLint) * 3, result3i);
	test_result = test_result &&
				  verifyResult((void*)input3i, (void*)result3i, sizeof(GLint) * 3, "getnUniformiv [false negative]");
	test_result = test_result && verifyError(gl.getError(), GL_NO_ERROR, "getnUniformiv [false negative]");

	m_pGetnUniformiv(program.m_id, 12, sizeof(GLint) * 2, result3i);
	test_result = test_result && verifyError(gl.getError(), GL_INVALID_OPERATION, "getnUniformiv [false positive]");

	m_pGetnUniformuiv(program.m_id, 13, sizeof(GLuint) * 4, result4ui);
	test_result = test_result && verifyResult((void*)input4ui, (void*)result4ui, sizeof(GLuint) * 4,
											  "getnUniformuiv [false negative]");
	test_result = test_result && verifyError(gl.getError(), GL_NO_ERROR, "getnUniformuiv [false negative]");

	m_pGetnUniformuiv(program.m_id, 13, sizeof(GLuint) * 3, result4ui);
	test_result = test_result && verifyError(gl.getError(), GL_INVALID_OPERATION, "getnUniformuiv [false positive]");

	/* Set result */
	if (true == test_result)
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}
	else
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}

	/* Done */
	return tcu::TestNode::STOP;
}

std::string GetnUniformTest::getComputeShader()
{
	static const GLchar* cs = "#version 320 es\n"
							  "\n"
							  "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
							  "\n"
							  "layout (location = 11) uniform vec4 inputf;\n"
							  "layout (location = 12) uniform ivec3 inputi;\n"
							  "layout (location = 13) uniform uvec4 inputu;\n"
							  "\n"
							  "shared float valuef;\n"
							  "shared int valuei;\n"
							  "shared uint valueu;\n"
							  "\n"
							  "void main()\n"
							  "{\n"
							  "   valuef = inputf.r + inputf.g + inputf.b + inputf.a;\n"
							  "   valuei = inputi.r + inputi.g + inputi.b;\n"
							  "   valueu = inputu.r + inputu.g + inputu.b + inputu.a;\n"
							  "}\n"
							  "\n";

	std::string source = cs;

	return source;
}

bool GetnUniformTest::verifyResult(const void* inputData, const void* resultData, int size, const char* method)
{
	int diff = memcmp(inputData, resultData, size);
	if (diff != 0)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! " << method << " result is not as expected."
						   << tcu::TestLog::EndMessage;

		return false;
	}

	return true;
}

bool GetnUniformTest::verifyError(GLint error, GLint expectedError, const char* method)
{
	if (error != expectedError)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! " << method << " throws unexpected error ["
						   << error << "]." << tcu::TestLog::EndMessage;

		return false;
	}

	return true;
}

/** Constructor
 *
 * @param context Test context
 **/
ReadnPixelsTest::ReadnPixelsTest(deqp::Context& context)
	: TestCase(context, "readnpixels", "Verifies if read pixels to the buffer with bufSize less than expected result "
									   "with GL_INVALID_OPERATION error")
	, m_pReadnPixels(DE_NULL)
{
	/* Nothing to be done here */
}

/** Execute test
 *
 * @return tcu::TestNode::STOP
 **/
tcu::TestNode::IterateResult ReadnPixelsTest::iterate()
{
	if (!checkExtension(m_context, "GL_KHR_robustness") ||
		!checkExtension(m_context, "GL_KHR_robust_buffer_access_behavior"))
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not Supported");
		return STOP;
	}

	m_pReadnPixels = (PFNGLREADNPIXELS)m_context.getRenderContext().getProcAddress("glReadnPixels");

	if (DE_NULL == m_pReadnPixels)
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_INTERNAL_ERROR,
												 "Pointer to function glReadnPixels is NULL.");
		return STOP;
	}

	static const GLuint elements[] = {
		0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6, 0, 6, 7, 0, 7, 8, 0, 8, 1,
	};

	static const GLfloat vertices[] = {
		0.0f,  0.0f,  0.0f, 1.0f, /* 0 */
		-1.0f, 0.0f,  0.0f, 1.0f, /* 1 */
		-1.0f, 1.0f,  0.0f, 1.0f, /* 2 */
		0.0f,  1.0f,  0.0f, 1.0f, /* 3 */
		1.0f,  1.0f,  0.0f, 1.0f, /* 4 */
		1.0f,  0.0f,  0.0f, 1.0f, /* 5 */
		1.0f,  -1.0f, 0.0f, 1.0f, /* 6 */
		0.0f,  -1.0f, 0.0f, 1.0f, /* 7 */
		-1.0f, -1.0f, 0.0f, 1.0f, /* 8 */
	};

	static const GLchar* fs = "#version 320 es\n"
							  "\n"
							  "layout (location = 0) out lowp vec4 out_fs_color;\n"
							  "\n"
							  "void main()\n"
							  "{\n"
							  "    out_fs_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
							  "}\n"
							  "\n";

	static const GLchar* vs = "#version 320 es\n"
							  "\n"
							  "layout (location = 0) in vec4 in_vs_position;\n"
							  "\n"
							  "void main()\n"
							  "{\n"
							  "    gl_Position = in_vs_position;\n"
							  "}\n"
							  "\n";

	static const GLuint height	 = 8;
	static const GLuint width	  = 8;
	static const GLuint n_vertices = 24;

	/* GL entry points */
	const Functions& gl = m_context.getRenderContext().getFunctions();

	/* Test case objects */
	Program		program(m_context);
	Texture		texture(m_context);
	Buffer		elements_buffer(m_context);
	Buffer		vertices_buffer(m_context);
	VertexArray vao(m_context);

	/* Vertex array initialization */
	VertexArray::Generate(gl, vao.m_id);
	VertexArray::Bind(gl, vao.m_id);

	/* Texture initialization */
	Texture::Generate(gl, texture.m_id);
	Texture::Bind(gl, texture.m_id, GL_TEXTURE_2D);
	Texture::Storage(gl, GL_TEXTURE_2D, 1, GL_R8UI, width, height, 0);
	Texture::Bind(gl, 0, GL_TEXTURE_2D);

	/* Framebuffer initialization */
	GLuint fbo;
	gl.genFramebuffers(1, &fbo);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenFramebuffers");
	gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindFramebuffer");
	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.m_id, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "FramebufferTexture2D");

	/* Buffers initialization */
	elements_buffer.InitData(GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(elements), elements);
	vertices_buffer.InitData(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, sizeof(vertices), vertices);

	/* Shaders initialization */
	program.Init("" /* cs */, fs, "" /* gs */, "" /* tcs */, "" /* tes */, vs);
	Program::Use(gl, program.m_id);

	/* Vertex buffer initialization */
	vertices_buffer.Bind();
	gl.bindVertexBuffer(0 /* bindindex = location */, vertices_buffer.m_id, 0 /* offset */, 16 /* stride */);
	gl.enableVertexAttribArray(0 /* location */);

	/* Binding elements/indices buffer */
	elements_buffer.Bind();

	cleanTexture(texture.m_id);

	gl.drawElements(GL_TRIANGLES, n_vertices, GL_UNSIGNED_INT, 0 /* indices */);
	GLU_EXPECT_NO_ERROR(gl.getError(), "DrawElements");

	/* Set result */
	if (verifyResults())
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}
	else
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}

	/* Done */
	return tcu::TestNode::STOP;
}

/** Fill texture with value 128
 *
 * @param texture_id Id of texture
 **/
void ReadnPixelsTest::cleanTexture(glw::GLuint texture_id)
{
	static const GLuint height = 8;
	static const GLuint width  = 8;

	const Functions& gl = m_context.getRenderContext().getFunctions();

	GLubyte pixels[width * height];
	for (GLuint i = 0; i < width * height; ++i)
	{
		pixels[i] = 64;
	}

	Texture::Bind(gl, texture_id, GL_TEXTURE_2D);

	Texture::SubImage(gl, GL_TEXTURE_2D, 0 /* level  */, 0 /* x */, 0 /* y */, 0 /* z */, width, height, 0 /* depth */,
					  GL_RED_INTEGER, GL_UNSIGNED_BYTE, pixels);

	/* Unbind */
	Texture::Bind(gl, 0, GL_TEXTURE_2D);
}

/** Verifies glReadnPixels results
 *
 * @return true when glReadnPixels , false otherwise
 **/
bool ReadnPixelsTest::verifyResults()
{
	static const GLuint height = 8;
	static const GLuint width  = 8;

	const Functions& gl = m_context.getRenderContext().getFunctions();

	//Valid buffer size test
	const GLint bufSizeValid = width * height;
	GLubyte		pixelsValid[bufSizeValid];

	m_pReadnPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, bufSizeValid, pixelsValid);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ReadnPixels");

	//Verify glReadnPixels result
	for (GLuint i = 0; i < width * height; ++i)
	{
		if (pixelsValid[i] != 255)
			return false;
	}

	//Invalid buffer size test
	const GLint bufSizeInvalid = width * height - 1;
	GLubyte		pixelsInvalid[bufSizeInvalid];

	m_pReadnPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, bufSizeInvalid, pixelsInvalid);
	if (!verifyError(gl.getError(), GL_INVALID_OPERATION, "ReadnPixels [false positive]"))
		return false;

	return true;
}

/** Verify operation errors
 *
 * @param error OpenGL ES error code
 * @param expectedError Expected error code
 * @param method Method name marker
 *
 * @return true when error is as expected, false otherwise
 **/
bool ReadnPixelsTest::verifyError(GLint error, GLint expectedError, const char* method)
{
	if (error != expectedError)
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! " << method << " throws unexpected error ["
						   << error << "]." << tcu::TestLog::EndMessage;

		return false;
	}

	return true;
}

} // RobustBufferAccessBehavior namespace

RobustnessTests::RobustnessTests(deqp::Context& context)
	: TestCaseGroup(context, "robustness", "Verifies API coverage and functionality of GL_KHR_robustness extension.")
{
}

void RobustnessTests::init()
{
	deqp::TestCaseGroup::init();

	try
	{
		addChild(new ResetNotificationStrategy::NoResetNotificationCase(
			m_context, "noResetNotification", "Verifies if NO_RESET_NOTIFICATION strategy works as expected."));
		addChild(new ResetNotificationStrategy::LoseContextOnResetCase(
			m_context, "loseContextOnReset", "Verifies if LOSE_CONTEXT_ON_RESET strategy works as expected."));

		addChild(new RobustnessRobustBufferAccessBehavior::GetnUniformTest(m_context));
		addChild(new RobustnessRobustBufferAccessBehavior::ReadnPixelsTest(m_context));
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestCaseGroup::deinit();
		throw;
	}
}

} // es32cts namespace
