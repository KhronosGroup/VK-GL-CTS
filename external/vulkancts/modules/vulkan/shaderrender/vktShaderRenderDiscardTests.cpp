/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Copyright (c) 2016 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Shader discard statement tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderDiscardTests.hpp"
#include "vktShaderRender.hpp"
#include "tcuStringTemplate.hpp"
#include "gluTexture.hpp"

#include <string>

using tcu::StringTemplate;

namespace vkt
{
namespace sr
{
namespace
{

class SamplerUniformSetup : public UniformSetup
{
public:
						SamplerUniformSetup			(bool useSampler)
							: m_useSampler(useSampler)
						{}

	virtual void		setup						 (ShaderRenderCaseInstance& instance, const tcu::Vec4&) const
						{
							instance.useUniform(0u, UI_ONE);
							instance.useUniform(1u, UI_TWO);
							if (m_useSampler)
								instance.useSampler(2u, 0u); // To the uniform binding location 2 bind the texture 0
						}

private:
	const bool			m_useSampler;
};


class ShaderDiscardCaseInstance : public ShaderRenderCaseInstance
{
public:
						ShaderDiscardCaseInstance	(Context&				context,
													bool					isVertexCase,
													const ShaderEvaluator&	evaluator,
													const UniformSetup&		uniformSetup,
													bool					usesTexture,
													bool					fuzzyCompare);
	virtual				~ShaderDiscardCaseInstance	(void);
};

ShaderDiscardCaseInstance::ShaderDiscardCaseInstance (Context&					context,
													 bool						isVertexCase,
													 const ShaderEvaluator&		evaluator,
													 const UniformSetup&		uniformSetup,
													 bool						usesTexture,
													 bool						fuzzyCompare)
	: ShaderRenderCaseInstance	(context, isVertexCase, evaluator, uniformSetup, DE_NULL, IMAGE_BACKING_MODE_REGULAR, static_cast<deUint32>(GRID_SIZE_DEFAULTS), fuzzyCompare)
{
	if (usesTexture)
	{
		de::SharedPtr<TextureBinding> brickTexture(new TextureBinding(m_context.getTestContext().getArchive(),
																	  "vulkan/data/brick.png",
																	  TextureBinding::TYPE_2D,
																	  tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE,
																					tcu::Sampler::CLAMP_TO_EDGE,
																					tcu::Sampler::CLAMP_TO_EDGE,
																					tcu::Sampler::LINEAR,
																					tcu::Sampler::LINEAR)));
		m_textures.push_back(brickTexture);
	}
}

ShaderDiscardCaseInstance::~ShaderDiscardCaseInstance (void)
{
}

class ShaderDiscardCase : public ShaderRenderCase
{
public:
							ShaderDiscardCase			(tcu::TestContext&		testCtx,
														 const char*			name,
														 const char*			description,
														 const char*			shaderSource,
														 const ShaderEvalFunc	evalFunc,
														 bool					usesTexture,
														 bool					fuzzyCompare,
														 bool					demote);
	virtual TestInstance*	createInstance				(Context& context) const
							{
								DE_ASSERT(m_evaluator != DE_NULL);
								DE_ASSERT(m_uniformSetup != DE_NULL);
								return new ShaderDiscardCaseInstance(context, m_isVertexCase, *m_evaluator, *m_uniformSetup, m_usesTexture, m_fuzzyCompare);
							}

