/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2018 Google LLC
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
 * \brief GraphicsFuzz tests
 *//*--------------------------------------------------------------------*/

#include "vktTestGroupUtil.hpp"
#include "vktAmberGraphicsFuzzTests.hpp"
#include "vktAmberTestCase.hpp"

namespace vkt
{
namespace cts_amber
{
namespace
{

void createAmberTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	createAmberTestsFromIndexFile(testCtx, group, "index.txt", "graphicsfuzz");
}

} // anonymous

tcu::TestCaseGroup* createGraphicsFuzzTests (tcu::TestContext& testCtx, const std::string& name)
{
	return createTestGroup(testCtx, name.c_str(), "Amber GraphicsFuzz Tests", createAmberTests);
}

} // cts_amber
} // vkt
