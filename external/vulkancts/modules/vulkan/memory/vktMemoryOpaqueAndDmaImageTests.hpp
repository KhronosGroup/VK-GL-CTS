#ifndef _VKTMEMORYOPAQUEANDDMAIMAGETESTS_HPP
#define _VKTMEMORYOPAQUEANDDMAIMAGETESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
 * Copyright (c) 2026 Valve Corporation.
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
 * \brief Check using image memory as Opaque FD and DMA at the same time.
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"

namespace vkt::memory
{

tcu::TestCaseGroup *createOpaqueAndDmaImageTests(tcu::TestContext &testCtx);

} // namespace vkt::memory

#endif // _VKTMEMORYOPAQUEANDDMAIMAGETESTS_HPP
