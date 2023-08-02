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
#include "wsi/vktNativeObjectsUtil.hpp"

#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkWsiUtil.hpp"

#include "deString.h"
#include "deStringUtil.hpp"

#include <map>
#include <vector>

using namespace vk;
using namespace vk::wsi;
using namespace std;

typedef vector<VkExtensionProperties> Extensions;

namespace vkt
{

namespace api
{

struct TestParams
{
	VkFormat					 format;
	bool						 useExtension;
	VkImageCompressionControlEXT control;
	Type						 wsiType;
};

static void checkImageCompressionControlSupport(Context& context, bool swapchain = false)
{
	context.requireDeviceFunctionality("VK_EXT_image_compression_control");
	vk::VkPhysicalDeviceImageCompressionControlFeaturesEXT imageCompressionControlFeatures	  = initVulkanStructure();
	vk::VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT imageCompressionSwapchain = initVulkanStructure();
	vk::VkPhysicalDeviceFeatures2 features2 = initVulkanStructure(&imageCompressionControlFeatures);
	if (swapchain)
	{
		context.requireDeviceFunctionality("VK_EXT_image_compression_control_swapchain");
		imageCompressionControlFeatures.pNext = &imageCompressionSwapchain;
	}

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (!imageCompressionControlFeatures.imageCompressionControl)
		TCU_THROW(NotSupportedError, "VK_EXT_image_compression_control Image "
									 "compression control feature not supported.");
	if (swapchain && !imageCompressionSwapchain.imageCompressionControlSwapchain)
		TCU_THROW(NotSupportedError, "VK_EXT_image_compression_control_swapchain Image "
									 "compression control feature for swapchains not supported.");
}

static void validate(const InstanceInterface& vki, const DeviceInterface& vkd, tcu::ResultCollector& results,
					 VkPhysicalDevice physicalDevice, VkDevice device, TestParams& testParams, VkImage image)
{
	constexpr VkImageAspectFlags planeAspects[]{ VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_ASPECT_PLANE_1_BIT,
												 VK_IMAGE_ASPECT_PLANE_2_BIT };
	const bool isYCbCr   = isYCbCrFormat(testParams.format);
	const int  numPlanes = isYCbCr ? getPlaneCount(testParams.format) : 1;
	for (int planeIndex = 0; planeIndex < numPlanes; planeIndex++)
	{
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		if (isYCbCr)
		{
			aspect = planeAspects[planeIndex];
		}

		VkImageCompressionPropertiesEXT compressionProperties = initVulkanStructure();
		VkImageSubresource2EXT			subresource			  = initVulkanStructure();
		subresource.imageSubresource.aspectMask				  = aspect;
		VkSubresourceLayout2EXT subresourceLayout			  = initVulkanStructure(&compressionProperties);
		vkd.getImageSubresourceLayout2KHR(device, image, &subresource, &subresourceLayout);

		VkImageCompressionControlEXT compressionEnabled		  = initVulkanStructure();
		compressionEnabled.compressionControlPlaneCount		  = testParams.control.compressionControlPlaneCount;
		compressionEnabled.flags							  = testParams.control.flags;
		VkImageCompressionFixedRateFlagsEXT fixedRateFlags[3] = {
			VK_IMAGE_COMPRESSION_FIXED_RATE_FLAG_BITS_MAX_ENUM_EXT,
			VK_IMAGE_COMPRESSION_FIXED_RATE_FLAG_BITS_MAX_ENUM_EXT,
			VK_IMAGE_COMPRESSION_FIXED_RATE_FLAG_BITS_MAX_ENUM_EXT
		};

		if (compressionEnabled.compressionControlPlaneCount > 0)
		{
			compressionEnabled.pFixedRateFlags = fixedRateFlags;
		}

		VkPhysicalDeviceImageFormatInfo2 formatInfo = initVulkanStructure(&compressionEnabled);
		formatInfo.format							= testParams.format;
		formatInfo.type								= VK_IMAGE_TYPE_2D;
		formatInfo.tiling							= VK_IMAGE_TILING_OPTIMAL;
		formatInfo.usage							= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkImageCompressionPropertiesEXT compressionPropertiesSupported = initVulkanStructure();
		VkImageFormatProperties2		properties2 = initVulkanStructure(&compressionPropertiesSupported);

		vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &formatInfo, &properties2);

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

static void checkAhbImageSupport (const Context& context, const TestParams testParams, const deUint32 width, const deUint32	height, const VkImageUsageFlagBits vkUsage)
{
	using namespace vkt::ExternalMemoryUtil;

	// Check android hardware buffer can be allocated for the format with usage.
	AndroidHardwareBufferExternalApi* ahbApi = AndroidHardwareBufferExternalApi::getInstance();
	if (!ahbApi)
	{
		TCU_THROW(NotSupportedError, "Platform doesn't support Android Hardware Buffer handles");
	}
	deUint64 ahbUsage =  ahbApi->vkUsageToAhbUsage(vkUsage);
	{
		pt::AndroidHardwareBufferPtr ahb = ahbApi->allocate(width,height, 1, ahbApi->vkFormatToAhbFormat(testParams.format), ahbUsage);
		if (ahb.internal == DE_NULL)
		{
			TCU_THROW(NotSupportedError, "Android hardware buffer format not supported");
		}
	}

	// Check external memory supported.
	const VkPhysicalDeviceExternalImageFormatInfoKHR external_image_format_info =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		&testParams.control,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
	};

	const VkPhysicalDeviceImageFormatInfo2			info				=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		&external_image_format_info,
		testParams.format,
		VK_IMAGE_TYPE_2D,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		0,
	};

