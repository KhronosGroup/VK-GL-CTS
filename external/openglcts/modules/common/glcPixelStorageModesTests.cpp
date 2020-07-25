/*-------------------------------------------------------------------------
* OpenGL Conformance Test Suite
* -----------------------------
*
* Copyright (c) 2020 The Khronos Group Inc.
* Copyright (c) 2020 Intel Corporation
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
* \file  glcPixelStorageModesTests.cpp
* \brief Conformance tests for usage of pixel storage modes.
*/ /*-------------------------------------------------------------------*/

#include "stdlib.h"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuDefs.hpp"
#include "tcuFloat.hpp"
#include "tcuStringTemplate.hpp"
#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "gluContextInfo.hpp"
#include "gluObjectWrapper.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "gluTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluDrawUtil.hpp"
#include "gluDefs.hpp"
#include "sglrGLContext.hpp"
#include "sglrContextWrapper.hpp"
#include "sglrContextUtil.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "glsTextureTestUtil.hpp"
#include "glcPixelStorageModesTests.hpp"

#include <algorithm>

namespace glcts
{

static const char* const vs_template_src =
	"${GLSL_VERSION}\n"
	"in highp vec4 pos;\n"
	"out highp ${TEXCOORDS_TYPE} texcoords;\n"
	"${LAYER}\n"
	"void main (void)\n"
	"{\n"
	"	 texcoords = ${TEXCOORDS};\n"
	"	 gl_Position = pos;\n"
	"}\n";

static const char* const fs_template_src =
	"${GLSL_VERSION}\n"
	"precision highp float;\n"
	"precision highp int;\n"
	"out vec4 fragColour;\n"
	"in ${TEXCOORDS_TYPE} texcoords;\n"
	"uniform highp ${SAMPLER_TYPE} sampler;\n"
	"uniform ${COL_TYPE} refcolour;\n"
	"void main (void)\n"
	"{\n"
	"	 ${COL_TYPE} colour = texelFetch(sampler, i${TEXCOORDS_TYPE}(texcoords), 0);\n"
	"	 if (${CONDITION})\n"
	"		 fragColour = vec4(0.0, 1.0, 0.0, 1.0);\n"
	"	 else\n"
	"		 fragColour = vec4(colour);\n"
	"}\n";

double getEps(deUint32 internalFormat)
{
	double eps = 0.0;
	switch (internalFormat)
	{
	case GL_RGBA4:
		eps = 1.0 / (double)(1 << 4);
		break;
	case GL_RGB565:
	case GL_RGB5_A1:
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		eps = 1.0 / (double)(1 << 5);
		break;
	case GL_R8:
	case GL_R8_SNORM:
	case GL_RG8:
	case GL_RG8_SNORM:
	case GL_RGB8:
	case GL_SRGB8:
	case GL_RGB8_SNORM:
	case GL_RGBA8:
	case GL_SRGB8_ALPHA8:
	case GL_RGBA8_SNORM:
		eps = 1.0 / (double)(1 << 8);
		break;
	case GL_RGB9_E5:
		eps = 1.0 / (double)(1 << 9);
		break;
	case GL_R11F_G11F_B10F:
	case GL_RGB10_A2:
		eps = 1.0 / (double)(1 << 10);
		break;
	case GL_R16F:
	case GL_RG16F:
	case GL_RGB16F:
	case GL_RGBA16F:
	case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
		eps = 1.0 / (double)(1 << 16);
		break;
	case GL_R32F:
	case GL_RG32F:
	case GL_RGB32F:
	case GL_RGBA32F:
		eps = 1.0 / (double)(1 << 31);
		break;
	default:
		TCU_FAIL("Invalid internal format");
		break;
	}

	return std::max(0.01, eps);
}

bool inrange(int x, int left, int right)
{
	return (x >= left && x < right);
}

class TexImageUtils
{
public:
	TexImageUtils(deUint32 internalFormat,
				  int cuboid_w, int cuboid_h, int cuboid_d,
				  int subcuboid_x0, int subcuboid_y0, int subcuboid_z0,
				  int subcuboid_w, int subcuboid_h, int subcuboid_d,
				  glu::GLSLVersion glsl_version);
	~TexImageUtils (void);
protected:
	void writePixel(glw::GLubyte *p, glw::GLdouble col);
	void writeChannel(glw::GLubyte *p, int channel, glw::GLdouble col);
	template <typename T>
	void writeToUnsignedChannel(glw::GLubyte *p, int channel, glw::GLdouble col);
	template <typename T>
	void writeToSignedChannel(glw::GLubyte *p, int channel, glw::GLdouble col);
	void writeToFloatChannel(glw::GLubyte *p, int channel, glw::GLdouble col);
	void writeToHalfFloatChannel(glw::GLubyte *p, int channel, glw::GLdouble col);
	template <typename T,
				 unsigned int size_1, unsigned int size_2, unsigned int size_3,
				 unsigned int off_1, unsigned int off_2, unsigned int off_3>
	void write3Channel(glw::GLubyte *p, int channel, glw::GLdouble col);
	template <typename T,
				 unsigned int size_1, unsigned int size_2,
				 unsigned int size_3, unsigned int size_4,
				 unsigned int off_1, unsigned int off_2,
				 unsigned int off_3, unsigned int off_4>
	void write4Channel(glw::GLubyte *p, int channel, glw::GLdouble col);
	void write11F_11F_10F_Channel(glw::GLubyte *p, int channel, glw::GLdouble col);
	void setRefcolour (glu::CallLogWrapper gl, glw::GLdouble col);
	template <typename T>
	void setUnsignedRefcolour(glu::CallLogWrapper gl, glw::GLdouble col);
	template <typename T>
	void setSignedRefcolour(glu::CallLogWrapper gl, glw::GLdouble col);
	void setRGB10A2Refcolour (glu::CallLogWrapper gl, glw::GLdouble col);
	bool verify(tcu::Surface dst, tcu::Surface *errMask);

	glw::GLubyte *m_src_data;
	deUint32 tex;
	glu::ShaderProgram* prog;

	deUint32 m_internalFormat;
	deUint32 m_format;
	deUint32 m_type;
	int m_pixelsize;
	int m_num_channels;
	int m_cuboid_w;
	int m_cuboid_h;
	int m_cuboid_d;
	int m_subcuboid_x0;
	int m_subcuboid_y0;
	int m_subcuboid_z0;
	int m_subcuboid_w;
	int m_subcuboid_h;
	int m_subcuboid_d;

	glu::GLSLVersion m_glsl_version;
};

TexImageUtils::TexImageUtils (deUint32 internalFormat,
							  int cuboid_w,
							  int cuboid_h,
							  int cuboid_d,
							  int subcuboid_x0,
							  int subcuboid_y0,
							  int subcuboid_z0,
							  int subcuboid_w,
							  int subcuboid_h,
							  int subcuboid_d,
							  glu::GLSLVersion glsl_version)
	: m_internalFormat(internalFormat)
	, m_format(glu::getTransferFormat(glu::mapGLInternalFormat(internalFormat)).format)
	, m_type(glu::getTransferFormat(glu::mapGLInternalFormat(internalFormat)).dataType)
	, m_pixelsize(tcu::getPixelSize(glu::mapGLInternalFormat(internalFormat)))
	, m_num_channels(tcu::getNumUsedChannels(glu::mapGLInternalFormat(internalFormat).order))
	, m_cuboid_w(cuboid_w)
	, m_cuboid_h(cuboid_h)
	, m_cuboid_d(cuboid_d)
	, m_subcuboid_x0(subcuboid_x0)
	, m_subcuboid_y0(subcuboid_y0)
	, m_subcuboid_z0(subcuboid_z0)
	, m_subcuboid_w(subcuboid_w)
	, m_subcuboid_h(subcuboid_h)
	, m_subcuboid_d(subcuboid_d)
	, m_glsl_version(glsl_version)
{
}

TexImageUtils::~TexImageUtils (void)
{
}

void TexImageUtils::writePixel(glw::GLubyte *p, glw::GLdouble col)
{
	for (int ch = 0; ch < m_num_channels; ch++)
		writeChannel(p, ch, (ch == 3) ? 1.0 : col);
}

void TexImageUtils::writeChannel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	switch (m_type)
	{
	case GL_UNSIGNED_BYTE:
		writeToUnsignedChannel<glw::GLubyte>(p, channel, col);
		break;
	case GL_BYTE:
		writeToSignedChannel<glw::GLbyte>(p, channel, col);
		break;
	case GL_UNSIGNED_SHORT:
		writeToUnsignedChannel<glw::GLushort>(p, channel, col);
		break;
	case GL_UNSIGNED_SHORT_5_6_5:
		write3Channel<glw::GLushort, 5, 6, 5, 11, 5, 0>(p, channel, col);
		break;
	case GL_SHORT:
		writeToSignedChannel<glw::GLshort>(p, channel, col);
		break;
	case GL_UNSIGNED_INT:
		writeToUnsignedChannel<glw::GLuint>(p, channel, col);
		break;
	case GL_UNSIGNED_INT_2_10_10_10_REV:
		write4Channel<glw::GLuint, 2, 10, 10, 10, 30, 20, 10, 0>(p, 3 - channel, col);
		break;
	case GL_UNSIGNED_INT_10F_11F_11F_REV:
		write11F_11F_10F_Channel(p, channel, col);
		break;
	case GL_UNSIGNED_SHORT_4_4_4_4:
		write4Channel<glw::GLushort, 4, 4, 4, 4, 12, 8, 4, 0>(p, channel, col);
		break;
	case GL_UNSIGNED_SHORT_5_5_5_1:
		write4Channel<glw::GLushort, 5, 5, 5, 1, 11, 6, 1, 0>(p, channel, col);
		break;
	case GL_INT:
		writeToSignedChannel<glw::GLint>(p, channel, col);
		break;
	case GL_HALF_FLOAT:
		writeToHalfFloatChannel(p, channel, col);
		break;
	case GL_FLOAT:
		writeToFloatChannel(p, channel, col);
		break;
	default:
		TCU_FAIL("Invalid type");
		break;
	}
}

