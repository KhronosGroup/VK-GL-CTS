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
 * \brief Dynamic State Tests - Base Class
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateBaseClass.hpp"

#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

DynamicStateBaseClass::DynamicStateBaseClass (Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
	: TestInstance				(context)
	, m_colorAttachmentFormat	(vk::VK_FORMAT_R8G8B8A8_UNORM)
	, m_topology				(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
	, m_vk						(context.getDeviceInterface())
	, m_pipeline				(context.getDeviceInterface(), context.getDevice(), pipelineConstructionType)
	, m_vertexShaderName		(vertexShaderName)
	, m_fragmentShaderName		(fragmentShaderName)
{
}

void DynamicStateBaseClass::initialize (void)
{
	const vk::VkDevice device		= m_context.getDevice();
	const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

	PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	if (m_descriptorSetLayout) {
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &*m_descriptorSetLayout;
	}
	m_pipelineLayout = vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

	const vk::VkExtent3D targetImageExtent = { WIDTH, HEIGHT, 1 };
	const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT,
												vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
	m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

	const vk::VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0,
		(deUint32)sizeof(tcu::Vec4) * 2,
		vk::VK_VERTEX_INPUT_RATE_VERTEX,
	};

	const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
	{
		{
			0u,
			0u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},
		{
			1u,
			0u,
			vk::VK_FORMAT_R32G32B32A32_SFLOAT,
			(deUint32)(sizeof(float)* 4),
		}
	};

	m_vertexInputState = PipelineCreateInfo::VertexInputState(
		1,
		&vertexInputBindingDescription,
		2,
		vertexInputAttributeDescriptions);

	const vk::VkDeviceSize dataSize = m_data.size() * sizeof(PositionColorVertex);
	m_vertexBuffer = Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
											m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<unsigned char *>(m_vertexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &m_data[0], (size_t)dataSize);

	vk::flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());

	const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
	m_cmdPool = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);

	const vk::VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		*m_cmdPool,											// VkCommandPool			commandPool;
		vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
		1u,													// deUint32					bufferCount;
	};
	m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, &cmdBufferAllocateInfo);

	initRenderPass(device);
	initFramebuffer(device);
	initPipeline(device);
}


void DynamicStateBaseClass::initRenderPass (const vk::VkDevice device)
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

	const vk::VkAttachmentReference colorAttachmentReference =
	{
		0,
		vk::VK_IMAGE_LAYOUT_GENERAL
	};

	renderPassCreateInfo.addSubpass(SubpassDescription(
		vk::VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		0,
		DE_NULL,
		1,
		&colorAttachmentReference,
		DE_NULL,
		AttachmentReference(),
		0,
		DE_NULL
	)
	);

	m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);
}

void DynamicStateBaseClass::initFramebuffer (const vk::VkDevice device)
{
	std::vector<vk::VkImageView> colorAttachments(1);
	colorAttachments[0] = *m_colorTargetView;

	const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, colorAttachments, WIDTH, HEIGHT, 1);

	m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
}

void DynamicStateBaseClass::initPipeline (const vk::VkDevice device)
{
	const PipelineCreateInfo::ColorBlendState				colorBlendState(1, &m_attachmentState);
	const PipelineCreateInfo::RasterizerState				rasterizerState;
	const PipelineCreateInfo::DepthStencilState             depthStencilState;
	const PipelineCreateInfo::DynamicState					dynamicState;
	const PipelineCreateInfo::MultiSampleState				multisampleState;

	const vk::Unique<vk::VkShaderModule>	vs			(createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0));
	const vk::Unique<vk::VkShaderModule>	fs			(createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0));
	std::vector<vk::VkViewport>				viewports	{ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } };
	std::vector<vk::VkRect2D>				scissors	{ { { 0u, 0u }, { 0u, 0u } }};

	m_pipeline.setDefaultTopology(m_topology)
			  .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicState))
			  .setupVertexInputStete(&m_vertexInputState)
			  .setupPreRasterizationShaderState(viewports,
												scissors,
												*m_pipelineLayout,
												*m_renderPass,
												0u,
												*vs,
												static_cast<const vk::VkPipelineRasterizationStateCreateInfo*>(&rasterizerState))
			  .setupFragmentShaderState(*m_pipelineLayout, *m_renderPass, 0u, *fs, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilState), &multisampleState)
			  .setupFragmentOutputState(*m_renderPass, 0u, static_cast<const vk::VkPipelineColorBlendStateCreateInfo*>(&colorBlendState), &multisampleState)
			  .setMonolithicPipelineLayout(*m_pipelineLayout)
			  .buildPipeline();
}

