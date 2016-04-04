/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright 2014 The Android Open Source Project
 * Copyright (c) 2015 The Khronos Group Inc.
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
 * \brief Early fragment tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineEarlyFragmentTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"

#include <string>

using namespace vk;

namespace vkt
{
namespace pipeline
{
namespace
{

// \note Some utility functions are general, but others are custom tailored to this test.

class Buffer
{
public:
									Buffer			(const DeviceInterface&		vk,
													 const VkDevice				device,
													 Allocator&					allocator,
													 const VkBufferCreateInfo&	bufferCreateInfo,
													 const MemoryRequirement	memoryRequirement)

										: m_buffer		(createBuffer(vk, device, &bufferCreateInfo))
										, m_allocation	(allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement))
									{
										VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
									}

	const VkBuffer&					get				(void) const { return *m_buffer; }
	const VkBuffer&					operator*		(void) const { return get(); }
	Allocation&						getAllocation	(void) const { return *m_allocation; }

private:
	const Unique<VkBuffer>			m_buffer;
	const de::UniquePtr<Allocation>	m_allocation;

	// "deleted"
									Buffer			(const Buffer&);
	Buffer&							operator=		(const Buffer&);
};

class Image
{
public:
									Image			(const DeviceInterface&		vk,
													 const VkDevice				device,
													 Allocator&					allocator,
													 const VkImageCreateInfo&	imageCreateInfo,
													 const MemoryRequirement	memoryRequirement)

										: m_image		(createImage(vk, device, &imageCreateInfo))
										, m_allocation	(allocator.allocate(getImageMemoryRequirements(vk, device, *m_image), memoryRequirement))
									{
										VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));
									}

	const VkImage&					get				(void) const { return *m_image; }
	const VkImage&					operator*		(void) const { return get(); }
	Allocation&						getAllocation	(void) const { return *m_allocation; }

private:
	const Unique<VkImage>			m_image;
	const de::UniquePtr<Allocation>	m_allocation;

	// "deleted"
									Image			(const Image&);
	Image&							operator=		(const Image&);
};

Move<VkImageView> makeImageView (const DeviceInterface&			vk,
								 const VkDevice					vkDevice,
								 const VkImage					image,
								 const VkImageViewType			imageViewType,
								 const VkFormat					format,
								 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo imageViewParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkImageViewCreateFlags)0,						// VkImageViewCreateFlags	flags;
		image,											// VkImage					image;
		imageViewType,									// VkImageViewType			viewType;
		format,											// VkFormat					format;
		makeComponentMappingRGBA(),						// VkComponentMapping		components;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return createImageView(vk, vkDevice, &imageViewParams);
}

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize			bufferSize,
										 const VkBufferUsageFlags	usage)
{
	const VkBufferCreateInfo bufferCreateInfo =
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

Move<VkDescriptorSet> makeDescriptorSet (const DeviceInterface&			vk,
										 const VkDevice					device,
										 const VkDescriptorPool			descriptorPool,
										 const VkDescriptorSetLayout	setLayout)
{
	const VkDescriptorSetAllocateInfo allocateParams =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		descriptorPool,										// VkDescriptorPool				descriptorPool;
		1u,													// deUint32						setLayoutCount;
		&setLayout,											// const VkDescriptorSetLayout*	pSetLayouts;
	};
	return allocateDescriptorSet(vk, device, &allocateParams);
}

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkDescriptorSetLayout	descriptorSetLayout)
{
	const VkPipelineLayoutCreateInfo pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							setLayoutCount;
		&descriptorSetLayout,								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};
	return createPipelineLayout(vk, device, &pipelineLayoutParams);
}

Move<VkCommandPool> makeCommandPool (const DeviceInterface& vk, const VkDevice device, const deUint32 queueFamilyIndex)
{
	const VkCommandPoolCreateInfo commandPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCommandPoolCreateFlags	flags;
		queueFamilyIndex,									// deUint32					queueFamilyIndex;
	};
	return createCommandPool(vk, device, &commandPoolParams);
}

Move<VkCommandBuffer> makeCommandBuffer (const DeviceInterface& vk, const VkDevice device, const VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo bufferAllocateParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		commandPool,										// VkCommandPool			commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// VkCommandBufferLevel		level;
		1u,													// deUint32					bufferCount;
	};
	return allocateCommandBuffer(vk, device, &bufferAllocateParams);
}

void beginCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		(VkCommandBufferUsageFlags)0,					// VkCommandBufferUsageFlags		flags;
		DE_NULL,										// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &commandBufBeginParams));
}

void endCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	VK_CHECK(vk.endCommandBuffer(commandBuffer));
}

void submitCommandsAndWait (const DeviceInterface&	vk,
							const VkDevice			device,
							const VkQueue			queue,
							const VkCommandBuffer	commandBuffer)
{
	const VkFenceCreateInfo	fenceParams =
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		(VkFenceCreateFlags)0,					// VkFenceCreateFlags	flags;
	};
	const Unique<VkFence> fence(createFence(vk, device, &fenceParams));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		0u,									// deUint32					waitSemaphoreCount;
		DE_NULL,							// const VkSemaphore*		pWaitSemaphores;
		DE_NULL,							// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,									// deUint32					commandBufferCount;
		&commandBuffer,						// const VkCommandBuffer*	pCommandBuffers;
		0u,									// deUint32					signalSemaphoreCount;
		DE_NULL,							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

VkImageMemoryBarrier makeImageMemoryBarrier	(const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		srcAccessMask,									// VkAccessFlags			outputMask;
		dstAccessMask,									// VkAccessFlags			inputMask;
		oldLayout,										// VkImageLayout			oldLayout;
		newLayout,										// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes)
{
	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		srcAccessMask,								// VkAccessFlags	srcAccessMask;
		dstAccessMask,								// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			destQueueFamilyIndex;
		buffer,										// VkBuffer			buffer;
		offset,										// VkDeviceSize		offset;
		bufferSizeBytes,							// VkDeviceSize		size;
	};
	return barrier;
}

//! Basic 2D image.
inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		(VkImageCreateFlags)0,									// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
		format,													// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),					// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		1u,														// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
		usage,													// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};
	return imageParams;
}

