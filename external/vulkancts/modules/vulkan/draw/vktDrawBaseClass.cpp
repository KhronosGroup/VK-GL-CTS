/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Command draw Tests - Base Class
 *//*--------------------------------------------------------------------*/

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace Draw
{

DrawTestsBaseClass::DrawTestsBaseClass (Context& context,
										const char* vertexShaderName,
										const char* fragmentShaderName,
										const SharedGroupParams groupParams,
										vk::VkPrimitiveTopology topology,
										const uint32_t layers)
	: TestInstance				(context)
	, m_colorAttachmentFormat	(vk::VK_FORMAT_R8G8B8A8_UNORM)
	, m_groupParams				(groupParams)
	, m_topology				(topology)
	, m_layers					(layers)
	, m_vk						(context.getDeviceInterface())
	, m_vertexShaderName		(vertexShaderName)
	, m_fragmentShaderName		(fragmentShaderName)
{
}

void DrawTestsBaseClass::initialize (void)
{
	const vk::VkDevice device				= m_context.getDevice();
	const deUint32 queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const auto viewMask						= getDefaultViewMask();
	const auto multiview					= (viewMask != 0u);

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout						= vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

	const vk::VkExtent3D targetImageExtent	= { WIDTH, HEIGHT, 1 };
	const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1, m_layers, vk::VK_SAMPLE_COUNT_1_BIT,
		vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_colorTargetImage						= Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageSubresourceRange colorSRR (vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_layers);
	const auto imageViewType = (multiview ? vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY : vk::VK_IMAGE_VIEW_TYPE_2D);
	const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), imageViewType, m_colorAttachmentFormat, colorSRR);
	m_colorTargetView						= vk::createImageView(m_vk, device, &colorTargetViewInfo);

	// create renderpass and framebuffer only when we are not using dynamic rendering
	if (!m_groupParams->useDynamicRendering)
	{
		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,
																 vk::VK_SAMPLE_COUNT_1_BIT,
																 vk::VK_ATTACHMENT_LOAD_OP_LOAD,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																 vk::VK_ATTACHMENT_STORE_OP_STORE,
																 vk::VK_IMAGE_LAYOUT_GENERAL,
																 vk::VK_IMAGE_LAYOUT_GENERAL));

		const vk::VkAttachmentReference colorAttachmentReference
		{
			0,
			vk::VK_IMAGE_LAYOUT_GENERAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
														   0,
														   0,
														   DE_NULL,
														   1,
														   &colorAttachmentReference,
														   DE_NULL,
														   AttachmentReference(),
														   0,
														   DE_NULL));

		const std::vector<uint32_t> viewMasks (1u, viewMask);

		const vk::VkRenderPassMultiviewCreateInfo multiviewCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,	// VkStructureType	sType;
			nullptr,													// const void*		pNext;
			de::sizeU32(viewMasks),										// uint32_t		subpassCount;
			de::dataOrNull(viewMasks),									// const uint32_t*	pViewMasks;
			0u,															// uint32_t		dependencyCount;
			nullptr,													// const int32_t*	pViewOffsets;
			de::sizeU32(viewMasks),										// uint32_t		correlationMaskCount;
			de::dataOrNull(viewMasks),									// const uint32_t*	pCorrelationMasks;
		};

		if (multiview)
			renderPassCreateInfo.pNext = &multiviewCreateInfo;

		m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

		// create framebuffer
		std::vector<vk::VkImageView>	colorAttachments		{ *m_colorTargetView };
		const FramebufferCreateInfo		framebufferCreateInfo	(*m_renderPass, colorAttachments, WIDTH, HEIGHT, 1);
		m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
	}

	const vk::VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0,
		sizeof(VertexElementData),
		vk::VK_VERTEX_INPUT_RATE_VERTEX,
	};

	const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,
			0u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},  // VertexElementData::color
		{
			2u,
			0u,
			vk::VK_FORMAT_R32_SINT,
			static_cast<deUint32>(sizeof(tcu::Vec4)) * 2
		}   // VertexElementData::refVertexIndex
	};

	m_vertexInputState = PipelineCreateInfo::VertexInputState(1,
															  &vertexInputBindingDescription,
															  DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),
															  vertexInputAttributeDescriptions);

	const vk::VkDeviceSize dataSize = m_data.size() * sizeof(VertexElementData);
	m_vertexBuffer = Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize,
		vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_vertexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &m_data[0], static_cast<size_t>(dataSize));

	vk::flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());

	const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
	m_cmdPool	= vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
	m_cmdBuffer	= vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	if (m_groupParams->useSecondaryCmdBuffer)
		m_secCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	initPipeline(device);
}

