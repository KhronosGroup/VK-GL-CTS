/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2020 Google Inc.
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
 * \file  es3cNumberParsingTests.cpp
 * \brief Tests for numeric value parsing in GLSL ES 3.0
 */ /*-------------------------------------------------------------------*/

#include "es3cNumberParsingTests.hpp"

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

#include <string>
#include <vector>
#include <map>

#include <functional>

namespace es3cts
{

namespace
{
using std::string;
using std::vector;
using std::map;

using std::function;
using std::bind;
using namespace std::placeholders;

static const string					defaultVertexShader					=
	"#version 300 es\n"
	"in vec4 vPosition;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = vPosition;\n"
	"}\n";

static const string					fragmentShaderTemplate				=
	"#version 300 es\n"
	"precision highp float;\n"
	"out vec4 my_FragColor;\n"
	"${TEST_GLOBALS}"
	"void main()\n"
	"{\n"
    "${TEST_CODE}"
    "    my_FragColor = vec4(0.0, correct, 0.0, 1.0);\n"
	"}\n";

typedef function<void (const glu::ShaderProgram&, const glw::Functions&)> SetupUniformsFn;

enum struct TestType
{
	NORMAL				= 0,
	EXPECT_SHADER_FAIL
};

struct TestParams
{
	TestType			testType;
	string				name;
	string				description;
	string				testGlobals;
	string				testCode;
	SetupUniformsFn	setupUniformsFn;
};

static void initializeExpectedValue(const glu::ShaderProgram& program, const glw::Functions& gl, const deUint32 value);
static void initializeZeroValue(const glu::ShaderProgram& program, const glw::Functions& gl);

static const TestParams			tests[]									=
{
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_above_signed_range_decimal",																	// string			name
		"Test that uint value higher than INT_MAX is parsed correctly",													// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 3221225472u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 3221225472u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_above_signed_range_base8",																	// string			name
		"Test that uint value higher than INT_MAX is parsed correctly in base 8 (octal)",								// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 030000000000u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 3221225472u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_above_signed_range_base16",																	// string			name
		"Test that uint value higher than INT_MAX is parsed correctly in base 16 (hex)",								// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 0xc0000000u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 3221225472u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_smallest_value_above_signed_range_decimal",													// string			name
		"Test that uint value equal to INT_MAX+1 is parsed correctly",													// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 2147483648u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 2147483648u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_smallest_value_above_signed_range_base8",														// string			name
		"Test that uint value equal to INT_MAX+1 is parsed correctly in base 8 (octal)",								// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 020000000000u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 2147483648u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_smallest_value_above_signed_range_base16",													// string			name
		"Test that uint value equal to INT_MAX+1 is parsed correctly in base 16 (hex)",									// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 0x80000000u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 2147483648u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_max_value_decimal",																			// string			name
		"Test that uint value equal to UINT_MAX is parsed correctly",													// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 4294967295u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 4294967295u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_max_value_base8",																				// string			name
		"Test that uint value equal to UINT_MAX is parsed correctly in base 8 (octal)",									// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 037777777777u;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 4294967295u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_max_value_base16",																			// string			name
		"Test that uint value equal to UINT_MAX is parsed correctly in base 16 (hex)",									// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = 0xffffffffu;\n"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 4294967295u)																// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::EXPECT_SHADER_FAIL,																					// TestType			testType
		"unsigned_integer_too_large_value_invalid",																		// string			name
		"Test that uint value outside uint range fails to compile",														// string			description
		"",																												// string			testGlobals
		"    uint i        = 0xfffffffffu;"
		"    float correct = 0.0;",
		nullptr																											// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"unsigned_integer_negative_value_as_uint",																		// string			name
		"Test that -1u is parsed correctly",																			// string			description
		"uniform uint expected;\n",																						// string			testGlobals
		"    uint i        = -1u;"
		"    float correct = (i == expected) ? 1.0 : 0.0;\n",
		bind(initializeExpectedValue, _1, _2, 0xffffffffu)																// SetupUniformsFn	setupUniformsFn
	},
	/* The following floating point parsing tests are taken from the Khronos WebGL conformance tests at:
	 *     https://www.khronos.org/registry/webgl/sdk/tests/conformance2/glsl3/float-parsing.html */
	{
		TestType::NORMAL,																								// TestType			testType
		"float_out_of_range_as_infinity",																				// string			name
		"Floats of too large magnitude should be converted infinity",													// string			description
		"",																												// string			testGlobals
		"    // Out-of-range floats should overflow to infinity\n"														// string			testCode
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"
		"    // \"If the value of the floating point number is too large (small) to be stored as a single precision value, it is converted to positive (negative) infinity\"\n"
		"    float correct = isinf(1.0e40) ? 1.0 : 0.0;\n",
		nullptr																											// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"float_out_of_range_as_zero",																					// string			name
		"Floats of too small magnitude should be converted to zero",													// string			description
		"",																												// string			testGlobals
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"																	// string			testCode
		"    // \"A value with a magnitude too small to be represented as a mantissa and exponent is converted to zero.\"\n"
		"    // 1.0e-50 is small enough that it can't even be stored as subnormal.\n"
		"    float correct = (1.0e-50 == 0.0) ? 1.0 : 0.0;\n",
		nullptr																											// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"float_no_limit_on_number_of_digits_positive_exponent",															// string			name
		"Number of digits in any digit-sequence is not limited - test with a small mantissa and large exponent",		// string			description
		"",																												// string			testGlobals
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"																	// string			testCode
		"    // \"There is no limit on the number of digits in any digit-sequence.\"\n"
		"    // The below float string has 100 zeros after the decimal point, but represents 1.0.\n"
		"    float x = 0.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e101;\n"
		"    float correct = (x == 1.0) ? 1.0 : 0.0;\n",
		nullptr																											// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"float_no_limit_on_number_of_digits_negative_exponent",															// string			name
		"Number of digits in any digit-sequence is not limited - test with a large mantissa and negative exponent",		// string			description
		"",																												// string			testGlobals
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"																	// string			testCode
		"    // \"There is no limit on the number of digits in any digit-sequence.\"\n"
		"    // The below float string has 100 zeros, but represents 1.0.\n"
		"    float x = 10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.0e-100;\n"
		"    float correct = (x == 1.0) ? 1.0 : 0.0;\n",
		nullptr																											// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"float_slightly_out_of_range_exponent_as_positive_infinity",													// string			name
		"Test that an exponent that slightly overflows signed 32-bit int range works",									// string			description
		"",																												// string			testGlobals
		"    // Out-of-range floats should overflow to infinity\n"														// string			testCode
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"
		"    // \"If the value of the floating point number is too large (small) to be stored as a single precision value, it is converted to positive (negative) infinity\"\n"
		"    float correct = isinf(1.0e2147483649) ? 1.0 : 0.0;\n",
		nullptr																											// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"float_overflow_to_positive_infinity",																			// string			name
		"Out-of-range floats greater than zero should overflow to positive infinity",									// string			description
		"uniform float zero;\n",																						// string			testGlobals
		"    // Out-of-range floats should overflow to infinity\n"														// string			testCode
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"
		"    // \"If the value of the floating point number is too large (small) to be stored as a single precision value, it is converted to positive (negative) infinity\"\n"
		"    float f = 1.0e2048 - zero;\n"
		"    float correct = (isinf(f) && f > 0.0) ? 1.0 : 0.0;\n",
		initializeZeroValue																								// SetupUniformsFn	setupUniformsFn
	},
	{
		TestType::NORMAL,																								// TestType			testType
		"float_overflow_to_negative_infinity",																			// string			name
		"Out-of-range floats less than zero should overflow to negative infinity",										// string			description
		"uniform float zero;\n",																						// string			testGlobals
		"    // Out-of-range floats should overflow to infinity\n"														// string			testCode
		"    // GLSL ES 3.00.6 section 4.1.4 Floats:\n"
		"    // \"If the value of the floating point number is too large (small) to be stored as a single precision value, it is converted to positive (negative) infinity\"\n"
		"    float f = -1.0e2048 + zero;\n"
		"    float correct = (isinf(f) && f < 0.0) ? 1.0 : 0.0;\n",
		initializeZeroValue																								// SetupUniformsFn	setupUniformsFn
	}
};

static void initializeExpectedValue(const glu::ShaderProgram& program, const glw::Functions& gl, const deUint32 value)
{
	const auto location = gl.getUniformLocation(program.getProgram(), "expected");
	GLU_EXPECT_NO_ERROR(gl.getError(), "GetAttribLocation call failed");

	gl.uniform1ui(location, value);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Set uniform value failed");
}

static void initializeZeroValue(const glu::ShaderProgram& program, const glw::Functions& gl)
{
	const auto location = gl.getUniformLocation(program.getProgram(), "zero");
	GLU_EXPECT_NO_ERROR(gl.getError(), "GetAttribLocation call failed");

	gl.uniform1f(location, 0.0f);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Set uniform value failed");
}

static string replacePlaceholders(const string& shaderTemplate, const TestParams& params)
{
	map<string,string> fields;
	fields["TEST_GLOBALS"]	= params.testGlobals;
	fields["TEST_CODE"]		= params.testCode;

	tcu::StringTemplate output(shaderTemplate);
	return output.specialize(fields);
}

static const std::vector<float>		positions				=
{
	-1.0f, -1.0f,
	 1.0f, -1.0f,
	-1.0f,  1.0f,
	 1.0f,  1.0f
};

static const std::vector<deUint32>	indices					= { 0, 1, 2, 3 };

const deInt32						RENDERTARGET_WIDTH		= 16;
const deInt32						RENDERTARGET_HEIGHT		= 16;

class NumberParsingCase : public deqp::TestCase
{
public:
	NumberParsingCase(deqp::Context& context, const string& name, const TestParams& params, const string& vertexShader, const string& fragmentShader);

