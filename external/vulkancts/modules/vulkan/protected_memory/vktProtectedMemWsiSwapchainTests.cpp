/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory interaction with VkSwapchain Tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemWsiSwapchainTests.hpp"

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
#include "vkObjUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkAllocationCallbackUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"
#include "deSharedPtr.hpp"

#include <limits>

#include "vktProtectedMemContext.hpp"
#include "vktProtectedMemUtils.hpp"

namespace vkt
{
namespace ProtectedMem
{

namespace
{

typedef std::vector<vk::VkExtensionProperties> Extensions;

void checkAllSupported (const Extensions& supportedExtensions, const std::vector<std::string>& requiredExtensions)
{
	for (std::vector<std::string>::const_iterator requiredExtName = requiredExtensions.begin();
		 requiredExtName != requiredExtensions.end();
		 ++requiredExtName)
	{
		if (!isExtensionSupported(supportedExtensions, vk::RequiredExtension(*requiredExtName)))
			TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
	}
}

std::vector<std::string> getRequiredWsiExtensions (const Extensions&	supportedExtensions,
												   vk::wsi::Type		wsiType)
{
	std::vector<std::string>	extensions;

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
	if (isExtensionSupported(supportedExtensions, vk::RequiredExtension("VK_EXT_swapchain_colorspace")))
		extensions.push_back("VK_EXT_swapchain_colorspace");

	// VK_KHR_surface_protected_capabilities adds a way to check if swapchain can be
	// created for protected VkSurface, so if this extension is enabled then we can
	// check for that capability.
	// To check this capability, vkGetPhysicalDeviceSurfaceCapabilities2KHR needs
	// to be called so add VK_KHR_get_surface_capabilities2 for this.
	if (isExtensionSupported(supportedExtensions, vk::RequiredExtension("VK_KHR_surface_protected_capabilities")))
	{
		extensions.push_back("VK_KHR_get_surface_capabilities2");
		extensions.push_back("VK_KHR_surface_protected_capabilities");
	}

	checkAllSupported(supportedExtensions, extensions);

	return extensions;
}

de::MovePtr<vk::wsi::Display> createDisplay (const vk::Platform&	platform,
											 const Extensions&		supportedExtensions,
											 vk::wsi::Type			wsiType)
{
	try
	{
		return de::MovePtr<vk::wsi::Display>(platform.createWsiDisplay(wsiType));
	}
	catch (const tcu::NotSupportedError& e)
	{
		if (isExtensionSupported(supportedExtensions, vk::RequiredExtension(getExtensionName(wsiType))) &&
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

de::MovePtr<vk::wsi::Window> createWindow (const vk::wsi::Display& display, const tcu::Maybe<tcu::UVec2>& initialSize)
{
	try
	{
		return de::MovePtr<vk::wsi::Window>(display.createWindow(initialSize));
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
	const de::UniquePtr<vk::wsi::Display>	display;
	const de::UniquePtr<vk::wsi::Window>	window;

	NativeObjects (Context&							context,
				   const Extensions&				supportedExtensions,
				   vk::wsi::Type					wsiType,
				   const tcu::Maybe<tcu::UVec2>&	initialWindowSize = tcu::nothing<tcu::UVec2>())
		: display				(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
		, window				(createWindow(*display, initialWindowSize))
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
	vk::wsi::Type	wsiType;
	TestDimension	dimension;

	TestParameters (vk::wsi::Type wsiType_, TestDimension dimension_)
		: wsiType	(wsiType_)
		, dimension	(dimension_)
	{}

	TestParameters (void)
		: wsiType	(vk::wsi::TYPE_LAST)
		, dimension	(TEST_DIMENSION_LAST)
	{}
};

static vk::VkCompositeAlphaFlagBitsKHR firstSupportedCompositeAlpha(const vk::VkSurfaceCapabilitiesKHR& capabilities)
{
	deUint32 alphaMode = 1u;

	for (;alphaMode < capabilities.supportedCompositeAlpha;	alphaMode = alphaMode<<1u)
	{
		if ((alphaMode & capabilities.supportedCompositeAlpha) != 0)
		{
			break;
		}
	}

	return (vk::VkCompositeAlphaFlagBitsKHR)alphaMode;
}

std::vector<vk::VkSwapchainCreateInfoKHR> generateSwapchainParameterCases (vk::wsi::Type								wsiType,
																		   TestDimension								dimension,
																		   const ProtectedContext&						context,
																		   const vk::VkSurfaceCapabilitiesKHR&			capabilities,
																		   const std::vector<vk::VkSurfaceFormatKHR>&	formats,
																		   const std::vector<vk::VkPresentModeKHR>&		presentModes)
{
	std::vector<vk::VkSwapchainCreateInfoKHR>	cases;
	const vk::wsi::PlatformProperties&			platformProperties	= getPlatformProperties(wsiType);
	const vk::VkSurfaceTransformFlagBitsKHR		defaultTransform	= (capabilities.supportedTransforms & vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
																		? vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const vk::VkSwapchainCreateInfoKHR			baseParameters		=
	{
		vk::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
#ifndef NOT_PROTECTED
		vk::VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR,
#else
		(vk::VkSwapchainCreateFlagsKHR)0,
#endif
		(vk::VkSurfaceKHR)0,
		capabilities.minImageCount,
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE
			? capabilities.minImageExtent : capabilities.currentExtent),
		1u,									// imageArrayLayers
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		defaultTransform,
		firstSupportedCompositeAlpha(capabilities),
		vk::VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(vk::VkSwapchainKHR)0				// oldSwapchain
	};

	vk::VkImageCreateFlags imageCreateFlag =
#ifndef NOT_PROTECTED
		vk::VK_IMAGE_CREATE_PROTECTED_BIT;
#else
		(vk::VkImageCreateFlags)0u;
#endif

	switch (dimension)
	{
		case TEST_DIMENSION_MIN_IMAGE_COUNT:
		{
			// Estimate how much memory each swapchain image consumes. This isn't perfect, since
			// swapchain images may have additional constraints that equivalent non-swapchain
			// images don't have. But it's the best we can do.
			const vk::DeviceInterface&				vkd					= context.getDeviceInterface();
			vk::VkDevice							device				= context.getDevice();
			vk::VkMemoryRequirements				memoryRequirements;
			{
				const vk::VkImageCreateInfo			imageInfo			=
				{
					vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					DE_NULL,
					imageCreateFlag,
					vk::VK_IMAGE_TYPE_2D,
					baseParameters.imageFormat,
					{
						baseParameters.imageExtent.width,
						baseParameters.imageExtent.height,
						1,
					},
					1,	// mipLevels
					baseParameters.imageArrayLayers,
					vk::VK_SAMPLE_COUNT_1_BIT,
					vk::VK_IMAGE_TILING_OPTIMAL,
					baseParameters.imageUsage,
					baseParameters.imageSharingMode,
					baseParameters.queueFamilyIndexCount,
					baseParameters.pQueueFamilyIndices,
					vk::VK_IMAGE_LAYOUT_UNDEFINED
				};
				vk::Move<vk::VkImage>				image				= vk::createImage(vkd, device, &imageInfo);

				memoryRequirements	= vk::getImageMemoryRequirements(vkd, device, *image);
			}

			// Determine the maximum memory heap space available for protected images
			vk::VkPhysicalDeviceMemoryProperties	memoryProperties	= vk::getPhysicalDeviceMemoryProperties(context.getInstanceDriver(), context.getPhysicalDevice());
			vk::VkDeviceSize						protectedHeapSize	= 0;
			deUint32								protectedHeapMask	= 0;

			for (deUint32 memType = 0; memType < memoryProperties.memoryTypeCount; memType++)
			{
				deUint32 heapIndex	= memoryProperties.memoryTypes[memType].heapIndex;
				if ((memoryRequirements.memoryTypeBits & (1u << memType)) != 0 &&
#ifndef NOT_PROTECTED
					(memoryProperties.memoryTypes[memType].propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0 &&
#endif
					(protectedHeapMask & (1u << heapIndex)) == 0)
				{
					protectedHeapSize = de::max(protectedHeapSize, memoryProperties.memoryHeaps[heapIndex].size);
					protectedHeapMask |= 1u << heapIndex;
				}
			}

			// If the implementation doesn't have a max image count, min+16 means we won't clamp.
			// Limit it to how many protected images we estimate can be allocated
			const deUint32	maxImageCount		= de::min((capabilities.maxImageCount > 0) ? capabilities.maxImageCount : capabilities.minImageCount + 16u,
														  deUint32(protectedHeapSize / memoryRequirements.size));
			if (maxImageCount < capabilities.minImageCount)
				TCU_THROW(NotSupportedError, "Memory heap doesn't have enough memory!.");

			const deUint32	maxImageCountToTest	= de::clamp(16u, capabilities.minImageCount, maxImageCount);
			for (deUint32 imageCount = capabilities.minImageCount; imageCount <= maxImageCountToTest; ++imageCount)
			{
				cases.push_back(baseParameters);
				cases.back().minImageCount = imageCount;
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_FORMAT:
		{
			const vk::DeviceInterface&				vkd					= context.getDeviceInterface();
			vk::VkDevice							device				= context.getDevice();
			vk::VkPhysicalDeviceMemoryProperties	memoryProperties	= vk::getPhysicalDeviceMemoryProperties(context.getInstanceDriver(), context.getPhysicalDevice());
			vk::VkDeviceSize						protectedHeapSize	= 0;

			for (deUint32 memType = 0; memType < memoryProperties.memoryTypeCount; memType++)
			{
				deUint32 heapIndex	= memoryProperties.memoryTypes[memType].heapIndex;
#ifndef NOT_PROTECTED
				if (memoryProperties.memoryTypes[memType].propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT)
#endif
				{
					protectedHeapSize = de::max(protectedHeapSize, memoryProperties.memoryHeaps[heapIndex].size);
				}
			}

			for (std::vector<vk::VkSurfaceFormatKHR>::const_iterator curFmt = formats.begin(); curFmt != formats.end(); ++curFmt)
			{
			    vk::VkMemoryRequirements memoryRequirements;
			    {
					const vk::VkImageCreateInfo imageInfo =
					{
						vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
						DE_NULL,
						imageCreateFlag,
						vk::VK_IMAGE_TYPE_2D,
						curFmt->format,
						{
							platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE
								? capabilities.minImageExtent.width : capabilities.currentExtent.width,
							platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE
							? capabilities.minImageExtent.height : capabilities.currentExtent.height,
							1,
						},
						1,	// mipLevels
						baseParameters.imageArrayLayers,
						vk::VK_SAMPLE_COUNT_1_BIT,
						vk::VK_IMAGE_TILING_OPTIMAL,
						baseParameters.imageUsage,
						baseParameters.imageSharingMode,
						baseParameters.queueFamilyIndexCount,
						baseParameters.pQueueFamilyIndices,
						vk::VK_IMAGE_LAYOUT_UNDEFINED
					};

						vk::Move<vk::VkImage> image = vk::createImage(vkd, device, &imageInfo);

						memoryRequirements = vk::getImageMemoryRequirements(vkd, device, *image);
					}

					// Check for the image size requirement based on double/triple buffering
					if (memoryRequirements.size  * capabilities.minImageCount < protectedHeapSize)
					{
						cases.push_back(baseParameters);
						cases.back().imageFormat		= curFmt->format;
						cases.back().imageColorSpace	= curFmt->colorSpace;
					}
			}

			break;
		}

		case TEST_DIMENSION_IMAGE_EXTENT:
		{
			static const vk::VkExtent2D	s_testSizes[]	=
			{
				{ 1, 1 },
				{ 16, 32 },
				{ 32, 16 },
				{ 632, 231 },
				{ 117, 998 },
			};

			const vk::DeviceInterface&				vkd					= context.getDeviceInterface();
			vk::VkDevice							device				= context.getDevice();
			vk::VkPhysicalDeviceMemoryProperties	memoryProperties	= vk::getPhysicalDeviceMemoryProperties(context.getInstanceDriver(), context.getPhysicalDevice());
			vk::VkDeviceSize						protectedHeapSize	= 0;

			for (deUint32 memType = 0; memType < memoryProperties.memoryTypeCount; memType++)
			{
				deUint32 heapIndex	= memoryProperties.memoryTypes[memType].heapIndex;
#ifndef NOT_PROTECTED
				if (memoryProperties.memoryTypes[memType].propertyFlags & vk::VK_MEMORY_PROPERTY_PROTECTED_BIT)
#endif
				{
					protectedHeapSize = de::max(protectedHeapSize, memoryProperties.memoryHeaps[heapIndex].size);
				}
			}

			if (platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE ||
				platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE)
			{
				for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_testSizes); ++ndx)
				{
					vk::VkMemoryRequirements memoryRequirements;
					{
						const vk::VkImageCreateInfo imageInfo =
						{
							vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
							DE_NULL,
							imageCreateFlag,
							vk::VK_IMAGE_TYPE_2D,
							baseParameters.imageFormat,
							{
								s_testSizes[ndx].width,
								s_testSizes[ndx].height,
								1,
							},
							1,	// mipLevels
							baseParameters.imageArrayLayers,
							vk::VK_SAMPLE_COUNT_1_BIT,
							vk::VK_IMAGE_TILING_OPTIMAL,
							baseParameters.imageUsage,
							baseParameters.imageSharingMode,
							baseParameters.queueFamilyIndexCount,
							baseParameters.pQueueFamilyIndices,
							vk::VK_IMAGE_LAYOUT_UNDEFINED
						};

						vk::Move<vk::VkImage> image = vk::createImage(vkd, device, &imageInfo);

						memoryRequirements = vk::getImageMemoryRequirements(vkd, device, *image);
					}

					// Check for the image size requirement based on double/triple buffering
					if (memoryRequirements.size  * capabilities.minImageCount < protectedHeapSize)
					{
						cases.push_back(baseParameters);
						cases.back().imageExtent.width	= de::clamp(s_testSizes[ndx].width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
						cases.back().imageExtent.height	= de::clamp(s_testSizes[ndx].height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
					}
				}
			}

			if (platformProperties.swapchainExtent != vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE)
			{
				vk::VkMemoryRequirements memoryRequirements;
				{
					const vk::VkImageCreateInfo imageInfo =
					{
						vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
						DE_NULL,
						imageCreateFlag,
						vk::VK_IMAGE_TYPE_2D,
						baseParameters.imageFormat,
						{
							capabilities.currentExtent.width,
							capabilities.currentExtent.height,
							1,
						},
						1,	// mipLevels
						baseParameters.imageArrayLayers,
						vk::VK_SAMPLE_COUNT_1_BIT,
						vk::VK_IMAGE_TILING_OPTIMAL,
						baseParameters.imageUsage,
						baseParameters.imageSharingMode,
						baseParameters.queueFamilyIndexCount,
						baseParameters.pQueueFamilyIndices,
						vk::VK_IMAGE_LAYOUT_UNDEFINED
					};

					vk::Move<vk::VkImage> image = vk::createImage(vkd, device, &imageInfo);

					memoryRequirements = vk::getImageMemoryRequirements(vkd, device, *image);
				}

				// Check for the image size requirement based on double/triple buffering
				if (memoryRequirements.size  * capabilities.minImageCount < protectedHeapSize)
				{
					cases.push_back(baseParameters);
					cases.back().imageExtent = capabilities.currentExtent;
				}
			}

			if (platformProperties.swapchainExtent != vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE)
			{
				static const vk::VkExtent2D	s_testExtentSizes[]	=
				{
					{ capabilities.minImageExtent.width, capabilities.minImageExtent.height },
					{ capabilities.maxImageExtent.width, capabilities.maxImageExtent.height },
				};

				for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_testExtentSizes); ++ndx)
				{
					vk::VkMemoryRequirements memoryRequirements;
					{
						const vk::VkImageCreateInfo	imageInfo =
						{
							vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
							DE_NULL,
							imageCreateFlag,
							vk::VK_IMAGE_TYPE_2D,
							baseParameters.imageFormat,
							{
								s_testExtentSizes[ndx].width,
								s_testExtentSizes[ndx].height,
								1,
							},
							1,	// mipLevels
							baseParameters.imageArrayLayers,
							vk::VK_SAMPLE_COUNT_1_BIT,
							vk::VK_IMAGE_TILING_OPTIMAL,
							baseParameters.imageUsage,
							baseParameters.imageSharingMode,
							baseParameters.queueFamilyIndexCount,
							baseParameters.pQueueFamilyIndices,
							vk::VK_IMAGE_LAYOUT_UNDEFINED
						};

						vk::Move<vk::VkImage> image = vk::createImage(vkd, device, &imageInfo);

						memoryRequirements = vk::getImageMemoryRequirements(vkd, device, *image);
					}

					// Check for the image size requirement based on double/triple buffering
					if (memoryRequirements.size  * capabilities.minImageCount < protectedHeapSize)
					{
						cases.push_back(baseParameters);
						cases.back().imageExtent =s_testExtentSizes[ndx];
					}
				}
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
			cases.back().imageSharingMode = vk::VK_SHARING_MODE_EXCLUSIVE;

			cases.push_back(baseParameters);
			cases.back().imageSharingMode = vk::VK_SHARING_MODE_CONCURRENT;

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
					cases.back().preTransform = (vk::VkSurfaceTransformFlagBitsKHR)transform;
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
					cases.back().compositeAlpha = (vk::VkCompositeAlphaFlagBitsKHR)alphaMode;
				}
			}

			break;
		}

		case TEST_DIMENSION_PRESENT_MODE:
		{
			for (std::vector<vk::VkPresentModeKHR>::const_iterator curMode = presentModes.begin(); curMode != presentModes.end(); ++curMode)
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

std::vector<vk::VkSwapchainCreateInfoKHR> generateSwapchainParameterCases (vk::wsi::Type					wsiType,
																		   TestDimension					dimension,
																		   const ProtectedContext&			context,
																		   vk::VkSurfaceKHR					surface)
{
	const vk::InstanceInterface&				vki				= context.getInstanceDriver();
	vk::VkPhysicalDevice						physicalDevice	= context.getPhysicalDevice();
	const vk::VkSurfaceCapabilitiesKHR			capabilities	= vk::wsi::getPhysicalDeviceSurfaceCapabilities(vki,
																											   physicalDevice,
																											   surface);
	const std::vector<vk::VkSurfaceFormatKHR>	formats			= vk::wsi::getPhysicalDeviceSurfaceFormats(vki,
																										   physicalDevice,
																										   surface);
	const std::vector<vk::VkPresentModeKHR>		presentModes	= vk::wsi::getPhysicalDeviceSurfacePresentModes(vki,
																											    physicalDevice,
																											    surface);

	return generateSwapchainParameterCases(wsiType, dimension, context, capabilities, formats, presentModes);
}

tcu::TestStatus createSwapchainTest (Context& baseCtx, TestParameters params)
{
	std::vector<vk::VkExtensionProperties>			supportedExtensions (enumerateInstanceExtensionProperties(baseCtx.getPlatformInterface(), DE_NULL));
	std::vector<std::string>						instExts	= getRequiredWsiExtensions(supportedExtensions, params.wsiType);
	std::vector<std::string>						devExts;
	devExts.push_back("VK_KHR_swapchain");

	const NativeObjects								native		(baseCtx, supportedExtensions, params.wsiType);
	ProtectedContext								context		(baseCtx, params.wsiType, *native.display, *native.window, instExts, devExts);
	vk::VkSurfaceKHR								surface		= context.getSurface();
	const std::vector<vk::VkSwapchainCreateInfoKHR>	cases		(generateSwapchainParameterCases(params.wsiType,
																								 params.dimension,
																								 context,
																								 surface));
	deUint32										queueIdx	= context.getQueueFamilyIndex();
	for (size_t caseNdx = 0; caseNdx < cases.size(); ++caseNdx)
	{
		vk::VkSwapchainCreateInfoKHR	curParams	= cases[caseNdx];

		curParams.surface				= surface;
		curParams.queueFamilyIndexCount	= 1u;
		curParams.pQueueFamilyIndices	= &queueIdx;

		context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Sub-case " << (caseNdx+1) << " / " << cases.size() << ": " << curParams << tcu::TestLog::EndMessage;

		{
			const vk::Unique<vk::VkSwapchainKHR>	swapchain	(createSwapchainKHR(context.getDeviceDriver(), context.getDevice(), &curParams));
		}
	}

	return tcu::TestStatus::pass("Creating swapchain succeeded");
}

struct GroupParameters
{
	typedef FunctionInstance1<TestParameters>::Function	Function;

	vk::wsi::Type	wsiType;
	Function		function;

	GroupParameters (vk::wsi::Type wsiType_, Function function_)
		: wsiType	(wsiType_)
		, function	(function_)
	{}

	GroupParameters (void)
		: wsiType	(vk::wsi::TYPE_LAST)
		, function	((Function)DE_NULL)
	{}
};

void checkSupport (Context& context, TestParameters)
{
	checkProtectedQueueSupport(context);
}

void populateSwapchainGroup (tcu::TestCaseGroup* testGroup, GroupParameters params)
{
	for (int dimensionNdx = 0; dimensionNdx < TEST_DIMENSION_LAST; ++dimensionNdx)
	{
		const TestDimension		testDimension	= (TestDimension)dimensionNdx;

		addFunctionCase(testGroup, getTestDimensionName(testDimension), "", checkSupport, params.function, TestParameters(params.wsiType, testDimension));
	}
}

vk::VkSwapchainCreateInfoKHR getBasicSwapchainParameters (vk::wsi::Type					wsiType,
														  const vk::InstanceInterface&	vki,
														  vk::VkPhysicalDevice			physicalDevice,
														  vk::VkSurfaceKHR				surface,
														  const tcu::UVec2&				desiredSize,
														  deUint32						desiredImageCount)
{
	const vk::VkSurfaceCapabilitiesKHR			capabilities		= vk::wsi::getPhysicalDeviceSurfaceCapabilities(vki,
																												    physicalDevice,
																												    surface);
	const std::vector<vk::VkSurfaceFormatKHR>	formats				= vk::wsi::getPhysicalDeviceSurfaceFormats(vki,
																											   physicalDevice,
																											   surface);
	const vk::wsi::PlatformProperties&			platformProperties	= vk::wsi::getPlatformProperties(wsiType);
	const vk::VkSurfaceTransformFlagBitsKHR		transform			= (capabilities.supportedTransforms & vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
																		? vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
	const vk::VkSwapchainCreateInfoKHR			parameters			=
	{
		vk::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		DE_NULL,
		vk::VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR,
		surface,
		de::clamp(desiredImageCount, capabilities.minImageCount, capabilities.maxImageCount > 0 ? capabilities.maxImageCount : capabilities.minImageCount + desiredImageCount),
		formats[0].format,
		formats[0].colorSpace,
		(platformProperties.swapchainExtent == vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE
			? capabilities.currentExtent : vk::makeExtent2D(desiredSize.x(), desiredSize.y())),
		1u,									// imageArrayLayers
		vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		vk::VK_SHARING_MODE_EXCLUSIVE,
		0u,
		(const deUint32*)DE_NULL,
		transform,
		firstSupportedCompositeAlpha(capabilities),
		vk::VK_PRESENT_MODE_FIFO_KHR,
		VK_FALSE,							// clipped
		(vk::VkSwapchainKHR)0				// oldSwapchain
	};

	return parameters;
}

typedef de::SharedPtr<vk::Unique<vk::VkImageView> >		ImageViewSp;
typedef de::SharedPtr<vk::Unique<vk::VkFramebuffer> >	FramebufferSp;

class TriangleRenderer
{
public:
												TriangleRenderer	(ProtectedContext&				context,
																	 const vk::BinaryCollection&	binaryRegistry,
																	 const std::vector<vk::VkImage>	swapchainImages,
																	 const vk::VkFormat				framebufferFormat,
																	 const tcu::UVec2&				renderSize);
												~TriangleRenderer	(void);

	void										recordFrame			(vk::VkCommandBuffer			cmdBuffer,
																	 deUint32						imageNdx,
																	 deUint32						frameNdx) const;

	static void									getPrograms			(vk::SourceCollections&			dst);

private:
	static vk::Move<vk::VkRenderPass>			createRenderPass	(const vk::DeviceInterface&		vkd,
																	 const vk::VkDevice				device,
																	 const vk::VkFormat				colorAttachmentFormat);
	static vk::Move<vk::VkPipelineLayout>		createPipelineLayout(const vk::DeviceInterface&		vkd,
																	 vk::VkDevice					device);
	static vk::Move<vk::VkPipeline>				createPipeline		(const vk::DeviceInterface&		vkd,
																	 const vk::VkDevice				device,
																	 const vk::VkRenderPass			renderPass,
																	 const vk::VkPipelineLayout		pipelineLayout,
																	 const vk::BinaryCollection&	binaryCollection,
																	 const tcu::UVec2&				renderSize);

	const vk::DeviceInterface&					m_vkd;

	const std::vector<vk::VkImage>				m_swapchainImages;
	const tcu::UVec2							m_renderSize;

	const vk::Unique<vk::VkRenderPass>			m_renderPass;
	const vk::Unique<vk::VkPipelineLayout>		m_pipelineLayout;
	const vk::Unique<vk::VkPipeline>			m_pipeline;

	const de::UniquePtr<vk::BufferWithMemory>	m_vertexBuffer;

	std::vector<ImageViewSp>					m_attachmentViews;
	std::vector<FramebufferSp>					m_framebuffers;
};

vk::Move<vk::VkRenderPass> TriangleRenderer::createRenderPass (const vk::DeviceInterface&	vkd,
															   const vk::VkDevice			device,
															   const vk::VkFormat			colorAttachmentFormat)
{
	const vk::VkAttachmentDescription	colorAttDesc		=
	{
		(vk::VkAttachmentDescriptionFlags)0,
		colorAttachmentFormat,
		vk::VK_SAMPLE_COUNT_1_BIT,
		vk::VK_ATTACHMENT_LOAD_OP_CLEAR,
		vk::VK_ATTACHMENT_STORE_OP_STORE,
		vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,
		vk::VK_IMAGE_LAYOUT_UNDEFINED,
		vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};
	const vk::VkAttachmentReference		colorAttRef			=
	{
		0u,
		vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	const vk::VkSubpassDescription		subpassDesc			=
	{
		(vk::VkSubpassDescriptionFlags)0u,
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
		0u,							// inputAttachmentCount
		DE_NULL,					// pInputAttachments
		1u,							// colorAttachmentCount
		&colorAttRef,				// pColorAttachments
		DE_NULL,					// pResolveAttachments
		DE_NULL,					// depthStencilAttachment
		0u,							// preserveAttachmentCount
		DE_NULL,					// pPreserveAttachments
	};
	const vk::VkSubpassDependency		dependencies[]		=
	{
		{
			VK_SUBPASS_EXTERNAL,	// srcSubpass
			0u,						// dstSubpass
			vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			vk::VK_ACCESS_MEMORY_READ_BIT,
			(vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			vk::VK_DEPENDENCY_BY_REGION_BIT
		},
		{
			0u,						// srcSubpass
			VK_SUBPASS_EXTERNAL,	// dstSubpass
			vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			vk::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			(vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|
			 vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			vk::VK_ACCESS_MEMORY_READ_BIT,
			vk::VK_DEPENDENCY_BY_REGION_BIT
		},
	};
	const vk::VkRenderPassCreateInfo	renderPassParams	=
	{
		vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		DE_NULL,
		(vk::VkRenderPassCreateFlags)0,
		1u,
		&colorAttDesc,
		1u,
		&subpassDesc,
		DE_LENGTH_OF_ARRAY(dependencies),
		dependencies,
	};

	return vk::createRenderPass(vkd, device, &renderPassParams);
}

vk::Move<vk::VkPipelineLayout> TriangleRenderer::createPipelineLayout (const vk::DeviceInterface&	vkd,
																	   const vk::VkDevice			device)
{
	const vk::VkPushConstantRange					pushConstantRange		=
	{
		vk::VK_SHADER_STAGE_VERTEX_BIT,
		0u,											// offset
		(deUint32)sizeof(deUint32),					// size
	};
	const vk::VkPipelineLayoutCreateInfo			pipelineLayoutParams	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(vk::VkPipelineLayoutCreateFlags)0,
		0u,											// setLayoutCount
		DE_NULL,									// pSetLayouts
		1u,
		&pushConstantRange,
	};

	return vk::createPipelineLayout(vkd, device, &pipelineLayoutParams);
}

vk::Move<vk::VkPipeline> TriangleRenderer::createPipeline (const vk::DeviceInterface&	vkd,
														   const vk::VkDevice			device,
														   const vk::VkRenderPass		renderPass,
														   const vk::VkPipelineLayout	pipelineLayout,
														   const vk::BinaryCollection&	binaryCollection,
														   const tcu::UVec2&			renderSize)
{
	// \note VkShaderModules are fully consumed by vkCreateGraphicsPipelines()
	//		 and can be deleted immediately following that call.
	const vk::Unique<vk::VkShaderModule>				vertShaderModule		(createShaderModule(vkd, device, binaryCollection.get("tri-vert"), 0));
	const vk::Unique<vk::VkShaderModule>				fragShaderModule		(createShaderModule(vkd, device, binaryCollection.get("tri-frag"), 0));
	const std::vector<vk::VkViewport>					viewports				(1, vk::makeViewport(renderSize));
	const std::vector<vk::VkRect2D>						scissors				(1, vk::makeRect2D(renderSize));

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

TriangleRenderer::TriangleRenderer (ProtectedContext&				context,
									const vk::BinaryCollection&		binaryRegistry,
									const std::vector<vk::VkImage>	swapchainImages,
									const vk::VkFormat				framebufferFormat,
									const tcu::UVec2&				renderSize)
	: m_vkd					(context.getDeviceInterface())
	, m_swapchainImages		(swapchainImages)
	, m_renderSize			(renderSize)
	, m_renderPass			(createRenderPass(m_vkd, context.getDevice(), framebufferFormat))
	, m_pipelineLayout		(createPipelineLayout(m_vkd, context.getDevice()))
	, m_pipeline			(createPipeline(m_vkd, context.getDevice(), *m_renderPass, *m_pipelineLayout, binaryRegistry, renderSize))
	, m_vertexBuffer		(makeBuffer(context,
									PROTECTION_DISABLED,
									context.getQueueFamilyIndex(),
									(deUint32)(sizeof(float)*4*3),
									vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
									vk::MemoryRequirement::HostVisible))
{
	m_attachmentViews.resize(swapchainImages.size());
	m_framebuffers.resize(swapchainImages.size());

	for (size_t imageNdx = 0; imageNdx < swapchainImages.size(); ++imageNdx)
	{
		m_attachmentViews[imageNdx]	= ImageViewSp(new vk::Unique<vk::VkImageView>(createImageView(context, swapchainImages[imageNdx], framebufferFormat)));
		m_framebuffers[imageNdx]	= FramebufferSp(new vk::Unique<vk::VkFramebuffer>(createFramebuffer(context,
																										renderSize.x(),
																										renderSize.y(),
																										*m_renderPass,
																										**m_attachmentViews[imageNdx])));
	}

	// Upload vertex data
	{
		const tcu::Vec4				vertices[]	=
		{
			tcu::Vec4(-0.5f, -0.5f, 0.0f, 1.0f),
			tcu::Vec4(+0.5f, -0.5f, 0.0f, 1.0f),
			tcu::Vec4( 0.0f, +0.5f, 0.0f, 1.0f)
		};
		DE_STATIC_ASSERT(sizeof(vertices) == sizeof(float)*4*3);

		deMemcpy(m_vertexBuffer->getAllocation().getHostPtr(), &vertices[0], sizeof(vertices));
		flushAlloc(m_vkd, context.getDevice(), m_vertexBuffer->getAllocation());
	}
}

TriangleRenderer::~TriangleRenderer (void)
{
}

void TriangleRenderer::recordFrame (vk::VkCommandBuffer	cmdBuffer,
									deUint32			imageNdx,
									deUint32			frameNdx) const
{
	const vk::VkFramebuffer	curFramebuffer	= **m_framebuffers[imageNdx];

	beginCommandBuffer(m_vkd, cmdBuffer, 0u);

	beginRenderPass(m_vkd, cmdBuffer, *m_renderPass, curFramebuffer, vk::makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), tcu::Vec4(0.125f, 0.25f, 0.75f, 1.0f));
	m_vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	{
		const vk::VkDeviceSize bindingOffset = 0;
		m_vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_vertexBuffer->get(), &bindingOffset);
	}

	m_vkd.cmdPushConstants(cmdBuffer, *m_pipelineLayout, vk::VK_SHADER_STAGE_VERTEX_BIT, 0u, (deUint32)sizeof(deUint32), &frameNdx);
	m_vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	endRenderPass(m_vkd, cmdBuffer);

	endCommandBuffer(m_vkd, cmdBuffer);
}

void TriangleRenderer::getPrograms (vk::SourceCollections& dst)
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

typedef de::SharedPtr<vk::Unique<vk::VkCommandBuffer> >	CommandBufferSp;
typedef de::SharedPtr<vk::Unique<vk::VkFence> >			FenceSp;
typedef de::SharedPtr<vk::Unique<vk::VkSemaphore> >		SemaphoreSp;

std::vector<FenceSp> createFences (const vk::DeviceInterface&	vkd,
								   const vk::VkDevice			device,
								   size_t						numFences)
{
	std::vector<FenceSp> fences(numFences);

	for (size_t ndx = 0; ndx < numFences; ++ndx)
		fences[ndx] = FenceSp(new vk::Unique<vk::VkFence>(createFence(vkd, device)));

	return fences;
}

std::vector<SemaphoreSp> createSemaphores (const vk::DeviceInterface&	vkd,
										   const vk::VkDevice			device,
										   size_t						numSemaphores)
{
	std::vector<SemaphoreSp> semaphores(numSemaphores);

	for (size_t ndx = 0; ndx < numSemaphores; ++ndx)
		semaphores[ndx] = SemaphoreSp(new vk::Unique<vk::VkSemaphore>(createSemaphore(vkd, device)));

	return semaphores;
}

std::vector<CommandBufferSp> allocateCommandBuffers (const vk::DeviceInterface&		vkd,
													 const vk::VkDevice				device,
													 const vk::VkCommandPool		commandPool,
													 const vk::VkCommandBufferLevel	level,
													 const size_t					numCommandBuffers)
{
	std::vector<CommandBufferSp>			buffers		(numCommandBuffers);

	for (size_t ndx = 0; ndx < numCommandBuffers; ++ndx)
		buffers[ndx] = CommandBufferSp(new vk::Unique<vk::VkCommandBuffer>(allocateCommandBuffer(vkd, device, commandPool, level)));

	return buffers;
}

tcu::TestStatus basicRenderTest (Context& baseCtx, vk::wsi::Type wsiType)
{
	std::vector<vk::VkExtensionProperties>	supportedExtensions			(enumerateInstanceExtensionProperties(baseCtx.getPlatformInterface(), DE_NULL));
	std::vector<std::string>				instExts					= getRequiredWsiExtensions(supportedExtensions, wsiType);
	std::vector<std::string>				devExts;
	devExts.push_back("VK_KHR_swapchain");

	const tcu::UVec2						desiredSize					(256, 256);
	const NativeObjects						native						(baseCtx,  supportedExtensions, wsiType, tcu::just(desiredSize));
	ProtectedContext						context						(baseCtx, wsiType, *native.display, *native.window, instExts, devExts);
	vk::VkSurfaceKHR						surface						= context.getSurface();
	const vk::DeviceInterface&				vkd							= context.getDeviceInterface();
	const vk::VkDevice						device						= context.getDevice();
	const vk::VkSwapchainCreateInfoKHR		swapchainInfo				= getBasicSwapchainParameters(wsiType,
																								  context.getInstanceDriver(),
																								  context.getPhysicalDevice(),
																								  surface,
																								  desiredSize,
																								  2);
	const vk::Unique<vk::VkSwapchainKHR>	swapchain					(createSwapchainKHR(vkd, device, &swapchainInfo));
	const std::vector<vk::VkImage>			swapchainImages				= vk::wsi::getSwapchainImages(vkd, device, *swapchain);

	const TriangleRenderer					renderer					(context,
																		 context.getBinaryCollection(),
																		 swapchainImages,
																		 swapchainInfo.imageFormat,
																		 tcu::UVec2(swapchainInfo.imageExtent.width, swapchainInfo.imageExtent.height));

	const vk::Unique<vk::VkCommandPool>		commandPool					(makeCommandPool(vkd, device, PROTECTION_ENABLED,
																					 context.getQueueFamilyIndex()));

	const size_t							maxQueuedFrames				= swapchainImages.size()*2;

	// We need to keep hold of fences from vkAcquireNextImageKHR to actually
	// limit number of frames we allow to be queued.
	const std::vector<FenceSp>				imageReadyFences			(createFences(vkd, device, maxQueuedFrames));

	// We need maxQueuedFrames+1 for imageReadySemaphores pool as we need to pass
	// the semaphore in same time as the fence we use to meter rendering.
	const std::vector<SemaphoreSp>			imageReadySemaphores		(createSemaphores(vkd, device, maxQueuedFrames+1));

	// For rest we simply need maxQueuedFrames as we will wait for image
	// from frameNdx-maxQueuedFrames to become available to us, guaranteeing that
	// previous uses must have completed.
	const std::vector<SemaphoreSp>			renderingCompleteSemaphores	(createSemaphores(vkd, device, maxQueuedFrames));
	const std::vector<CommandBufferSp>		commandBuffers				(allocateCommandBuffers(vkd,
																								device,
																								*commandPool,
																								vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,
																								maxQueuedFrames));

	if (isExtensionSupported(supportedExtensions, vk::RequiredExtension("VK_KHR_surface_protected_capabilities")))
	{
		// Check if swapchain can be created for protected surface
		const vk::InstanceInterface&			vki			= context.getInstanceDriver();
		vk::VkSurfaceCapabilities2KHR			extCapabilities;
		vk::VkSurfaceProtectedCapabilitiesKHR		extProtectedCapabilities;
		const vk::VkPhysicalDeviceSurfaceInfo2KHR	surfaceInfo =
		{
			vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
			DE_NULL,
			surface
		};

		extProtectedCapabilities.sType			= vk::VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR;
		extProtectedCapabilities.pNext			= DE_NULL;
		extProtectedCapabilities.supportsProtected	= DE_FALSE;

		extCapabilities.sType				= vk::VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
		extCapabilities.pNext				= &extProtectedCapabilities;

		VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(context.getPhysicalDevice(), &surfaceInfo, &extCapabilities));

		if (extProtectedCapabilities.supportsProtected == DE_FALSE)
			TCU_THROW(NotSupportedError, "Swapchain creation for Protected VkSurface is not Supported.");
	}

	try
	{
		const deUint32	numFramesToRender	= 60*10;

		for (deUint32 frameNdx = 0; frameNdx < numFramesToRender; ++frameNdx)
		{
			const vk::VkFence		imageReadyFence		= **imageReadyFences[frameNdx%imageReadyFences.size()];
			const vk::VkSemaphore	imageReadySemaphore	= **imageReadySemaphores[frameNdx%imageReadySemaphores.size()];
			deUint32				imageNdx			= ~0u;

			if (frameNdx >= maxQueuedFrames)
				VK_CHECK(vkd.waitForFences(device, 1u, &imageReadyFence, VK_TRUE, std::numeric_limits<deUint64>::max()));

			VK_CHECK(vkd.resetFences(device, 1, &imageReadyFence));

			{
				const vk::VkResult	acquireResult	= vkd.acquireNextImageKHR(device,
																			  *swapchain,
																			  std::numeric_limits<deUint64>::max(),
																			  imageReadySemaphore,
																			  0,
																			  &imageNdx);

				if (acquireResult == vk::VK_SUBOPTIMAL_KHR)
					context.getTestContext().getLog() << tcu::TestLog::Message << "Got " << acquireResult << " at frame " << frameNdx << tcu::TestLog::EndMessage;
				else
					VK_CHECK(acquireResult);
			}

			TCU_CHECK((size_t)imageNdx < swapchainImages.size());

			{
				const vk::VkSemaphore			renderingCompleteSemaphore	= **renderingCompleteSemaphores[frameNdx%renderingCompleteSemaphores.size()];
				const vk::VkCommandBuffer		commandBuffer				= **commandBuffers[frameNdx%commandBuffers.size()];
				const vk::VkPipelineStageFlags	waitDstStage				= vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				vk::VkSubmitInfo				submitInfo					=
				{
					vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
					DE_NULL,
					1u,
					&imageReadySemaphore,
					&waitDstStage,
					1u,
					&commandBuffer,
					1u,
					&renderingCompleteSemaphore
				};

				const vk::VkProtectedSubmitInfo		protectedInfo   =
				{
					vk::VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO,		// sType
					DE_NULL,											// pNext
					VK_TRUE,											// protectedSubmit
				};
				submitInfo.pNext = &protectedInfo;

				const vk::VkPresentInfoKHR		presentInfo					=
				{
					vk::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					DE_NULL,
					1u,
					&renderingCompleteSemaphore,
					1u,
					&*swapchain,
					&imageNdx,
					(vk::VkResult*)DE_NULL
				};

				renderer.recordFrame(commandBuffer, imageNdx, frameNdx);
				VK_CHECK(vkd.queueSubmit(context.getQueue(), 1u, &submitInfo, imageReadyFence));
				VK_CHECK_WSI(vkd.queuePresentKHR(context.getQueue(), &presentInfo));
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

void getBasicRenderPrograms (vk::SourceCollections& dst, vk::wsi::Type)
{
	TriangleRenderer::getPrograms(dst);
}

void checkSupport (Context& context, vk::wsi::Type)
{
	checkProtectedQueueSupport(context);
}

void populateRenderGroup (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addFunctionCaseWithPrograms(testGroup, "basic", "Basic Rendering Test", checkSupport, getBasicRenderPrograms, basicRenderTest, wsiType);
}

void createSwapchainTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addTestGroup(testGroup, "create",			"Create VkSwapchain with various parameters",					populateSwapchainGroup,		GroupParameters(wsiType, createSwapchainTest));
	addTestGroup(testGroup, "render",			"Rendering Tests",												populateRenderGroup,		wsiType);
}

void createTypeSpecificTests (tcu::TestCaseGroup* testGroup, vk::wsi::Type wsiType)
{
	addTestGroup(testGroup, "swapchain", "VkSwapchain Tests", createSwapchainTests, wsiType);
}

} // anonymous

tcu::TestCaseGroup* createSwapchainTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> wsiTestGroup (new tcu::TestCaseGroup(testCtx, "wsi", "WSI Tests"));

	for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
	{
		const vk::wsi::Type	wsiType		= (vk::wsi::Type)typeNdx;

		addTestGroup(&*wsiTestGroup, getName(wsiType), "", createTypeSpecificTests, wsiType);
	}

	return wsiTestGroup.release();
}

} // wsi
} // vkt
