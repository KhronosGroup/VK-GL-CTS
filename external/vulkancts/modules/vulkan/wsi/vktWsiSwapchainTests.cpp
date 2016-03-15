/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be
 * included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
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

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

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

Move<VkInstance> createInstanceWithWsi (const PlatformInterface&	vkp,
										const Extensions&			supportedExtensions,
										Type						wsiType)
{
	vector<string>	extensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));

	checkAllSupported(supportedExtensions, extensions);

	return createDefaultInstance(vkp, vector<string>(), extensions);
}

VkPhysicalDeviceFeatures getDeviceFeaturesForWsi (void)
{
	VkPhysicalDeviceFeatures features;
	deMemset(&features, 0, sizeof(features));
	return features;
}

Move<VkDevice> createDeviceWithWsi (const InstanceInterface&	vki,
									VkPhysicalDevice			physicalDevice,
									const Extensions&			supportedExtensions,
									const deUint32				queueFamilyIndex)
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
		0u,									// enabledExtensionCount
		DE_NULL,							// ppEnabledExtensionNames
		&features
	};

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(extensions); ++ndx)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(extensions[ndx])))
			TCU_THROW(NotSupportedError, (string(extensions[ndx]) + " is not supported").c_str());
	}

	return createDevice(vki, physicalDevice, &deviceParams, (const VkAllocationCallbacks*)DE_NULL);
}

deUint32 getNumQueueFamilyIndices (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
{
	deUint32	numFamilies		= 0;

	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numFamilies, DE_NULL);

	return numFamilies;
}

vector<deUint32> getSupportedQueueFamilyIndices (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	const deUint32		numTotalFamilyIndices	= getNumQueueFamilyIndices(vki, physicalDevice);
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

	InstanceHelper (Context& context, Type wsiType)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context.getPlatformInterface(),
													   supportedExtensions,
													   wsiType))
		, vki					(context.getPlatformInterface(), *instance)
	{}
};

VkQueue getDeviceQueue (const DeviceInterface& vkd, VkDevice device, deUint32 queueFamilyIndex, deUint32 queueIndex)
{
	VkQueue queue = (VkQueue)0;
	vkd.getDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
	return queue;
}

struct DeviceHelper
{
	const VkPhysicalDevice	physicalDevice;
	const deUint32			queueFamilyIndex;
	const Unique<VkDevice>	device;
	const DeviceDriver		vkd;
	const VkQueue			queue;

	DeviceHelper (Context& context, const InstanceInterface& vki, VkInstance instance, VkSurfaceKHR surface)
		: physicalDevice	(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, surface))
		, device			(createDeviceWithWsi(vki,
												 physicalDevice,
												 enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
												 queueFamilyIndex))
		, vkd				(vki, *device)
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
		if (isExtensionSupported(supportedExtensions, RequiredExtension(getExtensionName(wsiType))))
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
		VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(VkSwapchainKHR)0					// oldSwapchain
	};

	switch (dimension)
	{
		case TEST_DIMENSION_MIN_IMAGE_COUNT:
		{
			const deUint32	maxImageCountToTest	= de::clamp(16u, capabilities.minImageCount, capabilities.maxImageCount);

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
	const InstanceHelper					instHelper	(context, params.wsiType);
	const NativeObjects						native		(context, instHelper.supportedExtensions, params.wsiType);
	const Unique<VkSurfaceKHR>				surface		(createSurface(instHelper.vki, *instHelper.instance, params.wsiType, *native.display, *native.window));
	const DeviceHelper						devHelper	(context, instHelper.vki, *instHelper.instance, *surface);
	const vector<VkSwapchainCreateInfoKHR>	cases		(generateSwapchainParameterCases(params.wsiType, params.dimension, instHelper.vki, devHelper.physicalDevice, *surface));

	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		VkSwapchainCreateInfoKHR	curParams	= cases[caseNdx];

		curParams.surface				= *surface;
		curParams.queueFamilyIndexCount	= 1u;
		curParams.pQueueFamilyIndices	= &devHelper.queueFamilyIndex;

		context.getTestContext().getLog()
			<< TestLog::Message << "Sub-case " << (caseNdx+1) << " / " << cases.size() << ": " << curParams << TestLog::EndMessage;

		{
			const Unique<VkSwapchainKHR>	swapchain	(createSwapchainKHR(devHelper.vkd, *devHelper.device, &curParams));
		}
	}

	return tcu::TestStatus::pass("Creating swapchain succeeded");
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

} // anonymous

void createSwapchainTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addTestGroup(testGroup, "create", "Create VkSwapchain with various parameters", populateSwapchainGroup, GroupParameters(wsiType, createSwapchainTest));
}

} // wsi
} // vkt