	virtual void				checkSupport					(Context& context) const;

private:
	const bool				m_usesTexture;
	const bool				m_fuzzyCompare;
	const bool				m_demote;
};

ShaderDiscardCase::ShaderDiscardCase (tcu::TestContext&		testCtx,
									  const char*			name,
									  const char*			description,
									  const char*			shaderSource,
									  const ShaderEvalFunc	evalFunc,
									  bool					usesTexture,
									  bool					fuzzyCompare,
									  bool					demote)
	: ShaderRenderCase	(testCtx, name, description, false, evalFunc, new SamplerUniformSetup(usesTexture), DE_NULL)
	, m_usesTexture		(usesTexture)
	, m_fuzzyCompare	(fuzzyCompare)
	, m_demote			(demote)
{
	m_fragShaderSource	= shaderSource;
	m_vertShaderSource	=
		"#version 310 es\n"
		"layout(location=0) in  highp   vec4 a_position;\n"
		"layout(location=1) in  highp   vec4 a_coords;\n"
		"layout(location=2) in  highp   vec4 a_one;\n"
		"layout(location=0) out mediump vec4 v_color;\n"
		"layout(location=1) out mediump vec4 v_coords;\n\n"
		"layout(location=2) out mediump vec4 v_one;\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = a_position;\n"
		"    v_color = vec4(a_coords.xyz, 1.0);\n"
		"    v_coords = a_coords;\n"
		"    v_one = a_one;\n"
		"}\n";
}

void ShaderDiscardCase::checkSupport(Context& context) const
{
	if (m_demote && !context.getShaderDemoteToHelperInvocationFeatures().shaderDemoteToHelperInvocation)
		TCU_THROW(NotSupportedError, "VK_EXT_shader_demote_to_helper_invocation not supported");
}

enum DiscardMode
{
	DISCARDMODE_ALWAYS = 0,
	DISCARDMODE_NEVER,
	DISCARDMODE_UNIFORM,
	DISCARDMODE_DYNAMIC,
	DISCARDMODE_TEXTURE,
	DISCARDMODE_DERIV,

	DISCARDMODE_LAST
};

enum DiscardTemplate
{
	DISCARDTEMPLATE_MAIN_BASIC = 0,
	DISCARDTEMPLATE_FUNCTION_BASIC,
	DISCARDTEMPLATE_MAIN_STATIC_LOOP,
	DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP,
	DISCARDTEMPLATE_FUNCTION_STATIC_LOOP,