template <typename T>
void TexImageUtils::writeToUnsignedChannel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	static const T max = -1;

	const glw::GLdouble d_max = (glw::GLdouble)max;
	const glw::GLdouble d_value = col * d_max;
	const T t_value = (T)d_value;

	T* ptr = (T*)p;

	ptr[channel] = t_value;
}

template <typename T>
void TexImageUtils::writeToSignedChannel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	static const T max = (T)((1u << (sizeof(T) * 8u - 1u)) - 1u);

	const glw::GLdouble d_max = (glw::GLdouble)max;
	const glw::GLdouble d_value = col * d_max;
	const T t_value = (T)d_value;

	T* ptr = (T*)p;

	ptr[channel] = t_value;
}

void TexImageUtils::writeToFloatChannel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	const glw::GLfloat t_value = (glw::GLfloat)col;

	glw::GLfloat *ptr = (glw::GLfloat*)p;

	ptr[channel] = t_value;
}

void TexImageUtils::writeToHalfFloatChannel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	deUint16* ptr = (deUint16*)p;

	tcu::Float16 val(col);

	ptr[channel] = val.bits();
}

template <typename T,
			 unsigned int size_1, unsigned int size_2, unsigned int size_3,
			 unsigned int off_1, unsigned int off_2, unsigned int off_3>
void TexImageUtils::write3Channel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	T mask = 0;
	T max = 0;
	T off = 0;
	T* ptr = (T*)p;
	T result = 0;

	const T max_1 = (1 << size_1) - 1;
	const T max_2 = (1 << size_2) - 1;
	const T max_3 = (1 << size_3) - 1;

	switch (channel)
	{
	case 0:
		mask = max_1;
		max  = max_1;
		off  = off_1;
		break;
	case 1:
		mask = max_2;
		max  = max_2;
		off  = off_2;
		break;
	case 2:
		mask = max_3;
		max  = max_3;
		off  = off_3;
		break;
	default:
		TCU_FAIL("Invalid channel");
		break;
	}

	const glw::GLdouble d_max	 = (glw::GLdouble)max;
	const glw::GLdouble d_value  = col * d_max;
	const T t_value = (T)d_value;

	result = (T)((t_value & mask) << off);

	*ptr |= result;
}

template <typename T,
			 unsigned int size_1, unsigned int size_2,
			 unsigned int size_3, unsigned int size_4,
			 unsigned int off_1, unsigned int off_2,
			 unsigned int off_3, unsigned int off_4>
void TexImageUtils::write4Channel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	T mask	 = 0;
	T max	 = 0;
	T off	 = 0;
	T* ptr	 = (T*)p;
	T result = 0;

	T max_1 = (1 << size_1) - 1;
	T max_2 = (1 << size_2) - 1;
	T max_3 = (1 << size_3) - 1;
	T max_4 = (1 << size_4) - 1;

	switch (channel)
	{
	case 0:
		mask = max_1;
		max  = max_1;
		off  = off_1;
		break;
	case 1:
		mask = max_2;
		max  = max_2;
		off  = off_2;
		break;
	case 2:
		mask = max_3;
		max  = max_3;
		off  = off_3;
		break;
	case 3:
		mask = max_4;
		max  = max_4;
		off  = off_4;
		break;
	default:
		TCU_FAIL("Invalid channel");
		break;
	}

	const glw::GLdouble d_max	 = (glw::GLdouble)max;
	const glw::GLdouble d_value  = col * d_max;
	const T t_value = (T)d_value;

	result = (T)((t_value & mask) << off);

	*ptr |= result;
}

void TexImageUtils::write11F_11F_10F_Channel(glw::GLubyte *p, int channel, glw::GLdouble col)
{
	deUint32* ptr = (deUint32*)p;

	switch (channel)
	{
	case 0:
	{
		tcu::Float<deUint32, 5, 6, 15, tcu::FLOAT_SUPPORT_DENORM> val(col);
		deUint32 bits = val.bits();

		*ptr |= bits;
	}
	break;
	case 1:
	{
		tcu::Float<deUint32, 5, 6, 15, tcu::FLOAT_SUPPORT_DENORM> val(col);
		deUint32 bits = val.bits();

		*ptr |= (bits << 11);
	}
	break;
	case 2:
	{
		tcu::Float<deUint32, 5, 5, 15, tcu::FLOAT_SUPPORT_DENORM> val(col);
		deUint32 bits = val.bits();

		*ptr |= (bits << 22);
	}
	break;
	default:
		TCU_FAIL("Invalid channel");
		break;
	}
}

void TexImageUtils::setRefcolour (glu::CallLogWrapper gl, glw::GLdouble col)
{
	switch (m_format)
	{
	case GL_RED:
	case GL_RG:
	case GL_RGB:
	case GL_RGBA:
		gl.glUniform4f(gl.glGetUniformLocation(prog->getProgram(), "refcolour"),
					   m_num_channels > 0 ? col : 0.0f,
					   m_num_channels > 1 ? col : 0.0f,
					   m_num_channels > 2 ? col : 0.0f,
					   1.0f);
		break;
	default:
		switch (m_type)
		{
		case GL_UNSIGNED_BYTE:
			setUnsignedRefcolour<glw::GLubyte>(gl, col);
			break;
		case GL_BYTE:
			setSignedRefcolour<glw::GLubyte>(gl, col);
			break;
		case GL_UNSIGNED_SHORT:
		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
			setUnsignedRefcolour<glw::GLushort>(gl, col);
			break;
		case GL_SHORT:
			setSignedRefcolour<glw::GLushort>(gl, col);
			break;
		case GL_UNSIGNED_INT:
			setUnsignedRefcolour<glw::GLuint>(gl, col);
			break;
		case GL_UNSIGNED_INT_2_10_10_10_REV:
			setRGB10A2Refcolour(gl, col);
			break;
		case GL_INT:
			setSignedRefcolour<glw::GLuint>(gl, col);
			break;
		default:
			TCU_FAIL("Invalid type");
			break;
		}
	}
}

template <typename T>
void TexImageUtils::setUnsignedRefcolour (glu::CallLogWrapper gl, glw::GLdouble col)
{
	static const T max = -1;
	const glw::GLdouble d_max   = (glw::GLdouble)max;
	const glw::GLdouble d_value = d_max * col;
	const T t_value = (T)d_value;

	unsigned int refcol[4] =
	{
		m_num_channels > 0 ? t_value : 0u,
		m_num_channels > 1 ? t_value : 0u,
		m_num_channels > 2 ? t_value : 0u,
		255u,
	};

	gl.glUniform4uiv(gl.glGetUniformLocation(prog->getProgram(), "refcolour"), 1,
					 refcol);
}

template <typename T>
void TexImageUtils::setSignedRefcolour (glu::CallLogWrapper gl, glw::GLdouble col)
{
	static const T umax = -1;
	static const T max  = umax >> 1;

	const glw::GLdouble d_max   = (glw::GLdouble)max;
	const glw::GLdouble d_value = d_max * col;
	const T t_value = (T)d_value;

	int refcol[4] =
	{
		(m_num_channels > 0 ? (int)t_value : 0),
		(m_num_channels > 1 ? (int)t_value : 0),
		(m_num_channels > 2 ? (int)t_value : 0),
		255,
	};

	gl.glUniform4iv(gl.glGetUniformLocation(prog->getProgram(), "refcolour"), 1,
					refcol);
}

void TexImageUtils::setRGB10A2Refcolour (glu::CallLogWrapper gl, glw::GLdouble col)
{
	unsigned int max_channel_value = 1023u;

	const glw::GLdouble d_max_channel_value = (glw::GLdouble)max_channel_value;
	const glw::GLdouble d_value = (glw::GLdouble)d_max_channel_value * col;
	unsigned int t_value = (unsigned int)d_value;

	unsigned int refcol[4] =
	{
		(m_num_channels > 0 ? t_value : 0u),
		(m_num_channels > 1 ? t_value : 0u),
		(m_num_channels > 2 ? t_value : 0u),
		255u,
	};

	gl.glUniform4uiv(gl.glGetUniformLocation(prog->getProgram(), "refcolour"), 1,
					 refcol);
}

