#ifndef _VKTBINDINGDESCRIPTORSETRANDOMTESTS_HPP
#define _VKTBINDINGDESCRIPTORSETRANDOMTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
namespace BindingModel
{

tcu::TestCaseGroup *createDescriptorSetRandomTests(tcu::TestContext &testCtx);

} // namespace BindingModel
} // namespace vkt

#endif // _VKTBINDINGDESCRIPTORSETRANDOMTESTS_HPP
