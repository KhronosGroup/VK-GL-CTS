/*-------------------------------------------------------------------------
* OpenGL Conformance Test Suite
* -----------------------------
*
* Copyright (c) 2017 The Khronos Group Inc.
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
* \file  gl4cShaderGroupVoteTests.cpp
* \brief Conformance tests for the ARB_shader_group_vote functionality.
*/ /*-------------------------------------------------------------------*/

#include "gl4cShaderGroupVoteTests.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluDrawUtil.hpp"
#include "gluObjectWrapper.hpp"
#include "gluShaderProgram.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuRenderTarget.hpp"

using namespace glw;

namespace gl4cts
{

ShaderGroupVoteTestCaseBase::ComputeShader::ComputeShader(const std::string& name, const std::string& shader)
	: m_name(name), m_shader(shader), m_program(NULL), m_compileOnly(true)
{
}

ShaderGroupVoteTestCaseBase::ComputeShader::ComputeShader(const std::string& name, const std::string& shader,
														  const tcu::Vec4& desiredColor)
	: m_name(name), m_shader(shader), m_program(NULL), m_desiredColor(desiredColor), m_compileOnly(false)
{
}

ShaderGroupVoteTestCaseBase::ComputeShader::~ComputeShader()
{
	if (m_program)
	{
		delete m_program;
	}
}

void ShaderGroupVoteTestCaseBase::ComputeShader::create(deqp::Context& context)
{
	glu::ProgramSources sourcesCompute;
	sourcesCompute.sources[glu::SHADERTYPE_COMPUTE].push_back(m_shader);
	m_program = new glu::ShaderProgram(context.getRenderContext(), sourcesCompute);

	if (!m_program->isOk())
	{
		TCU_FAIL("Shader compilation failed");
	}
}

void ShaderGroupVoteTestCaseBase::ComputeShader::execute(deqp::Context& context)
{
	if (m_compileOnly)
	{
		return;
	}

	const glw::Functions& gl = context.getRenderContext().getFunctions();
	const glu::Texture	outputTexture(context.getRenderContext());

	gl.clearColor(0.5f, 0.5f, 0.5f, 1.0f);
	gl.clear(GL_COLOR_BUFFER_BIT);

	gl.useProgram(m_program->getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram failed");

	// output image
	gl.bindTexture(GL_TEXTURE_2D, *outputTexture);
	gl.texStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, 16, 16);
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Uploading image data failed");

	// bind image
	gl.bindImageTexture(2, *outputTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);
	GLU_EXPECT_NO_ERROR(gl.getError(), "bindImageTexture failed");

	// dispatch compute
	gl.dispatchCompute(1, 1, 1);
	GLU_EXPECT_NO_ERROR(gl.getError(), "dispatchCompute failed");

	// render output texture
	std::string vs = "#version 450 core\n"
					 "in highp vec2 position;\n"
					 "in vec2 inTexcoord;\n"
					 "out vec2 texcoord;\n"
					 "void main()\n"
					 "{\n"
					 "	texcoord = inTexcoord;\n"
					 "	gl_Position = vec4(position, 0.0, 1.0);\n"
					 "}\n";

	std::string fs = "#version 450 core\n"
					 "uniform sampler2D sampler;\n"
					 "in vec2 texcoord;\n"
					 "out vec4 color;\n"
					 "void main()\n"
					 "{\n"
					 "	color = texture(sampler, texcoord);\n"
					 "}\n";

	glu::ProgramSources sources;
	sources.sources[glu::SHADERTYPE_VERTEX].push_back(vs);
	sources.sources[glu::SHADERTYPE_FRAGMENT].push_back(fs);
	glu::ShaderProgram renderShader(context.getRenderContext(), sources);

	if (!m_program->isOk())
	{
		TCU_FAIL("Shader compilation failed");
	}

	gl.bindTexture(GL_TEXTURE_2D, *outputTexture);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glBindTexture() call failed.");

	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	GLU_EXPECT_NO_ERROR(gl.getError(), "texParameteri failed");

	gl.useProgram(renderShader.getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram failed");

	gl.uniform1i(gl.getUniformLocation(renderShader.getProgram(), "sampler"), 0);
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUniform1i failed");

	deUint16 const quadIndices[] = { 0, 1, 2, 2, 1, 3 };

	float const position[] = { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };

	float const texCoord[] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };

	glu::VertexArrayBinding vertexArrays[] = { glu::va::Float("position", 2, 4, 0, position),
											   glu::va::Float("inTexcoord", 2, 4, 0, texCoord) };

	glu::draw(context.getRenderContext(), renderShader.getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays), vertexArrays,
			  glu::pr::TriangleStrip(DE_LENGTH_OF_ARRAY(quadIndices), quadIndices));

