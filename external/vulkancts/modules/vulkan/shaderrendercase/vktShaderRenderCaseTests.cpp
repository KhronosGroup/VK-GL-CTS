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
						ShaderEvalFunc evalFunc)
		: ShaderRenderCase(testCtx, name, description, isVertexCase, evalFunc)
	{
		m_vertShaderSource = "#version 300 es\n"
        "layout(location = 0) in highp vec4 a_position;\n"
        "void main (void) { gl_Position = a_position; }\n";
		m_fragShaderSource = "#version 300 es\n"
        "layout(location = 0) out lowp vec4 o_color;\n"
        "void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n";
	}
};

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> shaderRenderCaseTests (new tcu::TestCaseGroup(testCtx, "shaderRenderCase", "ShaderRenderCase Tests"));

	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testVertex", "testVertex", true, evalCoordsPassthrough));
	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testFragment", "testFragment", false, eval_DEBUG));

	return shaderRenderCaseTests.release();
}

} // shaderrendercase
} // vkt
