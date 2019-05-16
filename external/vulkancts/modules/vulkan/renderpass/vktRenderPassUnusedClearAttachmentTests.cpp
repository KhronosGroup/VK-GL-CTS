/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Tests vkCmdClearAttachments with unused attachments.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassUnusedClearAttachmentTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include <sstream>
#include <functional>
#include <vector>

namespace vkt
{
namespace renderpass
{

namespace
{

constexpr size_t COLOR_ATTACHMENTS_NUMBER = 4; // maxColorAttachments is guaranteed to be at least 4.

struct TestParams
{
	TestParams(size_t N, RenderPassType type)
		: isUsed(N, DE_FALSE)
		, renderPassType(type)
		{}

	std::vector<deBool>	isUsed;
	RenderPassType		renderPassType;
};

class UnusedClearAttachmentTestInstance : public vkt::TestInstance
{
public:
											UnusedClearAttachmentTestInstance	(Context&			context,
																				 const TestParams&	testParams);
	virtual									~UnusedClearAttachmentTestInstance	(void) {}
	virtual tcu::TestStatus					iterate								(void);
	template<typename RenderpassSubpass>
	void									createCommandBuffer					(const DeviceInterface&	vk,
																				 VkDevice				vkDevice);
private:
	static constexpr deUint32				kImageWidth		= 32;
	static constexpr deUint32				kImageHeight	= 32;
	const tcu::UVec2						m_renderSize	= { kImageWidth, kImageHeight };

	VkClearValue							m_initialColor;
	VkClearValue							m_clearColor;

	const TestParams						m_testParams;
	std::vector<Move<VkImage>>				m_colorImages;
	std::vector<de::MovePtr<Allocation>>	m_colorImageAllocs;
	std::vector<Move<VkImageView>>			m_colorAttachmentViews;