	GLU_EXPECT_NO_ERROR(gl.getError(), "glu::draw error");

	gl.flush();
}

void ShaderGroupVoteTestCaseBase::ComputeShader::validate(deqp::Context& context)
{
	if (m_compileOnly)
	{
		return;
	}

	bool		validationResult   = validateScreenPixels(context, m_desiredColor);
	std::string validationErrorMsg = "Validation failed for " + m_name + " test";

	TCU_CHECK_MSG(validationResult, validationErrorMsg.c_str());
}

bool ShaderGroupVoteTestCaseBase::ComputeShader::validateColor(tcu::Vec4 testedColor, tcu::Vec4 desiredColor)
{
	const float epsilon = 0.008f;
	return de::abs(testedColor.x() - desiredColor.x()) < epsilon &&
		   de::abs(testedColor.y() - desiredColor.y()) < epsilon &&
		   de::abs(testedColor.z() - desiredColor.z()) < epsilon;
}

bool ShaderGroupVoteTestCaseBase::ComputeShader::validateScreenPixels(deqp::Context& context, tcu::Vec4 desiredColor)
{
	const glw::Functions&   gl			 = context.getRenderContext().getFunctions();
	const tcu::RenderTarget renderTarget = context.getRenderContext().getRenderTarget();
	tcu::IVec2				size(renderTarget.getWidth(), renderTarget.getHeight());

	glw::GLfloat* pixels = new glw::GLfloat[size.x() * size.y() * 4];

	// clear buffer
	for (int x = 0; x < size.x(); ++x)
	{
		for (int y = 0; y < size.y(); ++y)
		{
			int mappedPixelPosition = y * size.x() + x;

			pixels[mappedPixelPosition * 4 + 0] = -1.0f;
			pixels[mappedPixelPosition * 4 + 1] = -1.0f;
			pixels[mappedPixelPosition * 4 + 2] = -1.0f;
			pixels[mappedPixelPosition * 4 + 3] = -1.0f;
		}
	}

	// read pixels
	gl.readPixels(0, 0, size.x(), size.y(), GL_RGBA, GL_FLOAT, pixels);

	// validate pixels
	bool validationResult = true;

	for (int x = 0; x < size.x(); ++x)
	{
		for (int y = 0; y < size.y(); ++y)
		{
			int mappedPixelPosition = y * size.x() + x;

			tcu::Vec4 color(pixels[mappedPixelPosition * 4 + 0], pixels[mappedPixelPosition * 4 + 1],
							pixels[mappedPixelPosition * 4 + 2], pixels[mappedPixelPosition * 4 + 3]);

			if (!validateColor(color, desiredColor))
			{
				validationResult = false;
			}
		}
	}

	delete[] pixels;

	return validationResult;
}

/** Constructor.
*
*  @param context Rendering context
*  @param name Test name
*  @param description Test description
*/
ShaderGroupVoteTestCaseBase::ShaderGroupVoteTestCaseBase(deqp::Context& context, const char* name,
														 const char* description)
	: TestCaseBase(context, glcts::ExtParameters(glu::GLSL_VERSION_450, glcts::EXTENSIONTYPE_EXT), name, description)
	, m_glslFunctionPostfix("")

