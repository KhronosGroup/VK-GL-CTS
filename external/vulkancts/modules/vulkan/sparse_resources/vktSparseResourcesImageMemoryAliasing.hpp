#ifndef _VKTSPARSERESOURCESIMAGEMEMORYALIASING_HPP
#define _VKTSPARSERESOURCESIMAGEMEMORYALIASING_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesImageMemoryAliasing.hpp
 * \brief Sparse image memory aliasing tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"

namespace vkt
{
namespace sparse
{

tcu::TestCaseGroup *createImageSparseMemoryAliasingTests(tcu::TestContext &testCtx);
tcu::TestCaseGroup *createDeviceGroupImageSparseMemoryAliasingTests(tcu::TestContext &testCtx);

} // namespace sparse
} // namespace vkt

#endif // _VKTSPARSERESOURCESIMAGEMEMORYALIASING_HPP
