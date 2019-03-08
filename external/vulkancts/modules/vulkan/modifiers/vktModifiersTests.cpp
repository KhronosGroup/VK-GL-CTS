/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Modifiers tests
 *//*--------------------------------------------------------------------*/

#include "vktModifiersTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>

namespace vkt
{
namespace modifiers
{
namespace
{
using namespace vk;
using tcu::UVec2;
using tcu::TestLog;

void checkModifiersSupported (Context& context, VkFormat)
{
	if (!context.isDeviceFunctionalitySupported("VK_EXT_image_drm_format_modifier"))
		TCU_THROW(NotSupportedError, "VK_EXT_image_drm_format_modifier is not supported");

	if (!context.isInstanceFunctionalitySupported("VK_KHR_get_physical_device_properties2"))
		TCU_THROW(TestError, "VK_KHR_get_physical_device_properties2 not supported");

	if (!context.isDeviceFunctionalitySupported("VK_KHR_bind_memory2"))
		TCU_THROW(TestError, "VK_KHR_bind_memory2 not supported");

	if (!context.isDeviceFunctionalitySupported("VK_KHR_image_format_list"))
		TCU_THROW(TestError, "VK_KHR_image_format_list not supported");
}

std::string getFormatCaseName (VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

std::vector<VkDrmFormatModifierPropertiesEXT> getDrmFormatModifiers (const InstanceInterface&	vki,
																	 VkPhysicalDevice			physicalDevice,
																	 VkFormat					format)
{
	VkDrmFormatModifierPropertiesListEXT			modifierProperties {};
	modifierProperties.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
	VkFormatProperties2								formatProperties {};
	std::vector<VkDrmFormatModifierPropertiesEXT>	drmFormatModifiers;
	formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
	formatProperties.pNext = &modifierProperties;

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

	drmFormatModifiers.resize(modifierProperties.drmFormatModifierCount);
	modifierProperties.pDrmFormatModifierProperties = drmFormatModifiers.data();

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

	return drmFormatModifiers;
}

VkImageFormatProperties2 getImageFormatPropertiesForModifier (const InstanceInterface&	vki,
															  VkPhysicalDevice			physicalDevice,
															  const VkFormat*			formats,
															  const deUint32			nFormats,
															  const VkImageType			imageType,
															  const VkImageUsageFlags	imageUsages,
															  const deUint64			drmFormatModifier)
{
	const VkPhysicalDeviceImageDrmFormatModifierInfoEXT	imageFormatModifierInfo	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
		DE_NULL,
		drmFormatModifier,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		DE_NULL,
	};
	const VkImageFormatListCreateInfoKHR				imageFormatListInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
		&imageFormatModifierInfo,
		nFormats,
		formats,
	};
	const VkPhysicalDeviceImageFormatInfo2				imageFormatInfo			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		&imageFormatListInfo,
		formats[0],
		imageType,
		VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
		imageUsages,
		0,
	};
	VkImageFormatProperties2							imageProperties {};
	imageProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;

	if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
	{
		TCU_THROW(NotSupportedError, de::toString(formats[0]) + " does not support any DRM modifiers");
	};

