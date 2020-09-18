/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2020 Valve Coporation.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \file  glcNearestEdgeTests.cpp
 * \brief
 */ /*-------------------------------------------------------------------*/

#include "glcNearestEdgeTests.hpp"

#include "gluDefs.hpp"
#include "gluTextureUtil.hpp"
#include "gluDrawUtil.hpp"
#include "gluShaderProgram.hpp"

#include "glwDefs.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

#include <utility>
#include <map>
#include <algorithm>
#include <memory>
#include <cmath>

namespace glcts
{

namespace
{

enum class OffsetDirection
{
	LEFT	= 0,
	RIGHT	= 1,
};

// Test sampling at the edge of texels. This test is equivalent to:
//  1) Creating a texture using the same format and size as the frame buffer.
//  2) Drawing a full screen quad with GL_NEAREST using the texture.
//  3) Verifying the frame buffer image and the texture match pixel-by-pixel.
//
// However, texture coodinates are not located in the exact frame buffer corners. A small offset is applied instead so sampling
// happens near a texel border instead of in the middle of the texel.
class NearestEdgeTestCase : public deqp::TestCase
{
public:
	NearestEdgeTestCase(deqp::Context& context, OffsetDirection direction);

	void							deinit();
	void							init();
	tcu::TestNode::IterateResult	iterate();

	static std::string				getName			(OffsetDirection direction);
	static std::string				getDesc			(OffsetDirection direction);
	static tcu::TextureFormat		toTextureFormat	(const tcu::PixelFormat& pixelFmt);

private:
	static const glw::GLenum kTextureType	= GL_TEXTURE_2D;

	void createTexture	();
	void deleteTexture	();
	void fillTexture	();
	void renderQuad		();
	bool verifyResults	();

	const float						m_offsetSign;
	const int						m_width;
	const int						m_height;
	const tcu::PixelFormat&			m_format;
	const tcu::TextureFormat		m_texFormat;
	const tcu::TextureFormatInfo	m_texFormatInfo;
	const glu::TransferFormat		m_transFormat;
	std::string						m_vertShaderText;
	std::string						m_fragShaderText;
	glw::GLuint						m_texture;
	std::vector<deUint8>			m_texData;
};

std::string NearestEdgeTestCase::getName (OffsetDirection direction)
{
	switch (direction)
	{
	case OffsetDirection::LEFT:		return "offset_left";
	case OffsetDirection::RIGHT:	return "offset_right";
	default: DE_ASSERT(false); break;
	}
	// Unreachable.
	return "";
}

std::string NearestEdgeTestCase::getDesc (OffsetDirection direction)
{
	switch (direction)
	{
	case OffsetDirection::LEFT:		return "Sampling point near the left edge";
	case OffsetDirection::RIGHT:	return "Sampling point near the right edge";
	default: DE_ASSERT(false); break;
	}
	// Unreachable.
	return "";
}

// Translate pixel format in the frame buffer to texture format.
// Copied from sglrReferenceContext.cpp.
tcu::TextureFormat NearestEdgeTestCase::toTextureFormat (const tcu::PixelFormat& pixelFmt)
{
	static const struct
	{
		tcu::PixelFormat	pixelFmt;
		tcu::TextureFormat	texFmt;
	} pixelFormatMap[] =
	{
		{ tcu::PixelFormat(8,8,8,8),	tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNORM_INT8)				},
		{ tcu::PixelFormat(8,8,8,0),	tcu::TextureFormat(tcu::TextureFormat::RGB,		tcu::TextureFormat::UNORM_INT8)				},
		{ tcu::PixelFormat(4,4,4,4),	tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNORM_SHORT_4444)		},
		{ tcu::PixelFormat(5,5,5,1),	tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNORM_SHORT_5551)		},
		{ tcu::PixelFormat(5,6,5,0),	tcu::TextureFormat(tcu::TextureFormat::RGB,		tcu::TextureFormat::UNORM_SHORT_565)		},
		{ tcu::PixelFormat(10,10,10,2), tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNORM_INT_1010102_REV)	},
		{ tcu::PixelFormat(16,16,16,16), tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::HALF_FLOAT)				},
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(pixelFormatMap); ndx++)
	{
		if (pixelFormatMap[ndx].pixelFmt == pixelFmt)
			return pixelFormatMap[ndx].texFmt;
	}

	TCU_FAIL("Unable to map pixel format to texture format");
}

NearestEdgeTestCase::NearestEdgeTestCase (deqp::Context& context, OffsetDirection direction)
	: TestCase(context, getName(direction).c_str(), getDesc(direction).c_str())
	, m_offsetSign		{(direction == OffsetDirection::LEFT) ? -1.0f : 1.0f}
	, m_width			{context.getRenderTarget().getWidth()}
	, m_height			{context.getRenderTarget().getHeight()}
	, m_format			{context.getRenderTarget().getPixelFormat()}
	, m_texFormat		{toTextureFormat(m_format)}
	, m_texFormatInfo	{tcu::getTextureFormatInfo(m_texFormat)}
	, m_transFormat		{glu::getTransferFormat(m_texFormat)}
{
}

