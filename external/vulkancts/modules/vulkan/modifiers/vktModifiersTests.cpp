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
#include "vktExternalMemoryUtil.hpp"
#include "vktImageTestsUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageIO.hpp"
#include "tcuImageCompare.hpp"
#include "tcuMaybe.hpp"
#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

namespace vkt
{
namespace modifiers
{
namespace
{
using namespace vk;
using tcu::UVec2;
using tcu::TestLog;

struct ExplicitModifier
{
	uint64_t				modifier;
	uint32_t				modifierPlaneCount;
	VkSubresourceLayout*	pPlaneLayouts;
};

void checkModifiersSupported (Context& context, VkFormat)
{
	if (!context.isDeviceFunctionalitySupported("VK_EXT_image_drm_format_modifier"))
		TCU_THROW(NotSupportedError, "VK_EXT_image_drm_format_modifier is not supported");

	if (!context.isInstanceFunctionalitySupported("VK_KHR_get_physical_device_properties2"))
		TCU_THROW(NotSupportedError, "VK_KHR_get_physical_device_properties2 not supported");

	if (!context.isDeviceFunctionalitySupported("VK_KHR_bind_memory2"))
		TCU_THROW(NotSupportedError, "VK_KHR_bind_memory2 not supported");

	if (!context.isDeviceFunctionalitySupported("VK_KHR_image_format_list"))
		TCU_THROW(NotSupportedError, "VK_KHR_image_format_list not supported");
}

void checkModifiersList2Supported (Context& context, VkFormat fmt)
{
	checkModifiersSupported(context, fmt);

	if (!context.isDeviceFunctionalitySupported("VK_KHR_format_feature_flags2"))
		TCU_THROW(NotSupportedError, "VK_KHR_format_feature_flags2 not supported");
}

std::string getFormatCaseName (VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
std::vector<ModifierProps> getDrmFormatModifiers (const InstanceInterface&	vki,
												  VkPhysicalDevice			physicalDevice,
												  VkFormat					format)
{
	ModifierList									modifierProperties;
	deMemset(&modifierProperties, 0, sizeof(modifierProperties));

	modifierProperties.sType = modifierListSType;
	VkFormatProperties2								formatProperties;
	deMemset(&formatProperties, 0, sizeof(formatProperties));

	std::vector<ModifierProps>						drmFormatModifiers;
	formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
	formatProperties.pNext = &modifierProperties;

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

	drmFormatModifiers.resize(modifierProperties.drmFormatModifierCount);
	modifierProperties.pDrmFormatModifierProperties = drmFormatModifiers.data();

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);

	return drmFormatModifiers;
}

// Returns true if the image with the given parameters and modifiers supports the given handle type.
bool verifyHandleTypeForFormatModifier (const InstanceInterface&				vki,
									    VkPhysicalDevice						physicalDevice,
									    const VkFormat							format,
									    const VkImageType						imageType,
									    const VkImageUsageFlags					imageUsages,
									    const VkExternalMemoryHandleTypeFlags	handleType,
									    const deUint64							drmFormatModifier)
{
	const VkPhysicalDeviceImageDrmFormatModifierInfoEXT imageFormatModifierInfo =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
		DE_NULL,
		drmFormatModifier,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		DE_NULL,
	};

	const VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		&imageFormatModifierInfo,
		(VkExternalMemoryHandleTypeFlagBits)handleType,
	};

	const VkPhysicalDeviceImageFormatInfo2 imageFormatInfo =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		&externalImageFormatInfo,
		format,
		imageType,
		VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
		imageUsages,
		0,
	};

	VkExternalImageFormatProperties	externalImageProperties	= initVulkanStructure();
	VkImageFormatProperties2		imageProperties			= initVulkanStructure(&externalImageProperties);

	if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		return false;

	if ((externalImageProperties.externalMemoryProperties.compatibleHandleTypes & handleType) != handleType)
		return false;

	return true;
}

