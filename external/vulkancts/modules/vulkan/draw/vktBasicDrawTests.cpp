/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
 * \brief Simple Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktBasicDrawTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "deDefs.h"
#include "deRandom.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuRGBA.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"

#include "rrRenderer.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace Draw
{
namespace
{
static const deUint32 SEED			= 0xc2a39fu;
static const deUint32 INDEX_LIMIT	= 10000;
// To avoid too big and mostly empty structures
static const deUint32 OFFSET_LIMIT	= 1000;
// Number of primitives to draw
static const deUint32 PRIMITIVE_COUNT[] = {1, 3, 17, 45};

enum DrawCommandType
{
	DRAW_COMMAND_TYPE_DRAW,
	DRAW_COMMAND_TYPE_DRAW_INDEXED,
	DRAW_COMMAND_TYPE_DRAW_INDIRECT,
	DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT,

	DRAW_COMMAND_TYPE_DRAW_LAST
};

const char* getDrawCommandTypeName (DrawCommandType command)
{
	switch (command)
	{
		case DRAW_COMMAND_TYPE_DRAW:					return "draw";
		case DRAW_COMMAND_TYPE_DRAW_INDEXED:			return "draw_indexed";
		case DRAW_COMMAND_TYPE_DRAW_INDIRECT:			return "draw_indirect";
		case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT:	return "draw_indexed_indirect";
		default:					DE_ASSERT(false);
	}
	return "";
}

rr::PrimitiveType mapVkPrimitiveTopology (vk::VkPrimitiveTopology primitiveTopology)
{
	switch (primitiveTopology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:						return rr::PRIMITIVETYPE_POINTS;
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:						return rr::PRIMITIVETYPE_LINES;
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:						return rr::PRIMITIVETYPE_LINE_STRIP;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:					return rr::PRIMITIVETYPE_TRIANGLES;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:					return rr::PRIMITIVETYPE_TRIANGLE_FAN;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:					return rr::PRIMITIVETYPE_TRIANGLE_STRIP;
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:		return rr::PRIMITIVETYPE_LINES_ADJACENCY;
		case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:		return rr::PRIMITIVETYPE_LINE_STRIP_ADJACENCY;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:	return rr::PRIMITIVETYPE_TRIANGLES_ADJACENCY;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:	return rr::PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY;
		default:
			DE_ASSERT(false);
	}
	return rr::PRIMITIVETYPE_LAST;
}

struct DrawParamsBase
{
	std::vector<PositionColorVertex>	vertices;
	vk::VkPrimitiveTopology				topology;
	bool								useMaintenance5;
	GroupParams							groupParams;	// we can't use SharedGroupParams here

	DrawParamsBase ()
	{}

	DrawParamsBase (const vk::VkPrimitiveTopology top, const SharedGroupParams gParams)
		: topology			(top)
		, useMaintenance5	(false)
		, groupParams
		{
			gParams->useDynamicRendering,
			gParams->useSecondaryCmdBuffer,
			gParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass,
			gParams->nestedSecondaryCmdBuffer
		}
	{}

	virtual void checkSupport (Context&) const {}
};

struct IndexedParamsBase
{
	std::vector<deUint32>	indexes;
	const vk::VkIndexType	indexType;

	IndexedParamsBase (const vk::VkIndexType indexT)
		: indexType	(indexT)
	{}
};

// Structs to store draw parameters
struct DrawParams : DrawParamsBase
{
	// vkCmdDraw parameters is like a single VkDrawIndirectCommand
	vk::VkDrawIndirectCommand	params;

	DrawParams (const vk::VkPrimitiveTopology top, const SharedGroupParams gParams, const deUint32 vertexC, const deUint32 instanceC, const deUint32 firstV, const deUint32 firstI)
		: DrawParamsBase	(top, gParams)
	{
		params.vertexCount		= vertexC;
		params.instanceCount	= instanceC;
		params.firstVertex		= firstV;
		params.firstInstance	= firstI;
	}
};

struct DrawIndexedParams : DrawParamsBase, IndexedParamsBase
{
	// vkCmdDrawIndexed parameters is like a single VkDrawIndexedIndirectCommand
	vk::VkDrawIndexedIndirectCommand	params;

	DrawIndexedParams (const vk::VkPrimitiveTopology top, const SharedGroupParams gParams, const vk::VkIndexType indexT, const deUint32 indexC, const deUint32 instanceC, const deUint32 firstIdx, const deInt32 vertexO, const deUint32 firstIns)
		: DrawParamsBase	(top, gParams)
		, IndexedParamsBase	(indexT)
	{
		params.indexCount		= indexC;
		params.instanceCount	= instanceC;
		params.firstIndex		= firstIdx;
		params.vertexOffset		= vertexO;
		params.firstInstance	= firstIns;
	}
};

struct DrawIndirectParams : DrawParamsBase
{
	std::vector<vk::VkDrawIndirectCommand>	commands;
	bool									multiDraw;

	DrawIndirectParams (const vk::VkPrimitiveTopology top, const SharedGroupParams gParams, bool multiDraw_)
		: DrawParamsBase	(top, gParams)
		, commands			()
		, multiDraw			(multiDraw_)
	{}

	void addCommand (const deUint32 vertexC, const deUint32 instanceC, const deUint32 firstV, const deUint32 firstI)
	{
		vk::VkDrawIndirectCommand	cmd;
		cmd.vertexCount				= vertexC;
		cmd.instanceCount			= instanceC;
		cmd.firstVertex				= firstV;
		cmd.firstInstance			= firstI;

		commands.push_back(cmd);
	}

	void checkSupport (Context& context) const override
	{
		if (multiDraw && commands.size() > 1)
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT);
	}
};

struct DrawIndexedIndirectParams : DrawParamsBase, IndexedParamsBase
{
	std::vector<vk::VkDrawIndexedIndirectCommand>	commands;
	bool											multiDraw;

	DrawIndexedIndirectParams (const vk::VkPrimitiveTopology top, const SharedGroupParams gParams, const vk::VkIndexType indexT, bool multiDraw_)
		: DrawParamsBase	(top, gParams)
		, IndexedParamsBase	(indexT)
		, commands			()
		, multiDraw			(multiDraw_)
	{}

	void addCommand (const deUint32 indexC, const deUint32 instanceC, const deUint32 firstIdx, const deInt32 vertexO, const deUint32 firstIns)
	{
		vk::VkDrawIndexedIndirectCommand	cmd;
		cmd.indexCount						= indexC;
		cmd.instanceCount					= instanceC;
		cmd.firstIndex						= firstIdx;
		cmd.vertexOffset					= vertexO;
		cmd.firstInstance					= firstIns;

		commands.push_back(cmd);
	}

	void checkSupport (Context& context) const override
	{
		if (multiDraw && commands.size() > 1)
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT);
	}
};