	VkImageCompressionPropertiesEXT compressionPropertiesSupported =
	{
		VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT,
		DE_NULL,
		0,
		0
	};

	VkAndroidHardwareBufferUsageANDROID		ahbUsageProperties	=
	{
		VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID,
		&compressionPropertiesSupported,
		0u
	};

	VkExternalImageFormatProperties					externalProperties	=
	{
		VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
		&ahbUsageProperties,
		{ 0u, 0u, 0u }
	};

	VkImageFormatProperties2						properties			=
	{
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		&externalProperties,
		{
			{ 0u, 0u, 0u },
			0u,
			0u,
			0u,
			0u
		}
	};

	VkResult result = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(context.getPhysicalDevice(), &info, &properties);

	if(result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Format not supported");

	if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) == 0)
		TCU_THROW(NotSupportedError, "External handle type doesn't support exporting image");

	if ((externalProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) == 0)
		TCU_THROW(NotSupportedError, "External handle type requires dedicated allocation");

	if((compressionPropertiesSupported.imageCompressionFlags == VK_IMAGE_COMPRESSION_DISABLED_EXT)
		&& (testParams.control.flags != VK_IMAGE_COMPRESSION_DISABLED_EXT))
	{
		TCU_THROW(NotSupportedError, "Compression is disbaled, and other compression flags are not supported");
	}