bool TexImageUtils::verify(tcu::Surface dst, tcu::Surface *errMask)
{
	*errMask = tcu::Surface (dst.getWidth(), dst.getHeight());
	tcu::clear(errMask->getAccess(), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
	bool pass = true;

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			if (dst.getPixel(x, y) != tcu::RGBA::green())
			{
				pass = false;
				errMask->setPixel(x, y, tcu::RGBA::red());
			}
		}
	}

	return pass;
}

class TexImage2DCase : public deqp::TestCase
					 , public sglr::ContextWrapper
					 , public TexImageUtils
{
public:
	TexImage2DCase (deqp::Context& context, const char* name, const char* desc,
					deUint32 internalFormat,
					int rect_w, int rect_h,
					int subrect_x0, int subrect_y0,
					int subrect_w, int subrect_h,
					glu::GLSLVersion glsl_version);
	~TexImage2DCase (void);
	IterateResult iterate (void);
protected:
	void generateSrcData();
	void createTexture (void);
	void createShader (void);
	tcu::Surface renderToSurf (void);
	void cleanup (void);
};

TexImage2DCase::TexImage2DCase (deqp::Context& context,
								const char* name,
								const char* desc,
								deUint32 internalFormat,
								int rect_w,
								int rect_h,
								int subrect_x0,
								int subrect_y0,
								int subrect_w,
								int subrect_h,
								glu::GLSLVersion glsl_version)
	: TestCase(context, name, desc)
	, TexImageUtils(internalFormat,
					rect_w, rect_h, 1,
					subrect_x0, subrect_y0, 0,
					subrect_w, subrect_h, 1,
					glsl_version)
{
}

TexImage2DCase::~TexImage2DCase(void)
{
}

TexImage2DCase::IterateResult TexImage2DCase::iterate(void)
{
	glu::RenderContext& renderCtx = TestCase::m_context.getRenderContext();
	tcu::TestLog& log = m_testCtx.getLog();
	tcu::Surface dst, errMask;

	bool pass = true;

	sglr::GLContext gl_ctx (renderCtx,
							log,
							sglr::GLCONTEXT_LOG_CALLS,
							tcu::IVec4(0, 0, m_subcuboid_w, m_subcuboid_h));

	setContext((sglr::Context*)&gl_ctx);

	generateSrcData();
	createTexture();
	createShader();
	dst = renderToSurf();

	pass = verify(dst, &errMask);

	cleanup();

	if (pass)
	{
		m_testCtx.getLog()
		<< tcu::TestLog::Message << "Image is valid" << tcu::TestLog::EndMessage
		<< tcu::TestLog::ImageSet("ImageVerification", "Image verification")
		<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
		<< tcu::TestLog::EndImageSet;
	}
	else
	{
		m_testCtx.getLog()
		<< tcu::TestLog::Message << "Image is invalid" << tcu::TestLog::EndMessage
		<< tcu::TestLog::ImageSet("ErrorVerification", "Image verification")
		<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
		<< tcu::TestLog::Image("ErrorMask", "Error mask", errMask.getAccess())
		<< tcu::TestLog::EndImageSet;
	}

	m_testCtx.setTestResult(pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
							pass ? "Pass" : "Fail");

	return STOP;
}

void TexImage2DCase::generateSrcData()
{
	m_src_data = new glw::GLubyte[m_cuboid_w * m_cuboid_h * m_pixelsize]();

	glw::GLdouble col = 0.0;

	for (int y = 0; y < m_cuboid_h; y++)
	{
		for (int x = 0; x < m_cuboid_w; x++)
		{
			if (inrange(y, m_subcuboid_y0, m_subcuboid_y0 + m_subcuboid_h) &&
				inrange(x, m_subcuboid_x0, m_subcuboid_x0 + m_subcuboid_w))
				col = 1.0;
			else
				col = 0.0;
			int offset = y * m_cuboid_w * m_pixelsize +
						 x * m_pixelsize;
			writePixel(m_src_data + offset, col);
		}
	}
}

void TexImage2DCase::createTexture (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glGenTextures(1, &tex);
	gl.glBindTexture(GL_TEXTURE_2D, tex);
	gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	m_cuboid_w);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,   m_subcuboid_y0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_subcuboid_x0);

	gl.glTexImage2D(GL_TEXTURE_2D,
					0,
					m_internalFormat,
					m_subcuboid_w,
					m_subcuboid_h,
					0,
					m_format,
					m_type,
					m_src_data);

	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 0);
}

void TexImage2DCase::createShader (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	const tcu::StringTemplate vs_src (vs_template_src);
	const tcu::StringTemplate fs_src (fs_template_src);

	std::map<std::string,std::string> params;
	params["GLSL_VERSION"]	 = getGLSLVersionDeclaration(m_glsl_version);
	params["TEXCOORDS_TYPE"] = "vec2";
	params["LAYER"]			 = "";
	params["TEXCOORDS"]		 = "pos.xy";
	params["CONDITION"]		 = "colour.rgb == refcolour.rgb";

	switch (m_format)
	{
	case GL_RED_INTEGER:
	case GL_RG_INTEGER:
	case GL_RGB_INTEGER:
	case GL_RGBA_INTEGER:
		switch (m_type)
		{
		case GL_BYTE:
		case GL_SHORT:
		case GL_INT:
			params["SAMPLER_TYPE"] = "isampler2D";
			params["COL_TYPE"]	 = "ivec4";
			break;
		default:
			params["SAMPLER_TYPE"] = "usampler2D";
			params["COL_TYPE"]	   = "uvec4";
			break;
		}
		break;
	default:
		params["SAMPLER_TYPE"] = "sampler2D";
		params["COL_TYPE"]	   = "vec4";
		break;
	}

	prog = new glu::ShaderProgram(m_context.getRenderContext(),
								  glu::ProgramSources() <<
								  glu::VertexSource(vs_src.specialize(params)) <<
								  glu::FragmentSource(fs_src.specialize(params)));

	if (!prog->isOk())
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << ""
			<< tcu::TestLog::EndMessage
			<< tcu::TestLog::ShaderProgram(false, "")
			<< tcu::TestLog::Shader(QP_SHADER_TYPE_VERTEX,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).infoLog)

			<< tcu::TestLog::Shader(QP_SHADER_TYPE_FRAGMENT,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).infoLog)
			<< tcu::TestLog::EndShaderProgram;
		TCU_FAIL("Shader creation failed");
	}

	gl.glUseProgram(prog->getProgram());
	gl.glUniform1i(gl.glGetUniformLocation(prog->getProgram(), "sampler"), 0);
}

tcu::Surface TexImage2DCase::renderToSurf (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());
	gl.glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	gl.glClear(GL_COLOR_BUFFER_BIT);

	static const float vertexPositions[4*3] =
	{
		-1.0, -1.0, -1.0f,
		 1.0, -1.0,  0.0f,
		-1.0,  1.0,  0.0f,
		 1.0,  1.0,  1.0f,
	};

	static const deUint16 indices[6] = { 0, 1, 2, 2, 1, 3 };

	const glu::VertexArrayBinding attrBindings[] =
	{
		glu::va::Float("pos", 3, 4, 0, &vertexPositions[0])
	};

	gl.glViewport(0, 0, m_subcuboid_w, m_subcuboid_h);
	setRefcolour(gl, 1.0);
	glu::draw(m_context.getRenderContext(),
			  prog->getProgram(),
			  DE_LENGTH_OF_ARRAY(attrBindings),
			  &attrBindings[0],
			  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));

	tcu::Surface dst;
	dst.setSize(m_subcuboid_w, m_subcuboid_h);

	glu::readPixels(m_context.getRenderContext(), 0, 0, dst.getAccess());

	return dst;
}

void TexImage2DCase::cleanup (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glDeleteTextures(1, &tex);
	delete[] m_src_data;
	delete prog;
}

class TexImage3DCase : public deqp::TestCase
					 , public sglr::ContextWrapper
					 , public TexImageUtils
{
public:
	TexImage3DCase (deqp::Context& context, const char* name, const char* desc,
					deUint32 internalFormat,
					int cuboid_w, int cuboid_h, int cuboid_d,
					int subcuboid_x0, int subrect_y0, int subcuboid_z0,
					int subcuboid_w, int subcuboid_h, int subcuboid_d,
					glu::GLSLVersion glsl_version);
	~TexImage3DCase (void);
	IterateResult iterate (void);
protected:
	void generateSrcData();
	void createTexture (void);
	void createShader (void);
	tcu::Surface renderToSurf (int layer);
	void cleanup (void);
};

TexImage3DCase::TexImage3DCase (deqp::Context& context,
								const char* name,
								const char* desc,
								deUint32 internalFormat,
								int cuboid_w,
								int cuboid_h,
								int cuboid_d,
								int subcuboid_x0,
								int subcuboid_y0,
								int subcuboid_z0,
								int subcuboid_w,
								int subcuboid_h,
								int subcuboid_d,
								glu::GLSLVersion glsl_version)
	: TestCase(context, name, desc)
	, TexImageUtils(internalFormat,
					cuboid_w, cuboid_h, cuboid_d,
					subcuboid_x0, subcuboid_y0, subcuboid_z0,
					subcuboid_w, subcuboid_h, subcuboid_d,
					glsl_version)
{
}

TexImage3DCase::~TexImage3DCase(void)
{
}

