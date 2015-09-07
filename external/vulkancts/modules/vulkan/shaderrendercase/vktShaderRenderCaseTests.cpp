/*------------------------------------------------------------------------
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
 * \brief Vulkan shader render test cases
 *//*--------------------------------------------------------------------*/

#include "vktShaderRenderCaseTests.hpp"

#include "vktShaderRenderCase.hpp"
#include "vktTexture.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace shaderrendercase
{

inline void eval_DEBUG      (ShaderEvalContext& c) { c.color = tcu::Vec4(1, 0, 1, 1); }
inline void eval_DEBUG_TEX  (ShaderEvalContext& c) { c.color.xyz() = c.texture2D(0, c.coords.swizzle(0, 1)).swizzle(0,1,2); }

void empty_uniform (ShaderRenderCaseInstance& /* instance */) {}

struct  test_struct {
	tcu::Vec4 a;
	tcu::Vec4 b;
	tcu::Vec4 c;
	tcu::Vec4 d;
};



void dummy_uniforms (ShaderRenderCaseInstance& instance)
{
	instance.useUniform(0u, UI_ZERO);
	instance.useUniform(1u, UI_ONE);
	//instance.addUniform(1u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f);
	//instance.addUniform(0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.5f);
	instance.addUniform(2u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, tcu::Vec4(1, 0.5f, 1.0f, 0.5f));

	test_struct data =
	{
		tcu::Vec4(0.1f),
		tcu::Vec4(0.2f),
		tcu::Vec4(0.3f),
		tcu::Vec4(0.4f),
	};

	instance.addUniform<test_struct>(3u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, data);
	instance.useSampler2D(4u, 0u);
}

void dummy_attributes (ShaderRenderCaseInstance& instance, deUint32 numVertices)
{
	std::vector<float> data;
	data.resize(numVertices);
	for(deUint32 i = 0; i < numVertices; i++)
		data[i] = 1.0;

	instance.addAttribute(4u, vk::VK_FORMAT_R32_SFLOAT, sizeof(float), numVertices, &data[0]);
}


class DummyShaderRenderCaseInstance : public ShaderRenderCaseInstance
{
public:
					DummyShaderRenderCaseInstance	(Context& context,
													bool isVertexCase,
													ShaderEvaluator& evaluator,
													UniformSetupFunc uniformFunc,
													AttributeSetupFunc attribFunc)
						: ShaderRenderCaseInstance(context, isVertexCase, evaluator, uniformFunc, attribFunc)
					{}

	virtual			~DummyShaderRenderCaseInstance	(void)
					{
						delete m_brickTexture;
						m_brickTexture = DE_NULL;
					}

protected:
	virtual void	setup							(void)
					{
						m_brickTexture = Texture2D::create(m_context, m_context.getTestContext().getArchive(), "data/brick.png");
						m_textures.push_back(TextureBinding(m_brickTexture, tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE,
																						tcu::Sampler::CLAMP_TO_EDGE,
																						tcu::Sampler::CLAMP_TO_EDGE,
																						tcu::Sampler::LINEAR,
																						tcu::Sampler::LINEAR)));
					}

	Texture2D*		m_brickTexture;
};



class DummyTestRenderCase : public ShaderRenderCase<DummyShaderRenderCaseInstance>
{
public:
	DummyTestRenderCase	(tcu::TestContext& testCtx,
						const std::string& name,
						const std::string& description,
						bool isVertexCase,
						ShaderEvalFunc evalFunc,
						std::string vertexShader,
						std::string fragmentShader)
		: ShaderRenderCase(testCtx, name, description, isVertexCase, evalFunc, dummy_uniforms, dummy_attributes)
	{
		m_vertShaderSource = vertexShader;
		m_fragShaderSource = fragmentShader;
	}
};

static tcu::TestCaseGroup* dummyTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> dummyTests (new tcu::TestCaseGroup(testCtx, "dummy", "Dummy ShaderRenderCase based Tests"));

	std::string base_vertex = "#version 140\n"
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"#extension GL_ARB_shading_language_420pack : enable\n"

        "layout(location = 0) in highp vec4 a_position;\n"
        "layout(location = 1) in highp vec4 a_coords;\n"
        "layout(location = 2) in highp vec4 a_unitCoords;\n"
        "layout(location = 3) in mediump float a_one;\n"
		"layout(location = 4) in mediump float a_in1;\n"

		"layout (set=0, binding=0) uniform buf {\n"
		"	bool item;\n"
		"};\n"

		"layout (set=0, binding=1) uniform buf2 {\n"
		"	int item2;\n"
		"};\n"

		"layout (set=0, binding=2) uniform buf3 {\n"
		"	vec4 item3;\n"
		"};\n"

		"struct FF { highp float a, b; };\n"
		"layout (set=0, binding=3) uniform buf4 {\n"
		"	FF f_1;\n"
		"	FF f_2;\n"
		"	highp vec2 f_3[2];\n"
		"};\n"

		"layout(location=0) out mediump vec4 v_color;\n"
		"layout(location=1) out mediump vec4 v_coords;\n"
        "void main (void) { \n"
		"	gl_Position = a_position;\n"
		"	v_coords = a_coords;\n"
		"	v_color = vec4(a_coords.xyz, f_1.a + f_2.a + f_3[0].x + f_3[1].x - (item ? item2 : 0));\n"
		"}\n";

	std::string base_fragment = "#version 300 es\n"
        "layout(location = 0) out lowp vec4 o_color;\n"
        "in mediump vec4 v_color;\n"
        "void main (void) { o_color = v_color; }\n";

	std::string debug_fragment = "#version 140 \n"
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"#extension GL_ARB_shading_language_420pack : enable\n"

		"layout(location=0) in vec4 v_color;\n"
		"layout(location=1) in vec4 v_coords;\n"
        "layout(location = 0) out lowp vec4 o_color;\n"

		"layout (set=0, binding=3) uniform buf {\n"
		"	float item[4];\n"
		"};\n"

		"layout (set=0, binding=4) uniform sampler2D tex;\n"

        "void main (void) { o_color = texture(tex, v_coords.xy); }\n";


	dummyTests->addChild(new DummyTestRenderCase(testCtx, "testVertex", "testVertex", true, evalCoordsPassthrough, base_vertex, base_fragment));
	dummyTests->addChild(new DummyTestRenderCase(testCtx, "testFragment", "testFragment", false, eval_DEBUG_TEX, base_vertex, debug_fragment));

	return dummyTests.release();
}

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> shaderRenderCaseTests (new tcu::TestCaseGroup(testCtx, "shaderRenderCase", "ShaderRenderCase Tests"));

	shaderRenderCaseTests->addChild(dummyTests(testCtx));

	return shaderRenderCaseTests.release();
}

} // shaderrendercase
} // vkt
