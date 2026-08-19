#ifndef _VKTDRAWLARGEVIEWPORTTESTS_HPP
#define _VKTDRAWLARGEVIEWPORTTESTS_HPP
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
 * \brief Large/Off-center view port tests
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktDrawGroupParams.hpp"

namespace vkt
{
namespace Draw
{

tcu::TestCaseGroup *createLargeViewportTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams);

} // namespace Draw
} // namespace vkt

#endif // _VKTDRAWLARGEVIEWPORTTESTS_HPP
