/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 LunarG, Inc.
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google LLC
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
 *//*!
 * \file
 * \brief Dynamic State Discard Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateDiscardTests.hpp"

#include "vktDynamicStateBaseClass.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBufferWithMemory.hpp"

namespace vkt
{
namespace DynamicState
{
using namespace Draw;
using namespace vk;

enum TestDynamicStateDiscard
{
	TEST_STENCIL,
	TEST_VIEWPORT,
	TEST_SCISSOR,
	TEST_DEPTH,
	TEST_BLEND_CONSTANTS,
	TEST_LINE_WIDTH,
};

VkFormat pickSupportedStencilFormat	(const InstanceInterface&	instanceInterface,
									 const VkPhysicalDevice		device)
{
	static const VkFormat stencilFormats[] =
	{
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(stencilFormats); ++i)
	{
		VkFormatProperties formatProps;
		instanceInterface.getPhysicalDeviceFormatProperties(device, stencilFormats[i], &formatProps);

		if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			return stencilFormats[i];
		}
	}
	TCU_FAIL("Cannot find supported stencil format");
}

bool isFormatStencil (VkFormat format)
{
	const auto textureFormat = vk::mapVkFormat(format);
	return (textureFormat.order == tcu::TextureFormat::DS || textureFormat.order == tcu::TextureFormat::S);
}

class DiscardTestInstance : public DynamicStateBaseClass
{
public:
	DiscardTestInstance (Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName, vk::VkFormat depthStencilFormat);

	virtual void								initRenderPass	(const vk::VkDevice		device);
	virtual void								initFramebuffer	(const vk::VkDevice		device);
	virtual void								initPipeline	(const vk::VkDevice		device);

	void										beginRenderPass	(const vk::VkClearColorValue& clearColor);

	virtual tcu::TestStatus						iterate			(void);

protected:

	virtual void								setDynamicState	(void) {
		DE_ASSERT(false);
	}
	virtual tcu::TestStatus						verifyResults	(void) {
		DE_ASSERT(false);
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "");
	}
	const vk::VkFormat					m_depthStencilAttachmentFormat;

	de::SharedPtr<Draw::Image>			m_depthStencilImage;
	vk::Move<vk::VkImageView>			m_depthStencilView;
	std::vector<vk::VkDynamicState>		m_dynamicStates;
	VkBool32							m_depthBounds;
};

DiscardTestInstance::DiscardTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName, vk::VkFormat depthStencilFormat)
	: DynamicStateBaseClass(context, pipelineConstructionType, vertexShaderName, fragmentShaderName)
	, m_depthStencilAttachmentFormat(depthStencilFormat)
	, m_depthBounds(VK_FALSE)
{
	const vk::VkDevice				device					= m_context.getDevice();

	const vk::VkExtent3D			stencilImageExtent		= { WIDTH, HEIGHT, 1 };
	const ImageCreateInfo			stencilImageCreateInfo	(vk::VK_IMAGE_TYPE_2D, m_depthStencilAttachmentFormat, stencilImageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_depthStencilImage	= Image::createAndAlloc(m_vk, device, stencilImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageViewCreateInfo		stencilViewInfo(m_depthStencilImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_depthStencilAttachmentFormat);
	m_depthStencilView	= vk::createImageView(m_vk, device, &stencilViewInfo);

	m_topology		= vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
	m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
	m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
	m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

	const vk::VkDescriptorSetLayoutBinding binding =
	{
		0u,
		vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		vk::VK_SHADER_STAGE_FRAGMENT_BIT,
		DE_NULL
	};

	DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(1, &binding);
	m_otherSetLayout = vk::createDescriptorSetLayout(m_vk, device, &descriptorSetLayoutCreateInfo);
}

void DiscardTestInstance::initRenderPass (const vk::VkDevice device)
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
	renderPassCreateInfo.addAttachment(AttachmentDescription(m_depthStencilAttachmentFormat,
		vk::VK_SAMPLE_COUNT_1_BIT,
		vk::VK_ATTACHMENT_LOAD_OP_LOAD,
		vk::VK_ATTACHMENT_STORE_OP_STORE,
		vk::VK_ATTACHMENT_LOAD_OP_LOAD,
		vk::VK_ATTACHMENT_STORE_OP_STORE,
		vk::VK_IMAGE_LAYOUT_GENERAL,
		vk::VK_IMAGE_LAYOUT_GENERAL));

	const vk::VkAttachmentReference colorAttachmentReference =
	{
		0,
		vk::VK_IMAGE_LAYOUT_GENERAL
	};

	const vk::VkAttachmentReference stencilAttachmentReference =
	{
		1,
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
		stencilAttachmentReference,
		0,
		DE_NULL
	)
	);

	m_renderPass = vk::RenderPassWrapper(m_pipelineConstructionType, m_vk, device, &renderPassCreateInfo);
}