template <typename FlagsType>
static deBool featuresCompatible(FlagsType modifierFeatures, VkFormatFeatureFlags testFeatures)
{
	// All the format feature flags alias with their equivalents in the lower
	// 32 bits of VkFormatFeatureFlags2KHR, so as long as we're casting "up",
	// this should always be safe
	DE_STATIC_ASSERT(sizeof(modifierFeatures) >= sizeof(testFeatures));
	return ((modifierFeatures & static_cast<FlagsType>(testFeatures)) == static_cast<FlagsType>(testFeatures));
}

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
std::vector<deUint64> getExportImportCompatibleModifiers (Context& context, VkFormat format)
{
	const auto&				vki					= context.getInstanceInterface();
	const auto				drmFormatModifiers	= getDrmFormatModifiers<ModifierList, ModifierProps, modifierListSType>(vki, context.getPhysicalDevice(), format);
	std::vector<deUint64>	compatibleModifiers;

	if (drmFormatModifiers.empty())
		return compatibleModifiers;

	const VkFormatFeatureFlags testFeatures = (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
									VK_FORMAT_FEATURE_BLIT_SRC_BIT |
									VK_FORMAT_FEATURE_BLIT_DST_BIT |
									VK_FORMAT_FEATURE_TRANSFER_DST_BIT);

	for (const auto& modifierProps : drmFormatModifiers)
	{
		if (modifierProps.drmFormatModifierTilingFeatures == 0)
			TCU_FAIL(de::toString(format) + " does not support any DRM modifier tiling features");

		if (!featuresCompatible(modifierProps.drmFormatModifierTilingFeatures, testFeatures))
			continue;

		const auto&	modifier	= modifierProps.drmFormatModifier;
		const auto	supported	= verifyHandleTypeForFormatModifier(vki, context.getPhysicalDevice(), format,
																	VK_IMAGE_TYPE_2D,
																	(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																	VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																	modifier);

		if (!supported)
			continue;

		compatibleModifiers.push_back(modifier);
	}

	return compatibleModifiers;
}

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
void checkExportImportExtensions (Context& context, VkFormat format)
{
	// tcuTexture.cpp getChannelSize, that is used by intThresholdCompare does not support the following formats.
	// TODO: Add tcuTexture.cpp support for the following formats.
	const VkFormat					skippedFormats[]				=
	{
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
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
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
	};

	if (std::find(std::begin(skippedFormats), std::end(skippedFormats), format) != std::end(skippedFormats))
		TCU_THROW(NotSupportedError, de::toString(format) + " can't be checked for correctness");

	if (!context.isDeviceFunctionalitySupported("VK_KHR_external_memory_fd"))
		TCU_THROW(NotSupportedError, "VK_KHR_external_memory_fd not supported");

	if (modifierListSType == VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT)
		checkModifiersSupported(context, format);
	else
		checkModifiersList2Supported(context, format);

	const auto compatibleModifiers = getExportImportCompatibleModifiers<ModifierList, ModifierProps, modifierListSType>(context, format);
	if (compatibleModifiers.empty())
		TCU_THROW(NotSupportedError, "Could not find a format modifier supporting required transfer features for " + de::toString(format));
}

deBool isModifierCompatibleWithImageProperties (const InstanceInterface&				vki,
												VkPhysicalDevice						physicalDevice,
												const VkFormat*							formats,
												const deUint32							nFormats,
												const VkImageType						imageType,
												const VkImageUsageFlags					imageUsages,
												const VkExternalMemoryHandleTypeFlags	handleType,
												const deUint64							drmFormatModifier,
												VkImageFormatProperties2&				imageProperties)
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

	const VkPhysicalDeviceExternalImageFormatInfo	externalImageFormatInfo =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		&imageFormatModifierInfo,
		(VkExternalMemoryHandleTypeFlagBits)handleType,
	};

	const VkImageFormatListCreateInfo				imageFormatListInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
		&externalImageFormatInfo,
		nFormats,
		formats,
	};

	const VkPhysicalDeviceImageFormatInfo2			imageFormatInfo			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		&imageFormatListInfo,
		formats[0],
		imageType,
		VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
		imageUsages,
		0,
	};

	VkExternalImageFormatProperties	externalImageProperties	= initVulkanStructure();
	imageProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
	imageProperties.pNext = &externalImageProperties;

	if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageFormatInfo, &imageProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		return false;

	if ((externalImageProperties.externalMemoryProperties.compatibleHandleTypes & handleType) != handleType)
		return false;

	return true;
}

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
tcu::TestStatus listModifiersCase (Context& context, VkFormat format)
{
	TestLog&					log					= context.getTestContext().getLog();
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const auto					drmFormatModifiers	= getDrmFormatModifiers<ModifierList, ModifierProps, modifierListSType>(vki, context.getPhysicalDevice(), format);
	bool											noneCompatible		= true;

	if (drmFormatModifiers.empty())
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers");

	for (deUint32 m = 0; m < drmFormatModifiers.size(); m++) {
		VkImageFormatProperties2	imageProperties {};
		deBool						isCompatible	= isModifierCompatibleWithImageProperties(vki, context.getPhysicalDevice(),
																							  &format, 1u, VK_IMAGE_TYPE_2D,
																							  (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																							  VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																							  drmFormatModifiers[m].drmFormatModifier, imageProperties);

		if (drmFormatModifiers[m].drmFormatModifierTilingFeatures == 0)
			TCU_FAIL(de::toString(format) + " does not support any DRM modifier tiling features");

		if (!isCompatible)
			continue;
		noneCompatible = false;

		TCU_CHECK(imageProperties.imageFormatProperties.maxExtent.width >= 1 && imageProperties.imageFormatProperties.maxExtent.height >= 1);
		TCU_CHECK(imageProperties.imageFormatProperties.maxArrayLayers >= 1);

		log << TestLog::Message
			<< "format modifier " << m << ":\n"
			<< drmFormatModifiers[m] << "\n"
			<< imageProperties
			<< TestLog::EndMessage;
	}

	if (noneCompatible)
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers for the requested image features");

	return tcu::TestStatus::pass("OK");
}

Move<VkImage> createImageNoModifiers (const DeviceInterface&			vkd,
												 const VkDevice					device,
												 const VkImageUsageFlags		imageUsages,
												 const VkFormat					format,
												 const UVec2&					size)
{
	const VkImageCreateInfo							createInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		DE_NULL,
		0,
		VK_IMAGE_TYPE_2D,
		format,
		makeExtent3D(size.x(), size.y(), 1u),
		1u,		// mipLevels
		1u,		// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		imageUsages,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		VK_IMAGE_LAYOUT_PREINITIALIZED,
	};

	return createImage(vkd, device, &createInfo);
}

