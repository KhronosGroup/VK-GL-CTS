/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
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
 * \brief Vulkan Multi View Render Util
 *//*--------------------------------------------------------------------*/

#include "vktMultiViewRenderUtil.hpp"

#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

namespace vkt
{
namespace MultiView
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using std::vector;

VkImageAspectFlags getAspectFlags (tcu::TextureFormat format)
{
	VkImageAspectFlags	aspectFlag	= 0;
	aspectFlag |= (tcu::hasDepthComponent(format.order)? VK_IMAGE_ASPECT_DEPTH_BIT : 0);
	aspectFlag |= (tcu::hasStencilComponent(format.order)? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

	if (!aspectFlag)
		aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;

	return aspectFlag;
}

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize bufferSize, const VkBufferUsageFlags usage)
{
	const VkBufferCreateInfo bufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		(VkBufferCreateFlags)0,					// VkBufferCreateFlags	flags;
		bufferSize,								// VkDeviceSize			size;
		usage,									// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		DE_NULL,								// const deUint32*		pQueueFamilyIndices;
	};
	return bufferCreateInfo;
}

VkImageCreateInfo makeImageCreateInfo (const VkImageType imageType, const VkExtent3D& extent, const VkFormat format, const VkImageUsageFlags usage, const VkSampleCountFlagBits samples)
{
	const VkImageCreateInfo imageInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags		flags;
		imageType,									// VkImageType				imageType;
		format,										// VkFormat					format;
		{extent.width, extent.height, 1u},			// VkExtent3D				extent;
		1u,											// uint32_t					mipLevels;
		extent.depth,								// uint32_t					arrayLayers;
		samples,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
		0u,											// uint32_t					queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			initialLayout;
	};
	return imageInfo;
}

Move<VkImageView> makeImageView (const DeviceInterface&			vk,
								 const VkDevice					device,
								 const VkImage					image,
								 const VkImageViewType			viewType,
								 const VkFormat					format,
								 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo imageViewParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkImageViewCreateFlags)0,						// VkImageViewCreateFlags	flags;
		image,											// VkImage					image;
		viewType,										// VkImageViewType			viewType;
		format,											// VkFormat					format;
		makeComponentMappingRGBA(),						// VkComponentMapping		components;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return createImageView(vk, device, &imageViewParams);
}

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&		vk,
									 const VkDevice				device,
									 const VkRenderPass			renderPass,
									 const vector<VkImageView>&	attachments,
									 const deUint32				width,
									 const deUint32				height,
									 const deUint32				layers)
{
	const VkFramebufferCreateInfo framebufferInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags	flags;
		renderPass,										// VkRenderPass				renderPass;
		static_cast<deUint32>(attachments.size()),		// uint32_t					attachmentCount;
		&attachments[0],								// const VkImageView*		pAttachments;
		width,											// uint32_t					width;
		height,											// uint32_t					height;
		layers,											// uint32_t					layers;
	};
	return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkDescriptorSetLayout*	pSetLayouts)
{
	const VkPipelineLayoutCreateInfo info =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		(VkPipelineLayoutCreateFlags)0,					// VkPipelineLayoutCreateFlags	flags;
		(DE_NULL == pSetLayouts)? 0u : 1u,				// deUint32						setLayoutCount;
		pSetLayouts,									// const VkDescriptorSetLayout*	pSetLayouts;
		0u,												// deUint32						pushConstantRangeCount;
		DE_NULL,										// const VkPushConstantRange*	pPushConstantRanges;
	};
	return createPipelineLayout(vk, device, &info);
}

Move<VkDescriptorSetLayout> makeDescriptorSetLayout (const DeviceInterface&		vk,
													const VkDevice				device)
{
	const VkDescriptorSetLayoutBinding		binding		=
	{
		0u,											//deUint32				binding;
		vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	//VkDescriptorType		descriptorType;
		1u,											//deUint32				descriptorCount;
		vk::VK_SHADER_STAGE_FRAGMENT_BIT,			//VkShaderStageFlags	stageFlags;
		DE_NULL										//const VkSampler*		pImmutableSamplers;
	};

	const VkDescriptorSetLayoutCreateInfo	createInfo	=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	//VkStructureType						sType;
		DE_NULL,													//const void*							pNext;
		0u,															//VkDescriptorSetLayoutCreateFlags		flags;
		1u,															//deUint32								bindingCount;
		&binding													//const VkDescriptorSetLayoutBinding*	pBindings;
	};
	return vk::createDescriptorSetLayout(vk, device, &createInfo);
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&		vk,
								   const VkDevice				device,
								   const VkFormat				colorFormat,
								   const vector<deUint32>&		viewMasks,
								   const VkSampleCountFlagBits	samples,
								   const VkAttachmentLoadOp		colorLoadOp)

