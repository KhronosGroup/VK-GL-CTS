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
 * \brief VkSurface Tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiSurfaceTests.hpp"

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

using std::string;
using std::vector;

enum
{
	SURFACE_EXTENT_DETERMINED_BY_SWAPCHAIN_MAGIC	= 0xffffffff
};

void checkInstanceGlobalExtensionSupport (const PlatformInterface& vkp, const vector<string>& extensionNames)
{
	const vector<VkExtensionProperties>	supportedExtensions	= enumerateInstanceExtensionProperties(vkp, DE_NULL);

	for (vector<string>::const_iterator requiredExtName = extensionNames.begin();
		 requiredExtName != extensionNames.end();
		 ++requiredExtName)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(*requiredExtName)))
			TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
	}
}

Move<VkInstance> createInstanceWithWsi (const PlatformInterface& vkp, Type wsiType)
{
	vector<string>	extensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));

	checkInstanceGlobalExtensionSupport(vkp, extensions);

	return createDefaultInstance(vkp, vector<string>(), extensions);
}

tcu::TestStatus createSurfaceTest (Context& context, Type wsiType)
{
	const vk::Platform&			vkPlatform	= context.getTestContext().getPlatform().getVulkanPlatform();
	const Unique<VkInstance>	instance	(createInstanceWithWsi(context.getPlatformInterface(), wsiType));
	const InstanceDriver		vki			(context.getPlatformInterface(), *instance);

	try
	{
		const de::UniquePtr<Display>	nativeDisplay	(vkPlatform.createWsiDisplay(wsiType));
		const de::UniquePtr<Window>		nativeWindow	(nativeDisplay->createWindow());
		const Unique<VkSurfaceKHR>		surface			(createSurface(vki, *instance, wsiType, *nativeDisplay, *nativeWindow));
	}
	catch (const tcu::NotSupportedError&)
	{
		return tcu::TestStatus::fail("Platform support for WSI not implemented");
	}

	return tcu::TestStatus::pass("Creating surface succeeded");
}

deUint32 getNumQueueFamilies (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
{
	deUint32	numFamilies		= 0;

	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numFamilies, DE_NULL);

	return numFamilies;
}

tcu::TestStatus querySurfaceSupportTest (Context& context, Type wsiType)
{
	tcu::TestLog&				log						= context.getTestContext().getLog();
	const vk::Platform&			vkPlatform				= context.getTestContext().getPlatform().getVulkanPlatform();
	const Unique<VkInstance>	instance				(createInstanceWithWsi(context.getPlatformInterface(), wsiType));
	const InstanceDriver		vki						(context.getPlatformInterface(), *instance);

	// On Android surface must be supported by all devices and queue families
	const bool					expectSupportedOnAll	= wsiType == TYPE_ANDROID;

	try
	{
		const de::UniquePtr<Display>	nativeDisplay	(vkPlatform.createWsiDisplay(wsiType));
		const de::UniquePtr<Window>		nativeWindow	(nativeDisplay->createWindow());
		const Unique<VkSurfaceKHR>		surface			(createSurface(vki, *instance, wsiType, *nativeDisplay, *nativeWindow));

		{
			const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(vki, *instance);
			tcu::ResultCollector			results			(log);

			for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
			{
				const VkPhysicalDevice		physicalDevice		= physicalDevices[deviceNdx];
				const deUint32				numQueueFamilies	= getNumQueueFamilies(vki, physicalDevice);

				for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numQueueFamilies; ++queueFamilyNdx)
				{
					const VkBool32	isSupported		= getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, *surface);

					log << TestLog::Message << "Device " << deviceNdx << ", queue family " << queueFamilyNdx << ": "
											<< (isSupported == VK_FALSE ? "NOT " : "") << "supported"
						<< TestLog::EndMessage;

					if (expectSupportedOnAll && !isSupported)
						results.fail("Surface must be supported by all devices and queue families");
				}
			}

			return tcu::TestStatus(results.getResult(), results.getMessage());
		}
	}
	catch (const tcu::NotSupportedError&)
	{
		return tcu::TestStatus::fail("Platform support for WSI not implemented");
	}
}

bool isSupportedByAnyQueue (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	const deUint32	numQueueFamilies	= getNumQueueFamilies(vki, physicalDevice);

	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numQueueFamilies; ++queueFamilyNdx)
	{
		if (getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, surface) != VK_FALSE)
			return true;
	}

	return false;
}