TexImage3DCase::IterateResult TexImage3DCase::iterate(void)
{
	glu::RenderContext& renderCtx = TestCase::m_context.getRenderContext();
	tcu::TestLog& log = m_testCtx.getLog();
	tcu::Surface dst, errMask;

	bool pass = true;

	sglr::GLContext gl_ctx (renderCtx,
							log,
							sglr::GLCONTEXT_LOG_CALLS,
							tcu::IVec4(0, 0, m_subcuboid_w, m_subcuboid_h));

	setContext((sglr::Context*)&gl_ctx);

	generateSrcData();
	createTexture();
	createShader();

	for (int z = 0; z < m_subcuboid_d; z++)
	{
		dst = renderToSurf(z);

		bool layer_pass = verify(dst, &errMask);

		if (layer_pass)
		{
			m_testCtx.getLog()
				<< tcu::TestLog::Message << "Layer " << z	 << " is valid"
				<< tcu::TestLog::EndMessage
				<< tcu::TestLog::ImageSet("LayerVerification", "Layer verification")
				<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
				<< tcu::TestLog::EndImageSet;
		}
		else
		{
			m_testCtx.getLog()
				<< tcu::TestLog::Message << "Layer " << z << " is invalid"
				<< tcu::TestLog::EndMessage
				<< tcu::TestLog::ImageSet("ErrorVerification", "Layer verification")
				<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
				<< tcu::TestLog::Image("ErrorMask", "Error mask", errMask.getAccess())
				<< tcu::TestLog::EndImageSet;
		}

		pass &= layer_pass;
	}

	cleanup();

	if (pass)
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << "Image is valid" << tcu::TestLog::EndMessage;
	}
	else
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << "Image is invalid" << tcu::TestLog::EndMessage;
	}

	m_testCtx.setTestResult(pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
									pass ? "Pass" : "Fail");

	return STOP;
}

void TexImage3DCase::generateSrcData()
{
	m_src_data = new glw::GLubyte[m_cuboid_w *
								  m_cuboid_h *
								  m_cuboid_d *
								  m_pixelsize]();

	glw::GLdouble col = 0.0;

	for (int z = 0; z < m_cuboid_d; z++)
	{
		for (int y = 0; y < m_cuboid_h; y++)
		{
			for (int x = 0; x < m_cuboid_w; x++)
			{
				if (inrange(z, m_subcuboid_z0, m_subcuboid_z0 + m_subcuboid_d) &&
					inrange(y, m_subcuboid_y0, m_subcuboid_y0 + m_subcuboid_h) &&
					inrange(x, m_subcuboid_x0, m_subcuboid_x0 + m_subcuboid_w))
					col = 0.125 + (z - m_subcuboid_z0) * 0.125; /* [0.125, 0.250..1.0] */
				else
					col = 0.0;
				int offset = z * m_cuboid_h * m_cuboid_w * m_pixelsize +
							 y * m_cuboid_w * m_pixelsize +
							 x * m_pixelsize;
				writePixel(m_src_data + offset, col);
			}
		}
	}
}

void TexImage3DCase::createTexture (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glGenTextures(1, &tex);
	gl.glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl.glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_cuboid_h);
	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 m_cuboid_w);
	gl.glPixelStorei(GL_UNPACK_SKIP_IMAGES,	 m_subcuboid_z0);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 m_subcuboid_y0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS,	 m_subcuboid_x0);

	gl.glTexImage3D(GL_TEXTURE_2D_ARRAY,
					0,
					m_internalFormat,
					m_subcuboid_w,
					m_subcuboid_h,
					m_subcuboid_d,
					0,
					m_format,
					m_type,
					m_src_data);

	gl.glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_IMAGES,  0);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS,  0);
}

void TexImage3DCase::createShader (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	const tcu::StringTemplate vs_src (vs_template_src);
	const tcu::StringTemplate fs_src (fs_template_src);

	std::map<std::string,std::string> params;
	params["GLSL_VERSION"]	 = getGLSLVersionDeclaration(m_glsl_version);
	params["TEXCOORDS_TYPE"] = "vec3";
	params["LAYER"]			 = "uniform int layer;";
	params["TEXCOORDS"]		 = "vec3(pos.xy, layer)";

	switch (m_format)
	{
	case GL_RED_INTEGER:
	case GL_RG_INTEGER:
	case GL_RGB_INTEGER:
	case GL_RGBA_INTEGER:
		switch (m_type)
		{
		case GL_BYTE:
		case GL_SHORT:
		case GL_INT:
			params["SAMPLER_TYPE"] = "isampler2DArray";
			params["COL_TYPE"]	 = "ivec4";
			params["CONDITION"]	 = "all(lessThan(uvec4(abs(colour - refcolour)).rgb, uvec3(2u)))";
			break;
		default:
			params["SAMPLER_TYPE"] = "usampler2DArray";
			params["COL_TYPE"]	   = "uvec4";
			params["CONDITION"]	   = "all(lessThan(uvec4(abs(ivec4(colour) - ivec4(refcolour))).rgb, uvec3(2u)))";
			break;
		}
		break;
	default:
		const tcu::StringTemplate fs_condition ("all(lessThan((abs(colour - refcolour)).rgb, vec3(${EPS})))");
		std::map<std::string, std::string> fs_condition_params;
		fs_condition_params["EPS"] = std::to_string(getEps(m_internalFormat));
		params["SAMPLER_TYPE"] = "sampler2DArray";
		params["COL_TYPE"]	  = "vec4";
		params["CONDITION"]	  = fs_condition.specialize(fs_condition_params);
		break;
	}

	prog = new glu::ShaderProgram(m_context.getRenderContext(),
								  glu::ProgramSources() <<
								  glu::VertexSource(vs_src.specialize(params)) <<
								  glu::FragmentSource(fs_src.specialize(params)));

	if (!prog->isOk())
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << ""
			<< tcu::TestLog::EndMessage
			<< tcu::TestLog::ShaderProgram(false, "")
			<< tcu::TestLog::Shader(QP_SHADER_TYPE_VERTEX,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).infoLog)

			<< tcu::TestLog::Shader(QP_SHADER_TYPE_FRAGMENT,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).infoLog)
			<< tcu::TestLog::EndShaderProgram;
		TCU_FAIL("Shader creation failed");
	}

	gl.glUseProgram(prog->getProgram());
	gl.glUniform1i(gl.glGetUniformLocation(prog->getProgram(), "sampler"), 0);
}

tcu::Surface TexImage3DCase::renderToSurf (int layer)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());
	gl.glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	gl.glClear(GL_COLOR_BUFFER_BIT);

	static const float vertexPositions[4*3] =
	{
		-1.0, -1.0, -1.0f,
		 1.0, -1.0,	 0.0f,
		-1.0,  1.0,	 0.0f,
		1.0,   1.0,	 1.0f,
	};

	static const deUint16 indices[6] = { 0, 1, 2, 2, 1, 3 };

	const glu::VertexArrayBinding attrBindings[] =
	{
		glu::va::Float("pos", 3, 4, 0, &vertexPositions[0])
	};

	gl.glViewport(0, 0, m_subcuboid_w, m_subcuboid_h);

	gl.glUniform1i(gl.glGetUniformLocation(prog->getProgram(), "layer"), layer);
	glw::GLfloat refcol = 0.125 + layer * 0.125;
	setRefcolour(gl, refcol);
	glu::draw(m_context.getRenderContext(),
			  prog->getProgram(),
			  DE_LENGTH_OF_ARRAY(attrBindings),
			  &attrBindings[0],
			  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));

	tcu::Surface dst;
	dst.setSize(m_subcuboid_w, m_subcuboid_h);

	glu::readPixels(m_context.getRenderContext(), 0, 0, dst.getAccess());

	return dst;
}

void TexImage3DCase::cleanup (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glDeleteTextures(1, &tex);
	delete[] m_src_data;
	delete prog;
}

class CompressedTexImageUtils
{
public:
	CompressedTexImageUtils (deUint32 internalFormat,
							 int cuboid_w, int cuboid_h, int cuboid_d,
							 int subcuboid_x0, int subcuboid_y0, int subcuboid_z0,
							 int subcuboid_w, int subcuboid_h, int subcuboid_d,
							 glu::GLSLVersion glsl_version);
	~CompressedTexImageUtils (void);
protected:
	int getImageSize (int width, int height, int depth);
	bool verify(tcu::Surface dst, tcu::Surface *errMask);

	glw::GLubyte *m_src_data;
	deUint32 tex;
	glu::ShaderProgram *prog;

	int m_bw;					/* block width */
	int m_bh;					/* block height */
	int m_bd;					/* block depth */
	int m_bs;					/* block size */

	deUint32 m_internalFormat;
	int m_cuboid_w;
	int m_cuboid_h;
	int m_cuboid_d;
	int m_subcuboid_x0;
	int m_subcuboid_y0;
	int m_subcuboid_z0;
	int m_subcuboid_w;
	int m_subcuboid_h;
	int m_subcuboid_d;

	glu::GLSLVersion m_glsl_version;
};

