#ifndef _VKTIMAGEPROCESSINGBOXFILTERSAMPLINGTESTS_HPP
#define _VKTIMAGEPROCESSINGBOXFILTERSAMPLINGTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Image processing box filter sampling tests
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"

#include "vkPipelineConstructionUtil.hpp"

#include "deUniquePtr.hpp"

#include "tcuDefs.hpp"

namespace vkt
{
namespace ImageProcessing
{

tcu::TestCaseGroup *createImageProcessingBoxFilterSamplingGraphicsTests(
    tcu::TestContext &testCtx, const vk::PipelineConstructionType pipelineConstructionType);
tcu::TestCaseGroup *createImageProcessingBoxFilterSamplingComputeTests(tcu::TestContext &testCtx);

} // namespace ImageProcessing
} // namespace vkt

#endif // _VKTIMAGEPROCESSINGBOXFILTERSAMPLINGTESTS_HPP
