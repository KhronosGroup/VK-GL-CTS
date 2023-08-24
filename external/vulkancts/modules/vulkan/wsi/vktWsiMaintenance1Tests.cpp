/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief VK_EXT_surface_maintenance1 and VK_EXT_swapchain_maintenance1 extension tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiMaintenance1Tests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"

#include "deRandom.hpp"

#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuCommandLine.hpp"

#include <limits>
#include <random>
#include <set>

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

constexpr uint64_t kMaxFenceWaitTimeout = 2000000000ul;

template <typename T>
void checkAllSupported (const Extensions& supportedExtensions,
						const std::vector<T>& requiredExtensions)
{
	for (auto &requiredExtension : requiredExtensions)
	{
		if (!isExtensionStructSupported(supportedExtensions, RequiredExtension(requiredExtension)))
			TCU_THROW(NotSupportedError, (std::string(requiredExtension) + " is not supported").c_str());
	}
}

CustomInstance createInstanceWithWsi (Context&						context,
									  const Extensions&				supportedExtensions,
									  Type							wsiType,
									  bool							requireDeviceGroup,
									  const VkAllocationCallbacks*	pAllocator	= DE_NULL)
{
	const deUint32	version		= context.getUsedApiVersion();
	std::vector<std::string>	extensions;

	extensions.push_back("VK_KHR_surface");
	extensions.push_back(getExtensionName(wsiType));
	if (isDisplaySurface(wsiType))
		extensions.push_back("VK_KHR_display");

	if (!vk::isCoreInstanceExtension(version, "VK_KHR_get_physical_device_properties2"))
		extensions.push_back("VK_KHR_get_physical_device_properties2");

	if (isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_KHR_get_surface_capabilities2")))
		extensions.push_back("VK_KHR_get_surface_capabilities2");

	extensions.push_back("VK_EXT_surface_maintenance1");

	if (requireDeviceGroup)
		extensions.push_back("VK_KHR_device_group_creation");

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
									VkInstance						instance,
									const InstanceInterface&		vki,
									VkPhysicalDevice				physicalDevice,
									const Extensions&				supportedExtensions,
									const deUint32					queueFamilyIndex,
									const VkAllocationCallbacks*	pAllocator,
									bool							requireSwapchainMaintenance1,
									bool							requireDeviceGroup,
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
			&queuePriorities[0],
		}
	};
	const VkPhysicalDeviceFeatures	features		= getDeviceFeaturesForWsi();
	std::vector<const char *>		extensions;

	extensions.push_back("VK_KHR_swapchain");
	if (requireSwapchainMaintenance1)
	{
		extensions.push_back("VK_EXT_swapchain_maintenance1");
	}
	if (requireDeviceGroup)
	{
		extensions.push_back("VK_KHR_device_group");
	}
	if (isExtensionStructSupported(supportedExtensions, RequiredExtension("VK_KHR_shared_presentable_image")))
	{
		extensions.push_back("VK_KHR_shared_presentable_image");
	}

	checkAllSupported(supportedExtensions, extensions);

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
		&features,
	};

	return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

struct InstanceHelper
{
	const std::vector<VkExtensionProperties>	supportedExtensions;
	const CustomInstance				instance;
	const InstanceDriver&				vki;

	InstanceHelper (Context&	context,
					Type		wsiType,
					bool		requireDeviceGroup,
					const		VkAllocationCallbacks* pAllocator = DE_NULL)
		: supportedExtensions	(enumerateInstanceExtensionProperties(context.getPlatformInterface(),
																	  DE_NULL))
		, instance				(createInstanceWithWsi(context,
													   supportedExtensions,
													   wsiType,
													   requireDeviceGroup,
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
				  bool							requireSwapchainMaintenance1,
				  bool							requireDeviceGroup,
				  const VkAllocationCallbacks*	pAllocator = DE_NULL)
		: physicalDevice			(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
		, queueFamilyIndex			(chooseQueueFamilyIndex(vki, physicalDevice, surface))
		, device					(createDeviceWithWsi(context.getPlatformInterface(),
														 instance,
														 vki,
														 physicalDevice,
														 enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL),
														 queueFamilyIndex,
														 pAllocator,
														 requireSwapchainMaintenance1,
														 requireDeviceGroup,
														 context.getTestContext().getCommandLine().isValidationEnabled()))
		, vkd						(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion())
		, queue						(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
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
		if (isExtensionStructSupported(supportedExtensions, RequiredExtension(getExtensionName(wsiType))) &&
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

constexpr deUint32 kDefaultWindowWidth = 128;
constexpr deUint32 kDefaultWindowHeight = 256;

struct TestNativeObjects
{
	const de::UniquePtr<Display>		display;
	tcu::UVec2							windowSize;
	std::vector<de::MovePtr<Window>>	windows;

	TestNativeObjects	(Context&				context,
						 const Extensions&		supportedExtensions,
						 Type					wsiType,
						 deUint32				windowCount)
		: display		(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
		, windowSize	(tcu::UVec2(kDefaultWindowWidth, kDefaultWindowHeight))
	{
		for (deUint32 i = 0; i < windowCount; ++i)
		{
			windows.push_back(createWindow(*display, windowSize));
			windows.back()->setVisible(true);
			if (wsiType == TYPE_WIN32)
			{
				windows.back()->setForeground();
			}
		}
	}
};

VkSwapchainCreateInfoKHR getBasicSwapchainParameters (VkSurfaceKHR					surface,
													  VkSurfaceFormatKHR			surfaceFormat,
													  const tcu::UVec2&				desiredSize,
													  VkPresentModeKHR				presentMode,
													  VkSurfaceTransformFlagBitsKHR	transform,
													  deUint32						desiredImageCount,
													  bool							deferMemoryAllocation)
{
	const VkSwapchainCreateInfoKHR		parameters	=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		(VkSwapchainCreateFlagsKHR)(deferMemoryAllocation ? VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_EXT : 0),
		surface,
		desiredImageCount,
		surfaceFormat.format,
		surfaceFormat.colorSpace,
		vk::makeExtent2D(desiredSize.x(), desiredSize.y()),
		1u,									// imageArrayLayers
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		transform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		presentMode,
		VK_FALSE,							// clipped
		DE_NULL,							// oldSwapchain
	};

	return parameters;
}

VkSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities (const vk::InstanceInterface&	vki,
															   VkPhysicalDevice				physicalDevice,
															   VkSurfaceKHR					surface,
															   VkImageUsageFlags*			sharedImageUsage)
{
	const VkPhysicalDeviceSurfaceInfo2KHR	info		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		DE_NULL,
		surface,
	};
	VkSharedPresentSurfaceCapabilitiesKHR	sharedCapabilities;
	VkSurfaceCapabilities2KHR				capabilities;

	sharedCapabilities.sType	= VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR;
	sharedCapabilities.pNext	= DE_NULL;

	capabilities.sType			= VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	capabilities.pNext			= sharedImageUsage ? &sharedCapabilities : DE_NULL;

	VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

	if (sharedImageUsage)
	{
		*sharedImageUsage		= sharedCapabilities.sharedPresentSupportedUsageFlags;
	}

	return capabilities.surfaceCapabilities;
}

std::vector<VkPresentModeKHR> getSurfaceCompatiblePresentModes (const vk::InstanceInterface&	vki,
																VkPhysicalDevice				physicalDevice,
																VkSurfaceKHR					surface,
																VkPresentModeKHR				presentMode)
{
	VkSurfacePresentModeEXT					presentModeInfo	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
		DE_NULL,
		presentMode,
	};
	const VkPhysicalDeviceSurfaceInfo2KHR	info			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		&presentModeInfo,
		surface,
	};

	// Currently there are 6 present modes, 100 should cover all future ones!
	std::vector<VkPresentModeKHR>			compatibleModes	(100);

	VkSurfacePresentModeCompatibilityEXT	compatibility	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
		DE_NULL,
		(deUint32)compatibleModes.size(),
		compatibleModes.data(),
	};
	VkSurfaceCapabilities2KHR				capabilities	=
	{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		&compatibility,
		{},
	};

	VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

	compatibleModes.resize(compatibility.presentModeCount);
	return compatibleModes;
}

VkSurfacePresentScalingCapabilitiesEXT getSurfaceScalingCapabilities (const vk::InstanceInterface&	vki,
																	  VkPhysicalDevice				physicalDevice,
																	  VkPresentModeKHR				presentMode,
																	  VkSurfaceKHR					surface)
{
	VkSurfacePresentModeEXT					presentModeInfo	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
		DE_NULL,
		presentMode,
	};
	const VkPhysicalDeviceSurfaceInfo2KHR	info			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		&presentModeInfo,
		surface,
	};

	VkSurfacePresentScalingCapabilitiesEXT	scaling			=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_EXT,
		DE_NULL,
		0,
		0,
		0,
		{},
		{},
	};
	VkSurfaceCapabilities2KHR				capabilities	=
	{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		&scaling,
		{},
	};

	VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

	return scaling;
}

VkSurfaceCapabilitiesKHR getPerPresentSurfaceCapabilities (const vk::InstanceInterface&	vki,
														   VkPhysicalDevice				physicalDevice,
														   VkSurfaceKHR					surface,
														   VkPresentModeKHR				presentMode)
{
	VkSurfacePresentModeEXT					presentModeInfo	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
		DE_NULL,
		presentMode,
	};
	const VkPhysicalDeviceSurfaceInfo2KHR	info			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		&presentModeInfo,
		surface,
	};

	VkSurfaceCapabilities2KHR				capabilities	=
	{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		DE_NULL,
		{},
	};

	VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

	return capabilities.surfaceCapabilities;
}

typedef de::SharedPtr<Unique<VkCommandBuffer>>	CommandBufferSp;
typedef de::SharedPtr<Unique<VkFence>>			FenceSp;
typedef de::SharedPtr<Unique<VkSemaphore>>		SemaphoreSp;
typedef de::SharedPtr<Unique<VkImage>>			ImageSp;

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

