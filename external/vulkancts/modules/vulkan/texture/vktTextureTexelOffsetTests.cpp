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
 * \brief Texel buffer tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureTexelOffsetTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace texture
{

tcu::TestCaseGroup* createTextureTexelOffsetTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> texelOffset (new tcu::TestCaseGroup(testCtx, "texel_offset", "Test texel offset"));
	static const char			dataDir[]	= "texture/texel_offset";
	static const std::string	cases[][2]		=
	{
		{ "texel_offset",	"A fragment shader that uses texture loads with an offset specified" }
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		const std::string			fileName	= cases[i][0] + ".amber";
		cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i][0].c_str(), cases[i][1].c_str(), dataDir, fileName);

		texelOffset->addChild(testCase);
	}

	return texelOffset.release();
}

} // texture
} // vkt
