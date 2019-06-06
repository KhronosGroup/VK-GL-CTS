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

Move<VkInstance> createInstanceWithWsi (const PlatformInterface&		vkp,
										deUint32						version,
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

	return vk::createDefaultInstance(vkp, version, vector<string>(), extensions, pAllocator);
}

VkPhysicalDeviceFeatures getDeviceFeaturesForWsi (void)
{
	VkPhysicalDeviceFeatures features;
	deMemset(&features, 0, sizeof(features));
	return features;
}

Move<VkDevice> createDeviceWithWsi (const PlatformInterface&		vkp,
									VkInstance						instance,
									const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									const Extensions&				supportedExtensions,
									const deUint32					queueFamilyIndex,
									const VkAllocationCallbacks*	pAllocator = DE_NULL)
{
	const float						queuePriorities[]	= { 1.0f };
	const VkDeviceQueueCreateInfo	queueInfos[]		=
	{
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			DE_NULL,
			(VkDeviceQueueCreateFlags)0,
			queueFamilyIndex,
			DE_LENGTH_OF_ARRAY(queuePriorities),
			&queuePriorities[0]
		}
	};
	const VkPhysicalDeviceFeatures	features		= getDeviceFeaturesForWsi();
	const char* const				extensions[]	= { "VK_KHR_swapchain" };
	const VkDeviceCreateInfo		deviceParams	=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		DE_NULL,
		(VkDeviceCreateFlags)0,
		DE_LENGTH_OF_ARRAY(queueInfos),
		&queueInfos[0],
		0u,									// enabledLayerCount
		DE_NULL,							// ppEnabledLayerNames
		DE_LENGTH_OF_ARRAY(extensions),		// enabledExtensionCount
		DE_ARRAY_BEGIN(extensions),			// ppEnabledExtensionNames
		&features
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(extensions); ++ndx)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(extensions[ndx])))
			TCU_THROW(NotSupportedError, (string(extensions[ndx]) + " is not supported").c_str());
	}

	return createDevice(vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

vector<deUint32> getSupportedQueueFamilyIndices (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	deUint32 numTotalFamilyIndices;
	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, DE_NULL);

	vector<VkQueueFamilyProperties> queueFamilyProperties(numTotalFamilyIndices);
	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, &queueFamilyProperties[0]);

	vector<deUint32>	supportedFamilyIndices;
	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numTotalFamilyIndices; ++queueFamilyNdx)
	{
		if (getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, surface) != VK_FALSE)
			supportedFamilyIndices.push_back(queueFamilyNdx);
	}

	return supportedFamilyIndices;
}

deUint32 chooseQueueFamilyIndex (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	const vector<deUint32>	supportedFamilyIndices	= getSupportedQueueFamilyIndices(vki, physicalDevice, surface);

	if (supportedFamilyIndices.empty())
		TCU_THROW(NotSupportedError, "Device doesn't support presentation");

	return supportedFamilyIndices[0];
}

struct InstanceHelper
{
	const vector<VkExtensionProperties>	supportedExtensions;
	const Unique<VkInstance>			instance;
	const InstanceDriver				vki;

	InstanceHelper (Context& context, Type wsiType, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context.getPlatformInterface(),
													   context.getUsedApiVersion(),
													   supportedExtensions,
													   wsiType,
													   vector<string>(),
													   pAllocator))
		, vki					(context.getPlatformInterface(), *instance)
	{}

	InstanceHelper (Context& context, Type wsiType, const vector<string>& extensions, const VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context.getPlatformInterface(),
													   context.getUsedApiVersion(),
													   supportedExtensions,
													   wsiType,
													   extensions,
													   pAllocator))
		, vki					(context.getPlatformInterface(), *instance)
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
				  VkSurfaceKHR					surface,
				  const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: physicalDevice	(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, surface))
		, device			(createDeviceWithWsi(context.getPlatformInterface(),
												 context.getInstance(),
												 vki,
												 physicalDevice,
												 enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
												 queueFamilyIndex,
												 pAllocator))
		, vkd				(context.getPlatformInterface(), context.getInstance(), *device)
		, queue				(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
	{
	}
};

