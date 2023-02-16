/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Google LLC
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
 * \brief vkGetDeviceProcAddr tests
 *//*--------------------------------------------------------------------*/

#include "vktApiGetDeviceProcAddrTests.hpp"
#include "tcuCommandLine.hpp"
#include "tcuDefs.hpp"

#include "vkGetDeviceProcAddr.inl"

namespace vkt
{

namespace api
{

tcu::TestCaseGroup* createGetDeviceProcAddrTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(
		new tcu::TestCaseGroup(testCtx, "get_device_proc_addr", "Test for vkGetDeviceProcAddr."));
	addGetDeviceProcAddrTests(group.get());
	return group.release();
}

} // api
} // vkt