	if((ahbUsageProperties.androidHardwareBufferUsage & ahbUsage) != ahbUsage)
	{
		TCU_THROW(NotSupportedError, "Android hardware buffer usage is not supported");
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
	const VkImageUsageFlagBits vkUsage			= VK_IMAGE_USAGE_SAMPLED_BIT;
	const bool				   is_fixed_rate_ex = testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT;
	const uint32_t			   numPlanes		= isYCbCrFormat(testParams.format) ? getPlaneCount(testParams.format) : 1;

	testParams.control.compressionControlPlaneCount = is_fixed_rate_ex ? numPlanes : 0;

	VkImageCompressionFixedRateFlagsEXT planeFlags[3]{};

	for (unsigned i{}; i < (is_fixed_rate_ex ? 24 : 1); i++)
	{
		planeFlags[0] ^= 3 << i;
		planeFlags[1] ^= 5 << i;
		planeFlags[2] ^= 7 << i;

		if (is_fixed_rate_ex)
		{
			testParams.control.compressionControlPlaneCount = numPlanes;
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
												   vkUsage,
												   vk::VK_SHARING_MODE_EXCLUSIVE,
												   1,
												   &queueFamilyIndex,
												   vk::VK_IMAGE_LAYOUT_UNDEFINED };

		checkAhbImageSupport(context, testParams, width, height, vkUsage);

		Move<VkImage>			   image		= vk::createImage(vkd, device, &createInfo);
		const VkMemoryRequirements requirements = ExternalMemoryUtil::getImageMemoryRequirements(
			vkd, device, image.get(), VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID);
		const deUint32		 exportedMemoryTypeIndex(ExternalMemoryUtil::chooseMemoryType(requirements.memoryTypeBits));
		Move<VkDeviceMemory> memory = ExternalMemoryUtil::allocateExportableMemory(
			vkd, device, requirements.size, exportedMemoryTypeIndex,
			VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID, image.get());

		VK_CHECK(vkd.bindImageMemory(device, image.get(), memory.get(), 0u));
		validate(context.getInstanceInterface(), context.getDeviceInterface(), results, context.getPhysicalDevice(),
				 context.getDevice(), testParams, image.get());
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
	const bool			 is_fixed_rate_ex = testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT;

	VkImageCompressionFixedRateFlagsEXT planeFlags[3]{};

	for (unsigned i{}; i < (is_fixed_rate_ex ? 24 : 1); i++)
	{
		planeFlags[0] ^= 3 << i;
		planeFlags[1] ^= 5 << i;
		planeFlags[2] ^= 7 << i;

		if (is_fixed_rate_ex)
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

		validate(context.getInstanceInterface(), context.getDeviceInterface(), results, context.getPhysicalDevice(),
				 context.getDevice(), testParams, image.get());
	}
	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void addImageCompressionControlTests(tcu::TestCaseGroup* group, TestParams testParams)
{
	const bool is_fixed_rate_ex = testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT;

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

			const uint32_t numPlanes = isYCbCrFormat(testParams.format) ? getPlaneCount(testParams.format) : 1;
			testParams.control.compressionControlPlaneCount = is_fixed_rate_ex ? numPlanes : 0;

			const char* const enumName = getFormatName(testParams.format);
			const string	  caseName = de::toLower(string(enumName).substr(10));
			addFunctionCase(group, caseName, enumName, imageCreateTest, testParams);
		}
	}
}

CustomInstance createInstanceWithWsi(Context& context, Type wsiType, const vector<string> extraExtensions,
									 const VkAllocationCallbacks* pAllocator = DE_NULL)
{
	const deUint32 version	  = context.getUsedApiVersion();
	vector<string> extensions = extraExtensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));
	extensions.push_back("VK_KHR_get_surface_capabilities2");

	vector<string> instanceExtensions;
	for (const auto& ext : extensions)
	{
		if (!context.isInstanceFunctionalitySupported(ext))
			TCU_THROW(NotSupportedError, (ext + " is not supported").c_str());

		if (!isCoreInstanceExtension(version, ext))
			instanceExtensions.push_back(ext);
	}

	return vkt::createCustomInstanceWithExtensions(context, instanceExtensions, pAllocator);
}
struct InstanceHelper
{
	const vector<VkExtensionProperties> supportedExtensions;
	CustomInstance						instance;
	const InstanceDriver&				vki;

	InstanceHelper(Context& context, Type wsiType, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL))
		, instance(createInstanceWithWsi(context, wsiType, vector<string>(), pAllocator))
		, vki(instance.getDriver())
	{
	}

	InstanceHelper(Context& context, Type wsiType, const vector<string>& extensions,
				   const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL))
		, instance(createInstanceWithWsi(context, wsiType, extensions, pAllocator))
		, vki(instance.getDriver())
	{
	}
};

