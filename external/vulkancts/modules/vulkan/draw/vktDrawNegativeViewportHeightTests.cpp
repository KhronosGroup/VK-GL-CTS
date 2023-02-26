/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Negative viewport height (part of VK_KHR_maintenance1)
 *//*--------------------------------------------------------------------*/

#include "vktDrawNegativeViewportHeightTests.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include "deSharedPtr.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;
using tcu::Vec4;
using de::SharedPtr;
using de::MovePtr;

class DynRenderHelper
{
public:
	DynRenderHelper (const SharedGroupParams params)
		: m_params(params)
		{}

	void beginSecondaryCmdBuffer (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer, const VkFormat& colorAttachmentFormat) const
	{
#ifndef CTS_USES_VULKANSC
		VkRenderingFlags renderingFlags = 0u;
		if (m_params->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			renderingFlags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

		VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType					sType;
			DE_NULL,																// const void*						pNext;
			renderingFlags,															// VkRenderingFlagsKHR				flags;
			0u,																		// uint32_t							viewMask;
			1u,																		// uint32_t							colorAttachmentCount;
			&colorAttachmentFormat,													// const VkFormat*					pColorAttachmentFormats;
			VK_FORMAT_UNDEFINED,													// VkFormat							depthAttachmentFormat;
			VK_FORMAT_UNDEFINED,													// VkFormat							stencilAttachmentFormat;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			rasterizationSamples;
		};
		const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

		VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (!m_params->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

		const VkCommandBufferBeginInfo commandBufBeginParams
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
			DE_NULL,																// const void*						pNext;
			usageFlags,																// VkCommandBufferUsageFlags		flags;
			&bufferInheritanceInfo
		};

		VK_CHECK(vkd.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
#else
		DE_UNREF(vkd);
		DE_UNREF(cmdBuffer);
		DE_UNREF(colorAttachmentFormat);
		DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
	}

	void beginRendering(const DeviceInterface&		vkd,
						const VkCommandBuffer		cmdBuffer,
						const bool					isPrimaryCmdBuffer,
						const VkImageView			colorImageView,
						const VkRect2D&				renderArea,
						const VkClearValue&			clearValue,
						const VkImageLayout			imageLayout) const
	{
#ifndef CTS_USES_VULKANSC
		VkRenderingFlagsKHR renderingFlags = 0u;
		if (isPrimaryCmdBuffer && m_params->useSecondaryCmdBuffer && !m_params->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			renderingFlags |= VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;

		vk::beginRendering(vkd, cmdBuffer, colorImageView, renderArea, clearValue, imageLayout, VK_ATTACHMENT_LOAD_OP_LOAD, renderingFlags);
#else
		DE_UNREF(vkd);
		DE_UNREF(cmdBuffer);
		DE_UNREF(isPrimaryCmdBuffer);
		DE_UNREF(colorImageView);
		DE_UNREF(renderArea);
		DE_UNREF(clearValue);
		DE_UNREF(imageLayout);
		DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
	}

protected:
	SharedGroupParams	m_params;
};

enum Constants
{
	WIDTH	= 256,
	HEIGHT	= WIDTH/2,
};

struct TestParams
{
	VkFrontFace					frontFace;
	VkCullModeFlagBits			cullMode;
	bool						zeroViewportHeight;
	const SharedGroupParams		groupParams;
};

class NegativeViewportHeightTestInstance : public TestInstance
{
public:
									NegativeViewportHeightTestInstance	(Context& context, const TestParams& params);
	tcu::TestStatus					iterate								(void);
	void							preRenderCommands					(VkCommandBuffer cmdBuffer, const VkClearValue& clearColor);
	void							draw								(VkCommandBuffer cmdBuffer, const VkViewport& viewport);

	MovePtr<tcu::TextureLevel>		generateReferenceImage				(void) const;
	bool							isCulled							(const VkFrontFace triangleFace) const;

private:
	const TestParams				m_params;
	const DynRenderHelper			m_dynRenderHelper;
	const VkFormat					m_colorAttachmentFormat;
	SharedPtr<Image>				m_colorTargetImage;
	Move<VkImageView>				m_colorTargetView;
	SharedPtr<Buffer>				m_vertexBuffer;
	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;
	Move<VkPipelineLayout>			m_pipelineLayout;
	Move<VkPipeline>				m_pipeline;
};

NegativeViewportHeightTestInstance::NegativeViewportHeightTestInstance (Context& context, const TestParams& params)
	: TestInstance				(context)
	, m_params					(params)
	, m_dynRenderHelper			(params.groupParams)
	, m_colorAttachmentFormat	(VK_FORMAT_R8G8B8A8_UNORM)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	// Vertex data
	{
		std::vector<Vec4> vertexData;

		// CCW triangle
		vertexData.push_back(Vec4(-0.8f, -0.6f, 0.0f, 1.0f));	//  0-----2
		vertexData.push_back(Vec4(-0.8f,  0.6f, 0.0f, 1.0f));	//   |  /
		vertexData.push_back(Vec4(-0.2f, -0.6f, 0.0f, 1.0f));	//  1|/

		// CW triangle
		vertexData.push_back(Vec4( 0.2f, -0.6f, 0.0f, 1.0f));	//  0-----1
		vertexData.push_back(Vec4( 0.8f, -0.6f, 0.0f, 1.0f));	//    \  |
		vertexData.push_back(Vec4( 0.8f,  0.6f, 0.0f, 1.0f));	//      \|2

		const VkDeviceSize dataSize = vertexData.size() * sizeof(Vec4);
		m_vertexBuffer = Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
												m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), &vertexData[0], static_cast<std::size_t>(dataSize));
		flushMappedMemoryRange(vk, device, m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}