void DiscardTestInstance::initFramebuffer (const vk::VkDevice device)
{
	std::vector<vk::VkImageView> attachments(2);
	attachments[0] = *m_colorTargetView;
	attachments[1] = *m_depthStencilView;

	const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);

	m_renderPass.createFramebuffer(m_vk, device, &framebufferCreateInfo, {m_colorTargetImage->object(), m_depthStencilImage->object()});
}

void DiscardTestInstance::initPipeline(const vk::VkDevice device)
{
	const vk::ShaderWrapper									vs(vk::ShaderWrapper	(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0));
	const vk::ShaderWrapper									fs(vk::ShaderWrapper	(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0));
	std::vector<vk::VkViewport>								viewports				{ { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 1.0f } };
	std::vector<vk::VkRect2D>								scissors				{ { { 0u, 0u }, { WIDTH, HEIGHT } } };

	const PipelineCreateInfo::ColorBlendState::Attachment	attachmentState;
	const PipelineCreateInfo::ColorBlendState				colorBlendState(1u, static_cast<const vk::VkPipelineColorBlendAttachmentState*>(&attachmentState));
	const PipelineCreateInfo::RasterizerState				rasterizerState;
	PipelineCreateInfo::DepthStencilState					depthStencilState;
	const PipelineCreateInfo::DynamicState					dynamicState(m_dynamicStates);

	depthStencilState.depthTestEnable		= VK_TRUE;
	depthStencilState.depthWriteEnable		= VK_TRUE;
	depthStencilState.depthCompareOp		= VK_COMPARE_OP_ALWAYS;
	depthStencilState.depthBoundsTestEnable = m_depthBounds;
	depthStencilState.minDepthBounds		= 0.0f;
	depthStencilState.maxDepthBounds		= 1.0f;
	depthStencilState.stencilTestEnable		= VK_TRUE;
	depthStencilState.front.failOp			= VK_STENCIL_OP_KEEP;
	depthStencilState.front.passOp			= VK_STENCIL_OP_REPLACE;
	depthStencilState.front.depthFailOp		= VK_STENCIL_OP_KEEP;
	depthStencilState.front.compareOp		= VK_COMPARE_OP_ALWAYS;
	depthStencilState.front.compareMask		= 0u;
	depthStencilState.front.writeMask		= 0u;
	depthStencilState.front.reference		= 0u;
	depthStencilState.back.failOp			= VK_STENCIL_OP_KEEP;
	depthStencilState.back.passOp			= VK_STENCIL_OP_REPLACE;
	depthStencilState.back.depthFailOp		= VK_STENCIL_OP_KEEP;
	depthStencilState.back.compareOp		= VK_COMPARE_OP_ALWAYS;
	depthStencilState.back.compareMask		= 0u;
	depthStencilState.back.writeMask		= 0u;
	depthStencilState.back.reference		= 0u;

	m_pipeline.setDefaultTopology(m_topology)
		.setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo*>(&dynamicState))
		.setDefaultMultisampleState()
		.setupVertexInputState(&m_vertexInputState)
		.setupPreRasterizationShaderState(viewports,
			scissors,
			m_pipelineLayout,
			*m_renderPass,
			0u,
			vs,
			static_cast<const vk::VkPipelineRasterizationStateCreateInfo*>(&rasterizerState))
		.setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fs, static_cast<const vk::VkPipelineDepthStencilStateCreateInfo*>(&depthStencilState))
		.setupFragmentOutputState(*m_renderPass, 0u, static_cast<const vk::VkPipelineColorBlendStateCreateInfo*>(&colorBlendState))
		.setMonolithicPipelineLayout(m_pipelineLayout)
		.buildPipeline();
}