Move<VkDevice> createDeviceWithWsi(const PlatformInterface& vkp, deUint32 apiVersion, VkInstance instance,
								   const InstanceInterface& vki, VkPhysicalDevice physicalDevice,
								   const Extensions& supportedExtensions, const vector<string>& additionalExtensions,
								   deUint32 queueFamilyIndex, bool validationEnabled,
								   const VkAllocationCallbacks* pAllocator = DE_NULL)
{
	const float					  queuePriorities[] = { 1.0f };
	const VkDeviceQueueCreateInfo queueInfo			= {
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		nullptr,
		(VkDeviceQueueCreateFlags)0,
		queueFamilyIndex,
		DE_LENGTH_OF_ARRAY(queuePriorities),
		&queuePriorities[0],
	};

	vector<string> extensions;
	extensions.push_back("VK_KHR_swapchain");
	extensions.push_back("VK_EXT_image_compression_control");
	extensions.push_back("VK_EXT_image_compression_control_swapchain");
	extensions.insert(end(extensions), begin(additionalExtensions), end(additionalExtensions));

	for (const auto& extName : extensions)
	{
		if (!isCoreDeviceExtension(apiVersion, extName) &&
			!isExtensionStructSupported(supportedExtensions, RequiredExtension(extName)))
			TCU_THROW(NotSupportedError, extName + " is not supported");
	}

	vk::VkPhysicalDeviceImageCompressionControlSwapchainFeaturesEXT imageCompressionSwapchain = initVulkanStructure();
	imageCompressionSwapchain.imageCompressionControlSwapchain = VK_TRUE;

	const VkPhysicalDeviceFeatures features = {};

	// Convert from std::vector<std::string> to std::vector<const char*>.
	std::vector<const char*> extensionsChar;
	extensionsChar.reserve(extensions.size());
	std::transform(begin(extensions), end(extensions), std::back_inserter(extensionsChar),
				   [](const std::string& s) { return s.c_str(); });

	const VkDeviceCreateInfo deviceParams = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
											  &imageCompressionSwapchain,
											  (VkDeviceCreateFlags)0,
											  1u,
											  &queueInfo,
											  0u,											// enabledLayerCount
											  nullptr,										// ppEnabledLayerNames
											  static_cast<deUint32>(extensionsChar.size()), // enabledExtensionCount
											  extensionsChar.data(),						// ppEnabledExtensionNames
											  &features };

	return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

struct DeviceHelper
{
	const VkPhysicalDevice physicalDevice;
	const deUint32		   queueFamilyIndex;
	const Unique<VkDevice> device;
	const DeviceDriver	   vkd;
	const VkQueue		   queue;

	DeviceHelper(Context& context, const InstanceInterface& vki, VkInstance instance,
				 const vector<VkSurfaceKHR>& surface, const vector<string>& additionalExtensions = vector<string>(),
				 const VkAllocationCallbacks* pAllocator = DE_NULL)
		: physicalDevice(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex(chooseQueueFamilyIndex(vki, physicalDevice, surface))
		, device(createDeviceWithWsi(context.getPlatformInterface(), context.getUsedApiVersion(), instance, vki,
									 physicalDevice, enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
									 additionalExtensions, queueFamilyIndex,
									 context.getTestContext().getCommandLine().isValidationEnabled(), pAllocator))
		, vkd(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion())
		, queue(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
	{
	}

	// Single-surface shortcut.
	DeviceHelper(Context& context, const InstanceInterface& vki, VkInstance instance, VkSurfaceKHR surface,
				 const vector<string>&		  additionalExtensions = vector<string>(),
				 const VkAllocationCallbacks* pAllocator		   = DE_NULL)
		: DeviceHelper(context, vki, instance, vector<VkSurfaceKHR>(1u, surface), additionalExtensions, pAllocator)
	{
	}
};
static tcu::TestStatus swapchainCreateTest(Context& context, TestParams testParams)
{
	checkImageCompressionControlSupport(context, true);

	tcu::TestLog&		 log = context.getTestContext().getLog();
	tcu::ResultCollector results(log);

	const InstanceHelper	 instHelper(context, testParams.wsiType);
	const wsi::NativeObjects native(context, instHelper.supportedExtensions, testParams.wsiType);
	const bool				 is_fixed_rate_ex = testParams.control.flags == VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT;

	VkExtent2D							extent2d = { 16, 16 };
	VkImageCompressionFixedRateFlagsEXT planeFlags[3]{};

	for (unsigned i{}; i < (is_fixed_rate_ex ? 24 : 1); i++)
	{
		planeFlags[0] ^= 3 << i;

		if (is_fixed_rate_ex)
		{
			testParams.control.pFixedRateFlags = planeFlags;
		}

		const Unique<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType,
														 native.getDisplay(), native.getWindow(),
														 context.getTestContext().getCommandLine()));

		const DeviceHelper devHelper(context, instHelper.vki, instHelper.instance, *surface, vector<string>());

		VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = initVulkanStructure();
		VkSurfaceCapabilities2KHR		caps		= initVulkanStructure();
		surfaceInfo.surface							= surface.get();

		instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &caps);

		deUint32 numFormats;
		instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(devHelper.physicalDevice, &surfaceInfo, &numFormats,
														   nullptr);

		vector<VkSurfaceFormat2KHR> formats(numFormats);
		for (auto& surfaceFormat : formats)
		{
			surfaceFormat = initVulkanStructure();
		}