void NearestEdgeTestCase::deinit()
{
}

void NearestEdgeTestCase::init()
{
	if (m_width < 2 || m_height < 2)
		TCU_THROW(NotSupportedError, "Render target size too small");

	m_vertShaderText =
		"#version ${VERSION}\n"
		"\n"
		"in highp vec2 position;\n"
		"in highp vec2 inTexCoord;\n"
		"out highp vec2 commonTexCoord;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    commonTexCoord = inTexCoord;\n"
		"    gl_Position = vec4(position, 0.0, 1.0);\n"
		"}\n"
		;

	m_fragShaderText =
		"#version ${VERSION}\n"
		"\n"
		"in highp vec2 commonTexCoord;\n"
		"out highp vec4 fragColor;\n"
		"\n"
		"uniform highp sampler2D texSampler;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    fragColor = texture(texSampler, commonTexCoord);\n"
		"}\n"
		"\n";

	tcu::StringTemplate vertShaderTemplate{m_vertShaderText};
	tcu::StringTemplate fragShaderTemplate{m_fragShaderText};
	std::map<std::string, std::string> replacements;

	if (glu::isContextTypeGLCore(m_context.getRenderContext().getType()))
		replacements["VERSION"] = "130";
	else
		replacements["VERSION"] = "300 es";

	m_vertShaderText = vertShaderTemplate.specialize(replacements);
	m_fragShaderText = fragShaderTemplate.specialize(replacements);
}

