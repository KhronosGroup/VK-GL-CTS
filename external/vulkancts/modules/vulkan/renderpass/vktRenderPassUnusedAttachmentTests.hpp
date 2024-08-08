#ifndef _VKTRENDERPASSUNUSEDATTACHMENTTESTS_HPP
#define _VKTRENDERPASSUNUSEDATTACHMENTTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Tests attachments unused by subpasses
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktRenderPassGroupParams.hpp"

namespace vkt
{
namespace renderpass
{

tcu::TestCaseGroup *createRenderPassUnusedAttachmentTests(tcu::TestContext &testCtx,
                                                          const SharedGroupParams groupParams);

} // namespace renderpass
} // namespace vkt

#endif // _VKTRENDERPASSUNUSEDATTACHMENTTESTS_HPP
