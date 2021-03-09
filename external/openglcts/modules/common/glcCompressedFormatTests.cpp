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
 * \file glcCompressedFormatTests.cpp
 * \brief Tests for OpenGL ES 3.1 and 3.2 compressed image formats
 */ /*-------------------------------------------------------------------*/

#include "glcCompressedFormatTests.hpp"

#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluStrUtil.hpp"

#include "tcuResource.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"

#include <algorithm>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <fstream>

namespace glcts
{
namespace
{
using namespace glw;
using namespace glu;
using namespace tcu;
using namespace std;

struct FormatInfo
{
	ApiType		minApi;
	const char*	name;
	GLenum		internalFormat;
	GLenum		format;
	GLenum		sizedFormat;
	bool		issRGB;
	IVec2		blockSize;
};

const ApiType		gles31							= ApiType::es(3, 1);
const ApiType		gles32							= ApiType::es(3, 2);

// List of compressed texture formats (table 8.17)
const FormatInfo		compressedFormats[]			=
{
//  ETC (table C.2)
//    minApi, name								, internalFormat								, format		, sizedFormat		, issRGB	, blockSize
	{ gles31, "r11_eac"							, GL_COMPRESSED_R11_EAC							, GL_RED		, GL_R8				, false		, { 4, 4 } },
	{ gles31, "signed_r11_eac"					, GL_COMPRESSED_SIGNED_R11_EAC					, GL_RED		, GL_R8				, false		, { 4, 4 } },
	{ gles31, "rg11_eac"						, GL_COMPRESSED_RG11_EAC						, GL_RG			, GL_RG8			, false		, { 4, 4 } },
	{ gles31, "signed_rg11_eac"					, GL_COMPRESSED_SIGNED_RG11_EAC					, GL_RG			, GL_RG8			, false		, { 4, 4 } },
	{ gles31, "rgb8_etc2"						, GL_COMPRESSED_RGB8_ETC2						, GL_RGB		, GL_RGB8			, false		, { 4, 4 } },
	{ gles31, "srgb8_etc2"						, GL_COMPRESSED_SRGB8_ETC2						, GL_RGB		, GL_SRGB8			, true		, { 4, 4 } },
	{ gles31, "rgb8_punchthrough_alpha1_etc2"	, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2	, GL_RGBA		, GL_RGBA8			, false		, { 4, 4 } },
	{ gles31, "srgb8_punchthrough_alpha1_etc2"	, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2	, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 4, 4 } },
	{ gles31, "rgba8_etc2_eac"					, GL_COMPRESSED_RGBA8_ETC2_EAC					, GL_RGBA		, GL_RGBA8			, false		, { 4, 4 } },
	{ gles31, "srgb8_alpha8_etc2_eac"			, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 4, 4 } },
//  ASTC (table C.1)
//    minApi, name								, internalFormat								, format		, sizedFormat		, issRGB	, blockSize
	{ gles32, "rgba_astc_4x4"					, GL_COMPRESSED_RGBA_ASTC_4x4					, GL_RGBA		, GL_RGBA8			, false		, { 4, 4 } },
	{ gles32, "rgba_astc_5x4"					, GL_COMPRESSED_RGBA_ASTC_5x4					, GL_RGBA		, GL_RGBA8			, false		, { 5, 4 } },
	{ gles32, "rgba_astc_5x5"					, GL_COMPRESSED_RGBA_ASTC_5x5					, GL_RGBA		, GL_RGBA8			, false		, { 5, 5 } },
	{ gles32, "rgba_astc_6x5"					, GL_COMPRESSED_RGBA_ASTC_6x5					, GL_RGBA		, GL_RGBA8			, false		, { 6, 5 } },
	{ gles32, "rgba_astc_6x6"					, GL_COMPRESSED_RGBA_ASTC_6x6					, GL_RGBA		, GL_RGBA8			, false		, { 6, 6 } },
	{ gles32, "rgba_astc_8x5"					, GL_COMPRESSED_RGBA_ASTC_8x5					, GL_RGBA		, GL_RGBA8			, false		, { 8, 5 } },
	{ gles32, "rgba_astc_8x6"					, GL_COMPRESSED_RGBA_ASTC_8x6					, GL_RGBA		, GL_RGBA8			, false		, { 8, 6 } },
	{ gles32, "rgba_astc_8x8"					, GL_COMPRESSED_RGBA_ASTC_8x8					, GL_RGBA		, GL_RGBA8			, false		, { 8, 8 } },
	{ gles32, "rgba_astc_10x5"					, GL_COMPRESSED_RGBA_ASTC_10x5					, GL_RGBA		, GL_RGBA8			, false		, { 10, 5 } },
	{ gles32, "rgba_astc_10x6"					, GL_COMPRESSED_RGBA_ASTC_10x6					, GL_RGBA		, GL_RGBA8			, false		, { 10, 6 } },
	{ gles32, "rgba_astc_10x8"					, GL_COMPRESSED_RGBA_ASTC_10x8					, GL_RGBA		, GL_RGBA8			, false		, { 10, 8 } },
	{ gles32, "rgba_astc_10x10"					, GL_COMPRESSED_RGBA_ASTC_10x10					, GL_RGBA		, GL_RGBA8			, false		, { 10, 10 } },
	{ gles32, "rgba_astc_12x10"					, GL_COMPRESSED_RGBA_ASTC_12x10					, GL_RGBA		, GL_RGBA8			, false		, { 12, 10 } },
	{ gles32, "rgba_astc_12x12"					, GL_COMPRESSED_RGBA_ASTC_12x12					, GL_RGBA		, GL_RGBA8			, false		, { 12, 12 } },
	{ gles32, "srgb8_alpha8_astc_4x4"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 4, 4 } },
	{ gles32, "srgb8_alpha8_astc_5x4"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 5, 4 } },
	{ gles32, "srgb8_alpha8_astc_5x5"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 5, 5 } },
	{ gles32, "srgb8_alpha8_astc_6x5"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 6, 5 } },
	{ gles32, "srgb8_alpha8_astc_6x6"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 6, 6 } },
	{ gles32, "srgb8_alpha8_astc_8x5"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 8, 5 } },
	{ gles32, "srgb8_alpha8_astc_8x6"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 8, 6 } },
	{ gles32, "srgb8_alpha8_astc_8x8"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 8, 8 } },
	{ gles32, "srgb8_alpha8_astc_10x5"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 10, 5 } },
	{ gles32, "srgb8_alpha8_astc_10x6"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 10, 6 } },
	{ gles32, "srgb8_alpha8_astc_10x8"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 10, 8 } },
	{ gles32, "srgb8_alpha8_astc_10x10"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 10, 10 } },
	{ gles32, "srgb8_alpha8_astc_12x10"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 12, 10 } },
	{ gles32, "srgb8_alpha8_astc_12x12"			, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12			, GL_RGBA		, GL_SRGB8_ALPHA8	, true		, { 12, 12 } },
};