// Reference renderer shaders
class PassthruVertShader : public rr::VertexShader
{
public:
	PassthruVertShader (void)
	: rr::VertexShader (2, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_inputs[1].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeVertices (const rr::VertexAttrib* inputs, rr::VertexPacket* const* packets, const int numPackets) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			packets[packetNdx]->position = rr::readVertexAttribFloat(inputs[0],
																	 packets[packetNdx]->instanceNdx,
																	 packets[packetNdx]->vertexNdx);

			tcu::Vec4 color = rr::readVertexAttribFloat(inputs[1],
														packets[packetNdx]->instanceNdx,
														packets[packetNdx]->vertexNdx);

			packets[packetNdx]->outputs[0] = color;
		}
	}
};

class PassthruFragShader : public rr::FragmentShader
{
public:
	PassthruFragShader (void)
		: rr::FragmentShader(1, 1)
	{
		m_inputs[0].type	= rr::GENERICVECTYPE_FLOAT;
		m_outputs[0].type	= rr::GENERICVECTYPE_FLOAT;
	}

	void shadeFragments (rr::FragmentPacket* packets, const int numPackets, const rr::FragmentShadingContext& context) const
	{
		for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
		{
			rr::FragmentPacket& packet = packets[packetNdx];
			for (deUint32 fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
			{
				tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
				rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
			}
		}
	}
};

inline bool imageCompare (tcu::TestLog& log, const tcu::ConstPixelBufferAccess& reference, const tcu::ConstPixelBufferAccess& result, const vk::VkPrimitiveTopology topology)
{
	if (topology == vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
	{
		return tcu::intThresholdPositionDeviationCompare(
			log, "Result", "Image comparison result", reference, result,
			tcu::UVec4(4u),					// color threshold
			tcu::IVec3(1, 1, 0),			// position deviation tolerance
			true,							// don't check the pixels at the boundary
			tcu::COMPARE_LOG_RESULT);
	}
	else
		return tcu::fuzzyCompare(log, "Result", "Image comparison result", reference, result, 0.053f, tcu::COMPARE_LOG_RESULT);
}

class DrawTestInstanceBase : public TestInstance
{
public:
									DrawTestInstanceBase	(Context& context);
	virtual							~DrawTestInstanceBase	(void) = 0;
	void							initialize				(const DrawParamsBase& data);
	void							initPipeline			(const vk::VkDevice device);
	void							preRenderBarriers		(void);
	void							beginRenderPass			(vk::VkCommandBuffer cmdBuffer);
	void							endRenderPass			(vk::VkCommandBuffer cmdBuffer);

#ifndef CTS_USES_VULKANSC
	void							beginSecondaryCmdBuffer	(const vk::DeviceInterface& vk, vk::VkRenderingFlagsKHR renderingFlags = 0u);
	void							beginNestedCmdBuffer	(const vk::DeviceInterface& vk, vk::VkRenderingFlagsKHR renderingFlags = 0u);
	void							beginDynamicRender		(vk::VkCommandBuffer cmdBuffer, vk::VkRenderingFlagsKHR renderingFlags = 0u);
	void							beginNestedDynamicRender(vk::VkCommandBuffer cmdBuffer, bool nested, vk::VkRenderingFlagsKHR renderingFlags = 0u);
	void							endDynamicRender		(vk::VkCommandBuffer cmdBuffer);
#endif // CTS_USES_VULKANSC

	// Specialize this function for each type
	virtual tcu::TestStatus			iterate					(void) = 0;
protected:
	// Specialize this function for each type
	virtual void					generateDrawData		(void) = 0;
	void							generateRefImage		(const tcu::PixelBufferAccess& access, const std::vector<tcu::Vec4>& vertices, const std::vector<tcu::Vec4>& colors) const;

	DrawParamsBase											m_data;
	const vk::DeviceInterface&								m_vk;
	vk::Move<vk::VkPipeline>								m_pipeline;
	vk::Move<vk::VkPipelineLayout>							m_pipelineLayout;
	vk::VkFormat											m_colorAttachmentFormat;
	de::SharedPtr<Image>									m_colorTargetImage;
	vk::Move<vk::VkImageView>								m_colorTargetView;
	vk::Move<vk::VkRenderPass>								m_renderPass;
	vk::Move<vk::VkFramebuffer>								m_framebuffer;
	PipelineCreateInfo::VertexInputState					m_vertexInputState;
	de::SharedPtr<Buffer>									m_vertexBuffer;
	vk::Move<vk::VkCommandPool>								m_cmdPool;
	vk::Move<vk::VkCommandBuffer>							m_cmdBuffer;
	vk::Move<vk::VkCommandBuffer>							m_secCmdBuffer;
	vk::Move<vk::VkCommandBuffer>							m_nestedCmdBuffer;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

DrawTestInstanceBase::DrawTestInstanceBase (Context& context)
	: vkt::TestInstance			(context)
	, m_vk						(context.getDeviceInterface())
	, m_colorAttachmentFormat	(vk::VK_FORMAT_R8G8B8A8_UNORM)
{
}

DrawTestInstanceBase::~DrawTestInstanceBase (void)
{
}

void DrawTestInstanceBase::initialize (const DrawParamsBase& data)
{
	m_data = data;

	const vk::VkDevice	device				= m_context.getDevice();
	const deUint32		queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	m_pipelineLayout						= vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

	const vk::VkExtent3D targetImageExtent	= { WIDTH, HEIGHT, 1 };
	const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT,
		vk::VK_IMAGE_TILING_OPTIMAL, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_colorTargetImage						= Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), m_context.getUniversalQueueFamilyIndex());

	const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
	m_colorTargetView						= vk::createImageView(m_vk, device, &colorTargetViewInfo);

	// create render pass only when we are not using dynamic rendering
	if (!m_data.groupParams.useDynamicRendering)
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

		m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

		// create framebuffer
		std::vector<vk::VkImageView>	colorAttachments		{ *m_colorTargetView };
		const FramebufferCreateInfo		framebufferCreateInfo	(*m_renderPass, colorAttachments, WIDTH, HEIGHT, 1);
		m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
	}

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

	m_vertexInputState = PipelineCreateInfo::VertexInputState(1,
															  &vertexInputBindingDescription,
															  2,
															  vertexInputAttributeDescriptions);