Move<VkBuffer> createBufferAndBindMemory (const DeviceHelper& devHelper, SimpleAllocator& allocator, const tcu::UVec4 color, deUint32 count, de::MovePtr<Allocation>* pAlloc)
{
	const DeviceInterface&	vkd				= devHelper.vkd;
	const VkDevice			device			= *devHelper.device;
	const deUint32			queueIndex		= devHelper.queueFamilyIndex;

	const VkBufferCreateInfo bufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,       // VkStructureType      sType;
		DE_NULL,                                    // const void*          pNext;
		0u,                                         // VkBufferCreateFlags  flags;
		count * 4,                                  // VkDeviceSize         size;
		vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,       // VkBufferUsageFlags   usage;
		VK_SHARING_MODE_EXCLUSIVE,                  // VkSharingMode        sharingMode;
		1u,                                         // deUint32             queueFamilyCount;
		&queueIndex                                 // const deUint32*      pQueueFamilyIndices;
	};

	Move<VkBuffer> buffer = createBuffer(vkd, device, &bufferParams);

	*pAlloc = allocator.allocate(getBufferMemoryRequirements(vkd, device, *buffer), MemoryRequirement::HostVisible);
	VK_CHECK(vkd.bindBufferMemory(device, *buffer, (*pAlloc)->getMemory(), (*pAlloc)->getOffset()));

	// Upload color to buffer.  Assuming RGBA, but surface format could be different, such as BGRA.  For the purposes of the test, that doesn't matter.
	const deUint32			color32			= color.x() | color.y() << 8 | color.z() << 16 | color.w() << 24;
	std::vector<deUint32>	colors			(count, color32);
	deMemcpy((*pAlloc)->getHostPtr(), colors.data(), colors.size() * sizeof(colors[0]));
	flushAlloc(vkd, device, **pAlloc);

	return buffer;
}

void copyBufferToImage(const DeviceInterface& vkd, VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, const tcu::UVec2 offset, const tcu::UVec2 extent)
{
	const VkBufferImageCopy	region			=
	{
		0,
		0,
		0,
		{
			vk::VK_IMAGE_ASPECT_COLOR_BIT,
			0,
			0,
			1,
		},
		{ (deInt32)offset.x(), (deInt32)offset.y(), 0 },
		{
			extent.x(),
			extent.y(),
			1u,
		},
	};

	vkd.cmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

struct PresentFenceTestConfig
{
	vk::wsi::Type					wsiType;
	std::vector<VkPresentModeKHR>	modes;
	bool							deferMemoryAllocation;
	bool							bindImageMemory;
	bool							changePresentModes;
	bool							verifyFenceOrdering;
};

bool canDoMultiSwapchainPresent(vk::wsi::Type wsiType)
{
	// Android has a bug with the implementation of multi-swapchain present.
	// This bug has existed since Vulkan 1.0 and is unrelated to
	// VK_EXT_swapchain_maintenance1.  Once that bug is fixed, multi-swapchain
	// present tests can be enabled for this platform.
	return wsiType != TYPE_ANDROID;
}

deUint32 getIterations(std::vector<VkPresentModeKHR> presentModes,
					   std::vector<std::vector<VkPresentModeKHR>> compatiblePresentModes,
					   bool testResizesWindowsFrequently)
{
	// Look at all the modes that will be used by the test.
	bool						hasFifo			= false;
	bool						hasShared		= false;
	bool						hasNoVsync		= false;

	std::set<VkPresentModeKHR>	allModes;

	for (VkPresentModeKHR mode : presentModes)
		allModes.insert(mode);

	for (const auto &compatibleModes : compatiblePresentModes)
		for (VkPresentModeKHR mode : compatibleModes)
			allModes.insert(mode);

	for (VkPresentModeKHR mode : allModes)
	{
		switch (mode)
		{
		case VK_PRESENT_MODE_FIFO_KHR:
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
			hasFifo = true;
			break;
		case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
		case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
			hasShared = true;
			break;
		case VK_PRESENT_MODE_IMMEDIATE_KHR:
		case VK_PRESENT_MODE_MAILBOX_KHR:
		default:
			hasNoVsync = true;
			break;
		}
	}

	// Return an iteration count that is as high as possible while keeping the test time and memory usage reasonable.
	//
	// - If FIFO is used, limit to 120 (~2s on 60Hz)
	// - Else, limit to 1000

	if (hasFifo)
		return testResizesWindowsFrequently ? 60 : 120;

	(void)hasShared;
	(void)hasNoVsync;
	deUint32 iterations = 1000;

	// If the test resizes windows frequently, reduce the testing time as that's a very slow operation.
	if (testResizesWindowsFrequently)
		iterations /= 50;

	return iterations;
}

ImageSp bindSingleImageMemory(const DeviceInterface&			vkd,
							  const VkDevice					device,
							  const VkSwapchainKHR				swapchain,
							  const VkSwapchainCreateInfoKHR	swapchainCreateInfo,
							  deUint32							imageIndex)
{
	VkImageSwapchainCreateInfoKHR imageSwapchainCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		swapchain,
	};

	VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&imageSwapchainCreateInfo,
		(VkImageCreateFlags)0u,							// flags
		VK_IMAGE_TYPE_2D,								// imageType
		swapchainCreateInfo.imageFormat,				// format
		{												// extent
			swapchainCreateInfo.imageExtent.width,		//   width
			swapchainCreateInfo.imageExtent.height,		//   height
			1u,											//   depth
		},
		1u,												// mipLevels
		1u,												// arrayLayers
		VK_SAMPLE_COUNT_1_BIT,							// samples
		VK_IMAGE_TILING_OPTIMAL,						// tiling
		swapchainCreateInfo.imageUsage,					// usage
		VK_SHARING_MODE_EXCLUSIVE,						// sharingMode
		0u,												// queueFamilyIndexCount
		DE_NULL,										// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,						// initialLayout
	};

	ImageSp								image = ImageSp(new Unique<VkImage>(createImage(vkd, device, &imageCreateInfo)));

	VkBindImageMemorySwapchainInfoKHR	bimSwapchainInfo =
	{
		VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR,
		DE_NULL,
		swapchain,
		imageIndex,
	};

	VkBindImageMemoryInfo				bimInfo =
	{
		VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
		&bimSwapchainInfo,
		**image,
		DE_NULL,
		0u,
	};

	VK_CHECK(vkd.bindImageMemory2(device, 1, &bimInfo));

	return image;
}

std::vector<ImageSp> bindImageMemory(const DeviceInterface&				vkd,
									 const VkDevice						device,
									 const VkSwapchainKHR				swapchain,
									 const VkSwapchainCreateInfoKHR		swapchainCreateInfo)
{
	deUint32 numImages = 0;
	VK_CHECK(vkd.getSwapchainImagesKHR(device, swapchain, &numImages, DE_NULL));

	std::vector<ImageSp>							images				(numImages);

	for (deUint32 i = 0; i < numImages; ++i)
	{
		images[i] = bindSingleImageMemory(vkd, device, swapchain, swapchainCreateInfo, i);
	}

	return images;
}

void verifyFenceSignalOrdering(const DeviceInterface&		vkd,
							   const VkDevice				device,
							   const std::vector<FenceSp>	&fences,
							   const deUint32				stride,
							   const deUint32				offset,
							   const deUint32				lastKnownSignaled,
							   const deUint32				maxIndex,
							   tcu::ResultCollector*		results)
{
	// Go over fences from end to last-known-signaled.  Verify that fences are
	// signaled in order by making sure that a consecutive set of fences are
	// encountered that are not signaled, followed by potentially a number of
	// fences that are.
	bool visitedSignaledFence = false;
	for (deUint32 i = maxIndex; i > lastKnownSignaled; --i)
	{
		const VkFence fence = **fences[(i - 1) * stride + offset];
		bool isSignaled = vkd.getFenceStatus(device, fence) != VK_NOT_READY;

		// Ordering guarantee is broken if an unsignaled fence is encountered when a later fence is signaled.
		results->check(isSignaled || !visitedSignaledFence,
			"Encountered unsignaled fence while a later fence is signaled");

		if (isSignaled)
		{
			visitedSignaledFence = true;
		}
	}
}

