#ifndef _VKWSIUTIL_HPP
#define _VKWSIUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
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
 * \brief Windowing System Integration (WSI) Utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkWsiPlatform.hpp"
#include "vkRef.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"

#include <vector>

namespace vk
{
namespace wsi
{

struct PlatformProperties
{
	enum FeatureFlags
	{
		FEATURE_INITIAL_WINDOW_SIZE		= (1<<0),		//!< Platform honors initial window size request
		FEATURE_RESIZE_WINDOW			= (1<<1),		//!< Platform supports resizing window
	};

	enum SwapchainExtent
	{
		SWAPCHAIN_EXTENT_MUST_MATCH_WINDOW_SIZE = 0,	//!< Swapchain extent must match window size
		SWAPCHAIN_EXTENT_SETS_WINDOW_SIZE,				//!< Window will be resized to swapchain size when first image is presented
		SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE,			//!< Presented image contents will be scaled to window size

		SWAPCHAIN_EXTENT_LAST
	};

	deUint32		features;
	SwapchainExtent	swapchainExtent;
	deUint32		maxDisplays;
	deUint32		maxWindowsPerDisplay;
};

const char*						getName									(Type wsiType);
const char*						getExtensionName						(Type wsiType);

const PlatformProperties&		getPlatformProperties					(Type wsiType);

VkResult						createSurface							(const InstanceInterface&		vki,
																		 VkInstance						instance,
																		 Type							wsiType,
																		 const Display&					nativeDisplay,
																		 const Window&					nativeWindow,
																		 const VkAllocationCallbacks*	pAllocator,
																		 VkSurfaceKHR*					pSurface);

Move<VkSurfaceKHR>				createSurface							(const InstanceInterface&		vki,
																		 VkInstance						instance,
																		 Type							wsiType,
																		 const Display&					nativeDisplay,
																		 const Window&					nativeWindow,
																		 const VkAllocationCallbacks*	pAllocator = DE_NULL);

VkBool32						getPhysicalDeviceSurfaceSupport			(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 deUint32						queueFamilyIndex,
																		 VkSurfaceKHR					surface);

VkBool32						getPhysicalDevicePresentationSupport	(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 deUint32						queueFamilyIndex,
																		 Type							wsiType,
																		 const Display&					nativeDisplay);

VkSurfaceCapabilitiesKHR		getPhysicalDeviceSurfaceCapabilities	(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

VkSurfaceCapabilities2EXT		getPhysicalDeviceSurfaceCapabilities2EXT(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

bool							sameSurfaceCapabilities					(const VkSurfaceCapabilitiesKHR&	khr,
																		 const VkSurfaceCapabilities2EXT&	ext);

std::vector<VkSurfaceFormatKHR>	getPhysicalDeviceSurfaceFormats			(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

std::vector<VkPresentModeKHR>	getPhysicalDeviceSurfacePresentModes	(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

std::vector<VkImage>			getSwapchainImages						(const DeviceInterface&			vkd,
																		 VkDevice						device,
																		 VkSwapchainKHR					swapchain);

deUint32						chooseQueueFamilyIndex					(const InstanceInterface&			vki,
																		 VkPhysicalDevice					physicalDevice,
																		 const std::vector<VkSurfaceKHR>&	surfaces);

deUint32						chooseQueueFamilyIndex					(const InstanceInterface&		vki,
																		 VkPhysicalDevice				physicalDevice,
																		 VkSurfaceKHR					surface);

std::vector<deUint32>			getCompatibleQueueFamilyIndices			(const InstanceInterface&			vki,
																		 VkPhysicalDevice					physicalDevice,
																		 const std::vector<VkSurfaceKHR>&	surface);

class WsiTriangleRenderer
{
public:
										WsiTriangleRenderer	(const DeviceInterface&		vkd,
															 const VkDevice				device,
															 Allocator&					allocator,
															 const BinaryCollection&	binaryRegistry,
															 bool						explicitLayoutTransitions,
															 const std::vector<VkImage>	swapchainImages,
															 const std::vector<VkImage>	aliasImages,
															 const VkFormat				framebufferFormat,
															 const tcu::UVec2&			renderSize);

										WsiTriangleRenderer	(WsiTriangleRenderer&&		other);

										~WsiTriangleRenderer(void);

	void								recordFrame			(VkCommandBuffer			cmdBuffer,
															 deUint32					imageNdx,
															 deUint32					frameNdx) const;

	void								recordDeviceGroupFrame (VkCommandBuffer			cmdBuffer,
																deUint32				imageNdx,
																deUint32				firstDeviceID,
																deUint32				secondDeviceID,
																deUint32				devicesCount,
																deUint32				frameNdx) const;

	static void							getPrograms			(SourceCollections& dst);

private:
	static Move<VkRenderPass>			createRenderPass	(const DeviceInterface&		vkd,
															 const VkDevice				device,
															 const VkFormat				colorAttachmentFormat,
															 const bool					explicitLayoutTransitions);

	static Move<VkPipelineLayout>		createPipelineLayout(const DeviceInterface&		vkd,
															 VkDevice					device);

	static Move<VkPipeline>				createPipeline		(const DeviceInterface&		vkd,
															 const VkDevice				device,
															 const VkRenderPass			renderPass,
															 const VkPipelineLayout		pipelineLayout,
															 const BinaryCollection&	binaryCollection,
															 const tcu::UVec2&			renderSize);

	static Move<VkImageView>			createAttachmentView(const DeviceInterface&		vkd,
															 const VkDevice				device,
															 const VkImage				image,
															 const VkFormat				format);

	static Move<VkFramebuffer>			createFramebuffer	(const DeviceInterface&		vkd,
															 const VkDevice				device,
															 const VkRenderPass			renderPass,
															 const VkImageView			colorAttachment,
															 const tcu::UVec2&			renderSize);

	static Move<VkBuffer>				createBuffer		(const DeviceInterface&		vkd,
															 VkDevice					device,
															 VkDeviceSize				size,
															 VkBufferUsageFlags			usage);

	const DeviceInterface&				m_vkd;

	bool								m_explicitLayoutTransitions;
	std::vector<VkImage>				m_swapchainImages;
	std::vector<VkImage>				m_aliasImages;
	tcu::UVec2							m_renderSize;

	Move<VkRenderPass>					m_renderPass;
	Move<VkPipelineLayout>				m_pipelineLayout;
	Move<VkPipeline>					m_pipeline;

	Move<VkBuffer>						m_vertexBuffer;
	de::MovePtr<Allocation>				m_vertexBufferMemory;

	using ImageViewSp	= de::SharedPtr<Unique<VkImageView>>;
	using FramebufferSp	= de::SharedPtr<Unique<VkFramebuffer>>;

	std::vector<ImageViewSp>			m_attachmentViews;
	mutable std::vector<VkImageLayout>	m_attachmentLayouts;
	std::vector<FramebufferSp>			m_framebuffers;
};

} // wsi
} // vk

#endif // _VKWSIUTIL_HPP
