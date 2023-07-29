/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC
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
 * \brief Shader invocations tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawShaderInvocationTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "amber/vktAmberTestCase.hpp"

#include "tcuTestCase.hpp"

using namespace vk;

namespace vkt
{
namespace Draw
{
namespace
{

void checkHelperInvocationTestSupport(Context &context, std::string testName)
{
    DE_UNREF(testName);

    if ((context.getSubgroupProperties().supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT) == 0u)
    {
        TCU_THROW(NotSupportedError, "Device does not support subgroup quad operations");
    }

    if (!context.isDeviceFunctionalitySupported("VK_EXT_shader_demote_to_helper_invocation"))
    {
        TCU_THROW(NotSupportedError, "VK_EXT_shader_demote_to_helper_invocation not supported.");
    }
}

void createTests(tcu::TestCaseGroup *testGroup)
{
    tcu::TestContext &testCtx   = testGroup->getTestContext();
    static const char dataDir[] = "draw/shader_invocation";
    cts_amber::AmberTestCase *testCase =
        cts_amber::createAmberTestCase(testCtx, "helper_invocation", "", dataDir, "helper_invocation.amber");

    testCase->setCheckSupportCallback(checkHelperInvocationTestSupport);

    testGroup->addChild(testCase);
}

} // namespace

tcu::TestCaseGroup *createShaderInvocationTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "shader_invocation", "Shader Invocation tests", createTests);
}

} // namespace Draw
} // namespace vkt