	Move<VkRenderPass>						m_renderPass;
	Move<VkFramebuffer>						m_framebuffer;
	Move<VkShaderModule>					m_vertexShaderModule;
	Move<VkShaderModule>					m_fragmentShaderModule;
	Move<VkDescriptorSetLayout>				m_descriptorSetLayout;
	Move<VkPipelineLayout>					m_pipelineLayout;
	Move<VkPipeline>						m_graphicsPipeline;
	Move<VkCommandPool>						m_cmdPool;
	Move<VkCommandBuffer>					m_cmdBuffer;
};

class UnusedClearAttachmentTest : public vkt::TestCase
{
public:
										UnusedClearAttachmentTest	(tcu::TestContext&	testContext,
																	 const std::string&	name,
																	 const std::string&	description,
																	 const TestParams&	testParams)
											: vkt::TestCase(testContext, name, description)
											, m_testParams(testParams)
											{}
	virtual								~UnusedClearAttachmentTest	(void) {}
	virtual void						initPrograms				(SourceCollections&	sourceCollections) const;
	virtual TestInstance*				createInstance				(Context&			context) const;
private:
	const TestParams					m_testParams;
};

TestInstance* UnusedClearAttachmentTest::createInstance (Context& context) const
{
	return new UnusedClearAttachmentTestInstance(context, m_testParams);
}

// These shaders are needed to create the graphics pipeline, but they will not be actually used because we will not draw anything.
void UnusedClearAttachmentTest::initPrograms (SourceCollections& sourceCollections) const
{
	// Vertex shader.
	sourceCollections.glslSources.add("vert_shader") << glu::VertexSource(
		"#version 450\n"
		"precision highp float;\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 0) out vec4 vtxColor;\n"
		"void main (void)\n"
		"{\n"
		"\tgl_Position = position;\n"
		"\tvtxColor = vec4(0.5, 0.5, 0.5, 1.0);\n"
		"}\n");

	// Fragment shader.
	std::ostringstream fragmentSource;

	fragmentSource	<< "#version 450\n"
					<< "precision highp float;\n"
					<< "layout(location = 0) in vec4 vtxColor;\n";

	for (size_t i = 0; i < m_testParams.isUsed.size(); ++i)
	{
		if (m_testParams.isUsed[i])
			fragmentSource << "layout(location = " << i << ") out vec4 fragColor" << i << ";\n";
	}

	fragmentSource	<< "void main (void)\n"
					<< "{\n";

	for (size_t i = 0; i < m_testParams.isUsed.size(); ++i)
	{
		if (m_testParams.isUsed[i])
			fragmentSource << "\tfragColor" << i << " = vtxColor;\n";
	}

	fragmentSource	<< "}\n";

	sourceCollections.glslSources.add("frag_shader") << glu::FragmentSource(fragmentSource.str());
}

// Create a render pass for this use case.
template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPass (const DeviceInterface&	vk,
									 VkDevice				vkDevice,
									 const TestParams		testParams)
{
	const VkImageAspectFlags	aspectMask						= testParams.renderPassType == RENDERPASS_TYPE_LEGACY ? 0 : VK_IMAGE_ASPECT_COLOR_BIT;

	// Create attachment descriptions.
	const AttachmentDesc		attachmentDescription			(
		DE_NULL,									// const void*						pNext
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags
		VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat							format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples
		VK_ATTACHMENT_LOAD_OP_LOAD,					// VkAttachmentLoadOp				loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout
	);
	std::vector<AttachmentDesc>	attachmentDescriptions			(testParams.isUsed.size(), attachmentDescription);

	// Mark attachments as used or not depending on the test parameters.
	std::vector<AttachmentRef>	attachmentReferences;
	for (size_t i = 0; i < testParams.isUsed.size(); ++i)
	{
		attachmentReferences.push_back(AttachmentRef(
			DE_NULL,																	// const void*			pNext
			(testParams.isUsed[i] ? static_cast<deUint32>(i) : VK_ATTACHMENT_UNUSED),	// deUint32				attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,									// VkImageLayout		layout
			aspectMask																	// VkImageAspectFlags	aspectMask
		));
	}

	// Create subpass description with the previous color attachment references.
	const SubpassDesc			subpassDescription				(
		DE_NULL,
		(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint
		0u,													// deUint32							viewMask
		0u,													// deUint32							inputAttachmentCount
		DE_NULL,											// const VkAttachmentReference*		pInputAttachments
		static_cast<deUint32>(attachmentReferences.size()),	// deUint32							colorAttachmentCount
		attachmentReferences.data(),						// const VkAttachmentReference*		pColorAttachments
		DE_NULL,											// const VkAttachmentReference*		pResolveAttachments
		DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment
		0u,													// deUint32							preserveAttachmentCount
		DE_NULL												// const deUint32*					pPreserveAttachments
	);

	const RenderPassCreateInfo	renderPassInfo					(
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
		1u,														// deUint32							subpassCount
		&subpassDescription,									// const VkSubpassDescription*		pSubpasses
		0u,														// deUint32							dependencyCount
		DE_NULL,												// const VkSubpassDependency*		pDependencies
		0u,														// deUint32							correlatedViewMaskCount
		DE_NULL													// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

UnusedClearAttachmentTestInstance::UnusedClearAttachmentTestInstance(Context&			context,
																	 const TestParams&	testParams)
	: vkt::TestInstance(context)
	, m_testParams(testParams)
{
	// Initial color for all images.
	m_initialColor.color.float32[0] = 0.0f;
	m_initialColor.color.float32[1] = 0.0f;
	m_initialColor.color.float32[2] = 0.0f;
	m_initialColor.color.float32[3] = 1.0f;

	// Clear color for used attachments.
	m_clearColor.color.float32[0] = 1.0f;
	m_clearColor.color.float32[1] = 1.0f;
	m_clearColor.color.float32[2] = 1.0f;
	m_clearColor.color.float32[3] = 1.0f;

	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				vkDevice				= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	// Check for renderpass2 extension if used
	if (testParams.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
		context.requireDeviceExtension("VK_KHR_create_renderpass2");

	// Create color images.
	{
		const VkImageCreateInfo	colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			VK_FORMAT_R8G8B8A8_UNORM,												// VkFormat					format;
			{ kImageWidth, kImageHeight, 1u },										// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
				| VK_IMAGE_USAGE_TRANSFER_DST_BIT,									// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		for (size_t i = 0; i < testParams.isUsed.size(); ++i)
		{
			// Create, allocate and bind image memory.
			m_colorImages.emplace_back(createImage(vk, vkDevice, &colorImageParams));
			m_colorImageAllocs.emplace_back(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImages.back()), MemoryRequirement::Any));
			VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImages.back(), m_colorImageAllocs.back()->getMemory(), m_colorImageAllocs.back()->getOffset()));

			// Create image view.
			{
				const VkImageViewCreateInfo colorAttachmentViewParams =
				{
					VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
					DE_NULL,											// const void*				pNext;
					0u,													// VkImageViewCreateFlags	flags;
					*m_colorImages.back(),								// VkImage					image;
					VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
					VK_FORMAT_R8G8B8A8_UNORM,							// VkFormat					format;
					componentMappingRGBA,								// VkChannelMapping			channels;
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
				};

				m_colorAttachmentViews.emplace_back(createImageView(vk, vkDevice, &colorAttachmentViewParams));
			}

			// Fill image in black and leave it prepared to be used as a color attachment.
			{
				const VkImageAspectFlags		aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
				Move<VkCommandPool>				cmdPool;
				Move<VkCommandBuffer>			cmdBuffer;

				// Create command pool and buffer
				cmdPool		= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
				cmdBuffer	= allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

				// From undefined layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
				const VkImageMemoryBarrier preImageBarrier =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
					DE_NULL,								// const void*				pNext;
					0u,										// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
					*m_colorImages.back(),					// VkImage					image;
					{										// VkImageSubresourceRange	subresourceRange;
						aspectMask,							// VkImageAspect			aspect;
						0u,									// deUint32					baseMipLevel;
						1u,									// deUint32					mipLevels;
						0u,									// deUint32					baseArraySlice;
						1u									// deUint32					arraySize;
					}
				};

				// From VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
				const VkImageMemoryBarrier postImageBarrier =
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
					DE_NULL,									// const void*				pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags			srcAccessMask;
					VK_ACCESS_SHADER_READ_BIT,					// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,					// deUint32					dstQueueFamilyIndex;
					*m_colorImages.back(),						// VkImage					image;
					{											// VkImageSubresourceRange	subresourceRange;
						aspectMask,								// VkImageAspect			aspect;
						0u,										// deUint32					baseMipLevel;
						1u,										// deUint32					mipLevels;
						0u,										// deUint32					baseArraySlice;
						1u										// deUint32					arraySize;
					}
				};

				const VkImageSubresourceRange clearRange	=
				{
					aspectMask,	// VkImageAspectFlags	aspectMask;
					0u,			// deUint32				baseMipLevel;
					1u,			// deUint32				levelCount;
					0u,			// deUint32				baseArrayLayer;
					1u			// deUint32				layerCount;
				};

				// Clear image and transfer layout.
				beginCommandBuffer(vk, *cmdBuffer);
					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &preImageBarrier);
					vk.cmdClearColorImage(*cmdBuffer, *m_colorImages.back(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_initialColor.color, 1, &clearRange);
					vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &postImageBarrier);
				endCommandBuffer(vk, *cmdBuffer);

				submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), cmdBuffer.get());
			}
		}
	}

	// Create render pass.
	if (testParams.renderPassType == RENDERPASS_TYPE_LEGACY)
		m_renderPass = createRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, vkDevice, testParams);
	else
		m_renderPass = createRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);

	// Create framebuffer
	{
		std::vector<VkImageView>		imageViews;

		for (auto& movePtr : m_colorAttachmentViews)
			imageViews.push_back(movePtr.get());

		const VkFramebufferCreateInfo	framebufferParams	=
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,									// const void*				pNext;
			0u,											// VkFramebufferCreateFlags	flags;
			*m_renderPass,								// VkRenderPass				renderPass;
			static_cast<deUint32>(imageViews.size()),	// deUint32					attachmentCount;
			imageViews.data(),							// const VkImageView*		pAttachments;
			kImageWidth,								// deUint32					width;
			kImageHeight,								// deUint32					height;
			1u											// deUint32					layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout for subpass 0.
	{
		const VkDescriptorSetLayoutCreateInfo	descriptorSetLayoutParams	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType						sType
			DE_NULL,												// const void*							pNext
			0u,														// VkDescriptorSetLayoutCreateFlags		flags
			0u,														// deUint32								bindingCount
			DE_NULL													// const VkDescriptorSetLayoutBinding*	pBindings
		};
		m_descriptorSetLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkPipelineLayoutCreateFlags		flags;
			1u,													// deUint32							setLayoutCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("vert_shader"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("frag_shader"), 0);

	// Create pipeline.
	{
		const std::vector<VkViewport>						viewports						(1, makeViewport(m_renderSize));
		const std::vector<VkRect2D>							scissors						(1, makeRect2D(m_renderSize));

		const VkPipelineColorBlendAttachmentState			colorBlendAttachmentState		=
		{
			VK_FALSE,					// VkBool32					blendEnable
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcColorBlendFactor
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstColorBlendFactor
			VK_BLEND_OP_ADD,			// VkBlendOp				colorBlendOp
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcAlphaBlendFactor
			VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstAlphaBlendFactor
			VK_BLEND_OP_ADD,			// VkBlendOp				alphaBlendOp
			VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags	colorWriteMask
			| VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT
		};

		std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates;
		for (size_t i = 0; i < testParams.isUsed.size(); ++i)
			colorBlendAttachmentStates.push_back(colorBlendAttachmentState);

		const VkPipelineColorBlendStateCreateInfo			colorBlendStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
			DE_NULL,													// const void*									pNext
			0u,															// VkPipelineColorBlendStateCreateFlags			flags
			VK_FALSE,													// VkBool32										logicOpEnable
			VK_LOGIC_OP_CLEAR,											// VkLogicOp									logicOp
			static_cast<deUint32>(colorBlendAttachmentStates.size()),	// deUint32										attachmentCount
			colorBlendAttachmentStates.data(),							// const VkPipelineColorBlendAttachmentState*	pAttachments
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4]
		};

		m_graphicsPipeline = makeGraphicsPipeline(vk,									// const DeviceInterface&							vk
												  vkDevice,								// const VkDevice									device
												  *m_pipelineLayout,					// const VkPipelineLayout							pipelineLayout
												  *m_vertexShaderModule,				// const VkShaderModule								vertexShaderModule
												  DE_NULL,								// const VkShaderModule								tessellationControlModule
												  DE_NULL,								// const VkShaderModule								tessellationEvalModule
												  DE_NULL,								// const VkShaderModule								geometryShaderModule
												  *m_fragmentShaderModule,				// const VkShaderModule								fragmentShaderModule
												  *m_renderPass,						// const VkRenderPass								renderPass
												  viewports,							// const std::vector<VkViewport>&					viewports
												  scissors,								// const std::vector<VkRect2D>&						scissors
												  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology						topology
												  0u,									// const deUint32									subpass
												  0u,									// const deUint32									patchControlPoints
												  DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
												  DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
												  DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
												  DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
												  &colorBlendStateCreateInfo);			// const VkPipelineColorBlendStateCreateInfo*		colorBlendStateCreateInfo
	}

	// Create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// Create command buffer
	if (testParams.renderPassType == RENDERPASS_TYPE_LEGACY)
		createCommandBuffer<RenderpassSubpass1>(vk, vkDevice);
	else
		createCommandBuffer<RenderpassSubpass2>(vk, vkDevice);
}

