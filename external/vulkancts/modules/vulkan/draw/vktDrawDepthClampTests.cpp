/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Depth clamp tests.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vktDrawDepthClampTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"

#include <cmath>
#include <limits>
#include "deMath.h"

namespace vkt
{
namespace Draw
{
namespace {
using namespace vk;
using namespace de;
using std::string;
using tcu::Vec4;

static const int					WIDTH				= 256;
static const int					HEIGHT				= 256;

struct ViewportData
{
	float minDepth;
	float maxDepth;
	float depthValue;
	float expectedValue;
};

struct TestParams
{
	string							testNameSuffix;
	std::vector<ViewportData>		viewportData;
	bool							enableDepthBias;
	float							depthBiasConstantFactor;
	bool							skipUNorm;
	bool							skipSNorm;
	std::vector<const char*>		requiredExtensions;
};

const VkFormat		depthStencilImageFormatsToTest[]	=
{
	VK_FORMAT_D16_UNORM,
	VK_FORMAT_X8_D24_UNORM_PACK32,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT_S8_UINT
};
const float			depthEpsilonValuesByFormat[]		=
{
	1e-5f,
	std::numeric_limits<float>::epsilon(),
	std::numeric_limits<float>::epsilon(),
	1e-5f,
	std::numeric_limits<float>::epsilon(),
	std::numeric_limits<float>::epsilon()
};

const float			initialClearDepth					= 0.5f;
const TestParams	depthClearValuesToTest[]			=
{
	{
		"",											// testNameSuffix
		{ {											// viewportData
			0.0f,									//   minDepth
			1.0f,									//   maxDepth
			0.3f,									//   depthValue
			0.3f,									//   expectedValue
		} },
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_clamp_input_negative",					// testNameSuffix
		{ {											// viewportData
			0.0f,									//   minDepth
			1.0f,									//   maxDepth
		   -1e6f,									//   depthValue
			0.0f,									//   expectedValue
		} },
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_clamp_input_positive",					// testNameSuffix
		{ {											// viewportData
			0.0f,									//   minDepth
			1.0f,									//   maxDepth
			1.e6f,									//   depthValue
			1.0f,									//   expectedValue
		} },
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_depth_bias_clamp_input_negative",			// testNameSuffix
		{ {											// viewportData
			0.0f,									//   minDepth
			1.0f,									//   maxDepth
			0.3f,									//   depthValue
			0.0f,									//   expectedValue
		} },
		true,										// enableDepthBias
		-2e11f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_depth_bias_clamp_input_positive",			// testNameSuffix
		{ {											// viewportData
			0.0f,									//   minDepth
			1.0f,									//   maxDepth
			0.7f,									//   depthValue
			1.0f,									//   expectedValue
		} },
		true,										// enableDepthBias
		2e11f,										// depthBiasConstantFactor
		false,										// skipUNorm
		false,										// skipSNorm
		{},											// requiredExtensions
	},
	{
		"_depth_range_unrestricted_negative",		// testNameSuffix
		{ {											// viewportData
		   -1.5f,									//   minDepth
			1.0f,									//   maxDepth
		   -1.5f,									//   depthValue
		   -1.5f,									//   expectedValue
		} },
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		true,										// skipUNorm
		true,										// skipSNorm
		{
			"VK_EXT_depth_range_unrestricted"		// requiredExtensions[0]
		},
	},
	{
		"_depth_range_unrestricted_positive",		// testNameSuffix
		{ {											// viewportData
			0.0f,									//   minDepth
			1.5f,									//   maxDepth
			1.5f,									//   depthValue
			1.5f,									//   expectedValue
		} },
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		true,										// skipUNorm
		true,										// skipSNorm
		{
			"VK_EXT_depth_range_unrestricted"		// requiredExtensions[0]
		},
	},
	{
		"_clamp_four_viewports",					// testNameSuffix
		{											// viewportData
			{
				0.0f,								//   minDepth
				0.5f,								//   maxDepth
				0.7f,								//   depthValue
				0.35f,								//   expectedValue: 0.7 * 0.5 + (1.0 - 0.7) * 0.0) = 0.35
			},
			{
				0.9f,								//   minDepth
				1.0f,								//   maxDepth
				1.0f,								//   depthValue
				1.0f,								//   expectedValue: 1.0 * 1.0 + (1.0 - 1.0) * 0.9 = 1.0
			},
			{
				0.5f,								//   minDepth
				1.0f,								//   maxDepth
				0.9f,								//   depthValue
				0.95f,								//   expectedValue: 0.9 * 1.0 + (1.0 - 0.9) * 0.5 = 0.95
			},
			{
				0.5f,								//   minDepth
				0.9f,								//   maxDepth
				0.4f,								//   depthValue
				0.66f,								//   expectedValue: 0.4 * 0.9 + (1.0 - 0.4) * 0.5 = 0.66
			},
		},
		false,										// enableDepthBias
		0.0f,										// depthBiasConstantFactor
		true,										// skipUNorm
		true,										// skipSNorm
		{},
	}
};

bool isUnormDepthFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D16_UNORM_S8_UINT:
			/* Special case for combined depth-stencil-unorm modes for which tcu::getTextureChannelClass()
			   returns TEXTURECHANNELCLASS_LAST */
			return true;
		default:
			return vk::isUnormFormat(format);
	}
}

class DepthClampTestInstance : public TestInstance {
public:
								DepthClampTestInstance	(Context& context, const TestParams& params, const VkFormat format, const float epsilon, const SharedGroupParams groupParams);
	tcu::TestStatus				iterate					();

private:
	tcu::ConstPixelBufferAccess draw					();

