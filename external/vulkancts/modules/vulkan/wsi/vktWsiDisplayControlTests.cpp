/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief VK_EXT_display_control tests
 *//*--------------------------------------------------------------------*/

#include "vkRefUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkWsiUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktWsiDisplayControlTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deClock.h"

#include <vector>
#include <string>

using std::vector;
using std::string;

using tcu::Maybe;
using tcu::UVec2;
using tcu::TestLog;

namespace vkt
{
namespace wsi
{
namespace
{

using namespace vk;
using namespace vk::wsi;

typedef vector<VkExtensionProperties> Extensions;

CustomInstance createInstance (Context& context)
{
	vector<string> extensions =
	{
		"VK_KHR_surface",
		"VK_KHR_display",
		"VK_EXT_display_surface_counter",
	};

	return vkt::createCustomInstanceWithExtensions(context, extensions);
}

deUint32 chooseQueueFamilyIndex (const InstanceInterface& vki, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	deUint32 numTotalFamilyIndices;
	vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &numTotalFamilyIndices, DE_NULL);

	for (deUint32 queueFamilyNdx = 0; queueFamilyNdx < numTotalFamilyIndices; ++queueFamilyNdx)
	{
		if (wsi::getPhysicalDeviceSurfaceSupport(vki, physicalDevice, queueFamilyNdx, surface) == VK_TRUE)
			return queueFamilyNdx;
	}

	TCU_THROW(NotSupportedError, "Device doesn't support presentation");
	return 0;
}

Move<VkDevice> createTestDevice (const vk::Platform&			platform,
								 const PlatformInterface&		vkp,
								 const VkInstance				instance,
								 const InstanceInterface&		vki,
								 VkPhysicalDevice				physicalDevice,
								 const Extensions&				supportedExtensions,
								 const deUint32					queueFamilyIndex,
								 bool							validationEnabled,
								 const VkAllocationCallbacks*	pAllocator = DE_NULL)
{
	const float queuePriorities[] = { 1.0f };
	bool displayAvailable = true;
	const VkDeviceQueueCreateInfo queueInfos[] =
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

	VkPhysicalDeviceFeatures features;
	deMemset(&features, 0, sizeof(features));

	const char* extensions[] =
	{
		"VK_KHR_swapchain",
		"VK_EXT_display_control"
	};

	const VkDeviceCreateInfo deviceParams =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		DE_NULL,
		(VkDeviceCreateFlags)0,
		DE_LENGTH_OF_ARRAY(queueInfos),
		&queueInfos[0],
		0u,
		DE_NULL,
		DE_LENGTH_OF_ARRAY(extensions),
		&extensions[0],
		&features
	};

	for (auto ext: extensions)
	{
		if (!isExtensionSupported(supportedExtensions, RequiredExtension(ext)))
			TCU_THROW(NotSupportedError, (string(ext) + " is not supported").c_str());
	}

	for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
	{
		vk::wsi::Type	wsiType = (vk::wsi::Type)typeNdx;
		if (platform.hasDisplay(wsiType))
		{
			displayAvailable = false;
			break;
		}
	}

	if (!displayAvailable)
		TCU_THROW(NotSupportedError, "Display is unavailable as windowing system has access");

	return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

VkDisplayKHR getDisplayAndDisplayPlane(const InstanceInterface& vki, VkPhysicalDevice physicalDevice, deUint32 *pPlaneIndex)
{
	deUint32 countDisplays = 0;
	VkResult result = vki.getPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &countDisplays, DE_NULL);
	if (result != VK_SUCCESS)
		TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceDisplayPropertiesKHR failed");

	if (countDisplays == 0)
		TCU_THROW(NotSupportedError, "No displays available");

	deUint32 countDisplayPlanes = 0;
	result = vki.getPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &countDisplayPlanes, DE_NULL);
	if (result != VK_SUCCESS || !countDisplayPlanes)
		TCU_FAIL("GetPhysicalDeviceDisplayPlanePropertiesKHR failed");

	for (deUint32 p = 0; p < countDisplayPlanes; p++)
	{
		deUint32 count = 0u;
		result = vki.getDisplayPlaneSupportedDisplaysKHR(physicalDevice, p, &count, DE_NULL);
		if (result != VK_SUCCESS)
			TCU_FAIL("GetDisplayPlaneSupportedDisplaysKHR failed");

		// No displays that can make use of this plane are available.
		if (!count)
			continue;

		std::vector<VkDisplayKHR> displays(count);
		result = vki.getDisplayPlaneSupportedDisplaysKHR(physicalDevice, p, &count, &displays[0]);
		if (result != VK_SUCCESS)
			TCU_FAIL("GetDisplayPlaneSupportedDisplaysKHR failed");

		// return first plane with an available display
		*pPlaneIndex = p;
		return displays[0];
	}

	TCU_FAIL("No intersection between displays and display planes");