CompressedTexImageUtils::CompressedTexImageUtils (deUint32 internalFormat,
												  int cuboid_w,
												  int cuboid_h,
												  int cuboid_d,
												  int subcuboid_x0,
												  int subcuboid_y0,
												  int subcuboid_z0,
												  int subcuboid_w,
												  int subcuboid_h,
												  int subcuboid_d,
												  glu::GLSLVersion glsl_version)
	: m_internalFormat(internalFormat)
	, m_cuboid_w(cuboid_w)
	, m_cuboid_h(cuboid_h)
	, m_cuboid_d(cuboid_d)
	, m_subcuboid_x0(subcuboid_x0)
	, m_subcuboid_y0(subcuboid_y0)
	, m_subcuboid_z0(subcuboid_z0)
	, m_subcuboid_w(subcuboid_w)
	, m_subcuboid_h(subcuboid_h)
	, m_subcuboid_d(subcuboid_d)
	, m_glsl_version(glsl_version)
{
}

CompressedTexImageUtils::~CompressedTexImageUtils(void)
{
}

int CompressedTexImageUtils::getImageSize (int width, int height, int depth)
{
	return (width / m_bw + (width % m_bw > 0)) *
		   (height / m_bh + (height % m_bh > 0)) *
		   (depth / m_bd + (depth % m_bd > 0)) *
		   m_bs;
}

bool CompressedTexImageUtils::verify(tcu::Surface dst, tcu::Surface *errMask)
{
	*errMask = tcu::Surface (dst.getWidth(), dst.getHeight());
	tcu::clear(errMask->getAccess(), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
	bool pass = true;

	for (int y = 0; y < dst.getHeight(); y++)
	{
		for (int x = 0; x < dst.getWidth(); x++)
		{
			if (dst.getPixel(x, y) != tcu::RGBA::green())
			{
				pass = false;
				errMask->setPixel(x, y, tcu::RGBA::red());
			}
		}
	}

	return pass;
}

class CompressedTexImage2DCase : public deqp::TestCase
							   , public sglr::ContextWrapper
							   , public CompressedTexImageUtils
{
public:
	CompressedTexImage2DCase (deqp::Context& context, const char *name, const char *desc,
							  deUint32 internalFormat,
							  int cuboid_w, int cuboid_h,
							  int subcuboid_x0, int subcuboid_y0,
							  int subcuboid_w, int subcuboid_h,
							  glu::GLSLVersion glsl_version);
	~CompressedTexImage2DCase (void);
	IterateResult iterate (void);
protected:
	void generateSrcData_s3tc (void);
	void generateSrcData_astc (void);
	void createTexture (void);
	void createShader (void);
	tcu::Surface renderToSurf (void);
	void cleanup (void);
};

CompressedTexImage2DCase::CompressedTexImage2DCase (deqp::Context& context,
													const char *name,
													const char *desc,
													deUint32 internalFormat,
													int cuboid_w,
													int cuboid_h,
													int subcuboid_x0,
													int subcuboid_y0,
													int subcuboid_w,
													int subcuboid_h,
													glu::GLSLVersion glsl_version)
	: TestCase(context, name, desc)
	, CompressedTexImageUtils(internalFormat,
							  cuboid_w,
							  cuboid_h,
							  1,
							  subcuboid_x0,
							  subcuboid_y0,
							  0,
							  subcuboid_w,
							  subcuboid_h,
							  1,
							  glsl_version)
{
}

CompressedTexImage2DCase::~CompressedTexImage2DCase (void)
{
}

CompressedTexImage2DCase::IterateResult CompressedTexImage2DCase::iterate (void)
{
	glu::RenderContext& renderCtx = TestCase::m_context.getRenderContext();
	const glu::ContextInfo& ctxInfo = m_context.getContextInfo();
	tcu::TestLog& log = m_testCtx.getLog();
	tcu::Surface dst, errMask;

	bool pass = true;

	sglr::GLContext gl_ctx (renderCtx,
							log,
							sglr::GLCONTEXT_LOG_CALLS,
							tcu::IVec4(0, 0, m_subcuboid_w, m_subcuboid_h));

	setContext((sglr::Context*)&gl_ctx);

	switch (m_internalFormat)
	{
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		if (!ctxInfo.isExtensionSupported("GL_EXT_texture_compression_s3tc"))
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
									"GL_EXT_texture_compression_s3tc extension is not supported");
			return STOP;
		}

		m_bw = 4;
		m_bh = 4;
		m_bd = 1;
		m_bs = 8;

		generateSrcData_s3tc();
		break;
	case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
		if (!ctxInfo.isExtensionSupported("GL_KHR_texture_compression_astc_ldr"))
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
									"GL_KHR_texture_compression_astc_ldr extension is not supported");
			return STOP;
		}
		m_bw = 8;
		m_bh = 5;
		m_bd = 1;
		m_bs = 16;

		generateSrcData_astc();
		break;
	default:
		TCU_FAIL("Invalid internal format");
		break;
	}

	createTexture();
	createShader();

	dst = renderToSurf();
	pass = verify(dst, &errMask);

	cleanup();

	if (pass)
	{
		m_testCtx.getLog()
		<< tcu::TestLog::Message << "Image is valid" << tcu::TestLog::EndMessage
		<< tcu::TestLog::ImageSet("ImageVerification", "Image verification")
		<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
		<< tcu::TestLog::EndImageSet;
	}
	else
	{
		m_testCtx.getLog()
		<< tcu::TestLog::Message << "Image is invalid" << tcu::TestLog::EndMessage
		<< tcu::TestLog::ImageSet("ErrorVerification", "Image verification")
		<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
		<< tcu::TestLog::Image("ErrorMask", "Error mask", errMask.getAccess())
		<< tcu::TestLog::EndImageSet;
	}

	m_testCtx.setTestResult(pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
							pass ? "Pass" : "Fail");

	return STOP;
}

void CompressedTexImage2DCase::generateSrcData_s3tc (void)
{
	deUint64 *src = new deUint64[m_cuboid_w / m_bw * m_cuboid_h / m_bh];

	deUint64 col = 0x0;

	for (int y = 0; y < m_cuboid_h; y += m_bh)
	{
		for (int x = 0; x < m_cuboid_w; x += m_bw)
		{
			if (inrange(x, m_subcuboid_x0, m_subcuboid_x0 + m_subcuboid_w) &&
				inrange(y, m_subcuboid_y0, m_subcuboid_y0 + m_subcuboid_h))
			{
				col = 0xffff;
			}
			else
			{
				col = 0x0;
			}
			int index = (y / m_bh) * (m_cuboid_w / m_bw) +
							(x / m_bw);
			src[index] = col;
		}
	}

	m_src_data = (glw::GLubyte*)src;
}

void CompressedTexImage2DCase::generateSrcData_astc (void)
{
	deUint64 col = 0x0;
	deUint64 mask = 0xfffffffffffffdfc;

	int img_size = 2 * (m_cuboid_w / m_bw + (m_cuboid_w % m_bw > 0)) *
				   (m_cuboid_h / m_bh + (m_cuboid_h % m_bh > 0));

	deUint64 *src = new deUint64[img_size];

	for (int y = 0; y < m_cuboid_h; y += m_bh)
	{
		for (int x = 0; x < m_cuboid_w; x += m_bw)
		{
			if (inrange(x, m_subcuboid_x0, m_subcuboid_x0 + m_subcuboid_w) &&
				inrange(y, m_subcuboid_y0, m_subcuboid_y0 + m_subcuboid_h))
			{
				col = 0xffffffffffffffff; /* (1.0, 1.0, 1.0) */
			}
			else
			{
				col = 0x0; /* (0.0, 0.0, 0.0) */
			}
			int index = (y / m_bh) * (m_cuboid_w / m_bw + (m_cuboid_w % m_bw > 0)) +
						(x / m_bw);
			src[2 * index] = mask;
			src[2 * index + 1] = col;
		}
	}

	m_src_data = (glw::GLubyte*)src;
}

void CompressedTexImage2DCase::createTexture (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glGenTextures(1, &tex);
	gl.glBindTexture(GL_TEXTURE_2D, tex);
	gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE,	 m_bs);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH,	 m_bw);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, m_bh);

	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 m_cuboid_w);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 m_subcuboid_y0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS, m_subcuboid_x0);

	gl.glCompressedTexImage2D(GL_TEXTURE_2D,
							  0,
							  m_internalFormat,
							  m_subcuboid_w,
							  m_subcuboid_h,
							  0,
							  getImageSize(m_subcuboid_w,
										   m_subcuboid_h,
										   m_subcuboid_d),
							  m_src_data);

	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE,	 0);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH,	 0);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT,  0);

	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS,  0);
}

void CompressedTexImage2DCase::createShader (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	const tcu::StringTemplate vs_src (vs_template_src);
	const tcu::StringTemplate fs_src (fs_template_src);

	std::map<std::string,std::string> params;
	params["GLSL_VERSION"]	 = getGLSLVersionDeclaration(m_glsl_version);
	params["TEXCOORDS_TYPE"] = "vec2";
	params["LAYER"]			 = "";
	params["TEXCOORDS"]		 = "pos.xy";
	params["SAMPLER_TYPE"]	 = "sampler2D";
	params["COL_TYPE"]		 = "vec4";
	params["CONDITION"]		 = "colour.rgb == refcolour.rgb";

	prog = new glu::ShaderProgram(m_context.getRenderContext(),
								  glu::ProgramSources() <<
								  glu::VertexSource(vs_src.specialize(params)) <<
								  glu::FragmentSource(fs_src.specialize(params)));

	if (!prog->isOk())
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << ""
			<< tcu::TestLog::EndMessage
			<< tcu::TestLog::ShaderProgram(false, "")
			<< tcu::TestLog::Shader(QP_SHADER_TYPE_VERTEX,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).infoLog)

			<< tcu::TestLog::Shader(QP_SHADER_TYPE_FRAGMENT,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).infoLog)
			<< tcu::TestLog::EndShaderProgram;
		TCU_FAIL("Shader creation failed");
	}

	gl.glUseProgram(prog->getProgram());
	gl.glUniform1i(gl.glGetUniformLocation(prog->getProgram(), "sampler"), 0);
}