Move<VkImage> createImageWithDrmFormatExplicitModifier (const DeviceInterface&					vkd,
														const VkDevice							device,
														const VkImageType						imageType,
														const VkImageUsageFlags					imageUsages,
														const VkExternalMemoryHandleTypeFlags	externalMemoryHandleTypeFlags,
														const std::vector<VkFormat>&			formats,
														const UVec2&							size,
														const ExplicitModifier					drmFormatModifier)
{
	const VkImageDrmFormatModifierExplicitCreateInfoEXT	modifierExplicitCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
		DE_NULL,
		drmFormatModifier.modifier,
		drmFormatModifier.modifierPlaneCount,
		drmFormatModifier.pPlaneLayouts,
	};

	const VkExternalMemoryImageCreateInfo externalMemoryCreateInfo =
	{
		VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		&modifierExplicitCreateInfo,
		externalMemoryHandleTypeFlags,
	};

	const void* pNext = &externalMemoryCreateInfo;
	if (!externalMemoryHandleTypeFlags)
	{
		pNext = &modifierExplicitCreateInfo;
	}

	const VkImageFormatListCreateInfo			imageFormatListInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
		pNext,
		de::sizeU32(formats),
		de::dataOrNull(formats),
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
		nullptr,
		VK_IMAGE_LAYOUT_UNDEFINED,
	};

	return createImage(vkd, device, &createInfo);
}

