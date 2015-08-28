#include "vktShaderRenderCaseTests.hpp"

#include "vktShaderRenderCase.hpp"


#include "deUniquePtr.hpp"

namespace vkt
{
namespace shaderrendercase
{

inline void eval_DEBUG      (ShaderEvalContext& c) { c.color = tcu::Vec4(1, 0, 1, 1); }

void empty_uniform (ShaderRenderCaseInstance& instance) {}


void dummy_uniforms (ShaderRenderCaseInstance& instance)
{
	instance.addUniform(0u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f);
	instance.addUniform(1u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0.5f);
	instance.addUniform(2u, vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, tcu::Vec4(1, 0.5f, 1.0f, 0.5f));
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
		: ShaderRenderCase(testCtx, name, description, isVertexCase, evalFunc, dummy_uniforms)
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

		"layout (set=0, binding=0) uniform buf {\n"
		"	float item;\n"
		"};\n"

		"layout (set=0, binding=1) uniform buf2 {\n"
		"	float item2;\n"
		"};\n"

		"layout (set=0, binding=2) uniform buf3 {\n"
		"	vec4 item3;\n"
		"};\n"

		"out mediump vec4 v_color;\n"
        "void main (void) { gl_Position = a_position; v_color = vec4(a_coords.xyz, item3.x); }\n";

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