	const VkExtent3D		targetImageExtent		= { WIDTH, HEIGHT, 1 };
	const VkImageUsageFlags	targetImageUsageFlags	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	const ImageCreateInfo	targetImageCreateInfo(
		VK_IMAGE_TYPE_2D,						// imageType,
		m_colorAttachmentFormat,				// format,
		targetImageExtent,						// extent,
		1u,										// mipLevels,
		1u,										// arrayLayers,
		VK_SAMPLE_COUNT_1_BIT,					// samples,
		VK_IMAGE_TILING_OPTIMAL,				// tiling,
		targetImageUsageFlags);					// usage,

	m_colorTargetImage = Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
	m_colorTargetView = createImageView(vk, device, &colorTargetViewInfo);

	// Render pass and framebuffer
	if (!m_params.groupParams->useDynamicRendering)
	{
		RenderPassCreateInfo	renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(
			m_colorAttachmentFormat,				// format
			VK_SAMPLE_COUNT_1_BIT,					// samples
			VK_ATTACHMENT_LOAD_OP_LOAD,				// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,			// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,		// stencilStoreOp
			VK_IMAGE_LAYOUT_GENERAL,				// initialLayout
			VK_IMAGE_LAYOUT_GENERAL));				// finalLayout

		const VkAttachmentReference colorAttachmentReference =
		{
			0u,
			VK_IMAGE_LAYOUT_GENERAL
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(
			VK_PIPELINE_BIND_POINT_GRAPHICS,		// pipelineBindPoint
			(VkSubpassDescriptionFlags)0,			// flags
			0u,										// inputAttachmentCount
			DE_NULL,								// inputAttachments
			1u,										// colorAttachmentCount
			&colorAttachmentReference,				// colorAttachments
			DE_NULL,								// resolveAttachments
			AttachmentReference(),					// depthStencilAttachment
			0u,										// preserveAttachmentCount
			DE_NULL));								// preserveAttachments

		m_renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<VkImageView>		colorAttachments		{ *m_colorTargetView };
		const FramebufferCreateInfo		framebufferCreateInfo	(*m_renderPass, colorAttachments, WIDTH, HEIGHT, 1);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	// Vertex input

	const VkVertexInputBindingDescription		vertexInputBindingDescription =
	{
		0u,										// uint32_t             binding;
		sizeof(Vec4),							// uint32_t             stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate    inputRate;
	};

	const VkVertexInputAttributeDescription		vertexInputAttributeDescription =
	{
		0u,										// uint32_t    location;
		0u,										// uint32_t    binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat    format;
		0u										// uint32_t    offset;
	};

