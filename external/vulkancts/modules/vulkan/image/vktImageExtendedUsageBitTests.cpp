/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 The Android Open Source Project
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
 * \brief Memory qualifiers tests
 *//*--------------------------------------------------------------------*/

#include "vktImageExtendedUsageBitTests.hpp"

#include "vkRef.hpp"
#include "vkQueryUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktImageTestsUtil.hpp"

#include "tcuTestLog.hpp"

using namespace vk;

namespace vkt
{
namespace image
{
namespace
{

bool isCompatibleCompressedFormat(VkFormat format0, VkFormat format1)
{
	DE_ASSERT(isCompressedFormat(format0) && isCompressedFormat(format1));
	// update this mapping if VkFormat changes
	DE_STATIC_ASSERT(VK_CORE_FORMAT_LAST == 185);

	bool result = false;

	std::map<VkFormat, VkFormat> map =
	{
		{ VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK },
		{ VK_FORMAT_BC1_RGB_SRGB_BLOCK, VK_FORMAT_BC1_RGB_UNORM_BLOCK },
		{ VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK },
		{ VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_UNORM_BLOCK },
		{ VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK },
		{ VK_FORMAT_BC2_SRGB_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK },
		{ VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK },
		{ VK_FORMAT_BC3_SRGB_BLOCK, VK_FORMAT_BC3_UNORM_BLOCK },
		{ VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_BC4_SNORM_BLOCK },
		{ VK_FORMAT_BC4_SNORM_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK},
		{ VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK },
		{ VK_FORMAT_BC5_SNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK },
		{ VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK },
		{ VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_BC7_UNORM_BLOCK },
		{ VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK },
		{ VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK },
		{ VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK },
		{ VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK },
		{ VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK },
		{ VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK },
		{ VK_FORMAT_EAC_R11_UNORM_BLOCK, VK_FORMAT_EAC_R11_SNORM_BLOCK },
		{ VK_FORMAT_EAC_R11_SNORM_BLOCK, VK_FORMAT_EAC_R11_UNORM_BLOCK },
		{ VK_FORMAT_EAC_R11G11_UNORM_BLOCK, VK_FORMAT_EAC_R11G11_SNORM_BLOCK },
		{ VK_FORMAT_EAC_R11G11_SNORM_BLOCK, VK_FORMAT_EAC_R11G11_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_4x4_UNORM_BLOCK, VK_FORMAT_ASTC_4x4_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_4x4_SRGB_BLOCK, VK_FORMAT_ASTC_4x4_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_5x4_UNORM_BLOCK, VK_FORMAT_ASTC_5x4_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_5x4_SRGB_BLOCK, VK_FORMAT_ASTC_5x4_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_5x5_UNORM_BLOCK, VK_FORMAT_ASTC_5x5_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_5x5_SRGB_BLOCK, VK_FORMAT_ASTC_5x5_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_6x5_UNORM_BLOCK, VK_FORMAT_ASTC_6x5_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_6x5_SRGB_BLOCK,  VK_FORMAT_ASTC_6x5_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_6x6_UNORM_BLOCK, VK_FORMAT_ASTC_6x6_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_6x6_SRGB_BLOCK, VK_FORMAT_ASTC_6x6_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_8x5_UNORM_BLOCK, VK_FORMAT_ASTC_8x5_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_8x5_SRGB_BLOCK, VK_FORMAT_ASTC_8x5_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_8x6_UNORM_BLOCK, VK_FORMAT_ASTC_8x6_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_8x6_SRGB_BLOCK, VK_FORMAT_ASTC_8x6_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_8x8_UNORM_BLOCK, VK_FORMAT_ASTC_8x8_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_8x8_SRGB_BLOCK, VK_FORMAT_ASTC_8x8_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_10x5_UNORM_BLOCK, VK_FORMAT_ASTC_10x5_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_10x5_SRGB_BLOCK, VK_FORMAT_ASTC_10x5_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_10x6_UNORM_BLOCK, VK_FORMAT_ASTC_10x6_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_10x6_SRGB_BLOCK, VK_FORMAT_ASTC_10x6_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_10x8_UNORM_BLOCK, VK_FORMAT_ASTC_10x8_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_10x8_SRGB_BLOCK, VK_FORMAT_ASTC_10x8_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_10x10_UNORM_BLOCK, VK_FORMAT_ASTC_10x10_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_10x10_SRGB_BLOCK, VK_FORMAT_ASTC_10x10_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_12x10_UNORM_BLOCK, VK_FORMAT_ASTC_12x10_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_12x10_SRGB_BLOCK, VK_FORMAT_ASTC_12x10_UNORM_BLOCK },
		{ VK_FORMAT_ASTC_12x12_UNORM_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK },
		{ VK_FORMAT_ASTC_12x12_SRGB_BLOCK, VK_FORMAT_ASTC_12x12_UNORM_BLOCK },
	};

	if (map.find(format1) != map.end() && map.find(format1)->second == format0)
		result = true;

	return result;
}

bool isCompatibleFormat(VkFormat format0, VkFormat format1)
{
	if (format0 == format1)
		return true;

	// Uncompressed color formats are compatible with each other if they occupy the same number of bits per texel block.
	if (!isDepthStencilFormat(format0) && !isCompressedFormat(format0) &&
		!isDepthStencilFormat(format1) && !isCompressedFormat(format1) &&
		mapVkFormat(format0).getPixelSize() == mapVkFormat(format1).getPixelSize())
		return true;

	if (isCompressedFormat(format0) && isCompressedFormat(format1) &&
	    isCompatibleCompressedFormat(format0, format1))
		 return true;

	return false;
}

struct TestParams
{
	VkFormat			imageFormat;
	VkImageUsageFlags	usage;
	VkImageTiling		tiling;
};

class PhysicalDeviceImageFormatProperties
{
public:
	virtual VkResult getPhysicalDeviceImageFormatProperties(const InstanceInterface &vki, VkPhysicalDevice device, VkFormat viewFormat, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags)
	{
		VkImageFormatProperties formatProperties;
		return vki.getPhysicalDeviceImageFormatProperties(device, viewFormat, VK_IMAGE_TYPE_2D, tiling, usage, flags, &formatProperties);
	}
};

class PhysicalDeviceImageFormatProperties2 : public PhysicalDeviceImageFormatProperties
{
public:
	virtual VkResult getPhysicalDeviceImageFormatProperties(const InstanceInterface &vki, VkPhysicalDevice device, VkFormat viewFormat, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags)
	{
		VkImageFormatProperties2			formatProperties2	= initVulkanStructure();
		VkPhysicalDeviceImageFormatInfo2	imageFormatInfo2	=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	// VkStructureType		sType
			DE_NULL,												// const void*			pNext
			viewFormat,												// VkFormat				format
			VK_IMAGE_TYPE_2D,										// VkImageType			type
			tiling,													// VkImageTiling		tiling
			usage,													// VkImageUsageFlags	usage
			flags													// VkImageCreateFlags	flags
		};
		return vki.getPhysicalDeviceImageFormatProperties2(device, &imageFormatInfo2, &formatProperties2);
	}
};

template<typename T>
tcu::TestStatus testExtendedUsageBitCompatiblity (Context& context, TestParams params)
{
	T							func;
	VkFormat					viewFormat;
	VkResult					expected			= VK_ERROR_FORMAT_NOT_SUPPORTED;
	const InstanceInterface&	vki					= context.getInstanceInterface();

	for (viewFormat = (VkFormat)(VK_FORMAT_UNDEFINED + 1); viewFormat < VK_CORE_FORMAT_LAST; viewFormat = (VkFormat)(viewFormat + 1))
	{
		if (!isCompatibleFormat((VkFormat)viewFormat, params.imageFormat))
			continue;

		if (func.getPhysicalDeviceImageFormatProperties(vki, context.getPhysicalDevice(), (VkFormat)viewFormat, params.tiling, params.usage, 0) == VK_SUCCESS)
		{
			expected = VK_SUCCESS;
			break;
		}
	}

	// No compatible view format supports the tested usage.
	if (viewFormat == VK_CORE_FORMAT_LAST)
	{
		std::ostringstream error;
		error << "Usage is not supported by any compatible format";
		throw tcu::NotSupportedError(error.str().c_str());
	}

	VkResult res = func.getPhysicalDeviceImageFormatProperties(vki, context.getPhysicalDevice(), params.imageFormat, params.tiling, params.usage, VK_IMAGE_CREATE_EXTENDED_USAGE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);

	if (res != expected)
	{
		std::ostringstream error;
		error << "Fail: view format " << getFormatStr(viewFormat);
		return tcu::TestStatus::fail(error.str().c_str());
	}

	return tcu::TestStatus::pass("Pass");
}

void checkSupport (Context& context, TestParams params)
{
	context.requireDeviceFunctionality("VK_KHR_maintenance2");

	VkFormatProperties formatProperties;
	context.getInstanceInterface().getPhysicalDeviceFormatProperties(context.getPhysicalDevice(), params.imageFormat, &formatProperties);

	if (params.tiling == vk::VK_IMAGE_TILING_OPTIMAL && formatProperties.optimalTilingFeatures == 0)
		throw tcu::NotSupportedError("Format not supported");

	if (params.tiling == vk::VK_IMAGE_TILING_LINEAR && formatProperties.linearTilingFeatures == 0)
		throw tcu::NotSupportedError("Format not supported");

#ifndef CTS_USES_VULKANSC
	if (params.usage & VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR ||
		params.usage & VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR ||
		params.usage & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR)
		context.requireDeviceFunctionality("VK_KHR_video_decode_queue");

	if (params.usage & VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR ||
		params.usage & VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR ||
		params.usage & VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR)
		context.requireDeviceFunctionality("VK_KHR_video_encode_queue");

	if (params.usage & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT)
		context.requireDeviceFunctionality("VK_EXT_fragment_density_map");

	if (params.usage & VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR)
		context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

	if (params.usage & VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI)
		context.requireDeviceFunctionality("VK_HUAWEI_invocation_mask");
#endif // CTS_USES_VULKANSC

}

} // anonymous ns

tcu::TestCaseGroup* createImageExtendedUsageBitTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> imageExtendedUsageBitTests(new tcu::TestCaseGroup(testCtx, "extended_usage_bit_compatibility", "VK_IMAGE_CREATE_EXTENDED_USAGE_BIT tests to check format compatibility"));
	de::MovePtr<tcu::TestCaseGroup> getPhysicalDeviceImageFormatPropertiesTests(new tcu::TestCaseGroup(testCtx, "image_format_properties", "vkGetPhysicalDeviceImageFormatProperties() tests"));
	de::MovePtr<tcu::TestCaseGroup> getPhysicalDeviceImageFormatProperties2Tests(new tcu::TestCaseGroup(testCtx, "image_format_properties2", "vkGetPhysicalDeviceImageFormatProperties2() tests"));

