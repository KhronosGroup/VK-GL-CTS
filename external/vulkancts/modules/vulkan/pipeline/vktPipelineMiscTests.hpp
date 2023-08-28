#ifndef _VKTPIPELINEMISCTESTS_HPP
#define _VKTPIPELINEMISCTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests using only vertex shader in a graphics pipeline
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt
{
namespace pipeline
{

tcu::TestCaseGroup* createMiscTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType);

} // pipeline
} // vkt

#endif // _VKTPIPELINEMISCTESTS_HPP
