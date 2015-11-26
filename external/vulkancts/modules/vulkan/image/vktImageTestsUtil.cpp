/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Mobica Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
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
 * \brief Image Tests Utility Classes
 *//*--------------------------------------------------------------------*/

#include "vktImageTestsUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"

using namespace vk;

namespace vkt
{
namespace image
{

Buffer::Buffer (const DeviceInterface&		vk,
				const VkDevice				device,
				Allocator&					allocator,
				const VkBufferCreateInfo&	bufferCreateInfo,
				const MemoryRequirement		memoryRequirement)
{
	m_buffer = createBuffer(vk, device, &bufferCreateInfo);
	m_allocation = allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement);
	VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
}

Image::Image (const DeviceInterface&	vk,
			  const VkDevice			device,
			  Allocator&				allocator,
			  const VkImageCreateInfo&	imageCreateInfo,
			  const MemoryRequirement	memoryRequirement)
{
	m_image = createImage(vk, device, &imageCreateInfo);
	m_allocation = allocator.allocate(getImageMemoryRequirements(vk, device, *m_image), memoryRequirement);
	VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));
}

VkBufferCreateInfo makeBufferCreateInfo (const VkDeviceSize			bufferSize,
										 const VkBufferUsageFlags	usage)
{
	const VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,								// const void*			pNext;
		0u,										// VkBufferCreateFlags	flags;
		bufferSize,								// VkDeviceSize			size;
		usage,									// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		DE_NULL,								// const deUint32*		pQueueFamilyIndices;
	};
	return bufferCreateInfo;
}

VkBufferImageCopy makeBufferImageCopy (const VkExtent3D extent, 
									   const deUint32	arraySize)
{
	const VkBufferImageCopy copyParams =
	{
		0ull,																		//	VkDeviceSize				bufferOffset;
		0u,																			//	deUint32					bufferRowLength;
		0u,																			//	deUint32					bufferImageHeight;
		makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, arraySize),	//	VkImageSubresourceLayers	imageSubresource;
		makeOffset3D(0, 0, 0),														//	VkOffset3D					imageOffset;
		extent,																		//	VkExtent3D					imageExtent;
	};
	return copyParams;
}

Move<VkCommandPool> makeCommandPool (const DeviceInterface& vk, const VkDevice device, const deUint32 queueFamilyIndex)
{
	const VkCommandPoolCreateInfo commandPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		queueFamilyIndex,									// deUint32					queueFamilyIndex;
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCommandPoolCreateFlags	flags;
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

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkDescriptorSetLayout	descriptorSetLayout)
{
	const VkPipelineLayoutCreateInfo pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							setLayoutCount;
		&descriptorSetLayout,								// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};
	return createPipelineLayout(vk, device, &pipelineLayoutParams);
}

Move<VkPipeline> makeComputePipeline (const DeviceInterface&	vk,
									  const VkDevice			device,
									  const VkPipelineLayout	pipelineLayout,
									  const VkShaderModule		shaderModule)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineShaderStageCreateFlags		flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
		shaderModule,											// VkShaderModule						module;
		"main",													// const char*							pName;
		DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,											// VkPipeline						basePipelineHandle;
		0,													// deInt32							basePipelineIndex;
	};
	return createComputePipeline(vk, device, DE_NULL , &pipelineCreateInfo);
}

Move<VkBufferView> makeBufferView (const DeviceInterface&	vk,
								   const VkDevice			vkDevice,
								   const VkBuffer			buffer,
								   const VkFormat			format,
								   const VkDeviceSize		offset,
								   const VkDeviceSize		size)
{
	const VkBufferViewCreateInfo bufferViewParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkBufferViewCreateFlags	flags;
		buffer,										// VkBuffer					buffer;
		format,										// VkFormat					format;
		offset,										// VkDeviceSize				offset;
		size,										// VkDeviceSize				range;
	};
	return createBufferView(vk, vkDevice, &bufferViewParams);
}

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
		0u,												// VkImageViewCreateFlags	flags;
		image,											// VkImage					image;
		imageViewType,									// VkImageViewType			viewType;
		format,											// VkFormat					format;
		makeComponentMappingRGBA(),						// VkComponentMapping		components;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return createImageView(vk, vkDevice, &imageViewParams);
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

void beginCommandBuffer (const DeviceInterface& vk, const VkCommandBuffer commandBuffer)
{
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	//	VkStructureType					sType;
		DE_NULL,										//	const void*						pNext;
		0u,												//	VkCommandBufferOptimizeFlags	flags;
		DE_NULL,										//	VkRenderPass					renderPass;
		0u,												//	deUint32						subpass;
		DE_NULL,										//	VkFramebuffer					framebuffer;
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
		0u,										// VkFenceCreateFlags	flags;
	};
	const Unique<VkFence> fence(createFence(vk, device, &fenceParams));

	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		0u,									// deUint32					waitSemaphoreCount;
		DE_NULL,							// const VkSemaphore*		pWaitSemaphores;
		1u,									// deUint32					commandBufferCount;
		&commandBuffer,						// const VkCommandBuffer*	pCommandBuffers;
		0u,									// deUint32					signalSemaphoreCount;
		DE_NULL,							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, *fence));
	VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
}

} // image
} // vkt
