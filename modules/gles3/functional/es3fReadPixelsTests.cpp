/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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
 * \brief Read pixels tests
 *//*--------------------------------------------------------------------*/

#include "es3fReadPixelsTests.hpp"

#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"

#include <cstring>
#include <sstream>

#include "glw.h"

using std::vector;

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace
{

class ReadPixelsTest : public TestCase
{
public:
					ReadPixelsTest	(Context& context, const char* name, const char* description, bool chooseFormat, int alignment, GLint rowLength, GLint skipRows, GLint skipPixels, GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE);

	IterateResult	iterate			(void);
	void			render			(tcu::Texture2D& reference);

private:
	int				m_seed;
	bool			m_chooseFormat;
	int				m_alignment;
	GLint			m_rowLength;
	GLint			m_skipRows;
	GLint			m_skipPixels;
	GLint			m_format;
	GLint			m_type;

	const int		m_width;
	const int		m_height;

	void			getFormatInfo	(tcu::TextureFormat& format, int& pixelSize, bool& align);
	void			clearColor		(tcu::Texture2D& reference, vector<deUint8>& pixelData, bool align, int pixelSize);
};

ReadPixelsTest::ReadPixelsTest	(Context& context, const char* name, const char* description, bool chooseFormat, int alignment, GLint rowLength, GLint skipRows, GLint skipPixels, GLenum format, GLenum type)
	: TestCase			(context, name, description)
	, m_seed			(deStringHash(name))
	, m_chooseFormat	(chooseFormat)
	, m_alignment		(alignment)
	, m_rowLength		(rowLength)
	, m_skipRows		(skipRows)
	, m_skipPixels		(skipPixels)
	, m_format			(format)
	, m_type			(type)
	, m_width			(13)
	, m_height			(13)
{
}

void ReadPixelsTest::render (tcu::Texture2D& reference)
{
	// Create program
	const char* vertexSource =
	"#version 300 es\n"
	"in mediump vec2 i_coord;\n"
	"void main (void)\n"
	"{\n"
	"\tgl_Position = vec4(i_coord, 0.0, 1.0);\n"
	"}\n";

	std::stringstream fragmentSource;


	fragmentSource <<
	"#version 300 es\n";

	if (reference.getFormat().type == tcu::TextureFormat::SIGNED_INT32)
		fragmentSource << "layout(location = 0) out mediump ivec4 o_color;\n";
	else if (reference.getFormat().type == tcu::TextureFormat::UNSIGNED_INT32)
		fragmentSource << "layout(location = 0) out mediump uvec4 o_color;\n";
	else
		fragmentSource << "layout(location = 0) out mediump vec4 o_color;\n";

	fragmentSource <<
	"void main (void)\n"
	"{\n";

	if (reference.getFormat().type == tcu::TextureFormat::UNSIGNED_INT32)
		fragmentSource << "\to_color = uvec4(0, 0, 0, 1000);\n";
	else if (reference.getFormat().type == tcu::TextureFormat::SIGNED_INT32)
		fragmentSource << "\to_color = ivec4(0, 0, 0, 1000);\n";
	else
		fragmentSource << "\to_color = vec4(0.0, 0.0, 0.0, 1.0);\n";

	fragmentSource <<
	"}\n";

	glu::ShaderProgram program(m_context.getRenderContext(), glu::makeVtxFragSources(vertexSource, fragmentSource.str()));

	m_testCtx.getLog() << program;
	TCU_CHECK(program.isOk());
	GLU_CHECK_CALL(glUseProgram(program.getProgram()));

	// Render
	{
		const float coords[] =
		{
			-0.5f, -0.5f,
			 0.5f, -0.5f,
			 0.5f,  0.5f,

			 0.5f,  0.5f,
			-0.5f,  0.5f,
			-0.5f, -0.5f
		};
		GLuint coordLoc;

		coordLoc = glGetAttribLocation(program.getProgram(), "i_coord");
		GLU_CHECK_MSG("glGetAttribLocation()");

		GLU_CHECK_CALL(glEnableVertexAttribArray(coordLoc));

		GLU_CHECK_CALL(glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, coords));

		GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
		GLU_CHECK_CALL(glDisableVertexAttribArray(coordLoc));
	}

	// Render reference

	const int coordX1 = (int)((-0.5f * reference.getWidth()		/ 2.0f) + reference.getWidth() / 2.0f);
	const int coordY1 = (int)((-0.5f * reference.getHeight()	/ 2.0f) + reference.getHeight() / 2.0f);
	const int coordX2 = (int)(( 0.5f * reference.getWidth()		/ 2.0f) + reference.getWidth() / 2.0f);
	const int coordY2 = (int)(( 0.5f * reference.getHeight()	/ 2.0f) + reference.getHeight() / 2.0f);

	for (int x = 0; x < reference.getWidth(); x++)
	{
		if (x < coordX1 || x > coordX2)
			continue;

		for (int y = 0; y < reference.getHeight(); y++)
		{
			if (y >= coordY1 && y <= coordY2)
			{
				if (reference.getFormat().type == tcu::TextureFormat::SIGNED_INT32)
					reference.getLevel(0).setPixel(tcu::IVec4(0, 0, 0, 1000), x, y);
				else if (reference.getFormat().type == tcu::TextureFormat::UNSIGNED_INT32)
					reference.getLevel(0).setPixel(tcu::UVec4(0, 0, 0, 1000), x, y);
				else
					reference.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), x, y);
			}
		}
	}
}