tcu::TestStatus presentFenceTest(Context& context, const PresentFenceTestConfig testParams)
{
	tcu::TestLog&							log				= context.getTestContext().getLog();
	tcu::ResultCollector					results			(log);

	const deUint32							surfaceCount	= (deUint32)testParams.modes.size();
	const InstanceHelper					instHelper		(context, testParams.wsiType, testParams.bindImageMemory);
	const TestNativeObjects					native			(context, instHelper.supportedExtensions, testParams.wsiType, surfaceCount);
	std::vector<Move<VkSurfaceKHR>>			surfaces;
	for (deUint32 i = 0; i < surfaceCount; ++i)
	{
		surfaces.push_back(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.windows[i], context.getTestContext().getCommandLine()));
	}

	const DeviceHelper						devHelper		(context, instHelper.vki, instHelper.instance, *surfaces[0], true, testParams.bindImageMemory);
	const DeviceInterface&					vkd				= devHelper.vkd;
	const VkDevice							device			= *devHelper.device;

	for (deUint32 i = 0; i < surfaceCount; ++i)
	{
		const std::vector<VkPresentModeKHR>	presentModes	= getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surfaces[i]);
		if (std::find(presentModes.begin(), presentModes.end(), testParams.modes[i]) == presentModes.end())
			TCU_THROW(NotSupportedError, "Present mode not supported");
	}

	std::vector<VkSurfaceFormatKHR>			surfaceFormats	= getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surfaces[0]);
	if (surfaceFormats.empty())
		return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

	std::vector<bool>						isSharedPresentMode	(surfaceCount);

	for (deUint32 i = 0; i < surfaceCount; ++i)
	{
		isSharedPresentMode[i]				= testParams.modes[i] == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
												testParams.modes[i] == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
	}

	std::vector<VkSwapchainCreateInfoKHR>	swapchainInfo;
	std::vector<Move<VkSwapchainKHR>>		swapchains;
	std::vector<VkSwapchainKHR>				swapchainHandles;
	std::vector<std::vector<VkImage>>		swapchainImages;
	std::vector<std::vector<ImageSp>>		bimImages;
	std::vector<std::vector<VkPresentModeKHR>> compatiblePresentModes;
	for (deUint32 i = 0; i < surfaceCount; ++i)
	{
		VkImageUsageFlags						sharedImageUsage	= 0;
		const VkSurfaceCapabilitiesKHR			capabilities	= getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surfaces[i], isSharedPresentMode[i] ? &sharedImageUsage : DE_NULL);
		const VkSurfaceTransformFlagBitsKHR		transform		= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;

		if (isSharedPresentMode[i] && (sharedImageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0)
			TCU_THROW(NotSupportedError, "Transfer dst with shared present mode not supported");

		swapchainInfo.push_back(getBasicSwapchainParameters(*surfaces[i], surfaceFormats[0], native.windowSize, testParams.modes[i], transform, isSharedPresentMode[i] ? 1 : capabilities.minImageCount, testParams.deferMemoryAllocation));

		VkSwapchainPresentModesCreateInfoEXT	compatibleModesCreateInfo	=
		{
			VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,
			DE_NULL,
			0,
			DE_NULL,
		};
		if (testParams.changePresentModes)
		{
			compatiblePresentModes.push_back(getSurfaceCompatiblePresentModes(instHelper.vki, devHelper.physicalDevice, *surfaces[i], testParams.modes[i]));

			compatibleModesCreateInfo.presentModeCount	= (deUint32)compatiblePresentModes.back().size();
			compatibleModesCreateInfo.pPresentModes	= compatiblePresentModes.back().data();
			swapchainInfo.back().pNext				= &compatibleModesCreateInfo;
		}

		swapchains.push_back(createSwapchainKHR(vkd, device, &swapchainInfo.back()));
		swapchainHandles.push_back(*swapchains.back());

		if (testParams.bindImageMemory)
		{
			deUint32 numImages = 0;
			VK_CHECK(vkd.getSwapchainImagesKHR(device, *swapchains.back(), &numImages, DE_NULL));
			swapchainImages.push_back(std::vector<VkImage>(numImages, DE_NULL));

			// If memory allocation is deferred, bind image memory lazily at acquire time.
			if (testParams.deferMemoryAllocation)
			{
				bimImages.push_back(std::vector<ImageSp>(numImages));
			}
			else
			{
				bimImages.push_back(bindImageMemory(vkd, device, *swapchains.back(), swapchainInfo.back()));
				for (size_t j = 0; j < bimImages.back().size(); ++j)
				{
					swapchainImages.back()[j]					= **bimImages.back()[j];
				}
			}
		}
		else
		{
			swapchainImages.push_back(getSwapchainImages(vkd, device, *swapchains.back()));
		}
	}

	const Unique<VkCommandPool>				commandPool		(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

	const deUint32							iterations		= getIterations(testParams.modes, compatiblePresentModes, false);

	// Do iterations presents, each with an associated fence.  Destroy the wait semaphores as soon as the corresponding fence signals.
	const std::vector<FenceSp>				presentFences	(createFences(vkd, device, iterations * surfaceCount));
	const std::vector<SemaphoreSp>			acquireSems		(createSemaphores(vkd, device, iterations * surfaceCount));
	std::vector<SemaphoreSp>				presentSems		(createSemaphores(vkd, device, iterations));

	const std::vector<CommandBufferSp>		commandBuffers	(allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, iterations));

	const deUint64							foreverNs		= 0xFFFFFFFFFFFFFFFFul;

	VkImageSubresourceRange					range			=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		0,
		1,
		0,
		1,
	};

	const deUint32							configHash		=
		(deUint32)testParams.wsiType |
		(deUint32)testParams.modes[0] << 4 |
		(deUint32)testParams.deferMemoryAllocation << 28 |
		(deUint32)testParams.bindImageMemory << 29 |
		(deUint32)testParams.changePresentModes << 30 |
		(deUint32)testParams.verifyFenceOrdering << 31;
	de::Random								rng				(0x53A4C8A1u ^ configHash);

	try
	{
		std::vector<deUint32>				nextUnfinishedPresent(surfaceCount, 0);

		for (deUint32 i = 0; i < iterations; ++i)
		{
			const VkSemaphore*				presentSem		= &**presentSems[i];
			std::vector<VkSemaphore>		acquireSem;
			std::vector<VkFence>			presentFence;
			std::vector<deUint32>			imageIndex		(surfaceCount, 0x12345);	// initialize to junk value
			// Acquire an image and clear it
			beginCommandBuffer(vkd, **commandBuffers[i], 0u);

			VkImageMemoryBarrier barrier = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				DE_NULL,
				0,
				0,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				DE_NULL,
				range,
			};

			for (deUint32 j = 0; j < surfaceCount; ++j)
			{
				acquireSem.push_back(**acquireSems[i * surfaceCount + j]);
				presentFence.push_back(**presentFences[i * surfaceCount+ j]);

				VK_CHECK(vkd.acquireNextImageKHR(device, *swapchains[j], foreverNs, acquireSem[j], DE_NULL, &imageIndex[j]));

				// If memory allocation is deferred and bind image memory is used, lazily bind image memory now if this is the first time the image is acquired.
				VkImage&					acquiredImage	= swapchainImages[j][imageIndex[j]];
				if (acquiredImage == DE_NULL)
				{
					DE_ASSERT(testParams.bindImageMemory && testParams.deferMemoryAllocation);
					DE_ASSERT(!bimImages[j][imageIndex[j]]);

					bimImages[j][imageIndex[j]] = bindSingleImageMemory(vkd, device, *swapchains[j], swapchainInfo[j], imageIndex[j]);
					acquiredImage = **bimImages[j][imageIndex[j]];
				}


				barrier.newLayout			= isSharedPresentMode[j] ? VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				barrier.image				= acquiredImage;

				vkd.cmdPipelineBarrier(**commandBuffers[i],
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						0u,
						0, DE_NULL,
						0, DE_NULL,
						1, &barrier);
			}

			for (deUint32 j = 0; j < surfaceCount; ++j)
			{
				VkClearColorValue				clearValue;
				clearValue.float32[0]			= static_cast<float>((i + j * 5) % 33) / 32.0f;
				clearValue.float32[1]			= static_cast<float>(((i + j * 5) + 7) % 33) / 32.0f;
				clearValue.float32[2]			= static_cast<float>(((i + j * 5) + 17) % 33) / 32.0f;
				clearValue.float32[3]			= 1.0f;

				vkd.cmdClearColorImage(**commandBuffers[i], swapchainImages[j][imageIndex[j]], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
			}

			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			for (deUint32 j = 0; j < surfaceCount; ++j)
			{
				if (!isSharedPresentMode[j])
				{
					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				}
				else
				{
					barrier.oldLayout = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
				}
				barrier.image = swapchainImages[j][imageIndex[j]];

				vkd.cmdPipelineBarrier(**commandBuffers[i],
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						0u,
						0, DE_NULL,
						0, DE_NULL,
						1, &barrier);
			}

			endCommandBuffer(vkd, **commandBuffers[i]);

			// Submit the command buffer
			VkPipelineStageFlags			waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			const VkSubmitInfo				submitInfo =
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,
				DE_NULL,
				surfaceCount,
				acquireSem.data(),
				&waitStage,
				1u,
				&**commandBuffers[i],
				1u,
				presentSem,
			};
			VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, DE_NULL));

			// Present the frame
			VkSwapchainPresentFenceInfoEXT presentFenceInfo		=
			{
				VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
				DE_NULL,
				surfaceCount,
				presentFence.data(),
			};
			std::vector<VkResult> result(surfaceCount);

			VkSwapchainPresentModeInfoEXT	presentModeInfo		=
			{
				VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT,
				DE_NULL,
				surfaceCount,
				DE_NULL,
			};
			std::vector<VkPresentModeKHR>	presentModes;
			if (testParams.changePresentModes && rng.getUint32() % 10 != 0)
			{
				presentModes.resize(surfaceCount);
				presentModeInfo.pPresentModes				= presentModes.data();
				presentFenceInfo.pNext						= &presentModeInfo;

				// Randomly switch modes.  This is randomly not done to test that the driver doens't expect it to be specified every time.
				for (deUint32 j = 0; j < surfaceCount; ++j)
				{
					deUint32				randomIndex		= rng.getUint32() % (deUint32)compatiblePresentModes[j].size();
					presentModes[j]							= compatiblePresentModes[j][randomIndex];
				}
			}

			const VkPresentInfoKHR presentInfo	=
			{
				VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				&presentFenceInfo,
				1u,
				presentSem,
				surfaceCount,
				swapchainHandles.data(),
				imageIndex.data(),
				result.data(),
			};
			VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
			for (deUint32 j = 0; j < surfaceCount; ++j)
			{
				VK_CHECK_WSI(result[j]);
			}

			for (deUint32 j = 0; j < surfaceCount; ++j)
			{
				// Check previous presents; if any is signaled, immediatey destroy its wait semaphore
				while (nextUnfinishedPresent[j] < i)
				{
					if (vkd.getFenceStatus(device, **presentFences[nextUnfinishedPresent[j] * surfaceCount + j]) == VK_NOT_READY)
						break;

					presentSems[nextUnfinishedPresent[j]].clear();
					++nextUnfinishedPresent[j];
				}

				if (testParams.verifyFenceOrdering)
					verifyFenceSignalOrdering(vkd, device, presentFences, surfaceCount, j, nextUnfinishedPresent[j], iterations, &results);
			}
		}

		// Wait for outstanding presents and destroy their wait semaphores
		for (deUint32 j = 0; j < surfaceCount; ++j)
		{
			if (testParams.verifyFenceOrdering)
				verifyFenceSignalOrdering(vkd, device, presentFences, surfaceCount, j, nextUnfinishedPresent[j], iterations, &results);

			while (nextUnfinishedPresent[j] < iterations)
			{
				VK_CHECK(vkd.waitForFences(device, 1u, &**presentFences[nextUnfinishedPresent[j] * surfaceCount + j], VK_TRUE, kMaxFenceWaitTimeout));
				presentSems[nextUnfinishedPresent[j]].clear();
				++nextUnfinishedPresent[j];
			}
		}
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	for (deUint32 i = 0; i < surfaceCount; ++i)
	{
		native.windows[i]->setVisible(false);
	}

	return tcu::TestStatus(results.getResult(), results.getMessage());
}

void populatePresentFenceGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	const struct
	{
		VkPresentModeKHR	mode;
		const char*			name;
	} presentModes[] =
	{
		{ VK_PRESENT_MODE_IMMEDIATE_KHR,					"immediate"		},
		{ VK_PRESENT_MODE_MAILBOX_KHR,						"mailbox"		},
		{ VK_PRESENT_MODE_FIFO_KHR,							"fifo"			},
		{ VK_PRESENT_MODE_FIFO_RELAXED_KHR,					"fifo_relaxed"	},
		{ VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,		"demand"		},
		{ VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,	"continuous"	},
	};

	for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup>	presentModeGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name, presentModes[presentModeNdx].name));

		PresentFenceTestConfig			config;
		config.wsiType					= wsiType;
		config.modes					= std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
		config.deferMemoryAllocation	= false;
		config.bindImageMemory			= false;
		config.changePresentModes		= false;
		config.verifyFenceOrdering		= false;

		addFunctionCase(&*presentModeGroup, "basic", "Basic present fence test", presentFenceTest, config);

		config.verifyFenceOrdering		= true;
		addFunctionCase(&*presentModeGroup, "ordering", "Test ordering guarantee of present fence signals", presentFenceTest, config);

		if (canDoMultiSwapchainPresent(wsiType))
		{
			config.verifyFenceOrdering		= false;
			config.modes					= std::vector<VkPresentModeKHR>(3, presentModes[presentModeNdx].mode);
			addFunctionCase(&*presentModeGroup, "multi_swapchain", "Present fence test with multiple swapchains", presentFenceTest, config);

			config.verifyFenceOrdering		= true;
			addFunctionCase(&*presentModeGroup, "mult_swapchain_ordering", "Test ordering guarantee of present fence signals with multiple swapchains", presentFenceTest, config);
		}

		testGroup->addChild(presentModeGroup.release());
	}
}

struct PresentModesTestConfig
{
	vk::wsi::Type		wsiType;
	VkPresentModeKHR	mode;
};

tcu::TestStatus verifyCompatiblePresentModes(const std::vector<VkPresentModeKHR>&	supportedModes,
											 const VkPresentModeKHR					queryMode,
											 const std::vector<VkPresentModeKHR>&	compatibleModes,
											 const std::vector<VkPresentModeKHR>*	previouslyQueriedCompatibleModes)
{
	// Every returned compatible mode must be supported by the surface
	for (size_t i = 0; i < compatibleModes.size(); ++i)
		if (std::find(supportedModes.begin(), supportedModes.end(), compatibleModes[i]) == supportedModes.end())
			return tcu::TestStatus::fail("Returned compatible present mode " + de::toString(compatibleModes[i]) + " is not a supported present mode");

	// The original mode being queried must always be in the compatible list
	if (!compatibleModes.empty() && std::find(compatibleModes.begin(), compatibleModes.end(), queryMode) == compatibleModes.end())
		return tcu::TestStatus::fail("Returned compatible present modes does not include the mode used in the query");

	// There should be no duplicates in the returned modes
	std::set<VkPresentModeKHR> visitedModes;
	for (VkPresentModeKHR compatibleMode : compatibleModes)
	{
		if (visitedModes.find(compatibleMode) != visitedModes.end())
			return tcu::TestStatus::fail("Duplicate mode " + de::toString(compatibleMode) + " returned in list of compatible present modes");
		visitedModes.insert(compatibleMode);
	}

	// If provided, the returned list of modes should match the last previous query
	if (previouslyQueriedCompatibleModes)
	{
		for (VkPresentModeKHR previousCompatibleMode : *previouslyQueriedCompatibleModes)
			if (visitedModes.find(previousCompatibleMode) == visitedModes.end())
				return tcu::TestStatus::fail("Different sets of compatible modes returned on re-query (present mode " + de::toString(previousCompatibleMode) + " missing on requery)");
	}

	return tcu::TestStatus::pass("");
}

tcu::TestStatus presentModesQueryTest(Context& context, const PresentModesTestConfig testParams)
{
	const InstanceHelper					instHelper		(context, testParams.wsiType, false);
	const TestNativeObjects					native			(context, instHelper.supportedExtensions, testParams.wsiType, 1);
	Unique<VkSurfaceKHR>					surface			(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.windows[0], context.getTestContext().getCommandLine()));
	const DeviceHelper						devHelper		(context, instHelper.vki, instHelper.instance, *surface, false, false);

	const std::vector<VkPresentModeKHR>		presentModes	= getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
	if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
		TCU_THROW(NotSupportedError, "Present mode not supported");

	// Get the compatible present modes with the given one.
	VkSurfacePresentModeEXT					presentModeInfo	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
		DE_NULL,
		testParams.mode,
	};
	const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		&presentModeInfo,
		*surface,
	};
	VkSurfacePresentModeCompatibilityEXT	compatibility	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
		DE_NULL,
		0,
		DE_NULL,
	};
	VkSurfaceCapabilities2KHR				capabilities	=
	{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		&compatibility,
		{},
	};

	// Test that querying only the count works.
	VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

	// The return value must be at least one, as every mode is compatible with itself.
	if (compatibility.presentModeCount < 1)
		return tcu::TestStatus::fail("Empty compatible present mode list");

	// Test again providing a buffer that's too small
	constexpr VkPresentModeKHR				invalidValue	= (VkPresentModeKHR)0x1234;
	std::vector<VkPresentModeKHR>			compatibleModes	(compatibility.presentModeCount, invalidValue);
	compatibility.pPresentModes								= compatibleModes.data();

	uint32_t								originalCompatibleModesCount = compatibility.presentModeCount;

	// Check result when count is 0
	compatibility.presentModeCount									= 0;
	VkResult result = instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail("Wrong result when the size is 0");

	// Check result when count is too small
	compatibility.presentModeCount									= originalCompatibleModesCount - 1;
	result = instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail("Wrong result when the size is too small");

	// Make sure whatever _is_ returned is valid.
	if (compatibility.presentModeCount > originalCompatibleModesCount - 1)
		return tcu::TestStatus::fail("Re-query returned more results than provided");

	// Ensure the rest of the array is not overwritten
	for (size_t i = compatibility.presentModeCount; i < compatibleModes.size(); ++i)
	{
		if (compatibleModes[i] != invalidValue)
			return tcu::TestStatus::fail("Query overwrote beyond returned count");
	}
	compatibleModes.resize(compatibility.presentModeCount);
	tcu::TestStatus status = verifyCompatiblePresentModes(presentModes, testParams.mode, compatibleModes, nullptr);
	if (status.isFail())
		return status;

	// Check result when count is correct
	compatibility.presentModeCount									= originalCompatibleModesCount;
	std::vector<VkPresentModeKHR>			compatibleModes2(compatibility.presentModeCount, invalidValue);
	compatibility.pPresentModes								= compatibleModes2.data();

	VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

	// Make sure returned modes are valid.
	if (compatibility.presentModeCount != originalCompatibleModesCount)
		return tcu::TestStatus::fail("Re-query returned different results count than provided");

	status = verifyCompatiblePresentModes(presentModes, testParams.mode, compatibleModes2, &compatibleModes);
	if (status.isFail())
		return status;

	// Check that querying with a count higher than supported still returns as many results as before.
	compatibility.presentModeCount									= originalCompatibleModesCount * 2;
	std::vector<VkPresentModeKHR>			compatibleModes3(compatibility.presentModeCount, invalidValue);
	compatibility.pPresentModes								= compatibleModes3.data();

	VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

	// Make sure returned modes are the same as before.
	if (compatibility.presentModeCount != originalCompatibleModesCount)
		return tcu::TestStatus::fail("Re-query returned different results count than provided");

	// Ensure the rest of the array is not overwritten
	for (size_t i = compatibility.presentModeCount; i < compatibleModes3.size(); ++i)
	{
		if (compatibleModes3[i] != invalidValue)
			return tcu::TestStatus::fail("Query overwrote beyond returned count");
	}

	compatibleModes3.resize(compatibility.presentModeCount);
	status = verifyCompatiblePresentModes(presentModes, testParams.mode, compatibleModes3, &compatibleModes2);
	if (status.isFail())
		return status;

	return tcu::TestStatus::pass("Tests ran successfully");
}

void populatePresentModesGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	const struct
	{
		VkPresentModeKHR	mode;
		const char*			name;
	} presentModes[] =
	{
		{ VK_PRESENT_MODE_IMMEDIATE_KHR,					"immediate"		},
		{ VK_PRESENT_MODE_MAILBOX_KHR,						"mailbox"		},
		{ VK_PRESENT_MODE_FIFO_KHR,							"fifo"			},
		{ VK_PRESENT_MODE_FIFO_RELAXED_KHR,					"fifo_relaxed"	},
		{ VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,		"demand"		},
		{ VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,	"continuous"	},
	};

	for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup>	presentModeGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name, presentModes[presentModeNdx].name));

		{
			PresentModesTestConfig		config;
			config.wsiType				= wsiType;
			config.mode					= presentModes[presentModeNdx].mode;

			addFunctionCase(&*presentModeGroup, "query", "Query compatible present modes", presentModesQueryTest, config);
		}

		{
			PresentFenceTestConfig			config;
			config.wsiType					= wsiType;
			config.modes					= std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
			config.deferMemoryAllocation	= false;
			config.bindImageMemory			= false;
			config.changePresentModes		= true;
			config.verifyFenceOrdering		= false;

			addFunctionCase(&*presentModeGroup, "change_modes", "Switch between compatible modes", presentFenceTest, config);

			if (canDoMultiSwapchainPresent(wsiType))
			{
				config.modes					= std::vector<VkPresentModeKHR>(4, presentModes[presentModeNdx].mode);

				addFunctionCase(&*presentModeGroup, "change_modes_multi_swapchain", "Switch between compatible modes with multiple swapchains", presentFenceTest, config);

				config.modes					= std::vector<VkPresentModeKHR>(2, presentModes[presentModeNdx].mode);
				config.deferMemoryAllocation	= true;

				addFunctionCase(&*presentModeGroup, "change_modes_with_deferred_alloc", "Switch between compatible modes while swapchain uses deferred allocation", presentFenceTest, config);
			}
		}

		testGroup->addChild(presentModeGroup.release());
	}

	if (canDoMultiSwapchainPresent(wsiType))
	{
		de::MovePtr<tcu::TestCaseGroup>	heterogenousGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), "heterogenous", "Switch between compatible modes with multiple swapchains in different modes"));

		std::vector<VkPresentModeKHR>	modes(3);
		for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(presentModes); i++)
		{
			for (size_t j = 0; j < DE_LENGTH_OF_ARRAY(presentModes); j++)
			{
				for (size_t k = 0; k < DE_LENGTH_OF_ARRAY(presentModes); k++)
				{
					// Skip if not actually heterogenous
					if (i == j && i == k)
						continue;

					std::string						testName	= presentModes[i].name;
					testName									+= "_";
					testName									+= presentModes[j].name;
					testName									+= "_";
					testName									+= presentModes[k].name;

					modes[0]									= presentModes[i].mode;
					modes[1]									= presentModes[j].mode;
					modes[2]									= presentModes[k].mode;

					PresentFenceTestConfig			config;
					config.wsiType					= wsiType;
					config.modes					= modes;
					config.deferMemoryAllocation	= false;
					config.bindImageMemory			= false;
					config.changePresentModes		= true;
					config.verifyFenceOrdering		= false;

					addFunctionCase(&*heterogenousGroup, testName, testName, presentFenceTest, config);
				}
			}
		}

		testGroup->addChild(heterogenousGroup.release());
	}
}