void DiscardTestInstance::beginRenderPass (const vk::VkClearColorValue& clearColor)
{
	beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

	initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
		vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

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

	if (isFormatStencil(m_depthStencilAttachmentFormat)) {
		initialTransitionStencil2DImage(m_vk, *m_cmdBuffer, m_depthStencilImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
	}
	else
	{
		initialTransitionDepth2DImage(m_vk, *m_cmdBuffer, m_depthStencilImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
	}


	const vk::VkClearDepthStencilValue	depthStencilClearValue	= { 0.0f, 0 };
	const ImageSubresourceRange			subresourceRangeStencil = m_depthStencilAttachmentFormat == vk::VK_FORMAT_S8_UINT ? vk::VK_IMAGE_ASPECT_STENCIL_BIT : vk::VK_IMAGE_ASPECT_DEPTH_BIT;
	m_vk.cmdClearDepthStencilImage(*m_cmdBuffer, m_depthStencilImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, &depthStencilClearValue, 1, &subresourceRangeStencil);

	vk::VkMemoryBarrier dsMemBarrier;
	dsMemBarrier.sType = vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	dsMemBarrier.pNext = NULL;
	dsMemBarrier.srcAccessMask = vk::VK_ACCESS_TRANSFER_WRITE_BIT;
	dsMemBarrier.dstAccessMask = vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 1, &dsMemBarrier, 0, NULL, 0, NULL);

	m_renderPass.begin(m_vk, *m_cmdBuffer, vk::makeRect2D(0, 0, WIDTH, HEIGHT));
}
tcu::TestStatus DiscardTestInstance::iterate(void) {
	const vk::VkQueue					queue				= m_context.getUniversalQueue();
	const vk::VkDevice					device				= m_context.getDevice();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const VkDescriptorPoolSize			poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	1u	},
	};
	const VkDescriptorPoolCreateInfo	poolInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,		// maxSets
		DE_LENGTH_OF_ARRAY(poolSizes),
		poolSizes,
	};

	vk::Move<vk::VkDescriptorPool>		descriptorPool		= createDescriptorPool(m_vk, device, &poolInfo);
	vk::Move<vk::VkDescriptorSet>		descriptorSet		= makeDescriptorSet(m_vk, device, *descriptorPool, *m_otherSetLayout);

	const vk::VkDeviceSize				size				= sizeof(int);

	const BufferWithMemory				buffer				(m_vk, device, allocator, makeBufferCreateInfo(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), MemoryRequirement::HostVisible);

	deUint32*							ptr					= (deUint32*)buffer.getAllocation().getHostPtr();
	deMemset(ptr, 0u, static_cast<std::size_t>(size));

	{
		const vk::VkDescriptorBufferInfo	bufferInfo		= makeDescriptorBufferInfo(*buffer, 0, size);
		const vk::VkWriteDescriptorSet		descriptorWrite =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			DE_NULL,
			*descriptorSet,
			0u,		// dstBinding
			0u,		// dstArrayElement
			1u,		// descriptorCount
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			DE_NULL,
			&bufferInfo,
			DE_NULL,
		};

		m_vk.updateDescriptorSets(device, 1, &descriptorWrite, 0u, DE_NULL);
	}

	const vk::VkClearColorValue			clearColor			= { { 0.0f, 0.0f, 0.0f, 1.0f } };
	beginRenderPass(clearColor);
	m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &*descriptorSet, 0, nullptr);
	m_pipeline.bind(*m_cmdBuffer);
	const vk::VkDeviceSize vertexBufferOffset = 0;
	const vk::VkBuffer		vertexBuffer = m_vertexBuffer->object();
	m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	setDynamicState();

	m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
	m_renderPass.end(m_vk, *m_cmdBuffer);
	endCommandBuffer(m_vk, *m_cmdBuffer);

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	return verifyResults();
}

