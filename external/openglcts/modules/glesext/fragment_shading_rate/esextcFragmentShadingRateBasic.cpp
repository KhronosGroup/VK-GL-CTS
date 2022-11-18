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
 * \file  esextcFragmentShadingRateBasic.hpp
 * \brief FragmentShadingRateEXT basic
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateBasic.hpp"
#include "deRandom.h"
#include "esextcFragmentShadingRateTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

#define DEFAULT_COLOR_FBO_SIZE 255
#define TRIANGLE_COUNT 100

namespace glcts
{

enum
{
	ERROR_NONE				 = 0,
	ERROR_SHADING_RATE_ERROR = 1,
};

///  Constructor
///
/// @param context     Test context
/// @param name        Test case's name
/// @param description Test case's description
FragmentShadingRateBasic::FragmentShadingRateBasic(Context& context, const ExtParameters& extParams, const char* name,
												   const char* description)
	: TestCaseBase(context, extParams, name, description), m_program(nullptr)
{
}

/// Initialize test
void FragmentShadingRateBasic::init(void)
{
	TestCaseBase::init();

	// Skip if required extensions are not supported.
	if (!m_is_fragment_shading_rate_supported)
	{
		throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
	}
}

/// Deinitializes all GLES objects created for the test.
void FragmentShadingRateBasic::deinit(void)
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
std::string FragmentShadingRateBasic::genVS() const
{
	std::ostringstream os;
	os << "#version 310 es                        \n"
	   << "precision highp float;                 \n"
	   << "precision highp int;                   \n"
	   << "layout(location = 0) in vec4 position; \n"
	   << "void main() {                          \n"
	   << "    gl_Position = position;            \n"
	   << "}";
	return os.str();
}

/// Generate Fragment Shader string
std::string FragmentShadingRateBasic::genFS() const
{
	std::ostringstream os;
	os << "#version 310 es\n"
	   << "#extension GL_EXT_fragment_shading_rate : enable\n"
	   << "precision highp float;\n"
	   << "precision highp int;\n"
	   << "layout(location = 0) out ivec4 color0;\n"
	   << "uniform int drawID;\n"
	   << "uniform int shadingRate;\n"
	   << "void main() {\n"
	   << "    color0.x = gl_ShadingRateEXT;\n"
	   << "    color0.y = drawID;\n"
	   << "    color0.z = 0;\n"
	   << "    color0.w = 0;\n"
	   << "    if (gl_ShadingRateEXT != shadingRate) { \n"
	   << "        color0.w = " << ERROR_SHADING_RATE_ERROR << ";\n"
	   << "    }\n"
	   << "}";

	return os.str();
}

/// Initializes all GLES objects and reference values for the test.
void FragmentShadingRateBasic::setupTest(void)
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

	// Allocate unsigned integer storage
	gl.bindTexture(GL_TEXTURE_2D, m_to_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");
	gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, m_tcParam.width, m_tcParam.height);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");

	// Attach it to the framebuffer
	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_to_id, 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error attaching texture to frame buffer");

	constexpr deUint32 kVerticesCount = (TRIANGLE_COUNT * 3 * 2);
	float			   randomVertices[kVerticesCount];

	deRandom rnd;
	deRandom_init(&rnd, m_tcParam.seed);
	for (deUint32 i = 0; i < kVerticesCount; i++)
	{
		randomVertices[i] = deRandom_getFloat(&rnd) * 2.0f - 1.0f;
	}

	gl.genBuffers(1, &m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error generate buffer objects");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding buffer objects");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(randomVertices), randomVertices, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error uploading buffer data");
}

/// Test if the error code returned by glGetError is the same as expected.
/// If the error is different from expected description is logged.
///
/// @param expected_error    GLenum error which is expected
/// @param description       Log message in the case of failure.
///
/// @return true if error is equal to expected, false otherwise.
glw::GLboolean FragmentShadingRateBasic::verifyError(const glw::GLenum expected_error, const char* description) const
{
	// Retrieve GLES entry points.
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	glw::GLboolean test_passed = true;
	glw::GLenum	   error_code  = gl.getError();

	if (error_code != expected_error)
	{
		test_passed = false;

		m_testCtx.getLog() << tcu::TestLog::Message << description << tcu::TestLog::EndMessage;
	}

	return test_passed;
}

/// Executes the test.
///  Sets the test result to QP_TEST_RESULT_FAIL if the test failed, QP_TEST_RESULT_PASS otherwise.
///  Note the function throws exception should an error occur!
///
///  @return STOP if the test has finished, CONTINUE to indicate iterate should be called once again.
///
tcu::TestNode::IterateResult FragmentShadingRateBasic::iterate(void)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Initialization
	m_tcParam.width	 = DEFAULT_COLOR_FBO_SIZE;
	m_tcParam.height = DEFAULT_COLOR_FBO_SIZE;

	setupTest();

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

	gl.shadingRateEXT(GL_SHADING_RATE_1X1_PIXELS_EXT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error to set shadingRateEXT as default");

	gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.clear(GL_COLOR_BUFFER_BIT);

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error use program");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind buffer vertex data");

	gl.enableVertexAttribArray(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error enabling vertex attrib pointer 0");

	gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding vertex attrib pointer 0");

	// draw ID start from 1
	for (deUint32 drawID = 1; drawID < TRIANGLE_COUNT; drawID++)
	{
		gl.uniform1i(gl.getUniformLocation(m_program->getProgram(), "shadingRate"),
					 fsrutils::packShadingRate(translateDrawIDToShadingRate(drawID)));
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set uniform shading Rate value");

		gl.uniform1i(gl.getUniformLocation(m_program->getProgram(), "drawID"), drawID);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set uniform drawID value");

		gl.shadingRateEXT(translateDrawIDToShadingRate(drawID));
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set shading rate");
		gl.drawArrays(GL_TRIANGLES, drawID * 2, 3);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error draw a triangle");
	}

	const deUint32		  dataSize = m_tcParam.width * m_tcParam.height * 4;
	std::vector<deUint32> resultData(dataSize);

	gl.readPixels(0, 0, m_tcParam.width, m_tcParam.height, GL_RGBA_INTEGER, GL_UNSIGNED_INT, resultData.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error reading pixels from frame buffer!");

	for (deUint32 y = 0; y < m_tcParam.height; y++)
	{
		for (deUint32 x = 0; x < m_tcParam.width; x++)
		{
			const deUint32* sample = &resultData[(y * m_tcParam.width + x) * 4];
			if (sample[1] == 0) // nothing rendered
			{
				continue;
			}

			const deUint32 shadingRate = sample[0];
			const deUint32 drawID	   = sample[1];

			if (fsrutils::packShadingRate(translateDrawIDToShadingRate(drawID)) != shadingRate)
			{
				DE_ASSERT(sample[3] == ERROR_SHADING_RATE_ERROR); // sample 3 is error code

				std::stringstream error_sstream;

				error_sstream << "The draw ID is " << drawID << "Shading Rate is" << shadingRate << ", But we expect "
							  << fsrutils::packShadingRate(translateDrawIDToShadingRate(drawID));

				m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, error_sstream.str().c_str());

				return STOP;
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
glw::GLenum FragmentShadingRateBasic::translateDrawIDToShadingRate(deUint32 drawID) const
{
	return m_availableShadingRates[drawID % m_availableShadingRates.size()];
}

} // namespace glcts
