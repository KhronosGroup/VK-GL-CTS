#ifndef _VKTWSIMAINTENANCE1TESTS_HPP
#define _VKTWSIMAINTENANCE1TESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief VK_EXT_surface_maintenance1 and VK_EXT_swapchain_maintenance1 extension tests
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "vkDefs.hpp"

namespace vkt
{
namespace wsi
{

void	createMaintenance1Tests	(tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType);

} // wsi
} // vkt

#endif // _VKTWSIMAINTENANCE1TESTS_HPP
