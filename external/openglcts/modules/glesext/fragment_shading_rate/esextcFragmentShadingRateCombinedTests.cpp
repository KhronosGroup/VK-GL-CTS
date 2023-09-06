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
 * \file  esextcesextcFragmentShadingRateCombinedTests.hpp
 * \brief FragmentShadingRateEXT combined tests
 */ /*-------------------------------------------------------------------*/

#include "esextcFragmentShadingRateCombinedTests.hpp"
#include "deRandom.h"
#include "esextcFragmentShadingRateTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

#define TRIANGLE_COUNT 100
constexpr deUint32 kShadingRateCount = 16;

namespace glcts
{

/// check combiner is trivial operation or not
///
/// @param combineOp combiner which want to check
///
/// @return true for trivial combiner
bool isTrivialCombiner(glw::GLenum combineOp)
{
	return ((combineOp == GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT) ||
			(combineOp == GL_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_EXT)) ?
			   true :
			   false;
}

/// Constructor
///
/// @param context     Test context
/// @param extParams   Extension Parameter
/// @param testcaseParam Test case Parameter
/// @param name        Test case's name
/// @param description Test case's description
FragmentShadingRateCombined::FragmentShadingRateCombined(
	Context& context, const ExtParameters& extParams, const FragmentShadingRateCombined::TestcaseParam& testcaseParam,
	const char* name, const char* description)
	: TestCaseBase(context, extParams, name, description)
	, m_tcParam(testcaseParam)
	, m_renderProgram(nullptr)
	, m_computeProgram(nullptr)
	, m_to_id(0)
	, m_sr_to_id(0)
	, m_fbo_id(0)
	, m_vbo_id(0)
	, m_simulationCache(kShadingRateCount * kShadingRateCount * kShadingRateCount, 0xFFFFFFFF)
{
}

/// Initialize test
void FragmentShadingRateCombined::init(void)
{
	TestCaseBase::init();

	// Skip if required extensions are not supported.
	if (!m_is_fragment_shading_rate_supported)
	{
		throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
	}

	if (!m_is_fragment_shading_rate_primitive_supported)
	{
		if (m_tcParam.useShadingRatePrimitive)
		{
			throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
		}

		if (m_tcParam.combinerOp0 != GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT)
		{
			throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
		}
	}

	if (!m_is_fragment_shading_rate_attachment_supported)
	{
		if (m_tcParam.useShadingRateAttachment)
		{
			throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
		}

		if (m_tcParam.combinerOp1 != GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT)
		{
			throw tcu::NotSupportedError(FRAGMENT_SHADING_RATE_NOT_SUPPORTED, "", __FILE__, __LINE__);
		}
	}

	if (!isTrivialCombiner(m_tcParam.combinerOp0) || !isTrivialCombiner(m_tcParam.combinerOp1))
	{
		const glw::Functions& gl						= m_context.getRenderContext().getFunctions();
		glw::GLboolean		  supportNonTrivialCombiner = false;
		gl.getBooleanv(GL_FRAGMENT_SHADING_RATE_NON_TRIVIAL_COMBINERS_SUPPORTED_EXT, &supportNonTrivialCombiner);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error getBooleanv non trivial combiner");

		if (!supportNonTrivialCombiner)
		{
			throw tcu::NotSupportedError("Non trivial combiner is not supported", "", __FILE__, __LINE__);
		}
	}
}

/// Deinitializes all GLES objects created for the test.
void FragmentShadingRateCombined::deinit(void)
{
	// Retrieve GLES entry points.
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Reset GLES state
	gl.bindTexture(GL_TEXTURE_2D, 0);
	gl.bindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
	gl.bindBuffer(GL_ARRAY_BUFFER, 0);
	gl.bindFramebuffer(GL_FRAMEBUFFER, 0);

	gl.deleteTextures(1, &m_to_id);
	gl.deleteFramebuffers(1, &m_fbo_id);
	gl.deleteBuffers(1, &m_vbo_id);

	if (m_tcParam.useShadingRateAttachment)
	{
		gl.deleteTextures(1, &m_sr_to_id);
	}

	delete m_renderProgram;
	delete m_computeProgram;

	// Deinitialize base class
	TestCaseBase::deinit();
}

/// Generate Vertex Shader string
std::string FragmentShadingRateCombined::genVS()
{
	std::ostringstream os;
	os << "#version 310 es                        \n"
	   << "#extension GL_EXT_fragment_shading_rate : enable\n"
	   << "precision highp float;                 \n"
	   << "precision highp int;                   \n"
	   << "layout(location = 0) in vec4 position; \n"
	   << "uniform int primShadingRate;           \n"
	   << "void main() {                          \n"
	   << "    gl_Position = position;            \n";

	if (m_tcParam.useShadingRatePrimitive)
	{
		os << "    gl_PrimitiveShadingRateEXT = primShadingRate;\n";
	}
	os << "}";
	return os.str();
}

/// Generate Fragment Shader string
std::string FragmentShadingRateCombined::genFS()
{
	std::ostringstream os;
	os << "#version 310 es\n"
	   << "#extension GL_EXT_fragment_shading_rate : enable\n"
	   << "precision highp float;\n"
	   << "precision highp int;\n"
	   << "layout(location = 0) out vec4 color0;\n"
	   << "uniform int primID;\n"
	   << "uniform int drawID;\n"
	   << "void main() {\n"
	   << "    color0.x = float(gl_ShadingRateEXT);\n"
	   << "    color0.y = float(drawID);\n";

	if (m_tcParam.useShadingRatePrimitive)
	{
		os << "    color0.z = float(primID);\n";
	};

	os << "    color0.w = 0.0;\n"
	   << "}";

	return os.str();
}

/// Generate Compute Shader string for copy
std::string FragmentShadingRateCombined::genCS()
{
	deUint32		   samples = m_tcParam.msaa ? 4 : 1;
	std::ostringstream os;
	os << "#version 310 es\n"
	   << "precision highp float;\n"
	   << "precision highp int;\n"
	   << (m_tcParam.msaa ? "uniform highp sampler2DMS colorTex;\n" : "uniform highp sampler2D colorTex;\n")
	   << "layout (binding = 0, std430) buffer ColorBuf {\n"
	   << "    uvec4 values[];\n"
	   << "} colorbuf;\n"
	   << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
	   << "void main()\n"
	   << "{\n"
	   << "    for (uint i = 0u; i < " << samples << "u; ++i) \n"
	   << "    {\n"
	   << "        uint index = ((gl_GlobalInvocationID.y * " << m_tcParam.framebufferSize
	   << "u) + gl_GlobalInvocationID.x) * " << samples << "u + i;\n"
	   << "        colorbuf.values[index] = uvec4(round(texelFetch(colorTex, ivec2(gl_GlobalInvocationID.xy), "
	   << "int(i))));\n"
	   << "    }\n"
	   << "}";
	return os.str();
}

/// Initializes all GLES objects and reference values for the test.
void FragmentShadingRateCombined::setupTest(void)
{
	m_renderProgram =
		new glu::ShaderProgram(m_context.getRenderContext(), glu::makeVtxFragSources(genVS().c_str(), genFS().c_str()));

	if (!m_renderProgram->isOk())
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "" << tcu::TestLog::EndMessage
						   << tcu::TestLog::ShaderProgram(false, "")
						   << tcu::TestLog::Shader(QP_SHADER_TYPE_VERTEX,
												   m_renderProgram->getShaderInfo(glu::SHADERTYPE_VERTEX, 0).source,
												   false,
												   m_renderProgram->getShaderInfo(glu::SHADERTYPE_VERTEX, 0).infoLog)

						   << tcu::TestLog::Shader(QP_SHADER_TYPE_FRAGMENT,
												   m_renderProgram->getShaderInfo(glu::SHADERTYPE_FRAGMENT, 0).source,
												   false,
												   m_renderProgram->getShaderInfo(glu::SHADERTYPE_FRAGMENT, 0).infoLog)
						   << tcu::TestLog::EndShaderProgram;
		TCU_FAIL("Shader creation failed");
	}

