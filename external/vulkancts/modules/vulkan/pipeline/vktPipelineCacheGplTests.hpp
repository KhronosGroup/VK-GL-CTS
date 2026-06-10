#ifndef _VKTPIPELINECACHEGPLTESTS_HPP
#define _VKTPIPELINECACHEGPLTESTS_HPP
/*------------------------------------------------------------------------
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
 * \brief GPL Cache Collision Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt::pipeline
{

tcu::TestCaseGroup *createGplCacheCollisionTests(tcu::TestContext &testCtx,
                                                 vk::PipelineConstructionType constructionType);

} // namespace vkt::pipeline

#endif // _VKTPIPELINECACHEGPLTESTS_HPP