	// Unreachable.
	return DE_NULL;
}

VkSurfaceKHR createSurface(const InstanceInterface& vki, VkInstance instance, VkPhysicalDevice physicalDevice, VkDisplayKHR display, deUint32 planeIndex)
{
	// get number of display modes for this display
	deUint32 displayModesCount = 0;
	VkResult result = vki.getDisplayModePropertiesKHR(physicalDevice, display, &displayModesCount, DE_NULL);
	if (result != VK_SUCCESS)
		TCU_FAIL("GetDisplayModePropertiesKHR failed");

	// get first display mode of this display
	std::vector<vk::VkDisplayModePropertiesKHR> modeProperties(displayModesCount);
	result = vki.getDisplayModePropertiesKHR(physicalDevice, display, &displayModesCount, &modeProperties[0]);
	if (result != VK_SUCCESS)
		TCU_FAIL("GetDisplayModePropertiesKHR failed");
	VkDisplayModeKHR displayMode = modeProperties[0].displayMode;

	// get capabielieties for first plane of this display
	VkDisplayPlaneCapabilitiesKHR planeCapabilities;
	result = vki.getDisplayPlaneCapabilitiesKHR(physicalDevice, displayMode, planeIndex, &planeCapabilities);
	if (result != VK_SUCCESS)
		TCU_FAIL("GetDisplayPlaneCapabilitiesKHR failed");

	// get plane properties count
	deUint32 planePropertiesCount = 0;
	result = vki.getPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropertiesCount, DE_NULL);
	if (result != VK_SUCCESS || !planePropertiesCount)
		TCU_FAIL("GetPhysicalDeviceDisplayPlanePropertiesKHR failed");

	// get plane properties
	std::vector <VkDisplayPlanePropertiesKHR> planeProperties(planePropertiesCount);
	result = vki.getPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropertiesCount, &planeProperties[0]);
	if (result != VK_SUCCESS)
		TCU_FAIL("GetPhysicalDeviceDisplayPlanePropertiesKHR failed");

	// define surface create info
	const VkDisplaySurfaceCreateInfoKHR createInfo =
	{
		VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,	// VkStructureType					sType
		DE_NULL,											// const void*						pNext
		0,													// VkDisplaySurfaceCreateFlagsKHR	flags
		displayMode,										// VkDisplayModeKHR					displayMode
		planeIndex,											// uint32_t							planeIndex
		planeProperties[planeIndex].currentStackIndex,		// uint32_t							planeStackIndex
		VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,				// VkSurfaceTransformFlagBitsKHR	transform
		1.0f,												// float							globalAlpha
		VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,				// VkDisplayPlaneAlphaFlagBitsKHR	alphaMode
		{													// VkExtent2D						imageExtent
			planeCapabilities.minDstExtent.width,
			planeCapabilities.minDstExtent.height
		}
	};

	VkSurfaceKHR surface = DE_NULL;
	result = vki.createDisplayPlaneSurfaceKHR(instance, &createInfo, DE_NULL, &surface);
	if (result != VK_SUCCESS)
		TCU_FAIL("CreateDisplayPlaneSurfaceKHR failed");

	if (surface == DE_NULL)
		TCU_FAIL("Invalid surface handle returned");

	return surface;
}

void initSemaphores (const DeviceInterface&		vkd,
					 VkDevice					device,
					 std::vector<VkSemaphore>&	semaphores)
{
	for (VkSemaphore& semaphore : semaphores)
		semaphore = createSemaphore(vkd, device).disown();
}

void deinitSemaphores (const DeviceInterface&	vkd,
					 VkDevice					device,
					 std::vector<VkSemaphore>&	semaphores)
{
	for (VkSemaphore& semaphore : semaphores)
	{
		if (semaphore == (VkSemaphore)0)
			continue;

		vkd.destroySemaphore(device, semaphore, DE_NULL);
		semaphore = (VkSemaphore)0;
	}

	semaphores.clear();
}

void initFences (const DeviceInterface&	vkd,
				 VkDevice				device,
				 std::vector<VkFence>&	fences)
{
	for (VkFence& fence : fences)
		fence = createFence(vkd, device).disown();
}

void deinitFences (const DeviceInterface&	vkd,
				   VkDevice					device,
				   std::vector<VkFence>&	fences)
{
	for (VkFence& fence : fences)
	{
		if (fence == (VkFence)0)
			continue;

		vkd.destroyFence(device, fence, DE_NULL);
		fence = (VkFence)0;
	}

	fences.clear();
}