MovePtr<Display> createDisplay (const vk::Platform&	platform,
								const Extensions&	supportedExtensions,
								Type				wsiType)
{
	try
	{
		return MovePtr<Display>(platform.createWsiDisplay(wsiType));
	}
	catch (const tcu::NotSupportedError& e)
	{
		if (isExtensionSupported(supportedExtensions, RequiredExtension(getExtensionName(wsiType))) &&
		    platform.hasDisplay(wsiType))
		{
			// If VK_KHR_{platform}_surface was supported, vk::Platform implementation
			// must support creating native display & window for that WSI type.
			throw tcu::TestError(e.getMessage());
		}
		else
			throw;
	}
}

MovePtr<Window> createWindow (const Display& display, const Maybe<UVec2>& initialSize)
{
	try
	{
		return MovePtr<Window>(display.createWindow(initialSize));
	}
	catch (const tcu::NotSupportedError& e)
	{
		// See createDisplay - assuming that wsi::Display was supported platform port
		// should also support creating a window.
		throw tcu::TestError(e.getMessage());
	}
}

struct NativeObjects
{
	const UniquePtr<Display>	display;
	const UniquePtr<Window>		window;

	NativeObjects (Context&				context,
				   const Extensions&	supportedExtensions,
				   Type					wsiType,
				   const Maybe<UVec2>&	initialWindowSize = tcu::nothing<UVec2>())
		: display	(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
		, window	(createWindow(*display, initialWindowSize))
	{}
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

vector<VkSwapchainCreateInfoKHR> generateSwapchainParameterCases (Type								wsiType,
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
			cases.push_back(baseParameters);
			cases.back().imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

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

	return generateSwapchainParameterCases(wsiType, dimension, capabilities, formats, presentModes);
}

tcu::TestStatus createSwapchainTest (Context& context, TestParameters params)
{
	tcu::TestLog&							log			= context.getTestContext().getLog();
	const InstanceHelper					instHelper	(context, params.wsiType);
	const NativeObjects						native		(context, instHelper.supportedExtensions, params.wsiType);
	const Unique<VkSurfaceKHR>				surface		(createSurface(instHelper.vki, *instHelper.instance, params.wsiType, *native.display, *native.window));
	const DeviceHelper						devHelper	(context, instHelper.vki, *instHelper.instance, *surface);
	const vector<VkSwapchainCreateInfoKHR>	cases		(generateSwapchainParameterCases(params.wsiType, params.dimension, instHelper.vki, devHelper.physicalDevice, *surface));

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		std::ostringstream subcase;
		subcase << "Sub-case " << (caseNdx+1) << " / " << cases.size() << ": ";

		VkSwapchainCreateInfoKHR	curParams	= cases[caseNdx];

		curParams.surface				= *surface;
		curParams.queueFamilyIndexCount	= 1u;
		curParams.pQueueFamilyIndices	= &devHelper.queueFamilyIndex;

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
			}
			log << TestLog::Message << subcase.str()
				<< "Creating swapchain succeeeded" << TestLog::EndMessage;
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
																			*instHelper.instance,
																			params.wsiType,
																			*native.display,
																			*native.window,
																			failingAllocator.getCallbacks()));
		const DeviceHelper						devHelper	(context, instHelper.vki, *instHelper.instance, *surface, failingAllocator.getCallbacks());
		const vector<VkSwapchainCreateInfoKHR>	allCases	(generateSwapchainParameterCases(params.wsiType, params.dimension, instHelper.vki, devHelper.physicalDevice, *surface));

		if (maxCases < allCases.size())
			log << TestLog::Message << "Note: Will only test first " << maxCases << " cases out of total of " << allCases.size() << " parameter combinations" << TestLog::EndMessage;

		for (size_t caseNdx = 0; caseNdx < de::min(maxCases, allCases.size()); ++caseNdx)
		{
			log << TestLog::Message << "Testing parameter case " << caseNdx << ": " << allCases[caseNdx] << TestLog::EndMessage;

			for (deUint32 numPassingAllocs = 0; numPassingAllocs <= maxAllocs; ++numPassingAllocs)
			{
				bool	gotOOM	= false;

				failingAllocator.reset(DeterministicFailAllocator::MODE_COUNT_AND_FAIL, numPassingAllocs);

				log << TestLog::Message << "Testing with " << numPassingAllocs << " first allocations succeeding" << TestLog::EndMessage;

				try
				{
					VkSwapchainCreateInfoKHR	curParams	= allCases[caseNdx];

					curParams.surface				= *surface;
					curParams.queueFamilyIndexCount	= 1u;
					curParams.pQueueFamilyIndices	= &devHelper.queueFamilyIndex;

					{
						const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &curParams, failingAllocator.getCallbacks()));
					}
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
					results.addResult(QP_TEST_RESULT_QUALITY_WARNING, "Creating swapchain did not succeed, callback limit exceeded");
			}

			context.getTestContext().touchWatchdog();
		}
	}

	if (!validateAndLog(log, allocationRecorder, 0u))
		results.fail("Detected invalid system allocation callback");

	return tcu::TestStatus(results.getResult(), results.getMessage());
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

