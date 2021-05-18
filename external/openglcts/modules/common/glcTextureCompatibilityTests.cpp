/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2021 Google Inc.
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \file glcTexSubImageTests.cpp
 * \brief Texture compatibility tests
 */ /*-------------------------------------------------------------------*/

#include "glcTextureCompatibilityTests.hpp"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

#include "gluDefs.hpp"
#include "gluDrawUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"

#include <map>
#include <memory>
#include <vector>

namespace glcts
{
namespace
{
const float		vertexPositions[]	=
{
	-1.0f,	-1.0f,
	1.0f,	-1.0f,
	-1.0f,	1.0f,
	1.0f,	1.0f,
};

const float		vertexTexCoords[]	=
{
	0.0f,	0.0f,
	1.0f,	0.0f,
	0.0f,	1.0f,
	1.0f,	1.0f,
};

const char*		vertShader			=
	"${VERSION}\n"
	"in highp vec4 in_position;\n"
	"in highp vec2 in_texCoord;\n"
	"out highp vec2 v_texCoord;\n"
	"void main (void)\n"
	"{\n"
	"	gl_Position = in_position;\n"
	"	v_texCoord = in_texCoord;\n"
	"}\n";

const char*		fragShader			=
	"${VERSION}\n"
	"precision mediump float;\n"
	"uniform sampler2D texture0;\n"
	"in vec2 v_texCoord;\n"
	"out vec4 color;\n"
	"void main(void)\n"
	"{\n"
	"	color = texture(texture0, v_texCoord);\n"
	"}";

struct TestParameters
{
	std::string		testName;
	glw::GLenum		internalFormat;
	glw::GLenum		format;
	glw::GLenum		testType;
};

static const TestParameters testParameters[] =
{
	{ "rgba4_unsigned_byte",		GL_RGBA4,				GL_RGBA,			GL_UNSIGNED_BYTE				},
	{ "rgb5_a1_unsigned_byte",		GL_RGB5_A1,				GL_RGBA,			GL_UNSIGNED_BYTE				},
	{ "rgb5_a1_unsigned_int_10_a2",	GL_RGB5_A1,				GL_RGBA,			GL_UNSIGNED_INT_2_10_10_10_REV	},
	{ "r16f_float",					GL_R16F,				GL_RED,				GL_FLOAT						},
	{ "rg16f_float",				GL_RG16F,				GL_RG,				GL_FLOAT						},
	{ "rgb16f_float",				GL_RGB16F,				GL_RGB,				GL_FLOAT						},
	{ "rgba16f_float",				GL_RGBA16F,				GL_RGBA,			GL_FLOAT						},
	{ "r11f_g11f_b10f_half_float",	GL_R11F_G11F_B10F,		GL_RGB,				GL_HALF_FLOAT					},
	{ "r11f_g11f_b10f_float",		GL_R11F_G11F_B10F,		GL_RGB,				GL_FLOAT						},
	{ "rgb9_e5_half_float",			GL_RGB9_E5,				GL_RGB,				GL_HALF_FLOAT					},
	{ "rgb9_e5_float",				GL_RGB9_E5,				GL_RGB,				GL_FLOAT						},
	{ "rgb565_unsigned_byte",		GL_RGB565,				GL_RGB,				GL_UNSIGNED_BYTE				},
	{ "depth_component16_uint",		GL_DEPTH_COMPONENT16,	GL_DEPTH_COMPONENT,	GL_UNSIGNED_INT					}
};

class SubImageFormatTest : public deqp::TestCase
{
public:
	SubImageFormatTest(deqp::Context& context, const TestParameters testParam, glw::GLsizei textureSize);

	virtual ~SubImageFormatTest();

	virtual void			init		(void);
	virtual void			deinit		(void);

	virtual IterateResult	iterate		(void);

private:
	void setTextureParameters			(glw::GLenum target);
	void drawTexture					(void);

	void initializeProgram();
	void setVertexBufferObjects();
	void setVertexArrayObjects();

	std::shared_ptr<glu::ShaderProgram>	m_program;