		instHelper.vki.getPhysicalDeviceSurfaceFormats2KHR(devHelper.physicalDevice, &surfaceInfo, &numFormats,
														   formats.data());

		deUint32 queueFamilyIndex = devHelper.queueFamilyIndex;

		for (auto& format : formats)
		{
			testParams.format = format.surfaceFormat.format;

			const uint32_t numPlanes = isYCbCrFormat(testParams.format) ? getPlaneCount(testParams.format) : 1;
			testParams.control.compressionControlPlaneCount = is_fixed_rate_ex ? numPlanes : 0;

			VkSwapchainCreateInfoKHR swapchainInfo = initVulkanStructure();
			swapchainInfo.surface				   = surface.get();
			swapchainInfo.minImageCount			   = caps.surfaceCapabilities.minImageCount;
			swapchainInfo.imageFormat			   = format.surfaceFormat.format;
			swapchainInfo.imageColorSpace		   = format.surfaceFormat.colorSpace;
			swapchainInfo.imageExtent			   = extent2d;
			swapchainInfo.imageArrayLayers		   = 1;
			swapchainInfo.imageUsage			   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			swapchainInfo.imageSharingMode		   = VK_SHARING_MODE_EXCLUSIVE;
			swapchainInfo.queueFamilyIndexCount	   = 1;
			swapchainInfo.pQueueFamilyIndices	   = &queueFamilyIndex;
			swapchainInfo.preTransform			   = caps.surfaceCapabilities.currentTransform;
			swapchainInfo.compositeAlpha		   = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
			swapchainInfo.presentMode			   = VK_PRESENT_MODE_FIFO_KHR;
			swapchainInfo.clipped				   = VK_TRUE;

			swapchainInfo.pNext = &testParams.control;

			Move<VkSwapchainKHR> swapchain = createSwapchainKHR(devHelper.vkd, devHelper.device.get(), &swapchainInfo);

			deUint32 imageCount = 0;
			devHelper.vkd.getSwapchainImagesKHR(devHelper.device.get(), swapchain.get(), &imageCount, nullptr);
			vector<VkImage> images(imageCount);
			devHelper.vkd.getSwapchainImagesKHR(devHelper.device.get(), swapchain.get(), &imageCount, images.data());

			validate(instHelper.vki, devHelper.vkd, results, devHelper.physicalDevice, devHelper.device.get(),
					 testParams, images[0]);
		}
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void addAhbCompressionControlTests(tcu::TestCaseGroup *group, TestParams testParams)
{
	// Ahb formats
	static const vk::VkFormat ahbFormats[] = {
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_S8_UINT
	};

	for (int index = 0; index < DE_LENGTH_OF_ARRAY(ahbFormats); ++index)
	{
		testParams.format = ahbFormats[index];
		const char *const enumName = getFormatName(testParams.format);
		const string caseName = de::toLower(string(enumName).substr(10));
		addFunctionCase(group, caseName, enumName, ahbImageCreateTest, testParams);
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
	} constexpr compression_flags[] = {
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

	subgroup = new tcu::TestCaseGroup(testCtx, "android_hardware_buffer",
									  "Test creating Android Hardware buffer with compression control struct");

	for (auto& flag : compression_flags)
	{
		testParams.control.flags = flag.flag;
		subgroup->addChild(createTestGroup(testCtx, flag.name,
										   "Queries images created with compression control struct.",
										   addAhbCompressionControlTests, testParams));
	}

	group->addChild(subgroup);

	subgroup = new tcu::TestCaseGroup(testCtx, "swapchain", "swapchain");
	for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
	{
		const vk::wsi::Type wsiType = (vk::wsi::Type)typeNdx;
		testParams.wsiType			= wsiType;

		tcu::TestCaseGroup* wsi_subgroup(new tcu::TestCaseGroup(testCtx, getName(wsiType), "Swapchain tests"));

		for (auto& flag : compression_flags)
		{
			testParams.control.flags = flag.flag;
			addFunctionCase(wsi_subgroup, flag.name, flag.name, swapchainCreateTest, testParams);
		}
		subgroup->addChild(wsi_subgroup);
	}

	group->addChild(subgroup);

	return group.release();
}

} // namespace api

} // namespace vkt
