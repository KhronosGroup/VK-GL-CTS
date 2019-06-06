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
 *//*--------------------------------------------------------------------*/

#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace cts_amber
{

AmberTestCase* createAmberTestCase (tcu::TestContext&	testCtx,
									const char*			name,
									const char*			description,
									const char*			category,
									const std::string&	filename)
{
	AmberTestCase *testCase = new AmberTestCase(testCtx, name, description);

	// shader_test files are saved in <path>/external/vulkancts/data/vulkan/amber/<categoryname>/
	// Make sure the input can be parsed before we use it.
	if (testCase->parse(category, filename))
		return testCase;
	else
	{
		const std::string msg = "Failed to parse Amber file: " + filename;

		delete testCase;
		TCU_THROW(InternalError, msg.c_str());
	}

	return DE_NULL;
}

} // cts_amber
} // vkt
