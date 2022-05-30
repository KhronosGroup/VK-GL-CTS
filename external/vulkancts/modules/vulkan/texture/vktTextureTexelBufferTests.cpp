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

	// .srgb
	{
		tcu::TestCaseGroup* const	srgb		= new tcu::TestCaseGroup(testCtx, "srgb", "Test uniform texel buffer with srgb formats");
		static const char			dataDir[]	= "texture/texel_buffer/uniform/srgb";

		static const struct {
			std::string	testName;
			VkFormat	format;
		}							cases[]		=
		{
			{"r8g8b8a8_srgb", VK_FORMAT_R8G8B8A8_SRGB},
			{"b8g8r8a8_srgb", VK_FORMAT_B8G8R8A8_SRGB},
			{"b8g8r8_srgb",   VK_FORMAT_B8G8R8_SRGB},
			{"r8g8b8_srgb",   VK_FORMAT_R8G8B8_SRGB},
			{"r8g8_srgb",     VK_FORMAT_R8G8_SRGB},
			{"r8_srgb",       VK_FORMAT_R8_SRGB}
		};

		uniform->addChild(srgb);

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		{
			const std::string							fileName			= cases[i].testName + ".amber";
			const VkImageUsageFlags						usageFlags			= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			const deUint32								width				= 8;
			const deUint32								height				= 8;

			VkImageCreateInfo							imageParams			=
			{
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType         sType;
				DE_NULL,								//  const void*             pNext;
				DE_NULL,								//  VkImageCreateFlags      flags;
				VK_IMAGE_TYPE_2D,						//  VkImageType             imageType;
				cases[i].format,						//  VkFormat                format;
				VkExtent3D({width, height, 1u}),		//  VkExtent3D              extent;
				1u,										//  deUint32                mipLevels;
				1u,										//  deUint32                arrayLayers;
				VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits   samples;
				VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling           tiling;
				usageFlags,								//  VkImageUsageFlags       usage;
				VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode           sharingMode;
				0u,										//  deUint32                queueFamilyIndexCount;
				DE_NULL,								//  const deUint32*         pQueueFamilyIndices;
				VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout           initialLayout;
			};

			std::vector<VkImageCreateInfo>				imageRequirements;
			imageRequirements.push_back(imageParams);

			std::vector<cts_amber::BufferRequirement>	bufferRequirements;
			bufferRequirements.push_back({cases[i].format, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT});

			cts_amber::AmberTestCase*					testCase			=
					cts_amber::createAmberTestCase(testCtx, cases[i].testName.c_str(), "",
												   dataDir, fileName, std::vector<std::string>(),
												   imageRequirements, bufferRequirements);
			srgb->addChild(testCase);
		}
	}

	// .packed
#ifndef CTS_USES_VULKANSC
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
#endif

	// .snorm
#ifndef CTS_USES_VULKANSC
	{
		tcu::TestCaseGroup* const	snorm		= new tcu::TestCaseGroup(testCtx, "snorm", "Test uniform texel buffer with SNORM formats");
		static const char			dataDir[]	= "texture/texel_buffer/uniform/snorm";

		static const struct {
			std::string	testName;
			bool		mandatoryFormat;
			VkFormat	format;
		} cases[]                               =
		{
			{	"b8g8r8-snorm",			false,	VK_FORMAT_B8G8R8_SNORM			},
			{	"b8g8r8a8-snorm",		false,	VK_FORMAT_B8G8R8A8_SINT			},
			{	"r16-snorm",			false,	VK_FORMAT_R16_SNORM				},
			{	"r16g16-snorm",			false,	VK_FORMAT_R16G16_SNORM			},
			{	"r16g16b16-snorm",		false,	VK_FORMAT_R16G16B16_SNORM		},
			{	"r16g16b16a16-snorm",	false,	VK_FORMAT_R16G16B16A16_SNORM	},
			{	"r8-snorm",				true,	VK_FORMAT_R8_SNORM				},
			{	"r8g8-snorm",			true,	VK_FORMAT_R8G8_SNORM			},
			{	"r8g8b8-snorm",			false,	VK_FORMAT_R8G8B8_SNORM			},
			{	"r8g8b8a8-snorm",		false,	VK_FORMAT_R8G8B8A8_SNORM		}
		};

		uniform->addChild(snorm);

		for (const auto& c : cases)
		{
			const std::string							fileName			= c.testName + ".amber";
			std::vector<cts_amber::BufferRequirement>	bufferRequirements;

			if (!c.mandatoryFormat)
				bufferRequirements.push_back({c.format, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT});

			cts_amber::AmberTestCase*					testCase			= cts_amber::createAmberTestCase(testCtx, c.testName.c_str(), "",
																											 dataDir, fileName,
																											 std::vector<std::string>(),
																											 std::vector<vk::VkImageCreateInfo>(),
																											 bufferRequirements);

			snorm->addChild(testCase);
		}
	}
#endif

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