void ReadPixelsTest::getFormatInfo (tcu::TextureFormat& format, int& pixelSize, bool& align)
{
	if (m_chooseFormat)
	{
		GLU_CHECK_CALL(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &m_format));
		GLU_CHECK_CALL(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &m_type));
	}

	format = glu::mapGLTransferFormat(m_format, m_type);

	switch (m_type)
	{
		case GL_BYTE:
		case GL_UNSIGNED_BYTE:
		case GL_SHORT:
		case GL_UNSIGNED_SHORT:
		case GL_INT:
		case GL_UNSIGNED_INT:
		case GL_FLOAT:
		case GL_HALF_FLOAT:
			align = true;
			break;

		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
		case GL_UNSIGNED_INT_2_10_10_10_REV:
		case GL_UNSIGNED_INT_10F_11F_11F_REV:
		case GL_UNSIGNED_INT_24_8:
		case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
		case GL_UNSIGNED_INT_5_9_9_9_REV:
			align = false;
			break;

		default:
			throw tcu::InternalError("Unsupported format", "", __FILE__, __LINE__);
	}

	pixelSize = format.getPixelSize();
}

void ReadPixelsTest::clearColor (tcu::Texture2D& reference, vector<deUint8>& pixelData, bool align, int pixelSize)
{
	de::Random					rnd(m_seed);
	GLuint						framebuffer = 0;
	GLuint						renderbuffer = 0;

	if (m_format == GL_RGBA_INTEGER)
	{
		if (m_type == GL_UNSIGNED_INT)
		{
			GLU_CHECK_CALL(glGenRenderbuffers(1, &renderbuffer));
			GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));
			GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32UI, m_width, m_height));
		}
		else if (m_type == GL_INT)
		{
			GLU_CHECK_CALL(glGenRenderbuffers(1, &renderbuffer));
			GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer));
			GLU_CHECK_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32I, m_width, m_height));
		}
		else
			DE_ASSERT(false);

		GLU_CHECK_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
		GLU_CHECK_CALL(glGenFramebuffers(1, &framebuffer));
		GLU_CHECK_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer));
		GLU_CHECK_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer));
	}
	else if (m_format == GL_RGBA || m_format == GL_BGRA || m_format == GL_RGB)
	{
		// Empty
	}
	else
		DE_ASSERT(false);

	GLU_CHECK_CALL(glViewport(0, 0, reference.getWidth(), reference.getHeight()));

	// Clear color
	if (m_format == GL_RGBA || m_format == GL_BGRA || m_format == GL_RGB)
	{
		const float red		= rnd.getFloat();
		const float green	= rnd.getFloat();
		const float blue	= rnd.getFloat();
		const float alpha	= rnd.getFloat();

		const GLfloat color[] = { red, green, blue, alpha };
		// Clear target
		GLU_CHECK_CALL(glClearColor(red, green, blue, alpha));
		m_testCtx.getLog() << tcu::TestLog::Message << "ClearColor: (" << red << ", " << green << ", " << blue << ")" << tcu::TestLog::EndMessage;

		GLU_CHECK_CALL(glClearBufferfv(GL_COLOR, 0, color));

		// Clear reference
		for (int x = 0; x < reference.getWidth(); x++)
			for (int y = 0; y < reference.getHeight(); y++)
					reference.getLevel(0).setPixel(tcu::UVec4((deUint32)(255.0f * red), (deUint32)(255.0f * green), (deUint32)(255.0f * blue), (deUint32)(255 * alpha)), x, y);
	}
	else if (m_format == GL_RGBA_INTEGER)
	{
		if (m_type == GL_INT)
		{
			const GLint red		= rnd.getUint32();
			const GLint green	= rnd.getUint32();
			const GLint blue	= rnd.getUint32();
			const GLint alpha	= rnd.getUint32();

			const GLint color[] = { red, green, blue, alpha };
			m_testCtx.getLog() << tcu::TestLog::Message << "ClearColor: (" << red << ", " << green << ", " << blue << ")" << tcu::TestLog::EndMessage;

			GLU_CHECK_CALL(glClearBufferiv(GL_COLOR, 0, color));

			// Clear reference
			for (int x = 0; x < reference.getWidth(); x++)
				for (int y = 0; y < reference.getHeight(); y++)
						reference.getLevel(0).setPixel(tcu::IVec4(red, green, blue, alpha), x, y);
		}
		else if (m_type == GL_UNSIGNED_INT)
		{
			const GLuint red	= rnd.getUint32();
			const GLuint green	= rnd.getUint32();
			const GLuint blue	= rnd.getUint32();
			const GLuint alpha	= rnd.getUint32();

			const GLuint color[] = { red, green, blue, alpha };
			m_testCtx.getLog() << tcu::TestLog::Message << "ClearColor: (" << red << ", " << green << ", " << blue << ")" << tcu::TestLog::EndMessage;

			GLU_CHECK_CALL(glClearBufferuiv(GL_COLOR, 0, color));

			// Clear reference
			for (int x = 0; x < reference.getWidth(); x++)
				for (int y = 0; y < reference.getHeight(); y++)
						reference.getLevel(0).setPixel(tcu::UVec4(red, green, blue, alpha), x, y);
		}
		else
			DE_ASSERT(false);
	}
	else
		DE_ASSERT(false);

	render(reference);

	const int rowWidth	= (m_rowLength == 0 ? m_width : m_rowLength) + m_skipPixels;
	const int rowPitch	= (align ? m_alignment * deCeilFloatToInt32(pixelSize * rowWidth / (float)m_alignment) : rowWidth * pixelSize);

	pixelData.resize(rowPitch * (m_height + m_skipRows), 0);

	GLU_CHECK_CALL(glReadPixels(0, 0, m_width, m_height, m_format, m_type, &(pixelData[0])));

	if (framebuffer)
		GLU_CHECK_CALL(glDeleteFramebuffers(1, &framebuffer));

	if (renderbuffer)
		GLU_CHECK_CALL(glDeleteRenderbuffers(1, &renderbuffer));
}

