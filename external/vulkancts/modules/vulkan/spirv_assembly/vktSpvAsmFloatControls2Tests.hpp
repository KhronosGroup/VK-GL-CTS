#ifndef _VKTSPVASMFLOATCONTROLS2TESTS_HPP
#define _VKTSPVASMFLOATCONTROLS2TESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief SPIR-V Assembly Tests for VK_KHR_shader_float_controls.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace SpirVAssembly
{

tcu::TestCaseGroup *createFloatControls2ComputeGroup(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createFloatControls2GraphicsGroup(tcu::TestContext &testCtx);

} // namespace SpirVAssembly
} // namespace vkt

#endif // _VKTSPVASMFLOATCONTROLS2TESTS_HPP