Move<VkCommandBuffer> createCommandBuffer (const DeviceInterface&	vkd,
										   VkDevice					device,
										   VkCommandPool			commandPool,
										   VkRenderPass				renderPass,
										   VkImage					image,
										   VkFramebuffer			framebuffer,
										   VkPipeline				pipeline,
										   deUint32					imageWidth,
										   deUint32					imageHeight)
{
	const VkCommandBufferAllocateInfo allocateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		DE_NULL,

		commandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};

	VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkAccessFlags			srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
		image,										// VkImage					image;
		{											// VkImageSubresourceRange	subresourceRange;
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags		aspectMask;
			0u,										// deUint32					baseMipLevel;
			1u,										// deUint32					mipLevels;
			0u,										// deUint32					baseArraySlice;
			1u										// deUint32					arraySize;
		}
	};

	Move<VkCommandBuffer>	commandBuffer	(allocateCommandBuffer(vkd, device, &allocateInfo));
	beginCommandBuffer(vkd, *commandBuffer, 0u);

	vkd.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		(VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrier);

	beginRenderPass(vkd, *commandBuffer, renderPass, framebuffer, makeRect2D(0, 0, imageWidth, imageHeight), tcu::Vec4(0.25f, 0.5f, 0.75f, 1.0f));

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);

	endRenderPass(vkd, *commandBuffer);

	endCommandBuffer(vkd, *commandBuffer);
	return commandBuffer;
}

void deinitCommandBuffers (const DeviceInterface&			vkd,
						   VkDevice							device,
						   VkCommandPool					commandPool,
						   std::vector<VkCommandBuffer>&	commandBuffers)
{
	for (size_t ndx = 0; ndx < commandBuffers.size(); ndx++)
	{
		if (commandBuffers[ndx] != (VkCommandBuffer)0)
			vkd.freeCommandBuffers(device, commandPool, 1u,  &commandBuffers[ndx]);

		commandBuffers[ndx] = (VkCommandBuffer)0;
	}

	commandBuffers.clear();
}

Move<VkCommandPool> createCommandPool (const DeviceInterface&	vkd,
									   VkDevice					device,
									   deUint32					queueFamilyIndex)
{
	const VkCommandPoolCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		DE_NULL,
		0u,
		queueFamilyIndex
	};

	return createCommandPool(vkd, device, &createInfo);
}

void initFramebuffers (const DeviceInterface&		vkd,
					   VkDevice						device,
					   VkRenderPass					renderPass,
					   std::vector<VkImageView>		imageViews,
					   deUint32						width,
					   deUint32						height,
					   std::vector<VkFramebuffer>&	framebuffers)
{
	DE_ASSERT(framebuffers.size() == imageViews.size());

	for (size_t ndx = 0; ndx < framebuffers.size(); ndx++)
	{
		const VkFramebufferCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			DE_NULL,

			0u,
			renderPass,
			1u,
			&imageViews[ndx],
			width,
			height,
			1u
		};

		framebuffers[ndx] = createFramebuffer(vkd, device, &createInfo).disown();
	}
}

void deinitFramebuffers (const DeviceInterface&			vkd,
						 VkDevice						device,
						 std::vector<VkFramebuffer>&	framebuffers)
{
	for (size_t ndx = 0; ndx < framebuffers.size(); ndx++)
	{
		if (framebuffers[ndx] != (VkFramebuffer)0)
			vkd.destroyFramebuffer(device, framebuffers[ndx], DE_NULL);

		framebuffers[ndx] = (VkFramebuffer)0;
	}

	framebuffers.clear();
}

Move<VkImageView> createImageView (const DeviceInterface&	vkd,
								   VkDevice					device,
								   VkImage					image,
								   VkFormat					format)
{
	const VkImageViewCreateInfo	createInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		DE_NULL,

		0u,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		format,
		makeComponentMappingRGBA(),
		{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0u,
			1u,
			0u,
			1u
		}
	};

	return createImageView(vkd, device, &createInfo, DE_NULL);
}

void initImageViews (const DeviceInterface&			vkd,
					 VkDevice						device,
					 const std::vector<VkImage>&	images,
					 VkFormat						format,
					 std::vector<VkImageView>&		imageViews)
{
	DE_ASSERT(images.size() == imageViews.size());

	for (size_t ndx = 0; ndx < imageViews.size(); ndx++)
		imageViews[ndx] = createImageView(vkd, device, images[ndx], format).disown();
}

void deinitImageViews (const DeviceInterface&		vkd,
					   VkDevice						device,
					   std::vector<VkImageView>&	imageViews)
{
	for (size_t ndx = 0; ndx < imageViews.size(); ndx++)
	{
		if (imageViews[ndx] != (VkImageView)0)
			vkd.destroyImageView(device, imageViews[ndx], DE_NULL);

		imageViews[ndx] = (VkImageView)0;
	}

	imageViews.clear();
}