	IterateResult iterate();

private:
	void setupRenderTarget();
	void releaseRenderTarget();

	glw::GLuint			m_fboId;
	glw::GLuint			m_rboId;

	const TestParams&	m_params;
	string				m_vertexShader;
	string				m_fragmentShader;
};

NumberParsingCase::NumberParsingCase(deqp::Context& context, const string& name, const TestParams& params, const string& vertexShader, const string& fragmentShader)
	: TestCase(context, name.c_str(), params.description.c_str())
	, m_params(params)
	, m_vertexShader(vertexShader)
	, m_fragmentShader(fragmentShader)
{
}

NumberParsingCase::IterateResult NumberParsingCase::iterate(void)
{
	const auto&	renderContext	= m_context.getRenderContext();
	const auto&	gl				= renderContext.getFunctions();
	const auto	textureFormat	= tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNORM_INT8);
	const auto	transferFormat	= glu::getTransferFormat(textureFormat);

	setupRenderTarget();

	glu::ShaderProgram program(renderContext, glu::makeVtxFragSources(m_vertexShader, m_fragmentShader));
	if (!program.isOk())
		switch(m_params.testType)
		{
		case TestType::EXPECT_SHADER_FAIL:
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
			return STOP;
		default:
			TCU_FAIL("Shader compilation failed:\nVertex shader:\n" + m_vertexShader + "\nFragment shader:\n" + m_fragmentShader);
			break;
		}

