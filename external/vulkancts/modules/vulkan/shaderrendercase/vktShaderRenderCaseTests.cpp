#include "vktShaderRenderCaseTests.hpp"

#include "vktShaderRenderCase.hpp"


#include "deUniquePtr.hpp"

namespace vkt
{
namespace shaderrendercase
{

inline void eval_DEBUG      (ShaderEvalContext& c) { c.color = tcu::Vec4(1, 0, 1, 1); }

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
		: ShaderRenderCase(testCtx, name, description, isVertexCase, evalFunc)
	{
		m_vertShaderSource = vertexShader;
		m_fragShaderSource = fragmentShader;
	}
};

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> shaderRenderCaseTests (new tcu::TestCaseGroup(testCtx, "shaderRenderCase", "ShaderRenderCase Tests"));

	std::string base_vertex = "#version 300 es\n"
        "layout(location = 0) in highp vec4 a_position;\n"
        "layout(location = 1) in highp vec4 a_coords;\n"
		"out mediump vec4 v_color;\n"
        "void main (void) { gl_Position = a_position; v_color = vec4(a_coords.xyz, 1.0); }\n";

	std::string base_fragment = "#version 300 es\n"
        "layout(location = 0) out lowp vec4 o_color;\n"
        "in mediump vec4 v_color;\n"
        "void main (void) { o_color = v_color; }\n";

	std::string debug_fragment = "#version 300 es\n"
        "layout(location = 0) out lowp vec4 o_color;\n"
        "in mediump vec4 v_color;\n"
        "void main (void) { o_color = vec4(1,0,1,1); }\n";


	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testVertex", "testVertex", true, evalCoordsPassthrough, base_vertex, base_fragment));
	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testFragment", "testFragment", false, eval_DEBUG, base_vertex, debug_fragment));

	return shaderRenderCaseTests.release();
}

} // shaderrendercase
} // vkt
