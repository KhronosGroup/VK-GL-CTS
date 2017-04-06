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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "es32cRobustnessTests.hpp"
#include "gluRenderConfig.hpp"
#include "gluRenderContext.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

using namespace glw;
using namespace deqp::RobustBufferAccessBehavior;

namespace es32cts
{
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
	glu::RenderConfig renderCfg(glu::ContextType(m_context.getRenderContext().getType().getAPI(), glu::CONTEXT_ROBUST));

	const tcu::CommandLine& commandLine = m_context.getTestContext().getCommandLine();
	glu::parseRenderConfig(&renderCfg, commandLine);

	if (commandLine.getSurfaceType() == tcu::SURFACETYPE_WINDOW)
	{
		renderCfg.resetNotificationStrategy = reset;
		renderCfg.surfaceType = glu::RenderConfig::SURFACETYPE_OFFSCREEN_GENERIC;
	}
	else
	{
		throw tcu::NotSupportedError("Test not supported in non-windowed context");
	}

	m_robustContext = glu::createRenderContext(m_testCtx.getPlatform(), commandLine, renderCfg);

	m_robustContext->makeCurrent();
}

void RobustnessBase::releaseRobustContext(void)
{
	if (m_robustContext)
	{
		delete m_robustContext;
		m_robustContext = NULL;
		m_context.getRenderContext().makeCurrent();
	}
}

class NoResetNotificationCase : public RobustnessBase
{
public:
	NoResetNotificationCase(deqp::Context& context, const char* name, const char* description)
		: RobustnessBase(context, name, description)
	{
	}

	virtual void deinit(void)
	{
		releaseRobustContext();
	}

	virtual IterateResult iterate(void)
	{
		createRobustContext(glu::RESET_NOTIFICATION_STRATEGY_NO_RESET_NOTIFICATION);

		glw::GLint reset = 0;

		const glw::Functions& gl = getRobustContext()->getFunctions();
		gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &reset);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv");

		if (reset != GL_NO_RESET_NOTIFICATION)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! glGet returned wrong value [" << reset
							   << ", expected " << GL_NO_RESET_NOTIFICATION << "]." << tcu::TestLog::EndMessage;

			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			releaseRobustContext();
			return STOP;
		}

		glw::GLint status = gl.getGraphicsResetStatus();
		if (status != GL_NO_ERROR)
		{
			m_testCtx.getLog() << tcu::TestLog::Message
							   << "Test failed! glGetGraphicsResetStatus returned wrong value [" << status
							   << ", expected " << GL_NO_ERROR << "]." << tcu::TestLog::EndMessage;

			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			releaseRobustContext();
			return STOP;
		}

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		releaseRobustContext();
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

	virtual void deinit(void)
	{
		releaseRobustContext();
	}

	virtual IterateResult iterate(void)
	{
		createRobustContext(glu::RESET_NOTIFICATION_STRATEGY_LOSE_CONTEXT_ON_RESET);

		glw::GLint reset = 0;

		const glw::Functions& gl = getRobustContext()->getFunctions();
		gl.getIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &reset);
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv");

		if (reset != GL_LOSE_CONTEXT_ON_RESET)
		{
			m_testCtx.getLog() << tcu::TestLog::Message << "Test failed! glGet returned wrong value [" << reset
							   << ", expected " << GL_LOSE_CONTEXT_ON_RESET << "]." << tcu::TestLog::EndMessage;

			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
			releaseRobustContext();
			return STOP;
		}

		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		releaseRobustContext();
		return STOP;
	}
};

} // ResetNotificationStrategy namespace

