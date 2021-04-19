/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief VkSwapchain Tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiSwapchainTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktNativeObjectsUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjTypeImpl.inl"
#include "vkObjUtil.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"
#include "deSharedPtr.hpp"

#include <limits>
#include <algorithm>
#include <iterator>

namespace vkt
{
namespace wsi
{

namespace
{

using namespace vk;
using namespace vk::wsi;

using tcu::TestLog;
using tcu::Maybe;
using tcu::UVec2;

using de::MovePtr;
using de::UniquePtr;
using de::SharedPtr;

using std::string;
using std::vector;

typedef vector<VkExtensionProperties> Extensions;

void checkAllSupported (const Extensions& supportedExtensions, const vector<string>& requiredExtensions)
{
	for (vector<string>::const_iterator requiredExtName = requiredExtensions.begin();
		 requiredExtName != requiredExtensions.end();
		 ++requiredExtName)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(*requiredExtName)))
			TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
	}
}

CustomInstance createInstanceWithWsi (Context&						context,
									  const Extensions&				supportedExtensions,
									  Type							wsiType,
									  const vector<string>			extraExtensions,
									  const VkAllocationCallbacks*	pAllocator	= DE_NULL)
{
	vector<string>	extensions = extraExtensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));

	// VK_EXT_swapchain_colorspace adds new surface formats. Driver can enumerate
	// the formats regardless of whether VK_EXT_swapchain_colorspace was enabled,
	// but using them without enabling the extension is not allowed. Thus we have
	// two options:
	//
	// 1) Filter out non-core formats to stay within valid usage.
	//
	// 2) Enable VK_EXT_swapchain colorspace if advertised by the driver.
	//
	// We opt for (2) as it provides basic coverage for the extension as a bonus.
	if (isExtensionSupported(supportedExtensions, RequiredExtension("VK_EXT_swapchain_colorspace")))
		extensions.push_back("VK_EXT_swapchain_colorspace");

	checkAllSupported(supportedExtensions, extensions);

	return vkt::createCustomInstanceWithExtensions(context, extensions, pAllocator);
}

VkPhysicalDeviceFeatures getDeviceFeaturesForWsi (void)
{
	VkPhysicalDeviceFeatures features;
	deMemset(&features, 0, sizeof(features));
	return features;
}

Move<VkDevice> createDeviceWithWsi (const PlatformInterface&		vkp,
									deUint32						apiVersion,
									VkInstance						instance,
									const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									const Extensions&				supportedExtensions,
									const vector<string>&			additionalExtensions,
									const vector<deUint32>&			queueFamilyIndices,
									bool							validationEnabled,
									const VkAllocationCallbacks*	pAllocator = DE_NULL)
{
	const float						queuePriorities[] = { 1.0f };
	vector<VkDeviceQueueCreateInfo>	queueInfos;

	for (const auto familyIndex : queueFamilyIndices)
	{
		const VkDeviceQueueCreateInfo info =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr,
			(VkDeviceQueueCreateFlags)0,
			familyIndex,
			DE_LENGTH_OF_ARRAY(queuePriorities),
			&queuePriorities[0],
		};

		queueInfos.push_back(info);
	}

	vector<string> extensions;
	extensions.push_back("VK_KHR_swapchain");
	extensions.insert(end(extensions), begin(additionalExtensions), end(additionalExtensions));

	for (const auto& extName : extensions)
	{
		if (!isCoreDeviceExtension(apiVersion, extName) && !isExtensionSupported(supportedExtensions, RequiredExtension(extName)))
			TCU_THROW(NotSupportedError, extName + " is not supported");
	}

	const void *					pNext			= nullptr;
	const VkPhysicalDeviceFeatures	features		= getDeviceFeaturesForWsi();

	VkDevicePrivateDataCreateInfoEXT pdci = initVulkanStructure();
	pdci.privateDataSlotRequestCount = 4u;

	VkPhysicalDevicePrivateDataFeaturesEXT privateDataFeatures = initVulkanStructure(&pdci);
	privateDataFeatures.privateData = VK_TRUE;

	if (de::contains(begin(extensions), end(extensions), "VK_EXT_private_data"))
	{
		pNext = &privateDataFeatures;
	}

	// Convert from std::vector<std::string> to std::vector<const char*>.
	std::vector<const char*> extensionsChar;
	extensionsChar.reserve(extensions.size());
	std::transform(begin(extensions), end(extensions), std::back_inserter(extensionsChar), [](const std::string& s) { return s.c_str(); });

	const VkDeviceCreateInfo deviceParams =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		pNext,
		(VkDeviceCreateFlags)0,
		static_cast<deUint32>(queueInfos.size()),
		queueInfos.data(),
		0u,												// enabledLayerCount
		nullptr,										// ppEnabledLayerNames
		static_cast<deUint32>(extensionsChar.size()),	// enabledExtensionCount
		extensionsChar.data(),							// ppEnabledExtensionNames
		&features
	};

	return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

Move<VkDevice> createDeviceWithWsi (const PlatformInterface&		vkp,
									deUint32						apiVersion,
									VkInstance						instance,
									const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									const Extensions&				supportedExtensions,
									const vector<string>&			additionalExtensions,
									const deUint32					queueFamilyIndex,
									bool							validationEnabled,
									const VkAllocationCallbacks*	pAllocator = DE_NULL)
{
	return createDeviceWithWsi(vkp, apiVersion, instance, vki, physicalDevice, supportedExtensions, additionalExtensions, vector<deUint32>(1u, queueFamilyIndex), validationEnabled, pAllocator);
}

struct InstanceHelper
{
	const vector<VkExtensionProperties>	supportedExtensions;
	const CustomInstance				instance;
	const InstanceDriver&				vki;

	InstanceHelper (Context& context, Type wsiType, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context,
													   supportedExtensions,
													   wsiType,
													   vector<string>(),
													   pAllocator))
		, vki					(instance.getDriver())
	{}

	InstanceHelper (Context& context, Type wsiType, const vector<string>& extensions, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context,
													   supportedExtensions,
													   wsiType,
													   extensions,
													   pAllocator))
		, vki					(instance.getDriver())
	{}
};

struct DeviceHelper
{
	const VkPhysicalDevice	physicalDevice;
	const deUint32			queueFamilyIndex;
	const Unique<VkDevice>	device;
	const DeviceDriver		vkd;
	const VkQueue			queue;

