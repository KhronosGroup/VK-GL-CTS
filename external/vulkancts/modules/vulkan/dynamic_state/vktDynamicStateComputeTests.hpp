#ifndef _VKTDYNAMICSTATECOMPUTETESTS_HPP
#define _VKTDYNAMICSTATECOMPUTETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Dynamic State tests mixing it with compute and transfer.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace DynamicState
{

tcu::TestCaseGroup* createDynamicStateComputeTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType);
void cleanupDevice();

} // DynamicState
} // vkt

#endif // _VKTDYNAMICSTATECOMPUTETESTS_HPP