typedef de::SharedPtr<Unique<VkImageView> >		ImageViewSp;
typedef de::SharedPtr<Unique<VkFramebuffer> >	FramebufferSp;

class TriangleRenderer
{
public:
									TriangleRenderer	(const DeviceInterface&		vkd,
														 const VkDevice				device,
														 Allocator&					allocator,
														 const BinaryCollection&	binaryRegistry,
														 const vector<VkImage>		swapchainImages,
														 const VkFormat				framebufferFormat,
														 const UVec2&				renderSize);
									~TriangleRenderer	(void);

	void							recordFrame			(VkCommandBuffer			cmdBuffer,
														 deUint32					imageNdx,
														 deUint32					frameNdx) const;

	void							recordDeviceGroupFrame (VkCommandBuffer			cmdBuffer,
															deUint32				imageNdx,
															deUint32				firstDeviceID,
															deUint32				secondDeviceID,
															deUint32				devicesCount,
															deUint32				frameNdx) const;

	static void						getPrograms			(SourceCollections& dst);

private:
	static Move<VkRenderPass>		createRenderPass	(const DeviceInterface&		vkd,
														 const VkDevice				device,
														 const VkFormat				colorAttachmentFormat);
	static Move<VkPipelineLayout>	createPipelineLayout(const DeviceInterface&		vkd,
														 VkDevice					device);
	static Move<VkPipeline>			createPipeline		(const DeviceInterface&		vkd,
														 const VkDevice				device,
														 const VkRenderPass			renderPass,
														 const VkPipelineLayout		pipelineLayout,
														 const BinaryCollection&	binaryCollection,
														 const UVec2&				renderSize);

	static Move<VkImageView>		createAttachmentView(const DeviceInterface&		vkd,
														 const VkDevice				device,
														 const VkImage				image,
														 const VkFormat				format);
	static Move<VkFramebuffer>		createFramebuffer	(const DeviceInterface&		vkd,
														 const VkDevice				device,
														 const VkRenderPass			renderPass,
														 const VkImageView			colorAttachment,
														 const UVec2&				renderSize);

	static Move<VkBuffer>			createBuffer		(const DeviceInterface&		vkd,
														 VkDevice					device,
														 VkDeviceSize				size,
														 VkBufferUsageFlags			usage);

	const DeviceInterface&			m_vkd;

	const vector<VkImage>			m_swapchainImages;
	const tcu::UVec2				m_renderSize;

	const Unique<VkRenderPass>		m_renderPass;
	const Unique<VkPipelineLayout>	m_pipelineLayout;
	const Unique<VkPipeline>		m_pipeline;

	const Unique<VkBuffer>			m_vertexBuffer;
	const UniquePtr<Allocation>		m_vertexBufferMemory;

	vector<ImageViewSp>				m_attachmentViews;
	vector<FramebufferSp>			m_framebuffers;
};

