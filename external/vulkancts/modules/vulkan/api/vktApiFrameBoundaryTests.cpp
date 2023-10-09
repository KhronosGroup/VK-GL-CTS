/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2019 Advanced Micro Devices, Inc.
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
* \file
* \brief VK_EXT_frame_boundary tests
*//*--------------------------------------------------------------------*/

#include "vktApiFrameBoundaryTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkWsiUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuPlatform.hpp"

#include <iostream>
#include <string>
#include <vector>

#include <string.h>

namespace vkt
{
namespace api
{
namespace
{

using namespace vk;

enum ExtensionUse
{
	EXTENSION_USE_NONE,
	EXTENSION_USE_SYNC2,
};

enum TestType
{
	TEST_TYPE_SINGLE_FRAME,
	TEST_TYPE_SINGLE_FRAME_MULTIPLE_SUBMISSIONS,
	TEST_TYPE_MULTIPLE_FRAMES,
	TEST_TYPE_MULTIPLE_FRAMES_MULTIPLE_SUBMISSIONS,
	TEST_TYPE_MULTIPLE_OVERLAPPING_SUBMISSIONS,

	TEST_TYPE_LAST,
};

struct TestParams
{
	ExtensionUse m_extensionUse;
	TestType m_testType;
};

void checkSupport (Context& context, TestParams params)
{
	context.requireDeviceFunctionality("VK_EXT_frame_boundary");

	if (params.m_extensionUse == EXTENSION_USE_SYNC2)
		context.requireDeviceFunctionality("VK_KHR_synchronization2");
}

void checkWsiSupport (Context& context, vk::wsi::Type wsiType)
{
	context.requireDeviceFunctionality("VK_EXT_frame_boundary");

	context.requireInstanceFunctionality("VK_KHR_surface");
	context.requireInstanceFunctionality(vk::wsi::getExtensionName(wsiType));
	context.requireDeviceFunctionality("VK_KHR_swapchain");
}

void recordCommands(Context& context, VkCommandBuffer cmdBuffer, VkImage image)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();

	beginCommandBuffer(vk, cmdBuffer);

	const VkImageMemoryBarrier		imageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType
		DE_NULL,									// const void*				pNext
		0,											// VkAccessFlags			srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex
		image,										// VkImage					image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags		aspectMask
			0u,										// deUint32					baseMipLevel
			VK_REMAINING_MIP_LEVELS,				// deUint32					levelCount
			0u,										// deUint32					baseArrayLayer
			VK_REMAINING_ARRAY_LAYERS,				// deUint32					layerCount
		},											// VkImageSubresourceRange	subresourceRange
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1, &imageBarrier);

	const VkClearColorValue			clearColor		{{ 1.0f, 1.0f, 1.0f, 1.0f }};
	const VkImageSubresourceRange	range			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	vk.cmdClearColorImage(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u, &range);
	endCommandBuffer(vk, cmdBuffer);
}