	struct ImageUsageFlags
	{
		VkImageUsageFlags		usage;
		const char*				name;
	};

	ImageUsageFlags				usages[]	=
	{
		{ VK_IMAGE_USAGE_TRANSFER_SRC_BIT,							"VK_IMAGE_USAGE_TRANSFER_SRC_BIT" },
		{ VK_IMAGE_USAGE_TRANSFER_DST_BIT,							"VK_IMAGE_USAGE_TRANSFER_DST_BIT" },
		{ VK_IMAGE_USAGE_SAMPLED_BIT,								"VK_IMAGE_USAGE_SAMPLED_BIT" },
		{ VK_IMAGE_USAGE_STORAGE_BIT,								"VK_IMAGE_USAGE_STORAGE_BIT" },
		{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,						"VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT" },
		{ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,				"VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT" },
		{ VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,					"VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT" },
		{ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,						"VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT" },
#ifndef CTS_USES_VULKANSC
		{ VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,					"VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR" },
		{ VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR,					"VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR" },
		{ VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,					"VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR" },
		{ VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,				"VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT" },
		{ VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,	"VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR" },
		{ VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR,					"VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR" },
		{ VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,					"VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR" },
		{ VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,					"VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR" },
		{ VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI,				"VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI" },
		{ VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV,					"VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV" },
#endif // CTS_USES_VULKANSC
	};

