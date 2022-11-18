#ifndef _VKTAPIBUFFERMEMORYREQUIREMENTSTESTS_HPP
#define _VKTAPIBUFFERMEMORYREQUIREMENTSTESTS_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Cover for non-zero of memoryTypeBits from vkGetBufferMemoryRequirements*() tests.
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

tcu::TestCaseGroup* createBufferMemoryRequirementsTests (tcu::TestContext& testCtx);

} // api
} // vkt

#endif // _VKTAPIBUFFERMEMORYREQUIREMENTSTESTS_HPP
