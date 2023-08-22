/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Shader Object Tests
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "vktShaderObjectApiTests.hpp"
#include "vktShaderObjectCreateTests.hpp"
#include "vktShaderObjectLinkTests.hpp"
#include "vktShaderObjectBinaryTests.hpp"
#include "vktShaderObjectPipelineInteractionTests.hpp"
#include "vktShaderObjectBindingTests.hpp"
#include "vktShaderObjectPerformanceTests.hpp"
#include "vktShaderObjectRenderingTests.hpp"
#include "vktShaderObjectMiscTests.hpp"

namespace vkt
{
namespace ShaderObject
{

namespace
{
using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;
}

tcu::TestCaseGroup*	createTests	(tcu::TestContext& testCtx, const std::string& name)
{
	GroupPtr mainGroup	(new tcu::TestCaseGroup(testCtx, name.c_str(), "Shader Object Tests"));

	mainGroup->addChild(createShaderObjectApiTests(testCtx));
	mainGroup->addChild(createShaderObjectCreateTests(testCtx));
	mainGroup->addChild(createShaderObjectLinkTests(testCtx));
	mainGroup->addChild(createShaderObjectBinaryTests(testCtx));
	mainGroup->addChild(createShaderObjectPipelineInteractionTests(testCtx));
	mainGroup->addChild(createShaderObjectBindingTests(testCtx));
	mainGroup->addChild(createShaderObjectPerformanceTests(testCtx));
	mainGroup->addChild(createShaderObjectRenderingTests(testCtx));
	mainGroup->addChild(createShaderObjectMiscTests(testCtx));

	return mainGroup.release();
}

} // ShaderObject
} // vkt
