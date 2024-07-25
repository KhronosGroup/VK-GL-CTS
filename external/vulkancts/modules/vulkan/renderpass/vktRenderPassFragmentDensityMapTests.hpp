#ifndef _VKTRENDERPASSFRAGMENTDENSITYMAPTESTS_HPP
#define _VKTRENDERPASSFRAGMENTDENSITYMAPTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests fragment density map extension ( VK_EXT_fragment_density_map )
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktRenderPassGroupParams.hpp"

namespace vkt
{
namespace renderpass
{

tcu::TestCaseGroup *createFragmentDensityMapTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams);

} // namespace renderpass
} // namespace vkt

#endif // _VKTRENDERPASSFRAGMENTDENSITYMAPTESTS_HPP
