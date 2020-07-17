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

void populateUfloatNegativeValuesTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx = group->getTestContext();
	VkImageUsageFlags	usage	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

	VkImageCreateInfo	info	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType
		DE_NULL,								// const void*              pNext
		0,										// VkImageCreateFlags       flags
		VK_IMAGE_TYPE_2D,						// VkImageType              imageType
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,		// VkFormat                 format
		{50u, 50u, 1u},							// VkExtent3D               extent
		1u,										// uint32_t                 mipLevels
		1u,										// uint32_t                 arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling            tiling
		usage,									// VkImageUsageFlags        usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode
		0u,										// uint32_t                 queueFamilyIndexCount
		DE_NULL,								// const uint32_t*          pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout
	};

	group->addChild(cts_amber::createAmberTestCase(testCtx, "b10g11r11", "", "texture/conversion/ufloat_negative_values", "b10g11r11-ufloat-pack32.amber",
					std::vector<std::string>(), std::vector<VkImageCreateInfo>(1, info)));
}

void populateSnormClampTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&	testCtx	= group->getTestContext();
	VkImageUsageFlags	usage	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkImageCreateInfo	info	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType
		DE_NULL,								// const void*              pNext
		0,										// VkImageCreateFlags       flags
		VK_IMAGE_TYPE_1D,						// VkImageType              imageType
		VK_FORMAT_UNDEFINED,					// VkFormat                 format
		{1u, 1u, 1u},							// VkExtent3D               extent
		1u,										// uint32_t                 mipLevels
		1u,										// uint32_t                 arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling            tiling
		usage,									// VkImageUsageFlags        usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode
		0u,										// uint32_t                 queueFamilyIndexCount
		DE_NULL,								// const uint32_t*          pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout
	};

	struct TestParams
	{
		std::string testName;
		std::string amberFile;
		VkFormat	format;
	} params[] =
	{
		{ "a2b10g10r10_snorm_pack32",	"a2b10g10r10-snorm-pack32.amber",	VK_FORMAT_A2B10G10R10_SNORM_PACK32	},
		{ "a2r10g10b10_snorm_pack32",	"a2r10g10b10-snorm-pack32.amber",	VK_FORMAT_A2R10G10B10_SNORM_PACK32	},
		{ "a8b8g8r8_snorm_pack32",		"a8b8g8r8-snorm-pack32.amber",		VK_FORMAT_A8B8G8R8_SNORM_PACK32		},
		{ "b8g8r8a8_snorm",				"b8g8r8a8-snorm.amber",				VK_FORMAT_B8G8R8A8_SNORM			},
		{ "b8g8r8_snorm",				"b8g8r8-snorm.amber",				VK_FORMAT_B8G8R8_SNORM				},
		{ "r16g16b16a16_snorm",			"r16g16b16a16-snorm.amber",			VK_FORMAT_R16G16B16A16_SNORM		},
		{ "r16g16b16_snorm",			"r16g16b16-snorm.amber",			VK_FORMAT_R16G16B16_SNORM			},
		{ "r16g16_snorm",				"r16g16-snorm.amber",				VK_FORMAT_R16G16_SNORM				},
		{ "r16_snorm",					"r16-snorm.amber",					VK_FORMAT_R16_SNORM					},
		{ "r8g8b8a8_snorm",				"r8g8b8a8-snorm.amber",				VK_FORMAT_R8G8B8A8_SNORM			},
		{ "r8g8b8_snorm",				"r8g8b8-snorm.amber",				VK_FORMAT_R8G8B8_SNORM				},
		{ "r8g8_snorm",					"r8g8-snorm.amber",					VK_FORMAT_R8G8_SNORM				},
		{ "r8_snorm",					"r8-snorm.amber",					VK_FORMAT_R8_SNORM					}
	};

	for (const auto& param : params)
	{
		info.format = param.format;
		group->addChild(cts_amber::createAmberTestCase(testCtx, param.testName.c_str(), "", "texture/conversion/snorm_clamp", param.amberFile.c_str(),
						std::vector<std::string>(), std::vector<VkImageCreateInfo>(1, info)));
	}
}

void populateTextureConversionTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(createTestGroup(testCtx, "ufloat_negative_values", "Tests for converting negative floats to unsigned floats", populateUfloatNegativeValuesTests));
	group->addChild(createTestGroup(testCtx, "snorm_clamp", "Tests for SNORM corner cases when smallest negative number gets clamped to -1", populateSnormClampTests));
}

} // anonymous

tcu::TestCaseGroup* createTextureConversionTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "conversion", "Texture conversion tests.", populateTextureConversionTests);
}

} // texture
} // vkt