	void						preRenderCommands		(VkCommandBuffer cmdBuffer, const VkImageAspectFlagBits aspectBits, const VkClearValue& clearValue) const;
	void						drawCommands			(VkCommandBuffer cmdBuffer) const;

#ifndef CTS_USES_VULKANSC
	void						beginSecondaryCmdBuffer	(VkCommandBuffer cmdBuffer, VkRenderingFlagsKHR renderingFlags = 0u) const;
	void						beginDynamicRender		(VkCommandBuffer cmdBuffer, VkClearValue clearValue, VkRenderingFlagsKHR renderingFlags = 0u) const;
#endif // CTS_USES_VULKANSC

	const TestParams									m_params;
	const VkFormat										m_format;
	const float											m_epsilon;
	std::vector<VkViewport>								m_viewportVect;
	std::vector<VkRect2D>								m_scissorVect;
	const SharedGroupParams								m_groupParams;
	SharedPtr<Image>									m_depthTargetImage;
	Move<VkImageView>									m_depthTargetView;
	SharedPtr<Buffer>									m_vertexBuffer;
	Move<VkRenderPass>									m_renderPass;
	Move<VkFramebuffer>									m_framebuffer;
	Move<VkPipelineLayout>								m_pipelineLayout;
	Move<VkPipeline>									m_pipeline;
};

static const Vec4					vertices[]			= {
	Vec4(-1.0f, -1.0f,  0.5f, 1.0f),	// 0 -- 2
	Vec4(-1.0f,  1.0f,  0.5f, 1.0f),	// |  / |
	Vec4( 1.0f, -1.0f,  0.5f, 1.0f),	// | /  |
	Vec4( 1.0f,  1.0f,  0.5f, 1.0f)		// 1 -- 3
};
static const VkPrimitiveTopology	verticesTopology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

DepthClampTestInstance::DepthClampTestInstance (Context& context, const TestParams& params, const VkFormat format, const float epsilon, const SharedGroupParams groupParams)
	: TestInstance		(context)
	, m_params			(params)
	, m_format			(format)
	, m_epsilon			(epsilon)
	, m_viewportVect	(params.viewportData.size(), VkViewport())
	, m_scissorVect		(params.viewportData.size(), VkRect2D())
	, m_groupParams		(groupParams)
{
	const DeviceInterface&		vk								= m_context.getDeviceInterface();
	const VkDevice				device							= m_context.getDevice();
	const deUint32				queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();
	const deUint32				viewportCount					= static_cast<deUint32>(m_params.viewportData.size());

	// create viewport grid
	{
		const deUint32	columnCount		= deCeilFloatToInt32(deFloatSqrt(static_cast<float>(viewportCount)));
		const deUint32	rowCount		= deCeilFloatToInt32(static_cast<float>(viewportCount) / static_cast<float>(columnCount));
		const deUint32	rectWidth		= WIDTH / columnCount;
		const deUint32	rectHeight		= HEIGHT / rowCount;

		VkOffset2D		pos				{ 0, 0 };

		for (deUint32 viewportIndex = 0; viewportIndex < viewportCount; ++viewportIndex)
		{
			// move to next row
			if ((viewportIndex != 0) && (viewportIndex % columnCount == 0))
			{
				pos.x = 0;
				pos.y += rectHeight;
			}

			m_viewportVect[viewportIndex] =
			{
				static_cast<float>(pos.x),							// float	x;
				static_cast<float>(pos.y),							// float	y;
				static_cast<float>(rectWidth),						// float	width;
				static_cast<float>(rectHeight),						// float	height;
				m_params.viewportData[viewportIndex].minDepth,		// float	minDepth;
				m_params.viewportData[viewportIndex].maxDepth,		// float	maxDepth;
			};

			m_scissorVect[viewportIndex] =
			{
				pos,
				{rectWidth, rectHeight}
			};

			pos.x += rectWidth;
		}
	}

	DescriptorPoolBuilder		descriptorPoolBuilder;
	DescriptorSetLayoutBuilder	descriptorSetLayoutBuilder;
	// Vertex data
	{
		const size_t			verticesCount					= DE_LENGTH_OF_ARRAY(vertices);
		const VkDeviceSize		dataSize						= verticesCount * sizeof(Vec4);
		m_vertexBuffer											= Buffer::createAndAlloc(vk, device, BufferCreateInfo(dataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
																m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

		Vec4 testVertices[verticesCount];
		deMemcpy(testVertices, vertices, dataSize);
		for(size_t i = 0; i < verticesCount; ++i)
			testVertices[i][2] = params.viewportData[0].depthValue;
		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), testVertices, static_cast<std::size_t>(dataSize));
		flushMappedMemoryRange(vk, device, m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}

	const VkImageUsageFlags		targetImageUsageFlags						= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const ImageCreateInfo		targetImageCreateInfo						(VK_IMAGE_TYPE_2D, m_format, { WIDTH, HEIGHT, 1u }, 1u,	1u,	VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, targetImageUsageFlags);
	m_depthTargetImage														= Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), queueFamilyIndex);

