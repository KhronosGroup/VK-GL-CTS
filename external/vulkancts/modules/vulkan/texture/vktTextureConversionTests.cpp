/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google Inc.
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
 * \brief Texture conversion tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureConversionTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace texture
{
namespace
{

void populateTextureConversionTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(cts_amber::createAmberTestCase(testCtx, "b10g11r11-negative-values", "", "texture/conversion", "b10g11r11-ufloat-pack32-negative-values.amber"));
}

} // anonymous

tcu::TestCaseGroup* createTextureConversionTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "conversion", "Texture conversion tests.", populateTextureConversionTests);
}

} // texture
} // vkt