	const vk::VkDeviceSize dataSize = m_data.vertices.size() * sizeof(PositionColorVertex);
	BufferCreateInfo createInfo(dataSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

#ifndef CTS_USES_VULKANSC
	vk::VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();
	if (m_data.useMaintenance5)
	{
		bufferUsageFlags2.usage = vk::VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT_KHR;
		createInfo.pNext = &bufferUsageFlags2;
		createInfo.usage = 0xBAD00000;
	}
#endif // CTS_USES_VULKANSC

	m_vertexBuffer = Buffer::createAndAlloc(m_vk, device, createInfo,
		m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

	deUint8* ptr = reinterpret_cast<deUint8*>(m_vertexBuffer->getBoundMemory().getHostPtr());
	deMemcpy(ptr, &(m_data.vertices[0]), static_cast<size_t>(dataSize));

	vk::flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());

	const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
	m_cmdPool	= vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
	m_cmdBuffer	= vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	if (m_data.groupParams.useSecondaryCmdBuffer)
		m_secCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	if (m_data.groupParams.nestedSecondaryCmdBuffer)
		m_nestedCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	initPipeline(device);
}

void DrawTestInstanceBase::initPipeline (const vk::VkDevice device)
{
	const vk::Unique<vk::VkShaderModule>	vs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const vk::Unique<vk::VkShaderModule>	fs(createShaderModule(m_vk, device, m_context.getBinaryCollection().get("frag"), 0));

	const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

	vk::VkViewport viewport	= vk::makeViewport(WIDTH, HEIGHT);
	vk::VkRect2D scissor	= vk::makeRect2D(WIDTH, HEIGHT);

	// when dynamic_rendering is tested then renderPass won't be created and VK_NULL_HANDLE will be used here
	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
	pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_data.topology));
	pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
	pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport), std::vector<vk::VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
	pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
	pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

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

	if (m_data.groupParams.useDynamicRendering)
		pipelineCreateInfo.pNext = &renderingCreateInfo;

	vk::VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
		pipelineCreateInfo.pNext,
		vk::VK_PIPELINE_CREATE_2_ALLOW_DERIVATIVES_BIT_KHR
	};
	if (m_data.useMaintenance5)
	{
		// Previously these flags were set to a bad value like 0xBAD00000, but
		// using VK_PIPELINE_CREATE_LIBRARY_BIT_KHR is much more interesting due
		// to the implications of using this flag by mistake. In particular,
		// several Mesa drivers crashed at some point due to this.
		pipelineCreateInfo.flags = vk::VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		pipelineCreateInfo.pNext = &pipelineFlags2CreateInfo;
	}
#endif // CTS_USES_VULKANSC

	m_pipeline = vk::createGraphicsPipeline(m_vk, device, DE_NULL, &pipelineCreateInfo);
}

void DrawTestInstanceBase::preRenderBarriers (void)
{
	const vk::VkClearValue clearColor { { { 0.0f, 0.0f, 0.0f, 1.0f } } };

	initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
								  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

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

void DrawTestInstanceBase::beginRenderPass (vk::VkCommandBuffer cmdBuffer)
{
	const vk::VkClearValue	clearColor	{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	const vk::VkRect2D		renderArea	= vk::makeRect2D(WIDTH, HEIGHT);

	vk::beginRenderPass(m_vk, cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, 1u, &clearColor);
}

void DrawTestInstanceBase::endRenderPass (vk::VkCommandBuffer cmdBuffer)
{
	vk::endRenderPass(m_vk, cmdBuffer);
}

#ifndef CTS_USES_VULKANSC
void DrawTestInstanceBase::beginSecondaryCmdBuffer(const vk::DeviceInterface& vk, vk::VkRenderingFlagsKHR renderingFlags)
{
	const vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&m_colorAttachmentFormat,												// const VkFormat*					pColorAttachmentFormats;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							depthAttachmentFormat;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							stencilAttachmentFormat;
		vk::VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			rasterizationSamples;
	};

	const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,					// VkStructureType					sType;
		&inheritanceRenderingInfo,												// const void*						pNext;
		DE_NULL,																// VkRenderPass						renderPass;
		0u,																		// deUint32							subpass;
		DE_NULL,																// VkFramebuffer					framebuffer;
		VK_FALSE,																// VkBool32							occlusionQueryEnable;
		(vk::VkQueryControlFlags)0u,											// VkQueryControlFlags				queryFlags;
		(vk::VkQueryPipelineStatisticFlags)0u									// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
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

void DrawTestInstanceBase::beginNestedCmdBuffer(const vk::DeviceInterface& vk, vk::VkRenderingFlagsKHR renderingFlags)
{
	const vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		1u,																		// uint32_t							colorAttachmentCount;
		&m_colorAttachmentFormat,												// const VkFormat*					pColorAttachmentFormats;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							depthAttachmentFormat;
		vk::VK_FORMAT_UNDEFINED,												// VkFormat							stencilAttachmentFormat;
		vk::VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			rasterizationSamples;
	};

	const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,					// VkStructureType					sType;
		&inheritanceRenderingInfo,												// const void*						pNext;
		DE_NULL,																// VkRenderPass						renderPass;
		0u,																		// deUint32							subpass;
		DE_NULL,																// VkFramebuffer					framebuffer;
		VK_FALSE,																// VkBool32							occlusionQueryEnable;
		(vk::VkQueryControlFlags)0u,											// VkQueryControlFlags				queryFlags;
		(vk::VkQueryPipelineStatisticFlags)0u									// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const vk::VkCommandBufferBeginInfo commandBufBeginParams
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,						// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	VK_CHECK(vk.beginCommandBuffer(*m_nestedCmdBuffer, &commandBufBeginParams));
}

void DrawTestInstanceBase::beginDynamicRender(vk::VkCommandBuffer cmdBuffer, vk::VkRenderingFlagsKHR renderingFlags)
{
	const vk::VkClearValue	clearColor{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	const vk::VkRect2D		renderArea = vk::makeRect2D(WIDTH, HEIGHT);

	vk::beginRendering(m_vk, cmdBuffer, *m_colorTargetView, renderArea, clearColor, vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, renderingFlags);
}

void DrawTestInstanceBase::endDynamicRender(vk::VkCommandBuffer cmdBuffer)
{
	vk::endRendering(m_vk, cmdBuffer);
}
#endif // CTS_USES_VULKANSC

void DrawTestInstanceBase::generateRefImage (const tcu::PixelBufferAccess& access, const std::vector<tcu::Vec4>& vertices, const std::vector<tcu::Vec4>& colors) const
{
	const PassthruVertShader				vertShader;
	const PassthruFragShader				fragShader;
	const rr::Program						program			(&vertShader, &fragShader);
	const rr::MultisamplePixelBufferAccess	colorBuffer		= rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(access);
	const rr::RenderTarget					renderTarget	(colorBuffer);
	const rr::RenderState					renderState		((rr::ViewportState(colorBuffer)), m_context.getDeviceProperties().limits.subPixelPrecisionBits);
	const rr::Renderer						renderer;

	const rr::VertexAttrib	vertexAttribs[] =
	{
		rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vertices[0]),
		rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &colors[0])
	};

	renderer.draw(rr::DrawCommand(renderState,
								  renderTarget,
								  program,
								  DE_LENGTH_OF_ARRAY(vertexAttribs),
								  &vertexAttribs[0],
								  rr::PrimitiveList(mapVkPrimitiveTopology(m_data.topology), (deUint32)vertices.size(), 0)));
}

template<typename T>
class DrawTestInstance : public DrawTestInstanceBase
{
public:
							DrawTestInstance		(Context& context, const T& data);
	virtual					~DrawTestInstance		(void);
	virtual void			generateDrawData		(void);
	virtual void			draw					(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer indirectBuffer = DE_NULL, vk::VkDeviceSize indirectOffset = 0ul);
	virtual tcu::TestStatus	iterate					(void);
private:
	T						m_data;
};

template<typename T>
DrawTestInstance<T>::DrawTestInstance (Context& context, const T& data)
	: DrawTestInstanceBase	(context)
	, m_data(data)
{
	generateDrawData();
	initialize(m_data);
}

