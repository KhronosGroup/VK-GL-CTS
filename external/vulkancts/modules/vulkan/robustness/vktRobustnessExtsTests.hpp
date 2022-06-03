#ifndef _VKTROBUSTNESSEXTSTESTS_HPP
#define _VKTROBUSTNESSEXTSTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019-2020 NVIDIA Corporation
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
 * \brief Tests for randomly-generated descriptor set layouts.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace robustness
{

tcu::TestCaseGroup* createRobustness2Tests(tcu::TestContext& testCtx);
tcu::TestCaseGroup* createImageRobustnessTests(tcu::TestContext& testCtx);
tcu::TestCaseGroup* createPipelineRobustnessTests(tcu::TestContext& testCtx);

} // robustness
} // vkt

#endif // _VKTROBUSTNESSEXTSTESTS_HPP