	DISCARDTEMPLATE_LAST
};

// Evaluation functions
inline void evalDiscardAlways	(ShaderEvalContext& c) { c.discard(); }
inline void evalDiscardNever	(ShaderEvalContext& c) { c.color.xyz() = c.coords.swizzle(0,1,2); }
inline void evalDiscardDynamic	(ShaderEvalContext& c) { c.color.xyz() = c.coords.swizzle(0,1,2); if (c.coords.x()+c.coords.y() > 0.0f) c.discard(); }

inline void evalDiscardTexture (ShaderEvalContext& c)
{
	c.color.xyz() = c.coords.swizzle(0,1,2);
	if (c.texture2D(0, c.coords.swizzle(0,1) * 0.25f + 0.5f).x() < 0.7f)
		c.discard();
}

static ShaderEvalFunc getEvalFunc (DiscardMode mode)
{
	switch (mode)
	{
		case DISCARDMODE_ALWAYS:	return evalDiscardAlways;
		case DISCARDMODE_NEVER:		return evalDiscardNever;
		case DISCARDMODE_UNIFORM:	return evalDiscardAlways;
		case DISCARDMODE_DYNAMIC:	return evalDiscardDynamic;
		case DISCARDMODE_TEXTURE:	return evalDiscardTexture;
		case DISCARDMODE_DERIV:		return evalDiscardAlways;
		default:
			DE_ASSERT(DE_FALSE);
			return evalDiscardAlways;
	}
}

static const char* getTemplate (DiscardTemplate variant)
{
	#define GLSL_SHADER_TEMPLATE_HEADER \
				"#version 310 es\n"	\
				"#extension GL_EXT_demote_to_helper_invocation : enable\n"	\
				"layout(location = 0) in mediump vec4 v_color;\n"	\
				"layout(location = 1) in mediump vec4 v_coords;\n"	\
				"layout(location = 2) in mediump vec4 a_one;\n"	\
				"layout(location = 0) out mediump vec4 o_color;\n"	\
				"layout(set = 0, binding = 2) uniform sampler2D    ut_brick;\n"	\
				"layout(set = 0, binding = 0) uniform block0 { mediump int  ui_one; };\n\n"

	switch (variant)
	{
		case DISCARDTEMPLATE_MAIN_BASIC:
			return GLSL_SHADER_TEMPLATE_HEADER
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    ${DISCARD};\n"
				   "}\n";

		case DISCARDTEMPLATE_FUNCTION_BASIC:
			return GLSL_SHADER_TEMPLATE_HEADER
				   "void myfunc (void)\n"
				   "{\n"
				   "    ${DISCARD};\n"
				   "}\n\n"
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    myfunc();\n"
				   "}\n";

		case DISCARDTEMPLATE_MAIN_STATIC_LOOP:
			return GLSL_SHADER_TEMPLATE_HEADER
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    for (int i = 0; i < 2; i++)\n"
				   "    {\n"
				   "        if (i > 0) {\n"
				   "            ${DISCARD};\n"
				   "        }\n"
				   "    }\n"
				   "}\n";

		case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:
			return GLSL_SHADER_TEMPLATE_HEADER
				   "layout(set = 0, binding = 1) uniform block1 { mediump int  ui_two; };\n\n"
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    for (int i = 0; i < ui_two; i++)\n"
				   "    {\n"
				   "        if (i > 0) {\n"
				   "            ${DISCARD};\n"
				   "        }\n"
				   "    }\n"
				   "}\n";

		case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:
			return GLSL_SHADER_TEMPLATE_HEADER
				   "void myfunc (void)\n"
				   "{\n"
				   "    for (int i = 0; i < 2; i++)\n"
				   "    {\n"
				   "        if (i > 0) {\n"
				   "            ${DISCARD};\n"
				   "        }\n"
				   "    }\n"
				   "}\n\n"
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    myfunc();\n"
				   "}\n";

		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}

	#undef GLSL_SHADER_TEMPLATE_HEADER
}

static const char* getTemplateName (DiscardTemplate variant)
{
	switch (variant)
	{
		case DISCARDTEMPLATE_MAIN_BASIC:			return "basic";
		case DISCARDTEMPLATE_FUNCTION_BASIC:		return "function";
		case DISCARDTEMPLATE_MAIN_STATIC_LOOP:		return "static_loop";
		case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:		return "dynamic_loop";
		case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:	return "function_static_loop";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

static const char* getModeName (DiscardMode mode)
{
	switch (mode)
	{
		case DISCARDMODE_ALWAYS:	return "always";
		case DISCARDMODE_NEVER:		return "never";
		case DISCARDMODE_UNIFORM:	return "uniform";
		case DISCARDMODE_DYNAMIC:	return "dynamic";
		case DISCARDMODE_TEXTURE:	return "texture";
		case DISCARDMODE_DERIV:		return "deriv";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

static const char* getTemplateDesc (DiscardTemplate variant)
{
	switch (variant)
	{
		case DISCARDTEMPLATE_MAIN_BASIC:			return "main";
		case DISCARDTEMPLATE_FUNCTION_BASIC:		return "function";
		case DISCARDTEMPLATE_MAIN_STATIC_LOOP:		return "static loop";
		case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:		return "dynamic loop";
		case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:	return "static loop in function";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

static const char* getModeDesc (DiscardMode mode)
{
	switch (mode)
	{
		case DISCARDMODE_ALWAYS:	return "Always discard";
		case DISCARDMODE_NEVER:		return "Never discard";
		case DISCARDMODE_UNIFORM:	return "Discard based on uniform value";
		case DISCARDMODE_DYNAMIC:	return "Discard based on varying values";
		case DISCARDMODE_TEXTURE:	return "Discard based on texture value";
		case DISCARDMODE_DERIV:		return "Discard based on derivatives after an earlier discard";
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

de::MovePtr<ShaderDiscardCase> makeDiscardCase (tcu::TestContext& testCtx, DiscardTemplate tmpl, DiscardMode mode, const std::string& discardStr)
{
	StringTemplate shaderTemplate(getTemplate(tmpl));

	std::map<std::string, std::string> params;

	switch (mode)
	{
		case DISCARDMODE_ALWAYS:	params["DISCARD"] = discardStr;																break;
		case DISCARDMODE_NEVER:		params["DISCARD"] = "if (false) " + discardStr;												break;
		case DISCARDMODE_UNIFORM:	params["DISCARD"] = "if (ui_one > 0) " + discardStr;										break;
		case DISCARDMODE_DYNAMIC:	params["DISCARD"] = "if (v_coords.x+v_coords.y > 0.0) " + discardStr;						break;
		case DISCARDMODE_TEXTURE:	params["DISCARD"] = "if (texture(ut_brick, v_coords.xy*0.25+0.5).x < 0.7) " + discardStr;	break;
		case DISCARDMODE_DERIV:		params["DISCARD"] =
										// First demote pixels where fragCoord.xy LSBs are not both zero, leaving only one
										// non-helper pixel per quad. Then compute derivatives of "one+fragCoord" and check they
										// are 0 or 1 as appropriate. Also check that helperInvocationEXT varies in the quad and
										// is false on non-helper pixels. Demote the pixel if it gets the right values, so the final
										// image should be entirely the clear color. If we don't get the right values, output red.
										// This test case would not work for discard, because derivatives become undefined.
										"  ivec2 f = ivec2(gl_FragCoord.xy);\n"
										"  int lsb = (f.x | f.y)&1;\n"
										"  if (lsb != 0) demote;\n"
										"  bool isHelper = helperInvocationEXT();\n"
										"  highp vec2 dx = dFdx(a_one.xy + gl_FragCoord.xy);\n"
										"  highp vec2 dy = dFdy(a_one.xy + gl_FragCoord.xy);\n"
										"  highp float dh = dFdx(float(isHelper));\n"
										"  bool valid = abs(dx.x-1.0) < 0.01 && dx.y == 0.0 && dy.x == 0.0 && abs(dy.y-1.0) < 0.01 && abs(dh-1.0) < 0.1 && !isHelper;\n"
										"  if (valid) demote;\n"
										"  o_color = vec4(1,0,0,1);\n";
										break;
		default:
			DE_ASSERT(DE_FALSE);
			break;
	}

	std::string name		= std::string(getTemplateName(tmpl)) + "_" + getModeName(mode);
	std::string description	= std::string(getModeDesc(mode)) + " in " + getTemplateDesc(tmpl);

	return de::MovePtr<ShaderDiscardCase>(new ShaderDiscardCase(testCtx, name.c_str(),
																description.c_str(),
																shaderTemplate.specialize(params).c_str(),
																getEvalFunc(mode),
																mode == DISCARDMODE_TEXTURE,		// usesTexture
																mode != DISCARDMODE_DERIV,			// fuzzyCompare
																discardStr == "demote"));			// demote
}

class ShaderDiscardTests : public tcu::TestCaseGroup
{
public:
							ShaderDiscardTests		(tcu::TestContext& textCtx, const char *groupName);
	virtual					~ShaderDiscardTests		(void);

	virtual void			init					(void);

private:
							ShaderDiscardTests		(const ShaderDiscardTests&);		// not allowed!
	ShaderDiscardTests&		operator=				(const ShaderDiscardTests&);		// not allowed!
	const std::string		m_groupName;
};

ShaderDiscardTests::ShaderDiscardTests (tcu::TestContext& testCtx, const char *groupName)
	: TestCaseGroup(testCtx, groupName, "Discard statement tests")
	, m_groupName(groupName)
{
}

ShaderDiscardTests::~ShaderDiscardTests (void)
{
}

void ShaderDiscardTests::init (void)
{
	for (int tmpl = 0; tmpl < DISCARDTEMPLATE_LAST; tmpl++)
	{
		for (int mode = 0; mode < DISCARDMODE_LAST; mode++)
		{
			if (mode == DISCARDMODE_DERIV && m_groupName == "discard")
				continue;
			addChild(makeDiscardCase(m_testCtx, (DiscardTemplate)tmpl, (DiscardMode)mode, m_groupName).release());
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createDiscardTests (tcu::TestContext& testCtx)
{
	return new ShaderDiscardTests(testCtx, "discard");
}

tcu::TestCaseGroup* createDemoteTests (tcu::TestContext& testCtx)
{
	return new ShaderDiscardTests(testCtx, "demote");
}

} // sr
} // vkt