Move<VkPipeline> createPipeline (const DeviceInterface&	vkd,
								 VkDevice				device,
								 VkRenderPass			renderPass,
								 VkPipelineLayout		layout,
								 VkShaderModule			vertexShaderModule,
								 VkShaderModule			fragmentShaderModule,
								 deUint32				width,
								 deUint32				height)
{
	const VkPipelineVertexInputStateCreateInfo	vertexInputState	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		DE_NULL,
		0u,
		0u,
		DE_NULL,
		0u,
		DE_NULL
	};
	const std::vector<VkViewport>	viewports	(1, makeViewport(tcu::UVec2(width, height)));
	const std::vector<VkRect2D>		scissors	(1, makeRect2D(tcu::UVec2(width, height)));

	return makeGraphicsPipeline(vkd,										// const DeviceInterface&                        vk
								device,										// const VkDevice                                device
								layout,										// const VkPipelineLayout                        pipelineLayout
								vertexShaderModule,							// const VkShaderModule                          vertexShaderModule
								DE_NULL,									// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,									// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,									// const VkShaderModule                          geometryShaderModule
								fragmentShaderModule,						// const VkShaderModule                          fragmentShaderModule
								renderPass,									// const VkRenderPass                            renderPass
								viewports,									// const std::vector<VkViewport>&                viewports
								scissors,									// const std::vector<VkRect2D>&                  scissors
								VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,		// const VkPrimitiveTopology                     topology
								0u,											// const deUint32                                subpass
								0u,											// const deUint32                                patchControlPoints
								&vertexInputState);							// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
}

Move<VkPipelineLayout> createPipelineLayout (const DeviceInterface&	vkd,
												   VkDevice			device)
{
	const VkPipelineLayoutCreateInfo createInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		0u,
		0u,
		DE_NULL,
		0u,
		DE_NULL,
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

VkSwapchainCounterCreateInfoEXT createSwapchainCounterConfig()
{
	const VkSwapchainCounterCreateInfoEXT swapchainCounterConfig =
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT,
		DE_NULL,
		VK_SURFACE_COUNTER_VBLANK_EXT
	};
	return swapchainCounterConfig;
}

VkSwapchainCreateInfoKHR createSwapchainConfig (VkSurfaceKHR						surface,
												deUint32							queueFamilyIndex,
												const VkSurfaceCapabilities2EXT&	properties,
												const vector<VkSurfaceFormatKHR>&	formats,
												const vector<VkPresentModeKHR>&		presentModes,
												VkPresentModeKHR					presentMode,
												VkSwapchainCounterCreateInfoEXT *swapchainCounterInfo)
{
	if ((properties.supportedSurfaceCounters & VK_SURFACE_COUNTER_VBLANK_EXT) == 0)
		TCU_THROW(NotSupportedError, "vblank counter not supported");

	const deUint32				imageLayers		= 1u;
	const VkImageUsageFlags		imageUsage		= properties.supportedUsageFlags;
	const VkBool32				clipped			= VK_FALSE;

	const deUint32				imageWidth		= (properties.currentExtent.width != 0xFFFFFFFFu)
													? properties.currentExtent.width
													: de::min(1024u, properties.minImageExtent.width + ((properties.maxImageExtent.width - properties.minImageExtent.width) / 2));
	const deUint32				imageHeight		= (properties.currentExtent.height != 0xFFFFFFFFu)
													? properties.currentExtent.height
													: de::min(1024u, properties.minImageExtent.height + ((properties.maxImageExtent.height - properties.minImageExtent.height) / 2));
	const VkExtent2D			imageSize		= { imageWidth, imageHeight };

	if (std::find(presentModes.begin(), presentModes.end(), presentMode) == presentModes.end())
		TCU_THROW(NotSupportedError, "Present mode not supported");

	// Pick the first supported transform, alpha, and format:
	VkSurfaceTransformFlagsKHR transform;
	for (transform = 1u; transform <= properties.supportedTransforms; transform = transform << 1u)
	{
		if ((properties.supportedTransforms & transform) != 0)
			break;
	}

	VkCompositeAlphaFlagsKHR alpha;
	for (alpha = 1u; alpha <= properties.supportedCompositeAlpha; alpha = alpha << 1u)
	{
		if ((alpha & properties.supportedCompositeAlpha) != 0)
			break;
	}

	{
		const VkSurfaceTransformFlagBitsKHR	preTransform	= (VkSurfaceTransformFlagBitsKHR)transform;
		const VkCompositeAlphaFlagBitsKHR	compositeAlpha	= (VkCompositeAlphaFlagBitsKHR)alpha;
		const VkFormat						imageFormat		= formats[0].format;
		const VkColorSpaceKHR				imageColorSpace	= formats[0].colorSpace;
		const VkSwapchainCreateInfoKHR		createInfo		=
		{
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			swapchainCounterInfo,
			0u,
			surface,
			properties.minImageCount,
			imageFormat,
			imageColorSpace,
			imageSize,
			imageLayers,
			imageUsage,
			VK_SHARING_MODE_EXCLUSIVE,
			1u,
			&queueFamilyIndex,
			preTransform,
			compositeAlpha,
			presentMode,
			clipped,
			(VkSwapchainKHR)0
		};

		return createInfo;
	}
}