template <typename RenderpassSubpass>
void UnusedClearAttachmentTestInstance::createCommandBuffer (const DeviceInterface&	vk,
															 VkDevice				vkDevice)
{
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	const VkRenderPassBeginInfo							renderPassBeginInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		*m_renderPass,								// VkRenderPass			renderPass;
		*m_framebuffer,								// VkFramebuffer		framebuffer;
		makeRect2D(m_renderSize),					// VkRect2D				renderArea;
		0u,											// uint32_t				clearValueCount;
		DE_NULL										// const VkClearValue*	pClearValues;
	};

	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const VkClearRect									clearRect			=
	{
		{												// VkRect2D		rect;
			{ 0, 0, },									//	VkOffset2D	offset;
			{ kImageWidth, kImageHeight }				//	VkExtent2D	extent;
		},
		0u,												// uint32_t		baseArrayLayer;
		1u												// uint32_t		layerCount;
	};

	std::vector<VkClearAttachment> clearAttachments;
	for (size_t i = 0; i < m_testParams.isUsed.size(); ++i)
	{
		const VkClearAttachment clearAttachment = {
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags	aspectMask;
			static_cast<deUint32>(i),	// uint32_t				colorAttachment;
			m_clearColor				// VkClearValue			clearValue;
		};
		clearAttachments.push_back(clearAttachment);
	}

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);
		RenderpassSubpass::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);
		vk.cmdClearAttachments(*m_cmdBuffer, static_cast<deUint32>(clearAttachments.size()), clearAttachments.data(), 1u, &clearRect);
		RenderpassSubpass::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus	UnusedClearAttachmentTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			vkDevice			= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator			allocator			(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	// Read result images.
	std::vector<de::MovePtr<tcu::TextureLevel>> imagePixels;
	for (size_t i = 0; i < m_testParams.isUsed.size(); ++i)
		imagePixels.emplace_back(pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, *m_colorImages[i], VK_FORMAT_R8G8B8A8_UNORM, m_renderSize).release());

	// Verify pixel colors match.
	for (size_t i = 0; i < imagePixels.size(); ++i)
	{
		const tcu::ConstPixelBufferAccess&	imageAccess		= imagePixels[i]->getAccess();
		const float*						refColor		= (m_testParams.isUsed[i] ? m_clearColor.color.float32 : m_initialColor.color.float32);

		for (int y = 0; y < imageAccess.getHeight(); ++y)
		for (int x = 0; x < imageAccess.getWidth(); ++x)
		{
			const tcu::Vec4	color = imageAccess.getPixel(x, y);

			for (deUint32 cpnt = 0; cpnt < 4; ++cpnt)
				if (de::abs(color[cpnt] - refColor[cpnt]) > 0.01f)
				{
					std::ostringstream msg;

					msg << "Attachment " << i << " with mismatched pixel (" << x << ", " << y << "): expecting pixel value [";
					for (deUint32 j = 0; j < 4; ++j)
						msg << ((j == 0) ? "" : ", ") << refColor[j];
					msg << "] and found [";
					for (deUint32 j = 0; j < 4; ++j)
						msg << ((j == 0) ? "" : ", ") << color[j];
					msg << "]";

					return tcu::TestStatus::fail(msg.str());
				}
		}
	}

	return tcu::TestStatus::pass("Pass");
}