{
	glu::ContextType contextType		 = m_context.getRenderContext().getType();
	bool			 contextSupportsGL46 = glu::contextSupports(contextType, glu::ApiType::core(4, 6));
	m_extensionSupported =
		contextSupportsGL46 || context.getContextInfo().isExtensionSupported("GL_ARB_shader_group_vote");

	if (contextSupportsGL46)
	{
		m_specializationMap["VERSION"]					  = "#version 460 core";
		m_specializationMap["GROUP_VOTE_EXTENSION"]		  = "";
		m_specializationMap["ALL_INVOCATIONS_FUNC"]		  = "allInvocations";
		m_specializationMap["ANY_INVOCATION_FUNC"]		  = "anyInvocation";
		m_specializationMap["ALL_INVOCATIONS_EQUAL_FUNC"] = "allInvocationsEqual";
	}
	else
	{
		m_specializationMap["VERSION"]					  = "#version 450 core";
		m_specializationMap["GROUP_VOTE_EXTENSION"]		  = "#extension GL_ARB_shader_group_vote : enable";
		m_specializationMap["ALL_INVOCATIONS_FUNC"]		  = "allInvocationsARB";
		m_specializationMap["ANY_INVOCATION_FUNC"]		  = "anyInvocationARB";
		m_specializationMap["ALL_INVOCATIONS_EQUAL_FUNC"] = "allInvocationsEqualARB";
		m_glslFunctionPostfix							  = "ARB";
	}
}

void ShaderGroupVoteTestCaseBase::init()
{
	if (m_extensionSupported)
	{
		for (ComputeShaderIter iter = m_shaders.begin(); iter != m_shaders.end(); ++iter)
		{
			(*iter)->create(m_context);
		}
	}
}

void ShaderGroupVoteTestCaseBase::deinit()
{
	for (ComputeShaderIter iter = m_shaders.begin(); iter != m_shaders.end(); ++iter)
	{
		delete (*iter);
	}
}

tcu::TestNode::IterateResult ShaderGroupVoteTestCaseBase::iterate()
{
	if (!m_extensionSupported)
	{
		m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
		return STOP;
	}

	for (ComputeShaderIter iter = m_shaders.begin(); iter != m_shaders.end(); ++iter)
	{
		(*iter)->execute(m_context);
		(*iter)->validate(m_context);
	}

	m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
	return STOP;
}

/** Constructor.
*
*  @param context Rendering context
*/
ShaderGroupVoteAvailabilityTestCase::ShaderGroupVoteAvailabilityTestCase(deqp::Context& context)
	: ShaderGroupVoteTestCaseBase(context, "ShaderGroupVoteAvailabilityTestCase", "Implements ...")
{
	const char* shader = "${VERSION}\n"
						 "${GROUP_VOTE_EXTENSION}\n"
						 "layout(rgba32f, binding = 2) writeonly uniform highp image2D destImage;\n"
						 "layout(local_size_x = 16, local_size_y = 16) in;\n"
						 "void main (void)\n"
						 "{\n"
						 "	vec4 outColor = vec4(0.0);\n"
						 "	outColor.r = ${ALL_INVOCATIONS_FUNC}(true) ? 1.0 : 0.0;\n"
						 "	outColor.g = ${ANY_INVOCATION_FUNC}(true) ? 1.0 : 0.0;\n"
						 "	outColor.b = ${ALL_INVOCATIONS_EQUAL_FUNC}(true) ? 1.0 : 0.0;\n"
						 "	imageStore(destImage, ivec2(gl_GlobalInvocationID.xy), outColor);\n"
						 "}\n";
	std::string cs = specializeShader(1, &shader);
	m_shaders.push_back(new ComputeShader("availability", cs));
}