class SwapchainCounterTestInstance : public TestInstance
{
public:
						SwapchainCounterTestInstance	(Context& context);
						~SwapchainCounterTestInstance	(void);

	tcu::TestStatus		iterate							(void);

private:
	void				initSwapchainResources			(void);
	void				deinitSwapchainResources		(void);
	void				render							(void);

private:
	const PlatformInterface&			m_vkp;
	const CustomInstance				m_instance;
	const InstanceDriver&				m_vki;
	const VkPhysicalDevice				m_physicalDevice;
	deUint32							m_planeIndex;
	const VkDisplayKHR					m_display;
	const VkSurfaceKHR					m_surface;

	const deUint32						m_queueFamilyIndex;
	const Extensions					m_deviceExtensions;
	const Unique<VkDevice>				m_device;
	const DeviceDriver					m_vkd;
	const VkQueue						m_queue;

	const Unique<VkCommandPool>			m_commandPool;
	const Unique<VkShaderModule>		m_vertexShaderModule;
	const Unique<VkShaderModule>		m_fragmentShaderModule;
	const Unique<VkPipelineLayout>		m_pipelineLayout;

	const VkSurfaceCapabilities2EXT		m_surfaceProperties;
	const vector<VkSurfaceFormatKHR>	m_surfaceFormats;
	const vector<VkPresentModeKHR>		m_presentModes;

	tcu::ResultCollector				m_resultCollector;

	Move<VkSwapchainKHR>				m_swapchain;
	std::vector<VkImage>				m_swapchainImages;

	Move<VkRenderPass>					m_renderPass;
	Move<VkPipeline>					m_pipeline;

	std::vector<VkImageView>			m_swapchainImageViews;
	std::vector<VkFramebuffer>			m_framebuffers;
	std::vector<VkCommandBuffer>		m_commandBuffers;
	std::vector<VkSemaphore>			m_acquireSemaphores;
	std::vector<VkSemaphore>			m_renderSemaphores;
	std::vector<VkFence>				m_fences;

	VkSwapchainCounterCreateInfoEXT		m_swapchainCounterConfig;
	VkSwapchainCreateInfoKHR			m_swapchainConfig;

	const size_t						m_frameCount;
	size_t								m_frameNdx;

	const size_t						m_maxOutOfDateCount;
	size_t								m_outOfDateCount;
};