	const ImageViewCreateInfo	depthTargetViewInfo							(m_depthTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_format);
	m_depthTargetView = createImageView(vk, device, &depthTargetViewInfo);

	// Render pass and framebuffer
	if (!m_groupParams->useDynamicRendering)
	{
		RenderPassCreateInfo		renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(
			m_format,												// format
			VK_SAMPLE_COUNT_1_BIT,									// samples
			VK_ATTACHMENT_LOAD_OP_LOAD,								// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,							// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// stencilStoreOp
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// initialLayout
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));		// finalLayout
		const VkAttachmentReference depthAttachmentReference					= makeAttachmentReference(0u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		renderPassCreateInfo.addSubpass(SubpassDescription(
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// pipelineBindPoint
			(VkSubpassDescriptionFlags)0,		// flags
			0u,									// inputAttachmentCount
			DE_NULL,							// inputAttachments
			0u,									// colorAttachmentCount
			DE_NULL,							// colorAttachments
			DE_NULL,							// resolveAttachments
			depthAttachmentReference,			// depthStencilAttachment
			0u,									// preserveAttachmentCount
			DE_NULL));							// preserveAttachments
		m_renderPass															= createRenderPass(vk, device, &renderPassCreateInfo);

		const std::vector<VkImageView>				depthAttachments			{ *m_depthTargetView };
		FramebufferCreateInfo						framebufferCreateInfo		(*m_renderPass, depthAttachments, WIDTH, HEIGHT, 1);
		m_framebuffer															= createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	// Vertex input
	const VkVertexInputBindingDescription		vertexInputBindingDescription	=
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

	const PipelineCreateInfo::VertexInputState	vertexInputState				= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription,
																													   1, &vertexInputAttributeDescription);

