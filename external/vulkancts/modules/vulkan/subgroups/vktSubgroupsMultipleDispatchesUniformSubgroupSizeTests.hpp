#ifndef _VKTSUBGROUPSMULTIPLEDISPATCHESUNIFORMSUBGROUPSIZETESTS_HPP
#define _VKTSUBGROUPSMULTIPLEDISPATCHESUNIFORMSUBGROUPSIZETESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC.
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
 * \brief Tests that compute shaders have a subgroup size that is uniform in
 * command scope.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace subgroups
{

tcu::TestCaseGroup* createMultipleDispatchesUniformSubgroupSizeTests	(tcu::TestContext& testCtx);

} // subgroups
} // vkt

#endif // _VKTSUBGROUPSMULTIPLEDISPATCHESUNIFORMSUBGROUPSIZETESTS_HPP