template<typename T>
DrawTestInstance<T>::~DrawTestInstance (void)
{
}

template<typename T>
void DrawTestInstance<T>::generateDrawData (void)
{
	DE_FATAL("Using the general case of this function is forbidden!");
}

template<typename T>
void DrawTestInstance<T>::draw(vk::VkCommandBuffer, vk::VkBuffer, vk::VkDeviceSize)
{
	DE_FATAL("Using the general case of this function is forbidden!");
}

template<typename T>
tcu::TestStatus DrawTestInstance<T>::iterate (void)
{
	DE_FATAL("Using the general case of this function is forbidden!");
	return tcu::TestStatus::fail("");
}

template<typename T>
class DrawTestCase : public TestCase
{
	public:
									DrawTestCase		(tcu::TestContext& context, const char* name, const T data);
									~DrawTestCase		(void);
	virtual	void					initPrograms		(vk::SourceCollections& programCollection) const;
	virtual void					initShaderSources	(void);
	virtual void					checkSupport		(Context& context) const;
	virtual TestInstance*			createInstance		(Context& context) const;

private:
	T													m_data;
	std::string											m_vertShaderSource;
	std::string											m_fragShaderSource;
};

template<typename T>
DrawTestCase<T>::DrawTestCase (tcu::TestContext& context, const char* name, const T data)
	: vkt::TestCase	(context, name)
	, m_data		(data)
{
	initShaderSources();
}

template<typename T>
DrawTestCase<T>::~DrawTestCase	(void)
{
}

template<typename T>
void DrawTestCase<T>::initPrograms (vk::SourceCollections& programCollection) const
{
	programCollection.glslSources.add("vert") << glu::VertexSource(m_vertShaderSource);
	programCollection.glslSources.add("frag") << glu::FragmentSource(m_fragShaderSource);
}

template<typename T>
void DrawTestCase<T>::checkSupport (Context& context) const
{
	if (m_data.topology == vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY ||
		m_data.topology == vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ||
		m_data.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY ||
		m_data.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
	}

#ifndef CTS_USES_VULKANSC
	if (m_data.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");

	if (m_data.groupParams.useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

	if (m_data.groupParams.nestedSecondaryCmdBuffer) {
		context.requireDeviceFunctionality("VK_EXT_nested_command_buffer");
		const auto& features = *vk::findStructure<vk::VkPhysicalDeviceNestedCommandBufferFeaturesEXT>(&context.getDeviceFeatures2());
		if (!features.nestedCommandBuffer)
			TCU_THROW(NotSupportedError, "nestedCommandBuffer is not supported");
		if (!features.nestedCommandBufferRendering)
			TCU_THROW(NotSupportedError, "nestedCommandBufferRendering is not supported, so VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT cannot be used");
	}

	if (m_data.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
		!context.getPortabilitySubsetFeatures().triangleFans)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
	}
#endif // CTS_USES_VULKANSC

	// Parameter-specific support checks.
	m_data.checkSupport(context);
}

template<typename T>
void DrawTestCase<T>::initShaderSources (void)
{
	std::stringstream vertShader;
	vertShader	<< "#version 430\n"
				<< "layout(location = 0) in vec4 in_position;\n"
				<< "layout(location = 1) in vec4 in_color;\n"
				<< "layout(location = 0) out vec4 out_color;\n"

				<< "out gl_PerVertex {\n"
				<< "    vec4  gl_Position;\n"
				<< "    float gl_PointSize;\n"
				<< "};\n"
				<< "void main() {\n"
				<< "    gl_PointSize = 1.0;\n"
				<< "    gl_Position  = in_position;\n"
				<< "    out_color    = in_color;\n"
				<< "}\n";

	m_vertShaderSource = vertShader.str();

	std::stringstream fragShader;
	fragShader	<< "#version 430\n"
				<< "layout(location = 0) in vec4 in_color;\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "void main()\n"
				<< "{\n"
				<< "    out_color = in_color;\n"
				<< "}\n";

	m_fragShaderSource = fragShader.str();
}

template<typename T>
TestInstance* DrawTestCase<T>::createInstance (Context& context) const
{
	return new DrawTestInstance<T>(context, m_data);
}

// Specialized cases
template<>
void DrawTestInstance<DrawParams>::generateDrawData (void)
{
	de::Random		rnd			(SEED ^ m_data.params.firstVertex ^ m_data.params.vertexCount);

	const deUint32	vectorSize	= m_data.params.firstVertex + m_data.params.vertexCount;

	// Initialize the vector
	m_data.vertices = std::vector<PositionColorVertex>(vectorSize, PositionColorVertex(tcu::Vec4(0.0, 0.0, 0.0, 0.0), tcu::Vec4(0.0, 0.0, 0.0, 0.0)));

	// Fill only the used indexes
	for (deUint32 vertexIdx = m_data.params.firstVertex; vertexIdx < vectorSize; ++vertexIdx)
	{
		const float f0 = rnd.getFloat(-1.0f, 1.0f);
		const float f1 = rnd.getFloat(-1.0f, 1.0f);

		m_data.vertices[vertexIdx] = PositionColorVertex(
			tcu::Vec4(f0, f1, 1.0f, 1.0f),	// Coord
			tcu::randomVec4(rnd));			// Color
	}
}

template<>
void DrawTestInstance<DrawParams>::draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer, vk::VkDeviceSize)
{
	m_vk.cmdDraw(cmdBuffer, m_data.params.vertexCount, m_data.params.instanceCount, m_data.params.firstVertex, m_data.params.firstInstance);
}

