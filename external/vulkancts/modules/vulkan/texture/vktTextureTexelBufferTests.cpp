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

#include "vktTextureTexelBufferTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace texture
{
namespace
{

tcu::TestCaseGroup* createUniformTexelBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	uniform	 (new tcu::TestCaseGroup(testCtx, "uniform", "Test uniform texel buffer"));

	// .packed
	{
		tcu::TestCaseGroup* const	packed		= new tcu::TestCaseGroup(testCtx, "packed", "Test uniform texel buffer with packed formats");
		static const char			dataDir[]	= "texture/texel_buffer/uniform/packed";

		static const std::string	cases[]		=
		{
			"a2b10g10r10-uint-pack32",
			"a2b10g10r10-unorm-pack32",
			"a8b8g8r8-sint-pack32",
			"a8b8g8r8-snorm-pack32",
			"a8b8g8r8-uint-pack32",
			"a8b8g8r8-unorm-pack32",
			"b10g11r11-ufloat-pack32"
		};

		uniform->addChild(packed);

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		{
			const std::string			fileName	= cases[i] + ".amber";
			cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].c_str(), "", dataDir, fileName);

			packed->addChild(testCase);
		}
	}

	return uniform.release();
}

} // anonymous

tcu::TestCaseGroup* createTextureTexelBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> texelBuffer (new tcu::TestCaseGroup(testCtx, "texel_buffer", "Test texel buffer"));

	texelBuffer->addChild(createUniformTexelBufferTests(testCtx));

	return texelBuffer.release();
}

} // texture
} // vkt