	glu::ProgramSources sourcesCompute;
	sourcesCompute.sources[glu::SHADERTYPE_COMPUTE].push_back(genCS());
	m_computeProgram = new glu::ShaderProgram(m_context.getRenderContext(), sourcesCompute);
	if (!m_computeProgram->isOk())
	{
		m_testCtx.getLog() << tcu::TestLog::Message << "" << tcu::TestLog::EndMessage
						   << tcu::TestLog::ShaderProgram(false, "")
						   << tcu::TestLog::Shader(QP_SHADER_TYPE_COMPUTE,
												   m_computeProgram->getShaderInfo(glu::SHADERTYPE_COMPUTE, 0).source,
												   false,
												   m_computeProgram->getShaderInfo(glu::SHADERTYPE_COMPUTE, 0).infoLog);
		TCU_FAIL("Shader creation failed");
	}

	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Generate framebuffer objects
	gl.genFramebuffers(1, &m_fbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error setting up framebuffer objects");

	gl.bindFramebuffer(GL_FRAMEBUFFER, m_fbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding frame buffer object!");

	// Generate texture objects
	gl.genTextures(1, &m_to_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error generating texture objects");

	gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error set pixelStorei for unpack alignment");

	// Allocate unsigned integer storage
	glw::GLenum textureTarget = GL_TEXTURE_2D;
	if (m_tcParam.msaa)
	{
		textureTarget = GL_TEXTURE_2D_MULTISAMPLE;
		gl.bindTexture(textureTarget, m_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");
		gl.texStorage2DMultisample(textureTarget, 4, GL_RGBA32F, m_tcParam.framebufferSize, m_tcParam.framebufferSize,
								   true);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");
	}
	else
	{
		gl.bindTexture(textureTarget, m_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");
		gl.texStorage2D(textureTarget, 1, GL_RGBA32F, m_tcParam.framebufferSize, m_tcParam.framebufferSize);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");
	}

	// Attach it to the framebuffer
	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, m_to_id, 0); /* level */
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error attaching texture to frame buffer");

	if (m_tcParam.useShadingRateAttachment)
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

		gl.genTextures(1, &m_sr_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error generating texture objects");

		// Allocate unsigned integer storage
		gl.bindTexture(GL_TEXTURE_2D, m_sr_to_id);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding texture object!");

		gl.texStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, srWidth, srHeight);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocating texture object!");

		std::vector<deUint8> attachmentShadingRateData;
		attachmentShadingRateData.reserve(srWidth * srHeight);
		for (deUint32 sry = 0; sry < srHeight; sry++)
		{
			for (deUint32 srx = 0; srx < srWidth; srx++)
			{
				deUint8 packedShadingRate =
					static_cast<unsigned char>(fsrutils::packShadingRate(translateCoordsToShadingRate(srx, sry)));
				attachmentShadingRateData.push_back(packedShadingRate);
			}
		}

		gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srWidth, srHeight, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
						 attachmentShadingRateData.data());
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error updating shading rate data to texture");

		// Attach it to the framebuffer
		gl.framebufferShadingRateEXT(GL_FRAMEBUFFER, GL_SHADING_RATE_ATTACHMENT_EXT, m_sr_to_id, 0, 1, m_srTexelWidth,
									 m_srTexelHeight);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error attaching shading rate attachment to frame buffer");
	}

	constexpr deUint32 kVerticesCount = (TRIANGLE_COUNT * 3 * 2);
	float			   randomVertices[kVerticesCount];

	deRandom rnd;
	deRandom_init(&rnd, 0);
	for (deUint32 i = 0; i < kVerticesCount; i++)
	{
		randomVertices[i] = deRandom_getFloat(&rnd) * 2.0f - 1.0f;
	}

	gl.genBuffers(1, &m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error setting up buffer objects");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding buffer objects");

	gl.bufferData(GL_ARRAY_BUFFER, sizeof(randomVertices), randomVertices, GL_STATIC_DRAW);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error uploading buffer data");
}

/// Executes the test.
/// Sets the test result to QP_TEST_RESULT_FAIL if the test failed, QP_TEST_RESULT_PASS otherwise.
/// Note the function throws exception should an error occur!
///
/// @return STOP if the test has finished, CONTINUE to indicate iterate should be called once again.
tcu::TestNode::IterateResult FragmentShadingRateCombined::iterate(void)
{
	const glw::Functions& gl = m_context.getRenderContext().getFunctions();

	// Initialization
	const deUint32	   sampleCount = m_tcParam.msaa ? 4 : 1;
	constexpr deUint32 kMaxRateCount =
		16; // SHADING_RATE_1X1_PIXELS_EXT ~ SHADING_RATE_4X4_PIXELS_EXT, actually 9 is enough
	glw::GLenum	 shadingRates[kMaxRateCount];
	glw::GLsizei count = 0;

	gl.getFragmentShadingRatesEXT(sampleCount, kMaxRateCount, &count, shadingRates);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error to get shading rate getFragmentShadingRatesEXT");
	DE_ASSERT(count > 0);

	for (glw::GLsizei i = 0; i < count; i++)
	{
		m_availableShadingRates.push_back(shadingRates[i]);
	}

	setupTest();

	gl.shadingRateEXT(GL_SHADING_RATE_1X1_PIXELS_EXT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error to set shadingRateEXT as default");

	gl.clearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.clear(GL_COLOR_BUFFER_BIT);

	gl.useProgram(m_renderProgram->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error use program");

	gl.bindBuffer(GL_ARRAY_BUFFER, m_vbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind buffer vertex data");

	gl.enableVertexAttribArray(0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error enabling vertex attrib pointer 0");

	gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding vertex attrib pointer 0");

	// primitive ID start from 1
	for (deUint32 drawID = 1; drawID < TRIANGLE_COUNT; drawID++)
	{
		const deUint32 primID			 = getPrimitiveID(drawID);
		const deUint32 packedShadingRate = fsrutils::packShadingRate(translatePrimIDToShadingRate(primID));
		gl.uniform1i(gl.getUniformLocation(m_renderProgram->getProgram(), "primShadingRate"), packedShadingRate);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set uniform shadingRate value");

		gl.uniform1i(gl.getUniformLocation(m_renderProgram->getProgram(), "primID"), primID);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set uniform primID value");

		gl.uniform1i(gl.getUniformLocation(m_renderProgram->getProgram(), "drawID"), drawID);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set uniform drawID value");

		if (m_tcParam.useShadingRateAPI)
		{
			gl.shadingRateEXT(translateDrawIDToShadingRate(drawID));
			GLU_EXPECT_NO_ERROR(gl.getError(), "Error set shading rate");
		}

		gl.shadingRateCombinerOpsEXT(m_tcParam.combinerOp0, m_tcParam.combinerOp1);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error set Shading Rate combiner operations");

		gl.drawArrays(GL_TRIANGLES, drawID * 2, 3);
		GLU_EXPECT_NO_ERROR(gl.getError(), "Error draw a triangle");
	}

	constexpr deUint32 kChannels = 4;
	const deUint32	   dataSize =
		m_tcParam.framebufferSize * m_tcParam.framebufferSize * sampleCount * sizeof(deUint32) * kChannels;

	// Copy the result color buffer to shader storage buffer to access for the msaa case
	glw::GLuint ssbo_id;
	gl.genBuffers(1, &ssbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error generate buffer object");

	gl.bindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind buffer object");

	gl.bufferData(GL_SHADER_STORAGE_BUFFER, dataSize, nullptr, GL_DYNAMIC_COPY);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error allocate buffer object");

	gl.useProgram(m_computeProgram->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error use compute object");

	gl.bindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind buffer object to program");

	gl.uniform1i(gl.getUniformLocation(m_renderProgram->getProgram(), "colorTex"), 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind set colorTex uniform value");

	glw::GLenum textureTarget = GL_TEXTURE_2D;
	if (m_tcParam.msaa)
	{
		textureTarget = GL_TEXTURE_2D_MULTISAMPLE;
	}

	gl.bindTexture(textureTarget, m_to_id);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error bind texture");

	gl.dispatchCompute(m_tcParam.framebufferSize, m_tcParam.framebufferSize, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error dispatching copy compute program");

	gl.flush();
	GLU_EXPECT_NO_ERROR(gl.getError(), "Error for flushing");

	const deUint32* resPtr =
		static_cast<const deUint32*>(gl.mapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, dataSize, GL_MAP_READ_BIT));
	for (deInt32 y = 0; y < m_tcParam.framebufferSize; y++)
	{
		for (deInt32 x = 0; x < m_tcParam.framebufferSize; x++)
		{
			for (deUint32 s = 0; s < sampleCount; s++)
			{
				const deUint32	index  = ((y * m_tcParam.framebufferSize + x) * sampleCount + s) * kChannels;
				const deUint32* sample = &resPtr[index];
				if (sample[1] == 0) // nothing rendered
				{
					continue;
				}

				const deUint32 shadingRate			   = sample[0];
				const deUint32 drawID				   = sample[1];
				const deUint32 primID				   = sample[2];
				const deUint32 expectedShadingRateMask = simulate(drawID, primID, x, y);
				if (!(expectedShadingRateMask & (1 << shadingRate)))
				{
					std::stringstream error_sstream;

					error_sstream << "The draw ID is " << drawID << "The primitive ID is " << primID
								  << "Shading Rate is" << shadingRate << ", But we expect one mask of "
								  << expectedShadingRateMask;

					m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, error_sstream.str().c_str());

					gl.deleteBuffers(1, &ssbo_id);
					return STOP;
				}
			}
		}
	}

	gl.deleteBuffers(1, &ssbo_id);

	/* All done */
	if (m_testCtx.getTestResult() != QP_TEST_RESULT_FAIL)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	}

	return STOP;
}

/// Translate Primitive ID to ShadingRate enumeration
///
/// @param primID primitive ID to translate shading rate
///
/// @return shading rate enumeration
glw::GLenum FragmentShadingRateCombined::translateDrawIDToShadingRate(deUint32 drawID) const
{
	return m_availableShadingRates[drawID % m_availableShadingRates.size()];
}

/// Translate Primitive ID to ShadingRate enumeration
///
/// @param primID primitive ID to translate shading rate
///
/// @return shading rate enumeration
glw::GLenum FragmentShadingRateCombined::translatePrimIDToShadingRate(deUint32 primID) const
{
	return m_availableShadingRates[(primID * 7) % m_availableShadingRates.size()];
}

/// Translate Coordinates ID to ShadingRate enumeration
///
///@param srx x coord in the shading rate attachment to translate shading rate
///@param sry y coord in the shading rate attachment to translate shading rate
///
///@return shading rate enumeration
glw::GLenum FragmentShadingRateCombined::translateCoordsToShadingRate(deUint32 srx, deUint32 sry) const
{
	return m_availableShadingRates[(srx + sry) % m_availableShadingRates.size()];
}

/// getPrimitiveID from drawID
///
/// @param drawID draw ID to translate
///
/// @return primitive ID
deUint32 FragmentShadingRateCombined::getPrimitiveID(deUint32 drawID) const
{
	return drawID + 1;
}

/// Map an extent to a mask of all modes smaller than or equal to it in either dimension
///
/// @param ext extent to get available shading rate mask
/// @param allowSwap swap allowable between width and height
///
/// @return shifted mask which is shifted from all candidate rates
deUint32 FragmentShadingRateCombined::shadingRateExtentToClampedMask(Extent2D ext, bool allowSwap) const
{
	deUint32 desiredSize = ext.width * ext.height;

	deUint32 mask = 0;

	while (desiredSize > 0)
	{
		// First, find modes that maximize the area
		for (deUint32 i = 0; i < m_availableShadingRates.size(); ++i)
		{
			deUint32 packedShadingRate = fsrutils::packShadingRate(m_availableShadingRates[i]);
			Extent2D fragmentSize	   = packedShadingRateToExtent(packedShadingRate);

			if (fragmentSize.width * fragmentSize.height == desiredSize &&
				((fragmentSize.width <= ext.width && fragmentSize.height <= ext.height) ||
				 (fragmentSize.height <= ext.width && fragmentSize.width <= ext.height && allowSwap)))
			{
				deUint32 candidate = (deCtz32(fragmentSize.width) << 2) | deCtz32(fragmentSize.height);
				mask |= 1 << candidate;
			}
		}
		if (mask)
		{
			// Amongst the modes that maximize the area, pick the ones that
			// minimize the aspect ratio. Prefer ratio of 1, then 2, then 4.
			// 1x1 = 0, 2x2 = 5, 4x4 = 10
			static const deUint32 aspectMaskRatio1 = 0x421;
			// 2x1 = 4, 1x2 = 1, 4x2 = 9, 2x4 = 6
			static const deUint32 aspectMaskRatio2 = 0x252;
			// 4x1 = 8, 1x4 = 2,
			static const deUint32 aspectMaskRatio4 = 0x104;

			if (mask & aspectMaskRatio1)
			{
				mask &= aspectMaskRatio1;
				break;
			}
			if (mask & aspectMaskRatio2)
			{
				mask &= aspectMaskRatio2;
				break;
			}
			if (mask & aspectMaskRatio4)
			{
				mask &= aspectMaskRatio4;
				break;
			}
			DE_ASSERT(0);
		}
		desiredSize /= 2;
	}

	return mask;
}

/// Software simulate to compare with GPU result
///
/// @param drawID draw ID
/// @param primID primitive ID
/// @param x fragment coordinates X
/// @param y fragment coordinates Y
///
/// @return shifted mask which is shifted from all candidate rates
deUint32 FragmentShadingRateCombined::simulate(deUint32 drawID, deUint32 primID, deUint32 x, deUint32 y)
{

	const deUint32 rate0 =
		m_tcParam.useShadingRateAPI ? fsrutils::packShadingRate(translateDrawIDToShadingRate(drawID)) : 0;
	const deUint32 rate1 =
		m_tcParam.useShadingRatePrimitive ? fsrutils::packShadingRate(translatePrimIDToShadingRate(primID)) : 0;
	const deUint32 rate2 =
		m_tcParam.useShadingRateAttachment ?
			fsrutils::packShadingRate(translateCoordsToShadingRate(x / m_srTexelWidth, y / m_srTexelWidth)) :
			0;

	deUint32& cachedRate = m_simulationCache[(rate2 * kShadingRateCount + rate1) * kShadingRateCount + rate0];
	if (cachedRate != 0xFFFFFFFF)
	{
		return cachedRate;
	}

	const Extent2D extent0 = packedShadingRateToExtent(rate0);
	const Extent2D extent1 = packedShadingRateToExtent(rate1);
	const Extent2D extent2 = packedShadingRateToExtent(rate2);

	deUint32		  finalMask = 0;
	std::vector<bool> allowSwaps{ false, true };
	// Simulate once for implementations that don't allow swapping rate xy,
	// and once for those that do. Any of those results is allowed.
	for (bool allowSwap : allowSwaps)
	{
		// Combine rate 0 and 1, get a mask of possible clamped rates
		Extent2D intermediate	  = combine(extent0, extent1, m_tcParam.combinerOp0);
		deUint32 intermediateMask = shadingRateExtentToClampedMask(intermediate, allowSwap);

		// For each clamped rate, combine that with rate 2 and accumulate the possible clamped rates
		for (deUint32 i = 0; i < kShadingRateCount; ++i)
		{
			if (intermediateMask & (1 << i))
			{
				Extent2D final = combine(packedShadingRateToExtent(i), extent2, m_tcParam.combinerOp1);
				finalMask |= shadingRateExtentToClampedMask(final, allowSwap);
			}
		}
		{
			// unclamped intermediate value is also permitted
			Extent2D final = combine(intermediate, extent2, m_tcParam.combinerOp1);
			finalMask |= shadingRateExtentToClampedMask(final, allowSwap);
		}
	}

	cachedRate = finalMask;

	return finalMask;
}

/// translate packed rate to Extent
///
/// @param packedRate packed with (log(width) << 2 | log(height))
///
/// @return shading rate Extent
FragmentShadingRateCombined::Extent2D FragmentShadingRateCombined::packedShadingRateToExtent(deUint32 packedRate) const
{
	Extent2D ret = { static_cast<deUint32>(1 << ((packedRate / 4) & 3)), static_cast<deUint32>(1 << (packedRate & 3)) };

	return ret;
}

/// combine the two extent with given combine operation
///
/// @param extent0 extent0
/// @param extent1 extent1
/// @param combineOp combination operation
///
/// @return combined extent
FragmentShadingRateCombined::Extent2D FragmentShadingRateCombined::combine(Extent2D extent0, Extent2D extent1,
																		   glw::GLenum combineOp) const
{
	Extent2D resultExtent;

	switch (combineOp)
	{
	case GL_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_EXT:
		return extent0;
	case GL_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_EXT:
		return extent1;
	case GL_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_EXT:
		resultExtent.width	= std::min(extent0.width, extent1.width);
		resultExtent.height = std::min(extent0.height, extent1.height);
		break;
	case GL_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_EXT:
		resultExtent.width	= std::max(extent0.width, extent1.width);
		resultExtent.height = std::max(extent0.height, extent1.height);
		break;
	case GL_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_EXT:
		resultExtent.width	= extent0.width * extent1.width;
		resultExtent.height = extent0.height * extent1.height;
		break;
	}

	return resultExtent;
}

} // namespace glcts
