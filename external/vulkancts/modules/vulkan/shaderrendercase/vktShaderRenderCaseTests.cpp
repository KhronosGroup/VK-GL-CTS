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

#include "deUniquePtr.hpp"

namespace vkt
{
namespace shaderrendercase
{

inline void eval_DEBUG      (ShaderEvalContext& c) { c.color = tcu::Vec4(1, 0, 1, 1); }

void empty_uniform (ShaderRenderCaseInstance& /* instance */) {}

struct  test_struct {
	tcu::Vec4 a;
	tcu::Vec4 b;
	tcu::Vec4 c;
	tcu::Vec4 d;
};



void dummy_uniforms (ShaderRenderCaseInstance& instance)
{
	instance.addUniform(0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f);
	instance.addUniform(1u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.5f);
	instance.addUniform(2u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, tcu::Vec4(1, 0.5f, 1.0f, 0.5f));

	test_struct data =
	{
		tcu::Vec4(0.1f),
		tcu::Vec4(0.2f),
		tcu::Vec4(0.3f),
		tcu::Vec4(0.4f),
	};

	instance.addUniform<test_struct>(3u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, data);
}

void dummy_attributes (ShaderRenderCaseInstance& instance, deUint32 numVertices)
{
	std::vector<float> data;
	data.resize(numVertices);
	for(deUint32 i = 0; i < numVertices; i++)
		data[i] = 1.0;

	instance.addAttribute(4u, vk::VK_FORMAT_R32_SFLOAT, sizeof(float), numVertices, &data[0]);
}


class DummyShaderRenderCaseInstance;

class DummyTestRenderCase : public ShaderRenderCase<ShaderRenderCaseInstance>
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

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> shaderRenderCaseTests (new tcu::TestCaseGroup(testCtx, "shaderRenderCase", "ShaderRenderCase Tests"));

	std::string base_vertex = "#version 140\n"
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"#extension GL_ARB_shading_language_420pack : enable\n"

        "layout(location = 0) in highp vec4 a_position;\n"
        "layout(location = 1) in highp vec4 a_coords;\n"
        "layout(location = 2) in highp vec4 a_unitCoords;\n"
        "layout(location = 3) in mediump float a_one;\n"
		"layout(location = 4) in mediump float a_in1;\n"

		"layout (set=0, binding=0) uniform buf {\n"
		"	float item;\n"
		"};\n"

		"layout (set=0, binding=1) uniform buf2 {\n"
		"	float item2;\n"
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

		"out mediump vec4 v_color;\n"
        "void main (void) { gl_Position = a_position; v_color = vec4(a_coords.xyz, f_1.a + f_2.a + f_3[0].x + f_3[1].x); }\n";

	std::string base_fragment = "#version 300 es\n"
        "layout(location = 0) out lowp vec4 o_color;\n"
        "in mediump vec4 v_color;\n"
        "void main (void) { o_color = v_color; }\n";

	std::string debug_fragment = "#version 140 \n"
		"#extension GL_ARB_separate_shader_objects : enable\n"
		"#extension GL_ARB_shading_language_420pack : enable\n"

        "layout(location = 0) out lowp vec4 o_color;\n"

		"layout (set=0, binding=2) uniform buf {\n"
		"	float item[4];\n"
		"};\n"

        "in mediump vec4 v_color;\n"
        "void main (void) { o_color = vec4(1,0,item[0],1); }\n";


	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testVertex", "testVertex", true, evalCoordsPassthrough, base_vertex, base_fragment));
	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testFragment", "testFragment", false, eval_DEBUG, base_vertex, debug_fragment));

	return shaderRenderCaseTests.release();
}

} // shaderrendercase
} // vkt