	return imageProperties;
}

tcu::TestStatus listModifiersCase (Context& context, VkFormat format)
{
	TestLog&										log					= context.getTestContext().getLog();
	const InstanceInterface&						vki					= context.getInstanceInterface();
	std::vector<VkDrmFormatModifierPropertiesEXT>	drmFormatModifiers	= getDrmFormatModifiers(vki, context.getPhysicalDevice(), format);

	if (drmFormatModifiers.size() < 1)
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers");

	for (deUint32 m = 0; m < drmFormatModifiers.size(); m++) {
		VkImageFormatProperties2					imageProperties		= getImageFormatPropertiesForModifier(vki, context.getPhysicalDevice(),
																											  &format, 1u, VK_IMAGE_TYPE_2D,
																											  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
																											  drmFormatModifiers[m].drmFormatModifier);

		TCU_CHECK(imageProperties.imageFormatProperties.maxExtent.width >= 1 && imageProperties.imageFormatProperties.maxExtent.height >= 1);
		TCU_CHECK(imageProperties.imageFormatProperties.maxArrayLayers >= 1);

		log << TestLog::Message
			<< "format modifier " << m << ":\n"
			<< drmFormatModifiers[m] << "\n"
			<< imageProperties
			<< TestLog::EndMessage;
	}

	return tcu::TestStatus::pass("OK");
}

Move<VkImage> createImageWithDrmFormatModifiers (const DeviceInterface&			vkd,
												 const VkDevice					device,
												 const VkImageType				imageType,
												 const VkImageUsageFlags		imageUsages,
												 const std::vector<VkFormat>					formats,
												 const UVec2&					size,
												 const std::vector<deUint64>&	drmFormatModifiers)
{
	const VkImageDrmFormatModifierListCreateInfoEXT	modifierListCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
		DE_NULL,
		(deUint32)drmFormatModifiers.size(),
		drmFormatModifiers.data(),
	};
	const VkImageFormatListCreateInfoKHR			imageFormatListInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
		&modifierListCreateInfo,
		static_cast<deUint32>(formats.size()),
		formats.data(),
	};

	const VkImageCreateInfo							createInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&imageFormatListInfo,
		0,
		imageType,
		formats.front(),
		makeExtent3D(size.x(), size.y(), 1u),
		1u,		// mipLevels
		1u,		// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
		imageUsages,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};

	return createImage(vkd, device, &createInfo);
}

tcu::TestStatus createImageListModifiersCase (Context& context, const VkFormat format)
{
	const InstanceInterface&						vki					= context.getInstanceInterface();
	const DeviceInterface&							vkd					= context.getDeviceInterface();
	const VkDevice									device				= context.getDevice();
	std::vector<VkDrmFormatModifierPropertiesEXT>	drmFormatModifiers	= getDrmFormatModifiers(vki, context.getPhysicalDevice(), format);

	if (drmFormatModifiers.size() < 1)
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers");

	for (deUint32 modifierNdx = 0; modifierNdx < drmFormatModifiers.size(); modifierNdx++) {
		VkImageDrmFormatModifierPropertiesEXT	properties;
		std::vector<deUint64>					modifiers;
		bool									found		= false;

		deMemset(&properties, 0, sizeof(properties));
		properties.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;

		for (deUint32 m = 0; m <= modifierNdx; m++)
			modifiers.push_back(drmFormatModifiers[m].drmFormatModifier);

		{
			Move<VkImage>						image		(createImageWithDrmFormatModifiers(vkd, device, VK_IMAGE_TYPE_2D,
																								 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
																								 VK_IMAGE_USAGE_SAMPLED_BIT,
																								 {format}, UVec2(64, 64), modifiers));


			VK_CHECK(vkd.getImageDrmFormatModifierPropertiesEXT(device, *image, &properties));
		}

		for (deUint32 m = 0; m < modifiers.size(); m++)
		{
			if (properties.drmFormatModifier == modifiers[m]) {
				found = true;
				break;
			}
		}

		if (!found)
			return tcu::TestStatus::fail("Image created with modifier not specified in the create list");
	}

	return tcu::TestStatus::pass("OK");
}

} // anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	drmFormatModifiersGroup	(new tcu::TestCaseGroup(testCtx, "drm_format_modifiers", "DRM format modifiers tests"));
	const VkFormat					formats[]				=
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_UINT,
		VK_FORMAT_R64_SINT,
		VK_FORMAT_R64_SFLOAT,
		VK_FORMAT_R64G64_UINT,
		VK_FORMAT_R64G64_SINT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_UINT,
		VK_FORMAT_R64G64B64_SINT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_UINT,
		VK_FORMAT_R64G64B64A64_SINT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
	};

	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "list_modifiers", "Check that listing supported modifiers is functional"));

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
			addFunctionCase(group.get(), getFormatCaseName(formats[formatNdx]), "Check that listing supported modifiers is functional", checkModifiersSupported, listModifiersCase, formats[formatNdx]);

		drmFormatModifiersGroup->addChild(group.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "create_list_modifiers", "Check that creating images with modifier list is functional"));

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
			addFunctionCase(group.get(), getFormatCaseName(formats[formatNdx]), "Check that creating images with modifier list is functional", checkModifiersSupported, createImageListModifiersCase, formats[formatNdx]);

		drmFormatModifiersGroup->addChild(group.release());
	}

	return drmFormatModifiersGroup.release();
}

} // modifiers
} // vkt