enum class SwapchainWindowSize
{
	Identical,
	SwapchainBigger,
	SwapchainSmaller,
};

enum class SwapchainWindowAspect
{
	Identical,
	SwapchainTaller,
	SwapchainWider,
};

struct ScalingQueryTestConfig
{
	vk::wsi::Type					wsiType;
	VkPresentModeKHR				mode;
};

struct ScalingTestConfig
{
	vk::wsi::Type					wsiType;
	VkPresentModeKHR				mode;
	VkPresentScalingFlagsEXT		scaling;
	VkPresentGravityFlagsEXT		gravityX;
	VkPresentGravityFlagsEXT		gravityY;
	SwapchainWindowSize				size;
	SwapchainWindowAspect			aspect;
	// Either have the swapchain be created with a different size, or resize the window after swapchain creation
	bool							resizeWindow;
};

tcu::TestStatus scalingQueryTest(Context& context, const ScalingQueryTestConfig testParams)
{
	const InstanceHelper					instHelper		(context, testParams.wsiType, false);
	const TestNativeObjects					native			(context, instHelper.supportedExtensions, testParams.wsiType, 1);
	Unique<VkSurfaceKHR>					surface			(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.windows[0], context.getTestContext().getCommandLine()));
	const DeviceHelper						devHelper		(context, instHelper.vki, instHelper.instance, *surface, false, false);

	// Query the scaling capabilities and make sure they only report acceptable values.
	VkSurfacePresentScalingCapabilitiesEXT	scaling			= getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, testParams.mode, *surface);

	constexpr VkPresentScalingFlagsEXT		scalingFlags	= VK_PRESENT_SCALING_ONE_TO_ONE_BIT_EXT | VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_EXT | VK_PRESENT_SCALING_STRETCH_BIT_EXT;
	constexpr VkPresentGravityFlagsEXT		gravityFlags	= VK_PRESENT_GRAVITY_MIN_BIT_EXT | VK_PRESENT_GRAVITY_MAX_BIT_EXT | VK_PRESENT_GRAVITY_CENTERED_BIT_EXT;

	if ((scaling.supportedPresentScaling & ~scalingFlags) != 0)
		return tcu::TestStatus::fail("Invalid bits in scaling flags");

	if ((scaling.supportedPresentGravityX & ~gravityFlags) != 0)
		return tcu::TestStatus::fail("Invalid bits in gravity flags (x axis)");

	if ((scaling.supportedPresentGravityY & ~gravityFlags) != 0)
		return tcu::TestStatus::fail("Invalid bits in gravity flags (y axis)");

	return tcu::TestStatus::pass("Tests ran successfully");
}

tcu::TestStatus scalingQueryCompatibleModesTest(Context& context, const ScalingQueryTestConfig testParams)
{
	const InstanceHelper					instHelper		(context, testParams.wsiType, false);
	const TestNativeObjects					native			(context, instHelper.supportedExtensions, testParams.wsiType, 1);
	Unique<VkSurfaceKHR>					surface			(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.windows[0], context.getTestContext().getCommandLine()));
	const DeviceHelper						devHelper		(context, instHelper.vki, instHelper.instance, *surface, false, false);

	// Query compatible present modes, and scaling capabilities for each mode.  They must all be identical.
	VkSurfacePresentModeEXT					presentModeInfo	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT,
		DE_NULL,
		testParams.mode,
	};
	const VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		&presentModeInfo,
		*surface,
	};
	VkSurfacePresentModeCompatibilityEXT	compatibility	=
	{
		VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT,
		DE_NULL,
		0,
		DE_NULL,
	};
	VkSurfaceCapabilities2KHR				capabilities	=
	{
		VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
		&compatibility,
		{},
	};

	VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));
	std::vector<VkPresentModeKHR>			compatibleModes	(compatibility.presentModeCount, (VkPresentModeKHR)0x5678);
	compatibility.pPresentModes								= compatibleModes.data();

	VK_CHECK(instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(devHelper.physicalDevice, &surfaceInfo, &capabilities));

	std::vector<VkSurfacePresentScalingCapabilitiesEXT>	scaling(compatibility.presentModeCount);

	for (uint32_t i = 0; i < compatibility.presentModeCount; ++i)
		scaling[i]											= getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, compatibleModes[i], *surface);

	for (uint32_t i = 1; i < compatibility.presentModeCount; ++i)
	{
		if (scaling[i].supportedPresentScaling != scaling[0].supportedPresentScaling)
			return tcu::TestStatus::fail("Different scaling flags for compatible present modes is not allowed");

		if (scaling[i].supportedPresentGravityX != scaling[0].supportedPresentGravityX)
			return tcu::TestStatus::fail("Different gravity flags (x axis) for compatible present modes is not allowed");

		if (scaling[i].supportedPresentGravityY != scaling[0].supportedPresentGravityY)
			return tcu::TestStatus::fail("Different gravity flags (y axis) for compatible present modes is not allowed");
	}

	return tcu::TestStatus::pass("Tests ran successfully");
}