Move<VkRenderPass> TriangleRenderer::createRenderPass (const DeviceInterface&	vkd,
													   const VkDevice			device,
													   const VkFormat			colorAttachmentFormat)
{
	const VkAttachmentDescription	colorAttDesc		=
	{
		(VkAttachmentDescriptionFlags)0,
		colorAttachmentFormat,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};
	const VkAttachmentReference		colorAttRef			=
	{
		0u,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	const VkSubpassDescription		subpassDesc			=
	{
		(VkSubpassDescriptionFlags)0u,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0u,							// inputAttachmentCount
		DE_NULL,					// pInputAttachments
		1u,							// colorAttachmentCount
		&colorAttRef,				// pColorAttachments
		DE_NULL,					// pResolveAttachments
		DE_NULL,					// depthStencilAttachment
		0u,							// preserveAttachmentCount
		DE_NULL,					// pPreserveAttachments
	};
	const VkSubpassDependency		dependencies[]		=
	{
		{
			VK_SUBPASS_EXTERNAL,	// srcSubpass
			0u,						// dstSubpass
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			VK_DEPENDENCY_BY_REGION_BIT
		},
		{
			0u,						// srcSubpass
			VK_SUBPASS_EXTERNAL,	// dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			VK_ACCESS_MEMORY_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT
		},
	};
	const VkRenderPassCreateInfo	renderPassParams	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		DE_NULL,
		(VkRenderPassCreateFlags)0,
		1u,
		&colorAttDesc,
		1u,
		&subpassDesc,
		DE_LENGTH_OF_ARRAY(dependencies),
		dependencies,
	};

	return vk::createRenderPass(vkd, device, &renderPassParams);
}

Move<VkPipelineLayout> TriangleRenderer::createPipelineLayout (const DeviceInterface&	vkd,
															   const VkDevice			device)
{
	const VkPushConstantRange						pushConstantRange		=
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		0u,											// offset
		(deUint32)sizeof(deUint32),					// size
	};
	const VkPipelineLayoutCreateInfo				pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,											// setLayoutCount
		DE_NULL,									// pSetLayouts
		1u,
		&pushConstantRange,
	};

	return vk::createPipelineLayout(vkd, device, &pipelineLayoutParams);
}

Move<VkPipeline> TriangleRenderer::createPipeline (const DeviceInterface&	vkd,
												   const VkDevice			device,
												   const VkRenderPass		renderPass,
												   const VkPipelineLayout	pipelineLayout,
												   const BinaryCollection&	binaryCollection,
												   const UVec2&				renderSize)
{
	// \note VkShaderModules are fully consumed by vkCreateGraphicsPipelines()
	//		 and can be deleted immediately following that call.
	const Unique<VkShaderModule>					vertShaderModule		(createShaderModule(vkd, device, binaryCollection.get("tri-vert"), 0));
	const Unique<VkShaderModule>					fragShaderModule		(createShaderModule(vkd, device, binaryCollection.get("tri-frag"), 0));
	const std::vector<VkViewport>					viewports				(1, makeViewport(renderSize));
	const std::vector<VkRect2D>						scissors				(1, makeRect2D(renderSize));

	return vk::makeGraphicsPipeline(vkd,				// const DeviceInterface&            vk
									device,				// const VkDevice                    device
									pipelineLayout,		// const VkPipelineLayout            pipelineLayout
									*vertShaderModule,	// const VkShaderModule              vertexShaderModule
									DE_NULL,			// const VkShaderModule              tessellationControlShaderModule
									DE_NULL,			// const VkShaderModule              tessellationEvalShaderModule
									DE_NULL,			// const VkShaderModule              geometryShaderModule
									*fragShaderModule,	// const VkShaderModule              fragmentShaderModule
									renderPass,			// const VkRenderPass                renderPass
									viewports,			// const std::vector<VkViewport>&    viewports
									scissors);			// const std::vector<VkRect2D>&      scissors
}