	DeviceHelper (Context&						context,
				  const InstanceInterface&		vki,
				  VkInstance					instance,
				  const vector<VkSurfaceKHR>&	surface,
				  const vector<string>&			additionalExtensions = vector<string>(),
				  const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: physicalDevice	(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, surface))
		, device			(createDeviceWithWsi(context.getPlatformInterface(),
												 context.getUsedApiVersion(),
												 instance,
												 vki,
												 physicalDevice,
												 enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
												 additionalExtensions,
												 queueFamilyIndex,
												 context.getTestContext().getCommandLine().isValidationEnabled(),
												 pAllocator))
		, vkd				(context.getPlatformInterface(), instance, *device)
		, queue				(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
	{
	}

	// Single-surface shortcut.
	DeviceHelper (Context&						context,
				  const InstanceInterface&		vki,
				  VkInstance					instance,
				  VkSurfaceKHR					surface,
				  const vector<string>&			additionalExtensions = vector<string>(),
				  const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: DeviceHelper(context, vki, instance, vector<VkSurfaceKHR>(1u, surface), additionalExtensions, pAllocator)
	{
	}
};

// Similar to the one above with no queues and multiple queue families.
struct MultiQueueDeviceHelper
{
	const VkPhysicalDevice	physicalDevice;
	const vector<deUint32>	queueFamilyIndices;
	const Unique<VkDevice>	device;
	const DeviceDriver		vkd;

	MultiQueueDeviceHelper (Context&						context,
							const InstanceInterface&		vki,
							VkInstance						instance,
							const vector<VkSurfaceKHR>&		surface,
							const vector<string>&			additionalExtensions = vector<string>(),
							const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: physicalDevice	(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndices(getCompatibleQueueFamilyIndices(vki, physicalDevice, surface))
		, device			(createDeviceWithWsi(context.getPlatformInterface(),
												 context.getUsedApiVersion(),
												 instance,
												 vki,
												 physicalDevice,
												 enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
												 additionalExtensions,
												 queueFamilyIndices,
												 context.getTestContext().getCommandLine().isValidationEnabled(),
												 pAllocator))
		, vkd				(context.getPlatformInterface(), instance, *device)
	{
	}

	// Single-surface shortcut.
	MultiQueueDeviceHelper (Context&						context,
							const InstanceInterface&		vki,
							VkInstance						instance,
							VkSurfaceKHR					surface,
							const vector<string>			additionalExtensions = vector<string>(),
							const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: MultiQueueDeviceHelper(context, vki, instance, vector<VkSurfaceKHR>(1u, surface), additionalExtensions, pAllocator)
	{
	}
};

enum TestDimension
{
	TEST_DIMENSION_MIN_IMAGE_COUNT = 0,	//!< Test all supported image counts
	TEST_DIMENSION_IMAGE_FORMAT,		//!< Test all supported formats
	TEST_DIMENSION_IMAGE_EXTENT,		//!< Test various (supported) extents
	TEST_DIMENSION_IMAGE_ARRAY_LAYERS,
	TEST_DIMENSION_IMAGE_USAGE,
	TEST_DIMENSION_IMAGE_SHARING_MODE,
	TEST_DIMENSION_PRE_TRANSFORM,
	TEST_DIMENSION_COMPOSITE_ALPHA,
	TEST_DIMENSION_PRESENT_MODE,
	TEST_DIMENSION_CLIPPED,

	TEST_DIMENSION_LAST
};

const char* getTestDimensionName (TestDimension dimension)
{
	static const char* const s_names[] =
	{
		"min_image_count",
		"image_format",
		"image_extent",
		"image_array_layers",
		"image_usage",
		"image_sharing_mode",
		"pre_transform",
		"composite_alpha",
		"present_mode",
		"clipped"
	};
	return de::getSizedArrayElement<TEST_DIMENSION_LAST>(s_names, dimension);
}

struct TestParameters
{
	Type			wsiType;
	TestDimension	dimension;

	TestParameters (Type wsiType_, TestDimension dimension_)
		: wsiType	(wsiType_)
		, dimension	(dimension_)
	{}

	TestParameters (void)
		: wsiType	(TYPE_LAST)
		, dimension	(TEST_DIMENSION_LAST)
	{}
};

vector<VkSwapchainCreateInfoKHR> generateSwapchainParameterCases (const InstanceInterface&			vki,
																  VkPhysicalDevice					physicalDevice,
																  Type								wsiType,
																  TestDimension						dimension,
																  const VkSurfaceCapabilitiesKHR&	capabilities,
																  const vector<VkSurfaceFormatKHR>&	formats,
																  const vector<VkPresentModeKHR>&	presentModes)
{
	const PlatformProperties&			platformProperties	= getPlatformProperties(wsiType);
	vector<VkSwapchainCreateInfoKHR>	cases;
	const VkSurfaceTransformFlagBitsKHR defaultTransform	= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const VkSwapchainCreateInfoKHR		baseParameters		=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		(VkSwapchainCreateFlagsKHR)0,
		(VkSurfaceKHR)0,
		capabilities.minImageCount,
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE
			? capabilities.minImageExtent : capabilities.currentExtent),
		1u,									// imageArrayLayers
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		defaultTransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(VkSwapchainKHR)0					// oldSwapchain
	};

	switch (dimension)
	{
		case TEST_DIMENSION_MIN_IMAGE_COUNT:
		{
			const deUint32	maxImageCountToTest	= de::clamp(16u, capabilities.minImageCount, (capabilities.maxImageCount > 0) ? capabilities.maxImageCount : capabilities.minImageCount + 16u);

			for (deUint32 imageCount = capabilities.minImageCount; imageCount <= maxImageCountToTest; ++imageCount)
			{
				cases.push_back(baseParameters);
				cases.back().minImageCount = imageCount;
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_FORMAT:
		{
			for (vector<VkSurfaceFormatKHR>::const_iterator curFmt = formats.begin(); curFmt != formats.end(); ++curFmt)
			{
				cases.push_back(baseParameters);
				cases.back().imageFormat		= curFmt->format;
				cases.back().imageColorSpace	= curFmt->colorSpace;
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_EXTENT:
		{
			static const VkExtent2D	s_testSizes[]	=
			{
				{ 1, 1 },
				{ 16, 32 },
				{ 32, 16 },
				{ 632, 231 },
				{ 117, 998 },
			};

			if (platformProperties.swapchainExtent == PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE ||
				platformProperties.swapchainExtent == PlatformProperties::SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE)
			{
				for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_testSizes); ++ndx)
				{
					cases.push_back(baseParameters);
					cases.back().imageExtent.width	= de::clamp(s_testSizes[ndx].width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
					cases.back().imageExtent.height	= de::clamp(s_testSizes[ndx].height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
				}
			}

			if (platformProperties.swapchainExtent != PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE)
			{
				cases.push_back(baseParameters);
				cases.back().imageExtent = capabilities.currentExtent;
			}

			if (platformProperties.swapchainExtent != PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE)
			{
				cases.push_back(baseParameters);
				cases.back().imageExtent = capabilities.minImageExtent;

				cases.push_back(baseParameters);
				cases.back().imageExtent = capabilities.maxImageExtent;
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_ARRAY_LAYERS:
		{
			const deUint32	maxLayers	= de::min(capabilities.maxImageArrayLayers, 16u);

			for (deUint32 numLayers = 1; numLayers <= maxLayers; ++numLayers)
			{
				cases.push_back(baseParameters);
				cases.back().imageArrayLayers = numLayers;
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_USAGE:
		{
			for (deUint32 flags = 1u; flags <= capabilities.supportedUsageFlags; ++flags)
			{
				VkImageFormatProperties imageProps;

				if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice,
															   baseParameters.imageFormat,
															   VK_IMAGE_TYPE_2D,
															   VK_IMAGE_TILING_OPTIMAL,
															   flags,
															   (VkImageCreateFlags)0u,
															   &imageProps) != VK_SUCCESS)
					continue;

				if ((flags & ~capabilities.supportedUsageFlags) == 0)
				{
					cases.push_back(baseParameters);
					cases.back().imageUsage = flags;
				}
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_SHARING_MODE:
		{
#if 0
			// Skipping since this matches the base parameters.
			cases.push_back(baseParameters);
			cases.back().imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
#endif

			cases.push_back(baseParameters);
			cases.back().imageSharingMode = VK_SHARING_MODE_CONCURRENT;

			break;
		}

		case TEST_DIMENSION_PRE_TRANSFORM:
		{
			for (deUint32 transform = 1u;
				 transform <= capabilities.supportedTransforms;
				 transform = transform<<1u)
			{
				if ((transform & capabilities.supportedTransforms) != 0)
				{
					cases.push_back(baseParameters);
					cases.back().preTransform = (VkSurfaceTransformFlagBitsKHR)transform;
				}
			}

			break;
		}

		case TEST_DIMENSION_COMPOSITE_ALPHA:
		{
			for (deUint32 alphaMode = 1u;
				 alphaMode <= capabilities.supportedCompositeAlpha;
				 alphaMode = alphaMode<<1u)
			{
				if ((alphaMode & capabilities.supportedCompositeAlpha) != 0)
				{
					cases.push_back(baseParameters);
					cases.back().compositeAlpha = (VkCompositeAlphaFlagBitsKHR)alphaMode;
				}
			}

			break;
		}

		case TEST_DIMENSION_PRESENT_MODE:
		{
			for (vector<VkPresentModeKHR>::const_iterator curMode = presentModes.begin(); curMode != presentModes.end(); ++curMode)
			{
				cases.push_back(baseParameters);
				cases.back().presentMode = *curMode;
			}

			break;
		}

		case TEST_DIMENSION_CLIPPED:
		{
			cases.push_back(baseParameters);
			cases.back().clipped = VK_FALSE;

			cases.push_back(baseParameters);
			cases.back().clipped = VK_TRUE;

			break;
		}

		default:
			DE_FATAL("Impossible");
	}

	DE_ASSERT(!cases.empty());
	return cases;
}

vector<VkSwapchainCreateInfoKHR> generateSwapchainParameterCases (Type								wsiType,
																  TestDimension						dimension,
																  const InstanceInterface&			vki,
																  VkPhysicalDevice					physicalDevice,
																  VkSurfaceKHR						surface)
{
	const VkSurfaceCapabilitiesKHR		capabilities	= getPhysicalDeviceSurfaceCapabilities(vki,
																							   physicalDevice,
																							   surface);
	const vector<VkSurfaceFormatKHR>	formats			= getPhysicalDeviceSurfaceFormats(vki,
																						  physicalDevice,
																						  surface);
	const vector<VkPresentModeKHR>		presentModes	= getPhysicalDeviceSurfacePresentModes(vki,
																							   physicalDevice,
																							   surface);

	return generateSwapchainParameterCases(vki, physicalDevice, wsiType, dimension, capabilities, formats, presentModes);
}

tcu::TestStatus createSwapchainTest (Context& context, TestParameters params)
{
	tcu::TestLog&							log			= context.getTestContext().getLog();
	const InstanceHelper					instHelper	(context, params.wsiType);
	const NativeObjects						native		(context, instHelper.supportedExtensions, params.wsiType);
	const Unique<VkSurfaceKHR>				surface		(createSurface(instHelper.vki, instHelper.instance, params.wsiType, native.getDisplay(), native.getWindow()));
	const MultiQueueDeviceHelper			devHelper	(context, instHelper.vki, instHelper.instance, *surface);
	const vector<VkSwapchainCreateInfoKHR>	cases		(generateSwapchainParameterCases(params.wsiType, params.dimension, instHelper.vki, devHelper.physicalDevice, *surface));
	const VkSurfaceCapabilitiesKHR			capabilities(getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface));

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		std::ostringstream subcase;
		subcase << "Sub-case " << (caseNdx+1) << " / " << cases.size() << ": ";

		VkSwapchainCreateInfoKHR	curParams	= cases[caseNdx];

		if (curParams.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
		{
			const deUint32 numFamilies = static_cast<deUint32>(devHelper.queueFamilyIndices.size());
			if (numFamilies < 2u)
				TCU_THROW(NotSupportedError, "Only " + de::toString(numFamilies) + " queue families available for VK_SHARING_MODE_CONCURRENT");
			curParams.queueFamilyIndexCount	= numFamilies;
		}
		else
		{
			// Take only the first queue.
			if (devHelper.queueFamilyIndices.empty())
				TCU_THROW(NotSupportedError, "No queue families compatible with the given surface");
			curParams.queueFamilyIndexCount	= 1u;
		}
		curParams.pQueueFamilyIndices	= devHelper.queueFamilyIndices.data();
		curParams.surface				= *surface;

		log << TestLog::Message << subcase.str() << curParams << TestLog::EndMessage;

		// The Vulkan 1.1.87 spec contains the following VU for VkSwapchainCreateInfoKHR:
		//
		//     * imageFormat, imageUsage, imageExtent, and imageArrayLayers must be supported for VK_IMAGE_TYPE_2D
		//     VK_IMAGE_TILING_OPTIMAL images as reported by vkGetPhysicalDeviceImageFormatProperties.
		VkImageFormatProperties properties;
		const VkResult propertiesResult = instHelper.vki.getPhysicalDeviceImageFormatProperties(devHelper.physicalDevice,
																								curParams.imageFormat,
																								VK_IMAGE_TYPE_2D,
																								VK_IMAGE_TILING_OPTIMAL,
																								curParams.imageUsage,
																								0, // flags
																								&properties);

		log << TestLog::Message << subcase.str()
			<< "vkGetPhysicalDeviceImageFormatProperties => "
			<< getResultStr(propertiesResult) << TestLog::EndMessage;

		switch (propertiesResult) {
		case VK_SUCCESS:
			{
				// The maxExtents case might not be able to create the requested surface due to insufficient
				// memory, so in this case *only* we handle the OOM exception.
				if (params.dimension == TEST_DIMENSION_IMAGE_EXTENT &&
					capabilities.maxImageExtent.width == curParams.imageExtent.width &&
					capabilities.maxImageExtent.height == curParams.imageExtent.height)
				{
					try
					{
						const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &curParams));

						log << TestLog::Message << subcase.str()
							<< "Creating swapchain succeeded" << TestLog::EndMessage;
					}
					catch (const OutOfMemoryError& e)
					{
						log << TestLog::Message << subcase.str() << "vkCreateSwapchainKHR with maxImageExtent encountered " << e.getError() << TestLog::EndMessage;
					}
				}
				else
				{
					const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &curParams));

					log << TestLog::Message << subcase.str()
						<< "Creating swapchain succeeded" << TestLog::EndMessage;
				}
			}
			break;
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			log << TestLog::Message << subcase.str()
				<< "Skip because vkGetPhysicalDeviceImageFormatProperties returned VK_ERROR_FORMAT_NOT_SUPPORTED" << TestLog::EndMessage;
			break;
		default:
			log << TestLog::Message << subcase.str()
				<< "Fail because vkGetPhysicalDeviceImageFormatProperties returned "
				<< getResultStr(propertiesResult) << TestLog::EndMessage;
			return tcu::TestStatus::fail("Unexpected result from vkGetPhysicalDeviceImageFormatProperties");
		}
	}

	return tcu::TestStatus::pass("No sub-case failed");
}

template<typename T> static deUint64 HandleToInt(T t) { return t.getInternal(); }

tcu::TestStatus createSwapchainPrivateDataTest (Context& context, TestParameters params)
{
	if (!context.getPrivateDataFeaturesEXT().privateData)
		TCU_THROW(NotSupportedError, "privateData not supported");

	tcu::TestLog&							log			= context.getTestContext().getLog();
	const InstanceHelper					instHelper	(context, params.wsiType);
	const NativeObjects						native		(context, instHelper.supportedExtensions, params.wsiType);
	const Unique<VkSurfaceKHR>				surface		(createSurface(instHelper.vki, instHelper.instance, params.wsiType, native.getDisplay(), native.getWindow()));
	const vector<string>					extraExts	(1u, "VK_EXT_private_data");
	const MultiQueueDeviceHelper			devHelper	(context, instHelper.vki, instHelper.instance, *surface, extraExts);
	const vector<VkSwapchainCreateInfoKHR>	cases		(generateSwapchainParameterCases(params.wsiType, params.dimension, instHelper.vki, devHelper.physicalDevice, *surface));

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		std::ostringstream subcase;
		subcase << "Sub-case " << (caseNdx+1) << " / " << cases.size() << ": ";

		VkSwapchainCreateInfoKHR	curParams	= cases[caseNdx];

		if (curParams.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
		{
			const deUint32 numFamilies = static_cast<deUint32>(devHelper.queueFamilyIndices.size());
			if (numFamilies < 2u)
				TCU_THROW(NotSupportedError, "Only " + de::toString(numFamilies) + " queue families available for VK_SHARING_MODE_CONCURRENT");
			curParams.queueFamilyIndexCount	= numFamilies;
		}
		else
		{
			// Take only the first queue.
			if (devHelper.queueFamilyIndices.empty())
				TCU_THROW(NotSupportedError, "No queue families compatible with the given surface");
			curParams.queueFamilyIndexCount	= 1u;
		}
		curParams.pQueueFamilyIndices	= devHelper.queueFamilyIndices.data();
		curParams.surface				= *surface;

		log << TestLog::Message << subcase.str() << curParams << TestLog::EndMessage;

		// The Vulkan 1.1.87 spec contains the following VU for VkSwapchainCreateInfoKHR:
		//
		//     * imageFormat, imageUsage, imageExtent, and imageArrayLayers must be supported for VK_IMAGE_TYPE_2D
		//     VK_IMAGE_TILING_OPTIMAL images as reported by vkGetPhysicalDeviceImageFormatProperties.
		VkImageFormatProperties properties;
		const VkResult propertiesResult = instHelper.vki.getPhysicalDeviceImageFormatProperties(devHelper.physicalDevice,
																								curParams.imageFormat,
																								VK_IMAGE_TYPE_2D,
																								VK_IMAGE_TILING_OPTIMAL,
																								curParams.imageUsage,
																								0, // flags
																								&properties);

		log << TestLog::Message << subcase.str()
			<< "vkGetPhysicalDeviceImageFormatProperties => "
			<< getResultStr(propertiesResult) << TestLog::EndMessage;

		switch (propertiesResult) {
		case VK_SUCCESS:
			{
				const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &curParams));

				const int numSlots = 100;
				typedef Unique<VkPrivateDataSlotEXT>				PrivateDataSlotUp;
				typedef SharedPtr<PrivateDataSlotUp>				PrivateDataSlotSp;
				vector<PrivateDataSlotSp> slots;

				const VkPrivateDataSlotCreateInfoEXT createInfo =
				{
					VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO_EXT,	// VkStructureType                    sType;
					DE_NULL,												// const void*                        pNext;
					0u,														// VkPrivateDataSlotCreateFlagsEXT    flags;
				};

				for (int i = 0; i < numSlots; ++i)
				{
					Move<VkPrivateDataSlotEXT> s = createPrivateDataSlotEXT(devHelper.vkd, *devHelper.device, &createInfo, DE_NULL);
					slots.push_back(PrivateDataSlotSp(new PrivateDataSlotUp(s)));
				}

				// Based on code in vktApiObjectManagementTests.cpp
				for (int r = 0; r < 3; ++r)
				{
					deUint64 data;

					for (int i = 0; i < numSlots; ++i)
					{
						data = 1234;
						devHelper.vkd.getPrivateDataEXT(*devHelper.device, getObjectType<VkSwapchainKHR>(), HandleToInt(swapchain.get()), **slots[i], &data);
						// Don't test default value of zero on Android, due to spec erratum
						if (params.wsiType != TYPE_ANDROID)
						{
							if (data != 0)
								return tcu::TestStatus::fail("Expected initial value of zero");
						}
					}

					for (int i = 0; i < numSlots; ++i)
						VK_CHECK(devHelper.vkd.setPrivateDataEXT(*devHelper.device, getObjectType<VkSwapchainKHR>(), HandleToInt(swapchain.get()), **slots[i], i*i*i + 1));

					for (int i = 0; i < numSlots; ++i)
					{
						data = 1234;
						devHelper.vkd.getPrivateDataEXT(*devHelper.device, getObjectType<VkSwapchainKHR>(), HandleToInt(swapchain.get()), **slots[i], &data);
						if (data != (deUint64)(i*i*i + 1))
							return tcu::TestStatus::fail("Didn't read back set value");
					}

					// Destroy and realloc slots for the next iteration
					slots.clear();
					for (int i = 0; i < numSlots; ++i)
					{
						Move<VkPrivateDataSlotEXT> s = createPrivateDataSlotEXT(devHelper.vkd, *devHelper.device, &createInfo, DE_NULL);
						slots.push_back(PrivateDataSlotSp(new PrivateDataSlotUp(s)));
					}
				}


			}
			break;
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			log << TestLog::Message << subcase.str()
				<< "Skip because vkGetPhysicalDeviceImageFormatProperties returned VK_ERROR_FORMAT_NOT_SUPPORTED" << TestLog::EndMessage;
			break;
		default:
			log << TestLog::Message << subcase.str()
				<< "Fail because vkGetPhysicalDeviceImageFormatProperties returned "
				<< getResultStr(propertiesResult) << TestLog::EndMessage;
			return tcu::TestStatus::fail("Unexpected result from vkGetPhysicalDeviceImageFormatProperties");
		}
	}

	return tcu::TestStatus::pass("No sub-case failed");
}

tcu::TestStatus createSwapchainSimulateOOMTest (Context& context, TestParameters params)
{
	const size_t				maxCases			= 300u;
	const deUint32				maxAllocs			= 1024u;

	tcu::TestLog&				log					= context.getTestContext().getLog();
	tcu::ResultCollector		results				(log);

	AllocationCallbackRecorder	allocationRecorder	(getSystemAllocator());
	DeterministicFailAllocator	failingAllocator	(allocationRecorder.getCallbacks(),
													 DeterministicFailAllocator::MODE_DO_NOT_COUNT,
													 0);
	{
		const InstanceHelper					instHelper	(context, params.wsiType, failingAllocator.getCallbacks());
		const NativeObjects						native		(context, instHelper.supportedExtensions, params.wsiType);
		const Unique<VkSurfaceKHR>				surface		(createSurface(instHelper.vki,
																			instHelper.instance,
																			params.wsiType,
																			native.getDisplay(),
																			native.getWindow(),
																			failingAllocator.getCallbacks()));
		const DeviceHelper						devHelper	(context, instHelper.vki, instHelper.instance, *surface, vector<string>(), failingAllocator.getCallbacks());
		const vector<VkSwapchainCreateInfoKHR>	allCases	(generateSwapchainParameterCases(params.wsiType, params.dimension, instHelper.vki, devHelper.physicalDevice, *surface));
		const VkSurfaceCapabilitiesKHR			capabilities(getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface));

		if (maxCases < allCases.size())
			log << TestLog::Message << "Note: Will only test first " << maxCases << " cases out of total of " << allCases.size() << " parameter combinations" << TestLog::EndMessage;

		for (size_t caseNdx = 0; caseNdx < de::min(maxCases, allCases.size()); ++caseNdx)
		{
			log << TestLog::Message << "Testing parameter case " << caseNdx << ": " << allCases[caseNdx] << TestLog::EndMessage;

			for (deUint32 numPassingAllocs = 0; numPassingAllocs <= maxAllocs; ++numPassingAllocs)
			{
				bool						gotOOM		= false;
				VkSwapchainCreateInfoKHR	curParams	= allCases[caseNdx];
				curParams.surface				= *surface;
				curParams.queueFamilyIndexCount	= 1u;
				curParams.pQueueFamilyIndices	= &devHelper.queueFamilyIndex;

				failingAllocator.reset(DeterministicFailAllocator::MODE_COUNT_AND_FAIL, numPassingAllocs);

				log << TestLog::Message << "Testing with " << numPassingAllocs << " first allocations succeeding" << TestLog::EndMessage;

				try
				{
					// With concurrent sharing mode, at least two queues are needed.
					if (curParams.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
						continue;

					const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &curParams, failingAllocator.getCallbacks()));
				}
				catch (const OutOfMemoryError& e)
				{
					log << TestLog::Message << "Got " << e.getError() << TestLog::EndMessage;
					gotOOM = true;
				}

				if (!gotOOM)
				{
					log << TestLog::Message << "Creating swapchain succeeded!" << TestLog::EndMessage;

					if (numPassingAllocs == 0)
						results.addResult(QP_TEST_RESULT_QUALITY_WARNING, "Allocation callbacks were not used");

					break;
				}
				else if (numPassingAllocs == maxAllocs)
				{
					// The maxExtents case might not be able to create the requested surface due to insufficient
					// memory, so in this case *only* we allow the OOM exception upto maxAllocs.
					if (params.dimension == TEST_DIMENSION_IMAGE_EXTENT &&
						capabilities.maxImageExtent.width == curParams.imageExtent.width &&
						capabilities.maxImageExtent.height == curParams.imageExtent.height)
						break;

					results.addResult(QP_TEST_RESULT_QUALITY_WARNING, "Creating swapchain did not succeed, callback limit exceeded");
				}
			}

			context.getTestContext().touchWatchdog();
		}
	}

	if (!validateAndLog(log, allocationRecorder, 0u))
		results.fail("Detected invalid system allocation callback");

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus testImageSwapchainCreateInfo (Context& context, Type wsiType)
{
	const tcu::UVec2			desiredSize			(256, 256);
	const InstanceHelper		instHelper			(context, wsiType, vector<string>(1, string("VK_KHR_device_group_creation")));
	const NativeObjects			native				(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>	surface				(createSurface(instHelper.vki,
																   instHelper.instance,
																   wsiType,
																   native.getDisplay(),
																   native.getWindow()));
	const DeviceHelper			devHelper			(context, instHelper.vki, instHelper.instance, *surface, vector<string>(1u, "VK_KHR_bind_memory2"));
	const Extensions&			deviceExtensions	= enumerateDeviceExtensionProperties(instHelper.vki, devHelper.physicalDevice, DE_NULL);

	// structures this tests checks were added in revision 69
	if (!isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_swapchain", 69)))
		TCU_THROW(NotSupportedError, "Required extension revision is not supported");

	const VkSurfaceCapabilitiesKHR		capabilities		= getPhysicalDeviceSurfaceCapabilities(instHelper.vki,
																								   devHelper.physicalDevice,
																								   *surface);
	const vector<VkSurfaceFormatKHR>	formats				= getPhysicalDeviceSurfaceFormats(instHelper.vki,
																							  devHelper.physicalDevice,
																							  *surface);
	const PlatformProperties&			platformProperties	= getPlatformProperties(wsiType);
	const VkSurfaceTransformFlagBitsKHR transform			= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const deUint32						desiredImageCount	= 2;
	const VkSwapchainCreateInfoKHR		swapchainInfo		=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		(VkSwapchainCreateFlagsKHR)0,
		*surface,
		de::clamp(desiredImageCount, capabilities.minImageCount, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : capabilities.minImageCount + desiredImageCount),
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE
			? capabilities.currentExtent : vk::makeExtent2D(desiredSize.x(), desiredSize.y())),
		1u,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		transform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(VkSwapchainKHR)0					// oldSwapchain
	};

	const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &swapchainInfo));
	deUint32						numImages	= 0;
	VK_CHECK(devHelper.vkd.getSwapchainImagesKHR(*devHelper.device, *swapchain, &numImages, DE_NULL));
	if (numImages == 0)
		return tcu::TestStatus::pass("Pass");

	VkImageSwapchainCreateInfoKHR imageSwapchainCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		*swapchain
	};

	VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&imageSwapchainCreateInfo,
		(VkImageCreateFlags)0u,					// flags
		VK_IMAGE_TYPE_2D,						// imageType
		formats[0].format,						// format
		{										// extent
			desiredSize.x(),					//   width
			desiredSize.y(),					//   height
			1u									//   depth
		},
		1u,										// mipLevels
		1u,										// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// samples
		VK_IMAGE_TILING_OPTIMAL,				// tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,	// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		0u,										// queueFamilyIndexCount
		DE_NULL,								// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED				// initialLayout
	};

	typedef vk::Unique<VkImage>			UniqueImage;
	typedef de::SharedPtr<UniqueImage>	ImageSp;

	std::vector<ImageSp>							images						(numImages);
	std::vector<VkBindImageMemorySwapchainInfoKHR>	bindImageMemorySwapchainInfo(numImages);
	std::vector<VkBindImageMemoryInfo>				bindImageMemoryInfos		(numImages);

	for (deUint32 idx = 0; idx < numImages; ++idx)
	{
		// Create image
		images[idx] = ImageSp(new UniqueImage(createImage(devHelper.vkd, *devHelper.device, &imageCreateInfo)));

		VkBindImageMemorySwapchainInfoKHR bimsInfo =
		{
			VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,
			DE_NULL,
			*swapchain,
			idx
		};
		bindImageMemorySwapchainInfo[idx] = bimsInfo;

		VkBindImageMemoryInfo bimInfo =
		{
			VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
			&bindImageMemorySwapchainInfo[idx],
			**images[idx],
			DE_NULL,				// If the pNext chain includes an instance of VkBindImageMemorySwapchainInfoKHR, memory must be VK_NULL_HANDLE
			0u						// If swapchain <in VkBindImageMemorySwapchainInfoKHR> is not NULL, the swapchain and imageIndex are used to determine the memory that the image is bound to, instead of memory and memoryOffset.
		};

		bindImageMemoryInfos[idx] = bimInfo;
	}

	VK_CHECK(devHelper.vkd.bindImageMemory2(*devHelper.device, numImages, &bindImageMemoryInfos[0]));

	return tcu::TestStatus::pass("Pass");
}

struct GroupParameters
{
	typedef FunctionInstance1<TestParameters>::Function	Function;

	Type		wsiType;
	Function	function;

	GroupParameters (Type wsiType_, Function function_)
		: wsiType	(wsiType_)
		, function	(function_)
	{}

	GroupParameters (void)
		: wsiType	(TYPE_LAST)
		, function	((Function)DE_NULL)
	{}
};

void populateSwapchainGroup (tcu::TestCaseGroup* testGroup, GroupParameters params)
{
	for (int dimensionNdx = 0; dimensionNdx < TEST_DIMENSION_LAST; ++dimensionNdx)
	{
		const TestDimension		testDimension	= (TestDimension)dimensionNdx;

		addFunctionCase(testGroup, getTestDimensionName(testDimension), "", params.function, TestParameters(params.wsiType, testDimension));
	}

	addFunctionCase(testGroup, "image_swapchain_create_info", "Test VkImageSwapchainCreateInfoKHR", testImageSwapchainCreateInfo, params.wsiType);
}

void populateSwapchainPrivateDataGroup (tcu::TestCaseGroup* testGroup, GroupParameters params)
{
	for (int dimensionNdx = 0; dimensionNdx < TEST_DIMENSION_LAST; ++dimensionNdx)
	{
		const TestDimension		testDimension	= (TestDimension)dimensionNdx;
		if (testDimension == TEST_DIMENSION_IMAGE_EXTENT)
			continue;

		addFunctionCase(testGroup, getTestDimensionName(testDimension), "", params.function, TestParameters(params.wsiType, testDimension));
	}
}