/** Constructor.
*
*  @param context Rendering context
*  @param name Test name
*  @param description Test description
*/
ShaderGroupVoteFunctionTestCaseBase::ShaderGroupVoteFunctionTestCaseBase(deqp::Context& context, const char* name,
																		 const char* description)
	: ShaderGroupVoteTestCaseBase(context, name, description)
{
	m_shaderBase = "${VERSION}\n"
				   "${GROUP_VOTE_EXTENSION}\n"
				   "layout(rgba32f, binding = 2) writeonly uniform highp image2D destImage;\n"
				   "layout(local_size_x = 16, local_size_y = 16) in;\n"
				   "void main (void)\n"
				   "{\n"
				   "	bool result = ${FUNC_RESULT};\n"
				   "	vec4 outColor = vec4(vec3(result ? 1.0 : 0.0), 1.0);\n"
				   "	imageStore(destImage, ivec2(gl_GlobalInvocationID.xy), outColor);\n"
				   "}\n";
}

/** Constructor.
*
*  @param context Rendering context
*/
ShaderGroupVoteAllInvocationsTestCase::ShaderGroupVoteAllInvocationsTestCase(deqp::Context& context)
	: ShaderGroupVoteFunctionTestCaseBase(context, "ShaderGroupVoteAllInvocationsTestCase", "Implements ...")
{
	m_specializationMap["FUNC_RESULT"] = "allInvocations" + m_glslFunctionPostfix + "(true)";
	m_shaders.push_back(
		new ComputeShader("allInvocationsARB", specializeShader(1, &m_shaderBase), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)));
}

/** Constructor.
*
*  @param context Rendering context
*/
ShaderGroupVoteAnyInvocationTestCase::ShaderGroupVoteAnyInvocationTestCase(deqp::Context& context)
	: ShaderGroupVoteFunctionTestCaseBase(context, "ShaderGroupVoteAnyInvocationTestCase", "Implements ...")
{
	m_specializationMap["FUNC_RESULT"] = "anyInvocation" + m_glslFunctionPostfix + "(false)";
	m_shaders.push_back(
		new ComputeShader("anyInvocationARB", specializeShader(1, &m_shaderBase), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));
}

/** Constructor.
*
*  @param context Rendering context
*/
ShaderGroupVoteAllInvocationsEqualTestCase::ShaderGroupVoteAllInvocationsEqualTestCase(deqp::Context& context)
	: ShaderGroupVoteFunctionTestCaseBase(context, "ShaderGroupVoteAllInvocationsEqualTestCase", "Implements ...")
{
	m_specializationMap["FUNC_RESULT"] = "allInvocationsEqual" + m_glslFunctionPostfix + "(true)";
	m_shaders.push_back(new ComputeShader("allInvocationsEqual", specializeShader(1, &m_shaderBase),
										  tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)));

	m_specializationMap["FUNC_RESULT"] = "allInvocationsEqual" + m_glslFunctionPostfix + "(false)";
	m_shaders.push_back(new ComputeShader("allInvocationsEqual", specializeShader(1, &m_shaderBase),
										  tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f)));
}

/** Constructor.
*
*  @param context Rendering context.
*/
ShaderGroupVote::ShaderGroupVote(deqp::Context& context)
	: TestCaseGroup(context, "shader_group_vote_tests",
					"Verify conformance of CTS_ARB_shader_group_vote implementation")
{
}

/** Initializes the test group contents. */
void ShaderGroupVote::init()
{
	addChild(new ShaderGroupVoteAvailabilityTestCase(m_context));
	addChild(new ShaderGroupVoteAllInvocationsTestCase(m_context));
	addChild(new ShaderGroupVoteAnyInvocationTestCase(m_context));
	addChild(new ShaderGroupVoteAllInvocationsEqualTestCase(m_context));
}
} /* gl4cts namespace */
