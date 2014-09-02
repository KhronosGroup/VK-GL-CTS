/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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

#include "es2fReadPixelsTests.hpp"

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

#include "glw.h"

#include <cstring>

namespace deqp
{
namespace gles2
{
namespace Functional
{

class ReadPixelsTest : public TestCase
{
public:
					ReadPixelsTest	(Context& context, const char* name, const char* description, bool chooseFormat, int alignment);

	IterateResult	iterate			(void);
	void			render			(tcu::Texture2D& reference);

private:
	bool			m_chooseFormat;
	int				m_alignment;
	int				m_seed;

	void			getFormatInfo	(tcu::TextureFormat& format, GLint& glFormat, GLint& glType, int& pixelSize, bool& align);
};

ReadPixelsTest::ReadPixelsTest	(Context& context, const char* name, const char* description, bool chooseFormat, int alignment)
	: TestCase			(context, name, description)
	, m_chooseFormat	(chooseFormat)
	, m_alignment		(alignment)
	, m_seed			(deStringHash(name))
{
}

void ReadPixelsTest::render (tcu::Texture2D& reference)
{
	// Create program
	const char* vertexSource =
	"attribute mediump vec2 a_coord;\n"
	"void main (void)\n"
	"{\n"
	"\tgl_Position = vec4(a_coord, 0.0, 1.0);\n"
	"}\n";

	const char* fragmentSource =
	"void main (void)\n"
	"{\n"
	"\tgl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
	"}\n";

	glu::ShaderProgram program(m_context.getRenderContext(), glu::makeVtxFragSources(vertexSource, fragmentSource));

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

		coordLoc = glGetAttribLocation(program.getProgram(), "a_coord");
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
				reference.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), x, y);
		}
	}
}

void ReadPixelsTest::getFormatInfo (tcu::TextureFormat& format, GLint& glFormat, GLint& glType, int& pixelSize, bool& align)
{
	if (m_chooseFormat)
	{
		GLU_CHECK_CALL(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &glFormat));
		GLU_CHECK_CALL(glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &glType));

		format = glu::mapGLTransferFormat(glFormat, glType);

		// Check if aligment is allowed
		switch (glType)
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

		pixelSize	= format.getPixelSize();
	}
	else
	{
		format		= tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
		pixelSize	= 1 * 4;
		align		= true;
		glFormat	= GL_RGBA;
		glType		= GL_UNSIGNED_BYTE;
	}
}

TestCase::IterateResult ReadPixelsTest::iterate (void)
{
	// Create reference
	const int					width	= 13;
	const int					height	= 13;

	de::Random					rnd(m_seed);

	tcu::TextureFormat			format(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
	int							pixelSize;
	GLint						glFormat;
	GLint						glType;
	bool						align;

	getFormatInfo(format, glFormat, glType, pixelSize, align);
	m_testCtx.getLog() << tcu::TestLog::Message << "Format: " << glu::getPixelFormatStr(glFormat) << ", Type: " << glu::getTypeStr(glType) << tcu::TestLog::EndMessage;

	tcu::Texture2D reference(format, width, height);
	reference.allocLevel(0);

	GLU_CHECK_CALL(glViewport(0, 0, width, height));

	// Clear color
	{
		const float red		= rnd.getFloat();
		const float green	= rnd.getFloat();
		const float blue	= rnd.getFloat();
		const float alpha	= 1.0f;

		m_testCtx.getLog() << tcu::TestLog::Message << "Clear color: (" << red << ", " << green << ", " << blue << ", " << alpha << ")" << tcu::TestLog::EndMessage;

		// Clear target
		GLU_CHECK_CALL(glClearColor(red, green, blue, alpha));
		GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

		// Clear reference
		for (int x = 0; x < reference.getWidth(); x++)
			for (int y = 0; y < reference.getHeight(); y++)
					reference.getLevel(0).setPixel(tcu::Vec4(red, green, blue, alpha), x, y);
	}

	render(reference);

	std::vector<deUint8> pixelData;
	const int rowPitch = (align ? m_alignment * deCeilFloatToInt32(pixelSize * width / (float)m_alignment) : width * pixelSize);

	pixelData.resize(rowPitch * height, 0);

	GLU_CHECK_CALL(glPixelStorei(GL_PACK_ALIGNMENT, m_alignment));
	GLU_CHECK_CALL(glReadPixels(0, 0, width, height, glFormat, glType, &(pixelData[0])));

	if (m_context.getRenderTarget().getNumSamples() > 1)
	{
		const tcu::IVec4	formatBitDepths	= tcu::getTextureFormatBitDepth(format);
		const deUint8		redThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().redBits,		formatBitDepths.x()))));
		const deUint8		greenThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().greenBits,	formatBitDepths.y()))));
		const deUint8		blueThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().blueBits,		formatBitDepths.z()))));
		const deUint8		alphaThreshold	= (deUint8)deCeilFloatToInt32(256.0f * (2.0f / (1 << deMin32(m_context.getRenderTarget().getPixelFormat().alphaBits,	formatBitDepths.w()))));

		if (tcu::bilinearCompare(m_testCtx.getLog(), "Result", "Result", reference.getLevel(0), tcu::PixelBufferAccess(format, width, height, 1, rowPitch, 0, &(pixelData[0])), tcu::RGBA(redThreshold, greenThreshold, blueThreshold, alphaThreshold), tcu::COMPARE_LOG_RESULT))
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
		if (tcu::floatThresholdCompare(m_testCtx.getLog(), "Result", "Result", reference.getLevel(0), tcu::PixelBufferAccess(format, width, height, 1, rowPitch, 0, &(pixelData[0])), tcu::Vec4(redThreshold, greenThreshold, blueThreshold, alphaThreshold), tcu::COMPARE_LOG_RESULT))
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
		else
			m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
	}

	return STOP;
}

ReadPixelsTests::ReadPixelsTests (Context& context)
	: TestCaseGroup(context, "read_pixels", "ReadPixel tests")
{
}

void ReadPixelsTests::init (void)
{
	addChild(new ReadPixelsTest(m_context, "rgba_ubyte_align_1", "", false, 1));
	addChild(new ReadPixelsTest(m_context, "rgba_ubyte_align_2", "", false, 2));
	addChild(new ReadPixelsTest(m_context, "rgba_ubyte_align_4", "", false, 4));
	addChild(new ReadPixelsTest(m_context, "rgba_ubyte_align_8", "", false, 8));

	addChild(new ReadPixelsTest(m_context, "choose_align_1", "", true, 1));
	addChild(new ReadPixelsTest(m_context, "choose_align_2", "", true, 2));
	addChild(new ReadPixelsTest(m_context, "choose_align_4", "", true, 4));
	addChild(new ReadPixelsTest(m_context, "choose_align_8", "", true, 8));
}

} // Functional
} // gles2
} // deqp