VkSwapchainCreateInfoKHR getBasicSwapchainParameters (Type						wsiType,
													  const InstanceInterface&	vki,
													  VkPhysicalDevice			physicalDevice,
													  VkSurfaceKHR				surface,
													  const tcu::UVec2&			desiredSize,
													  deUint32					desiredImageCount)
{
	const VkSurfaceCapabilitiesKHR		capabilities		= getPhysicalDeviceSurfaceCapabilities(vki,
																								   physicalDevice,
																								   surface);
	const vector<VkSurfaceFormatKHR>	formats				= getPhysicalDeviceSurfaceFormats(vki,
																							  physicalDevice,
																							  surface);
	const PlatformProperties&			platformProperties	= getPlatformProperties(wsiType);
	const VkSurfaceTransformFlagBitsKHR transform			= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const VkSwapchainCreateInfoKHR		parameters			=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		(VkSwapchainCreateFlagsKHR)0,
		surface,
		de::clamp(desiredImageCount, capabilities.minImageCount, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : capabilities.minImageCount + desiredImageCount),
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE
			? capabilities.currentExtent : vk::makeExtent2D(desiredSize.x(), desiredSize.y())),
		1u,									// imageArrayLayers
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		transform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(VkSwapchainKHR)0					// oldSwapchain
	};

	return parameters;
}

typedef de::SharedPtr<Unique<VkCommandBuffer> >	CommandBufferSp;
typedef de::SharedPtr<Unique<VkFence> >			FenceSp;
typedef de::SharedPtr<Unique<VkSemaphore> >		SemaphoreSp;

vector<FenceSp> createFences (const DeviceInterface&	vkd,
							  const VkDevice			device,
							  size_t					numFences,
							  bool                      isSignaled = true)
{
	vector<FenceSp> fences(numFences);

	for (size_t ndx = 0; ndx < numFences; ++ndx)
		fences[ndx] = FenceSp(new Unique<VkFence>(createFence(vkd, device, (isSignaled) ? vk::VK_FENCE_CREATE_SIGNALED_BIT : 0)));

	return fences;
}

vector<SemaphoreSp> createSemaphores (const DeviceInterface&	vkd,
									  const VkDevice			device,
									  size_t					numSemaphores)
{
	vector<SemaphoreSp> semaphores(numSemaphores);

	for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
		semaphores[ndx] = SemaphoreSp(new Unique<VkSemaphore>(createSemaphore(vkd, device)));

	return semaphores;
}

vector<CommandBufferSp> allocateCommandBuffers (const DeviceInterface&		vkd,
												const VkDevice				device,
												const VkCommandPool			commandPool,
												const VkCommandBufferLevel	level,
												const size_t				numCommandBuffers)
{
	vector<CommandBufferSp>				buffers		(numCommandBuffers);

	for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
		buffers[ndx] = CommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

	return buffers;
}

class AcquireNextImageWrapper
{
public:

	AcquireNextImageWrapper(const DeviceInterface&	vkd,
							VkDevice				device,
							deUint32				deviceMask,
							VkSwapchainKHR			swapchain,
							deUint64				timeout)
		: m_vkd			(vkd)
		, m_device		(device)
		, m_swapchain	(swapchain)
		, m_timeout		(timeout)
	{
		DE_UNREF(deviceMask);	// needed for compatibility with acquireNextImage2KHR
	}

	bool featureAvailable(Context&)
	{
		return true;			// needed for compatibility with acquireNextImage2KHR
	}

	VkResult call(VkSemaphore semaphore, VkFence fence, deUint32* imageIndex)
	{
		return m_vkd.acquireNextImageKHR(m_device,
										 m_swapchain,
										 m_timeout,
										 semaphore,
										 fence,
										 imageIndex);
	}

protected:

	const DeviceInterface&	m_vkd;
	VkDevice				m_device;
	VkSwapchainKHR			m_swapchain;
	deUint64				m_timeout;
};

class AcquireNextImage2Wrapper
{
public:

	AcquireNextImage2Wrapper(const DeviceInterface&	vkd,
							 VkDevice				device,
							 deUint32				deviceMask,
							 VkSwapchainKHR			swapchain,
							 deUint64				timeout)
		: m_vkd		(vkd)
		, m_device	(device)
	{
		m_info.sType		= VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
		m_info.pNext		= DE_NULL;
		m_info.swapchain	= swapchain;
		m_info.timeout		= timeout;
		m_info.semaphore	= DE_NULL;
		m_info.fence		= DE_NULL;
		m_info.deviceMask	= deviceMask;
	}

	bool featureAvailable(Context& context)
	{
		return context.isDeviceFunctionalitySupported("VK_KHR_device_group");
	}

	VkResult call(VkSemaphore semaphore, VkFence fence, deUint32* imageIndex)
	{
		m_info.semaphore	= semaphore;
		m_info.fence		= fence;
		return m_vkd.acquireNextImage2KHR(m_device,
										  &m_info,
										  imageIndex);
	}

protected:

	const DeviceInterface&		m_vkd;
	VkDevice					m_device;
	VkAcquireNextImageInfoKHR	m_info;
};


template <typename AcquireWrapperType>
tcu::TestStatus basicRenderTest (Context& context, Type wsiType)
{
	const tcu::UVec2				desiredSize					(256, 256);
	const InstanceHelper			instHelper					(context, wsiType);
	const NativeObjects				native						(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface						(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper				devHelper					(context, instHelper.vki, instHelper.instance, *surface);
	const DeviceInterface&			vkd							= devHelper.vkd;
	const VkDevice					device						= *devHelper.device;
	SimpleAllocator					allocator					(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
	const VkSwapchainCreateInfoKHR	swapchainInfo				= getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const Unique<VkSwapchainKHR>	swapchain					(createSwapchainKHR(vkd, device, &swapchainInfo));
	const vector<VkImage>			swapchainImages				= getSwapchainImages(vkd, device, *swapchain);

	AcquireWrapperType acquireImageWrapper(vkd, device, 1u, *swapchain, std::numeric_limits<deUint64>::max());
	if (!acquireImageWrapper.featureAvailable(context))
		TCU_THROW(NotSupportedError, "Required extension is not supported");

	const WsiTriangleRenderer		renderer					(vkd,
																 device,
																 allocator,
																 context.getBinaryCollection(),
																 false,
																 swapchainImages,
																 swapchainImages,
																 swapchainInfo.imageFormat,
																 tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

	const Unique<VkCommandPool>		commandPool					(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

	const size_t					maxQueuedFrames				= swapchainImages.size()*2;

	// We need to keep hold of fences from vkAcquireNextImage(2)KHR to actually
	// limit number of frames we allow to be queued.
	const vector<FenceSp>			imageReadyFences			(createFences(vkd, device, maxQueuedFrames));

	// We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to pass
	// the semaphore in same time as the fence we use to meter rendering.
	const vector<SemaphoreSp>		imageReadySemaphores		(createSemaphores(vkd, device, maxQueuedFrames+1));

	// For rest we simply need maxQueuedFrames as we will wait for image
	// from frameNdx-maxQueuedFrames to become available to us, guaranteeing that
	// previous uses must have completed.
	const vector<SemaphoreSp>		renderingCompleteSemaphores	(createSemaphores(vkd, device, maxQueuedFrames));
	const vector<CommandBufferSp>	commandBuffers				(allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames));

	try
	{
		const deUint32	numFramesToRender	= 60*10;

		for (deUint32 frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
		{
			const VkFence		imageReadyFence		= **imageReadyFences[frameNdx%imageReadyFences.size()];
			const VkSemaphore	imageReadySemaphore	= **imageReadySemaphores[frameNdx%imageReadySemaphores.size()];
			deUint32			imageNdx			= ~0u;

			VK_CHECK(vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));
			VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

			{
				const VkResult	acquireResult	= acquireImageWrapper.call(imageReadySemaphore, (VkFence)0, &imageNdx);

				if (acquireResult == VK_SUBOPTIMAL_KHR)
					context.getTestContext().getLog() << TestLog::Message << "Got " << acquireResult << " at frame " << frameNdx << TestLog::EndMessage;
				else
					VK_CHECK(acquireResult);
			}

			TCU_CHECK((size_t)imageNdx < swapchainImages.size());

			{
				const VkSemaphore			renderingCompleteSemaphore	= **renderingCompleteSemaphores[frameNdx%renderingCompleteSemaphores.size()];
				const VkCommandBuffer		commandBuffer				= **commandBuffers[frameNdx%commandBuffers.size()];
				const VkPipelineStageFlags	waitDstStage				= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				const VkSubmitInfo			submitInfo					=
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,
					DE_NULL,
					1u,
					&imageReadySemaphore,
					&waitDstStage,
					1u,
					&commandBuffer,
					1u,
					&renderingCompleteSemaphore
				};
				const VkPresentInfoKHR		presentInfo					=
				{
					VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					DE_NULL,
					1u,
					&renderingCompleteSemaphore,
					1u,
					&*swapchain,
					&imageNdx,
					(VkResult*)DE_NULL
				};

				renderer.recordFrame(commandBuffer, imageNdx, frameNdx);
				VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, imageReadyFence));
				VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
			}
		}

		VK_CHECK(vkd.deviceWaitIdle(device));
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	return tcu::TestStatus::pass("Rendering tests succeeded");
}

class FrameStreamObjects
{
public:
	struct FrameObjects
	{
		const vk::VkFence&			renderCompleteFence;
		const vk::VkSemaphore&		renderCompleteSemaphore;
		const vk::VkSemaphore&		imageAvailableSemaphore;
		const vk::VkCommandBuffer&	commandBuffer;
	};

	FrameStreamObjects (const vk::DeviceInterface& vkd, vk::VkDevice device, vk::VkCommandPool cmdPool, size_t maxQueuedFrames)
		: renderingCompleteFences(createFences(vkd, device, maxQueuedFrames))
		, renderingCompleteSemaphores(createSemaphores(vkd, device, maxQueuedFrames))
		, imageAvailableSemaphores(createSemaphores(vkd, device, maxQueuedFrames))
		, commandBuffers(allocateCommandBuffers(vkd, device, cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames))
		, m_maxQueuedFrames(maxQueuedFrames)
		, m_nextFrame(0u)
	{}

	size_t frameNumber (void) const { DE_ASSERT(m_nextFrame > 0u); return m_nextFrame - 1u; }

	FrameObjects newFrame ()
	{
		const size_t mod = m_nextFrame % m_maxQueuedFrames;
		FrameObjects ret =
		{
			**renderingCompleteFences[mod],
			**renderingCompleteSemaphores[mod],
			**imageAvailableSemaphores[mod],
			**commandBuffers[mod],
		};
		++m_nextFrame;
		return ret;
	}

private:
	const vector<FenceSp>			renderingCompleteFences;
	const vector<SemaphoreSp>		renderingCompleteSemaphores;
	const vector<SemaphoreSp>		imageAvailableSemaphores;
	const vector<CommandBufferSp>	commandBuffers;

	const size_t	m_maxQueuedFrames;
	size_t			m_nextFrame;
};

struct MultiSwapchainParams
{
	Type	wsiType;
	size_t	swapchainCount;
};

struct AccumulatedPresentInfo
{
	vector<VkSemaphore>		semaphores;
	vector<VkSwapchainKHR>	swapchains;
	vector<deUint32>		imageIndices;

	AccumulatedPresentInfo ()
		: semaphores(), swapchains(), imageIndices()
	{
	}

	void push_back (VkSemaphore sem, VkSwapchainKHR sc, deUint32 index)
	{
		semaphores.push_back(sem);
		swapchains.push_back(sc);
		imageIndices.push_back(index);
	}

	void reset ()
	{
		semaphores.resize(0);
		swapchains.resize(0);
		imageIndices.resize(0);
	}

	size_t size () const
	{
		// Any of the vectors would do.
		return semaphores.size();
	}
};

template <typename AcquireWrapperType>
tcu::TestStatus multiSwapchainRenderTest (Context& context, MultiSwapchainParams params)
{
	DE_ASSERT(params.swapchainCount > 0);

	const tcu::UVec2		desiredSize	(256, 256);
	const InstanceHelper	instHelper	(context, params.wsiType);

	// Create native window system objects, surfaces and helper surface vector.
	std::unique_ptr<NativeObjects> native;
	try
	{
		native.reset(new NativeObjects(context, instHelper.supportedExtensions, params.wsiType, params.swapchainCount, tcu::just(desiredSize)));
	}
	catch(tcu::ResourceError&)
	{
		std::ostringstream msg;
		msg << "Unable to create " << params.swapchainCount << " windows";
		TCU_THROW(NotSupportedError, msg.str());
	}

	vector<Move<VkSurfaceKHR>>	surface;
	vector<VkSurfaceKHR>		surfaceKHR;	// The plain Vulkan objects from the vector above.

	for (size_t i = 0; i < params.swapchainCount; ++i)
	{
		surface.emplace_back(createSurface(instHelper.vki, instHelper.instance, params.wsiType, native->getDisplay(), native->getWindow(i)));
		surfaceKHR.push_back(surface.back().get());
	}

	// Create a device compatible with all surfaces.
	const DeviceHelper		devHelper	(context, instHelper.vki, instHelper.instance, surfaceKHR);
	const DeviceInterface&	vkd			= devHelper.vkd;
	const VkDevice			device		= *devHelper.device;
	SimpleAllocator			allocator	(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));

	// Create several swapchains and images.
	vector<VkSwapchainCreateInfoKHR>	swapchainInfo;
	vector<Move<VkSwapchainKHR>>		swapchain;
	vector<vector<VkImage>>				swapchainImages;
	vector<AcquireWrapperType>			acquireImageWrapper;
	for (size_t i = 0; i < params.swapchainCount; ++i)
	{
		swapchainInfo.emplace_back(getBasicSwapchainParameters(params.wsiType, instHelper.vki, devHelper.physicalDevice, *surface[i], desiredSize, 2));
		swapchain.emplace_back(createSwapchainKHR(vkd, device, &swapchainInfo.back()));
		swapchainImages.emplace_back(getSwapchainImages(vkd, device, swapchain.back().get()));
		acquireImageWrapper.emplace_back(vkd, device, 1u, swapchain.back().get(), std::numeric_limits<deUint64>::max());
	}

	// Every acquire wrapper requires the same features, so we only check the first one.
	if (!acquireImageWrapper.front().featureAvailable(context))
		TCU_THROW(NotSupportedError, "Required extension is not supported");

	// Renderer per swapchain.
	vector<WsiTriangleRenderer> renderer;
	for (size_t i = 0; i < params.swapchainCount; ++i)
	{
		renderer.emplace_back(vkd,
							  device,
							  allocator,
							  context.getBinaryCollection(),
							  false,
							  swapchainImages[i],
							  swapchainImages[i],
							  swapchainInfo[i].imageFormat,
							  tcu::UVec2(swapchainInfo[i].imageExtent.width, swapchainInfo[i].imageExtent.height));
	}

	const Unique<VkCommandPool>	commandPool			(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));
	const size_t				maxQueuedFrames		= swapchainImages.front().size()*2;	// Limit in-flight frames.

	vector<FrameStreamObjects>	frameStreamObjects;
	for (size_t i = 0; i < params.swapchainCount; ++i)
		frameStreamObjects.emplace_back(vkd, device, commandPool.get(), maxQueuedFrames);

	try
	{
		// 3 seconds for 60 Hz screens.
		const deUint32			kNumFramesToRender		= 60*3*static_cast<deUint32>(params.swapchainCount);
		AccumulatedPresentInfo	accumulatedPresentInfo;

		for (size_t frameNdx = 0; frameNdx < kNumFramesToRender; ++frameNdx)
		{
			size_t		swapchainIndex	= frameNdx % params.swapchainCount;
			auto&		fsObjects		= frameStreamObjects[swapchainIndex];
			auto		frameObjects	= fsObjects.newFrame();
			deUint32	imageNdx		= std::numeric_limits<deUint32>::max();

			VK_CHECK(vkd.waitForFences(device, 1u, &frameObjects.renderCompleteFence, VK_TRUE, std::numeric_limits<deUint64>::max()));
			VK_CHECK(vkd.resetFences(device, 1u, &frameObjects.renderCompleteFence));

			{
				const VkResult acquireResult = acquireImageWrapper[swapchainIndex].call(frameObjects.imageAvailableSemaphore, (VkFence)DE_NULL, &imageNdx);
				if (acquireResult == VK_SUBOPTIMAL_KHR)
					context.getTestContext().getLog() << TestLog::Message << "Got " << acquireResult << " at frame " << frameNdx << TestLog::EndMessage;
				else
					VK_CHECK(acquireResult);
			}

			TCU_CHECK(static_cast<size_t>(imageNdx) < swapchainImages[swapchainIndex].size());

			{
				const VkPipelineStageFlags	waitDstStage	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				const VkSubmitInfo			submitInfo		=
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,
					DE_NULL,
					1u,
					&frameObjects.imageAvailableSemaphore,
					&waitDstStage,
					1u,
					&frameObjects.commandBuffer,
					1u,
					&frameObjects.renderCompleteSemaphore,
				};

				renderer[swapchainIndex].recordFrame(frameObjects.commandBuffer, imageNdx, static_cast<deUint32>(frameNdx));
				VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, frameObjects.renderCompleteFence));

				// Save present information for the current frame.
				accumulatedPresentInfo.push_back(frameObjects.renderCompleteSemaphore, swapchain[swapchainIndex].get(), imageNdx);

				// Present frames when we have accumulated one frame per swapchain.
				if (accumulatedPresentInfo.size() == params.swapchainCount)
				{
					DE_ASSERT(	accumulatedPresentInfo.semaphores.size() == accumulatedPresentInfo.swapchains.size()	&&
								accumulatedPresentInfo.semaphores.size() == accumulatedPresentInfo.imageIndices.size()	);

					vector<VkResult> results(params.swapchainCount, VK_ERROR_DEVICE_LOST);

					const VkPresentInfoKHR presentInfo =
					{
						VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
						DE_NULL,
						static_cast<deUint32>(accumulatedPresentInfo.semaphores.size()),
						accumulatedPresentInfo.semaphores.data(),
						static_cast<deUint32>(accumulatedPresentInfo.swapchains.size()),
						accumulatedPresentInfo.swapchains.data(),
						accumulatedPresentInfo.imageIndices.data(),
						results.data(),
					};

					// Check both the global result and the individual results.
					VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
					for (const auto& result : results)
						VK_CHECK_WSI(result);

					accumulatedPresentInfo.reset();
				}
			}
		}

		VK_CHECK(vkd.deviceWaitIdle(device));
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	return tcu::TestStatus::pass("Rendering tests succeeded");
}

