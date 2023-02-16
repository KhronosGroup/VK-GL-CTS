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
 * \brief Shader struct tests.
 */ /*-------------------------------------------------------------------*/

#include "glcShaderFunctionTests.hpp"
#include "glcShaderRenderCase.hpp"
#include "gluTexture.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

using namespace glu;

namespace deqp
{

typedef void (*SetupUniformsFunc)(const glw::Functions& gl, deUint32 programID, const tcu::Vec4& constCoords);

class ShaderFunctionCase : public ShaderRenderCase
{
public:
	ShaderFunctionCase(Context& context, const char* name, const char* description, bool isVertexCase, bool usesTextures,
					 ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc, const char* vertShaderSource,
					 const char* fragShaderSource);
	~ShaderFunctionCase(void);

	void init(void);
	void deinit(void);

	virtual void setupUniforms(deUint32 programID, const tcu::Vec4& constCoords);

private:
	ShaderFunctionCase(const ShaderFunctionCase&);
	ShaderFunctionCase& operator=(const ShaderFunctionCase&);

	SetupUniformsFunc m_setupUniforms;
	bool			  m_usesTexture;

	glu::Texture2D* m_gradientTexture;
};

ShaderFunctionCase::ShaderFunctionCase(Context& context, const char* name, const char* description, bool isVertexCase,
								   bool usesTextures, ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc,
								   const char* vertShaderSource, const char* fragShaderSource)
	: ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
					   description, isVertexCase, evalFunc)
	, m_setupUniforms(setupUniformsFunc)
	, m_usesTexture(usesTextures)
	, m_gradientTexture(DE_NULL)
{
	m_vertShaderSource = vertShaderSource;
	m_fragShaderSource = fragShaderSource;
}

ShaderFunctionCase::~ShaderFunctionCase(void)
{
}

void ShaderFunctionCase::init(void)
{
	if (m_usesTexture)
	{
		m_gradientTexture = new glu::Texture2D(m_renderCtx, GL_RGBA8, 128, 128);

		m_gradientTexture->getRefTexture().allocLevel(0);
		tcu::fillWithComponentGradients(m_gradientTexture->getRefTexture().getLevel(0), tcu::Vec4(0.0f),
										tcu::Vec4(1.0f));
		m_gradientTexture->upload();

		m_textures.push_back(TextureBinding(
			m_gradientTexture, tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
											tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::LINEAR, tcu::Sampler::LINEAR)));
		DE_ASSERT(m_textures.size() == 1);
	}
	ShaderRenderCase::init();
}

void ShaderFunctionCase::deinit(void)
{
	if (m_usesTexture)
	{
		delete m_gradientTexture;
	}
	ShaderRenderCase::deinit();
}

void ShaderFunctionCase::setupUniforms(deUint32 programID, const tcu::Vec4& constCoords)
{
	ShaderRenderCase::setupUniforms(programID, constCoords);
	if (m_setupUniforms)
		m_setupUniforms(m_renderCtx.getFunctions(), programID, constCoords);
}