{
	const deUint32								subpassCount				= static_cast<deUint32>(viewMasks.size());
	const VkAttachmentDescription				colorAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags	flags;
		colorFormat,														// VkFormat						format;
		samples,															// VkSampleCountFlagBits		samples;
		colorLoadOp,														// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp			stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,							// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL							// VkImageLayout				finalLayout;
	};

	const VkAttachmentReference					colorAttachmentReference	=
	{
		0u,																	// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL							// VkImageLayout	layout;
	};

	const VkAttachmentReference					depthAttachmentReference	=
	{
		VK_ATTACHMENT_UNUSED,												// deUint32			attachment;
		VK_IMAGE_LAYOUT_UNDEFINED											// VkImageLayout	layout;
	};

	const VkSubpassDescription					subpassDescription			=
	{
		(VkSubpassDescriptionFlags)0,										// VkSubpassDescriptionFlags	flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,									// VkPipelineBindPoint			pipelineBindPoint;
		0u,																	// deUint32						inputAttachmentCount;
		DE_NULL,															// const VkAttachmentReference*	pInputAttachments;
		1u,																	// deUint32						colorAttachmentCount;
		&colorAttachmentReference,											// const VkAttachmentReference*	pColorAttachments;
		DE_NULL,															// const VkAttachmentReference*	pResolveAttachments;
		&depthAttachmentReference,											// const VkAttachmentReference*	pDepthStencilAttachment;
		0u,																	// deUint32						preserveAttachmentCount;
		DE_NULL																// const deUint32*				pPreserveAttachments;
	};
	vector <VkSubpassDescription>				subpassDescriptions			(subpassCount, subpassDescription);

	const VkRenderPassMultiviewCreateInfo		renderPassMultiviewInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO_KHR,			//VkStructureType	sType;
		DE_NULL,															//const void*		pNext;
		subpassCount,														//uint32_t			subpassCount;
		&viewMasks[0],														//const uint32_t*	pViewMasks;
		0u,																	//uint32_t			dependencyCount;
		DE_NULL,															//const int32_t*	pViewOffsets;
		0u,																	//uint32_t			correlationMaskCount;
		DE_NULL,															//const uint32_t*	pCorrelationMasks;
	};

	vector <VkSubpassDependency>				subpassDependencies;
	for(deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
	{
		const VkSubpassDependency				subpassDependency			=
		{
			subpassNdx,														// deUint32				srcSubpass;
			(subpassNdx ==subpassCount - 1u) ? subpassNdx : subpassNdx+1u,	// deUint32				dstSubpass;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,					// VkPipelineStageFlags	srcStageMask;
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,							// VkPipelineStageFlags	dstStageMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,							// VkAccessFlags		srcAccessMask;
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,							// VkAccessFlags		dstAccessMask;
			VK_DEPENDENCY_VIEW_LOCAL_BIT_KHR,								// VkDependencyFlags	dependencyFlags;
		};
		subpassDependencies.push_back(subpassDependency);
	}

	const VkRenderPassCreateInfo				renderPassInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,							// VkStructureType					sType;
		&renderPassMultiviewInfo,											// const void*						pNext;
		(VkRenderPassCreateFlags)0,											// VkRenderPassCreateFlags			flags;
		1u,																	// deUint32							attachmentCount;
		&colorAttachmentDescription,										// const VkAttachmentDescription*	pAttachments;
		subpassCount,														// deUint32							subpassCount;
		&subpassDescriptions[0],											// const VkSubpassDescription*		pSubpasses;
		subpassCount,														// deUint32							dependencyCount;
		&subpassDependencies[0]												// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkRenderPass> makeRenderPassWithAttachments (const DeviceInterface&	vk,
												  const VkDevice			device,
												  const VkFormat			colorFormat,
												  const vector<deUint32>&	viewMasks)
{
	const deUint32								subpassCount				= static_cast<deUint32>(viewMasks.size());

	const VkAttachmentDescription				colorAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags;
		colorFormat,								// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription				inputAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags;
		colorFormat,								// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_GENERAL						// VkImageLayout					finalLayout;
	};

	vector<VkAttachmentDescription>				attachments;
	attachments.push_back(colorAttachmentDescription);
	attachments.push_back(inputAttachmentDescription);

	const VkAttachmentReference					colorAttachmentReference	=
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference					inputAttachmentReference	=
	{
		1u,						// deUint32			attachment;
		VK_IMAGE_LAYOUT_GENERAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference					depthAttachmentReference	=
	{
		VK_ATTACHMENT_UNUSED,		// deUint32			attachment;
		VK_IMAGE_LAYOUT_UNDEFINED	// VkImageLayout	layout;
	};

	const VkSubpassDescription					subpassDescription			=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
		1u,									// deUint32							inputAttachmentCount;
		&inputAttachmentReference,			// const VkAttachmentReference*		pInputAttachments;
		1u,									// deUint32							colorAttachmentCount;
		&colorAttachmentReference,			// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,							// const VkAttachmentReference*		pResolveAttachments;
		&depthAttachmentReference,			// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,									// deUint32							preserveAttachmentCount;
		DE_NULL								// const deUint32*					pPreserveAttachments;
	};
	vector <VkSubpassDescription>				subpassDescriptions			(subpassCount, subpassDescription);

	const VkRenderPassMultiviewCreateInfo	renderPassMultiviewInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO_KHR,	//VkStructureType	sType;
		DE_NULL,													//const void*		pNext;
		subpassCount,												//uint32_t			subpassCount;
		&viewMasks[0],												//const uint32_t*	pViewMasks;
		0u,															//uint32_t			dependencyCount;
		DE_NULL,													//const int32_t*	pViewOffsets;
		0u,															//uint32_t			correlationMaskCount;
		DE_NULL,													//const uint32_t*	pCorrelationMasks;
	};

	vector <VkSubpassDependency>				subpassDependencies;
	for(deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
	{
		const VkSubpassDependency subpassDependency =
		{
			subpassNdx,														// deUint32				srcSubpass;
			(subpassNdx ==subpassCount - 1u) ? subpassNdx : subpassNdx+1u,	// deUint32				dstSubpass;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,					// VkPipelineStageFlags	srcStageMask;
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,							// VkPipelineStageFlags	dstStageMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,							// VkAccessFlags		srcAccessMask;
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,							// VkAccessFlags		dstAccessMask;
			VK_DEPENDENCY_VIEW_LOCAL_BIT_KHR,								// VkDependencyFlags	dependencyFlags;
		};
		subpassDependencies.push_back(subpassDependency);
	}

	const VkRenderPassCreateInfo				renderPassInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		&renderPassMultiviewInfo,					// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		2u,											// deUint32							attachmentCount;
		&attachments[0],							// const VkAttachmentDescription*	pAttachments;
		subpassCount,								// deUint32							subpassCount;
		&subpassDescriptions[0],					// const VkSubpassDescription*		pSubpasses;
		subpassCount,								// deUint32							dependencyCount;
		&subpassDependencies[0]						// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

void beginCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	const VkCommandBufferBeginInfo info =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,		// VkStructureType							sType;
		DE_NULL,											// const void*								pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,		// VkCommandBufferUsageFlags				flags;
		DE_NULL,											// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &info));
}

