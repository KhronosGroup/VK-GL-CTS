/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
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
 * \brief Pipeline tests
 *//*--------------------------------------------------------------------*/

#include "vktApiPipelineTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "deUniquePtr.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"

namespace vkt
{
namespace api
{

namespace
{

using namespace std;
using namespace vk;

VkFormat getRenderTargetFormat (const InstanceInterface& vk, const VkPhysicalDevice& device)
{
	VkFormatProperties formatProperties;

	vk.getPhysicalDeviceFormatProperties(device, VK_FORMAT_B8G8R8A8_UNORM, &formatProperties);

	if (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT || formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
		return VK_FORMAT_B8G8R8A8_UNORM;

	vk.getPhysicalDeviceFormatProperties(device, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);

	if (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT || formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
		return VK_FORMAT_R8G8B8A8_UNORM;

	TCU_THROW(NotSupportedError, "Device does not support VK_FORMAT_B8G8R8A8_UNORM nor VK_FORMAT_R8G8B8A8_UNORM");

	return VK_FORMAT_UNDEFINED;
}

Move<VkCommandBuffer> createCommandBuffer (const DeviceInterface& vkd, VkDevice device, VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo allocateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType         sType;
		DE_NULL,											// const void*             pNext;
		commandPool,										// VkCommandPool           commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// VkCommandBufferLevel    level;
		1u													// deUint32                commandBufferCount;
	};

	return allocateCommandBuffer(vkd, device, &allocateInfo);
}

// This test has the same functionality as VkPositiveLayerTest.DestroyPipelineRenderPass
tcu::TestStatus renderpassLifetimeTest (Context& context)
{
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const InstanceInterface&						vki								= context.getInstanceInterface();
	const VkDevice									device							= context.getDevice();
	const VkPhysicalDevice							physicalDevice					= context.getPhysicalDevice();

	const VkFormat									format							= getRenderTargetFormat(vki, physicalDevice);
	const VkFormatProperties						formatProperties				(getPhysicalDeviceFormatProperties(vki, physicalDevice, format));
	const VkImageTiling								imageTiling						= (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_LINEAR
																					: (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_OPTIMAL
																					: VK_IMAGE_TILING_LAST;

	const VkImageCreateInfo							attachmentImageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType;
		DE_NULL,								// const void*              pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,						// VkImageType              imageType;
		format,									// VkFormat                 format;
		{
			256u,	// deUint32    width;
			256u,	// deUint32    height;
			1u		// deUint32    depth;
		},										// VkExtent3D               extent;
		1u,										// deUint32                 mipLevels;
		1u,										// deUint32                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples;
		imageTiling,							// VkImageTiling            tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode;
		0u,										// deUint32                 queueFamilyIndexCount;
		DE_NULL,								// const deUint32*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout;
	};

	const Unique<VkImage>							attachmentImage					(createImage(vk, device, &attachmentImageCreateInfo));
	de::MovePtr<Allocation>							attachmentImageMemory			= context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, device, *attachmentImage), MemoryRequirement::Any);

	VK_CHECK(vk.bindImageMemory(device, *attachmentImage, attachmentImageMemory->getMemory(), attachmentImageMemory->getOffset()));

	const deUint32									queueFamilyIndex				= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo					commandPoolParams				=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		(VkCommandPoolCreateFlags)0u,					// VkCommandPoolCreateFlags    flags;
		queueFamilyIndex								// deUint32                    queueFamilyIndex;
	};

	const Unique<VkCommandPool>						commandPool						(createCommandPool(vk, device, &commandPoolParams, DE_NULL));
	const Unique<VkCommandBuffer>					commandBuffer					(createCommandBuffer(vk, device, commandPool.get()));

	const VkCommandBufferBeginInfo					commandBufferBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags                flags;
		DE_NULL											// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	VK_CHECK(vk.beginCommandBuffer(commandBuffer.get(), &commandBufferBeginInfo));

