/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * -----------------------------
 *
 * Copyright (c) 2020 Google Inc.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Tests for multiple color or depth clears within a render pass
 *//*--------------------------------------------------------------------*/

#include "vktDrawMultipleClearsWithinRenderPass.hpp"

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
#include "vktDrawTestCaseUtil.hpp"

#include "deStringUtil.hpp"

#include <cmath>
#include <vector>
#include <string>
#include <sstream>

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;
using tcu::Vec4;
using de::SharedPtr;
using std::string;
using std::abs;
using std::vector;
using std::ostringstream;

const deUint32						WIDTH					= 400;
const deUint32						HEIGHT					= 300;

enum struct Topology
{
	TRIANGLE_STRIP = 0,
	TRIANGLES,
	TRIANGLE
};

const Topology						topologiesToTest[]		=
{
	Topology::TRIANGLE_STRIP,
	Topology::TRIANGLES,
	Topology::TRIANGLE
};

struct FormatPair
{
	VkFormat colorFormat;
	VkFormat depthFormat;
};

const FormatPair					formatsToTest[]			=
{
	{ VK_FORMAT_R8G8B8A8_UNORM	, VK_FORMAT_UNDEFINED	},
	{ VK_FORMAT_R8G8B8A8_SNORM	, VK_FORMAT_UNDEFINED	},
	{ VK_FORMAT_UNDEFINED		, VK_FORMAT_D32_SFLOAT	},
	{ VK_FORMAT_UNDEFINED		, VK_FORMAT_D16_UNORM	},
	{ VK_FORMAT_R8G8B8A8_UNORM	, VK_FORMAT_D32_SFLOAT	},
	{ VK_FORMAT_R8G8B8A8_UNORM	, VK_FORMAT_D16_UNORM	},
	{ VK_FORMAT_R8G8B8A8_SNORM	, VK_FORMAT_D32_SFLOAT	},
	{ VK_FORMAT_R8G8B8A8_SNORM	, VK_FORMAT_D16_UNORM	},
};

const Vec4							verticesTriangleStrip[]	=
{
	Vec4(-1.0f, -1.0f,	0.0f, 1.0f),	// 0 -- 2
	Vec4(-1.0f,	 1.0f,	0.0f, 1.0f),	// |  / |
	Vec4( 1.0f, -1.0f,	0.0f, 1.0f),	// | /	|
	Vec4( 1.0f,	 1.0f,	0.0f, 1.0f)		// 1 -- 3
};
const Vec4							verticesTriangles[]		=
{
	Vec4(-1.0f, -1.0f,	0.0f, 1.0f),	// 0 - 1
	Vec4(-1.0f,	 1.0f,	0.0f, 1.0f),	// | /
	Vec4( 1.0f, -1.0f,	0.0f, 1.0f),	// 2
	Vec4( 1.0f, -1.0f,	0.0f, 1.0f),	//	   4
	Vec4(-1.0f,	 1.0f,	0.0f, 1.0f),	//	 / |
	Vec4( 1.0f,	 1.0f,	0.0f, 1.0f)		// 3 - 5
};
const Vec4							verticesBigTriangle[]	=
{
	Vec4(-1.0f, -1.0f,	0.0f, 1.0f),	// 0 - 2
	Vec4(-1.0f,	 3.0f,	0.0f, 1.0f),	// | /
	Vec4( 3.0f, -1.0f,	0.0f, 1.0f),	// 1
};

const deUint32			TOPOLOGY_MAX_VERTICES_COUNT			= 6;
const deUint32			TEST_MAX_STEPS_COUNT				= 3;

struct Vertices
{
	const char*			testNameSuffix;
	VkPrimitiveTopology	topology;
	deUint32			verticesCount;
	const Vec4*			vertices;
};

const Vertices			verticesByTopology[]				=
{
	{
		"_triangle_strip",
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
		DE_LENGTH_OF_ARRAY(verticesTriangleStrip),
		verticesTriangleStrip
	},
	{
		"_triangles",
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		DE_LENGTH_OF_ARRAY(verticesTriangles),
		verticesTriangles
	},
	{
		"_big_triangle",
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		DE_LENGTH_OF_ARRAY(verticesBigTriangle),
		verticesBigTriangle
	}
};

