/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
#include "vkCmdUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

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

CustomInstance createInstanceWithWsi (Context&						context,
									  const Extensions&				supportedExtensions,
									  Type							wsiType,
									  const VkAllocationCallbacks*	pAllocator	= DE_NULL)
{
	vector<string>	extensions;

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

	return createCustomInstanceWithExtensions(context, extensions, pAllocator);
}

VkPhysicalDeviceFeatures getDeviceFeaturesForWsi (void)
{
	VkPhysicalDeviceFeatures features;
	deMemset(&features, 0, sizeof(features));
	return features;
}

Move<VkDevice> createDeviceWithWsi (const vk::PlatformInterface&	vkp,
									vk::VkInstance					instance,
									const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									const Extensions&				supportedExtensions,
									const deUint32					queueFamilyIndex,
									const VkAllocationCallbacks*	pAllocator,
									bool							validationEnabled)
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
	vector<const char*>		extensions;

	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_swapchain")))
		TCU_THROW(NotSupportedError, (string(extensions[0]) + " is not supported").c_str());
	extensions.push_back("VK_KHR_swapchain");

	if (isExtensionSupported(supportedExtensions, RequiredExtension("VK_EXT_hdr_metadata")))
		extensions.push_back("VK_EXT_hdr_metadata");

	VkDeviceCreateInfo		deviceParams	=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		DE_NULL,
		(VkDeviceCreateFlags)0,
		DE_LENGTH_OF_ARRAY(queueInfos),
		&queueInfos[0],
		0u,									// enabledLayerCount
		DE_NULL,							// ppEnabledLayerNames
		(deUint32)extensions.size(),
		extensions.empty() ? DE_NULL : &extensions[0],
		&features
	};

	return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
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
				  VkSurfaceKHR					surface,
				  const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: physicalDevice	(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex	(chooseQueueFamilyIndex(vki, physicalDevice, surface))
		, device			(createDeviceWithWsi(context.getPlatformInterface(),
												 instance,
												 vki,
												 physicalDevice,
												 enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
												 queueFamilyIndex,
												 pAllocator,
												 context.getTestContext().getCommandLine().isValidationEnabled()))
		, vkd				(context.getPlatformInterface(), instance, *device)
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

VkSwapchainCreateInfoKHR getBasicSwapchainParameters (Type						wsiType,
													  const InstanceInterface&	vki,
													  VkPhysicalDevice			physicalDevice,
													  VkSurfaceKHR				surface,
													  VkSurfaceFormatKHR		surfaceFormat,
													  const tcu::UVec2&			desiredSize,
													  deUint32					desiredImageCount)
{
	const VkSurfaceCapabilitiesKHR		capabilities		= getPhysicalDeviceSurfaceCapabilities(vki,
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
		surfaceFormat.format,
		surfaceFormat.colorSpace,
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

tcu::TestStatus basicExtensionTest (Context& context, Type wsiType)
{
	const tcu::UVec2				desiredSize		(256, 256);
	const InstanceHelper			instHelper		(context, wsiType);
	const NativeObjects				native			(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>		surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper				devHelper		(context, instHelper.vki, instHelper.instance, *surface);

	if (!de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(), "VK_EXT_swapchain_colorspace"))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_swapchain_colorspace not supported");

	const vector<VkSurfaceFormatKHR>	formats			= getPhysicalDeviceSurfaceFormats(instHelper.vki,
																						  devHelper.physicalDevice,
																						  *surface);

	bool found = false;
	for (vector<VkSurfaceFormatKHR>::const_iterator curFmt = formats.begin(); curFmt != formats.end(); ++curFmt)
	{
		if (curFmt->colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			found = true;
			break;
		}
	}
	if (!found)
	{
		TCU_THROW(NotSupportedError, "VK_EXT_swapchain_colorspace supported, but no non-SRGB_NONLINEAR_KHR surface formats found.");
	}
	return tcu::TestStatus::pass("Extension tests succeeded");
}

tcu::TestStatus surfaceFormatRenderTest (Context& context,
										 Type wsiType,
										 const InstanceHelper& instHelper,
										 const DeviceHelper& devHelper,
										 VkSurfaceKHR surface,
										 VkSurfaceFormatKHR curFmt,
										 deBool checkHdr = false)
{
	const tcu::UVec2					desiredSize		(256, 256);
	const DeviceInterface&				vkd				= devHelper.vkd;
	const VkDevice						device			= *devHelper.device;
	SimpleAllocator						allocator		(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));

	const VkSwapchainCreateInfoKHR		swapchainInfo			= getBasicSwapchainParameters(wsiType, instHelper.vki, devHelper.physicalDevice, surface, curFmt, desiredSize, 2);
	const Unique<VkSwapchainKHR>		swapchain				(createSwapchainKHR(vkd, device, &swapchainInfo));
	const vector<VkImage>				swapchainImages			= getSwapchainImages(vkd, device, *swapchain);
	const vector<VkExtensionProperties>	deviceExtensions		(enumerateDeviceExtensionProperties(instHelper.vki, devHelper.physicalDevice, DE_NULL));

	if (checkHdr && !isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_hdr_metadata")))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_hdr_metadata not supported");

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

			if (frameNdx >= maxQueuedFrames)
				VK_CHECK(vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));

			VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

			{
				const VkResult	acquireResult	= vkd.acquireNextImageKHR(device,
																		  *swapchain,
																		  std::numeric_limits<deUint64>::max(),
																		  imageReadySemaphore,
																		  (vk::VkFence)0,
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

				if (checkHdr) {
					const VkHdrMetadataEXT hdrData = {
							VK_STRUCTURE_TYPE_HDR_METADATA_EXT,
							DE_NULL,
							makeXYColorEXT(0.680f, 0.320f),
							makeXYColorEXT(0.265f, 0.690f),
							makeXYColorEXT(0.150f, 0.060f),
							makeXYColorEXT(0.3127f, 0.3290f),
							1000.0,
							0.0,
							1000.0,
							70.0
					};
					vector<VkSwapchainKHR> swapchainArray;

					swapchainArray.push_back(*swapchain);
					vkd.setHdrMetadataEXT(device, (deUint32)swapchainArray.size(), swapchainArray.data(), &hdrData);
				}

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

	return tcu::TestStatus::pass("Rendering test succeeded");
}

tcu::TestStatus surfaceFormatRenderTests (Context& context, Type wsiType)
{
	const tcu::UVec2					desiredSize		(256, 256);
	const InstanceHelper				instHelper		(context, wsiType);
	const NativeObjects					native			(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>			surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper					devHelper		(context, instHelper.vki, instHelper.instance, *surface);

	if (!de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(), "VK_EXT_swapchain_colorspace"))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_swapchain_colorspace not supported");

	const vector<VkSurfaceFormatKHR>	formats			= getPhysicalDeviceSurfaceFormats(instHelper.vki,
																							 devHelper.physicalDevice,
																							 *surface);
	for (vector<VkSurfaceFormatKHR>::const_iterator curFmt = formats.begin(); curFmt != formats.end(); ++curFmt)
	{
		surfaceFormatRenderTest(context, wsiType, instHelper, devHelper, *surface, *curFmt);
	}
	return tcu::TestStatus::pass("Rendering tests succeeded");
}

tcu::TestStatus surfaceFormatRenderWithHdrTests (Context& context, Type wsiType)
{
	const tcu::UVec2					desiredSize		(256, 256);
	const InstanceHelper				instHelper		(context, wsiType);
	const NativeObjects					native			(context, instHelper.supportedExtensions, wsiType, tcu::just(desiredSize));
	const Unique<VkSurfaceKHR>			surface			(createSurface(instHelper.vki, instHelper.instance, wsiType, *native.display, *native.window));
	const DeviceHelper					devHelper		(context, instHelper.vki, instHelper.instance, *surface);

	if (!de::contains(context.getInstanceExtensions().begin(), context.getInstanceExtensions().end(), "VK_EXT_swapchain_colorspace"))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_swapchain_colorspace not supported");

	const vector<VkSurfaceFormatKHR>	formats			= getPhysicalDeviceSurfaceFormats(instHelper.vki,
																						  devHelper.physicalDevice,
																						  *surface);
	for (vector<VkSurfaceFormatKHR>::const_iterator curFmt = formats.begin(); curFmt != formats.end(); ++curFmt)
	{
		surfaceFormatRenderTest(context, wsiType, instHelper, devHelper, *surface, *curFmt, true);
	}
	return tcu::TestStatus::pass("Rendering tests succeeded");
}

void getBasicRenderPrograms (SourceCollections& dst, Type)
{
	WsiTriangleRenderer::getPrograms(dst);
}

} // anonymous

void createColorSpaceTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addFunctionCase(testGroup, "extensions", "Verify Colorspace Extensions", basicExtensionTest, wsiType);
	addFunctionCaseWithPrograms(testGroup, "basic", "Basic Rendering Tests", getBasicRenderPrograms, surfaceFormatRenderTests, wsiType);
	addFunctionCaseWithPrograms(testGroup, "hdr", "Basic Rendering Tests with HDR", getBasicRenderPrograms, surfaceFormatRenderWithHdrTests, wsiType);
}

} // wsi
} // vkt