tcu::Surface CompressedTexImage2DCase::renderToSurf (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	gl.glClear(GL_COLOR_BUFFER_BIT);

	static const float vertexPositions[4*3] =
	{
		-1.0, -1.0, -1.0f,
		 1.0, -1.0,	 0.0f,
		-1.0,  1.0,	 0.0f,
		 1.0,  1.0,	 1.0f,
	};

	static const deUint16 indices[6] = { 0, 1, 2, 2, 1, 3 };

	const glu::VertexArrayBinding attrBindings[] =
	{
		glu::va::Float("pos", 3, 4, 0, &vertexPositions[0])
	};

	gl.glViewport(0, 0, m_subcuboid_w, m_subcuboid_h);

	glw::GLfloat refcol = 1.0f;

	gl.glUniform4f(gl.glGetUniformLocation(prog->getProgram(), "refcolour"),
				   refcol, refcol, refcol, 1.0f);

	glu::draw(m_context.getRenderContext(),
			  prog->getProgram(),
			  DE_LENGTH_OF_ARRAY(attrBindings),
			  &attrBindings[0],
			  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));

	tcu::Surface dst;
	dst.setSize(m_subcuboid_w, m_subcuboid_h);

	glu::readPixels(m_context.getRenderContext(), 0, 0, dst.getAccess());

	return dst;
}

void CompressedTexImage2DCase::cleanup (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glDeleteTextures(1, &tex);
	delete[] m_src_data;
	delete prog;
}

class CompressedTexImage3DCase : public deqp::TestCase
							   , public sglr::ContextWrapper
							   , public CompressedTexImageUtils
{
public:
	CompressedTexImage3DCase (deqp::Context& context, const char *name, const char *desc,
							  deUint32 internalFormat,
							  int cuboid_w, int cuboid_h, int cuboid_d,
							  int subcuboid_x0, int subcuboid_y0, int subcuboid_z0,
							  int subcuboid_w, int subcuboid_h, int subcuboid_d,
							  glu::GLSLVersion glsl_version);
	~CompressedTexImage3DCase (void);
	IterateResult iterate (void);
protected:
	void generateSrcData_s3tc (void);
	void generateSrcData_astc (void);
	void createTexture (void);
	void createShader (void);
	tcu::Surface renderToSurf (int layer);
	void cleanup (void);
};

CompressedTexImage3DCase::CompressedTexImage3DCase (deqp::Context& context,
													const char *name,
													const char *desc,
													deUint32 internalFormat,
													int cuboid_w,
													int cuboid_h,
													int cuboid_d,
													int subcuboid_x0,
													int subcuboid_y0,
													int subcuboid_z0,
													int subcuboid_w,
													int subcuboid_h,
													int subcuboid_d,
													glu::GLSLVersion glsl_version)
	: TestCase(context, name, desc)
	, CompressedTexImageUtils(internalFormat,
							  cuboid_w,
							  cuboid_h,
							  cuboid_d,
							  subcuboid_x0,
							  subcuboid_y0,
							  subcuboid_z0,
							  subcuboid_w,
							  subcuboid_h,
							  subcuboid_d,
							  glsl_version)
{
}

CompressedTexImage3DCase::~CompressedTexImage3DCase (void)
{
}

CompressedTexImage3DCase::IterateResult CompressedTexImage3DCase::iterate (void)
{
	glu::RenderContext& renderCtx = TestCase::m_context.getRenderContext();
	const glu::ContextInfo& ctxInfo = m_context.getContextInfo();
	tcu::TestLog& log = m_testCtx.getLog();
	tcu::Surface dst, errMask;

	bool pass = true;

	sglr::GLContext gl_ctx (renderCtx,
							log,
							sglr::GLCONTEXT_LOG_CALLS,
							tcu::IVec4(0, 0, m_subcuboid_w, m_subcuboid_h));

	setContext((sglr::Context*)&gl_ctx);

	switch (m_internalFormat)
	{
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		if (!ctxInfo.isExtensionSupported("GL_EXT_texture_compression_s3tc"))
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
									"GL_EXT_texture_compression_s3tc extension is not supported");
			return STOP;
		}

		m_bw = 4;
		m_bh = 4;
		m_bd = 1;
		m_bs = 8;

		generateSrcData_s3tc();
		break;
	case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
		if (!ctxInfo.isExtensionSupported("GL_KHR_texture_compression_astc_ldr"))
		{
			m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED,
									"GL_KHR_texture_compression_astc_ldr extension is not supported");
			return STOP;
		}
		m_bw = 8;
		m_bh = 5;
		m_bd = 1;
		m_bs = 16;

		generateSrcData_astc();
		break;
	default:
		TCU_FAIL("Invalid internal format");
		break;
	}

	createTexture();
	createShader();

	for (int z = 0; z < m_subcuboid_d; z++)
	{
		dst = renderToSurf(z);

		bool layer_pass = verify(dst, &errMask);

		if (layer_pass)
		{
			m_testCtx.getLog()
				<< tcu::TestLog::Message << "Layer " << z	 << " is valid"
				<< tcu::TestLog::EndMessage
				<< tcu::TestLog::ImageSet("LayerVerification", "Layer verification")
				<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
				<< tcu::TestLog::EndImageSet;
		}
		else
		{
			m_testCtx.getLog()
				<< tcu::TestLog::Message << "Layer " << z << " is invalid"
				<< tcu::TestLog::EndMessage
				<< tcu::TestLog::ImageSet("ErrorVerification", "Layer verification")
				<< tcu::TestLog::Image("Result", "Rendered result", dst.getAccess())
				<< tcu::TestLog::Image("ErrorMask", "Error mask", errMask.getAccess())
				<< tcu::TestLog::EndImageSet;
		}

		pass &= layer_pass;
	}

	cleanup();

	if (pass)
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << "Image is valid" << tcu::TestLog::EndMessage;
	}
	else
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << "Image is invalid" << tcu::TestLog::EndMessage;
	}

	m_testCtx.setTestResult(pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
							pass ? "Pass" : "Fail");

	return STOP;
}

void CompressedTexImage3DCase::generateSrcData_s3tc()
{
	deUint64 *src = new deUint64[m_cuboid_w / m_bw *
								 m_cuboid_h / m_bh *
								 m_cuboid_d / m_bd];

	deUint64 col_list[] =
	{
		0x18E3,					/* (0.125, 0.125, 0.125) */
		0x39E7,					/* (0.250, 0.250, 0.250) */
		0x5AEB,					/* (0.375, 0.375, 0.375) */
		0x7BEF,					/* (0.500, 0.500, 0.500) */
		0x9CF3,					/* (0.625, 0.625, 0.625) */
		0xBDF7,					/* (0.750, 0.750, 0.750) */
		0xDEFB,					/* (0.875, 0.875, 0.875) */
		0xffff,					/* (1.000, 1.000, 1.000) */
	};

	deUint64 col = 0x0;

	for (int z = 0; z < m_cuboid_d; z += m_bd)
	{
		for (int y = 0; y < m_cuboid_h; y += m_bh)
		{
			for (int x = 0; x < m_cuboid_w; x += m_bw)
			{
				if (inrange(x, m_subcuboid_x0, m_subcuboid_x0 + m_subcuboid_w) &&
					inrange(y, m_subcuboid_y0, m_subcuboid_y0 + m_subcuboid_h) &&
					inrange(z, m_subcuboid_z0, m_subcuboid_z0 + m_subcuboid_d))
					col = col_list[z % 8];
				else
					col = 0x0;

				int index = (z / m_bd) * (m_cuboid_h / m_bh) * (m_cuboid_w / m_bw) +
							(y / m_bh) * (m_cuboid_w / m_bw) +
							(x / m_bw);
				src[index] = col;
			}
		}
	}

	m_src_data = (glw::GLubyte*)src;
}