SwapchainCounterTestInstance::SwapchainCounterTestInstance (Context& context)
	: TestInstance				(context)
	, m_vkp						(context.getPlatformInterface())
	, m_instance				(createInstance(context))
	, m_vki						(m_instance.getDriver())
	, m_physicalDevice			(chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
	, m_planeIndex				(0)
	, m_display					(getDisplayAndDisplayPlane(m_vki, m_physicalDevice, &m_planeIndex))
	, m_surface					(createSurface(m_vki, m_instance, m_physicalDevice, m_display, m_planeIndex))

	, m_queueFamilyIndex		(chooseQueueFamilyIndex(m_vki, m_physicalDevice, m_surface))
	, m_deviceExtensions		(enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
	, m_device					(createTestDevice(context.getTestContext().getPlatform().getVulkanPlatform(), m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex, context.getTestContext().getCommandLine().isValidationEnabled()))
	, m_vkd						(m_vkp, m_instance, *m_device)
	, m_queue					(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))

	, m_commandPool				(createCommandPool(m_vkd, *m_device, m_queueFamilyIndex))
	, m_vertexShaderModule		(createShaderModule(m_vkd, *m_device, context.getBinaryCollection().get("quad-vert"), 0u))
	, m_fragmentShaderModule	(createShaderModule(m_vkd, *m_device, context.getBinaryCollection().get("quad-frag"), 0u))
	, m_pipelineLayout			(createPipelineLayout(m_vkd, *m_device))

	, m_surfaceProperties		(wsi::getPhysicalDeviceSurfaceCapabilities2EXT(m_vki, m_physicalDevice, m_surface))
	, m_surfaceFormats			(wsi::getPhysicalDeviceSurfaceFormats(m_vki, m_physicalDevice, m_surface))
	, m_presentModes			(wsi::getPhysicalDeviceSurfacePresentModes(m_vki, m_physicalDevice, m_surface))

	, m_swapchainCounterConfig	(createSwapchainCounterConfig())
	, m_swapchainConfig			(createSwapchainConfig(m_surface, m_queueFamilyIndex, m_surfaceProperties, m_surfaceFormats, m_presentModes, VK_PRESENT_MODE_FIFO_KHR, &m_swapchainCounterConfig))

	, m_frameCount				(20u)
	, m_frameNdx				(0u)

	, m_maxOutOfDateCount		(10u)
	, m_outOfDateCount			(0u)
{
}

SwapchainCounterTestInstance::~SwapchainCounterTestInstance (void)
{
	deinitSwapchainResources();

	m_vki.destroySurfaceKHR(m_instance, m_surface, DE_NULL);
}

void SwapchainCounterTestInstance::initSwapchainResources (void)
{
	const deUint32		imageWidth	= m_swapchainConfig.imageExtent.width;
	const deUint32		imageHeight	= m_swapchainConfig.imageExtent.height;
	const VkFormat		imageFormat	= m_swapchainConfig.imageFormat;

	m_swapchain			= createSwapchainKHR(m_vkd, *m_device, &m_swapchainConfig);
	m_swapchainImages	= wsi::getSwapchainImages(m_vkd, *m_device, *m_swapchain);

	m_renderPass		= makeRenderPass(m_vkd, *m_device, imageFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	m_pipeline			= createPipeline(m_vkd, *m_device, *m_renderPass, *m_pipelineLayout, *m_vertexShaderModule, *m_fragmentShaderModule, imageWidth, imageHeight);

	const size_t swapchainImagesCount	= m_swapchainImages.size();
	const size_t fenceCount				= swapchainImagesCount * 2;

	m_swapchainImageViews	= std::vector<VkImageView>(swapchainImagesCount, (VkImageView)0);
	m_framebuffers			= std::vector<VkFramebuffer>(swapchainImagesCount, (VkFramebuffer)0);
	m_acquireSemaphores		= std::vector<VkSemaphore>(swapchainImagesCount+1, (VkSemaphore)0);
	m_renderSemaphores		= std::vector<VkSemaphore>(swapchainImagesCount+1, (VkSemaphore)0);

	m_fences				= std::vector<VkFence>(fenceCount, (VkFence)0);
	m_commandBuffers		= std::vector<VkCommandBuffer>(fenceCount, (VkCommandBuffer)0);

	initImageViews(m_vkd, *m_device, m_swapchainImages, imageFormat, m_swapchainImageViews);
	initFramebuffers(m_vkd, *m_device, *m_renderPass, m_swapchainImageViews, imageWidth, imageHeight, m_framebuffers);
	initSemaphores(m_vkd, *m_device, m_acquireSemaphores);
	initSemaphores(m_vkd, *m_device, m_renderSemaphores);

	initFences(m_vkd, *m_device, m_fences);
}

void SwapchainCounterTestInstance::deinitSwapchainResources (void)
{
	VK_CHECK(m_vkd.queueWaitIdle(m_queue));

	deinitSemaphores(m_vkd, *m_device, m_acquireSemaphores);
	deinitSemaphores(m_vkd, *m_device, m_renderSemaphores);
	deinitFences(m_vkd, *m_device, m_fences);
	deinitCommandBuffers(m_vkd, *m_device, *m_commandPool, m_commandBuffers);
	deinitFramebuffers(m_vkd, *m_device, m_framebuffers);
	deinitImageViews(m_vkd, *m_device, m_swapchainImageViews);

	m_swapchainImages.clear();

	m_swapchain		= Move<VkSwapchainKHR>();
	m_renderPass	= Move<VkRenderPass>();
	m_pipeline		= Move<VkPipeline>();
}

void SwapchainCounterTestInstance::render (void)
{
	const deUint64		foreverNs		= ~0x0ull;
	VkCommandBuffer&	commandBuffer	= m_commandBuffers[m_frameNdx % m_commandBuffers.size()];
	const VkFence		fence			= m_fences[m_frameNdx % m_fences.size()];
	const deUint32		width			= m_swapchainConfig.imageExtent.width;
	const deUint32		height			= m_swapchainConfig.imageExtent.height;

	if (m_frameNdx >= m_fences.size())
		VK_CHECK(m_vkd.waitForFences(*m_device, 1u, &fence, VK_TRUE, foreverNs));
	VK_CHECK(m_vkd.resetFences(*m_device, 1u, &fence));

	VkSemaphore currentAcquireSemaphore	= m_acquireSemaphores[m_frameNdx % m_acquireSemaphores.size()];
	VkSemaphore currentRenderSemaphore	= m_renderSemaphores[m_frameNdx % m_renderSemaphores.size()];

	// Acquire next image
	deUint32 imageIndex;
	VK_CHECK(m_vkd.acquireNextImageKHR(*m_device, *m_swapchain, foreverNs, currentAcquireSemaphore, (VkFence)0, &imageIndex));

	// Create command buffer
	commandBuffer = createCommandBuffer(m_vkd, *m_device, *m_commandPool, *m_renderPass, m_swapchainImages[imageIndex],
										m_framebuffers[imageIndex], *m_pipeline, width, height).disown();

	// Submit command buffer
	{
		const VkPipelineStageFlags	dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		const VkSubmitInfo			submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			DE_NULL,
			1u,
			&currentAcquireSemaphore,
			&dstStageMask,
			1u,
			&commandBuffer,
			1u,
			&currentRenderSemaphore
		};

		VK_CHECK(m_vkd.queueSubmit(m_queue, 1u, &submitInfo, fence));
	}

	VkResult result;
	const VkPresentInfoKHR presentInfo =
	{
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		DE_NULL,
		1u,
		&currentRenderSemaphore,
		1u,
		&*m_swapchain,
		&imageIndex,
		&result
	};

	VK_CHECK_WSI(m_vkd.queuePresentKHR(m_queue, &presentInfo));
	VK_CHECK_WSI(result);

	// verify counter on last frame - we know that we must have presented as meny frames
	// as we rendered minus the number of images in swapchain - that may not have been presented yet
	if (m_frameNdx >= m_frameCount)
	{
		deUint64 counter = 0;
		m_vkd.getSwapchainCounterEXT(*m_device, *m_swapchain, VK_SURFACE_COUNTER_VBLANK_EXT, &counter);
		if ((counter < (m_frameCount - m_swapchainImages.size())) || (counter > m_frameCount))
		{
			deinitSwapchainResources();
			m_resultCollector.fail("Invalid surface counter value");
		}
	}
}

tcu::TestStatus SwapchainCounterTestInstance::iterate (void)
{
	try
	{
		// Initialize swapchain specific resources
		if (m_frameNdx == 0)
			initSwapchainResources();

		// Render frame
		render();
	}
	catch (const Error& error)
	{
		if (error.getError() == VK_ERROR_OUT_OF_DATE_KHR)
		{
			if (m_outOfDateCount < m_maxOutOfDateCount)
			{
				m_context.getTestContext().getLog() << TestLog::Message << "Frame " << m_frameNdx
					<< ": Swapchain out of date. Recreating resources." << TestLog::EndMessage;
				deinitSwapchainResources();
				m_outOfDateCount++;
				m_frameNdx = 0;

				return tcu::TestStatus::incomplete();
			}

			m_context.getTestContext().getLog() << TestLog::Message << "Frame " << m_frameNdx
				<< ": Swapchain out of date." << TestLog::EndMessage;
			return tcu::TestStatus::fail("Received too many VK_ERROR_OUT_OF_DATE_KHR errors.");
		}

		deinitSwapchainResources();
		return tcu::TestStatus::fail(error.what());
	}

	m_frameNdx++;
	if (m_frameNdx < m_frameCount)
		return tcu::TestStatus::incomplete();

	deinitSwapchainResources();
	return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
}

class SwapchainCounterTestCase : public TestCase
{
public:

	SwapchainCounterTestCase(tcu::TestContext& context, const char* name);
	~SwapchainCounterTestCase() = default;

	void					initPrograms(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance(Context& context) const;
	virtual void			checkSupport(Context& context) const;
};

SwapchainCounterTestCase::SwapchainCounterTestCase(tcu::TestContext& context, const char* name)
	: vkt::TestCase(context, name, name)
{
}

void SwapchainCounterTestCase::initPrograms(SourceCollections& dst) const
{
	dst.glslSources.add("quad-vert") << glu::VertexSource(
		"#version 450\n"
		"out gl_PerVertex {\n"
		"    vec4 gl_Position;\n"
		"};\n"
		"highp float;\n"
		"void main (void) {\n"
		"    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
		"                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
		"}\n");
	dst.glslSources.add("quad-frag") << glu::FragmentSource(
		"#version 450\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"void main (void)\n"
		"{\n"
		"    o_color = vec4(1.0, 0.5, 0.0, 1.0);\n"
		"}\n");
}

TestInstance* SwapchainCounterTestCase::createInstance(Context& context) const
{
	return new SwapchainCounterTestInstance(context);
}

void SwapchainCounterTestCase::checkSupport(Context& context) const
{
	context.requireInstanceFunctionality("VK_KHR_display");
	context.requireDeviceFunctionality("VK_EXT_display_control");
}

void getDisplays(Context& context, std::vector<VkDisplayKHR>& availableDisplays)
{
	// get number of displays
	deUint32					countReported		= 0u;
	VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const InstanceInterface&	vki					= context.getInstanceInterface();
	const vk::Platform&	platform	= context.getTestContext().getPlatform().getVulkanPlatform();

	VkResult result = vki.getPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &countReported, DE_NULL);
	if (result != VK_SUCCESS)
		TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceDisplayPropertiesKHR failed");

	if (countReported == 0)
		TCU_THROW(NotSupportedError, "No displays available");

	for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
	{
		vk::wsi::Type	wsiType = (vk::wsi::Type)typeNdx;
		if (platform.hasDisplay(wsiType))
		{
			TCU_THROW(NotSupportedError, "Display is unavailable as windowing system has access");
		}
	}

	// get display properties
	std::vector<VkDisplayPropertiesKHR> displaysProperties(countReported);
	result = vki.getPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &countReported, &displaysProperties[0]);

	if (result != VK_SUCCESS)
		TCU_THROW(NotSupportedError, "vkGetPhysicalDeviceDisplayPropertiesKHR failed");

	availableDisplays.clear();
	for (const auto& dp : displaysProperties)
		availableDisplays.push_back(dp.display);
}

