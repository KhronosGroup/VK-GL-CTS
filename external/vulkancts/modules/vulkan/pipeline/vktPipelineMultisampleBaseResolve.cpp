/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2016 The Khronos Group Inc.
* Copyright (c) 2023 LunarG, Inc.
* Copyright (c) 2023 Nintendo
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
*//*
* \file vktPipelineMultisampleBaseResolve.cpp
* \brief Base class for tests that check results of multisample resolve
*//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleBaseResolve.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "tcuTestLog.hpp"
#include <vector>

namespace vkt
{
namespace pipeline
{
namespace multisample
{

using namespace vk;

tcu::TestStatus MSInstanceBaseResolve::iterate (void)
{
	// cases creating this tests are defined using templates and we do not have easy access
	// to image type - to do this check in checkSupport bigger reffactoring would be needed
#ifndef CTS_USES_VULKANSC
	if (m_context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!m_context.getPortabilitySubsetFeatures().multisampleArrayImage &&
		(m_imageType == IMAGE_TYPE_2D_ARRAY) &&
		(m_imageMSParams.numSamples != VK_SAMPLE_COUNT_1_BIT) &&
		(m_imageMSParams.imageSize.z() != 1))
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Implementation does not support image array with multiple samples per texel");
	}
#endif // CTS_USES_VULKANSC

	const InstanceInterface&		instance			= m_context.getInstanceInterface();
	const DeviceInterface&			deviceInterface		= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkPhysicalDevice			physicalDevice		= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures&	features			= m_context.getDeviceFeatures();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const bool						usePushConstants	= (m_imageMSParams.componentData.source == ComponentSource::PUSH_CONSTANT);
	const deUint32					pushConstantSize	= static_cast<deUint32>(sizeof(decltype(m_imageMSParams.componentData.index)));

	VkImageCreateInfo				imageMSInfo;
	VkImageCreateInfo				imageRSInfo;

	// Check if image size does not exceed device limits
	validateImageSize(instance, physicalDevice, m_imageType, m_imageMSParams.imageSize);