	const PipelineCreateInfo::VertexInputState	vertexInputState = PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription,
																										1, &vertexInputAttributeDescription);

	// Graphics pipeline

	const VkRect2D scissor = makeRect2D(WIDTH, HEIGHT);

	std::vector<VkDynamicState>		dynamicStates;
	dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);

	const Unique<VkShaderModule>	vertexModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>	fragmentModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

	const PipelineLayoutCreateInfo	pipelineLayoutCreateInfo;
	m_pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const PipelineCreateInfo::ColorBlendState::Attachment colorBlendAttachmentState;

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vertexModule,   "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fragmentModule, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState (PipelineCreateInfo::VertexInputState	(vertexInputState));
	pipelineCreateInfo.addState (PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
	pipelineCreateInfo.addState (PipelineCreateInfo::ColorBlendState	(1, &colorBlendAttachmentState));
	pipelineCreateInfo.addState (PipelineCreateInfo::ViewportState		(1, std::vector<VkViewport>(), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState (PipelineCreateInfo::DepthStencilState	());
	pipelineCreateInfo.addState (PipelineCreateInfo::RasterizerState	(
		VK_FALSE,					// depthClampEnable
		VK_FALSE,					// rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,		// polygonMode
		m_params.cullMode,			// cullMode
		m_params.frontFace,			// frontFace
		VK_FALSE,					// depthBiasEnable
		0.0f,						// depthBiasConstantFactor
		0.0f,						// depthBiasClamp
		0.0f,						// depthBiasSlopeFactor
		1.0f));						// lineWidth
	pipelineCreateInfo.addState (PipelineCreateInfo::MultiSampleState	());
	pipelineCreateInfo.addState (PipelineCreateInfo::DynamicState		(dynamicStates));

#ifndef CTS_USES_VULKANSC
	vk::VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		DE_NULL,
		0u,
		1u,
		&m_colorAttachmentFormat,
		vk::VK_FORMAT_UNDEFINED,
		vk::VK_FORMAT_UNDEFINED
	};

	if (m_params.groupParams->useDynamicRendering)
		pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

void NegativeViewportHeightTestInstance::preRenderCommands(VkCommandBuffer cmdBuffer, const VkClearValue& clearColor)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const ImageSubresourceRange	subresourceRange	(VK_IMAGE_ASPECT_COLOR_BIT);

	initialTransitionColor2DImage(vk, cmdBuffer, m_colorTargetImage->object(), VK_IMAGE_LAYOUT_GENERAL,
								  VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vk.cmdClearColorImage(cmdBuffer, m_colorTargetImage->object(), VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &subresourceRange);

	const VkMemoryBarrier memBarrier
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,												// VkStructureType		sType;
		DE_NULL,																		// const void*			pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,													// VkAccessFlags		srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT		// VkAccessFlags		dstAccessMask;
	};

	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
}

void NegativeViewportHeightTestInstance::draw (VkCommandBuffer cmdBuffer, const VkViewport& viewport)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkBuffer			buffer	= m_vertexBuffer->object();
	const VkDeviceSize		offset	= 0;

	if (m_params.zeroViewportHeight)
	{
		// Set zero viewport height
		const VkViewport zeroViewportHeight
		{
			viewport.x,			// float    x;
			viewport.y / 2.0f,	// float    y;
			viewport.width,		// float    width;
			0.0f,				// float    height;
			viewport.minDepth,	// float    minDepth;
			viewport.maxDepth	// float    maxDepth;
		};

		vk.cmdSetViewport(cmdBuffer, 0u, 1u, &zeroViewportHeight);
	}
	else
		vk.cmdSetViewport(cmdBuffer, 0u, 1u, &viewport);

	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offset);
	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	vk.cmdDraw(cmdBuffer, 6, 1, 0, 0);
}

//! Determine if a triangle with triangleFace orientation will be culled or not
bool NegativeViewportHeightTestInstance::isCulled (const VkFrontFace triangleFace) const
{
	const bool isFrontFacing = (triangleFace == m_params.frontFace);

	if (m_params.cullMode == VK_CULL_MODE_FRONT_BIT && isFrontFacing)
		return true;
	if (m_params.cullMode == VK_CULL_MODE_BACK_BIT  && !isFrontFacing)
		return true;

	return m_params.cullMode == VK_CULL_MODE_FRONT_AND_BACK;
}