tcu::TestStatus DynamicStateBaseClass::iterate (void)
{
	DE_ASSERT(false);
	return tcu::TestStatus::fail("Implement iterate() method!");
}

void DynamicStateBaseClass::beginRenderPass (void)
{
	const vk::VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	beginRenderPassWithClearColor(clearColor);
}

void DynamicStateBaseClass::beginRenderPassWithClearColor(const vk::VkClearColorValue& clearColor, const bool skipBeginCmdBuffer, const bool previousTransfer)
{
	if (!skipBeginCmdBuffer)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
	}

	if (previousTransfer)
	{
		const auto transfer2Transfer = vk::makeMemoryBarrier(vk::VK_ACCESS_TRANSFER_WRITE_BIT, (vk::VK_ACCESS_TRANSFER_WRITE_BIT | vk::VK_ACCESS_TRANSFER_READ_BIT));
		vk::cmdPipelineMemoryBarrier(m_vk, *m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, &transfer2Transfer);
	}
	else
	{
		initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
									  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
	}

	const ImageSubresourceRange subresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT);
	m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(),
		vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);

	const vk::VkMemoryBarrier memBarrier =
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT,
		vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
		vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	vk::beginRenderPass(m_vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, vk::makeRect2D(0, 0, WIDTH, HEIGHT));
}

void DynamicStateBaseClass::setDynamicViewportState (const deUint32 width, const deUint32 height)
{
	vk::VkViewport viewport = vk::makeViewport(tcu::UVec2(width, height));
	m_vk.cmdSetViewport(*m_cmdBuffer, 0, 1, &viewport);

	vk::VkRect2D scissor = vk::makeRect2D(tcu::UVec2(width, height));
	m_vk.cmdSetScissor(*m_cmdBuffer, 0, 1, &scissor);
}

void DynamicStateBaseClass::setDynamicViewportState (deUint32 viewportCount, const vk::VkViewport* pViewports, const vk::VkRect2D* pScissors)
{
	m_vk.cmdSetViewport(*m_cmdBuffer, 0, viewportCount, pViewports);
	m_vk.cmdSetScissor(*m_cmdBuffer, 0, viewportCount, pScissors);
}

void DynamicStateBaseClass::setDynamicRasterizationState (const float lineWidth,
														 const float depthBiasConstantFactor,
														 const float depthBiasClamp,
														 const float depthBiasSlopeFactor)
{
	m_vk.cmdSetLineWidth(*m_cmdBuffer, lineWidth);
	m_vk.cmdSetDepthBias(*m_cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

void DynamicStateBaseClass::setDynamicBlendState (const float const1, const float const2, const float const3, const float const4)
{
	float blendConstantsants[4] = { const1, const2, const3, const4 };
	m_vk.cmdSetBlendConstants(*m_cmdBuffer, blendConstantsants);
}

void DynamicStateBaseClass::setDynamicDepthStencilState (const float	minDepthBounds,
														 const float	maxDepthBounds,
														 const deUint32 stencilFrontCompareMask,
														 const deUint32 stencilFrontWriteMask,
														 const deUint32 stencilFrontReference,
														 const deUint32 stencilBackCompareMask,
														 const deUint32 stencilBackWriteMask,
														 const deUint32 stencilBackReference)
{
	m_vk.cmdSetDepthBounds(*m_cmdBuffer, minDepthBounds, maxDepthBounds);
	m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontCompareMask);
	m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontWriteMask);
	m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontReference);
	m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackCompareMask);
	m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackWriteMask);
	m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackReference);
}

} // DynamicState
} // vkt
