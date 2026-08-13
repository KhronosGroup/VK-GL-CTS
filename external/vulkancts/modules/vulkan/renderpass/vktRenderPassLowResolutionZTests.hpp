#ifndef _VKTRENDERPASSLOWRESOLUTIONZTESTS_HPP
#define _VKTRENDERPASSLOWRESOLUTIONZTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief Test coverage for depth optimizations like LRZ (Adreno GPUs) and
 * EZ (Broadcom GPUs). It is based on tests for LRZ and has comments
 * talking about LRZ implementation details, which are left to give
 * a context on the intent of tests.
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "vktRenderPassGroupParams.hpp"

namespace vkt::renderpass
{

tcu::TestCaseGroup *createRenderPassLowResolutionZTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams);

} // namespace vkt::renderpass

#endif // _VKTRENDERPASSLOWRESOLUTIONZTESTS_HPP