	// Check if device supports image format as color attachment
	validateImageFeatureFlags(instance, physicalDevice, mapTextureFormat(m_imageFormat), VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

	imageMSInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageMSInfo.pNext					= DE_NULL;
	imageMSInfo.flags					= 0u;
	imageMSInfo.imageType				= mapImageType(m_imageType);
	imageMSInfo.format					= mapTextureFormat(m_imageFormat);
	imageMSInfo.extent					= makeExtent3D(getLayerSize(m_imageType, m_imageMSParams.imageSize));
	imageMSInfo.arrayLayers				= getNumLayers(m_imageType, m_imageMSParams.imageSize);
	imageMSInfo.mipLevels				= 1u;
	imageMSInfo.samples					= m_imageMSParams.numSamples;
	imageMSInfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	imageMSInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imageMSInfo.usage					= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageMSInfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imageMSInfo.queueFamilyIndexCount	= 0u;
	imageMSInfo.pQueueFamilyIndices		= DE_NULL;

	if (m_imageType == IMAGE_TYPE_CUBE || m_imageType == IMAGE_TYPE_CUBE_ARRAY)
	{
		imageMSInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	validateImageInfo(instance, physicalDevice, imageMSInfo);

	const de::UniquePtr<ImageWithMemory> imageMS(new ImageWithMemory(deviceInterface, device, allocator, imageMSInfo, MemoryRequirement::Any));

	imageRSInfo			= imageMSInfo;
	imageRSInfo.samples	= VK_SAMPLE_COUNT_1_BIT;

	validateImageInfo(instance, physicalDevice, imageRSInfo);

	const de::UniquePtr<ImageWithMemory> imageRS(new ImageWithMemory(deviceInterface, device, allocator, imageRSInfo, MemoryRequirement::Any));

	// Create render pass
	const VkAttachmentDescription attachmentMSDesc =
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
		imageMSInfo.format,							// VkFormat							format;
		imageMSInfo.samples,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription attachmentRSDesc =
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
		imageRSInfo.format,							// VkFormat							format;
		imageRSInfo.samples,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,			// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription attachments[] = { attachmentMSDesc, attachmentRSDesc };

	const VkAttachmentReference attachmentMSRef =
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference attachmentRSRef =
	{
		1u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference* resolveAttachment = m_imageMSParams.numSamples == VK_SAMPLE_COUNT_1_BIT ? DE_NULL : &attachmentRSRef;

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0u,						// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint;
		0u,													// deUint32							inputAttachmentCount;
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments;
		1u,													// deUint32							colorAttachmentCount;
		&attachmentMSRef,									// const VkAttachmentReference*		pColorAttachments;
		resolveAttachment,								// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,													// deUint32							preserveAttachmentCount;
		DE_NULL												// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0u,						// VkRenderPassCreateFlags			flags;
		2u,													// deUint32							attachmentCount;
		attachments,										// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	RenderPassWrapper renderPass (m_imageMSParams.pipelineConstructionType, deviceInterface, device, &renderPassInfo);

	const VkImageSubresourceRange fullImageRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, imageMSInfo.mipLevels, 0u, imageMSInfo.arrayLayers);

	// Create color attachments image views
	const Unique<VkImageView> imageMSView(makeImageView(deviceInterface, device, **imageMS, mapImageViewType(m_imageType), imageMSInfo.format, fullImageRange));
	const Unique<VkImageView> imageRSView(makeImageView(deviceInterface, device, **imageRS, mapImageViewType(m_imageType), imageMSInfo.format, fullImageRange));

	std::vector<VkImage> images = { **imageMS, **imageRS};
	const VkImageView attachmentsViews[] = { *imageMSView, *imageRSView };

	// Create framebuffer
	const VkFramebufferCreateInfo framebufferInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,									// const void*                                 pNext;
		(VkFramebufferCreateFlags)0u,				// VkFramebufferCreateFlags                    flags;
		*renderPass,								// VkRenderPass                                renderPass;
		2u,											// uint32_t                                    attachmentCount;
		attachmentsViews,							// const VkImageView*                          pAttachments;
		imageMSInfo.extent.width,					// uint32_t                                    width;
		imageMSInfo.extent.height,					// uint32_t                                    height;
		imageMSInfo.arrayLayers,					// uint32_t                                    layers;
	};

	renderPass.createFramebuffer(deviceInterface, device, &framebufferInfo, images);

	std::vector<vk::VkPushConstantRange>	pushConstantRanges;

	if (usePushConstants)
	{
		const vk::VkPushConstantRange pushConstantRange =
		{
			vk::VK_SHADER_STAGE_ALL,	// VkShaderStageFlags	stageFlags;
			0u,							// deUint32				offset;
			pushConstantSize,			// deUint32				size;
		};
		pushConstantRanges.push_back(pushConstantRange);
	}

	// Create pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,						// VkStructureType					sType;
		DE_NULL,															// const void*						pNext;
		(VkPipelineLayoutCreateFlags)0u,									// VkPipelineLayoutCreateFlags		flags;
		0u,																	// deUint32							setLayoutCount;
		DE_NULL,															// const VkDescriptorSetLayout*		pSetLayouts;
		static_cast<deUint32>(pushConstantRanges.size()),					// deUint32							pushConstantRangeCount;
		(pushConstantRanges.empty() ? nullptr : pushConstantRanges.data()),	// const VkPushConstantRange*		pPushConstantRanges;
	};

	const PipelineLayoutWrapper pipelineLayout(m_imageMSParams.pipelineConstructionType, deviceInterface, device, &pipelineLayoutParams);

	// Create vertex attributes data
	const VertexDataDesc vertexDataDesc = getVertexDataDescripton();