	// Graphics pipeline
	const Unique<VkShaderModule>	vertexModule								(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	Move<VkShaderModule>			geometryModule;
	const Unique<VkShaderModule>	fragmentModule								(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));

	if (viewportCount > 1)
		geometryModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0);

	const PipelineLayoutCreateInfo	pipelineLayoutCreateInfo					(0u, DE_NULL, 0u, DE_NULL);
	m_pipelineLayout															= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	std::vector<VkDynamicState>		dynamicStates								{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vertexModule,   "main", VK_SHADER_STAGE_VERTEX_BIT));
	if (*geometryModule != 0)
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*geometryModule, "main", VK_SHADER_STAGE_GEOMETRY_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fragmentModule, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState (PipelineCreateInfo::VertexInputState	(vertexInputState));
	pipelineCreateInfo.addState (PipelineCreateInfo::InputAssemblerState(verticesTopology));
	pipelineCreateInfo.addState (PipelineCreateInfo::ViewportState		(viewportCount, m_viewportVect, m_scissorVect));
	pipelineCreateInfo.addState (PipelineCreateInfo::DepthStencilState	(VK_TRUE, VK_TRUE, VK_COMPARE_OP_ALWAYS, VK_FALSE, VK_FALSE));
	pipelineCreateInfo.addState (PipelineCreateInfo::RasterizerState	(
		VK_TRUE,										// depthClampEnable
		VK_FALSE,										// rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,							// polygonMode
		VK_CULL_MODE_NONE,								// cullMode
		VK_FRONT_FACE_CLOCKWISE,						// frontFace
		m_params.enableDepthBias ? VK_TRUE : VK_FALSE,	// depthBiasEnable
		m_params.depthBiasConstantFactor,				// depthBiasConstantFactor
		0.0f,											// depthBiasClamp
		0.0f,											// depthBiasSlopeFactor
		1.0f));											// lineWidth
	pipelineCreateInfo.addState (PipelineCreateInfo::MultiSampleState	());
	pipelineCreateInfo.addState (PipelineCreateInfo::DynamicState		(dynamicStates));

#ifndef CTS_USES_VULKANSC
	VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		DE_NULL,
		0u,
		0u,
		DE_NULL,
		m_format,
		m_format
	};

	if (m_groupParams->useDynamicRendering)
		pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

tcu::ConstPixelBufferAccess DepthClampTestInstance::draw ()
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const VkQueue						queue				= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo				cmdPoolCreateInfo	(queueFamilyIndex);
	const Unique<VkCommandPool>			cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	Move<VkCommandBuffer>				secCmdBuffer;
	const VkClearValue					clearDepth			= makeClearValueDepthStencil(initialClearDepth, 0u);
	const bool							isCombinedType		= tcu::isCombinedDepthStencilType(mapVkFormat(m_format).type) && m_format != VK_FORMAT_X8_D24_UNORM_PACK32;
	const VkImageAspectFlagBits			aspectBits			= (VkImageAspectFlagBits)(isCombinedType ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT);