Move<VkImage> createImageWithDrmFormatModifiers (const DeviceInterface&					vkd,
												 const VkDevice							device,
												 const VkImageType						imageType,
												 const VkImageUsageFlags				imageUsages,
												 const VkExternalMemoryHandleTypeFlags	externalMemoryHandleTypeFlags,
												 const std::vector<VkFormat>&			formats,
												 const UVec2&							size,
												 const std::vector<deUint64>&			drmFormatModifiers)
{
	const VkImageDrmFormatModifierListCreateInfoEXT	modifierListCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
		DE_NULL,
		(deUint32)drmFormatModifiers.size(),
		drmFormatModifiers.data(),
	};

	const VkExternalMemoryImageCreateInfo externalMemoryCreateInfo =
	{
		VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		&modifierListCreateInfo,
		externalMemoryHandleTypeFlags,
	};

	const void* pNext = &externalMemoryCreateInfo;
	if (!externalMemoryHandleTypeFlags)
	{
		pNext = &modifierListCreateInfo;
	}

	const VkImageFormatListCreateInfo			imageFormatListInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
		pNext,
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

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
tcu::TestStatus createImageListModifiersCase (Context& context, const VkFormat format)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vkd					= context.getDeviceInterface();
	const VkDevice				device				= context.getDevice();
	const auto					drmFormatModifiers	= getDrmFormatModifiers<ModifierList, ModifierProps, modifierListSType>(vki, context.getPhysicalDevice(), format);

	if (drmFormatModifiers.empty())
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers");

	// Get the list of modifiers supported for some specific image parameters.
	std::vector<deUint64> modifiers;

	for (const auto& modProps : drmFormatModifiers)
	{
		VkImageFormatProperties2	imgFormatProperties	= initVulkanStructure();
		const auto					isCompatible		= isModifierCompatibleWithImageProperties(vki, context.getPhysicalDevice(), &format, 1u, VK_IMAGE_TYPE_2D,
																								  (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																								  VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																								  modProps.drmFormatModifier, imgFormatProperties);
		if (isCompatible)
			modifiers.push_back(modProps.drmFormatModifier);
		if (modProps.drmFormatModifierTilingFeatures == 0)
			TCU_FAIL(de::toString(format) + " does not support any DRM modifier tiling features");
	}

	if (modifiers.empty())
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers for the requested image features");

	// Test with lists of compatible modifiers of increasing lengths.
	for (size_t len = 1u; len <= modifiers.size(); ++len)
	{
		std::vector<deUint64> creationModifiers;
		creationModifiers.reserve(len);
		std::copy_n(begin(modifiers), len, std::back_inserter(creationModifiers));

		VkImageDrmFormatModifierPropertiesEXT	properties	= initVulkanStructure();

		{
			std::vector<VkFormat>	formats	(1u, format);
			const auto				image	= createImageWithDrmFormatModifiers(vkd, device, VK_IMAGE_TYPE_2D,
																				(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																				0, formats, UVec2(64, 64), creationModifiers);

			VK_CHECK(vkd.getImageDrmFormatModifierPropertiesEXT(device, *image, &properties));
		}

		if (!de::contains(begin(creationModifiers), end(creationModifiers), properties.drmFormatModifier))
			return tcu::TestStatus::fail("Image created with modifier not specified in the create list");
	}

	return tcu::TestStatus::pass("OK");
}

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
tcu::TestStatus createImageModifierExplicitCase (Context& context, const VkFormat format)
{
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const DeviceInterface&		vkd					= context.getDeviceInterface();
	const VkDevice				device				= context.getDevice();
	const auto					drmFormatModifiers	= getDrmFormatModifiers<ModifierList, ModifierProps, modifierListSType>(vki, context.getPhysicalDevice(), format);

	if (drmFormatModifiers.empty())
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers");

	// Get the list of modifiers supported for some specific image parameters.
	std::vector<ExplicitModifier> modifiers;

	for (const auto& modProps : drmFormatModifiers)
	{
		if (modProps.drmFormatModifierTilingFeatures == 0)
			TCU_FAIL(de::toString(format) + " does not support any DRM modifier tiling features");

		VkImageFormatProperties2	imgFormatProperties	= initVulkanStructure();
		const auto					isCompatible		= isModifierCompatibleWithImageProperties(vki, context.getPhysicalDevice(), &format, 1u, VK_IMAGE_TYPE_2D,
																								  (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																								  VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																								  modProps.drmFormatModifier, imgFormatProperties);
		if (isCompatible)
		{
			const ExplicitModifier modifier =
			{
				modProps.drmFormatModifier,				// modifier
				modProps.drmFormatModifierPlaneCount,	// modifierPlaneCount
				DE_NULL,								// pPlaneLayouts
			};

			modifiers.push_back(modifier);
		}
	}

	if (modifiers.empty())
		TCU_THROW(NotSupportedError, de::toString(format) + " does not support any DRM modifiers for the requested image features");

	for (auto& modifier : modifiers)
	{
		std::vector<VkFormat> formats			(1u, format);
		std::vector<uint64_t> creationModifier	(1u, modifier.modifier);

		VkImageDrmFormatModifierPropertiesEXT properties = initVulkanStructure();

		const auto imageRef = createImageWithDrmFormatModifiers(vkd, device, VK_IMAGE_TYPE_2D,
																(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																0, formats, UVec2(64, 64), creationModifier);

		std::vector<VkSubresourceLayout>	planeLayouts;
		for (uint32_t i = 0; i < modifier.modifierPlaneCount; i++)
		{
			VkImageSubresource imageSubresource;
			VkSubresourceLayout subresourceLayout;

			deMemset(&imageSubresource, 0, sizeof(imageSubresource));

			imageSubresource.aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i;

			vkd.getImageSubresourceLayout(device, *imageRef, &imageSubresource, &subresourceLayout);

			// From the spec:
			//   VUID-VkImageDrmFormatModifierExplicitCreateInfoEXT-size-02267
			//   For each element of pPlaneLayouts, size must be 0
			//
			//   VUID-VkImageDrmFormatModifierExplicitCreateInfoEXT-arrayPitch-02268
			//   For each element of pPlaneLayouts, arrayPitch must be 0 if VkImageCreateInfo::arrayLayers is 1
			//
			//   VUID-VkImageDrmFormatModifierExplicitCreateInfoEXT-depthPitch-02269
			//   For each element of pPlaneLayouts, depthPitch must be 0 if VkImageCreateInfo::extent.depth is 1
			subresourceLayout.size = 0;
			subresourceLayout.arrayPitch = 0;
			subresourceLayout.depthPitch = 0;

			planeLayouts.push_back(subresourceLayout);

		}
		modifier.pPlaneLayouts = planeLayouts.data();

		const auto image = createImageWithDrmFormatExplicitModifier(vkd, device, VK_IMAGE_TYPE_2D,
																    (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
																    0, formats, UVec2(64, 64), modifier);
		VK_CHECK(vkd.getImageDrmFormatModifierPropertiesEXT(device, *image, &properties));

		if (modifier.modifier != properties.drmFormatModifier)
			return tcu::TestStatus::fail("The created image's modifier with an explicit modifier not matched");
	}

	return tcu::TestStatus::pass("OK");
}

deUint32 chooseMemoryType(deUint32 bits)
{
	DE_ASSERT(bits != 0);

	for (deUint32 memoryTypeIndex = 0; (1u << memoryTypeIndex) <= bits; memoryTypeIndex++)
	{
		if ((bits & (1u << memoryTypeIndex)) != 0)
			return memoryTypeIndex;
	}

	DE_FATAL("No supported memory types");
	return -1;
}

bool exportImportMemoryExplicitModifiersCase (Context& context, const VkFormat format, deUint64 modifier)
{
	const InstanceInterface&						vki					= context.getInstanceInterface();
	const DeviceInterface&							vkd					= context.getDeviceInterface();
	const VkDevice									device				= context.getDevice();


	const auto supported = verifyHandleTypeForFormatModifier(vki, context.getPhysicalDevice(), format,
															 VK_IMAGE_TYPE_2D,
															 (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
															 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
															 modifier);

	if (!supported)
		TCU_FAIL("Modifier " + de::toString(modifier) + " for format " + de::toString(format) + " expected to be compatible");

	std::vector<deUint64>					modifiers;
	modifiers.push_back(modifier);


	const UVec2									imageSize		(64, 64);
	const tcu::TextureFormat referenceTextureFormat (mapVkFormat(format));
	deUint32 bufferSize = 1<<16;
	const de::UniquePtr<BufferWithMemory>		inputBuffer		(new BufferWithMemory(vkd, device, context.getDefaultAllocator(),
																																							makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
																						MemoryRequirement::HostVisible));
	tcu::PixelBufferAccess				referenceImage	(referenceTextureFormat, imageSize.x(), imageSize.y(), 1, inputBuffer->getAllocation().getHostPtr());
	const de::UniquePtr<BufferWithMemory>		outputBuffer		(new BufferWithMemory(vkd, device, context.getDefaultAllocator(),
																																							makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
																						MemoryRequirement::HostVisible));
	Unique<VkCommandPool>			cmdPool				(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, context.getUniversalQueueFamilyIndex(), DE_NULL));
	vkt::ExternalMemoryUtil::NativeHandle								inputImageMemFd;

	const tcu::TextureFormatInfo				formatInfo		(tcu::getTextureFormatInfo(referenceTextureFormat));
	tcu::fillWithComponentGradients(referenceImage, formatInfo.valueMin, formatInfo.valueMax);

	flushAlloc(vkd, device, inputBuffer->getAllocation());

	Move<VkImage>						srcImage		(createImageNoModifiers(vkd, device,
																								 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
																								 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																								 format, UVec2(64, 64)));
	VkMemoryRequirements srcImageMemoryReq = getImageMemoryRequirements(vkd, device, *srcImage);
	const vk::VkMemoryAllocateInfo	allocationInfo	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		DE_NULL,
		srcImageMemoryReq.size,
		chooseMemoryType(srcImageMemoryReq.memoryTypeBits),
	};
	vk::Move<vk::VkDeviceMemory>	srcMemory			(vk::allocateMemory(vkd, device, &allocationInfo));
	VK_CHECK(vkd.bindImageMemory(device, *srcImage, *srcMemory, 0));


	Unique<VkCommandBuffer>			cmdBuffer			(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const VkCommandBufferBeginInfo	cmdBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};

	VK_CHECK(vkd.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));

	{
		const VkImageAspectFlags		aspect				= VK_IMAGE_ASPECT_COLOR_BIT;
		std::vector<VkBufferImageCopy>	copies;

		copies.push_back(image::makeBufferImageCopy(makeExtent3D(imageSize.x(), imageSize.y(), 1u), 1u));
		copyBufferToImage(vkd, *cmdBuffer, inputBuffer->get(), bufferSize,
											copies, aspect, 1, 1, *srcImage,
											VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	}

	Move<VkImage>						dstImage		(createImageWithDrmFormatModifiers(vkd, device, VK_IMAGE_TYPE_2D,
																																				 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
																																				 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																																				 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																																				 {format}, UVec2(64, 64), modifiers));
	VkMemoryRequirements dstImageMemoryReq = getImageMemoryRequirements(vkd, device, *dstImage);
	vk::Move<vk::VkDeviceMemory>	dstMemory			(vkt::ExternalMemoryUtil::allocateExportableMemory(vkd, device,
																																																 dstImageMemoryReq.size,
																																																 chooseMemoryType(dstImageMemoryReq.memoryTypeBits),
																																																 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																																																 *dstImage));

	VK_CHECK(vkd.bindImageMemory(device, *dstImage, *dstMemory, 0));
	const VkImageMemoryBarrier		srcImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*srcImage,						// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};
	const VkImageMemoryBarrier		dstImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*dstImage,						// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};
	vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &srcImageBarrier);
	vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &dstImageBarrier);

	VkImageBlit		imageBlit
	{
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{{0,0,0}, {64,64,1}},
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{{0,0,0}, {64,64,1}},
	};
	vkd.cmdBlitImage(*cmdBuffer, *srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);

	const VkImageMemoryBarrier		exportImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
		context.getUniversalQueueFamilyIndex(),		// deUint32					dstQueueFamilyIndex;
		VK_QUEUE_FAMILY_FOREIGN_EXT,				// deUint32					srcQueueFamilyIndex;
		*dstImage,									// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags		aspectMask;
			0u,								// deUint32					baseMipLevel;
			1u,								// deUint32					mipLevels;
			0u,								// deUint32					baseArraySlice;
			1u								// deUint32					arraySize;
		}
	};

	vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &exportImageBarrier);
	VK_CHECK(vkd.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vkd, device, context.getUniversalQueue(), *cmdBuffer);
	VkImageDrmFormatModifierPropertiesEXT	properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT;
	VK_CHECK(vkd.getImageDrmFormatModifierPropertiesEXT(device, *dstImage, &properties));
	TCU_CHECK(properties.drmFormatModifier == modifiers.front());
	inputImageMemFd	= vkt::ExternalMemoryUtil::getMemoryFd(vkd, device, *dstMemory, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);

	Move<VkImage>				importedSrcImage		(createImageWithDrmFormatModifiers(vkd, device, VK_IMAGE_TYPE_2D,
																																						 VK_IMAGE_USAGE_TRANSFER_DST_BIT |
																																						 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																																						 VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
																																						 {format}, UVec2(64, 64), modifiers));

	VkMemoryRequirements importedSrcImageMemoryReq = getImageMemoryRequirements(vkd, device, *importedSrcImage);

	Move<VkDeviceMemory>						importedMemory (vkt::ExternalMemoryUtil::importDedicatedMemory(vkd, device, *importedSrcImage,
                                                                                                               importedSrcImageMemoryReq,
                                                                                                               VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                                                                                                               ~0u, inputImageMemFd));
	VK_CHECK(vkd.bindImageMemory(device, *importedSrcImage, *importedMemory, 0));

	Move<VkImage>						outImage		(createImageNoModifiers(vkd, device,
																															VK_IMAGE_USAGE_TRANSFER_DST_BIT |
																															VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
																															format, UVec2(64, 64)));
	VkMemoryRequirements outImageMemoryReq = getImageMemoryRequirements(vkd, device, *outImage);
	const vk::VkMemoryAllocateInfo	outAllocationInfo	=
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		DE_NULL,
		outImageMemoryReq.size,
		chooseMemoryType(outImageMemoryReq.memoryTypeBits),
	};
	vk::Move<vk::VkDeviceMemory>	outMemory			(vk::allocateMemory(vkd, device, &outAllocationInfo));
	VK_CHECK(vkd.bindImageMemory(device, *outImage, *outMemory, 0));

	Unique<VkCommandBuffer>			cmdBuffer2			(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	VK_CHECK(vkd.beginCommandBuffer(*cmdBuffer2, &cmdBufferBeginInfo));

	const VkImageMemoryBarrier		importedImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_FOREIGN_EXT,					// deUint32					srcQueueFamilyIndex;
		context.getUniversalQueueFamilyIndex(),			// deUint32					dstQueueFamilyIndex;
		*importedSrcImage,						// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};
	const VkImageMemoryBarrier		outImageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,		// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		*outImage,						// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			0u,								// deUint32				baseMipLevel;
			1u,								// deUint32				mipLevels;
			0u,								// deUint32				baseArraySlice;
			1u								// deUint32				arraySize;
		}
	};

	vkd.cmdPipelineBarrier(*cmdBuffer2, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &importedImageBarrier);
	vkd.cmdPipelineBarrier(*cmdBuffer2, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &outImageBarrier);

	VkImageBlit		imageBlit2
	{
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{{0,0,0}, {64,64,1}},
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		{{0,0,0}, {64,64,1}},
	};
	vkd.cmdBlitImage(*cmdBuffer2, *importedSrcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit2, VK_FILTER_NEAREST);


	copyImageToBuffer(vkd, *cmdBuffer2, *outImage,
										outputBuffer->get(), tcu::IVec2(imageSize.x(), imageSize.y()),
										VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

	VK_CHECK(vkd.endCommandBuffer(*cmdBuffer2));

	submitCommandsAndWait(vkd, device, context.getUniversalQueue(), *cmdBuffer2);


	tcu::ConstPixelBufferAccess	result	(referenceTextureFormat, imageSize.x(), imageSize.y(), 1, outputBuffer->getAllocation().getHostPtr());
	const tcu::UVec4 threshold (0u);

	invalidateAlloc(vkd, device, outputBuffer->getAllocation());

	return tcu::intThresholdCompare(context.getTestContext().getLog(), "Compare", "Result comparison", referenceImage, result, threshold, tcu::COMPARE_LOG_RESULT);
}