enum struct ClearOp
{
	LOAD = 0,
	DRAW,
	CLEAR
};

struct ClearStep
{
	ClearOp					clearOp;
	Vec4					color;
	float					depth;
};

struct TestParams
{
	VkFormat				colorFormat;
	VkFormat				depthFormat;
	Topology				topology;
	Vec4					expectedColor;
	float					colorEpsilon;
	float					expectedDepth;
	float					depthEpsilon;
	deUint32				repeatCount;
	bool					enableBlend;
	bool					useDynamicRendering;
	vector<ClearStep>		steps;
};

class MultipleClearsTest : public TestInstance
{
public:
							MultipleClearsTest		(Context& context, const TestParams& params);
	virtual tcu::TestStatus	iterate					(void);
private:
	void					clearAttachments		(const DeviceInterface& vk, VkCommandBuffer cmdBuffer, const ClearOp clearOp, const size_t stepIndex);

	SharedPtr<Image>		m_colorTargetImage;
	SharedPtr<Image>		m_depthTargetImage;
	Move<VkImageView>		m_colorTargetView;
	Move<VkImageView>		m_depthTargetView;
	SharedPtr<Buffer>		m_vertexBuffer;
	Move<VkRenderPass>		m_renderPass;
	Move<VkFramebuffer>		m_framebuffer;
	Move<VkPipelineLayout>	m_pipelineLayout;
	Move<VkPipeline>		m_pipeline;

	const TestParams		m_params;
	Vec4					m_vertices[TOPOLOGY_MAX_VERTICES_COUNT * TEST_MAX_STEPS_COUNT];
};