#ifndef CTS_USES_VULKANSC
	if (m_groupParams->useSecondaryCmdBuffer)
	{
		secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

		// record secondary command buffer
		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			beginSecondaryCmdBuffer(*secCmdBuffer, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
			beginDynamicRender(*secCmdBuffer, clearDepth);
		}
		else
			beginSecondaryCmdBuffer(*secCmdBuffer);

		drawCommands(*secCmdBuffer);

		if (m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endRendering(vk, *secCmdBuffer);

		endCommandBuffer(vk, *secCmdBuffer);

		// record primary command buffer
		beginCommandBuffer(vk, *cmdBuffer, 0u);
		preRenderCommands(*cmdBuffer, aspectBits, clearDepth);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			beginDynamicRender(*cmdBuffer, clearDepth, VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

		vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

		if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
			endRendering(vk, *cmdBuffer);
	}
	else if (m_groupParams->useDynamicRendering)
	{
		beginCommandBuffer(vk, *cmdBuffer);
		preRenderCommands(*cmdBuffer, aspectBits, clearDepth);
		beginDynamicRender(*cmdBuffer, clearDepth);
		drawCommands(*cmdBuffer);
		endRendering(vk, *cmdBuffer);
	}
#endif // CTS_USES_VULKANSC

	if (!m_groupParams->useDynamicRendering)
	{
		const VkRect2D renderArea = makeRect2D(0, 0, WIDTH, HEIGHT);

		beginCommandBuffer(vk, *cmdBuffer);
		preRenderCommands(*cmdBuffer, aspectBits, clearDepth);
		beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, renderArea);
		drawCommands(*cmdBuffer);
		endRenderPass(vk, *cmdBuffer);
	}

	transition2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), aspectBits,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	VK_CHECK(vk.queueWaitIdle(queue));

	return m_depthTargetImage->readDepth(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { 0, 0, 0 }, WIDTH, HEIGHT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void DepthClampTestInstance::preRenderCommands(VkCommandBuffer cmdBuffer, const VkImageAspectFlagBits aspectBits, const VkClearValue& clearValue) const
{
	const DeviceInterface&	vk				= m_context.getDeviceInterface();
	const bool				isCombinedType	= tcu::isCombinedDepthStencilType(mapVkFormat(m_format).type) && m_format != VK_FORMAT_X8_D24_UNORM_PACK32;

	if (isCombinedType)
		initialTransitionDepthStencil2DImage(vk, cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	else
		initialTransitionDepth2DImage(vk, cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	const ImageSubresourceRange subresourceRange (aspectBits);

	vk.cmdClearDepthStencilImage(cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.depthStencil, 1, &subresourceRange);

	transition2DImage(vk, cmdBuffer, m_depthTargetImage->object(), aspectBits,
					  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					  VK_ACCESS_TRANSFER_WRITE_BIT		  , VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					  VK_PIPELINE_STAGE_TRANSFER_BIT	  , VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

	{
		const VkMemoryBarrier memBarrier					= { VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
	}
}

void DepthClampTestInstance::drawCommands(VkCommandBuffer cmdBuffer) const
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDeviceSize		offset	= 0;
	const VkBuffer			buffer	= m_vertexBuffer->object();

	// if there is more then one viewport we are also checking
	// proper behaviour of cmdSetViewport/Scissor - there was
	// a driver bug that caused invalid behaviour of those
	// functions when firstViewport/Scissor had a non 0 value
	deUint32 indexCount = static_cast<deUint32>(m_viewportVect.size());
	for (deUint32 index = 0; index < indexCount; ++index)
	{
		vk.cmdSetViewport(cmdBuffer, index, 1u, &m_viewportVect[index]);
		vk.cmdSetScissor(cmdBuffer, index, 1u, &m_scissorVect[index]);
	}

	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &buffer, &offset);
	vk.cmdDraw(cmdBuffer, DE_LENGTH_OF_ARRAY(vertices), 1, 0, 0);
}

#ifndef CTS_USES_VULKANSC
void DepthClampTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkRenderingFlagsKHR renderingFlags) const
{
	VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,		// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		renderingFlags,															// VkRenderingFlagsKHR				flags;
		0u,																		// uint32_t							viewMask;
		0u,																		// uint32_t							colorAttachmentCount;
		DE_NULL,																// const VkFormat*					pColorAttachmentFormats;
		m_format,																// VkFormat							depthAttachmentFormat;
		VK_FORMAT_UNDEFINED,													// VkFormat							stencilAttachmentFormat;
		VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			rasterizationSamples;
	};

	const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

	VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (!m_groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

	const VkCommandBufferBeginInfo commandBufBeginParams
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,							// VkStructureType					sType;
		DE_NULL,																// const void*						pNext;
		usageFlags,																// VkCommandBufferUsageFlags		flags;
		&bufferInheritanceInfo
	};

	const DeviceInterface& vk = m_context.getDeviceInterface();
	VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}

void DepthClampTestInstance::beginDynamicRender(VkCommandBuffer cmdBuffer, VkClearValue clearValue, VkRenderingFlagsKHR renderingFlags) const
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkRect2D			renderArea	= makeRect2D(0, 0, WIDTH, HEIGHT);

	VkRenderingAttachmentInfoKHR depthAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,		// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		*m_depthTargetView,										// VkImageView							imageView;
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// VkImageLayout						imageLayout;
		VK_RESOLVE_MODE_NONE,									// VkResolveModeFlagBits				resolveMode;
		DE_NULL,												// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_LOAD,								// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp					storeOp;
		clearValue												// VkClearValue							clearValue;
	};

	VkRenderingInfoKHR renderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		DE_NULL,
		renderingFlags,											// VkRenderingFlagsKHR					flags;
		renderArea,												// VkRect2D								renderArea;
		1u,														// deUint32								layerCount;
		0u,														// deUint32								viewMask;
		0u,														// deUint32								colorAttachmentCount;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
		&depthAttachment,										// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
		DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
	};

	vk.cmdBeginRendering(cmdBuffer, &renderingInfo);
}
#endif // CTS_USES_VULKANSC

