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
 * \brief Texture multisample tests.
 *//*--------------------------------------------------------------------*/

#include "vktTextureMultisampleTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestGroupUtil.hpp"

using namespace vk;

namespace vkt
{
namespace texture
{
namespace
{

tcu::TestCaseGroup* createAtomicTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> atomic		(new tcu::TestCaseGroup(testCtx, "atomic", "Test atomic oprerations on multisample textures"));
	static const char				dataDir[]	= "texture/multisample/atomic";

	static const std::string		cases[]		=
	{
		"storage_image_r32i",
		"storage_image_r32ui"
	};

	std::vector<std::string>		requirements;

	requirements.push_back("Features.shaderStorageImageMultisample");

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
	{
		const std::string			fileName	= cases[i] + ".amber";
		cts_amber::AmberTestCase*	testCase	= cts_amber::createAmberTestCase(testCtx, cases[i].c_str(), "", dataDir, fileName, requirements);

		atomic->addChild(testCase);
	}

	return atomic.release();
}

} // anonymous

tcu::TestCaseGroup* createTextureMultisampleTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> multisample (new tcu::TestCaseGroup(testCtx, "multisample", "Multisample texture tests"));

	multisample->addChild(createAtomicTests(testCtx));

	return multisample.release();
}

} // texture
} // vkt