void CompressedTexImage3DCase::generateSrcData_astc (void)
{
	deUint64 col_list[] =
	{
		0xffff1fff1fff1fff,		/* (0.125, 0.125, 0.125) */
		0xffff3fff3fff3fff,		/* (0.250, 0.250, 0.250) */
		0xffff5fff5fff5fff,		/* (0.375, 0.375, 0.375) */
		0xffff7fff7fff7fff,		/* (0.500, 0.500, 0.500) */
		0xffff9fff9fff9fff,		/* (0.625, 0.625, 0.625) */
		0xffffbfffbfffbfff,		/* (0.750, 0.750, 0.750) */
		0xffffdfffdfffdfff,		/* (0.875, 0.875, 0.875) */
		0xffffffffffffffff,		/* (1.000, 1.000, 1.000) */
	};
	deUint64 col = 0x0;
	deUint64 mask = 0xFFFFFFFFFFFFFDFC;

	int img_size = 2 * (m_cuboid_w / m_bw + (m_cuboid_w % m_bw > 0)) *
					   (m_cuboid_h / m_bh + (m_cuboid_h % m_bh > 0)) *
					   (m_cuboid_d / m_bd + (m_cuboid_d % m_bd > 0));

	deUint64 *src = new deUint64[img_size];

	for (int z = 0; z < m_cuboid_d; z += m_bd)
	{
		for (int y = 0; y < m_cuboid_h; y += m_bh)
		{
			for (int x = 0; x < m_cuboid_w; x += m_bw)
			{
				if (inrange(x, m_subcuboid_x0, m_subcuboid_x0 + m_subcuboid_w) &&
					inrange(y, m_subcuboid_y0, m_subcuboid_y0 + m_subcuboid_h) &&
					inrange(z, m_subcuboid_z0, m_subcuboid_z0 + m_subcuboid_d))
					col = col_list[z % 8];
				else
					col = 0x0;

				int index = (z / m_bd) * (m_cuboid_h / m_bh + (m_cuboid_h % m_bh > 0)) *
										 (m_cuboid_w / m_bw + (m_cuboid_w % m_bw > 0)) +
							(y / m_bh) * (m_cuboid_w / m_bw + (m_cuboid_w % m_bw > 0)) +
							(x / m_bw);
				src[2 * index] = mask;
				src[2 * index + 1] = col;
			}
		}
	}

	m_src_data = (glw::GLubyte*)src;
}

void CompressedTexImage3DCase::createTexture (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glGenTextures(1, &tex);
	gl.glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE,	 m_bs);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH,	 m_bw);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT,  m_bh);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH,	 m_bd);

	gl.glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, m_cuboid_h);
	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 m_cuboid_w);
	gl.glPixelStorei(GL_UNPACK_SKIP_IMAGES,  m_subcuboid_z0);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 m_subcuboid_y0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS,  m_subcuboid_x0);

	gl.glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY,
							  0,
							  m_internalFormat,
							  m_subcuboid_w,
							  m_subcuboid_h,
							  m_subcuboid_d,
							  0,
							  getImageSize(m_subcuboid_w,
										   m_subcuboid_h,
										   m_subcuboid_d),
							  m_src_data);

	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE,	 0);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH,	 0);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT,  0);
	gl.glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH,	 0);

	gl.glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
	gl.glPixelStorei(GL_UNPACK_ROW_LENGTH,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_IMAGES,  0);
	gl.glPixelStorei(GL_UNPACK_SKIP_ROWS,	 0);
	gl.glPixelStorei(GL_UNPACK_SKIP_PIXELS,  0);
}

void CompressedTexImage3DCase::createShader (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	const tcu::StringTemplate vs_src (vs_template_src);
	const tcu::StringTemplate fs_src (fs_template_src);

	std::map<std::string,std::string> params;
	params["GLSL_VERSION"]	 = getGLSLVersionDeclaration(m_glsl_version);
	params["TEXCOORDS_TYPE"] = "vec3";
	params["LAYER"]			 = "uniform int layer;";
	params["TEXCOORDS"]		 = "vec3(pos.xy, layer)";
	params["SAMPLER_TYPE"]	 = "sampler2DArray";
	params["COL_TYPE"]		 = "vec4";

	const tcu::StringTemplate fs_condition ("all(lessThan((abs(colour - refcolour)).rgb, vec3(${EPS})))");
	std::map<std::string,std::string> fs_condition_params;
	fs_condition_params["EPS"] = std::to_string(getEps(m_internalFormat));
	params["CONDITION"] = fs_condition.specialize(fs_condition_params);

	prog = new glu::ShaderProgram(m_context.getRenderContext(),
								  glu::ProgramSources() <<
								  glu::VertexSource(vs_src.specialize(params)) <<
								  glu::FragmentSource(fs_src.specialize(params)));

	if (!prog->isOk())
	{
		m_testCtx.getLog()
			<< tcu::TestLog::Message << ""
			<< tcu::TestLog::EndMessage
			<< tcu::TestLog::ShaderProgram(false, "")
			<< tcu::TestLog::Shader(QP_SHADER_TYPE_VERTEX,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_VERTEX,
														0).infoLog)

			<< tcu::TestLog::Shader(QP_SHADER_TYPE_FRAGMENT,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).source,
									false,
									prog->getShaderInfo(glu::SHADERTYPE_FRAGMENT,
														0).infoLog)
			<< tcu::TestLog::EndShaderProgram;
		TCU_FAIL("Shader creation failed");
	}

	gl.glUseProgram(prog->getProgram());
	gl.glUniform1i(gl.glGetUniformLocation(prog->getProgram(), "sampler"), 0);
}

tcu::Surface CompressedTexImage3DCase::renderToSurf (int layer)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	gl.glClear(GL_COLOR_BUFFER_BIT);

	static const float vertexPositions[4*3] =
	{
		-1.0, -1.0, -1.0f,
		 1.0, -1.0,	 0.0f,
		-1.0,  1.0,	 0.0f,
		 1.0,  1.0,	 1.0f,
	};

	static const deUint16 indices[6] = { 0, 1, 2, 2, 1, 3 };

	const glu::VertexArrayBinding attrBindings[] =
	{
		glu::va::Float("pos", 3, 4, 0, &vertexPositions[0])
	};

	gl.glViewport(0, 0, m_subcuboid_w, m_subcuboid_h);

	gl.glUniform1i(gl.glGetUniformLocation(prog->getProgram(), "layer"), layer);

	glw::GLfloat refcols[8] = { 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0 };
	glw::GLfloat refcol = refcols[(layer + m_subcuboid_z0 % 8) % 8];

	gl.glUniform4f(gl.glGetUniformLocation(prog->getProgram(), "refcolour"),
				   refcol, refcol, refcol, 1.0f);

	glu::draw(m_context.getRenderContext(),
			  prog->getProgram(),
			  DE_LENGTH_OF_ARRAY(attrBindings),
			  &attrBindings[0],
			  glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));

	tcu::Surface dst;
	dst.setSize(m_subcuboid_w, m_subcuboid_h);

	glu::readPixels(m_context.getRenderContext(), 0, 0, dst.getAccess());

	return dst;
}

void CompressedTexImage3DCase::cleanup (void)
{
	glu::CallLogWrapper gl (m_context.getRenderContext().getFunctions(),
							m_testCtx.getLog());

	gl.glDeleteTextures(1, &tex);
	delete[] m_src_data;
	delete prog;
}

PixelStorageModesTests::PixelStorageModesTests (deqp::Context& context,
												glu::GLSLVersion glsl_version)
	: TestCaseGroup(context, "pixelstoragemodes", "Pixel Storage Modes Tests")
	, m_glsl_version(glsl_version)
{
}

PixelStorageModesTests::~PixelStorageModesTests (void)
{
}