	const std::vector<glu::VertexArrayBinding> vertexArrays =
	{
		glu::va::Float("vPosition", 2, positions.size(), 0, positions.data()),
	};

	gl.useProgram(program.getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram failed");

	if (m_params.setupUniformsFn != DE_NULL)
		m_params.setupUniformsFn(program, gl);

	gl.clear(GL_COLOR_BUFFER_BIT);


	glu::draw(renderContext, program.getProgram(),
			  static_cast<int>(vertexArrays.size()), vertexArrays.data(),
			  glu::pr::TriangleStrip(static_cast<int>(indices.size()), indices.data()));

	const auto						pixelSize				= tcu::getPixelSize(textureFormat);
	std::vector<deUint8>			fbData					(RENDERTARGET_WIDTH * RENDERTARGET_HEIGHT * pixelSize);

	if (pixelSize < 4)
		gl.pixelStorei(GL_PACK_ALIGNMENT, 1);

	gl.readPixels(0, 0, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT, transferFormat.format, transferFormat.dataType, fbData.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");

	tcu::ConstPixelBufferAccess		fbAccess				{ textureFormat, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT, 1, fbData.data() };
	const auto						expectedColor			= tcu::RGBA::green().toVec();
	bool pass = true;
	for(int y = 0; pass && y < RENDERTARGET_HEIGHT; ++y)
		for(int x = 0; x < RENDERTARGET_WIDTH; ++x)
			if (fbAccess.getPixel(x,y) != expectedColor)
			{
				pass = false;
				break;
			}

	releaseRenderTarget();

	const qpTestResult				result					= (pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL);
	const char*						desc					= (pass ? "Pass" : "Pixel mismatch; numeric value parsed incorrectly");

	m_testCtx.setTestResult(result, desc);

	return STOP;
}

void NumberParsingCase::setupRenderTarget()
{
	const auto&	renderContext	= m_context.getRenderContext();
	const auto&	gl				= renderContext.getFunctions();

	gl.genFramebuffers(1, &m_fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenFramebuffers");

	gl.genRenderbuffers(1, &m_rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "GenRenderBuffers");

	gl.bindRenderbuffer(GL_RENDERBUFFER, m_rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindRenderBuffer");

	gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "RenderBufferStorage");

	gl.bindFramebuffer(GL_FRAMEBUFFER, m_fboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "BindFrameBuffer");

	gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_rboId);
	GLU_EXPECT_NO_ERROR(gl.getError(), "FrameBufferRenderBuffer");

	glw::GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
	gl.drawBuffers(1, &drawBuffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "DrawBuffers");

	glw::GLfloat clearColor[4] = { 0, 0, 0, 0 };
	gl.clearBufferfv(GL_COLOR, 0, clearColor);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ClearBuffers");

	gl.viewport(0, 0, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Viewport");
}

void NumberParsingCase::releaseRenderTarget()
{
	const auto&	renderContext	= m_context.getRenderContext();
	const auto&	gl				= renderContext.getFunctions();
	if (m_fboId != 0)
	{
		gl.deleteFramebuffers(1, &m_fboId);
		m_fboId = 0;
	}
	if (m_rboId != 0)
	{
		gl.deleteRenderbuffers(1, &m_rboId);
		m_rboId = 0;
	}
}

}

NumberParsingTests::NumberParsingTests(deqp::Context& context)
	: deqp::TestCaseGroup(context, "number_parsing", "GLSL number parsing tests")
{
}

NumberParsingTests::~NumberParsingTests(void)
{
}

void NumberParsingTests::init(void)
{
	for(const auto& params : tests)
	{
		addChild(new NumberParsingCase(m_context, params.name, params, defaultVertexShader, replacePlaceholders(fragmentShaderTemplate, params)));
	}
}

}