using CallbackFunction = std::function<void(const std::vector<deBool>&)>;

void runCallbackOnCombination(std::vector<deBool>& array, size_t current_index, CallbackFunction callback)
{
	static const deBool values[] = { DE_FALSE, DE_TRUE };

	DE_ASSERT(current_index < array.size());
	for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(values); ++i)
	{
		array[current_index] = values[i];
		if (current_index == array.size() - 1)
			callback(array);
		else
			runCallbackOnCombination(array, current_index + 1, callback);
	}
}

void runCallbackOnCombination(std::vector<deBool>& array, CallbackFunction callback)
{
	runCallbackOnCombination(array, 0, callback);
}

std::string getTestName(const std::vector<deBool>& array)
{
	std::ostringstream name;
	for (size_t i = 0; i < array.size(); ++i)
		name << ((i == 0)? "" : "_") << ((array[i]) ? "used" : "unused" );
	return name.str();
}

} // anonymous


tcu::TestCaseGroup* createRenderPassUnusedClearAttachmentTests (tcu::TestContext& testCtx, const RenderPassType renderPassType)
{
	de::MovePtr<tcu::TestCaseGroup>	testGroup (new tcu::TestCaseGroup(testCtx, "unused_clear_attachments", "Unused attachments with vkCmdClearAttachments"));

	std::vector<TestParams>			testTypes;
	testTypes.emplace_back(1, renderPassType);							// Single attachment.
	testTypes.emplace_back(COLOR_ATTACHMENTS_NUMBER, renderPassType);	// Multiple attachments.

	for (auto& params : testTypes)
	{
		runCallbackOnCombination(params.isUsed, [&](const std::vector<deBool>& array) {
			std::string name = getTestName(array);
			testGroup->addChild(new UnusedClearAttachmentTest(testCtx, name, "", params));
		});
	}

	return testGroup.release();
}

} // renderpass
} // vkt
