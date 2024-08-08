#ifndef _VKTDRAWAHBTESTS_HPP
#define _VKTDRAWAHBTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google LLC
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
 * \brief Android Hardware Buffer Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace Draw
{

tcu::TestCaseGroup *createAhbTests(tcu::TestContext &testCtx);

} // namespace Draw
} // namespace vkt

#endif // _VKTDRAWAHBTESTS_HPP
