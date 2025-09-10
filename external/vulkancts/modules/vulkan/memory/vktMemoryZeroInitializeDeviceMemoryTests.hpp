#ifndef _VKTMEMORYZEROINITIALIZEDEVICEMEMORYTESTS_HPP
#define _VKTMEMORYZEROINITIALIZEDEVICEMEMORYTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Tests for VK_EXT_zero_initialize_device_memory
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"

namespace vkt
{
namespace memory
{

tcu::TestCaseGroup *createClearedAllocationControlTests(tcu::TestContext &testCtx);

} // namespace memory
} // namespace vkt

#endif // _VKTMEMORYZEROINITIALIZEDEVICEMEMORYTESTS_HPP
