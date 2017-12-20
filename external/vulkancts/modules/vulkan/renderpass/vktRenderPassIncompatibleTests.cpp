/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Tests incompatible render pass.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassIncompatibleTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"

#include "vkDefs.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

using namespace vk;

using tcu::UVec4;
using std::string;

namespace vkt
{
namespace
{

void bindImageMemory (const DeviceInterface& vk, VkDevice device, VkImage image, VkDeviceMemory mem, VkDeviceSize memOffset)
{
	VK_CHECK(vk.bindImageMemory(device, image, mem, memOffset));
}

de::MovePtr<Allocation> createImageMemory (const DeviceInterface&	vk,
										   VkDevice					device,
										   Allocator&				allocator,
										   VkImage					image)
{
	de::MovePtr<Allocation> allocation (allocator.allocate(getImageMemoryRequirements(vk, device, image), MemoryRequirement::Any));
	bindImageMemory(vk, device, image, allocation->getMemory(), allocation->getOffset());
	return allocation;
}

Move<VkImage> createImage (const DeviceInterface&	vkd,
						   const InstanceInterface&	instanceInterface,
						   VkPhysicalDevice			physicalDevice,
						   VkDevice					device,
						   VkFormat					vkFormat,
						   deUint32					queueFamilyIndex,
						   deUint32					width,
						   deUint32					height)
{
	const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(instanceInterface, physicalDevice, vkFormat);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
		TCU_THROW(NotSupportedError, "Format can't be used as color attachment");

	const VkImageCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		0u,										// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		vkFormat,								// VkFormat					format;
		{width, height, 1u},					// VkExtent3D				extent;
		1u,										// uint32_t					mipLevels;
		1u,										// uint32_t					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,	// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		1u,										// uint32_t					queueFamilyIndexCount;
		&queueFamilyIndex,						// const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	return createImage(vkd, device, &pCreateInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&	vk,
								   VkDevice					device,
								   VkImageViewCreateFlags	flags,
								   VkImage					image,
								   VkImageViewType			viewType,
								   VkFormat					format,
								   VkComponentMapping		components,
								   VkImageSubresourceRange	subresourceRange)
{
	const VkImageViewCreateInfo pCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		flags,										// VkImageViewCreateFlags	flags;
		image,										// VkImage					image;
		viewType,									// VkImageViewType			viewType;
		format,										// VkFormat					format;
		components,									// VkComponentMapping		components;
		subresourceRange							// VkImageSubresourceRange	subresourceRange;
	};