TestCase::IterateResult ReadPixelsTest::iterate (void)
{
	tcu::TextureFormat			format(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
	int							pixelSize;
	bool						align;

	getFormatInfo(format, pixelSize, align);
	m_testCtx.getLog() << tcu::TestLog::Message << "Format: " << glu::getPixelFormatStr(m_format) << ", Type: " << glu::getTypeStr(m_type) << tcu::TestLog::EndMessage;

	tcu::Texture2D reference(format, m_width, m_height);
	reference.allocLevel(0);

	GLU_CHECK_CALL(glPixelStorei(GL_PACK_ALIGNMENT, m_alignment));
	m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_ALIGNMENT: " << m_alignment << tcu::TestLog::EndMessage;

	GLU_CHECK_CALL(glPixelStorei(GL_PACK_ROW_LENGTH, m_rowLength));
	m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_ROW_LENGTH: " << m_rowLength << tcu::TestLog::EndMessage;

	GLU_CHECK_CALL(glPixelStorei(GL_PACK_SKIP_ROWS, m_skipRows));
	m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_SKIP_ROWS: " << m_skipRows << tcu::TestLog::EndMessage;

	GLU_CHECK_CALL(glPixelStorei(GL_PACK_SKIP_PIXELS, m_skipPixels));
	m_testCtx.getLog() << tcu::TestLog::Message << "GL_PACK_SKIP_PIXELS: " << m_skipPixels << tcu::TestLog::EndMessage;

	GLU_CHECK_CALL(glViewport(0, 0, m_width, m_height));

	vector<deUint8>	pixelData;
	clearColor(reference, pixelData, align, pixelSize);

	const int rowWidth	= (m_rowLength == 0 ? m_width : m_rowLength);
	const int rowPitch	= (align ? m_alignment * deCeilFloatToInt32(pixelSize * rowWidth / (float)m_alignment) : rowWidth * pixelSize);

	// \note GL_RGBA_INTEGER uses always renderbuffers that are never multisampled. Otherwise default framebuffer is used.
	if (m_format != GL_RGBA_INTEGER && m_context.getRenderTarget().getNumSamples() > 1)
	{
		const tcu::IVec4	formatBitDepths	= tcu::getTextureFormatBitDepth(format);
		const deUint8		redThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().redBits,		formatBitDepths.x()))));
		const deUint8		greenThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().greenBits,	formatBitDepths.y()))));
		const deUint8		blueThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().blueBits,		formatBitDepths.z()))));
		const deUint8		alphaThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().alphaBits,	formatBitDepths.w()))));

		if (tcu::bilinearCompare(m_testCtx.getLog(), "Result", "Result", reference.getLevel(0), tcu::PixelBufferAccess(format, m_width, m_height, 1, rowPitch, 0, &(pixelData[0])), tcu::RGBA(redThreshold, greenThreshold, blueThreshold, alphaThreshold), tcu::COMPARE_LOG_RESULT))
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		else
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}
	else
	{
		const tcu::IVec4	formatBitDepths	= tcu::getTextureFormatBitDepth(format);
		const float			redThreshold	= 2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().redBits,	formatBitDepths.x()));
		const float			greenThreshold	= 2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().greenBits,	formatBitDepths.y()));
		const float			blueThreshold	= 2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().blueBits,	formatBitDepths.z()));
		const float			alphaThreshold	= 2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().alphaBits,	formatBitDepths.w()));

		// Compare
		if (tcu::floatThresholdCompare(m_testCtx.getLog(), "Result", "Result", reference.getLevel(0), tcu::PixelBufferAccess(format, m_width, m_height, 1, rowPitch, 0, &(pixelData[pixelSize * m_skipPixels + m_skipRows * rowPitch])), tcu::Vec4(redThreshold, greenThreshold, blueThreshold, alphaThreshold), tcu::COMPARE_LOG_RESULT))
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		else
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}

	return STOP;
}

} // anonymous

