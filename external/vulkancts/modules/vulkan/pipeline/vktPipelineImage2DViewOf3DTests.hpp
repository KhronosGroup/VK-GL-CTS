#ifndef _VKTPIPELINEIMAGE2DVIEWOF3DTESTS_HPP
#define _VKTPIPELINEIMAGE2DVIEWOF3DTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC.
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
 * \brief 2D Image View of 3D image Tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"

namespace vkt
{
namespace pipeline
{

tcu::TestCaseGroup* createImage2DViewOf3DTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType);

} // pipeline
} // vkt

#endif // _VKTPIPELINEIMAGE2DVIEWOF3DTESTS_HPP
