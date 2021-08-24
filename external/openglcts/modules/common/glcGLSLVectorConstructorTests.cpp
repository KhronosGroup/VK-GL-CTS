/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file
 * \brief GLSL vector constructor tests.
 */ /*-------------------------------------------------------------------*/
#include "glcGLSLVectorConstructorTests.hpp"

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

#include <functional>
#include <map>
#include <vector>
#include <sstream>
#include <string>
#include <tuple>

namespace deqp
{

namespace
{
using std::string;

using std::map;
using std::vector;

using std::function;
using std::bind;
using namespace std::placeholders;

using std::ostringstream;

enum struct TestType
{
	VERTEX_SHADER_ERROR = 0,
	FRAGMENT_SHADER_ERROR,
	VERTEX_SHADER,
	FRAGMENT_SHADER
};

struct TestDefinition
{
	vector<string>			outputTypes;
	vector<vector<string>>	inputTypeLists;
	string					extraFields;
};

const TestDefinition					tests[]					=
{
	{
		{ "vec2", "vec3", "vec4" },								// vector<string>			outputTypes
		{														// vector<vector<string>>	inputTypeLists
			{ "mat2" },
			{ "mat2x3" },
			{ "mat2x4" },
			{ "mat3" },
			{ "mat3x2" },
			{ "mat3x4" },
			{ "mat4" },
			{ "mat4x2" },
			{ "mat4x3" },
			{ "float", "mat2" },
			{ "float", "mat2x3" },
			{ "float", "mat2x4" },
			{ "float", "mat3" },
			{ "float", "mat3x2" },
			{ "float", "mat3x4" },
			{ "float", "mat4" },
			{ "float", "mat4x2" },
			{ "float", "mat4x3" },
		},
		"const float errorBound = 1.0E-5;\n"					// deUint32					extraFields;
	},
	{
		{ "ivec2", "ivec3", "ivec4" },							// vector<string>			outputTypes
		{														// vector<vector<string>>	inputTypeLists
			{ "mat2" },
			{ "mat2x3" },
			{ "mat2x4" },
			{ "mat3" },
			{ "mat3x2" },
			{ "mat3x4" },
			{ "mat4" },
			{ "mat4x2" },
			{ "mat4x3" },
			{ "int", "mat2" },
			{ "int", "mat2x3" },
			{ "int", "mat2x4" },
			{ "int", "mat3" },
			{ "int", "mat3x2" },
			{ "int", "mat3x4" },
			{ "int", "mat4" },
			{ "int", "mat4x2" },
			{ "int", "mat4x3" },
		},
		""														// deUint32					extraFields;
	},
	{
		{ "bvec2", "bvec3", "bvec4" },							// vector<string>			outputTypes
		{														// vector<vector<string>>	inputTypeLists
			{ "mat2" },
			{ "mat2x3" },
			{ "mat2x4" },
			{ "mat3" },
			{ "mat3x2" },
			{ "mat3x4" },
			{ "mat4" },
			{ "mat4x2" },
			{ "mat4x3" },
			{ "bool", "mat2" },
			{ "bool", "mat2x3" },
			{ "bool", "mat2x4" },
			{ "bool", "mat3" },
			{ "bool", "mat3x2" },
			{ "bool", "mat3x4" },
			{ "bool", "mat4" },
			{ "bool", "mat4x2" },
			{ "bool", "mat4x3" },
		},
		""														// deUint32					extraFields;
	},
};

struct TestParams
{
	string			name;
	string			description;
	TestType		testType;
	string			outputType;
	vector<string>	inputTypes;
	string			extraFields;
};

vector<TestParams> generateTestParams()
{
	vector<TestParams> result;
	result.reserve(64);
	for(const auto& test : tests)
	{
		for(const auto& outputType : test.outputTypes)
		{
			for(const auto& inputTypes : test.inputTypeLists)
			{
				ostringstream testNameVs, testNameFs;
				ostringstream testDescriptionVs, testDescriptionFs;
				testNameVs << outputType << "_from";
				testNameFs << outputType << "_from";
				testDescriptionVs << outputType << "(";
				testDescriptionFs << outputType << "(";
				for(vector<string>::size_type i = 0; i < inputTypes.size(); ++i)
				{
					const auto& inputType = inputTypes[i];
					testNameVs << "_" << inputType;
					testNameFs << "_" << inputType;
					if (i > 0) {
						testDescriptionVs << ",";
						testDescriptionFs << ",";
					}
					testDescriptionVs << inputType;
				}
				ostringstream testNameInvalidVs, testNameInvalidFs;
				testNameInvalidVs << testNameVs.str() << "_" << inputTypes[0] << "_invalid_vs";
				testNameInvalidFs << testNameFs.str() << "_" << inputTypes[0] << "_invalid_fs";

				testNameVs << "_vs";
				testNameFs << "_fs";
				testDescriptionVs << ") vertex shader";
				testDescriptionFs << ") fragment shader";
				result.push_back({ testNameVs.str(), testDescriptionVs.str(), TestType::VERTEX_SHADER, outputType, inputTypes, test.extraFields });
				result.push_back({ testNameFs.str(), testDescriptionFs.str(), TestType::FRAGMENT_SHADER, outputType, inputTypes, test.extraFields });

				vector<string> failInputTypes;
				failInputTypes.insert(failInputTypes.end(), inputTypes.begin(), inputTypes.end());
				failInputTypes.push_back(inputTypes[0]);
				testDescriptionVs << " invalid";
				testDescriptionFs << " invalid";
				result.push_back({ testNameInvalidVs.str(), testDescriptionVs.str(), TestType::VERTEX_SHADER_ERROR, outputType, failInputTypes, test.extraFields });
				result.push_back({ testNameInvalidFs.str(), testDescriptionFs.str(), TestType::FRAGMENT_SHADER_ERROR, outputType, failInputTypes, test.extraFields });

			}
		}
	}
	return result;
}

const string									defaultVertexShader		=
	"${GLSL_VERSION}\n"
	"in vec4 vPosition;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = vPosition;\n"
	"}\n";

const string									defaultFragmentShader	=
	"${GLSL_VERSION}\n"
	"precision mediump float;\n"
	"in vec4 vColor;\n"
	"out vec4 my_FragColor;\n"
	"void main() {\n"
	"    my_FragColor = vColor;\n"
	"}\n";

const string									vertexShaderTemplate	=
	"${GLSL_VERSION}\n"
	"in vec4 vPosition;\n"
	"precision mediump int;\n"
	"precision mediump float;\n"
	"const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
	"const vec4 red	  = vec4(1.0, 0.0, 0.0, 1.0);\n"
	"${TEST_CONSTANTS}"
	"out vec4 vColor;\n"
	"void main() {\n"
	"    ${TEST_CODE}\n"
	"    if ${TEST_CONDITION}\n"
	"        vColor = green;\n"
	"    else\n"
	"        vColor = red;\n"
	"    gl_Position = vPosition;\n"
	"}\n";

const string									fragmentShaderTemplate	=
	"${GLSL_VERSION}\n"
	"precision mediump int;\n"
	"precision mediump float;\n"
	"const vec4 green = vec4(0.0, 1.0, 0.0, 1.0);\n"
	"const vec4 red	  = vec4(1.0, 0.0, 0.0, 1.0);\n"
	"${TEST_CONSTANTS}"
	"out vec4 my_FragColor;\n"
	"void main() {\n"
	"    ${TEST_CODE}\n"
	"    if ${TEST_CONDITION}\n"
	"        my_FragColor = green;\n"
	"    else\n"
	"        my_FragColor = red;\n"
	"}\n";

const map<string, string>						testConditions			=
{
	{ "vec2"	, "(abs(v[0] - 0.0) <= errorBound && abs(v[1] - 1.0) <= errorBound)" },
	{ "vec3"	, "(abs(v[0] - 0.0) <= errorBound && abs(v[1] - 1.0) <= errorBound && abs(v[2] - 2.0) <= errorBound)" },
	{ "vec4"	, "(abs(v[0] - 0.0) <= errorBound && abs(v[1] - 1.0) <= errorBound && abs(v[2] - 2.0) <= errorBound && abs(v[3] - 3.0) <= errorBound)" },
	{ "ivec2"	, "(v[0] == 0 && v[1] == 1)" },
	{ "ivec3"	, "(v[0] == 0 && v[1] == 1 && v[2] == 2)" },
	{ "ivec4"	, "(v[0] == 0 && v[1] == 1 && v[2] == 2 && v[3] == 3)" },
	{ "bvec2"	, "(v[0] == false && v[1] == true)" },
	{ "bvec3"	, "(v[0] == false && v[1] == true && v[2] == true)" },
	{ "bvec4"	, "(v[0] == false && v[1] == true && v[2] == true && v[3] == true)" }
};

typedef function<void (ostringstream&, size_t)> GeneratorFn;

struct DataTypeInfo
{
	size_t			numElements;
	GeneratorFn		valueFn;
	GeneratorFn		beforeValueFn;
	GeneratorFn		afterValueFn;
};

void generateValueFloat(ostringstream& out, const size_t index)
{
	out << index << ".0";
}

void generateValueInt(ostringstream& out, const size_t index)
{
	out << index;
}

void generateValueBool(ostringstream& out, const size_t index)
{
	out << ((index != 0) ? "true" : "false");
}

void generateCtorOpen(const char* className, ostringstream& out, const size_t)
{
	out << className << "(";
}

void generateCtorClose(ostringstream &out, const size_t)
{
	out << ")";
}

const map<string, DataTypeInfo>					dataTypeInfos			=
{
	//				numElements	, valueFn			, beforeValueFn								, afterValueFn
	{ "float"	, { 1			, generateValueFloat, DE_NULL									, DE_NULL			} },
	{ "vec2"	, { 2			, generateValueFloat, bind(generateCtorOpen, "vec2", _1, _2)	, generateCtorClose	} },
	{ "vec3"	, { 3			, generateValueFloat, bind(generateCtorOpen, "vec3", _1, _2)	, generateCtorClose	} },
	{ "vec4"	, { 4			, generateValueFloat, bind(generateCtorOpen, "vec4", _1, _2)	, generateCtorClose	} },
	{ "int"		, { 1			, generateValueInt	, DE_NULL									, DE_NULL			} },
	{ "ivec2"	, { 2			, generateValueInt	, bind(generateCtorOpen, "ivec2", _1, _2)	, generateCtorClose	} },
	{ "ivec3"	, { 3			, generateValueInt	, bind(generateCtorOpen, "ivec3", _1, _2)	, generateCtorClose	} },
	{ "ivec4"	, { 4			, generateValueInt	, bind(generateCtorOpen, "ivec4", _1, _2)	, generateCtorClose	} },
	{ "bool"	, { 1			, generateValueBool	, DE_NULL									, DE_NULL			} },
	{ "bvec2"	, { 2			, generateValueBool	, bind(generateCtorOpen, "bvec2", _1, _2)	, generateCtorClose	} },
	{ "bvec3"	, { 3			, generateValueBool	, bind(generateCtorOpen, "bvec3", _1, _2)	, generateCtorClose	} },
	{ "bvec4"	, { 4			, generateValueBool	, bind(generateCtorOpen, "bvec4", _1, _2)	, generateCtorClose	} },
	{ "mat2"	, { 4			, generateValueFloat, bind(generateCtorOpen, "mat2", _1, _2)	, generateCtorClose	} },
	{ "mat2x3"	, { 6			, generateValueFloat, bind(generateCtorOpen, "mat2x3", _1, _2)	, generateCtorClose	} },
	{ "mat2x4"	, { 8			, generateValueFloat, bind(generateCtorOpen, "mat2x4", _1, _2)	, generateCtorClose	} },
	{ "mat3"	, { 9			, generateValueFloat, bind(generateCtorOpen, "mat3", _1, _2)	, generateCtorClose	} },
	{ "mat3x2"	, { 6			, generateValueFloat, bind(generateCtorOpen, "mat3x2", _1, _2)	, generateCtorClose	} },
	{ "mat3x4"	, { 12			, generateValueFloat, bind(generateCtorOpen, "mat3x4", _1, _2)	, generateCtorClose	} },
	{ "mat4"	, { 16			, generateValueFloat, bind(generateCtorOpen, "mat4", _1, _2)	, generateCtorClose	} },
	{ "mat4x2"	, { 8			, generateValueFloat, bind(generateCtorOpen, "mat4x2", _1, _2)	, generateCtorClose	} },
	{ "mat4x3"	, { 12			, generateValueFloat, bind(generateCtorOpen, "mat4x3", _1, _2)	, generateCtorClose	} },
};

string generateTestCode(const string& outputType, const vector<string>& inputTypes)
{
	ostringstream output;
	const auto outputTypeInfo = dataTypeInfos.find(outputType);
	DE_ASSERT(outputTypeInfo != dataTypeInfos.end());

	output << outputType << " v = ";
	if (outputTypeInfo->second.beforeValueFn != DE_NULL)
		outputTypeInfo->second.beforeValueFn(output, -1);
	int outputElementsRemaining = outputTypeInfo->second.numElements;
	int outputElementIndex = 0;
	for(size_t i = 0; i < inputTypes.size() && outputElementsRemaining > 0; ++i)
	{
		const auto& inputType = inputTypes[i];
		const auto inputTypeInfo = dataTypeInfos.find(inputType);
		DE_ASSERT(inputTypeInfo != dataTypeInfos.end());

		if (outputElementIndex > 0)
			output << ", ";
		if (inputTypeInfo->second.beforeValueFn != DE_NULL)
			inputTypeInfo->second.beforeValueFn(output, i);
		for(size_t j = 0; j < inputTypeInfo->second.numElements; ++j)
		{
			if (j > 0)
				output << ", ";

			inputTypeInfo->second.valueFn(output, outputElementIndex++);
			--outputElementsRemaining;
		}
		if (inputTypeInfo->second.afterValueFn != DE_NULL)
			inputTypeInfo->second.afterValueFn(output, i);
	}
	if (outputTypeInfo->second.afterValueFn != DE_NULL)
		outputTypeInfo->second.afterValueFn(output, -1);
	output << ";";
	return output.str();
}

string replacePlaceholders(const string& shaderTemplate, const TestParams& params, const glu::GLSLVersion glslVersion)
{
	const auto condition = testConditions.find(params.outputType);
	return tcu::StringTemplate(shaderTemplate).specialize(
	{
		{ "GLSL_VERSION"	, glu::getGLSLVersionDeclaration(glslVersion) },
		{ "TEST_CONSTANTS"	, params.extraFields },
		{ "TEST_CODE"		, generateTestCode(params.outputType, params.inputTypes) },
		{ "TEST_CONDITION"	, (condition != testConditions.end()) ? condition->second : "" }
	});
}

const vector<float>								positions				=
{
	-1.0f, -1.0f,
	 1.0f, -1.0f,
	-1.0f,	1.0f,
	 1.0f,	1.0f
};

const vector<deUint32>							indices					= { 0, 1, 2, 3 };

const int										RENDERTARGET_WIDTH		= 16;
const int										RENDERTARGET_HEIGHT		= 16;

class GLSLVectorConstructorTestCase : public deqp::TestCase
{
public:
	GLSLVectorConstructorTestCase(deqp::Context& context, glu::GLSLVersion glslVersion, const TestParams& params);