tcu::TestStatus testDisplayPowerControl(Context& context)
{
	// make sure VK_EXT_display_control is available
	context.requireDeviceFunctionality("VK_EXT_display_control");

	// get all connected displays
	std::vector<VkDisplayKHR> availableDisplays;
	getDisplays(context, availableDisplays);

	struct PowerStateData
	{
		VkDisplayPowerStateEXT	state;
		deUint32				waitMs;
	};
	vector<PowerStateData> powerStateDataVect =
	{
		{ VK_DISPLAY_POWER_STATE_ON_EXT,		1000 },
		{ VK_DISPLAY_POWER_STATE_SUSPEND_EXT,	1000 },
		{ VK_DISPLAY_POWER_STATE_OFF_EXT,		1000 },
		{ VK_DISPLAY_POWER_STATE_ON_EXT,		1000 },
	};

	// iterate over all displays
	VkDevice						device	= context.getDevice();
	const vk::DeviceInterface&		vkd		= context.getDeviceInterface();
	for (const auto& display : availableDisplays)
	{
		// iterate over tested sequence of power states
		for (const auto& psd : powerStateDataVect)
		{
			VkDisplayPowerInfoEXT displayPowerInfo =
			{
				VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT,
				DE_NULL,
				psd.state
			};

			VkResult result = vkd.displayPowerControlEXT(device, display, &displayPowerInfo);
			if (result != VK_SUCCESS)
				return tcu::TestStatus::fail(std::string("vkDisplayPowerControlEXT returned invalid result for ") + de::toString(psd.state));

			deSleep(psd.waitMs);
		}
	}

	return tcu::TestStatus::pass("pass");
}

