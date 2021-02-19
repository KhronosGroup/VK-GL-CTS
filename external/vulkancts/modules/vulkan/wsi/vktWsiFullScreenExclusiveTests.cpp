/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief VK_EXT_full_screen_exclusive extension Tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiFullScreenExclusiveTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"

#if ( DE_OS == DE_OS_WIN32 )
	#define NOMINMAX
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

namespace vkt
{
namespace wsi
{

namespace
{

using namespace vk;
using namespace vk::wsi;

typedef std::vector<VkExtensionProperties> Extensions;

struct TestParams
{
	vk::wsi::Type				wsiType;
	VkFullScreenExclusiveEXT	fseType;
};

void checkAllSupported (const Extensions& supportedExtensions,
						const std::vector<std::string>& requiredExtensions)
{
	for (std::vector<std::string>::const_iterator requiredExtName = requiredExtensions.begin();
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
	std::vector<std::string>	extensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));

	if (isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_get_surface_capabilities2")))
		extensions.push_back("VK_KHR_get_surface_capabilities2");

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
	std::vector<const char*>		extensions;

	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_swapchain")))
		TCU_THROW(NotSupportedError, (std::string(extensions[0]) + " is not supported").c_str());
	extensions.push_back("VK_KHR_swapchain");

	if (isExtensionSupported(supportedExtensions, RequiredExtension("VK_EXT_full_screen_exclusive")))
	{
		extensions.push_back("VK_EXT_full_screen_exclusive");
	}

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
	const std::vector<VkExtensionProperties>	supportedExtensions;
	const CustomInstance				instance;
	const InstanceDriver&				vki;

	InstanceHelper (Context& context,
					Type wsiType,
					const VkAllocationCallbacks* pAllocator = DE_NULL)
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

de::MovePtr<Display> createDisplay (const vk::Platform&	platform,
									const Extensions&	supportedExtensions,
									Type				wsiType)
{
	try
	{
		return de::MovePtr<Display>(platform.createWsiDisplay(wsiType));
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

de::MovePtr<Window> createWindow (const Display& display,
								  const tcu::Maybe<tcu::UVec2>& initialSize)
{
	try
	{
		return de::MovePtr<Window>(display.createWindow(initialSize));
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
	const de::UniquePtr<Display>	display;
	tcu::UVec2						windowSize;
	const de::UniquePtr<Window>		window;

	NativeObjects (Context&				context,
				   const Extensions&	supportedExtensions,
				   Type					wsiType)
		: display		(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
		, windowSize	(getFullScreenSize(wsiType, *display.get(), tcu::UVec2(256U, 256U)))
		, window		(createWindow(*display, windowSize))
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

std::vector<FenceSp> createFences (const DeviceInterface&	vkd,
								   const VkDevice			device,
								   size_t					numFences)
{
	std::vector<FenceSp> fences(numFences);

	for (size_t ndx = 0; ndx < numFences; ++ndx)
		fences[ndx] = FenceSp(new Unique<VkFence>(createFence(vkd, device)));

	return fences;
}

std::vector<SemaphoreSp> createSemaphores (const DeviceInterface&	vkd,
										   const VkDevice			device,
										   size_t					numSemaphores)
{
	std::vector<SemaphoreSp> semaphores(numSemaphores);

	for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
		semaphores[ndx] = SemaphoreSp(new Unique<VkSemaphore>(createSemaphore(vkd, device)));

	return semaphores;
}

std::vector<CommandBufferSp> allocateCommandBuffers (const DeviceInterface&		vkd,
													 const VkDevice				device,
													 const VkCommandPool		commandPool,
													 const VkCommandBufferLevel	level,
													 const size_t				numCommandBuffers)
{
	std::vector<CommandBufferSp>				buffers		(numCommandBuffers);

	for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
		buffers[ndx] = CommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

	return buffers;
}

tcu::TestStatus fullScreenExclusiveTest(Context& context,
	TestParams testParams)
{
	if (!de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_EXT_full_screen_exclusive"))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_full_screen_exclusive not supported");

	const InstanceHelper						instHelper(context, testParams.wsiType);
	const NativeObjects							native(context, instHelper.supportedExtensions, testParams.wsiType);
	const Unique<VkSurfaceKHR>					surface(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.window));
	const DeviceHelper							devHelper(context, instHelper.vki, instHelper.instance, *surface);
	const std::vector<VkExtensionProperties>	deviceExtensions(enumerateDeviceExtensionProperties(instHelper.vki, devHelper.physicalDevice, DE_NULL));
	if (!isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_full_screen_exclusive")))
		TCU_THROW(NotSupportedError, "Extension VK_EXT_full_screen_exclusive not supported");

	native.window->setVisible(true);

	if (testParams.wsiType == TYPE_WIN32)
	{
		native.window->setForeground();
	}

	// add information about full screen exclusive to VkSwapchainCreateInfoKHR
	VkSurfaceFullScreenExclusiveInfoEXT			fseInfo =
	{
		VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT,			// VkStructureType             sType;
		DE_NULL,															// void*                       pNext;
		testParams.fseType													// VkFullScreenExclusiveEXT    fullScreenExclusive;
	};

	// for Win32 - create structure containing HMONITOR value
#if ( DE_OS == DE_OS_WIN32 )
	VkSurfaceFullScreenExclusiveWin32InfoEXT	fseWin32Info				= {
		VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT,		// VkStructureType    sType;
		DE_NULL,															// const void*        pNext;
		pt::Win32MonitorHandle(0)											// HMONITOR           hmonitor;
	};
	if (testParams.wsiType == TYPE_WIN32)
	{
		Win32WindowInterface*					windowInterface				= dynamic_cast<Win32WindowInterface*>(native.window.get());
		fseWin32Info.hmonitor												= (pt::Win32MonitorHandle)MonitorFromWindow((HWND)windowInterface->getNative().internal, MONITOR_DEFAULTTONEAREST);
	}
#endif

	// check surface capabilities
	VkSurfaceCapabilitiesFullScreenExclusiveEXT	surfaceCapabilitiesFSE		= {
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT,	// VkStructureType    sType;
		DE_NULL,															// void*              pNext;
		DE_FALSE															// VkBool32           fullScreenExclusiveSupported;
	};
	VkSurfaceCapabilities2KHR					surfaceCapabilities2		= {
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,						// VkStructureType             sType;
		&surfaceCapabilitiesFSE,											// void*                       pNext;
		VkSurfaceCapabilitiesKHR {}											// VkSurfaceCapabilitiesKHR    surfaceCapabilities;
	};
	VkPhysicalDeviceSurfaceInfo2KHR				surfaceInfo					= {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,				// VkStructureType    sType;
		DE_NULL,															// const void*        pNext;
		*surface															// VkSurfaceKHR       surface;
	};

	surfaceInfo.pNext = &fseInfo;

#if ( DE_OS == DE_OS_WIN32 )
	if (testParams.wsiType == TYPE_WIN32)
	{
		fseInfo.pNext = &fseWin32Info;
	}
#endif

	instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &surfaceCapabilities2);
	if (surfaceCapabilitiesFSE.fullScreenExclusiveSupported == DE_FALSE)
		TCU_THROW(NotSupportedError, "VkSurfaceCapabilitiesFullScreenExclusiveEXT::fullScreenExclusiveSupported is set to false");

	const DeviceInterface&						vkd							= devHelper.vkd;
	const VkDevice								device						= *devHelper.device;
	SimpleAllocator								allocator					(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));

	std::vector<VkSurfaceFormatKHR>				surfaceFormats				= vk::wsi::getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surface);
	if(surfaceFormats.empty())
		return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

	VkSwapchainCreateInfoKHR					swapchainInfo				= getBasicSwapchainParameters(testParams.wsiType, instHelper.vki, devHelper.physicalDevice, *surface, surfaceFormats[0], native.windowSize, 2);

	swapchainInfo.pNext = &fseInfo;

#if ( DE_OS == DE_OS_WIN32 )
	if (testParams.wsiType == TYPE_WIN32)
	{
		fseInfo.pNext = &fseWin32Info;
	}
#endif

	Move<VkSwapchainKHR>						swapchain;
	{
		VkSwapchainKHR object = 0;
		VkResult result = vkd.createSwapchainKHR(device, &swapchainInfo, DE_NULL, &object);
		if (result == VK_ERROR_INITIALIZATION_FAILED && testParams.fseType == VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT)
		{
			// In some cases, swapchain creation may fail if exclusive full-screen mode is requested for application control,
			// but for some implementation-specific reason exclusive full-screen access is unavailable for the particular combination
			// of parameters provided. If this occurs, VK_ERROR_INITIALIZATION_FAILED will be returned.
			return  tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Failed to create swapchain with exclusive full-screen mode for application control.");
		}
		else
		{
			VK_CHECK(result);
		}

		swapchain = Move<VkSwapchainKHR>(check<VkSwapchainKHR>(object), Deleter<VkSwapchainKHR>(vkd, device, DE_NULL));
	}
	const std::vector<VkImage>					swapchainImages				= getSwapchainImages(vkd, device, *swapchain);

	const WsiTriangleRenderer					renderer					(vkd,
																			 device,
																			 allocator,
																			 context.getBinaryCollection(),
																			 true,
																			 swapchainImages,
																			 swapchainImages,
																			 swapchainInfo.imageFormat,
																			 tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

	const Unique<VkCommandPool>					commandPool					(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

	const size_t								maxQueuedFrames				= swapchainImages.size()*2;

	// We need to keep hold of fences from vkAcquireNextImageKHR to actually
	// limit number of frames we allow to be queued.
	const std::vector<FenceSp>					imageReadyFences			(createFences(vkd, device, maxQueuedFrames));

	// We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to pass
	// the semaphore in same time as the fence we use to meter rendering.
	const std::vector<SemaphoreSp>				imageReadySemaphores		(createSemaphores(vkd, device, maxQueuedFrames+1));

	// For rest we simply need maxQueuedFrames as we will wait for image
	// from frameNdx-maxQueuedFrames to become available to us, guaranteeing that
	// previous uses must have completed.
	const std::vector<SemaphoreSp>				renderingCompleteSemaphores	(createSemaphores(vkd, device, maxQueuedFrames));
	const std::vector<CommandBufferSp>			commandBuffers				(allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, maxQueuedFrames));

	bool										fullScreenAcquired			= (testParams.fseType != VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT);

	bool										fullScreenLost				= false;

	try
	{
		const deUint32	numFramesToRender					= 60;

		for (deUint32 frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
		{
			const VkFence		imageReadyFence		= **imageReadyFences[frameNdx%imageReadyFences.size()];
			const VkSemaphore	imageReadySemaphore	= **imageReadySemaphores[frameNdx%imageReadySemaphores.size()];
			deUint32			imageNdx			= ~0u;

			if (!fullScreenAcquired)
			{
				const VkResult acquireResult = vkd.acquireFullScreenExclusiveModeEXT(device, *swapchain);

				switch (acquireResult)
				{
					case VK_SUCCESS:
					{
						fullScreenAcquired = true;
						break;
					}
					case VK_ERROR_INITIALIZATION_FAILED:
					{
						break;
					}
					case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
					{
						context.getTestContext().getLog() << tcu::TestLog::Message << "Got VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT at vkAcquireFullScreenExclusiveModeEXT. Frame " << frameNdx << tcu::TestLog::EndMessage;
						break;
					}
					default:
					{
						VK_CHECK(acquireResult);
						break;
					}
				}
			}

			if (frameNdx >= maxQueuedFrames)
				VK_CHECK(vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));

			VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

			VkResult	acquireResult;

			{
				acquireResult	= vkd.acquireNextImageKHR(device,
														  *swapchain,
														  std::numeric_limits<deUint64>::max(),
														  imageReadySemaphore,
														  (vk::VkFence)0,
														  &imageNdx);
				if (acquireResult == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
				{
					context.getTestContext().getLog() << tcu::TestLog::Message << "Got VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT at vkAcquireNextImageKHR" << tcu::TestLog::EndMessage;

					fullScreenLost = true;
				}
				VK_CHECK_WSI(acquireResult);
			}

			if (acquireResult != VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
			{
				TCU_CHECK((size_t)imageNdx < swapchainImages.size());

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
				const VkResult presentResult = vkd.queuePresentKHR(devHelper.queue, &presentInfo);
				if (presentResult == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
				{
					context.getTestContext().getLog() << tcu::TestLog::Message << "Got VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT at vkQueuePresentKHR" << tcu::TestLog::EndMessage;

					fullScreenLost = true;
				}
				VK_CHECK_WSI(presentResult);
			}
			else
			{
				// image was not acquired, just roll the synchronization
				VK_CHECK(vkd.queueSubmit(devHelper.queue, 0u, DE_NULL, imageReadyFence));
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

	if (fullScreenAcquired && testParams.fseType == VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT)
	{
		const VkResult releaseResult = vkd.releaseFullScreenExclusiveModeEXT(device, *swapchain);
		if (releaseResult == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
		{
			context.getTestContext().getLog() << tcu::TestLog::Message << "Got VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT at vkReleaseFullScreenExclusiveModeEXT" << tcu::TestLog::EndMessage;

			fullScreenLost = true;
		}
		VK_CHECK_WSI(releaseResult);
	}

	native.window->setVisible(false);

	if (fullScreenAcquired && !fullScreenLost)
	{
		return tcu::TestStatus::pass("Rendering tests succeeded");
	}
	else
	{
		if (fullScreenLost)
		{
			return  tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Full screen exclusive was lost during test, but did not end with an error.");
		}
		else
		{
			return  tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Failed to acquire full screen exclusive, but did not end with an error.");
		}
	}
}

void getBasicRenderPrograms (SourceCollections& dst, TestParams)
{
	WsiTriangleRenderer::getPrograms(dst);
}

} // anonymous

void createFullScreenExclusiveTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	struct
	{
		VkFullScreenExclusiveEXT				testType;
		const char*								name;
	} fullScreenTestTypes[] =
	{
		{ VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT,					"default" },
		{ VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT,					"allowed" },
		{ VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT,				"disallowed" },
		{ VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT,	"application_controlled" },
	};

	for (size_t fseNdx = 0; fseNdx < DE_LENGTH_OF_ARRAY(fullScreenTestTypes); ++fseNdx)
	{
		TestParams testParams
		{
			wsiType,
			fullScreenTestTypes[fseNdx].testType
		};
		addFunctionCaseWithPrograms(testGroup, fullScreenTestTypes[fseNdx].name, "", getBasicRenderPrograms, fullScreenExclusiveTest, testParams);
	}
}

} // wsi

} // vkt