tcu::TestStatus DepthClampTestInstance::iterate (void)
{
	const tcu::ConstPixelBufferAccess resultImage = draw();

	DE_ASSERT((isUnormDepthFormat(m_format) == false) ||
		(m_params.viewportData[0].expectedValue >= 0.0f && m_params.viewportData[0].expectedValue <= 1.0f));

	for(deUint32 viewportIndex = 0 ; viewportIndex < m_scissorVect.size() ; ++viewportIndex)
	{
		const float		expectedValue	= m_params.viewportData[viewportIndex].expectedValue;
		const VkRect2D&	viewRect		= m_scissorVect[viewportIndex];

		deInt32 xStart	= viewRect.offset.x;
		deInt32 xEnd	= xStart + viewRect.extent.width;
		deInt32 yStart	= viewRect.offset.y;
		deInt32 yEnd	= yStart + viewRect.extent.height;

		for (int y = yStart; y < yEnd; ++y)
		for (int x = xStart; x < xEnd; ++x)
		{
			if (std::abs(expectedValue - resultImage.getPixDepth(x, y, 0)) >= m_epsilon)
			{
				tcu::TestLog& log = m_context.getTestContext().getLog();
				log << tcu::TestLog::ImageSet("Result of rendering", "")
					<< tcu::TestLog::Image("Result", "", resultImage)
					<< tcu::TestLog::EndImageSet;

				std::ostringstream msg;
				msg << "Depth value mismatch, expected: " << expectedValue
					<< ", got: " << resultImage.getPixDepth(x, y, 0) << " at (" << x << ", " << y << ", 0)";

				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class DepthClampTest : public TestCase
{
public:
	DepthClampTest (tcu::TestContext &testCtx, const string& name, const string& description, const TestParams &params, const VkFormat format, const float epsilon, const SharedGroupParams groupParams)
		: TestCase		(testCtx, name, description)
		, m_params		(params)
		, m_format		(format)
		, m_epsilon		(epsilon)
		, m_groupParams	(groupParams)
	{
	}

	virtual void initPrograms (SourceCollections& programCollection) const
	{
		programCollection.glslSources.add("vert") << glu::VertexSource(
			"#version 450\n"
			"\n"
			"layout(location = 0) in vec4 in_position;\n"
			"void main(void)\n"
			"{\n"
			"    gl_Position = in_position;\n"
			"}\n");

		if (m_params.viewportData.size() > 1)
		{
			// gl_ViewportIndex built-in variable is available only to the geometry shader

			std::string depthValues = "";
			for (const auto& vd : m_params.viewportData)
				depthValues += std::to_string(vd.depthValue) + ", ";

			// this geometry shader draws the same quad but with diferent depth to all viewports
			programCollection.glslSources.add("geom") << glu::GeometrySource(
				std::string("#version 450\n") +
				"#extension GL_EXT_geometry_shader : require\n"
				"layout(invocations = " + std::to_string(m_params.viewportData.size()) + ") in;\n"
				"layout(triangles) in;\n"
				"layout(triangle_strip, max_vertices = 4) out;\n"
				"void main()\n"
				"{\n"
				"  const float depthValues[] = { " + depthValues + " 0.0 };\n"
				"  for (int i = 0; i < gl_in.length(); i++)\n"
				"  {\n"
				"    gl_ViewportIndex = gl_InvocationID;\n"
				"    gl_Position      = gl_in[i].gl_Position;\n"
				"    gl_Position.z    = depthValues[gl_InvocationID];\n"
				"    EmitVertex();\n"
				"  }\n"
				"  EndPrimitive();\n"
				"}");
		}

		programCollection.glslSources.add("frag") << glu::FragmentSource(
			"#version 450\n"
			"void main(void)\n"
			"{\n"
			"}\n");
	}

	virtual void checkSupport (Context& context) const
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_CLAMP);
		for(const auto& extensionName : m_params.requiredExtensions)
			context.requireDeviceFunctionality(extensionName);

		if (m_params.viewportData.size() > 1)
		{
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
			if (!context.getDeviceFeatures().geometryShader)
				throw tcu::NotSupportedError("Geometry shader is not supported");
		}

		VkImageFormatProperties imageFormatProperties;
		const auto&	vki		= context.getInstanceInterface();
		const auto&	vkd		= context.getPhysicalDevice();
		const auto	usage	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (vki.getPhysicalDeviceImageFormatProperties(vkd, m_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0u, &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		{
			TCU_THROW(NotSupportedError, "Format not supported");
		}

		if (m_groupParams->useDynamicRendering)
			context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
	}

	virtual TestInstance* createInstance (Context& context) const
	{
		return new DepthClampTestInstance(context, m_params, m_format, m_epsilon, m_groupParams);
	}

private:
	const TestParams			m_params;
	const VkFormat				m_format;
	const float					m_epsilon;
	const SharedGroupParams		m_groupParams;
};

std::string getFormatCaseName (VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

void createTests (tcu::TestCaseGroup* testGroup, const SharedGroupParams groupParams)
{
	for(int i = 0; i < DE_LENGTH_OF_ARRAY(depthStencilImageFormatsToTest); ++i)
	{
		const auto format = depthStencilImageFormatsToTest[i];

		// reduce number of tests for dynamic rendering cases where secondary command buffer is used
		if (groupParams->useSecondaryCmdBuffer && (format != VK_FORMAT_D16_UNORM))
			continue;

		const float		epsilon			= depthEpsilonValuesByFormat[i];
		const auto		formatCaseName	= getFormatCaseName(format);
		for(const auto& params : depthClearValuesToTest)
		{
			if ((params.skipSNorm && vk::isSnormFormat(format)) || (params.skipUNorm && isUnormDepthFormat(format)))
				continue;
			const auto	testCaseName	= formatCaseName + params.testNameSuffix;
			testGroup->addChild(new DepthClampTest(testGroup->getTestContext(), testCaseName, "Depth clamp", params, format, epsilon, groupParams));
		}
	}
}
}	// anonymous

tcu::TestCaseGroup*	createDepthClampTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	return createTestGroup(testCtx, "depth_clamp", "Depth Clamp Tests", createTests, groupParams);
}
}	// Draw
}	// vkt