template<>
tcu::TestStatus DrawTestInstance<DrawParams>::iterate (void)
{
	tcu::TestLog			&log				= m_context.getTestContext().getLog();
	const vk::VkQueue		queue				= m_context.getUniversalQueue();
	const vk::VkDevice		device				= m_context.getDevice();
	const vk::VkDeviceSize	vertexBufferOffset	= 0;
	const vk::VkBuffer		vertexBuffer		= m_vertexBuffer->object();

#ifndef CTS_USES_VULKANSC
	if (m_data.groupParams.useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_secCmdBuffer);

		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		if (m_data.groupParams.nestedSecondaryCmdBuffer) {
			// record buffer to nest secondary buffer in
			beginNestedCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT | vk::VK_RENDERING_CONTENTS_INLINE_BIT_EXT);
			m_vk.cmdExecuteCommands(*m_nestedCmdBuffer, 1u, &*m_secCmdBuffer);
			endCommandBuffer(m_vk, *m_nestedCmdBuffer);
		}

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		preRenderBarriers();

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

		if (m_data.groupParams.nestedSecondaryCmdBuffer) {
			m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_nestedCmdBuffer);
		} else {
			m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);
		}

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if(m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer);

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginRenderPass(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer);

		endRenderPass(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	tcu::TextureLevel refImage (vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
	tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	std::vector<tcu::Vec4>	vertices;
	std::vector<tcu::Vec4>	colors;

	for (std::vector<PositionColorVertex>::const_iterator vertex = m_data.vertices.begin() + m_data.params.firstVertex; vertex != m_data.vertices.end(); ++vertex)
	{
		vertices.push_back(vertex->position);
		colors.push_back(vertex->color);
	}
	generateRefImage(refImage.getAccess(), vertices, colors);

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!imageCompare(log, refImage.getAccess(), renderedFrame, m_data.topology))
		res = QP_TEST_RESULT_FAIL;

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

template<>
void DrawTestInstance<DrawIndexedParams>::generateDrawData (void)
{
	de::Random		rnd			(SEED ^ m_data.params.firstIndex ^ m_data.params.indexCount);
	const deUint32	indexSize	= m_data.params.firstIndex + m_data.params.indexCount;

	// Initialize the vector with zeros
	m_data.indexes = std::vector<deUint32>(indexSize, 0);

	deUint32		highestIndex	= 0;	// Store to highest index to calculate the vertices size
	// Fill the indexes from firstIndex
	for (deUint32 idx = 0; idx < m_data.params.indexCount; ++idx)
	{
		deUint32	vertexIdx	= rnd.getInt(m_data.params.vertexOffset, INDEX_LIMIT);
		highestIndex = (vertexIdx > highestIndex) ? vertexIdx : highestIndex;

		m_data.indexes[m_data.params.firstIndex + idx]	= vertexIdx;
	}

	// Fill up the vertex coordinates with zeros until the highestIndex including the vertexOffset
	m_data.vertices = std::vector<PositionColorVertex>(m_data.params.vertexOffset + highestIndex + 1, PositionColorVertex(tcu::Vec4(0.0, 0.0, 0.0, 0.0), tcu::Vec4(0.0, 0.0, 0.0, 0.0)));

	// Generate random vertex only where you have index pointing at
	for (std::vector<deUint32>::const_iterator indexIt = m_data.indexes.begin() + m_data.params.firstIndex; indexIt != m_data.indexes.end(); ++indexIt)
	{
		// Get iterator to the vertex position  with the vertexOffset
		std::vector<PositionColorVertex>::iterator vertexIt = m_data.vertices.begin() + m_data.params.vertexOffset + *indexIt;

		tcu::VecAccess<float, 4, 4>	positionAccess = vertexIt->position.xyzw();
		const float f0 = rnd.getFloat(-1.0f, 1.0f);
		const float f1 = rnd.getFloat(-1.0f, 1.0f);
		positionAccess = tcu::Vec4(f0, f1, 1.0f, 1.0f);

		tcu::VecAccess<float, 4, 4>	colorAccess = vertexIt->color.xyzw();
		colorAccess = tcu::randomVec4(rnd);
	}
}

template<>
void DrawTestInstance<DrawIndexedParams>::draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer, vk::VkDeviceSize)
{
	m_vk.cmdDrawIndexed(cmdBuffer, m_data.params.indexCount, m_data.params.instanceCount, m_data.params.firstIndex, m_data.params.vertexOffset, m_data.params.firstInstance);
}

template<>
tcu::TestStatus DrawTestInstance<DrawIndexedParams>::iterate (void)
{
	tcu::TestLog				&log				= m_context.getTestContext().getLog();
	const vk::DeviceInterface&	vk					= m_context.getDeviceInterface();
	const vk::VkDevice			vkDevice			= m_context.getDevice();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue			queue				= m_context.getUniversalQueue();
	vk::Allocator&				allocator			= m_context.getDefaultAllocator();
	const vk::VkDeviceSize		vertexBufferOffset	= 0;
	const vk::VkBuffer			vertexBuffer		= m_vertexBuffer->object();
	const deUint32				bufferSize			= (deUint32)(m_data.indexes.size() * sizeof(deUint32));

	const vk::VkBufferCreateInfo	bufferCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		bufferSize,									// VkDeviceSize			size;
		vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT,		// VkBufferUsageFlags	usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
	};

	vk::Move<vk::VkBuffer>			indexBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);
	de::MovePtr<vk::Allocation>		indexAlloc;

	indexAlloc = allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *indexBuffer), vk::MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *indexBuffer, indexAlloc->getMemory(), indexAlloc->getOffset()));

	deMemcpy(indexAlloc->getHostPtr(), &(m_data.indexes[0]), bufferSize);

	vk::flushAlloc(m_vk, vkDevice, *indexAlloc);

#ifndef CTS_USES_VULKANSC
	if (m_data.groupParams.useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_secCmdBuffer, *indexBuffer, 0u, m_data.indexType);
		draw(*m_secCmdBuffer);

		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		preRenderBarriers();

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, *indexBuffer, 0u, m_data.indexType);
		draw(*m_cmdBuffer);

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginRenderPass(*m_cmdBuffer);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, *indexBuffer, 0u, m_data.indexType);
		draw(*m_cmdBuffer);

		endRenderPass(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, vkDevice, queue, m_cmdBuffer.get());

	// Validation
	tcu::TextureLevel	refImage	(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
	tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	std::vector<tcu::Vec4>	vertices;
	std::vector<tcu::Vec4>	colors;

	for (std::vector<deUint32>::const_iterator it = m_data.indexes.begin() + m_data.params.firstIndex; it != m_data.indexes.end(); ++it)
	{
		deUint32 idx = m_data.params.vertexOffset + *it;
		vertices.push_back(m_data.vertices[idx].position);
		colors.push_back(m_data.vertices[idx].color);
	}
	generateRefImage(refImage.getAccess(), vertices, colors);

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!imageCompare(log, refImage.getAccess(), renderedFrame, m_data.topology))
		res = QP_TEST_RESULT_FAIL;

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

template<>
void DrawTestInstance<DrawIndirectParams>::generateDrawData (void)
{
	de::Random	rnd(SEED ^ m_data.commands[0].vertexCount ^ m_data.commands[0].firstVertex);

	deUint32 lastIndex	= 0;

	// Find the interval which will be used
	for (std::vector<vk::VkDrawIndirectCommand>::const_iterator it = m_data.commands.begin(); it != m_data.commands.end(); ++it)
	{
		const deUint32	index = it->firstVertex + it->vertexCount;
		lastIndex	= (index > lastIndex) ? index : lastIndex;
	}

	// Initialize with zeros
	m_data.vertices = std::vector<PositionColorVertex>(lastIndex, PositionColorVertex(tcu::Vec4(0.0, 0.0, 0.0, 0.0), tcu::Vec4(0.0, 0.0, 0.0, 0.0)));

	// Generate random vertices only where necessary
	for (std::vector<vk::VkDrawIndirectCommand>::const_iterator it = m_data.commands.begin(); it != m_data.commands.end(); ++it)
	{
		std::vector<PositionColorVertex>::iterator vertexStart = m_data.vertices.begin() + it->firstVertex;

		for (deUint32 idx = 0; idx < it->vertexCount; ++idx)
		{
			std::vector<PositionColorVertex>::iterator vertexIt = vertexStart + idx;

			tcu::VecAccess<float, 4, 4> positionAccess = vertexIt->position.xyzw();
			const float f0 = rnd.getFloat(-1.0f, 1.0f);
			const float f1 = rnd.getFloat(-1.0f, 1.0f);
			positionAccess = tcu::Vec4(f0, f1, 1.0f, 1.0f);

			tcu::VecAccess<float, 4, 4> colorAccess = vertexIt->color.xyzw();
			colorAccess = tcu::randomVec4(rnd);
		}
	}
}