MovePtr<tcu::TextureLevel> NegativeViewportHeightTestInstance::generateReferenceImage (void) const
{
	DE_ASSERT(HEIGHT == WIDTH/2);

	MovePtr<tcu::TextureLevel>		image	(new tcu::TextureLevel(mapVkFormat(m_colorAttachmentFormat), WIDTH, HEIGHT));
	const tcu::PixelBufferAccess	access	(image->getAccess());
	const Vec4						blue	(0.125f, 0.25f, 0.5f, 1.0f);
	const Vec4						white	(1.0f);
	const Vec4						gray	(0.5f, 0.5f, 0.5f, 1.0f);

	tcu::clear(access, blue);

	// Zero viewport height
	if (m_params.zeroViewportHeight)
	{
		return image;
	}
	// Negative viewport height
	else
	{
		const int p1 =      static_cast<int>(static_cast<float>(HEIGHT) * (1.0f - 0.6f) / 2.0f);
		const int p2 = p1 + static_cast<int>(static_cast<float>(HEIGHT) * (2.0f * 0.6f) / 2.0f);

		// left triangle (CCW -> CW after y-flip)
		if (!isCulled(VK_FRONT_FACE_CLOCKWISE))
		{
			const Vec4& color = (m_params.frontFace == VK_FRONT_FACE_CLOCKWISE ? white : gray);

			for (int y = p1; y <= p2; ++y)
			for (int x = p1; x <  y;  ++x)
				access.setPixel(color, x, y);
		}

		// right triangle (CW -> CCW after y-flip)
		if (!isCulled(VK_FRONT_FACE_COUNTER_CLOCKWISE))
		{
			const Vec4& color = (m_params.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE ? white : gray);

			for (int y = p1;        y <= p2;          ++y)
			for (int x = WIDTH - y; x <  p2 + HEIGHT; ++x)
				access.setPixel(color, x, y);
		}

		return image;
	}
}

std::string getCullModeStr (const VkCullModeFlagBits cullMode)
{
	// Cull mode flags are a bit special, because there's a meaning to 0 and or'ed flags.
	// The function getCullModeFlagsStr() doesn't work too well in this case.

	switch (cullMode)
	{
		case VK_CULL_MODE_NONE:				return "VK_CULL_MODE_NONE";
		case VK_CULL_MODE_FRONT_BIT:		return "VK_CULL_MODE_FRONT_BIT";
		case VK_CULL_MODE_BACK_BIT:			return "VK_CULL_MODE_BACK_BIT";
		case VK_CULL_MODE_FRONT_AND_BACK:	return "VK_CULL_MODE_FRONT_AND_BACK";

		default:
			DE_ASSERT(0);
			return std::string();
	}
}