class StencilTestInstance : public DiscardTestInstance
{
public:
	StencilTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
		: DiscardTestInstance(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, pickSupportedStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice()))
	{
		m_dynamicStates = { vk::VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK, vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK, vk::VK_DYNAMIC_STATE_STENCIL_REFERENCE };
		const auto features = context.getDeviceFeatures();
		m_depthBounds = features.depthBounds;

		DynamicStateBaseClass::initialize();
	}

	virtual void setDynamicState(void) {
		uint32_t value = 0x80;
		m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, value);
		m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, value);
		m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, value);
		m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, value);
		m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, value);
		m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, value);
	}

	virtual tcu::TestStatus verifyResults(void) {
		const vk::VkQueue					queue			= m_context.getUniversalQueue();
		const vk::VkOffset3D				zeroOffset		= { 0, 0, 0 };
		tcu::ConstPixelBufferAccess			renderedFrame	= m_depthStencilImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_STENCIL_BIT);
		de::SharedPtr<tcu::TextureLevel>	stencilFrame;

		if (tcu::isCombinedDepthStencilType(renderedFrame.getFormat().type))
		{
			stencilFrame = de::SharedPtr<tcu::TextureLevel>( new tcu::TextureLevel(tcu::TextureFormat(tcu::TextureFormat::S, tcu::TextureFormat::UNSIGNED_INT8), WIDTH, HEIGHT, 1u));

			tcu::copy(stencilFrame->getAccess(), tcu::getEffectiveDepthStencilAccess(renderedFrame, tcu::Sampler::MODE_STENCIL));
			renderedFrame = stencilFrame->getAccess();
		}

		for (int i = 0; i < WIDTH; ++i) {
			for (int j = 0; j < HEIGHT; ++j) {
				const tcu::Vec4 pixel = renderedFrame.getPixel(i, j);
				if (pixel[0] != 0.0f) {
					return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
				}
			}
		}
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class ViewportTestInstance : public DiscardTestInstance
{
public:
	ViewportTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
		: DiscardTestInstance(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, pickSupportedStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice()))
	{
		m_dynamicStates = { vk::VK_DYNAMIC_STATE_VIEWPORT };
		DynamicStateBaseClass::initialize();
	}

	virtual void setDynamicState(void) {
		vk::VkViewport viewport = vk::makeViewport(tcu::UVec2(WIDTH, HEIGHT));
		if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
		{
#ifndef CTS_USES_VULKANSC
			m_vk.cmdSetViewportWithCount(*m_cmdBuffer, 1, &viewport);
#else
			m_vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1, &viewport);
#endif
		}
		else
		{
			m_vk.cmdSetViewport(*m_cmdBuffer, 0, 1, &viewport);
		}
	}

	virtual tcu::TestStatus verifyResults(void) {
		const vk::VkQueue	queue = m_context.getUniversalQueue();
		tcu::Texture2D		referenceFrame(vk::mapVkFormat(m_depthStencilAttachmentFormat), WIDTH, HEIGHT);
		referenceFrame.allocLevel(0);

		const vk::VkOffset3D				zeroOffset = { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess	renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
			vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

		for (int i = 0; i < WIDTH; ++i) {
			for (int j = 0; j < HEIGHT; ++j) {
				const tcu::Vec4 pixel = renderedFrame.getPixel(i, j);
				if (pixel != tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
					return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
				}
			}
		}
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class ScissorTestInstance : public DiscardTestInstance
{
public:
	ScissorTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
		: DiscardTestInstance(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, pickSupportedStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice()))
	{
		m_dynamicStates = { vk::VK_DYNAMIC_STATE_SCISSOR };
		DynamicStateBaseClass::initialize();
	}

	virtual void setDynamicState(void) {
		vk::VkRect2D scissor = vk::makeRect2D(tcu::UVec2(WIDTH, HEIGHT));
		m_vk.cmdSetScissor(*m_cmdBuffer, 0, 1, &scissor);
	}

	virtual tcu::TestStatus verifyResults(void) {
		const vk::VkQueue	queue = m_context.getUniversalQueue();
		tcu::Texture2D		referenceFrame(vk::mapVkFormat(m_depthStencilAttachmentFormat), WIDTH, HEIGHT);
		referenceFrame.allocLevel(0);

		const vk::VkOffset3D				zeroOffset = { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess	renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
			vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

		for (int i = 0; i < WIDTH; ++i) {
			for (int j = 0; j < HEIGHT; ++j) {
				const tcu::Vec4 pixel = renderedFrame.getPixel(i, j);
				if (pixel != tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
					return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
				}
			}
		}
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class DepthTestInstance : public DiscardTestInstance
{
public:
	DepthTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
		: DiscardTestInstance(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, vk::VK_FORMAT_D32_SFLOAT)
	{
		m_dynamicStates = { vk::VK_DYNAMIC_STATE_DEPTH_BIAS, vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS };
		DynamicStateBaseClass::initialize();
	}

	virtual void setDynamicState(void) {
		m_vk.cmdSetDepthBounds(*m_cmdBuffer, 0.0f, 1.0f);
		m_vk.cmdSetDepthBias(*m_cmdBuffer, 1.0f, 1.0f, 1.0f);
	}

	virtual tcu::TestStatus verifyResults(void) {
		const vk::VkQueue					queue			= m_context.getUniversalQueue();
		const vk::VkOffset3D				zeroOffset		= { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess	renderedFrame	= m_depthStencilImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_DEPTH_BIT);

		for (int i = 0; i < WIDTH; ++i) {
			for (int j = 0; j < HEIGHT; ++j) {
				const tcu::Vec4 pixel = renderedFrame.getPixel(i, j);
				if (pixel[0] != 0.0f) {
					return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
				}
			}
		}
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class BlendTestInstance : public DiscardTestInstance
{
public:
	BlendTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
		: DiscardTestInstance(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, pickSupportedStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice()))
	{
		m_dynamicStates = { vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS };
		DynamicStateBaseClass::initialize();
	}

	virtual void setDynamicState(void) {
		float blendConstantsants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_vk.cmdSetBlendConstants(*m_cmdBuffer, blendConstantsants);
	}

	virtual tcu::TestStatus verifyResults(void) {
		const vk::VkQueue	queue = m_context.getUniversalQueue();
		tcu::Texture2D		referenceFrame(vk::mapVkFormat(m_depthStencilAttachmentFormat), WIDTH, HEIGHT);
		referenceFrame.allocLevel(0);

		const vk::VkOffset3D				zeroOffset = { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess	renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
			vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

		for (int i = 0; i < WIDTH; ++i) {
			for (int j = 0; j < HEIGHT; ++j) {
				const tcu::Vec4 pixel = renderedFrame.getPixel(i, j);
				if (pixel != tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
					return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
				}
			}
		}
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class LineTestInstance : public DiscardTestInstance
{
public:
	LineTestInstance(Context& context, vk::PipelineConstructionType pipelineConstructionType, const char* vertexShaderName, const char* fragmentShaderName)
		: DiscardTestInstance(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, pickSupportedStencilFormat(context.getInstanceInterface(), context.getPhysicalDevice()))
	{
		m_dynamicStates = { vk::VK_DYNAMIC_STATE_LINE_WIDTH };
		DynamicStateBaseClass::initialize();
	}

	virtual void setDynamicState(void) {
		m_vk.cmdSetLineWidth(*m_cmdBuffer, 1.0f);
	}

	virtual tcu::TestStatus verifyResults(void) {
		const vk::VkQueue	queue = m_context.getUniversalQueue();
		tcu::Texture2D		referenceFrame(vk::mapVkFormat(m_depthStencilAttachmentFormat), WIDTH, HEIGHT);
		referenceFrame.allocLevel(0);

		const vk::VkOffset3D				zeroOffset = { 0, 0, 0 };
		const tcu::ConstPixelBufferAccess	renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
			vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

		for (int i = 0; i < WIDTH; ++i) {
			for (int j = 0; j < HEIGHT; ++j) {
				const tcu::Vec4 pixel = renderedFrame.getPixel(i, j);
				if (pixel != tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
					return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
				}
			}
		}
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
	}
};

class DiscardTestCase : public vkt::TestCase
{
public:
	DiscardTestCase (tcu::TestContext& context, const char* name, const char* description, vk::PipelineConstructionType pipelineConstructionType, TestDynamicStateDiscard testCase)
		: TestCase	(context, name, description)
		, m_pipelineConstructionType(pipelineConstructionType)
		, m_testCase(testCase)
		, m_depthBounds(false)
	{
	}

	virtual void	checkSupport(Context& context) const
	{
		checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
	}

	TestInstance* createInstance(Context& context) const
	{
		switch (m_testCase) {
		case TEST_STENCIL:
			return new StencilTestInstance(context, m_pipelineConstructionType, "discard.vert", "discard.frag");
		case TEST_VIEWPORT:
			return new ViewportTestInstance(context, m_pipelineConstructionType, "discard.vert", "discard.frag");
		case TEST_SCISSOR:
			return new ScissorTestInstance(context, m_pipelineConstructionType, "discard.vert", "discard.frag");
		case TEST_DEPTH:
			return new DepthTestInstance(context, m_pipelineConstructionType, "discard.vert", "discard.frag");
		case TEST_BLEND_CONSTANTS:
			return new BlendTestInstance(context, m_pipelineConstructionType, "discard.vert", "discard.frag");
		case TEST_LINE_WIDTH:
			return new LineTestInstance(context, m_pipelineConstructionType, "discard.vert", "discard.frag");
		default:
			break;
		}
		DE_ASSERT(false);
		return nullptr;
	}

	virtual void initPrograms(vk::SourceCollections& programCollection) const
	{
		std::ostringstream vert;
		vert
			<< "#version 450\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;"
			<< "layout(location = 1) in vec4 in_color;"
			<< "\n"
			<< "layout(location = 0) out vec4 out_color;"
			<< "out gl_PerVertex { vec4 gl_Position; };"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "	gl_Position = in_position;\n"
			<< "	out_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("discard.vert") << glu::VertexSource(vert.str());

		std::ostringstream frag;
		frag
			<< "#version 450\n"
			<< "\n"
			<< "layout (set=0, binding=0, std140) uniform InputBlock {\n"
			<< "	int discard_all;\n"
			<< "} unif;\n"
			<< "\n"
			<< "layout (location = 0) in vec4 in_color;"
			<< "\n"
			<< "layout (location = 0) out vec4 color;"
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "	if (unif.discard_all == 0) {\n"
			<< "		discard;\n"
			<< "	}\n"
			<< "	color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("discard.frag") << glu::FragmentSource(frag.str());
	}

protected:
	vk::PipelineConstructionType	m_pipelineConstructionType;
	TestDynamicStateDiscard			m_testCase;
	VkBool32						m_depthBounds;
};

DynamicStateDiscardTests::DynamicStateDiscardTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
	: TestCaseGroup					(testCtx, "discard", "Tests for dynamic state")
	, m_pipelineConstructionType	(pipelineConstructionType)
{
	/* Left blank on purpose */
}

DynamicStateDiscardTests::~DynamicStateDiscardTests ()
{
}

void DynamicStateDiscardTests::init (void)
{
	addChild(new DiscardTestCase(m_testCtx, "stencil", "Use dynamic stencil with discard", m_pipelineConstructionType, TestDynamicStateDiscard::TEST_STENCIL));
	addChild(new DiscardTestCase(m_testCtx, "viewport", "Use dynamic viewport with discard", m_pipelineConstructionType, TestDynamicStateDiscard::TEST_VIEWPORT));
	addChild(new DiscardTestCase(m_testCtx, "scissor", "Use dynamic scissor with discard", m_pipelineConstructionType, TestDynamicStateDiscard::TEST_SCISSOR));
	addChild(new DiscardTestCase(m_testCtx, "depth", "Use dynamic depth with discard", m_pipelineConstructionType, TestDynamicStateDiscard::TEST_DEPTH));
	addChild(new DiscardTestCase(m_testCtx, "blend", "Use dynamic blend constants with discard", m_pipelineConstructionType, TestDynamicStateDiscard::TEST_BLEND_CONSTANTS));
	addChild(new DiscardTestCase(m_testCtx, "line", "Use dynamic line width with discard", m_pipelineConstructionType, TestDynamicStateDiscard::TEST_LINE_WIDTH));
}

} // DynamicState
} // vkt