tcu::TestStatus deviceGroupRenderTest (Context& context, Type wsiType)
{
	const InstanceHelper		instHelper			(context, wsiType, vector<string>(1, string("VK_KHR_device_group_creation")));
	const tcu::CommandLine&		cmdLine				= context.getTestContext().getCommandLine();
	VkPhysicalDevice			physicalDevice		= chooseDevice(instHelper.vki, instHelper.instance, cmdLine);
	const Extensions&			supportedExtensions	= enumerateDeviceExtensionProperties(instHelper.vki, physicalDevice, DE_NULL);

	std::vector<const char*> deviceExtensions;
	deviceExtensions.push_back("VK_KHR_swapchain");
	if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_device_group"))
		deviceExtensions.push_back("VK_KHR_device_group");

	for (std::size_t ndx = 0; ndx < deviceExtensions.size(); ++ndx)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(deviceExtensions[ndx])))
			TCU_THROW(NotSupportedError, (string(deviceExtensions[ndx]) + " is not supported").c_str());
	}

	const tcu::UVec2								desiredSize					(256, 256);
	const NativeObjects								native						(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>						surface						(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));

	const deUint32									devGroupIdx					= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32									deviceIdx					= context.getTestContext().getCommandLine().getVKDeviceId() - 1u;
	const vector<VkPhysicalDeviceGroupProperties>	deviceGroupProps			= enumeratePhysicalDeviceGroups(instHelper.vki, instHelper.instance);
	deUint32										physicalDevicesInGroupCount	= deviceGroupProps[devGroupIdx].physicalDeviceCount;
	const VkPhysicalDevice*							physicalDevicesInGroup		= deviceGroupProps[devGroupIdx].physicalDevices;
	deUint32										queueFamilyIndex			= chooseQueueFamilyIndex(instHelper.vki, physicalDevicesInGroup[deviceIdx], *surface);
	const std::vector<VkQueueFamilyProperties>		queueProps					= getPhysicalDeviceQueueFamilyProperties(instHelper.vki, physicalDevicesInGroup[deviceIdx]);
	const float										queuePriority				= 1.0f;
	const deUint32									firstDeviceID				= 0;
	const deUint32									secondDeviceID				= 1;

	// create a device group
	const VkDeviceGroupDeviceCreateInfo groupDeviceInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR,			// stype
		DE_NULL,														// pNext
		physicalDevicesInGroupCount,									// physicalDeviceCount
		physicalDevicesInGroup											// physicalDevices
	};
	const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,						// type
		DE_NULL,														// pNext
		(VkDeviceQueueCreateFlags)0u,									// flags
		queueFamilyIndex,												// queueFamilyIndex
		1u,																// queueCount
		&queuePriority,													// pQueuePriorities
	};

	const VkDeviceCreateInfo	deviceCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							// sType
		&groupDeviceInfo,												// pNext
		(VkDeviceCreateFlags)0u,										// flags
		1,																// queueRecordCount
		&deviceQueueCreateInfo,											// pRequestedQueues
		0,																// layerCount
		DE_NULL,														// ppEnabledLayerNames
		deUint32(deviceExtensions.size()),								// enabledExtensionCount
		&deviceExtensions[0],											// ppEnabledExtensionNames
		DE_NULL,														// pEnabledFeatures
	};

	Move<VkDevice>					groupDevice					= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), instHelper.instance, instHelper.vki, physicalDevicesInGroup[deviceIdx], &deviceCreateInfo);
	const DeviceDriver				vkd							(context.getPlatformInterface(), instHelper.instance, *groupDevice);
	VkQueue							queue						(getDeviceQueue(vkd, *groupDevice, queueFamilyIndex, 0));
	SimpleAllocator					allocator					(vkd, *groupDevice, getPhysicalDeviceMemoryProperties(instHelper.vki, physicalDevicesInGroup[deviceIdx]));

	// create swapchain for device group
	VkDeviceGroupSwapchainCreateInfoKHR deviceGroupSwapchainInfo = initVulkanStructure();
	deviceGroupSwapchainInfo.modes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;

	VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(wsiType,
																		 instHelper.vki,
																		 physicalDevicesInGroup[deviceIdx],
																		 *surface,
																		 desiredSize,
																		 2);
	swapchainInfo.pNext = &deviceGroupSwapchainInfo;

	const Unique<VkSwapchainKHR>	swapchain					(createSwapchainKHR(vkd, *groupDevice, &swapchainInfo));
	const vector<VkImage>			swapchainImages				= getSwapchainImages(vkd, *groupDevice, *swapchain);

	const WsiTriangleRenderer		renderer					(vkd,
																 *groupDevice,
																 allocator,
																 context.getBinaryCollection(),
																 false,
																 swapchainImages,
																 swapchainImages,
																 swapchainInfo.imageFormat,
																 tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

	const Unique<VkCommandPool>		commandPool					(createCommandPool(vkd, *groupDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));

	const size_t					maxQueuedFrames				= swapchainImages.size()*2;

	// We need to keep hold of fences from vkAcquireNextImage2KHR
	// to actually limit number of frames we allow to be queued.
	const vector<FenceSp>			imageReadyFences			(createFences(vkd, *groupDevice, maxQueuedFrames));

	// We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to
	// pass the semaphore in same time as the fence we use to meter rendering.
	const vector<SemaphoreSp>		imageReadySemaphores		(createSemaphores(vkd, *groupDevice, maxQueuedFrames+1));

	// For rest we simply need maxQueuedFrames as we will wait for image from frameNdx-maxQueuedFrames
	// to become available to us, guaranteeing that previous uses must have completed.
	const vector<SemaphoreSp>		renderingCompleteSemaphores	(createSemaphores(vkd, *groupDevice, maxQueuedFrames));
	const vector<CommandBufferSp>	commandBuffers				(allocateCommandBuffers(vkd, *groupDevice, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames));

	try
	{
		const deUint32	numFramesToRender = 60*10;

		for (deUint32 frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
		{
			const VkFence		imageReadyFence		= **imageReadyFences[frameNdx%imageReadyFences.size()];
			const VkSemaphore	imageReadySemaphore	= **imageReadySemaphores[frameNdx%imageReadySemaphores.size()];
			deUint32			imageNdx			= ~0u;

			VK_CHECK(vkd.waitForFences(*groupDevice, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));
			VK_CHECK(vkd.resetFences(*groupDevice, 1, &imageReadyFence));

			{
				VkAcquireNextImageInfoKHR acquireNextImageInfo =
				{
					VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
					DE_NULL,
					*swapchain,
					std::numeric_limits<deUint64>::max(),
					imageReadySemaphore,
					(VkFence)0,
					(1 << firstDeviceID)
				};

				const VkResult acquireResult = vkd.acquireNextImage2KHR(*groupDevice, &acquireNextImageInfo, &imageNdx);

				if (acquireResult == VK_SUBOPTIMAL_KHR)
					context.getTestContext().getLog() << TestLog::Message << "Got " << acquireResult << " at frame " << frameNdx << TestLog::EndMessage;
				else
					VK_CHECK(acquireResult);
			}

			TCU_CHECK((size_t)imageNdx < swapchainImages.size());

			{
				const VkSemaphore			renderingCompleteSemaphore	= **renderingCompleteSemaphores[frameNdx%renderingCompleteSemaphores.size()];
				const VkCommandBuffer		commandBuffer				= **commandBuffers[frameNdx%commandBuffers.size()];
				const VkPipelineStageFlags	waitDstStage				= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

				// render triangle using one or two subdevices when available
				renderer.recordDeviceGroupFrame(commandBuffer, firstDeviceID, secondDeviceID, physicalDevicesInGroupCount, imageNdx, frameNdx);

				// submit queue
				deUint32 deviceMask = (1 << firstDeviceID);
				std::vector<deUint32> deviceIndices(1, firstDeviceID);
				if (physicalDevicesInGroupCount > 1)
				{
					deviceMask |= (1 << secondDeviceID);
					deviceIndices.push_back(secondDeviceID);
				}
				const VkDeviceGroupSubmitInfo deviceGroupSubmitInfo =
				{
					VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR,		// sType
					DE_NULL,											// pNext
					deUint32(deviceIndices.size()),						// waitSemaphoreCount
					&deviceIndices[0],									// pWaitSemaphoreDeviceIndices
					1u,													// commandBufferCount
					&deviceMask,										// pCommandBufferDeviceMasks
					deUint32(deviceIndices.size()),						// signalSemaphoreCount
					&deviceIndices[0],									// pSignalSemaphoreDeviceIndices
				};
				const VkSubmitInfo submitInfo =
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
					&deviceGroupSubmitInfo,								// pNext
					1u,													// waitSemaphoreCount
					&imageReadySemaphore,								// pWaitSemaphores
					&waitDstStage,										// pWaitDstStageMask
					1u,													// commandBufferCount
					&commandBuffer,										// pCommandBuffers
					1u,													// signalSemaphoreCount
					&renderingCompleteSemaphore,						// pSignalSemaphores
				};
				VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, imageReadyFence));

				// present swapchain image
				deviceMask = (1 << firstDeviceID);
				const VkDeviceGroupPresentInfoKHR deviceGroupPresentInfo =
				{
					VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR,
					DE_NULL,
					1u,
					&deviceMask,
					VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR,
				};
				const VkPresentInfoKHR presentInfo =
				{
					VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					&deviceGroupPresentInfo,
					1u,
					&renderingCompleteSemaphore,
					1u,
					&*swapchain,
					&imageNdx,
					(VkResult*)DE_NULL
				};
				VK_CHECK_WSI(vkd.queuePresentKHR(queue, &presentInfo));
			}
		}

		VK_CHECK(vkd.deviceWaitIdle(*groupDevice));
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(*groupDevice);
		throw;
	}

	return tcu::TestStatus::pass("Rendering tests succeeded");
}