tcu::TestStatus NegativeViewportHeightTestInstance::iterate (void)
{
	// Set up the viewport and draw

	const VkViewport viewport
	{
		0.0f,							// float    x;
		static_cast<float>(HEIGHT),		// float    y;
		static_cast<float>(WIDTH),		// float    width;
		-static_cast<float>(HEIGHT),	// float    height;
		0.0f,							// float    minDepth;
		1.0f,							// float    maxDepth;
	};
	VkRect2D rect = makeRect2D(0, 0, WIDTH, HEIGHT);

	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkClearValue				clearColor			= makeClearValueColorF32(0.125f, 0.25f, 0.5f, 1.0f);
	const CmdPoolCreateInfo			cmdPoolCreateInfo	(queueFamilyIndex);
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	Move<VkCommandBuffer>			secCmdBuffer;

#ifndef CTS_USES_VULKANSC
	if (m_params.groupParams->useSecondaryCmdBuffer)
	{
		secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

		// record secondary command buffer
		m_dynRenderHelper.beginSecondaryCmdBuffer(vk, *secCmdBuffer, m_colorAttachmentFormat);

		if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			m_dynRenderHelper.beginRendering(vk, *secCmdBuffer, false/*isPrimary*/, *m_colorTargetView, rect, clearColor, VK_IMAGE_LAYOUT_GENERAL);

		draw(*secCmdBuffer, viewport);

		if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endRendering(vk, *secCmdBuffer);

		endCommandBuffer(vk, *secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(vk, *cmdBuffer, 0u);

		preRenderCommands(*cmdBuffer, clearColor);

		if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			m_dynRenderHelper.beginRendering(vk, *cmdBuffer, true/*isPrimary*/, *m_colorTargetView, rect, clearColor, VK_IMAGE_LAYOUT_GENERAL);

		vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

		if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endRendering(vk, *cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);
	}
	else if (m_params.groupParams->useDynamicRendering)
	{
		beginCommandBuffer(vk, *cmdBuffer);

		preRenderCommands(*cmdBuffer, clearColor);
		m_dynRenderHelper.beginRendering(vk, *cmdBuffer, true/*isPrimary*/, *m_colorTargetView, rect, clearColor, VK_IMAGE_LAYOUT_GENERAL);
		draw(*cmdBuffer, viewport);
		endRendering(vk, *cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_params.groupParams->useDynamicRendering)
	{
		beginCommandBuffer(vk, *cmdBuffer);

		preRenderCommands(*cmdBuffer, clearColor);
		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, rect);
		draw(*cmdBuffer, viewport);
		endRenderPass(vk, *cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);
	}

	// Submit
	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Get result
	const VkOffset3D					zeroOffset	= { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess	resultImage	= m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, VK_IMAGE_ASPECT_COLOR_BIT);

	// Verify the results

	tcu::TestLog&				log				= m_context.getTestContext().getLog();
	MovePtr<tcu::TextureLevel>	referenceImage	= generateReferenceImage();

	// Zero viewport height
	if (m_params.zeroViewportHeight)
	{
		log << tcu::TestLog::Message
			<< "Drawing two triangles with zero viewport height."
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message
			<< "Result image should be empty."
			<< tcu::TestLog::EndMessage;
	}
	// Negative viewport height
	else
	{
		log << tcu::TestLog::Message
			<< "Drawing two triangles with negative viewport height, which will cause a y-flip. This changes the sign of the triangle's area."
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message
			<< "After the flip, the triangle on the left is CW and the triangle on the right is CCW. Right angles of the both triangles should be at the bottom of the image."
			<< " Front face is white, back face is gray."
			<< tcu::TestLog::EndMessage;
	}

	log << tcu::TestLog::Message
		<< "Front face: " << getFrontFaceName(m_params.frontFace) << "\n"
		<< "Cull mode: "  << getCullModeStr  (m_params.cullMode)  << "\n"
		<< tcu::TestLog::EndMessage;

	if (!tcu::fuzzyCompare(log, "Image compare", "Image compare", referenceImage->getAccess(), resultImage, 0.02f, tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Rendered image is incorrect");
	else
		return tcu::TestStatus::pass("Pass");
}

class NegativeViewportHeightTest : public TestCase
{
public:
	NegativeViewportHeightTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
		: TestCase	(testCtx, name, description)
		, m_params	(params)
	{
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 in_position;\n"
				<< "\n"
				<< "out gl_PerVertex {\n"
				<< "    vec4  gl_Position;\n"
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    gl_Position = in_position;\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Fragment shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    if (gl_FrontFacing)\n"
				<< "        out_color = vec4(1.0);\n"
				<< "    else\n"
				<< "        out_color = vec4(vec3(0.5), 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
	}

	virtual void checkSupport (Context& context) const
	{
		if (m_params.groupParams->useDynamicRendering)
			context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

		context.requireDeviceFunctionality("VK_KHR_maintenance1");
	}

	virtual TestInstance* createInstance (Context& context) const
	{
		return new NegativeViewportHeightTestInstance(context, m_params);
	}

private:
	const TestParams	m_params;
};

struct SubGroupParams
{
	bool					zeroViewportHeight;
	const SharedGroupParams	groupParams;
};

void populateTestGroup (tcu::TestCaseGroup* testGroup, SubGroupParams subGroupParams)
{
	const struct
	{
		const char* const	name;
		VkFrontFace			frontFace;
	} frontFace[] =
	{
		{ "front_ccw",	VK_FRONT_FACE_COUNTER_CLOCKWISE	},
		{ "front_cw",	VK_FRONT_FACE_CLOCKWISE			},
	};

	const struct
	{
		const char* const	name;
		VkCullModeFlagBits	cullMode;
	} cullMode[] =
	{
		{ "cull_none",	VK_CULL_MODE_NONE			},
		{ "cull_front",	VK_CULL_MODE_FRONT_BIT		},
		{ "cull_back",	VK_CULL_MODE_BACK_BIT		},
		{ "cull_both",	VK_CULL_MODE_FRONT_AND_BACK	},
	};

	for (int ndxFrontFace = 0; ndxFrontFace < DE_LENGTH_OF_ARRAY(frontFace); ++ndxFrontFace)
	for (int ndxCullMode  = 0; ndxCullMode  < DE_LENGTH_OF_ARRAY(cullMode);  ++ndxCullMode)
	{
		const TestParams params =
		{
			frontFace[ndxFrontFace].frontFace,
			cullMode[ndxCullMode].cullMode,
			subGroupParams.zeroViewportHeight,
			subGroupParams.groupParams
		};
		std::ostringstream	name;
		name << frontFace[ndxFrontFace].name << "_" << cullMode[ndxCullMode].name;

		testGroup->addChild(new NegativeViewportHeightTest(testGroup->getTestContext(), name.str(), "", params));
	}
}

enum class OffScreenAxisCase
{
	ONSCREEN		= 0,
	NEGATIVE_SIDE	= 1,
	POSITIVE_SIDE	= 2,
};

struct OffScreenParams
{
	const uint32_t			randomSeed;
	const OffScreenAxisCase	xAxis;
	const OffScreenAxisCase	yAxis;
	const bool				negativeHeight;
	const SharedGroupParams	groupParams;

	OffScreenParams (uint32_t seed, OffScreenAxisCase x, OffScreenAxisCase y, bool negH, const SharedGroupParams gp)
		: randomSeed		(seed)
		, xAxis				(x)
		, yAxis				(y)
		, negativeHeight	(negH)
		, groupParams		(gp)
	{
		// At least one of them must be offscreen.
		DE_ASSERT(xAxis != OffScreenAxisCase::ONSCREEN || yAxis != OffScreenAxisCase::ONSCREEN);
	}
};

class OffScreenViewportCase : public vkt::TestCase
{
public:
	static constexpr int32_t	kFramebufferSize	= 32;	// Width and Height of framebuffer.
	static constexpr int32_t	kViewportMaxDim		= 1024;	// When generating offscreen coords, use this limit as the negative or positive max coord for X/Y.
	static constexpr uint32_t	kVertexCount		= 4u;

	// Choose a couple of values for the Axis range (X or Y) according to the chosen Axis case.
	static tcu::IVec2 genAxis (de::Random& rnd, OffScreenAxisCase axisCase)
	{
		int32_t minVal = 0;
		int32_t maxVal = 0;

		if (axisCase == OffScreenAxisCase::ONSCREEN)
			maxVal = kFramebufferSize - 1;
		else if (axisCase == OffScreenAxisCase::NEGATIVE_SIDE)
		{
			minVal = -kViewportMaxDim;
			maxVal = -1;
		}
		else if (axisCase == OffScreenAxisCase::POSITIVE_SIDE)
		{
			minVal = kFramebufferSize + 1;
			maxVal = kViewportMaxDim;
		}

		const auto a = rnd.getInt(minVal, maxVal);
		const auto b = rnd.getInt(minVal, maxVal);

		const tcu::IVec2 axisRange (de::min(a, b), de::max(a, b));
		return axisRange;
	}

					OffScreenViewportCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const OffScreenParams& params)
						: vkt::TestCase	(testCtx, name, description)
						, m_params		(params)
						{}

	virtual			~OffScreenViewportCase	(void) {}

	void			initPrograms	(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance	(Context& context) const override;
	void			checkSupport	(Context& context) const override;

protected:
	const OffScreenParams m_params;
};

class OffScreenViewportInstance : public vkt::TestInstance
{
public:
						OffScreenViewportInstance	(Context& context, const OffScreenParams& params)
							: vkt::TestInstance	(context)
							, m_params			(params)
							, m_dynRenderHelper	(params.groupParams)
							{}

	virtual				~OffScreenViewportInstance	(void) {}

	tcu::TestStatus		iterate						(void) override;

protected:
	const OffScreenParams	m_params;
	const DynRenderHelper	m_dynRenderHelper;
};

TestInstance* OffScreenViewportCase::createInstance (Context &context) const
{
	return new OffScreenViewportInstance(context, m_params);
}

void OffScreenViewportCase::checkSupport (Context &context) const
{
	if (m_params.groupParams->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (m_params.negativeHeight)
		context.requireDeviceFunctionality("VK_KHR_maintenance1");
}

void OffScreenViewportCase::initPrograms (vk::SourceCollections &programCollection) const
{
	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "const int vertexCount = " << kVertexCount << ";\n"
		<< "vec2 positions[vertexCount] = vec2[](\n"
		<< "    vec2(-1.0, -1.0),\n"
		<< "    vec2(-1.0,  1.0),\n"
		<< "    vec2( 1.0, -1.0),\n"
		<< "    vec2( 1.0,  1.0)\n"
		<< ");\n"
		<< "void main (void) { gl_Position = vec4(positions[gl_VertexIndex % vertexCount], 0.0, 1.0); }\n"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main (void) { outColor = vec4(0.0, 0.0, 1.0, 1.0); }\n"
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus OffScreenViewportInstance::iterate (void)
{
	de::Random rnd(m_params.randomSeed);

	// Pseudorandomly generate viewport data.
	const auto	xAxis	= OffScreenViewportCase::genAxis(rnd, m_params.xAxis);
	auto		yAxis	= OffScreenViewportCase::genAxis(rnd, m_params.yAxis);
	const auto	width	= xAxis.y() - xAxis.x() + 1;
	auto		height	= yAxis.y() - yAxis.x() + 1;

	if (m_params.negativeHeight)
	{
		height = -height;
		std::swap(yAxis[0], yAxis[1]);
	}

	const VkViewport testViewport =
	{
		static_cast<float>(xAxis.x()),	//	float	x;
		static_cast<float>(yAxis.x()),	//	float	y;
		static_cast<float>(width),		//	float	width;
		static_cast<float>(height),		//	float	height;
		0.0f,							//	float	minDepth;
		1.0f,							//	float	maxDepth;
	};

	// Framebuffer parameters.
	const auto kIFbSize	= OffScreenViewportCase::kFramebufferSize;
	const auto fbSize	= static_cast<uint32_t>(kIFbSize);
	const auto fbExtent	= makeExtent3D(fbSize, fbSize, 1u);
	const auto fbFormat	= VK_FORMAT_R8G8B8A8_UNORM;
	const auto fbUsage	= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	const auto&				ctx			= m_context.getContextCommonData();
	CommandPoolWithBuffer	cmd			(ctx.vkd, ctx.device, ctx.qfIndex);
	ImageWithBuffer			colorRes	(ctx.vkd, ctx.device, ctx.allocator, fbExtent, fbFormat, fbUsage, VK_IMAGE_TYPE_2D);

	const auto&		binaries	= m_context.getBinaryCollection();
	const auto		vertModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto		fragModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

	// Render pass and framebuffer.
	const auto		renderPass	= makeRenderPass(ctx.vkd, ctx.device, fbFormat, VK_FORMAT_UNDEFINED /* DS format */, VK_ATTACHMENT_LOAD_OP_LOAD);
	const auto		framebuffer	= makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), colorRes.getImageView(), fbExtent.width, fbExtent.height);

	// Pipeline.
	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

	const std::vector<VkViewport>	viewports(1u, testViewport);
	const std::vector<VkRect2D>		scissors (1u, makeRect2D(fbExtent));

	const auto pipelineLayout	= makePipelineLayout(ctx.vkd, ctx.device);
	const auto pipelineRP		= (m_params.groupParams->useDynamicRendering ? VK_NULL_HANDLE : renderPass.get());
	const auto pipeline			= makeGraphicsPipeline(ctx.vkd, ctx.device, pipelineLayout.get(),
		vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(),
		pipelineRP, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputStateCreateInfo);

	const auto cmdBuffer		= cmd.cmdBuffer.get();
	const auto secCmdBufferPtr	= (m_params.groupParams->useSecondaryCmdBuffer
								? allocateCommandBuffer(ctx.vkd, ctx.device, cmd.cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY)
								: Move<VkCommandBuffer>());
	const auto secCmdBuffer		= secCmdBufferPtr.get();
	const auto clearColor		= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const auto clearColorVal	= makeClearValueColorVec4(clearColor);
	const auto colorSRR			= makeDefaultImageSubresourceRange();

	// Draw (offscreen due to the viewport).
	beginCommandBuffer(ctx.vkd, cmdBuffer);

	// Clear color image outside render pass.
	const auto preClearBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		colorRes.getImage(), colorSRR);
	cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);

	ctx.vkd.cmdClearColorImage(cmdBuffer, colorRes.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorVal.color, 1u, &colorSRR);

	const auto postClearBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		colorRes.getImage(), colorSRR);
	cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);

	// Render pass.
	if (!m_params.groupParams->useDynamicRendering)
	{
		beginRenderPass(ctx.vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u));
		ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		ctx.vkd.cmdDraw(cmdBuffer, OffScreenViewportCase::kVertexCount, 1u, 0u, 0u);
		endRenderPass(ctx.vkd, cmdBuffer);
	}
	else
	{
#ifndef CTS_USES_VULKANSC
		const bool secondary				= m_params.groupParams->useSecondaryCmdBuffer;
		const bool allInSecondary			= m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass;
		const auto beginEndCmdBuffer		= (allInSecondary ? secCmdBuffer : cmdBuffer);
		const auto rpContentsCmdBuffer		= (secondary ? secCmdBuffer : cmdBuffer);
		const auto endAndExecuteSecondary	= [&cmdBuffer, &secCmdBuffer, &ctx](void)
			{
				endCommandBuffer(ctx.vkd, secCmdBuffer);
				ctx.vkd.cmdExecuteCommands(cmdBuffer, 1u, &secCmdBuffer);
			};

		if (secondary)
			m_dynRenderHelper.beginSecondaryCmdBuffer(ctx.vkd, secCmdBuffer, fbFormat);

		m_dynRenderHelper.beginRendering(ctx.vkd, beginEndCmdBuffer, !allInSecondary/*isPrimary*/, colorRes.getImageView(), scissors.at(0), clearColorVal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		ctx.vkd.cmdBindPipeline(rpContentsCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		ctx.vkd.cmdDraw(rpContentsCmdBuffer, OffScreenViewportCase::kVertexCount, 1u, 0u, 0u);
		if (secondary && !allInSecondary)
			endAndExecuteSecondary();
		endRendering(ctx.vkd, beginEndCmdBuffer);

		if (secondary && allInSecondary)
			endAndExecuteSecondary();
#else
		DE_UNREF(secCmdBuffer);
		DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
	}

	// Copy to results buffer.
	copyImageToBuffer(ctx.vkd, cmdBuffer, colorRes.getImage(), colorRes.getBuffer(), tcu::IVec2(kIFbSize, kIFbSize),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		1u, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

	// Verify color buffer.
	invalidateAlloc(ctx.vkd, ctx.device, colorRes.getBufferAllocation());

	const tcu::ConstPixelBufferAccess	resultAccess	(mapVkFormat(fbFormat), tcu::IVec3(kIFbSize, kIFbSize, 1), colorRes.getBufferAllocation().getHostPtr());
	auto&								log				= m_context.getTestContext().getLog();
	const tcu::Vec4						threshold		(0.0f, 0.0f, 0.0f, 0.0f);

	if (!tcu::floatThresholdCompare(log, "Result", "", clearColor, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected color result; check log for details");

	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

tcu::TestCaseGroup*	createNegativeViewportHeightTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	SubGroupParams subGroupParams { false, groupParams };
	return createTestGroup(testCtx, "negative_viewport_height", "Negative viewport height (VK_KHR_maintenance1)", populateTestGroup, subGroupParams);
}

tcu::TestCaseGroup*	createZeroViewportHeightTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	SubGroupParams subGroupParams{ false, groupParams };
	return createTestGroup(testCtx, "zero_viewport_height", "Zero viewport height (VK_KHR_maintenance1)", populateTestGroup, subGroupParams);
}

tcu::TestCaseGroup* createOffScreenViewportTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	const struct
	{
		const OffScreenAxisCase	axisCase;
		const char*				suffix;
	} axisCases[] =
	{
		{ OffScreenAxisCase::ONSCREEN,		"_on_screen"			},
		{ OffScreenAxisCase::NEGATIVE_SIDE,	"_off_screen_negative"	},
		{ OffScreenAxisCase::POSITIVE_SIDE,	"_off_screen_positive"	},
	};

	const struct
	{
		const bool			negativeHeight;
		const char*			suffix;
	} negativeHeightCases[] =
	{
		{ false,		""					},
		{ true,			"_negative_height"	},
	};

	uint32_t seed	= 1674229780;
	GroupPtr group	(new tcu::TestCaseGroup(testCtx, "offscreen_viewport", "Test using off-screen viewports"));

	for (const auto& xCase : axisCases)
		for (const auto& yCase : axisCases)
		{
			// At least one of the axis has to be offscreen for the framebuffer to remain clear.
			if (xCase.axisCase == OffScreenAxisCase::ONSCREEN && yCase.axisCase == OffScreenAxisCase::ONSCREEN)
				continue;

			for (const auto& negHeightCase : negativeHeightCases)
			{
				OffScreenParams params(seed, xCase.axisCase, yCase.axisCase, negHeightCase.negativeHeight, groupParams);
				++seed;

				const auto testName = std::string("x") + xCase.suffix + "_y" + yCase.suffix + negHeightCase.suffix;
				group->addChild(new OffScreenViewportCase(testCtx, testName, "", params));
			}
		}
	return group.release();
}

}	// Draw
}	// vkt