void DrawTestsBaseClass::initPipeline (const vk::VkDevice device)
{
	const vk::Unique<vk::VkShaderModule> vs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0));
	const vk::Unique<vk::VkShaderModule> fs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0));

	const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

	vk::VkViewport viewport	= vk::makeViewport(WIDTH, HEIGHT);
	vk::VkRect2D scissor	= vk::makeRect2D(WIDTH, HEIGHT);

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_topology));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport), std::vector<vk::VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

#ifndef CTS_USES_VULKANSC
	const auto viewMask = getDefaultViewMask();

	vk::VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		DE_NULL,
		viewMask,
		1u,
		&m_colorAttachmentFormat,
		vk::VK_FORMAT_UNDEFINED,
		vk::VK_FORMAT_UNDEFINED
	};

	if (m_groupParams->useDynamicRendering)
		pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

	m_pipeline = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo);
}

void DrawTestsBaseClass::preRenderBarriers(void)
{
	const vk::VkClearValue clearColor { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

	initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, m_layers);

	const ImageSubresourceRange subresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT);
	m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(),
		vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &subresourceRange);

	const vk::VkMemoryBarrier memBarrier
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
		vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
}

void DrawTestsBaseClass::beginLegacyRender (vk::VkCommandBuffer cmdBuffer, const vk::VkSubpassContents content)
{
	const vk::VkRect2D renderArea = vk::makeRect2D(WIDTH, HEIGHT);

	vk::beginRenderPass(m_vk, cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, content);
}

void DrawTestsBaseClass::endLegacyRender (vk::VkCommandBuffer cmdBuffer)
{
	vk::endRenderPass(m_vk, cmdBuffer);
}

#ifndef CTS_USES_VULKANSC
void DrawTestsBaseClass::beginSecondaryCmdBuffer(const vk::DeviceInterface& vk, const vk::VkRenderingFlagsKHR renderingFlags)
{
	vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		getDefaultViewMask(),													// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&m_colorAttachmentFormat,												// const VkFormat*					pColorAttachmentFormats;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							depthAttachmentFormat;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							stencilAttachmentFormat;
		vk::VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			rasterizationSamples;
	};
	const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo = vk::initVulkanStructure(&inheritanceRenderingInfo);

	vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const vk::VkCommandBufferBeginInfo commandBufBeginParams
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,						// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	VK_CHECK(vk.beginCommandBuffer(*m_secCmdBuffer, &commandBufBeginParams));
}

void DrawTestsBaseClass::beginDynamicRender(vk::VkCommandBuffer cmdBuffer, const vk::VkRenderingFlagsKHR renderingFlags)
{
	const vk::VkClearValue	clearColor{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	const vk::VkRect2D		renderArea = vk::makeRect2D(WIDTH, HEIGHT);

	vk::beginRendering(m_vk, cmdBuffer, *m_colorTargetView, renderArea, clearColor, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, renderingFlags, m_layers, getDefaultViewMask());
}

void DrawTestsBaseClass::endDynamicRender(vk::VkCommandBuffer cmdBuffer)
{
	vk::endRendering(m_vk, cmdBuffer);
}
#endif // CTS_USES_VULKANSC

uint32_t DrawTestsBaseClass::getDefaultViewMask (void) const
{
	const bool		multiview	= (m_layers > 1u);
	const uint32_t	viewMask	= (multiview ? ((1u << m_layers) - 1u) : 0u);
	return viewMask;
}

}	// Draw
}	// vkt
