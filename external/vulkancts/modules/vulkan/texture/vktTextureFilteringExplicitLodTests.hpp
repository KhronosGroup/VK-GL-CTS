#ifndef _VKTTEXTUREFILTERINGEXPLICITLODTESTS_HPP
#define _VKTTEXTUREFILTERINGEXPLICITLODTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Texture filtering tests with explicit LOD instructions
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"

namespace vkt
{
namespace texture
{

tcu::TestCaseGroup *createExplicitLodTests(tcu::TestContext &testCtx);

} // namespace texture
} // namespace vkt

#endif // _VKTTEXTUREFILTERINGEXPLICITLODTESTS_HPP