Move<VkImageView> TriangleRenderer::createAttachmentView (const DeviceInterface&	vkd,
														  const VkDevice			device,
														  const VkImage				image,
														  const VkFormat			format)
{
	const VkImageViewCreateInfo		viewParams	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,
		(VkImageViewCreateFlags)0,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		format,
		vk::makeComponentMappingRGBA(),
		{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0u,						// baseMipLevel
			1u,						// levelCount
			0u,						// baseArrayLayer
			1u,						// layerCount
		},
	};

	return vk::createImageView(vkd, device, &viewParams);
}

Move<VkFramebuffer> TriangleRenderer::createFramebuffer	(const DeviceInterface&		vkd,
														 const VkDevice				device,
														 const VkRenderPass			renderPass,
														 const VkImageView			colorAttachment,
														 const UVec2&				renderSize)
{
	const VkFramebufferCreateInfo	framebufferParams	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		(VkFramebufferCreateFlags)0,
		renderPass,
		1u,
		&colorAttachment,
		renderSize.x(),
		renderSize.y(),
		1u,							// layers
	};

	return vk::createFramebuffer(vkd, device, &framebufferParams);
}

Move<VkBuffer> TriangleRenderer::createBuffer (const DeviceInterface&	vkd,
											   VkDevice					device,
											   VkDeviceSize				size,
											   VkBufferUsageFlags		usage)
{
	const VkBufferCreateInfo	bufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(VkBufferCreateFlags)0,
		size,
		usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		DE_NULL
	};

	return vk::createBuffer(vkd, device, &bufferParams);
}

TriangleRenderer::TriangleRenderer (const DeviceInterface&	vkd,
									const VkDevice			device,
									Allocator&				allocator,
									const BinaryCollection&	binaryRegistry,
									const vector<VkImage>	swapchainImages,
									const VkFormat			framebufferFormat,
									const UVec2&			renderSize)
	: m_vkd					(vkd)
	, m_swapchainImages		(swapchainImages)
	, m_renderSize			(renderSize)
	, m_renderPass			(createRenderPass(vkd, device, framebufferFormat))
	, m_pipelineLayout		(createPipelineLayout(vkd, device))
	, m_pipeline			(createPipeline(vkd, device, *m_renderPass, *m_pipelineLayout, binaryRegistry, renderSize))
	, m_vertexBuffer		(createBuffer(vkd, device, (VkDeviceSize)(sizeof(float)*4*3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
	, m_vertexBufferMemory	(allocator.allocate(getBufferMemoryRequirements(vkd, device, *m_vertexBuffer),
							 MemoryRequirement::HostVisible))
{
	m_attachmentViews.resize(swapchainImages.size());
	m_framebuffers.resize(swapchainImages.size());

	for (size_t imageNdx = 0; imageNdx < swapchainImages.size(); ++imageNdx)
	{
		m_attachmentViews[imageNdx]	= ImageViewSp(new Unique<VkImageView>(createAttachmentView(vkd, device, swapchainImages[imageNdx], framebufferFormat)));
		m_framebuffers[imageNdx]	= FramebufferSp(new Unique<VkFramebuffer>(createFramebuffer(vkd, device, *m_renderPass, **m_attachmentViews[imageNdx], renderSize)));
	}

	VK_CHECK(vkd.bindBufferMemory(device, *m_vertexBuffer, m_vertexBufferMemory->getMemory(), m_vertexBufferMemory->getOffset()));

	{
		const VkMappedMemoryRange	memRange	=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
			DE_NULL,
			m_vertexBufferMemory->getMemory(),
			m_vertexBufferMemory->getOffset(),
			VK_WHOLE_SIZE
		};
		const tcu::Vec4				vertices[]	=
		{
			tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
			tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
			tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
		};
		DE_STATIC_ASSERT(sizeof(vertices) == sizeof(float)*4*3);

		deMemcpy(m_vertexBufferMemory->getHostPtr(), &vertices[0], sizeof(vertices));
		VK_CHECK(vkd.flushMappedMemoryRanges(device, 1u, &memRange));
	}
}

TriangleRenderer::~TriangleRenderer (void)
{
}

void TriangleRenderer::recordFrame (VkCommandBuffer	cmdBuffer,
									deUint32		imageNdx,
									deUint32		frameNdx) const
{
	const VkFramebuffer	curFramebuffer	= **m_framebuffers[imageNdx];

	beginCommandBuffer(m_vkd, cmdBuffer, 0u);

	beginRenderPass(m_vkd, cmdBuffer, *m_renderPass, curFramebuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), tcu::Vec4(0.125f, 0.25f, 0.75f, 1.0f));

	m_vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	{
		const VkDeviceSize bindingOffset = 0;
		m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &bindingOffset);
	}

	m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, (deUint32)sizeof(deUint32), &frameNdx);
	m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	endRenderPass(m_vkd, cmdBuffer);

	endCommandBuffer(m_vkd, cmdBuffer);
}

