/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Imagination Technologies Ltd.
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
 * \brief Robust Buffer Access Tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessTests.hpp"
#include "vktRobustnessExtsTests.hpp"
#include "vktRobustnessBufferAccessTests.hpp"
#include "vktRobustnessVertexAccessTests.hpp"
#include "vktRobustnessIndexAccessTests.hpp"
#include "vktRobustBufferAccessWithVariablePointersTests.hpp"
#include "vktNonRobustBufferAccessTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktRobustness1VertexAccessTests.hpp"

namespace vkt
{
namespace robustness
{

namespace
{

class IsNodeNamed
{
public:
	IsNodeNamed(const std::string& name)
		: checkName(name)
	{}
	bool operator()(tcu::TestNode* node)
	{
		return checkName == std::string(node->getName());
	}
private:
	const std::string checkName;
};

}

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	de::MovePtr<tcu::TestCaseGroup> robustnessTests(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

	robustnessTests->addChild(createBufferAccessTests(testCtx));
	robustnessTests->addChild(createVertexAccessTests(testCtx));
	robustnessTests->addChild(createIndexAccessTests(testCtx));

	std::vector<tcu::TestNode*> children;
	robustnessTests->getChildren(children);
	std::vector<tcu::TestNode*>::iterator buffer_access = std::find_if(children.begin(), children.end(), IsNodeNamed("buffer_access"));
	if (buffer_access != children.end())
	{
		(*buffer_access)->addChild(createBufferAccessWithVariablePointersTests(testCtx));
	}
	else
	{
		de::MovePtr<tcu::TestCaseGroup> bufferAccess(new tcu::TestCaseGroup(testCtx, "buffer_access", ""));
		bufferAccess->addChild(createBufferAccessWithVariablePointersTests(testCtx));
		robustnessTests->addChild(bufferAccess.release());
	}

	robustnessTests->addChild(createRobustness2Tests(testCtx));
	robustnessTests->addChild(createImageRobustnessTests(testCtx));
#ifndef CTS_USES_VULKANSC
	robustnessTests->addChild(createPipelineRobustnessTests(testCtx));
#endif
	robustnessTests->addChild(createNonRobustBufferAccessTests(testCtx));

#ifndef CTS_USES_VULKANSC
	robustnessTests->addChild(createPipelineRobustnessBufferAccessTests(testCtx));
	robustnessTests->addChild(createCmdBindIndexBuffer2Tests(testCtx));
#endif
	robustnessTests->addChild(createRobustness1VertexAccessTests(testCtx));

	return robustnessTests.release();
}

} // robustness
} // vkt