tcu::TestStatus scalingTest(Context& context, const ScalingTestConfig testParams)
{
	const InstanceHelper					instHelper		(context, testParams.wsiType, false);
	const TestNativeObjects					native			(context, instHelper.supportedExtensions, testParams.wsiType, 1);
	Unique<VkSurfaceKHR>					surface			(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.windows[0], context.getTestContext().getCommandLine()));

	const DeviceHelper						devHelper		(context, instHelper.vki, instHelper.instance, *surface, true, false);
	const DeviceInterface&					vkd				= devHelper.vkd;
	const VkDevice							device			= *devHelper.device;
	SimpleAllocator							allocator		(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice));

	std::vector<VkSurfaceFormatKHR>			surfaceFormats	= getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surface);
	if(surfaceFormats.empty())
		return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

	const VkSurfaceCapabilitiesKHR			capabilities	= getPhysicalDeviceSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface, DE_NULL);
	const VkSurfaceTransformFlagBitsKHR		transform		= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;

	const std::vector<VkPresentModeKHR>		presentModes	= getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
	if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
		TCU_THROW(NotSupportedError, "Present mode not supported");

	// Skip if configuration is not supported
	VkSurfacePresentScalingCapabilitiesEXT	scaling			= getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, testParams.mode, *surface);

	if ((scaling.supportedPresentScaling & testParams.scaling) == 0)
		TCU_THROW(NotSupportedError, "Scaling mode is not supported");
	if (testParams.scaling != VK_PRESENT_SCALING_STRETCH_BIT_EXT)
	{
		if ((scaling.supportedPresentGravityX & testParams.gravityX) == 0)
			TCU_THROW(NotSupportedError, "Gravity mode is not supported (x axis)");
		if ((scaling.supportedPresentGravityY & testParams.gravityY) == 0)
			TCU_THROW(NotSupportedError, "Gravity mode is not supported (y axis)");
	}

	tcu::UVec2								swapchainSize	= native.windowSize;
	if (!testParams.resizeWindow)
	{
		switch (testParams.size)
		{
		case SwapchainWindowSize::SwapchainBigger:
			swapchainSize.x()				*= 2;
			swapchainSize.y()				*= 2;
			break;
		case SwapchainWindowSize::SwapchainSmaller:
			swapchainSize.x()				/= 2;
			swapchainSize.y()				/= 2;
			break;
		default:
			break;
		}
		switch (testParams.aspect)
		{
		case SwapchainWindowAspect::SwapchainTaller:
			swapchainSize.y()				+= swapchainSize.y() / 2;
			break;
		case SwapchainWindowAspect::SwapchainWider:
			swapchainSize.x()				+= swapchainSize.x() / 2;
			break;
		default:
			break;
		}
	}

	VkSwapchainCreateInfoKHR				swapchainInfo	= getBasicSwapchainParameters(*surface, surfaceFormats[0], swapchainSize, testParams.mode, transform, capabilities.minImageCount, false);

	VkSwapchainPresentScalingCreateInfoEXT	scalingInfo		=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,
		DE_NULL,
		testParams.scaling,
		testParams.gravityX,
		testParams.gravityY,
	};
	swapchainInfo.pNext										= &scalingInfo;

	const Unique<VkSwapchainKHR>			swapchain		(createSwapchainKHR(vkd, device, &swapchainInfo));
	std::vector<VkImage>					swapchainImages	= getSwapchainImages(vkd, device, *swapchain);

	const Unique<VkCommandPool>				commandPool		(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

	constexpr deUint32						iterations		= 100;

	// Do testParams.iterations presents, with a fence associated with the last one.
	FenceSp									presentFence	= FenceSp(new Unique<VkFence>(createFence(vkd, device)));
	const std::vector<SemaphoreSp>			acquireSems		(createSemaphores(vkd, device, iterations));
	const std::vector<SemaphoreSp>			presentSems		(createSemaphores(vkd, device, iterations));

	const std::vector<CommandBufferSp>		commandBuffers	(allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, iterations));

	const deUint64							foreverNs		= 0xFFFFFFFFFFFFFFFFul;

	VkImageSubresourceRange					range			=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		0,
		1,
		0,
		1,
	};

	tcu::UVec2								windowSize	= native.windowSize;
	if (testParams.resizeWindow)
	{
		switch (testParams.size)
		{
		case SwapchainWindowSize::SwapchainBigger:
			windowSize.x()					/= 2;
			windowSize.y()					/= 2;
			break;
		case SwapchainWindowSize::SwapchainSmaller:
			windowSize.x()					*= 2;
			windowSize.y()					*= 2;
			break;
		default:
			break;
		}
		switch (testParams.aspect)
		{
		case SwapchainWindowAspect::SwapchainTaller:
			windowSize.x()					+= windowSize.x() / 2;
			break;
		case SwapchainWindowAspect::SwapchainWider:
			windowSize.y()					+= windowSize.y() / 2;
			break;
		default:
			break;
		}

		native.windows[0]->resize(windowSize);
	}

	const deUint32							quarterPixels	= swapchainSize.x() * swapchainSize.y() / 4;
	const tcu::UVec4						red				(255, 30, 20, 255);
	const tcu::UVec4						green			(0, 255, 50, 255);
	const tcu::UVec4						blue			(40, 60, 255, 255);
	const tcu::UVec4						yellow			(200, 220, 20, 255);
	de::MovePtr<Allocation>					redMemory;
	de::MovePtr<Allocation>					greenMemory;
	de::MovePtr<Allocation>					blueMemory;
	de::MovePtr<Allocation>					yellowMemory;
	const vk::Move<vk::VkBuffer>			redBuffer		= createBufferAndBindMemory(devHelper, allocator, red, quarterPixels, &redMemory);
	const vk::Move<vk::VkBuffer>			greenBuffer		= createBufferAndBindMemory(devHelper, allocator, green, quarterPixels, &greenMemory);
	const vk::Move<vk::VkBuffer>			blueBuffer		= createBufferAndBindMemory(devHelper, allocator, blue, quarterPixels, &blueMemory);
	const vk::Move<vk::VkBuffer>			yellowBuffer	= createBufferAndBindMemory(devHelper, allocator, yellow, quarterPixels, &yellowMemory);

	try
	{
		for (deUint32 i = 0; i < iterations; ++i)
		{
			const VkSemaphore				presentSem		= **presentSems[i];
			const VkSemaphore				acquireSem		= **acquireSems[i];
			deUint32						imageIndex		= 0x12345;	// initialize to junk value

			VK_CHECK(vkd.acquireNextImageKHR(device, *swapchain, foreverNs, acquireSem, DE_NULL, &imageIndex));

			beginCommandBuffer(vkd, **commandBuffers[i], 0u);

			VkImageMemoryBarrier barrier = {
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				DE_NULL,
				0,
				0,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				swapchainImages[imageIndex],
				range,
			};

			vkd.cmdPipelineBarrier(**commandBuffers[i],
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					0u,
					0, DE_NULL,
					0, DE_NULL,
					1, &barrier);

			const tcu::UVec2				halfSwapchainSize	= swapchainSize / 2u;
			copyBufferToImage(vkd, **commandBuffers[i], *redBuffer, swapchainImages[imageIndex], tcu::UVec2(0, 0), halfSwapchainSize);
			copyBufferToImage(vkd, **commandBuffers[i], *greenBuffer, swapchainImages[imageIndex], tcu::UVec2(halfSwapchainSize.x(), 0), tcu::UVec2(swapchainSize.x() - halfSwapchainSize.x(), halfSwapchainSize.y()));
			copyBufferToImage(vkd, **commandBuffers[i], *blueBuffer, swapchainImages[imageIndex], tcu::UVec2(0, halfSwapchainSize.y()), tcu::UVec2(halfSwapchainSize.x(), swapchainSize.y() - halfSwapchainSize.y()));
			copyBufferToImage(vkd, **commandBuffers[i], *yellowBuffer, swapchainImages[imageIndex], halfSwapchainSize, tcu::UVec2(swapchainSize.x() - halfSwapchainSize.x(), swapchainSize.y() - halfSwapchainSize.y()));

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			vkd.cmdPipelineBarrier(**commandBuffers[i],
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					0u,
					0, DE_NULL,
					0, DE_NULL,
					1, &barrier);

			endCommandBuffer(vkd, **commandBuffers[i]);

			// Submit the command buffer
			VkPipelineStageFlags			waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			const VkSubmitInfo				submitInfo =
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,
				DE_NULL,
				1,
				&acquireSem,
				&waitStage,
				1u,
				&**commandBuffers[i],
				1u,
				&presentSem,
			};
			VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, DE_NULL));

			// Present the frame
			const VkSwapchainPresentFenceInfoEXT presentFenceInfo =
			{
				VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
				DE_NULL,
				1,
				&**presentFence,
			};
			VkResult result;

			const VkPresentInfoKHR presentInfo =
			{
				VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				// Signal the present fence on the last present.
				i + 1 == iterations ? &presentFenceInfo : nullptr,
				1u,
				&presentSem,
				1,
				&*swapchain,
				&imageIndex,
				&result,
			};
			VK_CHECK_WSI(vkd.queuePresentKHR(devHelper.queue, &presentInfo));
			VK_CHECK_WSI(result);

			// TODO: wait for present, capture the screen and verify that scaling is done correctly.
		}

		// Wait for all presents before terminating the test (when semaphores are destroyed)
		VK_CHECK(vkd.waitForFences(device, 1u, &**presentFence, VK_TRUE, kMaxFenceWaitTimeout));
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	native.windows[0]->setVisible(false);

	return tcu::TestStatus::pass("Tests ran successfully");
}

void populateScalingTests (tcu::TestCaseGroup *testGroup, Type wsiType, bool resizeWindow)
{
	const struct
	{
		VkPresentModeKHR	mode;
		const char*			name;
	} presentModes[] =
	{
		{ VK_PRESENT_MODE_IMMEDIATE_KHR,					"immediate"		},
		{ VK_PRESENT_MODE_MAILBOX_KHR,						"mailbox"		},
		{ VK_PRESENT_MODE_FIFO_KHR,							"fifo"			},
		{ VK_PRESENT_MODE_FIFO_RELAXED_KHR,					"fifo_relaxed"	},
		{ VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,		"demand"		},
		{ VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,	"continuous"	},
	};

	const struct
	{
		VkPresentScalingFlagBitsEXT		scaling;
		const char*						name;
	} scalingFlags[] =
	{
		{ VK_PRESENT_SCALING_ONE_TO_ONE_BIT_EXT,			"one_to_one"		},
		{ VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_EXT,	"aspect_stretch"	},
		{ VK_PRESENT_SCALING_STRETCH_BIT_EXT,				"stretch"			},
	};

	const struct
	{
		VkPresentGravityFlagBitsEXT		gravity;
		const char*						name;
	} gravityFlags[] =
	{
		{ VK_PRESENT_GRAVITY_MIN_BIT_EXT,		"min"		},
		{ VK_PRESENT_GRAVITY_MAX_BIT_EXT,		"max"		},
		{ VK_PRESENT_GRAVITY_CENTERED_BIT_EXT,	"center"	},
	};

	for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup>	presentModeGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name, presentModes[presentModeNdx].name));

		{
			ScalingQueryTestConfig	config;
			config.wsiType			= wsiType;
			config.mode				= presentModes[presentModeNdx].mode;

			de::MovePtr<tcu::TestCaseGroup>	queryGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), "query", "Query supported scaling modes"));
			addFunctionCase(&*queryGroup, "basic", "Basic test", scalingQueryTest, config);
			addFunctionCase(&*queryGroup, "verify_compatible_present_modes", "Verify compatible present modes have the same scaling capabilities", scalingQueryCompatibleModesTest, config);
			presentModeGroup->addChild(queryGroup.release());
		}

		for (size_t scalingFlagNdx = 0; scalingFlagNdx < DE_LENGTH_OF_ARRAY(scalingFlags); scalingFlagNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup>	scalingFlagGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), scalingFlags[scalingFlagNdx].name, scalingFlags[scalingFlagNdx].name));

			const bool isStretch = scalingFlags[scalingFlagNdx].scaling == VK_PRESENT_SCALING_STRETCH_BIT_EXT;

			for (size_t gravityFlagXNdx = 0; gravityFlagXNdx < DE_LENGTH_OF_ARRAY(gravityFlags); gravityFlagXNdx++)
			{
				for (size_t gravityFlagYNdx = 0; gravityFlagYNdx < DE_LENGTH_OF_ARRAY(gravityFlags); gravityFlagYNdx++)
				{
					std::string						testName	= gravityFlags[gravityFlagXNdx].name;
					testName									+= "_";
					testName									+= gravityFlags[gravityFlagYNdx].name;

					de::MovePtr<tcu::TestCaseGroup>	gravityFlagsGroup	(new tcu::TestCaseGroup(scalingFlagGroup->getTestContext(), testName.c_str(), testName.c_str()));

					ScalingTestConfig		config;
					config.wsiType			= wsiType;
					config.mode				= presentModes[presentModeNdx].mode;
					config.scaling			= scalingFlags[scalingFlagNdx].scaling;
					config.gravityX			= gravityFlags[gravityFlagXNdx].gravity;
					config.gravityY			= gravityFlags[gravityFlagYNdx].gravity;
					config.size				= SwapchainWindowSize::Identical;
					config.aspect			= SwapchainWindowAspect::Identical;
					config.resizeWindow		= resizeWindow;

					// Gravity does not apply to stretch
					de::MovePtr<tcu::TestCaseGroup> *group = isStretch ? &scalingFlagGroup : &gravityFlagsGroup;

					addFunctionCase(&**group, "same_size_and_aspect", "Basic test without actual scaling", scalingTest, config);

					config.size				= SwapchainWindowSize::SwapchainBigger;
					addFunctionCase(&**group, "swapchain_bigger_same_aspect", "Swapchain is bigger than window, but has same aspect", scalingTest, config);

					config.size				= SwapchainWindowSize::SwapchainSmaller;
					addFunctionCase(&**group, "swapchain_smaller_same_aspect", "Swapchain is smaller than window, but has same aspect", scalingTest, config);

					config.size				= SwapchainWindowSize::Identical;
					config.aspect			= SwapchainWindowAspect::SwapchainTaller;
					addFunctionCase(&**group, "swapchain_taller", "Swapchain has same width, but is taller than window", scalingTest, config);

					config.size				= SwapchainWindowSize::SwapchainBigger;
					addFunctionCase(&**group, "swapchain_bigger_taller_aspect", "Swapchain is bigger than window, and is taller in aspect ratio", scalingTest, config);

					config.size				= SwapchainWindowSize::SwapchainSmaller;
					addFunctionCase(&**group, "swapchain_smaller_taller_aspect", "Swapchain is smaller than window, but is taller in aspect ratio", scalingTest, config);

					config.size				= SwapchainWindowSize::Identical;
					config.aspect			= SwapchainWindowAspect::SwapchainWider;
					addFunctionCase(&**group, "swapchain_wider", "Swapchain has same height, but is wider than window", scalingTest, config);

					config.size				= SwapchainWindowSize::SwapchainBigger;
					addFunctionCase(&**group, "swapchain_bigger_wider_aspect", "Swapchain is bigger than window, and is wider in aspect ratio", scalingTest, config);

					config.size				= SwapchainWindowSize::SwapchainSmaller;
					addFunctionCase(&**group, "swapchain_smaller_wider_aspect", "Swapchain is smaller than window, but is wider in aspect ratio", scalingTest, config);

					if (isStretch)
					{
						break;
					}

					scalingFlagGroup->addChild(gravityFlagsGroup.release());
				}

				if (isStretch)
				{
					break;
				}
			}

			presentModeGroup->addChild(scalingFlagGroup.release());
		}

		testGroup->addChild(presentModeGroup.release());
	}
}

void populateScalingGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	populateScalingTests(testGroup, wsiType, false);

	de::MovePtr<tcu::TestCaseGroup>	resizeWindowGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), "resize_window", "Resize the window instead of creating the swapchain with a different size"));
	populateScalingTests(&*resizeWindowGroup, wsiType, true);
	testGroup->addChild(resizeWindowGroup.release());
}

void populateDeferredAllocGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	const struct
	{
		VkPresentModeKHR	mode;
		const char*			name;
	} presentModes[] =
	{
		{ VK_PRESENT_MODE_IMMEDIATE_KHR,					"immediate"		},
		{ VK_PRESENT_MODE_MAILBOX_KHR,						"mailbox"		},
		{ VK_PRESENT_MODE_FIFO_KHR,							"fifo"			},
		{ VK_PRESENT_MODE_FIFO_RELAXED_KHR,					"fifo_relaxed"	},
		{ VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,		"demand"		},
		{ VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,	"continuous"	},
	};

	for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup>	presentModeGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name, presentModes[presentModeNdx].name));

		PresentFenceTestConfig			config;
		config.wsiType					= wsiType;
		config.modes					= std::vector<VkPresentModeKHR>(1, presentModes[presentModeNdx].mode);
		config.deferMemoryAllocation	= true;
		config.bindImageMemory			= false;
		config.changePresentModes		= false;
		config.verifyFenceOrdering		= false;

		addFunctionCase(&*presentModeGroup, "basic", "Basic deferred allocation test", presentFenceTest, config);

		config.bindImageMemory			= true;

		// Bind image memory + shared present mode crashing on some drivers for unrelated reasons to VK_EXT_swapchain_maintenance1.  Will enable this test separately.
		if (presentModes[presentModeNdx].mode != VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR && presentModes[presentModeNdx].mode != VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
		{
			addFunctionCase(&*presentModeGroup, "bind_image", "Bind image with VkBindImageMemorySwapchainInfoKHR", presentFenceTest, config);
		}

		if (canDoMultiSwapchainPresent(wsiType))
		{
			config.modes					= std::vector<VkPresentModeKHR>(2, presentModes[presentModeNdx].mode);

			addFunctionCase(&*presentModeGroup, "bind_image_multi_swapchain", "Bind image with VkBindImageMemorySwapchainInfoKHR with multiple swapchains", presentFenceTest, config);
		}

		testGroup->addChild(presentModeGroup.release());
	}
}

enum class ResizeWindow
{
	No,
	BeforeAcquire,
	BeforePresent,
};

struct ReleaseImagesTestConfig
{
	vk::wsi::Type					wsiType;
	VkPresentModeKHR				mode;
	VkPresentScalingFlagsEXT		scaling;
	ResizeWindow					resizeWindow;
	bool							releaseBeforePresent;
	bool							releaseBeforeRetire;
};