void validateSurfaceCapabilities (tcu::ResultCollector& results, const VkSurfaceCapabilitiesKHR& capabilities)
{
	results.check(capabilities.minImageCount > 0,
				  "minImageCount must be larger than 0");

	results.check(capabilities.minImageExtent.width > 0 &&
				  capabilities.minImageExtent.height > 0,
				  "minImageExtent dimensions must be larger than 0");

	results.check(capabilities.maxImageExtent.width > 0 &&
				  capabilities.maxImageExtent.height > 0,
				  "maxImageExtent dimensions must be larger than 0");

	results.check(capabilities.minImageExtent.width <= capabilities.maxImageExtent.width &&
				  capabilities.minImageExtent.height <= capabilities.maxImageExtent.height,
				  "maxImageExtent must be larger or equal to minImageExtent");

	if (capabilities.currentExtent.width != SURFACE_EXTENT_DETERMINED_BY_SWAPCHAIN_MAGIC ||
		capabilities.currentExtent.height != SURFACE_EXTENT_DETERMINED_BY_SWAPCHAIN_MAGIC)
	{
		results.check(capabilities.currentExtent.width > 0 &&
					  capabilities.currentExtent.height > 0,
					  "currentExtent dimensions must be larger than 0");

		results.check(de::inRange(capabilities.currentExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width) &&
					  de::inRange(capabilities.currentExtent.height, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
					  "currentExtent is not in supported extent limits");
	}

	results.check(capabilities.maxImageArrayLayers > 0,
				  "maxImageArrayLayers must be larger than 0");

	results.check((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0,
				  "VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT must be set in supportedUsageFlags");

	results.check(capabilities.supportedTransforms != 0,
				  "At least one transform must be supported");

	results.check(dePop32(capabilities.currentTransform) != 0,
				  "Invalid currentTransform");

	results.check((capabilities.supportedTransforms & capabilities.currentTransform) != 0,
				  "currentTransform is not supported by surface");

	results.check(capabilities.supportedCompositeAlpha != 0,
				  "At least one alpha mode must be supported");
}

tcu::TestStatus querySurfaceCapabilitiesTest (Context& context, Type wsiType)
{
	const vk::Platform&			vkPlatform	= context.getTestContext().getPlatform().getVulkanPlatform();
	const Unique<VkInstance>	instance	(createInstanceWithWsi(context.getPlatformInterface(), wsiType));
	const InstanceDriver		vki			(context.getPlatformInterface(), *instance);
	tcu::TestLog&				log			= context.getTestContext().getLog();

	try
	{
		const de::UniquePtr<Display>	nativeDisplay	(vkPlatform.createWsiDisplay(wsiType));
		const de::UniquePtr<Window>		nativeWindow	(nativeDisplay->createWindow());
		const Unique<VkSurfaceKHR>		surface			(createSurface(vki, *instance, wsiType, *nativeDisplay, *nativeWindow));

		{
			const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(vki, *instance);
			tcu::ResultCollector			results			(log);

			for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
			{
				if (isSupportedByAnyQueue(vki, physicalDevices[deviceNdx], *surface))
				{
					const VkSurfaceCapabilitiesKHR	capabilities	= getPhysicalDeviceSurfaceCapabilities(vki, physicalDevices[deviceNdx], *surface);

					log << TestLog::Message << "Device " << deviceNdx << ": " << capabilities << TestLog::EndMessage;

					validateSurfaceCapabilities(results, capabilities);
				}
				// else skip query as surface is not supported by the device
			}

			return tcu::TestStatus(results.getResult(), results.getMessage());
		}
	}
	catch (const tcu::NotSupportedError&)
	{
		return tcu::TestStatus::fail("Platform support for WSI not implemented");
	}
}

tcu::TestStatus createSurfaceInitialSizeTest (Context& context, Type wsiType)
{
	const vk::Platform&				vkPlatform		= context.getTestContext().getPlatform().getVulkanPlatform();
	const Unique<VkInstance>		instance		(createInstanceWithWsi(context.getPlatformInterface(), wsiType));
	const InstanceDriver			vki				(context.getPlatformInterface(), *instance);
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(vki, *instance);
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);
	const UVec2						sizes[]			=
	{
		UVec2(64, 64),
		UVec2(124, 119),
		UVec2(256, 512)
	};

	DE_ASSERT(getPlatformProperties(wsiType).features & PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE);

	try
	{
		const de::UniquePtr<Display>	nativeDisplay	(vkPlatform.createWsiDisplay(wsiType));

		for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
		{
			const UVec2						testSize		= sizes[sizeNdx];
			const de::UniquePtr<Window>		nativeWindow	(nativeDisplay->createWindow(tcu::just(testSize)));
			const Unique<VkSurfaceKHR>		surface			(createSurface(vki, *instance, wsiType, *nativeDisplay, *nativeWindow));

			for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
			{
				if (isSupportedByAnyQueue(vki, physicalDevices[deviceNdx], *surface))
				{
					const VkSurfaceCapabilitiesKHR	capabilities	= getPhysicalDeviceSurfaceCapabilities(vki, physicalDevices[deviceNdx], *surface);

					// \note Assumes that surface size is NOT set by swapchain if initial window size is honored by platform
					results.check(capabilities.currentExtent.width == testSize.x() &&
									capabilities.currentExtent.height == testSize.y(),
									"currentExtent " + de::toString(capabilities.currentExtent) + " doesn't match requested size " + de::toString(testSize));
				}
			}
		}
	}
	catch (const tcu::NotSupportedError&)
	{
		return tcu::TestStatus::fail("Platform support for WSI not implemented");
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

tcu::TestStatus resizeSurfaceTest (Context& context, Type wsiType)
{
	const vk::Platform&				vkPlatform		= context.getTestContext().getPlatform().getVulkanPlatform();
	const Unique<VkInstance>		instance		(createInstanceWithWsi(context.getPlatformInterface(), wsiType));
	const InstanceDriver			vki				(context.getPlatformInterface(), *instance);
	const vector<VkPhysicalDevice>	physicalDevices	= enumeratePhysicalDevices(vki, *instance);
	tcu::TestLog&					log				= context.getTestContext().getLog();
	tcu::ResultCollector			results			(log);
	const UVec2						sizes[]			=
	{
		UVec2(64, 64),
		UVec2(124, 119),
		UVec2(256, 512)
	};

	DE_ASSERT(getPlatformProperties(wsiType).features & PlatformProperties::FEATURE_RESIZE_WINDOW);

	try
	{
		const de::UniquePtr<Display>	nativeDisplay	(vkPlatform.createWsiDisplay(wsiType));
		const de::UniquePtr<Window>		nativeWindow	(nativeDisplay->createWindow());
		const Unique<VkSurfaceKHR>		surface			(createSurface(vki, *instance, wsiType, *nativeDisplay, *nativeWindow));

		for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
		{
			const UVec2		testSize	= sizes[sizeNdx];

			try
			{
				nativeWindow->resize(testSize);
			}
			catch (const tcu::Exception& e)
			{
				// Make sure all exception types result in a test failure
				results.fail(e.getMessage());
			}

			for (size_t deviceNdx = 0; deviceNdx < physicalDevices.size(); ++deviceNdx)
			{
				if (isSupportedByAnyQueue(vki, physicalDevices[deviceNdx], *surface))
				{
					const VkSurfaceCapabilitiesKHR	capabilities	= getPhysicalDeviceSurfaceCapabilities(vki, physicalDevices[deviceNdx], *surface);

					// \note Assumes that surface size is NOT set by swapchain if initial window size is honored by platform
					results.check(capabilities.currentExtent.width == testSize.x() &&
									capabilities.currentExtent.height == testSize.y(),
									"currentExtent " + de::toString(capabilities.currentExtent) + " doesn't match requested size " + de::toString(testSize));
				}
			}
		}
	}
	catch (const tcu::NotSupportedError&)
	{
		return tcu::TestStatus::fail("Platform support for WSI not implemented");
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

} // anonymous

void createSurfaceTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	const PlatformProperties&	platformProperties	= getPlatformProperties(wsiType);

	addFunctionCase(testGroup, "create",				"Create surface",				createSurfaceTest,				wsiType);
	addFunctionCase(testGroup, "query_support",			"Query surface support",		querySurfaceSupportTest,		wsiType);
	addFunctionCase(testGroup, "query_capabilities",	"Query surface capabilities",	querySurfaceCapabilitiesTest,	wsiType);

	if ((platformProperties.features & PlatformProperties::FEATURE_INITIAL_WINDOW_SIZE) != 0)
		addFunctionCase(testGroup, "initial_size",	"Create surface with initial window size set",	createSurfaceInitialSizeTest,	wsiType);

	if ((platformProperties.features & PlatformProperties::FEATURE_RESIZE_WINDOW) != 0)
		addFunctionCase(testGroup, "resize",		"Resize window and surface",					resizeSurfaceTest,				wsiType);
}

} // wsi
} // vkt
