/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 *
 * Copyright (c) 2022 Khronos Group
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
 */ /*!
* \file
* \brief Test for Image Compression control
*/ /*--------------------------------------------------------------------*/

#include <iostream>
#include <typeinfo>

#include "tcuCommandLine.hpp"
#include "tcuDefs.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include "vkApiVersion.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"

#include "vktApiVersionCheck.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"

#include "deString.h"
#include "deStringUtil.hpp"

#include <map>
#include <vector>

using namespace vk;
using namespace std;

namespace vkt
{

namespace api
{

struct TestParams
{
	VkFormat					 format;
	bool						 useExtension;
	VkImageCompressionControlEXT control;
};

static void checkImageCompressionControlSupport(Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_image_compression_control");
	vk::VkPhysicalDeviceImageCompressionControlFeaturesEXT imageCompressionControlFeatures{};
	imageCompressionControlFeatures.sType =
		vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT;
	imageCompressionControlFeatures.pNext = DE_NULL;

	vk::VkPhysicalDeviceFeatures2 features2{};
	features2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &imageCompressionControlFeatures;

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (!imageCompressionControlFeatures.imageCompressionControl)
		TCU_THROW(NotSupportedError, "VK_EXT_image_compression_control Image "
									 "compression control feature not supported.");
}

static void validate(Context& context, tcu::ResultCollector& results, VkDevice device, TestParams& testParams,
					 VkImage image)
{

	for (unsigned planeIndex = 0; planeIndex < testParams.control.compressionControlPlaneCount; planeIndex++)
	{
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		if (isYCbCrFormat(testParams.format))
		{
			VkImageAspectFlags planeAspects[]{ VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_ASPECT_PLANE_1_BIT,
											   VK_IMAGE_ASPECT_PLANE_2_BIT };
			aspect = planeAspects[planeIndex];
		}

		VkImageCompressionPropertiesEXT compressionProperties = initVulkanStructure();
		VkImageSubresource2EXT subresource = initVulkanStructure();
		subresource.imageSubresource.aspectMask = aspect;
		VkSubresourceLayout2EXT subresourceLayout = initVulkanStructure(&compressionProperties);
		context.getDeviceInterface().getImageSubresourceLayout2EXT(device, image, &subresource, &subresourceLayout);


		VkImageCompressionControlEXT compressionEnabled = initVulkanStructure();
		compressionEnabled.compressionControlPlaneCount = testParams.control.compressionControlPlaneCount;
		compressionEnabled.flags = testParams.control.flags;
		VkImageCompressionFixedRateFlagsEXT fixedRateFlags[3] = { VK_IMAGE_COMPRESSION_FIXED_RATE_FLAG_BITS_MAX_ENUM_EXT,
													VK_IMAGE_COMPRESSION_FIXED_RATE_FLAG_BITS_MAX_ENUM_EXT,
													VK_IMAGE_COMPRESSION_FIXED_RATE_FLAG_BITS_MAX_ENUM_EXT };
		compressionEnabled.pFixedRateFlags = fixedRateFlags;

		VkPhysicalDeviceImageFormatInfo2 formatInfo = initVulkanStructure(&compressionEnabled);
		formatInfo.format							= testParams.format;
		formatInfo.type								= VK_IMAGE_TYPE_2D;
		formatInfo.tiling							= VK_IMAGE_TILING_OPTIMAL;
		formatInfo.usage							= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkImageCompressionPropertiesEXT compressionPropertiesSupported = initVulkanStructure();
		VkImageFormatProperties2		properties2 = initVulkanStructure(&compressionPropertiesSupported);

		context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &formatInfo,
																			   &properties2);

		if (testParams.useExtension)
		{

			if ((compressionPropertiesSupported.imageCompressionFixedRateFlags &
				 compressionProperties.imageCompressionFixedRateFlags) !=
				compressionProperties.imageCompressionFixedRateFlags)
			{
				results.fail("Got image with fixed rate flags that are not supported "
							 "in image format properties.");
			}
			if ((compressionPropertiesSupported.imageCompressionFlags & compressionProperties.imageCompressionFlags) !=
					compressionProperties.imageCompressionFlags &&
				compressionProperties.imageCompressionFlags != VK_IMAGE_COMPRESSION_DISABLED_EXT)
			{
				results.fail("Got image with compression flags that are not supported "
							 "in image format properties.");
			}
			if (testParams.control.flags == VK_IMAGE_COMPRESSION_DEFAULT_EXT &&
				compressionProperties.imageCompressionFixedRateFlags != 0)
			{
				results.fail("Got lossy compression when DEFAULT compression was requested.");
			}
			if (testParams.control.flags == VK_IMAGE_COMPRESSION_DISABLED_EXT &&
				compressionProperties.imageCompressionFlags != VK_IMAGE_COMPRESSION_DISABLED_EXT)
			{
				results.fail("Image compression not disabled.");
			}
			if (testParams.control.flags == VK_IMAGE_COMPRESSION_DISABLED_EXT &&
				compressionProperties.imageCompressionFixedRateFlags != 0)
			{
				results.fail("Image compression disabled but got fixed rate flags.");
			}
			if (testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT &&
				!(compressionProperties.imageCompressionFlags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT ||
				  compressionProperties.imageCompressionFlags == VK_IMAGE_COMPRESSION_DISABLED_EXT ||
				  compressionProperties.imageCompressionFlags == VK_IMAGE_COMPRESSION_DEFAULT_EXT))
			{
				results.fail("Explicit compression flags not returned for image "
							 "creation with FIXED RATE DEFAULT.");
			}

			if (testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT)
			{
				uint32_t minRequestedRate = 1 << deCtz32(testParams.control.pFixedRateFlags[planeIndex]);
				uint32_t actualRate		  = compressionProperties.imageCompressionFixedRateFlags;
				if (compressionProperties.imageCompressionFlags != VK_IMAGE_COMPRESSION_DISABLED_EXT &&
					compressionProperties.imageCompressionFlags != VK_IMAGE_COMPRESSION_DEFAULT_EXT)
				{

					if (minRequestedRate > actualRate)
					{
						results.fail("Image created with less bpc than requested.");
					}
				}
			}
		}
		else
		{
			if (compressionProperties.imageCompressionFixedRateFlags != VK_IMAGE_COMPRESSION_FIXED_RATE_NONE_EXT)
			{
				results.fail("Fixed rate compression should not be enabled.");
			}

			if (compressionProperties.imageCompressionFlags != VK_IMAGE_COMPRESSION_DISABLED_EXT &&
				compressionProperties.imageCompressionFlags != VK_IMAGE_COMPRESSION_DEFAULT_EXT)
			{
				results.fail("Image compression should be default or not be enabled.");
			}
		}
	}
}

static tcu::TestStatus ahbImageCreateTest(Context& context, TestParams testParams)
{
	using namespace vkt::ExternalMemoryUtil;

	context.requireDeviceFunctionality("VK_ANDROID_external_memory_android_hardware_buffer");
	context.requireDeviceFunctionality("VK_EXT_image_compression_control");

	const deUint32			   width			= 32;
	const deUint32			   height			= 32;
	deUint32				   queueFamilyIndex = context.getUniversalQueueFamilyIndex();
	const vk::DeviceInterface& vkd				= context.getDeviceInterface();
	VkDevice				   device			= context.getDevice();
	tcu::TestLog&			   log				= context.getTestContext().getLog();
	tcu::ResultCollector	   results(log);

	VkImageCompressionFixedRateFlagsEXT planeFlags[3]{};

	for (unsigned i{}; i < (testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT ? 24 : 1); i++)
	{

		planeFlags[0] ^= 3 << i;
		planeFlags[1] ^= 5 << i;
		planeFlags[2] ^= 7 << i;
		if (testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT)
		{
			testParams.control.pFixedRateFlags = planeFlags;
		}
		const vk::VkExternalMemoryImageCreateInfo externalCreateInfo = {
			vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, &testParams.control,
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
		};
		const vk::VkImageCreateInfo createInfo = { vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
												   &externalCreateInfo,
												   0,
												   vk::VK_IMAGE_TYPE_2D,
												   testParams.format,
												   {
													   width,
													   height,
													   1u,
												   },
												   1,
												   1,
												   vk::VK_SAMPLE_COUNT_1_BIT,
												   VK_IMAGE_TILING_OPTIMAL,
												   VK_IMAGE_USAGE_SAMPLED_BIT,
												   vk::VK_SHARING_MODE_EXCLUSIVE,
												   1,
												   &queueFamilyIndex,
												   vk::VK_IMAGE_LAYOUT_UNDEFINED };

		Move<VkImage>			   image		= vk::createImage(vkd, device, &createInfo);
		const VkMemoryRequirements requirements = ExternalMemoryUtil::getImageMemoryRequirements(
			vkd, device, image.get(), VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID);
		const deUint32		 exportedMemoryTypeIndex(ExternalMemoryUtil::chooseMemoryType(requirements.memoryTypeBits));
		Move<VkDeviceMemory> memory = ExternalMemoryUtil::allocateExportableMemory(
			vkd, device, requirements.size, exportedMemoryTypeIndex,
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID, image.get());

		VK_CHECK(vkd.bindImageMemory(device, image.get(), memory.get(), 0u));
		validate(context, results, context.getDevice(), testParams, image.get());
	}
	return tcu::TestStatus(results.getResult(), results.getMessage());
}

static tcu::TestStatus imageCreateTest(Context& context, TestParams testParams)
{
	checkImageCompressionControlSupport(context);
	deUint32			 queueFamilyIndex = context.getUniversalQueueFamilyIndex();
	const VkDevice		 device			  = context.getDevice();
	VkExtent3D			 extent			  = { 16, 16, 1 };
	tcu::TestLog&		 log			  = context.getTestContext().getLog();
	tcu::ResultCollector results(log);

	VkImageCompressionFixedRateFlagsEXT planeFlags[3]{};

	for (unsigned i{}; i < (testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT ? 24 : 1); i++)
	{

		planeFlags[0] ^= 3 << i;
		planeFlags[1] ^= 5 << i;
		planeFlags[2] ^= 7 << i;
		if (testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT)
		{
			testParams.control.pFixedRateFlags = planeFlags;
		}

		VkImageCreateInfo imageCreateInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
			DE_NULL,							 // const void*                  pNext;
			0,									 // VkImageCreateFlags   flags;
			VK_IMAGE_TYPE_2D,					 // VkImageType
			testParams.format,					 // VkFormat format;
			extent,								 // VkExtent3D extent;
			1u,									 // deUint32                             mipLevels;
			1u,									 // deUint32 arraySize;
			VK_SAMPLE_COUNT_1_BIT,				 // deUint32 samples;
			VK_IMAGE_TILING_OPTIMAL,			 // VkImageTiling                tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags    usage;
			VK_SHARING_MODE_EXCLUSIVE,			 // VkSharingMode sharingMode;
			1u,									 // deUint32                             queueFamilyCount;
			&queueFamilyIndex,					 // const deUint32* pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,			 // VkImageLayout initialLayout;
		};

		if (testParams.useExtension)
		{
			imageCreateInfo.pNext = &testParams.control;
		}

		checkImageSupport(context.getInstanceInterface(), context.getPhysicalDevice(), imageCreateInfo);

		Move<VkImage> image = createImage(context.getDeviceInterface(), device, &imageCreateInfo);

		validate(context, results, context.getDevice(), testParams, image.get());
	}
	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void addImageCompressionControlTests(tcu::TestCaseGroup* group, TestParams testParams)
{
	static const struct
	{
		VkFormat begin;
		VkFormat end;
	} s_formatRanges[] = {
		// core formats
		{ (VkFormat)(VK_FORMAT_UNDEFINED + 1), VK_CORE_FORMAT_LAST },

		// YCbCr formats
		{ VK_FORMAT_G8B8G8R8_422_UNORM, (VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM + 1) },

		// YCbCr extended formats
		{ VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT, (VkFormat)(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT + 1) },
	};

	for (int rangeNdx = 0; rangeNdx < DE_LENGTH_OF_ARRAY(s_formatRanges); ++rangeNdx)
	{
		const VkFormat rangeBegin = s_formatRanges[rangeNdx].begin;
		const VkFormat rangeEnd	  = s_formatRanges[rangeNdx].end;

		for (testParams.format = rangeBegin; testParams.format != rangeEnd;
			 testParams.format = (VkFormat)(testParams.format + 1))
		{
			if (isCompressedFormat(testParams.format))
				continue;
			testParams.control.compressionControlPlaneCount =
				isYCbCrFormat(testParams.format) ? getPlaneCount(testParams.format) : 1;
			const char* const enumName = getFormatName(testParams.format);
			const string	  caseName = de::toLower(string(enumName).substr(10));
			addFunctionCase(group, caseName, enumName, imageCreateTest, testParams);
		}
	}
}

tcu::TestCaseGroup* createImageCompressionControlTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(
		new tcu::TestCaseGroup(testCtx, "image_compression_control", "Test for image compression control."));

