/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Valve Corporation.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Amber tests in the GLSL group.
 *//*--------------------------------------------------------------------*/

#include "vktAmberGlslTests.hpp"
#include "vktAmberTestCase.hpp"

#include <vector>
#include <utility>
#include <string>

namespace vkt
{
namespace cts_amber
{

tcu::TestCaseGroup*	createCombinedOperationsGroup (tcu::TestContext& testCtx)
{
	static const std::string										kGroupName				= "combined_operations";
	static const std::vector<std::pair<std::string, std::string>>	combinedOperationsTests	=
	{
		{ "notxor",			"Bitwise negation of a bitwise xor operation"		},
		{ "negintdivand",	"Bitwise and of a negative value that was divided"	},
	};

	de::MovePtr<tcu::TestCaseGroup> group{new tcu::TestCaseGroup{testCtx, kGroupName.c_str(), "Combined operations test group"}};
	for (const auto& test : combinedOperationsTests)
	{
		group->addChild(createAmberTestCase(testCtx, test.first.c_str(), test.second.c_str(), kGroupName.c_str(), test.first + ".amber"));
	}
	return group.release();
}

} // cts_amber
} // vkt
