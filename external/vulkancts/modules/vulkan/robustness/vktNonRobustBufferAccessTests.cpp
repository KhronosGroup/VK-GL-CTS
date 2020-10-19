/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
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
 * \brief Tests for non robust oob buffer access in unexecuted shder code
 *        paths.
 *//*--------------------------------------------------------------------*/

#include "vktNonRobustBufferAccessTests.hpp"
#include "amber/vktAmberTestCase.hpp"

#include <vector>
#include <utility>
#include <string>

namespace vkt
{
namespace robustness
{
using namespace cts_amber;

tcu::TestCaseGroup*	createNonRobustBufferAccessTests (tcu::TestContext& testCtx)
{
	static const std::string										kGroupName					= "non_robust_buffer_access";
	static const std::vector<std::pair<std::string, std::string>>	nonRobustBufferAccessTests	=
	{
		{ "unexecuted_oob_underflow",	"Test for correct handling of buffer access index underflow in unexecuted shader code paths" },
		{ "unexecuted_oob_overflow",	"Test for correct handling of buffer access index overflow in unexecuted shader code paths" }
	};

	de::MovePtr<tcu::TestCaseGroup> group{new tcu::TestCaseGroup{testCtx, kGroupName.c_str(), "Non-robust buffer access test group"}};
	for (const auto& test : nonRobustBufferAccessTests)
	{
		group->addChild(createAmberTestCase(testCtx, test.first.c_str(), test.second.c_str(), kGroupName.c_str(), test.first + ".amber"));
	}
	return group.release();
}

} // cts_amber
} // vkt
