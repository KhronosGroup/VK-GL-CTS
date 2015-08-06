#include "vktShaderRenderCaseTests.hpp"

#include "vktShaderRenderCase.hpp"


#include "deUniquePtr.hpp"

namespace vkt
{
namespace shaderrendercase
{

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> shaderRenderCaseTests (new tcu::TestCaseGroup(testCtx, "shaderRenderCase", "ShaderRenderCase Tests"));

	shaderRenderCaseTests->addChild(new ShaderRenderCase(testCtx, "test", "test", false, evalCoordsPassthrough));

	return shaderRenderCaseTests.release();
}

} // shaderrendercase
} // vkt
