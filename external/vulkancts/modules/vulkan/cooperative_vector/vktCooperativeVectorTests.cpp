/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Cooperative Vector Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktCooperativeVectorTests.hpp"
#include "vktCooperativeVectorBasicTests.hpp"
#include "vktCooperativeVectorMatrixTests.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace cooperative_vector
{

namespace
{

void createChildren(tcu::TestCaseGroup *cooperativeVectorTests)
{
    DE_UNREF(cooperativeVectorTests);

#ifndef CTS_USES_VULKANSC
    tcu::TestContext &testCtx = cooperativeVectorTests->getTestContext();

    cooperativeVectorTests->addChild(createCooperativeVectorBasicTests(testCtx));
    cooperativeVectorTests->addChild(createCooperativeVectorMatrixMulTests(testCtx));
    cooperativeVectorTests->addChild(createCooperativeVectorTrainingTests(testCtx));
    cooperativeVectorTests->addChild(createCooperativeVectorMatrixLayoutTests(testCtx));
    cooperativeVectorTests->addChild(createCooperativeVectorMatrixTypeConversionTests(testCtx));
#endif // CTS_USES_VULKANSC
}

} // namespace

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name, createChildren);
}

} // namespace cooperative_vector
} // namespace vkt
