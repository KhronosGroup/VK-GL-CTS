#ifndef _VKTTENSORTESTS_HPP
#define _VKTTENSORTESTS_HPP

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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace tcu
{
class TestCaseGroup;
class TestContext;
} // namespace tcu

namespace vkt
{
namespace tensor
{

tcu::TestCaseGroup *createBasicAccessTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createTensorCreateRequirementsTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createDimensionQueryTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createArrayAccessTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createTensorCopyTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createGraphicsPipelineTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createTensorBoolTests(tcu::TestContext &testCtx);

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name);

} // namespace tensor
} // namespace vkt

#endif // _VKTTENSORTESTS_HPP