	return createImageView(vk, device, &pCreateInfo);
}

Move<VkImageView> createImageView (const DeviceInterface&	vkd,
								   VkDevice					device,
								   VkImage					image,
								   VkFormat					format,
								   VkImageAspectFlags		aspect)
{
	const VkImageSubresourceRange range =
	{
		aspect,	// VkImageAspectFlags	aspectMask;
		0u,		// uint32_t				baseMipLevel;
		1u,		// uint32_t				levelCount;
		0u,		// uint32_t				baseArrayLayer;
		1u		// uint32_t				layerCount;
	};

	return createImageView(vkd, device, 0u, image, VK_IMAGE_VIEW_TYPE_2D, format, makeComponentMappingRGBA(), range);
}

Move<VkRenderPass> createRenderPass (const DeviceInterface&	vkd,
									 VkDevice				device,
									 VkFormat				dstFormat)
{
	const VkAttachmentReference		dstAttachmentRef	=
	{
		0u,											// uint32_t			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentDescription	dstAttachment		=
	{
		0u,											// VkAttachmentDescriptionFlags	flags;
		dstFormat,									// VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout;
	};

	const VkSubpassDescription		subpasses[]			=
	{
		{
			(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint			pipelineBindPoint;
			0u,									// uint32_t						inputAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*	pInputAttachments;
			1u,									// uint32_t						colorAttachmentCount;
			&dstAttachmentRef,					// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,							// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,							// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,									// uint32_t						preserveAttachmentCount;
			DE_NULL								// const uint32_t*				pPreserveAttachments;
		}
	};

	const VkRenderPassCreateInfo	createInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0u,				// VkRenderPassCreateFlags			flags;
		1u,											// uint32_t							attachmentCount;
		&dstAttachment,								// const VkAttachmentDescription*	pAttachments;
		1u,											// uint32_t							subpassCount;
		subpasses,									// const VkSubpassDescription*		pSubpasses;
		0u,											// uint32_t							dependencyCount;
		DE_NULL										// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vkd, device, &createInfo);
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface&	vkd,
									   VkDevice					device,
									   VkRenderPass				renderPass,
									   VkImageView				dstImageView,
									   deUint32					width,
									   deUint32					height)
{
	const VkFramebufferCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkFramebufferCreateFlags	flags;
		renderPass,									// VkRenderPass				renderPass;
		1u,											// uint32_t					attachmentCount;
		&dstImageView,								// const VkImageView*		pAttachments;
		width,										// uint32_t					width;
		height,										// uint32_t					height;
		1u											// uint32_t					layers;
	};

	return createFramebuffer(vkd, device, &createInfo);
}

Move<VkPipelineLayout> createRenderPipelineLayout (const DeviceInterface&	vkd,
												   VkDevice					device)
{
	const VkPipelineLayoutCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		(vk::VkPipelineLayoutCreateFlags)0,				// VkPipelineLayoutCreateFlags	flags;
		0u,												// uint32_t						setLayoutCount;
		DE_NULL,										// const VkDescriptorSetLayout*	pSetLayouts;
		0u,												// uint32_t						pushConstantRangeCount;
		DE_NULL											// const VkPushConstantRange*	pPushConstantRanges;
	};

	return createPipelineLayout(vkd, device, &createInfo);
}

Move<VkPipeline> createRenderPipeline (const DeviceInterface&							vkd,
									   VkDevice											device,
									   VkRenderPass										renderPass,
									   VkPipelineLayout									pipelineLayout,
									   const BinaryCollection&							binaryCollection,
									   deUint32											width,
									   deUint32											height)
{
	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vkd, device, binaryCollection.get("quad-vert"), 0u));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vkd, device, binaryCollection.get("quad-frag"), 0u));

	const VkSpecializationInfo						emptyShaderSpecializations		=
	{
		0u,			// uint32_t							mapEntryCount;
		DE_NULL,	// const VkSpecializationMapEntry*	pMapEntries;
		0u,			// size_t							dataSize;
		DE_NULL		// const void*						pData;
	};

	// Disable blending
	const VkPipelineColorBlendAttachmentState		attachmentBlendState			=
	{
		VK_FALSE,								// VkBool32					blendEnable;
		VK_BLEND_FACTOR_SRC_ALPHA,				// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,						// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,					// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE,					// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,						// VkBlendOp				alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT				// VkColorComponentFlags	colorWriteMask;
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineShaderStageCreateInfo			shaderStages[2]					=
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			(VkPipelineShaderStageCreateFlags)0u,					// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits			stage;
			*vertexShaderModule,									// VkShaderModule					module;
			"main",													// const char*						pName;
			&emptyShaderSpecializations								// const VkSpecializationInfo*		pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			(VkPipelineShaderStageCreateFlags)0u,					// VkPipelineShaderStageCreateFlags	flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits			stage;
			*fragmentShaderModule,									// VkShaderModule					module;
			"main",													// const char*						pName;
			&emptyShaderSpecializations								// const VkSpecializationInfo*		pSpecializationInfo;
		}
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineVertexInputStateCreateFlags)0u,					// VkPipelineVertexInputStateCreateFlags	flags;
		0u,															// uint32_t									vertexBindingDescriptionCount;
		DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		0u,															// uint32_t									vertexAttributeDescriptionCount;
		DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0u,					// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
		VK_FALSE														// VkBool32									primitiveRestartEnable;
	};

	const VkViewport								viewport						=
	{
		0.0f,			// float	x;
		0.0f,			// float	y;
		(float)width,	// float	width;
		(float)height,	// float	height;
		0.0f,			// float	minDepth;
		1.0f			// float	maxDepth;
	};

	const VkRect2D									scissor							=
	{
		{ 0u, 0u },			// VkOffset2D	offset;
		{ width, height }	// VkExtent2D	extent;
	};

	const VkPipelineViewportStateCreateInfo			viewportState					=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkPipelineViewportStateCreateFlags)0u,					// VkPipelineViewportStateCreateFlags	flags;
		1u,														// uint32_t								viewportCount;
		&viewport,												// const VkViewport*					pViewports;
		1u,														// uint32_t								scissorCount;
		&scissor												// const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterState						=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineRasterizationStateCreateFlags)0u,				// VkPipelineRasterizationStateCreateFlags	flags;
		VK_TRUE,													// VkBool32									depthClampEnable;
		VK_FALSE,													// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
		VK_FALSE,													// VkBool32									depthBiasEnable;
		0.0f,														// float									depthBiasConstantFactor;
		0.0f,														// float									depthBiasClamp;
		0.0f,														// float									depthBiasSlopeFactor;
		1.0f														// float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleState				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0u,					// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE,													// VkBool32									alphaToOneEnable;
	};

	const VkPipelineColorBlendStateCreateInfo		blendState						=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0u,					// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// uint32_t										attachmentCount;
		&attachmentBlendState,										// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
	};

	const VkGraphicsPipelineCreateInfo				createInfo						=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		DE_NULL,											// const void*										pNext;
		(VkPipelineCreateFlags)0u,							// VkPipelineCreateFlags							flags;
		2u,													// uint32_t											stageCount;
		shaderStages,										// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputState,									// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyState,								// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&viewportState,										// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterState,										// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&multisampleState,									// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&blendState,										// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,										// VkPipelineLayout									layout;
		renderPass,											// VkRenderPass										renderPass;
		0u,													// uint32_t											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		0u													// int32_t											basePipelineIndex;
	};

	return createGraphicsPipeline(vkd, device, DE_NULL, &createInfo);
}