void TriangleRenderer::recordDeviceGroupFrame (VkCommandBuffer	cmdBuffer,
											   deUint32			firstDeviceID,
											   deUint32			secondDeviceID,
											   deUint32			devicesCount,
											   deUint32			imageNdx,
											   deUint32			frameNdx) const
{
	const VkFramebuffer	curFramebuffer	= **m_framebuffers[imageNdx];

	beginCommandBuffer(m_vkd, cmdBuffer, 0u);

	// begin renderpass
	{
		const VkClearValue clearValue = makeClearValueColorF32(0.125f, 0.25f, 0.75f, 1.0f);

		VkRect2D zeroRect = { { 0, 0, },{ 0, 0, } };
		vector<VkRect2D> renderAreas;
		for (deUint32 i = 0; i < devicesCount; i++)
			renderAreas.push_back(zeroRect);

		// Render completely if there is only 1 device
		if (devicesCount == 1u)
		{
			renderAreas[0].extent.width = (deInt32)m_renderSize.x();
			renderAreas[0].extent.height = (deInt32)m_renderSize.y();
		}
		else
		{
			// Split into 2 vertical halves
			renderAreas[firstDeviceID].extent.width		= (deInt32)m_renderSize.x() / 2;
			renderAreas[firstDeviceID].extent.height	= (deInt32)m_renderSize.y();
			renderAreas[secondDeviceID]					= renderAreas[firstDeviceID];
			renderAreas[secondDeviceID].offset.x		= (deInt32)m_renderSize.x() / 2;
		}

		const VkDeviceGroupRenderPassBeginInfo deviceGroupRPBeginInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,
			DE_NULL,
			(deUint32)((1 << devicesCount) - 1),
			devicesCount,
			&renderAreas[0]
		};

		const VkRenderPassBeginInfo passBeginParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,						// sType
			&deviceGroupRPBeginInfo,										// pNext
			*m_renderPass,													// renderPass
			curFramebuffer,													// framebuffer
			{
				{ 0, 0 },
				{ m_renderSize.x(), m_renderSize.y() }
			},																// renderArea
			1u,																// clearValueCount
			&clearValue,													// pClearValues
		};
		m_vkd.cmdBeginRenderPass(cmdBuffer, &passBeginParams, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	{
		const VkDeviceSize bindingOffset = 0;
		m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer.get(), &bindingOffset);
	}

	m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, (deUint32)sizeof(deUint32), &frameNdx);
	m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	endRenderPass(m_vkd, cmdBuffer);

	endCommandBuffer(m_vkd, cmdBuffer);
}

void TriangleRenderer::getPrograms (SourceCollections& dst)
{
	dst.glslSources.add("tri-vert") << glu::VertexSource(
		"#version 310 es\n"
		"layout(location = 0) in highp vec4 a_position;\n"
		"layout(push_constant) uniform FrameData\n"
		"{\n"
		"    highp uint frameNdx;\n"
		"} frameData;\n"
		"void main (void)\n"
		"{\n"
		"    highp float angle = float(frameData.frameNdx) / 100.0;\n"
		"    highp float c     = cos(angle);\n"
		"    highp float s     = sin(angle);\n"
		"    highp mat4  t     = mat4( c, -s,  0,  0,\n"
		"                              s,  c,  0,  0,\n"
		"                              0,  0,  1,  0,\n"
		"                              0,  0,  0,  1);\n"
		"    gl_Position = t * a_position;\n"
		"}\n");
	dst.glslSources.add("tri-frag") << glu::FragmentSource(
		"#version 310 es\n"
		"layout(location = 0) out lowp vec4 o_color;\n"
		"void main (void) { o_color = vec4(1.0, 0.0, 1.0, 1.0); }\n");
}