template<>
void DrawTestInstance<DrawIndirectParams>::draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer indirectBuffer, vk::VkDeviceSize indirectOffset)
{
	if (m_data.multiDraw)
	{
		m_vk.cmdDrawIndirect(cmdBuffer, indirectBuffer, indirectOffset, (deUint32)m_data.commands.size(), sizeof(vk::VkDrawIndirectCommand));
	}
	else // Use multiple single draws.
	{
		for (deUint32 cmdIdx = 0; cmdIdx < m_data.commands.size(); ++cmdIdx)
		{
			const deUint32	offset = (deUint32)(indirectOffset + cmdIdx * sizeof(vk::VkDrawIndirectCommand));
			m_vk.cmdDrawIndirect(cmdBuffer, indirectBuffer, offset, 1, sizeof(vk::VkDrawIndirectCommand));
		}
	}
}

template<>
tcu::TestStatus DrawTestInstance<DrawIndirectParams>::iterate (void)
{
	tcu::TestLog						&log				= m_context.getTestContext().getLog();
	const vk::DeviceInterface&			vk					= m_context.getDeviceInterface();
	const vk::VkDevice					vkDevice			= m_context.getDevice();
	vk::Allocator&						allocator			= m_context.getDefaultAllocator();
	const vk::VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const vk::VkDeviceSize				vertexBufferOffset	= 0;
	const vk::VkBuffer					vertexBuffer		= m_vertexBuffer->object();
	vk::Move<vk::VkBuffer>				indirectBuffer;
	de::MovePtr<vk::Allocation>			indirectAlloc;

	{
		const vk::VkDeviceSize	indirectInfoSize	= m_data.commands.size() * sizeof(vk::VkDrawIndirectCommand);

		const vk::VkBufferCreateInfo	indirectCreateInfo =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			indirectInfoSize,							// VkDeviceSize			size;
			vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,	// VkBufferUsageFlags	usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

		indirectBuffer	= createBuffer(vk, vkDevice, &indirectCreateInfo);
		indirectAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *indirectBuffer), vk::MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *indirectBuffer, indirectAlloc->getMemory(), indirectAlloc->getOffset()));

		deMemcpy(indirectAlloc->getHostPtr(), &(m_data.commands[0]), (size_t)indirectInfoSize);

		vk::flushAlloc(m_vk, vkDevice, *indirectAlloc);
	}

#ifndef CTS_USES_VULKANSC
	if (m_data.groupParams.useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_secCmdBuffer, *indirectBuffer, indirectAlloc->getOffset());

		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		preRenderBarriers();

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer, *indirectBuffer, indirectAlloc->getOffset());

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginRenderPass(*m_cmdBuffer);

		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		draw(*m_cmdBuffer, *indirectBuffer, indirectAlloc->getOffset());

		endRenderPass(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, vkDevice, queue, m_cmdBuffer.get());

	// Validation
	tcu::TextureLevel refImage (vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
	tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	for (std::vector<vk::VkDrawIndirectCommand>::const_iterator it = m_data.commands.begin(); it != m_data.commands.end(); ++it)
	{
		std::vector<tcu::Vec4>	vertices;
		std::vector<tcu::Vec4>	colors;

		std::vector<PositionColorVertex>::const_iterator	firstIt	= m_data.vertices.begin() + it->firstVertex;
		std::vector<PositionColorVertex>::const_iterator	lastIt	= firstIt + it->vertexCount;

		for (std::vector<PositionColorVertex>::const_iterator vertex = firstIt; vertex != lastIt; ++vertex)
		{
			vertices.push_back(vertex->position);
			colors.push_back(vertex->color);
		}
		generateRefImage(refImage.getAccess(), vertices, colors);
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!imageCompare(log, refImage.getAccess(), renderedFrame, m_data.topology))
		res = QP_TEST_RESULT_FAIL;

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

template<>
void DrawTestInstance<DrawIndexedIndirectParams>::generateDrawData (void)
{
	de::Random		rnd			(SEED ^ m_data.commands[0].firstIndex ^ m_data.commands[0].indexCount);

	deUint32		lastIndex	= 0;

	// Get the maximum range of indexes
	for (std::vector<vk::VkDrawIndexedIndirectCommand>::const_iterator it = m_data.commands.begin(); it != m_data.commands.end(); ++it)
	{
		const deUint32	index		= it->firstIndex + it->indexCount;
						lastIndex	= (index > lastIndex) ? index : lastIndex;
	}

	// Initialize the vector with zeros
	m_data.indexes = std::vector<deUint32>(lastIndex, 0);

	deUint32	highestIndex	= 0;

	// Generate random indexes for the ranges
	for (std::vector<vk::VkDrawIndexedIndirectCommand>::const_iterator it = m_data.commands.begin(); it != m_data.commands.end(); ++it)
	{
		for (deUint32 idx = 0; idx < it->indexCount; ++idx)
		{
			const deUint32	vertexIdx	= rnd.getInt(it->vertexOffset, INDEX_LIMIT);
			const deUint32	maxIndex	= vertexIdx + it->vertexOffset;

			highestIndex = (maxIndex > highestIndex) ? maxIndex : highestIndex;
			m_data.indexes[it->firstIndex + idx] = vertexIdx;
		}
	}

	// Initialize the vertex vector
	m_data.vertices = std::vector<PositionColorVertex>(highestIndex + 1, PositionColorVertex(tcu::Vec4(0.0, 0.0, 0.0, 0.0), tcu::Vec4(0.0, 0.0, 0.0, 0.0)));

	// Generate random vertices in the used locations
	for (std::vector<vk::VkDrawIndexedIndirectCommand>::const_iterator cmdIt = m_data.commands.begin(); cmdIt != m_data.commands.end(); ++cmdIt)
	{
		deUint32	firstIdx	= cmdIt->firstIndex;
		deUint32	lastIdx		= firstIdx + cmdIt->indexCount;

		for (deUint32 idx = firstIdx; idx < lastIdx; ++idx)
		{
			std::vector<PositionColorVertex>::iterator	vertexIt = m_data.vertices.begin() + cmdIt->vertexOffset + m_data.indexes[idx];

			tcu::VecAccess<float, 4, 4> positionAccess = vertexIt->position.xyzw();
			const float f0 = rnd.getFloat(-1.0f, 1.0f);
			const float f1 = rnd.getFloat(-1.0f, 1.0f);
			positionAccess = tcu::Vec4(f0, f1, 1.0f, 1.0f);

			tcu::VecAccess<float, 4, 4> colorAccess = vertexIt->color.xyzw();
			colorAccess = tcu::randomVec4(rnd);
		}
	}
}

template<>
void DrawTestInstance<DrawIndexedIndirectParams>::draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer indirectBuffer, vk::VkDeviceSize indirectOffset)
{
	if (m_data.multiDraw)
	{
		m_vk.cmdDrawIndexedIndirect(cmdBuffer, indirectBuffer, indirectOffset, (deUint32)m_data.commands.size(), sizeof(vk::VkDrawIndexedIndirectCommand));
	}
	else // Use multiple single draws.
	{
		for (deUint32 cmdIdx = 0; cmdIdx < m_data.commands.size(); ++cmdIdx)
		{
			const deUint32	offset = (deUint32)(indirectOffset + cmdIdx * sizeof(vk::VkDrawIndexedIndirectCommand));
			m_vk.cmdDrawIndexedIndirect(cmdBuffer, indirectBuffer, offset, 1, sizeof(vk::VkDrawIndexedIndirectCommand));
		}
	}
}