struct UnsizedFormatInfo
{
	GLenum	format;
	GLenum	dataType;
};

const map<GLenum, UnsizedFormatInfo>
						unsizedFormats				=
{
	{ GL_RGBA32UI	, { GL_RGBA_INTEGER	, GL_UNSIGNED_INT	} },
	{ GL_RGBA32I	, { GL_RGBA_INTEGER	, GL_INT			} },
	{ GL_RGBA32F	, { GL_RGBA			, GL_FLOAT			} },
	{ GL_RGBA16F	, { GL_RGBA			, GL_FLOAT			} },
	{ GL_RG32F		, { GL_RG			, GL_FLOAT			} },
	{ GL_RGBA16UI	, { GL_RGBA_INTEGER	, GL_UNSIGNED_SHORT	} },
	{ GL_RG32UI		, { GL_RG_INTEGER	, GL_UNSIGNED_INT	} },
	{ GL_RGBA16I	, { GL_RGBA_INTEGER	, GL_SHORT			} },
	{ GL_RG32I		, { GL_RG_INTEGER	, GL_INT			} }
};

const vector<pair<vector<GLenum>, vector<GLenum>>>
						copyFormats					=
{
	// Table 16.3 - copy between compressed and uncompressed
	// 128bit texel / block size
	{
		{ GL_RGBA32UI, GL_RGBA32I, GL_RGBA32F },
		{
			GL_COMPRESSED_RGBA8_ETC2_EAC, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, GL_COMPRESSED_RG11_EAC,
			GL_COMPRESSED_SIGNED_RG11_EAC, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4,
			GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6,
			GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8,
			GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8,
			GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12,
		}
	},
	// 64bit texel / block size
	{
		{ GL_RGBA16F, GL_RG32F, GL_RGBA16UI, GL_RG32UI, GL_RGBA16I, GL_RG32I },
		{
			GL_COMPRESSED_RGB8_ETC2, GL_COMPRESSED_SRGB8_ETC2, GL_COMPRESSED_R11_EAC, GL_COMPRESSED_SIGNED_R11_EAC,
			GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2
		}
	},
	// Table 16.4 - only entries for compressed formats are included
	{ { GL_COMPRESSED_R11_EAC						, GL_COMPRESSED_SIGNED_R11_EAC					}, {} },
	{ { GL_COMPRESSED_RG11_EAC						, GL_COMPRESSED_SIGNED_RG11_EAC					}, {} },
	{ { GL_COMPRESSED_RGB8_ETC2						, GL_COMPRESSED_SRGB8_ETC2						}, {} },
	{ { GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2	, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2	}, {} },
	{ { GL_COMPRESSED_RGBA8_ETC2_EAC				, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_4x4					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_5x4					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_5x5					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_6x5					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_6x6					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_8x5					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_8x6					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_8x8					, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_10x5				, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_10x6				, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_10x8				, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_10x10				, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_12x10				, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10			}, {} },
	{ { GL_COMPRESSED_RGBA_ASTC_12x12				, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12			}, {} }
};

#include "glcCompressedFormatTests_data.inl"

const float				vertexPositions[]			=
{
	-1.0f, -1.0f,
	1.0f, -1.0f,
	-1.0f, 1.0f,
	1.0f, 1.0f,
};

const float				vertexTexCoords[]			=
{
	0.0f, 0.0f,
	1.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
};

const char*				vertexShader				=
	"${VERSION}\n"
	"in highp vec4 in_position;\n"
	"in highp vec2 in_texCoord;\n"
	"out highp vec2 v_texCoord;\n"
	"void main (void)\n"
	"{\n"
	"	gl_Position = in_position;\n"
	"	v_texCoord = in_texCoord;\n"
	"}\n";

const char*				fragmentShader				=
	"${VERSION}\n"
	"uniform highp vec4 offset;\n"
	"uniform highp vec4 scale;\n"
	"uniform highp sampler2D sampler;\n"
	"in highp vec2 v_texCoord;\n"
	"layout(location = 0) out highp vec4 out_color;\n"
	"void main (void)\n"
	"{\n"
	"	out_color = texture(sampler, v_texCoord) * scale + offset;\n"
	"}\n";

struct OffsetInfo
{
	Vec4	offset;
	Vec4	scale;
};

const OffsetInfo					defaultOffset		{ { 0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } };
const map<GLenum, OffsetInfo>		offsets				=
{
	{ GL_COMPRESSED_SIGNED_R11_EAC	, { { 0.5f, 0.0f, 0.0f, 0.0f }, { 0.5f, 0.0f, 0.0f, 1.0f } } },
	{ GL_COMPRESSED_SIGNED_RG11_EAC	, { { 0.5f, 0.5f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.0f, 1.0f } } }
};

class SharedData
{
public:
	explicit		SharedData	(deqp::Context& context);
	virtual			~SharedData	();

	void			init();
	void			deinit();

	GLuint			programId() const;
	GLuint			texId(int index) const;
	GLuint			vaoId() const;
	GLuint			offsetLoc() const;
	GLuint			scaleLoc() const;

private:
	deqp::Context&				m_context;
	size_t						m_initCount;
	vector<GLuint>				m_texIds;
	shared_ptr<ShaderProgram>	m_program;
	GLuint						m_vaoId;
	GLuint						m_vboIds[2];
	GLuint						m_offsetLoc;
	GLuint						m_scaleLoc;

	SharedData					(const SharedData& other) = delete;
	SharedData&		operator=	(const SharedData& other) = delete;
};

SharedData::SharedData (deqp::Context& context)
	: m_context(context)
	, m_initCount(0)
{
}

SharedData::~SharedData ()
{
	DE_ASSERT(m_initCount <= 0);
}