typedef de::SharedPtr<Unique<VkCommandBuffer> >	CommandBufferSp;
typedef de::SharedPtr<Unique<VkFence> >			FenceSp;
typedef de::SharedPtr<Unique<VkSemaphore> >		SemaphoreSp;

vector<FenceSp> createFences (const DeviceInterface&	vkd,
							  const VkDevice			device,
							  size_t					numFences)
{
	vector<FenceSp> fences(numFences);

	for (size_t ndx = 0; ndx < numFences; ++ndx)
		fences[ndx] = FenceSp(new Unique<VkFence>(createFence(vkd, device)));

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

	bool featureAvailable(const deUint32 deviceVersion, const Extensions& supportedExtensions)
	{
		DE_UNREF(deviceVersion);
		DE_UNREF(supportedExtensions);
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

	bool featureAvailable(const deUint32 deviceVersion, const Extensions& supportedExtensions)
	{
		return isDeviceExtensionSupported(deviceVersion, supportedExtensions, RequiredExtension("VK_KHR_device_group"));
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
	const NativeObjects				native						(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface						(createSurface(instHelper.vki, *instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper				devHelper					(context, instHelper.vki, *instHelper.instance, *surface);
	const DeviceInterface&			vkd							= devHelper.vkd;
	const VkDevice					device						= *devHelper.device;
	SimpleAllocator					allocator					(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));
	const VkSwapchainCreateInfoKHR	swapchainInfo				= getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, *surface, desiredSize, 2);
	const Unique<VkSwapchainKHR>	swapchain					(createSwapchainKHR(vkd, device, &swapchainInfo));
	const vector<VkImage>			swapchainImages				= getSwapchainImages(vkd, device, *swapchain);

	AcquireWrapperType acquireImageWrapper(vkd, device, 1u, *swapchain, std::numeric_limits<deUint64>::max());
	if (!acquireImageWrapper.featureAvailable(context.getUsedApiVersion(), instHelper.supportedExtensions))
		TCU_THROW(NotSupportedError, "Required extension is not supported");

	const TriangleRenderer			renderer					(vkd,
																 device,
																 allocator,
																 context.getBinaryCollection(),
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

			if (frameNdx >= maxQueuedFrames)
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

tcu::TestStatus deviceGroupRenderTest (Context& context, Type wsiType)
{
	const InstanceHelper		instHelper			(context, wsiType, vector<string>(1, string("VK_KHR_device_group_creation")));
	const tcu::CommandLine&		cmdLine				= context.getTestContext().getCommandLine();
	VkPhysicalDevice			physicalDevice		= chooseDevice(instHelper.vki, *instHelper.instance, cmdLine);
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
	const NativeObjects								native						(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>						surface						(createSurface(instHelper.vki, *instHelper.instance, wsiType, *native.display, *native.window));

	const deUint32									devGroupIdx					= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32									deviceIdx					= context.getTestContext().getCommandLine().getVKDeviceId() - 1u;
	const vector<VkPhysicalDeviceGroupProperties>	deviceGroupProps			= enumeratePhysicalDeviceGroups(instHelper.vki, *instHelper.instance);
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
	const VkDeviceCreateInfo deviceCreateInfo =
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
	Move<VkDevice>					groupDevice					= createDevice(context.getPlatformInterface(), *instHelper.instance, instHelper.vki, physicalDevicesInGroup[deviceIdx], &deviceCreateInfo);
	const DeviceDriver				vkd							(context.getPlatformInterface(), *instHelper.instance, *groupDevice);
	VkQueue							queue						(getDeviceQueue(vkd, *groupDevice, queueFamilyIndex, 0));
	SimpleAllocator					allocator					(vkd, *groupDevice, getPhysicalDeviceMemoryProperties(instHelper.vki, physicalDevicesInGroup[deviceIdx]));

	// create swapchain for device group
	struct VkDeviceGroupSwapchainCreateInfoKHR deviceGroupSwapchainInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR
	};
	VkSwapchainCreateInfoKHR swapchainInfo = getBasicSwapchainParameters(wsiType,
																		 instHelper.vki,
																		 physicalDevicesInGroup[deviceIdx],
																		 *surface,
																		 desiredSize,
																		 2);
	swapchainInfo.pNext = &deviceGroupSwapchainInfo;

	const Unique<VkSwapchainKHR>	swapchain					(createSwapchainKHR(vkd, *groupDevice, &swapchainInfo));
	const vector<VkImage>			swapchainImages				= getSwapchainImages(vkd, *groupDevice, *swapchain);

	const TriangleRenderer			renderer					(vkd,
																 *groupDevice,
																 allocator,
																 context.getBinaryCollection(),
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

			if (frameNdx >= maxQueuedFrames)
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
	const NativeObjects				native				(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface				(createSurface(instHelper.vki, *instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper				devHelper			(context, instHelper.vki, *instHelper.instance, *surface);
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
		const TriangleRenderer			renderer					(vkd,
																	device,
																	allocator,
																	context.getBinaryCollection(),
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

				if (frameNdx >= maxQueuedFrames)
					VK_CHECK(vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));

				VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

				{
					const VkResult	acquireResult	= vkd.acquireNextImageKHR(device,
																			  *swapchain,
																			  std::numeric_limits<deUint64>::max(),
																			  imageReadySemaphore,
																			  imageReadyFence,
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
					VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, (VkFence)0));
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
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, *instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper				devHelper		(context, instHelper.vki, *instHelper.instance, *surface);
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
	const NativeObjects				native(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface(createSurface(instHelper.vki, *instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper				devHelper(context, instHelper.vki, *instHelper.instance, *surface);
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
	const Unique<VkSurfaceKHR>	surface		(createSurface(instHelper.vki, *instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper			devHelper	(context, instHelper.vki, *instHelper.instance, *surface);
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

void getBasicRenderPrograms (SourceCollections& dst, Type)
{
	TriangleRenderer::getPrograms(dst);
}

void populateRenderGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	addFunctionCaseWithPrograms(testGroup, "basic", "Basic Rendering Test", getBasicRenderPrograms, basicRenderTest<AcquireNextImageWrapper>, wsiType);
	addFunctionCaseWithPrograms(testGroup, "basic2", "Basic Rendering Test using AcquireNextImage2", getBasicRenderPrograms, basicRenderTest<AcquireNextImage2Wrapper>, wsiType);
	addFunctionCaseWithPrograms(testGroup, "device_group", "Basic Rendering Test using device_group", getBasicRenderPrograms, deviceGroupRenderTest, wsiType);
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

} // anonymous

void createSwapchainTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addTestGroup(testGroup, "create",			"Create VkSwapchain with various parameters",					populateSwapchainGroup,		GroupParameters(wsiType, createSwapchainTest));
	addTestGroup(testGroup, "simulate_oom",		"Simulate OOM using callbacks during swapchain construction",	populateSwapchainGroup,		GroupParameters(wsiType, createSwapchainSimulateOOMTest));
	addTestGroup(testGroup, "render",			"Rendering Tests",												populateRenderGroup,		wsiType);
	addTestGroup(testGroup, "modify",			"Modify VkSwapchain",											populateModifyGroup,		wsiType);
	addTestGroup(testGroup, "destroy",			"Destroy VkSwapchain",											populateDestroyGroup,		wsiType);
	addTestGroup(testGroup, "get_images",		"Get swapchain images",											populateGetImagesGroup,		wsiType);
}

} // wsi
} // vkt