tcu::TestStatus testDisplayEvent(Context& context)
{
	// make sure VK_EXT_display_control is available
	context.requireDeviceFunctionality("VK_EXT_display_control");

	// get all connected displays
	std::vector<vk::VkDisplayKHR> availableDisplays;
	getDisplays(context, availableDisplays);

	VkDevice				device	= context.getDevice();
	const DeviceInterface&	vkd		= context.getDeviceInterface();
	std::vector<VkFence>	fences	= std::vector<VkFence>(availableDisplays.size(), (VkFence)0);

	// iterate over all displays
	for (size_t i = 0 ; i < availableDisplays.size() ; ++i)
	{
		VkDisplayEventInfoEXT displayEventInfo =
		{
			VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT,
			DE_NULL,
			VK_DISPLAY_EVENT_TYPE_FIRST_PIXEL_OUT_EXT
		};

		VkFence&		fence		= fences[i];
		VkDisplayKHR&	display		= availableDisplays[i];
		VkResult		result		= vkd.registerDisplayEventEXT(device, display, &displayEventInfo, DE_NULL, &fence);
		if (result != VK_SUCCESS)
			return tcu::TestStatus::fail(std::string("vkRegisterDisplayEventEXT returned invalid result"));
	}

	// deinit fence
	deinitFences (vkd, device, fences);

	return tcu::TestStatus::pass("pass");
}

tcu::TestStatus testDeviceEvent(Context& context)
{
	// make sure VK_EXT_display_control is available
	context.requireDeviceFunctionality("VK_EXT_display_control");

	VkDevice				device = context.getDevice();
	const DeviceInterface&	vkd = context.getDeviceInterface();
	std::vector<VkFence>	fences = std::vector<VkFence>(1, (VkFence)0);

	vk::VkDeviceEventInfoEXT deviceEventInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT,
		DE_NULL,
		VK_DEVICE_EVENT_TYPE_DISPLAY_HOTPLUG_EXT
	};

	VkResult result = vkd.registerDeviceEventEXT(device, &deviceEventInfo, DE_NULL, &fences[0]);
	if (result != VK_SUCCESS)
		return tcu::TestStatus::fail(std::string("vkRegisterDeviceEventEXT returned invalid result"));

	// deinit fence
	deinitFences(vkd, device, fences);

	return tcu::TestStatus::pass("pass");
}

} // anonymous

void createDisplayControlTests (tcu::TestCaseGroup* testGroup)
{
	testGroup->addChild(new SwapchainCounterTestCase(testGroup->getTestContext(), "swapchain_counter"));
	addFunctionCase(testGroup, "display_power_control",		"Test display power control",	testDisplayPowerControl);
	addFunctionCase(testGroup, "register_display_event",	"Test register display event",	testDisplayEvent);
	addFunctionCase(testGroup, "register_device_event",		"Test register device event",	testDeviceEvent);
}

} // wsi
} // vkt