template <typename ModifierList, typename ModifierProps, VkStructureType modifierListSType>
tcu::TestStatus exportImportMemoryExplicitModifiersCase (Context& context, const VkFormat format)
{
	const auto compatibleModifiers = getExportImportCompatibleModifiers<ModifierList, ModifierProps, modifierListSType>(context, format);

	if (compatibleModifiers.empty())
		TCU_FAIL("Expected non-empty list of compatible modifiers for the given format");

	for (const auto& modifier : compatibleModifiers)
	{
		if (!exportImportMemoryExplicitModifiersCase(context, format, modifier))
			return tcu::TestStatus::fail("Unexpected copy image result");
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
		VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT,
		VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT,
	};

	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "list_modifiers", "Check that listing supported modifiers is functional"));
		de::MovePtr<tcu::TestCaseGroup> group2(new tcu::TestCaseGroup(testCtx, "list_modifiers_fmt_features2", "Check that listing supported modifiers is functional with VK_KHR_format_feature_flags2"));

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			addFunctionCase(group.get(), getFormatCaseName(formats[formatNdx]), "Check that listing supported modifiers is functional", checkModifiersSupported, listModifiersCase<VkDrmFormatModifierPropertiesListEXT, VkDrmFormatModifierPropertiesEXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT>, formats[formatNdx]);
			addFunctionCase(group2.get(), getFormatCaseName(formats[formatNdx]), "Check that listing supported modifiers is functional", checkModifiersList2Supported, listModifiersCase<VkDrmFormatModifierPropertiesList2EXT, VkDrmFormatModifierProperties2EXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT>, formats[formatNdx]);
		}

		drmFormatModifiersGroup->addChild(group.release());
		drmFormatModifiersGroup->addChild(group2.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "create_list_modifiers", "Check that creating images with modifier list is functional"));
		de::MovePtr<tcu::TestCaseGroup> group2(new tcu::TestCaseGroup(testCtx, "create_list_modifiers_fmt_features2", "Check that creating images with modifier list is functional with VK_KHR_format_feature_flags2"));

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			addFunctionCase(group.get(), getFormatCaseName(formats[formatNdx]), "Check that creating images with modifier list is functional", checkModifiersSupported, createImageListModifiersCase<VkDrmFormatModifierPropertiesListEXT, VkDrmFormatModifierPropertiesEXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT>, formats[formatNdx]);
			addFunctionCase(group2.get(), getFormatCaseName(formats[formatNdx]), "Check that creating images with modifier list is functional", checkModifiersList2Supported, createImageListModifiersCase<VkDrmFormatModifierPropertiesList2EXT, VkDrmFormatModifierProperties2EXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT>, formats[formatNdx]);
		}

		drmFormatModifiersGroup->addChild(group.release());
		drmFormatModifiersGroup->addChild(group2.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "create_explicit_modifier", "Check that creating images with an explicit modifier is functional"));
		de::MovePtr<tcu::TestCaseGroup> group2(new tcu::TestCaseGroup(testCtx, "create_explicit_modifier_fmt_features2", "Check that creating images with an explicit modifier is functional with VK_KHR_format_feature_flags2"));

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			addFunctionCase(group.get(), getFormatCaseName(formats[formatNdx]), "Check that creating images with an explicit modifier is functional", checkModifiersSupported, createImageModifierExplicitCase<VkDrmFormatModifierPropertiesListEXT, VkDrmFormatModifierPropertiesEXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT>, formats[formatNdx]);
			addFunctionCase(group2.get(), getFormatCaseName(formats[formatNdx]), "Check that creating images with an explicit modifier is functional", checkModifiersList2Supported, createImageModifierExplicitCase<VkDrmFormatModifierPropertiesList2EXT, VkDrmFormatModifierProperties2EXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT>, formats[formatNdx]);
		}

		drmFormatModifiersGroup->addChild(group.release());
		drmFormatModifiersGroup->addChild(group2.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "export_import", "Test exporting/importing images with modifiers"));
		de::MovePtr<tcu::TestCaseGroup> group2(new tcu::TestCaseGroup(testCtx, "export_import_fmt_features2", "Test exporting/importing images with modifiers with VK_KHR_format_feature_flags2"));

		for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			addFunctionCase(group.get(), getFormatCaseName(formats[formatNdx]), "Test exporting/importing images with modifiers", checkExportImportExtensions<VkDrmFormatModifierPropertiesListEXT, VkDrmFormatModifierPropertiesEXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT>, exportImportMemoryExplicitModifiersCase<VkDrmFormatModifierPropertiesListEXT, VkDrmFormatModifierPropertiesEXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT>, formats[formatNdx]);
			addFunctionCase(group2.get(), getFormatCaseName(formats[formatNdx]), "Test exporting/importing images with modifiers", checkExportImportExtensions<VkDrmFormatModifierPropertiesList2EXT, VkDrmFormatModifierProperties2EXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT>, exportImportMemoryExplicitModifiersCase<VkDrmFormatModifierPropertiesList2EXT, VkDrmFormatModifierProperties2EXT, VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT>, formats[formatNdx]);
		}

		drmFormatModifiersGroup->addChild(group.release());
		drmFormatModifiersGroup->addChild(group2.release());
	}

	return drmFormatModifiersGroup.release();
}

} // modifiers
} // vkt
