#include "vktShaderRenderCaseTests.hpp"

#include "vktShaderRenderCase.hpp"


#include "deUniquePtr.hpp"

namespace vkt
{
namespace shaderrendercase
{

inline void eval_DEBUG      (ShaderEvalContext& c) { c.color.x() = 100; }

class DummyTestRenderCase : public ShaderRenderCase<ShaderRenderCaseInstance>
{
public:
	DummyTestRenderCase	(tcu::TestContext& testCtx,
						const std::string& name,
						const std::string& description,
						bool isVertexCase,
						ShaderEvalFunc evalFunc)
		: ShaderRenderCase(testCtx, name, description, isVertexCase, evalFunc)
	{}
};

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> shaderRenderCaseTests (new tcu::TestCaseGroup(testCtx, "shaderRenderCase", "ShaderRenderCase Tests"));

	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testVertex", "testVertex", true, eval_DEBUG));
	shaderRenderCaseTests->addChild(new DummyTestRenderCase(testCtx, "testFragment", "testFragment", false, eval_DEBUG));

	return shaderRenderCaseTests.release();
}

} // shaderrendercase
} // vkt