void SharedData::init ()
{
	++m_initCount;
	if (m_initCount > 1)
		return;

	const auto&	gl					= m_context.getRenderContext().getFunctions();
	// program
	const bool	supportsES32		= contextSupports(m_context.getRenderContext().getType(), glu::ApiType::es(3, 2));
	const auto	glslVersion			= getGLSLVersionDeclaration(supportsES32 ? glu::GLSL_VERSION_320_ES : glu::GLSL_VERSION_310_ES);
	const auto	args				= map<string, string> { { "VERSION", glslVersion } };
	const auto	vs					= StringTemplate(vertexShader).specialize(args);
	const auto	fs					= StringTemplate(fragmentShader).specialize(args);
	m_program						= make_shared<ShaderProgram>(m_context.getRenderContext(), ProgramSources() << glu::VertexSource(vs) << glu::FragmentSource(fs));
	if (!m_program->isOk())
		throw runtime_error("Compiling shader program failed");

	const auto	program				= m_program->getProgram();
	const auto	positionLoc			= gl.getAttribLocation(program, "in_position");
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation() failed");
	const auto	texCoordLoc			= gl.getAttribLocation(program, "in_texCoord");
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation() failed");
	m_offsetLoc						= gl.getUniformLocation(program, "offset");
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation() failed");
	m_scaleLoc						= gl.getUniformLocation(program, "scale");
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGetAttribLocation() failed");

	// buffers
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

	// vertex array objects
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

void SharedData::deinit ()
{
	DE_ASSERT(m_initCount > 0);
	--m_initCount;

	if (m_initCount > 0)
		return;

	const auto&	gl	= m_context.getRenderContext().getFunctions();
	gl.deleteBuffers(1, &m_vaoId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers() failed");

	gl.deleteBuffers(DE_LENGTH_OF_ARRAY(m_vboIds), m_vboIds);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers() failed");

	gl.deleteTextures(m_texIds.size(), m_texIds.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures() failed");
}

GLuint SharedData::programId () const
{
	return m_program->getProgram();
}

GLuint SharedData::texId (int index) const
{
	return m_texIds[index];
}

GLuint SharedData::vaoId () const
{
	return m_vaoId;
}

GLuint SharedData::offsetLoc () const
{
	return m_offsetLoc;
}

GLuint SharedData::scaleLoc () const
{
	return m_scaleLoc;
}

struct {
	const GLsizei			width		= 8;
	const GLsizei			height		= 8;
	const GLsizei			depth		= 6;
	const vector<deUint8>	data		=
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};
} invalidTexture;

struct ApiTestContext
{
	TestLog&				log;
	const glw::Functions&	gl;
	vector<GLuint>&			texIds;
	vector<GLuint>&			bufferIds;
	const Archive&			archive;

	void	bindTexture(GLenum target, GLuint texId);
};

void ApiTestContext::bindTexture (GLenum target, GLuint texId)
{
	gl.bindTexture(target, texId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");
}

typedef function<void (ApiTestContext&)> ApiCaseFn;

struct ApiCaseStep
{
	ApiCaseFn				code;
	GLenum					expectedError;
};

typedef function<void (deqp::Context&, vector<ApiCaseStep>&)> ApiCaseStepGeneratorFn;

struct ApiCaseParams
{
	ApiType					minApi;
	string					name;
	string					description;
	size_t					texIdsCount;
	size_t					bufferIdsCount;
	vector<ApiCaseStep>		steps;
	ApiCaseStepGeneratorFn	stepsGenerator;
};

const GLenum				cubemapFaces[] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

struct ImageInfo
{
	GLuint					width;
	GLuint					height;
	vector<GLubyte>			data;
};

ImageInfo					loadImage (const Archive& archive, GLenum format, size_t imageIndex)
{
	const auto data = imageData.find(format);
	if (data == imageData.end())
	{
		ostringstream	msg;
		msg << "No image data found for format: " << format;
		TCU_FAIL(msg.str().c_str());
	}
	if (imageIndex >= data->second.size())
	{
		ostringstream	msg;
		msg << "Image index out of range for format: " << format << " index: " << imageIndex;
		TCU_FAIL(msg.str().c_str());
	}
	const de::UniquePtr<Resource>	resource	(archive.getResource(data->second[imageIndex].path.c_str()));
	if (!resource || resource->getSize() <= 0)
		TCU_FAIL("Failed to read file: "+data->second[imageIndex].path);
	ImageInfo result;
	result.width = data->second[imageIndex].width;
	result.height = data->second[imageIndex].height;
	const auto size = resource->getSize();
	result.data.resize(size);
	resource->setPosition(0);
	resource->read(result.data.data(), size);
	return result;
}

void setTextureParameters (const glw::Functions& gl, GLenum target)
{
	gl.texParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	gl.texParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	gl.texParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	gl.texParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	const auto repeatMode = GL_CLAMP_TO_EDGE;
	gl.texParameteri(target, GL_TEXTURE_WRAP_S, repeatMode);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	gl.texParameteri(target, GL_TEXTURE_WRAP_T, repeatMode);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
	gl.texParameteri(target, GL_TEXTURE_WRAP_R, repeatMode);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri() failed");
}

ApiCaseParams	apiTests[] =
{
	{
		gles31,																		// ApiType					minApi;
		"invalid_target",															// string					name;
		"Invalid texture target for compressed format",								// string					description;
		1,																			// size_t					texIdsCount;
		0,																			// size_t					bufferIdsCount;
		{																			// vector<ApiCaseStep>		steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_3D, context.texIds[0]);
				},
				GL_NO_ERROR
			},
			{
				[](ApiTestContext& context)
				{
					context.gl.compressedTexImage2D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RGB8_ETC2, invalidTexture.width, invalidTexture.height, 0, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_ENUM
			},
			{
				[](ApiTestContext& context)
				{
					context.gl.compressedTexSubImage2D(GL_TEXTURE_3D, 0, 0, 0, invalidTexture.width, invalidTexture.height, GL_COMPRESSED_RGB8_ETC2, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_ENUM
			},
		},
		DE_NULL,																	// ApiCaseStepGeneratorFn	stepsGenerator;
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_width_or_height",													// string                   name;
		"Different values for width and height for cubemap texture target",			// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
				},
				GL_NO_ERROR
			}
		},
		[](deqp::Context&, vector<ApiCaseStep>& steps)								// ApiCaseStepGeneratorFn	stepsGenerator;
		{
			steps.push_back(
				{
					[](ApiTestContext& context)
					{
						context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
					},
					GL_NO_ERROR
				});
			for(size_t i = 0; i < DE_LENGTH_OF_ARRAY(cubemapFaces); ++i)
			{
				steps.push_back(
					{
						[i](ApiTestContext& context)
						{
							context.gl.compressedTexImage2D(cubemapFaces[i], 0, GL_COMPRESSED_RGB8_ETC2, invalidTexture.width - i % 2, invalidTexture.height - (i + 1) % 2, 0, invalidTexture.data.size(), invalidTexture.data.data());
						},
						GL_INVALID_VALUE
					});
				steps.push_back(
					{
						[i](ApiTestContext& context)
						{
							const auto		format			= GL_COMPRESSED_RGB8_ETC2;
							const GLsizei	blockSize		= 4;
							const GLsizei	blockDataSize	= 8;
							const auto		data			= loadImage(context.archive, format, 0);
							const auto&		gl				= context.gl;
							gl.compressedTexImage2D(cubemapFaces[i], 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
							GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");

							const auto		updateWidth		= invalidTexture.width - (i % 2) * blockSize;
							const auto		updateHeight	= invalidTexture.height - ((i + 1) % 2) * blockSize;
							const auto		updateDataSize	= (updateWidth / blockSize) * (updateHeight / blockSize) * blockDataSize;
							DE_ASSERT(updateDataSize <= invalidTexture.data.size());
							context.gl.compressedTexSubImage2D(cubemapFaces[i], 0, 0, 0, updateWidth, updateHeight, format, updateDataSize, invalidTexture.data.data());
						},
						GL_NO_ERROR
					});
			}
		}
	},
	{
		gles32,																		// ApiType					minApi;
		"invalid_width_or_height_array",											// string                   name;
		"Different values for width and height for cubemap texture target",			// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
				},
				GL_NO_ERROR
			},
			{
				[](ApiTestContext& context)
				{
					context.gl.compressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_COMPRESSED_RGB8_ETC2, invalidTexture.width - 1, invalidTexture.height, 6, 0, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_VALUE
			}
		},
		DE_NULL,																	// ApiCaseStepGeneratorFn	stepsGenerator;
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_size_value_negative",												// string                   name;
		"Negative width, height or imageSize for compressed texture image",			// string                   description;
		3,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn	stepsGenerator;
		{
			auto		format	= GL_COMPRESSED_RGB8_ETC2;
			const auto	data	= loadImage(testContext.getTestContext().getArchive(), format, 0);
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						DE_ASSERT(context.texIds.size() >= 3);
						context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
						context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, -1, 0, data.data.size(), data.data.data());
					},
					GL_INVALID_VALUE
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
					},
					GL_NO_ERROR
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, data.width, -1, format, data.data.size(), data.data.data());
					},
					GL_INVALID_VALUE
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						DE_ASSERT(context.texIds.size() >= 3);
						context.bindTexture(GL_TEXTURE_2D, context.texIds[1]);
						context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, -276, data.height, 0, data.data.size(), data.data.data());
					},
					GL_INVALID_VALUE
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
					},
					GL_NO_ERROR
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -276, data.height, format, data.data.size(), data.data.data());
					},
					GL_INVALID_VALUE
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						DE_ASSERT(context.texIds.size() >= 3);
						context.bindTexture(GL_TEXTURE_2D, context.texIds[2]);
						context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, -66543, data.data.data());
					},
					GL_INVALID_VALUE
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
					},
					GL_NO_ERROR
				});
			steps.push_back(
				{
					[format, data](ApiTestContext& context)
					{
						context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, data.width, data.height, format, -66543, data.data.data());
					},
					GL_INVALID_VALUE
				});
		}
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_border_nonzero",													// string                   name;
		"Non zero border values are not supported",									// string                   description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
					context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, invalidTexture.width, invalidTexture.height, 1, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_VALUE
			},
		},
		[](deqp::Context&, vector<ApiCaseStep>& steps)								// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(size_t j = 0; j < DE_LENGTH_OF_ARRAY(cubemapFaces); ++j)
				steps.push_back(
					{
						[j](ApiTestContext& context)
						{
							context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[1]);
							context.gl.compressedTexImage2D(cubemapFaces[j], 0, GL_COMPRESSED_RGB8_ETC2, invalidTexture.width, invalidTexture.height, 1, invalidTexture.data.size(), invalidTexture.data.data());
						},
						GL_INVALID_VALUE
					});
		},
	},
	{
		gles32,																		// ApiType					minApi;
		"invalid_border_nonzero_array",												// string                   name;
		"Non zero border values are not supported",									// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
					context.gl.compressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_COMPRESSED_RGB8_ETC2, invalidTexture.width, invalidTexture.height, invalidTexture.depth, 1, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_VALUE
			},
		},
		DE_NULL,																	// ApiCaseStepGeneratorFn   stepsGenerator;
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_format_mismatch",													// string                   name;
		"Subimage format differs from previously specified texture format",			// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					const auto&	gl				= context.gl;
					const auto  format0			= GL_COMPRESSED_RGB8_ETC2;
					const auto	data0			= loadImage(context.archive, format0, 0);
					const auto	format1			= GL_COMPRESSED_R11_EAC;
					const auto	data1			= loadImage(context.archive, format1, 0);
					DE_ASSERT(data0.width == data1.width && data0.height == data1.height);

					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);

					gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format0, data0.width, data0.height, 0, data0.data.size(), data0.data.data());
					GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
					gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, data1.width, data1.height, format1, data1.data.size(), data1.data.data());
				},
				GL_INVALID_OPERATION
			},
		},
		DE_NULL,																	// ApiCaseStepGeneratorFn   stepsGenerator;
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_target_3d",														// string                   name;
		"Invalid texture target for compressed texture",							// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(auto i = 0; i < DE_LENGTH_OF_ARRAY(compressedFormats); ++i)
			{
				if (!contextSupports(testContext.getRenderContext().getType(), compressedFormats[i].minApi))
					continue;

				const auto	data	= loadImage(testContext.getTestContext().getArchive(), compressedFormats[i].internalFormat, 0);
				steps.push_back(
					{
						[](ApiTestContext& context)
						{
							context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
						},
						GL_NO_ERROR
					});
				steps.push_back(
					{
						[i, data](ApiTestContext& context)
						{
							context.gl.compressedTexImage3D(GL_TEXTURE_2D, 0, compressedFormats[i].internalFormat, data.width, data.height, 1, 0, data.data.size(), data.data.data());
						},
						GL_INVALID_ENUM
					});
				steps.push_back(
					{
						[i, data](ApiTestContext& context)
						{
							context.gl.compressedTexSubImage3D(GL_TEXTURE_2D, 0, 0, 0, 0, data.width, data.height, 1, compressedFormats[i].internalFormat, data.data.size(), data.data.data());
						},
						GL_INVALID_ENUM
					});
			}
		}
	},
	{
		gles31,																		// ApiType					minApi;
		"texstorage_accepts_compressed_format",										// string                   name;
		"TexStorage should accept compressed format",								// string                   description;
		DE_LENGTH_OF_ARRAY(compressedFormats),										// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(auto i = 0; i < DE_LENGTH_OF_ARRAY(compressedFormats); ++i)
			{
				if (!contextSupports(testContext.getRenderContext().getType(), compressedFormats[i].minApi))
					continue;

				steps.push_back(
					{
						[i](ApiTestContext& context)
						{
							const auto&		gl				= context.gl;
							const size_t	textureWidth	= 240;
							const size_t	textureHeight	= 240;
							context.bindTexture(GL_TEXTURE_2D, context.texIds[i]);
							gl.texStorage2D(GL_TEXTURE_2D, 1, compressedFormats[i].internalFormat, textureWidth, textureHeight);
						},
						GL_NO_ERROR
					});
			}
		}
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_teximage_with_compressed_format",									// string                   name;
		"TexImage should not accept compressed format",								// string                   description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(auto i = 0; i < DE_LENGTH_OF_ARRAY(compressedFormats); ++i)
			{
				const auto	format	= compressedFormats[i];
				if (!contextSupports(testContext.getRenderContext().getType(), format.minApi))
					continue;

				const auto	data	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 0);
				steps.push_back(
					{
						[format, data](ApiTestContext& context)
						{
							const auto&	gl		= context.gl;
							context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
							gl.texImage2D(GL_TEXTURE_2D, 0, format.internalFormat, data.width, data.height, 0, format.format, GL_UNSIGNED_BYTE, data.data.data());
						},
						GL_INVALID_VALUE
					});
				steps.push_back(
					{
						[format, data](ApiTestContext& context)
						{
							const auto&	gl		= context.gl;
							context.bindTexture(GL_TEXTURE_3D, context.texIds[1]);
							gl.texImage3D(GL_TEXTURE_3D, 0, format.internalFormat, data.width, data.height, 1, 0, format.format, GL_UNSIGNED_BYTE, data.data.data());
						},
						GL_INVALID_VALUE
					});
			}
		}
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_format",															// string                   name;
		"Uncompressed internal format for compressed texture",						// string                   description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_2D, context.texIds[1]);
				},
				GL_NO_ERROR
			},
			{
				[](ApiTestContext& context)
				{
					context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, invalidTexture.width, invalidTexture.height, 0, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_ENUM
			},
			{
				[](ApiTestContext& context)
				{
					context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, invalidTexture.width, invalidTexture.height, GL_RGB, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_OPERATION
			},
			{
				[](ApiTestContext& context)
				{
					const GLenum	format	= GL_COMPRESSED_RGB8_ETC2;
					const auto		data	= loadImage(context.archive, format, 0);
					const auto&		gl		= context.gl;
					gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
					GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");

					context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, invalidTexture.width, invalidTexture.height, GL_RGB, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_OPERATION
			},
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
				},
				GL_NO_ERROR
			}
		},
		[](deqp::Context&, vector<ApiCaseStep>& steps)								// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(size_t j = 0; j < DE_LENGTH_OF_ARRAY(cubemapFaces); ++j)
			{
				steps.push_back(
					{
						[j](ApiTestContext& context)
						{
							context.gl.compressedTexImage2D(cubemapFaces[j], 0, GL_RGB, invalidTexture.width, invalidTexture.height, 0, invalidTexture.data.size(), invalidTexture.data.data());
						},
						GL_INVALID_ENUM
					});
				steps.push_back(
					{
						[j](ApiTestContext& context)
						{
							const GLenum	format	= GL_COMPRESSED_RGB8_ETC2;
							const auto		data	= loadImage(context.archive, format, 0);
							const auto&		gl		= context.gl;
							gl.compressedTexImage2D(cubemapFaces[j], 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
							GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");

							context.gl.compressedTexSubImage2D(cubemapFaces[j], 0, 0, 0, invalidTexture.width, invalidTexture.height, GL_RGB, invalidTexture.data.size(), invalidTexture.data.data());
						},
						GL_INVALID_OPERATION
					});
			}
		}
	},
	{
		gles32,																		// ApiType					minApi;
		"invalid_format_array",														// string                   name;
		"Uncompressed internal format for compressed texture",						// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
				},
				GL_NO_ERROR
			},
			{
				[](ApiTestContext& context)
				{
					context.gl.compressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGB, invalidTexture.width, invalidTexture.height, 6, 0, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_ENUM
			},
			{
				[](ApiTestContext& context)
				{
					const GLenum	format	= GL_COMPRESSED_RGB8_ETC2;
					const auto		data	= loadImage(context.archive, format, 0);
					const auto&		gl		= context.gl;
					vector<GLubyte> arrayData;
					arrayData.reserve(6 * data.data.size());
					for(size_t k = 0; k < 6; ++k)
						std::copy(data.data.begin(), data.data.end(), std::back_inserter(arrayData));

					context.gl.compressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, format, data.width, data.height, 6, 0, arrayData.size(), arrayData.data());
					GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage3D() failed");

					context.gl.compressedTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, invalidTexture.width, invalidTexture.height, 6, GL_RGB, invalidTexture.data.size(), invalidTexture.data.data());context.gl.compressedTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, invalidTexture.width, invalidTexture.height, 6, GL_RGB, invalidTexture.data.size(), invalidTexture.data.data());
				},
				GL_INVALID_OPERATION
			}
		},
		DE_NULL																		// ApiCaseStepGeneratorFn   stepsGenerator;
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_too_small_unpack_buffer",											// string                   name;
		"Pixel unpack buffer with not enough space for required texture data",		// string                   description;
		1,																			// size_t                   texIdsCount;
		1,																			// size_t                   bufferIdsCount;
		{																			// vector<ApiCaseStep>      steps;
			{
				[](ApiTestContext& context)
				{
					const GLenum	format	= GL_COMPRESSED_RGB8_ETC2;
					const auto		data	= loadImage(context.archive, format, 0);
					const auto&		gl		= context.gl;

					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
					gl.bindBuffer(GL_PIXEL_UNPACK_BUFFER, context.bufferIds[0]);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");
					gl.bufferData(GL_PIXEL_UNPACK_BUFFER, data.data.size() / 2, data.data.data(), GL_STATIC_READ);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData() failed");
					gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, data.data.size(), 0);
				},
				GL_INVALID_OPERATION
			},
			{
				[](ApiTestContext& context)
				{
					const GLenum	format	= GL_COMPRESSED_RGB8_ETC2;
					const auto		data	= loadImage(context.archive, format, 0);
					const auto&		gl		= context.gl;

					context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
					gl.bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
					gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0, data.data.size(), data.data.data());
					GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
					gl.bindBuffer(GL_PIXEL_UNPACK_BUFFER, context.bufferIds[0]);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer() failed");
					gl.bufferData(GL_PIXEL_UNPACK_BUFFER, data.data.size() / 2, data.data.data(), GL_STATIC_READ);
					GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData() failed");
					gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, data.width, data.height, format, data.data.size(), 0);
				},
				GL_INVALID_OPERATION
			}
		},
		DE_NULL																		// ApiCaseStepGeneratorFn   stepsGenerator;
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_inconsistent_data_size",											// string                   name;
		"Data size is not consistent with texture internal format and dimensions",	// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(auto i = 0; i < DE_LENGTH_OF_ARRAY(compressedFormats); ++i)
			{
				const auto& format	= compressedFormats[i];
				if (!contextSupports(testContext.getRenderContext().getType(), format.minApi))
					continue;

				const auto	data0	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 0);
				const auto	data1	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 1);

				steps.push_back(
					{
						[format, data0](ApiTestContext& context)
						{
							const auto&	gl		= context.gl;
							context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
							gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format.internalFormat, data0.width - 12, data0.height - 12, 0, data0.data.size(), data0.data.data());
						},
						GL_INVALID_VALUE
					});
			}
		}
	},
	{
		gles32,																		// ApiType					minApi;
		"invalid_inconsistent_data_size_array",										// string                   name;
		"Data size is not consistent with texture internal format and dimensions",	// string                   description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(auto i = 0; i < DE_LENGTH_OF_ARRAY(compressedFormats); ++i)
			{
				const auto& format	= compressedFormats[i];
				if (!contextSupports(testContext.getRenderContext().getType(), format.minApi))
					continue;

				const auto	data0	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 0);
				const auto	data1	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 1);
				steps.push_back(
					{
						[format, data0](ApiTestContext& context)
						{
							context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[0]);
						},
						GL_NO_ERROR
					});
				for(size_t j = 0; j < DE_LENGTH_OF_ARRAY(cubemapFaces); ++j)
					steps.push_back(
						{
							[j, format, data0](ApiTestContext& context)
							{
								context.gl.compressedTexImage2D(cubemapFaces[j], 0, format.internalFormat, data0.width, data0.height, 0, data0.data.size(), data0.data.data());
							},
							GL_NO_ERROR
						});
				steps.push_back(
					{
						[format, data0](ApiTestContext& context)
						{
							vector<GLubyte> arrayData;
							arrayData.reserve(6 * data0.data.size());
							for(size_t k = 0; k < 6; ++k)
								std::copy(data0.data.begin(), data0.data.end(), std::back_inserter(arrayData));
							context.bindTexture(GL_TEXTURE_CUBE_MAP, context.texIds[1]);
							context.gl.compressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, format.internalFormat, data0.width, data0.height, 6, 0, arrayData.size(), arrayData.data());
						},
						GL_NO_ERROR
					});
				steps.push_back(
					{
						[format, data1](ApiTestContext& context)
						{
							context.gl.compressedTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, data1.width, data1.height, 1, format.internalFormat, data1.data.size() - 1, data1.data.data());
						},
						GL_INVALID_VALUE
					});
			}
		}
	},
	{
		gles31,																		// ApiType					minApi;
		"invalid_offset_or_size",													// string                   name;
		"Offset or image size not aligned with block size",							// string                   description;
		1,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context& testContext, vector<ApiCaseStep>& steps)					// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(auto i = 0; i < DE_LENGTH_OF_ARRAY(compressedFormats); ++i)
			{
				const auto&	format	= compressedFormats[i];
				if (!contextSupports(testContext.getRenderContext().getType(), format.minApi))
					continue;

				const auto	data0	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 0);
				const auto	data1	= loadImage(testContext.getTestContext().getArchive(), format.internalFormat, 1);
				steps.push_back(
					{
						[format, data0](ApiTestContext& context)
						{
							context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
							context.gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format.internalFormat, data0.width, data0.height, 0, data0.data.size(), data0.data.data());
						},
						GL_NO_ERROR
					});
				steps.push_back(
					{
						[format, data1](ApiTestContext& context)
						{
							context.gl.compressedTexImage2D(GL_TEXTURE_2D, 1, format.internalFormat, data1.width, data1.height, 0, data1.data.size(), data1.data.data());
						},
						GL_NO_ERROR
					});
				steps.push_back(
					{
						[format, data1](ApiTestContext& context)
						{
							context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, format.blockSize.x() - 2, 0, data1.width, data1.height, format.internalFormat, data1.data.size(), data1.data.data());
						},
						GL_INVALID_OPERATION
					});
				steps.push_back(
					{
						[format, data1](ApiTestContext& context)
						{
							context.gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, format.blockSize.y() - 2, data1.width, data1.height, format.internalFormat, data1.data.size(), data1.data.data());
						},
						GL_INVALID_OPERATION
					});
			}
		}
	},
	{
		gles32,																		// ApiType					minApi;
		"copy_compressed_to_uncompressed",											// string					name;
		"Copying pixels from compressed to uncompressed texture",					// string					description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context&, vector<ApiCaseStep>& steps)								// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(const auto& format : copyFormats)
			{
				if (format.second.empty())
					continue;
				for(const auto& uncompressedFormat : format.first)
				{
					for(const auto& compressedFormat : format.second)
					{
						steps.push_back(
							{
								[uncompressedFormat, compressedFormat](ApiTestContext& context)
								{
									const auto&	gl				= context.gl;
									const auto&	image			= imageData.at(compressedFormat);
									const auto& unsizedInfo		= unsizedFormats.at(uncompressedFormat);
									const auto	textureData		= loadImage(context.archive, compressedFormat, 0);
									const auto	compressedInfo	= find_if(begin(compressedFormats), end(compressedFormats), [compressedFormat](const FormatInfo& fmt) { return fmt.internalFormat == compressedFormat; });

									DE_ASSERT((GLsizei)textureData.width == image[0].width && (GLsizei)textureData.height == image[0].height);
									DE_ASSERT(compressedInfo != end(compressedFormats));

									const auto	targetWidth		= image[0].width / compressedInfo->blockSize[0];
									const auto	targetHeight	= image[0].height / compressedInfo->blockSize[1];

									context.log
										<< TestLog::Message
										<< "Copying from " << getTextureFormatStr(compressedFormat).toString() << " " << image[0].width << "x" << image[0].height
										<< " to " << getTextureFormatStr(uncompressedFormat).toString() << " " << targetWidth << "x" << targetHeight
										<< TestLog::EndMessage;

									context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
									gl.texImage2D(GL_TEXTURE_2D, 0, uncompressedFormat, targetWidth, targetHeight, 0, unsizedInfo.format, unsizedInfo.dataType, 0);
									GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D() failed");
									setTextureParameters(context.gl, GL_TEXTURE_2D);

									context.bindTexture(GL_TEXTURE_2D, context.texIds[1]);
									gl.compressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormat, image[0].width, image[0].height, 0, textureData.data.size(), textureData.data.data());
									GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
									setTextureParameters(context.gl, GL_TEXTURE_2D);

									context.bindTexture(GL_TEXTURE_2D, 0);

									gl.copyImageSubData(context.texIds[1], GL_TEXTURE_2D, 0, 0, 0, 0, context.texIds[0], GL_TEXTURE_2D, 0, 0, 0, 0, image[0].width, image[0].height, 1);
								},
								GL_NO_ERROR
							});
					}
				}
			}
		}
	},
	{
		gles32,																		// ApiType					minApi;
		"copy_uncompressed_to_compressed",											// string					name;
		"Copying pixels from uncompressed to compressed texture",					// string					description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context&, vector<ApiCaseStep>& steps)								// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(const auto& format : copyFormats)
			{
				if (format.second.empty())
					continue;
				for(const auto& uncompressedFormat : format.first)
				{
					for(const auto& compressedFormat : format.second)
					{
						steps.push_back(
							{
								[uncompressedFormat, compressedFormat](ApiTestContext& context)
								{
									const auto&	gl				= context.gl;
									const auto&	image			= imageData.at(compressedFormat);
									const auto& unsizedInfo		= unsizedFormats.at(uncompressedFormat);
									const auto	textureData		= loadImage(context.archive, compressedFormat, 0);
									const auto	compressedInfo	= find_if(begin(compressedFormats), end(compressedFormats), [compressedFormat](const FormatInfo& fmt) { return fmt.internalFormat == compressedFormat; });

									DE_ASSERT(compressedInfo != end(compressedFormats));
									const auto	sourceWidth		= image[0].width / compressedInfo->blockSize[0];
									const auto	sourceHeight	= image[0].height / compressedInfo->blockSize[1];

									DE_ASSERT((GLsizei)textureData.width == image[0].width && (GLsizei)textureData.height == image[0].height);

									context.log
										<< TestLog::Message
										<< "Copying from " << getTextureFormatStr(uncompressedFormat).toString() << " " << sourceWidth << "x" << sourceHeight
										<< " to " << getTextureFormatStr(compressedFormat).toString() << " " << image[0].width << "x" << image[0].height
										<< TestLog::EndMessage;

									context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
									gl.texImage2D(GL_TEXTURE_2D, 0, uncompressedFormat, sourceWidth, sourceHeight, 0, unsizedInfo.format, unsizedInfo.dataType, 0);
									GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D() failed");
									setTextureParameters(context.gl, GL_TEXTURE_2D);

									context.bindTexture(GL_TEXTURE_2D, context.texIds[1]);
									gl.compressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormat, image[0].width, image[0].height, 0, textureData.data.size(), textureData.data.data());
									GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
									setTextureParameters(context.gl, GL_TEXTURE_2D);

									context.bindTexture(GL_TEXTURE_2D, 0);
									gl.copyImageSubData(context.texIds[0], GL_TEXTURE_2D, 0, 0, 0, 0, context.texIds[1], GL_TEXTURE_2D, 0, 0, 0, 0, sourceWidth, sourceHeight, 1);
								},
								GL_NO_ERROR
							});
					}
				}
			}
		}
	},
	{
		gles32,																		// ApiType					minApi;
		"copy_compressed_to_compressed",											// string					name;
		"Copying of pixels between compatible compressed texture formats",			// string					description;
		2,																			// size_t                   texIdsCount;
		0,																			// size_t                   bufferIdsCount;
		{},																			// vector<ApiCaseStep>      steps;
		[](deqp::Context&, vector<ApiCaseStep>& steps)								// ApiCaseStepGeneratorFn   stepsGenerator;
		{
			for(const auto& format : copyFormats)
			{
				if (!format.second.empty())
					continue;
				for(const auto& format0 : format.first)
				{
					for(const auto& format1 : format.first)
					{
						steps.push_back(
							{
								[format0, format1](ApiTestContext& context)
								{
									const auto&	gl		= context.gl;
									const auto image0	= loadImage(context.archive, format0, 0);
									const auto image1	= loadImage(context.archive, format1, 1);

									DE_ASSERT(image0.width == 2 * image1.width && image0.height == 2 * image1.height);

									context.bindTexture(GL_TEXTURE_2D, context.texIds[0]);
									gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format0, image0.width, image0.height, 0, image0.data.size(), image0.data.data());
									GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
									setTextureParameters(context.gl, GL_TEXTURE_2D);

									context.bindTexture(GL_TEXTURE_2D, context.texIds[1]);
									gl.compressedTexImage2D(GL_TEXTURE_2D, 0, format1, image1.width, image1.height, 0, image1.data.size(), image1.data.data());
									GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
									setTextureParameters(context.gl, GL_TEXTURE_2D);

									context.bindTexture(GL_TEXTURE_2D, 0);

									gl.copyImageSubData(context.texIds[1], GL_TEXTURE_2D, 0, 0, 0, 0, context.texIds[0], GL_TEXTURE_2D, 0, 0, 0, 0, image1.width, image1.height, 1);
								},
								GL_NO_ERROR
							});
					}
				}
			}
		}
	}
};