tcu::TestStatus deviceGroupRenderTest2 (Context& context, Type wsiType)
{
	const InstanceHelper		instHelper			(context, wsiType, vector<string>(1, string("VK_KHR_device_group_creation")));
	const tcu::CommandLine&		cmdLine				= context.getTestContext().getCommandLine();
	VkPhysicalDevice			physicalDevice		= chooseDevice(instHelper.vki, instHelper.instance, cmdLine);
	const Extensions&			deviceExtensions	= enumerateDeviceExtensionProperties(instHelper.vki, physicalDevice, DE_NULL);

	// structures this tests checks were added in revision 69
	if (!isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_swapchain", 69)))
		TCU_THROW(NotSupportedError, "Required extension revision is not supported");

	std::vector<const char*> requiredExtensions;
	requiredExtensions.push_back("VK_KHR_swapchain");
	if (!isCoreDeviceExtension(context.getUsedApiVersion(), "VK_KHR_device_group"))
	{
		requiredExtensions.push_back("VK_KHR_device_group");
		if (!isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_device_group")))
			TCU_THROW(NotSupportedError, "VK_KHR_device_group is not supported");
	}

	const tcu::UVec2								desiredSize					(256, 256);
	const NativeObjects								native						(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>						surface						(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));

	const deUint32									devGroupIdx					= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32									deviceIdx					= context.getTestContext().getCommandLine().getVKDeviceId() - 1u;
	const vector<VkPhysicalDeviceGroupProperties>	deviceGroupProps			= enumeratePhysicalDeviceGroups(instHelper.vki, instHelper.instance);
	deUint32										physicalDevicesInGroupCount	= deviceGroupProps[devGroupIdx].physicalDeviceCount;
	const VkPhysicalDevice*							physicalDevicesInGroup		= deviceGroupProps[devGroupIdx].physicalDevices;
	deUint32										queueFamilyIndex			= chooseQueueFamilyIndex(instHelper.vki, physicalDevicesInGroup[deviceIdx], *surface);
	const std::vector<VkQueueFamilyProperties>		queueProps					= getPhysicalDeviceQueueFamilyProperties(instHelper.vki, physicalDevicesInGroup[deviceIdx]);
	const float										queuePriority				= 1.0f;
	const deUint32									firstDeviceID				= 0;
	const deUint32									secondDeviceID				= 1;
	const deUint32									deviceIndices[]				= { firstDeviceID, secondDeviceID };

	if (physicalDevicesInGroupCount < 2)
		TCU_THROW(NotSupportedError, "Test requires more than 1 device in device group");

	// create a device group
	const VkDeviceGroupDeviceCreateInfo groupDeviceInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR,			// stype
		DE_NULL,														// pNext
		physicalDevicesInGroupCount,									// physicalDeviceCount
		physicalDevicesInGroup											// physicalDevices
	};
	const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,						// type
		DE_NULL,														// pNext
		(VkDeviceQueueCreateFlags)0u,									// flags
		queueFamilyIndex,												// queueFamilyIndex
		1u,																// queueCount
		&queuePriority,													// pQueuePriorities
	};

	const VkDeviceCreateInfo deviceCreateInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							// sType
		&groupDeviceInfo,												// pNext
		(VkDeviceCreateFlags)0u,										// flags
		1,																// queueRecordCount
		&deviceQueueCreateInfo,											// pRequestedQueues
		0,																// layerCount
		DE_NULL,														// ppEnabledLayerNames
		deUint32(requiredExtensions.size()),							// enabledExtensionCount
		&requiredExtensions[0],											// ppEnabledExtensionNames
		DE_NULL,														// pEnabledFeatures
	};

	Move<VkDevice>						groupDevice			= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(), instHelper.instance, instHelper.vki, physicalDevicesInGroup[deviceIdx], &deviceCreateInfo);
	const DeviceDriver					vkd					(context.getPlatformInterface(), instHelper.instance, *groupDevice);
	VkQueue								queue				(getDeviceQueue(vkd, *groupDevice, queueFamilyIndex, 0));
	SimpleAllocator						allocator			(vkd, *groupDevice, getPhysicalDeviceMemoryProperties(instHelper.vki, physicalDevicesInGroup[deviceIdx]));

	// create swapchain for device group
	const VkSurfaceCapabilitiesKHR		capabilities		= getPhysicalDeviceSurfaceCapabilities(instHelper.vki, physicalDevice, *surface);
	const vector<VkSurfaceFormatKHR>	formats				= getPhysicalDeviceSurfaceFormats(instHelper.vki, physicalDevice, *surface);
	const PlatformProperties&			platformProperties	= getPlatformProperties(wsiType);
	const VkSurfaceTransformFlagBitsKHR transform			= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const deUint32						desiredImageCount	= 2;

	struct VkDeviceGroupSwapchainCreateInfoKHR deviceGroupSwapchainInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR
	};
	const VkSwapchainCreateInfoKHR swapchainInfo =
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		&deviceGroupSwapchainInfo,
		VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR,
		*surface,
		de::clamp(desiredImageCount, capabilities.minImageCount, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : capabilities.minImageCount + desiredImageCount),
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE
			? capabilities.currentExtent : vk::makeExtent2D(desiredSize.x(), desiredSize.y())),
		1u,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		transform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,
		(VkSwapchainKHR)0
	};

	const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(vkd, *groupDevice, &swapchainInfo));
	deUint32						numImages	= 0;
	VK_CHECK(vkd.getSwapchainImagesKHR(*groupDevice, *swapchain, &numImages, DE_NULL));
	if (numImages == 0)
		return tcu::TestStatus::pass("Pass");

	VkImageSwapchainCreateInfoKHR imageSwapchainCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		*swapchain
	};

	VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&imageSwapchainCreateInfo,
		VK_IMAGE_CREATE_ALIAS_BIT |
		VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR,		// flags
		VK_IMAGE_TYPE_2D,											// imageType
		formats[0].format,											// format
		{															// extent
			desiredSize.x(),										//   width
			desiredSize.y(),										//   height
			1u														//   depth
		},
		1u,															// mipLevels
		1u,															// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,										// samples
		VK_IMAGE_TILING_OPTIMAL,									// tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,						// usage
		VK_SHARING_MODE_EXCLUSIVE,									// sharingMode
		0u,															// queueFamilyIndexCount
		DE_NULL,													// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED									// initialLayout
	};

	typedef vk::Unique<VkImage>			UniqueImage;
	typedef de::SharedPtr<UniqueImage>	ImageSp;

	vector<ImageSp>								images							(numImages);
	vector<VkImage>								rawImages						(numImages);
	vector<ImageSp>								imagesSfr						(numImages);
	vector<VkImage>								rawImagesSfr					(numImages);
	vector<VkBindImageMemorySwapchainInfoKHR>	bindImageMemorySwapchainInfo	(numImages);

	// Create non-SFR image aliases for image layout transition
	{
		vector<VkBindImageMemoryInfo>				bindImageMemoryInfos			(numImages);

		for (deUint32 idx = 0; idx < numImages; ++idx)
		{
			// Create image
			images[idx] = ImageSp(new UniqueImage(createImage(vkd, *groupDevice, &imageCreateInfo)));

			VkBindImageMemorySwapchainInfoKHR bimsInfo =
			{
				VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,
				DE_NULL,
				*swapchain,
				idx
			};
			bindImageMemorySwapchainInfo[idx] = bimsInfo;

			VkBindImageMemoryInfo bimInfo =
			{
				VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
				&bindImageMemorySwapchainInfo[idx],
				**images[idx],
				DE_NULL,				// If the pNext chain includes an instance of VkBindImageMemorySwapchainInfoKHR, memory must be VK_NULL_HANDLE
				0u						// If swapchain <in VkBindImageMemorySwapchainInfoKHR> is not NULL, the swapchain and imageIndex are used to determine the memory that the image is bound to, instead of memory and memoryOffset.
			};
			bindImageMemoryInfos[idx]	= bimInfo;
			rawImages[idx]				= **images[idx];
		}

		VK_CHECK(vkd.bindImageMemory2(*groupDevice, numImages, &bindImageMemoryInfos[0]));
	}

	// Create the SFR images
	{
		vector<VkBindImageMemoryDeviceGroupInfo	>	bindImageMemoryDeviceGroupInfo	(numImages);
		vector<VkBindImageMemoryInfo>				bindImageMemoryInfos			(numImages);
		for (deUint32 idx = 0; idx < numImages; ++idx)
		{
			// Create image
			imagesSfr[idx] = ImageSp(new UniqueImage(createImage(vkd, *groupDevice, &imageCreateInfo)));

		// Split into 2 vertical halves
		// NOTE: the same split has to be done also in WsiTriangleRenderer::recordDeviceGroupFrame
		const deUint32	halfWidth			= desiredSize.x() / 2;
		const deUint32	height				= desiredSize.y();
		const VkRect2D	sfrRects[] =
		{
			{	{ 0, 0 },					{ halfWidth, height }	},		// offset, extent
			{	{ (deInt32)halfWidth, 0 },	{ halfWidth, height }	},		// offset, extent
			{	{ 0, 0 },					{ halfWidth, height }	},		// offset, extent
			{	{ (deInt32)halfWidth, 0 },	{ halfWidth, height }	}		// offset, extent
		};

		VkBindImageMemoryDeviceGroupInfo bimdgInfo =
		{
			VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO,
			&bindImageMemorySwapchainInfo[idx],
			DE_LENGTH_OF_ARRAY(deviceIndices),
			deviceIndices,
			DE_LENGTH_OF_ARRAY(sfrRects),
			sfrRects
		};
		bindImageMemoryDeviceGroupInfo[idx] = bimdgInfo;

		VkBindImageMemoryInfo bimInfo =
		{
			VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
			&bindImageMemoryDeviceGroupInfo[idx],
				**imagesSfr[idx],
			DE_NULL,				// If the pNext chain includes an instance of VkBindImageMemorySwapchainInfoKHR, memory must be VK_NULL_HANDLE
			0u						// If swapchain <in VkBindImageMemorySwapchainInfoKHR> is not NULL, the swapchain and imageIndex are used to determine the memory that the image is bound to, instead of memory and memoryOffset.
		};
		bindImageMemoryInfos[idx]	= bimInfo;
			rawImagesSfr[idx]			= **imagesSfr[idx];
	}

	VK_CHECK(vkd.bindImageMemory2(*groupDevice, numImages, &bindImageMemoryInfos[0]));
	}

	VkPeerMemoryFeatureFlags peerMemoryFeatures = 0u;
	vkd.getDeviceGroupPeerMemoryFeatures(*groupDevice, 0, firstDeviceID, secondDeviceID, &peerMemoryFeatures);
	bool explicitLayoutTransitions = !(peerMemoryFeatures & VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT) ||
									 !(peerMemoryFeatures & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT);

	const WsiTriangleRenderer			renderer					(vkd,
																 *groupDevice,
																 allocator,
																 context.getBinaryCollection(),
																 explicitLayoutTransitions,
																 rawImagesSfr,
																 rawImages,
																 swapchainInfo.imageFormat,
																 tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

	const Unique<VkCommandPool>		commandPool					(createCommandPool(vkd, *groupDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));

	const size_t					maxQueuedFrames				= rawImagesSfr.size()*2;

	// We need to keep hold of fences from vkAcquireNextImage2KHR
	// to actually limit number of frames we allow to be queued.
	const vector<FenceSp>			imageReadyFences			(createFences(vkd, *groupDevice, maxQueuedFrames));

	// We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to
	// pass the semaphore in same time as the fence we use to meter rendering.
	const vector<SemaphoreSp>		imageReadySemaphores		(createSemaphores(vkd, *groupDevice, maxQueuedFrames+1));

	// For rest we simply need maxQueuedFrames as we will wait for image from frameNdx-maxQueuedFrames
	// to become available to us, guaranteeing that previous uses must have completed.
	const vector<SemaphoreSp>		renderingCompleteSemaphores	(createSemaphores(vkd, *groupDevice, maxQueuedFrames));
	const vector<CommandBufferSp>	commandBuffers				(allocateCommandBuffers(vkd, *groupDevice, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames));

	try
	{
		const deUint32	numFramesToRender = 60*10;

		for (deUint32 frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
		{
			const VkFence		imageReadyFence		= **imageReadyFences[frameNdx%imageReadyFences.size()];
			const VkSemaphore	imageReadySemaphore	= **imageReadySemaphores[frameNdx%imageReadySemaphores.size()];
			deUint32			imageNdx			= ~0u;

			VK_CHECK(vkd.waitForFences(*groupDevice, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));
			VK_CHECK(vkd.resetFences(*groupDevice, 1, &imageReadyFence));

			{
				VkAcquireNextImageInfoKHR acquireNextImageInfo =
				{
					VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
					DE_NULL,
					*swapchain,
					std::numeric_limits<deUint64>::max(),
					imageReadySemaphore,
					(VkFence)0,
					(1 << firstDeviceID)
				};

				const VkResult acquireResult = vkd.acquireNextImage2KHR(*groupDevice, &acquireNextImageInfo, &imageNdx);

				if (acquireResult == VK_SUBOPTIMAL_KHR)
					context.getTestContext().getLog() << TestLog::Message << "Got " << acquireResult << " at frame " << frameNdx << TestLog::EndMessage;
				else
					VK_CHECK(acquireResult);
			}

			TCU_CHECK((size_t)imageNdx < rawImagesSfr.size());

			{
				const VkSemaphore			renderingCompleteSemaphore	= **renderingCompleteSemaphores[frameNdx%renderingCompleteSemaphores.size()];
				const VkCommandBuffer		commandBuffer				= **commandBuffers[frameNdx%commandBuffers.size()];
				const VkPipelineStageFlags	waitDstStage				= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

				// render triangle using one or two subdevices when available
				renderer.recordDeviceGroupFrame(commandBuffer, firstDeviceID, secondDeviceID, physicalDevicesInGroupCount, imageNdx, frameNdx);

				// submit queue
				deUint32 deviceMask = (1 << firstDeviceID) | (1 << secondDeviceID);
				const VkDeviceGroupSubmitInfo deviceGroupSubmitInfo =
				{
					VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR,		// sType
					DE_NULL,											// pNext
					DE_LENGTH_OF_ARRAY(deviceIndices),					// waitSemaphoreCount
					deviceIndices,										// pWaitSemaphoreDeviceIndices
					1u,													// commandBufferCount
					&deviceMask,										// pCommandBufferDeviceMasks
					DE_LENGTH_OF_ARRAY(deviceIndices),					// signalSemaphoreCount
					deviceIndices,										// pSignalSemaphoreDeviceIndices
				};
				const VkSubmitInfo submitInfo =
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
					&deviceGroupSubmitInfo,								// pNext
					1u,													// waitSemaphoreCount
					&imageReadySemaphore,								// pWaitSemaphores
					&waitDstStage,										// pWaitDstStageMask
					1u,													// commandBufferCount
					&commandBuffer,										// pCommandBuffers
					1u,													// signalSemaphoreCount
					&renderingCompleteSemaphore,						// pSignalSemaphores
				};
				VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, imageReadyFence));

				// present swapchain image -  asume that first device has a presentation engine
				deviceMask = (1 << firstDeviceID);
				const VkDeviceGroupPresentInfoKHR deviceGroupPresentInfo =
				{
					VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR,
					DE_NULL,
					1u,
					&deviceMask,
					VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR,
				};
				const VkPresentInfoKHR presentInfo =
				{
					VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					&deviceGroupPresentInfo,
					1u,
					&renderingCompleteSemaphore,
					1u,
					&*swapchain,
					&imageNdx,
					(VkResult*)DE_NULL
				};
				VK_CHECK(vkd.queuePresentKHR(queue, &presentInfo));
			}
		}

		VK_CHECK(vkd.deviceWaitIdle(*groupDevice));
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(*groupDevice);
		throw;
	}

	return tcu::TestStatus::pass("Rendering tests succeeded");
}