MultipleClearsTest::MultipleClearsTest (Context &context, const TestParams& params)
	: TestInstance(context)
	, m_params(params)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				device				= m_context.getDevice();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const bool					hasColor			= m_params.colorFormat != VK_FORMAT_UNDEFINED;
	const bool					hasDepth			= m_params.depthFormat != VK_FORMAT_UNDEFINED;

	DescriptorSetLayoutBuilder	descriptorSetLayoutBuilder;
	// Vertex data
	const auto&					vertexData			= verticesByTopology[(size_t)m_params.topology];
	{
		DE_ASSERT(vertexData.verticesCount <= TOPOLOGY_MAX_VERTICES_COUNT);
		const size_t			verticesCount		= vertexData.verticesCount;
		const VkDeviceSize		dataSize			= verticesCount * sizeof(Vec4);
		const VkDeviceSize		totalDataSize		= m_params.steps.size() * dataSize;
		DE_ASSERT(totalDataSize <= sizeof(m_vertices));

		for(size_t i = 0; i < m_params.steps.size(); ++i)
		{
			const size_t start = i * verticesCount;
			deMemcpy(&m_vertices[start], vertexData.vertices, static_cast<size_t>(dataSize));
			for(size_t j = 0; j < verticesCount; ++j)
				m_vertices[start + j][2] = m_params.steps[i].depth;
		}
		m_vertexBuffer								= Buffer::createAndAlloc(vk, device, BufferCreateInfo(totalDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
																			 m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
		deMemcpy(m_vertexBuffer->getBoundMemory().getHostPtr(), m_vertices, static_cast<std::size_t>(totalDataSize));
		flushMappedMemoryRange(vk, device, m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}

	if (hasColor)
	{
		const VkImageUsageFlags		targetImageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		const ImageCreateInfo		targetImageCreateInfo		(VK_IMAGE_TYPE_2D, m_params.colorFormat, { WIDTH, HEIGHT, 1u }, 1u,	1u,	VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, targetImageUsageFlags);
		m_colorTargetImage										= Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(), queueFamilyIndex);
		const ImageViewCreateInfo	colorTargetViewInfo			(m_colorTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_params.colorFormat);
		m_colorTargetView										= createImageView(vk, device, &colorTargetViewInfo);
	}

	if (hasDepth)
	{
		const VkImageUsageFlags		depthImageUsageFlags		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		const ImageCreateInfo		depthImageCreateInfo		(VK_IMAGE_TYPE_2D, m_params.depthFormat, { WIDTH, HEIGHT, 1u }, 1u,	1u,	VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, depthImageUsageFlags);
		m_depthTargetImage										= Image::createAndAlloc(vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(), queueFamilyIndex);
		const ImageViewCreateInfo	depthTargetViewInfo			(m_depthTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_params.depthFormat);
		m_depthTargetView										= createImageView(vk, device, &depthTargetViewInfo);
	}

	// Render pass
	if (!m_params.useDynamicRendering)
	{
		RenderPassCreateInfo			renderPassCreateInfo;
		if (hasColor)
		{
			renderPassCreateInfo.addAttachment(AttachmentDescription(
				 m_params.colorFormat,								// format
				 VK_SAMPLE_COUNT_1_BIT,								// samples
				 VK_ATTACHMENT_LOAD_OP_LOAD,						// loadOp
				 VK_ATTACHMENT_STORE_OP_STORE,						// storeOp
				 VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// stencilLoadOp
				 VK_ATTACHMENT_STORE_OP_DONT_CARE,					// stencilStoreOp
				 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// initialLayout
				 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));		// finalLayout
		}
		if (hasDepth)
		{
			renderPassCreateInfo.addAttachment(AttachmentDescription(
				m_params.depthFormat,								// format
				VK_SAMPLE_COUNT_1_BIT,								// samples
				VK_ATTACHMENT_LOAD_OP_LOAD,							// loadOp
				VK_ATTACHMENT_STORE_OP_STORE,						// storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// stencilStoreOp
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// initialLayout
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));	// finalLayout
		}
		const VkAttachmentReference colorAttachmentReference		= hasColor ? makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) : AttachmentReference();
		const VkAttachmentReference depthAttachmentReference		= hasDepth ? makeAttachmentReference(hasColor ? 1u : 0u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) : AttachmentReference();
		renderPassCreateInfo.addSubpass(SubpassDescription(
			VK_PIPELINE_BIND_POINT_GRAPHICS,						// pipelineBindPoint
			(VkSubpassDescriptionFlags)0,							// flags
			0u,														// inputAttachmentCount
			DE_NULL,												// inputAttachments
			hasColor ? 1 : 0,										// colorAttachmentCount
			hasColor ? &colorAttachmentReference : DE_NULL,			// colorAttachments
			DE_NULL,												// resolveAttachments
			depthAttachmentReference,								// depthStencilAttachment
			0u,														// preserveAttachmentCount
			DE_NULL));												// preserveAttachments
		m_renderPass												= createRenderPass(vk, device, &renderPassCreateInfo);

		std::vector<VkImageView> attachments;
		if (hasColor)
			attachments.push_back(*m_colorTargetView);
		if (hasDepth)
			attachments.push_back(*m_depthTargetView);
		const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);
		m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	// Vertex input
	const VkVertexInputBindingDescription		vertexInputBindingDescription	=
	{
		0u,										// uint32_t				binding;
		sizeof(Vec4),							// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription		vertexInputAttributeDescription =
	{
		0u,										// uint32_t		location;
		0u,										// uint32_t		binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat		format;
		0u										// uint32_t		offset;
	};

	const PipelineCreateInfo::VertexInputState	vertexInputState				= PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription,
																													   1, &vertexInputAttributeDescription);

	// Graphics pipeline
	const Unique<VkShaderModule>				vertexModule					(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
	const Unique<VkShaderModule>				fragmentModule					(createShaderModule(vk, device, m_context.getBinaryCollection().get(hasColor ? "frag" : "frag_depthonly"), 0));

	const VkPushConstantRange					pcRange							= vk::VkPushConstantRange { VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ClearStep::color) };
	const PipelineLayoutCreateInfo				pipelineLayoutCreateInfo		(0u, DE_NULL, 1u, &pcRange);
	m_pipelineLayout															= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const VkRect2D								scissor							= makeRect2D(WIDTH, HEIGHT);
	const VkViewport							viewport						= makeViewport(WIDTH, HEIGHT);

	const auto									vkCbAttachmentState				= makePipelineColorBlendAttachmentState(
		m_params.enableBlend ? VK_TRUE : VK_FALSE,	// VkBool32					blendEnable
		VK_BLEND_FACTOR_SRC_ALPHA,					// VkBlendFactor			srcColorBlendFactor
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,		// VkBlendFactor			dstColorBlendFactor
		VK_BLEND_OP_ADD,							// VkBlendOp				colorBlendOp
		VK_BLEND_FACTOR_ZERO,						// VkBlendFactor			srcAlphaBlendFactor
		VK_BLEND_FACTOR_ONE,						// VkBlendFactor			dstAlphaBlendFactor
		VK_BLEND_OP_ADD,							// VkBlendOp				alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT |					// VkColorComponentFlags	colorWriteMask
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT);
	PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, (VkPipelineCreateFlags)0);
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vertexModule,	  "main", VK_SHADER_STAGE_VERTEX_BIT));
	pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fragmentModule, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
	pipelineCreateInfo.addState (PipelineCreateInfo::VertexInputState	(vertexInputState));
	pipelineCreateInfo.addState (PipelineCreateInfo::InputAssemblerState(vertexData.topology));
	pipelineCreateInfo.addState (PipelineCreateInfo::ColorBlendState	(1, &vkCbAttachmentState));
	pipelineCreateInfo.addState (PipelineCreateInfo::ViewportState		(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
	pipelineCreateInfo.addState (PipelineCreateInfo::DepthStencilState	(hasDepth, hasDepth, VK_COMPARE_OP_ALWAYS, VK_FALSE, VK_FALSE));
	pipelineCreateInfo.addState (PipelineCreateInfo::RasterizerState	());
	pipelineCreateInfo.addState (PipelineCreateInfo::MultiSampleState	());

	vk::VkPipelineRenderingCreateInfoKHR renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		DE_NULL,
		0u,
		hasColor,
		(hasColor ? &m_params.colorFormat : DE_NULL),
		(hasDepth ? m_params.depthFormat : VK_FORMAT_UNDEFINED),
		(hasDepth ? m_params.depthFormat : VK_FORMAT_UNDEFINED)
	};

	if (m_params.useDynamicRendering)
		pipelineCreateInfo.pNext = &renderingCreateInfo;

	m_pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);
}

