/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2022-2022 The Khronos Group Inc.
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
 */

/*!
 * \file  esextcFragmentShadingRateAttachmentTests.hpp
 * \brief FragmentShadingRateEXT Attachment related tests
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateAttachmentTests.hpp"
#include "deRandom.h"
#include "esextcFragmentShadingRateTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

#define TRIANGLE_COUNT 100

namespace glcts
{

///  Constructor
///
/// @param context     Test context
/// @param extParams   extension params
/// @param testcaseParam Test case params
/// @param name        Test case's name
/// @param description Test case's description
FragmentShadingRateAttachment::FragmentShadingRateAttachment(
	Context& context, const ExtParameters& extParams, const FragmentShadingRateAttachment::TestcaseParam& testcaseParam,
	const char* name, const char* description)
	: TestCaseBase(context, extParams, name, description), m_tcParam(testcaseParam), m_program(nullptr)
{
}

/// Initialize test
void FragmentShadingRateAttachment::init(void)
{
	TestCaseBase::init();

	// Skip if required extensions are not supported.
	if (!m_is_fragment_shading_rate_supported)
	{
		throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
	}

	if (!m_is_fragment_shading_rate_attachment_supported)
	{
		if (m_tcParam.attachmentShadingRate)
		{
			throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
		}

		if (m_tcParam.multiShadingRate)
		{
			throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
		}
	}

	if (!m_is_multiview_ovr_supported)
	{
		throw tcu::NotSupportedError(MULTIVIEW_OVR_NOT_SUPPORTED, "", __FILE__, __LINE__);
	}
}

/// Deinitializes all GLES objects created for the test.
void FragmentShadingRateAttachment::deinit(void)
{
	// Retrieve GLES entry points.
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Reset GLES state
	gl.bindTexture(GL_TEXTURE_2D, 0);
	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	gl.bindFramebuffer(GL_FRAMEBUFFER, 0);

	gl.deleteTextures(1, &m_to_id);
	gl.deleteFramebuffers(1, &m_fbo_id);
	gl.deleteBuffers(1, &m_vbo_id);

	delete m_program;

	// Deinitialize base class
	TestCaseBase::deinit();
}

/// Generate Vertex Shader string
std::string FragmentShadingRateAttachment::genVS() const
{
	std::ostringstream os;
	os << "#version 310 es\n";

	if (m_tcParam.testKind == TestKind::MultiView)
	{
		os << "#extension GL_OVR_multiview: enable\n"
			  "layout(num_views = 2) in;\n";
	}

	os << "precision highp float;\n"
	   << "precision highp int;\n"
	   << "uniform int drawID;\n"
	   << "layout(location = 0) in vec4 position;\n"
	   << "void main() {\n"
	   << "    gl_Position = position;\n";
	if (m_tcParam.testKind == TestKind::MultiView)
	{
		os << "if (gl_ViewID_OVR == 1u) {\n"
		   << "gl_Position.x  += 0.1;\n"
		   << "}\n";
	}
	os << "}";
	return os.str();
}

/// Generate Fragment Shader string
std::string FragmentShadingRateAttachment::genFS() const
{
	std::ostringstream os;
	os << "#version 310 es\n"
	   << "#extension GL_EXT_fragment_shading_rate : enable\n"
	   << "precision highp float;\n"
	   << "precision highp int;\n"
	   << "layout(location = 0) out ivec4 color0;\n"
	   << "uniform int drawID;\n"
	   << "void main() {\n"
	   << "    color0.x = gl_ShadingRateEXT;\n"
	   << "    color0.y = drawID;\n"
	   << "    color0.z = 0;\n"
	   << "    color0.w = 0;\n"
	   << "}";

	return os.str();
}

/// Initializes all GLES objects and reference values for the test.
void FragmentShadingRateAttachment::setupTest(void)
{
	m_program =
		new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(genVS().c_str(), genFS().c_str()));

	if (!m_program->isOk())
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "" << tcu::TestLog::EndMessage
						   << tcu::TestLog::ShaderProgram(false, "")
						   << tcu::TestLog::Shader(QP_SHADER_TYPE_VERTEX,
												   m_program->getShaderInfo(glu::SHADERTYPE_VERTEX, 0).source, false,
												   m_program->getShaderInfo(glu::SHADERTYPE_VERTEX, 0).infoLog)

						   << tcu::TestLog::Shader(QP_SHADER_TYPE_FRAGMENT,
												   m_program->getShaderInfo(glu::SHADERTYPE_FRAGMENT, 0).source, false,
												   m_program->getShaderInfo(glu::SHADERTYPE_FRAGMENT, 0).infoLog)
						   << tcu::TestLog::EndShaderProgram;
		TCU_FAIL("Shader creation failed");
	}

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Generate framebuffer objects
	gl.genFramebuffers(1, &m_fbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error setting up framebuffer objects");

	gl.bindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding frame buffer object!");

	// Generate a new texture name
	gl.genTextures(1, &m_to_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error generating texture objects");

	if (m_tcParam.layerCount > 1)
	{
		// Allocate unsigned integer storage
		gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");

		gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA32UI, m_tcParam.framebufferSize, m_tcParam.framebufferSize,
						m_tcParam.layerCount);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");
	}
	else
	{
		// Allocate unsigned integer storage
		gl.bindTexture(GL_TEXTURE_2D, m_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");

		gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, m_tcParam.framebufferSize, m_tcParam.framebufferSize);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");
	}

	// Attach it to the framebuffer
	if (m_tcParam.testKind == TestKind::MultiView)
	{
		gl.framebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_to_id, 0, 0,
										  m_tcParam.layerCount);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error attaching texture to frame buffer");
	}
	else
	{
		gl.framebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_to_id, 0);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error attaching texture to frame buffer");
	}

	// Generate a shading rate texture name
	if (m_tcParam.attachmentShadingRate)
	{
		// generate shading rate texture
		gl.getIntegerv(GL_MAX_FRAGMENT_SHADING_RATE_ATTACHMENT_TEXEL_WIDTH_EXT, &m_srTexelWidth);
		GLU_EXPECT_NO_ERROR(gl.getError(),
							"Error getIntegerv GL_MAX_FRAGMENT_SHADING_RATE_ATTACHMENT_TEXEL_WIDTH_EXT!");
		gl.getIntegerv(GL_MAX_FRAGMENT_SHADING_RATE_ATTACHMENT_TEXEL_HEIGHT_EXT, &m_srTexelHeight);
		GLU_EXPECT_NO_ERROR(gl.getError(),
							"Error getIntegerv GL_MAX_FRAGMENT_SHADING_RATE_ATTACHMENT_TEXEL_HEIGHT_EXT!");

		const deUint32 srWidth	= (m_tcParam.framebufferSize + m_srTexelWidth - 1) / m_srTexelWidth;
		const deUint32 srHeight = (m_tcParam.framebufferSize + m_srTexelHeight - 1) / m_srTexelHeight;

		std::vector<deUint8> attachmentShadingRateData;
		const deUint32		 srLayerCount = m_tcParam.multiShadingRate ? 2 : 1;
		attachmentShadingRateData.reserve(srWidth * srHeight * srLayerCount);
		for (deUint32 srLayer = 0; srLayer < srLayerCount; srLayer++)
		{
			for (deUint32 y = 0; y < srHeight; y++)
			{
				for (deUint32 x = 0; x < srWidth; x++)
				{
					deUint8 packedShadingRate = static_cast<unsigned char>(
						fsrutils::packShadingRate(translateCoordsToShadingRate(srLayer, x, y)));
					attachmentShadingRateData.push_back(packedShadingRate);
				}
			}
		}

		gl.genTextures(1, &m_sr_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error generating texture objects");
		gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set pixelStorei for unpack alignment");

		if (m_tcParam.multiShadingRate)
		{
			DE_ASSERT(m_tcParam.layerCount > 1);
			// Allocate unsigned integer storage
			gl.bindTexture(GL_TEXTURE_2D_ARRAY, m_sr_to_id);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");

			gl.texStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8UI, srWidth, srHeight, m_tcParam.layerCount);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");

			gl.texSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, srWidth, srHeight, m_tcParam.layerCount, GL_RED_INTEGER,
							 GL_UNSIGNED_BYTE, attachmentShadingRateData.data());
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error updating shading rate data to texture");

			// Attach it to the framebuffer
			gl.framebufferShadingRateEXT(GL_FRAMEBUFFER, GL_SHADING_RATE_ATTACHMENT_EXT, m_sr_to_id, 0,
										 m_tcParam.layerCount, m_srTexelWidth, m_srTexelHeight);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error framebufferShadingRate");
		}
		else
		{
			// Allocate unsigned integer storage
			gl.bindTexture(GL_TEXTURE_2D, m_sr_to_id);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");

			gl.texStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, srWidth, srHeight);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");

			gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srWidth, srHeight, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
							 attachmentShadingRateData.data());
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error updating shading rate data to texture");

			// Attach it to the framebuffer
			gl.framebufferShadingRateEXT(GL_FRAMEBUFFER, GL_SHADING_RATE_ATTACHMENT_EXT, m_sr_to_id, 0, 1,
										 m_srTexelWidth, m_srTexelHeight);
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error framebufferShadingRate");
		}
	}

	constexpr deUint32 kVerticesCount = (TRIANGLE_COUNT * 3 * 2);
	std::vector<float> randomVertices;
	randomVertices.reserve(kVerticesCount);

	// 1st draw triangle is huge to fill all rect.
	randomVertices.push_back(-3.0);
	randomVertices.push_back(-3.0);
	randomVertices.push_back(-3.0);
	randomVertices.push_back(3.0);
	randomVertices.push_back(3.0);
	randomVertices.push_back(-3.0);

	deRandom rnd;
	deRandom_init(&rnd, 0);
	for (deUint32 i = 0; i < kVerticesCount; i++)
	{
		randomVertices.push_back((deRandom_getFloat(&rnd) * 2.0f - 1.0f));
	}

	gl.genBuffers(1, &m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error generate buffer objects");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding buffer objects");

	gl.bufferData(GL_ARRAY_BUFFER, randomVertices.size() * sizeof(float), randomVertices.data(), GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error uploading buffer data");

	if (m_tcParam.testKind == TestKind::Scissor)
	{
		m_scissorBox = { m_tcParam.framebufferSize / 3, m_tcParam.framebufferSize / 3, m_tcParam.framebufferSize / 3,
						 m_tcParam.framebufferSize / 3 };
	}
}

/// Executes the test.
///  Sets the test result to QP_TEST_RESULT_FAIL if the test failed, QP_TEST_RESULT_PASS otherwise.
///  Note the function throws exception should an error occur!
///
///  @return STOP if the test has finished, CONTINUE to indicate iterate should be called once again.
///
tcu::TestNode::IterateResult FragmentShadingRateAttachment::iterate(void)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Initialization
	constexpr deUint32 kMaxRateCount =
		16; // SHADING_RATE_1X1_PIXELS_EXT ~ SHADING_RATE_4X4_PIXELS_EXT, actually 9 is enough
	glw::GLenum	 shadingRates[kMaxRateCount];
	glw::GLsizei count = 0;

	gl.getFragmentShadingRatesEXT(1, kMaxRateCount, &count, shadingRates);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error to get shading rate getFragmentShadingRatesEXT");
	DE_ASSERT(count > 0);

	for (glw::GLsizei i = 0; i < count; i++)
	{
		m_availableShadingRates.push_back(shadingRates[i]);
	}

	setupTest();

	gl.disable(GL_DEPTH_TEST);

	gl.shadingRateEXT(GL_SHADING_RATE_1X1_PIXELS_EXT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error to set shadingRateEXT as default");

	gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error clear Color");

	gl.clear(GL_COLOR_BUFFER_BIT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error clear");

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error use program");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind buffer vertex data");

	gl.enableVertexAttribArray(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error enabling vertex attrib pointer 0");

	gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding vertex attrib pointer 0");

	if (m_tcParam.attachmentShadingRate)
	{
		gl.shadingRateCombinerOpsEXT(GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT,
									 GL_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_EXT);
	}
	else
	{
		gl.shadingRateCombinerOpsEXT(GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT,
									 GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT);
	}

	if (m_tcParam.testKind == TestKind::Scissor)
	{
		gl.scissor(m_scissorBox.x, m_scissorBox.y, m_scissorBox.width, m_scissorBox.height);
		gl.enable(GL_SCISSOR_TEST);
	}

	// draw ID start from 1
	for (deUint32 drawID = 1; drawID < TRIANGLE_COUNT; drawID++)
	{
		gl.uniform1i(gl.getUniformLocation(m_program->getProgram(), "drawID"), drawID);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set uniform drawID value");

		if (!m_tcParam.attachmentShadingRate)
		{
			gl.shadingRateEXT(translateDrawIDToShadingRate(drawID));
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error set shading rate");
		}

		const deUint32 startVertex = (drawID - 1) * 2; // to use first vertices "-1" because drawID start from 1
		gl.drawArrays(GL_TRIANGLES, startVertex, 3);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error draw a triangle");
	}

	for (uint32_t layer = 0; layer < m_tcParam.layerCount; layer++)
	{
		if (m_tcParam.layerCount > 1)
		{
			gl.framebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_to_id, 0, layer);
		}
		const deUint32		  dataSize = m_tcParam.framebufferSize * m_tcParam.framebufferSize * 4;
		std::vector<deUint32> resultData(dataSize);
		gl.readPixels(0, 0, m_tcParam.framebufferSize, m_tcParam.framebufferSize, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
					  resultData.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error reading pixels from frame buffer!");

		for (deUint32 y = 0; y < m_tcParam.framebufferSize; y++)
		{
			for (deUint32 x = 0; x < m_tcParam.framebufferSize; x++)
			{
				const deUint32* sample = &resultData[((y * m_tcParam.framebufferSize) + x) * 4];

				if (m_tcParam.testKind == TestKind::Scissor)
				{
					if (!m_scissorBox.in(x, y)) // out of scissor box
					{
						if (sample[1] != 0) // should be cleared to 0
						{
							std::stringstream error_sstream;
							error_sstream << "out of scissor box should be 0"
										  << "scissor: " << m_scissorBox.x << " " << m_scissorBox.width << " "
										  << m_scissorBox.y << " " << m_scissorBox.height;
						}
						else
						{
							// success. outside scissor is always 0
							continue;
						}
					}
					else
					{
						if (sample[1] == 0) // if it is cleared to 0, error. All cleared by 1st big triangle
						{
							std::stringstream error_sstream;
							error_sstream << "inside of scissor box should not be 0"
										  << "scissor: " << m_scissorBox.x << " " << m_scissorBox.width << " "
										  << m_scissorBox.y << " " << m_scissorBox.height;
						}
					}
				}
				else if (sample[1] == 0) // nothing rendered for the other case except scissor test
				{
					continue;
				}

				const deUint32 shadingRate = sample[0];
				const deUint32 drawID	   = sample[1];

				deUint32 expectedShadingRate = 0;
				if (m_tcParam.attachmentShadingRate)
				{
					deUint32 srLayer = m_tcParam.multiShadingRate ? layer : 0;
					expectedShadingRate = fsrutils::packShadingRate(
						translateCoordsToShadingRate(srLayer, x / m_srTexelWidth, y / m_srTexelHeight));
				}
				else
				{
					expectedShadingRate = fsrutils::packShadingRate(translateDrawIDToShadingRate(drawID));
				}

				if (expectedShadingRate != shadingRate)
				{
					std::stringstream error_sstream;

					error_sstream << "The draw ID is " << drawID << "Shading Rate is" << shadingRate
								  << ", But we expect " << expectedShadingRate;

					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, error_sstream.str().c_str());

					return STOP;
				}
			}
		}
	}

	// All done
	if (m_testCtx.getTestResult() != QP_TEST_RESULT_FAIL)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}

	return STOP;
}

/// Translate draw ID to ShadingRate enumeration
///
/// @param drawID draw ID to translate shading rate
///
/// @return shading rate enumeration
glw::GLenum FragmentShadingRateAttachment::translateDrawIDToShadingRate(deUint32 drawID) const
{
	return m_availableShadingRates[drawID % m_availableShadingRates.size()];
}

/// translate draw ID to View ID
///
/// @param drawID ID to translate to View ID
///
/// @return translated ViewID
deUint32 FragmentShadingRateAttachment::drawIDToViewID(deUint32 drawID) const
{
	return drawID & 1;
}

/// Translate Coordinates ID to ShadingRate enumeration
///
///@param srLayer shading rate layer index
///@param srx x coord in the shading rate attachment to translate shading rate
///@param sry y coord in the shading rate attachment to translate shading rate
///
///@return shading rate enumeration
glw::GLenum FragmentShadingRateAttachment::translateCoordsToShadingRate(deUint32 srLayer, deUint32 srx,
																		deUint32 sry) const
{
	DE_ASSERT(m_tcParam.multiShadingRate || srLayer == 0);
	return m_availableShadingRates[(srLayer + srx + sry) % m_availableShadingRates.size()];
}

} // namespace glcts