	{
		const VkImageMemoryBarrier imageMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType            sType;
			DE_NULL,									// const void*                pNext;
			(VkAccessFlags)0u,							// VkAccessFlags              srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout              oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout              newLayout;
			0u,											// deUint32                   srcQueueFamilyIndex;
			0u,											// deUint32                   dstQueueFamilyIndex;
			attachmentImage.get(),						// VkImage                    image;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags    aspectMask;
				0u,								// deUint32              baseMipLevel;
				1u,								// deUint32              levelCount;
				0u,								// deUint32              baseArrayLayer;(
				1u								// deUint32              layerCount;
			}											// VkImageSubresourceRange    subresourceRange;
		};

		vk.cmdPipelineBarrier(commandBuffer.get(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlagBits)0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageMemoryBarrier);
	}

	const VkAttachmentDescription					attachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags    flags;
		format,										// VkFormat                        format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout;
	};

	const VkAttachmentReference						attachmentReference				=
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkSubpassDescription						subpassDescription				=
	{
		(VkSubpassDescriptionFlags)0u,		// VkSubpassDescriptionFlags       flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&attachmentReference,				// const VkAttachmentReference*    pColorAttachments
		DE_NULL,							// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		DE_NULL								// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo					renderPassCreateInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// VkStructureType                   sType;
		DE_NULL,										// const void*                       pNext;
		(VkRenderPassCreateFlags)0u,					// VkRenderPassCreateFlags           flags;
		1u,												// deUint32                          attachmentCount
		&attachmentDescription,							// const VkAttachmentDescription*    pAttachments
		1u,												// deUint32                          subpassCount
		&subpassDescription,							// const VkSubpassDescription*       pSubpasses
		0u,												// deUint32                          dependencyCount
		DE_NULL											// const VkSubpassDependency*        pDependencies
	};

	// Create two compatible renderpasses
	VkRenderPass									renderPassA;

	VK_CHECK(vk.createRenderPass(device, &renderPassCreateInfo, DE_NULL, &renderPassA));

	const Unique<VkRenderPass>						renderPassB						(createRenderPass(vk, device, &renderPassCreateInfo));

	const VkImageViewCreateInfo						attachmentImageViewCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType            sType;
		DE_NULL,										// const void*                pNext;
		(VkImageViewCreateFlags)0u,						// VkImageViewCreateFlags     flags;
		attachmentImage.get(),							// VkImage                    image;
		VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType            viewType;
		format,											// VkFormat                   format;
		{
			VK_COMPONENT_SWIZZLE_R,			// VkComponentSwizzle    r;
			VK_COMPONENT_SWIZZLE_G,			// VkComponentSwizzle    g;
			VK_COMPONENT_SWIZZLE_B,			// VkComponentSwizzle    b;
			VK_COMPONENT_SWIZZLE_A			// VkComponentSwizzle    a;
		},											// VkComponentMapping         components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags    aspectMask;
			0u,								// deUint32              baseMipLevel;
			1u,								// deUint32              levelCount;
			0u,								// deUint32              baseArrayLayer;
			1u								// deUint32              layerCount;
		}											// VkImageSubresourceRange    subresourceRange;
	};

	const Unique<VkImageView>						attachmentImageView				(createImageView(vk, device, &attachmentImageViewCreateInfo));

	const VkFramebufferCreateInfo					framebufferCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		(VkFramebufferCreateFlags)0u,					// VkFramebufferCreateFlags    flags;
		renderPassB.get(),								// VkRenderPass                renderPass;
		1u,												// deUint32                    attachmentCount;
		&attachmentImageView.get(),						// const VkImageView*          pAttachments;
		256u,											// deUint32                    width;
		256u,											// deUint32                    height;
		1u												// deUint32                    layers;
	};

	const Unique<VkFramebuffer>						frameBuffer						(createFramebuffer(vk, device, &framebufferCreateInfo));

	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0));

	const VkPipelineShaderStageCreateInfo			shaderStageCreateInfos[]		=
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,												// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0u,					// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits               stage;
			vertexShaderModule.get(),								// VkShaderModule                      shader;
			"main",													// const char*                         pName;
			DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,												// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0u,					// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits               stage;
			fragmentShaderModule.get(),								// VkShaderModule                      shader;
			"main",													// const char*                         pName;
			DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
		}
	};

	const VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType                     sType;
		DE_NULL,											// const void*                         pNext;
		(VkPipelineLayoutCreateFlags)0u,						// VkPipelineLayoutCreateFlags         flags;
		0u,													// deUint32                            setLayoutCount;
		DE_NULL,											// const VkDescriptorSetLayout*        pSetLayouts;
		0u,													// deUint32                            pushConstantRangeCount;
		DE_NULL												// const VkPushConstantRange*          pPushConstantRanges;
	};

	const Unique<VkPipelineLayout>					pipelineLayout					(createPipelineLayout(vk, device, &pipelineLayoutCreateInfo));

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0u,						// VkPipelineVertexInputStateCreateFlags       flags;
		0u,																// deUint32                                    vertexBindingDescriptionCount;
		DE_NULL,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		0u,																// deUint32                                    vertexAttributeDescriptionCount;
		DE_NULL															// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,														// const void*                                pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0u,					// VkPipelineInputAssemblyStateCreateFlags    flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology                        topology;
		VK_FALSE														// VkBool32                                   primitiveRestartEnable;
	};

	const VkViewport								viewport						=
	{
		0.0f,	// float    x;
		0.0f,	// float    y;
		0.0f,	// float    width;
		0.0f,	// float    height;
		0.0f,	// float    minDepth;
		0.0f,	// float    maxDepth;
	};

	const VkRect2D									scissor							=
	{
		{ 0,	0	},	// VkOffset2D    offset;
		{ 0u,	0u	}	// VkExtent2D    extent;
	};

	const VkPipelineViewportStateCreateInfo			viewPortStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,		// VkStructureType                       sType;
		DE_NULL,													// const void*                           pNext;
		(VkPipelineViewportStateCreateFlags)0u,						// VkPipelineViewportStateCreateFlags    flags;
		1u,															// deUint32                              viewportCount;
		&viewport,													// const VkViewport*                     pViewports;
		1u,															// deUint32                              scissorCount;
		&scissor													// const VkRect2D*                       pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                            sType;
		DE_NULL,														// const void*                                pNext;
		(VkPipelineRasterizationStateCreateFlags)0u,					// VkPipelineRasterizationStateCreateFlags    flags;
		VK_FALSE,														// VkBool32                                   depthClampEnable;
		VK_FALSE,														// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode                              polygonMode;
		VK_CULL_MODE_BACK_BIT | VK_CULL_MODE_FRONT_AND_BACK,			// VkCullModeFlags                            cullMode;
		VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace                                frontFace;
		VK_FALSE,														// VkBool32                                   depthBiasEnable;
		0.0f,															// float                                      depthBiasConstantFactor;
		0.0f,															// float                                      depthBiasClamp;
		0.0f,															// float                                      depthBiasSlopeFactor;
		1.0f															// float                                      lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,						// VkPipelineMultisampleStateCreateFlags    flags;
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits                    rasterizationSamples;
		VK_FALSE,														// VkBool32                                 sampleShadingEnable;
		0.0f,															// float                                    minSampleShading;
		DE_NULL,														// const VkSampleMask*                      pSampleMask;
		VK_FALSE,														// VkBool32                                 alphaToCoverageEnable;
		VK_FALSE														// VkBool32                                 alphaToOneEnable;
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
	{
		VK_FALSE,						// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp                alphaBlendOp;
		(VkColorComponentFlags)0xF,		// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType                               sType;
		DE_NULL,														// const void*                                   pNext;
		(VkPipelineColorBlendStateCreateFlags)0u,						// VkPipelineColorBlendStateCreateFlags          flags;
		VK_FALSE,														// VkBool32                                      logicOpEnable;
		VK_LOGIC_OP_CLEAR,												// VkLogicOp                                     logicOp;
		1u,																// deUint32                                      attachmentCount;
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*    pAttachments;
		{ 1.0f, 1.0f, 1.0f, 1.0f }										// float                                         blendConstants[4];
	};

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType                                  sType;
		DE_NULL,											// const void*                                      pNext;
		(VkPipelineCreateFlags)0u,							// VkPipelineCreateFlags                            flags;
		DE_LENGTH_OF_ARRAY(shaderStageCreateInfos),			// deUint32                                         stageCount;
		shaderStageCreateInfos,								// const VkPipelineShaderStageCreateInfo*           pStages;
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
		&viewPortStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*         pViewportState;
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
		&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
		&colorBlendStateCreateInfo,							// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
		pipelineLayout.get(),								// VkPipelineLayout                                 layout;
		renderPassA,										// VkRenderPass                                     renderPass;
		0u,													// deUint32                                         subpass;
		DE_NULL,											// VkPipeline                                       basePipelineHandle;
		0													// int                                              basePipelineIndex;
	};

	const Unique<VkPipeline>						graphicsPipeline				(createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo));

	const VkClearValue								clearValues						=
	{
		{ { 0.25f, 0.25f, 0.25f, 0.0f } },		// VkClearColorValue           color;
	};

	const VkRenderPassBeginInfo						renderPassBeginInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,		// VkStructureType        sType;
		DE_NULL,										// const void*            pNext;
		renderPassB.get(),								// VkRenderPass           renderPass;
		frameBuffer.get(),								// VkFramebuffer          framebuffer;
		{
			{ 0,	0		},		// VkOffset2D    offset;
			{ 256u,	256u	}		// VkExtent2D    extent;
		},												// VkRect2D               renderArea;
		1,												// deUint32               clearValueCount;
		&clearValues									// const VkClearValue*    pClearValues;
	};

	vk.cmdBeginRenderPass(commandBuffer.get(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vk.cmdBindPipeline(commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());

	// Destroy the renderpass that was used to create the graphics pipeline
	vk.destroyRenderPass(device, renderPassA, DE_NULL);

	vk.cmdDraw(commandBuffer.get(), 3u, 1u, 0u, 0u);

	vk.cmdEndRenderPass(commandBuffer.get());

	VK_CHECK(vk.endCommandBuffer(commandBuffer.get()));

	const VkSubmitInfo								submitInfo						=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType                sType;
		DE_NULL,							// const void*                    pNext;
		0u,									// deUint32                       waitSemaphoreCount;
		DE_NULL,							// const VkSemaphore*             pWaitSemaphores;
		DE_NULL,							// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,									// deUint32                       commandBufferCount;
		&commandBuffer.get(),				// const VkCommandBuffer*         pCommandBuffers;
		0u,									// deUint32                       signalSemaphoreCount;
		DE_NULL								// const VkSemaphore*             pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(context.getUniversalQueue(), 1, &submitInfo, DE_NULL));

	VK_CHECK(vk.queueWaitIdle(context.getUniversalQueue()));
	VK_CHECK(vk.deviceWaitIdle(device));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

void createDestroyPipelineRenderPassSource (SourceCollections& dst)
{
	dst.glslSources.add("vertex") << glu::VertexSource(
		"#version 310 es\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
		"}\n");

	dst.glslSources.add("fragment") << glu::FragmentSource(
		"#version 310 es\n"
		"layout (location = 0) out highp vec4 color;\n"
		"void main (void)\n"
		"{\n"
		"    color = vec4(1.0, 0.0, 1.0, 1.0);\n"
		"}\n");
}

tcu::TestCaseGroup* createrenderpassTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> renderPassTests(new tcu::TestCaseGroup(testCtx, "renderpass", "Renderpass tests"));

	addFunctionCaseWithPrograms(renderPassTests.get(), "destroy_pipeline_renderpass", "Draw after destroying the renderpass used to create a pipeline", createDestroyPipelineRenderPassSource, renderpassLifetimeTest);

	return renderPassTests.release();
}

} // anonymous

tcu::TestCaseGroup* createPipelineTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> descriptorSetTests(new tcu::TestCaseGroup(testCtx, "pipeline", "Descriptor set tests"));

	descriptorSetTests->addChild(createrenderpassTests(testCtx));

	return descriptorSetTests.release();
}

} // api
} // vkt
