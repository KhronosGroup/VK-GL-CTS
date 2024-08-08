#ifndef _VKTIMAGEEXTENDEDUSAGEBITTESTS_HPP
#define _VKTIMAGEEXTENDEDUSAGEBITTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation
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
 * \brief VK_IMAGE_CREATE_EXTENDED_USAGE_BIT tests to check format compatibility
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace image
{

tcu::TestCaseGroup *createImageExtendedUsageBitTests(tcu::TestContext &testCtx);

} // namespace image
} // namespace vkt

#endif // _VKTIMAGEEXTENDEDUSAGEBITTESTS_HPP