vector<tcu::UVec2> getSwapchainSizeSequence (const VkSurfaceCapabilitiesKHR& capabilities, const tcu::UVec2& defaultSize)
{
	vector<tcu::UVec2> sizes(3);
	sizes[0] = defaultSize / 2u;
	sizes[1] = defaultSize;
	sizes[2] = defaultSize * 2u;

	for (deUint32 i = 0; i < sizes.size(); ++i)
	{
		sizes[i].x() = de::clamp(sizes[i].x(), capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
		sizes[i].y() = de::clamp(sizes[i].y(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

	return sizes;
}

tcu::TestStatus resizeSwapchainTest (Context& context, Type wsiType)
{
	const tcu::UVec2				desiredSize			(256, 256);
	const InstanceHelper			instHelper			(context, wsiType);
	const NativeObjects				native				(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface				(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper				devHelper			(context, instHelper.vki, instHelper.instance, *surface);
	const PlatformProperties&		platformProperties	= getPlatformProperties(wsiType);
	const VkSurfaceCapabilitiesKHR	capabilities		= getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface);
	const DeviceInterface&			vkd					= devHelper.vkd;
	const VkDevice					device				= *devHelper.device;
	SimpleAllocator					allocator			(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
	vector<tcu::UVec2>				sizes				= getSwapchainSizeSequence(capabilities, desiredSize);
	Move<VkSwapchainKHR>			prevSwapchain;

	DE_ASSERT(platformProperties.swapchainExtent != PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE);
	DE_UNREF(platformProperties);

	for (deUint32 sizeNdx = 0; sizeNdx < sizes.size(); ++sizeNdx)
	{
		// \todo [2016-05-30 jesse] This test currently waits for idle and
		// recreates way more than necessary when recreating the swapchain. Make
		// it match expected real app behavior better by smoothly switching from
		// old to new swapchain. Once that is done, it will also be possible to
		// test creating a new swapchain while images from the previous one are
		// still acquired.

		VkSwapchainCreateInfoKHR		swapchainInfo				= getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, sizes[sizeNdx], 2);
		swapchainInfo.oldSwapchain = *prevSwapchain;

		Move<VkSwapchainKHR>			swapchain					(createSwapchainKHR(vkd, device, &swapchainInfo));
		const vector<VkImage>			swapchainImages				= getSwapchainImages(vkd, device, *swapchain);
		const WsiTriangleRenderer		renderer					(vkd,
																	device,
																	allocator,
																	context.getBinaryCollection(),
																	false,
																	swapchainImages,
																	swapchainImages,
																	swapchainInfo.imageFormat,
																	tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));
		const Unique<VkCommandPool>		commandPool					(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));
		const size_t					maxQueuedFrames				= swapchainImages.size()*2;

		// We need to keep hold of fences from vkAcquireNextImageKHR to actually
		// limit number of frames we allow to be queued.
		const vector<FenceSp>			imageReadyFences			(createFences(vkd, device, maxQueuedFrames));

		// We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to pass
		// the semaphore in same time as the fence we use to meter rendering.
		const vector<SemaphoreSp>		imageReadySemaphores		(createSemaphores(vkd, device, maxQueuedFrames+1));

		// For rest we simply need maxQueuedFrames as we will wait for image
		// from frameNdx-maxQueuedFrames to become available to us, guaranteeing that
		// previous uses must have completed.
		const vector<SemaphoreSp>		renderingCompleteSemaphores	(createSemaphores(vkd, device, maxQueuedFrames));
		const vector<CommandBufferSp>	commandBuffers				(allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames));

		try
		{
			const deUint32	numFramesToRender	= 60;

			for (deUint32 frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
			{
				const VkFence		imageReadyFence		= **imageReadyFences[frameNdx%imageReadyFences.size()];
				const VkSemaphore	imageReadySemaphore	= **imageReadySemaphores[frameNdx%imageReadySemaphores.size()];
				deUint32			imageNdx			= ~0u;

				VK_CHECK(vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));
				VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

				{
					const VkResult	acquireResult	= vkd.acquireNextImageKHR(device,
																			  *swapchain,
																			  std::numeric_limits<deUint64>::max(),
																			  imageReadySemaphore,
																			  DE_NULL,
																			  &imageNdx);

					if (acquireResult == VK_SUBOPTIMAL_KHR)
						context.getTestContext().getLog() << TestLog::Message << "Got " << acquireResult << " at frame " << frameNdx << TestLog::EndMessage;
					else
						VK_CHECK(acquireResult);
				}

				TCU_CHECK((size_t)imageNdx < swapchainImages.size());

				{
					const VkSemaphore			renderingCompleteSemaphore	= **renderingCompleteSemaphores[frameNdx%renderingCompleteSemaphores.size()];
					const VkCommandBuffer		commandBuffer				= **commandBuffers[frameNdx%commandBuffers.size()];
					const VkPipelineStageFlags	waitDstStage				= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					const VkSubmitInfo			submitInfo					=
					{
						VK_STRUCTURE_TYPE_SUBMIT_INFO,
						DE_NULL,
						1u,
						&imageReadySemaphore,
						&waitDstStage,
						1u,
						&commandBuffer,
						1u,
						&renderingCompleteSemaphore
					};
					const VkPresentInfoKHR		presentInfo					=
					{
						VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
						DE_NULL,
						1u,
						&renderingCompleteSemaphore,
						1u,
						&*swapchain,
						&imageNdx,
						(VkResult*)DE_NULL
					};

					renderer.recordFrame(commandBuffer, imageNdx, frameNdx);
					VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, imageReadyFence));
					VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
				}
			}

			VK_CHECK(vkd.deviceWaitIdle(device));

			prevSwapchain = swapchain;
		}
		catch (...)
		{
			// Make sure device is idle before destroying resources
			vkd.deviceWaitIdle(device);
			throw;
		}
	}

	return tcu::TestStatus::pass("Resizing tests succeeded");
}