void MultipleClearsTest::clearAttachments (const DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, const ClearOp clearOp, const size_t stepIndex)
{
	const Vec4& color = m_params.steps[stepIndex].color;
	const float depth = m_params.steps[stepIndex].depth;
	switch(clearOp) {
	case ClearOp::LOAD:
		break;
	case ClearOp::DRAW:
		{
			const auto&		vertexData		= verticesByTopology[(size_t)m_params.topology];
			const deUint32	verticesCount	= vertexData.verticesCount;
			vk.cmdPushConstants(cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(color), color.getPtr());
			vk.cmdDraw(cmdBuffer, verticesCount, 1, static_cast<deUint32>(verticesCount * stepIndex), 0);
		}
		break;
	case ClearOp::CLEAR:
		{
			vector<VkClearAttachment>	clearAttachments;
			if (m_params.colorFormat != VK_FORMAT_UNDEFINED)
			{
				const VkClearAttachment	clearAttachment	=
				{
					VK_IMAGE_ASPECT_COLOR_BIT,						// VkImageAspectFlags	 aspectMask
					static_cast<deUint32>(clearAttachments.size()),	// uint32_t				 colorAttachment
					makeClearValueColor(color)						// VkClearValue			 clearValue
				};
				clearAttachments.push_back(clearAttachment);
			}
			if (m_params.depthFormat != VK_FORMAT_UNDEFINED)
			{
				const VkClearAttachment	clearAttachment	=
				{
					VK_IMAGE_ASPECT_DEPTH_BIT,						// VkImageAspectFlags	 aspectMask
					static_cast<deUint32>(clearAttachments.size()),	// uint32_t				 colorAttachment
					makeClearValueDepthStencil(depth, 0)			// VkClearValue			 clearValue
				};
				clearAttachments.push_back(clearAttachment);
			}
			const VkClearRect			clearRect		=
			{
				makeRect2D(WIDTH, HEIGHT),							// VkRect2D	   rect
				0,													// uint32_t	   baseArrayLayer
				1													// uint32_t	   layerCount
			};
			vk.cmdClearAttachments(cmdBuffer, static_cast<deUint32>(clearAttachments.size()), clearAttachments.data(), 1, &clearRect);
		}
		break;
	default:
		break;
	}
}