void beginSecondaryCommandBuffer (const DeviceInterface&				vk,
								   const VkCommandBuffer				commandBuffer,
								   const VkRenderPass					renderPass,
								   const deUint32						subpass,
								   const VkFramebuffer					framebuffer)
{
	const VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	//VkStructureType				sType;
		DE_NULL,											//const void*					pNext;
		renderPass,											//VkRenderPass					renderPass;
		subpass,											//deUint32						subpass;
		framebuffer,										//VkFramebuffer					framebuffer;
		VK_FALSE,											//VkBool32						occlusionQueryEnable;
		(VkQueryControlFlags)0u,							//VkQueryControlFlags			queryFlags;
		(VkQueryPipelineStatisticFlags)0u,					//VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	const VkCommandBufferBeginInfo			info					=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,		// VkStructureType							sType;
		DE_NULL,											// const void*								pNext;
		VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,	// VkCommandBufferUsageFlags				flags;
		&secCmdBufInheritInfo,							// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &info));
}

void imageBarrier (const DeviceInterface&			vk,
				   const VkCommandBuffer			cmdBuffer,
				   const VkImage					image,
				   const VkImageSubresourceRange	subresourceRange,
				   const VkImageLayout				oldLayout,
				   const VkImageLayout				newLayout,
				   const VkAccessFlags				srcAccessMask,
				   const VkAccessFlags				dstAccessMask,
				   const VkPipelineStageFlags		srcStageMask,
				   const VkPipelineStageFlags		dstStageMask)
{
	const VkImageMemoryBarrier		barrier				=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		srcAccessMask,							// VkAccessFlags			srcAccessMask;
		dstAccessMask,							// VkAccessFlags			dstAccessMask;
		oldLayout,								// VkImageLayout			oldLayout;
		newLayout,								// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
		image,									// VkImage					image;
		subresourceRange,						// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL,
		0u, (const VkBufferMemoryBarrier*)DE_NULL,
		1u, &barrier);
}

void submitCommandsAndWait (const DeviceInterface&	vk,
							const VkDevice			device,
							const VkQueue			queue,
							const VkCommandBuffer	commandBuffer)
{
	const VkFenceCreateInfo	fenceInfo	=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		(VkFenceCreateFlags)0,					// VkFenceCreateFlags	flags;
	};
	const Unique<VkFence>	fence		(createFence(vk, device, &fenceInfo));

	const VkSubmitInfo		submitInfo	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		DE_NULL,						// const void*					pNext;
		0u,								// uint32_t						waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*			pWaitSemaphores;
		DE_NULL,						// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// uint32_t						commandBufferCount;
		&commandBuffer,					// const VkCommandBuffer*		pCommandBuffers;
		0u,								// uint32_t						signalSemaphoreCount;
		DE_NULL,						// const VkSemaphore*			pSignalSemaphores;
	};
	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

} // MultiView
} // vkt