ReadPixelsTests::ReadPixelsTests (Context& context)
	: TestCaseGroup(context, "read_pixels", "ReadPixel tests")
{
}

void ReadPixelsTests::init (void)
{
	{
		TestCaseGroup* group = new TestCaseGroup(m_context, "alignment", "Read pixels pack alignment parameter tests");

		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_1", "", false, 1, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_2", "", false, 2, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_4", "", false, 4, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_8", "", false, 8, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));

		group->addChild(new ReadPixelsTest(m_context, "rgba_int_1", "", false, 1, 0, 0, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_2", "", false, 2, 0, 0, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_4", "", false, 4, 0, 0, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_8", "", false, 8, 0, 0, 0, GL_RGBA_INTEGER, GL_INT));

		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_1", "", false, 1, 0, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_2", "", false, 2, 0, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_4", "", false, 4, 0, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_8", "", false, 8, 0, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));

		group->addChild(new ReadPixelsTest(m_context, "choose_1", "", true, 1, 0, 0, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_2", "", true, 2, 0, 0, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_4", "", true, 4, 0, 0, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_8", "", true, 8, 0, 0, 0));

		addChild(group);
	}

	{
		TestCaseGroup* group = new TestCaseGroup(m_context, "rowlength", "Read pixels rowlength test");
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_17", "", false, 4, 17, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_19", "", false, 4, 19, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_23", "", false, 4, 23, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_29", "", false, 4, 29, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE));

		group->addChild(new ReadPixelsTest(m_context, "rgba_int_17", "", false, 4, 17, 0, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_19", "", false, 4, 19, 0, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_23", "", false, 4, 23, 0, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_29", "", false, 4, 29, 0, 0, GL_RGBA_INTEGER, GL_INT));

		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_17", "", false, 4, 17, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_19", "", false, 4, 19, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_23", "", false, 4, 23, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_29", "", false, 4, 29, 0, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));

		group->addChild(new ReadPixelsTest(m_context, "choose_17", "", true, 4, 17, 0, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_19", "", true, 4, 19, 0, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_23", "", true, 4, 23, 0, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_29", "", true, 4, 29, 0, 0));

		addChild(group);
	}

	{
		TestCaseGroup* group = new TestCaseGroup(m_context, "skip", "Read pixels skip pixels and rows test");
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_0_3", "", false, 4, 17, 0, 3, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_3_0", "", false, 4, 17, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_3_3", "", false, 4, 17, 3, 3, GL_RGBA, GL_UNSIGNED_BYTE));
		group->addChild(new ReadPixelsTest(m_context, "rgba_ubyte_3_5", "", false, 4, 17, 3, 5, GL_RGBA, GL_UNSIGNED_BYTE));

		group->addChild(new ReadPixelsTest(m_context, "rgba_int_0_3", "", false, 4, 17, 0, 3, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_3_0", "", false, 4, 17, 3, 0, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_3_3", "", false, 4, 17, 3, 3, GL_RGBA_INTEGER, GL_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_int_3_5", "", false, 4, 17, 3, 5, GL_RGBA_INTEGER, GL_INT));

		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_0_3", "", false, 4, 17, 0, 3, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_3_0", "", false, 4, 17, 3, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_3_3", "", false, 4, 17, 3, 3, GL_RGBA_INTEGER, GL_UNSIGNED_INT));
		group->addChild(new ReadPixelsTest(m_context, "rgba_uint_3_5", "", false, 4, 17, 3, 5, GL_RGBA_INTEGER, GL_UNSIGNED_INT));

		group->addChild(new ReadPixelsTest(m_context, "choose_0_3", "", true, 4, 17, 0, 3));
		group->addChild(new ReadPixelsTest(m_context, "choose_3_0", "", true, 4, 17, 3, 0));
		group->addChild(new ReadPixelsTest(m_context, "choose_3_3", "", true, 4, 17, 3, 3));
		group->addChild(new ReadPixelsTest(m_context, "choose_3_5", "", true, 4, 17, 3, 5));

		addChild(group);
	}
}

} // Functional
} // gles3
} // deqp
