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

tcu::TestCaseGroup* createInvalidSampleIndexTests(tcu::TestContext& testCtx)
{
	std::pair <std::string, VkSampleCountFlagBits>	cases[]			=
	{
		{ "sample_count_2",		VK_SAMPLE_COUNT_2_BIT	},
		{ "sample_count_4",		VK_SAMPLE_COUNT_4_BIT	},
		{ "sample_count_8",		VK_SAMPLE_COUNT_8_BIT	},
		{ "sample_count_16",	VK_SAMPLE_COUNT_16_BIT	},
		{ "sample_count_32",	VK_SAMPLE_COUNT_32_BIT	},
		{ "sample_count_64",	VK_SAMPLE_COUNT_64_BIT	}
	};

	de::MovePtr<tcu::TestCaseGroup>					invalidWrites	(new tcu::TestCaseGroup(testCtx, "invalid_sample_index", "Writes to invalid sample indices should be discarded."));
	static const char								dataDir[]		= "texture/multisample/invalidsampleindex";
	std::vector<std::string>						requirements	= { "Features.shaderStorageImageMultisample" };

	for (const auto& testCase : cases)
	{
		const VkImageCreateInfo			vkImageCreateInfo	=
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// sType
			DE_NULL,								// pNext
			0,										// flags
			VK_IMAGE_TYPE_2D,						// imageType
			VK_FORMAT_R8G8B8A8_UNORM,				// format
			{ 16, 16, 1 },							// extent
			1,										// mipLevels
			1,										// arrayLayers
			testCase.second,						// samples
			VK_IMAGE_TILING_OPTIMAL,				// tiling
			VK_IMAGE_USAGE_SAMPLED_BIT,				// usage
			VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
			0,										// queueFamilyIndexCount
			DE_NULL,								// pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED,				// initialLayout
		};

		std::vector<VkImageCreateInfo>	imageRequirements	= { vkImageCreateInfo };
		const std::string				fileName			= testCase.first + ".amber";

		invalidWrites->addChild(cts_amber::createAmberTestCase(testCtx, testCase.first.c_str(), "", dataDir, fileName, requirements, imageRequirements));
	}

	return invalidWrites.release();
}

} // anonymous

tcu::TestCaseGroup* createTextureMultisampleTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> multisample (new tcu::TestCaseGroup(testCtx, "multisample", "Multisample texture tests"));

	multisample->addChild(createAtomicTests(testCtx));
	multisample->addChild(createInvalidSampleIndexTests(testCtx));

	return multisample.release();
}

} // texture
} // vkt