	void init(void);
	void deinit(void);
	IterateResult iterate();

private:
	void setupRenderTarget();
	void releaseRenderTarget();

	const glu::GLSLVersion		m_glslVersion;
	const TestParams			m_params;
	glw::GLuint					m_fboId;
	glw::GLuint					m_rboId;

	string						m_vertexShader;
	string						m_fragmentShader;
};

GLSLVectorConstructorTestCase::GLSLVectorConstructorTestCase(deqp::Context& context, glu::GLSLVersion glslVersion, const TestParams& params)
	: TestCase(context, params.name.c_str(), params.description.c_str())
	, m_glslVersion(glslVersion)
	, m_params(params)
	, m_fboId(0)
	, m_rboId(0)
{
	switch(m_params.testType)
	{
	case TestType::VERTEX_SHADER_ERROR:
	case TestType::VERTEX_SHADER:
		m_vertexShader = replacePlaceholders(vertexShaderTemplate, m_params, m_glslVersion);
		m_fragmentShader = replacePlaceholders(defaultFragmentShader, m_params, m_glslVersion);
		break;
	case TestType::FRAGMENT_SHADER_ERROR:
	case TestType::FRAGMENT_SHADER:
		m_vertexShader = replacePlaceholders(defaultVertexShader, m_params, m_glslVersion);
		m_fragmentShader = replacePlaceholders(fragmentShaderTemplate, m_params, m_glslVersion);
		break;
	}
}

void GLSLVectorConstructorTestCase::init(void)
{
	deqp::TestCase::init();
}

void GLSLVectorConstructorTestCase::deinit(void)
{
	deqp::TestCase::deinit();
}

GLSLVectorConstructorTestCase::IterateResult GLSLVectorConstructorTestCase::iterate()
{
	const auto&								renderContext	= m_context.getRenderContext();
	const auto&								gl				= renderContext.getFunctions();
	const auto								textureFormat	= tcu::TextureFormat(tcu::TextureFormat::RGBA,	tcu::TextureFormat::UNORM_INT8);
	const auto								transferFormat	= glu::getTransferFormat(textureFormat);

	setupRenderTarget();

	glu::ShaderProgram program(renderContext, glu::makeVtxFragSources(m_vertexShader, m_fragmentShader));
	if (!program.isOk())
	{
		switch(m_params.testType)
		{
		case TestType::VERTEX_SHADER_ERROR:
		case TestType::FRAGMENT_SHADER_ERROR:
			m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
			return STOP;
		default:
			TCU_FAIL("Shader compilation failed:\nVertex shader:\n" + m_vertexShader + "\nFragment shader:\n" + m_fragmentShader);
		}
	}

	const vector<glu::VertexArrayBinding>	vertexArrays	=
	{
		glu::va::Float("vPosition", 2, positions.size(), 0, positions.data()),
	};

	gl.useProgram(program.getProgram());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glUseProgram failed");

	gl.clear(GL_COLOR_BUFFER_BIT);

	glu::draw(renderContext, program.getProgram(),
			  static_cast<int>(vertexArrays.size()), vertexArrays.data(),
			  glu::pr::TriangleStrip(static_cast<int>(indices.size()), indices.data()));

	const auto								pixelSize		= tcu::getPixelSize(textureFormat);
	vector<deUint8>							fbData			(RENDERTARGET_WIDTH * RENDERTARGET_HEIGHT * pixelSize);

	if (pixelSize < 4)
		gl.pixelStorei(GL_PACK_ALIGNMENT, 1);

	gl.readPixels(0, 0, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT, transferFormat.format, transferFormat.dataType, fbData.data());
	GLU_EXPECT_NO_ERROR(gl.getError(), "glReadPixels");

	tcu::ConstPixelBufferAccess				fbAccess		{ textureFormat, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT, 1, fbData.data() };
	const auto								expectedColor	= tcu::RGBA::green().toVec();
	bool pass = true;
	for(int y = 0; pass && y < RENDERTARGET_HEIGHT; ++y)
		for(int x = 0; x < RENDERTARGET_WIDTH; ++x)
			if (fbAccess.getPixel(x,y) != expectedColor)
			{
				pass = false;
				break;
			}

	releaseRenderTarget();

	const qpTestResult						result			= (pass ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL);
	const char*								desc			= (pass ? "Pass" : "Pixel mismatch; vector initialization failed");

	m_testCtx.setTestResult(result, desc);

	return STOP;
}

void GLSLVectorConstructorTestCase::setupRenderTarget()
{
	const auto&		renderContext	= m_context.getRenderContext();
	const auto&		gl				= renderContext.getFunctions();

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

	glw::GLenum		drawBuffer		= GL_COLOR_ATTACHMENT0;
	gl.drawBuffers(1, &drawBuffer);
	GLU_EXPECT_NO_ERROR(gl.getError(), "DrawBuffers");

	glw::GLfloat	clearColor[4]	= { 0, 0, 0, 0 };
	gl.clearBufferfv(GL_COLOR, 0, clearColor);
	GLU_EXPECT_NO_ERROR(gl.getError(), "ClearBuffers");

	gl.viewport(0, 0, RENDERTARGET_WIDTH, RENDERTARGET_HEIGHT);
	GLU_EXPECT_NO_ERROR(gl.getError(), "Viewport");
}

void GLSLVectorConstructorTestCase::releaseRenderTarget()
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

GLSLVectorConstructorTests::GLSLVectorConstructorTests(Context& context, glu::GLSLVersion glslVersion)
	: deqp::TestCaseGroup(context, "glsl_constructors", "GLSL vector constructor tests")
	, m_glslVersion(glslVersion)
{
}

GLSLVectorConstructorTests::~GLSLVectorConstructorTests()
{
}

void GLSLVectorConstructorTests::init()
{
	for(const auto& params : generateTestParams())
		addChild(new GLSLVectorConstructorTestCase(m_context, m_glslVersion, params));
}

} // deqp