void PixelStorageModesTests::init(void)
{
	const int cuboid_w = 64;
	const int cuboid_h = 64;
	const int cuboid_d = 64;
	const int subcuboid_w = 32;
	const int subcuboid_h = 32;
	const int subcuboid_d = 8;

	struct
	{
		const char *name;
		deUint32 internalFmt;
	} internalFmts[] =
	{
		{ "r8",			 GL_R8,				  },
		{ "r8snorm",	 GL_R8_SNORM,		  },
		{ "r16f",		 GL_R16F,			  },
		{ "r32f",		 GL_R32F,			  },
		{ "r8ui",		 GL_R8UI,			  },
		{ "r8i",		 GL_R8I,			  },
		{ "r16ui",		 GL_R16UI,			  },
		{ "r16i",		 GL_R16I,			  },
		{ "r32ui",		 GL_R32UI,			  },
		{ "r32i",		 GL_R32I,			  },
		{ "rg8",		 GL_RG8,			  },
		{ "rg8snorm",	 GL_RG8_SNORM,		  },
		{ "rg16f",		 GL_RG16F,			  },
		{ "rg32f",		 GL_RG32F,			  },
		{ "rg8ui",		 GL_RG8UI,			  },
		{ "rg8i",		 GL_RG8I,			  },
		{ "rg16ui",		 GL_RG16UI,			  },
		{ "rg16i",		 GL_RG16I,			  },
		{ "rg32ui",		 GL_RG32UI,			  },
		{ "rg32i",		 GL_RG32I,			  },
		{ "rgb8",		 GL_RGB8,			  },
		{ "rgb565",		 GL_RGB565,			  },
		{ "rgb8snorm",	 GL_RGB8_SNORM,		  },
		{ "r11g11b10f",	 GL_R11F_G11F_B10F,	  },
		{ "rgb16f",		 GL_RGB16F,			  },
		{ "rgb32f",		 GL_RGB32F,			  },
		{ "rgb8ui",		 GL_RGB8UI,			  },
		{ "rgb8i",		 GL_RGB8I,			  },
		{ "rgb16ui",	 GL_RGB16UI,		  },
		{ "rgb16i",		 GL_RGB16I,			  },
		{ "rgb32ui",	 GL_RGB32UI,		  },
		{ "rgb32i",		 GL_RGB32I,			  },
		{ "rgba8",		 GL_RGBA8,			  },
		{ "rgba8snorm",	 GL_RGBA8_SNORM,	  },
		{ "rgb5a1",		 GL_RGB5_A1,		  },
		{ "rgba4",		 GL_RGBA4,			  },
		{ "rgb10a2",	 GL_RGB10_A2,		  },
		{ "rgba16f",	 GL_RGBA16F,		  },
		{ "rgba32f",	 GL_RGBA32F,		  },
		{ "rgba8ui",	 GL_RGBA8UI,		  },
		{ "rgba8i",		 GL_RGBA8I,			  },
		{ "rgb10a2ui",	 GL_RGB10_A2UI,		  },
		{ "rgba16ui",	 GL_RGBA16UI,		  },
		{ "rgba16i",	 GL_RGBA16I,		  },
		{ "rgba32i",	 GL_RGBA32I,		  },
		{ "rgba32ui",	 GL_RGBA32UI,		  },
	};

	struct
	{
		const char *name;
		deUint32 internalFmt;
		int bw;
		int bh;
		int bd;
	} internalFmts_compressed[] =
	{
		{ "rgb_s3tc_dxt1", GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 1 },
		{ "rgba_astc_8x5", GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 8, 5, 1 },
	};

	tcu::TestCaseGroup* texImage2DGroup = new tcu::TestCaseGroup(m_testCtx,
																 "teximage2d",
																 "glTexImage2D cases");
	addChild(texImage2DGroup);

	for (int fmts = 0; fmts < DE_LENGTH_OF_ARRAY(internalFmts); fmts++)
	{
		tcu::TestCaseGroup* formatsGroup = new tcu::TestCaseGroup(m_testCtx,
																  internalFmts[fmts].name,
																  "");
		texImage2DGroup->addChild(formatsGroup);
		int bw = 1;
		int bh = 1;
		int skip_pixels[3] =
			{ 0, bw, bw * (subcuboid_w / (2 * bw)) };
		int skip_rows[3] =
			{ 0, bh, bh * (subcuboid_h / (2 * bh)) };

		for (int r = 0; r < 3; r++)
		{
			for (int p = r; p < 3; p++)
			{
				std::string skip_name =
					std::to_string(skip_pixels[p]) +
					"_" +
					std::to_string(skip_rows[r]);
				std::string skip_desc =
					"Skip " +
					std::to_string(skip_pixels[p]) +
					" pixels and " +
					std::to_string(skip_rows[r]) +
					" rows";
				formatsGroup->addChild(new TexImage2DCase(m_context,
														  skip_name.c_str(),
														  skip_desc.c_str(),
														  internalFmts[fmts].internalFmt,
														  cuboid_w,
														  cuboid_h,
														  skip_pixels[p],
														  skip_rows[r],
														  subcuboid_w,
														  subcuboid_h,
														  m_glsl_version));
			}
		}
	}

	tcu::TestCaseGroup* texImage3DGroup = new tcu::TestCaseGroup(m_testCtx,
																 "teximage3d",
																 "glTexImage3D cases");
	addChild(texImage3DGroup);

	for (int fmts = 0; fmts < DE_LENGTH_OF_ARRAY(internalFmts); fmts++)
	{
		tcu::TestCaseGroup* formatsGroup = new tcu::TestCaseGroup(m_testCtx,
																  internalFmts[fmts].name,
																  "");
		texImage3DGroup->addChild(formatsGroup);
		int bw = 1;
		int bh = 1;
		int bd = 1;
		int skip_pixels[3] =
			{ 0, bw, bw * (subcuboid_w / (2 * bw)) };
		int skip_rows[3] =
			{ 0, bh, bh * (subcuboid_h / (2 * bh)) };
		int skip_images[3] =
			{ 0, bd, bd * (subcuboid_d / (2 * bd)) };

		for (int i = 0; i < 3; i++)
		{
			for (int r = i; r < 3; r++)
			{
				for (int p = r; p < 3; p++)
				{
					std::string skip_name =
						std::to_string(skip_pixels[p]) +
						"_" +
						std::to_string(skip_rows[r]) +
						"_" +
						std::to_string(skip_images[i]);
					std::string skip_desc =
						"Skip " +
						std::to_string(skip_pixels[p]) +
						" pixels, " +
						std::to_string(skip_rows[r]) +
						" rows, and " +
						std::to_string(skip_images[i]) +
						" images";
					formatsGroup->addChild(new TexImage3DCase(m_context,
															  skip_name.c_str(),
															  skip_desc.c_str(),
															  internalFmts[fmts].internalFmt,
															  cuboid_w,
															  cuboid_h,
															  cuboid_d,
															  skip_pixels[p],
															  skip_rows[r],
															  skip_images[i],
															  subcuboid_w,
															  subcuboid_h,
															  subcuboid_d,
															  m_glsl_version));
				}
			}
		}
	}

	if (!glu::isContextTypeES(m_context.getRenderContext().getType()))
	{
		tcu::TestCaseGroup* compressedTexImage2DGroup =
			new tcu::TestCaseGroup(m_testCtx,
								   "compressedteximage2d",
								   "glCompressedTexImage2D cases");
		addChild(compressedTexImage2DGroup);

		for (int fmts = 0; fmts < DE_LENGTH_OF_ARRAY(internalFmts_compressed); fmts++)
		{
			tcu::TestCaseGroup* formatsGroup
				= new tcu::TestCaseGroup(m_testCtx,
										 internalFmts_compressed[fmts].name,
										 "");
			compressedTexImage2DGroup->addChild(formatsGroup);
			int bw = internalFmts_compressed[fmts].bw;
			int bh = internalFmts_compressed[fmts].bh;
			int skip_pixels[4] =
				{ 0, bw, bw * (subcuboid_w / (2 * bw)), bw * (subcuboid_w / bw) };
			int skip_rows[4] =
				{ 0, bh, bh * (subcuboid_h / (2 * bh)), bh * (subcuboid_h / bh) };
			for (int r = 0; r < 4; r++)
			{
				for (int p = 0; p < 4; p++)
				{
					std::string skip_name =
						std::to_string(skip_pixels[p]) +
						"_" +
						std::to_string(skip_rows[r]);
					std::string skip_desc =
						"Skip " +
						std::to_string(skip_pixels[p]) +
						" pixels and " +
						std::to_string(skip_rows[r]) +
						" rows";
					formatsGroup->addChild(new CompressedTexImage2DCase(
											  m_context,
											  skip_name.c_str(),
											  skip_desc.c_str(),
											  internalFmts_compressed[fmts].internalFmt,
											  cuboid_w,
											  cuboid_h,
											  skip_pixels[p],
											  skip_rows[r],
											  subcuboid_w,
											  subcuboid_h,
											  m_glsl_version));
				}
			}
		}

		tcu::TestCaseGroup* compressedTexImage3DGroup =
			new tcu::TestCaseGroup(m_testCtx,
								   "compressedteximage3d",
								   "glCompressedTexImage3D cases");
		addChild(compressedTexImage3DGroup);

		for (int fmts = 0; fmts < DE_LENGTH_OF_ARRAY(internalFmts_compressed); fmts++)
		{
			tcu::TestCaseGroup* formatsGroup
				= new tcu::TestCaseGroup(m_testCtx,
										 internalFmts_compressed[fmts].name,
										 "");
			compressedTexImage3DGroup->addChild(formatsGroup);
			int bw = internalFmts_compressed[fmts].bw;
			int bh = internalFmts_compressed[fmts].bh;
			int bd = internalFmts_compressed[fmts].bd;
			int skip_pixels[4] =
				{ 0, bw, bw * (subcuboid_w / (2 * bw)), bw * (subcuboid_w / bw) };
			int skip_rows[4] =
				{ 0, bh, bh * (subcuboid_h / (2 * bh)), bh * (subcuboid_h / bh) };
			int skip_images[4] =
				{ 0, bd, bd * (subcuboid_d / (2 * bd)), bd * (subcuboid_d / bd) };
			for (int i = 0; i < 4; i++)
			{
				for (int r = 0; r < 4; r++)
				{
					for (int p = 0; p < 4; p++)
					{
						std::string skip_name =
							std::to_string(skip_pixels[p]) +
							"_" +
							std::to_string(skip_rows[r]) +
							"_" +
							std::to_string(skip_images[i]);
						std::string skip_desc =
							"Skip " +
							std::to_string(skip_pixels[p]) +
							" pixels, " +
							std::to_string(skip_rows[r]) +
							" rows, and " +
							std::to_string(skip_images[i]) +
							" images";
						formatsGroup->addChild(new CompressedTexImage3DCase(
												  m_context,
												  skip_name.c_str(),
												  skip_desc.c_str(),
												  internalFmts_compressed[fmts].internalFmt,
												  cuboid_w,
												  cuboid_h,
												  cuboid_d,
												  skip_pixels[p],
												  skip_rows[r],
												  skip_images[i],
												  subcuboid_w,
												  subcuboid_h,
												  subcuboid_d,
												  m_glsl_version));
					}
				}
			}
		}
	}
}

} /* namespace glcts */
