#ifndef _VKTDGCCOMPUTEPUSHINDEXHEAPTESTSEXT_HPP
#define _VKTDGCCOMPUTEPUSHINDEXHEAPTESTSEXT_HPP
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
 * \brief Device Generated Commands EXT compute tests that use
 *        DGC push data to select a descriptor heap slot via
 *        VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT.
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"

namespace vkt
{
namespace DGC
{
tcu::TestCaseGroup *createDGCComputePushIndexHeapTestsExt(tcu::TestContext &testCtx);
} // namespace DGC
} // namespace vkt

#endif // _VKTDGCCOMPUTEPUSHINDEXHEAPTESTSEXT_HPP