tcu::TestStatus getImagesIncompleteResultTest (Context& context, Type wsiType)
{
	const tcu::UVec2				desiredSize		(256, 256);
	const InstanceHelper			instHelper		(context, wsiType);
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper				devHelper		(context, instHelper.vki, instHelper.instance, *surface);
	const VkSwapchainCreateInfoKHR	swapchainInfo	= getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const Unique<VkSwapchainKHR>	swapchain		(createSwapchainKHR(devHelper.vkd, *devHelper.device, &swapchainInfo));

	vector<VkImage>		swapchainImages	= getSwapchainImages(devHelper.vkd, *devHelper.device, *swapchain);

	ValidateQueryBits::fillBits(swapchainImages.begin(), swapchainImages.end());

	const deUint32		usedCount		= static_cast<deUint32>(swapchainImages.size() / 2);
	deUint32			count			= usedCount;
	const VkResult		result			= devHelper.vkd.getSwapchainImagesKHR(*devHelper.device, *swapchain, &count, &swapchainImages[0]);

	if (count != usedCount || result != VK_INCOMPLETE || !ValidateQueryBits::checkBits(swapchainImages.begin() + count, swapchainImages.end()))
		return tcu::TestStatus::fail("Get swapchain images didn't return VK_INCOMPLETE");
	else
		return tcu::TestStatus::pass("Get swapchain images tests succeeded");
}

tcu::TestStatus getImagesResultsCountTest (Context& context, Type wsiType)
{
	const tcu::UVec2				desiredSize(256, 256);
	const InstanceHelper			instHelper(context, wsiType);
	const NativeObjects				native(context, instHelper.supportedExtensions, wsiType, 1u, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper				devHelper(context, instHelper.vki, instHelper.instance, *surface);
	const VkSwapchainCreateInfoKHR	swapchainInfo = getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const Unique<VkSwapchainKHR>	swapchain(createSwapchainKHR(devHelper.vkd, *devHelper.device, &swapchainInfo));

	deUint32	numImages = 0;

	VK_CHECK(devHelper.vkd.getSwapchainImagesKHR(*devHelper.device, *swapchain, &numImages, DE_NULL));

	if (numImages > 0)
	{
		std::vector<VkImage>	images			(numImages + 1);
		const deUint32			numImagesOrig	= numImages;

		// check if below call properly overwrites formats count
		numImages++;

		VK_CHECK(devHelper.vkd.getSwapchainImagesKHR(*devHelper.device, *swapchain, &numImages, &images[0]));

		if ((size_t)numImages != numImagesOrig)
			TCU_FAIL("Image count changed between calls");
	}
	return tcu::TestStatus::pass("Get swapchain images tests succeeded");
}

tcu::TestStatus destroyNullHandleSwapchainTest (Context& context, Type wsiType)
{
	const InstanceHelper		instHelper	(context, wsiType);
	const NativeObjects			native		(context, instHelper.supportedExtensions, wsiType);
	const Unique<VkSurfaceKHR>	surface		(createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper			devHelper	(context, instHelper.vki, instHelper.instance, *surface);
	const VkSwapchainKHR		nullHandle	= DE_NULL;

	// Default allocator
	devHelper.vkd.destroySwapchainKHR(*devHelper.device, nullHandle, DE_NULL);

	// Custom allocator
	{
		AllocationCallbackRecorder	recordingAllocator	(getSystemAllocator(), 1u);

		devHelper.vkd.destroySwapchainKHR(*devHelper.device, nullHandle, recordingAllocator.getCallbacks());

		if (recordingAllocator.getNumRecords() != 0u)
			return tcu::TestStatus::fail("Implementation allocated/freed the memory");
	}

	return tcu::TestStatus::pass("Destroying a VK_NULL_HANDLE surface has no effect");
}

tcu::TestStatus acquireTooManyTest (Context& context, Type wsiType)
{
	const tcu::UVec2               desiredSize     (256, 256);
	const InstanceHelper           instHelper      (context, wsiType);
	const NativeObjects            native          (context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>     surface         (createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper             devHelper       (context, instHelper.vki, instHelper.instance, *surface);
	const VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const Unique<VkSwapchainKHR>   swapchain       (createSwapchainKHR(devHelper.vkd, *devHelper.device, &swapchainInfo));

	deUint32 numImages;
	VK_CHECK(devHelper.vkd.getSwapchainImagesKHR(*devHelper.device, *swapchain, &numImages, DE_NULL));
	const deUint32 minImageCount = getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface).minImageCount;
	if (numImages < minImageCount) return tcu::TestStatus::fail("Get swapchain images returned less than minImageCount images");
	const deUint32 numAcquirableImages = numImages - minImageCount + 1;

	const auto fences = createFences(devHelper.vkd, *devHelper.device, numAcquirableImages + 1, false);
	deUint32 dummy;
	for (deUint32 i = 0; i < numAcquirableImages; ++i) {
		VK_CHECK_WSI(devHelper.vkd.acquireNextImageKHR(*devHelper.device, *swapchain, std::numeric_limits<deUint64>::max(), (VkSemaphore)0, **fences[i], &dummy));
	}

	const auto result = devHelper.vkd.acquireNextImageKHR(*devHelper.device, *swapchain, 0, (VkSemaphore)0, **fences[numAcquirableImages], &dummy);

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_NOT_READY ){
		return tcu::TestStatus::fail("Implementation failed to respond well acquiring too many images with 0 timeout");
	}

	// cleanup
	const deUint32 numFences = (result == VK_NOT_READY) ? static_cast<deUint32>(fences.size() - 1) : static_cast<deUint32>(fences.size());
	vector<vk::VkFence> fencesRaw(numFences);
	std::transform(fences.begin(), fences.begin() + numFences, fencesRaw.begin(), [](const FenceSp& f) -> vk::VkFence{ return **f; });
	VK_CHECK(devHelper.vkd.waitForFences(*devHelper.device, numFences, fencesRaw.data(), VK_TRUE, std::numeric_limits<deUint64>::max()));

	return tcu::TestStatus::pass("Acquire too many swapchain images test succeeded");
}

tcu::TestStatus acquireTooManyTimeoutTest (Context& context, Type wsiType)
{
	const tcu::UVec2               desiredSize     (256, 256);
	const InstanceHelper           instHelper      (context, wsiType);
	const NativeObjects            native          (context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>     surface         (createSurface(instHelper.vki, instHelper.instance, wsiType, native.getDisplay(), native.getWindow()));
	const DeviceHelper             devHelper       (context, instHelper.vki, instHelper.instance, *surface);
	const VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const Unique<VkSwapchainKHR>   swapchain       (createSwapchainKHR(devHelper.vkd, *devHelper.device, &swapchainInfo));

	deUint32 numImages;
	VK_CHECK(devHelper.vkd.getSwapchainImagesKHR(*devHelper.device, *swapchain, &numImages, DE_NULL));
	const deUint32 minImageCount = getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface).minImageCount;
	if (numImages < minImageCount) return tcu::TestStatus::fail("Get swapchain images returned less than minImageCount images");
	const deUint32 numAcquirableImages = numImages - minImageCount + 1;

	const auto fences = createFences(devHelper.vkd, *devHelper.device, numAcquirableImages + 1, false);
	deUint32 dummy;
	for (deUint32 i = 0; i < numAcquirableImages; ++i) {
		VK_CHECK_WSI(devHelper.vkd.acquireNextImageKHR(*devHelper.device, *swapchain, std::numeric_limits<deUint64>::max(), (VkSemaphore)0, **fences[i], &dummy));
	}

	const deUint64 millisecond = 1000000;
	const deUint64 timeout = 50 * millisecond; // arbitrary realistic non-0 non-infinite timeout
	const auto result = devHelper.vkd.acquireNextImageKHR(*devHelper.device, *swapchain, timeout, (VkSemaphore)0, **fences[numAcquirableImages], &dummy);

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_TIMEOUT ){
		return tcu::TestStatus::fail("Implementation failed to respond well acquiring too many images with timeout");
	}

	// cleanup
	const deUint32 numFences = (result == VK_TIMEOUT) ? static_cast<deUint32>(fences.size() - 1) : static_cast<deUint32>(fences.size());
	vector<vk::VkFence> fencesRaw(numFences);
	std::transform(fences.begin(), fences.begin() + numFences, fencesRaw.begin(), [](const FenceSp& f) -> vk::VkFence{ return **f; });
	VK_CHECK(devHelper.vkd.waitForFences(*devHelper.device, numFences, fencesRaw.data(), VK_TRUE, std::numeric_limits<deUint64>::max()));

	return tcu::TestStatus::pass("Acquire too many swapchain images test succeeded");
}

void getBasicRenderPrograms (SourceCollections& dst, Type)
{
	WsiTriangleRenderer::getPrograms(dst);
}

void getBasicRenderPrograms (SourceCollections& dst, MultiSwapchainParams)
{
	WsiTriangleRenderer::getPrograms(dst);
}

void populateRenderGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	addFunctionCaseWithPrograms(testGroup, "basic", "Basic Rendering Test", getBasicRenderPrograms, basicRenderTest<AcquireNextImageWrapper>, wsiType);
	addFunctionCaseWithPrograms(testGroup, "basic2", "Basic Rendering Test using AcquireNextImage2", getBasicRenderPrograms, basicRenderTest<AcquireNextImage2Wrapper>, wsiType);
	addFunctionCaseWithPrograms(testGroup, "device_group", "Basic Rendering Test using device_group", getBasicRenderPrograms, deviceGroupRenderTest, wsiType);
	addFunctionCaseWithPrograms(testGroup, "device_group2", "Rendering Test using device_group and VkImageSwapchainCreateInfo", getBasicRenderPrograms, deviceGroupRenderTest2, wsiType);

	const MultiSwapchainParams kTwoSwapchains	{ wsiType, 2u	};
	const MultiSwapchainParams kTenSwapchains	{ wsiType, 10u	};

	addFunctionCaseWithPrograms(testGroup, "2swapchains", "2 Swapchains Rendering Test", getBasicRenderPrograms, multiSwapchainRenderTest<AcquireNextImageWrapper>, kTwoSwapchains);
	addFunctionCaseWithPrograms(testGroup, "2swapchains2", "2 Swapchains Rendering Test using AcquireNextImage2", getBasicRenderPrograms, multiSwapchainRenderTest<AcquireNextImage2Wrapper>, kTwoSwapchains);
	addFunctionCaseWithPrograms(testGroup, "10swapchains", "10 Swapchains Rendering Test", getBasicRenderPrograms, multiSwapchainRenderTest<AcquireNextImageWrapper>, kTenSwapchains);
	addFunctionCaseWithPrograms(testGroup, "10swapchains2", "10 Swapchains Rendering Test using AcquireNextImage2", getBasicRenderPrograms, multiSwapchainRenderTest<AcquireNextImage2Wrapper>, kTenSwapchains);
}

void populateGetImagesGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	addFunctionCase(testGroup, "incomplete", "Test VK_INCOMPLETE return code", getImagesIncompleteResultTest, wsiType);
	addFunctionCase(testGroup, "count",	"Test proper count of images", getImagesResultsCountTest, wsiType);
}

void populateModifyGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	const PlatformProperties&	platformProperties	= getPlatformProperties(wsiType);

	if (platformProperties.swapchainExtent != PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE)
	{
		addFunctionCaseWithPrograms(testGroup, "resize", "Resize Swapchain Test", getBasicRenderPrograms, resizeSwapchainTest, wsiType);
	}

	// \todo [2016-05-30 jesse] Add tests for modifying preTransform, compositeAlpha, presentMode
}

void populateDestroyGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	addFunctionCase(testGroup, "null_handle", "Destroying a VK_NULL_HANDLE swapchain", destroyNullHandleSwapchainTest, wsiType);
}

void populateAcquireGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	addFunctionCase(testGroup, "too_many", "Test acquiring too many images with 0 timeout", acquireTooManyTest, wsiType);
	addFunctionCase(testGroup, "too_many_timeout", "Test acquiring too many images with timeout", acquireTooManyTimeoutTest, wsiType);
}

} // anonymous

void createSwapchainTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addTestGroup(testGroup, "create",			"Create VkSwapchain with various parameters",					populateSwapchainGroup,					GroupParameters(wsiType, createSwapchainTest));
	addTestGroup(testGroup, "simulate_oom",		"Simulate OOM using callbacks during swapchain construction",	populateSwapchainGroup,					GroupParameters(wsiType, createSwapchainSimulateOOMTest));
	addTestGroup(testGroup, "render",			"Rendering Tests",												populateRenderGroup,					wsiType);
	addTestGroup(testGroup, "modify",			"Modify VkSwapchain",											populateModifyGroup,					wsiType);
	addTestGroup(testGroup, "destroy",			"Destroy VkSwapchain",											populateDestroyGroup,					wsiType);
	addTestGroup(testGroup, "get_images",		"Get swapchain images",											populateGetImagesGroup,					wsiType);
	addTestGroup(testGroup, "acquire",			"Ancquire next swapchain image",								populateAcquireGroup,					wsiType);
	addTestGroup(testGroup, "private_data",		"Create VkSwapchain and use VK_EXT_private_data",				populateSwapchainPrivateDataGroup,		GroupParameters(wsiType, createSwapchainPrivateDataTest));
}

} // wsi
} // vkt