void beginRenderPass (const DeviceInterface&	vk,
					  const VkCommandBuffer		commandBuffer,
					  const VkRenderPass		renderPass,
					  const VkFramebuffer		framebuffer,
					  const VkRect2D&			renderArea,
					  const tcu::Vec4&			clearColor,
					  const float				clearDepth,
					  const deUint32			clearStencil)
{
	const VkClearValue clearValues[] =
	{
		makeClearValueColor(clearColor),						// attachment 0
		makeClearValueDepthStencil(clearDepth, clearStencil),	// attachment 1
	};

	const VkRenderPassBeginInfo renderPassBeginInfo = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType         sType;
		DE_NULL,										// const void*             pNext;
		renderPass,										// VkRenderPass            renderPass;
		framebuffer,									// VkFramebuffer           framebuffer;
		renderArea,										// VkRect2D                renderArea;
		DE_LENGTH_OF_ARRAY(clearValues),				// uint32_t                clearValueCount;
		clearValues,									// const VkClearValue*     pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void endRenderPass (const DeviceInterface&	vk,
					const VkCommandBuffer	commandBuffer)
{
	vk.cmdEndRenderPass(commandBuffer);
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&	vk,
								   const VkDevice			device,
								   const VkFormat			colorFormat,
								   const bool				useDepthStencilAttachment,
								   const VkFormat			depthStencilFormat)
{
	const VkAttachmentDescription attachments[] =
	{
		// color
		{
			(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
			colorFormat,										// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					finalLayout;
		},
		// depth/stencil
		{
			(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
			depthStencilFormat,									// VkFormat							format;
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout					finalLayout;
		}
	};

	const VkAttachmentReference unusedAttachmentReference =
	{
		VK_ATTACHMENT_UNUSED,								// deUint32			attachment;
		VK_IMAGE_LAYOUT_UNDEFINED							// VkImageLayout	layout;
	};

	const VkAttachmentReference colorAttachmentReference =
	{
		0u,													// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout	layout;
	};

	const VkAttachmentReference depthStencilAttachmentReference =
	{
		1u,													// deUint32			attachment;
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference* pDepthStencilAttachment = (useDepthStencilAttachment ? &depthStencilAttachmentReference : &unusedAttachmentReference);

	const VkSubpassDescription subpassDescription =
	{
		0u,													// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		0u,													// deUint32							inputAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
		1u,													// deUint32							colorAttachmentCount;
		&colorAttachmentReference,							// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments;
		pDepthStencilAttachment,							// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0,							// VkRenderPassCreateFlags			flags;
		(useDepthStencilAttachment ? 2u : 1u),				// deUint32							attachmentCount;
		attachments,										// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&		vk,
									 const VkDevice				device,
									 const VkRenderPass			renderPass,
									 const deUint32				attachmentCount,
									 const VkImageView*			pAttachments,
									 const tcu::IVec2			size)
{
	const VkFramebufferCreateInfo framebufferInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,										// const void*                                 pNext;
		(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags                    flags;
		renderPass,										// VkRenderPass                                renderPass;
		attachmentCount,								// uint32_t                                    attachmentCount;
		pAttachments,									// const VkImageView*                          pAttachments;
		static_cast<deUint32>(size.x()),				// uint32_t                                    width;
		static_cast<deUint32>(size.y()),				// uint32_t                                    height;
		1u,												// uint32_t                                    layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&	vk,
									   const VkDevice			device,
									   const VkPipelineLayout	pipelineLayout,
									   const VkRenderPass		renderPass,
									   const VkShaderModule		vertexModule,
									   const VkShaderModule		fragmentModule,
									   const tcu::IVec2&		renderSize,
									   const bool				enableDepthTest,
									   const bool				enableStencilTest)
{
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,								// uint32_t				binding;
		sizeof(tcu::Vec4),				// uint32_t				stride;		// Vertex is a 4-element vector XYZW, position only
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescription =
	{
		0u,									// uint32_t			location;
		0u,									// uint32_t			binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,		// VkFormat			format;
		0u,									// uint32_t			offset;
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		1u,															// uint32_t                                    vertexBindingDescriptionCount;
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		1u,															// uint32_t                                    vertexAttributeDescriptionCount;
		&vertexInputAttributeDescription,							// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags     flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology                         topology;
		VK_FALSE,														// VkBool32                                    primitiveRestartEnable;
	};

	const VkViewport viewport = makeViewport(
		0.0f, 0.0f,
		static_cast<float>(renderSize.x()), static_cast<float>(renderSize.y()),
		0.0f, 1.0f);

	const VkRect2D scissor = {
		makeOffset2D(0, 0),
		makeExtent2D(renderSize.x(), renderSize.y()),
	};

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,												// const void*                                 pNext;
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags          flags;
		1u,														// uint32_t                                    viewportCount;
		&viewport,												// const VkViewport*                           pViewports;
		1u,														// uint32_t                                    scissorCount;
		&scissor,												// const VkRect2D*                             pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags  flags;
		VK_FALSE,														// VkBool32                                 depthClampEnable;
		VK_FALSE,														// VkBool32                                 rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};

	const VkStencilOpState stencilOpState = makeStencilOpState(
		VK_STENCIL_OP_KEEP,		// stencil fail
		VK_STENCIL_OP_KEEP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_EQUAL,	// compare op
		1u,						// compare mask
		1u,						// write mask
		1u);					// reference

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
		enableDepthTest,											// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		enableStencilTest,											// VkBool32									stencilTestEnable;
		stencilOpState,												// VkStencilOpState							front;
		stencilOpState,												// VkStencilOpState							back;
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// Number of blend attachments must equal the number of color attachments.
	const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState =
	{
		VK_FALSE,							// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ONE,				// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,				// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,					// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,				// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,				// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,					// VkBlendOp				alphaBlendOp;
		colorComponentsAll,					// VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&pipelineColorBlendAttachmentState,							// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
	};

	const VkPipelineShaderStageCreateInfo pShaderStages[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage;
			vertexModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits				stage;
			fragmentModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		}
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		DE_NULL,											// const void*										pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
		DE_LENGTH_OF_ARRAY(pShaderStages),					// deUint32											stageCount;
		pShaderStages,										// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,								// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,					// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,					// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,						// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,						// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,										// VkPipelineLayout									layout;
		renderPass,											// VkRenderPass										renderPass;
		0u,													// deUint32											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		0,													// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

VkBufferImageCopy makeBufferImageCopy (const VkImageAspectFlags aspectFlags, const tcu::IVec2& renderSize)
{
	const VkBufferImageCopy copyParams =
	{
		0ull,															//	VkDeviceSize				bufferOffset;
		0u,																//	deUint32					bufferRowLength;
		0u,																//	deUint32					bufferImageHeight;
		makeImageSubresourceLayers(aspectFlags, 0u, 0u, 1u),			//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, 0),											//	VkOffset3D					imageOffset;
		makeExtent3D(renderSize.x(), renderSize.y(), 1u),				//	VkExtent3D					imageExtent;
	};
	return copyParams;
}

void commandClearStencilAttachment (const DeviceInterface&	vk,
									const VkCommandBuffer	commandBuffer,
									const VkOffset2D&		offset,
									const VkExtent2D&		extent,
									const deUint32			clearValue)
{
	const VkClearAttachment stencilAttachment =
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,					// VkImageAspectFlags    aspectMask;
		0u,												// uint32_t              colorAttachment;
		makeClearValueDepthStencil(0.0f, clearValue),	// VkClearValue          clearValue;
	};

	const VkClearRect rect =
	{
		{ offset, extent },		// VkRect2D    rect;
		0u,						// uint32_t    baseArrayLayer;
		1u,						// uint32_t    layerCount;
	};

	vk.cmdClearAttachments(commandBuffer, 1u, &stencilAttachment, 1u, &rect);
}

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, const VkPhysicalDevice device, const VkFormat format)
{
	VkFormatProperties formatProps;
	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat pickSupportedDepthStencilFormat (const InstanceInterface&	instanceInterface,
										  const VkPhysicalDevice	device,
										  const deUint32			numFormats,
										  const VkFormat*			pFormats)
{
	for (deUint32 i = 0; i < numFormats; ++i)
		if (isSupportedDepthStencilFormat(instanceInterface, device, pFormats[i]))
			return pFormats[i];
	return VK_FORMAT_UNDEFINED;
}

enum Flags
{
	FLAG_TEST_DEPTH							= 1u << 0,
	FLAG_TEST_STENCIL						= 1u << 1,
	FLAG_DONT_USE_TEST_ATTACHMENT			= 1u << 2,
	FLAG_DONT_USE_EARLY_FRAGMENT_TESTS		= 1u << 3,
};

class EarlyFragmentTest : public TestCase
{
public:
						EarlyFragmentTest	(tcu::TestContext&		testCtx,
											 const std::string		name,
											 const deUint32			flags);

	void				initPrograms		(SourceCollections&		programCollection) const;
	TestInstance*		createInstance		(Context&				context) const;

private:
	const deUint32		m_flags;
};

EarlyFragmentTest::EarlyFragmentTest (tcu::TestContext& testCtx, const std::string name, const deUint32 flags)
	: TestCase	(testCtx, name, "")
	, m_flags	(flags)
{
}

void EarlyFragmentTest::initPrograms (SourceCollections& programCollection) const
{
	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		const bool useEarlyTests = (m_flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0;
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< (useEarlyTests ? "layout(early_fragment_tests) in;\n" : "")
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "layout(binding = 0) coherent buffer Output {\n"
			<< "    uint result;\n"
			<< "} sb_out;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    atomicAdd(sb_out.result, 1u);\n"
			<< "	fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

class EarlyFragmentTestInstance : public TestInstance
{
public:
							EarlyFragmentTestInstance (Context& context, const deUint32 flags);

	tcu::TestStatus			iterate					  (void);

private:
	enum TestMode
	{
		MODE_INVALID,
		MODE_DEPTH,
		MODE_STENCIL,
	};

	const TestMode			m_testMode;
	const bool				m_useTestAttachment;
	const bool				m_useEarlyTests;
};

EarlyFragmentTestInstance::EarlyFragmentTestInstance (Context& context, const deUint32 flags)
	: TestInstance			(context)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
{
	DE_ASSERT(m_testMode != MODE_INVALID);
}

tcu::TestStatus EarlyFragmentTestInstance::iterate (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	// Color attachment

	const tcu::IVec2			  renderSize			= tcu::IVec2(32, 32);
	const VkFormat				  colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Image					  colorImage			(vk, device, allocator, makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::Any);
	const Unique<VkImageView>	  colorImageView		(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Test attachment (depth or stencil)
	static const VkFormat stencilFormats[] =
	{
		// One of the following formats must be supported, as per spec requirement.
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const VkFormat testFormat = (m_testMode == MODE_STENCIL ? pickSupportedDepthStencilFormat(vki, physDevice, DE_LENGTH_OF_ARRAY(stencilFormats), stencilFormats)
															: VK_FORMAT_D16_UNORM);		// spec requires this format to be supported
	if (testFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("Required depth/stencil format not supported");

	if (m_useTestAttachment)
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth/stencil format " << getFormatName(testFormat) << tcu::TestLog::EndMessage;

	const VkImageSubresourceRange testSubresourceRange	  = makeImageSubresourceRange(getImageAspectFlags(testFormat), 0u, 1u, 0u, 1u);
	const Image					  testImage				  (vk, device, allocator, makeImageCreateInfo(renderSize, testFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), MemoryRequirement::Any);
	const Unique<VkImageView>	  testImageView			  (makeImageView(vk, device, *testImage, VK_IMAGE_VIEW_TYPE_2D, testFormat, testSubresourceRange));
	const VkImageView			  attachmentImages[]	  = { *colorImageView, *testImageView };
	const deUint32				  numUsedAttachmentImages = (m_useTestAttachment ? 2u : 1u);

	// Vertex buffer

	const deUint32	   numVertices			 = 6;
	const VkDeviceSize vertexBufferSizeBytes = sizeof(tcu::Vec4) * numVertices;
	const Buffer	   vertexBuffer			 (vk, device, allocator, makeBufferCreateInfo(vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible);

	{
		const Allocation& alloc = vertexBuffer.getAllocation();
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(alloc.getHostPtr());

		pVertices[0] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[2] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);

		pVertices[3] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);
		pVertices[4] = tcu::Vec4( 1.0f,  1.0f,  1.0f,  1.0f);
		pVertices[5] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);

		flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), vertexBufferSizeBytes);
		// No barrier needed, flushed memory is automatically visible
	}

	// Result buffer

	const VkDeviceSize resultBufferSizeBytes = sizeof(deUint32);
	const Buffer resultBuffer(vk, device, allocator, makeBufferCreateInfo(resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	{
		const Allocation& alloc = resultBuffer.getAllocation();
		deUint32* const pData = static_cast<deUint32*>(alloc.getHostPtr());

		*pData = 0;
		flushMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), resultBufferSizeBytes);
	}

	// Render result buffer (to retrieve color attachment contents)

	const VkDeviceSize colorBufferSizeBytes = tcu::getPixelSize(mapVkFormat(colorFormat)) * renderSize.x() * renderSize.y();
	const Buffer	   colorBuffer			(vk, device, allocator, makeBufferCreateInfo(colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet				 (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  resultBufferDescriptorInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDescriptorInfo)
		.update(vk, device);

	// Pipeline

	const Unique<VkShaderModule>	vertexModule  (createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass	  (makeRenderPass(vk, device, colorFormat, m_useTestAttachment, testFormat));
	const Unique<VkFramebuffer>		framebuffer	  (makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages, renderSize));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline	  (makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, renderSize,
												  (m_testMode == MODE_DEPTH), (m_testMode == MODE_STENCIL)));
	const Unique<VkCommandPool>		cmdPool		  (makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer	  (makeCommandBuffer(vk, device, *cmdPool));

	// Draw commands

	{
		const VkRect2D renderArea = {
			makeOffset2D(0, 0),
			makeExtent2D(renderSize.x(), renderSize.y()),
		};
		const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
		const VkDeviceSize vertexBufferOffset = 0ull;

		beginCommandBuffer(vk, *cmdBuffer);

		{
			const VkImageMemoryBarrier barriers[] = {
				makeImageMemoryBarrier(
					0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					*colorImage, colorSubresourceRange),
				makeImageMemoryBarrier(
					0u, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					*testImage, testSubresourceRange),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		// Will clear the attachments with specified depth and stencil values.
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor, 0.5f, 0u);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_useTestAttachment && m_testMode == MODE_STENCIL)
			commandClearStencilAttachment(vk, *cmdBuffer, makeOffset2D(0, 0), makeExtent2D(renderSize.x()/2, renderSize.y()), 1u);

		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		{
			const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, resultBufferSizeBytes);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
				0u, DE_NULL, 1u, &shaderWriteBarrier, 0u, DE_NULL);

			const VkImageMemoryBarrier preCopyColorImageBarrier = makeImageMemoryBarrier(
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				*colorImage, colorSubresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyColorImageBarrier);

			const VkBufferImageCopy copyRegion = makeBufferImageCopy(VK_IMAGE_ASPECT_COLOR_BIT, renderSize);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &copyRegion);

			const VkBufferMemoryBarrier postCopyColorBufferBarrier = makeBufferMemoryBarrier(
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, colorBufferSizeBytes);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
				0u, DE_NULL, 1u, &postCopyColorBufferBarrier, 0u, DE_NULL);
		}

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Log result image
	{
		const Allocation& alloc = colorBuffer.getAllocation();
		invalidateMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), colorBufferSizeBytes);

		const tcu::ConstPixelBufferAccess imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, alloc.getHostPtr());

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Image("color0", "Rendered image", imagePixelAccess);
	}

	// Verify results
	{
		const Allocation& alloc = resultBuffer.getAllocation();
		invalidateMappedMemoryRange(vk, device, alloc.getMemory(), alloc.getOffset(), resultBufferSizeBytes);

		const int  actualCounter	   = *static_cast<deInt32*>(alloc.getHostPtr());
		const bool expectPartialResult = (m_useEarlyTests && m_useTestAttachment);
		const int  expectedCounter	   = expectPartialResult ? renderSize.x() * renderSize.y() / 2 : renderSize.x() * renderSize.y();
		const int  tolerance		   = expectPartialResult ? de::max(renderSize.x(), renderSize.y()) * 3	: 0;
		const int  expectedMin         = de::max(0, expectedCounter - tolerance);
		const int  expectedMax		   = expectedCounter + tolerance;

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Expected value"
			<< (expectPartialResult ? " in range: [" + de::toString(expectedMin) + ", " + de::toString(expectedMax) + "]" : ": " + de::toString(expectedCounter))
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Result value: " << de::toString(actualCounter) << tcu::TestLog::EndMessage;

		if (expectedMin <= actualCounter && actualCounter <= expectedMax)
			return tcu::TestStatus::pass("Success");
		else
			return tcu::TestStatus::fail("Value out of range");
	}
}

TestInstance* EarlyFragmentTest::createInstance (Context& context) const
{
	// Check required features
	{
		VkPhysicalDeviceFeatures features;
		context.getInstanceInterface().getPhysicalDeviceFeatures(context.getPhysicalDevice(), &features);

		// SSBO writes in fragment shader
		if (!features.fragmentStoresAndAtomics)
			throw tcu::NotSupportedError("Missing required feature: fragmentStoresAndAtomics");
	}

	return new EarlyFragmentTestInstance(context, m_flags);
}

} // anonymous ns

tcu::TestCaseGroup* createEarlyFragmentTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "early_fragment", "early fragment test cases"));

	static const struct
	{
		std::string caseName;
		deUint32	flags;
	} cases[] =
	{
		{ "no_early_fragment_tests_depth",					FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
		{ "no_early_fragment_tests_stencil",				FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
		{ "early_fragment_tests_depth",						FLAG_TEST_DEPTH																			},
		{ "early_fragment_tests_stencil",					FLAG_TEST_STENCIL																		},
		{ "no_early_fragment_tests_depth_no_attachment",	FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
		{ "no_early_fragment_tests_stencil_no_attachment",	FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
		{ "early_fragment_tests_depth_no_attachment",		FLAG_TEST_DEPTH   |										 FLAG_DONT_USE_TEST_ATTACHMENT  },
		{ "early_fragment_tests_stencil_no_attachment",		FLAG_TEST_STENCIL |										 FLAG_DONT_USE_TEST_ATTACHMENT	},
	};

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
		testGroup->addChild(new EarlyFragmentTest(testCtx, cases[i].caseName, cases[i].flags));

	return testGroup.release();
}

} // pipeline
} // vkt