void NearestEdgeTestCase::createTexture ()
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	gl.genTextures(1, &m_texture);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glGenTextures");
	gl.bindTexture(kTextureType, m_texture);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture");

	gl.texParameteri(kTextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
	gl.texParameteri(kTextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
	gl.texParameteri(kTextureType, GL_TEXTURE_WRAP_S, GL_REPEAT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
	gl.texParameteri(kTextureType, GL_TEXTURE_WRAP_T, GL_REPEAT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
	gl.texParameteri(kTextureType, GL_TEXTURE_MAX_LEVEL, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexParameteri");
}

void NearestEdgeTestCase::deleteTexture ()
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	gl.deleteTextures(1, &m_texture);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteTextures");
}

void NearestEdgeTestCase::fillTexture ()
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	m_texData.resize(m_width * m_height * tcu::getPixelSize(m_texFormat));
	tcu::PixelBufferAccess texAccess{m_texFormat, m_width, m_height, 1, m_texData.data()};

	// Create gradient over the whole texture.
	DE_ASSERT(m_width > 1);
	DE_ASSERT(m_height > 1);

	const float divX = static_cast<float>(m_width - 1);
	const float divY = static_cast<float>(m_height - 1);

	for (int x = 0; x < m_width; ++x)
	for (int y = 0; y < m_height; ++y)
	{
		const float colorX = static_cast<float>(x) / divX;
		const float colorY = static_cast<float>(y) / divY;
		const float colorZ = std::min(colorX, colorY);

		tcu::Vec4 color{colorX, colorY, colorZ, 1.0f};
		tcu::Vec4 finalColor = (color - m_texFormatInfo.lookupBias) / m_texFormatInfo.lookupScale;
		texAccess.setPixel(finalColor, x, y);
	}

	const auto internalFormat = glu::getInternalFormat(m_texFormat);
	if (tcu::getPixelSize(m_texFormat) < 4)
		gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.texImage2D(kTextureType, 0, internalFormat, m_width,  m_height, 0 /* border */, m_transFormat.format, m_transFormat.dataType, m_texData.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glTexImage2D");
}

// Draw full screen quad with the texture and an offset of almost half a texel in one direction, so sampling happens near the texel
// border and verifies truncation is happening properly.
void NearestEdgeTestCase::renderQuad ()
{
	const auto& renderContext	= m_context.getRenderContext();
	const auto& gl				= renderContext.getFunctions();

	float minU = 0.0f;
	float maxU = 1.0f;
	float minV = 0.0f;
	float maxV = 1.0f;

	// Apply offset of almost half a texel to the texture coordinates.
	DE_ASSERT(m_offsetSign == 1.0f || m_offsetSign == -1.0f);
	const float offset			= 0.5f - pow(2.0f, -8.0f);
	const float offsetWidth		= offset / static_cast<float>(m_width);
	const float offsetHeight	= offset / static_cast<float>(m_height);

	minU += m_offsetSign * offsetWidth;
	maxU += m_offsetSign * offsetWidth;
	minV += m_offsetSign * offsetHeight;
	maxV += m_offsetSign * offsetHeight;

	const std::vector<float>	positions	= { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
	const std::vector<float>	texCoords	= { minU, minV, minU, maxV, maxU, minV, maxU, maxV };
	const std::vector<deUint16>	quadIndices	= { 0, 1, 2, 2, 1, 3 };

	const std::vector<glu::VertexArrayBinding> vertexArrays =
	{
		glu::va::Float("position", 2, 4, 0, positions.data()),
		glu::va::Float("inTexCoord", 2, 4, 0, texCoords.data())
	};

	glu::ShaderProgram program(m_context.getRenderContext(), glu::makeVtxFragSources(m_vertShaderText, m_fragShaderText));
	if (!program.isOk())
		TCU_FAIL("Shader compilation failed");

	gl.useProgram(program.getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram failed");

	gl.uniform1i(gl.getUniformLocation(program.getProgram(), "texSampler"), 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

	gl.clear(GL_COLOR_BUFFER_BIT);

	glu::draw(renderContext, program.getProgram(),
			  static_cast<int>(vertexArrays.size()), vertexArrays.data(),
			  glu::pr::TriangleStrip(static_cast<int>(quadIndices.size()), quadIndices.data()));
}

bool NearestEdgeTestCase::verifyResults ()
{
	const auto& gl = m_context.getRenderContext().getFunctions();

	std::vector<deUint8> fbData(m_width * m_height * tcu::getPixelSize(m_texFormat));
	if (tcu::getPixelSize(m_texFormat) < 4)
		gl.pixelStorei(GL_PACK_ALIGNMENT, 1);
	gl.readPixels(0, 0, m_width, m_height, m_transFormat.format, m_transFormat.dataType, fbData.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");

	tcu::ConstPixelBufferAccess texAccess	{m_texFormat, m_width, m_height, 1, m_texData.data()};
	tcu::ConstPixelBufferAccess fbAccess	{m_texFormat, m_width, m_height, 1, fbData.data()};

	// Difference image to ease spotting problems.
	const tcu::TextureFormat		diffFormat	{tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8};
	const auto						diffBytes	= tcu::getPixelSize(diffFormat) * m_width * m_height;
	std::unique_ptr<deUint8[]>		diffData	{new deUint8[diffBytes]};
	const tcu::PixelBufferAccess	diffAccess	{diffFormat, m_width, m_height, 1, diffData.get()};

	const tcu::Vec4					colorRed	{1.0f, 0.0f, 0.0f, 1.0f};
	const tcu::Vec4					colorGreen	{0.0f, 1.0f, 0.0f, 1.0f};

	bool pass = true;
	for (int x = 0; x < m_width; ++x)
	for (int y = 0; y < m_height; ++y)
	{
		const auto texPixel	= texAccess.getPixel(x, y);
		const auto fbPixel	= fbAccess.getPixel(x, y);

		// Require perfect pixel match.
		if (texPixel != fbPixel)
		{
			pass = false;
			diffAccess.setPixel(colorRed, x, y);
		}
		else
		{
			diffAccess.setPixel(colorGreen, x, y);
		}
	}

	if (!pass)
	{
		auto& log = m_testCtx.getLog();
		log
			<< tcu::TestLog::Message << "\n"
			<< "Width:       " << m_width << "\n"
			<< "Height:      " << m_height << "\n"
			<< tcu::TestLog::EndMessage;

		log << tcu::TestLog::Image("texture", "Generated Texture", texAccess);
		log << tcu::TestLog::Image("fb", "Frame Buffer Contents", fbAccess);
		log << tcu::TestLog::Image("diff", "Mismatched pixels in red", diffAccess);
	}

	return pass;
}

tcu::TestNode::IterateResult NearestEdgeTestCase::iterate ()
{
	// Populate and configure m_texture.
	createTexture();

	// Fill m_texture with data.
	fillTexture();

	// Draw full screen quad using the texture and a slight offset left or right.
	renderQuad();

	// Verify results.
	bool pass = verifyResults();

	// Destroy texture.
	deleteTexture();

	const qpTestResult	result	= (pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL);
	const char*			desc	= (pass ? "Pass" : "Pixel mismatch; check the generated images");

	m_testCtx.setTestResult(result, desc);
	return STOP;
}

} /* anonymous namespace */

NearestEdgeCases::NearestEdgeCases(deqp::Context& context)
	: TestCaseGroup(context, "nearest_edge", "GL_NEAREST edge cases")
{
}

NearestEdgeCases::~NearestEdgeCases(void)
{
}

void NearestEdgeCases::init(void)
{
	static const std::vector<OffsetDirection> kDirections = { OffsetDirection::LEFT, OffsetDirection::RIGHT };
	for (const auto direction : kDirections)
		addChild(new NearestEdgeTestCase{m_context, direction});
}

} /* glcts namespace */
