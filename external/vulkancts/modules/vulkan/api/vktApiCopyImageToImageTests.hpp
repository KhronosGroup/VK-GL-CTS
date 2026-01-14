#ifndef _VKTAPICOPYIMAGETOIMAGETESTS_HPP
#define _VKTAPICOPYIMAGETOIMAGETESTS_HPP
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
 * \brief Vulkan Copy Image To Image Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiCopiesAndBlittingUtil.hpp"

namespace vkt
{

namespace api
{

void addCopyImageToImageTestsSimpleOnly(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams);
void addCopyImageToImageTests(tcu::TestCaseGroup *group, TestGroupParamsPtr testGroupParams);

} // namespace api
} // namespace vkt

#endif // _VKTAPICOPYIMAGETOIMAGETESTS_HPP