class CompressedApiTest : public deqp::TestCase
{
public:
	explicit				CompressedApiTest	(deqp::Context& context, const ApiCaseParams& params);
	virtual					~CompressedApiTest	();

	virtual void			init				(void) override;
	virtual void			deinit				(void) override;

	virtual IterateResult	iterate				(void) override;
private:
	ApiCaseParams	m_params;
	vector<GLuint>	m_texIds;
	vector<GLuint>	m_bufferIds;
};

CompressedApiTest::CompressedApiTest (deqp::Context& context, const ApiCaseParams& params)
	: deqp::TestCase(context, params.name.c_str(), params.description.c_str())
	, m_params(params)
{
}

CompressedApiTest::~CompressedApiTest ()
{
}

void CompressedApiTest::init (void)
{
	const auto&	gl	= m_context.getRenderContext().getFunctions();
	if (m_params.texIdsCount > 0)
	{
		m_texIds.resize(m_params.texIdsCount);
		gl.genTextures(m_texIds.size(), m_texIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");
		m_bufferIds.resize(m_params.bufferIdsCount);
		gl.genBuffers(m_bufferIds.size(), m_bufferIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers() failed");
	}
}

void CompressedApiTest::deinit (void)
{
	const auto&	gl	= m_context.getRenderContext().getFunctions();
	if (!m_bufferIds.empty())
	{
		gl.deleteBuffers(m_bufferIds.size(), m_bufferIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers() failed");
		m_bufferIds.erase(m_bufferIds.begin(), m_bufferIds.end());
	}
	if (!m_texIds.empty())
	{
		gl.deleteTextures(m_texIds.size(), m_texIds.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures() failed");
		m_texIds.erase(m_texIds.begin(), m_texIds.end());
	}
}

CompressedApiTest::IterateResult CompressedApiTest::iterate (void)
{
	const auto&				gl			= m_context.getRenderContext().getFunctions();
	ApiTestContext			caseContext	=
	{
		m_context.getTestContext().getLog(),
		gl,
		m_texIds,
		m_bufferIds,
		m_context.getTestContext().getArchive()
	};
	vector<ApiCaseStep>		steps		(m_params.steps);
	if (m_params.stepsGenerator)
		m_params.stepsGenerator(m_context, steps);
	size_t stepIndex = 0;
	for(const auto& step : steps)
	{
		step.code(caseContext);
		const auto	errorCode	= gl.getError();
		if (errorCode != step.expectedError)
		{
			ostringstream msg;
			msg << "Got wrong error code: " << glu::getErrorStr(errorCode)
				<< ", expected: " << glu::getErrorStr(step.expectedError)
				<< " after step " << stepIndex;
			TCU_FAIL(msg.str().c_str());
		}
		++stepIndex;
	}
	m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return IterateResult::STOP;
}

class CompressedFormatTest : public deqp::TestCase
{
public:
							CompressedFormatTest	(deqp::Context& context, shared_ptr<SharedData> data, const FormatInfo& format);
	virtual					~CompressedFormatTest	();

	virtual void			init					(void);
	virtual void			deinit					(void);
	virtual IterateResult	iterate					(void);
private:
	Surface					drawTestImage			(const glw::Functions& gl, GLuint texId, GLsizei width, GLsizei height);

	shared_ptr<SharedData>	m_data;
	const FormatInfo&		formatInfo;
};

CompressedFormatTest::CompressedFormatTest (deqp::Context& context, shared_ptr<SharedData> data, const FormatInfo& format)
	: deqp::TestCase(context, format.name, "Test rendering of compressed format ")
	, m_data(data)
	, formatInfo(format)
{
}

CompressedFormatTest::~CompressedFormatTest ()
{
}

void CompressedFormatTest::init (void)
{
	m_data->init();
}

void CompressedFormatTest::deinit (void)
{
	m_data->deinit();
}

Surface CompressedFormatTest::drawTestImage (const glw::Functions& gl, GLuint texId, GLsizei width, GLsizei height)
{
	gl.clearColor(1.0f, 0.2f, 1.0f, 1.0f);
	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glClear() failed");

	gl.disable(GL_BLEND);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDisable() failed");

	gl.bindTexture(GL_TEXTURE_2D, texId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");

	gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDrawArrays() failed");

	gl.bindTexture(GL_TEXTURE_2D, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");

	Surface result(width, height);
	readPixels(m_context.getRenderContext(), 0, 0, result.getAccess());
	return result;
}

CompressedFormatTest::IterateResult CompressedFormatTest::iterate (void)
{
	const auto&	archive			= m_context.getTestContext().getArchive();
	const auto	image0			= loadImage(archive, formatInfo.internalFormat, 0);
	const auto	image1			= loadImage(archive, formatInfo.internalFormat, 1);
	const auto	image2			= loadImage(archive, formatInfo.internalFormat, 2);

	DE_ASSERT(image0.width		== 2 * image1.width &&
			  image0.height		== 2 * image1.height &&
			  image0.width % 4	== 0 &&
			  image0.height % 4	== 0 &&
			  image0.width		== image2.width &&
			  image0.height		== image2.height);

	const auto& gl				= m_context.getRenderContext().getFunctions();

	GLuint		texIds[2];
	gl.genTextures(DE_LENGTH_OF_ARRAY(texIds), texIds);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures() failed");

	GLuint		fboId;
	gl.genFramebuffers(1, &fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenFramebuffers() failed");

	GLuint		rboId;
	gl.genRenderbuffers(1, &rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenRenderbuffers() failed");

	gl.bindRenderbuffer(GL_RENDERBUFFER, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindRenderbuffer() failed");

	const GLenum	bufferFormats[2][2]	= {
		{ GL_RGB8			, GL_SRGB8_ALPHA8	},
		{ GL_RGBA8			, GL_SRGB8_ALPHA8	}
	};
	const bool		hasAlpha			= formatInfo.format == GL_RGBA;
	gl.renderbufferStorage(GL_RENDERBUFFER, bufferFormats[hasAlpha][formatInfo.issRGB], image0.width, image0.height);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glRenderbufferStorage() failed");

	gl.bindRenderbuffer(GL_RENDERBUFFER, 0);

	gl.bindFramebuffer(GL_FRAMEBUFFER, fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindFramebuffer() failed");

	gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glFramebufferRenderbuffer() failed");

	gl.viewport(0,0, image0.width, image0.height);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glViewport() failed");

	gl.useProgram(m_data->programId());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram() failed");

	gl.uniform4fv(m_data->offsetLoc(), 1, defaultOffset.offset.getPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform4fv() failed");
	gl.uniform4fv(m_data->scaleLoc(), 1, defaultOffset.scale.getPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform4fv() failed");

	gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBlendFunc() failed");
	gl.disable(GL_BLEND);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDisable() failed");

	// reference image
	gl.bindTexture(GL_TEXTURE_2D, texIds[0]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");
	gl.texImage2D(GL_TEXTURE_2D, 0, formatInfo.sizedFormat, image2.width, image2.height, 0, formatInfo.format, GL_UNSIGNED_BYTE, image2.data.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D() failed");
	setTextureParameters(gl, GL_TEXTURE_2D);

	// compressed image
	gl.bindTexture(GL_TEXTURE_2D, texIds[1]);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");
	gl.compressedTexImage2D(GL_TEXTURE_2D, 0, formatInfo.internalFormat, image0.width, image0.height, 0, image0.data.size(), image0.data.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexImage2D() failed");
	gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image1.width, image1.height, formatInfo.internalFormat, image1.data.size(), image1.data.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glCompressedTexSubImage2D() failed");
	setTextureParameters(gl, GL_TEXTURE_2D);

	gl.bindTexture(GL_TEXTURE_2D, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() failed");

	gl.bindVertexArray(m_data->vaoId());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindVertexArray() failed");

	gl.useProgram(m_data->programId());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram() failed");

	gl.uniform4fv(m_data->offsetLoc(), 1, defaultOffset.offset.getPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform4fv() failed");
	gl.uniform4fv(m_data->scaleLoc(), 1, defaultOffset.scale.getPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform4fv() failed");
	const auto	referenceImage	= drawTestImage(gl, texIds[0], image0.width, image0.height);

	const auto&	offsetIt	= offsets.find(formatInfo.internalFormat);
	const auto&	offset		= offsetIt != offsets.end() ? offsetIt->second : defaultOffset;
	gl.uniform4fv(m_data->offsetLoc(), 1, offset.offset.getPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform4fv() failed");
	gl.uniform4fv(m_data->scaleLoc(), 1, offset.scale.getPtr());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform4fv() failed");
	const auto	compressedImage	= drawTestImage(gl, texIds[1], image0.width, image0.height);

	gl.disable(GL_BLEND);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDisable() failed");

	gl.bindTexture(GL_TEXTURE_2D, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindtexture() failed");

	gl.deleteRenderbuffers(1, &rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteRenderbuffers() failed");

	gl.deleteTextures(DE_LENGTH_OF_ARRAY(texIds), texIds);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures() failed");

	gl.deleteFramebuffers(1, &fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteFramebuffers() failed");

	if (!fuzzyCompare(m_testCtx.getLog(), "compressed_vs_uncompressed", "Image comparison result", referenceImage, compressedImage, 0.0f, CompareLogMode::COMPARE_LOG_ON_ERROR))
		TCU_FAIL("Rendered image comparison failed.");

	m_context.getTestContext().setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return IterateResult::STOP;
}

} //

CompressedFormatTests::CompressedFormatTests (deqp::Context& context)
	: deqp::TestCaseGroup(context, "compressed_format", "Tests for compressed image formats")
{
}

CompressedFormatTests::~CompressedFormatTests (void)
{
}

void CompressedFormatTests::init (void)
{
	const auto apiGroup		= new TestCaseGroup(m_context, "api", "Api call return values");
	addChild(apiGroup);
	for(const auto& apiCase : apiTests)
		if (glu::contextSupports(m_context.getRenderContext().getType(), apiCase.minApi))
			apiGroup->addChild(new CompressedApiTest(m_context, apiCase));

	const auto formatGroup	= new TestCaseGroup(m_context, "format", "Compressed format textures");
	addChild(formatGroup);
	const auto	sharedData	= make_shared<SharedData>(m_context);
	for(const auto& formatInfo : compressedFormats)
		if (glu::contextSupports(m_context.getRenderContext().getType(), formatInfo.minApi))
			formatGroup->addChild(new CompressedFormatTest(m_context, sharedData, formatInfo));
}

} // glcts