	TestParameters						m_testParams;
	glw::GLsizei						m_textureSize;
	glw::GLuint							m_texId;
	glw::GLuint							m_vaoId;
	glw::GLenum							m_vboIds[2];
};

SubImageFormatTest::SubImageFormatTest	(deqp::Context& context, const TestParameters testParams, glw::GLsizei textureSize)
	: deqp::TestCase					(context, ("texsubimage_format_" + testParams.testName).c_str(), "Pass glTexSubImage with different client format to glTexImage")
	, m_testParams						(testParams)
	, m_textureSize						(textureSize)
	, m_texId							(0)
	, m_vaoId							(0)
	, m_vboIds							{ 0, 0 }
{
}

SubImageFormatTest::~SubImageFormatTest()
{
}

void SubImageFormatTest::init(void)
{
	initializeProgram();
	setVertexBufferObjects();
	setVertexArrayObjects();
}

void SubImageFormatTest::deinit(void)
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	gl.deleteTextures(1, &m_texId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures() failed");

	gl.deleteBuffers(1, &m_vaoId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers() failed");

	gl.deleteBuffers(DE_LENGTH_OF_ARRAY(m_vboIds), m_vboIds);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers() failed");
}

SubImageFormatTest::IterateResult SubImageFormatTest::iterate(void)
{
	const auto&			gl					= m_context.getRenderContext().getFunctions();

	m_context.getTestContext().setTestResult(QP_TEST_RESULT_FAIL, "Fail");

	// Render buffer
	glw::GLuint			rboId;
	gl.genRenderbuffers(1, &rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenRenderbuffers() failed");
	gl.bindRenderbuffer(GL_RENDERBUFFER, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");
	gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, m_textureSize, m_textureSize);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glRenderbufferStorage() failed");
	gl.bindRenderbuffer(GL_RENDERBUFFER, 0);

	// Frame buffer
	glw::GLuint			fboId;
	gl.genFramebuffers(1, &fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenFramebuffers() failed");
	gl.bindFramebuffer(GL_FRAMEBUFFER, fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindFramebuffer() failed");
	gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glFramebufferRenderbuffer() failed");

	gl.viewport(0, 0, m_textureSize, m_textureSize);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport() failed");

	gl.disable(GL_BLEND);

	gl.bindTexture(GL_TEXTURE_2D, m_texId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");

	gl.bindVertexArray(m_vaoId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray() failed");

	// Create main texture.
	tcu::TextureFormat	refTexFormat		(glu::mapGLInternalFormat(m_testParams.internalFormat));
	glu::TransferFormat	refTransferFormat	= glu::getTransferFormat(refTexFormat);
	tcu::Texture2D		refTexture			(refTexFormat, m_textureSize, m_textureSize, 1);

	refTexture.allocLevel(0);
	tcu::fillWithComponentGradients(refTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	// Create variables for reference sub texture.
	tcu::TextureFormat	refTexFormatSub		= glu::mapGLInternalFormat(m_testParams.internalFormat);
	tcu::Texture2D		refSubTexture		(refTexFormatSub, m_textureSize / 2, m_textureSize / 2, 1);
	tcu::Surface		refSurface			(m_textureSize, m_textureSize);

	refSubTexture.allocLevel(0);
	tcu::fillWithComponentGradients(refSubTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	// Upload the main texture data.
	gl.texImage2D(GL_TEXTURE_2D, 0, m_testParams.internalFormat, m_textureSize, m_textureSize, 0, refTransferFormat.format, refTransferFormat.dataType, refTexture.getLevel(0).getDataPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), ("gltexImage2D() failed" + glu::getTextureFormatStr(m_testParams.internalFormat).toString()).c_str());

	// Update the content of a previously allocated texture with reference sub texture.
	// Reference sub texture is using the same format and data type than main texture.
	gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_textureSize / 2, m_textureSize / 2, refTransferFormat.format, refTransferFormat.dataType, refSubTexture.getLevel(0).getDataPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), ("gltexSubImage2D() failed" + glu::getTextureFormatStr(m_testParams.internalFormat).toString()).c_str());

	setTextureParameters(GL_TEXTURE_2D);

	drawTexture();

	m_context.getTestContext().getLog() << tcu::TestLog::Message << m_testParams.testName.c_str() << " ("
		<< m_textureSize << " x " << m_textureSize << ")" << tcu::TestLog::EndMessage;

	// Read rendered pixels to reference surface.
	glu::readPixels(m_context.getRenderContext(), 0, 0, refSurface.getAccess());

	// Create variables for test sub texture.
	tcu::TextureFormat	testTexFormatSub	= glu::mapGLTransferFormat(m_testParams.format, m_testParams.testType);
	tcu::Texture2D		testSubTexture		(testTexFormatSub, m_textureSize / 2, m_textureSize / 2, 1);
	tcu::Surface		testSurface			(m_textureSize, m_textureSize);

	testSubTexture.allocLevel(0);
	tcu::fillWithComponentGradients(testSubTexture.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	// Upload the main texture data again.
	gl.texImage2D(GL_TEXTURE_2D, 0, m_testParams.internalFormat, m_textureSize, m_textureSize, 0, refTransferFormat.format, refTransferFormat.dataType, refTexture.getLevel(0).getDataPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), ("gltexImage2D() failed" + glu::getTextureFormatStr(m_testParams.internalFormat).toString()).c_str());

	// Update the content of a previously allocated texture with sub texture but different data type.
	gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_textureSize / 2, m_textureSize / 2, m_testParams.format, m_testParams.testType, testSubTexture.getLevel(0).getDataPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), ("gltexSubImage2D() failed" + glu::getTextureFormatStr(m_testParams.internalFormat).toString()).c_str());

	setTextureParameters(GL_TEXTURE_2D);

	drawTexture();

	// Read rendered pixels to test surface.
	readPixels(m_context.getRenderContext(), 0, 0, testSurface.getAccess());

	// Execute the comparison.
	if (tcu::fuzzyCompare(m_testCtx.getLog(), "texsubimage_format_", "Pass glTexSubImage with different client format to glTexImage",
		refSurface, testSurface, 0.001f, tcu::COMPARE_LOG_RESULT))
	{
		m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}

	// Cleanup
	gl.bindTexture(GL_TEXTURE_2D, 0);
	gl.bindFramebuffer(GL_FRAMEBUFFER, fboId);
	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	gl.bindFramebuffer(GL_FRAMEBUFFER, 0);
	gl.deleteFramebuffers(1, &fboId);

	return IterateResult::STOP;
}

void SubImageFormatTest::setTextureParameters(glw::GLenum target)
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");

	gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");

	gl.texParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");

	gl.texParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");

	gl.texParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");

	gl.texParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");

	gl.texParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
}

void SubImageFormatTest::drawTexture(void)
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram() failed");

	gl.clearColor(1.0f, 0.0f, 1.0f, 1.0f);
	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glClear() failed");

	gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays() failed");

	gl.finish();
}

void SubImageFormatTest::setVertexBufferObjects()
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	gl.genBuffers(DE_LENGTH_OF_ARRAY(m_vboIds), m_vboIds);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers() failed");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(vertexPositions), vertexPositions, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData() failed");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(vertexTexCoords), vertexTexCoords, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData() failed");

	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");
}

void SubImageFormatTest::setVertexArrayObjects()
{
	const auto&	gl			= m_context.getRenderContext().getFunctions();
	const auto	program		= m_program->getProgram();
	const auto	positionLoc	= gl.getAttribLocation(program, "in_position");
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation() failed");

	const auto	texCoordLoc	= gl.getAttribLocation(program, "in_texCoord");
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation() failed");

	gl.genVertexArrays(1, &m_vaoId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenVertexArrays() failed");

	gl.bindVertexArray(m_vaoId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray() failed");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vboIds[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");

	gl.enableVertexAttribArray(positionLoc);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray() failed");

	gl.vertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer() failed");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vboIds[1]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");

	gl.enableVertexAttribArray(texCoordLoc);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glEnableVertexAttribArray() failed");

	gl.vertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glVertexAttribPointer() failed");

	gl.bindVertexArray(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray() failed");
}

void SubImageFormatTest::initializeProgram()
{
	const auto&	gl				= m_context.getRenderContext().getFunctions();

	gl.genTextures(1, &m_texId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

	const bool	supportsES32	= glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::es(3, 2));
	const auto	glslVersion		= glu::getGLSLVersionDeclaration(supportsES32 ? glu::GLSLVersion::GLSL_VERSION_320_ES : glu::GLSLVersion::GLSL_VERSION_310_ES);
	const auto	args			= std::map<std::string, std::string>{ { "VERSION", glslVersion } };
	const auto	vs				= tcu::StringTemplate(vertShader).specialize(args);
	const auto	fs				= tcu::StringTemplate(fragShader).specialize(args);

	m_program = std::make_shared<glu::ShaderProgram>(m_context.getRenderContext(),
		glu::ProgramSources() << glu::VertexSource(vs) << glu::FragmentSource(fs));

	if (!m_program->isOk())
		throw std::runtime_error("Compiling shader program failed.");
}

} // <anonymous>

TextureCompatibilityTests::TextureCompatibilityTests(deqp::Context& context)
	: deqp::TestCaseGroup(context, "texture_compatibility", "Tests for texture format compatibility")
{
}

TextureCompatibilityTests::~TextureCompatibilityTests(void)
{
}

void TextureCompatibilityTests::init(void)
{
	for (const auto& test : testParameters)
		addChild(new SubImageFormatTest(m_context, test, 32));
}

} // glcts