	de::SharedPtr<BufferWithMemory> vertexBuffer = de::SharedPtr<BufferWithMemory>(new BufferWithMemory(deviceInterface, device, allocator, makeBufferCreateInfo(vertexDataDesc.dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
	const Allocation& vertexBufferAllocation = vertexBuffer->getAllocation();

	uploadVertexData(vertexBufferAllocation, vertexDataDesc);

	flushAlloc(deviceInterface, device, vertexBufferAllocation);

	const VkVertexInputBindingDescription vertexBinding =
	{
		0u,							// deUint32				binding;
		vertexDataDesc.dataStride,	// deUint32				stride;
		VK_VERTEX_INPUT_RATE_VERTEX	// VkVertexInputRate	inputRate;
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,			// VkStructureType                             sType;
		DE_NULL,															// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0u,							// VkPipelineVertexInputStateCreateFlags       flags;
		1u,																	// uint32_t                                    vertexBindingDescriptionCount;
		&vertexBinding,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		static_cast<deUint32>(vertexDataDesc.vertexAttribDescVec.size()),	// uint32_t                                    vertexAttributeDescriptionCount;
		dataPointer(vertexDataDesc.vertexAttribDescVec),					// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const std::vector<VkViewport>	viewports	{ makeViewport(imageMSInfo.extent) };
	const std::vector<VkRect2D>		scissors	{ makeRect2D(imageMSInfo.extent) };

	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,						// VkPipelineMultisampleStateCreateFlags	flags;
		imageMSInfo.samples,											// VkSampleCountFlagBits					rasterizationSamples;
		features.sampleRateShading,										// VkBool32									sampleShadingEnable;
		1.0f,															// float									minSampleShading;
		DE_NULL,														// const VkSampleMask*						pSampleMask;
		VK_FALSE,														// VkBool32									alphaToCoverageEnable;
		VK_FALSE,														// VkBool32									alphaToOneEnable;
	};

	const ShaderWrapper vsModule(ShaderWrapper(deviceInterface, device, m_context.getBinaryCollection().get("vertex_shader"), (VkShaderModuleCreateFlags)0));
	const ShaderWrapper fsModule(ShaderWrapper(deviceInterface, device, m_context.getBinaryCollection().get("fragment_shader"), (VkShaderModuleCreateFlags)0));

	// Create graphics pipeline
	GraphicsPipelineWrapper graphicsPipeline(instance, deviceInterface, physicalDevice, device, m_context.getDeviceExtensions(), m_imageMSParams.pipelineConstructionType);
	graphicsPipeline.setDefaultRasterizationState()
					.setDefaultColorBlendState()
					.setDefaultDepthStencilState()
					.setDefaultTopology(vertexDataDesc.primitiveTopology)
					.setupVertexInputState(&vertexInputStateInfo)
					.setupPreRasterizationShaderState(viewports,
						scissors,
						pipelineLayout,
						*renderPass,
						0u,
						vsModule)
					.setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fsModule, DE_NULL, &multisampleStateInfo)
					.setupFragmentOutputState(*renderPass, 0, DE_NULL, &multisampleStateInfo)
					.setMonolithicPipelineLayout(pipelineLayout)
					.buildPipeline();

	// Create command buffer for compute and transfer oparations
	const Unique<VkCommandPool>	  commandPool(createCommandPool(deviceInterface, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  queueFamilyIndex));
	const Unique<VkCommandBuffer> commandBuffer(makeCommandBuffer(deviceInterface, device, *commandPool));

	// Start recording commands
	beginCommandBuffer(deviceInterface, *commandBuffer);

	{
		VkImageMemoryBarrier imageOutputAttachmentBarriers[2];

		imageOutputAttachmentBarriers[0] = makeImageMemoryBarrier
		(
			0u,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			**imageMS,
			fullImageRange
		);

		imageOutputAttachmentBarriers[1] = makeImageMemoryBarrier
		(
			0u,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			**imageRS,
			fullImageRange
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 2u, imageOutputAttachmentBarriers);
	}

	{
		const VkDeviceSize vertexStartOffset = 0u;

		std::vector<VkClearValue> clearValues;
		clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));
		clearValues.push_back(makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)));

