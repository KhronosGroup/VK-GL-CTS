/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Shader discard statement tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderDiscardTests.hpp"
#include "tcuStringTemplate.hpp"
#include "gluTexture.hpp"

#include "vktTexture.hpp"

#include <map>
#include <sstream>
#include <string>

using tcu::StringTemplate;

using std::map;
using std::string;
using std::ostringstream;

namespace vkt
{
namespace shaderrendercase
{

class SamplerUniformSetup : public UniformSetup
{
public:
						SamplerUniformSetup			(bool useSampler)
							: m_useSampler(useSampler)
						{}

	virtual void		setup						 (ShaderRenderCaseInstance& instance, const tcu::Vec4& constCoords)
						{
							DE_UNREF(constCoords);
							instance.useUniform(0u, UI_ONE);
							instance.useUniform(1u, UI_TWO);
							if (m_useSampler)
								instance.useSampler2D(2u, 0u); // To the uniform binding location 2 bind the texture 0
						}

private:
	bool 				m_useSampler;
};


class ShaderDiscardCaseInstance : public ShaderRenderCaseInstance
{
public:
						ShaderDiscardCaseInstance	(Context&			context,
													bool 				isVertexCase,
													ShaderEvaluator&	evaulator,
													UniformSetup&		uniformSetup,
													bool				usesTexture);
	virtual				~ShaderDiscardCaseInstance	(void);

private:
	bool				m_usesTexture;
	Texture2D*			m_brickTexture;
};

ShaderDiscardCaseInstance::ShaderDiscardCaseInstance (Context&			context,
													 bool				isVertexCase,
													 ShaderEvaluator&	evaulator,
													 UniformSetup&		uniformSetup,
													 bool				usesTexture)
	: ShaderRenderCaseInstance	(context, isVertexCase, evaulator, uniformSetup, DE_NULL)
	, m_usesTexture				(usesTexture)
{
	if (m_usesTexture)
	{
		m_brickTexture = Texture2D::create(m_context, m_context.getTestContext().getArchive(), "vulkan/data/brick.png");
		m_textures.push_back(TextureBinding(m_brickTexture, tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE,
																		tcu::Sampler::CLAMP_TO_EDGE,
																		tcu::Sampler::CLAMP_TO_EDGE,
																		tcu::Sampler::LINEAR,
																		tcu::Sampler::LINEAR)));
	}
}

ShaderDiscardCaseInstance::~ShaderDiscardCaseInstance (void)
{
	if (m_usesTexture)
	{
		delete m_brickTexture;
		m_brickTexture = DE_NULL;
	}
}

class ShaderDiscardCase : public ShaderRenderCase
{
public:
						ShaderDiscardCase			(tcu::TestContext&	testCtx,
													 const char*		name,
													 const char*		description,
													 const char*		shaderSource,
													 ShaderEvalFunc		evalFunc,
													 bool				usesTexture);
	virtual TestInstance*	createInstance			(Context& context) const
							{
								DE_ASSERT(m_evaluator != DE_NULL);
								DE_ASSERT(m_uniformSetup != DE_NULL);
								return new ShaderDiscardCaseInstance(context, m_isVertexCase, *m_evaluator, *m_uniformSetup, m_usesTexture);
							}

private:
	bool				m_usesTexture;
};

ShaderDiscardCase::ShaderDiscardCase (tcu::TestContext&	testCtx,
									  const char*		name,
									  const char*		description,
									  const char*		shaderSource,
									  ShaderEvalFunc	evalFunc,
									  bool				usesTexture)
	: ShaderRenderCase	(testCtx, name, description, false, evalFunc, new SamplerUniformSetup(usesTexture), DE_NULL)
	, m_usesTexture		(usesTexture)
{
	m_fragShaderSource	= shaderSource;
	m_vertShaderSource	=
		"#version 140\n"
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"#extension GL_ARB_shading_language_420pack : enable\n"
		"layout(location=0) in  highp   vec4 a_position;\n"
		"layout(location=1) in  highp   vec4 a_coords;\n"
		"layout(location=0) out mediump vec4 v_color;\n"
		"layout(location=1) out mediump vec4 v_coords;\n\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = a_position;\n"
		"    v_color = vec4(a_coords.xyz, 1.0);\n"
		"    v_coords = a_coords;\n"
		"}\n";
}


ShaderDiscardTests::ShaderDiscardTests (tcu::TestContext& testCtx)
	: TestCaseGroup(testCtx, "discard", "Discard statement tests")
{
}

ShaderDiscardTests::~ShaderDiscardTests (void)
{
}

enum DiscardMode
{
	DISCARDMODE_ALWAYS = 0,
	DISCARDMODE_NEVER,
	DISCARDMODE_UNIFORM,
	DISCARDMODE_DYNAMIC,
	DISCARDMODE_TEXTURE,

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
		default:
			DE_ASSERT(DE_FALSE);
			return evalDiscardAlways;
	}
}

