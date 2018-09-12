#ifndef _VKTAPIMEMORYREQUIREMENTINVARIANCETESTS_HPP
#define _VKTAPIMEMORYREQUIREMENTINVARIANCETESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Google Inc.
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
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVectorType.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace api
{

	tcu::TestCaseGroup* createMemoryRequirementInvarianceTests(tcu::TestContext& testCtx);

} // api
} // vkt

#endif // _VKTAPIMEMORYREQUIREMENTINVARIANCETESTS_HPP