	struct TilingParams
	{
		VkImageTiling		tiling;
		const char*			name;
	};
	TilingParams			tiling[]	=
	{
		{ VK_IMAGE_TILING_LINEAR,	"linear"},
		{ VK_IMAGE_TILING_OPTIMAL,	"optimal"}
	};

	for (VkFormat imageFormat = (VkFormat)(VK_FORMAT_UNDEFINED + 1); imageFormat < VK_CORE_FORMAT_LAST; imageFormat = (VkFormat)(imageFormat + 1))
	{
		for (unsigned tilingNdx = 0; tilingNdx < DE_LENGTH_OF_ARRAY(tiling); tilingNdx++)
		{
			for (unsigned usageNdx = 0; usageNdx < DE_LENGTH_OF_ARRAY(usages); usageNdx++)
			{
				struct TestParams params = { imageFormat, usages[usageNdx].usage, tiling[tilingNdx].tiling };
				std::ostringstream name;
				std::string usageName = usages[usageNdx].name;
				name << getFormatShortString(imageFormat) << "_" << tiling[tilingNdx].name << "_" << de::toLower(usageName.substr(15));
				addFunctionCase(getPhysicalDeviceImageFormatPropertiesTests.get(), name.str().c_str(), "Checks usage bit format compatibility among compatible image view formats", checkSupport, testExtendedUsageBitCompatiblity<PhysicalDeviceImageFormatProperties>, params);
				addFunctionCase(getPhysicalDeviceImageFormatProperties2Tests.get(), name.str().c_str(), "Checks usage bit format compatibility among compatible image view formats", checkSupport, testExtendedUsageBitCompatiblity<PhysicalDeviceImageFormatProperties2>, params);
			}
		}
	}

	imageExtendedUsageBitTests->addChild(getPhysicalDeviceImageFormatPropertiesTests.release());
	imageExtendedUsageBitTests->addChild(getPhysicalDeviceImageFormatProperties2Tests.release());
	return imageExtendedUsageBitTests.release();
}

} // image
} // vkt
