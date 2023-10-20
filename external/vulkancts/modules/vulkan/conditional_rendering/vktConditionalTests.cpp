/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Conditional Rendering Tests
 *//*--------------------------------------------------------------------*/

#include "vktConditionalTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktConditionalDrawTests.hpp"
#include "vktConditionalDispatchTests.hpp"
#include "vktConditionalClearAttachmentTests.hpp"
#include "vktConditionalDrawAndClearTests.hpp"
#include "vktConditionalIgnoreTests.hpp"

namespace vkt
{
namespace conditional
{

namespace
{

void createChildren (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx		= group->getTestContext();

	group->addChild(new ConditionalDrawTests(testCtx));
	group->addChild(new ConditionalDispatchTests(testCtx));
	group->addChild(new ConditionalClearAttachmentTests(testCtx));
	group->addChild(new ConditionalRenderingDrawAndClearTests(testCtx));
	group->addChild(new ConditionalIgnoreTests(testCtx));
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "Conditional Rendering Tests", createChildren);
}

} // conditional
} // vkt