template<>
tcu::TestStatus DrawTestInstance<DrawIndexedIndirectParams>::iterate (void)
{
	tcu::TestLog						&log				= m_context.getTestContext().getLog();
	const vk::DeviceInterface&			vk					= m_context.getDeviceInterface();
	const vk::VkDevice					vkDevice			= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue					queue				= m_context.getUniversalQueue();
	vk::Allocator&						allocator			= m_context.getDefaultAllocator();
	const vk::VkDeviceSize				vertexBufferOffset	= 0;
	const vk::VkBuffer					vertexBuffer		= m_vertexBuffer->object();
	vk::Move<vk::VkBuffer>				indirectBuffer;
	de::MovePtr<vk::Allocation>			indirectAlloc;

	{
		const vk::VkDeviceSize	indirectInfoSize	= m_data.commands.size() * sizeof(vk::VkDrawIndexedIndirectCommand);

		vk::VkBufferCreateInfo	indirectCreateInfo
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			indirectInfoSize,							// VkDeviceSize			size;
			vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,	// VkBufferUsageFlags	usage;
			vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
		};

#ifndef CTS_USES_VULKANSC
		vk::VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();
		if (m_data.useMaintenance5)
		{
			bufferUsageFlags2.usage = vk::VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT_KHR;
			indirectCreateInfo.pNext = &bufferUsageFlags2;
			indirectCreateInfo.usage = 0xBAD00000;
		}
#endif // CTS_USES_VULKANSC

		indirectBuffer	= createBuffer(vk, vkDevice, &indirectCreateInfo);
		indirectAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *indirectBuffer), vk::MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *indirectBuffer, indirectAlloc->getMemory(), indirectAlloc->getOffset()));

		deMemcpy(indirectAlloc->getHostPtr(), &(m_data.commands[0]), (size_t)indirectInfoSize);

		vk::flushAlloc(m_vk, vkDevice, *indirectAlloc);
	}

	const deUint32	bufferSize = (deUint32)(m_data.indexes.size() * sizeof(deUint32));

	vk::Move<vk::VkBuffer>			indexBuffer;

	vk::VkBufferCreateInfo	bufferCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		bufferSize,									// VkDeviceSize			size;
		vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT,		// VkBufferUsageFlags	usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex,							// const deUint32*		pQueueFamilyIndices;
	};

#ifndef CTS_USES_VULKANSC
	vk::VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();
	if (m_data.useMaintenance5)
	{
		bufferUsageFlags2.usage = vk::VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT_KHR;
		bufferCreateInfo.pNext = &bufferUsageFlags2;
		bufferCreateInfo.usage = 0xBAD00000;
	}
#endif // CTS_USES_VULKANSC

	indexBuffer = createBuffer(vk, vkDevice, &bufferCreateInfo);

	de::MovePtr<vk::Allocation>	indexAlloc;

	indexAlloc = allocator.allocate(getBufferMemoryRequirements(vk, vkDevice, *indexBuffer), vk::MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *indexBuffer, indexAlloc->getMemory(), indexAlloc->getOffset()));

	deMemcpy(indexAlloc->getHostPtr(), &(m_data.indexes[0]), bufferSize);

	vk::flushAlloc(m_vk, vkDevice, *indexAlloc);