	TestParams			testParams{};
	tcu::TestCaseGroup* subgroup(
		new tcu::TestCaseGroup(testCtx, "create_image", "Test creating images with compression control struct"));

	subgroup->addChild(createTestGroup(testCtx, "no_compression_control",
									   "Queries images created without compression control struct.",
									   addImageCompressionControlTests, testParams));

	testParams.useExtension	 = true;
	testParams.control		 = initVulkanStructure();
	testParams.control.flags = VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT;

	struct
	{
		const char*				   name;
		VkImageCompressionFlagsEXT flag;
	} compression_flags[] = {
		{ "default", VK_IMAGE_COMPRESSION_DEFAULT_EXT },
		{ "fixed_rate_default", VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT },
		{ "disabled", VK_IMAGE_COMPRESSION_DISABLED_EXT },
		{ "explicit", VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT },
	};

	for (auto& flag : compression_flags)
	{
		testParams.control.flags = flag.flag;
		subgroup->addChild(createTestGroup(testCtx, flag.name,
										   "Queries images created with compression control struct.",
										   addImageCompressionControlTests, testParams));
	}
	group->addChild(subgroup);

	testParams.control.compressionControlPlaneCount = 1;

	subgroup = new tcu::TestCaseGroup(testCtx, "android_hardware_buffer",
									  "Test creating Android Hardware buffer with compression control struct");
	for (auto& flag : compression_flags)
	{
		testParams.control.flags = flag.flag;
		addFunctionCase(subgroup, flag.name, flag.name, ahbImageCreateTest, testParams);
	}
	group->addChild(subgroup);

	return group.release();
}

} // namespace api

} // namespace vkt