		renderPass.begin(deviceInterface, *commandBuffer, makeRect2D(0, 0, imageMSInfo.extent.width, imageMSInfo.extent.height), (deUint32)clearValues.size(), &clearValues[0]);

		// Bind graphics pipeline
		graphicsPipeline.bind(*commandBuffer);

		// Bind vertex buffer
		deviceInterface.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer->get(), &vertexStartOffset);

		// Push constants.
		if (usePushConstants)
			deviceInterface.cmdPushConstants(*commandBuffer, *pipelineLayout, vk::VK_SHADER_STAGE_ALL, 0u, pushConstantSize, &m_imageMSParams.componentData.index);

		// Draw full screen quad
		deviceInterface.cmdDraw(*commandBuffer, vertexDataDesc.verticesCount, 1u, 0u, 0u);

		// End render pass
		renderPass.end(deviceInterface, *commandBuffer);
	}

	const VkImage sourceImage = m_imageMSParams.numSamples == VK_SAMPLE_COUNT_1_BIT ? **imageMS : **imageRS;

	{
		const VkImageMemoryBarrier imageTransferSrcBarrier = makeImageMemoryBarrier
		(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			sourceImage,
			fullImageRange
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageTransferSrcBarrier);
	}

	// Copy data from resolve image to buffer
	const deUint32				imageRSSizeInBytes = getImageSizeInBytes(imageRSInfo.extent, imageRSInfo.arrayLayers, m_imageFormat, imageRSInfo.mipLevels);

	const VkBufferCreateInfo				bufferRSInfo = makeBufferCreateInfo(imageRSSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const de::UniquePtr<BufferWithMemory>	bufferRS(new BufferWithMemory(deviceInterface, device, allocator, bufferRSInfo, MemoryRequirement::HostVisible));

	{
		const VkBufferImageCopy bufferImageCopy =
		{
			0u,																						//	VkDeviceSize				bufferOffset;
			0u,																						//	deUint32					bufferRowLength;
			0u,																						//	deUint32					bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, imageRSInfo.arrayLayers),	//	VkImageSubresourceLayers	imageSubresource;
			makeOffset3D(0, 0, 0),																	//	VkOffset3D					imageOffset;
			imageRSInfo.extent,																		//	VkExtent3D					imageExtent;
		};

		deviceInterface.cmdCopyImageToBuffer(*commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bufferRS->get(), 1u, &bufferImageCopy);
	}

	{
		const VkBufferMemoryBarrier bufferRSHostReadBarrier = makeBufferMemoryBarrier
		(
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT,
			bufferRS->get(),
			0u,
			imageRSSizeInBytes
		);

		deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferRSHostReadBarrier, 0u, DE_NULL);
	}

	// End recording commands
	endCommandBuffer(deviceInterface, *commandBuffer);

	// Submit commands for execution and wait for completion
	submitCommandsAndWait(deviceInterface, device, queue, *commandBuffer);

	// Retrieve data from buffer to host memory
	const Allocation& bufferRSAllocation = bufferRS->getAllocation();

	invalidateAlloc(deviceInterface, device, bufferRSAllocation);

	const tcu::ConstPixelBufferAccess bufferRSData (m_imageFormat,
													imageRSInfo.extent.width,
													imageRSInfo.extent.height,
													imageRSInfo.extent.depth * imageRSInfo.arrayLayers,
													bufferRSAllocation.getHostPtr());

	std::stringstream imageName;
	imageName << getImageTypeName(m_imageType) << "_" << bufferRSData.getWidth() << "_" << bufferRSData.getHeight() << "_" << bufferRSData.getDepth() << std::endl;

	m_context.getTestContext().getLog()
		<< tcu::TestLog::Section(imageName.str(), imageName.str())
		<< tcu::LogImage("image", "", bufferRSData)
		<< tcu::TestLog::EndSection;

	return verifyImageData(imageRSInfo, bufferRSData);
}

} // multisample
} // pipeline
} // vkt