class IncompatibleTestInstance : public TestInstance
{
public:
									IncompatibleTestInstance	(Context& context, VkFormat format);
									~IncompatibleTestInstance	(void);

									tcu::TestStatus	iterate		(void);

private:
	const deUint32					m_width;
	const deUint32					m_height;
	const VkFormat					m_format;

	const Unique<VkImage>			m_dstImage;
	const de::UniquePtr<Allocation>	m_dstImageMemory;
	const Unique<VkImageView>		m_dstImageView;

	const Unique<VkRenderPass>		m_renderPass;
	const Unique<VkRenderPass>		m_renderPassIncompatible;
	const Unique<VkFramebuffer>		m_framebuffer;

	const Unique<VkPipelineLayout>	m_renderPipelineLayout;
	const Unique<VkPipeline>		m_renderPipeline;

	const Unique<VkCommandPool>		m_commandPool;
};

IncompatibleTestInstance::IncompatibleTestInstance (Context& context, VkFormat format)
	: TestInstance				(context)
	, m_width					(32u)
	, m_height					(32u)
	, m_format					(format)
	, m_dstImage				(createImage(context.getDeviceInterface(), context.getInstanceInterface(), context.getPhysicalDevice(), context.getDevice(), m_format, context.getUniversalQueueFamilyIndex(), m_width, m_height))
	, m_dstImageMemory			(createImageMemory(context.getDeviceInterface(), context.getDevice(), context.getDefaultAllocator(), *m_dstImage))
	, m_dstImageView			(createImageView(context.getDeviceInterface(), context.getDevice(), *m_dstImage, m_format, VK_IMAGE_ASPECT_COLOR_BIT))
	, m_renderPass				(createRenderPass(context.getDeviceInterface(), context.getDevice(), m_format))
	, m_renderPassIncompatible	(createRenderPass(context.getDeviceInterface(), context.getDevice(), VK_FORMAT_R8G8B8A8_UNORM))
	, m_framebuffer				(createFramebuffer(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_dstImageView, m_width, m_height))
	, m_renderPipelineLayout	(createRenderPipelineLayout(context.getDeviceInterface(), context.getDevice()))
	, m_renderPipeline			(createRenderPipeline(context.getDeviceInterface(), context.getDevice(), *m_renderPass, *m_renderPipelineLayout, context.getBinaryCollection(), m_width, m_height))
	, m_commandPool				(createCommandPool(context.getDeviceInterface(), context.getDevice(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()))
{
}

IncompatibleTestInstance::~IncompatibleTestInstance (void)
{
}

tcu::TestStatus IncompatibleTestInstance::iterate (void)
{
	const DeviceInterface&			vkd				(m_context.getDeviceInterface());
	const Unique<VkCommandBuffer>	commandBuffer	(allocateCommandBuffer(vkd, m_context.getDevice(), *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	{
		const VkCommandBufferBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType							sType;
			DE_NULL,										// const void*								pNext;
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags				flags;
			DE_NULL											// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};

		VK_CHECK(vkd.beginCommandBuffer(*commandBuffer, &beginInfo));
	}

	{
		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			*m_renderPass,								// VkRenderPass			renderPass;
			*m_framebuffer,								// VkFramebuffer		framebuffer;
			{ { 0u, 0u }, { m_width, m_height } },		// VkRect2D				renderArea;
			0u,											// uint32_t				clearValueCount;
			DE_NULL										// const VkClearValue*	pClearValues;
		};
		vkd.cmdBeginRenderPass(*commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
	vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	vkd.cmdEndRenderPass(*commandBuffer);

	VK_CHECK(vkd.endCommandBuffer(*commandBuffer));

	{
		const VkCommandBufferInheritanceInfo	commandBufferInheritanceInfo	=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,	// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			*m_renderPassIncompatible,							// VkRenderPass						renderPass;
			0u,													// uint32_t							subpass;
			0u,													// VkFramebuffer					framebuffer;
			VK_FALSE,											// VkBool32							occlusionQueryEnable;
			0u,													// VkQueryControlFlags				queryFlags;
			0u													// VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		const VkCommandBufferBeginInfo			beginInfo						=
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType							sType;
			DE_NULL,										// const void*								pNext;
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags				flags;
			&commandBufferInheritanceInfo					// const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};

		VK_CHECK(vkd.beginCommandBuffer(*commandBuffer, &beginInfo));
	}

	{
		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			*m_renderPassIncompatible,					// VkRenderPass			renderPass;
			*m_framebuffer,								// VkFramebuffer		framebuffer;
			{ { 0u, 0u }, { m_width, m_height } },		// VkRect2D				renderArea;
			0u,											// uint32_t				clearValueCount;
			DE_NULL										// const VkClearValue*	pClearValues;
		};
		vkd.cmdBeginRenderPass(*commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_renderPipeline);
	vkd.cmdDraw(*commandBuffer, 6u, 1u, 0u, 0u);
	vkd.cmdEndRenderPass(*commandBuffer);
	VK_CHECK(vkd.endCommandBuffer(*commandBuffer));

	{
		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
			DE_NULL,						// const void*					pNext;
			0u,								// uint32_t						waitSemaphoreCount;
			DE_NULL,						// const VkSemaphore*			pWaitSemaphores;
			DE_NULL,						// const VkPipelineStageFlags*	pWaitDstStageMask;
			1u,								// uint32_t						commandBufferCount;
			&*commandBuffer,				// const VkCommandBuffer*		pCommandBuffers;
			0u,								// uint32_t						signalSemaphoreCount;
			DE_NULL							// const VkSemaphore*			pSignalSemaphores;
		};

		VK_CHECK(vkd.queueSubmit(m_context.getUniversalQueue(), 1u, &submitInfo, (VkFence)0u));
		VK_CHECK(vkd.queueWaitIdle(m_context.getUniversalQueue()));
	}

	return tcu::TestStatus(QP_TEST_RESULT_PASS, "Pass");
}

struct Programs
{
	void init (vk::SourceCollections& dst, VkFormat format) const
	{
		std::ostringstream				fragmentShader;
		const tcu::TextureFormat		texFormat		(mapVkFormat(format));
		const UVec4						bits			(tcu::getTextureFormatBitDepth(texFormat).cast<deUint32>());
		const tcu::TextureChannelClass	channelClass	(tcu::getTextureChannelClass(texFormat.type));

		dst.glslSources.add("quad-vert") << glu::VertexSource(
			"#version 450\n"
			"out gl_PerVertex {\n"
			"\tvec4 gl_Position;\n"
			"};\n"
			"highp float;\n"
			"void main (void)\n"
			"{\n"
			"    gl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
			"                       ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
			"}\n");

		switch (channelClass)
		{
			case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			{
				fragmentShader <<
					"#version 450\n"
					"layout(location = 0) out highp uvec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    o_color = uvec4(" << de::toString(1u << (bits.x()-1)) << ", " << de::toString(1u << (bits.y()-2)) << ", " << de::toString(1u << (bits.z()-3)) << ", 0xffffffff);"
					"}\n";
			}
			break;

			case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			{
				fragmentShader <<
					"#version 450\n"
					"layout(location = 0) out highp ivec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    o_color = ivec4(" << de::toString(1u << (bits.x()-2)) << ", " << de::toString(1u << (bits.y()-3)) << ", " << de::toString(1u << (bits.z()-4)) << ", 0xffffffff);"
					"}\n";
			}
			break;

			default:
			{
				fragmentShader <<
					"#version 450\n"
					"layout(location = 0) out highp vec4 o_color;\n"
					"void main (void)\n"
					"{\n"
					"    o_color = vec4(0.5, 0.25, 0.125, 1.0);\n"
					"}\n";
			}
			break;
		};

		dst.glslSources.add("quad-frag") << glu::FragmentSource(fragmentShader.str());
	}
};

string formatToName (VkFormat format)
{
	const string	formatStr	= de::toString(format);
	const string	prefix		= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

void initTests (tcu::TestCaseGroup* group)
{
	static const VkFormat	formats[]	=
	{
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};

	tcu::TestContext&		testCtx		(group->getTestContext());

	for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
	{
		const VkFormat	format		(formats[formatNdx]);
		string			testName	(formatToName(format));

		group->addChild(new InstanceFactory1<IncompatibleTestInstance, VkFormat, Programs>(testCtx, tcu::NODETYPE_SELF_VALIDATE, testName.c_str(), testName.c_str(), format));
	}
}

} // anonymous

tcu::TestCaseGroup* createRenderPassIncompatibleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "incompatible", "Incompatible render pass tests", initTests);
}

} // vkt