static ShaderFunctionCase* createStructCase(Context& context, const char* name, const char* description,
										  glu::GLSLVersion glslVersion, bool isVertexCase, bool usesTextures,
										  ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniforms,
										  const LineStream& shaderSrc)
{
	const std::string versionDecl = glu::getGLSLVersionDeclaration(glslVersion);

	const std::string defaultVertSrc = versionDecl + "\n"
													 "in highp vec4 a_position;\n"
													 "in highp vec4 a_coords;\n"
													 "out mediump vec4 v_coords;\n\n"
													 "void main (void)\n"
													 "{\n"
													 "   v_coords = a_coords;\n"
													 "   gl_Position = a_position;\n"
													 "}\n";
	const std::string defaultFragSrc = versionDecl + "\n"
													 "in mediump vec4 v_color;\n"
													 "layout(location = 0) out mediump vec4 o_color;\n\n"
													 "void main (void)\n"
													 "{\n"
													 "   o_color = v_color;\n"
													 "}\n";

	// Fill in specialization parameters.
	std::map<std::string, std::string> spParams;
	if (isVertexCase)
	{
		spParams["HEADER"] = versionDecl + "\n"
										   "in highp vec4 a_position;\n"
										   "in highp vec4 a_coords;\n"
										   "out mediump vec4 v_color;";
		spParams["COORDS"]	 = "a_coords";
		spParams["DST"]		   = "v_color";
		spParams["ASSIGN_POS"] = "gl_Position = a_position;";
	}
	else
	{
		spParams["HEADER"] = versionDecl + "\n"
										   "#ifdef GL_ES\n"
										   "    precision mediump float;\n"
										   "#endif\n"
										   "\n"
										   "in mediump vec4 v_coords;\n"
										   "layout(location = 0) out mediump vec4 o_color;";
		spParams["COORDS"]	 = "v_coords";
		spParams["DST"]		   = "o_color";
		spParams["ASSIGN_POS"] = "";
	}

	if (isVertexCase)
		return new ShaderFunctionCase(context, name, description, isVertexCase, usesTextures, evalFunc, setupUniforms,
									tcu::StringTemplate(shaderSrc.str()).specialize(spParams).c_str(),
									defaultFragSrc.c_str());
	else
		return new ShaderFunctionCase(context, name, description, isVertexCase, usesTextures, evalFunc, setupUniforms,
									defaultVertSrc.c_str(),
									tcu::StringTemplate(shaderSrc.str()).specialize(spParams).c_str());
}

ShaderFunctionTests::ShaderFunctionTests(Context& context, glu::GLSLVersion glslVersion)
	: TestCaseGroup(context, "function", "Function Tests"), m_glslVersion(glslVersion)
{
}

ShaderFunctionTests::~ShaderFunctionTests(void)
{
}

void ShaderFunctionTests::init(void)
{
#define FUNCTION_CASE(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY)                                      \
	do                                                                                                    \
	{                                                                                                     \
		struct Eval_##NAME                                                                                \
		{                                                                                                 \
			static void eval(ShaderEvalContext& c) EVAL_FUNC_BODY                                         \
		};                                                                                                \
		addChild(createStructCase(m_context, #NAME "_vertex", DESCRIPTION, m_glslVersion, true, false,    \
								  &Eval_##NAME::eval, DE_NULL, SHADER_SRC));                              \
		addChild(createStructCase(m_context, #NAME "_fragment", DESCRIPTION, m_glslVersion, false, false, \
								  &Eval_##NAME::eval, DE_NULL, SHADER_SRC));                              \
	} while (deGetFalse())

	FUNCTION_CASE(local_variable_aliasing, "Function out parameter aliases local variable",
				  LineStream() << "${HEADER}"
							   << ""
							   << "bool out_params_are_distinct(float x, out float y) {"
							   << "    y = 2.;"
							   << "    return x == 1. && y == 2.;"
							   << "}"
							   << ""
							   << "void main (void)"
							   << "{"
							   << "    float x = 1.;"
							   << "    ${DST} = out_params_are_distinct(x, x) ? vec4(0.,1.,0.,1.) : vec4(1.,0.,0.,1.);"
							   << "    ${ASSIGN_POS}"
							   << "}",
				  { c.color.xyz() = tcu::Vec3(0.0f, 1.0f, 0.0f); });

	FUNCTION_CASE(global_variable_aliasing, "Function out parameter aliases global variable",
				  LineStream() << "${HEADER}"
							   << ""
							   << "float x = 1.;"
							   << "bool out_params_are_distinct_from_global(out float y) {"
							   << "    y = 2.;"
							   << "    return x == 1. && y == 2.;"
							   << "}"
							   << ""
							   << "void main (void)"
							   << "{"
							   << "    ${DST} = out_params_are_distinct_from_global(x) ? vec4(0.,1.,0.,1.) : vec4(1.,0.,0.,1.);"
							   << "    ${ASSIGN_POS}"
							   << "}",
				  { c.color.xyz() = tcu::Vec3(0.0f, 1.0f, 0.0f); });
}

} // deqp