namespace RobustBufferAccessBehavior
{
/** Constructor
 *
 * @param context Test context
 **/
GetnUniformTest::GetnUniformTest(deqp::Context& context)
	: TestCase(context, "getnuniform", "Verifies if read uniform variables to the buffer with bufSize less than "
									   "expected result with GL_INVALID_OPERATION")
{
	/* Nothing to be done here */
}

/** Execute test
 *
 * @return tcu::TestNode::STOP
 **/
tcu::TestNode::IterateResult GetnUniformTest::iterate()
{
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

	gl.getnUniformfv(program.m_id, 11, sizeof(GLfloat) * 4, result4f);
	test_result = test_result &&
				  verifyResult((void*)input4f, (void*)result4f, sizeof(GLfloat) * 4, "getnUniformfv [false negative]");
	test_result = test_result && verifyError(gl.getError(), GL_NO_ERROR, "getnUniformfv [false negative]");

	gl.getnUniformfv(program.m_id, 11, sizeof(GLfloat) * 3, result4f);
	test_result = test_result && verifyError(gl.getError(), GL_INVALID_OPERATION, "getnUniformfv [false positive]");

	gl.getnUniformiv(program.m_id, 12, sizeof(GLint) * 3, result3i);
	test_result = test_result &&
				  verifyResult((void*)input3i, (void*)result3i, sizeof(GLint) * 3, "getnUniformiv [false negative]");
	test_result = test_result && verifyError(gl.getError(), GL_NO_ERROR, "getnUniformiv [false negative]");

	gl.getnUniformiv(program.m_id, 12, sizeof(GLint) * 2, result3i);
	test_result = test_result && verifyError(gl.getError(), GL_INVALID_OPERATION, "getnUniformiv [false positive]");

	gl.getnUniformuiv(program.m_id, 13, sizeof(GLuint) * 4, result4ui);
	test_result = test_result && verifyResult((void*)input4ui, (void*)result4ui, sizeof(GLuint) * 4,
											  "getnUniformuiv [false negative]");
	test_result = test_result && verifyError(gl.getError(), GL_NO_ERROR, "getnUniformuiv [false negative]");

	gl.getnUniformuiv(program.m_id, 13, sizeof(GLuint) * 3, result4ui);
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
{
	/* Nothing to be done here */
}

/** Execute test
 *
 * @return tcu::TestNode::STOP
 **/
tcu::TestNode::IterateResult ReadnPixelsTest::iterate()
{
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
							  "layout (location = 0) out lowp uvec4 out_fs_color;\n"
							  "\n"
							  "void main()\n"
							  "{\n"
							  "    out_fs_color = uvec4(1, 0, 0, 1);\n"
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
	static const GLuint height	 = 8;
	static const GLuint width	  = 8;
	static const GLuint pixel_size = 4 * sizeof(GLuint);

	const Functions& gl = m_context.getRenderContext().getFunctions();

	//Valid buffer size test
	const GLint bufSizeValid = width * height * pixel_size;
	GLubyte		pixelsValid[bufSizeValid];

	gl.readnPixels(0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, bufSizeValid, pixelsValid);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ReadnPixels");

	//Verify glReadnPixels result
	for (unsigned int i = 0; i < width * height; ++i)
	{
		const size_t offset = i * pixel_size;
		const GLuint value  = *(GLuint*)(pixelsValid + offset);

		if (value != 1)
		{
			m_context.getTestContext().getLog() << tcu::TestLog::Message << "Invalid pixel value: " << value
												<< ". Offset: " << offset << tcu::TestLog::EndMessage;
			return false;
		}
	}

	//Invalid buffer size test
	const GLint bufSizeInvalid = width * height * pixel_size - 1;
	GLubyte		pixelsInvalid[bufSizeInvalid];

	gl.readnPixels(0, 0, width, height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, bufSizeInvalid, pixelsInvalid);
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
		addChild(new RobustBufferAccessBehavior::GetnUniformTest(m_context));
		addChild(new RobustBufferAccessBehavior::ReadnPixelsTest(m_context));
	}
	catch (...)
	{
		// Destroy context.
		deqp::TestCaseGroup::deinit();
		throw;
	}
}

} // es32cts namespace