tcu::TestStatus releaseImagesTest(Context& context, const ReleaseImagesTestConfig testParams)
{
	const InstanceHelper					instHelper		(context, testParams.wsiType, false);
	const TestNativeObjects					native			(context, instHelper.supportedExtensions, testParams.wsiType, 1);
	Unique<VkSurfaceKHR>					surface			(createSurface(instHelper.vki, instHelper.instance, testParams.wsiType, *native.display, *native.windows[0], context.getTestContext().getCommandLine()));

	const DeviceHelper						devHelper		(context, instHelper.vki, instHelper.instance, *surface, true, false);
	const DeviceInterface&					vkd				= devHelper.vkd;
	const VkDevice							device			= *devHelper.device;

	std::vector<VkSurfaceFormatKHR>			surfaceFormats	= getPhysicalDeviceSurfaceFormats(instHelper.vki, devHelper.physicalDevice, *surface);
	if(surfaceFormats.empty())
		return tcu::TestStatus::fail("No VkSurfaceFormatKHR defined");

	const VkSurfaceCapabilitiesKHR			capabilities	= getPerPresentSurfaceCapabilities(instHelper.vki, devHelper.physicalDevice, *surface, testParams.mode);
	const VkSurfaceTransformFlagBitsKHR		transform		= (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0 ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;

	const std::vector<VkPresentModeKHR>		presentModes	= getPhysicalDeviceSurfacePresentModes(instHelper.vki, devHelper.physicalDevice, *surface);
	if (std::find(presentModes.begin(), presentModes.end(), testParams.mode) == presentModes.end())
		TCU_THROW(NotSupportedError, "Present mode not supported");

	if (testParams.scaling != 0)
	{
		// Skip if configuration is not supported
		VkSurfacePresentScalingCapabilitiesEXT	scaling			= getSurfaceScalingCapabilities(instHelper.vki, devHelper.physicalDevice, testParams.mode, *surface);

		if ((scaling.supportedPresentScaling & testParams.scaling) == 0)
			TCU_THROW(NotSupportedError, "Scaling mode is not supported");
	}

	const bool isSharedPresentMode = testParams.mode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR || testParams.mode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
	if (isSharedPresentMode && (capabilities.minImageCount != 1 || capabilities.maxImageCount != 1))
	{
		return tcu::TestStatus::fail("min and max image count for shared present modes must be 1");
	}

	deUint32								imageCount		= capabilities.minImageCount + 10;
	if (capabilities.maxImageCount > 0)
		imageCount											= de::min(imageCount, capabilities.maxImageCount);

	VkSwapchainCreateInfoKHR				swapchainInfo	= getBasicSwapchainParameters(*surface, surfaceFormats[0], native.windowSize, testParams.mode, transform, imageCount, false);

	VkSwapchainPresentScalingCreateInfoEXT	scalingInfo		=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,
		DE_NULL,
		testParams.scaling,
		0,
		0,
	};
	swapchainInfo.pNext										= &scalingInfo;

	Move<VkSwapchainKHR>					swapchain		(createSwapchainKHR(vkd, device, &swapchainInfo));
	std::vector<VkImage>					swapchainImages	= getSwapchainImages(vkd, device, *swapchain);

	const Unique<VkCommandPool>				commandPool		(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, devHelper.queueFamilyIndex));

	const deUint32							iterations		= getIterations({testParams.mode}, {}, testParams.resizeWindow != ResizeWindow::No);

	// Do testParams.iterations presents, with a fence associated with the last one.
	FenceSp									presentFence	= FenceSp(new Unique<VkFence>(createFence(vkd, device)));
	const std::vector<SemaphoreSp>			acquireSems		(createSemaphores(vkd, device, iterations));
	const std::vector<SemaphoreSp>			presentSems		(createSemaphores(vkd, device, iterations));

	const std::vector<CommandBufferSp>		commandBuffers	(allocateCommandBuffers(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, iterations));

	const deUint64							foreverNs		= 0xFFFFFFFFFFFFFFFFul;

	VkImageSubresourceRange					range			=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		0,
		1,
		0,
		1,
	};

	const deUint32							configHash		=
		(deUint32)testParams.wsiType |
		(deUint32)testParams.mode << 4 |
		(deUint32)testParams.scaling << 24 |
		(deUint32)testParams.resizeWindow << 28 |
		(deUint32)testParams.releaseBeforePresent << 30 |
		(deUint32)testParams.releaseBeforeRetire << 31;
	de::Random								rng				(0x53A4C8A1u ^ configHash);

	try
	{
		for (deUint32 i = 0; i < iterations; ++i)
		{
			// Decide on how many acquires to do, and whether a presentation is to be done.  Presentation is always done for the last iteration, to facilitate clean up (by adding a present fence).
			const deUint32					maxAllowedAcquires	= (deUint32)swapchainImages.size() - capabilities.minImageCount + 1;
			const deUint32					acquireCount	= rng.getUint32() % maxAllowedAcquires + 1;
			const bool						doPresent		= i + 1 == iterations || rng.getUint32() % 10 != 0;
			const bool						doResize		= testParams.resizeWindow != ResizeWindow::No && rng.getUint32() % 10 != 0;
			const deUint32					presentIndex	= doPresent ? rng.getUint32() % acquireCount : acquireCount;

			// Resize the window if requested.
			if (doResize && testParams.resizeWindow == ResizeWindow::BeforeAcquire)
			{
				tcu::UVec2					windowSize		= native.windowSize;
				windowSize.x()								= windowSize.x() - 20 + rng.getUint32() % 41;
				windowSize.y()								= windowSize.y() - 20 + rng.getUint32() % 41;

				native.windows[0]->resize(windowSize);
			}

			// Acquire N times
			const VkSemaphore				presentSem		= **presentSems[i];
			const VkSemaphore				acquireSem		= **acquireSems[i];
			std::vector<deUint32>			acquiredIndices	(acquireCount, 0x12345);

			VkResult result = vkd.acquireNextImageKHR(device, *swapchain, foreverNs, presentIndex == 0 ? acquireSem : DE_NULL, DE_NULL, &acquiredIndices[0]);

			// If out of date, recreate the swapchain and reacquire.
			if (result == VK_ERROR_OUT_OF_DATE_KHR)
			{
				swapchainInfo.oldSwapchain					= *swapchain;
				Move<VkSwapchainKHR>		newSwapchain	(createSwapchainKHR(vkd, device, &swapchainInfo));
				swapchain									= std::move(newSwapchain);

				const size_t previousImageCount				= swapchainImages.size();
				swapchainImages								= getSwapchainImages(vkd, device, *swapchain);
				if (previousImageCount != swapchainImages.size())
					TCU_THROW(InternalError, "Unexpected change in number of swapchain images when recreated during window resize");

				result = vkd.acquireNextImageKHR(device, *swapchain, foreverNs, presentIndex == 0 ? acquireSem : DE_NULL, DE_NULL, &acquiredIndices[0]);
			}

			VK_CHECK_WSI(result);

			for (deUint32 j = 1; j < acquireCount; ++j)
			{
				VK_CHECK_WSI(vkd.acquireNextImageKHR(device, *swapchain, foreverNs, presentIndex == j ? acquireSem : DE_NULL, DE_NULL, &acquiredIndices[j]));
			}

			// Construct a list of image indices to be released.  That is every index except the one being presented, if any.
			std::vector<deUint32>			releaseIndices	= acquiredIndices;
			if (doPresent)
			{
				releaseIndices.erase(releaseIndices.begin() + presentIndex);
			}

			// Randomize the indices to be released.
			rng.shuffle(releaseIndices.begin(), releaseIndices.end());

			if (doResize && testParams.resizeWindow == ResizeWindow::BeforePresent)
			{
				tcu::UVec2					windowSize		= native.windowSize;
				windowSize.x()								= windowSize.x() - 20 + rng.getUint32() % 41;
				windowSize.y()								= windowSize.y() - 20 + rng.getUint32() % 41;

				native.windows[0]->resize(windowSize);
			}

			if (doPresent)
			{
				beginCommandBuffer(vkd, **commandBuffers[i], 0u);

				VkImageMemoryBarrier barrier = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					DE_NULL,
					0,
					0,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED,
					VK_QUEUE_FAMILY_IGNORED,
					swapchainImages[acquiredIndices[presentIndex]],
					range,
				};

				VkClearColorValue				clearValue;
				clearValue.float32[0]			= static_cast<float>(i % 33) / 32.0f;
				clearValue.float32[1]			= static_cast<float>((i + 7) % 33) / 32.0f;
				clearValue.float32[2]			= static_cast<float>((i + 17) % 33) / 32.0f;
				clearValue.float32[3]			= 1.0f;

				vkd.cmdClearColorImage(**commandBuffers[i], swapchainImages[acquiredIndices[presentIndex]], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				vkd.cmdPipelineBarrier(**commandBuffers[i],
						VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						0u,
						0, DE_NULL,
						0, DE_NULL,
						1, &barrier);

				endCommandBuffer(vkd, **commandBuffers[i]);

				// Submit the command buffer
				VkPipelineStageFlags			waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				const VkSubmitInfo				submitInfo =
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,
					DE_NULL,
					1,
					&acquireSem,
					&waitStage,
					1u,
					&**commandBuffers[i],
					1u,
					&presentSem,
				};
				VK_CHECK(vkd.queueSubmit(devHelper.queue, 1u, &submitInfo, DE_NULL));
			}

			// If asked to release before present, do so now.
			const VkReleaseSwapchainImagesInfoEXT	releaseInfo =
			{
				VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT,
				DE_NULL,
				*swapchain,
				(deUint32)releaseIndices.size(),
				releaseIndices.data(),
			};

			bool imagesReleased = false;
			if (testParams.releaseBeforePresent)
			{
				VK_CHECK(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
				imagesReleased = true;
			}

			// Present the frame
			if (doPresent)
			{
				const VkSwapchainPresentFenceInfoEXT presentFenceInfo =
				{
					VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
					DE_NULL,
					1,
					&**presentFence,
				};

				const VkPresentInfoKHR presentInfo =
				{
					VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					// Signal the present fence on the last present.
					i + 1 == iterations ? &presentFenceInfo : nullptr,
					1u,
					&presentSem,
					1,
					&*swapchain,
					&acquiredIndices[presentIndex],
					&result,
				};
				VkResult aggregateResult = vkd.queuePresentKHR(devHelper.queue, &presentInfo);
				if (aggregateResult == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
				{
					// If OUT_OF_DATE is returned from present, recreate the swapchain and release images to the retired swapchain.
					if (!imagesReleased && testParams.releaseBeforeRetire)
					{
						VK_CHECK(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
						imagesReleased = true;
					}

					swapchainInfo.oldSwapchain				= *swapchain;
					Move<VkSwapchainKHR>	newSwapchain	(createSwapchainKHR(vkd, device, &swapchainInfo));

					if (!imagesReleased && !testParams.releaseBeforeRetire)
					{
						// Release the images to the retired swapchain before deleting it (as part of move assignment below)
						VK_CHECK(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
						imagesReleased = true;
					}

					// Must have released old swapchain's images before destruction
					DE_ASSERT(imagesReleased);
					swapchain								= std::move(newSwapchain);

					const size_t previousImageCount			= swapchainImages.size();
					swapchainImages							= getSwapchainImages(vkd, device, *swapchain);
					if (previousImageCount != swapchainImages.size())
						TCU_THROW(InternalError, "Unexpected change in number of swapchain images when recreated during window resize");
				}
				else
				{
					VK_CHECK_WSI(result);
					VK_CHECK_WSI(result);
				}
			}

			// If asked to release after present, do it now.
			if (!imagesReleased)
			{
				VK_CHECK_WSI(vkd.releaseSwapchainImagesEXT(device, &releaseInfo));
			}
		}

		// Wait for all presents before terminating the test (when semaphores are destroyed)
		VK_CHECK(vkd.waitForFences(device, 1u, &**presentFence, VK_TRUE, kMaxFenceWaitTimeout));
	}
	catch (...)
	{
		// Make sure device is idle before destroying resources
		vkd.deviceWaitIdle(device);
		throw;
	}

	native.windows[0]->setVisible(false);

	return tcu::TestStatus::pass("Tests ran successfully");
}

void populateReleaseImagesGroup (tcu::TestCaseGroup* testGroup, Type wsiType)
{
	const struct
	{
		VkPresentModeKHR	mode;
		const char*			name;
	} presentModes[] =
	{
		{ VK_PRESENT_MODE_IMMEDIATE_KHR,					"immediate"		},
		{ VK_PRESENT_MODE_MAILBOX_KHR,						"mailbox"		},
		{ VK_PRESENT_MODE_FIFO_KHR,							"fifo"			},
		{ VK_PRESENT_MODE_FIFO_RELAXED_KHR,					"fifo_relaxed"	},
		{ VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR,		"demand"		},
		{ VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR,	"continuous"	},
	};

	const struct
	{
		VkPresentScalingFlagsEXT		scaling;
		const char*						name;
	} scalingFlags[] =
	{
		{ 0,												"no_scaling"		},
		{ VK_PRESENT_SCALING_STRETCH_BIT_EXT,				"stretch"			},
	};

	for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup>	presentModeGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name, presentModes[presentModeNdx].name));

		for (size_t scalingFlagNdx = 0; scalingFlagNdx < DE_LENGTH_OF_ARRAY(scalingFlags); scalingFlagNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup>	scalingFlagGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), scalingFlags[scalingFlagNdx].name, scalingFlags[scalingFlagNdx].name));

			ReleaseImagesTestConfig			config;
			config.wsiType					= wsiType;
			config.mode						= presentModes[presentModeNdx].mode;
			config.scaling					= scalingFlags[scalingFlagNdx].scaling;
			config.resizeWindow				= ResizeWindow::No;
			config.releaseBeforePresent		= false;
			config.releaseBeforeRetire		= false;

			addFunctionCase(&*scalingFlagGroup, "basic", "Basic release acquired images test", releaseImagesTest, config);

			config.releaseBeforePresent		= true;
			addFunctionCase(&*scalingFlagGroup, "release_before_present", "Basic release acquired images test where release happens before presenting an image", releaseImagesTest, config);

			config.releaseBeforePresent		= false;
			config.resizeWindow				= ResizeWindow::BeforeAcquire;
			addFunctionCase(&*scalingFlagGroup, "resize_window", "Release acquired images after a window resize before acquire", releaseImagesTest, config);

			config.resizeWindow				= ResizeWindow::BeforePresent;
			addFunctionCase(&*scalingFlagGroup, "resize_window_after_acquire", "Release acquired images after a window resize after acquire", releaseImagesTest, config);

			config.releaseBeforeRetire		= true;
			addFunctionCase(&*scalingFlagGroup, "resize_window_after_acquire_release_before_retire", "Release acquired images after a window resize after acquire, but release the images before retiring the swapchain", releaseImagesTest, config);

			presentModeGroup->addChild(scalingFlagGroup.release());
		}

		testGroup->addChild(presentModeGroup.release());
	}
}

} // anonymous

void createMaintenance1Tests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addTestGroup(testGroup, "present_fence",			"Present fence",				populatePresentFenceGroup,	wsiType);
	addTestGroup(testGroup, "present_modes",			"Change present modes",			populatePresentModesGroup,	wsiType);
	addTestGroup(testGroup, "scaling",					"Scaling and gravity",			populateScalingGroup,		wsiType);
	addTestGroup(testGroup, "deferred_alloc",			"Deferred allocation",			populateDeferredAllocGroup,	wsiType);
	addTestGroup(testGroup, "release_images",			"Release acquired images",		populateReleaseImagesGroup,	wsiType);
}

} // wsi

} // vkt