void submitCommands(ExtensionUse extensionUse, Context& context, VkCommandBuffer cmdBuffer, bool lastInFrame, uint32_t frameID, VkImage *pImages)
{
	const DeviceInterface&		vk				= context.getDeviceInterface();
	const VkDevice				vkDevice		= context.getDevice();

	const VkFenceCreateInfo		fenceParams		=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,							// VkStructureType		sType
		DE_NULL,														// const void*			pNext
		0u,																// VkFenceCreateFlags	flags
	};

	const Move<VkFence>			fence			= createFence(vk, vkDevice, &fenceParams);

	VkFrameBoundaryEXT			frameBoundary	=
	{
		VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT,							// VkStructureType			sType
		DE_NULL,														// const void*				pNext
		0u,																// VkFrameBoundaryFlagsEXT	flags
		frameID,														// uint64_t					frameID
		(lastInFrame ? 1u : 0u),										// uint32_t					imageCount
		(lastInFrame ? pImages : DE_NULL),								// const VkImage*			pImages
		0u,																// deUint32					bufferCount;
		DE_NULL,														// VkBuffer*				pBuffers;
		0u,																// uint64_t					tagName
		0u,																// size_t					tagSize
		DE_NULL,														// const void*				pTag
	};

	if (lastInFrame)
		frameBoundary.flags = VK_FRAME_BOUNDARY_FRAME_END_BIT_EXT;

	switch (extensionUse)
	{
		case EXTENSION_USE_NONE:
		{
			const VkSubmitInfo	submitInfo	=
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,							// VkStructureType				sType
				&frameBoundary,											// const void*					pNext
				0,														// uint32_t						waitSemaphoreCount
				DE_NULL,												// const VkSemaphore*			pWaitSemaphores
				DE_NULL,												// const VkPipelineStageFlags*	pWaitDstStageMask
				1,														// uint32_t						commandBufferCount
				&cmdBuffer,												// const VkCommandBuffer*		pCommandBuffers
				0u,														// uint32_t						signalSemaphoreCount
				DE_NULL,												// const VkSemaphore*			pSignalSemaphores
			};

			VK_CHECK(vk.queueSubmit(context.getUniversalQueue(), 1, &submitInfo, *fence));
			break;
		}
		case EXTENSION_USE_SYNC2:
		{
			const VkCommandBufferSubmitInfo		cmdBufferSubmitInfo	=
			{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,		// VkStructureType	sType
				DE_NULL,												// const void*		pNext
				cmdBuffer,												// VkCommandBuffer	commandBuffer
				0u,														// uint32_t			deviceMask
			};

			const VkSubmitInfo2					submitInfo2KHR		=
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO_2,						// VkStructureType					sType
				&frameBoundary,											// const void*						pNext
				0u,														// VkSubmitFlagsKHR					flags
				0u,														// uint32_t							waitSemaphoreInfoCount
				DE_NULL,												// const VkSemaphoreSubmitInfo*		pWaitSemaphoreInfos
				1u,														// uint32_t							commandBufferInfoCount
				&cmdBufferSubmitInfo,									// const VkCommandBufferSubmitInfo*	pCommandBufferInfos
				0u,														// uint32_t							signalSemaphoreInfoCount
				DE_NULL,												// const VkSemaphoreSubmitInfo*		pSignalSemaphoreInfos
			};

			VK_CHECK(vk.queueSubmit2(context.getUniversalQueue(), 1, &submitInfo2KHR, *fence));
			break;
		}
		default:
			DE_ASSERT(false);
	}

	VK_CHECK(vk.waitForFences(vkDevice, 1, &fence.get(), VK_TRUE, ~0ull));
}

tcu::TestStatus testCase(Context& context, TestParams params)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice				vkDevice			= context.getDevice();

	const VkExtent3D			extent				{ 16, 16, 1 };
	const VkImageCreateInfo		imageParams			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,								// const void*			pNext
		0u,										// VkImageCreateFlags	flags
		VK_IMAGE_TYPE_2D,						// VkImageType			imageType
		VK_FORMAT_R8G8B8A8_UNORM,				// VkFormat				format
		extent,									// VkExtent3D			extent
		1u,										// deUint32				mipLevels
		1u,										// deUint32				arraySize
		VK_SAMPLE_COUNT_1_BIT,					// deUint32				samples
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling		tiling
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0u,										// deUint32				queueFamilyIndexCount
		(const deUint32*)DE_NULL,				// const deUint32*		pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout		initialLayout
	};

	Move<VkImage>				image				= createImage(vk, vkDevice, &imageParams);
	VkMemoryRequirements		memoryRequirements	= getImageMemoryRequirements(vk, vkDevice, *image);
	de::MovePtr<Allocation>		imageAllocation		= context.getDefaultAllocator().allocate(memoryRequirements, MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageAllocation->getMemory(), 0u));

	Move<VkCommandPool>			cmdPool				= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer>		cmdBuffer			= allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VkImage						frameImages[]		= { *image };

	recordCommands(context, *cmdBuffer, *image);

	switch (params.m_testType)
	{
		case TEST_TYPE_SINGLE_FRAME:
		{
			submitCommands(params.m_extensionUse, context, *cmdBuffer, true, 1, frameImages);
			break;
		}
		case TEST_TYPE_SINGLE_FRAME_MULTIPLE_SUBMISSIONS:
		{
			for (uint32_t i = 0; i < 4; i++)
			{
				bool lastInFrame = (i == 3);
				submitCommands(params.m_extensionUse, context, *cmdBuffer, lastInFrame, 1, frameImages);
			}

			break;
		}
		case TEST_TYPE_MULTIPLE_FRAMES:
		{
			for (uint32_t i = 1; i <= 4; i++)
				submitCommands(params.m_extensionUse, context, *cmdBuffer, true, i, frameImages);

			break;
		}
		case TEST_TYPE_MULTIPLE_FRAMES_MULTIPLE_SUBMISSIONS:
		{
			for (uint32_t i = 1; i <= 4; i++)
			{
				submitCommands(params.m_extensionUse, context, *cmdBuffer, false, i, frameImages);
				submitCommands(params.m_extensionUse, context, *cmdBuffer, true, i, frameImages);
			}

			break;
		}
		case TEST_TYPE_MULTIPLE_OVERLAPPING_SUBMISSIONS:
		{
			submitCommands(params.m_extensionUse, context, *cmdBuffer, false, 1, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, false, 2, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, true, 1, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, false, 3, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, true, 2, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, false, 4, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, true, 3, frameImages);
			submitCommands(params.m_extensionUse, context, *cmdBuffer, true, 4, frameImages);
			break;
		}
		default:
			DE_ASSERT(false);
	}


	return tcu::TestStatus::pass("Pass");
}

