/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor Tests Classes
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTestGroupUtil.hpp"

#include "deUniquePtr.hpp"

namespace vkt
{
namespace tensor
{

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    de::MovePtr<tcu::TestCaseGroup> tensorTests(new tcu::TestCaseGroup(testCtx, name.c_str()));

    tensorTests->addChild(createTensorCreateRequirementsTests(testCtx));
    tensorTests->addChild(createTensorCopyTests(testCtx));
    tensorTests->addChild(createBasicAccessTests(testCtx));
    tensorTests->addChild(createDimensionQueryTests(testCtx));
    tensorTests->addChild(createArrayAccessTests(testCtx));
    tensorTests->addChild(createGraphicsPipelineTests(testCtx));
    tensorTests->addChild(createTensorBoolTests(testCtx));

    return tensorTests.release();
}

} // namespace tensor
} // namespace vkt