tcu::TestStatus MultipleClearsTest::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	const CmdPoolCreateInfo			cmdPoolCreateInfo	(queueFamilyIndex);
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const bool						hasColor			= m_params.colorFormat != VK_FORMAT_UNDEFINED;
	const bool						hasDepth			= m_params.depthFormat != VK_FORMAT_UNDEFINED;

	beginCommandBuffer(vk, *cmdBuffer);
	if (hasColor)
		initialTransitionColor2DImage(vk, *cmdBuffer, m_colorTargetImage->object(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	if (hasDepth)
		initialTransitionDepth2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

	const VkRect2D renderArea = makeRect2D(0, 0, WIDTH, HEIGHT);
	if (m_params.useDynamicRendering)
	{
		VkClearValue clearColorValue = makeClearValueColor(tcu::Vec4(0.0f));
		VkClearValue clearDepthValue = makeClearValueDepthStencil(0.0f, 0u);
		if (!m_params.steps.empty() && m_params.steps[0].clearOp == ClearOp::LOAD)
		{
			clearColorValue = makeClearValueColor(m_params.steps[0].color);
			clearDepthValue = makeClearValueDepthStencil(m_params.steps[0].depth, 0u);
		}

		vk::VkRenderingAttachmentInfoKHR colorAttachment
		{
			vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			*m_colorTargetView,										// VkImageView							imageView;
			vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout						imageLayout;
			vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
			DE_NULL,												// VkImageView							resolveImageView;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout						resolveImageLayout;
			vk::VK_ATTACHMENT_LOAD_OP_LOAD,							// VkAttachmentLoadOp					loadOp;
			vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
			clearColorValue											// VkClearValue							clearValue;
		};

		vk::VkRenderingAttachmentInfoKHR depthAttachment
		{
			vk::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			*m_depthTargetView,										// VkImageView							imageView;
			vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout						imageLayout;
			vk::VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
			DE_NULL,												// VkImageView							resolveImageView;
			vk::VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout						resolveImageLayout;
			vk::VK_ATTACHMENT_LOAD_OP_LOAD,							// VkAttachmentLoadOp					loadOp;
			vk::VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
			clearDepthValue											// VkClearValue							clearValue;
		};

		vk::VkRenderingInfoKHR renderingInfo
		{
			vk::VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			DE_NULL,
			0u,														// VkRenderingFlagsKHR					flags;
			renderArea,												// VkRect2D								renderArea;
			1u,														// deUint32								layerCount;
			0u,														// deUint32								viewMask;
			hasColor,												// deUint32								colorAttachmentCount;
			(hasColor ? &colorAttachment : DE_NULL),				// const VkRenderingAttachmentInfoKHR*	pColorAttachments;
			(hasDepth ? &depthAttachment : DE_NULL),				// const VkRenderingAttachmentInfoKHR*	pDepthAttachment;
			DE_NULL,												// const VkRenderingAttachmentInfoKHR*	pStencilAttachment;
		};

		vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
	}
	else
	{
		if (!m_params.steps.empty() && m_params.steps[0].clearOp == ClearOp::LOAD)
			beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, m_params.steps[0].color, m_params.steps[0].depth, 0);
		else
			beginRenderPass(vk, *cmdBuffer, *m_renderPass, *m_framebuffer, renderArea);
	}

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	{
		const VkDeviceSize	offset	= 0;
		const VkBuffer		buffer	= m_vertexBuffer->object();
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &buffer, &offset);
	}
	for(deUint32 i = 0; i < m_params.repeatCount; ++i)
		for(size_t j = 0; j < m_params.steps.size(); ++j)
		{
			const auto& step = m_params.steps[j];
			// ClearOp::LOAD only supported for first step
			DE_ASSERT(j == 0 || step.clearOp != ClearOp::LOAD);
			clearAttachments(vk, *cmdBuffer, step.clearOp, j);
		}

	if (m_params.useDynamicRendering)
		endRendering(vk, *cmdBuffer);
	else
		endRenderPass(vk, *cmdBuffer);

	if (hasDepth)
	{
		const VkMemoryBarrier	memBarrier	= { VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
	}
	if (hasColor)
	{
		const VkMemoryBarrier	memBarrier	= { VK_STRUCTURE_TYPE_MEMORY_BARRIER, DE_NULL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);
	}

	if (hasColor)
		transition2DImage(vk, *cmdBuffer, m_colorTargetImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
						  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_HOST_BIT);
	if (hasDepth)
		transition2DImage(vk, *cmdBuffer, m_depthTargetImage->object(), VK_IMAGE_ASPECT_DEPTH_BIT,
						  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
						  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_HOST_BIT);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	VK_CHECK(vk.queueWaitIdle(queue));

	if (hasColor)
	{
		const auto		resultImage	= m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { 0, 0, 0 }, WIDTH, HEIGHT, VK_IMAGE_ASPECT_COLOR_BIT);

		for(int z = 0; z < resultImage.getDepth(); ++z)
		for(int y = 0; y < resultImage.getHeight(); ++y)
		for(int x = 0; x < resultImage.getWidth(); ++x)
		{
			const Vec4	difference	= m_params.expectedColor - resultImage.getPixel(x,y,z);
			if (abs(difference.x()) >= m_params.colorEpsilon || abs(difference.y()) >= m_params.colorEpsilon || abs(difference.z()) >= m_params.colorEpsilon)
			{
				ostringstream msg;
				msg << "Color value mismatch, expected: " << m_params.expectedColor << ", got: " << resultImage.getPixel(x,y,z) << " at " << "(" << x << ", " << y << ", " << z << ")";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	if (hasDepth)
	{
		const auto		resultImage	= m_depthTargetImage->readSurface(queue, m_context.getDefaultAllocator(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { 0, 0, 0 }, WIDTH, HEIGHT, VK_IMAGE_ASPECT_DEPTH_BIT);

		for(int z = 0; z < resultImage.getDepth(); ++z)
		for(int y = 0; y < resultImage.getHeight(); ++y)
		for(int x = 0; x < resultImage.getWidth(); ++x)
		{
			const float	difference	= m_params.expectedDepth - resultImage.getPixDepth(x,y,z);
			if (abs(difference) >= m_params.depthEpsilon)
			{
				ostringstream msg;
				msg << "Depth value mismatch, expected: " << m_params.expectedDepth << ", got: " << resultImage.getPixDepth(x,y,z) << " at " << "(" << x << ", " << y << ", " << z << ")";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	return tcu::TestStatus::pass("Pass");
}

class MultipleClearsWithinRenderPassTest : public TestCase
{
public:
	MultipleClearsWithinRenderPassTest (tcu::TestContext& testCtx, const string& name, const string& description, const TestParams& params)
		: TestCase(testCtx, name, description)
		, m_params(params)
	{
		DE_ASSERT(m_params.steps.size() <= static_cast<size_t>(TEST_MAX_STEPS_COUNT));
	}

	virtual void initPrograms (SourceCollections& programCollection) const
	{
		{
			ostringstream src;
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
		{
			ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant) uniform Color { vec4 color; } u_color;\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    out_color = u_color.color;\n"
				<< "}\n";
			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
		{
			ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant) uniform Color { vec4 color; } u_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";
			programCollection.glslSources.add("frag_depthonly") << glu::FragmentSource(src.str());
		}
	}

	virtual void checkSupport (Context& context) const
	{
		VkImageFormatProperties imageFormatProperties;
		const auto&	vki	= context.getInstanceInterface();
		const auto&	vkd	= context.getPhysicalDevice();
		if (m_params.colorFormat != VK_FORMAT_UNDEFINED)
		{
			const auto	colorUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			if (vki.getPhysicalDeviceImageFormatProperties(vkd, m_params.colorFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, colorUsage, 0u, &imageFormatProperties) != VK_SUCCESS)
				TCU_THROW(NotSupportedError, "Color format not supported");
		}
		if (m_params.depthFormat != VK_FORMAT_UNDEFINED)
		{
			const auto	depthUsage	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			if (vki.getPhysicalDeviceImageFormatProperties(vkd, m_params.depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, depthUsage, 0u, &imageFormatProperties) != VK_SUCCESS)
				TCU_THROW(NotSupportedError, "Depth format not supported");
		}

		if (m_params.useDynamicRendering)
			context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
	}

	virtual TestInstance* createInstance (Context& context) const
	{
		return new MultipleClearsTest(context, m_params);
	}

private:
	const TestParams	m_params;
};

}	// anonymous

MultipleClearsWithinRenderPassTests::MultipleClearsWithinRenderPassTests (tcu::TestContext &testCtx, bool useDynamicRendering)
	: TestCaseGroup	(testCtx, "multiple_clears_within_render_pass", "Tests for multiple clears within render pass")
	, m_useDynamicRendering(useDynamicRendering)
{
}

MultipleClearsWithinRenderPassTests::~MultipleClearsWithinRenderPassTests ()
{
}

void MultipleClearsWithinRenderPassTests::init ()
{
	for(const auto &formatPair : formatsToTest)
	{
		ostringstream			formatSuffix;
		if (formatPair.colorFormat != VK_FORMAT_UNDEFINED)
			formatSuffix << "_c" << de::toLower(string(getFormatName(formatPair.colorFormat)).substr(9));
		if (formatPair.depthFormat != VK_FORMAT_UNDEFINED)
			formatSuffix << "_d" << de::toLower(string(getFormatName(formatPair.depthFormat)).substr(9));
		for(const auto &topology : topologiesToTest)
		{
			const string	testNameSuffix	= formatSuffix.str() + verticesByTopology[(deUint32)topology].testNameSuffix;
			{
				const TestParams params	=
				{
					formatPair.colorFormat,			// VkFormat				colorFormat;
					formatPair.depthFormat,			// VkFormat				depthFormat;
					topology,						// Topology				topology;
					Vec4(0.0f, 0.5f, 0.5f, 1.0f),	// Vec4					expectedColor;
					0.01f,							// float				colorEpsilon;
					0.9f,							// float				expectedDepth;
					0.01f,							// float				depthEpsilon;
					1u,								// deUint32				repeatCount;
					true,							// bool					enableBlend;
					m_useDynamicRendering,			// bool					useDynamicRendering;
					{								// vector<ClearStep>	steps;
						{ ClearOp::LOAD		, Vec4(1.0f, 0.0f, 0.0f, 1.0f)	, 0.7f },
						{ ClearOp::CLEAR	, Vec4(0.0f, 1.0f, 0.0f, 1.0f)	, 0.3f },
						{ ClearOp::DRAW		, Vec4(0.0f, 0.0f, 1.0f, 0.5f)	, 0.9f }
					}
				};
				addChild(new MultipleClearsWithinRenderPassTest(m_testCtx, "load_clear_draw" + testNameSuffix, "Multiple clears within same render pass, methods: load, clear, draw", params));
			}
			{
				const TestParams params	=
				{
					formatPair.colorFormat,			// VkFormat				format;
					formatPair.depthFormat,			// VkFormat				depthFormat;
					topology,						// Topology				topology;
					Vec4(0.0f, 0.5f, 0.5f, 1.0f),	// Vec4					expectedColor;
					0.01f,							// float				colorEpsilon;
					0.9f,							// float				expectedDepth;
					0.01f,							// float				depthEpsilon;
					1u,								// deUint32				repeatCount;
					true,							// bool					enableBlend;
					m_useDynamicRendering,			// bool					useDynamicRendering;
					{								// vector<ClearStep>	steps;
						{ ClearOp::DRAW		, Vec4(1.0f, 0.0f, 0.0f, 1.0f)	, 0.7f },
						{ ClearOp::CLEAR	, Vec4(0.0f, 1.0f, 0.0f, 1.0f)	, 0.3f },
						{ ClearOp::DRAW		, Vec4(0.0f, 0.0f, 1.0f, 0.5f)	, 0.9f }
					}
				};
				addChild(new MultipleClearsWithinRenderPassTest(m_testCtx, "draw_clear_draw" + testNameSuffix, "Multiple clears within same render pass, methods: draw, clear, draw", params));
			}
			{
				const TestParams params	=
				{
					formatPair.colorFormat,			// VkFormat				format;
					formatPair.depthFormat,			// VkFormat				depthFormat;
					topology,						// Topology				topology;
					Vec4(0.0f, 0.5f, 0.5f, 1.0f),	// Vec4					expectedColor;
					0.01f,							// float				colorEpsilon;
					0.9f,							// float				expectedDepth;
					0.01f,							// float				depthEpsilon;
					1u,								// deUint32				repeatCount;
					true,							// bool					enableBlend;
					m_useDynamicRendering,			// bool					useDynamicRendering;
					{								// vector<ClearStep>	steps;
						{ ClearOp::CLEAR	, Vec4(1.0f, 0.0f, 0.0f, 1.0f)	, 0.7f },
						{ ClearOp::CLEAR	, Vec4(0.0f, 1.0f, 0.0f, 1.0f)	, 0.3f },
						{ ClearOp::DRAW		, Vec4(0.0f, 0.0f, 1.0f, 0.5f)	, 0.9f }
					}
				};
				addChild(new MultipleClearsWithinRenderPassTest(m_testCtx, "clear_clear_draw" + testNameSuffix, "Multiple clears within same render pass, methods: clear, clear, draw", params));
			}
			{
				const TestParams params	=
				{
					formatPair.colorFormat,			// VkFormat				format;
					formatPair.depthFormat,			// VkFormat				depthFormat;
					topology,						// Topology				topology;
					Vec4(0.0f, 1.0f, 0.0f, 1.0f),	// Vec4					expectedColor;
					0.01f,							// float				colorEpsilon;
					0.9f,							// float				expectedDepth;
					0.01f,							// float				depthEpsilon;
					1u,								// deUint32				repeatCount;
					false,							// bool					enableBlend;
					m_useDynamicRendering,			// bool					useDynamicRendering;
					{								// vector<ClearStep>	steps;
						{ ClearOp::LOAD		, Vec4(1.0f, 0.0f, 0.0f, 1.0f)	, 0.3f },
						{ ClearOp::CLEAR	, Vec4(0.0f, 1.0f, 0.0f, 1.0f)	, 0.9f }
					}
				};
				addChild(new MultipleClearsWithinRenderPassTest(m_testCtx, "load_clear" + testNameSuffix, "Multiple clears within same render pass, methods: load, clear", params));
			}
			{
				const TestParams params	=
				{
					formatPair.colorFormat,			// VkFormat				format;
					formatPair.depthFormat,			// VkFormat				depthFormat;
					topology,						// Topology				topology;
					Vec4(0.0f, 1.0f, 0.0f, 1.0f),	// Vec4					expectedColor;
					0.01f,							// float				colorEpsilon;
					0.9f,							// float				expectedDepth;
					0.01f,							// float				depthEpsilon;
					1u,								// deUint32				repeatCount;
					false,							// bool					enableBlend;
					m_useDynamicRendering,			// bool					useDynamicRendering;
					{								// vector<ClearStep>	steps;
						{ ClearOp::DRAW		, Vec4(1.0f, 0.0f, 0.0f, 1.0f)	, 0.3f },
						{ ClearOp::CLEAR	, Vec4(0.0f, 1.0f, 0.0f, 1.0f)	, 0.9f }
					}
				};
				addChild(new MultipleClearsWithinRenderPassTest(m_testCtx, "draw_clear" + testNameSuffix, "Multiple clears within same render pass, methods: draw, clear", params));
			}
			{
				const TestParams params	=
				{
					formatPair.colorFormat,			// VkFormat				format;
					formatPair.depthFormat,			// VkFormat				depthFormat;
					topology,						// Topology				topology;
					Vec4(0.0f, 1.0f, 0.0f, 1.0f),	// Vec4					expectedColor;
					0.01f,							// float				colorEpsilon;
					0.9f,							// float				expectedDepth;
					0.01f,							// float				depthEpsilon;
					1u,								// deUint32				repeatCount;
					false,							// bool					enableBlend;
					m_useDynamicRendering,			// bool					useDynamicRendering;
					{								// vector<ClearStep>	steps;
						{ ClearOp::CLEAR	, Vec4(1.0f, 0.0f, 0.0f, 1.0f)	, 0.3f },
						{ ClearOp::CLEAR	, Vec4(0.0f, 1.0f, 0.0f, 1.0f)	, 0.9f }
					}
				};
				addChild(new MultipleClearsWithinRenderPassTest(m_testCtx, "clear_clear" + testNameSuffix, "Multiple clears within same render pass, methods: clear, clear", params));
			}
		}
	}
}
}	// Draw
}	// vkt