typedef std::vector<VkExtensionProperties> Extensions;

de::MovePtr<wsi::Display> createDisplay(const vk::Platform&	platform,
	const Extensions&	supportedExtensions,
	wsi::Type			wsiType)
{
	try
	{
		return de::MovePtr<wsi::Display>(platform.createWsiDisplay(wsiType));
	}
	catch (const tcu::NotSupportedError& e)
	{
		if (isExtensionStructSupported(supportedExtensions, RequiredExtension(wsi::getExtensionName(wsiType))) &&
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

de::MovePtr<wsi::Window> createWindow(const wsi::Display& display, const tcu::Maybe<tcu::UVec2>& initialSize)
{
	try
	{
		return de::MovePtr<wsi::Window>(display.createWindow(initialSize));
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
	const de::UniquePtr<wsi::Display>	display;
	const de::UniquePtr<wsi::Window>	window;

	NativeObjects(Context&				context,
				  const Extensions&		supportedExtensions,
				  wsi::Type				wsiType,
				  const tcu::Maybe<tcu::UVec2>&	initialWindowSize = tcu::Nothing)
		: display(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
		, window(createWindow(*display, initialWindowSize))
	{}
};

Move<VkSwapchainKHR> createSwapchain(Context& context, VkSurfaceKHR surface)
{
	const InstanceInterface&			vki					= context.getInstanceInterface();
	const DeviceInterface&				vk					= context.getDeviceInterface();
	const VkDevice						vkDevice			= context.getDevice();
	const VkPhysicalDevice				vkPhysicalDevice	= context.getPhysicalDevice();

	const VkSurfaceCapabilitiesKHR		capabilities		= wsi::getPhysicalDeviceSurfaceCapabilities(vki,
																										vkPhysicalDevice,
																										surface);

	if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		TCU_THROW(NotSupportedError, "supportedUsageFlags does not contain VK_IMAGE_USAGE_TRANSFER_DST_BIT");

	const std::vector<VkSurfaceFormatKHR>surfaceFormats		= wsi::getPhysicalDeviceSurfaceFormats(vki,
																								   vkPhysicalDevice,
																								   surface);

	const VkSurfaceFormatKHR			surfaceFormat		= surfaceFormats[0];

	const VkExtent2D					swapchainExtent		= {
		de::clamp(16u, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		de::clamp(16u, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
	};

	const VkSwapchainCreateInfoKHR		swapchainParams		=
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,		// VkStructureType					sType
		DE_NULL,											// const void*						pNext
		0u,													// VkSwapchainCreateFlagsKHR		flags
		surface,											// VkSurfaceKHR						surface
		std::max(1u, capabilities.minImageCount),			// deUint32							minImageCount
		surfaceFormat.format,								// VkFormat							imageFormat
		surfaceFormat.colorSpace,							// VkColorSpaceKHR					imageColorSpace
		swapchainExtent,									// VkExtent2D						imageExtent
		1,													// deUint32							imageArrayLayers
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,					// VkImageUsageFlags				imageUsage
		VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode					imageSharingMode
		0u,													// deUint32							queueFamilyIndexCount
		(const deUint32*)DE_NULL,							// const deUint32*					pQueueFamilyIndices
		capabilities.currentTransform,						// VkSurfaceTransformFlagBitsKHR	preTransform
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,					// VkCompositeAlphaFlagBitsKHR		compositeAlpha
		VK_PRESENT_MODE_FIFO_KHR,							// VkPresentModeKHR					presentMode
		VK_FALSE,											// VkBool32							clipped
		(VkSwapchainKHR)0									// VkSwapchainKHR					oldSwapchain
	};

	return createSwapchainKHR(vk, vkDevice, &swapchainParams);
}

tcu::TestStatus testCaseWsi(Context& context, vk::wsi::Type wsiType)
{
	const InstanceInterface&		vki					= context.getInstanceInterface();
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkInstance				vkInstance			= context.getInstance();
	const VkDevice					vkDevice			= context.getDevice();

	const NativeObjects				native				(context, enumerateInstanceExtensionProperties(context.getPlatformInterface(), DE_NULL), wsiType);
	const Unique<VkSurfaceKHR>		surface				(wsi::createSurface(vki, vkInstance, wsiType, *native.display, *native.window, context.getTestContext().getCommandLine()));
	const Unique<VkSwapchainKHR>	swapchain			(createSwapchain(context, *surface));
	const std::vector<VkImage>		swapchainImages		= wsi::getSwapchainImages(vk, vkDevice, *swapchain);

	const Move<VkCommandPool>		cmdPool				= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex());
	const Move<VkCommandBuffer>		cmdBuffer			= allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkSemaphoreCreateInfo		semaphoreCreateInfo	=
	{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		0u,													// VkSemaphoreCreateFlags	flags;
	};

	const Move<VkSemaphore>			acquireSemaphore	= createSemaphore(vk, vkDevice, &semaphoreCreateInfo);

	VkSemaphore						acquireSemaphores[]	= { *acquireSemaphore };
	VkPipelineStageFlags			waitStageMask[]		= { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	uint32_t						currentBuffer		= 0;
	VK_CHECK(vk.acquireNextImageKHR(vkDevice, *swapchain, ~0ull, *acquireSemaphore, VK_NULL_HANDLE, &currentBuffer));

	recordCommands(context, *cmdBuffer, swapchainImages[currentBuffer]);

	submitCommandsAndWait(vk, vkDevice, context.getUniversalQueue(), *cmdBuffer, false, 0U, 1U, acquireSemaphores, waitStageMask);

	VkSwapchainKHR					swapchains[]		= { *swapchain };
	VkImage							frameImages[]		= { swapchainImages[currentBuffer] };

	const VkFrameBoundaryEXT frameBoundary				=
	{
		VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT,				// VkStructureType			sType
		DE_NULL,											// const void*				pNext
		VK_FRAME_BOUNDARY_FRAME_END_BIT_EXT,				// VkFrameBoundaryFlagsEXT	flags
		1,													// uint64_t					frameID
		1u,													// uint32_t					imageCount
		frameImages,										// const VkImage*			pImages
		0u,													// deUint32					bufferCount;
		DE_NULL,											// VkBuffer*				pBuffers;
		0u,													// uint64_t					tagName
		0u,													// size_t					tagSize
		DE_NULL,											// const void*				pTag
	};

	const VkPresentInfoKHR presentInfo					=
	{
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,					// VkStructureType			sType;
		&frameBoundary,										// const void*				pNext;
		0U,													// uint32_t					waitSemaphoreCount;
		DE_NULL,											// const VkSemaphore*		pWaitSemaphores;
		1U,													// uint32_t					swapchainCount;
		swapchains,											// const VkSwapchainKHR*	pSwapchains;
		&currentBuffer,										// const uint32_t*			pImageIndices;
		DE_NULL,											// VkResult*				pResults;
	};

	VK_CHECK(vk.queuePresentKHR(context.getUniversalQueue(), &presentInfo));

	return tcu::TestStatus::pass("Pass");
}

void createExecTestCases(tcu::TestCaseGroup* group, ExtensionUse extensionUse)
{
	const std::string testName[TEST_TYPE_LAST] =
	{
		"single_frame",
		"single_frame_multi_submissions",
		"multi_frame",
		"multi_frame_multi_submissions",
		"multi_frame_overlapping_submissions",
	};

	for (uint32_t testType = 0; testType < TEST_TYPE_LAST; testType++)
	{
		TestParams testParams { extensionUse, (TestType) testType };
		addFunctionCase(group, testName[testType], "", checkSupport, testCase, testParams);
	}
}

void createWsiTestCases(tcu::TestCaseGroup* group)
{
	for (uint32_t wsiType = 0; wsiType < wsi::TYPE_LAST; wsiType++)
	{
		addFunctionCase(group, wsi::getName((wsi::Type) wsiType), "", checkWsiSupport, testCaseWsi, (wsi::Type) wsiType);
	}
}

void createTestCases (tcu::TestCaseGroup* group)
{
	addTestGroup(group, "core", "VK_EXT_frame_boundary tests", createExecTestCases, EXTENSION_USE_NONE);
	addTestGroup(group, "sync2", "VK_EXT_frame_boundary tests using sync2", createExecTestCases, EXTENSION_USE_SYNC2);
	addTestGroup(group, "wsi", "VK_EXT_frame_boundary wsi tests", createWsiTestCases);
}

} // anonymous

tcu::TestCaseGroup*	createFrameBoundaryTests(tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "frame_boundary", "VK_EXT_frame_boundary tests", createTestCases);
}

} // api
} // vkt