static const char* getTemplate (DiscardTemplate variant)
{
	#define HEADER \
				"#version 140\n"	\
				"#extension GL_ARB_separate_shader_objects : enable\n"	\
				"#extension GL_ARB_shading_language_420pack : enable\n"	 \
				"layout(location = 0) in mediump vec4 v_color;\n"	\
				"layout(location = 1) in mediump vec4 v_coords;\n"	\
				"layout(location = 0) out mediump vec4 o_color;\n"	\
				"layout(set = 0, binding = 2) uniform sampler2D    ut_brick;\n"	\
				"layout(set = 0, binding = 0) uniform block0 { mediump int  ui_one; };\n\n"

	switch (variant)
	{
		case DISCARDTEMPLATE_MAIN_BASIC:
			return HEADER
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    ${DISCARD};\n"
				   "}\n";

		case DISCARDTEMPLATE_FUNCTION_BASIC:
			return HEADER
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
			return HEADER
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    for (int i = 0; i < 2; i++)\n"
				   "    {\n"
				   "        if (i > 0)\n"
				   "            ${DISCARD};\n"
				   "    }\n"
				   "}\n";

		case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:
			return HEADER
				   "layout(set = 0, binding = 1) uniform block1 { mediump int  ui_two; };\n\n"
				   "void main (void)\n"
				   "{\n"
				   "    o_color = v_color;\n"
				   "    for (int i = 0; i < ui_two; i++)\n"
				   "    {\n"
				   "        if (i > 0)\n"
				   "            ${DISCARD};\n"
				   "    }\n"
				   "}\n";

		case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:
			return HEADER
				   "void myfunc (void)\n"
				   "{\n"
				   "    for (int i = 0; i < 2; i++)\n"
				   "    {\n"
				   "        if (i > 0)\n"
				   "            ${DISCARD};\n"
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

	#undef HEADER
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
		default:
			DE_ASSERT(DE_FALSE);
			return DE_NULL;
	}
}

ShaderDiscardCase* makeDiscardCase (tcu::TestContext& testCtx, DiscardTemplate tmpl, DiscardMode mode)
{
	StringTemplate shaderTemplate(getTemplate(tmpl));

	map<string, string> params;

	switch (mode)
	{
		case DISCARDMODE_ALWAYS:	params["DISCARD"] = "discard";										break;
		case DISCARDMODE_NEVER:		params["DISCARD"] = "if (false) discard";							break;
		case DISCARDMODE_UNIFORM:	params["DISCARD"] = "if (ui_one > 0) discard";						break;
		case DISCARDMODE_DYNAMIC:	params["DISCARD"] = "if (v_coords.x+v_coords.y > 0.0) discard";		break;
		case DISCARDMODE_TEXTURE:	params["DISCARD"] = "if (texture(ut_brick, v_coords.xy*0.25+0.5).x < 0.7) discard";	break;
		default:
			DE_ASSERT(DE_FALSE);
			break;
	}

	string name			= string(getTemplateName(tmpl)) + "_" + getModeName(mode);
	string description	= string(getModeDesc(mode)) + " in " + getTemplateDesc(tmpl);

	return new ShaderDiscardCase(testCtx, name.c_str(), description.c_str(), shaderTemplate.specialize(params).c_str(), getEvalFunc(mode), mode == DISCARDMODE_TEXTURE);
}

void ShaderDiscardTests::init (void)
{
	for (int tmpl = 0; tmpl < DISCARDTEMPLATE_LAST; tmpl++)
		for (int mode = 0; mode < DISCARDMODE_LAST; mode++)
			addChild(makeDiscardCase(m_testCtx, (DiscardTemplate)tmpl, (DiscardMode)mode));
}

} // shaderrendercase
} // vkt