#ifndef CTS_USES_VULKANSC
	if (m_data.groupParams.useSecondaryCmdBuffer)
	{
		// record secondary command buffer
		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(m_vk, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*m_secCmdBuffer);
		}
		else
			beginSecondaryCmdBuffer(m_vk);

		m_vk.cmdBindPipeline(*m_secCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdBindVertexBuffers(*m_secCmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_secCmdBuffer, *indexBuffer, 0u, m_data.indexType);
		draw(*m_secCmdBuffer, *indirectBuffer, indirectAlloc->getOffset());

		if (m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_secCmdBuffer);

		endCommandBuffer(m_vk, *m_secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

		preRenderBarriers();

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*m_cmdBuffer, vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

		if (!m_data.groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endDynamicRender(*m_cmdBuffer);

		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
	else if (m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginDynamicRender(*m_cmdBuffer);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, *indexBuffer, 0u, m_data.indexType);
		draw(*m_cmdBuffer, *indirectBuffer, indirectAlloc->getOffset());

		endDynamicRender(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_data.groupParams.useDynamicRendering)
	{
		beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
		preRenderBarriers();
		beginRenderPass(*m_cmdBuffer);

		m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		m_vk.cmdBindIndexBuffer(*m_cmdBuffer, *indexBuffer, 0u, m_data.indexType);
		draw(*m_cmdBuffer, *indirectBuffer, indirectAlloc->getOffset());

		endRenderPass(*m_cmdBuffer);
		endCommandBuffer(m_vk, *m_cmdBuffer);
	}

	submitCommandsAndWait(m_vk, vkDevice, queue, m_cmdBuffer.get());

	// Validation
	tcu::TextureLevel refImage (vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
	tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	for (std::vector<vk::VkDrawIndexedIndirectCommand>::const_iterator cmd = m_data.commands.begin(); cmd != m_data.commands.end(); ++cmd)
	{
		std::vector<tcu::Vec4>	vertices;
		std::vector<tcu::Vec4>	colors;

		for (deUint32 idx = 0; idx < cmd->indexCount; ++idx)
		{
			const deUint32 vertexIndex = cmd->vertexOffset + m_data.indexes[cmd->firstIndex + idx];
			vertices.push_back(m_data.vertices[vertexIndex].position);
			colors.push_back(m_data.vertices[vertexIndex].color);
		}
		generateRefImage(refImage.getAccess(), vertices, colors);
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!imageCompare(log, refImage.getAccess(), renderedFrame, m_data.topology))
		res = QP_TEST_RESULT_FAIL;

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

typedef DrawTestCase<DrawParams>				DrawCase;
typedef DrawTestCase<DrawIndexedParams>			IndexedCase;
typedef DrawTestCase<DrawIndirectParams>		IndirectCase;
typedef DrawTestCase<DrawIndexedIndirectParams>	IndexedIndirectCase;

struct TestCaseParams
{
	const DrawCommandType			command;
	const vk::VkPrimitiveTopology	topology;
	const SharedGroupParams			groupParams;

	TestCaseParams (const DrawCommandType cmd, const vk::VkPrimitiveTopology top, const SharedGroupParams gParams)
		: command		(cmd)
		, topology		(top)
		, groupParams	(gParams)
	{}
};

}	// anonymous

void populateSubGroup (tcu::TestCaseGroup* testGroup, const TestCaseParams caseParams)
{
	de::Random						rnd						(SEED ^ deStringHash(testGroup->getName()));
	tcu::TestContext&				testCtx					= testGroup->getTestContext();
	const DrawCommandType			command					= caseParams.command;
	const vk::VkPrimitiveTopology	topology				= caseParams.topology;
	const SharedGroupParams			groupParams				= caseParams.groupParams;
	const deUint32					primitiveCountArrLength = DE_LENGTH_OF_ARRAY(PRIMITIVE_COUNT);

	for (deUint32 primitiveCountIdx = 0; primitiveCountIdx < primitiveCountArrLength; ++primitiveCountIdx)
	{
		const deUint32 primitives = PRIMITIVE_COUNT[primitiveCountIdx];

		// when testing VK_KHR_dynamic_rendering there is no need to duplicate tests for all primitive counts; use just 1 and 45
		if (groupParams->useDynamicRendering && (primitiveCountIdx != 0) && (primitiveCountIdx != primitiveCountArrLength-1))
			continue;

		deUint32	multiplier	= 1;
		deUint32	offset		= 0;
		// Calculated by Vulkan 23.1
		switch (topology)
		{
			case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:													break;
			case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:						multiplier = 2;				break;
			case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:													break;
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:					multiplier = 3;				break;
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:												break;
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:									offset = 1;	break;
			case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:		multiplier = 4;	offset = 1;	break;
			case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:						offset = 1;	break;
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:	multiplier = 6;				break;
			case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:	multiplier = 2;				break;
			default:														DE_FATAL("Unsupported topology.");
		}

		const deUint32	vertexCount		= multiplier * primitives + offset;
		std::string		name			= de::toString(primitives);

		switch (command)
		{
			case DRAW_COMMAND_TYPE_DRAW:
			{
				deUint32	firstPrimitive	= rnd.getInt(0, primitives);
				deUint32	firstVertex		= multiplier * firstPrimitive;
				testGroup->addChild(new DrawCase(testCtx, name.c_str(),
					DrawParams(topology, groupParams, vertexCount, 1, firstVertex, 0))
				);
				break;
			}
			case DRAW_COMMAND_TYPE_DRAW_INDEXED:
			{
				deUint32	firstIndex			= rnd.getInt(0, OFFSET_LIMIT);
				deUint32	vertexOffset		= rnd.getInt(0, OFFSET_LIMIT);
				testGroup->addChild(new IndexedCase(testCtx, name.c_str(),
					DrawIndexedParams(topology, groupParams, vk::VK_INDEX_TYPE_UINT32, vertexCount, 1, firstIndex, vertexOffset, 0))
				);
				break;
			}
			case DRAW_COMMAND_TYPE_DRAW_INDIRECT:
			{
				deUint32	firstVertex		= rnd.getInt(0, OFFSET_LIMIT);

				DrawIndirectParams	params	= DrawIndirectParams(topology, groupParams, false);

				params.addCommand(vertexCount, 1, 0, 0);
				testGroup->addChild(new IndirectCase(testCtx, (name + "_single_command").c_str(), params));

				params.addCommand(vertexCount, 1, firstVertex, 0);
				testGroup->addChild(new IndirectCase(testCtx, (name + "_multi_command").c_str(), params));

				params.multiDraw = true;
				testGroup->addChild(new IndirectCase(testCtx, (name + "_multi_command_multi_draw").c_str(), params));

				break;
			}
			case DRAW_COMMAND_TYPE_DRAW_INDEXED_INDIRECT:
			{
				deUint32	firstIndex		= rnd.getInt(vertexCount, OFFSET_LIMIT);
				deUint32	vertexOffset	= rnd.getInt(vertexCount, OFFSET_LIMIT);

				DrawIndexedIndirectParams	params	= DrawIndexedIndirectParams(topology, groupParams, vk::VK_INDEX_TYPE_UINT32, false);

				params.addCommand(vertexCount, 1, 0, 0, 0);
				testGroup->addChild(new IndexedIndirectCase(testCtx, (name + "_single_command").c_str(), params));

				params.addCommand(vertexCount, 1, firstIndex, vertexOffset, 0);
				testGroup->addChild(new IndexedIndirectCase(testCtx, (name + "_multi_command").c_str(), params));

				params.multiDraw = true;
				testGroup->addChild(new IndexedIndirectCase(testCtx, (name + "_multi_command_multi_draw").c_str(), params));

				break;
			}
			default:
				DE_FATAL("Unsupported draw command.");
		}
	}
}

void createDrawTests(tcu::TestCaseGroup* testGroup, const SharedGroupParams groupParams)
{
	for (deUint32 drawTypeIndex = 0; drawTypeIndex < DRAW_COMMAND_TYPE_DRAW_LAST; ++drawTypeIndex)
	{
		const DrawCommandType			command			(static_cast<DrawCommandType>(drawTypeIndex));
		de::MovePtr<tcu::TestCaseGroup>	topologyGroup	(new tcu::TestCaseGroup(testGroup->getTestContext(), getDrawCommandTypeName(command)));

		for (deUint32 topologyIdx = 0; topologyIdx != vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; ++topologyIdx)
		{
			const vk::VkPrimitiveTopology	topology	(static_cast<vk::VkPrimitiveTopology>(topologyIdx));
			const std::string				groupName	(de::toLower(getPrimitiveTopologyName(topology)).substr(22));

			// reduce number of tests for dynamic rendering cases where secondary command buffer is used
			if (groupParams->useSecondaryCmdBuffer && (topologyIdx % 2u))
				continue;

			if (groupParams->nestedSecondaryCmdBuffer && drawTypeIndex != DRAW_COMMAND_TYPE_DRAW)
				continue;

			// Testcases with a specific topology.
			addTestGroup(topologyGroup.get(), groupName, populateSubGroup, TestCaseParams(command, topology, groupParams));
		}

		testGroup->addChild(topologyGroup.release());
	}

#ifndef CTS_USES_VULKANSC
	de::MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testGroup->getTestContext(), "misc"));
	if (groupParams->useDynamicRendering == false)
	{
		DrawIndexedIndirectParams params(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, groupParams, vk::VK_INDEX_TYPE_UINT32, false);
		params.addCommand(4, 1, 0, 0, 0);
		params.useMaintenance5 = true;
		miscGroup->addChild(new IndexedIndirectCase(testGroup->getTestContext(), "maintenance5", params));
	}
	testGroup->addChild(miscGroup.release());
#endif // CTS_USES_VULKANSC
}

tcu::TestCaseGroup*	createBasicDrawTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "basic_draw", createDrawTests, groupParams);
}

}	// DrawTests
}	// vkt
